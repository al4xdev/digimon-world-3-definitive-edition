# Desktop build of the standalone C launcher (Linux): make [SDL=1].
# PortMaster armhf builds use .ci-cd/portmaster/build_arm.sh with -DDW3_TARGET_PORTMASTER -DHAVE_SDL2.
# Official Linux/Windows release builds use psxstack/launcher/ via package_appimage.sh / package_windows.sh.
SRC = main.c ui.c plat.c cfg.c loading.c cloud.c sha256.c assets/assets.c
CFLAGS ?= -std=gnu99 -D_GNU_SOURCE -O2 -Wall
TARGET ?= pc
ifeq ($(TARGET),portmaster)
CFLAGS += -DDW3_TARGET_PORTMASTER
else
CFLAGS += -DDW3_TARGET_PC
endif
ifeq ($(SDL),1)
CFLAGS += -DHAVE_SDL2 $(shell pkg-config --cflags sdl2)
LIBS += $(shell pkg-config --libs sdl2)
endif
dw3-launcher: $(SRC) *.h assets/assets.h stb_truetype.h
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LIBS) -lm

clean:
	rm -f dw3-launcher
