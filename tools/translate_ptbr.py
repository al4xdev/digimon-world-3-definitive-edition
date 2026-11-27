#!/usr/bin/env python3
"""Translate Digimon World 3 (PS1) English text + Definitive Edition overrides into PT-BR using local Gemma 4.

Usage:
  tools/venv/bin/python tools/translate_ptbr.py --benchmark          # Test prompt/reasoning on 20 strings
  tools/venv/bin/python tools/translate_ptbr.py --tier p0            # Translate en.txt + all UI/Menu/Item/Skill/Card files
  tools/venv/bin/python tools/translate_ptbr.py --tier all           # Translate everything (P0 + DMG* + TALK*)
  tools/venv/bin/python tools/translate_ptbr.py --export             # Generate tools/text_override/pt.txt from cache

NOTE: Do not pass sampling parameters (temperature, top_p, top_k) to the local server; the server manages them.
"""
import argparse
import json
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import disc_files as df  # noqa: E402
import dump_text as dt  # noqa: E402

CACHE_DIR = ROOT / ".dumps/tasks/translation"
CACHE_FILE = CACHE_DIR / "cache_ptbr.jsonl"
PT_OUT = ROOT / "tools/text_override/pt.txt"
EN_OVR = ROOT / "tools/text_override/en.txt"

# Characters normalized before encoding into the European PS1 font
CHAR_ALIASES = {
    "-": "−",
    "–": "−",
    "—": "―",
    "'": "’",
    "‘": "’",
    "`": "’",
    '"': "”",
    "“": "”",
    "«": "”",
    "»": "”",
    "~": "〜",
    "…": "...",
    "ª": "a",
    "°": "º",
    "\u00a0": " ",
}

# $NAME keys in tools/text_override/en.txt that must remain verbatim in PT-BR
INVARIANT_DOLLAR_KEYS = {
    "$Weak", "$Resist", "$BatonPass", "$Neutral", "$Unknown", "$Slash",
    "$TrainPityMarks", "$TrainPityDigits", "$HuntHeld", "$HuntSpace",
    "$HuntRomanI", "$HuntRomanV", "$HuntRomanX", "$RecipeSep",
    "$DriSep", "$DriDash", "$GuideSep", "$RushPlus", "$RushExp",
    "$RushTimes", "$Guardromon", "$HuntGotSuffix", "$BMBits", "$RushBits",
    "$GuideBit", "$EvoLv",
}

