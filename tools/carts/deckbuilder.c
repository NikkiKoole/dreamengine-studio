#include "studio.h"
#include <stddef.h>
#include <string.h>

// ── DECKBUILDER — a single Slay-the-Spire-grain run ───────────────
// You have ENERGY per turn and a HAND of cards. Click attacks to
// damage the foe, skills to gain BLOCK (a shield that soaks the next
// hit). The enemy shows its INTENT before it acts. Win a fight, pick
// one of three reward cards, walk the 3-node map, beat the boss.
//
//   • click a card to play it (targets the enemy)
//   • click END TURN to let the enemy act
//   • click a reward card to keep it (or SKIP)
//   • click the next map node to advance
//   keyboard: 1-5 play hand slots, ENTER end turn, SPACE continue
//
// All primitives — no sprite sheet. Mouse-driven.

// ── card definitions ──────────────────────────────────────────────
enum { KIND_ATTACK, KIND_SKILL };
typedef struct { const char *name; int cost, kind, value; int col; } CardDef;

enum {
    C_STRIKE, C_STRIKE2, C_BASH, C_DEFEND, C_DEFEND2,
    C_CLEAVE, C_BULWARK, C_HEAVY, C_DARTS,
    N_CARDDEFS
};
static const CardDef CARDS[N_CARDDEFS] = {
    /* STRIKE  */ { "STRIKE",  1, KIND_ATTACK, 6,  CLR_RED },
    /* STRIKE2 */ { "STRIKE",  1, KIND_ATTACK, 6,  CLR_RED },
    /* BASH    */ { "BASH",    2, KIND_ATTACK, 10, CLR_DARK_ORANGE },
    /* DEFEND  */ { "DEFEND",  1, KIND_SKILL,  5,  CLR_BLUE },
    /* DEFEND2 */ { "DEFEND",  1, KIND_SKILL,  5,  CLR_BLUE },
    /* CLEAVE  */ { "CLEAVE",  1, KIND_ATTACK, 8,  CLR_PINK },
    /* BULWARK */ { "BULWARK", 2, KIND_SKILL,  9,  CLR_TRUE_BLUE },
    /* HEAVY   */ { "HEAVY",   2, KIND_ATTACK, 14, CLR_DARK_RED },
    /* DARTS   */ { "DARTS",   0, KIND_ATTACK, 3,  CLR_ORANGE },
};
// cards offered as fight rewards
static const int REWARD_POOL[6] = { C_CLEAVE, C_BULWARK, C_HEAVY, C_DARTS, C_BASH, C_DEFEND };

// ── game state ────────────────────────────────────────────────────
enum { ST_MAP, ST_COMBAT, ST_REWARD, ST_WIN, ST_LOSE };
static int state = ST_MAP;

#define MAXDECK 40
static int deck[MAXDECK];    static int deck_n;     // draw pile
static int discard[MAXDECK]; static int discard_n;
static int hand[10];         static int hand_n;
static int full_deck[MAXDECK]; static int full_n;   // master list, rebuilt each combat

static int energy, max_energy = 3;
static int php, pmaxhp, pblock;

// enemy
enum { I_ATTACK, I_BLOCK, I_BUFF };
static int ehp, emaxhp, eblock, edmg;
static int intent_kind, intent_value;
static const char *ename;

// map / progression
static int node = 0;            // 0,1 = fights, 2 = boss
static bool node_done[3];
static int reward[3];

// juice
static float hit_flash;         // enemy hit white flash timer
static float dmg_pop_t;         // floating damage number
static int   dmg_pop_v;
static float plr_flash;         // player took damage red flash
static float enemy_wobble;      // when enemy acts
static bool  player_turn = true;
static float enemy_act_timer = 0;

static bool ready = false;

// ── deck plumbing ─────────────────────────────────────────────────
static void shuffle_into_deck(void) {
    // move discard into deck, shuffle deck
    for (int i = 0; i < discard_n; i++) deck[deck_n++] = discard[i];
    discard_n = 0;
    for (int i = deck_n - 1; i > 0; i--) {
        int j = rnd(i + 1);
        int t = deck[i]; deck[i] = deck[j]; deck[j] = t;
    }
}

