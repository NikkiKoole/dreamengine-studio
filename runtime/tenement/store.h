// ─────────────────────────────────────────────────────────────────────────────
// tenement/store.h — items, containers, ownership, hauling (design §6).
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tns_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
//
// ── WHAT THIS MODULE IS ─────────────────────────────────────────────────────
// There is NO storage system here, and that is the point. Storage is FURNITURE that happens to
// declare a storage tag in its offer set: a fridge offers TN_SERVE_HUNGER *and* TN_STORE_FOOD, so
// "where can this go" is the same lookup as "who serves hunger" (design §2, one index, three
// consumers). Nothing below asks what an object IS, only what it OFFERS, and nothing below asks
// what an item IS, only which class it belongs to. A new kind of thing is a row in TNS_ITEMS.
//
// ── OWNERSHIP IS A TERM, NOT A FILTER ───────────────────────────────────────
// The headline. §10 records the slice's finding: household-1 tenants eat out of household-0's
// fridge because `household` is not in the score. The fix is NOT a permission check. The design's
// own hard-won lesson (§2, §10) is that a deficit belongs in the score rather than in a pre-filter,
// and ownership is the same shape: using a neighbour's things costs a DETOUR, measured in tiles so
// it lands in the same unit as travel. Being sated you walk home; being desperate you raid the
// fridge next door, which is exactly the small legible disaster §6 asks the game to generate.
//
// tn_ownership_penalty() is that one number and it lives here, in the ownership module, so the need
// side and the storage side can never drift apart. THE NEED SIDE IS NOT WIRED YET: the score lives
// in offer.h, which this module does not own. See "REPORT" at the bottom of this file for the exact
// three-line change, and tn_store_selfcheck()'s case S5, which pins the arithmetic of the flip.
//
// NOT in v1: nesting, stack sizes, per-item-type filters, reservation locking.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_STORE_H
#define TENEMENT_STORE_H

#ifdef DE_SPEC
#include "spec.h"                 // for tn_store_selfcheck() at the bottom; guarded, dev-only
#endif

// ─────────────────────────────────────────────────────────────────────────────
// THE ITEM CATALOGUE — a new kind of thing is a TABLE ROW (contract rule 3).
//
// An item IS its storage class and its value; there is no item-type enum and nothing switches on
// which row a thing came from. That is the navkit stockpile-filter fix (design §3b): the fridge
// accepts `leftovers` for exactly the same reason it accepts `meal`, and adding either costs one
// line here and zero lines everywhere else.
//
// `name` is for the HUD and for spec messages. It is never matched on.
// ─────────────────────────────────────────────────────────────────────────────
typedef struct { const char *name; unsigned char store_tag; unsigned char value; } TnsItemDef;
static const TnsItemDef TNS_ITEMS[] = {
    { "meal",      TN_STORE_FOOD,     4 },
    { "bolt",      TN_STORE_GOODS,   12 },   // TN_RECIPES[0]'s output. NOTHING offers TN_STORE_GOODS
                                             // yet, so it has nowhere to live — a missing table row
                                             // in offer.h, not a missing code path. See REPORT.
    { "shirt",     TN_STORE_CLOTHES,  6 },
    { "leftovers", TN_STORE_FOOD,     1 },   // ← APPENDED AFTER tn_store_tick() WAS WRITTEN, on
                                             // purpose. No line of code anywhere names it. Case S6
                                             // proves the fridge takes it anyway.
};
#define TNS_ITEM_N ((int)(sizeof TNS_ITEMS / sizeof TNS_ITEMS[0]))

// The contract stores indices in `signed char` fields (TnItem.held_by/.stored_in,
// TnAgent.carrying/.target_obj) while TN_MAX_ITEMS is 256 and TN_MAX_OBJECTS is 192. Index 128
// would wrap NEGATIVE, and negative already MEANS "loose on the floor" / "empty-handed" — a silent
// teleport, not a crash. Refused at the door here; the real fix is a contract change (see REPORT).
#define TNS_IDX_MAX 127

// The ownership detour, IN TILES so it is in the same unit as travel. Larger than the diagonal of
// the building (13x9 → ~16), so a comfortable tenant crosses the whole floor to reach its own
// things rather than touch a neighbour's — and small enough that it vanishes under desperation.
#define TNS_TRESPASS 24

// ── plumbing ────────────────────────────────────────────────────────────────
// The offer row for one tag on one object. Deliberately computed from the PUBLIC tables rather than
// borrowed from offer.h's private helper, so this module depends on the contract and not on a
// sibling's internals (and so it can be included before offer.h if the integrator prefers that to a
// declaration in model.h — see REPORT).
static const TnOffer *tns_offer_of(int obj, TnTag tag) {
    const int kind = tn_obj[obj].kind;
    for (int i = 0; i < TN_OFFER_N[kind]; i++)
        if (TN_OFFERS[kind][i].tag == (unsigned char)tag) return &TN_OFFERS[kind][i];
    return NULL;
}

// Straight-line tile distance, the same measure offer.h scores travel with. When that becomes a
// real path, both change together and nothing else here moves.
static int tns_dist(int ax, int ay, int bx, int by) {
    const int dx = ax - bx, dy = ay - by;
    return (int)(sqrtf((float)(dx * dx + dy * dy)) + 0.5f);
}

