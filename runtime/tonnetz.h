// tonnetz.h — CHORDS AS PLACES: neo-Riemannian moves on the Tonnetz, plus a vector "automaton" that
// walks them by itself. Pure logic: no sound, no UI, no clock.
//
// Ported from Ornament & Crime's Harrington 1200 / Automatonnetz (o_c_REV/tonnetz/*.h and
// APP_AUTOMATONNETZ.ino, util/util_grid.h). docs/design/open-source-audio-toys.md §2.2.
//   Copyright (c) 2015, 2016 Patrick Dowling, Tim Churches — MIT License. The move table below
//   (TZ_OC_OFFSETS) is theirs; the permission notice is reproduced at the end of this file.
//
// THE HONEST CORE. Every major and minor triad is a triangle on a grid of pitch classes. Three moves
// flip it across one of its edges, and each moves EXACTLY ONE note, by a semitone or a whole tone:
//   P (parallel)  C ↔ Cm    the third moves a semitone
//   L (leading)   C ↔ Em    major: the root drops a semitone · minor: the fifth rises one
//   R (relative)  C ↔ Am    major: the fifth rises a tone   · minor: the root drops one
// so ANY chain of moves sounds smooth, however far it wanders, and nobody needs to know a key.
// Three compound moves, each three steps: N = R·L·P (C → Fm), S = L·P·R (C → D♭m), H = L·P·L (C → A♭m).
//
// ── HOW A CART USES IT ────────────────────────────────────────────────────────────────────────────────
//   TzChord c = tz_chord(0, 0, 60);             // C major, voiced around middle C
//   int moved = tz_move(&c, TZ_L);              // → E minor; returns the VOICE that moved (0..2)
//   if (moved >= 0) note_pitch(voice[moved], c.note[moved]);   // slide only that voice
//   tz_jump(&c, 9, 1);                          // anywhere else (A minor), with the least motion
//
//   TzTri t = tz_tri_of(...); t = tz_tri_move(t, TZ_R);   // the same moves, as triangles on a map
//
//   TzGrid g; tz_grid_init(&g, 7);              // the automaton: 5×5 cells, one move each
//   on every clock tick:  int m = tz_grid_tick(&g); if (m != TZ_NONE) tz_move(&c, m);
//
// ── CHANGED ON THE WAY IN ─────────────────────────────────────────────────────────────────────────────
// • Voices are REAL notes with a stable order (voice 0/1/2 keep their identity across moves), so a cart
//   can glide exactly the one voice that moved. O&C kept intervals plus a root index.
// • O&C's chord never comes back into range, so a long walk slowly climbs or sinks by octaves. Here a
//   voice that leaves the register window [lo, hi] is folded back by an octave (tz_keep).
// • The grid counts a cell as 840 units, which 1/2 … 1/8 all divide EXACTLY, so no fraction ever
//   creeps. O&C used 24-bit fixed point with +1 nudges; its 1/6 has no nudge (R/6 rounds down, six steps
//   land 4 units short), so its first 1/6 crossing comes a tick late.
// • tz_jump (voice-lead to an arbitrary chord) and the triangle geometry are ours.
#ifndef TONNETZ_H
#define TONNETZ_H

#include <stdint.h>

typedef enum { TZ_NONE = 0, TZ_P, TZ_L, TZ_R, TZ_N, TZ_S, TZ_H, TZ_RESET, TZ_NMOVE } TzMove;
static const char *const TZ_NAME[TZ_NMOVE] = { "*", "P", "L", "R", "N", "S", "H", "@" };
static const char *const TZ_PC[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };   // flats: the small fonts draw # as H

static inline int tz_mod12(int x) { x %= 12; return x < 0 ? x + 12 : x; }

typedef struct {
    int note[3];       // voiced MIDI notes, one per VOICE (order is identity, not pitch)
    int root;          // pitch class 0..11
    int minor;         // 0 = major, 1 = minor
    int lo, hi;        // register window the voices are kept inside
} TzChord;

