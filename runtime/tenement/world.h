// ─────────────────────────────────────────────────────────────────────────────
// tenement/world.h — the contract's globals, spawning, the WALL GRID, ROOMS, and the level.
// Owner of tn_obj/tn_item/tn_clock/tn_bw/tn_bh.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tnw_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
//
// ── WALLS ARE EDGES, NOT TILES, AND THEY ARE TERRAIN, NOT OBJECTS ────────────
// Two decisions, both load-bearing:
//
// 1. A wall sits on the BOUNDARY BETWEEN two tiles, not on a tile of its own. Reasons, in order of
//    weight:
//      • A tile-thick wall eats a third of a small flat at 13x9 tiles. Rooms this size cannot
//        afford to spend floor on structure.
//      • It sidesteps the trap iso-rooms.md §8 warns about ("a turned non-square object's footprint
//        does not follow its art"). A wall model is 6x2 voxels — NON-SQUARE — so if a wall were an
//        object with a `facing`, its art would turn and its footprint would not. Here orientation is
//        not stored at all: it is implied by WHICH EDGE the segment is on, and the art module reads
//        that straight off the direction (N/S edge → the *_NS model, E/W edge → the *_EW model).
//        The bug is not solved, it is made unrepresentable.
//      • A doorway is then the ABSENCE of a segment on one edge, so a door costs no new concept.
//    Storage is canonical: only the NORTH and WEST edge of each tile is stored. The east edge of
//    (x,y) IS the west edge of (x+1,y); asking either way gives the same answer.
//
// 2. A wall is NOT a TnObject. It cannot be: spec() case 5 asserts every object kind declares at
//    least one valid offer, and a wall offers nothing. Walls are terrain, queried through the two
//    functions below, so neither `path` nor `art` ever switches on an object kind to find them
//    (contract rule 2).
//
//      tn_edge_at(tx,ty,dir)   → TN_WALL_NONE / _SOLID / _DOOR on that SIDE of that tile.  art
//      tn_can_step(tx,ty,dir)  → may a walker cross that side at all?                      path
//
//    The building's outer shell is IMPLICIT: any edge with the building on one side and nothing on
//    the other reports _SOLID without being stored, so growing tn_bw/tn_bh moves the shell for free.
//
//    NAMED `tn_edge_*` AND NOT `tn_wall_*` FOR A REASON: `path` briefly owned a rival TILE-based
//    wall layer under `tn_wall_at(x,y)` / `tn_wall_set(x,y,solid)`, which would not even have
//    compiled beside this one. It reads tn_can_step() now. Item 1 of the report at the bottom of
//    this header keeps the argument that settled it, because it decides how walls are drawn too.
//
// ── ROOMS ARE DERIVED, NEVER DECLARED ───────────────────────────────────────
// A room is a connected run of tiles with NO wall segment at all between them: a door still BOUNDS
// a room (it is a hole in an enclosure, not the absence of one) while remaining walkable. So room
// ids fall out of the wall grid by flood fill, which means the moment the `build` agent lets the
// player move a wall, ownership re-derives itself instead of drifting.
//
// Room IDS renumber on every rebuild, so identity lives in a SEED TILE, not in an id: a household
// declares "I live in the room containing this tile" (tn_room_assign) and tn_rooms_rebuild()
// re-derives every `dwelling` from the seeds afterwards.
//
// ── THE LEVEL: four flats off one hall, one shared WC, one machine ───────────
// `·` = where a resident starts.  B bed (1x2 tiles, spills DOWN)  F fridge  W wardrobe
// S sofa (2x1 tiles, spills RIGHT)  L loom  T toilet.  ┄ = a door.
//
//         x:  0  1  2  3  4  5   6  7  8  9 10 11 12
//            ┌─────────────────┬──────────────────────┐
//     y=0    │ B  F  W  .  S  S│ B  F  .  .  .  .  W  │
//     y=1    │ B  .  .  ·  .  . │ B  .  .  ·  .  .  . │   FLAT A (h0)  │  FLAT B (h1)
//     y=2    │ .  .  .  .  .  . │ .  .  .  .  .  .  . │
//            └────┄────────────┴─────┄─────────────────┘
//              .  .  .  .  L  .│ T  .│ .  .  .  S  S       y=3   THE HALL (communal)
//              .  .  .  .  .  .└──┄──┘ .  .  .  .  .       y=4   + the shared WC
//            ┌────┄────────────┬─────┄─────────────────┐
//     y=5    │ .  .  .  .  .  . │ .  .  .  .  .  .  . │
//     y=6    │ .  .  .  ·  .  . │ .  .  .  ·  .  .  . │   FLAT C (h2)  │  FLAT D (h3)
//     y=7    │ B  .  .  .  .  . │ B  .  .  .  .  .  . │
//     y=8    │ B  .  F  .  .  W │ B  .  F  .  .  .  W │
//            └─────────────────┴──────────────────────┘
//
// WHY THIS SHAPE GENERATES THE CONTENTION design §1 IS ABOUT — every scarcity here is a COUNT, so
// it is asserted rather than hoped for (tn_world_selfcheck below):
//   • ONE toilet, in a shared WC, for four households. Bladder is the fastest-decaying need in the
//     game (agents.h: 10/hr, the highest) and the toilet has capacity 1. This is the design's own
//     "four people failing to share one bathroom", made structural instead of incidental.
//   • ONE loom, capacity 1, on a 480-minute shift: whoever reaches it first owns the whole day.
//   • ONE communal sofa (capacity 2) for three households — flat A has its own, and that visible
//     inequality is the point. Nothing here is symmetric.
//   • FOUR doors onto ONE hall, and the hall's only through-route is row y=4, kept deliberately
//     clear of furniture. Every trip anyone makes crosses it.
//   • Fridges are PRIVATE but ownership is not yet a term in the score (spec case 8), and the flats
//     are a couple of tiles apart through a wall the score cannot see. Residents WILL raid the
//     nearest neighbour's fridge. That is the comedy §6 asks for, produced by the layout.
// Furniture hugs the wall furthest from the hall, so each flat keeps a clear lane to its own door.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_WORLD_H
#define TENEMENT_WORLD_H

