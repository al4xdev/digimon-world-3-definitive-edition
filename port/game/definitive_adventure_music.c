/* Digimon Adventure ("Butter-Fly") music (tools/gen_adventure_music.py): played by the game's LIBSND from a SEP
 * generated from the owner's local MIDI, on the samples of BGM031 (sound bank 33) with BOSS02's vag 8 guitar sample
 * copied into vag 14 and program 5 retargeted to it.
 *
 * The score is in definitive_adventure_music.inc, which is generated and not committed. Without it (no MIDI or no disc
 * when it was generated) dw3_adventure_music_available() returns 0, dw3_adventure_music() is a no-op, and all sound
 * hooks cleanly do nothing.
 *
 * Uses its own bank id (DW3_ADVENTURE_BANK = 73, after DW3_VELVET_BANK = 72) so BGM031 and Velvet keep their own
 * scores and bank edits. */
#include "common.h"

#include "psyq/libsnd.h"
#include "sound.h"

#include "psxstack/spu.h"

void port_log(const char *fmt, ...); /* psxstack/port_runtime.h's */

#define DW3_ADVENTURE_BANK 73 /* sound_banks has 72 slots (0..71), 72 is Velvet, 73 is Adventure */
#define BGM031 33             /* the disc's bank this one copies (SOUND/BGM031) */
#define BUFFER_SIZE 0xA000    /* entries 1 and 2: sound_buffer_1/2 */

extern SoundBank *sound_banks[];

#if defined(__has_include)
#if __has_include("definitive_adventure_music.inc")
#include "definitive_adventure_music.inc"
#define ADV_HAVE_SCORE 1
#endif
#endif

#ifdef ADV_HAVE_SCORE

static SoundBank adv_bank;
static int adv_reverb_live; /* this score raised the reverb depth: the game's own is 0 (sound_init) */

static u16 rd16(const u8 *p) {
    return (u16)(p[0] | p[1] << 8);
}

static void wr16(u8 *p, int v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
}

/* The VAB header of the buffer (the file's sub-file table: word 0 is VAB header, word 1 is SEP). */
static u8 *vab_head(s32 *buffer) {
    u8 *head = (u8 *)buffer + buffer[0];

    return head[0] == 'p' && head[1] == 'B' && head[2] == 'A' && head[3] == 'V' ? head : NULL;
}

static u8 *tone_of(u8 *head, int prog, int tone) {
    int programs = rd16(head + 0x12);
    int block = 0;
    int i;

    for (i = 0; i < prog; i++) {
        block += head[0x20 + 16 * i] != 0;
    }
    if (head[0x20 + 16 * prog] == 0 || block >= programs || tone >= head[0x20 + 16 * prog]) {
        return NULL;
    }
    return head + 0x20 + 0x800 + 0x200 * block + 32 * tone;
}

#endif

void dw3_adventure_music(s32 *music, s32 *sound) {
#ifdef ADV_HAVE_SCORE
    *music = DW3_ADVENTURE_BANK;
    *sound = 0x60000000 | (DW3_ADVENTURE_BANK << 18); /* bit 30: current music; sequence 0 of SEP 0 */
#else
    (void)music;
    (void)sound;
#endif
}

int dw3_adventure_music_available(void) {
#ifdef ADV_HAVE_SCORE
    return 1;
#else
    return 0;
#endif
}

/* sound_load_bank: the record to load for `id`. Never indexes sound_banks[] out of bounds. */
SoundBank *dw3_adventure_sound_bank(s32 id) {
#ifdef ADV_HAVE_SCORE
    if (id == DW3_ADVENTURE_BANK) {
        adv_bank = *sound_banks[BGM031];
        return &adv_bank;
    }
#endif
    if (id >= 0 && id < 72) {
        return sound_banks[id];
    }
    return NULL;
}

