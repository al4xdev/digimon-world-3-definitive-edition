# Sonhos do Igor e história mais séria (pedido do dono, 2026-11-17)

Continuação de `velvet-historia.md`. Reaproveita a estrutura do sonho da abertura (`dw3_velvet_pull_by` com
`DW3_VELVET_PULL_IGOR`: som do portal, sala só com o Igor, volta ao ponto exato do mapa). Só mudam as falas.

## Pedidos do dono nesta rodada

1. **Hóspedes:** o Gennai entra no passo 10 com o Igor (o começo do jogo é arrastado); o Guilmon do Demon Bell vai para
   o passo 16 (inverter os dois).
2. **Sonho desalinhado:** na sala do sonho o Junior ficava de costas para o Igor. Quem chama vira o jogador para si; no
   sonho o jogador entra lado a lado com o Igor, de frente para ele.
3. **Sonhos fáceis de disparar:** uma tabela de sonhos (passo, condição, fala do Igor, escolha). Novo sonho = uma linha
   da tabela + falas no `pt.txt`/`en.txt`. Pode haver sonhos extras para dar ritmo em trechos parados.
4. **Múltipla escolha:** escolhas que não mudam eventos, mas ficam guardadas em sequência (um byte por escolha no save) e
   podem ser lembradas depois (o Ato III cita a resposta do Ato II).
5. **História mais séria (modelo menor, Sonnet):** expandir diálogos infantis/simples para algo mais sério, **sem mudar
   ordem nem quests**; acrescentar escolhas que não mudam eventos; gênero mais neutro (ajuda o modo Kaili, Girls
   Power); achar as falas da Kaili sobre o irmão (Kurt) e, no modo Kaili, fazer delas falas da heroína, sem o Junior
   (NPC) falar do irmão como se fosse dele.

## Os três atos (texto do dono, guardado como veio)

### Ato I — O Engano da Game Master / A Farsa do Lucky Mouse (início-meio de Asuka)

- **No original:** a Game Master manipula o Junior/Kaili (e os jogadores presos no Asuka Server), fazendo todo mundo
  acreditar que o hacker Lucky Mouse é o terrorista responsável pela crise, quando na verdade é a própria A.o.A.
  operando por trás das cortinas.
- **O puxão:** logo após a cena em que a Game Master planta essa narrativa, o Igor puxa o protagonista para a sala azul:
  > "Os olhos frequentemente enxergam apenas o reflexo projetado na parede... Nem toda máscara pertence ao verdadeiro
  > culpado. Observe além do que lhe dizem para odiar."
- **Efeito:** ao voltar, o protagonista ignora o aviso (jovem herói impulsivo) e segue caçando o Lucky Mouse: ironia
  dramática para o veterano e terreno para o arco do Oikawa (também manipulado).
- **Escolha:** só opções em que o protagonista segue em frente (todas levam ao mesmo caminho); a resposta é guardada.

### Ato II — A Travessia para o Amaterasu Server (O Peso do Fardo)

- **No original:** depois da conspiração da A.o.A. e do bloqueio do Oikawa na fronteira, o protagonista cruza para o
  Amaterasu Server (o sistema caiu nas mãos dos generais da A.o.A.; os jogadores viraram Oinkmon).
- **O puxão:** no instante em que cruza o portal entre servidores:
  > "Além deste limiar não há mais retorno fácil. A ruína de um mundo inteiro repousa agora sobre os ombros de uma única
  > criança... Você escolhe carregar esse fardo por vontade própria?"
- **Efeito:** peso real à entrada no segundo ato, ligando a derrota melancólica do Oikawa à responsabilidade assumida.
- **Escolha:** guardada; o Ato III lembra dela.

### Ato III — A Falha no Gunslinger / Despertar do Galacticmon (O Momento do Abismo)

- **No original:** no clímax no Gunslinger, o protagonista invade o núcleo para impedir o Vemmon, mas chega tarde: o
  Vemmon absorve o satélite e se funde em Galacticmon, deixando o protagonista impotente antes do confronto final.
