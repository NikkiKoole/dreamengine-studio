// ─────────────────────────────────────────────────────────────────────────────
// lockup/hud.h — INPUT · TOOLS · CAMERA · ALL CHROME  (module tag: lkh_)
//
// Design: docs/design/lockup.md.  "The UI is chrome, not clutter — the map is
// the game; panels come in on demand and get out."  This module owns everything
// the player TOUCHES and everything drawn that is NOT the world.
//
// OWNS (the contract's hud globals):
//     lk_tool, lk_tool_arg, lk_overlay, lk_cam_x, lk_cam_y,
//     lk_sel_actor, lk_sel_room
//
// IMPLEMENTS: lk_hud_init / lk_hud_update / lk_hud_draw / lk_hud_toast /
//             lk_hud_viewport.
//
// ── KEY ALGORITHMS ──────────────────────────────────────────────────────────
//  1. REFLOW, NEVER SCALE.  Every rect is re-solved from screen_w()/screen_h()/
//     safe_rect() each frame through lay.h, so the cart can be resized live and
//     the canvas stays 1:1 with device px — which is what keeps ui.h's raw-canvas
//     hit-testing honest.  There is deliberately NO world zoom (a scaled camera
//     would silently desync every widget; see CLAUDE.md).  Three breakpoints:
//     roomy (labels), compact (<560 px: short labels), narrow (<430 px: the
//     toolbar goes two rows).  FIXED chrome is only the top bar + toolbar; every
//     picker and panel FLOATS over the map so the viewport — and therefore the
//     camera clamp and the tile the mouse is over — never jumps under the player.
//
//  2. MOUSE→WORLD, ONE PLACE.   world = canvas - viewport.origin + camera.
//     lkh_pt_tile() is the ONLY conversion in the module; everything (ghosts,
//     drags, selection, readout) goes through it, so a click can never land on a
//     different tile than the one under the highlight.
//
//  3. ONE DRAG STATE MACHINE for three gestures (BUILD / ERASE / PAN) across
//     three sources (a touch contact, right mouse, middle mouse).  The desktop
//     mouse arrives as a synthetic finger in the engine's touch pool, so the
//     contact path covers mouse AND touch with no branch.  Per tool:
//       SELECT              drag = pan; a drag shorter than LKH_CLICK_SLOP px
//                           is a CLICK → pick an actor, else a room
//       FLOOR/ROOM/ZONE     rubber-band RECTANGLE, applied on release
//       WALL                rubber-band rectangle, applied to its PERIMETER —
//                           one drag encloses a room (the choice model.h's brief
//                           left open: a band for everything reads consistent,
//                           and the perimeter is what a builder actually wants)
//       DOOR/OBJECT         placed on press, then once per NEW tile entered
//       DEMOLISH            rubber-band rectangle
//     Right-drag is a tool-aware ERASE: cancel a pending designation first, else
//     unpaint a room / reset a zone, else queue a demolition.  Nothing built is
//     ever destroyed by a stray click that had a designation to cancel instead.
//
//  4. NOTHING IS REFUSED SILENTLY.  lkh_designate() calls lk_queue() and then
//     OBSERVES lk_t[c].job: if the designation didn't take, lkh_why_refused()
//     names the reason from the same tables the grid checks, and it goes out as a
//     toast.  Money is likewise a real check, not a lockout: affordability is
//     lk.money MINUS the cost already COMMITTED to the pending job queue
//     (lkh_committed), so the player is told "you have $400, $1,250 is already
//     queued" rather than mysteriously going broke two minutes later.
//
//  5. THE "WHY" CHAIN (lkh_need_why) is the honest core made legible: for a
//     prisoner's worst need it walks §1's five conditions in order — is a room of
//     that type BUILT · is it VALID (and which object is missing) · is it
//     REACHABLE from where this prisoner stands · is there a FREE SLOT · what
//     does the REGIME permit right now — and reports the first one that fails,
//     in words.  It uses only contract queries (lk_room_find / lk_room[].missing
//     / lk_reachable / lk_room_free_slot / lk_nearest), so it cannot drift from
//     what the sim actually does.  This is why every number in the inspector can
//     be explained by pointing at a tile.
//
// ── IMPLEMENTATION NOTES ────────────────────────────────────────────────────
//  A. NOT IN THE CONTRACT — needed, worked around, please reconcile:
//     A1. `void lk_set_zone(int c, int zone)` does not exist, and Tile.zone has
//         no other mutator, so lkh_set_zone() writes lk_t[c].zone DIRECTLY (the
//         one place this module touches a tile field).  path.h folds zone into
//         its rolling permission signature, so the change is picked up within a
//         few frames on its own — no dirty flag needed.  Please add the setter.
//     A2. lk_queue() takes no facing, and lk_job_progress() reads the facing back
//         out of Tile.obj_dir when the job completes — so a rotated object can
//         only be designated by writing lk_t[c].obj_dir before/after the queue
//         call (lkh_designate does exactly that, and only for JB_OBJECT).
//         Cleanest fix: `lk_queue_obj(c, ob, dir)`.
//     A3. Actor has no name.  lkh_name() mints a stable one from the actor INDEX
//         plus lk.seed out of two 16-word tables.  If actors ever grows a real
//         name, delete lkh_name and read it.
//     A4. actors owns the need↔activity permission table (lka_permits, static).
//         The HUD never asserts "not permitted"; it only ever reports FACTS from
//         lk_regime[] — the current activity, and the next hour that schedules
//         the activity which serves a need (LKH_NEED_ACT).  So a mismatch with
//         actors' internal table can never make the UI lie.
//     A5. econ has lk_econ_net()/lk_econ_why()/lk_econ_forecast(), but they are
//         not in the contract, so the finance panel re-sums lk_econ_line()
//         itself.  Swap to lk_econ_net() once it's promoted.
//  B. ui.h OWNERSHIP: this module calls ui_begin() FIRST and ui_end() LAST inside
//     lk_hud_draw().  ⚠ THE CART MUST NOT ALSO CALL THEM — a second ui_begin()
//     in the same frame discards the recorded presses and every widget in the
//     HUD goes dead-but-hovering (ui.h's own documented trap).  Corollary: call
//     lk_hud_draw() exactly once per frame, after the world.
//  C. GHOSTS: lk_art_ghost(c, job, arg, ok) takes a tile and no camera, and
//     (VERIFIED against art.h) draws at raw world px — so a world camera must be
//     active.  lkh_draw_world_layer() therefore installs camera(lk_cam_x - view.x,
//     lk_cam_y - view.y), draws the ghosts + rubber-band + selection marks in that
//     transform, and resets to camera(0,0) before a single pixel of chrome.  This
//     is the same transform art.h documents for lk_art_world, so the ghost lands
//     on exactly the tile the highlight is on.  It also means lk_hud_draw is safe
//     to call whether or not the cart left a camera set.
//  D. WALL/ZONE swatch colours in the pickers and the overlay legend are
//     HUD-LOCAL (LKH_WALL_COL / LKH_ZONE_COL): art owns how walls and zones
//     actually look and the contract doesn't publish those colours.  Floors,
//     objects, rooms and needs all draw from the frozen tables, so only these two
//     can disagree with the map — point them at art's constants if it exports any.
//  E. lk_hud_update(d) wants the REAL frame delta, not the sim-scaled one, so the
//     camera and the toasts keep working while paused.  It clamps d defensively
//     and substitutes 1/60 for a zero, so passing a paused dt merely makes
//     panning feel slightly wrong rather than freezing it.
//  F. lk.speed is written here (the pause/1x/2x/4x buttons and SPACE) — the cart
//     owns the field but the player owns the control.  lk_sel_actor / lk_sel_room
//     are re-validated every update (a prisoner can die and a room can be
//     demolished while its inspector is open), so other modules may read them
//     without checking.  lk_sel_room == 0 means "nothing picked" — room ids start
//     at 1, matching Tile.room.
//  G. KEYS CLAIMED (a key the cart reads is the cart's key): 1-8 · space · tab ·
//     O · R · [ ] · T · F · H · / · L · X · C · esc · WASD · arrows · both shifts
//     (raylib 340/344, pan accelerate).  Anything else is free for the cart.
//  H. The argument picker is capped at 4 rows and half the viewport height, so on
//     a very short window the tail of the 24-object list is not drawn.  [ and ]
//     always reach every choice, and the toolbar spells the current one out — so
//     nothing is unreachable, only unclicked.  A phone in portrait gets a
//     two-row toolbar and short labels (LKH_NARROW_W / LKH_COMPACT_W).
// ─────────────────────────────────────────────────────────────────────────────
#ifndef LOCKUP_HUD_H
#define LOCKUP_HUD_H

#include "studio.h"
#include "lockup/model.h"

// ui.h tunables — set BEFORE the include so the shared widgets wear the prison's
// institutional palette instead of the default synth-rack blues.  Guarded: if
// another module already pulled ui.h in, its defaults stand (harmless — every
// button face in here is overdrawn by lkh_btn anyway).
#ifndef UI_MAX_WID
#define UI_MAX_WID     128       // the object picker alone can register 24
#endif
#ifndef UI_COL_BG
#define UI_COL_BG      CLR_DARKER_GREY
#endif
#ifndef UI_COL_FILL
#define UI_COL_FILL    CLR_DARK_GREY
#endif
#ifndef UI_COL_FILL_HOT
#define UI_COL_FILL_HOT CLR_BROWN
#endif
#ifndef UI_COL_FRAME
#define UI_COL_FRAME   CLR_DARK_GREY
#endif
#ifndef UI_COL_TEXT
#define UI_COL_TEXT    CLR_LIGHT_GREY
#endif
#ifndef UI_COL_TEXT_HOT
#define UI_COL_TEXT_HOT CLR_WHITE
#endif
#ifndef UI_COL_FOCUS
#define UI_COL_FOCUS   CLR_LIGHT_YELLOW
#endif
#include "lay.h"
#include "ui.h"

// ═══ the globals this module owns ════════════════════════════════════════════
int lk_tool = TL_SELECT, lk_tool_arg = FL_CONCRETE;
int lk_overlay = OV_NONE;
int lk_cam_x = 0, lk_cam_y = 0;
int lk_sel_actor = -1;
int lk_sel_room  = 0;                    // room ids start at 1; 0 = nothing picked

// ═══ tunables ════════════════════════════════════════════════════════════════
#define LKH_CLICK_SLOP    5      // px of travel still counted as a click, not a drag
#define LKH_PAN_KEY     260.0f   // camera px/s on arrows/WASD
#define LKH_PAN_FAST      2.6f   // …times this while shift is held
#define LKH_EDGE          10     // edge-scroll band inside the viewport, px
#define LKH_EDGE_SPD    340.0f   // edge-scroll px/s at the very edge
#define LKH_BAND_MAX      48     // biggest rubber-band side, tiles (queue guard)
#define LKH_GHOST_MAX    420     // ghost tiles drawn before we fall back to an outline
#define LKH_TOASTS         4
#define LKH_TOAST_LEN     56
#define LKH_TOAST_SECS   4.2f
#define LKH_COMPACT_W    560     // below this: short labels
#define LKH_NARROW_W     430     // below this: the toolbar stacks two rows

// chrome palette — warm institutional greys, cream text, amber accent
#define LKH_C_PANEL   CLR_BROWNISH_BLACK
#define LKH_C_RAISE   CLR_DARKER_GREY
#define LKH_C_FRAME   CLR_DARK_GREY
#define LKH_C_EDGE    CLR_MEDIUM_GREY
#define LKH_C_TEXT    CLR_LIGHT_GREY
#define LKH_C_HI      CLR_WHITE
#define LKH_C_DIM     CLR_DARK_GREY
#define LKH_C_ACC     CLR_LIGHT_YELLOW
#define LKH_C_GOOD    CLR_MEDIUM_GREEN
#define LKH_C_WARN    CLR_ORANGE
#define LKH_C_BAD     CLR_RED

// which panel is open (only one of these at a time; the inspector is separate)
enum { LKH_P_NONE = 0, LKH_P_REGIME, LKH_P_FINANCE, LKH_P_HELP };
// drag modes / sources
enum { LKH_D_NONE = 0, LKH_D_BUILD, LKH_D_ERASE, LKH_D_PAN };
enum { LKH_S_TOUCH = 0, LKH_S_RBTN, LKH_S_MBTN };
#define LKH_NOID 0x7fffffff

// ═══ HUD-local tables ════════════════════════════════════════════════════════
static const char *const LKH_TOOL_NAME[TL_COUNT] = {
    "SELECT", "FLOOR", "WALL", "DOOR", "OBJECT", "ROOM", "ZONE", "DEMOLISH"
};
static const char *const LKH_TOOL_SHORT[TL_COUNT] = {
    "SEL", "FLR", "WAL", "DOR", "OBJ", "ROM", "ZON", "DEM"
};
static const char *const LKH_TOOL_TIP[TL_COUNT] = {
    "click a prisoner or a room to inspect it; drag to pan",
    "drag a rectangle of paving - workmen lay it, you pay on delivery",
    "drag a rectangle: its OUTLINE becomes wall, so one drag encloses a room",
    "click a wall tile to cut a doorway into it (R rotates nothing - doors join)",
    "click to place; R rotates; drag to place a run of them",
    "paint an INTENT over floor - the room is then DISCOVERED by flood fill",
    "drag to mark deployment: open, staff only, or secure",
    "drag a rectangle to tear structure down; some of the money comes back"
};
static const unsigned char LKH_TOOL_JOB[TL_COUNT] = {
    JB_NONE, JB_FLOOR, JB_WALL, JB_DOOR, JB_OBJECT, JB_NONE, JB_NONE, JB_DEMOLISH
};
static const char *const LKH_ZONE_NAME[ZN_COUNT] = { "open", "staff only", "secure" };
static const char *const LKH_ZONE_TIP[ZN_COUNT] = {
    "anyone may walk here",
    "prisoners are kept out of staff ground",
    "the secure core - max-security movement is escorted"
};
static const unsigned char LKH_ZONE_COL[ZN_COUNT] = {   // HUD-local (note D)
    CLR_DARK_GREY, CLR_TRUE_BLUE, CLR_DARK_RED
};
static const unsigned char LKH_WALL_COL[WL_COUNT] = {   // HUD-local (note D)
    CLR_BLACK, CLR_BROWN, CLR_MEDIUM_GREY, CLR_DARK_GREY, CLR_DARKER_GREY
};
static const char *const LKH_WALL_TIP[WL_COUNT] = {
    "", "cheap and warm - the everyday interior wall",
    "solid: what you want around a cell block",
    "you can see through it and it stops nobody - yard boundaries",
    "the outer skin. Expensive, and the last thing between them and out there"
};
static const char *const LKH_DOOR_TIP[DR_COUNT] = {
    "", "anyone opens it, always",
    "prisoners only pass when it is UNLOCKED - lockdown shuts every one",
    "staff only. Prisoners never come through",
    "the vehicle gate - wide, slow, and locked with the jail doors"
};
static const char *const LKH_JOB_NAME[JB_COUNT] = {
    "", "floor", "wall", "door", "object", "demolition"
};
static const char *const LKH_GRADE_NAME[5] = {
    "Safety", "Hygiene", "Food", "Recreation", "Reform"
};
static const char *const LKH_OVL_NAME[OV_COUNT] = {
    "none", "rooms", "deploy", "needs", "unrest"
};
static const char *const LKH_OVL_TIP[OV_COUNT] = {
    "no overlay - just the prison",
    "every room tinted by type; hatched = invalid, and the inspector says why",
    "the deployment zones you painted with the ZONE tool",
    "how badly needs are going unmet, per block - green content, red desperate",
    "local unrest: volatility plus the memory of violence. Red is where it starts"
};

// the FF_* flow-field a need is served by, so the why-chain can ask the path
// module "is there a free one of these reachable from here?"  ND_COUNT = no field.
static const signed char LKH_NEED_FF[ND_COUNT] = {
    FF_BED, FF_SERVING, FF_TOILET, FF_SHOWER, FF_YARD,
    FF_PHONE, FF_BENCH, -1, -1
};
// the regime activity that SCHEDULES a need (fact-only, see note A4). -1 = any hour.
static const signed char LKH_NEED_ACT[ND_COUNT] = {
    AC_SLEEP, AC_EAT, -1, AC_SHOWER, AC_YARD, AC_FREE, -1, -1, AC_SLEEP
};

// picker order: grouped the way a builder thinks, not the way the enum happens to
// run.  Anything missing from these lists is appended at init, so a new OB_*/RM_*
// can never become unbuildable by being forgotten here.
static const unsigned char LKH_OBJ_ORDER[] = {
    OB_BED, OB_BUNK, OB_TOILET, OB_SINK, OB_SHOWERHEAD,          // living
    OB_TABLE, OB_BENCH, OB_CHAIR, OB_SERVING, OB_COOKER, OB_FRIDGE,  // feeding
    OB_TV, OB_POOLTABLE, OB_WEIGHTS, OB_BOOKSHELF, OB_PHONE,     // recreation
    OB_DESK, OB_CABINET, OB_LOCKER, OB_MEDBED,                   // staff
    OB_DETECTOR, OB_CCTV, OB_LIGHT, OB_BIN                       // fittings
};
static const unsigned char LKH_ROOM_ORDER[] = {
    RM_CELL, RM_DORM, RM_SOLITARY, RM_HOLDING,
    RM_CANTEEN, RM_KITCHEN, RM_SHOWER,
    RM_YARD, RM_COMMON, RM_WORKSHOP, RM_VISIT,
    RM_OFFICE, RM_STAFFROOM, RM_INFIRMARY, RM_STORE
};

