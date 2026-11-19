/* The launcher's check for a newer release (update.h). */
#include "update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>

#include "win32.h"
static SRWLOCK up_lock = SRWLOCK_INIT;
#define UP_LOCK() AcquireSRWLockExclusive(&up_lock)
#define UP_UNLOCK() ReleaseSRWLockExclusive(&up_lock)
#else
#include <pthread.h>
static pthread_mutex_t up_lock = PTHREAD_MUTEX_INITIALIZER;
#define UP_LOCK() pthread_mutex_lock(&up_lock)
#define UP_UNLOCK() pthread_mutex_unlock(&up_lock)
#endif
static UpdateInfo up_info;
static int up_started;
static char up_cache[600];
static char up_current[64];

/* X.Y.Z at the start of `s` (after an optional "v"); the number of characters read, 0 when it is not there. */
static int parse_semver(const char *s, int v[3]) {
    const char *p = s;
    if (*p == 'v' || *p == 'V') p++;
    for (int i = 0; i < 3; i++) {
        if (*p < '0' || *p > '9') return 0;
        v[i] = 0;
        while (*p >= '0' && *p <= '9') v[i] = v[i] * 10 + (*p++ - '0');
        if (i < 2) {
            if (*p != '.') return 0;
            p++;
        }
    }
    return (int)(p - s);
}

int update_is_newer(const char *latest, const char *current) {
    int a[3], b[3];
    int nb;
    if (!latest || !current || !parse_semver(latest, a) || !(nb = parse_semver(current, b))) return 0;
    if (current[nb] != 0) return 0; /* "1.0.0-3-gabcdef1", "1.0.0-dirty": a build of its own, not a release */
    for (int i = 0; i < 3; i++) {
        if (a[i] != b[i]) return a[i] > b[i];
    }
    return 0;
}

/* The string value of "key" in a JSON object, unescaped only of \/ (a tag and a URL need no more). */
static int json_string(const char *json, const char *key, char *out, size_t n) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p++ != ':') return 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p++ != '"') return 0;
    size_t j = 0;
    while (*p && *p != '"' && j + 1 < n) {
        if (p[0] == '\\' && p[1] == '/') p++;
        else if (*p == '\\') return 0; /* nothing a tag or a release URL holds */
        out[j++] = *p++;
    }
    out[j] = 0;
    return *p == '"' && j > 0;
}

static void finish(int state, const char *latest, const char *url) {
    UP_LOCK();
    up_info.state = state;
    snprintf(up_info.latest, sizeof(up_info.latest), "%s", latest ? latest : "");
    snprintf(up_info.url, sizeof(up_info.url), "%s", url ? url : "");
    UP_UNLOCK();
}

/* The cache: "<epoch> <tag> <url>". The answer for a newer release is kept as it came, and judged again against the
 * running version on read, so an update installed since does not keep the notice up. */
static int cache_read(char *tag, size_t tn, char *url, size_t un) {
    FILE *f = fopen(up_cache, "r");
    if (!f) return 0;
    long when = 0;
    char t[64] = "", u[300] = "";
    int ok = fscanf(f, "%ld %63s %299s", &when, t, u) == 3;
    fclose(f);
    long now = (long)time(NULL);
    if (!ok || when > now || now - when >= DW3_UPDATE_EVERY_S) return 0;
    snprintf(tag, tn, "%s", t);
    snprintf(url, un, "%s", u);
    return 1;
}

static void cache_write(const char *tag, const char *url) {
    FILE *f = fopen(up_cache, "w");
    if (!f) return;
    fprintf(f, "%ld %s %s\n", (long)time(NULL), tag, url);
    fclose(f);
}

static void check_body(void) {
    char *body = NULL;
    size_t len = 0;
    int status = 0;
    char tag[32] = "", url[256] = "";

    if (launcher_http_get(DW3_RELEASES_API, &body, &len, &status) != 0 || status != 200 || !body) {
        free(body);
        finish(UPDATE_FAILED, NULL, NULL);
        return;
    }
    int ok = json_string(body, "tag_name", tag, sizeof(tag)) && json_string(body, "html_url", url, sizeof(url)) &&
             !strncmp(url, "https://github.com/", 19) && !strchr(url, ' ');
    free(body);
    if (!ok) {
        finish(UPDATE_FAILED, NULL, NULL);
        return;
    }
    cache_write(tag, url);
    finish(update_is_newer(tag, up_current) ? UPDATE_AVAILABLE : UPDATE_CURRENT, tag, url);
}

#ifdef _WIN32
static int check_thread(void *arg) {
    (void)arg;
    check_body();
    return 0;
}
#else
static void *check_thread(void *arg) {
    (void)arg;
    check_body();
    return NULL;
}
#endif

void update_check_start(const char *dir, const char *current, int enabled) {
    if (up_started || !enabled || !current) return;
    int v[3], n = parse_semver(current, v);
    if (!n || current[n] != 0) return; /* not a release: there is nothing to compare with */
    up_started = 1;
    snprintf(up_current, sizeof(up_current), "%s", current);
    snprintf(up_cache, sizeof(up_cache), "%s/update.cache", dir && dir[0] ? dir : ".");

    char tag[64], url[300];
    if (cache_read(tag, sizeof(tag), url, sizeof(url))) {
        finish(update_is_newer(tag, up_current) ? UPDATE_AVAILABLE : UPDATE_CURRENT, tag, url);
        return;
    }
    finish(UPDATE_CHECKING, NULL, NULL);
#ifdef _WIN32
    if (!w32_thread(check_thread, NULL)) {
        finish(UPDATE_FAILED, NULL, NULL);
        return;
    }
#else
    pthread_t t;
    if (pthread_create(&t, NULL, check_thread, NULL) != 0) {
        finish(UPDATE_FAILED, NULL, NULL);
        return;
    }
    pthread_detach(t);
#endif
}

void update_poll(UpdateInfo *out) {
    UP_LOCK();
    *out = up_info;
    UP_UNLOCK();
}
