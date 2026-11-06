/* de:meta
{
  "slug": "drumhand",
  "title": "drumhand",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "drum-synthesis",
    "step-sequencer",
    "swing-timing"
  ],
  "lineage": "The drum-kit member of the humanized-playing spikes (strumhand, breathflute, tablahand, reedhand, basshand; docs/design/humanized-playing.md section 7.1). A kit adds two new kinds of rule to the needs table: a TECHNIQUE chosen per note (snare centre / edge / rimshot / cross-stick / ghost, hat tip / shank, ride bow / bell, kick buried / bounced) and a shared continuous control (the hi-hat PEDAL every hat hit reads). Voices built from INSTR_MEMBRANE (kick, snare head, rim, toms), INSTR_NOISE (snare wires) and INSTR_METAL (hats, ride, bell, crash), every voice tuned against a CC0 recording (pitch, energy band, time to -20 dB; build/refs/drums-cc0/SOURCES.txt), the cymbals lowpassed where the bogie recipes let 15-20 kHz hiss through. The groove is original.",
  "description": {
    "summary": "An 8-bar soul groove played by a DRUMMER instead of a grid: two hands and two feet, a technique per stroke, a hi-hat pedal that every hat hit reads, ghost notes, flams, a buzz and a fill that builds and lands on the crash. TAB flips to the grid version.",
    "detail": "Four limbs. The right hand keeps the hats (or the ride) on top of the beat, the left hand plays the backbeat a hair behind it and the ghost notes in between, the right foot the kick, the left foot the hi-hat PEDAL. Each stroke is a TECHNIQUE: the snare is struck in the centre, near the edge, as a rimshot (stick on head and rim at once), as a cross-stick (stick laid across the rim) or as a ghost; the hat is hit with the tip (a light tick) or the shank (a thick chunk); the ride on the bow or the bell; the kick is buried (beater held into the head, short) or bounced (it rings). The pedal is a continuous openness every hat hit reads: closed tick, half-open sizzle, open wash, and when the foot closes it makes its own chick and stops a ringing open hat. The drummer has a body: a hand cannot be on the hat and the snare at once, so the hats stop in a fill and the fill alternates right and left; a quick second stroke by the same hand is a little late and softer; the energy builds over the eight bars into the fill and the crash, and over a long take the left hand tires (lighter ghosts, a little dragging). Keys 1-5 switch each layer on and off.",
    "controls": "SPACE play/stop · TAB drummer / grid · L foot ease-down as booked LANE_LEVEL ramps / ridden per frame · 1 time 2 dynamics 3 technique 4 pedal 5 limbs · UP/DOWN humanity · LEFT/RIGHT tempo · R new take · K cymbals: recorded / MODAL ride (50 measured modes) / synth"
  },
  "todo": [
    "Synth ride: the deepest bass is still ~9 dB heavy against the recording (125 Hz octave -27 vs -36 dB re 500 Hz, measured on the groove stem).",
    "Synth ride BELL is still the old tone build (a band-passed METAL bank); give it the struck-resonator treatment the bow got (583ccae6).",
    "MODAL ride (K): the recorded ride's 400 strongest modes (measured phases) + half-octave washes + ticks, played live by INSTR_MODAL + instrument_modes on one mono voice (~2% of a core). Single hit vs the recording: 250 Hz-4 kHz within 1-4 dB over 3 s; the first 0.3 s 3-5 dB bright at 1-4 kHz, and 8 kHz rings long past ~1.5 s (+13 dB at 2.7 s, ~50 dB down: one straight decay per band, the real wash curves). By ear 2026-11-06: 50 modes better than the struck-resonator ride (Nikki), but 'the real one is richer'; 400 = the first count as rich as the recording (200 not). LISTEN to the 400-mode version. Next steps: the BELL from its own recording, and an energy cascade (the swell after a hard hit), which a linear bank cannot make."
  ]
}
de:meta */
// drumhand — a spike: what does a drum kit need to sound PLAYED, and what would a booked note
// have to carry for it? (docs/design/humanized-playing.md, section 7.1)
//
// Engine side, no new API:
//   - every stroke is booked on the audio clock (schedule_note_at, ~0.12 s lookahead)
//   - a TECHNIQUE is today a bundle of: which slots fire (a rimshot = head + wires + rim crack),
//     the note number, per-note macro OFFSETS (strike position = membrane timbre), the GATE length,
//     the GAIN and, for the noise wires, the ACCENT (routed to the filter cutoff: the only per-note
//     filter lever a booked note has)
//   - the hi-hat PEDAL is cart state; each hat hit READS it at booking time and turns it into a gate
//     length + a noise offset. The foot closing books a chick on its own slot that CHOKES the hat
//     (instrument_choke). Easing the foot down on a ringing open hat can only be done by riding the
//     hat SLOT's level per frame (instrument_level), which is the shared-control gap.
//   - L (NoteOpts step 4 acceptance): that ease-down BOOKED instead, as LANE_LEVEL ramps on the hat
//     slot, placed when the chick is booked (down over the 100 ms before it, back to 0 dB on it).
//
// The DRUMMER (four limbs, a phrase, a body) lives entirely in this cart.

#include "studio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// ---- slots ----
#define S_KICK   10
#define S_HEAD   11     // snare head (membrane)
#define S_WIRE   12     // snare wires (noise)
#define S_RIM    13     // rim: the cross-stick tock and the rimshot crack
#define S_HAT    14     // hi-hat, mono (one pair of cymbals)
#define S_CHICK  15     // the foot closing the hat; chokes S_HAT
#define S_RIDE   16
#define S_CRASH  17
#define S_TOMHI  18
#define S_TOMLO  19
#define S_BELL   26     // the ride's BELL: its own slot, since it wants the opposite filter to the bow
#define S_RIDESH 29     // the ride's shimmer: a METAL bank band-passed to 2.6 kHz (the recording's 2-4 kHz plateau)
#define RIDE_TANK 1     // the ride's private reverb tank: the RESONATOR both ride voices are struck into
// RECORDED cymbals (2026-11-04, Nikki: "an awful cymbal and ride sound"): CC0 one-shots loaded at
// runtime from build/refs/drums-cc0/*.f32 (the binary runs with cwd = build/), the breakchop route.
// Missing files = the synthesized cymbals, so the cart never breaks. K switches kits.
#define R_RIDE   20
#define R_BELL   21
#define R_CRASH  22
#define R_HATC   23
#define R_HATS   24
#define R_HATO   25
#define NREC     6
static const char *REC_FILE[NREC] = { "refs/drums-cc0/ride.f32", "refs/drums-cc0/bell.f32", "refs/drums-cc0/crash.f32",
                                      "refs/drums-cc0/hatc.f32", "refs/drums-cc0/hats.f32", "refs/drums-cc0/hato.f32" };
static const float REC_LVL[NREC] = { 0.16f, 0.20f, 0.18f, 0.18f, 0.18f, 0.17f };   // hats -5 dB: oneshot-compare vs the same real kit (2026-11-04)   // under the +14 dB master EQ
static bool rec_ok = false;        // all six loaded
enum { KIT_REC, KIT_MODAL, KIT_SYNTH, NKIT };
static int kit = KIT_REC;          // K cycles: recorded cymbals / recorded + the MODAL ride bow / all synth
#define rec_kit (kit != KIT_SYNTH)  // the recorded cymbals play in both REC and MODAL
#define LOOKAHEAD 0.12
#define HAT_LVL   0.094f    // -8 dB: oneshot-compare had the synth hats 8-11 dB hot vs the real kit

// ---- the written groove: 8 bars, 16 steps each ----
// kick : K bounced, B buried, k soft
// snare: S centre, E edge, R rimshot, X cross-stick, g ghost, f flam, d drag, z buzz
// hat  : X shank (accent), x tip; the PEDAL line decides how open it is
// ride : r bow, b bell      tom: T high, t floor      crash: C
// foot : - closed, ~ half, O open, c chick (foot strikes closed), . hold
typedef struct { const char *kick, *snare, *hat, *ride, *tom, *crash, *foot; int fill_from; } Bar;
static const Bar BARS[8] = {
    { "K.....K.B.K.....", "....S..g.g..S..g", "X.x.X.x.X.x.X.x.", "................", "................", "C...............", "----------------", 16 },
    { "K.....K.B.K...k.", "....S..g.g..S..g", "X.x.X.x.X.x.X.x.", "................", "................", "................", "--------------O-", 16 },
    { "K.....K.B.K.....", "....S..g.g..S.gg", "X.x.X.x.X.x.X.x.", "................", "................", "................", "c-----------~~~~", 16 },
    { "K.....K.B.K....K", "....S..g.g..f.dE", "X.x.X.x.X.x.X.x.", "................", "................", "................", "c---------------", 12 },
    { "K.....K.B.K.....", "....X..g.g..X..g", "................", "r.r.r.b.r.r.r.b.", "................", "C...............", "....c.......c...", 16 },
    { "K..k..K.B.K.....", "....X..g.g..X..g", "................", "r.r.r.b.r.r.r.b.", "................", "................", "....c.......c...", 16 },
    { "K.....K.B.K...k.", "....R..g.g..R.gg", "................", "r.r.r.b.r.r.r.b.", "................", "................", "....c.......c...", 16 },
    { "K.......K...K...", "gSgSgSSz........", "................", "................", "........TTTTtttt", "................", "c...c...c...c...", 0 },
};

enum { L_TIME, L_DYN, L_TECH, L_PEDAL, L_LIMBS, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "time", "dyn", "tech", "pedal", "limbs" };

static bool  playing = true, human = true;
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 94;
static int   seed = 1;

static double t_step;          // audio time of the next 16th (nominal)
static int    step, cur_bar, cycle;
static double take_start;

// ---- the drummer's state ----
enum { RH, LH, RF, LF, NLIMB };
static const char *LIMB_NAME[NLIMB] = { "R hand", "L hand", "R foot", "L foot" };
static double last_hit[NLIMB];
static double rh_away_until;   // the right hand is off the hat (in a fill) until then
static int    sticking;        // fill strokes alternate R / L
static float  pedal = 0;       // hi-hat openness 0..1, what the foot is doing right now
static float  energy = 0.5f, fatigue = 0;
static float  snare_spot = 0.15f;   // where the left stick lands, random walk (0 centre .. 1 edge)
static float  hat_spot = 0;         // the right stick drifts between tip and shoulder

// foot closing events, for the live ease-down of a ringing open hat
#define NCLOSE 8
static double close_t[NCLOSE];
static int    nclose;
static float  hat_level_last = -1;

