#include "studio.h"
#include <stdio.h>
#include <math.h>

// ============================================================================
// CITYPLAN — a procedural CITY you can LIFT THE ROOF off.
//
//   ◄ ► ▲ ▼ / WASD  pan     Z / X (or wheel)  zoom     R  new seed
//   F  force-lift every roof (dollhouse)      G  debug grid
//
// ── what this is ──────────────────────────────────────────────────────────────
// The merge of lotfill + interiors. They were always the SAME fill-language at two
// scales — lotfill lays out the land/blocks/lots/streets BETWEEN buildings; interiors
// fills the floor-plan INSIDE one footprint. cityplan stacks them so you read both by
// ZOOMING:
//   • zoom OUT  → a city of ROOFS (lotfill's block grid: varied lots, terraced rows,
//                 yards, driveways — only bigger, so a footprint is room-capable).
//   • zoom IN   → the roof LIFTS and you see the rooms (interiors' BSP plan).
//   • zoom IN+  → the furniture too (beds, sofas, shop shelving).
//   • F         → force every roof off at once (the dollhouse/X-ray look).
//
// The reveal is pure LOD: a roof lifts only when its building is big enough ON SCREEN
// to read — so the wide city never shows a mush of beds, and generating the floor-plan
// (the expensive part) happens LAZILY, only for buildings you're close to.
//
// ── seams (inherited from both parents) ─────────────────────────────────────────
// 1. Pure fn of a hash-seed: one block → its lots; one footprint → its whole plan.
// 2. Draw == query: room_at/solid_at is the renderer run as a query (the collision seam).
// 3. LOD gates DRAWING + (here) GENERATING — far buildings cost only a roof rect.
// ============================================================================

static int lf_seed = 0;

static unsigned hash2(int a, int b) {                 // per-cell pseudo-random
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}
static unsigned hmix(unsigned h, int k) { return hash2((int)h, k * 2654435761u + 0x9e3779b9u); }
static int ifloordiv(int a, int b) { int q = a / b; if ((a % b) != 0 && ((a < 0) != (b < 0))) q--; return q; }
static int posmod(int a, int b) { int m = a % b; return m < 0 ? m + b : m; }
static float frac01(unsigned h) { return (h & 4095u) / 4095.0f; }     // 12 bits → [0,1]

typedef struct { int x, y, w, h; } Rect;              // a bounded region (world px)

// ════════════════════════════════════════════════════════════════════════════
//  THE CITY LAYER — lotfill's block → ring-of-varied-lots subdivision (scaled up)
// ════════════════════════════════════════════════════════════════════════════
#define BLK_P 230        // block pitch (world px) — big enough that lots hold a floor-plan
#define ST_W  12         // street width

enum { ZN_RES, ZN_COM, ZN_IND, ZN_N };                // residential / commercial / industrial
static const char *ZN_NAME[ZN_N] = { "residential", "commercial", "industrial" };

static int zone_at(float x, float y) {
    float i = noise3(x * 0.00075f, y * 0.00075f, (float)lf_seed + 90.0f);
    return i < 0.48f ? ZN_RES : i < 0.76f ? ZN_COM : ZN_IND;
}
// urban density: drives lot fineness + building massing (organic clusters)
static float urban_density_at(float x, float y) {
    float z = (float)lf_seed;
    float a = noise3(x * 0.0024f,       y * 0.0024f,       z + 500.0f);
    float b = noise3(x * 0.0024f * 2.5f, y * 0.0024f * 2.5f, z + 517.0f);
    return clamp(a * 0.62f + b * 0.38f, 0.0f, 1.0f);
}
static int massing_at(float x, float y, int zone, unsigned h) {       // 0..3 cottage→tower
    float v = urban_density_at(x, y) + (frac01(h >> 9) - 0.5f) * 0.25f;
    int base = (zone == ZN_IND) ? 1 : (zone == ZN_COM) ? 1 : 0;
    int bump = v > 0.78f ? 2 : v > 0.50f ? 1 : 0;
    int m = base + bump;
    return m > 3 ? 3 : m;
}

typedef struct { Rect lot; unsigned hash; unsigned char outward, attached, massing; } LotSlot;

