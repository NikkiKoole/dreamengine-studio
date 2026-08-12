// ─────────────────────────────────────────────────────────────────────────────
// tenement/econ.h — households, money, rent, bills, buying (design §5).
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tne_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_ECON_H
#define TENEMENT_ECON_H

// Money is a per-household quantity with SOURCES and SINKS and nothing else. There is exactly one
// source, `tn_sell()` (TN_SEAM_EXTERNAL, owned by the offer module), and this file owns the sinks:
// rent, bills, and buying objects. Symmetrically, there is exactly ONE function in here that takes
// money out of a purse, `tne_charge()`, so the sink side can be found the same way the source can.
//
// NOTHING SCORES ANY OF IT. Design §8a leaves the win condition open on purpose, so this module
// reports numbers and never draws a conclusion from them: no eviction, no game over, no score, no
// "you lose". The open questions it ran into are written down at the bottom, unanswered.

// ── the calendar (seam 6): rent day is a DATE, not an elapsed-hours counter ──
// A bare hour counter would work today and would have to be thrown away the moment seasons or a
// weekly market exist. These are dates in `tn_clock.day`, which is what seam 6 bought.
#define TNE_RENT_EVERY   7            // rent falls due every 7th day (day 7, 14, 21, …)
#define TNE_RENT_MINUTE  (9 * 60)     // 09:00 — collected in the morning, so it is a visible event
#define TNE_BILL_EVERY   7            // bills are weekly too …
#define TNE_BILL_DAY_OF  3            // … but land on day 3, 10, 17: a SEPARATE, distinguishable event
#define TNE_BILL_MINUTE  (18 * 60)    // 18:00
#define TNE_UPKEEP_PCT   5            // a thing costs this % of its own price to keep, per bill
#define TNE_PURSE_MAX    32767        // TnHousehold.money and the arrears below are both `short`

// Module-private ledger. None of this is in the contract, and it deliberately stays here until
// something outside actually needs it (see "what this module wanted from the contract", bottom).
static short tne_arrears[TN_MAX_HOUSEHOLDS];   // charged but not paid. A RECORD, not a punishment.
static int   tne_sunk;                         // every coin that has left a purse, ever
static short tne_rent_day;                     // last day rent was collected  (0 = never)
static short tne_bill_day;                     // last day bills were collected
static short tne_seen_day;                     // latest day seen, to notice a rewound calendar

// A new world is a new game, so the ledger must not be inherited from the last one. The contract
// has no per-module reset hook and `tn_world_init()` belongs to the world module, so this is both
// callable directly and triggered automatically when the calendar goes BACKWARDS — which is the
// one observable trace a rebuild leaves behind. Without it a scenario that ends on day 7 and then
// rebuilds would find rent day 7 already marked paid, and the first rent of the new world would be
// silently free.
void tn_econ_reset(void) {
    for (int h = 0; h < TN_MAX_HOUSEHOLDS; h++) tne_arrears[h] = 0;
    tne_sunk = 0;
    tne_rent_day = tne_bill_day = tne_seen_day = 0;
}

// Saturating, so a household that never pays accumulates a debt that stops growing instead of
// wrapping to a large NEGATIVE debt (i.e. a windfall) some hundreds of days in. `money` is a
// `short` and so is this, on purpose: the two numbers are meant to be read against each other.
static short tne_owe_add(short cur, int add) {
    const int v = (int)cur + add;
    return (short)(v > TNE_PURSE_MAX ? TNE_PURSE_MAX : (v < 0 ? 0 : v));
}

// THE ONE PLACE MONEY LEAVES A PURSE — the mirror of tn_sell(), the one place it enters.
// Returns what was ACTUALLY taken, which is not always what was due: a purse holds what it holds.
// The unpaid remainder is recorded in `tne_arrears` and NOTHING ACTS ON IT (design §8a).
//
// The purse floors at zero rather than going negative. That is a representation choice, not a
// consequence: a purse holds coins and cannot hold minus four of them, and debt has its own number
// so "cannot pay" is legible without this module inventing what happens next.
static int tne_charge(int household, int due) {
    if (household < 0 || household >= tn_house_n || due <= 0) return 0;
    const int have = tn_house[household].money > 0 ? tn_house[household].money : 0;
    const int pay  = due < have ? due : have;
    if (pay > 0) tn_house[household].money = (short)(tn_house[household].money - pay);
    tne_sunk += pay;                                   // conservation: what left a purse is counted
    if (pay < due) tne_arrears[household] = tne_owe_add(tne_arrears[household], due - pay);
    return pay;
}

