// ─────────────────────────────────────────────────────────────────────────────
// tenement/path.h — REAL PATHFINDING. A BFS distance field over the tile grid.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tnp_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
//
// INCLUDE ORDER: after world.h (whose wall grid this reads) and BEFORE offer.h (whose travel cost
// this becomes). Both are hard requirements, not preferences.
//
// ── WHY THIS MODULE EXISTS ──────────────────────────────────────────────────
// design/tenement.md §1 claims that CONTENTION for space is the game, and that "a badly planned
// building becomes visible as a traffic pattern". Until this file, `tno_travel()` in offer.h
// measured straight-line distance with sqrtf, which means walls and corridors cost NOTHING: an
// agent's bid for a fridge on the other side of a solid wall was identical to its bid for a fridge
// across an open floor. The central design claim could not be judged, because the simulation did
// not believe in walls. This module makes travel cost the truth, so the score can.
//
// ── WHAT THIS MODULE DOES *NOT* OWN: THE WALLS ──────────────────────────────
// `world` owns the wall grid, and it owns it as EDGES between tiles rather than as tiles (see its
// header: a tile-thick wall eats a third of a small flat, and an edge makes the iso footprint bug
// unrepresentable). It publishes exactly one movement rule, and this module's whole relationship
// to walls is that one call:
//
//      tn_can_step(tx, ty, dir)   → may a walker cross that side of that tile?
//
// A DOOR is walkable and a wall is not; the building's outer shell is implicit. So this module
// never sees the north/west canonical storage, never asks what KIND of wall something is, and a
// doorway needs no special case anywhere in here — it is simply an edge you may cross.
//
// world's report prescribed calling tn_can_step() inline in the flood. This CACHES it instead, as
// four bits per tile rebuilt once per tick (tnp_rebuild), for two reasons: the flood runs once per
// agent per tick and would otherwise make ~3000 calls each (72k/tick against 3072), and hashing the
// cached map is what lets the field notice a wall that moved without being told. Same rule, same
// answers — tn_can_step remains the only thing asked about walls, and CASE 0 pins that the two
// direction tables agree. This module also registers tn_walls_on_change(tn_path_dirty), so a
// segment moved mid-tick invalidates immediately rather than at the next minute.
//
// The one thing left to path's judgement is OBJECTS — world's report parks it under "NOT ASKED FOR,
// DELIBERATELY: whether furniture stops a walker is `path`'s policy" — and the answer here is that
// every object blocks, uniformly. That asks nothing about `kind` (contract rule 2): see
// tnp_obj_blocks(), the single line that will read a passability property once the contract has one.
// AGENTS never block: four residents jammed in a corridor still get through, because the comedy the
// design wants is a QUEUE, not a deadlock.
//
// An object dropped in a doorway therefore seals a route, and the sim will show that as residents
// giving up on the far half of the building. That is the feature, not a bug.
//
// ── THE SHAPE, AND WHY IT IS A FIELD AND NOT A ROUTE ────────────────────────
// The caller is a scoring loop: for ONE agent, price EVERY object (offer.h → tn_best_action).
// A route-at-a-time pathfinder would run one search per (agent, object) pair — up to
// TN_MAX_AGENTS * TN_MAX_OBJECTS = 4608 searches per tick. So the primitive here is a single
// BFS FLOOD from the agent's tile, which yields the distance to every tile at once, cached. The
// scoring loop then pays ONE flood per agent per tick and O(1) per object.
//
// Two consequences worth knowing:
//   • The field is rooted at the MOVER, so "how far is that?" is O(1) but "what is my next step?"
//     costs a walk back down the gradient, O(path length). That is the right way round: the sim
//     asks the first question thousands of times a tick and the second once per walking agent.
//   • No came-from array. The distance field IS the parent pointer: any neighbour whose distance
//     is one less is a predecessor on a shortest path.
//
// Two exemptions keep the thing honest rather than clever:
//   • THE START TILE is always leaveable, even if the player just dropped a bed on your head.
//   • A BLOCKED GOAL still gets a distance (one more than its cheapest free neighbour) but is
//     never expanded THROUGH. This is what lets the offer index ask "how far is that fridge?" and
//     get a number, while nobody ever walks through the fridge. An adjacent object is 1 away,
//     which is the same scale the old straight-line distance used, so the swap in offer.h changes
//     the units of nothing.
//
// ── UNREACHABLE IS A NUMBER, NOT -1 ─────────────────────────────────────────
// tn_path_len() returns TN_UNREACHABLE, not -1, when there is no route. The reason is the call
// site this exists for: offer.h computes `deficit * strength / (travel + 1 + queue)`. A -1 there
// is a DIVIDE BY ZERO in the core of the sim. TN_UNREACHABLE is instead chosen larger than any
// numerator the offer table can ever produce (255 * 127, the widest deficit times the widest
// `signed char` strength), so an unreachable object divides to a score of exactly 0, never wins,
// and needs no guard at a call site this module does not own. tn_path_reachable() is there for
// callers who want the honest yes/no.
//
// ── COST — MEASURED, and there is exactly one way to hold it wrong ──────────
// One flood clears and floods at most TN_MW*TN_MH = 768 tiles: ~4k integer ops, no floats, no
// allocation, 0.005 ms. A cache hit is ~5 ops. Measured on the worst case the arrays allow
// (32x24 building, 24 agents, 192 objects, a partitioned floor, 1000 ticks, -O2):
//
//   the scoring loop, sqrtf straight-line (before) : 0.133 ms/tick
//   the scoring loop, this module      (after)     : 0.138 ms/tick   ← real pathfinding is FREE
//   today's 13x9 building, 4 agents                : 0.002 ms/tick
//
// Free because the flood is amortised over every object: the per-object loop offer.h already runs
// costs more than the one flood that answers all of it. So NO extra caching is needed.
//
// THE ONE WAY TO BREAK THAT: the field cache is a SINGLE SLOT, keyed on the source tile. Callers
// must stay AGENT-MAJOR — settle every query for one agent, then move to the next, which is what
// tn_best_action does. A caller that loops objects on the OUTSIDE and agents on the inside
// re-floods on every query and costs 18.0 ms/tick instead of 0.13: measured 138x. If a future
// consumer genuinely needs interleaved sources, the fix is a small ring of fields (4 slots = 6 KB)
// rather than anything cleverer.
//
// All integer, so it is bit-identical on arm64/x86-64/wasm — unlike the sqrtf it replaces.
//
// ── HOW TO WIRE IT (three lines, and one fixture that the truth breaks) ─────
// 1. tools/carts/tenement.c — include this BETWEEN world.h and offer.h:
//        #include "tenement/path.h"     // real travel cost, before the index that spends it
// 2. offer.h — tno_travel()'s three-line body becomes one line:
//        return tn_path_len(tn_agent[agent].tx, tn_agent[agent].ty, tn_obj[obj].tx, tn_obj[obj].ty);
//    Nothing else in offer.h changes: TN_UNREACHABLE is sized so the division still yields 0.
// 3. tools/carts/tenement.c — call tn_path_selfcheck() from spec().
//
// THEN ONE EXISTING ASSERTION FAILS, AND IT SHOULD. spec() case 1's converse hand-moves the fridge
// to (1,3) and calls that "travel equalised". On the level world.h now ships, (1,3) is in the HALL,
// behind flat A's party wall: 3 steps from the agent at (1,2), not 1. The fixture was only ever
// "equal" to a straight line that could not see the wall — which is the entire point of this
// module. Fix it by moving the fridge to a tile in the agent's OWN room (verified: 1 step, and the
// hungrier need then wins as the case intends):
//        tn_obj[1].tx = 2; tn_obj[1].ty = 2; tn_path_dirty();
// With that, all of case 1/2/3/4/7/8 pass against the swap — checked by building offer.h with the
// swap applied and replaying them.
//
// ── WHAT THE CONTRACT COULD NOT EXPRESS (contract rule 3: flagged, not smuggled) ──
//   1. NOTHING IN model.h DECLARES ANY OF THIS. The wall vocabulary and the movement rule live in
//      world.h behind its TENEMENT_WALLS_IN_CONTRACT guard, and the path surface below lives here.
//      Both want to move up: `agents` needs the route, `offer` needs the length, `hud`/`art` want
//      the field to draw traffic. That is the contract's own test for shared state.
//   2. PASSABILITY. TnObject has no "blocks movement" property, so "every object blocks" is a
//      policy this module had to choose (world's report leaves it here deliberately). A rug,
//      a doorway mat, a sofa you sit ON all want to be walk-through, and none of them can say so.
//   3. FOOTPRINT — the one that actually costs correctness today. TnObject has tx,ty ("the
//      footprint origin") and no size, so every object blocks exactly ONE tile here, while the art
//      atlas draws a bed as 1x2 and a sofa as 2x1. A walker will clip through half a bed. `world`
//      hit this independently (its report, "THE FOOTPRINT TABLE IS MISSING") and wants the same fix,
//      beside the other data tables:
//          extern const unsigned char TN_OBJ_FOOTPRINT[TN_OBJ_KIND_COUNT][2];   // in TILES
//      When it lands, this module changes in ONE place: tnp_rebuild()'s object loop.
//   4. OBJECTS HAVE NO CHANGE SEAM, while walls now do. world.h grew tn_walls_on_change() so a
//      cache can be told a segment moved; nothing equivalent exists for tn_obj[], so an object
//      MOVED in place (same count, same tick) is invisible until the clock ticks and this module
//      re-hashes. Wanted, mirroring the wall seam exactly:
//          void tn_objs_on_change(void (*fn)(void));   // fired by tn_add_obj / a move / a removal
//      Until then the protocol is: move an object, call tn_path_dirty(). It bit the spec fixture
//      above, which is the cheapest possible place for it to bite.
//   5. THE MODULE TAG TABLE in model.h lists world/offer/agents/work/econ/store/art/hud/cart but
//      not `path` → `tnp_`.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_PATH_H
#define TENEMENT_PATH_H

