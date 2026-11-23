/* tests/port/definitive_midi.py's driver for port/game/definitive_midi.c: the folder (argv[1], DW3_MIDI_DIR), the list,
 * the choice file, the battle music and the bank hooks, with BGM031's header file (argv[2]) in the buffer the game copies
 * it to. Writes the patched buffer to argv[3] and prints "ok name" / "FAIL name" and "name=value" lines. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "psyq/libsnd.h"
#include "sound.h"

#include "psxstack/settings.h"

SoundBank bank_33 = { 0x2DF, 0x2DE, 0x2DE0000, 0x2DF0000, { 0x2DE0001, 0 } };
SoundBank *sound_banks[72];
PortSettings port_settings;
int port_settings_loaded;

static int depth_calls;
void SsUtSetReverbDepth(short l, short r) {
    (void)l;
    (void)r;
    depth_calls++;
}
static int forgotten;
void dw3_midi_sound_forget(void) {
    forgotten++;
}
int def_on(void) {
    return 1;
}
int port_make_dirs(const char *dir) {
    (void)dir;
    return 0;
}
void port_log(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    printf("log: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

int def_midi_scan(void);
const char *def_midi_name(int i);
int def_midi_was_cut(int i);
int def_midi_chosen(void);
void def_midi_choose(int i);
int def_midi_battle_music(int music);
void def_midi_dir(char *out, unsigned long size);
SoundBank *dw3_midi_sound_bank(s32 id);
void dw3_midi_sound_header(s32 id, s32 *buffer);
void dw3_midi_sound_music(s32 id);
void dw3_midi_sound_stopped(s32 id);
void dw3_midi_sound_silenced(void);

static int failures;
static void check(const char *name, int ok) {
    if (!ok) {
        failures++;
    }
    printf("%s %s\n", ok ? "ok" : "FAIL", name);
}

static s32 buffer[0xA000 / 4];

int main(int argc, char **argv) {
    FILE *f;
    int count, i, music;
    char path[1024];
    const int own = 0x42;

    if (argc < 4) {
        return 2;
    }
    sound_banks[33] = &bank_33;
    f = fopen(argv[2], "rb");
    if (fread(buffer, 1, sizeof(buffer), f) == 0) {
        return 2;
    }
    fclose(f);

    count = def_midi_scan();
    printf("count=%d\n", count);
    for (i = 0; i < count; i++) {
        printf("file=%s cut=%d\n", def_midi_name(i), def_midi_was_cut(i));
    }
    if (argc > 6 && argv[4][0] == 'p') { /* a playlist run: row argv[5], argv[6] battles; the python side reads the log */
        int k;

        if (atoi(argv[5]) >= 0) {
            def_midi_choose(atoi(argv[5]));
        }
        for (k = 0; k < atoi(argv[6]); k++) {
            printf("key=%x\n", def_midi_battle_music(own));
        }
        return 0;
    }
    check("nothing chosen: the game's own music", def_midi_chosen() == 0 && def_midi_battle_music(own) == own);
    def_midi_choose(3 + (argc > 4 ? atoi(argv[4]) : 0));
    check("a chosen file is read back", def_midi_chosen() == 3 + (argc > 4 ? atoi(argv[4]) : 0));
    music = def_midi_battle_music(own);
    check("the chosen file's key is the MIDI bank's", music == (0x60000000 | 74 << 18));
    check("the bank is a copy of BGM031's record", memcmp(dw3_midi_sound_bank(74), &bank_33, sizeof(bank_33)) == 0);
    dw3_midi_sound_header(74, buffer);
    check("the VAB magic is kept", memcmp((char *)buffer + buffer[0], "pBAV", 4) == 0);
    check("the SEP is in", memcmp((char *)buffer + buffer[1], "pQES", 4) == 0);
    dw3_midi_sound_music(74);
    dw3_midi_sound_music(33);
    check("another bank's music puts the reverb depth back", depth_calls == 1);
    dw3_midi_sound_silenced();
    check("stop-all after that does nothing more", depth_calls == 1);
    dw3_midi_sound_music(74);
    dw3_midi_sound_stopped(33);
    check("another bank's stop leaves it", depth_calls == 1);
    dw3_midi_sound_stopped(74);
    check("the piece's stop puts it back", depth_calls == 2);
    f = fopen(argv[3], "wb");
    fwrite(buffer, 1, sizeof(buffer), f);
    fclose(f);
    check("the same piece again forgets nothing", (def_midi_battle_music(own), forgotten == 0));
    {
        s32 other[4] = { 8, 100, 0, 0 };

        dw3_midi_sound_header(5, other);
        check("another bank's header is left alone", other[0] == 8 && other[1] == 100);
    }

    if (argc > 4) {
        return failures ? 1 : 0; /* a run for one file's SEP only */
    }

    /* the python side changes the chosen file, then deletes it */
    printf("STEP change\n");
    fflush(stdout);
    {
        char line[64];

        if (fgets(line, sizeof(line), stdin) == NULL) {
            return 2;
        }
    }
    def_midi_battle_music(own);
    check("another piece in the chosen file: the loaded bank is forgotten", forgotten == 1);
    printf("STEP delete\n");
    fflush(stdout);
    {
        char line[64];

        if (fgets(line, sizeof(line), stdin) == NULL) {
            return 2;
        }
    }
    check("the chosen file gone: the game's own music", def_midi_battle_music(own) == own);
    def_midi_dir(path, sizeof(path));
    def_midi_choose(0);
    check("the game's own music chosen", def_midi_chosen() == 0 && def_midi_battle_music(own) == own);
    return failures ? 1 : 0;
}
