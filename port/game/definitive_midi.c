/* The players' own battle music (Etemon's jukebox, definitive_midi_screen.c): standard MIDI files in a folder, played by
 * the game's own LIBSND like the Velvet Room's and the Adventure scores (definitive_velvet_music.c), but converted at run
 * time, in C, instead of at configure time by a generator.
 *
 * The pipeline. A .mid is read whole and turned into a SEP, the sequence format LIBSND plays (docs/FORMATS.md "SEP"):
 * the notes, the volume and pan controls, the tempo map and the program changes, merged from every track by tick, with
 * the file's channels kept. The file's General MIDI programs are mapped onto the instruments of BGM031 (sound bank 33,
 * the bank the other two scores borrow: the disc has no General MIDI set); channel 10 is mapped note by note onto the
 * four drum tones of BGM031's program 0. The SEP goes where BGM031's own SEP is in the header buffer of an extra entry
 * (sound.c: bank id DW3_MIDI_BANK 74, a copy of BGM031's record), so the 32 KB the buffer has past the VAB header is all
 * a piece may use: a longer one is cut where it fits (every sounding note is closed there, then the piece loops). The
 * voices are the SPU's 24, shared with the sound effects.
 *
 * Where the files live: a `midi` folder beside the settings file (the memory cards' folder: the launcher's per-user
 * directory on a desktop, the package's on PortMaster, the app's own files folder on Android), or DW3_MIDI_DIR. The
 * choice is in `battle_music.txt` in that folder (`mode:` own / file / shuffle / order, the file's name or the last one played). It belongs to
 * the install and not to the save: the files are the player's, on this device, and a save moved to another one must not
 * play a piece that is not there. A file that is gone is the game's music again, with no message.
 *
 * Which battles: the random encounters (fieldstg_encounter_start; the Rare Targets and the Rush's pick are the same
 * encounter), not the scripted ones, the bosses and the Super Rares: those keep the stage's own music. */
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "common.h"

#include "psyq/libsnd.h"
#include "sound.h"

#include "definitive.h"

#include "psxstack/platform.h"
#include "psxstack/settings.h"

void port_log(const char *fmt, ...); /* psxstack/port_runtime.h's */
extern int def_on(void);
void dw3_midi_sound_forget(void); /* sound.c: the sound entries that hold the bank let it go */

#define BUFFER_SIZE 0xA000   /* the extra entries' header buffers (sound_buffer_1/2) */
#define SEP_MAX 31000        /* a bit under what the buffer has past BGM031's VAB header (40960 - 8744) */
#define MIDI_FILE_MAX (3 << 20)
#define MIDI_EVENTS_MAX 400000
#define BGM031 33
#define DW3_MIDI_BANK 74 /* after the Velvet Room's 72 and the Adventure score's 73 */
#define OBOE_PROGRAM 5       /* BGM031's reed: its tone is centred two octaves off for the notes a score plays */
#define OBOE_CENTER 62
/* The "middle setting" (owner, 2026-11-23): the bank's own tones attack in 0.2-2 ms (a click) and release in 6-46 ms (the note
 * is cut at its note-off), which no file was written for. Every melodic tone of the piece gets the same linear attack
 * (~46 ms) and release (~186 ms), the SPU's nearest shift/step to what the Velvet Room's score picks per instrument
 * (tools/gen_velvet_music.py), and the reverb flag, with LIBSND's reverb (type 3, depth 64) raised by the piece and put
 * back when it stops. Only bank 74's copy of the header is edited: the Velvet Room's and the Adventure's banks are theirs. */
#define ATTACK_SHIFT 9
#define ATTACK_STEP 3
#define RELEASE_SHIFT 12
#define REVERB_TYPE 3
#define REVERB_DEPTH 64

extern SoundBank *sound_banks[];

/* ---- The converter: a MIDI file's bytes to a SEP ---- */

enum { EV_CTRL, EV_PROG, EV_OFF, EV_ON, EV_TEMPO }; /* in the order they sort at one tick: controls before notes,
                                                     * an off before an on (a repeated note is not cut by its own off) */

typedef struct {
    u32 tick;
    u32 order;
    u32 value; /* the tempo, in microseconds per quarter */
    u8 kind, ch, d1, d2;
} MidiEv;

static u32 be32(const u8 *p) {
    return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3];
}

/* A variable-length quantity at *o, bounded by `end`; -1 when it runs out. */
static long vlq_read(const u8 *d, size_t end, size_t *o) {
    long v = 0;
    int i;

    for (i = 0; i < 4; i++) {
        if (*o >= end) {
            return -1;
        }
        v = v << 7 | (d[*o] & 0x7F);
        if (!(d[(*o)++] & 0x80)) {
            return v;
        }
    }
    return -1;
}

static size_t vlq_write(u8 *out, u32 n) {
    u8 tmp[5];
    size_t k = 0, i;

    tmp[k++] = n & 0x7F;
    while ((n >>= 7) != 0) {
        tmp[k++] = (u8)((n & 0x7F) | 0x80);
    }
    for (i = 0; i < k; i++) {
        out[i] = tmp[k - 1 - i];
    }
    return k;
}

/* General MIDI program to the program of BGM031 that sounds nearest (the samples: 1 a plucked horn-like tone, 2 and 3 the
 * low strings, 4 and 6 the violin and viola, 5 the reed, 8 the flute; 0 is the four drum tones). */
