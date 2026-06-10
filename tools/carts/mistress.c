#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── ELECTRIC MISTRESS ──────────────────────────────────────────────────────
// Electro-Harmonix's Electric Mistress (1976) — the flanger pedal on half the
// records you know (Andy Summers, Gilmour, the chime on "Walking on the Moon").
// The showcase for `flanger()`: a short swept delay with feedback whose moving
// comb-filter notches give the JET-PLANE whoosh and the metallic chime. Flanging
// only reads on a rich, broadband source — so you strum a GUITAR through it, and
// the one-button JET fires a noise swell that *is* the textbook jet overhead.
// Third box in the effects family: spacecho (echo) · cathedral (reverb) ·
// juno (chorus) · mistress (flanger).
//
//   FLANGER   the footswitch — on/off (the dry guitar vs the swept comb).
//   RATE      sweep speed     DEPTH  sweep range
//   FEEDBACK  the jet/metallic intensity (low = gentle, high = screaming comb)
//   MIX       dry..wet
//   JET       fire a 2s noise swell through a deep slow flange — the jet whoosh
//   PADS      strum E / A / D / G (or press 1..4)   SPACE  AUTO progression

#define GTR 8
#define NZ  9

static float k_rate = 0.10f;   // → 0.05..5 Hz
static float k_dep  = 0.75f;
static float k_fb   = 0.70f;   // → 0..0.95
static float k_mix  = 0.55f;
static bool  on     = true;    // footswitch (on at boot — it's a flanger pedal)
static bool  autop  = true;
static bool  show_help;
static int   last_step = -1, auto_i = 0;
static int   jet = 0;          // frames remaining of the JET swell (cosmetic)
static float comb_ph = 0.0f;   // comb-filter animation phase

// four open-guitar chords (E A D G), MIDI roots
static const int ROOT[4] = { 40, 45, 50, 43 };
static const char *CNAME[4] = { "E", "A", "D", "G" };

static float rate_hz(void) { return 0.05f + k_rate * 4.95f; }
static float fb_v(void)    { return k_fb * 0.95f; }

static void apply_flanger(void) { flanger(rate_hz(), k_dep, fb_v(), on ? k_mix : 0.0f); }

static void strum_chord(int i) { strum(ROOT[i], CHORD_MAJ, GTR, 6, 18); }

static void fire_jet(void) {
    // a deep slow flange + a 2s noise swell = the classic jet overhead
    flanger(0.12f, 0.97f, 0.9f, 0.6f);
    on = true;
    hit(48, NZ, 6, 2000);
    jet = 130;
}

void init(void) {
    bpm(52);
    instrument(GTR, INSTR_GUITAR, 2, 0, 7, 600);   // steel-ish string
    instrument_harmonics(GTR, 0.55f);              // resonant body
    instrument_timbre(GTR, 0.70f);                 // bright steel
    instrument(NZ, INSTR_NOISE, 400, 0, 7, 600);   // the JET source (slow swell)
    apply_flanger();
}

void update(void) {
    if (keyp(KEY_SPACE)) { autop = !autop; last_step = -1; }
    if (keyp('H')) show_help = !show_help;
    if (keyp('F')) { on = !on; apply_flanger(); }
    if (keyp('J')) fire_jet();
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) strum_chord(i);

    if (autop) {
        int step = beat();
        if (step != last_step) {
            last_step = step;
            if ((step & 1) == 0) { strum_chord(auto_i); auto_i = (auto_i + 1) % 4; }
        }
    }

    comb_ph += rate_hz() / 60.0f;                  // visual sweep tracks the LFO
    if (comb_ph >= 1.0f) comb_ph -= 1.0f;
    if (jet > 0) jet--;
}

