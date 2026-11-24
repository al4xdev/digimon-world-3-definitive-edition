# A desktop checkout's quick build of the launcher (Linux, SDL2): make [SDL=1]; ./dw3-launcher --sdl finds
# build/port-sdl/dw2003. The releases build these same sources: CMakeLists.txt (Linux AppImage and Windows, SDL3),
# .ci-cd/portmaster/build_arm.sh (PortMaster), android/CMakeLists.txt (Android).
SRC = main.c ui.c plat.c cfg.c loading.c cloud.c sha256.c update.c assets/assets.c
CFLAGS ?= -std=gnu99 -D_GNU_SOURCE -O2 -Wall
TARGET ?= pc
ifeq ($(TARGET),portmaster)
CFLAGS += -DDW3_TARGET_PORTMASTER
else
DW3_STAMP := $(shell git describe --tags --match "v[0-9]*.[0-9]*.[0-9]*" --always --dirty 2>/dev/null || echo dev)
SRC += selfupdate.c
CFLAGS += -DDW3_TARGET_PC -DDW3_LAUNCHER_VERSION=\"dev\" -DDW3_LAUNCHER_STAMP=\"$(DW3_STAMP)\"
endif
ifeq ($(SDL),1)
CFLAGS += -DHAVE_SDL2 $(shell pkg-config --cflags sdl2)
LIBS += $(shell pkg-config --libs sdl2)
endif
dw3-launcher: $(SRC) *.h assets/assets.h stb_truetype.h
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LIBS) -lm -lpthread

dw3-updater: updater.c sha256.c third_party/miniz/miniz.c *.h
	$(CC) $(CFLAGS) -DMINIZ_NO_DEFLATE_APIS -DMINIZ_NO_TIME updater.c sha256.c third_party/miniz/miniz.c -o $@

clean:
	rm -f dw3-launcher dw3-updater
