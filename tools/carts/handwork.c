/* de:meta
{
  "title": "Handwork",
  "status": "active",
  "kind": [
    "toy",
    "probe"
  ],
  "teaches": [
    "state-machine"
  ],
  "lineage": "A probe cart for sloop's production subsystem (docs/design/sloop-handwork.md) — isolates the opportunity-cost loop (hand bench vs. delegate machine vs. door buyer) from the CDDA-inspired The Cut, without that game's follower/faction layer.",
  "genre": "simulation",
  "description": "The EMBODIED half of sloop's production subsystem (docs/design/sloop-handwork.md) — the CDDA body-in-the-world handling loop The Cut skipped. You walk a workroom, harvest plants into a carried BASKET, then turn raw STALKS into trimmed FLOWERS two ways: by HAND at the BENCH you stand there LOCKED and TIME each cut on a bouncing meter — a clean cut makes a FINE flower (premium-grade); or load the MACHINE and walk away while it trims on its own at a fixed, always-COARSE quality. A machine is your first DELEGATE (one pure transform, run by your timed hands or the machine's fixed stamp; followers later slot into the machine's seat). The hook is the OPPORTUNITY COST: BUYERS arrive at the DOOR and WAIT — a PREMIUM buyer wants a few FINE flowers and pays by quality, a BULK buyer wants a cheap pile of anything — and they leave if you don't serve them in time. So standing locked at the bench COSTS you sales; the machine earns its keep by trimming the bulk while your hands chase the premium order. One body can't be in two places — that live choice is the game. Skill rises as you hand-trim, making FINE easier. WASD/arrows walk; Z harvest a ripe plant / pick up the basket / cut at the bench / load the machine / serve the buyer at the door; X drop the basket / collect the machine's output / leave the bench."
}
de:meta */
#include "studio.h"
#include <string.h>

// ── HANDWORK — the embodied production loop (sloop, v1) ─────────────
// The CDDA body-in-the-world half of sloop's production subsystem. You
// walk a workroom, harvest plants into a basket, then turn raw STALKS
// into trimmed FLOWERS two ways — and the contrast IS the experiment:
//
//   • by HAND at the BENCH (embodied / active): you stand there, locked,
//     and TIME each cut on a bouncing meter. Good timing = a focus bonus
//     = higher quality. Costs your attention (you can't be anywhere else).
//   • by MACHINE at the TRIMMER (light / passive): load stalks, walk away.
//     It trims on its own at fixed, lower quality. Costs no attention.
//
// A machine is your first DELEGATE (docs/design/sloop-handwork.md): one
// pure transform `flower_q(skill, focus)`, run by you (focus from timing)
// or by the machine (fixed, always COARSE). When followers land they slot
// into the machine's seat.
//
// THE OPPORTUNITY COST (why the machine is a relief, not just an upgrade):
// BUYERS arrive at the DOOR and WAIT — a PREMIUM buyer wants a few FINE
// flowers (only your skilled, well-timed HANDS make those) and pays well;
// a BULK buyer wants a pile of anything cheap (the MACHINE's lane). They
// leave if you don't serve them in time. So standing locked at the bench
// COSTS you sales at the door — one body can't be in two places. The
// machine earns its keep by trimming the bulk while your hands chase the
// premium order. The live choice each minute IS the game.
//
//   WASD / arrows  walk
//   Z   harvest plant · pick up basket · cut at bench · load trimmer · serve buyer
//   X   drop basket · collect trimmer output · leave the bench

#define TS 18
#define OX 16
#define OY 28
#define GW 16
#define GH 8

// carry caps
#define CAP_HAND   3     // stalks in your bare hands
#define CAP_BASKET 12    // stalks in the basket
#define CAP_FLOWER 10    // trimmed flowers you can carry

// machine
#define HOPPER_CAP 8
#define OUT_CAP    16
#define PROC_T     1.1f  // seconds per stalk the trimmer chews
#define MACH_Q     45    // the machine's FIXED quality — always coarse, never premium

