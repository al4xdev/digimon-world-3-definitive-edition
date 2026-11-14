# Plano: Tradução PT-BR (#2) + Buff/Debuff Fixo sem Stack (#4) + Ajuste Overlevel Rush (8 Níveis)

## 1. Visão Geral e Escopo Ajustado

Achamos a documentação prévia em `.plan/tasks/definitive-rebalance-prd.md` e `.dumps/tasks/persona/tecnicas-v2.md`. Com os seus ajustes, o escopo fica fechado em 3 entregas:

1. **Ajuste Rápido no Overlevel Rush (`DEF_OVERLEVEL_GAP`: `15 → 8`):**
   - Em `port/game/definitive.h:10`, reduzir `#define DEF_OVERLEVEL_GAP 15` para **`8`** (e atualizar `README.md:413` de `15 or more levels` para `8 or more levels`), permitindo que o Overlevel Rush ative com 8 níveis de vantagem.
2. **Foco 100% no PT-BR agora (o idioma mais pesado, que traduz o jogo inteiro + Guia + Menus novos):**
   - O disco europeu (`SLES_039.36`) **já possui** a campanha original traduzida para Francês, Alemão, Italiano e Espanhol (`FS*`, `GS*`, `IS*`, `SS*`). O que falta nesses 4 idiomas são apenas os **textos novos/alterados da Definitive Edition** (`tools/text_override/en.txt`: Guia in-game, Bestiário, Caçada, Forja, Mercado Negro, DRI e `SKLINF`).
   - Portanto, **não vamos gastar tempo com FR/DE/ES/IT agora**: fazemos **todo o PT-BR primeiro** (base do jogo + Guia + menus novos + descrições de técnicas) para você jogar e validar na prática.
   - No **final do plano (Seção 5)**, deixamos o **gancho pronto** na arquitetura de `def_text_override` e no script para, depois que você aprovar o PT-BR jogando, traduzirmos apenas o delta da Definitive Edition (`en.txt`: Guia, quests, menus novos e `SKLINF`) para FR, DE, ES e IT.
   - **Termos de Mecânica e Callouts ficam em Inglês (não traduzir):** `WEAK`, `RESIST`, `OVERFINISH`, `Baton Pass`, `Relay`, `Libra`, `Digimon`, `Digi-Lab`, `Load Tech`, `Blast Gauge`, `Bits`, `TP`, `MP`, `HP`, além dos nomes próprios de Digimon e NPCs.
3. **Execução em 2 Frentes Paralelas:**
   - **Frente A — Subagente `google-antigravity/gemini-4-hydrogen-high` (em background):** implementa a **Task #4 (Buff/Debuff em 3 níveis fixos, sem stack, duração em turnos e descrições explícitas em `SKLINF`)** + o ajuste de **`DEF_OVERLEVEL_GAP = 8`** em `port/game/definitive.h`.
   - **Frente B — Agente Principal + Servidor Local Gemma 4 (~150 tok/s):** prepara o suporte a PT-BR no motor/launcher e roda o script Python (`tools/translate_ptbr.py`) no seu servidor Gemma 4 local, segurando apenas as descrições `SKLINF` de buff/debuff até o subagente da Frente A terminar.

---

## 2. Frente A — Task #4: Buff/Debuff Fixo em 3 Níveis, Sem Stack, Turnos Explícitos + `DEF_OVERLEVEL_GAP = 8` (Subagente Gemini 4 Hydrogen)

### 2.1. Diagnóstico do Motor Atual (`src/fightstg/fightstg_8008D3B4.c`)
- Hoje, `fightstg_rules_change_modifier` (`:9175`) **soma/acumula** o modificador (`m->modifiers[kind] += base * amount / 128`) até `[-base/2, +base]`.
- A duração em `fightstg_events_start_modifier` (`:7303`) usa um timer aleatório na fila de eventos (`pad_random.next() % base + 2000 + own->stats[2] * 10`), sem relação clara com turnos.
- Já os debuffs elementais (`207 Resist Down` e `208 Mega Break`) em `port/game/definitive.c` (`:770`) usam contador de turnos real (`resist_turns[victim][slot] = DEF_BREAK_TURNS`).