// ── containers ──────────────────────────────────────────────────────────────

// How many items are inside this object right now.
int tn_store_count(int obj) {
    int n = 0;
    for (int i = 0; i < tn_item_n; i++) if (tn_item[i].stored_in == obj) n++;
    return n;
}

// Free slots for `tag`, or 0 if this object does not accept that class at all.
//
// On a STORAGE offer, TnOffer.capacity counts SLOTS rather than simultaneous users (the contract's
// wording covers the need case). One field, two readings, no ambiguity in practice because a need
// offer and a storage offer are different rows. If an object ever offers two storage classes they
// SHARE the same physical fill, which is why the count below is per-object and not per-tag.
int tn_store_room(int obj, TnTag tag) {
    if (obj < 0 || obj >= tn_obj_n) return 0;
    if ((unsigned)tag < TN_SERVE_COUNT) return 0;          // a need is not a storage class
    const TnOffer *of = tns_offer_of(obj, tag);
    if (!of) return 0;
    const int room = (int)of->capacity - tn_store_count(obj);
    return room > 0 ? room : 0;
}

// Will this container physically take this item? Tag and room ONLY. Ownership is deliberately NOT
// here: it is a cost in the ranking, not a wall (see the header). A wall would mean a tenant with a
// full fridge stands holding its dinner forever next to the neighbour's empty one.
bool tn_store_accepts(int obj, int item) {
    if (item < 0 || item >= tn_item_n) return false;
    return tn_store_room(obj, (TnTag)tn_item[item].store_tag) > 0;
}

// THE OWNERSHIP TERM. Extra tiles this agent should feel for reaching for this object.
//   • yours, or communal (household -1): nothing
//   • a NEED offer on someone else's thing: the full detour when comfortable, fading to zero as the
//     need bottoms out. That gradient IS design §6's comedy: the desperate tenant raids the fridge
//   • a capability or storage class: the flat detour. There is no deficit to be desperate about, and
//     the honest fallback for an item is the floor
int tn_ownership_penalty(int agent, int obj, TnTag tag) {
    if (agent < 0 || agent >= tn_agent_n || obj < 0 || obj >= tn_obj_n) return 0;
    const int owner = tn_obj[obj].household;
    if (owner < 0 || owner == (int)tn_agent[agent].household) return 0;
    if ((unsigned)tag < TN_SERVE_COUNT)
        return TNS_TRESPASS * (int)tn_agent[agent].need[tag] / 255;
    return TNS_TRESPASS;
}

// Where should THIS agent put THIS item? An argmin over (travel + ownership), among containers that
// will physically take it. -1 is a legitimate answer the caller must handle, and tn_store_tick()
// handles it by putting the thing on the floor.
//
// This is the ownership- and capacity-aware answer that the contract's tn_find_store() promises
// ("nearest store accepting its tag THAT THE HOUSEHOLD MAY USE") and that offer.h does not yet
// deliver. Case S5 asserts that gap as current behaviour; REPORT has the one-line fix.
int tn_store_pick(int agent, int item) {
    if (item < 0 || item >= tn_item_n) return -1;
    const TnTag tag = (TnTag)tn_item[item].store_tag;
    int best = -1, best_cost = 0;
    for (int o = 0; o < tn_obj_n; o++) {
        if (o > TNS_IDX_MAX) break;                        // TnItem.stored_in cannot address it
        if (!tn_store_accepts(o, item)) continue;
        const int cost = tns_dist(tn_agent[agent].tx, tn_agent[agent].ty, tn_obj[o].tx, tn_obj[o].ty)
                       + tn_ownership_penalty(agent, o, tag);
        if (best < 0 || cost < best_cost) { best = o; best_cost = cost; }
    }
    return best;
}

// ── items ───────────────────────────────────────────────────────────────────

// A new thing, loose on the floor. The primitive: the catalogue spawner below and a work order's
// output (TnRecipe.out_store_tag / out_value) both come through here, so an item produced by work
// and an item placed by a scenario are the same kind of object.
int tn_item_new(int store_tag, int value, int tx, int ty) {
    if (tn_item_n >= TN_MAX_ITEMS) return -1;
    tn_item[tn_item_n] = (TnItem){ (unsigned char)store_tag, (unsigned char)value, -1, -1,
                                   (unsigned char)tx, (unsigned char)ty };
    return tn_item_n++;
}

// A row of the catalogue, loose on the floor.
int tn_item_spawn(int def, int tx, int ty) {
    if (def < 0 || def >= TNS_ITEM_N) return -1;
    return tn_item_new(TNS_ITEMS[def].store_tag, TNS_ITEMS[def].value, tx, ty);
}

const char *tn_item_name(int item) {                       // HUD sugar. Matched on by nobody.
    if (item < 0 || item >= tn_item_n) return "-";
    for (int d = 0; d < TNS_ITEM_N; d++)
        if (TNS_ITEMS[d].store_tag == tn_item[item].store_tag &&
            TNS_ITEMS[d].value     == tn_item[item].value) return TNS_ITEMS[d].name;
    return "thing";
}

