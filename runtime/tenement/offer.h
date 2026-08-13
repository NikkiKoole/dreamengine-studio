// ─────────────────────────────────────────────────────────────────────────────
// tenement/offer.h — the OFFER INDEX: one argmax, three consumers. THE core of the design.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tno_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_OFFER_H
#define TENEMENT_OFFER_H

const TnOffer TN_OFFERS[TN_OBJ_KIND_COUNT][TN_MAX_OFFERS] = {
    [TN_OBJ_BED]      = { { TN_SERVE_REST,    120, 480, 1, TN_POSE_LIE } },   // 8h; you LIE on a bed
    [TN_OBJ_FRIDGE]   = { { TN_SERVE_HUNGER,  100,  30, 1, TN_POSE_STAND },
                          { TN_STORE_FOOD,     -1,   0, 8, TN_POSE_STAND } },
    [TN_OBJ_COUNTER]  = { { TN_SERVE_HUNGER,   40,  20, 1, TN_POSE_STAND },  // a snack, worse than the fridge
                          { TN_CAP_WORK,       -1,   0, 1, TN_POSE_STAND } },
    [TN_OBJ_TOILET]   = { { TN_SERVE_BLADDER, 110,  10, 1, TN_POSE_SIT } },
    [TN_OBJ_SOFA]     = { { TN_SERVE_FUN,      90,  60, 2, TN_POSE_SIT } },   // the only shareable thing
    [TN_OBJ_LOOM]     = { { TN_CAP_WORK,       -1, 480, 1, TN_POSE_STAND } },  // the dumb machine, §4
    [TN_OBJ_WARDROBE] = { { TN_STORE_CLOTHES,  -1,   0, 6, TN_POSE_STAND } },
};
const unsigned char TN_OFFER_N[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = 1, [TN_OBJ_FRIDGE] = 2, [TN_OBJ_COUNTER] = 2, [TN_OBJ_TOILET] = 1,
    [TN_OBJ_SOFA] = 1, [TN_OBJ_LOOM] = 1, [TN_OBJ_WARDROBE] = 1,
};
const short TN_OBJ_PRICE[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = 120, [TN_OBJ_FRIDGE] = 200, [TN_OBJ_COUNTER] = 60, [TN_OBJ_TOILET] = 90,
    [TN_OBJ_SOFA] = 140, [TN_OBJ_LOOM] = 300, [TN_OBJ_WARDROBE] = 80,
};

// Footprint in TILES (see model.h). Derived from tools/voxel-models/tenement.js at 6 voxels per
// tile, but stated here as sim data so nothing has to reach into the art to find out what a bed
// covers. A mismatch with the models is a legitimate bug for a critic to look for.
const unsigned char TN_OBJ_FOOTPRINT[TN_OBJ_KIND_COUNT][2] = {
    [TN_OBJ_BED]      = { 1, 2 },   // 6x12 voxels
    [TN_OBJ_FRIDGE]   = { 1, 1 },
    [TN_OBJ_COUNTER]  = { 1, 1 },
    [TN_OBJ_TOILET]   = { 1, 1 },
    [TN_OBJ_SOFA]     = { 2, 1 },   // 12x6 voxels
    [TN_OBJ_LOOM]     = { 1, 1 },
    [TN_OBJ_WARDROBE] = { 1, 1 },
};

// v1 recipes: a dumb machine turns TIME into a GOOD. in_n == 0 is the marked open loop (design §5).
const TnRecipe TN_RECIPES[] = {
    { TN_CAP_WORK, 0, 0, TN_STORE_GOODS, 12, 480 },
};
const int TN_RECIPE_N = 1;

bool tn_offers(int obj, TnTag tag, int *strength) {
    const int kind = tn_obj[obj].kind;
    for (int i = 0; i < TN_OFFER_N[kind]; i++) {
        if (TN_OFFERS[kind][i].tag == (unsigned char)tag) {
            if (strength) *strength = TN_OFFERS[kind][i].strength;
            return true;
        }
    }
    return false;
}

static const TnOffer *tno_offer_of(int obj, TnTag tag) {
    const int kind = tn_obj[obj].kind;
    for (int i = 0; i < TN_OFFER_N[kind]; i++)
        if (TN_OFFERS[kind][i].tag == (unsigned char)tag) return &TN_OFFERS[kind][i];
    return NULL;
}

static int tno_travel(int agent, int obj) {
    // A REAL WALK over world's edge walls, not a crow's flight. The straight line was the biggest
    // lie in the simulation: it made corridors and party walls free, so the design's central claim
    // (a badly planned building shows up as a traffic pattern) could not be judged at all.
    //
    // It costs nothing measurable. path floods ONCE per agent and answers every object from the
    // same field, and the per-object loop here already costs more than the flood: 0.133 -> 0.138
    // ms/tick at the arrays' worst case, 0.002 on today's building. One caveat, and it is why this
    // stays a one-liner: the field cache is keyed on the source tile, so callers must stay
    // AGENT-major. tn_best_action is. An object-major caller re-floods per query and measures 138x
    // worse.
    //
    // Unreachable comes back as TN_UNREACHABLE, a large number, never -1: this function's result is
    // a DIVISOR below, so a negative would be far worse than a big one. At 100000 an unreachable
    // object scores 0 and simply never wins, with no guard needed at any call site.
    return tn_path_len(tn_agent[agent].tx, tn_agent[agent].ty, tn_obj[obj].tx, tn_obj[obj].ty);
}