### 2.2. Regras do Novo Sistema de Buff/Debuff (`port/game/definitive.h` & `definitive.c`)
1. **Overlevel Rush em 8 Níveis (`port/game/definitive.h:10` & `README.md:413`):**
   - Alterar `#define DEF_OVERLEVEL_GAP 8` (antes `15`) e atualizar a menção em `README.md`.

2. **Escala Padronizada de 3 Níveis (Buff `+1, +2, +3` e Debuff `-1, -2, -3`) com Valores Fixos:**
   - Todo buff ou debuff de stat (`kind 0` = Ataque Físico + Mágico via Camada B; `kind 1` = Defesa Física + Mágica; `kind 2` = Velocidade) opera por **nível discreto** `mod_level[side][slot][kind] ∈ {-3, -2, -1, 0, +1, +2, +3}`:
     - **Buffs:**
       - `DEF_BUFF_LV1_PCT = 20` (`+20%`)
       - `DEF_BUFF_LV2_PCT = 35` (`+35%`)
       - `DEF_BUFF_LV3_PCT = 50` (`+50%`)
     - **Debuffs** (multiplicados por `DEF_BOSS_DEBUFF_PCT / 100` quando o chefe bloquearia no original):
       - `DEF_DEBUFF_LV1_PCT = 15` (`-15%`)
       - `DEF_DEBUFF_LV2_PCT = 25` (`-25%`)
       - `DEF_DEBUFF_LV3_PCT = 35` (`-35%`)
   - O valor efetivo em `m->modifiers[kind]` é recalculado diretamente do stat base (`base * pct / 100`), sem soma cumulativa.

3. **Bloqueio de Stack (Apenas Renova o Tempo):**
   - Aplicar um buff/debuff de **mesmo sinal** **não soma** porcentagem: mantém o maior nível entre o atual e o aplicado (`max(|atual|, |novo|)`) e **renova a duração em turnos** (`max(turnos_atuais, turnos_da_tecnica)`).
   - Aplicar um efeito de **sinal oposto** (Buff sobre Debuff ou Debuff sobre Buff no mesmo stat) substitui pelo novo efeito e inicia a contagem de turnos dele.

4. **Duração Determinística em Turnos (e Variação de Turnos quando 3 Escalas Forem Poucas):**
   - Contador `mod_turns[2][3][3]` (persistido no `game_savestate`), decrementado a cada turno daquele combatente (mesma cadência de `resist_turns`), zerando `m->modifiers[kind]` e `mod_level` quando chega a `0`.
   - Para diferenciar técnicas que cairiam no mesmo nível de intensidade, usamos também a **duração em turnos** (`2`, `3` ou `4` turnos):
     - Golpe de dano com debuff secundário (`k=26, 27`): **Nível 1 por 2 ou 3 turnos** (`DEF_MOD_TURNS_SHORT = 2` / `DEF_MOD_TURNS_BASE = 3`).
     - Técnica dedicada de buff/debuff individual (`198 Double Power`, `200 Double Guard`, `205 Armor Break`, `194 Power Seal`): **Nível 2 por 3 ou 4 turnos** (`DEF_MOD_TURNS_BASE = 3`, `DEF_MOD_TURNS_LONG = 4`).
     - Técnica dedicada forte ou de grupo (`199 Mega Strength`, `201 Mega Protection`, `206 Armor Off`, `195 Sapping Bloom`): **Nível 2 em grupo (3 turnos) ou Nível 3 em alvo único (3–4 turnos)**.
   - Incorpora o mapeamento de técnicas da Camada H (`.dumps/tasks/persona/tecnicas-v2.md`, exclusivamente em IDs com `E=0` para não alterar técnicas de inimigos).