// Bigger than any (deficit * strength) the offer table can produce, so offer.h's
// `deficit * strength / (travel + 1 + queue)` yields 0 for an unreachable object. Asserted rather
// than commented: `strength` is a signed char, so 127 is its ceiling for all time.
#define TN_UNREACHABLE 100000
_Static_assert(255 * 127 < TN_UNREACHABLE,
               "TN_UNREACHABLE must dwarf the widest deficit*strength or an unreachable object "
               "still bids (see the header block, 'UNREACHABLE IS A NUMBER')");

// ── PUBLIC SURFACE ──────────────────────────────────────────────────────────
// Distance in TILES from (fromx,fromy) to (tox,toy), walking the four cardinal directions, through
// doors, around walls and objects. 0 = you are there. TN_UNREACHABLE = no route (or either end is
// off the building). A blocked destination counts the final step onto it, so an adjacent object is
// 1 away — the same scale the straight-line distance used.
int  tn_path_len(int fromx, int fromy, int tox, int toy);
bool tn_path_reachable(int fromx, int fromy, int tox, int toy);   // the honest yes/no

// Store the actual ROUTE an agent can walk, in the module's path buffer. Returns how many tiles
// were stored (0 = already there), or -1 if there is no route. The buffer holds tiles you can
// STAND on, so a blocked destination's final step is dropped: the route ends adjacent to it, which
// is where an agent using an object stands. Read it back with tn_path_n/tn_path_x/tn_path_y.
int  tn_path_to(int fromx, int fromy, int tox, int toy);
int  tn_path_n(void);                     // tiles stored, <= TN_MAX_PATH
int  tn_path_x(int i);                    // -1 if i is out of range
int  tn_path_y(int i);

