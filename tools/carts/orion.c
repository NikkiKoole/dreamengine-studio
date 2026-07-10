/* de:meta
{
  "slug": "orion",
  "title": "master of orion",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "turn-based-loop",
    "finite-state-ai"
  ],
  "lineage": "Homage to Master of Orion (1993); distills 4X into a single-file cart — procedural galaxy placement, greedy nearest-target AI that skips fights it can't win, defender-advantage combat math.",
  "genre": "4x",
  "homage": "Master of Orion (1993)",
  "description": "A tiny 4X space strategy. A galaxy of star systems: your stars (green) build ships each turn based on their planet's production, and you send fleets to colonise neutral stars (grey) or invade the rival's (red) — send enough to overcome the defenders, who get a defensive edge. An AI empire expands against you at the same time. Turn-based: queue your moves, then end the turn and watch the fleets fly. Conquer every enemy star to win the galaxy; lose all of yours and your empire falls. Arrows pick a star, A selects a source then a target to send all its ships, B ends the turn.",
  "todo": [
    "ui-audit: \"ALTAIR\" runs off the right edge and overlaps \"FOE 1st 8sh\"."
  ]
}
de:meta */
#include "studio.h"

// MASTER OF ORION  — a tiny 4X
// Conquer the galaxy one star at a time. Your stars (green) build ships each
// turn; send fleets to colonise neutral stars (grey) or invade the rival's
// (red). Send enough ships to beat the defenders — they get a defensive edge.
// Take every enemy star to win; lose all of yours and it's over.
// Mouse: hover a star to inspect it; click a green (your) star, then a target,
//   to send all its ships. Right-click (or click empty space) cancels.
//   Click the END TURN button (top-right) to advance the turn.
// Keys still work: arrows / WASD pick a star, Z select/send, X end turn.

#define NSTAR   14
#define MAXFLT  48
#define DEF     1.3f          // defender's advantage
#define P_YOU   1
#define P_FOE   2

// end-turn button (mouse), top-right of the star map
#define ETB_X (SCREEN_W - 74)
#define ETB_Y 3
#define ETB_W 70
#define ETB_H 11

typedef struct { int x, y, owner, ships, prod; float build; const char *name; } Star;
typedef struct { int from, to, owner, ships; float prog, speed; bool active; } Fleet;

static Star  st[NSTAR];
static Fleet flt[MAXFLT];
static int   sel, src, turn, state;   // state 0 play,1 win,2 lose
static char  msg[48];

static const char *NAMES[NSTAR] = {
    "SOL","VEGA","RIGEL","ORION","DENEB","ALTAIR","SIRIUS",
    "POLLUX","ANTARES","CASTOR","MIZAR","SPICA","DRACO","CETUS"
};

// ====================================================================

static void say(const char *s) { int i = 0; while (s[i] && i < 47) { msg[i] = s[i]; i++; } msg[i] = 0; }

static void gen_galaxy() {
    for (int i = 0; i < NSTAR; i++) {
        int ok, tries = 0, x = 0, y = 0;
        do {
            ok = 1; x = rnd_between(24, 296); y = rnd_between(20, 162);
            for (int j = 0; j < i; j++) if (distance(x, y, st[j].x, st[j].y) < 34) { ok = 0; break; }
        } while (!ok && ++tries < 60);
        st[i].x = x; st[i].y = y; st[i].name = NAMES[i];
        st[i].owner = 0; st[i].prod = 1 + rnd(4); st[i].build = 0;
        st[i].ships = 2 + rnd(3);
    }
    // homeworlds: yours nearest bottom-left, foe nearest top-right
    int you = 0, foe = 0; float yb = 1e9f, fb = 1e9f;
    for (int i = 0; i < NSTAR; i++) {
        float dy = distance(st[i].x, st[i].y, 0, 200);
        float df = distance(st[i].x, st[i].y, 320, 0);
        if (dy < yb) { yb = dy; you = i; }
        if (df < fb) { fb = df; foe = i; }
    }
    if (foe == you) foe = (you + 1) % NSTAR;
    st[you].owner = P_YOU; st[you].ships = 8; st[you].prod = 4;
    st[foe].owner = P_FOE; st[foe].ships = 8; st[foe].prod = 4;
    sel = you;
}

static void new_game() {
    for (int i = 0; i < MAXFLT; i++) flt[i].active = false;
    gen_galaxy();
    src = -1; turn = 1; state = 0;
    say("expand across the galaxy");
}

void init() { new_game(); }

// ====================================================================

static void launch(int from, int to, int owner) {
    if (st[from].ships <= 0) return;
    for (int i = 0; i < MAXFLT; i++) {
        if (flt[i].active) continue;
        float d = distance(st[from].x, st[from].y, st[to].x, st[to].y);
        flt[i] = (Fleet){ from, to, owner, st[from].ships, 0, clamp(60.0f / d, 0.18f, 1.0f), true };
        st[from].ships = 0;
        return;
    }
}

