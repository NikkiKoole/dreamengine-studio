/* de:meta
{
  "slug": "hyperbox",
  "title": "hyperbox",
  "status": "active",
  "created": "2026-07-26",
  "kind": ["instrument", "tech-demo"],
  "genre": null,
  "teaches": ["subtractive-synth", "drum-synthesis", "step-sequencer", "scale-quantize"],
  "homage": "ReBirth RB-338 (Propellerhead, 1997) — the four-box rack shape, and the idea that a rack IS a genre argument. The sound aimed at is hyperpop's, the 100 gecs / SOPHIE lineage: pitched-up vocals, seven detuned saws, and a master bus deliberately overdriven.",
  "lineage": "The tiny first build of docs/design/contemporary-rebirth.md: a CONTEMPORARY ReBirth clones TECHNIQUES, not machines, because modern genres were never made on gear. It is also the showcase cart for multiband() (audio-notes §17 #34, Rung A of that doc) — the OTT box, three bands squashed down AND pushed up, which is the sound of a modern master. The rack shape is acidcandy/acidrack's; the supersaw box is supersaw's instrument_unison stack; the drums are tr909.h. NOT a mic cart: the voice box is INSTR_VOICE (deterministic, no permission prompt) standing in until the formant/harmoniser dial lands (that doc's Rung B).",
  "description": {
    "summary": "A four-box hyperpop rack where the master chain has no bypass. A pitch-snapped vocal lead, seven detuned saws, always-clipped drums, and OTT pinned at 100% — everything is too loud on purpose. The one knob that matters is RETUNE: it only goes from hard to harder, because in this genre there is no natural setting.",
    "detail": "Four boxes and a locked master, in the ReBirth body plan. VOICE is the lead instrument, not a vocal: an INSTR_VOICE line whose pitch is SNAPPED to a scale, where RETUNE sets how instantly it jumps between notes (0 = the hard robot jump, up = a natural glide) — the retune-speed knob, the same control Auto-Tune made famous, played by a synth so a take replays deterministically. Three vocal presets move the vowel/size/effort macros: CHIP (small tract, pitched up), ROBO (pressed and flat), CHOIR (three stacked voices). SAW is the JP-8000 wall: instrument_unison at seven voices, with DETUNE riding the spread live, BRIGHT on the filter, and TOY crushing it toward ringtone territory, because hyperpop flirts with cheap MIDI on purpose. DRUMS is a 16-step tr909.h kick lane where a second tap makes a step a RATCHET (a 1/32 burst via tr909_fire_stroke) — and the last two steps are always ratcheted, so the pattern is deliberately too fast at the end. MASTER is the point of the cart: multiband() at 100% (three bands pulled down, quiet detail pushed up) into a hard clipper, in that order via fx_order, with NO bypass drawn anywhere. RISER throws a pitch ramp over the top.",
    "controls": "SPACE — play/stop. Click a step in the DRUM lane: off → hit → RATCHET → off. Drag the knobs: RETUNE (hard→harder), DETUNE / BRIGHT / TOY on the saw wall, and UP (how hard the master lifts quiet detail — the one master knob that moves). 1 / 2 / 3 — vocal presets CHIP / ROBO / CHOIR. R — throw the riser. The master chain is deliberately not switchable."
  },
  "todo": [
    "v2: swap the INSTR_VOICE stand-in for the real vocal chain (mic + autotune_mic + the formant/harmoniser dial) once contemporary-rebirth.md Rung B lands.",
    "The pattern is one bank with no arrangement, per the constraint-is-the-product thesis. If a generator is added it should deliberately overshoot (too fast, too loud), since in this genre the mistakes are the genre."
  ]
}
de:meta */
//
// hyperbox — a tiny contemporary-ReBirth rack (hyperpop), and the showcase for multiband().
//
// The thesis (docs/design/contemporary-rebirth.md): ReBirth cloned MACHINES because in 1997 the
// genre lived in unobtainable boxes. Modern genres were never made on gear, so a contemporary
// version clones MOVES: hard tune, the seven-saw wall, always-clipped drums, and a master bus with
// no bypass. Four boxes, one screen, no arrangement view — the constraint IS the product.
//
// What each box is made of, engine-wise:
//   VOICE   INSTR_VOICE + a scale snap + note_glide as the RETUNE knob (an honest stand-in for a
//           mic'd vocal chain: deterministic, no permission prompt — see the de:meta todo)
//   SAW     instrument_unison(7) + instrument_unison_detune (live bloom) + filter + crush ("toy")
//   DRUMS   tr909.h — tr909_fire for a hit, tr909_fire_stroke(TR9_ST_RATCHET) for a 1/32 burst
//   MASTER  multiband() → drive_insert(DRIVE_HARD) via fx_order, both pinned on
//
// The master effects are SET-AND-HOLD (CLAUDE.md): apply_fx() only fires when a value actually
// changed, because re-calling a bus effect every frame rebuilds its DSP and stutters silently.
// multiband() itself is ride-safe, but the clipper next to it is not, so the whole chain goes
// through one change-detector.