// Just the next tile to step to — what a one-tile-per-tick walker actually needs. false if there
// is no route or you are already there.
bool tn_path_next(int fromx, int fromy, int tox, int toy, int *outx, int *outy);

// Tile queries. Walls are EDGES and belong to world (tn_can_step); these are about what STANDS on
// a tile. tn_tile_occupied is the O(1) cached twin of world's O(objects) tn_obj_at.
bool tn_tile_occupied(int x, int y);      // an object stands here
bool tn_walkable(int x, int y);           // on the building AND nothing standing on it

// Throw away the cached field. Call it after changing the world WITHIN a tick — placing a wall or
// an object and then asking a distance before the clock moves. Across ticks it is automatic; see
// the cache block. Cheap: it sets one flag.
void tn_path_dirty(void);

#ifdef DE_SPEC
void tn_path_selfcheck(void);             // this module's own assertions; the cart's spec() calls it
#endif

// ── module state ────────────────────────────────────────────────────────────
// Row stride is TN_MW, NOT tn_bw: the grid is a fixed-stride array and the building is a window
// into it, so growing the building never reshuffles a tile index.
#define TNP_IDX(x, y) ((y) * TN_MW + (x))

// Direction deltas in TN_DIR_* order (N, E, S, W) — the order world.h's tn_can_step expects. Its
// own copy is private to it, so this is a parallel table by necessity, and tn_path_selfcheck
// asserts the two agree rather than trusting that they do.
static const int TNP_DX[4] = {  0,  1,  0, -1 };
static const int TNP_DY[4] = { -1,  0,  1,  0 };

static unsigned char tnp_edge[TN_N];      // bit d = tn_can_step(x,y,d) — the wall grid, flattened
static unsigned char tnp_occ[TN_N];       // an object stands on this tile
static short         tnp_dist[TN_N];      // the cached field. -1 = unvisited/off-building.
static short         tnp_q[TN_N];         // BFS queue of tile indices (TN_N == 768, fits a short)

static bool tnp_ready = false;            // are tnp_edge/tnp_occ current?
static bool tnp_hooked = false;           // have we registered with world's wall-change seam?
static int  tnp_srcx = -1, tnp_srcy = -1; // whose field is in tnp_dist (-1 = none)
static unsigned tnp_hash;                 // signature of the world tnp_dist was flooded through
static int  tnp_seen_min = -1, tnp_seen_day = -1, tnp_seen_objn = -1;
static int  tnp_seen_bw = -1, tnp_seen_bh = -1;

// The path buffer, plus the scratch the gradient walk fills (which runs goal→start, so it cannot
// be truncated in place: a 200-tile route must keep its FIRST TN_MAX_PATH tiles, not its last).
static unsigned char tnp_px[TN_MAX_PATH], tnp_py[TN_MAX_PATH];
static int           tnp_n;
static unsigned char tnp_rx[TN_N], tnp_ry[TN_N];

// The BUILDING is tn_bw x tn_bh (variables: the player grows the place). world's tn_in_building is
// the truth about that, and this adds the ARRAY bound on top, so a build module that over-grows the
// building past TN_MW/TN_MH gets a smaller world here rather than a stomped heap.
static bool tnp_inside(int x, int y) { return tn_in_building(x, y) && x < TN_MW && y < TN_MH; }

