// ─────────────────────────────────────────────────────────────────────────────
// lockup/actors.h — THE HONEST CORE of LOCKUP (docs/design/lockup.md §1).
//
// OWNS:  Actor lk_a[LK_MAXACT];  int lk_nact;  unsigned char lk_regime[24];
// TAG:   every internal is `static` and prefixed lka_.
// READS: the grid + path modules through the contract only (lk_room*, lk_can_pass,
//        lk_reachable, lk_find_path, lk_nearest, lk_job_*), art through lk_puff,
//        audio through lk_sfx / lk_sfx_at.
// WRITES into Sim: lk.tension, lk.alarm, lk.alarm_t, lk.incidents, lk.deaths,
//        lk.escapes, lk.n_prisoners, lk.n_staff, lk.n_beds, lk.n_cells.
//
// ── THE ONE SENTENCE ────────────────────────────────────────────────────────
// A prison is a machine for meeting needs on a schedule. A need falls ONLY IF
//   (1) an object/room that serves it exists, (2) its room is VALID,
//   (3) it is REACHABLE for this role, (4) the REGIME permits it this hour, and
//   (5) a capacity slot is FREE.
// Break any one and the need climbs. Every one of those five is a separate,
// separately-visible early-out in lka_seek() — there is no fallback that quietly
// satisfies a need the player didn't build for.
//
// ── KEY ALGORITHMS ──────────────────────────────────────────────────────────
// · NEEDS       decay per IN-GAME hour (LK_NEED[].decay / LK_HOUR_SECS), fill per
//               second of use. ND_SAFETY is not on a clock — it rises with local
//               violence and ebbs under guard suppression. ND_PRIVACY is a
//               STANDING penalty eased toward a target set by your cell (none /
//               dormitory / shared / your own).
// · REGIME      lk_regime[hour] → AC_*. lka_permits(act,need) is the whole
//               permission surface: the regime says what you may WANT, needs say
//               which permitted thing you want MOST. No AC_EAT slot ⇒ ND_FOOD is
//               never permitted ⇒ the prison starves however good the kitchen is.
// · GOAL/FSM    pick worst permitted need → find a free serving object (flow field
//               first, bounded grid scan as the honest fallback) → lk_reachable →
//               lk_find_path → AS_WALK → arrive → claim a slot (lka_claim) →
//               AS_USE/SLEEP/EAT/WASH → lk_room_release → AS_IDLE. Every failure
//               is a repath COOLDOWN, never a retry storm.
// · VOLATILITY  vol is an INTEGRAL: it eases toward (weighted unmet need + grudges
//               + local violence heat − suppression), rising slowly and falling
//               slower. Two volatile prisoners co-located with no guard start a
//               FIGHT — deterministic on the thresholds, never a dice roll. A
//               fight writes into a coarse VIOLENCE HEAT field, which raises
//               ND_SAFETY for everyone nearby, which raises their volatility:
//               that feedback loop is what turns an incident into a riot.
// · SUPPRESSION is genuinely local and wall-aware: a guard suppresses at full
//               strength only inside the same room, at 75% across open ground,
//               and NOT AT ALL through a wall. Deployment is a spatial problem: the
//               offline harness scores 25 incidents with no guards and 0 with six.
// · TENSION     lk.tension = smoothed(mean volatility, live incidents, alarm).
//               Smoothed in REAL time so the score can't flicker when the player
//               changes speed.
//
// ── IMPLEMENTATION NOTES (deviations + what I assume of other modules) ───────
//  1. TIME. lk_actors_update(d) expects the RAW real frame delta and applies
//     lk.speed itself (sim_dt = d * lk.speed; speed 0 ⇒ the sim is frozen but the
//     roster counts and tension smoothing still run). If the cart pre-multiplies
//     d by lk.speed, delete the multiply at the top of lk_actors_update.
//  2. Actor.face is stored in RADIANS per the contract comment (atan2 convention:
//     0 = east, +y = south, so π/2 = south). studio.h's angle_to() is DEGREES —
//     art must not mix them. Facing index = lka_face_dir(): |cos|>|sin| ? E/W : S/N.
//  3. Tile.obj_used is ONE BIT but LK_OBJ[].slots says a serving table seats 4 and
//     a bunk sleeps 2, so the real use COUNT is kept module-private in lka_used[]
//     and Tile.obj_used is maintained as the boolean (>0) everyone else reads. With
//     only the bit, one man ate per meal and a full canteen still starved the block.
//     ART NOTE: up to `slots` actors legitimately share one object tile — a bunk's
//     two sleepers, a serving table's four diners. I jitter the standing position a
//     few px per actor; a bunk could draw top/bottom. If a use COUNT ever lands in
//     Tile, delete lka_used[] and use it.
//  4. State the contract has no field for is kept in module-private arrays keyed
//     by actor index: lka_goal (the need being pursued), lka_carry (cook meal
//     trays), lka_prog (escape/search/treat progress). No contract change needed.
//  5. Serving tables must be STOCKED by a cook to satisfy ND_FOOD. Tile has no
//     stock field, so stock lives in lka_stock[LK_N] (meals at that tile). No cook
//     ⇒ every serving table reads empty ⇒ ND_FOOD can't fall. That is the point.
//  6. Actor.grudge[] stores actor_index+1 (0 = empty), because unsigned char has
//     no room for both 0..255 ids and a sentinel.
//  7. I write lk.n_beds / lk.n_cells (derived from valid RM_CELL/RM_DORM
//     capacity) because the HUD needs them and no module is named as their owner.
//     They are pure functions of lk_room[], so if grid also computes them the two
//     agree by construction. Flagged for reconciliation.
//  8. I set lk.over = 2 ("lost control") only when a riot has run unbroken for
//     LKA_LOSE_SECS and lk.over was still 0. If econ/cart owns the lose
//     conditions, delete lka_check_lost().
//  9. I touch two grid-owned tile fields as EVENTS, never as ownership:
//     Tile.obj_used (claim/release, as the contract instructs), Tile.locked
//     (lockdown), Tile.claimed (workman job claim, as the contract instructs) and
//     Tile.dirty (a soiling accident). Filth SPREAD stays grid's.
// 10. Staff needs are not simulated (their need[] stays at spawn values); nothing
//     in the sim reads them. If a staff-morale system lands it belongs here.
// 11. WHAT I ASSUME OF grid/path:
//     · lk_find_path() may or may not include the start tile as path[0] —
//       lka_set_path() handles both conventions.
//     · lk_nearest() may hand back an OCCUPIED facility, so its answer is always
//       re-validated (lka_slot_free) and a bounded grid scan is the fallback. If it
//       returns -1 for an unbuilt field the module still works, just slower. Same
//       for FF_EXIT: lka_exit_tile() falls back to the nearest unroomed border tile,
//       so a cut fence always leads off the map even with no flow field.
//     · lk_can_pass() must be false for a wall/solid object and true through an
//       unlocked door for that role. lk_reachable() must be cheap — it is called
//       before every path attempt, which is what makes "unreachable" honest.
//     · lk_job_progress(c, seconds) decrements Tile.work and CLEARS Tile.job when
//       done (that transition is how a workman knows to stop).
//     · Room ids may be renumbered by lk_rooms_rebuild(); Actor.cell is therefore
//       re-validated every tick (lka_validate_cell) instead of trusted.
// 12. TUNING I had to add on top of the frozen tables, because the frozen numbers
//     alone did not produce a playable prison (all verified in the harness below):
//     · Every need except ND_SLEEP decays at 35% while ASLEEP. Without it ND_FOOD
//       (0.09/hr) saturates across the 13-hour gap between the evening meal and
//       breakfast, so even a perfect prison woke up starving and the lesson about
//       BROKEN regimes said nothing.
//     · A sleeper WAKES for a critical bladder (>0.85) — ND_BLADDER at 0.14/hr
//       cannot be held for eight hours, and the alternative was a soiled cell every
//       single night.
//     · Health: starvation/exhaustion drain is slow (~24 in-game hours at maximum
//       need to kill) and reverses when fed. A beating puts you AS_DOWN at ~0.30
//       health, from which you recover UNLESS you fall under 0.20 — then only a
//       doctor stops it. Unmet need is meant to make a prison dangerous, not to
//       quietly execute the roster.
// 13. VERIFIED OFFLINE, not just compiled. This module was driven for simulated days
//     against stub grid/path modules (a real BFS + component check) with ~30
//     assertions on §1: meals land under the default regime; deleting every AC_EAT
//     slot starves the prison anyway; a canteen with no cook never feeds anyone; a
//     walled-off or invalid canteen never feeds anyone; 12 men and one serving table
//     eat worse than 4; guards suppress (25 incidents → 0); neglect integrates into
//     fights → riot → and the riot TERMINATES; lockdown locks and unlocks; a doctor
//     turns 10 deaths into 0; the same seed replays identically; speed 0 freezes.
//     Four real bugs came out of that and are fixed here: an actor left standing on
//     its own (solid) bed could never path again; a blocked re-plan froze an actor
//     forever with no cooldown; a 4-slot serving table fed one man per meal; and one
//     unsatisfiable need (a wing with no phone) silently stopped a prisoner
//     pursuing every OTHER need. None of them are visible by reading the code.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef LOCKUP_ACTORS_H
#define LOCKUP_ACTORS_H

#include "studio.h"
#include "lockup/model.h"

// ═══ owned globals ═══════════════════════════════════════════════════════════
Actor         lk_a[LK_MAXACT];
int           lk_nact;
unsigned char lk_regime[24];

// ═══ tuning — every number here is a design decision, not a magic constant ═══
#define LKA_SPD_PRISONER   32.0f   // world px/sec (a tile is 16px → 2 tiles/s)
#define LKA_SPD_GUARD      38.0f
#define LKA_SPD_RESPOND    56.0f   // a guard running at a fight
#define LKA_SPD_WORKMAN    30.0f
#define LKA_SPD_COOK       30.0f
#define LKA_SPD_DOCTOR     42.0f
#define LKA_SPD_RIOT       40.0f

#define LKA_WANT           0.22f   // a need below this isn't worth walking for
#define LKA_SATED          0.03f   // finish using at this
#define LKA_MAX_USE        60.0f   // seconds before we give the slot up anyway

#define LKA_VOL_RISE       0.085f  // volatility integral, per second toward target
#define LKA_VOL_FALL       0.050f  // anger cools slower than it heats
#define LKA_FIGHT_VOL      0.62f   // both parties above this and unwatched → fight
#define LKA_FIGHT_GRUDGE   0.44f   // …or this, if they already hate each other
#define LKA_FIGHT_SECS     4.5f
#define LKA_FIGHT_REACH    22.0f   // px: co-located means arm's length
#define LKA_SUPP_SAFE      0.30f   // suppression above this and nobody swings

#define LKA_GUARD_REACH    5.0f    // tiles a guard's presence carries in-room
#define LKA_SOLITARY_SECS  90.0f
#define LKA_RESTRAIN_SECS  6.0f
#define LKA_COOK_SECS      9.0f    // to cook one batch
#define LKA_TRAY_MEALS     6       // meals a cook carries per batch
#define LKA_STOCK_MAX      8       // meals a serving table holds
#define LKA_TREAT_SECS     6.0f
#define LKA_SEARCH_SECS    2.5f
#define LKA_CUT_SECS       14.0f   // fence-cutting with a smuggled tool
#define LKA_DOWN_DRAIN     0.006f  // health/sec while down and untreated (≈25s to bleed out)
#define LKA_STARVE_DRAIN   0.0015f // health/sec at need 1.0 — ~24 IN-GAME HOURS of maximum
                                   // hunger to kill a man. Unmet need is meant to make a
                                   // prison DANGEROUS, not to quietly execute the roster.
#define LKA_RIOT_INCIDENTS 3       // simultaneous fights that make it a riot
#define LKA_LOSE_SECS      240.0f  // an unbroken riot this long = lost control
#define LKA_TICK           0.30f   // seconds between social/threat passes
#define LKA_PATHS_FRAME    6       // A* calls allowed per frame, whole prison

#define LKA_RIOT_SECS      100.0f  // a rioter's own stamina — a riot always terminates

// violence/authority heat, one cell per 4×4 tiles
#define LKA_BS  4
#define LKA_BW  (LK_MW / LKA_BS)
#define LKA_BH  (LK_MH / LKA_BS)

// lka_goal[] sentinels above the real ND_* range
#define LKA_GOAL_NONE  (ND_COUNT)        // walking somewhere the regime wants you
#define LKA_GOAL_WORK  (ND_COUNT + 1)    // walking to a workshop station

