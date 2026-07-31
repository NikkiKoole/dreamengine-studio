// ─────────────────────────────────────────────────────────────────────────────
// lockup/path.h — MODULE: path.  Everything about "how does an actor get there".
//
// OWNS (all statics, all `lkp_`-prefixed — no extern global belongs to this
// module; `lk_dirty_struct` is grid's and is only CLEARED here, per contract):
//   · the A* scratch space + the indexed binary min-heap
//   · the connected-component labels, ONE ARRAY PER ROLE CLASS — which double as
//     the cached passability bitmap the hot loop reads
//   · the FF_* nearest-facility flow fields, one per (class, kind)
//   · the cached per-tile step cost + a per-frame actor-occupancy snapshot
//   · three rolling map signatures, used to notice invalidation nobody flagged
//
// ── THE FOUR ALGORITHMS ─────────────────────────────────────────────────────
// 1. lk_find_path — A* over 4-neighbours (NEVER diagonal: a prison is
//    orthogonal, and a diagonal step would let an actor clip a wall corner),
//    with a real INDEXED binary min-heap.  h is Manhattan, which is admissible
//    because the cheapest step costs 1.  Ties break on smaller h, then on tile
//    index, so a run is bit-deterministic AND open ground doesn't explode into a
//    diamond of equal-f expansions.
//    Complexity O(V log V), V ≤ LK_N = 6144; each tile closes at most once and
//    the heap holds at most one live entry per tile.  Five things make the
//    constant small enough for ~180 actors:
//      · GENERATION STAMPS.  One int per tile compared against a monotonic
//        counter, so a search costs nothing to set up.  dwarffort
//        (tools/carts/dwarffort.c:123-150) clears 6144 entries per call AND
//        linearly rescans the whole open set on every pop — O(N) per step.
//      · PACKED HEAP KEYS.  The heap stores (f<<32)|(h<<16)|tile in one u64, so
//        a sift compares two integers with zero pointer chasing, and no f or h
//        array is needed at all.  (A lazy-deletion heap — duplicate entries, no
//        index — was tried and measured 9% SLOWER: keeping the index and doing a
//        real decrease-key wins, because the heap stays small.)
//      · CACHED PASSABILITY.  A tile is passable for a class exactly when it has
//        a component label, so the inner loop reads one short instead of calling
//        lk_can_pass ~4× per expansion.  Same array, two jobs.
//      · NO div/mod IN THE INNER LOOP.  Neighbours are ±1 / ±LK_MW on the flat
//        index, bounds-checked against x,y computed once per pop.
//      · A BOUNDED, SHARED PER-FRAME EXPANSION BUDGET, so the module's cost is
//        capped no matter how many actors ask at once — D11.
// 2. lk_region / lk_reachable — one BFS flood per role class over passable
//    tiles, O(LK_N), rebuilt only when the map's permission signature moves.
//    lk_reachable is then an O(1) label compare, which is what lets the HUD say
//    "unreachable" the instant you paint it instead of after a failed walk.
// 3. lk_nearest — one MULTI-SOURCE DIJKSTRA per (class, kind), seeded from every
//    qualifying tile at once, so 120 prisoners asking "where is a FREE toilet"
//    costs one field instead of 120 searches.  Each cell stores the distance AND
//    the source tile it was reached from, so a query is O(1) — no downhill walk
//    needed (see notes).  Weighted, not a plain BFS, so a toilet behind three
//    jail doors correctly loses to one a little further down the corridor.
// 4. lk_step_cost — 1 on open floor, 3..6 through a door (doors are real
//    chokepoints and a crowd visibly queues at them), plus a bounded penalty for
//    tiles other actors are standing on so traffic self-organises.
//
// Memory: ~600 KB of flat static arrays (≈490 KB of that is the 20 flow fields).
// Deliberate: flat int/short arrays beat anything clever at this size, the hot
// working set is ~120 KB so it lives in L2, and there is not one allocation.
//
// MEASURED (96×64 map, 900 scattered walls, 180 actors, -O2, this Mac):
//   · realistic load — 180 actors at ~4.5 repaths/frame, fields rebuilding:
//       0.08 ms/frame, every path fully optimal for the weighted heuristic
//   · absolute worst case — all 180 actors demanding a fresh cross-map path in
//       the SAME frame (what a regime-slot boundary looks like):
//       ~1.9 ms/frame, degrading path QUALITY rather than the frame time
//   · one flow-field rebuild 0.29 ms; at most LKP_BUILD_PER_FRAME = 2 per frame,
//       and only while something is stale
//   · lk_path_update's fixed part (signatures + occupancy + labels) 0.006 ms
//
// ── IMPLEMENTATION NOTES ────────────────────────────────────────────────────
// D1. ROLE CLASSES.  Labels and fields are kept per CLASS, not per role:
//     class 0 = RL_PRISONER, class 1 = every staff role (represented by
//     RL_GUARD when calling lk_can_pass).  This assumes the four staff roles
//     share one permission set.  If grid ever makes, say, a cook unable to open
//     a staff door that a guard can, this module needs a third class — one line
//     (LKP_NCLASS + LKP_CLASS_ROLE) and everything else follows.
// D2. SOLID GOALS.  A bed / toilet / bench is `solid` in LK_OBJ, so you can
//     never stand ON the thing you want to use.  lk_find_path therefore accepts
//     a solid `to`: it runs a multi-goal A* over the ≤4 passable orthogonal
//     neighbours of `to` and returns the path to the cheapest one.  out[len-1]
//     is the tile actually REACHED, which is adjacent to `to`.  Callers do NOT
//     need to hunt for a standing tile themselves.
// D3. RETURN 0 IS AMBIGUOUS, by contract ("0 = no path").  It also covers
//     "you are already there / already adjacent".  Distinguish with
//     lk_reachable(from, to, role): true + length 0 == arrived.
// D4. lk_nearest returns the FACILITY tile (the object / yard / exit tile), not
//     the tile you stand on to use it — that is what a caller needs for
//     Tile.obj_used and lk_room_release.  Feed it straight back into
//     lk_find_path, which handles the standing tile (D2).
// D5. FIELD FRESHNESS.  A field is rebuilt when it is marked stale, at most
//     LKP_BUILD_PER_FRAME per lk_path_update, round-robin, so frame cost is
//     bounded and even.  Stale is set by: lk_dirty_struct; a change in the
//     rolling PERMISSION signature (wall/door/locked/zone/obj); a change in the
//     rolling SLOT signature (obj_used — this is the occupancy invalidation the
//     brief asked for, and it is exact, not a guess); a change in the ROOM
//     signature (tile→room ids plus the room table's valid/type, which is what
//     FF_YARD and FF_SOLITARY are made of); and on demand, when a query notices
//     its own answer has gone bad.  The three signatures hash a QUARTER of the
//     map per frame (~1500 tiles, a few µs), so worst-case detection latency is
//     4 frames and a full drain of 20 stale fields is 10 frames.  Honest
//     staleness of a few frames; never a field that stops rebuilding.
// D6. STALE ANSWER FALLBACK.  If a query's cached target no longer qualifies
//     (someone took that toilet two frames ago), lk_nearest marks the field
//     stale AND does a bounded local box scan (radius LKP_LOCAL_R) for another
//     qualifying, label-reachable source rather than lying or giving up.  If
//     nothing is near it returns -1, and the field is right again next frame.
// D7. ROOM VALIDITY is only consulted for the two ROOM kinds (FF_YARD,
//     FF_SOLITARY, which require `valid`).  The object kinds index every free,
//     reachable, role-permitted serving object regardless of whether its room is
//     valid — this module is a spatial index, not the rules engine.  Gating "a
//     bed in an unenclosed cell does not satisfy Sleep" is the actors module's
//     job (docs/design/lockup.md §1, conditions 1+2).
// D8. OB_BUNK has slots = 2 but a tile has one obj_used flag, so a bunk is
//     treated as capacity 1 by the FF_BED field.  A fix needs a second flag in
//     Tile, i.e. a contract change — flagged, not made.
// D9. CACHED PASSABILITY / COST ARE REFRESHED WITH THE LABELS, so both are
//     authoritative as of the last relabel — i.e. the same frame lk_dirty_struct
//     was set, or within 4 frames for a change nobody flagged.  That defensive
//     signature exists because a module mutating a tile without setting
//     lk_dirty_struct would otherwise leave lk_reachable silently LYING forever.
//     A LOCKDOWN (which flips Tile.locked and genuinely re-routes the prison) is
//     caught whether grid flags it or not.
// D10. NEEDS FROM OTHERS:
//     · lk_can_pass(c, role) must be pure and side-effect free, and must NOT
//       depend on Tile.door_open — that is animation state.  A closed but
//       unlocked door is passable; it opens as the actor arrives.  (If passability
//       flickered with an animation, reachability and every flow field would
//       flicker with it.)
//     · Tile.obj must be set on the ORIGIN tile only, with obj_ref = 1 on the
//       tiles a multi-tile object covers.
//     · lk_t[c].room must index lk_room[], with 0 meaning "none".
//     · grid must set lk_dirty_struct on any structural change (contract);
//       the signatures are a backstop, not a substitute.
//     · Call lk_path_update BEFORE lk_actors_update in the frame, so the
//       occupancy snapshot matches the positions actors then move from.
//       Reading lk_a[] here is read-only.
// D11. TWO COST GOVERNORS, because a prison hits ONE pathological frame per
//     in-game hour by design: at a regime-slot boundary every prisoner re-picks a
//     goal at once.  Unbounded, that frame measured 4.8 ms — 9.1 ms with an
//     unweighted heuristic — i.e. it hitches.
//       · A SHARED FRAME BUDGET.  lk_path_update refills LKP_FRAME_EXPAND
//         expansions; each lk_find_path takes what it needs from the pot, and
//         every call is guaranteed at least LKP_MINEXPAND even when the pot is
//         empty, so nobody ever gets nothing.  When a search runs out it returns
//         the path to the CLOSEST tile it did reach (smallest h) — a real,
//         shorter path in the right direction, not a failure — and the actor
//         repaths next frame with a fresh budget.  So the burst is amortised
//         across frames instead of dropped.  Hard bound: LKP_FRAME_EXPAND +
//         nact × LKP_MINEXPAND expansions per frame, whatever callers do.
//       · A WEIGHTED HEURISTIC.  f = g + 1.25·h.  Measured on the bench above:
//         1.6× fewer expansions for paths 2.4% longer (81 tiles → 83).  Bounded
//         suboptimality, still fully deterministic — no dice, and the 2.4% is
//         invisible next to "guard response time is a path length".  Set
//         LKP_HEUR_W=4 for provably optimal A* if a spec() assertion needs it.
//     Considered and rejected: a bucket-dial (radix) queue, which would be
//     faster still but is only valid for a CONSISTENT heuristic, i.e. it and the
//     weighting are mutually exclusive; and hierarchical/portal pathing, which is
//     the real answer at 10× this map size and not worth its bug surface at 96×64.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef LOCKUP_PATH_H
#define LOCKUP_PATH_H

