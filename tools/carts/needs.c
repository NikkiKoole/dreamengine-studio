/* de:meta
{
  "slug": "needs",
  "title": "needs",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "finite-state-ai",
    "pathfinding",
    "sonification"
  ],
  "lineage": "Directly cites RogueBasin 'Need-driven AI' and 'Variety in NPC behavior'; implements utility-scoring AI with per-creature personality weights and Dijkstra distance-field pathing, plus positionally panned pentatonic sound as a data readout.",
  "description": "Need-driven (utility) AI, the @ colony (RogueBasin: Need driven AI + Variety in NPC behavior). Seven creatures each carry four draining needs (hunger, thirst, energy, social); every decision they score each need by urgency = (100-value)*weight and go satisfy the most urgent -- food f, water ~, a bed =, or the nearest friend. No scripts: behaviour falls out of the meters. PERSONALITY (per-creature weights + drain rates) makes a glutton, camel, sloth, loner, socialite live visibly different lives from one rule set. Pathing reuses Dijkstra fields (one per resource). A thought-bubble glyph over each creature shows its current goal; click one to inspect its need bars. SOUND: every satisfied need rings a short pentatonic motif (munch/gulp/snore/chime), pitched per creature and panned by position -> the colony's resolving needs become a generative texture. R reseeds."
}
de:meta */
#include "studio.h"

// NEEDS — need-driven (utility) AI, the @ colony.
// Every creature carries four needs that drain over time (hunger, thirst,
// energy, social). Each decision it scores every need by URGENCY = (100-value)
// * personality_weight, and goes to satisfy the most urgent one — food f,
// water ~, a bed =, or the nearest friend. No scripts, no schedules: behaviour
// falls out of the meters. PERSONALITY (per-creature weights + drain rates)
// makes a glutton, a camel, a loner and a socialite live visibly different
// lives from the same rules ("variety in NPC behaviour").
//   Pathing reuses the Dijkstra-map idea: one distance field per resource type,
// flooded once; a creature just rolls downhill on the field for its current need.
//   SOUND: every satisfied need rings a short pentatonic motif (munch / gulp /
// snore / chime), pitched per creature and PANNED by position — so the colony's
// resolving needs become an evolving generative texture. (RogueBasin: Need
// driven AI + Variety in NPC behavior.)
//
// Click a creature to inspect its needs + current thought.  R reseeds.

#define TILE 8
#define GW 40
#define GH 20
#define PANEL_Y 160
#define NC 7
#define INF 1e9f

enum { HUNGER, THIRST, ENERGY, SOCIAL, NEEDS };
static const char *NEEDNM[NEEDS]  = { "hunger", "thirst", "energy", "social" };
static const int   NEEDCOL[NEEDS] = { CLR_ORANGE, CLR_BLUE, CLR_INDIGO, CLR_PINK };
static const char  NEEDGLY[NEEDS] = { 'f', '~', 'z', 3 };   // 3 = heart glyph

#define BASE_DECAY 0.045f
#define FILL       1.4f
#define SATISFIED  96.0f
#define WANDER_URG 28.0f      // below this urgency, just wander

static unsigned char wall[GH][GW];
static signed char  res[GH][GW];           // -1 none, else need index 0..2 (food/water/bed)
static float fld[3][GH][GW];               // distance field per resource type (social is dynamic)

typedef struct {
    int x, y, px, py; float t;             // cell pos + smooth-move fraction
    float need[NEEDS];
    int action;                            // need being served, or -1 = wander
    int tx, ty;                            // wander target
    int mcd, dcd, sndcd;
    float w[NEEDS], dec[NEEDS];            // personality: weights + drain rates
    int col; char glyph; const char *name;
    int base;                              // base MIDI pitch (pentatonic)
} Crea;
static Crea cr[NC];
static int sel;

