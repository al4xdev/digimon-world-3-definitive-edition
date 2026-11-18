# Skins do herói (Kaili, ou Junior sem Girls Power)

Estado em 2026-11-17 (noite): **pronto e testado no jogo.** A base foi no commit do Blast (`8c0971b`); a página de
skins no quadro do Sepikmon foi no commit da forja regional.

## Decisões do dono

- **Skin só do protagonista.** Os Digimon do jogador ficam com as cores deles (pintar o time foi fora do escopo e saiu).
  Só os inimigos raros mudam de cor (`definitive_recolor.c`, parte da caçada).
- **Quem entrega as skins é o próprio Sepikmon** (decisão de 2026-11-17, depois de "sem sala nova"): ele é o mestre das
  caçadas e cada caçada vencida libera uma skin, então não há NPC próprio nem mudança em `definitive_velvet.c`.
- Ligação com a caçada: **cada caçada vencida libera uma skin.** É recompensa sem mexer em Bits e XP, que o dono tem
  medo de quebrar.

## Como ficou

**Módulo** `port/game/definitive_skins.c`:
- 13 receitas, uma por caçada (`DEF_SKIN_COUNT` = `DEF_SUPER_COUNT`): a skin n abre quando n caçadas foram vencidas
  (`def_super_done() >= n`), então o tutorial do Ogremon dá a primeira. Skin 0 = as cores próprias, sempre aberta.
- Cada receita tem duas partes, **cabelo** (o boné, no Junior) e **roupa** (todo o resto menos a pele e o
  transparente), cada uma com giro de tom (graus), saturação (%) e luz (%). A pele nunca muda.
- `def_skin_set_hero` só aceita skin aberta; `def_skin_worn` é a que os sprites usam agora (0 num script de replay ou se
  a skin escolhida ficou travada, por exemplo um cartão com menos caçadas).
- Receitas: cada parte é `ROT` (giro de tom a partir das cores do herói, com saturação e luz; usado para preto, prata e
  branco, que são cinzas) ou `TINT` (toda cor da parte vira um tom só e guarda a luz, então o sombreado fica; uma cor
  quase cinza ganha `TINT_MIN_SAT` antes). O `TINT` entrou em 2026-11-17: o primeiro conjunto (só giros) foi fotografado
  nos dois heróis e o mesmo giro dava cores diferentes (cabelo rosa da Kaili virava violeta, boné azul do Junior virava
  menta), e as receitas só de roupa quase não apareciam na Kaili.
- O conjunto (escolhido por mim em 2026-11-17 olhando as 14 nos dois heróis, `.dumps/tasks/hunt/play/skins_sheet.png`):
  1 Black Hair/Cabelo Preto (a do tutorial), 2 Green Hair/Cabelo Verde, 3 White Outfit/Roupa Branca, 4 Gold
  Hair/Cabelo Dourado, 5 Red Outfit/Roupa Vermelha, 6 All Black/Todo Preto, 7 Purple Hair/Cabelo Roxo, 8 Navy
  Outfit/Roupa Marinho, 9 Silver Hair/Cabelo Prateado, 10 Ocean/Oceano, 11 Sunset/Pôr do Sol, 12 Wild Colors/Cores
  Malucas, 13 Silver and Black/Prata e Preto (`$Skin0..$Skin13`). Os tons de cabelo ficam longe do rosa e do azul
  claro, que um dos heróis já usa. Ajustáveis em `skin_recipes`.
- Save: **1 byte** no segundo registro do cartão, logo antes dos bytes da caçada (`REC2_SKIN` em
  `definitive_bestiary.c`); fora do alcance lê como 0. Savestate: `def_skin_savestate` (chamado em `definitive.c`).

**Pintura** (`port/game/girls_power.c`):
- Os sprites do herói no campo são quadros de 4 bits, cada um com a sua CLUT de 16 cores. Os arquivos do herói são os
  que os atores do Junior usam (0x186, 0x187, 0x3C9..0x3CC); com Girls Power ligado eles viram a Kaili quando o
  cdload os lê.
