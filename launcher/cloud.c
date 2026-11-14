#include "cloud.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "sha256.h"

/* ---- small JSON helpers (schema-fixed, dependency-free) ---- */
static void json_escape(const char *src, char *dst, size_t n) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < n; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            dst[j++] = '\\';
            dst[j++] = (char)c;
        } else if (c >= 0x20 && c < 0x7f) {
            dst[j++] = (char)c;
        }
    }
    dst[j] = 0;
}

static const char *find_json_val(const char *json, const char *key) {
    char pat[80];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = json;
    while ((p = strstr(p, pat)) != NULL) {
        p += strlen(pat);
        const char *q = p;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        if (*q != ':') continue;
        q++;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
        return q;
    }
    return NULL;
}

static int jget_str(const char *json, const char *key, char *out, size_t n) {
    if (!n) return 0;
    out[0] = 0;
    const char *v = find_json_val(json, key);
    if (!v || *v != '"') return 0;
    v++;
    size_t i = 0;
    while (*v && *v != '"') {
        if (*v == '\\') {
            if (!v[1]) { out[0] = 0; return 0; }
            v++;
        }
        if (i + 1 >= n) { out[0] = 0; return 0; }
        out[i++] = *v++;
    }
    if (*v != '"') { out[0] = 0; return 0; }
    out[i] = 0;
    return 1;
}

static int jget_long(const char *json, const char *key, long *out) {
    const char *v = find_json_val(json, key);
    if (!v) return 0;
    if (*v != '-' && !isdigit((unsigned char)*v)) return 0;
    errno = 0;
    char *end = NULL;
    long val = strtol(v, &end, 10);
    if (errno == ERANGE || end == v) return 0;
    *out = val;
    return 1;
}

static int jget_int(const char *json, const char *key, int *out) {
    long v = 0;
    if (!jget_long(json, key, &v) || v < INT_MIN || v > INT_MAX) return 0;
    *out = (int)v;
    return 1;
}

static int jget_bool(const char *json, const char *key, int *out) {
    const char *v = find_json_val(json, key);
    if (!v) return 0;
    if (!strncmp(v, "true", 4)) { *out = 1; return 1; }
    if (!strncmp(v, "false", 5)) { *out = 0; return 1; }
    return 0;
}

static char *read_all(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0 || sz > 4 * 1024 * 1024) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)sz, f);
    buf[r] = 0;
    fclose(f);
    if (len_out) *len_out = r;
    return buf;
}

static int write_atomic_bytes(const char *path, const void *data, size_t len, mode_t mode) {
    char tmp[1400];
    int tn = snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", path, (long)getpid());
    if (tn <= 0 || (size_t)tn >= sizeof(tmp)) return -1;
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) return -1;
    const uint8_t *p = (const uint8_t *)data;
    size_t rem = len;
    while (rem > 0) {
        ssize_t w = write(fd, p, rem);
        if (w < 0) {
            if (errno == EINTR) continue;
            close(fd);
            unlink(tmp);
            return -1;
        }
        p += (size_t)w;
        rem -= (size_t)w;
    }
    fsync(fd);
    close(fd);
    if (rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

/* ---- Local PS1 Memory Card validation & rotation (1..100) ---- */
int save_is_valid_mc(const char *path, char sha_out[65]) {
    size_t sz = 0;
    char *buf = read_all(path, &sz);
    if (!buf) return 0;
    int ok = (sz == PS1_MC_SIZE && (uint8_t)buf[0] == 0x4d && (uint8_t)buf[1] == 0x43);
    if (ok && sha_out) sha256_bytes(buf, sz, sha_out);
    free(buf);
    return ok;
}

typedef struct {
    SaveBackup bk;
    time_t mtime;
} BackupEntry;

static int cmp_desc_backup(const void *a, const void *b) {
    const BackupEntry *sa = (const BackupEntry *)a;
    const BackupEntry *sb = (const BackupEntry *)b;
    if (sb->mtime != sa->mtime) return sb->mtime > sa->mtime ? 1 : -1;
    return strcmp(sb->bk.name, sa->bk.name);
}

static void format_backup_label(const char *name_in, char *label, size_t n, char sha_short[9]) {
    char name[64], s[9] = "";
    snprintf(name, sizeof(name), "%s", name_in);
    /* Expected: card1_YYYYMMDD_HHMMSS_XXXXXXXX.mcd */
    const char *p = strchr(name, '_');
    if (p && strlen(p + 1) >= 24) {
        p++;
        snprintf(s, sizeof(s), "%.8s", p + 16);
        snprintf(label, n, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s  [%.8s]",
                 p, p + 4, p + 6, p + 9, p + 11, p + 13, s);
    } else {
        snprintf(label, n, "%.39s", name);
    }
    memcpy(sha_short, s, 9);
}

int save_backup_list(const char *game_dir, SaveBackup *out, int max_items) {
    char bdir[1024];
    snprintf(bdir, sizeof(bdir), "%s/saves_backup", game_dir);
    DIR *d = opendir(bdir);
    if (!d) return 0;

    size_t cap = 64, cnt = 0;
    BackupEntry *arr = malloc(cap * sizeof(BackupEntry));
    if (!arr) { closedir(d); return 0; }

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        if (strncmp(e->d_name, "card1_", 6) != 0) continue;
        if (l < 10 || l >= 96 || strcmp(e->d_name + l - 4, ".mcd") != 0) continue;
        if (cnt == cap) {
            if (cap >= 4096) break;
            cap *= 2;
            BackupEntry *na = realloc(arr, cap * sizeof(BackupEntry));
            if (!na) break;
            arr = na;
        }
        char full[1200];
        struct stat st;
        snprintf(full, sizeof(full), "%s/%s", bdir, e->d_name);
        time_t mt = (stat(full, &st) == 0) ? st.st_mtime : 0;
        snprintf(arr[cnt].bk.name, sizeof(arr[cnt].bk.name), "%s", e->d_name);
        format_backup_label(e->d_name, arr[cnt].bk.label, sizeof(arr[cnt].bk.label), arr[cnt].bk.sha_short);
        arr[cnt].mtime = mt;
        cnt++;
    }
    closedir(d);
    if (cnt > 1) qsort(arr, cnt, sizeof(arr[0]), cmp_desc_backup);
    int n = (int)cnt < max_items ? (int)cnt : max_items;
    for (int i = 0; i < n; i++) out[i] = arr[i].bk;
    free(arr);
    return n;
}