// display
#define NDOT 96
typedef struct { double t; int cur_bar, step, limb, kind; float gain, dev; char sym; } Dot;
static Dot dots[NDOT];
static int ndot;
static float shown_pedal;
static bool  lane_mode = false;   // L: the foot's ease-down as booked LANE_LEVEL ramps
#define EASE_S   0.10             // the foot eases down over the last 100 ms before the chick
#define EASE_DB  (-6.94f)         // to 0.45 of the level (the ridden path's 1 - 0.55)
enum { D_KICK, D_SNARE, D_HAT, D_RIDE, D_TOM, D_CRASH, D_CHICK, ND };
static const char *DK_NAME[ND] = { "kick", "snare", "hat", "ride", "toms", "crash", "foot" };
static const int   DK_COL[ND]  = { CLR_ORANGE, CLR_WHITE, CLR_YELLOW, CLR_PEACH, CLR_PINK, CLR_GREEN, CLR_BLUE };

static float r1(void) { return rnd_float() * 2.0f - 1.0f; }
static float drift(double x, float k, float off) { return noise((float)x * k + seed * 13.1f + off) * 2.0f - 1.0f; }
static float smooth01(float x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return x * x * (3 - 2 * x); }

static bool L(int i) { return human && layer[i]; }

static void setup_sound(void) {
    // kick: a low membrane, centre strike, a small pitch drop off the beater
    instrument(S_KICK, INSTR_MEMBRANE, 0, 0, 7, 70);
    instrument_harmonics(S_KICK, 0.80f); instrument_timbre(S_KICK, 0.12f); instrument_morph(S_KICK, 0.30f);
    instrument_filter(S_KICK, FILTER_LOW, 6000, 1);
    // snare head: a tighter membrane, inharmonic, the gate shapes its ring
    instrument(S_HEAD, INSTR_MEMBRANE, 0, 0, 7, 60);
    instrument_harmonics(S_HEAD, 0.70f); instrument_timbre(S_HEAD, 0.15f); instrument_morph(S_HEAD, 0.10f);
    // snare wires: broad band noise; accent opens the band (per-note brightness)
    instrument(S_WIRE, INSTR_NOISE, 0, 170, 0, 50);
    instrument_filter(S_WIRE, FILTER_LOW, 7000, 2);   // was a band at 2.2 kHz whose skirt hissed to 12 kHz
    instrument_accent(S_WIRE, ACC_CUTOFF, 1.0f);
    instrument_accent(S_WIRE, ACC_VOL, 0.0f);
    // rim: a small hard inharmonic body, struck at the edge = wood/metal click
    instrument(S_RIM, INSTR_MEMBRANE, 0, 0, 7, 30);
    instrument_harmonics(S_RIM, 1.0f); instrument_timbre(S_RIM, 0.75f); instrument_morph(S_RIM, 0.0f);
    instrument_filter(S_RIM, FILTER_HIGH, 400, 1);
    // CYMBALS: every one voiced against a CC0 recording (build/refs/drums-cc0, SOURCES.txt) by its
    // energy band (10/50/90%) and its time to -20 dB. The METAL bank's mode 0 sits at f0 x 1.18, so the
    // NOTE places the cymbal; its built-in bright noise lives up at 15-20 kHz, so every cymbal is
    // LOWPASSED (the old high-passes let that hiss through: the crash measured 3-20 kHz, flatness 0.82).
    // hi-hat: the bank played high (tip note 81 / shank 84), lowpassed at 12.5 kHz. Real tip 2.7/8.2/
    // 14.4 kHz, ours 2.5/7.1/15.4. The GATE (from the pedal) ends it
    instrument(S_HAT, INSTR_METAL, 0, 0, 7, 60);
    instrument_harmonics(S_HAT, 0.70f); instrument_timbre(S_HAT, 0.85f); instrument_morph(S_HAT, 0.85f);
    instrument_mode(S_HAT, MODE_METAL_DECAY, 0.66f);
    instrument_mode(S_HAT, MODE_METAL_SPREAD, 0.20f);
    instrument_env(S_HAT, 0, ENV_PITCH, 0, 80, 1.4f);
    instrument_filter(S_HAT, FILTER_LOW, 12500, 4);
    instrument_mono(S_HAT, 1);
    // the chick: the two cymbals clapped shut by the foot, short and darker
    instrument(S_CHICK, INSTR_METAL, 0, 0, 7, 25);
    instrument_harmonics(S_CHICK, 0.30f); instrument_timbre(S_CHICK, 0.30f); instrument_morph(S_CHICK, 0.5f);
    instrument_mode(S_CHICK, MODE_METAL_DECAY, 0.10f);
    instrument_filter(S_CHICK, FILTER_BAND, 2400, 1);
    instrument_choke(S_CHICK, S_HAT);
    // RIDE, aimed at the RECORDED kit's ride (build/refs/drums-cc0/ride.wav), 2026-11-04/05, picked by ear
    // from level-matched renders. The recording: "you hear something at the start, then just a beautiful
    // noise" (Nikki), a broad but BROWN noise packed into the octave around 500 Hz, a soft plateau at 2-4 kHz,
    // almost no low end, and each stroke dropping ~10 dB before the next. Built from TONES (METAL banks,
    // a MODAL body) it stayed "in a certain pitch" whatever was tuned; so the ride is now STRUCK INTO A
    // RESONATOR: a short dark noise stroke and a short shimmer both sent fully into a private reverb tank
    // (RIDE_TANK, set up at the end of init), whose dense resonances make the wash. No fixed pitch anywhere.
    //   S_RIDE    the stroke: noise band-passed at 500 Hz (Q 14), a fast drop then a quiet ~1 s tail. The fast
    //             drop is what keeps strokes apart: a 350 ms swell piled the tails into a hiss bed (6.5 dB
    //             between strokes vs the recording's 10)
    //   S_RIDESH  the shimmer, a METAL bank band-passed at 2.6 kHz, quiet and a little long (a short loud one
    //             was spiky)
    // Measured on the ride stem in the groove (octave bands re 500 Hz, recording in brackets): 250 -14 (-10),
    // 1k -9 (-10), 2k -10 (-8), 4k -9 (-8), 8k -19 (-20); 10.2 dB between strokes (10.1); level-matched.
    // Still off: the deepest bass, 125 Hz at -27 vs -36.
    // History: a bright "ping" ride (2026-11-04 morning, "ringy pingy metallic plonky"), then tone banks +
    // a noise wash ("tones to it", "too much hiss noise" from the overlapping tails).
    instrument(S_RIDE, INSTR_NOISE, 0, 90, 1, 900);
    instrument_filter(S_RIDE, FILTER_BAND, 500, 14);
    instrument_mono(S_RIDE, 1);
    instrument(S_RIDESH, INSTR_METAL, 0, 60, 5, 700);
    instrument_harmonics(S_RIDESH, 1.0f); instrument_timbre(S_RIDESH, 0.10f); instrument_morph(S_RIDESH, 0.5f);
    instrument_mode(S_RIDESH, MODE_METAL_DECAY, 0.55f); instrument_mode(S_RIDESH, MODE_METAL_SPREAD, 0.0f);
    instrument_filter(S_RIDESH, FILTER_BAND, 2600, 0); instrument_mono(S_RIDESH, 1); instrument_pan(S_RIDESH, 0.38f);
    instrument_level(S_RIDESH, 0.198f);
    // ride bell, aimed at the bell Nikki picked (Meinl Byzance 20" "B2", CC0): a clear, nearly pure
    // ding, partials 3036 / 3435 / 3790 Hz, -20 dB in 345 ms. The bank in a tight spread through a
    // narrow band at 3.4 kHz: ours 3047 / 4016 Hz, -20 dB in 340 ms
    instrument(S_BELL, INSTR_METAL, 0, 0, 7, 300);
    instrument_harmonics(S_BELL, 0.60f); instrument_timbre(S_BELL, 0.05f); instrument_morph(S_BELL, 0.30f);
    instrument_mode(S_BELL, MODE_METAL_DECAY, 0.42f);
    instrument_mode(S_BELL, MODE_METAL_SPREAD, 0.0f);
    instrument_filter(S_BELL, FILTER_BAND, 3400, 10);
    // crash: its partials cluster at 3.5-5.5 kHz (an 18" crash: 4145 / 5469 Hz), so the bank sits HIGH
    // (note 100), little noise, lowpassed at 8.5 kHz, -20 dB in about a second
    instrument(S_CRASH, INSTR_METAL, 0, 0, 7, 400);
    instrument_harmonics(S_CRASH, 0.55f); instrument_timbre(S_CRASH, 0.30f); instrument_morph(S_CRASH, 0.30f);
    instrument_mode(S_CRASH, MODE_METAL_DECAY, 0.68f);
    instrument_mode(S_CRASH, MODE_METAL_SPREAD, 0.15f);
    instrument_filter(S_CRASH, FILTER_LOW, 8500, 4);
    // toms: tuned heads (rack 129 Hz -20 dB in 140 ms; floor ~90 Hz), short; see tom_stroke for the 2nd mode
    instrument(S_TOMHI, INSTR_MEMBRANE, 0, 0, 7, 60);
    instrument_harmonics(S_TOMHI, 0.30f); instrument_timbre(S_TOMHI, 0.55f); instrument_morph(S_TOMHI, 0.15f);
    instrument(S_TOMLO, INSTR_MEMBRANE, 0, 0, 7, 80);
    instrument_harmonics(S_TOMLO, 0.30f); instrument_timbre(S_TOMLO, 0.50f); instrument_morph(S_TOMLO, 0.15f);

    // the room and the drummer's-view stereo picture
    instrument_pan(S_HAT, -0.35f); instrument_pan(S_CHICK, -0.35f);
    instrument_pan(S_RIDE, 0.38f); instrument_pan(S_BELL, 0.38f); instrument_pan(S_CRASH, -0.45f);
    instrument_pan(S_TOMHI, -0.15f); instrument_pan(S_TOMLO, 0.30f);
    instrument_pan(S_RIM, 0.05f);
    instrument_level(S_KICK, 0.9f);    // the 118 Hz second-mode voice made it fuller; keep the peak off 0 dBFS
    instrument_level(S_HEAD, 1.0f);    // up 2.5 dB for the RIMSHOT; centre/edge/ghost/x-stick gains come down instead
    instrument_level(S_WIRE, 0.25f);   // the real snare's energy is mostly its 205 Hz tone (raised with the head, gains lowered)
    instrument_level(S_RIM, 0.80f);
    instrument_level(S_CHICK, 0.25f);
    instrument_level(S_HAT, HAT_LVL);
    instrument_level(S_RIDE, 0.25f);    // the ride, level-matched to the recorded ride in the groove
    instrument_level(S_BELL, 0.15f);
    instrument_level(S_CRASH, 0.25f);
    instrument_level(S_TOMHI, 1.0f); instrument_level(S_TOMLO, 0.9f);
    int s[] = { S_KICK, S_HEAD, S_WIRE, S_RIM, S_HAT, S_CHICK, S_RIDE, S_CRASH, S_TOMHI, S_TOMLO, S_BELL };
    float send[] = { 0.06f, 0.18f, 0.18f, 0.12f, 0.08f, 0.06f, 0.12f, 0.15f, 0.14f, 0.14f, 0.12f };
    for (int i = 0; i < 11; i++) instrument_reverb(s[i], send[i]);
    reverb(0.28f, 0.45f);
    eq(14.0f, 10.0f, 5.0f);     // no master volume: the master EQ is the fader (membranes are quiet)
    // the ride's resonator (see RIDE above). AFTER the send loop: instrument_reverb() would re-route the slots
    reverb_bus(RIDE_TANK, 0.97f, 0.80f);
    reverb_bus_fx(RIDE_TANK, FX_EQ, -12.0f, -6.0f, -12.0f);     // brown: lows and highs out of the tail
    instrument_reverb_bus(S_RIDE, RIDE_TANK, 1.0f);
    instrument_reverb_bus(S_RIDESH, RIDE_TANK, 1.0f);
}