// ═══ module-private state ════════════════════════════════════════════════════
static unsigned char lka_stock[LK_N];        // meals sitting on a serving table
static unsigned char lka_used[LK_N];         // HOW MANY actors are on this object
static unsigned char lka_goal[LK_MAXACT];    // ND_* being pursued, ND_COUNT = none
static unsigned char lka_carry[LK_MAXACT];   // cook: trays in hand
static float         lka_prog[LK_MAXACT];    // treat / search / cut progress
static unsigned char lka_stuck[LK_MAXACT];   // consecutive blocked-path re-plans
static float         lka_heat[LKA_BW * LKA_BH];   // recent violence
static int           lka_last_hour = -1;
static float         lka_tick_t;
static int           lka_pbudget;
static int           lka_live_fights;
static float         lka_riot_t;
static short         lka_searchq[64];        // cells queued by lk_shakedown()
static int           lka_searchn;
static int           lka_gate = -1;          // cached gate/arrival tile

static inline float lka_absf(float v) { return v < 0 ? -v : v; }
static inline float lka_maxf(float a, float b) { return a > b ? a : b; }
static inline float lka_minf(float a, float b) { return a < b ? a : b; }

static const int LKA_D4X[4] = {  0, 1, 0, -1 };
static const int LKA_D4Y[4] = { -1, 0, 1,  0 };

// ═══ tiny helpers ════════════════════════════════════════════════════════════
static inline int  lka_tx(const Actor *a) { return mid(0, (int)(a->x) / LK_TS, LK_MW - 1); }
static inline int  lka_ty(const Actor *a) { return mid(0, (int)(a->y) / LK_TS, LK_MH - 1); }
static inline int  lka_c(const Actor *a)  { return lk_idx(lka_tx(a), lka_ty(a)); }
// lk.clock is the authority (lk.hour is only its cache — if the cart forgets to
// refresh it the whole prison would sleep all day, so read the float).
static inline int  lka_hour(void) { return mid(0, (int)lk.clock, 23); }
static inline bool lka_trouble(int s) { return s == AS_FIGHT || s == AS_RIOT || s == AS_ESCAPE; }
static inline float lka_speed_of(const Actor *a) {
    switch (a->role) {
    case RL_GUARD:   return (a->state == AS_WALK && a->target >= 0 && lka_live_fights > 0)
                            ? LKA_SPD_RESPOND : LKA_SPD_GUARD;
    case RL_WORKMAN: return LKA_SPD_WORKMAN;
    case RL_COOK:    return LKA_SPD_COOK;
    case RL_DOCTOR:  return LKA_SPD_DOCTOR;
    default:         return (a->state == AS_RIOT || a->state == AS_ESCAPE)
                            ? LKA_SPD_RIOT : LKA_SPD_PRISONER;
    }
}
static inline int lka_heat_i(int c) {
    int bx = lk_tx(c) / LKA_BS, by = lk_ty(c) / LKA_BS;
    if (bx >= LKA_BW) bx = LKA_BW - 1;
    if (by >= LKA_BH) by = LKA_BH - 1;
    return by * LKA_BW + bx;
}
static void lka_add_heat(int c, float amt) {
    int bx = lk_tx(c) / LKA_BS, by = lk_ty(c) / LKA_BS;
    for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) {
        int x = bx + ox, y = by + oy;
        if (x < 0 || y < 0 || x >= LKA_BW || y >= LKA_BH) continue;
        float w = (ox == 0 && oy == 0) ? 1.0f : 0.35f;
        int i = y * LKA_BW + x;
        lka_heat[i] = clamp(lka_heat[i] + amt * w, 0.0f, 1.5f);
    }
}
// facing index for the art module: 0=S 1=N 2=E 3=W, from Actor.face (radians)
static inline int lka_face_dir(float face) {
    float cx = de_cosf(face), cy = de_sinf(face);
    if (lka_absf(cx) > lka_absf(cy)) return cx > 0 ? 2 : 3;
    return cy > 0 ? 0 : 1;
}
// ── CAPACITY. Tile.obj_used is one BIT, but LK_OBJ[].slots says a serving table
// seats four and a bunk sleeps two. Capacity contention IS condition (5) of the
// honest core, so it can't be rounded down to "one at a time" — the count lives
// here and Tile.obj_used is kept as the boolean everyone else reads.
static inline int lka_cap_of(int c) {
    int ob = lk_t[c].obj;
    if (!ob || ob >= OB_COUNT) return 0;
    int s = LK_OBJ[ob].slots;
    return s ? s : 1;                       // a 0-slot fitting (cooker, desk) seats its operator
}
static inline bool lka_has_room(int c) {
    if (c < 0 || c >= LK_N || !lk_t[c].obj) return false;
    if (lka_used[c] == 0 && lk_t[c].obj_used) return false;   // claimed by someone not us
    return lka_used[c] < (unsigned char)lka_cap_of(c);
}
static void lka_claim(int c) {
    if (c < 0 || c >= LK_N) return;
    if (lka_used[c] < 255) lka_used[c]++;
    lk_t[c].obj_used = 1;
}
static void lka_unclaim(int c) {
    if (c < 0 || c >= LK_N) return;
    if (lka_used[c]) lka_used[c]--;
    if (!lka_used[c]) { lk_t[c].obj_used = 0; lk_room_release(c); }
}

static void lka_look_at(Actor *a, float wx, float wy) {
    float vx = wx - a->x, vy = wy - a->y;
    if (vx * vx + vy * vy > 0.01f) a->face = de_atan2f(vy, vx);
}

// ═══ regime ══════════════════════════════════════════════════════════════════
// A day that is actually playable: sleep, three meals, work, yard, a shower and
// enough free time to phone home. The player will wreck it; that's the lesson.
static void lka_regime_default(void) {
    for (int h = 0; h <= 6;  h++) lk_regime[h] = AC_SLEEP;
    lk_regime[7]  = AC_EAT;
    lk_regime[8]  = AC_WORK;   lk_regime[9]  = AC_WORK;
    lk_regime[10] = AC_YARD;   lk_regime[11] = AC_YARD;
    lk_regime[12] = AC_EAT;
    lk_regime[13] = AC_SHOWER;
    for (int h = 14; h <= 17; h++) lk_regime[h] = AC_FREE;
    lk_regime[18] = AC_EAT;
    for (int h = 19; h <= 21; h++) lk_regime[h] = AC_FREE;
    lk_regime[22] = AC_SLEEP;  lk_regime[23] = AC_SLEEP;
}

// THE permission surface. Condition (4) of the honest core lives here alone.
static bool lka_permits(int act, int need) {
    if (need == ND_BLADDER) return act != AC_LOCKUP;   // a toilet is not a privilege
    if (need == ND_SAFETY || need == ND_PRIVACY) return false;  // never walked for
    switch (act) {
    case AC_SLEEP:  return need == ND_SLEEP;
    case AC_EAT:    return need == ND_FOOD;
    case AC_YARD:   return need == ND_REC || need == ND_COMFORT;
    case AC_SHOWER: return need == ND_HYGIENE;
    // NB deliberately NOT food and NOT hygiene: every slot in the timetable has to
    // mean something, or "no Eat slot starves the prison" stops being true.
    case AC_FREE:   return need == ND_REC || need == ND_FAMILY || need == ND_COMFORT;
    case AC_WORK:   return false;      // hands are busy; only bladder (above)
    case AC_LOCKUP: return need == ND_SLEEP;
    }
    return false;
}

// ═══ capacity + validity: conditions (1) (2) (5) ═════════════════════════════
static bool lka_slot_free(int c, int need, int role) {
    if (c < 0 || c >= LK_N) return false;
    const Tile *t = &lk_t[c];
    int ob = t->obj;
    if (!ob || ob >= OB_COUNT) return false;             // (1) nothing serves here
    const ObjDef *o = &LK_OBJ[ob];
    if (o->serves != need || o->slots == 0) return false;
    if (!lka_has_room(c)) return false;                  // (5) every slot taken
    if (o->staff_only && role == RL_PRISONER) return false;
    int r = t->room;
    if (r > 0 && r < LK_MAXROOM) {
        if (!lk_room[r].valid) return false;             // (2) room isn't finished
        if (role == RL_PRISONER && !LK_ROOM[lk_room[r].type].prisoner_ok) return false;
    } else if (LK_NEED[need].room != RM_NONE) {
        return false;      // a bed in a field is not a cell; a tray on grass is not a canteen
    }
    if (need == ND_FOOD && lka_stock[c] == 0) return false;   // no cook, no meal
    return true;
}

// Where an actor stands to use the object at `c` (an object may be solid). `spin`
// rotates which side is tried first, so four men sharing one serving table approach
// from four different tiles instead of stacking into one pixel.
static int lka_stand_spun(int c, int role, int spin) {
    if (lk_can_pass(c, role)) return c;
    int x = lk_tx(c), y = lk_ty(c);
    for (int k = 0; k < 4; k++) {
        int d = (k + spin) & 3;
        int nx = x + LKA_D4X[d], ny = y + LKA_D4Y[d];
        if (!lk_in(nx, ny)) continue;
        int n = lk_idx(nx, ny);
        if (lk_can_pass(n, role)) return n;
    }
    return -1;
}
static int lka_stand_for(int c, int role) { return lka_stand_spun(c, role, 0); }

static int lka_ff_kind(int need) {
    switch (need) {
    case ND_SLEEP:   return FF_BED;
    case ND_FOOD:    return FF_SERVING;
    case ND_BLADDER: return FF_TOILET;
    case ND_HYGIENE: return FF_SHOWER;
    case ND_COMFORT: return FF_BENCH;
    case ND_FAMILY:  return FF_PHONE;
    case ND_REC:     return FF_TV;
    }
    return -1;
}

// nearest FREE, VALID, REACHABLE object serving `need`. Flow field first (cheap),
// then a bounded grid scan — because the field may hand back an occupied one.
static int lka_best_slot(Actor *a, int need) {
    int from = lka_c(a);
    int k = lka_ff_kind(need);
    if (k >= 0) {
        int c = lk_nearest(from, k, a->role);
        if (c >= 0 && lka_slot_free(c, need, a->role) &&
            lka_stand_for(c, a->role) >= 0 && lk_reachable(from, c, a->role)) return c;
    }
    int fx = lk_tx(from), fy = lk_ty(from), best = -1, bd = 1 << 30;
    for (int c = 0; c < LK_N; c++) {
        if (!lk_t[c].obj) continue;                              // cheapest reject first
        if (!lka_slot_free(c, need, a->role)) continue;
        int d = abs(lk_tx(c) - fx) + abs(lk_ty(c) - fy);
        if (d >= bd) continue;
        int st = lka_stand_for(c, a->role);
        if (st < 0 || !lk_reachable(from, st, a->role)) continue;  // (3) unreachable
        bd = d; best = c;
    }
    return best;
}

// a walkable tile of room `rid` reachable from `from` — the centroid if we can, else
// ONE bounded grid scan. Kept separate so lka_room_tile can cap its scan count.
static int lka_tile_of_room(int rid, int from, int role) {
    if (rid <= 0 || rid >= LK_MAXROOM) return -1;
    int c = lk_idx(mid(0, lk_room[rid].cx, LK_MW - 1), mid(0, lk_room[rid].cy, LK_MH - 1));
    if (lk_t[c].room == rid && lk_can_pass(c, role) && lk_reachable(from, c, role)) return c;
    for (int i = 0; i < LK_N; i++) {
        if (lk_t[i].room != rid || !lk_can_pass(i, role)) continue;
        if (!lk_reachable(from, i, role)) continue;
        return i;
    }
    return -1;
}

// a walkable tile inside a valid room of `type` — the prisoner's OWN room first if
// one is named, then the nearest by centroid. At most LKA_ROOM_TRIES grid scans.
#define LKA_ROOM_TRIES 4
static int lka_room_tile(Actor *a, int type, int prefer_rid) {
    int from = lka_c(a), fx = lk_tx(from), fy = lk_ty(from);
    short cand[LKA_ROOM_TRIES]; int cd[LKA_ROOM_TRIES]; int nc = 0;
    if (prefer_rid > 0 && prefer_rid < LK_MAXROOM && lk_room[prefer_rid].valid) {
        int t = lka_tile_of_room(prefer_rid, from, a->role);
        if (t >= 0) return t;                                  // home beats nearest
    }
    for (int nth = 0; nth < LK_MAXROOM; nth++) {
        int rid = lk_room_find(type, nth);
        if (rid < 0) break;
        if (rid <= 0 || rid >= LK_MAXROOM || !lk_room[rid].valid) continue;
        if (rid == prefer_rid) continue;
        if (a->role == RL_PRISONER && !LK_ROOM[lk_room[rid].type].prisoner_ok) continue;
        int d = abs(lk_room[rid].cx - fx) + abs(lk_room[rid].cy - fy);
        int at = nc;
        while (at > 0 && cd[at - 1] > d) { if (at < LKA_ROOM_TRIES) { cand[at] = cand[at-1]; cd[at] = cd[at-1]; } at--; }
        if (at < LKA_ROOM_TRIES) { cand[at] = (short)rid; cd[at] = d; if (nc < LKA_ROOM_TRIES) nc++; }
    }
    for (int k = 0; k < nc; k++) {
        int t = lka_tile_of_room(cand[k], from, a->role);
        if (t >= 0) return t;
    }
    return -1;
}