static int bank_program(int gm) {
    static const u8 strings[8] = { 4, 6, 2, 3, 4, 1, 1, 3 }; /* violin, viola, cello, contrabass, tremolo, pizzicato, harp, timpani */

    switch (gm >> 3) { /* the General MIDI families */
    case 0: case 1: case 3: case 13: case 14:
        return 1; /* pianos, tuned percussion, guitars, ethnic, percussive */
    case 2: case 8:
        return 5; /* organs, reeds */
    case 4:
        return 3; /* basses */
    case 5:
        return strings[gm & 7];
    case 7:
        return 2; /* brass */
    case 9: case 10:
        return 8; /* pipes, synth leads */
    default:
        return 6; /* ensembles, pads, effects */
    }
}

/* A General MIDI drum note to one of BGM031's program 0 tones (36 kick, 38 snare, 44 hat, 49 cymbal); -1 drops it. */
static int drum_note(int n) {
    switch (n) {
    case 35: case 36: case 41: case 43: case 45:
        return 36;
    case 37: case 38: case 39: case 40:
        return 38;
    case 42: case 44: case 46:
        return 44;
    case 49: case 51: case 52: case 53: case 55: case 57: case 59:
        return 49;
    }
    return -1;
}

static int ev_cmp(const void *a, const void *b) {
    const MidiEv *x = a, *y = b;

    if (x->tick != y->tick) {
        return x->tick < y->tick ? -1 : 1;
    }
    if (x->kind != y->kind) {
        /* tempo, controls and programs (before any note) first, then the offs, then the ons */
        int kx = x->kind == EV_TEMPO ? -1 : x->kind, ky = y->kind == EV_TEMPO ? -1 : y->kind;

        return kx < ky ? -1 : 1;
    }
    return x->order < y->order ? -1 : x->order > y->order;
}

/* Reads `d` (a format 0 or 1 file, metric time) into the events of every track. 0 on a file this cannot play. */
static int midi_parse(const u8 *d, size_t n, MidiEv **out, size_t *count, u32 *division) {
    size_t o, cap = 4096, k = 0;
    int ntrk, t;
    u32 hlen, order = 0;
    MidiEv *ev;

    if (n < 14 || memcmp(d, "MThd", 4) != 0 || (hlen = be32(d + 4)) < 6 || hlen > n - 8) {
        return 0;
    }
    ntrk = d[10] << 8 | d[11];
    *division = (u32)(d[12] << 8 | d[13]);
    if (*division == 0 || (*division & 0x8000) || ntrk < 1) {
        return 0; /* SMPTE time */
    }
    ev = malloc(cap * sizeof(*ev));
    if (ev == NULL) {
        return 0;
    }
    o = 8 + hlen;
    for (t = 0; t < ntrk && o + 8 <= n; t++) {
        size_t end;
        u32 tick = 0;
        int status = 0;

        if (memcmp(d + o, "MTrk", 4) != 0) {
            break;
        }
        end = o + 8 + be32(d + o + 4);
        if (end > n) {
            end = n;
        }
        o += 8;
        while (o < end) {
            long dt = vlq_read(d, end, &o);
            int b, kind;
            MidiEv e;

            if (dt < 0 || o >= end) {
                break;
            }
            tick += (u32)dt;
            b = d[o];
            if (b == 0xFF) { /* meta */
                long len;
                int type;

                if (o + 2 > end) {
                    break;
                }
                type = d[o + 1];
                o += 2;
                len = vlq_read(d, end, &o);
                if (len < 0 || o + (size_t)len > end) {
                    break;
                }
                if (type == 0x2F) {
                    o = end;
                    break;
                }
                if (type == 0x51 && len == 3) {
                    e.tick = tick;
                    e.kind = EV_TEMPO;
                    e.ch = e.d1 = e.d2 = 0;
                    e.value = (u32)d[o] << 16 | d[o + 1] << 8 | d[o + 2];
                    e.order = order++;
                    if (e.value != 0) {
                        goto push;
                    }
                }
                o += (size_t)len;
                status = 0;
                continue;
            }
            if (b == 0xF0 || b == 0xF7) { /* sysex */
                long len;

                o++;
                len = vlq_read(d, end, &o);
                if (len < 0 || o + (size_t)len > end) {
                    break;
                }
                o += (size_t)len;
                status = 0;
                continue;
            }
            if (b & 0x80) {
                status = b;
                o++;
            } else if (status == 0) {
                break; /* data with no status: a damaged track */
            }
            kind = status & 0xF0;
            e.tick = tick;
            e.ch = status & 0xF;
            e.value = 0;
            e.order = order++;
            e.d1 = e.d2 = 0;
            if (kind == 0xC0 || kind == 0xD0) {
                if (o >= end) {
                    break;
                }
                e.d1 = d[o++] & 0x7F;
                if (kind == 0xC0) {
                    e.kind = EV_PROG;
                    goto push;
                }
                continue;
            }
            if (kind < 0x80 || kind > 0xE0) {
                break;
            }
            if (o + 2 > end) {
                break;
            }
            e.d1 = d[o++] & 0x7F;
            e.d2 = d[o++] & 0x7F;
            if (kind == 0x90 && e.d2 != 0) {
                e.kind = EV_ON;
            } else if (kind == 0x90 || kind == 0x80) {
                e.kind = EV_OFF;
            } else if (kind == 0xB0 && (e.d1 == 7 || e.d1 == 10)) { /* channel volume, pan: all the mix a SEP keeps */
                e.kind = EV_CTRL;
            } else {
                continue; /* aftertouch, pitch bend, the other controllers: the SEP's budget is for the notes */
            }
        push:
            if (k == cap) {
                MidiEv *bigger;

                if (cap >= MIDI_EVENTS_MAX || (bigger = realloc(ev, cap * 2 * sizeof(*ev))) == NULL) {
                    free(ev);
                    return 0;
                }
                ev = bigger;
                cap *= 2;
            }
            ev[k++] = e;
        }
        o = end;
    }
    if (k == 0) {
        free(ev);
        return 0;
    }
    qsort(ev, k, sizeof(*ev), ev_cmp);
    *out = ev;
    *count = k;
    return 1;
}

