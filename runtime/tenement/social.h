// ─────────────────────────────────────────────────────────────────────────────
// tenement/social.h — RELATIONSHIPS: people join the population of OFFERERS.
//
// Design: docs/design/tenement.md §11 (and §8b, which §11 answers: relationships are the SINK the
// economy is missing, because time is the only thing that can be spent on someone else).
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tns2_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break — `store` already owns tns_, hence the 2.
// Rules: runtime/tenement/model.h header.
//
// ── WHAT THIS MODULE IS, IN ONE PARAGRAPH ───────────────────────────────────
// A resident is an OFFERER. It offers TN_SERVE_SOCIAL, priced by the SAME formula every object is
// priced by — deficit * strength / (travel + reluctance) — with strength and reluctance read off
// the pair matrix instead of a table row. Nothing here re-scores an object and nothing here picks
// a need first. §11's claim that "this is not a new system" holds for the SCORE, exactly as it
// promised. It does NOT hold for two other things, and the honest version of both is below.
//
// ── THE ONE GENUINELY NEW MECHANIC: AN INTERACTION IS TWO-SIDED ─────────────
// A fridge does not have to want you back, so every other outcome of the argmax writes ONE agent
// and reserves ONE offerer (`tn_obj[o].users++`). A person must agree, and that breaks in two
// places the design doc does not mention:
//
//   (a) ALLOCATION becomes two-sided. If both parties merely score, A commits to the meeting and
//       B independently picks the fridge: A ends up standing next to somebody who left. So there
//       is one atomic act that writes BOTH agents (tn_social_begin) and one invariant that says
//       so (tns2_with[tns2_with[i]] == i, healed and asserted every tick). That is the new
//       primitive. It is the two-sided twin of `users++`, not a new system beside the index.
//
//   (b) THE SCORE BECOMES RECURSIVE, which is the part worth knowing. "Will the host agree?"
//       means "would the host's own argmax also pick this?", and the host's argmax ranges over
//       people, one of whom is the asker. A asks B, B weighs A, who is weighing B… Cut here by
//       ONE rule: the host's alternative is its best OBJECT bid, never another person's offer.
//       An interaction has to beat the FURNITURE; it does not get to out-negotiate a third party.
//       That rule is free, because the contract already has an objects-only argmax
//       (tn_best_action) and agents.h already publishes its result on the agent as `bid_score`.
//       So consent costs no extra flood and no extra scan, and what the mechanism consults is
//       exactly the number the HUD already draws.
//
// Refusal is PRICED, not pre-filtered, and the distinction is the one §10 spent a slice on:
//   • continuous half — the bond moves `strength` and moves `shyness`, which is in TILES and lands
//     in the same divisor as travel (the shape store.h's ownership detour established). You cross
//     the building for a friend and not for a stranger, and that is arithmetic, not a branch.
//   • zero half — an unavailable or unwilling host bids 0, so it simply never wins an argmax that
//     starts at 0. No caller branches on it, tn_social_why() says out loud WHY it was 0, and the
//     asker falls through to the next best offer, which is usually an object. What would be a
//     pre-filter is choosing the SOCIAL need first and then hunting for a partner. That never
//     happens here: the person is one more row of one argmax.
//
// The GUEST's opportunity cost is deliberately NOT a term. That is the argmax itself. Only the
// HOST's is, because the host is not the one choosing.
//
// ── NOT IN SCOPE (design §11 puts these on seam 3) ──────────────────────────
// No children, no reproduction, no family trees, no relationship KINDS. One number per ordered
// pair, and the mechanism that makes two residents spend an hour on each other.
//
// ── WHAT DOES NOT WORK UNTIL THE CONTRACT LANDS ─────────────────────────────
// See "REPORT" at the bottom. Short version: the mechanism runs today (tn_social_tick owns the
// approach, the meeting and the payoff), but residents cannot yet CHOOSE company from inside
// tn_best_action, because that function's return value cannot say which population won. Until
// offer.h gains the seam, company is only reachable through the marked SHIM at the end of
// tn_social_tick, which — by design — only fires for a resident that has nothing better to do.
// Which is §8b's own thesis, arriving as a side effect: company is what spare hours are for.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_SOCIAL_H
#define TENEMENT_SOCIAL_H

#ifdef DE_SPEC
#include "spec.h"                 // for tn_social_selfcheck() at the bottom; guarded, dev-only
#endif

// ═════════════════════════════════════════════════════════════════════════════
// LOCAL DEFINITION BLOCK — THIS IS THE DIFF model.h NEEDS.
//
// model.h is FROZEN and not mine, so everything the contract owes this module is defined here,
// structured exactly as it should read in the contract. Paste it in, then `#define
// TN_SOCIAL_IN_CONTRACT` (in model.h, right after the tag enum) and this whole block turns
// itself off. Nothing outside this block knows which side of that switch we are on.
// ═════════════════════════════════════════════════════════════════════════════
#ifndef TN_SOCIAL_IN_CONTRACT

// (1) THE TAG. In model.h it is the last row of the NEEDS run — needs must stay the leading run
//     of TnTag (the contract asserts that), so it goes AFTER TN_SERVE_FUN and BEFORE the count:
//
//         TN_SERVE_FUN,
//     +   TN_SERVE_SOCIAL,                  // served by another RESIDENT, never by an object
//         TN_SERVE_COUNT,
//
//     A tag row cannot be added from out here, so the stand-in is a value one past the contract's
//     enum, plus private storage for the need it would index. CAUTION while it is a stand-in: it
//     is NOT a legal index into need[] or into hud.h's TAG_NAME[] — use the two macros below,
//     which is what this module does everywhere.
#define TN_SERVE_SOCIAL   ((TnTag)TN_TAG_COUNT)
static unsigned char tns2_need_standin[TN_MAX_AGENTS];
#define TN_SOC(a)         tns2_need_standin[a]
#define TNS2_SHOW_TAG     ((unsigned char)TN_SERVE_COUNT)   // what to park in TnAgent.bid_tag

// (2) THE DECAY ROW. agents.h owns need decay from one table, and it is initialised
//     POSITIONALLY: `static const unsigned char TNA_DECAY[TN_NEED_COUNT] = { 8, 6, 5, 10, 4 };`.
//     Adding the tag grows that array to six and leaves the new element ZERO, so the social need
//     would never decay and nobody would ever get lonely: a silent no-op, the same shape of bug
//     as the aux-param channel lint. The row must be added in the same edit as the tag:
//
//         static const unsigned char TNA_DECAY[TN_NEED_COUNT] = { 8, 6, 5, 10, 4, 5 };
//                                                                              ^ SOCIAL
//     While the tag is a stand-in this module decays its own need at that rate (see
//     tn_social_tick), and that block MUST go when the row lands or it decays twice as fast.
#define TNS2_DECAY 5                       // social need lost per hour == the TNA_DECAY row above

