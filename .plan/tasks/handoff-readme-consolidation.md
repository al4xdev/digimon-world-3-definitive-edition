# Handoff: consolidar planos e handoffs no README.md e limpar o repo (2026-11-18)

Objetivo do dono: fechar o escopo da 1.0 e apagar `.plan/` e os handoffs. O `README.md` não é um README normal: é o
dump de tudo que foi feito, para ninguém esquecer nada. Antes de apagar qualquer plano, todo fato duradouro que só
existe nos planos tem de estar no README. Idioma com o dono: pt-BR. README em inglês, estilo de referência seca.

Este arquivo também será apagado no fim (é um handoff).

## Onde está tudo

`.dumps/readme-merge/` (gitignored, local; não apagar até acabar):

- `brief.md`: as decisões finais do dono e as regras de trabalho dos agentes (leia primeiro, na íntegra);
- `gap_A.md` caça/economia/DRI · `gap_B.md` Velvet · `gap_C.md` combate/progressão · `gap_D.md` super raros/forja/DRI/skins/
  Girls Power/Guia · `gap_E.md` planos fechados de plataforma e menu de batalha · `gap_F.md` launcher/Android/handoffs ·
  `gap_G1.md` código de `port/game/` contra README · `gap_G2.md` tools/testes/launcher/mods/CI contra README.

Cada relatório tem: MISSING com texto pronto em inglês e a seção de destino, PARTLY, CONTRADICTED, OPEN ITEMS.
Os agentes que escreveram os relatórios eram só leitura. Os que editam o README devem seguir o `brief.md`.

## Decisões do dono (resumo; o detalhe está no brief)

1. Acrescentar todos os fatos que faltam e corrigir todos os trechos errados (o código vence o relatório).
2. Incorporar tudo dos relatórios de código (G1, G2): tabela de variáveis de ambiente, ferramentas, `scripts/`, telas e
   chaves do launcher, CI, lista de testes, camada de história.
3. REMOVER do README (sem registrar como decisão): widescreen do portátil pendente/não verificado; campos elementais e
   Numemon (nunca feitos, fora da 1.0); toda a documentação de Android (o dono continua o Android em paralelo e
   reescreve depois; `android/` não se mexe).
4. NÃO acrescentar: checklist de playtest pré-1.0, traduções PT pendentes do texto novo da Velvet/história, "chefes
   finais reescalados para a Chosen", submissão ao PortMaster e teste em segundo aparelho (o dono guarda em notas dele).
5. Itens de gameplay não testados entram na lista "Not tested in a real game"/"Pending" do README.
6. "Falta commitar" só se confirmado no `git`: o dono diz que Black Market, forja e Cores de réplica já estão no jogo e
   commitados (o lote 1 já citou os commits reais).
7. Plano 9, item 6 (o menu de batalha lê o pad por dois caminhos, `fightstg_8008D3B4.c` ~4183 e ~6164): fica como está; o
   dono considera os inputs perfeitos. Itens 1, 3, 7 e 9 estão feitos no código.
8. Ferramentas de música movidas para `tools/music_lab/` (já feito, ver abaixo).
9. Pode apagar todos os planos antigos e os handoffs no fim (movendo para `/tmp/`, regra do `CLAUDE.md` do dono; só `rm`
   se ele disser "apaga" — ele já disse "pode apagar", mas mover para `/tmp/` continua sendo o jeito seguro).

## Estado em 2026-11-18

Feito:

- **Lote 1** (caça, economia, combate, forja, DRI, skins, Girls Power): aplicado e verificado pelo agente.
- **Lote 2** (Velvet, nova seção `## The story layer (Velvet Room in the main story)`, música da Velvet e da Adventure,
  Blast, estados desatualizados): aplicado e verificado pelo agente.
- 4 menções ao `dw3-menu.py` (removido em `cbf34ce`) corrigidas no README.
- Parágrafo do ícone de elemento em toda lista de técnicas no README (feito na parte do código, abaixo).
- `tools/music_lab/velvet/` e `tools/music_lab/adventure/` (cópia das ferramentas de `.dumps/velvet-live/` e
  `.dumps/adventure-music/`, caminhos ajustados, `py_compile` ok, estado de sessão no `.gitignore` da pasta). Não rodei
  uma sessão de áudio de verdade, só a importação.
- Os dois staging (aarch64/muOS e tradução PT-BR) já foram movidos para `/tmp/dw3-plan-removed/` a pedido do dono.

**Lote 3: INTERROMPIDO no meio** (o dono pediu para pausar). O README tem 2703 linhas (eram 2102 no começo da sessão);
não se sabe o que do lote 3 já entrou: ainda há 18 menções a "android" no README, então a remoção de Android provavelmente
não terminou. Rode `git diff README.md` e compare com a lista abaixo antes de retomar; os edits são pequenos e
independentes, nada deve estar corrompido, mas confira.

