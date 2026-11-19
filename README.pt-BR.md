<p align="center"><img src="docs/images/definitive/hero.svg" width="900" alt="Digimon World 3: Definitive Edition. Ache a fraqueza. Atordoe. Mais uma. Passe o bastão."></p>

<p align="center"><a href="README.md">English</a> · <b>Português (Brasil)</b> · <a href="README.fr.md">Français</a> · <a href="README.de.md">Deutsch</a> · <a href="README.it.md">Italiano</a> · <a href="README.es.md">Español</a></p>

<p align="center">
  <b>Digimon World 3, como poderia ter sido.</b><br>
  Um remaster feito por fã que roda nativamente, sem emulador, em portáteis RK3326, celulares Android e no PC.
</p>

<p align="center">
  <sub><b>Grátis</b> · <b>sem emulador</b> · <b>traga o seu próprio disco</b> (o <i>Digimon World 2003</i> europeu) ·
  English, Português (Brasil), Français, Deutsch, Italiano, Español · versão para PortMaster em testes</sub>
</p>

<p align="center">
  <a href="#comece-aqui">Comece aqui</a> ·
  <a href="#batalhas-que-pedem-estratégia">Combate</a> ·
  <a href="#uma-jornada-que-respeita-o-seu-tempo">Progressão</a> ·
  <a href="#a-caçada">A caçada</a> ·
  <a href="#a-velvet-room">Velvet Room</a> ·
  <a href="#uma-história-que-escuta">História</a> ·
  <a href="#blast-e-a-chosen-evolution">Blast</a> ·
  <a href="#caçadas-super-rare">Super Rares</a> ·
  <a href="#jogue-em-qualquer-lugar">Plataformas</a> ·
  <a href="#o-que-falta">O que falta</a>
</p>

<a href="https://recomplauncher.com/game/digimon-world-3-definitive-edition"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

---

Em 2002, *Digimon World 3* nos deu um Mundo Digital inteiro para explorar, um time de parceiros para criar e um sistema
de batalha que, no fim das contas, só pedia que você apertasse Fight até o grind acabar. Muita gente largou o jogo por
volta do nível 40, bem antes da reta final.

A **Definitive Edition** mantém a jornada e reconstrói tudo o que a cerca. As batalhas giram em torno de fraquezas
elementais, no espírito de *Persona*: ache a brecha, atordoe o inimigo, ganhe mais uma ação, passe o bastão. A reta
final chega justamente onde os jogadores costumavam desistir. Cada região esconde presas que valem a caçada, e o que
você traz de volta paga por equipamentos que nenhuma loja jamais vendeu. Uma misteriosa sala azul se abre nas
pousadas conforme a história avança, com convidados, sonhos e uma escolha ou outra só sua. Você pode jogar como
**Kail**. E o seu save antigo continua funcionando.

<p align="center"><img src="docs/images/definitive/shots/battle-weak.gif" width="640" alt="Uma técnica de Thunder acerta uma fraqueza: WEAK!, o inimigo fica atordoado e você age de novo; o Inspect agora mostra a família que você descobriu."></p>
<p align="center"><sub>Uma técnica de Thunder encontra uma fraqueza: <b>WEAK!</b>, o inimigo fica atordoado e o turno volta a ser seu.</sub></p>

<table>
<tr>
<td width="50%" valign="top">

**⚔️ Lutas em que você pensa**<br>
Sete elementos em cinco famílias. Acerte uma fraqueza, atordoe, ganhe mais uma ação e encadeie.

</td>
<td width="50%" valign="top">

**⏳ Menos grind, a mesma jornada**<br>
A reta final chega por volta do nível 40-50. As formas sobem de nível 14× mais rápido e o grupo inteiro cresce junto.

</td>
</tr>
<tr>
<td valign="top">

**🧭 Um motivo para explorar**<br>
Sequências de vitórias fazem aparecer o Rare Target de cada região. Os materiais alimentam a forja, o Black Market e o DRI Gauntlet.

