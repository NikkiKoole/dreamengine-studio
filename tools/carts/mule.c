/* de:meta
{
  "title": "M.U.L.E.",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "save-load-persistence"
  ],
  "lineage": "Homage to M.U.L.E. (1983, Ozark Softscape); novel in distilling the full economic loop — install, produce, dynamic auction price — into a single-player one-round cart.",
  "genre": "tycoon",
  "homage": "M.U.L.E. (1983)",
  "description": "One full round of the 1983 economic classic on planet Irata. The whole loop: buy a MULE at the store, outfit it for food / energy / smithore, then race it out to a land plot before it bolts (a runaway timer). Installed MULEs harvest goods based on their terrain, and then you sell your ore in an auction where every sale floods the market and drops the price — so sell while it is high. Supply and demand, made playable. Left/right choose, Z acts, X skips; net worth is scored and your best is saved.",
  "todo": [
    "Doesn't feel like the original M.U.L.E. (was it really a rhythm game?) — rethink.",
    "ui-audit: the \"Z: buy MULE ($100) / X: skip to harvest\" hint runs off the right edge."
  ]
}
de:meta */
#include "studio.h"

// M.U.L.E. — one full round of the 1983 economic classic, on planet Irata.
//
// The loop, in five beats:
//   STORE    buy a MULE (a multi-use labour robot)
//   OUTFIT   fit it to make FOOD, ENERGY or SMITHORE (ore)
//   INSTALL  race it out to a plot before the ornery thing bolts (a timer)
//   PRODUCE  each installed MULE yields goods based on its land
//   AUCTION  sell your ore to the store — but every sale floods the market and
//            drops the price, so sell while it's high. That's supply & demand.
//
//   left/right: choose      Z: act / buy / sell      X: skip
//   (after the round, Z plays again)

#define COLS 5
#define ROWS 3
#define NPLOT (COLS * ROWS)
#define TOWN  7                       // centre plot
#define MX 35
#define MY 26
#define PW 50
#define PH 34

enum { T_PLAIN, T_RIVER, T_MTN };
enum { G_FOOD, G_ENERGY, G_ORE, G_NONE };
enum { PH_STORE, PH_OUTFIT, PH_INSTALL, PH_PRODUCE, PH_AUCTION, PH_DONE };

static const char *GNAME[3] = { "FOOD", "ENERGY", "SMITHORE" };
static const int   GCOST[3] = { 25, 50, 75 };
static const int   GCOL[3]  = { CLR_GREEN, CLR_RED, CLR_LIGHT_GREY };

static int  terrain[NPLOT], mule[NPLOT];   // mule = G_NONE or a good
static int  money, food, energy, ore;
static int  phase, outfit, cursor, best;
static int  itimer;                        // install runaway timer
static int  gain_f, gain_e, gain_o;        // last production
static float price, basep; static int atimer;
static const char *flash = "";

// ── background music: an upbeat, walking-bass chiptune in the spirit of the
//    M.U.L.E. theme (original arrangement, not the copyrighted recording).
//    Driven off the synth beat clock, 32 sixteenth-notes per loop. ──
static const int MEL[32] = {
    67, 0, 72, 0, 71, 0, 72, 0, 74, 0, 72, 0, 69, 71, 67, 0,
    65, 0, 67, 0, 69, 0, 71, 72, 72, 0, 67, 0, 64, 0, 67, 0,
};
static const int BASS[32] = {   // root on the beat, octave bounce on the "and"
    36, 0, 48, 0, 43, 0, 55, 0, 41, 0, 53, 0, 43, 0, 55, 0,
    36, 0, 48, 0, 45, 0, 57, 0, 41, 0, 53, 0, 43, 0, 55, 0,
};
static int last_step = -1;

static void play_music(void) {
    int s16 = beat() * 4 + (int)(beat_pos() * 4);
    if (s16 == last_step) return;
    last_step = s16;
    int s = s16 % 32;
    if (MEL[s])  hit(MEL[s],  INSTR_SQUARE, 3, 100);   // lead
    if (BASS[s]) hit(BASS[s], INSTR_TRI,    4, 90);    // walking bass
    if (s % 8 == 0) hit(81,   INSTR_NOISE,  1, 18);    // soft tick on the downbeat
}

static void new_round(void) {
    money = 1000; food = 4; energy = 4; ore = 2;
    for (int i = 0; i < NPLOT; i++) {
        mule[i] = G_NONE;
        terrain[i] = i == TOWN ? 0 : (i % COLS == 1) ? T_RIVER : rnd(2) ? T_PLAIN : T_MTN;
    }
    mule[2] = G_FOOD; mule[11] = G_ORE;     // a couple from "last round" so the colony looks lived-in
    phase = PH_STORE; outfit = G_FOOD; flash = "";
}