// ── things that BELONG IN THE CONTRACT, parked here until the owner moves them ──
// model.h is frozen and this module may not edit it, so the wall vocabulary lives here behind a
// guard: define TENEMENT_WALLS_IN_CONTRACT in model.h when these move up, and this block goes quiet
// without a single edit here. See the report at the bottom of this header for the full list.
#ifndef TENEMENT_WALLS_IN_CONTRACT
typedef enum { TN_WALL_NONE = 0, TN_WALL_SOLID, TN_WALL_DOOR } TnWallKind;
// Directions. NOT `TN_N` — the contract already spends that name on TN_MW*TN_MH.
enum { TN_DIR_N = 0, TN_DIR_E, TN_DIR_S, TN_DIR_W };
#define TN_MAX_ROOMS 24
#endif

TnObject    tn_obj[TN_MAX_OBJECTS];       int tn_obj_n;
TnItem      tn_item[TN_MAX_ITEMS];        int tn_item_n;
TnAgent     tn_agent[TN_MAX_AGENTS];      int tn_agent_n;
TnHousehold tn_house[TN_MAX_HOUSEHOLDS];  int tn_house_n;
TnOrder     tn_order[TN_MAX_ORDERS];      int tn_order_n;
TnClock     tn_clock;
int         tn_bw = 13, tn_bh = 9;   // sized so the diamond fits 320px: (13+9) * 12 = 264
int         tn_rot;

// ── the wall grid + the derived room map (private: the canonical N/W trick is nobody else's
// business, and `path` must not be able to forget to canonicalise) ───────────
static unsigned char tnw_wall_n[TN_MH][TN_MW];   // segment on the NORTH edge of tile (tx,ty)
static unsigned char tnw_wall_w[TN_MH][TN_MW];   // segment on the WEST  edge of tile (tx,ty)
static signed char   tnw_room[TN_MH][TN_MW];     // room id per tile, -1 = outside / unlabelled
static int           tnw_room_n;
static int           tnw_shared = -1;            // the communal circulation space (the hall)
static signed char   tnw_shared_x = -1, tnw_shared_y = -1;
static signed char   tnw_seed_x[TN_MAX_HOUSEHOLDS], tnw_seed_y[TN_MAX_HOUSEHOLDS];
static short         tnw_q[TN_N];                // one BFS queue, reused. TN_N == 768, fits a short.

static const signed char TNW_DX[4] = { 0, 1, 0, -1 };
static const signed char TNW_DY[4] = { -1, 0, 1, 0 };

// ── the two queries every other module uses ──────────────────────────────────
bool tn_in_building(int tx, int ty) {
    return tx >= 0 && ty >= 0 && tx < tn_bw && ty < tn_bh;
}

// What is on side `dir` of tile (tx,ty)? ART: draw the *_NS model for a N/S side and the *_EW model
// for an E/W side — the orientation is the direction, so no facing is stored and nothing turns.
// (isoroom.c does exactly this already: it pushes wall_ns on the north/south edges and wall_ew on
// the east/west ones, offset OUTSIDE the floor by the model's own thickness. Same geometry.)
int tn_edge_at(int tx, int ty, int dir) {
    dir &= 3;
    const int nx = tx + TNW_DX[dir], ny = ty + TNW_DY[dir];
    const bool here = tn_in_building(tx, ty), there = tn_in_building(nx, ny);
    if (!here && !there) return TN_WALL_NONE;    // an edge nowhere near the building
    if (!here || !there) return TN_WALL_SOLID;   // the outer shell, implicit and always solid
    switch (dir) {
    case TN_DIR_N: return tnw_wall_n[ty][tx];
    case TN_DIR_W: return tnw_wall_w[ty][tx];
    case TN_DIR_S: return tnw_wall_n[ny][nx];    // the south edge IS the next tile's north edge
    default:       return tnw_wall_w[ny][nx];    // the east  edge IS the next tile's west  edge
    }
}

