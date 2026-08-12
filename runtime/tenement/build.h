// ─────────────────────────────────────────────────────────────────────────────
// tenement/build.h — BUILD MODE. THE PLAYER'S VERB, and the only one there is.
//
// design §1: you shape SPACE. You never touch a person. You pick a tile, you pay for a thing, you
// put it there, and the simulation reports whether your building works. Every bad outcome has to be
// legibly your fault, which is why this module refuses illegal placements LOUDLY (a coloured ghost
// and a named reason) instead of silently snapping to somewhere it likes better.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by the
// cart before this file) plus engine headers. NEVER include a sibling module. Every static in here
// is prefixed tnb_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
//
// ── THE THREE THINGS TO KNOW BEFORE EDITING ─────────────────────────────────
//
// 1. PICKING IS AN EXACT INVERSE, NOT A SEARCH. tnb_unproject() is the algebraic inverse of
//    tnb_project(), copied from tools/carts/isoroom.c where 4 of its 33 spec assertions pin it.
//    It is exact at all four rotations, which is the only reason a click lands on the tile you are
//    pointing at. Do not replace it with "loop over tiles and test the diamond": that is slower,
//    and its failure mode is a 1px sliver between tiles that nobody notices for a month.
//    This is the SECOND consumer of that projection (art.h is the first). Per ADR-0006 a shared
//    runtime/isoview.h is now justified — see the note at the bottom of this file.
//
// 2. FACING IS NOT EXPOSED TO THE PLAYER, ON PURPOSE. iso-rooms.md §8: "a turned non-square
//    object's footprint does not follow its art." The MODEL half of that is solved here (an odd
//    facing swaps the footprint's w/h, and the spec pins it), but the ART half is not: art.h still
//    sorts depth and draws the contact shadow from the UNROTATED footprint. So the placement code
//    is rotation-correct and rotation is simply not offered until art.h turns its footprint too.
//    Shipping a rotate key today would put a sofa's art and its claimed tiles in different places,
//    which is exactly the silent wrongness the design doc warns about.
//
// 3. LEGALITY AND AFFORDABILITY ARE SEPARATE QUESTIONS. tn_build_legal() answers "does this fit in
//    the world" (geometry only, no money, no purse, usable by anything that places objects).
//    tn_build_check() adds "can this household pay for it, and is there an object slot left". The
//    cursor colours them differently because "you can't put it there" and "you can't afford it" are
//    different sentences to a player.
//
// NOTHING HERE SWITCHES ON AN OBJECT KIND FOR BEHAVIOUR (contract rule 2). `kind` is used to read
// three data tables: the footprint, the height, and the name. All three are art/geometry, which is
// the same thing art.h uses OBJ_CELL for. Adding an object is a row in each table.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_BUILD_H
#define TENEMENT_BUILD_H

// Picking needs the camera offset, and the contract does not expose it: art.h owns `cam_x/cam_y` as
// file-scope statics. This is not an include of a sibling (see the rules) — it is an assertion that
// the cart already included one, which the contract's own dependency order guarantees. If this
// fires, tenement.c is including build.h too early. Flagged in the handover: the contract wants
// `extern float tn_cam_x, tn_cam_y;` so this module can stop depending on include ORDER.
#ifndef TENEMENT_ART_H
#  error "include tenement/build.h AFTER tenement/art.h — picking needs art's camera offset"
#endif

// The contract mirrors atlas.h's geometry (TN_TW/TN_TH vs ISO_TW/ISO_TH). If they ever disagree,
// picking silently lands on the wrong tile while the render stays correct, which is the worst
// possible bug shape. art.h has already pulled atlas.h in, so pin them together for free.
#ifdef ISO_TW
_Static_assert(TN_TW == ISO_TW && TN_TH == ISO_TH,
               "the contract's tile geometry has drifted from the baked atlas — picking would lie");
#endif

// ── verdicts ────────────────────────────────────────────────────────────────
// A named reason, not a bool, because the cursor has to SAY why. design §1: the player's fault has
// to be legible, and "nothing happened when I clicked" is the opposite of legible.
typedef enum {
    TN_BUILD_OK = 0,
    TN_BUILD_OFF_GRID,      // outside tn_bw / tn_bh (or the footprint hangs off the edge)
    TN_BUILD_OCCUPIED,      // another object's footprint is already there
    TN_BUILD_BLOCKED,       // a wall / permanently unbuildable tile — see tn_build_set_blocked
    TN_BUILD_NO_MONEY,      // the household purse cannot cover TN_OBJ_PRICE
    TN_BUILD_NO_ROOM        // TN_MAX_OBJECTS reached
} TnBuildVerdict;

