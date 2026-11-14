# Launcher Console-First em C — Digimon World 3: Definitive Edition

## 1. Objetivo e Visão de Produto

Substituir o menu atual (`handheld/dw3-menu.py`) — e unificar a experiência de entrada para **portáteis (`armhf`/`aarch64`), Linux PC e Windows** — por um launcher nativo em **C99**, leve (< 2 MB de binário, < 20 MB de RAM, abertura instantânea) e **100% focado em controle**, com estética inspirada no **Xbox Clássico + PortMaster + Digimon Digital World**:

- **Zero cara de "emulador/desktop" e sem `cover.png` genérico:** como é um launcher dedicado a **um único jogo**, a Home é uma **tela única estilizada para Digimon** (fundo Digital World com profundidade/grade sutil, cabeçalho com emblema do jogo, menu limpo com ícones, barra de descrição contextual e rodapé com pílulas de botões).
- **Experiência unificada sem tela preta (com Minigame de Loading Hardcore):** ao apertar **PLAY**, o launcher continua na tela rodando um **minigame estilo Chrome Dino com os 3 Digimons correndo juntos** (Kumamon, Guilmon e Agumon, controlados individualmente pelos botões **Esquerda / Cima / Direita**) enquanto o `dw2003` verifica o disco e inicializa em segundo plano; só entrega a tela ao jogo quando chegar `port: start:`.
- **Enxuto para a Definitive Edition:** remove opções redundantes/legadas (`VIDEO`, `SAVE ANYWHERE`, `CONTROL STYLE`, `BATTLE MENU: ENABLED ALWAYS`), consolida tudo de combate em **`BATTLE HUD`** (incluindo o 7º botão `INSPECT`), fixa **`BATTLE TEXT`** no padrão **`SKIP`** e adota a rotação moderna (**Triângulo ↔ Bola** + rotação dos glifos) como padrão único em **`CONTROLS`**.

---

## 2. Identidade Visual, UI/UX e Ativos (Ícones & Sprites)

### 2.1 Paleta e Linguagem Visual (Xbox Clássico × Digital World)
- **Resolução base:** `640×480` nativo (perfeito no painel dos RK3326) com escala inteira/limpa em janelas HD no PC/Windows.
- **Cores:**
  - Fundo principal: `#071019` → `#0C1B2A` (gradiente radial + grade digital/circuito sutil estilo Digivice/Digital World)
  - Painéis e cartões: `#102436` com borda `#1F435E` e raio arredondado (`8 px`)
  - Item selecionado (foco estilo Xbox): faixa iluminada `#15394A` + barra lateral verde/ciano `#40FF80` / `#00E5C3` + texto `#FFCC33`
  - Ligado (`ON`): `#40FF80` (verde esmeralda) · Desligado (`off`): `#6E8494`
  - Conflito / Duplicado / Hardware incompatível: `#FF4D4D` (vermelho vivo com ícone de alerta)
  - Textos principais: `#E6F1F5` · Notas/rodapé: `#8BA3B5`
- **Tipografia:** 1 fonte TTF geométrica/legível embutida via `stb_truetype.h` (sem depender de `SDL2_ttf`).

### 2.2 Catálogo de Ícones a Incluir (`assets/icons/` → embutidos/empacotados em PNG ou RGBA)
Todos desenhados em SVG limpo e rasterizados em PNG (ou carregados via `stb_image.h`) para ficarem nítidos em `24×24` e `16×16`:

| Grupo | Ícones | Onde aparecem |
|---|---|---|
| **Menu Principal** | `digi_crest` (emblema cabeçalho), `play` (iniciar), `gamepad_gear` (Game), `crosshair_hud` (Battle HUD), `dpad` (Controls), `led_wave` (LED Bar), `bug_wrench` (Debug), `reset_arrow` (Reset) | Cabeçalho e linhas da Home |
| **Glifos de Controle** | `face_a`, `face_b`, `face_x`, `face_y` (Xbox + Switch), `ps_cross`, `ps_circle`, `ps_square`, `ps_triangle`, `l1`, `r1`, `l2`, `r2`, `start`, `select`, `dpad_lr` | Rodapé de ajuda, `CONTROLS`, dentro da **cruz do `BATTLE HUD`** e legenda do minigame de Loading |
| **Status / UI** | `warn_triangle` (duplicado / hardware incompatível), `check_on`, `fast_forward` (`>>`), `eye_inspect` (Inspect), `sword_fight`, `bolt_tech`, `dna_dv`, `arrows_tag`, `potion_item`, `wing_run`, `obstacle_spike` (obstáculo digital do minigame) | Na cruz do `BATTLE HUD`, alertas de conflito, modais e pista do Loading |

