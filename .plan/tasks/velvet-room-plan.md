# Sala Velvet (Velvet Server) + porta azul nas pousadas

Handoff/estado para outro harness. Dono pt-BR. Não commitar sem pedido. Scratch só em `.dumps/`.

## Estado (2026-11-16, noite)

**Implementado e compilando** (`build/port`); falta o dono testar no jogo.

| O quê | Onde |
|---|---|
| A sala (modo Velvet do mapa 0x20F) | `port/game/definitive_velvet.c` |
| Gancho do idle do campo (porta + desenho) | `src/fieldstg/fieldstg_80087DB0.c` (`fieldstg_manager_update`, passo RUN/0) |
| Saída do mapa redirecionada | `fieldstg_goto_map_delayed` → `dw3_velvet_leave` |
| Porta de cima fechada | `fieldstg_map_events_check` → `dw3_velvet_event_closed` |
| Setup da sala | `src/wstag/wstag255.c` (fim de `wstag255_setup`) → `dw3_velvet_stage_setup` |
| Comerciante morando na sala | `port/game/definitive_market_screen.c` `traders[]` = `{ 0x20F, 42 }` |
| Flag do recurso | `dw3_feat_velvet` (`DW3_FEAT_VELVET`, padrão ligado; desligado sob `--script`) |
| Falas dos NPCs | `tools/text_override/en.txt` + `pt.txt` (`TALK05 81`, `TALK05 82`) → `port/game/definitive_text.c` / `_pt.inc` |
| Getter dos nós do Guardromon | `port/game/definitive_travel.c` `dw3_travel_node` |
| Título do mapa ("VELVET / SERVER") | `src/fieldstg/fieldstg_80085590.c` (`fieldstg_map_title_create_windows`) → `dw3_velvet_title_window` |

### Como funciona

- **Entrada**: a porta azul (retângulo em pé encostado numa parede) fica na pousada, a poucos passos do
  Guardromon. O jogador pisa no pé da porta (raio 12 px) → `fieldstg_goto_map(0x20F, entry, 215, 335, dir 1)`
  com o som do warp. `vl_from` guarda a pousada de origem.
- **Modo sala**: `vl_room` (RAM) → `wstag255_setup` termina chamando `dw3_velvet_stage_setup`, que troca
  `fieldstg_stage.actors` pelos NPCs, a música e pinta o vazio em azul (gradiente de 16 faixas na camada 0x1000,
  medido do mock do dono).
  **Atenção (erro que já foi cometido):** o campo toca `fieldstg_stage.sound` (a faixa) *depois* de carregar o banco
  de `fieldstg_stage.music`; trocar só `music` muda o banco e a música continua a mesma. A sala usa o par do mapa
  0x25F: `music = 0x3B`, `sound = 0x60EC0000`.
- **NPCs** (todos na sala):
  - O **comerciante do Black Market** atrás do balcão (personagem original, desenhado à mão em 2026-11-16: cabelo
    preto até os ombros, casaco escuro liso, moeda de ouro na mão; um desenho só, 3/4 de frente para baixo-esquerda,
    ele não vira): continua sendo o **ator 42** (é ele que dá a caixa de fala e abre a tela do mercado). O
    `fieldstg_actor_draw` chama `dw3_velvet_actor_art`, que desenha a arte **no lugar** do sprite do ator (mesma
    entrada da ordering table: o jogador passa na frente e atrás dele; a sombra do jogo fica). A arte vive em
    **`.dumps/tasks/velvet/art/v2/hand.txt`** (pixels à mão, um caractere por pixel, paleta de 16; o NPC P068SARM só
    aparece apagado por baixo como guia de pose e tamanho, nenhum pixel do disco entra). `art/v2/hand.py show OUT.png`
    renderiza a revisão (desenho 8x, sobre o guia, ao lado de NPCs do jogo, no bar); `hand.py header
    port/game/definitive_velvet_art.h` gera o header. Respira como os NPCs da cidade (2 quadros, 23 vsyncs cada). O
    port desenha **um quad por corrida de cor** — sem VRAM, sem CLUT, cores exatas; a rota do TIM 4-bit foi tentada e
    **descartada** porque o upload (via `tim.load`) sujava a VRAM do cenário. As artes anteriores (Oikawa, depois o
    vendedor de chapéu) foram reprovadas pelo dono por destoarem do estilo do jogo.
    **Falas dele** (EN+PT-BR, entradas 8, 9, 12 e 13 do `TALK05`): as duas saudações se **alternam a cada visita**; a
    fala de "comprou algo" (12) e a de "saiu sem comprar" (13) aparecem na **visita seguinte** (a tela do mercado não
    tem onde mostrar texto depois de si mesma) e **uma vez por sequência** — depois a saudação volta a alternar.
    Fluxo: falar → fala do Oikawa no diálogo do jogo → (a tela do Black Market abre em seguida) → sair → a próxima
    visita comenta como a anterior terminou. O estado é RAM (`vl_shop_*` em `definitive_velvet.c`).
    **Idiomas**: o override do port só valia para inglês (records_language 1/2) e PT-BR; nas outras línguas o disco
    tem só a letra "a" nesses entries. Agora `dw3_velvet_text_own()` (lista em `definitive_velvet.c`) marca as
    entradas que são **texto do port** e o `def_text_override` serve o texto inglês **em qualquer idioma** (o resto
    continua o texto do disco, como antes). Provado com `records_language = 3` (francês): o balão mostra a fala em
    inglês em vez de "a". **Traduzir** essas 7 falas para FR/IT/DE/ES é a tarefa de tradução geral; PT-BR já tem as
    suas (`pt.txt`)., em **(275,255)**,
    **dir 1** (frente-esquerda, a direção em que o balcão corre — escolhida pelo dono) e **`no_turn = 1`** (não vira ao
    falar: fica na pose do balcão). Tentativa descartada: dar a ele o arquivo do herói (o Girls Power repinta esse
    arquivo para a Kaili, então ficavam *dois* heróis iguais — o dono vetou). — exatamente onde o
    próprio jogo punha o barman (o ator do WSTAG255 está em (275,255)). A caixa de interação de
    `fieldstg_actor_find_at` é `width` de largura e **metade disso de altura** (32 × 16 px, centrada no ator), então
    ele alcança quem está até 24 px abaixo; em (275,244) ficava fora do alcance e o jogador não conseguia falar.
  - **mentor = ator 0x39** (o velhinho dos "Grandpa ..."; vram 5), no palco entre os tubos de órgão, em (318,165)
    dir 1 (2026-11-17; antes era o Kotemon 0x178).
  - **Etemon** (ator 0x61 = 97, vram 4) **de costas para a sala, virado para o piano**, em (352,284) **dir 5**: o
    teclado está para cima e para a direita dele, então é o 5 que aponta para as teclas. Ele vira quando o jogador
    fala (fala: "ouvi essa música em outro jogo").
  - **o porco** (ator 318 = 0x13E, vram 6) em (245,292): é a **única arte do jogo que nenhum mapa usa**; a fala é a
    piada do dono ("alguém me achou numa pilha de brinquedos que ninguém usava"). Entrada 79 do TALK05.