// ── data tables: footprint, height, name. ART AND GEOMETRY, never behaviour. ─
// Footprint is in TILES at facing 0. Derived from atlas.h's ISO_FOOTPRINT (which is in voxels, 6 to
// a tile) but NOT read from it, because only the art module may include the generated atlas. That
// duplication is real and is the first item in the handover: the contract should carry the footprint,
// since it is a fact about the SIM (which tiles are claimed), not about the picture.
static const unsigned char tnb_foot[TN_OBJ_KIND_COUNT][2] = {
    [TN_OBJ_BED]      = {1, 2},     // ISO_FOOTPRINT {6,12}
    [TN_OBJ_FRIDGE]   = {1, 1},     // {6,6}
    [TN_OBJ_COUNTER]  = {1, 1},     // {6,6}
    [TN_OBJ_TOILET]   = {1, 1},     // {6,6}
    [TN_OBJ_SOFA]     = {2, 1},     // {12,6}
    [TN_OBJ_LOOM]     = {1, 1},     // {6,4}
    [TN_OBJ_WARDROBE] = {1, 1},     // {6,4}
};
// Height in VOXELS, for the ghost's wireframe box. iso-rooms.md §8: at this scale objects read by
// silhouette and by HEIGHT, so a ghost that is only a flat diamond does not tell you whether you
// are about to put a wardrobe or a toilet against the window.
static const unsigned char tnb_high[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = 4, [TN_OBJ_FRIDGE] = 12, [TN_OBJ_COUNTER] = 7, [TN_OBJ_TOILET] = 6,
    [TN_OBJ_SOFA] = 6, [TN_OBJ_LOOM] = 12, [TN_OBJ_WARDROBE] = 10,
};
static const char *tnb_name[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED]="BED", [TN_OBJ_FRIDGE]="FRIDGE", [TN_OBJ_COUNTER]="COUNTER",
    [TN_OBJ_TOILET]="TOILET", [TN_OBJ_SOFA]="SOFA", [TN_OBJ_LOOM]="LOOM",
    [TN_OBJ_WARDROBE]="WARDROBE",
};

// ── state (owner: build) ────────────────────────────────────────────────────
int tn_build_kind = TN_OBJ_BED;     // what the cursor is holding
int tn_build_hh   = 0;              // whose purse pays. NOT in the contract — see the handover.
static int tnb_on = 0;              // build mode vs live mode
static int tnb_tx = -1, tnb_ty = -1;// the picked tile, or -1,-1 for "off the floor"
static int tnb_facing = 0;          // always 0 in v1; see THING TO KNOW #2
static int tnb_placed = 0, tnb_spent = 0;   // session totals, for the HUD if it wants them
static bool (*tnb_blocked)(int tx, int ty) = NULL;

// ── projection: the SECOND consumer of isoroom's iso_project/iso_unproject ──
// tnb_turn is byte-for-byte art.h's tnr_iso_turn (and isoroom's iso_turn, and voxel-bake.js's
// projector()); tnb_unturn is its inverse and is the piece art.h does not have, because drawing
// only ever goes forwards. Copied rather than re-derived: this maths has a 4-assertion oracle in
// isoroom's spec() and re-deriving it is how you get picking that works at rotation 0 only.
static void tnb_turn(int q, float x, float y, float *X, float *Y) {
    switch (q & 3) { case 0: *X= x; *Y= y; break; case 1: *X=-y; *Y= x; break;
                     case 2: *X=-x; *Y=-y; break; default: *X= y; *Y=-x; break; }
}
static void tnb_unturn(int q, float X, float Y, float *x, float *y) {
    switch (q & 3) { case 0: *x= X; *y= Y; break; case 1: *x= Y; *y=-X; break;
                     case 2: *x=-X; *y=-Y; break; default: *x=-Y; *y= X; break; }
}
// voxel space → screen offset, BEFORE the camera shift.
static void tnb_project(int r, float vx, float vy, float vz, float *sx, float *sy) {
    float X, Y; tnb_turn(r, vx, vy, &X, &Y);
    *sx = (X - Y) * (TN_TW * 0.5f);
    *sy = (X + Y) * (TN_TH * 0.5f) - vz * TN_ZH;
}
// screen offset → the point on the FLOOR (z = 0) under it. The exact inverse of the above.
static void tnb_unproject(int r, float sx, float sy, float *vx, float *vy) {
    const float d = sx / (TN_TW * 0.5f);            // X - Y
    const float s = sy / (TN_TH * 0.5f);            // X + Y
    const float X = (s + d) * 0.5f, Y = (s - d) * 0.5f;
    tnb_unturn(r, X, Y, vx, vy);
}

// ── public: picking ─────────────────────────────────────────────────────────
// Screen pixel → tile. Takes coordinates rather than reading the mouse so it is testable and so a
// touch/gamepad cursor can use it too. Returns false and leaves *tx/*ty alone when the point is off
// the building. Exact at all four rotations (tn_build_selfcheck asserts every tile, every rotation).
bool tn_build_pick(int sx, int sy, int *tx, int *ty) {
    float vx, vy;
    tnb_unproject(tn_rot, (float)sx - cam_x, (float)sy - cam_y, &vx, &vy);
    const int px = (int)floorf(vx / TN_TILE_VOX), py = (int)floorf(vy / TN_TILE_VOX);
    if (px < 0 || py < 0 || px >= tn_bw || py >= tn_bh) return false;
    if (tx) *tx = px;
    if (ty) *ty = py;
    return true;
}