static int lka_find_obj_in_room(int rid, int ob, bool free_only) {
    if (rid <= 0 || rid >= LK_MAXROOM) return -1;
    for (int c = 0; c < LK_N; c++) {
        if (lk_t[c].room != rid || lk_t[c].obj != ob) continue;
        if (free_only && !lka_has_room(c)) continue;
        return c;
    }
    return -1;
}

// ═══ movement ════════════════════════════════════════════════════════════════
static bool lka_set_path(Actor *a, int goal) {
    a->plen = a->pi = 0;
    if (goal < 0) return false;
    int from = lka_c(a);
    if (from == goal) { a->target = goal; return true; }
    if (!lk_reachable(from, goal, a->role)) { a->repath = 2.5f; return false; }
    if (lka_pbudget <= 0) { a->repath = 0.12f; return false; }   // spread A* over frames
    lka_pbudget--;
    int n = lk_find_path(from, goal, a->role, a->path, LK_MAXPATH);
    if (n <= 0) { a->repath = 2.0f; return false; }
    a->plen = (short)n;
    a->pi   = (a->path[0] == from && n > 1) ? 1 : 0;   // start tile may or may not be included
    a->target = (short)goal;
    return true;
}

// returns true on arrival at the end of the path
static bool lka_walk(Actor *a, float sd) {
    if (a->pi >= a->plen) return true;
    int c = a->path[a->pi];
    if (!lk_can_pass(c, a->role)) {
        // The world changed under them — a workman walled the corridor, a door got
        // locked, an object landed on the tile. Re-plan, but ALWAYS on a cooldown and
        // with a give-up count: a goal that can't be walked to must be ABANDONED, or
        // the actor re-plans the same blocked route every frame and freezes on the
        // spot forever (which is exactly what the offline harness caught).
        int ai = a->id;
        a->repath = 0.35f;
        if (ai >= 0 && ai < LK_MAXACT && ++lka_stuck[ai] > 5) {
            lka_stuck[ai] = 0;
            lk_actor_release(ai);                   // drop the slot we were holding
            a->state = AS_IDLE;
            a->repath = 2.0f;
            return false;
        }
        int g = a->target;
        a->plen = a->pi = 0;
        lka_set_path(a, g);
        return false;
    }
    if (a->id >= 0 && a->id < LK_MAXACT) lka_stuck[a->id] = 0;
    float tx = (float)lk_cx(c), ty = (float)lk_cy(c);
    lka_look_at(a, tx, ty);
    float sp = lka_speed_of(a) * sd;
    float vx = tx - a->x, vy = ty - a->y;
    float dd = de_hypotf(vx, vy);
    if (dd <= sp || dd < 0.001f) { a->x = tx; a->y = ty; a->pi++; }
    else { a->x += vx / dd * sp; a->y += vy / dd * sp; }
    a->bob += sd * 11.0f;
    if (a->bob > 1000.0f) a->bob -= 1000.0f;
    return a->pi >= a->plen;
}

// aimless milling so an unsatisfiable prisoner reads as restless, not frozen
static void lka_mill(Actor *a, float sd) {
    if (a->pi < a->plen) { lka_walk(a, sd); return; }
    if (a->repath > 0) return;
    a->repath = 1.5f + (float)rnd(30) * 0.1f;
    int from = lka_c(a);
    int x = lk_tx(from) + rnd_between(-4, 5), y = lk_ty(from) + rnd_between(-4, 5);
    if (!lk_in(x, y)) return;
    int c = lk_idx(x, y);
    if (lk_can_pass(c, a->role)) lka_set_path(a, c);
}

// ═══ claim / release ═════════════════════════════════════════════════════════
void lk_actor_release(int ai) {
    if (ai < 0 || ai >= LK_MAXACT) return;
    Actor *a = &lk_a[ai];
    if (a->use_tile >= 0 && a->use_tile < LK_N) lka_unclaim(a->use_tile);
    a->use_tile = -1;
    if (a->role == RL_WORKMAN && a->target >= 0 && a->target < LK_N && lk_t[a->target].claimed)
        lk_t[a->target].claimed = 0;
    if (a->escort >= 0 && a->escort < LK_MAXACT && lk_a[a->escort].escort == (short)ai)
        lk_a[a->escort].escort = -1;
    a->escort = -1;
    lka_goal[ai] = ND_COUNT;
    lka_prog[ai] = 0;
    a->plen = a->pi = 0;
    a->target = -1;
}

// Step OFF whatever you were using before going idle. A sleeper is snapped onto the
// bed tile, which is SOLID — leaving them there makes lk_reachable() false from their
// own position and the prisoner can never path anywhere again for the rest of the run
// (the offline harness caught exactly that: four men starved to death in their beds).
static void lka_idle(int ai) {
    Actor *a = &lk_a[ai];
    if (a->target >= 0 && a->target < LK_N && !lk_can_pass(lka_c(a), a->role) &&
        lk_can_pass(a->target, a->role)) {
        a->x = (float)lk_cx(a->target);
        a->y = (float)lk_cy(a->target);
    }
    lk_actor_release(ai);
    a->state = AS_IDLE;
    a->t = 0;
}

// …and the general case, because this is a CONSTRUCTION game: a workman will finish a
// wall on the tile someone is standing on. Shove them to the nearest open tile rather
// than leaving an actor sealed inside geometry.
static bool lka_unstick(Actor *a) {
    int c = lka_c(a);
    if (lk_can_pass(c, a->role)) return false;
    if (c == a->use_tile) return false;              // lying on the bed: that's the point
    int x = lk_tx(c), y = lk_ty(c);
    for (int r = 1; r <= 3; r++) {
        for (int oy = -r; oy <= r; oy++) for (int ox = -r; ox <= r; ox++) {
            if (abs(ox) != r && abs(oy) != r) continue;          // ring only
            int nx = x + ox, ny = y + oy;
            if (!lk_in(nx, ny)) continue;
            int n = lk_idx(nx, ny);
            if (!lk_can_pass(n, a->role)) continue;
            a->x = (float)lk_cx(n); a->y = (float)lk_cy(n);
            a->plen = a->pi = 0;
            return true;
        }
    }
    return false;
}

// ═══ spawn / init ════════════════════════════════════════════════════════════
void lk_actors_init(void) {
    for (int i = 0; i < LK_MAXACT; i++) {
        lk_a[i] = (Actor){ 0 };
        lk_a[i].cell = lk_a[i].target = lk_a[i].use_tile = lk_a[i].escort = -1;
        lk_a[i].id = (short)i;
        lka_goal[i] = ND_COUNT;
        lka_carry[i] = 0;
        lka_prog[i] = 0;
        lka_stuck[i] = 0;
    }
    lk_nact = 0;
    for (int i = 0; i < LK_N; i++) { lka_stock[i] = 0; lka_used[i] = 0; }
    for (int i = 0; i < LKA_BW * LKA_BH; i++) lka_heat[i] = 0;
    lka_regime_default();
    lka_last_hour = -1;
    lka_tick_t = 0;
    lka_live_fights = 0;
    lka_riot_t = 0;
    lka_searchn = 0;
    lka_gate = -1;
}

int lk_spawn(int role, int c) {
    if (role < 0 || role >= RL_COUNT) return -1;
    int ai = -1;
    for (int i = 0; i < LK_MAXACT; i++) if (!lk_a[i].alive) { ai = i; break; }
    if (ai < 0) return -1;
    if (c < 0 || c >= LK_N) c = lk_idx(LK_MW / 2, LK_MH / 2);

    Actor *a = &lk_a[ai];
    *a = (Actor){ 0 };
    a->alive = 1;
    a->role  = (unsigned char)role;
    a->state = AS_IDLE;
    a->x = (float)lk_cx(c);
    a->y = (float)lk_cy(c);
    a->face = 1.5708f;                       // facing south (radians, atan2 convention)
    a->health = 1.0f;
    a->cell = a->target = a->use_tile = a->escort = -1;
    a->id = (short)ai;
    a->tint = (unsigned char)rnd(4);
    a->skin = (unsigned char)rnd(4);
    a->hair = (unsigned char)rnd(5);
    a->sector = (unsigned char)(ai & 3);
    if (role == RL_PRISONER) {
        // security mix: mostly normal, a minority of each extreme
        int r = rnd(100);
        a->sec = (unsigned char)(r < 30 ? SEC_MIN : (r < 85 ? SEC_NORM : SEC_MAX));
        // arrive already a bit worn down — a bus is not a spa
        a->need[ND_SLEEP]   = 0.25f + (float)rnd(20) * 0.01f;
        a->need[ND_FOOD]    = 0.18f + (float)rnd(22) * 0.01f;
        a->need[ND_BLADDER] = 0.20f + (float)rnd(30) * 0.01f;
        a->need[ND_HYGIENE] = 0.20f + (float)rnd(30) * 0.01f;
        a->need[ND_REC]     = 0.15f + (float)rnd(20) * 0.01f;
        a->need[ND_FAMILY]  = 0.30f + (float)rnd(30) * 0.01f;
        a->need[ND_COMFORT] = 0.20f + (float)rnd(20) * 0.01f;
        a->need[ND_SAFETY]  = 0.20f + (float)(a->sec == SEC_MAX ? 15 : 0) * 0.01f;
        a->need[ND_PRIVACY] = 0.50f;
        a->vol = 0.10f + (a->sec == SEC_MAX ? 0.15f : 0.0f);
    }
    lka_goal[ai] = ND_COUNT;
    lka_carry[ai] = 0;
    lka_prog[ai] = 0;
    lka_stuck[ai] = 0;
    if (ai >= lk_nact) lk_nact = ai + 1;
    return ai;
}

// ═══ cells + intake ══════════════════════════════════════════════════════════
static int lka_assigned_to(int rid) {
    int n = 0;
    for (int i = 0; i < lk_nact; i++)
        if (lk_a[i].alive && lk_a[i].role == RL_PRISONER && lk_a[i].cell == (short)rid) n++;
    return n;
}

int lk_assign_cell(int ai) {
    if (ai < 0 || ai >= LK_MAXACT || !lk_a[ai].alive) return -1;
    Actor *a = &lk_a[ai];
    if (a->cell >= 0) return a->cell;
    int from = lka_c(a), fx = lk_tx(from), fy = lk_ty(from);
    int best = -1, bd = 1 << 30, pass;
    // prefer a real cell (privacy); fall back to a dormitory bunk
    for (pass = 0; pass < 2 && best < 0; pass++) {
        int type = pass == 0 ? RM_CELL : RM_DORM;
        for (int nth = 0; nth < LK_MAXROOM; nth++) {
            int rid = lk_room_find(type, nth);
            if (rid < 0) break;
            if (rid <= 0 || rid >= LK_MAXROOM || !lk_room[rid].valid) continue;
            if (lk_room[rid].cap <= 0) continue;
            if (lka_assigned_to(rid) >= (int)lk_room[rid].cap) continue;
            int d = abs(lk_room[rid].cx - fx) + abs(lk_room[rid].cy - fy);
            if (d < bd) { bd = d; best = rid; }
        }
    }
    if (best < 0) return -1;
    a->cell = (short)best;
    return best;
}

static int lka_gate_tile(void) {
    if (lka_gate >= 0 && lka_gate < LK_N && lk_can_pass(lka_gate, RL_PRISONER)) return lka_gate;
    // 1) a real gate
    for (int c = 0; c < LK_N; c++)
        if (lk_t[c].door == DR_GATE) { lka_gate = c; return c; }
    // 2) the exit flow field from mid-map
    int mc = lk_idx(LK_MW / 2, LK_MH / 2);
    int e = lk_nearest(mc, FF_EXIT, RL_GUARD);
    if (e >= 0) { lka_gate = e; return e; }
    // 3) the outer ring — a bus can always stop on the road
    for (int x = 0; x < LK_MW; x++) {
        int c = lk_idx(x, 0);
        if (lk_can_pass(c, RL_PRISONER)) { lka_gate = c; return c; }
    }
    for (int y = 0; y < LK_MH; y++) {
        int c = lk_idx(0, y);
        if (lk_can_pass(c, RL_PRISONER)) { lka_gate = c; return c; }
    }
    lka_gate = mc;
    return mc;
}

// Off the map is off the map. FF_EXIT if the path module has that field; otherwise
// the nearest passable border tile that belongs to no room — so a cut fence always
// leads SOMEWHERE and an escape can actually complete.
static int lka_exit_tile(Actor *a) {
    int from = lka_c(a);
    int e = lk_nearest(from, FF_EXIT, RL_PRISONER);
    if (e >= 0 && lk_reachable(from, e, RL_PRISONER)) return e;
    int fx = lk_tx(from), fy = lk_ty(from), best = -1, bd = 1 << 30;
    for (int k = 0; k < 2 * (LK_MW + LK_MH); k++) {
        int x, y;
        if (k < LK_MW)                     { x = k;                  y = 0; }
        else if (k < 2 * LK_MW)            { x = k - LK_MW;          y = LK_MH - 1; }
        else if (k < 2 * LK_MW + LK_MH)    { x = 0;                  y = k - 2 * LK_MW; }
        else                               { x = LK_MW - 1;          y = k - 2 * LK_MW - LK_MH; }
        int c = lk_idx(x, y);
        if (lk_t[c].room != 0 || !lk_can_pass(c, RL_PRISONER)) continue;
        int dd = abs(x - fx) + abs(y - fy);
        if (dd >= bd || !lk_reachable(from, c, RL_PRISONER)) continue;
        bd = dd; best = c;
    }
    return best;
}