- **Falas**: entradas 81 e 82 do arquivo de falas do bar (`records_language + 0xE8`, `TALK05`), que no disco são
  só a letra "a" e ninguém usa. O port sobrescreve as duas em EN e PT-BR (`tools/gen_text_override.py`).
  O conteúdo do mentor é placeholder (a tarefa do Blast Gauge escreve o de verdade).
- **Saída**: a porta da frente da sala (o spot de saída do próprio bar, em (205,348), tecla de confirmação) volta
  para a pousada de origem, no nó do Guardromon. A outra porta do bar (a da direita, que no jogo leva à pousada
  0x20A) fica **fechada** enquanto a sala roda.
- **HUD do Black Market** (revisada a pedido do dono): a fonte de 14 px do port desenha o **dois-pontos como
  "0"** (glifo extra 7), então "Price: 40000" saía "Price0 40000". As strings do mercado (Give/Get/Held/Cost/Price e
  as quatro linhas de teclas) agora não usam dois-pontos ("Price 40000 Bits   Held 0", "× Buy   △ Back"), em EN e
  PT-BR. **O mesmo defeito atinge outras 30 strings do port** (`× button: ...`, `L1/R1: ...`, `Pity:`, etc.) — fica
  para uma passada própria (ou consertar o glifo na fonte).
- **Título**: ao entrar na sala o cartão do mapa mostra "VELVET / SERVER" nas duas línguas (`def_str_VelvetTitle`/`2`;
  o dono pediu "Velvet Server" em 2026-11-16, os nomes de lugar ficam em inglês no PT-BR); no bar normal continua o
  nome do bar.
- **Alcance da fala**: a caixa do jogo (`fieldstg_actor_find_at`) é 16 x 8 em volta de cada figura, e a fileira da
  frente do balcão deixa o ponto à frente do jogador 12 a 23 px abaixo do comerciante: ninguém alcançava. Quando o jogo
  não acha ninguém, `fieldstg_actor_talk_to` pergunta a `dw3_velvet_talk_find`: o comerciante alcança 30 px para os
  lados, 16 acima e 28 abaixo (a fileira do balcão de x 256 a 300); as outras figuras, 24 x 16; a mais perto ganha.
  Testado (`.dumps/tasks/velvet/talktest4.py`, um processo por ponto): comerciante, mentor, Guilmon do Demon Bell,
  Etemon e o porco respondem.
- **Save state com a loja aberta (consertado 2026-11-16)**: o estado da tela do Black Market (`bm_ui`: aberta, cursor,
  janelas no heap do campo) e o da sala (`vl_room`, `vl_from`, `vl_shop_*`) não iam no save state; carregar um estado
  de antes da loja abrir deixava a tela "aberta" sobre janelas que o heap já não tinha (o jogador travava, o terceiro
  load seguido dava segfault). Agora `def_bm_savestate` (definitive_market_screen.c) e `dw3_velvet_savestate`
  (definitive_velvet.c) entram no `def_savestate`, como o Guide já fazia. Testado: 7 loads seguidos alternando
  comerciante (com a loja abrindo), Etemon e o porco, sem travar (`talktest3.py`).
- **Fora da sala** o mapa 0x20F continua sendo o bar do jogo: nada muda entrando pelas portas normais.

### Posições das portas (25 mapas = 13 pousadas + 12 cópias da segunda metade)

**Refeitas de verdade.** A primeira vez foi por algoritmo + folha de contato (o dono pegou porta na frente de Digimon
e no meio do cenário); a segunda foi olhando 13 telas pequenas e eu errei o que estava atrás em várias. Agora:

- A porta é um **portal em forma de porta**: batente escuro (dois batentes, lintel e soleira) com a luz em 6 faixas
  (apagada no lintel, clara no chão), então ela se sustenta em qualquer lugar e não parece uma placa.
- Profundidade **3** (a camada desenha de trás para frente): fica na frente do chão e das paredes e **atrás do
  jogador** — antes, com a profundidade 2, o jogador desaparecia atrás da porta.
- Cada uma das 13 foi conferida **no seu mapa, em 2x**, com os atores onde o jogo os põe (a busca exclui 56 px em
  volta de todo ator). **O dono liberou qualquer ponto do mapa**, não precisa ser ao lado do Guardromon.

```
0x20A (339,192) parede direita, junto da escada   0x223 (205,269) ponta de toras do deque
0x230 (516,380) patamar da escada, vitral         0x238 (300,440) chão da loja (sem parede; nada na frente)
0x23F (580,392) parede branca do balcão           0x247 (540,304) pé da rampa metálica (sem parede)
0x249 (403,187) degrau de pedra da trilha         0x25B (355,313) borda da plataforma (sem parede)
0x25D (1074,604) mureta de tijolo                 0x263 (383,235) parede de tora, junto da poltrona
0x269 (553,262) face do bloco de máquinas         0x26D (396,520) parede da torre
0x26F (1156,833) parede metálica, longe dos Digimon
```

Segunda metade: mesmas coordenadas em 0x279, 0x292, 0x29E, 0x2A5, 0x2AC, 0x2B1, 0x2B3, 0x2C4, 0x2C6, 0x2CB,
0x2D1, 0x2D6 (0x26D não tem cópia). Prova: `.dumps/tasks/velvet/doors/*.png` (as 25 telas),
`doors_final_a.png` e `doors_final_b.png`.

Compromissos assumidos: 0x223, 0x238, 0x247 e 0x25B **não têm parede** onde a porta caiba sem cobrir ator ou prop —
nesses quatro ela fica em pé no chão aberto, sustentada pelo próprio batente.

## Atores que o jogo nunca usa (estudo de 2026-11-16)

Relatório: `.dumps/tasks/velvet/unused/REPORT.md`; previews: `unused/previews/` (63 folhas, `_shortlist.png`);
lista crua: `unused_ids.txt`. Ferramentas novas (scratch): `unused/tim.py`, `render.py`, `contact.py`,
`sprites_fixed.py` — decodificam TIM 4-bit do disco e desenham as folhas de animação (o `sprites.py` antigo lia o
TIM errado: CLUT em bytes, `w`/`h` da imagem em palavras → quadro `w*4` × `h`).

- 406 IDs de ator em `fieldstg_actor_sprite_files`; 342 são colocados por algum dos 293 stages. Dos 64 restantes,
  9 o motor usa sem colocar (ID 2 = jogador de reserva; 3..10 = os 8 parceiros). Sobram **54 IDs nunca usados**.
- **"ID não usado" ≠ arte inédita**: 54 dos 64 desenham o mesmo sub-arquivo de um ID que o jogo coloca. A única arte
  que ninguém vê é `P182BOMP` (ID 318) — **um porco**.
- Identificados por sprite gêmeo + tag `<7>Nome<7>` das falas: 29 Divermon; 188–191/387/388 Etemon; 207 MetalEtemon;
  233/234 Patamon; 236 Renamon; 237/238 Kotemon; 240 Monmon; 248–250 Knightmon; 286 WaruMonzaemon; 312/313 soldado;
  366 Agumon; 368 Veemon; 370 Guilmon; 372 Patamon; 374 Renamon; 376 Kotemon; 378/380 Kumamon; 17/18/19 corpos
  genéricos; 177/251 porcos; 275–279 "Guard A/B/C"; 353–360 são os objetos BLOK (0x148/0x15F/0x160), não NPCs.
- **Usado na sala** até 2026-11-17: mentor = 376 (Kotemon, `P189PKEL.BIN`/0x19, 32×32, 1 anim); hoje o velhinho 0x39.
  (o slot `vram_places[5]`: o vizinho usa u=32, então o quadro tem de ter ≤32 px de largura).

## Verificação feita

- `.dumps/tasks/velvet/trigger.py`: as 25 portas, nas posições finais, levam ao mapa 0x20F (0 falhas). Lê a
  tabela direto de `definitive_velvet.c` (`doors_of.py`), para não divergir.
- `.dumps/tasks/velvet/e2e.py`: pousada 0x20A → porta → sala → falar com o Etemon (diálogo com a fala do port) →
  porta da frente → volta para 0x20A no Guardromon. Telas em `.dumps/tasks/velvet/e2e-*.png`.
- `tools/gen_text_override.py --check` passa.
- `tests/port/run.py` (replays): passa (`port test: pass`), a sala fica desligada sob `--script`.
- `tests/port/field.py --only lobby_lab_exit items_list`: ok. Os casos `swamp_dive`/`swamp_flies` **já falhavam
  antes** por um passo `write_mem` que o `psxstack/runtime/script.c` deste checkout não implementa
  ("unknown step type"), sem relação com a sala.
- Build SDL3 (`build/port-sdl`): compila e a sala aparece igual (`sdl_room.png`).

## Ferramentas de scratch (em `.dumps/tasks/velvet/`)