// flowers / market
#define FINE_Q       65   // q >= this = a FINE flower (premium-grade); below = COARSE (bulk)
#define PREMIUM_RATE 14   // $ per fine flower (× quality) to a premium buyer
#define BULK_RATE     4   // $ per flower to a bulk buyer
#define BUYER_GAP   6.0f  // seconds of quiet between buyers
#define PAT_PREMIUM 9.0f  // how long a premium buyer waits
#define PAT_BULK   14.0f  // how long a bulk buyer waits

// plant regrow
#define REGROW_T 5.0f

// fixtures
enum { F_NONE, F_PLANT, F_BENCH, F_TRIMMER, F_DOOR, F_BASKETSPOT };
// buyer kinds
enum { BUY_NONE, BUY_PREMIUM, BUY_BULK };

static unsigned char fix[GH][GW];     // fixture type per cell
static float regrow[GH][GW];          // plant: 0 = ripe, >0 = seconds until ripe

// ── player ──
static float px, py;
static int   facex = 0, facey = 1;

// ── inventory ──
static int   hand_stalks;             // stalks in hand (cap CAP_HAND)
static int   basket_stalks;           // stalks in the basket (cap CAP_BASKET)
static bool  basket_held;             // carrying the basket?
static int   basket_gx, basket_gy;    // basket cell when on the ground
// carried flowers split into the two lanes (hands make FINE, machine makes COARSE)
static int   fine_n;    static long fine_qsum;
static int   coarse_n;  static long coarse_qsum;

// ── the trimmer (machine) ──
static int   hopper;                  // stalks waiting
static float proc_t;                  // countdown on the current stalk
static int   out_n;    static long out_qsum;  // finished flowers in the output tray

// ── the market (buyer at the door) ──
static int   buyer;                   // BUY_NONE / BUY_PREMIUM / BUY_BULK
static int   want, want0;             // units remaining / original order size
static float patience, patience0;     // seconds left / full
static float buyer_gap;               // seconds until the next buyer (when none)
static int   spawn_count, missed, cash;

// ── skill ──
static int   skill, xp;

// ── trim minigame state ──
static bool  trimming;                // locked at the bench?
static float meter;                   // 0..1 bouncing marker
static int   meter_dir = 1;

// ── time / juice ──
static int   day = 1;
static float clockT;                  // 0..1 across a session-day (felt clock)
static float popT; static char popMsg[28]; static int popColor;
static bool  inited = false;

static void say(const char *m, int c) {
    strncpy(popMsg, m, sizeof(popMsg) - 1);
    popMsg[sizeof(popMsg) - 1] = 0;
    popColor = c; popT = 1.3f;
}

static void reset_game(void) {
    memset(fix, 0, sizeof(fix));
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) regrow[y][x] = 0;

    // plants down the left column
    for (int y = 1; y < GH - 1; y++) fix[y][1] = F_PLANT;
    // stations down the right side
    fix[1][GW - 2] = F_BENCH;
    fix[3][GW - 2] = F_TRIMMER;
    fix[5][GW - 2] = F_DOOR;
    // basket starts off to the side (not under the player)
    fix[GH - 2][GW / 2 - 2] = F_BASKETSPOT;
    basket_gx = GW / 2 - 2; basket_gy = GH - 2; basket_held = false;

    px = OX + (GW / 2) * TS + TS / 2;
    py = OY + (GH / 2) * TS + TS / 2;
    facex = 0; facey = 1;

    hand_stalks = basket_stalks = 0;
    fine_n = 0; fine_qsum = 0; coarse_n = 0; coarse_qsum = 0;
    hopper = 0; proc_t = 0; out_n = 0; out_qsum = 0;
    buyer = BUY_NONE; want = want0 = 0; patience = patience0 = 0;
    buyer_gap = 3.0f; spawn_count = 0; missed = 0; cash = 0;
    skill = 0; xp = 0;
    trimming = false; meter = 0; meter_dir = 1;
    day = 1; clockT = 0; popT = 0;
    inited = true;
}

static int ptx(void) { return (int)((px - OX) / TS); }
static int pty(void) { return (int)((py - OY) / TS); }
static void faced_cell(int *fx, int *fy) { *fx = ptx() + facex; *fy = pty() + facey; }

static bool blocked_cell(int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= GW || cy >= GH) return true;
    int t = fix[cy][cx];
    return t == F_BENCH || t == F_TRIMMER || t == F_DOOR || t == F_PLANT;  // act from beside
}