typedef struct {
    u8 *buf;
    size_t cap, len;
    int prev_status; /* the last status byte written (0 after a meta event: LIBSND then reads the next byte as a meta type) */
    u32 prev_tick;
    int overflow;
} SepOut;

static void sep_put(SepOut *s, u32 tick, const u8 *raw, size_t n, int meta) {
    u8 tmp[8];
    size_t dl = vlq_write(tmp, tick - s->prev_tick), skip = 0;

    if (!meta && raw[0] == s->prev_status && ((raw[0] & 0xF0) == 0x90 || (raw[0] & 0xF0) == 0xB0 || (raw[0] & 0xF0) == 0xC0)) {
        skip = 1; /* running status */
    }
    if (s->len + dl + n - skip > s->cap) {
        s->overflow = 1;
        return;
    }
    memcpy(s->buf + s->len, tmp, dl);
    s->len += dl;
    memcpy(s->buf + s->len, raw + skip, n - skip);
    s->len += n - skip;
    s->prev_tick = tick;
    s->prev_status = meta ? 0 : raw[0];
}

static void put_tempo(SepOut *s, u32 tick, u32 us) {
    u8 raw[5] = { 0xFF, 0x51, (u8)(us >> 16), (u8)(us >> 8), (u8)us };

    sep_put(s, tick, raw, 5, 1);
}

/* The sequence-0 body for events up to `cut` (ticks); *len its size, 0 when it does not fit `cap`. The loop is the disc's
 * own: NRPN 20 with a count of 127 before the music and NRPN 30 after it (tools/gen_velvet_music.py's). */