static const char *const LKH_FIRST[16] = {
    "Ray", "Vic", "Sol", "Dex", "Marv", "Otis", "Gus", "Lenny",
    "Cliff", "Duke", "Hank", "Wes", "Nico", "Boyd", "Emmet", "Rudy"
};
static const char *const LKH_LAST[16] = {
    "Calloway", "Deacon", "Fitch", "Gower", "Hollis", "Kessler", "Lomax", "Mundy",
    "Novak", "Pike", "Quill", "Renn", "Sawyer", "Tate", "Vance", "Wilder"
};

// ═══ module state ════════════════════════════════════════════════════════════
static Box lkh_scr, lkh_top, lkh_bar, lkh_v, lkh_pick, lkh_pan, lkh_ins;
static int lkh_compact, lkh_narrow, lkh_barrows;
static float lkh_floor_y;                 // lowest y a floating side panel may reach

static int   lkh_panel;                   // LKH_P_*
static int   lkh_arg[TL_COUNT];           // the remembered argument per tool
static int   lkh_obj_dir;                 // 0..3 facing for the OBJECT tool
static unsigned char lkh_objord[OB_COUNT]; static int lkh_nobjord;
static unsigned char lkh_roomord[RM_COUNT]; static int lkh_nroomord;

static int   lkh_dmode, lkh_dsrc, lkh_did = LKH_NOID;
static int   lkh_ax, lkh_ay, lkh_bx, lkh_by;   // anchor + current, canvas px
static int   lkh_ac = -1;                      // anchor TILE — the band survives a
                                               // mid-drag edge-scroll, which the raw
                                               // canvas anchor would not
static int   lkh_pan0x, lkh_pan0y;             // camera when the pan began
static int   lkh_moved, lkh_last_place = -1;

static int   lkh_seen[16], lkh_nseen;
static int   lkh_rgid = LKH_NOID;              // contact painting the regime grid
static int   lkh_brush = AC_FREE;               // regime paint activity
static Box   lkh_rg_grid;                       // regime hour grid, for hit-testing

static char  lkh_toast[LKH_TOASTS][LKH_TOAST_LEN];
static float lkh_toast_t[LKH_TOASTS];
static int   lkh_toast_n[LKH_TOASTS];
static int   lkh_ntoast;

static int   lkh_committed;               // $ already promised to the job queue
static int   lkh_day_open;                // lk.money when the day rolled over
static int   lkh_day_seen = -1;
static int   lkh_last_speed = 1;
static int   lkh_follow;                  // camera tracks lk_sel_actor
static int   lkh_fin_scroll;
static int   lkh_hover_c = -1;            // tile under the pointer, -1 = off-map
static int   lkh_mouse_seen;              // a real mouse has moved (gates edge-scroll)
static int   lkh_mx_prev = -9999, lkh_my_prev = -9999;
static int   lkh_multi_prev, lkh_multi_x, lkh_multi_y;
static char  lkh_tipb1[96], lkh_tipb2[96];   // tooltip for this frame, COPIED:
static int   lkh_tip_on;                     // str()'s ring is recycled long before
static float lkh_pulse;                      // the tip is drawn at end of frame

// ═══ small helpers ═══════════════════════════════════════════════════════════
static void lkh_fnt(int f) { font(f); }

// a light palette entry needs dark ink on top of it
static int lkh_ink(int bg) {
    switch (bg) {
    case CLR_WHITE: case CLR_LIGHT_GREY: case CLR_LIGHT_PEACH: case CLR_MEDIUM_GREY:
    case CLR_YELLOW: case CLR_LIGHT_YELLOW: case CLR_ORANGE: case CLR_GREEN:
    case CLR_LIME_GREEN: case CLR_BLUE: case CLR_PINK: case CLR_PEACH:
    case CLR_DARK_PEACH:
        return CLR_BROWNISH_BLACK;
    default:
        return CLR_WHITE;
    }
}

// copy a transient string (a str() ring slot) somewhere it will still be there later
static const char *lkh_keep(const char *s) {
    static char buf[128];
    int i = 0;
    while (s && s[i] && i < (int)sizeof(buf) - 1) { buf[i] = s[i]; i++; }
    buf[i] = 0;
    return buf;
}

// trim text to fit `maxw` px in the ACTIVE font, appending ".." when cut.
// Three rotating buffers so two fitted strings can be alive at once.
static const char *lkh_fit(const char *s, int maxw) {
    static char bufs[3][96];
    static int which = 0;
    char *buf = bufs[which];
    which = (which + 1) % 3;
    int n = 0;
    while (s[n] && n < 95) n++;
    for (int i = 0; i <= n; i++) buf[i] = s[i];
    if (text_width(buf) <= maxw) return buf;
    while (n > 2) {
        buf[n - 1] = '.'; buf[n] = '.'; buf[n + 1] = 0;
        if (text_width(buf) <= maxw) return buf;
        buf[n - 1] = 0; n--;
    }
    buf[0] = 0;
    return buf;
}

// $12,400 — thousands separated, because a prison budget is read at a glance.
// Four rotating buffers: one toast line prints three different sums.
static const char *lkh_money(int v) {
    static char bufs[4][20];
    static int which = 0;
    char *buf = bufs[which];
    which = (which + 1) % 4;
    char d[12];
    int neg = v < 0, n = 0, o = 0;
    unsigned int a = (unsigned int)(neg ? -v : v);
    do { d[n++] = (char)('0' + (a % 10u)); a /= 10u; } while (a && n < 11);
    if (neg) buf[o++] = '-';
    buf[o++] = '$';
    for (int i = n - 1; i >= 0; i--) {
        buf[o++] = d[i];
        if (i && (i % 3) == 0) buf[o++] = ',';
    }
    buf[o] = 0;
    return buf;
}

static int lkh_hour(void) {
    int h = (int)lk.clock;
    return h < 0 ? 0 : (h > 23 ? 23 : h);
}
static const char *lkh_clock(void) {
    int h = lkh_hour(), m = (int)((lk.clock - (float)h) * 60.0f);
    if (m < 0) m = 0; if (m > 59) m = 59;
    return str("%02d:%02d", h, m);
}

// green → amber → red, for anything where "more" is worse
static int lkh_heat(float v) {
    return v < 0.34f ? LKH_C_GOOD : (v < 0.67f ? LKH_C_WARN : LKH_C_BAD);
}

static void lkh_shadow(Box b) {
    fillp(FILL_CHECKER, -1);
    rectfill((int)b.x + 3, (int)b.y + 3, (int)b.w, (int)b.h, CLR_BLACK);
    fillp_reset();
}

// ═══ toasts ══════════════════════════════════════════════════════════════════
void lk_hud_toast(const char *msg) {
    if (!msg || !msg[0]) return;
    if (lkh_ntoast > 0) {                       // same line twice = a counter, not a stack
        char *top = lkh_toast[lkh_ntoast - 1];
        int same = 1;
        for (int i = 0; i < LKH_TOAST_LEN; i++) {
            if (top[i] != msg[i]) { same = 0; break; }
            if (!top[i]) break;
        }
        if (same) { lkh_toast_t[lkh_ntoast - 1] = LKH_TOAST_SECS;
                    lkh_toast_n[lkh_ntoast - 1]++; return; }
    }
    if (lkh_ntoast >= LKH_TOASTS) {             // scroll the oldest off
        for (int i = 1; i < LKH_TOASTS; i++) {
            for (int c = 0; c < LKH_TOAST_LEN; c++) lkh_toast[i - 1][c] = lkh_toast[i][c];
            lkh_toast_t[i - 1] = lkh_toast_t[i];
            lkh_toast_n[i - 1] = lkh_toast_n[i];
        }
        lkh_ntoast = LKH_TOASTS - 1;
    }
    int i = lkh_ntoast++;
    int k = 0;
    while (msg[k] && k < LKH_TOAST_LEN - 1) { lkh_toast[i][k] = msg[k]; k++; }
    lkh_toast[i][k] = 0;
    lkh_toast_t[i] = LKH_TOAST_SECS;
    lkh_toast_n[i] = 1;
}
static void lkh_toasts_step(float d) {
    for (int i = 0; i < lkh_ntoast; i++) lkh_toast_t[i] -= d;
    int w = 0;
    for (int i = 0; i < lkh_ntoast; i++) {
        if (lkh_toast_t[i] <= 0) continue;
        if (w != i) {
            for (int c = 0; c < LKH_TOAST_LEN; c++) lkh_toast[w][c] = lkh_toast[i][c];
            lkh_toast_t[w] = lkh_toast_t[i];
            lkh_toast_n[w] = lkh_toast_n[i];
        }
        w++;
    }
    lkh_ntoast = w;
}

// ═══ tool arguments ══════════════════════════════════════════════════════════
static int lkh_arg_n(int tool) {
    switch (tool) {
    case TL_FLOOR:  return FL_COUNT;
    case TL_WALL:   return WL_COUNT - 1;
    case TL_DOOR:   return DR_COUNT - 1;
    case TL_OBJECT: return lkh_nobjord;
    case TL_ROOM:   return lkh_nroomord;
    case TL_ZONE:   return ZN_COUNT;
    default:        return 0;
    }
}
static int lkh_arg_val(int tool, int i) {
    int n = lkh_arg_n(tool);
    if (n <= 0) return 0;
    if (i < 0) i = 0; if (i >= n) i = n - 1;
    switch (tool) {
    case TL_FLOOR:  return i;
    case TL_WALL:   return i + 1;
    case TL_DOOR:   return i + 1;
    case TL_OBJECT: return lkh_objord[i];
    case TL_ROOM:   return lkh_roomord[i];
    case TL_ZONE:   return i;
    default:        return 0;
    }
}
static int lkh_arg_slot(int tool, int val) {         // value → picker index
    int n = lkh_arg_n(tool);
    for (int i = 0; i < n; i++) if (lkh_arg_val(tool, i) == val) return i;
    return 0;
}
static const char *lkh_arg_name(int tool, int val) {
    switch (tool) {
    case TL_FLOOR:  return (val >= 0 && val < FL_COUNT) ? LK_FLOOR_NAME[val] : "?";
    case TL_WALL:   return (val > 0 && val < WL_COUNT)  ? LK_WALL_NAME[val]  : "?";
    case TL_DOOR:   return (val > 0 && val < DR_COUNT)  ? LK_DOOR_NAME[val]  : "?";
    case TL_OBJECT: return (val > 0 && val < OB_COUNT)  ? LK_OBJ[val].name   : "?";
    case TL_ROOM:   return (val > 0 && val < RM_COUNT)  ? LK_ROOM[val].name  : "?";
    case TL_ZONE:   return (val >= 0 && val < ZN_COUNT) ? LKH_ZONE_NAME[val] : "?";
    default:        return "";
    }
}
static int lkh_arg_cost(int tool, int val) {
    switch (tool) {
    case TL_FLOOR:  return (val >= 0 && val < FL_COUNT) ? LK_FLOOR_COST[val] : 0;
    case TL_WALL:   return (val > 0 && val < WL_COUNT)  ? LK_WALL_COST[val]  : 0;
    case TL_DOOR:   return (val > 0 && val < DR_COUNT)  ? LK_DOOR_COST[val]  : 0;
    case TL_OBJECT: return (val > 0 && val < OB_COUNT)  ? LK_OBJ[val].cost   : 0;
    default:        return 0;                          // rooms + zones are free to mark
    }
}
static void lkh_set_tool(int t) {
    if (t < 0 || t >= TL_COUNT) return;
    if (t != lk_tool) lk_sfx(SFX_CLICK);
    lk_tool = t;
    lk_tool_arg = lkh_arg[t];
    if (lkh_arg_n(t) > 0) lk_tool_arg = lkh_arg_val(t, lkh_arg_slot(t, lk_tool_arg));
}
static void lkh_set_arg(int val) {
    lk_tool_arg = val;
    lkh_arg[lk_tool] = val;
    lk_sfx(SFX_CLICK);
}
static void lkh_cycle_arg(int dir) {
    int n = lkh_arg_n(lk_tool);
    if (n <= 0) return;
    int i = lkh_arg_slot(lk_tool, lk_tool_arg) + dir;
    i = (i % n + n) % n;
    lkh_set_arg(lkh_arg_val(lk_tool, i));
}

// ═══ money: what is already promised to the queue ════════════════════════════
static int lkh_job_cost(int job, int arg) {
    switch (job) {
    case JB_FLOOR:  return (arg >= 0 && arg < FL_COUNT) ? LK_FLOOR_COST[arg] : 0;
    case JB_WALL:   return (arg >= 0 && arg < WL_COUNT) ? LK_WALL_COST[arg]  : 0;
    case JB_DOOR:   return (arg >= 0 && arg < DR_COUNT) ? LK_DOOR_COST[arg]  : 0;
    case JB_OBJECT: return (arg >= 0 && arg < OB_COUNT) ? LK_OBJ[arg].cost   : 0;
    default:        return 0;                          // demolition pays back, never out
    }
}
static void lkh_recount_committed(void) {
    int sum = 0;
    for (int c = 0; c < LK_N; c++)
        if (lk_t[c].job != JB_NONE) sum += lkh_job_cost(lk_t[c].job, lk_t[c].job_arg);
    lkh_committed = sum;
}
static int lkh_budget(void) { return lk.money - lkh_committed; }
static int lkh_afford(int cost) { return cost <= 0 || lkh_budget() >= cost; }

// ═══ camera + pointer→world ══════════════════════════════════════════════════
static int lkh_cam_maxx(void) { int m = LK_WPX - (int)lkh_v.w; return m < 0 ? 0 : m; }
static int lkh_cam_maxy(void) { int m = LK_HPX - (int)lkh_v.h; return m < 0 ? 0 : m; }
static void lkh_cam_clamp(void) {
    if (lk_cam_x < 0) lk_cam_x = 0;
    if (lk_cam_y < 0) lk_cam_y = 0;
    if (lk_cam_x > lkh_cam_maxx()) lk_cam_x = lkh_cam_maxx();
    if (lk_cam_y > lkh_cam_maxy()) lk_cam_y = lkh_cam_maxy();
}
static void lkh_cam_center(float wx, float wy) {
    lk_cam_x = (int)(wx - lkh_v.w * 0.5f);
    lk_cam_y = (int)(wy - lkh_v.h * 0.5f);
    lkh_cam_clamp();
}
// the ONE canvas→world conversion (key algorithm 2)
static int lkh_pt_tile(int cx, int cy) {
    if (!binside(lkh_v, cx, cy)) return -1;
    float wx = (float)(cx - (int)lkh_v.x + lk_cam_x);
    float wy = (float)(cy - (int)lkh_v.y + lk_cam_y);
    if (wx < 0 || wy < 0 || wx >= (float)LK_WPX || wy >= (float)LK_HPX) return -1;
    return lk_cell_at(wx, wy);
}
static float lkh_pt_wx(int cx) { return (float)(cx - (int)lkh_v.x + lk_cam_x); }
static float lkh_pt_wy(int cy) { return (float)(cy - (int)lkh_v.y + lk_cam_y); }
// world → canvas, for the marks this module draws itself
static void lkh_world_cam(void) { camera(lk_cam_x - (int)lkh_v.x, lk_cam_y - (int)lkh_v.y); }

