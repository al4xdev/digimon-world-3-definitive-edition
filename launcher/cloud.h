#ifndef DW3_CLOUD_H
#define DW3_CLOUD_H

#include <stddef.h>
#include <stdint.h>

#define DW3_DEFAULT_SYNC_URL "https://dw3-cloud-sync.al4xdev.workers.dev"
#define PS1_MC_SIZE 131072
#define BACKUP_MAX_LIST 128
#define CLOUD_MAX_DEVICES 16

typedef struct {
    char name[96];     /* card1_YYYYMMDD_HHMMSS_12345678.mcd */
    char label[40];    /* 2026-11-14 12:34:56  [12345678] */
    char sha_short[9];
} SaveBackup;

typedef struct {
    char device_id[64];
    char name[48];
    char platform[20];
    char status[16];
    char country[12];
    long last_seen_at;
    int is_current;
} CloudDevice;

typedef struct {
    char worker_url[256];
    char account_id[64];
    char device_id[64];
    char device_token[128];
    char device_name[48];
    int registered;
    int revoked;
    char revoke_reason[96];
    int retention_days;
    int active_devices;
    long expires_at;
    long last_sync_at;
    int last_seq;
    char last_sha256[65];
    int cloud_seq;
    char cloud_sha256[65];
    long cloud_updated_at;
} CloudState;

typedef struct {
    char status[32]; /* pending | awaiting_approval | approved | denied */
    int expires_in;
    char pending_name[48];
    char pending_platform[20];
    char pending_country[12];
} PairStatus;

typedef struct {
    char claim_id[64];
    char claim_secret[128];
} PairClaim;

/* The expansion save: the game keeps what has no room on the card in a JSON beside it (card1.mcd -> card1.def.json,
 * port/game/definitive_expansion.h). The two are one save here: the sha, the backups (card1_<stamp>_<sha>.mcd and its
 * .def.json), restore, the cloud and the RecompLauncher mirror carry both. The sha is sha256 of the card then the JSON,
 * which is the card's own sha when there is no JSON: a card from before the expansion keeps the sha it always had.
 * The JSON's schema ("version") is the game's; migrating it is not done here yet (.plan/backlog/launcher-expansion-save.md). */
#define EXPANSION_MAX (1 << 20) /* the cloud's limit too (cloud-sync/src/types.ts) */
int save_expansion_path(const char *card_path, char *out, size_t n);

/* Local PS1 memory card validation & rotation (1..100). sha_out: the pair's (above). */
int save_is_valid_mc(const char *path, char sha_out[65]);
int save_backup_rotate(const char *game_dir, const char *slot_file, int max_keep);
int save_backup_list(const char *game_dir, SaveBackup *out, int max_items);
int save_backup_restore(const char *game_dir, const char *backup_name, int max_keep, char *err, size_t err_n);

/* RecompLauncher mirror. When RecompLauncher starts the launcher (RECOMPLAUNCHER is set) the card is mirrored to
 * <game_dir>/recomp/dw3-save.dw3s: one JSON header line (format, sha256, device, time, cloud seq) then the 128 KB card;
 * format 2 adds expansion_size and the expansion's JSON after the card (sha256 then the pair's).
 * That folder is the only save RecompLauncher knows (recomplauncher.json "saves"), so its own backups and cloud copy
 * the mirror and never card1.mcd. <game_dir>/recomp_mirror.state holds the sha of the last mirror this launcher wrote
 * or took in; a mirror that is valid but matches neither that nor card1.mcd was changed from outside (a restore, a
 * sync from another device) and is offered as a third choice next to the cloud's save and the local one. */
typedef struct {
    char sha256[65];
    char device_id[64];
    char device_name[48];
    long written_at;
    int cloud_seq;
} RecompMirror;

int recomp_active(void);
/* 0: written or already current; 1: left alone (a changed mirror waits for the player's choice, unless force); -1: no valid card or IO error. */
int recomp_mirror_write(const char *game_dir, const char *device_id, const char *device_name, int cloud_seq, int force);
/* 1 when the mirror is valid and changed from outside (see above); *out is its header. */
int recomp_mirror_foreign(const char *game_dir, RecompMirror *out);
/* Replaces card1.mcd with the mirror's card (the current one is backed up first); 0 on success. */
int recomp_mirror_adopt(const char *game_dir, int backup_max, char *err, size_t err_n);

/* Persistent cloud identity & sync metadata (<game_dir>/cloud_sync.json). */
void cloud_state_defaults(CloudState *st);
int cloud_state_load(CloudState *st, const char *game_dir);
int cloud_state_save(const CloudState *st, const char *game_dir);
void cloud_state_clear_auth(CloudState *st);

/* Cloud API operations (all return 0 on success, -1 on failure with err populated). */
int cloud_register(CloudState *st, const char *game_dir, int is_portmaster, char *err, size_t err_n);
int cloud_refresh_me(CloudState *st, const char *game_dir, char *err, size_t err_n);

/* Sync card1.mcd. If interactive && conflict, sets *conflict_out = 1 and returns 1 without overwriting. */
int cloud_sync_card1(CloudState *st, const char *game_dir, int backup_max, int interactive,
                     int *conflict_out, char *msg_out, size_t msg_n);
int cloud_push_card1(CloudState *st, const char *game_dir, int backup_max, int force, char *msg_out, size_t msg_n);
int cloud_pull_card1(CloudState *st, const char *game_dir, int backup_max, char *msg_out, size_t msg_n);

/* 2-step cross-play pairing & device management. */
int cloud_pair_create(CloudState *st, char code_out[8], int *expires_out, char *err, size_t err_n);
int cloud_pair_status(CloudState *st, const char *code, PairStatus *out, char *err, size_t err_n);
int cloud_pair_approve(CloudState *st, const char *code, int approve, char *err, size_t err_n);
int cloud_pair_claim(CloudState *st, const char *code, int is_portmaster, PairClaim *claim_out, char *err, size_t err_n);
int cloud_pair_poll(CloudState *st, const char *game_dir, const PairClaim *claim, int *approved_out, char *err, size_t err_n);

int cloud_list_devices(CloudState *st, CloudDevice *out, int max_n, int *count_out, char *err, size_t err_n);
int cloud_revoke_device(CloudState *st, const char *device_id, char *err, size_t err_n);

#endif