#include "studio.h"
#include "lockup/model.h"

// ── tuning ──────────────────────────────────────────────────────────────────
#define LKP_NCLASS            2      // 0 = prisoner, 1 = staff (see D1)
#define LKP_INF           65535      // "unreachable" in a flow field
#define LKP_FDMAX         65000      // distance clamp, keeps INF distinguishable
#define LKP_BUILD_PER_FRAME   2      // flow fields rebuilt per lk_path_update
#define LKP_SIGQ              4      // map-signature quarters (1 hashed / frame)
#define LKP_LOCAL_R          14      // stale-answer fallback box radius, tiles
#define LKP_OCC_STEP          2      // path cost per actor standing on a tile
#define LKP_OCC_MAX           6      // …capped, so a crowd is never a wall
#define LKP_MAXLAB        32000      // component-id saturation guard
// Search cost governors — see D11.  All -D overridable so a spec() run can pin
// exact A* (LKP_HEUR_W=4) and an unbounded budget if it wants provable optima.
#ifndef LKP_HEUR_W
#define LKP_HEUR_W            5      // heuristic weight in QUARTERS (4 == exact A*)
#endif
#ifndef LKP_MAXEXPAND
#define LKP_MAXEXPAND      6144      // ceiling on tiles ONE search may close
#endif
#ifndef LKP_MINEXPAND
#define LKP_MINEXPAND        48      // …floor every call is guaranteed regardless
#endif
#ifndef LKP_FRAME_EXPAND
#define LKP_FRAME_EXPAND  20000      // shared pathing budget per frame
#endif

