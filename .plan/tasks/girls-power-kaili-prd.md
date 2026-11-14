# Girls Power: Kaili como protagonista

Origem: `definitive-rebalance-prd.md` § "Girls Power". Decisões do dono (2026-11-14):
- **Trocar os papéis:** a Kaili antiga vira o Junior.
- **Arte nova:** a escolha técnica ficou comigo.
- **Entrega:** tudo de uma vez.
- **Texto:** nenhum muda. A Kaili se chama "Junior" e o Junior se chama "Kaili".
- **Onde:** no próprio jogo, não no memory card.

## Estado: feito; falta o playtest do dono do começo ao fim

### O que mudou no disco (censo de 2026-11-14)

Os personagens do campo são sprites 2D de 4 bits (`FIELD/SPRT/P###xxxx.BIN`, `docs/FORMATS.md` "Actor sprites"). A
paleta exata do Junior (16 cores) foi procurada nos 2382 arquivos do disco. Ela aparece só nestes:

| Arquivo | O que é | Tratamento |
|---|---|---|
| `P001PLAY` (0x186) | Junior das cenas (ator 1), 27 animações | vira Kaili |
| `P002PLAY` (0x187) | Junior controlável (ator 2) | vira Kaili |
| `P002PLYD/F/K/L` (0x3C9–0x3CC) | ações: `PLYD`, pesca, chute, `PLYL` | vira Kaili |
| `P011MINO` (0x190), `P012MINT` (0x2F8) | Kaili NPC (atores 11 e 178) | vira Junior |
| `NAMEENT/STNMETTM` (0x28B), `SCREEN/NAMEDTTM` (0x771) | páginas de VRAM das telas de nome, confirmação, nome do Digimon e deck, com o quadro 85 do Junior de frente | recebem o quadro parado de frente da própria Kaili e a CLUT dela |

A premissa original ("a Kaili tem quase todos os sprites") não se confirmou: ela tem 9 animações, contra as 27 dele mais
as ações. Por isso a solução não aponta o ator para o arquivo dela. Ela **repinta os quadros dele**.

### Como funciona (`port/game/girls_power.c`)

- **Quando:** assim que o `cdload` termina de ler um dos 8 arquivos (`src/main/cdload.c`, transição 2→3), cada quadro
  que as tabelas de animação citam é repintado no próprio buffer.
- **A cabeça:** acha a cabeça (o boné dele ou o cabelo dela) e decide a direção pela pele do rosto (frente, 3/4,
  perfil, costas e o lado). Corta a cabeça e cola a do outro, da mesma direção, alinhada pelo queixo.
- **A roupa:** recolorida com uma tabela de 16 índices, e a CLUT passa a ser a do outro.
- **O que não muda:** animações, tempos e scripts continuam os do jogo. Por isso o chute, a pesca e todas as cenas
  funcionam sem tradução de animação.
- **Referências:** as cabeças vêm dos quadros parados originais, lidos direto do disco (`port_disc_read_user`, novo no
  `psxstack/runtime/disc.c`).
- **Telas de menu:** `tim_load_all` (`src/main/tim.c`) chama `port_girls_tim` antes de cada envio. A página que tem o
  quadro 85 do Junior e a CLUT dele recebe o quadro da Kaili (animação 1, direção 0) e a CLUT dela, reconhecidas pelo
  conteúdo.
- **Liga e desliga:** pergunta "CHOOSE YOUR HERO!" no New Game, logo depois da dificuldade e no mesmo painel
  (`src/stdwtitl/stdwtitl_800852F0.c`, passo 11; X volta para a dificuldade). Kaili vem primeiro. A escolha vai para
  o memory card como `DEF_SAVE_KAILI` (o registro extra do slot, como o Hard Mode). Saves antigos ficam com o Junior.
  `port_girls_refresh` (New Game, leitura do save no `stgmcard`) solta do cache os 8 arquivos se a heroína mudou.
  Desligado sob script de replay. `DW3_GIRLS=0/1` força. As letras J e P foram adicionadas à fonte de polígonos
  (`definitive_callout.c`).
- **Build de matching:** intocado (tudo sob `PC_PORT`).

### Verificado

- **C igual ao protótipo:** o C é idêntico ao protótipo em Python (`tools/girls_power_preview.py`) nos 465 quadros dos
  8 arquivos (`DW3_GIRLS_DUMP=<dir>` grava os arquivos repintados).
- **Teste:** `tests/port/girls_power.py`, na camada `port` do `scripts/test.sh`. Em todo quadro, a CLUT é a do outro, o
  quadro mudou e tem a cabeça do outro. Sob script fica desligado.
- **Replays:** `tests/port/run.py` passa (`first_battle_save` e `new_game` sem mudança).
- **No jogo (capturas em `.dumps/tasks/girls-power/st/`):**
  - a Kaili anda no campo em várias direções;
  - na rua de Kusanagi aparecem Kaili, Junior (a antiga Kaili) e Takuya juntos;
  - na tela de confirmação do Digimon Online aparece a Kaili.

### Para o playtest (o que olhar)

- **Poses deitadas** (anims 16, 38 e 39 das cenas): a cabeça colada fica em pé num corpo deitado. É legível, mas é o
  ponto mais fraco.
- **Quadros 3/4 de costas** com chapéu inclinado: um ou outro pode sobrar com um pixel do boné.
- **Chute em árvore, pesca e as ações `PLYD`/`PLYL`:** foram verificadas quadro a quadro nas folhas, mas não jogando.
- **Tela de deck e nome do Digimon:** usam a mesma página `0x771`, mas não foram vistas rodando.
- **Fora do censo:** qualquer outro lugar com o Junior. O censo pela paleta exata não achou mais nenhum, mas uma
  imagem com outra paleta passaria.

Para afinar o algoritmo: `tools/venv/bin/python tools/girls_power_preview.py <dir>` gera as folhas antes/depois. Mude
o Python, compare a olho e depois replique no C. `DW3_GIRLS_DUMP` mais o teste confirmam que os dois batem.
