/* de:meta
{
  "slug": "interiors",
  "title": "Interiors — a fill language for floor-plans",
  "status": "active",
  "created": "2026-06-17",
  "kind": [
    "tech-demo",
    "generative"
  ],
  "teaches": [
    "dungeon-generation"
  ],
  "lineage": "The indoor sibling of lotfill: fills a building footprint with a recursive BSP room tree, emergent room labeling (label falls from area/frontage/adjacency rules, no type list), door circulation ensuring every room is reachable, and deterministic hash-seeded generation — extends lotfill's open-world scatter into bounded recursive subdivision.",
  "description": "The indoor sibling of Lotfill (docs/design/interiors-brief.md): where lotfill fills the land between buildings, this fills the space INSIDE one building's footprint - believable house / shop / warehouse layouts. A floor-plan is a BOUNDED region generated wholesale from one hash-seed, so unlike lotfill's open-world scatter we MAY GROW: recursive BSP splits the shell into a room tree (bounded => recursion is legal). The thesis: rooms EMERGE, they are not a type list - partition() splits the shell into anonymous rooms, then a SELECTOR reads each room's AREA / FRONTAGE / ADJACENCY and lets the label fall out (largest-fronting-entry = living/shop/bay, smallest = bath/office, medium-on-an-exterior-wall = kitchen, the rest = bedrooms/storage); believability comes from the RULES, not hand-placed set-pieces. Building type (residential / commercial / industrial) is the top-level selector input, picked per cell from a zone field. The tabs (TAB to switch): PROGRAM = the DRIVER (default) composing shell->partition->circulation->fixtures->furnish over a grid of footprints; SHELL = the exterior envelope + the entry gap on the frontage wall; PARTITION = the recursive BSP room-split (the heart); CIRCULATION = doors through the shared walls - every split's wall gets a door, so the room graph is a TREE => every room reachable from the entry by construction (no orphans); FIXTURES = windows on exterior walls + doors on interior; FURNISH = props per room keyed by the emergent label. Honors lotfill's seams: pure fn of the hash-seed (deterministic, byte-reproducible), draw == query (room_at/solid_at IS the future collision seam for sloop), LOD gates drawing not generating, atoms read no globals. Inspection: 1-4 peel layers with live counts, G shows the BSP walls + entry markers, O tints the building-type field. WASD/ZX move, R new seed."
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <math.h>

// ============================================================================
// INTERIORS — a fill language for building FLOOR-PLANS + its WORKBENCH.
//
//   ◄ ► ▲ ▼ / WASD   pan        Z / X (or wheel)  zoom        R  new seed
//   TAB  switch atom  1-4  peel layers   G  debug grid   O  type overlay
//
// ── what this is ─────────────────────────────────────────────────────────────
// The indoor sibling of lotfill. lotfill fills the land *between* buildings; this
// fills the space INSIDE one building's footprint — believable house / shop /
// warehouse layouts. Design: docs/design/interiors-brief.md +
// docs/design/streetlevel-content.md (the parent thesis).
//
// The KEY LIBERATION over lotfill's open-world scatter: a floor-plan is a BOUNDED
// region, generated wholesale from one hash-seed and never spanning a chunk seam.
// So — unlike the "lattice instances only, no grown sets" rule that binds lotfill —
// we MAY GROW: recursive BSP splits the shell into a room tree. Bounded ⇒ recursion
// is legal.
//
// ── the thesis: rooms EMERGE, they are not a type list ─────────────────────────
// We never write a "kitchen" generator. partition() splits the shell into anonymous
// rooms; a SELECTOR (assign_labels) reads each room's AREA / FRONTAGE / ADJACENCY
// and lets the label fall out — largest-fronting-entry = living/shop/bay, smallest =
// bath/office, medium-on-an-exterior-wall = kitchen, the rest = bedrooms/storage.
// Believability comes from the RULES, not hand-placed set-pieces.
//
// ── the tabs (TAB to switch) ───────────────────────────────────────────────────
// • PROGRAM   — the DRIVER (default): composes shell→partition→circulation→fixtures
//               →furnish over a grid of footprints, building type per cell from a
//               zone field. lotfill's WORLD-tab analogue (same fns, the driver
//               changes). Layers peel the phases.
// • SHELL     — the building envelope: exterior walls + the entry gap on the
//               frontage (outward) wall. The bounded region every other atom fills.
// • PARTITION — recursive BSP split of the shell into a room tree. The heart.
// • CIRCULATION — doors through the shared walls. Every split's wall gets a door, so
//               the room adjacency graph is a TREE ⇒ every room reachable from the
//               entry by construction (no orphan rooms — a theorem, like footprint's
//               perimeter-fronting).
// • FIXTURES  — stroke openings on walls: windows on exterior walls, doors on interior.
// • FURNISH   — scatter props per room, keyed by the room's emergent label.
//
// ── seams honored (same as lotfill) ───────────────────────────────────────────
// 1. Pure fn of the region's hash-seed: one building = one seed → the whole plan,
//    deterministic and byte-reproducible.
// 2. Draw == query: room_at(x,y)/solid_at(x,y) is the renderer's code run as a query
//    — the eventual collision/interaction seam for sloop (building_at/interior_at).
// 3. LOD gates DRAWING, never generating (zoom>= gates, same as lotfill).
// 4. Atoms read no cam/seed globals — they take the rect + show-flags, so the SAME
//    fn runs on one building on stage AND composed over every building in the world.
// ============================================================================

static int lf_seed = 0;

static unsigned hash2(int a, int b) {                 // per-cell pseudo-random
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static unsigned hmix(unsigned h, int k) { return hash2((int)h, k * 2654435761u + 0x9e3779b9u); }
static int ifloordiv(int a, int b) { int q = a / b; if ((a % b) != 0 && ((a < 0) != (b < 0))) q--; return q; }
static float frac01(unsigned h) { return (h & 4095u) / 4095.0f; }     // 12 bits → [0,1]

// ── the model ──────────────────────────────────────────────────────────────────
typedef struct { int x, y, w, h; } Rect;              // a bounded region (world px)

enum { BT_RES, BT_COM, BT_IND, BT_N };                // residential / commercial / industrial
static const char *BT_NAME[BT_N] = { "residential", "commercial", "industrial" };

// emergent room labels — produced by the selector, NOT enumerated up front
enum { RM_HALL, RM_LIVING, RM_BED, RM_KITCHEN, RM_BATH,
       RM_SHOP, RM_BACK, RM_OFFICE, RM_BAY, RM_STORE, RM_N };
static const char *RM_NAME[RM_N] = { "hall", "living", "bedroom", "kitchen", "bath",
                                     "shop floor", "back-of-house", "office", "bay", "storage" };
static const int RM_FLOOR[RM_N] = {
    [RM_HALL]    = CLR_DARK_BROWN,  [RM_LIVING] = CLR_BROWN,     [RM_BED]   = CLR_DARK_PURPLE,
    [RM_KITCHEN] = CLR_DARK_GREY,   [RM_BATH]   = CLR_BLUE_GREEN,[RM_SHOP]  = CLR_LIGHT_GREY,
    [RM_BACK]    = CLR_DARKER_GREY, [RM_OFFICE] = CLR_MEDIUM_GREY,[RM_BAY]  = CLR_DARKER_GREY,
    [RM_STORE]   = CLR_DARK_BROWN,
};

// exterior-wall touch bitmask
enum { T_N = 1, T_E = 2, T_S = 4, T_W = 8 };

typedef struct { Rect r; int label; int touch; } Room;
// an interior partition wall with one door gap. vert=1 → vertical wall at column x,
// spanning rows [y, y+len); door gap is rows [dpos, dpos+dlen). vert=0 → horizontal.
typedef struct { int x, y, len, vert, dpos, dlen; } Wall;

#define MAX_ROOMS 48
#define MAX_WALLS 48
typedef struct {
    Rect shell;          // exterior envelope (incl. 1px wall ring)
    int  type, outward;  // outward ∈ {0 N,1 E,2 S,3 W} = frontage/street side
    Room room[MAX_ROOMS]; int nroom;
    Wall wall[MAX_WALLS]; int nwall;
    int  egx, egy;       // entry-gap center on the frontage wall
    int  entry_room;     // room the entry opens into
} Plan;

// ── partition: recursive BSP split of the shell interior into a room tree ───────
typedef struct { int min_w, min_h, maxdepth; float bias_lo, bias_hi; } SplitP;
static const SplitP SPLIT[BT_N] = {
    [BT_RES] = { 14, 14, 4, 0.34f, 0.66f },   // many rooms
    [BT_COM] = { 20, 20, 2, 0.34f, 0.66f },   // a few — shop floor + back
    [BT_IND] = { 22, 22, 1, 0.70f, 0.84f },   // one big bay + a small office strip
};

static void emit_room(Plan *p, Rect r) {
    if (p->nroom >= MAX_ROOMS) return;
    int t = 0;
    if (r.y <= p->shell.y + 1)                       t |= T_N;
    if (r.y + r.h >= p->shell.y + p->shell.h - 1)    t |= T_S;
    if (r.x <= p->shell.x + 1)                       t |= T_W;
    if (r.x + r.w >= p->shell.x + p->shell.w - 1)    t |= T_E;
    p->room[p->nroom++] = (Room){ r, RM_BED, t };
}

static void bsp(Plan *p, Rect r, unsigned h, int depth, const SplitP *sp) {
    int canV = r.w >= 2 * sp->min_w;     // room to split into left|right
    int canH = r.h >= 2 * sp->min_h;     // room to split into top/bottom
    int leaf = (!canV && !canH) || depth >= sp->maxdepth || p->nroom >= MAX_ROOMS - 2;
    if (!leaf && depth > 0) {            // stochastic early stop → irregular tree
        float pstop = 0.08f + 0.13f * depth;
        if (frac01(hmix(h, 91)) < pstop) leaf = 1;
    }
    if (leaf) { emit_room(p, r); return; }

    int vert;                            // vertical wall (split along x)?
    if (canV && !canH)      vert = 1;
    else if (canH && !canV) vert = 0;
    else {                               // both possible: split the LONGER side, sometimes cross
        vert = (r.w >= r.h);
        if (frac01(hmix(h, 55)) < 0.22f) vert = !vert;
    }

    float bias = sp->bias_lo + frac01(hmix(h, 7)) * (sp->bias_hi - sp->bias_lo);
    if (vert) {
        int lo = r.x + sp->min_w, hi = r.x + r.w - sp->min_w;
        int s = r.x + (int)(bias * r.w); if (s < lo) s = lo; if (s > hi) s = hi;
        int dlen = 9, span = r.h - 2 * 4 - dlen;
        int dpos = r.y + 4 + (span > 0 ? (int)(frac01(hmix(h, 13)) * span) : 0);
        if (p->nwall < MAX_WALLS) p->wall[p->nwall++] = (Wall){ s, r.y, r.h, 1, dpos, dlen };
        bsp(p, (Rect){ r.x, r.y, s - r.x, r.h }, hmix(h, 1), depth + 1, sp);
        bsp(p, (Rect){ s, r.y, r.x + r.w - s, r.h }, hmix(h, 2), depth + 1, sp);
    } else {
        int lo = r.y + sp->min_h, hi = r.y + r.h - sp->min_h;
        int s = r.y + (int)(bias * r.h); if (s < lo) s = lo; if (s > hi) s = hi;
        int dlen = 9, span = r.w - 2 * 4 - dlen;
        int dpos = r.x + 4 + (span > 0 ? (int)(frac01(hmix(h, 13)) * span) : 0);
        if (p->nwall < MAX_WALLS) p->wall[p->nwall++] = (Wall){ r.x, s, r.w, 0, dpos, dlen };
        bsp(p, (Rect){ r.x, r.y, r.w, s - r.y }, hmix(h, 1), depth + 1, sp);
        bsp(p, (Rect){ r.x, s, r.w, r.y + r.h - s }, hmix(h, 2), depth + 1, sp);
    }
}

// ── the selector: emergent labels from area / frontage / adjacency ──────────────
static int front_bit(int outward) { return outward == 0 ? T_N : outward == 1 ? T_E : outward == 2 ? T_S : T_W; }

static void assign_labels(Plan *p) {
    int n = p->nroom; if (n == 0) return;
    int fb = front_bit(p->outward);
    long tot = 0;
    for (int i = 0; i < n; i++) { p->room[i].label = RM_BED; tot += (long)p->room[i].r.w * p->room[i].r.h; }
    long avg = tot / n;

    int big = -1, small = -1;            // largest / smallest by area
    for (int i = 0; i < n; i++) {
        long a = (long)p->room[i].r.w * p->room[i].r.h;
        if (big < 0   || a > (long)p->room[big].r.w   * p->room[big].r.h)   big = i;
        if (small < 0 || a < (long)p->room[small].r.w * p->room[small].r.h) small = i;
    }
    int front = -1;                      // largest room touching the frontage wall
    for (int i = 0; i < n; i++)
        if (p->room[i].touch & fb)
            if (front < 0 || (long)p->room[i].r.w * p->room[i].r.h > (long)p->room[front].r.w * p->room[front].r.h) front = i;

    if (p->type == BT_RES) {
        int liv = (front >= 0) ? front : big;
        p->room[liv].label = RM_LIVING;
        if (n > 1 && small != liv) p->room[small].label = RM_BATH;
        int ki = -1;                     // medium room on an exterior wall (plumbing) → kitchen
        for (int i = 0; i < n; i++) {
            if (i == liv || p->room[i].label == RM_BATH || !p->room[i].touch) continue;
            if (ki < 0 || (long)p->room[i].r.w * p->room[i].r.h > (long)p->room[ki].r.w * p->room[ki].r.h) ki = i;
        }
        if (ki >= 0) p->room[ki].label = RM_KITCHEN;
        int e = p->entry_room;           // entry into a small/anonymous room ⇒ a hall
        if (e >= 0 && e != liv && p->room[e].label == RM_BED &&
            (long)p->room[e].r.w * p->room[e].r.h < avg) p->room[e].label = RM_HALL;
    } else if (p->type == BT_COM) {
        for (int i = 0; i < n; i++) p->room[i].label = RM_BACK;   // back-of-house by default
        int shop = (front >= 0) ? front : big;
        p->room[shop].label = RM_SHOP;
        if (n > 1 && small != shop) p->room[small].label = RM_OFFICE;
    } else {                              // BT_IND
        for (int i = 0; i < n; i++) p->room[i].label = RM_STORE;
        p->room[big].label = RM_BAY;
        if (n > 1 && small != big) p->room[small].label = RM_OFFICE;
    }
}

// ── build one plan — the PURE FN (rect + type + outward + seed → whole plan) ────
static void plan_build(Plan *p, Rect shell, int type, int outward, unsigned h) {
    p->shell = shell; p->type = type; p->outward = outward;
    p->nroom = p->nwall = 0; p->entry_room = -1;
    Rect interior = { shell.x + 1, shell.y + 1, shell.w - 2, shell.h - 2 };
    if (interior.w < 4 || interior.h < 4) { emit_room(p, interior); }
    else bsp(p, interior, h, 0, &SPLIT[type]);

    // entry gap: centered on the frontage edge of the shell
    int cx = shell.x + shell.w / 2, cy = shell.y + shell.h / 2;
    int ix = cx, iy = cy;                // an interior point just inside the entry
    switch (outward) {
        case 0: p->egx = cx; p->egy = shell.y;              ix = cx; iy = shell.y + 2;            break;
        case 1: p->egx = shell.x + shell.w - 1; p->egy = cy; ix = shell.x + shell.w - 3; iy = cy; break;
        case 2: p->egx = cx; p->egy = shell.y + shell.h - 1; ix = cx; iy = shell.y + shell.h - 3; break;
        default:p->egx = shell.x; p->egy = cy;              ix = shell.x + 2; iy = cy;            break;
    }
    for (int i = 0; i < p->nroom; i++) {     // which room does the entry open into?
        Rect r = p->room[i].r;
        if (ix >= r.x && ix < r.x + r.w && iy >= r.y && iy < r.y + r.h) { p->entry_room = i; break; }
    }
    assign_labels(p);
}

// ════════════════════════════════════════════════════════════════════════════
//  QUERY == DRAW (the future collision/interaction seam)
// ════════════════════════════════════════════════════════════════════════════
static int room_at(const Plan *p, int x, int y) {
    for (int i = 0; i < p->nroom; i++) {
        Rect r = p->room[i].r;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return i;
    }
    return -1;
}
static int on_entry(const Plan *p, int x, int y) {       // within the entry gap?
    int half = 5;
    if (p->outward == 0 || p->outward == 2) return y >= p->egy - 1 && y <= p->egy + 1 && x > p->egx - half && x < p->egx + half;
    return x >= p->egx - 1 && x <= p->egx + 1 && y > p->egy - half && y < p->egy + half;
}
static int solid_at(const Plan *p, int x, int y) {       // is a wall here? (draw==query)
    Rect s = p->shell;
    if (x < s.x || x >= s.x + s.w || y < s.y || y >= s.y + s.h) return 0;   // outside
    int on_border = (x == s.x || x == s.x + s.w - 1 || y == s.y || y == s.y + s.h - 1);
    if (on_border) return on_entry(p, x, y) ? 0 : 1;
    for (int i = 0; i < p->nwall; i++) {
        Wall w = p->wall[i];
        if (w.vert) {
            if (x == w.x && y >= w.y && y < w.y + w.len && !(y >= w.dpos && y < w.dpos + w.dlen)) return 1;
        } else {
            if (y == w.y && x >= w.x && x < w.x + w.len && !(x >= w.dpos && x < w.dpos + w.dlen)) return 1;
        }
    }
    return 0;
}

// ════════════════════════════════════════════════════════════════════════════
//  RENDER — one plan, phase flags chosen by the active tab
// ════════════════════════════════════════════════════════════════════════════
#define WALL_EXT  CLR_BROWNISH_BLACK
#define WALL_INT  CLR_DARK_GREY
#define WIN_COL   CLR_BLUE
#define DOOR_COL  CLR_ORANGE
#define SLAB_COL  CLR_DARKER_GREY

typedef struct { bool slab, floors, ext, intw, win, doors, props; } Show;

// draw an axis-aligned exterior edge, skipping the entry gap
static void edge(int x0, int y0, int len, int vert, int gap0, int gaplen, int col) {
    for (int i = 0; i < len; i++) {
        int gx = vert ? y0 + i : x0 + i;
        if (gaplen > 0 && gx >= gap0 && gx < gap0 + gaplen) continue;
        if (vert) pset(x0, y0 + i, col); else pset(x0 + i, y0, col);
    }
}

static void furnish(const Room *rm) {
    Rect r = rm->r;
    int ix = r.x + 2, iy = r.y + 2, iw = r.w - 4, ih = r.h - 4;
    if (iw < 4 || ih < 4) return;
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    switch (rm->label) {
        case RM_BED:                                                       // bed against the top wall + pillow
            rectfill(ix, iy, iw * 3 / 5, ih / 2, CLR_INDIGO);
            rectfill(ix + 1, iy + 1, (iw * 3 / 5) - 2, 2, CLR_LIGHT_PEACH);
            break;
        case RM_LIVING:                                                    // sofa + coffee table
            rectfill(ix, iy + ih - 4, iw * 2 / 3, 3, CLR_DARK_RED);
            rectfill(cx - 3, cy, 6, 3, CLR_DARK_BROWN);
            break;
        case RM_KITCHEN:                                                   // counter L along two walls + stove
            rectfill(ix, iy, iw, 2, CLR_LIGHT_GREY);
            rectfill(ix, iy, 2, ih, CLR_LIGHT_GREY);
            rectfill(ix + iw - 3, iy + 1, 2, 2, CLR_DARK_GREY);
            break;
        case RM_BATH:                                                      // tub + toilet
            rectfill(ix, iy, iw / 2 + 1, 3, CLR_BLUE);
            circfill(ix + iw - 2, iy + ih - 2, 1, CLR_WHITE);
            break;
        case RM_SHOP:                                                      // shelving rows + counter near entry
            for (int yy = iy + 1; yy < iy + ih - 3; yy += 3) line(ix, yy, ix + iw - 1, yy, CLR_DARK_BROWN);
            rectfill(ix, iy + ih - 2, iw, 2, CLR_BROWN);
            break;
        case RM_OFFICE:                                                    // desk + chair
            rectfill(ix, iy, iw * 2 / 3, 3, CLR_DARK_BROWN);
            rectfill(ix + 1, iy + 4, 3, 3, CLR_DARK_GREY);
            break;
        case RM_BAY:                                                       // pallets/crates scattered
            for (int yy = iy + 2; yy < iy + ih - 3; yy += 7)
                for (int xx = ix + 2; xx < ix + iw - 3; xx += 8) {
                    if (hash2(xx, yy + lf_seed) & 1) continue;
                    rectfill(xx, yy, 4, 4, CLR_BROWN); rect(xx, yy, 4, 4, CLR_DARK_BROWN);
                }
            break;
        case RM_STORE: case RM_BACK:                                       // shelving rows
            for (int yy = iy + 1; yy < iy + ih - 1; yy += 3) line(ix, yy, ix + iw - 1, yy, CLR_DARK_BROWN);
            break;
        case RM_HALL:                                                      // a runner rug
            rectfill(cx - 1, iy, 3, ih, CLR_DARK_RED);
            break;
    }
}

static void plan_draw(const Plan *p, Show s) {
    Rect sh = p->shell;
    if (s.slab) rectfill(sh.x + 1, sh.y + 1, sh.w - 2, sh.h - 2, SLAB_COL);
    if (s.floors)
        for (int i = 0; i < p->nroom; i++) {
            Rect r = p->room[i].r;
            rectfill(r.x, r.y, r.w, r.h, RM_FLOOR[p->room[i].label]);
        }
    if (s.props) for (int i = 0; i < p->nroom; i++) furnish(&p->room[i]);

    if (s.intw)                                  // interior partition walls (door gaps left open)
        for (int i = 0; i < p->nwall; i++) {
            Wall w = p->wall[i];
            if (w.vert) {
                for (int y = w.y; y < w.y + w.len; y++) if (!(y >= w.dpos && y < w.dpos + w.dlen)) pset(w.x, y, WALL_INT);
            } else {
                for (int x = w.x; x < w.x + w.len; x++) if (!(x >= w.dpos && x < w.dpos + w.dlen)) pset(x, w.y, WALL_INT);
            }
        }
    if (s.doors)                                 // door jambs + entry mat
        for (int i = 0; i < p->nwall; i++) {
            Wall w = p->wall[i];
            if (w.vert) { pset(w.x, w.dpos, DOOR_COL); pset(w.x, w.dpos + w.dlen - 1, DOOR_COL); }
            else        { pset(w.dpos, w.y, DOOR_COL); pset(w.dpos + w.dlen - 1, w.y, DOOR_COL); }
        }

    if (s.ext) {                                 // exterior envelope with the entry gap carved out
        int gx = (p->outward == 0 || p->outward == 2) ? p->egx - 5 : 0;
        int gy = (p->outward == 1 || p->outward == 3) ? p->egy - 5 : 0;
        int gl = 10;
        edge(sh.x, sh.y, sh.w, 0, p->outward == 0 ? gx : -1, p->outward == 0 ? gl : 0, WALL_EXT);                    // N
        edge(sh.x, sh.y + sh.h - 1, sh.w, 0, p->outward == 2 ? gx : -1, p->outward == 2 ? gl : 0, WALL_EXT);         // S
        edge(sh.x, sh.y, sh.h, 1, p->outward == 3 ? gy : -1, p->outward == 3 ? gl : 0, WALL_EXT);                    // W
        edge(sh.x + sh.w - 1, sh.y, sh.h, 1, p->outward == 1 ? gy : -1, p->outward == 1 ? gl : 0, WALL_EXT);         // E
    }
    if (s.win) {                                 // windows: dashes on exterior walls, skip corners + frontage gap
        for (int x = sh.x + 6; x < sh.x + sh.w - 6; x += 7) {
            if (!(p->outward == 0 && abs(x - p->egx) < 7)) { pset(x, sh.y, WIN_COL); pset(x + 1, sh.y, WIN_COL); }
            if (!(p->outward == 2 && abs(x - p->egx) < 7)) { pset(x, sh.y + sh.h - 1, WIN_COL); pset(x + 1, sh.y + sh.h - 1, WIN_COL); }
        }
        for (int y = sh.y + 6; y < sh.y + sh.h - 6; y += 7) {
            if (!(p->outward == 3 && abs(y - p->egy) < 7)) { pset(sh.x, y, WIN_COL); pset(sh.x, y + 1, WIN_COL); }
            if (!(p->outward == 1 && abs(y - p->egy) < 7)) { pset(sh.x + sh.w - 1, y, WIN_COL); pset(sh.x + sh.w - 1, y + 1, WIN_COL); }
        }
    }
    if (s.doors) {                               // entry mat just inside the frontage gap
        int mx = p->egx, my = p->egy;
        switch (p->outward) {
            case 0: rectfill(mx - 4, my + 1, 9, 2, CLR_DARK_GREEN); break;
            case 2: rectfill(mx - 4, my - 2, 9, 2, CLR_DARK_GREEN); break;
            case 1: rectfill(mx - 2, my - 4, 2, 9, CLR_DARK_GREEN); break;
            default:rectfill(mx + 1, my - 4, 2, 9, CLR_DARK_GREEN); break;
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  THE WORKBENCH — shared inspection state every atom reads
// ════════════════════════════════════════════════════════════════════════════
#define MAX_LAYERS 4
static bool layer_on[MAX_LAYERS] = { true, true, true, true };
static bool dbg = false;            // G: BSP walls + door/entry markers
static int  ovl = 0;                // O: building-type field overlay (0 off / 1 on)
static int  cnt[3];
static const char *cnt_lbl[3];

// the building grid the whole workbench tiles (each cell = one independent building)
#define CELL_P 132
static int building_type_at(float x, float y) {
    float i = noise3(x * 0.0017f, y * 0.0017f, (float)lf_seed + 70.0f);
    return i < 0.50f ? BT_RES : i < 0.78f ? BT_COM : BT_IND;
}
// derive one building's params from its cell — shared by every tab + the probe
static void cell_plan(int cxi, int cyi, Plan *p) {
    int ox = cxi * CELL_P, oy = cyi * CELL_P;
    unsigned h = hash2(cxi + lf_seed * 149, cyi * 131 + 5);
    int type = building_type_at(ox + CELL_P / 2.0f, oy + CELL_P / 2.0f);
    int w = 86 + (int)(h % 30), ht = 74 + (int)((h >> 6) % 30);
    int outward = (h >> 12) & 3;
    Rect shell = { ox + (CELL_P - w) / 2, oy + (CELL_P - ht) / 2, w, ht };
    plan_build(p, shell, type, outward, hmix(h, 3));
}

// tile the visible building grid, drawing each plan with the tab's phase flags
static void tile(float cam_x, float cam_y, float zoom, Show base, float z_walls, float z_detail) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - CELL_P, Rpx = (int)(cx + hwv) + CELL_P;
    int Tpx = (int)(cy - hhv) - CELL_P, Bpx = (int)(cy + hhv) + CELL_P;

    Show s = base;                               // fold LOD into the phase flags
    if (zoom < z_walls)  { s.intw = false; s.ext = s.ext && false; }
    if (zoom < z_detail) { s.win = false; s.doors = false; s.props = false; }

    int nb = 0, nr = 0, nd = 0;
    for (int gy = ifloordiv(Tpx, CELL_P); gy <= ifloordiv(Bpx, CELL_P); gy++)
        for (int gx = ifloordiv(Lpx, CELL_P); gx <= ifloordiv(Rpx, CELL_P); gx++) {
            Plan p; cell_plan(gx, gy, &p);
            // yard/ground behind the building so PROGRAM reads like lots
            if (s.slab || s.floors || s.ext)
                rectfill(gx * CELL_P, gy * CELL_P, CELL_P, CELL_P,
                         (gx + gy + lf_seed) & 1 ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN);
            plan_draw(&p, s);
            if (dbg) {
                for (int i = 0; i < p.nwall; i++) {           // BSP walls (red) under inspection
                    Wall w = p.wall[i];
                    if (w.vert) line(w.x, w.y, w.x, w.y + w.len - 1, CLR_DARK_RED);
                    else        line(w.x, w.y, w.x + w.len - 1, w.y, CLR_DARK_RED);
                }
                circ(p.egx, p.egy, 1, CLR_GREEN);             // entry
            }
            nb++; nr += p.nroom; nd += p.nwall;
        }
    cnt[0] = nb; cnt_lbl[0] = "bldgs";
    cnt[1] = nr; cnt_lbl[1] = "rooms";
    cnt[2] = nd; cnt_lbl[2] = "doors";
}

// shared probe: report the building type + the room label under the crosshair
static const char *plan_probe(float fx, float fy) {
    static char buf[48];
    int gx = ifloordiv((int)fx, CELL_P), gy = ifloordiv((int)fy, CELL_P);
    Plan p; cell_plan(gx, gy, &p);
    int x = (int)fx, y = (int)fy;
    if (x < p.shell.x || x >= p.shell.x + p.shell.w || y < p.shell.y || y >= p.shell.y + p.shell.h) {
        snprintf(buf, sizeof buf, "%s  (outside)", BT_NAME[p.type]); return buf;
    }
    if (solid_at(&p, x, y)) { snprintf(buf, sizeof buf, "%s  wall", BT_NAME[p.type]); return buf; }
    int ri = room_at(&p, x, y);
    snprintf(buf, sizeof buf, "%s / %s", BT_NAME[p.type], ri >= 0 ? RM_NAME[p.room[ri].label] : "—");
    return buf;
}

// ── the atoms (tabs): each picks phase flags from layer_on[] ────────────────────
static void program_draw(float cx, float cy, float z) {
    Show s = { .slab = false, .floors = layer_on[0], .ext = layer_on[0],
               .intw = layer_on[1], .doors = layer_on[1], .win = layer_on[2], .props = layer_on[3] };
    tile(cx, cy, z, s, 0.4f, 0.9f);
}
static void shell_draw(float cx, float cy, float z) {
    Show s = { .slab = layer_on[0], .ext = layer_on[1], .doors = layer_on[2] };
    tile(cx, cy, z, s, 0.0f, 0.0f);
}
static void partition_draw(float cx, float cy, float z) {
    Show s = { .floors = layer_on[0], .ext = layer_on[1], .intw = layer_on[1] };
    tile(cx, cy, z, s, 0.4f, 0.0f);
}
static void circ_draw(float cx, float cy, float z) {
    Show s = { .floors = layer_on[0], .ext = layer_on[1], .intw = layer_on[1], .doors = layer_on[2] };
    tile(cx, cy, z, s, 0.4f, 0.9f);
}
static void fixtures_draw(float cx, float cy, float z) {
    Show s = { .slab = true, .ext = layer_on[0], .intw = layer_on[0], .win = layer_on[1], .doors = layer_on[2] };
    tile(cx, cy, z, s, 0.4f, 0.6f);
}
static void furnish_draw(float cx, float cy, float z) {
    Show s = { .floors = layer_on[0], .ext = layer_on[1], .intw = layer_on[1], .doors = layer_on[1], .props = layer_on[2] };
    tile(cx, cy, z, s, 0.4f, 0.6f);
}

typedef struct {
    const char *name; int n_layers; const char *layer[MAX_LAYERS];
    void (*draw)(float, float, float);
    const char *(*probe)(float, float);
} Atom;
static Atom atoms[] = {
    { "PROGRAM",     4, { "shell", "rooms", "fixtures", "furnish" }, program_draw,   plan_probe },
    { "SHELL",       3, { "slab", "envelope", "entry" },            shell_draw,     plan_probe },
    { "PARTITION",   2, { "floors", "walls" },                      partition_draw, plan_probe },
    { "CIRCULATION", 3, { "floors", "walls", "doors" },             circ_draw,      plan_probe },
    { "FIXTURES",    3, { "walls", "windows", "doors" },            fixtures_draw,  plan_probe },
    { "FURNISH",     3, { "floors", "walls", "props" },             furnish_draw,   plan_probe },
};
#define N_ATOMS ((int)(sizeof atoms / sizeof atoms[0]))

// ════════════════════════════════════════════════════════════════════════════
//  THE SHELL — free-fly explorer over the current atom
// ════════════════════════════════════════════════════════════════════════════
static float cam_x = 132, cam_y = 120;     // open framing a 2×2 of buildings
static float zoom  = 0.92f;
static int   seed  = 1;
static int   cur   = 0;

void init(void) { lf_seed = seed; }

void update(void) {
    float pan = 5.0f / zoom;
    if (key(KEY_RIGHT) || key('D')) cam_x += pan;
    if (key(KEY_LEFT)  || key('A')) cam_x -= pan;
    if (key(KEY_DOWN)  || key('S')) cam_y += pan;
    if (key(KEY_UP)    || key('W')) cam_y -= pan;
    if (key('Z')) zoom *= 1.04f;
    if (key('X')) zoom /= 1.04f;
    zoom += mouse_wheel() * 0.08f * zoom;
    zoom = clamp(zoom, 0.25f, 8.0f);
    if (keyp('R')) { seed++; lf_seed = seed; }
    if (keyp(KEY_TAB)) { cur = (cur + 1) % N_ATOMS;
                         for (int i = 0; i < MAX_LAYERS; i++) layer_on[i] = true; }
    for (int i = 0; i < atoms[cur].n_layers; i++)
        if (keyp('1' + i)) layer_on[i] = !layer_on[i];
    if (keyp('G')) dbg = !dbg;
    if (keyp('O')) ovl = (ovl + 1) % 2;
}

static const int TYPE_COL[BT_N] = { CLR_DARK_GREEN, CLR_YELLOW, CLR_RED };
static void draw_overlay(void) {            // building-type field tint
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int OV = 16;
    int L = ((int)(cx - hwv) / OV - 1) * OV, R = (int)(cx + hwv) + OV;
    int T = ((int)(cy - hhv) / OV - 1) * OV, B = (int)(cy + hhv) + OV;
    fillp(0xA5A5, -1);
    for (int wy = T; wy <= B; wy += OV)
        for (int wx = L; wx <= R; wx += OV)
            rectfill(wx, wy, OV, OV, TYPE_COL[building_type_at(wx + OV / 2.0f, wy + OV / 2.0f)]);
    fillp_reset();
}

void draw(void) {
    cls(CLR_BLACK);
    camera_ex((int)cam_x, (int)cam_y, zoom, 0);
    atoms[cur].draw(cam_x, cam_y, zoom);
    if (ovl) draw_overlay();

    camera(0, 0);
    int mx = SCREEN_W / 2, my = SCREEN_H / 2;
    line(mx - 5, my, mx + 5, my, CLR_WHITE);
    line(mx, my - 5, mx, my + 5, CLR_WHITE);

    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    char buf[72];
    int x = print(atoms[cur].name, 3, 1, CLR_YELLOW);
    snprintf(buf, sizeof buf, "  s%d  z%.1f  overlay:%s", seed, (double)zoom, ovl ? "type" : "off");
    print(buf, x, 1, CLR_LIGHT_GREY);

    rectfill(0, 9, SCREEN_W, 8, CLR_BLACK);
    font(FONT_SMALL);
    int lx = 3;
    for (int i = 0; i < atoms[cur].n_layers; i++) {
        snprintf(buf, sizeof buf, "%d:%s ", i + 1, atoms[cur].layer[i]);
        lx = print(buf, lx, 11, layer_on[i] ? CLR_GREEN : CLR_DARK_GREY);
    }
    lx += 6;
    for (int i = 0; i < 3; i++)
        if (cnt_lbl[i]) {
            snprintf(buf, sizeof buf, "%s:%d ", cnt_lbl[i], cnt[i]);
            lx = print(buf, lx, 11, CLR_LIGHT_GREY);
        }

    rectfill(0, SCREEN_H - 16, SCREEN_W, 16, CLR_BLACK);
    font(FONT_NORMAL);
    print(atoms[cur].probe(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f), 3, SCREEN_H - 16, CLR_WHITE);
    font(FONT_SMALL);
    print("WASD/ZX move  R seed  TAB atom  1-4 layers  G grid  O overlay",
          3, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
