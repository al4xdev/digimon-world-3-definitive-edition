# Revisão da Fase 2 — Setor Leste & Seiryu City (PT-BR)

> Documento para revisão antes de avançarmos para a **Fase 3 (Setor Central & Sul / Suzaku)**.  
> Nenhum NPC de Card Battle ou Digimon Battle foi tocado para não interferir no fluxo de batalhas.

---

## 1. Mapa de NPCs & Mini-Arcos — Fase 2 (Seiryu City: 6 Mini-Arcos em 3 Mapas)

| Mapa | NPC (Ator) | Etapa 0 (`progress 0..11`) | Opções (`Q208..Q213`) | Etapa 1 (`progress 12+`) |
|---|---|---|---|---|
| **Praça Seiryu** (`0x22E`) | **Maria Chorona** (`46`) | `TALK01 10` (`Q208`) — Desesperada porque perdeu a amiga Elle | **1:** Ela tá na outra torre! (`rep:+1`)<br>**2:** Para de chorar, ué (`keith:+1`)<br>**3:** Vai comer algo antes (`teddy:+1`)<br>**0:** Ignorou | `TALK01 16`: procura na torre errada (Gale vs Zephyr), engolindo o choro / comendo salgado / chorando sozinha. |
| **Lojas Seiryu** (`0x22F`) | **Elle Charmosa** (`46`) | `TALK01 65` (`Q209`) — Preocupada com a Maria chorona | **1:** Ela tá chorando por ti! (`rep:+1`)<br>**2:** Deixa ela se virar (`keith:+1`)<br>**3:** Vocês duas se merecem (`ivy:+1`)<br>**0:** Ignorou | `TALK01 67`: descobre que combinou Torre Zephyr enquanto Maria foi pra Torre Gale — reagindo à tua resposta. |
| **Praça Seiryu** (`0x22E`) | **Sarah** (`50`) | `TALK01 11` (`Q210`) — Fã #1 apaixonada pelo Líder de Seiryu | **1:** Vou derrubar ele já já (`keith:+1,rep:+1`)<br>**2:** Parece um Golden, né? (`teddy:+1`)<br>**3:** Foco no treino, moça (`lisa:+1`)<br>**0:** Ignorou | `TALK01 17`: solta a pérola de que o Líder parece um Golden Retriever — creditando você se escolheu a opção 2! |
| **Pousada Seiryu** (`0x230`) | **Gerente Sasaki** (`53`) | `TALK01 77` (`Q211`) — Espião da concorrente da MAGAMI que vaza o plano pro primeiro estranho | **1:** Péssimo espião, hein! (`ivy:+1`)<br>**2:** Me contrata então! (`rep:+1`)<br>**3:** Vou dedurar pra MAGAMI (`rep:-1`)<br>**0:** Ignorou | `TALK01 81`: tenta ficar discreto / foge da vaga / pede pra não dedurar enquanto a Srta. Olivia surta com a MAGAMI. |
| **Pousada Seiryu** (`0x230`) | **Srta. Olivia** (`54`) | `TALK01 78` (`Q212`) — Colega espiã que viciou no Digimon Online | **1:** Esquece o chefe e joga! (`teddy:+1`)<br>**2:** Anota pro relatório (`lisa:+1`)<br>**3:** Tá perdendo tempo aqui (`keith:+1`)<br>**0:** Ignorou | `TALK01 82`: indignada com a manutenção da MAGAMI — reagindo direto ao teu conselho! |
| **Praça Seiryu** (`0x22E`) | **Agumon de Seiryu** (`365`) | `TALK01 15` (`Q213`) — Recepcionista da Cidade do Vento | **1:** Ventania braba aqui! (`teddy:+1`)<br>**2:** Cadê o Líder daqui? (`rep:+1`)<br>**3:** Já vi cidades melhores (`rep:-1`)<br>**0:** Ignorou | `TALK01 12`: acabado depois do treino, lembrando se você foi gente boa ou "turista chato". |

---

## 2. Prompt Síncrono de Batalha Adicionado (Fase 2)

| Prompt | Gatilho | Escolhas & Efeitos Nativos |
|---|---|---|
| **Líder de Seiryu — Pressão** (`Q223`) | Luta de Líder/Tamer (`type 1`, `progress 4..9`, HP ≤ 50%) | • *"Pode soprar que eu não arredo pé!"* → **DEF +2** (`lisa:+1, rep:+2`)<br>• *"Entendi teu ritmo!"* → **SPD +1** (`teddy:+1, rep:+1`)<br>• *"Esse ventinho aí não assusta ninguém!"* → **ATK inimigo +1** (provocou demais! `keith:+1, rep:-1`) |

---

## 3. Exemplos de Falas Reescritas (História Principal — Fase 2)

- **MasterTyrannomon (`DMG400:0–1`):**
  > **MasterTyranno:** *"Aí sim! Acabei de bater um pratão e tô tinindo!"*  
  > **MasterTyranno:** *"Olha a marra da fera! Só no gogó você não me derruba não!"*  
  > **MasterTyranno (pós-luta):** *"Pô, jovem... tu bate pesado mesmo! Só não relaxa: o Líder de Seiryu é casca-grossa. Entra ligado!"*
- **Líder de Seiryu (`DMG400:2–3`, escolhas `Q45` e `Q46`):**
  > **Protagonista (`Q45` opção 3):** *"Líder Seiryu! O dinossauro já caiu. Agora é tua vez!"*  
  > **Protagonista (`Q46` opção 1):** *"Sou `<nome>` (`<8,1><5,1>`). Que lutaça, hein! Valeu cada segundo!"*
- **Gôndola pro Setor Sul (`DMG400:4–6`):**
  > **Protagonista:** *"O bondinho tá sacudindo todo! Terremoto?! Espera aí... Tem algo se mexendo lá embaixo! Aquilo é um Digimon?!"*

---

## Pontos para sua Revisão
1. Os 6 mini-arcos de Seiryu (destaque para a dupla **Maria/Elle**, os espiões **Sasaki/Olivia** e a **Sarah** do Golden Retriever) ficaram divertidos e naturais?
2. Posso commitar a **Fase 2** e seguir para a **Fase 3 (Setor Central & Sul / Suzaku)**?