### 2.3 Sprites da Tela de Loading ("Trio Runner" — D-Tector 1-bit com contorno colorido)
- **Sprites escolhidos e validados no preview (`.dumps/sprites/preview_trio.sh`):**
  - **Kumamon** (`24×24` @ `3×` = `72×72`, contorno ciano `#00C3AF`): `k1` (idle) → `k2` (attack/passo) → `k3` (crush/pulo).
  - **Guilmon** (`24×24` @ `3×` = `72×72`, contorno laranja `#FF8C40`): `g1` (idle) → `g2` (passo) → `g3` (lean) → `g4` (`^_^` pulo).
  - **Agumon** (`16×16` @ `4×` = `64×64` para igualar a altura dos outros dois, contorno amarelo-ouro `#FFCC33`): `a2` (walk1) → `a3` (walk2) → `a4` (`^_^` pulo) → `a1` (roar).
- **Renderização:** pixel art 1-bit convertida para bitmap/mask em C (pesa alguns KB embutido direto no binário!), fundo limpo por flood-fill, espelhada para correr para a direita, com contorno colorido para leitura perfeita sobre o fundo escuro.

---

## 3. Desenhos das Telas (Wireframes UI/UX 640×480)

### Tela 1 — Home (`main`)
**Tela única dedicada e estilizada para Digimon** (sem caixa lateral de `cover.png`): fundo Digital World com emblema/arte sutil, lista centralizada/limpa com ícones, faixa de descrição dinâmica abaixo e rodapé com pílulas de botões.

```text
+==========================================================================+
|  [DIGI-CREST]   DIGIMON WORLD 3 : DEFINITIVE EDITION            [v3.x]   |
+==========================================================================+
|                                                                          |
|     > [>]  PLAY                                                          |
|       [*]  GAME                                     SETTINGS             |
|       [+]  BATTLE HUD                               CUSTOMIZE            |
|       [D]  CONTROLS                                 REMAP                |
|       [~]  LED BAR                                  ON                   |
|       [!]  DEBUG                                    SETTINGS             |
|       [R]  RESET SETTINGS                           ASK                  |
|                                                                          |
|  ----------------------------------------------------------------------  |
|  [i] Start Digimon World 3 with your saved settings. (Disc: dw2003.cue)  |
+==========================================================================+
|  [A] Select    [START] Play                        [SELECT+START] Quit   |
+==========================================================================+
```
*(Alterações vs. atual: sem `cover.png`, sem `VIDEO` e sem `BATTLE MENU` duplicado; `PLAY` em destaque e descrição contextual na barra inferior.)*

---

### Tela 2 — `GAME`
Focada apenas nas opções de grinding e comportamento de texto escolhidas pelo jogador.

```text
+==========================================================================+
|  DIGIMON WORLD 3  >  GAME                                                |
+==========================================================================+
|                                                                          |
|  > [>>] FAST-FORWARD                 [ ON  ·  8X      ]                  |
|    [..] SKIP DIALOGUES               [ off            ]                  |
|    [⚔ ] SKIP BATTLE ANIMS            [ off            ]                  |
|         └─ HIT REACTION              [ off            ]                  |
|    [💬] BATTLE TEXT                  [ SKIP           ]  <- padrao SKIP  |
|                                                                          |
|  ----------------------------------------------------------------------  |
|  [i] Skip battle message boxes automatically so combat flows into the    |
|      action. Recommended: SKIP.                                          |
+--------------------------------------------------------------------------+
|  [A] Toggle / Cycle    [</>] Change    [B] Back    [START] Play          |
+==========================================================================+
```
*(Removidos: `SAVE ANYWHERE` — regra fixa da Definitive Edition — e `BATTLE MENU / REMEMBER CURSOR / BUFFER INPUT`, que migram para `BATTLE HUD`.)*