// the HAND transform — your skill + a timing focus bonus (0..30). The machine
// doesn't use this: it stamps a fixed MACH_Q, so only hands ever reach FINE.
static int flower_q(int skl, int focus) {
    int base = 38 + skl * 8;          // skilled hands start cleaner (CDDA maker-skill)
    return (int)clamp(base + focus, 5, 99);
}

static int  carried_flowers(void) { return fine_n + coarse_n; }
static void add_flower(int q) {                 // file a finished flower into its lane
    if (carried_flowers() >= CAP_FLOWER) return;
    if (q >= FINE_Q) { fine_n++;   fine_qsum   += q; }
    else             { coarse_n++; coarse_qsum += q; }
}

// where do carried stalks live right now?
static int  carried_stalks(void) { return basket_held ? basket_stalks : hand_stalks; }
static int  stalk_cap(void)      { return basket_held ? CAP_BASKET : CAP_HAND; }
static void add_carried(int n)   { if (basket_held) basket_stalks += n; else hand_stalks += n; }
static int  take_carried(int n)  {                                  // remove up to n, return taken
    int have = carried_stalks(), got = have < n ? have : n;
    if (basket_held) basket_stalks -= got; else hand_stalks -= got;
    return got;
}

static void gain_xp(int n) {
    xp += n;
    int need = (skill + 1) * 6;        // 6,12,18… cuts per level
    if (xp >= need && skill < 6) { skill++; say(str("skill up! %d", skill), CLR_LIGHT_YELLOW);
        strum(67, CHORD_MAJ7, INSTR_TRI, 3, 50); }
}

static void spawn_buyer(void) {
    spawn_count++;
    if (spawn_count % 2 == 0) {        // premium: rarer-feeling, small, picky, lucrative
        buyer = BUY_PREMIUM; want = 2 + spawn_count % 3; patience = patience0 = PAT_PREMIUM;
    } else {                            // bulk: big pile of anything cheap
        buyer = BUY_BULK; want = 5 + spawn_count % 4; patience = patience0 = PAT_BULK;
    }
    want0 = want;
    note(76, INSTR_SQUARE, 1); note(72, INSTR_SQUARE, 1);   // ding-dong at the door
    say(buyer == BUY_PREMIUM ? "a premium buyer!" : "a bulk buyer", CLR_LIGHT_YELLOW);
}

void init(void) { bpm(100); reset_game(); }