// Floor (or a container) → hands. One pair of hands, no stack sizes in v1.
bool tn_item_pick_up(int agent, int item) {
    if (agent < 0 || agent >= tn_agent_n) return false;
    if (item < 0 || item >= tn_item_n || item > TNS_IDX_MAX) return false;   // see TNS_IDX_MAX
    if (tn_agent[agent].carrying >= 0) return false;
    if (tn_item[item].held_by >= 0) return false;          // somebody got there first. No locking
                                                           // needed in v1: whoever arrives, wins
    tn_item[item].stored_in = -1;
    tn_item[item].held_by   = (signed char)agent;
    tn_agent[agent].carrying = (signed char)item;
    return true;
}

// Hands → container. Refuses rather than overfilling or mis-filing.
bool tn_item_put(int agent, int item, int obj) {
    if (agent < 0 || agent >= tn_agent_n) return false;
    if (item < 0 || item >= tn_item_n || obj < 0 || obj >= tn_obj_n) return false;
    if (obj > TNS_IDX_MAX) return false;                   // see TNS_IDX_MAX
    if (tn_agent[agent].carrying != item) return false;
    if (!tn_store_accepts(obj, item)) return false;
    tn_item[item].held_by   = -1;
    tn_item[item].stored_in = (signed char)obj;
    // tx/ty are "valid only when loose" per the contract; parking them on the container's tile
    // costs nothing and lets art draw a stored item without a special case.
    tn_item[item].tx = tn_obj[obj].tx; tn_item[item].ty = tn_obj[obj].ty;
    tn_agent[agent].carrying = -1;
    return true;
}

// Hands → floor, where you stand. The honest answer when there is nowhere to put a thing.
void tn_item_drop(int agent) {
    if (agent < 0 || agent >= tn_agent_n) return;
    const int it = tn_agent[agent].carrying;
    if (it < 0 || it >= tn_item_n) { tn_agent[agent].carrying = -1; return; }
    tn_item[it].held_by = -1; tn_item[it].stored_in = -1;
    tn_item[it].tx = (unsigned char)tn_agent[agent].tx;
    tn_item[it].ty = (unsigned char)tn_agent[agent].ty;
    tn_agent[agent].carrying = -1;
}

// ── hauling ─────────────────────────────────────────────────────────────────
// The fetch target, +1 so that a zeroed array means "no job" without an init pass. Module-private,
// so the contract needs no new field: this is the one piece of state hauling adds.
static int tns_fetch1[TN_MAX_AGENTS];

// The nearest LOOSE item this agent could usefully shelve, or -1. "Usefully" = somewhere exists to
// put it, so nobody walks across the building to pick up a thing it would only stand holding.
static int tns_loose_for(int agent) {
    int best = -1, best_d = 0;
    for (int i = 0; i < tn_item_n; i++) {
        if (i > TNS_IDX_MAX) break;                        // TnAgent.carrying cannot address it
        if (tn_item[i].held_by >= 0 || tn_item[i].stored_in >= 0) continue;
        if (tn_store_pick(agent, i) < 0) continue;
        const int d = tns_dist(tn_agent[agent].tx, tn_agent[agent].ty, tn_item[i].tx, tn_item[i].ty);
        if (best < 0 || d < best_d) { best = i; best_d = d; }
    }
    return best;
}

// One tile a minute, x before y — the same gait agents.h walks with, deliberately duplicated rather
// than shared, because the alternative is this module reaching into a sibling's internals. Returns
// true once the agent is on or beside the target tile (the same arrival test agents.h uses).
static bool tns_step_to(int agent, int tx, int ty) {
    TnAgent *a = &tn_agent[agent];
    if (a->tx != tx) a->tx += (a->tx < tx) ? 1 : -1;
    else if (a->ty != ty) a->ty += (a->ty < ty) ? 1 : -1;
    a->facing = (unsigned char)((a->tx < tx) ? 1 : (a->tx > tx) ? 3 : (a->ty < ty) ? 2 : 0);
    return abs(a->tx - tx) + abs(a->ty - ty) <= 1;
}

