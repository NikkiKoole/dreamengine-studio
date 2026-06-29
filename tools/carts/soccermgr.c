/* de:meta
{
  "title": "Football Manager",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "state-machine",
    "inventory-system"
  ],
  "lineage": "Inspired by FC Mobile / Football Manager lite; novel in combining a drag-to-arrange formation pitch, a procedurally refreshing transfer market, and a Poisson-ish strength-ratio match simulator feeding a league table.",
  "genre": "sports",
  "homage": "Football Manager (1982)",
  "description": "An FC-Mobile-style manager mode: run a club's starting XI on a drag-to-arrange formation pitch, with a bench of spares, switchable 4-4-2 / 4-3-3 shapes, and an out-of-position penalty that makes your shape matter. Wheel and deal in a transfer market against a cash budget — sign a face, sell the deadwood — then hit PLAY MATCH and the engine auto-sims a scoreline from your XI's strength versus the opponent's, ticking the goals up on a scoreboard before slamming home WIN/DRAW/DEFEAT. Other clubs play their own fixtures and the 6-team league table re-sorts live on points and goal difference across a 10-round season; finish top to be champions. Drag players between the pitch and the bench, click 4-4-2 / 4-3-3 to switch shape, click a price to BUY or a value to SELL in the market, and click PLAY MATCH (TAB cycles SQUAD/MARKET/LEAGUE, ENTER plays)."
}
de:meta */
#include "studio.h"
#include <stddef.h>

// SOCCERMGR — an FC-Mobile-style football manager mode.
// You run a squad: a starting XI laid out on a formation pitch + a bench,
// every player a position and a rating. Drag bodies between the pitch slots
// and the bench to set your team, switch the shape (4-4-2 / 4-3-3), wheel and
// deal in a TRANSFER MARKET against a cash budget, then hit PLAY MATCH — the
// engine auto-sims a scoreline from your starting-XI strength vs the opponent's
// and feeds the result into a small league table. Survive 10 rounds; finish top.
//
// It's a menu + table game, mouse-first. No real-time match, no training.
//
// MOUSE: drag a player to move him between a formation slot and the bench;
// click 4-4-2 / 4-3-3 to switch shape; in the MARKET click BUY on a listed
// player or SELL on one of yours; click PLAY MATCH to sim the round; click
// LEAGUE to see the table. Keys: TAB cycles SQUAD/MARKET/TABLE, ENTER plays.

// ---------------------------------------------------------------------------
// positions
enum { POS_GK, POS_DEF, POS_MID, POS_FWD, NPOS };
static const char *POSL[NPOS] = { "GK", "DEF", "MID", "FWD" };
static const int   POSC[NPOS] = { CLR_ORANGE, CLR_BLUE, CLR_GREEN, CLR_RED };

// ---------------------------------------------------------------------------
// players — one pool. slots 0..10 = starting XI, 11.. = bench. -1 rating = empty.
#define MAXP 18
typedef struct { char name[12]; int pos; int rating; int kit; } Player;
static Player squad[MAXP];     // your roster (filled 0..nplayers-1)
static int    nplayers;

// formation: which player index (into squad[]) sits in each of the 11 XI slots,
// plus bench list. We keep XI[11] (player idx or -1) and BENCH order implicit:
// a player is "starting" if his index appears in XI, else he's on the bench.
static int XI[11];             // player index per pitch slot, -1 = empty slot

// formation shapes: slot -> position requirement + pitch coords (0..1 of pitch box)
enum { F_442, F_433, NFORM };
static const char *FORML[NFORM] = { "4-4-2", "4-3-3" };
// per shape: position of each of 11 slots
static const int FPOS[NFORM][11] = {
    { POS_GK, POS_DEF,POS_DEF,POS_DEF,POS_DEF, POS_MID,POS_MID,POS_MID,POS_MID, POS_FWD,POS_FWD },
    { POS_GK, POS_DEF,POS_DEF,POS_DEF,POS_DEF, POS_MID,POS_MID,POS_MID,        POS_FWD,POS_FWD,POS_FWD },
};
// normalized pitch coords (x,y in 0..100) per slot
static const int FX[NFORM][11] = {
    { 50,  18,38,62,82,  18,38,62,82,  38,62 },
    { 50,  18,38,62,82,  30,50,70,     22,50,78 },
};
static const int FY[NFORM][11] = {
    { 90,  68,72,72,68,  46,50,50,46,  22,22 },
    { 90,  68,72,72,68,  48,44,48,     22,18,22 },
};
static int formation;

// ---------------------------------------------------------------------------
// transfer market — a refreshing shortlist of buyable players
#define NMARKET 6
static Player market[NMARKET];
static int    mprice[NMARKET];    // asking price
static bool   msold[NMARKET];

