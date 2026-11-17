# Caçadas de super raros + raros regionais com um Digimon só

Estado em 2026-11-17: **pronto e testado no jogo, falta commitar.** O commit espera a sessão do Blast commitar a parte
dela, porque os arquivos se misturam (`definitive.c`, `definitive.h`, `definitive_bestiary.c`, `include/port.h`, os
textos) e `definitive_recolor.c` chama funções dela (`def_blast_dark`, `def_purify_fighting`). Depois disso, commitar o
que sobrar desta frente num commit que compila sozinho.

Saves antigos não importam ("ninguém está jogando ainda, é tudo debug").

## Decisões do dono

**Raros regionais** (os "Núcleos"):
- Fixos por região, **um Digimon só por batalha** (sem trios), qualquer espécie temática, de preferência uma que não
  aparece na região.
- Nível = média dos encontros da região + 5 (pode baixar um chefe forte num lugar fraco). HP × 4, teto 30000.
- Núcleo com o nome da espécie: "<espécie> Core <numeral>", numeral só quando a espécie repete.
- **Galacticmon nunca** (chefe final), nem raro nem super raro.
- Com uma caçada ativa e o jogador no território do alvo, o raro regional "atrapalha" (estilo Monster Hunter): a cadeia
  dá chance com 2 vitórias (o Rush conta) em vez de 6, e a fuga do jogador contra ele cai para 50%.

**Caçadas de super raros:**
- 12 super raros, um Digimon cada, sem repetir, liberados **em sequência**: todos aparecem na lista com o nível
  recomendado, a próxima abre quando a anterior é concluída, as concluídas podem ser **repetidas**, a ativa pode ser
  **cancelada** (o alvo se cura). Níveis de 29 a 61, HP de 15000 a 30000 (aprovados: "está ótimo os níveis").
- Território fixo por Digimon: um setor (Norte/Leste/Central/Oeste/Sul, tabela feita à mão) de um servidor.
- O alvo fica num **mapa sorteado** do território, num **ponto** alcançável; a **bússola de temperatura** guia entre
  mapas (distância pelo grafo de saídas) e dentro do mapa (distância ao ponto). Pisou no ponto, começa a batalha.
- Batalha: o alvo sozinho; ao perder **25% da barra** naquela luta, foge para outro mapa sorteado; **guarda o HP
  perdido**; cada reencontro dá **+20% em todos os status**, acumulando. Fugir ou perder: a caçada continua. O Rush
  nunca pula o super raro. Sair do território: o alvo fica onde estava.
- Recompensa: **1 cristal por caçada**, que vende caro; nada de bônus grande de Bits/XP ("tenho medo de quebrar algo").
- **Cor:** troca de paleta de verdade (shiny) nos super raros **e** nos raros regionais.
- **Tutorial** (pedido depois): uma caçada nível 3 que o Sepikmon dá na Velvet Room. O jogo intercepta a primeira chegada
  a **Seiryu City** na história (a primeira cidade depois de Asuka) e leva o jogador para a Velvet Room pela primeira
  vez. Alvo: **Ogremon no Central Park**, perto de Asuka.
- **Bússola em volta do território** (pedido depois): ela também aparece nos mapas até 3 saídas fora do território, para
  levar o jogador até lá (no tutorial o território é só o Central Park). Regra única para todas as caçadas.
- O **Sepikmon fica em (176,306)**, colado na entrada: o dono decidiu em 2026-11-17 deixar ali.

## Como ficou

**Gerador** `tools/gen_hunt_targets.py` → `port/game/definitive_hunt_targets.c` (proposta legível em
`.dumps/tasks/hunt/alvos.md`, com a seção "Bússola": por caçada, as áreas de fora onde ela aparece):
- lê também as listas de encontro por rota da história (antes 35 fases ficavam de fora): 71 regiões, 149 espécies com
  material comum;
- raro regional = batalha + slot do inimigo, 71 espécies sem repetir, sem Cardmon (duelos de carta) e sem Galacticmon;
- `def_super_rares[13]`: linha 0 é o tutorial (`{ 204, 1, 3, 480, 2, 0, 0x1D, Ogremon, 1500 }`: batalha, slot, nível,
  HP, setor, servidor, área só dele, espécie, preço), depois os 12 super raros em ordem de nível, preço 8000 a 16000.
  Campo `area` ≥ 0 restringe a caçada a uma área (o tutorial); -1 usa o setor inteiro.