// Entering a door costs this instead of 1.  A jail door is dearest because it is
// the thing a prison is made of: paths prefer open corridors, and a block with
// one jail door genuinely bottlenecks.
static const unsigned char LKP_DOOR_COST[DR_COUNT] = { 1, 3, 5, 4, 6 };

// Which FF_* kinds depend on Tile.obj_used (a FREE facility, not just any).
#define LKP_OCC_KINDS ( (1u << FF_TOILET) | (1u << FF_BED)   | (1u << FF_SHOWER) | \
                        (1u << FF_BENCH)  | (1u << FF_PHONE) | (1u << FF_TV)     | \
                        (1u << FF_SERVING) )
// Which FF_* kinds are derived from rooms (and so need lk_room[].valid).
#define LKP_ROOM_KINDS ( (1u << FF_YARD) | (1u << FF_SOLITARY) )

static const signed char LKP_DX[4] = {  0, 1, 0, -1 };        // N E S W — a fixed
static const signed char LKP_DY[4] = { -1, 0, 1,  0 };        // order == stable ties
static const short       LKP_DI[4] = { -LK_MW, 1, LK_MW, -1 };// flat-index deltas

// ── state ───────────────────────────────────────────────────────────────────
// One monotonic generation stamps the whole scratch space, so a search never
// clears anything: lkp_state[c] == 2*gen means "open", 2*gen+1 means "closed",
// anything smaller is a leftover from an older search and reads as unvisited.
static int   lkp_gen;
static int   lkp_state[LK_N];
static int   lkp_g[LK_N];            // cost from start (A*) / distance (field)
static short lkp_from[LK_N];         // parent tile (A*) / SOURCE tile (field)
static short lkp_hpos[LK_N];         // slot in lkp_heap, -1 = not queued

// The heap stores its own sort key, so a compare never touches another array:
//   bits 63..32 = f   ·   31..16 = h (tie-break: pull toward the goal)
//   bits 15..0  = tile index (final tie-break: fully deterministic order)
static unsigned long long lkp_heap[LK_N];
static int                lkp_hn;
#define LKP_KEY(f, h, c)  ( ((unsigned long long)(unsigned int)(f) << 32)      \
                          | ((unsigned long long)((unsigned)(h) & 0xffffu) << 16) \
                          | (unsigned long long)((unsigned)(c) & 0xffffu) )
#define LKP_KTILE(k)      ((int)((k) & 0xffffu))

static int   lkp_pops;               // tiles closed by the last search (debug)
static int   lkp_budget;             // expansions left in this frame's share
static int   lkp_spent;              // …and how many went (debug readout)
static short lkp_tmp[LK_N];          // path reconstruction buffer
static int   lkp_q[LK_N];            // BFS FIFO for component labelling

// Connected components per role class.  -1 = impassable for that class, which is
// also how the hot loop tests passability (D9).
static short lkp_lab[LKP_NCLASS][LK_N];
static int   lkp_nlab[LKP_NCLASS];
static int   lkp_need_label;

// Cached step cost: the door term (structure only), refreshed with the labels.
static unsigned char lkp_scost[LK_N];

// Flow fields: distance to the nearest source, and WHICH source that was.
static unsigned short lkp_ffd[LKP_NCLASS][FF_COUNT][LK_N];
static short          lkp_ffs[LKP_NCLASS][FF_COUNT][LK_N];
static unsigned char  lkp_ffstale[LKP_NCLASS][FF_COUNT];
static int            lkp_rr;        // round-robin cursor over class*FF_COUNT

// Actor occupancy, stored as the ALREADY-CAPPED path penalty (0, 2, 4, 6).
// Rebuilt every lk_path_update and cleared by list, never by memset.
static unsigned char lkp_occ[LK_N];
static int           lkp_occ_list[LK_MAXACT];
static int           lkp_nocc;

// Rolling map signatures — see D5/D9.
static unsigned int lkp_sig_perm[LKP_SIGQ];
static unsigned int lkp_sig_slot[LKP_SIGQ];
static unsigned int lkp_sig_room[LKP_SIGQ];
static unsigned int lkp_sig_rval;    // the lk_room[] valid/type digest
static int          lkp_sigq;

// ── little helpers ──────────────────────────────────────────────────────────
static inline int lkp_iabs(int v) { return v < 0 ? -v : v; }
static inline int lkp_cls(int role) { return role == RL_PRISONER ? 0 : 1; }
static const unsigned char LKP_CLASS_ROLE[LKP_NCLASS] = { RL_PRISONER, RL_GUARD };

// A generation is one int; wrap safely rather than pretend 55 hours can't pass.
static int lkp_gen_next(void) {
    if (lkp_gen >= 0x3ffffff0) {
        for (int i = 0; i < LK_N; i++) lkp_state[i] = 0;
        lkp_gen = 0;
    }
    return ++lkp_gen * 2;                       // == the "open" stamp for this run
}

