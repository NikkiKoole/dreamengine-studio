// ─────────────────────────────────────────────────────────────────────────────
// lockup/econ.h — MODULE: econ.  The prison's books, and its report card.
//
// OWNS
//   • lk.money movement — lk_spend / lk_earn are the ONLY places money changes.
//   • the day LEDGER: a bounded, label-merged page of every transaction, which
//     lk_econ_line()/lk_econ_lines() hand to the finance panel.
//   • the daily balance (lk_econ_day_end): federal fees in, wages out,
//     utilities out, intake grant in.
//   • lk.grade[5] — safety / hygiene / food / recreation / reform, each derived
//     from the SIM (needs, filth, staff, rooms), never from a hidden counter.
//   • lk_valuation() — what the place is worth, from the frozen cost tables.
//
// KEY ALGORITHMS
//
//   1. CONSTRUCTION BILLS ITSELF BY OBSERVATION.  The grid module applies jobs
//      and does not charge, and it cannot call us (modules never include each
//      other).  So econ keeps a 4-byte SHADOW of every tile (floor/wall/door/
//      object) and, a few times a second, diffs the world against it and bills
//      the difference.  Consequences, all of them good:
//        • the bill can never disagree with what is actually standing;
//        • it is charge-ON-DELIVERY (the Prison Architect rule) for free — the
//          money leaves exactly when the tile changes, i.e. when a workman
//          finishes, NOT when the player designates.  Queue all you like; you
//          pay when it is built.  (Unqueue is therefore free and needs no
//          refund path.)
//        • demolishing a wall/floor/door refunds nothing (the material is
//          destroyed); removing an OBJECT refunds LKE_OBJ_REFUND_PCT of its
//          cost — you sold it — which is a 50%-loss round trip, so it can't be
//          farmed.
//      The first scan after init SYNCS the shadow WITHOUT billing, so the
//      starting terrain and perimeter are not charged to the player.
//
//   2. ONE SURVEY, MANY CONSUMERS.  Utilities, all five grades and the
//      valuation all want the same census of the map (built tiles, walls,
//      doors, objects by kind, slots per need inside VALID rooms, filth, staff
//      by role).  It is gathered once into lke_sv by lke_survey() and reused.
//
//   3. GRADES ARE PROVISION × MET.  Each grade blends what the prison PROVIDES
//      (computable even with nobody in it — this is what gates intake before
//      the first bus) with how MET the matching need actually is across living
//      prisoners.  With zero prisoners the met half is undefined, so the grade
//      falls back to provision alone.  Violence enters safety through lke_heat,
//      a continuously-decaying incident/death accumulator, so a fight cools off
//      over a couple of in-game hours instead of stepping at midnight.
//      Every grade has a comment naming the thing a player looks AT on the map
//      to see why it is what it is — the ADR-0022 legibility bar.
//
// IMPLEMENTATION NOTES
//   • DEVIATION (ledger overflow): the brief said "oldest dropped".  Dropping
//     the oldest entry would silently make the page stop summing to the day's
//     real money movement — and would drop the day-end balance lines, which are
//     appended last.  Instead labels MERGE (so "brick wall" appears once, with
//     a "x12" count), and if a day somehow produces more than LKE_LEDGER-1
//     distinct labels the rest fold into one honest "other" line.  The page
//     always reconciles.
//   • DEVIATION (ledger reset timing): the brief said day_end resets the page.
//     A literal reset would mean the finance panel can NEVER be opened on a
//     closed day — the statement would be built and destroyed in the same call.
//     So the reset is DEFERRED: day_end appends its lines and marks the page
//     closed; the page is cleared by the FIRST transaction of the new day.
//     Net effect for the HUD: lk_econ_line() is always "the current accounting
//     page" — today's transactions so far, or, just after midnight, yesterday's
//     complete closed statement.  Nothing else changes.
//   • lk_econ_day_end() is IDEMPOTENT PER DAY (it early-returns if lk.day has
//     already been closed), and lk_econ_update() auto-closes a day it sees roll
//     over.  So the cart may call day_end itself or not call it at all; either
//     way wages are paid exactly once per day.  HUD/cart: calling it twice is
//     safe.
//   • No double-charging: construction is CAPITAL (one-off, on delivery),
//     utilities are a SERVICE charge on the standing stock at day end.  They
//     never bill the same thing.
//   • Natural ground (FL_DIRT, FL_GRASS) is terrain, not an asset: it is not
//     valued, not billed utilities, and not counted as "built".  A player who
//     lays grass deliberately still pays for it (the diff sees the change).
//     The STARTING PERIMETER, however, is real standing structure, so it does
//     count toward lk_valuation() (≈$11.5k on the default map) even though the
//     player never paid for it — a summary screen wanting "value ADDED" should
//     subtract the day-0 valuation.
//   • lk_econ_day_end() fires lk_sfx(SFX_CASH) once, tied to the one event
//     (the books closing).  Delete that single line if score/hud would rather
//     own the sound.
//
//   NEEDS FROM OTHER MODULES (all read-only, all contract fields):
//     grid   — lk_t[].floor/wall/door/obj/obj_ref/dirty/room, lk_room[].valid/
//              type/area.  Object COSTS come from LK_OBJ, so grid must not
//              charge for anything itself.
//     actors — lk_a[].alive/role/sec/state/need[], and lk.incidents / lk.deaths
//              as monotonically-rising counters (econ only reads their deltas).
//     cart   — lk.day increments once per in-game day; lk.money starts at the
//              seed capital; lk.alarm is AL_*.
//     econ writes ONLY lk.money and lk.grade[].  It never touches
//     lk.n_prisoners / n_staff / n_beds / n_cells (it derives its own counts,
//     so it cannot be poisoned by a stale cache).
//
//   NOT IN THE CONTRACT (added at the bottom of this module's block, per model.h
//   rule 3 — flag for reconciliation): lk_econ_net(), lk_econ_why(),
//   lk_econ_forecast().  All three are pure reads; the finance panel wants them.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef LOCKUP_ECON_H
#define LOCKUP_ECON_H