// ═══ layout ══════════════════════════════════════════════════════════════════
static void lkh_relayout(void) {
    int sw = screen_w(), sh = screen_h();
    int sx = 0, sy = 0, sww = sw, shh = sh;
    safe_rect(&sx, &sy, &sww, &shh);
    if (sww <= 0) { sx = 0; sww = sw; }
    if (shh <= 0) { sy = 0; shh = sh; }
    lkh_scr = box(0, 0, (float)sw, (float)sh);
    Box safe = box((float)sx, (float)sy, (float)sww, (float)shh), rest;

    lkh_compact = sww < LKH_COMPACT_W;
    lkh_narrow  = sww < LKH_NARROW_W;
    lkh_barrows = lkh_narrow ? 2 : 1;

    float th = lay_clamp((float)shh * 0.055f, 18, 24);
    float rowh = lay_clamp((float)shh * 0.070f, 24, 30);
    float bh = rowh * (float)lkh_barrows;
    if (bh > (float)shh * 0.34f) bh = (float)shh * 0.34f;   // never eat the map

    lkh_top = lay_split(safe, EDGE_TOP, th, &rest);
    lkh_bar = lay_split(rest, EDGE_BOTTOM, bh, &rest);
    lkh_v   = rest;
    if (lkh_v.h < 40) lkh_v.h = 40;

    // ── the argument picker floats over the bottom of the map ──
    lkh_pick = box(0, 0, 0, 0);
    int an = lkh_arg_n(lk_tool);
    if (an > 0 && lkh_panel != LKH_P_REGIME && lkh_panel != LKH_P_HELP) {
        float chipw = lkh_compact ? 58.0f : 78.0f;
        Box inner = lay_inset(lkh_v, 4);
        if (inner.w < chipw) chipw = inner.w;
        int cols = lay_wrap_cols(inner, an, chipw, 2);
        int rows = (an + cols - 1) / cols;
        if (rows > 4) rows = 4;                      // taller than this and it is a wall
        float ph = 13.0f + (float)rows * 22.0f;
        if (ph > lkh_v.h * 0.5f) ph = lkh_v.h * 0.5f;
        lkh_pick = box(lkh_v.x + 4, lkh_v.y + lkh_v.h - 4 - ph, lkh_v.w - 8, ph);
    }

    // ── the one open panel ──
    lkh_pan = box(0, 0, 0, 0);
    lkh_rg_grid = box(0, 0, 0, 0);
    if (lkh_panel == LKH_P_REGIME) {
        float pw = lkh_v.w - 8, ph = lay_clamp(lkh_v.h * 0.30f, 86, 116);
        lkh_pan = box(lkh_v.x + 4, lkh_v.y + lkh_v.h - 4 - ph, pw, ph);
        // the hour grid is solved HERE, not in draw, so the tile the finger paints
        // and the cell it lights up can never be a frame apart
        Box rg = lay_pad(lkh_pan, 15, 4, 4, 4), r2;
        lay_split(rg, EDGE_TOP,    14, &r2); rg = r2;    // brush row
        lay_split(rg, EDGE_BOTTOM,  8, &r2); rg = r2;    // warning line
        lay_split(rg, EDGE_TOP,     7, &r2); rg = r2;    // hour labels
        if (rg.h > 26) rg.h = 26;
        if (rg.h < 8)  rg.h = 8;
        lkh_rg_grid = rg;
    } else if (lkh_panel == LKH_P_FINANCE) {
        float pw = lay_clamp(lkh_v.w * 0.46f, 150, 250);
        lkh_pan = box(lkh_v.x + 4, lkh_v.y + 4, pw, 0);
    } else if (lkh_panel == LKH_P_HELP) {
        float pw = lay_clamp(lkh_v.w - 16, 180, 372);
        float ph = lay_clamp(lkh_v.h - 16, 110, 206);
        lkh_pan = lay_at(lkh_v, L_C, pw, ph, 0);
    }

    // ── how far down a side panel may run ──
    lkh_floor_y = lkh_v.y + lkh_v.h - 4;
    if (lkh_pick.h > 0 && lkh_pick.y - 3 < lkh_floor_y) lkh_floor_y = lkh_pick.y - 3;
    if (lkh_panel == LKH_P_REGIME && lkh_pan.y - 3 < lkh_floor_y) lkh_floor_y = lkh_pan.y - 3;
    if (lkh_panel == LKH_P_FINANCE) {
        lkh_pan.h = lkh_floor_y - lkh_pan.y;
        if (lkh_pan.h < 60) lkh_pan.h = 60;
    }

    // ── the inspector, right-docked ──
    lkh_ins = box(0, 0, 0, 0);
    int show_a = lk_sel_actor >= 0 && lk_sel_actor < LK_MAXACT && lk_a[lk_sel_actor].alive;
    int show_r = !show_a && lk_sel_room > 0 && lk_sel_room < LK_MAXROOM &&
                 lk_room[lk_sel_room].type != RM_NONE;
    if ((show_a || show_r) && lkh_panel != LKH_P_HELP) {
        float pw = lay_clamp(lkh_v.w * 0.40f, 132, 180);
        float ph = show_a ? 198.0f : 116.0f;
        float avail = lkh_floor_y - (lkh_v.y + 4);
        if (ph > avail) ph = avail;
        if (ph < 52) ph = 52;
        lkh_ins = box(lkh_v.x + lkh_v.w - 4 - pw, lkh_v.y + 4, pw, ph);
    }
    lkh_cam_clamp();
}

void lk_hud_viewport(int *x, int *y, int *w, int *h) {
    lkh_relayout();
    if (x) *x = (int)lkh_v.x;
    if (y) *y = (int)lkh_v.y;
    if (w) *w = (int)lkh_v.w;
    if (h) *h = (int)lkh_v.h;
}

// is this canvas point over chrome (so the map must ignore it)?
static int lkh_blocked(int x, int y) {
    if (!binside(lkh_v, x, y)) return 1;
    if (lkh_pick.h > 0 && binside(lkh_pick, x, y)) return 1;
    if (lkh_pan.h  > 0 && binside(lkh_pan,  x, y)) return 1;
    if (lkh_ins.h  > 0 && binside(lkh_ins,  x, y)) return 1;
    return 0;
}

// ═══ chrome primitives ═══════════════════════════════════════════════════════
static Box lkh_frame(Box b, const char *title, int accent) {
    lkh_shadow(b);
    boxfill(b, LKH_C_PANEL);
    rectfill((int)b.x + 1, (int)b.y + 1, (int)b.w - 2, 11, LKH_C_RAISE);
    boxrect(b, LKH_C_FRAME);
    line((int)b.x + 1, (int)b.y + 12, (int)b.x + (int)b.w - 2, (int)b.y + 12, LKH_C_FRAME);
    lkh_fnt(FONT_SMALL);
    print(lkh_fit(title, (int)b.w - 32), (int)b.x + 5, (int)b.y + 4, accent);
    return lay_pad(b, 15, 4, 4, 4);
}

// tooltips are COPIED, not referenced: they are drawn at the very end of the
// frame, hundreds of str() calls after the hover that set them.
static void lkh_tip(const char *a, const char *b) {
    int i = 0;
    while (a && a[i] && i < 95) { lkh_tipb1[i] = a[i]; i++; }
    lkh_tipb1[i] = 0;
    i = 0;
    while (b && b[i] && i < 95) { lkh_tipb2[i] = b[i]; i++; }
    lkh_tipb2[i] = 0;
    lkh_tip_on = 1;
}

// the ONE button: ui.h owns capture / fat-finger pads / focus / the audit hook,
// this owns the face.  `enabled == 0` still registers and still reports the click
// so the caller can say WHY — a dead-looking button that explains itself teaches.
static int lkh_btn(Box b, const char *label, int sel, int enabled, const char *tip) {
    int x = (int)b.x, y = (int)b.y, w = (int)b.w, h = (int)b.h;
    if (w < 6 || h < 6) return 0;
    int hit = ui_button(x, y, w, h, NULL);
    int hot = binside(b, mouse_x(), mouse_y());
    int fill = sel ? CLR_BROWN : (hot && enabled ? LKH_C_FRAME : LKH_C_RAISE);
    if (!enabled) fill = CLR_DARKER_BLUE;
    rectfill(x + 1, y + 1, w - 2, h - 2, fill);
    rect(x, y, w, h, sel ? LKH_C_ACC : (hot ? LKH_C_EDGE : LKH_C_FRAME));
    if (label && label[0]) {
        int col = !enabled ? LKH_C_DIM : (sel ? CLR_WHITE : (hot ? LKH_C_HI : LKH_C_TEXT));
        lkh_fnt(FONT_SMALL);
        const char *t = lkh_fit(label, w - 4);
        print(t, x + (w - text_width(t)) / 2, y + (h - 6) / 2, col);
    }
    if (hot && tip) lkh_tip(tip, NULL);
    return hit;
}

// a labelled 0..1 meter with the number spelled out — never a bare bar
static void lkh_meter(int x, int y, int w, const char *lab, float v, int col, int pct) {
    lkh_fnt(FONT_SMALL);
    int lw = lab ? text_width(lab) + 2 : 0;
    if (lab) print(lab, x, y, LKH_C_TEXT);
    int bw = w - lw - (pct ? 18 : 0);
    if (bw < 6) bw = 6;
    bar(x + lw, y, bw, 5, v, col, CLR_DARKER_BLUE);
    rect(x + lw, y, bw, 5, LKH_C_FRAME);
    if (pct) print(str("%d%%", (int)(v * 100.0f + 0.5f)), x + lw + bw + 2, y, LKH_C_DIM);
}

static void lkh_pips(int x, int y, int n, int of, int col) {
    for (int i = 0; i < of; i++) {
        if (i < n) rectfill(x + i * 6, y, 5, 5, col);
        else       rect(x + i * 6, y, 5, 5, LKH_C_FRAME);
    }
}

// 12×12 procedural tool glyphs — no sprite budget, and they can't drift from art
static void lkh_glyph(int tool, int x, int y, int c) {
    switch (tool) {
    case TL_SELECT:
        line(x + 2, y + 1, x + 2, y + 10, c);
        line(x + 2, y + 1, x + 9, y + 8, c);
        line(x + 2, y + 10, x + 6, y + 8, c);
        line(x + 6, y + 8, x + 9, y + 8, c);
        break;
    case TL_FLOOR:
        for (int gy = 0; gy < 3; gy++) for (int gx = 0; gx < 3; gx++)
            if (((gx + gy) & 1) == 0) rectfill(x + 1 + gx * 4, y + 1 + gy * 4, 3, 3, c);
        rect(x, y, 13, 13, CLR_DARKER_GREY);
        break;
    case TL_WALL:
        rectfill(x + 1, y + 2, 11, 3, c);
        rectfill(x + 1, y + 6, 11, 3, c);
        rectfill(x + 1, y + 10, 11, 2, c);
        pset(x + 4, y + 2, LKH_C_PANEL); pset(x + 4, y + 3, LKH_C_PANEL);
        pset(x + 8, y + 6, LKH_C_PANEL); pset(x + 8, y + 7, LKH_C_PANEL);
        break;
    case TL_DOOR:
        rect(x + 2, y + 1, 9, 11, c);
        line(x + 3, y + 2, x + 3, y + 10, c);
        pset(x + 8, y + 6, c); pset(x + 8, y + 7, c);
        break;
    case TL_OBJECT:                                     // a bed, read side-on
        rectfill(x + 1, y + 4, 11, 7, c);
        rectfill(x + 2, y + 2, 4, 3, c);
        rect(x + 1, y + 4, 11, 7, CLR_DARKER_GREY);
        break;
    case TL_ROOM:
        for (int i = 0; i < 12; i += 3) {
            line(x + i, y, x + i + 1, y, c);
            line(x + i, y + 11, x + i + 1, y + 11, c);
            line(x, y + i, x, y + i + 1, c);
            line(x + 11, y + i, x + 11, y + i + 1, c);
        }
        pset(x + 5, y + 5, c); pset(x + 6, y + 6, c);
        break;
    case TL_ZONE:
        for (int i = 0; i < 3; i++) line(x + 1 + i * 4, y + 11, x + 6 + i * 4, y + 1, c);
        break;
    case TL_DEMOLISH:
        line(x + 1, y + 1, x + 10, y + 10, c);
        line(x + 2, y + 1, x + 11, y + 10, c);
        line(x + 10, y + 1, x + 1, y + 10, c);
        line(x + 11, y + 1, x + 2, y + 10, c);
        break;
    default: break;
    }
}

// ═══ names + the inspector's reasoning ═══════════════════════════════════════
static const char *lkh_name(int ai) {
    if (ai < 0 || ai >= LK_MAXACT) return "nobody";
    int s = ai * 7 + (lk.seed & 15);
    return str("%s %s", LKH_FIRST[s & 15], LKH_LAST[(ai * 11 + 5) & 15]);
}

static const char *lkh_sec_name(int s) {
    return s == SEC_MIN ? "min" : (s == SEC_MAX ? "MAX" : "normal");
}

// the next hour that schedules `act`, from the current one. -1 = never
static int lkh_next_act_hour(int act) {
    int h0 = lkh_hour();
    for (int k = 1; k <= 24; k++) {
        int h = (h0 + k) % 24;
        if (lk_regime[h] == act) return h;
    }
    return -1;
}

// the worst-need index, mirroring lk_need_worst's threshold so the name and the
// bar the UI highlights are always the same need
static int lkh_worst_need(int ai) {
    const Actor *a = &lk_a[ai];
    int w = -1; float wv = 0.12f;
    for (int n = 0; n < ND_COUNT; n++) if (a->need[n] > wv) { wv = a->need[n]; w = n; }
    return w;
}

// KEY ALGORITHM 5 — §1's five conditions, in order, in words.
static const char *lkh_need_why(int ai, int need) {
    if (ai < 0 || need < 0 || need >= ND_COUNT) return "";
    const Actor *a = &lk_a[ai];
    int here = lk_cell_at(a->x, a->y);
    const NeedDef *nd = &LK_NEED[need];

    if (need == ND_SAFETY) {
        int guards = 0;
        for (int i = 0; i < lk_nact; i++) {
            if (!lk_a[i].alive || lk_a[i].role != RL_GUARD) continue;
            float dx = (lk_a[i].x - a->x) / (float)LK_TS, dy = (lk_a[i].y - a->y) / (float)LK_TS;
            if (dx * dx + dy * dy < 100.0f) guards++;
        }
        if (guards == 0) return "no guard within sight of this block";
        return "still afraid - violence here is remembered";
    }
    if (need == ND_PRIVACY) {
        if (a->cell < 0) return "no cell of their own to retreat to";
        if (lk_room[a->cell].type == RM_DORM) return "sharing a dormitory - no privacy in one";
        return "away from their cell";
    }

    // 1 · does a room that serves this need exist at all?
    if (nd->room != RM_NONE) {
        int rid = lk_room_find(nd->room, 0);
        if (rid < 0) {
            // is there an INVALID one? then say what it needs — that is the fix.
            for (int i = 1; i < lk_nroom; i++) {
                const Room *r = &lk_room[i];
                if (r->type != nd->room || r->valid) continue;
                if (r->leaks) return str("the %s is not enclosed", LK_ROOM[nd->room].name);
                if (r->area < LK_ROOM[nd->room].min_area)
                    return str("the %s is too small (%d of %d tiles)", LK_ROOM[nd->room].name,
                               (int)r->area, (int)LK_ROOM[nd->room].min_area);
                if (r->missing) return str("the %s has no %s", LK_ROOM[nd->room].name,
                                          LK_OBJ[r->missing].name);
                return str("the %s is not valid yet", LK_ROOM[nd->room].name);
            }
            return str("no %s has been built", LK_ROOM[nd->room].name);
        }
        // 3 · reachable?
        int reach = -1;
        for (int nth = 0; nth < 8; nth++) {
            int r2 = lk_room_find(nd->room, nth);
            if (r2 < 0) break;
            if (!lk_in(lk_room[r2].cx, lk_room[r2].cy)) continue;
            if (lk_reachable(here, lk_idx(lk_room[r2].cx, lk_room[r2].cy), RL_PRISONER))
                { reach = r2; break; }
        }
        if (reach < 0) return str("the %s cannot be walked to from here", LK_ROOM[nd->room].name);
        // 5 · a free slot?
        int free_any = 0;
        for (int nth = 0; nth < 8; nth++) {
            int r2 = lk_room_find(nd->room, nth);
            if (r2 < 0) break;
            if (lk_room_free_slot(r2, need) >= 0) { free_any = 1; break; }
        }
        if (!free_any) {
            const Room *r = &lk_room[reach];
            int cobj = LK_ROOM[nd->room].cap_obj;
            if (cobj) return str("every %s is taken (%d of %d)", LK_OBJ[cobj].name,
                                 (int)r->used, (int)r->cap);
            return str("the %s is full", LK_ROOM[nd->room].name);
        }
    } else if (LKH_NEED_FF[need] >= 0) {
        // object-bound, room-free (a toilet, a bench): one flow-field query answers
        // built + reachable + free at once
        if (lk_nearest(here, LKH_NEED_FF[need], RL_PRISONER) < 0) {
            if (nd->obj) return str("no free %s within reach", LK_OBJ[nd->obj].name);
            return "nothing within reach that would help";
        }
    }

    // 4 · the regime.  FACTS ONLY (note A4): what it is now, when the slot comes.
    int act = lk_regime[lkh_hour()];
    int want = LKH_NEED_ACT[need];
    if (want >= 0 && act != want) {
        int nh = lkh_next_act_hour(want);
        if (nh < 0) return str("the regime has no %s slot at all", LK_ACT[want].name);
        return str("regime is %s; %s at %02d:00", LK_ACT[act].name, LK_ACT[want].name, nh);
    }
    return "on their way";
}

// ═══ designation ═════════════════════════════════════════════════════════════
static void lkh_set_zone(int c, int zn) {     // note A1
    if (c < 0 || c >= LK_N || zn < 0 || zn >= ZN_COUNT) return;
    lk_t[c].zone = (unsigned char)zn;
}