// PATH: this is the whole movement rule, and it is the ONLY function a pathfinder needs from here.
// A door is walkable, a wall is not, the lot edge is not. Walls cost no floor, so there is no
// "which tiles are wall" question to ask — which is the point of putting them on edges.
bool tn_can_step(int tx, int ty, int dir) {
    const int nx = tx + TNW_DX[dir & 3], ny = ty + TNW_DY[dir & 3];
    if (!tn_in_building(tx, ty) || !tn_in_building(nx, ny)) return false;
    return tn_edge_at(tx, ty, dir) != TN_WALL_SOLID;
}

// Anyone caching a route or a distance field over these walls registers here and gets told when a
// segment moves. A callback rather than a direct call because this module may not name a sibling
// (`path` is included after it, and its prototypes are not in the contract yet). Same seam shape as
// build.h's tn_build_set_blocked.
static void (*tnw_on_change)(void) = NULL;
void tn_walls_on_change(void (*fn)(void)) { tnw_on_change = fn; }

// Put a segment on a side. For the `build` agent: it canonicalises, so the caller never has to know
// that east is somebody else's west. false = that edge is the outer shell (implicit) or off-grid.
// Rooms are NOT relabelled here: a player dragging a wall wants one rebuild at the end of the drag,
// not one per segment, so tn_rooms_rebuild() is the caller's to call.
bool tn_edge_set(int tx, int ty, int dir, int kind) {
    dir &= 3;
    const int nx = tx + TNW_DX[dir], ny = ty + TNW_DY[dir];
    if (!tn_in_building(tx, ty) || !tn_in_building(nx, ny)) return false;
    unsigned char *slot;
    switch (dir) {
    case TN_DIR_N: slot = &tnw_wall_n[ty][tx]; break;
    case TN_DIR_W: slot = &tnw_wall_w[ty][tx]; break;
    case TN_DIR_S: slot = &tnw_wall_n[ny][nx]; break;
    default:       slot = &tnw_wall_w[ny][nx]; break;
    }
    if (*slot == (unsigned char)kind) return true;          // no change, no invalidation
    *slot = (unsigned char)kind;
    if (tnw_on_change) tnw_on_change();
    return true;
}

// ── rooms ───────────────────────────────────────────────────────────────────
int tn_room_at(int tx, int ty) { return tn_in_building(tx, ty) ? tnw_room[ty][tx] : -1; }
int tn_room_count(void)        { return tnw_room_n; }
int tn_shared_room(void)       { return tnw_shared; }

// Which household owns this room, or -1 for communal. Derived from `dwelling`, so there is exactly
// one truth. tnw_seed_x tells housed households from the default-zeroed ones, because `dwelling` is
// an unsigned char and CANNOT hold "nowhere" (see the report at the bottom).
int tn_room_owner(int room) {
    if (room < 0 || room == tnw_shared) return -1;
    for (int h = 0; h < tn_house_n; h++)
        if (tnw_seed_x[h] >= 0 && (int)tn_house[h].dwelling == room) return h;
    return -1;
}
int tn_dwelling_of(int household) {
    if (household < 0 || household >= tn_house_n || tnw_seed_x[household] < 0) return -1;
    return (int)tn_house[household].dwelling;
}

// Which object's footprint ORIGIN is on this tile, or -1. Honest limit: a multi-tile object's OTHER
// tiles are invisible to this, because the contract carries no footprint table (see the report) —
// footprints live only in the art atlas, which this module must not include.
int tn_obj_at(int tx, int ty) {
    for (int o = 0; o < tn_obj_n; o++)
        if (tn_obj[o].tx == (unsigned char)tx && tn_obj[o].ty == (unsigned char)ty) return o;
    return -1;
}

// Re-label every room from the wall grid, then re-derive who lives where. The `build` agent calls
// this after moving a wall; nothing else has to know that ownership just changed.
void tn_rooms_rebuild(void) {
    for (int ty = 0; ty < TN_MH; ty++)
        for (int tx = 0; tx < TN_MW; tx++) tnw_room[ty][tx] = -1;
    tnw_room_n = 0;
    for (int ty = 0; ty < tn_bh; ty++) for (int tx = 0; tx < tn_bw; tx++) {
        if (tnw_room[ty][tx] >= 0 || tnw_room_n >= TN_MAX_ROOMS) continue;
        const int id = tnw_room_n++;
        int head = 0, tail = 0;
        tnw_room[ty][tx] = (signed char)id; tnw_q[tail++] = (short)(ty * TN_MW + tx);
        while (head < tail) {
            const int p = tnw_q[head++], px = p % TN_MW, py = p / TN_MW;
            for (int d = 0; d < 4; d++) {
                const int qx = px + TNW_DX[d], qy = py + TNW_DY[d];
                if (!tn_in_building(qx, qy) || tnw_room[qy][qx] >= 0) continue;
                if (tn_edge_at(px, py, d) != TN_WALL_NONE) continue;   // a DOOR still bounds a room
                tnw_room[qy][qx] = (signed char)id; tnw_q[tail++] = (short)(qy * TN_MW + qx);
            }
        }
    }
    // Ids renumbered, so re-derive identity from the seeds rather than trusting the old numbers.
    tnw_shared = (tnw_shared_x >= 0) ? tn_room_at(tnw_shared_x, tnw_shared_y) : -1;
    for (int h = 0; h < TN_MAX_HOUSEHOLDS; h++) {
        if (tnw_seed_x[h] < 0) continue;
        const int r = tn_room_at(tnw_seed_x[h], tnw_seed_y[h]);
        if (r >= 0) tn_house[h].dwelling = (unsigned char)r;
    }
}