static bool load_f32(const char *path, int sample_slot) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f) / 4; fseek(f, 0, SEEK_SET);
    if (n < 64) { fclose(f); return false; }
    float *buf = (float *)malloc((size_t)n * sizeof(float));
    if (!buf) { fclose(f); return false; }
    size_t got = fread(buf, sizeof(float), (size_t)n, f);
    fclose(f);
    float pk = 0; for (size_t i = 0; i < got; i++) pk = fmaxf(pk, fabsf(buf[i]));
    if (pk > 0.0001f) for (size_t i = 0; i < got; i++) buf[i] *= 0.9f / pk;
    sample_load(sample_slot, buf, (int)got);
    free(buf);
    return got >= 64;
}

static void setup_recorded(void) {
    rec_ok = true;
    for (int i = 0; i < NREC; i++) if (!load_f32(REC_FILE[i], i)) { rec_ok = false; break; }
    if (!rec_ok) return;
    for (int i = 0; i < NREC; i++) {
        int sl = R_RIDE + i;
        bool hat = sl >= R_HATC;
        instrument(sl, INSTR_SAMPLE, 0, 0, 7, hat ? 35 : 120);
        instrument_sample(sl, i, 60);                // played at note 60 = as recorded
        instrument_level(sl, REC_LVL[i]);
        instrument_reverb(sl, hat ? 0.06f : 0.12f);
        instrument_pan(sl, hat ? -0.35f : sl == R_CRASH ? -0.45f : 0.38f);
    }
    // one pair of hats: any new hat sound stops the one ringing, and the foot closing stops them all
    for (int a = R_HATC; a <= R_HATO; a++) {
        for (int b = R_HATC; b <= R_HATO; b++) if (a != b) instrument_choke(a, b);
        instrument_choke(S_CHICK, a);
        instrument_mono(a, 1);
    }
}

