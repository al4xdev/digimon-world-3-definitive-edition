/* dw2003-updater: the only program that touches the installed files when the launcher updates itself. A small
 * program of its own, no SDL and no window, so that it is the part that almost never changes .
 *  The launcher runs a COPY of it from <work>/run/ (an installed copy is a
 * file the update itself may replace; inside an AppImage it would hold the mount), then keeps its minigame up and
 * reads <status> while this program:
 *
 *   1. downloads <url> into <work>/ and checks its size and sha256 (nothing else is touched until both match),
 *   2. unpacks it into <work>/new/ (a zip: its one top folder dropped; a plain file: as it is), refusing any path that
 *      leaves that folder, and runs the staged launcher's --version once (--smoke),
 *   3. writes READY and waits: for <work>/update.go, which the launcher writes when the player says so, and for the
 *      launcher's process (--pid) to be gone for real,
 *   4. moves every file the package has out of <dir> into <backup>/ and the new one into its place (rename, on one
 *      volume, never an overwrite: Windows lets a running executable be renamed, not deleted or overwritten); the
 *      backup of the update before is deleted first,
 *   5. on any failure puts every moved file back, in reverse order, and starts the OLD launcher,
 *   6. starts --restart and ends.
 *
 * --rollback does step 5 by hand from <backup>. */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "compat.h"
#include "sha256.h"
#include "third_party/miniz/miniz.h"

#ifdef _WIN32
#include <windows.h>

#include "win32.h"
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <signal.h>
#include <sys/wait.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#define MAXFILES 2000
#define MAXTOTAL (300L * 1024 * 1024)

static struct {
    const char *kind, *url, *sha, *dir, *work, *backup, *status, *restart, *smoke, *target;
    long size, pid;
    int rollback;
} A;

static char logp[1200];

static void say(const char *fmt, ...) {
    FILE *f = fopen(logp, "a");
    if (!f) return;
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));
    fprintf(f, "%s ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

/* The launcher's view: one line "STATE pct text", replaced whole (a reader never sees half of one). */
static void set_status(const char *state, int pct, const char *text) {
    char tmp[1200], line[400];
    snprintf(tmp, sizeof(tmp), "%s.tmp", A.status);
    snprintf(line, sizeof(line), "%s %d %s\n", state, pct, text ? text : "");
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    fputs(line, f);
    fclose(f);
    rename_over(tmp, A.status);
}

static int fail(const char *why) {
    say("FAILED: %s", why);
    set_status("FAILED", 0, why);
    return 1;
}

/* ---- files ---- */
static int is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static int mkdir_p(const char *path) {
    char b[1200];
    snprintf(b, sizeof(b), "%s", path);
    for (char *p = b + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (!is_dir(b)) mkdir(b, 0755);
            *p = '/';
        }
    }
    if (!is_dir(b) && mkdir(b, 0755) != 0) return -1;
    return 0;
}

static int mkdir_parent(const char *file) {
    char b[1200];
    snprintf(b, sizeof(b), "%s", file);
    char *s = strrchr(b, '/');
    if (!s) return 0;
    *s = 0;
    return mkdir_p(b);
}

/* stat that does not follow a link (a symlink or a junction is removed, never entered). */
static int stat_nofollow(const char *p, struct stat *st) {
#ifdef _WIN32
    return stat(p, st);
#else
    return lstat(p, st);
#endif
}

static void rmtree(const char *path) {
    struct stat ls;
    if (stat_nofollow(path, &ls) == 0 && !S_ISDIR(ls.st_mode)) { remove(path); return; }
    DIR *d = opendir(path);
    if (!d) { remove(path); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char c[1400];
        snprintf(c, sizeof(c), "%s/%s", path, e->d_name);
        struct stat cs;
        if (stat_nofollow(c, &cs) == 0 && S_ISDIR(cs.st_mode)) rmtree(c);
        else remove(c);
    }
    closedir(d);
    rmdir(path);
}