// ── the indexed binary min-heap (keys packed, holes shifted, no swaps) ──────
static void lkp_hup(int i) {                                     // O(log n)
    unsigned long long k = lkp_heap[i];
    while (i > 0) {
        int p = (i - 1) >> 1;
        if (lkp_heap[p] <= k) break;
        lkp_heap[i] = lkp_heap[p];
        lkp_hpos[LKP_KTILE(lkp_heap[i])] = (short)i;
        i = p;
    }
    lkp_heap[i] = k;
    lkp_hpos[LKP_KTILE(k)] = (short)i;
}
static void lkp_hdown(int i) {                                   // O(log n)
    unsigned long long k = lkp_heap[i];
    for (;;) {
        int l = 2 * i + 1;
        if (l >= lkp_hn) break;
        int r = l + 1;
        int m = (r < lkp_hn && lkp_heap[r] < lkp_heap[l]) ? r : l;
        if (lkp_heap[m] >= k) break;
        lkp_heap[i] = lkp_heap[m];
        lkp_hpos[LKP_KTILE(lkp_heap[i])] = (short)i;
        i = m;
    }
    lkp_heap[i] = k;
    lkp_hpos[LKP_KTILE(k)] = (short)i;
}
static inline void lkp_hpush(unsigned long long k) {             // O(log n)
    if (lkp_hn >= LK_N) return;                 // cannot happen: 1 live slot/tile
    lkp_heap[lkp_hn] = k;
    lkp_hpos[LKP_KTILE(k)] = (short)lkp_hn;
    lkp_hn++;
    lkp_hup(lkp_hn - 1);
}
static inline unsigned long long lkp_hpop(void) {                // O(log n)
    unsigned long long top = lkp_heap[0];
    lkp_hpos[LKP_KTILE(top)] = -1;
    lkp_hn--;
    if (lkp_hn > 0) {
        lkp_heap[0] = lkp_heap[lkp_hn];
        lkp_hpos[LKP_KTILE(lkp_heap[0])] = 0;
        lkp_hdown(0);
    }
    return top;
}
// the key of a queued tile got smaller (or it needs re-queueing)
static inline void lkp_hlower(int c, unsigned long long k) {
    int p = lkp_hpos[c];
    if (p >= 0) { lkp_heap[p] = k; lkp_hup(p); } else lkp_hpush(k);
}

// ── step cost ───────────────────────────────────────────────────────────────
// O(1).  lkp_scost is the structural term (doors); lkp_occ is this frame's
// crowd penalty, already capped.  A flow field uses lkp_scost ALONE — a field
// must depend only on structure or it would churn as actors shuffle about.
int lk_step_cost(int c, int role) {
    (void)role;
    if (c < 0 || c >= LK_N) return 1;
    int base = lkp_scost[c];
    return (base ? base : 1) + lkp_occ[c];
}

// ── component labelling ─────────────────────────────────────────────────────
// O(LK_N) per class: one BFS flood per component over lk_can_pass tiles.  This
// is the ONLY place lk_can_pass is consulted in bulk; everything else reads the
// label array it produces (D9).
static void lkp_label_class(int cls) {
    int role = LKP_CLASS_ROLE[cls];
    short *lab = lkp_lab[cls];
    for (int i = 0; i < LK_N; i++) lab[i] = -1;
    int n = 0;
    for (int s = 0; s < LK_N; s++) {
        if (lab[s] >= 0 || !lk_can_pass(s, role)) continue;
        int id = n < LKP_MAXLAB ? n : LKP_MAXLAB;      // saturate, never wrap short
        n++;
        int qh = 0, qt = 0;
        lkp_q[qt++] = s; lab[s] = (short)id;
        while (qh < qt) {
            int c = lkp_q[qh++];
            int y = c / LK_MW, x = c - y * LK_MW;
            for (int d = 0; d < 4; d++) {
                int nx = x + LKP_DX[d], ny = y + LKP_DY[d];
                if ((unsigned)nx >= (unsigned)LK_MW || (unsigned)ny >= (unsigned)LK_MH) continue;
                int nc = c + LKP_DI[d];
                if (lab[nc] >= 0 || !lk_can_pass(nc, role)) continue;
                lab[nc] = (short)id;
                lkp_q[qt++] = nc;
            }
        }
    }
    lkp_nlab[cls] = n;
}
// O(LK_N).  Mirrors the door term of the cost so the hot loop reads one byte.
static void lkp_scost_refresh(void) {
    for (int c = 0; c < LK_N; c++) {
        int dr = lk_t[c].door;
        lkp_scost[c] = (dr > 0 && dr < DR_COUNT) ? LKP_DOOR_COST[dr] : 1;
    }
}
static void lkp_relabel(void) {
    for (int cls = 0; cls < LKP_NCLASS; cls++) lkp_label_class(cls);
    lkp_scost_refresh();
    lkp_need_label = 0;
}

int lk_region(int c, int role) {                    // O(1)
    if (c < 0 || c >= LK_N) return -1;
    return lkp_lab[lkp_cls(role)][c];              // -1 when impassable
}

// The labels touching tile `c`: its own if it is passable, plus its 4 orthogonal
// neighbours'.  That fallback is what makes lk_reachable answer "can I get to
// that BED" — a bed is solid, so it has no label of its own, and lk_region stays
// strict and returns -1 for it, per contract.
static int lkp_labels_at(int c, int cls, int *outv) {
    const short *lab = lkp_lab[cls];
    int n = 0;
    if (lab[c] >= 0) outv[n++] = lab[c];
    int y = c / LK_MW, x = c - y * LK_MW;
    for (int d = 0; d < 4; d++) {
        int nx = x + LKP_DX[d], ny = y + LKP_DY[d];
        if ((unsigned)nx >= (unsigned)LK_MW || (unsigned)ny >= (unsigned)LK_MH) continue;
        int l = lab[c + LKP_DI[d]];
        if (l < 0) continue;
        int dup = 0;
        for (int i = 0; i < n; i++) if (outv[i] == l) { dup = 1; break; }
        if (!dup) outv[n++] = l;
    }
    return n;                                      // 0..5
}