/* sound_update_loading, step 1, before the VAB header is opened: inject the SEP score and tone edits. */
void dw3_adventure_sound_header(s32 id, s32 *buffer) {
#ifdef ADV_HAVE_SCORE
    u8 *head;
    u8 *sep;
    unsigned i;

    if (id != DW3_ADVENTURE_BANK) {
        return;
    }
    head = vab_head(buffer);
    sep = (u8 *)buffer + buffer[1];
    if (head == NULL || sep < head || sep + sizeof(adv_score) > (u8 *)buffer + BUFFER_SIZE) {
        port_log("adventure music: the bank's header does not hold the score (%u bytes), BGM031's own plays",
                 (unsigned)sizeof(adv_score));
        return;
    }
    for (i = 0; i < sizeof(adv_score); i++) {
        sep[i] = adv_score[i];
    }
    for (i = 0; i < sizeof(adv_tone_edits) / sizeof(adv_tone_edits[0]); i++) {
        const int *e = adv_tone_edits[i];
        u8 *t = tone_of(head, e[0], e[1]);

        if (t == NULL) {
            continue;
        }
        if (e[2] >= 0) {
            t[2] = (u8)e[2];
        }
        if (e[3] >= 0) {
            t[4] = (u8)e[3];
        }
        if (e[4] >= 0) {
            t[5] = (u8)e[4];
        }
        if (e[5] >= 0) {
            t[1] = (u8)e[5];
        }
        if (e[6] >= 0) {
            wr16(t + 0x10, e[6]);
        }
        if (e[7] >= 0) {
            wr16(t + 0x12, e[7]);
        }
        if (e[8] >= 0) {
            wr16(t + 0x16, e[8]);
        }
    }
#else
    (void)id;
    (void)buffer;
#endif
}

/* sound_update_loading, step 3, after the body is in SPU RAM: upload the guitar sample into ADV_GUITAR_VAG (vag 14). */
void dw3_adventure_sound_body(s32 id, s32 *buffer, s32 spu_addr) {
#ifdef ADV_HAVE_SCORE
    u8 *head;
    const u8 *sizes;
    u32 addr = (u32)spu_addr;
    int i;

    if (id != DW3_ADVENTURE_BANK || (head = vab_head(buffer)) == NULL) {
        return;
    }
    sizes = head + 0x20 + 0x800 + 0x200 * rd16(head + 0x12);
    for (i = 1; i < ADV_GUITAR_VAG; i++) {
        addr += (u32)rd16(sizes + 2 * i) * 8;
    }
    if (sizeof(adv_guitar) > (u32)rd16(sizes + 2 * ADV_GUITAR_VAG) * 8) {
        port_log("adventure music: the guitar sample (%u bytes) does not fit its slot", (unsigned)sizeof(adv_guitar));
        return;
    }
    spu_write16(0x1A6, (uint16_t)(addr >> 3)); /* transfer address, in units of 8 bytes */
    spu_dma_write((const uint16_t *)adv_guitar, sizeof(adv_guitar) / 2);
#else
    (void)id;
    (void)buffer;
    (void)spu_addr;
#endif
}

static void dw3_adventure_reverb_off(void) {
#ifdef ADV_HAVE_SCORE
    if (adv_reverb_live) {
        adv_reverb_live = 0;
        SsUtSetReverbDepth(0, 0);
    }
#endif
}

void dw3_adventure_sound_music(s32 id) {
#ifdef ADV_HAVE_SCORE
    if (id == DW3_ADVENTURE_BANK) {
        adv_reverb_live = ADV_REVERB_DEPTH > 0;
    } else {
        dw3_adventure_reverb_off();
    }
#else
    (void)id;
#endif
}

void dw3_adventure_sound_stopped(s32 id) {
    if (id == DW3_ADVENTURE_BANK) {
        dw3_adventure_reverb_off();
    }
}

void dw3_adventure_sound_silenced(void) {
    dw3_adventure_reverb_off();
}
