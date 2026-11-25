# Revisão da Fase 6 — Servidor Amaterasu, Clímax & Epílogo (PT-BR)

> Documento para revisão final do arco PT-BR antes da **Fase 7 (Port para Inglês e demais idiomas)**.  
> Todas as falas seguem a **regra de gênero neutro** para o protagonista, **≤ 22 caracteres/linha**, e evitam NPCs de Card/Digimon Battle.

---

## 1. Encerramentos Finais de NPCs Antigos (`progress 21+` / `38+`)

Ao retornar a Asuka City e Genbu City após a queda da A.o.A em Amaterasu, o balãozinho `...` acende para fechar o ciclo de 4 NPCs queridos das fases anteriores:

| Cidade | NPC (Arco) | Fala Final | Desfecho |
|---|---|---|---|
| **Asuka** (`0x203`) | **Shiro Novato** (`Q205`) | `TALK00 428` (`prog 38+`) | Fecha a piada do beliscão/máquina/passeio: comemora a volta pro Mundo Real (e o rango da mamãe!). |
| **Asuka** (`0x203`) | **Alan Preguiçoso** (`Q206`) | `TALK00 450` (`prog 38+`) | Com a A.o.A presa, volta pro sono/café/férias dele — reagindo à tua escolha da Fase 1. |
| **Asuka** (`0x203`) | **Mary Fã de Digimon** (`Q207`) | `TALK00 431` (`prog 21+`) | Virada cômica: depois de tanto surtar entre Gallantmon e Beelzemon, se apaixona por **Sukamon e Vegiemon**! |
| **Genbu** (`0x26F`) | **Guarda DO Portão** (`Q229`) | `TALK01 250` (`prog 38+`) | Fechamento da busca pelo amigo **Chuck**: descobre aliviado que o Chuck não só está vivo como **liderou a Resistência**! |

---

## 2. Novos Mini-Arcos — Fase 6 (Amaterasu City, `TALK03`, `prog 38 → 43 pós-game`)

| Mapa | NPC (Ator) | Etapa 0 (`prog 38..42`) | Opções (`Q231, Q232`) | Etapa 1 Pós-Game (`prog 43+`) |
|---|---|---|---|---|
| **Amaterasu Central** (`0x270`) | **Jim Mentiroso** (`45`) | `TALK03 194` (`Q231`) — *"Olha a A.o.A aí!! ...Pegadinha do malandro!"* | **1:** Quase me mata! (`teddy:+1`)<br>**2:** Piada sem graça (`lisa:+1`)<br>**3:** Se vierem, eu encaro (`keith:+1,rep:+1`)<br>**0:** Ignorou | `TALK03 196` (3 meses depois): jura que os torneios voltaram (*"Glu-glu, ié-ié!"*), tenta outra pegadinha ou admite que não ousa zoar quem derrubou o chefão do espaço! |
| **Amaterasu Bridge** (`0x271`) | **Comilão** (`51`) | `TALK03 216` (`Q232`) — *"Que trauma! Fiquei até sem graça de comer torresmo!"* | **1:** Passa já já! (`teddy:+1`)<br>**2:** Vai de salada então (`ivy:+1`)<br>**3:** Pelo menos salvamos (`rep:+1`)<br>**0:** Ignorou | `TALK03 218` (3 meses depois): voltou pro torresmo na feira, perdeu 5 kg com alface, ou te paga um churrasco! |

---

## 3. Prompts Síncronos de Batalha do Clímax (`definitive_battle_prompt.c`)

| Prompt | Gatilho (`records_state.battle`) | Escolhas & Efeitos Nativos |
|---|---|---|
| **Oficial-Chefe** (`Q233`) | `battle 32`, HP ≤ 50% (Master Room Amaterasu) | • *"Teu império acaba hoje!"* → **ATK +2** (`lisa:+2, rep:+2`)<br>• *"Aguenta o tranco, equipe!"* → **DEF +2** (`teddy:+1, rep:+1`)<br>• *"Destromon vai virar sucata!"* → **ATK chefe +1** (`keith:+1, rep:-1`) |
| **Lord Megadeath** (`Q234`) | `battle 33`, HP ≤ 50% (Gunslinger) | • *"Deus nenhum! Eu vou te derrubar!"* → **ATK +2** (`rep:+2, teddy:+2`)<br>• *"Todo plano tem uma falha!"* → **Libra** (`lisa:+2, rep:+1`)<br>• *"Ele é forte demais... não dá..."* → **DEF -1** (`ivy:-1, rep:-1`) |
| **Galacticmon — Batalha Final** (`Q235`) | `battle 324` (`0x144`), HP ≤ 50% (Espaço) | • *"Por todos os nossos mundos!"* → **ATK +2** (`teddy:+2, ivy:+2, rep:+2`)<br>• *"Não caio antes de vencer!"* → **DEF +2** (`lisa:+2, rep:+2`)<br>• *"Vamos superar a luz!"* → **SPD +2** (`keith:+2, rep:+2`)<br>*(Todas as 3 opções são heroicas e escalam com o Vínculo/Reputação acumulados na campanha!)* |

---

## 4. História Principal Reescrita (Fase 6 — Tom Dramático Firme + Epílogo Caloroso)

- **Oficial-Chefe (`DMG300:17`):** confronto seco e direto sem concessões (*"Prefiro virar pó a sujar minhas mãos!"*).
- **Lord Megadeath & Snatchmon → Galacticmon (`DMG800:15–17`):** removidas gírias deslocadas do clímax espacial (como *"última bolacha"*), elevando o peso de Snatchmon devorando o próprio criador e da ameaça de **Gaiamon**.
- **Duelo Final com Kurt — World Champ (`DMG900:4–5`):** encerramento caloroso e empolgante de fim de temporada (*"Mostra tudo! Sem aliviar um golpe!"* / *"O posto de Tamer mais forte agora é seu."*).

---

## Pontos para sua Revisão
1. O fechamento dos **4 NPCs antigos** (especialmente o Guarda de Genbu descobrindo que o **Chuck** liderou a Resistência) + os **3 prompts de batalha do clímax** ficaram redondos?
2. Posso commitar a **Fase 6** (fechando 100% do conteúdo PT-BR da campanha inteira) e preparar a **Fase 7 (Port EN / demais idiomas)** ou fazer uma última auditoria antes?