// THE PERSON GOES TO THE STORE, NEVER THE STORE TO THE PERSON (design §6, and navkit's note that
// DF's carry-the-container-to-the-item pattern is where the locking and reservation trouble lives).
//
// Runs AFTER tn_agents_tick() in the cart's tick order, which is what gives hauling the last word
// over a person who is already holding something: carrying a thing outranks wanting a thing, so an
// agent puts its load down before it goes to bed. Two things it will not do:
//   • interrupt a USE — agents.h decrements the object's `users` only when the use COMPLETES, so
//     stealing an agent mid-use would leak a phantom occupant and quietly close the object forever
//   • start a fetch when there is anything on offer. Needs outrank tidying, which is the only
//     priority rule in this module and it is a stopgap: see REPORT, "hauling should be a bid".
void tn_store_tick(void) {
    for (int i = 0; i < tn_agent_n; i++) {
        TnAgent *a = &tn_agent[i];
        if (a->activity == TN_ACT_USE || a->activity == TN_ACT_OFF_LOT) continue;

        if (a->carrying < 0) {
            // ── FETCH: walk to a loose item and pick it up ──
            int want = tns_fetch1[i] - 1;
            if (want >= tn_item_n) want = -1;                                  // world was rebuilt
            if (want >= 0 && (tn_item[want].held_by >= 0 || tn_item[want].stored_in >= 0))
                want = -1;                                                     // somebody else got it
            if (want < 0 && a->activity != TN_ACT_IDLE) { tns_fetch1[i] = 0; continue; }
            if (want < 0) want = tns_loose_for(i);
            if (want < 0) { tns_fetch1[i] = 0; continue; }
            tns_fetch1[i] = want + 1;
            a->activity = TN_ACT_HAUL; a->target_obj = -1;
            a->bid_tag = tn_item[want].store_tag;          // the HUD shows WHY it is walking (§1)
            a->bid_score = 0;
            if (tns_step_to(i, tn_item[want].tx, tn_item[want].ty) && !tn_item_pick_up(i, want)) {
                tns_fetch1[i] = 0; a->activity = TN_ACT_IDLE;
            }
            continue;                                      // carry on next minute
        }

        // ── HAUL: carry it to the best container going ──
        tns_fetch1[i] = 0;
        const int it = a->carrying;
        const int dest = tn_store_pick(i, it);
        if (dest < 0) { tn_item_drop(i); a->activity = TN_ACT_IDLE; continue; }
        a->activity = TN_ACT_HAUL;
        a->target_obj = (signed char)dest;
        a->bid_tag = tn_item[it].store_tag;
        a->bid_score = 0;
        tn_item[it].tx = (unsigned char)a->tx;             // the thing travels with the person
        tn_item[it].ty = (unsigned char)a->ty;
        if (tns_step_to(i, tn_obj[dest].tx, tn_obj[dest].ty)) {
            if (!tn_item_put(i, it, dest)) tn_item_drop(i);  // filled up while we walked
            a->activity = TN_ACT_IDLE;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SPEC — the module's own assertions, per runtime/spec.h "SPECS ON AN INCLUDEABLE". A shared header
// cannot define spec() (one per cart), so it exposes a selfcheck the cart's spec() calls:
//
//     #ifdef DE_SPEC ... tn_store_selfcheck(); ... #endif      ← one line in tools/carts/tenement.c
//
// Every case is written to be able to FAIL: each has a converse, a number that would come out wrong
// if a constant were wrong, or an invariant checked after a long run of the real building.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DE_SPEC
void tn_store_selfcheck(void);

static char tns_sp[200];

static void tns_spec_reset(void) {                         // an empty building, and no stale jobs
    tn_world_init();
    tn_obj_n = 0; tn_agent_n = 0; tn_item_n = 0;
    for (int i = 0; i < TN_MAX_AGENTS; i++) tns_fetch1[i] = 0;
}
// Scenario sugar: put `n` items straight into a container, bypassing the walk. The API path itself
// is proved by S3; this is only for building a "full fridge" to choose against.
static void tns_spec_fill(int obj, int store_tag, int n) {
    for (int i = 0; i < n; i++) {
        const int it = tn_item_new(store_tag, 1, tn_obj[obj].tx, tn_obj[obj].ty);
        if (it >= 0 && it <= TNS_IDX_MAX) tn_item[it].stored_in = (signed char)obj;
    }
}
// The store offer's capacity on an object, or 0. For the "never overfilled" invariant.
static int tns_spec_cap(int obj) {
    const int kind = tn_obj[obj].kind; int cap = 0;
    for (int i = 0; i < TN_OFFER_N[kind]; i++)
        if (TN_OFFERS[kind][i].tag >= TN_SERVE_COUNT && TN_OFFERS[kind][i].strength < 0 &&
            TN_OFFERS[kind][i].capacity > cap) cap = TN_OFFERS[kind][i].capacity;
    return cap;
}

void tn_store_selfcheck(void) {
    // ── S1: A CONTAINER TAKES AN ITEM BECAUSE OF A TAG ──────────────────────
    // Nothing in the path from "holding a meal" to "meal in the fridge" names a fridge or a meal.
    tns_spec_reset();
    {
        const int fridge = tn_add_obj(TN_OBJ_FRIDGE, 1, 1, 0);
        const int who    = tn_add_agent(0, 1, 3);
        const int meal   = tn_item_new(TN_STORE_FOOD, 4, 1, 3);
        expect(tn_item_pick_up(who, meal) && tn_agent[who].carrying == meal,
               "store S1: an item can be picked up, and the agent is holding exactly it");
        for (int t = 0; t < 12; t++) tn_store_tick();
        snprintf(tns_sp, sizeof tns_sp,
                 "store S1: a carried item ends up inside the container that OFFERS its class "
                 "(stored_in %d, fridge %d, %d inside)",
                 tn_item[meal].stored_in, fridge, tn_store_count(fridge));
        expect(tn_item[meal].stored_in == fridge && tn_item[meal].held_by == -1 &&
               tn_agent[who].carrying == -1 && tn_store_count(fridge) == 1, tns_sp);
    }

    // ── S2: THE PERSON GOES TO THE STORE, NEVER THE STORE TO THE PERSON ─────
    tns_spec_reset();
    {
        const int far = tn_add_obj(TN_OBJ_FRIDGE, 10, 7, 0);
        const int who = tn_add_agent(0, 1, 1);
        const int it  = tn_item_new(TN_STORE_FOOD, 4, 1, 1);
        tn_item_pick_up(who, it);
        const int ox = tn_obj[far].tx, oy = tn_obj[far].ty, ax = tn_agent[who].tx;
        for (int t = 0; t < 40; t++) tn_store_tick();
        expect(tn_obj[far].tx == ox && tn_obj[far].ty == oy && tn_agent[who].tx != ax,
               "store S2: the PERSON walked the length of the building; the container never moved");
        expect(tn_item[it].stored_in == far,
               "store S2: and the item is inside it at the end of the walk");
    }

    // ── S3: CAPACITY, AND A FULL STORE IS NOT A STORE ───────────────────────
    tns_spec_reset();
    {
        const int fr  = tn_add_obj(TN_OBJ_FRIDGE, 1, 1, 0);
        const int who = tn_add_agent(0, 1, 2);
        const int cap = tn_store_room(fr, TN_STORE_FOOD);          // == the offer's capacity, empty
        for (int i = 0; i < cap; i++) {                            // fill it through the real API
            const int it = tn_item_new(TN_STORE_FOOD, 1, 1, 2);
            tn_item_pick_up(who, it); tn_item_put(who, it, fr);
        }
        snprintf(tns_sp, sizeof tns_sp,
                 "store S3: a container fills to exactly its offer capacity (%d) and no further", cap);
        expect(cap > 1 && tn_store_count(fr) == cap && tn_store_room(fr, TN_STORE_FOOD) == 0, tns_sp);

        const int extra = tn_item_new(TN_STORE_FOOD, 1, 1, 2);
        tn_item_pick_up(who, extra);
        expect(tn_store_pick(who, extra) == -1,
               "store S3: a FULL container is not a store: there is nowhere to put the next one");
        expect(tn_item_put(who, extra, fr) == false && tn_store_count(fr) == cap,
               "store S3: and the put is refused rather than overfilling");
        for (int t = 0; t < 4; t++) tn_store_tick();
        expect(tn_item[extra].stored_in == -1 && tn_item[extra].held_by == -1 &&
               tn_agent[who].carrying == -1,
               "store S3: with nowhere to go it goes on the FLOOR, not carried around forever");
    }

    // ── S4: OWNERSHIP DECIDES WHERE A THING GOES ────────────────────────────
    // The headline, on the side this module can wire on its own. Three directions, so it cannot
    // pass by always preferring one answer.
    tns_spec_reset();
    {
        const int nb  = tn_add_obj(TN_OBJ_FRIDGE,  1, 1,  0);   // the NEIGHBOUR's, one tile away
        const int own = tn_add_obj(TN_OBJ_FRIDGE, 10, 7,  1);   // this household's, right across
        const int who = tn_add_agent(1, 1, 2);
        const int it  = tn_item_new(TN_STORE_FOOD, 4, 1, 2);
        tn_item_pick_up(who, it);
        snprintf(tns_sp, sizeof tns_sp,
                 "store S4: ownership counts — a tenant crosses the building to its OWN container "
                 "(%d) rather than use the neighbour's next door (%d)", own, nb);
        expect(tn_store_pick(who, it) == own, tns_sp);

        tns_spec_fill(own, TN_STORE_FOOD, tn_store_room(own, TN_STORE_FOOD));
        expect(tn_store_pick(who, it) == nb,
               "store S4: with its own full, the neighbour's beats the floor. Ownership is a COST, "
               "not a wall — which is what keeps the comedy in §6 possible");

        const int comm = tn_add_obj(TN_OBJ_FRIDGE, 5, 5, -1);   // communal
        expect(tn_store_pick(who, it) == comm,
               "store S4: a communal container (household -1) is everybody's, so it outranks the "
               "neighbour's even from further away");
    }

    // ── S5: THE OWNERSHIP TERM, AND THE FLIP CASE 8 IS WAITING FOR ──────────
    // The need side of ownership lives in offer.h's tno_score, which this module does not own. What
    // CAN be pinned here is the term itself, and the arithmetic of the choice it would make: two
    // fridges differ only by travel (same deficit, same strength, both free), so the ordering is
    // exactly "distance + detour", and these numbers are the flip.
    tns_spec_reset();
    {
        const int f0  = tn_add_obj(TN_OBJ_FRIDGE,  1, 1,  0);   // household 0's
        const int f1  = tn_add_obj(TN_OBJ_FRIDGE, 11, 3,  1);   // household 1's
        const int fc  = tn_add_obj(TN_OBJ_FRIDGE, 11, 7, -1);   // communal
        const int who = tn_add_agent(1, 1, 2);                  // an h1 tenant, stood at f0
        for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[who].need[n] = 255;

        tn_agent[who].need[TN_SERVE_HUNGER] = 200;              // peckish, not desperate
        expect(tn_ownership_penalty(who, f1, TN_SERVE_HUNGER) == 0,
               "store S5: your own things cost no detour");
        expect(tn_ownership_penalty(who, fc, TN_SERVE_HUNGER) == 0,
               "store S5: communal things cost no detour either");
        expect(tn_ownership_penalty(who, f0, TN_SERVE_HUNGER) > 0,
               "store S5: the neighbour's fridge costs something");
        tn_agent[who].need[TN_SERVE_HUNGER] = 0;                // bottomed out
        expect(tn_ownership_penalty(who, f0, TN_SERVE_HUNGER) == 0,
               "store S5: and a bottomed-out need pays NO detour — the desperate tenant raids the "
               "neighbour's fridge, which is design §6's comedy rather than a bug");

        const int d0 = tns_dist(tn_agent[who].tx, tn_agent[who].ty, tn_obj[f0].tx, tn_obj[f0].ty);
        const int d1 = tns_dist(tn_agent[who].tx, tn_agent[who].ty, tn_obj[f1].tx, tn_obj[f1].ty);
        tn_agent[who].need[TN_SERVE_HUNGER] = 200;
        snprintf(tns_sp, sizeof tns_sp,
                 "store S5: comfortable, the detour outweighs the walk home (%d+%d vs %d), so the "
                 "term is big enough to flip case 8", d0,
                 tn_ownership_penalty(who, f0, TN_SERVE_HUNGER), d1);
        expect(d0 + tn_ownership_penalty(who, f0, TN_SERVE_HUNGER) > d1, tns_sp);
        tn_agent[who].need[TN_SERVE_HUNGER] = 0;
        expect(d0 + tn_ownership_penalty(who, f0, TN_SERVE_HUNGER) < d1,
               "store S5: desperate, the detour collapses and the near fridge wins again. BOTH "
               "directions, so the term is a gradient and not a ban");

        // GAP NOW CLOSED, and this assertion was flipped by the integrator, deliberately, which is
        // what "flip when the one-line fix lands" meant. tn_find_store() used to rank by travel
        // alone and hand an h1 tenant the h0 fridge one tile away, contradicting its own contract
        // comment ("that the household may use"). It now delegates to tn_store_pick, so the two
        // MUST agree: one API, one answer.
        const int it = tn_item_new(TN_STORE_FOOD, 4, 1, 2);
        tn_item_pick_up(who, it);
        snprintf(tns_sp, sizeof tns_sp,
                 "store S5: tn_find_store and tn_store_pick now agree (%d), because the former "
                 "delegates to the latter instead of ranking by travel alone",
                 tn_store_pick(who, it));
        expect(tn_find_store(who, it) == tn_store_pick(who, it), tns_sp);
    }

    // ── S6: A NEW ITEM IS A TABLE ROW, NOT A CODE PATH ──────────────────────
    // The strongest claim in the module. The loop never asks what an item is; it asks whether ANY
    // container offers that item's class and whether the placement agreed.
    tns_spec_reset();
    {
        const int fr  = tn_add_obj(TN_OBJ_FRIDGE,   1, 1, 0);
        (void)               tn_add_obj(TN_OBJ_WARDROBE, 3, 1, 0);
        const int who = tn_add_agent(0, 2, 2);
        int mismatched = 0, placed = 0, homeless = 0;
        for (int d = 0; d < TNS_ITEM_N; d++) {
            const int it = tn_item_spawn(d, 2, 2);
            tn_item_pick_up(who, it);
            const int dest = tn_store_pick(who, it);
            int somewhere = 0;
            for (int o = 0; o < tn_obj_n; o++)
                if (tn_offers(o, (TnTag)tn_item[it].store_tag, NULL)) somewhere = 1;
            if ((dest >= 0) != (somewhere != 0)) mismatched++;
            if (dest >= 0) { if (tn_item_put(who, it, dest)) placed++; else mismatched++; }
            else { homeless++; tn_item_drop(who); }
        }
        snprintf(tns_sp, sizeof tns_sp,
                 "store S6: every row of the item catalogue is filed by TAG alone — %d of %d placed, "
                 "%d honestly homeless (no container offers that class yet), 0 mismatches",
                 placed, TNS_ITEM_N, homeless);
        expect(mismatched == 0 && placed > 0, tns_sp);

        // THE PROOF: the last row was appended after this module was written and no line of code
        // anywhere names it. It goes in the fridge because it carries TN_STORE_FOOD, full stop.
        const int newby = tn_item_spawn(TNS_ITEM_N - 1, 2, 2);
        tn_item_pick_up(who, newby);
        for (int t = 0; t < 12; t++) tn_store_tick();
        snprintf(tns_sp, sizeof tns_sp,
                 "store S6: a NEW item ('%s', added as a table row) is accepted by an EXISTING "
                 "container with no code change anywhere", TNS_ITEMS[TNS_ITEM_N - 1].name);
        expect(tn_item[newby].stored_in == fr &&
               tn_offers(fr, (TnTag)tn_item[newby].store_tag, NULL), tns_sp);
    }

    // ── S7: THE REAL BUILDING, 900 MINUTES, NOTHING CORRUPTED ───────────────
    // Hand every resident something to put away (which is what a finished work module will do) and
    // run the whole tick alongside needs. Then check the invariants that a bookkeeping bug breaks.
    tns_spec_reset();
    tn_world_init();
    {
        int mine[TN_MAX_AGENTS];
        for (int i = 0; i < tn_agent_n; i++) {
            mine[i] = tn_item_new(TN_STORE_FOOD, 4, tn_agent[i].tx, tn_agent[i].ty);
            tn_item_pick_up(i, mine[i]);
        }
        for (int t = 0; t < 900; t++) { tn_agents_tick(); tn_store_tick(); }

        int both = 0, wrong_home = 0, over = 0, wrong_owner = 0, still_held = 0, stored = 0;
        for (int i = 0; i < tn_item_n; i++) {
            if (tn_item[i].held_by >= 0 && tn_item[i].stored_in >= 0) both++;
            if (tn_item[i].held_by >= 0) still_held++;
            if (tn_item[i].stored_in >= 0) {
                stored++;
                if (!tn_offers(tn_item[i].stored_in, (TnTag)tn_item[i].store_tag, NULL)) wrong_home++;
            }
        }
        for (int o = 0; o < tn_obj_n; o++) if (tn_store_count(o) > tns_spec_cap(o)) over++;
        for (int i = 0; i < tn_agent_n; i++) {
            const int in = tn_item[mine[i]].stored_in;
            if (in < 0) continue;
            const int owner = tn_obj[in].household;
            if (owner >= 0 && owner != (int)tn_agent[i].household) wrong_owner++;
        }
        expect(both == 0, "store S7: no item is ever in two places at once (held AND stored)");
        expect(wrong_home == 0,
               "store S7: nothing ever lands in a container that does not offer its class");
        expect(over == 0, "store S7: no container is ever over its capacity");
        snprintf(tns_sp, sizeof tns_sp,
                 "store S7: after 900 minutes of the real building every resident had put its load "
                 "away (%d stored, %d still in hand)", stored, still_held);
        expect(stored == tn_agent_n && still_held == 0, tns_sp);
        expect(wrong_owner == 0,
               "store S7: and every one of them used its OWN household's container, not the "
               "neighbour's — ownership working in the actual level, not just in a fixture");
    }

    // ── S8: HAULING FILLS THE GAPS, IT DOES NOT COMPETE WITH NEEDS ──────────
    tns_spec_reset();
    {
        const int fr  = tn_add_obj(TN_OBJ_FRIDGE, 1, 1, 0);
        const int who = tn_add_agent(0, 4, 4);
        for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[who].need[n] = 255;   // nothing on offer
        tn_clock.minute = 481;                                  // clear of the hourly decay tick
        const int loose = tn_item_new(TN_STORE_FOOD, 4, 6, 4);
        for (int t = 0; t < 40; t++) { tn_agents_tick(); tn_store_tick(); }
        expect(tn_item[loose].stored_in == fr,
               "store S8: an agent with nothing on offer fetches a loose item and shelves it");
    }
    tns_spec_reset();
    {
        (void) tn_add_obj(TN_OBJ_FRIDGE, 1, 1, 0);
        const int who = tn_add_agent(0, 4, 4);
        for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[who].need[n] = 255;
        tn_agent[who].need[TN_SERVE_HUNGER] = 10;               // now there IS something on offer
        const int loose = tn_item_new(TN_STORE_FOOD, 4, 5, 4);  // and a job at its feet
        tn_agents_tick(); tn_store_tick();
        expect(tn_agent[who].carrying == -1 && tn_item[loose].held_by == -1,
               "store S8: needs outrank tidying — a hungry agent leaves the item where it is");
    }

    // ── S9: THE CONTRACT'S signed char INDEX CEILING, GUARDED NOT WRAPPED ───
    // TnAgent.carrying is a signed char and TN_MAX_ITEMS is 256, so item 128 cannot be held: the
    // index would wrap NEGATIVE, and negative already means "loose on the floor". Asserted so the
    // ceiling is a work item and not folklore (REPORT has the contract fix).
    tns_spec_reset();
    {
        (void) tn_add_obj(TN_OBJ_FRIDGE, 1, 1, 0);
        const int who = tn_add_agent(0, 1, 2);
        while (tn_item_n <= TNS_IDX_MAX) (void) tn_item_new(TN_STORE_FOOD, 1, 1, 2);
        const int unreachable = tn_item_new(TN_STORE_FOOD, 1, 1, 2);
        snprintf(tns_sp, sizeof tns_sp,
                 "store S9: item %d cannot be carried and is refused, not wrapped negative "
                 "(signed char index vs TN_MAX_ITEMS %d)", unreachable, TN_MAX_ITEMS);
        expect(unreachable == TNS_IDX_MAX + 1 && tn_item_pick_up(who, unreachable) == false &&
               tn_agent[who].carrying == -1, tns_sp);
    }

    tn_world_init();                                            // leave the world as we found it
    for (int i = 0; i < TN_MAX_AGENTS; i++) tns_fetch1[i] = 0;
}
#endif // DE_SPEC

// ─────────────────────────────────────────────────────────────────────────────
// REPORT — what this module needs from files it does not own. Nothing here has been edited.
//
// 1. offer.h, THE OWNERSHIP TERM (the headline; three lines). In tno_score(), ownership joins the
//    score as a term in the SAME UNIT as travel:
//
//        -    return deficit * of->strength / (tno_travel(agent, obj) + 1 + queue);
//        +    const int detour = tn_ownership_penalty(agent, obj, tag);   // store.h owns the rule
//        +    return deficit * of->strength / (tno_travel(agent, obj) + 1 + detour + queue);
//
//    and, because offer.h is included BEFORE this file, ONE declaration in model.h beside the other
//    offer-index entry points:
//
//        int tn_ownership_penalty(int agent, int obj, TnTag tag);   // owner: store
//
//    (Alternative if the contract is to stay untouched: move `#include "tenement/store.h"` above
//    `#include "tenement/offer.h"` in tools/carts/tenement.c. This module is written to allow that —
//    it touches no tno_* internals.)
//
// 2. offer.h, tn_find_store() (one line). It currently ranks by travel alone and never asks whether
//    a container is FULL, so it contradicts its own contract comment ("nearest store accepting its
//    tag that the household may use"):
//
//        -    return tn_best_offer(agent, (TnTag)tn_item[item].store_tag, NULL);
//        +    return tn_store_pick(agent, item);
//
//    Then spec case S5's "KNOWN GAP" assertion must be flipped to `tn_find_store(who, it) == f1`.
//
// 3. tools/carts/tenement.c, spec case 8 — NOT flipped by adding the term, and worth knowing why.
//    Case 8's building holds exactly one object, so the h1 tenant's only options are "h0's fridge"
//    or "nothing at all"; a penalty cannot change an argmax with one candidate, and the case would
//    keep passing while ownership silently worked. To make it the test it wants to be it needs a
//    second fridge and both directions:
//
//        tn_add_obj(TN_OBJ_FRIDGE, 1, 1, 0);        // the neighbour's, one tile away
//        tn_add_obj(TN_OBJ_FRIDGE, 11, 3, 1);       // its OWN, across the building
//        tn_add_agent(1, 1, 2);
//        need[TN_SERVE_HUNGER] = 200;  → tn_best_action picks obj 1 (walks home)
//        need[TN_SERVE_HUNGER] = 0;    → tn_best_action picks obj 0 (raids the neighbour)
//
//    A hard ban would flip case 8 as written, and it is the wrong fix twice over: it kills §6's
//    comedy, and it re-introduces the pre-filter that §2/§10 spent the whole slice removing.
//
// 4. offer.h, ONE MISSING TABLE ROW (not a code path). Nothing in TN_OFFERS offers TN_STORE_GOODS,
//    which is exactly what TN_RECIPES[0] produces, so the loom's output has nowhere to live and
//    lands on the floor. A crate/shelf kind offering { TN_STORE_GOODS, -1, 0, 8 }, or that row added
//    to TN_OBJ_COUNTER, closes it. Case S6 counts it as "honestly homeless" rather than asserting
//    its absence, so adding the row does not fail this module's spec.
//
// 5. model.h, the signed char INDEX CEILING (a real latent bug, guarded here, case S9). TnItem
//    .held_by/.stored_in and TnAgent.carrying/.target_obj are `signed char` while TN_MAX_ITEMS is
//    256 and TN_MAX_OBJECTS is 192. Index 128+ wraps NEGATIVE and negative MEANS "none", so an item
//    would appear to teleport to the floor with no error. Fix: make those four fields `short`
//    (agents.h's target_obj assignment is already a cast, so nothing else changes).
//
// 6. model.h, DECLARATIONS so work.h and econ.h can use this module (they are included before it):
//        int  tn_item_new(int store_tag, int value, int tx, int ty);   // owner: store
//        int  tn_item_spawn(int def, int tx, int ty);
//        bool tn_item_pick_up(int agent, int item);
//        bool tn_item_put(int agent, int item, int obj);
//        void tn_item_drop(int agent);
//        int  tn_store_pick(int agent, int item);
//        int  tn_store_room(int obj, TnTag tag);
//        int  tn_store_count(int obj);
//        bool tn_store_accepts(int obj, int item);
//        const char *tn_item_name(int item);
//    A work order finishing a shift is then `tn_item_pick_up(a, tn_item_new(r->out_store_tag,
//    r->out_value, a->tx, a->ty))` and this module hauls it away with no further wiring.
//
// 7. OPEN, and the one place this module cheats: HAULING SHOULD BE A BID. An agent only fetches a
//    loose item when tn_best_action() offers it nothing, which is the single hand-written priority
//    in here and it is against the grain of §2. In practice needs are almost never all sated, so
//    autonomous tidying will rarely fire in the running game. The design-faithful fix is a
//    VOCABULARY extension, not a code path: a loose item advertises an offer (a TN_HAUL_* tag with a
//    strength), it enters the same argmax as everything else, and "shelving the dinner beats a
//    half-full fun need" becomes a number instead of an if. That also gives §8b's reclaimed hours
//    something to buy. Out of v1 as briefed, and the reason this note exists rather than a guess.
// ─────────────────────────────────────────────────────────────────────────────

#endif // TENEMENT_STORE_H
