/* de:meta
{
  "slug": "ringtrack",
  "title": "ring track",
  "status": "active",
  "created": "2026-09-14",
  "kind": [
    "instrument",
    "probe"
  ],
  "teaches": [
    "per-instrument-fx-bus"
  ],
  "resizable": true,
  "lineage": "Engine-reach §7.3 proof: note-tracking ringmod is a RATIO on the shipped FX_RINGMOD insert, not a third engine. Fixed-Hz clang goes atonal up the keyboard; ratio clang stays harmonic. docs/design/engine-reach-ringmod-research.md.",
  "description": {
    "summary": "Play a saw up the keyboard through a ring modulator. HZ is a fixed 440 Hz carrier — the clang goes atonal as you leave A4. RATIO tracks the played note, so the clang stays the same colour on every key.",
    "detail": "The hearable answer to engine-reach §7.3. One slot, one saw, one ring-mod insert. HZ calls instrument_ringmod(slot, 440, mix) — the Dalek / atonal clang we already shipped. RATIO calls instrument_ringmod_ratio(slot, ratio, mix) — carrier = ratio × last-played pitch, so a fifth (3/2) puts sidebands at 0.5× and 2.5× of every note. Autoplay walks a C-major scale so the difference is a scale, not an argument. Last-started voice wins; a held chord shares one carrier (that is the bus, and it is honest).",
    "controls": "Tap HZ / RATIO (or H / R). Tap 1/2, 1, 3/2, 2 to pick the interval. Keybed: A-K white keys, W-E-T-Y-U-O-P black, Z/X octave (+ mouse/touch/MIDI). M = autoplay on/off."
  }
}
de:meta */
#include "studio.h"
#include "keybed.h"
#include "ui.h"
#include "face.h"

// RING TRACK — engine-reach §7.3, decision A.
//
// One saw, one ring-mod insert. HZ = fixed 440 Hz (the clang goes atonal as you
// leave A4). RATIO = carrier follows the played pitch (the clang stays harmonic).
// That is the whole claim. Autoplay walks a C-major scale so you hear it without
// playing. See docs/design/engine-reach-ringmod-research.md.
//
//   H / R     HZ / RATIO
//   1 2 3 4   ratio chips  1/2 · 1 · 3/2 · 2
//   M         autoplay
//   A–K …     keybed (touch / mouse / QWERTY / MIDI)

#define SLOT 5
#define MIX  0.80f
#define HZ   440.0f

static const float RATIOS[4] = { 0.5f, 1.0f, 1.5f, 2.0f };
static const char *RLAB[4]   = { "1/2", "1", "3/2", "2" };

static const int SCALE[] = { 60, 62, 64, 65, 67, 69, 71, 72 };  // C4..C5
#define NSCALE ((int)(sizeof SCALE / sizeof SCALE[0]))
#define NOTE_FR  22
#define GAP_FR    4

static int   mode = 1;          // 0 = HZ, 1 = RATIO
static int   rsel = 2;          // 3/2 — a fifth, the hearable interval
static int   ap = 1;            // autoplay on
static int   f = 0;
static int   ap_h = 0;
static int   last_mode = -1, last_rsel = -1;

static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP, 0.30f, "nav"  },
    { FACE_HERO, 0,        0.00f, "keys" },
};
#define NZ 2
static Face g_face;
static Box  z_nav, z_keys;

static void apply_fx(void) {
    if (mode == 0) instrument_ringmod(SLOT, HZ, MIX);
    else           instrument_ringmod_ratio(SLOT, RATIOS[rsel], MIX);
    last_mode = mode;
    last_rsel = rsel;
}

static void relayout(void) {
    face_resize();
    Box area = face_area(2);
    g_face = face_layout(area, ZONES, NZ, 8);
    z_nav  = g_face.box[0];
    z_keys = g_face.box[1];
    keybed_layout(z_keys.x, z_keys.y, z_keys.w, z_keys.h);
}

void init(void) {
    instrument(SLOT, INSTR_SAW, 6, 80, 5, 180);
    keybed_config(SLOT, 4, 14);
    apply_fx();
}

void update(void) {
    relayout();
    keybed_update();
    f++;

    if (keyp('H')) mode = 0;
    if (keyp('R')) mode = 1;
    if (keyp('M')) ap = !ap;
    if (keyp('1')) rsel = 0;
    if (keyp('2')) rsel = 1;
    if (keyp('3')) rsel = 2;
    if (keyp('4')) rsel = 3;

    if (mode != last_mode || rsel != last_rsel) apply_fx();

    if (ap) {
        int period = NSCALE * (NOTE_FR + GAP_FR);
        int t = (f - 1) % period;
        int i = t / (NOTE_FR + GAP_FR);
        int sub = t % (NOTE_FR + GAP_FR);
        if (sub == 0) {
            if (ap_h) { note_off(ap_h); ap_h = 0; }
            ap_h = note_on(SCALE[i], SLOT, 6);
        } else if (sub == NOTE_FR && ap_h) {
            note_off(ap_h);
            ap_h = 0;
        }
    } else if (ap_h) {
        note_off(ap_h);
        ap_h = 0;
    }

#ifdef DE_TRACE
    watch("mode", "%d", mode);
    watch("ratio", "%.2f", (double)(mode ? RATIOS[rsel] : 0.0f));
    watch("hz", "%d", mode ? 0 : (int)HZ);
#endif
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    ui_begin();

    // ── nav: title + HZ/RATIO + interval chips ──
    font(FONT_SMALL);
    print("RING TRACK", z_nav.x + 4, z_nav.y + 3, CLR_WHITE);
    print(mode ? "ratio x pitch" : "fixed 440 Hz", z_nav.x + 90, z_nav.y + 3,
          mode ? CLR_LIGHT_YELLOW : CLR_ORANGE);

    int bw = 36, bh = 14;
    int by = z_nav.y + 16;
    int bx = z_nav.x + 4;
    if (ui_button(bx, by, bw, bh, mode == 0 ? ">HZ" : " HZ")) { mode = 0; apply_fx(); }
    if (ui_button(bx + bw + 4, by, bw + 8, bh, mode == 1 ? ">RATIO" : " RATIO")) { mode = 1; apply_fx(); }
    if (ui_button(bx + bw * 2 + 20, by, 28, bh, ap ? ">M" : " M")) ap = !ap;

    int cy = by + bh + 4;
    for (int i = 0; i < 4; i++) {
        int x = z_nav.x + 4 + i * 28;
        int on = (mode == 1 && rsel == i);
        if (ui_button(x, cy, 26, 12, RLAB[i]) && mode == 1) { rsel = i; apply_fx(); }
        if (on) rect(x - 1, cy - 1, 28, 14, CLR_LIGHT_YELLOW);
    }
    print(mode ? str("carrier = %.2f x note", (double)RATIOS[rsel])
               : "carrier stuck at 440 — leave A4, go atonal",
          z_nav.x + 4, z_nav.y + z_nav.h - 10, CLR_DARK_GREY);

    keybed_draw();
    ui_end();
}