/* Every regular file under root, as paths relative to it. */
static int walk(const char *root, const char *rel, char (*out)[400], int n, int max) {
    char dir[1400];
    snprintf(dir, sizeof(dir), "%s%s%s", root, rel[0] ? "/" : "", rel);
    DIR *d = opendir(dir);
    if (!d) return n;
    struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char r[400], full[1500];
        snprintf(r, sizeof(r), "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        snprintf(full, sizeof(full), "%s/%s", root, r);
        if (is_dir(full)) n = walk(root, r, out, n, max);
        else snprintf(out[n++], 400, "%s", r);
    }
    closedir(d);
    return n;
}

/* ---- download ---- */
static long now_ms(void) {
#ifdef _WIN32
    return (long)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
#endif
}

static void progress_status(long got, long total) {
    static long last_ms;
    long now = now_ms();
    if (now - last_ms < 150 && got != total) return;
    last_ms = now;
    int pct = total > 0 ? (int)(got * 100 / total) : 0;
    set_status("DOWNLOADING", pct, "");
}

#ifdef _WIN32
static int dl_progress(long got, long total, void *u) {
    (void)u;
    progress_status(got, total ? total : A.size);
    return 0;
}

static int download(const char *dest) {
    int status = 0;
    long total = 0;
    int rc = w32_http_download(A.url, dest, &status, &total, dl_progress, NULL);
    if (rc != 0 || status != 200) { say("download: rc %d, HTTP %d", rc, status); return -1; }
    return 0;
}
#else
/* The launcher already talks through curl on Linux (cloud.c): the same here, its progress read from the file's size. */
static int download(const char *dest) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execlp("curl", "curl", "-fsSL", "--connect-timeout", "10", "--max-time", "900", "-o", dest, A.url, (char *)NULL);
        _exit(127);
    }
    int st = 0;
    for (;;) {
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) break;
        struct stat s;
        if (stat(dest, &s) == 0) progress_status((long)s.st_size, A.size);
        SLEEP_MS(100);
    }
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { say("curl ended with status %d", st); return -1; }
    return 0;
}
#endif

/* ---- the package ---- */
/* An entry's name inside the zip, made into a path under new/, or 0 when it is not a plain relative one. */
static int safe_rel(const char *name, int strip, char *out, size_t n) {
    if (!name[0] || name[0] == '/' || name[0] == '\\' || strchr(name, '\\') || strchr(name, ':')) return 0;
    const char *p = name;
    for (int i = 0; i < strip; i++) {
        p = strchr(p, '/');
        if (!p) return 0;
        p++;
    }
    if (!*p) return 0;
    for (const char *q = p; *q;) {
        const char *e = strchr(q, '/');
        size_t l = e ? (size_t)(e - q) : strlen(q);
        if (l == 0 || (l == 1 && q[0] == '.') || (l == 2 && q[0] == '.' && q[1] == '.')) return 0;
        q += l + (e ? 1 : 0);
        if (!e) break;
    }
    snprintf(out, n, "%s", p);
    return 1;
}

