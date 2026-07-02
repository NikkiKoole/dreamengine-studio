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
  "description": "The acid machine — third box in the classic-machine family (cr-78, tr-808), but a monosynth, not a drum kit: ONE held voice (saw or square) through a resonant lowpass — the engine's FILTER_DIODE, a real diode-ladder model (~18dB/oct, bass drains as the resonance climbs, and the resonance saturates inside the loop so it growls, the way a 303 does) — sequenced from a mouse-drawn piano roll. All the 303 signatures: slide (a slid step doesn't retrigger — note_glide carries the pitch into the next note while the filter envelope keeps decaying, exactly like the real circuit), accent (louder + a harder filter kick), staccato gating at 70% of the step, and six draggable knobs — CUTOFF and RESO apply live to the ringing voice via note_cutoff/note_res (the entire point of acid), and DRV is the new instrument_drive/note_drive saturation: tanh AFTER the filter, so the resonant peak screams into it — RES + DRV up is the proper acid bite. Press N for a fresh random acid line (root-heavy minor pentatonic walk with random accents and slides, the honest way it was always done). The SQUELCH slider multiplies the env sweep up to 3x (full ENV + full slider = a 9kHz scream), and H opens a help panel with every control. Two authored patterns + the generator; LEFT/RIGHT pattern, UP/DOWN tempo, SPACE run/stop."
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
//           erase it. Rows below the roll: OCT (+1 octave), ACC (accent),
//           SLD (slide into next step). Knobs: drag vertically (or hover +
//           wheel on desktop). The SAW/SQR switch toggles the wave. The
//           SQUELCH slider (bottom) multiplies the filter-env sweep up to
//           3x — ENV knob full + slider full = the 9kHz scream.
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
enum { K_CUT, K_RES, K_ENV, K_DEC, K_ACC, K_DRV, NK };
static const char *KNAME[NK] = { "CUT", "RES", "ENV", "DEC", "ACC", "DRV" };
static int knob[NK] = { 45, 70, 60, 40, 60, 35 };

// per-step pattern data (the real 303's programming model)
static int  pitches[STEPS];   // semitone 0..12 above BASE
static bool on[STEPS], octv[STEPS], acc[STEPS], sld[STEPS];

typedef struct {
    const char *name; int tempo;
    const char *nt;    // 16 chars: '.' rest, '0'-'9'/'A'/'B'/'C' = semitone 0-12
    const char *oc, *ac, *sl;
} Pat;