- `cycle.sh COMANDO...`: recompila, regera `field.state` (um estado só carrega no binário que o salvou) e roda.
- `shot.py MAPA X Y DIR saída [--poke sym=valor] [--player X:Y:DIR]`: tela de um mapa.
- `doors_try.py MAPA:x:y ...`: move portas em RAM (a tabela `doors` não é `const` de propósito) e tira uma tela de
  cada, sem recompilar — foi assim que as 13 posições foram revistas.
- `walk.py`, `montage.py` (13 pousadas), `trigger.py` (as 25 portas), `e2e.py` (o caminho completo), `attr.py`
  (mapa de colisão em texto; `--around X Y`).
- `sprites.py` (renderizar atores do disco): **quebrado** (offset do script do frame), não usar.
- Notas de campo: `POLY_F4` com `setSemiTrans` no campo sujam o vazio/tiles (o blend fica ligado para os prims
  do jogo) — usar só opaco; a camada desenha o que foi **adicionado por último primeiro** (último = atrás).

## Decisões do dono (histórico)

1. Primeiro o **Velvet Server**: sala onde vivem os NPCs novos.
2. NPCs: o **comerciante do Black Market** (muda de lugar, passa a viver na sala) e o **mentor tipo Gennai**
   (Blast Gauge / Chosen Evolution; só o gancho e o lugar, o conteúdo é de outra tarefa). Depois o dono pediu o
   **Etemon no piano** de brincadeira, com a fala de "ouvi essa música em outro jogo", pela mesma via (textos
   traduzíveis).
3. Cenário: o **bar de Asuka City, mapa 0x20F** (`src/wstag/wstag255.c`, background `S255PACK` = file 0x1AA),
   com o vazio em azul e a sala em tom azul.
4. Entrada: **porta azul** em cada uma das **13 pousadas** do Guardromon (`definitive_travel.c` `nodes[]`,
   `game_mods.c` `gs_inns[]`). Dono aprova ou muda cada uma.
5. Porta da frente (saída) volta para a pousada; a de cima fica fechada.
6. Música: aleatória por enquanto, entre faixas pouco usadas; ele compõe depois.
7. Pode compilar uma versão de teste; ele ajusta depois.
8. (2026-11-16) Nome da área: **Velvet Server** (o cartão do mapa mostra VELVET / SERVER nas duas línguas).
9. (2026-11-16) O comerciante se chama **Oikawa** (Yukio Oikawa), referência ao personagem de Digimon; o nome fica
   nas falas. A arte é original (desenhada à mão), não a do personagem: as duas tentativas parecidas com ele e a de
   chapéu foram recusadas por destoar do estilo do jogo.
10. (2026-11-16) O **mentor tipo Gennai** por enquanto é um NPC antigo do jogo: pode trocar o que está lá hoje (o
    Kotemon 0x178) por outro e mudar o lugar dele na sala para um canto bom. Desenhar um NPC novo fica para depois; o
    foco agora é a mecânica.

## Artefatos antigos (reconhecimento)

- `.dumps/tasks/velvet/bar_20F.png`, `bar_mock_azul.png`: bar e o mock azul que definiu o degradê.
- `.dumps/tasks/velvet/inns/*.png`: fundos das 13 pousadas com um círculo ciano no Guardromon.
- Renderer de fundo (scratch antigo, sumiu): decodificava `S###PACK` pelo `docs/FORMATS.md` "Background".

## Mapa de código (referência)

| O quê | Arquivo |
|---|---|
| Stage 0x20F (bar) | `src/wstag/wstag255.c` (setup, actors[], sprites[], map_events[]) |
| Stage code por mapa | `src/fieldstg/fieldstg_80087DB0.c` `fieldstg_stages[240]` (`{id, file, entry}`) |
| Fala de ator | `fieldstg_actor_talk_to` (~3214): `def_bm_talk`/`def_dri_talk` antes do redirect |
| Traders (mapa, ator) | `port/game/definitive_market_screen.c` `traders[]`, `def_bm_talk` (~542) |
| Porta/saída entre mapas | `FieldstgMapEvent` (flags, type 1 = mover, x,y,dir, route, room), `include/fieldstg.h` |
| Spots de mapa (camada 7) | `fieldstg_map_events_check`: `attr & 0x1F` = índice, `attr >> 5` = direção |
| Pousadas e posições | `port/game/definitive_travel.c` `nodes[]`; `port/game/game_mods.c` `gs_inns[]` |
| Música | `fieldstg_stage.music` (wstag255 usa 8; a sala usa 0x3B) |
| Background | `fieldstg_stage.background_file` (`S###PACK`, 0x1AA para o bar) |

Hooks PC do port: `#ifdef PC_PORT` no código do jogo e as funções em `port/game/*.c`, protótipos em
`include/port.h` (o port.h é incluído pelo `common.h`, sem tipos do jogo: por isso `dw3_velvet_event_closed`
recebe `type, param` e não o `FieldstgMapEvent`).

## Próximas tasks (do dono, 2026-11-16) — Blast Gauge e o mundo do Velvet

Ainda não começadas. O dono vai focar nas mecânicas; o NPC do mentor fica um NPC antigo do jogo por ora (decisão 10).
Texto do dono, como ele mandou:

### 5. Blast Gauge: Da Maldição à Transcendência ("Chosen Evolution")

**Fase 1: A Evolução Maldita (Campanha)**
- Carregamento por Agressividade: A barra só sobe com ações realmente ofensivas e arriscadas: acertar fraquezas
  (WEAK!), encadear One More, bater em alvos já em Stagger e executar OVERFINISH (continuar golpeando o inimigo após o
  nocaute). Ações passivas (defender, curar, fugir) não carregam ou esfriam a barra.
