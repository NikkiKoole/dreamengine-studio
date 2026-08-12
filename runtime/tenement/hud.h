// ─────────────────────────────────────────────────────────────────────────────
// tenement/hud.h — the panels. Its job is to make the invisible VISIBLE: every bid, not just
// the winner, because the interesting half of this simulation cannot be seen otherwise.
//
// Written as ONE MODULE OWNED BY ONE AGENT. Include ONLY tenement/model.h (already included by
// the cart before this file) plus engine headers. NEVER include a sibling module. Every static in
// here is prefixed tnh_ because the whole cart is ONE translation unit and two modules sharing an
// unprefixed `static int idx` is a build break. Rules: runtime/tenement/model.h header.
//
// ── THE RULE THIS FILE IS WRITTEN TO ────────────────────────────────────────
// A NUMBER A STRANGER CANNOT INTERPRET IS WORSE THAN NO NUMBER. So everything here is a
// RELATION or a WORD, and a bare quantity only survives where it is comparable to the
// quantity beside it:
//   • `asleep`, `off to the loo`, `carrying a bolt`     — the activity, in English
//   • `2 want the loo, none free`                       — contention, which §1 says is the game
//   • `nowhere to wash`                                 — a need the building cannot serve AT ALL
//   • `in A's flat` / `home`                            — §6's comedy, visible without a tooltip
//   • `owes 60`, `-20`, `short`                         — §5's money, as a relation to the rent
//   • the TAB table                                     — the argmax's own terms, side by side
// The five need pips are the one wordless thing, and they are only there to give the WORD
// beside them ("bursting") a shape a stranger can watch move.
//
// ── THE CANVAS, WHICH IS THE HARD PART ──────────────────────────────────────
// 320x200, and the building is a 264x156 DIAMOND parked in the middle of it: tn_camera()
// centres it in SCREEN_H-26 and the projection puts its four corners at (136,45) (292,123)
// (184,177) (28,99) at rotations 0/2 and (184,45) (292,99) (136,177) (28,123) at 1/3. So the
// bounding box is the same at every rotation and the four TRIANGULAR CORNERS are always empty
// sky. Everything permanent lives in that free space:
//   • an 18px top band          (the world's own silhouette starts at y=21, its top corner)
//   • a 26px bottom band        (one line per resident — the four rows are the game)
//   • an 84x32 panel top-right  (the purses; the top-right corner triangle)
//   • the TAB table is an OVERLAY over the left of the world, because it is a thing you open
// Three fonts, each because a band's HEIGHT decided it: the clock is 8x8 (40 columns) because it
// is the one thing read at a glance; the panels are FONT_SMALL (5px advance, 7px cell, 64
// columns); the resident rows are FONT_TINY (4px, 6px cell, 80 columns) because four rows have to
// fit in 26px and a sentence per resident needs the columns. Font is restored to FONT_NORMAL on
// the way out — the next drawer is whoever the cart calls after us, and it did not ask for ours.
//
// Build mode draws its own label at x=3, y=11..24 and a hint at y=167 (ending at 174), so
// nothing here goes there: the top band stops at 18, the TAB overlay runs 24..162, and the
// bottom band starts at exactly 174. Those numbers are all somebody else's, not preferences.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef TENEMENT_HUD_H
#define TENEMENT_HUD_H

#include <string.h>               // strlen/strcat for the news line; strcmp in the selfcheck
#include "lay.h"                  // engine-side layout (rect in, rect out). Not a sibling module.
#ifdef DE_SPEC
#include "spec.h"                 // for tn_hud_selfcheck() at the bottom; guarded, dev-only
#endif

extern int tnc_show_bids;   // owner: cart (input state)

// TAG_NAME is visible to spec() through the one-TU include order (the cart includes this file
// before its own spec block). That is deliberate: a test that names a tag should print its name.
static const char *TAG_NAME[TN_TAG_COUNT] = {
    [TN_SERVE_HUNGER]="HUNGER", [TN_SERVE_REST]="REST", [TN_SERVE_HYGIENE]="HYGIENE",
    [TN_SERVE_BLADDER]="BLADDER", [TN_SERVE_FUN]="FUN", [TN_SERVE_COUNT]="-",
    [TN_CAP_WORK]="WORK", [TN_CAP_HEAT]="HEAT", [TN_CAP_CUT]="CUT", [TN_CAP_POWER]="POWER",
    [TN_STORE_FOOD]="ST_FOOD", [TN_STORE_GOODS]="ST_GOODS", [TN_STORE_CLOTHES]="ST_CLOTHES",
};