---

### Tela 3 — `BATTLE HUD` *(unificada, com cruz interativa + `INSPECT` + trava anti-duplicado)*
Metade superior desenha a **cruz real do combate** (exatamente como o jogador a vê na batalha, usando os glifos escolhidos em `CONTROLS`); metade inferior lista os 7 botões + opções da HUD.

```text
+==========================================================================+
|  DIGIMON WORLD 3  >  BATTLE HUD                                          |
+==========================================================================+
|   +------------------------------------------------------------------+   |
|   |        [L1] TECH           [Y] DIGIVOLVE           [R1] ITEM     |   |
|   |                   [X] RUN                 [B] FIGHT              |   |
|   |        [L2] INSPECT        [A] TAG                               |   |
|   +------------------------------------------------------------------+   |
|                                                                          |
|  > [⚔] FIGHT            (B)  EAST     |  [!] DIGIVOLVE     (Y) NORTH     |
|    [⚡] TECH             (L1) L1       |  [⇄] TAG           (A) SOUTH     |
|    [🎒] ITEM             (R1) R1       |  [🪶] RUN           (X) WEST      |
|    [👁] INSPECT          (L2) L2       |                                  |
|  ----------------------------------------------------------------------  |
|    REMEMBER CURSOR     [ ON  ]        |  BUFFER INPUT      [ SHORT ]     |
+--------------------------------------------------------------------------+
|  (!) DUPLICATE BUTTONS ON [TECH, FIGHT] — FIX BEFORE SAVING  (se confl.) |
|  [A] Rebind   [</>] Cycle   [B] Save & Back                              |
+==========================================================================+
```
**Regras obrigatórias desta tela:**
1. **`INSPECT` configurável:** elevar `PORT_MOD_MAX_OPTIONS` em `psxstack/include/psxstack/mods.h` (de `8` para `12`) e registrar `inspect` em `port/mods/battle_menu/mod.json` e `port/game/game_mods.c` (mantendo fallback automático `L2 -> R2 -> ...` se ausente em `settings.json` antigo).
2. **Sem auto-swap silencioso:** alterar uma ação **permite duplicidade temporária** durante a edição para facilitar trocas encadeadas.
3. **Destaque + Bloqueio ao sair:** linhas e células da cruz em conflito ficam **vermelhas** com ícone de alerta; apertar `B` (ou `START`) **não sai nem salva** enquanto houver duplicado, exibindo o aviso no rodapé.
4. **Descarte em saída forçada:** se o usuário fechar à força (`Select+Start` ou fechar janela), as alterações em edição são **descartadas** sem escrever `settings.json`.

---

### Tela 4 — `CONTROLS` *(rotação moderna Bola ↔ Triângulo + glifos rotacionados juntos)*

```text
+==========================================================================+
|  DIGIMON WORLD 3  >  CONTROLS                                            |
+==========================================================================+
|                                                                          |
|  > BUTTON GLYPHS       < XBOX (A B X Y) >                                |
|    ------------------------------------------------------------------    |
|    CONFIRM             SOUTH  ─────────────────────────  [ A ]           |
|    BACK                EAST   ─────────────────────────  [ B ]           |
|    MENU                NORTH  ─────────────────────────  [ Y ]           |
|    CARD                WEST   ─────────────────────────  [ X ]           |
|    L1                  LEFT SHOULDER  ─────────────────  [ L1 ]          |
|    R1                  RIGHT SHOULDER ─────────────────  [ R1 ]          |
|    START               START  ─────────────────────────  [ START ]       |
|    SELECT              SELECT ─────────────────────────  [ SELECT ]      |
|                                                                          |
+--------------------------------------------------------------------------+
|  (!) DUPLICATE BUTTONS — FIX BEFORE SAVING                   (se confl.) |
|  [A] Press to Rebind   [</>] Switch Glyphs   [B] Save & Back             |
+==========================================================================+
```
**Regras obrigatórias desta tela (consistência total de rotação + glifos):**
- **Rotação padrão única (sem `CONTROL STYLE`):** `pad.map` já nasce com **Bola ↔ Triângulo rotacionado** junto com os **glifos rotacionados**, de modo que o jogador **esquece completamente que Triângulo voltava no PS1**:
  - `CONFIRM` (ação `cross` do jogo) → posição física `SOUTH` (glifo `A` no Xbox / `✕` no PS1 / `B` no Switch)
  - `BACK` (ação `triangle` do jogo) → posição física `EAST` (glifo `B` no Xbox / `◯` no PS1 / `A` no Switch — logo todo texto in-game de "voltar" mostra `B` ou `◯`, nunca `△`!)
  - `MENU` (ação `circle` do jogo) → posição física `NORTH` (glifo `Y` no Xbox / `△` no PS1 / `X` no Switch)
  - `CARD` (ação `square` do jogo) → posição física `WEST` (glifo `X` no Xbox / `▢` no PS1 / `Y` no Switch)
