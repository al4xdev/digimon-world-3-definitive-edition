#include "plat.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "ui.h"

#ifdef __linux__
#include <linux/fb.h>
#include <linux/input.h>
#endif
#ifdef HAVE_SDL2
#include <SDL2/SDL.h>
#elif defined(HAVE_SDL3)
#include <SDL3/SDL.h>
#endif

extern int is_portmaster, is_android;
static int kind_open = -1;
const char *plat_shot_dir = ".";
static int desk_w = 1280, desk_h = 720;

void plat_desktop_size(int *w, int *h) {
    if (w) *w = desk_w;
    if (h) *h = desk_h;
}

#if defined(HAVE_SDL2) || defined(HAVE_SDL3)
static SDL_Window *win;
#endif

void plat_toggle_fullscreen(void) {
#ifdef HAVE_SDL2
    if (kind_open == PLAT_SDL && win) {
        Uint32 f = SDL_GetWindowFlags(win);
        SDL_SetWindowFullscreen(win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
#elif defined(HAVE_SDL3)
    if (kind_open == PLAT_SDL && win) {
        SDL_WindowFlags f = SDL_GetWindowFlags(win);
        SDL_SetWindowFullscreen(win, (f & SDL_WINDOW_FULLSCREEN) == 0);
    }
#endif
}

long plat_now_ms(void);
static long virt_ms;

long plat_now_ms(void) {
    if (kind_open == PLAT_NULL) {
        return virt_ms;
    }
#ifdef _WIN32
    return (long)SDL_GetTicks();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
#endif
}

int plat_open_url(const char *url) {
/* SDL_OpenURL: SDL3, or SDL2 2.0.14 and later (PortMaster's Debian 10 SDL2 is older, and it never asks for it). */
#if defined(HAVE_SDL3) || (defined(HAVE_SDL2) && SDL_VERSION_ATLEAST(2, 0, 14))
    return strncmp(url, "https://", 8) == 0 && SDL_OpenURL(url) ? 0 : -1;
#else
    (void)url;
    return -1;
#endif
}

int plat_pref_dir(char *out, size_t n) {
#if defined(HAVE_SDL2) || defined(HAVE_SDL3)
    /* The game's own default (psxstack/runtime/main.c: SDL_GetPrefPath("", <id>)), and RecompLauncher's
     * (recomplauncher.json: {local}/dw2003 on Linux, {config}/dw2003 on Windows). SDL creates it. */
    char *p = SDL_GetPrefPath("", "dw2003");
    if (!p) return -1;
    size_t l = strlen(p);
    while (l > 1 && (p[l - 1] == '/' || p[l - 1] == '\\')) p[--l] = 0;
    snprintf(out, n, "%s", p);
    SDL_free(p);
    return 0;
#else
    (void)out;
    (void)n;
    return -1;
#endif
}

/* ---- framebuffer + evdev (the handheld's; never on Windows) ---- */
#ifndef _WIN32
#define MAX_EV_FDS 16
static int fbfd = -1;
static int evfds[MAX_EV_FDS];
static int ev_hat_x[MAX_EV_FDS], ev_hat_y[MAX_EV_FDS];
static int ev_count;
static unsigned char *fbmap;
static size_t fbsize;
static int fb_stride, fb_bpp = 32, fb_w = W, fb_h = H;
static int fb_r_off = 16, fb_r_len = 8, fb_g_off = 8, fb_g_len = 8, fb_b_off = 0, fb_b_len = 8;

static int fb_open(void) {
    const char *dev = getenv("DW3_FB");
    fbfd = open(dev ? dev : "/dev/fb0", O_RDWR);
    if (fbfd < 0) {
        return -1;
    }
#ifdef __linux__
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    memset(&var, 0, sizeof(var));
    memset(&fix, 0, sizeof(fix));
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &var) == 0) {
        fb_w = (int)var.xres;
        fb_h = (int)var.yres;
        fb_bpp = (int)var.bits_per_pixel;
        if (var.red.length > 0 && var.green.length > 0 && var.blue.length > 0) {
            fb_r_off = (int)var.red.offset;
            fb_r_len = (int)var.red.length;
            fb_g_off = (int)var.green.offset;
            fb_g_len = (int)var.green.length;
            fb_b_off = (int)var.blue.offset;
            fb_b_len = (int)var.blue.length;
        }
    }
    fb_stride = fb_w * (fb_bpp / 8);
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &fix) == 0 && fix.line_length != 0) {
        fb_stride = (int)fix.line_length;
    }
    /* Portrait hardware panels (e.g. 480x640 RG28XX-H / RGB10) need SDL rotation rather than raw fb0 writes. */
    if (fb_w < 320 || fb_h < 240 || fb_w < fb_h || (fb_bpp != 32 && fb_bpp != 16)) {
        close(fbfd);
        fbfd = -1;
        return -1;
    }
    size_t vis = (size_t)fb_stride * (size_t)fb_h;
    fbsize = fix.smem_len >= vis ? (size_t)fix.smem_len : vis;
    fbmap = mmap(NULL, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmap == MAP_FAILED) {
        fbmap = NULL;
        close(fbfd);
        fbfd = -1;
        return -1;
    }
    memset(fbmap, 0, vis);
    return 0;