// ---- personalities ---------------------------------------------------------
static const struct { const char *name; char g; int col;
                      float w[NEEDS], dec[NEEDS]; } PERS[NC] = {
    { "glutton",   'G', CLR_RED,        {2.0f,1,1,1},   {1.9f,1,1,1} },
    { "camel",     'C', CLR_YELLOW,     {1,1.5f,1,1},   {1,0.4f,1,1} },
    { "sloth",     'S', CLR_BROWN,      {1,1,1.8f,0.8f},{1,1,1.7f,1} },
    { "loner",     'L', CLR_DARK_GREEN, {1,1,1,0.3f},   {1,1,1,0.5f} },
    { "socialite", 'O', CLR_PINK,       {1,1,1,2.0f},   {1,1,1,1.9f} },
    { "worker",    'W', CLR_LIGHT_GREY, {1.2f,1.2f,1,1},{1.3f,1.2f,1.4f,1} },
    { "wanderer",  'A', CLR_LIGHT_PEACH, {1,1,1,1},     {1,1,1,1} },
};
static const int PENTA[5] = { 0, 2, 4, 7, 9 };

// ---- tiny panned-voice pool ------------------------------------------------
typedef struct { int h, ttl; } Voice;
static Voice voices[16];
static void play_pan(int midi, int instr, int vol, float pan, int ttl) {
    for (int i = 0; i < 16; i++) if (voices[i].ttl <= 0) {
        voices[i].h = note_on(midi, instr, vol);
        note_pan(voices[i].h, pan);
        voices[i].ttl = ttl;
        return;
    }
}
static void voices_tick(void) {
    for (int i = 0; i < 16; i++) if (voices[i].ttl > 0 && --voices[i].ttl == 0) note_off(voices[i].h);
}

// ---- dijkstra distance fields (flood once from each resource type) ----------
static bool blocked(int x, int y) { return x < 0 || x >= GW || y < 0 || y >= GH || wall[y][x]; }
static void flood(float f[GH][GW], int type) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        f[y][x] = (res[y][x] == type) ? 0 : INF;
    bool ch = true; int g = 0;
    while (ch && g++ < GW * GH) {
        ch = false;
        for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
            if (wall[y][x]) continue;
            float b = f[y][x];
            for (int k = 0; k < 4; k++) {
                int nx = x + (k==0) - (k==1), ny = y + (k==2) - (k==3);
                if (!blocked(nx, ny) && f[ny][nx] + 1 < b) b = f[ny][nx] + 1;
            }
            if (b < f[y][x]) { f[y][x] = b; ch = true; }
        }
    }
}

// ---- setup -----------------------------------------------------------------
static void put(int x, int y, int t) { res[y][x] = t; }
static void reseed(void) {
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) { wall[y][x] = 0; res[y][x] = -1; }
    // a few obstacles so paths have to bend
    for (int y = 5; y < 14; y++) wall[y][20] = 1;
    wall[9][20] = 0; wall[10][20] = 0;
    for (int x = 10; x < 16; x++) wall[7][x] = 1;
    for (int x = 26; x < 32; x++) wall[13][x] = 1;
    // resources: food (3), water pond, beds (dormitory)
    put(6,3,HUNGER); put(34,4,HUNGER); put(22,17,HUNGER);
    put(6,15,THIRST); put(7,15,THIRST); put(6,16,THIRST); put(7,16,THIRST);
    put(34,16,ENERGY); put(35,16,ENERGY); put(36,16,ENERGY); put(34,17,ENERGY); put(35,17,ENERGY);
    flood(fld[HUNGER], HUNGER); flood(fld[THIRST], THIRST); flood(fld[ENERGY], ENERGY);

    for (int i = 0; i < NC; i++) {
        Crea *c = &cr[i];
        c->name = PERS[i].name; c->glyph = PERS[i].g; c->col = PERS[i].col;
        for (int n = 0; n < NEEDS; n++) { c->w[n] = PERS[i].w[n]; c->dec[n] = PERS[i].dec[n]; c->need[n] = 55 + rnd(40); }
        c->base = 48 + PENTA[i % 5] + 12 * (i / 5);
        do { c->x = 2 + rnd(GW - 4); c->y = 2 + rnd(GH - 4); } while (wall[c->y][c->x] || res[c->y][c->x] >= 0);
        c->px = c->x; c->py = c->y; c->t = 1;
        c->action = -1; c->dcd = rnd(30); c->mcd = rnd(8); c->sndcd = 0;
        c->tx = c->x; c->ty = c->y;
    }
}