void update(void) {
    if (!inited) reset_game();
    float d = dt();
    if (popT > 0) popT = clamp(popT - d, 0, 1.3f);
    clockT += d * 0.010f; if (clockT >= 1.0f) { clockT = 0; day++; say("a new day", CLR_LIGHT_YELLOW); }

#ifdef DE_TRACE
    watch("stalks",  "%d", carried_stalks());
    watch("hopper",  "%d", hopper);
    watch("out",     "%d", out_n);
    watch("fine",    "%d", fine_n);
    watch("coarse",  "%d", coarse_n);
    watch("buyer",   "%d", buyer);
    watch("want",    "%d", want);
    watch("cash",    "%d", cash);
    watch("skill",   "%d", skill);
#endif

    // ── the market runs on its own clock — a buyer waits, then leaves ──
    if (buyer == BUY_NONE) {
        buyer_gap -= d;
        if (buyer_gap <= 0) spawn_buyer();
    } else {
        patience = clamp(patience - d, 0, patience0);
        if (patience <= 0) { missed++; say("buyer left!", CLR_RED); note(40, INSTR_SQUARE, 2);
            buyer = BUY_NONE; buyer_gap = BUYER_GAP; }
    }

    // ── plants regrow (a small passive process) ──
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        if (fix[y][x] == F_PLANT && regrow[y][x] > 0) regrow[y][x] = clamp(regrow[y][x] - d, 0, REGROW_T);

    // ── the trimmer runs on its own — the automaton ──
    if (hopper > 0 && out_n < OUT_CAP) {
        proc_t -= d;
        if (proc_t <= 0) {
            hopper--; proc_t = PROC_T;
            out_n++; out_qsum += MACH_Q;         // the machine always stamps a fixed, coarse q
            note(70, INSTR_SINE, 1);
        }
    }

    int fx, fy; faced_cell(&fx, &fy);
    int ft = (fx >= 0 && fy >= 0 && fx < GW && fy < GH) ? fix[fy][fx] : F_NONE;

    // ── trimming at the bench: locked in place, time each cut ──
    if (trimming) {
        meter += meter_dir * d * 1.7f;
        if (meter > 1) { meter = 1; meter_dir = -1; }
        if (meter < 0) { meter = 0; meter_dir = 1; }

        if (btnp(0, BTN_A)) {                    // commit a cut
            if (carried_stalks() > 0 && carried_flowers() < CAP_FLOWER) {
                take_carried(1);
                float off = meter - 0.5f; if (off < 0) off = -off;   // 0 = perfect centre
                int focus = (int)((0.5f - off) / 0.5f * 30);          // 0..30
                if (focus < 0) focus = 0;
                int q = flower_q(skill, focus);
                add_flower(q);
                gain_xp(1);
                if (q >= FINE_Q)      { hit(84, INSTR_TRI, 2, 30); say(str("FINE cut!  q%d", q), CLR_LIME_GREEN); }
                else if (focus >= 12) { note(76, INSTR_TRI, 2);    say(str("coarse  q%d", q), CLR_GREEN); }
                else                  { note(60, INSTR_SQUARE, 1); say(str("rough  q%d", q), CLR_YELLOW); }
                // re-seed the bounce so the next cut isn't free
                meter = (off < 0.25f) ? 0.0f : 0.85f; meter_dir = (meter < 0.5f) ? 1 : -1;
            } else if (carried_flowers() >= CAP_FLOWER) { say("hands full of flowers", CLR_ORANGE); note(45, INSTR_SQUARE, 1); }
        }
        if (btnp(0, BTN_B) || carried_stalks() == 0) {
            trimming = false; say("left the bench", CLR_LIGHT_GREY);
        }
        return;   // movement disabled while trimming
    }

    // ── movement ──
    float spd = 58.0f, mvx = 0, mvy = 0;
    if (btn(0, BTN_LEFT))  mvx -= 1;
    if (btn(0, BTN_RIGHT)) mvx += 1;
    if (btn(0, BTN_UP))    mvy -= 1;
    if (btn(0, BTN_DOWN))  mvy += 1;
    if (mvx != 0 || mvy != 0) {
        if (mvx != 0) { facex = (int)sgn((int)mvx); facey = 0; }
        else          { facey = (int)sgn((int)mvy); facex = 0; }
        float nx = px + mvx * spd * d, ny = py + mvy * spd * d;
        float minx = OX + 3, maxx = OX + GW * TS - 3, miny = OY + 3, maxy = OY + GH * TS - 3;
        int ncx = (int)((nx - OX) / TS); if (nx > minx && nx < maxx && !blocked_cell(ncx, pty())) px = nx;
        int ncy = (int)((ny - OY) / TS); if (ny > miny && ny < maxy && !blocked_cell(ptx(), ncy)) py = ny;
    }

    // ── Z: context action on the faced cell ──
    if (btnp(0, BTN_A)) {
        if (ft == F_PLANT) {
            if (regrow[fy][fx] <= 0) {
                int room = stalk_cap() - carried_stalks();
                if (room <= 0) { say(basket_held ? "basket full" : "hands full", CLR_ORANGE); note(45, INSTR_SQUARE, 1); }
                else { int got = room < 2 ? room : 2; add_carried(got); regrow[fy][fx] = REGROW_T;
                    note(58, INSTR_NOISE, 2); say(str("+%d stalks", got), CLR_GREEN); }
            } else say("not ripe yet", CLR_LIGHT_GREY);
        } else if (ft == F_BASKETSPOT && !basket_held) {
            basket_held = true; fix[basket_gy][basket_gx] = F_NONE;
            say(str("basket  (%d stalks)", basket_stalks), CLR_PEACH); note(72, INSTR_SQUARE, 1);
        } else if (ft == F_BENCH) {
            if (carried_stalks() > 0) { trimming = true; meter = 0; meter_dir = 1;
                say("trim — time your cut (Z)", CLR_LIGHT_YELLOW); }
            else say("no stalks to trim", CLR_LIGHT_GREY);
        } else if (ft == F_TRIMMER) {
            int room = HOPPER_CAP - hopper, n = take_carried(room);
            if (n > 0) { if (hopper == 0) proc_t = PROC_T; hopper += n;
                say(str("loaded %d", n), CLR_BLUE); note(64, INSTR_SQUARE, 2); }
            else if (room <= 0) say("hopper full", CLR_ORANGE);
            else say("no stalks", CLR_LIGHT_GREY);
        } else if (ft == F_DOOR) {
            if (buyer == BUY_NONE) { say("nobody at the door", CLR_LIGHT_GREY); }
            else {
                int delivered = 0, paid = 0;
                if (buyer == BUY_PREMIUM) {              // FINE flowers only; pay scales with quality
                    int give = want < fine_n ? want : fine_n;
                    if (give > 0) {
                        long avg = fine_qsum / fine_n, qmove = fine_qsum * give / fine_n;
                        fine_n -= give; fine_qsum -= qmove;
                        paid = (int)(give * PREMIUM_RATE * avg / 100);
                        delivered = give; want -= give;
                    }
                } else {                                 // BULK: coarse first, then fine; flat rate
                    int from_c = want < coarse_n ? want : coarse_n;
                    if (from_c > 0) { coarse_qsum -= coarse_qsum * from_c / coarse_n; coarse_n -= from_c; }
                    int rem = want - from_c, from_f = rem < fine_n ? rem : fine_n;
                    if (from_f > 0) { fine_qsum -= fine_qsum * from_f / fine_n; fine_n -= from_f; }
                    delivered = from_c + from_f; paid = delivered * BULK_RATE; want -= delivered;
                }
                if (delivered <= 0) {
                    say(buyer == BUY_PREMIUM ? "need FINE flowers!" : "no flowers", CLR_ORANGE);
                    note(45, INSTR_SQUARE, 1);
                } else {
                    cash += paid;
                    if (want <= 0) {                     // order complete — bonus, buyer leaves happy
                        int bonus = buyer == BUY_PREMIUM ? 12 : 6;
                        cash += bonus;
                        say(str("order done!  +$%d", paid + bonus), CLR_LIME_GREEN);
                        strum(72, CHORD_MAJ, INSTR_SQUARE, 3, 40);
                        buyer = BUY_NONE; buyer_gap = BUYER_GAP;
                    } else {
                        say(str("+$%d   (%d more)", paid, want), CLR_YELLOW);
                        note(79, INSTR_SQUARE, 1);
                    }
                }
            }
        }
    }

    // ── X: drop basket / collect trimmer output ──
    if (btnp(0, BTN_B)) {
        if (ft == F_TRIMMER && out_n > 0) {
            int room = CAP_FLOWER - carried_flowers(), take = out_n < room ? out_n : room;
            if (take > 0) {                              // machine output is all COARSE (MACH_Q)
                coarse_n += take; coarse_qsum += (long)take * MACH_Q;
                out_n -= take; out_qsum -= (long)take * MACH_Q;
                say(str("collected %d", take), CLR_PINK); note(80, INSTR_TRI, 2);
            } else say("hands full", CLR_ORANGE);
        } else if (basket_held) {
            int cx = ptx(), cy = pty();
            if (cx >= 0 && cy >= 0 && cx < GW && cy < GH && fix[cy][cx] == F_NONE) {
                basket_held = false; basket_gx = cx; basket_gy = cy; fix[cy][cx] = F_BASKETSPOT;
                say("set the basket down", CLR_PEACH); note(55, INSTR_SQUARE, 1);
            } else say("can't drop here", CLR_LIGHT_GREY);
        }
    }
}

