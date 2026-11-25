# Reescrita Narrativa PT-BR ("Dublagem Clássica 80/90") + Escolhas com Peso Real

> **Status:** Alinhado com o dono (2026-11-25) — pronto para iniciar a **Fase 0 (Infra C)**.  
> **Escopo inicial de idioma:** 100% **PT-BR** primeiro (`pt_story.txt`, `pt_choices.txt`, `pt_echoes.txt`, `pt_kaili.txt`, `pt.txt`). Propagação para EN e demais idiomas somente após aprovação final do PT-BR.

---

## 1. Diretrizes Criativas & Tom

- **Tom (Equilíbrio Yu Yu Hakusho):** humor ácido, gírias clássicas dos anos 80/90, timing cômico e personalidade forte no dia a dia e nos NPCs; **drama firme, coerente e com nexo** nos momentos sérios (A.o.A, Velvet Room, clímax e viradas da trama).
- **Dar vida aos NPCs ("mini-arcos"):** mapear NPCs secundários cujas falas evoluem com `gamestate_data.progress` ao longo da campanha e transformá-los em **mini-arcos contínuos** (ex.: o fã do Patamon em Asuka que recebe uma dica sua no início e aparece evoluído/reagindo mais tarde).
- **Regra das 3 opções + omissão natural:** toda escolha explícita tem **3 opções (1, 2, 3)**. O estado **`0` ("não falou / ignorou")** funciona automaticamente como o **4º desfecho** — se o jogador passar batido naquela fase, o NPC se vira sozinho naquele arco e suas falas futuras refletem isso.
- **Kaili & Nomes:** manter compatibilidade com o modo Kaili (`pt_kaili.txt`). Adaptações culturais BR na medida certa, preservando nomes oficiais de Digimons, golpes e lugares do jogo.

---

## 2. Arquitetura Técnica — Fase 0 (Infraestrutura C)

### 2.1 Expansion Save Narrativo (`port/game/definitive_story_save.c` + `definitive_expansion_sections.c`)
- Migrar o armazenamento de escolhas do limite de 160 × 2 bits (`story_answers[40]` no record 2 do memory card) para uma seção dedicada `story` no **Expansion Save** (mantendo leitura/migração transparente de saves antigos).
- Estrutura persistida:
  - **Respostas expandidas (`Q0..QN`)**: sem limite prático (ex.: 512+ perguntas, 2 bits cada: `0 = ignorou/não falou`, `1..3 = escolha`).
  - **Memória de mini-arcos de NPCs**: flags de estágio / se o jogador já leu a fala atual do NPC naquela fase da campanha (alimenta o indicador visual na cabeça do NPC).
  - **Afinidade individual** (signed int8/int16) com os 5 aliados principais: **Teddy, Ivy, Lisa, Keith, Kurt**.
  - **Reputação global** (ex.: reputação geral / eixos de postura para reações nas cidades e com rivais).
- Integrar com `def_expansion_savestate` e `def_story_savestate`.

### 2.2 Motor de Echoes & Condições (`port/game/definitive_story.c` + `tools/gen_text_override.py`)
- Ampliar `DefStoryEcho` para suportar:
  - Condição `answer == 0` (caminho "ignorou/não falou na fase anterior").
  - Condições opcionais de **afinidade** (ex.: `ally >= X` ou `ally <= Y`) e **reputação global**.
- Permitir que escolhas declarem efeitos colaterais de dados (delta de afinidade por aliado e delta de reputação) diretamente no formato de `pt_choices.txt` / compilador `gen_text_override.py`.

### 2.3 Indicador Visual de Fala Nova nos NPCs (`port/game/definitive_story.c` / `fieldstg`)
- Desenhar um **balãozinho de pensamento / ícone de fala nova** acima da cabeça de qualquer NPC de mini-arco que tenha uma fala/etapa nova disponível no `progress` atual e ainda não lida pelo jogador.
- Ao conversar com o NPC, marcar aquela etapa como vista no *expansion save* e ocultar o indicador até a próxima fase em que o mini-arco avançar.

