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
| 4 · The Seiryu Badge | Seiryu City, antes do Líder | Dizem que o Líder (o treinador) sumiu, em vez de mandar às Protocol Ruins. Junior sente algo estranho. Quest no GUIDE: Velvet Room → Sepikmon → caça do Ogremon (feita para isso) → falar com o Sepikmon de novo → volta à quest principal |
| 1..39 | campanha | Blast amaldiçoado; o "Gennai" vende melhorias do boost a cada forma nova |
| 26 → 27 | passagem Asuka → Amaterasu (Bug Maze Pit, ~nv 30) | Oikawa bloqueia (sprite encapuzado sem o "4" de debug), luta (Arukenimon + Dinohyumon + MaloMyotismon, nv 30), libera a passagem e some. Missão no GUIDE: Velvet Room; o Oikawa atrás do balcão abre o Black Market (fechado até aqui) |
| 40 · No way back | antes do warp para Gunslinger | Uma porta da Velvet aparece ali. Dentro, os 3 NPCs (o velhinho é o Myotismon sem a Chosen, o Gennai com ela). A porta de cima da sala leva a 3 áreas clássicas do jogo com fundo azul, uma por NPC; luta um a um. Recuar ou perder: recomeça do primeiro. Sem passar, o warp não abre. Aviso antes: sem a Chosen, recomenda terminar a Velvet Room |
| 41..44 | Gunslinger e o fim | chefes reescalados para a Chosen |

## Já existe (outra sessão; pronto, falta commitar)

- Tutorial de caça: em Seiryu City (passo 4+), na primeira vez, a sala puxa o jogador e o Sepikmon dá a caça do
  Ogremon no Central Park. O gancho do passo 4 **reaproveita isso**, trocando o puxão direto pela história do Líder
  sumido e pela missão no GUIDE.

## Ordem de trabalho

1. Mecânicas do Blast/Chosen: boost, superaquecimento, venda das melhorias.
2. Gancho do passo 4 (Líder sumido → Sepikmon → Ogremon → volta).
3. Igor na sala + gancho do passo 0 + falas de conselheiro de DNA (olha a party e o que já está desbloqueado).
4. Arco do Oikawa (passo 26 → 27) + trava do Black Market + missão no GUIDE.
5. Gatekeeper do passo 40: porta, os 3 NPCs, as 3 áreas azuis, trava do warp.
6. Reescala dos chefes finais.