// would lk_queue take this?  Used for the ghost colour AND to explain a refusal.
static const char *lkh_why_refused(int c, int job, int arg) {
    if (c < 0 || c >= LK_N) return "off the map";
    const Tile *t = &lk_t[c];
    if (t->job != JB_NONE && t->claimed) return "a workman is already on this tile";
    if (lk_job_count() >= LK_MAXJOB) return "the work queue is full - let them catch up";
    switch (job) {
    case JB_FLOOR:
        if (t->wall != WL_NONE) return "there is a wall in the way";
        if (t->floor == arg) return "already paved with that";
        return "cannot pave there";
    case JB_WALL:
        if (t->wall == arg) return "already that wall";
        if (t->door != DR_NONE) return "that is a doorway";
        return "cannot build a wall there";
    case JB_DOOR:
        if (t->wall == WL_NONE && t->door == DR_NONE) return "a door needs a wall to sit in";
        if (t->door == arg) return "already that door";
        return "cannot fit a door there";
    case JB_OBJECT: {
        int w = LK_OBJ[arg].w, h = LK_OBJ[arg].h, x = lk_tx(c), y = lk_ty(c);
        if (!lk_in(x + w - 1, y + h - 1)) return "it would hang off the map";
        for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) {
            const Tile *q = &lk_t[lk_idx(x + dx, y + dy)];
            if (q->wall != WL_NONE) return "a wall is in the way";
            if (q->door != DR_NONE) return "that is a doorway";
            if (q->obj != OB_NONE || q->obj_ref) return "something is already there";
        }
        if (LK_OBJ[arg].w > 1 || LK_OBJ[arg].h > 1)
            return str("no room for a %dx%d %s here", LK_OBJ[arg].w, LK_OBJ[arg].h,
                       LK_OBJ[arg].name);
        return str("cannot place a %s there", LK_OBJ[arg].name);
    }
    case JB_DEMOLISH:
        return "nothing built there to tear down";
    default:
        return "cannot do that here";
    }
}
static int lkh_ok(int c, int job, int arg) {
    if (c < 0 || c >= LK_N) return 0;
    const Tile *t = &lk_t[c];
    if (t->job != JB_NONE && t->claimed) return 0;
    switch (job) {
    case JB_FLOOR:  return t->wall == WL_NONE && t->floor != arg;
    case JB_WALL:   return t->wall != arg && t->door == DR_NONE;
    case JB_DOOR:   return (t->wall != WL_NONE || t->door != DR_NONE) && t->door != arg;
    case JB_OBJECT: {
        int w = LK_OBJ[arg].w, h = LK_OBJ[arg].h, x = lk_tx(c), y = lk_ty(c);
        if (!lk_in(x + w - 1, y + h - 1)) return 0;
        for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) {
            const Tile *q = &lk_t[lk_idx(x + dx, y + dy)];
            if (q->wall != WL_NONE || q->door != DR_NONE) return 0;
            if (q->obj != OB_NONE || q->obj_ref) return 0;
        }
        return 1;
    }
    case JB_DEMOLISH:
        return t->wall != WL_NONE || t->door != DR_NONE || t->obj != OB_NONE || t->obj_ref;
    default: return 0;
    }
}

// Queue it and then LOOK: the grid is the authority on what it accepted (alg 4).
static int lkh_designate(int c, int job, int arg, int loud) {
    if (c < 0 || c >= LK_N) return 0;
    if (job == JB_DOOR && lk_t[c].wall == WL_NONE && lk_t[c].door == DR_NONE) {
        if (loud) { lk_hud_toast("a door needs a wall to sit in"); lk_sfx(SFX_DENY); }
        return 0;                                   // house rule, kinder than the grid's
    }
    int cost = lkh_job_cost(job, arg);
    if (!lkh_afford(cost)) {
        if (loud) {
            lk_hud_toast(str("cannot afford %s: %s, and %s is already queued",
                             lkh_arg_name(lk_tool, arg), lkh_money(cost),
                             lkh_money(lkh_committed)));
            lk_sfx(SFX_DENY);
        }
        return 0;
    }
    if (job == JB_OBJECT) lk_t[c].obj_dir = (unsigned char)(lkh_obj_dir & 3);   // note A2
    lk_queue(c, job, arg);
    if (lk_t[c].job != (unsigned char)job) {
        if (loud) { lk_hud_toast(lkh_why_refused(c, job, arg)); lk_sfx(SFX_DENY); }
        return 0;
    }
    lkh_committed += cost;
    if (loud) lk_sfx(SFX_PLACE);
    return 1;
}

static void lkh_erase_tile(int c) {
    if (c < 0 || c >= LK_N) return;
    if (lk_t[c].job != JB_NONE) { lk_unqueue(c); return; }      // cancel first, always
    if (lk_tool == TL_ROOM && lk_t[c].paint != RM_NONE) { lk_paint_room(c, RM_NONE); return; }
    if (lk_tool == TL_ZONE && lk_t[c].zone != ZN_OPEN) { lkh_set_zone(c, ZN_OPEN); return; }
    lk_queue(c, JB_DEMOLISH, 0);
}

// clamp a band to LKH_BAND_MAX per side, keeping the anchor corner fixed
static void lkh_band(int ca, int cb, int *x0, int *y0, int *x1, int *y1) {
    int ax = lk_tx(ca), ay = lk_ty(ca), bx = lk_tx(cb), by = lk_ty(cb);
    if (bx - ax >  LKH_BAND_MAX - 1) bx = ax + LKH_BAND_MAX - 1;
    if (ax - bx >  LKH_BAND_MAX - 1) bx = ax - LKH_BAND_MAX + 1;
    if (by - ay >  LKH_BAND_MAX - 1) by = ay + LKH_BAND_MAX - 1;
    if (ay - by >  LKH_BAND_MAX - 1) by = ay - LKH_BAND_MAX + 1;
    *x0 = ax < bx ? ax : bx; *x1 = ax < bx ? bx : ax;
    *y0 = ay < by ? ay : by; *y1 = ay < by ? by : ay;
}
static int lkh_on_band_edge(int x, int y, int x0, int y0, int x1, int y1) {
    return x == x0 || x == x1 || y == y0 || y == y1;
}

static void lkh_apply_band(int ca, int cb, int erase) {
    int x0, y0, x1, y1;
    lkh_band(ca, cb, &x0, &y0, &x1, &y1);
    int done = 0, tried = 0, cost = 0, broke = 0;
    const char *fail = NULL;
    int job = LKH_TOOL_JOB[lk_tool], arg = lk_tool_arg;
    int unit = lkh_job_cost(job, arg);

    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        int c = lk_idx(x, y);
        if (erase) { lkh_erase_tile(c); done++; continue; }
        if (lk_tool == TL_WALL && !lkh_on_band_edge(x, y, x0, y0, x1, y1)) continue;
        tried++;
        if (lk_tool == TL_ROOM) {
            if (lk_t[c].wall != WL_NONE || lk_t[c].door != DR_NONE) continue;
            lk_paint_room(c, arg); done++;
        } else if (lk_tool == TL_ZONE) {
            lkh_set_zone(c, arg); done++;
        } else if (job != JB_NONE) {
            if (!lkh_afford(unit)) { broke = 1; continue; }   // ran out mid-band
            if (lkh_designate(c, job, arg, 0)) { done++; cost += unit; }
            else if (!fail) fail = lkh_keep(lkh_why_refused(c, job, arg));
        }
    }
    if (broke && !done) {
        lk_hud_toast(str("out of money: %s each, %s left after the %s already queued",
                         lkh_money(unit), lkh_money(lkh_budget()), lkh_money(lkh_committed)));
        lk_sfx(SFX_DENY);
        return;
    }
    if (erase) {
        if (done) { lk_hud_toast(str("cleared %d tiles", done)); lk_sfx(SFX_CLICK); }
        return;
    }
    if (done) {
        lk_sfx(SFX_PLACE);
        if (lk_tool == TL_ROOM)
            lk_hud_toast(str("%d tiles marked as %s - the fill decides if it counts",
                             done, LK_ROOM[arg].name));
        else if (lk_tool == TL_ZONE)
            lk_hud_toast(str("%d tiles set to %s", done, LKH_ZONE_NAME[arg]));
        else if (broke)
            lk_hud_toast(str("%d queued for %s - the money ran out there", done, lkh_money(cost)));
        else
            lk_hud_toast(str("%d queued, %s of material", done, lkh_money(cost)));
    } else if (tried) {
        lk_hud_toast(fail ? fail : "nothing to do there");
        lk_sfx(SFX_DENY);
    }
}

// ═══ picking ═════════════════════════════════════════════════════════════════
static void lkh_pick_at(int cx, int cy) {
    int c = lkh_pt_tile(cx, cy);
    if (c < 0) return;
    int ai = lk_actor_at(lkh_pt_wx(cx), lkh_pt_wy(cy), 11);
    if (ai >= 0) {
        lk_sel_actor = ai; lk_sel_room = 0; lk_sfx(SFX_CLICK);
        lk_hud_toast(str("%s - %s", lkh_name(ai), lk_state_name(lk_a[ai].state)));
        return;
    }
    int rid = lk_room_of(c);
    lk_sel_actor = -1; lkh_follow = 0;
    if (rid > 0) {
        lk_sel_room = rid; lk_sfx(SFX_CLICK);
    } else {
        lk_sel_room = 0;
        if (lk_t[c].paint != RM_NONE)
            lk_hud_toast(str("painted %s, but no room here yet - is it walled in?",
                             LK_ROOM[lk_t[c].paint].name));
    }
}

// ═══ input ═══════════════════════════════════════════════════════════════════
static int lkh_seen_has(int id) {
    for (int i = 0; i < lkh_nseen; i++) if (lkh_seen[i] == id) return 1;
    return 0;
}
static void lkh_seen_drop(int id) {
    for (int i = 0; i < lkh_nseen; i++)
        if (lkh_seen[i] == id) { lkh_seen[i] = lkh_seen[--lkh_nseen]; return; }
}

// is this tool's gesture a rubber-band rectangle (applied on release)?
static int lkh_is_band_tool(int erase) {
    return erase || lk_tool == TL_FLOOR || lk_tool == TL_WALL ||
           lk_tool == TL_ROOM || lk_tool == TL_ZONE || lk_tool == TL_DEMOLISH;
}

static void lkh_drag_end(void) {
    if (lkh_dmode == LKH_D_BUILD || lkh_dmode == LKH_D_ERASE) {
        int cb = lkh_pt_tile(lkh_bx, lkh_by);
        int ca = lkh_ac >= 0 ? lkh_ac : cb;
        if (cb < 0) cb = ca;
        int erase = lkh_dmode == LKH_D_ERASE;
        if (ca >= 0 && lkh_is_band_tool(erase)) lkh_apply_band(ca, cb, erase);
    } else if (lkh_dmode == LKH_D_PAN && lk_tool == TL_SELECT && !lkh_moved) {
        lkh_pick_at(lkh_bx, lkh_by);
    }
    lkh_dmode = LKH_D_NONE; lkh_did = LKH_NOID; lkh_last_place = -1;
    lkh_moved = 0; lkh_ac = -1;
}

static void lkh_drag_start(int mode, int src, int id, int x, int y) {
    lkh_dmode = mode; lkh_dsrc = src; lkh_did = id;
    lkh_ax = lkh_bx = x; lkh_ay = lkh_by = y;
    lkh_ac = lkh_pt_tile(x, y);
    lkh_pan0x = lk_cam_x; lkh_pan0y = lk_cam_y;
    lkh_moved = 0; lkh_last_place = -1;
    if (mode == LKH_D_PAN) { lkh_follow = 0; return; }
    // point tools fire immediately, and again on every new tile
    if (mode == LKH_D_BUILD && (lk_tool == TL_DOOR || lk_tool == TL_OBJECT)) {
        if (lkh_ac >= 0 && lkh_designate(lkh_ac, LKH_TOOL_JOB[lk_tool], lk_tool_arg, 1))
            lkh_last_place = lkh_ac;
    }
}

static void lkh_pointer(void) {
    // ── follow the contact driving the map (mouse LMB arrives here too) ──
    if (lkh_dmode != LKH_D_NONE && lkh_dsrc == LKH_S_TOUCH) {
        for (int i = 0; i < touch_count(); i++)
            if (touch_id(i) == lkh_did) { lkh_bx = touch_x(i); lkh_by = touch_y(i); }
        for (int i = 0; i < touch_ended_count(); i++)
            if (touch_ended_id(i) == lkh_did) {
                lkh_bx = touch_ended_x(i); lkh_by = touch_ended_y(i);   // the release point
            }
    }
    // ── the contact painting the regime grid ──
    if (lkh_rgid != LKH_NOID) {
        int live = 0, gx = 0, gy = 0;
        for (int i = 0; i < touch_count(); i++)
            if (touch_id(i) == lkh_rgid) { gx = touch_x(i); gy = touch_y(i); live = 1; }
        if (live && lkh_panel == LKH_P_REGIME && lkh_rg_grid.w > 0 &&
            gy >= lkh_rg_grid.y - 4 && gy < lkh_rg_grid.y + lkh_rg_grid.h + 4) {
            int h = (int)((gx - lkh_rg_grid.x) / (lkh_rg_grid.w / 24.0f));
            if (h >= 0 && h < 24 && lk_regime[h] != (unsigned char)lkh_brush) {
                lk_regime[h] = (unsigned char)lkh_brush;
                lk_sfx(SFX_CLICK);
            }
        }
        if (!live) lkh_rgid = LKH_NOID;
    }

    // ── new contacts ──
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (lkh_seen_has(id)) continue;
        if (lkh_nseen < 16) lkh_seen[lkh_nseen++] = id;
        int tx = touch_x(i), ty = touch_y(i);
        // the regime grid claims its own contacts (ui.h can't drag-paint 24 cells)
        if (lkh_panel == LKH_P_REGIME && lkh_rg_grid.w > 0 && lkh_rgid == LKH_NOID &&
            binside(lkh_rg_grid, tx, ty)) {
            lkh_rgid = id;
            int h = (int)((tx - lkh_rg_grid.x) / (lkh_rg_grid.w / 24.0f));
            if (h >= 0 && h < 24) { lk_regime[h] = (unsigned char)lkh_brush; lk_sfx(SFX_CLICK); }
            continue;
        }
        if (lkh_dmode != LKH_D_NONE) continue;
        if (touch_count() >= 2) continue;                  // a pinch/pan, not a paint
        if (lkh_blocked(tx, ty)) continue;                 // chrome — ui.h will handle it
        lkh_drag_start(lk_tool == TL_SELECT ? LKH_D_PAN : LKH_D_BUILD, LKH_S_TOUCH, id, tx, ty);
    }
    for (int i = 0; i < touch_ended_count(); i++) lkh_seen_drop(touch_ended_id(i));

    // ── mouse right / middle: erase and pan ──
    int mx = mouse_x(), my = mouse_y();
    if (lkh_dmode == LKH_D_NONE) {
        if (mouse_pressed(MOUSE_MIDDLE) && !lkh_blocked(mx, my))
            lkh_drag_start(LKH_D_PAN, LKH_S_MBTN, LKH_NOID, mx, my);
        else if (mouse_pressed(MOUSE_RIGHT) && !lkh_blocked(mx, my)) {
            if (lk_tool == TL_SELECT) lkh_drag_start(LKH_D_PAN, LKH_S_RBTN, LKH_NOID, mx, my);
            else                      lkh_drag_start(LKH_D_ERASE, LKH_S_RBTN, LKH_NOID, mx, my);
        } else if (mouse_pressed(MOUSE_RIGHT) && lkh_panel == LKH_P_REGIME &&
                   lkh_rg_grid.w > 0 && binside(lkh_rg_grid, mx, my)) {
            int h = (int)((mx - lkh_rg_grid.x) / (lkh_rg_grid.w / 24.0f));   // eyedropper
            if (h >= 0 && h < 24) { lkh_brush = lk_regime[h]; lk_sfx(SFX_CLICK); }
        }
    } else if (lkh_dsrc != LKH_S_TOUCH) {
        lkh_bx = mx; lkh_by = my;
    }

    if (lkh_dmode == LKH_D_NONE) return;

    // is the gesture still down?  Recomputed HERE, after a drag may have just
    // STARTED this frame — computing it earlier ended every drag on frame one.
    int alive = 0;
    if (lkh_dsrc == LKH_S_TOUCH) {
        for (int i = 0; i < touch_count(); i++) if (touch_id(i) == lkh_did) alive = 1;
    } else {
        alive = mouse_down(lkh_dsrc == LKH_S_RBTN ? MOUSE_RIGHT : MOUSE_MIDDLE);
    }

    int dx = lkh_bx - lkh_ax, dy = lkh_by - lkh_ay;
    if (dx * dx + dy * dy > LKH_CLICK_SLOP * LKH_CLICK_SLOP) lkh_moved = 1;

    if (lkh_dmode == LKH_D_PAN) {
        lk_cam_x = lkh_pan0x - dx;
        lk_cam_y = lkh_pan0y - dy;
        lkh_cam_clamp();
    } else if (lkh_dmode == LKH_D_BUILD && (lk_tool == TL_DOOR || lk_tool == TL_OBJECT)) {
        int c = lkh_pt_tile(lkh_bx, lkh_by);
        if (c >= 0 && c != lkh_last_place) {
            if (lkh_designate(c, LKH_TOOL_JOB[lk_tool], lk_tool_arg, 0)) lkh_last_place = c;
        }
    }
    if (!alive) lkh_drag_end();
}

// two fingers = pan the map (and never paint), the phone gesture
static void lkh_multitouch(void) {
    int n = touch_count();
    if (n >= 2) {
        int sx = 0, sy = 0;
        for (int i = 0; i < n; i++) { sx += touch_x(i); sy += touch_y(i); }
        sx /= n; sy /= n;
        if (lkh_multi_prev) {
            lk_cam_x -= sx - lkh_multi_x;
            lk_cam_y -= sy - lkh_multi_y;
            lkh_cam_clamp();
        } else if (lkh_dmode != LKH_D_NONE) {
            // the second finger takes over: abandon the one-finger gesture WITHOUT
            // applying it, so a pan never paints and never double-moves the camera
            lkh_dmode = LKH_D_NONE; lkh_did = LKH_NOID;
            lkh_ac = -1; lkh_last_place = -1; lkh_moved = 0;
        }
        lkh_multi_x = sx; lkh_multi_y = sy; lkh_multi_prev = 1;
    } else {
        lkh_multi_prev = 0;
    }
}

