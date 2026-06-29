/* de:meta
{
  "title": "LPG",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "lowpass-gate",
    "wavefolder",
    "west-coast-synthesis"
  ],
  "homage": "Buchla low-pass gate",
  "description": "A low-pass gate — the signature voice of west-coast / Buchla synthesis. When a real struck thing decays it gets quieter AND duller at once (the highs die before the fundamental); a low-pass gate models that by closing a VCA and a lowpass filter together from one springy vactrol envelope. So this isn't a filter you sweep — it's the gate that turns a steady tone into an organic 'bonk'. Eight tuned marimba bars are each a held tone, silent until struck; a strike fires a vactrol envelope that drives note_vol AND note_cutoff AND the wavefolder in lockstep. FOLD adds harmonics (sine→metallic) for the gate to chew on; DECAY is short woody plonk → long ringing marimba; COUPLE is the headline A/B — 0 = a plain VCA (the tone stays bright as it fades), 1 = the full low-pass gate (brightness dies with the volume, the organic plonk). Each bar glows white→yellow→orange→wood as its gate closes, so you SEE the timbre decay. Play it like a marimba: A S D F G H J K, or tap the bars. Built on a pool of held note_on voices + coupled note_vol/note_cutoff/note_drive — the one liveset plaything with a genuinely new timbre."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// LPG — a low-pass gate, the signature voice of west-coast / Buchla synthesis.
//
// The whole idea: when a real struck/plucked thing decays it gets QUIETER and DULLER at
// once — the highs die before the fundamental. A low-pass gate models that by closing a
// VCA and a lowpass filter together from ONE springy (vactrol) envelope. So this isn't a
// filter you sweep; it's the gate that turns a steady tone into an organic "bonk".
//
// Each of the 8 tuned bars is a held tone, silent until you strike it. A strike fires a
// vactrol envelope that drives note_vol AND note_cutoff (and the wavefolder) in lockstep:
//   • FOLD     adds harmonics (sine→metallic) for the gate to chew on (west-coast timbre)
//   • COUPLE   the headline A/B — 0 = a plain VCA (tone stays bright as it fades),
//              1 = full LPG (brightness dies with the volume — the organic plonk)
//   • DECAY    short woody plonk → long ringing marimba
//
//   • A S D F G H J K  (or tap the bars) strike them — play it like a marimba/mbira.
//
//   Built on a pool of held note_on voices + coupled note_vol/note_cutoff/note_drive — no
//   new engine DSP. The last of the liveset playthings (cart-library-direction §2c); the
//   one with a genuinely new timbre.

#define NB 8
#define SL 5
static int   h[NB];                 // one held voice per bar
static float env[NB];               // each bar's vactrol envelope (0..1)
static bool  atk[NB];               // in the (brief, rounded) attack phase
static int   strk[NB];              // frame() last struck

static float k_decay  = 0.45f;      // → 0.15..3.0 s ring
static float k_fold   = 0.55f;      // → wavefolder amount (harmonics to gate)
static float k_couple = 1.0f;       // → 0 VCA-only … 1 full low-pass gate

// C major pentatonic, two octaves of bars (a mellow marimba range)
static const int PENTA[5] = { 0, 2, 4, 7, 9 };
static const int KEY[NB]  = { 'A','S','D','F','G','H','J','K' };
static int bar_pitch(int b) { return 60 + 12 * (b / 5) + PENTA[b % 5]; }

#define LX 18
#define BARW 30
#define BARGAP 6
#define BASE_Y 150

static void strike(int b) { atk[b] = true; if (env[b] < 0.05f) env[b] = 0.05f; strk[b] = frame(); }

void init(void) {
    instrument(SL, INSTR_TRI, 4, 0, 9, 90);           // sustaining base tone — the cart's env does the shaping
    instrument_filter(SL, FILTER_LOW, 8000, 3);       // the gate's lowpass (note_cutoff rides it per voice)
    instrument_drive_mode(SL, DRIVE_FOLD);            // west-coast wavefolder (note_drive rides it per voice)
    instrument_drive(SL, 0.0f);
    for (int b = 0; b < NB; b++) {
        h[b] = note_on(bar_pitch(b), SL, 0);          // alive but silent
        note_glide(h[b], 0);
        env[b] = 0; atk[b] = false; strk[b] = -100;
    }
}

void update(void) {
    for (int b = 0; b < NB; b++) {
        if (keyp(KEY[b])) strike(b);
        int bx = LX + b * (BARW + BARGAP);
        if (tapp(bx, 40, BARW, BASE_Y - 40)) strike(b);   // tap/click anywhere on the bar's column
    }

    float T = 0.15f + k_decay * k_decay * 2.85f;          // squared → finer control of short plonks
    float dcoef = expf(logf(0.0015f) * (1.0f / 60.0f) / T);
    for (int b = 0; b < NB; b++) {
        if (atk[b]) { env[b] += (1.0f / 60.0f) / 0.012f; if (env[b] >= 1.0f) { env[b] = 1.0f; atk[b] = false; } }
        else        env[b] *= dcoef;
        if (env[b] < 0.0008f) { if (env[b] != 0) { env[b] = 0; note_vol(h[b], 0); } continue; }

        float e = env[b];
        note_vol(h[b], e * 6.0f);
        // COUPLE morphs cutoff from "always open" (VCA) to "tracks the envelope" (full LPG).
        // ^1.6 so brightness falls faster than loudness — the highs die first.
        float cut = 300.0f + 7700.0f * ((1.0f - k_couple) + k_couple * powf(e, 1.6f));
        note_cutoff(h[b], (int)cut);
        note_drive(h[b], k_fold * e);                     // fold blooms on the strike, settles toward pure tone
    }

#ifdef DE_TRACE
    watch("couple", "%.2f", k_couple);
    watch("fold",   "%.2f", k_fold);
    watch("decayT", "%.2f", T);
    watch("env0",   "%.3f", env[0]);
#endif
}

static const char *NN[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("LPG", 2, 4, CLR_LIGHT_YELLOW);
    print(k_couple > 0.5f ? "LOW-PASS GATE" : (k_couple > 0.05f ? "half-coupled" : "VCA only"),
          40, 4, k_couple > 0.5f ? CLR_GREEN : CLR_MEDIUM_GREY);
    print_right("west-coast bonk", 318, 4, CLR_DARK_GREY);

    // ── the marimba bars — each glows with its envelope AND fades dark as the gate closes ──
    for (int b = 0; b < NB; b++) {
        int bx = LX + b * (BARW + BARGAP);
        int bh = 104 - b * 7;                              // lower (left) bars taller
        int by = BASE_Y - bh;
        float e = env[b];
        // colour = brightness dying with the gate: white → yellow → orange → dim wood
        int col = e > 0.6f ? CLR_WHITE : e > 0.32f ? CLR_LIGHT_YELLOW
                : e > 0.10f ? CLR_ORANGE : CLR_BROWN;   // wood at rest, glows + dulls with the gate
        bool fresh = (frame() - strk[b]) < 4;
        int sq = fresh ? 3 : 0;                            // tiny squash on the hit
        rectfill(bx, by + sq, BARW, bh - sq, col);
        rect(bx, by + sq, BARW, bh - sq, fresh ? CLR_WHITE : CLR_DARKER_GREY);
        // ring lines rising off a sounding bar (more/brighter while loud)
        if (e > 0.04f) {
            int rings = 1 + (int)(e * 4);
            for (int r = 0; r < rings; r++)
                line(bx + 4 + r * 6, by + sq, bx + 4 + r * 6, by + sq - (int)(e * 16) - 2, col);
        }
        font(FONT_SMALL);
        char k[2] = { (char)KEY[b], 0 };
        print(k, bx + BARW / 2 - 2, BASE_Y + 3, CLR_MEDIUM_GREY);
        int p = bar_pitch(b);
        print(NN[p % 12], bx + 2, by + sq + 2, e > 0.32f ? CLR_BROWNISH_BLACK : CLR_DARK_GREY);
        font(FONT_NORMAL);
    }
    line(LX, BASE_Y + 1, LX + NB * (BARW + BARGAP) - BARGAP, BASE_Y + 1, CLR_DARKER_GREY);

    // ── knobs ──
    ui_begin();
    font(FONT_SMALL);
    ui_knob(&k_decay,  40,  176, "DECAY");
    ui_knob(&k_fold,   110, 176, "FOLD");
    ui_knob(&k_couple, 180, 176, "COUPLE");
    print("A S D F G H J K", 214, 166, CLR_DARK_GREY);
    print("or tap the bars", 214, 174, CLR_DARK_GREY);
    print("COUPLE 0=VCA 1=LPG", 214, 184, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    ui_end();
}