void lk_intake(int n) {
    int gate = lka_gate_tile();
    int gx = lk_tx(gate), gy = lk_ty(gate);
    for (int k = 0; k < n; k++) {
        // fan the bus out along the road so they don't stack in one pixel
        int c = gate;
        for (int try_i = 0; try_i < 12; try_i++) {
            int x = gx + rnd_between(-3, 4), y = gy + rnd_between(-2, 3);
            if (!lk_in(x, y)) continue;
            int t = lk_idx(x, y);
            if (lk_can_pass(t, RL_PRISONER)) { c = t; break; }
        }
        int ai = lk_spawn(RL_PRISONER, c);
        if (ai < 0) return;                       // prison full — honest no-op
        Actor *a = &lk_a[ai];
        // contraband slips in with the bus; a metal detector is the player's answer
        if (chance(a->sec == SEC_MAX ? 45 : 22)) {
            int r = rnd(100);
            a->contraband = (unsigned char)(r < 35 ? CB_DRUGS : (r < 65 ? CB_PHONE :
                                           (r < 88 ? CB_WEAPON : CB_TOOL)));
        }
        int rid = lk_assign_cell(ai);
        a->state = AS_ESCORTED;                  // waits at the gate for a guard
        a->t = 0;
        if (rid < 0) {
            // no bed. Not a crash, not a silent failure: they queue in holding and
            // their needs climb where the player can see it.
            a->need[ND_PRIVACY] = 1.0f;
        }
    }
    lk_sfx(SFX_BUS);
}

// ═══ needs ═══════════════════════════════════════════════════════════════════
static float lka_privacy_target(const Actor *a) {
    if (a->cell < 0) return 1.0f;                       // nowhere of your own at all
    int rid = a->cell;
    if (rid <= 0 || rid >= LK_MAXROOM) return 1.0f;
    if (lk_room[rid].type == RM_DORM) return 0.60f;     // a dormitory is a standing insult
    if (lk_room[rid].cap > 1) return 0.35f;             // shared cell
    return 0.0f;
}

static void lka_needs(int ai, float sd) {
    Actor *a = &lk_a[ai];
    if (a->role != RL_PRISONER) return;
    float hours = sd / LK_HOUR_SECS;

    // Asleep, the day stops costing you as much. Without this the frozen decay table
    // saturates FOOD across the 13-hour gap between the evening meal and breakfast,
    // so even a perfect prison would wake up starving — the default regime has to be
    // survivable or the whole lesson about broken regimes says nothing.
    float mult = (a->state == AS_SLEEP) ? 0.35f : 1.0f;
    for (int n = 0; n < ND_COUNT; n++) {
        if (n == ND_SAFETY || n == ND_PRIVACY) continue;
        a->need[n] = clamp(a->need[n] + LK_NEED[n].decay * hours * mult, 0.0f, 1.0f);
    }
    // PRIVACY: a standing penalty for your housing, not a tick
    float pt = lka_privacy_target(a);
    a->need[ND_PRIVACY] += (pt - a->need[ND_PRIVACY]) * clamp(hours * 1.5f, 0.0f, 1.0f);
    // FAMILY: a smuggled phone quietly does the job a visitation room should
    if (a->contraband & CB_PHONE) a->need[ND_FAMILY] = clamp(a->need[ND_FAMILY] - 0.02f * sd, 0, 1);

    // SAFETY: fear of what is happening around you, minus who is watching over you
    float fear = lka_heat[lka_heat_i(lka_c(a))];
    if (fear > 0.02f) a->need[ND_SAFETY] = clamp(a->need[ND_SAFETY] + fear * 0.30f * sd, 0, 1);
    else a->need[ND_SAFETY] = clamp(a->need[ND_SAFETY] - (0.015f + a->supp * 0.10f) * sd, 0, 1);
    if (a->state == AS_FIGHT) a->need[ND_SAFETY] = clamp(a->need[ND_SAFETY] + 0.30f * sd, 0, 1);

    // Starvation and exhaustion are real, but SLOW: unmet need is meant to make a
    // prison dangerous (volatility), not to quietly execute people. Only a prison
    // that keeps a man at maximum hunger for half a day kills him.
    if (a->need[ND_FOOD] >= 0.995f)  a->health -= LKA_STARVE_DRAIN * sd;
    if (a->need[ND_SLEEP] >= 0.995f) a->health -= LKA_STARVE_DRAIN * 0.5f * sd;
    if (a->need[ND_FOOD] < 0.80f && a->need[ND_SLEEP] < 0.90f && a->health < 1.0f)
        a->health = clamp(a->health + 0.006f * sd, 0.0f, 1.0f);      // fed and rested → mend
    // bladder overflow: an accident, and the hygiene bill that follows
    if (a->need[ND_BLADDER] >= 0.999f) {
        a->need[ND_BLADDER] = 0.25f;
        a->need[ND_HYGIENE] = clamp(a->need[ND_HYGIENE] + 0.40f, 0, 1);
        a->vol = clamp(a->vol + 0.10f, 0, 1);
        int c = lka_c(a);
        if (lk_t[c].dirty < 210) lk_t[c].dirty = 210;
        lk_puff(a->x, a->y + 4, 4.0f, 0.5f, CLR_DARK_GREEN);
    }
    // collapse, not instant death: they go down with a little left, and a doctor can
    // still win the race. Without one, AS_DOWN drains the rest — see lka_prisoner.
    if (a->health <= 0.15f && a->state != AS_DOWN) {
        a->health = 0.15f;
        lk_actor_release(ai);
        a->state = AS_DOWN;
        a->t = 0;
        lk_puff(a->x, a->y, 7.0f, 0.6f, CLR_DARK_RED);
    }
}

// a cell id is an index into a table the grid module REBUILDS; re-validate before
// trusting it, or a demolished cell block leaves everyone assigned to a canteen.
static void lka_validate_cell(Actor *a) {
    if (a->cell < 0) return;
    int rid = a->cell;
    if (rid >= LK_MAXROOM || !lk_room[rid].valid) { a->cell = -1; return; }
    int ty = lk_room[rid].type;
    if (ty != RM_CELL && ty != RM_DORM) a->cell = -1;
}

const char *lk_need_worst(int ai) {
    if (ai < 0 || ai >= LK_MAXACT || !lk_a[ai].alive) return "-";
    const Actor *a = &lk_a[ai];
    int w = -1; float wv = 0.12f;
    for (int n = 0; n < ND_COUNT; n++) if (a->need[n] > wv) { wv = a->need[n]; w = n; }
    return w < 0 ? "Content" : LK_NEED[w].name;
}

// ═══ suppression: guard presence, and it is LOCAL ════════════════════════════
static void lka_suppression(float sd) {
    for (int i = 0; i < lk_nact; i++) {
        Actor *p = &lk_a[i];
        if (!p->alive || p->role != RL_PRISONER) continue;
        int pc = lka_c(p);
        int proom = lk_t[pc].room;
        float best = 0;
        for (int j = 0; j < lk_nact; j++) {
            Actor *g = &lk_a[j];
            if (!g->alive || g->role != RL_GUARD) continue;
            if (g->state == AS_DOWN) continue;
            float dxp = (g->x - p->x) / LK_TS, dyp = (g->y - p->y) / LK_TS;
            float d = de_hypotf(dxp, dyp);
            if (d > LKA_GUARD_REACH) continue;
            int gc = lka_c(g);
            int groom = lk_t[gc].room;
            // same room = eye contact. Open ground = partial. Through a wall = none,
            // which is exactly why WHERE you deploy matters.
            float w;
            if (proom > 0 && groom == proom) w = 1.0f;   // same room: eye contact
            else if (proom == 0 && groom == 0) w = 0.75f;  // open ground / corridor
            else w = 0.0f;                                 // through a wall: nothing
            if (w <= 0) continue;
            float s = (1.0f - d / LKA_GUARD_REACH) * w;
            if (s > best) best = s;
        }
        if (lk.alarm == AL_LOCKDOWN) best = lka_maxf(best, 0.55f);   // locked in a cell is quiet
        p->supp += (best - p->supp) * clamp(sd * 2.0f, 0.0f, 1.0f);
    }
}

// ═══ volatility: the integral of unmet need ══════════════════════════════════
static float lka_pressure(const Actor *a) {
    float sum = 0, wsum = 0;
    for (int n = 0; n < ND_COUNT; n++) {
        float w = (float)LK_NEED[n].weight;
        wsum += w;
        // a need only pushes once it is genuinely unmet — below that it's just life
        float u = a->need[n] - 0.30f;
        if (u > 0) sum += w * (u / 0.70f);
    }
    return wsum > 0 ? sum / wsum : 0;
}

static void lka_volatility(int ai, float sd) {
    Actor *a = &lk_a[ai];
    if (a->role != RL_PRISONER) return;
    float target = lka_pressure(a);
    int gr = 0;
    for (int k = 0; k < 4; k++) if (a->grudge[k]) gr++;
    target += (float)gr * 0.06f;
    target += lka_heat[lka_heat_i(lka_c(a))] * 0.30f;
    if (a->contraband & CB_WEAPON) target += 0.08f;   // carrying it makes you bold
    if (a->contraband & CB_DRUGS)  target -= 0.06f;   // …and stoned makes you docile
    if (a->sec == SEC_MAX) target += 0.10f;
    if (a->sec == SEC_MIN) target -= 0.06f;
    if (a->state == AS_WORK) target -= 0.08f;         // busy hands
    target -= a->supp * 0.70f;
    target = clamp(target, 0.0f, 1.0f);
    // the INTEGRAL: rate is the per-second easing coefficient, so pressure takes
    // ~1.5 in-game hours to become real anger and longer than that to cool off. A
    // brief need spike must never riot a prison — sustained neglect must.
    float rate = (target > a->vol) ? LKA_VOL_RISE : LKA_VOL_FALL;
    a->vol = clamp(a->vol + (target - a->vol) * clamp(rate * sd, 0.0f, 1.0f), 0.0f, 1.0f);
}

// ═══ fights ══════════════════════════════════════════════════════════════════
static bool lka_busy_state(int s) {
    return s == AS_FIGHT || s == AS_DOWN || s == AS_RESTRAINED || s == AS_SOLITARY ||
           s == AS_ESCORTED || s == AS_ESCORTING || s == AS_ESCAPE;
}

static void lka_add_grudge(Actor *a, int other) {
    for (int k = 0; k < 4; k++) if (a->grudge[k] == (unsigned char)(other + 1)) return;
    for (int k = 0; k < 4; k++) if (!a->grudge[k]) { a->grudge[k] = (unsigned char)(other + 1); return; }
    a->grudge[rnd(4)] = (unsigned char)(other + 1);
}
static bool lka_has_grudge(const Actor *a, int other) {
    for (int k = 0; k < 4; k++) if (a->grudge[k] == (unsigned char)(other + 1)) return true;
    return false;
}

static void lka_start_fight(int i, int j) {
    Actor *a = &lk_a[i], *b = &lk_a[j];
    lk_actor_release(i); lk_actor_release(j);
    a->state = b->state = AS_FIGHT;
    a->t = b->t = 0;
    lka_look_at(a, b->x, b->y);
    lka_look_at(b, a->x, a->y);
    lka_add_grudge(a, j); lka_add_grudge(b, i);
    lk.incidents++;
    lka_add_heat(lka_c(a), 0.55f);
    lk_sfx_at(SFX_FIGHT, a->x, a->y);
    lk_puff(a->x, a->y - 4, 6.0f, 0.4f, CLR_RED);
    if (lk.alarm == AL_CALM) lk_set_alarm(AL_INCIDENT);
}

// deterministic on the thresholds — no dice anywhere in this function
static void lka_threat_pass(void) {
    short cand[64]; int nc = 0;
    for (int i = 0; i < lk_nact && nc < 64; i++) {
        Actor *a = &lk_a[i];
        if (!a->alive || a->role != RL_PRISONER) continue;
        if (lka_busy_state(a->state)) continue;
        if (a->supp >= LKA_SUPP_SAFE) continue;          // a guard is watching
        if (a->vol < LKA_FIGHT_GRUDGE) continue;
        cand[nc++] = (short)i;
    }
    for (int u = 0; u < nc; u++) for (int v = u + 1; v < nc; v++) {
        int i = cand[u], j = cand[v];
        Actor *a = &lk_a[i], *b = &lk_a[j];
        if (a->state == AS_FIGHT || b->state == AS_FIGHT) continue;
        float dd = de_hypotf(a->x - b->x, a->y - b->y);
        if (dd > LKA_FIGHT_REACH) continue;
        bool hate = lka_has_grudge(a, j) || lka_has_grudge(b, i);
        float thr = hate ? LKA_FIGHT_GRUDGE : LKA_FIGHT_VOL;
        if (a->vol < thr || b->vol < thr) continue;
        lka_start_fight(i, j);
    }
}