# Exact glossary translations for critical short HUD/UI labels ($NAME)
FIXED_DOLLAR_PT = {
    "$Fire": "Fogo",
    "$Water": "Água",
    "$Ice": "Gelo",
    "$Wind": "Vento",
    "$Thunder": "Trovão",
    "$Machine": "Máquina",
    "$Dark": "Trevas",
    "$Twice": "Inimigo x2",
    "$MsgLibra": "Fraquezas reveladas!",
    "$MsgRelay": "Próxima troca grátis!",
    "$MsgBreak": "Resistência reduzida!",
    "$Bestiary": "Bestiário",
    "$BestiaryMenu": "BESTIÁRIO",
    "$BestiaryHint": "L1/R1: Página",
    "$LevelTpFull": "<5,1><1>Nível <5,2>! TP cheio: treine!<2,2>",
    "$TrainPityPrefix": "Garantia:",
    "$TrainPityReady": "Próx. 100%!",
    "$EvoOpen": "□ Próx. Formas",
    "$EvoTitle": "Próximas Formas",
    "$EvoKeys": "↑↓ Forma   L1/R1 Parceiro   △ Voltar",
    "$EvoLevel": "Nível",
    "$EvoReady": "Pronto",
    "$EvoReadyLine": "Aprende ao subir de nível",
    "$EvoMissing": "faltam",
    "$EvoNone": "Nenhuma ao alcance ainda.",
    "$EvoNoneHint": "Suba suas formas e atributos.",
    "$EvoStr": "Força",
    "$EvoDef": "Defesa",
    "$EvoSpi": "Espírito",
    "$EvoWis": "Sabedoria",
    "$EvoSpd": "Velocidade",
    "$EvoCha": "Carisma",
    "$Save": "SALVAR",
    "$Teleport": "TELEPORTE",
    "$TravelKeys": "Botão ×: Escolher<1>Botão △: Voltar",
    "$TravelKeys2": "Botão ×: Viajar<1>Botão △: Voltar",
    "$HuntGotPrefix": "Material de caça:<1>",
    "$HuntDrops": "Caça",
    "$HuntRarePrefix": "Material raro:<1>",
    "$HuntRareWord": "Núcleo",
    "$HuntRareLabel": "Núcleos Raros",
    "$RecipeMissing": "Você não tem materiais suficientes.",
    "$BMTitle": "MERCADO NEGRO",
    "$BMTabTrade": "Trocar",
    "$BMTabReplica": "Réplicas",
    "$BMHeadCores": "Seus Núcleos Raros",
    "$BMHeadMats": "Receber 12 de qual material?",
    "$BMHeadReplicas": "Réplicas Lendárias",
    "$BMGive": "Dar: ",
    "$BMGet": "Receber: ",
    "$BMHeld": "   Tem: ",
    "$BMTakes": "Gasta: ",
    "$BMBits": " Bits",
    "$BMCoreTrade": "Troque um por 12 de um Material de Caça.",
    "$BMCoreForge": "As lojas fazem equipamentos com materiais.",
    "$BMCoreKeep": "Réplicas também gastam Núcleos: 2 chefes cada.",
    "$BMNone": "Você não tem Núcleos Raros para trocar.",
    "$BMNoneWhere": "Inimigos fortes os deixam: veja o Bestiário.",
    "$BMKindWeapon": "Arma",
    "$BMKindTwo": "Duas mãos",
    "$BMKindArmor": "Armadura",
    "$BMKindAcc": "Acessório",
    "$BMStat1": "Força",
    "$BMStat2": "Defesa",
    "$BMStat3": "Espírito",
    "$BMStat4": "Sabedoria",
    "$BMStat5": "Velocidade",
    "$BMStat6": "Carisma",
    "$BMPlus": "+",
    "$BMMinus": "-",
    "$BMWears": "Quem usa: ",
    "$BMWearsAll": "Qualquer Digimon seu pode usar.",
    "$BMWearsNone": "Nenhum Digimon seu pode usar.",
    "$BMTraded": "Trocado.",
    "$BMBought": "Comprado. Réplica revende por 0 Bits.",
    "$BMArmed": "Réplica revende por 0 Bits. Comprar?",
    "$BMNoRoom": "Sem espaço: um material vai até 99.",
    "$BMNeedA": "Exige Núcleos de ",
    "$BMNeedB": " chefes diferentes.",
    "$BMNoBits": "Bits insuficientes.",
    "$BMBagFull": "Você não pode carregar mais disso.",
    "$BMKeysTrade": "↑↓ Escolher   L1/R1 Aba<1>Botão ×: Trocar   Botão △: Voltar",
    "$BMKeysPick": "↑↓ Material<1>Botão ×: Trocar um   Botão △: Voltar",
    "$BMKeysReplica": "↑↓ Escolher   L1/R1 Aba<1>Botão ×: Comprar   Botão △: Voltar",
    "$BMKeysArmed": "Botão ×: Comprar<1>Botão △: Voltar",
    "$RushLvUp": "  SUBIU NV!",
    "$RushNewTech": "NOVA TECH!",
    "$RushTpFull": "TP CHEIO!",
    "$DriTitle": "Simulações DRI",
    "$DriOf": " de ",
    "$DriReward": "Prêmio ",
    "$DriUnlock": "Liberar ",
    "$DriBest": "Melhor ",
    "$DriClear": "Venceu todas as fases",
    "$DriKeysUnlock": "Botão ×: Liberar<1>Botão △: Voltar",
    "$DriKeysStart": "Botão ×: Iniciar<1>Botão △: Voltar",
    "$DriKeysConfirm": "Botão ×: Confirmar<1>Botão △: Voltar",
    "$DriKeysFight": "Botão ×: Lutar<1>Botão △: Voltar",
    "$DriStages": "Fases vencidas ",
    "$DriNewReward": "Novo prêmio",
    "$DriNoReward": "Sem novo prêmio",
    "$DriOk": "Botão ×: OK",
    "$GuideMenu": "GUIA",
    "$GuideTitle": "Guia",
    "$GuideTabQuest": "Missão",
    "$GuideTabAuctions": "Leilões",
    "$GuideMain": "Missão Principal",
    "$GuideGoTo": "Ir para: ",
    "$GuideKeysQuest": "L1/R1: Leilões<1>Botão △: Voltar",
    "$GuideKeysAuctions": "L1/R1: Missão<1>Botão △: Voltar",
    "$GuideKeysTravel": "Botão ×: Viajar a El Dorado<1>Botão △: Voltar",
    "$GuideKeysTravel2": "Botão ×: Confirmar viagem<1>Botão △: Voltar",
    "$AucOpen": "ABERTO AGORA",
    "$AucDone": "Concluído",
    "$AucMissed": "Perdido",
    "$AucSoon": "Ainda fechado",
    "$AucOpenHint": "Na Cargo Tower: desça a escada de<1>El Dorado e siga pelo Water Tunnel.",
    "$AucMissedHint": "Uma loja secreta em Amaterasu City<1>vende os itens de leilões perdidos.",
    "$AucDoneHint": "Este leilão já terminou.",
    "$NoticeQuest": "NOVO OBJETIVO",
    "$NoticeAuction": "LEILÃO ABERTO",
    "$Q20T": "Os portões de Asuka City fecharam. Do<1>cais da Asuka Bridge, Submarimon leva<1>ao Asuka Sewers: vença Datamon na<1>Control Room. Ele pede um Staff Pass:<1>veja Kurt e ache Keith e Nick no<1>Suzaku UG Lake (cais de Suzaku City).",
    "$Q22T": "Entre no Admin Center pelo portão do<1>Asuka Sewers e suba à Master Room,<1>no segundo andar. Os quadros formam<1>MONSTER. Vença o Game Master.",
    "$Q24T": "Byakko City também está livre. Fale com<1>o Líder no Byakko Dome e depois com o<1>general da A.o.A. na Underground Cave.",
    "$M0T": "Ponchomon, em El Dorado (o telhado<1>azul de Asuka City), entrega ao falar<1>pela segunda vez com 210 Carisma. Isso<1>abre os leilões da Cargo Tower.",
    "$M1T": "Divermon, no Yellow Cruiser (o telhado<1>amarelo do nível inferior de Asuka<1>City), dá a bolsa para Card Battles.",
}