bool lk_reachable(int from, int to, int role) {     // O(1) — 25 compares worst
    if (from < 0 || from >= LK_N || to < 0 || to >= LK_N) return false;
    if (from == to) return true;
    int cls = lkp_cls(role);
    int la[5], lb[5];
    int na = lkp_labels_at(from, cls, la);
    int nb = lkp_labels_at(to, cls, lb);
    for (int i = 0; i < na; i++)
        for (int j = 0; j < nb; j++)
            if (la[i] == lb[j]) return true;
    return false;
}

// ── A* ──────────────────────────────────────────────────────────────────────
// O(V log V), V ≤ LK_N.  Writes travel order EXCLUDING `from` into out, clamped
// to cap, and returns the length (0 = no path — see D3).
int lk_find_path(int from, int to, int role, short *out, int cap) {
    if (!out || cap <= 0) return 0;
    if (from < 0 || from >= LK_N || to < 0 || to >= LK_N) return 0;
    if (from == to) return 0;

    int cls = lkp_cls(role);
    const short *lab = lkp_lab[cls];
    int gy = to / LK_MW, gx = to - gy * LK_MW;

    // Build the goal set.  A passable goal is itself; a solid one (a bed, a
    // toilet, a bench) becomes its passable orthogonal neighbours — D2.
    int goal[4], ngoal = 0, adj = 0;
    if (lab[to] >= 0) {
        goal[ngoal++] = to;
    } else {
        adj = 1;
        for (int d = 0; d < 4; d++) {
            int nx = gx + LKP_DX[d], ny = gy + LKP_DY[d];
            if ((unsigned)nx >= (unsigned)LK_MW || (unsigned)ny >= (unsigned)LK_MH) continue;
            int nc = to + LKP_DI[d];
            if (nc == from) return 0;              // already standing next to it
            if (lab[nc] >= 0) goal[ngoal++] = nc;
        }
        if (!ngoal) return 0;                      // walled in — nothing to do
    }

    // O(1) reject, so an impossible goal never costs a full-map search.  Skipped
    // while a relabel is pending: the labels have not caught up, and refusing a
    // valid path is worse than one wasted A*.
    if (!lk_dirty_struct && !lkp_need_label) {
        int ok = 0;
        for (int i = 0; i < ngoal; i++)
            if (lk_reachable(from, goal[i], role)) { ok = 1; break; }
        if (!ok) return 0;
    }

    // How many tiles this search may close.  Every call gets at least
    // LKP_MINEXPAND so an actor always makes SOME progress; beyond that it draws
    // on what is left of the frame's shared budget.  That is what bounds the
    // module no matter how hard the actors module leans on it (D11).
    int quota = lkp_budget > 0 ? lkp_budget : 0;
    if (quota < LKP_MINEXPAND) quota = LKP_MINEXPAND;
    if (quota > LKP_MAXEXPAND) quota = LKP_MAXEXPAND;

    int base = lkp_gen_next(), closed = base + 1;
    lkp_hn = 0;
    // `from` is seeded even if it is impassable — an actor sealed inside a
    // freshly built wall must still be able to walk out.
    int fy = from / LK_MW, fx = from - fy * LK_MW;
    int h0 = lkp_iabs(fx - gx) + lkp_iabs(fy - gy);
    if (adj && h0 > 0) h0--;
    lkp_state[from] = base;
    lkp_g[from] = 0;
    lkp_from[from] = -1;
    lkp_hpush(LKP_KEY((h0 * LKP_HEUR_W) >> 2, h0, from));

    int reached = -1, pops = 0;
    int best = -1, bestk = h0;                     // closest tile reached, for the cap
    while (lkp_hn > 0) {
        int c = LKP_KTILE(lkp_hpop());
        if (lkp_state[c] >= closed) continue;      // stale heap entry
        lkp_state[c] = closed;
        if (ngoal == 1) { if (c == goal[0]) { reached = c; break; } }
        else { int hit = 0;
               for (int i = 0; i < ngoal; i++) if (c == goal[i]) { hit = 1; break; }
               if (hit) { reached = c; break; } }
        if (++pops >= quota) break;                // out of budget — use `best`

        int y = c / LK_MW, x = c - y * LK_MW;
        int gc = lkp_g[c];
        for (int d = 0; d < 4; d++) {              // 4-neighbour ONLY, no diagonals
            int nx = x + LKP_DX[d], ny = y + LKP_DY[d];
            if ((unsigned)nx >= (unsigned)LK_MW || (unsigned)ny >= (unsigned)LK_MH) continue;
            int nc = c + LKP_DI[d];
            if (lab[nc] < 0) continue;             // impassable (cached — D9)
            int st = lkp_state[nc];
            if (st >= closed) continue;
            int tg = gc + lkp_scost[nc] + lkp_occ[nc];
            if (st == base && tg >= lkp_g[nc]) continue;
            int h = lkp_iabs(nx - gx) + lkp_iabs(ny - gy);
            if (adj && h > 0) h--;
            if (h < bestk) { bestk = h; best = nc; }
            lkp_g[nc] = tg;
            lkp_from[nc] = (short)c;
            unsigned long long k = LKP_KEY(tg + ((h * LKP_HEUR_W) >> 2), h, nc);
            if (st == base) lkp_hlower(nc, k);
            else { lkp_state[nc] = base; lkp_hpush(k); }
        }
    }
    lkp_pops = pops;
    lkp_budget -= pops;
    // Out of budget: walk toward the closest tile the search DID reach rather
    // than stand still.  The actor makes real progress and repaths from there.
    if (reached < 0) reached = best;
    if (reached < 0) return 0;

    int n = 0;
    for (int c = reached; c >= 0 && c != from; c = lkp_from[c]) {
        if (n >= LK_N) break;                      // impossible; cheap guard
        lkp_tmp[n++] = (short)c;
    }
    // Reverse into travel order, keeping the FIRST `cap` steps — a truncated
    // path still walks the right way, and the actor repaths when it runs out.
    int len = 0;
    for (int i = n - 1; i >= 0 && len < cap; i--) out[len++] = lkp_tmp[i];
    return len;
}