void init(void) {
    reverb(0.45f, 0.5f);          // a little room (set-and-hold; never per-frame)
    reseed();
}

// ---- AI --------------------------------------------------------------------
static int nearest_other(Crea *c) {
    int best = -1; float bd = INF;
    for (int i = 0; i < NC; i++) {
        Crea *o = &cr[i]; if (o == c) continue;
        float d = (o->x - c->x) * (o->x - c->x) + (o->y - c->y) * (o->y - c->y);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static void decide(Crea *c) {
    int bn = -1; float bu = WANDER_URG;
    for (int n = 0; n < NEEDS; n++) {
        float u = (100 - c->need[n]) * c->w[n];
        if (u > bu) { bu = u; bn = n; }
    }
    c->action = bn;
    if (bn == -1) { c->tx = 2 + rnd(GW - 4); c->ty = 2 + rnd(GH - 4); }   // wander
}
// one cell toward the lowest neighbour of a field (8-dir)
static void step_field(Crea *c, float f[GH][GW]) {
    float best = f[c->y][c->x]; int bx = c->x, by = c->y;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (!dx && !dy) continue;
        int nx = c->x + dx, ny = c->y + dy;
        if (blocked(nx, ny)) continue;
        if (dx && dy && (blocked(c->x+dx, c->y) || blocked(c->x, c->y+dy))) continue;
        if (f[ny][nx] < best) { best = f[ny][nx]; bx = nx; by = ny; }
    }
    c->px = c->x; c->py = c->y; c->x = bx; c->y = by;
}
static void step_toward(Crea *c, int gx, int gy) {
    int dx = (gx > c->x) - (gx < c->x), dy = (gy > c->y) - (gy < c->y);
    int nx = c->x + dx, ny = c->y + dy;
    if (blocked(nx, ny)) { if (!blocked(c->x+dx, c->y)) ny = c->y; else if (!blocked(c->x, c->y+dy)) nx = c->x; else return; }
    c->px = c->x; c->py = c->y; c->x = nx; c->y = ny;
}

// play the motif for a satisfied need, pitched + panned for this creature
static void need_sound(Crea *c, int n) {
    if (c->sndcd > 0) return;
    float pan = (float)c->x / GW * 2 - 1;
    if      (n == HUNGER) play_pan(c->base + 12, INSTR_MALLET, 3, pan, 10);   // munch
    else if (n == THIRST) play_pan(c->base + 19, INSTR_PLUCK,  3, pan, 10);   // gulp blip
    else if (n == ENERGY) play_pan(c->base - 12, INSTR_SINE,   2, pan, 44);   // snore pad
    else                  play_pan(c->base + 7,  INSTR_FM,     3, pan, 16);   // social chime
    c->sndcd = 70;
}

void update(void) {
    voices_tick();
    if (keyp('R')) reseed();
    if (mouse_pressed(0) && mouse_y() < PANEL_Y) {
        int mx = mouse_x() / TILE, my = mouse_y() / TILE, best = -1, bd = 9999;
        for (int i = 0; i < NC; i++) { int d = abs(cr[i].x-mx)+abs(cr[i].y-my); if (d < bd) { bd = d; best = i; } }
        if (best >= 0) sel = best;
    }

    for (int i = 0; i < NC; i++) {
        Crea *c = &cr[i];
        if (c->sndcd > 0) c->sndcd--;
        if (c->t < 1) c->t += 0.16f;
        // needs drain
        for (int n = 0; n < NEEDS; n++) { c->need[n] -= BASE_DECAY * c->dec[n]; if (c->need[n] < 0) c->need[n] = 0; }

        if (c->dcd > 0) c->dcd--; else { decide(c); c->dcd = 24; }   // re-evaluate (allows preemption)

        // are we AT the thing we need? -> satisfy (and stay put)
        bool serving = false;
        if (c->action >= HUNGER && c->action <= ENERGY) {
            if (res[c->y][c->x] == c->action) serving = true;
        } else if (c->action == SOCIAL) {
            int o = nearest_other(c);
            if (o >= 0 && abs(cr[o].x-c->x) + abs(cr[o].y-c->y) <= 1) serving = true;
        }

        if (serving) {
            float before = c->need[c->action];
            c->need[c->action] += FILL;
            if (before < SATISFIED && c->need[c->action] >= SATISFIED) { need_sound(c, c->action); c->dcd = 0; }
            if (c->need[c->action] > 100) c->need[c->action] = 100;
        } else if (c->mcd > 0) c->mcd--; else {                       // move
            c->mcd = 6;
            if (c->action >= HUNGER && c->action <= ENERGY) step_field(c, fld[c->action]);
            else if (c->action == SOCIAL) { int o = nearest_other(c); if (o >= 0) step_toward(c, cr[o].x, cr[o].y); }
            else { if (c->x == c->tx && c->y == c->ty) decide(c); else step_toward(c, c->tx, c->ty); }
        }
    }

#ifdef DE_TRACE
    Crea *s = &cr[sel];
    watch("sel", "%s", s->name); watch("action", "%d", s->action);
    watch("hunger", "%d", (int)s->need[HUNGER]); watch("social", "%d", (int)s->need[SOCIAL]);
#endif
}

// ---- draw ------------------------------------------------------------------
static void glyph(char ch, int cx, int cy, int col) { print(str("%c", ch), cx * TILE + 1, cy * TILE, col); }

void draw(void) {
    cls(CLR_BLACK);
    // map: walls, resources
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
        if (wall[y][x]) { rectfill(x*TILE, y*TILE, TILE, TILE, CLR_DARKER_GREY); continue; }
        if (res[y][x] == HUNGER) glyph('f', x, y, CLR_ORANGE);
        else if (res[y][x] == THIRST) rectfill(x*TILE, y*TILE, TILE, TILE, CLR_DARK_BLUE);
        else if (res[y][x] == ENERGY) glyph('=', x, y, CLR_BROWN);
    }
    // creatures + their "thought" bubble
    for (int i = 0; i < NC; i++) {
        Crea *c = &cr[i];
        float fx = lerp(c->px, c->x, c->t), fy = lerp(c->py, c->y, c->t);
        int sx = (int)(fx*TILE), sy = (int)(fy*TILE);
        if (i == sel) rect(sx-1, sy-1, TILE+1, TILE+1, CLR_WHITE);
        print(str("%c", c->glyph), sx+1, sy, c->col);
        if (c->action >= 0) print(str("%c", NEEDGLY[c->action]), sx+1, sy-7, NEEDCOL[c->action]);
    }

    // panel: selected creature's needs
    rectfill(0, PANEL_Y, SCREEN_W, SCREEN_H-PANEL_Y, CLR_DARKER_BLUE);
    Crea *s = &cr[sel];
    print(str("%c %s", s->glyph, s->name), 4, PANEL_Y+3, s->col);
    print(s->action < 0 ? "wandering" : str("-> %s", NEEDNM[s->action]),
          110, PANEL_Y+3, s->action < 0 ? CLR_MEDIUM_GREY : NEEDCOL[s->action]);
    for (int n = 0; n < NEEDS; n++) {
        int by = PANEL_Y + 14 + n * 6;
        print(str("%c", NEEDGLY[n]), 4, by-1, NEEDCOL[n]);
        rect(14, by, 62, 5, CLR_DARK_GREY);
        rectfill(15, by+1, (int)(60 * s->need[n] / 100), 3, NEEDCOL[n]);
    }
    font(FONT_SMALL);
    print(str("click a creature to inspect    R reseed    %d colonists", NC), 110, PANEL_Y+16, CLR_MEDIUM_GREY);
    print("needs drain -> they seek food f / water ~ / bed = / a friend; sound = a need just met", 110, PANEL_Y+24, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
