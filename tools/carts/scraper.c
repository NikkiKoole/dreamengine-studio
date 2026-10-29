/* de:meta
{
  "slug": "scraper",
  "title": "scraper",
  "status": "active",
  "created": "2026-10-29",
  "kind": ["probe", "tech-demo"],
  "genre": null,
  "teaches": ["drum-synthesis"],
  "lineage": "The showcase + acceptance probe for INSTR_SCRAPE (GitHub #45): scraped and rattled percussion made of discrete teeth through a small body, replacing the instrument-recipes cr78/guiro trick (an LFO chopping noise). Targets measured from the guiro and quijada samples Puppetmaker 2 shipped.",
  "description": {
    "summary": "A guiro, a quijada, a cabasa and a ratchet, each one instrument() and three knobs.",
    "detail": "Four INSTR_SCRAPE patches play a little hand-percussion groove: the guiro scrapes on the offbeats, the quijada rattles at the end of the bar, the cabasa shuffles sixteenths and the ratchet winds up before the turnaround. Every tooth is its own short burst through a body (gourd, bone, metal), so a scrape has clicks and a rattle dies away, rather than a tremolo on a hiss. The bars at the top draw each patch's three macros: BODY, RATE and GESTURE. Build with DE_DEFINES=PROBE for the measuring timeline (one guiro and one quijada at fixed times, for tools/scrape-check.js).",
    "controls": "It plays on its own. 1-4 hit one instrument (guiro / quijada / cabasa / ratchet); hold a key to scrape longer."
  }
}
de:meta */
//
// scraper — can one instrument() and three macros make a guiro that has TEETH? The probe for INSTR_SCRAPE.
//
// The patches below are the recipes (instrument-recipes.md): BODY / RATE / GESTURE on harmonics /
// timbre / morph. PROBE (DE_DEFINES=PROBE) plays the guiro at frame PR_GUIRO for PR_GUIRO_LEN frames
// and the quijada at PR_QUIJ, so tools/scrape-check.js can measure each against the sample it replaces.

#include "studio.h"
#include "cursor.h"

enum { GUIRO = 5, QUIJADA, CABASA, RATCHET, NKIT };
typedef struct { int slot; const char *name; float body, rate, gesture; int note, col; } Patch;
static const Patch KIT[4] = {
    { GUIRO,   "GUIRO",   0.00f, 0.87f, 0.00f, 60, CLR_ORANGE },
    { QUIJADA, "QUIJADA", 0.50f, 0.35f, 1.00f, 60, CLR_PEACH },
    { CABASA,  "CABASA",  1.00f, 0.62f, 0.55f, 60, CLR_LIGHT_GREY },
    { RATCHET, "RATCHET", 0.38f, 0.10f, 0.00f, 55, CLR_BROWN },
};

#ifdef PROBE
#define PR_GUIRO      20
#define PR_GUIRO_LEN  16     // ~270 ms: the sample's audible scrape
#define PR_QUIJ       80
#define PR_QUIJ_LEN   55
#endif

static int tick = 0;
static int held[4] = { -1, -1, -1, -1 };
static int lit[4];

static void play(int k, int frames_or_hold) {   // frames > 0 = a timed hit, 0 = held until the key lifts
    if (held[k] >= 0) note_off(held[k]);
    held[k] = frames_or_hold > 0 ? -1 : note_on(KIT[k].note, KIT[k].slot, 6);
    if (frames_or_hold > 0) hit(KIT[k].note, KIT[k].slot, 6, frames_or_hold * 1000 / 60);
    lit[k] = 8;
}

void init(void) {
    for (int k = 0; k < 4; k++) {
        instrument(KIT[k].slot, INSTR_SCRAPE, 0, 0, 7, 30);
        instrument_harmonics(KIT[k].slot, KIT[k].body);
        instrument_timbre(KIT[k].slot, KIT[k].rate);
        instrument_morph(KIT[k].slot, KIT[k].gesture);
        instrument_level(KIT[k].slot, 0.8f);
    }
    bpm(100);
}

void update(void) {
    tick++;
#ifdef PROBE
    if (tick == PR_GUIRO) play(0, PR_GUIRO_LEN);
    if (tick == PR_QUIJ)  play(1, PR_QUIJ_LEN);
#else
    // a 16-step bar at 100 bpm = 9 frames a step
    int step = (tick / 9) % 16, at = tick % 9 == 0;
    if (at) {
        if (step % 4 == 2) play(0, 14);                          // guiro on the offbeats
        if (step == 14)    play(1, 50);                          // quijada at the end of the bar
        if (step % 2 == 1) play(2, 5);                           // cabasa shuffle
        if (step == 8 && (tick / 144) % 2 == 1) play(3, 30);     // ratchet winds up, every other bar
    }
    for (int k = 0; k < 4; k++) {
        if (keyp('1' + k)) play(k, 0);
        if (keyr('1' + k) && held[k] >= 0) { note_off(held[k]); held[k] = -1; }
    }
#endif
    for (int k = 0; k < 4; k++) if (lit[k] > 0) lit[k]--;
#ifdef DE_TRACE
    watch("tick", "%d", tick);
#endif
}

static void knob_bar(int x, int y, int w, float v, int col) {
    rect(x, y, w, 5, CLR_DARK_GREY);
    rectfill(x + 1, y + 1, (int)((w - 2) * v), 3, col);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print("SCRAPER", 6, 4, CLR_YELLOW);
    font(FONT_SMALL);
    print("teeth through a body, not a tremolo on a hiss", 70, 6, CLR_LIGHT_GREY);
    for (int k = 0; k < 4; k++) {
        int x = 8 + k * 78, y = 24, on = lit[k] > 0 || held[k] >= 0;
        rect(x, y, 72, 150, on ? KIT[k].col : CLR_DARK_GREY);
        print(KIT[k].name, x + 4, y + 4, on ? KIT[k].col : CLR_LIGHT_GREY);
        print("BODY", x + 4, y + 16, CLR_DARK_GREY);    knob_bar(x + 4, y + 23, 64, KIT[k].body, KIT[k].col);
        print("RATE", x + 4, y + 32, CLR_DARK_GREY);    knob_bar(x + 4, y + 39, 64, KIT[k].rate, KIT[k].col);
        print("GESTURE", x + 4, y + 48, CLR_DARK_GREY); knob_bar(x + 4, y + 55, 64, KIT[k].gesture, KIT[k].col);
        // the instrument itself: ridges, with the stick sliding while it sounds
        int cx = x + 36, cy = y + 104;
        for (int r = 0; r < 9; r++) {
            int rx = cx - 24 + r * 6;
            line(rx, cy - 18, rx, cy + 18, KIT[k].col);
        }
        if (on) {
            int sx = cx - 24 + (tick * 3) % 48;
            line(sx, cy - 26, sx + 8, cy + 26, CLR_WHITE);
        }
        char key[2] = { (char)('1' + k), 0 };
        print(key, x + 33, y + 138, CLR_DARK_GREY);
    }
    print("1-4: hit one   hold: scrape longer", 8, 190, CLR_DARK_GREY);
    font(FONT_NORMAL);
    cursor_draw(CUR_ARROW);
}