// ---- MODAL RIDE (2026-11-06): the recorded ride (build/refs/drums-cc0/ride.wav) taken apart by sub-band
// ESPRIT into ~2000 decaying modes. Played LIVE by INSTR_MODAL + instrument_modes (setup_engine_ride below):
// the 400 strongest, each from its MEASURED starting phase, + decaying half-octave NOISE washes standing in for
// the ~1600 weaker ones + 6 ms stick TICKs where the modes leave one, all MEASURED off the recording. Single
// hit vs the recorded one through the same chain: within ~1-4 dB per octave over the first 3 s, 2 kHz swell
// included. (Before: 50 modes 0bd38416/3600f394, the first count that sounded like a cymbal; pre-rendered
// variants before that.)
// The table: `python3 -I tools/mode-extract.py build/refs/drums-cc0/ride.wav --top 400` (the 2026-11-06 full
// run's modes.json via --from). 400 by ear (Nikki: 50 is the first count that sounds like a cymbal, 400 the
// first as RICH as the recording; 200 not). The noise rows are what those 400 leave, refitted by the tool.
static const float RIDE_MODES[400][4] = {   // Hz, amplitude at the strike (re full scale), decay dB/s, starting phase (turns)
    {203.19f,0.002579f,7.0f,0.460f}, {267.01f,0.006073f,12.9f,0.711f}, {271.47f,0.002003f,0.7f,0.187f}, {271.48f,0.020841f,26.3f,0.872f},
    {281.39f,0.018577f,30.4f,0.323f}, {299.89f,0.012944f,13.2f,0.052f}, {302.29f,0.016145f,7.0f,0.096f}, {310.78f,0.002331f,3.3f,0.282f},
    {311.83f,0.008567f,7.9f,0.199f}, {312.64f,0.002794f,4.5f,0.975f}, {313.25f,0.009287f,11.7f,0.366f}, {317.52f,0.003975f,7.2f,0.264f},
    {326.98f,0.019091f,35.0f,0.101f}, {359.58f,0.012352f,23.6f,0.980f}, {393.13f,0.009735f,11.1f,0.246f}, {394.51f,0.013168f,14.2f,0.409f},
    {396.37f,0.036118f,61.8f,0.085f}, {400.15f,0.019246f,18.8f,0.987f}, {402.13f,0.001997f,3.4f,0.340f}, {403.47f,0.044503f,18.7f,0.967f},
    {404.62f,0.011643f,9.2f,0.263f}, {407.30f,0.014438f,15.7f,0.100f}, {408.71f,0.013877f,6.6f,0.293f}, {409.22f,0.020245f,9.7f,0.542f},
    {410.78f,0.047421f,24.7f,0.477f}, {412.45f,0.102211f,61.4f,0.473f}, {416.92f,0.007008f,6.6f,0.061f}, {417.62f,0.108029f,41.5f,0.557f},
    {419.08f,0.024084f,8.9f,0.355f}, {420.03f,0.066433f,20.8f,0.593f}, {421.00f,0.004557f,4.7f,0.482f}, {421.54f,0.009215f,6.0f,0.590f},
    {422.09f,0.015217f,7.2f,0.729f}, {422.86f,0.035567f,17.7f,0.855f}, {426.96f,0.018633f,31.2f,0.368f}, {430.49f,0.029429f,34.0f,0.001f},
    {434.39f,0.006132f,9.8f,0.266f}, {493.64f,0.058680f,8.4f,0.500f}, {493.73f,0.115340f,24.3f,0.989f}, {501.88f,0.021466f,23.5f,0.364f},
    {507.15f,0.020745f,18.2f,0.839f}, {509.62f,0.002238f,3.8f,0.636f}, {511.35f,0.009243f,9.9f,0.817f}, {518.72f,0.002298f,3.0f,0.902f},
    {521.20f,0.011073f,13.1f,0.717f}, {524.66f,0.045398f,76.4f,0.013f}, {526.48f,0.021971f,18.2f,0.529f}, {531.23f,0.006895f,7.3f,0.315f},
    {544.27f,0.040076f,19.4f,0.550f}, {545.16f,0.068541f,36.7f,0.964f}, {547.95f,0.014419f,13.1f,0.980f}, {552.20f,0.006478f,7.2f,0.654f},
    {564.13f,0.010069f,12.3f,0.579f}, {568.64f,0.007499f,14.1f,0.891f}, {569.95f,0.002323f,3.2f,0.050f}, {578.62f,0.005522f,2.0f,0.327f},
    {579.03f,0.017926f,9.5f,0.664f}, {591.70f,0.002631f,6.3f,0.351f}, {594.83f,0.002903f,8.1f,0.515f}, {612.14f,0.007640f,20.3f,0.844f},
    {627.29f,0.007328f,14.9f,0.587f}, {628.38f,0.006877f,6.2f,0.868f}, {653.42f,0.004319f,24.0f,0.453f}, {664.56f,0.002219f,0.6f,0.677f},
    {664.81f,0.007209f,4.5f,0.991f}, {665.62f,0.022248f,38.4f,0.343f}, {669.00f,0.020433f,33.6f,0.377f}, {692.86f,0.021520f,106.4f,0.253f},
    {715.24f,0.010549f,26.3f,0.319f}, {716.49f,0.002122f,4.7f,0.580f}, {717.25f,0.009056f,12.5f,0.758f}, {729.86f,0.003657f,12.9f,0.071f},
    {751.54f,0.022679f,97.8f,0.799f}, {763.76f,0.006629f,13.7f,0.392f}, {782.65f,0.002480f,17.3f,0.724f}, {793.96f,0.003031f,8.4f,0.206f},
    {794.24f,0.002284f,5.5f,0.385f}, {808.85f,0.004995f,18.9f,0.296f}, {809.47f,0.003198f,5.4f,0.692f}, {813.40f,0.001532f,5.4f,0.113f},
    {853.76f,0.005237f,24.1f,0.040f}, {856.57f,0.004816f,36.4f,0.147f}, {860.72f,0.006527f,13.7f,0.180f}, {862.13f,0.008479f,26.5f,0.567f},
    {863.67f,0.002039f,10.6f,0.606f}, {865.65f,0.002355f,9.7f,0.835f}, {868.19f,0.002792f,10.5f,0.628f}, {904.22f,0.003436f,16.8f,0.946f},
    {905.65f,0.005156f,21.2f,0.244f}, {910.05f,0.005433f,32.4f,0.102f}, {914.94f,0.015769f,62.4f,0.191f}, {919.83f,0.003218f,13.2f,0.375f},
    {923.59f,0.002576f,16.2f,0.381f}, {991.31f,0.003418f,8.7f,0.151f}, {1036.91f,0.001239f,4.4f,0.975f}, {1061.05f,0.002041f,8.7f,0.938f},
    {1073.51f,0.002740f,18.7f,0.036f}, {1083.92f,0.001230f,2.9f,0.679f}, {1084.35f,0.003279f,8.1f,0.019f}, {1155.37f,0.001386f,5.1f,0.984f},
    {1156.06f,0.002428f,16.9f,0.422f}, {1177.36f,0.013928f,32.1f,0.038f}, {1181.01f,0.002983f,6.1f,0.050f}, {1181.31f,0.006225f,9.1f,0.334f},
    {1181.87f,0.002078f,8.5f,0.644f}, {1183.05f,0.013439f,40.9f,0.430f}, {1200.65f,0.005187f,21.7f,0.738f}, {1201.82f,0.000741f,1.7f,0.608f},
    {1202.26f,0.004865f,21.2f,1.000f}, {1204.75f,0.001249f,4.5f,0.908f}, {1215.58f,0.000773f,1.7f,0.980f}, {1221.13f,0.002702f,8.8f,0.087f},
    {1288.86f,0.003431f,5.1f,0.710f}, {1297.40f,0.004191f,10.3f,0.213f}, {1301.02f,0.006485f,9.6f,0.856f}, {1303.55f,0.005351f,5.4f,0.856f},
    {1304.60f,0.003521f,6.2f,0.362f}, {1305.83f,0.015336f,9.7f,0.903f}, {1306.26f,0.002230f,5.1f,0.292f}, {1307.28f,0.013861f,36.2f,0.351f},
    {1317.86f,0.001917f,2.2f,0.374f}, {1318.22f,0.003170f,6.1f,0.769f}, {1330.68f,0.003583f,7.5f,0.027f}, {1331.09f,0.009607f,17.2f,0.245f},
    {1333.14f,0.002353f,2.7f,0.876f}, {1335.58f,0.018263f,67.4f,0.472f}, {1338.29f,0.000697f,0.2f,0.916f}, {1338.51f,0.001085f,1.1f,0.261f},
    {1340.97f,0.004376f,10.8f,0.609f}, {1341.58f,0.003482f,2.7f,0.827f}, {1344.73f,0.004992f,5.8f,0.816f}, {1349.26f,0.010252f,10.6f,0.603f},
    {1351.94f,0.005537f,3.5f,0.408f}, {1352.46f,0.003150f,5.4f,0.350f}, {1352.91f,0.003487f,3.6f,0.691f}, {1354.45f,0.002577f,4.5f,0.712f},
    {1354.84f,0.008695f,8.8f,0.916f}, {1382.28f,0.003609f,7.0f,0.998f}, {1383.09f,0.003150f,6.1f,0.281f}, {1398.75f,0.001707f,3.3f,0.541f},
    {1399.07f,0.003897f,7.9f,0.009f}, {1402.73f,0.001913f,6.0f,0.790f}, {1450.74f,0.006689f,37.4f,0.410f}, {1452.49f,0.000778f,1.2f,0.559f},
    {1452.75f,0.002354f,5.0f,0.883f}, {1454.43f,0.002271f,12.4f,0.071f}, {1456.46f,0.007023f,14.6f,0.146f}, {1457.37f,0.002571f,5.4f,0.360f},
    {1460.06f,0.009589f,62.2f,0.221f}, {1465.56f,0.003267f,2.4f,0.122f}, {1465.76f,0.008017f,6.2f,0.498f}, {1467.51f,0.018344f,46.8f,0.345f},
    {1468.41f,0.006807f,25.0f,0.719f}, {1476.86f,0.001859f,2.8f,0.196f}, {1477.18f,0.002254f,4.6f,0.519f}, {1479.27f,0.009191f,32.0f,0.308f},
    {1482.75f,0.001479f,5.9f,0.466f}, {1495.41f,0.001671f,2.6f,0.469f}, {1495.80f,0.002378f,8.9f,0.060f}, {1496.42f,0.001898f,6.2f,0.582f},
    {1499.70f,0.003699f,7.9f,0.942f}, {1500.58f,0.001464f,6.0f,0.760f}, {1501.85f,0.005257f,20.7f,0.229f}, {1741.54f,0.002029f,9.0f,0.559f},
    {1742.27f,0.002230f,14.7f,0.867f}, {1743.02f,0.002616f,10.8f,0.136f}, {1773.13f,0.002837f,18.8f,0.839f}, {1799.60f,0.002845f,7.0f,0.888f},
    {1800.17f,0.003363f,11.9f,0.343f}, {1838.73f,0.001959f,5.3f,0.800f}, {1839.24f,0.002656f,8.5f,0.803f}, {1840.19f,0.002546f,15.7f,0.524f},
    {1845.08f,0.005774f,19.1f,0.463f}, {1846.78f,0.004023f,4.7f,0.801f}, {1847.00f,0.000862f,1.6f,0.358f}, {1847.76f,0.007805f,9.6f,0.840f},
    {1849.39f,0.002488f,7.2f,0.116f}, {1882.45f,0.000907f,2.3f,0.431f}, {1894.02f,0.003333f,6.4f,0.858f}, {1894.87f,0.005046f,5.0f,0.083f},
    {1895.68f,0.004169f,10.6f,0.367f}, {1900.47f,0.004467f,15.5f,0.770f}, {1901.26f,0.002770f,4.2f,0.967f}, {1901.87f,0.003295f,7.1f,0.138f},
    {1924.16f,0.001909f,7.7f,0.425f}, {1924.97f,0.005479f,9.8f,0.822f}, {1925.58f,0.002293f,3.1f,0.590f}, {1925.80f,0.008150f,5.6f,0.813f},
    {1926.21f,0.018254f,12.5f,0.171f}, {1927.44f,0.010182f,17.6f,0.442f}, {1933.08f,0.002620f,8.6f,0.355f}, {1933.63f,0.003320f,3.6f,0.657f},
    {1942.42f,0.002423f,5.6f,0.849f}, {1942.93f,0.004059f,5.9f,0.182f}, {1943.61f,0.002612f,7.8f,0.511f}, {1945.53f,0.004917f,7.0f,0.634f},
    {1946.38f,0.004840f,6.1f,0.788f}, {1946.90f,0.005019f,8.2f,0.164f}, {2087.79f,0.003174f,6.4f,0.727f}, {2088.28f,0.004732f,16.8f,0.076f},
    {2102.87f,0.001375f,4.2f,0.754f}, {2103.08f,0.002464f,9.4f,0.171f}, {2107.06f,0.001875f,7.5f,0.654f}, {2217.01f,0.001348f,3.7f,0.763f},
    {2217.38f,0.002579f,8.5f,0.269f}, {2334.36f,0.003683f,11.4f,0.272f}, {2335.43f,0.053583f,14.4f,0.507f}, {2335.62f,0.057691f,15.6f,0.960f},
    {2336.49f,0.004528f,9.2f,0.163f}, {2338.22f,0.002714f,8.1f,0.116f}, {2340.75f,0.009353f,36.8f,0.934f}, {2341.26f,0.005907f,9.7f,0.231f},
    {2342.01f,0.004619f,13.2f,0.536f}, {2344.92f,0.001577f,6.8f,0.448f}, {2346.31f,0.002765f,7.9f,0.963f}, {2347.33f,0.014172f,18.4f,0.962f},
    {2348.22f,0.001097f,3.2f,0.117f}, {2348.69f,0.001857f,6.3f,0.109f}, {2349.56f,0.005083f,13.5f,0.220f}, {2351.87f,0.011235f,35.9f,0.302f},
    {2370.13f,0.003907f,14.1f,0.068f}, {2371.40f,0.004643f,12.3f,0.320f}, {2373.27f,0.003840f,17.9f,0.554f}, {2381.78f,0.005745f,8.7f,0.029f},
    {2382.21f,0.008753f,11.1f,0.445f}, {2387.09f,0.001951f,7.6f,0.289f}, {2387.86f,0.001910f,3.0f,0.670f}, {2518.58f,0.019376f,17.2f,0.863f},
    {2519.54f,0.036684f,22.3f,0.180f}, {2520.22f,0.018490f,11.4f,0.628f}, {2533.55f,0.004903f,11.9f,0.594f}, {2534.51f,0.005187f,7.1f,0.219f},
    {2535.10f,0.006789f,6.0f,0.921f}, {2535.41f,0.003208f,4.3f,0.199f}, {2536.09f,0.006674f,7.3f,0.394f}, {2539.53f,0.003398f,12.2f,0.786f},
    {2544.82f,0.005231f,5.2f,0.543f}, {2545.64f,0.003145f,7.1f,0.604f}, {2548.56f,0.001850f,3.7f,0.186f}, {2548.85f,0.004720f,4.4f,0.440f},
    {2549.35f,0.007854f,9.1f,0.747f}, {2552.10f,0.004457f,16.3f,0.626f}, {2553.40f,0.006150f,6.0f,0.682f}, {2553.82f,0.002411f,4.9f,0.019f},
    {2555.83f,0.002920f,7.0f,0.737f}, {2556.79f,0.004851f,4.6f,0.164f}, {2557.01f,0.005558f,4.7f,0.909f}, {2557.66f,0.001498f,3.8f,0.456f},
    {2559.94f,0.003009f,9.8f,0.586f}, {2568.41f,0.005552f,5.2f,0.422f}, {2568.76f,0.006654f,7.9f,0.915f}, {2571.22f,0.005201f,6.5f,0.251f},
    {2571.56f,0.005032f,8.5f,0.685f}, {2587.76f,0.005084f,5.5f,0.073f}, {2591.41f,0.002849f,13.0f,0.232f}, {2598.70f,0.007956f,12.1f,0.811f},
    {2598.92f,0.009257f,6.7f,0.253f}, {2686.04f,0.001449f,6.1f,0.615f}, {2686.85f,0.002213f,6.2f,0.353f}, {2687.14f,0.004931f,5.0f,0.671f},
    {2687.64f,0.004301f,15.0f,0.990f}, {2692.96f,0.003254f,9.5f,0.811f}, {2693.42f,0.001309f,3.3f,0.036f}, {2785.09f,0.001010f,2.7f,0.873f},
    {2797.71f,0.001341f,5.3f,0.651f}, {2798.24f,0.001972f,10.0f,0.817f}, {2805.97f,0.002568f,4.1f,0.557f}, {2806.24f,0.005135f,7.6f,0.934f},
    {2815.73f,0.006495f,7.0f,0.881f}, {2816.11f,0.007439f,11.2f,0.290f}, {2816.95f,0.002572f,13.1f,0.430f}, {2826.78f,0.001498f,5.9f,0.384f},
    {2932.62f,0.013343f,21.4f,0.854f}, {2934.09f,0.021776f,43.2f,0.296f}, {2935.64f,0.006153f,5.9f,0.561f}, {2936.06f,0.007042f,9.2f,0.934f},
    {2937.44f,0.003226f,10.1f,0.298f}, {2938.61f,0.002335f,13.7f,0.385f}, {2987.04f,0.001645f,2.6f,0.515f}, {2997.11f,0.004759f,25.1f,0.347f},
    {2997.45f,0.003488f,8.7f,0.764f}, {3016.17f,0.002955f,6.1f,0.306f}, {3042.94f,0.001417f,3.5f,0.951f}, {3046.58f,0.003735f,4.0f,0.168f},
    {3047.13f,0.001948f,4.6f,0.074f}, {3050.33f,0.005166f,14.6f,0.906f}, {3051.80f,0.004999f,10.9f,0.949f}, {3052.90f,0.005308f,6.9f,0.041f},
    {3053.90f,0.021724f,32.9f,0.275f}, {3055.14f,0.004846f,11.0f,0.601f}, {3056.67f,0.003848f,10.4f,0.269f}, {3057.38f,0.004781f,6.9f,0.430f},
    {3057.89f,0.009887f,32.4f,0.665f}, {3066.54f,0.003892f,12.7f,0.299f}, {3091.48f,0.000902f,2.1f,0.363f}, {3098.95f,0.002514f,6.5f,0.086f},
    {3110.13f,0.001168f,4.1f,0.395f}, {3230.61f,0.001748f,6.5f,0.017f}, {3237.97f,0.002621f,7.2f,0.793f}, {3239.70f,0.002425f,5.0f,0.021f},
    {3243.39f,0.009906f,69.9f,0.753f}, {3248.86f,0.002885f,4.9f,0.797f}, {3251.98f,0.002194f,10.9f,0.839f}, {3254.54f,0.001315f,4.4f,0.570f},
    {3256.93f,0.001891f,4.6f,0.182f}, {3257.73f,0.015013f,13.2f,0.402f}, {3258.52f,0.000929f,2.6f,0.980f}, {3258.73f,0.007729f,9.9f,0.725f},
    {3260.20f,0.006848f,13.5f,0.667f}, {3261.08f,0.007513f,7.8f,0.720f}, {3262.27f,0.008988f,8.1f,0.874f}, {3262.85f,0.004900f,8.1f,0.969f},
    {3264.01f,0.005157f,7.1f,0.304f}, {3265.01f,0.009427f,17.7f,0.861f}, {3266.29f,0.005229f,15.1f,0.216f}, {3296.72f,0.009473f,11.1f,0.537f},
    {3298.08f,0.017596f,16.9f,0.942f}, {3300.85f,0.005652f,17.8f,0.041f}, {3303.72f,0.002675f,12.0f,0.879f}, {3304.99f,0.004372f,9.4f,0.038f},
    {3306.07f,0.003027f,5.6f,0.844f}, {3308.98f,0.002735f,5.9f,0.823f}, {3310.28f,0.004609f,13.5f,0.824f}, {3311.77f,0.013135f,18.3f,0.126f},
    {3312.81f,0.013380f,8.4f,0.563f}, {3313.36f,0.007197f,8.5f,0.807f}, {3350.19f,0.004633f,29.6f,0.784f}, {3390.19f,0.004617f,7.8f,0.957f},
    {3390.75f,0.005574f,7.0f,0.408f}, {3463.39f,0.009002f,8.5f,0.808f}, {3464.48f,0.002517f,5.5f,0.075f}, {3465.70f,0.008415f,12.8f,0.091f},
    {3469.27f,0.003701f,13.8f,0.082f}, {3472.07f,0.005237f,13.7f,0.898f}, {3473.22f,0.006033f,17.6f,0.154f}, {3475.12f,0.003138f,7.6f,0.119f},
    {3477.60f,0.002628f,6.5f,0.150f}, {3486.40f,0.007337f,10.7f,0.675f}, {3486.88f,0.011676f,15.1f,0.952f}, {3487.52f,0.001667f,5.6f,0.214f},
    {3488.41f,0.004957f,16.7f,0.137f}, {3492.02f,0.001919f,9.4f,0.973f}, {3493.15f,0.002609f,7.1f,0.315f}, {3498.12f,0.010832f,14.6f,0.666f},
    {3498.69f,0.003361f,6.3f,0.703f}, {3499.16f,0.011414f,14.5f,0.052f}, {3500.39f,0.001575f,4.1f,0.921f}, {3500.78f,0.003131f,7.1f,0.897f},
    {3501.71f,0.011363f,17.8f,0.334f}, {3503.42f,0.001880f,8.0f,0.564f}, {3510.45f,0.001590f,4.2f,0.533f}, {3516.72f,0.001020f,3.2f,0.829f},
    {3521.22f,0.005718f,48.8f,0.659f}, {3523.32f,0.002306f,5.2f,0.006f}, {3524.23f,0.009434f,9.3f,0.939f}, {3530.12f,0.002197f,3.3f,0.149f},
    {3530.39f,0.004668f,7.7f,0.656f}, {3599.19f,0.002883f,10.8f,0.001f}, {3610.02f,0.004545f,11.1f,0.267f}, {3610.91f,0.005212f,14.1f,0.580f},
    {3705.42f,0.002878f,5.2f,0.239f}, {3716.47f,0.004027f,14.0f,0.001f}, {3717.02f,0.004444f,13.7f,0.336f}, {3756.68f,0.005999f,28.8f,0.860f},
    {3759.07f,0.004015f,12.7f,0.248f}, {3760.40f,0.003288f,8.5f,0.144f}, {3767.12f,0.003838f,21.2f,0.154f}, {3794.75f,0.002968f,15.0f,0.407f},
    {3930.82f,0.004694f,22.1f,0.493f}, {3986.25f,0.002920f,7.3f,0.303f}, {4054.39f,0.003160f,14.9f,0.305f}, {4121.75f,0.002209f,9.0f,0.816f},
    {4169.77f,0.006590f,26.9f,0.941f}, {4169.81f,0.004609f,9.1f,0.435f}, {4231.69f,0.003384f,14.9f,0.445f}, {4232.73f,0.003615f,20.5f,0.590f},
    {4342.48f,0.002444f,13.1f,0.304f}, {4349.33f,0.003144f,10.2f,0.767f}, {4563.29f,0.002661f,15.3f,0.146f}, {4564.44f,0.008543f,38.2f,0.456f},
    {4565.37f,0.005570f,15.2f,0.742f}, {4566.26f,0.002043f,12.3f,0.004f}, {4863.09f,0.004542f,32.8f,0.438f}, {4911.55f,0.003841f,36.9f,0.043f},
    {4912.46f,0.002216f,11.3f,0.273f}, {5521.47f,0.003342f,22.0f,0.294f}, {5825.33f,0.005456f,25.7f,0.757f}, {5825.62f,0.004760f,31.6f,0.186f},
    {5867.17f,0.009451f,78.0f,0.351f}, {5868.35f,0.011832f,73.0f,0.728f}, {6318.75f,0.006735f,44.1f,0.717f}, {6319.31f,0.004948f,17.1f,0.152f},
    {6338.01f,0.005789f,16.5f,0.039f}, {6338.57f,0.004004f,25.2f,0.466f}, {6415.97f,0.007478f,39.4f,0.624f}, {6746.13f,0.011275f,64.4f,0.023f},
    {6746.33f,0.002440f,16.0f,0.435f}, {7280.40f,0.004082f,40.7f,0.711f}, {7281.38f,0.005700f,46.1f,0.040f}, {8692.47f,0.004994f,56.3f,0.882f},
};
static const float RIDE_BANDS[17][4] = {   // Hz, band RMS at the strike, decay dB/s, width (octaves): half-octave wash BEDs + 6 ms TICKs
    {   250, 0.006985f,     6.0f, 0.5f },   // bed
    {   354, 0.024835f,     7.3f, 0.5f },   // bed
    {   500, 0.029208f,     6.7f, 0.5f },   // bed
    {   707, 0.016692f,     7.8f, 0.5f },   // bed
    {  1000, 0.007001f,     7.7f, 0.5f },   // bed
    {  1414, 0.011882f,     5.3f, 0.5f },   // bed
    {  2000, 0.010596f,     5.4f, 0.5f },   // bed
    {  2828, 0.017136f,     5.9f, 0.5f },   // bed
    {  4000, 0.009142f,     6.2f, 0.5f },   // bed
    {  5657, 0.006403f,    10.9f, 0.5f },   // bed
    {  8000, 0.005047f,    17.5f, 0.5f },   // bed
    { 11314, 0.010065f,    54.2f, 0.5f },   // bed
    {   250, 0.029957f,  1447.6f, 0.5f },   // tick
    {  5657, 0.021210f,  1447.6f, 0.5f },   // tick
    {  8000, 0.042727f,  1447.6f, 0.5f },   // tick
    { 11314, 0.040250f,  1447.6f, 0.5f },   // tick
    { 16000, 0.030512f,  1447.6f, 0.5f },   // tick
};