- **`BUTTON GLYPHS`:** alterna os rótulos mostrados nos diálogos e na HUD entre **`XBOX`** (`A/B/X/Y`), **`NINTENDO`** (`B/A/Y/X`), **`PS1`** (`✕/◯/▢/△`) e **`CUSTOM`** (ciclo individual por botão).
- **Mesma regra anti-duplicado:** permite duplicar na edição, destaca conflito em vermelho, bloqueia saída até resolver e descarta em saída forçada (`Select+Start`).

---

### Tela 5 — `DEBUG` *(mantida para suporte PortMaster)*

```text
+==========================================================================+
|  DIGIMON WORLD 3  >  DEBUG                                               |
+==========================================================================+
|                                                                          |
|  > LOGS                [ OFF  |  SOFT  |  HEAVY ]                        |
|    SAVE LOGS           [ CREATE dw3-logs-YYYYMMDD.zip ]                  |
|    CLEAN LOGS          [ ASK BEFORE DELETING ]                           |
|                                                                          |
|  Status: Saved dw3-logs-20261114-090000.zip                              |
+--------------------------------------------------------------------------+
|  [A] Run / Cycle    [B] Back    [START] Play                             |
+==========================================================================+
```

---

### Tela 6 — `LED BAR` *(detecção de hardware + 2 telas de confirmação quando incompatível)*

1. **Detecção automática antes de abrir/instalar:** o launcher verifica se o console atual é compatível com a barra de LED UART (`XF35H` via `/proc/device-tree/model` / `/sys/firmware/devicetree/base/model` + `/dev/ttyS2`).
2. **Quando compatível (XF35H):** mostra o aviso normal de instalação na 1ª vez e depois vira toggle `ON / off`.
3. **Quando NÃO compatível (outro portátil, desktop Linux ou Windows):**
   - **Não ativa direto:** exibe **aviso de hardware incompatível** e exige **duas telas sequenciais de confirmação** caso o usuário ainda insista em continuar:

```text
+==========================================================================+
|  DIGIMON WORLD 3  >  LED BAR  [CONFIRMACAO 1 / 2]                        |
+==========================================================================+
|  +--------------------------------------------------------------------+  |
|  |  [!] HARDWARE INCOMPATIVEL DETECTADO                               |  |
|  |                                                                    |  |
|  |  Este aparelho nao e um XF35H (hardware de LED UART ausente ou     |  |
|  |  nao reconhecido). Ativar o LED BAR aqui nao tera efeito visual    |  |
|  |  e pode enviar bytes para outra porta serial /dev/ttyS2.           |  |
|  +--------------------------------------------------------------------+  |
|                                                                          |
|  > [B] VOLTAR (RECOMENDADO)        [A] CONTINUAR MESMO ASSIM (1/2)       |
+==========================================================================+
```
*(Se apertar `A`, abre a **Tela de Confirmação 2 / 2** exigindo segunda confirmação explícita de risco antes de prosseguir.)*
```text
+==========================================================================+
|  DIGIMON WORLD 3  >  LED BAR  [CONFIRMACAO 2 / 2]                        |
+==========================================================================+
|  +--------------------------------------------------------------------+  |
|  |  [!!] CONFIRMACAO FINAL DE RISCO                                   |  |
|  |                                                                    |  |
|  |  Tem certeza absoluta de que deseja instalar/ativar o daemon do    |  |
|  |  LED BAR em hardware nao compativel?                               |  |
|  +--------------------------------------------------------------------+  |
|                                                                          |
|  > [B] CANCELAR                    [A] SIM, TENHO CERTEZA (INSTALAR)     |
+==========================================================================+
```