</td>
<td valign="top">

**🚪 A Velvet Room**<br>
Uma sala entre mundos que cresce com a história: um anfitrião que lê o futuro dos seus parceiros, um mestre das caçadas, um negociante.

</td>
</tr>
<tr>
<td valign="top">

**🔥 Blast e a Chosen Evolution**<br>
Um poder amaldiçoado que cobra seu preço, uma missão para purificá-lo e um parceiro que conquista uma forma Mega só dele.

</td>
<td valign="top">

**🎮 Jogue em qualquer lugar**<br>
Portáteis, Android e PC, widescreen 16:9, renderizador de GPU e shaders, e saves na nuvem com cross-play, de graça.

</td>
</tr>
</table>

> [!IMPORTANT]
> **Este repositório não contém nenhum dado do jogo.** Você precisa da sua própria cópia de *Digimon World 2003 (Europe) (En,Fr,De,Es,It)*,
> em `.bin` + `.cue` brutos, MODE2/2352, 692,146,560 bytes, SHA-1 `457cb233349ba841e03b33d8060f8fbcadd45cb3`.
> Este projeto não é afiliado nem endossado pela Bandai, pela Bandai Namco ou pela Sony.

## Comece aqui

**Portáteis (PortMaster)**

> [!NOTE]
> **A versão para PortMaster está a caminho**: o port está em fase de testes no Discord do PortMaster. Ele ainda não está
> no catálogo do PortMaster, mas já rodou, sob emulação, em todas as custom firmwares da matriz de testes.

