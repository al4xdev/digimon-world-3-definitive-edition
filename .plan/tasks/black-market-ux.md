# Black Market: UI/UX pass (pedido do dono, 2026-11-16)

Pedido: "faz um UI/UX na tela do black market, para o player, como game design, para falar o que o item faz etc., e
ajustar a legibilidade". Código: `port/game/definitive_market_screen.c` (as regras de `definitive_market.c` não
mudaram).

Estado (2026-11-17): **feito e testado; não commitado.** Esta tarefa é só o menu. A Velvet Room é de outra sessão.

## O que estava ruim

- A fonte pequena (laranja) não tem `/`, `,`, `:` nem acentos: "30/99" saía "30099" e o custo saía "Datamon II0 BK…".
- A cor normal e a de destaque eram o mesmo laranja.
- O preço que o jogador não pode pagar ficava azul, sem explicação.
- Ao escolher o material, só se via um por vez. Os Bits só apareciam no detalhe da réplica.
- Nada explicava o que é um Core, o que a réplica faz ou quem pode equipá-la.

## O que mudou

- **Fonte:** tudo na fonte dos diálogos (a 1), branca e com acentos e pontuação.
- **Cores com sentido fixo:**
  - branco: texto;
  - laranja: linha do cursor, ou o que falta;
  - verde: deu certo, ou já é do jogador;
  - cinza: o que não vale agora, e as explicações.
- **Lista:**
  - cabeçalho ("Your Rare Cores", "Get 12 of which material?", "Legendary Replicas");
  - posição na lista ("3/16");
  - 6 linhas, com uma faixa fixa na linha escolhida: azul na lista, marrom ao trocar ou comprar.
- **Troca:**
  - na lista de Cores, o detalhe diz quanto um Core vale aqui, para que servem os materiais e que as réplicas também
    gastam Cores (sem Cores, diz de onde eles vêm);
  - o × abre a lista dos 16 materiais com "tem/99" (cinza o que não cabe +12);
  - o detalhe mostra "Give: … Held: 3", "Get: … x12 Held: 30/99" e o aviso de que não cabe.
- **Réplicas:** detalhe em 4 linhas:
  1. descrição da loja (ITMINF);
  2. tipo e status, lidos do item como a loja lê (arma, duas mãos, armadura, acessório; Força, Defesa…, elementos);
  3. **quem do time pode equipar.** A maioria das armas serve em 1 a 3 dos 8 Digimon. Fica laranja quando nenhum do
     time pode;
  4. quais Cores a compra gasta.
  - Na confirmação aparecem o custo e o aviso de revenda 0. Se ninguém pode usar, o aviso fica na tela, mas a compra
    não é bloqueada, porque um Digimon que entrar depois pode usar.
- **Rodapé:** os Bits do jogador sempre visíveis, em laranja quando a réplica escolhida custa mais.
- **Textos:**
  - strings novas em `en.txt`/`pt.txt` e no glossário de `tools/translate_ptbr.py`;
  - removidos `$BMCost` e `$BMPrice`;
  - os elementos reusam `$Fire`…`$Dark` do Inspect;
  - uma linha de detalhe mais larga que o painel vai para o log ("market: detail line … pixels wide").
- **PT:** traduzidos ITMNAM 356–359, que estavam em inglês: Poder Máquina S, Poder de Trevas 1/2/S.

## Testes

- `tests/port/definitive_market.py`: pass.
- `tests/port/run.py --exe build/port-ux/dw2003`: pass.
- Screenshots em EN e PT com `.dumps/tasks/market/bmshot.py`:
  - lista, troca, materiais cheios;
  - todas as réplicas, compra e confirmação;
  - sem Cores, sem Bits, sem chefes;
  - nenhuma linha estourou.
- O build usado foi `build/port-ux`, para não disputar `build/port` com a outra sessão.

## Pendências pequenas (fora do escopo, nomes PT que já existiam)

- "Crest de Recuper" (ITMNAM 320, cortado);
- "Pulse Guard" sem tradução;
- "Shard de Bits/Bytes" meio em inglês.