#else
    close(fbfd);
    fbfd = -1;
    return -1;
#endif
}

#ifdef __linux__
static int has_bit(const unsigned long *bits, int bit) {
    return (bits[bit / (8 * (int)sizeof(unsigned long))] >> (bit % (8 * (int)sizeof(unsigned long)))) & 1UL;
}

static int ev_is_pad(int fd) {
    unsigned long evbits[2] = {0};
    unsigned long keybits[(KEY_MAX + 8 * sizeof(unsigned long)) / (8 * sizeof(unsigned long))] = {0};
    unsigned long absbits[(ABS_MAX + 8 * sizeof(unsigned long)) / (8 * sizeof(unsigned long))] = {0};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits) < 0) return 0;
    if (has_bit(evbits, EV_KEY) && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0) {
        if (has_bit(keybits, BTN_SOUTH) || has_bit(keybits, BTN_EAST) ||
            has_bit(keybits, BTN_START) || has_bit(keybits, BTN_DPAD_UP)) {
            return 1;
        }
    }
    if (has_bit(evbits, EV_ABS) && ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) >= 0) {
        if (has_bit(absbits, ABS_HAT0X) || has_bit(absbits, ABS_HAT0Y)) {
            return 1;
        }
    }
    return 0;
}
#endif

static int ev_open(void) {
    ev_count = 0;
    for (int i = 0; i < MAX_EV_FDS; i++) {
        evfds[i] = -1;
        ev_hat_x[i] = 0;
        ev_hat_y[i] = 0;
    }
#ifdef __linux__
    const char *dev = getenv("DW3_PAD");
    if (dev && dev[0]) {
        int fd = open(dev, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            struct input_event e;
            while (read(fd, &e, sizeof(e)) > 0) {}
            evfds[ev_count++] = fd;
        }
        return ev_count > 0 ? 0 : -1;
    }
    for (int i = 0; i < MAX_EV_FDS; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (!ev_is_pad(fd)) {
            close(fd);
            continue;
        }
        struct input_event e; /* the button that launched this must not count */
        while (read(fd, &e, sizeof(e)) > 0) {}
        evfds[ev_count++] = fd;
    }
    if (ev_count == 0) {
        int fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            struct input_event e;
            while (read(fd, &e, sizeof(e)) > 0) {}
            evfds[ev_count++] = fd;
        }
    }
#endif
    return ev_count > 0 ? 0 : -1;
}
#endif

#ifdef HAVE_SDL2
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_GameController *pad;
static int trig_l, trig_r;