#include "studio.h"
#include "lockup/model.h"

// ── tunables ────────────────────────────────────────────────────────────────
#define LKE_LEDGER      32        // ledger lines per page (last one = "other")
#define LKE_LABEL       24        // chars per label, incl. the NUL

// federal per-prisoner-per-day fee, by security category.  Max security pays
// best and is the most volatile — that is the whole trade.
static const short LKE_FEE[SEC_COUNT] = { 60, 100, 140 };

#define LKE_GRANT_PER_BED    30   // $/day per SPARE BUILT bed (capacity held)
#define LKE_GRANT_MAX_BEDS   12   // …capped, so empty wings aren't an income farm
#define LKE_GRANT_MIN_GRADE   2   // mean grade needed to be trusted with intake
#define LKE_ROOM_BONUS      200   // valuation bonus per VALID room
#define LKE_OBJ_REFUND_PCT   50   // sell-back on removing an object

// utilities, in DIMES (1/10 $) per day per item — summed then divided, so a
// tile can cost 20¢ without floats.
#define LKE_U_FLOOR     2
#define LKE_U_WALL      1
#define LKE_U_DOOR      5
#define LKE_U_GATE     10
#define LKE_U_POWER    40         // cooker, fridge, tv, cctv, detector, phone…
#define LKE_U_LIGHT    10
#define LKE_U_WATER    20         // toilet, sink, shower head
#define LKE_U_PLAIN     5

#define LKE_BILL_TICKS      8     // update calls between construction diffs
#define LKE_SURVEY_TICKS   48     // update calls a survey stays fresh for
#define LKE_REFORM_DAYS     4     // clean days for a full reform streak
#define LKE_HEAT_INC     0.35f    // violence heat added per incident
#define LKE_HEAT_DEATH   1.00f    // …per death
#define LKE_HEAT_DECAY   0.008f   // heat bled per sim second (≈2h to forget one)

// how much of the population one unit of provision covers (full marks at 1.0)
#define LKE_COVER_GUARD   8.0f    // prisoners per guard
#define LKE_COVER_SHOWER  8.0f    // prisoners per shower head
#define LKE_COVER_TOILET  4.0f    // prisoners per toilet
#define LKE_COVER_SEAT    1.0f    // prisoners per canteen seat
#define LKE_COVER_SERVE   2.5f    // prisoners per serving slot
#define LKE_COVER_REC     3.0f    // prisoners per rec slot

#define LKE_IS_BUILT_FLOOR(f)  ((f) >= FL_GRAVEL)   // dirt + grass are terrain

// ── the ledger ──────────────────────────────────────────────────────────────
typedef struct {
    char label[LKE_LABEL];        // what the panel shows ("brick wall x12")
    char base[LKE_LABEL];         // the merge key, without the count
    int  amount;                  // signed: negative = money out
    int  count;
} LkeLine;
static LkeLine lke_led[LKE_LEDGER];
static int     lke_nled;
static bool    lke_page_closed;   // the visible page is a CLOSED day statement