- Ativação Opcional: Não dispara mais automaticamente; o jogador aciona quando quiser (via botão dedicado) ou recebe
  a tentação de usá-la em momentos críticos (como quando o parceiro cai no Hard Mode).
- Pool Curado de Formas Sombrias: Substitui as evoluções aleatórias do jogo original por uma lista selecionada a dedo
  de formas sombrias/agressivas escaladas pelo nível do jogador.
- O Preço do Poder (Debuff Severo):
  - Ao término da batalha em que a Blast Gauge foi usada, o Digimon fica com HP e MP reduzidos a 1 e recebe um debuff
    persistente que só é removido ao visitar uma Gatomon / Inn.
  - Item Maldito (Catalisador): O antigo item inútil de carregar Blast (que exigia 10 usos) agora ativa a Blast Gauge
    instantaneamente com 1 uso, mas consome TP (Training Points) do Digimon, avisando na descrição que força o
    parceiro além do seu limite.

**Fase 2: Purificação & Julgamento Interno (Late Game)**
- Registro no Save: O jogo salva cada forma sombria única que o jogador invocou via Blast Gauge durante a campanha.
- Batalha de Purificação (Estilo Gauntlet): No late game, o jogador pode pagar uma taxa em Bits/materiais para
  enfrentar, em sequência contínua (sem recuperar HP/MP entre ondas, igual ao DRI Gauntlet), todas as formas únicas
  que ele mesmo forçou ao longo do jogo.
- Recompensa de Purificação: Vencer devolve os TPs sacrificados pelos itens malditos.

