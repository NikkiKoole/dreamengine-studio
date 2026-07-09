/* de:meta
{
  "title": "tb-303 bass line",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "instrument",
    "generative"
  ],
  "teaches": [
    "subtractive-synth",
    "adsr-envelope",
    "generative-melody",
    "step-sequencer"
  ],
  "lineage": "Roland TB-303 (1981) acid machine; a monosynth-plus-sequencer where the sound is the resonant lowpass — live cutoff/reso/drive modulation on a held voice, authentic non-retriggering slide, and an N-key random root-heavy minor-pentatonic line generator.",
  "homage": "Roland TB-303 Bass Line (1981)",
  "todo": [
    "note-off / erase now works (was a capture bug: erase set PTR_ROLL so the held drag repainted it). Optional nicety: right-click / right-drag to erase a run of steps on desktop.",
    "port the full sequencer (ties / octave-down / length / swing) into acidrack's embedded 303 lanes — acidrack has its OWN 303, so these features are standalone-only for now (acidrack todo already flags its 303 doesn't shuffle).",
    "slide connector may not draw when sliding BETWEEN a tie note and a normal note — saw it once, couldn't reproduce. The tie→slide-out connector only covers note→tie→note; other tie/normal adjacencies may miss a line. Now scriptable to repro via the harness press/release/drag — build a saved .script that lays down the exact tie+slide combo and eyeball the connector. (The separate LEN-wrap glitch that drew stray slide lines + phantom tie bars is fixed: draw is linear + clipped to < plen.)"
  ],
  "description": "The acid machine — third box in the classic-machine family (cr-78, tr-808), but a monosynth, not a drum kit: ONE held voice (saw or square) through a resonant lowpass — the engine's FILTER_DIODE, a real diode-ladder model (~18dB/oct, bass drains as the resonance climbs, and the resonance saturates inside the loop so it growls, the way a 303 does) — sequenced from a mouse-drawn piano roll. All the 303 signatures: slide (a slid step doesn't retrigger — note_glide carries the pitch into the next note while the filter envelope keeps decaying, exactly like the real circuit), accent (louder + a harder filter kick), staccato gating at 70% of the step, and eight draggable knobs — CUTOFF and RESO apply live to the ringing voice via note_cutoff/note_res (the entire point of acid), DRV is the instrument_drive/note_drive saturation (tanh AFTER the filter, so the resonant peak screams into it — RES + DRV up is the proper acid bite), SQL (squelch) multiplies the filter-env sweep up to 3x (full ENV + full SQL = a 9kHz scream), and SWING shuffles the off-16ths (an anachronism, like the drum carts' — the real 303 was straight). The full 303 sequencer: per-step OCTAVE up OR down, accent, slide, and TIE (hold the previous note across steps — the sustained roots a slide can't give you; put SLD on a tie step and the held note glides on into the next, so you can slide from one long tied note into another), plus an adjustable pattern LENGTH (tap the strip along the bottom; odd lengths roll against the beat for hypnotic polymeter, and a red divider in the roll marks the loop end). Press N for a fresh random acid line (root-heavy minor pentatonic walk with random accents, slides and ties, the honest way it was always done). H opens a help panel with every control. Two authored patterns + the generator; LEFT/RIGHT pattern, UP/DOWN tempo, SPACE run/stop."
}
de:meta */
#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <stdio.h>
#include <math.h>

