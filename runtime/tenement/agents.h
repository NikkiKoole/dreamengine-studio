// ─────────────────────────────────────────────────────────────────────────────
// tenement/agents.h — need decay and the activity state machine. Dumb by design: takes the best offer.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tna_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_AGENTS_H
#define TENEMENT_AGENTS_H

// Decay rates per need, per hour. DATA, per seam 1: a new need is a row here, not a code path.
static const unsigned char TNA_DECAY[TN_NEED_COUNT] = { 8, 6, 5, 10, 4 };

void tn_agents_tick(void) {
    tn_clock.minute += 1;
    if (tn_clock.minute >= 1440) { tn_clock.minute = 0; tn_clock.day++; }

    for (int i = 0; i < tn_agent_n; i++) {
        TnAgent *a = &tn_agent[i];
        if (tn_clock.minute % 60 == 0)
            for (int n = 0; n < TN_NEED_COUNT; n++)
                a->need[n] = (unsigned char)(a->need[n] > TNA_DECAY[n] ? a->need[n] - TNA_DECAY[n] : 0);

        switch (a->activity) {
        case TN_ACT_USE:
            if (tn_now() >= a->until || a->until < 0) {
                if (a->target_obj >= 0) {
                    const TnOffer *of = tno_offer_of(a->target_obj, a->bid_tag);
                    if (of && of->strength > 0) {
                        const int v = a->need[a->bid_tag] + of->strength;
                        a->need[a->bid_tag] = (unsigned char)(v > 255 ? 255 : v);
                    }
                    if (tn_obj[a->target_obj].users > 0) tn_obj[a->target_obj].users--;
                }
                a->target_obj = -1; a->activity = TN_ACT_IDLE;
                a->pose = TN_POSE_STAND;   // done: back on your feet
            }
            break;
        case TN_ACT_WALK:
            if (a->target_obj < 0) { a->activity = TN_ACT_IDLE; break; }
            {
                const int ox = tn_obj[a->target_obj].tx, oy = tn_obj[a->target_obj].ty;
                // FOLLOW THE ROUTE. This used to be two lines of unconditional axis-stepping with no
                // wall test at all, which made the whole simulation dishonest in the worst possible
                // way: tno_travel already PRICED the real BFS route, so a bid was costed over a walk
                // nobody ever took. Measured before this fix, 11.8% of all steps crossed a
                // TN_WALL_SOLID edge, the first one straight through flat A and B's party wall, and
                // a trip priced at 11 tiles completed in 5 minutes.
                //
                // It also falsified the design's central claim. §1 says a badly planned building
                // becomes visible as a traffic pattern; residents who ignore walls never enter a
                // corridor, so a corridor can never jam, so the player's verb (shape space) was
                // disconnected from the only thing that reports on it. path.h existed, was correct,
                // was measured free, and had ZERO consumers outside its own selfcheck.
                // ARRIVAL IS TESTED FIRST, and the order is the whole trick. A resident already
                // standing beside its target must not ask for a route: the goal tile is BLOCKED by
                // the object itself, so tn_path_next honestly answers "no route onto it" and the
                // first version of this code took that as "give up", which left everyone idling
                // beside the thing they wanted, forever. Step only when there is still ground to
                // cover.
                if (abs(a->tx - ox) + abs(a->ty - oy) <= 1) {
                    const TnOffer *of = tno_offer_of(a->target_obj, a->bid_tag);
                    if (of && tn_obj[a->target_obj].users < of->capacity) {
                        tn_obj[a->target_obj].users++;
                        a->activity = TN_ACT_USE;
                        a->pose = of->pose;   // the OBJECT decides what your body does
                        a->until = tn_now() + of->minutes;   // absolute: cannot wrap
                    } else {
                        // FULL ON ARRIVAL: go idle but KEEP the target, which is what turns the
                        // priced wait in tno_score into a visible queue. Dropping it here (the
                        // original) meant a resident who had decided the wait was worth it forgot
                        // that the moment it arrived, and the next tick's argmax re-derived the
                        // same answer from scratch with nothing on screen to show for it. Idle is
                        // still the re-decision point, so a waiter that stops being right about
                        // waiting walks away by itself: nobody is queued to anything by force.
                        a->activity = TN_ACT_IDLE;
                    }
                } else {
                    int nx, ny;
                    if (!tn_path_next(a->tx, a->ty, ox, oy, &nx, &ny)) {
                        a->target_obj = -1; a->activity = TN_ACT_IDLE;   // genuinely unreachable
                    } else {
                        a->facing = (unsigned char)(nx > a->tx ? 1 : nx < a->tx ? 3
                                                  : ny > a->ty ? 2 : 0);
                        a->tx = (short)nx; a->ty = (short)ny;
                    }
                }
            }
            break;
        default: {                                          // IDLE: take the best offer going
            TnTag tag; int score;
            const int o = tn_best_action(i, &tag, &score);
            a->bid_tag = tag; a->bid_score = score;
            // TnIdx, not signed char: the contract widened these and the casts had to follow, or an
            // object index above 127 truncates negative and negative already means "none".
            if (o >= 0) { a->target_obj = (TnIdx)o; a->activity = TN_ACT_WALK; }
            else a->target_obj = -1;   // wanting nothing must also SHOW as nothing: since arrival
                                       // at a full object now keeps the target, a sated agent
                                       // would otherwise stand there still pointed at it.
            break;
        }
        }
    }
}

#endif // TENEMENT_AGENTS_H
