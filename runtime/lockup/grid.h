// ─────────────────────────────────────────────────────────────────────────────
// lockup/grid.h — MODULE: grid.  THE WORLD MODEL for LOCKUP.
// Design: docs/design/lockup.md.  Contract: lockup/model.h (frozen; read it first).
//
// OWNS (defines) ─ Tile lk_t[LK_N] · Room lk_room[LK_MAXROOM] · int lk_nroom
//                  int lk_dirty_struct
// Reads (never writes) ─ lk_a[] / lk_nact (actors, for door proximity + floor wear)
// Writes into the cart's `lk` ─ ONLY lk.n_beds, lk.n_cells (and lk.seed at init).
//
// ── THE FIVE ALGORITHMS ─────────────────────────────────────────────────────
//
// 1. THE PLOT (lk_grid_init).  Value-noise dirt/grass/gravel outdoors, a public
//    asphalt road along the north edge, a fenced compound with a delivery gate,
//    and a small starting block: five valid cells, a VALID office, an INVALID
//    canteen (tables + benches, no serving table) and an empty yard.  Legible on
//    frame 1, obviously unfinished — the first thing the player does is finish it.
//    Every cosmetic choice comes from a hash of (x,y,seed), so the plot is
//    byte-identical across saves, replays and platforms.  No rnd() anywhere in
//    this module (the global stream belongs to whoever else is pulling from it).
//
// 2. WALL JOINS (lk_joins_refresh).  An 8-bit neighbour mask per wall/door tile:
//    bit0 N, 1 E, 2 S, 3 W, 4 NE, 5 SE, 6 SW, 7 NW.  A bit is set when that
//    neighbour presents a JOIN: a wall of the same *group* (solid = brick/
//    concrete, mesh = fence/perimeter), or a door of any kind — a doorway is a
//    gap IN a wall, so the wall either side of it reads continuous instead of
//    ending in two stubs.  Doors get a mask too (it tells the renderer which way
//    the door hangs).  Any structural edit refreshes the tile AND its 8
//    neighbours; the renderer trusts this completely.
//
// 3. ROOMS ARE DISCOVERED, NEVER PLACED.  lk_paint_room records INTENT in
//    Tile.paint; lk_rooms_rebuild flood-fills each painted region (4-connected,
//    same paint type, bounded by walls AND doors) and then JUDGES it: enclosure,
//    required objects, min area, capacity from objects present.  Room.missing is
//    the FIRST absent required object, because the UI shows it as the reason and
//    a wrong reason is worse than none.  Room 0 is reserved as "no room".
//    lk_rooms_touch is the incremental path: it frees only the room(s) around one
//    tile and re-derives them, preferring each region's PREVIOUS id so a prisoner
//    keeps their cell when you drop a sink in it.  Splits and merges both fall
//    out of this for free.
//
// 4. ENCLOSURE == an outdoor flood, not a per-room escape test.  One BFS from
//    every map-border tile across everything that isn't a ROOFED barrier gives
//    lkg_outdoor[]; a room leaks iff any member tile is outdoor.  "Roofed" means
//    the solid wall group (brick/concrete) and doors set into it — a fence has no
//    roof, so a fenced compound is still outdoors and the yard, the weather and
//    the daylight all behave.  This is also exactly lk_indoors().
//
// 5. CONSTRUCTION.  A designation lives on the tile (job / job_arg / work) plus a
//    512-entry index so claiming is a scan of the queue, not of the map.  A job
//    APPLIES ITSELF on completion (lk_job_progress) through the same mutators the
//    player's tools use, so there is exactly one code path that can change the
//    world — which is what keeps joins, rooms and lk_dirty_struct honest.
//
// ── IMPLEMENTATION NOTES ────────────────────────────────────────────────────
// * lk_dirty_struct is set on ANY change that can alter routing OR the location
//   of a facility: walls, doors, locks, objects (solid or not — a new toilet
//   invalidates the FF_TOILET flow field), and paint (FF_YARD/FF_SOLITARY are
//   room-derived).  Floor material alone does not set it (walkability unchanged).
//   As a SAFETY NET this module also fingerprints the set of locked doors every
//   frame and sets lk_dirty_struct itself when it changes — so lockdown really
//   re-routes prisoners even if the caller that flipped Tile.locked forgot.
// * DOORS AND WALLS ARE MUTUALLY EXCLUSIVE.  Setting a door clears the wall and
//   vice versa, because lk_can_pass tests `wall != WL_NONE → never pass` first
//   (as the contract specifies).  A door tile still joins its wall run (§2), so
//   it draws as part of the structure.
// * OBJECT FOOTPRINTS DO NOT ROTATE.  LK_OBJ.w/h is always w tiles east and h
//   tiles south of the origin, because the frozen sprite layout (s, s+1 across,
//   s+8 down) is fixed-orientation art.  Tile.obj_dir is FACING only (cosmetic /
//   which side an actor uses it from).  Renderer + actors: please assume the same.
// * Tile.obj_used is BINARY, per the contract ("1 = an actor occupies this").  So
//   a bunk bed's 2 slots count toward room capacity but only one actor can be in
//   the object at a time.  Flagged, not fixed: fixing it means changing the
//   contract's meaning of obj_used, which would break every parallel module.
// * Tile.var is a FULL 0..255 hash byte (entropy for speckle, brick offset, tuft
//   placement).  Renderer: mask before indexing a 2-entry table — LK_FLOOR_SPR
//   [floor][var & 1], never [var].
// * DEVIATION (documented, deliberate) — CAPACITY.  Taken literally, "sum the
//   slots of objects matching cap_obj" leaves half the room types permanently
//   full: cap_obj is OB_NONE for a yard and a storeroom (sum = 0), and a desk, a
//   cooker and a workshop table all carry slots == 0 (so office / kitchen /
//   workshop / visitation = 0 too).  So: cap_obj OB_NONE → cap = area (a yard is
//   limited by its floor), and a matching capacity object always counts at least
//   one slot.  Cell = beds, canteen = 2 per bench, kitchen = 1 per cooker.
// * A capacity-LESS object is not a seat either: lk_room_free_slot won't hand out
//   a canteen table (slots 0, serves ND_COMFORT) while the room has benches, or
//   twelve prisoners would "sit" in a room with eight bench slots.  See the
//   comment on that function for the exact rule and why cooks still find cookers.
// * DEVIATION: OB_BUNK satisfies an OB_BED requirement and counts for OB_BED
//   capacity (a bunk bed is a bed).  Nothing else is aliased.
// * lk_job_claim accepts any non-prisoner role (the queue is workmen's work in
//   practice; refusing other staff outright risked a silent no-op if the actors
//   module passes a different role).  It does NOT set Tile.claimed — the caller
//   does, after it has confirmed a path.
// * JOB ENCODING for the econ module: job_arg is FL_* for JB_FLOOR, WL_* for
//   JB_WALL, DR_* for JB_DOOR, OB_* for JB_OBJECT, 0 for JB_DEMOLISH.  Cost is
//   therefore LK_FLOOR_COST / LK_WALL_COST / LK_DOOR_COST / LK_OBJ[].cost — or
//   just call `lkg_job_cost(job, arg)` (non-static, tag-prefixed so it cannot
//   collide; the only symbol here that isn't in the contract).  A JB_OBJECT
//   designation takes its facing from Tile.obj_dir — set it before or after
//   lk_queue, this module never clobbers it.
// * Filth only RISES indoors (nothing in the contract cleans it; there is no
//   janitor role) and weathers away slowly outdoors.  A freshly laid floor is
//   clean.  If a cleaning mechanic lands, it wants Tile.dirty -= x.
// * lk_nroom is a HIGH-WATER MARK (exclusive bound), not a count: iterate
//   `for (i = 1; i < lk_nroom; i++)` and skip slots with type == RM_NONE, which
//   are free.  Real ids start at 1.
// * lk_grid_update wants the SIM delta (already multiplied by lk.speed), like
//   every other _update — at speed 4 doors swing and floors soil four times as
//   fast, which is what you want.  d <= 0 is a no-op, so pause freezes the world.
// * Static footprint ≈ 350 KB: lk_t is 172 KB and the scratch fields (flood
//   queues, visit stamps, the sub-unit filth accumulator) another 170 KB.  All
//   BSS, all fixed — no allocation anywhere in this module.
// * SELF-TESTED: 170 assertions over the plot, joins, permissions, enclosure,
//   room split/merge, capacity contention, footprints, the job pipeline, doors,
//   filth and determinism — including a 4000-random-edit stress run proving
//   lk_rooms_touch agrees with lk_rooms_rebuild on partition, judgement, join
//   masks and the outdoor field.  Worth folding into the cart's spec().
// ─────────────────────────────────────────────────────────────────────────────
#ifndef LOCKUP_GRID_H
#define LOCKUP_GRID_H