// ── TB-303 BASS LINE ──────────────────────────────────────────────────────
// The acid machine. Roland's TB-303 (1981) was meant to replace a bass
// player for practicing guitarists; it failed at that, got dumped into
// pawn shops, and in 1987 Chicago's Phuture turned its knobs while it
// played ("Acid Tracks") — and invented acid house. Third box in the
// classic-machine family (cr78.c, tr808.c), but a different animal: not a
// drum kit, a MONOSYNTH with a sequencer, and the sound is 90% filter.
//
// How a 303 works, and how this cart maps it onto the API:
//
//   one voice — saw or square through a resonant lowpass. Here: ONE
//     note_on() handle through FILTER_DIODE — the engine's TB-303 diode
//     ladder (~18dB/oct, bass drains as resonance climbs, and the
//     resonance saturates INSIDE the loop, so it growls instead of
//     ringing clean). ACID_FILTER swaps the circuit for an A/B
//     (audio-notes §25 has the measured comparison).
//   the envelope — a one-shot decay sweep of the filter cutoff (the env
//     mod + decay knobs). Here: instrument_env(ENV_CUTOFF) re-issued
//     before every retrigger so the current knob values stick.
//   slide — THE 303 trick. A slid step doesn't retrigger: the gate holds
//     and the pitch glides (~60ms) into the next note, envelope still
//     decaying. Here: note_glide(h, 60) + note_pitch(h, midi) on the held
//     handle. Authentic detail for free: the filter envelope genuinely
//     does NOT refire on slid steps, same as the real circuit.
//   accent — louder AND a harder filter kick (accent interacts with env).
//     Here: vol 5 → 7 plus the env amount scaled by the ACC knob.
//   gate — non-slid notes are staccato: the gate drops at ~70% of the
//     step, then the next step retriggers.
//   the acid part — you turn CUTOFF and RESO while it plays. Here:
//     note_cutoff()/note_res() applied live to the ringing voice, plus
//     instrument_filter() so the next trigger agrees. Grab a knob.
//   drive — the missing ingredient (audio-notes §17): a real 303's squelch
//     is the filter driven into saturation. DRV = instrument_drive(), tanh
//     AFTER the filter so the resonant peak screams into it; note_drive()
//     moves it on the ringing voice. RES + DRV up = the proper acid bite.
//
//   POINTER piano roll: press/drag to draw the line, press a note to
//           erase it (tap the lit note = rest; only PLACING captures the
//           finger for drag-paint, so an erase-tap isn't repainted). Rows below
//           the roll: OCT (tap cycles +1 / -1 / off), ACC (accent), SLD (slide
//           into next step), TIE (hold the previous note through this step —
//           sustain, no retrigger, no pitch change). The strip along the BOTTOM
//           sets pattern LENGTH (tap a cell = loop end; a red divider marks it
//           in the roll). Knobs: drag vertically (or hover +
//           wheel on desktop) — SWG is the swing/shuffle, SQL (lower-left,
//           under CUT) is the filter-env depth. The SAW/SQR switch toggles the wave.
//           MULTITOUCH: every finger is its own pointer — ride CUT and RES
//           with two fingers while the line runs (the acid move). Header is
//           tappable too: < name > pattern, BPM halves tempo, > run/stop.
//   N       roll a fresh random acid line (the honest way to write acid)
//   H       help panel with all of this on screen
//   LEFT / RIGHT  pattern    UP / DOWN  tempo    SPACE  run / stop

#define STEPS 16
#define SLOT  9          // our one instrument slot
#define BASE  36         // C2 — the roll's bottom row

// knobs — named indices (house rule). Values all 0..100, mapped per-knob.
// K_DRV appended at the END (never insert mid-list — reorders cross-wire saved values).
enum { K_CUT, K_RES, K_ENV, K_DEC, K_ACC, K_DRV, K_SWING, K_SQ, NK };
static const char *KNAME[NK] = { "CUT", "RES", "ENV", "DEC", "ACC", "DRV", "SWG", "SQL" };
static int knob[NK] = { 45, 70, 60, 40, 60, 35, 0, 33 };   // SQL = the ex-squelch (env-sweep depth)

// per-step pattern data (the real 303's programming model)
static int  pitches[STEPS];              // semitone 0..12 above BASE
static bool on[STEPS], acc[STEPS], sld[STEPS], tie[STEPS];
static signed char oct[STEPS];           // -1 / 0 / +1 octave transpose (real 303 has both)
static int  plen = STEPS;                // pattern length 1..16 (real 303 runs 1..16)

typedef struct {
    const char *name; int tempo, len;
    const char *nt;    // 16 chars: '.' rest, '0'-'9'/'A'/'B'/'C' = semitone 0-12
    const char *oc;    // 'x' = +1 octave, 'v' = -1 octave, else none
    const char *ac, *sl, *ti;   // ti: 'x' = TIE — hold the previous note through this step
} Pat;