// (3) WHO WON AN ARGMAX. The one thing §11 does not account for: growing the population of
//     offerers changes the TYPE of the answer. `int tn_best_action(...)` returns an index into
//     tn_obj[], and no amount of table rows lets it return a person. Two new types, and they are
//     what makes the answer honest instead of an encoded index nobody can validate.
typedef enum { TN_OFFERER_OBJ = 0, TN_OFFERER_AGENT, TN_OFFERER_COUNT } TnOfferer;
typedef struct {
    unsigned char kind;                    // TnOfferer: how to read `idx`
    TnIdx         idx;                     // object index, or agent index, or TN_NONE
    unsigned char tag;                     // TnTag that won
    int           score;
} TnBid;

// (4) THE PAIR MATRIX (owner: social — defined below, so the contract only declares it):
//
//         extern short tn_bond[TN_MAX_AGENTS][TN_MAX_AGENTS];   // owner: social
//
//     ASYMMETRIC on purpose, which is also why §11's "TN_MAX_AGENTS²" is 576 and not the 276 of a
//     triangle: tn_bond[a][b] is how much a values b, and it is the whole reason a refusal can be
//     interesting rather than a scheduling accident.

// (5) THE ENTRY POINTS (owner: social), beside the other module entry points in the contract:
//
//         int  tn_social_bid(int guest, int host, TnTag *out_tag);   // the seam offer.h calls
//         bool tn_social_begin(int guest, int host);                 // the two-sided commit
//         int  tn_social_partner(int agent);                         // TN_NONE if alone
//         int  tn_social_why(int guest, int host);                   // TnSocialWhy, for the HUD
//         void tn_social_tick(void);                                 // approach, meet, payoff
//         void tn_social_reset(void);                                // called by tn_world_init

#else   // ── the contract has it: use the real thing ──────────────────────────
#define TN_SOC(a)         tn_agent[a].need[TN_SERVE_SOCIAL]
#define TNS2_SHOW_TAG     ((unsigned char)TN_SERVE_SOCIAL)
#define TNS2_DECAY        5
// The one mistake that would be silent: a tag row landing AFTER TN_SERVE_COUNT still compiles,
// still scores, and mis-indexes need[] forever. The contract asserts needs are the leading run;
// this asserts THIS need is inside it.
_Static_assert((int)TN_SERVE_SOCIAL < (int)TN_SERVE_COUNT,
               "TN_SERVE_SOCIAL must be inside the NEEDS run of TnTag, before TN_SERVE_COUNT");
#endif  // TN_SOCIAL_IN_CONTRACT

// ── tuning, all in units the contract already uses ──────────────────────────
#define TNS2_BOND_MAX     255              // one byte's worth of "how much a values b"
#define TNS2_COMPANY_MIN   40              // need points a STRANGER's company restores
#define TNS2_COMPANY_MAX  120              // ... and a close friend's. Compare: a sofa's FUN is 90.
#define TNS2_MEET_MIN      20              // minutes. A stranger's chat.
#define TNS2_MEET_MAX      50              // ... an old friend's. Both well under a bed's 480.
#define TNS2_SHY           16              // extra TILES a stranger's distance FEELS like. Same
                                           // unit and the same divisor as travel, which is the
                                           // shape store.h's ownership detour established.
#define TNS2_WARMTH        14              // bond both sides gain from one completed meeting
#define TNS2_COOL           1              // bond both sides lose per day, unattended
#define TNS2_PATIENCE     120              // minutes an approach may take before it is abandoned

// THERE IS DELIBERATELY NO COOLDOWN, and it is worth saying why, because the behaviour looks like a
// missing rule the first time you watch it: two residents with nothing else on offer will meet
// again the minute they part. That is the argmax being right. The meeting refills the need, so the
// next bid is much smaller, and ANY real need outbids it (case S7) — so back-to-back chatting only
// happens between two residents who have no needs and nothing else to do, which is exactly the
// state design §8b says the late game collapses into, now with somewhere for the hours to go. A
// "you just talked" damper would be a hand-written rule competing with the score. If it is ever
// wanted, it belongs as a TERM (a decaying satiety per pair in the matrix), never as a filter.

// ── state ───────────────────────────────────────────────────────────────────
// The pair matrix (design §11: "small enough to be a plain array"). 24² shorts = 1152 bytes.
short tn_bond[TN_MAX_AGENTS][TN_MAX_AGENTS];      // owner: social. tn_bond[a][b] = a values b.

// THE TWO-SIDED RESERVATION. `users++` on an object is one number on the offerer; this is a pair
// of pointers that must agree, and every tick begins by proving they do (tns2_heal).
enum { TNS2_FREE = 0, TNS2_GOING, TNS2_WAITING, TNS2_TOGETHER };
static TnIdx         tns2_with[TN_MAX_AGENTS];    // partner, or TN_NONE
static unsigned char tns2_phase[TN_MAX_AGENTS];
static int           tns2_ends[TN_MAX_AGENTS];    // ABSOLUTE minute (tn_now): the give-up deadline
                                                 // while approaching, the end of the meeting once
                                                 // together. Absolute, never minute-of-day — that
                                                 // wrap is the bug the contract's `until` comment
                                                 // was written about.
static short         tns2_day = -1;               // for the daily cooling of the whole matrix

// Why a bid was what it was. Not needed by the score — needed because design §1 says the
// interesting half of this sim is invisible, and "0" is the least legible number in it.
typedef enum { TN_SOCIAL_OFFERED = 0, TN_SOCIAL_SELF, TN_SOCIAL_SATED, TN_SOCIAL_ENGAGED,
               TN_SOCIAL_BUSY, TN_SOCIAL_UNREACHABLE, TN_SOCIAL_UNWILLING,
               TN_SOCIAL_WHY_COUNT } TnSocialWhy;
static const char *TN_SOCIAL_WHY_NAME[TN_SOCIAL_WHY_COUNT] = {
    "offered", "self", "sated", "engaged", "busy", "unreachable", "refused",
};

// ── plumbing ────────────────────────────────────────────────────────────────
static bool tns2_agent(int a) { return a >= 0 && a < tn_agent_n && a < TN_MAX_AGENTS; }

static int tns2_bond_of(int a, int b) {
    if (!tns2_agent(a) || !tns2_agent(b)) return 0;
    const int v = tn_bond[a][b];
    return v < 0 ? 0 : (v > TNS2_BOND_MAX ? TNS2_BOND_MAX : v);
}

// A person's OFFER, in the contract's own TnOffer shape so nothing new appears in the score. It is
// COMPUTED rather than looked up, and that is the second thing §11 glosses over: TN_OFFERS is a
// table indexed by object KIND, and a person's strength depends on the PAIR, so it cannot live
// there. Same struct, same units, different provenance.
static TnOffer tns2_company(int guest, int host) {
    const int bond = tns2_bond_of(guest, host);
    TnOffer of;
    of.tag      = (unsigned char)TN_SERVE_SOCIAL;
    of.strength = (signed char)(TNS2_COMPANY_MIN +
                  (TNS2_COMPANY_MAX - TNS2_COMPANY_MIN) * bond / TNS2_BOND_MAX);
    of.minutes  = (short)(TNS2_MEET_MIN + (TNS2_MEET_MAX - TNS2_MEET_MIN) * bond / TNS2_BOND_MAX);
    of.capacity = 1;                       // a pair. Company is the capacity-1 offer of the design.
    of.pose     = TN_POSE_STAND;           // they stand and talk; art reads the pose, nothing branches
    return of;
}

