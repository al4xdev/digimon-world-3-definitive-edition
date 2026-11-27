The licences of what this package contains.

  dw2003recomp.txt        dw2003recomp: the launcher, the PC port's runtime and the game's decompiled C
                          (https://github.com/gascarcella/dw2003recomp)
  SDL3.txt                SDL 3 (zlib licence), linked statically into both programs (dw2003.exe and
                          dw2003-launcher.exe)
  LiberationSans.txt      the launcher's font, Liberation Sans Bold (SIL Open Font License 1.1), embedded in
                          dw2003-launcher.exe
  stb_truetype.txt        stb_truetype (MIT or public domain), the launcher's font rasterizer, compiled into it
  miniz.txt               miniz (MIT), the updater's unzip, compiled into dw2003-updater.exe
  cJSON.txt               cJSON (MIT), the game's reader and writer of its expansion save, compiled into dw2003.exe
  llvm.txt                the LLVM project (Apache 2.0 with the LLVM exception): the parts of its runtime linked
                          statically into both programs by llvm-mingw (compiler-rt's builtins, libunwind)
                          (https://github.com/mstorsjo/llvm-mingw)
  mingw-w64-runtime.txt   the mingw-w64 runtime (its own notices), linked statically into both programs
                          (https://www.mingw-w64.org/)
  winpthreads.txt         mingw-w64's winpthreads (MIT and BSD), linked statically into both programs

The Windows C runtime itself (the UCRT) is part of Windows and is not included.
No game data is included: the game is read from your own disc image of Digimon World 2003 (PS1, Europe).
