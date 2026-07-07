/* de:meta
{
  "title": "Circle Machine",
  "status": "active",
  "created": "2026-07-07",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "generative-melody",
    "scale-quantize",
    "subtractive-synth"
  ],
  "lineage": "After Raymond Scott's Circle Machine (~1959, Manhattan Research Inc.) — a step sequencer a decade before the word. Novel here: the sequencer is a RING swept by a rotating photocell arm, not a left-to-right grid, and each bulb's BRIGHTNESS sets both pitch (up the scale) and accent — the one control the real machine had.",
  "homage": "Raymond Scott — Circle Machine",
  "description": {
    "summary": "Raymond Scott's Circle Machine: a ring of glowing bulbs swept by a rotating photocell arm — a step sequencer from 1959. Each bulb's brightness sets its note (brighter = higher, up the scale); drag it dark for a rest. The arm spins at tempo and plays whatever it passes, round and round.",
    "detail": "Not a keyboard — a machine you configure and let cycle, the way Scott actually worked. Drag a bulb up/down to set its pitch (snapped to the scale so it always plays in tune); drag it to the bottom for a rest. The photocell arm sweeps the ring; rotation speed IS the tempo. Three voices on the same ring: RING (a clean electronic bleep), GLIDE (the Clavivox — one voice that slides between notes, portamento), and BONGO (the Bandito auto-drummer — pitched electronic percussion). RINGMOD adds Scott's signature metallic clang; REVERB and ECHO are his tape space. NUDGE mutates the loop a little (the Electronium 'suggest-and-steer' duet), so you curate variations instead of authoring every step.",
    "controls": "Drag a bulb up/down = its note (down to the bottom = rest). Arrow keys: LEFT/RIGHT select a bulb, UP/DOWN change its note. TEMPO = spin speed. STEPS = ring size. LEVEL knob edits the selected bulb. VOICE cycles RING/GLIDE/BONGO. SCALE cycles the scale. NUDGE (or SPACE) mutates the loop. RINGMOD/REVERB/ECHO shape the sound."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// CIRCLE MACHINE — after Raymond Scott's Circle Machine (~1959, Manhattan Research Inc.):
// a RING of incandescent bulbs, each with a brightness knob, swept by a photocell on a
// rotating spindle. As the photocell passes a bulb, that bulb's brightness sets the PITCH
// of a tone; the spindle's rotation speed sets the tempo. A step sequencer a decade before
// the word — and, per STATUS #21, visually unlike every other music cart here.
//
// The whole machine is ONE gesture: dial a loop of pitches into the ring and let it cycle.
// You perform by reaching into the running loop and reshaping it — the Scott "man-machine
// duet" made tactile. Design of record: docs/design/scott-blind-brief.md.
//
//   • a ring of bulbs; brightness = note (brighter = higher, up the scale); dark = rest
//   • a rotating photocell arm sweeps the ring — rotation speed = TEMPO
//   • drag a bulb up/down to set its note · arrows select + tune · LEVEL knob too
//   • 3 voices on the same ring — RING (bleep) · GLIDE (Clavivox portamento) · BONGO (Bandito)
//   • RINGMOD (the metallic clang) · REVERB · ECHO — Scott's processing
//   • NUDGE / SPACE mutates the loop — the Electronium "suggest and steer"

#define MAXN 16
#define CX   96
#define CY   108
#define R    82

// ── voices ──
enum { V_RING, V_GLIDE, V_BONGO, V_COUNT };
static const char *VNAME[V_COUNT] = { "RING", "GLIDE", "BONGO" };
enum { SL_RING = 5, SL_GLIDE, SL_BONGO };

// ── scales (semitone offsets from root, one octave) ──
typedef struct { const char *name; int n; int iv[7]; } Scale;
static const Scale SCALES[] = {
    { "PENTA",  5, { 0, 3, 5, 7, 10 } },        // minor pentatonic — always pretty
    { "MAJOR",  7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "WHOLE",  6, { 0, 2, 4, 6, 8, 10 } },     // whole-tone — the eerie Raymond Scott dream
    { "BLUES",  6, { 0, 3, 5, 6, 7, 10 } },
};
#define NSCALE (int)(sizeof(SCALES)/sizeof(SCALES[0]))
#define ROOT   48                                // C3

// ── state ──
static float lvl[MAXN];                          // per-bulb brightness 0..1 (0 = rest)
static float k_tempo, k_steps, k_ringmod, k_reverb, k_echo;
static int   sel = 0, grab = -1;
static int   gstep = -1, last_tick = -1;
static int   voice = V_RING, scale = 0;
static int   flash[MAXN];
static int   glide_h = -1;                       // the Clavivox held voice
static bool  pdown_prev = false;
static float ap_rm = -1, ap_rv = -1, ap_ec = -1; // last-applied fx (fx-frame rule)

static int   n_steps(void) { return 4 + (int)(k_steps * 12 + 0.5f); }   // 4..16
static int   tempo(void)   { return 70 + (int)(k_tempo * 110 + 0.5f); } // 70..180
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// a bulb's screen position (12 o'clock = step 0, clockwise)
static void bulb_pos(int i, int n, int *x, int *y) {
    float a = -1.5707963f + (float)i / n * 6.2831853f;
    *x = CX + (int)(cosf(a) * R);
    *y = CY + (int)(sinf(a) * R);
}

// a bulb's MIDI note from its brightness, snapped to the scale; -1 = rest
static int bulb_midi(int i) {
    if (lvl[i] < 0.06f) return -1;
    const Scale *s = &SCALES[scale];
    int maxdeg = s->n * 2;                        // ~two octaves of range
    int deg = (int)((lvl[i] - 0.06f) / 0.94f * maxdeg + 0.5f);
    if (deg > maxdeg) deg = maxdeg;
    return ROOT + (deg / s->n) * 12 + s->iv[deg % s->n];
}

// unified pointer (mouse OR first touch)
static bool pdn(void) { return mouse_down(0) || touch_count() > 0; }
static int  ptx(void) { return touch_count() > 0 ? touch_x(0) : mouse_x(); }
static int  pty(void) { return touch_count() > 0 ? touch_y(0) : mouse_y(); }

static void set_voice(int v) {
    if (v == voice) return;
    if (voice == V_GLIDE && glide_h >= 0) { note_off(glide_h); glide_h = -1; }  // leaving glide
    voice = ((v % V_COUNT) + V_COUNT) % V_COUNT;
    if (voice == V_GLIDE) { glide_h = note_on(ROOT + 12, SL_GLIDE, 0); note_glide(glide_h, 80); }
}

// the Electronium "suggest": nudge every bulb a touch, so the loop morphs but stays musical
static void nudge(void) {
    for (int i = 0; i < MAXN; i++) {
        if (rnd(3) == 0) lvl[i] = clampf(lvl[i] + (rnd(3) - 1) * 0.15f, 0, 1);
        if (rnd(9) == 0) lvl[i] = (lvl[i] < 0.06f) ? 0.5f : 0.0f;   // flip a rest occasionally
    }
}

void init(void) {
    instrument(SL_RING,  INSTR_SQUARE, 1, 150, 0, 40);   instrument_filter(SL_RING, FILTER_LOW, 3200, 2);
    instrument(SL_GLIDE, INSTR_SINE,   8, 260, 6, 140);  // the Clavivox — holds + slides
    instrument(SL_BONGO, INSTR_MEMBRANE, 0, 90, 0, 40);  // the Bandito auto-drummer

    k_tempo   = (108 - 70) / 110.0f;    // ~108 BPM
    k_steps   = (12 - 4) / 12.0f;       // 12 bulbs
    k_ringmod = 0.0f;
    k_reverb  = 0.35f;
    k_echo    = 0.20f;

    // a gentle rising/falling contour with a couple of rests — pretty on cold-open (ADR-0022)
    static const float seed[12] = { .45f,.6f,.75f,.55f, 0,.65f,.8f,.5f, 0,.7f,.85f,.4f };
    for (int i = 0; i < MAXN; i++) lvl[i] = seed[i % 12];
}

// set-and-hold fx: reconfigure ONLY when a knob has moved (never every frame)
static void apply_fx(void) {
    if (fabsf(k_ringmod - ap_rm) > 0.005f) { ringmod(180.0f, k_ringmod); ap_rm = k_ringmod; }
    if (fabsf(k_reverb  - ap_rv) > 0.005f) { reverb(k_reverb, 0.45f);    ap_rv = k_reverb; }
    if (fabsf(k_echo    - ap_ec) > 0.005f) { echo(270, k_echo * 0.85f, 0.5f); ap_ec = k_echo; }
}

static void trigger(int i) {
    int midi = bulb_midi(i);
    if (midi < 0) {                                  // rest
        if (voice == V_GLIDE && glide_h >= 0) note_vol(glide_h, 0);
        return;
    }
    int vol = 3 + (int)(lvl[i] * 4);                 // brighter bulb = louder (accent)
    switch (voice) {
        case V_RING:  hit(midi, SL_RING, vol, 60000 / tempo() / 4 * 4 / 5); break;
        case V_BONGO: hit(midi, SL_BONGO, vol, 90); break;
        case V_GLIDE:
            if (glide_h >= 0) { note_pitch(glide_h, (float)midi); note_vol(glide_h, 3 + lvl[i] * 3); }
            break;
    }
    flash[i] = frame();
}

void update(void) {
    int n = n_steps();
    if (sel >= n) sel = n - 1;

    // ── keyboard ──
    if (btnp(0, BTN_LEFT))  sel = (sel - 1 + n) % n;
    if (btnp(0, BTN_RIGHT)) sel = (sel + 1) % n;
    if (btnp(0, BTN_UP))    lvl[sel] = clampf(lvl[sel] + 0.08f, 0, 1);
    if (btnp(0, BTN_DOWN))  lvl[sel] = clampf(lvl[sel] - 0.08f, 0, 1);
    if (keyp('V'))          set_voice(voice + 1);
    if (keyp('K'))          scale = (scale + 1) % NSCALE;
    if (keyp(KEY_SPACE))    nudge();

    // ── drag a bulb up/down to set its note (grab on the down-edge, ring area only) ──
    bool down = pdn(); int mx = ptx(), my = pty();
    if (down && !pdown_prev && mx < 185) {
        int best = -1; float bd = 121;              // within ~11px
        for (int i = 0; i < n; i++) {
            int bx, by; bulb_pos(i, n, &bx, &by);
            float d2 = (float)((mx - bx) * (mx - bx) + (my - by) * (my - by));
            if (d2 < bd) { bd = d2; best = i; }
        }
        if (best >= 0) { grab = best; sel = best; }
    }
    if (!down) grab = -1;
    if (grab >= 0 && down)
        lvl[grab] = clampf((float)(CY + R + 8 - my) / (2 * R + 16), 0, 1);   // up = brighter
    pdown_prev = down;

    // ── transport: the spindle. n bulbs per revolution, one bulb per 16th at TEMPO ──
    bpm(tempo());
    float pos = beat() * 4 + beat_pos() * 4.0f;
    int tick = (int)pos;
    if (tick != last_tick) { last_tick = tick; gstep++; trigger(gstep % n); }

    apply_fx();

#ifdef DE_TRACE
    watch("voice", "%s", VNAME[voice]);
    watch("scale", "%s", SCALES[scale].name);
    watch("tempo", "%d", tempo());
    watch("steps", "%d", n);
    watch("sel",   "%d", sel);
    watch("cur",   "%d", gstep < 0 ? -1 : gstep % n);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    int n = n_steps();
    int cur = gstep < 0 ? -1 : gstep % n;
    float frac = beat_pos() * 4.0f; frac -= (int)frac;                // within the 16th
    float armA = -1.5707963f + (((cur < 0 ? 0 : cur) + frac) / n) * 6.2831853f;

    print("CIRCLE MACHINE", 4, 4, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print(str("%s  %s", VNAME[voice], SCALES[scale].name), 4, 14, CLR_ORANGE);
    font(FONT_NORMAL);
    print_right(str("%d BPM", tempo()), 190, 4, CLR_LIGHT_GREY);

    // guide ring + the rotating photocell arm
    circ(CX, CY, R, CLR_DARKER_GREY);
    int ax = CX + (int)(cosf(armA) * (R + 3)), ay = CY + (int)(sinf(armA) * (R + 3));
    line(CX, CY, ax, ay, CLR_DARK_GREY);
    circfill(ax, ay, 3, CLR_LIGHT_YELLOW);                            // the photocell

    // the bulbs
    for (int i = 0; i < n; i++) {
        int x, y; bulb_pos(i, n, &x, &y);
        bool rest = lvl[i] < 0.06f;
        bool head = (i == cur);
        bool hot  = (frame() - flash[i]) < 6;
        if (rest) {
            circfill(x, y, 2, i == sel ? CLR_MEDIUM_GREY : CLR_DARKER_GREY);
        } else {
            int r = 2 + (int)(lvl[i] * 4);
            int glow = (head && hot) ? CLR_WHITE : (lvl[i] > 0.66f ? CLR_LIGHT_YELLOW
                                                  : lvl[i] > 0.33f ? CLR_ORANGE : CLR_BROWN);
            if (lvl[i] > 0.5f) circfill(x, y, r + 2, CLR_DARK_BROWN);  // halo
            circfill(x, y, r, glow);
        }
        if (i == sel) circ(x, y, 8, CLR_MEDIUM_GREY);                 // selection ring
    }
    // center hub, pulses on the beat
    circfill(CX, CY, (frame() - flash[cur < 0 ? 0 : cur]) < 6 ? 4 : 2, CLR_DARK_GREY);

    // ── right panel ──
    ui_begin();
    font(FONT_SMALL);
    ui_knob(&k_tempo,   206, 34, "TEMPO");
    ui_knob(&k_steps,   246, 34, "STEPS");
    ui_knob(&lvl[sel],  286, 34, "LEVEL");
    ui_knob(&k_ringmod, 206, 86, "RINGMOD");
    ui_knob(&k_reverb,  246, 86, "REVERB");
    ui_knob(&k_echo,    286, 86, "ECHO");
    if (ui_button(200, 126, 58, 16, VNAME[voice]))          set_voice(voice + 1);
    if (ui_button(262, 126, 56, 16, SCALES[scale].name))    scale = (scale + 1) % NSCALE;
    if (ui_button(200, 148, 118, 16, "NUDGE"))              nudge();
    print("drag a bulb up/down = note", 200, 172, CLR_DARK_GREY);
    print("V voice  K scale  SPACE=nudge", 200, 180, CLR_DARK_GREY);
    font(FONT_NORMAL);
    ui_end();
}
