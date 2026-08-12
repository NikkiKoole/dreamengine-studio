// ─────────────────────────────────────────────────────────────────────────────
// tenement/world.h — the contract's globals, spawning, and the level. Owner of tn_obj/tn_item/tn_clock.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tnw_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_WORLD_H
#define TENEMENT_WORLD_H

TnObject    tn_obj[TN_MAX_OBJECTS];       int tn_obj_n;
TnItem      tn_item[TN_MAX_ITEMS];        int tn_item_n;
TnAgent     tn_agent[TN_MAX_AGENTS];      int tn_agent_n;
TnHousehold tn_house[TN_MAX_HOUSEHOLDS];  int tn_house_n;
TnOrder     tn_order[TN_MAX_ORDERS];      int tn_order_n;
TnClock     tn_clock;
int         tn_bw = 13, tn_bh = 9;   // sized so the diamond fits 320px: (13+9) * 12 = 264
int         tn_rot;

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
    return tn_agent_n - 1;
}

void tn_world_init(void) {
    tn_obj_n = tn_agent_n = tn_item_n = tn_order_n = 0;
    tn_house_n = 2;
    for (int h = 0; h < tn_house_n; h++) tn_house[h] = (TnHousehold){ 200, {0}, 0, 20, (unsigned char)h };
    // Household 0, left half. Household 1, right half. One communal loom in the middle: the
    // contention that design §1 is about starts the moment two households want one machine.
    tn_add_obj(TN_OBJ_BED,      1, 1, 0);  tn_add_obj(TN_OBJ_FRIDGE,  1, 3, 0);
    tn_add_obj(TN_OBJ_TOILET,   3, 1, 0);  tn_add_obj(TN_OBJ_SOFA,    1, 6, 0);
    tn_add_obj(TN_OBJ_WARDROBE, 3, 3, 0);
    tn_add_obj(TN_OBJ_BED,     11, 1, 1);  tn_add_obj(TN_OBJ_FRIDGE, 11, 3, 1);
    tn_add_obj(TN_OBJ_TOILET,   9, 1, 1);  tn_add_obj(TN_OBJ_COUNTER,11, 6, 1);
    tn_add_obj(TN_OBJ_LOOM,     6, 4, -1);
    tn_add_agent(0, 2, 2); tn_add_agent(0, 2, 5);
    tn_add_agent(1, 10, 2); tn_add_agent(1, 10, 5);
    tn_clock = (TnClock){ 8 * 60, 1 };
}

#endif // TENEMENT_WORLD_H