int save_backup_rotate(const char *game_dir, const char *slot_file, int max_keep) {
    if (max_keep < 1) max_keep = 1;
    if (max_keep > 100) max_keep = 100;

    char src[1024];
    snprintf(src, sizeof(src), "%s/%s", game_dir, slot_file);
    size_t sz = 0;
    char *raw = read_all(src, &sz);
    if (!raw) return -1;
    if (sz != PS1_MC_SIZE || (uint8_t)raw[0] != 0x4d || (uint8_t)raw[1] != 0x43) {
        free(raw);
        return -1;
    }
    char sha[65];
    sha256_bytes(raw, sz, sha);

    char bdir[1024];
    snprintf(bdir, sizeof(bdir), "%s/saves_backup", game_dir);
    mkdir(bdir, 0755);

    SaveBackup list[256];
    int existing = save_backup_list(game_dir, list, 256);
    int duplicate = 0;
    if (existing > 0 && strncmp(list[0].sha_short, sha, 8) == 0) {
        char newest_path[1200], newest_sha[65];
        snprintf(newest_path, sizeof(newest_path), "%s/%s", bdir, list[0].name);
        if (save_is_valid_mc(newest_path, newest_sha) && strcmp(newest_sha, sha) == 0) {
            duplicate = 1;
        }
    }

    if (!duplicate) {
        time_t t = time(NULL);
        struct tm tm;
        localtime_r(&t, &tm);
        char stamp[32], dst[1200];
        strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);
        snprintf(dst, sizeof(dst), "%s/card1_%s_%.8s.mcd", bdir, stamp, sha);
        if (write_atomic_bytes(dst, raw, sz, 0644) != 0) {
            free(raw);
            return -1;
        }
    }
    free(raw);

    existing = save_backup_list(game_dir, list, 256);
    for (int i = max_keep; i < existing; i++) {
        char rm_path[1200];
        snprintf(rm_path, sizeof(rm_path), "%s/%s", bdir, list[i].name);
        unlink(rm_path);
    }
    return 0;
}

int save_backup_restore(const char *game_dir, const char *backup_name, int max_keep, char *err, size_t err_n) {
    if (!backup_name || !backup_name[0] || strchr(backup_name, '/') || strstr(backup_name, "..")) {
        snprintf(err, err_n, "INVALID BACKUP NAME");
        return -1;
    }
    char src[1200], dst[1200];
    snprintf(src, sizeof(src), "%s/saves_backup/%s", game_dir, backup_name);
    snprintf(dst, sizeof(dst), "%s/card1.mcd", game_dir);

    size_t sz = 0;
    char *raw = read_all(src, &sz);
    if (!raw || sz != PS1_MC_SIZE || (uint8_t)raw[0] != 0x4d || (uint8_t)raw[1] != 0x43) {
        free(raw);
        snprintf(err, err_n, "BACKUP FILE IS CORRUPT");
        return -1;
    }

    /* Preserve the current card1.mcd before restoring an older snapshot. */
    if (save_is_valid_mc(dst, NULL)) {
        save_backup_rotate(game_dir, "card1.mcd", max_keep);
    }
    int rc = write_atomic_bytes(dst, raw, sz, 0644);
    free(raw);
    if (rc != 0) {
        snprintf(err, err_n, "COULD NOT WRITE card1.mcd");
        return -1;
    }
    return 0;
}

/* ---- Cloud state persistence ---- */
static void default_device_name(char *out, size_t n, int is_portmaster) {
    char host[64] = "";
    if (gethostname(host, sizeof(host) - 1) == 0 && host[0]) {
        for (size_t i = 0; host[i]; i++) {
            if ((unsigned char)host[i] < 0x20 || (unsigned char)host[i] > 0x7e) host[i] = '-';
        }
        snprintf(out, n, "%.36s", host);
    } else {
        snprintf(out, n, "%s", is_portmaster ? "DW3-Handheld" : "DW3-Desktop");
    }
}

void cloud_state_defaults(CloudState *st) {
    memset(st, 0, sizeof(*st));
    const char *env_url = getenv("DW3_SYNC_URL");
    snprintf(st->worker_url, sizeof(st->worker_url), "%s",
             (env_url && env_url[0]) ? env_url : DW3_DEFAULT_SYNC_URL);
    st->retention_days = 40;
}

