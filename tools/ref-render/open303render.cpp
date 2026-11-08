// Render the acid probe pattern through Robin Schmidt's Open303 (MIT) to raw f32 mono on stdout.
// The SAME pattern, on the SAME sample grid, as tools/carts/acidref.c renders through acid303.h,
// so the two WAVs line up sample for sample and drop into the same oracles.
//
//   open303render [cutoff_hz res_pct envmod_pct decay_ms accent_pct]
//
// Defaults = every 303 pot at 12 o'clock on Open303's own (measured, stock-303) ranges:
//   cutoff 314..2394 Hz exp -> 867 Hz · reso 50% · envmod 50% · decay 200..2000 ms exp -> 632 ms · accent 50%
#include "rosic_Open303.h"
#include <cstdio>
#include <cstdlib>
using namespace rosic;

// ── the pattern: keep IDENTICAL to tools/carts/acidref.c ─────────────────────────────────
static const int N[16] = { 0, 0, 12, 0, 3, 0, 7, 5, 0, 12, 10, 0, 3, 15, 0, 7 };
static const int A[16] = { 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0 };
static const int S[16] = { 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0 };   // slide INTO the next step
#define BASE 36
#define FRAME 735          // one 60 fps frame at 44.1k
#define STEP (8 * FRAME)   // a 16th = 8 frames (112.5 bpm)
#define GATE (5 * FRAME)   // gate closes 5 frames into a step
#define LONG_A 40          // plain long note starts on this step
#define LONG_B 64          // accented long note
#define LONG_LEN 15        // steps (then the usual GATE)
#define TOTAL (88 * STEP)

int main(int argc, char **argv) {
    double cut = argc > 1 ? atof(argv[1]) : 867.0, res = argc > 2 ? atof(argv[2]) : 50.0;
    double env = argc > 3 ? atof(argv[3]) : 50.0, dec = argc > 4 ? atof(argv[4]) : 632.0;
    double acc = argc > 5 ? atof(argv[5]) : 50.0;
    Open303 s;
    s.setSampleRate(44100.0);
    s.setCutoff(cut); s.setResonance(res); s.setEnvMod(env); s.setDecay(dec); s.setAccent(acc);
    s.setWaveform(0.0);                 // saw
    s.setVolume(-18.0);
    int held = -1;
    for (long i = 0; i < TOTAL; i++) {
        long st = i / STEP, sub = i % STEP;
        if (st < 32) {
            int k = (int)(st % 16), prev = (int)((st + 15) % 16);
            if (sub == 0) {
                int n = BASE + N[k];
                bool slid = st > 0 && S[prev] && held >= 0;
                s.noteOn(n, A[k] ? 127 : 80, 0.0);
                if (slid) s.noteOn(held, 0, 0.0);     // legato: new note first, then release the old one
                held = n;
            }
            if (sub == GATE && !S[k] && held >= 0) { s.noteOn(held, 0, 0.0); held = -1; }
            if (st == 31 && sub == GATE && held >= 0) { s.noteOn(held, 0, 0.0); held = -1; }
        } else if (st == LONG_A || st == LONG_B) {
            if (sub == 0) { held = BASE + 12; s.noteOn(held, st == LONG_B ? 127 : 80, 0.0); }
        } else if ((st == LONG_A + LONG_LEN || st == LONG_B + LONG_LEN) && sub == GATE && held >= 0) {
            s.noteOn(held, 0, 0.0); held = -1;
        }
        float x = (float)s.getSample();
        fwrite(&x, sizeof x, 1, stdout);
    }
    return 0;
}