# Standardized PT-BR overrides for SKLINF (<= 34 chars) and specific UI/dialogue lines
FIXED_CACHE_OVERRIDES = {
    "SKLINF::User Atk+2 (phys/mag), 4 turns.": "Atq+2 usuário (fís/mág), 4 turnos.",
    "SKLINF::Allies Atk+3 (phys/mag), 3 turns.": "Atq+3 aliados (fís/mág), 3 turnos.",
    "SKLINF::User Def+2 vs all dmg, 4 turns.": "Def+2 usuário vs tudo, 4 turnos.",
    "SKLINF::Allies Def+3 vs all dmg, 3 turns.": "Def+3 aliados vs tudo, 3 turnos.",
    "SKLINF::Enemy Atk-2 (phys/mag), 3 turns.": "Atq−2 inimigo (fís/mág), 3 turnos.",
    "SKLINF::Enemy Atk-3 (phys/mag), 3 turns.": "Atq−3 inimigo (fís/mág), 3 turnos.",
    "SKLINF::Enemy Def-2 vs all dmg, 3 turns.": "Def−2 inimigo vs tudo, 3 turnos.",
    "SKLINF::Enemy Def-3 vs all dmg, 3 turns.": "Def−3 inimigo vs tudo, 3 turnos.",
    "SKLINF::Foe best Elem Resist-1, 3 turns.": "Res. Elem.−1 no alvo, 3 turnos.",
    "SKLINF::Foe 2 Elem Resists-2, 3 turns.": "2 Res. Elem.−2 no alvo, 3 turnos.",
    "SKLINF::Dark magic. Foe Atk-2, 3 turns.": "Magia Trevas. Atq−2, 3 turnos.",
    "SKLINF::Ice phys. Foe Atk-1, 2 turns.": "Físico de Gelo. Atq−1, 2 turnos.",
    "SKLINF::Physical. Foe Def-1, 2 turns.": "Ataque Físico. Def−1, 2 turnos.",
    "SKLINF::Physical. Foe Atk-1, 2 turns.": "Ataque Físico. Atq−1, 2 turnos.",
    "SKLINF::Mach phys. Foe Atk-2, 3 turns.": "Físico Máquina. Atq−2, 3 turnos.",
    "SKLINF::Mach phys. Foe Def-2, 3 turns.": "Físico Máquina. Def−2, 3 turnos.",
    "SKLINF::Next attack deals extra dmg.": "Próximo ataque causa dano extra.",
    "SKLINF::Next attack deals max extra dmg.": "Próx. ataque dá dano extra máx.",
    "SKLINF::Relay: the next swap is free.": "Relay: a próxima troca é grátis.",
    "SKLINF::Reveals the enemy's weaknesses.": "Revela as fraquezas do inimigo.",
    "SKLINF::Strengthen fire, weaken ice.": "Aumenta Fogo, reduz Gelo.",
    "SKLINF::Strengthen water, weaken fire.": "Aumenta Água, reduz Fogo.",
    "SKLINF::Strengthen ice, weaken wind.": "Aumenta Gelo, reduz Vento.",
    "SKLINF::Strengthen wind, weaken water.": "Aumenta Vento, reduz Água.",
    "SKLINF::Strengthen thunder, weaken metal.": "Aumenta Trovão, reduz Máquina.",
    "SKLINF::Strengthen metal, weaken dark.": "Aumenta Máquina, reduz Trevas.",
    "SKLINF::Strengthen dark, weaken thunder.": "Aumenta Trevas, reduz Trovão.",
    "UI::ITMINF::Light, fast. Boost for less Power.": "Leve e rápido. +Velocidade, −Força.",
    "UI::ITMINF::Used by wizard to protect the head.": "Usado por mago p/ proteger a cabeça.",
    "UI::TRAINI::Pick TP amount to use.": "Escolha o TP a usar.",
    "UI::TRAINI::Results:<1>Fire Affinity is up by <5,1>!<2,2>": "Resultado:<1>Afinidade de Fogo +<5,1>!<2,2>",
    "UI::TRAINI::Results:<1>Water Affinity is up by <5,1>!<2,2>": "Resultado:<1>Afinidade de Água +<5,1>!<2,2>",
    "UI::TRAINI::Results:<1>Ice Affinity is up by <5,1>!<2,2>": "Resultado:<1>Afinidade de Gelo +<5,1>!<2,2>",
    "UI::TRAINI::Results:<1>Wind Affinity is up by <5,1>!<2,2>": "Resultado:<1>Afinidade de Vento +<5,1>!<2,2>",
    "UI::TRAINI::Results:<1>Thunder Affinity is up by <5,1>!<2,2>": "Resultado:<1>Afinidade de Trovão +<5,1>!<2,2>",
    "UI::TRAINI::Results:<1>Machine Affinity is up by <5,1>!<2,2>": "Resultado:<1>Afinidade de Máquina +<5,1>!<2,2>",
    "UI::TRAINI::Results:<1>Dark Affinity is up by <5,1>!<2,2>": "Resultado:<1>Afinidade de Trevas +<5,1>!<2,2>",
    "UI::TRAINI::Results:<1><3>Fire Affinity is up by <5,1>!<1>Maximum MP is up by <5,2>!<2,2>": "Resultado:<1><3>Afinidade de Fogo +<5,1>!<1>MP máximo +<5,2>!<2,2>",
    "UI::TRAINI::Results:<1><3>Machine Affinity is up by <5,1>!<1>Max HP is up by <5,2>!<2,2>": "Resultado:<1><3>Afinidade de Máquina +<5,1>!<1>HP máximo +<5,2>!<2,2>",
    "UI::DGLABO::Cannot load technique yet.": "Não carrega ainda.",
    "UI::DGLABO::Cannot load technique.": "Não carrega técnica.",
    "UI::DGLABO::Cannot digivolve.": "Não digivolve.",
    "UI::DGLABO::Can’t change digivolve.": "Não muda Digivolve.",
    "UI::DGLABO::Can’t load technique.": "Não carrega técnica.",
    "UI::MFIGHT::Strength, defense, speed cursed!": "Força, defesa e veloc. reduzidas!",
    # the Black Market sells 356 and 359 (Legendary Replicas): the cache had kept them, and 357-358, in English
    "SHORT::ITMNAM::Metal Power S": "Poder Máquina S",
    "SHORT::ITMNAM::Dark Power 1": "Poder de Trevas 1",
    "SHORT::ITMNAM::Dark Power 2": "Poder de Trevas 2",
    "SHORT::ITMNAM::Dark Power S": "Poder de Trevas S",
    "SHORT::HTLNAM::Each Digimon<1><5,1> BIT": "Cada Digimon<1><5,1> BIT",
    "SHORT::HTLNAM::Stay in Inn": "Dormir",
    "SHORT::HTLNAM::Do not stay in Inn": "Não dormir",
    "SHORT::HTLNAM::You don’t have enough BIT!": "Você não tem BIT suficiente!",
    "MAP::Protocol Ruins<1>Protocol Forest": "Ruínas Protocol<1>Floresta Protocol",
    "MAP::Bulk Swamp<1>Bulk Bridge": "Pântano Bulk<1>Ponte Bulk",
    "DLG::<7>Leomon<7>This is Card Island −<1>a utopia for<1>cardsters! It’s<2,2><3>against the rules<1>for people without<1>Folder Bags to<2,2><3>enter!<2,2>": "<7>Leomon<7>Esta é a Card Island −<1>uma utopia para<1>cardsters! É<2,2><3>contra as regras<1>entrar sem uma<1>Folder Bag!<2,2>",
    # the machine translation put 4 lines in the first box (a box holds 3) and left "você tem os" alone in the next
    "DLG::<7>Zhu Que Leader<7>What? You have the<1>nerve to challenge<1>me? So, you have<2,2><3>the 5 strongest Epic<1>Items. You aren’t<1>half bad! Very well.<2,2><3>Show me what you’ve<1>got. I’ll fight you<1>now!<2,2>": "<7>Zhu Que Leader<7>O quê? Você tem a<1>audácia de me<1>desafiar? Então...<2,2><3>você tem os 5 Epic<1>Items mais fortes.<1>Você não é tão ruim!<2,2><3>Muito bem. Mostre−me<1>o que você tem. Eu<1>lutarei com você agora!<2,2>",
    "DLG::<7>Mr. Smith<7>Going... Going...<1>GONE!<2,2><3>Dramon Shield!<1>Sold at 1600 BIT!<2,2><3>Congratulations<1>to you over there!<2,2><3>I’m transferring <1>the item to your<1>satellite now!<2,2>": "<7>Sr. Smith<7>Dou−lhe uma... duas...<1>VENDIDO!<2,2><3>Dramon Shield!<1>Vendido por 1600 BIT!<2,2><3>Parabéns para<1>você aí!<2,2><3>Estou transferindo<1>o item para o seu<1>satélite agora!<2,2>",
}

