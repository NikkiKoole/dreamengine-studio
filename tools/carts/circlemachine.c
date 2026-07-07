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
  "lineage": "After Raymond Scott's Circle Machine (~1959, Manhattan Research Inc.) — a step sequencer a decade before the word. Novel here: the sequencer is a RING swept by a rotating photocell arm, not a left-to-right grid; each bulb's BRIGHTNESS sets both pitch and accent; and TWO concentric rings run at once with different step counts, drifting against each other (polymeter) the way Scott's multi-voice machines (the Electronium's 12 channels, the Wall of Sound) layered — so the loop never repeats.",
  "homage": "Raymond Scott — Circle Machine",
  "description": {
    "summary": "Raymond Scott's Circle Machine, doubled: TWO rings of glowing bulbs swept by one rotating photocell arm — a step sequencer from 1959. Each bulb's brightness sets its note (brighter = higher); drag it dark for a rest. The rings have different step counts, so they drift against each other and never line up — over a bed of tape wobble, saturation and grime.",
    "detail": "Not a keyboard — a machine you configure and let cycle, the way Scott actually worked. Two concentric rings (outer = lead, inner = a lower counter-line) each with its own step count; one arm sweeps both, so they drift in and out of phase (polymeter — his multi-voice, never-repeating texture). Drag a bulb up/down to set its pitch (snapped to the scale); drag it to the bottom for a rest. Three voices per ring: RING (a clean electronic bleep), GLIDE (the Clavivox — one voice that slides between notes), BONGO (the Bandito auto-drummer). DIRT is the tape machine — wow/flutter, saturation, valve hiss and, cranked, bitcrush — the wobble and grime the records swim in. RINGMOD adds Scott's metallic clang; REVERB and ECHO the tape space; NUDGE mutates the loop (the Electronium 'suggest-and-steer').",
    "controls": "1/2 (or the A|B button) pick a ring. Drag a bulb up/down = its note (bottom = rest). Arrows: LEFT/RIGHT select a bulb, UP/DOWN change its note. TEMPO = spin speed. STEPS = the selected ring's size (drift). LEVEL edits the selected bulb. VOICE cycles RING/GLIDE/BONGO for the selected ring. SCALE (K) cycles the scale. DIRT = tape wobble + saturation + grime. RINGMOD/REVERB/ECHO shape the sound. NUDGE (SPACE) mutates the loop."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>

// CIRCLE MACHINE — after Raymond Scott's Circle Machine (~1959, Manhattan Research Inc.):
// a RING of incandescent bulbs, each with a brightness knob, swept by a photocell on a
// rotating spindle. The photocell passes a bulb; that bulb's brightness sets the PITCH of a
// tone; spindle speed = tempo. A step sequencer a decade before the word (STATUS #21).
//
// Doubled, because the records aren't one clean voice: TWO concentric rings with DIFFERENT
// step counts run under one arm and DRIFT against each other (polymeter) — the hypnotic,
// never-repeating, multi-voice texture of Scott's bigger machines (the Electronium's 12
// channels, the Wall of Sound). And it all swims in TAPE — wow/flutter, saturation, valve
// grime — the wobble and dirt those records are drenched in. Design: docs/design/scott-blind-brief.md.
//
//   • two rings of bulbs; brightness = note (brighter = higher); dark = rest
//   • one rotating photocell arm sweeps both — different step counts → they drift apart
//   • 1/2 pick a ring · drag a bulb up/down = its note · arrows select + tune
//   • 3 voices per ring — RING (bleep) · GLIDE (Clavivox portamento) · BONGO (Bandito)
//   • DIRT = tape wobble + saturation + grime · RINGMOD (clang) · REVERB · ECHO
//   • NUDGE / SPACE mutates the loop — the Electronium "suggest and steer"

#define MAXN 16
#define CX   92
#define CY   104
static const int RAD[2] = { 82, 50 };            // outer (A) , inner (B)
static const int ROOTS[2] = { 48, 36 };          // A = C3 lead, B = C2 counter-line

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

// ── a ring ──
typedef struct {
    float lvl[MAXN];        // per-bulb brightness 0..1 (0 = rest)
    float ksteps;           // step-count knob → 4..16
    int   voice;
    int   flash[MAXN];
    int   glide_h;          // the Clavivox held voice (-1 = none)
    int   last_step;        // last step the arm triggered (crossing detection)
} Ring;

static Ring  rg[2];
static int   selR = 0, selS = 0, grab = -1, grabR = 0;
static int   scale = 0;
static float k_tempo, k_ringmod, k_reverb, k_echo, k_dirt;
static bool  pdown_prev = false;
static float ap_rm = -1, ap_rv = -1, ap_ec = -1, ap_dt = -1;   // last-applied fx (fx-frame rule)
static float phase[2];              // each ring's OWN rotation (revolutions) — they can de-sync
static float last_pos;              // previous frame's musical position (for the delta)
static int   last_abs[2] = { -999, -999 };  // last absolute tick each ring fired
static float k_drift, k_human;      // DRIFT = deliberate de-sync (ring B spins faster) · HUMAN = analog wobble

static int   n_steps(int r) { return 4 + (int)(rg[r].ksteps * 12 + 0.5f); }  // 4..16
static int   tempo(void)    { return 70 + (int)(k_tempo * 110 + 0.5f); }       // 70..180
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// a bulb's screen position (12 o'clock = step 0, clockwise)
static void bulb_pos(int r, int i, int n, int *x, int *y) {
    float a = -1.5707963f + (float)i / n * 6.2831853f;
    *x = CX + (int)(cosf(a) * RAD[r]);
    *y = CY + (int)(sinf(a) * RAD[r]);
}

// a bulb's MIDI note from its brightness, snapped to the ring's scale/root; -1 = rest
static int bulb_midi(int r, int i) {
    if (rg[r].lvl[i] < 0.06f) return -1;
    const Scale *s = &SCALES[scale];
    int maxdeg = s->n * 2;                        // ~two octaves of range
    int deg = (int)((rg[r].lvl[i] - 0.06f) / 0.94f * maxdeg + 0.5f);
    if (deg > maxdeg) deg = maxdeg;
    return ROOTS[r] + (deg / s->n) * 12 + s->iv[deg % s->n];
}

// unified pointer (mouse OR first touch)
static bool pdn(void) { return mouse_down(0) || touch_count() > 0; }
static int  ptx(void) { return touch_count() > 0 ? touch_x(0) : mouse_x(); }
static int  pty(void) { return touch_count() > 0 ? touch_y(0) : mouse_y(); }

static void set_voice(int r, int v) {
    if (v == rg[r].voice) return;
    if (rg[r].voice == V_GLIDE && rg[r].glide_h >= 0) { note_off(rg[r].glide_h); rg[r].glide_h = -1; }
    rg[r].voice = ((v % V_COUNT) + V_COUNT) % V_COUNT;
    if (rg[r].voice == V_GLIDE) { rg[r].glide_h = note_on(ROOTS[r] + 12, SL_GLIDE, 0); note_glide(rg[r].glide_h, 90); }
}

// snap the two rings back together (and lock) — the "put a hand on the spinning disc" re-sync
static void resync(void) {
    phase[1] = phase[0];
    k_drift = 0;
    for (int r = 0; r < 2; r++) last_abs[r] = (int)(phase[r] * n_steps(r));  // no spurious retrigger
}

// the Electronium "suggest": nudge every bulb of both rings a touch — morph, stay musical
static void nudge(void) {
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < MAXN; i++) {
            if (rnd(3) == 0) rg[r].lvl[i] = clampf(rg[r].lvl[i] + (rnd(3) - 1) * 0.15f, 0, 1);
            if (rnd(9) == 0) rg[r].lvl[i] = (rg[r].lvl[i] < 0.06f) ? 0.5f : 0.0f;
        }
}