static int run_split(int L, int target_w, unsigned h, int *bnd) {     // → k; fills bnd[0..k]
    int k = target_w > 0 ? L / target_w : 1;
    if (k < 1) k = 1; if (k > 6) k = 6;
    float w[6], sum = 0;
    for (int i = 0; i < k; i++) { w[i] = 0.6f + 1.5f * frac01(h >> (i * 3)); sum += w[i]; }
    float acc = 0; bnd[0] = 0;
    for (int i = 0; i < k; i++) { acc += w[i] / sum * (float)L; bnd[i + 1] = (int)(acc + 0.5f); }
    bnd[k] = L;
    return k;
}

// a block interior → a street-fronting RING of VARIED lots (+ a courtyard gap). The
// SAME subdivide lotfill uses, only the constants are bigger. Every perimeter lot
// fronts a street by construction (reachability theorem). Pure fn of (block, seed).
static int block_lots(int bx, int by, int zone, LotSlot *out, int cap) {
    int ox = bx * BLK_P, oy = by * BLK_P, inx = ox + ST_W, iny = oy + ST_W, IN = BLK_P - ST_W;
    unsigned bh = hash2(bx * 131 + lf_seed * 7, by * 89 + 5);
    float ud = urban_density_at(inx + IN / 2.0f, iny + IN / 2.0f);
    int D  = 50 + (int)(frac01(bh >> 3) * 26) + (zone == ZN_IND ? 8 : 0);     // ring depth 50..84
    if (D > IN / 2 - 6) D = IN / 2 - 6;
    int tw = (int)(88.0f - 34.0f * ud) + (zone == ZN_IND ? 16 : 0);          // target lot width
    int attach_res = (zone == ZN_RES && ud > 0.50f);                         // dense res → terraces
    int n = 0, bnd[7];
    for (int s = 0; s < 4 && n < cap; s++) {                                 // 4 sides of the ring
        int horiz = (s < 2);
        int runL  = horiz ? IN : (IN - 2 * D);
        if (runL < 16) continue;
        unsigned sh = hash2((int)bh + s * 1009, (int)(bh >> 7) + s * 17);
        int k = run_split(runL, tw, sh, bnd);
        for (int i = 0; i < k && n < cap; i++) {
            int a0 = bnd[i], lw = bnd[i + 1] - a0;
            if (lw < 16) continue;
            Rect r; int outward;
            if      (s == 0) { r = (Rect){ inx + a0,     iny,           lw, D }; outward = 0; }  // top
            else if (s == 1) { r = (Rect){ inx + a0,     iny + IN - D,  lw, D }; outward = 2; }  // bottom
            else if (s == 2) { r = (Rect){ inx,          iny + D + a0,  D, lw }; outward = 3; }  // left
            else             { r = (Rect){ inx + IN - D, iny + D + a0,  D, lw }; outward = 1; }  // right
            unsigned lh = hash2((int)sh + i * 2399, (int)(sh >> 5) - i * 41);
            out[n].lot = r; out[n].hash = lh; out[n].outward = (unsigned char)outward;
            out[n].attached = (unsigned char)(attach_res && lw <= 64);
            out[n].massing  = (unsigned char)massing_at(r.x + r.w / 2.0f, r.y + r.h / 2.0f, zone, lh);
            n++;
        }
    }
    return n;
}

// the building SHELL inside a lot: set back from the frontage, side gap (0 if terraced
// → neighbours share a party wall), back margin toward the courtyard. (lotfill's body.)
static int footprint_body(Rect lot, int outward, int zone, int attached, Rect *out) {
    int side = attached ? 0 : (zone == ZN_IND) ? 3 : 4;
    int back = (zone == ZN_IND) ? 3 : 4;
    int fs   = (zone == ZN_RES) ? (attached ? 5 : 12) : (zone == ZN_COM) ? 8 : 5;
    int ix = lot.x, iy = lot.y, iw = lot.w, ih = lot.h;
    switch (outward) {
        case 0: iy += fs;   ih -= fs + back; ix += side; iw -= 2 * side; break;  // fronts up
        case 2: iy += back; ih -= fs + back; ix += side; iw -= 2 * side; break;  // fronts down
        case 1: ix += back; iw -= fs + back; iy += side; ih -= 2 * side; break;  // fronts right
        case 3: ix += fs;   iw -= fs + back; iy += side; ih -= 2 * side; break;  // fronts left
    }
    if (iw < 6 || ih < 6) return 0;
    *out = (Rect){ ix, iy, iw, ih };
    return 1;
}

