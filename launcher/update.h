#ifndef DW3_UPDATE_H
#define DW3_UPDATE_H

#include <stddef.h>

/* The launcher's check for a newer release (update.c): GitHub's latest full release (never a pre-release) against the
 * running build's version and the platform's asset to install (selfupdate.h: Windows and the Linux AppImage), or else
 * only its page, which the launcher opens. A build that is not a release (a version with a suffix, "1.0.0-3-gabcdef1", "dev-abcdef1") never asks.
 * PortMaster has its own updates, so the PC launcher (Windows and Linux: DEBUG > CHECK FOR UPDATES, the notice on HOME)
 * and the Android launcher are the ones that call this; RecompLauncher's own updater is independent of it. */

#define DW3_RELEASES_API "https://api.github.com/repos/al4xdev/digimon-world-3-definitive-edition/releases/latest"
#define DW3_UPDATE_EVERY_S (12 * 3600) /* one request per 12 hours at most: GitHub allows 60 an hour per address */

enum {
    UPDATE_IDLE = 0,  /* not started, off, or a build that is not a release */
    UPDATE_CHECKING,  /* the request is out */
    UPDATE_CURRENT,   /* the running build is the latest release (or newer) */
    UPDATE_AVAILABLE, /* a newer release: UpdateInfo.latest, UpdateInfo.url */
    UPDATE_FAILED     /* no answer (offline, rate limit): nothing is shown */
};

typedef struct {
    int state;
    char latest[32]; /* the release's tag, "v1.0.1" */
    char url[256];   /* its page */
    /* What can be installed over the running build: this platform's asset (Windows: the zip; Linux: the AppImage), with
     * the sha256 the API publishes for it. Empty when there is none (Android, PortMaster, a release without it): the
     * launcher then only opens the page. */
    char asset_name[96];
    char asset_url[300];
    char asset_sha256[65];
    long asset_size;
} UpdateInfo;

/* The request the check makes (cloud.c: curl, or Java's HttpURLConnection on Android). 0 with a malloc'ed body the
 * caller frees and the HTTP status; -1 when no response arrived. */
int launcher_http_get(const char *url, char **body_out, size_t *len_out, int *status_out);

/* 1 when version `latest` ("v1.2.3" or "1.2.3") is newer than `current`; 0 otherwise, and always 0 when `current` is
 * not a plain release version (a suffix after its X.Y.Z). */
int update_is_newer(const char *latest, const char *current);

/* Starts the check on a thread of its own, at most once per run: a result younger than DW3_UPDATE_EVERY_S, kept in
 * <dir>/update.cache, is used instead of a request. `enabled` 0 (the player's setting) does nothing. */
void update_check_start(const char *dir, const char *current, int enabled);

/* The check's state now (it never blocks). */
void update_poll(UpdateInfo *out);

#endif