// ── flow-field sources ──────────────────────────────────────────────────────
static bool lkp_room_is(int c, int type) {
    int rid = lk_t[c].room;
    if (rid <= 0 || rid >= LK_MAXROOM || rid >= lk_nroom) return false;
    return lk_room[rid].type == type && lk_room[rid].valid;
}
// The map edge is where the bus arrives and where an escape ends: any border
// tile without a wall, plus every gate anywhere.
static bool lkp_is_exit(int c) {
    if (lk_t[c].door == DR_GATE) return true;
    int y = c / LK_MW, x = c - y * LK_MW;
    if (x != 0 && y != 0 && x != LK_MW - 1 && y != LK_MH - 1) return false;
    return lk_t[c].wall == WL_NONE;
}
// Does tile `c` qualify as a source for this field?  O(1).  Also used to VERIFY
// a cached answer, which is how a stale one gets noticed (D6).
static bool lkp_is_src(int c, int kind, int cls) {
    const Tile *t = &lk_t[c];
    switch (kind) {
    case FF_TOILET: case FF_BED: case FF_SHOWER: case FF_BENCH:
    case FF_PHONE:  case FF_TV:  case FF_SERVING: {
        int ob = t->obj;
        if (!ob || ob >= OB_COUNT || t->obj_ref) return false;
        if (t->obj_used) return false;                       // a FREE one
        if (LK_OBJ[ob].slots == 0) return false;             // no capacity to give
        if (cls == 0 && LK_OBJ[ob].staff_only) return false; // prisoners never target it
        switch (kind) {
        case FF_TOILET:  return LK_OBJ[ob].serves == ND_BLADDER;
        case FF_BED:     return LK_OBJ[ob].serves == ND_SLEEP;
        case FF_SHOWER:  return ob == OB_SHOWERHEAD;         // not the kitchen sink
        case FF_BENCH:   return LK_OBJ[ob].serves == ND_COMFORT;
        case FF_PHONE:   return LK_OBJ[ob].serves == ND_FAMILY;
        case FF_TV:      return LK_OBJ[ob].serves == ND_REC; // tv/pool/weights/books
        case FF_SERVING: return LK_OBJ[ob].serves == ND_FOOD;
        default:         return false;
        }
    }
    case FF_YARD:     return lkp_room_is(c, RM_YARD);
    case FF_SOLITARY: return lkp_room_is(c, RM_SOLITARY);
    case FF_EXIT:     return lkp_is_exit(c);
    default:          return false;
    }
}

// One multi-source Dijkstra: O(LK_N log LK_N) ≈ 6k pops, ~0.35 ms measured, and
// lk_path_update runs at most LKP_BUILD_PER_FRAME of them.  lkp_g carries the
// distance and lkp_from carries the SOURCE tile (a Dijkstra needs no parent
// pointers), so there are no extra arrays and the query is O(1).
static void lkp_field_build(int cls, int kind) {
    unsigned short *fd = lkp_ffd[cls][kind];
    short          *fs = lkp_ffs[cls][kind];
    const short    *lab = lkp_lab[cls];
    int base = lkp_gen_next(), closed = base + 1;
    lkp_hn = 0;

    // Seed every qualifying tile at distance 0.  A solid facility seeds the
    // passable tiles AROUND it instead — that is where you stand to use it.
    // First source wins by scan order, which keeps it deterministic.
    for (int c = 0; c < LK_N; c++) {
        if (!lkp_is_src(c, kind, cls)) continue;
        if (lab[c] >= 0) {
            if (lkp_state[c] < base) {
                lkp_state[c] = base; lkp_g[c] = 0; lkp_from[c] = (short)c;
                lkp_hpush(LKP_KEY(0, 0, c));
            }
        } else {
            int y = c / LK_MW, x = c - y * LK_MW;
            for (int d = 0; d < 4; d++) {
                int nx = x + LKP_DX[d], ny = y + LKP_DY[d];
                if ((unsigned)nx >= (unsigned)LK_MW || (unsigned)ny >= (unsigned)LK_MH) continue;
                int nc = c + LKP_DI[d];
                if (lab[nc] < 0 || lkp_state[nc] >= base) continue;
                lkp_state[nc] = base; lkp_g[nc] = 0; lkp_from[nc] = (short)c;
                lkp_hpush(LKP_KEY(0, 0, nc));
            }
        }
    }

    while (lkp_hn > 0) {
        int c = LKP_KTILE(lkp_hpop());
        if (lkp_state[c] >= closed) continue;
        lkp_state[c] = closed;
        int y = c / LK_MW, x = c - y * LK_MW;
        int dc = lkp_g[c];
        short src = lkp_from[c];
        for (int d = 0; d < 4; d++) {
            int nx = x + LKP_DX[d], ny = y + LKP_DY[d];
            if ((unsigned)nx >= (unsigned)LK_MW || (unsigned)ny >= (unsigned)LK_MH) continue;
            int nc = c + LKP_DI[d];
            if (lab[nc] < 0) continue;
            int st = lkp_state[nc];
            if (st >= closed) continue;
            int nd = dc + lkp_scost[nc];           // structure only — see lk_step_cost
            if (st == base) {
                if (nd >= lkp_g[nc]) continue;
                lkp_g[nc] = nd; lkp_from[nc] = src;
                lkp_hlower(nc, LKP_KEY(nd, 0, nc));
            } else {
                lkp_state[nc] = base;
                lkp_g[nc] = nd; lkp_from[nc] = src;
                lkp_hpush(LKP_KEY(nd, 0, nc));
            }
        }
    }

    for (int c = 0; c < LK_N; c++) {
        if (lkp_state[c] >= base) {
            int v = lkp_g[c];
            fd[c] = (unsigned short)(v > LKP_FDMAX ? LKP_FDMAX : v);
            fs[c] = lkp_from[c];
        } else {
            fd[c] = LKP_INF; fs[c] = -1;
        }
    }
    lkp_ffstale[cls][kind] = 0;
}

