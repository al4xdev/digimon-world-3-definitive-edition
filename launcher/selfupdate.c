/* The launcher's side of the self-update (selfupdate.h). */
#include "selfupdate.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cloud.h"
#include "compat.h"

#ifdef _WIN32
#include <windows.h>

#include "win32.h"
#define UPDATER_NAME "dw2003-updater.exe"
static void *su_handle;
static unsigned long su_pid;
#else
#include <signal.h>
#include <sys/wait.h>
#define UPDATER_NAME "dw2003-updater"
static long su_pid;
#endif

static int su_active;
static char su_work[1300], su_status[1400];

typedef struct {
    char dir[1100], work[1200], backup[1200], restart[1200], target[200], smoke[200], updater[1300];
    const char *kind;
} Layout;

/* Where this build is installed and how it restarts. Windows: the folder of the launcher's exe, restarted by it. Linux:
 * the folder of the AppImage file, which is the one file that is replaced (and restarted). 0 when neither applies. */
static int layout(const char *exe_dir, Layout *L) {
    memset(L, 0, sizeof(*L));
#ifdef _WIN32
    char self[1100];
    if (w32_exe_path(self, sizeof(self)) != 0) return 0;
    for (char *p = self; *p; p++) if (*p == '\\') *p = '/';
    snprintf(L->dir, sizeof(L->dir), "%s", exe_dir);
    snprintf(L->work, sizeof(L->work), "%s/.update", L->dir);
    snprintf(L->backup, sizeof(L->backup), "%s/dw2003-backup", L->dir); /* a bare "backup" may be the player's own folder, and the next update deletes this one */
    snprintf(L->restart, sizeof(L->restart), "%s", self);
    const char *slash = strrchr(self, '/');
    snprintf(L->smoke, sizeof(L->smoke), "%s", slash ? slash + 1 : self);
    L->kind = "zip";
    snprintf(L->updater, sizeof(L->updater), "%s/%s", exe_dir, UPDATER_NAME);
#else
    const char *ai = getenv("APPIMAGE");
    if (!ai || ai[0] != '/') return 0;
    snprintf(L->restart, sizeof(L->restart), "%s", ai);
    const char *slash = strrchr(ai, '/');
    snprintf(L->dir, sizeof(L->dir), "%.*s", (int)(slash - ai) ? (int)(slash - ai) : 1, ai);
    snprintf(L->target, sizeof(L->target), "%s", slash + 1);
    snprintf(L->smoke, sizeof(L->smoke), "%s", slash + 1);
    snprintf(L->work, sizeof(L->work), "%s/.dw2003-update", L->dir);
    snprintf(L->backup, sizeof(L->backup), "%s/.dw2003-backup", L->dir);
    L->kind = "file";
    snprintf(L->updater, sizeof(L->updater), "%s/%s", exe_dir, UPDATER_NAME);
#endif
    return 1;
}

int selfupd_available(const UpdateInfo *u, const char *exe_dir) {
    Layout L;
    if (!u->asset_url[0] || strlen(u->asset_sha256) != 64 || u->asset_size <= 0) return 0;
    if (recomp_active()) return 0; /* RecompLauncher updates the launcher itself */
    if (!layout(exe_dir, &L)) return 0;
    if (access(L.updater, F_OK) != 0) return 0;
    /* access(W_OK) on a folder says yes on Windows whatever its permissions (Program Files): try to make a file */
    char probe[1300];
    snprintf(probe, sizeof(probe), "%s/.dw2003-write-test", L.dir);
    FILE *t = fopen(probe, "wb");
    if (!t) return 0;
    fclose(t);
    remove(probe);
    return 1;
}

static void rmtree(const char *path) {
    /* on Windows the updater's own copy may still be running for a moment: what cannot be removed now goes next start */
    DIR *d = opendir(path);
    if (!d) { remove(path); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char c[1500];
        snprintf(c, sizeof(c), "%s/%s", path, e->d_name);
        struct stat st;
#ifdef _WIN32
        int r = stat(c, &st);
#else
        int r = lstat(c, &st); /* a link is removed, never entered */
#endif
        if (r == 0 && S_ISDIR(st.st_mode)) rmtree(c);
        else remove(c);
    }
    closedir(d);
    rmdir(path);
}

static void mkdirs(const char *path) {
    char b[1300];
    snprintf(b, sizeof(b), "%s", path);
    for (char *p = b + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(b, 0755); *p = '/'; }
    }
    mkdir(b, 0755);
}

static int copy_file(const char *from, const char *to) {
    FILE *a = fopen(from, "rb"), *b = a ? fopen(to, "wb") : NULL;
    if (!a || !b) { if (a) fclose(a); return -1; }
    char buf[16384];
    size_t n;
    int rc = 0;
    while ((n = fread(buf, 1, sizeof(buf), a)) > 0) {
        if (fwrite(buf, 1, n, b) != n) { rc = -1; break; }
    }
    fclose(a);
    if (fclose(b) != 0) rc = -1;
#ifndef _WIN32
    chmod(to, 0755);
#endif
    return rc;
}