static size_t encode_body(const MidiEv *ev, size_t n, u32 cut, u32 division, u8 *body, size_t cap, int *notes) {
    SepOut s = { body, cap, 0, 0, 0, 0 };
    int cur[16];
    u8 on[16][128];
    u32 last = 0;
    size_t i;
    int ch;
    u8 raw[3];
    u32 first_tempo = 500000;
    int max_vel = 0, max_cc7 = 0;
    int ch_has_cc7[16];
    int ch_has_note[16];

    memset(on, 0, sizeof(on));
    memset(ch_has_cc7, 0, sizeof(ch_has_cc7));
    memset(ch_has_note, 0, sizeof(ch_has_note));
    for (ch = 0; ch < 16; ch++) {
        cur[ch] = -1;
    }
    *notes = 0;
    for (i = 0; i < n; i++) {
        if (ev[i].kind == EV_TEMPO) {
            first_tempo = ev[i].value;
            break;
        }
    }
    for (i = 0; i < n && ev[i].tick <= cut; i++) {
        const MidiEv *e = &ev[i];

        if (e->kind == EV_ON) {
            int note = e->ch == 9 ? drum_note(e->d1) : e->d1 <= 120 ? e->d1 : -1;

            if (note >= 0) {
                ch_has_note[e->ch] = 1;
                if (e->d2 > max_vel) {
                    max_vel = e->d2;
                }
            }
        } else if (e->kind == EV_CTRL && e->d1 == 7 && e->ch != 9) {
            ch_has_cc7[e->ch] = 1;
            if (e->d2 > max_cc7) {
                max_cc7 = e->d2;
            }
        }
    }
    for (ch = 0; ch < 16; ch++) {
        if (ch != 9 && ch_has_note[ch] && !ch_has_cc7[ch] && max_cc7 < 100) {
            max_cc7 = 100;
        }
    }
    if (max_vel <= 0) {
        max_vel = 127;
    }
    if (max_cc7 <= 0) {
        max_cc7 = 127;
    }
    /* the reverb: NRPN 15 = type, 16 = depth (before the loop, once) */
    {
        static const u8 nrpn[2][2] = { { 15, REVERB_TYPE }, { 16, REVERB_DEPTH } };
        int k;

        for (k = 0; k < 2; k++) {
            raw[0] = 0xB0;
            raw[1] = 99;
            raw[2] = 16;
            sep_put(&s, 0, raw, 3, 0);
            raw[1] = 98;
            raw[2] = nrpn[k][0];
            sep_put(&s, 0, raw, 3, 0);
            raw[1] = 6;
            raw[2] = nrpn[k][1];
            sep_put(&s, 0, raw, 3, 0);
        }
    }
    /* before the loop: every channel's level and pan as a sane default, the drums quieter */
    for (ch = 0; ch < 16; ch++) {
        int def_vol = ch == 9 ? 70 : 100;

        if (max_cc7 < 127) {
            def_vol = (def_vol * 127 + max_cc7 / 2) / max_cc7;
            if (def_vol > 127) {
                def_vol = 127;
            }
        }
        raw[0] = (u8)(0xB0 | ch);
        raw[1] = 7;
        raw[2] = (u8)def_vol;
        sep_put(&s, 0, raw, 3, 0);
        raw[1] = 10;
        raw[2] = 64;
        sep_put(&s, 0, raw, 3, 0);
    }
    raw[0] = 0xC9;
    raw[1] = 0;
    sep_put(&s, 0, raw, 2, 0);
    cur[9] = 0;
    raw[0] = 0xB0;
    raw[1] = 99;
    raw[2] = 20;
    sep_put(&s, 0, raw, 3, 0);
    raw[1] = 6;
    raw[2] = 127;
    sep_put(&s, 1, raw, 3, 0); /* the piece starts one tick in, as the generators' */
    put_tempo(&s, 1, first_tempo);
    for (i = 0; i < n && !s.overflow; i++) {
        const MidiEv *e = &ev[i];
        u32 t = e->tick + 1;
        int note;

        if (e->tick > cut) {
            break;
        }
        switch (e->kind) {
        case EV_TEMPO:
            if (e->value != first_tempo || e->tick > 0) {
                put_tempo(&s, t, e->value);
                first_tempo = e->value;
            }
            break;
        case EV_PROG:
            if (e->ch != 9 && cur[e->ch] != bank_program(e->d1)) {
                cur[e->ch] = bank_program(e->d1);
                raw[0] = (u8)(0xC0 | e->ch);
                raw[1] = (u8)cur[e->ch];
                sep_put(&s, t, raw, 2, 0);
            }
            break;
        case EV_CTRL:
            raw[0] = (u8)(0xB0 | e->ch);
            raw[1] = e->d1;
            if (e->d1 == 7) {
                int v = e->ch == 9 ? (e->d2 * 70 / 100) : e->d2;

                if (max_cc7 < 127) {
                    v = (v * 127 + max_cc7 / 2) / max_cc7;
                    if (v > 127) {
                        v = 127;
                    }
                }
                raw[2] = (u8)v;
            } else {
                raw[2] = e->d2;
            }
            sep_put(&s, t, raw, 3, 0);
            break;
        case EV_ON:
        case EV_OFF:
            note = e->ch == 9 ? drum_note(e->d1) : e->d1 <= 120 ? e->d1 : -1;
            if (note < 0) {
                break;
            }
            if (e->kind == EV_OFF && !on[e->ch][note]) {
                break; /* an off for a note this score never started (or dropped) */
            }
            if (cur[e->ch] < 0) { /* a channel that plays before any program change: the piano's (GM 0) */
                cur[e->ch] = bank_program(0);
                raw[0] = (u8)(0xC0 | e->ch);
                raw[1] = (u8)cur[e->ch];
                sep_put(&s, t, raw, 2, 0);
            }
            raw[0] = (u8)(0x90 | e->ch);
            raw[1] = (u8)note;
            if (e->kind == EV_ON) {
                int v = e->d2;

                if (max_vel < 127) {
                    v = (v * 127 + max_vel / 2) / max_vel;
                    if (v > 127) {
                        v = 127;
                    }
                }
                raw[2] = (u8)(v > 0 ? v : 1);
            } else {
                raw[2] = 0;
            }
            sep_put(&s, t, raw, 3, 0);
            on[e->ch][note] = e->kind == EV_ON;
            if (e->kind == EV_ON) {
                (*notes)++;
            }
            break;
        }
        last = t;
    }
    for (ch = 0; ch < 16; ch++) { /* a piece cut short closes what still sounds */
        for (i = 0; i < 128; i++) {
            if (on[ch][i]) {
                raw[0] = (u8)(0x90 | ch);
                raw[1] = (u8)i;
                raw[2] = 0;
                sep_put(&s, last, raw, 3, 0);
            }
        }
    }
    raw[0] = 0xB0;
    raw[1] = 99;
    raw[2] = 30;
    sep_put(&s, last + 2 * division, raw, 3, 0); /* the release's rest before the loop turns */
    {
        static const u8 end[3] = { 0xFF, 0x2F, 0x00 };

        sep_put(&s, last + 2 * division, end, 3, 1);
    }
    return s.overflow ? 0 : s.len;
}

/* One sequence's header: number, resolution, tempo, the time signature the disc's SEPs carry, the body's size. */
static size_t seq_header(u8 *out, int number, u32 division, u32 tempo, size_t body_len) {
    out[0] = 0;
    out[1] = (u8)number;
    out[2] = (u8)(division >> 8);
    out[3] = (u8)division;
    out[4] = (u8)(tempo >> 16);
    out[5] = (u8)(tempo >> 8);
    out[6] = (u8)tempo;
    out[7] = 4;
    out[8] = 2;
    out[9] = (u8)(body_len >> 24);
    out[10] = (u8)(body_len >> 16);
    out[11] = (u8)(body_len >> 8);
    out[12] = (u8)body_len;
    return 13;
}

/* The SEP for the MIDI file `d` into `out` (`cap` bytes): its length, 0 when the file cannot be played. *cut says that it
 * was too long and was cut where it fits; *notes how many notes it has. */