static void start_full_deck(void) {
    deck_n = 0; discard_n = 0; hand_n = 0;
    for (int i = 0; i < full_n; i++) deck[deck_n++] = full_deck[i];
    // shuffle draw pile
    for (int i = deck_n - 1; i > 0; i--) {
        int j = rnd(i + 1);
        int t = deck[i]; deck[i] = deck[j]; deck[j] = t;
    }
}

static void draw_cards(int count) {
    for (int k = 0; k < count; k++) {
        if (hand_n >= 8) break;
        if (deck_n == 0) shuffle_into_deck();
        if (deck_n == 0) break;
        hand[hand_n++] = deck[--deck_n];
    }
}

// ── combat setup ──────────────────────────────────────────────────
static void roll_intent(void) {
    // boss buffs sometimes; grunts attack/block
    if (node == 2 && chance(30)) {
        intent_kind = I_BUFF; intent_value = 0;
    } else if (chance(28)) {
        intent_kind = I_BLOCK; intent_value = 6;
    } else {
        intent_kind = I_ATTACK;
        intent_value = edmg + rnd(4) - 1;
        if (intent_value < 1) intent_value = 1;
    }
}

static void start_combat(void) {
    if (node == 2) { ename = "DREAD KNIGHT"; emaxhp = 60; edmg = 12; }
    else if (node == 1) { ename = "JAW WORM"; emaxhp = 34; edmg = 9; }
    else { ename = "CULTIST"; emaxhp = 26; edmg = 6; }
    ehp = emaxhp; eblock = 0;
    pblock = 0; energy = max_energy;
    start_full_deck();
    draw_cards(5);
    roll_intent();
    player_turn = true;
    enemy_act_timer = 0;
    state = ST_COMBAT;
}

static void new_run(void) {
    full_n = 0;
    for (int i = 0; i < 5; i++) full_deck[full_n++] = (i < 1) ? C_BASH : C_STRIKE;
    full_deck[full_n++] = C_DEFEND;
    full_deck[full_n++] = C_DEFEND2;
    full_deck[full_n++] = C_DARTS;
    pmaxhp = 50; php = 50;
    node = 0;
    node_done[0] = node_done[1] = node_done[2] = false;
    state = ST_MAP;
}

static void prep_reward(void) {
    // three distinct cards from the pool
    int a = rnd(6);
    int b; do { b = rnd(6); } while (b == a);
    int c; do { c = rnd(6); } while (c == a || c == b);
    reward[0] = REWARD_POOL[a];
    reward[1] = REWARD_POOL[b];
    reward[2] = REWARD_POOL[c];
}

// ── playing a card ────────────────────────────────────────────────
static void deal_damage_to_enemy(int amt) {
    int d = amt;
    if (eblock > 0) {
        int absorbed = (d < eblock) ? d : eblock;
        eblock -= absorbed; d -= absorbed;
    }
    if (d > 0) { ehp -= d; if (ehp < 0) ehp = 0; }
    hit_flash = 0.18f;
    dmg_pop_t = 0.7f; dmg_pop_v = amt;
    sfx(-1);
    hit(40, INSTR_NOISE, 4, 70);
    note(64, INSTR_TRI, 3);
    shake(2);
}

static void play_card(int slot) {
    if (slot < 0 || slot >= hand_n) return;
    const CardDef *cd = &CARDS[hand[slot]];
    if (cd->cost > energy) { note(48, INSTR_SQUARE, 2); return; }
    energy -= cd->cost;
    if (cd->kind == KIND_ATTACK) {
        deal_damage_to_enemy(cd->value);
    } else {
        pblock += cd->value;
        note(72, 5, 3);            // shield-up pluck
    }
    // discard the card
    discard[discard_n++] = hand[slot];
    for (int i = slot; i < hand_n - 1; i++) hand[i] = hand[i + 1];
    hand_n--;
    if (ehp <= 0) {
        // victory
        fade(0);
        node_done[node] = true;
        if (node == 2) { state = ST_WIN; save_int("runsWon", load_int("runsWon", 0) + 1); }
        else { prep_reward(); state = ST_REWARD; }
        strum(60, CHORD_MAJ, INSTR_TRI, 4, 45);
    }
}