// ---------------------------------------------------------------------------
// league — you + 5 AI clubs
#define NCLUB 6
typedef struct { char name[12]; int str; int W,D,L,GF,GA,pts; } Club;
static Club club[NCLUB];          // club[0] = YOU
#define YOU 0

// ---------------------------------------------------------------------------
// state
enum { TITLE, SQUAD, MARKET, RESULT, TABLE, OVER };
static int state;
static int cash;
static int round_;                 // 1..NROUNDS
#define NROUNDS 10
static int best;

// match result animation
static int   res_my, res_op;       // your goals / opp goals this match
static int   res_opp;              // opponent club index
static float res_t;                // 0..1 reveal
static int   res_shown_my, res_shown_op;   // ticking score
static float flashT; static int flashColor;

// drag state
static int   drag_from;            // XI slot being dragged (0..10), or -1
static int   drag_bench;           // bench player index being dragged, or -1
static int   drag_dx, drag_dy;

// ---------------------------------------------------------------------------
// names
static const char *FIRST[] = { "Rui","Kai","Tom","Leo","Max","Jon","Sam","Ade","Ivo","Bru","Dan","Eze","Nik","Pol","Yan","Oma" };
static const char *LAST[]  = { "Vos","Berg","Cruz","Lima","Kane","Mane","Diaz","Sane","Roy","Cole","Park","Reus","Toni","Mbe","Foden","Saka" };
#define NF (int)(sizeof FIRST / sizeof *FIRST)
#define NL (int)(sizeof LAST / sizeof *LAST)
static const char *CLUBNM[] = { "YOUR FC","REAL ROCK","INTER LUX","BAY VOLT","AC NOVA","OLD PORT","FK STORM","UTD IRON" };

static void mk_name(char *out, int seed) {
    const char *a = LAST[(seed * 7 + 3) % NL];
    int n = 0; while (a[n] && n < 11) { out[n] = a[n]; n++; } out[n] = 0;
}

// random player of a position with a rating band
static void gen_player(Player *p, int pos, int lo, int hi) {
    mk_name(p->name, rnd(9999));
    p->pos = pos;
    p->rating = rnd_between(lo, hi + 1);
    p->kit = 0;
}

// ---------------------------------------------------------------------------
// helpers
static bool hover(int x, int y, int w, int h) { return point_in_box(mouse_x(), mouse_y(), x, y, w, h); }
static bool clicked(int x, int y, int w, int h) { return mouse_pressed(MOUSE_LEFT) && hover(x, y, w, h); }
static int  rating_col(int r) { return r >= 85 ? CLR_GREEN : r >= 75 ? CLR_LIME_GREEN : r >= 65 ? CLR_YELLOW : CLR_ORANGE; }

static void coin(void) { hit(81, INSTR_SQUARE, 3, 40); schedule(45, 88, INSTR_SQUARE, 3); }
static void nope(void) { note(45, INSTR_SQUARE, 2); }
static void whistle(void){ note(96, INSTR_TRI, 3); schedule(90, 99, INSTR_TRI, 3); }

// is a player index currently in the starting XI? returns slot or -1
static int xi_slot_of(int pidx) { for (int s = 0; s < 11; s++) if (XI[s] == pidx) return s; return -1; }

// strength of starting XI: sum of ratings, with a penalty when a slot's
// occupant doesn't match the required position (out of position), empty slots = 0.
static int xi_strength(void) {
    int total = 0;
    for (int s = 0; s < 11; s++) {
        int pidx = XI[s];
        if (pidx < 0) continue;
        int r = squad[pidx].rating;
        if (squad[pidx].pos != FPOS[formation][s]) r = r * 8 / 10;   // -20% out of position
        total += r;
    }
    return total;
}
static int xi_filled(void) { int n = 0; for (int s = 0; s < 11; s++) if (XI[s] >= 0) n++; return n; }

// bench = squad players not in the XI
static int bench_list(int *out) {
    int n = 0;
    for (int i = 0; i < nplayers; i++) if (xi_slot_of(i) < 0) out[n++] = i;
    return n;
}

// ---------------------------------------------------------------------------
static void refresh_market(void) {
    for (int i = 0; i < NMARKET; i++) {
        int pos = i == 0 ? POS_GK : 1 + rnd(NPOS - 1);
        gen_player(&market[i], pos, 60, 89);
        // price scales steeply with rating
        int r = market[i].rating;
        mprice[i] = (r - 55) * (r - 55) * 4 + 200;
        msold[i] = false;
    }
}

static void new_league(void) {
    static const int STR[NCLUB] = { 0, 880, 840, 800, 770, 720 };  // club[0] set from squad
    for (int c = 0; c < NCLUB; c++) {
        int n = 0; const char *nm = CLUBNM[c];
        while (nm[n] && n < 11) { club[c].name[n] = nm[n]; n++; } club[c].name[n] = 0;
        club[c].str = STR[c];
        club[c].W = club[c].D = club[c].L = club[c].GF = club[c].GA = club[c].pts = 0;
    }
}

