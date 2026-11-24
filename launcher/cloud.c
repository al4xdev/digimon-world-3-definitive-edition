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
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <time.h>
#include <unistd.h>

#include "compat.h"
#include "sha256.h"
#include "win32.h"

#ifdef __ANDROID__
#include <sys/system_properties.h>

/* android/android_entry.c: one HTTP request through Java's HttpURLConnection (see http_exec_android). */
extern int android_http_request(const char *method, const char *url, const char *headers,
                                const void *body, size_t body_len, int *status_out,
                                char **body_out, size_t *body_len_out, char *resp_headers, size_t resp_headers_n);
#endif

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
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode);
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
    if (rename_over(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

/* ---- The card and its expansion save (cloud.h) ---- */
int save_expansion_path(const char *card_path, char *out, size_t n) {
    size_t l = strlen(card_path);
    if (l >= 4 && !strcmp(card_path + l - 4, ".mcd")) l -= 4;
    int w = snprintf(out, n, "%.*s.def.json", (int)l, card_path);
    return w > 0 && (size_t)w < n;
}

/* The card then its JSON in one malloc'd buffer (*len_out bytes, *ext_len_out of them the JSON: 0 when there is none);
 * NULL when the card is not a valid one. A JSON past EXPANSION_MAX (the game takes up to 4 MB) or not starting with
 * '{' is not carried: the card goes alone, so the card itself still syncs and is backed up (the file stays on disk). */
static char *pair_load(const char *card_path, size_t *len_out, size_t *ext_len_out) {
    size_t cl = 0, el = 0;
    char *card = read_all(card_path, &cl);
    if (!card || cl != PS1_MC_SIZE || (uint8_t)card[0] != 0x4d || (uint8_t)card[1] != 0x43) {
        free(card);
        return NULL;
    }
    char ep[1200];
    char *ext = save_expansion_path(card_path, ep, sizeof(ep)) ? read_all(ep, &el) : NULL;
    if (ext && (el > EXPANSION_MAX || (el > 0 && ext[0] != '{'))) {
        fprintf(stderr, "[cloud] %s: %zu bytes, not carried with the card (past %d or not a JSON object)\n", ep, el,
                EXPANSION_MAX);
        free(ext);
        ext = NULL;
        el = 0;
    }
    if (ext && el > 0) {
        char *nb = realloc(card, PS1_MC_SIZE + el + 1);
        if (!nb) { free(ext); free(card); return NULL; }
        card = nb;
        memcpy(card + PS1_MC_SIZE, ext, el);
        card[PS1_MC_SIZE + el] = 0;
    } else {
        el = 0;
    }
    free(ext);
    *len_out = PS1_MC_SIZE + el;
    *ext_len_out = el;
    return card;
}

/* Writes a pair over card_path and its JSON: the JSON first, as the game does (the card's generation names it), then
 * the card. ext_len 0: the card being written has none, so the old JSON is moved aside to <json>.prev, not deleted
 * (the caller's backup may not hold it: no valid card beside it, or one too big to carry). */
static int pair_write(const char *card_path, const char *data, size_t ext_len) {
    char ep[1200], prev[1210];
    if (!save_expansion_path(card_path, ep, sizeof(ep))) return -1;
    if (ext_len > 0) {
        if (write_atomic_bytes(ep, data + PS1_MC_SIZE, ext_len, 0644) != 0) return -1;
    } else {
        snprintf(prev, sizeof(prev), "%s.prev", ep);
        remove(prev); /* rename() over an existing file fails on Windows */
        if (rename(ep, prev) != 0 && errno != ENOENT) return -1;
    }
    if (write_atomic_bytes(card_path, data, PS1_MC_SIZE, 0644) != 0) {
        if (ext_len > 0) unlink(ep); /* a backup's JSON without its card would never be rotated away */
        return -1;
    }
    return 0;
}

/* ---- Local PS1 Memory Card validation & rotation (1..100) ---- */
int save_is_valid_mc(const char *path, char sha_out[65]) {
    size_t sz = 0, el = 0;
    char *buf = pair_load(path, &sz, &el);
    if (!buf) return 0;
    if (sha_out) sha256_bytes(buf, sz, sha_out);
    free(buf);
    return 1;
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
    size_t sz = 0, el = 0;
    char *raw = pair_load(src, &sz, &el);
    if (!raw) return -1;
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
        if (pair_write(dst, raw, el) != 0) { /* the JSON beside it, card1_<stamp>_<sha>.def.json */
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
        char rm_ext[1200];
        if (save_expansion_path(rm_path, rm_ext, sizeof(rm_ext))) unlink(rm_ext);
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

    size_t sz = 0, el = 0;
    char *raw = pair_load(src, &sz, &el);
    if (!raw) {
        snprintf(err, err_n, "BACKUP FILE IS CORRUPT");
        return -1;
    }

    /* Preserve the current card1.mcd before restoring an older snapshot. */
    if (save_is_valid_mc(dst, NULL)) {
        save_backup_rotate(game_dir, "card1.mcd", max_keep);
    }
    int rc = pair_write(dst, raw, el); /* a backup from before the expansion has no JSON: none is left beside the card */
    free(raw);
    if (rc != 0) {
        snprintf(err, err_n, "COULD NOT WRITE card1.mcd");
        return -1;
    }
    return 0;
}

/* ---- RecompLauncher mirror (see cloud.h) ---- */
#define RECOMP_DIR "recomp"
#define RECOMP_FILE "dw3-save.dw3s"
#define RECOMP_STATE "recomp_mirror.state"

int recomp_active(void) {
    const char *e = getenv("RECOMPLAUNCHER");
    return e && e[0] && strcmp(e, "0") != 0;
}

static void recomp_state_read(const char *game_dir, char sha[65]) {
    char path[1100];
    sha[0] = 0;
    snprintf(path, sizeof(path), "%s/%s", game_dir, RECOMP_STATE);
    size_t n = 0;
    char *b = read_all(path, &n);
    if (!b) return;
    if (n >= 64) {
        memcpy(sha, b, 64);
        sha[64] = 0;
        for (int i = 0; i < 64; i++) {
            if (!isxdigit((unsigned char)sha[i])) { sha[0] = 0; break; }
        }
    }
    free(b);
}

static void recomp_state_write(const char *game_dir, const char *sha) {
    char path[1100];
    snprintf(path, sizeof(path), "%s/%s", game_dir, RECOMP_STATE);
    write_atomic_bytes(path, sha, 64, 0644);
}

/* A valid mirror: 1, with its header in *m and the whole file in *buf (malloc'd; *card points at the card inside it,
 * *ext_len bytes of the expansion's JSON after it: format 2; format 1 has none). */
static int recomp_read(const char *game_dir, RecompMirror *m, char **buf, const char **card, size_t *ext_len) {
    char path[1100];
    snprintf(path, sizeof(path), "%s/%s/%s", game_dir, RECOMP_DIR, RECOMP_FILE);
    size_t len = 0;
    char *b = read_all(path, &len);
    if (!b) return 0;
    char *nl = memchr(b, '\n', len < 1024 ? len : 1024);
    if (!nl) { free(b); return 0; }
    *nl = 0;
    const char *data = nl + 1;
    size_t data_len = len - (size_t)(nl - b) - 1;
    long el = 0;
    int fmt = 0, ok = jget_int(b, "format", &fmt) && (fmt == 1 || (fmt == 2 && jget_long(b, "expansion_size", &el))) &&
                      el >= 0 && el <= EXPANSION_MAX && data_len == PS1_MC_SIZE + (size_t)el &&
                      (uint8_t)data[0] == 0x4d && (uint8_t)data[1] == 0x43;
    char sha[65];
    memset(m, 0, sizeof(*m));
    if (ok) {
        sha256_bytes(data, data_len, sha);
        ok = jget_str(b, "sha256", m->sha256, sizeof(m->sha256)) && strcmp(m->sha256, sha) == 0;
    }
    if (!ok) { free(b); return 0; }
    jget_str(b, "device_id", m->device_id, sizeof(m->device_id));
    jget_str(b, "device_name", m->device_name, sizeof(m->device_name));
    jget_long(b, "written_at", &m->written_at);
    jget_int(b, "cloud_seq", &m->cloud_seq);
    *buf = b;
    *card = data;
    *ext_len = (size_t)el;
    return 1;
}

int recomp_mirror_write(const char *game_dir, const char *device_id, const char *device_name, int cloud_seq, int force) {
    char card_p[1100], sha[65], state_sha[65];
    snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
    size_t sz = 0, el = 0;
    char *raw = pair_load(card_p, &sz, &el);
    if (!raw) return -1;
    sha256_bytes(raw, sz, sha);
    recomp_state_read(game_dir, state_sha);

    RecompMirror cur;
    char *mb = NULL;
    const char *md = NULL;
    size_t mel = 0;
    if (recomp_read(game_dir, &cur, &mb, &md, &mel)) {
        free(mb);
        if (!force) {
            if (!strcmp(cur.sha256, sha)) {
                if (strcmp(state_sha, sha) != 0) recomp_state_write(game_dir, sha);
                free(raw);
                return 0;
            }
            if (strcmp(cur.sha256, state_sha) != 0) { /* changed from outside: it waits for the player's choice */
                free(raw);
                return 1;
            }
        }
    }

    char eid[128], ename[96], hdr[512];
    json_escape(device_id ? device_id : "", eid, sizeof(eid));
    json_escape(device_name ? device_name : "", ename, sizeof(ename));
    /* Format 1 (the card alone) while there is no JSON, so a launcher from before the expansion still reads it; format 2
     * carries the JSON after the card, and its sha256 is the pair's. */
    char ext_field[48] = "";
    if (el > 0) snprintf(ext_field, sizeof(ext_field), ",\"expansion_size\":%lu", (unsigned long)el);
    int hn = snprintf(hdr, sizeof(hdr),
                      "{\"format\":%d,\"sha256\":\"%s\",\"device_id\":\"%s\",\"device_name\":\"%s\",\"written_at\":%ld,\"cloud_seq\":%d%s}\n",
                      el > 0 ? 2 : 1, sha, eid, ename, (long)time(NULL), cloud_seq, ext_field);
    if (hn <= 0 || (size_t)hn >= sizeof(hdr)) { free(raw); return -1; }
    char *out = malloc((size_t)hn + sz);
    if (!out) { free(raw); return -1; }
    memcpy(out, hdr, (size_t)hn);
    memcpy(out + hn, raw, sz);
    free(raw);

    char dir[1100], file[1200];
    snprintf(dir, sizeof(dir), "%s/%s", game_dir, RECOMP_DIR);
    mkdir(dir, 0755);
    snprintf(file, sizeof(file), "%s/%s", dir, RECOMP_FILE);
    int rc = write_atomic_bytes(file, out, (size_t)hn + sz, 0644);
    free(out);
    if (rc != 0) return -1;
    recomp_state_write(game_dir, sha);
    return 0;
}

int recomp_mirror_foreign(const char *game_dir, RecompMirror *out) {
    RecompMirror m;
    char *mb = NULL;
    const char *md = NULL;
    size_t mel = 0;
    if (!recomp_read(game_dir, &m, &mb, &md, &mel)) return 0;
    free(mb);
    char state_sha[65], local_sha[65] = "", card_p[1100];
    recomp_state_read(game_dir, state_sha);
    snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
    save_is_valid_mc(card_p, local_sha);
    int foreign = strcmp(m.sha256, state_sha) != 0 && strcmp(m.sha256, local_sha) != 0;
    if (foreign && out) *out = m;
    return foreign;
}

int recomp_mirror_adopt(const char *game_dir, int backup_max, char *err, size_t err_n) {
    RecompMirror m;
    char *mb = NULL;
    const char *md = NULL;
    size_t mel = 0;
    if (!recomp_read(game_dir, &m, &mb, &md, &mel)) {
        snprintf(err, err_n, "RECOMPLAUNCHER SAVE IS CORRUPT");
        return -1;
    }
    char card_p[1100];
    snprintf(card_p, sizeof(card_p), "%s/card1.mcd", game_dir);
    if (save_is_valid_mc(card_p, NULL)) save_backup_rotate(game_dir, "card1.mcd", backup_max);
    int rc = pair_write(card_p, md, mel);
    if (rc == 0) recomp_state_write(game_dir, m.sha256);
    free(mb);
    if (rc != 0) {
        snprintf(err, err_n, "COULD NOT WRITE card1.mcd");
        return -1;
    }
    snprintf(err, err_n, "RECOMPLAUNCHER SAVE APPLIED");
    return 0;
}

/* ---- Cloud state persistence ---- */
static const char *client_platform(int is_portmaster) {
#ifdef __ANDROID__
    (void)is_portmaster;
    return "android";
#else
    return is_portmaster ? "portmaster" : "pc";
#endif
}

static void default_device_name(char *out, size_t n, int is_portmaster) {
#ifdef __ANDROID__
    /* An app's hostname is "localhost"; the model ("moto g84 5G") is what the owner recognizes in the device list. */
    char model[PROP_VALUE_MAX] = "";
    if (__system_property_get("ro.product.model", model) > 0) {
        for (size_t i = 0; model[i]; i++) {
            if ((unsigned char)model[i] < 0x20 || (unsigned char)model[i] > 0x7e) model[i] = '-';
        }
        snprintf(out, n, "%.36s", model);
    } else {
        snprintf(out, n, "DW3-Android");
    }
    (void)is_portmaster;
    return;
#endif
    char host[64] = "";
#ifdef _WIN32
    const char *cn = getenv("COMPUTERNAME");
    snprintf(host, sizeof(host), "%s", cn ? cn : "");
    if (host[0]) {
#else
    if (gethostname(host, sizeof(host) - 1) == 0 && host[0]) {
#endif
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

#if defined(__ANDROID__) || defined(_WIN32)
/* Android has no curl to rely on: AOSP does not ship /system/bin/curl (some vendors add one, with their own CA setup),
 * and fork() from a process that runs a JVM is fragile. Java's HttpURLConnection is on every device and uses the
 * system's CA store, so the request goes there; the response comes back in the same shape as the curl path.
 * Windows has no fork either: WinHTTP (win32.c) takes the same request, with the system's certificate store. */
static int http_exec_native(const char *method, const char *url, const char *bearer,
                             const char *const *extra_headers, int n_extra,
                             const char *json_body, const char *upload_file,
                             const char *download_file, HttpResp *resp) {
    char hdrs[1024] = "";
    size_t hl = 0;
    if (bearer && bearer[0]) hl += (size_t)snprintf(hdrs + hl, sizeof(hdrs) - hl, "Authorization: Bearer %s\n", bearer);
    for (int i = 0; i < n_extra && hl < sizeof(hdrs); i++) {
        hl += (size_t)snprintf(hdrs + hl, sizeof(hdrs) - hl, "%s\n", extra_headers[i]);
    }
    if (hl < sizeof(hdrs) && (json_body || upload_file)) {
        hl += (size_t)snprintf(hdrs + hl, sizeof(hdrs) - hl, "Content-Type: %s\n",
                               json_body ? "application/json" : "application/octet-stream");
    }
    if (hl >= sizeof(hdrs)) return -1;

    char *upload = NULL;
    const void *body = json_body;
    size_t body_len = json_body ? strlen(json_body) : 0;
    if (!json_body && upload_file) {
        upload = read_all(upload_file, &body_len);
        if (!upload) return -1;
        body = upload;
    }
#ifdef _WIN32
    int rc = w32_http_request(method, url, hdrs, body, body_len, &resp->status, &resp->body, &resp->body_len,
                              resp->headers, sizeof(resp->headers));
#else
    int rc = android_http_request(method, url, hdrs, body, body_len, &resp->status, &resp->body, &resp->body_len,
                                  resp->headers, sizeof(resp->headers));
#endif
    free(upload);
    if (rc != 0) return -1;
    if (download_file) {
        /* The curl path writes the body to the file whatever the status; the body also stays in resp so an error
         * code ("device_revoked") is still readable. */
        if (write_atomic_bytes(download_file, resp->body ? resp->body : "", resp->body_len, 0644) != 0) {
            http_resp_free(resp);
            return -1;
        }
    }
    return 0;
}
#endif

static int http_exec(const char *method, const char *url, const char *bearer,
                     const char *const *extra_headers, int n_extra,
                     const char *json_body, const char *upload_file,
                     const char *download_file, HttpResp *resp) {
    memset(resp, 0, sizeof(*resp));
    if (!valid_http_url(url)) return -1;
#if defined(__ANDROID__) || defined(_WIN32)
    return http_exec_native(method, url, bearer, extra_headers, n_extra, json_body, upload_file, download_file, resp);
#else
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
#endif
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

/* SEND LOGS: the zip (logs_pack(upload=1)) to the worker's /v1/logs. No account is needed (a crash can come before the
 * registration); the device id rides along when there is one, so a report can be matched to a cloud save. */
int cloud_send_logs(const CloudState *st, const char *zip_path, const char *launcher, int is_portmaster,
                    char *id_out, size_t id_n, char *err, size_t err_n) {
    char url[320], h_plat[48], h_dev[96], h_ver[64];
    const char *base = st->worker_url[0] ? st->worker_url : DW3_DEFAULT_SYNC_URL;
    snprintf(url, sizeof(url), "%s/v1/logs", base);
    snprintf(h_plat, sizeof(h_plat), "X-Platform: %s", client_platform(is_portmaster));
    snprintf(h_dev, sizeof(h_dev), "X-Device-Id: %s", st->device_id);
    snprintf(h_ver, sizeof(h_ver), "X-Launcher: %s", launcher ? launcher : "");
    const char *hdrs[3] = {h_plat, h_ver, h_dev};
    HttpResp r;
    if (http_exec("POST", url, NULL, hdrs, st->device_id[0] ? 3 : 2, NULL, zip_path, NULL, &r) != 0) {
        snprintf(err, err_n, "OFFLINE OR NETWORK ERROR");
        return -1;
    }
    int ok = r.status == 200 && r.body;
    if (ok) {
        if (id_out && id_n) { id_out[0] = 0; jget_str(r.body, "id", id_out, id_n); }
    } else if (r.status == 429) {
        snprintf(err, err_n, "TOO MANY REPORTS, TRY LATER");
    } else if (r.status == 503) {
        snprintf(err, err_n, "THE SERVER IS BUSY, TRY LATER");
    } else if (r.status == 413) {
        snprintf(err, err_n, "THE ZIP IS TOO BIG");
    } else {
        snprintf(err, err_n, "SERVER ANSWERED %d", r.status);
    }
    http_resp_free(&r);
    return ok ? 0 : -1;
}

/* ---- Cloud API ---- */
int cloud_register(CloudState *st, const char *game_dir, int is_portmaster, char *err, size_t err_n) {
    char url[320], name[48], ename[96], body[256];
    default_device_name(name, sizeof(name), is_portmaster);
    json_escape(name, ename, sizeof(ename));
    snprintf(url, sizeof(url), "%s/v1/register", st->worker_url);
    snprintf(body, sizeof(body), "{\"device_name\":\"%s\",\"platform\":\"%s\"}",
             ename, client_platform(is_portmaster));

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
        /* the pair's sha (the card and its expansion); a worker from before it has the card's only */
        if (!jget_str(c1, "pair_sha256", st->cloud_sha256, sizeof(st->cloud_sha256)))
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
    /* The body is the card then its expansion's JSON (X-Save-Expansion-Size of it, 0: none, which clears the cloud's). */
    size_t sz = 0, el = 0;
    char *raw = pair_load(card, &sz, &el);
    if (!raw) {
        snprintf(msg_out, msg_n, "NO VALID LOCAL card1.mcd");
        return -1;
    }
    char sha[65], card_sha[65];
    sha256_bytes(raw, sz, sha);
    sha256_bytes(raw, PS1_MC_SIZE, card_sha);

    char snap[1100];
#if defined(__ANDROID__) || defined(_WIN32)
    /* Android and Windows have no /tmp; the settings directory is always writable. */
    snprintf(snap, sizeof(snap), "%s/dw3push_XXXXXX", game_dir);
#else
    snprintf(snap, sizeof(snap), "/tmp/dw3push_XXXXXX");
#endif
    int sfd = mkstemp(snap);
    if (sfd < 0) {
        free(raw);
        snprintf(msg_out, msg_n, "TMP WRITE FAILED");
        return -1;
    }
#ifdef _WIN32
    _setmode(sfd, O_BINARY); /* the card's bytes as they are, never a \n turned into \r\n */
#endif
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

    char url[320], h_sha[96], h_pair[112], h_ext[64], h_seq[48], h_force[24];
    snprintf(url, sizeof(url), "%s/v1/saves/card1/push", st->worker_url);
    snprintf(h_sha, sizeof(h_sha), "X-Save-Sha256: %s", card_sha);
    snprintf(h_pair, sizeof(h_pair), "X-Save-Pair-Sha256: %s", sha);
    snprintf(h_ext, sizeof(h_ext), "X-Save-Expansion-Size: %lu", (unsigned long)el);
    snprintf(h_seq, sizeof(h_seq), "X-Expected-Seq: %d", st->last_seq);
    const char *hdrs[5] = {h_sha, h_pair, h_ext, h_seq, NULL};
    int nh = 4;
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
        jget_str(r.body ? r.body : "", "server_sha256", st->cloud_sha256, sizeof(st->cloud_sha256)); /* the card's */
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

    /* Asked for, the expansion's JSON comes after the card (X-Save-Expansion-Size); a worker from before it sends the
     * card alone and no such header, read as no JSON. */
    static const char *const pull_hdrs[1] = {"X-Accept-Expansion: 1"};
    HttpResp r;
    if (http_exec("GET", url, st->device_token, pull_hdrs, 1, NULL, NULL, tmp, &r) != 0) {
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

    char dl_sha[65], card_sha[65], hdr_sha[65], ext_s[32], seq_s[32];
    size_t dl_len = 0;
    long el = 0;
    char *dl = read_all(tmp, &dl_len);
    unlink(tmp);
    if (header_val(r.headers, "X-Save-Expansion-Size", ext_s, sizeof(ext_s))) el = atol(ext_s);
    if (!dl || el < 0 || el > EXPANSION_MAX || dl_len != PS1_MC_SIZE + (size_t)el || (uint8_t)dl[0] != 0x4d ||
        (uint8_t)dl[1] != 0x43 || (el > 0 && dl[PS1_MC_SIZE] != '{')) {
        free(dl);
        http_resp_free(&r);
        snprintf(msg_out, msg_n, "CORRUPT CLOUD PAYLOAD");
        return -1;
    }
    sha256_bytes(dl, dl_len, dl_sha);
    sha256_bytes(dl, PS1_MC_SIZE, card_sha);
    if ((header_val(r.headers, "X-Save-Pair-Sha256", hdr_sha, sizeof(hdr_sha)) && strcasecmp(hdr_sha, dl_sha) != 0) ||
        (header_val(r.headers, "X-Save-Sha256", hdr_sha, sizeof(hdr_sha)) && strcasecmp(hdr_sha, card_sha) != 0)) {
        free(dl);
        http_resp_free(&r);
        snprintf(msg_out, msg_n, "CLOUD PAYLOAD SHA MISMATCH");
        return -1;
    }

    if (save_is_valid_mc(dst, NULL)) {
        save_backup_rotate(game_dir, "card1.mcd", backup_max);
    }
    int wrc = pair_write(dst, dl, (size_t)el); /* no JSON in the cloud: the local one (backed up) goes with the old card */
    free(dl);
    if (wrc != 0) {
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
    if (!jget_str(r.body, "pair_sha256", csha, sizeof(csha))) jget_str(r.body, "sha256", csha, sizeof(csha));
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
             code, ename, client_platform(is_portmaster));
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

/* update.h's request: a plain GET through the same path as the cloud's (curl, or Java on Android). GitHub's API asks
 * for its media type. */
int launcher_http_get(const char *url, char **body_out, size_t *len_out, int *status_out) {
    static const char *const hdrs[] = {"Accept: application/vnd.github+json"};
    HttpResp r;
    if (http_exec("GET", url, NULL, hdrs, 1, NULL, NULL, NULL, &r) != 0) return -1;
    *body_out = r.body;
    *len_out = r.body_len;
    *status_out = r.status;
    return 0;
}