// ─────────────────────────────────────────────────────────────────────────────
// WORDS. All of it DISPLAY ONLY, and all of it keyed by a TAG except the first table, which is
// keyed by kind — the one thing TnObjKind is allowed to be used for (contract rule 2: a NAME is
// fine, a DECISION is not). Nothing below this line branches on either; they are lookups.
//
// A new object or a new tag is a row here, and if a row is missing the column reads "?" rather
// than reading off the end — tn_hud_selfcheck() asserts the tables are complete, because a
// missing word is invisible in a screenshot of a building where that object happens to be idle.
// ─────────────────────────────────────────────────────────────────────────────
static const char *tnh_kind_word[TN_OBJ_KIND_COUNT] = {
    [TN_OBJ_BED]="bed", [TN_OBJ_FRIDGE]="fridge", [TN_OBJ_COUNTER]="counter",
    [TN_OBJ_TOILET]="toilet", [TN_OBJ_SOFA]="sofa", [TN_OBJ_LOOM]="loom",
    [TN_OBJ_WARDROBE]="wardrobe",
};
// What a resident is DOING, and what it is ON ITS WAY to do. Two tables because "asleep" and
// "off to bed" is the difference between the two halves of the state machine, and reading a
// building is mostly reading who is walking.
static const char *tnh_doing[TN_TAG_COUNT] = {
    [TN_SERVE_HUNGER]="eating", [TN_SERVE_REST]="asleep", [TN_SERVE_HYGIENE]="washing",
    [TN_SERVE_BLADDER]="on the loo", [TN_SERVE_FUN]="relaxing", [TN_SERVE_COUNT]="busy",
    [TN_CAP_WORK]="working", [TN_CAP_HEAT]="working", [TN_CAP_CUT]="working",
    [TN_CAP_POWER]="working",
    [TN_STORE_FOOD]="putting it away", [TN_STORE_GOODS]="putting it away",
    [TN_STORE_CLOTHES]="putting it away",
};
static const char *tnh_going[TN_TAG_COUNT] = {
    [TN_SERVE_HUNGER]="off to eat", [TN_SERVE_REST]="off to bed", [TN_SERVE_HYGIENE]="off to wash",
    [TN_SERVE_BLADDER]="off to the loo", [TN_SERVE_FUN]="off to relax", [TN_SERVE_COUNT]="wandering",
    [TN_CAP_WORK]="off to work", [TN_CAP_HEAT]="off to work", [TN_CAP_CUT]="off to work",
    [TN_CAP_POWER]="off to work",
    [TN_STORE_FOOD]="off to tidy", [TN_STORE_GOODS]="off to tidy", [TN_STORE_CLOTHES]="off to tidy",
};
// The thing being competed FOR, for the contention line: "2 want the loo, none free".
static const char *tnh_thing[TN_TAG_COUNT] = {
    [TN_SERVE_HUNGER]="a meal", [TN_SERVE_REST]="a bed", [TN_SERVE_HYGIENE]="a wash",
    [TN_SERVE_BLADDER]="the loo", [TN_SERVE_FUN]="some fun", [TN_SERVE_COUNT]="anything",
    [TN_CAP_WORK]="work", [TN_CAP_HEAT]="work", [TN_CAP_CUT]="work", [TN_CAP_POWER]="work",
    [TN_STORE_FOOD]="a cupboard", [TN_STORE_GOODS]="a cupboard", [TN_STORE_CLOTHES]="a cupboard",
};
// Needs only: the verb for "nowhere to X", and the word for a need that has bottomed out. The
// second table is the whole soul of the thing — "bursting" is a sentence a stranger reads
// without a tooltip, and 135 is not.
static const char *tnh_cure[TN_NEED_COUNT] = { "eat", "sleep", "wash", "use a loo", "have fun" };
static const char *tnh_bad [TN_NEED_COUNT] = { "starving", "exhausted", "filthy", "bursting", "bored" };

// Two accessors, not one, because the two tables have different LENGTHS: a tag table runs to
// TN_TAG_COUNT and the kind table to TN_OBJ_KIND_COUNT, and one shared bounds check would read
// six entries off the end of the shorter one.
static const char *tnh_word(const char *const *tbl, int tag) {
    if (tag < 0 || tag >= TN_TAG_COUNT || !tbl[tag]) return "?";
    return tbl[tag];
}
static const char *tnh_kindname(int kind) {
    if (kind < 0 || kind >= TN_OBJ_KIND_COUNT || !tnh_kind_word[kind]) return "?";
    return tnh_kind_word[kind];
}
static char tnh_letter(int household) {         // household 0 is flat A, and the flats panel agrees
    return (household < 0 || household > 25) ? '-' : (char)('A' + household);
}

// ── layout constants (see the canvas note in the header) ────────────────────
#define TNH_TOP_H  18                 // 18, not 17: the band's hairline rule must not sit ON the
                                      // descenders of the news line (it did, and it read as mud)
// 26, not 30, and the resident rows are FONT_TINY inside it. Both numbers are somebody else's:
// art.h centres the building in SCREEN_H-26, so a taller band eats the world's bottom corner, and
// build.h prints its hint at y=167 (a 7px cell, so it ends at 174) — a 30px band put row one at
// 172 and ui-audit caught eight overlapping pairs the moment build mode was open. 26px at a 6px
// pitch is four rows starting at exactly 174, which is the only thing that fits both.
#define TNH_BOT_H  26
#define TNH_ROW_H   6                 // FONT_TINY's cell (3x5 glyphs, 4px advance, 80 columns)
#define TNH_LINE    7                 // FONT_SMALL's cell height. Rows closer than this OVERLAP,
                                      // which ui-audit reports and a reader sees as mud.
#define TNH_ROWS    4                 // rows the bottom band and the purse panel each have

// ─────────────────────────────────────────────────────────────────────────────
// small pure helpers. These are the only things in a HUD that can actually FAIL rather than
// merely look wrong, so they are what tn_hud_selfcheck() asserts.
// ─────────────────────────────────────────────────────────────────────────────

// A duration, clamped to 7 characters so it cannot run into its neighbour, and SPELLED OUT
// because the first cut wrote "77m" in FONT_TINY and the 3x5 `m` reads as an `H`: on screen it
// said "77h left" for a 77-MINUTE shift, which is a 60x error a stranger has no way to catch.
// A unit that is ambiguous in the font it is drawn in is the same bug as a number with no
// referent. Truncating rather than rounding, because this is always time LEFT and rounding 89
// minutes up to 2 hrs reads as a promise.
static const char *tnh_dur(char *buf, int cap, int minutes) {
    if (minutes < 0) minutes = 0;
    if (minutes < 90)        snprintf(buf, (size_t)cap, "%d min", minutes);
    else if (minutes < 120)  snprintf(buf, (size_t)cap, "1 hr");
    else if (minutes < 5940) snprintf(buf, (size_t)cap, "%d hrs", minutes / 60);
    else                     snprintf(buf, (size_t)cap, "99+ hrs");
    return buf;
}

