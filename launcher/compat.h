#ifndef DW3_COMPAT_H
#define DW3_COMPAT_H
/* What the launcher takes from POSIX that Windows (llvm-mingw, UCRT) spells differently or lacks. The bigger pieces
 * (the game's process, the file dialog, HTTP) are win32.h's. On Linux and Android this header adds nothing but
 * rename_over and exe_ok, which are the POSIX calls themselves there.
 *
 * Paths stay narrow (char, UTF-8) on Windows too: the launcher's manifest sets the process's code page to UTF-8
 * (launcher/windows/dw2003-launcher.manifest), so fopen, opendir and the -A calls take UTF-8 (Windows 10 1903 or
 * newer). */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>

#define mkdir(path, mode) _mkdir(path)
#define fsync(fd) _commit(fd)
#define setenv(name, value, overwrite) _putenv_s((name), (value))
#define localtime_r(t, tm) (localtime_s((tm), (t)) == 0 ? (tm) : NULL)
/* realpath without the existence check (every caller checks the path first); a drive path with backslashes. */
#define realpath(path, out) _fullpath((out), (path), 1024)

/* rename() does not replace an existing file on Windows: MoveFileEx does (win32.c). */
int w32_rename_over(const char *from, const char *to);
#define rename_over(from, to) w32_rename_over((from), (to))
/* X_OK is not a mode the UCRT's access() accepts (an invalid-parameter abort): a program exists or it does not. */
#define exe_ok(path) (access((path), F_OK) == 0)
#define PATH_SEPS "/\\"
#else
#define rename_over(from, to) rename((from), (to))
#define exe_ok(path) (access((path), X_OK) == 0)
#define PATH_SEPS "/"
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* The last path separator in p ('/', and '\\' on Windows), or NULL. */
static inline char *path_last_sep(const char *p) {
    const char *last = NULL;
    for (const char *s = p; *s; s++) {
        if (*s == '/'
#ifdef _WIN32
            || *s == '\\'
#endif
        ) {
            last = s;
        }
    }
    return (char *)last;
}

/* A filesystem root: "/", or on Windows a drive's ("C:\", "C:/", "C:"). The browser stops going up there. */
static inline int path_is_root(const char *p) {
    if (p[0] == '/' && p[1] == 0) return 1;
#ifdef _WIN32
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':' &&
        (p[2] == 0 || ((p[2] == '\\' || p[2] == '/') && p[3] == 0))) {
        return 1;
    }
#endif
    return 0;
}
#endif