// ── drawing ──────────────────────────────────────────────────────
static void draw_floor(void) {
    for (int gy = 0; gy < GH; gy++) for (int gx = 0; gx < GW; gx++)
        rectfill(OX + gx * TS, OY + gy * TS, TS, TS, ((gx + gy) & 1) ? CLR_DARKER_GREY : CLR_DARK_GREY);
}

static void draw_fixture(int gx, int gy) {
    int x = OX + gx * TS, y = OY + gy * TS, t = fix[gy][gx];
    if (t == F_PLANT) {
        bool ripe = regrow[gy][gx] <= 0;
        rectfill(x + 3, y + TS - 5, TS - 6, 3, CLR_DARK_BROWN);     // little pot
        line(x + TS / 2, y + TS - 5, x + TS / 2, y + 5, CLR_DARK_GREEN);  // stem
        if (ripe) { int bob = (int)(sin_deg(now() * 180 + gx * 50) * 1.5f);
            circfill(x + TS / 2, y + 5 + bob, 3, CLR_PINK);
            circfill(x + TS / 2 - 3, y + 7 + bob, 2, CLR_MAUVE);
            circfill(x + TS / 2 + 3, y + 7 + bob, 2, CLR_MAUVE); }
        else { circfill(x + TS / 2, y + 8, 2, CLR_GREEN);
            bar(x + 3, y + 1, TS - 6, 2, 1.0f - regrow[gy][gx] / REGROW_T, CLR_DARK_GREEN, CLR_BROWNISH_BLACK); }
    } else if (t == F_BENCH) {
        rectfill(x + 1, y + 6, TS - 2, TS - 8, CLR_BROWN);
        rectfill(x + 1, y + 6, TS - 2, 2, CLR_LIGHT_PEACH);
        line(x + 4, y + 10, x + TS - 5, y + 10, CLR_DARK_BROWN);
        print("hand", x - 2, y - 7, CLR_LIGHT_PEACH);
    } else if (t == F_TRIMMER) {
        rectfill(x + 2, y + 3, TS - 4, TS - 5, CLR_LIGHT_GREY);
        rect(x + 2, y + 3, TS - 4, TS - 5, CLR_DARK_GREY);
        int spin = (hopper > 0) ? (int)(now() * 400) % 360 : 0;       // running = blade spins
        line(x + TS / 2, y + TS / 2, x + TS / 2 + (int)(sin_deg(spin) * 4), y + TS / 2 - (int)(cos_deg(spin) * 4),
             hopper > 0 ? CLR_RED : CLR_DARK_GREY);
        print("mach", x - 2, y - 7, CLR_BLUE);
    } else if (t == F_DOOR) {
        rectfill(x + 1, y + 1, TS - 2, TS - 1, CLR_DARK_BROWN);   // frame
        rectfill(x + 3, y + 3, TS - 6, TS - 3, CLR_BROWN);        // door
        pset(x + TS - 5, y + TS / 2, CLR_YELLOW);                 // knob
        print("door", x - 2, y - 7, CLR_PEACH);
    } else if (t == F_BASKETSPOT) {
        rectfill(x + 4, y + 7, TS - 8, TS - 10, CLR_DARK_ORANGE);
        rect(x + 4, y + 7, TS - 8, TS - 10, CLR_BROWN);
        if (basket_stalks > 0) for (int i = 0; i < 3 && i < basket_stalks; i++)
            line(x + 6 + i * 3, y + 7, x + 5 + i * 3, y + 3, CLR_GREEN);
    }
}