// THE ENGINE VOICE (2026-11-06): the same table played LIVE by INSTR_MODAL + instrument_modes. One mono slot
// = one cymbal; each stroke STRIKES the bank already ringing. The tick + wash become noise rows. The stroke's
// spot and force are per-note macro offsets (harmonics = spot, timbre = hardness), so no two are alike.
#define M_RIDE   36
#define MR_LVL   0.316f    // level-matched to the recorded ride on the groove stem (rms)
static void setup_engine_ride(void) {
    instrument(M_RIDE, INSTR_MODAL, 0, 0, 7, 2000);
    instrument_mono(M_RIDE, 1);
    instrument_modes(M_RIDE, &RIDE_MODES[0][0], 400);
    instrument_modes_noise(M_RIDE, &RIDE_BANDS[0][0], 17);
    // the 250 Hz tick's skirt kept 125 Hz ~7 dB heavy: a 4-pole high-pass at 180 Hz, as the prototype, built from
    // the voice filter + the slot's bus filter (one 2-pole at 220 shaved 250 Hz instead, its knee is soft)
    instrument_filter(M_RIDE, FILTER_HIGH, 180, 4);              // resonance 4 = Q ~0.7: at 0 the engine's
    instrument_fx_filter(M_RIDE, FILTER_HIGH, 180.0f, 0.0f);      // high-pass is critically damped and ate 250 Hz
    instrument_level(M_RIDE, MR_LVL);
    instrument_reverb(M_RIDE, 0.12f);
    instrument_pan(M_RIDE, 0.38f);
}

static void new_take(void) {
    rnd_seed(seed);
    step = 0; cur_bar = 0; cycle = 0; sticking = 0; nclose = 0; pedal = 0;
    for (int i = 0; i < NLIMB; i++) last_hit[i] = -1;
    rh_away_until = -1;
    t_step = audio_time() + 0.25;
    take_start = t_step;
}

static void book(int slot, int midi, double t, float gain, int gate, float h, float tb, float mo, float acc) {
    gain = clamp(gain, 0.04f, 1.0f);                     // NoteOpts gain 0 means FULL
    NoteOpts o = { .gain = gain, .harmonics = h, .timbre = tb, .morph = mo, .accent = acc };
    schedule_note_at(t, midi, slot, 7, gate, &o);
}