// Bounded fallback for a cached answer that went bad between rebuilds (D6).
// O((2R+1)^2) ≈ 841 tile tests, and only on a stale hit.
static int lkp_local(int from, int kind, int cls) {
    int fy = from / LK_MW, fx = from - fy * LK_MW;
    int mine[5];
    int nmine = lkp_labels_at(from, cls, mine);
    if (!nmine) return -1;
    int best = -1, bestd = 1 << 30;
    for (int dy = -LKP_LOCAL_R; dy <= LKP_LOCAL_R; dy++) {
        int y = fy + dy;
        if ((unsigned)y >= (unsigned)LK_MH) continue;
        for (int dx = -LKP_LOCAL_R; dx <= LKP_LOCAL_R; dx++) {
            int x = fx + dx;
            if ((unsigned)x >= (unsigned)LK_MW) continue;
            int d = lkp_iabs(dx) + lkp_iabs(dy);
            if (d >= bestd) continue;              // ties keep the lower index
            int c = y * LK_MW + x;
            if (!lkp_is_src(c, kind, cls)) continue;
            int lc[5];
            int nl = lkp_labels_at(c, cls, lc), ok = 0;
            for (int i = 0; i < nmine && !ok; i++)
                for (int j = 0; j < nl; j++) if (mine[i] == lc[j]) { ok = 1; break; }
            if (!ok) continue;
            best = c; bestd = d;
        }
    }
    return best;
}

// O(1) in the common case: read the field, then verify the answer still
// qualifies.  Returns the FACILITY tile (D4), or -1 for "no reachable free one".
int lk_nearest(int from, int kind, int role) {
    if (kind < 0 || kind >= FF_COUNT) return -1;
    if (from < 0 || from >= LK_N) return -1;
    int cls = lkp_cls(role);
    const short *lab = lkp_lab[cls];

    int q = from;
    if (lab[from] < 0) {                           // standing in a wall / on an object
        q = -1;
        int y = from / LK_MW, x = from - y * LK_MW;
        for (int d = 0; d < 4; d++) {
            int nx = x + LKP_DX[d], ny = y + LKP_DY[d];
            if ((unsigned)nx >= (unsigned)LK_MW || (unsigned)ny >= (unsigned)LK_MH) continue;
            if (lab[from + LKP_DI[d]] >= 0) { q = from + LKP_DI[d]; break; }
        }
        if (q < 0) return -1;
    }
    if (lkp_ffd[cls][kind][q] >= LKP_INF) return -1;
    int tgt = lkp_ffs[cls][kind][q];
    if (tgt < 0 || tgt >= LK_N) return -1;
    if (!lkp_is_src(tgt, kind, cls)) {             // gone stale under us
        lkp_ffstale[cls][kind] = 1;                // priority rebuild next frame
        return lkp_local(from, kind, cls);
    }
    return tgt;
}

// ── invalidation bookkeeping ────────────────────────────────────────────────
static void lkp_stale_mask(unsigned int kindmask) {
    for (int cls = 0; cls < LKP_NCLASS; cls++)
        for (int k = 0; k < FF_COUNT; k++)
            if ((kindmask >> k) & 1u) lkp_ffstale[cls][k] = 1;
}
static void lkp_stale_all(void) {
    for (int cls = 0; cls < LKP_NCLASS; cls++)
        for (int k = 0; k < FF_COUNT; k++) lkp_ffstale[cls][k] = 1;
}

// FNV-1a over one quarter of the map, three digests in one pass (~1500 tiles):
//   perm — anything that changes PASSABILITY (→ relabel + every field)
//   slot — Tile.obj_used              (→ the occupancy-sensitive fields)
//   room — tile→room ids              (→ the room-derived fields)
static void lkp_sig_quarter(int q, unsigned int *perm, unsigned int *slot,
                            unsigned int *room) {
    int span = LK_N / LKP_SIGQ;
    int lo = span * q, hi = (q == LKP_SIGQ - 1) ? LK_N : lo + span;
    unsigned int hp = 2166136261u, hs = 2166136261u, hr = 2166136261u;
    for (int c = lo; c < hi; c++) {
        const Tile *t = &lk_t[c];
        unsigned int v = (unsigned int)t->wall
                       | ((unsigned int)t->door   << 3)
                       | ((unsigned int)t->locked << 6)
                       | ((unsigned int)t->zone   << 7)
                       | ((unsigned int)t->obj    << 9);
        hp = (hp ^ v) * 16777619u;
        hs = (hs ^ (unsigned int)t->obj_used) * 16777619u;
        hr = (hr ^ (unsigned int)t->room) * 16777619u;
    }
    *perm = hp; *slot = hs; *room = hr;
}
// Room validity is not per tile, so digest the table too (≤ LK_MAXROOM, trivial).
static unsigned int lkp_sig_rooms(void) {
    unsigned int h = 2166136261u;
    int n = lk_nroom < LK_MAXROOM ? lk_nroom : LK_MAXROOM;
    if (n < 0) n = 0;
    h = (h ^ (unsigned int)n) * 16777619u;
    for (int i = 0; i < n; i++) {
        unsigned int v = (unsigned int)lk_room[i].type
                       | ((unsigned int)lk_room[i].valid << 5);
        h = (h ^ v) * 16777619u;
    }
    return h;
}

