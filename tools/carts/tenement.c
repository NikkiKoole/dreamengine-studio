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

// ORDER IS A HARD REQUIREMENT, and every constraint here was discovered by a module author
// rather than designed by me:
//   world  before path   — path routes over world's edge walls (tn_can_step)
//   path   before offer  — offer prices a bid by tn_path_len, not by a straight line
//   econ   before build  — build spends through econ's single debit path, so econ's
//                          money-destroyed ledger stays conserved
//   art    before build  — build reads art's camera to turn a click into a tile
// The offer<->store cycle (offer needs the ownership penalty, store needs tn_find_store) is
// broken by forward declarations in the contract, not by include order.
#include "tenement/world.h"     // globals, spawning, EDGE walls, rooms, the level
#include "tenement/path.h"      // BFS distance field over those walls
#include "tenement/offer.h"     // THE index
#include "tenement/agents.h"    // decay + the state machine
#include "tenement/work.h"      // orders, shifts, the sell seam
#include "tenement/econ.h"      // households, rent, bills, buying
#include "tenement/store.h"     // items, containers, ownership
#include "tenement/art.h"       // the isometric view
#include "tenement/hud.h"       // the panels
#include "tenement/build.h"     // the player's verb

// ── input ───────────────────────────────────────────────────────────────────
static int tnc_paused = 0, tnc_speed = 1;

void init(void) { colorkey(-1); tn_world_init(); }

