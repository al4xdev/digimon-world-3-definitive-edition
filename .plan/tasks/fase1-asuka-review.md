# Revisão da Fase 0 (Infra C) + Fase 1 (Prólogo & Asuka City — PT-BR)

> Documento gerado para sua revisão antes de avançarmos para a **Fase 2 (Setor Leste & Seiryu)**.  
> Tudo aqui já está implementado, compilado e ativo no modo PT-BR.

---

## 1. Bíblia de Tom & Voz (PT-BR — "Dublagem Clássica Anos 80/90")

- **Protagonista (Junior / Kaili):** esperto, impaciente na medida certa, responde provocações com ironia afiada ("quem na fila do pão?", "senta lá, fera"), mas segura a bronca quando a coisa fica séria.
- **Teddy:** parceiro leal e atrapalhado, alívio cômico natural ("travei na tela de nome, ficou Teddy mesmo").
- **Ivy / Kail:** pavio curto e decidida ("O apelido é meu! Vai encarar ou vai ficar de gracinha?"), protetora quando o bicho pega.
- **Tamer Genji:** mestre velhinho malandro estilo Genkai / Mestre Kame ("carne nova no pedaço!", "o negócio se aprende levando tabefe!").
- **Keith:** rival barulhento e fanfarrão estilo Kuwabara/Vegeta ("Sayonara, mané!", "Só peguei leve pra você não sair chorando pra mamãe!").
- **Etemon:** malandro carioca/anos 90 puro ("Fala comigo, bebê!", "Tão querendo queimar o meu filme, bicho!").
- **Nota de terminologia:** usamos **Vínculo** (`bond`) para relacionamento com aliados (Teddy, Ivy, Lisa, Keith, Kurt), mantendo **Afinidade** (`affinity`) exclusiva do cálculo elemental de dano.

---

## 2. Infraestrutura Entregue (Fase 0)