// ── ending the turn / enemy acts ──────────────────────────────────
static void end_turn(void) {
    // discard hand
    for (int i = 0; i < hand_n; i++) discard[discard_n++] = hand[i];
    hand_n = 0;
    player_turn = false;
    enemy_act_timer = 0.55f;       // brief telegraph before the enemy resolves
}

static void enemy_resolve(void) {
    if (intent_kind == I_ATTACK) {
        int d = intent_value;
        if (pblock > 0) {
            int absorbed = (d < pblock) ? d : pblock;
            pblock -= absorbed; d -= absorbed;
        }
        if (d > 0) {
            php -= d; if (php < 0) php = 0;
            plr_flash = 0.3f; shake(5);
            hit(34, INSTR_NOISE, 5, 120);
        } else {
            note(80, 5, 2);        // fully blocked — tink
        }
    } else if (intent_kind == I_BLOCK) {
        eblock += intent_value;
        note(72, INSTR_SINE, 2);
    } else { // buff
        edmg += 3;
        note(55, INSTR_SAW, 3);
    }
    enemy_wobble = 0.3f;
    if (php <= 0) { state = ST_LOSE; note(36, INSTR_SAW, 5); return; }
    // start player's next turn
    pblock = 0;                    // block evaporates each turn
    energy = max_energy;
    draw_cards(5);
    roll_intent();
    player_turn = true;
}

// ── layout constants ──────────────────────────────────────────────
#define CARD_W 44
#define CARD_H 60
#define HAND_Y 134
#define ENDTURN_X 250
#define ENDTURN_Y 110
#define ENDTURN_W 64
#define ENDTURN_H 20

static int card_x(int i) {
    int total = hand_n * (CARD_W + 4) - 4;
    int start = SCREEN_W / 2 - total / 2;
    return start + i * (CARD_W + 4);
}

static bool clicked_box(int x, int y, int w, int h) {
    return mouse_pressed(MOUSE_LEFT) && point_in_box(mouse_x(), mouse_y(), x, y, w, h);
}

// ── update ────────────────────────────────────────────────────────
void init(void) {
    instrument(5, INSTR_TRI, 2, 60, 3, 80);    // pluck for skills/shields
    new_run();
    ready = true;
}

void update(void) {
    if (!ready) { init(); }
    float d = dt();
    if (hit_flash > 0)    hit_flash    -= d;
    if (plr_flash > 0)    plr_flash    -= d;
    if (dmg_pop_t > 0)    dmg_pop_t    -= d;
    if (enemy_wobble > 0) enemy_wobble -= d;

    if (state == ST_MAP) {
        // click the next reachable node
        for (int i = 0; i < 3; i++) {
            int nx = 70 + i * 95, ny = 100;
            bool reachable = (i == node) && !node_done[i];
            if (reachable && clicked_box(nx - 22, ny - 22, 44, 44)) {
                start_combat();
                note(67, INSTR_SINE, 3);
            }
        }
    }
    else if (state == ST_COMBAT) {
        if (player_turn) {
            // play cards by click or number key
            for (int i = 0; i < hand_n; i++) {
                int cx = card_x(i);
                if (clicked_box(cx, HAND_Y, CARD_W, CARD_H)) { play_card(i); break; }
            }
            for (int k = 0; k < 5; k++) if (keyp('1' + k)) { play_card(k); break; }
            // end turn
            if (clicked_box(ENDTURN_X, ENDTURN_Y, ENDTURN_W, ENDTURN_H) || keyp(KEY_ENTER))
                end_turn();
        } else {
            enemy_act_timer -= d;
            if (enemy_act_timer <= 0) enemy_resolve();
        }
    }
    else if (state == ST_REWARD) {
        for (int i = 0; i < 3; i++) {
            int cx = 50 + i * 80, cy = 70;
            if (clicked_box(cx, cy, CARD_W, CARD_H)) {
                if (full_n < MAXDECK) full_deck[full_n++] = reward[i];
                note(72, INSTR_TRI, 4);
                node++;
                state = ST_MAP;
            }
        }
        // skip
        if (clicked_box(SCREEN_W/2 - 30, 145, 60, 18) || keyp(KEY_SPACE)) {
            node++;
            state = ST_MAP;
        }
        if (node >= 3) state = ST_WIN;
    }
    else if (state == ST_WIN || state == ST_LOSE) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE) || keyp(KEY_ENTER)) {
            new_run();
        }
    }
}

