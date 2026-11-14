#include "plat.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "ui.h"

#ifdef __linux__
#include <linux/input.h>
#endif
#ifdef HAVE_SDL2
#include <SDL2/SDL.h>
#endif

static int kind_open = -1;
const char *plat_shot_dir = ".";
static int desk_w = 1280, desk_h = 720;

void plat_desktop_size(int *w, int *h) {
    if (w) *w = desk_w;
    if (h) *h = desk_h;
}

#ifdef HAVE_SDL2
static SDL_Window *win;
#endif

void plat_toggle_fullscreen(void) {
#ifdef HAVE_SDL2
    if (kind_open == PLAT_SDL && win) {
        Uint32 f = SDL_GetWindowFlags(win);
        SDL_SetWindowFullscreen(win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
#endif
}

long plat_now_ms(void);
static long virt_ms;

long plat_now_ms(void) {
    if (kind_open == PLAT_NULL) {
        return virt_ms;
    }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* ---- framebuffer + evdev ---- */
static int fbfd = -1, evfd = -1;
static unsigned char *fbmap;
static size_t fbsize;
static int fb_stride, fb_bpp = 32, fb_w = W, fb_h = H;

static int fb_open(void) {
    const char *dev = getenv("DW3_FB");
    fbfd = open(dev ? dev : "/dev/fb0", O_RDWR);
    if (fbfd < 0) {
        return -1;
    }
    unsigned var[40] = {0}, fix[20] = {0};
    if (ioctl(fbfd, 0x4600, var) == 0) { /* FBIOGET_VSCREENINFO: xres, yres, ..., bits_per_pixel at word 6 */
        fb_w = (int)var[0];
        fb_h = (int)var[1];
        fb_bpp = (int)var[6];
    }
    fb_stride = fb_w * (fb_bpp / 8);
    if (ioctl(fbfd, 0x4602, fix) == 0 && fix[6] != 0) { /* FBIOGET_FSCREENINFO: line_length */
        fb_stride = (int)fix[6];
    }
    if (fb_w < W || fb_h < H || (fb_bpp != 32 && fb_bpp != 16)) {
        close(fbfd);
        fbfd = -1;
        return -1;
    }
    fbsize = (size_t)fb_stride * fb_h;
    fbmap = mmap(NULL, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmap == MAP_FAILED) {
        fbmap = NULL;
        close(fbfd);
        fbfd = -1;
        return -1;
    }
    return 0;
}

static int ev_open(void) {
    const char *dev = getenv("DW3_PAD");
    evfd = open(dev ? dev : "/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (evfd < 0) {
        return -1;
    }
    struct input_event e; /* the button that launched this must not count */
    while (read(evfd, &e, sizeof(e)) > 0) {
    }
    return 0;
}

#ifdef HAVE_SDL2
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_GameController *pad;
static int trig_l, trig_r;

static int sdl_open(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
        return -1;
    }
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
        desk_w = dm.w;
        desk_h = dm.h;
    }
    /* Pick crisp internal UI render scale (2x = 1280x960, 3x = 1920x1440) and fit window comfortably on monitor. */
    int sc = (desk_h >= 1350 && desk_w >= 1800) ? 3 : 2;
    ui_init(sc);
    int win_h = desk_h * 82 / 100;
    if (win_h < 480) win_h = 480;
    if (win_h > ui_h) win_h = ui_h;
    int win_w = win_h * 4 / 3;
    if (win_w > desk_w * 90 / 100) {
        win_w = desk_w * 90 / 100;
        win_h = win_w * 3 / 4;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    win = SDL_CreateWindow("Digimon World 3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        return -1;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
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

static int pending_release; /* a press token emits its release on the next poll */
static int pending_code;
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
    }
#ifdef HAVE_SDL2
    return sdl_open();
#else
    return -1;
#endif
}

void plat_present(void) {
    if (kind_open == PLAT_FB && fbmap) {
        for (int y = 0; y < H; y++) {
            if (fb_bpp == 32) {
                memcpy(fbmap + (size_t)y * fb_stride, &fb[y * W], W * 4);
            } else {
                uint16_t *d = (uint16_t *)(fbmap + (size_t)y * fb_stride);
                for (int x = 0; x < W; x++) {
                    uint32_t c = fb[y * W + x];
                    d[x] = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x1F));
                }
            }
        }
    }
#ifdef HAVE_SDL2
    if (kind_open == PLAT_SDL && ren) {
        SDL_UpdateTexture(tex, NULL, fb, ui_w * 4);
        SDL_SetRenderDrawColor(ren, 7, 16, 25, 255);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
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
            if (strncmp(tok, "hold:", 5) == 0 || strncmp(tok, "release:", 8) == 0) {
                int down = tok[0] == 'h';
                ev->code = code_of(tok + (down ? 5 : 8));
                ev->down = down;
                return 1;
            }
            int c = code_of(tok);
            if (c) {
                pending_release = 1;
                pending_code = c;
                ev->code = c;
                ev->down = 1;
                return 1;
            }
        }
        return -1; /* the script ended */
    }
    if (kind_open == PLAT_FB) {
        struct pollfd p = { evfd, POLLIN, 0 };
        if (poll(&p, 1, timeout_ms) > 0) {
            struct input_event e;
            while (read(evfd, &e, sizeof(e)) == (ssize_t)sizeof(e)) {
                if (e.type == EV_KEY && e.value != 2) {
                    ev->code = e.code;
                    ev->down = e.value;
                    return 1;
                }
            }
        }
        return 0;
    }
#ifdef HAVE_SDL2
    if (kind_open == PLAT_SDL) {
        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, timeout_ms)) {
            return 0;
        }
        ev->code = 0;
        switch (e.type) {
        case SDL_QUIT: return -1;
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
#endif
    return 0;
}

void plat_release(void) {
    if (evfd >= 0) {
        close(evfd);
        evfd = -1;
    }
    if (fbmap) {
        munmap(fbmap, fbsize);
        fbmap = NULL;
    }
    if (fbfd >= 0) {
        close(fbfd);
        fbfd = -1;
    }
#ifdef HAVE_SDL2
    if (kind_open == PLAT_SDL) {
        if (tex) SDL_DestroyTexture(tex);
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        tex = NULL; ren = NULL; win = NULL;
        SDL_Quit();
    }
#endif
    kind_open = -1;
}