// "Household h lives in the room containing this tile" — the only way ownership is stated, so a
// later wall move cannot silently hand someone else's flat to them.
void tn_room_assign(int household, int tx, int ty) {
    if (household < 0 || household >= TN_MAX_HOUSEHOLDS) return;
    tnw_seed_x[household] = (signed char)tx; tnw_seed_y[household] = (signed char)ty;
    const int r = tn_room_at(tx, ty);
    if (r >= 0) tn_house[household].dwelling = (unsigned char)r;
}
void tn_room_set_shared(int tx, int ty) {
    tnw_shared_x = (signed char)tx; tnw_shared_y = (signed char)ty;
    tnw_shared = tn_room_at(tx, ty);
}

// ── spawning (unchanged: spec() builds its scenarios with these) ─────────────
int tn_now(void) { return tn_clock.day * 1440 + tn_clock.minute; }

int tn_add_obj(int kind, int tx, int ty, int household) {
    if (tn_obj_n >= TN_MAX_OBJECTS) return -1;
    tn_obj[tn_obj_n] = (TnObject){ (unsigned char)kind, (unsigned char)tx, (unsigned char)ty,
                                     0, (signed char)household, 0 };
    return tn_obj_n++;
}
int tn_add_agent(int household, int tx, int ty) {
    if (tn_agent_n >= TN_MAX_AGENTS) return -1;
    TnAgent *a = &tn_agent[tn_agent_n++];
    *a = (TnAgent){0};
    a->species = TN_SPECIES_ADULT; a->household = (unsigned char)household;
    a->tx = (short)tx; a->ty = (short)ty; a->target_obj = -1; a->carrying = -1;
    for (int n = 0; n < TN_NEED_COUNT; n++) a->need[n] = (unsigned char)(150 + 20 * n);
    a->bid_tag = TN_SERVE_COUNT; a->bid_score = 0;
    // Enrol the resident in its household's ROSTER. Found by the `econ` agent: nothing populated
    // `members`/`member_n`, so every household read as empty and anything reasoning about occupancy
    // (rent per head, "is this flat lived in") would have been silently free forever. The roster is
    // maintained here because this is the one place a resident comes into existence.
    // Written as a MOVE, not an append, so the invariant is "an agent is on exactly one roster":
    // spec() builds scenarios by resetting tn_agent_n by hand, which reuses agent indices, and an
    // append-only roster would list index 0 under two households at once.
    const int idx = tn_agent_n - 1;
    for (int h = 0; h < TN_MAX_HOUSEHOLDS; h++) {
        TnHousehold *o = &tn_house[h];
        for (int i = 0; i < o->member_n; i++)
            if (o->members[i] == (unsigned char)idx) {
                for (int j = i; j + 1 < o->member_n; j++) o->members[j] = o->members[j + 1];
                o->member_n--; i--;
            }
    }
    if (household >= 0 && household < TN_MAX_HOUSEHOLDS) {
        TnHousehold *hh = &tn_house[household];
        // members[] is 6 wide; a seventh resident is dropped from the roster rather than written
        // past the end. If households ever grow, that array is the thing to widen.
        if (hh->member_n < (unsigned char)(sizeof hh->members / sizeof hh->members[0]))
            hh->members[hh->member_n++] = (unsigned char)idx;
    }
    return idx;
}

// ── building the level ──────────────────────────────────────────────────────
// A run of segments, so the level reads as walls and doors rather than as array writes.
static void tnw_wall_row(int x0, int x1, int y, int kind) {          // the NORTH edge of (x0..x1, y)
    for (int x = x0; x <= x1; x++) tn_edge_set(x, y, TN_DIR_N, kind);
}
static void tnw_wall_col(int x, int y0, int y1, int kind) {          // the WEST edge of (x, y0..y1)
    for (int y = y0; y <= y1; y++) tn_edge_set(x, y, TN_DIR_W, kind);
}