// Where the ghost is. Set by tn_build_input() from the mouse, but public because the mouse is not
// the only pointer: a touch drag, a gamepad cursor, or a headless screenshot all need to say "hover
// here" without one. -1,-1 means off the floor and the cursor says so rather than guessing.
void tn_build_hover(int tx, int ty) { tnb_tx = tx; tnb_ty = ty; }
int  tn_build_hover_x(void) { return tnb_tx; }
int  tn_build_hover_y(void) { return tnb_ty; }

// The footprint in TILES for a kind at a facing. An odd quarter-turn swaps w/h — the model half of
// iso-rooms.md §8's unsolved problem, solved. (The art half is not; see THING TO KNOW #2.)
void tn_build_footprint(int kind, int facing, int *w, int *h) {
    const int k = (kind < 0 || kind >= TN_OBJ_KIND_COUNT) ? 0 : kind;
    const int a = tnb_foot[k][0], b = tnb_foot[k][1];
    if (facing & 1) { if (w) *w = b; if (h) *h = a; }
    else            { if (w) *w = a; if (h) *h = b; }
}

// ── public: legality (geometry only — no money, no purse) ────────────────────
// Anything that puts an object into the world should come through here, including the world module's
// own level layout, so "the level is legal" and "what the player builds is legal" are one rule.
int tn_build_legal(int kind, int tx, int ty, int facing) {
    int w, h; tn_build_footprint(kind, facing, &w, &h);
    if (tx < 0 || ty < 0 || tx + w > tn_bw || ty + h > tn_bh) return TN_BUILD_OFF_GRID;
    if (tnb_blocked)
        for (int y = ty; y < ty + h; y++) for (int x = tx; x < tx + w; x++)
            if (tnb_blocked(x, y)) return TN_BUILD_BLOCKED;
    for (int o = 0; o < tn_obj_n; o++) {
        int ow, oh; tn_build_footprint(tn_obj[o].kind, tn_obj[o].facing, &ow, &oh);
        const int ox = tn_obj[o].tx, oy = tn_obj[o].ty;
        if (tx < ox + ow && ox < tx + w && ty < oy + oh && oy < ty + h) return TN_BUILD_OCCUPIED;
    }
    return TN_BUILD_OK;
}

// Legality PLUS "can this household afford it and is there a slot free". Pure: safe to call every
// frame from the cursor.
int tn_build_check(int kind, int tx, int ty, int facing, int household) {
    const int v = tn_build_legal(kind, tx, ty, facing);
    if (v != TN_BUILD_OK) return v;
    if (tn_obj_n >= TN_MAX_OBJECTS) return TN_BUILD_NO_ROOM;
    // Affordability is ECON's rule, not ours (see the note on tn_build_place). Asking it here rather
    // than comparing tn_house[].money to TN_OBJ_PRICE ourselves means there is one answer to "can
    // they afford it", so the greyed-out cursor and the refused purchase can never disagree.
    if (!tn_can_afford(household, kind)) return TN_BUILD_NO_MONEY;
    return TN_BUILD_OK;
}

const char *tn_build_why(int verdict) {
    switch (verdict) {
        case TN_BUILD_OK:       return "ok";
        case TN_BUILD_OFF_GRID: return "off the floor";
        case TN_BUILD_OCCUPIED: return "in the way";
        case TN_BUILD_BLOCKED:  return "on a wall";
        case TN_BUILD_NO_MONEY: return "cannot afford";
        default:                return "no room";
    }
}

// A wall / unbuildable-tile predicate, registered by whoever owns walls. NOTHING owns them yet: the
// contract has no wall data at all (atlas.h bakes four wall MODELS but the sim has no array of them),
// so the default is "no tile is blocked" and TN_BUILD_BLOCKED is unreachable in the shipped cart.
// Wired as a predicate rather than an #if so the day walls land it is one call and no edit here.
// Second item in the handover.
void tn_build_set_blocked(bool (*fn)(int tx, int ty)) { tnb_blocked = fn; }

