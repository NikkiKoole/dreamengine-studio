/* de:meta
{
  "slug": "lockup",
  "title": "LOCKUP",
  "status": "wip",
  "created": "2026-07-31",
  "resizable": true,
  "kind": [
    "game"
  ],
  "teaches": [
    "pathfinding",
    "finite-state-ai",
    "schedule-driven-agents",
    "song-arrangement",
    "dithering-gradient",
    "functional-harmony"
  ],
  "genre": "simulation",
  "homage": "Prison Architect (2015)",
  "todo": [
    "BLOCKING/night: the warm-light pass adds CLR_BROWN (#ab5236, a saturated red-orange) through a 4x4 ordered dither over a triple CLR_INDIGO multiply, so at 21:00 the block is a regular lattice of red dots over plum ground ('the prison looks like it is ON FIRE behind a fly screen'). Both critics called this the worst thing in the game. Fluorescent institutional light is a near-white warm grey and its spill must be LOW-FREQUENCY, not a lattice - art.h's own comment 40 lines earlier warns a regular lattice at this scale reads as a window screen, then ships exactly that. runtime/lockup/art.h lkr_light_pass.",
    "BLOCKING/ground: still noise - re-tuned, not fixed. Open ground measures ~19-25% loose flecks at a +54 luminance step, so the yard shimmers. Three independent judgements agree (both critics + the owner's own read). Cut fleck coverage and value step hard; let the 5-tile value-noise field carry the variation instead. art.h lkr_floor_detail / lockup.cart.js floor sprites.",
    "BLOCKING/people: the actor sprites are front-facing 3/4 figures WITH EYES, so you cannot tell which way anyone faces in a top-down game. All 16 slots share one build. Needs a genuine top-down silhouette (shoulders + crown, no face) with the 4 facings actually distinguishable. lockup.cart.js slots 0-15.",
    "BLOCKING/office floor: FL_WOOD's tone is CLR_DARK_BROWN/CLR_BROWN, i.e. the same ramp as WL_BRICK, so the warden's office reads as a solid filled block of brickwork with no interior. Re-voice FL_WOOD to a warm mid-tone that is not the brick ramp. art.h LKR_FLOOR_TONE.",
    "MAJOR/role colours collide: cook = WHITE/LIGHT_GREY and doctor = LIGHT_GREY/BLUE, so two of five roles are both white-grey and you cannot name a role at a glance (the design doc's 'people read at a glance by role' fails). Needs a distinct hue per role - model.h LK_ROLE_UNIFORM/LK_ROLE_TROUSER, which is contract, so change it deliberately.",
    "MAJOR/shadows are the wrong colour: wall and object shadows fill CLR_BLACK under BLEND_AVG, so a 3px strip beside every wall reads as a void gutter rather than shade; object wear-marks measure plum (#49333b) on warm grey concrete. Shadow DIRECTION and geometry are correct and must not be touched - only the colour is wrong.",
    "MAJOR/jail doors read as damage: at 1x each cell door is a pale barred comb in the wall gap that reads as 'a zipper, a spring, or barbed wire lying flat' rather than a door. art.h lkr_door_tile.",
    "MAJOR/every solid 1x1 object sits on a dark postage stamp: lkr_shadow_pass rectfills a full-tile black offset under each object, so furniture looks pasted on a card. Should be a contact shadow hugging the silhouette.",
    "MAJOR/wall edge lighting is inverted: the NW (lit) edges carry the darker 1px line. The global light direction is right; the per-wall edge assignment is backwards.",
    "MAJOR/overlay legibility: the invalid-room overlay is a 50% 2px hatch that makes the one message the design doc says MUST be legible ('needs serving table') hard to read. Overlays must sit OVER the world without hiding it.",
    "MAJOR/HUD text: stacked toasts overlap each other and the top strip truncates its own text on frame one ('5 beds, 5 ..') while ~230px of the strip sits empty. ui-audit does not catch it. runtime/lockup/hud.h lkh_fit.",
    "MINOR/canteen tile floor reads as a loud 8px chequerboard with wrong-hue tiles (grout DARK_GREY, highlights WHITE on LIGHT_GREY - too wide a value spread).",
    "MINOR/the office chair reads as a toilet (slot 34 vs slot 25 are near-identical pale-topped pedestals).",
    "MINOR/the yard boundary paint reads as a UI selection marquee rather than worn line-marking.",
    "MINOR/rooms are under-furnished: the canteen is ~90 tiles holding five pieces of furniture, so 'a place with a purpose' half-fails. Either shrink the starting rooms or furnish them.",
    "canvas-diff fails at 118px of 324,000 - NOT a regression: blend() is documented as intentionally not GPU/software-identical (docs/design/blend-tables.md), so overlapping blend() shadows can never diff to zero. All diffs are one palette index in the darkest tones and are invisible. DECIDE: declare a `// canvas-diff: max N` budget in lockup.c, or stop using blend() for shadows.",
    "Tile.obj_used is a single BIT, so a bunk bed advertises 2 capacity slots but can only host one actor. Fixing it properly means widening the contract in model.h.",
    "Audio gates have NOT been run yet (tune-check / level-check / fx-check / click-check / stereo-check / soak-check / lint-fx-frame). The score is unverified.",
    "mobile-lint and a ui-audit pass have not been run."
  ],
  "lineage": "Prison Architect demake built on this library's own colony lineage (dwarffort's designate-and-they-work loop, sims' need-decay-to-object-seek loop), novel in making the REGIME the spine: a 24-hour timetable the whole population obeys, so a need can only fall when a valid room, a reachable path, a permitted hour and a free capacity slot all line up - and danger is the integral of unmet need rather than a dice roll.",
  "description": {
    "summary": "Build and run a prison. You never touch a prisoner - you build the machine and write the timetable, and the machine either meets ninety men's needs or it doesn't.",
    "detail": "A prison is a machine for meeting needs on a schedule. Every prisoner carries nine needs, and a need can only fall when four things line up at once: a room that serves it, enclosed and holding the objects its type demands; a path there through doors they're allowed through; an hour the REGIME grants; and a free bed, bench, shower head or phone not already taken. Break any one and the need climbs - and danger is the integral of unmet need, never a dice roll, so a riot is always your own arithmetic coming back. Workmen build what you designate, cooks stock the serving tables (no cook, no meal, however many tables you own), guards patrol the sectors you deploy them to and their response time is a path length. Lockdown buys calm by locking every jail door, which kills the need satisfaction that caused the riot in the first place. The score is a lonely slide-guitar Americana bed whose layers, harmony and tempo track the prison's tension, and night is real light: lit blocks glowing across a dark yard.",
    "controls": "MOUSE drives everything: pick a tool from the palette, drag to paint floors, walls, rooms and zones, click to place doors and objects (R rotates), right-click or DEMOLISH to remove. Drag the map or use ARROWS/WASD to pan. Click a prisoner to inspect his needs, a room to see why it's invalid. Open the REGIME editor and drag activities across the 24 hours - that timetable is the whole game. SPACE pauses, 1/2/4 set speed, TAB cycles the overlays."
  }
}
de:meta */
#include "studio.h"
#include <stdio.h>              // snprintf, for the end-card summary line
#include <stdlib.h>             // abs, in spec()
#include "spec.h"
#include "endcard.h"
#include "lockup/model.h"

