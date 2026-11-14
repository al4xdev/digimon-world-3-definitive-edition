#ifndef DW3_ASSETS_H
#define DW3_ASSETS_H
/* The pixel art and the font, as data (launcher/tools/gen_assets.py). */
typedef struct Sprite {
    const char *name; /* "a1": Agumon frame 1, "g3": Guilmon, "k2": Kumamon */
    int w, h;
    const unsigned char *px; /* w*h: 0 clear, 1 outline, 2 body */
} Sprite;
extern const Sprite sprites[];
extern const int sprite_count;
extern const unsigned char font_data[];
extern const unsigned int font_size;
#endif