// which voice holds this chord's root (0), third (1) or fifth (2)
static inline int tz_voice_of(const TzChord *c, int role) {
    int pc = tz_mod12(c->root + (role == 0 ? 0 : role == 1 ? (c->minor ? 3 : 4) : 7));
    for (int v = 0; v < 3; v++) if (tz_mod12(c->note[v]) == pc) return v;
    return -1;
}
static inline void tz_keep(TzChord *c) {
    for (int v = 0; v < 3; v++) {
        while (c->note[v] > c->hi) c->note[v] -= 12;
        while (c->note[v] < c->lo) c->note[v] += 12;
    }
}

// a triad near `center` (root position, root at or just below center), window = center ± 12
static inline TzChord tz_chord(int root, int minor, int center) {
    TzChord c; c.root = tz_mod12(root); c.minor = minor ? 1 : 0;
    c.lo = center - 12; c.hi = center + 12;
    int r = center - tz_mod12(center - c.root);
    c.note[0] = r; c.note[1] = r + (c.minor ? 3 : 4); c.note[2] = r + 7;
    tz_keep(&c);
    return c;
}

// one PRIMITIVE move: returns the voice that moved, or -1. (O&C's table, by role.)
static inline int tz__prim(TzChord *c, int m) {
    int role, d, nroot;
    if (!c->minor) {
        if (m == TZ_P) { role = 1; d = -1; nroot = c->root; }
        else if (m == TZ_L) { role = 0; d = -1; nroot = c->root + 4; }
        else /* R */      { role = 2; d = +2; nroot = c->root + 9; }
    } else {
        if (m == TZ_P) { role = 1; d = +1; nroot = c->root; }
        else if (m == TZ_L) { role = 2; d = +1; nroot = c->root + 8; }
        else /* R */      { role = 0; d = -2; nroot = c->root + 3; }
    }
    int v = tz_voice_of(c, role);
    if (v < 0) return -1;              // not a triad any more (a cart wrote note[] by hand)
    c->note[v] += d;
    c->root = tz_mod12(nroot);
    c->minor = !c->minor;
    tz_keep(c);
    return v;
}
static const int TZ_SEQ[TZ_NMOVE][3] = {
    { 0, 0, 0 }, { TZ_P, 0, 0 }, { TZ_L, 0, 0 }, { TZ_R, 0, 0 },
    { TZ_R, TZ_L, TZ_P }, { TZ_L, TZ_P, TZ_R }, { TZ_L, TZ_P, TZ_L }, { 0, 0, 0 },
};
// apply a move. Returns a bitmask of the voices that moved (bit v = voice v). TZ_RESET is the cart's job.
static inline int tz_move(TzChord *c, int m) {
    if (m <= TZ_NONE || m >= TZ_RESET) return 0;
    int mask = 0;
    for (int k = 0; k < 3 && TZ_SEQ[m][k]; k++) { int v = tz__prim(c, TZ_SEQ[m][k]); if (v >= 0) mask |= 1 << v; }
    return mask;
}

// go to ANY triad with the least total voice motion: try all 6 ways of giving the three target notes
// to the three voices, each at its nearest octave. A long jump still moves as little as it can.
static inline int tz_jump(TzChord *c, int root, int minor) {
    int pcs[3] = { tz_mod12(root), tz_mod12(root + (minor ? 3 : 4)), tz_mod12(root + 7) };
    static const int PERM[6][3] = { {0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0} };
    int best = 1 << 30, bn[3] = { 0, 0, 0 };
    for (int p = 0; p < 6; p++) {
        int cost = 0, n[3];
        for (int v = 0; v < 3; v++) {
            int pc = pcs[PERM[p][v]], cur = c->note[v];
            int d = tz_mod12(pc - cur); if (d > 6) d -= 12;     // nearest octave, ties go up
            n[v] = cur + d; cost += d < 0 ? -d : d;
        }
        if (cost < best) { best = cost; bn[0] = n[0]; bn[1] = n[1]; bn[2] = n[2]; }
    }
    int mask = 0;
    for (int v = 0; v < 3; v++) { if (bn[v] != c->note[v]) mask |= 1 << v; c->note[v] = bn[v]; }
    c->root = tz_mod12(root); c->minor = minor ? 1 : 0;
    tz_keep(c);
    return mask;
}

static inline const char *tz_name(const TzChord *c, char *buf) {   // "C", "Am" — buf ≥ 4
    const char *r = TZ_PC[c->root]; int i = 0;
    while (*r) buf[i++] = *r++;
    if (c->minor) buf[i++] = 'm';
    buf[i] = 0;
    return buf;
}

