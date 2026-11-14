# Android Minimal Port (`arm64-v8a` + Touchpad Overlay + Disc Picker fora do PortMaster)

> **Nota de fluxo para o próximo agente:** execute obrigatoriamente a **Fase 0 (Descoberta e Mapeamento)** antes de propor/alterar código ou abrir PR. Este plano é intencionalmente enxuto: focar **apenas em Android recente (`arm64-v8a`, API 30+)** com **Vulkan (`render_gpu.c`)** e **touchpad virtual simples**, sem multiplicar arquiteturas (`armeabi-v7a`, `x86`) nem gastar ciclos com remapeamento complexo de controles físicos.

---

## 1. Objetivo e Escopo

1. **Alvo único no Android:** `arm64-v8a` (Android 11+ / API 30+), renderização via **SDL3 + Vulkan** (`psxstack/runtime/render_gpu.c`) e áudio via **SDL3 Audio**. Sem software-only legacy nem múltiplas ABIs.
2. **Controles (abordagem minimalista):**
   - **Touchpad virtual** desenhado como **camada externa simples em Kotlin/Views no wrapper Android/Gradle** (ou camada SDL sobreposta se mais simples no mapeamento), enviando eventos padrão de teclado/botão virtual (`SDL_SendKeyboardKey` / JNI → fila de input) sem precisar mexer na lógica do jogo.
   - **Controles físicos:** sem suporte customizado/remapeamento complexo na v1 — no máximo aproveitar pass-through padrão que o Android/SDL3 já expõe nativamente (`GameController` padrão), se vier de graça.
   - **Opção exclusiva do Android no launcher (`#ifdef __ANDROID__`):** toggle **"Show Touch Controls" / "Ocultar touchpad na tela"** que **só aparece no build Android**, permitindo esconder o overlay caso o jogador conecte um controle Bluetooth/USB-C ou esteja gravando.
3. **Seleção manual de caminho do disco fora do PortMaster (Linux standalone + Android):**
   - Quando o launcher detectar que **não veio do PortMaster** (ausência de `control.txt` / ambiente PortMaster ou nenhum disco encontrado em `game/`), liberar uma opção direta para **escolher o caminho do `.cue`/`.bin` manualmente** (e persistir em `settings.json`, sem depender da estrutura rígida do PortMaster).
   - No Android (Scoped Storage), permitir selecionar via SAF (*Storage Access Framework* / `ACTION_OPEN_DOCUMENT`) copiando ou referenciando para a pasta privada do app (`SDL_GetAndroidExternalStoragePath()` / `GetPrefPath`) onde `fopen()` nativo do `psxstack/runtime/disc.c` consiga ler `.bin` + `.cue` sem dor de cabeça.

---

## 2. Fase 0 — Descoberta e Mapeamento (obrigatória antes de codar)

Antes de criar arquivos Gradle ou mexer em C/runtime, mapear e documentar em notas técnicas:

1. **Estado atual dos dois launchers e convergência:**
   - Verificar como `launcher/` (`dw3-launcher`, C99, onde já existe `is_pc` / `disc_path`) e `psxstack/launcher/` (`dw2003-launcher`, C++17 ImGui) estarão após o fechamento de `feat/girls-power` e `widescreen-field`.
   - Decidir qual launcher será empacotado no Android (ou se `dw3-launcher` C99 + SDL3/SDL2 ou `dw2003-launcher` ImGui + SDL3 é a base única fora do PortMaster).
2. **Audit do `psxstack` para NDK `arm64-v8a` (Bionic libc):**
   - `runtime/fiber.c`: confirmar que `port_fiber_swap_` (`__aarch64__`) compila e roda no Clang/NDK Android (alinhamento de stack, ausência de dependência em `ucontext`).
   - `runtime/crash.c`: mapear o que usa `ucontext_t` / `dladdr` / `backtrace` no glibc vs o que o Bionic do Android oferece (desabilitar ou adaptar `crash_write_registers` sob `#ifdef __ANDROID__`).
   - `runtime/overlay.c`, `runtime/platform.c`, `runtime/memcard.c`, `runtime/settings.c`, `runtime/disc.c`: listar toda chamada POSIX/Linux que difere no Android (caminhos `HOME`/`XDG_DATA_HOME`, `argv`/shared library `SDL_main`, shaders SPIR-V empacotados em assets/memória).
   - `cmake/psxstack.cmake`: mapear como compilar `dw2003` como `SHARED` (`libmain.so`) sob `CMAKE_SYSTEM_NAME=Android` (`ANDROID_ABI=arm64-v8a`), incluindo a compilação offline dos `.hlsl` via `dxc` no host antes do empacotamento do `.apk`.
3. **Audit de Disco / SAF + Fallback sem PortMaster:**
   - Mapear em `launcher/cfg.c` (ou `psxstack/launcher/src/disc.cpp`) como detectar ambiente PortMaster vs standalone Linux/Android e onde expor a tela/ação de **Escolher caminho do jogo/disco** na mão.
   - Definir o fluxo de importação `.bin` + `.cue` no Android (lembrando que o `.cue` referencia o `.bin` na mesma pasta — no Android, ou o usuário escolhe a pasta via `OPEN_DOCUMENT_TREE` / copia ambos para `filesDir`, ou seleciona um `.bin`/`.iso`/pasta).
4. **Mapeamento do Touchpad em Kotlin/Gradle:**
   - Definir layout mínimo dos botões PS1 sobre a `SDLActivity` (D-pad à esquerda, `Cross`/`Circle`/`Square`/`Triangle` à direita, `L1`/`R1`/`L2`/`R2` nos cantos superiores, `Start`/`Select` no topo/base), ponte JNI ↔ SDL3 (ou `KeyEvent` / `MotionEvent` padrão consumido pelo `runtime/input.c`), e flag de visibilidade lida das configurações do launcher (`#ifdef __ANDROID__`).

---

## 3. Fases de Implementação (após aprovação do mapeamento)

- **Fase 1 — Build `libmain.so` (`arm64-v8a`) no NDK + `psxstack`:**
  - Ajustes `#ifdef __ANDROID__` em `psxstack/runtime/` (`platform.c`, `crash.c`, `main.c` `SDL_main`, caminhos de assets/shaders).
  - Target Android `arm64-v8a` no CMake + esqueleto mínimo `android/` (Gradle + Kotlin `DW3Activity : SDLActivity`).
  - Gate: `libmain.so` compilado com NDK + `tests/port/run.py` / `hello` sem regressões no Linux/Windows.
- **Fase 2 — Seleção manual de disco (não-PortMaster) + Importação SAF no Android:**
  - No launcher: detecção de ambiente não-PortMaster + browser/seletor manual de caminho para `.cue`/`.bin` no Linux e chamada ao seletor nativo/SAF no Android.
  - Persistência do caminho em `settings.json` (`disc.path`).
- **Fase 3 — Camada Touchpad em Kotlin + Toggle Android-only:**
  - Overlay transparente sobre a surface SDL em Kotlin (`TouchOverlayView`) com D-pad + botões PS1 básicos enviando inputs padrão.
  - Toggle no launcher visível **somente** quando `#ifdef __ANDROID__` para mostrar/ocultar o touchpad (útil com controle Bluetooth/USB ou gravação).
  - Teste em aparelho Android real (`adb install` + `adb logcat`): boot, seleção do disco, entrada no jogo, batalha com Vulkan + widescreen, toggle do touchpad.