// ════════════════════════════════════════════════════════════════════════════
//  THE INTERIOR LAYER — interiors' BSP floor-plan (one footprint → its rooms)
// ════════════════════════════════════════════════════════════════════════════
enum { RM_HALL, RM_LIVING, RM_BED, RM_KITCHEN, RM_BATH,
       RM_SHOP, RM_BACK, RM_OFFICE, RM_BAY, RM_STORE, RM_N };
static const char *RM_NAME[RM_N] = { "hall", "living", "bedroom", "kitchen", "bath",
                                     "shop floor", "back-of-house", "office", "bay", "storage" };
static const int RM_FLOOR[RM_N] = {
    [RM_HALL]    = CLR_DARK_BROWN,  [RM_LIVING] = CLR_BROWN,      [RM_BED]   = CLR_DARK_PURPLE,
    [RM_KITCHEN] = CLR_DARK_GREY,   [RM_BATH]   = CLR_BLUE_GREEN, [RM_SHOP]  = CLR_LIGHT_GREY,
    [RM_BACK]    = CLR_DARKER_GREY, [RM_OFFICE] = CLR_MEDIUM_GREY,[RM_BAY]   = CLR_DARKER_GREY,
    [RM_STORE]   = CLR_DARK_BROWN,
};
enum { T_N = 1, T_E = 2, T_S = 4, T_W = 8 };          // exterior-wall touch bitmask

typedef struct { Rect r; int label; int touch; } Room;
typedef struct { int x, y, len, vert, dpos, dlen; } Wall;   // partition wall + one door gap

#define MAX_ROOMS 32
#define MAX_WALLS 32
typedef struct {
    Rect shell; int type, outward;
    Room room[MAX_ROOMS]; int nroom;
    Wall wall[MAX_WALLS]; int nwall;
    int  egx, egy, entry_room;
} Plan;

typedef struct { int min_w, min_h, maxdepth; float bias_lo, bias_hi; } SplitP;
static const SplitP SPLIT[ZN_N] = {
    [ZN_RES] = { 13, 13, 4, 0.34f, 0.66f },   // many small rooms
    [ZN_COM] = { 22, 22, 2, 0.34f, 0.66f },   // shop floor + back
    [ZN_IND] = { 26, 26, 1, 0.70f, 0.84f },   // one big bay + an office strip
};

static void emit_room(Plan *p, Rect r) {
    if (p->nroom >= MAX_ROOMS) return;
    int t = 0;
    if (r.y <= p->shell.y + 1)                    t |= T_N;
    if (r.y + r.h >= p->shell.y + p->shell.h - 1) t |= T_S;
    if (r.x <= p->shell.x + 1)                    t |= T_W;
    if (r.x + r.w >= p->shell.x + p->shell.w - 1) t |= T_E;
    p->room[p->nroom++] = (Room){ r, RM_BED, t };
}