void draw(void) {
    char buf[48];
    cls(CLR_DARK_GREEN);                            // the EHX pedal is green

    print("ELECTRIC MISTRESS", 8, 6, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print("flanger / filter matrix  -  flanger() showcase", 8, 18, CLR_MEDIUM_GREEN);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(24, 26, 272, 150, CLR_BLACK);
        rect(24, 26, 272, 150, CLR_LIME_GREEN);
        print("ELECTRIC MISTRESS", 96, 34, CLR_LIGHT_YELLOW);
        static const char *HL[] = {
            "A flanger: a short swept delay whose",
            "moving comb notches give the jet whoosh",
            "+ metallic chime. Needs a rich source -",
            "so strum the guitar through it.",
            "",
            "FLANGER  footswitch on/off        [F]",
            "RATE DEPTH FEEDBACK MIX  the flange",
            "FEEDBACK high = the screaming jet comb",
            "JET   a noise swell = jet overhead [J]",
            "1..4  strum E/A/D/G   SPACE  AUTO",
        };
        for (int i = 0; i < 10; i++)
            print(HL[i], 36, 46 + i * 12, i < 4 ? CLR_LIGHT_PEACH : CLR_WHITE);
        ui_begin();
        if (ui_button(130, 162, 60, 14, "CLOSE")) show_help = false;
        ui_end();
        return;
    }

    // ── the comb-filter window — notches sweeping when the flanger is on ───
    int gx = 8, gy = 30, gw = 304, gh = 56;
    rectfill(gx, gy, gw, gh, CLR_BLACK);
    rect(gx, gy, gw, gh, CLR_MEDIUM_GREEN);
    print("COMB", gx + 4, gy + 3, CLR_MEDIUM_GREEN);
    if (on || jet > 0) {
        // draw a swept comb: several notches whose positions slide with comb_ph
        float sweep = (1.0f - cosf(comb_ph * 6.2831853f)) * 0.5f;   // 0..1 triangle-ish
        for (int n = 1; n <= 7; n++) {
            float base = (float)n / 8.0f;
            float nx = base + sweep * k_dep * 0.06f * n;            // higher notches move more
            int x = gx + 4 + (int)(nx * (gw - 8));
            if (x > gx + gw - 2) continue;
            int depth = (int)(gh * (0.4f + 0.5f * fb_v()));         // feedback deepens the notch
            line(x, gy + gh - 4, x, gy + gh - 4 - depth, n & 1 ? CLR_LIME_GREEN : CLR_GREEN);
        }
        if (jet > 0) print("JET!", gx + gw / 2 - 14, gy + 6, (jet & 4) ? CLR_LIGHT_YELLOW : CLR_ORANGE);
    } else {
        print("(flanger off - dry)", gx + gw / 2 - 60, gy + gh / 2 - 4, CLR_DARK_GREEN);
    }

    // ── knobs + footswitch + JET ───────────────────────────────────────────
    ui_begin();
    bool ch = false;
    if (ui_knob(&k_rate, 36,  118, "RATE"))     ch = true;
    if (ui_knob(&k_dep,  96,  118, "DEPTH"))    ch = true;
    if (ui_knob(&k_fb,   156, 118, "FEEDBACK")) ch = true;
    if (ui_knob(&k_mix,  216, 118, "MIX"))      ch = true;
    if (ch) apply_flanger();
    if (ui_button(264, 104, 48, 22, on ? "FLANGER" : "BYPASS")) { on = !on; apply_flanger(); }
    if (ui_button(264, 130, 48, 16, "JET")) fire_jet();
    if (ui_button(264, 30, 30, 12, autop ? "AUTO*" : "AUTO")) { autop = !autop; last_step = -1; }
    ui_end();

    // ── chord pads ───────────────────────────────────────────────────────
    for (int i = 0; i < 4; i++) {
        int x = 8 + i * 60, y = 152, w = 56, h = 38;
        if (tapp(x, y, w, h)) strum_chord(i);
        rectfill(x, y, w, h, CLR_DARK_GREEN);
        rect(x, y, w, h, CLR_LIME_GREEN);
        print(CNAME[i], x + w / 2 - (int)strlen(CNAME[i]) * 4, y + h / 2 - 4, CLR_LIGHT_YELLOW);
        font(FONT_SMALL);
        sprintf(buf, "%d", i + 1);
        print(buf, x + 3, y + 3, CLR_MEDIUM_GREEN);
        font(FONT_NORMAL);
    }
    sprintf(buf, "%.1fHz", rate_hz());
    print(buf, 252, 152, CLR_MEDIUM_GREEN);
    print("H help", 270, 178, CLR_MEDIUM_GREEN);
}