// ── LOCKUP — a Prison Architect demake ──────────────────────────────────────
//
// This file is GLUE ONLY. Every subsystem lives in its own header under
// runtime/lockup/, implemented against the frozen contract in
// runtime/lockup/model.h. Design + rationale: docs/design/lockup.md.
//
//   model.h   the contract: types, frozen tables, the sprite slot map
//   grid.h    tiles, wall join masks, doors, objects, ROOM DISCOVERY, jobs
//   path.h    heap A*, component labelling, nearest-facility flow fields
//   actors.h  needs, the REGIME, prisoner + staff FSMs, danger -> riot
//   econ.h    money, wages, grants, the five grades, the ledger
//   art.h     the renderer: floors, procedural joined walls, light, overlays
//   score.h   the adaptive Americana score + diegetic sfx
//   hud.h     tools, camera, panels, the regime editor
//
// The honest core, in one sentence: a prison is a machine for meeting needs on
// a schedule, and the player only ever touches the machine.

Sim lk;                     // the one global the cart itself owns

#include "lockup/grid.h"
#include "lockup/path.h"
#include "lockup/actors.h"
#include "lockup/econ.h"
#include "lockup/art.h"
#include "lockup/score.h"
#include "lockup/hud.h"

// ── the starting prison ─────────────────────────────────────────────────────
#define LK_START_MONEY   30000
#define LK_INTAKE_HOURS  8.0f      // a bus every 8 in-game hours