- **O puxão:** no auge dessa falha:
  > "Então até a esperança mais brilhante pode vacilar diante do inevitável... O destino já parece selado acima das
  > estrelas. Diga-me, jovem viajante: você vai aceitar a derrota e desistir aqui... ou forjará um milagre mesmo na
  > beira do abismo?"
- **Efeito:** transforma a derrota canônica no despertar da Chosen Evolution. O Igor lembra a resposta do Ato II antes
  de perguntar, e a resposta dele lembra a Sala do Tempo já vencida.
- **Sala do Tempo (decisão do dono, 2026-11-17):** fica no passo 40, **antes** do Warp Gate para o Gunslinger, porque
  o jogo leva a uma área sem volta e a ideia é travar isso para o jogador não se frustrar
  (`port/game/definitive_time_room.c`). O Igor lembra a resposta do Ato II ao abrir a Sala do Tempo.

## Onde cada ato cai (achado no código)

| Ato | Gatilho | Passos válidos |
|---|---|---|
| I | primeiro quadro livre com a flag 0x1C0E (fim do evento 351 do WSTAG305: a Game Master acusa o Lucky Mouse, texto DMG305:1) | 12..18 |
| II | primeiro quadro livre no passo 27 (Amaterasu Server) | 27 |
| Oinkmon (dono, 2026-11-17) | primeiro quadro livre no passo 0x26: o evento 935 do WSTAG226 (mapa 0x276, DMG200:41: Teddy e Kurt ainda Oinkmon, "Wait here. I'll look into it.") termina no passo 0x26 e deixa o jogador livre no mapa 0x270. O Igor lembra a resposta do Ato I e pergunta o que sustenta o herói agora que está sozinho (pergunta 3, `$Dr4*`). O passo dura até os eventos do Master Room (0x27); a máquina de Qing Long fica dentro dele (flag 0x1A0A) | 0x26 |
| III | primeiro quadro livre no passo 0x2B no mapa 0x2DF (fim do evento 1080 do WSTAG820: Snatchmon se funde com o Gunslinger, DMG800:17). No jogo o evento termina ligando a flag 0x7401, que já começa a batalha do Galacticmon: não sobrava quadro livre e o sonho nunca vinha. O port segura a batalha (`def_story_hold_galacticmon`) e a começa depois do sonho respondido (`galactic_hook`); o gatilho do evento 1080 fica fechado daí em diante | 0x2B |

`DW3_DREAM=<n>` sonha a linha n da tabela no próximo quadro livre (teste).

## Como está feito

- `definitive_story.c`: tabela `dreams` (passo, flag, falas do Igor, opções, respostas, lembrança de uma resposta
  anterior); o puxão é o da abertura (`dw3_velvet_pull_by`, Igor sozinho, lado a lado); a volta é ao ponto exato.
- Painel de escolha genérico (`choice_*`), usado pelos sonhos e pelas cenas.
- Respostas: `def_record_story_answer` (segundo registro, logo depois do Blast): 40 bytes, 0..7 sonhos, 8..39
  escolhas das cenas. 0 = não respondida.
- Camada da reescrita: `tools/text_override/en_story.txt` (linhas novas em inglês), `en_kaili.txt` (modo Kaili;
  `>hero ` move o balão para a heroína), `en_choices.txt` (`Q<n> FILE:sub index@k: rótulo | fala`). Gerados em
  `port/game/definitive_story_text.c`. Só em inglês por enquanto; `DW3_STORY=0` mostra o texto original.
- `tools/check_story_text.py`: confere placa, códigos, largura (22 nos balões, 38 na narração), 3 linhas por página,
  estrutura das páginas e palavras de gênero sobre o herói.

## Estado