void tn_world_init(void) {
    tn_obj_n = tn_agent_n = tn_item_n = tn_order_n = 0;
    for (int ty = 0; ty < TN_MH; ty++) for (int tx = 0; tx < TN_MW; tx++)
        tnw_wall_n[ty][tx] = tnw_wall_w[ty][tx] = TN_WALL_NONE;
    if (tnw_on_change) tnw_on_change();      // the bulk clear bypasses tn_edge_set, so say so once
    for (int h = 0; h < TN_MAX_HOUSEHOLDS; h++) { tnw_seed_x[h] = -1; tnw_seed_y[h] = -1; }
    tnw_shared_x = tnw_shared_y = -1;

    tn_house_n = 4;
    for (int h = 0; h < tn_house_n; h++) tn_house[h] = (TnHousehold){ 200, {0}, 0, 20, 0 };
    // Rosters beyond tn_house_n too: a spec scenario may have parked a member on household 5, and a
    // stale roster is exactly the "occupancy that looks like it works" trap econ flagged.
    for (int h = tn_house_n; h < TN_MAX_HOUSEHOLDS; h++) tn_house[h].member_n = 0;

    // ── the structure. Only interior segments: the outer shell is implicit. ──
    tnw_wall_col(6, 0, 2, TN_WALL_SOLID);                 // A │ B
    tnw_wall_col(6, 5, 8, TN_WALL_SOLID);                 // C │ D
    tnw_wall_row(0, 12, 3, TN_WALL_SOLID);                // the north flats' party wall with the hall
    tnw_wall_row(0, 12, 5, TN_WALL_SOLID);                // the south flats'
    tn_edge_set(2, 3, TN_DIR_N, TN_WALL_DOOR);            // flat A's door
    tn_edge_set(9, 3, TN_DIR_N, TN_WALL_DOOR);            // flat B's
    tn_edge_set(2, 5, TN_DIR_N, TN_WALL_DOOR);            // flat C's
    tn_edge_set(9, 5, TN_DIR_N, TN_WALL_DOOR);            // flat D's
    // The shared WC: two tiles carved out of the hall at (6,3)-(7,3), one door onto the corridor.
    // Two tiles, not one, so there is somewhere to stand that is not on top of the toilet.
    tnw_wall_col(6, 3, 3, TN_WALL_SOLID);
    tnw_wall_col(8, 3, 3, TN_WALL_SOLID);
    tn_edge_set(6, 4, TN_DIR_N, TN_WALL_SOLID);
    tn_edge_set(7, 4, TN_DIR_N, TN_WALL_DOOR);

    tn_rooms_rebuild();
    tn_room_set_shared(6, 4);                             // the corridor: the room everyone crosses
    tn_room_assign(0, 3, 1); tn_room_assign(1, 9, 1);     // who lives where, by a tile in the room
    tn_room_assign(2, 3, 6); tn_room_assign(3, 9, 6);

    // ── the furniture. Owned per household (design §6), communal marked -1. ──
    // Every non-square object keeps facing 0 (tn_add_obj's default): a bed is 1 wide and 2 DEEP, a
    // sofa 2 wide and 1 deep, and iso-rooms.md §8 is explicit that turning one moves its art but not
    // its footprint. Nothing here is turned, so the tiles listed in the map above are the truth.
    tn_add_obj(TN_OBJ_BED,      0, 0, 0);   // FLAT A — the one flat with its own sofa
    tn_add_obj(TN_OBJ_FRIDGE,   1, 0, 0);
    tn_add_obj(TN_OBJ_WARDROBE, 2, 0, 0);
    tn_add_obj(TN_OBJ_SOFA,     4, 0, 0);
    tn_add_obj(TN_OBJ_BED,      6, 0, 1);   // FLAT B
    tn_add_obj(TN_OBJ_FRIDGE,   7, 0, 1);
    tn_add_obj(TN_OBJ_WARDROBE,12, 0, 1);
    // The gap beside each south bed is deliberate: the bed spills DOWN into it, and a fridge in it
    // would pocket the corner off for any walker that treats furniture as solid. The assertion below
    // found that, in exactly this spot, twice.
    tn_add_obj(TN_OBJ_BED,      0, 7, 2);   // FLAT C
    tn_add_obj(TN_OBJ_FRIDGE,   2, 8, 2);
    tn_add_obj(TN_OBJ_WARDROBE, 5, 8, 2);
    tn_add_obj(TN_OBJ_BED,      6, 7, 3);   // FLAT D
    tn_add_obj(TN_OBJ_FRIDGE,   8, 8, 3);
    tn_add_obj(TN_OBJ_WARDROBE,12, 8, 3);
    tn_add_obj(TN_OBJ_LOOM,     4, 3, -1);  // THE HALL — one machine for the whole building
    tn_add_obj(TN_OBJ_SOFA,    11, 3, -1);  // the communal sofa: capacity 2, three households
    tn_add_obj(TN_OBJ_TOILET,   6, 3, -1);  // THE WC — one toilet, four households

    tn_add_agent(0, 3, 1); tn_add_agent(1, 9, 1);
    tn_add_agent(2, 3, 6); tn_add_agent(3, 9, 6);
    tn_clock = (TnClock){ 8 * 60, 1 };
}

