/* de:meta
{
  "title": "tenement",
  "slug": "tenement",
  "kind": [
    "probe",
    "tech-demo"
  ],
  "teaches": [
    "isometric-projection",
    "finite-state-ai"
  ],
  "created": "2026-08-12",
  "lineage": "Thin vertical slice of the tenement sim: proves ONE claim, that a single argmax over every (object, need) pair produces different, better behaviour than the urgency-sort every other needs sim here uses. sims sorts needs then finds an object; this lets an adjacent nearly-free toilet outbid a distant fridge at higher hunger, which is what people recognise as Sims-like. Built against the frozen contract runtime/tenement/model.h before any fan-out, because that contract's centrepiece had never been exercised by a line of code.",
  "todo": [
    "WHERE THIS STANDS AFTER 2026-08-14. The maker's verdict was that the cart reads as an architectural diagram with a caption, and it is now answered on both halves. IDENTITY landed first (low-poly meshes, per-household colour, three poses): you can see whose flat someone is in and what they are doing. This pass did STATE and STAKES. Posture now carries the worst need, the notable moments happen above the residents' heads instead of in a text band, and — the big one — the ITEM ECONOMY IS REACHABLE FOR THE FIRST TIME, so goods are made, carried, shelved and bought, and a household that never builds storage quietly stops earning. What is left at the top is the RIM (two residents still read as one blob) and the open design question of whether something can be irrecoverably LOST.",
    "A RIM ON THE RESIDENTS is now the top item, and it is still cheap. Two residents on neighbouring tiles read as one blob, so a queue of three looks like a queue of one — and this pass made that worse in the one place it matters, because hauling puts more people in the hall at once. Per-household colour helped and did not solve it: two members of the SAME household adjacent are still one shape. With the depth buffer in place a rim is a per-pixel job rather than a sort problem.",
    "LET SOMETHING BE LOST, still the real design conversation rather than a task, but the ground under it moved. econ.h's header no longer describes the whole game ('no eviction, no game over, no score, no consequence'): a household with nowhere to shelve its goods now STOPS EARNING, which is the first thing in this sim that can actually go badly for you and is visible as bolts piling up on the floor (work.h case W5b pins it). That is a consequence, not yet a LOSS — nothing forecloses, arrears is still a record nothing reads, and there is still no MEMORY, so no day is worse than yesterday. Eviction is the obvious next version; a smaller one is needs that leave TRACES the building keeps. Design section 8a left the win condition open on purpose and it is still open.",
    "THE HYGIENE HOLE IS CLOSED, and the finding is worth keeping because it nearly wasted the posture work. Nothing served TN_SERVE_HYGIENE, so that need sat at zero from day one and every resident read 'filthy' forever — which meant the WORST need was the same need for everybody at every hour, and a posture keyed to it would have been a building permanently stooped. The fix was one table row (the WC gained a weak, slow cold-tap TN_SERVE_HYGIENE offer) rather than a washbasin object, on purpose: it aims more traffic at the ONE shared fixture the design is about. Watch what it did to contention before adding a second basin.",
    "NO POSE FOR HAUL — DONE, and it was never really an art problem. The reason a carrying resident looked identical to an empty-handed one is that no resident ever carried anything: work.h sold each good where it was made and deleted it in the same breath, so `carrying` was never once non-negative in a running game. person_haul exists in both views now (a load held proud of the chest, in wood rather than household colour so it does not read as a fat torso).",
    "THE POLYGON VIEW IS NOT MEASURED ON A DEVICE. Re-measured 2026-08-14 at 1.69ms average / 2.33ms max, about 15.6k one-row rectfill a frame, against the sprite view's 0.49ms — so the poses, the slump and the event chips added nothing measurable. Still ADR-0024's phone factor puts it near the edge: run ios/measure-device.sh before this ships to iOS. Note both cheap levers are already SPENT: there is no sort to speed up, and subdivision is already deleted (it existed only to make a sort sound, and removing it paid for the depth test almost exactly). What is left if it must come down is real work — fewer triangles per mesh, or a coarser canvas.",
    "THE MESHES EXIST TWICE — runtime/tenement/art.h and tools/carts/polyroom.c — and only polyroom's copy is GATED. Its spec is what checks footprint parity against the sprite each mesh replaces, that parts never interpenetrate, and that the two projections agree at a baked angle. A mesh edited in art.h alone has none of those. ADR-0006 wanted real consumers before a library header and there are now two, so extraction is due.",
    "INTERIOR WALLS ARE FULL HEIGHT except where the cutaway makes them low, so a room seen past another room's wall is hidden. Wall height is a free parameter for the first time now that walls are geometry rather than baked cells — which is exactly what the 'WC wants unconditionally-low interior walls' item below was waiting for. Also open and related: the free camera can tilt to nearly flat, which is a genuinely different read of the same floorplan and may be the better answer than shortening walls.",
    "THE FREE CAMERA IS A LOOKING TOOL ONLY, and build mode snaps back to a baked angle on purpose: build.h picks a tile by inverting the projection and that inverse only knows the four quarter turns, so at any other angle a click lands on a tile the player did not aim at. Generalising tnb_unproject to a continuous yaw and tilt is the fix, it is a change inside build.h, and it wants that module's assertions in front of whoever does it.",
    "The D/R fork, still PARKED, and the reason to wait is thinner than it was. D (price an action by its duration) and R (a day-shaped bid plus a floor under the argmax) default OFF, so the shipped sim is still the one 249 assertions describe. Together they take contention from 3.3% of frames to 16.2% and are the first time design section 1's claim actually happens. Turning them on rewrites case 1's converse, the cart's headline assertion. The old objection was 'contention with no CONSEQUENCE is just traffic' — and there IS a consequence now (a household that cannot shelve its goods stops earning), so the honest next step is to turn D+R on in a long run and read whether the contention costs anybody their income. If it does, unpark them. See design section 12; a live A/B is what the keys are for.",
    "The day's PHASE is wrong: with D+R residents sleep late afternoon and are up all night. Tuning, not structure. Open question first though, and it is a taste question not a numbers one: do we want a literal human day, or is a building that keeps odd hours funnier? Note the measured trap in offer.h before touching it, that a SHARPER day makes a QUIETER building.",
    "WHO GETS IT WHEN IT FREES is agent index order, so a low-index resident always wins a contested object. Arbitrary rather than first-come. Fine while there are 4 residents; a real unfairness at 12, and the fix is emergent (longest wait wins) rather than a queue structure. Half of this fixed ITSELF at the loom and the way it happened is worth copying: a worker who finishes now has a bolt to go and shelve, so it leaves and the neighbour gets the machine (work.h W8 used to assert the opposite as a known gap). Turn-taking out of an errand, with nothing scoring fairness. The remaining unfairness is real though — give the loser a longer errand and it starves again.",
    "The autarky gate in design section 3: the recipe still has NO INPUT (in_n == 0), so a loom turns time into value out of nothing, and that is the loop still open. The two other holes are closed — hygiene has a server (the WC's tap) and goods have somewhere to live (the wardrobe's second storage row) — which leaves the input side as the honest remaining one. Giving a recipe an input closes it and changes no structure.",
    "THE STORE MODULE IS REACHABLE NOW, and this was the single highest-leverage line in the sim. store.h had always carried a full fetch/haul/put loop over BFS routes with thirty assertions; work.h's two-line stub sold every good where it was made and deleted it in the same breath, so no item ever persisted, `carrying` was never non-negative, and TN_ACT_HAUL was a state nothing could enter. Deleting those two lines (they carried a note saying to) plus one offer row for TN_STORE_GOODS plus a BUYER in econ.h that only takes SHELVED goods turned the whole thing on. Watch for the follow-ons: hauling should be a BID rather than store.h's stopgap priority rule, and a communal shelf still pays nobody.",
    "SPECIES IS A FIELD, NOT A SEAM (contract seam 3). Children are the obvious second species and the contract has room, but nothing reads the field, so the seam is unexercised and therefore unproven.",
    "The WC wants unconditionally-low interior walls (you cannot see the comedy through a wall) and the loom currently reads as a second wardrobe. Both are art, both matter more than they sound: section 1's soul is a thing you SEE.",
    "social.h is committed but unwired, pending its contract diff.",
    "The iso projection is COPIED from isoroom, and the SECOND CONSUMER HAS NOW ARRIVED — polyroom carries its own copy of the same transform, generalized to a continuous yaw and tilt. So ADR-0006's condition is met (a library header wants real consumers, not speculation) and runtime/isoview.h is due. Extract the generalized form, not the four-quarter-turn one: the identity worth preserving is that yaw 45 + 90*rot with tilt 1 reproduces the baked projection exactly, which is what lets sprites and meshes share one draw list, and it is asserted in polyroom's spec.",
    "Objects are placeholders and the tag vocabulary is a first guess. Both should be revised by what the fan-out learns, not defended.",
    "RE-RUN THE CONTENTION TRACE after any change to the score, the decay rates or the offer table. The DE_TRACE block in update() is the instrument that found all four of section 12's findings, and every one of them was invisible to the assertions. Command is in the comment above it."
  ],
  "description": {
    "summary": "A few residents, some furniture, and one question: does the best offer on the table beat picking your worst need first? Watch the scores and see.",
    "detail": "The thin vertical slice of a bigger sim about several households sharing one building. Every object advertises what it offers, how strongly, for how long, and for how many people at once. Every resident does no thinking at all beyond taking the single best offer available, where a need's deficit is one term in the score rather than a filter applied first. That distinction is the whole point: the usual way to write this is to sort needs by urgency and then look for an object, which sends a hungry person past an empty toilet to a fridge on the far side of the building. Here the near thing can win. The HUD shows each resident's current winning bid and its score, because the interesting part of this simulation is invisible otherwise.",
    "controls": "Q/E turn the building a quarter at a time. ARROWS orbit and tilt freely, which only the polygon view can do — the sprites exist at four angles and nowhere between — and which switches off in build mode, because a click has to land on the tile you think you clicked. V flips the rendering between low-poly triangles (the default) and the baked voxel sprites, which is the same building drawn twice. TAB shows every bid the winning resident considered, not just the winner. SPACE pauses. 1/2/4 set speed. D and R are the two open design questions, off by default: D prices an action by how long it takes, R gives the day a shape and lets a resident decline a bad offer. Turn both on and the building starts fighting over the one toilet."
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

// THE TWO OPEN FORKS OF design §12, wired as live toggles rather than argued about. Both default
// OFF, so the committed simulation and every assertion still describe the cart as it shipped; each
// one turned on is a different claim about what a day in a building is, and the point of putting
// them on keys is that the difference is a THING YOU WATCH rather than a table in a doc.
//
//   D  price TIME     the score gains `+ minutes`, becoming value-per-minute-of-your-life.
//                     Without it an 8-hour sleep is priced exactly like a 10-minute toilet visit
//                     and residents sleep 62% of their lives. Costs: it breaks case 1's converse.
//   R  give it a DAY  needs decay on a circadian curve instead of a flat rate, so everybody gets
//                     sleepy at the same hour. Without it the clock decides nothing and four
//                     residents never synchronise, which is why the one WC is never fought over.
int tnc_price_time = 0;         // owner: cart. offer reads it.
int tnc_rhythm     = 0;         // owner: cart. agents reads it.

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
    // BUILD MODE FORCES A BAKED ANGLE. build.h picks a tile by inverting the projection, and that
    // inverse only knows the four quarter turns — so at any other angle the player would click one
    // tile and get another, silently. Snapping is unconditional rather than "only on entry" so no
    // path can leave the camera and the picker disagreeing.
    if (tnb_on) { tn_yaw = 45.0f + 90.0f * tn_rot; tn_tilt = 1.0f; }
    if (keyp('Q')) { tn_rot = (tn_rot + 3) & 3; tn_yaw = 45.0f + 90.0f * tn_rot; tn_tilt = 1.0f; }
    if (keyp('E')) { tn_rot = (tn_rot + 1) & 3; tn_yaw = 45.0f + 90.0f * tn_rot; tn_tilt = 1.0f; }
    // FREE ORBIT AND TILT, arrows, POLYGON VIEW ONLY — and off in build mode, which is not a
    // limitation to apologise for but the honest shape of the thing. The sprites exist at four angles
    // and nowhere between, and build.h turns a click into a tile through an inverse that only knows
    // those four; at any other angle you would be clicking one tile and getting another. So the free
    // camera is a LOOKING tool, and Q/E (or opening build) snap back to a baked angle.
    // Per frame rather than dt-scaled, so a scripted clip lands on a chosen angle: a headless run
    // compresses elapsed time to nearly nothing, which makes dt-driven motion unrepeatable.
    if (tn_poly && !tnb_on) {
        if (key(KEY_LEFT))  tn_yaw -= 1.5f;
        if (key(KEY_RIGHT)) tn_yaw += 1.5f;
        if (key(KEY_UP))   { tn_tilt += 0.02f; if (tn_tilt > 1.35f) tn_tilt = 1.35f; }
        if (key(KEY_DOWN)) { tn_tilt -= 0.02f; if (tn_tilt < 0.30f) tn_tilt = 0.30f; }
        while (tn_yaw <   0.0f) tn_yaw += 360.0f;
        while (tn_yaw >= 360.0f) tn_yaw -= 360.0f;
        // tn_rot tracks the nearest quarter turn, so the sprite view and the cell lookup stay sane
        // and flipping to V never lands on an angle the sprites do not have.
        tn_rot = ((int)floorf((tn_yaw - 45.0f) / 90.0f + 0.5f)) & 3;
        tn_camera();
    }
    if (keyp(KEY_TAB)) tnc_show_bids = !tnc_show_bids;
    // V flips the two RENDERINGS of the same building: low-poly triangles (the default now) and the
    // baked voxel sprites this cart shipped with. Kept as a toggle rather than a replacement because
    // the maker's verdict was about the PICTURE, and a verdict about a picture stays checkable only
    // while both pictures exist. The sim, the projection and every assertion are identical either
    // way — see the long note in runtime/tenement/art.h, and `polyroom` for the A/B bench.
    if (keyp('V')) tn_poly = !tn_poly;
    if (keyp('D')) tnc_price_time = !tnc_price_time;   // design §12b — see the block up top
    if (keyp('R')) tnc_rhythm     = !tnc_rhythm;       // design §12c
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
    // CONTENTION, measured rather than assumed, and the reason this block exists permanently.
    //
    // design §1 promises "queues form, corridors jam" and §4 promises "one loom, four tenants, and
    // a queue you can see". No oracle reads a design doc, and every one of the 242 assertions
    // describes a single DECISION, so nothing in the repo could see that the building was not doing
    // any of it. Only a distribution over time can: 99.6% of frames had somebody wanting a full
    // object and 2.3% had anybody standing at one, the headline scarcity (one WC, four households)
    // sat empty 91% of the time, and bed occupancy was flat across all 24 hours because residents
    // sleep 62% of their lives. The full reading is design §12; two of its three causes are still
    // open, so re-run this after any change to the score, the decay rates or the offer table:
    //
    //   node tools/play.js tenement script /dev/null --headless --frames 2000 --trace out.jsonl
    //
    // `uXXX` histogrammed by frame index gives the hour-of-day shape (1 frame = 4 sim minutes).
    {
        int taken = 0, share = 0, wait = 0, busy = 0;
        for (int o = 0; o < tn_obj_n; o++) {
            const int kind = tn_obj[o].kind;
            for (int i = 0; i < TN_OFFER_N[kind]; i++) {
                const TnOffer *of = &TN_OFFERS[kind][i];
                if (of->tag >= TN_SERVE_COUNT || tn_obj[o].users < of->capacity) continue;
                busy++;
                for (int a = 0; a < tn_agent_n; a++)
                    if (tn_agent[a].need[of->tag] < 255 && tn_agent[a].target_obj != o) taken++;
            }
        }
        for (int a = 0; a < tn_agent_n; a++) {
            if (tn_agent[a].target_obj < 0) continue;
            for (int b = a + 1; b < tn_agent_n; b++)
                if (tn_agent[b].target_obj == tn_agent[a].target_obj) share++;
            const int t = tn_agent[a].target_obj;
            if (tn_agent[a].activity != TN_ACT_USE &&
                abs(tn_agent[a].tx - tn_obj[t].tx) + abs(tn_agent[a].ty - tn_obj[t].ty) <= 1) wait++;
        }
        watch("taken", "%d", taken);   // wanted by someone, and full
        watch("share", "%d", share);   // two residents heading for the same thing
        watch("wait",  "%d", wait);    // standing at a thing without using it
        watch("busy",  "%d", busy);    // offers at capacity right now
        {
            int use[TN_OBJ_KIND_COUNT] = {0};
            for (int o = 0; o < tn_obj_n; o++) if (tn_obj[o].users > 0) use[tn_obj[o].kind]++;
            watch("uBED", "%d", use[TN_OBJ_BED]);
            watch("uWC",  "%d", use[TN_OBJ_TOILET]);
            watch("uFRG", "%d", use[TN_OBJ_FRIDGE]);
            watch("uSOF", "%d", use[TN_OBJ_SOFA]);
            watch("uLOO", "%d", use[TN_OBJ_LOOM]);
            int lowbl = 0;
            for (int a = 0; a < tn_agent_n; a++)
                if (tn_agent[a].need[TN_SERVE_BLADDER] < 128) lowbl++;
            watch("needWC", "%d", lowbl);   // how many residents are half-desperate at once
            // WORST NEED PER RESIDENT — the quantity art.h's slump is driven by (tnr_distress) and
            // the HUD's bottom band prints a word for. Traced because the SHAPE of its distribution
            // is what decides where the posture curve belongs, and guessing that wrong is invisible:
            // the first cut of the slump was keyed to a band nobody occupies and moved 15 pixels
            // across an 1800-frame run. Measured with D+R on: median 96, p25 44, 23% under 32.
            // Re-run this before re-tuning TNR_DISTRESS_AT or the squash/flare/lean constants.
            for (int a = 0; a < tn_agent_n && a < 4; a++) {
                int w = 255;
                for (int n = 0; n < TN_NEED_COUNT; n++) if (tn_agent[a].need[n] < w) w = tn_agent[a].need[n];
                char nm[8]; snprintf(nm, sizeof nm, "w%d", a);
                watch(nm, "%d", w);
            }
            // THE GOODS CHAIN, end to end: made -> carried -> shelved -> sold. Each stage is a
            // separate number because a break anywhere reads as "no money" at the far end, and the
            // money column alone cannot tell a factory that never ran from a cupboard nobody owns.
            {
                int held = 0, shelved = 0, loose = 0, gstore = 0;
                for (int i = 0; i < tn_item_n; i++) {
                    if (tn_item[i].store_tag == TN_ITEM_FREE) continue;
                    if      (tn_item[i].held_by   >= 0) held++;
                    else if (tn_item[i].stored_in >= 0) shelved++;
                    else                                loose++;
                }
                for (int o = 0; o < tn_obj_n; o++)
                    if (tn_offers(o, TN_STORE_GOODS, NULL) && tn_obj[o].household >= 0) gstore++;
                watch("iHeld",  "%d", held);
                watch("iShelf", "%d", shelved);
                watch("iLoose", "%d", loose);
                watch("gStore", "%d", gstore);   // owned cupboards that accept goods (buyer needs one)
                watch("sold",   "%d", tn_econ_goods_sold());
            }
        }
    }
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

    // ── CASE 2b: A WAIT IS PRICED, NOT BANNED ───────────────────────────────
    // Case 2 above fakes occupancy by poking `users` with nobody inside, which is why it kept
    // passing when the rule underneath it changed: an occupation with no occupant has no end date,
    // so it still reads as infinite. This case puts a REAL resident in there with a REAL clock on
    // it, which is the only way the interesting half gets exercised.
    //
    // What it pins is the thing that makes queues possible at all: how long you would wait is a
    // number in the same unit as how far you would walk, so a short wait for the right thing beats
    // a walk to the wrong one, and a long wait does not. Nobody wrote "toilets form queues and beds
    // do not" anywhere — it falls out of 10 minutes versus 480.
    tn_obj_n = 0; tn_agent_n = 0;
    tn_add_obj(TN_OBJ_TOILET, 1, 1, 0);          // obj 0 — a 10-minute offer
    tn_add_obj(TN_OBJ_SOFA,   3, 1, 0);          // obj 1 — the alternative, if waiting is not worth it
    tn_add_agent(0, 1, 2);                       // agent 0 — the one deciding
    tn_add_agent(0, 1, 0);                       // agent 1 — the one already in there
    for (int n = 0; n < TN_NEED_COUNT; n++) { tn_agent[0].need[n] = 255; tn_agent[1].need[n] = 255; }
    tn_agent[0].need[TN_SERVE_BLADDER] = 60;
    tn_agent[0].need[TN_SERVE_FUN]     = 90;
    tn_agent[1].activity   = TN_ACT_USE;
    tn_agent[1].target_obj = 0;
    tn_obj[0].users        = 1;
    {
        TnTag tag; int soon_score, late_score;
        tn_agent[1].until = tn_now() + 2;         // nearly done
        expect(tn_best_action(0, &tag, &soon_score) == 0 && tag == TN_SERVE_BLADDER,
               "case 2b: two minutes from free, the toilet is still the best offer — you WAIT");

        tn_agent[1].until = tn_now() + 400;       // just went to sleep on the job
        expect(tn_best_action(0, &tag, &late_score) == 1 && tag == TN_SERVE_FUN,
               "case 2b: 400 minutes from free, the same toilet loses to the sofa — you LEAVE");

        expect(tn_score_offer(0, 0, TN_SERVE_BLADDER) < soon_score,
               "case 2b: and it is a slope, not a switch — the longer the wait, the lower the bid");
    }
    // The state machine has to agree with the score, or the decision to wait is invisible: a
    // resident that arrives and finds the thing full must STAND THERE still pointed at it. The
    // original dropped the target on arrival, so a waiter re-derived the same answer every tick
    // with nothing on screen to show for it, and that is why contention read as teleportation.
    tn_agent[1].until      = tn_now() + 5;
    tn_agent[0].tx = 1; tn_agent[0].ty = 2;       // already beside it
    tn_agent[0].activity   = TN_ACT_WALK;
    tn_agent[0].target_obj = 0;
    tn_agents_tick();
    expect(tn_agent[0].target_obj == 0 && tn_agent[0].activity != TN_ACT_USE,
           "case 2b: arriving at a full object KEEPS the target — the wait is a place you stand");
    expect(tn_obj[0].users == 1,
           "case 2b: and waiting never sneaks past capacity — still exactly one person in there");

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
    tn_hud_selfcheck();

    tn_world_init();
}
#endif