// ── drawing ───────────────────────────────────────────────────────
static void draw_pip_bar(int x, int y, int w, int cur, int mx, int fillc, const char *lbl) {
    bar(x, y, w, 10, mx > 0 ? (float)cur / mx : 0, fillc, CLR_DARKER_GREY);
    rect(x, y, w, 10, CLR_DARK_GREY);
    print(str("%s %d/%d", lbl, cur, mx), x + 3, y + 1, CLR_WHITE);
}

static void draw_shield(int cx, int cy, int amt, int col) {
    // little shield badge with the block number
    trifill(cx - 9, cy - 8, cx + 9, cy - 8, cx, cy + 9, col);
    rectfill(cx - 9, cy - 9, 18, 5, col);
    rect(cx - 10, cy - 10, 20, 6, CLR_WHITE);
    char *s = (char*)str("%d", amt);
    print(s, cx - text_width(s)/2, cy - 6, CLR_WHITE);
}

static void draw_card_face(int x, int y, int type, bool playable, bool lift) {
    const CardDef *cd = &CARDS[type];
    int yy = y - (lift ? 8 : 0);
    int frame = playable ? CLR_WHITE : CLR_DARK_GREY;
    rectfill(x, yy, CARD_W, CARD_H, CLR_DARKER_PURPLE);
    rect(x, yy, CARD_W, CARD_H, frame);
    // header band tinted by card color
    rectfill(x + 1, yy + 1, CARD_W - 2, 12, cd->col);
    print(cd->name, x + 3, yy + 3, CLR_BLACK);
    // energy cost orb
    circfill(x + 7, yy + 22, 6, CLR_YELLOW);
    circ(x + 7, yy + 22, 6, CLR_ORANGE);
    print(str("%d", cd->cost), x + 5, yy + 19, CLR_BLACK);
    // big value + kind glyph
    if (cd->kind == KIND_ATTACK) {
        // sword
        line(x + 30, yy + 18, x + 30, yy + 30, CLR_LIGHT_GREY);
        line(x + 27, yy + 28, x + 33, yy + 28, CLR_LIGHT_GREY);
        print(str("%d", cd->value), x + 18, yy + 36, CLR_RED);
        print("DMG", x + 12, yy + 48, CLR_LIGHT_GREY);
    } else {
        trifill(x + 24, yy + 17, x + 36, yy + 17, x + 30, yy + 30, CLR_BLUE);
        print(str("%d", cd->value), x + 18, yy + 36, CLR_BLUE);
        print("BLK", x + 12, yy + 48, CLR_LIGHT_GREY);
    }
}

