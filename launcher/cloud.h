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

/* Local PS1 memory card validation & rotation (1..100). */
int save_is_valid_mc(const char *path, char sha_out[65]);
int save_backup_rotate(const char *game_dir, const char *slot_file, int max_keep);
int save_backup_list(const char *game_dir, SaveBackup *out, int max_items);
int save_backup_restore(const char *game_dir, const char *backup_name, int max_keep, char *err, size_t err_n);

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
