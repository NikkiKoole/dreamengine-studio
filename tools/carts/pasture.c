/* de:meta
{
  "title": "pasture",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "flocking",
    "schedule-driven-agents",
    "finite-state-ai"
  ],
  "lineage": "Based on two RogueBasin articles (townsfolk AI and denizen herding behavior) — combines boids sheep, a schedule-driven townsfolk system with personality overrides, and a simple wolf predator on a shared day/night clock.",
  "description": "A living world on one day/night clock (RogueBasin: townsfolk AI + Denizen Herding). TOWNSFOLK follow a daily SCHEDULE -- sleep at home, gather at the well, work the field, drink at the tavern by the hour -- with personalities bending it (a drunk haunts the tavern, a farmer never leaves the field); a glyph over each head shows the activity. SHEEP HERD by boids (cohesion + alignment + separation) and FLEE the wolf, harder the moment it HOWLS (sound drives behaviour). The WOLF hunts the nearest sheep and howls on a timer; the flock bolts. The clock drives a true-colour sky gradient (rectfill_rgb), an ambient chord that shifts with the phase, a bell on the hour, and bleats/howl panned by position. Click a townsperson to inspect, H to force a howl, R to reseed."
}
de:meta */
#include "studio.h"

// PASTURE — a living world: schedules + herding + predator/prey on one clock.
// (RogueBasin: Implementing interesting townsfolk AI + Denizen Herding Behavior.)
//
//  * TOWNSFOLK follow a daily SCHEDULE — sleep at home, gather at the well, work
//    the field, drink at the tavern — by the hour. Personalities bend it (a drunk
//    haunts the tavern, a farmer never leaves the field). A glyph over each head
//    shows the current activity.
//  * SHEEP HERD by boids (cohesion + alignment + separation) and FLEE the wolf —
//    harder the moment it HOWLS (sound drives behaviour, the herding article's
//    point: same-species pull, fractional so individuals still break off).
//  * the WOLF hunts the nearest sheep and howls on a timer; the flock bolts.
//  * a DAY/NIGHT clock drives a true-colour sky, an ambient chord that changes
//    with the phase, and a bell on the hour. Bleats + howl are panned by position.
//
// click a townsperson to inspect   H force a howl   R reseed

#define NT 5
#define NS 12
#define DAYLEN 70.0f          // real seconds per in-game day
#define HUD_Y 184

enum { HOME, WELL, FIELD, TAVERN };
static const char ACTGLY[4] = { 'z', 'o', '/', 'u' };       // sleep / gather / work / drink
static const int  ACTCOL[4] = { CLR_INDIGO, CLR_YELLOW, CLR_GREEN, CLR_ORANGE };
enum { ANY, DRUNK, FARMER };

static const int WELLX = 160, WELLY = 88, FIELDX = 55, FIELDY = 138, TAVX = 258, TAVY = 128;

typedef struct { float x, y, vx, vy; int hx, hy; char g; int col, kind; const char *name; } Town;
typedef struct { float x, y, vx, vy; } Sheep;
static Town  town[NT];
static Sheep sheep[NS];
static struct { float x, y, vx, vy; int howl, cool; } wolf;

static float clk = 6.0f;       // hours 0..24 (open at dawn)
static int   last_hour = -1;
static int   sel, bleat_cd;

static const struct { const char *name; char g; int col, kind; } PT[NT] = {
    { "mara",   'M', CLR_PINK,        ANY    },
    { "bert",   'B', CLR_ORANGE,      DRUNK  },
    { "tilda",  'T', CLR_LIGHT_PEACH, ANY    },
    { "gus",    'G', CLR_BROWN,       FARMER },
    { "ana",    'A', CLR_LIGHT_GREY,  ANY    },
};