int selfupd_start(const UpdateInfo *u, const char *exe_dir, char *err, size_t n) {
    Layout L;
    if (su_active || !selfupd_available(u, exe_dir) || !layout(exe_dir, &L)) { snprintf(err, n, "CANNOT UPDATE HERE"); return -1; }
    rmtree(L.work);
    char run[1300], copy[1400], szs[24], pids[24];
    snprintf(run, sizeof(run), "%s/run", L.work);
    mkdirs(run);
    snprintf(copy, sizeof(copy), "%s/%s", run, UPDATER_NAME);
    if (copy_file(L.updater, copy) != 0) { snprintf(err, n, "COULD NOT PREPARE THE UPDATER"); return -1; }
    snprintf(su_work, sizeof(su_work), "%s", L.work);
    snprintf(su_status, sizeof(su_status), "%s/update.status", L.dir);
    remove(su_status);
    snprintf(szs, sizeof(szs), "%ld", u->asset_size);
#ifdef _WIN32
    snprintf(pids, sizeof(pids), "%lu", (unsigned long)GetCurrentProcessId());
#else
    snprintf(pids, sizeof(pids), "%ld", (long)getpid());
#endif
    const char *argv[32];
    int ac = 0;
    argv[ac++] = copy;
    argv[ac++] = "--kind"; argv[ac++] = L.kind;
    argv[ac++] = "--url"; argv[ac++] = u->asset_url;
    argv[ac++] = "--sha256"; argv[ac++] = u->asset_sha256;
    argv[ac++] = "--size"; argv[ac++] = szs;
    argv[ac++] = "--dir"; argv[ac++] = L.dir;
    argv[ac++] = "--work"; argv[ac++] = L.work;
    argv[ac++] = "--backup"; argv[ac++] = L.backup;
    argv[ac++] = "--status"; argv[ac++] = su_status;
    argv[ac++] = "--pid"; argv[ac++] = pids;
    argv[ac++] = "--restart"; argv[ac++] = L.restart;
    argv[ac++] = "--smoke"; argv[ac++] = L.smoke;
    if (L.target[0]) { argv[ac++] = "--target"; argv[ac++] = L.target; }
    argv[ac] = NULL;
#ifdef _WIN32
    if (w32_start(argv, L.dir, &su_pid, &su_handle) != 0) { snprintf(err, n, "COULD NOT START THE UPDATER"); return -1; }
#else
    pid_t pid = fork();
    if (pid < 0) { snprintf(err, n, "COULD NOT START THE UPDATER"); return -1; }
    if (pid == 0) {
        setsid(); /* it outlives this launcher: that is the point */
        if (!freopen("/dev/null", "r", stdin) || !freopen("/dev/null", "w", stdout) || !freopen("/dev/null", "w", stderr)) _exit(126);
        execv(copy, (char *const *)argv);
        _exit(127);
    }
    su_pid = pid;
#endif
    su_active = 1;
    return 0;
}

static int updater_gone(void) {
#ifdef _WIN32
    return w32_pid_gone(su_pid, 0);
#else
    int st;
    pid_t r = waitpid((pid_t)su_pid, &st, WNOHANG);
    return r == (pid_t)su_pid || (r < 0 && errno == ECHILD);
#endif
}

void selfupd_poll(SelfUpdate *out) {
    memset(out, 0, sizeof(*out));
    if (!su_active) return;
    char line[256] = "", st[24] = "";
    int pct = 0;
    char text[160] = "";
    FILE *f = fopen(su_status, "r");
    if (f) {
        if (fgets(line, sizeof(line), f)) {
            sscanf(line, "%23s %d %159[^\n]", st, &pct, text);
        }
        fclose(f);
    }
    out->pct = pct;
    snprintf(out->text, sizeof(out->text), "%s", text);
    if (!strcmp(st, "READY")) out->state = SU_READY;
    else if (!strcmp(st, "FAILED")) { out->state = SU_FAILED; su_active = 0; }
    else if (!strcmp(st, "SWAPPING") || !strcmp(st, "DONE")) out->state = SU_READY;
    else {
        out->state = SU_RUNNING;
        snprintf(out->text, sizeof(out->text), "%s", st[0] ? st : "STARTING");
        /* the updater ended without a word: it crashed or could not start */
        if (updater_gone()) {
            out->state = SU_FAILED;
            snprintf(out->text, sizeof(out->text), "THE UPDATER ENDED UNEXPECTEDLY");
            su_active = 0;
        }
    }
}

void selfupd_go(void) {
    if (!su_active) return;
    char p[1400];
    snprintf(p, sizeof(p), "%s/update.go", su_work);
    FILE *f = fopen(p, "w");
    if (f) { fputs("go\n", f); fclose(f); }
}

void selfupd_cancel(void) {
    if (!su_active) return;
#ifdef _WIN32
    if (su_handle) w32_handle_kill(su_handle);
    su_handle = NULL;
#else
    kill(-(pid_t)su_pid, SIGTERM); /* its own process group (setsid): the updater and the curl it runs */
    kill((pid_t)su_pid, SIGTERM);
    waitpid((pid_t)su_pid, NULL, 0);
#endif
    su_active = 0;
    rmtree(su_work);
    remove(su_status);
}

void selfupd_cleanup(const char *exe_dir) {
    Layout L;
    if (!layout(exe_dir, &L)) return;
    char st[1400];
    rmtree(L.work);
    snprintf(st, sizeof(st), "%s/update.status", L.dir);
    remove(st);
}