void init(void) {
    instrument(SL_RING,  INSTR_SQUARE, 1, 150, 0, 40);   instrument_filter(SL_RING, FILTER_LOW, 3200, 2);
    instrument(SL_GLIDE, INSTR_SINE,   8, 260, 6, 140);  // the Clavivox — holds + slides
    instrument(SL_BONGO, INSTR_MEMBRANE, 0, 90, 0, 40);  // the Bandito auto-drummer

    reverb(0.80f, 0.40f);               // configure the space ONCE — a long tape-plate hall
    echo(320, 0.55f, 0.45f);            // and the echo bus (darkish tape repeats); the KNOBS drive the SENDS

    k_tempo   = (108 - 70) / 110.0f;    // ~108 BPM
    k_ringmod = 0.12f;                  // a little metallic edge by default
    k_reverb  = 0.55f;                  // send: roomy → drenched
    k_echo    = 0.40f;                  // send: slapback → dub throw
    k_dirt    = 0.45f;                  // the records swim in it — start dirty
    k_drift   = 0.0f;                    // locked by default — turn it up to shove the rings apart
    k_human   = 0.30f;                   // a little analog looseness in the timing (never machine-tight)
    phase[0] = phase[1] = 0.0f;
    last_pos = 0.0f;

    static const float seedA[12] = { .5f,.65f,.8f,.6f, 0,.7f,.85f,.55f, 0,.75f,.9f,.45f };
    static const float seedB[7]  = { .7f, 0,.5f,.6f, 0,.8f,.4f };   // sparser, lower counter-line
    rg[0].ksteps = (12 - 4) / 12.0f;  // 12 bulbs
    rg[1].ksteps = (7  - 4) / 12.0f;  // 7 bulbs → drifts against the 12
    for (int i = 0; i < MAXN; i++) { rg[0].lvl[i] = seedA[i % 12]; rg[1].lvl[i] = seedB[i % 7]; }
    for (int r = 0; r < 2; r++) { rg[r].voice = V_RING; rg[r].glide_h = -1; rg[r].last_step = -1; }
}