static const Pat PRESET[] = {
    { "ACID 1", 130,
      "0.C03.7A0.C0537A",
      "................",
      "x...x...x...x...",
      "......x.......x." },
    { "SQUELCH", 125,
      "0..0..7.0..3..A.",
      ".........x......",
      "x.......x.......",
      "..x.....x......." },
    { "RANDOM", 132, "", "", "", "" },   // generated — press N for a new one
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int   pre = 0, tempo = 130;
static bool  running = true;
static int   last16 = -1, playhead = 0;
static int   h = -1;          // the one voice
static bool  prev_slide;      // did the step we just played carry a slide?
static int   wave = INSTR_SAW;
static int   squelch = 33;    // 0..100 → 1x..3x filter-env depth
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
static float sq_mul(void)  { return 1.0f + squelch * 0.02f; }        // 1..3

static void define_voice(void) {
    instrument(SLOT, wave, 2, 60, 6, 25);
    instrument_duty(SLOT, 0.48f);
    instrument_filter(SLOT, ACID_FILTER, cut_hz(), res_q());
    instrument_drive(SLOT, drv_x());
    instrument_echo(SLOT, 0.10f);   // the delay pedal every acid set ran into — subtle slapback
}

static void sync_echo(void) { echo(60000 * 3 / (tempo * 4), 0.3f, 0.35f); }   // dotted-8th

static void gen_random(void) {
    int prev = 0;
    for (int s = 0; s < STEPS; s++) {
        on[s] = chance(72);
        // root-heavy minor pentatonic walk — the acid alphabet
        static const int pool[8] = { 0, 0, 0, 3, 5, 7, 10, 12 };
        int p = pool[rnd_between(0, 7)];
        if (chance(35)) p = prev;           // repeated notes groove harder
        pitches[s] = prev = p;
        octv[s] = chance(15);
        acc[s]  = chance(30);
        sld[s]  = chance(25);
    }
    on[0] = true; pitches[0] = 0;           // land on the root
}

static void load_preset(void) {
    const Pat *p = &PRESET[pre];
    tempo = p->tempo;
    bpm(tempo);
    sync_echo();
    if (!p->nt[0]) { gen_random(); return; }
    for (int s = 0; s < STEPS; s++) {
        char c = p->nt[s];
        on[s] = c != '.';
        pitches[s] = c >= 'A' ? 10 + c - 'A' : c >= '0' ? c - '0' : 0;
        octv[s] = p->oc[s] == 'x';
        acc[s]  = p->ac[s] == 'x';
        sld[s]  = p->sl[s] == 'x';
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
#define RSY  7     // semitone row height (13 rows: C..C, top = +12)
#define ROWY(r) (RY + (12 - (r)) * RSY)
#define OCTY (RY + 13 * RSY + 4)
#define ACCY (OCTY + 9)
#define SLDY (ACCY + 9)

static const int KX[NK] = { 22, 66, 110, 154, 198, 242 };
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

    if (show_help) {                       // help swallows the pointer; music keeps playing
        if (keyp('H') || tapp(0, 0, 320, 200)) show_help = false;   // any tap closes
        goto clock;
    }
    if (keyp('H') || tapp(258, 178, 56, 18)) show_help = true;

    // ── tappable header: < name > pattern, BPM halves tempo, > run/stop ──
    if (tapp(146, 0, 18, 22)) { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; all_off(); }
    if (tapp(214, 0, 18, 22)) { pre = (pre + 1) % NP;      load_preset(); last16 = -1; all_off(); }
    if (tapp(166, 0, 46, 22) && !PRESET[pre].nt[0]) gen_random();   // tap RANDOM's name = reroll
    if (tapp(226, 0, 30, 22)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); sync_echo(); }
    if (tapp(256, 0, 34, 22)) { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); sync_echo(); }
    if (tapp(292, 0, 28, 22)) { running = !running; last16 = -1; if (!running) all_off(); }

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
                int dx = tx - KX[k], dy = ty - KY;
                if (dx * dx + dy * dy <= (KR + 3) * (KR + 3)) { p->mode = PTR_KNOB; p->k = k; }
            }
            if (p->mode != PTR_IDLE) continue;
            if (tx >= 270 && tx < 306 && ty >= 30 && ty < 48) {        // wave switch
                wave = (wave == INSTR_SAW) ? INSTR_SQUARE : INSTR_SAW;
                define_voice();
                continue;
            }
            if (tx >= 76 && tx < 252 && ty >= 180 && ty < 194) {       // squelch slider
                p->mode = PTR_SLIDER;
                continue;                                              // value set below next frame
            }
            int col = (tx - RX) / RSX;
            if (tx >= RX && col >= 0 && col < STEPS) {
                if (ty >= RY && ty < RY + 13 * RSY) {                  // piano roll
                    int row = 12 - (ty - RY) / RSY;
                    if (on[col] && pitches[col] == row) on[col] = false;   // press your note = erase
                    else { on[col] = true; pitches[col] = row; }
                    p->mode = PTR_ROLL;
                } else {                                               // flag rows: press toggles
                    if (ty >= OCTY && ty < OCTY + 7) octv[col] = !octv[col];
                    if (ty >= ACCY && ty < ACCY + 7) acc[col]  = !acc[col];
                    if (ty >= SLDY && ty < SLDY + 7) sld[col]  = !sld[col];
                }
            }
        } else if (p->mode == PTR_KNOB) {
            if (ty != p->lastY) {
                knob[p->k] += (p->lastY - ty) * 2;
                if (knob[p->k] < 0)   knob[p->k] = 0;
                if (knob[p->k] > 100) knob[p->k] = 100;
                knob_changed(p->k);
            }
        } else if (p->mode == PTR_SLIDER) {
            squelch = (tx - 80) * 100 / 168;
            if (squelch < 0)   squelch = 0;
            if (squelch > 100) squelch = 100;
        } else if (p->mode == PTR_ROLL) {
            int col = (tx - RX) / RSX;
            if (tx >= RX && col >= 0 && col < STEPS && ty >= RY && ty < RY + 13 * RSY) {
                int row = 12 - (ty - RY) / RSY;
                on[col] = true; pitches[col] = row;    // drag paints
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
    if (wh != 0.0f)
        for (int k = 0; k < NK; k++) {
            int dx = mx - KX[k], dy = my - KY;
            if (dx * dx + dy * dy <= (KR + 3) * (KR + 3)) {
                knob[k] += wh > 0 ? 4 : -4;
                if (knob[k] < 0)   knob[k] = 0;
                if (knob[k] > 100) knob[k] = 100;
                knob_changed(k);
            }
        }

clock:
    if (!running) return;

    // sixteenth clock; the held voice can't be scheduled ahead, so steps
    // trigger on the frame the counter flips (like drummachine.c)
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    int s   = s16 & 15;
    if (s16 != last16) {
        last16  = s16;
        playhead = s;
        if (on[s]) {
            int midi = BASE + pitches[s] + (octv[s] ? 12 : 0);
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
    } else if (h >= 0 && on[s] && !sld[s]) {
        // staccato gate: non-slid notes release at ~70% of the step
        float f = beat_pos() * 4.0f; f -= (int)f;
        if (f > 0.7f) { note_off(h); h = -1; }
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
    print(running ? ">" : "#", 300, 8, running ? CLR_DARK_GREEN : CLR_DARK_RED);

    // knobs
    for (int k = 0; k < NK; k++) {
        circfill(KX[k], KY, KR, CLR_BLACK);
        circ(KX[k], KY, KR, CLR_DARK_GREY);
        float a = (-135.0f + 270.0f * knob[k] / 100.0f) * 0.0174533f - 1.5708f;
        line(KX[k], KY, KX[k] + (int)(cosf(a) * (KR - 2)), KY + (int)(sinf(a) * (KR - 2)), CLR_WHITE);
        print(KNAME[k], KX[k] - 11, KY + KR + 4, CLR_BLACK);
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
        if (octv[s]) rect(x, y, RSX - 3, RSY - 1, CLR_BLUE);
        if (sld[s] && on[(s + 1) & 15]) {              // connector into next note
            int y2 = ROWY(pitches[(s + 1) & 15]);
            line(x + RSX - 3, y + 3, x + RSX, y2 + 3, CLR_LIGHT_PEACH);
        }
    }

    // flag rows
    static const char *FL[3] = { "OCT", "ACC", "SLD" };
    static const int   FC[3] = { CLR_BLUE, CLR_RED, CLR_GREEN };
    for (int f = 0; f < 3; f++) {
        int y = f == 0 ? OCTY : f == 1 ? ACCY : SLDY;
        print(FL[f], RX - 36, y, CLR_BLACK);
        bool *arr = f == 0 ? octv : f == 1 ? acc : sld;
        for (int s = 0; s < STEPS; s++) {
            int x = RX + s * RSX;
            if (arr[s]) rectfill(x, y, RSX - 3, 6, FC[f]);
            else        rect(x, y, RSX - 3, 6, CLR_MEDIUM_GREY);
        }
    }

    // squelch slider — depth multiplier on the filter-env sweep
    print("SQUELCH", 14, 184, CLR_BLACK);
    rectfill(80, 186, 168, 3, CLR_DARK_GREY);
    rectfill(80, 186, squelch * 168 / 100, 3, CLR_DARK_RED);
    rectfill(78 + squelch * 168 / 100, 182, 5, 11, CLR_BLACK);
    print("H HELP", 262, 184, CLR_DARK_GREY);

    if (show_help) {
        rectfill(40, 28, 240, 152, CLR_BLACK);
        rect(40, 28, 240, 152, CLR_LIGHT_GREY);
        print("TB-303 CONTROLS", 100, 36, CLR_YELLOW);
        static const char *HL[] = {
            "ROLL      DRAW/DRAG NOTES",
            "          CLICK NOTE = ERASE",
            "OCT       STEP UP ONE OCTAVE",
            "ACC       ACCENT: LOUD+SHARP",
            "SLD       SLIDE TO NEXT NOTE",
            "KNOBS     DRAG OR WHEEL",
            "DRV       POST-FILTER GRIT",
            "SQUELCH   FILTER-ENV DEPTH",
            "WAVE BOX  SAW / SQR",
            "N         NEW RANDOM (TAP NAME)",
            "< > PATTERN  ^v TEMPO (TAP)",
            "SPACE/TAP > RUN/STOP  H CLOSE",
        };
        for (int i = 0; i < 12; i++)
            print(HL[i], 52, 50 + i * 11, i < 9 ? CLR_WHITE : CLR_LIGHT_PEACH);
    }
}
