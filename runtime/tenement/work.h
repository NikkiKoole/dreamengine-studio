// ─────────────────────────────────────────────────────────────────────────────
// tenement/work.h — work orders, the shift, goods, the sell seam (design §4/§5).
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tnk_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
//
// ── HOW THE LOOP RUNS, in one paragraph ─────────────────────────────────────
// A STANDING ORDER exists per household per recipe. A resident with nothing better to do claims
// one, asks the offer index for the nearest thing PROVIDING the recipe's capability, walks there,
// stands the recipe's shift, and the shift mints one good which is sold at the one external seam.
// Nothing here knows what a loom is. The word "loom" does not appear below this comment, and the
// selfcheck at the bottom proves the loop runs identically when the machine is a COUNTER instead —
// a different TnObjKind entirely, whose only relevant property is that it offers TN_CAP_WORK.
//
// ── WHAT THIS MODULE DELIBERATELY DOES NOT OWN ──────────────────────────────
// The IDLE/WALK/USE state machine and the `users` capacity count belong to the agents module.
// Work never increments or decrements `users` and never sets an agent IDLE. It hands an agent a
// destination and a bid tag, then reads the machine's own state to know where the shift is up to.
// One owner for capacity means work cannot leak one. See the notes on TN_ACT_WORK and on `until`.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_WORK_H
#define TENEMENT_WORK_H

// ── THE ONE TUNING NUMBER ───────────────────────────────────────────────────
// A resident works when NOTHING on the table bids more than this. It is in the SAME currency as the
// need argmax (deficit * strength / (travel + queue)), so "why is nobody working" is answerable by
// reading the HUD's bid column against one constant.
//
// Why a threshold at all, when §2 says one argmax: capabilities deliberately do NOT bid in
// tn_best_action — that boundary is what lets one index serve three consumers, and the cart's spec
// case 4 pins it. So work cannot be a row in that argmax, and something outside it has to decide.
// A single number in the argmax's own currency is the least invented thing that can. It is a first
// guess like the object placeholders, and it is the knob to turn if the building never works or
// never eats.
#define TNK_WORK_BID 1200

// Where one resident's shift is up to. NOT the order (that is contract data in tn_order[]) and not
// the activity (that is the agents module's): only the bookkeeping neither of them holds.
enum { TNK_PH_NONE = 0, TNK_PH_TRAVEL, TNK_PH_SHIFT };
typedef struct {
    signed char   order;                  // the order this resident is running, or -1
    short         obj;                     // the resolved workspot. SHORT, not signed char: object
                                           // indices run to TN_MAX_OBJECTS-1 = 191 (see the report —
                                           // the contract's own at_obj/target_obj cannot do that).
    unsigned char phase;
    short         logged;                  // minutes actually STOOD at the spot, counted here
} TnkShift;
static TnkShift tnk_shift[TN_MAX_AGENTS];

// Ledger, read by the selfcheck. Not drawn: the HUD is not this module's file. `tnk_claims` counts
// order claims, which is how a WASTED COMMUTE becomes visible: a resident that claims, walks, is
// turned away and claims again next minute leaves every other number looking healthy.
static int tnk_shifts_done, tnk_shifts_lost, tnk_goods_made, tnk_claims;

// ── helpers ─────────────────────────────────────────────────────────────────

// The offer row for a tag on an object, from the contract's public tables. Work asks what an object
// OFFERS and never what it IS (contract rule 2). Deliberately a tnk_ copy rather than a reach into
// a sibling's static helper: modules do not share privates, and this reads only TN_OFFERS/TN_OFFER_N.
static const TnOffer *tnk_offer(int obj, TnTag tag) {
    const int kind = tn_obj[obj].kind;
    for (int i = 0; i < TN_OFFER_N[kind]; i++)
        if (TN_OFFERS[kind][i].tag == (unsigned char)tag) return &TN_OFFERS[kind][i];
    return NULL;
}