CTRL_RE = re.compile(r"<(\d+(?:,\d+)*)>")
NON_LINE_CTRL_RE = re.compile(r"<(?!1>)(\d+(?:,\d+)*)>")
SPEAKER_RE = re.compile(r"^<7>(.*?)<7>(.*)$", re.DOTALL)


def build_font_metrics():
    """Return (valid_chars_set, char_px_width_dict) from dump_text and src/main/font.c."""
    chars, ext = dt.char_tables()
    src = (ROOT / "src/main/font.c").read_text()

    def parse_adv(name):
        block = src[src.index(f"FontGlyph {name}["):]
        block = block[:block.index("};")]
        out = []
        for line in block.splitlines()[1:]:
            nums = [int(x, 0) for x in re.findall(r"-?0x[0-9A-Fa-f]+|-?\d+", line)]
            if len(nums) == 11:
                out.append(nums[7] + nums[9])
        return out

    g14 = parse_adv("font_glyphs_14")
    g14_ext = parse_adv("font_glyphs_14_ext")

    px = {}
    for g, code in sorted(chars.items()):
        ch = dt.sjis(code)
        if 0 <= g - 4 < len(g14):
            px.setdefault(ch, g14[g - 4])
    for g, code in sorted(ext.items()):
        ch = dt.sjis(code)
        if 0 <= g - 1 < len(g14_ext):
            px.setdefault(ch, g14_ext[g - 1])
    for g, ch in dt.LATIN.items():
        if 0 <= g - 1 < len(g14_ext):
            px[ch] = g14_ext[g - 1]
    return set(px.keys()), px


VALID_CHARS, CHAR_PX = build_font_metrics()


def normalize_chars(text: str) -> str:
    text = text.replace("\n    ", "").replace("\n", "<1>")
    text = re.sub(r"<br\s*/?>", "<1>", text, flags=re.IGNORECASE)
    out = []
    i = 0
    while i < len(text):
        m = CTRL_RE.match(text, i)
        if m:
            out.append(m.group(0))
            i = m.end()
            continue
        ch = text[i]
        if ch in CHAR_ALIASES:
            out.append(CHAR_ALIASES[ch])
        else:
            out.append(ch)
        i += 1
    return "".join(out)


def line_pixel_width(line: str) -> int:
    """Exact pixel width in Font 1 (14px), counting <5,x>/<8,x> variables as 42px."""
    w = 0
    i = 0
    while i < len(line):
        m = CTRL_RE.match(line, i)
        if m:
            code = int(m.group(1).split(",")[0])
            if code in (5, 8):
                w += 42
            i = m.end()
            continue
        w += CHAR_PX.get(line[i], 6)
        i += 1
    return w


def visible_char_len(s: str) -> int:
    s2 = re.sub(r"<(?:5|8),\d+>", "XXXXXX", s)
    s2 = CTRL_RE.sub("", s2)
    return len(s2)