#include "studio.h"
#include "lockup/model.h"

// ── owned globals ───────────────────────────────────────────────────────────
Tile lk_t[LK_N];
Room lk_room[LK_MAXROOM];
int  lk_nroom       = 1;    // high-water mark; slot 0 is reserved "no room"
int  lk_dirty_struct = 1;

// ── module state (all lkg_-tagged; one shared translation unit) ──────────────
#define LKG_DOOR_SPEED  1160.0f   // door_open units/sec → ~0.22s to swing
#define LKG_FILTH_RATE     0.35f  // Tile.dirty per second under a standing actor
#define LKG_WEATHER_RATE   0.40f  // filth washed off an outdoor tile per second

static unsigned int   lkg_seed;
static unsigned char  lkg_outdoor[LK_N];        // 1 = the sky can reach this tile
static int            lkg_out_dirty = 1;
static int            lkg_oq[LK_N];             // outdoor-flood queue
static int            lkg_q[LK_N];              // room-fill queue AND member list
static int            lkg_stamp[LK_N];          // visited stamps (no clearing pass)
static int            lkg_stampv;
static unsigned short lkg_oldroom[LK_N];        // room id before a scoped rebuild
static int            lkg_cand[LK_N];           // scoped-rebuild candidate seeds
static short          lkg_rx0[LK_MAXROOM], lkg_ry0[LK_MAXROOM];   // room bbox, for
static short          lkg_rx1[LK_MAXROOM], lkg_ry1[LK_MAXROOM];   // slot scans
static int            lkg_jobs[LK_MAXJOB];      // queued designations
static int            lkg_njob;
static int            lkg_astamp[LK_N];         // actor-proximity stamp (doors)
static unsigned char  lkg_arole[LK_N];          // roles present near that tile
static int            lkg_astampv;
static float          lkg_wear[LK_N];           // sub-unit filth (Tile.dirty is a byte)
static float          lkg_tally_t;
static unsigned int   lkg_lockprint;            // fingerprint of the locked set
static int            lkg_bulk;                 // 1 = bulk world build in progress

static const int LKG_DX[8] = {  0,  1,  0, -1,  1,  1, -1, -1 };  // N E S W NE SE SW NW
static const int LKG_DY[8] = { -1,  0,  1,  0, -1,  1,  1, -1 };

static void lkg_out_refresh(void);
static void lkg_tally(void);
static void lkg_recount_used(void);

// ── deterministic hashing.  Cosmetics must be identical on every machine, so
// they come from (x, y, seed) and never from the shared rnd() stream. ─────────
static unsigned int lkg_hash(int x, int y, unsigned int s) {
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u
                   + s * 2654435761u + 0x9e3779b9u;
    h ^= h >> 13; h *= 1274126177u;
    h ^= h >> 16; h *= 2246822519u;
    return h ^ (h >> 15);
}
static float lkg_h01(int x, int y, unsigned int s) {
    return (float)(lkg_hash(x, y, s) >> 8) * (1.0f / 16777216.0f);
}
static float lkg_smooth(float t) { return t * t * (3.0f - 2.0f * t); }
// own floor(), because this header includes only studio.h + model.h (no math.h) —
// and an int truncation would fold the lattice around x = 0.
static int lkg_ifloor(float v) { int i = (int)v; return (v < (float)i) ? i - 1 : i; }
// value noise: hashed lattice + smoothstep interpolation → organic blobs, not
// blocky squares.  Cheap enough to call twice per tile at init.
static float lkg_noise(float x, float y, unsigned int s) {
    int   xi = lkg_ifloor(x), yi = lkg_ifloor(y);
    float fx = lkg_smooth(x - (float)xi), fy = lkg_smooth(y - (float)yi);
    float a = lkg_h01(xi,     yi,     s), b = lkg_h01(xi + 1, yi,     s);
    float c = lkg_h01(xi,     yi + 1, s), d = lkg_h01(xi + 1, yi + 1, s);
    float top = a + (b - a) * fx, bot = c + (d - c) * fx;
    return top + (bot - top) * fy;
}
static float lkg_fbm(float x, float y, unsigned int s) {
    return lkg_noise(x, y, s) * 0.64f + lkg_noise(x * 2.7f, y * 2.7f, s ^ 0x51ed27u) * 0.36f;
}

// ── tiny predicates ─────────────────────────────────────────────────────────
static bool lkg_ok(int c) { return c >= 0 && c < LK_N; }
static bool lkg_staff(int role) {
    return role == RL_GUARD || role == RL_WORKMAN || role == RL_COOK || role == RL_DOCTOR;
}
// two wall groups: solid masonry, and mesh (fence/perimeter).  Only same-group
// walls join — a fence butting a brick corner shouldn't weld into it.
static int lkg_wgroup(int w) { return (w == WL_FENCE || w == WL_PERIM) ? 1 : 0; }
// a bunk IS a bed, for requirements and for capacity.  Nothing else is aliased.
static bool lkg_obj_meets(int have, int want) {
    if (have == want) return true;
    return want == OB_BED && have == OB_BUNK;
}

// Tile.dirty is a BYTE and wear is a fraction of a unit per frame, so it has to
// be accumulated in floating point first — casting 0.006 into the byte every
// frame would leave the floor forever spotless, and casting 99.9996 back would
// scrub a unit off it sixty times a second.  Both bugs, both silent; hence this.
static void lkg_soil(int c, float amount) {
    lkg_wear[c] += amount;
    if (lkg_wear[c] >= 1.0f) {
        int whole = (int)lkg_wear[c];
        lkg_wear[c] -= (float)whole;
        int v = (int)lk_t[c].dirty + whole;
        lk_t[c].dirty = (unsigned char)(v > 255 ? 255 : v);
    } else if (lkg_wear[c] <= -1.0f) {
        int whole = (int)(-lkg_wear[c]);
        lkg_wear[c] += (float)whole;
        int v = (int)lk_t[c].dirty - whole;
        lk_t[c].dirty = (unsigned char)(v < 0 ? 0 : v);
    }
}