// The reluctance to cross a room for someone you barely know, IN TILES so it lands in the same
// divisor as travel. At full bond it is 0: a friend is worth the walk.
static int tns2_shyness(int guest, int host) {
    return TNS2_SHY * (TNS2_BOND_MAX - tns2_bond_of(guest, host)) / TNS2_BOND_MAX;
}

// Walk distance between two residents. SYMMETRIC over the walk graph, which matters for more than
// tidiness: path.h caches ONE flood keyed on the source tile, so asking it from both ends would
// thrash the cache (offer.h measured 138x for exactly that mistake). Every caller below floods
// from the GUEST once and reuses `d` for both sides of the consent test.
static int tns2_dist(int a, int b) {
    return tn_path_len(tn_agent[a].tx, tn_agent[a].ty, tn_agent[b].tx, tn_agent[b].ty);
}

static bool tns2_available(int host) {
    return tns2_agent(host) && tns2_phase[host] == TNS2_FREE &&
           tn_agent[host].activity == TN_ACT_IDLE && tn_agent[host].carrying < 0;
}

// THE SCORE. The contract's formula, unchanged, with the pair matrix supplying the two terms a
// table row would have supplied:  deficit * strength / (travel + 1 + shyness).
// `raw` because the consent term is not in it — see tns2_willing, and the recursion note in the
// header for why exactly one of the two may be recursive.
static int tns2_raw(int guest, int host, int d) {
    const int deficit = 255 - (int)TN_SOC(guest);
    if (deficit <= 0) return 0;
    const TnOffer of = tns2_company(guest, host);
    return deficit * (int)of.strength / (d + 1 + tns2_shyness(guest, host));
}

// ── CONSENT: the host runs the SAME argmax, one level deep ───────────────────
// Would the host rather be doing this than the best thing an OBJECT is offering it? The host's
// alternative is `TnAgent.bid_score`, the objects-only bid agents.h publishes on every idle
// resident each tick and the HUD already draws. Two reasons that is the right number and not a
// shortcut: it cuts the recursion (people are not in it), and it costs nothing — a fresh argmax
// per candidate PAIR would flood path from the host's tile and thrash the single-slot field cache.
//
// A tie goes to the person. Somebody with nothing better to do says yes, which is design §8b's
// whole point: the spare hour is the thing being spent.
static bool tns2_willing(int host, int guest, int d) {
    return tns2_raw(host, guest, d) >= tn_agent[host].bid_score;
}

// One implementation, two public faces: the number and the reason. The order of the tests IS the
// reason vocabulary, so they cannot drift.
static int tns2_bid_why(int guest, int host, int *why) {
    int w = TN_SOCIAL_OFFERED, s = 0;
    if (!tns2_agent(guest) || !tns2_agent(host) || guest == host) w = TN_SOCIAL_SELF;
    else if (tns2_phase[guest] != TNS2_FREE)                      w = TN_SOCIAL_ENGAGED;
    else if (255 - (int)TN_SOC(guest) <= 0)                       w = TN_SOCIAL_SATED;
    else if (!tns2_available(host))                               w = TN_SOCIAL_BUSY;
    else {
        const int d = tns2_dist(guest, host);
        if (d >= TN_UNREACHABLE)                                  w = TN_SOCIAL_UNREACHABLE;
        else if (!tns2_willing(host, guest, d))                   w = TN_SOCIAL_UNWILLING;
        else { s = tns2_raw(guest, host, d); if (s < 0) s = 0; }
    }
    if (why) *why = w;
    return w == TN_SOCIAL_OFFERED ? s : 0;
}

// ── public: the seam ────────────────────────────────────────────────────────

// What `host` offers `guest`, in the same units tno_score returns. 0 means "no offer", which the
// argmax already handles (it starts at 0 and takes strictly greater), so no caller branches.
int tn_social_bid(int guest, int host, TnTag *out_tag) {
    if (out_tag) *out_tag = TN_SERVE_SOCIAL;
    return tns2_bid_why(guest, host, NULL);
}

int tn_social_why(int guest, int host) { int w; (void)tns2_bid_why(guest, host, &w); return w; }
const char *tn_social_why_name(int why) {
    return (why >= 0 && why < TN_SOCIAL_WHY_COUNT) ? TN_SOCIAL_WHY_NAME[why] : "?";
}
int tn_social_partner(int agent) {
    return tns2_agent(agent) && tns2_phase[agent] != TNS2_FREE ? (int)tns2_with[agent] : TN_NONE;
}
int tn_social_bond(int a, int b) { return tns2_bond_of(a, b); }

// ── the two-sided reservation ───────────────────────────────────────────────
// Release ONE side, and the other only if it still points back. Never stomps an agent that has
// already moved on: it restores IDLE only from the state this module put it in (a USE with no
// object, which is the one activity value that parks a resident without agents.h re-deciding it —
// see REPORT item 3 for the honest fix).
static void tns2_let_go(int i) {
    if (i < 0 || i >= TN_MAX_AGENTS) return;
    TnAgent *a = &tn_agent[i];
    if (a->activity == TN_ACT_USE && a->target_obj < 0) {
        a->activity = TN_ACT_IDLE; a->pose = TN_POSE_STAND; a->until = 0;
    }
    tns2_phase[i] = TNS2_FREE; tns2_with[i] = TN_NONE; tns2_ends[i] = 0;
}
static void tns2_release(int i) {
    if (i < 0 || i >= TN_MAX_AGENTS) return;
    const int p = tns2_with[i];
    tns2_let_go(i);
    if (p >= 0 && p < TN_MAX_AGENTS && tns2_with[p] == (TnIdx)i) tns2_let_go(p);
}

// THE INVARIANT, proved rather than trusted, once per tick. It has to be: tn_world_init() does not
// know about this module yet (REPORT item 4), and spec fixtures reset tn_agent_n by hand, so a
// pair can outlive the residents it was made of. A half-broken pair would park somebody in the
// co-opted USE state forever — a resident frozen mid-room, which is the worst failure this module
// can have and the one a player would notice first.
static void tns2_heal(void) {
    for (int i = 0; i < TN_MAX_AGENTS; i++) {
        if (tns2_phase[i] == TNS2_FREE) { tns2_with[i] = TN_NONE; continue; }
        const int p = tns2_with[i];
        if (!tns2_agent(i) || !tns2_agent(p) || tns2_with[p] != (TnIdx)i ||
            tns2_phase[p] == TNS2_FREE)
            tns2_release(i);
    }
}

void tn_social_reset(void) {
    for (int i = 0; i < TN_MAX_AGENTS; i++) {
        tns2_phase[i] = TNS2_FREE; tns2_with[i] = TN_NONE; tns2_ends[i] = 0;
        for (int j = 0; j < TN_MAX_AGENTS; j++) tn_bond[i][j] = 0;
#ifndef TN_SOCIAL_IN_CONTRACT
        tns2_need_standin[i] = 255;         // a new resident is not lonely yet
#endif
    }
    tns2_day = -1;
}