// book with a per-note FILTER offset (NoteOpts.cutoff_oct): the ride's colour per stroke
static void book_cut(int slot, int midi, double t, float gain, int gate, float cut_oct) {
    gain = clamp(gain, 0.04f, 1.0f);
    NoteOpts o = { .gain = gain, .cutoff_oct = cut_oct };
    schedule_note_at(t, midi, slot, 7, gate, &o);
}

// book with a per-note DAMP (NoteOpts step 1): the ring dies at its own rate instead of being cut by the gate
static void book_damp(int slot, int midi, double t, float gain, int gate, float h, float tb, float mo, float damp) {
    gain = clamp(gain, 0.04f, 1.0f);
#ifdef NO_DAMP
    damp = 0;
#endif
    NoteOpts o = { .gain = gain, .harmonics = h, .timbre = tb, .morph = mo, .damp = damp };
    schedule_note_at(t, midi, slot, 7, gate, &o);
}
// how long a ring should last (seconds) -> the NoteOpts damp that gives it (the engine: tau = 3(1-d)^2 + 0.02)
static float damp_for(float tau) {
    if (tau <= 0.02f) return 1.0f;
    float d = 1.0f - sqrtf((tau - 0.02f) / 3.0f);
    return d < 0 ? 0 : d;
}

static void dot(double t, int limb, int kind, float gain, float dev, char sym) {
    dots[ndot % NDOT] = (Dot){ t, cur_bar, step, limb, kind, gain, dev, sym };
    ndot++;
}

// ================= the techniques: one function per drum, the technique picks the bundle =========
// spot: where on the head (0 centre .. 1 edge), already including the drummer's drift
static void snare_stroke(double t, char tech, float g, float spot) {
    float tb = -0.03f + 0.55f * spot;                    // offset on the head's 0.15 strike position
    switch (tech) {
    case 'S':  // centre: round head + full wires
        // levels: oneshot-compare had the snare 5 dB hot vs the real kit (heads/wires slots were raised for
        // the rimshot, so these gains carry -5 dB plus that raise)
        book(S_HEAD, 55, t, g * 0.38f, 70, 0, tb, 0, 0);
        book(S_WIRE, 60, t, g * 0.40f, 70, 0, 0, 0, 0.25f);
        break;
    case 'E':  // edge: less body, an overtone ring on top, the wires answer less and brighter
        book(S_HEAD, 55, t, g * 0.21f, 100, 0, tb + 0.50f, 0.05f, 0);
        book(S_HEAD, 63, t, g * 0.19f, 150, -0.3f, 0.6f, 0, 0);      // the head's upper mode ringing
        book(S_WIRE, 60, t, g * 0.26f, 130, 0, 0, 0, 0.6f);
        break;
    case 'R':  // rimshot: head and rim at once, a crack on top, the wires bright
        book(S_HEAD, 55, t, g, 90, 0, tb + 0.50f, 0.15f, 0);
        book(S_WIRE, 60, t, g, 80, 0, 0, 0, 1.0f);
        book(S_RIM, 84, t, g * 0.35f, 30, 0, 0.25f, 0, 0);    // the crack sits ~16 dB under the head (CC0 ref)
        book(S_RIM, 76, t, g * 0.25f, 40, 0, 0.1f, 0, 0);
        break;
    case 'X':  // cross-stick: the stick laid across, a woody tock, hardly any head or wire
        // (CC0 ref: a low knock ~90-120 Hz under a 1.5 kHz click, gone in ~45 ms)
        book(S_RIM, 91, t, g * 0.21f, 15, -0.1f, 0, 0, 0);          // -3 dB: it measured 3 dB hot
        book(S_HEAD, 45, t, g * 0.26f, 18, 0, tb + 0.30f, 0, 0);
        book(S_WIRE, 60, t, g * 0.05f, 18, 0, 0, 0, 0);
        break;
    case 'g':  // ghost: a soft tap, mostly wires, short
    default:
        book(S_HEAD, 55, t, g * 0.23f, 50, 0, tb - 0.05f, 0, 0);
        book(S_WIRE, 60, t, g * 0.40f, 45, 0, 0, 0, 0);
        break;
    }
}

static void hat_stroke(double t, bool shank, float g, float open, float spot) {
    if (rec_ok && rec_kit) {
        // recorded: the pedal picks the take (closed / half / open); the gate chokes it, as the foot does
        int sl = open < 0.12f ? R_HATC : open < 0.55f ? R_HATS : R_HATO;
        int gate = 30 + (int)(900.0f * powf(open, 1.6f));
        book(sl, 60, t, g * (shank ? 1.0f : 0.8f), gate, 0, 0, 0, 0);   // shank ~1.5 dB over the tip (the real kit)
        return;
    }
    // openness -> how long the cymbals ring and how much they sizzle (noise). The ring is DAMPED by the foot
    // (NoteOpts.damp, a natural decay) rather than cut by the gate; the gate stays as a long safety
    int gate = 40 + (int)(1200.0f * powf(open, 1.6f));
    float hat_damp = damp_for((16.0f + 700.0f * powf(open, 1.6f)) / 1000.0f + 0.02f);   // measured vs the real hats: closed tip/shank reach -20 dB in ~75/115 ms
    float sizzle = 0.35f * sinf(3.14159f * fminf(open, 1.0f)) - 0.08f * (1.0f - open);
    // tip: a light tick, more noise; shank (the shoulder on the edge): louder, the bank's tone, lower
    // the shank sits HIGHER and broader than the tip on a real hat (3.9/9.1/14.9 kHz vs 2.7/8.2/14.4)
    if (shank) book_damp(S_HAT, 84, t, g * 0.92f, gate + 12, 0.05f + spot * 0.05f, sizzle * 0.4f - 0.10f, 0, hat_damp);
    else       book_damp(S_HAT, 81, t, g * 0.70f, gate, spot * 0.05f, sizzle * 0.4f, 0, hat_damp);
}

// lane mode: book the ease-down on the hat slot, ending ON the chick (which chokes the ring), then level back
static void book_ease(double tc) {
    double now_a = audio_time(), t0 = tc - EASE_S;
    if (t0 < now_a) t0 = now_a;
    if (tc <= t0) return;
    lane_at(t0, S_HAT, LANE_LEVEL, EASE_DB, (int)((tc - t0) * 1000.0 + 0.5));
    lane_at(tc, S_HAT, LANE_LEVEL, 0, 0);
}

static void chick(double t, float g) {
    book(S_CHICK, 55, t, g, 40, 0, 0, 0, 0);
}

static void ride_stroke(double t, bool bell, float g, float spot) {
    if (!bell && kit == KIT_MODAL) { book(M_RIDE, 60, t, g * 0.8f, 3000, 0.45f * spot, 0.4f * (g - 0.68f), 0, 0); return; }
    if (rec_ok && rec_kit) { book(bell ? R_BELL : R_RIDE, 60, t, g * (bell ? 0.95f : 0.8f), bell ? 2600 : 3000, 0, 0, 0, 0); return; }
    if (bell) book(S_BELL, 88, t, g * 1.05f, 900, 0, 0, 0, 0);
    else {    // the bow: shimmer + body, one stroke
        // each stroke its own COLOUR (2026-11-05): a harder stroke opens the band and splashes more shimmer,
        // the stick's slow drift (spot -1..1, the same hat_spot the hats read) moves it toward the bell or
        // the edge. Small on purpose (at most +-0.3 octave) so the colour picked by ear stays the centre.
        float oct = clamp(0.45f * (g - 0.7f) + 0.15f * spot, -0.3f, 0.3f);
        book_cut(S_RIDE, 60, t, g * 0.8f, 60, oct);            // the stroke (noise: the pitch is not heard)
        book_cut(S_RIDESH, 84, t, g * 0.3f * (1.0f + 0.8f * (g - 0.7f)), 2400, 0.10f * spot);
    }
}

static void kick_stroke(double t, char tech, float g, float spot) {
    // the beater's slap: a tiny high, short voice on the same head (louder when the beater digs in)
    // a real kick (CC0 ref): 54 Hz with a STRONG mode at 118 Hz (2.2x, -3.5 dB), booked as its own voice
    if (tech == 'B') {  // buried: the beater stays in the head, short and slappy
        // the beater pressing the head DAMPS it (NoteOpts.damp), a natural short decay instead of a gate cut
        book_damp(S_KICK, 31, t, g, 150, 0, 0.30f + spot * 0.05f, 0.10f, 0.82f);
        book_damp(S_KICK, 44, t, g * 0.55f, 80, 0, 0.30f, 0, 0.82f);
        book(S_KICK, 72, t, g * 0.8f, 14, 0.2f, 0.85f, -0.3f, 0);
    } else {            // bounced: the head rings, round
        book(S_KICK, 31, t, g, 150, 0, -0.06f + spot * 0.05f, 0, 0);
        book(S_KICK, 44, t, g * 0.75f, 80, 0, 0, 0, 0);
        book(S_KICK, 72, t, g * 0.25f, 10, 0.2f, 0.85f, -0.3f, 0);
    }
}

static void tom_stroke(double t, bool hi, float g, float spot) {
    // a real tom (CC0 refs): the fundamental (rack 129 Hz, floor ~90) plus a STRONG mode ~2.1x up,
    // which the membrane's own modes leave 17-20 dB down, so it is booked as a second, shorter voice
    int sl = hi ? S_TOMHI : S_TOMLO, n = hi ? 47 : 40;
    book(sl, n, t, g, hi ? 120 : 140, 0, spot * 0.12f, 0, 0);
    book(sl, n + 13, t, g * (hi ? 0.65f : 1.0f), hi ? 70 : 90, 0, spot * 0.12f, 0, 0);
}

static void crash_stroke(double t, float g) {
    if (rec_ok && rec_kit) { book(R_CRASH, 60, t, g, 3000, 0, 0, 0, 0); return; }
    book(S_CRASH, 100, t, g, 2600, 0, 0, 0, 0);
}

// ================= the drummer ======================================================
// a stroke the groove wants: which drum, which technique, how loud as written
typedef struct { int kind; char sym; float gain; int limb; int prio; } Want;

static float written_gain(int kind, char c, int s) {
    switch (kind) {
    case D_KICK:  return c == 'k' ? 0.55f : 0.88f;
    case D_SNARE: return c == 'g' ? 0.22f : c == 'z' ? 0.6f : 0.88f;
    case D_HAT:   return c == 'X' ? 0.85f : (s & 1) ? 0.45f : 0.6f;
    case D_RIDE:  return c == 'b' ? 0.8f : 0.65f;
    case D_TOM:   return 0.8f;
    case D_CRASH: return 0.9f;
    }
    return 0.7f;
}