// How many of N things fit in TNH_ROWS rows, and how many are then LEFT OVER. Nothing may be
// silently hidden (that is the same lie as a number you cannot interpret), so the last row is
// spent saying how many did not fit. Used by both the resident rows and the purse panel.
static int tnh_fit_rows(int items, int *overflow) {
    if (items <= TNH_ROWS) { *overflow = 0; return items < 0 ? 0 : items; }
    *overflow = items - (TNH_ROWS - 1);
    return TNH_ROWS - 1;
}

// Append, but only if the whole thing fits: a sentence cut in half mid-word is worse than a
// sentence that is not there. Returns the new length.
static int tnh_cat(char *out, int cap, const char *sep, const char *add) {
    const int have = (int)strlen(out), need = (int)strlen(add) + (have ? (int)strlen(sep) : 0);
    if (have + need >= cap) return have;
    if (have) strcat(out, sep);
    strcat(out, add);
    return have + need;
}

// The offer row for a tag on an object. A tnh_ COPY of the same three lines work.h and store.h
// each wrote, for the same reason: modules read the contract's public tables, never a sibling's
// private helper.
static const TnOffer *tnh_offer_of(int obj, int tag) {
    if (obj < 0 || obj >= tn_obj_n) return NULL;
    const int kind = tn_obj[obj].kind;
    for (int i = 0; i < TN_OFFER_N[kind]; i++)
        if (TN_OFFERS[kind][i].tag == (unsigned char)tag) return &TN_OFFERS[kind][i];
    return NULL;
}

// Right-aligned print that REFUSES rather than colliding. Every panel here is packed to the
// pixel, so the failure mode is two strings on top of each other; this makes the loser vanish
// instead, and returns where it landed so the next one can be placed to its left.
static int tnh_right(const char *s, int right, int y, int col, int min_x) {
    const int x = right - text_width(s);
    if (x < min_x) return right;
    print(s, x, y, col);
    return x;
}
static void tnh_rnum(int v, int right, int y, int col) {
    char b[16];
    snprintf(b, sizeof b, "%d", v);
    print(b, right - text_width(b), y, col);
}

// ─────────────────────────────────────────────────────────────────────────────
// READINGS OF THE MODEL. All of it derived on the spot from public state: the HUD is a VIEW and
// owns nothing the sim needs. The one exception is the money flash below, which is a memory of
// last frame and could not be anything else — an event is a DIFFERENCE.
// ─────────────────────────────────────────────────────────────────────────────

// Which need is this resident worst off for. Ties go to the earlier need, so the answer is
// stable frame to frame and the word does not flicker between two equal miseries.
static int tnh_worst_need(int agent) {
    int worst = 0;
    for (int n = 1; n < TN_NEED_COUNT; n++)
        if (tn_agent[agent].need[n] < tn_agent[agent].need[worst]) worst = n;
    return worst;
}

// Where a resident IS, as a relation to who owns it — design §6's whole comedy in one column.
// "in A's flat" while A is asleep in it is the sentence this game exists to generate.
static const char *tnh_place(int agent, char *buf, int cap) {
    const int room = tn_room_at(tn_agent[agent].tx, tn_agent[agent].ty);
    if (room < 0)                 return "outside";
    if (room == tn_shared_room()) return "in the hall";
    const int owner = tn_room_owner(room);
    if (owner < 0)                             return "in a shared room";
    if (owner == (int)tn_agent[agent].household) return "home";
    snprintf(buf, (size_t)cap, "in %c's flat", tnh_letter(owner));
    return buf;
}

// THE MONEY EVENT. Purses only ever move at a sale, a rent day, a bill or a purchase, and all
// four are invisible: the number is simply different next frame. game-feel.md's one rule is that
// every effect is tied to a specific event, so the DIFFERENCE is what gets drawn — "-20" in
// orange on flat B's row for a second and a half is how rent day is felt rather than deduced.
#define TNH_FLASH 90                            // frames ≈ 1.5s at 60fps
static short tnh_money_was[TN_MAX_HOUSEHOLDS];
static short tnh_money_dt [TN_MAX_HOUSEHOLDS];
static short tnh_flash    [TN_MAX_HOUSEHOLDS];
static int   tnh_seeded;
static void tnh_track_money(void) {
    for (int h = 0; h < TN_MAX_HOUSEHOLDS; h++) {
        const short now_m = tn_house[h].money;
        const int   d     = (int)now_m - (int)tnh_money_was[h];
        if (d != 0 && tnh_seeded) { tnh_money_dt[h] = (short)d; tnh_flash[h] = TNH_FLASH; }
        tnh_money_was[h] = now_m;
        if (tnh_flash[h] > 0) tnh_flash[h]--;
    }
    tnh_seeded = 1;                             // frame one seeds the memory without flashing
}

