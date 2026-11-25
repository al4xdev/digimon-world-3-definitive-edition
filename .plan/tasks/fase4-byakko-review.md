# Revisão da Fase 4 — Retorno a Asuka (Etapa 2) + Setor Oeste / Byakko City (PT-BR)

> Documento para revisão antes de avançarmos para a **Fase 5 (Setor Norte / Genbu & Queda de Asuka)**.  
> Todas as falas seguem a **regra de gênero neutro** para o protagonista e evitam NPCs de Card/Digimon Battle.

---

## 1. Continuação dos NPCs Antigos de Asuka City — Etapa 2 (`progress 14+`)

Quando você volta de Suzaku para Asuka City atrás do Teddy/Centro Administrativo, o **balãozinho animado (`...`) acende de novo** sobre os NPCs antigos com a continuação dos mini-arcos iniciados na Fase 1:

| NPC | Fala de Etapa 2 | Como lembra da tua escolha da Fase 1 |
|---|---|---|
| **Mika** (`0x200`) & **Patamon** | `TALK00 624` + `614` (`Q200`) | A briguinha (*"não quer brincar comigo"* / *"só perguntei se queria ir pra casa"*) reage se você mandou treinar no Ginásio, chamou de pão de queijo, pediu paciência ou ignorou. |
| **Masato** (`0x200`) | `TALK00 666` (`Q201`) | Em pânico preso no servidor lembrando do encontro perdido / da tua ironia / da tua vitória em Seiryu. |
| **Agumon Coach** (`0x200`) | `TALK00 603` (`Q202`) | Solta a pista do Centro Administrativo (*"só Digimon entra, humano não!"*) no tom da tua relação com ele. |
| **Vovô Jake** (`0x201`) | `TALK00 642` (`Q203`) | Insiste no sonho de virar Digimon mesmo depois do teu aviso/gelo. |
| **Jack Maníaco** (`0x201`) | `TALK00 712` (`Q204`) | Ainda procurando o Guilmon e te culpando pela "zica" se você perguntou dele antes. |

---

## 2. Novos Mini-Arcos — Fase 4 (Byakko City: 4 Mini-Arcos em 2 Mapas)

| Mapa | NPC (Ator) | Etapa 0 (`progress 0..30`) | Opções (`Q218, Q219, Q226, Q227`) | Etapa 1 (`progress 31+`) |
|---|---|---|---|---|
| **Praça Byakko** (`0x25D`) | **Maya** (`54`) | `TALK01 327` (`Q218`) — Aliviada por sair do Deserto de Mobius | **1:** Acabou o sufoco! (`rep:+1`)<br>**2:** Areia até na orelha, né? (`teddy:+1`)<br>**3:** Olho vivo com a A.o.A (`ivy:+1`)<br>**0:** Ignorou | `TALK01 328`: sonha em ver o Servidor Asuka do alto reagindo ao teu papo. |
| **Praça Byakko** (`0x25D`) | **Vovó Plum** (`58`) | `TALK01 332` (`Q219`) — Pergunta quando haverá paz de verdade | **1:** A gente vai resolver! (`rep:+1`)<br>**2:** A coisa tá feia mesmo (`ivy:+1`)<br>**3:** Descansa um pouco, vovó (`teddy:+1`)<br>**0:** Ignorou | `TALK01 333`: perde o sono com medo da A.o.A voltar, lembrando da tua promessa, sinceridade ou do chá. |
| **Sala Byakko** (`0x25E`) | **Estudante Tony** (`47`) | `TALK01 189` (`Q226`) — Quase caiu no golpe da A.o.A de sequestrar Tamers | **1:** Escapou de boa, hein! (`teddy:+1`)<br>**2:** Tem que abrir o olho! (`lisa:+1`)<br>**3:** Eu derrubo eles todos (`keith:+1,rep:+1`)<br>**0:** Ignorou | `TALK01 194`: preocupado com o amigo **Tatsuya** no Servidor Amaterasu tomado pela A.o.A. |
| **Sala Byakko** (`0x25E`) | **Rep. Aiko** (`48`) | `TALK01 190` (`Q227`) — Elogia o Líder de Byakko por retomar a cidade | **1:** Ele mandou bem demais! (`rep:+1`)<br>**2:** Ainda falta descobrir tudo (`lisa:+1`)<br>**3:** Eu também ajudei, pô! (`keith:+1`)<br>**0:** Ignorou | `TALK01 195`: concorda contigo (ou pede desculpa por esquecer tua ajuda!) enquanto tenta entender o plano real da A.o.A. |

---

## 3. Prompt Síncrono de Batalha Adicionado (Fase 4)

| Prompt | Gatilho | Escolhas & Efeitos Nativos |
|---|---|---|
| **Emboscada da A.o.A em Byakko** (`Q225`) | Chefe (`type 2`, `progress 19..28`, Turno 1) | • *"Armadilha? Vocês que tão presos comigo!"* → **ATK +2** (`keith:+1, rep:+2`)<br>• *"Analisa esse esquema deles!"* → **Libra** revela fraquezas (`lisa:+1, rep:+1`)<br>• *"Espera... foi cilada mesmo?!"* → **DEF -1** (`teddy:-1, rep:-1`) |

---

## 4. Exemplos de Falas Reescritas (História Principal — Fase 4)

- **Numemon & Falsa Byakko City (`DMG500:4–5`):**
  > **Numemon:** *"Meia-volta, agora! É pro teu próprio bem. ...Por favor. Não cai nessa furada."*  
  > **Numemon:** *"O Líder. Os moradores. É tudo teatro. Tudo impostor da A.o.A."*
- **Vovô Jake (Etapa 2 em Asuka):**
  > **Jake:** *"Você disse pra tomar cuidado... mas o sonho fala mais alto! Hyo hyo!"*

---

## Pontos para sua Revisão
1. Gostou da continuação (Etapa 2) dos NPCs antigos de Asuka acendendo o balãozinho de novo quando você volta de Suzaku?
2. Posso commitar a **Fase 4** e seguir para a **Fase 5 (Setor Norte / Genbu & Queda de Asuka)**?