---

### Telas 7 & 8 — Confirmações (`RESET SETTINGS?` e `CLEAN LOGS AND DUMPS?`)

```text
+--------------------------------------------------------------------------+
|           +--------------------------------------------------+           |
|           |  [!] RESET SETTINGS?                             |           |
|           |                                                  |           |
|           |  Restore all defaults (Battle Text: SKIP,        |           |
|           |  Controls: Modern A/B, Battle HUD defaults,      |           |
|           |  LED Bar ON if supported). Cards untouched.      |           |
|           |                                                  |           |
|           |     [A] YES, RESET        [B] NO, KEEP           |           |
|           +--------------------------------------------------+           |
+--------------------------------------------------------------------------+
```
*(Desenhadas como **modal centralizado** por cima da tela anterior esmaecida.)*

---

### Tela 9 — `LOADING`: Minigame Hardcore "Trio Runner" *(estilo Chrome Dino com os 3 Digimons)*

```text
+==========================================================================+
|  DIGIMON WORLD 3 : LOADING                       SCORE 0042  ·  BEST 120 |
+==========================================================================+
|                                                                          |
|              [KUMAMON]       [GUILMON]       [AGUMON]                    |
|               (ciano)        (laranja)       (ouro)                      |
|               (walk)          (jump!)        (walk)                      |
|                                  ▲                                       |
|   ==============================[▓]===================================   |
|   ..  .    ...    ..   .     ...    ..    .    ...    ..    .    ...     |
|                                                                          |
|   JUMP:  [X / WEST] KUMAMON   ·   [Y / NORTH] GUILMON   ·   [B / EAST] AGUMON
|                                                                          |
|                    [████████████░░░░░░░░░░]  58%                         |
|                    VERIFYING DISC IMAGE...                               |
+==========================================================================+
```

**Mecânica do Minigame Hardcore na tela de Loading:**
1. **Mapeamento 1-pra-1 (no referencial moderno/físico, da esquerda para a direita):**
   - **Esquerda (`West` — `▢` / `X` no Xbox / `Y` no Switch):** pula o **Kumamon** (posição esquerda da fila).
   - **Cima (`North` — `△` / `Y` no Xbox / `X` no Switch):** pula o **Guilmon** (posição do meio).
   - **Direita (`East` — `◯` / `B` no Xbox / `A` no Switch):** pula o **Agumon** (posição da frente).
   - *(Os rótulos no rodapé da pista usam exatamente o conjunto de glifos escolhido em `CONTROLS`!)*
2. **Jogabilidade hardcore:**
   - Obstáculos digitais (espinhos / blocos firewall simples ou duplos, ou até um obstáculo voador em altura média mais pra frente) vêm da direita para a esquerda em velocidade crescente.
   - O jogador precisa apertar os 3 botões no ritmo certo conforme o obstáculo passa por **Agumon → Guilmon → Kumamon**!
   - **Colisão:** se algum Digimon bater no obstáculo, faz flash vermelho, zera o `SCORE` atual (mantendo o `BEST` da sessão/arquivo) e continua sem nunca atrapalhar o carregamento real do jogo.
3. **Transição limpa sem tela preta:**
   - O loop roda em ~30–60 FPS não-bloqueante enquanto o `dw2003` roda em processo filho com pipe de `stdout/stderr`.
   - Assim que o `dw2003` emitir `port: start:` (ou se estiver pronto), o launcher fecha seu dispositivo de input/framebuffer imediatamente e entrega a tela direto ao primeiro frame do jogo (e se `dw2003` falhar no boot com erro != 0, mostra o erro na tela em vez de fechar mudo).
   - *(Na v1 colocamos a versão enxuta do runner com pulo individual + 1 tipo de obstáculo + score/best, e depois refinamos juntos velocidade, padrões de obstáculos e efeitos visuais!)*