static void draw_player(void) {
    int x = (int)px, y = (int)py;
    int moving = btn(0,BTN_LEFT)||btn(0,BTN_RIGHT)||btn(0,BTN_UP)||btn(0,BTN_DOWN);
    int bob = (moving && !trimming) ? (int)(sin_deg(now() * 520)) : 0;
    y += bob;
    ovalfill(x, y + 6, 5, 2, CLR_BROWNISH_BLACK);
    rectfill(x - 4, y - 2, 8, 8, CLR_DARK_GREEN);     // apron
    circfill(x, y - 5, 4, CLR_LIGHT_PEACH);           // head
    pset(x + facex * 5, y - 1 + facey * 5, CLR_WHITE);
    pset(x + facex * 6, y - 1 + facey * 6, CLR_WHITE);
    if (basket_held) {                                 // carried basket
        rectfill(x - 3, y + 1, 6, 4, CLR_DARK_ORANGE);
        if (basket_stalks > 0) line(x - 1, y + 1, x - 2, y - 2, CLR_GREEN);
    }
}

static void draw_buyer(void) {
    if (buyer == BUY_NONE) return;
    int bx = OX + (GW - 2) * TS, by = OY + 5 * TS;       // at the door cell
    int col = buyer == BUY_PREMIUM ? CLR_YELLOW : CLR_LIGHT_GREY;
    int bob = (int)(sin_deg(now() * 240) * 1.0f);
    circfill(bx + TS / 2, by + 5 + bob, 3, col);                          // head
    rectfill(bx + TS / 2 - 3, by + 8 + bob, 6, 6, col);                   // body
    // order bubble above the door
    rectfill(bx - 8, by - 15, TS + 14, 11, CLR_WHITE);
    pset(bx + TS / 2, by - 4, CLR_WHITE);
    print(str("x%d %s", want, buyer == BUY_PREMIUM ? "FINE" : "any"),
          bx - 6, by - 13, buyer == BUY_PREMIUM ? CLR_DARK_PURPLE : CLR_BROWNISH_BLACK);
    // patience bar (turns red as they tire)
    float p = patience / patience0;
    bar(bx - 8, by - 18, TS + 14, 2, p, p > 0.3f ? CLR_GREEN : CLR_RED, CLR_DARK_RED);
}

