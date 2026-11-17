/* tests/port/definitive_velvet_music.py's driver for port/game/definitive_velvet_music.c: BGM031's header file (argv[1], as
 * the disc has it, in the 40 KB buffer sound_update_loading copies it to) goes through the hooks the way the game calls
 * them. Prints the buffer's SEP and tone edits for the Python side to check, and "ok name" / "FAIL name" lines. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

#include "psyq/libsnd.h"
#include "sound.h"

#include "psxstack/spu.h"

SoundBank bank_33 = { 0x2DF, 0x2DE, 0x2DE0000, 0x2DF0000, { 0x2DE0001, 0 } };
SoundBank bank_5;
SoundBank *sound_banks[72];

static int depth_calls, depth_last = -1;
void SsUtSetReverbDepth(short l, short r) {
    (void)r;
    depth_calls++;
    depth_last = l;
}
void port_log(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    printf("log: ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

static uint32_t xfer;
static unsigned char spu_ram[SPU_RAM_SIZE];
void spu_write16(uint32_t offset, uint16_t value) {
    if (offset == 0x1A6) {
        xfer = (uint32_t)value * 8;
    }
}
void spu_dma_write(const uint16_t *data, uint32_t halfwords) {
    memcpy(spu_ram + xfer, data, halfwords * 2);
    xfer += halfwords * 2;
}

SoundBank *dw3_velvet_sound_bank(s32 id);
void dw3_velvet_sound_header(s32 id, s32 *buffer);
void dw3_velvet_sound_body(s32 id, s32 *buffer, s32 spu_addr);
void dw3_velvet_sound_music(s32 id);
void dw3_velvet_sound_stopped(s32 id);
void dw3_velvet_sound_silenced(void);
void dw3_velvet_music(s32 *music, s32 *sound);

static int failures;
static void check(const char *name, int ok) {
    if (!ok) {
        failures++;
    }
    printf("%s %s\n", ok ? "ok" : "FAIL", name);
}

static s32 buffer[0xA000 / 4];
static s32 other[0xA000 / 4];

int main(int argc, char **argv) {
    FILE *f;
    size_t n;
    s32 music = 0x3B, sound = 0x60EC0000;
    SoundBank *b;
    s32 spu_addr = 0x49C10;
    int i;

    if (argc < 3) {
        return 2;
    }
    sound_banks[33] = &bank_33;
    sound_banks[5] = &bank_5;
    f = fopen(argv[1], "rb");
    n = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);
    memcpy(other, buffer, n);

    dw3_velvet_music(&music, &sound);
    check("stage music is the room's bank", music == 72 && sound == (0x60000000 | 72 << 18));
    b = dw3_velvet_sound_bank(72);
    check("room bank is BGM031's files", b != &bank_33 && memcmp(b, &bank_33, sizeof(bank_33)) == 0);
    check("other banks are the table's", dw3_velvet_sound_bank(5) == &bank_5);

    dw3_velvet_sound_header(5, other);
    check("other banks' headers are left alone", memcmp(other, buffer, n) == 0);
    {
        s32 saved[2] = { buffer[0], buffer[1] };

        dw3_velvet_sound_header(72, buffer);
        check("sub-file table untouched", buffer[0] == saved[0] && buffer[1] == saved[1]);
        check("VAB magic kept", memcmp((char *)buffer + buffer[0], "pBAV", 4) == 0);
        check("SEP magic", memcmp((char *)buffer + buffer[1], "pQES", 4) == 0);
    }
    /* the VAB header up to the SEP is only edited in the tone records, never past them */
    f = fopen(argv[2], "wb");
    fwrite(buffer, 1, sizeof(buffer), f);
    fclose(f);

    /* the piano goes where the VAB's size table puts vag 14 */
    dw3_velvet_sound_body(5, other, spu_addr);
    check("other banks get no piano", xfer == 0);
    dw3_velvet_sound_body(72, buffer, spu_addr);
    f = fopen(argv[2], "ab");
    fwrite(&xfer, 4, 1, f);
    fwrite(spu_ram, 1, SPU_RAM_SIZE, f);
    fclose(f);

    dw3_velvet_sound_music(72);
    check("room music leaves the depth to its score", depth_calls == 0);
    dw3_velvet_sound_music(33);
    i = depth_calls;
    check("other music puts the reverb depth back", i == 1 && depth_last == 0);
    dw3_velvet_sound_silenced();
    check("silence after that does nothing more", depth_calls == 1);
    dw3_velvet_sound_music(72);
    dw3_velvet_sound_stopped(33);
    check("another bank's stop does not touch it", depth_calls == 1);
    dw3_velvet_sound_stopped(72);
    check("the room's stop puts it back", depth_calls == 2);
    dw3_velvet_sound_music(72);
    dw3_velvet_sound_silenced();
    check("stop-all puts it back", depth_calls == 3);
    return failures ? 1 : 0;
}