static void lkh_camera_keys(float d) {
    float sp = LKH_PAN_KEY * d * (key(340) || key(344) ? LKH_PAN_FAST : 1.0f);  // raylib shifts
    float mx = 0, my = 0;
    if (key(KEY_LEFT)  || key('A')) mx -= 1;
    if (key(KEY_RIGHT) || key('D')) mx += 1;
    if (key(KEY_UP)    || key('W')) my -= 1;
    if (key(KEY_DOWN)  || key('S')) my += 1;
    if (mx || my) { lkh_follow = 0; lk_cam_x += (int)(mx * sp); lk_cam_y += (int)(my * sp); }

    // wheel scrolls the map (there is no zoom — see key algorithm 1)
    int pmx = mouse_x(), pmy = mouse_y();
    if (binside(lkh_v, pmx, pmy) && !lkh_blocked(pmx, pmy)) {
        float w = mouse_wheel(), wx = mouse_wheel_x();
        if (w != 0)  { lkh_follow = 0; lk_cam_y -= (int)(w * 44.0f); }
        if (wx != 0) { lkh_follow = 0; lk_cam_x += (int)(wx * 44.0f); }
    }
    // edge scroll — only once a real mouse has moved, so touch never drifts
    if (lkh_mouse_seen && lkh_dmode == LKH_D_NONE && binside(lkh_v, pmx, pmy) &&
        !lkh_blocked(pmx, pmy)) {
        float e = 0;
        if ((e = (float)(lkh_v.x + LKH_EDGE - pmx)) > 0)
            lk_cam_x -= (int)(LKH_EDGE_SPD * d * (e / LKH_EDGE));
        if ((e = (float)(pmx - (lkh_v.x + lkh_v.w - 1 - LKH_EDGE))) > 0)
            lk_cam_x += (int)(LKH_EDGE_SPD * d * (e / LKH_EDGE));
        if ((e = (float)(lkh_v.y + LKH_EDGE - pmy)) > 0)
            lk_cam_y -= (int)(LKH_EDGE_SPD * d * (e / LKH_EDGE));
        if ((e = (float)(pmy - (lkh_v.y + lkh_v.h - 1 - LKH_EDGE))) > 0)
            lk_cam_y += (int)(LKH_EDGE_SPD * d * (e / LKH_EDGE));
    }
    lkh_cam_clamp();
}

static void lkh_set_speed(int s) {
    if (s > 0) lkh_last_speed = s;
    lk.speed = s;
    lk_hud_toast(s == 0 ? "paused" : str("speed %dx", s));
}

static void lkh_keys(void) {
    for (int i = 0; i < TL_COUNT; i++) if (keyp('1' + i)) lkh_set_tool(i);
    if (keyp(KEY_SPACE)) lkh_set_speed(lk.speed == 0 ? lkh_last_speed : 0);
    if (keyp(KEY_TAB) || keyp('O')) {
        lk_overlay = (lk_overlay + 1) % OV_COUNT;
        lk_hud_toast(str("overlay: %s - %s", LKH_OVL_NAME[lk_overlay], LKH_OVL_TIP[lk_overlay]));
    }
    if (keyp('R') && lk_tool == TL_OBJECT) {
        lkh_obj_dir = (lkh_obj_dir + 1) & 3;
        lk_hud_toast(str("facing %s", lkh_obj_dir == 0 ? "south" : lkh_obj_dir == 1 ? "north"
                                    : lkh_obj_dir == 2 ? "east" : "west"));
    }
    if (keyp('[')) lkh_cycle_arg(-1);
    if (keyp(']')) lkh_cycle_arg(+1);
    if (keyp('T')) lkh_panel = lkh_panel == LKH_P_REGIME  ? LKH_P_NONE : LKH_P_REGIME;
    if (keyp('F')) lkh_panel = lkh_panel == LKH_P_FINANCE ? LKH_P_NONE : LKH_P_FINANCE;
    if (keyp('H') || keyp('/')) lkh_panel = lkh_panel == LKH_P_HELP ? LKH_P_NONE : LKH_P_HELP;
    if (keyp('L')) {
        int on = lk.alarm == AL_LOCKDOWN;
        lk_set_alarm(on ? AL_CALM : AL_LOCKDOWN);
        lk_hud_toast(on ? "lockdown lifted - the jail doors open"
                        : "LOCKDOWN - every jail door shut, and every need stops being met");
    }
    if (keyp('X')) { lk_shakedown(); lk_hud_toast("shakedown ordered - guards will search the cells"); }
    if (keyp('C') && lk_sel_actor >= 0) {
        lkh_follow = !lkh_follow;
        lk_hud_toast(lkh_follow ? "camera following" : "camera released");
    }
    if (keyp(KEY_ESCAPE)) {
        if (lkh_panel != LKH_P_NONE) lkh_panel = LKH_P_NONE;
        else if (lk_sel_actor >= 0 || lk_sel_room > 0) { lk_sel_actor = -1; lk_sel_room = 0; }
        else if (lk_tool != TL_SELECT) lkh_set_tool(TL_SELECT);
    }
}

// ═══ UPDATE ══════════════════════════════════════════════════════════════════
void lk_hud_init(void) {
    lkh_nobjord = 0;
    for (unsigned int i = 0; i < sizeof(LKH_OBJ_ORDER); i++) {
        int ob = LKH_OBJ_ORDER[i];
        if (ob > OB_NONE && ob < OB_COUNT) lkh_objord[lkh_nobjord++] = (unsigned char)ob;
    }
    for (int ob = OB_NONE + 1; ob < OB_COUNT; ob++) {          // nothing left unbuildable
        int found = 0;
        for (int i = 0; i < lkh_nobjord; i++) if (lkh_objord[i] == ob) { found = 1; break; }
        if (!found && lkh_nobjord < OB_COUNT) lkh_objord[lkh_nobjord++] = (unsigned char)ob;
    }
    lkh_nroomord = 0;
    for (unsigned int i = 0; i < sizeof(LKH_ROOM_ORDER); i++) {
        int rm = LKH_ROOM_ORDER[i];
        if (rm > RM_NONE && rm < RM_COUNT) lkh_roomord[lkh_nroomord++] = (unsigned char)rm;
    }
    for (int rm = RM_NONE + 1; rm < RM_COUNT; rm++) {
        int found = 0;
        for (int i = 0; i < lkh_nroomord; i++) if (lkh_roomord[i] == rm) { found = 1; break; }
        if (!found && lkh_nroomord < RM_COUNT) lkh_roomord[lkh_nroomord++] = (unsigned char)rm;
    }

    lkh_arg[TL_SELECT]   = 0;
    lkh_arg[TL_FLOOR]    = FL_CONCRETE;
    lkh_arg[TL_WALL]     = WL_BRICK;
    lkh_arg[TL_DOOR]     = DR_JAIL;
    lkh_arg[TL_OBJECT]   = OB_BED;
    lkh_arg[TL_ROOM]     = RM_CELL;
    lkh_arg[TL_ZONE]     = ZN_STAFF;
    lkh_arg[TL_DEMOLISH] = 0;
    lk_tool = TL_SELECT; lk_tool_arg = lkh_arg[TL_SELECT];
    lk_overlay = OV_NONE;
    lk_sel_actor = -1; lk_sel_room = 0;
    lkh_panel = LKH_P_NONE; lkh_obj_dir = 0; lkh_brush = AC_FREE;
    lkh_ntoast = 0; lkh_nseen = 0; lkh_rgid = LKH_NOID; lkh_did = LKH_NOID;
    lkh_dmode = LKH_D_NONE; lkh_follow = 0; lkh_fin_scroll = 0;
    lkh_day_open = lk.money; lkh_day_seen = lk.day;
    lkh_last_speed = lk.speed > 0 ? lk.speed : 1;

    lkh_relayout();
    lkh_cam_center(45.0f * LK_TS, 29.0f * LK_TS);      // the starting prison
    lkh_recount_committed();
    lk_hud_toast("press H for how a prison works, T for the regime");
}

void lk_hud_update(float d) {
    if (!(d > 0.0f)) d = 1.0f / 60.0f;                 // note E
    if (d > 1.0f / 15.0f) d = 1.0f / 15.0f;
    lkh_pulse += d;

    int mx = mouse_x(), my = mouse_y();
    if (mx != lkh_mx_prev || my != lkh_my_prev) {
        if (lkh_mx_prev != -9999) lkh_mouse_seen = 1;
        lkh_mx_prev = mx; lkh_my_prev = my;
    }

    lkh_relayout();
    lkh_keys();
    lkh_multitouch();
    lkh_pointer();
    lkh_camera_keys(d);
    lkh_toasts_step(d);
    lkh_recount_committed();

    if (lkh_day_seen != lk.day) { lkh_day_seen = lk.day; lkh_day_open = lk.money; }

    // the selection can die or be demolished under us
    if (lk_sel_actor >= 0 && (lk_sel_actor >= LK_MAXACT || !lk_a[lk_sel_actor].alive)) {
        lk_sel_actor = -1; lkh_follow = 0;
    }
    if (lk_sel_room > 0 && (lk_sel_room >= LK_MAXROOM || lk_room[lk_sel_room].type == RM_NONE))
        lk_sel_room = 0;
    if (lkh_follow && lk_sel_actor >= 0)
        lkh_cam_center(lk_a[lk_sel_actor].x, lk_a[lk_sel_actor].y);

    lkh_hover_c = lkh_blocked(mx, my) ? -1 : lkh_pt_tile(mx, my);

    // a pointer shape that says what the tool will do
    if (lkh_mouse_seen) {
        if (lkh_dmode == LKH_D_PAN) mouse_cursor(CURSOR_MOVE);
        else if (lkh_hover_c < 0) mouse_cursor(CURSOR_DEFAULT);
        else if (lk_tool == TL_SELECT) mouse_cursor(CURSOR_HAND);
        else if (LKH_TOOL_JOB[lk_tool] != JB_NONE &&
                 !lkh_ok(lkh_hover_c, LKH_TOOL_JOB[lk_tool], lk_tool_arg))
            mouse_cursor(CURSOR_NO);
        else mouse_cursor(CURSOR_CROSSHAIR);
    }
}

// ═══ DRAW: the world-space layer (ghosts, band, selection) ═══════════════════
static void lkh_draw_world_layer(void) {
    clip((int)lkh_v.x, (int)lkh_v.y, (int)lkh_v.w, (int)lkh_v.h);
    lkh_world_cam();                                    // note C

    int job = LKH_TOOL_JOB[lk_tool];
    int dragging = lkh_dmode == LKH_D_BUILD || lkh_dmode == LKH_D_ERASE;
    int band = dragging && lkh_is_band_tool(lkh_dmode == LKH_D_ERASE);

    // ── the rubber band ──
    if (band) {
        int cb = lkh_pt_tile(lkh_bx, lkh_by);
        int ca = lkh_ac >= 0 ? lkh_ac : cb;
        if (cb < 0) cb = ca;
        if (cb >= 0 && ca >= 0) {
            int x0, y0, x1, y1;
            lkh_band(ca, cb, &x0, &y0, &x1, &y1);
            int tiles = (x1 - x0 + 1) * (y1 - y0 + 1);
            int erase = lkh_dmode == LKH_D_ERASE;
            int col = erase ? LKH_C_BAD
                    : lk_tool == TL_ROOM ? LK_ROOM[lk_tool_arg].colour
                    : lk_tool == TL_ZONE ? LKH_ZONE_COL[lk_tool_arg]
                    : lk_tool == TL_DEMOLISH ? LKH_C_BAD : LKH_C_ACC;
            if (tiles <= LKH_GHOST_MAX && !erase) {
                for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
                    if (lk_tool == TL_WALL && !lkh_on_band_edge(x, y, x0, y0, x1, y1)) continue;
                    int c = lk_idx(x, y);
                    if (job != JB_NONE) lk_art_ghost(c, job, lk_tool_arg, lkh_ok(c, job, lk_tool_arg) != 0);
                    else {
                        fillp(FILL_CHECKER, -1);
                        rectfill(x * LK_TS, y * LK_TS, LK_TS, LK_TS, col);
                        fillp_reset();
                    }
                }
            }
            rect(x0 * LK_TS, y0 * LK_TS, (x1 - x0 + 1) * LK_TS, (y1 - y0 + 1) * LK_TS, col);
            rect(x0 * LK_TS + 1, y0 * LK_TS + 1, (x1 - x0 + 1) * LK_TS - 2,
                 (y1 - y0 + 1) * LK_TS - 2, blink(8) ? LKH_C_HI : col);
        }
    } else if (lkh_hover_c >= 0) {
        // ── the hover ghost ──
        int c = lkh_hover_c, x = lk_tx(c), y = lk_ty(c);
        if (job != JB_NONE) {
            int ok = lkh_ok(c, job, lk_tool_arg) && lkh_afford(lkh_job_cost(job, lk_tool_arg));
            lk_art_ghost(c, job, lk_tool_arg, ok != 0);
            int w = job == JB_OBJECT ? LK_OBJ[lk_tool_arg].w : 1;
            int h = job == JB_OBJECT ? LK_OBJ[lk_tool_arg].h : 1;
            rect(x * LK_TS, y * LK_TS, w * LK_TS, h * LK_TS, ok ? LKH_C_ACC : LKH_C_BAD);
        } else {
            int col = lk_tool == TL_ROOM ? LK_ROOM[lk_tool_arg].colour
                    : lk_tool == TL_ZONE ? LKH_ZONE_COL[lk_tool_arg] : LKH_C_HI;
            rect(x * LK_TS, y * LK_TS, LK_TS, LK_TS, col);
        }
    }

    // ── the selected room, outlined along its own edges ──
    if (lk_sel_room > 0 && lk_sel_room < LK_MAXROOM) {
        int t0x = lk_cam_x / LK_TS, t0y = lk_cam_y / LK_TS;
        int t1x = (lk_cam_x + (int)lkh_v.w) / LK_TS + 1, t1y = (lk_cam_y + (int)lkh_v.h) / LK_TS + 1;
        if (t1x > LK_MW) t1x = LK_MW;
        if (t1y > LK_MH) t1y = LK_MH;
        int col = blink(10) ? LKH_C_HI : LKH_C_ACC;
        for (int y = t0y; y < t1y; y++) for (int x = t0x; x < t1x; x++) {
            if (lk_t[lk_idx(x, y)].room != lk_sel_room) continue;
            int px = x * LK_TS, py = y * LK_TS;
            if (y == 0        || lk_t[lk_idx(x, y - 1)].room != lk_sel_room) line(px, py, px + LK_TS - 1, py, col);
            if (y == LK_MH - 1|| lk_t[lk_idx(x, y + 1)].room != lk_sel_room) line(px, py + LK_TS - 1, px + LK_TS - 1, py + LK_TS - 1, col);
            if (x == 0        || lk_t[lk_idx(x - 1, y)].room != lk_sel_room) line(px, py, px, py + LK_TS - 1, col);
            if (x == LK_MW - 1|| lk_t[lk_idx(x + 1, y)].room != lk_sel_room) line(px + LK_TS - 1, py, px + LK_TS - 1, py + LK_TS - 1, col);
        }
    }

    // ── the selected prisoner, ringed ──
    if (lk_sel_actor >= 0 && lk_a[lk_sel_actor].alive) {
        const Actor *a = &lk_a[lk_sel_actor];
        int r = 9 + (int)(sin_deg(lkh_pulse * 240.0f) * 1.5f);
        circ((int)a->x, (int)a->y, r, LKH_C_HI);
        circ((int)a->x, (int)a->y, r + 1, CLR_BROWNISH_BLACK);
        if (a->target >= 0 && a->target < LK_N) {          // where they are headed
            line((int)a->x, (int)a->y, lk_cx(a->target), lk_cy(a->target), CLR_DARK_GREY);
            rect(lk_tx(a->target) * LK_TS, lk_ty(a->target) * LK_TS, LK_TS, LK_TS, LKH_C_ACC);
        }
    }

    camera(0, 0);
    clip(0, 0, 0, 0);
}