size_t def_midi_to_sep(const u8 *d, size_t n, u8 *out, size_t cap, int *cut, int *notes) {
    MidiEv *ev;
    size_t count, body_len = 0, k;
    u32 division, tempo = 500000, last, limit;
    u8 *body;
    int i;

    *cut = 0;
    *notes = 0;
    if (!midi_parse(d, n, &ev, &count, &division)) {
        return 0;
    }
    for (k = 0; k < count; k++) {
        if (ev[k].kind == EV_TEMPO) {
            tempo = ev[k].value;
            break;
        }
    }
    last = ev[count - 1].tick;
    body = malloc(cap);
    if (body == NULL || cap < 512) {
        free(body);
        free(ev);
        return 0;
    }
    /* sequence 0 is the piece, 1..15 an empty end-of-track each, as LIBSND is opened with 16: the piece gets the rest */
    for (limit = last, i = 0; i < 60; i++) {
        body_len = encode_body(ev, count, limit, division, body, cap - 6 - 13 - 15 * 17, notes);
        if (body_len != 0) {
            break;
        }
        *cut = 1;
        limit -= limit / 12 + 1;
    }
    free(ev);
    if (body_len == 0 || *notes == 0) {
        free(body);
        return 0;
    }
    memcpy(out, "pQES\0\0", 6);
    k = 6 + seq_header(out + 6, 0, division, tempo, body_len);
    memcpy(out + k, body, body_len);
    k += body_len;
    for (i = 1; i < 16; i++) {
        static const u8 empty[4] = { 0x00, 0xFF, 0x2F, 0x00 };

        k += seq_header(out + k, i, division, 500000, 4);
        memcpy(out + k, empty, 4);
        k += 4;
    }
    free(body);
    return k;
}

/* ---- The folder, the files, the choice ---- */

#define MIDI_NAME 64
#define MIDI_LIST_MAX 64
#define CHOICE_FILE "battle_music.txt"

static char midi_names[MIDI_LIST_MAX][MIDI_NAME];
static int midi_cut[MIDI_LIST_MAX];
static int midi_count;
static char choice[MIDI_NAME];
static char last[MIDI_NAME]; /* the playlists: the file played last */
#define MIDI_VOL_MAX 64
#define MIDI_VOL_MIN 10
#define MIDI_VOL_MAX_PCT 400
#define MIDI_VOL_DEFAULT 50
static struct {
    char name[MIDI_NAME];
    u16 vol;
} midi_vols[MIDI_VOL_MAX];
static int midi_vol_count;
static int active_vol = MIDI_VOL_DEFAULT;
extern void dw3_midi_sound_set_vol(s32 vol);

static s32 midi_vol_to_sep(int pct) {
    u32 target, r = 0, bit = 1u << 14;

    if (pct < MIDI_VOL_MIN) {
        pct = MIDI_VOL_MIN;
    } else if (pct > MIDI_VOL_MAX_PCT) {
        pct = MIDI_VOL_MAX_PCT;
    }
    target = (u32)pct * 127u * 127u / (u32)MIDI_VOL_MAX_PCT;
    while (bit > target) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (target >= r + bit) {
            target -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r > 127 ? 127 : (s32)r;
}

static int vol_of_name(const char *name) {
    int i;

    if (name == NULL || name[0] == 0) {
        return MIDI_VOL_DEFAULT;
    }
    for (i = 0; i < midi_vol_count; i++) {
        if (strcmp(midi_vols[i].name, name) == 0) {
            return (int)midi_vols[i].vol;
        }
    }
    return MIDI_VOL_DEFAULT;
}

static void vol_set_name(const char *name, int pct) {
    int i;

    if (name == NULL || name[0] == 0) {
        return;
    }
    if (pct < MIDI_VOL_MIN) {
        pct = MIDI_VOL_MIN;
    } else if (pct > MIDI_VOL_MAX_PCT) {
        pct = MIDI_VOL_MAX_PCT;
    }
    for (i = 0; i < midi_vol_count; i++) {
        if (strcmp(midi_vols[i].name, name) == 0) {
            midi_vols[i].vol = (u16)pct;
            return;
        }
    }
    if (midi_vol_count < MIDI_VOL_MAX) {
        snprintf(midi_vols[midi_vol_count].name, MIDI_NAME, "%.*s", MIDI_NAME - 1, name);
        midi_vols[midi_vol_count].vol = (u16)pct;
        midi_vol_count++;
    }
}
static int choice_mode;
enum { DEF_MIDI_OWN, DEF_MIDI_FILE, DEF_MIDI_SHUFFLE, DEF_MIDI_ORDER };
static int choice_read;
static u8 midi_sep[SEP_MAX + 64];
static size_t midi_sep_len;
static u32 midi_sep_hash;    /* of the SEP the next load of the bank puts in */
static u32 midi_loaded_hash; /* of the SEP the bank in the sound entries holds (0: none loaded) */

void def_midi_dir(char *out, unsigned long size) {
    const char *env = getenv("DW3_MIDI_DIR");

    if (env != NULL && env[0] != 0) {
        snprintf(out, size, "%s", env);
    } else if (port_settings_loaded && port_settings.dir != NULL) {
        snprintf(out, size, "%s/midi", port_settings.dir);
    } else {
        snprintf(out, size, "midi");
    }
}

/* A case-insensitive compare of ASCII names (strcasecmp is not C99). */
static int ci_cmp(const char *a, const char *b) {
    while (*a != 0 && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static int has_midi_extension(const char *name) {
    const char *dot = strrchr(name, '.');

    if (dot == NULL) {
        return 0;
    }
    return ci_cmp(dot, ".mid") == 0 || ci_cmp(dot, ".midi") == 0;
}

/* The whole of a file into a malloc'd buffer; NULL when it is not there, empty or too big. */
static u8 *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    long n;
    u8 *buf;

    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) <= 0 || n > MIDI_FILE_MAX || fseek(f, 0, SEEK_SET) != 0 ||
        (buf = malloc((size_t)n)) == NULL) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return buf;
}

/* A bounded copy, one that the format checker can see is bounded: the SDL build compiles with -Werror and GCC cannot
 * tell how long the strings here are, so snprintf("%s") into a smaller buffer is an error there (a path is as long as
 * the player's folder plus the file's name). */
static void copy_trunc(char *dst, size_t size, const char *src) {
    size_t n = strnlen(src, size - 1);

    memcpy(dst, src, n);
    dst[n] = 0;
}

