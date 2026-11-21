/* tests/port/definitive_quest.py's driver for port/game/definitive_quest.c (the Guide's rules), linked with the
 * generated texts. gamestate_data, gamestate_flags (a small set of flags that are on), the price table,
 * port_script_active and records_language are stand-ins. Prints "ok name" / "FAIL name" per case; exits 1 on a failure. */
#include <stdio.h>

#include "common.h"

#include "gamestate.h"

#include "definitive.h"

extern const unsigned char def_str_Q5[], def_str_Q6[], def_str_Q6b[], def_str_Q6c[], def_str_Q6d[], def_str_Q6e[],
    def_str_Q6f[], def_str_Q6g[], def_str_Q6h[], def_str_Q9b[], def_str_Q12b[], def_str_Q14b[], def_str_M0[];

GamestateData gamestate_data;
GamestateFlags gamestate_flags;
s32 gamestate_money_check[] = { 800, 1600, 2700, 4000, 6000, 8700, 11500, 17500, 24000, 32000 };
int port_script_active;
s32 records_language = 2; /* the disc's English set: the generated texts' def_lang reads it */
int def_story_cond(int cond) { (void)cond; return 0; } /* the story hooks (definitive_story.c): none holds the story */
int dw3_velvet_in_room(void) { return 0; }
void port_log(const char *fmt, ...) {
    (void)fmt;
}

static u16 on_flags[16];
static int on_count;

/* gamestate_get_flag's contract as the Guide reads it: 1 when the flag's state is `value` */
static s32 get_flag(u16 flag, u16 value) {
    int i, state = 0;

    for (i = 0; i < on_count; i++) {
        if (on_flags[i] == flag) {
            state = 1;
        }
    }
    return state == value;
}

static void set_on(u16 flag) {
    on_flags[on_count++] = flag;
}

static int failures;
static void check(const char *name, int ok) {
    if (!ok) {
        failures++;
    }
    printf("%s %s\n", ok ? "ok" : "FAIL", name);
}

static const unsigned char *main_title(int progress) {
    const DefQuest *q;

    gamestate_data.progress = progress;
    q = def_quest_main();
    return q != NULL ? q->title : NULL;
}