static int unpack_zip(const char *zip, const char *newdir) {
    mz_zip_archive z;
    memset(&z, 0, sizeof(z));
    if (!mz_zip_reader_init_file(&z, zip, 0)) return fail("THE PACKAGE IS NOT A ZIP");
    mz_uint n = mz_zip_reader_get_num_files(&z);
    if (n == 0 || n > MAXFILES) { mz_zip_reader_end(&z); return fail("THE PACKAGE HAS AN ODD NUMBER OF FILES"); }
    long total = 0;
    for (mz_uint i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st)) { mz_zip_reader_end(&z); return fail("THE PACKAGE'S LIST IS BROKEN"); }
        if (mz_zip_reader_is_file_a_directory(&z, i)) continue;
        total += (long)st.m_uncomp_size;
        if (total > MAXTOTAL) { mz_zip_reader_end(&z); return fail("THE PACKAGE IS TOO BIG UNPACKED"); }
    }
    int strip = !strcmp(A.kind, "zip") ? 1 : 0;
    for (mz_uint i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        mz_zip_reader_file_stat(&z, i, &st);
        if (mz_zip_reader_is_file_a_directory(&z, i)) continue;
        char rel[400], dest[1700];
        if (!safe_rel(st.m_filename, strip, rel, sizeof(rel))) {
            say("refused entry: %s", st.m_filename);
            mz_zip_reader_end(&z);
            return fail("THE PACKAGE HAS A PATH THAT LEAVES ITS FOLDER");
        }
        snprintf(dest, sizeof(dest), "%s/%s", newdir, rel);
        if (mkdir_parent(dest) != 0 || !mz_zip_reader_extract_to_file(&z, i, dest, 0)) {
            say("could not unpack %s", rel);
            mz_zip_reader_end(&z);
            return fail("COULD NOT UNPACK THE PACKAGE");
        }
        set_status("PREPARING", (int)((i + 1) * 100 / n), "");
    }
    mz_zip_reader_end(&z);
    return 0;
}

/* ---- processes ---- */
/* The staged launcher says its version and ends: it runs at all, with the libraries it needs. 1 when it did. */
static int smoke_ok(const char *exe) {
#ifdef _WIN32
    const char *argv[] = {exe, "--version", NULL};
    void *h = NULL;
    unsigned long pid, code = 1;
    if (w32_start(argv, NULL, &pid, &h) != 0) return 0;
    return w32_handle_wait_kill(h, 15000, &code) && code == 0;
#else
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        execl(exe, exe, "--version", (char *)NULL);
        _exit(127);
    }
    for (int i = 0; i < 150; i++) {
        int st;
        if (waitpid(pid, &st, WNOHANG) == pid) return WIFEXITED(st) && WEXITSTATUS(st) == 0;
        SLEEP_MS(100);
    }
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return 0;
#endif
}

static int pid_gone(void) {
    if (A.pid <= 0) return 1;
#ifdef _WIN32
    return w32_pid_gone((unsigned long)A.pid, 0);
#else
    return kill((pid_t)A.pid, 0) != 0 && errno == ESRCH;
#endif
}

static void start_detached(const char *exe) {
    if (!exe || !exe[0]) return;
    say("starting %s", exe);
#ifdef _WIN32
    const char *argv[] = {exe, NULL};
    unsigned long pid;
    if (w32_start(argv, A.dir, &pid, NULL) != 0) say("could not start it");
#else
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        if (chdir(A.dir) != 0) { /* the launcher finds its own place */ }
        execl(exe, exe, (char *)NULL);
        _exit(127);
    }
#endif
}

/* ---- the swap ---- */
typedef struct {
    char rel[400];
    int had_old; /* the install had a file there, now in backup/ */
    int placed;  /* the new file is in the install: only then is what is there ours to take away again */
} Move;
static Move *moves;
static int nmoves;

static int put_back(void) {
    int bad = 0;
    for (int i = nmoves - 1; i >= 0; i--) {
        char inst[1700], bak[1700], failed[1700];
        snprintf(inst, sizeof(inst), "%s/%s", A.dir, moves[i].rel);
        snprintf(bak, sizeof(bak), "%s/%s", A.backup, moves[i].rel);
        snprintf(failed, sizeof(failed), "%s/failed/%s", A.work, moves[i].rel);
        /* A file whose old copy could not be moved is still the player's own: never touched unless ours was put there. */
        if (moves[i].placed && exists(inst)) {
            mkdir_parent(failed);
            if (rename_over(inst, failed) != 0) { say("rollback: could not move %s away", moves[i].rel); bad = 1; }
        }
        if (moves[i].had_old && rename_over(bak, inst) != 0) { say("rollback: could not restore %s", moves[i].rel); bad = 1; }
    }
    return bad;
}