// ── the census (see KEY ALGORITHMS 2) ───────────────────────────────────────
typedef struct {
    int prisoners, staff, role_n[RL_COUNT];
    int built_tiles, walls, doors, objects;
    int util_dimes;                  // utilities for one day, in 1/10 $
    int assets;                      // material value in place
    int valid_rooms, valid_room[RM_COUNT], room_area[RM_COUNT];
    int slots[ND_COUNT];             // usable capacity per need, valid rooms only
    int bed_slots, toilet_slots, shower_slots, sink_slots;
    int seat_slots, serve_slots, rec_slots, book_slots;
    int filth_sum, filth_tiles;
} LkeSurvey;
static LkeSurvey lke_sv;

// ── module state ────────────────────────────────────────────────────────────
static unsigned char lke_sh_floor[LK_N], lke_sh_wall[LK_N];
static unsigned char lke_sh_door[LK_N],  lke_sh_obj[LK_N];
static bool  lke_synced;             // shadow has been primed (no bill on prime)
static int   lke_tick, lke_bill_tick, lke_survey_tick;
static int   lke_last_day;           // last day closed; -1 = nothing closed yet
static int   lke_inc_seen, lke_dth_seen;
static float lke_heat;               // decaying violence memory, 0..n
static float lke_score[5];           // raw 0..1 behind lk.grade[], for lk_econ_why
static char  lke_tmp[LKE_LABEL];     // scratch for composed labels

// per-actor-slot reform tracking (slot reuse detected by id + alive edge)
static unsigned char lke_clean[LK_MAXACT];    // consecutive clean days
static unsigned char lke_flagged[LK_MAXACT];  // had an incident today
static unsigned char lke_was_alive[LK_MAXACT];
static short         lke_seen_id[LK_MAXACT];