// Room for one more pair of hands? The capability's own capacity says so; a capacity-1 machine with
// somebody at it is THE contention design §4 asks for, and it costs no queue code.
static int tnk_spot_free(int obj, TnTag cap) {
    const TnOffer *of = tnk_offer(obj, cap);
    return of && tn_obj[obj].users < of->capacity;
}

// Has another live order already resolved to this spot? The reservation is DERIVED from the order
// table (`at_obj`), so it holds no state of its own and cannot go stale. Without it two households
// claim the same machine in the same minute and one walks across the building to be turned away.
static int tnk_spot_reserved(int obj, int except_order) {
    for (int o = 0; o < tn_order_n; o++)
        if (o != except_order && tn_order[o].claimed_by >= 0 && tn_order[o].at_obj == obj) return 1;
    return 0;
}

// STANDING ORDERS: one per household per recipe, never consumed. Finishing a cycle hands the order
// back to the pool, which is why the table never churns and an order index stays valid forever.
// A one-off order is a `remaining` count on TnOrder and nothing else — no structure here changes.
static void tnk_post_orders(void) {
    for (int h = 0; h < tn_house_n; h++)
        for (int r = 0; r < TN_RECIPE_N; r++) {
            int have = 0;
            for (int o = 0; o < tn_order_n; o++)
                if (tn_order[o].household == (unsigned char)h && tn_order[o].recipe == (unsigned char)r)
                    { have = 1; break; }
            if (have || tn_order_n >= TN_MAX_ORDERS) continue;
            tn_order[tn_order_n++] = (TnOrder){ (unsigned char)r, -1, -1, (unsigned char)h };
        }
}

// An unclaimed order this resident's household would be paid for.
static int tnk_open_order(int agent) {
    for (int o = 0; o < tn_order_n; o++)
        if (tn_order[o].claimed_by < 0 && tn_order[o].household == tn_agent[agent].household) return o;
    return -1;
}

// Hand the order back, and ask the AGENTS module to end the stand rather than ending it here:
// `until < 0` is its own "done now" signal, so it does the users-- and the return to IDLE. One
// owner for capacity means work can never leak a count, however this function is reached.
static void tnk_release(int agent) {
    TnkShift *sh = &tnk_shift[agent];
    if (sh->order >= 0 && sh->order < tn_order_n && tn_order[sh->order].claimed_by == (signed char)agent) {
        tn_order[sh->order].claimed_by = -1;
        tn_order[sh->order].at_obj     = -1;
    }
    if (tn_agent[agent].activity == TN_ACT_USE) tn_agent[agent].until = -1;
    *sh = (TnkShift){ -1, -1, TNK_PH_NONE, 0 };
}