#include "studio.h"
#include "tr909.h"
#include "ui.h"
#include "cursor.h"

#define SAW    5          // the seven-saw wall
#define VOX    6          // the "vocal" lead (INSTR_VOICE)
#define RISER  7          // the pitch-riser noise sweep
// tr909.h owns slots TR909_BASE(9) .. 21 — do not reuse them here.

#define STEPS  16
#define BPM    160        // "160 bpm, everything too loud by design"

enum { ST_OFF, ST_HIT, ST_RATCHET };            // a step cycles through these
static unsigned char kick[STEPS] = { ST_HIT,0,0,0, ST_HIT,0,0,ST_RATCHET, ST_HIT,0,0,0, ST_HIT,0,ST_RATCHET,ST_RATCHET };

// knobs (0..1, the ui.h contract)
static float k_retune = 0.15f;   // THE knob: 0 = instant robot jump .. 1 = a natural glide. Never reaches "off"
static float k_detune = 0.42f;   // the unison bloom
static float k_bright = 0.70f;   // filter cutoff
static float k_toy    = 0.18f;   // crush toward ringtone
static float k_up     = 0.45f;   // multiband's UPWARD amount — the macro that makes the master "always on"
// the three band DOWN amounts. These used to be literals inside draw()'s bar heights, which meant the
// LOW/MID/HI bars were sized by constants and could never move: they read as three sliders that ignore
// you, in the one box labelled MASTER DESTRUCTION. multiband() always took them per band — they were
// simply never exposed. Now they are knobs, which also answers §7's "a knob that does nothing" worry
// honestly: the bypass stays absent (that IS the thesis), but what remains is real.
static float k_low    = 0.60f;   // sub squash
static float k_mid    = 0.48f;   // body squash — lightest, so the riff still speaks
static float k_high   = 0.72f;   // air squash — hardest, the hyperpop sizzle

// transport
static int   playing = 0;
static int   step = -1;          // last step fired
static float clock_pos = 0.0f;   // 0..STEPS, our own step clock (bpm() drives the tempo)

// voices
static int   vox_h = -1;         // the held "vocal" note
static int   vox_stack[2] = { -1, -1 };   // CHOIR's two extra stacked voices
static int   saw_h[3] = { -1, -1, -1 };
static int   riser_h = -1;
static float riser_t = -1.0f;    // 0..1 while a riser is in flight, <0 = idle

// vocal presets — vowel / size / effort on INSTR_VOICE's three macros
enum { P_CHIP, P_ROBO, P_CHOIR, P_COUNT };
static const char *P_NAME[P_COUNT] = { "CHIP", "ROBO", "CHOIR" };
static int preset = P_CHIP;

// tr909 per-voice knobs (the cart owns these arrays; 0.5 = neutral)
static float ktune[TR9_NV], kdecay[TR9_NV], kcolor[TR9_NV];

// the melody: scale degrees, snapped — a minor-pentatonic riff, one note per 4 steps
static const int RIFF[4]   = { 0, 3, 5, 3 };
static const int PENTA[5]  = { 0, 3, 5, 7, 10 };
#define ROOT 69                     // A4 — pitched-up on purpose (the chipmunk register)

static int snap_penta(int degree) { // degree → midi, wrapping octaves
    int oct = degree / 5, i = degree % 5;
    if (i < 0) { i += 5; oct--; }
    return ROOT + 12 * oct + PENTA[i];
}

// ── the locked master chain ──────────────────────────────────────────────────────────────────
// multiband() squashes three bands and lifts the quiet detail; drive_insert(DRIVE_HARD) is the
// clipper. fx_order pins squash BEFORE clip: level everything, then slam the result.
static const int MASTER_CHAIN[] = { FX_MULTIBAND, FX_DRIVE };