static void new_game(void) {
    nplayers = 0;
    // starting roster: 1 GK, 5 DEF, 6 MID, 4 FWD = 16, mid ratings
    int plan[][2] = { {POS_GK,1},{POS_DEF,5},{POS_MID,6},{POS_FWD,4} };
    for (int k = 0; k < 4; k++)
        for (int j = 0; j < plan[k][1] && nplayers < MAXP; j++)
            gen_player(&squad[nplayers++], plan[k][0], 62, 78);

    formation = F_442;
    // auto-fill XI: best matching-position player per slot, then any
    for (int s = 0; s < 11; s++) XI[s] = -1;
    bool used[MAXP] = { false };
    for (int s = 0; s < 11; s++) {
        int want = FPOS[formation][s], bestp = -1, bestr = -1;
        for (int i = 0; i < nplayers; i++)
            if (!used[i] && squad[i].pos == want && squad[i].rating > bestr) { bestr = squad[i].rating; bestp = i; }
        if (bestp < 0)
            for (int i = 0; i < nplayers; i++)
                if (!used[i] && squad[i].rating > bestr) { bestr = squad[i].rating; bestp = i; }
        if (bestp >= 0) { XI[s] = bestp; used[bestp] = true; }
    }

    cash = 4000;
    round_ = 1;
    refresh_market();
    new_league();
    drag_from = drag_bench = -1;
    res_t = 0;
}

void init(void) {
    best = load(0);
    instrument(5, INSTR_SAW, 6, 90, 4, 120); instrument_filter(5, FILTER_LOW, 1200, 4); // crowd-ish stab
    new_game();
    state = TITLE;
}

// ---------------------------------------------------------------------------
// switching formation: remap XI to keep players where positions still match,
// drop overflow (slot count changes only in arrangement, still 11 slots).
static void set_formation(int f) {
    if (f == formation) return;
    int keep[11]; for (int s = 0; s < 11; s++) keep[s] = XI[s];
    formation = f;
    // simple: keep the same player-per-slot mapping (both shapes are 11 slots),
    // the position-match penalty will just shift. That's the FC-mobile feel.
    for (int s = 0; s < 11; s++) XI[s] = keep[s];
    sfx(-1); coin();
}

// ---------------------------------------------------------------------------
// play one match: poisson-ish scoreline from strength ratio
static int sim_goals(int my_str, int op_str) {
    float ratio = (float)my_str / (float)(op_str + 1);
    float xg = clamp(0.7f + (ratio - 1.0f) * 2.2f, 0.15f, 4.2f);
    int g = 0;
    for (int k = 0; k < 7; k++) if (rnd_float() < xg / 7.0f * 1.6f) g++;   // up to ~7 chances
    return g;
}

static void apply_result(int my, int op, int opp) {
    Club *me = &club[YOU], *o = &club[opp];
    me->GF += my; me->GA += op; o->GF += op; o->GA += my;
    if (my > op)      { me->W++; o->L++; me->pts += 3; cash += 800; }
    else if (my < op) { me->L++; o->W++; o->pts += 3;  cash += 250; }
    else              { me->D++; o->D++; me->pts++; o->pts++; cash += 450; }

    // other AI clubs also play each other (round-robin-ish), advance the table
    for (int c = 1; c < NCLUB; c++) {
        if (c == opp) continue;
        int d = 1 + ((c + round_) % (NCLUB - 1));
        if (d == c) d = 1 + (d % (NCLUB - 1));
        if (d <= c) continue;            // each pair once
        int a = sim_goals(club[c].str, club[d].str);
        int b = sim_goals(club[d].str, club[c].str);
        club[c].GF += a; club[c].GA += b; club[d].GF += b; club[d].GA += a;
        if (a > b)      { club[c].W++; club[d].L++; club[c].pts += 3; }
        else if (b > a) { club[d].W++; club[c].L++; club[d].pts += 3; }
        else            { club[c].D++; club[d].D++; club[c].pts++; club[d].pts++; }
    }
}

static void play_match(void) {
    if (xi_filled() < 11) { nope(); return; }
    res_opp = 1 + ((round_ - 1) % (NCLUB - 1));
    club[YOU].str = xi_strength();
    res_my = sim_goals(club[YOU].str, club[res_opp].str);
    res_op = sim_goals(club[res_opp].str, club[YOU].str);
    apply_result(res_my, res_op, res_opp);
    res_t = 0; res_shown_my = res_shown_op = 0;
    state = RESULT;
    whistle();
}

static int league_pos(void) {     // your rank 1..NCLUB by pts then GD
    int rank = 1;
    int mygd = club[YOU].GF - club[YOU].GA;
    for (int c = 0; c < NCLUB; c++) {
        if (c == YOU) continue;
        int gd = club[c].GF - club[c].GA;
        if (club[c].pts > club[YOU].pts || (club[c].pts == club[YOU].pts && gd > mygd)) rank++;
    }
    return rank;
}