int lk_cell_at(float wx, float wy) {
    int x = lkg_ifloor(wx / (float)LK_TS), y = lkg_ifloor(wy / (float)LK_TS);
    if (x < 0) x = 0; if (x >= LK_MW) x = LK_MW - 1;
    if (y < 0) y = 0; if (y >= LK_MH) y = LK_MH - 1;
    return lk_idx(x, y);
}

// ── objects: origin lookup ──────────────────────────────────────────────────
// Covered tiles carry obj_ref = 1 and no obj, so the origin is found by scanning
// back at most one tile north/west (the largest footprint in LK_OBJ is 2×2).
static int lkg_obj_origin(int c) {
    if (!lkg_ok(c)) return -1;
    if (lk_t[c].obj != OB_NONE) return c;
    if (!lk_t[c].obj_ref) return -1;
    int x = lk_tx(c), y = lk_ty(c);
    for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
        if (!dx && !dy) continue;
        if (!lk_in(x - dx, y - dy)) continue;
        int n = lk_idx(x - dx, y - dy), ob = lk_t[n].obj;
        if (ob != OB_NONE && dx < LK_OBJ[ob].w && dy < LK_OBJ[ob].h) return n;
    }
    return -1;                    // orphaned ref (shouldn't happen) — treated as clear
}
static bool lkg_obj_solid(int c) {
    int o = lkg_obj_origin(c);
    return o >= 0 && LK_OBJ[lk_t[o].obj].solid != 0;
}
// does object `ob` fit with its origin at c?  Used both to validate a placement
// and to reject an impossible designation at queue time.
static bool lkg_fits(int c, int ob) {
    if (!lkg_ok(c) || ob <= OB_NONE || ob >= OB_COUNT) return false;
    int w = LK_OBJ[ob].w, h = LK_OBJ[ob].h, x = lk_tx(c), y = lk_ty(c);
    for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) {
        if (!lk_in(x + dx, y + dy)) return false;
        const Tile *t = &lk_t[lk_idx(x + dx, y + dy)];
        if (t->wall != WL_NONE || t->door != DR_NONE) return false;
        if (t->obj != OB_NONE || t->obj_ref) return false;
    }
    return true;
}
// remove the WHOLE object owning tile c, from any of its tiles.
static void lkg_obj_remove(int c) {
    int o = lkg_obj_origin(c);
    if (o < 0) { if (lkg_ok(c)) { lk_t[c].obj_ref = 0; lk_t[c].obj_used = 0; } return; }
    int ob = lk_t[o].obj, w = LK_OBJ[ob].w, h = LK_OBJ[ob].h;
    int x = lk_tx(o), y = lk_ty(o);
    for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) {
        if (!lk_in(x + dx, y + dy)) continue;
        Tile *t = &lk_t[lk_idx(x + dx, y + dy)];
        t->obj = OB_NONE; t->obj_ref = 0; t->obj_used = 0; t->obj_dir = 0;
    }
}

// ── walkability ─────────────────────────────────────────────────────────────
bool lk_solid(int c) {
    if (!lkg_ok(c)) return true;                   // off-map is a wall
    if (lk_t[c].wall != WL_NONE) return true;
    return lkg_obj_solid(c);
}

// The permission layer.  This one function is why a lockdown genuinely re-routes.
bool lk_can_pass(int c, int role) {
    if (!lkg_ok(c)) return false;
    const Tile *t = &lk_t[c];
    if (t->wall != WL_NONE) return false;          // walls never pass
    if (t->door != DR_NONE) {                      // a door is traversable-but-permissioned
        switch (t->door) {
        case DR_PLAIN: return true;
        case DR_STAFF: return lkg_staff(role);
        case DR_JAIL:
        case DR_GATE:
            if (lkg_staff(role))    return true;   // staff carry keys
            if (role == RL_PRISONER) return !t->locked;
            return false;
        default: return true;
        }
    }
    return !lkg_obj_solid(c);                      // a solid object never passes
}

bool lk_indoors(int c) {
    if (!lkg_ok(c)) return false;
    if (lkg_out_dirty) lkg_out_refresh();
    return !lkg_outdoor[c];
}

// ── the outdoor flood: what has a roof over it ───────────────────────────────
// Blocked only by the SOLID wall group and by doors set into solid walls; a fence
// or a perimeter has no roof, so a fenced compound stays outdoors (and the yard,
// the rain and the daylight all keep working).  4-connected on purpose: air does
// not leak through a diagonal wall join, and neither does a room's enclosure.
static bool lkg_blocks_out(int c) {
    const Tile *t = &lk_t[c];
    if (t->wall != WL_NONE) return lkg_wgroup(t->wall) == 0;
    if (t->door != DR_NONE) {                      // roofed iff it's in masonry
        int x = lk_tx(c), y = lk_ty(c);
        for (int d = 0; d < 4; d++) {
            if (!lk_in(x + LKG_DX[d], y + LKG_DY[d])) continue;
            const Tile *n = &lk_t[lk_idx(x + LKG_DX[d], y + LKG_DY[d])];
            if (n->wall != WL_NONE && lkg_wgroup(n->wall) == 0) return true;
        }
        return false;
    }
    return false;
}
static void lkg_out_refresh(void) {
    for (int i = 0; i < LK_N; i++) lkg_outdoor[i] = 0;
    int head = 0, tail = 0;
    for (int x = 0; x < LK_MW; x++) {              // every border tile is a sky source
        int a = lk_idx(x, 0), b = lk_idx(x, LK_MH - 1);
        if (!lkg_blocks_out(a) && !lkg_outdoor[a]) { lkg_outdoor[a] = 1; lkg_oq[tail++] = a; }
        if (!lkg_blocks_out(b) && !lkg_outdoor[b]) { lkg_outdoor[b] = 1; lkg_oq[tail++] = b; }
    }
    for (int y = 0; y < LK_MH; y++) {
        int a = lk_idx(0, y), b = lk_idx(LK_MW - 1, y);
        if (!lkg_blocks_out(a) && !lkg_outdoor[a]) { lkg_outdoor[a] = 1; lkg_oq[tail++] = a; }
        if (!lkg_blocks_out(b) && !lkg_outdoor[b]) { lkg_outdoor[b] = 1; lkg_oq[tail++] = b; }
    }
    while (head < tail) {
        int c = lkg_oq[head++], x = lk_tx(c), y = lk_ty(c);
        for (int d = 0; d < 4; d++) {
            int nx = x + LKG_DX[d], ny = y + LKG_DY[d];
            if (!lk_in(nx, ny)) continue;
            int n = lk_idx(nx, ny);
            if (lkg_outdoor[n] || lkg_blocks_out(n)) continue;
            lkg_outdoor[n] = 1; lkg_oq[tail++] = n;
        }
    }
    lkg_out_dirty = 0;
}

