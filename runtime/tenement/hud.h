// ─────────────────────────────────────────────────────────────────────────────
// tenement/hud.h — the panels. Its job is to make the invisible VISIBLE: every bid, not just
// the winner, because the interesting half of this simulation cannot be seen otherwise.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tnh_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_HUD_H
#define TENEMENT_HUD_H

extern int tnc_show_bids;   // owner: cart (input state)

// TAG_NAME is visible to spec() through the one-TU include order (the cart includes this file
// before its own spec block). That is deliberate: a test that names a tag should print its name.
static const char *TAG_NAME[TN_TAG_COUNT] = {
    [TN_SERVE_HUNGER]="HUNGER", [TN_SERVE_REST]="REST", [TN_SERVE_HYGIENE]="HYGIENE",
    [TN_SERVE_BLADDER]="BLADDER", [TN_SERVE_FUN]="FUN", [TN_SERVE_COUNT]="-",
    [TN_CAP_WORK]="WORK", [TN_CAP_HEAT]="HEAT", [TN_CAP_CUT]="CUT", [TN_CAP_POWER]="POWER",
    [TN_STORE_FOOD]="ST_FOOD", [TN_STORE_GOODS]="ST_GOODS", [TN_STORE_CLOTHES]="ST_CLOTHES",
};


// THE POINT OF THE HUD: the interesting part of this sim is invisible. Show the winning bid.
void tn_draw_hud(void) {
    char t[80];
    rectfill(0, 0, SCREEN_W, 9, CLR_BLACK);
    snprintf(t, sizeof t, "day %d  %02d:%02d   Q/E turn  TAB bids", tn_clock.day,
             tn_clock.minute / 60, tn_clock.minute % 60);
    print(t, 3, 1, CLR_LIGHT_GREY);

    rectfill(0, SCREEN_H - 26, SCREEN_W, 26, CLR_BLACK);
    // Two columns of 14 chars. The 8x8 font gives 40 columns across 320px, so a 14-char cell at
    // x=3 and x=160 fits with room to spare. The first cut used a 21-char format in a 20-char
    // column and the score ran into the next agent's id.
    for (int i = 0; i < tn_agent_n && i < 4; i++) {
        const char mark = tn_agent[i].activity == TN_ACT_USE ? '*'
                        : tn_agent[i].activity == TN_ACT_WALK ? '>' : '.';
        snprintf(t, sizeof t, "%d%d%c%-7s%4d", i, tn_agent[i].household, mark,
                 TAG_NAME[tn_agent[i].bid_tag] ? TAG_NAME[tn_agent[i].bid_tag] : "?", tn_agent[i].bid_score);
        print(t, 3 + (i % 2) * 160, SCREEN_H - 23 + (i / 2) * 9, CLR_WHITE);
    }
    if (tnc_show_bids) {                                  // every bid agent 0 considered, not just the winner
        int y = 12;
        for (int o = 0; o < tn_obj_n && y < SCREEN_H - 30; o++) {
            const int kind = tn_obj[o].kind;
            for (int k = 0; k < TN_OFFER_N[kind]; k++) {
                const TnOffer *of = &TN_OFFERS[kind][k];
                if (of->tag >= TN_SERVE_COUNT) continue;
                const int s = tn_score_offer(0, o, (TnTag)of->tag);
                if (s <= 0) continue;
                snprintf(t, sizeof t, "%-8s %5d", TAG_NAME[of->tag], s);
                print(t, 3, y, (o == tn_agent[0].target_obj) ? CLR_YELLOW : CLR_DARK_GREY);
                y += 8;
            }
        }
    }
}

#endif // TENEMENT_HUD_H