// ── tiny string helpers (no libc: this header includes only studio.h) ────────
static bool lke_streq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static int lke_len(const char *s) { int n = 0; while (s && s[n]) n++; return n; }
static void lke_cpy(char *dst, int cap, const char *src) {
    int i = 0;
    if (!dst || cap <= 0) return;
    if (src) while (src[i] && i < cap - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}
static void lke_join(char *dst, int cap, const char *a, const char *b) {
    int n, i = 0;
    lke_cpy(dst, cap, a);
    n = lke_len(dst);
    while (b && b[i] && n < cap - 1) dst[n++] = b[i++];
    dst[n] = 0;
}
// "brick wall" + 12 → "brick wall x12"; leaves the base alone if it won't fit.
static void lke_num(char *dst, int cap, const char *base, int n) {
    char tmp[12];
    int p, k = 0;
    lke_cpy(dst, cap, base);
    p = lke_len(dst);
    if (n < 0) n = 0;
    do { tmp[k++] = (char)('0' + n % 10); n /= 10; } while (n && k < 11);
    if (p + 2 + k > cap - 1) return;
    dst[p++] = ' '; dst[p++] = 'x';
    while (k > 0) dst[p++] = tmp[--k];
    dst[p] = 0;
}

// ── ledger ──────────────────────────────────────────────────────────────────
// Merges by label so twenty walls are one line.  Overflow folds into "other"
// rather than dropping, so the page always sums to the day's money movement.
static void lke_ledger_add(const char *why, int amount) {
    LkeLine *e;
    int i;
    if (lke_page_closed) { lke_nled = 0; lke_page_closed = false; }
    if (!why || !*why) why = "misc";
    for (i = 0; i < lke_nled; i++) {
        if (lke_streq(lke_led[i].base, why)) {
            lke_led[i].amount += amount;
            lke_led[i].count++;
            if (lke_led[i].count > 1)
                lke_num(lke_led[i].label, LKE_LABEL, lke_led[i].base, lke_led[i].count);
            return;
        }
    }
    if (lke_nled < LKE_LEDGER - 1) {
        e = &lke_led[lke_nled++];
        lke_cpy(e->base,  LKE_LABEL, why);
        lke_cpy(e->label, LKE_LABEL, e->base);
        e->amount = amount;
        e->count  = 1;
        return;
    }
    e = &lke_led[LKE_LEDGER - 1];
    if (lke_nled < LKE_LEDGER) {
        lke_nled = LKE_LEDGER;
        lke_cpy(e->base,  LKE_LABEL, "other");
        lke_cpy(e->label, LKE_LABEL, "other");
        e->amount = 0;
        e->count  = 0;
    }
    e->amount += amount;
    e->count++;
}

void lk_spend(int amount, const char *why) {
    if (amount == 0) return;                 // don't litter the page with $0
    lk.money -= amount;
    lke_ledger_add(why ? why : "spending", -amount);
}
void lk_earn(int amount, const char *why) {
    if (amount == 0) return;
    lk.money += amount;
    lke_ledger_add(why ? why : "income", amount);
}

const char *lk_econ_line(int i, int *amount) {
    if (amount) *amount = 0;
    if (i < 0 || i >= lke_nled) return NULL;
    if (amount) *amount = lke_led[i].amount;
    return lke_led[i].label;
}
int lk_econ_lines(void) { return lke_nled; }

// NOT IN THE CONTRACT — the page's net, so the panel can show a bottom line
// without the caller re-summing.
int lk_econ_net(void) {
    int i, n = 0;
    for (i = 0; i < lke_nled; i++) n += lke_led[i].amount;
    return n;
}

// ── the census ──────────────────────────────────────────────────────────────
static int lke_obj_util(int ob) {
    switch (ob) {
        case OB_COOKER: case OB_FRIDGE:   case OB_TV:   case OB_CCTV:
        case OB_DETECTOR: case OB_PHONE:  case OB_MEDBED:  return LKE_U_POWER;
        case OB_LIGHT:                                     return LKE_U_LIGHT;
        case OB_TOILET: case OB_SINK: case OB_SHOWERHEAD:  return LKE_U_WATER;
        default:                                           return LKE_U_PLAIN;
    }
}

static void lke_survey(void) {
    LkeSurvey s = (LkeSurvey){ 0 };
    int i, c;

    for (i = 0; i < LK_MAXACT; i++) {
        const Actor *a = &lk_a[i];
        int rl;
        if (!a->alive) continue;
        rl = a->role;
        if (rl < 0 || rl >= RL_COUNT) rl = RL_PRISONER;
        s.role_n[rl]++;
        if (rl == RL_PRISONER) s.prisoners++; else s.staff++;
    }

    for (c = 0; c < LK_N; c++) {
        const Tile *t = &lk_t[c];
        int f  = t->floor, w = t->wall, dr = t->door;
        int ob = t->obj_ref ? OB_NONE : t->obj;
        int rid = t->room, rt = RM_NONE;
        bool rvalid = false;
        if (f  >= FL_COUNT) f  = FL_DIRT;
        if (w  >= WL_COUNT) w  = WL_NONE;
        if (dr >= DR_COUNT) dr = DR_NONE;
        if (ob >= OB_COUNT) ob = OB_NONE;
        if (rid > 0 && rid < LK_MAXROOM) {
            const Room *r = &lk_room[rid];
            rt = r->type < RM_COUNT ? r->type : RM_NONE;
            rvalid = r->valid != 0;
        }

        if (LKE_IS_BUILT_FLOOR(f)) {
            s.built_tiles++;
            s.util_dimes += LKE_U_FLOOR;
            s.assets     += LK_FLOOR_COST[f];
        }
        if (w != WL_NONE) {
            s.walls++;
            s.util_dimes += LKE_U_WALL;
            s.assets     += LK_WALL_COST[w];
        }
        if (dr != DR_NONE) {
            s.doors++;
            s.util_dimes += (dr == DR_GATE) ? LKE_U_GATE : LKE_U_DOOR;
            s.assets     += LK_DOOR_COST[dr];
        }
        if (ob != OB_NONE) {
            const ObjDef *od = &LK_OBJ[ob];
            int nd = od->serves;
            s.objects++;
            s.util_dimes += lke_obj_util(ob);
            s.assets     += od->cost;
            // capacity only counts inside a VALID room — an unenclosed cell's
            // bed is furniture, not a bed space.  This is the contention rule.
            if (rvalid) {
                if (nd < ND_COUNT && !od->staff_only) s.slots[nd] += od->slots;
                if (rt == RM_CELL || rt == RM_DORM) {
                    if (nd == ND_SLEEP) s.bed_slots += od->slots;
                }
                if (rt == RM_CANTEEN) {
                    if (ob == OB_BENCH || ob == OB_CHAIR) s.seat_slots  += od->slots;
                    if (ob == OB_SERVING)                 s.serve_slots += od->slots;
                }
                if (ob == OB_SHOWERHEAD) s.shower_slots += od->slots;
                if (ob == OB_SINK)       s.sink_slots   += od->slots;
                if (ob == OB_TOILET)     s.toilet_slots += od->slots;
                if (ob == OB_BOOKSHELF)  s.book_slots   += od->slots;
                if (nd == ND_REC && !od->staff_only) s.rec_slots += od->slots;
            }
        }
        // filth only reads where the prison actually is — untouched dirt
        // outside the fence is not "dirty".
        if (LKE_IS_BUILT_FLOOR(f) || w != WL_NONE) {
            s.filth_sum += t->dirty;
            s.filth_tiles++;
        }
    }

    // rooms.  Scan the whole array and judge by content, so this works whether
    // grid keeps rooms in [0..nroom) or [1..nroom].
    for (i = 1; i < LK_MAXROOM; i++) {
        const Room *r = &lk_room[i];
        int rt = r->type;
        if (rt <= RM_NONE || rt >= RM_COUNT) continue;
        if (!r->area || !r->valid) continue;
        s.valid_rooms++;
        s.valid_room[rt]++;
        s.room_area[rt] += r->area;
    }
    // a yard is recreation without needing a single object in it
    s.rec_slots += s.room_area[RM_YARD] / 6;

    lke_sv = s;
    lke_survey_tick = lke_tick;
}

static void lke_survey_ensure(void) {
    if (lke_survey_tick >= 0 && (lke_tick - lke_survey_tick) < LKE_SURVEY_TICKS) return;
    lke_survey();
}

// ── construction billing by observation (see KEY ALGORITHMS 1) ──────────────
static void lke_sync_shadow(void) {
    int c;
    for (c = 0; c < LK_N; c++) {
        const Tile *t = &lk_t[c];
        lke_sh_floor[c] = t->floor;
        lke_sh_wall[c]  = t->wall;
        lke_sh_door[c]  = t->door;
        lke_sh_obj[c]   = t->obj_ref ? (unsigned char)OB_NONE : t->obj;
    }
}

static void lke_bill_world(void) {
    int c;
    for (c = 0; c < LK_N; c++) {
        const Tile *t = &lk_t[c];
        unsigned char f  = t->floor, w = t->wall, dr = t->door;
        unsigned char ob = t->obj_ref ? (unsigned char)OB_NONE : t->obj;

        if (f != lke_sh_floor[c]) {
            int nf = f < FL_COUNT ? f : FL_DIRT;
            lke_sh_floor[c] = f;
            if (LK_FLOOR_COST[nf] > 0) {
                lke_join(lke_tmp, LKE_LABEL, LK_FLOOR_NAME[nf], " floor");
                lk_spend(LK_FLOOR_COST[nf], lke_tmp);
            }
        }
        if (w != lke_sh_wall[c]) {
            int nw = w < WL_COUNT ? w : WL_NONE;
            lke_sh_wall[c] = w;
            if (nw != WL_NONE) {
                if (nw == WL_FENCE) lke_cpy(lke_tmp, LKE_LABEL, "fence");
                else lke_join(lke_tmp, LKE_LABEL, LK_WALL_NAME[nw], " wall");
                lk_spend(LK_WALL_COST[nw], lke_tmp);
            }
        }
        if (dr != lke_sh_door[c]) {
            int nd = dr < DR_COUNT ? dr : DR_NONE;
            lke_sh_door[c] = dr;
            if (nd != DR_NONE) lk_spend(LK_DOOR_COST[nd], LK_DOOR_NAME[nd]);
        }
        if (ob != lke_sh_obj[c]) {
            int no = ob < OB_COUNT ? ob : OB_NONE;
            int oo = lke_sh_obj[c] < OB_COUNT ? lke_sh_obj[c] : OB_NONE;
            lke_sh_obj[c] = ob;
            if (no != OB_NONE) {
                lk_spend(LK_OBJ[no].cost, LK_OBJ[no].name);
            } else if (oo != OB_NONE) {
                int back = LK_OBJ[oo].cost * LKE_OBJ_REFUND_PCT / 100;
                if (back > 0) {
                    lke_join(lke_tmp, LKE_LABEL, "sold ", LK_OBJ[oo].name);
                    lk_earn(back, lke_tmp);
                }
            }
        }
    }
}

// ── violence memory + reform streaks ────────────────────────────────────────
static bool lke_bad_state(int st) {
    return st == AS_FIGHT || st == AS_DOWN || st == AS_RESTRAINED ||
           st == AS_SOLITARY || st == AS_RIOT || st == AS_ESCAPE;
}

static void lke_track(void) {
    int i;
    int inc = lk.incidents - lke_inc_seen;
    int dth = lk.deaths    - lke_dth_seen;
    if (inc > 0) { lke_heat += LKE_HEAT_INC   * (float)inc; lke_inc_seen = lk.incidents; }
    if (dth > 0) { lke_heat += LKE_HEAT_DEATH * (float)dth; lke_dth_seen = lk.deaths; }
    if (inc < 0) lke_inc_seen = lk.incidents;      // counters reset (new game)
    if (dth < 0) lke_dth_seen = lk.deaths;

    for (i = 0; i < LK_MAXACT; i++) {
        const Actor *a = &lk_a[i];
        if (!a->alive) { lke_was_alive[i] = 0; continue; }
        if (!lke_was_alive[i] || lke_seen_id[i] != a->id) {
            lke_clean[i]   = 0;                    // a new body in this slot
            lke_flagged[i] = 0;
            lke_seen_id[i] = a->id;
        }
        lke_was_alive[i] = 1;
        if (a->role == RL_PRISONER && lke_bad_state(a->state)) lke_flagged[i] = 1;
    }
}

// ── grading ─────────────────────────────────────────────────────────────────
static int lke_band(float s) {
    if (s < 0.18f) return 0;
    if (s < 0.34f) return 1;
    if (s < 0.50f) return 2;
    if (s < 0.66f) return 3;
    if (s < 0.82f) return 4;
    return 5;
}
static float lke_cover(int slots, float per_prisoner, int np) {
    if (np <= 0) return slots > 0 ? 1.0f : 0.0f;
    return clamp((float)slots * per_prisoner / (float)np, 0.0f, 1.0f);
}
// how well fed / washed / rested the population actually is, 0..1
static float lke_met(int nd, int *np_out) {
    int i, np = 0;
    float sum = 0.0f;
    for (i = 0; i < LK_MAXACT; i++) {
        const Actor *a = &lk_a[i];
        if (!a->alive || a->role != RL_PRISONER) continue;
        np++;
        sum += 1.0f - clamp(a->need[nd], 0.0f, 1.0f);
    }
    if (np_out) *np_out = np;
    return np ? sum / (float)np : 1.0f;
}

// is the food chain actually joined up?  0 = no canteen, low = tables but no
// meals.  This is the design's "no cook, no meal, no matter how many tables".
static float lke_food_supply(int np) {
    float seats, serve, supply;
    if (!lke_sv.valid_room[RM_CANTEEN]) return 0.0f;
    seats  = lke_cover(lke_sv.seat_slots,  LKE_COVER_SEAT,  np);
    serve  = lke_cover(lke_sv.serve_slots, LKE_COVER_SERVE, np);
    supply = 0.5f * seats + 0.5f * serve;
    if (!lke_sv.valid_room[RM_KITCHEN] || lke_sv.role_n[RL_COOK] <= 0)
        supply *= 0.30f;                 // a dining room nobody cooks for
    return supply;
}

void lk_grade_recalc(void) {
    int np = 0, i;
    float filth, pen, prov, streak;
    float met_safe, met_hyg, met_food, met_rec;

    lke_survey_ensure();

    met_safe = lke_met(ND_SAFETY,  &np);
    met_hyg  = lke_met(ND_HYGIENE, 0);
    met_food = lke_met(ND_FOOD,    0);
    met_rec  = lke_met(ND_REC,     0);

    filth = lke_sv.filth_tiles
          ? clamp((float)lke_sv.filth_sum / (255.0f * (float)lke_sv.filth_tiles), 0.0f, 1.0f)
          : 0.0f;

    // recent violence, decaying — plus a hard hit while the prison is rioting.
    pen = clamp(lke_heat * 0.30f, 0.0f, 0.90f);
    if (lk.alarm >= AL_RIOT) pen = clamp(pen + 0.45f, 0.0f, 1.0f);

    // SAFETY — look at: guards on the floor vs population, the red SAFE bars,
    //          and the incident/death counters in the top bar.
    prov = lke_cover(lke_sv.role_n[RL_GUARD], LKE_COVER_GUARD, np);
    lke_score[0] = (np ? 0.60f * met_safe + 0.40f * prov : prov) - pen;

    // HYGIENE — look at: shower heads and toilets per prisoner, and the filth
    //           on the floors (the dirtier a block looks, the lower this is).
    //           Filth is a PENALTY, not a component: an empty field is not
    //           hygienic just because nobody has dirtied it yet.
    prov = 0.5f * lke_cover(lke_sv.shower_slots, LKE_COVER_SHOWER, np)
         + 0.5f * lke_cover(lke_sv.toilet_slots, LKE_COVER_TOILET, np);
    lke_score[1] = (np ? 0.55f * met_hyg + 0.45f * prov : prov) - 0.35f * filth;

    // FOOD — look at: a VALID canteen with enough benches and serving tables,
    //        a VALID kitchen behind it, and a cook alive to stand in it.
    prov = lke_food_supply(np);
    lke_score[2] = np ? 0.60f * met_food + 0.40f * prov : prov;

    // RECREATION — look at: yard area, TVs, weights, pool tables — anywhere a
    //              prisoner is allowed to spend a Free Time or Yard slot.
    prov = lke_cover(lke_sv.rec_slots, LKE_COVER_REC, np);
    lke_score[3] = np ? 0.60f * met_rec + 0.40f * prov : prov;

    // REFORM — look at: how many days the block has gone without a fight (a
    //          prisoner in solitary / down / restrained resets their streak),
    //          and whether there is a workshop, visitation or a bookshelf.
    streak = 0.0f;
    if (np) {
        int n = 0;
        for (i = 0; i < LK_MAXACT; i++) {
            const Actor *a = &lk_a[i];
            int cl;
            if (!a->alive || a->role != RL_PRISONER) continue;
            cl = lke_clean[i] > LKE_REFORM_DAYS ? LKE_REFORM_DAYS : lke_clean[i];
            streak += (float)cl / (float)LKE_REFORM_DAYS;
            n++;
        }
        if (n) streak /= (float)n;
    }
    prov = 0.0f;
    if (lke_sv.valid_room[RM_WORKSHOP]) prov += 0.40f;
    if (lke_sv.valid_room[RM_VISIT])    prov += 0.30f;
    if (lke_sv.book_slots > 0)          prov += 0.30f;
    lke_score[4] = np ? 0.55f * streak + 0.45f * prov : prov;

    for (i = 0; i < 5; i++) {
        lke_score[i] = clamp(lke_score[i], 0.0f, 1.0f);
        lk.grade[i]  = lke_band(lke_score[i]);
    }
}

// NOT IN THE CONTRACT — one line saying WHY a grade is what it is, so the panel
// can point the player at the map instead of showing a bare number.
const char *lk_econ_why(int g) {
    int np = lke_sv.prisoners;
    switch (g) {
        case 0:
            if (lk.alarm >= AL_RIOT)                return "riot in progress";
            if (lke_sv.role_n[RL_GUARD] <= 0)       return "no guards on the floor";
            if (lke_heat > 0.5f)                    return "recent violence";
            if (np && lke_score[0] < 0.5f)          return "prisoners feel unsafe";
            return "calm";
        case 1:
            if (lke_sv.shower_slots <= 0)           return "no shower heads";
            if (lke_sv.toilet_slots <= 0)           return "no toilets";
            if (lke_sv.filth_tiles &&
                lke_sv.filth_sum > 40 * lke_sv.filth_tiles) return "filth building up";
            if (np && lke_score[1] < 0.5f)          return "too few showers";
            return "clean";
        case 2:
            if (!lke_sv.valid_room[RM_CANTEEN])     return "no valid canteen";
            if (!lke_sv.valid_room[RM_KITCHEN])     return "no valid kitchen";
            if (lke_sv.role_n[RL_COOK] <= 0)        return "no cook on staff";
            if (lke_sv.serve_slots <= 0)            return "no serving table";
            if (np && lke_score[2] < 0.5f)          return "meals not reaching them";
            return "well fed";
        case 3:
            if (lke_sv.rec_slots <= 0)              return "nothing to do";
            if (np && lke_score[3] < 0.5f)          return "not enough yard time";
            return "occupied";
        case 4:
            if (!np)                                return "no prisoners yet";
            if (!lke_sv.valid_room[RM_WORKSHOP] &&
                !lke_sv.valid_room[RM_VISIT] &&
                !lke_sv.book_slots)                 return "no workshop or visits";
            if (lke_score[4] < 0.5f)                return "too many incidents";
            return "settling down";
        default: return "";
    }
}

// ── the daily balance ───────────────────────────────────────────────────────
// Shared by day_end (which applies it) and forecast (which only reports it).
static int lke_balance(int *fee, int *wages, int *util, int *grant, int *held) {
    int i, f = 0, w = 0, u, g = 0, h = 0, mean = 0, spare;
    for (i = 0; i < LK_MAXACT; i++) {
        const Actor *a = &lk_a[i];
        if (!a->alive) continue;
        if (a->role == RL_PRISONER) {
            int sc = a->sec;
            if (sc < 0 || sc >= SEC_COUNT) sc = SEC_NORM;
            f += LKE_FEE[sc];
        } else if (a->role > 0 && a->role < RL_COUNT) {
            w += LK_ROLE_WAGE[a->role];
        }
    }
    u = lke_sv.util_dimes / 10;

    // The grant pays for capacity that EXISTS — beds standing in valid cells
    // beyond the current population.  Promised capacity earns nothing.
    spare = lke_sv.bed_slots - lke_sv.prisoners;
    if (spare < 0) spare = 0;
    if (spare > LKE_GRANT_MAX_BEDS) spare = LKE_GRANT_MAX_BEDS;
    for (i = 0; i < 5; i++) mean += lk.grade[i];
    mean /= 5;
    if (spare > 0) {
        if (mean >= LKE_GRANT_MIN_GRADE) g = spare * LKE_GRANT_PER_BED;
        else h = 1;                       // fit to build, not fit to be trusted
    }
    if (fee)   *fee   = f;
    if (wages) *wages = w;
    if (util)  *util  = u;
    if (grant) *grant = g;
    if (held)  *held  = h;
    return f - w - u + g;
}

// NOT IN THE CONTRACT — tomorrow's balance at today's standing, for a "daily
// net" readout in the finance panel.  Pure read; applies nothing.
int lk_econ_forecast(int *fee, int *wages, int *util, int *grant) {
    lke_survey_ensure();
    return lke_balance(fee, wages, util, grant, 0);
}

void lk_econ_day_end(void) {
    int fee = 0, wages = 0, util = 0, grant = 0, held = 0, i;

    if (lke_last_day == lk.day) return;      // idempotent: one close per day
    lke_last_day = lk.day;

    lke_survey();                            // bill against the world as it is
    lke_balance(&fee, &wages, &util, &grant, &held);

    // Order matters: this is the statement the player reads at midnight.
    lk_earn (fee,   "federal fees");
    lk_spend(wages, "staff wages");
    lk_spend(util,  "utilities");
    if (grant)     lk_earn(grant, "intake grant");
    else if (held) lke_ledger_add("grant held: grade", 0);   // an honest $0 line

    // reform streaks roll over BEFORE grading, so today's clean day counts.
    for (i = 0; i < LK_MAXACT; i++) {
        const Actor *a = &lk_a[i];
        if (!a->alive || a->role != RL_PRISONER) { lke_flagged[i] = 0; continue; }
        if (lke_flagged[i]) lke_clean[i] = 0;
        else if (lke_clean[i] < 200) lke_clean[i]++;
        lke_flagged[i] = 0;
    }

    lk_grade_recalc();

    lke_page_closed = true;   // page clears on the new day's first transaction
    lk_sfx(SFX_CASH);
}

// ── valuation ───────────────────────────────────────────────────────────────
// What the place is worth: everything standing at its table cost, plus a bonus
// for each room that actually WORKS (a valid room is worth more than the sum of
// its furniture — that is the whole craft of the game).
int lk_valuation(void) {
    lke_survey_ensure();
    return lke_sv.assets + lke_sv.valid_rooms * LKE_ROOM_BONUS;
}

// ── lifecycle ───────────────────────────────────────────────────────────────
void lk_econ_init(void) {
    int i;
    lke_nled = 0;
    lke_page_closed = false;
    lke_synced = false;
    lke_tick = 0;
    lke_bill_tick = 0;
    lke_survey_tick = -1;
    lke_last_day = -1;
    lke_inc_seen = lk.incidents;
    lke_dth_seen = lk.deaths;
    lke_heat = 0.0f;
    lke_sv = (LkeSurvey){ 0 };
    for (i = 0; i < 5; i++) lke_score[i] = 0.0f;
    for (i = 0; i < LK_MAXACT; i++) {
        lke_clean[i] = 0;
        lke_flagged[i] = 0;
        lke_was_alive[i] = 0;
        lke_seen_id[i] = -1;
    }
    lke_tmp[0] = 0;
}

void lk_econ_update(float d) {
    lke_tick++;

    // First tick after init: prime the shadow WITHOUT billing (the starting
    // terrain and perimeter are not the player's purchase), then grade once so
    // the panel is never showing five zeroes.  Deferred to update rather than
    // init because init order across modules isn't ours to assume.
    if (!lke_synced) {
        lke_sync_shadow();
        lke_synced   = true;
        lke_last_day = lk.day;
        lke_inc_seen = lk.incidents;
        lke_dth_seen = lk.deaths;
        lke_survey();
        lke_track();
        lk_grade_recalc();
        return;
    }

    if (d < 0.0f)  d = 0.0f;
    if (d > 0.25f) d = 0.25f;               // don't let a hitch erase violence
    if (lke_heat > 0.0f) {
        lke_heat -= d * LKE_HEAT_DECAY;
        if (lke_heat < 0.0f) lke_heat = 0.0f;
    }

    // Construction billing runs off a TICK count, not dt, so a paused prison
    // still reconciles the moment anything changes.
    if (lke_tick - lke_bill_tick >= LKE_BILL_TICKS) {
        lke_bill_tick = lke_tick;
        lke_bill_world();
        lke_track();
    }

    // Backstop: if the cart never calls day_end, close the day ourselves.
    // Both paths are guarded by lke_last_day, so wages are paid exactly once.
    if (lk.day != lke_last_day) lk_econ_day_end();
}

#endif // LOCKUP_ECON_H