// ---- panned voice pool -----------------------------------------------------
typedef struct { int h, ttl; } Voice;
static Voice voices[16];
static void play_pan(int midi, int instr, int vol, float pan, int ttl) {
    for (int i = 0; i < 16; i++) if (voices[i].ttl <= 0) {
        voices[i].h = note_on(midi, instr, vol); note_pan(voices[i].h, pan); voices[i].ttl = ttl; return;
    }
}
static void voices_tick(void) {
    for (int i = 0; i < 16; i++) if (voices[i].ttl > 0 && --voices[i].ttl == 0) note_off(voices[i].h);
}
// ambient drone (held) — changes only on phase change (set-and-hold)
static int amb[3], amb_phase = -1;

static float frnd(void) { return rnd(1000) / 1000.0f; }

void init(void) {
    reverb(0.5f, 0.55f);
    instrument(9, INSTR_TRI, 500, 700, 4, 1400);     // soft ambient pad (slot 9)
    for (int i = 0; i < NT; i++) {
        Town *t = &town[i];
        t->name = PT[i].name; t->g = PT[i].g; t->col = PT[i].col; t->kind = PT[i].kind;
        t->hx = 28 + i * 58; t->hy = 30;             // each has a house along the top row
        t->x = t->hx; t->y = t->hy; t->vx = t->vy = 0;
    }
    for (int i = 0; i < NS; i++) { sheep[i].x = 180 + rnd(90); sheep[i].y = 70 + rnd(70); sheep[i].vx = sheep[i].vy = 0; }
    wolf.x = 290; wolf.y = 150; wolf.cool = 240;
}

// ---- routine: hour -> a target location for this townsperson ---------------
static int routine(Town *t, float h) {
    int loc;
    if (h < 5 || h >= 22)      loc = HOME;
    else if (h < 8)            loc = WELL;
    else if (h < 12)           loc = FIELD;
    else if (h < 13)           loc = TAVERN;
    else if (h < 18)           loc = FIELD;
    else                       loc = TAVERN;
    if (t->kind == DRUNK  && h >= 8 && h < 22) loc = TAVERN;   // personality bends the routine
    if (t->kind == FARMER && h >= 6 && h < 20) loc = FIELD;
    return loc;
}
static void loc_xy(Town *t, int loc, int *lx, int *ly) {
    if (loc == HOME)        { *lx = t->hx; *ly = t->hy; }
    else if (loc == WELL)   { *lx = WELLX; *ly = WELLY; }
    else if (loc == FIELD)  { *lx = FIELDX + (int)(t->hx % 30); *ly = FIELDY; }
    else                    { *lx = TAVX - (int)(t->hx % 24);   *ly = TAVY; }
}
static void steer(float *x, float *y, float *vx, float *vy, float tx, float ty, float spd) {
    float dx = tx - *x, dy = ty - *y, d = fsqrt(dx*dx + dy*dy);
    if (d > 1) { *vx += (dx/d) * spd; *vy += (dy/d) * spd; }
    *vx *= 0.82f; *vy *= 0.82f;                       // damping = arrival
    *x += *vx; *y += *vy;
}