def rewrap_dialogue_pages(text: str, orig_text: str, max_px_limit: int = 139) -> str:
    """Ensure every dialogue page between <2,2><3> or <2,2><1><1><1> fits within max_px_limit."""
    sp_m = SPEAKER_RE.match(text)
    prefix = ""
    body = text
    if sp_m:
        prefix = f"<7>{sp_m.group(1)}<7>"
        body = sp_m.group(2)

    orig_sp = SPEAKER_RE.match(orig_text)
    orig_body = orig_sp.group(2) if orig_sp else orig_text

    # Do not auto-rewrap if the dialogue has <5,x> or is a short choice list without <2,2>
    if "<2,2>" not in orig_body:
        return text

    orig_max_px = max((line_pixel_width(ln) for ln in re.split(r"<1>|<2,2>|<3>", orig_body)), default=139)
    target_px = max(max_px_limit, orig_max_px + 4)

    # Split into pages keeping separators (<2,2><3>, <2,2><1><1><1>, <2,2><1>, <2,2>)
    tokens = re.split(r"(<2,2>(?:<3>|(?:<1>)+)?)", body)
    new_tokens = []
    for idx, tok in enumerate(tokens):
        if idx % 2 == 1 or not tok:
            new_tokens.append(tok)
            continue
        lines = tok.split("<1>")
        if all(line_pixel_width(ln) <= target_px for ln in lines) and len(lines) <= 3:
            new_tokens.append(tok)
            continue
        # Re-wrap words in this page
        words = tok.replace("<1>", " ").split()
        if not words:
            new_tokens.append(tok)
            continue
        wrapped = []
        cur = words[0]
        for w in words[1:]:
            cand = cur + " " + w
            if line_pixel_width(cand) <= target_px:
                cur = cand
            else:
                wrapped.append(cur)
                cur = w
        wrapped.append(cur)
        # If it fits in <= 3 lines, join with <1>; if 4+ lines, split into 3-line pages with <2,2><3>
        if len(wrapped) <= 3:
            new_tokens.append("<1>".join(wrapped))
        else:
            pages = ["<1>".join(wrapped[i:i + 3]) for i in range(0, len(wrapped), 3)]
            new_tokens.append("<2,2><3>".join(pages))
    return prefix + "".join(new_tokens)


def validate_translation(en_text: str, pt_text: str, kind: str, max_len: int) -> tuple[bool, str, str]:
    """Validate and normalize pt_text against en_text. Returns (ok, fixed_pt, error_reason)."""
    pt = normalize_chars(pt_text.strip())
    en = normalize_chars(en_text.strip())

    # 1. Check unknown characters
    i = 0
    while i < len(pt):
        m = CTRL_RE.match(pt, i)
        if m:
            nums = [int(x) for x in m.group(1).split(",")]
            want = dt.CODE_LENGTHS[nums[0]] if nums[0] < len(dt.CODE_LENGTHS) and dt.CODE_LENGTHS[nums[0]] >= 2 else 2
            if len(nums) + 1 != want:
                return False, pt, f"Invalid control code {m.group(0)}"
            i = m.end()
            continue
        if pt[i] not in VALID_CHARS:
            return False, pt, f"Unsupported character {pt[i]!r}"
        i += 1

    # 2. Check non-<1> control codes (<2,2>, <3>, <5,1>, <7>, <8,1>, <9>)
    en_ctrls = NON_LINE_CTRL_RE.findall(en)
    pt_ctrls = NON_LINE_CTRL_RE.findall(pt)
    if en_ctrls != pt_ctrls:
        # Allow if the only difference is an extra <2,2><3> added by page-splitting, so check before rewrap first
        return False, pt, f"Control codes mismatch: expected {en_ctrls}, got {pt_ctrls}"

    # 3. Check speaker tag <7>...<7>
    if en.startswith("<7><9><7>") and not pt.startswith("<7><9><7>"):
        return False, pt, "Must start with <7><9><7>"
    sp_m = SPEAKER_RE.match(pt)
    if sp_m and len(sp_m.group(1)) > 29:
        return False, pt, f"Speaker name {sp_m.group(1)!r} too long (>29 chars)"

    # 4. Auto-rewrap dialogues if needed
    if kind == "dialogue":
        pt = rewrap_dialogue_pages(pt, en)

    # 5. Check length / line limits by kind
    if kind == "sklinf":
        pt = pt.replace("<1>", " ")
        vlen = visible_char_len(pt)
        if vlen > 34:
            return False, pt, f"SKLINF too long: {vlen} chars (max 34)"
    elif kind == "short_name":
        pt = pt.replace("<1>", " ")
        vlen = visible_char_len(pt)
        if vlen > max_len:
            return False, pt, f"Name too long: {vlen} chars (max {max_len})"
    elif kind == "guide":
        lines = pt.split("<1>")
        for ln in lines:
            vlen = visible_char_len(ln)
            if vlen > max_len:
                return False, pt, f"Guide line too long: {vlen} chars (max {max_len}) in {ln!r}"
    elif kind == "ui":
        lines = pt.split("<1>")
        for ln in lines:
            vlen = visible_char_len(ln)
            if vlen > max_len:
                return False, pt, f"UI line too long: {vlen} chars (max {max_len}) in {ln!r}"

    return True, pt, ""