static void file_path(char *out, size_t size, const char *name) {
    char dir[512];
    size_t d, n;

    def_midi_dir(dir, sizeof(dir));
    d = strnlen(dir, sizeof(dir) - 1); /* bounded by its own buffer: -Werror's stringop-overread wants to see that */
    if (d > size - 1) {
        d = size - 1;
    }
    memcpy(out, dir, d);
    out[d] = 0;
    if (d + 2 <= size) {
        out[d] = '/';
        n = strnlen(name, size - d - 2);
        memcpy(out + d + 1, name, n);
        out[d + 1 + n] = 0;
    }
}

static int name_cmp(const void *a, const void *b) {
    int r = ci_cmp((const char *)a, (const char *)b);

    return r != 0 ? r : strcmp((const char *)a, (const char *)b);
}

static u32 fnv(const u8 *p, size_t n) {
    u32 h = 2166136261u;

    while (n--) {
        h = (h ^ *p++) * 16777619u;
    }
    return h != 0 ? h : 1;
}

/* The folder's .mid / .midi names, sorted without regard to case, into `names`; the count (0 with no folder). With `make`
 * the folder is made when it is not there, so a player can find it. */
static int dir_names(char names[][MIDI_NAME], int make) {
    char dir[512];
    DIR *d;
    struct dirent *de;
    int found = 0;

    def_midi_dir(dir, sizeof(dir));
    if (make) {
        port_make_dirs(dir);
    }
    if ((d = opendir(dir)) == NULL) {
        return 0;
    }
    while ((de = readdir(d)) != NULL && found < MIDI_LIST_MAX) {
        if (has_midi_extension(de->d_name) && strlen(de->d_name) < MIDI_NAME) {
            memcpy(names[found], de->d_name, strlen(de->d_name) + 1);
            found++;
        }
    }
    closedir(d);
    qsort(names, (size_t)found, MIDI_NAME, name_cmp);
    return found;
}

/* The folder's playable files, sorted, into the list: only those that convert (a file of another kind named .mid, or one
 * with no notes, is not offered). Returns the count. */
int def_midi_scan(void) {
    char names[MIDI_LIST_MAX][MIDI_NAME];
    int found, i;
    u8 *sep = malloc(SEP_MAX + 64);

    midi_count = 0;
    found = dir_names(names, 1);
    for (i = 0; sep != NULL && i < found; i++) {
        char path[768];
        size_t size;
        u8 *data;
        int cut, notes;

        file_path(path, sizeof(path), names[i]);
        if ((data = read_file(path, &size)) == NULL) {
            continue;
        }
        if (def_midi_to_sep(data, size, sep, SEP_MAX, &cut, &notes) != 0) {
            copy_trunc(midi_names[midi_count], MIDI_NAME, names[i]);
            midi_cut[midi_count++] = cut;
        }
        free(data);
    }
    free(sep);
    return midi_count;
}

const char *def_midi_name(int i) {
    return i >= 0 && i < midi_count ? midi_names[i] : "";
}

int def_midi_was_cut(int i) {
    return i >= 0 && i < midi_count && midi_cut[i];
}

/* battle_music.txt: `shuffle` / `order` / `file`, then the name of the file (a single pick) or, for the two playlists, the
 * last file played (`last:`: "in order" goes on after it, so the position survives a restart, a folder that changed and a
 * file that was deleted; "shuffled" does not repeat it). An older file with only a name is a single pick. */
static void choice_load(void) {
    char path[768], line[2 * MIDI_NAME + 16];
    FILE *f;

    if (choice_read) {
        return;
    }
    choice_read = 1;
    choice[0] = last[0] = 0;
    choice_mode = DEF_MIDI_OWN;
    file_path(path, sizeof(path), CHOICE_FILE);
    if ((f = fopen(path, "rb")) == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        size_t n = strlen(line);
        char *v;

        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ')) {
            line[--n] = 0;
        }
        if (n == 0) {
            continue;
        }
        if (strncmp(line, "mode:", 5) == 0) {
            v = line + 5;
            while (*v == ' ') {
                v++;
            }
            choice_mode = strcmp(v, "shuffle") == 0 ? DEF_MIDI_SHUFFLE : strcmp(v, "order") == 0 ? DEF_MIDI_ORDER
                          : strcmp(v, "file") == 0 ? DEF_MIDI_FILE : DEF_MIDI_OWN;
        } else if (strncmp(line, "file:", 5) == 0 || strncmp(line, "last:", 5) == 0) {
            char *dst = line[0] == 'f' ? choice : last;

            v = line + 5;
            while (*v == ' ') {
                v++;
            }
            if (strchr(v, '/') == NULL && strchr(v, '\\') == NULL) { /* a name, never a path out of the folder */
                snprintf(dst, MIDI_NAME, "%.*s", MIDI_NAME - 1, v);
            }
        } else if (strncmp(line, "vol:", 4) == 0) {
            char *eq = strrchr(line + 4, '=');
            if (eq != NULL) {
                char vname[MIDI_NAME];
                size_t len = (size_t)(eq - (line + 4));
                if (len > 0 && len < MIDI_NAME) {
                    memcpy(vname, line + 4, len);
                    vname[len] = 0;
                    vol_set_name(vname, atoi(eq + 1));
                }
            }
        } else if (choice_mode == DEF_MIDI_OWN && strchr(line, ':') == NULL && strchr(line, '/') == NULL) {
            snprintf(choice, MIDI_NAME, "%.*s", MIDI_NAME - 1, line); /* the first version's file: just the name */
            choice_mode = DEF_MIDI_FILE;
        }
    }
    fclose(f);
    if (choice_mode == DEF_MIDI_FILE && choice[0] == 0) {
        choice_mode = DEF_MIDI_OWN;
    }
}

