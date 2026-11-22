/* Super Hard Mode: the Demon Bell, a key item the Velvet Room's red Digimon hands over (and takes back).
 *
 * The mode is nothing but the bag: while the player holds the Bell (item DEF_SUPER_ITEM, one of the Japanese region
 * tags the US disc never hands out, renamed in tools/text_override), every enemy has more HP and more Speed, and a
 * won battle pays more Bits and more Hunt Materials. Speed is a signed short that the fight's formulas read freely, so
 * it may pass the party's 999 cap: that is how an enemy gets a second action before the party's turn (the Inspect's
 * "Enemy x2"). Taking the Bell back (the NPC again) ends the mode, and the bag already goes to the memory card, so
 * there is no flag of ours to save.
 *
 * Off in a replay script (def_on): the disc's numbers. */
#include "common.h"
#include "gamestate.h"

#include "definitive.h"

extern int def_on(void); /* port.h declares it under PC_PORT only; the host test builds without it */

int def_super_on(void) {
    return def_on() && gamestate_data.items[DEF_SUPER_ITEM] > 0;
}

static int scale(int v, int pct) {
    return (int)((long)v * pct / 100);
}

int def_super_hp(int hp) {
    int v;

    if (!def_super_on()) {
        return hp;
    }
    v = scale(hp, DEF_SUPER_HP_PCT);
    return v > 0x7FFF ? 0x7FFF : v;
}

int def_super_speed(int speed) {
    int v;

    if (!def_super_on()) {
        return speed;
    }
    v = scale(speed, DEF_SUPER_SPEED_PCT);
    return v > 0x7FFF ? 0x7FFF : v;
}

int def_super_reward(int amount) {
    return def_super_on() ? scale(amount, DEF_SUPER_REWARD_PCT) : amount;
}

int def_super_materials(int amount) {
    return def_super_on() ? scale(amount, DEF_SUPER_MAT_PCT) : amount;
}

int def_super_bell_damage(int side, int damage) {
    int scaled;

    if (side != 0 || damage <= 0 || !def_super_on() || def_finale_golden_mode()) {
        return damage;
    }
    scaled = scale(damage, DEF_SUPER_PLAYER_DMG_PCT);
    return scaled < 1 ? 1 : scaled;
}

/* The Bell changes hands: 1 when the player now holds it (the NPC gave it), 0 when it was taken back. */
int def_super_toggle(void) {
    int on = !(gamestate_data.items[DEF_SUPER_ITEM] > 0);

    gamestate_data.items[DEF_SUPER_ITEM] = on ? 1 : 0;
    return on;
}
