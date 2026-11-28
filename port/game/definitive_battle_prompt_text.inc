/* The battle prompts' words (definitive_battle_prompt.c), one table per language in def_lang's order: English,
 * PT-BR, French, German, Italian, Spanish. Entry i is prompts[i]: who speaks, the line, then each option's label
 * and the result shown once it is picked. Names follow each language's own disc text (Dresseur Genji, Gov. di
 * Seiryu, Maestro Tyranno, ...), stat tags the port's SKLINF overrides (ATQ/DÉF/VIT, ANG/ABW/TEMPO, ...).
 * Fit at 14 px: label <= 256 px, line <= 274, result <= 278 (the bond's words get a line of their own); only
 * ASCII and the Latin-1 letters bp_latin maps reach the font (no Œ, no Î: MAITRE DE JEU). */
static const DefBattleText bp_texts[6][BP_PROMPTS] = {
    { /* English */
        { "TAMER GENJI", "Ho ho! Show me your Tamer's grit!", {
            { "Bring it on, old-timer!", "Spirits high! Attack rose (+1)!" },
            { "I'll read your moves first!", "Sharp eye! Libra found the weak spots!" },
            { "Hold on, I skipped the manual...", "Fumbled the start! Defense fell (-1)!" },
        } },
        { "KEITH", "Beat Seiryu on luck and now you're cocky?!", {
            { "Come find out!", "Blood boiling! Attack rose (+2)!" },
            { "I'll find your opening!", "Libra on! Weak spots revealed!" },
            { "Guard up and wait!", "Solid stance! Defense rose (+2)!" },
        } },
        { "MASTERTYRANNO", "Nice hits! But I'm still digesting lunch!", {
            { "Then take this!", "Broke his rhythm! Foe Defense fell (-1)!" },
            { "Keep the tempo, no rush!", "Perfect tempo! Speed rose (+1)!" },
            { "Uh, this dinosaur is way too big...", "Chickened out! Attack fell (-1)!" },
        } },
        { "SEIRYU LEADER", "Good fight! Can you stand Seiryu's wind?", {
            { "I'm not budging!", "Solid as a rock! Defense rose (+2)!" },
            { "I've read your rhythm!", "Sharp read! Speed rose (+1)!" },
            { "This little breeze scares nobody!", "Too much taunting! Foe Attack rose (+1)!" },
        } },
        { "SUZAKU LEADER", "Don't underestimate me! My flames bite!", {
            { "Then turn up the heat!", "Ring on fire! Attack rose (+2)!" },
            { "Time to read your game!", "Libra exposed the Leader's weak spots!" },
            { "Whoa, take it easy...", "Slipped up! Defense fell (-1)!" },
        } },
        { "BYAKKO LEADER", "Show your strength... and expect no mercy!", {
            { "I'm not here to play around!", "Ice cold! Attack rose (+2)!" },
            { "Something's off with that style!", "Libra exposed the Leader's weak spots!" },
            { "That smirk is way too suspicious...", "Hesitated! Defense fell (-1)!" },
        } },
        { "GAME MASTER", "Cute stubbornness... Too bad it ends here.", {
            { "For Teddy and for Asuka!", "Unshakable will! Attack rose (+2)!" },
            { "Brace for the counter!", "Wall up! Defense rose (+2)!" },
            { "Shut up! I'll finish you!", "Lost your cool! Foe Speed rose (+1)!" },
        } },
        { "CHIEF OFFICER", "Still at it?! Destromon is invincible!", {
            { "Your empire ends today!", "Clean hit! Attack rose (+2)!" },
            { "Hold the line, team!", "Guard locked! Defense rose (+2)!" },
            { "Destromon's going to be scrap!", "Too much taunting! Foe Attack rose (+1)!" },
        } },
        { "LORD MEGADEATH", "Humanity is finished! Bow to your god!", {
            { "You're no god! Down you go!", "Spirit ablaze! Attack rose (+2)!" },
            { "Every plan has a flaw!", "Libra exposed Megadeath's weak spots!" },
            { "He's too strong... I can't...", "Froze at the climax! Defense fell (-1)!" },
        } },
        { "GALACTICMON", "I am the end of all! Burn in the void!", {
            { "For all of our worlds!", "Power of bonds! Attack rose (+2)!" },
            { "I won't fall till I win!", "Unbreakable shield! Defense rose (+2)!" },
            { "We'll outrun the light!", "Full throttle! Speed rose (+2)!" },
        } },
    },
    { /* PT-BR */
        { "TAMER GENJI", "Ho ho! Mostra se tem fibra de Tamer!", {
            { "Pode mandar ver, coroa!", "Ânimo lá em cima! Ataque subiu (+1)!" },
            { "Vou ler o seu jogo primeiro!", "Olho clínico! Libra revelou fraquezas!" },
            { "Calma, nem li o manual ainda...", "Vacilou na entrada! Defesa caiu (-1)!" },
        } },
        { "KEITH", "Venceu Seiryu na sorte e já tá se achando?!", {
            { "Cai dentro e descobre!", "Sangue ferveu! Ataque subiu (+2)!" },
            { "Vou achar tua brecha!", "Libra ativado! Fraquezas reveladas!" },
            { "Fecha a guarda e espera!", "Postura firme! Defesa subiu (+2)!" },
        } },
        { "MASTERTYRANNO", "Bate bem, mas ainda não digeri o almoço!", {
            { "Aguenta essa então!", "Quebrou o ritmo dele! Defesa caiu (-1)!" },
            { "Mantém o compasso, sem pressa!", "Ritmo certeiro! Velocidade subiu (+1)!" },
            { "Ih, esse dinossauro é grande demais...", "Amarelou na hora H! Ataque caiu (-1)!" },
        } },
        { "LÍDER DE SEIRYU", "Belo combate! Aguenta o vento de Seiryu?", {
            { "Não arredo o pé daqui!", "Firme como rocha! Defesa subiu (+2)!" },
            { "Já saquei o teu ritmo!", "Leitura afiada! Velocidade subiu (+1)!" },
            { "Esse ventinho não assusta ninguém!", "Provocou demais! Ataque dele subiu (+1)!" },
        } },
        { "LÍDER DE SUZAKU", "Não me subestime! Minhas chamas não perdoam!", {
            { "Então aumenta essas chamas!", "Fogo no ringue! Ataque subiu (+2)!" },
            { "Hora de ler seu jogo!", "Libra revelou as fraquezas da Líder!" },
            { "Calma, pega leve aí...", "Bobeou na arena! Defesa caiu (-1)!" },
        } },
        { "LÍDER DE BYAKKO", "Mostre sua força... mas não espere piedade!", {
            { "Não vim pra brincar de lutar!", "Sangue frio! Ataque subiu (+2)!" },
            { "Tem algo estranho nesse estilo!", "Libra revelou as fraquezas do Líder!" },
            { "Esse sorrisinho tá suspeito demais...", "Hesitou na arena! Defesa caiu (-1)!" },
        } },
        { "GAME MASTER", "Teimosia adorável... Pena que acaba aqui.", {
            { "Pelo Teddy e por Asuka!", "Vontade inabalável! Ataque subiu (+2)!" },
            { "Segura o contra-ataque!", "Muralha levantada! Defesa subiu (+2)!" },
            { "Cala a boca! Eu acabo com você!", "Perdeu a cabeça! Ela acelerou (+1)!" },
        } },
        { "OFICIAL-CHEFE", "Ainda insiste?! O Destromon é invencível!", {
            { "Teu império acaba hoje!", "Golpe certeiro! Ataque subiu (+2)!" },
            { "Aguenta o tranco, equipe!", "Guarda fechada! Defesa subiu (+2)!" },
            { "Destromon vai virar sucata!", "Provocou demais! Ataque dele subiu (+1)!" },
        } },
        { "LORD MEGADEATH", "A humanidade acabou! Curvem-se ao deus!", {
            { "Deus nenhum! Eu vou te derrubar!", "Chama acesa! Ataque subiu (+2)!" },
            { "Todo plano tem uma falha!", "Libra revelou as fraquezas de Megadeath!" },
            { "Ele é forte demais... não dá...", "Vacilou no clímax! Defesa caiu (-1)!" },
        } },
        { "GALACTICMON", "Eu sou o fim de tudo! Queime no vácuo!", {
            { "Por todos os nossos mundos!", "Força dos vínculos! Ataque subiu (+2)!" },
            { "Não caio antes de vencer!", "Escudo inabalável! Defesa subiu (+2)!" },
            { "Vamos superar a luz!", "Velocidade máxima! Ritmo subiu (+2)!" },
        } },
    },
    { /* French */
        { "DRESSEUR GENJI", "Ho ho ! Montre-moi ce que tu vaux !", {
            { "Vas-y, papy, envoie !", "Moral au top ! Attaque en hausse (+1) !" },
            { "Je lis d'abord ton jeu !", "Bien vu ! Libra révèle les points faibles !" },
            { "Attends, j'ai pas lu le manuel...", "Raté au départ ! Défense en baisse (-1) !" },
        } },
        { "KEITH", "Battu Seiryu par chance, et tu frimes déjà ?!", {
            { "Viens vérifier !", "Sang bouillant ! Attaque en hausse (+2) !" },
            { "Je vais trouver ta faille !", "Libra activé ! Points faibles révélés !" },
            { "Garde haute, j'attends !", "Posture solide ! Défense en hausse (+2) !" },
        } },
        { "MASTERTYRANNO", "Pas mal ! Mais je digère encore mon repas !", {
            { "Alors encaisse ça !", "Rythme brisé ! Défense ennemie (-1) !" },
            { "Garde le tempo, sans hâte !", "Tempo parfait ! Vitesse en hausse (+1) !" },
            { "Euh, ce dino est bien trop gros...", "Quelle frousse ! Attaque en baisse (-1) !" },
        } },
        { "CHEF DE SEIRYU", "Beau combat ! Tiendras-tu face à Seiryu ?", {
            { "Je ne bouge pas d'ici !", "Tel un roc ! Défense en hausse (+2) !" },
            { "J'ai compris ton rythme !", "Bien lu ! Vitesse en hausse (+1) !" },
            { "Ta petite brise ne fait peur à personne !", "Trop de provoc ! Attaque ennemie (+1) !" },
        } },
        { "CHEF SUZAKU", "Ne me sous-estime pas ! Mes flammes brûlent !", {
            { "Alors monte le feu !", "Arène en feu ! Attaque en hausse (+2) !" },
            { "Voyons ton jeu !", "Libra révèle ses points faibles !" },
            { "Doucement, vas-y mollo...", "Bévue dans l'arène ! Défense en baisse (-1) !" },
        } },
        { "CHEF DE BYAKKO", "Montre ta force... sans espérer de pitié !", {
            { "Je ne suis pas là pour jouer !", "Sang-froid ! Attaque en hausse (+2) !" },
            { "Ce style cache quelque chose !", "Libra révèle ses points faibles !" },
            { "Ce petit sourire est trop louche...", "Hésitation ! Défense en baisse (-1) !" },
        } },
        { "MAITRE DE JEU", "Adorable entêtement... Dommage, c'est fini.", {
            { "Pour Teddy et pour Asuka !", "Volonté de fer ! Attaque en hausse (+2) !" },
            { "Encaisse la riposte !", "Muraille dressée ! Défense en hausse (+2) !" },
            { "Tais-toi ! Je vais t'achever !", "Sang-froid perdu ! Vitesse ennemie (+1) !" },
        } },
        { "COMMANDANT", "Tu insistes ?! Destromon est invincible !", {
            { "Ton empire tombe aujourd'hui !", "Coup précis ! Attaque en hausse (+2) !" },
            { "Tenez bon, l'équipe !", "Garde fermée ! Défense en hausse (+2) !" },
            { "Destromon finira à la casse !", "Trop de provoc ! Attaque ennemie (+1) !" },
        } },
        { "LORD MEGADEATH", "L'humanité est finie ! À genoux, mortels !", {
            { "Dieu ou pas, tu vas tomber !", "Flamme ardente ! Attaque en hausse (+2) !" },
            { "Tout plan a une faille !", "Libra révèle les failles de Megadeath !" },
            { "Il est trop fort... je peux pas...", "Flanché au pire moment ! Défense (-1) !" },
        } },
        { "GALACTICMON", "Je suis la fin de tout ! Brûlez dans le vide !", {
            { "Pour tous nos mondes !", "Force des liens ! Attaque en hausse (+2) !" },
            { "Je tiendrai jusqu'à la victoire !", "Bouclier inébranlable ! Défense (+2) !" },
            { "Plus vite que la lumière !", "Plein régime ! Vitesse en hausse (+2) !" },
        } },
    },
    { /* German */
        { "TAMER GENJI", "Ho ho! Zeig mal, was in dir steckt!", {
            { "Nur her damit, Opa!", "Feuer und Flamme! Angriff steigt (+1)!" },
            { "Erst lese ich deine Züge!", "Scharfer Blick! Libra zeigt Schwächen!" },
            { "Moment, die Anleitung kenn ich nicht...", "Fehlstart! Abwehr sinkt (-1)!" },
        } },
        { "KEITH", "Seiryu mit Glück besiegt und schon so frech?!", {
            { "Find's doch raus!", "Das Blut kocht! Angriff steigt (+2)!" },
            { "Ich finde deine Lücke!", "Libra an! Schwächen enthüllt!" },
            { "Deckung hoch und abwarten!", "Feste Haltung! Abwehr steigt (+2)!" },
        } },
        { "MASTERTYRANNO", "Gute Treffer! Aber ich verdaue noch!", {
            { "Dann nimm das!", "Rhythmus gebrochen! Feind-Abwehr sinkt (-1)!" },
            { "Im Takt bleiben, keine Eile!", "Perfekter Takt! Tempo steigt (+1)!" },
            { "Äh, der Dino ist viel zu groß...", "Kalte Füße! Angriff sinkt (-1)!" },
        } },
        { "SEIRYU LEADER", "Guter Kampf! Hältst du Seiryus Wind stand?", {
            { "Ich weiche keinen Schritt!", "Fest wie ein Fels! Abwehr steigt (+2)!" },
            { "Ich kenne deinen Rhythmus!", "Gut gelesen! Tempo steigt (+1)!" },
            { "Das Lüftchen macht keinem Angst!", "Zu viel Spott! Feind-Angriff steigt (+1)!" },
        } },
        { "SUZAKU LEADER", "Unterschätz mich nicht! Meine Flammen brennen!", {
            { "Dann dreh die Hitze auf!", "Arena in Flammen! Angriff steigt (+2)!" },
            { "Zeit, dein Spiel zu lesen!", "Libra zeigt ihre Schwächen!" },
            { "Ruhig, nur keine Hektik...", "Patzer in der Arena! Abwehr sinkt (-1)!" },
        } },
        { "BYAKKO LEADER", "Zeig deine Stärke... aber erwarte keine Gnade!", {
            { "Ich bin nicht zum Spielen hier!", "Eiskalt! Angriff steigt (+2)!" },
            { "Der Stil ist irgendwie faul!", "Libra zeigt seine Schwächen!" },
            { "Dieses Grinsen ist zu verdächtig...", "Gezögert! Abwehr sinkt (-1)!" },
        } },
        { "GAMEMASTER", "Süßer Starrsinn... Schade, hier ist Schluss.", {
            { "Für Teddy und für Asuka!", "Eiserner Wille! Angriff steigt (+2)!" },
            { "Konter abfangen!", "Die Mauer steht! Abwehr steigt (+2)!" },
            { "Klappe! Ich mach dich fertig!", "Kopf verloren! Feind-Tempo steigt (+1)!" },
        } },
        { "CHIEF OFFICER", "Immer noch?! Destromon ist unbesiegbar!", {
            { "Dein Imperium fällt heute!", "Volltreffer! Angriff steigt (+2)!" },
            { "Haltet durch, Team!", "Deckung dicht! Abwehr steigt (+2)!" },
            { "Destromon wird zu Schrott!", "Zu viel Spott! Feind-Angriff steigt (+1)!" },
        } },
        { "LORD MEGADEATH", "Die Menschheit ist am Ende! Kniet nieder!", {
            { "Du bist kein Gott! Du fällst!", "Feuer entfacht! Angriff steigt (+2)!" },
            { "Jeder Plan hat eine Lücke!", "Libra zeigt Megadeaths Schwächen!" },
            { "Er ist zu stark... ich kann nicht...", "Im Finale gezögert! Abwehr sinkt (-1)!" },
        } },
        { "GALACTICMON", "Ich bin das Ende von allem! Brennt im Nichts!", {
            { "Für all unsere Welten!", "Kraft der Bindung! Angriff steigt (+2)!" },
            { "Ich falle nicht vor dem Sieg!", "Eiserner Schild! Abwehr steigt (+2)!" },
            { "Schneller als das Licht!", "Vollgas! Tempo steigt (+2)!" },
        } },
    },
    { /* Italian */
        { "DOMATORE GENJI", "Oh oh! Fammi vedere che stoffa hai!", {
            { "Fatti sotto, nonnetto!", "Morale alle stelle! Attacco su (+1)!" },
            { "Prima studio le tue mosse!", "Occhio clinico! Libra svela i punti deboli!" },
            { "Aspetta, non ho letto il manuale...", "Falsa partenza! Difesa giù (-1)!" },
        } },
        { "KEITH", "Battuto Seiryu per fortuna e già te la tiri?!", {
            { "Vieni a scoprirlo!", "Sangue che bolle! Attacco su (+2)!" },
            { "Troverò il tuo punto debole!", "Libra attivo! Punti deboli svelati!" },
            { "Guardia alta e aspetto!", "Postura salda! Difesa su (+2)!" },
        } },
        { "MASTERTYRANNO", "Bei colpi! Ma sto ancora digerendo il pranzo!", {
            { "Allora prendi questo!", "Ritmo spezzato! Difesa nemica giù (-1)!" },
            { "Tieni il ritmo, senza fretta!", "Ritmo perfetto! Velocità su (+1)!" },
            { "Ehm, questo dinosauro è troppo grosso...", "Che fifa! Attacco giù (-1)!" },
        } },
        { "GOV. DI SEIRYU", "Bella lotta! Reggi il vento di Seiryu?", {
            { "Non mi muovo di qui!", "Come una roccia! Difesa su (+2)!" },
            { "Ho capito il tuo ritmo!", "Lettura perfetta! Velocità su (+1)!" },
            { "Questo venticello non spaventa nessuno!", "Troppe provocazioni! Attacco nemico su (+1)!" },
        } },
        { "GOV. SUZAKU", "Non sottovalutarmi! Le mie fiamme bruciano!", {
            { "Allora alza la fiamma!", "Arena in fiamme! Attacco su (+2)!" },
            { "Leggiamo il tuo gioco!", "Libra svela i suoi punti deboli!" },
            { "Calma, vacci piano...", "Che svista! Difesa giù (-1)!" },
        } },
        { "GOVERNATORE DI BYAKKO", "Mostra la tua forza... ma non aspettarti pietà!", {
            { "Non sono qui per giocare!", "Sangue freddo! Attacco su (+2)!" },
            { "Quello stile nasconde qualcosa!", "Libra svela i suoi punti deboli!" },
            { "Quel sorrisetto è troppo sospetto...", "Esitazione! Difesa giù (-1)!" },
        } },
        { "SIGNORA DEL GIOCO", "Adorabile testardaggine... Peccato, è finita.", {
            { "Per Teddy e per Asuka!", "Volontà d'acciaio! Attacco su (+2)!" },
            { "Para il contrattacco!", "Muro alzato! Difesa su (+2)!" },
            { "Sta' zitta! Ti faccio fuori!", "Hai perso la testa! Velocità nemica su (+1)!" },
        } },
        { "UFFICIALE CAPO", "Insisti ancora?! Destromon è invincibile!", {
            { "Il tuo impero cade oggi!", "Colpo preciso! Attacco su (+2)!" },
            { "Tenete duro, squadra!", "Guardia chiusa! Difesa su (+2)!" },
            { "Destromon diventerà un rottame!", "Troppe provocazioni! Attacco nemico su (+1)!" },
        } },
        { "LORD MEGAMORTE", "L'umanità è finita! Inchinatevi al vostro dio!", {
            { "Non sei un dio! Ora cadi!", "Fiamma accesa! Attacco su (+2)!" },
            { "Ogni piano ha una falla!", "Libra svela i punti deboli di Megamorte!" },
            { "È troppo forte... non ce la faccio...", "Crollo sul più bello! Difesa giù (-1)!" },
        } },
        { "GALACTICMON", "Io sono la fine di tutto! Bruciate nel vuoto!", {
            { "Per tutti i nostri mondi!", "Forza dei legami! Attacco su (+2)!" },
            { "Non cado prima di vincere!", "Scudo incrollabile! Difesa su (+2)!" },
            { "Superiamo la luce!", "A tutto gas! Velocità su (+2)!" },
        } },
    },
    { /* Spanish */
        { "DOMADOR GENJI", "¡Jo, jo! ¡Demuestra que tienes madera!", {
            { "¡Venga, abuelo, dale!", "¡Ánimo a tope! ¡Sube el Ataque (+1)!" },
            { "¡Primero leeré tu juego!", "¡Ojo clínico! ¡Libra revela debilidades!" },
            { "Espera, no me he leído el manual...", "¡Mala salida! ¡Baja la Defensa (-1)!" },
        } },
        { "KEITH", "¡¿Ganaste a Seiryu de chiripa y ya presumes?!", {
            { "¡Ven y descúbrelo!", "¡Sangre hirviendo! ¡Sube el Ataque (+2)!" },
            { "¡Encontraré tu punto débil!", "¡Libra activado! ¡Debilidades al aire!" },
            { "¡Guardia alta y a esperar!", "¡Postura firme! ¡Sube la Defensa (+2)!" },
        } },
        { "MAESTRO TYRANNO", "¡Buenos golpes! ¡Pero aún hago la digestión!", {
            { "¡Pues toma esto!", "¡Ritmo roto! ¡Baja su Defensa (-1)!" },
            { "Mantén el ritmo, sin prisa.", "¡Ritmo perfecto! ¡Sube la Velocidad (+1)!" },
            { "Uy, este dinosaurio es enorme...", "¡Qué canguelo! ¡Baja el Ataque (-1)!" },
        } },
        { "LÍDER SEIRYU", "¡Buen combate! ¿Resistes el viento de Seiryu?", {
            { "¡No me muevo de aquí!", "¡Firme como una roca! ¡Sube la Defensa (+2)!" },
            { "¡Ya pillé tu ritmo!", "¡Buena lectura! ¡Sube la Velocidad (+1)!" },
            { "¡Esta brisita no asusta a nadie!", "¡Demasiada provocación! ¡Sube su Ataque (+1)!" },
        } },
        { "LÍDER SUZAKU", "¡No me subestimes! ¡Mis llamas no perdonan!", {
            { "¡Pues sube el fuego!", "¡Arena en llamas! ¡Sube el Ataque (+2)!" },
            { "¡Hora de leer tu juego!", "¡Libra revela sus debilidades!" },
            { "Tranqui, sin pasarse...", "¡Despiste en la arena! ¡Baja la Defensa (-1)!" },
        } },
        { "LÍDER BYAKKO", "Muestra tu fuerza... ¡pero no esperes piedad!", {
            { "¡No he venido a jugar!", "¡Sangre fría! ¡Sube el Ataque (+2)!" },
            { "¡Ese estilo tiene algo raro!", "¡Libra revela sus debilidades!" },
            { "Esa sonrisita es muy sospechosa...", "¡Dudaste! ¡Baja la Defensa (-1)!" },
        } },
        { "MAESTRO DE JUEGO", "Adorable terquedad... Lástima que acabe aquí.", {
            { "¡Por Teddy y por Asuka!", "¡Voluntad de hierro! ¡Sube el Ataque (+2)!" },
            { "¡Aguanta el contraataque!", "¡Muralla en pie! ¡Sube la Defensa (+2)!" },
            { "¡Cállate! ¡Voy a acabar contigo!", "¡Perdiste la cabeza! ¡Sube su Velocidad (+1)!" },
        } },
        { "OFICIAL JEFE", "¡¿Aún insistes?! ¡Destromon es invencible!", {
            { "¡Tu imperio cae hoy!", "¡Golpe certero! ¡Sube el Ataque (+2)!" },
            { "¡Aguantad, equipo!", "¡Guardia cerrada! ¡Sube la Defensa (+2)!" },
            { "¡Destromon acabará en el desguace!", "¡Demasiada provocación! ¡Sube su Ataque (+1)!" },
        } },
        { "LORD MEGADEATH", "¡Se acabó la humanidad! ¡Postraos ante mí!", {
            { "¡No eres un dios! ¡Vas a caer!", "¡Llama encendida! ¡Sube el Ataque (+2)!" },
            { "¡Todo plan tiene un fallo!", "¡Libra revela los fallos de Megadeath!" },
            { "Es demasiado fuerte... no puedo...", "¡Flaqueaste al final! ¡Baja la Defensa (-1)!" },
        } },
        { "GALACTICMON", "¡Soy el fin de todo! ¡Arded en el vacío!", {
            { "¡Por todos nuestros mundos!", "¡Fuerza de los lazos! ¡Sube el Ataque (+2)!" },
            { "¡No caeré antes de vencer!", "¡Escudo inquebrantable! ¡Sube la Defensa (+2)!" },
            { "¡Superemos a la luz!", "¡A toda máquina! ¡Sube la Velocidad (+2)!" },
        } },
    },
};

/* Above the result, on a line of its own, when a bond or the reputation raised the buff a tier. */
static const char *const bp_bond[6] = {
    "Strong bond!", "Vínculo forte!", "Lien fort !",
    "Starkes Band!", "Legame forte!", "¡Vínculo fuerte!",
};
