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
            if (tn_clock.minute >= a->until || a->until < 0) {
                if (a->target_obj >= 0) {
                    const TnOffer *of = tno_offer_of(a->target_obj, a->bid_tag);
                    if (of && of->strength > 0) {
                        const int v = a->need[a->bid_tag] + of->strength;
                        a->need[a->bid_tag] = (unsigned char)(v > 255 ? 255 : v);
                    }
                    if (tn_obj[a->target_obj].users > 0) tn_obj[a->target_obj].users--;
                }
                a->target_obj = -1; a->activity = TN_ACT_IDLE;
            }
            break;
        case TN_ACT_WALK:
            if (a->target_obj < 0) { a->activity = TN_ACT_IDLE; break; }
            {
                const int ox = tn_obj[a->target_obj].tx, oy = tn_obj[a->target_obj].ty;
                if (a->tx != ox) a->tx += (a->tx < ox) ? 1 : -1;
                else if (a->ty != oy) a->ty += (a->ty < oy) ? 1 : -1;
                a->facing = (unsigned char)((a->tx < ox) ? 1 : (a->tx > ox) ? 3 : (a->ty < oy) ? 2 : 0);
                if (abs(a->tx - ox) + abs(a->ty - oy) <= 1) {
                    const TnOffer *of = tno_offer_of(a->target_obj, a->bid_tag);
                    if (of && tn_obj[a->target_obj].users < of->capacity) {
                        tn_obj[a->target_obj].users++;
                        a->activity = TN_ACT_USE;
                        a->until = (short)((tn_clock.minute + of->minutes) % 1440);
                    } else { a->target_obj = -1; a->activity = TN_ACT_IDLE; }
                }
            }
            break;
        default: {                                          // IDLE: take the best offer going
            TnTag tag; int score;
            const int o = tn_best_action(i, &tag, &score);
            a->bid_tag = tag; a->bid_score = score;
            if (o >= 0) { a->target_obj = (signed char)o; a->activity = TN_ACT_WALK; }
            break;
        }
        }
    }
}

#endif // TENEMENT_AGENTS_H