// ── public: the purchase ────────────────────────────────────────────────────
// Returns the new object index, or the NEGATED verdict (so a caller can print why).
//
// THIS MODULE DOES NOT TOUCH MONEY. econ.h owns the purse and states that `tn_buy_obj` is "the seam
// a future build module calls", so this is that call. Deliberately NOT a local
// `tn_house[h].money -= price`, even though that is one line and would compile: econ funnels every
// debit through its own tne_charge so its money-destroyed ledger stays conserved, and a second debit
// path would make that ledger quietly wrong — the kind of bug that shows up as an economy that does
// not balance three weeks later. Division of labour: we own WHERE it goes and WHETHER it is legal,
// econ owns WHAT IT COSTS and WHO PAYS.
//
// If this stops compiling, econ's purchase surface has moved. Follow it there. Do not re-inline the
// debit.
int tn_build_place(int kind, int tx, int ty, int facing, int household) {
    const int v = tn_build_check(kind, tx, ty, facing, household);
    if (v != TN_BUILD_OK) return -v;
    const int o = tn_buy_obj(household, kind, tx, ty);  // places AND charges, or -1 charging nothing
    if (o < 0) return -TN_BUILD_NO_ROOM;               // belt and braces; check() already tested it
    tn_obj[o].facing = (unsigned char)(facing & 3);
    tnb_placed++; tnb_spent += TN_OBJ_PRICE[kind];     // == what was charged, since check() passed
    return o;
}

// ── public: growing the building ────────────────────────────────────────────
// tn_bw/tn_bh are variables rather than #defines precisely so the player can grow the place (the
// contract says so). Clamped to the array bounds, and it REFUSES a shrink that would leave an object
// outside the building — an orphaned object would keep bidding from a tile that no longer exists.
// Returns true if the extent actually changed.
bool tn_build_grow(int dw, int dh) {
    int nw = tn_bw + dw, nh = tn_bh + dh;
    if (nw < 1) nw = 1; if (nw > TN_MW) nw = TN_MW;
    if (nh < 1) nh = 1; if (nh > TN_MH) nh = TN_MH;
    for (int o = 0; o < tn_obj_n; o++) {
        int ow, oh; tn_build_footprint(tn_obj[o].kind, tn_obj[o].facing, &ow, &oh);
        if (tn_obj[o].tx + ow > nw || tn_obj[o].ty + oh > nh) return false;
    }
    if (nw == tn_bw && nh == tn_bh) return false;
    tn_bw = nw; tn_bh = nh;
    return true;
}

// ── public: the mode ────────────────────────────────────────────────────────
void tn_build_toggle(void) { tnb_on = !tnb_on; if (!tnb_on) tnb_tx = tnb_ty = -1; }
bool tn_build_active(void) { return tnb_on != 0; }
// Session totals, for a HUD that wants to show what this building cost to furnish.
int  tn_build_spent(void)  { return tnb_spent; }
int  tn_build_placed(void) { return tnb_placed; }

// ── public: input. Call ONCE per frame from update(), BEFORE the sim ticks. ──
//   B          toggle build / live          (live mode reads nothing else)
//   [ ]  wheel cycle what you are holding
//   left click place it
//   right click back to live mode
//   G H        grow the building by one tile across / deep
// The sim keeps running in build mode on purpose: design §1 is hands-off, and watching the queue at
// the toilet while you decide where the second one goes IS the game.
void tn_build_input(void) {
    if (keyp('B')) tn_build_toggle();
    if (!tnb_on) return;

    if (keyp(']')) tn_build_kind = (tn_build_kind + 1) % TN_OBJ_KIND_COUNT;
    if (keyp('[')) tn_build_kind = (tn_build_kind + TN_OBJ_KIND_COUNT - 1) % TN_OBJ_KIND_COUNT;
    const float wheel = mouse_wheel();
    if (wheel > 0.0f) tn_build_kind = (tn_build_kind + 1) % TN_OBJ_KIND_COUNT;
    if (wheel < 0.0f) tn_build_kind = (tn_build_kind + TN_OBJ_KIND_COUNT - 1) % TN_OBJ_KIND_COUNT;
    if (keyp('G')) tn_build_grow(1, 0);
    if (keyp('H')) tn_build_grow(0, 1);

    if (!tn_build_pick(mouse_x(), mouse_y(), &tnb_tx, &tnb_ty)) { tnb_tx = tnb_ty = -1; }
    if (mouse_pressed(1)) { tn_build_toggle(); return; }
    if (mouse_pressed(0) && tnb_tx >= 0)
        tn_build_place(tn_build_kind, tnb_tx, tnb_ty, tnb_facing, tn_build_hh);
}