int cloud_state_load(CloudState *st, const char *game_dir) {
    cloud_state_defaults(st);
    char path[1024];
    snprintf(path, sizeof(path), "%s/cloud_sync.json", game_dir);
    char *buf = read_all(path, NULL);
    if (!buf) return -1;

    char url[256];
    if (jget_str(buf, "worker_url", url, sizeof(url)) && url[0] && !getenv("DW3_SYNC_URL")) {
        snprintf(st->worker_url, sizeof(st->worker_url), "%s", url);
    }
    jget_str(buf, "account_id", st->account_id, sizeof(st->account_id));
    jget_str(buf, "device_id", st->device_id, sizeof(st->device_id));
    jget_str(buf, "device_token", st->device_token, sizeof(st->device_token));
    jget_str(buf, "device_name", st->device_name, sizeof(st->device_name));
    jget_str(buf, "last_sha256", st->last_sha256, sizeof(st->last_sha256));
    jget_str(buf, "revoke_reason", st->revoke_reason, sizeof(st->revoke_reason));
    jget_int(buf, "last_seq", &st->last_seq);
    jget_long(buf, "last_sync_at", &st->last_sync_at);
    jget_int(buf, "revoked", &st->revoked);
    jget_int(buf, "retention_days", &st->retention_days);
    if (st->retention_days <= 0) st->retention_days = 40;
    st->registered = (st->account_id[0] && st->device_id[0] && st->device_token[0]) ? 1 : 0;
    free(buf);
    return 0;
}

int cloud_state_save(const CloudState *st, const char *game_dir) {
    char path[1024], e_url[512], e_name[128], e_rev[256], out[2048];
    snprintf(path, sizeof(path), "%s/cloud_sync.json", game_dir);
    json_escape(st->worker_url, e_url, sizeof(e_url));
    json_escape(st->device_name, e_name, sizeof(e_name));
    json_escape(st->revoke_reason, e_rev, sizeof(e_rev));
    int n = snprintf(out, sizeof(out),
        "{\n"
        "  \"worker_url\": \"%s\",\n"
        "  \"account_id\": \"%s\",\n"
        "  \"device_id\": \"%s\",\n"
        "  \"device_token\": \"%s\",\n"
        "  \"device_name\": \"%s\",\n"
        "  \"last_seq\": %d,\n"
        "  \"last_sha256\": \"%s\",\n"
        "  \"last_sync_at\": %ld,\n"
        "  \"revoked\": %d,\n"
        "  \"revoke_reason\": \"%s\",\n"
        "  \"retention_days\": %d\n"
        "}\n",
        e_url, st->account_id, st->device_id, st->device_token, e_name,
        st->last_seq, st->last_sha256, st->last_sync_at, st->revoked, e_rev,
        st->retention_days > 0 ? st->retention_days : 40);
    if (n <= 0 || (size_t)n >= sizeof(out)) return -1;
    return write_atomic_bytes(path, out, (size_t)n, 0600);
}

void cloud_state_clear_auth(CloudState *st) {
    st->account_id[0] = 0;
    st->device_id[0] = 0;
    st->device_token[0] = 0;
    st->registered = 0;
    st->revoked = 0;
    st->revoke_reason[0] = 0;
    st->active_devices = 0;
    st->expires_at = 0;
    st->last_seq = 0;
    st->last_sha256[0] = 0;
    st->cloud_seq = 0;
    st->cloud_sha256[0] = 0;
    st->cloud_updated_at = 0;
}

/* ---- HTTP via fork + execvp("curl", ...) (zero shell, zero extra C libs) ---- */
typedef struct {
    int status;
    char *body;
    size_t body_len;
    char headers[8192];
} HttpResp;

static void http_resp_free(HttpResp *r) {
    free(r->body);
    r->body = NULL;
    r->body_len = 0;
}

static int valid_http_url(const char *url) {
    return url && (!strncmp(url, "https://", 8) || !strncmp(url, "http://", 7));
}