// ─────────────────────────────────────────────────────────────────────────────
// WHAT THIS MODULE NEEDS FROM THE CONTRACT (reported, not smuggled in)
//
// 1. THERE WERE BRIEFLY TWO WALL MODELS, AND EDGES WON — recorded because the reasoning is the
//    interesting part and because it fixes an INCLUDE ORDER. `path` had landed a TILE wall layer of
//    its own (`tnp_wall[TN_N]`, a whole tile solid) with `tn_wall_at(x,y)`/`tn_wall_set(x,y,solid)`,
//    which collided with these names at different signatures — the cart would not have compiled.
//    It has since dropped that layer and now reads tn_can_step() (cached, one rebuild per wall
//    edit) and registers tn_walls_on_change(tn_path_dirty). The case that settled it:
//      • THE RENDERER ALREADY DOES EDGES. isoroom.c — the shipped probe this game inherits its
//        geometry from — pushes `wall_ns` on the north/south edges and `wall_ew` on the east/west
//        ones, offset OUTSIDE the floor by the model's own thickness ("Walls sit ENTIRELY OUTSIDE
//        the floor"). A tile-wall has no orientation to read, so art would have to INFER one from
//        its neighbours and guess at every corner.
//      • FLOOR IS THE SCARCE GOOD, which is the whole design (§1). These partitions would cost ~33
//        of 117 tiles as tile-walls: a quarter of the building spent on structure, in a game about
//        not having enough room.
//      • A DOORWAY IS FREE on an edge (one segment set to _DOOR). As tiles it is a GAP, so a room
//        cannot be distinguished from a corridor by its walls at all, and "is this dwelling actually
//        enclosed" — the assertion below, and the whole point of rooms — has no clean definition.
//      • IT MAKES iso-rooms.md §8's FOOTPRINT BUG UNREPRESENTABLE rather than merely avoided: a wall
//        never carries a `facing`, so its art and its extent cannot disagree.
//    CONSEQUENCE FOR INTEGRATION: world.h must be included BEFORE path.h (it already is first), and
//    `path` must be included before `offer` if offer's travel cost is to become a real path.
//
// 2. THE WALL VOCABULARY BELONGS IN model.h, whichever model wins. `art` and `path` both read it,
//    which is the contract's own test for shared state. Move up: TnWallKind, the TN_DIR_* enum,
//    TN_MAX_ROOMS, and the prototypes for tn_in_building / tn_edge_at / tn_can_step / tn_edge_set /
//    tn_walls_on_change / tn_room_at / tn_room_count / tn_room_owner / tn_dwelling_of /
//    tn_shared_room / tn_obj_at / tn_rooms_rebuild / tn_room_assign / tn_room_set_shared. Then
//    define TENEMENT_WALLS_IN_CONTRACT there and the guarded block at the top of this file goes
//    quiet. The GRID ITSELF should stay private here: the canonical north/west storage is a detail,
//    and a module reading it raw would forget that east is the neighbour's west.
//
// 3. THE FOOTPRINT TABLE IS MISSING, AND IT IS GAMEPLAY DATA. How many tiles an object occupies
//    lives ONLY in ISO_FOOTPRINT in the generated art atlas, which only `art` may include. So
//    `path` treats every object as one tile, `build` had to hand-copy the table (tnb_foot, and it
//    says so), and tn_obj_at() above can only answer for footprint ORIGINS. THREE modules now want
//    it, which is the contract's own bar. Wanted, beside TN_OFFERS and TN_OBJ_PRICE in the offer
//    module (a table row, not a code path):
//        extern const unsigned char TN_OBJ_FOOTPRINT[TN_OBJ_KIND_COUNT][2];   // in TILES
//    Values, read off the atlas: bed 1x2, sofa 2x1, everything else 1x1 — identical to build.h's
//    copy, which is the drift waiting to happen. The layout above survives EITHER reading: no free
//    tile is sealed off whether a walker treats an object as one tile or as its whole footprint.
//    That is care, not a guarantee, and the assertion below can only check the origin version.
//
// 4. `TnHousehold.dwelling` SHOULD BE `TnIdx` (or at least signed). It is `unsigned char`, so "this
//    household has no dwelling" is unrepresentable and a default-zeroed household silently claims
//    room 0 — the same class of bug as the TnIdx fix, one field it did not reach. This module works
//    around it with a private seed array; the fix is one word.
//
// 5. NOTHING IN THE OFFER TABLE SERVES TN_SERVE_HYGIENE, so that need decays to zero forever and no
//    object ever bids for it: one of the game's five needs is currently unserveable. Not this
//    module's table to edit — but the shared WC above is exactly where a washbasin goes, and per the
//    contract that is one row in TN_OFFERS plus one TnObjKind, no code path.
//
// 6. THE MODULE TAG TABLE in model.h does not list `build` → `tnb_` or `path` → `tnp_` either.
//
// 7. NOT ASKED FOR, DELIBERATELY: a `blocks` flag on objects. Whether furniture stops a walker is
//    `path`'s policy, and either answer works against the layout above (see 3).
// ─────────────────────────────────────────────────────────────────────────────

#ifdef DE_SPEC
// ── the layout's own oracle. The claim being tested is not "walls exist", it is that this building
// WORKS: every room enclosed, every room reachable, nothing sealed, and the scarcities that are
// supposed to cause contention actually scarce. A layout that claims four flats and leaves one
// walled shut is exactly the bug an eye should not have to catch.
// The cart's spec() calls tn_world_selfcheck(); wired at integration.
static unsigned char tnw_seen[TN_MH][TN_MW];