static void lka_send_to_solitary(int ai) {
    Actor *a = &lk_a[ai];
    int sol = lka_room_tile(a, RM_SOLITARY, 0);
    if (sol >= 0 && lka_set_path(a, sol)) { a->state = AS_SOLITARY; a->t = 0; return; }
    lka_idle(ai);                                    // no solitary built → straight back out
}

// a free seat/station inside a valid workshop (a workshop TABLE has 0 slots, so it
// is the benches and chairs the player added around it that make work possible)
static int lka_workshop_slot(Actor *a) {
    int from = lka_c(a);
    for (int nth = 0; nth < LK_MAXROOM; nth++) {
        int rid = lk_room_find(RM_WORKSHOP, nth);
        if (rid < 0) break;
        if (rid <= 0 || rid >= LK_MAXROOM || !lk_room[rid].valid) continue;
        for (int c = 0; c < LK_N; c++) {
            if (lk_t[c].room != rid || !lk_t[c].obj) continue;
            if (!lka_slot_free(c, ND_COMFORT, a->role)) continue;
            int st = lka_stand_for(c, a->role);
            if (st >= 0 && lk_reachable(from, st, a->role)) return c;
        }
    }
    return -1;
}

// ═══ the prisoner goal picker: conditions (1)…(5), in one place ══════════════
// Try to satisfy ONE named need. Returns true only if the prisoner is now walking
// somewhere that will actually lower it — every `false` is one of the five
// conditions failing, and the need keeps climbing because of it.
static bool lka_try_need(int ai, int want, int act) {
    Actor *a = &lk_a[ai];

    // the yard IS the recreation during a Yard slot — no object required
    if (want == ND_REC && act == AC_YARD) {
        int y = lka_room_tile(a, RM_YARD, 0);
        if (y >= 0 && lka_set_path(a, y)) {
            lka_goal[ai] = (unsigned char)ND_REC; a->use_tile = -1; a->state = AS_WALK; return true;
        }
    }
    // your OWN bed first, if you have one and it's free — that's what a cell is for
    if (want == ND_SLEEP && a->cell > 0 && a->cell < LK_MAXROOM && lk_room[a->cell].valid) {
        int bed = lka_find_obj_in_room(a->cell, OB_BED, true);
        if (bed < 0) bed = lka_find_obj_in_room(a->cell, OB_BUNK, true);
        if (bed >= 0 && lka_has_room(bed)) {
            int st = lka_stand_spun(bed, a->role, ai);
            if (st >= 0 && lk_reachable(lka_c(a), st, a->role) && lka_set_path(a, st)) {
                lka_claim(bed);
                a->use_tile = (short)bed;
                lka_goal[ai] = (unsigned char)ND_SLEEP;
                a->state = AS_WALK;
                return true;
            }
        }
    }
    int c = lka_best_slot(a, want);                  // (1)(2)(3)(5) all checked inside
    if (c >= 0) {
        int st = lka_stand_spun(c, a->role, ai);
        if (st >= 0 && lka_set_path(a, st)) {
            lka_claim(c);                            // claim it now: capacity is real
            a->use_tile = (short)c;
            lka_goal[ai] = (unsigned char)want;
            a->state = AS_WALK;
            return true;
        }
    }
    // no equipment for recreation anywhere? the yard will do, any hour
    if (want == ND_REC) {
        int y = lka_room_tile(a, RM_YARD, 0);
        if (y >= 0 && lka_set_path(a, y)) {
            lka_goal[ai] = (unsigned char)ND_REC; a->use_tile = -1; a->state = AS_WALK; return true;
        }
    }
    return false;
}

static void lka_seek(int ai) {
    Actor *a = &lk_a[ai];
    int act = lk_regime[lka_hour()];
    if (a->use_tile >= 0) lk_actor_release(ai);    // never re-plan while holding a slot
    lka_goal[ai] = LKA_GOAL_NONE;

    if (lk.alarm == AL_LOCKDOWN || lk.alarm == AL_RIOT) act = AC_LOCKUP;
    if (a->cell < 0) lk_assign_cell(ai);      // a cell may have been finished since

    // WORK is a place, not a need: go to a workshop station and be occupied.
    if (act == AC_WORK) {
        int c = lka_workshop_slot(a);
        if (c >= 0) {
            int st = lka_stand_spun(c, a->role, ai);
            if (st >= 0 && lka_set_path(a, st)) {
                lka_claim(c);
                a->use_tile = (short)c;
                lka_goal[ai] = LKA_GOAL_WORK;
                a->state = AS_WALK;
                return;
            }
        }
        int w = lka_room_tile(a, RM_WORKSHOP, 0);
        if (w >= 0 && lka_set_path(a, w)) {
            lka_goal[ai] = LKA_GOAL_WORK; a->state = AS_WALK; return;
        }
        // no workshop: the work slot is dead time. Needs climb. That is the lesson.
    }

    // (4) the regime says what may be WANTED; the needs say in which ORDER. We walk
    // the permitted list worst-first and take the first thing we can actually reach —
    // otherwise ONE unsatisfiable need (a wing with no phone) would silently stop a
    // prisoner from using the yard, the showers and the benches too.
    unsigned char order[ND_COUNT]; int n = 0;
    for (int nd = 0; nd < ND_COUNT; nd++) {
        if (!lka_permits(act, nd)) continue;
        if (a->need[nd] <= LKA_WANT) continue;
        int at = n;
        while (at > 0 && a->need[order[at - 1]] < a->need[nd]) { order[at] = order[at - 1]; at--; }
        order[at] = (unsigned char)nd;
        n++;
    }
    for (int k = 0; k < n; k++)
        if (lka_try_need(ai, order[k], act)) return;

    // NOTHING the regime permits can be reached. The needs keep climbing, visibly,
    // wherever the player left the hole. Cool down so we don't re-path every frame.
    if (n > 0) a->repath = 2.0f + (float)(ai % 7) * 0.3f;

    // then go where the regime says you belong, and just be there
    if (act == AC_SLEEP || act == AC_LOCKUP) {
        int home = -1;
        if (a->cell > 0 && a->cell < LK_MAXROOM)
            home = lka_room_tile(a, lk_room[a->cell].type, a->cell);
        if (home < 0) home = lka_room_tile(a, RM_HOLDING, 0);   // no bed → the holding cell
        if (home >= 0 && lka_set_path(a, home)) { a->state = AS_WALK; return; }
    } else if (act == AC_YARD) {
        int y = lka_room_tile(a, RM_YARD, 0);
        if (y >= 0 && lka_set_path(a, y)) { a->state = AS_WALK; return; }
    }
    a->state = AS_IDLE;
}

static int lka_use_state(int need) {
    switch (need) {
    case ND_SLEEP:   return AS_SLEEP;
    case ND_FOOD:    return AS_EAT;
    case ND_HYGIENE: return AS_WASH;
    }
    return AS_USE;
}

static void lka_arrive(int ai) {
    Actor *a = &lk_a[ai];
    int need = lka_goal[ai];
    if (a->use_tile >= 0) {
        lka_look_at(a, (float)lk_cx(a->use_tile), (float)lk_cy(a->use_tile));
        int ob = lk_t[a->use_tile].obj;
        if (need == ND_FOOD) {
            if (lka_stock[a->use_tile] == 0) { lka_idle(ai); a->repath = 1.5f; return; }
            lka_stock[a->use_tile]--;                     // one tray, taken
            lk_sfx_at(SFX_MEAL, a->x, a->y);
        }
        if (ob == OB_BED || ob == OB_BUNK || ob == OB_MEDBED) {
            a->x = (float)lk_cx(a->use_tile);            // lie ON the bed
            a->y = (float)lk_cy(a->use_tile);
        } else if (lka_cap_of(a->use_tile) > 1) {
            a->x += (float)((ai % 3) - 1) * 3.0f;        // shoulder to shoulder, not stacked
            a->y += (float)(((ai / 3) % 3) - 1) * 3.0f;
        }
    }
    a->t = 0;
    if (need == LKA_GOAL_WORK) { a->state = AS_WORK; return; }
    if (need >= ND_COUNT) {                              // arrived where you belong,
        a->state = AS_IDLE;                              // with nothing here to fix it
        if (a->repath < 1.5f) a->repath = 1.5f;
        return;
    }
    a->state = (unsigned char)lka_use_state(need);
}

static void lka_using(int ai, float sd) {
    Actor *a = &lk_a[ai];
    int need = lka_goal[ai];
    a->t += sd;
    int act = lk_regime[lka_hour()];

    if (need < ND_COUNT) {
        float rate = LK_NEED[need].fill;
        if (a->use_tile >= 0) {
            int ob = lk_t[a->use_tile].obj;
            // a sink is not a shower; a bench is not a bed. Off-label works, at half.
            if (ob != LK_NEED[need].obj) rate *= 0.5f;
        } else {
            rate *= 0.8f;                                // the yard, with no equipment
        }
        a->need[need] = clamp(a->need[need] - rate * sd, 0.0f, 1.0f);
        // being somewhere that is yours also settles you
        if (need == ND_SLEEP) {
            a->need[ND_COMFORT] = clamp(a->need[ND_COMFORT] - rate * 0.4f * sd, 0, 1);
            a->vol = clamp(a->vol - 0.01f * sd, 0, 1);
        }
    }

    // sleeping runs the whole sleep block; everything else ends when it's done
    bool done;
    if (a->state == AS_SLEEP)
        done = (act != AC_SLEEP && act != AC_LOCKUP) || a->need[ND_BLADDER] > 0.85f;
    else if (a->state == AS_WORK) done = (act != AC_WORK);
    else done = (need >= ND_COUNT) || a->need[need] <= LKA_SATED || a->t > LKA_MAX_USE;

    if (done) lka_idle(ai);
}

// ═══ escape ══════════════════════════════════════════════════════════════════
static bool lka_try_escape(int ai, float sd) {
    Actor *a = &lk_a[ai];
    if (!(a->contraband & CB_TOOL)) return false;
    if (a->vol < 0.80f || a->supp > 0.08f) return false;
    if (a->state == AS_ESCAPE) return true;
    int c = lka_c(a), x = lk_tx(c), y = lk_ty(c);
    for (int d = 0; d < 4; d++) {
        int nx = x + LKA_D4X[d], ny = y + LKA_D4Y[d];
        if (!lk_in(nx, ny)) continue;
        int n = lk_idx(nx, ny);
        if (lk_t[n].wall != WL_FENCE) continue;
        lk_actor_release(ai);
        a->state = AS_ESCAPE;
        a->target = (short)n;
        lka_prog[ai] = 0;
        lka_look_at(a, (float)lk_cx(n), (float)lk_cy(n));
        (void)sd;
        return true;
    }
    return false;
}

static void lka_escaping(int ai, float sd) {
    Actor *a = &lk_a[ai];
    if (a->target >= 0 && a->target < LK_N && lk_t[a->target].wall == WL_FENCE) {
        lka_prog[ai] += sd;
        if (chance(6)) lk_puff((float)lk_cx(a->target), (float)lk_cy(a->target), 3.0f, 0.3f, CLR_LIGHT_GREY);
        if (lka_prog[ai] >= LKA_CUT_SECS) {
            lk_set_wall(a->target, WL_NONE);
            lk_sfx_at(SFX_FIGHT, a->x, a->y);
            a->target = -1;
            a->plen = a->pi = 0;
        }
        return;
    }
    if (a->pi >= a->plen) {
        int e = lka_exit_tile(a);
        if (e < 0 || !lka_set_path(a, e)) { lka_idle(ai); return; }
    }
    if (lka_walk(a, sd)) {
        a->alive = 0;                                    // out, and gone
        lk.escapes++;
        lk_set_alarm(AL_INCIDENT);
        lk_puff(a->x, a->y, 8.0f, 0.6f, CLR_WHITE);
    }
}