static int sdl_open(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
            return -1;
        }
    }
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
        desk_w = dm.w;
        desk_h = dm.h;
    }
    int pm_mode = is_portmaster || is_android || getenv("DW3_PORTMASTER") != NULL || getenv("CFW_NAME") != NULL;
    int win_w = W, win_h = H;
    Uint32 wflags = 0;
    if (pm_mode) {
        ui_init(1);
        wflags = SDL_WINDOW_FULLSCREEN_DESKTOP;
    } else {
        /* Pick crisp internal UI render scale (2x = 1280x960, 3x = 1920x1440) and fit window comfortably on monitor. */
        int sc = (desk_h >= 1350 && desk_w >= 1800) ? 3 : 2;
        ui_init(sc);
        win_h = desk_h * 82 / 100;
        if (win_h < 480) win_h = 480;
        if (win_h > ui_h) win_h = ui_h;
        win_w = win_h * 4 / 3;
        if (win_w > desk_w * 90 / 100) {
            win_w = desk_w * 90 / 100;
            win_h = win_w * 3 / 4;
        }
        wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    win = SDL_CreateWindow(pm_mode ? "dw3-launcher" : "Digimon World 3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           win_w, win_h, wflags);
    if (!win) {
        return -1;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        ren = SDL_CreateRenderer(win, -1, 0);
    }
    if (!ren) {
        SDL_DestroyWindow(win);
        win = NULL;
        return -1;
    }
    SDL_RenderSetLogicalSize(ren, ui_w, ui_h);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ui_w, ui_h);
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i) && (pad = SDL_GameControllerOpen(i))) {
            break;
        }
    }
    return 0;
}

static int key_code(SDL_Keycode k) {
    switch (k) {
    case SDLK_RETURN: case SDLK_z: return K_SOUTH;
    case SDLK_x: case SDLK_BACKSPACE: case SDLK_ESCAPE: return K_EAST;
    case SDLK_s: return K_WEST;
    case SDLK_a: return K_NORTH;
    case SDLK_q: return K_L1;
    case SDLK_e: return K_R1;
    case SDLK_1: return K_L2;
    case SDLK_3: return K_R2;
    case SDLK_TAB: return K_SELECT;
    case SDLK_SPACE: return K_START;
    case SDLK_UP: return K_UP;
    case SDLK_DOWN: return K_DOWN;
    case SDLK_LEFT: return K_LEFT;
    case SDLK_RIGHT: return K_RIGHT;
    }
    return 0;
}

static int btn_code(int b) {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_A: return K_SOUTH;
    case SDL_CONTROLLER_BUTTON_B: return K_EAST;
    case SDL_CONTROLLER_BUTTON_X: return K_NORTH; /* the handheld's kernel calls the physical left "north" */
    case SDL_CONTROLLER_BUTTON_Y: return K_WEST;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return K_L1;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return K_R1;
    case SDL_CONTROLLER_BUTTON_BACK: return K_SELECT;
    case SDL_CONTROLLER_BUTTON_START: return K_START;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return K_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return K_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return K_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return K_RIGHT;
    }
    return 0;
}
#elif defined(HAVE_SDL3)
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_Gamepad *pad;
static int trig_l, trig_r;