// ---------------------------------------------------------------------------
// layout constants
#define HUD_H 12
// pitch box
#define PX 8
#define PY 18
#define PW 184
#define PH 168
// bench panel
#define BNX 198
#define BNY 18
#define BNW 116
// formation slot pixel center
static int slot_px(int s) { return PX + 14 + FX[formation][s] * (PW - 28) / 100; }
static int slot_py(int s) { return PY + 12 + FY[formation][s] * (PH - 24) / 100; }
// tab bar at very bottom
#define TBY 190
#define TBH 10

// ---------------------------------------------------------------------------
void update(void) {
    if (flashT > 0) flashT -= dt();

    if (state == TITLE) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_ENTER) || btnp(0, BTN_A)) state = SQUAD;
        return;
    }
    if (state == OVER) {
        if (mouse_pressed(MOUSE_LEFT) || keyp(KEY_ENTER) || btnp(0, BTN_A)) { new_game(); state = TITLE; }
        return;
    }
    if (state == RESULT) {
        res_t = clamp(res_t + dt() * 0.7f, 0, 1.4f);
        int tm = (int)lerp(0, res_my, clamp(res_t * 1.5f, 0, 1));
        int to = (int)lerp(0, res_op, clamp(res_t * 1.5f, 0, 1));
        if (tm != res_shown_my) { res_shown_my = tm; note(72, INSTR_SQUARE, 4); }
        if (to != res_shown_op) { res_shown_op = to; note(60, INSTR_SAW, 3); }
        if (res_t >= 1.2f && (mouse_pressed(MOUSE_LEFT) || keyp(KEY_ENTER) || btnp(0, BTN_A))) {
            if (round_ >= NROUNDS) { state = OVER; }
            else { round_++; refresh_market(); state = TABLE; }
        }
        return;
    }

    // tab bar (shared by SQUAD / MARKET / TABLE)
    int tbw = SCREEN_W / 3;
    if (clicked(0, TBY, tbw, TBH)) state = SQUAD;
    if (clicked(tbw, TBY, tbw, TBH)) state = MARKET;
    if (clicked(tbw * 2, TBY, SCREEN_W - tbw * 2, TBH)) state = TABLE;
    if (keyp(KEY_TAB)) state = state == SQUAD ? MARKET : state == MARKET ? TABLE : SQUAD;
    if (keyp(KEY_ENTER) && xi_filled() == 11) { play_match(); return; }

    if (state == SQUAD) {
        // formation buttons
        if (clicked(BNX, 158, 56, 12)) set_formation(F_442);
        if (clicked(BNX + 60, 158, 56, 12)) set_formation(F_433);
        // play match button
        if (clicked(BNX, 173, BNW, 14) && xi_filled() == 11) { play_match(); return; }

        int bench[MAXP]; int nb = bench_list(bench);

        // begin a drag (pitch slot or bench row)
        if (mouse_pressed(MOUSE_LEFT) && drag_from < 0 && drag_bench < 0) {
            for (int s = 0; s < 11; s++) {
                int cx = slot_px(s), cy = slot_py(s);
                if (XI[s] >= 0 && point_in_box(mouse_x(), mouse_y(), cx - 9, cy - 9, 18, 18)) {
                    drag_from = s; drag_dx = mouse_x() - cx; drag_dy = mouse_y() - cy; break;
                }
            }
            if (drag_from < 0) {
                for (int i = 0; i < nb && i < 7; i++) {
                    int ry = BNY + 18 + i * 16;
                    if (point_in_box(mouse_x(), mouse_y(), BNX, ry, BNW, 15)) { drag_bench = bench[i]; break; }
                }
            }
        }

        // drop
        if (mouse_released(MOUSE_LEFT) && (drag_from >= 0 || drag_bench >= 0)) {
            // find target slot under cursor
            int target = -1;
            for (int s = 0; s < 11; s++)
                if (point_in_box(mouse_x(), mouse_y(), slot_px(s) - 11, slot_py(s) - 11, 22, 22)) { target = s; break; }
            bool over_bench = hover(BNX, BNY + 14, BNW, 7 * 16);

            if (drag_from >= 0) {              // dragging an XI player
                if (target >= 0 && target != drag_from) {       // swap two slots
                    int tmp = XI[target]; XI[target] = XI[drag_from]; XI[drag_from] = tmp; coin();
                } else if (over_bench) {       // send to bench
                    XI[drag_from] = -1; coin();
                }
            } else if (drag_bench >= 0) {      // dragging a bench player onto a slot
                if (target >= 0) {
                    // bench player takes the slot; previous occupant goes to bench
                    XI[target] = drag_bench; coin();
                }
            }
            drag_from = drag_bench = -1;
        }
        return;
    }

    if (state == MARKET) {
        int bench[MAXP]; int nb = bench_list(bench);
        // buy buttons (left column)
        for (int i = 0; i < NMARKET; i++) {
            int ry = 30 + i * 18;
            if (!msold[i] && clicked(112, ry + 1, 40, 14)) {
                if (cash >= mprice[i] && nplayers < MAXP) {
                    cash -= mprice[i]; squad[nplayers++] = market[i]; msold[i] = true; coin();
                } else nope();
            }
        }
        // sell list: bench players (can't sell from XI directly — must bench first)
        for (int i = 0; i < nb && i < 7; i++) {
            int ry = 30 + i * 18, pidx = bench[i];
            int val = (squad[pidx].rating - 55) * (squad[pidx].rating - 55) * 3 + 150;
            if (clicked(258, ry + 1, 40, 14)) {
                if (nplayers > 11) {
                    cash += val;
                    // remove pidx from squad, compact, fix XI indices
                    for (int j = pidx; j < nplayers - 1; j++) squad[j] = squad[j + 1];
                    nplayers--;
                    for (int s = 0; s < 11; s++) {
                        if (XI[s] == pidx) XI[s] = -1;
                        else if (XI[s] > pidx) XI[s]--;
                    }
                    coin();
                } else nope();
            }
        }
        return;
    }

    if (state == TABLE) {
        if (clicked(110, 175, 100, 14) && xi_filled() == 11) { play_match(); return; }
        return;
    }
}