// THE SHIFT'S OUTPUT, and the marked open loop.
//
// v1's recipe has in_n == 0, so this turns TIME into VALUE and value appears from nowhere (design
// §5). That is stated, not hidden. When a recipe gains an input the only new code is a consume step
// above the mint; the item, the seam, the order and the state machine are untouched.
// Returns whether a good actually came out, so a shift stood against a full world is counted as
// LOST rather than DONE. Without that the ledger would claim a good the world never got, and the
// selfcheck's money == shifts x value invariant would be quietly wrong instead of loudly wrong.
static int tnk_deliver(int agent, const TnRecipe *rc, int household) {
    if (tn_item_n >= TN_MAX_ITEMS) return 0;                        // full is an answer, not a crash
    const int it = tn_item_n++;
    tn_item[it] = (TnItem){ rc->out_store_tag, rc->out_value, (signed char)agent, -1,
                            (unsigned char)tn_agent[agent].tx, (unsigned char)tn_agent[agent].ty };
    tn_agent[agent].carrying = (signed char)it;
    tnk_goods_made++;

    tn_sell(household, it);        // ←── TN_SEAM_EXTERNAL. The ONE place money enters (design §5).

    // v1 SELLS AT THE MACHINE, because store.h is a stub and a good with nowhere to go is a leak
    // that fills tn_item[] and then silently stops production. The good is a real tagged TnItem,
    // carried by the person who made it, for exactly as long as it takes to sell it.
    //
    // WHEN store.h CAN HAUL, DELETE THE TWO LINES BELOW. The item then stays carried, store finds
    // it a home by its tag, and the sale moves to whoever empties the store. Nothing else here
    // moves, which is the whole claim of the shape. The selfcheck asserts today's behaviour
    // ("no item is left behind") so that whoever lands hauling has to come here and flip it.
    tn_agent[agent].carrying = -1;
    if (it == tn_item_n - 1) tn_item_n--;
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
void tn_work_tick(void) {
    // tn_world_init() zeroes tn_order_n, and it is the only thing that does. That is the signal
    // that the world was rebuilt under us and every shift in flight refers to a building that no
    // longer exists. (Each shift is re-validated below as well, so a scenario that rebuilds only
    // the objects — as spec() does — is safe too.)
    if (tn_order_n == 0)
        for (int i = 0; i < TN_MAX_AGENTS; i++) tnk_shift[i] = (TnkShift){ -1, -1, TNK_PH_NONE, 0 };

    tnk_post_orders();

    for (int i = 0; i < tn_agent_n; i++) {
        TnAgent  *a  = &tn_agent[i];
        TnkShift *sh = &tnk_shift[i];

        // ── a shift in flight ───────────────────────────────────────────────
        if (sh->phase != TNK_PH_NONE) {
            const int live = sh->order >= 0 && sh->order < tn_order_n &&
                             sh->obj   >= 0 && sh->obj   < tn_obj_n   &&
                             tn_order[sh->order].claimed_by == (signed char)i;
            if (!live) { tnk_release(i); continue; }

            const TnRecipe *rc = &TN_RECIPES[tn_order[sh->order].recipe];
            // Re-assert the bid tag every tick: it is what the agents module's arrival check looks
            // the offer up by, and it is what the HUD prints. Saying WORK there is the point of
            // design §1 — visible labour that a stranger can read without a tooltip.
            a->bid_tag = (unsigned char)rc->needs_cap;

            if (a->activity == TN_ACT_USE && a->target_obj == (signed char)sh->obj) {
                sh->phase = TNK_PH_SHIFT;
                if (sh->logged < 32000) sh->logged++;
                a->bid_score = rc->minutes - sh->logged;          // minutes left, on the HUD
                if (sh->logged >= rc->minutes) {
                    if (tnk_deliver(i, rc, tn_order[sh->order].household)) tnk_shifts_done++;
                    else                                                   tnk_shifts_lost++;
                    tnk_release(i);
                } else {
                    // THE SHIFT LENGTH IS THE RECIPE'S, NEVER THE WORKSPOT'S. A workspot may
                    // advertise its capability with minutes 0 (one of them does), and letting the
                    // agents module time the stand from the OFFER would then finish a shift the
                    // minute it began and mint value every tick — a fountain, from a data table
                    // nobody thought was about work. Holding `until` ahead of the clock keeps the
                    // stand open exactly as long as TN_RECIPES says.
                    //
                    // +2, not +1: the agents module advances the clock at the top of its own tick,
                    // before its `minute >= until` test, so +1 would come due immediately.
                    a->until = tn_now() + 2;   // absolute now; see model.h on `until`
                }
                continue;
            }
            if (a->activity == TN_ACT_WALK && a->target_obj == (signed char)sh->obj) continue;  // commuting

            // Anything else means the stand ended early or never began: the spot filled up before
            // arrival, or the world moved. Give the order back, somebody else can have it.
            if (sh->phase == TNK_PH_SHIFT) tnk_shifts_lost++;
            tnk_release(i);
            continue;
        }

        // ── no shift: should this resident take an order? ───────────────────
        // NEVER touch someone mid-USE. Their `users` count is held by the agents module and is
        // released by its USE case; redirecting them out of it would leak the capacity forever and
        // quietly make an object unusable for the rest of the run.
        if (a->activity == TN_ACT_USE) continue;

        const int ord = tnk_open_order(i);
        if (ord < 0) continue;                                     // nothing posted for this household
        const TnRecipe *rc  = &TN_RECIPES[tn_order[ord].recipe];
        const TnTag     cap = (TnTag)rc->needs_cap;

        int best = 0;
        if (tn_best_action(i, NULL, &best) >= 0 && best >= TNK_WORK_BID) continue;   // a need outbids work

        // THE LINE THIS MODULE EXISTS FOR: the recipe says what it NEEDS, the index says WHERE.
        // No place is named, here or in the table. Swap the machine for another kind that offers the
        // same capability and this does not notice.
        const int spot = tn_find_workspot(i, cap);
        if (spot < 0) continue;                    // nothing in the building provides it; -1 is an
                                                   // honest answer and the order stays posted
        if (!tnk_spot_free(spot, cap) || tnk_spot_reserved(spot, ord)) continue;     // taken: contention

        tnk_claims++;
        tn_order[ord].claimed_by = (signed char)i;
        tn_order[ord].at_obj     = (signed char)spot;   // NOTE: signed char in the contract — see report
        *sh = (TnkShift){ (signed char)ord, (short)spot, TNK_PH_TRAVEL, 0 };
        a->bid_tag    = (unsigned char)cap;
        a->bid_score  = rc->minutes;
        a->target_obj = (signed char)spot;
        a->activity   = TN_ACT_WALK;               // the agents module owns the walking from here
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SELFCHECK — runtime/spec.h's "SPECS ON AN INCLUDEABLE" pattern: a shared header cannot define
// spec() (one per cart), so it exposes its own assertions and the cart's spec() calls them. Live
// only under -DDE_SPEC. Every case is built with the SAME spawn functions the game uses, so a
// scenario cannot drift from the game.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DE_SPEC
static char tnk_sp[200];

static int tnk_sc_money(void) {
    int t = 0; for (int h = 0; h < tn_house_n; h++) t += tn_house[h].money; return t;
}
// One machine, one resident, empty purse. `kind` is a PARAMETER on purpose: the same scenario is
// run with two different object kinds and must come out identical.
static void tnk_sc_build(int kind, int agents) {
    tn_world_init();
    tn_obj_n = 0; tn_agent_n = 0; tn_item_n = 0; tn_order_n = 0;
    tnk_shifts_done = tnk_shifts_lost = tnk_goods_made = tnk_claims = 0;
    tn_add_obj(kind, 2, 2, -1);                            // communal
    for (int i = 0; i < agents; i++) {
        tn_add_agent(i, 1, 2 + i);                         // one per household, so each has an order
        for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[i].need[n] = 255;   // nothing outbids work
    }
    for (int h = 0; h < tn_house_n; h++) tn_house[h].money = 0;
}
static void tnk_sc_run(int minutes) {
    for (int m = 0; m < minutes; m++) { tn_agents_tick(); tn_work_tick(); }
}

void tn_work_selfcheck(void) {
    const TnRecipe *rc = &TN_RECIPES[0];
    const int PAY = rc->out_value;

    // ── W1: ORDERS ARE POSTED, AND POSTING IS NOT A LEAK ────────────────────
    tnk_sc_build(TN_OBJ_LOOM, 1);
    tnk_sc_run(1);
    snprintf(tnk_sp, sizeof tnk_sp, "W1: one standing order per household per recipe (%d orders for "
             "%d households x %d recipes)", tn_order_n, tn_house_n, TN_RECIPE_N);
    expect(tn_order_n == tn_house_n * TN_RECIPE_N, tnk_sp);
    {
        int n_before = tn_order_n;
        tnk_sc_run(300);
        expect(tn_order_n == n_before, "W1: a standing order is re-used, not re-posted (the table "
                                       "does not grow by one every minute)");
    }

    // ── W2: AN UNCLAIMED ORDER NAMES NO PLACE ───────────────────────────────
    // The navkit lesson as an assertion. A recipe carries a capability and an order carries a
    // household; neither knows a location until the index resolves one at claim time.
    tnk_sc_build(TN_OBJ_WARDROBE, 1);                  // offers storage only: no capability at all
    tnk_sc_run(200);
    {
        int posted = 0, placed = 0;
        for (int o = 0; o < tn_order_n; o++) { posted++; if (tn_order[o].at_obj >= 0) placed++; }
        snprintf(tnk_sp, sizeof tnk_sp, "W2: with nothing providing the capability the order stays "
                 "POSTED and PLACELESS (%d posted, %d resolved to a place)", posted, placed);
        expect(posted == tn_house_n * TN_RECIPE_N && placed == 0, tnk_sp);
    }
    expect(tn_find_workspot(0, (TnTag)rc->needs_cap) == -1,
           "W2: and the index refuses to fall back on something merely nearby");
    expect(tnk_sc_money() == 0 && tnk_goods_made == 0,
           "W2: no capability, no good, no money — the seam cannot be reached another way");

    // ── W3: THE CAPABILITY IS THE ONLY THING THAT MATTERS ───────────────────
    // THE CASE THIS MODULE IS FOR. Run the identical scenario twice with two DIFFERENT object
    // kinds, whose only shared property is that both offer TN_CAP_WORK. Same order, same
    // resolution, same good, same money. If work ever grows a per-kind path, these diverge.
    expect(TN_OBJ_LOOM != TN_OBJ_COUNTER &&
           tn_offers(0, (TnTag)rc->needs_cap, NULL) == false,   /* obj 0 here is a WARDROBE */
           "W3 setup: the two machines about to be swapped really are different kinds");
    int loom_spot, loom_money, loom_shifts;
    tnk_sc_build(TN_OBJ_LOOM, 1);
    tnk_sc_run(600);
    loom_spot = tn_order[0].at_obj; loom_money = tnk_sc_money(); loom_shifts = tnk_shifts_done;
    snprintf(tnk_sp, sizeof tnk_sp, "W3: machine A stands one shift and sells one good (%d shifts, "
             "%d money at %d/good)", loom_shifts, loom_money, PAY);
    expect(loom_shifts == 1 && loom_money == PAY, tnk_sp);

    tnk_sc_build(TN_OBJ_COUNTER, 1);
    tnk_sc_run(600);
    snprintf(tnk_sp, sizeof tnk_sp, "W3: machine B, a DIFFERENT KIND ENTIRELY, produces the same "
             "outcome (%d/%d shifts, %d/%d money)", tnk_shifts_done, loom_shifts,
             tnk_sc_money(), loom_money);
    expect(tnk_shifts_done == loom_shifts && tnk_sc_money() == loom_money, tnk_sp);
    expect(tn_offers(0, (TnTag)rc->needs_cap, NULL),
           "W3: because the ONLY property the recipe ever asked about is the capability it offers");

    // ── W4: THE SHIFT IS THE RECIPE'S LENGTH, NOT THE WORKSPOT'S ────────────
    // The trap this pins: one workspot advertises its capability with minutes 0. If the stand were
    // timed from the OFFER, that machine would finish a shift the tick it began and mint value
    // every minute — a money fountain out of a table nobody thought was about work.
    {
        const TnOffer *cap_of = tnk_offer(0, (TnTag)rc->needs_cap);   // obj 0 is machine B
        expect(cap_of && cap_of->minutes == 0,
               "W4 setup: machine B advertises its capability with minutes 0, so this case is real");
    }
    tnk_sc_build(TN_OBJ_COUNTER, 1);
    tnk_sc_run(400);
    snprintf(tnk_sp, sizeof tnk_sp, "W4: 400 minutes in, a %d-minute shift has produced nothing "
             "(%d goods)", rc->minutes, tnk_goods_made);
    expect(tnk_goods_made == 0 && tnk_sc_money() == 0, tnk_sp);
    tnk_sc_run(200);
    expect(tnk_goods_made == 1, "W4: and at 600 it has produced exactly one, on the RECIPE's clock");

    // ── W5: VALUE CONSERVATION AT THE SEAM ──────────────────────────────────
    // Money is exactly shifts x out_value. Catches a double sale, a sale without a shift, and a
    // shift that pays twice — none of which any other check here would notice.
    tnk_sc_build(TN_OBJ_LOOM, 1);
    tnk_sc_run(3000);
    snprintf(tnk_sp, sizeof tnk_sp, "W5: money is EXACTLY shifts x value, no more and no less "
             "(%d shifts, %d goods, %d money)", tnk_shifts_done, tnk_goods_made, tnk_sc_money());
    expect(tnk_shifts_done > 1 && tnk_goods_made == tnk_shifts_done &&
           tnk_sc_money() == tnk_shifts_done * PAY, tnk_sp);
    expect(tnk_shifts_lost == 0, "W5: and no shift was stood and then thrown away");

    // ── W6: v1 SELLS AT THE MACHINE (CURRENT BEHAVIOUR, MARKED) ─────────────
    // Asserted as CURRENT, not desired, in the style of the cart's case 8: a gap a test describes
    // is a work item, a gap in a comment is folklore. store.h is a stub, so a good with nowhere to
    // go would fill tn_item[] and silently stop production. When hauling lands, FLIP THIS: the item
    // stays carried, store finds it a home, and the sale moves.
    expect(tn_item_n == 0 && tn_agent[0].carrying == -1,
           "W6 (KNOWN GAP): the good is sold at the machine and nothing is left to haul, because "
           "store.h is a stub. Flip this when §6 hauling lands.");

    // ── W7: CONTENTION, WITH NO QUEUE CODE ANYWHERE ─────────────────────────
    // Two households, two orders, ONE capacity-1 machine. Exactly one shift can be in flight, and
    // the loser's order stays posted rather than sending it across the building to be turned away.
    tnk_sc_build(TN_OBJ_LOOM, 2);
    tnk_sc_run(30);
    {
        int claimed = 0, working = 0;
        for (int o = 0; o < tn_order_n; o++) if (tn_order[o].claimed_by >= 0) claimed++;
        for (int i = 0; i < tn_agent_n; i++)
            if (tn_agent[i].activity == TN_ACT_USE && tn_agent[i].bid_tag == rc->needs_cap) working++;
        snprintf(tnk_sp, sizeof tnk_sp, "W7: one machine, two households, one worker (%d orders "
                 "claimed, %d standing a shift)", claimed, working);
        expect(claimed == 1 && working == 1, tnk_sp);
        // The sharp half: the loser must never have STARTED. A claim it has to abandon on arrival
        // is a walk across the building for nothing, and it repeats every minute — invisible in
        // every other number here, which is why the claim count is a number at all.
        snprintf(tnk_sp, sizeof tnk_sp, "W7: and the loser never set off (%d claims in 30 minutes, "
                 "not one wasted commute)", tnk_claims);
        expect(tnk_claims == 1, tnk_sp);
    }
    tnk_sc_run(570);
    expect(tnk_shifts_done == 1 && tnk_sc_money() == PAY,
           "W7: and only one good came out of it, so contention cost production rather than "
           "duplicating it");

    // ── W8: WHOEVER GETS THERE FIRST KEEPS IT (CURRENT BEHAVIOUR, MARKED) ───
    // Found by running: the finisher is re-offered the machine the instant it is free, because
    // nothing anywhere is a term for fairness or for waiting. In the real building needs pull a
    // worker away and it alternates, but in a bare scenario one household takes every shift. Same
    // shape as the cart's ownership gap: real, in the model rather than the code, and asserted so
    // that whoever adds order priority has to come here.
    tnk_sc_run(600);
    snprintf(tnk_sp, sizeof tnk_sp, "W8 (KNOWN GAP): both shifts' pay landed in ONE household "
             "(%d / %d) — nothing scores fairness or waiting time. Flip this when order priority "
             "lands.", tn_house[0].money, tn_house[1].money);
    expect(tnk_shifts_done == 2 && tnk_sc_money() == 2 * PAY &&
           (tn_house[0].money == 2 * PAY || tn_house[1].money == 2 * PAY), tnk_sp);

    // ── W9: WORK NEVER STEALS SOMEBODY MID-USE ──────────────────────────────
    // The leak this guards: `users` is incremented and decremented by the agents module's USE case.
    // Redirecting a resident out of USE would leave the count high forever and quietly make the
    // object unusable for the rest of the run — no crash, no warning, just a dead bed.
    tn_world_init();
    tn_obj_n = 0; tn_agent_n = 0; tn_item_n = 0; tn_order_n = 0;
    tnk_shifts_done = tnk_shifts_lost = tnk_goods_made = 0;
    tn_add_obj(TN_OBJ_BED,  2, 2, 0);                  // a need, strongly bid
    tn_add_obj(TN_OBJ_LOOM, 3, 2, -1);                 // and a machine right next to it
    tn_add_agent(0, 1, 2);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
    tn_agent[0].need[TN_SERVE_REST] = 60;              // a bid far above TNK_WORK_BID
    tnk_sc_run(5);
    expect(tn_agent[0].activity == TN_ACT_USE && tn_agent[0].target_obj == 0,
           "W9 setup: a need bid above the work threshold wins, and the resident is mid-USE");
    tnk_sc_run(100);
    expect(tn_agent[0].activity == TN_ACT_USE && tn_agent[0].target_obj == 0 && tn_obj[0].users == 1,
           "W9: work does not interrupt it, and the capacity count stays at exactly one");
    {
        int claimed = 0;
        for (int o = 0; o < tn_order_n; o++) if (tn_order[o].claimed_by >= 0) claimed++;
        expect(claimed == 0, "W9: and no order was claimed by someone who was already busy");
    }

    // ── W10: THE WHOLE BUILDING, RUN LONG ───────────────────────────────────
    // The integration case. Work has to actually happen in the real level (not only in a scenario
    // with nothing else to do), money has to arrive only through the seam, and the capacity counts
    // have to balance every single minute.
    //
    // The invariant that matters is CONSERVATION, not a ceiling: sum(users) == the number of
    // residents in TN_ACT_USE. A ceiling check (users <= capacity) is BLIND to the leak this
    // module could actually cause, because a stolen mid-USE resident leaves the count stuck AT
    // capacity, not above it — the object goes quietly dead and nothing complains. Checked every
    // minute rather than at the end, since a leak that later cancels out would otherwise hide.
    tn_world_init();
    tnk_shifts_done = tnk_shifts_lost = tnk_goods_made = 0;
    {
        const int money0 = tnk_sc_money();
        int over = 0, unbalanced = 0;
        for (int m = 0; m < 4000; m++) {
            tn_agents_tick(); tn_work_tick();
            int held = 0, using = 0;
            for (int o = 0; o < tn_obj_n; o++) {
                int cap = 0;
                for (int k = 0; k < TN_OFFER_N[tn_obj[o].kind]; k++)
                    if (TN_OFFERS[tn_obj[o].kind][k].capacity > cap)
                        cap = TN_OFFERS[tn_obj[o].kind][k].capacity;
                if (tn_obj[o].users > cap) over++;
                held += tn_obj[o].users;
            }
            for (int i = 0; i < tn_agent_n; i++) if (tn_agent[i].activity == TN_ACT_USE) using++;
            if (held != using) unbalanced++;
        }
        const int earned = tnk_sc_money() - money0;
        snprintf(tnk_sp, sizeof tnk_sp, "W10: in the real building residents work between needs "
                 "(%d shifts, %d earned over %d days)", tnk_shifts_done, earned, tn_clock.day - 1);
        expect(tnk_shifts_done >= 1 && earned == tnk_shifts_done * PAY, tnk_sp);
        snprintf(tnk_sp, sizeof tnk_sp, "W10: every minute of 4000, sum(users) == residents in USE "
                 "and nothing over capacity (%d unbalanced, %d over) — work leaked no count",
                 unbalanced, over);
        expect(unbalanced == 0 && over == 0, tnk_sp);
    }

    tn_world_init();
}
#endif // DE_SPEC

#endif // TENEMENT_WORK_H