// ── bills: what you own costs money to keep ─────────────────────────────────
// Pure, so the HUD (or a spec) can ask what a household owes without charging it.
//
// The SAME formula for every object, read out of the contract's own price table. Indexing
// TN_OBJ_PRICE by kind is a data lookup, which is what the contract says kind is FOR; it is not a
// branch on kind (rule 2), and a new object kind is therefore a table row (rule 3) and never a case
// in here. Nothing in this file asks what an object IS.
//
// Communal objects (household -1, design §6) are billed to nobody. The building's own loom is the
// building's expense, and rent is what covers it. If communal upkeep should be apportioned across
// the tenants, that is an economy question, not a bookkeeping one — see the open questions.
int tn_econ_upkeep(int household) {
    if (household < 0 || household >= tn_house_n) return 0;
    int total = 0;
    for (int o = 0; o < tn_obj_n; o++) {
        if (tn_obj[o].household != household) continue;
        total += TN_OBJ_PRICE[tn_obj[o].kind] * TNE_UPKEEP_PCT / 100;
    }
    return total;
}

// ── buying: the seam a future `build` module calls ──────────────────────────
// Deliberately two functions, because the two questions are asked in different places: the UI wants
// to know whether an object is affordable (to grey a button out) long before anybody commits.
bool tn_can_afford(int household, int kind) {
    if (household < 0 || household >= tn_house_n)   return false;
    if (kind < 0 || kind >= TN_OBJ_KIND_COUNT)      return false;
    return tn_house[household].money >= TN_OBJ_PRICE[kind];
}

// Buy one object and place it. Returns the new object index, or -1 having charged NOTHING.
// PLACE FIRST, THEN CHARGE: the world can be full, and a household that pays for a fridge that
// does not exist has had money destroyed by a bug rather than by a sink.
//
// There is no registration step and there could not be one: the moment the object exists it is in
// the offer index like any other, so a bought toilet is biddable and billable immediately. That is
// the whole payoff of the one principle, and spec() pins it.
int tn_buy_obj(int household, int kind, int tx, int ty) {
    if (!tn_can_afford(household, kind)) return -1;
    const int o = tn_add_obj(kind, tx, ty, household);
    if (o < 0) return -1;
    tne_charge(household, TN_OBJ_PRICE[kind]);         // affordable by the check above, so paid in full
    return o;
}

// ── readers, so nothing outside has to reach into the statics ───────────────
int tn_econ_arrears(int household) {
    if (household < 0 || household >= TN_MAX_HOUSEHOLDS) return 0;
    return tne_arrears[household];
}
int tn_econ_sunk(void)     { return tne_sunk; }        // total money destroyed by rent/bills/purchases
bool tn_econ_rent_day(void) { return tn_clock.day >= 1 && tn_clock.day % TNE_RENT_EVERY == 0; }
bool tn_econ_bill_day(void) { return tn_clock.day >= 1 && tn_clock.day % TNE_BILL_EVERY == TNE_BILL_DAY_OF; }