static void resolve(Fleet *f) {
    Star *t = &st[f->to];
    if (t->owner == f->owner) { t->ships += f->ships; return; }
    if (f->ships > t->ships * DEF) {                 // attacker takes the star
        int surv = f->ships - (int)(t->ships * DEF);
        t->owner = f->owner; t->ships = surv < 1 ? 1 : surv;
    } else {                                          // defender holds
        t->ships -= (int)(f->ships / DEF);
        if (t->ships < 0) t->ships = 0;
    }
}

static void ai_turn() {
    int launches = 0;
    for (int i = 0; i < NSTAR && launches < 2; i++) {
        if (st[i].owner != P_FOE || st[i].ships < 4) continue;
        int best = -1; float bestd = 1e9f;
        for (int j = 0; j < NSTAR; j++) {
            if (st[j].owner == P_FOE) continue;
            if (st[i].ships <= st[j].ships * DEF) continue;     // can't win — skip
            float d = distance(st[i].x, st[i].y, st[j].x, st[j].y);
            if (d < bestd) { bestd = d; best = j; }
        }
        if (best >= 0) { launch(i, best, P_FOE); launches++; }
    }
}

static void end_turn() {
    for (int i = 0; i < MAXFLT; i++) {
        if (!flt[i].active) continue;
        flt[i].prog += flt[i].speed;
        if (flt[i].prog >= 1.0f) { resolve(&flt[i]); flt[i].active = false; }
    }
    ai_turn();
    for (int i = 0; i < NSTAR; i++) {
        if (st[i].owner == 0) continue;
        st[i].build += st[i].prod * 0.45f;
        while (st[i].build >= 1.0f) { st[i].ships++; st[i].build -= 1.0f; }
    }
    turn++;

    int you = 0, foe = 0;
    for (int i = 0; i < NSTAR; i++) { if (st[i].owner == P_YOU) you++; if (st[i].owner == P_FOE) foe++; }
    if (foe == 0) { state = 1; }
    else if (you == 0) { state = 2; }
}

static void move_sel(int dx, int dy) {
    int best = -1, bestscore = 1 << 30;
    for (int i = 0; i < NSTAR; i++) {
        if (i == sel) continue;
        int vx = st[i].x - st[sel].x, vy = st[i].y - st[sel].y;
        if (dx && (vx * dx) <= 0) continue;
        if (dy && (vy * dy) <= 0) continue;
        int score = abs(vx) + abs(vy) + (dx ? abs(vy) * 3 : abs(vx) * 3);  // prefer on-axis
        if (score < bestscore) { bestscore = score; best = i; }
    }
    if (best >= 0) sel = best;
}

// nearest star under the pointer (with a little slop for easy clicking), or -1
static int star_at(int mx, int my) {
    int best = -1; float bd = 1e9f;
    for (int i = 0; i < NSTAR; i++) {
        int r = 3 + st[i].prod + 3;
        float d = distance(mx, my, st[i].x, st[i].y);
        if (d <= r && d < bd) { bd = d; best = i; }
    }
    return best;
}

static bool over_endturn(int mx, int my) {
    return mx >= ETB_X && mx < ETB_X + ETB_W && my >= ETB_Y && my < ETB_Y + ETB_H;
}

// select source, or send a fleet to the target — shared by mouse click and the Z key
static void try_select(int s) {
    if (src < 0) {
        if (st[s].owner == P_YOU && st[s].ships > 0) { src = s; say(str("send from %s — pick a target", st[s].name)); }
        else note(40, INSTR_SQUARE, 2);
    } else if (s == src) { src = -1; say("cancelled"); }
    else {
        launch(src, s, P_YOU);
        say(str("fleet away to %s", st[s].name));
        note(67, INSTR_SQUARE, 2);
        src = -1;
    }
}

void update() {
    int mx = mouse_x(), my = mouse_y();
    int hov = star_at(mx, my);

    if (state != 0) {
        if (btnp(0, BTN_A) || btnp(0, BTN_B) || mouse_pressed(MOUSE_LEFT)) new_game();
        return;
    }

    // hovering a star drives the selection, so the ring + HUD follow the pointer
    if (hov >= 0 && !over_endturn(mx, my)) sel = hov;

    // mouse: left-click a star to select/send, the button to end turn, empty space cancels
    if (mouse_pressed(MOUSE_LEFT)) {
        if (over_endturn(mx, my)) { end_turn(); note(60, INSTR_SINE, 1); }
        else if (hov >= 0)        try_select(hov);
        else if (src >= 0)        { src = -1; say("cancelled"); }
    }
    if (mouse_pressed(MOUSE_RIGHT) && src >= 0) { src = -1; say("cancelled"); }

    // keyboard / gamepad still work
    if (btnp(0, BTN_LEFT))  move_sel(-1, 0);
    if (btnp(0, BTN_RIGHT)) move_sel(1, 0);
    if (btnp(0, BTN_UP))    move_sel(0, -1);
    if (btnp(0, BTN_DOWN))  move_sel(0, 1);
    if (btnp(0, BTN_A))     try_select(sel);
    if (btnp(0, BTN_B))     { end_turn(); note(60, INSTR_SINE, 1); }
}

