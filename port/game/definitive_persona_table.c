/* The enemy personas, written by hand (definitive_persona.h has the helpers and the rules; the host test
 * tests/port/definitive_persona.py checks every row and prints the table for review). One row per special enemy;
 * what a row does not say comes from its species. KEEP ids name the discovery in saves: never reuse or renumber one. */
#include "definitive.h"
#include "definitive_persona.h"

/* GCC 8 (the PortMaster toolchains, Debian buster) reports nested designated initializers such as .phase[1].hue as
 * missing the phase's other fields; C zeroes them, which is what the rows mean. Newer GCC and clang do not warn. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

const DefPersona def_personas[] = {
    /* DRAFT 2026-11-23, written without the owner from the design guide; the review table is
     * .dumps/tasks/persona/personas.proposed.json. The molds' names are the guide's (Fechadura, Agulha, ...).
     * The red guard (owner, 2026-11-24, RED_*): about one campaign common Fechadura in three from Lv28, a reversed
     * tide at Lv55 and the BK WarGreymon story boss, so that physical play meets a guard it never opens; few on
     * purpose, a red guard being easy for the magic the game was planned around. */
    { CAMPAIGN(313, 141), GUARD_ALWAYS }, /* Datamon, Lv34: guard_campanha */
    { CAMPAIGN(15, 444), GUARD_ALWAYS }, /* BK Imperialdramon, Lv46: guard_campanha */
    /* owner, 2026-11-24: the campaign's red-guard boss, weak to Dark alone; its HP is kept (no lowering) */
    { CAMPAIGN(31, 448), KEEP(111), WEAK(DARK), RESIST(FIRE | WATER | WIND | THUNDER), RED_ALWAYS }, /* BK WarGreymon, Lv53: fechadura vermelha */
    { SUPER(1), KEEP(0), WEAK(WATER), RESIST(FIRE | WIND) }, /* MasterTyrannomon, Lv29: fraqueza_unica */
    { SUPER(2), KEEP(1), WEAK(WIND), RESIST(FIRE | THUNDER), GUARD_ALWAYS }, /* Pharaohmon, Lv37: fechadura */
    { SUPER(3), KEEP(2), WEAK(FIRE), RESIST(WATER), PHASES(2), B_WEAK(THUNDER), B_RESIST(WIND), HUES(330, 120) }, /* Cherrymon, Lv40: camaleao */
    { SUPER(4), KEEP(3), RESIST_ALL, GUARD_A, PHASES(2), DEF_PCT(60) }, /* Knightmon, Lv46: brutamontes+mare */
    { SUPER(5), KEEP(4), WEAK(WATER), RESIST(FIRE | WIND | THUNDER | DARK) }, /* Whamon, Lv46: contramao+agulha */
    { SUPER(6), KEEP(5), WEAK(FIRE), RESIST(WATER | WIND), SPEED_X(3), DEF_PCT(60) }, /* WaruMonzaemon, Lv47: relampago */
    { SUPER(7), KEEP(6), WEAK(DARK), RESIST(FIRE | WIND), GUARD_ALWAYS, PHASES(3), B_WEAK(THUNDER), B_RESIST(FIRE), HUES(270, 45) }, /* BK Seraphimon, Lv47: fechadura+camaleao */
    { SUPER(8), KEEP(7), WEAK(WATER), RESIST(FIRE), PHASES(2), B_WEAK(WIND), B_RESIST(THUNDER), HUES(200, 60), SPEED_X(3), DEF_PCT(60) }, /* Kimeramon, Lv52: camaleao+relampago */
    { SUPER(9), KEEP(8), WEAK(THUNDER), RESIST(WIND | FIRE), GUARD_A, PHASES(2) }, /* Gryphonmon, Lv56: mare+fraqueza_unica */
    { SUPER(10), KEEP(9), RESIST_ALL, SPEED_X(3), DEF_PCT(60) }, /* GrapLeomon, Lv57: brutamontes+relampago */
    { SUPER(11), KEEP(10), WEAK(THUNDER), RESIST(FIRE | WIND), GUARD_ALWAYS, ON_STAGGER, B_WEAK(WATER), B_RESIST(THUNDER), HUES(180, 30) }, /* HiAndromon, Lv59: reflexo */
    { SUPER(12), KEEP(11), WEAK(WIND), RESIST(WATER), GUARD_A, PHASES(2), B_WEAK(FIRE), B_RESIST(THUNDER), HUES(20, 280) }, /* Kyukimon, Lv61: mare+camaleao */
    { DRI(0), KEEP(20), RESIST_ALL, DEF_PCT(60) }, /* Terriermon (r9), Lv57: brutamontes */
    { DRI(1), KEEP(21), WEAK(WATER), RESIST(FIRE | WIND), GUARD_ALWAYS }, /* Veemon (r3), Lv33: fechadura */
    { DRI(2), KEEP(22), WEAK(WIND), RESIST(FIRE), PHASES(3), B_WEAK(THUNDER), B_RESIST(WATER), HUES(55, 200) }, /* Patamon (r4), Lv37: camaleao */
    { DRI(3), KEEP(23), WEAK(FIRE), RESIST(WATER | THUNDER) }, /* Agumon (r6), Lv45: contramao */
    { DRI(4), KEEP(24), WEAK(WATER), RESIST(FIRE), GUARD_ALWAYS, ON_STAGGER, B_WEAK(WIND), B_RESIST(THUNDER), HUES(200, 100) }, /* Guilmon (r7), Lv49: reflexo */
    { DRI(5), KEEP(25), WEAK(FIRE), RESIST(DARK), PHASES(2), B_WEAK(THUNDER), B_RESIST(DARK | WATER), HUES(280, 20) }, /* Devimon (r12), Lv69: camaleao */
    { DRI(6), KEEP(26), WEAK(THUNDER), RESIST(FIRE | WATER), SPEED_X(3), DEF_PCT(60) }, /* Renamon (r5), Lv41: relampago */
    { DRI(7), KEEP(27), WEAK(DARK), RESIST(FIRE | WATER | WIND | THUNDER) }, /* Angewomon (r13), Lv73: agulha */
    { DRI(8), KEEP(28), WEAK(FIRE), RESIST(WATER | WIND | THUNDER | DARK), GUARD_A, PHASES(2) }, /* Wormmon (r11), Lv65: mare+agulha */
    { DRI(9), KEEP(29), RESIST_ALL, GUARD_A, PHASES(3), DEF_PCT(60) }, /* Agumon (dark) (r14), Lv77: brutamontes+mare */
    { DRI(10), KEEP(30), WEAK(DARK), RESIST(FIRE), GUARD_ALWAYS, ON_STAGGER, B_WEAK(FIRE), B_RESIST(DARK), HUES(300, 10), SPEED_X(2), DEF_PCT(60) }, /* Impmon (r15), Lv81: reflexo+relampago */
    { DRI(11), KEEP(31), WEAK(THUNDER), RESIST(WATER) }, /* Kotemon (r0), Lv21: fraqueza_unica */
    { DRI(12), KEEP(32), WEAK(FIRE), RESIST(WATER | WIND), GUARD_ALWAYS }, /* Musyamon (r8), Lv53: fechadura */
    { DRI(13), KEEP(33), WEAK(WATER), RESIST(FIRE | WIND) }, /* Kumamon (r2), Lv29: contramao */
    { DRI(14), KEEP(34), WEAK(FIRE), RESIST(WIND), PHASES(2), B_WEAK(WATER), B_RESIST(THUNDER), HUES(15, 190) }, /* Kuwagamon (r10), Lv61: camaleao */
    { DRI(15), KEEP(35), WEAK(WIND) }, /* Monmon (r1), Lv25: fraqueza_unica */
    { RARE(0x1), KEEP(40), WEAK(FIRE), RESIST(WATER | WIND | THUNDER | DARK) }, /* Snimon, Lv48: agulha */
    { RARE(0x2), KEEP(41), WEAK(WATER), RESIST(FIRE | WIND) }, /* BK MegaGargomon, Lv46: contramao */
    { RARE(0x3), KEEP(42), WEAK(THUNDER), RESIST(WATER | WIND), GUARD_ALWAYS }, /* KingEtemon, Lv46: fechadura */
    { RARE(0x4), KEEP(43), WEAK(FIRE), RESIST(WATER), GUARD_A, PHASES(2) }, /* Antylamon, Lv46: mare */
    { RARE(0x7), KEEP(44), RESIST_ALL, DEF_PCT(60) }, /* Guardromon, Lv29: brutamontes */
    { RARE(0x8), KEEP(45), WEAK(THUNDER), RESIST(WATER | WIND), SPEED_X(3), DEF_PCT(60) }, /* BK WarGrowlmon, Lv48: relampago */
    { RARE(0x9), KEEP(46), WEAK(WATER), RESIST(FIRE), PHASES(2), B_WEAK(WIND), B_RESIST(WATER), HUES(40, 200) }, /* Deltamon, Lv46: camaleao */
    { RARE(0xd), GUARD_A, PHASES(2) }, /* Apemon, Lv13: mare */
    { RARE(0xf), KEEP(48), WEAK(WIND) }, /* Tuskmon, Lv11: fraqueza_unica */
    { RARE(0x10), KEEP(49), WEAK(WATER), RESIST(FIRE | WIND), GUARD_ALWAYS }, /* Tyrannomon, Lv27: fechadura */
    { RARE(0x11), KEEP(50), RESIST_ALL, DEF_PCT(60) }, /* Gizamon, Lv15: brutamontes */
    { RARE(0x12), KEEP(51), WEAK(WIND), RESIST(WATER), SPEED_X(3), DEF_PCT(60) }, /* Gesomon, Lv23: relampago */
    { RARE(0x13), KEEP(52), RESIST_ALL, DEF_PCT(60) }, /* Phantomon, Lv32: brutamontes */
    { RARE(0x14), GUARD_A, PHASES(2) }, /* Dokugumon, Lv14: mare */
    { RARE(0x15), KEEP(54), WEAK(FIRE), RESIST(WIND | THUNDER), GUARD_ALWAYS }, /* Kuwagamon, Lv16: fechadura */
    { RARE(0x16), GUARD_A, PHASES(2) }, /* Shellmon, Lv17: mare */
    { RARE(0x17), KEEP(56), WEAK(FIRE), RESIST(WIND | THUNDER), GUARD_ALWAYS }, /* Kokatorimon, Lv21: fechadura */
    { RARE(0x18), KEEP(57), WEAK(THUNDER) }, /* Woodmon, Lv11: fraqueza_unica */
    { RARE(0x19), KEEP(58), WEAK(WIND), RESIST(THUNDER), SPEED_X(3), DEF_PCT(60) }, /* Flymon, Lv26: relampago */
    { RARE(0x1a), KEEP(59), RESIST_ALL, DEF_PCT(60) }, /* Mammothmon, Lv25: brutamontes */
    { RARE(0x1b), KEEP(60), WEAK(WATER), RESIST(FIRE), PHASES(2), B_WEAK(THUNDER), B_RESIST(WATER), HUES(200, 20) }, /* WarGrowlmon, Lv25: camaleao */
    { RARE(0x1c), KEEP(61), WEAK(WATER), RESIST(FIRE | WIND | THUNDER | DARK) }, /* Vemmon, Lv27: agulha */
    { RARE(0x1d), KEEP(62), WEAK(FIRE) }, /* Betamon, Lv6: fraqueza_unica */
    { RARE(0x20), GUARD_A, PHASES(2) }, /* Coelamon, Lv12: mare */
    { RARE(0x22), KEEP(64), RESIST_ALL, DEF_PCT(60) }, /* MagnaAngemon, Lv26: brutamontes */
    { RARE(0x23), KEEP(65), WEAK(WIND), RESIST(FIRE | THUNDER) }, /* Persiamon, Lv38: contramao */
    { RARE(0x25), KEEP(66), WEAK(FIRE), RESIST(WATER | THUNDER), GUARD_ALWAYS }, /* Paildramon, Lv15: fechadura */
    { RARE(0x26), KEEP(67), WEAK(WATER), RESIST(WIND), PHASES(2), B_WEAK(THUNDER), B_RESIST(WATER), HUES(300, 90) }, /* Armormon, Lv27: camaleao */
    { RARE(0x27), KEEP(68), WEAK(WIND), RESIST(FIRE | WATER | THUNDER | DARK) }, /* Datamon, Lv27: agulha */
    { RARE(0x28), KEEP(69), WEAK(THUNDER) }, /* Etemon, Lv18: fraqueza_unica */
    { RARE(0x29), KEEP(70), WEAK(WIND), RESIST(WATER), GUARD_ALWAYS }, /* Tortomon, Lv25: fechadura */
    { RARE(0x2a), KEEP(71), WEAK(WATER), RESIST(FIRE | THUNDER) }, /* Crabmon, Lv18: contramao */
    { RARE(0x2b), KEEP(72), WEAK(THUNDER) }, /* ShogunGekomon, Lv13: fraqueza_unica */
    { RARE(0x2c), KEEP(73), WEAK(FIRE), RESIST(WATER), SPEED_X(3), DEF_PCT(60) }, /* Dolphmon, Lv21: relampago */
    { RARE(0x2d), KEEP(74), WEAK(WATER) }, /* Minotarumon, Lv13: fraqueza_unica */
    { RARE(0x81), KEEP(75), WEAK(FIRE), RESIST(WATER), GUARD_A, PHASES(2) }, /* Tapirmon, Lv52: mare */
    { RARE(0x82), KEEP(76), RESIST_ALL, DEF_PCT(60) }, /* Raidenmon, Lv53: brutamontes */
    { RARE(0x83), KEEP(77), WEAK(WIND), RESIST(WATER | THUNDER), SPEED_X(3), DEF_PCT(60) }, /* Suijinmon, Lv52: relampago */
    { RARE(0x84), KEEP(78), WEAK(FIRE), RESIST(WATER), PHASES(2), B_WEAK(DARK), B_RESIST(FIRE), HUES(200, 20) }, /* Raijinmon, Lv52: camaleao */
    { RARE(0x87), KEEP(79), WEAK(WATER), RESIST(DARK), GUARD_ALWAYS, ON_STAGGER, B_WEAK(DARK), B_RESIST(WATER), HUES(300, 90) }, /* Thundermon, Lv51: reflexo */
    { RARE(0x88), KEEP(80), WEAK(FIRE), RESIST(WATER | WIND | THUNDER | DARK) }, /* MetalGreymon, Lv53: agulha */
    { RARE(0x89), KEEP(81), WEAK(THUNDER), RESIST(WATER | WIND) }, /* Arukenimon, Lv53: contramao */
    { RARE(0x8d), KEEP(82), WEAK(WIND), RESIST(FIRE | WATER | THUNDER | DARK) }, /* Kurisarimon, Lv28: agulha */
    { RARE(0x8f), KEEP(83), WEAK(WATER), RESIST(FIRE), GUARD_A, PHASES(2) }, /* Triceramon, Lv41: mare */
    { RARE(0x90), KEEP(84), RESIST_ALL, DEF_PCT(60) }, /* Armageddemon, Lv50: brutamontes */
    { RARE(0x91), GUARD_A, PHASES(2) }, /* Airdramon, Lv27: mare */
    { RARE(0x92), KEEP(86), WEAK(THUNDER), RESIST(WATER), PHASES(2), B_WEAK(WIND), B_RESIST(THUNDER), HUES(60, 240) }, /* Gekomon, Lv42: camaleao */
    { RARE(0x93), KEEP(87), WEAK(FIRE), RESIST(THUNDER), GUARD_ALWAYS, ON_STAGGER, B_WEAK(THUNDER), B_RESIST(FIRE), HUES(160, 330) }, /* BK Imperialdramon, Lv39: reflexo */
    { RARE(0x94), KEEP(88), WEAK(WATER), RESIST(FIRE | WIND | THUNDER | DARK) }, /* Yanmamon, Lv35: agulha */
    { RARE(0x95), KEEP(89), WEAK(WIND), RESIST(FIRE | WATER) }, /* Okuwamon, Lv26: contramao */
    { RARE(0x96), KEEP(90), WEAK(THUNDER), RESIST(FIRE | WATER), GUARD_ALWAYS }, /* Ebidramon, Lv40: fechadura */
    { RARE(0x97), KEEP(91), WEAK(FIRE), RESIST(WATER), GUARD_A, PHASES(2) }, /* Vikemon, Lv43: mare */
    { RARE(0x98), KEEP(92), RESIST_ALL, DEF_PCT(60) }, /* Roachmon, Lv49: brutamontes */
    { RARE(0x99), KEEP(93), WEAK(WIND), RESIST(WATER | THUNDER), SPEED_X(3), DEF_PCT(60) }, /* Puppetmon, Lv50: relampago */
    { RARE(0x9a), KEEP(94), WEAK(THUNDER), RESIST(FIRE), PHASES(2), B_WEAK(DARK), B_RESIST(THUNDER), HUES(40, 200) }, /* Lynxmon, Lv49: camaleao */
    { RARE(0x9b), KEEP(95), WEAK(WATER), RESIST(FIRE), GUARD_ALWAYS, ON_STAGGER, B_WEAK(FIRE), B_RESIST(WATER), HUES(120, 300) }, /* Boltmon, Lv49: reflexo */
    { RARE(0x9c), KEEP(96), WEAK(WIND), RESIST(FIRE | WATER | THUNDER | DARK) }, /* Vilemon, Lv50: agulha */
    { RARE(0x9d), KEEP(97), WEAK(WATER), RESIST(FIRE | THUNDER) }, /* Goburimon, Lv21: contramao */
    { RARE(0xa0), KEEP(98), WEAK(FIRE), RESIST(WATER), GUARD_ALWAYS }, /* Divermon, Lv22: fechadura */
    { RARE(0xa2), KEEP(99), WEAK(WATER), RESIST(FIRE), GUARD_A, PHASES(2) }, /* Maildramon, Lv50: mare */
    { RARE(0xa3), KEEP(100), RESIST_ALL, DEF_PCT(60) }, /* Raremon, Lv44: brutamontes */
    { RARE(0xa5), KEEP(101), WEAK(THUNDER), RESIST(WATER), SPEED_X(3), DEF_PCT(60) }, /* Seadramon, Lv40: relampago */
    { RARE(0xa6), KEEP(102), WEAK(THUNDER), RESIST(FIRE), PHASES(2), B_WEAK(DARK), B_RESIST(THUNDER), HUES(200, 20) }, /* BK WarGreymon, Lv50: camaleao */
    { RARE(0xa7), KEEP(103), WEAK(WATER), RESIST(WIND), GUARD_ALWAYS, ON_STAGGER, B_WEAK(WIND), B_RESIST(WATER), HUES(300, 90) }, /* Fujinmon, Lv51: reflexo */
    { RARE(0xa8), KEEP(104), WEAK(FIRE), RESIST(WATER | WIND | THUNDER | DARK) }, /* Blossomon, Lv42: agulha */
    { RARE(0xa9), KEEP(105), WEAK(FIRE), RESIST(WIND | THUNDER) }, /* Scorpiomon, Lv42: contramao */
    { RARE(0xaa), KEEP(106), WEAK(WIND), RESIST(FIRE | THUNDER), GUARD_ALWAYS }, /* MetalSeadramon, Lv40: fechadura */
    { RARE(0xab), KEEP(107), WEAK(THUNDER), RESIST(FIRE), GUARD_A, PHASES(2) }, /* MegaSeadramon, Lv40: mare */
    { RARE(0xac), KEEP(108), RESIST_ALL, DEF_PCT(60) }, /* Megadramon, Lv41: brutamontes */
    { RARE(0xad), KEEP(109), WEAK(THUNDER), RESIST(WATER | WIND), SPEED_X(3), DEF_PCT(60) }, /* Cherrymon, Lv41: relampago */
    { RARE(0xff), KEEP(110), WEAK(WIND), RESIST(THUNDER), PHASES(2), B_WEAK(WATER), B_RESIST(WIND), HUES(60, 240) }, /* Bulbmon, Lv44: camaleao */
    { SPECIES(222), GUARD_A, PHASES(2) }, /* Woodmon, Lv12: mare */
    { SPECIES(277), RESIST_ALL, DEF_PCT(60) }, /* Baronmon, Lv16: brutamontes */
    { SPECIES(435), GUARD_A, PHASES(2) }, /* Guardromon, Lv29: mare */
    { SPECIES(208), RESIST_ALL, DEF_PCT(60) }, /* Tortomon, Lv29: brutamontes */
    { SPECIES(35), GUARD_A, PHASES(2) }, /* Shellmon, Lv39: mare */
    { SPECIES(108), RESIST_ALL, DEF_PCT(60) }, /* Rockmon, Lv49: brutamontes */
    { SPECIES(217), GUARD_A, PHASES(2) }, /* DemiDevimon, Lv10: mare */
    { SPECIES(42), GUARD_A, ON_STAGGER }, /* Etemon, Lv20: fechadura */
    { SPECIES(40), SPEED_X(2), DEF_PCT(60) }, /* Andromon, Lv24: relampago */
    { SPECIES(460), GUARD_A, PHASES(2) }, /* Cardmon, Lv27: mare */
    /* owner, 2026-11-24: the red guard's gatekeeper, a story fight of its own in the Admin Center (battle 322):
     * the weakest Digimon of its levels brought up to the Lv28 bosses (HP 1584, Power ~415), its two weaknesses
     * kept so the lesson is the guard, not a lock */
    { CAMPAIGN(322, 465), HP_PCT(200), POW_PCT(180), RED_ALWAYS }, /* Vemmon, Lv28: fechadura vermelha (porteiro) */
    { SPECIES(13), SPEED_X(2), DEF_PCT(60) }, /* Mamemon, Lv29: relampago */
    { SPECIES(53), RESIST_ALL, DEF_PCT(60) }, /* Nanimon, Lv29: brutamontes */
    { SPECIES(173), GUARD_A, PHASES(2) }, /* Musyamon, Lv31: mare */
    { SPECIES(200), RED_ALWAYS }, /* Blossomon, Lv34: fechadura vermelha */
    { SPECIES(61), SPEED_X(2), DEF_PCT(60) }, /* MegaSeadramon, Lv35: relampago */
    { SPECIES(37), GUARD_A, PHASES(2) }, /* Bakemon, Lv35: mare */
    { SPECIES(220), GUARD_A, ON_STAGGER }, /* Gesomon, Lv36: fechadura */
    { SPECIES(406), SPEED_X(2), DEF_PCT(60) }, /* Tortomon, Lv36: relampago */
    { SPECIES(407), RESIST_ALL, DEF_PCT(60) }, /* ShogunGekomon, Lv39: brutamontes */
    { SPECIES(414), GUARD_A, PHASES(2) }, /* Numemon, Lv39: mare */
    { SPECIES(422), RED_ALWAYS }, /* Kurisarimon, Lv40: fechadura vermelha */
    { SPECIES(418), SPEED_X(2), DEF_PCT(60) }, /* Raremon, Lv40: relampago */
    { SPECIES(166), GUARD_A, PHASES(2) }, /* Dokugumon, Lv41: mare */
    { SPECIES(462), GUARD_A, ON_STAGGER }, /* Cardmon, Lv42: fechadura */
    { SPECIES(50), SPEED_X(2), DEF_PCT(60) }, /* Kokatorimon, Lv42: relampago */
    { SPECIES(39), RESIST_ALL, DEF_PCT(60) }, /* Sukamon, Lv42: brutamontes */
    { SPECIES(89), GUARD_A, PHASES(2) }, /* Fugamon, Lv42: mare */
    { SPECIES(136), RED_ALWAYS }, /* Tuskmon, Lv42: fechadura vermelha */
    { SPECIES(202), SPEED_X(2), DEF_PCT(60) }, /* Scorpiomon, Lv43: relampago */
    { SPECIES(210), GUARD_A, PHASES(2) }, /* Dolphmon, Lv43: mare */
    { SPECIES(126), GUARD_A, ON_STAGGER }, /* Brachiomon, Lv44: fechadura */
    { SPECIES(28), SPEED_X(2), DEF_PCT(60) }, /* Vademon, Lv44: relampago */
    { SPECIES(397), RESIST_ALL, DEF_PCT(60) }, /* Dokugumon, Lv45: brutamontes */
    { SPECIES(420), GUARD_A, PHASES(2) }, /* Tankmon, Lv45: mare */
    { SPECIES(137), GUARD_A, ON_STAGGER }, /* Flymon, Lv45: fechadura */
    { SPECIES(269), SPEED_X(2), DEF_PCT(60) }, /* Valkyrimon, Lv45: relampago */
    { SPECIES(270), GUARD_A, PHASES(2) }, /* Vikemon, Lv45: mare */
    { SPECIES(138), GUARD_A, ON_STAGGER }, /* Deltamon, Lv45: fechadura */
    { SPECIES(23), SPEED_X(2), DEF_PCT(60) }, /* Frigimon, Lv45: relampago */
    { SPECIES(424), RESIST_ALL, DEF_PCT(60) }, /* Blossomon, Lv45: brutamontes */
    { SPECIES(238), GUARD_A, PHASES(2) }, /* Snimon, Lv46: mare */
    { SPECIES(65), GUARD_A, ON_STAGGER }, /* MetalEtemon, Lv46: fechadura */
    { SPECIES(426), SPEED_X(2), DEF_PCT(60) }, /* Whamon, Lv47: relampago */
    { SPECIES(464), GUARD_A, PHASES(2) }, /* Cardmon, Lv47: mare */
    { SPECIES(272), RED_ALWAYS }, /* Shadramon, Lv47: fechadura vermelha */
    { SPECIES(463), SPEED_X(2), DEF_PCT(60) }, /* Cardmon, Lv47: relampago */
    { SPECIES(135), RESIST_ALL, DEF_PCT(60) }, /* Devidramon, Lv48: brutamontes */
    { SPECIES(360), GUARD_A, PHASES(2) }, /* Antylamon, Lv49: mare */
    { SPECIES(431), GUARD_A, ON_STAGGER }, /* Vikemon, Lv50: fechadura */
    { SPECIES(7), SPEED_X(2), DEF_PCT(60) }, /* Airdramon, Lv50: relampago */
    { SPECIES(411), GUARD_A, PHASES(2) }, /* Maildramon, Lv50: mare */
    { SPECIES(115), RED_ALWAYS }, /* Machinedramon, Lv51: fechadura vermelha */
    { SPECIES(327), SPEED_X(2), DEF_PCT(60) }, /* Lynxmon, Lv52: relampago */
    { SPECIES(176), RESIST_ALL, DEF_PCT(60) }, /* Thundermon, Lv52: brutamontes */
    { SPECIES(132), GUARD_A, PHASES(2) }, /* Gizamon, Lv52: mare */
    { SPECIES(60), GUARD_A, ON_STAGGER }, /* H−Kabuterimon, Lv52: fechadura */
    { SPECIES(224), SPEED_X(2), DEF_PCT(60) }, /* MarineDevimon, Lv52: relampago */
    { SPECIES(203), GUARD_A, PHASES(2) }, /* Triceramon, Lv52: mare */
    { SPECIES(434), GUARD_A, ON_STAGGER }, /* Antylamon, Lv52: fechadura */
    { SPECIES(8), SPEED_X(2), DEF_PCT(60) }, /* Tyrannomon, Lv52: relampago */
    { SPECIES(206), RESIST_ALL, DEF_PCT(60) }, /* Tapirmon, Lv53: brutamontes */
    { SPECIES(110), GUARD_A, PHASES(2) }, /* Gekomon, Lv53: mare */
    { SPECIES(281), GUARD_A, ON_STAGGER }, /* Tylomon, Lv53: fechadura */
    { SPECIES(312), SPEED_X(2), DEF_PCT(60) }, /* Ghoulmon, Lv54: relampago */
    { SPECIES(247), GUARD_A, RED_B, PHASES(2) }, /* Creepymon, Lv55: mare invertida (laranja/vermelha) */
    { SPECIES(251), RED_ALWAYS }, /* Apokarimon, Lv56: fechadura vermelha */
    { SPECIES(396), SPEED_X(2), DEF_PCT(60) }, /* Yanmamon, Lv58: relampago */
    { SPECIES(428), RESIST_ALL, DEF_PCT(60) }, /* Ebidramon, Lv60: brutamontes */
};
const int def_persona_count = sizeof(def_personas) / sizeof(def_personas[0]);