void update(void) {
    voices_tick();
    clk += 24.0f / (DAYLEN * 60.0f);
    if (clk >= 24) clk -= 24;
    int hour = (int)clk;

    if (keyp('R')) init();
    if (mouse_pressed(0) && mouse_y() < HUD_Y) {
        int best = -1; float bd = 1e9f;
        for (int i = 0; i < NT; i++) { float d = (town[i].x-mouse_x())*(town[i].x-mouse_x())+(town[i].y-mouse_y())*(town[i].y-mouse_y()); if (d < bd) { bd = d; best = i; } }
        if (best >= 0) sel = best;
    }

    // ---- ambient chord on phase change -------------------------------------
    int phase = hour < 5 ? 0 : hour < 8 ? 1 : hour < 18 ? 2 : hour < 21 ? 3 : 0;   // night/dawn/day/dusk
    if (phase != amb_phase) {
        static const int CH[4][3] = { {38,45,53}, {45,52,59}, {48,55,64}, {43,50,58} };
        for (int i = 0; i < 3; i++) { if (amb_phase >= 0) note_off(amb[i]); amb[i] = note_on(CH[phase][i], 9, 2); }
        amb_phase = phase;
    }
    // ---- bell on the hour --------------------------------------------------
    if (hour != last_hour) { last_hour = hour; play_pan(72, INSTR_FM, (hour%6==0)?3:1, 0, 30); }

    // ---- townsfolk: steer to scheduled location ----------------------------
    for (int i = 0; i < NT; i++) {
        Town *t = &town[i];
        int loc = routine(t, clk), lx, ly; loc_xy(t, loc, &lx, &ly);
        steer(&t->x, &t->y, &t->vx, &t->vy, lx, ly, 0.20f);
    }

    // ---- wolf: hunt nearest sheep, howl on a timer -------------------------
    int target = 0; float bd = 1e9f;
    for (int i = 0; i < NS; i++) { float d = (sheep[i].x-wolf.x)*(sheep[i].x-wolf.x)+(sheep[i].y-wolf.y)*(sheep[i].y-wolf.y); if (d < bd) { bd = d; target = i; } }
    steer(&wolf.x, &wolf.y, &wolf.vx, &wolf.vy, sheep[target].x, sheep[target].y, 0.16f);
    if (wolf.cool > 0) wolf.cool--;
    if ((keyp('H') || wolf.cool == 0)) {
        wolf.howl = 70; wolf.cool = 300 + rnd(180);
        play_pan(33, INSTR_BRASS, 5, (wolf.x/SCREEN_W)*2-1, 70);     // the howl
    }
    if (wolf.howl > 0) wolf.howl--;

    // ---- sheep: boids + flee the wolf --------------------------------------
    float fear = wolf.howl > 0 ? 2.6f : 1.0f;
    for (int i = 0; i < NS; i++) {
        Sheep *s = &sheep[i];
        float cx = 0, cy = 0, ax = 0, ay = 0, sx = 0, sy = 0; int n = 0;
        for (int j = 0; j < NS; j++) {
            if (j == i) continue;
            float dx = sheep[j].x - s->x, dy = sheep[j].y - s->y, d2 = dx*dx + dy*dy;
            if (d2 < 1600) { cx += sheep[j].x; cy += sheep[j].y; ax += sheep[j].vx; ay += sheep[j].vy; n++;
                if (d2 < 90 && d2 > 0.01f) { sx -= dx/d2*40; sy -= dy/d2*40; } }
        }
        if (n) { cx = cx/n - s->x; cy = cy/n - s->y; ax /= n; ay /= n;
                 s->vx += cx*0.0016f + ax*0.04f + sx*0.02f;     // cohesion (fractional) + align + separate
                 s->vy += cy*0.0016f + ay*0.04f + sy*0.02f; }
        float wdx = s->x - wolf.x, wdy = s->y - wolf.y, wd = fsqrt(wdx*wdx + wdy*wdy);
        if (wd < 70) { s->vx += wdx/(wd+1)*fear; s->vy += wdy/(wd+1)*fear; }   // flee
        // graze wander + bounds (the pasture)
        s->vx += (frnd()-0.5f)*0.25f; s->vy += (frnd()-0.5f)*0.25f;
        if (s->x < 116) s->vx += 0.5f; if (s->x > 308) s->vx -= 0.5f;
        if (s->y < 28)  s->vy += 0.5f; if (s->y > 172) s->vy -= 0.5f;
        float sp = fsqrt(s->vx*s->vx + s->vy*s->vy), mx = fear > 1 ? 2.4f : 1.1f;
        if (sp > mx) { s->vx = s->vx/sp*mx; s->vy = s->vy/sp*mx; }
        s->x += s->vx; s->y += s->vy;
    }
    // occasional bleat (panned), more when spooked
    if (bleat_cd > 0) bleat_cd--; else if (rnd(100) < (wolf.howl>0?40:6)) {
        int i = rnd(NS); play_pan(64 + rnd(5), INSTR_REED, 2, (sheep[i].x/SCREEN_W)*2-1, 14); bleat_cd = 8;
    }

#ifdef DE_TRACE
    watch("hour", "%d", hour); watch("phase", "%d", amb_phase);
    watch("howl", "%d", wolf.howl); watch("sel_act", "%d", routine(&town[sel], clk));
#endif
}

