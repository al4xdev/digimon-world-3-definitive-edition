/* The Rare Target of each region (one enemy slot of an
 * existing battle, the level it fights at, its rare item's numeral: 0 when its species is no other region's),
 * the Super Rares in the hunts' order, then each bestiary species' common material. First written by
 * tools/gen_hunt_targets.py; hand-maintained since 2026-11-23 (the generator is LEGACY). */
#include "definitive.h"

const DefHuntTarget def_hunt_targets[] = {
    { 0x01, 175, 0, 48, 0 }, /* Snimon: Snimon Core */
    { 0x02, 13, 0, 46, 0 }, /* BK MegaGargomon: BK MegaGargomon Core */
    { 0x03, 133, 0, 46, 0 }, /* KingEtemon: KingEtemon Core */
    { 0x04, 126, 0, 46, 0 }, /* Antylamon: Antylamon Core */
    { 0x07, 76, 0, 29, 0 }, /* Guardromon: Guardromon Core */
    { 0x08, 23, 0, 48, 0 }, /* BK WarGrowlmon: BK WarGrowlmon Core */
    { 0x09, 238, 1, 46, 0 }, /* Deltamon: Deltamon Core */
    { 0x0D, 3, 2, 13, 0 }, /* Apemon: Apemon Core */
    { 0x0F, 210, 1, 11, 0 }, /* Tuskmon: Tuskmon Core */
    { 0x10, 233, 1, 27, 0 }, /* Tyrannomon: Tyrannomon Core */
    { 0x11, 214, 1, 15, 0 }, /* Gizamon: Gizamon Core */
    { 0x12, 62, 0, 23, 0 }, /* Gesomon: Gesomon Core */
    { 0x13, 56, 0, 32, 0 }, /* Phantomon: Phantomon Core */
    { 0x14, 215, 1, 14, 0 }, /* Dokugumon: Dokugumon Core */
    { 0x15, 263, 1, 16, 0 }, /* Kuwagamon: Kuwagamon Core */
    { 0x16, 59, 0, 17, 0 }, /* Shellmon: Shellmon Core */
    { 0x17, 156, 0, 21, 0 }, /* Kokatorimon: Kokatorimon Core */
    { 0x18, 152, 0, 11, 0 }, /* Woodmon: Woodmon Core */
    { 0x19, 6, 0, 26, 0 }, /* Flymon: Flymon Core */
    { 0x1A, 67, 0, 25, 0 }, /* Mammothmon: Mammothmon Core */
    { 0x1B, 265, 0, 25, 0 }, /* WarGrowlmon: WarGrowlmon Core */
    { 0x1C, 322, 0, 27, 0 }, /* Vemmon: Vemmon Core */
    { 0x1D, 208, 0, 6, 0 }, /* Betamon: Betamon Core */
    { 0x20, 54, 0, 12, 0 }, /* Coelamon: Coelamon Core */
    { 0x22, 266, 0, 26, 0 }, /* MagnaAngemon: MagnaAngemon Core */
    { 0x23, 10, 2, 38, 0 }, /* Persiamon: Persiamon Core */
    { 0x25, 271, 0, 15, 0 }, /* Paildramon: Paildramon Core */
    { 0x26, 270, 0, 27, 0 }, /* Armormon: Armormon Core */
    { 0x27, 9, 0, 27, 0 }, /* Datamon: Datamon Core */
    { 0x28, 68, 0, 18, 0 }, /* Etemon: Etemon Core */
    { 0x29, 6, 1, 25, 0 }, /* Tortomon: Tortomon Core */
    { 0x2A, 214, 0, 18, 0 }, /* Crabmon: Crabmon Core */
    { 0x2B, 4, 2, 13, 0 }, /* ShogunGekomon: ShogunGekomon Core */
    { 0x2C, 61, 0, 21, 0 }, /* Dolphmon: Dolphmon Core */
    { 0x2D, 3, 0, 13, 0 }, /* Minotarumon: Minotarumon Core */
    { 0x81, 245, 0, 52, 0 }, /* Tapirmon: Tapirmon Core */
    { 0x82, 32, 2, 53, 0 }, /* Raidenmon: Raidenmon Core */
    { 0x83, 33, 1, 52, 0 }, /* Suijinmon: Suijinmon Core */
    { 0x84, 33, 2, 52, 0 }, /* Raijinmon: Raijinmon Core */
    { 0x87, 243, 0, 51, 0 }, /* Thundermon: Thundermon Core */
    { 0x88, 301, 0, 53, 0 }, /* MetalGreymon: MetalGreymon Core */
    { 0x89, 245, 1, 53, 0 }, /* Arukenimon: Arukenimon Core */
    { 0x8D, 92, 0, 28, 0 }, /* Kurisarimon: Kurisarimon Core */
    { 0x8F, 14, 1, 41, 0 }, /* Triceramon: Triceramon Core */
    { 0x90, 144, 0, 50, 0 }, /* Armageddemon: Armageddemon Core */
    { 0x91, 154, 0, 27, 0 }, /* Airdramon: Airdramon Core */
    { 0x92, 222, 2, 42, 0 }, /* Gekomon: Gekomon Core */
    { 0x93, 15, 0, 39, 0 }, /* BK Imperialdramon: BK Imperialdramon Core */
    { 0x94, 202, 0, 35, 0 }, /* Yanmamon: Yanmamon Core */
    { 0x95, 99, 0, 26, 0 }, /* Okuwamon: Okuwamon Core */
    { 0x96, 180, 0, 40, 0 }, /* Ebidramon: Ebidramon Core */
    { 0x97, 117, 0, 43, 0 }, /* Vikemon: Vikemon Core */
    { 0x98, 84, 0, 49, 0 }, /* Roachmon: Roachmon Core */
    { 0x99, 197, 0, 50, 0 }, /* Puppetmon: Puppetmon Core */
    { 0x9A, 118, 0, 49, 0 }, /* Lynxmon: Lynxmon Core */
    { 0x9B, 26, 0, 49, 0 }, /* Boltmon: Boltmon Core */
    { 0x9C, 21, 2, 50, 0 }, /* Vilemon: Vilemon Core */
    { 0x9D, 149, 0, 21, 0 }, /* Goburimon: Goburimon Core */
    { 0xA0, 64, 0, 22, 0 }, /* Divermon: Divermon Core */
    { 0xA2, 195, 1, 50, 0 }, /* Maildramon: Maildramon Core */
    { 0xA3, 170, 0, 44, 0 }, /* Raremon: Raremon Core */
    { 0xA5, 213, 0, 40, 0 }, /* Seadramon: Seadramon Core */
    { 0xA6, 31, 0, 50, 0 }, /* BK WarGreymon: BK WarGreymon Core */
    { 0xA7, 33, 0, 51, 0 }, /* Fujinmon: Fujinmon Core */
    { 0xA8, 94, 0, 42, 0 }, /* Blossomon: Blossomon Core */
    { 0xA9, 16, 1, 42, 0 }, /* Scorpiomon: Scorpiomon Core */
    { 0xAA, 123, 0, 40, 0 }, /* MetalSeadramon: MetalSeadramon Core */
    { 0xAB, 179, 0, 40, 0 }, /* MegaSeadramon: MegaSeadramon Core */
    { 0xAC, 87, 0, 41, 0 }, /* Megadramon: Megadramon Core */
    { 0xAD, 16, 0, 41, 0 }, /* Cherrymon: Cherrymon Core */
    { 0xFF, 315, 0, 44, 0 }, /* Bulbmon: Bulbmon Core */
};
const int def_hunt_target_count = 71;