void init(void) { best = load(0); bpm(132); new_round(); }

static int yield_of(int plot, int good) {
    int t = terrain[plot];
    if (good == G_FOOD)   return 2 + (t == T_RIVER ? 3 : t == T_PLAIN ? 1 : 0);
    if (good == G_ENERGY) return 3;
    return 2 + (t == T_MTN ? 3 : 0);        // ore
}

static int next_plot(int from, int step) {
    int p = from;
    for (int k = 0; k < NPLOT; k++) {
        p = (p + step + NPLOT) % NPLOT;
        if (p != TOWN && mule[p] == G_NONE) return p;
    }
    return from;
}

void update(void) {
    play_music();

    if (phase == PH_STORE) {
        if (btnp(0, BTN_A) && money >= 100) { money -= 100; phase = PH_OUTFIT; note(64, INSTR_SQUARE, 4); }
        if (btnp(0, BTN_B)) { phase = PH_PRODUCE; gain_f = gain_e = gain_o = -1; }

    } else if (phase == PH_OUTFIT) {
        if (btnp(0, BTN_LEFT))  outfit = (outfit + 2) % 3;
        if (btnp(0, BTN_RIGHT)) outfit = (outfit + 1) % 3;
        if (btnp(0, BTN_A) && money >= GCOST[outfit]) {
            money -= GCOST[outfit];
            phase = PH_INSTALL; itimer = 300; cursor = next_plot(TOWN, 1);
            note(67, INSTR_SQUARE, 4);
        }

    } else if (phase == PH_INSTALL) {
        if (btnp(0, BTN_LEFT))  cursor = next_plot(cursor, -1);
        if (btnp(0, BTN_RIGHT)) cursor = next_plot(cursor, 1);
        if (--itimer <= 0) { flash = "the MULE bolted! lost it."; phase = PH_PRODUCE; gain_f = gain_e = gain_o = -1; hit(38, INSTR_NOISE, 5, 200); }
        if (btnp(0, BTN_A) && cursor != TOWN && mule[cursor] == G_NONE) {
            mule[cursor] = outfit; flash = "MULE installed!";
            hit(72, INSTR_SQUARE, 4, 80); hit(79, INSTR_SQUARE, 4, 120);
            phase = PH_PRODUCE; gain_f = gain_e = gain_o = -1;
        }

    } else if (phase == PH_PRODUCE) {
        if (gain_f < 0) {                     // tally production once on entry
            gain_f = gain_e = gain_o = 0;
            for (int i = 0; i < NPLOT; i++) if (mule[i] != G_NONE) {
                int y = yield_of(i, mule[i]);
                if (mule[i] == G_FOOD) gain_f += y; else if (mule[i] == G_ENERGY) gain_e += y; else gain_o += y;
            }
            food += gain_f; energy += gain_e; ore += gain_o;
        }
        if (btnp(0, BTN_A)) {
            if (ore > 0) { phase = PH_AUCTION; basep = 50; price = 50; atimer = 600; }
            else phase = PH_DONE;
        }

    } else if (phase == PH_AUCTION) {
        price = basep + 14 * sin_deg(frame() * 4) + (noise(frame() * 0.1f) - 0.5f) * 16;
        if (price < 5) price = 5;
        if (btnp(0, BTN_A) && ore > 0) {
            money += (int)price; ore--; basep = max(8.0f, basep - 4);
            note(60 + (int)(price / 6), INSTR_SINE, 4);
        }
        if (--atimer <= 0 || ore == 0) phase = PH_DONE;

    } else if (phase == PH_DONE) {
        if (btnp(0, BTN_A)) new_round();
    }
}

