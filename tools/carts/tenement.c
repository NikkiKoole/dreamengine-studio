/* de:meta
{
  "title": "tenement",
  "slug": "tenement",
  "kind": ["probe", "tech-demo"],
  "teaches": ["isometric-projection", "finite-state-ai"],
  "created": "2026-08-12",
  "lineage": "Thin vertical slice of the tenement sim: proves ONE claim, that a single argmax over every (object, need) pair produces different, better behaviour than the urgency-sort every other needs sim here uses. sims sorts needs then finds an object; this lets an adjacent nearly-free toilet outbid a distant fridge at higher hunger, which is what people recognise as Sims-like. Built against the frozen contract runtime/tenement/model.h before any fan-out, because that contract's centrepiece had never been exercised by a line of code.",
  "todo": [
    "SLICE, NOT THE GAME. Present: the offer index, needs decay, contention via queue penalty, and the spec that pins the argmax. Absent: households, money, work orders, storage, rent, building, pathfinding. Those are the fan-out.",
    "Distance is straight-line, not a path. Enough to prove the DECISION mechanism; the contention claim in design section 1 (corridors jam) needs real pathfinding before it can be judged.",
    "Split into the module files the contract names (runtime/tenement/*.h), one per future agent. work/econ/store are STUBS carrying their own briefs, waiting on the fan-out.",
    "The iso projection is COPIED from isoroom. Extract runtime/isoview.h when the second consumer proves the shape, per ADR-0006 (a library header wants real consumers first, not speculation).",
    "Objects are placeholders and the tag vocabulary is a first guess. Both should be revised by what the fan-out learns, not defended."
  ],
  "description": {
    "summary": "A few residents, some furniture, and one question: does the best offer on the table beat picking your worst need first? Watch the scores and see.",
    "detail": "The thin vertical slice of a bigger sim about several households sharing one building. Every object advertises what it offers, how strongly, for how long, and for how many people at once. Every resident does no thinking at all beyond taking the single best offer available, where a need's deficit is one term in the score rather than a filter applied first. That distinction is the whole point: the usual way to write this is to sort needs by urgency and then look for an object, which sends a hungry person past an empty toilet to a fridge on the far side of the building. Here the near thing can win. The HUD shows each resident's current winning bid and its score, because the interesting part of this simulation is invisible otherwise.",
    "controls": "Q/E turn the building. TAB shows every bid the winning resident considered, not just the winner. SPACE pauses. 1/2/4 set speed."
  }
}
de:meta */

// tenement — the cart shell. Input, entry points, and spec(). Everything else lives in the
// modules the contract names, one file each, so the fan-out can put one agent on each without
// two of them editing the same file. That was Phase 0 and the reason it had to be serial.
//
// Design: docs/design/tenement.md   Contract: runtime/tenement/model.h
//
// INCLUDE ORDER MATTERS and is the one thing to be careful about here. This is ONE translation
// unit: the contract first, then modules in dependency order (world defines the globals, offer
// prices them, agents drives them, art and hud draw them). A module never includes a sibling.

#include "studio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "spec.h"
#include "tenement/model.h"

int tnc_show_bids = 0;          // owner: cart. hud reads it.

#include "tenement/world.h"     // the globals + spawning + the level
#include "tenement/offer.h"     // THE index
#include "tenement/agents.h"    // decay + the state machine
#include "tenement/work.h"      // STUB — the `work` agent owns it
#include "tenement/econ.h"      // STUB — the `econ` agent owns it
#include "tenement/store.h"     // STUB — the `store` agent owns it
#include "tenement/art.h"       // the isometric view
#include "tenement/hud.h"       // the panels

// ── input ───────────────────────────────────────────────────────────────────
static int tnc_paused = 0, tnc_speed = 1;

void init(void) { colorkey(-1); tn_world_init(); }

void update(void) {
    if (keyp('Q')) tn_rot = (tn_rot + 3) & 3;
    if (keyp('E')) tn_rot = (tn_rot + 1) & 3;
    if (keyp(KEY_TAB)) tnc_show_bids = !tnc_show_bids;
    if (keyp(KEY_SPACE)) tnc_paused = !tnc_paused;
    if (keyp('1')) tnc_speed = 1;
    if (keyp('2')) tnc_speed = 2;
    if (keyp('4')) tnc_speed = 4;
    if (!tnc_paused)
        for (int s = 0; s < tnc_speed * 4; s++) {
            tn_agents_tick(); tn_work_tick(); tn_econ_tick(); tn_store_tick();
        }
    tn_camera();
#ifdef DE_TRACE
    watch("hunger0", "%d", tn_agent[0].need[TN_SERVE_HUNGER]);
    watch("act0",    "%d", tn_agent[0].activity);
    watch("bid0",    "%d", tn_agent[0].bid_score);
#endif
}

void draw(void) { cls(CLR_DARK_BLUE); tn_draw_world(); tn_draw_hud(); }

#ifdef DE_SPEC
static char sp[160];

