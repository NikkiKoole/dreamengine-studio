/* de:meta
{
  "slug": "tonnetz",
  "title": "tonnetz",
  "status": "active",
  "created": "2026-10-01",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "generative-sequencer"
  ],
  "homage": "Ornament & Crime's Harrington 1200 and Automatonnetz (Patrick Dowling, Tim Churches; MIT): neo-Riemannian Tonnetz moves on a Eurorack module, here as a map you click and a grid that plays itself.",
  "lineage": "docs/design/open-source-audio-toys.md §2.2. The moves, the O&C move table and the vector automaton live in runtime/tonnetz.h (ported, MIT notice kept); this cart is the map, the sound and the mouse. We had a Tonnetz LAYOUT (scalegrid's HEX mode) but not the MOVES.",
  "description": {
    "summary": "Chords are places on a map. Click a neighbour and exactly one note moves, so every path sounds smooth. Or let a 5x5 grid of moves walk the map by itself.",
    "detail": "Every major and minor chord is a triangle on a grid of notes: up-pointing = major, down-pointing = minor. The lit triangle is the chord you hear. Its three edge-neighbours are the three Riemann moves: P (C to C minor), L (C to E minor), R (C to A minor), and each one moves exactly ONE note of the chord by a semitone or a tone. The pad glides only that voice, so you hear the one note travel while the other two hold. Click any neighbour, or drag a path across the map; click a far-away chord and it jumps there with the least motion it can. On the right is the AUTOMATON (after O&C's Automatonnetz): 25 cells, each holding a move. A cursor jumps across the grid by a vector you set, whole cells plus a fraction, wrapping around, and fires the move of each new cell it lands in, so a few clicks make a progression that wanders for minutes without repeating. The lanes at the bottom draw the three voices, so you can SEE which one moved.",
    "controls": "Map: hover to see a chord, click to go there, drag to walk a path. Keys P L R N S H = the moves, 0 = back to C. Automaton: left-click a cell = next move, right-click = previous, middle-click or D (hovering) = DICE (the cell re-rolls after it fires). DX / DY < > or the mouse wheel over them set the vector. SPACE = run/stop, RATE = how often it steps, ARP = arpeggiate the chord, X = random grid, C = clear grid."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "tonnetz.h"
#include "patgen.h"
#include <math.h>
#include <stdlib.h>

// TONNETZ — chords as places. See runtime/tonnetz.h for the moves; this file is the map, the sound
// and the mouse.
//
//   the pad    three held voices (note_on), one per chord VOICE, with portamento on. A move returns which
//              voice changed, and only that one gets note_pitch: the voice-leading is what you hear.
//   the bass   the chord's root, two octaves down, struck on every change
//   the arp    optional: patgen.h plays the chord tones UP on 1/8s

#define SL_PAD   6
#define SL_BASS  7
#define SL_ARP   8
#define BPM      84

// ── layout (400×240, mouse-first) ──
#define MAP_X 4
#define MAP_Y 14
#define MAP_W 248
#define MAP_H 154
#define TS    30.0f                 // triangle side, px
#define TH    (TS * 0.8660254f)     // triangle height
#define GRID_X 262
#define GRID_Y 24
#define CELL   24
#define LANE_X 4
#define LANE_Y 196
#define LANE_W 248
#define LANE_H 40

static TzChord cur;
static TzTri   here;                   // where the chord sits on the map
static TzGrid  grid;
static PatGen  arp;
static int     pad_h[3] = { -1, -1, -1 };
static float   view_i, view_j;         // lattice coords at the map's centre (eased toward the cur)
static int     running = 1, rate_i = 1, arp_on = 0;
static double  next_tick = -1.0, next_arp = -1.0;
static const float RATE_BEATS[4] = { 0.5f, 1.0f, 2.0f, 4.0f };
static const char *const RATE_NAME[4] = { "1/8", "1/4", "1/2", "BAR" };

#define NTRAIL 10
static TzTri trail[NTRAIL]; static int trail_n;
#define NHIST 32
static struct { int note[3]; int mask; } hist[NHIST]; static int hist_n;
static int last_move = TZ_NONE; static float flash;   // the move that just happened, for the label

// ── sound ──
static void voice_chord(int mask) {
    for (int v = 0; v < 3; v++) {
        if (pad_h[v] < 0) { pad_h[v] = note_on(cur.note[v], SL_PAD, 4); continue; }
        if (mask & (1 << v)) note_pitch(pad_h[v], (float)cur.note[v]);
    }
    hit(36 + cur.root, SL_BASS, 6, (int)(60000.0f / BPM * 1.5f));
    pg_clear(&arp);
    int s[3] = { cur.note[0], cur.note[1], cur.note[2] };
    for (int a = 0; a < 3; a++) for (int b = a + 1; b < 3; b++) if (s[b] < s[a]) { int t = s[a]; s[a] = s[b]; s[b] = t; }
    for (int k = 0; k < 3; k++) pg_add(&arp, s[k] + 12);
    pg_add(&arp, s[0] + 24);
}
static void remember(int mask) {
    if (hist_n == NHIST) { for (int k = 1; k < NHIST; k++) hist[k - 1] = hist[k]; hist_n--; }
    for (int v = 0; v < 3; v++) hist[hist_n].note[v] = cur.note[v];
    hist[hist_n].mask = mask; hist_n++;
    if (trail_n == NTRAIL) { for (int k = 1; k < NTRAIL; k++) trail[k - 1] = trail[k]; trail_n--; }
    trail[trail_n++] = here;
}

static void apply_move(int m) {
    if (m == TZ_RESET) {                               // '@' / 0: home to C, with the least motion
        int mask = tz_jump(&cur, 0, 0);
        here = tz_tri(0, 0, 0);
        voice_chord(mask); remember(mask);
    } else {
        int mask = tz_move(&cur, m);
        if (!mask) return;
        here = tz_tri_move(here, m);
        voice_chord(mask); remember(mask);
    }
    last_move = m; flash = 1.0f;
}
static void go_to(TzTri t) {                           // a click on the map
    int link = tz_tri_link(here, t);
    if (link != TZ_NONE) { apply_move(link); return; }
    int mask = tz_jump(&cur, tz_tri_root(t), t.down);
    here = t;
    voice_chord(mask); remember(mask);
    last_move = TZ_NONE; flash = 1.0f;
}

// a chord's short name, in the neo-Riemannian convention: UPPERCASE = major, lowercase = minor ("Ab", "c").
// (The small fonts draw a trailing "m" like an H, and this is what the theory literature writes anyway.)
static const char *cname(int root, int minor) {
    const char *r = TZ_PC[tz_mod12(root)];
    if (!minor) return r;
    return str("%c%s", r[0] - 'A' + 'a', r + 1);
}

// ── map geometry: lattice node (i,j) → screen ──
static float node_x(float i, float j) { return MAP_X + MAP_W * 0.5f + ((i - view_i) + (j - view_j) * 0.5f) * TS; }
static float node_y(float i, float j) { return MAP_Y + MAP_H * 0.5f - (j - view_j) * TH; }
static void tri_pts(TzTri t, int *xy) {
    int ni[3], nj[3];
    if (!t.down) { ni[0] = t.i; nj[0] = t.j; ni[1] = t.i + 1; nj[1] = t.j; ni[2] = t.i; nj[2] = t.j + 1; }
    else         { ni[0] = t.i + 1; nj[0] = t.j; ni[1] = t.i; nj[1] = t.j + 1; ni[2] = t.i + 1; nj[2] = t.j + 1; }
    for (int k = 0; k < 3; k++) { xy[2 * k] = (int)node_x((float)ni[k], (float)nj[k]); xy[2 * k + 1] = (int)node_y((float)ni[k], (float)nj[k]); }
}
static int in_map(int x, int y) { return x >= MAP_X && x < MAP_X + MAP_W && y >= MAP_Y && y < MAP_Y + MAP_H; }
static TzTri tri_at(int x, int y) {                    // screen → the triangle under it
    float j = (MAP_Y + MAP_H * 0.5f - (float)y) / TH + view_j;
    float i = ((float)x - MAP_X - MAP_W * 0.5f) / TS - (j - view_j) * 0.5f + view_i;
    float fi = floorf(i), fj = floorf(j);
    return tz_tri((int)fi, (int)fj, (i - fi) + (j - fj) >= 1.0f);
}
static void tri_centre(TzTri t, float *ci, float *cj) {
    *ci = t.i + (t.down ? 0.6667f : 0.3333f); *cj = t.j + (t.down ? 0.6667f : 0.3333f);
}

// ── input ──
static TzTri hov; static int hov_ok, dragging, hov_cell = -1;
static void grid_cycle(int k, int dir) {
    int m = grid.move[k] + dir;
    if (m < 0) m = TZ_NMOVE - 1;
    if (m >= TZ_NMOVE) m = 0;
    grid.move[k] = (uint8_t)m;
}
static void random_grid(void) {
    static const uint8_t BAG[] = { TZ_P, TZ_L, TZ_L, TZ_R, TZ_R, TZ_NONE, TZ_NONE, TZ_N, TZ_S, TZ_H };
    for (int k = 0; k < TZ_GRID * TZ_GRID; k++) {
        grid.move[k] = BAG[tz__rand(&grid) % sizeof BAG];
        grid.dice[k] = (tz__rand(&grid) % 7) == 0;
    }
}

void init(void) {
    bpm(BPM);
    instrument(SL_PAD, INSTR_SAW, 450, 0, 7, 1400);
    instrument_filter(SL_PAD, FILTER_LOW, 1500, 1);
    instrument_unison(SL_PAD, 2, 0.12f);
    instrument_glide(SL_PAD, 260);                     // the moving voice SLIDES; the others hold
    instrument_reverb(SL_PAD, 0.45f);
    instrument(SL_BASS, INSTR_TRI, 6, 300, 4, 400);
    instrument(SL_ARP, INSTR_PLUCK, 2, 220, 0, 160);
    instrument_reverb(SL_ARP, 0.35f);
    instrument_echo(SL_ARP, 0.25f);
    echo((int)(60000.0f / BPM * 0.75f), 0.35f, 0.5f);
    reverb(0.75f, 0.55f);

    cur = tz_chord(0, 0, 60);
    here = tz_tri(0, 0, 0);
    float ci, cj; tri_centre(here, &ci, &cj); view_i = ci; view_j = cj;
    pg_init(&arp, 84); arp.order = PG_UP;

    // a default walk worth hearing: mostly L and R (smooth travel through keys), a P for colour,
    // two compound moves, an empty cell to breathe, one DICE cell
    static const uint8_t START[TZ_GRID * TZ_GRID] = {
        TZ_L,    TZ_R,    TZ_NONE, TZ_L,    TZ_P,
        TZ_R,    TZ_P,    TZ_L,    TZ_R,    TZ_NONE,
        TZ_NONE, TZ_L,    TZ_N,    TZ_R,    TZ_L,
        TZ_P,    TZ_R,    TZ_L,    TZ_NONE, TZ_S,
        TZ_L,    TZ_NONE, TZ_R,    TZ_P,    TZ_RESET,
    };
    tz_grid_init(&grid, 2026);
    for (int k = 0; k < TZ_GRID * TZ_GRID; k++) grid.move[k] = START[k];
    grid.dice[12] = 1;                                 // the centre cell re-rolls
    voice_chord(7); remember(7);
}

void update(void) {
    int mx = mouse_x(), my = mouse_y();
    // map hover / click / drag
    hov_ok = in_map(mx, my);
    if (hov_ok) hov = tri_at(mx, my);
    if (hov_ok && mouse_pressed(MOUSE_LEFT)) { if (!tz_tri_eq(hov, here)) go_to(hov); dragging = 1; }
    if (!mouse_down(MOUSE_LEFT)) dragging = 0;
    if (dragging && hov_ok && !tz_tri_eq(hov, here) && tz_tri_link(here, hov) != TZ_NONE) go_to(hov);   // drag = walk edges

    // grid cells
    hov_cell = -1;
    if (mx >= GRID_X && mx < GRID_X + CELL * TZ_GRID && my >= GRID_Y && my < GRID_Y + CELL * TZ_GRID)
        hov_cell = ((my - GRID_Y) / CELL) * TZ_GRID + (mx - GRID_X) / CELL;
    if (hov_cell >= 0) {
        if (mouse_pressed(MOUSE_LEFT))   grid_cycle(hov_cell, +1);
        if (mouse_pressed(MOUSE_RIGHT))  grid_cycle(hov_cell, -1);
        if (mouse_pressed(MOUSE_MIDDLE) || keyp('D')) grid.dice[hov_cell] = !grid.dice[hov_cell];
        float w = mouse_wheel(); if (w > 0) grid_cycle(hov_cell, +1); if (w < 0) grid_cycle(hov_cell, -1);
    }
    // the vector: wheel over its readout
    if (my >= GRID_Y + CELL * TZ_GRID + 4 && my < GRID_Y + CELL * TZ_GRID + 30 && mx >= GRID_X) {
        float w = mouse_wheel(); int *v = my < GRID_Y + CELL * TZ_GRID + 17 ? &grid.dx : &grid.dy;
        if (w > 0 && *v < TZ_VEC_MAX) (*v)++;
        if (w < 0 && *v > 0) (*v)--;
    }

    // keys
    static const char KEYS[6] = { 'P', 'L', 'R', 'N', 'S', 'H' };
    for (int k = 0; k < 6; k++) if (keyp(KEYS[k])) apply_move(TZ_P + k);
    if (keyp('0')) apply_move(TZ_RESET);
    if (keyp(' ')) running = !running;
    if (keyp('X')) random_grid();
    if (keyp('C')) for (int k = 0; k < TZ_GRID * TZ_GRID; k++) { grid.move[k] = TZ_NONE; grid.dice[k] = 0; }

    // the automaton's clock (frame-granular is fine for chord changes) + the arp (booked on the sound clock)
    double t = audio_time();
    double step = 60.0 / BPM * RATE_BEATS[rate_i];
    if (!running) next_tick = -1.0;
    else {
        if (next_tick < 0.0) next_tick = t + step;
        if (t >= next_tick) { int m = tz_grid_tick(&grid); if (m != TZ_NONE) apply_move(m); next_tick += step; }
    }
    if (arp_on) {
        double s8 = 60.0 / BPM * 0.5;
        if (next_arp < 0.0) next_arp = t + 0.05;
        while (next_arp < t + 0.10) { int n = pg_step(&arp); if (n != PG_REST) schedule_at(next_arp, n, SL_ARP, 5, 180); next_arp += s8; }
    } else next_arp = -1.0;

    // follow the chord across the map
    float ci, cj; tri_centre(here, &ci, &cj);
    view_i += (ci - view_i) * 0.06f; view_j += (cj - view_j) * 0.06f;
    if (flash > 0.0f) flash -= 1.0f / 40.0f;
#ifdef DE_TRACE
    watch("root", "%d", cur.root);
    watch("minor", "%d", cur.minor);
    watch("notes", "%d %d %d", cur.note[0], cur.note[1], cur.note[2]);
    watch("cell", "%d", grid.last);
#endif
}

// ── drawing ──
static void draw_map(void) {
    rectfill(MAP_X, MAP_Y, MAP_W, MAP_H, CLR_BLACK);
    clip(MAP_X, MAP_Y, MAP_W, MAP_H);
    int i0 = (int)floorf(view_i) - 9, j0 = (int)floorf(view_j) - 4;
    for (int j = j0; j <= j0 + 9; j++) for (int i = i0 - 4; i <= i0 + 20; i++) for (int dn = 0; dn < 2; dn++) {
        TzTri t = tz_tri(i, j, dn); int xy[6]; tri_pts(t, xy);
        int cx = (xy[0] + xy[2] + xy[4]) / 3, cy = (xy[1] + xy[3] + xy[5]) / 3;
        if (cx < MAP_X - TS || cx > MAP_X + MAP_W + TS || cy < MAP_Y - TS || cy > MAP_Y + MAP_H + TS) continue;
        int col = dn ? CLR_DARKER_PURPLE : CLR_DARKER_BLUE;
        for (int k = 0; k < trail_n; k++) if (tz_tri_eq(trail[k], t)) col = dn ? CLR_DARK_PURPLE : CLR_DARK_BLUE;
        int link = tz_tri_link(here, t);
        if (tz_tri_eq(t, here)) col = dn ? CLR_ORANGE : CLR_YELLOW;
        trifill(xy[0], xy[1], xy[2], xy[3], xy[4], xy[5], col);
        tri(xy[0], xy[1], xy[2], xy[3], xy[4], xy[5], CLR_DARKER_GREY);
        if (link != TZ_NONE) {                                         // the three ways out: the move + where it goes
            const char *nm = cname(tz_tri_root(t), dn);
            font(FONT_SMALL); print(TZ_NAME[link], cx - 2, cy - 7, CLR_WHITE);
            print(nm, cx - text_width(nm) / 2, cy + 1, CLR_LIGHT_GREY);
        }
    }
    if (hov_ok) { int xy[6]; tri_pts(hov, xy); tri(xy[0], xy[1], xy[2], xy[3], xy[4], xy[5], CLR_WHITE); }
    // the notes at the corners; the current chord's three notes ringed
    font(FONT_TINY);
    for (int j = j0; j <= j0 + 10; j++) for (int i = i0 - 4; i <= i0 + 21; i++) {
        int x = (int)node_x((float)i, (float)j), y = (int)node_y((float)i, (float)j);
        if (x < MAP_X - 4 || x > MAP_X + MAP_W + 4 || y < MAP_Y - 4 || y > MAP_Y + MAP_H + 4) continue;
        int pc = tz_node_pc(i, j), in = 0;
        int cxy[6]; tri_pts(here, cxy);                                // ring only THIS triangle's corners
        for (int k = 0; k < 3; k++) if (abs(cxy[2 * k] - x) <= 1 && abs(cxy[2 * k + 1] - y) <= 1) in = 1;
        circfill(x, y, in ? 6 : 4, in ? CLR_WHITE : CLR_DARKER_GREY);
        print(TZ_PC[pc], x - text_width(TZ_PC[pc]) / 2 + (in ? 0 : 0), y - 2, in ? CLR_BLACK : CLR_MEDIUM_GREY);
    }
    clip(0, 0, 0, 0);
    rect(MAP_X, MAP_Y, MAP_W, MAP_H, CLR_INDIGO);
}

static void draw_grid(void) {
    font(FONT_SMALL);
    print("AUTOMATON", GRID_X, GRID_Y - 9, running ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    for (int k = 0; k < TZ_GRID * TZ_GRID; k++) {
        int x = GRID_X + (k % TZ_GRID) * CELL, y = GRID_Y + (k / TZ_GRID) * CELL, m = grid.move[k];
        int bg = m == TZ_NONE ? CLR_BLACK : m >= TZ_N && m <= TZ_H ? CLR_DARK_PURPLE : m == TZ_RESET ? CLR_DARK_GREEN : CLR_DARK_BLUE;
        rectfill(x + 1, y + 1, CELL - 2, CELL - 2, bg);
        if (k == grid.last) rect(x, y, CELL, CELL, CLR_YELLOW);
        else if (k == hov_cell) rect(x, y, CELL, CELL, CLR_WHITE);
        print(TZ_NAME[m], x + CELL / 2 - 2, y + CELL / 2 - 3, m == TZ_NONE ? CLR_DARK_GREY : CLR_WHITE);
        if (grid.dice[k]) { rectfill(x + CELL - 7, y + 3, 4, 4, CLR_PINK); pset(x + CELL - 6, y + 4, CLR_BLACK); pset(x + CELL - 5, y + 5, CLR_BLACK); }
    }
    // the cursor at its fractional position, and the vector it will jump by
    float cx = GRID_X + (float)grid.x / TZ_ONE * CELL, cy = GRID_Y + (float)grid.y / TZ_ONE * CELL;
    float vx = (float)tz_vec_fp(grid.dx) / TZ_ONE * CELL, vy = (float)tz_vec_fp(grid.dy) / TZ_ONE * CELL;
    clip(GRID_X, GRID_Y, CELL * TZ_GRID, CELL * TZ_GRID);
    line((int)cx, (int)cy, (int)(cx + vx), (int)(cy + vy), CLR_PEACH);
    circfill((int)cx, (int)cy, 2, CLR_YELLOW);
    clip(0, 0, 0, 0);
    rect(GRID_X - 1, GRID_Y - 1, CELL * TZ_GRID + 2, CELL * TZ_GRID + 2, CLR_INDIGO);
}

static const char *vec_name(int v) { return str("%d %s", v / 8, TZ_FRAC_NAME[v % 8]); }

static void draw_lanes(void) {
    rectfill(LANE_X, LANE_Y, LANE_W, LANE_H, CLR_BLACK);
    rect(LANE_X, LANE_Y, LANE_W, LANE_H, CLR_INDIGO);
    static const int VC[3] = { CLR_PEACH, CLR_GREEN, CLR_BLUE };
    float sw = (float)(LANE_W - 4) / NHIST;
    for (int k = 0; k < hist_n; k++) for (int v = 0; v < 3; v++) {
        int n = hist[k].note[v];
        int y = LANE_Y + LANE_H - 3 - (int)((n - 46) * (LANE_H - 6) / 32.0f);
        int x0 = LANE_X + 2 + (int)(k * sw), x1 = LANE_X + 2 + (int)((k + 1) * sw) - 1;
        int moved = hist[k].mask & (1 << v);
        if (k > 0) {
            int py = LANE_Y + LANE_H - 3 - (int)((hist[k - 1].note[v] - 46) * (LANE_H - 6) / 32.0f);
            if (py != y) line(x0 - 1, py, x0, y, moved ? CLR_WHITE : VC[v]);
        }
        line(x0, y, x1, y, moved && k == hist_n - 1 ? CLR_WHITE : VC[v]);
        if (moved && k > 0) rectfill(x0, y - 1, 2, 3, CLR_WHITE);
    }
    font(FONT_TINY);
    print("VOICES  white = moved   UPPER major  lower minor", LANE_X + 3, LANE_Y + 2, CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    font(FONT_SMALL);
    print("TONNETZ", 4, 3, CLR_LIGHT_YELLOW);
    print(str("%s %s", TZ_PC[cur.root], cur.minor ? "minor" : "major"), 52, 3, CLR_WHITE);
    if (flash > 0.0f && last_move != TZ_NONE) print(str("<- %s", TZ_NAME[last_move]), 150, 3, CLR_YELLOW);
    if (hov_ok && !tz_tri_eq(hov, here)) {
        int link = tz_tri_link(here, hov);
        const char *nm = cname(tz_tri_root(hov), hov.down);
        print(link != TZ_NONE ? str("%s: %s", TZ_NAME[link], nm) : str("jump: %s", nm), 190, 3, CLR_LIGHT_GREY);
    }
    print_right(str("%d BPM", BPM), 396, 3, CLR_DARK_GREY);

    draw_map();
    draw_grid();
    draw_lanes();

    font(FONT_SMALL);
    ui_begin();
    // move buttons under the map
    for (int k = 0; k < 7; k++) {                     // each button says where it would take you
        TzChord to = cur; const char *lab;
        if (k == 6) lab = "@ C";
        else { tz_move(&to, TZ_P + k); lab = str("%s %s", TZ_NAME[TZ_P + k], cname(to.root, to.minor)); }
        if (ui_button(MAP_X + k * 36, 173, 33, 18, lab)) apply_move(k == 6 ? TZ_RESET : TZ_P + k);
    }
    // the vector
    int vy = GRID_Y + CELL * TZ_GRID + 4;
    font(FONT_SMALL);
    print("DX", GRID_X, vy + 3, CLR_LIGHT_GREY); print("DY", GRID_X, vy + 16, CLR_LIGHT_GREY);
    if (ui_button(GRID_X + 14, vy, 12, 11, "<") && grid.dx > 0) grid.dx--;
    if (ui_button(GRID_X + 92, vy, 12, 11, ">") && grid.dx < TZ_VEC_MAX) grid.dx++;
    if (ui_button(GRID_X + 14, vy + 13, 12, 11, "<") && grid.dy > 0) grid.dy--;
    if (ui_button(GRID_X + 92, vy + 13, 12, 11, ">") && grid.dy < TZ_VEC_MAX) grid.dy++;
    print(vec_name(grid.dx), GRID_X + 30, vy + 3, CLR_WHITE);
    print(vec_name(grid.dy), GRID_X + 30, vy + 16, CLR_WHITE);
    // transport
    if (ui_button(GRID_X, 182, 58, 16, running ? "STOP" : "RUN")) running = !running;
    if (ui_button(GRID_X + 62, 182, 58, 16, str("RATE %s", RATE_NAME[rate_i]))) rate_i = (rate_i + 1) % 4;
    if (ui_button(GRID_X, 202, 58, 16, arp_on ? "ARP on" : "ARP")) arp_on = !arp_on;
    if (ui_button(GRID_X + 62, 202, 28, 16, "RND")) random_grid();
    if (ui_button(GRID_X + 92, 202, 28, 16, "CLR")) for (int k = 0; k < TZ_GRID * TZ_GRID; k++) { grid.move[k] = TZ_NONE; grid.dice[k] = 0; }
    if (ui_button(GRID_X, 222, 120, 14, "HOME (cursor + C)")) { tz_grid_home(&grid); apply_move(TZ_RESET); }
    ui_end();
}

#ifdef DE_SPEC
void spec(void) {
    tonnetz_selfcheck();
    patgen_selfcheck();
    // the cart's own half: the map and the chord never disagree, whatever the automaton does
    step(1);
    int agree = 1;
    for (int k = 0; k < 400; k++) {
        int m = tz_grid_tick(&grid);
        if (m != TZ_NONE) apply_move(m);
        if (tz_tri_root(here) != cur.root || here.down != cur.minor) agree = 0;
    }
    expect(agree, "400 automaton ticks: the lit triangle always IS the chord you hear");
}
#endif