// The one-line news bulletin, in priority order, whole items only. Three kinds, and the first is
// the one that judges the PLAYER: a need that NOTHING in the building serves is a hole in the
// floor plan, and the verb of this game is shaping space.
static int tnh_news(char *out, int cap) {
    int n = 0;
    out[0] = 0;

    for (int nd = 0; nd < TN_NEED_COUNT && n < 2; nd++) {
        int serving = 0, worst = 255;
        for (int o = 0; o < tn_obj_n; o++) if (tnh_offer_of(o, nd)) serving++;
        for (int i = 0; i < tn_agent_n; i++)
            if (tn_agent[i].need[nd] < worst) worst = tn_agent[i].need[nd];
        if (serving == 0 && worst < 160) {
            char m[40];
            snprintf(m, sizeof m, "nowhere to %s", tnh_cure[nd]);
            tnh_cat(out, cap, "  ", m);
            n++;
        }
    }

    // Contention, over TAGS and never over kinds: how many residents are chasing this tag right
    // now against how many things offering it have room. §1 says a badly planned building shows
    // up as a queue; this is that queue, counted.
    if (n < 2) {
        int worst_tag = -1, worst_gap = 0, worst_want = 0, worst_free = 0;
        for (int tg = 0; tg < TN_TAG_COUNT; tg++) {
            int want = 0, freen = 0, total = 0;
            for (int i = 0; i < tn_agent_n; i++)
                if ((int)tn_agent[i].bid_tag == tg && tn_agent[i].activity != TN_ACT_USE) want++;
            for (int o = 0; o < tn_obj_n; o++) {
                const TnOffer *of = tnh_offer_of(o, tg);
                if (!of) continue;
                total++;
                if (tn_obj[o].users < of->capacity) freen++;
            }
            if (!want || !total) continue;
            if (want - freen > worst_gap) { worst_gap = want - freen; worst_tag = tg;
                                            worst_want = want; worst_free = freen; }
        }
        if (worst_tag >= 0) {
            char m[48];
            if (worst_free) snprintf(m, sizeof m, "%d want %s, %d free", worst_want,
                                     tnh_word(tnh_thing, worst_tag), worst_free);
            else            snprintf(m, sizeof m, "%d want %s, none free", worst_want,
                                     tnh_word(tnh_thing, worst_tag));
            tnh_cat(out, cap, "  ", m);
            n++;
        }
    }

    if (n < 2) {                                // a mess is the store module's job, made visible
        int loose = 0;
        for (int i = 0; i < tn_item_n; i++)
            if (tn_item[i].held_by < 0 && tn_item[i].stored_in < 0) loose++;
        if (loose) {
            char m[40];
            snprintf(m, sizeof m, "%d thing%s on the floor", loose, loose == 1 ? "" : "s");
            tnh_cat(out, cap, "  ", m);
            n++;
        }
    }
    return n;
}

// ─────────────────────────────────────────────────────────────────────────────
// THE BID TABLE (TAB) — the legibility argument of the whole design, so it gets the most room.
//
// It was already the right idea and the wrong table: it printed a tag and a score per row, so
// two fridges were two identical lines, a loser never said WHY it lost, and the whole thing sat
// in dark grey over the floorboards. Now it is a sorted table of the argmax's OWN TERMS —
//     bid = want x pull / (walk + own + wait)
// with one row per offer, best first, the pursued row marked `now`, and the terms read back out
// of the model (deficit, tn_path_len, tn_ownership_penalty, users vs capacity) while the BID
// itself comes from tn_score_offer(), the one true function. If a term and the score ever
// disagree, the score is still the truth and the disagreement is a finding.
// ─────────────────────────────────────────────────────────────────────────────
typedef struct { short obj; unsigned char tag; int score; } TnhBid;
static TnhBid tnh_bids[TN_MAX_OBJECTS];

// Columns, spaced from MEASURED widths rather than guessed: 5px per char in FONT_SMALL, longest
// entry per column ("counter" 7, "hall"/"mine" 4, 255, 3-digit walk, 2-digit own, 5-digit bid,
// "full"). The first cut had `want` and `walk` one pixel apart in the header and it read as one
// word, "wantwalk" — the gaps below are that bug, fixed.
#define TNH_C_WHAT   6                // left edges …
#define TNH_C_WHO   46
#define TNH_C_FLAG 176
#define TNH_R_WANT  90                // … and right edges, for the numbers
#define TNH_R_WALK 116
#define TNH_R_OWN  140
#define TNH_R_BID  172