static int http_exec(const char *method, const char *url, const char *bearer,
                     const char *const *extra_headers, int n_extra,
                     const char *json_body, const char *upload_file,
                     const char *download_file, HttpResp *resp) {
    memset(resp, 0, sizeof(*resp));
    if (!valid_http_url(url)) return -1;
    const char *curl_bin = getenv("DW3_CLOUD_CURL");
    if (!curl_bin || !curl_bin[0]) curl_bin = "curl";

    char hdr_tmp[64] = "";
    if (download_file) {
        snprintf(hdr_tmp, sizeof(hdr_tmp), "/tmp/dw3hdr_XXXXXX");
        int hfd = mkstemp(hdr_tmp);
        if (hfd < 0) return -1;
        close(hfd);
    }

    int out_pipe[2];
    if (pipe(out_pipe) != 0) {
        if (hdr_tmp[0]) unlink(hdr_tmp);
        return -1;
    }

    char auth_hdr[256] = "";
    char *at_arg = NULL;
    const char *argv[48];
    int ac = 0;
    argv[ac++] = curl_bin;
    argv[ac++] = "-s";
    argv[ac++] = "--connect-timeout";
    argv[ac++] = "4";
    argv[ac++] = "--max-time";
    argv[ac++] = "12";
    argv[ac++] = "-X";
    argv[ac++] = method;
    if (bearer && bearer[0]) {
        snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", bearer);
        argv[ac++] = "-H";
        argv[ac++] = auth_hdr;
    }
    for (int i = 0; i < n_extra && ac + 2 < 40; i++) {
        argv[ac++] = "-H";
        argv[ac++] = extra_headers[i];
    }
    if (json_body) {
        argv[ac++] = "-H";
        argv[ac++] = "Content-Type: application/json";
        argv[ac++] = "--data-raw";
        argv[ac++] = json_body;
    } else if (upload_file) {
        argv[ac++] = "-H";
        argv[ac++] = "Content-Type: application/octet-stream";
        argv[ac++] = "--data-binary";
        char at_path[1100];
        snprintf(at_path, sizeof(at_path), "@%s", upload_file);
        at_arg = strdup(at_path);
        if (!at_arg) {
            close(out_pipe[0]);
            close(out_pipe[1]);
            if (hdr_tmp[0]) unlink(hdr_tmp);
            return -1;
        }
        argv[ac++] = at_arg;
    }
    if (download_file) {
        argv[ac++] = "-o";
        argv[ac++] = download_file;
        if (hdr_tmp[0]) {
            argv[ac++] = "-D";
            argv[ac++] = hdr_tmp;
        }
        argv[ac++] = "-w";
        argv[ac++] = "%{http_code}";
    } else {
        argv[ac++] = "-w";
        argv[ac++] = "\n%{http_code}";
    }
    argv[ac++] = "--";
    argv[ac++] = url;
    argv[ac] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        free(at_arg);
        close(out_pipe[0]);
        close(out_pipe[1]);
        if (hdr_tmp[0]) unlink(hdr_tmp);
        return -1;
    }
    if (pid == 0) {
        dup2(out_pipe[1], 1);
        close(out_pipe[0]);
        close(out_pipe[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 2); close(devnull); }
        execvp(curl_bin, (char *const *)argv);
        _exit(127);
    }
    close(out_pipe[1]);
    free(at_arg);

    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (buf) {
        for (;;) {
            ssize_t n = read(out_pipe[0], buf + len, cap - len - 1);
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (n == 0) break;
            len += (size_t)n;
            if (len + 1024 >= cap) {
                if (cap >= 512 * 1024) break;
                cap *= 2;
                char *nb = realloc(buf, cap);
                if (!nb) break;
                buf = nb;
            }
        }
        buf[len] = 0;
    }
    close(out_pipe[0]);

    int wst = 0;
    pid_t wr;
    while ((wr = waitpid(pid, &wst, 0)) < 0 && errno == EINTR) {}
    if (wr != pid || !WIFEXITED(wst) || WEXITSTATUS(wst) != 0 || !buf) {
        free(buf);
        if (hdr_tmp[0]) unlink(hdr_tmp);
        return -1;
    }

    if (download_file) {
        resp->status = atoi(buf);
        free(buf);
        if (hdr_tmp[0]) {
            size_t hlen = 0;
            char *h = read_all(hdr_tmp, &hlen);
            if (h) {
                snprintf(resp->headers, sizeof(resp->headers), "%s", h);
                free(h);
            }
            unlink(hdr_tmp);
        }
        return 0;
    }

    char *nl = strrchr(buf, '\n');
    if (nl) {
        *nl = 0;
        resp->status = atoi(nl + 1);
        resp->body = buf;
        resp->body_len = (size_t)(nl - buf);
    } else {
        resp->status = atoi(buf);
        resp->body = buf;
        resp->body_len = 0;
    }
    return 0;
}

static int header_val(const char *headers, const char *name, char *out, size_t n) {
    out[0] = 0;
    size_t nlen = strlen(name);
    const char *p = headers;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t llen = eol ? (size_t)(eol - p) : strlen(p);
        if (llen > nlen + 1 && strncasecmp(p, name, nlen) == 0 && p[nlen] == ':') {
            const char *v = p + nlen + 1;
            while (*v == ' ' || *v == '\t') v++;
            size_t i = 0;
            while (v < p + llen && *v != '\r' && i + 1 < n) out[i++] = *v++;
            out[i] = 0;
            return 1;
        }
        if (!eol) break;
        p = eol + 1;
    }
    return 0;
}

static void check_revoked_resp(CloudState *st, const char *game_dir, const HttpResp *r, char *err, size_t err_n) {
    char code[64] = "";
    if (r->body) jget_str(r->body, "error", code, sizeof(code));
    if (r->status == 403 && (!strcmp(code, "device_revoked") || !strcmp(code, "account_banned"))) {
        st->revoked = 1;
        snprintf(st->revoke_reason, sizeof(st->revoke_reason), "%s",
                 !strcmp(code, "account_banned") ? "ACCOUNT BANNED" : "DEVICE REVOKED");
        if (game_dir) cloud_state_save(st, game_dir);
        snprintf(err, err_n, "%s - RENEW REGISTRATION", st->revoke_reason);
    } else if (r->status == 429) {
        snprintf(err, err_n, "RATE LIMITED - TRY AGAIN LATER");
    } else if (code[0]) {
        snprintf(err, err_n, "SERVER: %.64s", code);
    } else {
        snprintf(err, err_n, "HTTP %d", r->status);
    }
}

/* ---- Cloud API ---- */
int cloud_register(CloudState *st, const char *game_dir, int is_portmaster, char *err, size_t err_n) {
    char url[320], name[48], ename[96], body[256];
    default_device_name(name, sizeof(name), is_portmaster);
    json_escape(name, ename, sizeof(ename));
    snprintf(url, sizeof(url), "%s/v1/register", st->worker_url);
    snprintf(body, sizeof(body), "{\"device_name\":\"%s\",\"platform\":\"%s\"}",
             ename, is_portmaster ? "portmaster" : "pc");

    HttpResp r;
    if (http_exec("POST", url, NULL, NULL, 0, body, NULL, NULL, &r) != 0) {
        snprintf(err, err_n, "OFFLINE OR NETWORK ERROR");
        return -1;
    }
    if (r.status != 201 || !r.body) {
        check_revoked_resp(st, NULL, &r, err, err_n);
        http_resp_free(&r);
        return -1;
    }

    cloud_state_clear_auth(st);
    jget_str(r.body, "account_id", st->account_id, sizeof(st->account_id));
    jget_str(r.body, "device_id", st->device_id, sizeof(st->device_id));
    jget_str(r.body, "device_token", st->device_token, sizeof(st->device_token));
    jget_str(r.body, "device_name", st->device_name, sizeof(st->device_name));
    jget_int(r.body, "retention_days", &st->retention_days);
    jget_long(r.body, "expires_at", &st->expires_at);
    http_resp_free(&r);

    if (!st->account_id[0] || !st->device_token[0]) {
        snprintf(err, err_n, "INVALID SERVER RESPONSE");
        return -1;
    }
    st->registered = 1;
    st->revoked = 0;
    st->active_devices = 1;
    cloud_state_save(st, game_dir);
    return 0;
}