static void apply_fx(int force) {
    static float p_up = -1.0f, p_toy = -1.0f, p_lo = -1.0f, p_md = -1.0f, p_hi = -1.0f;
    if (!force && p_up == k_up && p_toy == k_toy &&
        p_lo == k_low && p_md == k_mid && p_hi == k_high) return;   // set-and-hold: only on change
    p_up = k_up; p_toy = k_toy; p_lo = k_low; p_md = k_mid; p_hi = k_high;
    multiband(k_low, k_mid, k_high, k_up, 1.0f);               // no bypass, by design
    drive_insert(0.30f, DRIVE_HARD, 0.85f);                    // clip: always
    instrument_crush(SAW, 5.0f - k_toy * 3.0f, 1.0f - k_toy * 0.8f, k_toy);
    if (force) fx_order(0, MASTER_CHAIN, 2);
}

static void voice_macros(void) {   // the preset's vowel / size / effort
    float vowel  = preset == P_CHIP ? 0.85f : preset == P_ROBO ? 0.35f : 0.60f;
    float size   = preset == P_CHIP ? 0.08f : preset == P_ROBO ? 0.30f : 0.45f;  // small tract = pitched up
    float effort = preset == P_CHIP ? 0.80f : preset == P_ROBO ? 1.00f : 0.65f;
    instrument_harmonics(VOX, vowel);
    instrument_timbre(VOX, size);
    instrument_morph(VOX, effort);
}

void init(void) {
    bpm(BPM);
    // the saw wall
    instrument(SAW, INSTR_SAW, 3, 210, 2, 150);   // a DECAYING stab, not a pad: the gap it leaves is what lets the kick punch
                                                  // through an always-on master (an OTT'd sustain fills every hole)
    instrument_unison(SAW, 7, 0.10f + k_detune * 0.60f);
    instrument_filter(SAW, FILTER_LOW, 900, 4);
    instrument_level(SAW, 0.26f);
    // the "vocal" lead
    instrument(VOX, INSTR_VOICE, 12, 40, 7, 90);
    instrument_level(VOX, 0.40f);
    voice_macros();
    // the riser
    instrument(RISER, INSTR_NOISE, 8, 200, 4, 120);
    instrument_filter(RISER, FILTER_BAND, 1200, 9);
    instrument_level(RISER, 0.28f);
    // drums
    for (int v = 0; v < TR9_NV; v++) { ktune[v] = kdecay[v] = kcolor[v] = 0.5f; }
    kdecay[TR9_BD] = 0.70f;                 // a long, loud kick
    tr909_build(TR909_BASE);
    tr909_metal(TR909_BASE, 0.40f, 0.33f);
    apply_fx(1);
}

// ── one 1/16 step of the pattern ──────────────────────────────────────────────────────────────
static void fire_step(int s) {
    int step_ms = (int)(15000.0f / (float)BPM);      // one 1/16 in ms

    if (kick[s] == ST_HIT)
        tr909_fire(TR909_BASE, TR9_BD, 1, 0, ktune, kdecay, kcolor);
    else if (kick[s] == ST_RATCHET)                  // a 1/32 burst — the "too fast at the end"
        tr909_fire_stroke(TR909_BASE, TR9_BD, TR9_ST_RATCHET, 0, 0, step_ms, ktune, kdecay, kcolor);
    if ((s & 3) == 2) tr909_fire(TR909_BASE, TR9_CH, -2, 0, ktune, kdecay, kcolor);  // offbeat hat, tucked under the kick

    if ((s & 3) == 0) {                              // one riff note + one chord stab per beat
        int deg = RIFF[(s >> 2) & 3];
        int midi = snap_penta(deg);
        // RETUNE: 0 = the pitch JUMPS (hard tune), up = it slides there (natural correction)
        int glide_ms = (int)(k_retune * 120.0f);
        if (vox_h < 0) {
            vox_h = note_on(midi, VOX, 6);
            note_glide(vox_h, glide_ms);
        } else {
            note_glide(vox_h, glide_ms);
            note_pitch(vox_h, (float)midi);          // no retrigger — the snap you HEAR
        }
        if (preset == P_CHOIR) {                     // the stack: two more voices, an interval apart
            for (int i = 0; i < 2; i++) {
                if (vox_stack[i] < 0) { vox_stack[i] = note_on(midi + (i ? 7 : 12), VOX, 4); note_glide(vox_stack[i], glide_ms); }
                else { note_glide(vox_stack[i], glide_ms); note_pitch(vox_stack[i], (float)(midi + (i ? 7 : 12))); }
            }
        } else for (int i = 0; i < 2; i++) if (vox_stack[i] >= 0) { note_off(vox_stack[i]); vox_stack[i] = -1; }

        if ((s & 7) == 0) {                          // re-stab the wall every half bar
            for (int i = 0; i < 3; i++) if (saw_h[i] >= 0) note_off(saw_h[i]);
            saw_h[0] = note_on(midi - 12, SAW, 5);
            saw_h[1] = note_on(midi - 5,  SAW, 4);
            saw_h[2] = note_on(midi,      SAW, 4);
        }
    }
}

