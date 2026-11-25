# Revisão da Fase 5 — Setor Norte / Genbu City & Queda de Asuka (PT-BR)

> Documento para revisão antes da **Fase 6 (Servidor Amaterasu, Clímax & Epílogo)**.  
> Todas as falas seguem a **regra de gênero neutro** para o protagonista e evitam NPCs de Card/Digimon Battle.

---

## 1. Continuação dos NPCs Antigos — Etapa 2 em Seiryu & Suzaku (`progress 25+`)

Ao voltar por Seiryu e Suzaku no auge da crise da A.o.A, o balãozinho `...` acende novamente com continuações diretas das escolhas anteriores:

| Cidade | NPC | Fala Etapa 2 | Continuidade |
|---|---|---|---|
| **Seiryu** (`0x22E`) | **Maria Chorona** (`Q208`) | `TALK01 41` | Pânico achando que a A.o.A pegou a Elle, reagindo ao teu conselho da Fase 2. |
| **Seiryu** (`0x22E`) | **Sarah** (`Q210`) | `TALK01 42` | Esquece o Líder "retriever" por um segundo preocupada se o pet Doria tá com fome. |
| **Seiryu** (`0x230`) | **Gerente Sasaki** (`Q211`) | `TALK01 93` | Admite que fazer um jogo do nível da MAGAMI é difícil — lembrando da zoeira de "péssimo espião" ou da vaga! |
| **Seiryu** (`0x230`) | **Srta. Olivia** (`Q212`) | `TALK01 97` | Choque total ao descobrir a A.o.A por trás da MAGAMI, reagindo ao relatório / vício no jogo. |
| **Suzaku** (`0x23E`) | **Lena** (`Q214`) | `TALK01 124` | Terremoto de magnitude 6 na escala Richter conectado à tua visão sobre o Lucky Mouse. |

---

## 2. Novos Mini-Arcos — Fase 5 (Genbu City, `0x26F`)

| Mapa | NPC (Ator) | Etapa 0 (`progress 0..36`) | Opções (`Q229, Q230`) | Etapa 1 (`progress 37+`) |
|---|---|---|---|---|
| **Genbu City** (`0x26F`) | **Guarda DO Portão** (`37`) | `TALK01 247` (`Q229`) — Confiante na fortaleza de Genbu | **1:** Ainda bem! (`teddy:+1`)<br>**2:** Não subestima eles (`lisa:+1,rep:+1`)<br>**3:** Se vierem, eu resolvo (`keith:+1`)<br>**0:** Ignorou | `TALK01 249`: preocupado com o amigo **Chuck** em Amaterasu, reconhecendo teu aviso ou pedindo pra salvar o amigo. |
| **Genbu City** (`0x26F`) | **Guarda DO Veterano** (`39`) | `TALK01 261` (`Q230`, `prog 0..37`) — Abalado quando a A.o.A leva o Juggernaut | **1:** A gente vai virar! (`rep:+1`)<br>**2:** Sem desanimar, guarda! (`ivy:+1`)<br>**3:** Cadê o Líder Genbu? (`lisa:+1`)<br>**0:** Ignorou | `TALK01 262` (`prog 38+`): repassa a mensagem do Líder Genbu reconhecendo que jovens corajosos salvam o mundo. |

---

## 3. Prompt Síncrono de Batalha Adicionado (Fase 5)

| Prompt | Gatilho | Escolhas & Efeitos Nativos |
|---|---|---|
| **Game Master — Queda de Asuka** (`Q228`) | Chefe (`records_state.battle == 10`, HP ≤ 50%) | • *"Pelo Teddy e por Asuka: acaba agora!"* → **ATK +2** (`teddy:+2, rep:+2`)<br>• *"Aguenta firme o contra-ataque!"* → **DEF +2** (`lisa:+1, rep:+1`)<br>• *"Cala a boca! Eu vou acabar com você!"* → **SPD inimigo +1** (perdeu a cabeça! `ivy:-1, rep:-1`) |