static void tnh_draw_bids(int who) {
    const Box panel = box(2, 24, 200, 138);
    boxfill(panel, CLR_BLACK);
    boxrect(panel, CLR_DARK_GREY);
    if (who < 0 || who >= tn_agent_n) { print("nobody lives here yet", TNH_C_WHAT, 30, CLR_DARK_GREY); return; }

    const TnAgent *a = &tn_agent[who];
    char t[64], b[24];

    snprintf(t, sizeof t, "WHY DOES %c DO THIS?", tnh_letter(a->household));
    print(t, TNH_C_WHAT, 28, CLR_WHITE);

    // Line two answers it in one sentence, and the WORK case is the important one: capabilities
    // deliberately do not bid in tn_best_action (spec case 4), so when a resident is at the
    // machine every row below LOST to something that is not in the table at all. Say so.
    if (a->bid_tag >= TN_SERVE_COUNT)
        snprintf(t, sizeof t, "took a %s order, %s left", tnh_word(TAG_NAME, a->bid_tag),
                 tnh_dur(b, sizeof b, a->bid_score));
    else if (a->target_obj >= 0)
        snprintf(t, sizeof t, "chose %s, bid %d", tnh_word(tnh_thing, a->bid_tag), a->bid_score);
    else
        snprintf(t, sizeof t, "nothing on offer beats doing nothing");
    print(t, TNH_C_WHAT, 36, CLR_LIGHT_GREY);

    // ── collect every bid, not just the winner ──
    int n = 0, met = 0, mute = 0;
    for (int o = 0; o < tn_obj_n; o++) {
        int bids = 0;
        const int kind = tn_obj[o].kind;
        for (int k = 0; k < TN_OFFER_N[kind]; k++) {
            const TnOffer *of = &TN_OFFERS[kind][k];
            if (of->tag >= TN_SERVE_COUNT) continue;         // only needs bid for attention
            bids++;
            const int s = tn_score_offer(who, o, (TnTag)of->tag);
            if (s <= 0) { met++; continue; }                 // that need is already full
            if (n < (int)(sizeof tnh_bids / sizeof tnh_bids[0]))
                tnh_bids[n++] = (TnhBid){ (short)o, of->tag, s };
        }
        if (!bids) mute++;
    }
    for (int i = 0; i < n; i++) {                            // best first: a table you read down
        int pick = i;
        for (int j = i + 1; j < n; j++) if (tnh_bids[j].score > tnh_bids[pick].score) pick = j;
        const TnhBid tmp = tnh_bids[i]; tnh_bids[i] = tnh_bids[pick]; tnh_bids[pick] = tmp;
    }

    print("what", TNH_C_WHAT, 45, CLR_MEDIUM_GREY);
    print("who",  TNH_C_WHO,  45, CLR_MEDIUM_GREY);
    print("want",  70,        45, CLR_MEDIUM_GREY);
    print("walk",  96,        45, CLR_MEDIUM_GREY);
    print("own",  125,        45, CLR_MEDIUM_GREY);
    print("bid",  157,        45, CLR_MEDIUM_GREY);

    const int rows = n < 10 ? n : 10;     // 10 rows + up to 3 footer lines + the formula = 136px
    for (int i = 0; i < rows; i++) {
        const int o = tnh_bids[i].obj, tag = tnh_bids[i].tag, y = 53 + i * TNH_LINE;
        const TnOffer *of = tnh_offer_of(o, tag);
        const int owner = tn_obj[o].household;
        const bool full = of && tn_obj[o].users >= of->capacity;
        const bool now  = (o == (int)a->target_obj && tag == (int)a->bid_tag);
        const int  col  = now ? CLR_YELLOW : full ? CLR_DARK_GREY : CLR_LIGHT_GREY;

        print(tnh_kindname(tn_obj[o].kind), TNH_C_WHAT, y, col);
        if (owner < 0)                              print("hall", TNH_C_WHO, y, col);
        else if (owner == (int)a->household)        print("mine", TNH_C_WHO, y, col);
        else { snprintf(t, sizeof t, "%c's", tnh_letter(owner)); print(t, TNH_C_WHO, y, col); }

        tnh_rnum(255 - a->need[tag], TNH_R_WANT, y, col);
        const int walk = tn_path_len(a->tx, a->ty, tn_obj[o].tx, tn_obj[o].ty);
        if (walk >= TN_UNREACHABLE) tnh_right("--", TNH_R_WALK, y, CLR_RED, 0);
        else                        tnh_rnum(walk, TNH_R_WALK, y, col);
        tnh_rnum(tn_ownership_penalty(who, o, (TnTag)tag), TNH_R_OWN, y, col);
        tnh_rnum(tnh_bids[i].score, TNH_R_BID, y, now ? CLR_YELLOW : CLR_WHITE);
        if (now)       print("now",  TNH_C_FLAG, y, CLR_YELLOW);
        else if (full) print("full", TNH_C_FLAG, y, CLR_DARK_GREY);
    }

    // The footer is where the table stops lying by omission: what was left out and why. The last
    // two lines are at FIXED y so they can never collide with the notes above them, however many
    // of those there are (10 rows + 3 notes ends at 146, which is where the legend starts).
    int fy = 53 + rows * TNH_LINE + 2;
    if (n > rows) { snprintf(t, sizeof t, "+%d weaker bid%s not shown", n - rows,
                             n - rows == 1 ? "" : "s");
                    print(t, TNH_C_WHAT, fy, CLR_DARK_GREY); fy += TNH_LINE; }
    if (met)  { snprintf(t, sizeof t, "%d offer%s bid 0: that need is met", met, met == 1 ? "" : "s");
                print(t, TNH_C_WHAT, fy, CLR_DARK_GREY); fy += TNH_LINE; }
    if (mute) { snprintf(t, sizeof t, "%d thing%s offer no need at all", mute, mute == 1 ? "" : "s");
                print(t, TNH_C_WHAT, fy, CLR_DARK_GREY); fy += TNH_LINE; }
    print("now = doing it,  full = in use",     TNH_C_WHAT, 146, CLR_DARK_GREY);
    print("bid = want x pull / (walk+own+wait)", TNH_C_WHAT, 153, CLR_DARK_GREY);
}