/* Renames that an antivirus scanning the file may refuse for a moment. */
static int move_retry(const char *from, const char *to) {
    for (int i = 0; i < 5; i++) {
        if (rename_over(from, to) == 0) return 0;
        SLEEP_MS(200);
    }
    return -1;
}

static int swap(char (*files)[400], int nfiles) {
    rmtree(A.backup); /* the earlier update's: only one is ever kept */
    moves = calloc((size_t)nfiles, sizeof(Move));
    if (!moves || mkdir_p(A.backup) != 0) return -1;
    for (int i = 0; i < nfiles; i++) {
        char inst[1700], bak[1700], neu[1700];
        snprintf(inst, sizeof(inst), "%s/%s", A.dir, files[i]);
        snprintf(bak, sizeof(bak), "%s/%s", A.backup, files[i]);
        snprintf(neu, sizeof(neu), "%s/new/%s", A.work, files[i]);
        snprintf(moves[nmoves].rel, sizeof(moves[nmoves].rel), "%s", files[i]);
        moves[nmoves].had_old = 0;
        moves[nmoves].placed = 0;
        nmoves++;
        if (mkdir_parent(inst) != 0 || mkdir_parent(bak) != 0) return -1;
        if (exists(inst)) {
            if (move_retry(inst, bak) != 0) { say("could not move %s to the backup", files[i]); return -1; }
            moves[nmoves - 1].had_old = 1;
        }
        if (move_retry(neu, inst) != 0) { say("could not put %s in place", files[i]); return -1; }
        moves[nmoves - 1].placed = 1;
#ifndef _WIN32
        chmod(inst, 0755); /* the AppImage must run; no other file of a Linux package is a program */
#endif
        set_status("SWAPPING", (i + 1) * 100 / nfiles, "");
    }
    return 0;
}

/* --rollback: everything in <backup> back into <dir>. */
static int manual_rollback(void) {
    static char files[MAXFILES][400];
    int n = walk(A.backup, "", files, 0, MAXFILES);
    if (n == 0) { fprintf(stderr, "nothing in %s\n", A.backup); return 1; }
    for (int i = 0; i < n; i++) {
        char from[1700], to[1700];
        snprintf(from, sizeof(from), "%s/%s", A.backup, files[i]);
        snprintf(to, sizeof(to), "%s/%s", A.dir, files[i]);
        mkdir_parent(to);
        if (move_retry(from, to) != 0) { fprintf(stderr, "could not restore %s\n", files[i]); return 1; }
    }
    printf("restored %d files from %s\n", n, A.backup);
    say("rolled back by hand: %d files from %s", n, A.backup); /* the Windows build has no console: the log says it */
    return 0;
}

static const char *arg_of(int argc, char **argv, const char *name) {
    for (int i = 1; i + 1 < argc; i++) {
        if (!strcmp(argv[i], name)) return argv[i + 1];
    }
    return NULL;
}

