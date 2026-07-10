/* de:meta
{
  "slug": "slots",
  "title": "one-armed bandit",
  "status": "active",
  "created": "2026-05-30",
  "kind": [
    "game"
  ],
  "teaches": [
    "save-load-persistence"
  ],
  "genre": "tabletop",
  "description": "A slot machine, and a lesson in why you never win. Three reels land on random symbols; match three to get paid. It feels fair, but the RTP (return-to-player) readout drifts below 100% over many spins — that gap is the house edge, and the maths guarantees the machine wins long-term. Credits persist between runs via save/load. Z spins, X takes a loan when broke.",
  "todo": [
    "Show the spinning reels, and show 3 rows (top and bottom slightly cut off) for a real spin effect.",
    "Better sfx — use FM synth and bells."
  ]
}
de:meta */
#include "studio.h"

// ONE-ARMED BANDIT — a slot machine, and a lesson in why you never win.
//
// Three reels, each lands on a random symbol. Match three and you get paid.
// It feels fair... but watch the RTP readout (Return To Player): over many
// spins, the total paid out stays UNDER what you put in. That gap is the house
// edge. Probability isn't your friend here — the maths guarantees the machine
// wins in the long run. Your credits are saved between runs (so is your regret).
//
//   Z   spin (costs 1 credit)        X   take a loan when you're broke

#define NSYM 6
static const char *SYM[NSYM]  = { "7", "BAR", "BELL", "CHRY", "LMON", "GEM" };
static const int   SCOL[NSYM] = { CLR_RED, CLR_LIGHT_GREY, CLR_YELLOW, CLR_RED, CLR_YELLOW, CLR_BLUE };
static const int   PAY3[NSYM] = { 20, 10, 8, 6, 4, 12 };   // three-of-a-kind payout
#define CHERRY 3

static int reel[3], disp[3], spinf[3];
static int credits, spins, won, lastpay = -1;

static bool any_spin(void) { return spinf[0] || spinf[1] || spinf[2]; }

static void persist(void) { save(0, credits); save(1, spins); save(2, won); }

void init(void) {
    credits = load(0); if (credits <= 0) credits = 10;
    spins = load(1); won = load(2);
    reel[0] = disp[0] = reel[1] = disp[1] = reel[2] = disp[2] = 0;   // three 7s lined up
}

static void evaluate(void) {
    int p = 0;
    if (reel[0] == reel[1] && reel[1] == reel[2]) p = PAY3[reel[0]];
    else {
        int ch = 0; for (int i = 0; i < 3; i++) if (reel[i] == CHERRY) ch++;
        if (ch == 2) p = 2;
    }
    credits += p; won += p; lastpay = p;
    if (p > 0) { hit(72, INSTR_SQUARE, 4, 90); hit(84, INSTR_SQUARE, 4, 120); }
    persist();
}

void update(void) {
    if (any_spin()) {
        for (int i = 0; i < 3; i++) if (spinf[i] > 0) {
            spinf[i]--;
            if (spinf[i] % 2 == 0) disp[i] = rnd(NSYM);
            if (spinf[i] == 0) { reel[i] = disp[i]; hit(60, INSTR_SQUARE, 2, 30); }
        }
        if (!any_spin()) evaluate();
        return;
    }
    if (btnp(0, BTN_A) && credits > 0) {
        credits--; spins++; lastpay = -1;
        for (int i = 0; i < 3; i++) spinf[i] = 18 + i * 10;
        persist();
    }
    if (btnp(0, BTN_B) && credits <= 0) { credits = 10; persist(); }
}

static void print_cx(const char *s, int cx, int y, int col) {
    int w = 0; for (const char *p = s; *p; p++) w += 8;
    print(s, cx - w / 2, y, col);
}

static void reel_window(int i, int cx) {
    int y = 70, w = 60, h = 50;
    rectfill(cx - w / 2, y, w, h, CLR_BLACK);
    rect(cx - w / 2, y, w, h, CLR_LIGHT_GREY);
    int s = any_spin() ? disp[i] : reel[i];
    circfill(cx, y + 20, 13, SCOL[s]);
    print_cx(SYM[s], cx, y + 16, CLR_BLACK);
}

void draw(void) {
    cls(CLR_DARK_PURPLE);
    print_centered("ONE-ARMED BANDIT", SCREEN_W/2, 8, CLR_LIGHT_YELLOW);

    reel_window(0, 96);
    reel_window(1, 160);
    reel_window(2, 224);

    print(str("CREDITS %d", credits), 8, 28, CLR_WHITE);
    int rtp = spins > 0 ? won * 100 / spins : 100;
    print_right(str("RTP %d%%", rtp), SCREEN_W - 8, 28, rtp < 100 ? CLR_RED : CLR_GREEN);
    print_centered(str("spins %d", spins), SCREEN_W/2, 28, CLR_DARK_GREY);

    const char *msg = any_spin() ? "..." :
                      credits <= 0 ? "BANKRUPT!  press X for a loan" :
                      lastpay > 0  ? str("WINNER!  +%d credits", lastpay) :
                      lastpay == 0 ? "no luck — spin again" : "press Z to pull the lever";
    print_centered(msg, SCREEN_W/2, 134, lastpay > 0 ? CLR_YELLOW : CLR_LIGHT_GREY);

    print_centered("the house always wins", SCREEN_W/2, SCREEN_H - 11, CLR_DARK_GREY);
}