// set-and-hold fx: reconfigure ONLY when a knob has moved (never every frame)
static void apply_fx(void) {
    if (fabsf(k_ringmod - ap_rm) > 0.005f) { ringmod(180.0f, k_ringmod); ap_rm = k_ringmod; }
    if (fabsf(k_reverb  - ap_rv) > 0.005f) {   // reverb is a SEND — route each voice into the tank
        for (int s = SL_RING; s <= SL_BONGO; s++) instrument_reverb(s, k_reverb);
        ap_rv = k_reverb;
    }
    if (fabsf(k_echo    - ap_ec) > 0.005f) {   // echo is a SEND too — how much each voice throws
        for (int s = SL_RING; s <= SL_BONGO; s++) instrument_echo(s, k_echo);
        ap_ec = k_echo;
    }
    if (fabsf(k_dirt    - ap_dt) > 0.005f) {                    // the tape machine
        float d = k_dirt;
        tape(0.18f + 0.55f * d, 0.12f + 0.45f * d, 0.25f + 0.55f * d);   // wow / flutter / saturation
        chorus(0.7f, 0.25f + 0.45f * d, 0.20f + 0.35f * d);              // the wobble
        amp_noise(0.10f * d, 0.08f * d, 60);                            // valve grime floor
        crush(d > 0.6f ? 16.0f - (d - 0.6f) * 20.0f : 16.0f, 1.0f, d > 0.6f ? (d - 0.6f) * 1.2f : 0.0f);
        ap_dt = d;
    }
}

static void trigger(int r, int i) {
    int midi = bulb_midi(r, i);
    if (midi < 0) {                                  // rest
        if (rg[r].voice == V_GLIDE && rg[r].glide_h >= 0) note_vol(rg[r].glide_h, 0);
        return;
    }
    int vol = 3 + (int)(rg[r].lvl[i] * 4);         // brighter bulb = louder (accent)
    switch (rg[r].voice) {
        case V_RING:  hit(midi, SL_RING, vol, 60000 / tempo() / 5); break;
        case V_BONGO: hit(midi, SL_BONGO, vol, 90); break;
        case V_GLIDE:
            if (rg[r].glide_h >= 0) { note_pitch(rg[r].glide_h, (float)midi); note_vol(rg[r].glide_h, 3 + rg[r].lvl[i] * 3); }
            break;
    }
    rg[r].flash[i] = frame();
}