// THE COMMIT. The one act in this design that writes two agents, because it is the one offer that
// had to be accepted. Consent is re-tested HERE and not taken on trust from the score: between the
// bid and the commit, an earlier resident in the same tick may already have taken the host.
bool tn_social_begin(int guest, int host) {
    if (!tns2_agent(guest) || !tns2_agent(host) || guest == host) return false;
    if (tns2_phase[guest] != TNS2_FREE || tn_agent[guest].activity != TN_ACT_IDLE) return false;
    if (!tns2_available(host)) return false;
    if (255 - (int)TN_SOC(guest) <= 0) return false;
    const int d = tns2_dist(guest, host);
    if (d >= TN_UNREACHABLE) return false;
    if (!tns2_willing(host, guest, d)) return false;

    const int deadline = tn_now() + TNS2_PATIENCE;
    const int side[2] = { guest, host };
    for (int k = 0; k < 2; k++) {
        const int i = side[k], other = side[1 - k];
        TnAgent *a = &tn_agent[i];
        tns2_with[i]  = (TnIdx)other;
        tns2_phase[i] = (unsigned char)(k == 0 ? TNS2_GOING : TNS2_WAITING);
        tns2_ends[i]  = deadline;
        // Park the resident where agents.h will not re-decide it: TN_ACT_USE with no object. The
        // USE branch there does its bookkeeping under `if (target_obj >= 0)`, so an objectless USE
        // costs nothing and applies nothing — this module owns the payoff. `until` sits two
        // minutes past our own deadline so OUR give-up always fires first and nobody is ever
        // released by the other module's timer.
        a->activity   = TN_ACT_USE;
        a->target_obj = TN_NONE;
        a->pose       = TN_POSE_STAND;
        a->until      = deadline + 2;
        a->bid_tag    = TNS2_SHOW_TAG;
        a->bid_score  = tns2_raw(i, other, d);
    }
    return true;
}

// ── the meeting ─────────────────────────────────────────────────────────────
// One step along the real route toward the partner, through tn_path_next — the SAME walk agents.h
// takes to an object, so a resident does not get a cleverer route for a friend than for a fridge
// (which would lie about the building's traffic, the thing design §1 is measured on). False means
// the route is gone and the approach must be abandoned honestly.
//
// This duplicates agents.h's WALK body, and that duplication is the visible cost of not having a
// target KIND in the contract: with one, agents.h would walk to a person with no new code anywhere
// and this function would not exist. REPORT item 3.
static bool tns2_step(int i, int tox, int toy) {
    TnAgent *a = &tn_agent[i];
    int nx, ny;
    if (!tn_path_next(a->tx, a->ty, tox, toy, &nx, &ny)) return false;
    a->facing = (unsigned char)(nx > a->tx ? 1 : nx < a->tx ? 3 : ny > a->ty ? 2 : 0);
    a->tx = (short)nx; a->ty = (short)ny;
    return true;
}
static int tns2_adjacent(int a, int b) {
    return abs(tn_agent[a].tx - tn_agent[b].tx) + abs(tn_agent[a].ty - tn_agent[b].ty) <= 1;
}

// Contact: both stop, both look at each other, and the clock starts. The meeting length comes from
// the ASKER's side of the bond, because it is the one who wanted it.
static void tns2_meet(int guest, int host) {
    const TnOffer of = tns2_company(guest, host);
    const int ends = tn_now() + of.minutes;
    const int side[2] = { guest, host };
    for (int k = 0; k < 2; k++) {
        const int i = side[k], other = side[1 - k];
        tns2_phase[i] = TNS2_TOGETHER;
        tns2_ends[i]  = ends;
        tn_agent[i].pose  = (unsigned char)of.pose;
        tn_agent[i].until = ends + 1;      // one minute of slack: our payoff lands first, then
                                           // agents.h finds an agent this module already freed
        tn_agent[i].facing = (unsigned char)(
            (tn_agent[i].tx < tn_agent[other].tx) ? 1 : (tn_agent[i].tx > tn_agent[other].tx) ? 3
          : (tn_agent[i].ty < tn_agent[other].ty) ? 2 : 0);
    }
}

// THE PAYOFF, and it is asymmetric on purpose: each side gains what the OTHER's company is worth
// to IT. The bond then grows both ways, which is the only place in this module time turns into
// state — design §8b's sink, in three lines.
static void tns2_finish(int a, int b) {
    const int side[2] = { a, b };
    for (int k = 0; k < 2; k++) {
        const int i = side[k], other = side[1 - k];
        const TnOffer of = tns2_company(i, other);
        const int v = (int)TN_SOC(i) + (int)of.strength;
        TN_SOC(i) = (unsigned char)(v > 255 ? 255 : v);
        const int nb = tns2_bond_of(i, other) + TNS2_WARMTH;
        tn_bond[i][other] = (short)(nb > TNS2_BOND_MAX ? TNS2_BOND_MAX : nb);
    }
    tns2_let_go(a); tns2_let_go(b);
}

// ── the composed argmax: objects AND people, through PUBLIC entry points only ─
// This is what tn_best_action becomes once the seam lands (REPORT item 2), written here so the
// behaviour can be PROVEN today. It re-implements no scoring: objects are priced by offer.h's own
// argmax and people by tns2_raw. It is also why the objects-only argmax must SURVIVE the seam —
// it is the recursion break consent depends on.
//
// Object first, strictly-greater for the person, so a tie goes to the furniture: the ONE claim
// this module must never accidentally invert is "needs outrank company".
TnBid tn_social_decide(int agent) {
    TnBid b; b.kind = TN_OFFERER_OBJ; b.idx = TN_NONE; b.tag = (unsigned char)TN_SERVE_COUNT; b.score = 0;
    TnTag t = TN_SERVE_COUNT; int s = 0;
    const int o = tn_best_action(agent, &t, &s);
    if (o >= 0 && s > 0) { b.idx = (TnIdx)o; b.tag = (unsigned char)t; b.score = s; }
    for (int h = 0; h < tn_agent_n; h++) {
        TnTag pt; const int ps = tn_social_bid(agent, h, &pt);
        if (ps > b.score) {
            b.kind = TN_OFFERER_AGENT; b.idx = (TnIdx)h; b.tag = (unsigned char)pt; b.score = ps;
        }
    }
    return b;
}

