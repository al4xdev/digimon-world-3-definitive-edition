# The PC launcher, dw2003-launcher: the release's on Windows (cross-built with psxstack/cmake/windows-x86_64.cmake) and
# in the AppImage. These sources, SDL3 linked statically, -DDW3_TARGET_PC. PortMaster and Android build the same sources
# their own way (.ci-cd/portmaster/build_arm.sh, android/CMakeLists.txt); the Makefile is a desktop checkout's SDL2 build.
#
#   cmake -S launcher -B build/launcher -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL3_DIR=tools/sdl3-desktop/lib/cmake/SDL3
#   build/launcher/dw2003-launcher --self-test build/launcher-test
cmake_minimum_required(VERSION 3.20)
project(dw2003_launcher C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON) # gnu99, as the handheld's build

find_package(SDL3 3.2 CONFIG REQUIRED)
message(STATUS "SDL3 ${SDL3_VERSION}: ${SDL3_DIR}")

add_executable(dw2003-launcher main.c ui.c plat.c cfg.c loading.c cloud.c sha256.c update.c assets/assets.c)

# The running build's version for the update notice (update.h): a release's X.Y.Z, from the tag the packaging scripts
# export as PSXSTACK_VERSION; any other build is "dev", which never asks.
set(DW3_LAUNCHER_VERSION "dev")
if("$ENV{PSXSTACK_VERSION}" MATCHES "^v?([0-9]+[.][0-9]+[.][0-9]+)$")
    set(DW3_LAUNCHER_VERSION "${CMAKE_MATCH_1}")
endif()
if(DEFINED ENV{PSXSTACK_VERSION} AND NOT "$ENV{PSXSTACK_VERSION}" STREQUAL "")
    set(DW3_LAUNCHER_STAMP "$ENV{PSXSTACK_VERSION}")
else()
    execute_process(
        COMMAND git describe --tags --match "v[0-9]*.[0-9]*.[0-9]*" --always --dirty
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE DW3_LAUNCHER_STAMP
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT DW3_LAUNCHER_STAMP)
        set(DW3_LAUNCHER_STAMP "${DW3_LAUNCHER_VERSION}")
    endif()
endif()
message(STATUS "launcher version: ${DW3_LAUNCHER_VERSION} (${DW3_LAUNCHER_STAMP})")
target_compile_definitions(dw2003-launcher PRIVATE DW3_TARGET_PC HAVE_SDL3 _GNU_SOURCE DW3_LAUNCHER_VERSION="${DW3_LAUNCHER_VERSION}" DW3_LAUNCHER_STAMP="${DW3_LAUNCHER_STAMP}")
target_compile_options(dw2003-launcher PRIVATE -Wall)
if(TARGET SDL3::SDL3-static)
    target_link_libraries(dw2003-launcher PRIVATE SDL3::SDL3-static)
else()
    target_link_libraries(dw2003-launcher PRIVATE SDL3::SDL3)
endif()

if(WIN32)
    # A GUI-subsystem program (no console window behind it; mingw's startup still calls main), with the manifest resource
    # (windows/: the UTF-8 code page, long paths, per-monitor DPI awareness). WinHTTP for the cloud sync, comdlg32 for
    # the file dialog. The toolchain file links everything statically and writes a PDB beside the executable.
    enable_language(RC)
    # The resource script is generated: the icon, the manifest and the version information (X.Y.Z of a release as four
    # numbers and as text, else 0.0.0.0 and "dev"), which Explorer, the Start menu and game libraries read.
    if(DW3_LAUNCHER_VERSION MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$")
        string(REPLACE "." "," DW3_RC_NUM "${DW3_LAUNCHER_VERSION},0")
    else()
        set(DW3_RC_NUM "0,0,0,0")
    endif()
    set(DW3_RC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/windows")
    configure_file(windows/dw2003-launcher.rc.in "${CMAKE_CURRENT_BINARY_DIR}/dw2003-launcher.rc" @ONLY)
    target_sources(dw2003-launcher PRIVATE win32.c "${CMAKE_CURRENT_BINARY_DIR}/dw2003-launcher.rc")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/dw2003-launcher.rc" PROPERTIES
        OBJECT_DEPENDS "${DW3_RC_DIR}/dw2003-launcher.manifest;${DW3_RC_DIR}/dw2003.ico")
    target_link_libraries(dw2003-launcher PRIVATE winhttp comdlg32)
    set_target_properties(dw2003-launcher PROPERTIES WIN32_EXECUTABLE ON)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(dw2003-launcher PRIVATE m Threads::Threads)
endif()