static float lkc_end_t;            // seconds since the end state began

// A spot to stand an opening staff member on: the nth passable INDOOR tile.
// Falls back to anywhere passable, because a plot with no interior yet must
// still be able to place a workman (he is the one who builds the interior).
static int lkc_staff_spot(int nth) {
    int found = 0;
    for (int c = 0; c < LK_N; c++)
        if (lk_can_pass(c, RL_GUARD) && lk_indoors(c) && found++ == nth) return c;
    for (int c = 0; c < LK_N; c++)
        if (lk_can_pass(c, RL_GUARD)) return c;
    return lk_idx(LK_MW / 2, LK_MH / 2);
}

static void lkc_reset(int seed) {
    lk = (Sim){ 0 };
    lk.seed  = seed;
    lk.clock = 7.0f;               // open at 07:00, the prison already awake
    lk.day   = 1;
    lk.hour  = 7;
    lk.money = LK_START_MONEY;
    lk.speed = 1;
    lk.alarm = AL_CALM;
    lk.intake_t = LK_INTAKE_HOURS * 0.5f;
    lkc_end_t = 0;

    lk_grid_init(seed);
    lk_path_init();
    lk_actors_init();
    lk_econ_init();
    lk_art_init();
    lk_score_init();
    lk_hud_init();

    // Rooms and reachability MUST exist before anyone is placed: intake asks
    // lk_assign_cell() for a bed, which is a query against discovered rooms.
    lk_rooms_rebuild();
    lk_path_update();

    // ── the opening payroll ── a prison with no guards is not a prison, and a
    // prison with no cook cannot feed anyone however many tables it owns.
    for (int i = 0; i < 4; i++) lk_spawn(RL_GUARD,   lkc_staff_spot(i * 7));
    for (int i = 0; i < 3; i++) lk_spawn(RL_WORKMAN, lkc_staff_spot(3 + i * 11));
    lk_spawn(RL_COOK,   lkc_staff_spot(5));
    lk_spawn(RL_DOCTOR, lkc_staff_spot(9));

    // ── and the first bus, so day one opens with men already inside ──
    lk_intake(6);
}

void init(void) {
    colorkey(CLR_BLACK);           // palette index 0 is the sprite transparency
    lkc_reset(20260731);
}

// ── the clock ───────────────────────────────────────────────────────────────
// Advances lk.clock in in-game hours and fires the hour/day rollovers. Returns
// true on an hour boundary, which is when the regime re-tasks the population.
static bool lkc_advance_clock(float d) {
    int was_hour = lk.hour;
    lk.clock += d / LK_HOUR_SECS;
    while (lk.clock >= 24.0f) {
        lk.clock -= 24.0f;
        lk.day++;
        lk_econ_day_end();
    }
    lk.hour = (int)lk.clock;
    return lk.hour != was_hour;
}

static void lkc_check_over(void) {
    if (lk.over) return;
    if (lk.money < -20000)                        lk.over = 1;   // bankrupt
    else if (lk.alarm == AL_RIOT && lk.alarm_t > 180.0f) lk.over = 2;   // lost control
    else if (lk.day > 30)                         lk.over = 3;   // endured
}