// ── the tick ────────────────────────────────────────────────────────────────
void tn_social_tick(void) {
    tns2_heal();                            // the invariant, every tick, before anything reads it

    if (tns2_day != tn_clock.day) {         // bonds cool unattended: a friendship is a flow
        if (tns2_day >= 0)
            for (int i = 0; i < TN_MAX_AGENTS; i++)
                for (int j = 0; j < TN_MAX_AGENTS; j++)
                    if (tn_bond[i][j] > 0) tn_bond[i][j] -= TNS2_COOL;
        tns2_day = tn_clock.day;
    }

#ifndef TN_SOCIAL_IN_CONTRACT
    // STAND-IN ONLY. agents.h decays every need from TNA_DECAY; the social row is not there yet,
    // so it is done here at the same cadence and the same rate. DELETE THIS BLOCK when the row
    // lands, or loneliness arrives twice as fast as the table says.
    if (tn_clock.minute % 60 == 0)
        for (int i = 0; i < tn_agent_n; i++)
            TN_SOC(i) = (unsigned char)(TN_SOC(i) > TNS2_DECAY ? TN_SOC(i) - TNS2_DECAY : 0);
#endif

    // ── run the interactions ────────────────────────────────────────────────
    for (int i = 0; i < tn_agent_n; i++) {
        const int p = tns2_with[i];
        if (tns2_phase[i] == TNS2_FREE || !tns2_agent(p)) continue;
        switch (tns2_phase[i]) {
        case TNS2_GOING:
            if (tn_now() >= tns2_ends[i]) { tns2_release(i); break; }   // patience ran out
            if (tns2_adjacent(i, p)) tns2_meet(i, p);
            else if (!tns2_step(i, tn_agent[p].tx, tn_agent[p].ty)) tns2_release(i);  // no route
            break;
        case TNS2_WAITING:
            if (tn_now() >= tns2_ends[i]) tns2_release(i);              // stood up
            break;
        case TNS2_TOGETHER:
            if (tn_now() >= tns2_ends[i] && i < p) tns2_finish(i, p);   // once per PAIR, not twice
            break;
        default: break;
        }
    }

    // ── SHIM: matchmaking, until offer.h's argmax can name a person ──────────
    // DELETE THIS LOOP when REPORT item 2 lands: agents.h's IDLE branch does it, for free, in the
    // one argmax. It is here so the mechanism is alive and testable meanwhile, and it is NOT a
    // hand-written "find a friend" pass: it runs the SAME composed argmax and acts only when a
    // person out-bids every object. A resident with an object bid has already been sent walking by
    // agents.h this tick, so today company is what a resident with nothing better to do does —
    // which is where design §8b was heading anyway.
    for (int g = 0; g < tn_agent_n; g++) {
        if (tns2_phase[g] != TNS2_FREE || tn_agent[g].activity != TN_ACT_IDLE) continue;
        const TnBid b = tn_social_decide(g);
        if (b.kind == TN_OFFERER_AGENT && b.idx >= 0) tn_social_begin(g, (int)b.idx);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// spec — runtime/spec.h "SPECS ON AN INCLUDEABLE". The cart's spec() calls this.
// The case that matters is S2: TWO-SIDEDNESS BITES, from both sides, and the refused asker goes
// and does something else instead of standing there.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DE_SPEC
static char tns2_sp[200];

static void tns2_spec_reset(void) {                 // an empty building and no stale pairs
    tn_world_init();
    tn_obj_n = 0; tn_agent_n = 0; tn_item_n = 0;
    tn_social_reset();
    tn_clock.minute = 481; tn_clock.day = 0;        // clear of the hourly decay tick
    for (int i = 0; i < TN_MAX_AGENTS; i++) {
        tn_agent[i].bid_tag = TN_SERVE_COUNT; tn_agent[i].bid_score = 0;
    }
}
// A resident with every OBJECT need sated, so the only thing that can bid for it is a person.
static int tns2_spec_free_agent(int household, int tx, int ty, int lonely) {
    const int i = tn_add_agent(household, tx, ty);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[i].need[n] = 255;
    TN_SOC(i) = (unsigned char)lonely;
    tn_agent[i].bid_score = 0; tn_agent[i].bid_tag = TN_SERVE_COUNT;
    return i;
}
// Put a resident to sleep in a bed, the way agents.h would have: a real USE of a real offer.
static int tns2_spec_asleep(int who, int bed) {
    tn_agent[who].activity   = TN_ACT_USE;
    tn_agent[who].target_obj = (TnIdx)bed;
    tn_agent[who].bid_tag    = TN_SERVE_REST;
    tn_agent[who].pose       = TN_POSE_LIE;
    tn_agent[who].until      = tn_now() + 480;
    tn_obj[bed].users        = 1;
    return who;
}
static int tns2_spec_invariant_ok(void) {
    for (int i = 0; i < TN_MAX_AGENTS; i++) {
        if (tns2_phase[i] == TNS2_FREE) continue;
        const int p = tns2_with[i];
        if (!tns2_agent(p) || tns2_with[p] != (TnIdx)i || tns2_phase[p] == TNS2_FREE) return 0;
    }
    return 1;
}

void tn_social_selfcheck(void) {
    // ── S1: A PERSON IS AN OFFERER, PRICED BY THE SAME SHAPE ────────────────
    tns2_spec_reset();
    {
        const int a = tns2_spec_free_agent(0, 2, 2,  40);   // lonely
        const int b = tns2_spec_free_agent(1, 3, 2, 255);   // sated, idle, nothing better to do
        const int near_bid = tn_social_bid(a, b, NULL);
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S1: a RESIDENT bids for another resident's attention (%d, %s) — the "
                 "population of offerers grew without a second index", near_bid,
                 tn_social_why_name(tn_social_why(a, b)));
        expect(near_bid > 0 && tn_social_why(a, b) == TN_SOCIAL_OFFERED, tns2_sp);

        tn_agent[b].tx = 10; tn_agent[b].ty = 7;            // same offer, further away
        const int far_bid = tn_social_bid(a, b, NULL);
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S1: travel is a TERM for a person exactly as for an object (near %d, far %d)",
                 near_bid, far_bid);
        expect(far_bid > 0 && far_bid < near_bid, tns2_sp);

        tn_agent[b].tx = 3; tn_agent[b].ty = 2;
        TN_SOC(a) = 255;                                    // not lonely any more
        expect(tn_social_bid(a, b, NULL) == 0 && tn_social_why(a, b) == TN_SOCIAL_SATED,
               "social S1: a sated need makes no bid — the same rule tno_score applies to objects");
    }

    // ── S2: TWO-SIDEDNESS BITES, AND THE REFUSED ASKER GOES ELSEWHERE ───────
    // THE HEADLINE. A resident who wants company and a resident who is asleep must not form an
    // interaction, and the wanting one must fall back to something else rather than standing
    // still. Asserted from BOTH sides, because a mechanism that only works when the sleeper
    // happens to have the higher index is not a mechanism.
    for (int mirror = 0; mirror < 2; mirror++) {
        tns2_spec_reset();
        const int bed    = tn_add_obj(TN_OBJ_BED,    6, 2, 0);
        const int fridge = tn_add_obj(TN_OBJ_FRIDGE, 3, 2, 0);
        // In pass 0 the WANTER is index 0; in pass 1 it is index 1. Same fixture, roles swapped.
        const int first  = tns2_spec_free_agent(0, 2, 2, 255);
        const int second = tns2_spec_free_agent(1, 6, 2, 255);
        const int wanter = mirror ? second : first, sleeper = mirror ? first : second;
        tn_agent[wanter].tx = 2; tn_agent[wanter].ty = 2;
        tn_agent[sleeper].tx = 6; tn_agent[sleeper].ty = 2;
        TN_SOC(wanter) = 30;                                // very lonely
        tn_agent[wanter].need[TN_SERVE_HUNGER] = 120;       // and there is a fridge
        tns2_spec_asleep(sleeper, bed);

        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S2/%d: a lonely resident's bid on a SLEEPING one is 0, and legibly so "
                 "(%s) — a person, unlike a fridge, has to be available",
                 mirror, tn_social_why_name(tn_social_why(wanter, sleeper)));
        expect(tn_social_bid(wanter, sleeper, NULL) == 0 &&
               tn_social_why(wanter, sleeper) == TN_SOCIAL_BUSY, tns2_sp);

        expect(tn_social_begin(wanter, sleeper) == false &&
               tn_social_partner(wanter) == TN_NONE && tn_social_partner(sleeper) == TN_NONE,
               "social S2: the COMMIT refuses too, so a bid that leaked through could not force it");

        const TnBid b = tn_social_decide(wanter);
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S2/%d: the refused resident FALLS BACK to the best object (kind %d, obj %d, "
                 "score %d) instead of standing still", mirror, b.kind, (int)b.idx, b.score);
        expect(b.kind == TN_OFFERER_OBJ && (int)b.idx == fridge && b.score > 0, tns2_sp);

        tn_agents_tick(); tn_social_tick();
        expect(tn_agent[wanter].activity != TN_ACT_IDLE && tn_social_partner(wanter) == TN_NONE,
               "social S2: after a real tick it is on its way somewhere, and still unpaired");
        expect(tn_agent[sleeper].activity == TN_ACT_USE &&
               (int)tn_agent[sleeper].target_obj == bed && tn_social_partner(sleeper) == TN_NONE,
               "social S2: and the sleeper was not dragged out of bed by somebody else's need");
        expect(tns2_spec_invariant_ok(), "social S2: the pair invariant held through the refusal");
    }

    // ── S3: REFUSAL BY OPPORTUNITY COST — both free, and still no ────────────
    // The half that is NOT availability. Both residents are idle and unpaired; the host simply has
    // something better on the table, which is the number agents.h published for it.
    tns2_spec_reset();
    {
        const int a = tns2_spec_free_agent(0, 2, 2,  30);    // lonely
        const int b = tns2_spec_free_agent(1, 3, 2, 255);    // not lonely at all
        tn_agent[b].bid_score = 4000;                        // ... and something better to do
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S3: an AVAILABLE host still refuses when its own best bid beats the "
                 "company (%s) — consent is the host's own argmax, one level deep",
                 tn_social_why_name(tn_social_why(a, b)));
        expect(tn_social_bid(a, b, NULL) == 0 &&
               tn_social_why(a, b) == TN_SOCIAL_UNWILLING, tns2_sp);
        expect(tn_social_begin(a, b) == false, "social S3: and the commit refuses on the same test");

        tn_agent[b].bid_score = 0;                           // nothing better to do after all
        expect(tn_social_bid(a, b, NULL) > 0 && tn_social_begin(a, b),
               "social S3: with the alternative gone the same pair commits — the refusal was the "
               "host's PRICE, not a ban on socialising");

        // And the third case, which is the one that makes it a mechanism rather than a gate: a host
        // who is ALSO lonely accepts something it would otherwise have refused.
        tns2_spec_reset();
        const int c = tns2_spec_free_agent(0, 2, 2, 30);
        const int d = tns2_spec_free_agent(1, 3, 2, 30);      // lonely too
        tn_agent[d].bid_score = 400;
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S3: a host who is ALSO lonely says yes at an alternative that a contented "
                 "one refuses (its own side of the offer scores %d vs bid %d)",
                 tns2_raw(d, c, tns2_dist(c, d)), tn_agent[d].bid_score);
        expect(tn_social_bid(c, d, NULL) > 0, tns2_sp);
    }

    // ── S4: THE COMMITMENT IS TWO-SIDED, AND CANNOT BE POACHED ──────────────
    tns2_spec_reset();
    {
        const int a = tns2_spec_free_agent(0, 2, 2, 40);
        const int b = tns2_spec_free_agent(1, 3, 2, 40);
        const int c = tns2_spec_free_agent(2, 4, 2, 40);
        expect(tn_social_begin(a, b), "social S4: a willing pair commits");
        expect(tn_social_partner(a) == b && tn_social_partner(b) == a &&
               tn_agent[a].activity != TN_ACT_IDLE && tn_agent[b].activity != TN_ACT_IDLE,
               "social S4: ONE act wrote BOTH residents — the reservation points both ways");
        expect(tn_social_bid(c, a, NULL) == 0 && tn_social_bid(c, b, NULL) == 0 &&
               tn_social_why(c, a) == TN_SOCIAL_BUSY,
               "social S4: a committed pair cannot be poached by a third resident");
        expect(tn_social_begin(c, a) == false && tn_social_begin(a, b) == false,
               "social S4: and neither side can be double-booked, from either direction");
        expect(tns2_spec_invariant_ok(), "social S4: the invariant holds while the pair is live");
    }

    // ── S5: THE BOND IS PER ORDERED PAIR, AND IT MOVES THE NUMBERS ──────────
    // Why the matrix is 576 and not the 276 of a triangle.
    tns2_spec_reset();
    {
        const int a = tns2_spec_free_agent(0, 2, 2, 40);
        const int b = tns2_spec_free_agent(1, 3, 2, 40);
        tn_bond[a][b] = TNS2_BOND_MAX; tn_bond[b][a] = 0;
        const int ab = tn_social_bid(a, b, NULL), ba = tn_social_bid(b, a, NULL);
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S5: one pair, two prices (%d vs %d) — the bond is ASYMMETRIC, which is "
                 "what makes a refusal interesting rather than a scheduling accident", ab, ba);
        expect(ab > ba && ba > 0, tns2_sp);
    }
    tns2_spec_reset();
    {
        // You cross the building for a friend and not for a stranger, and it is arithmetic: the
        // friend is FURTHER away and still wins, on strength and shyness alone.
        const int me       = tns2_spec_free_agent(0, 2, 2, 20);
        const int stranger = tns2_spec_free_agent(1, 4, 2, 255);
        const int friend_  = tns2_spec_free_agent(2, 9, 2, 255);
        tn_bond[me][friend_] = TNS2_BOND_MAX;
        const TnBid b = tn_social_decide(me);
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S5: the FAR friend (%d tiles, bid %d) outbids the NEAR stranger (%d tiles, "
                 "bid %d)", tns2_dist(me, friend_), tn_social_bid(me, friend_, NULL),
                 tns2_dist(me, stranger), tn_social_bid(me, stranger, NULL));
        expect(b.kind == TN_OFFERER_AGENT && (int)b.idx == friend_, tns2_sp);
    }

    // ── S6: THE WHOLE LOOP — approach, meet, payoff, release ────────────────
    tns2_spec_reset();
    {
        const int a = tns2_spec_free_agent(0, 2, 2, 40);
        const int b = tns2_spec_free_agent(1, 6, 2, 40);
        const int soc_a0 = TN_SOC(a), soc_b0 = TN_SOC(b);
        int saw_together = 0, saw_adjacent = 0, broke = 0, done = 0;
        for (int t = 0; t < 200 && !done; t++) {
            tn_agents_tick(); tn_social_tick();
            if (!tns2_spec_invariant_ok()) broke = 1;
            if (tns2_phase[a] == TNS2_TOGETHER) {
                saw_together = 1;
                if (tns2_adjacent(a, b)) saw_adjacent = 1;
            }
            if (saw_together && tns2_phase[a] == TNS2_FREE) done = 1;
        }
        expect(saw_together && saw_adjacent,
               "social S6: two lonely residents with time on their hands walk to each other and meet");
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S6: the meeting PAID both sides (social %d->%d and %d->%d) — the hours "
                 "went somewhere", soc_a0, TN_SOC(a), soc_b0, TN_SOC(b));
        expect(done && TN_SOC(a) > soc_a0 && TN_SOC(b) > soc_b0, tns2_sp);
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S6: and it left a relationship behind, both ways (%d / %d)",
                 tn_social_bond(a, b), tn_social_bond(b, a));
        expect(tn_social_bond(a, b) > 0 && tn_social_bond(b, a) > 0, tns2_sp);
        expect(tn_social_partner(a) == TN_NONE && tn_social_partner(b) == TN_NONE &&
               tn_agent[a].activity == TN_ACT_IDLE && tn_agent[b].activity == TN_ACT_IDLE,
               "social S6: both were released cleanly — nobody is parked in an interaction forever");
        expect(!broke, "social S6: the pair invariant held every minute of the whole interaction");
    }

    // ── S7: NEEDS OUTRANK COMPANY, UNTIL THEY DO NOT (design §8b) ──────────
    // The sequencing claim of the whole design, in two assertions: build the economy until
    // residents have spare hours, THEN give the hours somewhere to go.
    tns2_spec_reset();
    {
        const int fridge = tn_add_obj(TN_OBJ_FRIDGE, 3, 2, 0);
        const int me     = tns2_spec_free_agent(0, 2, 2, 30);
        const int mate   = tns2_spec_free_agent(1, 3, 3, 30);
        tn_agent[me].need[TN_SERVE_HUNGER] = 10;             // desperate
        const TnBid hungry = tn_social_decide(me);
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S7: a desperate resident takes the FRIDGE over the friend (kind %d idx %d "
                 "score %d) — company never jumps the queue in front of a real need",
                 hungry.kind, (int)hungry.idx, hungry.score);
        expect(hungry.kind == TN_OFFERER_OBJ && (int)hungry.idx == fridge, tns2_sp);

        tn_agent[me].need[TN_SERVE_HUNGER] = 255;            // needs handled: the hours are free
        const TnBid idle = tn_social_decide(me);
        snprintf(tns2_sp, sizeof tns2_sp,
                 "social S7: with its needs met the SAME resident spends the hour on the neighbour "
                 "(kind %d idx %d score %d) — relationships are the sink the economy was missing",
                 idle.kind, (int)idle.idx, idle.score);
        expect(idle.kind == TN_OFFERER_AGENT && (int)idle.idx == mate, tns2_sp);
    }

    // ── S8: NOBODY CAN BE LEFT PARKED — the failure a player would see first ─
    tns2_spec_reset();
    {
        const int a = tns2_spec_free_agent(0, 2, 2, 40);
        const int b = tns2_spec_free_agent(1, 3, 2, 40);
        expect(tn_social_begin(a, b), "social S8: a pair commits");
        // Both go sated the moment the pair exists, so what this measures is purely the GIVE-UP.
        // Left lonely they are re-matched by the shim inside the very same tick that frees them,
        // which is the right behaviour and hides the thing being tested (it is asserted below).
        TN_SOC(a) = 255; TN_SOC(b) = 255;
        tn_clock.day += 1;                                   // an hour is nothing; a day is a stall
        tn_social_tick();
        expect(tn_social_partner(a) == TN_NONE && tn_social_partner(b) == TN_NONE &&
               tn_agent[a].activity == TN_ACT_IDLE && tn_agent[b].activity == TN_ACT_IDLE,
               "social S8: a stalled interaction gives up and hands both residents back to the "
               "state machine — an interaction can never freeze somebody mid-room");

        // The same stall with both still lonely: they are freed and immediately re-matched, so the
        // invariant to state is the one that actually protects the player from a frozen resident —
        // a parked resident always has a LIVE deadline, whoever it is parked with.
        TN_SOC(a) = 40; TN_SOC(b) = 40;
        expect(tn_social_begin(a, b), "social S8: and it can be formed again afterwards");
        tn_clock.day += 1;
        tn_social_tick();
        {
            int stuck = 0;
            for (int i = 0; i < tn_agent_n; i++)
                if (tn_agent[i].activity == TN_ACT_USE && tn_agent[i].target_obj < 0 &&
                    tn_now() >= tns2_ends[i]) stuck++;
            expect(stuck == 0 && tns2_spec_invariant_ok(),
                   "social S8: after the stall nobody is parked past their own deadline — a lonely "
                   "resident is re-matched, not abandoned mid-room");
        }

        // A pair that outlives the residents it was made of. tn_world_init() does not know about
        // this module (REPORT item 4), and a spec fixture resets tn_agent_n by hand, so this is
        // not hypothetical — it is what every reset above would have done.
        TN_SOC(a) = 255; TN_SOC(b) = 255;                         // (again: no re-matching noise)
        tns2_phase[0] = TNS2_TOGETHER; tns2_with[0] = (TnIdx)19;   // agent 19 does not exist
        tns2_phase[1] = TNS2_GOING;    tns2_with[1] = TN_NONE;     // half a pair
        tn_social_tick();
        expect(tns2_spec_invariant_ok() && tns2_phase[0] == TNS2_FREE && tns2_phase[1] == TNS2_FREE,
               "social S8: a stale or half-formed pair cannot survive one tick");
    }

