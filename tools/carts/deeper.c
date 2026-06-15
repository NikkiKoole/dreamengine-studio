#include "studio.h"
#ifdef DE_DUMPGRID
#include <stdio.h>     // debug: dump the generated grid as ASCII (reachability proof)
#endif

// DEEPER — an atmospheric descent. A procedurally-built vertical world of rooms
// and corridors that only ever goes down: wide halls, tight chambers, and the
// narrow shafts you drop through between them. You hop around and your footsteps
// make little noises. There's no fail state — just the going-down, and the sound
// of your steps in whatever space you're in.
//
// Move: arrows / WASD.   Hop: Z or Up.   (small hops — you descend, you don't climb)
//
// ─── THE REVERB SEAM ─────────────────────────────────────────────────────────
// The whole point of this cart (once the effects bus lands) is that your steps
// REFLECT the room: a tight corridor gives a dry, slappy tick; a great hall
// gives a long, lush tail. All the plumbing for that is already here:
//   • room_openness()  measures the air around the player → 0 (corridor) .. 1 (hall)
//   • footstep()       is the ONE place sound is made; the reverb/echo calls live
//                      there behind `#define ACOUSTICS`, driven by openness.
//   • the HUD meter (bottom-right) shows openness live, so the mapping can be
//     tuned by eye BEFORE the audio is wired.
// To turn the acoustics off again, flip ACOUSTICS to 0 — the cart still plays
// its footsteps dry, and the HUD meter still previews what the reverb tracks.
#define ACOUSTICS 1
// ─────────────────────────────────────────────────────────────────────────────

// ── tuning: movement (small hops — see platform-tiles.c for the full feel layer)
#define GRAV       0.50f
#define MAX_FALL   8.0f
#define RUN_ACCEL  0.7f
#define RUN_FRIC   0.55f
#define AIR_FRIC   0.90f
#define RUN_MAX    2.4f
#define JUMP_V    -4.9f      // deliberately small: a hop, not a climb
#define COYOTE     6
#define BUFFER     6
#define CUT_JUMP   0.40f
#define APEX_GRAV  0.55f
#define APEX_BAND  1.2f

// ── world ──────────────────────────────────────────────────────────────────
#define TILE     8
#define WORLD_W  40                 // = SCREEN_W/TILE → pure vertical scroll, no h-scroll
#define WORLD_H  240                // ~30 screens of descent before it regenerates deeper
#define WPX      (WORLD_W * TILE)
#define HPX      (WORLD_H * TILE)
#define PW 10
#define PH 14
#define MAXDUST 28
#define MAXMOTE 64                  // glowing pickups across one section
#define PROBE   18                  // how far room_openness() casts (tiles)

typedef struct { float x, y, vx, vy; int life, col; } Dust;
typedef struct { int x, y; bool got; } Mote;   // a glowing pickup (pixel coords)

static unsigned char grid[WORLD_H][WORLD_W];   // 1 = solid rock, 0 = air
static int   spawn_x, spawn_floor;             // tile coords of the top room
static Mote  mote[MAXMOTE];
static int   nmote, collected;
static int   treasure_x, treasure_y;           // the super-treasure in the bottom hall
static bool  treasure_got;
static int   hoards;                            // super-treasures claimed

static float px, py, vx, vy;
static int   face;
static bool  grounded;
static int   coyote, jbuf;
static float squash;
static float strideAcc;                        // distance walked since last step tick
static int   depthBase;                        // tiles descended in earlier (regenerated) sections
static int   flash;                            // section-change flash
static int   drip;                             // ambient-drip timer
static float openShown;                        // smoothed openness (HUD + acoustics)
static Dust  dust[MAXDUST];

// ── grid access (out-of-bounds = solid, so the player is fenced in) ──────────
static int  gat(int c, int r) { if (c < 0 || c >= WORLD_W || r < 0 || r >= WORLD_H) return 1; return grid[r][c]; }
static bool tile_solid(int t) { return t == 1; }

