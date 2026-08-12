// ─────────────────────────────────────────────────────────────────────────────
// tenement/econ.h — households, money, rent, bills, buying (design §5). STUB.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tne_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_ECON_H
#define TENEMENT_ECON_H

// OWNER: the `econ` agent. Everything here is yours.
//
// What v1 needs:
//   • rent on a rent day, from tn_house[h].money, using tn_clock.day (a CALENDAR, seam 6)
//   • bills, and what happens when a household cannot pay (the design leaves the consequence OPEN,
//     §8: money is friction, not yet a scoreboard. Do not invent a win condition here)
//   • buying objects at TN_OBJ_PRICE and placing them, in concert with the `build` agent
//
// Money is a per-household quantity with sources and sinks, and NOTHING scores it yet. That is a
// deliberate hole (§8a), not a missing feature.
void tn_econ_tick(void) {}

#endif // TENEMENT_ECON_H