// ===========================================================================
// drawing
// ---------------------------------------------------------------------------
static void draw_hud(void) {
    rectfill(0, 0, SCREEN_W, HUD_H, CLR_DARKER_BLUE);
    print(str("$%d", cash), 4, 2, CLR_YELLOW);
    print_centered(str("ROUND %d/%d", round_, NROUNDS), SCREEN_W/2, 2, CLR_LIGHT_YELLOW);
    print_right(str("STR %d", xi_strength()), 316, 2, CLR_LIME_GREEN);
}

static void draw_tabs(void) {
    const char *lab[3] = { "SQUAD", "MARKET", "LEAGUE" };
    int active = state == SQUAD ? 0 : state == MARKET ? 1 : 2;
    int tbw = SCREEN_W / 3;
    for (int i = 0; i < 3; i++) {
        int x = i * tbw, w = (i == 2 ? SCREEN_W - x : tbw);
        bool hv = hover(x, TBY, w, TBH);
        rectfill(x, TBY, w, TBH, i == active ? CLR_DARK_GREEN : hv ? CLR_DARK_GREY : CLR_BLACK);
        print(lab[i], x + (w - text_width(lab[i])) / 2, TBY + 2, i == active ? CLR_WHITE : CLR_LIGHT_GREY);
        line(x, TBY, x, TBY + TBH, CLR_DARKER_BLUE);
    }
}

// draw a player chip (a little kit jersey + number-ish) at center cx,cy
static void draw_chip(int cx, int cy, Player *p, bool small, bool ghost) {
    int r = small ? 7 : 8;
    int kit = POSC[p->pos];
    if (ghost) { fillp(FILL_CHECKER, -1); }
    circfill(cx, cy + 1, r, CLR_BLACK);                 // shadow
    circfill(cx, cy, r, kit);
    circ(cx, cy, r, CLR_WHITE);
    if (ghost) fillp_reset();
    // rating number
    const char *rt = str("%d", p->rating);
    print(rt, cx - text_width(rt) / 2, cy - 3, CLR_WHITE);
}

static void draw_pitch(void) {
    // turf
    rectfill(PX, PY, PW, PH, CLR_DARK_GREEN);
    for (int i = 0; i < PH; i += 16) rectfill(PX, PY + i, PW, 8, CLR_MEDIUM_GREEN);
    rect(PX + 3, PY + 3, PW - 6, PH - 6, CLR_LIGHT_GREY);
    line(PX + 3, PY + PH / 2, PX + PW - 3, PY + PH / 2, CLR_LIGHT_GREY);
    circ(PX + PW / 2, PY + PH / 2, 16, CLR_LIGHT_GREY);
    // goal boxes
    rect(PX + PW / 2 - 26, PY + 3, 52, 18, CLR_LIGHT_GREY);
    rect(PX + PW / 2 - 26, PY + PH - 21, 52, 18, CLR_LIGHT_GREY);

    // empty-slot markers + chips
    for (int s = 0; s < 11; s++) {
        int cx = slot_px(s), cy = slot_py(s);
        if (s == drag_from) continue;                   // drawn floating
        if (XI[s] < 0) {
            circ(cx, cy, 8, FPOS[formation][s] == POS_GK ? CLR_ORANGE : CLR_DARKER_GREY);
            print(POSL[FPOS[formation][s]], cx - text_width(POSL[FPOS[formation][s]]) / 2, cy - 3, CLR_DARK_GREY);
        } else {
            bool wrong = squad[XI[s]].pos != FPOS[formation][s];
            draw_chip(cx, cy, &squad[XI[s]], false, false);
            if (wrong && blink(20)) circ(cx, cy, 10, CLR_RED);    // out-of-position warning
        }
    }
}