int cloud_refresh_me(CloudState *st, const char *game_dir, char *err, size_t err_n) {
    if (!st->registered) {
        snprintf(err, err_n, "NOT REGISTERED");
        return -1;
    }
    char url[320];
    snprintf(url, sizeof(url), "%s/v1/me", st->worker_url);
    HttpResp r;
    if (http_exec("GET", url, st->device_token, NULL, 0, NULL, NULL, NULL, &r) != 0) {
        snprintf(err, err_n, "OFFLINE OR NETWORK ERROR");
        return -1;
    }
    if (r.status != 200 || !r.body) {
        check_revoked_resp(st, game_dir, &r, err, err_n);
        http_resp_free(&r);
        return -1;
    }
    st->revoked = 0;
    st->revoke_reason[0] = 0;
    jget_int(r.body, "active_devices", &st->active_devices);
    jget_int(r.body, "retention_days", &st->retention_days);
    jget_long(r.body, "expires_at", &st->expires_at);

    const char *saves_obj = find_json_val(r.body, "saves");
    const char *c1 = saves_obj ? find_json_val(saves_obj, "card1") : NULL;
    if (c1) {
        jget_int(c1, "seq", &st->cloud_seq);
        jget_str(c1, "sha256", st->cloud_sha256, sizeof(st->cloud_sha256));
        jget_long(c1, "updated_at", &st->cloud_updated_at);
    } else {
        st->cloud_seq = 0;
        st->cloud_sha256[0] = 0;
        st->cloud_updated_at = 0;
    }
    http_resp_free(&r);
    cloud_state_save(st, game_dir);
    return 0;
}

int cloud_push_card1(CloudState *st, const char *game_dir, int backup_max, int force, char *msg_out, size_t msg_n) {
    if (!st->registered || st->revoked) {
        snprintf(msg_out, msg_n, "CLOUD NOT ACTIVE");
        return -1;
    }
    char card[1024];
    snprintf(card, sizeof(card), "%s/card1.mcd", game_dir);
    size_t sz = 0;
    char *raw = read_all(card, &sz);
    if (!raw || sz != PS1_MC_SIZE || (uint8_t)raw[0] != 0x4d || (uint8_t)raw[1] != 0x43) {
        free(raw);
        snprintf(msg_out, msg_n, "NO VALID LOCAL card1.mcd");
        return -1;
    }
    char sha[65];
    sha256_bytes(raw, sz, sha);

    char snap[64] = "/tmp/dw3push_XXXXXX";
    int sfd = mkstemp(snap);
    if (sfd < 0) {
        free(raw);
        snprintf(msg_out, msg_n, "TMP WRITE FAILED");
        return -1;
    }
    size_t rem = sz;
    const char *wp = raw;
    while (rem > 0) {
        ssize_t w = write(sfd, wp, rem);
        if (w < 0) {
            if (errno == EINTR) continue;
            close(sfd);
            unlink(snap);
            free(raw);
            snprintf(msg_out, msg_n, "TMP WRITE FAILED");
            return -1;
        }
        wp += (size_t)w;
        rem -= (size_t)w;
    }
    fsync(sfd);
    close(sfd);
    free(raw);

    save_backup_rotate(game_dir, "card1.mcd", backup_max);

    char url[320], h_sha[96], h_seq[48], h_force[24];
    snprintf(url, sizeof(url), "%s/v1/saves/card1/push", st->worker_url);
    snprintf(h_sha, sizeof(h_sha), "X-Save-Sha256: %s", sha);
    snprintf(h_seq, sizeof(h_seq), "X-Expected-Seq: %d", st->last_seq);
    const char *hdrs[3] = {h_sha, h_seq, NULL};
    int nh = 2;
    if (force) {
        snprintf(h_force, sizeof(h_force), "X-Force: 1");
        hdrs[nh++] = h_force;
    }

    HttpResp r;
    int rc = http_exec("POST", url, st->device_token, hdrs, nh, NULL, snap, NULL, &r);
    unlink(snap);
    if (rc != 0) {
        snprintf(msg_out, msg_n, "OFFLINE - LOCAL BACKUP SAVED");
        return -1;
    }
    if (r.status == 409) {
        jget_int(r.body ? r.body : "", "server_seq", &st->cloud_seq);
        jget_str(r.body ? r.body : "", "server_sha256", st->cloud_sha256, sizeof(st->cloud_sha256));
        http_resp_free(&r);
        snprintf(msg_out, msg_n, "CLOUD CONFLICT (SERVER #%d)", st->cloud_seq);
        return -2;
    }
    if (r.status != 200 || !r.body) {
        check_revoked_resp(st, game_dir, &r, msg_out, msg_n);
        http_resp_free(&r);
        return -1;
    }

    int seq = st->last_seq;
    jget_int(r.body, "seq", &seq);
    http_resp_free(&r);

    st->last_seq = seq;
    st->cloud_seq = seq;
    snprintf(st->last_sha256, sizeof(st->last_sha256), "%s", sha);
    snprintf(st->cloud_sha256, sizeof(st->cloud_sha256), "%s", sha);
    st->last_sync_at = (long)time(NULL);
    cloud_state_save(st, game_dir);
    snprintf(msg_out, msg_n, "UPLOADED REV #%d", seq);
    return 0;
}