static void choice_save(void) {
    char path[768];
    FILE *f;

    file_path(path, sizeof(path), CHOICE_FILE);
    if ((f = fopen(path, "wb")) == NULL) {
        port_log("midi: cannot write %s", path);
        return;
    }
    fprintf(f, "mode: %s\n", choice_mode == DEF_MIDI_SHUFFLE ? "shuffle" : choice_mode == DEF_MIDI_ORDER ? "order"
                              : choice_mode == DEF_MIDI_FILE ? "file" : "own");
    if (choice_mode == DEF_MIDI_FILE) {
        fprintf(f, "file: %s\n", choice);
    } else if (last[0] != 0) {
        fprintf(f, "last: %s\n", last);
    }
    {
        int i;
        for (i = 0; i < midi_vol_count; i++) {
            if (midi_vols[i].vol != MIDI_VOL_DEFAULT) {
                fprintf(f, "vol:%s=%d\n", midi_vols[i].name, (int)midi_vols[i].vol);
            }
        }
    }
    fclose(f);
}

/* The list's row of what is chosen: 0 the game's own music, 1 all shuffled, 2 all in order, 3.. the files (a chosen file the
 * list no longer has is the game's own music). */
int def_midi_chosen(void) {
    int i;

    choice_load();
    if (choice_mode == DEF_MIDI_SHUFFLE) {
        return 1;
    }
    if (choice_mode == DEF_MIDI_ORDER) {
        return 2;
    }
    for (i = 0; choice_mode == DEF_MIDI_FILE && i < midi_count; i++) {
        if (strcmp(choice, midi_names[i]) == 0) {
            return 3 + i;
        }
    }
    return 0;
}

/* Picks a row of the list (see def_midi_chosen) and writes the choice down. */
void def_midi_choose(int row) {
    choice_read = 1;
    choice[0] = last[0] = 0;
    choice_mode = row == 1 ? DEF_MIDI_SHUFFLE : row == 2 ? DEF_MIDI_ORDER : DEF_MIDI_OWN;
    if (row >= 3 && row - 3 < midi_count) {
        choice_mode = DEF_MIDI_FILE;
        snprintf(choice, sizeof(choice), "%s", midi_names[row - 3]);
    }
    choice_save();
    port_log("midi: battle music is now %s", choice_mode == DEF_MIDI_FILE ? choice : choice_mode == DEF_MIDI_SHUFFLE ? "all, shuffled"
                                            : choice_mode == DEF_MIDI_ORDER ? "all, in order" : "the game's own");
}