static const Pat PRESET[] = {
    { "ACID 1", 130, 16,
      "0.C03.7A0.C0537A",
      "................",
      "x...x...x...x...",
      "......x.......x.",
      ".x.............." },       // tie: the root sustains over step 1
    { "SQUELCH", 125, 16,
      "0..0..7.0..3..A.",
      ".........v......",       // an octave-down accent note
      "x.......x.......",
      "..x.....x.......",
      "................" },
    { "RANDOM", 132, 16, "", "", "", "", "" },   // generated — press N for a new one
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int   pre = 0, tempo = 130;
static bool  running = true;
static int   last16 = -1, playhead = 0;
static int   h = -1;          // the one voice
static bool  prev_slide;      // did the step we just played carry a slide?
static int   wave = INSTR_SAW;
static bool  show_help;

// per-finger pointer table — every finger drags its own knob, the slider,
// or draws in the roll (the desktop mouse = one synthetic finger)
enum { PTR_IDLE, PTR_KNOB, PTR_SLIDER, PTR_ROLL };
typedef struct { int id, mode, k, lastY; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];      // .id == PTR_NONE → slot free

// the lowpass circuit — one place to swap for the filter-fidelity A/B
// (docs/design/rebirth-classic.md §3: FILTER_LOW vs FILTER_LADDER vs FILTER_STEINER)
#define ACID_FILTER FILTER_DIODE

// ── knob value mappings ──────────────────────────────────────────────────
static int cut_hz(void)  { return (int)(60.0f * powf(2.0f, knob[K_CUT] * 0.06f)); } // 60..3840
static int res_q(void)   { return knob[K_RES] * 15 / 100; }
static int env_hz(void)  { return knob[K_ENV] * 30; }                // 0..3000
static int dec_ms(void)  { return 30 + knob[K_DEC] * 5; }            // 30..530
static float acc_mul(void) { return 1.0f + knob[K_ACC] * 0.015f; }   // 1..2.5
static float drv_x(void)   { return knob[K_DRV] / 100.0f; }          // 0..1
static float sq_mul(void)  { return 1.0f + knob[K_SQ] * 0.02f; }     // 1..3 (the SQL knob)

static void define_voice(void) {
    instrument(SLOT, wave, 2, 60, 6, 25);
    instrument_duty(SLOT, 0.48f);
    instrument_filter(SLOT, ACID_FILTER, cut_hz(), res_q());
    instrument_drive(SLOT, drv_x());
    instrument_echo(SLOT, 0.10f);   // the delay pedal every acid set ran into — subtle slapback
}

static void sync_echo(void) { echo(60000 * 3 / (tempo * 4), 0.3f, 0.35f); }   // dotted-8th

static void gen_random(void) {
    plen = STEPS;
    int prev = 0;
    for (int s = 0; s < STEPS; s++) {
        tie[s] = false;
        on[s] = chance(72);
        // root-heavy minor pentatonic walk — the acid alphabet
        static const int pool[8] = { 0, 0, 0, 3, 5, 7, 10, 12 };
        int p = pool[rnd_between(0, 7)];
        if (chance(35)) p = prev;           // repeated notes groove harder
        pitches[s] = prev = p;
        oct[s] = chance(12) ? (chance(50) ? 1 : -1) : 0;   // up OR down, sometimes
        acc[s] = chance(30);
        sld[s] = chance(25);
    }
    on[0] = true; pitches[0] = 0; tie[0] = false;   // land on the root
    // sprinkle ties onto rests → the prior note sustains (a 303 signature)
    for (int s = 1; s < STEPS; s++)
        if (!on[s] && chance(22)) tie[s] = true;
}

static void load_preset(void) {
    const Pat *p = &PRESET[pre];
    tempo = p->tempo;
    bpm(tempo);
    sync_echo();
    plen = p->len ? p->len : STEPS;
    if (!p->nt[0]) { gen_random(); return; }
    for (int s = 0; s < STEPS; s++) {
        char c = p->nt[s];
        tie[s] = p->ti && p->ti[s] == 'x';
        on[s] = c != '.' && !tie[s];        // a tie step carries no fresh note
        pitches[s] = c >= 'A' ? 10 + c - 'A' : c >= '0' ? c - '0' : 0;
        char o = p->oc[s];
        oct[s] = o == 'x' ? 1 : o == 'v' ? -1 : 0;
        acc[s] = p->ac[s] == 'x';
        sld[s] = p->sl[s] == 'x';
    }
}

static void all_off(void) { if (h >= 0) { note_off(h); h = -1; } prev_slide = false; }

void init(void) {
    PTR_CLEAR(ptr);
    define_voice();
    load_preset();
}

// ── layout ───────────────────────────────────────────────────────────────
#define RX   60    // roll left edge
#define RY   64    // roll top edge
#define RSX  15    // column stride
#define RSY  6     // semitone row height (13 rows: C..C, top = +12) — tightened to fit the TIE row
#define ROWY(r) (RY + (12 - (r)) * RSY)
#define OCTY (RY + 13 * RSY + 4)
#define ACCY (OCTY + 9)
#define SLDY (ACCY + 9)
#define TIEY (SLDY + 9)

static const int KX[NK] = { 22, 60, 98, 136, 174, 212, 250, 22 };   // 7th (SWG) clears the wave box; 8th (SQL) sits under CUT
static const int KYA[NK] = { 38, 38, 38, 38, 38, 38, 38, 92 };      // per-knob Y: the row at 38, SQL down the left margin
#define KY 38
#define KR 11

static void knob_changed(int k) {
    if (k == K_CUT || k == K_RES) {           // live acid: ringing voice follows
        instrument_filter(SLOT, ACID_FILTER, cut_hz(), res_q());
        if (h >= 0) { note_cutoff(h, cut_hz()); note_res(h, res_q()); }
    }
    if (k == K_DRV) {                         // drive too — the squelch screams INTO it
        instrument_drive(SLOT, drv_x());
        if (h >= 0) note_drive(h, drv_x());
    }
    // ENV / DEC / ACC apply at the next trigger
}

void update(void) {
    if (keyp(KEY_SPACE)) { running = !running; last16 = -1; if (!running) all_off(); }
    if (keyp(KEY_LEFT))  { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; all_off(); }
    if (keyp(KEY_RIGHT)) { pre = (pre + 1) % NP;      load_preset(); last16 = -1; all_off(); }
    if (keyp(KEY_UP))   { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); sync_echo(); }
    if (keyp(KEY_DOWN)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); sync_echo(); }
    if (keyp('N')) { gen_random(); }

    int mx = mouse_x(), my = mouse_y();

    // capture guard — the hand-rolled ui_grabbed: while ANY finger is dragging
    // a knob or painting the roll, ignore the header taps + the wheel so a drag
    // can't reach through and change an unrelated control. (Per-finger capture
    // in the ptr pool already isolates the grabbed finger itself; this guards
    // the few hit-tests that live OUTSIDE that pool.)
    bool grabbed = false;
    for (int i = 0; i < PTR_MAX; i++)
        if (ptr[i].id != PTR_NONE && ptr[i].mode != PTR_IDLE) grabbed = true;

    if (show_help) {                       // help swallows the pointer; music keeps playing
        if (keyp('H') || tapp(0, 0, 320, 200)) show_help = false;   // any tap closes
        goto clock;
    }
    if (keyp('H') || (!grabbed && tapp(304, 0, 16, 22))) show_help = true;   // "H" in the top-right corner

    // ── tappable header: < name > pattern, BPM halves tempo, > run/stop ──
    if (!grabbed) {
        if (tapp(146, 0, 18, 22)) { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; all_off(); }
        if (tapp(214, 0, 18, 22)) { pre = (pre + 1) % NP;      load_preset(); last16 = -1; all_off(); }
        if (tapp(166, 0, 46, 22) && !PRESET[pre].nt[0]) gen_random();   // tap RANDOM's name = reroll
        if (tapp(226, 0, 30, 22)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); sync_echo(); }
        if (tapp(256, 0, 28, 22)) { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); sync_echo(); }
        if (tapp(286, 0, 18, 22)) { running = !running; last16 = -1; if (!running) all_off(); }
    }

    // ── touch: every finger is its own pointer — a knob, the slider, the
    // wave switch, or drawing in the roll, all independently and at once
    // (the desktop mouse arrives as one synthetic finger) ─────────────────
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                              // pool full (>PTR_MAX fingers)
        if (fresh) {                                   // finger just landed
            *p = (Ptr){ id, PTR_IDLE, -1, ty };
            for (int k = 0; k < NK; k++) {
                int dx = tx - KX[k], dy = ty - KYA[k];
                if (dx * dx + dy * dy <= (KR + 3) * (KR + 3)) { p->mode = PTR_KNOB; p->k = k; }
            }
            if (p->mode != PTR_IDLE) continue;
            if (tx >= 270 && tx < 306 && ty >= 30 && ty < 48) {        // wave switch
                wave = (wave == INSTR_SAW) ? INSTR_SQUARE : INSTR_SAW;
                define_voice();
                continue;
            }
            if (tx >= RX && tx < RX + STEPS * RSX && ty >= 182 && ty < 195) {   // LENGTH strip (grid-aligned)
                int c = (tx - RX) / RSX; if (c < 0) c = 0; if (c > 15) c = 15;
                plen = c + 1;
                continue;
            }
            int col = (tx - RX) / RSX;
            if (tx >= RX && col >= 0 && col < STEPS) {
                if (ty >= RY && ty < RY + 13 * RSY) {                  // piano roll
                    int row = 12 - (ty - RY) / RSY;
                    if (on[col] && pitches[col] == row) {
                        on[col] = false;                               // tap your note = erase (rest) — NO capture
                    } else {                                           // place/move a note; capture for draw-drag
                        on[col] = true; pitches[col] = row; tie[col] = false;
                        p->mode = PTR_ROLL;                            // ONLY placing grabs the finger, so an
                    }                                                  // erase-tap can't be repainted by the drag branch
                } else {                                               // flag rows: press toggles
                    if (ty >= OCTY && ty < OCTY + 7) oct[col] = oct[col] == 0 ? 1 : oct[col] == 1 ? -1 : 0;  // +1 → -1 → off
                    if (ty >= ACCY && ty < ACCY + 7) acc[col]  = !acc[col];
                    if (ty >= SLDY && ty < SLDY + 7) sld[col]  = !sld[col];
                    if (ty >= TIEY && ty < TIEY + 7) { tie[col] = !tie[col]; if (tie[col]) on[col] = false; }
                }
            }
        } else if (p->mode == PTR_KNOB) {
            if (ty != p->lastY) {
                knob[p->k] += (p->lastY - ty) * 2;
                if (knob[p->k] < 0)   knob[p->k] = 0;
                if (knob[p->k] > 100) knob[p->k] = 100;
                knob_changed(p->k);
            }
        } else if (p->mode == PTR_ROLL) {
            int col = (tx - RX) / RSX;
            if (tx >= RX && col >= 0 && col < STEPS && ty >= RY && ty < RY + 13 * RSY) {
                int row = 12 - (ty - RY) / RSY;
                on[col] = true; pitches[col] = row; tie[col] = false;   // drag paints
            }
        }
        p->lastY = ty;
    }
    for (int i = 0; i < touch_ended_count(); i++) {    // lifted fingers free their slot
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    // ── knobs: hover + wheel still works on desktop ──────────────────────
    float wh = mouse_wheel();
    if (wh != 0.0f && !grabbed)
        for (int k = 0; k < NK; k++) {
            int dx = mx - KX[k], dy = my - KYA[k];
            if (dx * dx + dy * dy <= (KR + 3) * (KR + 3)) {
                knob[k] += wh > 0 ? 4 : -4;
                if (knob[k] < 0)   knob[k] = 0;
                if (knob[k] > 100) knob[k] = 100;
                knob_changed(k);
            }
        }

clock:
    if (!running) return;

    // sixteenth clock with SWING; the held voice can't be scheduled ahead, so
    // steps trigger on the frame the counter flips (like drummachine.c). Swing
    // delays the odd 16ths' onset by up to ~0.6 of a step (the shuffle) — the
    // even steps stay put, so the pair bounces long-short.
    int   raw  = beat() * 4 + (int)(beat_pos() * 4.0f);
    float frac = beat_pos() * 4.0f; frac -= (int)frac;    // 0..1 within the raw 16th
    float sw   = knob[K_SWING] / 100.0f * 0.60f;          // 0..0.6 of a step
    int   trig = ((raw & 1) && frac < sw) ? raw - 1 : raw;  // in the swing gap → prior even step
    int   s    = trig % plen; if (s < 0) s = 0;
    if (trig != last16) {
        last16  = trig;
        playhead = s;
        if (tie[s]) {
            prev_slide = sld[s];                       // hold: no retrigger. SLD on a tie step
                                                       // arms a glide OUT of the held note into
                                                       // the next — long tied notes that slide.
        } else if (on[s]) {
            int midi = BASE + pitches[s] + oct[s] * 12;
            int vol  = acc[s] ? 7 : 5;
            if (h >= 0 && prev_slide) {
                note_glide(h, 60);                     // the slide
                note_pitch(h, (float)midi);
                note_vol(h, vol);                      // env does NOT refire — authentic
            } else {
                if (h >= 0) note_off(h);
                float e = env_hz() * sq_mul() * (acc[s] ? acc_mul() : 1.0f);
                instrument_env(SLOT, 0, ENV_CUTOFF, 0, dec_ms(), e);
                h = note_on(midi, SLOT, vol);
                note_glide(h, 0);
            }
            prev_slide = sld[s];
        } else {
            all_off();                                 // rest releases the gate
        }
    } else if (h >= 0 && on[s] && !sld[s] && !tie[(s + 1) % plen]) {
        // staccato gate: release ~70% through the step — UNLESS the next step
        // ties (then the note sustains into it). Account for the swung onset.
        float onset = (trig & 1) ? sw : 0.0f;
        if (frac > onset + 0.7f * (1.0f - onset)) { note_off(h); h = -1; }
    }
}