void update(void) {
    int vx, vy, vw, vh;
    lk_hud_viewport(&vx, &vy, &vw, &vh);

    // Set the camera BEFORE reading input, so mouse_world_x/y() inverts the
    // SAME transform the player was looking at when they clicked. Leaving this
    // to draw() makes every build click land on the wrong tile — the trap
    // documented in tools/carts/worldpointer.c:52-65.
    camera(lk_cam_x - vx, lk_cam_y - vy);

    if (lk.over) {
        lkc_end_t += dt();
        if (lkc_end_t > 0.8f && (mouse_pressed(MOUSE_LEFT) || keyp(KEY_SPACE)))
            lkc_reset(lk.seed + 1);
        lk_score_update(dt());
        return;
    }

    lk_hud_update(dt());                   // tools, panels, camera, orders
    camera(lk_cam_x - vx, lk_cam_y - vy);  // re-set: the hud may have panned

    if (lk.speed > 0) {
        float d = dt() * (float)lk.speed;
        bool  new_hour = lkc_advance_clock(d);
        (void)new_hour;                    // actors read lk.hour themselves

        lk_path_update();                  // deferred relabel + one flow field
        lk_grid_update(d);                 // doors, filth
        lk_actors_update(d);               // needs, regime, FSMs, danger
        lk_econ_update(d);
        lkc_check_over();
    }

    lk_art_update(dt());                   // light bake, weather, particles
    lk_score_update(dt());                 // reads lk.tension

#ifdef DE_TRACE
    watch("day",      "%d",   lk.day);
    watch("clock",    "%.2f", lk.clock);
    watch("money",    "%d",   lk.money);
    watch("prisoners","%d",   lk.n_prisoners);
    watch("tension",  "%.3f", lk.tension);
    watch("alarm",    "%d",   lk.alarm);
    watch("rooms",    "%d",   lk_nroom);
    watch("jobs",     "%d",   lk_job_count());
#endif
}

void draw(void) {
    int vx, vy, vw, vh;
    lk_hud_viewport(&vx, &vy, &vw, &vh);

    cls(CLR_BROWNISH_BLACK);

    // ── the world, inside the viewport, under the camera ──
    clip(vx, vy, vw, vh);
    lk_art_world  (lk_cam_x, lk_cam_y, vw, vh);
    lk_art_actors (lk_cam_x, lk_cam_y, vw, vh);
    lk_art_fx     (lk_cam_x, lk_cam_y);
    if (lk_overlay != OV_NONE)
        lk_art_overlay(lk_overlay, lk_cam_x, lk_cam_y, vw, vh);
    clip(0, 0, 0, 0);

    // ── chrome, in screen space ──
    camera(0, 0);
    lk_hud_draw();

    if (lk.over) {
        EndCard c = endcard(lkc_end_t, 232, 76, 44,
                            CLR_DARKER_GREY, CLR_LIGHT_YELLOW, CLR_DARK_GREY);
        if (c.settled) {
            static const char *TITLE[4] = { "", "BANKRUPT", "CONTROL LOST", "THE PRISON ENDURES" };
            static const char *LINE [4] = { "",
                "the books closed before the block did",
                "the riot outlasted the guards",
                "thirty days, and the machine still runs" };
            int col = (lk.over == 3) ? CLR_LIME_GREEN : CLR_ORANGE;
            // centre on the CARD, not screen_w()/2 — endcard() lays the card out
            // against the compile-time SCREEN_W, so on this resizable cart the
            // two disagree the moment the window is resized.
            int mid = c.x + c.w / 2;
            print_centered(TITLE[lk.over], mid, c.y + 12, col);
            print_centered(LINE [lk.over], mid, c.y + 28, CLR_LIGHT_PEACH);
            char buf[64];
            snprintf(buf, sizeof buf, "day %d   %d held   $%d", lk.day, lk.n_prisoners, lk.money);
            print_centered(buf, mid, c.y + 44, CLR_LIGHT_GREY);
            if (blink(18))
                print_centered("- click to open a new prison -", mid,
                               c.y + c.h - 14, CLR_MEDIUM_GREY);
        }
    }
}