- Os índices de cabelo e pele vêm das máscaras `GpWho` de cada pessoa: Kaili cabelo 4/6/14, pele 1/2; Junior boné
  2/3/7, pele 8/9.
- `gp_skin` sempre parte da CLUT original do herói (`gp_ref_kaili.clut` ou `gp_ref_junior.clut`): um quadro cuja CLUT é
  a original, ou a original com qualquer skin, recebe a original com a skin usada. Por isso dá para repintar de novo
  sem acumular, e um quadro com outra CLUT fica como está.
- `port_girls_reskin` (chamado a cada quadro do campo, em `def_superhunt_frame`) repinta os arquivos que já estão na
  memória quando a skin usada muda (escolha, desbloqueio, cartão lido). Não precisa trocar de mapa: a mudança aparece
  no próximo quadro de animação (parado, o sprite não volta para a VRAM).
- `port_girls_refresh` (New Game, leitura de cartão) também libera os arquivos quando a skin muda.

**Teste**: `tests/port/definitive_skins.py` (+ `_driver.c`), no `scripts/test.sh`: desbloqueio, skin usada, CLUT
(cabelo e roupa mudam, pele, transparente e bit STP ficam, nenhuma cor vira transparente) e save.

**Visto no jogo** com o Junior (save de console, Phoenix Bay; `mem_write` em `st+1` = 13 e `skin_hero`): skin 4 deixou
o boné verde-oliva, skin 6 boné e roupa pretos com o rosto igual, skin 0 voltou ao original.

## Falta

1. ~~O NPC e a tela.~~ Feito em 2026-11-17 como uma página do quadro do Sepikmon (`definitive_superhunt_screen.c`,
   `skins_fill` e `skins_input`): R1 no quadro abre "Skins do Herói", L1 volta às caçadas. Lista as 14 (Original e as
   13), USANDO na que está vestida (paleta 3, a da caçada ativa: o verde da fonte pequena desenha o "O" esquisito; o dono gostou desse estilo no
   "DONE" das caçadas, que fica como está), "--" e nome apagado nas
   travadas; embaixo, "Vença a caçada de <alvo>" na travada ou "Sua, de uma caçada vencida."; × veste
   (`def_skin_set_hero`, "Pronto. Ande para ver."), × numa travada recusa. A dica das caçadas ganhou "R1: Skins".
   Visto no jogo: `.dumps/tasks/hunt/play/skins_page.png`, `skins_locked.png`, `skins_green_walk.png`.
2. ~~O painel de recompensa da caçada ganha a linha "Nova skin: <nome>".~~ Feito em 2026-11-17: aparece embaixo do
   cristal só na primeira vitória de uma caçada (`def_super_won_take(&first)`, texto `$SupSkin`); vencer de novo não
   abre skin. Testado no host (`definitive_superhunt.py`) e visto na tela (`.dumps/tasks/hunt/play/skin_debrief.png`: "New skin: Alt Outfit", o painel com 82 px).
3. ~~Testar com a Kaili.~~ Feito em 2026-11-17: as 14 vistas na Kaili e no Junior. Achado no caminho: um savestate
   carregado num processo novo perdia `gp_painted` (para quem os arquivos do herói foram pintados), e a Kaili não era
   repintada; agora `port_girls_savestate` (em `definitive.c`) guarda `gp_painted` e `gp_skinned`. A skin nova aparece no
   próximo quadro de animação, então com menos de um segundo andando. Para fotografar: um processo por skin (carregar o
   mesmo estado várias vezes no mesmo processo não repintava; não investigado, porque savestate é só de
   desenvolvimento, `--load-state` e o canal de debug, e o jogador nunca carrega um).
4. ~~Retratos, menus (status, Digi-Lab) e a tela de nome/cartão com skin.~~ Descartado pelo dono em 2026-11-17
   ("besteira"): essas imagens ficam com o herói sem skin.