---

## 4. Arquitetura Técnica (C99 Multi-Console + PC)

1. **Estrutura de código sugerida (`launcher/` ou `handheld/launcher_c/`):**
   - `ui_draw.c/.h`: buffer RGBA 32-bit em memória (`640×480` ou escalado), primitivas (retângulos arredondados, gradientes, alpha-blend, recorte), texto TTF via `stb_truetype.h`, imagens PNG via `stb_image.h` e blitter 1-bit com contorno colorido para os sprites D-Tector.
   - `loading_game.c/.h`: estado do minigame *Trio Runner* (física de pulo dos 3 Digimons, spawn de obstáculos, colisão, score/best, sync com progresso do pipe).
   - `platform_fbdev.c`: backend leve para portáteis (`/dev/fb0` + `/dev/input/event*` não-bloqueante), zero dependência de X11/Wayland/SDL_ttf.
   - `platform_sdl2.c`: backend para **Linux PC, Windows `.exe` e CFWs Wayland/KMS** (janela `SDL2` + `SDL_GameController` + `SDL_Texture`), compilável com o mesmo código C.
   - `config.c`: leitura/escrita atômica de `settings.json` e `pad.map` usando `psxstack/runtime/json.c`, validação de duplicados com cópia temporária (*draft* em memória, só persiste no `Save & Back` ou `Play`).
   - `runner.c`: dispara `./dw2003` com pipe na saída, mantém o minigame de *Loading* até `port: start:`, trata erro/crash caso `dw2003` saia antes com código != 0.
2. **Compatibilidade PortMaster & Multi-CFW:**
   - `Digimon World 3.sh` continua responsável por detectar ambiente (`control.txt`, `fbdev` vs `sdl2`, `cpu_part 0xd04 -> gles`) e chamar o novo binário `dw3-launcher` (com fallback limpo caso `/dev/fb0` não exista, abrindo via SDL2).

---

## 5. Fases de Execução

- **Fase 1 — Ajustes de contrato e padrões (base):**
  1. Elevar `PORT_MOD_MAX_OPTIONS` no `psxstack` e tornar `inspect` uma opção real de `battle_menu` (`mod.json` + `port/game/game_mods.c`).
  2. Atualizar os padrões: `battle_text.mode = "skip"`, `battle_menu` e rotação moderna padrão (**Bola ↔ Triângulo** + glifos rotacionados).
  3. Converter `a1..a4`, `g1..g4` e `k1..k3` em bitmaps limpos em C + preparar 1 fonte TTF e os ícones em `assets/`.
- **Fase 2 — Novo Launcher em C (`dw3-launcher`):**
  1. Implementar `ui_draw` (`stb_truetype` + `stb_image`) + backends `fbdev` e `sdl2`.
  2. Implementar todas as 9 telas acima (Home unificada sem cover, `BATTLE HUD` com cruz + `INSPECT`, `CONTROLS` com rotação moderna e glifos, `LED BAR` com detecção de compatibilidade + 2 confirmações, `DEBUG`, modais, validação estrita de duplicados com descarte em saída forçada).
  3. Implementar a **Tela 9 (`LOADING` — Minigame Hardcore "Trio Runner")** com pulo independente nos 3 botões (`West / North / East`), obstáculos, score e transição limpa no `port: start:`.
- **Fase 3 — Empacotamento e Validação:**
  1. Integrar no `tools/build_arm.sh` (cross-compile `armhf` e futuramente `aarch64`), `portmaster/make_release.sh` e build Linux/Windows.
  2. Testar no console portátil real (`192.168.0.185` EmuELEC) validando controles, `BATTLE HUD`, `LED BAR` (incluindo fluxo de hardware incompatível em 2 etapas), `DEBUG`, conflito de botões, o minigame de loading e a transição sem tela preta.