static int sdl_open(void) {
    if (is_android) {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            return -1;
        }
    }
    const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (dm && dm->w > 0 && dm->h > 0) {
        desk_w = dm->w;
        desk_h = dm->h;
    }
    int pm_mode = is_portmaster || is_android || getenv("DW3_PORTMASTER") != NULL || getenv("CFW_NAME") != NULL;
    int win_w = W, win_h = H;
    SDL_WindowFlags wflags = 0;
    if (pm_mode) {
        ui_init(1);
        wflags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    } else {
        int sc = (desk_h >= 1350 && desk_w >= 1800) ? 3 : 2;
        ui_init(sc);
        win_h = desk_h * 82 / 100;
        if (win_h < 480) win_h = 480;
        if (win_h > ui_h) win_h = ui_h;
        win_w = win_h * 4 / 3;
        if (win_w > desk_w * 90 / 100) {
            win_w = desk_w * 90 / 100;
            win_h = win_w * 3 / 4;
        }
        wflags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        /* RecompLauncher's Big Picture (couch play, a controller in hand): fullscreen, as the game does then
         * (psxstack/runtime/main.c) */
        const char *rl = getenv("RECOMPLAUNCHER_MODE");
        if (rl && !strcmp(rl, "bigpicture")) wflags |= SDL_WINDOW_FULLSCREEN;
    }
    win = SDL_CreateWindow(pm_mode ? "dw3-launcher" : "Digimon World 3", win_w, win_h, wflags);
    if (!win) {
        return -1;
    }
    ren = SDL_CreateRenderer(win, NULL);
    if (!ren) {
        SDL_DestroyWindow(win);
        win = NULL;
        return -1;
    }
    SDL_SetRenderVSync(ren, 1);
    SDL_SetRenderLogicalPresentation(ren, ui_w, ui_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, ui_w, ui_h);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    }
    int njoys = 0;
    SDL_JoystickID *joys = SDL_GetGamepads(&njoys);
    if (joys) {
        if (njoys > 0) {
            pad = SDL_OpenGamepad(joys[0]);
        }
        SDL_free(joys);
    }
    return 0;
}

static int key_code(SDL_Keycode k) {
    switch (k) {
    case SDLK_RETURN: case SDLK_Z: return K_SOUTH;
    case SDLK_X: case SDLK_BACKSPACE: case SDLK_ESCAPE: return K_EAST;
    case SDLK_S: return K_WEST;
    case SDLK_A: return K_NORTH;
    case SDLK_Q: return K_L1;
    case SDLK_E: return K_R1;
    case SDLK_1: return K_L2;
    case SDLK_3: return K_R2;
    case SDLK_TAB: return K_SELECT;
    case SDLK_SPACE: return K_START;
    case SDLK_UP: return K_UP;
    case SDLK_DOWN: return K_DOWN;
    case SDLK_LEFT: return K_LEFT;
    case SDLK_RIGHT: return K_RIGHT;
    }
    return 0;
}

static int btn_code(int b) {
    switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return K_SOUTH;
    case SDL_GAMEPAD_BUTTON_EAST: return K_EAST;
    case SDL_GAMEPAD_BUTTON_WEST: return K_NORTH;
    case SDL_GAMEPAD_BUTTON_NORTH: return K_WEST;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return K_L1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return K_R1;
    case SDL_GAMEPAD_BUTTON_BACK: return K_SELECT;
    case SDL_GAMEPAD_BUTTON_START: return K_START;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return K_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return K_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return K_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return K_RIGHT;
    }
    return 0;
}
#endif

/* ---- null: a script of tokens, PPM shots ---- */
static char *script_text, *script_pos;

static int code_of(const char *t) {
    static const struct { const char *n; int c; } names[] = {
        {"south", K_SOUTH}, {"a", K_SOUTH}, {"east", K_EAST}, {"b", K_EAST}, {"west", K_WEST}, {"north", K_NORTH},
        {"l1", K_L1}, {"r1", K_R1}, {"l2", K_L2}, {"r2", K_R2}, {"select", K_SELECT}, {"start", K_START},
        {"up", K_UP}, {"down", K_DOWN}, {"left", K_LEFT}, {"right", K_RIGHT},
    };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strcmp(t, names[i].n) == 0) {
            return names[i].c;
        }
    }
    return 0;
}

static void save_ppm(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", plat_shot_dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", ui_w, ui_h);
    for (int i = 0; i < ui_w * ui_h; i++) {
        fputc((fb[i] >> 16) & 255, f);
        fputc((fb[i] >> 8) & 255, f);
        fputc(fb[i] & 255, f);
    }
    fclose(f);
}

void plat_shot(const char *name) {
    save_ppm(name);
}

static int pending_release; /* a press token emits its release on the next poll */
static int pending_code;
static int pending_x, pending_y;
static int wait_ticks;