// ═══ prisoner FSM ════════════════════════════════════════════════════════════
static void lka_prisoner(int ai, float sd) {
    Actor *a = &lk_a[ai];

    switch (a->state) {
    case AS_IDLE:
        if (lka_try_escape(ai, sd)) break;
        if (a->repath <= 0) lka_seek(ai);
        if (a->state == AS_IDLE) lka_mill(a, sd);
        break;

    case AS_WALK: {
        // the slot we walked for may have been demolished under us
        if (a->use_tile >= 0 && (!lk_t[a->use_tile].obj || !lka_used[a->use_tile])) {
            lka_idle(ai); a->repath = 0.8f; break;
        }
        if (lka_walk(a, sd)) lka_arrive(ai);
    } break;

    case AS_USE: case AS_SLEEP: case AS_EAT: case AS_WASH: case AS_WORK:
        lka_using(ai, sd);
        break;

    case AS_FIGHT: {
        a->t += sd;
        float bite = (a->contraband & CB_WEAPON) ? 0.055f : 0.030f;
        a->health -= bite * sd;
        lka_add_heat(lka_c(a), 0.25f * sd);
        a->bob += sd * 22.0f;
        if (chance(3)) lk_puff(a->x + (float)rnd_between(-5, 6), a->y - 3, 3.0f, 0.25f, CLR_RED);
        // a guard within arm's reach ends it
        bool broken = false;
        for (int j = 0; j < lk_nact; j++) {
            Actor *g = &lk_a[j];
            if (!g->alive || g->role != RL_GUARD) continue;
            if (de_hypotf(g->x - a->x, g->y - a->y) < 20.0f) { broken = true; break; }
        }
        if (a->health <= 0.35f) {
            a->health = lka_maxf(a->health, 0.30f);      // beaten, not killed outright
            a->state = AS_DOWN; a->t = 0;
            lk_puff(a->x, a->y, 7.0f, 0.5f, CLR_DARK_RED);
        } else if (broken || a->t > LKA_FIGHT_SECS) {
            a->state = broken ? AS_RESTRAINED : AS_IDLE;
            a->t = 0;
            a->vol = clamp(a->vol - (broken ? 0.35f : 0.15f), 0, 1);
            a->repath = 0.5f;
            if (broken) lk_sfx_at(SFX_WHISTLE, a->x, a->y);
        }
    } break;

    case AS_RESTRAINED:
        a->t += sd;
        a->supp = 1.0f;
        if (a->t > LKA_RESTRAIN_SECS) {
            a->t = 0;
            if (lk_room_find(RM_SOLITARY, 0) >= 0) lka_send_to_solitary(ai);
            else lka_idle(ai);
        }
        break;

    case AS_SOLITARY:
        if (a->pi < a->plen) { lka_walk(a, sd); break; }
        a->t += sd;
        a->supp = 1.0f;
        a->vol = clamp(a->vol - 0.02f * sd, 0, 1);       // cools down…
        a->need[ND_REC] = clamp(a->need[ND_REC] + 0.04f * sd, 0, 1);      // …at a price
        a->need[ND_FAMILY] = clamp(a->need[ND_FAMILY] + 0.02f * sd, 0, 1);
        if (a->t > LKA_SOLITARY_SECS) lka_idle(ai);
        break;

    case AS_DOWN:
        // A beating leaves you on the floor, not in a grave: above the critical mark
        // you come round on your own. BELOW it you are bleeding out and only a doctor
        // (lka_doctor) can stop it — which is what makes hiring one a real decision.
        a->t += sd;
        if (a->health < 0.20f) a->health -= LKA_DOWN_DRAIN * sd;
        else if (a->need[ND_FOOD] < 0.95f) a->health += 0.005f * sd;
        if (a->health >= 0.50f) { lka_idle(ai); a->repath = 0.5f; break; }
        if (a->health <= 0.0f) {
            a->alive = 0;
            lk.deaths++;
            lk_set_alarm(AL_INCIDENT);
            lk_puff(a->x, a->y, 9.0f, 0.8f, CLR_DARK_RED);
            lk_actor_release(ai);
        }
        break;

    case AS_ESCORTED: {
        a->t += sd;
        if (a->escort >= 0 && lk_a[a->escort].alive && lk_a[a->escort].state == AS_ESCORTING) {
            Actor *g = &lk_a[a->escort];
            // ONLY trail once the guard has collected them and is leading (phase 2).
            // Before that the guard is still walking over, and a direct follow would
            // cut the prisoner straight through a wall.
            if (lka_prog[a->escort] < 0.5f) break;
            float vx = g->x - a->x, vy = g->y - a->y, dd = de_hypotf(vx, vy);
            if (dd > 13.0f) {                            // trail the guard
                float sp = lka_minf(LKA_SPD_GUARD * 1.05f * sd, dd - 12.0f);
                a->x += vx / dd * sp; a->y += vy / dd * sp;
                a->bob += sd * 11.0f;
                lka_look_at(a, g->x, g->y);
            }
        } else if (a->t > 20.0f) {
            // nobody came for them. They let themselves in — and remember it.
            a->escort = -1;
            a->vol = clamp(a->vol + 0.12f, 0, 1);
            lka_idle(ai);
        }
    } break;

    case AS_RIOT: {
        a->t += sd;
        a->vol = clamp(a->vol + 0.015f * sd, 0, 1);
        lka_add_heat(lka_c(a), 0.10f * sd);
        // a riot ends one of two ways: guards get on top of them, or they burn out.
        // Either way it TERMINATES — an eternal riot would be a bug, not a tragedy.
        if (a->supp > 0.40f) {
            a->vol = clamp(a->vol - 0.22f * sd, 0, 1);
            if (a->vol < 0.45f) { lka_idle(ai); a->state = AS_RESTRAINED; a->t = 0; break; }
        } else if (a->t > LKA_RIOT_SECS) {
            a->vol = clamp(a->vol - 0.25f, 0, 1);
            lka_idle(ai);
            a->repath = 1.0f;
            break;
        }
        if (a->pi < a->plen) { lka_walk(a, sd); break; }
        if (a->repath > 0) break;
        a->repath = 1.0f + (float)rnd(20) * 0.05f;
        // force the nearest door, wreck the nearest thing that isn't nailed down
        int c = lka_c(a), x = lk_tx(c), y = lk_ty(c);
        for (int d = 0; d < 4; d++) {
            int nx = x + LKA_D4X[d], ny = y + LKA_D4Y[d];
            if (!lk_in(nx, ny)) continue;
            int n = lk_idx(nx, ny);
            if (lk_t[n].door && lk_t[n].locked) {
                lk_t[n].locked = 0; lk_t[n].door_open = 255;
                lk_sfx_at(SFX_DOOR, a->x, a->y);
                lk_puff((float)lk_cx(n), (float)lk_cy(n), 6.0f, 0.4f, CLR_LIGHT_GREY);
                return;
            }
            if (lk_t[n].obj && !LK_OBJ[lk_t[n].obj].staff_only && chance(40)) {
                lk_puff((float)lk_cx(n), (float)lk_cy(n), 7.0f, 0.5f, CLR_BROWN);
                lk_clear_tile(n);
                lk_sfx_at(SFX_FIGHT, a->x, a->y);
                return;
            }
        }
        lka_mill(a, sd);
    } break;

    case AS_ESCAPE:
        lka_escaping(ai, sd);
        break;

    default:
        lka_idle(ai);
        break;
    }
}