**Grafo de mapas** `tools/gen_map_graph.py` → `port/game/definitive_map_graph.c`: 239 mapas, 415 saídas (map events
tipos 1 e 14), quais têm encontros (`def_map_wild`). `def_map_distance` faz BFS.

**Raro regional** (`port/game/definitive_hunt.c`): luta sozinho (o slot escolhido vai para o 1º, os outros zeram),
nível da tabela para cima ou para baixo, HP × `DEF_HUNT_RARE_HP_MULT`; no território da caçada ativa
`def_hunt_chance_in` (2 vitórias) e `def_hunt_flee` (50%).

**Caçada** (`port/game/definitive_superhunt.c`, lógica pura):
- ordem aberta/travada/feita/ativa, repetir, cancelar (cura), sorteio do mapa no território (`draw_map`);
- território: `hunt_region`/`hunt_map`; zona da bússola: `def_super_territory_distance` ≤ `DEF_SUPER_COMPASS_REACH` (3);
- temperatura: COLD ≥ 4 mapas, COOL 2–3, WARM 1, HOT no mapa do alvo, BURNING a ≤ 120 px do ponto; a batalha começa a
  ≤ `DEF_SUPER_POINT_RADIUS` (24 px);
- o ponto: o campo manda os candidatos (`fieldstg_super_points` em `src/fieldstg/fieldstg_80087DB0.c`: flood fill de 8
  vizinhos sobre a camada de colisão, células de 16 px, semente no chão andável mais próximo do jogador, só tiles de
  encontro) e o ponto é o que fica a 2/3 da distância;
- batalha: 1 inimigo, HP que sobrou, `stat_scale` +20% por reencontro (`def_super_scale`), fuga ao perder 25% da barra
  na luta (em unidades da batalha, Super Hard incluso), fuga do jogador sempre funciona contra ele;
- vitória: cristal (até `DEF_SUPER_CRYSTAL_CAP`), caçada encerrada, `def_super_won_take` para o painel;
- `def_super_crystal_sell` devolve o preço da linha.

**Tela, bússola e chamada** (`port/game/definitive_superhunt_screen.c`, `def_superhunt_frame` no passo ocioso do campo):
- **chamada de Seiryu City**: área 0x0E, passo da história ≥ 4, uma vez só (flag `DEF_SUPER_FLAG_MET`) →
  `dw3_velvet_pull` leva o jogador para a Velvet Room;