// ═══ DRAW: the top bar ═══════════════════════════════════════════════════════
static void lkh_draw_top(void) {
    Box b = lkh_top;
    rectfill(0, 0, (int)lkh_scr.w, (int)(b.y + b.h), LKH_C_PANEL);   // bleed past the safe area
    line(0, (int)(b.y + b.h) - 1, (int)lkh_scr.w, (int)(b.y + b.h) - 1, LKH_C_FRAME);

    Box row = lay_pad(b, 2, 4, 3, 4), rest = row;

    // ── right: speed + alarm ──
    float spw = lkh_compact ? 66.0f : 76.0f;
    Box sp = lay_split(rest, EDGE_RIGHT, spw, &rest);
    static const char *SPN[4] = { "II", "1x", "2x", "4x" };
    static const int   SPV[4] = { 0, 1, 2, 4 };
    for (int i = 0; i < 4; i++) {
        Box cb = lay_cell(sp, 0, 4, i, 1);
        if (lkh_btn(cb, SPN[i], lk.speed == SPV[i], 1,
                    i == 0 ? "pause the prison (space)" : "run this many times faster"))
            lkh_set_speed(SPV[i]);
    }
    Box al = lay_split(rest, EDGE_RIGHT, lkh_compact ? 52.0f : 62.0f, &rest);
    {
        static const char *AN[AL_COUNT] = { "CALM", "INCIDENT", "RIOT", "LOCKDOWN" };
        static const int   AC[AL_COUNT] = { CLR_DARK_GREEN, CLR_ORANGE, CLR_RED, CLR_DARK_RED };
        int on = lk.alarm != AL_CALM;
        int col = AC[lk.alarm < AL_COUNT ? lk.alarm : 0];
        if (on && blink(9)) col = CLR_WHITE;
        rectfill((int)al.x, (int)al.y, (int)al.w, (int)al.h, col);
        rect((int)al.x, (int)al.y, (int)al.w, (int)al.h, LKH_C_FRAME);
        lkh_fnt(FONT_SMALL);
        const char *t = lkh_fit(lk.over == 1 ? "BANKRUPT" : lk.over == 2 ? "LOST" :
                                lk.over == 3 ? "ENDURED" : AN[lk.alarm < AL_COUNT ? lk.alarm : 0],
                                (int)al.w - 2);
        print(t, (int)(al.x + (al.w - text_width(t)) / 2), (int)(al.y + (al.h - 6) / 2),
              lkh_ink(col));
        if (binside(al, mouse_x(), mouse_y()))
            lkh_tip("the prison's alarm state", "riot = simultaneous fights. L locks it down");
    }

    // ── left: money ──
    Box mb = lay_split(rest, EDGE_LEFT, lkh_compact ? 88.0f : 112.0f, &rest);
    {
        lkh_fnt(FONT_NORMAL);
        int col = lk.money < 0 ? LKH_C_BAD : LKH_C_HI;
        print(lkh_money(lk.money), (int)mb.x, (int)(mb.y + (mb.h - 8) / 2), col);
        int delta = lk.money - lkh_day_open;
        lkh_fnt(FONT_SMALL);
        const char *dt = str("%s%s today", delta < 0 ? "" : "+", lkh_money(delta));
        print_right(dt, (int)(mb.x + mb.w - 2), (int)(mb.y + mb.h - 7),
                    delta < 0 ? LKH_C_BAD : LKH_C_GOOD);
        if (binside(mb, mouse_x(), mouse_y()))
            lkh_tip(str("%s committed to the work queue", lkh_money(lkh_committed)),
                    "F opens the day's ledger");
    }

    // ── clock + day + the regime slot ──
    Box cb = lay_split(rest, EDGE_LEFT, lkh_compact ? 74.0f : 96.0f, &rest);
    {
        lkh_fnt(FONT_NORMAL);
        print(lkh_clock(), (int)cb.x, (int)(cb.y + (cb.h - 8) / 2), LKH_C_TEXT);
        lkh_fnt(FONT_SMALL);
        int rw = (int)cb.w - 44;
        print(lkh_fit(str("day %d", lk.day), rw), (int)cb.x + 42, (int)(cb.y + 1), LKH_C_DIM);
        int h = lkh_hour();
        int nh = -1;
        for (int k = 1; k <= 24; k++) if (lk_regime[(h + k) % 24] != lk_regime[h]) { nh = (h + k) % 24; break; }
        if (nh >= 0) print(lkh_fit(str("to %02d:00", nh), rw), (int)cb.x + 42,
                           (int)(cb.y + cb.h - 7), LKH_C_DIM);
    }
    if (rest.w > 60) {
        Box ab = lay_split(rest, EDGE_LEFT, lkh_compact ? 56.0f : 74.0f, &rest);
        int act = lk_regime[lkh_hour()];
        int col = LK_ACT[act].colour;
        rectfill((int)ab.x, (int)ab.y + 1, (int)ab.w, (int)ab.h - 2, col);
        rect((int)ab.x, (int)ab.y + 1, (int)ab.w, (int)ab.h - 2, LKH_C_FRAME);
        lkh_fnt(FONT_SMALL);
        const char *t = lkh_fit(LK_ACT[act].name, (int)ab.w - 3);
        print(t, (int)(ab.x + (ab.w - text_width(t)) / 2), (int)(ab.y + (ab.h - 6) / 2),
              lkh_ink(col));
        if (binside(ab, mouse_x(), mouse_y()))
            lkh_tip("what the regime permits right now", "T edits the timetable");
    }

    // ── the remainder: counts ──
    if (rest.w > 54) {
        lkh_fnt(FONT_SMALL);
        int x = (int)rest.x, y0 = (int)rest.y, y1 = (int)(rest.y + rest.h - 7);
        int over = lk.n_prisoners > lk.n_beds;
        int colw = rest.w > 120 ? 64 : (int)rest.w;
        print(lkh_fit(str("%d prisoners", lk.n_prisoners), colw), x, y0,
              over ? LKH_C_WARN : LKH_C_TEXT);
        print(lkh_fit(str("%d beds, %d cells", lk.n_beds, lk.n_cells), colw), x, y1,
              over ? LKH_C_WARN : LKH_C_DIM);
        if (rest.w > 120) {
            int w2 = (int)rest.w - 66;
            print(lkh_fit(str("%d staff", lk.n_staff), w2), x + 66, y0, LKH_C_TEXT);
            print(lkh_fit(str("%d jobs queued", lk_job_count()), w2), x + 66, y1, LKH_C_DIM);
        }
        if (binside(rest, mouse_x(), mouse_y()) && over)
            lkh_tip("more prisoners than beds", "the ones without a bed never sleep");
    }

    // ── tension: the prison's mood, along the bar's own edge ──
    {
        int w = (int)lkh_scr.w, y = (int)(b.y + b.h) - 3;
        rectfill(0, y, w, 2, CLR_DARKER_BLUE);
        int fw = (int)(clamp(lk.tension, 0, 1) * (float)w);
        rectfill(0, y, fw, 2, lkh_heat(lk.tension));
        if (!lkh_tip_on && mouse_y() >= y - 2 && mouse_y() <= y + 3)   // least specific: last
            lkh_tip(str("tension %d%%", (int)(lk.tension * 100.0f)),
                    "the integral of unmet need - it is what the music listens to");
    }
}

// ═══ DRAW: the toolbar ═══════════════════════════════════════════════════════
static void lkh_draw_toolbar(void) {
    Box b = lkh_bar;
    rectfill(0, (int)b.y, (int)lkh_scr.w, (int)(lkh_scr.h - b.y), LKH_C_PANEL);
    line(0, (int)b.y, (int)lkh_scr.w, (int)b.y, LKH_C_FRAME);

    Box rows = lay_pad(b, 2, 4, 2, 4), r1 = rows, r2 = rows;
    if (lkh_barrows == 2) r1 = lay_split(rows, EDGE_TOP, rows.h * 0.5f, &r2);

    // ── the eight tools ──
    Box tools = r1, side = r1;
    float need = (float)TL_COUNT * (lkh_compact ? 26.0f : 32.0f);
    if (lkh_barrows == 1) {
        if (need > r1.w * 0.62f) need = r1.w * 0.62f;
        tools = lay_split(r1, EDGE_LEFT, need, &side);
    }
    for (int i = 0; i < TL_COUNT; i++) {
        Box cb = lay_cell(tools, 0, TL_COUNT, i, 1);
        int sel = lk_tool == i;
        int hit = lkh_btn(cb, NULL, sel, 1, NULL);
        int gx = (int)(cb.x + (cb.w - 13) / 2), gy = (int)(cb.y + 2);
        int gc = sel ? CLR_WHITE : (binside(cb, mouse_x(), mouse_y()) ? LKH_C_HI : LKH_C_TEXT);
        if (cb.h >= 20) lkh_glyph(i, gx, gy, gc);
        lkh_fnt(FONT_TINY);
        if (cb.h >= 20) print(str("%d", i + 1), (int)cb.x + 2, (int)(cb.y + cb.h - 6), LKH_C_DIM);
        else {
            const char *t = lkh_fit(LKH_TOOL_SHORT[i], (int)cb.w - 2);
            print(t, (int)(cb.x + (cb.w - text_width(t)) / 2), (int)(cb.y + (cb.h - 5) / 2), gc);
        }
        if (binside(cb, mouse_x(), mouse_y()))
            lkh_tip(str("%d  %s", i + 1, LKH_TOOL_NAME[i]), LKH_TOOL_TIP[i]);
        if (hit) lkh_set_tool(i);
    }

    Box act = lkh_barrows == 2 ? r2 : side;

    // ── the current selection, spelled out ──
    if (lkh_arg_n(lk_tool) > 0 && act.w > 150) {
        Box cur = lay_split(act, EDGE_LEFT, lkh_compact ? 76.0f : 104.0f, &act);
        lkh_fnt(FONT_SMALL);
        int cost = lkh_arg_cost(lk_tool, lk_tool_arg);
        print(lkh_fit(lkh_arg_name(lk_tool, lk_tool_arg), (int)cur.w - 2),
              (int)cur.x + 2, (int)cur.y, LKH_C_HI);
        print(cost > 0 ? str("%s each", lkh_money(cost)) : "no material cost",
              (int)cur.x + 2, (int)(cur.y + cur.h - 7),
              cost > 0 && !lkh_afford(cost) ? LKH_C_BAD : LKH_C_DIM);
        if (binside(cur, mouse_x(), mouse_y()))
            lkh_tip("the current choice - [ and ] cycle it", NULL);
    }

    // ── overlay + panels + the two orders, right-aligned ──
    int nb = 6;
    float bw = act.w / (float)nb;
    if (bw > 60) bw = 60;
    Box strip = box(act.x + act.w - bw * nb, act.y, bw * nb, act.h);
    if (strip.x < act.x) strip.x = act.x;
    int i = 0;
    {
        Box cb = lay_cell(strip, 0, nb, i++, 1);
        if (lkh_btn(cb, lkh_compact ? LKH_OVL_NAME[lk_overlay] :
                    str("view: %s", LKH_OVL_NAME[lk_overlay]), lk_overlay != OV_NONE, 1,
                    LKH_OVL_TIP[lk_overlay])) {
            lk_overlay = (lk_overlay + 1) % OV_COUNT;
            lk_sfx(SFX_CLICK);
        }
    }
    {
        Box cb = lay_cell(strip, 0, nb, i++, 1);
        if (lkh_btn(cb, "REGIME", lkh_panel == LKH_P_REGIME, 1,
                    "the 24-hour timetable the whole prison obeys (T)"))
            lkh_panel = lkh_panel == LKH_P_REGIME ? LKH_P_NONE : LKH_P_REGIME;
    }
    {
        Box cb = lay_cell(strip, 0, nb, i++, 1);
        if (lkh_btn(cb, "MONEY", lkh_panel == LKH_P_FINANCE, 1,
                    "today's ledger, the five grades, what the place is worth (F)"))
            lkh_panel = lkh_panel == LKH_P_FINANCE ? LKH_P_NONE : LKH_P_FINANCE;
    }
    {
        Box cb = lay_cell(strip, 0, nb, i++, 1);
        int on = lk.alarm == AL_LOCKDOWN;
        if (lkh_btn(cb, on ? "RELEASE" : "LOCKDOWN", on, 1,
                    on ? "open the jail doors again (L)"
                       : "shut every jail door: calm, bought with unmet need (L)")) {
            lk_set_alarm(on ? AL_CALM : AL_LOCKDOWN);
            lk_hud_toast(on ? "lockdown lifted - the jail doors open"
                            : "LOCKDOWN - needs stop being met while it holds");
        }
    }
    {
        Box cb = lay_cell(strip, 0, nb, i++, 1);
        if (lkh_btn(cb, "SEARCH", 0, lk.n_staff > 0,
                    "send the guards through the cells for contraband (X)")) {
            if (lk.n_staff <= 0) lk_hud_toast("no guards to send");
            else { lk_shakedown(); lk_hud_toast("shakedown ordered - cells will be searched"); }
        }
    }
    {
        Box cb = lay_cell(strip, 0, nb, i++, 1);
        if (lkh_btn(cb, "?", lkh_panel == LKH_P_HELP, 1, "how a prison works, and every key (H)"))
            lkh_panel = lkh_panel == LKH_P_HELP ? LKH_P_NONE : LKH_P_HELP;
    }
}

// ═══ DRAW: the argument picker ═══════════════════════════════════════════════
static void lkh_draw_picker(void) {
    if (lkh_pick.h <= 0) return;
    Box b = lkh_pick;
    int n = lkh_arg_n(lk_tool);
    lkh_shadow(b);
    boxfill(b, LKH_C_PANEL);
    boxrect(b, LKH_C_FRAME);
    lkh_fnt(FONT_SMALL);
    print(lkh_fit(str("%s  -  %s", LKH_TOOL_NAME[lk_tool],
                      lk_tool == TL_OBJECT ? "click to place, R rotates" :
                      lk_tool == TL_ROOM   ? "paint an intent, the fill decides" :
                                             "drag to designate"),
                  (int)b.w - 8 - (lk_tool == TL_OBJECT ? 46 : 0)),
          (int)b.x + 4, (int)b.y + 3, LKH_C_DIM);

    // colorkey is set-and-hold: once per picker, not once per chip
    colorkey(lk_tool == TL_FLOOR ? -1 : CLR_BLACK);

    Box grid = lay_pad(b, 13, 3, 3, 3);
    float chipw = lkh_compact ? 58.0f : 78.0f;
    if (grid.w < chipw) chipw = grid.w;
    for (int i = 0; i < n; i++) {
        Box cb = lay_wrap(grid, n, i, chipw, 2);
        if (cb.h > 22) cb.h = 22;
        if (cb.y + cb.h > b.y + b.h - 2) continue;               // clipped by the panel
        int val = lkh_arg_val(lk_tool, i);
        int cost = lkh_arg_cost(lk_tool, val);
        int can = lkh_afford(cost);
        int sel = val == lk_tool_arg;
        int hit = lkh_btn(cb, NULL, sel, can, NULL);

        int x = (int)cb.x, y = (int)cb.y, w = (int)cb.w, h = (int)cb.h;
        // the swatch: real sprites where the contract publishes them
        int sw = h - 4;
        if (lk_tool == TL_FLOOR) {
            sspr(LK_FLOOR_SPR[val][0] % 8 * 16, LK_FLOOR_SPR[val][0] / 8 * 16, 16, 16,
                 x + 2, y + 2, sw, sw);
        } else if (lk_tool == TL_OBJECT) {
            int s = LK_OBJ[val].sprite;
            sspr(s % 8 * 16, s / 8 * 16, 16, 16, x + 2, y + 2, sw, sw);
        } else {
            int col = lk_tool == TL_WALL ? LKH_WALL_COL[val]
                    : lk_tool == TL_ROOM ? LK_ROOM[val].colour
                    : lk_tool == TL_ZONE ? LKH_ZONE_COL[val] : CLR_BROWN;
            rectfill(x + 2, y + 2, sw, sw, col);
            rect(x + 2, y + 2, sw, sw, LKH_C_FRAME);
            if (lk_tool == TL_DOOR) {
                line(x + 4, y + 4, x + 4, y + h - 6, LKH_C_HI);
                pset(x + sw - 1, y + 2 + sw / 2, LKH_C_ACC);
            }
        }
        if (!can) { fillp(FILL_CHECKER, -1); rectfill(x + 2, y + 2, sw, sw, CLR_BLACK); fillp_reset(); }

        lkh_fnt(FONT_SMALL);
        int tx = x + sw + 5, tw = w - sw - 7;
        int col = !can ? LKH_C_DIM : sel ? CLR_WHITE : LKH_C_TEXT;
        print(lkh_fit(lkh_arg_name(lk_tool, val), tw), tx, y + 3, col);
        if (lk_tool == TL_ROOM) {
            int rq = LK_ROOM[val].req[0];
            print(lkh_fit(rq ? str("needs %s", LK_OBJ[rq].name) : "no fittings needed", tw),
                  tx, y + 11, LKH_C_DIM);
        } else {
            print(cost > 0 ? lkh_money(cost) : "free", tx, y + 11, can ? LKH_C_ACC : LKH_C_BAD);
        }

        if (binside(cb, mouse_x(), mouse_y())) {
            const char *t2 = NULL;
            if (lk_tool == TL_WALL) t2 = LKH_WALL_TIP[val];
            else if (lk_tool == TL_DOOR) t2 = LKH_DOOR_TIP[val];
            else if (lk_tool == TL_ZONE) t2 = LKH_ZONE_TIP[val];
            else if (lk_tool == TL_ROOM) {
                static char rb[72];
                int o = 0;
                for (int k = 0; k < 4 && LK_ROOM[val].req[k]; k++) {
                    const char *nm = LK_OBJ[LK_ROOM[val].req[k]].name;
                    if (o && o < 68) rb[o++] = ',';
                    if (o && o < 68) rb[o++] = ' ';
                    for (int q = 0; nm[q] && o < 68; q++) rb[o++] = nm[q];
                }
                rb[o] = 0;
                t2 = o ? str("needs %s, min %d tiles", rb, LK_ROOM[val].min_area)
                       : str("min %d tiles, %s", LK_ROOM[val].min_area,
                             LK_ROOM[val].enclosed ? "must be enclosed" : "may be open sky");
            } else if (lk_tool == TL_OBJECT) {
                int sv = LK_OBJ[val].serves;
                t2 = sv < ND_COUNT ? str("serves %s, %d slot%s", LK_NEED[sv].name,
                                         LK_OBJ[val].slots, LK_OBJ[val].slots == 1 ? "" : "s")
                                   : (LK_OBJ[val].staff_only ? "staff equipment" : "a fitting");
            } else if (lk_tool == TL_FLOOR) {
                t2 = val == FL_DIRT ? "bare ground - free, and it is not a floor"
                                    : "paving. Rooms need a floor under them";
            }
            lkh_tip(!can ? str("%s - %s, you can spend %s", lkh_arg_name(lk_tool, val),
                               lkh_money(cost), lkh_money(lkh_budget()))
                         : lkh_arg_name(lk_tool, val), t2);
        }
        if (hit) {
            if (!can) {
                lk_hud_toast(str("%s costs %s; %s is free after the %s already queued",
                                 lkh_arg_name(lk_tool, val), lkh_money(cost),
                                 lkh_money(lkh_budget()), lkh_money(lkh_committed)));
                lk_sfx(SFX_DENY);
            } else lkh_set_arg(val);
        }
    }
    if (lk_tool == TL_OBJECT) {
        lkh_fnt(FONT_TINY);
        static const char *DIRN[4] = { "S", "N", "E", "W" };
        print_right(str("facing %s (R)", DIRN[lkh_obj_dir & 3]),
                    (int)(b.x + b.w - 4), (int)b.y + 4, LKH_C_ACC);
    }
}