// ─────────────────────────────────────────────────────────────────────────────
// The tick. Rent and bills are DATED EVENTS, each fired at most once per day by a watermark rather
// than by an exact minute match. That matters for a reason worth stating: this is called once per
// simulated minute today, but nothing guarantees the sim visits 09:00 exactly (a coarser step, a
// resumed save, a spec driving the clock straight to an afternoon). A watermark charges rent for
// the day the first time it is asked on or after the hour; an equality test would silently skip a
// whole rent day instead.
// ─────────────────────────────────────────────────────────────────────────────
void tn_econ_tick(void) {
    if (tn_clock.day < tne_seen_day) tn_econ_reset();  // the calendar rewound: a new world
    tne_seen_day = tn_clock.day;

    if (tn_econ_rent_day() && tn_clock.minute >= TNE_RENT_MINUTE && tne_rent_day != tn_clock.day) {
        tne_rent_day = tn_clock.day;
        for (int h = 0; h < tn_house_n; h++) tne_charge(h, tn_house[h].rent);
    }
    if (tn_econ_bill_day() && tn_clock.minute >= TNE_BILL_MINUTE && tne_bill_day != tn_clock.day) {
        tne_bill_day = tn_clock.day;
        for (int h = 0; h < tn_house_n; h++) tne_charge(h, tn_econ_upkeep(h));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OPEN QUESTIONS THIS MODULE HIT AND DID NOT ANSWER
//
// These are all one question wearing different hats — "what does money DO to you" — and design §8a
// says it is undecided. Writing them down is the job; answering one here would quietly install a
// win condition in the accounts department.
//
//  1. What does unpaid rent eventually cause? Nothing reads `tne_arrears`. Eviction, a landlord's
//     patience, a repossessed sofa, or nothing at all are all still open.
//  2. Are arrears ever COLLECTED? Today they only accumulate: a household that comes into money
//     does not have last month's rent taken out of it. "What you owe is added to what's due" is
//     the obvious other answer and it changes the feel of the whole economy, so it is a decision,
//     not a detail.
//  3. Who receives rent? There is no landlord purse, so rent DESTROYS money — which §5 explicitly
//     sanctions ("sinks for money: rent, bills, and buying objects") and `tne_sunk` keeps auditable.
//     If the landlord ever becomes a household, this becomes a transfer and only tne_charge moves.
//  4. Should a household in arrears be allowed to buy a sofa? `tn_buy_obj` says yes, because saying
//     no is a rule about consequences. Flag rather than fix.
//  5. Vacancy. `TnHousehold.members`/`member_n` exist in the contract but NOTHING populates them
//     (tn_add_agent does not), so every household currently reads as having zero members. Rent is
//     therefore charged to every household in [0, tn_house_n) and never gated on occupancy — a gate
//     on an always-zero field would make rent free forever and look like it worked. If "an empty
//     flat pays no rent" is wanted, populating members comes first.
//  6. With `work` still a stub, nothing calls tn_sell, so NO money enters the world and every
//     household eventually runs dry no matter how well the building is planned. That is expected
//     for the slice, and it is precisely why the consequence of not paying must stay open: right
//     now every consequence would fire on everyone.
//
// WHAT THIS MODULE WANTED FROM THE CONTRACT AND WORKED AROUND (model.h is frozen, not edited):
//  • `TnHousehold` has no arrears field, so the debt lives in a module-private array and the HUD
//     cannot draw it. One `short arrears;` on the struct would fix that, and the HUD showing "owes
//     15" is the kind of legibility design §1 asks for. Note the hud module is included AFTER this
//     file, so it can already call tn_econ_arrears() in the meantime.
//  • No per-module reset hook on `tn_world_init()`, hence the rewound-calendar detection above.
//  • `tn_buy_obj`/`tn_can_afford` are declared here rather than in the contract, so only modules
//     included after econ.h can see them. When `build` lands they belong in model.h.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// SPEC — this module's own assertions, per runtime/spec.h "SPECS ON AN INCLUDEABLE". A shared
// header cannot define spec() (one per cart), so it exposes a selfcheck the cart's spec() calls.
// Lives here rather than in the cart so six module authors are not appending to one file.
//
// Each case is written to be able to FAIL. The bookkeeping identity
//     sum(purses) + sunk  ==  sum(purses at the start) + everything tn_sell() ever added
// is the one that would catch a real bug: a charge that floors a purse without recording where the
// coins went, or a purchase that debits and then fails to place, both break it.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DE_SPEC
static char tne_sp[160];

// Drive the calendar straight to a date. Rent and bills are DATED events, so the cheap honest way
// to test them is to set the date, exactly as a resumed save or a coarser timestep would.
static void tne_at(int day, int minute) {
    tn_clock.day = (short)day; tn_clock.minute = (short)minute;
    tn_econ_tick();
}
static int tne_purses(void) {
    int t = 0;
    for (int h = 0; h < tn_house_n; h++) t += tn_house[h].money;
    return t;
}

void tn_econ_selfcheck(void) {
    // ── ECON A: RENT IS A DATE, AND IT IS CHARGED ONCE ──────────────────────
    // No objects, so upkeep is zero everywhere and the arithmetic below is rent and only rent.
    tn_world_init(); tn_obj_n = 0; tn_econ_reset();
    const int r0 = tn_house[0].rent, m0 = tn_house[0].money;
    expect(r0 > 0 && m0 > r0 * 3 && tn_econ_upkeep(0) == 0,
           "econ A setup: households start solvent and owe rent, and own nothing (so bills are 0)");

    tne_at(1, 1439);
    expect(tn_house[0].money == m0, "econ A: day 1 is not a rent day, however late in the day it gets");
    tne_at(TNE_RENT_EVERY, TNE_RENT_MINUTE - 1);
    expect(tn_house[0].money == m0, "econ A: on the rent day, before the rent hour, nothing is taken");
    tne_at(TNE_RENT_EVERY, TNE_RENT_MINUTE);
    expect(tn_house[0].money == m0 - r0, "econ A: at the rent hour, exactly the rent is taken");

    for (int i = 0; i < 200; i++) tne_at(TNE_RENT_EVERY, TNE_RENT_MINUTE + i);
    snprintf(tne_sp, sizeof tne_sp,
             "econ A: rent is charged ONCE per rent day, not once per tick (money %d, one rent is %d)",
             tn_house[0].money, r0);
    expect(tn_house[0].money == m0 - r0, tne_sp);

    for (int d = TNE_RENT_EVERY + 1; d < TNE_RENT_EVERY * 2; d++) tne_at(d, 1439);
    expect(tn_house[0].money == m0 - r0, "econ A: the days between rent days cost nothing");
    tne_at(TNE_RENT_EVERY * 2, 14 * 60);          // an AFTERNOON visit, never landing on the rent hour
    expect(tn_house[0].money == m0 - 2 * r0,
           "econ A: a rent day caught late still charges — a watermark, not an exact-minute match");

    // ── ECON B: BILLS FOLLOW WHAT A HOUSEHOLD OWNS ──────────────────────────
    tn_world_init(); tn_obj_n = 0; tn_econ_reset();
    expect(tn_econ_upkeep(0) == 0 && tn_econ_upkeep(1) == 0 && tn_house[1].money == m0,
           "econ B: a household that owns nothing owes no upkeep");
    tn_add_obj(TN_OBJ_FRIDGE, 1, 1, 0);
    tn_add_obj(TN_OBJ_BED,    1, 2, 0);
    const int want0 = TN_OBJ_PRICE[TN_OBJ_FRIDGE] * TNE_UPKEEP_PCT / 100
                    + TN_OBJ_PRICE[TN_OBJ_BED]    * TNE_UPKEEP_PCT / 100;
    snprintf(tne_sp, sizeof tne_sp, "econ B: upkeep is a share of each owned object's own price (%d)",
             tn_econ_upkeep(0));
    expect(tn_econ_upkeep(0) == want0 && want0 > 0, tne_sp);

    tn_add_obj(TN_OBJ_LOOM, 6, 4, -1);            // communal: the building's, not a tenant's
    expect(tn_econ_upkeep(0) == want0 && tn_econ_upkeep(1) == 0,
           "econ B: a COMMUNAL object is billed to nobody, and never to a neighbour");
    tn_add_obj(TN_OBJ_TOILET, 11, 1, 1);
    const int want1 = TN_OBJ_PRICE[TN_OBJ_TOILET] * TNE_UPKEEP_PCT / 100;
    expect(tn_econ_upkeep(1) == want1 && tn_econ_upkeep(0) == want0,
           "econ B: bills follow ownership — household 1's toilet does not appear on household 0's bill");

    tne_at(TNE_BILL_DAY_OF, TNE_BILL_MINUTE - 1);
    expect(tn_house[0].money == m0, "econ B: before the bill hour, nothing is taken");
    expect(tn_econ_bill_day() && !tn_econ_rent_day(),
           "econ B: bill day and rent day are DIFFERENT dates, so neither hides inside the other");
    tne_at(TNE_BILL_DAY_OF, TNE_BILL_MINUTE);
    for (int i = 0; i < 50; i++) tne_at(TNE_BILL_DAY_OF, TNE_BILL_MINUTE + 3);
    expect(tn_house[0].money == m0 - want0 && tn_house[1].money == m0 - want1,
           "econ B: each household is billed its own upkeep, exactly once for the day");

    // ── ECON C: THE UNPAYABLE CHARGE NEITHER WRAPS NOR VANISHES ─────────────
    // The interesting failure is silent: `money` is a short, so an unchecked subtraction reads as a
    // fortune after a few hundred unpaid weeks, and an unrecorded shortfall makes the ledger lie.
    tn_world_init(); tn_obj_n = 0; tn_econ_reset();
    tn_house[0].money = (short)(r0 - 5);           // not quite enough for one rent
    const int short_by = 5, paid = r0 - 5;
    tne_at(TNE_RENT_EVERY, TNE_RENT_MINUTE);
    snprintf(tne_sp, sizeof tne_sp, "econ C: a household that cannot pay floors at 0 (money %d, arrears %d)",
             tn_house[0].money, tn_econ_arrears(0));
    expect(tn_house[0].money == 0, tne_sp);
    expect(tn_econ_arrears(0) == short_by,
           "econ C: the part that could not be paid is RECORDED as owed, not quietly forgiven");
    expect(tn_econ_sunk() == paid + tn_house[1].rent,
           "econ C: only coins that actually MOVED are in the ledger, not the amount that was due");

    {   // ~4000 unpaid rent days: the arrears must saturate, never wrap to a negative debt.
        const int sunk_before = tn_econ_sunk(), h1_before = tn_house[1].money;
        for (int i = 2; i < 4000; i++) tne_at(TNE_RENT_EVERY * i, TNE_RENT_MINUTE);
        snprintf(tne_sp, sizeof tne_sp,
                 "econ C: after ~4000 unpaid rent days the debt SATURATES at %d and never wraps negative",
                 tn_econ_arrears(0));
        expect(tn_econ_arrears(0) == TNE_PURSE_MAX, tne_sp);
        expect(tn_house[0].money == 0, "econ C: an empty purse stays at 0 across thousands of charges");
        expect(tn_house[1].money == 0 && h1_before > 0,
               "econ C: the solvent household drained to exactly 0 as well, and stopped there");
        expect(tn_econ_sunk() == sunk_before + h1_before,
               "econ C: the ledger holds every coin household 1 had and not one more (an empty purse pays nothing)");
    }

    // ── ECON D: NOTHING CREATES MONEY EXCEPT tn_sell() ──────────────────────
    // sum(purses) + sunk is invariant under rent, bills and purchases, and moves ONLY at the seam.
    tn_world_init(); tn_econ_reset();
    const int total0 = tne_purses();
    tne_at(TNE_BILL_DAY_OF,      TNE_BILL_MINUTE);
    tne_at(TNE_RENT_EVERY,       TNE_RENT_MINUTE);
    tne_at(TNE_BILL_DAY_OF + 7,  TNE_BILL_MINUTE);
    tne_at(TNE_RENT_EVERY * 2,   TNE_RENT_MINUTE);
    snprintf(tne_sp, sizeof tne_sp,
             "econ D: rent and bills DESTROY money and every coin is accounted for (%d + %d == %d)",
             tne_purses(), tn_econ_sunk(), total0);
    expect(tne_purses() + tn_econ_sunk() == total0, tne_sp);
    expect(tne_purses() < total0, "econ D: … and the week actually cost something, so that was not vacuous");

    tn_item[0] = (TnItem){ TN_STORE_GOODS, 12, -1, -1, 0, 0 };
    if (tn_item_n < 1) tn_item_n = 1;
    tn_sell(0, 0);                                 // TN_SEAM_EXTERNAL, the one source
    expect(tne_purses() + tn_econ_sunk() == total0 + 12,
           "econ D: the ONLY thing that raises the world's total is tn_sell(), by exactly the item's value");
    const int bought = tn_buy_obj(0, TN_OBJ_WARDROBE, 4, 4);
    expect(bought >= 0 && tne_purses() + tn_econ_sunk() == total0 + 12,
           "econ D: buying an object moves money to the ledger and creates none");

    // ── ECON E: BUYING ──────────────────────────────────────────────────────
    tn_world_init(); tn_obj_n = 0; tn_econ_reset();
    const int cheap = TN_OBJ_PRICE[TN_OBJ_TOILET], dear = TN_OBJ_PRICE[TN_OBJ_LOOM];
    expect(dear > cheap, "econ E setup: the price table really does have a dear object and a cheap one");
    tn_house[0].money = (short)cheap;              // EXACTLY enough
    expect(tn_can_afford(0, TN_OBJ_TOILET) && !tn_can_afford(0, TN_OBJ_LOOM),
           "econ E: exactly enough money is enough, and one coin short of a loom is not");
    expect(!tn_can_afford(-1, TN_OBJ_TOILET) && !tn_can_afford(0, TN_OBJ_KIND_COUNT),
           "econ E: an out-of-range household or kind cannot afford anything, rather than reading off the end");
    {
        const int o = tn_buy_obj(0, TN_OBJ_TOILET, 2, 3);
        expect(o == 0 && tn_obj_n == 1 && tn_obj[0].household == 0 && tn_house[0].money == 0,
               "econ E: a purchase places the object, debits exactly its price, and leaves no change");
        expect(tn_econ_upkeep(0) == cheap * TNE_UPKEEP_PCT / 100,
               "econ E: an object bought is an object BILLED — no separate registration step exists");
        // And it is in the offer index immediately, because there is nowhere else it could be.
        for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
        tn_agent[0].need[TN_SERVE_BLADDER] = 40;
        TnTag tag; int score;
        expect(tn_best_action(0, &tag, &score) == o && tag == TN_SERVE_BLADDER,
               "econ E: the object is biddable the moment it is bought (the one principle, paid for)");
    }
    expect(tn_buy_obj(0, TN_OBJ_TOILET, 5, 5) == -1 && tn_obj_n == 1 && tn_house[0].money == 0,
           "econ E: a purchase you cannot afford changes NOTHING — no object, no debt, no negative purse");

    {   // The world is full. This is the one that would catch a debit-then-fail ordering bug.
        tn_house[0].money = 30000;
        while (tn_add_obj(TN_OBJ_BED, 1, 1, 0) >= 0) { }
        const int before = tn_house[0].money, n_before = tn_obj_n, sunk_before = tn_econ_sunk();
        expect(n_before == TN_MAX_OBJECTS, "econ E setup: the world really is full");
        expect(tn_buy_obj(0, TN_OBJ_TOILET, 5, 5) == -1 && tn_house[0].money == before
               && tn_obj_n == n_before && tn_econ_sunk() == sunk_before,
               "econ E: a purchase that cannot be PLACED charges nothing (place first, then charge)");
    }

    // ── ECON F: A REBUILT WORLD DOES NOT INHERIT THE LEDGER ─────────────────
    // Deliberately does NOT call tn_econ_reset(), because that is the thing under test: the module
    // cannot hook tn_world_init(), so it notices the rewound calendar instead. Set up the exact
    // collision — a watermark on day 7, then a new world that also runs to day 7.
    tn_world_init(); tn_obj_n = 0; tn_econ_reset();
    tne_at(TNE_RENT_EVERY, TNE_RENT_MINUTE);
    expect(tn_house[0].money == m0 - r0, "econ F setup: rent taken, so day 7 is marked paid");
    tn_world_init(); tn_obj_n = 0;                 // a NEW world, no reset call
    tn_econ_tick();                                // day 1 again: the rewind must be noticed here
    tne_at(TNE_RENT_EVERY, TNE_RENT_MINUTE);
    expect(tn_house[0].money == m0 - r0,
           "econ F: a rebuilt world's first rent day is NOT silently free (the stale watermark cleared)");
    expect(tn_econ_sunk() == r0 + tn_house[1].rent && tn_econ_arrears(0) == 0,
           "econ F: and the new world starts on a clean ledger, not the last scenario's debt");

    tn_world_init(); tn_econ_reset();              // leave the world as we found it
}
#endif // DE_SPEC

#endif // TENEMENT_ECON_H