int plat_open(int kind, const char *script) {
    kind_open = kind;
    if (kind == PLAT_NULL) {
        const char *sc = getenv("DW3_UI_SCALE");
        ui_init(sc ? atoi(sc) : 1);
        script_text = strdup(script ? script : "");
        script_pos = script_text;
        return 0;
    }
    if (kind == PLAT_FB) {
#ifdef _WIN32
        return -1;
#else
        ui_init(1);
        if (fb_open() != 0) {
            return -1;
        }
        if (ev_open() != 0) {
            munmap(fbmap, fbsize);
            close(fbfd);
            fbfd = -1;
            return -1;
        }
        return 0;
#endif
    }
#if defined(HAVE_SDL2) || defined(HAVE_SDL3)
    return sdl_open();
#else
    return -1;
#endif
}

#ifndef _WIN32
static uint32_t fb_pack32(uint32_t c) {
    if (fb_r_off == 16 && fb_g_off == 8 && fb_b_off == 0) {
        return c & 0x00ffffffu;
    }
    uint32_t r = (c >> 16) & 0xffu, g = (c >> 8) & 0xffu, b = c & 0xffu;
    return ((r >> (8 - fb_r_len)) << fb_r_off) | ((g >> (8 - fb_g_len)) << fb_g_off) | ((b >> (8 - fb_b_len)) << fb_b_off);
}

static uint16_t fb_pack16(uint32_t c) {
    uint32_t r = (c >> 16) & 0xffu, g = (c >> 8) & 0xffu, b = c & 0xffu;
    return (uint16_t)(((r >> (8 - fb_r_len)) << fb_r_off) | ((g >> (8 - fb_g_len)) << fb_g_off) | ((b >> (8 - fb_b_len)) << fb_b_off));
}

#endif