static void bsp(Plan *p, Rect r, unsigned h, int depth, const SplitP *sp) {
    int canV = r.w >= 2 * sp->min_w, canH = r.h >= 2 * sp->min_h;
    int leaf = (!canV && !canH) || depth >= sp->maxdepth || p->nroom >= MAX_ROOMS - 2;
    if (!leaf && depth > 0 && frac01(hmix(h, 91)) < 0.08f + 0.13f * depth) leaf = 1;
    if (leaf) { emit_room(p, r); return; }

    int vert;
    if (canV && !canH)      vert = 1;
    else if (canH && !canV) vert = 0;
    else { vert = (r.w >= r.h); if (frac01(hmix(h, 55)) < 0.22f) vert = !vert; }

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

static int front_bit(int outward) { return outward == 0 ? T_N : outward == 1 ? T_E : outward == 2 ? T_S : T_W; }

static void assign_labels(Plan *p) {
    int n = p->nroom; if (n == 0) return;
    int fb = front_bit(p->outward);
    long tot = 0;
    for (int i = 0; i < n; i++) { p->room[i].label = RM_BED; tot += (long)p->room[i].r.w * p->room[i].r.h; }
    long avg = tot / n;
    int big = -1, small = -1;
    for (int i = 0; i < n; i++) {
        long a = (long)p->room[i].r.w * p->room[i].r.h;
        if (big < 0   || a > (long)p->room[big].r.w   * p->room[big].r.h)   big = i;
        if (small < 0 || a < (long)p->room[small].r.w * p->room[small].r.h) small = i;
    }
    int front = -1;
    for (int i = 0; i < n; i++)
        if (p->room[i].touch & fb)
            if (front < 0 || (long)p->room[i].r.w * p->room[i].r.h > (long)p->room[front].r.w * p->room[front].r.h) front = i;

    if (p->type == ZN_RES) {
        int liv = (front >= 0) ? front : big;
        p->room[liv].label = RM_LIVING;
        if (n > 1 && small != liv) p->room[small].label = RM_BATH;
        int ki = -1;
        for (int i = 0; i < n; i++) {
            if (i == liv || p->room[i].label == RM_BATH || !p->room[i].touch) continue;
            if (ki < 0 || (long)p->room[i].r.w * p->room[i].r.h > (long)p->room[ki].r.w * p->room[ki].r.h) ki = i;
        }
        if (ki >= 0) p->room[ki].label = RM_KITCHEN;
        int e = p->entry_room;
        if (e >= 0 && e != liv && p->room[e].label == RM_BED &&
            (long)p->room[e].r.w * p->room[e].r.h < avg) p->room[e].label = RM_HALL;
    } else if (p->type == ZN_COM) {
        for (int i = 0; i < n; i++) p->room[i].label = RM_BACK;
        int shop = (front >= 0) ? front : big;
        p->room[shop].label = RM_SHOP;
        if (n > 1 && small != shop) p->room[small].label = RM_OFFICE;
    } else {
        for (int i = 0; i < n; i++) p->room[i].label = RM_STORE;
        p->room[big].label = RM_BAY;
        if (n > 1 && small != big) p->room[small].label = RM_OFFICE;
    }
}

static void plan_build(Plan *p, Rect shell, int type, int outward, unsigned h) {
    p->shell = shell; p->type = type; p->outward = outward;
    p->nroom = p->nwall = 0; p->entry_room = -1;
    Rect interior = { shell.x + 1, shell.y + 1, shell.w - 2, shell.h - 2 };
    if (interior.w < 4 || interior.h < 4) emit_room(p, interior);
    else bsp(p, interior, h, 0, &SPLIT[type]);

    int cx = shell.x + shell.w / 2, cy = shell.y + shell.h / 2, ix = cx, iy = cy;
    switch (outward) {
        case 0: p->egx = cx; p->egy = shell.y;               ix = cx; iy = shell.y + 2;            break;
        case 1: p->egx = shell.x + shell.w - 1; p->egy = cy; ix = shell.x + shell.w - 3; iy = cy;  break;
        case 2: p->egx = cx; p->egy = shell.y + shell.h - 1; ix = cx; iy = shell.y + shell.h - 3;  break;
        default:p->egx = shell.x; p->egy = cy;               ix = shell.x + 2; iy = cy;            break;
    }
    for (int i = 0; i < p->nroom; i++) {
        Rect r = p->room[i].r;
        if (ix >= r.x && ix < r.x + r.w && iy >= r.y && iy < r.y + r.h) { p->entry_room = i; break; }
    }
    assign_labels(p);
}

// ── query == draw ────────────────────────────────────────────────────────────
static int room_at(const Plan *p, int x, int y) {
    for (int i = 0; i < p->nroom; i++) {
        Rect r = p->room[i].r;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return i;
    }
    return -1;
}

// ── interior render ────────────────────────────────────────────────────────────
#define WALL_EXT  CLR_BROWNISH_BLACK
#define WALL_INT  CLR_DARK_GREY
#define WIN_COL   CLR_BLUE
#define DOOR_COL  CLR_ORANGE
#define SLAB_COL  CLR_DARKER_GREY

typedef struct { bool floors, walls, win, doors, props; } Show;

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
        case RM_BED:
            rectfill(ix, iy, iw * 3 / 5, ih / 2, CLR_INDIGO);
            rectfill(ix + 1, iy + 1, (iw * 3 / 5) - 2, 2, CLR_LIGHT_PEACH);
            break;
        case RM_LIVING:
            rectfill(ix, iy + ih - 4, iw * 2 / 3, 3, CLR_DARK_RED);
            rectfill(cx - 3, cy, 6, 3, CLR_DARK_BROWN);
            break;
        case RM_KITCHEN:
            rectfill(ix, iy, iw, 2, CLR_LIGHT_GREY);
            rectfill(ix, iy, 2, ih, CLR_LIGHT_GREY);
            rectfill(ix + iw - 3, iy + 1, 2, 2, CLR_DARK_GREY);
            break;
        case RM_BATH:
            rectfill(ix, iy, iw / 2 + 1, 3, CLR_BLUE);
            circfill(ix + iw - 2, iy + ih - 2, 1, CLR_WHITE);
            break;
        case RM_SHOP:
            for (int yy = iy + 1; yy < iy + ih - 3; yy += 3) line(ix, yy, ix + iw - 1, yy, CLR_DARK_BROWN);
            rectfill(ix, iy + ih - 2, iw, 2, CLR_BROWN);
            break;
        case RM_OFFICE:
            rectfill(ix, iy, iw * 2 / 3, 3, CLR_DARK_BROWN);
            rectfill(ix + 1, iy + 4, 3, 3, CLR_DARK_GREY);
            break;
        case RM_BAY:
            for (int yy = iy + 2; yy < iy + ih - 3; yy += 7)
                for (int xx = ix + 2; xx < ix + iw - 3; xx += 8) {
                    if (hash2(xx, yy + lf_seed) & 1) continue;
                    rectfill(xx, yy, 4, 4, CLR_BROWN); rect(xx, yy, 4, 4, CLR_DARK_BROWN);
                }
            break;
        case RM_STORE: case RM_BACK:
            for (int yy = iy + 1; yy < iy + ih - 1; yy += 3) line(ix, yy, ix + iw - 1, yy, CLR_DARK_BROWN);
            break;
        case RM_HALL:
            rectfill(cx - 1, iy, 3, ih, CLR_DARK_RED);
            break;
    }
}