// ── the map: triangles on a lattice. Node (i,j) holds pitch class 7i + 4j (fifths across, major thirds
// up). UP triangle (i,j) = nodes (i,j),(i+1,j),(i,j+1) = the MAJOR triad on pc(i,j). DOWN triangle (i,j) =
// nodes (i+1,j),(i,j+1),(i+1,j+1) = the MINOR triad on pc(i,j)+4. Each P/L/R flips across one edge.
typedef struct { int i, j, down; } TzTri;
static inline int tz_node_pc(int i, int j) { return tz_mod12(7 * i + 4 * j); }
static inline int tz_tri_root(TzTri t)  { return tz_mod12(tz_node_pc(t.i, t.j) + (t.down ? 4 : 0)); }
static inline TzTri tz_tri(int i, int j, int down) { TzTri t = { i, j, down }; return t; }
static inline TzTri tz__flip(TzTri t, int m) {
    if (!t.down) {
        if (m == TZ_P) return tz_tri(t.i, t.j - 1, 1);
        if (m == TZ_L) return tz_tri(t.i, t.j, 1);
        return tz_tri(t.i - 1, t.j, 1);                  // R
    }
    if (m == TZ_P) return tz_tri(t.i, t.j + 1, 0);
    if (m == TZ_L) return tz_tri(t.i, t.j, 0);
    return tz_tri(t.i + 1, t.j, 0);                      // R
}
static inline TzTri tz_tri_move(TzTri t, int m) {
    if (m <= TZ_NONE || m >= TZ_RESET) return t;
    for (int k = 0; k < 3 && TZ_SEQ[m][k]; k++) t = tz__flip(t, TZ_SEQ[m][k]);
    return t;
}
static inline int tz_tri_eq(TzTri a, TzTri b) { return a.i == b.i && a.j == b.j && a.down == b.down; }
// which primitive move goes from a to its edge-neighbour b (TZ_NONE if b isn't one)
static inline int tz_tri_link(TzTri a, TzTri b) {
    for (int m = TZ_P; m <= TZ_R; m++) if (tz_tri_eq(tz__flip(a, m), b)) return m;
    return TZ_NONE;
}

// ── the AUTOMATON (Automatonnetz): a 5×5 grid of moves and a cursor that jumps by a vector (dx, dy) on
// each tick, wrapping. A move fires only when the cursor ENTERS a different cell. Vector steps are
// whole cells plus a fraction (TZ_FRAC), so a slow vector lingers in a cell for several ticks. Whole-
// cell vectors trace a fixed line ((1, 2) walks one 5-cell diagonal for ever); it is the FRACTION that
// sweeps the grid, e.g. O&C's default (1, 1/5) scans it row by row.
#define TZ_GRID       5
#define TZ_ONE        840                         // units per cell: lcm(1..8), so every fraction is exact
#define TZ_GRID_FP    (TZ_GRID * TZ_ONE)
#define TZ_VEC_MAX    (8 * TZ_GRID - 1)           // a vector setting is 0..39 = whole*8 + fraction index
static const char *const TZ_FRAC_NAME[8] = { "", "1/8", "1/7", "1/6", "1/5", "1/4", "1/3", "1/2" };
static const int32_t TZ_FRAC[8] = {               // O&C's clock_fraction steps, exact
    0, TZ_ONE / 8, TZ_ONE / 7, TZ_ONE / 6, TZ_ONE / 5, TZ_ONE / 4, TZ_ONE / 3, TZ_ONE / 2 };

typedef struct {
    uint8_t  move[TZ_GRID * TZ_GRID];   // TzMove per cell
    uint8_t  dice[TZ_GRID * TZ_GRID];   // 1 = re-roll this cell's move after each visit (O&C's RAND event)
    int32_t  x, y;                      // cursor, fixed point
    int      dx, dy;                    // vector settings 0..TZ_VEC_MAX (O&C defaults: 8 = 1 cell, 4 = 1/5)
    uint32_t seed;
    int      last;                      // the cell index the last tick landed in
} TzGrid;