static void draw_mule(int cx, int cy, int good) {
    rectfill(cx - 4, cy - 2, 8, 5, CLR_DARK_BROWN);          // body
    line(cx - 3, cy + 3, cx - 3, cy + 6, CLR_DARK_BROWN);    // legs
    line(cx + 3, cy + 3, cx + 3, cy + 6, CLR_DARK_BROWN);
    line(cx + 4, cy - 1, cx + 7, cy - 2, CLR_DARK_BROWN);    // neck/head
    rectfill(cx - 3, cy - 6, 6, 4, GCOL[good]);             // cargo (colour = good)
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // ── HUD ──
    print(str("$%d", money), 4, 3, CLR_YELLOW);
    print(str("F:%d", food), 92, 3, CLR_GREEN);
    print(str("E:%d", energy), 140, 3, CLR_RED);
    print(str("ORE:%d", ore), 196, 3, CLR_LIGHT_GREY);
    print_right(str("best $%d", best), SCREEN_W - 4, 3, CLR_DARK_GREY);

    // ── colony map ──
    for (int i = 0; i < NPLOT; i++) {
        int x = MX + (i % COLS) * PW, y = MY + (i / COLS) * PH;
        int tc = i == TOWN ? CLR_MEDIUM_GREY : terrain[i] == T_RIVER ? CLR_TRUE_BLUE
               : terrain[i] == T_MTN ? CLR_BROWN : CLR_DARK_GREEN;
        rectfill(x + 1, y + 1, PW - 2, PH - 2, tc);
        rect(x, y, PW, PH, CLR_BLACK);
        if (i == TOWN) { rectfill(x + PW/2 - 8, y + PH/2 - 6, 16, 12, CLR_LIGHT_GREY); print("TOWN", x + PW/2 - 14, y + PH - 9, CLR_BLACK); }
        if (mule[i] != G_NONE) draw_mule(x + PW / 2, y + PH / 2, mule[i]);
        if (phase == PH_INSTALL && i == cursor) rect(x + 2, y + 2, PW - 4, PH - 4, CLR_YELLOW);
    }

    // ── per-phase panel ──
    int py = 134;
    if (phase == PH_STORE) {
        print("OUTFITTING STORE", 4, py, CLR_WHITE);
        print("Buy a MULE to work the land.", 4, py + 14, CLR_LIGHT_GREY);
        print(money >= 100 ? "Z: buy MULE ($100)    X: skip to harvest" : "not enough credits!", 4, py + 30, money >= 100 ? CLR_GREEN : CLR_RED);
    } else if (phase == PH_OUTFIT) {
        print("OUTFIT THE MULE  (left/right to choose)", 4, py, CLR_WHITE);
        for (int g = 0; g < 3; g++) {
            int x = 8 + g * 104;
            bool sel = g == outfit;
            rectfill(x, py + 14, 96, 22, sel ? CLR_DARK_PURPLE : CLR_DARKER_PURPLE);
            if (sel) rect(x, py + 14, 96, 22, CLR_YELLOW);
            print(GNAME[g], x + 6, py + 18, GCOL[g]);
            print(str("$%d", GCOST[g]), x + 6, py + 27, CLR_LIGHT_GREY);
        }
        print("Z: confirm", 4, py + 40, CLR_GREEN);
    } else if (phase == PH_INSTALL) {
        print(str("INSTALL the %s MULE — pick a plot, Z to plant it", GNAME[outfit]), 4, py, CLR_WHITE);
        rectfill(4, py + 16, 200, 8, CLR_DARKER_GREY);
        rectfill(4, py + 16, 200 * itimer / 300, 8, itimer < 100 ? CLR_RED : CLR_ORANGE);
        print("the MULE is struggling to break free!", 4, py + 30, CLR_LIGHT_GREY);
    } else if (phase == PH_PRODUCE) {
        print("HARVEST", 4, py, CLR_WHITE);
        if (flash[0]) print(flash, 4, py + 13, CLR_YELLOW);
        print(str("produced:  +%d food   +%d energy   +%d ore", gain_f, gain_e, gain_o), 4, py + 27, CLR_LIGHT_GREY);
        print("Z: to the auction", 4, py + 41, CLR_GREEN);
    } else if (phase == PH_AUCTION) {
        print("SMITHORE AUCTION — Z to sell one (price falls as you do)", 4, py, CLR_WHITE);
        // price scale
        int barx = 8, barw = 240, scaleY = py + 22, sh = 26;
        rect(barx, scaleY, barw, sh, CLR_DARK_GREY);
        int px = barx + (int)(price / 90.0f * barw); if (px > barx + barw) px = barx + barw;
        rectfill(barx, scaleY, px - barx, sh, price > 55 ? CLR_GREEN : price > 30 ? CLR_YELLOW : CLR_RED);
        print(str("$%d /ore", (int)price), barx + barw + 6, scaleY + 9, CLR_WHITE);
        print(str("ore left: %d    time: %d", ore, atimer / 60), 8, py + 52, CLR_LIGHT_GREY);
    } else {
        int worth = money + food * 4 + energy * 4 + ore * 20;
        for (int i = 0; i < NPLOT; i++) if (mule[i] != G_NONE) worth += 60;
        if (worth > best) { best = worth; save(0, best); }
        print_centered("ROUND COMPLETE", SCREEN_W/2, py, CLR_YELLOW);
        print_centered(str("net worth:  $%d", worth), SCREEN_W/2, py + 14, CLR_WHITE);
        print_centered("Z: play another round", SCREEN_W/2, py + 34, CLR_LIGHT_GREY);
    }
}