- [x] Gennai no nível 2 (passo 10), Guilmon no nível 3 (passo 16)
- [x] Jogador virado para quem chama; cenas do Igor lado a lado
- [x] Tabela de sonhos + painel de escolha + respostas no save (testado: Atos II e III, lembrança do II no III)
- [x] Atos I, II, III em inglês (PT depois, com a tradução da história)
- [x] Escolhas nas cenas (testado na abertura com uma escolha de teste, já removida)
- [x] Bíblia da história (Sonnet): `.dumps/tasks/story/rewrite/bible_events.md`, `bible_talk.md`
- [x] Reescrita (Sonnet, por capítulo) + verificação: 2724 linhas da história, 138 do modo Kaili, 96 opções (Q8..Q39)
- [x] Revisão adversária (`.dumps/tasks/story/rewrite/review.md`, 17 achados) aplicada direto em `en_*.txt`
  (`apply_review.py`). Atenção: o `merge.py` reescreve os `en_*.txt` a partir de `out/`; não rodar de novo sem
  repetir o `apply_review.py`.
- [x] Variantes do modo Kaili (o Kurt irmão da heroína; a amiga vira o Junior), incluindo a primeira cena da Kail
  (DMG200:3/4/5)
- [ ] Ato I visto no jogo pelo gatilho real (a flag); Atos II/III idem (só testados com `DW3_DREAM`)
- [x] Sala do Tempo no passo 40, antes do Warp Gate (ver `velvet-historia.md`)
- [x] Ato III: a batalha do Galacticmon espera o sonho
- [ ] Tradução para PT de tudo que é novo (o dono traduz)
- [x] Nomes no modo Girls Power (decisão do dono, 2026-11-17): os papéis trocam. A heroína é a Ivy no mundo real e o
  nick no Digimundo; o Kurt é irmão dela; o amigo é o Junior (menino) nos dois mundos (guardou o nome real como nome de
  conta). Em inglês toda fala sem linha própria troca sozinha (`tools/gen_text_override.py`: Junior -> Ivy, Ivy e Kail
  -> Junior; o Smith Junior fica). À mão em `en_kaili.txt`: as do irmão, as de "he/him" (DMG700:0 2, DMG200:32 3,
  DMG600:15 18/20/23) e a piada do nome de conta (DMG200:3/4/5 7-8). O menu diz KAIL (não Kaili). Os `$OpenIvy1K` /
  `$OpenJuniorK` saíram: a troca automática faz os `$Open*`.
  - PT por enquanto só troca Kail -> Junior. Na rodada de tradução: trocar Junior/Ivy no mundo real também, com os
    artigos e as concordâncias ("a Ivy" -> "o Junior", "sozinho" -> "sozinha"), e passar a Kail capturada para "ele".
- [ ] Testes de história no jogo: depois (o dono pediu para não gastar cota com screenshots agora)

## Próximas tasks (ordem combinada com o dono, 2026-11-17)

Primeiro terminar todas as features; os testes são do dono (uma run em inglês); a tradução PT só quando o dono decidir.
O `pt_kaili.txt` (3 falas do irmão em PT) foi um passo adiantado: fica, e entra na tradução quando ela vier.

1. **Mais escolhas na história (Sonnet).** Hoje são 32 pontos (Q8..Q39, 96 opções) e os slots acabaram (40 bytes,
   um por resposta). Guardar cada resposta em 2 bits (0 = não respondida, 1..3) dá 160 escolhas nos mesmos 40 bytes;
   depois o Sonnet espalha mais escolhas pelos trechos sem nenhuma e escreve falas que lembram respostas antigas
   (hoje só o Ato III lembra o Ato II).
   Pedido do dono: as escolhas têm de mudar diálogos mais à frente (falas que dependem da resposta). História
   adulta, mas não pesada; nada infantil (o jogo original pecava nisso). Pode aumentar as falas (mais páginas na
   mesma entrada) para explicar coisas ou criar teorias da lore do jogo.
2. ~~**Sala do Tempo**~~ feita: no passo 40, antes do Warp Gate (o dono corrigiu: antes da área sem volta).
3. **Arco do Oikawa** (passo 26 -> 27, o portão do Black Market).
4. Testes do dono (run em inglês), depois a tradução PT de tudo, depois o commit (separando os arquivos da outra
   sessão: música e rebalanceamento; o `definitive_buffs` não compila por falta do stub de `def_recipe_savestate`).