// ═══ DRAW: the regime editor — the most important panel in the game ══════════
static void lkh_draw_regime(void) {
    Box b = lkh_pan;
    Box body = lkh_frame(b, "REGIME  -  the schedule the whole prison obeys", LKH_C_ACC);
    if (lkh_btn(box(b.x + b.w - 13, b.y + 1, 12, 11), "x", 0, 1, "close (esc)"))
        lkh_panel = LKH_P_NONE;

    // ── the brush row ──
    Box brush = lay_split(body, EDGE_TOP, 14, &body);
    lkh_fnt(FONT_SMALL);
    float lw = 30;
    Box lab = lay_split(brush, EDGE_LEFT, lw, &brush);
    print("paint:", (int)lab.x, (int)lab.y + 4, LKH_C_DIM);
    for (int a = 0; a < AC_COUNT; a++) {
        Box cb = lay_cell(brush, 0, AC_COUNT, a, 2);
        int sel = lkh_brush == a;
        int hit = lkh_btn(cb, NULL, sel, 1, NULL);
        int col = LK_ACT[a].colour;
        rectfill((int)cb.x + 1, (int)cb.y + 1, (int)cb.w - 2, (int)cb.h - 2, col);
        rect((int)cb.x, (int)cb.y, (int)cb.w, (int)cb.h, sel ? CLR_WHITE : LKH_C_FRAME);
        lkh_fnt(FONT_SMALL);
        const char *t = lkh_fit(cb.w > 44 ? LK_ACT[a].name : LK_ACT[a].abbr, (int)cb.w - 3);
        print(t, (int)(cb.x + (cb.w - text_width(t)) / 2), (int)(cb.y + (cb.h - 6) / 2),
              lkh_ink(col));
        if (binside(cb, mouse_x(), mouse_y()))
            lkh_tip(str("%s - drag it across the hours", LK_ACT[a].name),
                    "right-click an hour to pick its activity up");
        if (hit) { lkh_brush = a; lk_sfx(SFX_CLICK); }
    }

    // ── warnings, before the grid: they are why the panel exists ──
    int tally[AC_COUNT];
    for (int a = 0; a < AC_COUNT; a++) tally[a] = 0;
    for (int h = 0; h < 24; h++) if (lk_regime[h] < AC_COUNT) tally[lk_regime[h]]++;
    Box warn = lay_split(body, EDGE_BOTTOM, 8, &body);
    {
        const char *w1 = NULL; int wc = LKH_C_WARN;
        if (!tally[AC_EAT])              { w1 = "NO EAT SLOT - they will starve however good the kitchen is"; wc = LKH_C_BAD; }
        else if (!tally[AC_SLEEP])       { w1 = "NO SLEEP SLOT - exhaustion, then collapse"; wc = LKH_C_BAD; }
        else if (tally[AC_SLEEP] < 6)    w1 = str("only %d sleep hours - they will not keep up", tally[AC_SLEEP]);
        else if (!tally[AC_SHOWER])      w1 = "no shower slot - hygiene climbs and never falls";
        else if (!tally[AC_YARD] && !tally[AC_FREE]) w1 = "no yard or free time - boredom becomes volatility";
        else if (tally[AC_EAT] < 2)      w1 = "one meal a day is thin; two or three feeds a big prison";
        else                             { w1 = "a workable day: they sleep, eat, wash and have somewhere to be"; wc = LKH_C_GOOD; }
        lkh_fnt(FONT_SMALL);
        print(lkh_fit(w1, (int)warn.w), (int)warn.x, (int)warn.y + 1, wc);
    }

    // ── the 24-hour grid, one shared column register.  The rect comes from
    // lkh_relayout so the hit test and the drawing are the SAME rect. ──
    Box cells = lkh_rg_grid;
    Box hours = box(cells.x, cells.y - 7, cells.w, 7);
    (void)lay_split(body, EDGE_TOP, 7, &body);
    LayLane lane = lay_lane(cells, 24);
    float step = cells.w / 24.0f;
    int now_h = lkh_hour();
    for (int h = 0; h < 24; h++) {
        Box hb = lay_lane_cell(lane, hours, h, 0);
        Box cb = lay_lane_cell(lane, cells, h, 1);
        int a = lk_regime[h] < AC_COUNT ? lk_regime[h] : AC_FREE;
        int col = LK_ACT[a].colour;
        lkh_fnt(FONT_TINY);
        if (step >= 11 || (h % 3) == 0)
            print(str("%02d", h), (int)(hb.x + (step - text_width("00")) / 2), (int)hb.y,
                  h == now_h ? LKH_C_HI : LKH_C_DIM);
        rectfill((int)cb.x, (int)cb.y, (int)cb.w, (int)cb.h, col);
        if (step >= 15) {
            lkh_fnt(FONT_SMALL);
            const char *t = LK_ACT[a].abbr;
            print(t, (int)(cb.x + (cb.w - text_width(t)) / 2), (int)(cb.y + (cb.h - 6) / 2),
                  lkh_ink(col));
        } else if (step >= 8) {
            lkh_fnt(FONT_TINY);
            char one[2] = { LK_ACT[a].abbr[0], 0 };
            print(one, (int)(cb.x + (cb.w - text_width(one)) / 2), (int)(cb.y + (cb.h - 5) / 2),
                  lkh_ink(col));
        }
        if (h == now_h) {
            rect((int)cb.x - 1, (int)cb.y - 1, (int)cb.w + 2, (int)cb.h + 2, LKH_C_HI);
            float frac = clamp(lk.clock - (float)now_h, 0, 1);
            int px = (int)(cb.x + frac * cb.w);
            line(px, (int)cb.y, px, (int)(cb.y + cb.h), CLR_WHITE);
        }
    }
    if (binside(cells, mouse_x(), mouse_y())) {
        int h = (int)((mouse_x() - cells.x) / step);
        if (h >= 0 && h < 24)
            lkh_tip(str("%02d:00  %s", h, LK_ACT[lk_regime[h]].name),
                    "drag to paint hours; right-click picks the activity up");
    }

    // ── the tally ──
    Box tal = body;
    tal.y = cells.y + cells.h + 1;
    tal.h = 7;
    if (tal.y + tal.h <= b.y + b.h - 2) {
        lkh_fnt(FONT_TINY);
        int x = (int)tal.x;
        for (int a = 0; a < AC_COUNT; a++) {
            if (x > tal.x + tal.w - 26) break;
            rectfill(x, (int)tal.y + 1, 3, 3, LK_ACT[a].colour);
            const char *t = str("%s %dh", LK_ACT[a].abbr, tally[a]);
            print(t, x + 5, (int)tal.y, tally[a] ? LKH_C_TEXT : LKH_C_DIM);
            x += 5 + text_width(t) + 6;
        }
    }
}

// ═══ DRAW: finance ═══════════════════════════════════════════════════════════
static void lkh_draw_finance(void) {
    Box b = lkh_pan;
    Box body = lkh_frame(b, str("FINANCE  -  day %d", lk.day), LKH_C_ACC);
    if (lkh_btn(box(b.x + b.w - 13, b.y + 1, 12, 11), "x", 0, 1, "close (esc)"))
        lkh_panel = LKH_P_NONE;
    if (binside(body, mouse_x(), mouse_y()))
        lkh_tip("grades gate your intake", "a mean of 2 or better and the buses keep coming");

    lkh_fnt(FONT_SMALL);
    int x = (int)body.x, y = (int)body.y, w = (int)body.w;
    print("balance", x, y, LKH_C_DIM);
    print_right(lkh_money(lk.money), x + w, y, lk.money < 0 ? LKH_C_BAD : LKH_C_HI);
    y += 8;
    int delta = lk.money - lkh_day_open;
    print("today", x, y, LKH_C_DIM);
    print_right(str("%s%s", delta < 0 ? "" : "+", lkh_money(delta)), x + w, y,
                delta < 0 ? LKH_C_BAD : LKH_C_GOOD);
    y += 8;
    print("committed", x, y, LKH_C_DIM);
    print_right(lkh_money(lkh_committed), x + w, y, lkh_committed ? LKH_C_WARN : LKH_C_DIM);
    y += 10;

    // ── the ledger ──
    line(x, y - 2, x + w, y - 2, LKH_C_FRAME);
    int nl = lk_econ_lines();
    int gradesH = 6 * 8 + 14;
    int room = (int)(body.y + body.h) - y - gradesH;
    int rows = room / 7;
    if (rows < 2) rows = 2;
    int hard = ((int)(body.y + body.h) - y) / 7;        // never past the panel, whatever
    if (hard < 0) hard = 0;
    if (rows > hard) rows = hard;
    if (rows > nl) rows = nl;
    if (lkh_fin_scroll > nl - rows) lkh_fin_scroll = nl - rows;
    if (lkh_fin_scroll < 0) lkh_fin_scroll = 0;
    if (binside(body, mouse_x(), mouse_y()) && mouse_wheel() != 0)
        lkh_fin_scroll -= (int)mouse_wheel();
    clip(x, y, w, rows * 7 + 1);
    int net = 0;
    for (int i = 0; i < nl; i++) { int amt = 0; lk_econ_line(i, &amt); net += amt; }
    for (int i = 0; i < rows; i++) {
        int li = i + lkh_fin_scroll;
        int amt = 0;
        const char *lab = lk_econ_line(li, &amt);
        if (!lab) break;
        print(lkh_fit(lab, w - 44), x, y + i * 7, LKH_C_TEXT);
        print_right(str("%s%s", amt < 0 ? "" : "+", lkh_money(amt)), x + w, y + i * 7,
                    amt < 0 ? LKH_C_BAD : LKH_C_GOOD);
    }
    clip(0, 0, 0, 0);
    if (nl == 0) print("nothing booked yet today", x, y, LKH_C_DIM);
    if (nl > rows) {
        lkh_fnt(FONT_TINY);
        print_right(str("%d more - scroll", nl - rows - lkh_fin_scroll), x + w,
                    y + rows * 7, LKH_C_DIM);
    }
    y += rows * 7 + 3;
    int bot = (int)(body.y + body.h);                   // nothing spills past the panel
    lkh_fnt(FONT_SMALL);
    if (y + 7 > bot) return;
    line(x, y - 2, x + w, y - 2, LKH_C_FRAME);
    print("net", x, y, LKH_C_HI);
    print_right(str("%s%s", net < 0 ? "" : "+", lkh_money(net)), x + w, y,
                net < 0 ? LKH_C_BAD : LKH_C_GOOD);
    y += 10;

    // ── the five grades ──
    if (y + 7 > bot) return;
    line(x, y - 2, x + w, y - 2, LKH_C_FRAME);
    for (int g = 0; g < 5; g++) {
        if (y + 7 > bot) return;
        int v = lk.grade[g];
        if (v < 0) v = 0;
        if (v > 5) v = 5;
        print(LKH_GRADE_NAME[g], x, y, LKH_C_TEXT);
        lkh_pips(x + w - 31, y, v, 5, v >= 4 ? LKH_C_GOOD : v >= 2 ? LKH_C_WARN : LKH_C_BAD);
        y += 8;
    }
    if (y + 7 > bot) return;
    print("valuation", x, y, LKH_C_DIM);
    print_right(lkh_money(lk_valuation()), x + w, y, LKH_C_TEXT);
}

// ═══ DRAW: the help panel ════════════════════════════════════════════════════
static void lkh_draw_help(void) {
    Box b = lkh_pan;
    Box body = lkh_frame(b, "LOCKUP  -  how a prison works", LKH_C_ACC);
    if (lkh_btn(box(b.x + b.w - 13, b.y + 1, 12, 11), "x", 0, 1, "close (esc)"))
        lkh_panel = LKH_P_NONE;
    lkh_fnt(FONT_SMALL);
    int x = (int)body.x, y = (int)body.y, w = (int)body.w;
    static const char *const WHY[] = {
        "A need only falls when FIVE things line up:",
        "  1  a room that serves it EXISTS",
        "  2  it is VALID - enclosed, with the fittings its type demands",
        "  3  it can be WALKED to, through doors they may use",
        "  4  the REGIME grants an activity that sends them there",
        "  5  a bed / bench / shower head is FREE",
        "Break one and the need climbs. Volatility is the integral of",
        "unmet need - a riot is your own arithmetic coming back."
    };
    for (unsigned int i = 0; i < sizeof(WHY) / sizeof(WHY[0]); i++) {
        if (y + 7 > body.y + body.h - 40) break;
        print(lkh_fit(WHY[i], w), x, y, i == 0 ? LKH_C_HI : LKH_C_TEXT);
        y += 7;
    }
    y += 3;
    line(x, y - 1, x + w, y - 1, LKH_C_FRAME);
    y += 2;
    static const char *const KEYS[] = {
        "1-8 tools", "[ ] choice", "R rotate", "space pause",
        "WASD/arrows pan", "wheel scroll", "tab overlay", "T regime",
        "F money", "L lockdown", "X search", "C follow",
        "right-drag erase", "esc close", "H this", "mid-drag pan"
    };
    int cols = w > 240 ? 4 : 2;
    for (unsigned int i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        int cw = w / cols;
        int px = x + (int)(i % cols) * cw, py = y + (int)(i / cols) * 7;
        if (py + 7 > body.y + body.h) break;
        print(lkh_fit(KEYS[i], cw - 2), px, py, LKH_C_DIM);
    }
}