5. **Descrições Explícitas com Turnos (`SKLINF` em `tools/text_override/en.txt`):**
   - Padronizar todas as descrições de técnicas de buff/debuff (`<= 34` caracteres) mostrando **efeito + nível (1/2/3) + duração em turnos**, por exemplo:
     - `198 Double Power`: `User Atk+2 (phys/mag), 3 turns.`
     - `199 Mega Strength`: `Allies Atk+2 (phys/mag), 3 turns.`
     - `200 Double Guard`: `User Def+2 vs all dmg, 3 turns.`
     - `201 Mega Protection`: `Allies Def+2 vs all dmg, 3 turns.`
     - `205 Armor Break`: `Foe Def-2 vs all dmg, 3 turns.`
     - `206 Armor Off`: `Foe Def-3 vs all dmg, 3 turns.`
     - `207 Resist Down`: `Foe best Elem Resist-1, 3 turns.`
     - `208 Mega Break`: `Foe 2 Elem Resists-2, 3 turns.`
     - Golpes com debuff secundário: `Mach phys. Foe Atk-1, 2 turns.`

6. **Validação da Frente A:**
   - Teste unitário/host (`tests/port/definitive_buffs.py` + driver C) provando os 3 níveis fixos, o bloqueio de stack com renovação de turnos, o decremento por turno até expirar, `DEF_OVERLEVEL_GAP == 8` e `scripts/test.sh` 100% verde.

---

## 3. Frente B — Task #2: Tradução PT-BR Completa via Gemma 4 Local

### 3.1. Inspeção de Fonte e Glifos Acentuados do Português
1. **Fonte Europeia (`font_chars` / `font_chars_ext`):**
   - Verificar na textura real da fonte europeia todos os slots `01 xx` (`0x3B..0x72`: `á, é, í, ó, ú, â, ê, ô, à, ç, ü, É, Í, Ó, Ú`) e checar `ã`/`õ` (injetando os glifos `ã`/`õ` em RAM sob `PC_PORT` caso a textura europeia não os tenha nativamente, garantindo acentuação 100% perfeita sem precisar trocar `ã` por `a`).

### 3.2. Suporte ao Idioma `Português` no Motor e no Launcher
1. **Motor (`port/game/game_mods.c`, `port/game/definitive.c`, `port/game/definitive.h`):**
   - Adicionar `"portuguese"` (`Português`) em `preset_language` (`mod.json`, `game_mods.c`) e no seletor de idioma do Launcher em C (`launcher/cfg.c`, `launcher/ui.c`).
   - Com `portuguese` ativo, `records_language` permanece `2` (carrega os arquivos `ES*.BIN` em inglês do disco) e ativa `port_language_ptbr = 1`.
   - Em `def_text_override(data, i)` e nas strings `def_str_*` (Guia in-game, missões, leilões de Asuka, Bestiário, Caçada, Forja, Mercado Negro, DRI), quando `port_language_ptbr == 1`, retorna a entrada traduzida da tabela PT-BR (`definitive_text_pt.c`, indexada com busca binária $O(\log N)$ por `(file_id, subtable, index)`) com **fallback automático para o inglês**.

### 3.3. Pipeline Python Otimizado para o Gemma 4 Local (`tools/translate_ptbr.py`)
1. **Conexão Direta ao Servidor Local (Sem Mexer nos Parâmetros de Sampling do Modelo):**
   - Script `tools/translate_ptbr.py` batendo direto no endpoint local do seu Gemma 4.
   - **Não envia** `temperature`, `top_p`, `top_k` nem `repetition_penalty` (deixa 100% a cargo do seu servidor).
   - **Calibração inicial de Prompt + Reasoning (`low` vs `medium`):**
     - Roda um mini-teste automático em 20 strings reais (descrições curtas de 34 chars, menus, Guia e diálogos `TALK*` com tags `<1>`, `<5,1>`) comparando `reasoning_effort="low"` vs `"medium"` e escolhe o que tiver 100% de respeito a tags/tamanho com melhor vazão.