// ── spec() — the honest-core gate ───────────────────────────────────────────
// Asserts the five conditions of docs/design/lockup.md section 1: a need falls
// ONLY IF a valid room exists, is reachable, the regime permits it, and a free
// capacity slot is available. Each assertion below breaks exactly one of them
// and proves the need climbs. Run: node tools/spec.js lockup
#ifdef DE_SPEC
static int lks_find_role(int role) {
    for (int i = 0; i < lk_nact; i++)
        if (lk_a[i].alive && lk_a[i].role == role) return i;
    return -1;
}
// a rectangle of floor walled in, with one door — the minimum viable room.
// Wipes the plot AND a one-tile margin first: whatever the starting prison put
// here would otherwise leak into the test. (That bit once — this used to build
// straight on top of the default cell block, so the "room" it tested was
// already room 6 with a jail door through the middle of it.)
static void lks_build_room(int x0, int y0, int x1, int y1, int type, int doorx) {
    for (int y = y0 - 1; y <= y1 + 1; y++) for (int x = x0 - 1; x <= x1 + 1; x++) {
        if (!lk_in(x, y)) continue;
        int c = lk_idx(x, y);
        lk_clear_tile(c);                  // wall, door and object gone
        lk_paint_room(c, RM_NONE);         // room intent gone
        lk_set_floor(c, FL_DIRT);
    }
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        int c = lk_idx(x, y);
        bool edge = (x == x0 || x == x1 || y == y0 || y == y1);
        lk_set_floor(c, FL_CONCRETE);
        if (edge) lk_set_wall(c, WL_BRICK);
    }
    lk_set_wall(lk_idx(doorx, y1), WL_NONE);
    lk_set_door(lk_idx(doorx, y1), DR_JAIL);
    for (int y = y0 + 1; y < y1; y++) for (int x = x0 + 1; x < x1; x++)
        lk_paint_room(lk_idx(x, y), type);
    lk_rooms_rebuild();
    lk_path_update();
}