// ── wall join masks ─────────────────────────────────────────────────────────
// Does neighbour tile n present a join to a wall of material `kind`?
// kind == WL_NONE means "the asker is a door", which joins any wall.
static bool lkg_joins_with(int n, int kind) {
    const Tile *t = &lk_t[n];
    if (t->door != DR_NONE) return true;           // a doorway is a gap IN a wall
    if (t->wall == WL_NONE)  return false;
    if (kind == WL_NONE)     return true;
    return lkg_wgroup(t->wall) == lkg_wgroup(kind);
}
static void lkg_join_one(int c) {
    Tile *t = &lk_t[c];
    if (t->wall == WL_NONE && t->door == DR_NONE) { t->joins = 0; return; }
    int x = lk_tx(c), y = lk_ty(c), kind = t->wall;
    unsigned int m = 0;
    for (int b = 0; b < 8; b++) {
        int nx = x + LKG_DX[b], ny = y + LKG_DY[b];
        if (!lk_in(nx, ny)) continue;              // off-map is nothing, not a join
        if (lkg_joins_with(lk_idx(nx, ny), kind)) m |= 1u << b;
    }
    t->joins = (unsigned char)m;
}
void lk_joins_refresh(int c) {
    if (!lkg_ok(c) || lkg_bulk) return;
    lkg_join_one(c);
    int x = lk_tx(c), y = lk_ty(c);
    for (int b = 0; b < 8; b++)
        if (lk_in(x + LKG_DX[b], y + LKG_DY[b])) lkg_join_one(lk_idx(x + LKG_DX[b], y + LKG_DY[b]));
}

// ═══ ROOMS ═══════════════════════════════════════════════════════════════════
static int lkg_room_alloc(int prefer) {
    // Prefer the region's PREVIOUS id: adding a sink to a cell must not renumber
    // it out from under the prisoner whose Actor.cell points at it.
    if (prefer > 0 && prefer < LK_MAXROOM && lk_room[prefer].type == RM_NONE) {
        if (prefer + 1 > lk_nroom) lk_nroom = prefer + 1;
        return prefer;
    }
    for (int i = 1; i < LK_MAXROOM; i++) if (lk_room[i].type == RM_NONE) {
        if (i + 1 > lk_nroom) lk_nroom = i + 1;
        return i;
    }
    return -1;                                     // 255 rooms is the cap
}
static void lkg_room_clear(int rid) {
    Room *r = &lk_room[rid];
    r->type = RM_NONE; r->valid = 0; r->missing = 0; r->leaks = 0;
    r->area = 0; r->cap = 0; r->used = 0; r->cx = 0; r->cy = 0; r->seed_tile = -1;
    lkg_rx0[rid] = lkg_ry0[rid] = 0; lkg_rx1[rid] = lkg_ry1[rid] = -1;
}

// Flood one painted region from `seed` and JUDGE it.  Assumes lkg_stampv is
// current and lkg_outdoor[] is fresh.
static void lkg_fill_room(int seed, int prefer) {
    if (!lkg_ok(seed) || lkg_stamp[seed] == lkg_stampv) return;
    int type = lk_t[seed].paint;
    if (type <= RM_NONE || type >= RM_COUNT) return;
    if (lk_t[seed].wall != WL_NONE || lk_t[seed].door != DR_NONE) return;

    int head = 0, tail = 0;
    lkg_stamp[seed] = lkg_stampv; lkg_q[tail++] = seed;
    while (head < tail) {
        int c = lkg_q[head++], x = lk_tx(c), y = lk_ty(c);
        for (int d = 0; d < 4; d++) {
            int nx = x + LKG_DX[d], ny = y + LKG_DY[d];
            if (!lk_in(nx, ny)) continue;
            int n = lk_idx(nx, ny);
            if (lkg_stamp[n] == lkg_stampv) continue;
            const Tile *t = &lk_t[n];
            if (t->paint != type) continue;                        // same intent only
            if (t->wall != WL_NONE || t->door != DR_NONE) continue; // bounded by both
            lkg_stamp[n] = lkg_stampv; lkg_q[tail++] = n;
        }
    }
    int nmem = tail;

    int rid = lkg_room_alloc(prefer);
    if (rid < 0) { for (int i = 0; i < nmem; i++) lk_t[lkg_q[i]].room = 0; return; }
    Room *r = &lk_room[rid];
    lkg_room_clear(rid);
    r->type = (unsigned char)type; r->seed_tile = (short)seed;
    r->area = (unsigned short)(nmem > 65535 ? 65535 : nmem);

    const RoomDef *rd = &LK_ROOM[type];
    unsigned char present[OB_COUNT];
    for (int i = 0; i < OB_COUNT; i++) present[i] = 0;
    long sx = 0, sy = 0;
    int  cap = 0, used = 0;
    int  x0 = LK_MW, y0 = LK_MH, x1 = -1, y1 = -1;

    for (int i = 0; i < nmem; i++) {
        int c = lkg_q[i], x = lk_tx(c), y = lk_ty(c);
        Tile *t = &lk_t[c];
        t->room = (unsigned short)rid;
        sx += x; sy += y;
        if (x < x0) x0 = x; if (x > x1) x1 = x;
        if (y < y0) y0 = y; if (y > y1) y1 = y;
        // enclosure: the sky reaching any member tile — or the region running off
        // the edge of the world — is a leak.
        if (lkg_outdoor[c]) r->leaks = 1;
        if (x == 0 || y == 0 || x == LK_MW - 1 || y == LK_MH - 1) r->leaks = 1;
        int ob = t->obj;
        if (ob != OB_NONE) {
            present[ob] = 1;
            if (lkg_obj_meets(ob, rd->cap_obj)) {
                // A capacity object always carries at least one slot: a desk, a
                // cooker and a workshop table all have slots == 0 in the frozen
                // table, and taking that literally would leave every office,
                // kitchen and workshop at capacity ZERO — i.e. permanently full.
                cap += LK_OBJ[ob].slots > 0 ? LK_OBJ[ob].slots : 1;
                if (t->obj_used) used++;
            }
        }
    }
    lkg_rx0[rid] = (short)x0; lkg_ry0[rid] = (short)y0;
    lkg_rx1[rid] = (short)x1; lkg_ry1[rid] = (short)y1;

    // centroid, snapped to a member tile so it is a legal goal as well as a label
    float ax = (float)sx / (float)nmem, ay = (float)sy / (float)nmem;
    int best = seed; float bd = 1e30f;
    for (int i = 0; i < nmem; i++) {
        float dx = (float)lk_tx(lkg_q[i]) - ax, dy = (float)lk_ty(lkg_q[i]) - ay;
        float dd = dx * dx + dy * dy;
        if (dd < bd) { bd = dd; best = lkg_q[i]; }
    }
    r->cx = (short)lk_tx(best); r->cy = (short)lk_ty(best);

    // capacity is derived from OBJECTS, never from area — that is what makes the
    // contention real.  Exception: a room with no capacity object (yard, store)
    // is limited by its floor instead of being capped at zero.
    r->cap  = (unsigned short)(rd->cap_obj == OB_NONE ? r->area : cap);
    r->used = (unsigned short)used;

    // ── judgement.  missing is the FIRST absent requirement, because the HUD
    // prints it as the reason and a wrong reason is worse than no reason. ──
    r->valid = 1;
    if (rd->enclosed && r->leaks)  r->valid = 0;
    if (r->area < rd->min_area)    r->valid = 0;
    for (int k = 0; k < 4; k++) {
        int rq = rd->req[k];
        if (!rq) break;
        bool found = false;
        for (int ob = 1; ob < OB_COUNT && !found; ob++)
            if (present[ob] && lkg_obj_meets(ob, rq)) found = true;
        if (!found) { r->missing = (unsigned char)rq; r->valid = 0; break; }
    }
}