// ═══ GENERATION — a downward chain of rooms joined by vertical shafts you fall
//     through. The invariant that keeps it ALWAYS descendable: the connecting
//     shaft is a fixed-width "band" that is fully interior to BOTH rooms it
//     joins. So every room has a flat floor with exactly ONE clean drop-hole;
//     you land from the ceiling, walk the flat floor to the hole, and drop — no
//     climbing is ever required. Room sizes still vary wildly (hall / chamber /
//     vault) for the acoustic contrast the cart is about.
#define SHAFT_W 3                  // connection band width (player is ~1.3 tiles)
#define ROOM_MIN_W (SHAFT_W + 4)   // room must hold the band with margin on both sides

static void gen_world(void) {
    for (int r = 0; r < WORLD_H; r++)
        for (int c = 0; c < WORLD_W; c++) grid[r][c] = 1;
    nmote = 0;

    int band = WORLD_W / 2 - 1;    // left column of the shaft entering the next room
    int y    = 2;                  // top of the next room
    int guard = 0;
    bool first = true;

    while (y < WORLD_H - 30 && guard++ < 500) {
        int kind = rnd(10), rw, rh;
        if (kind < 4)      { rw = rnd_between(30, 38); rh = rnd_between(10, 16); }  // GREAT HALL — vast + tall (near full width)
        else if (kind < 6) { rw = rnd_between(12, 18); rh = rnd_between(6, 9);  }   // chamber — mid
        else               { rw = rnd_between(ROOM_MIN_W, 8); rh = rnd_between(4, 6); } // TIGHT vault — small + low
        if (rw < ROOM_MIN_W)  rw = ROOM_MIN_W;
        if (rw > WORLD_W - 2) rw = WORLD_W - 2;

        // place the room so the incoming band [band, band+SHAFT_W) sits inside it
        // with ≥1 tile of wall margin on each side
        int rxmin = band + SHAFT_W + 1 - rw;  if (rxmin < 1) rxmin = 1;
        int rxmax = band - 1;                 if (rxmax > WORLD_W - 1 - rw) rxmax = WORLD_W - 1 - rw;
        int rx = (rxmax < rxmin) ? clamp(band - (rw - SHAFT_W) / 2, 1, WORLD_W - 1 - rw)
                                 : rnd_between(rxmin, rxmax + 1);

        // carve the room
        for (int yy = y; yy < y + rh; yy++)
            for (int xx = rx; xx < rx + rw; xx++) grid[yy][xx] = 0;

        if (first) { spawn_x = band + SHAFT_W / 2; spawn_floor = y + rh; first = false; }

        // pick the OUTGOING band — interior to this room, ≥1 tile margin each side
        int nband = rnd_between(rx + 1, rx + rw - SHAFT_W);

        // scatter a few glowing motes — floating just above the floor, off to the
        // sides so they reward exploring the room rather than the straight drop
        int want = rnd(3);                           // 0..2 per room
        for (int m = 0; m < want && nmote < MAXMOTE; m++) {
            int mc = rnd_between(rx + 1, rx + rw - 1);
            if (mc >= nband - 1 && mc <= nband + SHAFT_W) continue;  // not in the drop
            mote[nmote++] = (Mote){ mc * TILE + TILE / 2,
                                    (y + rh - 1) * TILE - 2, false };
        }

        // a few low rubble steps for hop-interest — never tall enough to trap you,
        // and never over the outgoing hole (the shaft carve below clears it anyway)
        if (rw >= 11) {
            int steps = rnd(3);
            for (int s = 0; s < steps; s++) {
                int sw = rnd_between(2, 5);
                int sx = rnd_between(rx + 1, rx + rw - sw);
                int sh = rnd_between(1, 2);                 // ≤2 tall, always hoppable
                for (int yy = y + rh - sh; yy < y + rh; yy++)
                    for (int xx = sx; xx < sx + sw; xx++) grid[yy][xx] = 1;
            }
        }

        // carve the outgoing shaft clear through the WHOLE room height (ceiling →
        // floor → down to the next room's top). Carving from the ceiling — not
        // just the floor — guarantees nothing (rubble!) ever lids the drop-hole.
        int gap  = rnd_between(1, 12);
        int ynext = y + rh + gap;
        if (ynext > WORLD_H - 2) ynext = WORLD_H - 2;
        for (int yy = y; yy <= ynext; yy++)
            for (int xx = nband; xx < nband + SHAFT_W; xx++) grid[yy][xx] = 0;

        band = nband;
        y    = ynext;
    }

    // ── THE BOTTOM: a grand treasure hall ────────────────────────────────────
    // A wide chamber (so its reverb is the biggest in the section) holding a
    // hoard of motes and one SUPER TREASURE at its centre. The exit shaft is off
    // to one side, so you land, cross the hall to claim the prize, then drop on.
    int rh = 12, rw = 30;
    if (rw > WORLD_W - 2)   rw = WORLD_W - 2;
    if (y + rh > WORLD_H - 2) rh = WORLD_H - 2 - y;
    int rxmin = band + SHAFT_W + 1 - rw; if (rxmin < 1) rxmin = 1;
    int rxmax = band - 1;                if (rxmax > WORLD_W - 1 - rw) rxmax = WORLD_W - 1 - rw;
    int rx = (rxmax < rxmin) ? clamp(band - (rw - SHAFT_W) / 2, 1, WORLD_W - 1 - rw)
                             : rnd_between(rxmin, rxmax + 1);
    for (int yy = y; yy < y + rh; yy++)
        for (int xx = rx; xx < rx + rw; xx++) grid[yy][xx] = 0;

    int cxc = rx + rw / 2;
    for (int m = 0; m < 8 && nmote < MAXMOTE; m++)              // a flanking hoard
        mote[nmote++] = (Mote){ (cxc + (m - 4) * 2) * TILE + TILE / 2,
                                (y + rh - 1) * TILE - 2, false };
    treasure_x = cxc * TILE + TILE / 2;                        // the prize, dead centre
    treasure_y = (y + rh - 1) * TILE - 4;
    treasure_got = false;

    // final shaft to the world floor, off at the right edge (regen fallback)
    int fb = clamp(rx + rw - 2 - SHAFT_W, rx + 1, rx + rw - 1 - SHAFT_W);
    for (int yy = y; yy < WORLD_H; yy++)
        for (int xx = fb; xx < fb + SHAFT_W; xx++) grid[yy][xx] = 0;
}