void draw(void) {
    if (!inited) reset_game();
    cls(CLR_BROWNISH_BLACK);
    draw_floor();
    rect(OX - 1, OY - 1, GW * TS + 2, GH * TS + 2, CLR_DARK_BROWN);

    for (int gy = 0; gy < GH; gy++) for (int gx = 0; gx < GW; gx++)
        if (fix[gy][gx] != F_NONE) draw_fixture(gx, gy);

    int fx, fy; faced_cell(&fx, &fy);
    if (fx >= 0 && fy >= 0 && fx < GW && fy < GH && fix[fy][fx] != F_NONE)
        rect(OX + fx * TS, OY + fy * TS, TS, TS, blink(8) ? CLR_YELLOW : CLR_LIGHT_YELLOW);

    draw_buyer();
    draw_player();

    // ── trim meter (only while at the bench) ──
    if (trimming) {
        int bw = 120, bx = SCREEN_W / 2 - bw / 2, by = SCREEN_H - 30;
        rectfill(bx - 2, by - 2, bw + 4, 14, CLR_BLACK);
        // sweet zone in the middle
        rectfill(bx + bw / 2 - 12, by, 24, 10, CLR_DARK_GREEN);
        rectfill(bx + bw / 2 - 4,  by, 8,  10, CLR_GREEN);
        int mx = bx + (int)(meter * bw);
        rectfill(mx - 1, by - 2, 3, 14, CLR_WHITE);
        print_centered("Z cut at the centre   ·   X leave", SCREEN_W / 2, by - 12, CLR_LIGHT_GREY);
    }

    // ── HUD ──
    rectfill(0, 0, SCREEN_W, OY - 2, CLR_BLACK);
    print(str("DAY %d", day), 4, 2, CLR_WHITE);
    print(str("SKILL %d", skill), 56, 2, CLR_LIGHT_YELLOW);
    bar(110, 3, 30, 5, (float)xp / ((skill + 1) * 6), CLR_LIGHT_YELLOW, CLR_DARKER_GREY);
    print_right(str("$%d", cash), SCREEN_W - 4, 2, CLR_YELLOW);

    // carried: stalks + the two flower lanes
    print(str("%s %d/%d", basket_held ? "BSKT" : "HAND", carried_stalks(), stalk_cap()),
          4, 13, basket_held ? CLR_PEACH : CLR_LIGHT_GREY);
    print(str("FINE %d", fine_n),   96, 13, fine_n   ? CLR_LIME_GREEN : CLR_DARK_GREY);
    print(str("COARSE %d", coarse_n), 150, 13, coarse_n ? CLR_PINK : CLR_DARK_GREY);
    print_right(str("MACH %d>%d", hopper, out_n), SCREEN_W - 4, 13,
                hopper > 0 ? CLR_BLUE : CLR_DARK_GREY);

    // session clock arc
    int sx = 4 + (int)(clockT * (SCREEN_W - 12));
    circfill(sx, OY - 5, 2, CLR_YELLOW);

    if (popT > 0)
        print_centered(popMsg, SCREEN_W / 2, (int)py - 18, popColor);

    if (cash == 0 && missed == 0)
        print_centered("hands make FINE, machine makes COARSE",
                       SCREEN_W / 2, SCREEN_H - 10, blink(12) ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
}