void plat_present(void) {
#ifndef _WIN32
    if (kind_open == PLAT_FB && fbmap) {
        int fast_xrgb = (fb_bpp == 32 && fb_r_off == 16 && fb_g_off == 8 && fb_b_off == 0);
        if (fb_w >= W && fb_h >= H) {
            int sx = fb_w / W, sy = fb_h / H;
            int sc = sx < sy ? sx : sy;
            if (sc < 1) sc = 1;
            int ox = (fb_w - W * sc) / 2, oy = (fb_h - H * sc) / 2;
            if (sc == 1 && fast_xrgb) {
                for (int y = 0; y < H; y++) {
                    memcpy((uint32_t *)(fbmap + (size_t)(oy + y) * fb_stride) + ox, &fb[y * W], W * 4);
                }
            } else {
                for (int y = 0; y < H; y++) {
                    for (int ry = 0; ry < sc; ry++) {
                        unsigned char *rowp = fbmap + (size_t)(oy + y * sc + ry) * fb_stride;
                        if (fb_bpp == 32) {
                            uint32_t *d = (uint32_t *)rowp + ox;
                            for (int x = 0; x < W; x++) {
                                uint32_t p = fast_xrgb ? (fb[y * W + x] & 0x00ffffffu) : fb_pack32(fb[y * W + x]);
                                for (int rx = 0; rx < sc; rx++) d[x * sc + rx] = p;
                            }
                        } else {
                            uint16_t *d = (uint16_t *)rowp + ox;
                            for (int x = 0; x < W; x++) {
                                uint16_t p = fb_pack16(fb[y * W + x]);
                                for (int rx = 0; rx < sc; rx++) d[x * sc + rx] = p;
                            }
                        }
                    }
                }
            }
        } else {
            /* Sub-640x480 panels (e.g. 480x320 RG351P/M): scale down cleanly to fit fb_w x fb_h. */
            for (int y = 0; y < fb_h; y++) {
                int sy = y * H / fb_h;
                unsigned char *rowp = fbmap + (size_t)y * fb_stride;
                if (fb_bpp == 32) {
                    uint32_t *d = (uint32_t *)rowp;
                    for (int x = 0; x < fb_w; x++) {
                        int sx = x * W / fb_w;
                        d[x] = fast_xrgb ? (fb[sy * W + sx] & 0x00ffffffu) : fb_pack32(fb[sy * W + sx]);
                    }
                } else {
                    uint16_t *d = (uint16_t *)rowp;
                    for (int x = 0; x < fb_w; x++) {
                        int sx = x * W / fb_w;
                        d[x] = fb_pack16(fb[sy * W + sx]);
                    }
                }
            }
        }
    }
#endif
#ifdef HAVE_SDL2
    if (kind_open == PLAT_SDL && ren) {
        SDL_UpdateTexture(tex, NULL, fb, ui_w * 4);
        SDL_SetRenderDrawColor(ren, 7, 16, 25, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
#elif defined(HAVE_SDL3)
    if (kind_open == PLAT_SDL && ren) {
        SDL_UpdateTexture(tex, NULL, fb, ui_w * 4);
        SDL_SetRenderDrawColor(ren, 7, 16, 25, 255);
        SDL_RenderClear(ren);
        SDL_RenderTexture(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
#endif
}

int plat_poll(Ev *ev, int timeout_ms) {
    if (kind_open == PLAT_NULL) {
        virt_ms += 33;
        if (pending_release) {
            pending_release = 0;
            ev->code = pending_code;
            ev->down = 0;
            ev->x = pending_x;
            ev->y = pending_y;
            return 1;
        }
        if (wait_ticks > 0) {
            wait_ticks--;
            return 0;
        }
        while (script_pos && *script_pos) {
            while (*script_pos == ' ' || *script_pos == ',' || *script_pos == '\n') {
                script_pos++;
            }
            char *e = script_pos;
            while (*e && *e != ',' && *e != ' ' && *e != '\n') {
                e++;
            }
            char tok[96];
            size_t n = (size_t)(e - script_pos) < sizeof(tok) - 1 ? (size_t)(e - script_pos) : sizeof(tok) - 1;
            memcpy(tok, script_pos, n);
            tok[n] = 0;
            script_pos = e;
            if (!tok[0]) {
                continue;
            }
            if (strncmp(tok, "shot:", 5) == 0) {
                save_ppm(tok + 5);
                continue;
            }
            if (strncmp(tok, "wait:", 5) == 0) {
                wait_ticks = atoi(tok + 5);
                return 0;
            }
            if (strncmp(tok, "tap:", 4) == 0) {
                int tx = 0, ty = 0;
                if (sscanf(tok + 4, "%d:%d", &tx, &ty) == 2 || sscanf(tok + 4, "%d;%d", &tx, &ty) == 2) {
                    pending_release = 1;
                    pending_code = K_TOUCH_UP;
                    pending_x = tx;
                    pending_y = ty;
                    ev->code = K_TOUCH_DOWN;
                    ev->down = 1;
                    ev->x = tx;
                    ev->y = ty;
                    return 1;
                }
                continue;
            }
            if (strncmp(tok, "hold:", 5) == 0 || strncmp(tok, "release:", 8) == 0) {
                int down = tok[0] == 'h';
                ev->code = code_of(tok + (down ? 5 : 8));
                ev->down = down;
                ev->x = ev->y = 0;
                return 1;
            }
            int c = code_of(tok);
            if (c) {
                pending_release = 1;
                pending_code = c;
                pending_x = pending_y = 0;
                ev->code = c;
                ev->down = 1;
                ev->x = ev->y = 0;
                return 1;
            }
        }
        return -1; /* the script ended */
    }
    if (kind_open == PLAT_FB) {
#ifdef __linux__
        struct pollfd p[MAX_EV_FDS];
        for (int i = 0; i < ev_count; i++) {
            p[i].fd = evfds[i];
            p[i].events = POLLIN;
            p[i].revents = 0;
        }
        if (ev_count > 0 && poll(p, (nfds_t)ev_count, timeout_ms) > 0) {
            for (int i = 0; i < ev_count; i++) {
                if (!(p[i].revents & POLLIN)) continue;
                struct input_event e;
                while (read(evfds[i], &e, sizeof(e)) == (ssize_t)sizeof(e)) {
                    if (e.type == EV_KEY && e.value != 2) {
                        ev->code = e.code;
                        ev->down = e.value;
                        return 1;
                    }
                    if (e.type == EV_ABS && (e.code == ABS_HAT0X || e.code == ABS_HAT0Y)) {
                        int *prev = e.code == ABS_HAT0X ? &ev_hat_x[i] : &ev_hat_y[i];
                        int nv = e.value < 0 ? -1 : e.value > 0 ? 1 : 0;
                        if (nv != *prev) {
                            int old = *prev;
                            *prev = nv;
                            if (nv != 0) {
                                ev->code = e.code == ABS_HAT0X ? (nv < 0 ? K_LEFT : K_RIGHT) : (nv < 0 ? K_UP : K_DOWN);
                                ev->down = 1;
                            } else {
                                ev->code = e.code == ABS_HAT0X ? (old < 0 ? K_LEFT : K_RIGHT) : (old < 0 ? K_UP : K_DOWN);
                                ev->down = 0;
                            }
                            return 1;
                        }
                    }
                }
            }
        }
#endif
        return 0;
    }
#ifdef HAVE_SDL2
    if (kind_open == PLAT_SDL) {
        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, timeout_ms)) {
            return 0;
        }
        ev->code = 0;
        ev->x = ev->y = 0;
        switch (e.type) {
        case SDL_QUIT: return -1;
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP: {
            int ow = 0, oh = 0;
            SDL_GetRendererOutputSize(ren, &ow, &oh);
            if (ow <= 0 || oh <= 0) { ow = W; oh = H; }
            float sx = (float)ow / (float)ui_w;
            float sy = (float)oh / (float)ui_h;
            float sc = sx < sy ? sx : sy;
            float vw = (float)ui_w * sc, vh = (float)ui_h * sc;
            float vx = ((float)ow - vw) * 0.5f, vy = ((float)oh - vh) * 0.5f;
            int lx = (int)(((e.tfinger.x * (float)ow - vx) / vw) * (float)W);
            int ly = (int)(((e.tfinger.y * (float)oh - vy) / vh) * (float)H);
            if (lx < 0) lx = 0;
            if (lx >= W) lx = W - 1;
            if (ly < 0) ly = 0;
            if (ly >= H) ly = H - 1;
            ev->x = lx;
            ev->y = ly;
            ev->code = e.type == SDL_FINGERDOWN ? K_TOUCH_DOWN : e.type == SDL_FINGERUP ? K_TOUCH_UP : K_TOUCH_MOVE;
            ev->down = e.type != SDL_FINGERUP;
            return 1;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) return 0;
            ev->x = e.button.x * W / (ui_w > 0 ? ui_w : W);
            ev->y = e.button.y * H / (ui_h > 0 ? ui_h : H);
            ev->code = e.type == SDL_MOUSEBUTTONDOWN ? K_TOUCH_DOWN : K_TOUCH_UP;
            ev->down = e.type == SDL_MOUSEBUTTONDOWN;
            return 1;
        case SDL_KEYDOWN: case SDL_KEYUP:
            if (e.key.repeat) {
                return 0;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F11) {
                plat_toggle_fullscreen();
                return 0;
            }
            ev->code = key_code(e.key.keysym.sym);
            ev->down = e.type == SDL_KEYDOWN;
            return ev->code != 0;
        case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
            ev->code = btn_code(e.cbutton.button);
            ev->down = e.type == SDL_CONTROLLERBUTTONDOWN;
            return ev->code != 0;
        case SDL_CONTROLLERAXISMOTION: {
            int *t = e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? &trig_l : e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT ? &trig_r : NULL;
            if (!t) {
                return 0;
            }
            int on = e.caxis.value > 16000;
            if (on == *t) {
                return 0;
            }
            *t = on;
            ev->code = e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? K_L2 : K_R2;
            ev->down = on;
            return 1;
        }
        case SDL_CONTROLLERDEVICEADDED:
            if (!pad) {
                pad = SDL_GameControllerOpen(e.cdevice.which);
            }
            return 0;
        }
        return 0;
    }
#elif defined(HAVE_SDL3)
    if (kind_open == PLAT_SDL) {
        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, timeout_ms)) {
            return 0;
        }
        ev->code = 0;
        ev->x = ev->y = 0;
        switch (e.type) {
        case SDL_EVENT_QUIT: return -1;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP: {
            int ow = 0, oh = 0;
            SDL_GetRenderOutputSize(ren, &ow, &oh);
            if (ow <= 0 || oh <= 0) { ow = W; oh = H; }
            float sx = (float)ow / (float)ui_w;
            float sy = (float)oh / (float)ui_h;
            float sc = sx < sy ? sx : sy;
            float vw = (float)ui_w * sc, vh = (float)ui_h * sc;
            float vx = ((float)ow - vw) * 0.5f, vy = ((float)oh - vh) * 0.5f;
            int lx = (int)(((e.tfinger.x * (float)ow - vx) / vw) * (float)W);
            int ly = (int)(((e.tfinger.y * (float)oh - vy) / vh) * (float)H);
            if (lx < 0) lx = 0;
            if (lx >= W) lx = W - 1;
            if (ly < 0) ly = 0;
            if (ly >= H) ly = H - 1;
            ev->x = lx;
            ev->y = ly;
            ev->code = e.type == SDL_EVENT_FINGER_DOWN ? K_TOUCH_DOWN : e.type == SDL_EVENT_FINGER_UP ? K_TOUCH_UP : K_TOUCH_MOVE;
            ev->down = e.type != SDL_EVENT_FINGER_UP;
            return 1;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) return 0;
            ev->x = (int)(e.button.x * W / (ui_w > 0 ? ui_w : W));
            ev->y = (int)(e.button.y * H / (ui_h > 0 ? ui_h : H));
            ev->code = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? K_TOUCH_DOWN : K_TOUCH_UP;
            ev->down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            return 1;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (e.key.repeat) {
                return 0;
            }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F11) {
                plat_toggle_fullscreen();
                return 0;
            }
            ev->code = is_android && e.key.key == SDLK_AC_BACK ? K_BACK : key_code(e.key.key);
            ev->down = e.type == SDL_EVENT_KEY_DOWN;
            return ev->code != 0;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            ev->code = btn_code(e.gbutton.button);
            ev->down = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            return ev->code != 0;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            int *t = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? &trig_l :
                     e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER ? &trig_r : NULL;
            if (!t) {
                return 0;
            }
            int on = e.gaxis.value > 16000;
            if (on == *t) {
                return 0;
            }
            *t = on;
            ev->code = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? K_L2 : K_R2;
            ev->down = on;
            return 1;
        }
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!pad) {
                pad = SDL_OpenGamepad(e.gdevice.which);
            }
            return 0;
        }
        return 0;
    }
#endif
    return 0;
}

void plat_release(void) {
#ifndef _WIN32
    for (int i = 0; i < ev_count; i++) {
        if (evfds[i] >= 0) {
            close(evfds[i]);
            evfds[i] = -1;
        }
    }
    ev_count = 0;
    if (fbmap) {
        munmap(fbmap, fbsize);
        fbmap = NULL;
    }
    if (fbfd >= 0) {
        close(fbfd);
        fbfd = -1;
    }
#endif
#ifdef HAVE_SDL2
    if (kind_open == PLAT_SDL) {
        if (pad) { SDL_GameControllerClose(pad); pad = NULL; }
        if (tex) SDL_DestroyTexture(tex);
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        tex = NULL; ren = NULL; win = NULL;
        if (is_android) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
        } else {
            SDL_Quit();
        }
    }
#elif defined(HAVE_SDL3)
    if (kind_open == PLAT_SDL) {
        if (pad) { SDL_CloseGamepad(pad); pad = NULL; }
        if (tex) SDL_DestroyTexture(tex);
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        tex = NULL; ren = NULL; win = NULL;
        if (is_android) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
        } else {
            SDL_Quit();
        }
    }
#endif
    kind_open = -1;
}
