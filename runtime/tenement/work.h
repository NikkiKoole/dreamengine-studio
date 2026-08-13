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
// stands the recipe's shift, and the shift mints one good INTO ITS HANDS. It is store.h that finds
// the good a shelf and econ.h's buyer that eventually pays for it, so making a thing and being paid
// for it are two events with a haul between them (they used to be one, at the machine).
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
// WHO stood them, which used to be readable from the pay: while work sold its own output, "both
// shifts' money landed in one purse" said everything about fairness. The sale moved to econ's buyer,
// so the money now says where the goods were SHELVED rather than who did the work. This counts the
// turns directly, which is what W8's fairness gap was always actually about.
static int tnk_shifts_by[TN_MAX_HOUSEHOLDS];

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
    if (sh->order >= 0 && sh->order < tn_order_n && tn_order[sh->order].claimed_by == (TnIdx)agent) {
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
// LOST rather than DONE. Without that the ledger would claim a good the world never got.
//
// ── THE FLIP HAPPENED (the two lines below this comment used to undo everything above them) ─────
// This function used to mint the good, sell it on the spot, and then DELETE it again — carrying
// back to -1 and tn_item_n decremented — with a note saying "WHEN store.h CAN HAUL, DELETE THE TWO
// LINES BELOW". store.h could haul: it had a full fetch/carry/put loop over BFS routes and thirty
// assertions. So the whole item economy was unreachable through a two-line stub, `TN_ACT_HAUL` was
// a state nothing could enter, and `TnAgent.carrying` was a field that was never once non-negative
// in a running game. Deleting those two lines is the entire change here, exactly as promised.
//
// The sale moved to the BUYER in econ.h, which is what "the sale moves to whoever empties the
// store" meant. tn_sell is still the one seam and still lives in offer.h; only its CALLER changed.
static int tnk_deliver(int agent, const TnRecipe *rc, int household) {
    (void)household;                     // the maker no longer sells: econ's buyer does, on its round
    // HANDS FULL IS AN ANSWER TOO, and this guard only became necessary with the flip above. While
    // the good was deleted the instant it was made, `carrying` could never already be occupied; now
    // it can, and the line below used to overwrite it — orphaning the previous item forever, since
    // its held_by still named this agent so no hauler would ever look at it again. A slow leak that
    // ends in a stopped economy, with nothing to see. One pair of hands, no stack sizes (store.h's
    // rule), so a shift finished with full hands is LOST, exactly as a shift against a full world is.
    if (tn_agent[agent].carrying >= 0) return 0;
    if (tn_item_n >= TN_MAX_ITEMS) return 0;                        // full is an answer, not a crash
    const int it = tn_item_n++;
    tn_item[it] = (TnItem){ rc->out_store_tag, rc->out_value, (TnIdx)agent, -1,
                            (unsigned char)tn_agent[agent].tx, (unsigned char)tn_agent[agent].ty };
    tn_agent[agent].carrying = (TnIdx)it;
    tnk_goods_made++;
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
                             tn_order[sh->order].claimed_by == (TnIdx)i;
            if (!live) { tnk_release(i); continue; }

            const TnRecipe *rc = &TN_RECIPES[tn_order[sh->order].recipe];
            // Re-assert the bid tag every tick: it is what the agents module's arrival check looks
            // the offer up by, and it is what the HUD prints. Saying WORK there is the point of
            // design §1 — visible labour that a stranger can read without a tooltip.
            a->bid_tag = (unsigned char)rc->needs_cap;

            if (a->activity == TN_ACT_USE && a->target_obj == sh->obj) {
                sh->phase = TNK_PH_SHIFT;
                if (sh->logged < 32000) sh->logged++;
                a->bid_score = rc->minutes - sh->logged;          // minutes left, on the HUD
                if (sh->logged >= rc->minutes) {
                    if (tnk_deliver(i, rc, tn_order[sh->order].household)) {
                        tnk_shifts_done++;
                        const int hh = tn_order[sh->order].household;
                        if (hh >= 0 && hh < TN_MAX_HOUSEHOLDS) tnk_shifts_by[hh]++;
                    }
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
            if (a->activity == TN_ACT_WALK && a->target_obj == sh->obj) continue;  // commuting

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
        tn_order[ord].at_obj     = (TnIdx)spot;   // TnIdx: the contract widened these, see model.h
        *sh = (TnkShift){ (signed char)ord, (short)spot, TNK_PH_TRAVEL, 0 };
        a->bid_tag    = (unsigned char)cap;
        a->bid_score  = rc->minutes;
        a->target_obj = (TnIdx)spot;
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
    for (int h = 0; h < TN_MAX_HOUSEHOLDS; h++) tnk_shifts_by[h] = 0;
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
// THE WHOLE CHAIN, in the cart's own tick order. Needed since the sale left this module: a good is
// now made here, carried, shelved by store, and only then sold by econ's buyer, so any case about
// what production is WORTH has to run all four. tnk_sc_run stays for the cases that are about work
// alone — and W10 uses the difference deliberately, as its proof that the seam really moved.
static void tnk_sc_run_full(int minutes) {
    for (int m = 0; m < minutes; m++) { tn_agents_tick(); tn_work_tick(); tn_econ_tick(); tn_store_tick(); }
}
// A machine AND somewhere to put what it makes, owned by household 0 so the buyer has someone to
// pay (it will not pay a communal shelf — see econ's buyer). The wardrobe is the goods store: it
// offers TN_STORE_GOODS, which is the row that let the loom's output exist at all.
static void tnk_sc_build_trade(int kind, int agents) {
    tnk_sc_build(kind, agents);
    tn_add_obj(TN_OBJ_WARDROBE, 4, 2, 0);
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
    // MEASURED IN GOODS, NOT IN MONEY, since the sale left this module (see tnk_deliver). That is a
    // sharper test of what W3 is actually about: two different kinds must produce the same OUTPUT,
    // and output is a good. Money would now be testing econ's buyer through three other modules.
    int loom_spot, loom_goods, loom_shifts;
    tnk_sc_build(TN_OBJ_LOOM, 1);
    tnk_sc_run(600);
    loom_spot = tn_order[0].at_obj; loom_goods = tnk_goods_made; loom_shifts = tnk_shifts_done;
    snprintf(tnk_sp, sizeof tnk_sp, "W3: machine A stands one shift and turns out one good (%d "
             "shifts, %d goods worth %d each)", loom_shifts, loom_goods, PAY);
    expect(loom_shifts == 1 && loom_goods == 1, tnk_sp);

    tnk_sc_build(TN_OBJ_COUNTER, 1);
    tnk_sc_run(600);
    snprintf(tnk_sp, sizeof tnk_sp, "W3: machine B, a DIFFERENT KIND ENTIRELY, produces the same "
             "outcome (%d/%d shifts, %d/%d goods)", tnk_shifts_done, loom_shifts,
             tnk_goods_made, loom_goods);
    expect(tnk_shifts_done == loom_shifts && tnk_goods_made == loom_goods, tnk_sp);
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

    // ── W5: PRODUCTION CONSERVATION ─────────────────────────────────────────
    // One shift, one good, every time. Catches a shift that produces twice, a good minted without a
    // shift, and a shift silently producing nothing. This used to read `money == shifts x value`;
    // the money half moved to econ's buyer with the sale (see econ's "buyer" cases), and asserting
    // it here would now be asserting three other modules through a keyhole. What is left is exactly
    // this module's own claim, and it is the sharper half: work makes GOODS, not money.
    tnk_sc_build_trade(TN_OBJ_LOOM, 1);
    tnk_sc_run_full(3000);
    snprintf(tnk_sp, sizeof tnk_sp, "W5: goods are EXACTLY shifts, no more and no less "
             "(%d shifts, %d goods worth %d each)", tnk_shifts_done, tnk_goods_made, PAY);
    expect(tnk_shifts_done > 1 && tnk_goods_made == tnk_shifts_done, tnk_sp);
    expect(tnk_shifts_lost == 0, "W5: and no shift was stood and then thrown away");

    // ── W5b: NO CUPBOARD MEANS NO INCOME, NOT NO WORK ───────────────────────
    // THE CASE THAT ONLY EXISTS BECAUSE GOODS ARE REAL, and the one that gives storage a job. Same
    // machine, same 3000 minutes, NO CUPBOARD. Production is completely unaffected — the residents
    // keep making bolts — and not one penny arrives, because the buyer only takes goods that are
    // SHELVED and there is no shelf. The bolts end up loose on the floor: store.h, finding nowhere
    // to put a thing down, drops it where it stands.
    //
    // WORTH READING TWICE, because the first draft of this case asserted the opposite and was
    // wrong: the guess was that full hands would STALL the factory. They do not. store.h frees the
    // hands by dropping the load, so the loop keeps turning and only the money stops. That is a
    // better story than a stall and it is visible in the room — a hallway filling up with unsold
    // goods — where a stalled machine would just look like a machine nobody uses.
    //
    // It is also the shape of an answer to "let something be LOST" (the punch list's open design
    // question): a household that never builds storage is never told off. It just stops earning,
    // and the evidence piles up on the floor where anyone can see it.
    tnk_sc_build(TN_OBJ_LOOM, 1);                        // a machine, and nowhere to put its output
    tnk_sc_run_full(3000);
    {
        int loose = 0;
        for (int i = 0; i < tn_item_n; i++)
            if (tn_item[i].store_tag == rc->out_store_tag &&
                tn_item[i].held_by < 0 && tn_item[i].stored_in < 0) loose++;
        snprintf(tnk_sp, sizeof tnk_sp, "W5b: with nowhere to shelve them the work still happens and "
                 "the goods pile up LOOSE — and nothing is earned (%d shifts, %d goods, %d on the "
                 "floor, %d money)", tnk_shifts_done, tnk_goods_made, loose, tnk_sc_money());
        expect(tnk_shifts_done > 1 && tnk_goods_made == tnk_shifts_done &&
               loose >= 1 && tnk_sc_money() == 0, tnk_sp);
    }

    // ── W6: THE GOOD SURVIVES ITS MAKER (WAS A KNOWN GAP; FLIPPED) ──────────
    // Run with tnk_sc_run (work only, no store tick) so the good is caught in the ONE state this
    // case is about: freshly made and still in its maker's hands. With the store tick running it
    // would already have been shelved or dropped, and the thing being asserted would be gone.
    tnk_sc_build(TN_OBJ_LOOM, 1);
    tnk_sc_run(600);
    // This case used to assert the OPPOSITE and say so: "the good is sold at the machine and
    // nothing is left to haul, because store.h is a stub. Flip this when §6 hauling lands." Hauling
    // had in fact landed — store.h carried a full fetch/haul/put loop over BFS routes and thirty
    // assertions — and only work.h's two-line stub stood between it and ever running. So the item
    // economy was unreachable, TN_ACT_HAUL was a state nothing could enter, and TnAgent.carrying was
    // never once non-negative in a running game.
    //
    // This is the flip, and it is the assertion that would catch the stub coming back: a real
    // tagged item, still in its maker's hands, waiting for somebody to put it somewhere.
    {
        int carried = 0, alive = 0;
        for (int i = 0; i < tn_item_n; i++) if (tn_item[i].store_tag != TN_ITEM_FREE) alive++;
        for (int i = 0; i < tn_agent_n; i++) if (tn_agent[i].carrying >= 0) carried++;
        snprintf(tnk_sp, sizeof tnk_sp, "W6: the good OUTLIVES the shift — it exists and is still "
                 "being carried, so store.h has something to haul (%d alive, %d in hands)",
                 alive, carried);
        expect(alive >= 1 && carried == 1 &&
               tn_item[tn_agent[0].carrying].store_tag == rc->out_store_tag, tnk_sp);
    }
    // The converse, and the reason the item survives at all: nothing in THIS module sells. If a
    // future edit re-adds a sale here, money appears with no buyer and this goes red.
    expect(tnk_sc_money() == 0,
           "W6: and work created no money doing it — the sale belongs to econ's buyer now, and "
           "this scenario never runs one");

    // ── W7: CONTENTION, WITH NO QUEUE CODE ANYWHERE ─────────────────────────
    // Two households, two orders, ONE capacity-1 machine. Exactly one shift can be in flight, and
    // the loser's order stays posted rather than sending it across the building to be turned away.
    // The full chain runs here (a cupboard, a hauler, a buyer) because W8 below needs a SECOND
    // shift to happen at all, and a worker cannot stand one with its hands still full.
    tnk_sc_build_trade(TN_OBJ_LOOM, 2);
    tnk_sc_run_full(30);
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
    tnk_sc_run_full(570);
    expect(tnk_shifts_done == 1 && tnk_goods_made == 1,
           "W7: and only one good came out of it, so contention cost production rather than "
           "duplicating it");

    // ── W8: THE MACHINE ALTERNATES — AND NOBODY MADE IT FAIR ────────────────
    // THIS CASE USED TO BE A KNOWN GAP, and hauling closed it without a line of fairness code.
    //
    // What it said before: "whoever gets there first keeps it — the finisher is re-offered the
    // machine the instant it is free, because nothing anywhere is a term for fairness or for
    // waiting", and in a bare scenario one household took every shift. That was true while a good
    // was sold where it was made, because a finisher had nothing else to do and simply turned round.
    //
    // Now the good is a real thing in its hands, so the finisher LEAVES to go and shelve it — and
    // the machine is free for the neighbour while it is gone. The turn-taking is a side effect of
    // an errand, which is the honest kind: nothing scores fairness, nothing queues, no priority was
    // added. The same shape as the design's other claims — behaviour out of interacting rules
    // rather than a rule about behaviour.
    //
    // Counted in SHIFTS STOOD rather than in pay, since pay left this module with the sale. It is
    // the sharper measure anyway: the complaint was never about money, it was about turns.
    // Still a REAL gap underneath, and it is now the one W8 leaves open: this alternates because
    // the winner is busy, NOT because waiting is worth anything. Give the loser a longer errand and
    // it starves again. The fix is still "longest wait wins", and it is still unwritten.
    tnk_sc_run_full(600);
    snprintf(tnk_sp, sizeof tnk_sp, "W8: two shifts, one machine, and they ALTERNATED (%d / %d) — "
             "because the finisher walks off to shelve its good, not because anything scores "
             "fairness", tnk_shifts_by[0], tnk_shifts_by[1]);
    expect(tnk_shifts_done == 2 && tnk_shifts_by[0] == 1 && tnk_shifts_by[1] == 1, tnk_sp);

    // ── W9: WORK NEVER STEALS SOMEBODY MID-USE ──────────────────────────────
    // The leak this guards: `users` is incremented and decremented by the agents module's USE case.
    // Redirecting a resident out of USE would leave the count high forever and quietly make the
    // object unusable for the rest of the run — no crash, no warning, just a dead bed.
    tn_world_init();
    tn_obj_n = 0; tn_agent_n = 0; tn_item_n = 0; tn_order_n = 0;
    tnk_shifts_done = tnk_shifts_lost = tnk_goods_made = 0;
    for (int h = 0; h < TN_MAX_HOUSEHOLDS; h++) tnk_shifts_by[h] = 0;
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
    for (int h = 0; h < TN_MAX_HOUSEHOLDS; h++) tnk_shifts_by[h] = 0;
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
        // GOODS, and NO money: this loop runs agents+work only, so the buyer never calls. That is
        // the point of checking `earned == 0` rather than dropping the money term — it proves the
        // seam really did move, in the one scenario big enough to have hidden a stray sale.
        snprintf(tnk_sp, sizeof tnk_sp, "W10: in the real building residents work between needs "
                 "(%d shifts, %d goods, %d money over %d days)", tnk_shifts_done, tnk_goods_made,
                 earned, tn_clock.day - 1);
        expect(tnk_shifts_done >= 1 && tnk_goods_made == tnk_shifts_done && earned == 0, tnk_sp);
        snprintf(tnk_sp, sizeof tnk_sp, "W10: every minute of 4000, sum(users) == residents in USE "
                 "and nothing over capacity (%d unbalanced, %d over) — work leaked no count",
                 unbalanced, over);
        expect(unbalanced == 0 && over == 0, tnk_sp);
    }

    tn_world_init();
}
#endif // DE_SPEC

#endif // TENEMENT_WORK_H