// ═══ ACOUSTICS — measure the air around the player. 0 = boxed-in corridor,
//     1 = wide-open hall. This is the value the reverb/echo will be driven by.
static float room_openness(void) {
    int cc = ((int)px + PW / 2) / TILE;
    int cr = ((int)py + PH / 2) / TILE;
    // WIDTH: sample at the player's row AND a couple tiles up, take the WIDER. Near the floor
    // the horizontal probe gets blocked by low rubble steps (≤2 tiles), so a hall used to read
    // narrow while standing and only open up mid-jump — but jumps are silent, so footsteps never
    // heard the hall. Sampling above the rubble fixes it: the room's true width registers whether
    // you're standing, landing, or hopping.
    int hbest = 0;
    for (int dr = 0; dr <= 4; dr += 2) {            // rows: at center, 2-up, 4-up (clears ≤2-tile rubble)
        int row = cr - dr; if (row < 0) continue;
        int l = 0, r = 0;
        while (l < PROBE && !gat(cc - l - 1, row)) l++;
        while (r < PROBE && !gat(cc + r + 1, row)) r++;
        if (l + r > hbest) hbest = l + r;
    }
    int up = 0, down = 0;
    while (up   < PROBE && !gat(cc, cr - up   - 1)) up++;
    while (down < PROBE && !gat(cc, cr + down + 1)) down++;
    float horiz = hbest / (float)(2 * PROBE);        // halls are wide
    float vert  = (up + down) / (float)(2 * PROBE);  // shafts are tall
    return clamp(horiz * 0.82f + vert * 0.18f, 0.0f, 1.0f);   // width dominates → tight spaces read truly dry
}

// ═══ ROOM ACOUSTICS — driven once per frame from a smoothed openness, so the
//     tail glides open as you walk a corridor into a hall (no per-step zipper).
//     Footsteps below just trigger dry hits INTO this already-configured space.
static float acLast = -1.0f;     // last openness we reconfigured the bus at
static void apply_acoustics(void) {
#if ACOUSTICS
    float o  = openShown;
    float oc = o * o;                         // CURVE: tight/mid spaces stay dry, only true halls bloom — exaggerates the contrast
    float d = o - acLast; if (d < 0) d = -d;
    if (acLast < 0 || d > 0.02f) {            // reconfigure on a real change (reverb jumps; echo TIME slews itself)
        acLast = o;
        reverb(0.05f + oc * 0.93f, 0.62f - oc * 0.34f);            // tight = tiny DEAD room; hall = vast, long, bright tail
        echo(60 + (int)(oc * 520), oc * 0.55f, 0.20f + oc * 0.50f); // tight = no echo; hall = a long resonant slap-back
    }
    // sends: corridors BONE dry, halls drenched (cheap to set every frame)
    instrument_reverb(INSTR_NOISE,    0.02f + oc * 0.93f);
    instrument_reverb(INSTR_MEMBRANE, 0.02f + oc * 0.93f);
    instrument_echo  (INSTR_NOISE,    oc * 0.55f);
    instrument_reverb(INSTR_SINE,     0.20f + oc * 0.78f);  // the ambient drip blooms in the halls
#else
    (void)acLast;
#endif
}