int main(int argc, char **argv) {
    A.kind = arg_of(argc, argv, "--kind");
    A.url = arg_of(argc, argv, "--url");
    A.sha = arg_of(argc, argv, "--sha256");
    A.dir = arg_of(argc, argv, "--dir");
    A.work = arg_of(argc, argv, "--work");
    A.backup = arg_of(argc, argv, "--backup");
    A.status = arg_of(argc, argv, "--status");
    A.restart = arg_of(argc, argv, "--restart");
    A.smoke = arg_of(argc, argv, "--smoke");
    A.target = arg_of(argc, argv, "--target");
    const char *sz = arg_of(argc, argv, "--size"), *pid = arg_of(argc, argv, "--pid");
    A.size = sz ? atol(sz) : 0;
    A.pid = pid ? atol(pid) : 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rollback")) A.rollback = 1;
    }
    if (!A.dir || !A.backup) { fprintf(stderr, "usage: dw2003-updater --rollback --dir D --backup B\n"); return 64; }
    snprintf(logp, sizeof(logp), "%s/update.log", A.dir);
    if (A.rollback) return manual_rollback();
    if (!A.kind || !A.url || !A.sha || !A.work || !A.status || strlen(A.sha) != 64 || A.size <= 0 ||
        (strcmp(A.kind, "zip") && strcmp(A.kind, "file"))) {
        fprintf(stderr, "dw2003-updater: bad arguments\n");
        return 64;
    }
    say("update: %s (%ld bytes) into %s", A.url, A.size, A.dir);

    /* 1. download and check */
    char pkg[1300], newdir[1300];
    snprintf(pkg, sizeof(pkg), "%s/pkg.part", A.work);
    snprintf(newdir, sizeof(newdir), "%s/new", A.work);
    rmtree(newdir);
    if (mkdir_p(A.work) != 0) return fail("COULD NOT MAKE THE WORK FOLDER");
    set_status("DOWNLOADING", 0, "");
    if (download(pkg) != 0) { remove(pkg); return fail("THE DOWNLOAD FAILED"); }
    set_status("CHECKING", 0, "");
    char got[65];
    size_t gsz = 0;
    if (sha256_file(pkg, got, &gsz) != 0) return fail("COULD NOT READ THE DOWNLOAD");
    if ((long)gsz != A.size || strcasecmp(got, A.sha) != 0) {
        say("size %zu (want %ld), sha256 %s (want %s)", gsz, A.size, got, A.sha);
        remove(pkg);
        return fail("THE DOWNLOAD DOES NOT MATCH ITS CHECKSUM");
    }

    /* 2. unpack and look at it */
    mkdir_p(newdir);
    if (!strcmp(A.kind, "zip")) {
        if (unpack_zip(pkg, newdir) != 0) return 1;
    } else {
        char dest[1700];
        snprintf(dest, sizeof(dest), "%s/%s", newdir, A.target && A.target[0] ? A.target : "package");
        if (rename_over(pkg, dest) != 0) return fail("COULD NOT STAGE THE PACKAGE");
#ifndef _WIN32
        chmod(dest, 0755);
#endif
    }
    if (A.smoke && A.smoke[0]) {
        char exe[1700];
        snprintf(exe, sizeof(exe), "%s/%s", newdir, A.smoke);
#ifndef _WIN32
        chmod(exe, 0755); /* a zip entry unpacked has no execute bit */
#endif
        if (!smoke_ok(exe)) return fail("THE NEW LAUNCHER DOES NOT START");
    }
    static char files[MAXFILES][400];
    int nfiles = walk(newdir, "", files, 0, MAXFILES);
    if (nfiles == 0) return fail("THE PACKAGE IS EMPTY");

    /* 3. ready: wait for the player's go-ahead and for the launcher to be truly gone */
    set_status("READY", 100, "");
    say("ready, waiting for the go-ahead");
    char gof[1300];
    snprintf(gof, sizeof(gof), "%s/update.go", A.work);
    for (;;) {
        /* gone first: a launcher that writes update.go and ends between the two looks must still count as going ahead */
        int gone = pid_gone(), go = exists(gof);
        if (go && gone) break;
        if (gone && !go) { say("the launcher ended without the go-ahead: staged, nothing changed"); return 0; }
        SLEEP_MS(200);
    }

    /* 4. swap, with the way back */
    set_status("SWAPPING", 0, "");
    if (swap(files, nfiles) != 0) {
        int bad = put_back();
        say("rolled back%s", bad ? " (with errors)" : "");
        set_status("FAILED", 0, bad ? "THE UPDATE FAILED AND NOT EVERYTHING WENT BACK" : "THE UPDATE FAILED, THE OLD VERSION IS BACK");
        start_detached(A.restart);
        return 1;
    }
    say("done: %d files", nfiles);
    set_status("DONE", 100, "");
    start_detached(A.restart);
    return 0;
}