void lk_rooms_rebuild(void) {
    if (lkg_out_dirty) lkg_out_refresh();
    for (int i = 0; i < LK_MAXROOM; i++) lkg_room_clear(i);
    lk_nroom = 1;
    for (int c = 0; c < LK_N; c++) lk_t[c].room = 0;
    lkg_stampv++;
    for (int c = 0; c < LK_N; c++)
        if (lk_t[c].paint != RM_NONE) lkg_fill_room(c, 0);
    lkg_tally();
}

// Incremental: free only the room(s) around c and re-derive them.  A wall dropped
// mid-room SPLITS it (two fills, one new id); a wall removed MERGES two (one fill
// re-uses one id, the other slot goes free).  Both fall out of "free, then re-fill
// from the freed tiles".
void lk_rooms_touch(int c) {
    if (!lkg_ok(c) || lkg_bulk) return;
    if (lkg_out_dirty) lkg_out_refresh();

    int rids[9], nr = 0;
    bool anypaint = (lk_t[c].paint != RM_NONE);
    int cx = lk_tx(c), cy = lk_ty(c);
    for (int b = -1; b < 8; b++) {
        int n = c;
        if (b >= 0) {
            int nx = cx + LKG_DX[b], ny = cy + LKG_DY[b];
            if (!lk_in(nx, ny)) continue;
            n = lk_idx(nx, ny);
        }
        if (lk_t[n].paint != RM_NONE) anypaint = true;
        int r = lk_t[n].room;
        if (r <= 0) continue;
        bool dup = false;
        for (int k = 0; k < nr; k++) if (rids[k] == r) dup = true;
        if (!dup && nr < 9) rids[nr++] = r;
    }
    if (nr == 0 && !anypaint) return;              // the common case: building on bare land

    // Free the affected rooms, remembering each tile's old id so a re-fill can
    // ask for it back.  One pass over 6144 tiles — cheaper than it looks, and
    // never once per frame (only on a structural edit).
    int ncand = 0;
    for (int i = 0; i < LK_N; i++) {
        int r = lk_t[i].room;
        if (r <= 0) continue;
        for (int k = 0; k < nr; k++) if (rids[k] == r) {
            lkg_oldroom[i] = (unsigned short)r;
            lk_t[i].room = 0;
            lkg_cand[ncand++] = i;
            break;
        }
    }
    for (int k = 0; k < nr; k++) lkg_room_clear(rids[k]);
    // the edited tile and its neighbours may be newly painted and belong to no
    // room yet — they are seeds too.
    for (int b = -1; b < 8 && ncand < LK_N; b++) {
        int n = c;
        if (b >= 0) {
            int nx = cx + LKG_DX[b], ny = cy + LKG_DY[b];
            if (!lk_in(nx, ny)) continue;
            n = lk_idx(nx, ny);
        }
        if (lk_t[n].paint != RM_NONE && lk_t[n].room == 0) { lkg_oldroom[n] = 0; lkg_cand[ncand++] = n; }
    }

    lkg_stampv++;
    for (int i = 0; i < ncand; i++) {
        int s = lkg_cand[i];
        if (lk_t[s].room != 0) continue;           // already re-absorbed by a fill
        lkg_fill_room(s, lkg_oldroom[s]);
    }
    lkg_tally();
}

void lk_paint_room(int c, int type) {
    if (!lkg_ok(c) || type < RM_NONE || type >= RM_COUNT) return;
    if (lk_t[c].paint == type) return;
    lk_t[c].paint = (unsigned char)type;
    lk_dirty_struct = 1;                           // room-derived flow fields moved
    lk_rooms_touch(c);
}

int lk_room_of(int c) { return lkg_ok(c) ? lk_t[c].room : 0; }

int lk_room_find(int type, int nth) {
    for (int i = 1; i < lk_nroom; i++) {
        const Room *r = &lk_room[i];
        if (r->type != type || !r->valid) continue;
        if (nth-- <= 0) return i;
    }
    return -1;
}

// The capacity contention the whole game rests on: a free object in this room
// that serves `need`.  Deterministic scan order (lowest tile first) so a replay
// hands out the same bed twice.  Scans the room's bbox, not the map.
//
// An object with slots == 0 contributes no capacity, so it is NOT a seat: a
// canteen table serves ND_COMFORT with 0 slots, and handing it out would let
// twelve prisoners "sit" in a room with eight bench slots.  If the room holds any
// slotted object for this need, only slotted ones are eligible — even when they
// are all taken (that IS the contention).  Only when nothing slotted serves the
// need do capacity-less utilities count, which is how a cook still finds the
// kitchen's cooker (slots 0, serves ND_COUNT).
int lk_room_free_slot(int rid, int need) {
    if (rid <= 0 || rid >= LK_MAXROOM) return -1;
    if (lk_room[rid].type == RM_NONE) return -1;
    int  slotted = -1, plain = -1;
    bool any_slotted = false;
    for (int y = lkg_ry0[rid]; y <= lkg_ry1[rid]; y++)
        for (int x = lkg_rx0[rid]; x <= lkg_rx1[rid]; x++) {
            int c = lk_idx(x, y);
            const Tile *t = &lk_t[c];
            if (t->room != rid || t->obj == OB_NONE) continue;
            if (LK_OBJ[t->obj].serves != need) continue;
            if (LK_OBJ[t->obj].slots > 0) {
                any_slotted = true;
                if (!t->obj_used && slotted < 0) slotted = c;
            } else if (!t->obj_used && plain < 0) plain = c;
        }
    if (any_slotted) return slotted;
    return plain;
}

void lk_room_release(int c) {
    int o = lkg_obj_origin(c);
    if (o < 0) { if (lkg_ok(c)) lk_t[c].obj_used = 0; return; }
    if (!lk_t[o].obj_used) return;
    lk_t[o].obj_used = 0;
    int rid = lk_t[o].room;
    if (rid > 0 && rid < LK_MAXROOM && lk_room[rid].used > 0
        && lkg_obj_meets(lk_t[o].obj, LK_ROOM[lk_room[rid].type].cap_obj))
        lk_room[rid].used--;
}

// lk.n_cells / lk.n_beds — the two Sim fields this module maintains.  Only VALID
// rooms count, so the HUD's bed total agrees with what lk_room_find will hand out.
static void lkg_tally(void) {
    int beds = 0, cells = 0;
    for (int i = 1; i < lk_nroom; i++) {
        const Room *r = &lk_room[i];
        if (r->type == RM_NONE || !r->valid) continue;
        if (r->type == RM_CELL || r->type == RM_DORM) { cells++; beds += r->cap; }
    }
    lk.n_cells = cells; lk.n_beds = beds;
}
// Room.used is re-derived rather than tracked, because lk_room_free_slot hands
// out a tile without claiming it (the caller sets obj_used once it commits).
static void lkg_recount_used(void) {
    for (int i = 1; i < lk_nroom; i++) if (lk_room[i].type != RM_NONE) lk_room[i].used = 0;
    for (int c = 0; c < LK_N; c++) {
        const Tile *t = &lk_t[c];
        if (t->room == 0 || t->obj == OB_NONE || !t->obj_used) continue;
        int rid = t->room;
        if (rid >= LK_MAXROOM || lk_room[rid].type == RM_NONE) continue;
        if (lkg_obj_meets(t->obj, LK_ROOM[lk_room[rid].type].cap_obj)) lk_room[rid].used++;
    }
}

