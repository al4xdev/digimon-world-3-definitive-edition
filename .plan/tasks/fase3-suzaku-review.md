# Revisão da Fase 3 — Setor Central & Sul / Suzaku City (PT-BR)

> Documento para revisão antes de avançarmos para a **Fase 4 (Setor Oeste / Byakko & Deserto)**.  
> Todas as falas seguem a **regra de gênero neutro** para o protagonista (100% compatível com Junior e Kaili) e evitam NPCs de Card Battle / Digimon Battle.

---

## 1. Mapa de NPCs & Mini-Arcos — Fase 3 (Suzaku City: 4 Mini-Arcos em 2 Mapas)

| Mapa | NPC (Ator) | Etapa 0 (`progress 0..19`) | Opções (`Q214..Q217`) | Etapa 1 (`progress 20+`) |
|---|---|---|---|---|
| **Praça Suzaku** (`0x23E`) | **Lena** (`50`) | `TALK01 112` (`Q214`) — Espalhando fofoca sobre a volta do Lucky Mouse | **1:** Esse cara é herói? (`rep:+1`)<br>**2:** Tem cheiro de encrenca (`ivy:+1`)<br>**3:** Nunca nem ouvi falar (`teddy:+1`)<br>**0:** Ignorou | `TALK01 114`: quando travam a saída pro Mundo Real, reage lembrando se você desconfiou, achou que era herói ou nem conhecia o Lucky Mouse. |
| **Praça Suzaku** (`0x23E`) | **Kyle Fortão** (`45`) | `TALK01 113` (`Q215`) — Impressionado por você ter derrubado Zanbamon | **1:** Zanbamon nem suou! (`keith:+1,rep:+1`)<br>**2:** Foi no limite, viu! (`teddy:+1`)<br>**3:** Qual Digimon é brabo? (`lisa:+1`)<br>**0:** Ignorou | `TALK01 121`: puxa o debate **Imperialdramon Fighter Mode vs Marsmon** respondendo direto à tua postura. |
| **Torre Suzaku** (`0x240`) | **Renamon** (`65`) | `TALK01 147` (`Q216`) — Guardiã da passagem para a Líder | **1:** Cheguei pra vencer! (`rep:+1`)<br>**2:** A Líder é braba mesmo? (`lisa:+1`)<br>**3:** Só quero a insígnia (`keith:+1`)<br>**0:** Ignorou | `TALK01 150`: libera a passagem comentando tua determinação, curiosidade ou pressa. |
| **Torre Suzaku** (`0x240`) | **Patamon Fã-Clube** (`66`) | `TALK01 151` (`Q217`) — Membro do fã-clube da Líder Suzaku | **1:** Quero entrar no clube! (`teddy:+1`)<br>**2:** Ela parece perigosa... (`ivy:+1`)<br>**3:** Vai treinar dança, vai! (`rep:+1`)<br>**0:** Ignorou | `TALK01 154`: dançando *"Un, deux, trois!"* com a Floramon — creditando você diretamente se você deu a ideia da coreografia! |

---

## 2. Prompt Síncrono de Batalha Adicionado (Fase 3)

| Prompt | Gatilho | Escolhas & Efeitos Nativos |
|---|---|---|
| **Líder de Suzaku — Pressão** (`Q224`) | Luta de Líder/Tamer (`type 1`, `progress 10..18`, HP ≤ 50%) | • *"Então aumenta esse fogo!"* → **ATK +2** (`keith:+1, rep:+2`)<br>• *"Vou achar a brecha dela!"* → **Libra** revela fraquezas (`lisa:+1, rep:+1`)<br>• *"Calma, pega leve aí..."* → **DEF -1** (`ivy:-1, rep:-1`) |

---

## 3. Exemplos de Falas Reescritas (História Principal — Fase 3)

- **Primeiro encontro com Lisa (`DMG500:0`):**
  > **Lisa:** *"...EUREKA! É um Ogremon! Só pode ser ele! Tô indo caçar esse fofinho agora! Tchau!"*  
  > **Protagonista:** *"Ogremon... fofinho?! Essa aí bateu a cabeça ou tá aprontando alguma?"*
- **Líder de Suzaku (`DMG500:1–2`):**
  > **Líder Suzaku:** *"Gosto assim, sem medo! Mas já vou avisando: quem me subestimou acabou beijando o chão!"*  
  > **Líder Suzaku (pós-luta):** *"Boa sorte, desafiante. Lá fora o caldo engrossa."*
- **Encontro com Kail (`DMG500:3`):**
  > **Kail:** *"Fica firme: o Teddy foi pro Centro Administrativo em Asuka City. Foi o último lugar que ele disse que ia checar."*

---

## Pontos para sua Revisão
1. O tom da **Lisa** ("despistada" caçando o Ogremon) e da **Líder de Suzaku** ficou bom?
2. Os 4 mini-arcos de Suzaku (destaque pro **Patamon da coreografia** e pro **Kyle Fortão**) estão no nível certo?
3. Posso commitar a **Fase 3** e seguir para a **Fase 4 (Setor Oeste / Byakko & Deserto)**?