int cloud_pull_card1(CloudState *st, const char *game_dir, int backup_max, char *msg_out, size_t msg_n) {
    if (!st->registered || st->revoked) {
        snprintf(msg_out, msg_n, "CLOUD NOT ACTIVE");
        return -1;
    }
    char url[320], tmp[1024], dst[1024];
    snprintf(url, sizeof(url), "%s/v1/saves/card1/pull", st->worker_url);
    snprintf(tmp, sizeof(tmp), "%s/card1.cloud.tmp.%ld", game_dir, (long)getpid());
    snprintf(dst, sizeof(dst), "%s/card1.mcd", game_dir);

    HttpResp r;
    if (http_exec("GET", url, st->device_token, NULL, 0, NULL, NULL, tmp, &r) != 0) {
        unlink(tmp);
        snprintf(msg_out, msg_n, "OFFLINE OR NETWORK ERROR");
        return -1;
    }
    if (r.status == 404) {
        unlink(tmp);
        http_resp_free(&r);
        snprintf(msg_out, msg_n, "NO CLOUD SAVE YET");
        return -1;
    }
    if (r.status != 200) {
        unlink(tmp);
        check_revoked_resp(st, game_dir, &r, msg_out, msg_n);
        http_resp_free(&r);
        return -1;
    }

    char dl_sha[65], hdr_sha[65], seq_s[32];
    if (!save_is_valid_mc(tmp, dl_sha)) {
        unlink(tmp);
        http_resp_free(&r);
        snprintf(msg_out, msg_n, "CORRUPT CLOUD PAYLOAD");
        return -1;
    }
    if (header_val(r.headers, "X-Save-Sha256", hdr_sha, sizeof(hdr_sha)) && strcasecmp(hdr_sha, dl_sha) != 0) {
        unlink(tmp);
        http_resp_free(&r);
        snprintf(msg_out, msg_n, "CLOUD PAYLOAD SHA MISMATCH");
        return -1;
    }

    if (save_is_valid_mc(dst, NULL)) {
        save_backup_rotate(game_dir, "card1.mcd", backup_max);
    }
    if (rename(tmp, dst) != 0) {
        unlink(tmp);
        http_resp_free(&r);
        snprintf(msg_out, msg_n, "COULD NOT REPLACE card1.mcd");
        return -1;
    }

    int seq = st->last_seq;
    if (header_val(r.headers, "X-Save-Seq", seq_s, sizeof(seq_s))) seq = atoi(seq_s);
    http_resp_free(&r);

    st->last_seq = seq;
    st->cloud_seq = seq;
    snprintf(st->last_sha256, sizeof(st->last_sha256), "%s", dl_sha);
    snprintf(st->cloud_sha256, sizeof(st->cloud_sha256), "%s", dl_sha);
    st->last_sync_at = (long)time(NULL);
    cloud_state_save(st, game_dir);
    snprintf(msg_out, msg_n, "DOWNLOADED REV #%d", seq);
    return 0;
}

int cloud_sync_card1(CloudState *st, const char *game_dir, int backup_max, int interactive,
                     int *conflict_out, char *msg_out, size_t msg_n) {
    if (conflict_out) *conflict_out = 0;
    if (!st->registered || st->revoked) {
        snprintf(msg_out, msg_n, st->revoked ? "DEVICE REVOKED" : "NOT REGISTERED");
        return -1;
    }

    char card[1024], local_sha[65] = "";
    snprintf(card, sizeof(card), "%s/card1.mcd", game_dir);
    int has_local = save_is_valid_mc(card, local_sha);

    char url[320];
    snprintf(url, sizeof(url), "%s/v1/saves/card1/pull?meta=1", st->worker_url);
    HttpResp r;
    if (http_exec("GET", url, st->device_token, NULL, 0, NULL, NULL, NULL, &r) != 0) {
        snprintf(msg_out, msg_n, "OFFLINE - USING LOCAL SAVE");
        return -1;
    }

    if (r.status == 404) {
        http_resp_free(&r);
        st->cloud_seq = 0;
        st->cloud_sha256[0] = 0;
        if (!has_local) {
            snprintf(msg_out, msg_n, "NO SAVE YET");
            return 0;
        }
        return cloud_push_card1(st, game_dir, backup_max, 0, msg_out, msg_n);
    }

    if (r.status != 200 || !r.body) {
        check_revoked_resp(st, game_dir, &r, msg_out, msg_n);
        http_resp_free(&r);
        return -1;
    }

    int cseq = 0;
    char csha[65] = "";
    jget_int(r.body, "seq", &cseq);
    jget_str(r.body, "sha256", csha, sizeof(csha));
    jget_long(r.body, "updated_at", &st->cloud_updated_at);
    http_resp_free(&r);

    st->cloud_seq = cseq;
    snprintf(st->cloud_sha256, sizeof(st->cloud_sha256), "%s", csha);

    if (has_local && !strcmp(local_sha, csha)) {
        st->last_seq = cseq;
        snprintf(st->last_sha256, sizeof(st->last_sha256), "%s", local_sha);
        st->last_sync_at = (long)time(NULL);
        cloud_state_save(st, game_dir);
        snprintf(msg_out, msg_n, "IN SYNC (REV #%d)", cseq);
        return 0;
    }

    if (!has_local) {
        return cloud_pull_card1(st, game_dir, backup_max, msg_out, msg_n);
    }

    int local_unchanged = (st->last_sha256[0] && !strcmp(local_sha, st->last_sha256));
    if (cseq > st->last_seq && local_unchanged) {
        return cloud_pull_card1(st, game_dir, backup_max, msg_out, msg_n);
    }

    if (cseq == st->last_seq) {
        int prc = cloud_push_card1(st, game_dir, backup_max, 0, msg_out, msg_n);
        if (prc == -2 && interactive && conflict_out) {
            *conflict_out = 1;
            return 1;
        }
        return prc;
    }

    /* Both cloud and local diverged (or first sync on a device with an existing local card). */
    save_backup_rotate(game_dir, "card1.mcd", backup_max);
    if (interactive && conflict_out) {
        *conflict_out = 1;
        snprintf(msg_out, msg_n, "CLOUD REV #%d DIFFERS FROM LOCAL", cseq);
        return 1;
    }
    snprintf(msg_out, msg_n, "CONFLICT SKIPPED (OPEN CLOUD MENU)");
    return -2;
}