SYSTEM_PROMPT_BASE = """You are localizing Digimon World 3 (PS1) from English to Brazilian Portuguese (PT-BR).
Strict Rules:
1. Preserve ALL control tags (<1>, <2,2>, <3>, <5,1>, <5,2>, <7>, <8,1>, <9>) in the exact same order.
   - If a string starts with `<7>Speaker<7>`, keep `<7>...<7>` at the start (keep proper names like Junior, Kail, Ivy, Teddy, Kurt, Lisa, Keith, Nick, Asuka, Seiryu, Suzaku, Byakko, Genbu, Amaterasu, A.o.A., MAGAMI; translate generic titles like Guard -> Guarda, Boy -> Garoto, Clerk -> Atendente).
   - If a string starts with `<7><9><7>`, keep `<7><9><7>` untouched.
2. Keep RPG/combat terms in English: Digimon, Digi-Lab, Load Tech, Blast Gauge, Bits, TP, MP, HP, AP, Baton Pass, Relay, Libra, OVERFINISH, WEAK, RESIST, DDNA, Digi-Egg, Card, Booster, Folder, Tamer, Server, Gym, Badge.
   - Keep all Digimon species names and technique names in English.
3. Standard PT-BR terminology for elements & stats:
   - Elements: Fire -> Fogo, Water -> Água, Ice -> Gelo, Wind -> Vento, Thunder -> Trovão, Machine -> Máquina, Dark -> Trevas.
   - Stats: Power/Strength/Atk -> Força/Atq, Guard/Defense/Def -> Defesa/Def, Spirit -> Espírito, Wisdom -> Sabedoria, Boost/Speed -> Velocidade/Vel, Charisma -> Carisma, Affinity -> Afinidade, Tolerance -> Tolerância.
4. Respect the character limit shown in `[max N chars]` when provided. Use natural, crisp PT-BR with proper accents (á, é, í, ó, ú, ã, õ, ç, â, ê, ô, à).
5. Return ONLY a valid JSON object mapping each input key to its translated PT-BR string."""


def call_gemma(url: str, payload_dict: dict[str, str], extra_instruction: str = "", effort: str = "none") -> dict[str, str]:
    sys_msg = SYSTEM_PROMPT_BASE
    if extra_instruction:
        sys_msg += "\n" + extra_instruction
    body = {
        "model": "gemma4-26b-a4b",
        "reasoning_effort": effort,
        "response_format": {"type": "json_object"},
        "messages": [
            {"role": "system", "content": sys_msg},
            {"role": "user", "content": json.dumps(payload_dict, ensure_ascii=False)},
        ],
    }
    req = urllib.request.Request(
        f"{url.rstrip('/')}/v1/chat/completions",
        data=json.dumps(body, ensure_ascii=False).encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=180) as resp:
        res = json.loads(resp.read().decode("utf-8"))
    content = res["choices"][0]["message"]["content"]
    # Strip markdown fences if any
    content = content.strip()
    if content.startswith("```"):
        content = re.sub(r"^```(?:json)?\s*|\s*```$", "", content, flags=re.DOTALL)
    return json.loads(content)


def load_cache() -> dict[str, str]:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    cache = {}
    if CACHE_FILE.exists():
        for line in CACHE_FILE.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            try:
                row = json.loads(line)
                cache[row["key"]] = row["pt"]
            except Exception:
                pass
    return cache


def save_cache_entries(entries: list[tuple[str, str, str]]):
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    with CACHE_FILE.open("a", encoding="utf-8") as f:
        for key, en, pt in entries:
            f.write(json.dumps({"key": key, "en": en, "pt": pt}, ensure_ascii=False) + "\n")


def parse_en_override() -> tuple[list[tuple[str, str]], dict[tuple[str, int], str]]:
    """Return (dollar_rows, file_overrides) from tools/text_override/en.txt."""
    dollars = []
    overrides = {}
    for raw in EN_OVR.read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].rstrip() if not raw.lstrip().startswith("#") else ""
        if not line.strip():
            continue
        head, sep, text = line.partition(":")
        parts = head.split()
        if sep and len(parts) == 1 and parts[0].startswith("$"):
            text = text.strip()
            if len(text) >= 2 and text[0] == text[-1] == '"':
                text = text[1:-1]
            dollars.append((parts[0], text))
        elif sep and len(parts) == 2 and parts[1].isdigit():
            overrides[(parts[0].upper(), int(parts[1]))] = text.strip()
    return dollars, overrides


