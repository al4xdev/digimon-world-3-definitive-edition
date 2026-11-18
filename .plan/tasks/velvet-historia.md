# Velvet Room na história principal (pedido do dono, 2026-11-17)

Continua `velvet-room-plan.md` (sala, Blast, quest do Blast, Chosen) e `super-rare-hunts.md` (Sepikmon, tutorial do
Ogremon). Os passos são `gamestate_data.progress` (0..45), os mesmos do GUIDE (`definitive_quest.c`).

## Decisões do dono

- **Eu faço tudo**: quests, ganchos e mecânicas. Foco em **boas falas** (Igor e quests).
- **Igor** por enquanto é o NPC de terno já existente, posto na sala; a arte própria fica para outra sessão.
- **Boost**: Blast **+30%** de atributos; Chosen **+50%**. Depois do ponto sem volta, os chefes finais são reescalados
  para a Chosen fazer sentido.
- **Superaquecimento**: vale igual para Blast e Chosen. Quando a forma acaba, o Digimon fica **3 turnos na Rookie**:
  sem evoluir e sem DNA/tag para as evoluções dele.
- **Blast na campanha**: o Myotismon disfarçado de Gennai cobra Bits para melhorar o boost do Blast, a cada forma nova
  liberada.
- **Sala do Tempo**: **sem prêmio**. É obrigatória e serve para medir o nível do jogador antes da última batalha, em
  vez de jogá-lo numa zona sem volta.

## Ganchos, em ordem na história

| Passo | Onde | O que acontece |
|---|---|---|
| 0 · Digimon Online | cutscene do mundo real (Junior, Teddy, Ivy) | Junior vai para a Velvet Room, o Igor dá a chave; ao voltar, Teddy e Ivy perguntam por que ele ficou parado |
| 4 · The Seiryu Badge | Seiryu City, pousada (0x230), patamar de cima | GUIDE: perguntar pelo Líder (Tom Repetitivo). Falar com o Tom: o Líder sumiu; Junior sente algo; a porta azul abre no tapete ao lado do Tom (228,388). Dentro: só o Igor reapresenta a sala (TALK05 20), corte, só o Sepikmon chama (17) → caça do Ogremon → relatório (16) → volta à quest principal. **Feito e testado** (.dumps/tasks/story/tom/tom_test.py) |
| 1..39 | campanha | Blast amaldiçoado; o "Gennai" vende melhorias do boost a cada forma nova |
| 26 → 27 | passagem Asuka → Amaterasu (Bug Maze Pit, ~nv 30) | Oikawa bloqueia (sprite encapuzado sem o "4" de debug), luta (Arukenimon + Dinohyumon + MaloMyotismon, nv 30), libera a passagem e some. Missão no GUIDE: Velvet Room; o Oikawa atrás do balcão abre o Black Market (fechado até aqui) |
| 40 · No way back | antes do warp para Gunslinger | Uma porta da Velvet aparece ali. Dentro, os 3 NPCs (o velhinho é o Myotismon sem a Chosen, o Gennai com ela). A porta de cima da sala leva a 3 áreas clássicas do jogo com fundo azul, uma por NPC; luta um a um. Recuar ou perder: recomeça do primeiro. Sem passar, o warp não abre. Aviso antes: sem a Chosen, recomenda terminar a Velvet Room |
| 41..44 | Gunslinger e o fim | chefes reescalados para a Chosen |

## Nível da Velvet Room (dono, 2026-11-17)

Os NPCs chegam com a história; a sala completa de hoje é a final. `def_story_velvet_level()` (definitive_story.c)
deriva o nível do passo + flags da história (saves antigos funcionam). As portas das pousadas só aparecem do nível 1.
No primeiro acesso depois de o nível subir, o hóspede novo chama o jogador (nível visto: bits 12..14 de story_flags).

| Nível | Quando | Quem entra |
|---|---|---|
| 0 | até a fala do Tom (passo 4) | portas fechadas |
| 1 | fala do Tom | Sepikmon |
| 2 | passo 10 (Suzaku Badge) | Igor (conselheiro de DNA) e o guardião do Demon Bell |
| 3 | passo 16 (The real Leader) | Gennai (quest do Blast) |
| 4 | passo 22 (Free Asuka City) | Etemon e o porco |
| 5 | Oikawa vencido (ou save já além do passo 27) | Black Market |