// ─────────────────────────────────────────────────────────────────────────────
void tn_draw_hud(void) {
    char t[96], b[24], news[52];
    tnh_track_money();

    const Box screen = box(0, 0, SCREEN_W, SCREEN_H);
    Box body;
    const Box top = lay_split(screen, EDGE_TOP,    TNH_TOP_H, &body);
    const Box bot = lay_split(body,   EDGE_BOTTOM, TNH_BOT_H, &body);

    // ── the top band: the clock, the money EVENT, and the one-line news ─────
    boxfill(top, CLR_BLACK);
    rectfill(0, (int)top.h - 1, SCREEN_W, 1, CLR_DARKER_GREY);
    snprintf(t, sizeof t, "day %d  %02d:%02d", tn_clock.day, tn_clock.minute / 60,
             tn_clock.minute % 60);
    print(t, 3, 1, CLR_LIGHT_GREY);
    const int clock_end = 3 + text_width(t) + 6;
    font(FONT_SMALL);                       // before the chips: they are measured, so the font
                                            // has to be the one they will be drawn in

    {   // Rent and bills are DATED events (econ.h), and today the HUD can only know the day HAS
        // come, not that it is coming: the period is econ's private constant. So this is the
        // event, said plainly, all day — and the "-20" on the purse below is the moment itself.
        int behind = 0;
        for (int h = 0; h < tn_house_n; h++) if (tn_econ_arrears(h) > 0) behind++;
        int rx = SCREEN_W - 3;
        if (behind) {
            snprintf(t, sizeof t, "%d of %d flats owe rent", behind, tn_house_n);
            rx = tnh_right(t, rx, 2, CLR_RED, clock_end) - 6;
        }
        if      (tn_econ_rent_day()) tnh_right("RENT DAY",  rx, 2, CLR_YELLOW, clock_end);
        else if (tn_econ_bill_day()) tnh_right("BILLS DUE", rx, 2, CLR_ORANGE, clock_end);
    }
    const int items = tnh_news(news, sizeof news);
    if (items) print(news, 3, 10, CLR_ORANGE);
    {   // The hint yields to the news, but never all the way: a player who cannot find TAB never
        // sees the best thing in here, so the short form is tried when the long one will not fit.
        const int min_x = 3 + (items ? text_width(news) : 0) + 8;
        const char *hint = tnc_show_bids ? "TAB hides why" : "TAB why  Q/E turn  B build";
        if (tnh_right(hint, SCREEN_W - 3, 10, CLR_DARK_GREY, min_x) == SCREEN_W - 3)
            tnh_right("TAB why", SCREEN_W - 3, 10, CLR_DARK_GREY, min_x);
    }

    // ── the purses, top-right, where the diamond never reaches ──────────────
    // Money as a RELATION: what is in the purse, and then whichever of the three things that
    // matters — it just moved, it is owed, or it will not cover the next rent.
    {
        int over = 0;
        const int rows = tnh_fit_rows(tn_house_n, &over);
        const Box panel = box(SCREEN_W - 86, TNH_TOP_H + 1, 84, (rows + (over ? 1 : 0)) * TNH_LINE + 4);
        boxfill(panel, CLR_BLACK);
        boxrect(panel, CLR_DARKER_GREY);
        for (int h = 0; h < rows; h++) {
            const int y = (int)panel.y + 2 + h * TNH_LINE;
            const int money = tn_house[h].money, rent = tn_house[h].rent;
            const int owed = tn_econ_arrears(h);
            snprintf(t, sizeof t, "%c", tnh_letter(h));
            print(t, (int)panel.x + 3, y, CLR_MEDIUM_GREY);
            tnh_rnum(money, (int)panel.x + 40, y,
                     money <= 0 ? CLR_RED : money < rent ? CLR_ORANGE : CLR_WHITE);
            if (tnh_flash[h] > 0) {
                snprintf(t, sizeof t, "%+d", tnh_money_dt[h]);
                print(t, (int)panel.x + 46, y, tnh_money_dt[h] > 0 ? CLR_GREEN : CLR_ORANGE);
            } else if (owed > 0) {
                snprintf(t, sizeof t, "owes %d", owed);
                print(t, (int)panel.x + 46, y, CLR_RED);
            } else {
                // The resting state is the RENT, not a blank: it is what makes "200" mean
                // something ("ten weeks of it") instead of being a number in a box, and when the
                // purse can no longer cover it the same string turns orange and IS the warning.
                snprintf(t, sizeof t, "rent %d", rent);
                print(t, (int)panel.x + 46, y, money < rent ? CLR_ORANGE : CLR_DARK_GREY);
            }
        }
        if (over) {
            snprintf(t, sizeof t, "+%d more flats", over);
            print(t, (int)panel.x + 3, (int)panel.y + 2 + rows * TNH_LINE, CLR_DARK_GREY);
        }
    }

    if (tnc_show_bids) tnh_draw_bids(0);     // agent 0: the cart owns the key, so it owns the pick

    // ── the residents: one SENTENCE each, which is the whole point ──────────
    boxfill(bot, CLR_BLACK);
    rectfill(0, (int)bot.y, SCREEN_W, 1, CLR_DARKER_GREY);
    font(FONT_TINY);            // 80 columns: what a sentence per resident costs in 26px
    {
        int over = 0;
        const int rows = tnh_fit_rows(tn_agent_n, &over);
        for (int i = 0; i < rows; i++) {
            const TnAgent *a = &tn_agent[i];
            const int y = (int)bot.y + i * TNH_ROW_H;

            snprintf(t, sizeof t, "%c", tnh_letter(a->household));
            print(t, 3, y, CLR_MEDIUM_GREY);

            // WHAT, in English. Derived from the activity and the TAG it is chasing — never from
            // what the object is (contract rule 2). Carrying wins the sentence, because a person
            // holding a bolt of cloth is the only visible sign that work produced anything.
            const char *verb = tnh_word(tnh_going, a->bid_tag);
            if (a->carrying >= 0) {
                snprintf(t, sizeof t, "carrying %s", tn_item_name(a->carrying));
                verb = t;
            } else if (a->activity == TN_ACT_USE)         verb = tnh_word(tnh_doing, a->bid_tag);
            else if (a->activity == TN_ACT_OFF_LOT)       verb = "out";
            else if (a->activity == TN_ACT_IDLE)          verb = "at a loose end";
            print(verb, 10, y, CLR_WHITE);

            // The five needs as pips, then the worst one NAMED. The pips give the word a shape
            // to move in; the word is what a stranger actually reads. Column x's are the longest
            // string in each at FONT_TINY's 4px advance: "carrying leftovers" (18 -> 72px), the
            // pips (24px), "exhausted" (36), "in a shared room" (64), "bid 30600" (36) — measured,
            // so nothing can run into its neighbour at any state the sim can reach.
            const Box pips = box(88, (float)y, 24, 5);
            for (int nd = 0; nd < TN_NEED_COUNT; nd++) {
                const int v = a->need[nd];
                const Box p = lay_cell(pips, 0, TN_NEED_COUNT, nd, 1);
                boxfill(p, CLR_BROWNISH_BLACK);
                const int fill = v * (int)pips.h / 255;
                if (fill > 0)
                    rectfill((int)p.x, (int)(p.y + pips.h) - fill, (int)p.w, fill,
                             v < 64 ? CLR_RED : v < 128 ? CLR_ORANGE : CLR_MEDIUM_GREEN);
            }
            const int worst = tnh_worst_need(i), wv = a->need[worst];
            if (wv < 96) print(tnh_bad[worst], 118, y, wv < 48 ? CLR_RED : CLR_ORANGE);
            else         print("ok",           118, y, CLR_DARK_GREY);

            // WHERE, coloured by WHOSE. Being at home is the boring case and reads dim; standing
            // in a neighbour's flat is the interesting one and reads PINK, so §6's comedy catches
            // the eye across the band without anyone having to read the words first.
            const char *where = tnh_place(i, b, sizeof b);
            const int   room  = tn_room_at(a->tx, a->ty);
            const int   owner = tn_room_owner(room);
            print(where, 160, y, room < 0 ? CLR_RED
                               : (owner >= 0 && owner != (int)a->household) ? CLR_PINK
                               : owner == (int)a->household ? CLR_DARK_GREY : CLR_MEDIUM_GREY);

            // The last column is a TIME whenever there is one, because "4h left" is the only
            // number here a stranger can act on. A capability's bid_score IS its minutes left
            // (work.h keeps it there); a need's is a bid, and then the clock is `until`.
            if (a->bid_tag >= TN_SERVE_COUNT && a->activity == TN_ACT_USE) {
                snprintf(t, sizeof t, "%s left", tnh_dur(b, sizeof b, a->bid_score));
                print(t, 230, y, CLR_MEDIUM_GREY);
            } else if (a->activity == TN_ACT_USE) {
                snprintf(t, sizeof t, "%s left", tnh_dur(b, sizeof b, a->until - tn_now()));
                print(t, 230, y, CLR_MEDIUM_GREY);
            } else if (a->activity == TN_ACT_WALK && a->bid_score > 0) {
                // Only while WALKING, because only then is the bid a LIVE commitment. An idle
                // resident still carries the score of the offer it last chased, and printing that
                // is the one genuinely misleading number this band could show: it reads as a
                // decision when it is the ghost of one that just fell through.
                snprintf(t, sizeof t, "bid %d", a->bid_score);
                print(t, 230, y, CLR_DARK_GREY);
            }
        }
        if (over) {
            snprintf(t, sizeof t, "+%d more residents", over);
            print(t, 3, (int)bot.y + rows * TNH_ROW_H, CLR_DARK_GREY);
        }
    }

    font(FONT_NORMAL);      // MUST be last: the next drawer did not ask for our font
}