// ── public: the cursor. Call from draw() AFTER tn_draw_world(), BEFORE tn_draw_hud() ──
// It draws on top of the world (so the ghost is not buried under furniture) and under the HUD (so it
// never covers the bid readout, which is the one thing the player is reading while they build).
static void tnb_quad(float x0, float y0, float x1, float y1, int color) {
    float q[4][2];
    tnb_project(tn_rot, x0, y0, 0, &q[0][0], &q[0][1]);
    tnb_project(tn_rot, x1, y0, 0, &q[1][0], &q[1][1]);
    tnb_project(tn_rot, x1, y1, 0, &q[2][0], &q[2][1]);
    tnb_project(tn_rot, x0, y1, 0, &q[3][0], &q[3][1]);
    quadfill((int)(q[0][0]+cam_x), (int)(q[0][1]+cam_y), (int)(q[1][0]+cam_x), (int)(q[1][1]+cam_y),
             (int)(q[2][0]+cam_x), (int)(q[2][1]+cam_y), (int)(q[3][0]+cam_x), (int)(q[3][1]+cam_y),
             color);
}
// The four verticals + the top rectangle of a box: a wireframe silhouette, which is how you read
// "that is a tall thing" at 24px per tile without owning a sprite.
static void tnb_box(float x0, float y0, float x1, float y1, float vz, int color) {
    const float cx[4] = { x0, x1, x1, x0 }, cy[4] = { y0, y0, y1, y1 };
    float lo[4][2], hi[4][2];
    for (int c = 0; c < 4; c++) {
        tnb_project(tn_rot, cx[c], cy[c], 0,  &lo[c][0], &lo[c][1]);
        tnb_project(tn_rot, cx[c], cy[c], vz, &hi[c][0], &hi[c][1]);
    }
    for (int c = 0; c < 4; c++) {
        const int d = (c + 1) & 3;
        line((int)(lo[c][0]+cam_x), (int)(lo[c][1]+cam_y), (int)(hi[c][0]+cam_x), (int)(hi[c][1]+cam_y), color);
        line((int)(hi[c][0]+cam_x), (int)(hi[c][1]+cam_y), (int)(hi[d][0]+cam_x), (int)(hi[d][1]+cam_y), color);
    }
}