**Fase 3: Domínio Total ("Chosen Evolution" / True Warp)** (texto original; o que foi feito está em "o redesenho
implementado", abaixo)
- Condição: Invocar e purificar todas as formas disponíveis na lista da Blast Gauge.
- Recompensa Máxima: Além de buffs permanentes nos atributos, a Blast Gauge é purificada e se transforma na evolução
  definitiva (inspirada no conceito de DigiEscolhido / Gennai):
  - Sem debuffs pós-batalha.
  - Regra do Rookie (Forma Base): Para ativar a Blast purificada, o jogador precisa iniciar/estar na forma base
    (Rookie) — assumindo o risco de ter menos Speed e atributos no início da luta — para então executar uma Warp
    Digivolution direta para a forma Mega canônica/escolhida.
  - Quebra do Teto de 999: Esta transformação é a única mecânica capaz de ultrapassar o limite de 999 nos atributos
    durante o combate (de forma controlada), servindo como trunfo máximo contra os chefes mais extremos do endgame.

### 6. Mundo, Onboarding Orgânico & "Velvet Room"

**A Sala Oculta (Referência Persona + Gennai)**
- Acesso pelo Mundo: Adicionar portas misteriosas ao lado dos guarda-roupas/armários espalhados pelos mapas do jogo.
  Entrar por elas leva a uma sala extradimensional única (inspirada na Velvet Room / Velvet Server).
- Habitantes:
  - O mercador do Black Market (troca de materiais raros e réplicas).
  - Um NPC mentor inspirado no Gennai, responsável por explicar o risco da Evolução Maldita, conduzir o Gauntlet de
    Purificação da Blast Gauge e conceder a Chosen Evolution.

**Integração Natural na Campanha (Onboarding sem Tutoriais Chatos)**
- Ajustes Pontuais nas Quests Originais: Inserir pequenos ganchos obrigatórios na progressão principal para garantir
  que o jogador descubra as novas mecânicas organicamente:
  - Fazer o jogador interagir com um Guardromon logo após liberar o segundo ponto de teleporte, ensinando a rede de
    viagem rápida na prática.
  - Direcionar o jogador a atravessar uma das portas ao lado dos guarda-roupas pela primeira vez no momento em que os
    Hunt Materials raros começam a aparecer, apresentando a sala oculta e seus NPCs sem quebrar o ritmo da história.

**As portas (resposta do dono, 2026-11-16)**: a ideia é eu reajustar as portas: hoje são as portas azuis das 13
pousadas, perto do Guardromon (decisão 4). Fazer um sprite de porta melhor (desenhado à mão no estilo do jogo, como
o Oikawa; nada recortado do disco) e colocar cada uma num lugar melhor: ao lado dos guarda-roupas/armários dos mapas,
como no texto acima.

## Feito em 2026-11-17

- **Save state das outras telas**: `def_travel_savestate` (o `nt` da tela do Guardromon), `def_evohint_savestate`
  (`eh`), `def_callout_savestate` (`callout`) e o `def_dri_savestate` agora leva também `dri`, `dri_wait` e os buffers
  de texto da lista. Registrados em `def_savestate` (definitive.c). Prova: `.dumps/tasks/velvet/ss/travel_ss.py BIN
  OUT` (abre a tela do Guardromon da pousada de Asuka, salva antes e com ela aberta, carrega alternando 4 vezes e
  confere aberta/fechada, cursor, △ e andar): o binário antigo **crasha no primeiro load**, o novo passa os 25
  checks. `tests/port/run.py` passa.
- **Mentor**: agora é o **velhinho 0x39** (`P074JIJI`, o corpo dos "Grandpa ..." e do Tamer Genji: careca, barba
  branca, 24x32, 5 direções; cabe no slot de VRAM 5) **no palco entre os tubos de órgão**, no alto da sala, em
  (318,165) dir 1. O palco é um beco sem saída ligado ao andar de cima por uma faixa (y 160..176): não bloqueia
  passagem, e a câmera da entrada já mostra as luzes vermelhas dele. Fala de qualquer lado (testado: esquerda, baixo,
  direita). Telas: `.dumps/tasks/velvet/mentor/new-*.png`. Alternativas vistas: Piximon (o treinador do Adventure,
  48x40: não cabe no slot), Wizardmon (32x40), Master John.
- **Build próprio**: `build/port-velvet/dw2003` (da árvore principal; o `build/port` é o dos outros agentes). Enquanto
  o Black Market estava em obras usei um worktree (`.dumps/tasks/velvet/wtsync.sh`). `vrun.py` roda as ferramentas de
  scratch contra outro binário (`DW3_BIN=...`).
- Nota de teste: o `talktest4.py` **não recarrega** o estado entre pontos (o balão anterior continua aberto e o ○
  avança a conversa dele): rodar um ponto por processo, como já dizia o plano.

## Blast Gauge, fase 1 (2026-11-17: feita e testada)

Como o jogo fazia (lido no código): `wfightmn_add_gauge` enchia `records_state.gauges[id do Digimon]` (0..1000) só
com **dano recebido**; em 1000 entrava o evento 0x12 → `wfightmn_blast` (mensagem 0x61 "Blasted!", forma
`blast_forms[faixa de nível]`, HP cheio, `blasted = 1`, fim agendado pelo evento 0x13). **Atenção (erro que já foi
cometido):** a tabela `wfightmn_handlers` é deslocada: o passo é o tipo do evento **+ 1** (evento 0x12 → passo 0x13;
o passo 0x12 é `wfightmn_seal_end`).

O que mudou (tudo sob `def_on()`; sob `--script` o jogo é o original). Números em `definitive.h` (`DEF_BLAST_*`):

1. **Carga por agressividade** (`def_action_resolved` → `blast_add`, definitive.c): por golpe que acerta, WEAK +150,
   alvo em Stagger +100, OVERFINISH +200, golpe numa ação extra (One More) +100, somados (`def_blast_gain`, pura).
   Item e fuga esfriam −100 (`def_blast_command`). Apanhar não enche, nada dispara sozinho (`def_blast_auto`).
2. **Ativação**: 8ª ação do `battle_menu`, **Blast**, em **R2** (opção `blast` do mod, `mod.json`, LAUNCHER.md), ícone
   da estrela escura (`01 36`) embaixo do L1. Aceita com o gauge em 1000, sem Blast ativo, com HP, sem os status 8 e
   0x20 (`def_blast_ready`); senão cinza e recusado. Escolha 7 → comando 7 (`DEF_BLAST_COMMAND_ID`) → passo 0x13, gasta
   o turno como o DV. `tests/port/mods.py` agora espera "R2: refused 7" com o gauge vazio.
3. **Formas sombrias** (`blast_pool`, definitive.c; `DW3_BLAST_FORM=linha` força uma em testes): por faixa de nível
   do jogo, uma lista; o parceiro (índice 0..7) pega `pool[parceiro % tamanho]`, então cada parceiro tem sua linha e as
   10 aparecem no time. <4 Devimon · <19 Devimon/SkullGreymon/Digitamamon · <39 SkullGreymon/Myotismon/Cannondramon ·
   <70 Myotismon/BK WarGreymon/GranKuwagamon · 70+ BK WarGreymon/MaloMyotismon/Beelzemon/Diaboromon. As 10 testadas
   uma a uma na primeira batalha (carregam, cena de Blast, sem travar): `.dumps/tasks/blast/forms/form*.png`.
4. **Preço** (`def_blast_started` em `wfightmn_blast`, `def_blast_price` depois do write-back de HP/MP no fim da
   batalha): HP e MP 1 e **`GamestateStats.penalties`** = 25% de Power, Guard e Speed. O jogo original nunca escreve
   esse campo: só `inn_heal_party` zera (a pousada) e as telas de status já marcam o status penalizado. Sem campo novo
   no save. Um segundo Blast antes da pousada não aprofunda.
5. **DV Plug maldito** (`def_blast_plug`, efeito 0x4C de `fightstg_8008D3B4.c`): um uso enche o gauge, tira
   `DEF_BLAST_PLUG_TP` = 10 TP (`values[1]`; o que tiver se tiver menos) e chama o evento 0x12 do jogo (o Blast vem
   logo depois). Descrição: "Blast past its limit. Costs 10 TP" / "Blast além do limite. Custa 10 TP".
   `tools/gen_equipment.py` ganhou `OTHER_TEXTS` (itens fora das variantes com texto do port: 26, 46, 67, 70, 76);
   antes o check já falhava por 26/46/67/70.
6. **Tentação** (feito 2026-11-17; em qualquer modo: o Hard Mode do texto do dono era só o exemplo): gauge cheio e
   momento crítico (um parceiro caiu nesta batalha, `def_member_knockout`, ou quem age com ≤ 25% de HP,
   `DEF_BLAST_TEMPT_HP_PCT`) → o botão Blast pulsa em vermelho (8 ticks aceso, 8 apagado) e "Blast calls..." / "O
   Blast chama..." aparece acima da cruz, na fonte de 14 px (`def_blast_tempt`). Não força nada. Teste:
   `.dumps/tasks/blast/tempt_test.py` (cheio+HP baixo tenta; cheio+HP cheio e vazio+HP baixo não).

Testes (scratch, `.dumps/tasks/blast/`; o estado de batalha vem de `tests/port/mods.py --only battle_menu --binary
BIN --out .dumps/tasks/blast/mods`, refazer a cada build): `blast_test.py` (R2 dispara; apanhar não enche),
`price_test.py` (vence a batalha e o log diz "digimon 0 pays: HP and MP 1, penalties 12 11 8"), `form_shot.py`
(fotos da forma), `plug_test.py` (DV Plug). `tests/port/run.py` passa; `travel_ss.py` passa; `gen_text_override.py
--check` e `gen_equipment.py --check` passam.

Launcher: o C (`launcher/`, desktop e Android) ganhou a linha BLAST (`H_BLAST`, `bm_blast`, padrão R2; no Android
as linhas da tela passaram de 27 para 25 px para caber); o do psxstack lê o `mod.json`.
Não testado em jogo real: uma batalha comum com WEAK/Stagger enchendo o gauge até 1000 (a regra está no log
`DW3_BATTLE_LOG`, linhas "blast gauge hit").

## Blast Gauge: redesenho do dono (2026-11-17, tarde) — substitui o que está abaixo onde conflitar

**Formas** (resposta "A"): as do jogo (`RecordsDigimon.blast_forms`, 5 por parceiro, 29 distintas), não a minha lista
de 10; **pintadas de preto** quando vêm pelo Blast (método de CLUT da outra sessão, `super-rare-hunts.md`, quando
pronto). **Em sequência por parceiro**: cada Blast dele avança para a próxima forma se tiver o nível dela (forma 1 nv
1, 2 nv 15, 3 nv 25, 4 nv 35, 5 nv 45), senão repete a última alcançada; depois da 5ª volta ao começo.

**Fora do escopo daqui** (dono, 2026-11-17): os ganchos na história principal (seção 6, "Integração Natural na
Campanha"). O dono está mudando a quest principal em outra sessão e depois cria quests guia; esta task não mexe nisso.

**Gravação**: o **primeiro** status de cada forma (o Digimon transformado na primeira vez: atributos, HP, MP, a
espécie dá as técnicas) e o **melhor** status que ela já teve. Precisa de um 3º registro no cartão (o 2º não cabe).

**A quest** (10 = 10 pares **parceiro+forma** distintos: a mesma espécie em dois parceiros conta duas vezes, então
dois parceiros com as 5 cada fecham 10 — funciona no Hard Mode, de 2 Digimon):
1. O velhinho da sala se apresenta como **Gennai** (é o Myotismon disfarçado). Só mostra quantas formas você liberou
   e manda dominá-las; nada de quest.
2. Ao chegar nas 10: ele **cobra os Bits** (a taxa), revela que te enganou e que seu Digimon está amaldiçoado; tela
   preta e um som; aparece o velhinho de verdade: ele é o Gennai, o outro era o Myotismon, e acredita que você é um
   DigiEscolhido.
3. O Gennai abre o **simulador**: as 10 formas em sequência (ondas de 3 encadeadas, sem tela de resultado entre elas),
   cada uma com o **primeiro status gravado − 10%** e as técnicas da espécie.
4. Vencendo: **todos os TP** do DV Plug de volta; **buff permanente por forma purificada: +10% do melhor status que
   aquela forma teve** (no atributo em que ela era mais forte, para o parceiro que a tomou; o que passar de 999 vai
   para o próximo atributo mais forte da forma, e assim por diante); e o Blast **some**, substituído pela **Chosen
   Evolution**.

**Chosen Evolution**:
- Só **um** Digimon por vez, escolhido com o Gennai (pode trocar com ele sempre).
- Exige a quest feita **e nível 45** do Digimon.
- Forma: a Mega padrão do parceiro; **Kumamon → Beelzemon**.
- Só a partir da forma **Rookie** (aviso se tentar em outra evolução); **uma por batalha**; **uma carga**: depois de
  usar, só volta a ter carga falando com o Gennai.
- Ao usar, recupera tudo (HP e MP).
- O **DV Plug** passa a reviver o Digimon na forma Rookie com 1 HP e 1 MP.

## Blast Gauge: o redesenho implementado (2026-11-17, noite)

Substitui a "fase 2" antiga (lista fixa de 10 formas, taxa por forma, espelho do grupo), que saiu do código.

- **Sequência** (`definitive.c`, `def_blast_form` / `def_blast_step`, pura e testada em `definitive_buffs`): as
  `blast_forms` do próprio jogo, o próximo passo de cada parceiro no 2º registro (`REC2_BLAST`), níveis
  `DEF_BLAST_STEP_LEVELS` {1, 15, 25, 35, 45}. `DW3_BLAST_STEP=k` força um passo nos testes (sem mexer na sequência).
- **Preto**: `DEF_RECOLOR_DARK` em `definitive_recolor.c` (arquivo da outra sessão, toque mínimo): luminância a 40%
  com leve tom roxo; modelo do grupo (id 1) quando o membro está em Blast (`def_blast_dark`), e os inimigos do
  simulador (id 2). A Chosen sai com as cores normais.
- **Gravação**: 3º registro do cartão, parte 7 (`memcard.c` 0x7E00 + slot × 0x80; `stgmcard` passos 0x4A na leitura e
  0x38 na escrita): "BLST", versão, checksum, até 10 pares parceiro+forma de 11 bytes (atributos da 1ª vez, HP, o
  atributo mais forte e o melhor valor que ele já teve). `def_blast_started` grava; testado em `definitive_bestiary`.
- **Quest** (`definitive_purify.c`, o cabeçalho explica): estágio no 2º registro (enganado / revelado / feita).
  "Gennai" mostra n de 10 e manda dominar; com 10, cobra `DEF_PURIFY_BITS` (10000), história em 2 páginas, tela preta
  com som (`PF_SND_CURSE`, provisório), o Gennai de verdade em 2 páginas, simulador. Ondas de 3 sem espécie repetida,
  status gravado × 90%, sem tela de resultado entre ondas (`wfightmn` manda direto ao campo), sem XP/Bits. Derrota:
  volta ao campo, HP/MP de antes; formas derrubadas ficam purificadas. Ao fim: TP devolvidos (até 99), buff +10% do
  melhor valor no atributo mais forte de cada forma (o excesso de 999 vai ao próximo), Blast vira Chosen.
- **Chosen** (`definitive.c`): o botão do Blast passa a ser a Chosen do parceiro escolhido com o Gennai (painel dele,
  que recarrega a carga a cada visita), nível 45, só da Rookie (linha "Chosen needs Rookie" no menu), uma por batalha,
  restaura HP e MP, sem maldição nem medidor. Megas em `DEF_CHOSEN_MEGAS` (Kumamon → Beelzemon). DV Plug depois da
  quest: item de reviver (passa pelo caminho do item 0x46 do jogo, que revive todos os caídos): cada parceiro caído
  volta na Rookie com 1 HP / 1 MP e o Blast desligado (`def_blast_plug_revive`); descrição trocada para "Revive:
  Rookie form, 1 HP and 1 MP" / "Revive em Rookie com 1 HP e 1 MP" (`$PlugRevive`, servida no lugar do ITMINF 76).
  Teste: `.dumps/tasks/blast/v3/plug_revive.py` (revive, gasta o item, sem maldição; antes da quest ainda é a
  maldição) passa.
- **Testes** (scratch, binário copiado para `.dumps/tasks/blast/v2/dw2003` porque a outra sessão recompila o
  compartilhado): `blast2_test.py` (sequência passo 0/3/2, gravação, preto, Chosen e recusas) passa;
  `dark_shots.py` (folhas de contato: GranKuwagamon escuro, GuardiAngemon normal); `quest_test.py` (quest inteira).

## Pendências / pedidos ao dono

- **Portas**: sprite novo desenhado à mão e reposicionar ao lado dos guarda-roupas/armários (seção "Próximas tasks").
- **Blast Gauge**: feita (redesenho acima). A seção 5 de "Próximas tasks" é o texto original do dono; onde ela
  conflita (pool curado, taxa por forma, teto de 999 na Chosen), vale o redesenho. Provisórios para o dono ajustar: o
  texto da história do Myotismon/Gennai e o som da tela preta (`PF_SND_CURSE`).
- **Onboarding** (seção 6, ganchos na quest principal): fora do escopo daqui; o dono faz em outra sessão.

### Ao dono

1. **Testar no jogo**: entrar numa pousada, achar a porta azul, entrar, falar com as figuras, sair pela frente.
2. **Posições das 13 portas** — aprovar ou dizer "nessa pousada, sobe/desce/encosta".
3. **Mentor**: agora o velhinho 0x39 no palco (2026-11-17), fala ainda provisória. Aprovar ou pedir outro.
4. **Música** da sala: hoje `0x3B` (só o mapa 0x25F usa); o dono compõe depois.
5. **Traduções**: as duas falas existem em EN e PT-BR. O override do port só cobre inglês e PT-BR;
   FRA/ITA/GER/SPN mostram o texto do disco (a letra "a") até a tarefa de tradução geral.
6. Salvar no cartão de memória dentro da sala: o save cai no 0xC00 genérico e, ao carregar, o 0x20F abre como bar
   normal (`vl_room` é RAM, não vai no cartão). Aceitável? Se não, é a próxima decisão. (O save state do port já leva
   `vl_room` desde 2026-11-16.)

## Regras do dono (resumo)

- pt-BR; sem commit sem pedido; sem atribuição de IA em arquivos do repo.
- Scratch em `.dumps/`; planos em `.plan/tasks/`.
- Mover arquivos para `/tmp` em vez de `rm`.
- Sem "não sei, espera": medir e seguir; só perguntar decisão que é dele.

## Super Hard (Sineta Demoníaca) — 2026-11-16

- **Guilmon** (ator 0x172, vram place 3) mora na sala, no andar de cima; `talk` alterna o item-chave **26** (slot de tag
  japonesa que o disco US nunca dá; renomeado "Demon Bell"/"Sineta Demoníaca" em `en.txt`/`pt.txt`). Falas: TALK05 77
  (entregou) e 78 (tomou de volta). O mentor (hoje o velhinho 0x39) foi para o palco no alto da sala (318,165).
- O modo **é** o item na mochila (`def_super_on`, `port/game/definitive_super.c`): HP ×1,5 (`wfightmn_init_members`),
  Speed ×1,5 (`fightstg_rules_get_enemy_stats`, pode passar de 999 → "Enemy x2"), Bits ×2 (`stfgtrep`, os dois
  caminhos) e Hunt Materials ×2 (`common_drops`). Números em `definitive.h` (`DEF_SUPER_*`).
- Não testado: uma luta real com a Sineta; o balanço dos números é do dono.