// ═══ DRAW: the prisoner inspector ════════════════════════════════════════════
static void lkh_draw_insp_actor(void) {
    int ai = lk_sel_actor;
    const Actor *a = &lk_a[ai];
    Box b = lkh_ins;
    Box body = lkh_frame(b, lkh_name(ai), LKH_C_HI);
    if (lkh_btn(box(b.x + b.w - 13, b.y + 1, 12, 11), "x", 0, 1, "close (esc)"))
        { lk_sel_actor = -1; lkh_follow = 0; return; }
    if (lkh_btn(box(b.x + b.w - 27, b.y + 1, 13, 11), "@", lkh_follow, 1,
                "keep the camera on them (C)"))
        lkh_follow = !lkh_follow;

    lkh_fnt(FONT_SMALL);
    int x = (int)body.x, y = (int)body.y, w = (int)body.w;
    int role = a->role < RL_COUNT ? a->role : 0;
    print(role == RL_PRISONER ? str("Prisoner  %s", lkh_sec_name(a->sec)) : LK_ROLE_NAME[role],
          x, y, role == RL_PRISONER && a->sec == SEC_MAX ? LKH_C_BAD : LKH_C_TEXT);
    y += 8;
    int st = a->state;
    int stc = (st == AS_FIGHT || st == AS_RIOT || st == AS_ESCAPE) ? LKH_C_BAD
            : (st == AS_DOWN || st == AS_RESTRAINED || st == AS_SOLITARY) ? LKH_C_WARN
            : LKH_C_HI;
    print(lkh_fit(lk_state_name(st), w), x, y, stc);
    y += 8;

    if (role == RL_PRISONER) {
        if (a->cell < 0) print("no cell assigned", x, y, LKH_C_BAD);
        else print(lkh_fit(str("%s #%d", LK_ROOM[lk_room[a->cell].type].name, a->cell), w),
                   x, y, LKH_C_TEXT);
        y += 9;
        lkh_meter(x, y, w, "hp ", a->health, a->health < 0.4f ? LKH_C_BAD : LKH_C_GOOD, 0);
        y += 7;
        lkh_meter(x, y, w, "vol", a->vol, lkh_heat(a->vol), 0);
        y += 7;
        lkh_meter(x, y, w, "sup", a->supp, CLR_TRUE_BLUE, 0);
        y += 9;

        int worst = lkh_worst_need(ai);
        print("NEEDS", x, y, LKH_C_DIM);
        print_right("full = desperate", x + w, y, LKH_C_DIM);
        y += 7;
        for (int n = 0; n < ND_COUNT; n++) {
            if (y + 6 > body.y + body.h - 16) break;
            float v = clamp(a->need[n], 0, 1);
            int hi = n == worst;
            print(LK_NEED[n].abbr, x, y, hi ? LKH_C_HI : LKH_C_TEXT);
            int bx = x + 20, bw = w - 20;
            bar(bx, y, bw, 5, v, LK_NEED[n].colour, CLR_DARKER_BLUE);
            rect(bx, y, bw, 5, v > 0.72f ? (blink(12) ? LKH_C_BAD : LKH_C_FRAME) : LKH_C_FRAME);
            if (hi) { pset(x - 2, y + 2, LKH_C_ACC); pset(x - 3, y + 1, LKH_C_ACC); pset(x - 3, y + 3, LKH_C_ACC); }
            y += 6;
        }
        y += 3;
        if (y + 14 <= body.y + body.h) {
            line(x, y - 2, x + w, y - 2, LKH_C_FRAME);
            if (worst < 0) print("content for now", x, y, LKH_C_GOOD);
            else {
                print(lkh_fit(str("worst: %s", lk_need_worst(ai)), w), x, y, LKH_C_WARN);
                lkh_fnt(FONT_TINY);
                const char *why = lkh_need_why(ai, worst);
                // two tiny lines, broken on the last SPACE that fits.  n is clamped
                // to the buffer BEFORE anything indexes with it.
                static char l1[96], l2[96];
                int n = 0;
                while (why[n] && n < 95) n++;
                for (int k = 0; k < n; k++) l1[k] = why[k];
                l1[n] = 0;
                int fitn = n, cut = 0;
                while (fitn > 4 && text_width(l1) > w) { fitn--; l1[fitn] = 0; }
                if (fitn < n) {
                    for (int k = fitn; k > 4; k--) if (why[k] == ' ') { fitn = k; break; }
                    l1[fitn] = 0;
                    cut = fitn + 1;
                }
                print(l1, x, y + 8, LKH_C_TEXT);
                if (cut && cut < n) {
                    int o = 0;
                    for (int k = cut; k < n && o < 95; k++) l2[o++] = why[k];
                    l2[o] = 0;
                    print(lkh_fit(l2, w), x, y + 14, LKH_C_TEXT);
                }
            }
        }
        if (a->contraband) {
            lkh_fnt(FONT_TINY);
            static char cb[40];
            int o = 0;
            if (a->contraband & CB_WEAPON) { const char *s = "weapon "; for (int k = 0; s[k]; k++) cb[o++] = s[k]; }
            if (a->contraband & CB_DRUGS)  { const char *s = "drugs ";  for (int k = 0; s[k]; k++) cb[o++] = s[k]; }
            if (a->contraband & CB_TOOL)   { const char *s = "tool ";   for (int k = 0; s[k]; k++) cb[o++] = s[k]; }
            if (a->contraband & CB_PHONE)  { const char *s = "phone";   for (int k = 0; s[k]; k++) cb[o++] = s[k]; }
            cb[o] = 0;
            print(lkh_fit(cb, w), x, (int)(body.y + body.h) - 5, LKH_C_BAD);
        }
    } else {
        lkh_meter(x, y, w, "hp ", a->health, a->health < 0.4f ? LKH_C_BAD : LKH_C_GOOD, 0);
        y += 9;
        lkh_fnt(FONT_TINY);
        print(str("wage %s a day", lkh_money(LK_ROLE_WAGE[role])), x, y, LKH_C_DIM);
        y += 7;
        print(role == RL_GUARD ? "patrols, escorts, breaks up fights"
            : role == RL_WORKMAN ? "builds whatever you designate"
            : role == RL_COOK ? "cooks, and carries trays to the serving table"
            : "treats the injured - without one, injuries kill", x, y, LKH_C_DIM);
    }
    if (binside(b, mouse_x(), mouse_y()) && !lkh_tip_on)
        lkh_tip("every bar here has a cause on the map", "esc closes, C follows");
}

// ═══ DRAW: the room inspector ════════════════════════════════════════════════
static void lkh_draw_insp_room(void) {
    int rid = lk_sel_room;
    const Room *r = &lk_room[rid];
    int ty = r->type < RM_COUNT ? r->type : RM_NONE;
    const RoomDef *rd = &LK_ROOM[ty];
    Box b = lkh_ins;
    Box body = lkh_frame(b, str("%s  #%d", rd->name, rid), rd->colour);
    if (lkh_btn(box(b.x + b.w - 13, b.y + 1, 12, 11), "x", 0, 1, "close (esc)"))
        { lk_sel_room = 0; return; }

    lkh_fnt(FONT_SMALL);
    int x = (int)body.x, y = (int)body.y, w = (int)body.w;
    if (r->valid) print("VALID - it works", x, y, LKH_C_GOOD);
    else          print("NOT VALID", x, y, LKH_C_BAD);
    y += 8;
    lkh_fnt(FONT_TINY);
    if (!r->valid) {
        const char *why;
        if (rd->enclosed && r->leaks) why = "the fill escaped outdoors - wall it in and roof it";
        else if (r->area < rd->min_area) why = str("too small: %d tiles, it needs %d",
                                                  (int)r->area, (int)rd->min_area);
        else if (r->missing) why = str("no %s in it yet", LK_OBJ[r->missing].name);
        else why = "not finished";
        print(lkh_fit(why, w), x, y, LKH_C_WARN);
        y += 6;
        if (r->missing) {
            print(lkh_fit(str("place one with tool 5 (%s)", LK_OBJ[r->missing].name), w),
                  x, y, LKH_C_DIM);
            y += 6;
        }
    }
    y += 2;
    lkh_fnt(FONT_SMALL);
    print(str("%d tiles", (int)r->area), x, y, LKH_C_TEXT);
    print_right(rd->prisoner_ok ? "prisoners" : "staff only", x + w, y, LKH_C_DIM);
    y += 8;
    if (rd->cap_obj) {
        print(lkh_fit(str("%d of %d %s", (int)r->used, (int)r->cap, LK_OBJ[rd->cap_obj].name), w),
              x, y, r->cap && r->used >= r->cap ? LKH_C_WARN : LKH_C_TEXT);
        y += 7;
        lkh_meter(x, y, w, NULL, r->cap ? (float)r->used / (float)r->cap : 0.0f,
                  r->cap && r->used >= r->cap ? LKH_C_BAD : LKH_C_GOOD, 0);
        y += 9;
    }
    lkh_fnt(FONT_TINY);
    // what it serves, straight out of the need table — never a second opinion
    {
        static char sb[64];
        int o = 0;
        for (int n = 0; n < ND_COUNT; n++) {
            if (LK_NEED[n].room != ty) continue;
            const char *nm = LK_NEED[n].name;
            if (o && o < 60) { sb[o++] = ','; sb[o++] = ' '; }
            for (int k = 0; nm[k] && o < 60; k++) sb[o++] = nm[k];
        }
        sb[o] = 0;
        if (o) { print(lkh_fit(str("serves %s", sb), w), x, y, LKH_C_DIM); y += 6; }
    }
    if (y + 6 <= body.y + body.h) {
        static char qb[72];
        int o = 0;
        for (int k = 0; k < 4 && rd->req[k]; k++) {
            const char *nm = LK_OBJ[rd->req[k]].name;
            if (o && o < 68) { qb[o++] = ','; qb[o++] = ' '; }
            for (int q = 0; nm[q] && o < 68; q++) qb[o++] = nm[q];
        }
        qb[o] = 0;
        print(lkh_fit(o ? str("requires %s", qb) : "requires nothing", w), x, y, LKH_C_DIM);
    }
}

// ═══ DRAW: the map readout, the legend, the toasts, the tooltip ══════════════
static void lkh_draw_readout(void) {
    if (lkh_hover_c < 0) return;
    const Tile *t = &lk_t[lkh_hover_c];
    static char s[120];
    int o = 0;
    #define LKH_CAT(txt) do { const char *_p = (txt); \
        for (int _k = 0; _p[_k] && o < 118; _k++) s[o++] = _p[_k]; } while (0)
    LKH_CAT(t->floor < FL_COUNT ? LK_FLOOR_NAME[t->floor] : "?");
    if (t->wall != WL_NONE) { LKH_CAT(" - "); LKH_CAT(LK_WALL_NAME[t->wall]); LKH_CAT(" wall"); }
    if (t->door != DR_NONE) {
        LKH_CAT(" - "); LKH_CAT(LK_DOOR_NAME[t->door]);
        if (t->locked) LKH_CAT(" (locked)");
    }
    int ob = t->obj;
    if (ob != OB_NONE && !t->obj_ref) { LKH_CAT(" - "); LKH_CAT(LK_OBJ[ob].name); }
    else if (t->obj_ref) LKH_CAT(" - furniture");
    if (t->room) {
        LKH_CAT(" - "); LKH_CAT(LK_ROOM[lk_room[t->room].type].name);
        LKH_CAT(lk_room[t->room].valid ? "" : " (invalid)");
    } else if (t->paint != RM_NONE) {
        LKH_CAT(" - painted "); LKH_CAT(LK_ROOM[t->paint].name); LKH_CAT(", not a room yet");
    }
    if (t->zone != ZN_OPEN) { LKH_CAT(" - "); LKH_CAT(LKH_ZONE_NAME[t->zone]); }
    if (t->job != JB_NONE) {
        LKH_CAT(" - queued: "); LKH_CAT(LKH_JOB_NAME[t->job]);
        if (t->claimed) LKH_CAT(" (being built)");
    }
    #undef LKH_CAT
    s[o] = 0;

    lkh_fnt(FONT_SMALL);
    int tw = text_width(s);
    int maxw = (int)lkh_v.w - 8;
    const char *txt = tw > maxw ? lkh_fit(s, maxw) : s;
    tw = text_width(txt);
    int bx = (int)lkh_v.x + 4, by = (int)lkh_floor_y - 10;
    rectfill(bx, by, tw + 6, 10, LKH_C_PANEL);
    rect(bx, by, tw + 6, 10, LKH_C_FRAME);
    print(txt, bx + 3, by + 2, LKH_C_TEXT);
}

static void lkh_draw_legend(void) {
    if (lk_overlay == OV_NONE) return;
    int rows = 0;
    int shown[8]; int nshown = 0;
    if (lk_overlay == OV_ROOMS) {
        for (int i = 1; i < lk_nroom && nshown < 8; i++) {
            int ty = lk_room[i].type;
            if (ty == RM_NONE) continue;
            int dup = 0;
            for (int k = 0; k < nshown; k++) if (shown[k] == ty) dup = 1;
            if (!dup) shown[nshown++] = ty;
        }
        rows = nshown ? nshown : 1;
    } else if (lk_overlay == OV_DEPLOY) rows = ZN_COUNT;
    else rows = 2;

    lkh_fnt(FONT_TINY);
    int w = 96, h = 22 + rows * 6;
    int bx = (int)(lkh_v.x + lkh_v.w) - 4 - w, by = (int)lkh_floor_y - h;
    if (lkh_ins.h > 0 && by < lkh_ins.y + lkh_ins.h) bx = (int)lkh_ins.x - 4 - w;
    if (bx < lkh_v.x) bx = (int)lkh_v.x;
    rectfill(bx, by, w, h, LKH_C_PANEL);
    rect(bx, by, w, h, LKH_C_FRAME);

    // every OV_* directly reachable, not just the next one in the cycle
    static const char *const OVK[OV_COUNT] = { "off", "room", "zone", "need", "risk" };
    for (int i = 0; i < OV_COUNT; i++) {
        Box cb = lay_cell(box((float)bx + 2, (float)by + 2, (float)w - 4, 9), 0, OV_COUNT, i, 1);
        if (lkh_btn(cb, OVK[i], lk_overlay == i, 1, LKH_OVL_TIP[i])) {
            lk_overlay = i; lk_sfx(SFX_CLICK);
        }
    }
    lkh_fnt(FONT_TINY);
    print(LKH_OVL_NAME[lk_overlay], bx + 3, by + 13, LKH_C_ACC);
    int y = by + 21;
    if (lk_overlay == OV_ROOMS) {
        if (!nshown) print("no rooms yet", bx + 3, y, LKH_C_DIM);
        for (int i = 0; i < nshown; i++) {
            rectfill(bx + 3, y + 1, 4, 4, LK_ROOM[shown[i]].colour);
            print(lkh_fit(LK_ROOM[shown[i]].name, w - 12), bx + 9, y, LKH_C_TEXT);
            y += 6;
        }
    } else if (lk_overlay == OV_DEPLOY) {
        for (int i = 0; i < ZN_COUNT; i++) {
            rectfill(bx + 3, y + 1, 4, 4, LKH_ZONE_COL[i]);
            print(LKH_ZONE_NAME[i], bx + 9, y, LKH_C_TEXT);
            y += 6;
        }
    } else {
        rectfill(bx + 3, y + 1, 4, 4, LKH_C_GOOD);
        print(lk_overlay == OV_NEEDS ? "met" : "calm", bx + 9, y, LKH_C_TEXT);
        y += 6;
        rectfill(bx + 3, y + 1, 4, 4, LKH_C_BAD);
        print(lk_overlay == OV_NEEDS ? "desperate" : "volatile", bx + 9, y, LKH_C_TEXT);
    }
}

static void lkh_draw_toasts(void) {
    if (!lkh_ntoast) return;
    lkh_fnt(FONT_SMALL);
    int cy = (int)lkh_floor_y - 12;
    if (lkh_hover_c >= 0) cy -= 11;                    // above the tile readout
    for (int i = lkh_ntoast - 1; i >= 0; i--) {
        float t = lkh_toast_t[i];
        const char *msg = lkh_toast_n[i] > 1 ? str("%s  x%d", lkh_toast[i], lkh_toast_n[i])
                                             : lkh_toast[i];
        int maxw = (int)lkh_v.w - 20;
        const char *txt = lkh_fit(msg, maxw);
        int tw = text_width(txt);
        int bx = (int)(lkh_v.x + (lkh_v.w - tw) * 0.5f) - 4;
        int fresh = t > LKH_TOAST_SECS - 0.18f;
        int dim = t < 0.9f;
        rectfill(bx, cy, tw + 8, 10, dim ? CLR_DARKER_BLUE : LKH_C_PANEL);
        rect(bx, cy, tw + 8, 10, fresh ? LKH_C_HI : (dim ? CLR_DARKER_BLUE : LKH_C_FRAME));
        print(txt, bx + 4, cy + 2, dim ? LKH_C_DIM : (fresh ? LKH_C_HI : LKH_C_TEXT));
        cy -= 11;
        if (cy < lkh_v.y) break;
    }
}

static void lkh_draw_tip(void) {
    if (!lkh_tip_on || !lkh_mouse_seen) return;
    lkh_fnt(FONT_SMALL);
    int w1 = text_width(lkh_tipb1);
    int w2 = lkh_tipb2[0] ? text_width(lkh_tipb2) : 0;
    int w = (w1 > w2 ? w1 : w2) + 8;
    int h = w2 ? 18 : 11;
    int maxw = (int)lkh_scr.w - 8;
    if (w > maxw) w = maxw;
    int x = mouse_x() + 10, y = mouse_y() - h - 6;
    if (x + w > lkh_scr.w - 2) x = (int)lkh_scr.w - 2 - w;
    if (x < 2) x = 2;
    if (y < 2) y = mouse_y() + 14;
    if (y + h > lkh_scr.h - 2) y = (int)lkh_scr.h - 2 - h;
    lkh_shadow(box((float)x, (float)y, (float)w, (float)h));
    rectfill(x, y, w, h, CLR_BROWNISH_BLACK);
    rect(x, y, w, h, LKH_C_EDGE);
    print(lkh_fit(lkh_tipb1, w - 6), x + 4, y + 2, LKH_C_HI);
    if (w2) print(lkh_fit(lkh_tipb2, w - 6), x + 4, y + 10, LKH_C_TEXT);
}

// ═══ DRAW ════════════════════════════════════════════════════════════════════
void lk_hud_draw(void) {
    lkh_relayout();
    camera(0, 0);
    pal_reset();
    fillp_reset();
    lkh_tip_on = 0; lkh_tipb1[0] = 0; lkh_tipb2[0] = 0;
    ui_begin();                                        // note B — the cart must NOT

    lkh_draw_world_layer();

    lkh_draw_top();
    lkh_draw_toolbar();
    lkh_draw_readout();
    lkh_draw_legend();
    lkh_draw_picker();
    if      (lkh_panel == LKH_P_REGIME)  lkh_draw_regime();
    else if (lkh_panel == LKH_P_FINANCE) lkh_draw_finance();
    if (lkh_ins.h > 0) {
        if (lk_sel_actor >= 0 && lk_a[lk_sel_actor].alive) lkh_draw_insp_actor();
        else if (lk_sel_room > 0) lkh_draw_insp_room();
    }
    if (lkh_panel == LKH_P_HELP) lkh_draw_help();
    lkh_draw_toasts();
    lkh_draw_tip();

    ui_end();
    // hand every sticky piece of engine state back the way art.h expects to find it
    lkh_fnt(FONT_NORMAL);
    colorkey(CLR_BLACK);
    pal_reset();
    fillp_reset();
    camera(0, 0);
    clip(0, 0, 0, 0);
}

#endif // LOCKUP_HUD_H
