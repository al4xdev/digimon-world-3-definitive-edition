#ifndef DW3_SELFUPDATE_H
#define DW3_SELFUPDATE_H

#include "update.h"

/* The launcher installing a newer release over itself (Windows and the Linux AppImage), with dw2003-updater
 * (updater.c) doing every file operation. This side decides whether
 * it can, starts the updater from a COPY (<work>/run/), and shows what it says: the old launcher stays up on its
 * minigame meanwhile and only closes when the player says so. */
enum {
    SU_NONE = 0,
    SU_RUNNING, /* downloading, checking, preparing: SelfUpdate.text names it, .pct is how far */
    SU_READY,   /* staged: waits for selfupd_go() and for this process to end */
    SU_FAILED   /* SelfUpdate.text says why; nothing was changed (or everything was put back) */
};

typedef struct {
    int state;
    int pct;
    char text[96];
} SelfUpdate;

/* 1 when `u` can be installed here: an asset with its sha256, not under RecompLauncher, a writable folder, the updater
 * beside the launcher; on Linux an AppImage (APPIMAGE set). Otherwise the launcher only opens the release's page. */
int selfupd_available(const UpdateInfo *u, const char *exe_dir);
/* Starts the update (the updater with the asset's URL, size and sha256). 0 when it runs. */
int selfupd_start(const UpdateInfo *u, const char *exe_dir, char *err, size_t n);
/* The updater's state now (reads its status file; never blocks). */
void selfupd_poll(SelfUpdate *out);
/* The player confirmed: the updater may swap as soon as this process has ended. */
void selfupd_go(void);
/* Ends the updater and removes what it staged. */
void selfupd_cancel(void);
/* At start: removes the working folder an earlier update left (best effort; a file still in use stays for next time). */
void selfupd_cleanup(const char *exe_dir);

#endif