static void all_off(void) {
    for (int i = 0; i < 3; i++) if (saw_h[i] >= 0) { note_off(saw_h[i]); saw_h[i] = -1; }
    for (int i = 0; i < 2; i++) if (vox_stack[i] >= 0) { note_off(vox_stack[i]); vox_stack[i] = -1; }
    if (vox_h >= 0) { note_off(vox_h); vox_h = -1; }
}

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; if (!playing) all_off(); else { clock_pos = 0.0f; step = -1; } }
    for (int i = 0; i < P_COUNT; i++)
        if (keyp('1' + i)) { preset = i; voice_macros(); }
    if (keyp('R') && riser_t < 0.0f) {             // throw the riser
        riser_t = 0.0f;
        riser_h = note_on(48, RISER, 5);
    }

    if (playing) {                                   // our own 1/16 clock, off bpm()
        clock_pos += dt() * (float)BPM / 60.0f * 4.0f;
        while (clock_pos >= (float)STEPS) clock_pos -= (float)STEPS;
        int s = (int)clock_pos;
        if (s != step) { step = s; fire_step(s); }
    }

    if (riser_t >= 0.0f) {                           // a rising sweep, then gone
        riser_t += dt() * 0.8f;
        if (riser_h >= 0) {
            note_pitch(riser_h, 48.0f + riser_t * 40.0f);
            note_cutoff(riser_h, (int)(600 + riser_t * 6000));
        }
        if (riser_t >= 1.0f) { if (riser_h >= 0) note_off(riser_h); riser_h = -1; riser_t = -1.0f; }
    }

    apply_fx(0);                                     // change-detected, never per-frame reconfigure

#ifdef DE_TRACE
    watch("step", "%d", step);
    watch("up",   "%.2f", k_up);
    watch("retune_ms", "%d", (int)(k_retune * 120.0f));
#endif
}

// ── drawing: four boxes and a locked master bar ───────────────────────────────────────────────
static void box_frame(int y, int h, const char *title, int accent) {
    rect(4, y, SCREEN_W - 8, h, CLR_DARK_GREY);
    print(title, 8, y + 3, accent);
}

// captions ride the 4x6 font: at 8x8 a 320px screen only fits 40 chars, and ui-audit
// caught both the run-off and the collisions with the 8x8 box titles.
static void caption(const char *t, int x, int y, int col) {
    font(FONT_SMALL); print(t, x, y, col); font(FONT_NORMAL);
}

// right-aligned caption — measured with text_width() under the active font rather than
// hand-counted, so a reworded string can't quietly run off the edge again
static void caption_r(const char *t, int right, int y, int col) {
    font(FONT_SMALL); print(t, right - text_width(t), y, col); font(FONT_NORMAL);
}