// Does this object stand in the way? EVERY object does, uniformly, which is why this asks nothing
// about `kind` (contract rule 2 — `if (obj->kind == TN_OBJ_FRIDGE)` would break the architecture).
// When TnObject grows a passability property, THIS is the one line that reads it.
static bool tnp_obj_blocks(int obj) { (void)obj; return true; }

void tn_path_dirty(void) { tnp_ready = false; tnp_srcx = tnp_srcy = -1; }

// ── THE CACHE, AND HOW IT IS KEPT HONEST ────────────────────────────────────
// A cached field that outlives the world it was flooded through is the one way this module can
// lie, and it would lie SILENTLY: agents walking through a wall that is already there. Walls
// belong to another module and objects move without telling anyone, so this does not rely on being
// notified. It REBUILDS the passability map once per simulated minute and hashes it: if the world
// moved, the field is dropped. Correct at tick granularity, no protocol, nobody to forget.
//
// The rebuild is ~4 tn_can_step calls per tile of the CURRENT building (468 at 13x9) once per
// tick, against the ~130k ops/tick the scoring loop spends — measured in the cost block, it does
// not show up. Change the world and ask within the SAME tick (a build cursor, a spec fixture) and
// you must call tn_path_dirty(); that is the only case, and it is one line.
static void tnp_rebuild(void) {
    unsigned h = 2166136261u ^ (unsigned)(tn_bw * 131 + tn_bh);
    for (int i = 0; i < TN_N; i++) { tnp_occ[i] = 0; tnp_edge[i] = 0; }
    for (int o = 0; o < tn_obj_n; o++) {
        if (!tnp_obj_blocks(o)) continue;
        const int x = tn_obj[o].tx, y = tn_obj[o].ty;   // ONE tile: no footprint in the contract
        if (!tnp_inside(x, y)) continue;                // (report item 3 — the bed's other half)
        tnp_occ[TNP_IDX(x, y)] = 1;
        h = h * 16777619u + (unsigned)(TNP_IDX(x, y) + 1);
    }
    for (int y = 0; y < TN_MH; y++)
        for (int x = 0; x < TN_MW; x++) {
            if (!tnp_inside(x, y)) continue;
            unsigned char m = 0;
            for (int d = 0; d < 4; d++)                 // the ONE thing walls are asked, per side
                if (tn_can_step(x, y, d)) m |= (unsigned char)(1 << d);
            tnp_edge[TNP_IDX(x, y)] = m;
            h = h * 16777619u + m;
        }
    if (h != tnp_hash) { tnp_hash = h; tnp_srcx = tnp_srcy = -1; }   // the world moved: field void
    tnp_seen_objn = tn_obj_n; tnp_seen_bw = tn_bw; tnp_seen_bh = tn_bh;
    tnp_seen_min  = tn_clock.minute; tnp_seen_day = tn_clock.day;
    tnp_ready = true;
}

static void tnp_sync(void) {
    if (!tnp_hooked) {                    // world's wall-change seam, so a moved segment is instant
        tnp_hooked = true;                // rather than noticed at the next tick. Registered lazily
        tn_walls_on_change(tn_path_dirty);// because this module has no init entry point and the cart
    }                                     // should not have to know. NOTE: single slot — a second
                                          // cache registering would displace this one.
    if (!tnp_ready || tn_obj_n != tnp_seen_objn || tn_bw != tnp_seen_bw || tn_bh != tnp_seen_bh ||
        tn_clock.minute != tnp_seen_min || tn_clock.day != tnp_seen_day)
        tnp_rebuild();
}

// ── the flood ───────────────────────────────────────────────────────────────
// Plain 4-way BFS, so every edge costs 1 and the first time a tile is reached is its shortest
// distance. Diagonals are deliberately absent: the art is a 2:1 iso diamond where a diagonal step
// reads as a full tile, and a free diagonal would make a room cheaper to cross than it looks —
// besides letting a walker cut the corner of a wall junction, which is exactly the thing edge
// walls exist to prevent.
static void tnp_flood(int sx, int sy) {
    for (int i = 0; i < TN_N; i++) tnp_dist[i] = -1;
    int head = 0, tail = 0;
    const int s = TNP_IDX(sx, sy);
    tnp_dist[s] = 0;
    tnp_q[tail++] = (short)s;                     // the START is always leaveable, occupied or not
    while (head < tail) {
        const int c = tnp_q[head++];
        const int cx = c % TN_MW, cy = c / TN_MW;
        const short nd = (short)(tnp_dist[c] + 1);
        const unsigned char m = tnp_edge[c];
        for (int d = 0; d < 4; d++) {
            if (!(m & (1 << d))) continue;             // a wall on that side, or the lot edge
            const int ni = TNP_IDX(cx + TNP_DX[d], cy + TNP_DY[d]);
            if (tnp_dist[ni] >= 0) continue;
            tnp_dist[ni] = nd;
            if (!tnp_occ[ni]) tnp_q[tail++] = (short)ni;   // an occupied tile gets a DISTANCE (so it
        }                                                  // can be a destination) but is never
    }                                                      // expanded THROUGH — nobody walks
    tnp_srcx = sx; tnp_srcy = sy;                          // inside the fridge
}