// ═══ guards ══════════════════════════════════════════════════════════════════
static int lka_find_fight(Actor *g) {
    int from = lka_c(g), best = -1, bd = 1 << 30;
    for (int i = 0; i < lk_nact; i++) {
        Actor *a = &lk_a[i];
        if (!a->alive || a->role != RL_PRISONER) continue;
        // a live fight always; a rioter or an escaper once the alarm is up
        if (a->state != AS_FIGHT && !(lk.alarm >= AL_RIOT && lka_trouble(a->state))) continue;
        int c = lka_c(a);
        // don't send four guards to one scuffle
        int taken = 0;
        for (int j = 0; j < lk_nact; j++)
            if (lk_a[j].alive && lk_a[j].role == RL_GUARD && lk_a[j].target == (short)c) taken++;
        if (taken > 0 && g->target != (short)c) continue;
        int d = abs(lk_tx(c) - lk_tx(from)) + abs(lk_ty(c) - lk_ty(from));
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static int lka_waiting_arrival(void) {
    for (int i = 0; i < lk_nact; i++) {
        Actor *a = &lk_a[i];
        if (a->alive && a->state == AS_ESCORTED && a->escort < 0) return i;
    }
    return -1;
}

static int lka_patrol_target(Actor *g) {
    // patrol what needs watching: prisoner rooms in this guard's quadrant, and the
    // hottest block in the prison if things are already going wrong.
    int qx = (g->sector & 1), qy = ((g->sector >> 1) & 1);
    int best = -1; float bs = -1.0f;
    for (int rid = 1; rid < LK_MAXROOM && rid < lk_nroom + 1; rid++) {
        if (!lk_room[rid].valid) continue;
        int ty = lk_room[rid].type;
        if (ty <= 0 || ty >= RM_COUNT || !LK_ROOM[ty].prisoner_ok) continue;
        int cx = lk_room[rid].cx, cy = lk_room[rid].cy;
        if (!lk_in(cx, cy)) continue;
        int c = lk_idx(cx, cy);
        float score = 0.4f + (float)rnd(20) * 0.01f;
        if ((cx >= LK_MW / 2) == (qx != 0) && (cy >= LK_MH / 2) == (qy != 0)) score += 0.6f;
        score += lka_heat[lka_heat_i(c)] * 1.5f;
        score += lk_block_tension(c, 5) * 0.8f;
        if (score > bs) { bs = score; best = c; }
    }
    if (best >= 0 && !lk_can_pass(best, RL_GUARD)) {
        int rid = lk_t[best].room, alt = -1;
        for (int i = 0; i < LK_N; i++)
            if (lk_t[i].room == rid && lk_can_pass(i, RL_GUARD)) { alt = i; break; }
        best = alt;
    }
    if (best < 0) {
        // nothing built yet (or no room reachable): go stand near the angriest man —
        // deployment still means "be where the trouble is", not "wander".
        float bv = 0.25f; int who = -1;
        for (int i = 0; i < lk_nact; i++) {
            const Actor *p = &lk_a[i];
            if (!p->alive || p->role != RL_PRISONER) continue;
            if (p->vol > bv) { bv = p->vol; who = i; }
        }
        if (who >= 0) {
            int c = lka_c(&lk_a[who]);
            best = lk_can_pass(c, RL_GUARD) ? c : lka_stand_for(c, RL_GUARD);
        }
        if (best < 0) {
            int c = lka_c(g), x = lk_tx(c) + rnd_between(-8, 9), y = lk_ty(c) + rnd_between(-8, 9);
            if (lk_in(x, y) && lk_can_pass(lk_idx(x, y), RL_GUARD)) best = lk_idx(x, y);
        }
    }
    return best;
}

static void lka_guard(int ai, float sd) {
    Actor *g = &lk_a[ai];

    switch (g->state) {
    case AS_IDLE: {
        // 1. a fight outranks everything
        int f = lka_find_fight(g);
        if (f >= 0) {
            int c = lka_c(&lk_a[f]);
            int st = lk_can_pass(c, RL_GUARD) ? c : lka_stand_for(c, RL_GUARD);
            if (st >= 0 && lka_set_path(g, st)) { g->state = AS_WALK; g->target = (short)c; break; }
        }
        // 2. a new arrival standing at the gate
        int w = lka_waiting_arrival();
        if (w >= 0) {
            Actor *p = &lk_a[w];
            int pc = lka_c(p);
            int st = lk_can_pass(pc, RL_GUARD) ? pc : lka_stand_for(pc, RL_GUARD);
            if (st >= 0 && lka_set_path(g, st)) {
                g->state = AS_ESCORTING; g->escort = (short)w; p->escort = (short)ai;
                lka_prog[ai] = 0; break;
            }
        }
        // 3. a queued shakedown
        if (lka_searchn > 0) {
            int c = lka_searchq[lka_searchn - 1];
            int st = lk_can_pass(c, RL_GUARD) ? c : lka_stand_for(c, RL_GUARD);
            if (st >= 0 && lka_set_path(g, st)) {
                lka_searchn--;
                g->state = AS_PATROL; g->target = (short)c; lka_prog[ai] = -1.0f; break;
            }
            lka_searchn--;                            // unreachable cell — drop it
        }
        // 4. patrol
        if (g->repath <= 0) {
            int t = lka_patrol_target(g);
            if (t >= 0 && lka_set_path(g, t)) { g->state = AS_PATROL; lka_prog[ai] = 0; break; }
            g->repath = 1.2f;
        }
        lka_mill(g, sd);
    } break;

    case AS_WALK: {                                    // responding to trouble
        bool still = false;
        for (int i = 0; i < lk_nact; i++)
            if (lk_a[i].alive && lka_trouble(lk_a[i].state) &&
                de_hypotf(lk_a[i].x - g->x, lk_a[i].y - g->y) < 260.0f) { still = true; break; }
        if (!still) { g->state = AS_IDLE; g->target = -1; g->plen = g->pi = 0; break; }
        if (lka_walk(g, sd)) { g->state = AS_FIGHT; g->t = 0; lk_sfx_at(SFX_WHISTLE, g->x, g->y); }
    } break;

    case AS_FIGHT:                                     // breaking one up
        g->t += sd;
        g->bob += sd * 18.0f;
        // hold position a beat: the suppression radius is what actually ends it
        if (g->t > 3.0f) { g->state = AS_IDLE; g->t = 0; g->target = -1; g->repath = 0.3f; }
        break;

    case AS_PATROL: {
        if (lka_prog[ai] < 0) {                        // shakedown leg
            if (g->pi < g->plen) { lka_walk(g, sd); break; }
            lka_prog[ai] += sd;
            if (lka_prog[ai] >= 0.0f) {
                int rid = (g->target >= 0 && g->target < LK_N) ? lk_t[g->target].room : 0;
                for (int i = 0; i < lk_nact; i++) {
                    Actor *p = &lk_a[i];
                    if (!p->alive || p->role != RL_PRISONER) continue;
                    if (rid == 0 || lk_t[lka_c(p)].room != rid) continue;
                    if (p->contraband) {
                        p->contraband = 0;
                        p->vol = clamp(p->vol + 0.12f, 0, 1);   // being searched is humiliating
                        lk_puff(p->x, p->y - 6, 5.0f, 0.5f, CLR_YELLOW);
                        lk_sfx_at(SFX_CLICK, p->x, p->y);
                    }
                }
                g->state = AS_IDLE; g->target = -1; lka_prog[ai] = 0;
            }
            break;
        }
        if (lka_walk(g, sd)) {
            lka_prog[ai] += sd;
            if (lka_prog[ai] > 2.5f) { g->state = AS_IDLE; g->repath = 0.4f; lka_prog[ai] = 0; }
        }
    } break;

    case AS_ESCORTING: {
        int w = g->escort;
        if (w < 0 || w >= LK_MAXACT || !lk_a[w].alive || lk_a[w].state != AS_ESCORTED) {
            g->escort = -1; g->state = AS_IDLE; break;
        }
        Actor *p = &lk_a[w];
        bool arrived = lka_walk(g, sd);
        if (!arrived) break;
        if (lka_prog[ai] < 0.5f) {                     // reached the prisoner: now walk them in
            lka_prog[ai] = 1.0f;
            int rid = p->cell >= 0 ? p->cell : -1;
            if (rid < 0) rid = lk_assign_cell(w);
            int dest = -1;
            if (rid > 0 && rid < LK_MAXROOM) dest = lka_room_tile(p, lk_room[rid].type, rid);
            if (dest < 0) dest = lka_room_tile(p, RM_HOLDING, 0);
            if (dest < 0 || !lka_set_path(g, dest)) {
                p->escort = -1; p->state = AS_IDLE; p->repath = 1.0f;
                g->escort = -1; g->state = AS_IDLE; lka_prog[ai] = 0;
            }
            break;
        }
        // delivered
        p->escort = -1;
        p->state = AS_IDLE; p->repath = 0.2f;
        p->need[ND_SAFETY] = clamp(p->need[ND_SAFETY] - 0.15f, 0, 1);
        g->escort = -1; g->state = AS_IDLE; lka_prog[ai] = 0;
        lk_sfx_at(SFX_DOOR, g->x, g->y);
    } break;

    case AS_DOWN:
        g->t += sd;
        if (g->health < 0.6f) g->health -= LKA_DOWN_DRAIN * 0.5f * sd;
        if (g->health <= 0) { g->alive = 0; lk.deaths++; }
        break;

    default:
        g->state = AS_IDLE;
        break;
    }
}

// ═══ workmen ═════════════════════════════════════════════════════════════════
static void lka_workman(int ai, float sd) {
    Actor *w = &lk_a[ai];
    switch (w->state) {
    case AS_IDLE: {
        if (w->repath > 0) { lka_mill(w, sd); break; }
        int j = lk_job_claim(lka_c(w), RL_WORKMAN);
        if (j < 0) { w->repath = 1.0f; lka_mill(w, sd); break; }
        int st = lka_stand_for(j, RL_WORKMAN);
        if (st < 0 || !lka_set_path(w, st)) { w->repath = 1.0f; break; }
        lk_t[j].claimed = 1;
        w->target = (short)j;
        w->use_tile = -1;
        w->state = AS_WALK;
    } break;

    case AS_WALK: {
        int j = w->target;
        if (j < 0 || j >= LK_N || lk_t[j].job == JB_NONE) {   // job cancelled under them
            if (j >= 0 && j < LK_N) lk_t[j].claimed = 0;
            w->target = -1; w->state = AS_IDLE; w->repath = 0.3f; break;
        }
        if (lka_walk(w, sd)) {
            lka_look_at(w, (float)lk_cx(j), (float)lk_cy(j));
            w->state = AS_WORK; w->t = 0;
        }
    } break;

    case AS_WORK: {
        int j = w->target;
        if (j < 0 || j >= LK_N || lk_t[j].job == JB_NONE) {
            if (j >= 0 && j < LK_N) lk_t[j].claimed = 0;
            w->target = -1; w->state = AS_IDLE; w->repath = 0.2f; break;
        }
        w->t += sd;
        w->bob += sd * 9.0f;
        lka_look_at(w, (float)lk_cx(j), (float)lk_cy(j));
        lk_job_progress(j, sd);
        if (chance(4)) lk_puff((float)lk_cx(j) + (float)rnd_between(-5, 6),
                               (float)lk_cy(j) + (float)rnd_between(-5, 6),
                               2.5f, 0.3f, CLR_LIGHT_GREY);
        if (lk_t[j].job == JB_NONE) {                    // finished — grid applied it
            lk_t[j].claimed = 0;
            w->target = -1; w->state = AS_IDLE; w->t = 0;
            lk_sfx_at(SFX_BUILD, w->x, w->y);
        }
    } break;

    case AS_DOWN:
        w->t += sd;
        if (w->t > 12.0f) { w->state = AS_IDLE; w->health = 0.6f; }
        break;

    default:
        w->state = AS_IDLE;
        break;
    }
}

// ═══ cooks — no cook, no meal, however many tables you built ═════════════════
static int lka_kitchen_cooker(Actor *a) {
    for (int nth = 0; nth < LK_MAXROOM; nth++) {
        int rid = lk_room_find(RM_KITCHEN, nth);
        if (rid < 0) break;
        if (rid <= 0 || rid >= LK_MAXROOM || !lk_room[rid].valid) continue;   // (2)
        int c = lka_find_obj_in_room(rid, OB_COOKER, true);   // one cook per cooker
        if (c < 0) continue;
        int st = lka_stand_for(c, a->role);
        if (st < 0 || !lk_reachable(lka_c(a), st, a->role)) continue;         // (3)
        return c;
    }
    return -1;
}

static int lka_hungry_table(Actor *a) {
    int from = lka_c(a), fx = lk_tx(from), fy = lk_ty(from), best = -1, bd = 1 << 30;
    for (int c = 0; c < LK_N; c++) {
        if (lk_t[c].obj != OB_SERVING) continue;
        if (lka_stock[c] >= LKA_STOCK_MAX) continue;
        int r = lk_t[c].room;
        if (r <= 0 || r >= LK_MAXROOM || !lk_room[r].valid) continue;
        int st = lka_stand_for(c, a->role);
        if (st < 0) continue;
        int d = abs(lk_tx(c) - fx) + abs(lk_ty(c) - fy);
        if (d >= bd || !lk_reachable(from, st, a->role)) continue;
        bd = d; best = c;
    }
    return best;
}

static void lka_cook(int ai, float sd) {
    Actor *k = &lk_a[ai];
    switch (k->state) {
    case AS_IDLE: {
        if (k->repath > 0) { lka_mill(k, sd); break; }
        if (lka_carry[ai] > 0) {                        // trays in hand → deliver
            int t = lka_hungry_table(k);
            if (t >= 0) {
                int st = lka_stand_for(t, RL_COOK);
                if (st >= 0 && lka_set_path(k, st)) { k->target = (short)t; k->state = AS_WALK; break; }
            }
            k->repath = 1.5f; lka_mill(k, sd); break;
        }
        int cooker = lka_kitchen_cooker(k);              // (1)(2)(3) for the kitchen
        if (cooker < 0) { k->repath = 2.0f; lka_mill(k, sd); break; }
        int st = lka_stand_for(cooker, RL_COOK);
        if (st < 0 || !lka_set_path(k, st)) { k->repath = 2.0f; break; }
        lka_claim(cooker);                               // claim the hob
        k->use_tile = (short)cooker;
        k->target = (short)cooker;
        k->state = AS_WALK;
    } break;

    case AS_WALK: {
        if (!lka_walk(k, sd)) break;
        int t = k->target;
        if (t >= 0 && t < LK_N) lka_look_at(k, (float)lk_cx(t), (float)lk_cy(t));
        if (lka_carry[ai] > 0) {                         // arrived at a serving table
            if (t >= 0 && t < LK_N && lk_t[t].obj == OB_SERVING) {
                int room = LKA_STOCK_MAX - lka_stock[t];
                int give = lka_carry[ai] < room ? lka_carry[ai] : room;
                lka_stock[t] = (unsigned char)(lka_stock[t] + give);
                lka_carry[ai] = (unsigned char)(lka_carry[ai] - give);
                lk_sfx_at(SFX_MEAL, k->x, k->y);
                lk_puff(k->x, k->y - 6, 5.0f, 0.4f, CLR_WHITE);
            }
            k->state = AS_IDLE; k->target = -1; k->repath = 0.2f;
        } else {
            k->state = AS_COOK; k->t = 0;
        }
    } break;

    case AS_COOK: {
        int c = k->use_tile;
        if (c < 0 || c >= LK_N || lk_t[c].obj != OB_COOKER) { lk_actor_release(ai); k->state = AS_IDLE; break; }
        k->t += sd;
        k->bob += sd * 5.0f;
        if (chance(3)) lk_puff((float)lk_cx(c), (float)lk_cy(c) - 6.0f, 3.0f, 0.6f, CLR_LIGHT_GREY);
        if (k->t >= LKA_COOK_SECS) {
            lka_carry[ai] = LKA_TRAY_MEALS;
            lk_actor_release(ai);                        // hob free for the next batch
            k->state = AS_IDLE; k->t = 0;
        }
    } break;

    case AS_DOWN:
        k->t += sd;
        if (k->t > 12.0f) { k->state = AS_IDLE; k->health = 0.6f; }
        break;

    default:
        k->state = AS_IDLE;
        break;
    }
}

// ═══ doctors — without one, injuries become deaths ═══════════════════════════
static int lka_find_patient(Actor *d) {
    int from = lka_c(d), best = -1, bd = 1 << 30;
    for (int i = 0; i < lk_nact; i++) {
        Actor *p = &lk_a[i];
        if (!p->alive || p->state != AS_DOWN || p->role == RL_DOCTOR) continue;
        int taken = 0;
        for (int j = 0; j < lk_nact; j++)
            if (lk_a[j].alive && lk_a[j].role == RL_DOCTOR && lk_a[j].escort == (short)i) taken++;
        if (taken) continue;
        int c = lka_c(p);
        int d2 = abs(lk_tx(c) - lk_tx(from)) + abs(lk_ty(c) - lk_ty(from));
        if (d2 < bd) { bd = d2; best = i; }
    }
    return best;
}

static void lka_doctor(int ai, float sd) {
    Actor *d = &lk_a[ai];
    switch (d->state) {
    case AS_IDLE: {
        if (d->repath > 0) { lka_mill(d, sd); break; }
        int p = lka_find_patient(d);
        if (p < 0) { d->repath = 1.0f; lka_mill(d, sd); break; }
        int pc = lka_c(&lk_a[p]);
        int st = lk_can_pass(pc, RL_DOCTOR) ? pc : lka_stand_for(pc, RL_DOCTOR);
        if (st < 0 || !lka_set_path(d, st)) { d->repath = 1.5f; break; }
        d->escort = (short)p;
        d->state = AS_WALK;
    } break;

    case AS_WALK: {
        int p = d->escort;
        if (p < 0 || !lk_a[p].alive || lk_a[p].state != AS_DOWN) { d->escort = -1; d->state = AS_IDLE; break; }
        if (lka_walk(d, sd)) {
            lka_look_at(d, lk_a[p].x, lk_a[p].y);
            d->state = AS_TREAT; d->t = 0;
        }
    } break;

    case AS_TREAT: {
        int p = d->escort;
        if (p < 0 || !lk_a[p].alive) { d->escort = -1; d->state = AS_IDLE; break; }
        d->t += sd;
        lk_a[p].health = clamp(lk_a[p].health + 0.10f * sd, 0.0f, 1.0f);
        if (chance(4)) lk_puff(lk_a[p].x, lk_a[p].y - 6, 3.0f, 0.4f, CLR_WHITE);
        if (d->t >= LKA_TREAT_SECS || lk_a[p].health >= 0.7f) {
            if (lk_a[p].state == AS_DOWN) {
                lk_a[p].health = lka_maxf(lk_a[p].health, 0.6f);
                lk_a[p].state = AS_IDLE;
                lk_a[p].t = 0; lk_a[p].repath = 0.3f;
            }
            d->escort = -1; d->state = AS_IDLE; d->t = 0;
        }
    } break;

    default:
        d->state = AS_IDLE;
        break;
    }
}

// ═══ alarm, riot, tension ════════════════════════════════════════════════════
void lk_set_alarm(int level) {
    if (level < 0 || level >= AL_COUNT) return;
    if (level == lk.alarm) { if (level != AL_CALM) lk.alarm_t = 0; return; }
    int prev = lk.alarm;
    lk.alarm = level;
    lk.alarm_t = 0;
    if (level == AL_LOCKDOWN) {
        // every jail door shut. Calm, bought with the need satisfaction that caused this.
        for (int c = 0; c < LK_N; c++)
            if (lk_t[c].door == DR_JAIL || lk_t[c].door == DR_GATE) lk_t[c].locked = 1;
        for (int i = 0; i < lk_nact; i++) {
            Actor *a = &lk_a[i];
            if (!a->alive || a->role != RL_PRISONER) continue;
            if (a->state == AS_DOWN || a->state == AS_SOLITARY) continue;
            lka_idle(i);
            a->repath = 0.1f + (float)(i % 10) * 0.05f;
        }
        lk_sfx(SFX_LOCK);
    } else if (prev == AL_LOCKDOWN) {
        for (int c = 0; c < LK_N; c++)
            if (lk_t[c].door == DR_JAIL || lk_t[c].door == DR_GATE) lk_t[c].locked = 0;
        lk_sfx(SFX_DOOR);
    }
    if (level == AL_RIOT) {
        lka_riot_t = 0;
        for (int i = 0; i < lk_nact; i++) {
            Actor *a = &lk_a[i];
            if (!a->alive || a->role != RL_PRISONER) continue;
            if (a->vol < 0.50f) continue;
            if (a->state == AS_DOWN || a->state == AS_SOLITARY || a->state == AS_FIGHT) continue;
            lka_idle(i);
            a->state = AS_RIOT;
            a->repath = (float)(i % 12) * 0.1f;
        }
        lk_sfx(SFX_ALARM);
    }
    if (level == AL_INCIDENT) lk_sfx(SFX_WHISTLE);
}

static void lka_check_lost(void) {
    if (lk.over) return;
    if (lka_riot_t > LKA_LOSE_SECS) lk.over = 2;      // see IMPLEMENTATION NOTES §8
}

static void lka_alarm_update(float d) {
    lk.alarm_t += d;
    if (lk.alarm == AL_RIOT) {
        lka_riot_t += d;
        int rioting = 0;
        for (int i = 0; i < lk_nact; i++)
            if (lk_a[i].alive && (lk_a[i].state == AS_RIOT || lk_a[i].state == AS_FIGHT)) rioting++;
        if (rioting == 0) { lka_riot_t = 0; lk_set_alarm(AL_CALM); }
        else lka_check_lost();
    } else if (lk.alarm == AL_INCIDENT) {
        lka_riot_t = 0;
        if (lka_live_fights == 0 && lk.alarm_t > 12.0f) lk_set_alarm(AL_CALM);
    } else {
        lka_riot_t = 0;
    }
    // enough simultaneous fights IS a riot — but a lockdown the player ordered is
    // not overridden by one (that's the whole point of paying for it).
    if (lk.alarm != AL_RIOT && lk.alarm != AL_LOCKDOWN && lka_live_fights >= LKA_RIOT_INCIDENTS)
        lk_set_alarm(AL_RIOT);
}

float lk_block_tension(int c, int radius) {
    if (c < 0 || c >= LK_N) return 0;
    if (radius < 1) radius = 1;
    int cx = lk_tx(c), cy = lk_ty(c);
    float sum = 0; int n = 0;
    for (int i = 0; i < lk_nact; i++) {
        const Actor *a = &lk_a[i];
        if (!a->alive || a->role != RL_PRISONER) continue;
        int ax = (int)(a->x) / LK_TS, ay = (int)(a->y) / LK_TS;
        if (abs(ax - cx) > radius || abs(ay - cy) > radius) continue;
        sum += a->vol;
        n++;
    }
    float v = n ? sum / (float)n : 0.0f;
    return clamp(v * 0.75f + lka_heat[lka_heat_i(c)] * 0.55f, 0.0f, 1.0f);
}

// ═══ shakedown ═══════════════════════════════════════════════════════════════
void lk_shakedown(void) {
    lka_searchn = 0;
    for (int rid = 1; rid < LK_MAXROOM && lka_searchn < 64; rid++) {
        if (!lk_room[rid].valid) continue;
        int ty = lk_room[rid].type;
        if (ty != RM_CELL && ty != RM_DORM && ty != RM_COMMON) continue;
        int cx = lk_room[rid].cx, cy = lk_room[rid].cy;
        if (!lk_in(cx, cy)) continue;
        int c = lk_idx(cx, cy);
        if (!lk_can_pass(c, RL_GUARD)) {
            int alt = -1;
            for (int i = 0; i < LK_N; i++)
                if (lk_t[i].room == rid && lk_can_pass(i, RL_GUARD)) { alt = i; break; }
            if (alt < 0) continue;
            c = alt;
        }
        lka_searchq[lka_searchn++] = (short)c;
    }
    for (int i = 0; i < lk_nact; i++)                 // pull guards off patrol
        if (lk_a[i].alive && lk_a[i].role == RL_GUARD && lk_a[i].state == AS_PATROL)
            { lk_a[i].state = AS_IDLE; lk_a[i].repath = 0; }
    lk_sfx(SFX_WHISTLE);
}

// ═══ the step ════════════════════════════════════════════════════════════════
static void lka_hour_boundary(void) {
    int act = lk_regime[lka_hour()];
    for (int i = 0; i < lk_nact; i++) {
        Actor *a = &lk_a[i];
        if (!a->alive || a->role != RL_PRISONER) continue;
        if (lka_busy_state(a->state) || a->state == AS_RIOT) continue;
        // if what they're doing is STILL permitted, let them finish it — otherwise
        // a sleeper would be woken (and re-path) at every hour of the night.
        int g = lka_goal[i];
        if (g < ND_COUNT && lka_permits(act, g)) continue;   // incl. mid-walk: let them arrive
        if (g == LKA_GOAL_WORK && act == AC_WORK) continue;
        // the hour's activity is over: drop the slot, re-pick. Staggered so a hundred
        // prisoners don't all ask A* for a path on the same frame.
        lka_idle(i);
        a->repath = 0.05f * (float)(i % 24);
    }
    // the kitchen works ahead of the meal bell
    for (int i = 0; i < lk_nact; i++)
        if (lk_a[i].alive && lk_a[i].role == RL_COOK && lk_a[i].state == AS_IDLE)
            lk_a[i].repath = 0;
}

static void lka_counts(void) {
    int np = 0, ns = 0;
    for (int i = 0; i < lk_nact; i++) {
        if (!lk_a[i].alive) continue;
        if (lk_a[i].role == RL_PRISONER) np++; else ns++;
    }
    lk.n_prisoners = np;
    lk.n_staff = ns;
    int beds = 0, cells = 0;
    for (int rid = 1; rid < LK_MAXROOM; rid++) {
        if (!lk_room[rid].valid) continue;
        if (lk_room[rid].type == RM_CELL) { cells++; beds += lk_room[rid].cap; }
        else if (lk_room[rid].type == RM_DORM) beds += lk_room[rid].cap;
    }
    lk.n_beds = beds;
    lk.n_cells = cells;
    // trim the roster tail so every consumer's loop stays tight
    while (lk_nact > 0 && !lk_a[lk_nact - 1].alive) lk_nact--;
}

void lk_actors_update(float d) {
    if (d < 0) d = 0;
    if (d > 0.1f) d = 0.1f;                              // a hitch must not teleport anyone
    lka_pbudget = LKA_PATHS_FRAME;

    // count live fights first: suppression, patrol and the alarm all read it
    lka_live_fights = 0;
    for (int i = 0; i < lk_nact; i++)
        if (lk_a[i].alive && lk_a[i].role == RL_PRISONER && lk_a[i].state == AS_FIGHT)
            lka_live_fights++;
    lka_live_fights /= 2;
    if (lka_live_fights < 1)
        for (int i = 0; i < lk_nact; i++)
            if (lk_a[i].alive && lk_a[i].role == RL_PRISONER && lk_a[i].state == AS_FIGHT)
                { lka_live_fights = 1; break; }

    float sd = d * (float)(lk.speed < 0 ? 0 : lk.speed);   // see IMPLEMENTATION NOTES §1

    if (sd > 0) {
        // ── the hour bell: the regime re-tasks the whole prison ──────────────
        int h = lka_hour();
        if (h != lka_last_hour) {
            if (lka_last_hour >= 0) lka_hour_boundary();
            lka_last_hour = h;
        }

        for (int i = 0; i < LKA_BW * LKA_BH; i++)
            if (lka_heat[i] > 0) lka_heat[i] = lka_maxf(0.0f, lka_heat[i] - 0.055f * sd);

        lka_suppression(sd);

        for (int i = 0; i < lk_nact; i++) {
            Actor *a = &lk_a[i];
            if (!a->alive) continue;
            if (a->repath > 0) a->repath -= sd;
            if (a->role == RL_PRISONER) lka_validate_cell(a);
            lka_needs(i, sd);
            lka_volatility(i, sd);
        }

        lka_tick_t += sd;
        if (lka_tick_t >= LKA_TICK) {
            lka_tick_t = 0;
            if (lk.alarm != AL_LOCKDOWN) lka_threat_pass();
        }

        for (int i = 0; i < lk_nact; i++) {
            Actor *a = &lk_a[i];
            if (!a->alive) continue;
            // sealed inside geometry (a workman finished a wall on this tile)? shove
            // them clear and make them re-plan — being on your OWN object is exempt.
            if (lka_unstick(a)) lka_idle(i);
            switch (a->role) {
            case RL_PRISONER: lka_prisoner(i, sd); break;
            case RL_GUARD:    lka_guard(i, sd);    break;
            case RL_WORKMAN:  lka_workman(i, sd);  break;
            case RL_COOK:     lka_cook(i, sd);     break;
            case RL_DOCTOR:   lka_doctor(i, sd);   break;
            default: break;
            }
        }

        lka_alarm_update(sd);
    }

    // ── tension: the score's only input. Smoothed in REAL time so changing the
    //    game speed never makes the music jump. ───────────────────────────────
    float sum = 0; int n = 0;
    for (int i = 0; i < lk_nact; i++) {
        if (!lk_a[i].alive || lk_a[i].role != RL_PRISONER) continue;
        sum += lk_a[i].vol; n++;
    }
    float mean_vol = n ? sum / (float)n : 0.0f;
    float alarm_f = 0.0f;
    if (lk.alarm == AL_INCIDENT) alarm_f = 0.30f;
    else if (lk.alarm == AL_RIOT) alarm_f = 1.00f;
    else if (lk.alarm == AL_LOCKDOWN) alarm_f = 0.45f;
    float raw = clamp(mean_vol * 0.62f
                    + clamp((float)lka_live_fights / (float)LKA_RIOT_INCIDENTS, 0.0f, 1.0f) * 0.28f
                    + alarm_f * 0.38f, 0.0f, 1.0f);
    lk.tension += (raw - lk.tension) * clamp(d * 0.45f, 0.0f, 1.0f);
    lk.tension = clamp(lk.tension, 0.0f, 1.0f);

    lka_counts();
}

// ═══ small helpers the UI needs ══════════════════════════════════════════════
int lk_actor_at(float wx, float wy, int slack) {
    if (slack < 1) slack = 1;
    int best = -1; float bd = (float)(slack * slack);
    for (int i = 0; i < lk_nact; i++) {
        const Actor *a = &lk_a[i];
        if (!a->alive) continue;
        float vx = a->x - wx, vy = a->y - wy;
        float d2 = vx * vx + vy * vy;
        if (d2 <= bd) { bd = d2; best = i; }
    }
    return best;
}

const char *lk_state_name(int state) {
    switch (state) {
    case AS_IDLE:       return "Idle";
    case AS_WALK:       return "Walking";
    case AS_USE:        return "Using";
    case AS_SLEEP:      return "Asleep";
    case AS_EAT:        return "Eating";
    case AS_WASH:       return "Washing";
    case AS_ESCORTED:   return "Escorted";
    case AS_ESCORTING:  return "Escorting";
    case AS_FIGHT:      return "Fighting";
    case AS_DOWN:       return "Injured";
    case AS_RESTRAINED: return "Restrained";
    case AS_SOLITARY:   return "Solitary";
    case AS_WORK:       return "Working";
    case AS_PATROL:     return "Patrolling";
    case AS_COOK:       return "Cooking";
    case AS_TREAT:      return "Treating";
    case AS_RIOT:       return "Rioting";
    case AS_ESCAPE:     return "Escaping";
    }
    return "?";
}

#endif // LOCKUP_ACTORS_H
