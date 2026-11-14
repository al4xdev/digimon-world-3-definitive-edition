cmake_minimum_required(VERSION 3.20)
project(dw2003_android C)

set(SDL2_ANDROID_DIR "/opt/SDL2" CACHE PATH "Path to unpacked SDL2 source tree")
if(NOT EXISTS "${SDL2_ANDROID_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "SDL2 source tree not found at ${SDL2_ANDROID_DIR}")
endif()

# Provide <SDL2/SDL.h> symlink without changing any upstream #include directives.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sdl2-hdrs")
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/sdl2-hdrs/SDL2")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink
        "${SDL2_ANDROID_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/sdl2-hdrs/SDL2")
endif()

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(HIDAPI OFF CACHE BOOL "" FORCE)
add_subdirectory("${SDL2_ANDROID_DIR}" sdl2_build EXCLUDE_FROM_ALL)

set(PSXSTACK_SDL2 ON CACHE BOOL "" FORCE)
set(PSXSTACK_SDL2_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/sdl2-hdrs" CACHE PATH "" FORCE)
set(PSXSTACK_OBJCOPY "${CMAKE_OBJCOPY}" CACHE FILEPATH "" FORCE)
set(PSXSTACK_OBJDUMP "${CMAKE_OBJDUMP}" CACHE FILEPATH "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../port" dw3_port)