static void draw_enemy(void) {
    int ex = 240, ey = 50;
    int wob = (enemy_wobble > 0) ? (int)(sin_deg(now() * 1400) * 3) : 0;
    if (hit_flash > 0) pal(CLR_DARK_PURPLE, CLR_WHITE);
    // body
    int body = (node == 2) ? CLR_DARK_RED : CLR_DARK_PURPLE;
    if (hit_flash > 0) body = CLR_WHITE;
    ovalfill(ex + wob, ey + 20, 22, 26, body);
    circfill(ex + wob, ey - 4, 16, body);
    // eyes
    circfill(ex + wob - 6, ey - 6, 3, CLR_YELLOW);
    circfill(ex + wob + 6, ey - 6, 3, CLR_YELLOW);
    circfill(ex + wob - 6, ey - 6, 1, CLR_BLACK);
    circfill(ex + wob + 6, ey - 6, 1, CLR_BLACK);
    if (node == 2) { // horns for the boss
        trifill(ex + wob - 14, ey - 14, ex + wob - 8, ey - 8, ex + wob - 18, ey - 4, CLR_LIGHT_GREY);
        trifill(ex + wob + 14, ey - 14, ex + wob + 8, ey - 8, ex + wob + 18, ey - 4, CLR_LIGHT_GREY);
    }
    pal_reset();

    // enemy hp bar
    draw_pip_bar(ex - 30, ey + 50, 60, ehp, emaxhp, CLR_RED, "HP");
    if (eblock > 0) draw_shield(ex - 38, ey + 55, eblock, CLR_TRUE_BLUE);
    print_centered(ename, 8, CLR_LIGHT_PEACH);

    // INTENT bubble above the enemy
    int ix = ex, iy = ey - 30;
    rectfill(ix - 26, iy - 10, 52, 18, CLR_DARKER_BLUE);
    rect(ix - 26, iy - 10, 52, 18, CLR_YELLOW);
    if (intent_kind == I_ATTACK) {
        // sword + number = "attacks for N"
        line(ix - 14, iy - 6, ix - 14, iy + 4, CLR_RED);
        line(ix - 17, iy + 2, ix - 11, iy + 2, CLR_RED);
        print(str("%d", intent_value), ix - 4, iy - 6, CLR_RED);
    } else if (intent_kind == I_BLOCK) {
        trifill(ix - 16, iy - 7, ix - 6, iy - 7, ix - 11, iy + 4, CLR_BLUE);
        print(str("%d", intent_value), ix - 2, iy - 6, CLR_BLUE);
    } else {
        print("BUFF", ix - 14, iy - 6, CLR_ORANGE);
    }
}

static void draw_combat(void) {
    cls(CLR_DARKER_PURPLE);
    rectfill(0, 0, SCREEN_W, 100, CLR_BROWNISH_BLACK);
    // floor line
    line(0, 100, SCREEN_W, 100, CLR_DARKER_GREY);

    if (plr_flash > 0) { fillp(FILL_CHECKER, -1); rectfill(0,0,SCREEN_W,SCREEN_H,CLR_RED); fillp_reset(); }

    draw_enemy();

    // floating damage number
    if (dmg_pop_t > 0) {
        float t = 1 - dmg_pop_t / 0.7f;
        int fy = 30 - (int)(t * 18);
        print_scaled(str("%d", dmg_pop_v), 232, fy, CLR_YELLOW, 2);
    }

    // ── player panel ──
    rectfill(0, 102, SCREEN_W, SCREEN_H - 102, CLR_DARKER_BLUE);
    rect(0, 102, SCREEN_W, SCREEN_H - 102, CLR_DARK_BLUE);

    // player HP + block + energy
    draw_pip_bar(6, 108, 90, php, pmaxhp, CLR_GREEN, "HP");
    if (pblock > 0) draw_shield(110, 113, pblock, CLR_TRUE_BLUE);
    // energy orb
    int eox = 150, eoy = 113;
    float pulse = 1 + 0.12f * sin_deg(now() * 300);
    int er = (int)(9 * pulse);
    circfill(eox, eoy, er, energy > 0 ? CLR_YELLOW : CLR_DARK_GREY);
    circ(eox, eoy, er, CLR_ORANGE);
    print(str("%d/%d", energy, max_energy), eox - 8, eoy - 3, CLR_BLACK);

    // draw / discard counts
    print(str("DRAW %d", deck_n), 6, 122, CLR_LIGHT_GREY);
    print(str("DISC %d", discard_n), 60, 122, CLR_LIGHT_GREY);

    // END TURN button
    bool can_end = player_turn;
    rectfill(ENDTURN_X, ENDTURN_Y, ENDTURN_W, ENDTURN_H, can_end ? CLR_DARK_GREEN : CLR_DARK_GREY);
    rect(ENDTURN_X, ENDTURN_Y, ENDTURN_W, ENDTURN_H, CLR_WHITE);
    print("END TURN", ENDTURN_X + 6, ENDTURN_Y + 6, CLR_WHITE);

    // hand
    int mx = mouse_x(), my = mouse_y();
    for (int i = 0; i < hand_n; i++) {
        int cx = card_x(i);
        bool hover = player_turn && point_in_box(mx, my, cx, HAND_Y - 8, CARD_W, CARD_H + 8);
        bool playable = player_turn && CARDS[hand[i]].cost <= energy;
        draw_card_face(cx, HAND_Y, hand[i], playable, hover);
    }

    if (!player_turn) print_centered("enemy turn...", 104, CLR_YELLOW);
}

