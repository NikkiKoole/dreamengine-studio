/* de:meta
{
  "slug": "latchbox",
  "title": "latch box",
  "status": "active",
  "created": "2026-10-01",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "generative-sequencer",
    "granular-synth"
  ],
  "resizable": true,
  "lineage": "The demo for docs/design/chompi-harvest.md: what we took from CHOMPI's MIT open-source release (CHOMPI Club / Chase Bliss). TEMPO's pattern generator (runtime/patgen.h), TAPE's warble (tape_warble), TEMPO's clock-locked freeze (grains_repeat), TAPE's one-knob delay and snap-to-fifths pitch. Not a CHOMPI clone and not named like one: the name and artwork are not licensed.",
  "description": {
    "summary": "Four latched keys and a rest pattern that takes steps but not notes, so the line keeps changing shape. Tap pads to reshape it.",
    "detail": "Tap pads to latch notes. They play back on the clock in the order you pressed them (or UP / DOWN / PING-PONG / RANDOM). A REST pattern takes steps away without taking notes, so with four keys held against XXX. the gap drifts through the line and it takes sixteen steps to come round. Add or lift one key and the whole line reshapes. OCT gives each note a chance to jump an octave. Under it, a worn tape: TAPE drives it (level-compensated, so it gets darker, not louder), WARBLE makes the pitch sag at random moments. DELAY is one knob: feedback also sets the mix. STUTTER is a beat-locked repeat of the last 1/4, 1/8 or 1/16, on the beat however late you hit it.",
    "controls": "Tap pads to latch/unlatch (or A W S E D F T G Y H U J). Z/X octave. ORDER / REST / DIV cycle the pattern (1 / 2 / 3). Knobs: PITCH (snaps to fifths + octaves), OCT, TAPE, WARBLE, DELAY. STUTTER toggles the repeat (or hold SPACE); LEN picks its length (4). C clears."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "face.h"
#include "patgen.h"
#include <math.h>

// LATCH BOX — chompi-harvest.md in one cart.
//
//   the pads        latch notes into a PatGen (runtime/patgen.h, TEMPO's pattern generator)
//   the clock       books each step on the SOUND clock (audio_time + schedule_at), so a 1/16 line
//                   stays tight however frames and audio buffers line up
//   the tape        tape(0, 0, sat) + tape_warble(): sparse random sags instead of a steady wobble
//   the delay       echo_insert driven by ONE knob, TAPE's macro: feedback also sets the mix
//   the stutter     grains_repeat + grains_freeze: a beat repeat at the END of the chain
//
// Every effect is SET-AND-HOLD: apply_fx() re-sends a value only when it changed.

#define SL_LEAD   5
#define BPM       112
#define NPAD      12
#define DESIGN_W  200
#define DESIGN_H  320

static PatGen pg;
static int    base_oct = 4;          // pads play C(base_oct)..B
static int    div_i    = 3;          // PG_DIV_BEATS index: 1/16
static float  k_pitch  = 0.5f;       // PITCH knob, snapped to TRANSPOSE[]
static float  k_oct    = 0.0f;
static float  k_tape   = 0.35f;
static float  k_warble = 0.25f;
static float  k_delay  = 0.3f;
static int    stutter, rep_i = 1;    // REPEAT_BEATS index
static double next_t = -1.0;         // sound-clock time of the next step (-1 = stopped)
static double stut_t = -1.0;         // sound-clock time the stutter froze (the line after it is not heard)

// TAPE's quantised pitch knob: the snap points are fifths and octaves, so a sweep never lands on a
// note that clashes with the line.
static const int   TRANSPOSE[5]    = { -12, -7, 0, 7, 12 };
static const char *const TNAME[5]  = { "-oct", "-5th", "0", "+5th", "+oct" };
static const float REPEAT_BEATS[3] = { 0.25f, 0.5f, 1.0f };
static const char *const RNAME[3]  = { "1/16", "1/8", "1/4" };
static const char *const ONAME[PG_NORDER] = { "PRESS", "UP", "DOWN", "P-PONG", "RANDOM" };

// what the lane shows: the last steps, booked ahead, lit when the sound clock reaches them
#define NHIST 24
static struct { double t; int midi; } hist[NHIST];
static int hist_n;
static int nsteps;                   // steps booked so far (the trace's step clock)

static int transpose(void) { int i = (int)(k_pitch * 4.999f); return TRANSPOSE[i]; }

// ── effects: change-gated, the groovebox apply_fx() pattern ──
static float a_tape = -1, a_warble = -1, a_delay = -1, a_rep = -1;
static int   a_stut = -1;
static void apply_fx(void) {
    if (k_tape != a_tape) {
        float sat = k_tape;
        tape(0.0f, 0.0f, sat);
        // tape()'s clipper is normalised for a full-scale PEAK, so a quieter line comes out LOUDER
        // (g/tanh(g): +5.6 dB at 0.4, measured — chompi-harvest §7). Trim it back by the same factor
        // so the knob changes the colour, not the volume. TAPE's DSPEngine does the same after its clip.
        float g = 1.0f + sat * 2.0f;
        instrument_level(SL_LEAD, sat > 0.0f ? tanhf(g) / g : 1.0f);
        a_tape = k_tape;
    }
    if (k_warble != a_warble) { tape_warble(k_warble); a_warble = k_warble; }
    if (k_delay != a_delay) {
        // TAPE's one-knob delay. Feedback is the knob; wet rises FAST to 50 % and dry falls SLOWLY
        // to 50 %, so the first quarter of the knob is "a little slapback" and the top is a wash.
        float fb  = k_delay * 0.9f;
        float wet = fb > 0.25f ? 0.5f : 2.0f * fb;
        float dry = fb > 0.83f ? 0.5f : 1.0f - 0.6f * fb;
        int   ms  = (int)(60000.0f / BPM * 0.75f);            // dotted 8th
        echo_insert(ms, fb, 0.45f, wet / (wet + dry));
        a_delay = k_delay;
    }
    float rep = REPEAT_BEATS[rep_i];
    if (rep != a_rep) { grains_repeat(rep); a_rep = rep; }
    if (stutter != a_stut) { grains_freeze(stutter); a_stut = stutter; stut_t = stutter ? audio_time() : -1.0; }
}

// ── the clock: book every step that falls in the next 100 ms ──
static void run_clock(void) {
    double now_t = audio_time();
    if (pg.n == 0) { next_t = -1.0; return; }
    if (next_t < 0.0) { next_t = now_t + 0.05; pg_restart(&pg); }   // first latch starts the run
    double step = 60.0 / BPM * PG_DIV_BEATS[div_i];                 // read per booking = a DIV change
    while (next_t < now_t + 0.10) {                                 //   lands on the next edge, not mid-step
        int midi = pg_step(&pg);
        if (midi != PG_REST) {
            midi += transpose();
            int gate = (int)(step * 1000.0 * 0.7);
            schedule_at(next_t, midi, SL_LEAD, 6, gate < 40 ? 40 : gate);
        }
        if (hist_n == NHIST) { for (int i = 1; i < NHIST; i++) hist[i - 1] = hist[i]; hist_n--; }
        hist[hist_n].t = next_t; hist[hist_n].midi = midi; hist_n++;
        next_t += step;
        nsteps++;
    }
}

// ── layout ──
static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.08f, "nav"     },
    { FACE_HERO, 0,           0.00f, "lane"    },
    { FACE_BAND, EDGE_BOTTOM, 0.22f, "pads"    },
    { FACE_BAND, EDGE_BOTTOM, 0.09f, "pattern" },
    { FACE_BAND, EDGE_BOTTOM, 0.15f, "knobs"   },
    { FACE_BAND, EDGE_BOTTOM, 0.10f, "stutter" },
};
#define NZ 6
static Face g_face;
static Box  z_nav, z_lane, z_pads, z_pat, z_knobs, z_stut;

static void relayout(void) {
    face_resize_to(DESIGN_W, DESIGN_H);
    g_face  = face_layout(face_area(3), ZONES, NZ, 12);
    z_nav   = g_face.box[0];
    z_lane  = g_face.box[1];
    z_pads  = g_face.box[2];
    z_pat   = g_face.box[3];
    z_knobs = g_face.box[4];
    z_stut  = g_face.box[5];
}

static const char QWERTY[NPAD] = { 'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J' };
static const char *const NOTE[NPAD] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static int pad_midi(int i) { return 12 * (base_oct + 1) + i; }

void init(void) {
    bpm(BPM);                                        // grains_repeat measures its beats in this tempo
    instrument(SL_LEAD, INSTR_PLUCK, 2, 260, 0, 180);
    grains(100, 10, 1.0f, 0.0f, 0.0f, 1.0f);         // the tank the stutter lives in (repeat mode = no cloud)
    pg_init(&pg, 2026);
    pg.order = PG_SEQ; pg.rest = PG_REST_3_1;
    // boot already playing: four keys against XXX., so the very first thing a stranger hears is the
    // gap drifting through the line. Tap a lit pad to lift it and hear the whole line reshape.
    pg_add(&pg, 60); pg_add(&pg, 64); pg_add(&pg, 67); pg_add(&pg, 71);
    apply_fx();
    int chain[] = { FX_TAPE, FX_ECHO, FX_GRAINS };   // tape → delay → stutter: the repeat catches the echoes
    fx_order(0, chain, 3);
}

void update(void) {
    relayout();
    for (int i = 0; i < NPAD; i++) if (keyp(QWERTY[i])) pg_toggle(&pg, pad_midi(i));
    if (keyp('Z') && base_oct > 2) base_oct--;
    if (keyp('X') && base_oct < 6) base_oct++;
    if (keyp('1')) pg.order = (pg.order + 1) % PG_NORDER;
    if (keyp('2')) pg.rest  = (pg.rest + 1) % PG_NREST;
    if (keyp('3')) div_i    = (div_i + 1) % 5;
    if (keyp('4')) rep_i    = (rep_i + 1) % 3;
    if (keyp('C')) pg_clear(&pg);
    if (keyp(' ')) stutter = 1;
    if (keyr(' ')) stutter = 0;
    pg.octave_chance = k_oct;
    apply_fx();
    run_clock();
#ifdef DE_TRACE
    watch("held", "%d", pg.n);
    watch("order", "%d", pg.order);
    watch("rest", "%d", pg.rest);
    watch("last", "%d", pg.last);
    watch("step", "%d", nsteps);
    watch("stutter", "%d", stutter);
#endif
}

static void draw_lane(void) {
    Box z = z_lane;
    rectfill((int)z.x, (int)z.y, (int)z.w, (int)z.h, stutter ? CLR_DARK_BLUE : CLR_BLACK);
    rect((int)z.x, (int)z.y, (int)z.w, (int)z.h, stutter ? CLR_YELLOW : CLR_INDIGO);
    // the rest mask: 20 dots, the current step ringed
    font(FONT_TINY);
    print(PG_REST_NAME[pg.rest], (int)z.x + 4, (int)z.y + 4, CLR_DARK_GREY);
    float dw = (z.w - 8) / PG_REST_STEPS;
    for (int i = 0; i < PG_REST_STEPS; i++) {
        int on = (PG_REST_MASK[pg.rest] >> i) & 1u;
        int cx = (int)(z.x + 4 + dw * (i + 0.5f)), cy = (int)z.y + 14;
        if (on) circfill(cx, cy, 2, CLR_MAUVE); else circ(cx, cy, 2, CLR_DARK_GREY);
        if (pg.n && (pg.rest_idx + PG_REST_STEPS - 1) % PG_REST_STEPS == i) circ(cx, cy, 4, CLR_WHITE);
    }
    // the phrase: one column per booked step, height = pitch; lit once the sound clock reaches it
    double t = audio_time();
    int top = (int)z.y + 24, h = (int)z.h - 30;
    float cw = (z.w - 8) / NHIST;
    for (int i = 0; i < hist_n; i++) {
        int x = (int)(z.x + 4 + cw * i);
        if (hist[i].midi == PG_REST) { line(x + 1, top + h - 1, x + (int)cw - 2, top + h - 1, CLR_DARK_GREY); continue; }
        int k = hist[i].midi - 12 * (base_oct + 1) + 12;            // -12..+35 around the pads
        int y = top + h - 3 - (int)((k < 0 ? 0 : k > 36 ? 36 : k) * (h - 6) / 36.0f);
        int lit = hist[i].t <= t;
        if (stut_t >= 0.0 && hist[i].t >= stut_t) {   // booked after the freeze: playing INTO the tank, unheard
            rectfill(x + 1, y, (int)cw - 2, 3, CLR_DARK_GREY); continue;
        }
        int playing = lit && (i == hist_n - 1 || hist[i + 1].t > t);
        rectfill(x + 1, y, (int)cw - 2, 3, playing ? CLR_WHITE : lit ? CLR_PEACH : CLR_DARK_PURPLE);
    }
    if (pg.n == 0) print("LATCH A FEW KEYS", (int)z.x + 4, (int)(z.y + z.h / 2), CLR_DARK_GREY);
    if (stutter) print(str("REPEATING THE LAST %s", RNAME[rep_i]), (int)z.x + 4, (int)(z.y + z.h) - 9, CLR_YELLOW);
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    rect(1, 1, screen_w() - 2, screen_h() - 2, CLR_DARK_GREY);
    font(FONT_SMALL);
    print("LATCH BOX", (int)z_nav.x + 4, (int)z_nav.y + 3, CLR_LIGHT_YELLOW);
    print(str("%d BPM  %s", BPM, PG_DIV_NAME[div_i]), (int)(z_nav.x + z_nav.w) - 64, (int)z_nav.y + 3, CLR_LIGHT_GREY);

    draw_lane();

    ui_begin();
    // pads: 2 rows of 6, a latched pad is lit
    float gap = 3.0f, pw = (z_pads.w - gap * 7) / 6, ph = (z_pads.h - gap * 3) / 2;
    for (int i = 0; i < NPAD; i++) {
        int r = i / 6, c = i % 6;
        Box b = box(z_pads.x + gap + c * (pw + gap), z_pads.y + gap + r * (ph + gap), pw, ph);
        int m = pad_midi(i);
        if (ui_button_cell(b, NOTE[i])) pg_toggle(&pg, m);
        if (pg_has(&pg, m)) {                      // latched: a lit bar; white while it is the sounding note
            int sounding = pg.last != PG_REST && ((pg.last - transpose()) % 12 + 12) % 12 == i;
            rectfill((int)b.x + 2, (int)(b.y + b.h) - 5, (int)b.w - 4, 3, sounding ? CLR_WHITE : CLR_MAUVE);
            rect((int)b.x - 1, (int)b.y - 1, (int)b.w + 2, (int)b.h + 2, CLR_MAUVE);
        }
    }
    // pattern row: ORDER · REST · DIV
    Box pr = lay_inset(z_pat, 2);
    if (ui_button_cell(lay_cell(pr, 0, 3, 0, 3), ONAME[pg.order]))       pg.order = (pg.order + 1) % PG_NORDER;
    if (ui_button_cell(lay_cell(pr, 0, 3, 1, 3), PG_REST_NAME[pg.rest])) pg.rest  = (pg.rest + 1) % PG_NREST;
    if (ui_button_cell(lay_cell(pr, 0, 3, 2, 3), PG_DIV_NAME[div_i]))    div_i    = (div_i + 1) % 5;
    // knobs
    Box kr = lay_inset(z_knobs, 1);
    ui_knob_cell(lay_cell(kr, 0, 5, 0, 2), &k_pitch,  TNAME[(int)(k_pitch * 4.999f)]);
    ui_knob_cell(lay_cell(kr, 0, 5, 1, 2), &k_oct,    "OCT");
    ui_knob_cell(lay_cell(kr, 0, 5, 2, 2), &k_tape,   "TAPE");
    ui_knob_cell(lay_cell(kr, 0, 5, 3, 2), &k_warble, "WARBLE");
    ui_knob_cell(lay_cell(kr, 0, 5, 4, 2), &k_delay,  "DELAY");
    // stutter
    Box sr = lay_inset(z_stut, 2);
    Box sb = box(sr.x, sr.y, sr.w * 0.72f, sr.h);
    Box lb = box(sr.x + sr.w * 0.75f, sr.y, sr.w * 0.25f, sr.h);
    if (ui_button_cell(sb, stutter ? "STUTTERING" : "STUTTER")) stutter = !stutter;
    if (ui_button_cell(lb, RNAME[rep_i])) rep_i = (rep_i + 1) % 3;
    ui_end();
    if (stutter) rect((int)sb.x - 1, (int)sb.y - 1, (int)sb.w + 2, (int)sb.h + 2, CLR_YELLOW);
}

#ifdef DE_SPEC
void spec(void) {
    patgen_selfcheck();
    // the cart-side half: the pitch knob snaps to fifths and octaves and nothing else
    float probe[5] = { 0.0f, 0.3f, 0.5f, 0.7f, 1.0f };
    int want[5] = { -12, -7, 0, 7, 12 };
    for (int i = 0; i < 5; i++) { k_pitch = probe[i]; expect_eq(transpose(), want[i], "PITCH snaps to fifths and octaves"); }
    k_pitch = 0.5f;
    // and the one-knob delay is a bypass at zero (TAPE's macro: no feedback = no wet at all)
    float fb = 0.0f, wet = fb > 0.25f ? 0.5f : 2.0f * fb;
    expect(wet == 0.0f, "DELAY at zero is dry");
}
#endif