/* The shuffle's generator (xorshift32): DW3_MIDI_SEED fixes it (the tests), else it starts from the clock. */
static u32 rng_next(void) {
    static u32 state;

    if (state == 0) {
        const char *e = getenv("DW3_MIDI_SEED");

        state = e != NULL ? (u32)strtoul(e, NULL, 0) : (u32)time(NULL) * 2654435761u;
        state = state != 0 ? state : 1;
    }
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

/* ---- The battles and the sound bank ---- */

/* The chosen file `name` converted into midi_sep; 1 when it plays (a file that is gone or does not convert is not an error:
 * the caller tries the next). */
static int load_piece(const char *name) {
    char path[768];
    u8 *data;
    size_t size, len;
    int cut, notes;

    file_path(path, sizeof(path), name);
    if ((data = read_file(path, &size)) == NULL) {
        return 0;
    }
    len = def_midi_to_sep(data, size, midi_sep, SEP_MAX, &cut, &notes);
    free(data);
    if (len == 0) {
        return 0;
    }
    midi_sep_len = len;
    midi_sep_hash = fnv(midi_sep, len);
    active_vol = vol_of_name(name);
    if (midi_loaded_hash != 0 && midi_loaded_hash != midi_sep_hash) {
        dw3_midi_sound_forget(); /* another piece than the one the sound entries hold: load the bank again */
        midi_loaded_hash = 0;
    }
    port_log("midi: %s, %d notes, vol %d%%, SEP %u bytes%s", name, notes, active_vol, (unsigned)len,
             cut ? " (cut to fit)" : "");
    return 1;
}

int def_midi_volume(int index) {
    choice_load();
    if (index < 0 || index >= midi_count) {
        return MIDI_VOL_DEFAULT;
    }
    return vol_of_name(midi_names[index]);
}

void def_midi_set_volume(int index, int pct) {
    choice_load();
    if (index < 0 || index >= midi_count) {
        return;
    }
    if (pct < MIDI_VOL_MIN) {
        pct = MIDI_VOL_MIN;
    } else if (pct > MIDI_VOL_MAX_PCT) {
        pct = MIDI_VOL_MAX_PCT;
    }
    vol_set_name(midi_names[index], pct);
    active_vol = pct;
    dw3_midi_sound_set_vol(midi_vol_to_sep(pct));
    choice_save();
}

int def_midi_preview_play(int index) {
    if (index < 0 || index >= midi_count) {
        return 0;
    }
    if (!load_piece(midi_names[index])) {
        return 0;
    }
    return (int)(0x60000000 | (DW3_MIDI_BANK << 18));
}

s32 dw3_midi_sound_vol(s32 id) {
    if (id != DW3_MIDI_BANK) {
        return 0x7F;
    }
    return midi_vol_to_sep(active_vol);
}

/* fieldstg_encounter_start: the music a random encounter plays: `music` (the stage's key) unless something is chosen and
 * can be played. A single pick plays its file. "All, in order" plays the next file after the last one (by name, wrapping);
 * "all, shuffled" a random one other than the last when there are two or more. A file that does not convert is skipped for
 * the next; when none does, the game's music. */
int def_midi_battle_music(int music) {
    char names[MIDI_LIST_MAX][MIDI_NAME];
    int n, i, start = 0;

    if (!def_on()) {
        return music;
    }
    choice_load();
    if (choice_mode == DEF_MIDI_OWN) {
        return music;
    }
    if (choice_mode == DEF_MIDI_FILE) {
        return load_piece(choice) ? (0x60000000 | DW3_MIDI_BANK << 18) : music;
    }
    n = dir_names(names, 0);
    if (n == 0) {
        return music;
    }
    if (choice_mode == DEF_MIDI_ORDER) {
        /* the first name after `last` in the sorted order: the position is the name, so it survives changes to the folder */
        for (start = 0; start < n && last[0] != 0 && name_cmp(names[start], last) <= 0; start++) {
        }
        start %= n;
    } else {
        start = (int)(rng_next() % (u32)n);
        if (n >= 2 && strcmp(names[start], last) == 0) {
            start = (start + 1 + (int)(rng_next() % (u32)(n - 1))) % n; /* not the one just played */
        }
    }
    for (i = 0; i < n; i++) {
        const char *name = names[(start + i) % n];

        if (n >= 2 && choice_mode == DEF_MIDI_SHUFFLE && i > 0 && strcmp(name, last) == 0) {
            continue; /* the fallbacks do not repeat it either */
        }
        if (load_piece(name)) {
            copy_trunc(last, sizeof(last), name);
            choice_save();
            return 0x60000000 | DW3_MIDI_BANK << 18;
        }
    }
    return music;
}

/* sound_load_bank: the record to load for `id`. */
SoundBank *dw3_midi_sound_bank(s32 id) {
    static SoundBank bank;

    (void)id;
    bank = *sound_banks[BGM031];
    return &bank;
}

/* sound_update_loading, step 1, before the VAB header is opened: the piece goes where BGM031's own SEP is. */
void dw3_midi_sound_header(s32 id, s32 *buffer) {
    u8 *head, *sep;

    if (id != DW3_MIDI_BANK) {
        return;
    }
    head = (u8 *)buffer + buffer[0];
    sep = (u8 *)buffer + buffer[1];
    if (midi_sep_len == 0 || head[0] != 'p' || head[1] != 'B' || head[2] != 'A' || head[3] != 'V' || sep < head ||
        sep + midi_sep_len > (u8 *)buffer + BUFFER_SIZE) {
        port_log("midi: the bank's header does not hold the piece (%u bytes), BGM031's own plays", (unsigned)midi_sep_len);
        return;
    }
    memcpy(sep, midi_sep, midi_sep_len);
    midi_loaded_hash = midi_sep_hash;
    {
        /* the melodic programs' first tone (the drums, program 0, and the noise-burst piano, 10, are not used): the shared
         * envelope and the reverb flag; the oboe's centre 84 plays the notes of a score two octaves flat (the room's fix) */
        static const u8 programs_used[] = { 1, 2, 3, 4, 5, 6, 8 };
        int programs = head[0x12] | head[0x13] << 8, i, k;

        for (k = 0; k < (int)sizeof(programs_used); k++) {
            int prog = programs_used[k], block = 0;
            u8 *tone;
            unsigned adsr1, adsr2;

            for (i = 0; i < prog; i++) {
                block += head[0x20 + 16 * i] != 0;
            }
            if (head[0x20 + 16 * prog] == 0 || block >= programs) {
                continue;
            }
            tone = head + 0x20 + 0x800 + 0x200 * block;
            adsr1 = tone[0x10] | tone[0x11] << 8;
            adsr2 = tone[0x12] | tone[0x13] << 8;
            adsr1 = (adsr1 & 0x00FF) | ATTACK_SHIFT << 10 | ATTACK_STEP << 8; /* linear attack: the exponential bit off */
            adsr2 = (adsr2 & 0xFFC0) | RELEASE_SHIFT;                          /* linear release */
            tone[0x10] = (u8)adsr1;
            tone[0x11] = (u8)(adsr1 >> 8);
            tone[0x12] = (u8)adsr2;
            tone[0x13] = (u8)(adsr2 >> 8);
            tone[1] = 4; /* the reverb flag */
            if (prog == OBOE_PROGRAM) {
                tone[4] = OBOE_CENTER;
            }
        }
    }
}

static int midi_reverb_live; /* the piece raised LIBSND's reverb depth: the game's own is 0 (sound_init) */

static void midi_reverb_off(void) {
    if (midi_reverb_live) {
        midi_reverb_live = 0;
        SsUtSetReverbDepth(0, 0);
    }
}

/* sound_play with the "current" bit: bank `id`'s music starts (any other bank's puts the depth back). */
void dw3_midi_sound_music(s32 id) {
    if (id == DW3_MIDI_BANK) {
        midi_reverb_live = 1;
    } else {
        midi_reverb_off();
    }
}

/* sound_stop of bank `id`'s sound. */
void dw3_midi_sound_stopped(s32 id) {
    if (id == DW3_MIDI_BANK) {
        midi_reverb_off();
    }
}

/* sound_stop_all. */
void dw3_midi_sound_silenced(void) {
    midi_reverb_off();
}