// ═══ THE ONE PLACE FOOTSTEP SOUND IS MADE. kind: 0 = step, 1 = land, 2 = hop.
static void footstep(int kind) {
    if      (kind == 0) hit(rnd_between(50, 57), INSTR_NOISE,    2, 18);   // soft tick
    else if (kind == 1) hit(rnd_between(34, 39), INSTR_MEMBRANE, 3, 60);   // landing thud
    else                hit(rnd_between(44, 49), INSTR_NOISE,    2, 22);   // scuff
}

// ── feel: dust ───────────────────────────────────────────────────────────────
static void spawn_dust(float x, float y, int n) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < MAXDUST; i++)
            if (dust[i].life <= 0) {
                dust[i] = (Dust){ x + rnd_between(-4, 5), y,
                    rnd_float_between(-1.0f, 1.0f), rnd_float_between(-1.4f, -0.1f),
                    rnd_between(8, 14), (k & 1) ? CLR_DARK_GREY : CLR_MEDIUM_GREY };
                break;
            }
}

// ═══ MOVER — per-axis resolution against the grid (see platform-tiles.c) ═════
static void move_x(void) {
    px += vx;
    if (px < 0)          { px = 0; vx = 0; }
    if (px > WPX - PW)   { px = WPX - PW; vx = 0; }
    int top = (int)py / TILE, bot = ((int)(py + PH) - 1) / TILE;
    if (vx > 0) {
        int col = ((int)(px + PW) - 1) / TILE;
        for (int r = top; r <= bot; r++)
            if (tile_solid(gat(col, r))) { px = col * TILE - PW; vx = 0; break; }
    } else if (vx < 0) {
        int col = (int)px / TILE;
        for (int r = top; r <= bot; r++)
            if (tile_solid(gat(col, r))) { px = (col + 1) * TILE; vx = 0; break; }
    }
}
static void move_y(void) {
    py += vy;
    grounded = false;
    int left = (int)px / TILE, right = ((int)(px + PW) - 1) / TILE;
    if (vy >= 0) {
        int row = (int)(py + PH) / TILE;
        for (int c = left; c <= right; c++)
            if (tile_solid(gat(c, row))) { py = row * TILE - PH; grounded = true; vy = 0; break; }
    } else {
        int row = (int)py / TILE;
        for (int c = left; c <= right; c++)
            if (tile_solid(gat(c, row))) { py = (row + 1) * TILE; vy = 0; break; }
    }
}

static void place_at_spawn(void) {
    px = spawn_x * TILE;
    py = spawn_floor * TILE - PH;
    vx = vy = 0; face = 1; grounded = true;
}

void init(void) {
    gen_world();
    place_at_spawn();
#ifdef DE_DUMPGRID
    for (int r = 0; r < WORLD_H; r++) {
        char ln[WORLD_W + 1];
        for (int c = 0; c < WORLD_W; c++) ln[c] = grid[r][c] ? '#' : '.';
        ln[WORLD_W] = 0;
        fprintf(stderr, "GRID %3d %s\n", r, ln);
    }
#endif
}

