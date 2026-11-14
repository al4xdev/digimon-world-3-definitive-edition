#ifndef DW3_LOADING_H
#define DW3_LOADING_H
/* The loading screen's game: three Digimon run in file, a firewall comes from the right, and each one jumps on its
 * own button. It never touches the real load: it only fills the time the game needs to check the disc and open. */
#include <stdint.h>

struct Cfg;
void load_init(const char *best_file);
void load_press(int code);                    /* a button went down: jump whoever it belongs to */
void load_step(long dt_ms);
void load_draw(const struct Cfg *c, long tick_ms, const char *stage, int percent);
#endif