int main(void) {
    const DefQuest *missions[DEF_QUEST_MISSIONS];
    int p, ok, n, arg, i;

    gamestate_flags.get_flag = get_flag;
    for (ok = 1, p = 0; p <= 45; p++) {
        const DefQuest *q;

        gamestate_data.progress = p;
        q = def_quest_main();
        ok &= q != NULL && q->first <= p && p <= q->last && q->title != NULL && q->text != NULL;
    }
    check("every story step 0..45 has its main quest row", ok);

    on_count = 0;
    check("step 5: to East Station's terminal", main_title(5) == def_str_Q5);
    /* The Blue Card (step 6): each link of the chain sets the flag the next one asks for (WSTAG240, 360, 425, 410,
     * 420, 365, 410 again, 415), and the row follows the latest one held. */
    check("step 6, nothing yet: the Guilmon of the Asuka Inn", main_title(6) == def_str_Q6);
    set_on(0x1C44);
    check("his cousin named: the Forest Inn", main_title(6) == def_str_Q6b);
    set_on(0x1C45);
    check("sent on to Seiryu: Zephyr Tower", main_title(6) == def_str_Q6c);
    set_on(0x8000 | 399);
    check("the 8lue Card: to the terminal", main_title(6) == def_str_Q6d);
    set_on(0x1C46);
    set_on(0x1C50);
    check("refused by the terminal: the Guilmon in Seiryu City", main_title(6) == def_str_Q6e);
    set_on(0x1C47);
    check("the hideout told: the Forest Inn's basement", main_title(6) == def_str_Q6f);
    set_on(0x8000 | 398);
    check("the real Blue Card: register it and ride", main_title(6) == def_str_Q6g);
    set_on(0x4018);
    check("Bulbmon fell on the track: the ride's row", main_title(6) == def_str_Q6h);
    check("and step 5 never takes a step 6 row", main_title(5) == def_str_Q5);
    on_count = 0;
    set_on(0x8000 | 398);
    check("the Blue Card alone at step 6 (a save from any point after it): register it",
          main_title(6) == def_str_Q6g);
    on_count = 0;
    set_on(0x8000 | 21);
    check("step 9 with the Sepik Mask: return it", main_title(9) == def_str_Q9b);
    on_count = 0;
    set_on(0x8000 | 15);
    check("step 12 with the Agumon Suit: meet Kail", main_title(12) == def_str_Q12b);
    on_count = 0;
    set_on(0x8000 | 14);
    check("step 14 with the TNT Chip: behind the shrine", main_title(14) == def_str_Q14b);
    on_count = 0;

    gamestate_data.progress = 3;
    check("before the field menu (step 3): no mission", def_quest_missions(missions, DEF_QUEST_MISSIONS) == 0);
    gamestate_data.progress = 4;
    check("from step 4, nothing owned: the four missions", def_quest_missions(missions, DEF_QUEST_MISSIONS) == 4 &&
                                                                missions[0]->title == def_str_M0);
    set_on(0x8000 | 8);
    n = def_quest_missions(missions, DEF_QUEST_MISSIONS);
    for (ok = n == 3, i = 0; i < n; i++) {
        ok &= missions[i]->title != def_str_M0;
    }
    check("the El Dorado ID owned: its mission is gone", ok);
    check("the list's room is respected", def_quest_missions(missions, 2) == 2);
    on_count = 0;

    gamestate_data.progress = 4;
    for (ok = 1, i = 0; i < DEF_QUEST_AUCTIONS; i++) {
        ok &= def_auction_state(i) == DEF_AUCTION_SOON;
    }
    check("step 4: every auction still to come, none open", ok && def_auction_open() == -1);
    gamestate_data.progress = 5;
    check("step 5: the first auction is open, at its price",
          def_auction_state(0) == DEF_AUCTION_OPEN && def_auction_open() == 0 && def_auction_price(0) == 800 &&
              def_auction_state(1) == DEF_AUCTION_SOON);
    set_on(DEF_AUCTION_FLAG + 0);
    check("attended (either answer sets its flag): done, none open",
          def_auction_state(0) == DEF_AUCTION_DONE && def_auction_open() == -1);
    on_count = 0;
    gamestate_data.progress = 6;
    check("past its step unattended: missed", def_auction_state(0) == DEF_AUCTION_MISSED);
    check("out of range: soon, no price", def_auction_state(-1) == DEF_AUCTION_SOON &&
                                              def_auction_state(DEF_QUEST_AUCTIONS) == DEF_AUCTION_SOON &&
                                              def_auction_price(DEF_QUEST_AUCTIONS) == 0);
    for (ok = 1, i = 0; i < DEF_QUEST_AUCTIONS; i++) {
        gamestate_data.progress = def_auctions[i].progress;
        ok &= def_auction_open() == i && def_auction_price(i) == gamestate_money_check[def_auctions[i].money];
    }
    check("each auction opens alone at its own step", ok);

    gamestate_data.progress = 5;
    check("travel: from a town, for the auction open now",
          def_guide_can_travel(0, 0x205) && def_guide_can_travel(0, 0x21D));
    check("no travel from El Dorado's own maps, outside the world map's, or for another auction",
          !def_guide_can_travel(0, DEF_GUIDE_TRAVEL_MAP) && !def_guide_can_travel(0, 0x210) &&
              !def_guide_can_travel(0, 0x212) && !def_guide_can_travel(0, 0x1FF) &&
              !def_guide_can_travel(0, DEF_GUIDE_LAST_MAP) && !def_guide_can_travel(1, 0x205));
    gamestate_data.progress = def_auctions[4].progress; /* 16: Byakko City's trap */
    check("no travel out of Byakko City's trap", !def_guide_can_travel(4, 0x25F) && def_guide_can_travel(4, 0x205));

    def_quest_notice_reset();
    gamestate_data.progress = 4;
    n = def_quest_notice(&arg);
    check("a loaded game: its objective once", n == DEF_NOTICE_QUEST && arg == def_quest_main_index() &&
                                                   def_quest_notice(&arg) == DEF_NOTICE_NONE);
    gamestate_data.progress = 5;
    n = def_quest_notice(&arg);
    ok = n == DEF_NOTICE_QUEST && arg == def_quest_main_index();
    n = def_quest_notice(&arg);
    check("a new step: its objective, then the auction it opens, then nothing",
          ok && n == DEF_NOTICE_AUCTION && arg == 0 && def_quest_notice(&arg) == DEF_NOTICE_NONE);
    set_on(DEF_AUCTION_FLAG + 0);
    check("the auction attended: no notice", def_quest_notice(&arg) == DEF_NOTICE_NONE);
    gamestate_data.progress = 6;
    n = def_quest_notice(&arg);
    ok = n == DEF_NOTICE_QUEST && arg == def_quest_main_index() && def_quest_notice(&arg) == DEF_NOTICE_NONE;
    set_on(0x1C44);
    n = def_quest_notice(&arg);
    check("a stage flag that moves the step's row on: a notice",
          ok && n == DEF_NOTICE_QUEST && arg == def_quest_main_index() && def_quest_main()->title == def_str_Q6b);
    set_on(0x8000 | 399);
    n = def_quest_notice(&arg);
    check("a key item that changes the step's row: a notice", n == DEF_NOTICE_QUEST && arg == def_quest_main_index());
    on_count = 0;

    port_script_active = 1;
    def_quest_notice_reset();
    check("in a replay script: no Guide, no notice", !def_guide_active() && def_quest_notice(&arg) == DEF_NOTICE_NONE);
    port_script_active = 0;
    check("and back outside it", def_guide_active());

    return failures ? 1 : 0;
}