void draw(void) {
    char buf[32];

    // silver face, black roll inset — the 303 look
    cls(CLR_LIGHT_GREY);
    rectfill(0, 0, 320, 22, CLR_MEDIUM_GREY);
    print("TB-303 BASS LINE", 14, 8, CLR_BLACK);
    font(FONT_SMALL);                                  // tappable: < name >, BPM halves, run
    print("<", 151, 10, CLR_DARK_GREY);
    print(">", 219, 10, CLR_DARK_GREY);
    font(FONT_NORMAL);
    print(PRESET[pre].name, 160, 8, CLR_DARK_RED);
    sprintf(buf, "%3d BPM", tempo);
    print(buf, 230, 8, CLR_BLACK);
    print(running ? ">" : "#", 296, 8, running ? CLR_DARK_GREEN : CLR_DARK_RED);
    print("H", 310, 8, CLR_DARK_GREY);                 // help — tap the corner (or press H)

    // knobs
    for (int k = 0; k < NK; k++) {
        int ky = KYA[k];
        circfill(KX[k], ky, KR, CLR_BLACK);
        circ(KX[k], ky, KR, CLR_DARK_GREY);
        float a = (-135.0f + 270.0f * knob[k] / 100.0f) * 0.0174533f - 1.5708f;
        line(KX[k], ky, KX[k] + (int)(cosf(a) * (KR - 2)), ky + (int)(sinf(a) * (KR - 2)), CLR_WHITE);
        print(KNAME[k], KX[k] - 11, ky + KR + 4, CLR_BLACK);
    }
    // wave switch
    rectfill(270, 30, 36, 18, CLR_BLACK);
    print(wave == INSTR_SAW ? "SAW" : "SQR", 276, 35, CLR_YELLOW);
    print("WAVE", 270, 52, CLR_BLACK);

    // roll background — white-key rows lighter, C rows marked
    rectfill(RX - 14, RY - 2, 16 * RSX + 16, 13 * RSY + 4, CLR_BLACK);
    for (int r = 0; r <= 12; r++) {
        static const bool whitek[12] = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
        int y = ROWY(r);
        if (whitek[r % 12]) rectfill(RX, y, 16 * RSX - 2, RSY - 1, CLR_BROWNISH_BLACK);
        rectfill(RX - 12, y, 10, RSY - 1, whitek[r % 12] ? CLR_WHITE : CLR_DARK_GREY);
        if (r % 12 == 0) print("C", RX - 10, y, CLR_BLACK);
    }
    // playhead
    if (running)
        rectfill(RX + playhead * RSX - 1, RY - 2, RSX - 1, 13 * RSY + 2, CLR_DARKER_GREY);

    // notes + slide connectors
    for (int s = 0; s < STEPS; s++) {
        if (!on[s]) continue;
        int x = RX + s * RSX, y = ROWY(pitches[s]);
        int c = (running && s == playhead) ? CLR_WHITE : acc[s] ? CLR_RED : CLR_ORANGE;
        rectfill(x, y, RSX - 3, RSY - 1, c);
        if (oct[s] > 0) rect(x, y, RSX - 3, RSY - 1, CLR_BLUE);          // +1 octave
        else if (oct[s] < 0) rect(x, y, RSX - 3, RSY - 1, CLR_INDIGO);  // -1 octave
        // connectors/tie-bars are LINEAR (s+1) and stay INSIDE the loop (< plen):
        // a %plen wrap drew stray slide lines + phantom tie bars from the last
        // step back across the grid whenever LEN < 16.
        int nx = s + 1;
        if (nx < plen) {
            if (sld[s] && on[nx]) {                    // connector into next note
                int y2 = ROWY(pitches[nx]);
                line(x + RSX - 3, y + 3, x + RSX, y2 + 3, CLR_LIGHT_PEACH);
            }
            if (tie[nx]) {
                rectfill(x + RSX - 3, y + 1, 3 + (RSX - 3), RSY - 3, c);       // held into a tie
                int nx2 = nx + 1;
                if (nx2 < plen && sld[nx] && on[nx2]) {  // the held note slides on OUT of the tie
                    int xt = RX + nx * RSX, y2 = ROWY(pitches[nx2]);
                    line(xt + RSX - 3, y + 3, xt + RSX, y2 + 3, CLR_LIGHT_PEACH);
                }
            }
        }
    }

    // OCT flag row (tri-state: +1 up / -1 down / off)
    print("OCT", RX - 36, OCTY, CLR_BLACK);
    for (int s = 0; s < STEPS; s++) {
        int x = RX + s * RSX;
        if (oct[s] > 0)      rectfill(x, OCTY, RSX - 3, 6, CLR_BLUE);
        else if (oct[s] < 0) rectfill(x, OCTY, RSX - 3, 6, CLR_INDIGO);
        else                 rect(x, OCTY, RSX - 3, 6, CLR_MEDIUM_GREY);
    }
    // ACC / SLD / TIE flag rows
    static const char *FL[3] = { "ACC", "SLD", "TIE" };
    static const int   FY[3] = { ACCY, SLDY, TIEY };
    static const int   FC[3] = { CLR_RED, CLR_GREEN, CLR_YELLOW };
    for (int f = 0; f < 3; f++) {
        print(FL[f], RX - 36, FY[f], CLR_BLACK);
        bool *arr = f == 0 ? acc : f == 1 ? sld : tie;
        for (int s = 0; s < STEPS; s++) {
            int x = RX + s * RSX;
            if (arr[s]) rectfill(x, FY[f], RSX - 3, 6, FC[f]);
            else        rect(x, FY[f], RSX - 3, 6, CLR_MEDIUM_GREY);
        }
    }
    // loop divider — everything right of it is outside the pattern
    if (plen < STEPS) {
        int dx = RX + plen * RSX - 1;
        line(dx, RY - 2, dx, TIEY + 6, CLR_DARK_RED);
    }

    // LENGTH strip — one cell per step, aligned under the roll columns so it
    // lines up with the grid. Green = in the pattern, bright cap = last step;
    // the red divider in the roll marks the same cut. (Label only — the lit
    // cells + divider read the length; no number needed.)
    print("LEN", RX - 36, 184, CLR_BLACK);
    for (int s = 0; s < STEPS; s++) {
        int x = RX + s * RSX;
        rectfill(x, 184, RSX - 3, 8, s < plen ? (s == plen - 1 ? CLR_GREEN : CLR_DARK_GREEN)
                                              : CLR_DARKER_GREY);
    }

    if (show_help) {
        rectfill(30, 22, 260, 172, CLR_BLACK);
        rect(30, 22, 260, 172, CLR_LIGHT_GREY);
        print("TB-303 CONTROLS", 95, 28, CLR_YELLOW);
        static const char *HL[] = {
            "ROLL      DRAW/DRAG NOTES",
            "          CLICK NOTE = ERASE",
            "LEN BAR   SET PATTERN LENGTH",
            "OCT       TAP: +1 / -1 / OFF",
            "ACC       ACCENT: LOUD+SHARP",
            "SLD       SLIDE TO NEXT NOTE",
            "TIE       HOLD PREV NOTE (SUSTAIN)",
            "SWG KNOB  SWING THE OFF-16THS",
            "DRV       POST-FILTER GRIT",
            "SQL KNOB  FILTER-ENV DEPTH",
            "WAVE BOX  SAW / SQR   N NEW RND",
            "< > PATTERN  ^v TEMPO (TAP)",
            "SPACE/TAP > RUN/STOP  H CLOSE",
        };
        int n = (int)(sizeof HL / sizeof HL[0]);
        for (int i = 0; i < n; i++)
            print(HL[i], 42, 42 + i * 11, i < 8 ? CLR_WHITE : CLR_LIGHT_PEACH);
    }
}