void update(void) {
#ifdef DE_TRACE
    watch("p", "py=%.0f depth=%d open=%.2f gnd=%d", py, depthBase + (int)py / TILE, room_openness(), grounded);
#endif
    if (flash > 0) flash--;

    // room acoustics track the space continuously (footsteps fire into them)
    openShown = lerp(openShown, room_openness(), 0.12f);
    apply_acoustics();

    // a slow drip, only in open spaces — lets the hall reverb sing in the quiet
    drip++;
    if (openShown > 0.5f && drip % 170 == 0) hit(rnd_between(74, 86), INSTR_SINE, 3, 45);

    // horizontal
    int dir = (btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0);
    if (dir != 0) { vx = clamp(vx + dir * RUN_ACCEL, -RUN_MAX, RUN_MAX); face = dir; }
    else { vx *= grounded ? RUN_FRIC : AIR_FRIC; if (vx > -0.1f && vx < 0.1f) vx = 0; }

    // hop: coyote + buffer + variable height
    bool was_gr = grounded;
    if (was_gr)        coyote = COYOTE;
    else if (coyote)   coyote--;
    if (btnp(0, BTN_A) || btnp(0, BTN_UP)) jbuf = BUFFER;
    else if (jbuf)                          jbuf--;
    if (jbuf > 0 && (grounded || coyote > 0)) {
        vy = JUMP_V; jbuf = 0; coyote = 0; squash = -0.6f;
        spawn_dust(px + PW / 2, py + PH, 3);
        footstep(2);
    }
    if ((btnr(0, BTN_A) || btnr(0, BTN_UP)) && vy < 0) vy *= CUT_JUMP;

    float gg = GRAV;
    if (!grounded && vy > -APEX_BAND && vy < APEX_BAND) gg *= APEX_GRAV;
    vy += gg;
    if (vy > MAX_FALL) vy = MAX_FALL;

    move_x();
    move_y();

    // landing
    if (!was_gr && grounded) {
        squash = 1.0f;
        spawn_dust(px + PW / 2, py + PH, 5);
        footstep(1);
        shake(0.5f);
        strideAcc = 0;
    }
    squash = lerp(squash, 0.0f, 0.22f);

    // walking step ticks
    if (grounded && (vx > 0.3f || vx < -0.3f)) {
        strideAcc += (vx < 0 ? -vx : vx);
        if (strideAcc > 13.0f) { footstep(0); strideAcc = 0; }
    }

    // collect glowing motes — a bright chime that rings into the room's reverb
    for (int i = 0; i < nmote; i++) {
        if (mote[i].got) continue;
        if (mote[i].x >= px - 2 && mote[i].x <= px + PW + 2 &&
            mote[i].y >= py - 2 && mote[i].y <= py + PH + 2) {
            mote[i].got = true; collected++;
            static const int penta[5] = { 0, 3, 5, 7, 10 };
            int n = 72 + penta[collected % 5] + 12 * ((collected / 5) % 2);
            instrument_reverb(INSTR_MALLET, 0.78f);   // rings into the room's tail
            instrument_echo(INSTR_MALLET, 0.30f);     // + a few repeats so even a
            hit(n, INSTR_MALLET, 4, 220);             //   tight corridor shimmers
            spawn_dust(mote[i].x, mote[i].y, 6);
        }
    }

    // the SUPER TREASURE — a grand arpeggio drenched in the hall's big reverb
    if (!treasure_got &&
        treasure_x >= px - 3 && treasure_x <= px + PW + 3 &&
        treasure_y >= py - 5 && treasure_y <= py + PH + 5) {
        treasure_got = true; hoards++; collected += 5;
        static const int arp[5] = { 0, 4, 7, 11, 16 };
        instrument_reverb(INSTR_MALLET, 0.92f);
        instrument_echo(INSTR_MALLET, 0.45f);
        for (int k = 0; k < 5; k++) schedule_hit(k * 95, 67 + arp[k], INSTR_MALLET, 5, 420);
        shake(2.2f);
        spawn_dust(treasure_x, treasure_y, 14);
    }

    // reached the bottom → fall through into a fresh, deeper section
    if (py > (WORLD_H - 5) * TILE) {
        depthBase += WORLD_H;
        gen_world();
        place_at_spawn();
        flash = 5;
    }
}