void draw(void) {
    cls(CLR_BLACK);

    // ── VOICE ────────────────────────────────────────────────────────────────
    box_frame(4, 44, "VOICE ENGINE", CLR_PINK);
    caption_r("the instrument, not a vocal", SCREEN_W - 9, 6, CLR_DARK_GREY);
    ui_knob(&k_retune, 26, 28, "RETUNE");
    print(k_retune < 0.34f ? "hard" : k_retune < 0.67f ? "harder" : "hardest", 52, 20, CLR_PINK);
    caption("tune speed only goes", 52, 31, CLR_DARK_GREY);
    caption("from hard to harder", 52, 38, CLR_DARK_GREY);
    for (int i = 0; i < P_COUNT; i++) {
        if (ui_button(150 + i * 54, 18, 50, 18, P_NAME[i])) { preset = i; voice_macros(); }
        if (i == preset) rect(149 + i * 54, 17, 52, 20, CLR_PINK);
    }

    // ── SUPERSAW ─────────────────────────────────────────────────────────────
    box_frame(52, 50, "SUPERSAW BOX", CLR_INDIGO);
    caption_r("7 voices detuned", SCREEN_W - 9, 55, CLR_DARK_GREY);
    if (ui_knob(&k_detune, 30, 80, "DETUNE")) instrument_unison_detune(SAW, 0.10f + k_detune * 0.60f);
    if (ui_knob(&k_bright, 90, 80, "BRIGHT")) instrument_filter(SAW, FILTER_LOW, (int)(300 + k_bright * 5200), 4);
    ui_knob(&k_toy, 150, 80, "TOY");
    // a fan of saw teeth that widens with DETUNE — the bloom, drawn
    for (int i = 0; i < 7; i++) {
        int cx = 210 + i * 14, spread = (int)(k_detune * 5.0f);
        int off = (i - 3) * spread / 2;
        line(cx + off, 92, cx + off + 8, 66, i & 1 ? CLR_INDIGO : CLR_BLUE);
        line(cx + off + 8, 66, cx + off + 8, 92, i & 1 ? CLR_INDIGO : CLR_BLUE);
    }

    // ── DRUMS ────────────────────────────────────────────────────────────────
    box_frame(104, 44, "BLOWN-OUT DRUMS", CLR_RED);
    caption_r("clip: always", SCREEN_W - 9, 107, CLR_RED);
    for (int s = 0; s < STEPS; s++) {
        int x = 8 + s * 19, y = 120, w = 17, h = 20;
        int col = kick[s] == ST_HIT ? CLR_RED : kick[s] == ST_RATCHET ? CLR_ORANGE : CLR_DARKER_GREY;
        rectfill(x, y, w, h, col);
        if (kick[s] == ST_RATCHET)                       // four ticks = the 1/32 burst
            for (int k = 0; k < 4; k++) line(x + 3 + k * 4, y + 4, x + 3 + k * 4, y + h - 5, CLR_BLACK);
        if (playing && s == step) rect(x, y, w, h, CLR_WHITE);
        if (tapp(x, y, w, h) || (mouse_pressed(MOUSE_LEFT) && mouse_x() >= x && mouse_x() < x + w &&
                                 mouse_y() >= y && mouse_y() < y + h))
            kick[s] = (kick[s] + 1) % 3;                 // off → hit → ratchet → off
    }
    caption("light = a 1/32 ratchet burst - the last steps run too fast", 8, 142, CLR_DARK_GREY);

    // ── MASTER DESTRUCTION (no bypass) ───────────────────────────────────────
    box_frame(150, 46, "MASTER DESTRUCTION", CLR_YELLOW);
    // (no "OTT 100% / no bypass / clipper: on" captions any more — they described the design instead of
    // being it, and the four knobs need the vertical room. The absent bypass speaks for itself.)
    // Four knobs on one row: the UP macro plus the three band DOWN amounts. Labels are drawn by hand in
    // the 4x6 font (ui_knob_at's own label uses the current 8x8 font, which collided with the captions on
    // the right — ui-audit caught it on the first pass), each with a squash bar under it so the shape of
    // the curve still reads at a glance, which is what the old static bars were for.
    {
        float *bk[4]; bk[0] = &k_up; bk[1] = &k_low; bk[2] = &k_mid; bk[3] = &k_high;
        static const char *bn[4] = { "UP", "LOW", "MID", "HI" };
        for (int b = 0; b < 4; b++) {
            int x = 22 + b * 30;
            if (ui_knob_at(bk[b], x, 170, 8, 0) && b > 0) apply_fx(1);      // squash changed → re-arm
            caption(bn[b], x - text_width(bn[b]) / 2, 180, b ? CLR_DARK_GREY : CLR_YELLOW);
            if (b) {                                                        // bands get the squash bar
                int hgt = (int)(1 + *bk[b] * 6.0f);
                rectfill(x - 8, 193 - hgt, 16, hgt, CLR_YELLOW);
                rectfill(x - 8, 193 - hgt - (int)(k_up * 4.0f) - 2, 16, 1, CLR_WHITE);   // upward lift
            }
        }
    }
    if (ui_button(262, 156, 50, 18, "RISER") && riser_t < 0.0f) { riser_t = 0.0f; riser_h = note_on(48, RISER, 5); }
    if (ui_button(262, 176, 50, 18, playing ? "STOP" : "PLAY")) {
        playing = !playing;
        if (!playing) all_off(); else { clock_pos = 0.0f; step = -1; }
    }
    caption_r("160 bpm", 256, 164, CLR_DARK_GREY);
    caption_r("one bank", 256, 172, CLR_DARK_GREY);
    caption_r("no arrangement", 256, 180, CLR_DARK_GREY);
    caption_r("too loud", 256, 189, CLR_YELLOW);

    cursor_draw(CUR_ARROW);
}