// HOW LONG UNTIL THIS OBJECT IS FREE, in minutes. DERIVED from the sitting users' own `until`
// rather than stored on the object, so there is exactly one truth about when an occupation ends
// and no field to forget to clear.
//
// Occupied by nobody we can find (spec fakes `users` by hand, or a user was destroyed) comes back
// as TN_UNREACHABLE, which is the honest answer: an occupation with no owner has no end. It also
// makes this a strict superset of the QUEUE_FULL wall it replaces, so the old assertions still
// mean what they said.
static int tno_free_in(int obj) {
    int soonest = TN_UNREACHABLE;
    for (int a = 0; a < tn_agent_n; a++) {
        const TnAgent *g = &tn_agent[a];
        if (g->activity != TN_ACT_USE || g->target_obj != obj) continue;
        const int left = g->until - tn_now();
        if (left < soonest) soonest = left < 0 ? 0 : left;
    }
    return soonest;
}

// THE SCORE, in one place so no module invents its own (contract, offer-index block).
//     deficit * strength / (travel + queue)
// Every term a number, which is what makes the choice oracle-able.
static int tno_score(int agent, int obj, const TnOffer *of, TnTag tag) {
    if (of->strength < 0) return -1;                       // capability/storage: not a need bid
    const int deficit = 255 - tn_agent[agent].need[tag];
    if (deficit <= 0) return -1;
    int queue = 0;
    // WAITING IS PRICED, NOT BANNED, and the unit falls out for free: an agent steps one tile per
    // tick and a tick is one minute, so a tile of walking and a minute of waiting are literally the
    // same quantity. `queue` is therefore just "how many more minutes until it's mine", added to
    // "how many tiles until I'm there".
    //
    // This replaces a flat QUEUE_FULL of 1000, which made any occupied capacity-1 object worth
    // approximately nothing. Measured over 1200 frames of the real building, that produced a
    // simulation where 99.6% of frames had somebody wanting a full object and 2.3% had anybody
    // standing at one: contention was continuous in the numbers and invisible on the screen,
    // because every collision resolved as a silent deflection before anyone moved. Design §1
    // promises "a queue you can see" and §4 promises "one loom, four tenants, and a queue"; the
    // score could not produce either, and §10 recorded the ban as a virtue.
    //
    // The good part is what the price decides for us, which no ban could express: whether a thing
    // forms a queue is now a property of HOW LONG IT TAKES. A toilet is a 10-minute offer, so its
    // worst wait is 10 tiles of walking and people queue for it. A bed is 480 and a loom is 480,
    // so waiting is never worth it and people deflect. Nobody wrote that rule down.
    if (tn_obj[obj].users >= of->capacity) queue = tno_free_in(obj);
    else queue = tn_obj[obj].users * 4;                    // sharing is worse than being alone
    // Ownership is a TERM, not a filter, and that distinction is design §6. A comfortable tenant
    // walks across the floor to its OWN fridge; a bottomed-out one raids the neighbour's. A hard ban
    // would kill the comedy the design is built on AND re-introduce exactly the pre-filter the
    // slice existed to remove. The penalty is in tiles so it lands in the same unit as travel.
    const int detour = tn_ownership_penalty(agent, obj, tag);
    return deficit * of->strength / (tno_travel(agent, obj) + 1 + detour + queue);
}

// ONE argmax over EVERY (object, need) pair. Not "most urgent need, then an object for it".
// See the contract's offer-index block for why that distinction is the entire design.
int tn_best_action(int agent, TnTag *out_tag, int *out_score) {
    int best = -1, best_score = 0; TnTag best_tag = TN_SERVE_COUNT;
    for (int o = 0; o < tn_obj_n; o++) {
        const int kind = tn_obj[o].kind;
        for (int i = 0; i < TN_OFFER_N[kind]; i++) {
            const TnOffer *of = &TN_OFFERS[kind][i];
            if (of->tag >= TN_SERVE_COUNT) continue;        // only needs bid for attention
            const int s = tno_score(agent, o, of, (TnTag)of->tag);
            if (s > best_score) { best_score = s; best = o; best_tag = (TnTag)of->tag; }
        }
    }
    if (out_tag)   *out_tag   = best_tag;
    if (out_score) *out_score = best_score;
    return best;
}

int tn_best_offer(int agent, TnTag tag, int *out_score) {
    int best = -1, best_score = 0;
    for (int o = 0; o < tn_obj_n; o++) {
        const TnOffer *of = tno_offer_of(o, tag);
        if (!of) continue;
        const int s = (of->strength < 0) ? (1000 / (tno_travel(agent, o) + 1))
                                        : tno_score(agent, o, of, tag);
        if (s > best_score) { best_score = s; best = o; }
    }
    if (out_score) *out_score = best_score;
    return best;
}

int tn_find_workspot(int agent, TnTag cap) { return tn_best_offer(agent, cap, NULL); }

int tn_find_store(int agent, int item) {
    // Delegates to store, which ranks by travel AND ownership AND asks whether the container will
    // physically take the thing. Ranking by travel alone (the slice's version) contradicted this
    // function's own contract comment: it would happily send someone to a full fridge.
    return tn_store_pick(agent, item);
}

void tn_sell(int household, int item) {                    // TN_SEAM_EXTERNAL (design §5)
    tn_house[household].money += tn_item[item].value;
}


// Public wrapper so the HUD can price any single offer (design §1: show the bids).
int tn_score_offer(int agent, int obj, TnTag tag) {
    const TnOffer *of = tno_offer_of(obj, tag);
    return of ? tno_score(agent, obj, of, tag) : -1;
}

#endif // TENEMENT_OFFER_H
