#include "studio.h"
#include <math.h>

// SOUND CHECK — the sound engine's self-test cart. init() slams the request queue with the
// WORST-CASE burst (all 27 instrument slots defined with envs/LFOs/filters/engine macros +
// all 4 user wavetables in one frame), then update() walks the whole API: every wave id
// audibly in sequence (including the PLUCK / MALLET / FM engines), chords/strums,
// a schedule_hit machine-gun burst (stresses the delayed pen), and a held note driven by
// every live setter.
//
// PASS/FAIL: the engine now counts dropped requests and printh-screams
//   "[sound] WARNING: request queue overflow..."
// if ANY sound call was lost. Run this cart and watch the log (or run headless:
//   node tools/play.js soundcheck run
// any [sound] WARNING line in the output = FAIL). Silence = the queue, the delayed pen,
// and the slot/wave banks all survived max load. Ears double-check: the 12 played waves
// must all sound DIFFERENT (a stuck-on-square bug is instantly audible).

static int   step = -1;          // current test step (-1 until the first beat)
static float t = 0;
static const char *label = "warming up";

static const char *WN[15] = { "SQUARE", "SAW", "TRI", "NOISE", "SINE", "USER0 org", "USER1 vox", "USER2 bel", "USER3 fld", "PLUCK ks", "MALLET bar", "FM 2op", "ORGAN B3", "EPIANO", "PD cz" };

static int  held = -1;           // the live-setter test voice
static int  burst_left = 0;      // schedule_hit machine-gun

void init(void) {
    // ---- worst-case define burst, all in one frame ----
    // 4 user wavetables (4 x 16 packed requests)
    float tbl[64];
    for (int w = 0; w < 4; w++) {
        for (int i = 0; i < 64; i++) {
            float x = i / 64.0f * 6.2831853f;
            tbl[i] = w == 0 ? 0.55f * sinf(x) + 0.28f * sinf(2 * x) + 0.18f * sinf(3 * x)   // org-ish
                   : w == 1 ? 0.45f * sinf(x) + 0.30f * sinf(3 * x) + 0.18f * sinf(5 * x)   // vox-ish
                   : w == 2 ? 0.60f * sinf(x) + 0.35f * sinf(4 * x) + 0.25f * sinf(7 * x)   // bel-ish
                            : sinf(2.5f * sinf(x));                                          // folded
        }
        wave_set(w, tbl, 64);
    }
    // every definable slot (5..31), each with ADSR + duty + 3 LFOs + filter + 2 envs +
    // 3 engine macros = 27 slots x 11 calls = 297 requests on top of the 64 wave requests.
    // If the queue can't take this, the tripwire screams and this cart FAILS.
    for (int s = 5; s <= 31; s++) {
        int wave = (s - 5) % 9 < 5 ? (s - 5) % 9 : INSTR_USER0 + (s - 5) % 9 - 5;
        instrument(s, wave, 3, 80, 5, 120);
        instrument_duty(s, 0.3f);
        instrument_lfo(s, 0, LFO_PITCH, 5.0f, 0.2f);
        instrument_lfo(s, 1, LFO_VOLUME, 7.0f, 0.2f);
        instrument_lfo(s, 2, LFO_CUTOFF, 0.6f, 300);
        instrument_filter(s, FILTER_LOW, 1200, 6);
        instrument_env(s, 0, ENV_CUTOFF, 0, 120, 900);
        instrument_env(s, 1, ENV_PITCH, 0, 40, 3);
        instrument_follow(s, LFO_CUTOFF, 3, 200, 800);
        instrument_harmonics(s, 0.6f);
        instrument_timbre(s, 0.5f);
        instrument_morph(s, 0.4f);
    }
    // the modeled engines: slot 31 re-defined as the KS pluck, slot 30 as the modal
    // mallet, slot 29 as the 2-op FM (all played in the wave walk)
    instrument(31, INSTR_PLUCK, 1, 0, 7, 120);
    instrument_harmonics(31, 0.6f);
    instrument_timbre(31, 0.7f);
    instrument_morph(31, 0.25f);
    instrument(30, INSTR_MALLET, 1, 0, 7, 120);
    instrument_harmonics(30, 0.3f);
    instrument_timbre(30, 0.7f);
    instrument_morph(30, 0.5f);
    instrument(29, INSTR_FM, 1, 400, 4, 120);
    instrument_harmonics(29, 0.15f);
    instrument_timbre(29, 0.6f);
    instrument_morph(29, 0.2f);
    instrument(28, INSTR_ORGAN, 1, 0, 7, 120);   // slot 28 = the tonewheel organ engine
    instrument_harmonics(28, 0.45f);             // jimmy-smith-ish registration
    instrument_timbre(28, 0.55f);
    instrument_morph(28, 0.7f);                  // scanner chorus + a touch of percussion
    instrument(27, INSTR_EPIANO, 1, 0, 7, 1200); // slot 27 = the electric-piano engine
    instrument_harmonics(27, 0.15f);             // Rhodes
    instrument_timbre(27, 0.35f);
    instrument_morph(27, 0.4f);                  // a little bark
    instrument(26, INSTR_PD, 1, 0, 7, 700);      // slot 26 = the phase-distortion (Casio CZ) engine
    instrument_harmonics(26, 0.85f);             // a resonant wavetype
    instrument_timbre(26, 0.45f);
    instrument_morph(26, 0.6f);                  // the DCW sweep on
    instrument_lfo(26, 2, LFO_MORPH, 3.0f, 0.4f);     // exercise the new macro-LFO destination
    instrument_env(26, 2, ENV_TIMBRE, 0, 200, 0.4f);  // and the new macro-ENV dest (3rd env slot)
    bpm(120);
}