// ====================================================================
// drawing
// ====================================================================

static int ocol(int o) { return o == P_YOU ? CLR_GREEN : o == P_FOE ? CLR_RED : CLR_LIGHT_GREY; }

void draw() {
    cls(CLR_BLACK);
    // faint starfield
    for (int i = 0; i < 40; i++) pset((i * 71 + 13) % SCREEN_W, (i * 37 + 7) % 170, CLR_DARKER_BLUE);

    // fleets in transit
    for (int i = 0; i < MAXFLT; i++) {
        if (!flt[i].active) continue;
        int fx = (int)lerp(st[flt[i].from].x, st[flt[i].to].x, flt[i].prog);
        int fy = (int)lerp(st[flt[i].from].y, st[flt[i].to].y, flt[i].prog);
        line(st[flt[i].from].x, st[flt[i].from].y, fx, fy, flt[i].owner == P_YOU ? CLR_DARK_GREEN : CLR_DARK_RED);
        circfill(fx, fy, 2, ocol(flt[i].owner));
        print(str("%d", flt[i].ships), fx + 3, fy - 3, ocol(flt[i].owner));
    }

    // send-line preview — follows the pointer
    if (src >= 0) line(st[src].x, st[src].y, mouse_x(), mouse_y(), CLR_YELLOW);

    // stars
    for (int i = 0; i < NSTAR; i++) {
        int r = 3 + st[i].prod;
        circfill(st[i].x, st[i].y, r, ocol(st[i].owner));
        circ(st[i].x, st[i].y, r, CLR_DARKER_GREY);
        if (st[i].owner != 0 || st[i].ships > 0)
            print(str("%d", st[i].ships), st[i].x - 3, st[i].y - r - 9, CLR_WHITE);
        print(st[i].name, st[i].x - 10, st[i].y + r + 1, CLR_DARK_GREY);
    }

    // selection ring (and source marker)
    circ(st[sel].x, st[sel].y, 3 + st[sel].prod + 3, CLR_WHITE);
    if (src >= 0) circ(st[src].x, st[src].y, 3 + st[src].prod + 5, CLR_YELLOW);

    // HUD
    rectfill(0, 170, SCREEN_W, 30, CLR_DARKER_PURPLE);
    int you = 0, foe = 0, yships = 0, fships = 0;
    for (int i = 0; i < NSTAR; i++) {
        if (st[i].owner == P_YOU) { you++; yships += st[i].ships; }
        if (st[i].owner == P_FOE) { foe++; fships += st[i].ships; }
    }
    print(str("TURN %d", turn), 4, 172, CLR_LIGHT_GREY);
    print(str("YOU %dst %dsh", you, yships), 70, 172, CLR_GREEN);
    print_right(str("FOE %dst %dsh", foe, fships), SCREEN_W - 4, 172, CLR_RED);
    Star *s = &st[sel];
    print(str("%s: %s  prod %d  ships %d",
              s->name, s->owner == P_YOU ? "YOURS" : s->owner == P_FOE ? "ENEMY" : "NEUTRAL",
              s->prod, s->ships), 4, 182, CLR_WHITE);
    print(msg, 4, 191, CLR_LIGHT_PEACH);

    // end-turn button (mouse)
    bool overET = over_endturn(mouse_x(), mouse_y());
    rectfill(ETB_X, ETB_Y, ETB_W, ETB_H, overET ? CLR_INDIGO : CLR_DARKER_PURPLE);
    rect    (ETB_X, ETB_Y, ETB_W, ETB_H, CLR_LIGHT_GREY);
    print("END TURN", ETB_X + 4, ETB_Y + 2, CLR_WHITE);

    if (state != 0) {
        rectfill(SCREEN_W / 2 - 74, SCREEN_H / 2 - 22, 148, 46, CLR_BLACK);
        rect    (SCREEN_W / 2 - 74, SCREEN_H / 2 - 22, 148, 46, state == 1 ? CLR_GREEN : CLR_RED);
        print_centered(state == 1 ? "GALAXY CONQUERED" : "YOUR EMPIRE FALLS", SCREEN_W/2, SCREEN_H / 2 - 12, state == 1 ? CLR_GREEN : CLR_RED);
        print_centered(str("in %d turns", turn), SCREEN_W/2, SCREEN_H / 2, CLR_YELLOW);
        print_centered("click to play again", SCREEN_W/2, SCREEN_H / 2 + 12, CLR_LIGHT_GREY);
    }
}