static void draw_bench(void) {
    rectfill(BNX, BNY, BNW, 134, CLR_DARKER_PURPLE);
    rect(BNX, BNY, BNW, 134, CLR_DARK_PURPLE);
    print("BENCH", BNX + 4, BNY + 3, CLR_LIGHT_YELLOW);

    int bench[MAXP]; int nb = bench_list(bench);
    for (int i = 0; i < nb && i < 7; i++) {
        int pidx = bench[i]; if (pidx == drag_bench) continue;
        int ry = BNY + 18 + i * 16;
        bool hv = hover(BNX, ry, BNW, 15);
        rectfill(BNX + 2, ry, BNW - 4, 14, hv ? CLR_DARKER_GREY : CLR_BLACK);
        rectfill(BNX + 4, ry + 2, 8, 10, POSC[squad[pidx].pos]);
        print(POSL[squad[pidx].pos], BNX + 16, ry + 3, CLR_LIGHT_GREY);
        print(squad[pidx].name, BNX + 38, ry + 3, CLR_WHITE);
        print_right(str("%d", squad[pidx].rating), BNX + BNW - 4, ry + 3, rating_col(squad[pidx].rating));
    }
    if (nb == 0) print("(empty)", BNX + 6, BNY + 20, CLR_DARK_GREY);
    if (nb > 7)  print(str("+%d more", nb - 7), BNX + 6, BNY + 18 + 7 * 16, CLR_DARK_GREY);

    // formation buttons
    for (int f = 0; f < NFORM; f++) {
        int x = BNX + f * 60, w = 56;
        bool hv = hover(x, 158, w, 12);
        rectfill(x, 158, w, 12, f == formation ? CLR_TRUE_BLUE : hv ? CLR_DARK_GREY : CLR_BLACK);
        rect(x, 158, w, 12, CLR_BLUE);
        print(FORML[f], x + (w - text_width(FORML[f])) / 2, 160, f == formation ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    // PLAY MATCH
    bool ready = xi_filled() == 11;
    bool hv = hover(BNX, 173, BNW, 14);
    rectfill(BNX, 173, BNW, 14, ready ? (hv ? CLR_LIME_GREEN : CLR_DARK_GREEN) : CLR_DARKER_GREY);
    rect(BNX, 173, BNW, 14, ready ? CLR_GREEN : CLR_DARK_GREY);
    const char *pm = ready ? "PLAY MATCH >" : str("XI %d/11", xi_filled());
    print(pm, BNX + (BNW - text_width(pm)) / 2, 176, ready ? CLR_WHITE : CLR_LIGHT_GREY);
}

static void draw_floating_chip(void) {
    if (drag_from >= 0 && XI[drag_from] >= 0)
        draw_chip(mouse_x() - drag_dx, mouse_y() - drag_dy, &squad[XI[drag_from]], false, false);
    if (drag_bench >= 0)
        draw_chip(mouse_x(), mouse_y(), &squad[drag_bench], false, false);
}

static void draw_squad(void) {
    cls(CLR_BLACK);
    draw_pitch();
    draw_bench();
    draw_floating_chip();
    draw_hud();
    draw_tabs();
    if (drag_from >= 0 || drag_bench >= 0)
        print_centered("drop on a slot, or the bench", SCREEN_W/2, PY + PH + 1, CLR_DARK_GREY);
    else
        print("drag players to set your XI", PX + 2, PY + PH + 1, CLR_DARK_GREY);
}

static void draw_market(void) {
    cls(CLR_BLACK);
    print("TRANSFER LIST", 10, 16, CLR_LIGHT_YELLOW);
    print("SELL (bench only)", 168, 16, CLR_LIGHT_YELLOW);
    line(160, 28, 160, 186, CLR_DARK_GREY);

    // BUY list (left column): kit chip | pos | name | rating | price-BUY button
    for (int i = 0; i < NMARKET; i++) {
        int ry = 30 + i * 18;
        bool can = !msold[i] && cash >= mprice[i] && nplayers < MAXP;
        rectfill(8, ry, 150, 16, msold[i] ? CLR_DARKER_GREY : CLR_BROWNISH_BLACK);
        rectfill(10, ry + 2, 6, 12, POSC[market[i].pos]);
        print(POSL[market[i].pos], 18, ry + 4, CLR_LIGHT_GREY);
        print(market[i].name, 40, ry + 4, CLR_WHITE);
        print_right(str("%d", market[i].rating), 108, ry + 4, rating_col(market[i].rating));
        // buy button at x=112, w=40
        if (msold[i]) {
            print("SOLD", 124, ry + 4, CLR_DARK_GREY);
        } else {
            bool hv = hover(112, ry + 1, 40, 14);
            rectfill(112, ry + 1, 40, 14, can ? (hv ? CLR_LIME_GREEN : CLR_DARK_GREEN) : CLR_DARKER_GREY);
            rect(112, ry + 1, 40, 14, can ? CLR_GREEN : CLR_DARK_GREY);
            const char *pl = str("%d", mprice[i]);
            print(pl, 112 + (40 - text_width(pl)) / 2, ry + 4, CLR_WHITE);
        }
    }

    // SELL column (bench players)
    int bench[MAXP]; int nb = bench_list(bench);
    for (int i = 0; i < nb && i < 7; i++) {
        int ry = 30 + i * 18, pidx = bench[i];
        int val = (squad[pidx].rating - 55) * (squad[pidx].rating - 55) * 3 + 150;
        rectfill(166, ry, 146, 16, CLR_DARKER_PURPLE);
        rectfill(168, ry + 2, 6, 12, POSC[squad[pidx].pos]);
        print(squad[pidx].name, 178, ry + 4, CLR_WHITE);
        print_right(str("%d", squad[pidx].rating), 252, ry + 4, rating_col(squad[pidx].rating));
        // sell button at x=258, w=40
        bool can = nplayers > 11;
        bool hs = hover(258, ry + 1, 40, 14);
        rectfill(258, ry + 1, 40, 14, can ? (hs ? CLR_DARK_RED : CLR_DARKER_GREY) : CLR_DARKER_GREY);
        rect(258, ry + 1, 40, 14, can ? CLR_RED : CLR_DARK_GREY);
        const char *sl = str("%d", val);
        print(sl, 258 + (40 - text_width(sl)) / 2, ry + 4, CLR_WHITE);
    }
    if (nb == 0) print("(no spares to sell)", 168, 32, CLR_DARK_GREY);

    print("click a price to buy / sell", 10, 178, CLR_DARK_GREY);
    draw_hud();
    draw_tabs();
}

static void draw_table(void) {
    cls(CLR_DARKER_BLUE);
    print_centered("LEAGUE TABLE", SCREEN_W/2, 16, CLR_LIGHT_YELLOW);
    // header
    int y0 = 30;
    print("# CLUB", 12, y0, CLR_DARK_GREY);
    print("P", 150, y0, CLR_DARK_GREY);
    print("W D L", 168, y0, CLR_DARK_GREY);
    print("GD", 240, y0, CLR_DARK_GREY);
    print("PTS", 280, y0, CLR_DARK_GREY);

    // sort clubs by pts then GD (indices)
    int ord[NCLUB]; for (int i = 0; i < NCLUB; i++) ord[i] = i;
    for (int a = 0; a < NCLUB; a++)
        for (int b = a + 1; b < NCLUB; b++) {
            int ga = club[ord[a]].GF - club[ord[a]].GA, gb = club[ord[b]].GF - club[ord[b]].GA;
            bool swap = club[ord[b]].pts > club[ord[a]].pts ||
                        (club[ord[b]].pts == club[ord[a]].pts && gb > ga);
            if (swap) { int t = ord[a]; ord[a] = ord[b]; ord[b] = t; }
        }
    for (int i = 0; i < NCLUB; i++) {
        int c = ord[i], ry = y0 + 12 + i * 18;
        bool me = c == YOU;
        if (me) { rectfill(8, ry - 2, 304, 17, CLR_DARK_GREEN); }
        int gd = club[c].GF - club[c].GA;
        int col = me ? CLR_WHITE : CLR_LIGHT_GREY;
        print(str("%d", i + 1), 12, ry + 2, i == 0 ? CLR_YELLOW : col);
        print(club[c].name, 26, ry + 2, col);
        print(str("%d", club[c].W + club[c].D + club[c].L), 150, ry + 2, col);
        print(str("%d %d %d", club[c].W, club[c].D, club[c].L), 168, ry + 2, col);
        print(str("%+d", gd), 240, ry + 2, gd >= 0 ? CLR_LIME_GREEN : CLR_ORANGE);
        print_right(str("%d", club[c].pts), 308, ry + 2, me ? CLR_YELLOW : col);
    }

    bool ready = xi_filled() == 11;
    bool hv = hover(110, 175, 100, 14);
    rectfill(110, 175, 100, 14, ready ? (hv ? CLR_LIME_GREEN : CLR_DARK_GREEN) : CLR_DARKER_GREY);
    rect(110, 175, 100, 14, CLR_GREEN);
    print_centered(ready ? "PLAY MATCH >" : "set your XI first", SCREEN_W/2, 178, CLR_WHITE);

    draw_hud();
    draw_tabs();
}

static void draw_result(void) {
    cls(CLR_DARK_GREEN);
    for (int i = 0; i < SCREEN_H; i += 16) rectfill(0, i, SCREEN_W, 8, CLR_MEDIUM_GREEN);
    fillp(FILL_CHECKER, -1); rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK); fillp_reset();

    print_centered("FULL TIME", SCREEN_W/2, 30, CLR_LIGHT_YELLOW);
    // scoreboard
    int bx = 50, by = 60, bw = 220, bh = 60;
    rectfill(bx, by, bw, bh, CLR_BLACK);
    rect(bx, by, bw, bh, CLR_LIGHT_YELLOW);
    print(club[YOU].name, bx + 12, by + 10, CLR_WHITE);
    print_right(club[res_opp].name, bx + bw - 12, by + 10, CLR_WHITE);
    print_scaled(str("%d", res_shown_my), bx + 56, by + 28, CLR_LIME_GREEN, 3);
    print_scaled("-", bx + bw / 2 - 4, by + 28, CLR_LIGHT_GREY, 3);
    print_scaled(str("%d", res_shown_op), bx + bw - 64, by + 28, CLR_RED, 3);

    if (res_t >= 1.0f) {
        const char *verdict = res_my > res_op ? "WIN!" : res_my < res_op ? "DEFEAT" : "DRAW";
        int vc = res_my > res_op ? CLR_GREEN : res_my < res_op ? CLR_RED : CLR_YELLOW;
        print_centered(verdict, SCREEN_W/2, 134, vc);
        print_centered(str("league position: %d", league_pos()), SCREEN_W/2, 150, CLR_LIGHT_YELLOW);
        if (blink(22)) print_centered(round_ >= NROUNDS ? "click for season end" : "click to continue", SCREEN_W/2, 168, CLR_WHITE);
    }
}

static void draw_title(void) {
    cls(CLR_DARK_GREEN);
    for (int i = 0; i < SCREEN_H; i += 16) rectfill(0, i, SCREEN_W, 8, CLR_MEDIUM_GREEN);
    circ(SCREEN_W / 2, 70, 24, CLR_WHITE);
    print_scaled("FOOTBALL", (SCREEN_W - text_width("FOOTBALL") * 3) / 2, 100, CLR_WHITE, 3);
    print_scaled("MANAGER", (SCREEN_W - text_width("MANAGER") * 2) / 2, 124, CLR_LIGHT_YELLOW, 2);
    print_centered("drag your XI - deal - PLAY MATCH", SCREEN_W/2, 150, CLR_LIGHT_GREY);
    print_centered(str("best finish: %s", best == 0 ? "none yet" : (best == 1 ? "CHAMPIONS" : str("#%d", best))), SCREEN_W/2, 164, CLR_YELLOW);
    print_centered("click / ENTER to kick off", SCREEN_W/2, 180, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

static void draw_over(void) {
    int pos = league_pos();
    if (best == 0 || pos < best) { best = pos; save(0, best); }
    cls(CLR_DARKER_BLUE);
    rectfill(40, 40, 240, 120, CLR_BLACK);
    rect(40, 40, 240, 120, CLR_LIGHT_YELLOW);
    print_centered("-- SEASON OVER --", SCREEN_W/2, 52, CLR_LIGHT_YELLOW);
    const char *r = pos == 1 ? "CHAMPIONS!" : pos == 2 ? "RUNNERS-UP" : str("FINISHED #%d", pos);
    int rc = pos == 1 ? CLR_GREEN : pos == 2 ? CLR_LIME_GREEN : CLR_ORANGE;
    print_scaled(r, (SCREEN_W - text_width(r) * 2) / 2, 70, rc, 2);
    print_centered(str("%dW %dD %dL", club[YOU].W, club[YOU].D, club[YOU].L), SCREEN_W/2, 98, CLR_WHITE);
    print_centered(str("%d points  %+d GD", club[YOU].pts, club[YOU].GF - club[YOU].GA), SCREEN_W/2, 112, CLR_LIGHT_GREY);
    print_centered(str("best finish: #%d", best), SCREEN_W/2, 128, CLR_YELLOW);
    print_centered("click to manage a new season", SCREEN_W/2, 146, blink(22) ? CLR_WHITE : CLR_DARK_GREY);
}

static void draw_flash(void) {
    if (flashT <= 0) return;
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, SCREEN_H, flashColor);
    fillp_reset();
}

void draw(void) {
    switch (state) {
        case TITLE:  draw_title();  break;
        case SQUAD:  draw_squad();  break;
        case MARKET: draw_market(); break;
        case TABLE:  draw_table();  break;
        case RESULT: draw_result(); break;
        case OVER:   draw_over();   break;
    }
    draw_flash();
}