void tn_build_draw(void) {
    if (!tnb_on) return;
    char t[64];
    const int kind  = (tn_build_kind < 0 || tn_build_kind >= TN_OBJ_KIND_COUNT) ? 0 : tn_build_kind;
    const int price = TN_OBJ_PRICE[kind];

    // The buildable extent, outlined. The verb is "shape space", so the space needs a visible edge —
    // and G/H push that edge, so it has to read as a thing you can move, not as scenery. Light grey
    // rather than dark: the first cut used CLR_DARK_GREY and it vanished into the floor's own border.
    {
        const float W = tn_bw * TN_TILE_VOX, H = tn_bh * TN_TILE_VOX;
        const float ex[4] = { 0, W, W, 0 }, ey[4] = { 0, 0, H, H };
        for (int c = 0; c < 4; c++) {
            const int d = (c + 1) & 3;
            float a[2], b[2];
            tnb_project(tn_rot, ex[c], ey[c], 0, &a[0], &a[1]);
            tnb_project(tn_rot, ex[d], ey[d], 0, &b[0], &b[1]);
            line((int)(a[0]+cam_x), (int)(a[1]+cam_y), (int)(b[0]+cam_x), (int)(b[1]+cam_y), CLR_LIGHT_GREY);
        }
    }

    const int verdict = tnb_tx < 0 ? TN_BUILD_OFF_GRID
                                   : tn_build_check(kind, tnb_tx, tnb_ty, tnb_facing, tn_build_hh);
    // Three colours for three different sentences: green "yes", orange "yes but you are broke",
    // red "no". Affordability is not a geometry failure and must not read like one.
    const int col = verdict == TN_BUILD_OK       ? CLR_GREEN
                  : verdict == TN_BUILD_NO_MONEY ? CLR_ORANGE : CLR_RED;

    if (tnb_tx >= 0) {
        int w, h; tn_build_footprint(kind, tnb_facing, &w, &h);
        const float x0 = tnb_tx * TN_TILE_VOX, y0 = tnb_ty * TN_TILE_VOX;
        const float x1 = x0 + w * TN_TILE_VOX,  y1 = y0 + h * TN_TILE_VOX;
        tnb_quad(x0 + 1, y0 + 1, x1 - 1, y1 - 1, col);          // the claimed tiles, inset a voxel
        tnb_box(x0 + 1, y0 + 1, x1 - 1, y1 - 1, (float)tnb_high[kind], col);
    }

    // The label. FONT_SMALL because this sits over the room and must not become the picture; reset
    // to FONT_NORMAL on the way out so the HUD (drawn after us) gets the font it was written for.
    font(FONT_SMALL);
    const int purse = (tn_build_hh >= 0 && tn_build_hh < tn_house_n) ? tn_house[tn_build_hh].money : 0;
    snprintf(t, sizeof t, "BUILD  %s  %d  [purse %d]", tnb_name[kind] ? tnb_name[kind] : "?",
             price, purse);
    print(t, 3, 11, verdict == TN_BUILD_OK ? CLR_WHITE : col);
    if (verdict != TN_BUILD_OK) print(tn_build_why(verdict), 3, 18, col);
    print("B live  [ ] pick  G H grow  RMB exit", 3, SCREEN_H - 33, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

// ─────────────────────────────────────────────────────────────────────────────
// SPEC — the module's own assertions (runtime/spec.h, "SPECS ON AN INCLUDEABLE"). The cart's
// spec() calls tn_build_selfcheck(); nothing here runs in a normal build.
//
// The load-bearing one is the round trip: EVERY tile at EVERY rotation, through the real public
// entry point including the camera, because picking that is wrong at rotation 2 only is silent —
// the room still draws perfectly and the click just lands one tile away. isoroom's version of this
// assertion caught real bugs.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DE_SPEC
static char tnb_sp[160];
// The stub wall predicate for case 5, forward-declared so the checks read in order. It exists only
// in the DE_SPEC build; nothing in the shipped cart registers a wall predicate at all.
static bool tnb_spec_wall(int tx, int ty);

void tn_build_selfcheck(void) {
    const int was_bw = tn_bw, was_bh = tn_bh, was_rot = tn_rot;
    tnb_blocked = NULL;

    // ── 1. PICKING IS EXACT AT ALL FOUR ROTATIONS ───────────────────────────
    // Through tn_build_pick(), so the camera, the floorf and the int truncation are all in frame.
    // Tile CENTRES: a corner is genuinely ambiguous (it belongs to four tiles), a centre is not.
    tn_obj_n = 0; tn_agent_n = 0;
    for (int r = 0; r < TN_ROTS; r++) {
        tn_rot = r; tn_camera();
        int bad = 0, tested = 0;
        for (int ty = 0; ty < tn_bh; ty++) for (int tx = 0; tx < tn_bw; tx++) {
            float sx, sy;
            tnb_project(r, tx * TN_TILE_VOX + TN_TILE_VOX * 0.5f,
                           ty * TN_TILE_VOX + TN_TILE_VOX * 0.5f, 0, &sx, &sy);
            int gx = -1, gy = -1;
            const bool hit = tn_build_pick((int)(sx + cam_x), (int)(sy + cam_y), &gx, &gy);
            tested++;
            if (!hit || gx != tx || gy != ty) bad++;
        }
        snprintf(tnb_sp, sizeof tnb_sp,
                 "build: rot %d, all %d tile centres pick their own tile", r, tested);
        expect(bad == 0, tnb_sp);
    }
    // And the converse, so it cannot pass by returning true for everything: a point well outside
    // the building is honestly off the floor rather than clamped to the nearest tile.
    tn_rot = 0; tn_camera();
    {
        int gx = 7, gy = 7;
        const bool hit = tn_build_pick(-400, -400, &gx, &gy);
        expect(!hit && gx == 7 && gy == 7,
               "build: a point off the floor returns false and does not touch the caller's tile");
    }

    // ── 2. THE QUARTER-TURN IS INVERTIBLE ───────────────────────────────────
    // If turn/unturn disagree, picking is wrong on three of four rotations, which reads as a
    // mysterious "only some angles work" bug rather than as a maths error.
    for (int q = 0; q < TN_ROTS; q++) {
        float X, Y, x, y;
        tnb_turn(q, 3.0f, 5.0f, &X, &Y);
        tnb_unturn(q, X, Y, &x, &y);
        snprintf(tnb_sp, sizeof tnb_sp, "build: quarter-turn %d then unturn is the identity", q);
        expect(spec_close(x, 3.0f, 1e-4f) && spec_close(y, 5.0f, 1e-4f), tnb_sp);
    }

    // ── 3. THE FOOTPRINT FOLLOWS THE TURN ───────────────────────────────────
    // iso-rooms.md §8's unsolved problem, model half. A bed is 1x2; turned a quarter it is 2x1.
    {
        int w = 0, h = 0;
        tn_build_footprint(TN_OBJ_BED, 0, &w, &h);
        expect(w == 1 && h == 2, "build: a bed claims 1x2 tiles unturned");
        tn_build_footprint(TN_OBJ_BED, 1, &w, &h);
        expect(w == 2 && h == 1, "build: a quarter-turned bed claims 2x1 — the footprint turns too");
        tn_build_footprint(TN_OBJ_BED, 2, &w, &h);
        expect(w == 1 && h == 2, "build: a half-turned bed is 1x2 again");
    }

    // ── 4. LEGALITY REJECTS EVERY ILLEGAL CASE, AND ACCEPTS THE LEGAL ONE ───
    tn_obj_n = 0; tn_agent_n = 0; tn_bw = 8; tn_bh = 8;
    expect(tn_build_legal(TN_OBJ_TOILET, 3, 3, 0) == TN_BUILD_OK,
           "build: an empty tile inside the building is legal");
    expect(tn_build_legal(TN_OBJ_TOILET, -1, 3, 0) == TN_BUILD_OFF_GRID,
           "build: a negative tile is off the grid, not wrapped");
    expect(tn_build_legal(TN_OBJ_TOILET, 8, 3, 0) == TN_BUILD_OFF_GRID,
           "build: a tile past tn_bw is off the grid");
    // The footprint is what must fit, not the origin: a 1x2 bed on the last row hangs off.
    expect(tn_build_legal(TN_OBJ_BED, 3, 7, 0) == TN_BUILD_OFF_GRID,
           "build: a 1x2 bed on the last row is off the grid — the FOOTPRINT has to fit, not the origin");
    expect(tn_build_legal(TN_OBJ_BED, 3, 6, 0) == TN_BUILD_OK,
           "build: and one row up it fits exactly");
    {
        const int bed = tn_add_obj(TN_OBJ_BED, 3, 3, 0);       // occupies 3,3 and 3,4
        expect(bed >= 0, "build: setup, the bed went in");
        expect(tn_build_legal(TN_OBJ_TOILET, 3, 3, 0) == TN_BUILD_OCCUPIED,
               "build: you cannot stack an object on another object");
        expect(tn_build_legal(TN_OBJ_TOILET, 3, 4, 0) == TN_BUILD_OCCUPIED,
               "build: nor on the SECOND tile of a 1x2 footprint — overlap is rectangles, not origins");
        expect(tn_build_legal(TN_OBJ_TOILET, 3, 5, 0) == TN_BUILD_OK,
               "build: the tile just past its footprint is free");
        // A 2x1 sofa arriving from the left must notice the bed it would overlap by one tile.
        expect(tn_build_legal(TN_OBJ_SOFA, 2, 3, 0) == TN_BUILD_OCCUPIED,
               "build: a 2x1 sofa overlapping by one tile is refused");
    }

    // ── 5. WALLS: the hook the contract has no data for ─────────────────────
    // TN_BUILD_BLOCKED is unreachable in the shipped cart because nothing owns walls yet. It is
    // asserted here against a stub predicate so that whoever adds walls inherits a working seam
    // rather than discovering an untested branch.
    tn_build_set_blocked(NULL);
    {
        tn_build_set_blocked(tnb_spec_wall);
        expect(tn_build_legal(TN_OBJ_TOILET, 6, 6, 0) == TN_BUILD_BLOCKED,
               "build: a blocked tile refuses placement, and says 'on a wall' rather than 'occupied'");
        // The whole footprint is tested, not just the origin tile.
        expect(tn_build_legal(TN_OBJ_SOFA, 4, 6, 0) == TN_BUILD_BLOCKED,
               "build: a footprint that only CLIPS a wall is refused too");
        expect(tn_build_legal(TN_OBJ_TOILET, 1, 1, 0) == TN_BUILD_OK,
               "build: and a tile the predicate does not claim is still legal");
        tn_build_set_blocked(NULL);
        expect(tn_build_legal(TN_OBJ_TOILET, 6, 6, 0) == TN_BUILD_OK,
               "build: with no wall predicate registered, nothing is blocked");
    }

    // ── 6. MONEY: paid on success, untouched on failure ─────────────────────
    // The design's constraint on the verb (§5): the purse is what stops you solving every problem
    // by buying another toilet. The arithmetic belongs to econ — what is asserted here is that this
    // module ROUTES to it correctly, i.e. that a refused placement never reaches the purse.
    tn_obj_n = 0; tn_house_n = 2;
    tn_house[0] = (TnHousehold){ 100, {0}, 0, 20, 0 };
    {
        const int price = TN_OBJ_PRICE[TN_OBJ_TOILET];
        expect(price > 0, "build: setup, a toilet costs something");
        tn_house[0].money = (short)(price * 2);
        const int a = tn_build_place(TN_OBJ_TOILET, 2, 2, 0, 0);
        snprintf(tnb_sp, sizeof tnb_sp, "build: placing debits the purse exactly TN_OBJ_PRICE (%d left of %d)",
                 tn_house[0].money, price * 2);
        expect(a >= 0 && tn_house[0].money == (short)price, tnb_sp);
        // Now it can afford exactly one more, but not on an occupied tile: the failure must cost
        // nothing. A refused purchase that still charges you is the least forgivable bug in a
        // game whose only verb is spending money.
        const int b = tn_build_place(TN_OBJ_TOILET, 2, 2, 0, 0);
        expect(b == -TN_BUILD_OCCUPIED && tn_house[0].money == (short)price,
               "build: a refused placement returns the negated reason and charges nothing");
        const int c = tn_build_place(TN_OBJ_TOILET, 4, 4, 0, 0);
        expect(c >= 0 && tn_house[0].money == 0, "build: the last affordable one empties the purse");
        const int d = tn_build_place(TN_OBJ_TOILET, 6, 4, 0, 0);
        expect(d == -TN_BUILD_NO_MONEY && tn_obj_n == 2,
               "build: broke means NO_MONEY and no object appears");
        // Affordability is not a legality failure: the same tile is still legal, just unaffordable.
        expect(tn_build_legal(TN_OBJ_TOILET, 6, 4, 0) == TN_BUILD_OK,
               "build: tn_build_legal ignores money — 'cannot afford' and 'cannot fit' stay separate");
        expect(tn_build_check(TN_OBJ_TOILET, 6, 4, 0, 0) == TN_BUILD_NO_MONEY,
               "build: and tn_build_check is the one that knows about the purse");
    }

    // ── 7. GROWING THE BUILDING, AND REFUSING TO ORPHAN ─────────────────────
    // tn_bw/tn_bh are variables so the player can grow the place (contract). Shrinking under a
    // placed object would leave it bidding from a tile that no longer exists.
    tn_obj_n = 0; tn_bw = 8; tn_bh = 8;
    expect(tn_build_grow(2, 3) && tn_bw == 10 && tn_bh == 11, "build: the building grows");
    tn_bw = TN_MW - 1; tn_bh = 4;
    expect(tn_build_grow(5, 0) && tn_bw == TN_MW, "build: growth clamps to the array bound TN_MW");
    expect(!tn_build_grow(5, 0), "build: at the bound, a further grow changes nothing and says so");
    tn_bw = 8; tn_bh = 8; tn_obj_n = 0;
    tn_add_obj(TN_OBJ_TOILET, 7, 7, 0);
    expect(!tn_build_grow(-2, 0) && tn_bw == 8,
           "build: a shrink that would orphan an object is refused outright");
    expect(tn_build_grow(0, 2) && tn_bh == 10, "build: but growing past it is fine");

    // ── 8. THE MODE IS A MODE ───────────────────────────────────────────────
    // The cart calls tn_build_input() unconditionally, so live mode has to be genuinely inert.
    {
        const int before = tnb_on;
        if (tnb_on) tn_build_toggle();
        expect(!tn_build_active(), "build: live mode is off by default");
        tn_build_toggle();
        expect(tn_build_active(), "build: the toggle turns build mode on");
        tn_build_toggle();
        expect(!tn_build_active(), "build: and off again");
        if (before) tn_build_toggle();
    }

    tn_bw = was_bw; tn_bh = was_bh; tn_rot = was_rot;
    tn_build_set_blocked(NULL);
    tn_world_init();
    tn_econ_reset();     // case 6 moved real money; leave econ's ledger as clean as we found it
    tn_camera();
}

// Two tiles of "wall" at (5,6) and (6,6), so both a 1x1 landing ON one and a 2x1 footprint that only
// CLIPS one can be told apart from an overlap with a real object.
static bool tnb_spec_wall(int tx, int ty) { return tx >= 5 && tx <= 6 && ty == 6; }
#endif  // DE_SPEC

// ── HANDOVER: what the contract could not express (model.h rule 3) ──────────
//  1. OBJECT FOOTPRINT. `TnObject` has tx/ty but no size, so `tnb_foot` here duplicates atlas.h's
//     ISO_FOOTPRINT. Which tiles an object CLAIMS is a fact about the sim, not about the picture:
//     agents path around it and this module refuses overlaps with it. Wants
//     `extern const unsigned char TN_OBJ_FOOT[TN_OBJ_KIND_COUNT][2];` in the contract, owned by
//     whoever owns the object tables, with art deriving its voxel box from that.
//  2. WALLS. There is no wall data anywhere in the contract — atlas.h bakes four wall models and
//     the sim has no array of them — so "not on a wall" is a registered predicate with no
//     registrant. Wants a wall representation (an occupancy bitmap per tile EDGE, most likely,
//     since a wall sits between tiles rather than on one).
//  3. A PURCHASE FUNCTION — RESOLVED, but not by the contract. The contract has no `tn_buy`, so
//     this module was written against `tn_house[].money` directly; econ.h then landed
//     `tn_can_afford(household, kind)` and `tn_buy_obj(household, kind, tx, ty)` and calls the
//     latter "the seam a future build module calls", so build.h now calls both and owns no money
//     code at all. Neither symbol is in model.h. They should be, since two modules depend on them.
//  4. THE CAMERA. art.h owns `cam_x/cam_y` as file-scope statics, so this module can only reach
//     them by being included after art.h (enforced by the #error at the top). Wants
//     `extern float tn_cam_x, tn_cam_y;`.
//  5. WHICH HOUSEHOLD IS BUYING. `tn_build_hh` is defined here because the contract has no notion
//     of a selected household. If the HUD grows a household picker it should move to the contract.
//  6. REMOVAL. Nothing owns deleting an object, and it is not safe to add alone: agents hold
//     `target_obj` as an INDEX, so compacting tn_obj[] would silently repoint them. It needs either
//     a tombstone (`kind = TN_OBJ_KIND_COUNT`) or a coordinated sweep, which is a contract decision.
//
// ── ADR-0006: EXTRACT runtime/isoview.h NOW ─────────────────────────────────
// tenement.c's own todo says "extract runtime/isoview.h when the second consumer proves the shape".
// This is that second consumer: tnb_turn/tnb_project are byte-identical to art.h's tnr_iso_turn/
// tnr_iso_project, and tnb_unturn/tnb_unproject are the inverse half isoroom.c already ships and
// spec-tests. Three copies of a projection whose failures are all SILENT is the case for a header:
// turn/unturn/project/unproject/depth, plus the camera-fitting loop. Not done here because this
// module owns one file and a shared header is a repo-level decision.
#endif // TENEMENT_BUILD_H