static void plan_draw(const Plan *p, Show s) {
    Rect sh = p->shell;
    rectfill(sh.x + 1, sh.y + 1, sh.w - 2, sh.h - 2, SLAB_COL);            // slab under everything
    if (s.floors)
        for (int i = 0; i < p->nroom; i++) {
            Rect r = p->room[i].r;
            rectfill(r.x, r.y, r.w, r.h, RM_FLOOR[p->room[i].label]);
        }
    if (s.props) for (int i = 0; i < p->nroom; i++) furnish(&p->room[i]);

    if (s.walls)
        for (int i = 0; i < p->nwall; i++) {
            Wall w = p->wall[i];
            if (w.vert) { for (int y = w.y; y < w.y + w.len; y++) if (!(y >= w.dpos && y < w.dpos + w.dlen)) pset(w.x, y, WALL_INT); }
            else        { for (int x = w.x; x < w.x + w.len; x++) if (!(x >= w.dpos && x < w.dpos + w.dlen)) pset(x, w.y, WALL_INT); }
        }
    if (s.doors)
        for (int i = 0; i < p->nwall; i++) {
            Wall w = p->wall[i];
            if (w.vert) { pset(w.x, w.dpos, DOOR_COL); pset(w.x, w.dpos + w.dlen - 1, DOOR_COL); }
            else        { pset(w.dpos, w.y, DOOR_COL); pset(w.dpos + w.dlen - 1, w.y, DOOR_COL); }
        }

    // exterior envelope with the entry gap carved on the frontage side
    int gx = (p->outward == 0 || p->outward == 2) ? p->egx - 5 : 0;
    int gy = (p->outward == 1 || p->outward == 3) ? p->egy - 5 : 0;
    int gl = 10;
    edge(sh.x, sh.y, sh.w, 0, p->outward == 0 ? gx : -1, p->outward == 0 ? gl : 0, WALL_EXT);
    edge(sh.x, sh.y + sh.h - 1, sh.w, 0, p->outward == 2 ? gx : -1, p->outward == 2 ? gl : 0, WALL_EXT);
    edge(sh.x, sh.y, sh.h, 1, p->outward == 3 ? gy : -1, p->outward == 3 ? gl : 0, WALL_EXT);
    edge(sh.x + sh.w - 1, sh.y, sh.h, 1, p->outward == 1 ? gy : -1, p->outward == 1 ? gl : 0, WALL_EXT);

    if (s.win) {
        for (int x = sh.x + 6; x < sh.x + sh.w - 6; x += 7) {
            if (!(p->outward == 0 && abs(x - p->egx) < 7)) { pset(x, sh.y, WIN_COL); pset(x + 1, sh.y, WIN_COL); }
            if (!(p->outward == 2 && abs(x - p->egx) < 7)) { pset(x, sh.y + sh.h - 1, WIN_COL); pset(x + 1, sh.y + sh.h - 1, WIN_COL); }
        }
        for (int y = sh.y + 6; y < sh.y + sh.h - 6; y += 7) {
            if (!(p->outward == 3 && abs(y - p->egy) < 7)) { pset(sh.x, y, WIN_COL); pset(sh.x, y + 1, WIN_COL); }
            if (!(p->outward == 1 && abs(y - p->egy) < 7)) { pset(sh.x + sh.w - 1, y, WIN_COL); pset(sh.x + sh.w - 1, y + 1, WIN_COL); }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  THE ROOF — what you see at city zoom (lotfill's footprint massing, as a lid)
// ════════════════════════════════════════════════════════════════════════════
static void draw_roof(Rect b, int zone, int massing, unsigned h) {
    static const int ROOF_RES [5] = { CLR_RED, CLR_DARK_RED, CLR_BROWN, CLR_PEACH, CLR_DARK_PURPLE };
    static const int ROOF_COM [4] = { CLR_DARK_GREY, CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_INDIGO };
    static const int ROOF_IND [3] = { CLR_DARKER_BLUE, CLR_DARKER_GREY, CLR_DARK_GREY };
    int col = zone == ZN_RES ? ROOF_RES[h % 5] : zone == ZN_COM ? ROOF_COM[h % 4] : ROOF_IND[h % 3];
    rectfill(b.x, b.y, b.w, b.h, col);
    rect(b.x, b.y, b.w, b.h, CLR_BROWNISH_BLACK);                          // eave outline (separates terraces)
    if (massing >= 1) {                                                   // taller block: lit top + window grid
        rectfill(b.x, b.y, b.w, 1, CLR_LIGHT_GREY);
        int vstep = massing >= 3 ? 3 : 4;
        for (int yy = b.y + 3; yy < b.y + b.h - 1; yy += vstep)
            for (int xx = b.x + 2; xx < b.x + b.w - 1; xx += 3) pset(xx, yy, CLR_DARKER_GREY);
    } else {                                                             // pitched-house ridge line
        if (b.w >= b.h) line(b.x + 1, b.y + b.h / 2, b.x + b.w - 2, b.y + b.h / 2, CLR_LIGHT_PEACH);
        else            line(b.x + b.w / 2, b.y + 1, b.x + b.w / 2, b.y + b.h - 2, CLR_LIGHT_PEACH);
    }
}

// a yard tree/bush + driveway in the setback of a DETACHED house (outdoor — always shown)
static void draw_tree(int x, int y, unsigned h) {
    int c = (h & 1) ? CLR_DARK_GREEN : CLR_MEDIUM_GREEN;
    circfill(x, y - 3, 3, CLR_DARK_GREEN); circfill(x - 1, y - 4, 2, c);
    rectfill(x, y - 1, 1, 3, CLR_DARK_BROWN);
}
static void draw_outdoor(Rect lot, Rect b, int outward, int zone, int attached, unsigned h, bool detail) {
    int dc = CLR_MEDIUM_GREY, dx = b.x + b.w / 2, dy = b.y + b.h / 2;     // driveway/path to the street
    switch (outward) {
        case 0: line(dx, lot.y, dx, b.y, dc);                         break;
        case 2: line(dx, b.y + b.h - 1, dx, lot.y + lot.h - 1, dc);   break;
        case 1: line(b.x + b.w - 1, dy, lot.x + lot.w - 1, dy, dc);   break;
        case 3: line(lot.x, dy, b.x, dy, dc);                         break;
    }
    if (detail && zone == ZN_RES && !attached) {                         // a yard tree in the front-yard strip
        int yx, yy;
        switch (outward) {
            case 0:  yx = lot.x + 3 + (int)(frac01(h) * (lot.w > 6 ? lot.w - 6 : 1)); yy = lot.y + 4; break;
            case 2:  yx = lot.x + 3 + (int)(frac01(h) * (lot.w > 6 ? lot.w - 6 : 1)); yy = lot.y + lot.h - 1; break;
            case 1:  yx = lot.x + lot.w - 2; yy = lot.y + 3 + (int)(frac01(h) * (lot.h > 6 ? lot.h - 6 : 1)); break;
            default: yx = lot.x + 3;         yy = lot.y + 3 + (int)(frac01(h) * (lot.h > 6 ? lot.h - 6 : 1)); break;
        }
        draw_tree(yx, yy, h);
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  THE DRIVER — tile the visible blocks; roof vs floor-plan chosen per-building
// ════════════════════════════════════════════════════════════════════════════
static bool roof_off = false;       // F: force every roof lifted (dollhouse)
static bool dbg      = false;       // G: block + lot grid
static int  n_bld, n_open;          // HUD tallies

#define ROOF_LIFT_Z 0.75f           // zoom at/above which roofs auto-lift
#define MIN_OPEN_PX 26              // a building must be ≥ this on screen to show rooms (else mush)

static void tile(float cam_x, float cam_y, float zoom) {
    if (zoom < 0.01f) zoom = 0.01f;
    float cx = cam_x + SCREEN_W / 2.0f, cy = cam_y + SCREEN_H / 2.0f;
    float hwv = (SCREEN_W / 2.0f) / zoom, hhv = (SCREEN_H / 2.0f) / zoom;
    int Lpx = (int)(cx - hwv) - BLK_P, Rpx = (int)(cx + hwv) + BLK_P;
    int Tpx = (int)(cy - hhv) - BLK_P, Bpx = (int)(cy + hhv) + BLK_P;

    bool want_peel = roof_off || zoom >= ROOF_LIFT_Z;
    bool outdoor_detail = zoom >= 0.32f;            // yard trees only when they'd read
    n_bld = n_open = 0;
    LotSlot slots[40];

    for (int by = ifloordiv(Tpx, BLK_P); by <= ifloordiv(Bpx, BLK_P); by++)
        for (int bx = ifloordiv(Lpx, BLK_P); bx <= ifloordiv(Rpx, BLK_P); bx++) {
            int ox = bx * BLK_P, oy = by * BLK_P, inx = ox + ST_W, iny = oy + ST_W, IN = BLK_P - ST_W;
            int zone = zone_at(inx + IN / 2.0f, iny + IN / 2.0f);
            rectfill(ox, oy, BLK_P, BLK_P, CLR_DARK_GREY);                                   // street band (L)
            rectfill(inx, iny, IN, IN, zone == ZN_RES ? CLR_DARK_GREEN : CLR_MEDIUM_GREY);   // block interior

            int nl = block_lots(bx, by, zone, slots, 40);
            for (int i = 0; i < nl; i++) {
                Rect lot = slots[i].lot, b;
                int outward = slots[i].outward, att = slots[i].attached;
                if (!footprint_body(lot, outward, zone, att, &b)) continue;
                n_bld++;
                draw_outdoor(lot, b, outward, zone, att, slots[i].hash, outdoor_detail);

                bool peel = want_peel && (b.w * zoom >= MIN_OPEN_PX || b.h * zoom >= MIN_OPEN_PX);
                if (!peel) { draw_roof(b, zone, slots[i].massing, slots[i].hash); continue; }

                Plan p; plan_build(&p, b, zone, outward, hmix(slots[i].hash, 3));
                Show s = { .floors = true, .walls = true,
                           .doors = zoom >= 0.95f, .win = zoom >= 0.95f, .props = zoom >= 1.5f };
                plan_draw(&p, s);
                n_open++;

                if (dbg) circ(p.egx, p.egy, 1, CLR_GREEN);                                   // entry marker
            }

            if (dbg) {
                rect(ox, oy, BLK_P, BLK_P, CLR_DARK_RED);
                for (int i = 0; i < nl; i++) rect(slots[i].lot.x, slots[i].lot.y, slots[i].lot.w, slots[i].lot.h, CLR_YELLOW);
            }
        }
}

// shared probe — zone + the room (or roof/street) under the crosshair
static const char *probe(float fx, float fy) {
    static char buf[52];
    int bx = ifloordiv((int)fx, BLK_P), by = ifloordiv((int)fy, BLK_P), IN = BLK_P - ST_W;
    int zone = zone_at(bx * BLK_P + ST_W + IN / 2.0f, by * BLK_P + ST_W + IN / 2.0f);
    if (posmod((int)fx, BLK_P) < ST_W || posmod((int)fy, BLK_P) < ST_W) {
        snprintf(buf, sizeof buf, "%s  street", ZN_NAME[zone]); return buf;
    }
    LotSlot slots[40]; int nl = block_lots(bx, by, zone, slots, 40);
    int x = (int)fx, y = (int)fy;
    for (int i = 0; i < nl; i++) {
        Rect b; if (!footprint_body(slots[i].lot, slots[i].outward, zone, slots[i].attached, &b)) continue;
        if (x < b.x || x >= b.x + b.w || y < b.y || y >= b.y + b.h) continue;
        Plan p; plan_build(&p, b, zone, slots[i].outward, hmix(slots[i].hash, 3));
        int ri = room_at(&p, x, y);
        snprintf(buf, sizeof buf, "%s / %s", ZN_NAME[zone], ri >= 0 ? RM_NAME[p.room[ri].label] : "wall");
        return buf;
    }
    snprintf(buf, sizeof buf, "%s  yard", ZN_NAME[zone]);
    return buf;
}

// ════════════════════════════════════════════════════════════════════════════
//  SHELL — free-fly explorer
// ════════════════════════════════════════════════════════════════════════════
static float cam_x = 360, cam_y = 360;     // open on a city block, zoomed out (roofs)
static float zoom  = 0.34f;
static int   seed  = 1;

void init(void) { lf_seed = seed; }

void update(void) {
    float pan = 7.0f / zoom;
    if (key(KEY_RIGHT) || key('D')) cam_x += pan;
    if (key(KEY_LEFT)  || key('A')) cam_x -= pan;
    if (key(KEY_DOWN)  || key('S')) cam_y += pan;
    if (key(KEY_UP)    || key('W')) cam_y -= pan;
    if (key('Z')) zoom *= 1.04f;
    if (key('X')) zoom /= 1.04f;
    zoom += mouse_wheel() * 0.08f * zoom;
    zoom = clamp(zoom, 0.12f, 5.0f);
    if (keyp('R')) { seed++; lf_seed = seed; }
    if (keyp('F')) roof_off = !roof_off;
    if (keyp('G')) dbg = !dbg;
}

void draw(void) {
    cls(CLR_BLACK);
    camera_ex((int)cam_x, (int)cam_y, zoom, 0);
    tile(cam_x, cam_y, zoom);

    camera(0, 0);
    int mx = SCREEN_W / 2, my = SCREEN_H / 2;
    line(mx - 5, my, mx + 5, my, CLR_WHITE);
    line(mx, my - 5, mx, my + 5, CLR_WHITE);

    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    char buf[80];
    const char *mode = roof_off ? "DOLLHOUSE" : zoom >= ROOF_LIFT_Z ? "PEELED" : "ROOFS";
    int x = print("CITYPLAN", 3, 1, CLR_YELLOW);
    snprintf(buf, sizeof buf, "  s%d  z%.2f  %s   open %d/%d", seed, (double)zoom, mode, n_open, n_bld);
    print(buf, x, 1, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H - 16, SCREEN_W, 16, CLR_BLACK);
    font(FONT_NORMAL);
    print(probe(cam_x + SCREEN_W / 2.0f, cam_y + SCREEN_H / 2.0f), 3, SCREEN_H - 16, CLR_WHITE);
    font(FONT_SMALL);
    print("WASD/ZX move   R seed   F lift roofs   G grid   (zoom in = roofs lift)",
          3, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