Nada foi commitado. A árvore tem trabalho de outras frentes misturado (ver "Commit").

## O que falta fazer, em ordem

### Passo 1: terminar o lote 3 (um agente `opus`, só ele edita o README por vez)

Relatórios: `gap_E.md`, `gap_F.md`, `gap_G2.md`, e a tabela de variáveis de ambiente e os itens 4.10/4.11 de `gap_G1.md`.
Áreas do README:

- Ferramentas: `tools/music_lab/` (o que cada script faz e como rodar: `tools/venv/bin/python
  tools/music_lab/velvet/play.py`, `... play.py adventure`, `solo.py`, `piano.py`, `adventure/guitar.py`),
  `gen_time_room.py`, `check_story_text.py`, `cc_psx.sh`, `psyq_compare.sh`, e os 15 scripts de `scripts/`.
- Testes: lista completa (`sound.py`, `definitive_velvet_music.py`, os `*_driver.c` agrupados, escopo do `mods.py`);
  corrigir "cinco testes não registrados" que nomeia seis (dri, market, economy, variants, quest, rush).
- Variáveis de ambiente: TODAS as `DW3_*` que o código lê, com arquivo. As 8 que faltam: `DW3_STORY`, `DW3_DREAM`,
  `DW3_TIME_TEST`, `DW3_TIME_SURVEY`, `DW3_VELVET_NOHINT`, `DW3_LOAD_LOG`, `DW3_SKIP_BIG`, `DW3_TOUCH_PAD`.
  Atenção: `DW3_FORCE_VIDEO` NÃO é fantasma (o `handheld/Digimon World 3.sh` exporta); `DW3_BLAST_FORM` já está marcado
  "old". Antes de chamar qualquer uma de fantasma, `grep` o repositório inteiro.
- Launcher: as telas que a lista de "9 telas" omite (VIDEO, SAVES & CLOUD, LOCAL BACKUPS, CLOUD ACCOUNT), chaves do
  `tui.state` (`btext`, `bmem`, `bbuf`, `renderer_pc`, `backup_max`, `sync_enabled`, `sync_interval`), flags de CLI
  (`--fb`, `--pc`, `--fill`, `--splash`, `--shots`, `--detach`: confirmar o significado em `launcher/*.c`), desenho dos
  jobs de CI, `recomplauncher.json`, `packaging/appimage/AppRun`, resumo agrupado dos 436 pontos `PC_PORT` em `src/`.
- Menu de batalha (planos 8 e 9): valores de `FightstgCommand->command` que o teste do plano 8 confere; por que L1/R1 não
  vazam para os submenus; âncora da lista antiga (x=`0x24`, y=`i*19+0x6D`); a passagem "simplification list ... never
  confirmed done" reescrita item a item (1, 3, 7, 9 feitos; 6 aberto de propósito; conferir 2, 4, 5, 8, 10 no código).
- Plano 3 (tabela de custo de quadro), plano 2 (ressalva sobre chefes), plano 1 (volta de GL 3.3 para GLES): se já
  estiverem em `docs/*.md`, só apontar.
- Dados do testador TrimUI no resumo da pesquisa de CFW (plano 11).
- Histórico: a linha ~2324 ainda cita "elemental fields, Numemon": remover.
- Pendências: resultado do teste em aparelho do plano 8 não registrado; detecção simulada e gate de hardware do plano 5
  não registrados; FMV nunca rodado no renderer `gles`; três `TODO: validate` em `handheld/probe.sh`.