void update(void) {
    int nA = n_steps(selR);
    if (selS >= nA) selS = nA - 1;

    // ── keyboard ──
    if (keyp('1')) selR = 0;
    if (keyp('2')) selR = 1;
    if (btnp(0, BTN_LEFT))  selS = (selS - 1 + nA) % nA;
    if (btnp(0, BTN_RIGHT)) selS = (selS + 1) % nA;
    if (btnp(0, BTN_UP))    rg[selR].lvl[selS] = clampf(rg[selR].lvl[selS] + 0.08f, 0, 1);
    if (btnp(0, BTN_DOWN))  rg[selR].lvl[selS] = clampf(rg[selR].lvl[selS] - 0.08f, 0, 1);
    if (keyp('V'))          set_voice(selR, rg[selR].voice + 1);
    if (keyp('K'))          scale = (scale + 1) % NSCALE;
    if (keyp('S'))          resync();
    if (keyp(KEY_SPACE))    nudge();

    // ── drag a bulb up/down to set its note (grab on the down-edge, ring area only) ──
    bool down = pdn(); int mx = ptx(), my = pty();
    if (down && !pdown_prev && mx < 185) {
        int best = -1, bestR = 0; float bd = 121;         // within ~11px, across both rings
        for (int r = 0; r < 2; r++) {
            int n = n_steps(r);
            for (int i = 0; i < n; i++) {
                int bx, by; bulb_pos(r, i, n, &bx, &by);
                float d2 = (float)((mx - bx) * (mx - bx) + (my - by) * (my - by));
                if (d2 < bd) { bd = d2; best = i; bestR = r; }
            }
        }
        if (best >= 0) { grab = best; grabR = bestR; selR = bestR; selS = best; }
    }
    if (!down) grab = -1;
    if (grab >= 0 && down)
        rg[grabR].lvl[grab] = clampf((float)(CY + RAD[0] + 8 - my) / (2 * RAD[0] + 16), 0, 1);
    pdown_prev = down;

    // ── transport: each ring has its OWN spinning arm (1 rev = 1 bar). HUMAN adds analog
    //    timing wobble; DRIFT spins ring B faster so it phases against A (out of sync, then
    //    laps back around); SYNC snaps them together. A ring fires when its arm crosses a bulb. ──
    bpm(tempo());
    float pos = beat() * 4 + beat_pos() * 4.0f;          // monotonic sixteenths
    float dpos = pos - last_pos; last_pos = pos;         // musical delta this frame
    if (dpos < 0) dpos = 0;                              // guard (bpm change / wrap)
    float base = dpos / 16.0f;                           // one revolution per bar
    float hd = 0.006f + k_human * 0.06f;                 // wobble depth 0.6%..6.6%
    float f  = (float)frame();
    float humA = 1.0f + hd * (0.6f * sinf(f * 0.031f)        + 0.4f * sinf(f * 0.013f + 1.0f));
    float humB = 1.0f + hd * (0.6f * sinf(f * 0.027f + 2.1f) + 0.4f * sinf(f * 0.017f + 3.3f));
    phase[0] += base * humA;
    phase[1] += base * humB * (1.0f + k_drift * 0.22f);  // DRIFT: up to ~22% faster → continuous phasing
    for (int r = 0; r < 2; r++) {
        int n = n_steps(r);
        int ab = (int)(phase[r] * n);
        if (ab != last_abs[r]) { last_abs[r] = ab; int loc = ((ab % n) + n) % n; rg[r].last_step = loc; trigger(r, loc); }
    }

    apply_fx();

#ifdef DE_TRACE
    watch("ring",   "%c", 'A' + selR);
    watch("voiceA", "%s", VNAME[rg[0].voice]);
    watch("voiceB", "%s", VNAME[rg[1].voice]);
    watch("scale",  "%s", SCALES[scale].name);
    watch("tempo",  "%d", tempo());
    watch("stepsA", "%d", n_steps(0));
    watch("stepsB", "%d", n_steps(1));
    watch("dirt",   "%.2f", k_dirt);
    watch("drift",  "%.2f", k_drift);
    watch("human",  "%.2f", k_human);
    watch("dphase", "%.3f", fmodf(phase[0] - phase[1] + 1.0f, 1.0f));   // A-vs-B phase separation
#endif
}

