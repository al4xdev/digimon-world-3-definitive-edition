/* The Velvet Room's music (tools/gen_velvet_music.py): the owner's own arrangement,
 * played by the game's LIBSND from a SEP the generator writes from his local MIDI, on the samples of BGM031 (sound bank
 * 33) with the piano of BOSS01 in the place of BGM031's noise-burst one.
 *
 * The score is in definitive_velvet_music.inc, which is generated and not committed. Without it (no MIDI or no disc when
 * it was generated) nothing here acts, dw3_velvet_music leaves the room's old music in place, and the game is the one
 * without this file.
 *
 * A bank id of its own (DW3_VELVET_BANK, past the disc's 71): BGM031 is also the music of other screens, which must
 * keep their own score, and a bank already loaded under its id is not loaded again. The hooks are in src/main/sound.c
 * (PC_PORT): the bank record, the loaded header (the SEP and the tones are edited in the copy of the entry's buffer,
 * never on the disc), the SPU RAM once the body is in it (the piano), and the music starting and stopping (the reverb the
 * score raises is the room's alone). */
#include "common.h"

#include "psyq/libsnd.h"
#include "sound.h"

#include "psxstack/spu.h"

void port_log(const char *fmt, ...); /* psxstack/port_runtime.h's */

#define DW3_VELVET_BANK 72 /* sound_banks has 72 slots (0 is none); the room's is the next */
#define BGM031 33          /* the disc's bank this one copies (SOUND/BGM031) */
#define BUFFER_SIZE 0xA000 /* entries 1 and 2: sound_buffer_1/2 */

extern SoundBank *sound_banks[];

#if defined(__has_include)
#if __has_include("definitive_velvet_music.inc")
#include "definitive_velvet_music.inc"
#define VL_HAVE_SCORE 1
#endif
#endif

#ifdef VL_HAVE_SCORE

static SoundBank vl_bank;
static int vl_reverb_live; /* the room's score raised the reverb depth: the game's own is 0 (sound_init) */

static u16 rd16(const u8 *p) {
    return (u16)(p[0] | p[1] << 8);
}

static void wr16(u8 *p, int v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
}

/* The VAB header of the buffer and the SEP's offset (the file's sub-file table: word 0 and 1). */
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

/* The room's stage: its music, once the score is there. */
void dw3_velvet_music(s32 *music, s32 *sound) {
#ifdef VL_HAVE_SCORE
    *music = DW3_VELVET_BANK;
    *sound = 0x60000000 | DW3_VELVET_BANK << 18; /* bit 30: the current music; sequence 0 of SEP 0 */
#else
    (void)music;
    (void)sound;
#endif
}

/* sound_load_bank: the record to load for `id`. */
SoundBank *dw3_velvet_sound_bank(s32 id) {
#ifdef VL_HAVE_SCORE
    if (id == DW3_VELVET_BANK) {
        vl_bank = *sound_banks[BGM031];
        return &vl_bank;
    }
#endif
    return sound_banks[id];
}

/* sound_update_loading, step 1, before the VAB header is opened: the room's bank gets its score and its tones. The SEP
 * goes where BGM031's own is (the buffer has room for it: BGM031's file is 22 KB of the 40 KB). */
void dw3_velvet_sound_header(s32 id, s32 *buffer) {
#ifdef VL_HAVE_SCORE
    u8 *head;
    u8 *sep;
    unsigned i;

    if (id != DW3_VELVET_BANK) {
        return;
    }
    head = vab_head(buffer);
    sep = (u8 *)buffer + buffer[1];
    if (head == NULL || (u8 *)sep < head || (u8 *)sep + sizeof(vl_score) > (u8 *)buffer + BUFFER_SIZE) {
        port_log("velvet music: the bank's header does not hold the score (%u bytes), BGM031's own plays",
                 (unsigned)sizeof(vl_score));
        return;
    }
    for (i = 0; i < sizeof(vl_score); i++) {
        sep[i] = vl_score[i];
    }
    for (i = 0; i < sizeof(vl_tone_edits) / sizeof(vl_tone_edits[0]); i++) {
        const int *e = vl_tone_edits[i];
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
    }
#else
    (void)id;
    (void)buffer;
#endif
}

/* sound_update_loading, step 3, the body is in the SPU: the piano's samples over the slot of vag VL_PIANO_VAG (the
 * VAB's sizes table says where it is and how much room it has; the VAGs sit back to back from the entry's address). */
void dw3_velvet_sound_body(s32 id, s32 *buffer, s32 spu_addr) {
#ifdef VL_HAVE_SCORE
    u8 *head;
    const u8 *sizes;
    u32 addr = (u32)spu_addr;
    int i;

    if (id != DW3_VELVET_BANK || (head = vab_head(buffer)) == NULL) {
        return;
    }
    sizes = head + 0x20 + 0x800 + 0x200 * rd16(head + 0x12);
    for (i = 1; i < VL_PIANO_VAG; i++) {
        addr += (u32)rd16(sizes + 2 * i) * 8;
    }
    if (sizeof(vl_piano) > (u32)rd16(sizes + 2 * VL_PIANO_VAG) * 8) {
        port_log("velvet music: the piano's sample (%u bytes) does not fit its slot", (unsigned)sizeof(vl_piano));
        return;
    }
    spu_write16(0x1A6, (uint16_t)(addr >> 3)); /* the transfer address, in units of 8 bytes */
    spu_dma_write((const uint16_t *)vl_piano, sizeof(vl_piano) / 2);
#else
    (void)id;
    (void)buffer;
    (void)spu_addr;
#endif
}

/* The reverb depth is global to LIBSND, and the game's own music sets it by its own sequences (or leaves the 0 sound_init
 * set): back to 0 when the room's music is replaced or stopped, or its effects would carry on in every sound after it. */
static void dw3_velvet_reverb_off(void) {
#ifdef VL_HAVE_SCORE
    if (vl_reverb_live) {
        vl_reverb_live = 0;
        SsUtSetReverbDepth(0, 0);
    }
#endif
}

/* sound_play with the "current" bit: bank `id`'s music starts. */
void dw3_velvet_sound_music(s32 id) {
#ifdef VL_HAVE_SCORE
    if (id == DW3_VELVET_BANK) {
        vl_reverb_live = VL_REVERB_DEPTH > 0;
    } else {
        dw3_velvet_reverb_off();
    }
#else
    (void)id;
#endif
}

/* sound_stop of bank `id`'s sound. */
void dw3_velvet_sound_stopped(s32 id) {
    if (id == DW3_VELVET_BANK) {
        dw3_velvet_reverb_off();
    }
}

/* sound_stop_all. */
void dw3_velvet_sound_silenced(void) {
    dw3_velvet_reverb_off();
}
