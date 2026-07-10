// disclose.h — device-adaptive panel DISCLOSURE, cart-land.
//
// Why this exists (design/device-adaptive-layout.md R2 · design/acidrack-layout-brief.md §2 ·
// field-notes/018 + /020): a dense rack has more panels than a phone can show at once, so it must
// DISCLOSE them — show the one you're working on large, the next few smaller, the rest as a glance,
// and reflow that split as the viewport changes. The priority + finger-footprint budget pass that
// decides this was proven in the acidfit/acidwire prototypes; this header graduates it so every rack
// CALLS it instead of re-hand-rolling the pass (the exact field-018 trap).
//
// THE BOUNDARY (brief §2): disclose decides WHERE a panel goes and HOW BIG (its state); the CART
// decides WHAT lives inside the box it hands back. Nothing in here draws a control — it returns
// (section, state, Box) placements; the cart draws into them. Content-paging inside a panel (e.g.
// acidwire's N/K/F) is the cart's business, not disclosure's.
//
// What's here (the genuinely-generic, genuinely-hard core):
//   - disclose_shape()  — classify the live viewport: TALL (phone portrait) / WIDE (phone landscape,
//                         accordions degenerate → the cart uses tabs) / ROOMY (tablet, show all).
//   - disclose_budget() — THE pass: the working panel EXPANDED, the rest FOLDED, promoted FOLDED→
//                         COMPACT by descending priority while the finger-budget allows.
//   - disclose_stack()  — lay the chosen states top-to-bottom → (section, state, Box) placements.
// NOT here (deliberately): the WIDE tab bar and ROOMY grid are one-line lay.h layouts (lay_grid /
// lay_split) that show every panel at one state — no budget, so no disclosure; the cart keeps them.
// A DE_TRACE self-report of the pass's decision is R4 (the judgment oracle), added when that lands.
//
// FINGER UNIT (R3, not yet an engine primitive): author `foot[]` in *finger multiples* — a comfortable
// touch target is ~44pt, which on the K=2 canvas is ~22 logical px. Pass that as `fu` at the call site
// (a cart #define, e.g. `FU 22.0f`) until `finger_px()` exists; then swap the constant for the call.
//
// Usage:
//   #include "studio.h"
//   #include "lay.h"
//   #include "disclose.h"
//   // per frame, for the TALL (accordion) arrangement:
//   DiscSection sec[N];
//   for (int i = 0; i < N; i++) { sec[i].prio = N - i;   // lower index promotes first
//       sec[i].foot[DISC_FOLDED]=fold_h; sec[i].foot[DISC_COMPACT]=comp_h; sec[i].foot[DISC_EXPANDED]=exp_h; }
//   int st[N]; disclose_budget(sec, N, working, body.h, gap, st);
//   DiscPlace pl[N]; disclose_stack(body, sec, N, st, gap, pl);
//   for (int i = 0; i < N; i++) draw_panel(pl[i].idx, pl[i].state, pl[i].box);

#ifndef DISCLOSE_H
#define DISCLOSE_H

#include "lay.h"   // Box, lay_split, lay_pad

// device shape from the live viewport (min-side + orientation). Values 0,1,2 so a cart can keep its
// own matching enum. Threshold: >= DISC_ROOMY_MIN logical px on the short side = a tablet.
typedef enum { DISC_TALL, DISC_WIDE, DISC_ROOMY } DiscShape;
#ifndef DISC_ROOMY_MIN
#define DISC_ROOMY_MIN 340
#endif
static DiscShape disclose_shape(int w, int h) {
    int mn = w < h ? w : h;
    if (mn >= DISC_ROOMY_MIN) return DISC_ROOMY;
    return (w > h) ? DISC_WIDE : DISC_TALL;
}

// panel states, smallest → largest. FOLDED/COMPACT/EXPANDED index foot[]; FOCUS is a whole-screen
// state the cart drives separately (it isn't produced by the budget pass). Values align with a cart's
// own {FOLDED,COMPACT,EXPANDED,FOCUS} enum.
typedef enum { DISC_FOLDED, DISC_COMPACT, DISC_EXPANDED, DISC_FOCUS } DiscState;

// a panel the cart wants disclosed. foot[FOLDED/COMPACT/EXPANDED] = that state's HEIGHT (logical px;
// author in finger multiples). prio: higher is promoted first (kept larger). pinned: reserved for a
// future "never fold" flag (the transport is usually a separate pinned band, not a section).
typedef struct { float foot[3]; int prio; int pinned; } DiscSection;

// a placement handed back to the cart: which section, at what state, in which box.
typedef struct { int idx; DiscState state; Box box; } DiscPlace;

// THE disclosure pass (the acidfit heart). `working` starts EXPANDED; every other section starts
// FOLDED and is promoted FOLDED→COMPACT, highest `prio` first, while the remaining height budget
// (avail − used − gaps) covers the extra a COMPACT costs over FOLDED. Writes each section's chosen
// state into out_state[] (values are DiscState). Returns the total height the chosen states occupy
// (incl. one `gap` per section). Equal priorities resolve in index order (stable), so prio = n−i
// reproduces a simple front-to-back promotion.
static float disclose_budget(const DiscSection *sec, int n, int working, float avail, float gap, int *out_state) {
    float used = 0;
    for (int i = 0; i < n; i++) {
        out_state[i] = (i == working) ? DISC_EXPANDED : DISC_FOLDED;
        used += sec[i].foot[out_state[i]] + gap;
    }
    float budget = avail - used;
    for (;;) {
        int best = -1; int bestp = 0;
        for (int i = 0; i < n; i++) {
            if (out_state[i] != DISC_FOLDED) continue;
            float extra = sec[i].foot[DISC_COMPACT] - sec[i].foot[DISC_FOLDED];
            if (extra <= budget && (best < 0 || sec[i].prio > bestp)) { best = i; bestp = sec[i].prio; }
        }
        if (best < 0) break;
        float extra = sec[best].foot[DISC_COMPACT] - sec[best].foot[DISC_FOLDED];
        budget -= extra; used += extra;
        out_state[best] = DISC_COMPACT;
    }
    return used;
}

// lay the sections top-to-bottom at their chosen-state heights inside `area`, one `gap` between,
// filling out[] with a placement per section (in the original index order). The cart draws each.
static void disclose_stack(Box area, const DiscSection *sec, int n, const int *state, float gap, DiscPlace *out) {
    Box cur = area;
    for (int i = 0; i < n; i++) {
        float h = sec[i].foot[state[i]];
        Box row = lay_split(cur, EDGE_TOP, h + gap, &cur);
        out[i].idx = i; out[i].state = (DiscState)state[i]; out[i].box = lay_pad(row, 0, 0, gap, 0);
    }
}

#endif // DISCLOSE_H