static void draw_ring(int r) {
    int n = n_steps(r), cur = rg[r].last_step;
    for (int i = 0; i < n; i++) {
        int x, y; bulb_pos(r, i, n, &x, &y);
        float lv = rg[r].lvl[i];
        bool rest = lv < 0.06f;
        bool head = (i == cur);
        bool hot  = (frame() - rg[r].flash[i]) < 6;
        bool selb = (r == selR && i == selS);
        if (rest) {
            circfill(x, y, 2, selb ? CLR_MEDIUM_GREY : CLR_DARKER_GREY);
        } else {
            int rr = 2 + (int)(lv * 4);
            int glow = (head && hot) ? CLR_WHITE : (lv > 0.66f ? CLR_LIGHT_YELLOW
                                                  : lv > 0.33f ? CLR_ORANGE : CLR_BROWN);
            if (lv > 0.5f) circfill(x, y, rr + 2, CLR_DARK_BROWN);   // halo
            circfill(x, y, rr, glow);
        }
        if (selb) circ(x, y, 8, CLR_MEDIUM_GREY);                   // selection ring
    }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    float aA = -1.5707963f + fmodf(phase[0], 1.0f) * 6.2831853f;   // each ring's own arm angle
    float aB = -1.5707963f + fmodf(phase[1], 1.0f) * 6.2831853f;

    print("CIRCLE MACHINE", 4, 4, CLR_LIGHT_YELLOW);
    font(FONT_SMALL);
    print(str("A:%s  B:%s  %s", VNAME[rg[0].voice], VNAME[rg[1].voice], SCALES[scale].name), 4, 14, CLR_ORANGE);
    font(FONT_NORMAL);
    print_right(str("%d BPM", tempo()), 190, 4, CLR_LIGHT_GREY);

    circ(CX, CY, RAD[0], CLR_DARKER_GREY);                          // guide rings
    circ(CX, CY, RAD[1], CLR_DARKER_GREY);
    draw_ring(1);                                                   // inner first
    draw_ring(0);                                                   // outer on top
    // TWO photocell arms — one per ring. They fan apart as the rings de-sync, converge on SYNC.
    int ax0 = CX + (int)(cosf(aA) * (RAD[0] + 3)), ay0 = CY + (int)(sinf(aA) * (RAD[0] + 3));
    int ax1 = CX + (int)(cosf(aB) * (RAD[1] + 3)), ay1 = CY + (int)(sinf(aB) * (RAD[1] + 3));
    line(CX, CY, ax0, ay0, CLR_DARK_GREY);   circfill(ax0, ay0, 3, CLR_LIGHT_YELLOW);   // ring A arm
    line(CX, CY, ax1, ay1, CLR_BROWN);       circfill(ax1, ay1, 2, CLR_ORANGE);         // ring B arm
    circfill(CX, CY, 2, CLR_DARK_GREY);                            // hub

    // ── right panel ──
    ui_begin();
    font(FONT_SMALL);
    ui_knob(&k_tempo,              206, 30, "TEMPO");
    ui_knob(&rg[selR].ksteps,      244, 30, "STEPS");
    ui_knob(&rg[selR].lvl[selS],   282, 30, "LEVEL");
    ui_knob(&k_ringmod,            206, 72, "RINGMOD");
    ui_knob(&k_reverb,             244, 72, "REVERB");
    ui_knob(&k_echo,               282, 72, "ECHO");
    ui_knob(&k_dirt,               206, 114, "DIRT");
    ui_knob(&k_drift,              244, 114, "DRIFT");
    ui_knob(&k_human,              282, 114, "HUMAN");
    if (ui_button(200, 148, 56, 14, str("RING %c", 'A' + selR)))  selR ^= 1;
    if (ui_button(262, 148, 56, 14, VNAME[rg[selR].voice]))       set_voice(selR, rg[selR].voice + 1);
    if (ui_button(200, 166, 56, 14, "SYNC"))                      resync();
    if (ui_button(262, 166, 56, 14, "NUDGE"))                     nudge();
    print("S=sync  V=voice  K=scale", 200, 186, CLR_DARK_GREY);
    font(FONT_NORMAL);
    ui_end();
}