const DefSuperRare def_super_rares[DEF_SUPER_COUNT] = {
    { 204, 1, 3, 480, 2, 0, 29, 34, 1500 }, /* Ogremon */
    { 305, 1, 29, 15000, 1, 0, -1, 124, 8000 }, /* MasterTyrannomon */
    { 305, 0, 37, 16363, 3, 0, -1, 358, 8727 }, /* Pharaohmon */
    { 306, 0, 40, 17727, 2, 0, -1, 425, 9454 }, /* Cherrymon */
    { 30, 0, 46, 19090, 4, 0, -1, 178, 10181 }, /* Knightmon */
    { 178, 0, 46, 20454, 4, 0, -1, 426, 10909 }, /* Whamon */
    { 308, 1, 47, 21818, 2, 1, -1, 70, 11636 }, /* WaruMonzaemon */
    { 318, 0, 47, 23181, 2, 1, -1, 447, 12363 }, /* BK Seraphimon */
    { 305, 2, 52, 24545, 4, 1, -1, 237, 13090 }, /* Kimeramon */
    { 307, 0, 56, 25909, 0, 0, -1, 193, 13818 }, /* Gryphonmon */
    { 312, 0, 57, 27272, 1, 1, -1, 456, 14545 }, /* GrapLeomon */
    { 307, 2, 59, 28636, 3, 1, -1, 356, 15272 }, /* HiAndromon */
    { 310, 0, 61, 30000, 0, 1, -1, 454, 16000 }, /* Kyukimon */
};