// the timing feel of each limb, in seconds (positive = behind the beat)
static double limb_feel(int limb, int kind, char sym, double t) {
    if (!L(L_TIME)) return 0;
    float H = humanity;
    double d = 0;
    switch (limb) {
    case RH: d = -0.004 + drift(t, 0.3f, 1) * 0.004 + r1() * 0.002; break;     // hats on top
    case LH: d = (sym == 'g' ? 0.002 : 0.010) + drift(t, 0.25f, 2) * 0.005 + r1() * (sym == 'g' ? 0.005 : 0.003); break;
    case RF: d = drift(t, 0.2f, 3) * 0.004 + r1() * 0.0025; break;
    case LF: d = 0.005 + r1() * 0.004; break;
    }
    if (kind == D_TOM || (kind == D_SNARE && cur_bar == 7)) d = drift(t, 0.3f, 4) * 0.004 + r1() * 0.004;
    d += fatigue * 0.006;                                // a tired drummer drags a little
    return d * H;
}

static void play_step(double t, double steplen) {
    const Bar *B = &BARS[cur_bar];
    int s = step;
    float H = humanity;
    bool fill = s >= B->fill_from;
    float fillp = fill ? (float)(s - B->fill_from) / (float)(16 - B->fill_from) : 0;

    // ---- the phrase: energy builds over 8 bars into the fill, a long take tires the left hand ----
    float phr = (cur_bar + s / 16.0f) / 8.0f;
    energy = 0.4f + 0.35f * phr * phr + 0.08f * drift(t, 0.15f, 5) + (cur_bar == 0 && s < 4 ? 0.15f : 0);
    fatigue = fminf(1.0f, (float)(t - take_start) / 150.0f) * 0.7f;
    snare_spot = clamp(snare_spot + r1() * 0.05f + (0.15f - snare_spot) * 0.1f, 0, 0.6f);
    hat_spot = drift(t, 0.5f, 6);

    // ---- the left foot: the pedal is a continuous control the hat hits read ----
    char fc = B->foot[s];
    float was = pedal;
    bool chick_now = false;
    float chick_g = 0;
    if (fc != '.') {
        float target = fc == 'O' ? 0.85f : fc == '~' ? 0.40f : 0.0f;
        if (L(L_PEDAL)) {
            if (fc == 'O') target = 0.60f + 0.30f * energy + 0.05f * r1();
            else if (fc == '~') target = 0.30f + 0.12f * drift(t, 0.4f, 7) + 0.08f * energy;
            else target = clamp(0.03f + 0.07f * energy + 0.03f * drift(t, 0.3f, 8), 0, 0.15f);  // never quite shut
        }
        pedal = target;
        if (fc == 'c') { chick_now = true; chick_g = 0.75f; }
        if (was > 0.3f && pedal < 0.2f) { chick_now = true; chick_g = fmaxf(chick_g, 0.5f + 0.4f * was); }
    }
    if (chick_now) {
        double tc = t + limb_feel(LF, D_CHICK, 'c', t);
        float g = L(L_DYN) ? chick_g * (0.85f + 0.2f * energy) + 0.05f * r1() : chick_g;
        chick(tc, g);
        if (L(L_PEDAL) && was > 0.3f) {   // ease the foot down first
            if (lane_mode) book_ease(tc);
            else { close_t[nclose % NCLOSE] = tc; nclose++; }
        }
        dot(tc, LF, D_CHICK, g, (float)(tc - t), 'c');
        last_hit[LF] = tc;
    }
    shown_pedal = pedal;

    // ---- gather what the bar wants at this step ----
    Want w[8]; int n = 0;
    char c;
    if ((c = B->crash[s]) != '.') w[n++] = (Want){ D_CRASH, c, 0, RH, 3 };
    if ((c = B->kick[s])  != '.') w[n++] = (Want){ D_KICK,  c, 0, RF, 3 };
    if ((c = B->snare[s]) != '.') w[n++] = (Want){ D_SNARE, c, 0, LH, 2 };
    if ((c = B->tom[s])   != '.') w[n++] = (Want){ D_TOM,   c, 0, LH, 2 };
    if ((c = B->hat[s])   != '.') w[n++] = (Want){ D_HAT,   c, 0, RH, 1 };
    if ((c = B->ride[s])  != '.') w[n++] = (Want){ D_RIDE,  c, 0, RH, 1 };

    // ---- the limbs: who plays what, a hand is in one place at a time ----
    bool used[NLIMB] = { false };
    if (L(L_LIMBS)) {
        // fill strokes alternate R / L, starting R; in the groove the left hand owns the snare
        if (s == B->fill_from) sticking = 0;
        for (int i = 0; i < n; i++) if ((w[i].kind == D_SNARE || w[i].kind == D_TOM) && fill) {
            w[i].limb = (sticking++ & 1) ? LH : RH;
        }
        // crash wants the right hand, unless it is busy on a drum
        for (int i = 0; i < n; i++) if (w[i].kind == D_CRASH)
            for (int j = 0; j < n; j++) if (j != i && w[j].limb == RH && w[j].prio >= 2) w[i].limb = LH;
        // allocate by priority: the hat/ride stroke drops if the right hand is elsewhere
        for (int p = 3; p >= 1; p--) for (int i = 0; i < n; i++) if (w[i].prio == p) {
            int lb = w[i].limb;
            bool away = (lb == RH && (w[i].kind == D_HAT || w[i].kind == D_RIDE) && t < rh_away_until);
            if (used[lb] || away) { w[i].kind = -1; continue; }
            used[lb] = true;
            if (lb == RH && w[i].kind != D_HAT && w[i].kind != D_RIDE && w[i].kind != D_CRASH)
                rh_away_until = t + steplen * 2.5;      // the stick went to the drums; back in two 16ths
        }
    }

    for (int i = 0; i < n; i++) {
        if (w[i].kind < 0) continue;
        int kind = w[i].kind, limb = w[i].limb;
        char sym = w[i].sym;
        double tt = t + limb_feel(limb, kind, sym, t);
        if (L(L_TIME) && fill) tt -= 0.010 * fillp * H;  // rushing a little into the landing

        // ---- dynamics ----
        float gw = written_gain(kind, sym, s), g = gw;
        if (L(L_DYN)) {
            switch (kind) {
            case D_HAT:   g = (sym == 'X' ? 0.78f : 0.48f + 0.08f * ((s & 3) == 2)) + 0.12f * energy + 0.05f * r1(); break;
            case D_SNARE: g = sym == 'g' ? (0.17f + 0.06f * rnd_float() - 0.06f * fatigue)
                                         : (0.80f + 0.15f * energy + 0.04f * r1()); break;
            case D_KICK:  g = (sym == 'k' ? 0.5f : 0.82f) + 0.1f * energy + 0.04f * r1(); break;
            case D_RIDE:  g = (sym == 'b' ? 0.75f : 0.55f + 0.08f * !(s & 3)) + 0.1f * energy + 0.04f * r1(); break;
            case D_TOM:   g = 0.7f + 0.04f * r1(); break;
            case D_CRASH: g = 0.85f + 0.12f * energy; break;
            }
            if (fill && kind != D_KICK) {                // a fill builds: soft to hard, the R hand a bit stronger
                float ghostish = (sym == 'g') ? 0.5f : 1.0f;
                g = (0.35f + 0.55f * fillp) * ghostish + (limb == RH ? 0.05f : 0) + 0.04f * r1();
            }
            g = gw + (g - gw) * H;                       // humanity scales how far the player strays from the page
        }
        // a quick second stroke with the same limb comes a little late and softer
        if (L(L_LIMBS) && last_hit[limb] > 0 && tt - last_hit[limb] < 0.10) { tt += 0.005 * H; g *= 1.0f - 0.12f * H; }

        // ---- technique micro-values: where the stick lands ----
        bool T = L(L_TECH);
        float spot = T ? 0.15f + (snare_spot - 0.15f) * H : 0.15f;
        float hs = T ? hat_spot * H : 0;
        char tech = sym;
        if (T && kind == D_SNARE && sym == 'S' && energy > 0.75f && rnd_float() < 0.25f * H) tech = 'R';  // excited: a rimshot
        if (T && kind == D_KICK && sym == 'K' && (s & 3) && rnd_float() < 0.3f * H) tech = 'B';            // off-beat kicks get buried

        // ---- flams, drags, buzz: several strokes (limbs layer) or one plain stroke ----
        bool LM = L(L_LIMBS);
        switch (kind) {
        case D_KICK:  kick_stroke(tt, tech, g, T ? r1() * 0.5f : 0); break;
        case D_HAT:   hat_stroke(tt, sym == 'X', g, pedal, hs); break;
        case D_RIDE:  ride_stroke(tt, sym == 'b', g, T ? hs : 0); break;
        case D_TOM:   tom_stroke(tt, sym == 'T', g, T ? r1() * 0.5f : 0); break;
        case D_CRASH: crash_stroke(tt, g); break;
        case D_SNARE:
            if (sym == 'f' && LM) {             // flam: the other hand a grace stroke just before
                snare_stroke(tt - (0.018 + 0.012 * rnd_float()) * H, 'g', g * 0.35f, spot);
                snare_stroke(tt, 'S', g, spot);
            } else if (sym == 'd' && LM) {      // drag: two bounced grace strokes, then the main
                double gap = 0.028 * (0.9 + 0.2 * rnd_float());
                snare_stroke(tt - gap * 2, 'g', g * 0.30f, spot);
                snare_stroke(tt - gap, 'g', g * 0.26f, spot);
                snare_stroke(tt, 'S', g, spot);
            } else if (sym == 'z' && LM) {      // buzz: the stick pressed in, bouncing faster and softer
                double bt = tt, gap = 0.030;
                float bg = g;
                int nb = 7 + (int)(rnd_float() * 3);
                for (int k = 0; k < nb; k++) {
                    snare_stroke(bt, 'g', bg, spot + 0.02f * k);
                    bt += gap * (0.9 + 0.2 * rnd_float()); gap *= 0.88; bg *= 0.84f;
                }
            } else {
                char tk = (tech == 'f' || tech == 'd' || tech == 'z') ? 'S' : tech;
                snare_stroke(tt, tk, g, spot);
            }
            break;
        }
        dot(tt, limb, kind, g, (float)(tt - t), tech);
        last_hit[limb] = tt;
    }
}