// ═══ TILE MUTATORS — the ONLY paths that change the world ════════════════════
void lk_set_floor(int c, int mat) {
    if (!lkg_ok(c) || mat < 0 || mat >= FL_COUNT) return;
    if (lk_t[c].floor == mat) return;
    lk_t[c].floor = (unsigned char)mat;
    lk_t[c].dirty = 0;                             // a fresh surface is a clean one
}

void lk_set_wall(int c, int mat) {
    if (!lkg_ok(c) || mat < 0 || mat >= WL_COUNT) return;
    if (lk_t[c].wall == mat) return;
    if (mat != WL_NONE) {                          // walls and doors are exclusive
        lkg_obj_remove(c);
        lk_t[c].door = DR_NONE; lk_t[c].door_open = 0; lk_t[c].locked = 0;
    }
    lk_t[c].wall = (unsigned char)mat;
    lk_joins_refresh(c);
    lkg_out_dirty = 1; lk_dirty_struct = 1;
    lk_rooms_touch(c);
}

void lk_set_door(int c, int kind) {
    if (!lkg_ok(c) || kind < 0 || kind >= DR_COUNT) return;
    if (lk_t[c].door == kind && (kind == DR_NONE || lk_t[c].wall == WL_NONE)) return;
    if (kind != DR_NONE) {
        lkg_obj_remove(c);
        lk_t[c].wall = WL_NONE;                    // the door replaces the wall it sits in
    }
    lk_t[c].door = (unsigned char)kind;
    lk_t[c].door_open = 0; lk_t[c].locked = 0;
    lk_joins_refresh(c);
    lkg_out_dirty = 1; lk_dirty_struct = 1;
    lk_rooms_touch(c);
}

bool lk_place_obj(int c, int ob, int dir) {
    if (!lkg_fits(c, ob)) return false;
    int w = LK_OBJ[ob].w, h = LK_OBJ[ob].h, x = lk_tx(c), y = lk_ty(c);
    for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) {
        Tile *t = &lk_t[lk_idx(x + dx, y + dy)];
        t->obj = OB_NONE; t->obj_ref = 1; t->obj_used = 0;
    }
    Tile *o = &lk_t[c];
    o->obj = (unsigned char)ob; o->obj_ref = 0;
    o->obj_dir = (unsigned char)(dir & 3); o->obj_used = 0;
    lk_dirty_struct = 1;        // solid → routing; any object → facility flow fields
    lk_rooms_touch(c);          // requirements + capacity may have just changed
    return true;
}

void lk_clear_tile(int c) {
    if (!lkg_ok(c)) return;
    lkg_obj_remove(c);                             // the WHOLE object, from any tile
    lk_t[c].wall = WL_NONE;
    lk_t[c].door = DR_NONE; lk_t[c].door_open = 0; lk_t[c].locked = 0;
    lk_joins_refresh(c);
    lkg_out_dirty = 1; lk_dirty_struct = 1;
    lk_rooms_touch(c);
}

// ═══ CONSTRUCTION QUEUE ══════════════════════════════════════════════════════
// Non-static and tag-prefixed so the econ module can bill a designation without
// re-deriving the encoding.  (The one symbol here outside the contract.)
int lkg_job_cost(int job, int arg) {
    switch (job) {
    case JB_FLOOR:  return (arg >= 0 && arg < FL_COUNT) ? LK_FLOOR_COST[arg] : 0;
    case JB_WALL:   return (arg >= 0 && arg < WL_COUNT) ? LK_WALL_COST[arg]  : 0;
    case JB_DOOR:   return (arg >= 0 && arg < DR_COUNT) ? LK_DOOR_COST[arg]  : 0;
    case JB_OBJECT: return (arg > OB_NONE && arg < OB_COUNT) ? LK_OBJ[arg].cost : 0;
    default:        return 0;                      // demolition is free
    }
}
// Work seconds.  Scaled off cost so a pool table takes visibly longer than a
// chair, but bounded — a workman must finish something while you watch.
static float lkg_work_for(int job, int arg) {
    float cost = (float)lkg_job_cost(job, arg);
    switch (job) {
    case JB_FLOOR:    return 1.0f + cost * 0.05f;      // 1.0 dirt .. 1.6 wood
    case JB_WALL:     return 2.0f + cost * 0.03f;      // 2.3 fence .. 3.8 perimeter
    case JB_DOOR:     return 3.0f + cost * 0.006f;     // 3.3 .. 4.8
    case JB_OBJECT:   return 1.5f + cost * 0.004f;     // 1.7 chair .. 3.9 pool table
    case JB_DEMOLISH: return 2.5f;
    default:          return 1.0f;
    }
}
static void lkg_job_add(int c) { if (lkg_njob < LK_MAXJOB) lkg_jobs[lkg_njob++] = c; }
static void lkg_job_del(int c) {
    for (int i = 0; i < lkg_njob; i++)
        if (lkg_jobs[i] == c) { lkg_jobs[i] = lkg_jobs[--lkg_njob]; return; }
}

void lk_queue(int c, int job, int arg) {
    if (!lkg_ok(c) || job <= JB_NONE || job >= JB_COUNT) return;
    Tile *t = &lk_t[c];
    if (t->job != JB_NONE) {
        if (t->job == job && t->job_arg == arg) return;   // already designated
        if (t->claimed) return;                           // someone is on it; leave it
        lk_unqueue(c);
    }
    if (lkg_njob >= LK_MAXJOB) return;
    switch (job) {                                        // refuse no-ops + impossibles
    case JB_FLOOR:
        if (arg < 0 || arg >= FL_COUNT || t->floor == arg || t->wall != WL_NONE) return;
        break;
    case JB_WALL:
        if (arg <= WL_NONE || arg >= WL_COUNT || t->wall == arg) return;
        break;
    case JB_DOOR:
        if (arg <= DR_NONE || arg >= DR_COUNT || t->door == arg) return;
        break;
    case JB_OBJECT:
        if (!lkg_fits(c, arg)) return;
        break;
    case JB_DEMOLISH:
        if (t->wall == WL_NONE && t->door == DR_NONE && lkg_obj_origin(c) < 0) return;
        break;
    default: return;
    }
    t->job = (unsigned char)job; t->job_arg = (unsigned char)arg;
    t->claimed = 0; t->work = lkg_work_for(job, arg);
    lkg_job_add(c);
}

void lk_unqueue(int c) {
    if (!lkg_ok(c) || lk_t[c].job == JB_NONE) return;
    lk_t[c].job = JB_NONE; lk_t[c].job_arg = 0; lk_t[c].claimed = 0; lk_t[c].work = 0;
    lkg_job_del(c);
}

int lk_job_count(void) { return lkg_njob; }

