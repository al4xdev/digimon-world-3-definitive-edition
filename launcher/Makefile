# A desktop checkout's quick build of the launcher (Linux, SDL2): make [SDL=1]; ./dw3-launcher --sdl finds
# build/port-sdl/dw2003. The releases build these same sources: CMakeLists.txt (Linux AppImage and Windows, SDL3),
# .ci-cd/portmaster/build_arm.sh (PortMaster), android/CMakeLists.txt (Android).
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