static void schedule_ahead(void) {
    double now_a = audio_time();
    double len = 60.0 / tempo / 4.0;
    while (t_step < now_a + LOOKAHEAD) {
        if (t_step < now_a) t_step = now_a + 0.01;
        double l = len;
        if (L(L_TIME)) {
            l *= 1.0 + 0.006 * drift(t_step, 0.05f, 9) * humanity;     // the tempo breathes
            if (step >= BARS[cur_bar].fill_from) l *= 1.0 - 0.012 * humanity; // and pushes in a fill
        }
        play_step(t_step, l);
        t_step += l;
        if (++step == 16) { step = 0; if (++cur_bar == 8) { cur_bar = 0; cycle++; } }
    }
}

// every frame: the foot easing down on a ringing open hat, the one thing a booked note cannot do
static void ride_pedal(void) {
    float lv = 1.0f;
    if (L(L_PEDAL) && !lane_mode) {
        double now_a = audio_time();
        for (int k = 0; k < NCLOSE && k < nclose; k++) {
            double until = close_t[k] - now_a;
            if (until > 0 && until < 0.10) lv = fminf(lv, 1.0f - 0.55f * smooth01((float)(0.10 - until) / 0.10f));
        }
    }
    if (fabsf(lv - hat_level_last) > 0.01f) { instrument_level(S_HAT, lv * HAT_LVL); hat_level_last = lv; }
}

#if defined(PROBE_TECH) || defined(PROBE_PEDAL)
// probes: isolate the techniques / the pedal, one hit per second, for measurement
static void probe(void) {
    playing = false;
    double t0 = audio_time() + 0.3;
#ifdef PROBE_TECH
    // 0 centre 1 edge 2 rimshot 3 cross-stick 4 ghost 5 hat tip 6 hat shank 7 ride bow 8 ride bell
    // 9 kick bounced 10 kick buried 11 flam 12 drag 13 buzz 16 tom high 17 tom floor 18 crash
    snare_stroke(t0 + 0, 'S', 0.85f, 0.15f);
    snare_stroke(t0 + 1, 'E', 0.85f, 0.15f);
    snare_stroke(t0 + 2, 'R', 0.85f, 0.15f);
    snare_stroke(t0 + 3, 'X', 0.85f, 0.15f);
    snare_stroke(t0 + 4, 'g', 0.20f, 0.15f);
    hat_stroke(t0 + 5, false, 0.7f, 0.05f, 0);
    hat_stroke(t0 + 6, true,  0.7f, 0.05f, 0);
    ride_stroke(t0 + 7, false, 0.7f, 0);
    ride_stroke(t0 + 9, true, 0.7f, 0);
    kick_stroke(t0 + 11, 'K', 0.85f, 0);
    kick_stroke(t0 + 12, 'B', 0.85f, 0);
    snare_stroke(t0 + 13 - 0.024, 'g', 0.3f, 0.15f); snare_stroke(t0 + 13, 'S', 0.85f, 0.15f);
    snare_stroke(t0 + 14 - 0.056, 'g', 0.26f, 0.15f); snare_stroke(t0 + 14 - 0.028, 'g', 0.22f, 0.15f); snare_stroke(t0 + 14, 'S', 0.85f, 0.15f);
    { double bt = t0 + 15, gap = 0.03; float bg = 0.6f; for (int k = 0; k < 8; k++) { snare_stroke(bt, 'g', bg, 0.15f); bt += gap; gap *= 0.88; bg *= 0.84f; } }
    tom_stroke(t0 + 16, true, 0.85f, 0); tom_stroke(t0 + 17, false, 0.85f, 0); crash_stroke(t0 + 18, 0.85f);
#endif
#ifdef PROBE_PEDAL
    // 0..6: tip hits at openness 0, .15, .3, .45, .6, .75, .9 · 7: open .85 choked by the foot at +0.25
    // 8: open .85 with the foot easing down (live level) then the chick at +0.30 · 9: chick alone
    for (int i = 0; i < 7; i++) hat_stroke(t0 + i, false, 0.7f, i * 0.15f, 0);
    hat_stroke(t0 + 7, false, 0.7f, 0.85f, 0); chick(t0 + 7.25, 0.8f);
    hat_stroke(t0 + 8, false, 0.7f, 0.85f, 0); chick(t0 + 8.30, 0.8f);
    if (lane_mode) book_ease(t0 + 8.30); else { close_t[0] = t0 + 8.30; nclose = 1; }
    chick(t0 + 9, 0.8f);
#endif
}
#endif

void init(void) {
    setup_sound();
    setup_recorded();
    setup_engine_ride();
#ifdef SYNTH_KIT
    kit = KIT_SYNTH;
#endif
#ifdef MODAL_KIT   // DE_DEFINES=MODAL_KIT: start on the modal ride (the A/B renders)
    kit = KIT_MODAL;
#endif
#ifdef LANES        // DE_DEFINES=LANES: start with the ease-down booked as LANE_LEVEL ramps (the step-4 A/B)
    lane_mode = true;
#endif
    new_take();
#if defined(PROBE_TECH) || defined(PROBE_PEDAL)
    probe();
#endif
#ifdef PROBE_GRID
    human = false;
#endif
}

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; t_step = audio_time() + 0.15; }
    if (keyp(KEY_TAB)) human = !human;
    if (keyp('K')) kit = (kit + 1) % NKIT;
    if (keyp('L')) { lane_mode = !lane_mode; nclose = 0; lane_at(audio_time(), S_HAT, LANE_LEVEL, 0, 0); }
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) layer[i] = !layer[i];
    if (keyp(KEY_UP))    humanity = fminf(1.5f, humanity + 0.25f);
    if (keyp(KEY_DOWN))  humanity = fmaxf(0.0f, humanity - 0.25f);
    if (keyp(KEY_RIGHT)) tempo = min(140, tempo + 4);
    if (keyp(KEY_LEFT))  tempo = max(60, tempo - 4);
    if (keyp('R')) { seed++; new_take(); }
    if (playing) schedule_ahead();
#ifdef PROBE_PEDAL
    { bool keep = human; human = true; ride_pedal(); human = keep; }
#else
    ride_pedal();
#endif
#ifdef DE_TRACE
    watch("rec", "%d%d", rec_ok, kit);
    watch("bar", "%d", cur_bar);
    watch("pedal", "%.2f", shown_pedal);
    watch("energy", "%.2f", energy);
#endif
}

// ================= the screen ===========================================================
void draw(void) {
    cls(CLR_DARKER_GREY);
    double at = audio_time();
    print(human ? "DRUMMER" : "GRID", 4, 4, human ? CLR_GREEN : CLR_RED);
    print(kit == KIT_MODAL ? "K: modal ride (400 modes)" : !rec_ok ? "synth cymbals" : kit == KIT_REC ? "K: recorded cymbals" : "K: synth cymbals", 4, 192, kit == KIT_MODAL ? CLR_GREEN : rec_ok && rec_kit ? CLR_YELLOW : CLR_DARK_GREY);
    print(str("%d bpm  bar %d/8  take %d", tempo, cur_bar + 1, seed), 76, 4, CLR_WHITE);
    print(lane_mode ? "L: lane" : "L: ridden", 200, 192, lane_mode ? CLR_YELLOW : CLR_DARK_GREY);

    // the bar as a grid: one row per drum, each stroke where it actually landed (offset = feel)
    int gx = 44, gy = 18, cw = 16, rh = 10;
    for (int k = 0; k < ND; k++) print(DK_NAME[k], 4, gy + k * rh + 1, DK_COL[k]);
    for (int s = 0; s < 16; s++) {
        int x = gx + s * cw;
        rectfill(x, gy - 2, 1, ND * rh + 2, s % 4 == 0 ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
    }
    int showbar = cur_bar;
    for (int k = 0; k < NDOT && k < ndot; k++) {
        Dot *d = &dots[k];
        float age = (float)(at - d->t);
        if (age < -0.2f || d->cur_bar != showbar || age > 2.5f) continue;
        double stepsec = 60.0 / tempo / 4.0;
        int x = gx + d->step * cw + (int)(d->dev / stepsec * cw * 4);   // timing offset magnified 4x
        int y = gy + d->kind * rh + 4;
        int sz = 1 + (int)(d->gain * 3.0f);
        circfill(x, y, sz, age < 0.12f && age > 0 ? CLR_WHITE : DK_COL[d->kind]);
        if (d->kind == D_SNARE && d->sym != 'S' && d->sym != 'g') print(str("%c", d->sym), x + 4, y - 3, CLR_LIGHT_GREY);
    }

    // the limbs: who is where right now
    int ly = 96;
    for (int l = 0; l < NLIMB; l++) {
        int x = 4 + l * 78;
        const char *where = "-";
        for (int k = 0; k < NDOT && k < ndot; k++) {
            Dot *d = &dots[k];
            float age = (float)(at - d->t);
            if (d->limb == l && age >= 0 && age < 0.18f) where = DK_NAME[d->kind];
        }
        print(LIMB_NAME[l], x, ly, CLR_LIGHT_GREY);
        print(where, x, ly + 10, strcmp(where, "-") ? CLR_YELLOW : CLR_DARK_GREY);
    }

    // the pedal, the phrase
    int py = 124;
    print("pedal", 4, py, CLR_BLUE);
    rect(52, py, 102, 8, CLR_LIGHT_GREY);
    rectfill(53, py + 1, (int)(100 * clamp(shown_pedal, 0, 1)), 6, CLR_BLUE);
    print(shown_pedal < 0.2f ? "closed" : shown_pedal < 0.5f ? "half" : "open", 160, py, CLR_BLUE);
    print("energy", 4, py + 12, CLR_ORANGE);
    rect(52, py + 12, 102, 8, CLR_LIGHT_GREY);
    rectfill(53, py + 13, (int)(100 * clamp(energy, 0, 1)), 6, CLR_ORANGE);
    print("tired", 4, py + 24, CLR_INDIGO);
    rect(52, py + 24, 102, 8, CLR_LIGHT_GREY);
    rectfill(53, py + 25, (int)(100 * clamp(fatigue, 0, 1)), 6, CLR_INDIGO);
    print("snare spot", 210, py, CLR_WHITE);
    rect(210, py + 12, 100, 8, CLR_LIGHT_GREY);
    circfill(211 + (int)(snare_spot / 0.6f * 96), py + 16, 3, CLR_WHITE);
    print("ctr      edge", 210, py + 24, CLR_DARK_GREY);

    int y2 = 176, lx = 4;
    for (int i = 0; i < NLAYER; i++) {
        int c = !human ? CLR_DARK_GREY : layer[i] ? CLR_GREEN : CLR_DARK_GREY;
        const char *t = str("%d %s", i + 1, LAYER_NAME[i]);
        print(t, lx, y2, c);
        lx += (int)strlen(t) * 8 + 6;
    }
    print(str("humanity %.2f  TAB  R  SPACE  </>", humanity), 4, y2 + 12, CLR_LIGHT_GREY);
}