// ---- draw ------------------------------------------------------------------
static int lerpc(int a, int b, float t) { return a + (int)((b - a) * t); }
static void sky(float h) {                            // true-colour gradient by hour
    static const int K[5][3] = { {12,14,34}, {196,120,118}, {138,196,236}, {226,128,54}, {12,14,34} };
    float seg = h / 6.0f; int i = (int)seg; if (i > 3) i = 3; float f = seg - i;
    int r = lerpc(K[i][0],K[i+1][0],f), g = lerpc(K[i][1],K[i+1][1],f), b = lerpc(K[i][2],K[i+1][2],f);
    rectfill_rgb(0, 0, SCREEN_W, HUD_Y, (r<<16)|(g<<8)|b);
}
static void house(int x, int y, int col) {
    rectfill(x-4, y-2, 9, 7, CLR_BROWN);
    trifill(x-5, y-2, x+5, y-2, x, y-7, col);
}

void draw(void) {
    float h = clk;
    float bright = (h >= 6 && h <= 18) ? 0.2f + 0.8f * sin_deg((h-6)/12*180) : 0.16f;
    sky(h);
    if (bright < 0.4f) for (int i = 0; i < 30; i++) pset((i*53) % SCREEN_W, (i*29) % 70, CLR_WHITE);  // stars
    // ground
    int gr = (int)(54*bright)+18, gg = (int)(120*bright)+24, gb = (int)(48*bright)+16;
    rectfill_rgb(0, 96, SCREEN_W, HUD_Y-96, (gr<<16)|(gg<<8)|gb);

    // landmarks
    for (int i = 0; i < NT; i++) house(town[i].hx, town[i].hy, town[i].col);     // homes
    circfill(WELLX, WELLY, 5, CLR_DARK_GREY); circfill(WELLX, WELLY, 3, CLR_DARK_BLUE);   // well
    fillp(FILL_CHECKER, -1); rectfill(FIELDX-18, FIELDY-8, 60, 18, CLR_DARK_GREEN); fillp_reset();  // field
    rectfill(TAVX-8, TAVY-8, 18, 12, CLR_BROWN); print("u", TAVX-2, TAVY-7, CLR_YELLOW);  // tavern

    // sheep
    for (int i = 0; i < NS; i++) {
        circfill((int)sheep[i].x, (int)sheep[i].y, 2, CLR_WHITE);
        pset((int)sheep[i].x, (int)sheep[i].y - 2, CLR_DARK_GREY);
    }
    // wolf
    int wx = (int)wolf.x, wy = (int)wolf.y;
    print("W", wx-3, wy-3, wolf.howl > 0 ? CLR_RED : CLR_DARK_GREY);
    if (wolf.howl > 0) circ(wx, wy, 10 + (70-wolf.howl)/4, CLR_RED);     // howl ring

    // townsfolk + activity bubble
    for (int i = 0; i < NT; i++) {
        Town *t = &town[i]; int x = (int)t->x, y = (int)t->y;
        if (i == sel) rect(x-3, y-3, 7, 8, CLR_WHITE);
        print(str("%c", t->g), x-2, y-3, t->col);
        int loc = routine(t, clk);
        print(str("%c", ACTGLY[loc]), x-2, y-10, ACTCOL[loc]);
    }

    // HUD
    rectfill(0, HUD_Y, SCREEN_W, SCREEN_H-HUD_Y, CLR_BLACK);
    const char *PH[4] = { "night", "dawn", "day", "dusk" };
    print(str("%02d:%02d  %s", (int)h, (int)((h-(int)h)*60), PH[amb_phase<0?0:amb_phase]), 4, HUD_Y+2, CLR_WHITE);
    Town *s = &town[sel];
    print(str("%s: %s", s->name,
          routine(s,clk)==HOME?"sleeping":routine(s,clk)==WELL?"at the well":routine(s,clk)==FIELD?"working":"at the tavern"),
          92, HUD_Y+2, s->col);
    font(FONT_SMALL);
    print("click townsfolk to inspect    H howl    R reseed", 4, HUD_Y+12, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