Os passos 10/16/22 foram escolha minha (espaçar pela campanha); ajustar na tabela `steps` se o dono quiser outros.

## Já existe (outra sessão; pronto, falta commitar)

- Tutorial de caça: em Seiryu City (passo 4+), na primeira vez, a sala puxa o jogador e o Sepikmon dá a caça do
  Ogremon no Central Park. O gancho do passo 4 **reaproveita isso**, trocando o puxão direto pela história do Líder
  sumido e pela missão no GUIDE.

## Ordem de trabalho

1. Mecânicas do Blast/Chosen: boost, superaquecimento, venda das melhorias.
2. ~~Gancho do passo 4~~ feito: gatilho na fala do Tom, porta na escada dele, cena do Igor → corte → Sepikmon, nível da sala.
3. Igor na sala + gancho do passo 0 + falas de conselheiro de DNA (olha a party e o que já está desbloqueado).
4. ~~Arco do Oikawa~~ feito (`definitive_story.c`, `oikawa_hook`). Bulbmon vencido, o evento 711 põe o passo 27 e
   deixa o jogador no salão do Bug Maze Pit (0x245, 1800,524); o encapuzado (ator 42 com a arte do vendedor por cima,
   sem o "4") está dois passos acima à esquerda, chama o jogador e luta: Arukenimon Lv28, Mummymon Lv30,
   VenomMyotismon Lv32 (Dinohyumon e MaloMyotismon não existem no disco). Sem fuga; derrota sem game over (a party
   volta como entrou e ele chama de novo). Vencido: se apresenta como Oikawa e some; o sonho do Ato II espera isso
   naquele mapa. GUIDE: o passo 27 mostra "The hooded man" enquanto ele não cai; depois, a missão "The Black Market"
   até o Oikawa abrir o mercado atrás do balcão (TALK05 99 na primeira vez, `DEF_STORY_MARKET_OPEN`). Nível 5 da sala:
   Oikawa vencido, ou save já além do passo 27. Falas: TALK06 5..7. Falta o dono ver na run o sprite (encaixe de
   VRAM 6 do mapa 0x245, do ator 78, que sai enquanto o Oikawa espera) e a posição.
5. ~~Gatekeeper do passo 40~~ feito (`port/game/definitive_time_room.c`, `tools/gen_time_room.py`). No Master Room
   de Asuka (mapa 0x288) o Warp Gate fica fechado até vencer a Sala do Tempo; pisar nele traz a voz do Igor (TALK03
   311 sem a Chosen: termine o caminho do Gennai ou prove sua força; 312 com ela). A porta azul fica no recanto à
   esquerda da plataforma (160,440). Dentro: Igor (abre lembrando a resposta do Ato II, TALK05 47..50), Gennai (ou o
   Myotismon disfarçado, se a quest do Blast ainda engana), o Sepikmon e o Oikawa. A porta de cima leva às 3 provas,
   uma por região do jogo com o fundo azul (dono, 2026-11-17): Oikawa no meio da Ponte de Asuka (0x202: Mummymon,
   Arukenimon, VenomMyotismon, Lv56..60), o Sepikmon na praça oeste de Genbu City (0x26F: Gryphonmon,
   SkullMammothmon, Kimeramon, Lv60..64), o velho no meio do Mobius Desert (0x258; Gennai: Valkyrimon,
   Knightmon, Zanbamon; Myotismon: Puppetmon, Machinedramon, Piedmon; Lv63..67). Cada prova é uma batalha de chefe
   com os 3 entrando em sequência, sem fuga, sem prêmio (sem tela de resultados). Perder (sem game over, a party volta
   como entrou) ou sair da área recomeça da primeira. Vencer as 3 liga `DEF_STORY_GATE_PASSED` e o warp abre.
   Posições medidas no mapa de atributos (`DW3_TIME_TEST=map:x:y:passo` + o levantamento no log); falta o dono ver
   na run em inglês se as posições e os sprites ficam bons.
6. Reescala dos chefes finais.