- **quadro do Sepikmon**: lista as 13 caçadas com nível, estado (NOVA/ATIVA/FEITA/--), onde (setor e servidor, ou "No
  Central Park perto de Asuka City"), HP restante, cristais e preço. × inicia; × duas vezes cancela a ativa; □ vende
  um cristal; ○/△ sai. Na primeira abertura o tutorial começa sozinho (`DEF_SUPER_FLAG_TUTORIAL`);
- **painel de recompensa** depois da vitória ("Hunt reward: <espécie> Crystal");
- **bússola**: polígonos na camada 0x1004 (sem janela no heap), palavra e 5 barras coloridas, em PT na versão PT.

**Sepikmon na Velvet Room** (`port/game/definitive_velvet.c`, arquivo de outra sessão, mudança combinada): ator 61 em
(176,306) dir 7, falas `TALK05 14` (apresentação e tutorial) e `TALK05 15` (as visitas seguintes), depois o quadro.

**Cristais na tela de MATERIAIS** (`port/game/definitive_materials.c`): depois dos Núcleos, um por caçada, "???" até ser
ganho; nome com mais de 21 letras tem a espécie cortada com ponto (`MAT_FIT`, `$MatCut`: "MasterTyrann. Crystal",
"Cristal de MasterTyr.").

**Troca de cor** (`port/game/definitive_recolor.c`): raro regional 120°, super raro 200° (`rc_degrees`, ajustável), e
as formas escuras do Blast (DARK, da outra sessão) pelo mesmo caminho.
- O inimigo entra em `fightstg_slots_add` com id ≥ 0x10 (0x10 no script de abertura, 0x11 numa entrada); o time com 0
  (1 numa entrada ou forma Blast).
- A textura do Digimon de batalha é uma página de VRAM de 16 bits com pixels de 4/8 bits e as linhas de CLUT juntas:
  trocar na hora do upload vira ruído. Por isso: quando a textura carrega (`fightstg_model_update`) a página é guardada
  (`def_recolor_texture_loaded`), e cada CLUT que o mesh usa (comando de textura, `fightstg_model_mesh_set_texture`) é
  lido da VRAM (`psyq_gpu_vram`), girado e escrito de volta uma vez só (`def_recolor_mesh_clut`).
- Cada modelo tem a sua página (`fightstg_slots_vram`), então o time não muda mesmo sendo da mesma espécie.

**Textos** em `tools/text_override/en.txt` e `pt.txt` (`$Sup*`, `$MatCrystal`, `$MatCut`, `TALK05 14/15`), gerados por
`tools/gen_text_override.py` em `definitive_text.c` e `definitive_text_pt.inc`.

**Save**: os últimos 24 bytes do segundo registro (`DEF_SUPER_RECORD`, em `definitive_bestiary.c`, longe do Blast que
cresce do começo): 0 ativa+1, 1 feitas, 2–3 mapa, 4–5 HP perdido, 6 reencontros, 7 flags, 8–20 cristais. A skin do
herói usa o byte logo antes (`REC2_SKIN`).

## Testes

- Host (sem jogo): `tests/port/definitive_superhunt.py` (+ `_driver.c`), `definitive_hunt.py`, e os que usam
  `tests/port/definitive_super_stub.c` (bestiary, dri, market, buffs). Todos no `scripts/test.sh`, todos passam.
- Visto no jogo:
  - super raro em Phoenix Bay: dispara no ponto, vem sozinho, foge aos 25%, troca de mapa, guarda o HP, volta com buff,
    a vitória dá o cristal;
  - tutorial inteiro: chamada de Seiryu, fala do Sepikmon, quadro, bússola COLD..BURNING no Central Park, Ogremon,
    painel de recompensa, cristal em MATERIAIS; o dono também jogou o tutorial até o fim;
  - cor do raro regional (RedVegiemon: corpo roxo, mãos rosa, sem ruído; o time igual).
  - visto em 2026-11-17 à tarde: a cor do super raro (200°: RedVegiemon verde-azulado com espinhos roxos, sem ruído, o
    Kumamon do time igual; `.dumps/tasks/hunt/play/super_recolor.png`, forçado com `run+6` = 1 numa batalha comum); a
    bússola fora do território (tutorial ativo, Shell Beach mostra WARM; `shell_compass.png`, savestate `shell.state`);
    a sessão do Blast viu a forma escura pintada pelo mesmo caminho de CLUT.
- **Visto no jogo em 2026-11-17 (noite)**: os nomes cortados em MATERIAIS em EN ("MasterTyrann. Crystal") e PT
  ("Cristal de MasterTyr.", "Cristal de WaruMonza."; START, MATERIAIS, ○ entra e ↑ no primeiro item vai ao fim da
  lista); os textos PT do quadro, do painel de recompensa (`.dumps/tasks/hunt/play/skin_debrief_pt.png`,
  `board_pt.png`) e do menu; as skins com a Kaili (ver `skins-npc.md`). Para jogar em PT pelo MCP: `game_start` com
  `config` = um JSON `{"schema": 1, "video": {"window": false}, "mods": {"preset_language": {"enabled": true,
  "language": "portuguese"}}}` e depois `state_load` de um estado em EN. A fuga de 50% fica no teste do host
  (`definitive_hunt_driver.c`, sorteio 49 foge e 50 não), sem partida no jogo.

## Como testar no jogo

- `.dumps/tasks/hunt/play/boot.py [MAP X Y DIR OUT]` sobe o jogo pelo save de console (`card1.mcd`, Zhu Que Inn, time
  nível 29), carrega e salva um savestate no mapa pedido (padrão: Phoenix Bay → `phoenix.state`). **Savestate só vale
  para o binário que o gravou**: depois de compilar, rodar o boot de novo.
- Pelo MCP (`mcp__dw2003__*`): `mem_write` em `st` (estado da caçada: `st+1` = caçadas feitas), `run` (o ponto),
  `hunt+1` (força o raro regional na próxima batalha), `skin_hero`. Em Phoenix Bay: → 200 quadros, ↓ 250 dá um
  encontro (pergunta do Rush; × luta).
- Se o link do `build/port` falhar em `__start_dw2003_data_wstagNNN`, é objeto cortado por dois builds ao mesmo tempo:
  mover os `.o` do `wstagNNN` e compilar de novo.

## Falta

1. Commitar (depois da sessão do Blast).
2. As conferências no jogo listadas em "Testes".
3. A posição do Sepikmon na sala (o dono decide quando arrumar a Velvet Room).

Ligado a esta frente: as skins do herói (`.plan/backlog/skins-npc.md`) usam o desbloqueio por caçada vencida.