static inline int32_t tz_vec_fp(int v) { if (v < 0) v = 0; if (v > TZ_VEC_MAX) v = TZ_VEC_MAX; return (v / 8) * TZ_ONE + TZ_FRAC[v % 8]; }
static inline int tz_grid_cell(const TzGrid *g) { return (g->y / TZ_ONE) * TZ_GRID + (g->x / TZ_ONE); }
static inline uint32_t tz__rand(TzGrid *g) { g->seed = (1103515245u * g->seed + 12345u) & 0x7FFFFFFFu; return g->seed >> 15; }   // high bits only

static inline void tz_grid_init(TzGrid *g, uint32_t seed) {
    for (int k = 0; k < TZ_GRID * TZ_GRID; k++) { g->move[k] = TZ_NONE; g->dice[k] = 0; }
    g->x = g->y = 0; g->dx = 8; g->dy = 4; g->seed = seed ? seed : 1u; g->last = 0;
}
static inline void tz_grid_home(TzGrid *g) { g->x = g->y = 0; g->last = 0; }

// one clock tick. Returns the move to apply (TZ_NONE if the cursor stayed in its cell or the cell is
// empty; TZ_RESET = the cart goes back to its start chord).
static inline int tz_grid_tick(TzGrid *g) {
    int ox = g->x / TZ_ONE, oy = g->y / TZ_ONE;
    int32_t x = (g->x + tz_vec_fp(g->dx)) % TZ_GRID_FP, y = (g->y + tz_vec_fp(g->dy)) % TZ_GRID_FP;
    g->x = x; g->y = y;
    if (x / TZ_ONE == ox && y / TZ_ONE == oy) return TZ_NONE;
    int k = tz_grid_cell(g);
    g->last = k;
    int m = g->move[k];
    if (g->dice[k]) g->move[k] = (uint8_t)(tz__rand(g) % TZ_NMOVE);   // O&C: the cell re-rolls AFTER it fires
    return m;
}

// ── SELF-CHECK (spec.h "specs on an includeable"; a cart's spec() calls tonnetz_selfcheck()) ──
#ifdef DE_SPEC
#include "spec.h"
static inline int tz__pcset(const TzChord *c) { int s = 0; for (int v = 0; v < 3; v++) s |= 1 << tz_mod12(c->note[v]); return s; }
static inline int tz__triad_set(int root, int minor) { return (1 << tz_mod12(root)) | (1 << tz_mod12(root + (minor ? 3 : 4))) | (1 << tz_mod12(root + 7)); }

// O&C's own table (tonnetz.h, `transformations[TRANSFORM_LAST][2]`): {root_shift, {root, third, fifth}
// offsets}, [major, minor]. Used as the ORACLE: our role-based primitives and three-step compounds
// must land exactly where their direct offsets do.
static const int TZ_OC_OFFSETS[7][2][3] = {
    { {  0,  0,  0 }, {  0,  0,  0 } },   // NONE
    { {  0, -1,  0 }, {  0,  1,  0 } },   // P
    { { -1,  0,  0 }, {  0,  0,  1 } },   // L
    { {  0,  0,  2 }, { -2,  0,  0 } },   // R
    { {  0,  1,  1 }, { -1, -1,  0 } },   // N
    { {  1,  0,  1 }, { -1,  0, -1 } },   // S
    { { -1, -1,  1 }, { -1,  1,  1 } },   // H
};

