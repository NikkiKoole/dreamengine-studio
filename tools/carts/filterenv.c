/* de:meta
{
  "title": "dual env",
  "status": "active",
  "created": "2026-06-03",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "adsr-envelope",
    "subtractive-synth"
  ],
  "lineage": "Sibling of pitchenv — the same mod-envelope system pointed at filter cutoff; the canonical tuning rig for the resonant-lowpass sweep (the pluck 'pew').",
  "description": {
    "summary": "A Minimoog/Model-D-style bass rig: THREE oscillators (two detuned saws + an octave-down triangle sub) through the Moog ladder filter, with a FILTER contour, a cutoff LFO, a LOUDNESS ADSR, plus DETUNE, DRIVE and a FAT low-shelf EQ for the low-end 'humpf'. Each control has its own slider/toggle and a live graph.",
    "detail": "Subtractive-synth fattening, demonstrated. The voice is two sawtooths (instrument_tune detunes the second a few cents → they beat = thick) through FILTER_LADDER (the Moog 4-pole), warmed by instrument_drive in DRIVE_ASYM (even harmonics = round grit). Graph 1: the FILTER cutoff — the one-shot ENV snaps it open on the attack, the LFO wobbles it continuously. Graph 2: the AMP envelope — what decides how LONG you hear the note (low sustain plucks and dies, high sustain rings to note-off, the red line). Row 3 is two views side by side. LEFT, SHAPE: a still idealized single cycle — a saw through a resonant filter (CUT rounds the corners, RES adds the ring), clipped on one side by DRIVE, plus the octave SUB; it redraws only when you change a SHAPE knob. RIGHT, SCOPE: the ACTUAL output via the scope_read engine API, zero-cross-triggered to hold still — the real signal, so the envelope sweep / LFO wobble / detune beating you DON'T see in the still shape all show up here, live. DETUNE/DRIVE plus the SUB OSC (octave triangle) and FAT EQ toggles are where the humpf lives. MONO makes it a one-note-at-a-time bass like a real Model D: every note chokes the last and RETRIGGERS the filter-env thump, snapped cleanly to pitch — punchy, with no polyphonic wash.",
    "controls": "A-K play notes - SPACE toggles the auto-arp - MONO / FAT EQ / SUB OSC toggles top-right - drag the FILTER / LFO / AMP / VOICE sliders"
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// DUAL ENV — a Minimoog/Model-D-style bass voice + filter rig. Where the "humpf" comes from:
//   THREE oscillators (like a Model D's 3 VCOs): two detuned saws (SLOT + SLOT2, beating = thick)
//     plus an octave-down TRIANGLE sub (SLOT3) for round low-end body — the SUB OSC toggle.
//   FILTER_LADDER — the Moog 4-pole transistor ladder (24 dB/oct), creamier than a 2-pole.
//   DRIVE (DRIVE_ASYM) — even-harmonic saturation = round, fat grit.
//   FAT EQ — a +6 dB low-shelf boost on the whole voice; the most direct weight knob.
// On top of that, three modulators:
//   FILTER contour (instrument_env + ENV_CUTOFF) — snaps the cutoff open on the attack.
//   cutoff LFO (instrument_lfo + LFO_CUTOFF) — the continuous wobble.
//   LOUDNESS contour (the amp ADSR) — its SUSTAIN decides how LONG you hear the note.
// Drag the sliders and listen. A–K play (GarageBand layout), Z/X octave, SPACE toggles the arp.

#define SLOT  5
#define SLOT2 6                   // the 2nd, detuned saw — the other half of the fat
#define SLOT3 7                   // 3rd oscillator: an octave-down TRIANGLE for low-end body (the SUB)
#define GATE_MS 450               // how long each note is gated (note-on → note-off)

typedef struct { const char *name; float lo, hi, val; } Knob;
enum { F_ATK, F_DEC, F_AMT, F_CUT, F_RES,   L_RATE, L_DEPTH,   A_ATK, A_DEC, A_SUS, A_REL,   V_DTN, V_DRV, NK };
static Knob K[NK] = {
    // FILTER contour
    { "ATK",     0.f,  120.f,    0.f },   // filter-env attack (ms) — plucks want ~0
    { "DEC",    10.f,  600.f,  140.f },   // filter-env decay (ms) — how fast the cutoff closes
    { "AMT",-3500.f, 3500.f, 1800.f },    // env depth on cutoff (Hz). bipolar: + opens, - closes first
    { "CUT",   80.f, 2000.f,  320.f },    // base cutoff the env rides on top of (Hz)
    { "RES",    0.f,   15.f,    9.f },    // filter resonance — the squelch
    // LFO on cutoff — the continuous wobble (per-voice: resets to phase 0 on each note-on)
    { "RTE",    0.f,   12.f,    5.f },    // LFO rate (Hz) — wobble speed
    { "DEP",    0.f, 3000.f,  600.f },    // LFO depth (Hz) — wobble amount on cutoff. 0 = off
    // LOUDNESS contour (amp ADSR)
    { "ATK",    0.f,  400.f,    2.f },    // amp attack (ms)
    { "DEC",    0.f,  600.f,  180.f },    // amp decay to sustain (ms)
    { "SUS",    0.f,    7.f,    2.f },    // amp SUSTAIN level (0..7) — THE note-length knob
    { "REL",    0.f,  800.f,  160.f },    // amp release after note-off (ms)
    // VOICE — the "humpf": detune the 2nd saw + saturate
    { "DTN",    0.f,   30.f,   12.f },    // detune of the 2nd saw (cents) — beating = fat. 0 = unison
    { "DRV",    0.f,  100.f,   25.f },    // drive (DRIVE_ASYM, %) — even-harmonic warmth/grit
};

static int   active = -1;        // slider being dragged, or -1
static bool  auto_on = true;
static bool  sub_on = true;      // 3rd osc: octave-down triangle (low-end body)
static bool  fat_on = false;     // low-shelf EQ boost (+6 dB lows) — direct humpf
static bool  mono_on = false;    // mono: one note at a time; every note chokes the last + retriggers the env (the thump), snapped to pitch
static int   mh[3] = { -1, -1, -1 };   // held-note handles (SLOT/SLOT2/SLOT3) while in mono mode
static bool  mono_held = false;
static float mono_off_at = 0.f;
static int   oct = 0;            // octave shift from Z/X (GarageBand-style)
static float arp_t = 0.f;
static int   arp_i = 0;
static float last_note_t = -9.f;

// toggle buttons (top-right): FAT (EQ low boost) then SUB (octave triangle)
#define SUBX 252
#define SUBY 3
#define SUBW 60
#define SUBH 11
#define FATX 198
#define FATW 50
#define MONX 100
#define MONW 48

// slider layout: four groups (5 filter | 2 lfo | 4 amp | 2 voice) in one row
#define SX 2
#define SW 20
#define SP 23
#define GAP 6
#define SY 116
#define SH 42

static int knob_x(int i) {
    if (i < 5)  return SX + i * SP;                              // filter group
    if (i < 7)  return SX + 5 * SP + GAP + (i - 5) * SP;         // lfo group
    if (i < 11) return SX + 7 * SP + 2 * GAP + (i - 7) * SP;     // amp group
    return SX + 11 * SP + 3 * GAP + (i - 11) * SP;              // voice group
}

static void apply(void) {
    float det = K[V_DTN].val / 100.f;     // cents → semitones (instrument_tune is in semitones)
    float drv = K[V_DRV].val / 100.f;     // % → 0..1
    int a = (int)K[A_ATK].val, d = (int)K[A_DEC].val, su = (int)K[A_SUS].val, r = (int)K[A_REL].val;
    int cut = (int)K[F_CUT].val, res = (int)K[F_RES].val;
    int slots[2] = { SLOT, SLOT2 };
    for (int s = 0; s < 2; s++) {         // two saws, the 2nd detuned → they beat = Moog-fat
        int sl = slots[s];
        instrument(sl, INSTR_SAW, a, d, su, r);
        instrument_filter(sl, FILTER_LADDER, cut, res);                               // the Moog 4-pole
        instrument_env(sl, 0, ENV_CUTOFF, (int)K[F_ATK].val, (int)K[F_DEC].val, K[F_AMT].val);
        instrument_lfo(sl, 0, LFO_CUTOFF, K[L_RATE].val, K[L_DEPTH].val);             // wobble rides on top
        instrument_drive_mode(sl, DRIVE_ASYM);                                        // even harmonics = fat
        instrument_drive(sl, drv);
        instrument_tune(sl, s == 0 ? 0.f : det);                                      // detune only the partner
    }
    // 3rd oscillator (the SUB): a TRIANGLE an octave down — round low-end body, not more buzz
    instrument(SLOT3, INSTR_TRI, a, d, su, r);
    instrument_filter(SLOT3, FILTER_LADDER, cut, res);
    instrument_env(SLOT3, 0, ENV_CUTOFF, (int)K[F_ATK].val, (int)K[F_DEC].val, K[F_AMT].val);
    instrument_drive_mode(SLOT3, DRIVE_ASYM);
    instrument_drive(SLOT3, drv);
    instrument_tune(SLOT3, -12.f);                                                    // one octave down
    eq(fat_on ? 6.0f : 0.0f, 0.0f, 0.0f);                                             // FAT: low-shelf boost
}

static void mono_off(void) {                       // release the held mono voice
    for (int i = 0; i < 3; i++) if (mh[i] >= 0) { note_off(mh[i]); mh[i] = -1; }
    mono_held = false;
}

static void play(int midi) {
    if (mono_on) {                                 // mono: choke the last note, RETRIGGER the env (thump), SNAP to pitch (in tune, no glide swoop)
        mono_off();                                // one voice at a time → no poly wash
        int sl[3] = { SLOT, SLOT2, SLOT3 };
        for (int i = 0; i < 3; i++) {
            if (i == 2 && !sub_on) { mh[2] = -1; continue; }
            mh[i] = note_on(midi, sl[i], 4);       // fresh voice → filter-env attack thump, snapped to pitch
        }
        mono_held = true;
        mono_off_at = now() + GATE_MS / 1000.f;
    } else {
        hit(midi, SLOT,  4, GATE_MS);
        hit(midi, SLOT2, 4, GATE_MS);              // the detuned partner saw
        if (sub_on) hit(midi, SLOT3, 4, GATE_MS);  // triangle sub (SLOT3 is tuned -1 octave)
    }
    last_note_t = now();
}

void init(void) {
    bpm(112);
    apply();
}

void update(void) {
    // toggle buttons (top-right) — separate region from the sliders
    if (mouse_pressed(MOUSE_LEFT) && mouse_y() >= SUBY && mouse_y() < SUBY + SUBH) {
        if (mouse_x() >= MONX && mouse_x() < MONX + MONW) { mono_on = !mono_on; if (!mono_on) mono_off(); }
        if (mouse_x() >= SUBX && mouse_x() < SUBX + SUBW) sub_on = !sub_on;
        if (mouse_x() >= FATX && mouse_x() < FATX + FATW) { fat_on = !fat_on; apply(); }  // EQ change → reapply
    }
    if (mono_on && mono_held && now() >= mono_off_at) mono_off();   // release after the last note's gate

    // sliders (drag vertically)
    if (mouse_pressed(MOUSE_LEFT)) {
        active = -1;
        for (int i = 0; i < NK; i++) {
            int cx = knob_x(i);
            if (mouse_x() >= cx && mouse_x() < cx + SW && mouse_y() >= SY - 8 && mouse_y() <= SY + SH + 8)
                active = i;
        }
    }
    if (!mouse_down(MOUSE_LEFT)) active = -1;
    if (active >= 0) {
        float t = 1.0f - clamp((float)(mouse_y() - SY) / SH, 0.f, 1.f);
        K[active].val = K[active].lo + t * (K[active].hi - K[active].lo);
        apply();
    }

    // GarageBand "musical typing" layout: A=C, W=C#, S=D, E=D#, D=E, F=F, T=F#, G=G,
    // Y=G#, H=A, U=A#, J=B, K=C, O=C#, L=D, P=D# — a chromatic octave-and-a-bit. Z/X shift octave.
    static const char gbk[16] = { 'A','W','S','E','D','F','T','G','Y','H','U','J','K','O','L','P' };
    for (int i = 0; i < 16; i++)
        if (keyp(gbk[i])) play(60 + oct * 12 + i);
    if (keyp('Z') && oct > -3) oct--;
    if (keyp('X') && oct <  3) oct++;
    if (keyp(KEY_SPACE)) auto_on = !auto_on;

    // auto arp so there's always something to judge by ear (follows the Z/X octave)
    if (auto_on) {
        arp_t += dt();
        if (arp_t >= 0.30f) { arp_t = 0.f; play(degree(SCALE_PENTA, 3 + oct, arp_i)); arp_i = (arp_i + 1) % 8; }
    }
}

// the cutoff `s` seconds into a note: base + one-shot ENV (mirrors sound_ad_env) + the LFO wobble
// (sine, phase 0 at note-on — matches our per-voice retrigger), all summed onto the cutoff
static float env_cutoff_at(float s) {
    float a = K[F_ATK].val / 1000.f, d = K[F_DEC].val / 1000.f, lvl;
    if (a > 0.f && s < a) lvl = s / a;
    else {
        float sd = s - a;
        lvl = (d <= 0.f || sd >= d) ? 0.f : expf(-4.0f * sd / d);
    }
    float lfo = sinf(6.2831853f * K[L_RATE].val * s) * K[L_DEPTH].val;
    return K[F_CUT].val + lvl * K[F_AMT].val + lfo;
}

// the AMP loudness 0..1 `s` seconds into a note, gated at GATE_MS (ADSR)
static float amp_at(float s) {
    float a = K[A_ATK].val / 1000.f, d = K[A_DEC].val / 1000.f;
    float sus = K[A_SUS].val / 7.f, r = K[A_REL].val / 1000.f, gate = GATE_MS / 1000.f;
    float t = s < gate ? s : gate, lvl;            // ADS portion runs only while the key is held
    if (a > 0.f && t < a)            lvl = t / a;
    else if (d > 0.f && t < a + d)   lvl = sus + (1.f - sus) * expf(-4.0f * (t - a) / d);
    else                             lvl = sus;
    if (s > gate) lvl = (r <= 0.f) ? 0.f : lvl * expf(-4.0f * (s - gate) / r);   // release after note-off
    return lvl < 0.f ? 0.f : (lvl > 1.f ? 1.f : lvl);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("DUAL ENV", 8, 4, CLR_LIGHT_PEACH);

    // toggles (top-right): MONO (mono+legato) + FAT (EQ low boost) + SUB (octave-down triangle)
    font(FONT_SMALL);
    rectfill(MONX, SUBY, MONW, SUBH, mono_on ? CLR_PINK : CLR_BROWNISH_BLACK);
    rect(MONX, SUBY, MONW, SUBH, CLR_DARKER_GREY);
    print("MONO", MONX + 5, SUBY + 3, mono_on ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
    rectfill(FATX, SUBY, FATW, SUBH, fat_on ? CLR_ORANGE : CLR_BROWNISH_BLACK);
    rect(FATX, SUBY, FATW, SUBH, CLR_DARKER_GREY);
    print("FAT EQ", FATX + 5, SUBY + 3, fat_on ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
    rectfill(SUBX, SUBY, SUBW, SUBH, sub_on ? CLR_GREEN : CLR_BROWNISH_BLACK);
    rect(SUBX, SUBY, SUBW, SUBH, CLR_DARKER_GREY);
    print("SUB OSC -1", SUBX + 5, SUBY + 3, sub_on ? CLR_BROWNISH_BLACK : CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // shared time axis: cover the whole note — gate + release tail (and at least the filter sweep)
    float gate = GATE_MS / 1000.f;
    float win = gate + K[A_REL].val / 1000.f + 0.04f;
    float fspan = (K[F_ATK].val + K[F_DEC].val) / 1000.f * 1.15f;
    if (win < fspan) win = fspan;
    if (win < 0.1f) win = 0.1f;
    int offx;                                          // note-off x, shared between strips

    int gx = 8, gw = 304;

    // ── strip 1: FILTER cutoff sweep ──────────────────────────────────────────
    {
        int gy = 15, gh = 28;
        rectfill(gx, gy, gw, gh, CLR_BROWNISH_BLACK);
        rect(gx, gy, gw, gh, CLR_DARKER_GREY);

        float base = K[F_CUT].val, amt = K[F_AMT].val, dep = K[L_DEPTH].val;
        float lo_c = base + (amt < 0.f ? amt : 0.f) - dep;   // widen the range for the LFO swing
        float hi_c = base + (amt > 0.f ? amt : 0.f) + dep;
        if (lo_c < 20.f) lo_c = 20.f;
        if (hi_c - lo_c < 50.f) hi_c = lo_c + 50.f;
        #define CY(c) (gy + gh - 1 - (int)((clamp((c), lo_c, hi_c) - lo_c) / (hi_c - lo_c) * (gh - 2)))

        int by = CY(base);                                    // faint base-cutoff rest line
        for (int x = gx + 2; x < gx + gw - 2; x += 6) pset(x, by, CLR_DARK_GREY);

        int px = -1, py = -1;
        for (int x = 0; x < gw; x++) {
            int yy = CY(env_cutoff_at((x / (float)gw) * win));
            if (px >= 0) line(px, py, gx + x, yy, amt < 0.f ? CLR_ORANGE : CLR_BLUE);
            px = gx + x; py = yy;
        }
        offx = gx + (int)((gate / win) * gw);
        for (int y = gy + 1; y < gy + gh - 1; y += 4) pset(offx, y, CLR_RED);   // note-off marker

        float since = now() - last_note_t;
        if (since >= 0.f && since < win)
            circfill(gx + (int)((since / win) * gw), CY(env_cutoff_at(since)), 2, CLR_YELLOW);
        #undef CY

        font(FONT_SMALL);
        print("FILTER cutoff", gx + 3, gy + 2, CLR_BLUE);
        print(str("%d->%d Hz", (int)base, (int)(base + amt < 20.f ? 20.f : base + amt)), gx + 3, gy + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
    }

    // ── strip 2: AMP loudness contour (ADSR) ──────────────────────────────────
    {
        int gy = 46, gh = 28;
        rectfill(gx, gy, gw, gh, CLR_BROWNISH_BLACK);
        rect(gx, gy, gw, gh, CLR_DARKER_GREY);
        #define AY(l) (gy + gh - 1 - (int)(clamp((l), 0.f, 1.f) * (gh - 2)))

        int sy = AY(K[A_SUS].val / 7.f);                      // faint sustain-level line
        for (int x = gx + 2; x < offx; x += 6) pset(x, sy, CLR_DARK_GREY);

        int px = -1, py = -1;
        for (int x = 0; x < gw; x++) {
            int yy = AY(amp_at((x / (float)gw) * win));
            if (px >= 0) line(px, py, gx + x, yy, CLR_LIME_GREEN);
            px = gx + x; py = yy;
        }
        for (int y = gy + 1; y < gy + gh - 1; y += 4) pset(offx, y, CLR_RED);   // note-off marker

        float since = now() - last_note_t;
        if (since >= 0.f && since < win)
            circfill(gx + (int)((since / win) * gw), AY(amp_at(since)), 2, CLR_YELLOW);
        #undef AY

        font(FONT_SMALL);
        print("AMP loudness", gx + 3, gy + 2, CLR_LIME_GREEN);
        print("note off", offx + 2 > gx + gw - 36 ? offx - 34 : offx + 2, gy + 2, CLR_RED);
        font(FONT_NORMAL);
    }

    // ── strip 3: two views of the wave, side by side ──────────────────────────
    //   LEFT  SHAPE — a still, idealized single cycle (resonant filter rounds/rings via
    //         CUT/RES, drive clips a side via DRV, SUB adds the octave). Redraws only on a
    //         SHAPE knob; the envelope/LFO/sustain are TIME knobs a frozen cycle can't show.
    //   RIGHT SCOPE — the ACTUAL output via scope_read(), zero-cross-triggered so it mostly
    //         holds still. The real signal — envelope sweep, LFO wobble, beating and all.
    {
        int gy = 76, gh = 28, half = gh / 2, mid = gy + half;
        int lw = 148, rx = gx + lw + 8, rw = gw - lw - 8;
        float limy = (float)(half - 2);

        // LEFT — still wave-shape diagram (predicted)
        rectfill(gx, gy, lw, gh, CLR_BROWNISH_BLACK);
        rect(gx, gy, lw, gh, CLR_DARKER_GREY);
        for (int x = gx + 2; x < gx + lw - 2; x += 6) pset(x, mid, CLR_DARK_GREY);
        {
            float bright = clamp((K[F_CUT].val - 80.f) / (2000.f - 80.f), 0.f, 1.f);
            float fc = 0.03f + bright * 0.28f;            // SVF cutoff — open filter → sharper edges
            float damp = 1.f - clamp(K[F_RES].val / 15.f, 0.f, 0.8f);   // RES → resonant ring
            float drv = K[V_DRV].val / 100.f, g = 1.f + drv * 6.f, bias = drv * 0.8f;
            int M = (lw - 2) / 2, N = M * 2;
            static float w[256];
            float low = 0.f, band = 0.f, subph = 0.f;
            for (int k = 0; k < 4 * M; k++) {             // 2 warm-up cycles, then keep 2
                int c = k / M, i = k % M;
                float saw = 2.f * (i / (float)M) - 1.f;
                float subtri = 1.f - 4.f * fabsf(subph - 0.5f);   // octave-down triangle (-1..1)
                float in = sub_on ? saw * 0.7f + subtri * 0.4f : saw;
                subph += 0.5f / M; if (subph >= 1.f) subph -= 1.f;
                low  += fc * band;                        // resonant SVF lowpass: rounds + rings
                band += fc * (in - low - damp * band);
                if (low > 3.f) low = 3.f; else if (low < -3.f) low = -3.f;   // stability guard
                float s = tanhf(g * low + bias) - tanhf(bias);   // asym drive = flatten one side
                if (c >= 2) w[(c - 2) * M + i] = s;
            }
            float pk = 0.001f;
            for (int i = 0; i < N; i++) { float v = w[i] < 0 ? -w[i] : w[i]; if (v > pk) pk = v; }
            float amp = (float)(half - 3) / pk;
            int px = -1, py = -1;
            for (int i = 0; i < N; i++) {
                int xx = gx + 1 + i, yy = mid - (int)(clamp(w[i] * amp, -limy, limy));
                if (px >= 0) line(px, py, xx, yy, CLR_YELLOW);
                px = xx; py = yy;
            }
            font(FONT_SMALL); print("SHAPE (still)", gx + 3, gy + 2, CLR_YELLOW); font(FONT_NORMAL);
        }

        // RIGHT — live scope of the REAL output (the scope_read engine API)
        rectfill(rx, gy, rw, gh, CLR_BROWNISH_BLACK);
        rect(rx, gy, rw, gh, CLR_DARKER_GREY);
        for (int x = rx + 2; x < rx + rw - 2; x += 6) pset(x, mid, CLR_DARK_GREY);
        {
            static float buf[512];
            scope_read(buf, 512);
            int start = 0;                                // trigger on a rising zero-crossing → holds still
            for (int i = 1; i < 160; i++) if (buf[i - 1] <= 0.f && buf[i] > 0.f) { start = i; break; }
            int win = 320;                                // ~7ms window (1–2 cycles of a bass note)
            float pk = 0.001f;
            for (int i = 0; i < win; i++) { float v = buf[start + i] < 0 ? -buf[start + i] : buf[start + i]; if (v > pk) pk = v; }
            float amp = (float)(half - 3) / pk;
            if (amp > 120.f) amp = 120.f;                 // don't amplify near-silence to full screen
            int px = -1, py = -1, cols = rw - 2;
            for (int x = 0; x < cols; x++) {
                int si = start + (int)((x / (float)cols) * win);
                int xx = rx + 1 + x, yy = mid - (int)(clamp(buf[si] * amp, -limy, limy));
                if (px >= 0) line(px, py, xx, yy, CLR_LIME_GREEN);
                px = xx; py = yy;
            }
            font(FONT_SMALL); print("SCOPE (live)", rx + 3, gy + 2, CLR_LIME_GREEN); font(FONT_NORMAL);
        }
    }

    // ── sliders: four groups (FILTER | LFO | AMP | VOICE) ──────────────────────
    font(FONT_SMALL);
    print("FILTER ENV", knob_x(0) + 1, SY - 9, CLR_BLUE);
    print("LFO", knob_x(5) + 1, SY - 9, CLR_MAUVE);
    print("AMP ENV", knob_x(7) + 1, SY - 9, CLR_LIME_GREEN);
    print("VOICE", knob_x(11) + 1, SY - 9, CLR_ORANGE);
    font(FONT_NORMAL);
    for (int i = 0; i < NK; i++) {
        int cx = knob_x(i);
        int col = active == i ? CLR_YELLOW
                : i < 5 ? CLR_TRUE_BLUE : i < 7 ? CLR_MAUVE : i < 11 ? CLR_GREEN : CLR_ORANGE;
        rect(cx, SY, SW, SH, CLR_DARKER_GREY);
        float t = (K[i].val - K[i].lo) / (K[i].hi - K[i].lo);
        int vy = SY + SH - 1 - (int)(t * (SH - 2));
        if (K[i].lo < 0.f) {                                 // bipolar knob: fill from the zero line
            float t0 = (0.f - K[i].lo) / (K[i].hi - K[i].lo);
            int zy = SY + SH - 1 - (int)(t0 * (SH - 2));
            int top = vy < zy ? vy : zy, h = (vy < zy ? zy - vy : vy - zy) + 1;
            rectfill(cx + 1, top, SW - 2, h, col);
            line(cx + 1, zy, cx + SW - 2, zy, CLR_LIGHT_GREY);
        } else {
            int fh = (int)(t * (SH - 2));
            rectfill(cx + 1, SY + SH - 1 - fh, SW - 2, fh, col);
        }
        font(FONT_SMALL);
        print(K[i].name, cx + 2, SY + SH + 2, CLR_LIGHT_GREY);
        print(str("%d", (int)K[i].val), cx + 2, SY + SH + 9, CLR_WHITE);
        font(FONT_NORMAL);
    }

    font(FONT_SMALL);
    print(auto_on ? "auto-arp ON" : "auto-arp off", 8, SCREEN_H - 7, auto_on ? CLR_LIME_GREEN : CLR_DARK_GREY);
    print(str("oct %+d", oct), 72, SCREEN_H - 7, CLR_MAUVE);
    print_right("GarageBand keys  Z/X oct  SPACE arp", 312, SCREEN_H - 7, CLR_LIGHT_GREY);
    font(FONT_NORMAL);
}