// Occupancy snapshot: O(lk_nact).  Cleared by LIST — 180 writes instead of 6144
// every frame.  Stores the capped penalty directly, so the A* inner loop adds
// one byte with no branch.
static void lkp_occ_snapshot(void) {
    for (int i = 0; i < lkp_nocc; i++) lkp_occ[lkp_occ_list[i]] = 0;
    lkp_nocc = 0;
    int n = lk_nact < LK_MAXACT ? lk_nact : LK_MAXACT;
    for (int i = 0; i < n; i++) {
        if (!lk_a[i].alive) continue;
        int c = lk_cell_at(lk_a[i].x, lk_a[i].y);
        if (c < 0 || c >= LK_N) continue;
        if (lkp_occ[c] == 0) {
            if (lkp_nocc >= LK_MAXACT) continue;
            lkp_occ_list[lkp_nocc++] = c;
        }
        if (lkp_occ[c] < LKP_OCC_MAX) lkp_occ[c] += LKP_OCC_STEP;
    }
}

// ── the per-frame tick ──────────────────────────────────────────────────────
// O(lk_nact + LK_N/4) fixed, plus at most LKP_BUILD_PER_FRAME field builds.
// THE ONLY PLACE lk_dirty_struct is cleared (contract).
void lk_path_update(void) {
    int structural = lkp_need_label;
    if (lk_dirty_struct) { structural = 1; lk_dirty_struct = 0; }

    // Rolling signatures: one quarter per frame, so an unflagged tile mutation
    // (a lockdown flipping Tile.locked, a zone repaint) is caught within 4
    // frames instead of never.  See D9.
    unsigned int hp, hs, hr;
    lkp_sig_quarter(lkp_sigq, &hp, &hs, &hr);
    if (hp != lkp_sig_perm[lkp_sigq]) { lkp_sig_perm[lkp_sigq] = hp; structural = 1; }
    if (hs != lkp_sig_slot[lkp_sigq]) { lkp_sig_slot[lkp_sigq] = hs; lkp_stale_mask(LKP_OCC_KINDS); }
    if (hr != lkp_sig_room[lkp_sigq]) { lkp_sig_room[lkp_sigq] = hr; lkp_stale_mask(LKP_ROOM_KINDS); }
    lkp_sigq = (lkp_sigq + 1) % LKP_SIGQ;

    unsigned int rv = lkp_sig_rooms();
    if (rv != lkp_sig_rval) { lkp_sig_rval = rv; lkp_stale_mask(LKP_ROOM_KINDS); }

    if (structural) { lkp_relabel(); lkp_stale_all(); }

    lkp_occ_snapshot();
    lkp_spent = LKP_FRAME_EXPAND - lkp_budget;     // what the last frame used
    lkp_budget = LKP_FRAME_EXPAND;

    // Bounded, even, round-robin rebuild of whatever is stale.
    int total = LKP_NCLASS * FF_COUNT;
    int built = 0;
    for (int scan = 0; scan < total && built < LKP_BUILD_PER_FRAME; scan++) {
        int i = (lkp_rr + scan) % total;
        int cls = i / FF_COUNT, kind = i % FF_COUNT;
        if (!lkp_ffstale[cls][kind]) continue;
        lkp_field_build(cls, kind);
        built++;
        lkp_rr = (i + 1) % total;
    }

#if defined(DE_TRACE) && defined(LK_PATH_TRACE)
    {
        int nstale = 0;
        for (int cls = 0; cls < LKP_NCLASS; cls++)
            for (int k = 0; k < FF_COUNT; k++) nstale += lkp_ffstale[cls][k];
        watch("lkp_regions", "%d/%d", lkp_nlab[0], lkp_nlab[1]);
        watch("lkp_pops", "%d last, %d/frame", lkp_pops, lkp_spent);
        watch("lkp_fields", "%d stale, %d built", nstale, built);
    }
#endif
}

// ── init ────────────────────────────────────────────────────────────────────
// O(LK_N * FF_COUNT) once.  Safe to call before OR after lk_grid_init: the
// signatures are primed from whatever the map currently is, every field is
// marked stale, and the labels are built immediately so a query made before the
// first lk_path_update still gets an honest answer.
void lk_path_init(void) {
    lkp_gen = 0;
    lkp_hn = 0;
    lkp_pops = 0;
    lkp_budget = LKP_FRAME_EXPAND;
    lkp_spent = 0;
    lkp_nocc = 0;
    lkp_rr = 0;
    lkp_sigq = 0;
    for (int i = 0; i < LK_N; i++) {
        lkp_state[i] = 0; lkp_g[i] = 0;
        lkp_from[i] = -1; lkp_hpos[i] = -1; lkp_heap[i] = 0;
        lkp_tmp[i] = 0; lkp_q[i] = 0; lkp_occ[i] = 0; lkp_scost[i] = 1;
    }
    for (int i = 0; i < LK_MAXACT; i++) lkp_occ_list[i] = 0;
    for (int cls = 0; cls < LKP_NCLASS; cls++) {
        lkp_nlab[cls] = 0;
        for (int i = 0; i < LK_N; i++) lkp_lab[cls][i] = -1;
        for (int k = 0; k < FF_COUNT; k++) {
            lkp_ffstale[cls][k] = 1;
            for (int i = 0; i < LK_N; i++) {
                lkp_ffd[cls][k][i] = LKP_INF;
                lkp_ffs[cls][k][i] = -1;
            }
        }
    }
    for (int q = 0; q < LKP_SIGQ; q++)
        lkp_sig_quarter(q, &lkp_sig_perm[q], &lkp_sig_slot[q], &lkp_sig_room[q]);
    lkp_sig_rval = lkp_sig_rooms();
    lkp_need_label = 1;
    lkp_relabel();            // usable immediately; redone if grid changes later
}

#endif // LOCKUP_PATH_H
