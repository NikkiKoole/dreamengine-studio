// ─────────────────────────────────────────────────────────────────────────────
// tenement/work.h — work orders, the loom shift, goods, the sell seam (design §4/§5). STUB.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tnk_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_WORK_H
#define TENEMENT_WORK_H

// OWNER: the `work` agent. Everything here is yours.
//
// What v1 needs (design §4 and §5):
//   • post work orders against TN_RECIPES, whose recipes require a CAPABILITY and never name a place
//   • an agent claims an order, walks to tn_find_workspot(agent, recipe->needs_cap), stands a shift
//   • the shift produces an item (out_store_tag / out_value), which is the MARKED open loop:
//     in_n == 0 means value from nothing. Giving a recipe an input closes it and changes no structure
//   • selling goes through tn_sell(), the ONE external seam, and nowhere else
//
// DO NOT: name a workshop, switch on TnObjKind, or add a per-object-type code path. Ask what it
// OFFERS. See the contract's THE ONE PRINCIPLE block.
void tn_work_tick(void) {}

#endif // TENEMENT_WORK_H