// ── queries ─────────────────────────────────────────────────────────────────
int tn_path_len(int fromx, int fromy, int tox, int toy) {
    tnp_sync();
    if (!tnp_inside(fromx, fromy) || !tnp_inside(tox, toy)) return TN_UNREACHABLE;
    if (fromx == tox && fromy == toy) return 0;
    if (tnp_srcx != fromx || tnp_srcy != fromy) tnp_flood(fromx, fromy);
    const short d = tnp_dist[TNP_IDX(tox, toy)];
    return d < 0 ? TN_UNREACHABLE : (int)d;
}

bool tn_path_reachable(int fromx, int fromy, int tox, int toy) {
    return tn_path_len(fromx, fromy, tox, toy) < TN_UNREACHABLE;
}

bool tn_tile_occupied(int x, int y) {
    tnp_sync();
    return tnp_inside(x, y) ? tnp_occ[TNP_IDX(x, y)] != 0 : false;
}
bool tn_walkable(int x, int y) { return tnp_inside(x, y) && !tn_tile_occupied(x, y); }

// ── the route ───────────────────────────────────────────────────────────────
// Walk DOWNHILL from the goal: a neighbour whose distance is one less is, by construction, a
// predecessor on a shortest path. Collect goal→start, then reverse, so a route longer than the
// TN_MAX_PATH buffer keeps its FIRST tiles (a valid prefix an agent can start walking) rather than
// its last. tn_path_len() stays the authority on the total length.
int tn_path_to(int fromx, int fromy, int tox, int toy) {
    tnp_n = 0;
    const int len = tn_path_len(fromx, fromy, tox, toy);   // this syncs + floods
    if (len >= TN_UNREACHABLE) return -1;
    if (len == 0) return 0;

    int rn = 0, cx = tox, cy = toy, d = len;
    if (!tnp_occ[TNP_IDX(tox, toy)]) {            // an occupied goal is not somewhere you can stand,
        tnp_rx[rn] = (unsigned char)cx;           // so the route stops next to it
        tnp_ry[rn] = (unsigned char)cy; rn++;
    }
    while (d > 1) {                               // stop at distance 1: that is the FIRST step, and
        bool found = false;                       // the tile you are standing on is not a step
        for (int k = 0; k < 4; k++) {
            if (!tnp_inside(cx + TNP_DX[k], cy + TNP_DY[k])) continue;
            const int ni = TNP_IDX(cx + TNP_DX[k], cy + TNP_DY[k]);
            if (tnp_dist[ni] != (short)(d - 1)) continue;
            if (tnp_occ[ni]) continue;            // predecessors are free by construction; belt and
            // Crossing back must be legal from the NEIGHBOUR's side too. It always is (the edge is
            // shared and tn_can_step is symmetric), and this reads it from the side being entered.
            if (!(tnp_edge[ni] & (1 << ((k + 2) & 3)))) continue;
            cx += TNP_DX[k]; cy += TNP_DY[k]; d--;
            if (rn < TN_N) { tnp_rx[rn] = (unsigned char)cx; tnp_ry[rn] = (unsigned char)cy; rn++; }
            found = true; break;
        }
        if (!found) break;                        // cannot happen on a consistent field; bail
    }                                             // rather than spin
    for (int i = rn - 1; i >= 0 && tnp_n < TN_MAX_PATH; i--) {
        tnp_px[tnp_n] = tnp_rx[i]; tnp_py[tnp_n] = tnp_ry[i]; tnp_n++;
    }
    return tnp_n;
}

int tn_path_n(void)  { return tnp_n; }
int tn_path_x(int i) { return (i >= 0 && i < tnp_n) ? tnp_px[i] : -1; }
int tn_path_y(int i) { return (i >= 0 && i < tnp_n) ? tnp_py[i] : -1; }