void update(void) {
    t += dt();
    if (burst_left > 0) {                       // machine-gun: 40 steps at 9ms via schedule_hit
        while (burst_left > 0) {                // (queued in one go — stresses the 64-slot delayed pen)
            schedule_hit((40 - burst_left) * 9, 70 + (40 - burst_left) % 12, INSTR_TRI, 3, 12);
            burst_left--;
        }
    }
    if (t < 0.6f) return;
    t = 0;
    step++;
    int s = step % 22;
    if (s < 15) {                               // each wave id, audibly, labeled
        label = WN[s];
        if      (s == 9)  hit(57, 31, 6, 500);   // the KS pluck engine (slot 31)
        else if (s == 10) hit(69, 30, 6, 500);   // the modal mallet engine (slot 30)
        else if (s == 11) hit(57, 29, 6, 500);   // the 2-op FM engine (slot 29)
        else if (s == 12) hit(45, 28, 6, 700);   // the tonewheel organ engine (slot 28)
        else if (s == 13) hit(57, 27, 6, 900);   // the electric-piano engine (slot 27)
        else if (s == 14) hit(45, 26, 6, 700);   // the phase-distortion (CZ) engine (slot 26)
        else              note(57, s, 6);        // slots 0-8: raw waves + the 4 user waves
    } else if (s == 15) {
        label = "chord + strum";
        chord(48, CHORD_MIN7, 5, 4);
        strum(60, CHORD_MAJ, 6, 4, 40);
    } else if (s == 16) {
        label = "tone + schedule";
        tone(SCALE_PENTA, 4, 7, 4);
        schedule(120, 72, 8, 4);
    } else if (s == 17) {
        label = "schedule_hit burst (40x9ms)";
        burst_left = 40;
    } else if (s == 18) {
        label = "note_on + live setters";
        held = note_on(52, 9, 5);
        note_glide(held, 80);
    } else if (s == 19 && held >= 0) {
        label = "live: pitch/cutoff/res/duty/lfo/env/macros";
        note_pitch(held, 59);
        note_cutoff(held, 2000);
        note_res(held, 9);
        note_duty(held, 0.2f);
        note_lfo(held, 0, LFO_PITCH, 6, 0.4f);
        note_env(held, 0, ENV_CUTOFF, 0, 150, 1200);
        note_follow(held, LFO_CUTOFF, 3, 200, 1500);
        note_filter(held, FILTER_BAND);
        note_harmonics(held, 0.9f);              // engine macros ride kind 22 — no-op on a
        note_timbre(held, 0.7f);                 // wavetable slot, but the request path and
        note_morph(held, 0.3f);                  // stale-handle safety must survive them
    } else if (s == 20) {
        label = "note_off + panic";
        if (held >= 0) { note_off(held); held = -1; }
        note_off_all();
    } else if (s == 21) {
        label = "sfx + music banks";
        sfx(0);
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("SOUND CHECK", 8, 6, CLR_WHITE);
    print_scaled(label, 8, 60, CLR_YELLOW, 2);
    print(str("step %d", step < 0 ? 0 : step % 22), 8, 90, CLR_LIGHT_GREY);
    print("PASS = no [sound] WARNING in the log", 8, 130, CLR_LIME_GREEN);
    print("       and all 15 waves sound different", 8, 140, CLR_LIME_GREEN);
    print("FAIL = any dropped-request warning", 8, 154, CLR_DARK_PEACH);
    font(FONT_SMALL);
    print("init slammed: 4 wavetables + 27 slots x 11 defines in one frame (the worst case)", 8, 178, CLR_MEDIUM_GREY);
    print("headless: node tools/play.js soundcheck run   - grep output for [sound]", 8, 188, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