1. **Expansion Save Narrativo** ([definitive_story_save.c](file:///home/alex/git/my/dw3/dw2003recomp/port/game/definitive_story_save.c), seção `"story"` em [definitive_expansion_sections.c](file:///home/alex/git/my/dw3/dw2003recomp/port/game/definitive_expansion_sections.c)):
   - Capacidade expandida de **160 → 512 perguntas** (`Q0..Q511`, 2 bits cada: `0 = ignorou/não falou`, `1..3 = opção`).
   - Migração automática e transparente de saves antigos (`story_answers[40]`).
   - Persistência de **Vínculo** individual (Teddy, Ivy, Lisa, Keith, Kurt), **Reputação global** (`rep`) e **estágio visto de cada NPC** (`npc_seen[128]`).
2. **Motor de Escolhas & Echoes Estendido** ([gen_text_override.py](file:///home/alex/git/my/dw3/dw2003recomp/tools/gen_text_override.py) + [definitive_story.c](file:///home/alex/git/my/dw3/dw2003recomp/port/game/definitive_story.c)):
   - Suporte a escolhas em **NPCs comuns** (`def_story_npc_talk` em `fieldstg_80087DB0.c`), não apenas em cutscenes.
   - Suporte a tags de efeito direto em `pt_choices.txt`: `[teddy:+1,rep:+1]`, `[keith:-1]`, etc.
   - Suporte a **`Q<n>=0`** em `pt_echoes.txt` (4º desfecho automático quando o jogador passa batido pelo NPC na fase anterior).
   - Suporte a escolhas e echoes **exclusivos PT-BR** sem travar o build de EN.
3. **Indicador Visual em NPCs com Fala Nova** (`def_story_npc_indicator_draw` em [definitive_story.c](file:///home/alex/git/my/dw3/dw2003recomp/port/game/definitive_story.c)):
   - Balãozinho animado (`...`) flutuando sobre a cabeça de NPCs de mini-arco sempre que houver uma etapa nova disponível no `progress` atual; some assim que você conversa com o NPC.
4. **Mensageria Síncrona de Batalha** ([definitive_battle_prompt.c](file:///home/alex/git/my/dw3/dw2003recomp/port/game/definitive_battle_prompt.c)):
   - Trava o turno do jogador, acinzenta a HUD de batalha e abre diálogo com 3 opções.
   - Reutiliza 100% os **buffs/debuffs nativos** do jogo (`def_mod_apply`: ATK/DEF/SPD ±1..±3) e o **Libra nativo** (`def_tech_effect(0, 202)` revelando fraquezas), além de gravar a escolha + Vínculo + Reputação.

---

## 3. Mapa de NPCs & Mini-Arcos — Fase 1 (Asuka City: 8 Mini-Arcos em 3 Mapas)

| Mapa | NPC (Ator) | Etapa 0 (`progress 0..11`) | Opções (`Q200..Q207`) | Etapa 1 (`progress 12+`) |
|---|---|---|---|---|
| **Central Park** (`0x200`) | **Mika** (`46`) & **Patamon** (`371`) | `TALK00 19` (`Q200`) — Mika cobra Patamon por não virar Angemon | **1:** Dá a dica do Ginásio (`rep:+1`)<br>**2:** Zoa o Patamon ("pão de queijo voador", `rep:-1`)<br>**3:** Pede paciência (`teddy:+1`)<br>**0:** Ignorou | `TALK00 623` (Mika) + `613` (Patamon): treino voando / promessa de sacode na força do ódio / relaxou / correu em volta da fonte sozinha. |
| **Central Park** (`0x200`) | **Masato** (`47`) | `TALK00 15` (`Q201`) — "Veterano" folgado mandando ir pra Seiryu | **1:** Valeu, mestre! (`rep:+1`)<br>**2:** E você, já ganhou? (`ivy:+1`)<br>**3:** Derrubo antes do almoço! (`keith:+1`)<br>**0:** Ignorou | `TALK00 665`: fofoca da Matrix / mordido com a tirada / impressionado contigo / resmungando sozinho. |
| **Central Park** (`0x200`) | **Agumon Coach** (`365`) | `TALK00 21` (`Q202`) — Personal trainer do Ginásio | **1:** Manda o treino, coach! (`rep:+1`)<br>**2:** Prefiro sair na mão! (`keith:+1`)<br>**3:** Só quero um lanche (`teddy:+1`)<br>**0:** Ignorou | `TALK00 602`: "aluno nota dez" / "valentão do mato" / confessa que largaria tudo por churrasco / reclamando da geração nova. |
| **Main Street** (`0x201`) | **Vovô Jake** (`57`) | `TALK00 22` (`Q203`) — Quer virar Digimon uma vez na vida | **1:** Viraria qual, vovô? (`teddy:+1`)<br>**2:** Cuidado com o pedido (`ivy:+1`, *foreshadowing* do Oinkmon!)<br>**3:** Cada doido em Asuka... (`rep:-1`)<br>**0:** Ignorou | `TALK00 646`: xingando "L.o.L. ou A.o.A" lembrando da conversa ou desistindo de virar Digimon pelo gelo que você deu. |
| **Main Street** (`0x201`) | **Jack Maníaco** (`51`) | `TALK00 11` (`Q204`) — Palestra de armas/equipamentos | **1:** Manda a build, chefe! (`rep:+1`)<br>**2:** Prefiro no soco limpo (`keith:+1`)<br>**3:** Cadê o teu Digimon? (`lisa:+1`)<br>**0:** Ignorou | `TALK00 711`: perdeu o Guilmon na muvuca (e te culpa pelo "azar" se você perguntou dele!), mas curte jogar de graça. |
| **Saguão** (`0x203`) | **Shiro Novato** (`45`) | `TALK00 17` (`Q205`) — Fascinado com a Matrix Chamber | **1:** Dá um beliscão aí! (`teddy:+1`)<br>**2:** Fica esperto com isso (`ivy:+1`)<br>**3:** Bem-vindo ao futuro! (`rep:+1`)<br>**0:** Ignorou | `TALK00 329`: se beliscou na bochecha seguindo teu conselho (ou tentando acordar desconfiado) e viu que não é sonho! |
| **Saguão** (`0x203`) | **Alan Preguiçoso** (`49`) | `TALK00 13` (`Q206`) — Explica o satélite morrendo de sono | **1:** Explica direito, ué (`rep:+1`)<br>**2:** Tá com sono, chapa? (`teddy:+1`)<br>**3:** Já saquei tudo (`keith:+1`)<br>**0:** Ignorou | `TALK00 331`: tomou o café que você recomendou (ou cedeu à cobrança) e solta a dica de fraqueza elemental. |
| **Saguão** (`0x203`) | **Mary Fã** (`52`) | `TALK00 239` (`Q207`) — Sonha em evoluir pro Gallantmon | **1:** Gallantmon é brabo! (`rep:+1`)<br>**2:** E o Beelzemon, não? (`keith:+1`)<br>**3:** Começa pelo básico (`lisa:+1`)<br>**0:** Ignorou | `TALK00 330`: trava no dilema **Gallantmon vs Beelzemon** (e te culpa direto se você plantou o Beelzemon na cabeça dela!). |

---

## 4. Batalhas com Mensageria Síncrona (Fase 1)

| Prompt | Gatilho | Exemplos de Escolhas & Efeitos Nativos |
|---|---|---|
| **Duelo de Tamer (Asuka)** (`Q220`) | Luta de Tamer (`type 1`, `progress 0..6`, Turno 1) | • *"Pode vir quente que eu tô fervendo!"* → **ATK +1** (3 turnos, `rep:+1`)<br>• *"Deixa eu ler o jogo dele primeiro..."* → **Libra** revela fraquezas (`teddy:+1, rep:+1`)<br>• *"Uai, eu nem li o tutorial direito..."* → **DEF -1** (2 turnos, `rep:-1`) |
| **Chefe — Abertura** (`Q221`) | Luta de Chefe (`type 2`, `progress 1..12`, Turno 1) | • *"Vai pra cima sem dó!"* → **ATK +2** (`ivy:+1, rep:+1`)<br>• *"Analisa a brecha dele!"* → **Libra** (`teddy:+1, rep:+1`)<br>• *"Segura a onda, não se expõe!"* → **DEF +2** (`lisa:+1`) |
| **Chefe — Pressão 50% HP** (`Q222`) | Luta de Chefe (`type 2`, `progress 1..15`, HP ≤ 50%) | • *"Cala a boca e segura essa!"* → **DEF inimigo -1** (`rep:+2`)<br>• *"Mantém o ritmo, sem pressa!"* → **SPD +1** (`teddy:+1`)<br>• *"Já era, vamos perder essa..."* → **ATK -1** (`ivy:-1, rep:-1`) |

---

## 5. Exemplos de Falas Reescritas (História Principal — Fase 1)

- **Chegada em Asuka (`DMG200:0` / `Q8`):**
  > **Protagonista (opção 1):** *"Nossa Senhora... Então é isso aqui o tal do Digimon Online?!"*  
  > **Protagonista (opção 2):** *"Bonito demais pra ser verdade... Onde tá a pegadinha?"*
- **Saguão com Teddy e Kail (`DMG200:3`):**
  > **Protagonista:** *"Ô seus pé-de-chumbo, que demora é essa? Bora logo!"*  
  > **Teddy:** *"Pé-de-chumbo o quê?! Você que saiu vazado feito foguete!"*  
  > **Kail:** *"O apelido é meu! Vai encarar ou vai ficar de gracinha?"*
- **Tamer Genji (`DMG200:11–12`):**
  > **Tamer Genji:** *"Ho ho ho! Olha só, carne nova no pedaço! Esquece manual, o negócio se aprende levando tabefe!"*
- **Keith (`DMG200:21–22`):**
  > **Protagonista:** *"...Sou eu mesmo. E você é quem na fila do pão?"*  
  > **Keith:** *"Cala a boca que eu nem lembro disso! Abre o olho! Na próxima eu te passo o rodo! Sayonara, mané!"*
- **Etemon (`DMG200:23`):**
  > **Etemon:** *"Fala comigo, bebê! O rei do balanço sabe de tudo nesse servidor!"*  
  > **Etemon:** *"E-eu?! Que calúnia! Tão querendo queimar o meu filme, bicho!"*

---

## Pontos para sua Revisão
1. O nível de zoeira/gírias das falas da história principal e dos NPCs está no ponto certo, ou quer puxar mais/menos?
2. Os 3 mini-arcos de Asuka City (Mika/Patamon, Masato e Agumon Coach) ficaram no formato que você imaginou?
3. Quer ajustar algum buff/debuff dos prompts de batalha ou podemos seguir para a **Fase 2 (Setor Leste & Seiryu)**?