1. Baixe o `dw3.zip` nas [releases](https://github.com/al4xdev/digimon-world-3-definitive-edition/releases) e
   descompacte na pasta `ports` do seu PortMaster.
2. Copie a imagem do seu disco para `ports/dw3/game/`.
3. Abra o *Digimon World 3* pelo menu de ports.

Dispositivos testados, detalhes de instalação e problemas conhecidos: [`.ci-cd/portmaster/README.md`](.ci-cd/portmaster/README.md).

**Android (arm64, Android 7 ou mais novo)**

1. Baixe o `dw2003-*-android-arm64-v8a.apk` nas
   [releases](https://github.com/al4xdev/digimon-world-3-definitive-edition/releases) e instale (permita instalar pelo
   navegador ou pelo gerenciador de arquivos).
2. Toque em **JOGAR** e escolha o seu `.bin` (e o `.cue`) no seletor de arquivos do sistema: o app copia os dois.
3. As setas no canto superior direito movem e redimensionam os controles de toque; o botão do canto esquerdo esconde
   os controles. Um controle físico também funciona, e o toque sai da frente enquanto ele está conectado.

**PC (Linux e Windows)**

1. Descompacte a versão para PC (um AppImage no Linux, um `.zip` no Windows), ou rode `./launcher/dw3-launcher --sdl`
   a partir de um checkout já compilado.
2. Escolha o seu `.cue` / `.bin` na primeira execução (dá para trocar quando quiser em `GAME > DISC IMAGE`).
3. Abra `VIDEO` para ativar **Widescreen (16:9)**, **Fullscreen**, a escala **`FILL` ou `INTEGER`**, o renderizador
   por **GPU** ou por software e os **filtros** de tela.

**Vindo de uma versão antiga?** O seu save abre e funciona. Para dar a ele as novas tabelas de MP e TP, rode uma vez
`tools/convert_save_mp.py card1.mcd` (ele guarda um backup).

---

## Batalhas que pedem estratégia

<p align="center"><img src="docs/images/definitive/combat-loop.svg" width="800" alt="Acerte uma fraqueza: WEAK!, o inimigo fica atordoado e você age de novo. Depois, acerte outra família fraca, use o Baton Pass, ou qualquer outro golpe (+0.2, o atordoamento termina). OVERFINISH: um inimigo nocauteado paga 10% de cada golpe em Bits."></p>

Todo inimigo tem fraquezas e resistências em **cinco famílias de elementos**: Fire, Water + Ice, Wind,
Thunder + Machine e Dark. Elementos pareados sempre compartilham o mesmo valor, então uma família inteira é fraca,
neutra ou resistente de uma vez.

- **Acerte uma fraqueza** com uma técnica elemental: um **WEAK!** inclinado, o inimigo **fica atordoado** e você ganha
  **ONE MORE** ação. Cada família fraca rende uma vez por rodada, então duas fraquezas encadeiam várias ações seguidas.
- **Enquanto ele está atordoado**: acerte outra fraqueza, troque de parceiro com um **Baton Pass** gratuito e mantenha
  o seu turno (uma vez por rodada), ou desfira qualquer outro golpe por um bônus que encerra o atordoamento.
- **OVERFINISH**: nocauteie um inimigo com um golpe que dá mais uma ação e ele continua caído para recebê-la. Cada
  golpe nele rende Bits.
- **Você abre a luta**, a não ser que seja emboscado. Um lado muito mais rápido age duas vezes, e o HUD avisa
  ("Enemy x2").
- **Você também ouve**: um som diferente para fraqueza, para golpe neutro e para golpe resistido.

<table>
<tr>
<td width="50%" align="center"><img src="docs/images/definitive/shots/battle-baton-pass.gif" alt="Com o inimigo atordoado, o Baton Pass coloca o WarGreymon em campo sem perder o turno."><br><sub><b>Baton Pass</b>: uma troca gratuita que mantém o seu turno</sub></td>
<td width="50%" align="center"><img src="docs/images/definitive/shots/battle-overfinish.gif" alt="Golpes num inimigo nocauteado mostram OVERFINISH! e rendem Bits."><br><sub><b>OVERFINISH</b>: o inimigo fica no chão, e cada golpe rende</sub></td>
</tr>
</table>

<p align="center"><img src="docs/images/definitive/weakness-discovery.svg" width="800" alt="O Inspect começa em ???. Um golpe de Fire mostra RESIST, um golpe de Ice mostra Water + Ice WEAK, o Resist Down transforma o Wind neutro em fraqueza, e o Libra revela o resto."></p>

**Conheça o seu inimigo.** Nada fica escondido, mas você precisa descobrir. O **Inspect** (`L2`) mostra as cinco
famílias do inimigo conforme você as descobre, `???` até lá; o **Libra** revela todas de uma vez; o **Bestiary**
guarda o que você aprendeu sobre cada espécie, com os seus Hunt Materials. Toda lista de técnicas mostra o elemento de
cada técnica, e cada descrição foi reescrita a partir do que a técnica realmente faz ("Magia de fogo. Chance de
sono.").

**Os seus atributos mudaram de função.** Os antigos atributos de "tolerância" elemental do seu Digimon viraram
**afinidade**: eles não defendem mais, e sim reforçam os seus ataques daquele elemento contra uma fraqueza ou um
inimigo atordoado. A **Defense** agora protege contra magia, e o **Spirit** é puramente o seu ataque mágico.

<p align="center"><img src="docs/images/definitive/damage-scale.svg" width="800" alt="Multiplicador: resistido 0.5, neutro 1.0, sem elemento atordoado 1.2, neutro atordoado 1.2 a 1.8, fraqueza 1.5 a 2.1, fraqueza atordoado 1.7 a 2.9. Afinidade: os seus antigos atributos elementais agora reforçam os seus ataques."></p>

**Buffs que viram plano.** Buffs e debuffs vêm em três níveis fixos e nunca se acumulam: vale o mais forte.
Chefes são imunes a status, **não** a debuffs: o **Resist Down** e o **Mega Break** conseguem abrir uma fraqueza que
não existia. Quatro técnicas foram refeitas para isso (Libra, Relay, Resist Down, Mega Break), e os rookies que
passavam fome de MP não passam mais.

<p align="center"><img src="docs/images/definitive/hud.svg" width="800" alt="HUD de batalha: Item L1, Tech R1, Fight B, Run X, Digivolve Y, Tag A, Inspect L2, Blast R2. Reconfigurável em BATTLE HUD no launcher."></p>

**Um HUD de batalha com um botão para cada ação**, num layout em formato de controle e reconfigurável pelo launcher.
Ele lembra a sua última técnica e o último item, guarda os comandos apertados cedo demais, e o texto de batalha não
interrompe mais a luta.

<p align="center"><img src="docs/images/definitive/shots/battle-inspect.png" width="640" alt="O HUD de batalha com botões diretos e o painel do Inspect mostrando as fraquezas descobertas até agora."></p>

<details>
<summary><b>Os números, para os curiosos</b></summary>

| O golpe | Multiplicador |
|---|---|
| Elemento resistido | 0.5 |
| Elemento neutro | 1.0 |
| Fraqueza | 1.5 + affinity |
| Elemento neutro, alvo atordoado | 1.2 + affinity |
| Fraqueza, alvo atordoado | 1.7 + 2 × affinity |
| Sem elemento, alvo atordoado | 1.2 |

A afinidade chega a 0.3 para Fire e Wind e a 0.6 para os pares e Dark, então o golpe mais forte é 2.3 ou 2.9.
Os níveis de buff são +20 / +35 / +50% e os de debuff −15 / −25 / −35%. O cálculo completo, passo a passo e com um
exemplo prático, está na [referência de combate](docs/definitive-edition/combat.md).

<p align="center"><img src="docs/images/definitive/buffs.svg" width="800" alt="Buffs +20/+35/+50%, debuffs -15/-25/-35%, com duração de 2 a 4 turnos. Nunca se acumulam: o nível mais forte vence, e o sinal oposto sobrescreve."></p>

</details>

## Uma jornada que respeita o seu tempo

<p align="center"><img src="docs/images/definitive/progression.svg" width="800" alt="Pontos de treino somados: a Definitive Edition dá 7, 9, 30 e depois 0 TP por nível, 613 até o nível 50, contra os 5 por nível do original, 490 até o nível 99. Também: treino garantido após três sessões imperfeitas, formas sobem de nível 14x mais rápido, o grupo inteiro ganha XP, boosters caem de 15 em 15."></p>

- **Mais pontos de treino, mais cedo.** No nível 50, um Digimon já ganhou mais TP do que o original dava até o nível 99
  (613 contra 490), e os níveis 41 a 50 são onde você molda os atributos dele.
- **Treino garantido.** Três sessões sem resultado perfeito e a quarta dá certo em todas as rodadas.
  O treino de resistência não custa mais um atributo.
- **Formas sobem de nível 14× mais rápido**, então experimentar uma nova digivolução é divertido em vez de tarefa chata.
- **O grupo cresce junto**: um parceiro no banco ganha a mesma experiência, e as três formas escolhidas de cada
  Digimon ganham experiência de forma, tenham lutado ou não.
- **Requisitos na tela**: o Digi-Lab mostra o que cada próxima forma exige e o quanto você já avançou ("Greymon Lv 5 (3/5)").
  A animação de digivolução toca completa uma vez e depois em versão curta; segure um botão para pulá-la.
- **Card boosters caem de 15 em 15**, então montar um deck não exige uma segunda jogada completa.
- **Hard Mode**, escolhido em New Game: o seu grupo tem dois Digimon em vez de três, durante o jogo inteiro.
- **Super Hard**, a qualquer momento: leve o Demon Bell da Velvet Room e os inimigos ganham 50% mais HP e velocidade,
  mas pagam o dobro de Bits e de materiais. Devolva-o para desligar.

## A caçada

<p align="center"><img src="docs/images/definitive/hunt-chain.svg" width="800" alt="A cadeia da caçada: cada batalha aleatória vencida numa região soma um; depois de 5 vitórias o Rare Target pode aparecer (20% na sexta, certo na décima). Ele luta sozinho com as cores giradas em 120 graus e dá o Core raro da região, um pacote de R-Booster e 10x de experiência."></p>

Opcional e aberta durante o jogo inteiro. Cada batalha aleatória que você vence soma na **hunt chain** da região onde
você está. Depois de cinco vitórias, o próximo encontro pode ser o **Rare Target** da região: um chefe recolorido e mais
forte, sozinho em campo, garantido na décima vitória. Cada uma das 71 regiões tem a sua própria espécie, e cada uma
solta o seu próprio **Core** para a sua coleção.

<p align="center"><img src="docs/images/definitive/economy-flow.svg" width="800" alt="Da caçada ao seu equipamento: materiais comuns das batalhas aleatórias e Cores raros dos Rare Targets vão para a forja de cada loja, o Black Market e o DRI Gauntlet, que dão equipamentos de loja, 16 réplicas e os acessórios Power."></p>

- **Hunt Materials**: dezesseis comuns por faixa de nível, do Bit Shard ao Obsidian Digizoid, e 71 Cores raros. O
  Bestiary diz quem solta o quê.
- **A forja**: as armas, armaduras e acessórios de cada loja pedem os materiais das regiões ao redor. A descrição
  mostra o que você tem e o que falta.
- **41 variantes de equipamento** construídas em torno de um estilo: armas de Speed, equipamentos Elemental que
  aumentam a sua afinidade, armaduras Tank, acessórios Trade-off.
- **O Black Market**: troque um Core por doze comuns, ou os Cores de dois chefes por uma das dezesseis **réplicas** que
  nenhuma loja vende (e, no fim da lista, uma marca que todo caçador quer).
- **O DRI Gauntlet**: os DRI Agents reencenam batalhas clássicas de Tamers. Uma luta, três etapas, de Champion a
  Ultimate a Mega, com um **DIGIVOLVE!** entre elas, e um acessório Power para cada etapa que você derrubar.
- **Os preços** dos equipamentos avançados subiram e o valor de revenda caiu, então um achado de sorte já não vende por
  uma fortuna e os Bits do OVERFINISH têm onde ser gastos.

<p align="center"><img src="docs/images/definitive/shots/black-market-replicas.png" width="640" alt="O Black Market do Oikawa: réplicas lendárias custam Cores raros."></p>
<p align="center"><sub>As réplicas do Black Market: os Cores de dois chefes e os Bits, e o equipamento é seu.</sub></p>

<details>
<summary><b>As cinco linhas do DRI Gauntlet</b></summary>

| Linha | Etapas e níveis | Recompensas |
|---|---|---|
| Terriermon (Guardromon) | Guardromon 24, Andromon 27, BK MegaGargomon 30 | Metal Power 1, 2, S |
| Veemon | Devidramon 32, Paildramon 35, BK Imperialdramon 38 | Bolt Power 1, 2, S |
| Patamon | Airdramon 40, MagnaAngemon 43, BK Seraphimon 46 | Wind Power 1, 2, S |
| Agumon | Tyrannomon 48, MetalGreymon 51, BK WarGreymon 54 | Fire Power 1, 2, S |
| Guilmon | Flarerizamon 56, WarGrowlmon 59, BK WarGrowlmon 62 | Dark Power 1, 2, S |

Perder não é game over: você volta ao campo como estava. Os custos de desbloqueio, as receitas da forja e todas as
variantes estão na [referência da caçada](docs/definitive-edition/hunt-and-economy.md).

</details>

## A Velvet Room

<table>
<tr>
<td width="50%" align="center"><img src="docs/images/definitive/shots/velvet-way-in.gif" alt="Uma luz azul suave na pousada marca a entrada para a Velvet Room."><br><sub>Uma luz azul suave numa pousada, e o caminho se abre</sub></td>
<td width="50%" align="center"><img src="docs/images/definitive/shots/velvet-room.png" alt="A Velvet Room e os seus convidados."><br><sub>A sala no auge, com o Kail como herói</sub></td>
</tr>
</table>

Em algum lugar do Mundo Digital, uma sala azul espera entre os servidores. No começo, as portas estão fechadas. Então
um boato começa a correr por Seiryu City e, em cada pousada, uma suave **luz azul** começa a pulsar sobre um pedaço
do cenário. Fique parado nela, aperte Circle, e pronto: você está lá dentro.

<p align="center"><img src="docs/images/definitive/velvet-room.svg" width="800" alt="A Velvet Room cresce com a história: Sepikmon no passo 4, Igor e Gennai no 10, Guilmon com o Demon Bell no 16, Etemon e o porco no 22, o Black Market do Oikawa depois que ele é derrotado, e a Time Room antes do ponto sem volta."></p>

A sala **cresce com a história**. Os convidados chegam um a um:

| Convidado | O que faz |
|---|---|
| **Igor**, o anfitrião | lê as formas de DNA dos seus parceiros e o que ainda falta para elas; de vez em quando, sonha com você |
| **O Sepikmon** | o mestre das caçadas: caçadas Super Rare, os cristais delas e as skins de herói que desbloqueiam |
| **Gennai** | a missão do Blast e a Chosen Evolution |
| **Guilmon** | guarda o Demon Bell, o Super Hard sob demanda |
| **Etemon** | ao piano. Ele já ouviu esta música em outro jogo |
| **O porco** | um ator que o jogo trouxe e nunca usou, finalmente no palco |
| **Oikawa** | atrás do balcão, quando você tiver merecido: o Black Market |

A sala tem o seu próprio arranjo da música, e uma **rede de Guardromons** liga as pousadas: salve ou teleporte-se
de qualquer Guardromon para os que você já encontrou.

## Uma história que escuta

A Definitive Edition não muda para onde a história vai. Ela muda como é contada e dá a você uma voz nela.

- **Os sonhos do Igor.** Em alguns pontos de virada, ele puxa você sozinho para a sala e pergunta alguma coisa. Não
  existe resposta errada, e ele se lembra do que você disse.
- **As suas escolhas, e os ecos delas.** 111 cenas agora deixam o herói responder com as próprias palavras, e umas 600
  falas posteriores mudam conforme o que você escolheu.
- **Um roteiro reescrito**: as falas infantilizadas ficaram adultas, a ordem e as missões ficaram intactas, e o herói
  foi escrito para quem estiver jogando.
- **Uma figura encapuzada** bloqueia o caminho no Bug Maze, e **uma sala onde o tempo para** espera antes do ponto sem
  volta, para garantir que você está pronto.

## Blast e a Chosen Evolution

<p align="center"><img src="docs/images/definitive/blast.svg" width="800" alt="Blast: encha o gauge com WEAK, atordoamento, ações extras e OVERFINISH, dispare com R2, assuma uma forma sombria e pague o preço. Purifique dez formas para conquistar a Chosen Evolution."></p>

Vença a caçada Super Rare do tutorial e um novo poder desperta. Lutar bem enche o **Blast Gauge**: fraquezas,
atordoamentos, OVERFINISH. Quando ele está cheio, `R2` libera a **forma sombria** do seu parceiro. É poder, e tem um
preço: a batalha termina com HP e MP em 1, atributos reduzidos até a próxima pousada e um parceiro superaquecido
demais para digivolver. Alguém na Velvet Room sabe como purificá-lo, se você puder confiar nessa pessoa. No fim dessa
estrada, um parceiro conquista a **Chosen Evolution**: uma forma Mega só dele, sem maldição.

## Caçadas Super Rare

<p align="center"><img src="docs/images/definitive/super-rare-compass.svg" width="800" alt="Caçadas Super Rare: uma bússola de temperatura (COLD a BURNING) leva você pelo território do alvo até o ponto dele. Depois de perder 25% da barra, ele foge para outro mapa, mantendo o dano, e volta com 20% mais atributos a cada vez. 13 caçadas em ordem; cada vitória dá um cristal e uma skin de herói."></p>

O quadro do Sepikmon lista **doze Digimon Super Rare** (e um Ogremon de tutorial para aprender), do nível 29 ao 61,
liberados um depois do outro. Cada um se esconde num mapa aleatório do seu território, e uma **bússola de temperatura**
avisa quando você está esquentando. Machuque-o e ele foge, mantendo o dano e voltando mais forte. Cada um que você
derrota paga um cristal e desbloqueia uma **skin de herói**.

## O seu herói, do seu jeito

**Girls Power**: em New Game, escolha **Kail** ou **Junior** como herói. Os papéis se invertem nos sprites de campo,
nas cutscenes, nas ações de campo e nos menus, com cada frame que o Kail não tinha desenhado à mão, e o texto da
história acompanha a troca. Treze **skins de herói** das caçadas Super Rare recoloram o cabelo e a roupa do seu herói.

## Conforto e qualidade de vida

- **GUIDE**: o passo atual da missão principal, as missões secundárias abertas e todos os dezesseis leilões da Asuka,
  com um atalho direto para um que esteja aberto. Um banner desliza na tela quando um objetivo muda.
- **Overlevel Rush**: quando o seu Digimon mais forte está 8 ou mais níveis acima de todos os inimigos de uma batalha
  aleatória, o jogo oferece pular a luta enquanto você continua andando, e as recompensas deslizam no alto da tela.
- **Texto de batalha que não interrompe a luta**, um marcador de objetivo no mapa, avanço rápido, glifos de botões que
  combinam com o seu controle (Xbox, Nintendo, PS1) e remapeamento completo.
- **Modo 60 Hz**: o disco europeu roda a 50 Hz; o modo de 60 Hz do próprio jogo, com a velocidade compensada, está a
  uma configuração de distância.

<table>
<tr>
<td width="50%" align="center"><img src="docs/images/definitive/shots/overlevel-rush.gif" alt="Uma luta fácil demais: use o Rush, e as recompensas deslizam enquanto você continua andando."><br><sub><b>Overlevel Rush</b>: pule a luta fácil, fique com as recompensas</sub></td>
<td width="50%" align="center"><img src="docs/images/definitive/shots/guide-quest.png" alt="O Guide do jogo mostra o passo atual da sua missão principal."><br><sub><b>GUIDE</b>: para onde ir agora, numa frase</sub></td>
</tr>
</table>

## Jogue em qualquer lugar

| Onde | O que você leva |
|---|---|
| **Portáteis RK3326** via PortMaster (o XF35H, o R36S e dispositivos parecidos) | ajustado de ponta a ponta num XF35H de verdade: framebuffer direto, renderizador GLES, vídeos em velocidade total, a barra de LED. Todas as custom firmwares da matriz de testes rodaram sob emulação; relatos de dispositivos reais são bem-vindos |
| **PC Linux e Windows** | renderizador de GPU SDL3 (Vulkan, D3D12), **widescreen 16:9** nas batalhas e no campo, tela cheia HighDPI com escala `FILL`, filtros `SHARP` / `SCANLINES` / `CRT` / `SMOOTH`, avanço rápido de até 8× |
| **Android** (arm64) | um APK com **widescreen 16:9**, controles de toque grandes que você move e redimensiona na tela (ou esconde), qualquer controle USB ou Bluetooth, e um seletor de disco que copia sua imagem para dentro do app |

<p align="center"><img src="docs/images/definitive/cross-play.svg" width="800" alt="Cloud Sync de cross-play: o launcher sincroniza o card1.mcd entre portáteis e PC por meio de um Cloudflare Worker gratuito. Pareie um dispositivo com um código de 6 dígitos que o host aprova, escolha um lado quando os dois mudarem, e mantenha de 1 a 100 snapshots locais rotativos."></p>

**Saves com cross-play.** O **Cloud Sync** gratuito mantém o seu memory card em dia entre um portátil, um celular Android e um PC: pareie um
dispositivo com um código de 6 dígitos, resolva conflitos, revogue um aparelho perdido e guarde até 100 snapshots
locais, com restauração em um botão. Um launcher pensado para console cuida do disco, do vídeo, dos controles, do HUD
de batalha e dos saves, e ainda exibe um joguinho de corrida com Kumamon, Guilmon e Agumon enquanto o seu disco é
verificado.

Se você testar em outro dispositivo, um relato no
[issue tracker](https://github.com/al4xdev/digimon-world-3-definitive-edition/issues) ajuda muito.

## O que falta

O jogo é jogável do começo ao fim, e todos os sistemas descritos acima estão implementados. Ele também é jovem: parte
dele só foi jogada algumas vezes, então espere arestas e hotfixes frequentes. O que resta:

- **O equilíbrio geral do jogo.** Até agora ele foi balanceado só por cálculo. Estou jogando em paralelo e lançando
  hotfixes pelo caminho.
- **A Velvet Room** ainda parece rasa em alguns pontos (alguns convidados e momentos precisam de mais o que dizer e
  fazer); hotfixes estão a caminho.
- **A batalha final** vai ganhar uma extensão. Ela não muda a história, e todos os outros sistemas já estão
  implementados.
- **Ajustes para outras plataformas e dispositivos**, além do XF35H e do PC em que foi afinado.

A lista detalhada dos itens em aberto está no [roadmap](docs/definitive-edition/roadmap.md).

## Por baixo do capô

Isto é construído sobre a [decompilação do Digimon World 2003](https://github.com/gascarcella/dw2003recomp) e o port
dela para PC, que são trabalho do projeto original: todo arquivo do jogo é reconstruído byte a byte idêntico a partir de
C equivalente. Este fork acrescenta o caminho para portáteis e a Definitive Edition.

- **Como a Definitive Edition funciona**, cada regra, número, arquivo e decisão de design:
  [`docs/definitive-edition/`](docs/definitive-edition/README.md).
- **A decompilação e o port**, formatos, desempenho, o trabalho de GPU: [`docs/`](docs/README.md).

| Para | Comando | Detalhes |
|---|---|---|
| O binário e o pacote para portáteis ARM | `.ci-cd/portmaster/build_arm.sh` | [`docs/HANDHELD.md`](docs/HANDHELD.md#building) |
| O zip do PortMaster | `.ci-cd/portmaster/make_release.sh` | [`.ci-cd/portmaster/PUBLISHING.md`](.ci-cd/portmaster/PUBLISHING.md) |
| O port para PC e os testes | veja o guia | [`docs/BUILDING.md`](docs/BUILDING.md) |

## Contribuindo e suporte

Leia primeiro o [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md). Relate bugs e tire dúvidas no
[issue tracker](https://github.com/al4xdev/digimon-world-3-definitive-edition/issues).

## Créditos e licença

A decompilação e o port para PC são obra do [projeto original](https://github.com/gascarcella/dw2003recomp).
Este fork acrescenta a Definitive Edition e o caminho para portáteis: o present direto no framebuffer, a geração de
código para Cortex-A35, o trabalho de vector span, o layout de linhas da VRAM, o medidor de frames e os extras para
portáteis, o renderizador GLES, o trabalho de áudio e o pacote. O trabalho próprio do projeto está sob a
[licença MIT](LICENSE); o jogo continua sendo propriedade da sua publisher.

Obrigado ao juandav e ao [ReGame-Labs/dw3_decomp](https://github.com/ReGame-Labs/dw3_decomp), a outra decompilação de Digimon World 3, cujos nomes e formas
de função ajudaram a fechar as últimas funções; cada ideia aproveitada está listada em
[`docs/THIRD_PARTY.md`](docs/THIRD_PARTY.md).

A Velvet Room é uma homenagem à série *Persona*, da Atlus, e as caçadas (Rare Targets, materiais, a forja) a
*Monster Hunter*, da Capcom. *Digimon* pertence à Bandai / Bandai Namco, *Persona* à Atlus e *Monster Hunter* à
Capcom; este projeto de fã não tem ligação com nenhuma delas nem o apoio delas.