/* ---- 2-Step Pairing & Device Management ---- */
int cloud_pair_create(CloudState *st, char code_out[8], int *expires_out, char *err, size_t err_n) {
    char url[320];
    snprintf(url, sizeof(url), "%s/v1/pair/create", st->worker_url);
    HttpResp r;
    if (http_exec("POST", url, st->device_token, NULL, 0, "{}", NULL, NULL, &r) != 0) {
        snprintf(err, err_n, "OFFLINE OR NETWORK ERROR");
        return -1;
    }
    if (r.status != 201 || !r.body) {
        check_revoked_resp(st, NULL, &r, err, err_n);
        http_resp_free(&r);
        return -1;
    }
    jget_str(r.body, "code", code_out, 8);
    int exp = 300;
    jget_int(r.body, "expires_in", &exp);
    if (expires_out) *expires_out = exp;
    http_resp_free(&r);
    return strlen(code_out) == 6 ? 0 : -1;
}

int cloud_pair_status(CloudState *st, const char *code, PairStatus *out, char *err, size_t err_n) {
    memset(out, 0, sizeof(*out));
    char url[360];
    snprintf(url, sizeof(url), "%s/v1/pair/status?code=%s", st->worker_url, code);
    HttpResp r;
    if (http_exec("GET", url, st->device_token, NULL, 0, NULL, NULL, NULL, &r) != 0) {
        snprintf(err, err_n, "NETWORK ERROR");
        return -1;
    }
    if (r.status != 200 || !r.body) {
        check_revoked_resp(st, NULL, &r, err, err_n);
        http_resp_free(&r);
        return -1;
    }
    jget_str(r.body, "status", out->status, sizeof(out->status));
    jget_int(r.body, "expires_in", &out->expires_in);
    jget_str(r.body, "pending_device_name", out->pending_name, sizeof(out->pending_name));
    jget_str(r.body, "pending_platform", out->pending_platform, sizeof(out->pending_platform));
    jget_str(r.body, "pending_country", out->pending_country, sizeof(out->pending_country));
    http_resp_free(&r);
    return 0;
}

int cloud_pair_approve(CloudState *st, const char *code, int approve, char *err, size_t err_n) {
    char url[320], body[128];
    snprintf(url, sizeof(url), "%s/v1/pair/approve", st->worker_url);
    snprintf(body, sizeof(body), "{\"code\":\"%s\",\"approve\":%s}", code, approve ? "true" : "false");
    HttpResp r;
    if (http_exec("POST", url, st->device_token, NULL, 0, body, NULL, NULL, &r) != 0) {
        snprintf(err, err_n, "OFFLINE OR NETWORK ERROR");
        return -1;
    }
    if (r.status != 200) {
        check_revoked_resp(st, NULL, &r, err, err_n);
        http_resp_free(&r);
        return -1;
    }
    http_resp_free(&r);
    return 0;
}

int cloud_pair_claim(CloudState *st, const char *code, int is_portmaster, PairClaim *claim_out, char *err, size_t err_n) {
    memset(claim_out, 0, sizeof(*claim_out));
    char url[320], name[48], ename[96], body[256];
    default_device_name(name, sizeof(name), is_portmaster);
    json_escape(name, ename, sizeof(ename));
    snprintf(url, sizeof(url), "%s/v1/pair/claim", st->worker_url);
    snprintf(body, sizeof(body), "{\"code\":\"%s\",\"device_name\":\"%s\",\"platform\":\"%s\"}",
             code, ename, is_portmaster ? "portmaster" : "pc");
    HttpResp r;
    if (http_exec("POST", url, NULL, NULL, 0, body, NULL, NULL, &r) != 0) {
        snprintf(err, err_n, "OFFLINE OR NETWORK ERROR");
        return -1;
    }
    if (r.status != 200 || !r.body) {
        check_revoked_resp(st, NULL, &r, err, err_n);
        http_resp_free(&r);
        return -1;
    }
    jget_str(r.body, "claim_id", claim_out->claim_id, sizeof(claim_out->claim_id));
    jget_str(r.body, "claim_secret", claim_out->claim_secret, sizeof(claim_out->claim_secret));
    http_resp_free(&r);
    if (!claim_out->claim_id[0] || !claim_out->claim_secret[0]) {
        snprintf(err, err_n, "INVALID CLAIM RESPONSE");
        return -1;
    }
    return 0;
}