void update(void) {
    tn_build_input();
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

void draw(void) { cls(CLR_DARK_BLUE); tn_draw_world(); tn_build_draw(); tn_draw_hud(); }

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
    // And the converse, so the test cannot pass by always preferring the toilet: put the fridge
    // just as close and it must win, because now its bigger deficit pays no travel penalty.
    //
    // (1,3) USED to be the "equalised" spot and it was wrong the moment travel became a real walk:
    // on the level world.h ships, (1,3) sits in the hall behind flat A's party wall, so it is three
    // steps, not one. The fixture was only equal to a straight line that could not see walls, which
    // is exactly the lie the path module was built to remove. Predicted by the path agent before it
    // broke, and it broke on cue.
    tn_obj[1].tx = 2; tn_obj[1].ty = 2;
    tn_path_dirty();                               // the field is cached; moving an object dirties it
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

    // ── CASE 9: A LONG ACTIVITY SURVIVES MIDNIGHT ───────────────────────────
    // Found by the `work` agent reading agents.h, not by any of the 21 assertions above, and that
    // is the point of the case. `until` was a WRAPPED minute-of-day tested with `minute >= until`,
    // so a 480-minute sleep started at 20:00 gave until=240 and completed on the very first tick.
    // Beds did not work in the evening, which is the only time anyone sleeps.
    //
    // Every case above ran from 08:00, where 480 minutes lands at 16:00 and never wraps, so the
    // whole suite was green while the most common action in the game was broken. A test that only
    // exercises the easy half of a range is worse than no test, because it buys confidence.
    tn_obj_n = 0; tn_agent_n = 0;
    tn_add_obj(TN_OBJ_BED, 1, 1, 0);
    tn_add_agent(0, 1, 2);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
    tn_agent[0].need[TN_SERVE_REST] = 10;          // desperate for the bed, and only the bed
    tn_clock.minute = 20 * 60;                     // 20:00, so 480 minutes crosses midnight
    tn_clock.day = 1;
    {
        int started = 0;
        for (int i = 0; i < 30 && !started; i++) { tn_agents_tick(); if (tn_agent[0].activity == TN_ACT_USE) started = 1; }
        snprintf(sp, sizeof sp, "case 9: the resident got into bed at %02d:%02d",
                 tn_clock.minute / 60, tn_clock.minute % 60);
        expect(started, sp);
        const int until = tn_agent[0].until;
        for (int i = 0; i < 240; i++) tn_agents_tick();   // run past midnight
        snprintf(sp, sizeof sp, "case 9: STILL asleep 4 hours later, past midnight (day %d %02d:%02d, "
                                "until=%d, now=%d)", tn_clock.day, tn_clock.minute / 60,
                 tn_clock.minute % 60, until, tn_now());
        expect(tn_agent[0].activity == TN_ACT_USE, sp);
        expect(until > 1440, "case 9: `until` is an ABSOLUTE minute, so it can exceed a day");
        // And it must still COMPLETE. A clock that never comes due is the opposite bug, and it
        // would look identical from the outside for a while.
        //
        // Do not test this as "no longer in bed": the resident finishes, rest is still the best
        // thing on offer (everything else is sated), and it climbs straight back in. That is
        // correct behaviour and the first version of this assertion called it a failure. The
        // observable proof of a COMPLETION is that the need was restored, which only happens on
        // the completion branch.
        for (int i = 0; i < 300; i++) tn_agents_tick();
        expect(tn_agent[0].need[TN_SERVE_REST] > 10,
               "case 9: the sleep COMPLETED and restored rest, so the clock does come due");
        expect(tn_agent[0].until != until,
               "case 9: and the activity clock moved on, rather than being stuck on the old one");
    }

    // ── CASE 10: POSTURE IS DATA, AND THE BODY FOLLOWS THE OBJECT ───────────
    // The maker's observation: residents were STANDING on their beds. The cause was not a drawing
    // bug, it was that nothing in the model had any notion of posture, so the renderer had only one
    // figure to draw. The fix is a field on the OFFER, which means a new posture for a new object is
    // a table row (contract rule 3) and the draw loop never asks what an object is.
    tn_obj_n = 0; tn_agent_n = 0;
    tn_add_obj(TN_OBJ_BED,    1, 1, 0);
    tn_add_obj(TN_OBJ_TOILET, 3, 1, 0);
    tn_add_agent(0, 1, 2);
    {
        // The table itself: every offer declares a pose, and the three that matter differ.
        expect(TN_OFFERS[TN_OBJ_BED][0].pose    == TN_POSE_LIE,   "case 10: a bed says LIE");
        expect(TN_OFFERS[TN_OBJ_TOILET][0].pose == TN_POSE_SIT,   "case 10: a toilet says SIT");
        expect(TN_OFFERS[TN_OBJ_LOOM][0].pose   == TN_POSE_STAND, "case 10: a loom says STAND");

        // And the body follows it in the running sim, which is the part that was broken.
        for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 255;
        tn_agent[0].need[TN_SERVE_REST] = 20;
        expect(tn_agent[0].pose == TN_POSE_STAND, "case 10: a resident starts on its feet");
        int lay = 0;
        for (int i = 0; i < 40 && !lay; i++) {
            tn_agents_tick();
            if (tn_agent[0].activity == TN_ACT_USE && tn_agent[0].pose == TN_POSE_LIE) lay = 1;
        }
        expect(lay, "case 10: a resident sent to bed is LYING DOWN, not standing on it");
        // Finishing must put it back on its feet, or it walks around horizontally forever.
        for (int i = 0; i < 600; i++) tn_agents_tick();
        expect(tn_agent[0].pose == TN_POSE_STAND || tn_agent[0].activity == TN_ACT_USE,
               "case 10: and it stands up again when it is done");
    }

    // ── CASE 11: NOBODY WALKS THROOUGH A WALL ───────────────────────────────
    // The worst bug this cart has had, found by a critic and not by any of the 213 other
    // assertions. tno_travel already priced a bid over the real BFS route, but the MOVEMENT was two
    // lines of unconditional axis-stepping with no wall test, so the sim charged for a walk nobody
    // took: 11.8% of all steps crossed a TN_WALL_SOLID edge, the first straight through a party wall,
    // and a trip priced at 11 tiles finished in 5 minutes.
    //
    // It also falsified the design's central claim: residents who ignore walls never enter a
    // corridor, so a corridor can never jam, so §1's "a badly planned building becomes visible as a
    // traffic pattern" was unreachable. path.h was correct, measured free, and had no consumers.
    //
    // This asserts the property directly over the REAL level rather than a fixture, because the bug
    // was invisible in every fixture: every step must be one tile and must be a step tn_can_step
    // permits.
    tn_world_init();
    {
        short px[TN_MAX_AGENTS], py[TN_MAX_AGENTS];
        for (int i = 0; i < tn_agent_n; i++) { px[i] = tn_agent[i].tx; py[i] = tn_agent[i].ty; }
        long steps = 0, illegal = 0;
        for (int t = 0; t < 4000; t++) {
            tn_agents_tick(); tn_work_tick(); tn_econ_tick(); tn_store_tick();
            for (int i = 0; i < tn_agent_n; i++) {
                const int dx = tn_agent[i].tx - px[i], dy = tn_agent[i].ty - py[i];
                if (dx || dy) {
                    steps++;
                    const int dir = dx > 0 ? TN_DIR_E : dx < 0 ? TN_DIR_W : dy > 0 ? TN_DIR_S : TN_DIR_N;
                    if (abs(dx) + abs(dy) != 1 || !tn_can_step(px[i], py[i], dir)) illegal++;
                }
                px[i] = tn_agent[i].tx; py[i] = tn_agent[i].ty;
            }
        }
        snprintf(sp, sizeof sp, "case 11: %ld walk steps and NONE crossed a wall or teleported "
                                "(%ld illegal)", steps, illegal);
        expect(steps > 100 && illegal == 0, sp);
    }

    // ── every module's own assertions, per spec.h's "SPECS ON AN INCLUDEABLE" ────────────────
    // Each module wrote and verified these against its own file; wiring them is the integrator's
    // job, and until this line existed none of them ran in the real build.
    tn_world_selfcheck();
    tn_path_selfcheck();
    tn_work_selfcheck();
    tn_econ_selfcheck();
    tn_store_selfcheck();
    tn_build_selfcheck();

    tn_world_init();
}
#endif
