# Desktop build of the launcher (Linux): make [SDL=1]; the handheld build is tools/build_arm.sh (framebuffer, no SDL).
SRC = main.c ui.c plat.c cfg.c loading.c cloud.c sha256.c assets/assets.c
CFLAGS ?= -std=gnu99 -D_GNU_SOURCE -O2 -Wall
ifeq ($(SDL),1)
CFLAGS += -DHAVE_SDL2 $(shell pkg-config --cflags sdl2)
LIBS += $(shell pkg-config --libs sdl2)
endif
dw3-launcher: $(SRC) *.h assets/assets.h stb_truetype.h
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LIBS) -lm

clean:
	rm -f dw3-launcher