// ─────────────────────────────────────────────────────────────────────────────
// SPEC — runtime/spec.h's "SPECS ON AN INCLUDEABLE". Almost nothing about a HUD is assertable
// (whether a stranger can READ it is judged by eye, and whether it collides by
// `node tools/ui-audit.js tenement`), so this asserts only the things that can actually FAIL
// SILENTLY: a formatter that overruns its column, a value that would overflow a field, and the
// WORD TABLES — a missing row there prints an empty column in a screenshot nobody re-takes.
//
// NOTE FOR THE INTEGRATOR: tenement.c's spec() does not call this yet. Add
// `tn_hud_selfcheck();` beside the other module selfchecks and these run.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DE_SPEC
static char tnh_sp[160];

void tn_hud_selfcheck(void) {
    char b[24];

    // ── HUD A: THE DURATION COLUMN CANNOT OVERRUN OR BE MISREAD ─────────────
    // It is fed `until - tn_now()` and a work order's remaining minutes. `until` is ABSOLUTE
    // (model.h), so the day it meets a stale clock this gets a six-digit number, and an
    // unclamped format would print it straight over the resident's whereabouts. The clamp is one
    // assertion; the SPELLED-OUT unit is the other, and that one is a bug that shipped: "77m" in
    // FONT_TINY reads as "77h".
    expect(strcmp(tnh_dur(b, sizeof b, 0), "0 min") == 0,   "hud A: 0 minutes reads as 0 min, not blank");
    expect(strcmp(tnh_dur(b, sizeof b, 89), "89 min") == 0, "hud A: 89 minutes stays in minutes");
    expect(strcmp(tnh_dur(b, sizeof b, 90), "1 hr") == 0,
           "hud A: 90 minutes TRUNCATES to 1 hr (time LEFT must never round up into a promise)");
    expect(strcmp(tnh_dur(b, sizeof b, 480), "8 hrs") == 0, "hud A: a night's sleep reads as 8 hrs");
    expect(strcmp(tnh_dur(b, sizeof b, -50), "0 min") == 0,
           "hud A: an activity already due reads 0 min, not a negative duration");
    {
        int worst = 0, ambiguous = 0;
        for (int m = 0; m < 200000; m += 7) {
            const char *s = tnh_dur(b, sizeof b, m);
            const int w = (int)strlen(s);
            if (w > worst) worst = w;
            if (!strstr(s, "min") && !strstr(s, "hr")) ambiguous++;   // every unit spelled out
        }
        snprintf(tnh_sp, sizeof tnh_sp,
                 "hud A: no minute count anywhere prints wider than its 7-char column (worst %d)", worst);
        expect(worst <= 7, tnh_sp);
        expect(ambiguous == 0,
               "hud A: every duration names its unit in letters the 3x5 font cannot blur together");
    }

    // ── HUD B: NOTHING IS SILENTLY HIDDEN ───────────────────────────────────
    // Four rows, up to 24 residents (TN_MAX_AGENTS) and 8 households. The failure this guards is
    // the quiet one: five residents in a four-row band and the fifth simply never mentioned,
    // which is the same lie as a number you cannot interpret.
    {
        int over = 99;
        expect(tnh_fit_rows(4, &over) == 4 && over == 0,
               "hud B: four residents fill the four rows and nothing is left over");
        expect(tnh_fit_rows(5, &over) == 3 && over == 2,
               "hud B: a fifth resident costs a row to SAY so — 3 shown, '+2 more'");
        expect(tnh_fit_rows(TN_MAX_AGENTS, &over) == 3 && over == TN_MAX_AGENTS - 3,
               "hud B: a full building accounts for every resident it does not draw");
        expect(tnh_fit_rows(0, &over) == 0 && over == 0,
               "hud B: an empty world draws no rows and claims no overflow");
        int rows_ok = 1;
        for (int nn = 0; nn <= TN_MAX_AGENTS; nn++) {
            int ov = 0;
            const int r = tnh_fit_rows(nn, &ov);
            if (r + ov != nn || r > TNH_ROWS) rows_ok = 0;     // shown + hidden == everybody
        }
        expect(rows_ok, "hud B: shown + overflow == the whole population, at every size");
    }

    // ── HUD C: THE NEWS LINE APPENDS WHOLE ITEMS OR NOT AT ALL ──────────────
    {
        char out[16] = "";
        tnh_cat(out, (int)sizeof out, "  ", "bursting");
        const int len = tnh_cat(out, (int)sizeof out, "  ", "and starving");
        snprintf(tnh_sp, sizeof tnh_sp,
                 "hud C: a news item that does not fit is DROPPED, never cut mid-word (\"%s\")", out);
        expect(strcmp(out, "bursting") == 0 && len == 8, tnh_sp);
        expect(tnh_cat(out, (int)sizeof out, "  ", "x") == 11 && strcmp(out, "bursting  x") == 0,
               "hud C: one that does fit is appended with its separator");
    }

    // ── HUD D: EVERY TAG AND EVERY KIND HAS A WORD ──────────────────────────
    // A new tag or object kind is a table row (contract rule 3) — including in here. Miss one and
    // the column is BLANK in a screenshot, which is exactly the failure this file exists to fix.
    {
        int missing = 0;
        for (int tg = 0; tg < TN_TAG_COUNT; tg++) {
            if (!TAG_NAME[tg] || !tnh_doing[tg] || !tnh_going[tg] || !tnh_thing[tg]) missing++;
        }
        for (int k = 0; k < TN_OBJ_KIND_COUNT; k++) if (!tnh_kind_word[k]) missing++;
        for (int nd = 0; nd < TN_NEED_COUNT; nd++) if (!tnh_cure[nd] || !tnh_bad[nd]) missing++;
        snprintf(tnh_sp, sizeof tnh_sp,
                 "hud D: every one of %d tags, %d kinds and %d needs has a display word (%d missing)",
                 TN_TAG_COUNT, TN_OBJ_KIND_COUNT, TN_NEED_COUNT, missing);
        expect(missing == 0, tnh_sp);
        expect(strcmp(tnh_word(tnh_doing, TN_TAG_COUNT), "?") == 0 &&
               strcmp(tnh_word(tnh_doing, -1), "?") == 0,
               "hud D: an out-of-range tag reads '?' rather than off the end of the table");
        expect(tnh_letter(0) == 'A' && tnh_letter(3) == 'D' && tnh_letter(-1) == '-',
               "hud D: household 0 is flat A everywhere, and a bad index is a dash");
    }

    // ── HUD E: THE WORST NEED IS STABLE AND HONEST ──────────────────────────
    // The word beside the pips must name the need that is actually lowest, and must not flicker
    // between two equal miseries (a row that changes its mind every frame is unreadable).
    tn_world_init();
    tn_obj_n = 0; tn_agent_n = 0;
    tn_add_agent(0, 1, 1);
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 200;
    tn_agent[0].need[TN_SERVE_BLADDER] = 12;
    expect(tnh_worst_need(0) == TN_SERVE_BLADDER && strcmp(tnh_bad[tnh_worst_need(0)], "bursting") == 0,
           "hud E: the resident nearest to a disaster is named by the RIGHT disaster");
    for (int n = 0; n < TN_NEED_COUNT; n++) tn_agent[0].need[n] = 12;
    expect(tnh_worst_need(0) == 0,
           "hud E: an all-equal tie resolves to the FIRST need, so the word cannot flicker");

    // ── HUD F: WHERE SOMEBODY IS, SAID AS OWNERSHIP ─────────────────────────
    // Design §6's comedy is only funny if it is READABLE, and "in A's flat" is the whole joke.
    tn_world_init();
    {
        // Its own buffer, NOT tnh_sp: tnh_place() writes into the buffer it is handed, and
        // building the failure message in that same buffer would rewrite the answer before
        // strcmp saw it — an assertion that passes for the wrong reason.
        char where[24];
        const int me = 0;                     // world.h houses household 0 in flat A at (3,1)
        tn_agent[me].tx = 3; tn_agent[me].ty = 1;
        expect(strcmp(tnh_place(me, where, sizeof where), "home") == 0,
               "hud F: a resident in its own flat is 'home', not 'in A's flat'");
        tn_agent[me].tx = 6; tn_agent[me].ty = 4;
        expect(strcmp(tnh_place(me, where, sizeof where), "in the hall") == 0,
               "hud F: the corridor everyone crosses says so");
        tn_agent[me].tx = 9; tn_agent[me].ty = 1;
        const char *p = tnh_place(me, where, sizeof where);
        snprintf(tnh_sp, sizeof tnh_sp, "hud F: a resident of A standing in B's flat reads \"%s\"", p);
        expect(strcmp(p, "in B's flat") == 0, tnh_sp);
    }
    tn_world_init();                          // leave the world as we found it
}
#endif // DE_SPEC

#endif // TENEMENT_HUD_H
