// ─────────────────────────────────────────────────────────────────────────────
// tenement/store.h — items, containers, ownership, hauling (design §6). STUB.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tns_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_STORE_H
#define TENEMENT_STORE_H

// OWNER: the `store` agent. Everything here is yours.
//
// What v1 needs:
//   • storage is FURNITURE with a capacity and an accepted tag set, already expressed as an OFFER
//     (a fridge offers TN_SERVE_HUNGER and TN_STORE_FOOD). No separate storage system exists or
//     should exist
//   • an agent carrying an item finds tn_find_store() and puts it there. The PERSON carries the item
//     to the store, never the store to the item: navkit flagged DF's carry-the-container-to-the-item
//     pattern as a source of locking and reservation trouble
//   • OWNERSHIP, which is the interesting one. spec() case 8 currently asserts that a household-1
//     resident helps itself to household-0's fridge, because `household` is not a term in the score.
//     Design §6 treats "whose fridge is it" as a FEATURE. When you make ownership count, go and flip
//     that assertion deliberately.
//
// NOT in v1: nesting, stack sizes, per-item-type filters, reservation locking.
void tn_store_tick(void) {}

#endif // TENEMENT_STORE_H
