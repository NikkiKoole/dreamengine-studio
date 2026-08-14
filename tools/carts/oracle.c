/* de:meta
{
  "slug": "oracle",
  "title": "oracle",
  "status": "active",
  "created": "2026-08-14",
  "kind": [
    "game"
  ],
  "teaches": [
    "algorithm-visualization",
    "state-machine",
    "title-play-gameover-loop",
    "save-load-persistence"
  ],
  "lineage": "The mechanism mule's ore auction gestures at (basep -= 4 per sale) built properly: a real LMSR cost function with simulated order flow, instead of the reroll() random-walk price table that druglord, tradewinds and merchant all share. First cart in the repo where a price is DISCOVERED rather than drawn from a distribution.",
  "genre": "simulation",
  "description": {
    "summary": "An insider's prediction market: you hear things before the crowd does, from people who are not always right.",
    "detail": "Eight markets, each a yes-or-no question with a hidden answer. The price is run by an LMSR automated market maker — the same cost function the real event exchanges use — so every trade genuinely moves it, and the line on the chart is the residue of who paid what rather than a scripted curve. But the price alone is useless to you: it is a correct Bayesian posterior over the crowd's evidence, and betting into a calibrated price pays exactly zero. Your edge is PRIVATE. Sources whisper to you before the crowd works it out — the harbourmaster is right 80% of the time, the tavern drunk barely better than a coin — and the bar shows two markers: what the market thinks, and what YOU think once you have weighed your sources. The gap between them is the only money on the table. Buy it and wait for the crowd to catch up, or discover your source was the drunk. A run ends with your calibration scored: when your read said 70%, how often were you right?",
    "controls": "Click BUY YES / BUY NO / SELL ALL, or keys Z buy yes, X buy no, C sell all. Q cycles lot size. SPACE pauses the clock. In MAKER mode pick THIN/MID/DEEP liquidity before each market and press P to pull out early. ENTER advances a resolved market, R restarts a finished run."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "endcard.h"

// ORACLE — a prediction market you can actually see the inside of.
//
// The honest core is the LMSR (logarithmic market scoring rule), Hanson's
// automated market maker and the mechanism the real event exchanges run on.
// A single scalar pair (qy, qn) — shares outstanding on each side — IS the
// market. Price falls out of it, cost of a trade falls out of it, and the
// maker's worst-case loss is bounded by b*ln(2). Nothing else stores a price.
//
//     p_yes = e^(qy/b) / (e^(qy/b) + e^(qn/b))        the price IS a probability
//     C     = b * ln(e^(qy/b) + e^(qn/b))             the cost function
//     cost  = C(after) - C(before)                    what a trade costs you
//
// Why the price MOVES is the other half. Each tick the world may generate a
// piece of evidence, correct with probability KAPPA. The informed crowd knows
// the running posterior; noise traders don't. Informed money pushes the price
// toward that posterior, which itself walks toward the hidden truth because
// correct evidence accumulates faster than wrong evidence. So the chart line
// is emergent — it is the residue of who paid what, never a scripted curve.
//
// That distinction is the whole cart: druglord/tradewinds/merchant re-roll a
// price from a distribution every day. Here nobody sets the price at all.
//
// ── the thing v1 got wrong, because it is worth not repeating ──────────────
// The first version had only the above, and it was boring — not badly tuned,
// STRUCTURALLY boring, and the measurement says why. The price is a correct
// Bayesian posterior; a correct posterior is calibrated; betting into a
// calibrated price returns exactly zero however cleverly you do it (measured
// at -0.001 per $1 over 20000 markets). The player held precisely the crowd's
// information, so no choice anywhere in the cart had a consequence. It was a
// spoiler with buttons: watch the line, press the matching button, feel
// nothing.
//
// The fix is not tuning, it is asymmetry. You get PRIVATE sources the price
// does not contain, so your posterior legitimately differs from the market's,
// and the gap is the only money on the table. Sources vary from an 80%
// harbourmaster to a 54% tavern drunk, so the judgement is who to believe —
// and the calibration screen scores exactly that. spec()'s "FUN GATE" pins it:
// a player who reads their sources must out-earn one who follows the line.
//
// MOUSE: the three trade buttons; the lot chip cycles size.
// KEYS:  Z buy yes, X buy no, C sell all, Q lot size, SPACE pause,
//        P pull out (maker), ENTER continue, R restart.

// ---- tuning ---------------------------------------------------------------
#define NMARKET      8      // markets in a run
#define TICKS       26      // ticks a market lives for
#define TICKFRAMES  48      // frames per tick (0.8s at 60fps)
#define START_BANK 1000
#define MAXTRADE    64      // calibration log capacity

// The crowd's evidence stream. These two numbers decide whether the cart is a
// GAME or a spoiler, so they were picked by simulation, not by feel (8000
// markets per config):
//
//   evidence/tick  accuracy   mean closing confidence   markets already decided
//        0.55        0.72             0.96                      86%
//        0.30        0.66             0.82                      38%   <- chosen
//        0.22        0.63             0.73                      11%
//
// The first row was the original tuning and it is why v1 was boring: the
// crowd had the answer by mid-market, so the last third was a formality.
// Softer evidence keeps the price genuinely undecided at the bell, which is
// what keeps a private tip worth acting on right to the end.
#define KAPPA     0.66f     // per-evidence accuracy: how often the world tells the truth
#define EVCHANCE  0.30f     // chance a tick produces any evidence at all

// LMSR liquidity. Higher b = deeper book: trades move the price less, the
// maker captures more flow but carries more risk. Worst-case maker loss is
// exactly b*ln(2), which spec() pins.
#define B_THIN     90.0f
#define B_MID     200.0f
#define B_DEEP    420.0f

enum { MODE_TRADER, MODE_MAKER };
enum { ST_PICK, ST_LIQ, ST_TRADE, ST_RESOLVE, ST_SUMMARY };

// ---- the market maker (pure) ----------------------------------------------
// Everything below is a pure function of (qy, qn, b). No globals, no side
// effects — which is exactly why spec() can pin it hard.
//
// de_expf/de_logf, not expf/logf: demath.h's rule is that anything whose
// output gets COMPARED must be bit-identical across architectures, and
// spec() compares. See docs/design/determinism.md.

static float lmsr_cost(float qy, float qn, float b) {
    float m = qy > qn ? qy : qn;                       // subtract the max: e^(big/b) overflows
    return m + b * de_logf(de_expf((qy - m) / b) + de_expf((qn - m) / b));
}

static float lmsr_price(float qy, float qn, float b) {
    float d = (qy - qn) / b;                           // logistic, both tails stable
    if (d >= 0) { float e = de_expf(-d); return 1.0f / (1.0f + e); }
    float e = de_expf(d);  return e / (1.0f + e);
}

// What it costs to add `n` shares to one side. Negative n = selling back.
static float lmsr_trade_cost(float qy, float qn, float b, int side, float n) {
    float ny = qy + (side == 0 ? n : 0.0f);
    float nn = qn + (side == 1 ? n : 0.0f);
    return lmsr_cost(ny, nn, b) - lmsr_cost(qy, qn, b);
}

// ---- the questions --------------------------------------------------------
static const char *QUESTION[] = {
    "WILL THE HARBOUR FREEZE BEFORE SPRING?",
    "WILL THE OLD BRIDGE REOPEN THIS YEAR?",
    "WILL THE MAYOR RESIGN BY MIDSUMMER?",
    "WILL THE COMET BE VISIBLE FROM TOWN?",
    "WILL THE MILL STRIKE LAST A FORTNIGHT?",
    "WILL THE FERRY LINE BE SOLD?",
    "WILL IT SNOW ON MARKET DAY?",
    "WILL THE LIGHTHOUSE KEEPER RETIRE?",
    "WILL THE CANAL FLOOD THE LOW QUARTER?",
    "WILL THE BREWERY WIN ITS APPEAL?",
    "WILL THE NIGHT TRAIN BE CUT?",
    "WILL THE SEA WALL HOLD THE AUTUMN TIDE?",
};
#define NQUESTION ((int)(sizeof QUESTION / sizeof QUESTION[0]))

// Public-evidence headlines, one pair per question — [0] points NO, [1] YES.
static const char *NEWSLINE[2] = {
    "word from the docks runs against it",
    "word from the docks runs for it",
};

// ---- your sources ---------------------------------------------------------
// The whole game lives here. The market price is a CORRECT posterior over the
// crowd's evidence, and a correct posterior is calibrated by construction —
// so trading on the price alone has an expected value of exactly zero, no
// matter how cleverly you do it. (Measured: -0.001 per $1 over 20000 markets.)
// The ONLY thing that can beat it is information the price does not contain.
//
// That is what a source is. Each tip is private, arrives before the crowd
// works it out for itself, and is right only as often as the source is
// trustworthy. A whisper from the harbourmaster is worth acting on; the same
// whisper from a drunk is worth almost nothing, and telling those two apart
// IS the skill the calibration screen scores.
#define NSOURCE 6
#define MAXTIP  3
static const char *SRC_NAME[NSOURCE] = {
    "the harbourmaster",
    "a customs clerk",
    "the pilot's wife",
    "the newspaper boy",
    "a bargeman",
    "the tavern drunk",
};
static const float SRC_REL[NSOURCE] = { 0.80f, 0.72f, 0.66f, 0.62f, 0.58f, 0.54f };
static const char *SRC_TAG[NSOURCE] = { "reliable", "solid", "fair", "shaky", "shaky", "dubious" };
static const char *SRC_SAYS[NSOURCE][2] = {
    { "the channel will stay open",   "the ice is thickening already" },
    { "the paperwork has gone quiet", "the paperwork is moving fast"  },
    { "she has heard nothing",        "she says it is all but done"   },
    { "nobody downtown believes it",  "everyone downtown expects it"  },
    { "the crews are standing down",  "the crews are being called in" },
    { "swears it will not happen",    "swears it is a certainty"      },
};

// ---- run + market state ---------------------------------------------------
static int   mode;                    // MODE_TRADER / MODE_MAKER
static int   st;                      // ST_*
static int   bank, best;
static int   marketno;                // 1..NMARKET
static int   qidx;                    // which question
static int   truth;                   // the hidden answer (0/1) — only the cart knows
static float qy, qn;                  // LMSR shares outstanding
static float lb = B_MID;              // liquidity in play
static int   liqpick = 1;             // 0 thin / 1 mid / 2 deep
static int   tick, frames, frozen;
static int   ev_y, ev_n;              // evidence the world has generated
static float hist[TICKS + 1];         // price history — the chart
static int   histnews[TICKS + 1];     // did public news land on that tick
static int   nhist;
static const char *news;              // current headline, NULL when quiet
static int   news_age;

// your private tips: rolled at market open, revealed one at a time
static int   tip_src[MAXTIP], tip_dir[MAXTIP], tip_at[MAXTIP];
static int   ntip, ntip_seen, tip_flash;

// player, trader mode
static float p_yes, p_no;             // shares held
static float p_spent;                 // cash paid in on the open position
static int   lotsel = 1;              // 0/1/2 -> 10/50/200
static const int LOT[3] = { 10, 50, 200 };

// player, maker mode
static float mk_cash;                 // premium collected this market
static int   mk_out;                  // pulled out early

// resolution + presentation
static int   last_pnl;
static float end_t, showbank;
static int   jolt;                    // frames of price-bar shake left

// calibration log — the soul of the summary screen
static float cal_p[MAXTRADE];         // the probability you paid for your side
static int   cal_win[MAXTRADE];       // did that side resolve true
static int   ncal;

static float frnd(void) { return rnd(10000) / 10000.0f; }

// ---- the world ------------------------------------------------------------
// The informed crowd's belief: the Bayesian posterior from all evidence so
// far. Each piece is correct with probability KAPPA, so in log-odds each one
// is worth ln(KAPPA/(1-KAPPA)) in its own direction. Correct evidence
// outnumbers wrong evidence over time, so this walks toward `truth`.
static float world_belief(void) {
    float w  = de_logf(KAPPA / (1.0f - KAPPA));
    float lo = (ev_y - ev_n) * w;
    if (lo >= 0) { float e = de_expf(-lo); return 1.0f / (1.0f + e); }
    float e = de_expf(lo);  return e / (1.0f + e);
}

// YOUR READ — the market price updated by the tips only you have heard.
//
// Bayes in log-odds, which is just addition: start from the price (the
// crowd's posterior, and a perfectly good prior) and add each source's
// weight, ln(r/(1-r)), in the direction it points. A 0.80 source is worth
// +1.39; the tavern drunk at 0.54 is worth +0.16 and barely tilts anything,
// which is exactly the point. The GAP between this and the price is your
// entire edge, and the cart draws it on the bar so you can see it.
static float player_read(void) {
    float p = lmsr_price(qy, qn, lb);
    if (p < 0.001f) p = 0.001f;
    if (p > 0.999f) p = 0.999f;
    float lo = de_logf(p / (1.0f - p));
    for (int i = 0; i < ntip_seen; i++) {
        float r = SRC_REL[tip_src[i]];
        lo += de_logf(r / (1.0f - r)) * (tip_dir[i] ? 1.0f : -1.0f);
    }
    if (lo >= 0) { float e = de_expf(-lo); return 1.0f / (1.0f + e); }
    float e = de_expf(lo);  return e / (1.0f + e);
}

// Apply a trade to the book and return what it cost. This is the ONLY place
// the price changes — the maker is the single source of truth.
static float fill(int side, float n) {
    float c = lmsr_trade_cost(qy, qn, lb, side, n);
    if (side == 0) qy += n; else qn += n;
    mk_cash += c;
    return c;
}

// One tick of order flow. Informed traders trade toward the crowd posterior,
// sized by how mispriced the book looks to them; noise traders just splash.
static void trade_flow(void) {
    int n = 1 + rnd(3);
    for (int i = 0; i < n; i++) {
        float p = lmsr_price(qy, qn, lb);
        if (frnd() < 0.40f) {                          // INFORMED
            float belief = world_belief();
            float edge   = belief - p;
            if (edge > 0.04f)       fill(0, 8.0f + edge * 140.0f);
            else if (edge < -0.04f) fill(1, 8.0f + (-edge) * 140.0f);
        } else {                                        // NOISE
            fill(rnd(2), 4.0f + frnd() * 30.0f);
        }
    }
}

static void push_hist(int hadnews) {
    if (nhist > TICKS) return;
    hist[nhist]     = lmsr_price(qy, qn, lb);
    histnews[nhist] = hadnews;
    nhist++;
}

// Roll this market's private tips. Each is correct with its source's
// reliability — so a good source usually points at the truth and a bad one is
// close to a coin. They land on separate ticks so they arrive as events.
static void roll_tips(void) {
    ntip = 1 + rnd(MAXTIP);                            // 1..3
    ntip_seen = 0;
    for (int i = 0; i < ntip; i++) {
        tip_src[i] = rnd(NSOURCE);
        float r = SRC_REL[tip_src[i]];
        tip_dir[i] = (frnd() < r) ? truth : !truth;
        tip_at[i]  = 1 + rnd(TICKS - 6);               // never in the last few ticks
    }
    for (int i = 1; i < ntip; i++)                     // sort so they reveal in order
        for (int j = i; j > 0 && tip_at[j] < tip_at[j - 1]; j--) {
            int a = tip_at[j]; tip_at[j] = tip_at[j-1]; tip_at[j-1] = a;
            a = tip_src[j]; tip_src[j] = tip_src[j-1]; tip_src[j-1] = a;
            a = tip_dir[j]; tip_dir[j] = tip_dir[j-1]; tip_dir[j-1] = a;
        }
}

static void new_market(void) {
    qidx  = rnd(NQUESTION);
    truth = rnd(2);
    qy = qn = 0.0f;
    ev_y = ev_n = 0;
    tick = frames = 0;
    nhist = 0;
    news = NULL; news_age = 0;
    roll_tips();
    tip_flash = 0;
    p_yes = p_no = 0.0f; p_spent = 0.0f;
    mk_cash = 0.0f; mk_out = 0;
    jolt = 0;
    lb = (liqpick == 0) ? B_THIN : (liqpick == 2) ? B_DEEP : B_MID;

    // Seed the book so it doesn't open at a dead-flat 50%.
    for (int i = 0; i < 3; i++) fill(rnd(2), 4.0f + frnd() * 20.0f);
    mk_cash = 0.0f;                                    // seeding isn't the player's flow
    push_hist(0);
}

// One market tick: the world may learn something, then the crowd trades on it.
static void step_tick(void) {
    int hadnews = 0;

    // Your source speaks first — before the crowd's own evidence lands, so
    // there is a window where you know something the price does not.
    while (ntip_seen < ntip && tip_at[ntip_seen] <= tick) { ntip_seen++; tip_flash = 30; }

    if (frnd() < EVCHANCE) {                           // a piece of evidence exists
        int sig = (frnd() < KAPPA) ? truth : !truth;
        if (sig) ev_y++; else ev_n++;
        if (frnd() < 0.30f) {                          // ...and it goes PUBLIC
            news = NEWSLINE[sig];
            news_age = 0;
            hadnews = 1;
        }
    }
    float before = lmsr_price(qy, qn, lb);
    trade_flow();
    float after = lmsr_price(qy, qn, lb);
    float moved = after - before; if (moved < 0) moved = -moved;
    if (moved > 0.03f) jolt = 8;

    push_hist(hadnews);
    tick++;
    news_age++;
    if (news_age > 4) news = NULL;
}

// ---- resolution -----------------------------------------------------------
static void resolve(void) {
    if (mode == MODE_TRADER) {
        float payout = truth ? p_yes : p_no;           // winning shares pay $1
        last_pnl = (int)(payout - p_spent);
        bank += (int)payout;
    } else {
        // Maker P&L telescopes: every premium collected, minus what the
        // winning side is owed. Bounded below by -lb*ln(2), which spec() pins.
        float owed = mk_out ? 0.0f : (truth ? qy : qn);
        float paid = mk_out ? 0.0f : 0.0f;
        (void)paid;
        last_pnl = (int)(mk_cash - owed);
        bank += last_pnl;
    }
    if (bank > best) { best = bank; save_int("best", best); }
    end_t = 0.0f;
    st = ST_RESOLVE;
}

// Log what YOU believed, not what you paid. Scoring the price would only
// re-measure the market's calibration (which is perfect by construction and
// therefore says nothing about you). Scoring your READ asks the question the
// cart is actually about: when you thought 70%, were you right 70% of the time?
static void log_trade(float belief, int side) {
    if (ncal >= MAXTRADE) return;
    cal_p[ncal]   = belief;
    cal_win[ncal] = (side == 0) ? truth : !truth;      // filled in now; the cart knows the answer
    ncal++;
}

static void buy(int side) {
    if (st != ST_TRADE || mode != MODE_TRADER) return;
    float n = (float)LOT[lotsel];
    float read = player_read();
    float belief = (side == 0) ? read : 1.0f - read;
    float c = fill(side, n);
    if (c > bank) { qy -= (side == 0 ? n : 0); qn -= (side == 1 ? n : 0); mk_cash -= c; return; }
    bank -= (int)c;
    p_spent += c;
    if (side == 0) p_yes += n; else p_no += n;
    log_trade(belief, side);
    jolt = 8;
    sfx(0);
}

// Sell the whole position back to the maker at the current price.
static void sell_all(void) {
    if (st != ST_TRADE || mode != MODE_TRADER) return;
    if (p_yes <= 0.0f && p_no <= 0.0f) return;
    float got = 0.0f;
    if (p_yes > 0.0f) got += -fill(0, -p_yes);
    if (p_no  > 0.0f) got += -fill(1, -p_no);
    bank += (int)got;
    p_spent -= got;
    p_yes = p_no = 0.0f;
    jolt = 8;
    sfx(0);
}

// Commit the maker's chosen liquidity and re-seed the book at that depth.
static void open_book(void) {
    lb = (liqpick == 0) ? B_THIN : (liqpick == 2) ? B_DEEP : B_MID;
    qy = qn = 0.0f; nhist = 0; mk_cash = 0.0f;
    for (int i = 0; i < 3; i++) fill(rnd(2), 4.0f + frnd() * 20.0f);
    mk_cash = 0.0f;                                    // seeding isn't the player's flow
    push_hist(0);
    st = ST_TRADE;
}

static void new_run(void) {
    bank = START_BANK;
    marketno = 1;
    ncal = 0;
    showbank = (float)bank;
    new_market();
    st = (mode == MODE_MAKER) ? ST_LIQ : ST_TRADE;
}

void init(void) {
    best = load_int("best", 0);
    mode = MODE_TRADER;
    st = ST_PICK;
    showbank = START_BANK;
}

// ---- update ---------------------------------------------------------------
void update(void) {
    showbank = lerp(showbank, (float)bank, 0.18f);
    if (jolt > 0) jolt--;
    if (tip_flash > 0) tip_flash--;

    if (st == ST_PICK) {
        if (keyp(KEY_LEFT)  || keyp('A')) mode = MODE_TRADER;
        if (keyp(KEY_RIGHT) || keyp('D')) mode = MODE_MAKER;
        if (keyp(KEY_ENTER) || keyp(KEY_SPACE)) new_run();
        return;
    }

    if (st == ST_LIQ) {
        if (keyp(KEY_LEFT))  liqpick = liqpick > 0 ? liqpick - 1 : 0;
        if (keyp(KEY_RIGHT)) liqpick = liqpick < 2 ? liqpick + 1 : 2;
        if (keyp(KEY_ENTER) || keyp(KEY_SPACE)) open_book();
        return;
    }

    if (st == ST_TRADE) {
        if (keyp(KEY_SPACE)) frozen = !frozen;
        if (mode == MODE_TRADER) {
            if (keyp('Z')) buy(0);
            if (keyp('X')) buy(1);
            if (keyp('C')) sell_all();
            if (keyp('Q')) lotsel = (lotsel + 1) % 3;
        } else {
            if (keyp('P') && !mk_out) { mk_out = 1; resolve(); return; }
        }
        if (!frozen) {
            frames++;
            if (frames >= TICKFRAMES) {
                frames = 0;
                step_tick();
                if (tick >= TICKS) resolve();
            }
        }
        return;
    }

    if (st == ST_RESOLVE) {
        end_t += dt();
        if (end_t > 0.5f && (keyp(KEY_ENTER) || keyp(KEY_SPACE) || mouse_pressed(MOUSE_LEFT))) {
            if (marketno >= NMARKET || bank <= 0) { st = ST_SUMMARY; end_t = 0.0f; }
            else { marketno++; new_market(); st = (mode == MODE_MAKER) ? ST_LIQ : ST_TRADE; }
        }
        return;
    }

    if (st == ST_SUMMARY) {
        end_t += dt();
        if (keyp('R') || keyp(KEY_ENTER)) st = ST_PICK;
        return;
    }
}

// ---- drawing --------------------------------------------------------------

static void draw_hud(void) {
    rectfill(0, 0, SCREEN_W, 11, CLR_BROWNISH_BLACK);
    line(0, 11, SCREEN_W - 1, 11, CLR_DARKER_GREY);
    print("ORACLE", 4, 2, CLR_LIGHT_YELLOW);
    print_centered(str("MARKET %d/%d", marketno, NMARKET), SCREEN_W / 2, 2, CLR_LIGHT_GREY);
    print_right(str("$%d", (int)showbank), SCREEN_W - 4, 2, bank >= START_BANK ? CLR_LIME_GREEN : CLR_RED);
}

// The probability bar. bar() is a studio.h builtin (studio.h:232) — the fill
// is the price, so this widget IS the market's current belief.
static void draw_probbar(int x, int y, int w) {
    float p = lmsr_price(qy, qn, lb);
    int   sh = (jolt > 0) ? ((jolt & 1) ? 1 : -1) : 0;
    int   pct = (int)(p * 100.0f + 0.5f);
    bar(x + sh, y, w, 14, p, CLR_MEDIUM_GREEN, CLR_DARK_RED);
    rect(x + sh, y, w, 14, CLR_DARKER_GREY);
    print(str("YES %d%%", pct), x + sh + 4, y + 4, CLR_WHITE);
    print_right(str("NO %d%%", 100 - pct), x + sh + w - 4, y + 4, CLR_WHITE);

    // YOUR READ — the caret. The distance between it and the price edge is
    // the whole game: money is only ever on the table when they disagree.
    if (mode != MODE_TRADER || ntip_seen == 0) return;
    float rd = player_read();
    int   rx = x + sh + (int)(rd * (w - 1));
    int   px = x + sh + (int)(p  * (w - 1));

    if (rx != px) {                                    // shade the gap you can trade
        int lo = rx < px ? rx : px, hi = rx < px ? px : rx;
        blend(BLEND_AVG);
        rectfill(lo, y - 4, hi - lo + 1, 3, CLR_YELLOW);
        blend_reset();
    }
    for (int i = 0; i < 3; i++) pset(rx, y - 8 + i, CLR_YELLOW);
    line(rx - 2, y - 9, rx + 2, y - 9, CLR_YELLOW);
    line(rx, y, rx, y + 13, CLR_LIGHT_YELLOW);

    font(FONT_SMALL);
    int lx = rx - 14; if (lx < x) lx = x; if (lx > x + w - 30) lx = x + w - 30;
    print(str("you %d%%", (int)(rd * 100.0f + 0.5f)), lx, y - 16, CLR_LIGHT_YELLOW);
    font(FONT_NORMAL);
}

// What only you have heard. The reliability tag is the load-bearing part —
// the same claim from the harbourmaster and from the tavern drunk should feel
// like two completely different things.
static void draw_sources(int x, int y, int w) {
    font(FONT_SMALL);
    if (ntip_seen == 0) {
        print("YOUR SOURCES", x, y, CLR_DARK_GREY);
        print("nobody has told you anything yet", x + 68, y, CLR_DARK_GREY);
        font(FONT_NORMAL);
        return;
    }
    // Three columns only. The flavour quote used to live here as a fourth and
    // collided with everything — FONT_SMALL advances 5px/char, not the 4 its
    // name suggests — so the quote is an EVENT in the news slot instead, which
    // also makes a tip's arrival feel like something happening.
    int fresh = (tip_flash > 0 && (tip_flash / 4) % 2 == 0);
    print("YOUR SOURCES", x, y, fresh ? CLR_YELLOW : CLR_LIGHT_YELLOW);
    for (int i = 0; i < ntip_seen; i++) {
        int s  = tip_src[i], yy = y + 8 + i * 8;
        int hot = (i == ntip_seen - 1 && fresh);
        print(SRC_NAME[s], x + 8, yy, hot ? CLR_WHITE : CLR_LIGHT_GREY);
        print(SRC_TAG[s], x + 104, yy,
              SRC_REL[s] >= 0.70f ? CLR_LIME_GREEN : SRC_REL[s] >= 0.60f ? CLR_ORANGE : CLR_DARK_PEACH);
        print(str("says %s", tip_dir[i] ? "YES" : "NO"), x + 156, yy,
              tip_dir[i] ? CLR_MEDIUM_GREEN : CLR_DARK_PEACH);
    }
    font(FONT_NORMAL);
}

// Price history. The ring-walk + break-on-gap idiom from pitchscope.c:82,
// with the CRT bezel treatment from indicators.c so it reads as an instrument.
static void draw_chart(int x, int y, int w, int h) {
    rrectfill(x - 3, y - 3, w + 6, h + 6, 3, CLR_DARK_GREY);
    rectfill(x, y, w, h, CLR_BROWNISH_BLACK);

    blend(BLEND_AVG);
    for (int i = 1; i < 4; i++) line(x, y + h * i / 4, x + w - 1, y + h * i / 4, CLR_BLUE_GREEN);
    blend_reset();
    for (int xx = x; xx < x + w; xx += 4) pset(xx, y + h / 2, CLR_DARK_GREEN);   // the 50% line

    if (nhist < 1) return;
    int px = -1, py = 0;
    for (int i = 0; i < nhist; i++) {
        int cx = x + (w - 1) * i / TICKS;
        int cy = y + h - 1 - (int)(hist[i] * (h - 2));
        if (histnews[i]) {                                    // public news landed here
            for (int yy = y; yy < y + h; yy += 3) pset(cx, yy, CLR_INDIGO);
        }
        if (px >= 0) line(px, py, cx, cy, CLR_LIME_GREEN);
        else pset(cx, cy, CLR_LIME_GREEN);
        px = cx; py = cy;
    }
    circfill(px, py, 2, CLR_YELLOW);                          // the live price
}

static void draw_position(int y) {
    font(FONT_SMALL);
    if (mode == MODE_TRADER) {
        float p = lmsr_price(qy, qn, lb);
        float mark = p_yes * p + p_no * (1.0f - p);           // mark-to-market
        int   open = (int)(mark - p_spent);
        if (p_yes > 0.0f || p_no > 0.0f) {
            if (p_yes > 0.0f) print(str("YES %d", (int)p_yes), 6, y, CLR_MEDIUM_GREEN);
            if (p_no  > 0.0f) print(str("NO %d",  (int)p_no),  6, y + 7, CLR_DARK_PEACH);
            print_right(str("open %s$%d", open >= 0 ? "+" : "-", open < 0 ? -open : open),
                        SCREEN_W - 6, y, open >= 0 ? CLR_LIME_GREEN : CLR_RED);
        } else {
            print("no position", 6, y, CLR_DARK_GREY);
        }
        print_right(str("lot %d  [Q]", LOT[lotsel]), SCREEN_W - 6, y + 7, CLR_LIGHT_GREY);
    } else {
        print(str("book  YES %d / NO %d", (int)qy, (int)qn), 6, y, CLR_LIGHT_GREY);
        print(str("premium $%d", (int)mk_cash), 6, y + 7, CLR_LIME_GREEN);
        print_right(str("risk -$%d  [P] pull out", (int)(lb * 0.6931f)),
                    SCREEN_W - 6, y + 3, CLR_ORANGE);
    }
    font(FONT_NORMAL);
}

static void draw_trade(void) {
    cls(CLR_DARKER_BLUE);
    draw_hud();

    font(FONT_SMALL);
    print_centered(QUESTION[qidx], SCREEN_W / 2, 14, CLR_LIGHT_YELLOW);
    font(FONT_NORMAL);

    draw_probbar(12, 38, SCREEN_W - 24);               // y=38: the read caret needs headroom
    draw_chart(14, 58, SCREEN_W - 28, 42);
    draw_sources(8, 100, SCREEN_W - 16);

    // One slot, three priorities: your fresh tip beats public news beats the clock.
    font(FONT_SMALL);
    if (tip_flash > 0 && ntip_seen > 0) {
        int s = tip_src[ntip_seen - 1];
        print_centered(str("\"%s\"  - %s", SRC_SAYS[s][tip_dir[ntip_seen - 1]], SRC_NAME[s]),
                       SCREEN_W / 2, 132, CLR_LIGHT_YELLOW);
    }
    else if (news) print_centered(str("NEWS: %s", news), SCREEN_W / 2, 132, CLR_PINK);
    else print_centered(str("tick %d/%d%s", tick, TICKS, frozen ? "   PAUSED" : ""),
                        SCREEN_W / 2, 132, CLR_DARK_GREY);
    font(FONT_NORMAL);

    draw_position(141);

    ui_begin();
    if (mode == MODE_TRADER) {
        if (ui_button(10,  158, 92, 18, "BUY YES  Z")) buy(0);
        if (ui_button(114, 158, 92, 18, "BUY NO  X"))  buy(1);
        if (ui_button(218, 158, 92, 18, "SELL  C"))    sell_all();
        if (ui_button(10,  180, 92, 16, str("LOT %d", LOT[lotsel]))) lotsel = (lotsel + 1) % 3;
    } else {
        if (ui_button(10, 158, 140, 18, "PULL OUT  P") && !mk_out) { mk_out = 1; resolve(); }
    }
    if (ui_button(218, 180, 92, 16, frozen ? "RESUME" : "PAUSE")) frozen = !frozen;
    ui_end();
}

static void draw_pick(void) {
    cls(CLR_DARKER_BLUE);
    print_scaled("ORACLE", (SCREEN_W - text_width("ORACLE") * 3) / 2, 22, CLR_LIGHT_YELLOW, 3);
    font(FONT_SMALL);
    print_centered("an insider's prediction market", SCREEN_W / 2, 50, CLR_LIGHT_GREY);
    print_centered("you hear things early, from people who are sometimes wrong.",
                   SCREEN_W / 2, 60, CLR_DARK_GREY);
    font(FONT_NORMAL);

    // The cards are tappable: on a phone there is no ENTER key, so a
    // keyboard-only title screen is an unplayable cart (mobile-lint caught it).
    // ui_button with an empty label = a pure hit rect; the card art draws over.
    int picked = -1;
    ui_begin();
    for (int i = 0; i < 2; i++)
        if (ui_button(22 + i * 148, 80, 128, 74, "")) picked = i;

    for (int i = 0; i < 2; i++) {
        int bx = 22 + i * 148, by = 80, bw = 128, bh = 74;
        int on = (mode == i);
        rrectfill(bx, by, bw, bh, 4, on ? CLR_BLUE_GREEN : CLR_BROWNISH_BLACK);
        if (on) rect(bx, by, bw, bh, CLR_YELLOW);
        print_centered(i == 0 ? "TRADER" : "MAKER", bx + bw / 2, by + 8,
                       on ? CLR_WHITE : CLR_LIGHT_GREY);
        font(FONT_SMALL);
        if (i == 0) {
            print_centered("private sources whisper",bx + bw / 2, by + 26, CLR_LIGHT_GREY);
            print_centered("before the crowd knows.",bx + bw / 2, by + 35, CLR_LIGHT_GREY);
            print_centered("the gap between them",   bx + bw / 2, by + 48, CLR_DARK_GREY);
            print_centered("is the only money there", bx + bw / 2, by + 57, CLR_DARK_GREY);
        } else {
            print_centered("set your liquidity and", bx + bw / 2, by + 26, CLR_LIGHT_GREY);
            print_centered("take the other side.",   bx + bw / 2, by + 35, CLR_LIGHT_GREY);
            print_centered("earn the noise,",        bx + bw / 2, by + 48, CLR_DARK_GREY);
            print_centered("bleed to the informed",  bx + bw / 2, by + 57, CLR_DARK_GREY);
        }
        font(FONT_NORMAL);
    }

    font(FONT_SMALL);
    print_centered("tap a mode, or  < >  and  ENTER", SCREEN_W / 2, 166, CLR_LIGHT_GREY);
    if (best > 0) print_centered(str("best run  $%d", best), SCREEN_W / 2, 180, CLR_LIGHT_YELLOW);
    font(FONT_NORMAL);
    ui_end();

    if (picked >= 0) { mode = picked; new_run(); }      // after ui_end(): the frame stays intact
}

static void draw_liq(void) {
    cls(CLR_DARKER_BLUE);
    draw_hud();
    font(FONT_SMALL);
    print_centered(QUESTION[qidx], SCREEN_W / 2, 24, CLR_LIGHT_YELLOW);
    print_centered("how deep do you want to make this book?", SCREEN_W / 2, 40, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    static const char *LN[3] = { "THIN", "MID", "DEEP" };
    static const float LV[3] = { B_THIN, B_MID, B_DEEP };

    int picked = -1;
    ui_begin();
    for (int i = 0; i < 3; i++)
        if (ui_button(18 + i * 96, 62, 84, 72, "")) picked = i;

    for (int i = 0; i < 3; i++) {
        int bx = 18 + i * 96, by = 62, bw = 84, bh = 72;
        int on = (liqpick == i);
        rrectfill(bx, by, bw, bh, 4, on ? CLR_BLUE_GREEN : CLR_BROWNISH_BLACK);
        if (on) rect(bx, by, bw, bh, CLR_YELLOW);
        print_centered(LN[i], bx + bw / 2, by + 8, on ? CLR_WHITE : CLR_LIGHT_GREY);
        font(FONT_SMALL);
        print_centered(str("b = %d", (int)LV[i]), bx + bw / 2, by + 26, CLR_LIGHT_GREY);
        print_centered("max loss", bx + bw / 2, by + 42, CLR_DARK_GREY);
        print_centered(str("-$%d", (int)(LV[i] * 0.6931f)), bx + bw / 2, by + 52, CLR_ORANGE);
        font(FONT_NORMAL);
    }
    font(FONT_SMALL);
    print_centered("deeper book = more flow to earn, more room to lose",
                   SCREEN_W / 2, 146, CLR_DARK_GREY);
    print_centered("tap a depth, or  < >  and  ENTER", SCREEN_W / 2, 164, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
    ui_end();

    if (picked >= 0) { liqpick = picked; open_book(); }
}

static void draw_resolve(void) {
    draw_trade();
    EndCard c = endcard(end_t, 240, 104, 40, CLR_DARKER_PURPLE, CLR_LIGHT_YELLOW, CLR_INDIGO);
    if (!c.settled) return;

    print_centered(truth ? "IT HAPPENED" : "IT DID NOT HAPPEN", SCREEN_W / 2, c.y + 10,
                   truth ? CLR_LIME_GREEN : CLR_DARK_PEACH);
    font(FONT_SMALL);
    print_centered(QUESTION[qidx], SCREEN_W / 2, c.y + 24, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    float closed = nhist > 0 ? hist[nhist - 1] : 0.5f;
    font(FONT_SMALL);
    print_centered(str("the market closed at %d%% YES", (int)(closed * 100.0f + 0.5f)),
                   SCREEN_W / 2, c.y + 42, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    int tally = (int)lerp(0, (float)last_pnl, ease_out(clamp((end_t - 0.55f) * 1.6f, 0, 1)));
    print_centered(str("%s$%d", last_pnl >= 0 ? "+" : "-", tally < 0 ? -tally : tally),
                   SCREEN_W / 2, c.y + 58, last_pnl >= 0 ? CLR_LIME_GREEN : CLR_RED);

    font(FONT_SMALL);
    if (blink(20)) print_centered(marketno >= NMARKET ? "ENTER for the reckoning" : "ENTER for the next market",
                                  SCREEN_W / 2, c.y + 86, CLR_YELLOW);
    font(FONT_NORMAL);
}

static void again_button(void) {
    ui_begin();
    int go = ui_button(110, 170, 100, 20, "PLAY AGAIN  R");
    ui_end();
    if (go) st = ST_PICK;
}

// The calibration screen — the point of the whole cart. Bucket every trade by
// the price you paid, and show how often that side actually came in. A
// well-calibrated forecaster's dots sit on the diagonal.
static void draw_summary(void) {
    cls(CLR_DARKER_BLUE);
    print_scaled("THE RECKONING", (SCREEN_W - text_width("THE RECKONING") * 2) / 2, 8,
                 CLR_LIGHT_YELLOW, 2);

    font(FONT_SMALL);
    print_centered(str("finished with $%d   (started $%d)", bank, START_BANK), SCREEN_W / 2, 28,
                   bank >= START_BANK ? CLR_LIME_GREEN : CLR_RED);
    if (bank >= best && bank > 0) print_centered("new best run", SCREEN_W / 2, 38, CLR_YELLOW);
    font(FONT_NORMAL);

    if (ncal == 0) {
        font(FONT_SMALL);
        print_centered("you never took a position, so there is nothing to score.",
                       SCREEN_W / 2, 96, CLR_DARK_GREY);
        font(FONT_NORMAL);
        again_button();
        return;
    }

    // Brier score: mean squared error of your implied probabilities. Lower is
    // better; 0.25 is what you'd score by always saying 50%.
    float brier = 0.0f;
    for (int i = 0; i < ncal; i++) {
        float d = cal_p[i] - (float)cal_win[i];
        brier += d * d;
    }
    brier /= (float)ncal;

    // Five buckets of paid-price vs realised frequency.
    int cnt[5] = {0,0,0,0,0}, won[5] = {0,0,0,0,0};
    for (int i = 0; i < ncal; i++) {
        int b = (int)(cal_p[i] * 5.0f); if (b > 4) b = 4; if (b < 0) b = 0;
        cnt[b]++; won[b] += cal_win[i];
    }

    int gx = 60, gy = 52, gw = 110, gh = 88;
    rectfill(gx, gy, gw, gh, CLR_BROWNISH_BLACK);
    rect(gx, gy, gw, gh, CLR_DARKER_GREY);
    line(gx, gy + gh - 1, gx + gw - 1, gy, CLR_DARK_GREY);      // the perfect-calibration diagonal

    for (int b = 0; b < 5; b++) {
        if (!cnt[b]) continue;
        float px = (b + 0.5f) / 5.0f;
        float py = (float)won[b] / (float)cnt[b];
        int   cx = gx + (int)(px * (gw - 1));
        int   cy = gy + gh - 1 - (int)(py * (gh - 1));
        int   r  = cnt[b] > 6 ? 3 : cnt[b] > 2 ? 2 : 1;
        circfill(cx, cy, r, CLR_LIME_GREEN);
    }

    font(FONT_SMALL);
    print("100%", gx - 22, gy - 2, CLR_DARK_GREY);
    print("0%",   gx - 14, gy + gh - 6, CLR_DARK_GREY);
    print_centered("what your read said", gx + gw / 2, gy + gh + 4, CLR_DARK_GREY);

    int tx = gx + gw + 14;
    print("CALIBRATION", tx, gy, CLR_LIGHT_YELLOW);
    print(str("%d trades", ncal), tx, gy + 12, CLR_LIGHT_GREY);
    print(str("brier %d.%02d", (int)brier, (int)(brier * 100) % 100), tx, gy + 22,
          brier < 0.25f ? CLR_LIME_GREEN : CLR_ORANGE);
    print(brier < 0.18f ? "you knew things"
        : brier < 0.25f ? "better than a coin"
        : "you trusted the drunk", tx, gy + 34, CLR_LIGHT_GREY);

    for (int b = 0; b < 5; b++) {
        if (!cnt[b]) continue;
        print(str("%d-%d%%  %d/%d", b * 20, b * 20 + 20, won[b], cnt[b]),
              tx, gy + 50 + b * 8, CLR_LIGHT_GREY);
    }

    print_centered("dots on the line = you knew what you knew", SCREEN_W / 2, 158, CLR_DARK_GREY);
    font(FONT_NORMAL);
    again_button();
}

void draw(void) {
    // watch() FIRST — every state early-returns below, so instrumentation
    // parked at the bottom only ever fires on the last one. (It did.)
#ifdef DE_TRACE
    watch("st", "%d", st);
    watch("mkt", "%d", marketno);
    watch("price", "%.3f", lmsr_price(qy, qn, lb));
    watch("belief", "%.3f", world_belief());
    watch("read", "%.3f", player_read());
    watch("ntip", "%d", ntip_seen);
    watch("truth", "%d", truth);
    watch("bank", "%d", bank);
    watch("pyes", "%d", (int)p_yes);
    watch("pno", "%d", (int)p_no);
    watch("ncal", "%d", ncal);
#endif

    if (st == ST_PICK)    { draw_pick();    return; }
    if (st == ST_LIQ)     { draw_liq();     return; }
    if (st == ST_TRADE)   { draw_trade();   return; }
    if (st == ST_RESOLVE) { draw_resolve(); return; }
    draw_summary();
}

// ---- spec() — the LMSR's invariants, pinned ------------------------------
#ifdef DE_SPEC
#include "spec.h"

void spec(void) {
    // ── the cost function is the whole market, so pin it hard ──
    expect(spec_close(lmsr_price(0, 0, B_MID), 0.5f, 0.001f),
           "an empty book prices a coin flip at 50%");
    expect(spec_close(lmsr_price(100, 0, B_MID) + lmsr_price(0, 100, B_MID), 1.0f, 0.001f),
           "YES and NO prices always sum to 1");

    float prev = 0.0f;
    for (int i = 0; i <= 10; i++) {
        float p = lmsr_price((float)i * 40.0f, 0, B_MID);
        expect(p > prev, "price is strictly increasing in YES shares outstanding");
        prev = p;
    }
    expect(lmsr_price(2000, 0, B_MID) < 1.0f, "price never actually reaches certainty");
    expect(lmsr_price(0, 2000, B_MID) > 0.0f, "...nor zero");

    // Path independence: the defining property of a scoring-rule maker. Ten
    // small bites must cost exactly what one big bite costs, or the market is
    // arbitrageable by chopping up your order.
    float one = lmsr_trade_cost(0, 0, B_MID, 0, 100.0f);
    float many = 0.0f, sy = 0.0f;
    for (int i = 0; i < 10; i++) { many += lmsr_trade_cost(sy, 0, B_MID, 0, 10.0f); sy += 10.0f; }
    expect(spec_close(one, many, 0.01f), "cost is path-independent: 10x10 shares == 1x100 shares");

    // Round-tripping must be free — buy then immediately sell back and you are
    // exactly where you started. (Real venues break this with a fee; we don't.)
    float rt = lmsr_trade_cost(0, 0, B_MID, 0, 50.0f) + lmsr_trade_cost(50.0f, 0, B_MID, 0, -50.0f);
    expect(spec_close(rt, 0.0f, 0.01f), "buy then sell the same size round-trips to zero");

    // The maker's worst case is b*ln(2) — the reason a maker can quote at all.
    // Drive the book to one extreme and check what it could owe.
    float qyy = 5000.0f;
    float collected = lmsr_cost(qyy, 0, B_MID) - lmsr_cost(0, 0, B_MID);
    float worst = qyy - collected;                      // pays out qyy, keeps `collected`
    expect(worst <= B_MID * 0.6932f + 0.5f, "maker loss is bounded by b*ln(2), even at the extreme");
    expect(worst > 0.0f, "...and the bound is actually approached, so it isn't vacuous");

    // Deeper book must move less on the same order — the entire meaning of b.
    float thin = lmsr_price(50, 0, B_THIN), deep = lmsr_price(50, 0, B_DEEP);
    expect(thin > deep, "the same order moves a THIN book further than a DEEP one");

    // ── the world model: evidence must accumulate toward the truth ──
    ev_y = 0; ev_n = 0;
    expect(spec_close(world_belief(), 0.5f, 0.001f), "with no evidence the crowd is at 50%");
    ev_y = 5; ev_n = 0;
    expect(world_belief() > 0.9f, "five confirming signals make the crowd confident");
    ev_y = 0; ev_n = 5;
    expect(world_belief() < 0.1f, "...and five denials do the same in reverse");
    ev_y = 3; ev_n = 3;
    expect(spec_close(world_belief(), 0.5f, 0.001f), "balanced evidence returns the crowd to 50%");
    ev_y = 0; ev_n = 0;

    // ── the loop: informed flow must drag the price toward the hidden truth ──
    // The mechanism is stochastic, so the honest claim is not "one run lands
    // near the truth" — a random walk does that often enough by luck. It is
    // that informed flow lands MEASURABLY closer than noise does. So we run
    // both arms and compare, which is a control the informed arm cannot pass
    // on its own.
    int TRIALS = 16;
    float err_informed = 0.0f, err_noise = 0.0f;
    int right = 0;

    for (int trial = 0; trial < TRIALS; trial++) {
        mode = MODE_TRADER; liqpick = 1;
        new_market();
        truth = (trial & 1);                            // force both directions
        for (int i = 0; i < TICKS; i++) step_tick();
        float p = lmsr_price(qy, qn, lb);
        float d = p - (float)truth; if (d < 0) d = -d;
        err_informed += d;
        if (truth ? (p > 0.5f) : (p < 0.5f)) right++;
    }
    err_informed /= (float)TRIALS;

    for (int trial = 0; trial < TRIALS; trial++) {
        mode = MODE_TRADER; liqpick = 1;
        new_market();
        truth = (trial & 1);
        for (int i = 0; i < TICKS; i++)                 // same volume, NO informed traders
            for (int k = 0; k < 3; k++) fill(rnd(2), 4.0f + frnd() * 30.0f);
        float p = lmsr_price(qy, qn, lb);
        float d = p - (float)truth; if (d < 0) d = -d;
        err_noise += d;
    }
    err_noise /= (float)TRIALS;

    expect(err_informed < err_noise * 0.75f,
           "informed flow lands closer to the truth than the same volume of noise");
    expect(err_informed < 0.35f, "...and close in absolute terms, so it really does converge");
    expect(err_noise > 0.35f, "...while noise alone stays uninformative (the control)");
    expect(right >= TRIALS * 3 / 4, "informed flow picks the right side of 50% most runs");

    // ── THE FUN GATE: the cart must contain a decision ──────────────────
    // v1 shipped without this and was boring, for a reason no other assertion
    // here could have caught: the price is a CORRECT Bayesian posterior, a
    // correct posterior is calibrated, and betting into a calibrated price has
    // an expected value of exactly zero however cleverly you do it. Every
    // strategy scored the same, so there was nothing to decide.
    //
    // Private tips are the fix, and this is the gate on it: a player who reads
    // their sources must beat one who just follows the line. If this ever goes
    // red the cart is a spoiler with buttons again, whatever else still passes.
    float pnl_tips = 0.0f, pnl_follow = 0.0f;
    int   n_tips = 0, n_follow = 0;
    for (int trial = 0; trial < 40; trial++) {
        mode = MODE_TRADER; liqpick = 1;
        new_market();
        truth = (trial & 1);                            // force both directions...
        roll_tips();                                    // ...and RE-ROLL: new_market()
        for (int i = 0; i < TICKS; i++) {               // rolled them against the old truth
            step_tick();
            float p    = lmsr_price(qy, qn, lb);
            float read = player_read();

            float gap = read - p; if (gap < 0) gap = -gap;
            if (gap > 0.10f) {                          // trade only a real edge
                n_tips++;
                if (read > p) pnl_tips += (truth == 1) ? (1.0f - p) : -p;
                else          pnl_tips += (truth == 0) ? p          : -(1.0f - p);
            }
            if (p > 0.55f || p < 0.45f) {               // the boring strategy
                n_follow++;
                if (p > 0.5f) pnl_follow += (truth == 1) ? (1.0f - p) : -p;
                else          pnl_follow += (truth == 0) ? p          : -(1.0f - p);
            }
        }
    }
    float ev_tips   = n_tips   ? pnl_tips   / (float)n_tips   : 0.0f;
    float ev_follow = n_follow ? pnl_follow / (float)n_follow : 0.0f;

    expect(n_tips > 40, "sources produce tradeable gaps often enough to play on");
    expect(ev_follow < 0.06f && ev_follow > -0.06f,
           "following the price alone earns ~nothing — the market is calibrated");
    expect(ev_tips > 0.08f, "reading your sources earns a real edge");
    expect(ev_tips > ev_follow + 0.08f,
           "THE FUN GATE: using your sources beats following the line");

    // A source's weight must scale with its reliability, or the whole
    // who-do-you-trust judgement is theatre.
    qy = qn = 0.0f; ntip_seen = 1; tip_src[0] = 0; tip_dir[0] = 1;   // harbourmaster, YES
    float strong = player_read();
    tip_src[0] = NSOURCE - 1;                                        // tavern drunk, YES
    float weak = player_read();
    expect(strong > weak, "a reliable source moves your read further than a dubious one");
    expect(weak > 0.5f && weak < 0.60f, "...and the drunk barely moves it at all");
    tip_dir[0] = 0; tip_src[0] = 0;
    expect(player_read() < 0.5f, "a source pointing NO moves your read below the price");
    ntip_seen = 0;
    expect(spec_close(player_read(), lmsr_price(qy, qn, lb), 0.001f),
           "with no sources your read IS the market price (no free edge)");

    // ── the player's ledger must balance ──
    mode = MODE_TRADER; liqpick = 1; new_market();
    st = ST_TRADE;
    int b0 = bank = 1000;
    lotsel = 1;
    buy(0);
    expect(bank < b0, "buying YES costs cash");
    expect(p_yes > 0.0f, "...and leaves you holding shares");
    float spent = p_spent;
    sell_all();
    expect(spec_close(p_spent, 0.0f, 2.0f), "selling the whole position closes the ledger out");
    expect(bank >= b0 - 2 && bank <= b0 + 1, "a round-trip returns your cash (within rounding)");
    expect(spent > 0.0f, "...and the trip was not a no-op");
}
#endif