// Flood fill from (sx,sy) through doors. `block_objects` also treats a tile holding an object's
// footprint origin as impassable — the strong version, which catches a fridge parked in the only
// doorway. Returns how many tiles were reached.
static int tnw_reach(int sx, int sy, int block_objects) {
    for (int ty = 0; ty < TN_MH; ty++) for (int tx = 0; tx < TN_MW; tx++) tnw_seen[ty][tx] = 0;
    if (!tn_in_building(sx, sy)) return 0;
    int head = 0, tail = 0;
    tnw_seen[sy][sx] = 1; tnw_q[tail++] = (short)(sy * TN_MW + sx);
    while (head < tail) {
        const int p = tnw_q[head++], px = p % TN_MW, py = p / TN_MW;
        for (int d = 0; d < 4; d++) {
            const int qx = px + TNW_DX[d], qy = py + TNW_DY[d];
            if (!tn_can_step(px, py, d) || tnw_seen[qy][qx]) continue;
            if (block_objects && tn_obj_at(qx, qy) >= 0) continue;
            tnw_seen[qy][qx] = 1; tnw_q[tail++] = (short)(qy * TN_MW + qx);
        }
    }
    return tail;
}

void tn_world_selfcheck(void) {
    static char m[160];
    tn_world_init();
    const int hall = tn_shared_room();

    // ── the shape of the building ────────────────────────────────────────────
    snprintf(m, sizeof m, "world: the level is 6 rooms — four flats, a hall and a shared WC (got %d)",
             tn_room_count());
    expect(tn_room_count() == 6, m);
    expect(hall >= 0 && tn_room_owner(hall) == -1,
           "world: the hall is a real room and nobody owns it");
    {
        int bad = 0, clash = 0;
        for (int h = 0; h < tn_house_n; h++) {
            const int r = tn_dwelling_of(h);
            if (r < 0 || r == hall || tn_room_owner(r) != h) bad++;
            for (int g = 0; g < h; g++) if (tn_dwelling_of(g) == r) clash++;
        }
        expect(tn_house_n == 4 && bad == 0,
               "world: all four households have a dwelling that is a room, is not the hall, and "
               "reports them back as its owner");
        expect(clash == 0, "world: no two households were handed the same flat");
    }

    // ── ENCLOSURE + ACCESS: a star of private rooms around one shared space ──
    // For every room but the hall: its boundary is fully walled, exactly ONE segment of it is a
    // door, and that door opens onto the hall. One door each is also the contention lever.
    {
        int leaky = 0, wrong_doors = 0, not_off_hall = 0;
        for (int r = 0; r < tn_room_count(); r++) {
            if (r == hall) continue;
            int doors = 0, leaks = 0, onto_hall = 0;
            for (int ty = 0; ty < tn_bh; ty++) for (int tx = 0; tx < tn_bw; tx++) {
                if (tn_room_at(tx, ty) != r) continue;
                for (int d = 0; d < 4; d++) {
                    const int nx = tx + TNW_DX[d], ny = ty + TNW_DY[d];
                    if (tn_room_at(nx, ny) == r) continue;         // an interior side
                    const int k = tn_edge_at(tx, ty, d);
                    if (k == TN_WALL_NONE) leaks++;
                    else if (k == TN_WALL_DOOR) { doors++; if (tn_room_at(nx, ny) == hall) onto_hall++; }
                }
            }
            if (leaks) leaky++;
            if (doors != 1) wrong_doors++;
            if (onto_hall != 1) not_off_hall++;
        }
        expect(leaky == 0, "world: every room is ENCLOSED — no boundary tile leaks into the next room");
        expect(wrong_doors == 0, "world: every room has exactly ONE door, so all its traffic funnels");
        expect(not_off_hall == 0,
               "world: every door opens onto the HALL — no flat is reached through a neighbour's");
    }
    {
        int out = 0;
        for (int ty = 0; ty < tn_bh; ty++) {
            if (tn_can_step(0, ty, TN_DIR_W)) out++;
            if (tn_can_step(tn_bw - 1, ty, TN_DIR_E)) out++;
        }
        for (int tx = 0; tx < tn_bw; tx++) {
            if (tn_can_step(tx, 0, TN_DIR_N)) out++;
            if (tn_can_step(tx, tn_bh - 1, TN_DIR_S)) out++;
        }
        expect(out == 0, "world: the outer shell is solid on all four sides, and it is never stored");
    }

    // ── REACHABILITY, by flood fill. The assertion the eye should not have to make. ──
    {
        const int tiles = tn_bw * tn_bh, got = tnw_reach(6, 4, 0);
        snprintf(m, sizeof m, "world: from the hall every one of the %d tiles is reachable through "
                              "doors (got %d) — no flat is sealed", tiles, got);
        expect(got == tiles, m);
    }
    {
        // The strong version: furniture blocks its own tile. Catches a fridge in the one doorway,
        // and a corner pocketed off behind a bed.
        int occupied = 0;
        for (int ty = 0; ty < tn_bh; ty++) for (int tx = 0; tx < tn_bw; tx++)
            if (tn_obj_at(tx, ty) >= 0) occupied++;
        const int want = tn_bw * tn_bh - occupied, got = tnw_reach(6, 4, 1);
        snprintf(m, sizeof m, "world: still reachable with FURNITURE SOLID — %d free tiles, %d reached",
                 want, got);
        expect(got == want, m);
        int stranded = 0;
        for (int o = 0; o < tn_obj_n; o++) {
            int ok = 0;
            for (int d = 0; d < 4; d++) {
                const int nx = tn_obj[o].tx + TNW_DX[d], ny = tn_obj[o].ty + TNW_DY[d];
                if (tn_can_step(tn_obj[o].tx, tn_obj[o].ty, d) && tnw_seen[ny][nx]) ok = 1;
            }
            if (!ok) stranded++;
        }
        expect(stranded == 0,
               "world: every object has a reachable tile to stand at — nothing is walled in with itself");
    }
    {
        int blocked = 0;
        for (int ty = 0; ty < tn_bh; ty++) for (int tx = 0; tx < tn_bw; tx++)
            for (int i = 0; i < 2; i++) {
                const int d = i ? TN_DIR_W : TN_DIR_N;       // each segment is stored once, N or W
                if (tn_edge_at(tx, ty, d) != TN_WALL_DOOR) continue;
                if (tn_obj_at(tx, ty) >= 0 ||
                    tn_obj_at(tx + TNW_DX[d], ty + TNW_DY[d]) >= 0) blocked++;
            }
        expect(blocked == 0, "world: no doorway has furniture standing in it, on either side");
    }

    // ── OWNERSHIP HAS A PLACE: people and things are where their household is ──
    {
        int wrong_agent = 0, wrong_obj = 0;
        for (int i = 0; i < tn_agent_n; i++)
            if (tn_room_at(tn_agent[i].tx, tn_agent[i].ty) != tn_dwelling_of(tn_agent[i].household))
                wrong_agent++;
        for (int o = 0; o < tn_obj_n; o++) {
            const int r = tn_room_at(tn_obj[o].tx, tn_obj[o].ty);
            if (tn_obj[o].household >= 0) { if (r != tn_dwelling_of(tn_obj[o].household)) wrong_obj++; }
            else if (tn_room_owner(r) != -1) wrong_obj++;      // a communal thing in a private room
        }
        expect(wrong_agent == 0, "world: every resident starts inside their OWN flat");
        // The roster, both directions. It can fail two ways and both are real: a missing enrolment
        // (occupancy reads zero and rent is free) and a double enrolment (a resident counted twice).
        int listed = 0, dupes = 0, mismatched = 0;
        for (int h = 0; h < TN_MAX_HOUSEHOLDS; h++)
            for (int i = 0; i < tn_house[h].member_n; i++) {
                const int a = tn_house[h].members[i];
                listed++;
                if (a >= tn_agent_n || tn_agent[a].household != (unsigned char)h) mismatched++;
                for (int g = 0; g < TN_MAX_HOUSEHOLDS; g++)
                    for (int j = 0; j < tn_house[g].member_n; j++)
                        if ((g != h || j != i) && tn_house[g].members[j] == (unsigned char)a) dupes++;
            }
        snprintf(m, sizeof m, "world: every resident is on exactly ONE household roster — %d agents, "
                              "%d enrolments, %d duplicates", tn_agent_n, listed, dupes);
        expect(listed == tn_agent_n && dupes == 0 && mismatched == 0, m);
        expect(wrong_obj == 0,
               "world: owned furniture stands in its owner's flat, and communal furniture in a "
               "room nobody owns — `dwelling` now means something spatial");
    }

    // ── THE CONTENTION CLAIM, counted through TAGS (never through an object kind) ──
    {
        int bladder = 0, communal_bladder = 0, work = 0, communal_work = 0, rest = 0, private_rest = 0;
        for (int o = 0; o < tn_obj_n; o++) {
            if (tn_offers(o, TN_SERVE_BLADDER, NULL)) { bladder++; if (tn_obj[o].household < 0) communal_bladder++; }
            if (tn_offers(o, TN_CAP_WORK, NULL))      { work++;    if (tn_obj[o].household < 0) communal_work++; }
            if (tn_offers(o, TN_SERVE_REST, NULL))    { rest++;    if (tn_obj[o].household >= 0) private_rest++; }
        }
        snprintf(m, sizeof m, "world: ONE toilet, communal, for %d households — the queue is "
                              "structural, not incidental (%d found)", tn_house_n, bladder);
        expect(bladder == 1 && communal_bladder == 1 && tn_house_n > bladder, m);
        expect(work == 1 && communal_work == 1,
               "world: ONE machine in the building and it belongs to nobody, so a shift is a race");
        expect(rest == tn_house_n && private_rest == rest,
               "world: sleep is the one thing NOT contended — a bed each, all privately owned");
    }

    tn_world_init();   // leave the world as the cart expects to find it
}
#endif

#endif // TENEMENT_WORLD_H