static inline void tonnetz_selfcheck(void) {
    TzChord c = tz_chord(0, 0, 60);
    expect(c.note[0] == 60 && c.note[1] == 64 && c.note[2] == 67, "tz_chord(C) = C4 E4 G4");
    struct { int m, root, minor; const char *msg; } T[] = {
        { TZ_P, 0, 1, "P: C -> Cm" }, { TZ_L, 4, 1, "L: C -> Em" }, { TZ_R, 9, 1, "R: C -> Am" },
        { TZ_N, 5, 1, "N: C -> Fm" }, { TZ_S, 1, 1, "S: C -> Dbm" }, { TZ_H, 8, 1, "H: C -> Abm" },
    };
    for (int k = 0; k < 6; k++) {
        TzChord d = tz_chord(0, 0, 60); tz_move(&d, T[k].m);
        expect(d.root == T[k].root && d.minor == T[k].minor && tz__pcset(&d) == tz__triad_set(T[k].root, T[k].minor), T[k].msg);
    }
    // every primitive, from every one of the 24 triads: moves exactly ONE voice, by 1 or 2 semitones,
    // lands on a real triad, and undoes itself when applied twice
    int one = 1, step = 1, triad = 1, invol = 1, oracle = 1, tris = 1;
    for (int r = 0; r < 12; r++) for (int q = 0; q < 2; q++) {
        for (int m = TZ_P; m <= TZ_R; m++) {
            TzChord a = tz_chord(r, q, 66), b = a;
            int mask = tz_move(&b, m), moved = 0, dist = 0;
            for (int v = 0; v < 3; v++) if (a.note[v] != b.note[v]) { moved++; dist = b.note[v] - a.note[v]; }
            if (moved != 1 || mask == 0 || (mask & (mask - 1))) one = 0;
            dist = dist < 0 ? -dist : dist; if (dist % 12 != 1 && dist % 12 != 2 && dist % 12 != 10 && dist % 12 != 11) step = 0;
            if (tz__pcset(&b) != tz__triad_set(b.root, b.minor)) triad = 0;
            TzChord z = b; tz_move(&z, m);
            if (z.root != a.root || z.minor != a.minor || tz__pcset(&z) != tz__pcset(&a)) invol = 0;
        }
        for (int m = TZ_P; m <= TZ_H; m++) {   // our moves vs O&C's direct offsets (pitch-class sets)
            TzChord a = tz_chord(r, q, 66), b = a; tz_move(&b, m);
            const int *o = TZ_OC_OFFSETS[m][q];
            int want = (1 << tz_mod12(r + o[0])) | (1 << tz_mod12(r + (q ? 3 : 4) + o[1])) | (1 << tz_mod12(r + 7 + o[2]));
            if (tz__pcset(&b) != want) oracle = 0;
            // and the map agrees with the chord: the moved triangle holds the moved chord's root+quality
            for (int i = -2; i <= 2; i++) for (int j = -2; j <= 2; j++) for (int dn = 0; dn < 2; dn++) {
                TzTri t = tz_tri(i, j, dn);
                if (tz_tri_root(t) != r || dn != q) continue;
                TzTri u = tz_tri_move(t, m);
                if (tz_tri_root(u) != b.root || u.down != b.minor) tris = 0;
            }
        }
    }
    expect(one, "every P/L/R moves exactly one voice (from all 24 triads)");
    expect(step, "...by a semitone or a whole tone");
    expect(triad, "...and lands on a real major/minor triad");
    expect(invol, "P, L and R each undo themselves (PP = LL = RR = nothing)");
    expect(oracle, "all six moves match O&C's own offset table, all 24 triads");
    expect(tris, "the triangle map agrees with the chord moves (all 6 moves, all 24 triads)");
    { TzTri t = tz_tri(0, 0, 0), u = tz__flip(t, TZ_L);
      expect_eq(tz_tri_link(t, u), TZ_L, "tz_tri_link names the edge between two neighbours");
      expect_eq(tz_tri_link(t, tz_tri(3, 3, 0)), TZ_NONE, "...and NONE for a triangle that isn't one"); }

    // register: a long one-way walk stays inside the window (O&C's drifted)
    TzChord w = tz_chord(0, 0, 60); int inside = 1;
    for (int k = 0; k < 400; k++) { tz_move(&w, (k & 1) ? TZ_R : TZ_L); for (int v = 0; v < 3; v++) if (w.note[v] < w.lo || w.note[v] > w.hi) inside = 0; }
    expect(inside, "400 L/R moves never leave the register window");

    // jump: C -> Am is one voice by a tone (G->A), the least-motion voicing
    TzChord j = tz_chord(0, 0, 60); int jm = tz_jump(&j, 9, 1);
    expect(j.note[0] == 60 && j.note[1] == 64 && j.note[2] == 69 && jm == 4, "tz_jump C->Am moves only G up to A");

    // the automaton: dx = 1 cell, dy = 0 walks the top row and wraps; a move fires on ENTERING a cell
    TzGrid g; tz_grid_init(&g, 3); g.dx = 8; g.dy = 0;
    for (int k = 0; k < 5; k++) g.move[k] = (uint8_t)(TZ_P + k % 3);
    int seq[6]; for (int k = 0; k < 6; k++) seq[k] = tz_grid_tick(&g);
    expect(seq[0] == TZ_L && seq[1] == TZ_R && seq[2] == TZ_P && seq[3] == TZ_L && seq[4] == TZ_P && seq[5] == TZ_L,
           "the cursor walks the row (cells 1,2,3,4,0,1) and fires each cell's move");
    tz_grid_init(&g, 3); g.dx = 1; g.dy = 0; g.move[1] = TZ_R;   // dx = 1/8 of a cell: 7 quiet ticks, then fire
    int fired = -1; for (int k = 0; k < 8; k++) if (tz_grid_tick(&g) == TZ_R && fired < 0) fired = k;
    expect_eq(fired, 7, "a 1/8 vector lingers 7 ticks and enters the next cell on the 8th");
    tz_grid_init(&g, 3); g.dx = 2; g.dy = 0; g.move[1] = TZ_L;   // 1/7: the +1 rounding must still cross on the 7th
    fired = -1; for (int k = 0; k < 7; k++) if (tz_grid_tick(&g) == TZ_L && fired < 0) fired = k;
    expect_eq(fired, 6, "a 1/7 vector crosses on exactly the 7th tick");
    tz_grid_init(&g, 3); g.dx = 3; g.dy = 0; g.move[1] = TZ_L;   // 1/6: O&C's own table lands a tick late here
    fired = -1; for (int k = 0; k < 7; k++) if (tz_grid_tick(&g) == TZ_L && fired < 0) fired = k;
    expect_eq(fired, 5, "a 1/6 vector crosses on exactly the 6th tick");
    tz_grid_init(&g, 3); g.dx = 2; g.dy = 0;
    for (int k = 0; k < 35 * 20; k++) tz_grid_tick(&g);          // 20 full wraps of 1/7 steps
    expect(g.x == 0, "...and 20 full wraps of 1/7 steps land back exactly on 0 (no creeping error)");
    tz_grid_init(&g, 3); g.dx = 3; g.dy = 0;
    for (int k = 0; k < 30 * 20; k++) tz_grid_tick(&g);
    expect(g.x == 0, "...same for 1/6");
    // COVERAGE is the vector's job, and whole-cell vectors don't give it: (1, 2) walks one 5-cell diagonal
    // forever. A FRACTION is what sweeps the grid: O&C's default (1, 1/5) scans it row by row.
    int seen[TZ_GRID * TZ_GRID], n;
    tz_grid_init(&g, 5);                                         // defaults: dx = 1, dy = 1/5
    for (int k = 0; k < 25; k++) seen[k] = 0;
    n = 0; for (int k = 0; k < 25; k++) { tz_grid_tick(&g); if (!seen[g.last]) { seen[g.last] = 1; n++; } }
    expect_eq(n, 25, "the default vector (1, 1/5) visits all 25 cells in 25 ticks");
    tz_grid_init(&g, 5); g.dx = 8; g.dy = 16;                    // (1, 2): whole cells only
    for (int k = 0; k < 25; k++) seen[k] = 0;
    n = 0; for (int k = 0; k < 500; k++) { tz_grid_tick(&g); if (!seen[g.last]) { seen[g.last] = 1; n++; } }
    expect_eq(n, 5, "...while (1, 2) only ever walks one 5-cell diagonal");
    tz_grid_init(&g, 11); g.dx = 8; g.dy = 0; g.move[1] = TZ_P; g.dice[1] = 1;
    int rolled = 0; for (int k = 0; k < 50; k++) { tz_grid_tick(&g); if (g.move[1] != TZ_P) rolled = 1; }
    expect(rolled, "a DICE cell re-rolls its move after it fires");
}
#endif  // DE_SPEC

/* Ornament & Crime — MIT License, as it appears in the ported files:
 *   Copyright (c) 2015, 2016 Patrick Dowling, Tim Churches
 *   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 *   associated documentation files (the "Software"), to deal in the Software without restriction,
 *   including without limitation the rights to use, copy, modify, merge, publish, distribute,
 *   sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions: The above copyright notice and this
 *   permission notice shall be included in all copies or substantial portions of the Software.
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 *   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 *   OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#endif  // TONNETZ_H