### 2.4 Mensageria Síncrona de Batalha (`port/game/definitive_battle_prompt.c`)
- Generalizar o padrão de pausa/HUD acinzentada de `definitive_flee_ask.c` + `definitive_callout.c`:
  - Trava o turno da batalha, escurece/acinzenta a HUD, exibe fala de chefe/aliado/rival + **2–3 opções de resposta** (ou confirmação).
  - **Consequência mecânica real (buff OU debuff):** dependendo da escolha (e/ou afinidade com o aliado), aplicar **buffs ou debuffs** imediatos na luta (ATK, DEF, SPD, Blast Gauge, TP/MP, etc.) — escolhas ruins/desastradas ou afinidade baixa podem gerar **debuff**.
  - Gravar a resposta no *expansion save* para repercutir no diálogo pós-luta, afinidade e echoes futuros.
- **Escopo de disparo:** batalhas-chave da história (Chefes, Líderes de Ginásio, A.o.A, Rivais e Velvet/Super Hunt), com **1 a 2 momentos** por luta.

---

## 3. Fases de Reescrita Narrativa (PT-BR direto, por arco)

Antes de editar os arquivos de texto, criar uma **Bíblia de Tom & Voz** curta (perfil de fala de Junior/Kaili, Teddy, Ivy, Lisa, Keith, Kurt, Igor, Etemon, vilões da A.o.A e catálogo de gírias/bordões).

| Fase | Arco / Região | Escopo Principal |
|---|---|---|
| **Fase 0** | **Infraestrutura C** | Expansion save narrativo, motor de echoes/afinidade, ícone de fala nova em NPCs, prompt síncrono de batalha com buff/debuff + testes |
| **Fase 1** | **Prólogo & Asuka City** | Abertura + Velvet Room, chegada em Asuka, coerência inicial da trama, 1ª leva de mini-arcos de NPCs de Asuka, 1–2 batalhas com prompt síncrono |
| **Fase 2** | **Setor Leste & Seiryu** | Busca pelo Líder de Seiryu, Velvet Room nível 1, Protocol Ruins, mini-arcos de NPCs do Setor Leste + continuação dos NPCs de Asuka |
| **Fase 3** | **Setor Central & Sul (Suzaku)** | Expansão do conflito com a A.o.A, rivalidades, ginásios, evolução dos mini-arcos e batalhas-chave com escolhas |
| **Fase 4** | **Setor Oeste (Byakko) & Deserto** | Reviravoltas da trama, laços com aliados (afinidade pesando em diálogos e batalhas), mini-arcos intermediários |
| **Fase 5** | **Setor Norte (Genbu) & Queda de Asuka** | Aumento de tensão dramática (tom firme estilo arco final de Yu Yu Hakusho), escolhas de alto impacto e consequências nos NPCs |
| **Fase 6** | **Servidor Amaterasu, Clímax & Epílogo** | Resolução dos mini-arcos de NPCs, payoffs de afinidade/reputação nas batalhas finais, Velvet Room e epílogo |
| **Fase 7** | **Localização Final (pós-aprovação)** | Exportar estrutura final para EN, FR, DE, IT e ES |

---

## 4. Próximo Passo Imediato

Iniciar a **Fase 0 — Infraestrutura C**, começando por:
1. Seção `story` no Expansion Save (`definitive_story_save.c` + migração automática de `story_answers[40]`).
2. Extensão do `gen_text_override.py` e `definitive_story.c` para `Q0..QN`, `answer == 0`, afinidade e reputação.
3. Indicador visual sobre NPCs com fala nova no `FIELDSTG`.
4. Prompt síncrono de batalha (`definitive_battle_prompt.c`) com suporte a buffs e debuffs.