bool tn_path_next(int fromx, int fromy, int tox, int toy, int *outx, int *outy) {
    if (tn_path_to(fromx, fromy, tox, toy) <= 0) return false;
    if (outx) *outx = tnp_px[0];
    if (outy) *outy = tnp_py[0];
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SPEC — this module's own assertions (spec.h, "SPECS ON AN INCLUDEABLE"). The cart's spec() calls
// tn_path_selfcheck(); it restores the world with tn_world_init() at both ends, so it can sit
// anywhere in the sequence.
//
// The case that matters is CASE 3: a wall between two points makes the path LONGER than the
// straight line, and a doorway makes it shorter again. That is the assertion that proves walls
// matter, and design §1's claim (a badly planned building shows up as a traffic pattern) rests on
// it. CASE 9 then makes the same claim about the SHIPPED level rather than a fixture.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DE_SPEC
static char tnp_sp[192];

// The straight-line distance offer.h used to use, kept here as the CONTROL the wall cases measure
// against. Not a fallback and not called by anything else: deleting it would make case 3 a bare
// number instead of a comparison.
static int tnp_crow(int fromx, int fromy, int tox, int toy) {
    const int dx = fromx - tox, dy = fromy - toy;
    return (int)(sqrtf((float)(dx * dx + dy * dy)) + 0.5f);
}

// A blank floor: every stored edge cleared, so only the implicit outer shell remains. Goes through
// the public setter, which canonicalises, so this cannot desync the north/west storage.
static void tnp_spec_open_floor(void) {
    for (int y = 0; y < tn_bh; y++)
        for (int x = 0; x < tn_bw; x++) {
            tn_edge_set(x, y, TN_DIR_N, TN_WALL_NONE);
            tn_edge_set(x, y, TN_DIR_W, TN_WALL_NONE);
        }
    tn_rooms_rebuild();
    tn_path_dirty();
}

void tn_path_selfcheck(void) {
    // ── CASE 0: THE DIRECTION TABLE AGREES WITH world.h's ───────────────────
    // TNP_DX/DY is a parallel copy of a table that is private to world.h, and a silent mismatch
    // would make every path subtly wrong rather than broken. Pin it: stepping in direction d from
    // a tile must land on the tile world.h thinks it does, which is checkable through tn_can_step
    // alone — on a blank floor, the only unsteppable sides are the ones facing off the building.
    tn_world_init();
    tn_bw = 13; tn_bh = 9;
    tn_obj_n = 0; tn_agent_n = 0;
    tnp_spec_open_floor();
    {
        int agree = 1;
        for (int d = 0; d < 4; d++) {
            const int x = 6 + TNP_DX[d], y = 4 + TNP_DY[d];
            if (!tn_can_step(6, 4, d)) agree = 0;                  // mid-floor: every side is open
            if (!tn_in_building(x, y)) agree = 0;
        }
        if (tn_can_step(0, 0, TN_DIR_N) || tn_can_step(0, 0, TN_DIR_W)) agree = 0;   // the shell
        expect(agree, "path: TN_DIR_* deltas match world's wall grid (a silent mismatch would bend "
                      "every route by one tile)");
    }

    // ── CASE 1: AN OPEN FLOOR COSTS EXACTLY MANHATTAN ───────────────────────
    // The floor of everything else: on a blank grid a 4-way BFS must equal |dx|+|dy|. Bigger and
    // the flood is leaking around a phantom obstacle; smaller and diagonals crept in, making every
    // room cheaper to cross than the art says.
    expect_eq(tn_path_len(1, 2, 5, 2), 4, "path: an open floor costs exactly the tiles walked");
    expect_eq(tn_path_len(1, 1, 4, 3), 5, "path: 4-way only — an L costs |dx|+|dy|, diagonals are not free");
    expect_eq(tn_path_len(3, 3, 3, 3), 0, "path: you are already where you are standing");
    expect_eq(tn_path_len(1, 2, 5, 2), tn_path_len(5, 2, 1, 2), "path: distance is symmetric");

    // ── CASE 2: OFF THE BUILDING IS UNREACHABLE, NOT GARBAGE ────────────────
    // tn_bw/tn_bh are VARIABLES (the player grows the building), so the edge is not a #define and
    // reading past it is the easy heap stomp in this module.
    expect_eq(tn_path_len(1, 1, tn_bw, 1), TN_UNREACHABLE, "path: a tile past the building's edge is unreachable");
    expect_eq(tn_path_len(-1, 1, 1, 1), TN_UNREACHABLE, "path: so is standing outside it");

    // ── CASE 3: A WALL COSTS SOMETHING, AND A DOORWAY GIVES IT BACK ─────────
    // THE case. Same two points throughout: A(1,2) in the left room, B(5,2) in the right one.
    // The straight line is 4 and NEVER CHANGES, because a straight line does not believe in walls.
    // That was tno_travel() before this module existed, which is why design §1's claim that a badly
    // planned building shows up as a traffic pattern could not be judged.
    {
        const int crow = tnp_crow(1, 2, 5, 2);
        expect_eq(tn_path_len(1, 2, 5, 2), crow, "path: with no wall, the walk and the crow agree");

        for (int y = 0; y < tn_bh; y++) tn_edge_set(3, y, TN_DIR_W, TN_WALL_SOLID);
        tn_path_dirty();                                   // same tick as the wall going up
        expect_eq(tn_path_len(1, 2, 5, 2), TN_UNREACHABLE, "path: an unbroken partition makes the far room unreachable");
        expect_eq(tnp_crow(1, 2, 5, 2), crow, "path: ...while the straight line still cheerfully says 4");

        tn_edge_set(3, 6, TN_DIR_W, TN_WALL_DOOR);         // one door, in the far corner
        tn_path_dirty();
        const int far_door = tn_path_len(1, 2, 5, 2);
        snprintf(tnp_sp, sizeof tnp_sp,
                 "path: A WALL MAKES THE WALK LONGER — %d tiles round to a far door where the "
                 "straight line says %d", far_door, crow);
        expect(far_door > crow, tnp_sp);
        expect_eq(far_door, 12, "path: and the detour is exactly the 12 tiles that door forces");
        expect_eq(tn_path_len(5, 2, 1, 2), far_door, "path: the detour is symmetric too");

        tn_edge_set(3, 2, TN_DIR_W, TN_WALL_DOOR);         // a door ON the straight line
        tn_path_dirty();
        const int near_door = tn_path_len(1, 2, 5, 2);
        snprintf(tnp_sp, sizeof tnp_sp,
                 "path: A DOORWAY MAKES IT SHORT AGAIN — %d tiles, down from %d, so WHERE the "
                 "player puts the door is worth something to the score", near_door, far_door);
        expect(near_door == crow && near_door < far_door, tnp_sp);

        // A door is walkable but still a wall to the eye: it must not become a hole in the room map.
        expect(tn_edge_at(3, 2, TN_DIR_W) == TN_WALL_DOOR,
               "path: and it is still a DOOR afterwards — path walks it, art draws it");
        // The partition must not be a global fog: a trip that never crosses it is untouched.
        expect_eq(tn_path_len(1, 2, 2, 5), 4, "path: a wall costs nothing to a walk that never crosses it");
    }

    // ── CASE 4: OBJECTS BLOCK, WITHOUT ANYONE NAMING A KIND ─────────────────
    // The kind below is arbitrary on purpose: blocking is not a property of kind (contract rule 2).
    tnp_spec_open_floor();
    {
        expect_eq(tn_path_len(2, 1, 4, 1), 2, "path: two tiles apart across an empty floor");
        tn_add_obj(TN_OBJ_BED, 3, 1, -1);                  // any kind: this one just happens to exist
        const int detour = tn_path_len(2, 1, 4, 1);
        snprintf(tnp_sp, sizeof tnp_sp, "path: an object in the way forces a detour (2 -> %d)", detour);
        expect(detour == 4, tnp_sp);

        // The offer index asks "how far is that object?", and the object's own tile is where it
        // stands. So an occupied destination must still answer with a number: adjacent means 1.
        expect_eq(tn_path_len(2, 1, 3, 1), 1, "path: an object's own tile is a legal destination, 1 step away");
        expect(tn_tile_occupied(3, 1) && !tn_walkable(3, 1), "path: and that tile reads as occupied");
        expect(tn_walkable(2, 1) && !tn_tile_occupied(2, 1), "path: while the floor beside it is walkable");

        // An object dropped in a doorway SEALS a route. That is the traffic pattern design §1 wants
        // to be visible, so it is asserted as intended behaviour rather than tolerated.
        tnp_spec_open_floor(); tn_obj_n = 0;
        for (int y = 0; y < tn_bh; y++) tn_edge_set(3, y, TN_DIR_W, TN_WALL_SOLID);
        tn_edge_set(3, 4, TN_DIR_W, TN_WALL_DOOR);
        tn_add_obj(TN_OBJ_WARDROBE, 3, 4, -1);             // parked in the only doorway
        tn_path_dirty();
        expect_eq(tn_path_len(1, 2, 5, 2), TN_UNREACHABLE,
                  "path: furniture parked in the only doorway seals the building, and the sim says so");
    }

    // ── CASE 5: UNREACHABLE IS A NUMBER THE SCORE SURVIVES ──────────────────
    // offer.h computes deficit*strength/(travel+1+queue). A -1 there divides by zero in the core of
    // the sim; TN_UNREACHABLE instead has to be big enough that every row of the offer table scores
    // 0. Checked against the table itself, so a future row cannot quietly break it.
    {
        int worst = 0;
        for (int k = 0; k < TN_OBJ_KIND_COUNT; k++)
            for (int i = 0; i < TN_OFFER_N[k]; i++) {
                const int s = TN_OFFERS[k][i].strength;
                if (s <= 0) continue;
                const int score = 255 * s / (TN_UNREACHABLE + 1);
                if (score > worst) worst = score;
            }
        snprintf(tnp_sp, sizeof tnp_sp,
                 "path: every offer in the table scores 0 at TN_UNREACHABLE (worst %d), so swapping "
                 "tno_travel cannot divide by zero and needs no extra guard", worst);
        expect(worst == 0, tnp_sp);
    }

    // ── CASE 6: YOU CAN ALWAYS STEP OFF WHERE YOU STAND ─────────────────────
    // The player may drop a bed on an agent. That must cost a step, not trap them forever.
    tnp_spec_open_floor(); tn_obj_n = 0;
    tn_add_obj(TN_OBJ_BED, 1, 1, -1);
    expect_eq(tn_path_len(1, 1, 5, 1), 4, "path: a blocked START is still walkable — you step off it");

    // ── CASE 7: THE ROUTE IS WALKABLE, TILE BY TILE ─────────────────────────
    // A length is a claim; a route is checkable. Every stored tile must be one cardinal step from
    // the last, standable, and on a side the walls allow — or an agent following it walks through
    // a wall, which is the exact bug this module exists to make impossible.
    tnp_spec_open_floor(); tn_obj_n = 0;
    for (int y = 0; y < tn_bh; y++) tn_edge_set(3, y, TN_DIR_W, TN_WALL_SOLID);
    tn_edge_set(3, 6, TN_DIR_W, TN_WALL_DOOR);
    tn_path_dirty();
    {
        const int n = tn_path_to(1, 2, 5, 2);
        expect_eq(n, 12, "path: the stored route is as long as the distance says");
        int ok = (n > 0), px = 1, py = 2;
        for (int i = 0; i < n; i++) {
            const int x = tn_path_x(i), y = tn_path_y(i);
            int legal = 0;
            for (int d = 0; d < 4; d++)                        // one cardinal step, through a side
                if (px + TNP_DX[d] == x && py + TNP_DY[d] == y && tn_can_step(px, py, d)) legal = 1;
            if (!legal) ok = 0;
            if (tn_tile_occupied(x, y)) ok = 0;
            px = x; py = y;
        }
        if (px != 5 || py != 2) ok = 0;                        // and it actually arrives
        expect(ok, "path: every tile of the route is one legal step, standable, and it arrives");

        int nx = 0, ny = 0;
        expect(tn_path_next(1, 2, 5, 2, &nx, &ny) && nx == tn_path_x(0) && ny == tn_path_y(0),
               "path: tn_path_next hands back the first tile of that same route");
        expect(tn_path_to(1, 2, 1, 2) == 0, "path: a route to where you stand is zero tiles, not an error");
    }

    // ── CASE 8: A ROUTE TO AN OCCUPIED TILE STOPS NEXT TO IT ────────────────
    // An agent using a fridge stands beside it (agents.h treats adjacency as arrival), so the
    // walkable route is one shorter than the distance the score uses.
    tnp_spec_open_floor(); tn_obj_n = 0;
    tn_add_obj(TN_OBJ_FRIDGE, 5, 2, -1);
    {
        const int len = tn_path_len(1, 2, 5, 2), n = tn_path_to(1, 2, 5, 2);
        expect(len == 4 && n == 3, "path: the score counts the step INTO the object, the route stops beside it");
        expect(abs(tn_path_x(n - 1) - 5) + abs(tn_path_y(n - 1) - 2) == 1,
               "path: and the tile it stops on is adjacent to the object");
    }
    // No route at all is said out loud, rather than as half a walk.
    tnp_spec_open_floor(); tn_obj_n = 0;
    for (int y = 0; y < tn_bh; y++) tn_edge_set(3, y, TN_DIR_W, TN_WALL_SOLID);
    tn_path_dirty();
    expect(tn_path_to(1, 2, 5, 2) == -1 && tn_path_n() == 0,
           "path: no route stores no route, and says -1 rather than half a walk");

    // ── CASE 9: THE SHIPPED LEVEL, NOT A FIXTURE ────────────────────────────
    // Everything above is a fixture built to make a point. This asks the same question of the
    // building the game actually starts with: over every (resident, object) pair, is the walk ever
    // longer than the crow's flight? If not, this module changed nothing that matters and the
    // sim's scores are still blind to the four flats, the hall and the five doors.
    tn_world_init();
    tn_path_dirty();
    {
        int longer = 0, shorter = 0, pairs = 0, worst = 0;
        for (int a = 0; a < tn_agent_n; a++)
            for (int o = 0; o < tn_obj_n; o++) {
                const int p = tn_path_len(tn_agent[a].tx, tn_agent[a].ty, tn_obj[o].tx, tn_obj[o].ty);
                const int c = tnp_crow(tn_agent[a].tx, tn_agent[a].ty, tn_obj[o].tx, tn_obj[o].ty);
                pairs++;
                if (p < c) shorter++;                          // impossible: a route cannot beat a
                if (p > c) { longer++; if (p - c > worst) worst = p - c; }   // straight line
            }
        expect(shorter == 0, "path: no walk in the shipped level is SHORTER than the straight line "
                             "(it cannot be, and a broken flood is how it would happen)");
        snprintf(tnp_sp, sizeof tnp_sp,
                 "path: in the SHIPPED building %d of %d (resident, object) pairs cost more than "
                 "the straight line, by up to %d tiles — the walls are now in the score",
                 longer, pairs, worst);
        expect(longer > 0, tnp_sp);

        // And the one that says the level is playable at all: everyone can reach the one toilet.
        // Found by what it OFFERS, never by kind (contract rule 2), and by the offer rather than by
        // a score, so a sated agent cannot make this case pass by accident.
        int t = -1;
        for (int o = 0; o < tn_obj_n && t < 0; o++)
            if (tn_offers(o, TN_SERVE_BLADDER, NULL)) t = o;
        int all = (t >= 0);
        for (int a = 0; a < tn_agent_n && t >= 0; a++)
            if (!tn_path_reachable(tn_agent[a].tx, tn_agent[a].ty, tn_obj[t].tx, tn_obj[t].ty)) all = 0;
        expect(all, "path: every resident of the shipped building can actually walk to a toilet");
    }

    // Leave the world exactly as the other cases expect to find it.
    tn_world_init();
    tn_path_dirty();
}
#endif // DE_SPEC

#endif // TENEMENT_PATH_H