void spec(void) {
    // ── CASE 1: ADVERTISEMENT IS NOT URGENCY-SORT ───────────────────────────
    // Build the scenario the two models disagree about. Hunger is the WORSE need, but the fridge
    // is across the building while a toilet is right here. Urgency-sort must pick the fridge
    // (biggest deficit first). Advertisement must pick the toilet (best offer on the table).
    // If this assertion ever flips, the design has silently become `sims`.
    tn_world_init();
    tn_obj_n = 0; tn_agent_n = 0;
    tn_add_obj(TN_OBJ_TOILET, 1, 1, 0);          // adjacent
    tn_add_obj(TN_OBJ_FRIDGE, 12, 8, 0);         // far corner
    tn_add_agent(0, 1, 2);
    tn_agent[0].need[TN_SERVE_HUNGER]  = 40;   // deficit 215  ← the MORE urgent need
    tn_agent[0].need[TN_SERVE_BLADDER] = 120;  // deficit 135
    for (int n = 0; n < TN_NEED_COUNT; n++)
        if (n != TN_SERVE_HUNGER && n != TN_SERVE_BLADDER) tn_agent[0].need[n] = 255;

    expect(255 - tn_agent[0].need[TN_SERVE_HUNGER] > 255 - tn_agent[0].need[TN_SERVE_BLADDER],
           "case 1 setup: hunger really is the more urgent need (so urgency-sort would pick it)");
    {
        TnTag tag; int score;
        const int o = tn_best_action(0, &tag, &score);
        snprintf(sp, sizeof sp, "the NEAR toilet outbids the FAR fridge despite lower urgency "
                                "(picked %s, score %d)", TAG_NAME[tag], score);
        expect(o == 0 && tag == TN_SERVE_BLADDER, sp);
    }
    // And the converse, so the test cannot pass by always preferring the toilet: move the fridge
    // next door and it must win, because now its bigger deficit is not paying a travel penalty.
    tn_obj[1].tx = 1; tn_obj[1].ty = 3;
    {
        TnTag tag; int score;
        const int o = tn_best_action(0, &tag, &score);
        snprintf(sp, sizeof sp, "with travel equalised the hungrier need wins (picked %s)", TAG_NAME[tag]);
        expect(o == 1 && tag == TN_SERVE_HUNGER, sp);
    }

    // ── CASE 2: CONTENTION IS IN THE SCORE ──────────────────────────────────
    // A capacity-1 object already in use must stop attracting anyone. This is what makes queues
    // and traffic emerge from the score instead of from queue-handling code.
    tn_obj_n = 0; tn_agent_n = 0;
    tn_add_obj(TN_OBJ_TOILET, 1, 1, 0);
    tn_add_obj(TN_OBJ_SOFA,   3, 1, 0);
    tn_add_agent(0, 1, 2);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
    tn_agent[0].need[TN_SERVE_BLADDER] = 60;
    tn_agent[0].need[TN_SERVE_FUN]     = 90;
    {
        TnTag tag; int free_score, busy_score;
        int o = tn_best_action(0, &tag, &free_score);
        expect(o == 0 && tag == TN_SERVE_BLADDER, "case 2: a free toilet wins while it is free");
        tn_obj[0].users = 1;                              // somebody is in there
        o = tn_best_action(0, &tag, &busy_score);
        snprintf(sp, sizeof sp, "an OCCUPIED capacity-1 object stops winning (was %d, now bids lower)",
                 free_score);
        expect(o == 1 && tag == TN_SERVE_FUN, sp);
        expect(busy_score < free_score, "case 2: the occupied object's own bid actually fell");
    }
    // A capacity-2 object is still usable by a second person. Sharing costs, but does not block.
    tn_obj[1].users = 1;                                   // sofa capacity 2
    {
        int strength = 0;
        expect(tn_offers(1, TN_SERVE_FUN, &strength) && strength == 90,
               "case 2: tn_offers reports the sofa's strength without anyone naming a sofa");
        TnTag tag; int score;
        expect(tn_best_action(0, &tag, &score) == 1 && tag == TN_SERVE_FUN,
               "case 2: a half-full capacity-2 object is still a valid offer");
    }

    // ── CASE 3: A SATED NEED MAKES NO BID ───────────────────────────────────
    tn_obj_n = 0; tn_agent_n = 0;
    tn_add_obj(TN_OBJ_TOILET, 1, 1, 0);
    tn_add_agent(0, 1, 2);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
    expect(tn_best_action(0, NULL, NULL) == -1,
           "case 3: a fully sated agent has nothing to do, and -1 says so honestly");

    // ── CASE 4: CAPABILITIES AND STORAGE NEVER BID FOR ATTENTION ────────────
    // The loom offers TN_CAP_WORK and the wardrobe TN_STORE_CLOTHES. Neither is a need, so neither
    // may ever be chosen by tn_best_action, however desperate the agent is. They are reachable
    // ONLY through the tag-specific lookups. This is the boundary that keeps one index serving
    // three consumers without them bleeding into each other.
    tn_obj_n = 0; tn_agent_n = 0;
    tn_add_obj(TN_OBJ_LOOM,     2, 2, -1);
    tn_add_obj(TN_OBJ_WARDROBE, 3, 2,  0);
    tn_add_agent(0, 1, 2);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 0;   // as desperate as possible
    expect(tn_best_action(0, NULL, NULL) == -1,
           "case 4: a loom and a wardrobe never bid for attention, even at zero needs");
    expect(tn_find_workspot(0, TN_CAP_WORK) == 0,
           "case 4: but the loom IS findable as a workspot, by capability");
    expect(tn_best_offer(0, TN_STORE_CLOTHES, NULL) == 1,
           "case 4: and the wardrobe IS findable as storage, by tag");
    expect(tn_find_workspot(0, TN_CAP_POWER) == -1,
           "case 4: a capability nothing provides resolves to -1, not to something nearby");

    // ── CASE 5: THE CONTRACT'S DATA TABLES AGREE WITH THEMSELVES ────────────
    {
        int bad = 0, needs = 0, caps = 0;
        for (int k = 0; k < TN_OBJ_KIND_COUNT; k++) {
            if (TN_OFFER_N[k] == 0 || TN_OFFER_N[k] > TN_MAX_OFFERS) bad++;
            for (int i = 0; i < TN_OFFER_N[k]; i++) {
                const TnOffer *of = &TN_OFFERS[k][i];
                if (of->tag >= TN_TAG_COUNT) bad++;
                if (of->capacity == 0) bad++;             // a capacity-0 object is unusable
                if (of->tag < TN_SERVE_COUNT) { needs++; if (of->strength <= 0) bad++; }
                else caps++;
            }
        }
        snprintf(sp, sizeof sp, "every object declares at least one valid offer (%d need bids, %d capabilities)",
                 needs, caps);
        expect(bad == 0, sp);
        expect(needs > 0 && caps > 0, "case 5: the one tag namespace really is carrying both kinds");
    }

    // ── CASE 6: THE 8-HOUR SHIFT SURVIVES THE TYPE ──────────────────────────
    // The contract originally had `minutes` as unsigned char and 480 wrapped to 224. Pin it.
    expect(TN_OFFERS[TN_OBJ_BED][0].minutes == 480, "case 6: a night's sleep is 480 minutes, not 224");
    expect(TN_RECIPES[0].minutes == 480,             "case 6: an 8-hour shift is 480 minutes, not 224");
    expect(TN_RECIPES[0].in_n == 0,
           "case 6: v1's recipe is the MARKED open loop (time -> good), per design section 5");

    // ── CASE 7: THE SIM RUNS AND NEEDS ACTUALLY MOVE ────────────────────────
    tn_world_init();
    {
        const int before = tn_agent[0].need[TN_SERVE_BLADDER];
        for (int i = 0; i < 600; i++) tn_agents_tick();
        int any_used = 0;
        for (int i = 0; i < tn_agent_n; i++) if (tn_agent[i].activity != TN_ACT_IDLE) any_used = 1;
        snprintf(sp, sizeof sp, "case 7: after 600 minutes somebody is doing something (bladder %d -> %d)",
                 before, tn_agent[0].need[TN_SERVE_BLADDER]);
        expect(any_used, sp);
        expect(tn_clock.minute != 8 * 60 || tn_clock.day != 1, "case 7: the calendar advanced");
    }

    // ── CASE 8: OWNERSHIP IS NOT ENFORCED, AND THAT IS A FINDING ────────────
    // The slice found this by running: residents from household 1 walk across the building and eat
    // out of household 0's fridge, because nothing in the score knows about `household`. The design
    // (§6) says storage is owned and treats "whose fridge is it" as a FEATURE, the source of the
    // comedy the game is supposed to generate. So this is a real gap in the model, not in the code.
    //
    // It is asserted here deliberately, as CURRENT behaviour rather than as desired behaviour, so
    // that whoever wires ownership has to come here and consciously flip it. A gap that a test
    // describes is a work item; a gap in a comment is folklore.
    tn_obj_n = 0; tn_agent_n = 0;
    tn_add_obj(TN_OBJ_FRIDGE, 1, 1, 0);          // belongs to household 0
    tn_add_agent(1, 1, 2);                        // a resident of household 1, standing next to it
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
    tn_agent[0].need[TN_SERVE_HUNGER] = 30;
    {
        TnTag tag; int score;
        const int o = tn_best_action(0, &tag, &score);
        expect(o == 0 && tag == TN_SERVE_HUNGER && tn_obj[0].household == 0 &&
               tn_agent[0].household == 1,
               "case 8 (KNOWN GAP): a household-1 resident helps itself to household-0's fridge, "
               "because ownership is not yet a term in the score. Flip this when §6 lands.");
    }

    tn_world_init();
}
#endif