// Nearest unclaimed designation, by squared tile distance, ties to the lower tile
// index (deterministic).  Does NOT claim it — the actors module sets Tile.claimed
// once it has a path, so an unreachable job doesn't lock a workman out.
int lk_job_claim(int from_c, int role) {
    if (role == RL_PRISONER) return -1;
    int fx = LK_MW / 2, fy = LK_MH / 2;
    if (lkg_ok(from_c)) { fx = lk_tx(from_c); fy = lk_ty(from_c); }
    int best = -1; long bd = 0;
    for (int i = 0; i < lkg_njob; i++) {
        int c = lkg_jobs[i];
        const Tile *t = &lk_t[c];
        if (t->job == JB_NONE || t->claimed) continue;
        long dx = lk_tx(c) - fx, dy = lk_ty(c) - fy;
        long d = dx * dx + dy * dy;
        if (best < 0 || d < bd || (d == bd && c < best)) { best = c; bd = d; }
    }
    return best;
}

// A finished job APPLIES ITSELF through the ordinary mutators, so joins, rooms
// and lk_dirty_struct can't drift from what the player's tools would have done.
void lk_job_progress(int c, float work) {
    if (!lkg_ok(c)) return;
    Tile *t = &lk_t[c];
    if (t->job == JB_NONE) return;
    t->work -= work;
    if (t->work > 0.0f) return;
    int job = t->job, arg = t->job_arg, dir = t->obj_dir;
    t->job = JB_NONE; t->job_arg = 0; t->claimed = 0; t->work = 0.0f;
    lkg_job_del(c);
    switch (job) {
    case JB_FLOOR:    lk_set_floor(c, arg); break;
    case JB_WALL:     lk_set_wall(c, arg);  break;
    case JB_DOOR:     lk_set_door(c, arg);  break;
    case JB_OBJECT:   lk_place_obj(c, arg, dir); break;
    case JB_DEMOLISH: lk_clear_tile(c); break;
    default: break;
    }
    lk_dirty_struct = 1;
}

// ═══ PER-FRAME ═══════════════════════════════════════════════════════════════
void lk_grid_update(float d) {
    if (d <= 0.0f) return;                         // paused: the world holds still

    // ── one pass over the actors: stamp who is near which tile, and wear the
    // floor they stand on.  Cheap (≤ 256 actors × 9 tiles) and it feeds the door
    // animation below without a per-door actor search. ──
    lkg_astampv++;
    for (int i = 0; i < lk_nact; i++) {
        const Actor *a = &lk_a[i];
        if (!a->alive) continue;
        int c = lk_cell_at(a->x, a->y), x = lk_tx(c), y = lk_ty(c);
        if (lk_t[c].floor != FL_DIRT && lk_t[c].floor != FL_GRASS)
            lkg_soil(c, LKG_FILTH_RATE * d * (a->role == RL_PRISONER ? 1.0f : 0.5f));
        for (int b = -1; b < 8; b++) {
            int n = c;
            if (b >= 0) {
                int nx = x + LKG_DX[b], ny = y + LKG_DY[b];
                if (!lk_in(nx, ny)) continue;
                n = lk_idx(nx, ny);
            }
            if (lkg_astamp[n] != lkg_astampv) { lkg_astamp[n] = lkg_astampv; lkg_arole[n] = 0; }
            lkg_arole[n] |= (unsigned char)(1 << a->role);
        }
    }

    // ── doors + weather, one map sweep ──
    unsigned int lockprint = 0x811c9dc5u;
    for (int c = 0; c < LK_N; c++) {
        Tile *t = &lk_t[c];
        if (t->door != DR_NONE) {
            if (t->locked) lockprint = (lockprint ^ (unsigned int)c) * 16777619u;
            // A door opens for someone standing in or beside it who is ALLOWED
            // through — a jail door does not swing wide for a prisoner in lockdown,
            // and a staff door ignores them entirely.  Otherwise it closes.
            bool want = false;
            if (lkg_astamp[c] == lkg_astampv) {
                for (int r = 0; r < RL_COUNT && !want; r++)
                    if ((lkg_arole[c] & (1 << r)) && lk_can_pass(c, r)) want = true;
            }
            float target = want ? 255.0f : 0.0f, cur = (float)t->door_open;
            float step = LKG_DOOR_SPEED * d;
            if (cur < target) cur = (cur + step > target) ? target : cur + step;
            else if (cur > target) cur = (cur - step < target) ? target : cur - step;
            t->door_open = (unsigned char)cur;
        }
        // outdoors, the weather takes the filth away; indoors nothing does (no
        // janitor role in the contract — flagged in the notes).
        if (t->dirty && lkg_outdoor[c]) lkg_soil(c, -LKG_WEATHER_RATE * d);
    }
    // SAFETY NET: whoever flips Tile.locked (lockdown, a zone change) may forget
    // that it changes lk_can_pass.  Notice it here so re-routing can't silently
    // fail to happen.
    if (lockprint != lkg_lockprint) { lkg_lockprint = lockprint; lk_dirty_struct = 1; }

    lkg_tally_t += d;
    if (lkg_tally_t >= 0.25f) { lkg_tally_t = 0.0f; lkg_recount_used(); lkg_tally(); }
}

// ═══ THE PLOT ════════════════════════════════════════════════════════════════
// Raw bulk builders — no per-tile join/room refresh (lkg_bulk suppresses it);
// lk_grid_init does one full refresh at the end.
static void lkg_fill_floor(int x0, int y0, int x1, int y1, int mat) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++)
        if (lk_in(x, y)) { lk_t[lk_idx(x, y)].floor = (unsigned char)mat; }
}
static void lkg_fill_paint(int x0, int y0, int x1, int y1, int type) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++)
        if (lk_in(x, y)) lk_t[lk_idx(x, y)].paint = (unsigned char)type;
}
static void lkg_fill_zone(int x0, int y0, int x1, int y1, int zn) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++)
        if (lk_in(x, y)) lk_t[lk_idx(x, y)].zone = (unsigned char)zn;
}
static void lkg_wall_at(int x, int y, int mat) {
    if (!lk_in(x, y)) return;
    Tile *t = &lk_t[lk_idx(x, y)];
    t->wall = (unsigned char)mat; t->door = DR_NONE; t->obj = OB_NONE; t->obj_ref = 0;
}
static void lkg_wall_h(int x0, int x1, int y, int mat) { for (int x = x0; x <= x1; x++) lkg_wall_at(x, y, mat); }
static void lkg_wall_v(int x, int y0, int y1, int mat) { for (int y = y0; y <= y1; y++) lkg_wall_at(x, y, mat); }
static void lkg_wall_box(int x0, int y0, int x1, int y1, int mat) {
    lkg_wall_h(x0, x1, y0, mat); lkg_wall_h(x0, x1, y1, mat);
    lkg_wall_v(x0, y0, y1, mat); lkg_wall_v(x1, y0, y1, mat);
}
static void lkg_door_at(int x, int y, int kind) {
    if (!lk_in(x, y)) return;
    Tile *t = &lk_t[lk_idx(x, y)];
    t->wall = WL_NONE; t->door = (unsigned char)kind; t->door_open = 0; t->locked = 0;
    t->obj = OB_NONE; t->obj_ref = 0;
}

