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
    [TN_OBJ_BED]      = { { TN_SERVE_REST,    120, 480, 1 } },   // 8 hours; short, not char
    [TN_OBJ_FRIDGE]   = { { TN_SERVE_HUNGER,  100,  30, 1 },
                          { TN_STORE_FOOD,     -1,   0, 8 } },
    [TN_OBJ_COUNTER]  = { { TN_SERVE_HUNGER,   40,  20, 1 },     // a snack, worse than the fridge
                          { TN_CAP_WORK,       -1,   0, 1 } },
    [TN_OBJ_TOILET]   = { { TN_SERVE_BLADDER, 110,  10, 1 } },
    [TN_OBJ_SOFA]     = { { TN_SERVE_FUN,      90,  60, 2 } },   // the only shareable thing here
    [TN_OBJ_LOOM]     = { { TN_CAP_WORK,       -1, 480, 1 } },   // the dumb machine, design §4
    [TN_OBJ_WARDROBE] = { { TN_STORE_CLOTHES,  -1,   0, 6 } },
};
const unsigned char TN_OFFER_N[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = 1, [TN_OBJ_FRIDGE] = 2, [TN_OBJ_COUNTER] = 2, [TN_OBJ_TOILET] = 1,
    [TN_OBJ_SOFA] = 1, [TN_OBJ_LOOM] = 1, [TN_OBJ_WARDROBE] = 1,
};
const short TN_OBJ_PRICE[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED] = 120, [TN_OBJ_FRIDGE] = 200, [TN_OBJ_COUNTER] = 60, [TN_OBJ_TOILET] = 90,
    [TN_OBJ_SOFA] = 140, [TN_OBJ_LOOM] = 300, [TN_OBJ_WARDROBE] = 80,
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

// Straight-line tile distance. A PATH would be correct; see de:meta.todo. It is enough to prove the
// decision mechanism, and swapping it for a path changes only this function.
static int tno_travel(int agent, int obj) {
    const int dx = tn_agent[agent].tx - tn_obj[obj].tx;
    const int dy = tn_agent[agent].ty - tn_obj[obj].ty;
    return (int)(sqrtf((float)(dx * dx + dy * dy)) + 0.5f);
}

// THE SCORE, in one place so no module invents its own (contract, offer-index block).
//     deficit * strength / (travel + queue)
// Every term a number, which is what makes the choice oracle-able.
#define QUEUE_FULL 1000                   // an occupied capacity-1 object is not worth waiting for
static int tno_score(int agent, int obj, const TnOffer *of, TnTag tag) {
    if (of->strength < 0) return -1;                       // capability/storage: not a need bid
    const int deficit = 255 - tn_agent[agent].need[tag];
    if (deficit <= 0) return -1;
    int queue = 0;
    if (tn_obj[obj].users >= of->capacity) queue = QUEUE_FULL;
    else queue = tn_obj[obj].users * 4;                    // sharing is worse than being alone
    return deficit * of->strength / (tno_travel(agent, obj) + 1 + queue);
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
    return tn_best_offer(agent, (TnTag)tn_item[item].store_tag, NULL);
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
