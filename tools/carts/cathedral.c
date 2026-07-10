/* de:meta
{
  "slug": "cathedral",
  "title": "cathedral",
  "status": "active",
  "created": "2026-06-10",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling",
    "chord-voicing"
  ],
  "lineage": "Port of navkit's Schroeder reverberator; teaches reverb-as-space (tail length + damping) vs echo, paired with a slow chord auto-player to let the room speak.",
  "description": "The showcase for THE reverb bus (the first effect on the new master FX section). A church organ in a vast stone hall: strike a chord and it BLOOMS - the dry attack is brief, but the reverb tail swells and hangs in the air for seconds, the way a chord rings under a vaulted ceiling. Echo repeats taps; reverb is a SPACE, and that difference is the whole point. Drag the three knobs: SIZE = chapel to endless cathedral (tail length), DAMP = bright stone to warm dark stone (tail tone), WET = how drenched the organ is. Tap a chord pad or press 1..6 to ring it into the hall; SPACE toggles AUTO, a slow self-playing processional; H for help. Each chord throws an expanding ring of light that rises through the arches and fades over the length of its own reverb tail. Built on instrument_reverb() sends into a reverb(size,damping) room - a port of navkits Schroeder reverberator."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── CATHEDRAL ──────────────────────────────────────────────────────────────
// The showcase cart for THE reverb bus (instrument-engines §8.10 — the first
// effect on the master FX section, decisions/0015). A church organ in a vast
// stone hall: strike a chord and it BLOOMS. The dry attack is brief, but the
// reverb tail swells and hangs in the air for seconds — the way a chord rings
// under a vaulted ceiling. That bloom is the whole point: echo repeats taps,
// reverb is a SPACE. This cart exists to prove the difference.
//
// How it maps onto the API:
//   reverb(size, damping)      — the room itself. size = chapel → endless
//                                cathedral (tail LENGTH); damping = bright
//                                stone → warm dark stone (tail TONE).
//   instrument_reverb(SLOT, x) — the SEND: how drenched the organ is. The dry
//                                organ always stays full; this is the wet copy
//                                that blooms into the hall.
//
//   SIZE / DAMP / WET   the three knobs (drag, or hover+wheel on desktop).
//   PADS  six chords    tap a pad — or press 1..6 — to ring it into the hall.
//   SPACE / AUTO        the chords play themselves, slowly, so you can just
//                       stand in the room and listen to it bloom and decay.
//   the bloom is DRAWN: each chord throws an expanding ring of light that
//   rises through the arches and fades over the length of its own reverb tail.

#define SLOT 8

// the three reverb knobs — ui.h wants floats 0..1
static float k_size = 0.88f;   // big bright cathedral by default
static float k_damp = 0.28f;   // mostly-bright stone
static float k_wet  = 0.62f;   // well drenched

// six chords, voiced around C4 — a calm cathedral progression
static const int CHORD[6][3] = {
    { 60, 64, 67 },   // C
    { 55, 59, 62 },   // G
    { 57, 60, 64 },   // Am
    { 52, 55, 59 },   // Em
    { 53, 57, 60 },   // F
    { 50, 53, 57 },   // Dm
};
static const char *CNAME[6] = { "C", "G", "Am", "Em", "F", "Dm" };

static bool  autop = true;     // AUTO: the self-playing progression (on at boot)
static bool  show_help;
static int   last_step = -1;   // beat-clock edge tracker for AUTO
static int   auto_i = 0;       // which chord AUTO plays next

// blooms — one expanding ring of light per chord struck, fading over its tail
#define NBLOOM 12
typedef struct { float age, life; int x, y, ci; bool on; } Bloom;
static Bloom bloom[NBLOOM];
static float glow = 0.0f;      // overall hall luminance, swells with activity

// pad layout (bottom row)
#define PADY 150
#define PADH 40
static int pad_x(int i) { return 8 + i * 51; }
static int pad_w(void)  { return 47; }

static void apply_room(void) { reverb(k_size, k_damp); }
static void apply_send(void) { instrument_reverb(SLOT, k_wet); }

static void play_chord(int i) {
    // a held-ish organ chord: brief dry body, the reverb does the blooming
    for (int n = 0; n < 3; n++) hit(CHORD[i][n], SLOT, 5, 520);
    // spawn the visible bloom from this pad
    for (int b = 0; b < NBLOOM; b++) {
        if (!bloom[b].on) {
            bloom[b] = (Bloom){ 0.0f, 90.0f + k_size * 230.0f,
                                pad_x(i) + pad_w() / 2, PADY, i, true };
            break;
        }
    }
    glow += 0.45f;
    if (glow > 1.0f) glow = 1.0f;
}

void init(void) {
    bpm(48);                                       // slow, processional
    // a church-organ pad: soft attack, sustained, long release
    instrument(SLOT, INSTR_ORGAN, 30, 0, 6, 600);
    instrument_harmonics(SLOT, 0.62f);             // full-ish drawbars
    instrument_timbre(SLOT, 0.5f);
    apply_room();
    apply_send();
}

void update(void) {
    // pure logic here; all drawing + ui.h widgets live in draw() (after cls,
    // or cls wipes them — the uikit/sfxgen convention).
    if (keyp(KEY_SPACE)) { autop = !autop; last_step = -1; }
    if (keyp('H')) show_help = !show_help;
    for (int i = 0; i < 6; i++) if (keyp('1' + i)) play_chord(i);

    // AUTO: walk the progression slowly (one chord every 2 beats)
    if (autop) {
        int step = beat();
        if (step != last_step) {
            last_step = step;
            if ((step & 1) == 0) { play_chord(auto_i); auto_i = (auto_i + 1) % 6; }
        }
    }

    // advance + retire blooms
    for (int b = 0; b < NBLOOM; b++) {
        if (!bloom[b].on) continue;
        bloom[b].age += 1.0f;
        if (bloom[b].age >= bloom[b].life) bloom[b].on = false;
    }
    glow *= 0.985f;                                // the hall slowly returns to dark
}

// a chord-pad color, dimmed
static int pad_color(int i) {
    static const int C[6] = { CLR_INDIGO, CLR_DARK_PURPLE, CLR_TRUE_BLUE,
                              CLR_DARK_BLUE, CLR_MAUVE, CLR_DARKER_PURPLE };
    return C[i];
}

void draw(void) {
    char buf[40];
    cls(CLR_BROWNISH_BLACK);

    // ── the vaulted hall: receding arches, brighter as the room glows ──────
    int litbg = glow > 0.5f ? CLR_DARKER_BLUE : CLR_BROWNISH_BLACK;
    rectfill(0, 70, 320, 78, litbg);
    for (int a = 0; a < 5; a++) {
        int cx = 160, top = 18 + a * 8, r = 150 - a * 26;
        int col = a == 4 ? CLR_DARK_GREY : CLR_DARK_PURPLE;
        // a pointed-arch outline (two arcs meeting at an apex) — drawn as a fan of points
        for (int s = 0; s <= 40; s++) {
            float t = s / 40.0f;                   // 0..1 across the span
            float xx = (t - 0.5f) * 2.0f;          // -1..1
            int   x  = cx + (int)(xx * r);
            int   y  = top + (int)((1.0f - sqrtf(1.0f - xx * xx)) * (r * 0.7f));
            if (x >= 0 && x < 320 && y >= 0 && y < 200) pset(x, y, col);
        }
    }
    line(0, 148, 320, 148, CLR_DARK_GREY);         // floor line

    // ── the blooms: each chord's expanding ring of light, rising + fading ──
    for (int b = 0; b < NBLOOM; b++) {
        if (!bloom[b].on) continue;
        float t   = bloom[b].age / bloom[b].life;  // 0..1 over the tail
        float rad = 6.0f + t * 120.0f;             // grows outward
        int   ry  = bloom[b].y - (int)(t * 96.0f); // rises toward the vault
        // fade the ring as it ages: bright → warm → gone (mirrors the tail decay)
        int col = t < 0.25f ? CLR_LIGHT_YELLOW : t < 0.5f ? CLR_YELLOW
                 : t < 0.75f ? CLR_ORANGE : CLR_MAUVE;
        if (t < 0.9f) circ(bloom[b].x, ry, (int)rad, col);
        if (t < 0.4f) circ(bloom[b].x, ry, (int)(rad * 0.6f), CLR_LIGHT_PEACH);
    }

    // ── title ──────────────────────────────────────────────────────────────
    print("CATHEDRAL", 8, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("reverb bus showcase", 8, 18, CLR_DARK_GREY);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(24, 24, 272, 156, CLR_BLACK);
        rect(24, 24, 272, 156, CLR_INDIGO);
        print("CATHEDRAL", 116, 32, CLR_LIGHT_YELLOW);
        static const char *HL[] = {
            "Strike a chord; it BLOOMS into the",
            "hall and hangs for seconds. The dry",
            "organ is brief - the REVERB is the",
            "space the chord rings in.",
            "",
            "SIZE   chapel .. endless cathedral",
            "DAMP   bright .. warm/dark stone",
            "WET    how drenched the organ is",
            "1..6   ring a chord into the hall",
            "SPACE  AUTO - it plays itself",
        };
        for (int i = 0; i < 10; i++)
            print(HL[i], 36, 46 + i * 11, i < 4 ? CLR_LIGHT_PEACH : CLR_WHITE);
        ui_begin();
        if (ui_button(130, 162, 60, 14, "CLOSE")) show_help = false;
        ui_end();
        return;
    }

    // ── control strip + the three reverb knobs (ui.h; drawn AFTER cls) ─────
    rectfill(8, 30, 304, 64, CLR_DARKER_PURPLE);
    rect(8, 30, 304, 64, CLR_INDIGO);
    sprintf(buf, "SIZE %d   DAMP %d   WET %d", (int)(k_size * 100), (int)(k_damp * 100), (int)(k_wet * 100));
    print(buf, 14, 80, CLR_LIGHT_PEACH);
    ui_begin();
    if (ui_knob(&k_size, 40,  52, "SIZE")) apply_room();
    if (ui_knob(&k_damp, 110, 52, "DAMP")) apply_room();
    if (ui_knob(&k_wet,  180, 52, "WET"))  apply_send();
    if (ui_button(244, 40, 64, 16, autop ? "AUTO ON" : "AUTO"))  { autop = !autop; last_step = -1; }
    if (ui_button(244, 62, 64, 16, "HELP")) show_help = true;
    ui_end();

    // ── chord pads (custom-styled; tapp handles touch, keys 1..6 in update) ─
    for (int i = 0; i < 6; i++) {
        int x = pad_x(i);
        if (tapp(x, PADY, pad_w(), PADH)) play_chord(i);
        rectfill(x, PADY, pad_w(), PADH, pad_color(i));
        rect(x, PADY, pad_w(), PADH, CLR_INDIGO);
        print(CNAME[i], x + pad_w() / 2 - (int)strlen(CNAME[i]) * 4, PADY + PADH / 2 - 4, CLR_LIGHT_PEACH);
        font(FONT_SMALL);
        sprintf(buf, "%d", i + 1);
        print(buf, x + 3, PADY + 3, CLR_MAUVE);
        font(FONT_NORMAL);
    }
}