// The starting plot.  A public road, a fenced compound with a delivery gate, and
// one block that is deliberately HALF a prison: five working cells, a working
// office, a canteen with no serving table, and a bare yard.  A stranger should be
// able to see the hole in it without being told.
void lk_grid_init(int seed) {
    lkg_seed = (unsigned int)seed * 2654435761u + 1u;
    lk.seed  = seed;
    lkg_bulk = 1;

    // ── bare land: dirt, with grass and gravel in organic patches ──
    for (int y = 0; y < LK_MH; y++) for (int x = 0; x < LK_MW; x++) {
        Tile *t = &lk_t[lk_idx(x, y)];
        t->floor = FL_DIRT; t->wall = WL_NONE; t->door = DR_NONE; t->door_open = 0;
        t->locked = 0; t->joins = 0; t->obj = OB_NONE; t->obj_ref = 0; t->obj_dir = 0;
        t->obj_used = 0; t->room = 0; t->paint = RM_NONE; t->zone = ZN_OPEN;
        t->job = JB_NONE; t->job_arg = 0; t->claimed = 0; t->dirty = 0; t->light = 0;
        t->work = 0.0f; lkg_wear[lk_idx(x, y)] = 0.0f;
        t->var = (unsigned char)(lkg_hash(x, y, lkg_seed) & 0xFF);
        float g = lkg_fbm((float)x / 7.0f, (float)y / 7.0f, lkg_seed);
        float v = lkg_fbm((float)x / 4.5f + 31.7f, (float)y / 4.5f + 11.3f, lkg_seed ^ 0x2f7bu);
        if (g > 0.615f)     t->floor = FL_GRASS;    // measured ~25% grass, ~8%
        else if (v > 0.70f) t->floor = FL_GRAVEL;   // gravel, rest bare dirt
    }
    lkg_njob = 0;

    // ── the public road along the north edge, and the delivery spur ──
    lkg_fill_floor(0, 2, LK_MW - 1, 4, FL_ASPHALT);
    lkg_fill_floor(0, 5, LK_MW - 1, 5, FL_GRAVEL);
    lkg_fill_floor(47, 5, 48, 8, FL_ASPHALT);

    // ── the perimeter: a fence, not masonry, so the compound is still OUTDOORS
    // (see §4) — the yard, the rain and the daylight all depend on that. ──
    lkg_wall_box(12, 8, 83, 56, WL_PERIM);
    lkg_door_at(47, 8, DR_GATE);
    lkg_door_at(48, 8, DR_GATE);
    lkg_fill_floor(44, 9, 52, 13, FL_ASPHALT);        // delivery apron inside the gate
    lkg_fill_floor(47, 9, 48, 25, FL_CONCRETE);       // path down to the block
    lkg_fill_floor(45, 23, 48, 25, FL_CONCRETE);      // and west to its door

    // ── the block: outer shell x 24..44, y 18..32 (interior 25..43 / 19..31) ──
    lkg_fill_floor(24, 18, 44, 32, FL_CONCRETE);
    lkg_wall_box(24, 18, 44, 32, WL_BRICK);

    // cells: five 3×3 rooms along the north side, dividers between them, each
    // with a jail door onto the corridor.
    lkg_wall_h(25, 43, 22, WL_BRICK);
    for (int k = 0; k < 4; k++) lkg_wall_v(28 + k * 4, 19, 22, WL_BRICK);
    for (int k = 0; k < 5; k++) {
        int x0 = 25 + k * 4;                           // 25 29 33 37 41
        lkg_fill_paint(x0, 19, x0 + 2, 21, RM_CELL);
        lkg_door_at(x0 + 1, 22, DR_JAIL);
    }
    lkg_fill_zone(24, 18, 44, 22, ZN_SECURE);

    // corridor y 23..25, its south wall at y 26, and the block's own entrance
    lkg_door_at(44, 24, DR_JAIL);
    lkg_wall_h(25, 43, 26, WL_BRICK);
    lkg_wall_v(36, 26, 31, WL_BRICK);

    // canteen (west) — tables and benches but NO serving table.  This is the hole
    // the player is meant to see on frame 1: valid == 0, missing == OB_SERVING.
    lkg_fill_floor(25, 27, 35, 31, FL_TILE);
    lkg_fill_paint(25, 27, 35, 31, RM_CANTEEN);
    lkg_door_at(30, 26, DR_PLAIN);

    // office (east) — desk + chair, so it validates and the player has one room
    // that already works to compare against.
    lkg_fill_floor(37, 27, 43, 31, FL_WOOD);
    lkg_fill_paint(37, 27, 43, 31, RM_OFFICE);
    lkg_fill_zone(37, 27, 43, 31, ZN_STAFF);
    lkg_door_at(40, 26, DR_PLAIN);

    // ── the yard: gravel, painted, no walls.  RM_YARD is the one enclosed == 0
    // room type, so it is valid immediately and prisoners have somewhere to go. ──
    lkg_fill_floor(52, 30, 72, 46, FL_GRAVEL);
    lkg_fill_paint(52, 30, 72, 46, RM_YARD);

    // ── objects ──
    for (int k = 0; k < 5; k++) {
        int x0 = 25 + k * 4;
        lk_place_obj(lk_idx(x0,     19), OB_BED,    0);   // 1×2, occupies y 19..20
        lk_place_obj(lk_idx(x0 + 2, 21), OB_TOILET, 3);
    }
    // Canteen seating: bench row / table row / bench row, so people sit ALONG the
    // long side of a 2-wide table instead of at its ends.  Two groups, either side
    // of a wide central aisle that lines up with the door column (x 30) — nothing
    // solid may stand in a doorway's path.  Row 31 is deliberately left bare: it
    // is where the serving counter goes, and the gap is the hint.
    for (int gx = 0; gx < 2; gx++) {
        int x = 26 + gx * 6;                               // 26 and 32
        lk_place_obj(lk_idx(x,     28), OB_TABLE, 0);       // 2×1, covers x..x+1
        lk_place_obj(lk_idx(x,     27), OB_BENCH, 2);       // facing south, at table
        lk_place_obj(lk_idx(x + 1, 27), OB_BENCH, 2);
        lk_place_obj(lk_idx(x,     29), OB_BENCH, 0);       // facing north
        lk_place_obj(lk_idx(x + 1, 29), OB_BENCH, 0);
    }
    lk_place_obj(lk_idx(39, 28), OB_DESK,    0);           // office
    lk_place_obj(lk_idx(39, 29), OB_CHAIR,   0);
    lk_place_obj(lk_idx(41, 28), OB_CABINET, 0);
    lk_place_obj(lk_idx(27, 24), OB_LIGHT,   0);           // something for night
    lk_place_obj(lk_idx(34, 24), OB_LIGHT,   0);
    lk_place_obj(lk_idx(30, 30), OB_LIGHT,   0);
    lk_place_obj(lk_idx(40, 30), OB_LIGHT,   0);
    lk_place_obj(lk_idx(43, 25), OB_BIN,     0);
    lk_place_obj(lk_idx(54, 34), OB_WEIGHTS, 0);           // yard: a weights corner…
    lk_place_obj(lk_idx(54, 36), OB_WEIGHTS, 0);
    lk_place_obj(lk_idx(54, 38), OB_WEIGHTS, 0);
    lk_place_obj(lk_idx(58, 31), OB_BENCH,   0);           // …and seating along the top
    lk_place_obj(lk_idx(62, 31), OB_BENCH,   0);
    lk_place_obj(lk_idx(66, 31), OB_BENCH,   0);

    // ── one refresh for the whole world ──
    lkg_bulk = 0;
    for (int c = 0; c < LK_N; c++) lkg_join_one(c);
    lkg_out_dirty = 1;
    lk_rooms_rebuild();
    lkg_recount_used();
    lkg_lockprint  = 0;
    lk_dirty_struct = 1;
}

#endif // LOCKUP_GRID_H