// ── lighting: walls dim with distance from the player (a torch-glow) ─────────
static int wall_color(int dist) {
    if (dist <= 3) return CLR_LIGHT_GREY;
    if (dist <= 6) return CLR_MEDIUM_GREY;
    if (dist <= 9) return CLR_DARK_GREY;
    return CLR_BROWNISH_BLACK;
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    if (flash > 0) cls(CLR_BLACK);

    follow((int)px + PW / 2, (int)py + PH / 2, WPX, HPX);

    int pcol = ((int)px + PW / 2) / TILE;
    int prow = ((int)py + PH / 2) / TILE;
    int r0 = prow - 16, r1 = prow + 16;
    if (r0 < 0) r0 = 0; if (r1 >= WORLD_H) r1 = WORLD_H - 1;

    // walls, lit by distance; a brighter cap line where rock meets air (a ledge)
    for (int r = r0; r <= r1; r++)
        for (int c = 0; c < WORLD_W; c++) {
            if (!grid[r][c]) continue;
            int dx = c - pcol, dy = r - prow;
            int dist = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
            int x = c * TILE, y = r * TILE;
            rectfill(x, y, TILE, TILE, wall_color(dist));
            if (!gat(c, r - 1) && dist <= 9) rectfill(x, y, TILE, 1, CLR_LIGHT_PEACH);
        }

    // the super treasure — a big pulsing crystal in the bottom hall
    if (!treasure_got && treasure_y >= r0 * TILE - 14 && treasure_y <= r1 * TILE + 14) {
        int tp = blink(14) ? 1 : 0;
        circ(treasure_x, treasure_y, 7 + tp, CLR_ORANGE);
        circ(treasure_x, treasure_y, 5, CLR_YELLOW);
        trifill(treasure_x, treasure_y - 6, treasure_x - 4, treasure_y, treasure_x + 4, treasure_y, CLR_WHITE);
        trifill(treasure_x - 4, treasure_y, treasure_x + 4, treasure_y, treasure_x, treasure_y + 6, CLR_LIGHT_PEACH);
        pset(treasure_x - 1, treasure_y - 2, CLR_WHITE);
    }

    // glowing motes — pulsing little lights that lure you off the straight drop
    int pulse = blink(22) ? 1 : 0;
    for (int i = 0; i < nmote; i++) {
        if (mote[i].got || mote[i].y < r0 * TILE - 8 || mote[i].y > r1 * TILE + 8) continue;
        circ(mote[i].x, mote[i].y, 3 + pulse, CLR_ORANGE);
        circfill(mote[i].x, mote[i].y, 2, CLR_YELLOW);
        pset(mote[i].x, mote[i].y - 1, CLR_WHITE);
    }

    // dust
    for (int i = 0; i < MAXDUST; i++)
        if (dust[i].life > 0) pset((int)dust[i].x, (int)dust[i].y, dust[i].col);

    // player: a little figure with a warm lantern
    int sq = (int)(squash * 3.0f);
    int w  = PW + (sq > 0 ? sq : 0);
    int h  = PH - sq;
    int x  = (int)(px + 0.5f) - (w - PW) / 2;
    int y  = (int)(py + 0.5f) + (PH - h);
    rectfill(x, y + 4, w, h - 4, CLR_BROWN);                 // cloak
    rectfill(x + 1, y, w - 2, 5, CLR_LIGHT_PEACH);           // head
    int lx = face > 0 ? x + w : x - 2;
    rectfill(lx, y + 5, 2, 3, CLR_YELLOW);                   // lantern
    circ(lx + 1, y + 6, 2, CLR_ORANGE);

    // ── HUD ──────────────────────────────────────────────────────────────────
    camera(0, 0);
    int depth = depthBase + (int)py / TILE;
    print(str("DEPTH %dm", depth), 4, 4, CLR_LIGHT_GREY);
    circfill(8, 18, 2, CLR_YELLOW);
    print(str("%d", collected), 14, 14, CLR_LIGHT_PEACH);

    // acoustics meter — the space the reverb/echo bus is currently tuned to
    int mw = 54, mx = SCREEN_W - mw - 6, my = 5;
    print_right(openShown < 0.33f ? "corridor" : openShown < 0.66f ? "chamber" : "hall",
                mx - 4, my, CLR_DARK_GREY);
    rect(mx, my, mw, 6, CLR_DARK_GREY);
    rectfill(mx + 1, my + 1, (int)((mw - 2) * openShown), 4,
             openShown < 0.33f ? CLR_DARK_GREEN : openShown < 0.66f ? CLR_GREEN : CLR_LIME_GREEN);
#if !ACOUSTICS
    print_right("space", mx + mw, my + 9, CLR_DARK_GREY);
#endif
}