/* The sector of each world map area (0..0x2D; the second server's are the same), -1: none. */
const signed char def_hunt_sector_of[DEF_HUNT_AREAS] = {
    /* 0x00 */  0,  0,  0,  0,  0,  0, -1,  3,  0,  0,  0,  2,  4,  2,  1,  1,
    /* 0x10 */  3,  2,  4,  2,  2,  2,  1,  1,  1,  3,  3,  3,  3,  2,  1,  4,
    /* 0x20 */  2,  3,  3,  4,  4,  4,  3,  3,  4,  4,  4,  4,  4,  4,
};

const unsigned char def_hunt_commons[DEF_BESTIARY_SPECIES] = {
    /*   0 */ 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x02, 0x01,
    /*  16 */ 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x06, 0x02, 0x02, 0x02, 0x05, 0x03, 0x05, 0x09, 0x04, 0x03,
    /*  32 */ 0x02, 0x04, 0x03, 0x03, 0x03, 0x0B, 0x0C, 0x03, 0x03, 0x04, 0x05, 0x04, 0x04, 0x04, 0x04, 0x05,
    /*  48 */ 0x05, 0x0D, 0x0D, 0x05, 0x05, 0x0D, 0x05, 0x06, 0x05, 0x06, 0x07, 0x0A, 0x06, 0x08, 0x07, 0x07,
    /*  64 */ 0x0C, 0x06, 0x07, 0x07, 0x0C, 0x08, 0x0B, 0x07, 0x07, 0x07, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09,
    /*  80 */ 0x09, 0x0A, 0x0D, 0x0A, 0x0E, 0x0A, 0x0A, 0x0B, 0x0B, 0x0B, 0x0A, 0x0A, 0x0B, 0x0B, 0x0F, 0x0E,
    /*  96 */ 0x0C, 0x0C, 0x0B, 0x0C, 0x0C, 0x0F, 0x0D, 0x0E, 0x0F, 0x0D, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0x02,
    /* 112 */ 0x06, 0x02, 0x03, 0x04, 0x06, 0x06, 0x03, 0x09, 0x03, 0x08, 0x04, 0x08, 0x08, 0x07, 0x0E, 0x09,
    /* 128 */ 0x0D, 0x0E, 0x0D, 0x06, 0x08, 0x0A, 0x0A, 0x0C, 0x09, 0x0B, 0x0D, 0x06, 0x09, 0xFF, 0x08, 0xFF,
    /* 144 */ 0x0E, 0x0C, 0xFF, 0x0E, 0x0C, 0x0E, 0x0E, 0x0F, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 160 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 176 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 192 */ 0xFF,
};