- Remover do README: toda passagem de Android (build, touch pad, migração SDL3, SAF, "Show Touch Controls", emulador, APK,
  notas de crash, itens de pendência) e todo texto de widescreen do portátil pendente/não verificado (o bullet "Widescreen
  field (handoff note ...)" e o item sobre o mod de campo para o pacote do portátil). Manter o que é verdade sobre o
  widescreen do PC. Em `docs/LAUNCHER.md` remover só a frase errada de Android SDL2 (~linhas 17-19; o código é SDL3).
- Corrigir: o README manda usar `scp` no deploy, mas `docs/HANDHELD.md:104` diz que o aparelho não tem servidor sftp.
- Se achar mais um item que o dono rejeitou (PortMaster submission, tradução PT, etc.), NÃO acrescentar.

O agente deve citar no final cada faixa de linhas de onde removeu Android.

### Passo 2: verificador independente (um agente `sonnet`, só leitura)

Prompt resumido: ler o `README.md` na íntegra, em partes, e para cada afirmação checável (nomes de função, constantes
`DEF_*`/`DW3_*`, caminhos de arquivo, números, comportamentos, testes registrados em `scripts/test.sh`, ferramentas
existentes em `tools/`) conferir no código. Escrever `.dumps/readme-merge/verify.md` com: (a) afirmações que o código não
confirma (linha do README, trecho, o que o código mostra), (b) caminhos de arquivo que não existem, (c) funções/constantes
que não existem mais, (d) seções que se contradizem entre si, (e) menções restantes a Android, widescreen de portátil,
campos elementais, Numemon, `dw3-menu.py`, `.dumps/`, "uncommitted"/"falta commitar". O dono quer uma lista do que ficou
desajustado para atualizarem juntos: NÃO editar o README nessa etapa, só listar. Pode dividir o README em 2-3 faixas e
rodar 2-3 verificadores em paralelo, cada um com seu arquivo de saída.

Depois: ler os relatórios, mostrar ao dono a lista de divergências e corrigir com ele.

### Passo 3: apagar planos e handoffs (só depois do passo 2 e do ok do dono)

Mover para `/tmp/dw3-plan-removed/` (criar a pasta; não usar `rm`):

- `.plan/tasks/*`, `.plan/closed/*`, `.plan/backlog/*`, `.plan/staging/*` (incluindo o 16-Telemetry, que é de outro
  projeto, e este arquivo), `.plan/.state/` se for só estado local;
- `.dumps/handoff_*.md` (`handoff_mig.md`, `handoff_velvet_music.md`, `handoff_adventure_music.md`).

Antes de mover, confirme que `.plan/` é versionado (32 arquivos no `git ls-files`): mover é uma mudança que o dono vai
commitar; não commitar sem ele pedir. Confirmar também se o `CLAUDE.md` do projeto ou outros arquivos referenciam
`.plan/` (o `CLAUDE.md` global diz que `.plan/` é o diretório canônico de planejamento: não apagar o diretório em si se
o dono quiser manter a estrutura; perguntar).

### Passo 4: ainda do dono (não fazer, só lembrar)

O dono tem em notas dele e faz depois: playtest de tudo que nunca foi visto numa partida real (arco do Oikawa, Sala do
Tempo, Atos I a III pelos gatilhos reais, as 6 luzes "divididas", salvar/carregar do registro HUNT, linha de requisitos da
forja numa loja, ícones de elemento no Digi-Lab), reescala dos chefes finais para a Chosen, tradução PT do texto novo,
Android (SIGSEGV no emulador, APK arm64 com SDL3, overlay de 1280 px, letterbox do mouse no launcher), widescreen no
portátil (se quiser), submissão ao PortMaster. Nada disso entra no README como pendência da 1.0 (ver decisão 4).

Achado ainda sem dono: os textos `$TomGone`, `$Q4sT` e `$M4T` ainda dizem "blue door" (a porta virou luz no cenário).
O dono decide se corrige.

## Commit (quando o dono pedir)

O worktree mistura várias frentes. Separar por trecho (`git add -p`) e commitar em grupos:

1. Ícone de elemento em toda lista de técnicas: `port/game/definitive.c` (`def_tech_icon`), `include/port.h` (declaração),
   `src/fightstg/fightstg_8008D3B4.c`, `src/stgdglab/stgdglab_80085470.c`, `stgdglab_8008C278.c`, `stgdglab_8008D7CC.c`,
   `src/ststatus/ststatus_8008833C.c`. Build verde, `tests/port/run.py`, `battle.py`, `definitive.py` passam. Visto no jogo:
   status da batalha e status de campo. Digi-Lab não foi aberto (a tela de escolher técnicas usa x=`0xDA`, a mais provável de
   precisar de ajuste visual).
2. Overheat do Blast/Chosen e afins (`definitive.c/.h`, `definitive_bestiary.c`, `definitive_purify.c`, `inn.c`,
   `wfightmn_800A6440.c`, `fightstg`): é de outra frente, confirmar com o dono antes.
3. `tools/music_lab/` (novo, não rastreado).
4. `README.md` + `docs/LAUNCHER.md` (a consolidação) e as remoções de `.plan/`/handoffs.

Outra sessão do Claude (`velvet_gates`) trabalhou na mesma árvore e commitou `46424ef` só com arquivos da Velvet Room;
não mexer nos arquivos dela sem avisar.

Atribuição de commit: terminar a mensagem com a linha `Co-Authored-By` indicada pela sessão; seguir a skill `git-commit`.

## Regras úteis para o próximo agente

- Memória automática desligada (`~/.claude/CLAUDE.md`); persistência entre sessões só por skills.
- Mover para `/tmp/` em vez de `rm`; Python do repo com `tools/venv/bin/python`; shell do dono é fish.
- Subagentes: passar `model` explícito (`haiku` busca, `sonnet` leitura longa/crítica, `opus` implementar).
- Só um agente edita o `README.md` por vez; usar a ferramenta Edit com âncoras pequenas e únicas, re-`grep` antes de cada
  edição (as linhas mexem a cada lote).
- Nenhuma afirmação sem evidência: cada número, nome ou comportamento escrito no README foi conferido no código; onde o
  relatório discorda do código, o código vence.