---

## 4. Auditoria Gemini Pro (Fases 0–5) — Correções Aplicadas

Antes de seguir para a Fase 6, rodamos uma auditoria completa com subagente **Gemini Pro** (código + narrativa) e corrigimos todos os achados:

1. **Prompts de Batalha (`port/game/definitive_battle_prompt.c`):**
   - Corrigido o gatilho: `fightstg_battle.state.type` é `0` para batalhas normais/líderes (`1` é só Bulbmon e `2` é só Zanbamon); todos os 7 prompts agora usam `records_state.battle` exato (`188` Genji, `200` Keith, `2` MasterTyranno, `3` Seiryu, `4` Suzaku, `5` Byakko, `10` Game Master).
   - Adicionada guarda `def_record_story_answer(p->question) == 0` para impedir re-disparo/farming na mesma batalha ou em revanche.
   - Adicionado encoder UTF-8 → glifos PS1 (`0x01 xx`) para renderização com acentuação completa em PT-BR.
   - `def_story_bond()` (≥2) e `def_story_rep()` (≥4) agora ampliam buffs positivos do jogador em +1 tier nativo.
   - Removidos anacronismos ("call", logs em caixa alta).
2. **Cronologia dos Mini-Arcos (`port/game/definitive_story.c` + `pt_choices.txt` + `pt_echoes.txt`):**
   - Realinhados os 25 NPCs em `npc_arcs[]` às faixas exatas de `progress` dos scripts `wstag*.c`, corrigindo as 3 inversões: **Vovô Jake** (`22 -> 642 -> 646`), **Agumon de Seiryu** (`12 -> 15`) e **Guarda DO Veterano** (`261 -> 262`).
3. **Zero Vazamento de Gênero & Modo Kaili (`pt_story.txt` + `pt_kaili.txt` + `pt_choices.txt`):**
   - Removidos termos masculinos endereçados ao protagonista (`menino`, `garoto`, `o novato`, `amigão`, `ô rei`) e adicionadas falas correspondentes do Junior em `pt_kaili.txt` (`DMG200:3/4/5 9`).
   - Removidos anacronismos da Lisa (`"spoiler"`, `"VIP"`, `"tutorial"`).
4. **Anti-Slop & Coerência (`pt_choices.txt` + `pt_echoes.txt`):**
   - Reescritos todos os ecos `Q200..Q230` eliminando fórmulas repetitivas (*"Você disse X / Você tinha razão"*), criando divergência real (Shiro, Agumon, Renamon, Maya), unificando placas de nomes (`Elle Encantadora`, `Sra. Olivia`, `Aluno Tony`, `Aiko Representante`), removendo spoiler da Sra. Olivia em `Q212` e ajustando falas para `≤ 22` letras/linha.

---

## 4. Exemplos de Falas Reescritas (História Principal — Fase 5)

- **Queda de Asuka / Enxame de Vemmon (`DMG300:22–23` — tom dramático firme):**
  > **Kail:** *"Um enxame de Vemmon veio de Amaterasu direto do Centro Administrativo. Dispararam raios pra todo lado... e todo mundo virou Oinkmon na hora."*  
  > **Kail:** *"...Tá bom. Eu confio em você, `<nome>`. Mas vê se volta bem, senão eu mesma te pego!"*  
  > **Protagonista (vendo Teddy/Lisa/Nick/Keith como Oinkmon):** *"Teddy... não dá pra entender nada do que você fala assim. Segura firme aí com a Lisa e o pessoal. Eu juro que vou dar um jeito de reverter isso."*

---

## Pontos para sua Revisão
1. O contraste de tom na **Queda de Asuka** (mais firme e dramático, sem perder a naturalidade) ficou no ponto certo?
2. Posso commitar a **Fase 5** e fechar o último arco PT-BR: **Fase 6 (Servidor Amaterasu, Clímax & Epílogo)**?