#if defined(TN_SOCIAL_IN_CONTRACT) && defined(TENEMENT_AGENTS_H)
    // The silent half of the contract change: the tag without the decay row means the need never
    // moves and nobody is ever lonely. Only checkable once both files are real.
    expect((int)(sizeof TNA_DECAY / sizeof TNA_DECAY[0]) == TN_NEED_COUNT &&
           TNA_DECAY[TN_SERVE_SOCIAL] > 0,
           "social S9: agents.h's TNA_DECAY has a row for TN_SERVE_SOCIAL (see REPORT item 1) — "
           "without it the need never decays and the whole mechanism is dead code");
#endif

    tns2_spec_reset();
    tn_world_init();                                         // leave the world as we found it
    tn_social_reset();
}
#endif // DE_SPEC

// ─────────────────────────────────────────────────────────────────────────────
// REPORT — what this module needs from files it does not own. Nothing there has been edited.
//
// WHAT WORKS TODAY, with only the two wiring lines of item 4: residents form two-sided
// interactions, walk to each other, spend real minutes, pay both sides, build a relationship, and
// refuse each other for legible reasons. What does NOT work until item 2 lands: a resident cannot
// CHOOSE company from inside the one argmax, so company only happens through the marked shim,
// i.e. only for a resident that had no object bid at all.
//
// 1. model.h — THE CONTRACT BLOCK. Paste the block marked "LOCAL DEFINITION BLOCK" at the top of
//    this file (items 1-5 in it), then `#define TN_SOCIAL_IN_CONTRACT` in model.h so this file
//    stops standing in for it. The two pieces of that block that are NOT in this file, because
//    they are edits to other files, are:
//       • TnTag gains `TN_SERVE_SOCIAL,` as the last row of the NEEDS run (before TN_SERVE_COUNT).
//       • agents.h's TNA_DECAY gains a sixth element: `{ 8, 6, 5, 10, 4, 5 }`. POSITIONAL
//         initialiser — miss it and the need never decays. Spec case S9 fails until it is there.
//       • hud.h's TAG_NAME gains `[TN_SERVE_SOCIAL]="SOCIAL"` (cosmetic; it is designated, so the
//         gap is a NULL, and line 41 already guards for that).
//
// 2. offer.h — THE SEAM (the headline; one function, five lines). The population of offerers
//    grows, and the ANSWER has to be able to say which population won:
//
//        +TnBid tn_best_bid(int agent) {                  // ONE argmax over BOTH populations
//        +    TnBid b; b.kind = TN_OFFERER_OBJ; b.idx = TN_NONE;
//        +    b.tag = (unsigned char)TN_SERVE_COUNT; b.score = 0;
//        +    TnTag t; int s;
//        +    const int o = tn_best_action(agent, &t, &s);        // objects, unchanged
//        +    if (o >= 0 && s > 0) { b.idx = (TnIdx)o; b.tag = (unsigned char)t; b.score = s; }
//        +    for (int h = 0; h < tn_agent_n; h++) {              // people, new
//        +        TnTag pt; const int ps = tn_social_bid(agent, h, &pt);
//        +        if (ps > b.score) { b.kind = TN_OFFERER_AGENT; b.idx = (TnIdx)h;
//        +                            b.tag = (unsigned char)pt; b.score = ps; }
//        +    }
//        +    return b;
//        +}
//
//    plus one declaration in model.h beside the other offer-index entry points:
//
//        TnBid tn_best_bid(int agent);                     // owner: offer
//
//    NOTE WHAT THIS DOES NOT DO: it does not change tn_best_action's signature, and it must not.
//    That function stays the OBJECTS-ONLY argmax, on purpose and now in the contract for a reason
//    beyond back-compatibility: it is the recursion break consent depends on (see the header). Nine
//    existing call sites keep compiling and keep meaning what they meant. work.h's "a need outbids
//    work" threshold is the one place worth a second thought — swap it to tn_best_bid if company
//    should also outbid a shift, leave it if it should not. That is a design call, not a port.
//    tn_social_decide() in this file is the same argmax composed from public entry points; delete
//    it when this lands (spec cases S2/S5/S7 use it, so retarget them to tn_best_bid).
//
// 3. agents.h — the IDLE branch, once item 2 exists (and NOT before: the current one cannot act on
//    a person). Two behaviours, and the fall-through is not optional — an earlier resident in the
//    same tick may already have taken the host:
//
//        -    const int o = tn_best_action(i, &tag, &score);
//        -    a->bid_tag = tag; a->bid_score = score;
//        -    if (o >= 0) { a->target_obj = (signed char)o; a->activity = TN_ACT_WALK; }
//        +    const TnBid b = tn_best_bid(i);
//        +    a->bid_tag = b.tag; a->bid_score = b.score;
//        +    if (b.kind == TN_OFFERER_AGENT && tn_social_begin(i, (int)b.idx)) break;
//        +    const int o = (b.kind == TN_OFFERER_OBJ) ? (int)b.idx
//        +                                            : tn_best_action(i, &tag, &score);
//        +    if (o >= 0) { a->target_obj = (TnIdx)o; a->activity = TN_ACT_WALK; }
//
//    And then delete the SHIM loop at the end of tn_social_tick().
//
//    OPTIONAL, and the honest version of the one hack in this file: this module parks an agent in
//    TN_ACT_USE with target_obj = TN_NONE, because that is the only activity value agents.h will
//    leave alone without an object, and it walks the asker itself (tns2_step duplicates three
//    lines of the WALK branch). The clean shape is a target KIND in the contract:
//
//        TnActivity gains   TN_ACT_SOCIAL
//        TnAgent gains      unsigned char target_kind;   // TnOfferer: how to read target_obj
//        agents.h's WALK/USE branches read tn_obj[] only when target_kind == TN_OFFERER_OBJ
//
//    That deletes tns2_step and makes the HUD's activity mark and art.h's pose honest (today a
//    resident walking to a friend reads as '*' using something). It is the same change children
//    will want on seam 3, so it may be worth doing once rather than twice. Everything in this
//    module works without it.
//
// 4. tools/carts/tenement.c — the two wiring lines this module needs to exist at all:
//
//        #include "tenement/social.h"      // after agents.h (it reads TNA_DECAY in spec) —
//                                          // last in the include list is fine
//        update():  ... tn_econ_tick(); tn_store_tick(); tn_social_tick();
//        spec():    ... tn_store_selfcheck(); tn_social_selfcheck(); tn_build_selfcheck();
//
//    and one line in world.h's tn_world_init(), which is where a new world should forget every
//    relationship the old one had:
//
//        tn_social_reset();
//
//    Without it this module heals stale pairs on the next tick (case S8 proves it) but the bond
//    matrix survives a re-init, so a fresh building would open with old friendships in it.
//
// 5. model.h, ONE MORE DECLARATION (tiny, and not caused by this module): `tn_path_next` is the
//    only path entry point NOT in the contract, so this module and agents.h both reach it through
//    path.h's own declaration and therefore silently depend on include order — the exact thing the
//    contract's cross-module block exists to prevent. It belongs beside tn_path_len:
//
//        bool tn_path_next(int fromx, int fromy, int tox, int toy, int *outx, int *outy);  // owner: path
//
// 6. A NOTE ON COST, since this is the first thing in the sim that scores agents against agents.
//    tn_social_decide is O(agents) on top of one objects argmax, and it costs exactly ONE path
//    flood — the guest's — because distance is symmetric (so both sides of the consent test reuse
//    it) and because consent reads the host's PUBLISHED bid_score rather than re-running its
//    argmax. Re-running it would have flooded from every candidate host and thrashed path.h's
//    single-slot field cache, which offer.h measured at 138x. If consent ever needs to be fresher
//    than one tick, cache an alt[] table per minute in a first pass; do not flood per pair.
// ─────────────────────────────────────────────────────────────────────────────

#endif // TENEMENT_SOCIAL_H