2. **Glossário Fixo (O que NÃO traduzir vs Padronizações):**
   - **Manter em inglês (intocados):** `WEAK`, `RESIST`, `OVERFINISH`, `Baton Pass`, `Relay`, `Libra`, `Digimon`, `Digi-Lab`, `Load Tech`, `Blast Gauge`, `Bits`, `TP`, `MP`, `HP`, nomes das espécies de Digimon e nomes de NPCs.
   - **Padronizar em PT-BR:**
     - Elementos (em textos descritivos e menus): `Fogo`, `Água`, `Gelo`, `Vento`, `Trovão`, `Máquina`, `Trevas`.
     - Atributos: `Força`, `Defesa`, `Espírito`, `Sabedoria`, `Velocidade`.
     - Descrições de técnicas (`SKLINF`): padrão enxuto com turnos (`"Atq+2 no usuário, 3 turnos."`, `"Def-2 no alvo, 3 turnos."`, `"Magia de Fogo. Causa Veneno."`).
3. **Cache Incremental + Validador Estático de Largura e Tags:**
   - Cache persistente em `.dumps/tasks/translation/cache_ptbr.jsonl` com deduplicação de strings repetidas entre mapas.
   - Validação automática por linha:
     - Preservação exata de todos os códigos `<1>`, `<2,...>`, `<5,1>`, etc.
     - Limite estrito de caracteres/pixels por janela (`<= 34` chars em `SKLINF`, largura máxima em pixels calculada pelas métricas da fonte para caixas de diálogo `TALK*`/`DMG*` e menus).
     - Auto-retry com feedback de tamanho se alguma linha exceder a caixa.

---

## 4. Ordem de Execução Imediata (Assim que Aprovado)

1. **Disparo do Subagente `gemini-4-hydrogen-high`** para implementar a **Frente A (Task #4: Buff/Debuff 3 níveis fixos, sem stack, turnos explícitos + `DEF_OVERLEVEL_GAP = 8` + testes)**.
2. **Em paralelo no Agente Principal (Frente B):**
   - Inspecionar glifos da fonte europeia, ligar a opção `Português` no Launcher C e em `def_text_override`, e deixar `tools/translate_ptbr.py` pronto.
   - Assim que seu servidor Gemma 4 estiver ativo no TTY, rodar o mini-benchmark (`low` vs `medium`), traduzir todo o jogo + Guia + menus novos para PT-BR, e por último traduzir o lote final de `SKLINF` de buffs/debuffs entregue pelo subagente.
3. **Verificação no Jogo:**
   - Rodar `scripts/test.sh` e capturar telas em PT-BR (menus, Guia, descrições de técnicas com turnos e diálogos).

---

## 5. Gancho Pós-Validação (Futuro): Traduzir apenas os Novos Menus e Guia para FR / DE / ES / IT

- Como o disco europeu já tem todos os diálogos e itens originais em **Francês (`records_language=3`), Italiano (`4`), Alemão (`5`) e Espanhol (`6`)**, deixaremos `def_text_override(data, i)` e `tools/gen_text_override.py` preparados para aceitar `tools/text_override/{fr,de,es,it}.txt` + tabelas `def_str_*` por idioma.
- Depois que você jogar e validar o PT-BR, bastará rodar o mesmo `tools/translate_ptbr.py --lang fr,de,es,it --only-definitive-delta` sobre as ~250 linhas de `tools/text_override/en.txt` e as strings `def_str_*` (Guia in-game, missões, leilões de Asuka, Bestiário, Caçada, Forja, Mercado Negro, DRI e descrições novas de `SKLINF`) para fechar os outros 4 idiomas europeus em poucos segundos!