int cloud_pair_poll(CloudState *st, const char *game_dir, const PairClaim *claim, int *approved_out, char *err, size_t err_n) {
    *approved_out = 0;
    char url[320], body[512], e_cid[128], e_csec[256];
    snprintf(url, sizeof(url), "%s/v1/pair/poll", st->worker_url);
    json_escape(claim->claim_id, e_cid, sizeof(e_cid));
    json_escape(claim->claim_secret, e_csec, sizeof(e_csec));
    snprintf(body, sizeof(body), "{\"claim_id\":\"%s\",\"claim_secret\":\"%s\"}", e_cid, e_csec);
    HttpResp r;
    if (http_exec("POST", url, NULL, NULL, 0, body, NULL, NULL, &r) != 0) {
        snprintf(err, err_n, "NETWORK ERROR");
        return -1;
    }
    if (r.status == 403 || r.status == 410 || r.status == 404) {
        check_revoked_resp(st, NULL, &r, err, err_n);
        http_resp_free(&r);
        return -2; /* terminal failure */
    }
    if (r.status != 200 || !r.body) {
        check_revoked_resp(st, NULL, &r, err, err_n);
        http_resp_free(&r);
        return -1;
    }

    char status[32] = "";
    jget_str(r.body, "status", status, sizeof(status));
    if (!strcmp(status, "approved")) {
        cloud_state_clear_auth(st);
        jget_str(r.body, "account_id", st->account_id, sizeof(st->account_id));
        jget_str(r.body, "device_id", st->device_id, sizeof(st->device_id));
        jget_str(r.body, "device_token", st->device_token, sizeof(st->device_token));
        jget_str(r.body, "device_name", st->device_name, sizeof(st->device_name));
        jget_int(r.body, "retention_days", &st->retention_days);
        http_resp_free(&r);
        if (!st->account_id[0] || !st->device_token[0]) {
            snprintf(err, err_n, "INVALID APPROVAL PAYLOAD");
            return -2;
        }
        st->registered = 1;
        st->revoked = 0;
        cloud_state_save(st, game_dir);
        *approved_out = 1;
        return 0;
    }
    http_resp_free(&r);
    return 0;
}

int cloud_list_devices(CloudState *st, CloudDevice *out, int max_n, int *count_out, char *err, size_t err_n) {
    *count_out = 0;
    char url[320];
    snprintf(url, sizeof(url), "%s/v1/devices", st->worker_url);
    HttpResp r;
    if (http_exec("GET", url, st->device_token, NULL, 0, NULL, NULL, NULL, &r) != 0) {
        snprintf(err, err_n, "OFFLINE OR NETWORK ERROR");
        return -1;
    }
    if (r.status != 200 || !r.body) {
        check_revoked_resp(st, NULL, &r, err, err_n);
        http_resp_free(&r);
        return -1;
    }

    int cnt = 0;
    const char *p = strstr(r.body, "\"devices\"");
    if (p) p = strchr(p, '[');
    while (p && cnt < max_n) {
        const char *obj = NULL;
        int in_str = 0;
        for (const char *q = p; *q; q++) {
            if (*q == '"' && (q == p || q[-1] != '\\')) in_str = !in_str;
            else if (!in_str && *q == '{') { obj = q; break; }
            else if (!in_str && *q == ']') break;
        }
        if (!obj) break;
        const char *end = NULL;
        int depth = 0;
        in_str = 0;
        for (const char *q = obj; *q; q++) {
            if (*q == '\\' && in_str && q[1]) { q++; continue; }
            if (*q == '"') in_str = !in_str;
            else if (!in_str && *q == '{') depth++;
            else if (!in_str && *q == '}' && --depth == 0) { end = q; break; }
        }
        if (!end) break;
        size_t len = (size_t)(end - obj + 1);
        char *slice = malloc(len + 1);
        if (!slice) break;
        memcpy(slice, obj, len);
        slice[len] = 0;

        CloudDevice *d = &out[cnt];
        memset(d, 0, sizeof(*d));
        jget_str(slice, "device_id", d->device_id, sizeof(d->device_id));
        jget_str(slice, "name", d->name, sizeof(d->name));
        jget_str(slice, "platform", d->platform, sizeof(d->platform));
        jget_str(slice, "status", d->status, sizeof(d->status));
        jget_str(slice, "country", d->country, sizeof(d->country));
        jget_long(slice, "last_seen_at", &d->last_seen_at);
        jget_bool(slice, "is_current", &d->is_current);
        free(slice);
        if (d->device_id[0]) cnt++;
        p = end + 1;
    }
    http_resp_free(&r);
    *count_out = cnt;
    return 0;
}

int cloud_revoke_device(CloudState *st, const char *device_id, char *err, size_t err_n) {
    char url[320], body[256], e_id[128];
    snprintf(url, sizeof(url), "%s/v1/devices/revoke", st->worker_url);
    json_escape(device_id, e_id, sizeof(e_id));
    snprintf(body, sizeof(body), "{\"device_id\":\"%s\"}", e_id);
    HttpResp r;
    if (http_exec("POST", url, st->device_token, NULL, 0, body, NULL, NULL, &r) != 0) {
        snprintf(err, err_n, "OFFLINE OR NETWORK ERROR");
        return -1;
    }
    if (r.status != 200) {
        check_revoked_resp(st, NULL, &r, err, err_n);
        http_resp_free(&r);
        return -1;
    }
    http_resp_free(&r);
    return 0;
}