def collect_tasks(tier: str) -> list[dict]:
    """Collect translation tasks with (cache_key, en_text, kind, max_len)."""
    dollars, en_overrides = parse_en_override()
    tasks = []
    seen_keys = set()

    def add_task(ckey: str, en_text: str, kind: str, max_len: int):
        if ckey in seen_keys:
            return
        seen_keys.add(ckey)
        tasks.append({"ckey": ckey, "en": en_text, "kind": kind, "max_len": max_len})

    # 1. Definitive Edition $NAME strings
    for dname, dtext in dollars:
        if dname in INVARIANT_DOLLAR_KEYS or dname in FIXED_DOLLAR_PT:
            continue
        max_l = 44 if (dname.startswith("$Q") or dname.startswith("$M") or dname.startswith("$Auc") or dname.startswith("$Dri")) else 24
        add_task(f"DOLLAR::{dname}::{dtext}", dtext, "guide" if max_l == 44 else "short_name", max_l)

    # 2. Definitive Edition FILE index overrides (SKLINF, STATUS, TRAINI, ITMNAM, ITMINF)
    for (fname, idx), otext in en_overrides.items():
        if fname == "SKLNAM":
            continue  # Keep technique names in English
        if fname == "SKLINF":
            add_task(f"SKLINF::{otext}", otext, "sklinf", 34)
        elif fname == "ITMNAM":
            add_task(f"ITMNAM::{otext}", otext, "short_name", 16)
        else:
            add_task(f"UI::{fname}::{otext}", otext, "ui", 42)

    # 3. Disc ENG files
    tables = dt.char_tables()
    eng_files = [
        e for e in df.files()
        if len(e.path.split("/")) == 5 and e.path.split("/")[2] == "COUNTRY" and e.path.split("/")[3] == "ENG"
    ]

    for e in eng_files:
        fname = e.path.split("/")[4][2:-4]
        if fname in ("DIGNAM", "SKLNAM", "SYSTEM"):
            continue
        is_talk = fname.startswith("TALK")
        is_dmg = fname.startswith("DMG")
        if tier == "p0" and (is_talk or is_dmg):
            continue
        if tier == "p1" and is_talk and fname not in ("TALK00", "TALK01", "TALK02", "TALK08", "TALK09"):
            continue

        d_en = (df.DISC / e.path).read_bytes()
        d_es = (df.DISC / dt.find_file(fname, "SPN").path).read_bytes()
        d_fr = (df.DISC / dt.find_file(fname, "FRA").path).read_bytes()

        for (sub, offs_en), (_, offs_es), (_, offs_fr) in zip(
            dt.tables_of(d_en), dt.tables_of(d_es), dt.tables_of(d_fr)
        ):
            for idx, (o_en, o_es, o_fr) in enumerate(zip(offs_en, offs_es, offs_fr)):
                if fname == "NAMEDT" and idx not in (1, 18):
                    continue
                if sub is None and (fname, idx) in en_overrides:
                    continue  # Already handled via en_overrides
                s_en = dt.decode(d_en, o_en, tables)[0].replace("\n    ", "")
                if not s_en.strip() or s_en.strip() == "a":
                    continue
                s_es = dt.decode(d_es, o_es, tables)[0].replace("\n    ", "")
                s_fr = dt.decode(d_fr, o_fr, tables)[0].replace("\n    ", "")
                # Skip proper nouns/cards identical in ENG, SPN, and FRA
                if s_en == s_es == s_fr and len(s_en) <= 20:
                    continue

                en_lines = s_en.split("<1>")
                es_lines = s_es.split("<1>")
                fr_lines = s_fr.split("<1>")
                max_orig_line = max(
                    max((visible_char_len(x) for x in en_lines), default=0),
                    max((visible_char_len(x) for x in es_lines), default=0),
                    max((visible_char_len(x) for x in fr_lines), default=0),
                )

                if fname == "SKLINF":
                    add_task(f"SKLINF::{s_en}", s_en, "sklinf", 34)
                elif fname in ("ITMNAM", "CARDNM", "HTLNAM", "SHPNAM", "STAREA"):
                    add_task(f"SHORT::{fname}::{s_en}", s_en, "short_name", max(17, max_orig_line + 1))
                elif fname in ("AMTMAP", "ASKMAP", "STNAME"):
                    add_task(f"MAP::{s_en}", s_en, "short_name", max(22, max_orig_line + 2))
                elif is_talk or is_dmg or fname == "NAMEET":
                    add_task(f"DLG::{s_en}", s_en, "dialogue", max(38, max_orig_line + 4))
                else:
                    add_task(f"UI::{fname}::{s_en}", s_en, "ui", max(38, max_orig_line + 3))

    return tasks


def translate_batch(url: str, items: list[dict]) -> list[tuple[str, str, str]]:
    """Translate a batch of task dicts and validate/retry failures."""
    payload = {}
    by_id = {}
    for i, item in enumerate(items):
        k = str(i + 1)
        by_id[k] = item
        if item["kind"] in ("sklinf", "short_name"):
            payload[k] = f"[max {item['max_len']} chars] {item['en']}"
        else:
            payload[k] = item["en"]

    try:
        res = call_gemma(url, payload, effort="none")
    except Exception as exc:
        print(f"  [warn] batch call failed ({exc}), retrying in halves...", flush=True)
        if len(items) <= 1:
            return [(items[0]["ckey"], items[0]["en"], normalize_chars(items[0]["en"]))]
        mid = len(items) // 2
        return translate_batch(url, items[:mid]) + translate_batch(url, items[mid:])

    accepted = []
    failed = []
    for k, item in by_id.items():
        raw_pt = res.get(k, "")
        if not isinstance(raw_pt, str) or not raw_pt.strip():
            failed.append((item, "Missing translation in JSON"))
            continue
        # Strip accidental [max N chars] echo if the model echoed the hint
        raw_pt = re.sub(r"^\[max\s+\d+\s+chars\]\s*", "", raw_pt.strip(), flags=re.IGNORECASE)
        ok, fixed_pt, reason = validate_translation(item["en"], raw_pt, item["kind"], item["max_len"])
        if ok:
            accepted.append((item["ckey"], item["en"], fixed_pt))
        else:
            failed.append((item, f"{reason} (got: {raw_pt!r})"))

    # Retry failed items individually or in a small corrective batch
    if failed:
        retry_payload = {}
        retry_map = {}
        for idx, (item, err) in enumerate(failed):
            rk = str(idx + 1)
            retry_map[rk] = item
            retry_payload[rk] = (
                f"FIX ERROR ({err}). Must keep exact control tags and <= {item['max_len']} chars per line. "
                f"Source: {item['en']}"
            )
        try:
            rres = call_gemma(
                url,
                retry_payload,
                extra_instruction="Fix the reported validation errors strictly. Shorten phrasing if too long.",
                effort="none",
            )
        except Exception:
            rres = {}

        for rk, item in retry_map.items():
            raw_pt = rres.get(rk, "")
            if isinstance(raw_pt, str) and raw_pt.strip():
                raw_pt = re.sub(r"^\[max\s+\d+\s+chars\]\s*", "", raw_pt.strip(), flags=re.IGNORECASE)
                ok, fixed_pt, _ = validate_translation(item["en"], raw_pt, item["kind"], item["max_len"])
                if ok:
                    accepted.append((item["ckey"], item["en"], fixed_pt))
                    continue
            # Safe fallback: keep normalized English so build never breaks
            print(f"  [fallback EN] {item['ckey'][:40]} -> kept EN", flush=True)
            accepted.append((item["ckey"], item["en"], normalize_chars(item["en"])))

    return accepted