static void draw_map(void) {
    cls(CLR_BROWNISH_BLACK);
    print_centered("THE DUNGEON", 16, CLR_LIGHT_PEACH);
    print_centered(str("runs cleared: %d", load_int("runsWon", 0)), 28, CLR_DARK_GREY);

    const char *labels[3] = { "FIGHT", "FIGHT", "BOSS" };
    // path
    for (int i = 0; i < 2; i++) {
        int x1 = 70 + i * 95, x2 = 70 + (i + 1) * 95;
        line(x1, 100, x2, 100, CLR_DARK_GREY);
    }
    for (int i = 0; i < 3; i++) {
        int nx = 70 + i * 95, ny = 100;
        bool done = node_done[i];
        bool here = (i == node) && !done;
        int col = done ? CLR_DARK_GREEN : here ? CLR_YELLOW : CLR_DARK_GREY;
        if (here && blink(20)) col = CLR_ORANGE;
        circfill(nx, ny, 20, i == 2 ? CLR_DARK_RED : CLR_DARKER_BLUE);
        circ(nx, ny, 20, col);
        if (i == 2) print("BOSS", nx - 14, ny - 3, col);
        else print(done ? "DONE" : "FIGHT", nx - (done?12:14), ny - 3, col);
        if (here) print_centered("click to enter", 140, CLR_WHITE);
    }
}

static void draw_reward(void) {
    cls(CLR_DARKER_PURPLE);
    print_centered("VICTORY! pick a card", 24, CLR_YELLOW);
    int mx = mouse_x(), my = mouse_y();
    for (int i = 0; i < 3; i++) {
        int cx = 50 + i * 80, cy = 70;
        bool hover = point_in_box(mx, my, cx, cy - 8, CARD_W, CARD_H + 8);
        draw_card_face(cx, cy, reward[i], true, hover);
    }
    rectfill(SCREEN_W/2 - 30, 145, 60, 18, CLR_DARK_GREY);
    rect(SCREEN_W/2 - 30, 145, 60, 18, CLR_WHITE);
    print("SKIP", SCREEN_W/2 - 14, 150, CLR_WHITE);
}

void draw(void) {
    if (!ready) { init(); }
    switch (state) {
        case ST_MAP:    draw_map(); break;
        case ST_COMBAT: draw_combat(); break;
        case ST_REWARD: draw_reward(); break;
        case ST_WIN:
            cls(CLR_DARK_GREEN);
            print_scaled("RUN CLEARED", SCREEN_W/2 - 88, 70, CLR_YELLOW, 2);
            print_centered("the dread knight falls.", 100, CLR_WHITE);
            print_centered(str("runs cleared: %d", load_int("runsWon", 0)), 120, CLR_LIGHT_PEACH);
            print_centered("click to start a new run", 150, CLR_LIGHT_GREY);
            break;
        case ST_LOSE:
            cls(CLR_DARK_RED);
            fade(0.2f);
            print_scaled("YOU DIED", SCREEN_W/2 - 64, 80, CLR_WHITE, 2);
            print_centered("click to try again", 130, CLR_LIGHT_GREY);
            break;
    }
}