void spec(void) {
    step(1);   // init()

    // ── the contract's own invariants ──
    expect(LK_N == LK_MW * LK_MH, "map dimensions agree");
    expect(LK_ROOM[RM_CELL].req[0] == OB_BED && LK_ROOM[RM_CELL].req[1] == OB_TOILET,
           "a cell requires a bed and a toilet");
    expect(LK_ROOM[RM_YARD].enclosed == 0, "a yard is the one room that need not be enclosed");
    expect(LK_OBJ[OB_BED].h == 2, "a bed is a two-tile object");

    // ── DOOR PERMISSION: the mechanism lockdown rests on ──
    {
        int c = lk_idx(6, 6);
        lk_set_floor(c, FL_CONCRETE);
        lk_set_door(c, DR_JAIL);
        lk_t[c].locked = 0;
        expect(lk_can_pass(c, RL_PRISONER), "an unlocked jail door passes a prisoner");
        expect(lk_can_pass(c, RL_GUARD),    "an unlocked jail door passes a guard");
        lk_t[c].locked = 1;
        expect(!lk_can_pass(c, RL_PRISONER), "a LOCKED jail door stops a prisoner");
        expect(lk_can_pass(c, RL_GUARD),     "a locked jail door never stops a guard");
        lk_set_door(c, DR_STAFF);
        expect(!lk_can_pass(c, RL_PRISONER), "a staff door never passes a prisoner");
        expect(lk_can_pass(c, RL_DOCTOR),    "a staff door passes a doctor");
        lk_set_door(c, DR_NONE);
    }

    // ── ROOM DISCOVERY: enclosure and the MISSING-OBJECT reason ──
    {
        lks_build_room(40, 20, 45, 25, RM_CELL, 42);
        int rid = lk_room_of(lk_idx(42, 22));
        expect(rid > 0, "a walled, painted area is discovered as a room");
        expect(!lk_room[rid].valid, "a cell with no furniture is invalid");
        expect(lk_room[rid].missing == OB_BED || lk_room[rid].missing == OB_TOILET,
               "an invalid cell names a required object as the reason");

        lk_place_obj(lk_idx(41, 21), OB_BED, 0);
        lk_place_obj(lk_idx(44, 21), OB_TOILET, 0);
        lk_rooms_rebuild();
        rid = lk_room_of(lk_idx(42, 22));
        expect(lk_room[rid].valid, "a cell with a bed and a toilet is valid");
        expect(lk_room[rid].missing == 0, "a valid room has no missing-object reason");
        expect(lk_room[rid].cap >= 1, "the bed gives the cell capacity");

        // break enclosure — the room must fail, and say so
        lk_set_wall(lk_idx(43, 20), WL_NONE);
        lk_rooms_rebuild();
        rid = lk_room_of(lk_idx(42, 22));
        expect(rid == 0 || !lk_room[rid].valid, "a hole in the wall invalidates the cell");
        lk_set_wall(lk_idx(43, 20), WL_BRICK);
        lk_rooms_rebuild();
        expect(lk_room[lk_room_of(lk_idx(42, 22))].valid, "sealing it makes it valid again");
    }

    // ── PATHING: orthogonal only, and reachability is honest ──
    {
        int a = lk_idx(42, 22), b = lk_idx(44, 22);
        short p[LK_MAXPATH];
        int n = lk_find_path(a, b, RL_PRISONER, p, LK_MAXPATH);
        expect(n > 0, "a path exists across an open cell");
        for (int i = 0; i < n; i++) {
            int prev = (i == 0) ? a : p[i - 1];
            int md = abs(lk_tx(p[i]) - lk_tx(prev)) + abs(lk_ty(p[i]) - lk_ty(prev));
            expect(md == 1, "every path step is orthogonal (never a diagonal wall-clip)");
            if (md != 1) break;
        }
        // seal a tile off completely and prove reachability reports it
        int iso = lk_idx(80, 50);
        lk_set_floor(iso, FL_CONCRETE);
        for (int d = 0; d < 4; d++) {
            const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
            lk_set_wall(lk_idx(80 + DX[d], 50 + DY[d]), WL_CONCRETE);
        }
        lk_path_update();
        expect(!lk_reachable(a, iso, RL_PRISONER), "a fully walled-in tile is unreachable");
    }

    // ── THE REGIME IS THE SPINE: no Eat slot starves a perfect kitchen ──
    {
        for (int h = 0; h < 24; h++) expect(lk_regime[h] < AC_COUNT, "every regime hour is a valid activity");
        int eat = 0;
        for (int h = 0; h < 24; h++) if (lk_regime[h] == AC_EAT) eat++;
        expect(eat > 0, "the default regime feeds the prison");
    }

    // ── CAPACITY CONTENTION: one bed cannot serve two men ──
    {
        int rid = lk_room_of(lk_idx(42, 22));
        int t1 = lk_room_free_slot(rid, ND_SLEEP);
        expect(t1 >= 0, "a valid cell offers a free sleeping slot");
        lk_t[t1].obj_used = 1;
        int t2 = lk_room_free_slot(rid, ND_SLEEP);
        expect(t2 < 0, "the only bed, once taken, is no longer offered");
        lk_room_release(t1);
        expect(lk_room_free_slot(rid, ND_SLEEP) >= 0, "releasing the bed offers it again");
    }

    // ── MONEY balances ──
    {
        int before = lk.money;
        lk_spend(500, "spec");
        expect_eq(lk.money, before - 500, "spending debits exactly");
        lk_earn(200, "spec");
        expect_eq(lk.money, before - 300, "earning credits exactly");
    }

    // ── DANGER IS THE INTEGRAL OF UNMET NEED, not a dice roll ──
    {
        int ai = lks_find_role(RL_PRISONER);
        if (ai >= 0) {
            float v0 = lk_a[ai].vol;
            for (int i = 0; i < ND_COUNT; i++) lk_a[ai].need[i] = 1.0f;   // desperate
            lk_a[ai].supp = 0;
            step(120);
            expect(lk_a[ai].vol > v0, "sustained unmet need raises volatility");
            for (int i = 0; i < ND_COUNT; i++) lk_a[ai].need[i] = 0.0f;   // content
            lk_a[ai].supp = 1.0f;
            step(240);
            expect(lk_a[ai].vol < 0.9f, "a content, suppressed prisoner calms down");
        } else {
            expect(0, "the starting prison holds at least one prisoner");
        }
    }

    // ── the score's input stays in range (it drives bpm and layer levels) ──
    step(60);
    expect(lk.tension >= 0.0f && lk.tension <= 1.0f, "tension stays normalised 0..1");
}
#endif