def export_pt_txt(cache: dict[str, str]):
    """Write tools/text_override/pt.txt combining all translated entries."""
    cache = {**cache, **FIXED_CACHE_OVERRIDES}
    dollars, en_overrides = parse_en_override()
    tables = dt.char_tables()
    lines = ["# Generated by tools/translate_ptbr.py — Brazilian Portuguese (PT-BR) localization", ""]

    # 1. $NAME strings
    for dname, dtext in dollars:
        if dname in INVARIANT_DOLLAR_KEYS:
            pt = dtext
        elif dname in FIXED_DOLLAR_PT:
            pt = FIXED_DOLLAR_PT[dname]
        else:
            pt = cache.get(f"DOLLAR::{dname}::{dtext}", dtext)
        # Preserve quotes if leading/trailing spaces exist
        if pt.startswith(" ") or pt.endswith(" "):
            lines.append(f'{dname}: "{pt}"')
        else:
            lines.append(f"{dname}: {pt}")

    lines.append("")

    # 2. All 50 disc files + en_overrides
    eng_files = [
        e for e in df.files()
        if len(e.path.split("/")) == 5 and e.path.split("/")[2] == "COUNTRY" and e.path.split("/")[3] == "ENG"
    ]

    count = 0
    for e in eng_files:
        fname = e.path.split("/")[4][2:-4]
        if fname in ("DIGNAM", "SYSTEM"):
            continue
        d_en = (df.DISC / e.path).read_bytes()
        d_es = (df.DISC / dt.find_file(fname, "SPN").path).read_bytes()
        d_fr = (df.DISC / dt.find_file(fname, "FRA").path).read_bytes()

        for (sub, offs_en), (_, offs_es), (_, offs_fr) in zip(
            dt.tables_of(d_en), dt.tables_of(d_es), dt.tables_of(d_fr)
        ):
            for idx, (o_en, o_es, o_fr) in enumerate(zip(offs_en, offs_es, offs_fr)):
                if fname == "SKLNAM":
                    if (fname, idx) in en_overrides:
                        lines.append(f"{fname} {idx}: {en_overrides[(fname, idx)]}")
                        count += 1
                    continue
                if fname == "NAMEDT" and idx not in (1, 18):
                    continue

                if sub is None and (fname, idx) in en_overrides:
                    s_en = en_overrides[(fname, idx)]
                else:
                    s_en = dt.decode(d_en, o_en, tables)[0].replace("\n    ", "")
                    if not s_en.strip() or s_en.strip() == "a":
                        continue
                    s_es = dt.decode(d_es, o_es, tables)[0].replace("\n    ", "")
                    s_fr = dt.decode(d_fr, o_fr, tables)[0].replace("\n    ", "")
                    if s_en == s_es == s_fr and len(s_en) <= 20:
                        continue

                # Lookup in cache by priority of key formats
                is_talk = fname.startswith("TALK")
                is_dmg = fname.startswith("DMG")
                if fname == "SKLINF":
                    ckey = f"SKLINF::{s_en}"
                elif fname == "ITMNAM" and (fname, idx) in en_overrides:
                    ckey = f"ITMNAM::{s_en}"
                elif fname in ("ITMNAM", "CARDNM", "HTLNAM", "SHPNAM", "STAREA"):
                    ckey = f"SHORT::{fname}::{s_en}"
                elif fname in ("AMTMAP", "ASKMAP", "STNAME"):
                    ckey = f"MAP::{s_en}"
                elif is_talk or is_dmg or fname == "NAMEET":
                    ckey = f"DLG::{s_en}"
                else:
                    ckey = f"UI::{fname}::{s_en}"

                pt = cache.get(ckey)
                if not pt:
                    continue
                pt = normalize_chars(pt)
                if sub is None:
                    lines.append(f"{fname} {idx}: {pt}")
                else:
                    lines.append(f"{fname}:{sub} {idx}: {pt}")
                count += 1

    PT_OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Exported {count} overrides + {len(dollars)} $NAME strings to {PT_OUT.relative_to(ROOT)}", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:8080")
    ap.add_argument("--tier", choices=["p0", "p1", "all"], default="all")
    ap.add_argument("--batch-size", type=int, default=35)
    ap.add_argument("--export", action="store_true")
    a = ap.parse_args()

    cache = load_cache()
    if a.export:
        export_pt_txt(cache)
        return

    tasks = collect_tasks(a.tier)
    pending = [t for t in tasks if t["ckey"] not in cache]
    print(
        f"Tier {a.tier}: {len(tasks)} total unique tasks, {len(cache)} cached, {len(pending)} pending",
        flush=True,
    )

    t_start = time.time()
    done = 0
    for i in range(0, len(pending), a.batch_size):
        batch = pending[i:i + a.batch_size]
        b0 = time.time()
        results = translate_batch(a.url, batch)
        save_cache_entries(results)
        for ck, _, pt in results:
            cache[ck] = pt
        done += len(batch)
        elapsed = time.time() - t_start
        rate = done / elapsed if elapsed > 0 else 0
        eta = (len(pending) - done) / rate if rate > 0 else 0
        print(
            f"[{done:4d}/{len(pending):4d}] batch={len(batch):2d} in {time.time()-b0:5.1f}s | "
            f"avg={rate:4.1f} str/s | ETA={eta/60:4.1f}m | sample: {results[0][2][:45]!r}",
            flush=True,
        )

    export_pt_txt(cache)


if __name__ == "__main__":
    main()
