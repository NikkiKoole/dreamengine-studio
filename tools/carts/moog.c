#include "studio.h"
#define KEYBED_WHITE_KEYS "ASDFGHJKL;'"   // 11 white keys (two octaves + the top notes)
#define KEYBED_BLACK_KEYS "WE TYU OP"     // W E (gap) T Y U (gap) O P
#include "keybed.h"
#include <math.h>

// ── DREAM SYNTH — MODEL D ──────────────────────────────────────────────────
// A Minimoog Model D in software (the layout the Behringer Model D clones). The
// thing that makes a Minimoog a Minimoog is THREE oscillators stacked and slightly
// detuned — the beating between them is the fat, thick, alive tone a single
// oscillator can never make. So every key fires up to FOUR voices on separate
// instrument slots and the mixer sums them, exactly like the hardware's three VCOs
// + noise (the mt70 multi-slot trick — zero new engine code):
//
//   press a key -> note_on(osc1 slot5) + note_on(osc2 slot6) + note_on(osc3 slot7)
//                  + note_on(noise slot8)   each at its own octave + detune + level
//   lift        -> note_off them all (gate-based, like the hardware)
//
// Per oscillator: WAVEFORM, OCTAVE (the 32'/16'/8'/4'/2' range switch), DETUNE
// (the fine-tune knob — the fatness lives here), and LEVEL (the mixer).
// All SIX of the Model D's waveforms: TRI · SAW · SQR · WIDE (wide rectangle) ·
// NARW (narrow pulse) · SHRK (the triangle-saw "shark" blend, baked as a custom
// single-cycle wave with wave_set). WIDE/NARW are the square engine at duty
// 0.30 / 0.12 (instrument_duty) — the hollow/woody and the thin/nasal pulse.
//
// GLOBAL: a blendable NOISE source (WHITE follows the filter / PINK stays dark),
// GLIDE (portamento — note_glide, the mono lead slide), DRIFT (a touch of per-voice
// random detune = analog VCO instability), DRIVE (the Minimoog warmth — its mixer
// overdriving the filter), and KEYBOARD TRACK (the filter cutoff follows the pitch,
// the 1/3·2/3 tracking switches — so high notes stay bright), and hard SYNC (OSC2 is
// synced to OSC1 — sweep the ratio, by hand or with a SYNC-targeted LFO, for the
// screaming/tearing sync lead). The FILTER offers the
// real 4-pole Moog LADder plus LP/HP/BP/NF; a filter-contour + pitch ENVELOPE give
// the classic "wow"; three LFOs are the modulation. The PRESET bank (top right) loads
// factory patches; SAVE stores your own, USER recalls it. Filter, drive, tracking and
// the LFOs follow their controls LIVE under a ringing chord; wave/octave/detune/ADSR
// snapshot at the next note.
//
// Play it on the on-screen keyboard, the mouse, your computer keys (A row = white,
// W E T Y U O P = black), or a plugged-in MIDI keyboard. Z/X octave, C/V velocity.

// slots: three oscillators + a noise source (the mixer)
#define SL1 5
#define SL2 6
#define SL3 7
#define SLN 8
#define CLI(v,a,b) ((int)clamp((float)(v),(float)(a),(float)(b)))

// ---- per-oscillator waveform: all 6 of the Model D's ----
enum { OW_TRI, OW_SAW, OW_SQR, OW_WIDE, OW_NARW, OW_SHRK };
static const char *OWN[6] = { "TRI", "SAW", "SQR", "WIDE", "NARW", "SHRK" };
static int   wave_instr(int w) { return w == OW_TRI ? INSTR_TRI : w == OW_SAW ? INSTR_SAW
                                      : w == OW_SHRK ? INSTR_USER0 : INSTR_SQUARE; }
static float wave_duty (int w) { return w == OW_WIDE ? 0.30f : w == OW_NARW ? 0.12f : 0.5f; }

typedef struct { int wave; int oct; float detune; int level; } Osc;   // detune in semitones, level 0..7
typedef struct { int target; float rate; float depth; } Lfo;          // target 0=OFF,1=PIT,2=DUT,3=VOL,4=CUT,5=DRV

// ---- global synth state (a fat default patch: two saws a hair apart + a square down) ----
Osc osc[3] = {
    { OW_SAW,  0,  0.00f, 7 },
    { OW_SAW,  0, +0.08f, 5 },
    { OW_SQR, -1, -0.11f, 4 },
};
static const int SLOT_OF[3] = { SL1, SL2, SL3 };

float attack  = 6,  decay = 140, sustain = 5, release = 320;   // ms, sustain 0..7
int   fmode   = 5;                 // FILTER_LADDER — the real 4-pole Moog lowpass, on by default
float cutoff  = 700, res = 7;      // lower base so the FILTER CONTOUR has room to open it (the Moog "wow")
float drive_v = 0.25f;             // post-filter saturation — a Minimoog runs a little hot by default
float noise_l = 0;                 // mixer: noise source level 0..7 (0 = off)
int   pink    = 0;                 // noise colour: 0 = white (follows the filter), 1 = pink (stays dark)
float glide_ms = 0;                // portamento time (note_glide); 0 = snap
float drift   = 0.04f;             // analog VCO instability: ± semitones of per-voice random detune
float ktrack  = 0.5f;              // keyboard tracking: how much the filter cutoff follows pitch (0..1)
float sync_amt = 1.0f;             // oscillator HARD SYNC ratio on OSC2 (1 = off/unison, up to 5 = bright tearing)

Lfo   lfos[3] = { {1, 5.0f, 0.25f}, {0, 3.0f, 0.3f}, {0, 0.4f, 0.6f} };

// mod-envelopes — filter contour (ENV_CUTOFF) + a pitch env (ENV_PITCH)
int   env_view = 0;                // 0 = AMP ADSR, 1 = FILTER contour, 2 = PITCH env
float fenv_amt = 1500, fenv_atk = 4, fenv_dec = 220;
float penv_amt = 0,    penv_atk = 0, penv_dec = 120;

int   vel  = 98;                   // 0..127

// ---- preset bank ----
typedef struct {
    int   w[3], oc[3], lv[3];
    float det[3];
    int   fmode, pink;
    float cutoff, res, drive_v, noise_l, glide_ms, drift, ktrack;
    float attack, decay, sustain, release;
    float fenv_amt, fenv_atk, fenv_dec, penv_amt, penv_atk, penv_dec;
    int   lt[3];
    float lr[3], ld[3];
    float sync_amt;
} Patch;

static const char *PNAME[6] = { "INIT", "BASS", "LEAD", "BRASS", "ACID", "SYNC" };
static const Patch FACTORY[6] = {
  // INIT — the fat default
  { {OW_SAW,OW_SAW,OW_SQR}, {0,0,-1}, {7,5,4}, {0.0f,0.08f,-0.11f},
    FILTER_LADDER, 0, 700, 7, 0.25f, 0, 0, 0.04f, 0.5f,
    6,140,5,320, 1500,4,220, 0,0,120,
    {1,0,0}, {5.0f,3.0f,0.4f}, {0.25f,0.3f,0.6f}, 1.0f },
  // BASS — two saws an octave down + a square, snappy filter contour
  { {OW_SAW,OW_SAW,OW_SQR}, {0,-1,-1}, {7,6,5}, {0.0f,0.10f,-0.08f},
    FILTER_LADDER, 0, 520, 8, 0.40f, 0, 0, 0.05f, 0.35f,
    2,180,3,180, 1900,2,150, 0,0,120,
    {0,0,0}, {5.0f,3.0f,0.4f}, {0.0f,0.0f,0.0f}, 1.0f },
  // LEAD — three detuned saws, gliding, singing
  { {OW_SAW,OW_SAW,OW_SAW}, {0,0,0}, {7,6,6}, {0.0f,0.12f,-0.10f},
    FILTER_LADDER, 0, 1600, 9, 0.45f, 0, 60, 0.06f, 0.7f,
    4,220,6,300, 1200,6,260, 0,0,120,
    {1,0,0}, {5.5f,3.0f,0.4f}, {0.30f,0.0f,0.0f}, 1.0f },
  // BRASS — slow filter swell (the contour IS the brass), gentle detune
  { {OW_SAW,OW_SAW,OW_SAW}, {0,0,0}, {7,5,5}, {0.0f,0.06f,-0.06f},
    FILTER_LADDER, 0, 700, 4, 0.25f, 0, 0, 0.04f, 0.6f,
    50,300,6,400, 2400,120,520, 0,0,120,
    {0,0,0}, {5.0f,3.0f,0.4f}, {0.0f,0.0f,0.0f}, 1.0f },
  // ACID — one saw, screaming resonance, fast snap (303-ish)
  { {OW_SAW,OW_SAW,OW_SQR}, {0,0,-1}, {7,0,0}, {0.0f,0.08f,-0.11f},
    FILTER_LADDER, 0, 480, 14, 0.50f, 0, 40, 0.04f, 0.5f,
    2,120,0,120, 2600,0,170, 0,0,120,
    {0,0,0}, {5.0f,3.0f,0.4f}, {0.0f,0.0f,0.0f}, 1.0f },
  // SYNC — OSC2 hard-synced to OSC1, swept slowly by LFO1: the screaming sync lead
  { {OW_SAW,OW_SAW,OW_SAW}, {0,0,-1}, {7,6,0}, {0.0f,0.0f,0.0f},
    FILTER_LADDER, 0, 2200, 5, 0.35f, 0, 30, 0.03f, 0.6f,
    4,300,6,300, 800,4,200, 0,0,120,
    {6,0,0}, {0.3f,3.0f,0.4f}, {0.6f,0.0f,0.0f}, 2.6f },
};

// ---- live voice handles: one per oscillator (+ noise) per MIDI note ----
static int   h_osc[3][128];        // -1 = silent
static int   h_nz[128];
static float osc_last[3];          // last target pitch per oscillator — for portamento
static int   have_last = 0;
static float shark[64];            // the SHRK single-cycle wave (baked at init)

// ---- UI pointer: the finger driving the panel (touch that began above the keys) ----
#define NOFINGER -999
int mx, my, ui_held = 0;
int ui_finger = NOFINGER, ui_press = 0, ui_down = 0;
int octdn_x = -1, octup_x = -1, veldn_x = -1, velup_x = -1;
int seen_ids[12]; int seen_n = 0;
int touch_began(int id) { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) return 0; return 1; }

#define NWHITE 11
#define KB_Y 290
int in_box(int x, int y, int w, int h) { return mx >= x && mx < x + w && my >= y && my < y + h; }

float lfo_scaled(int dest, float d) {
    return dest == LFO_PITCH  ? d * 5.0f
         : dest == LFO_DUTY   ? d * 0.45f
         : dest == LFO_VOLUME ? d
                              : d * 1800.0f;   // LFO_CUTOFF (Hz)
}

// keyboard tracking: the filter cutoff rises with pitch (the 1/3·2/3 switches).
// ktrack 1.0 = full tracking (cutoff doubles per octave, like the note); 0 = fixed.
int tracked_cutoff(int midi) {
    float c = cutoff * powf(2.0f, ktrack * (midi - 60) / 12.0f);
    return CLI(c, 40, 12000);
}

// program ONE slot (an oscillator or the noise source) with the shared patch.
void program_slot(int slot, int instr, float duty) {
    instrument(slot, instr, (int)attack, (int)decay, CLI(sustain + 0.5f, 0, 7), (int)release);
    if (instr == INSTR_SQUARE) instrument_duty(slot, duty);
    for (int L = 0; L < 3; L++) {
        if (lfos[L].target == 0 || lfos[L].target >= 5)   // OFF — and DRV, which is cart-side
            { instrument_lfo(slot, L, LFO_PITCH, 0, 0); continue; }
        int dest = lfos[L].target - 1;
        instrument_lfo(slot, L, dest, lfos[L].rate, lfo_scaled(dest, lfos[L].depth));
    }
    instrument_filter(slot, fmode, (int)cutoff, CLI(res + 0.5f, 0, 15));
    instrument_drive(slot, drive_v);
    instrument_env(slot, 0, ENV_CUTOFF, (int)fenv_atk, (int)fenv_dec, fenv_amt);
    instrument_env(slot, 1, ENV_PITCH,  (int)penv_atk, (int)penv_dec, penv_amt);
}

// hard sync on OSC2: LFO target 6 (SYNC) sweeps the ratio cart-side, just like DRIVE.
float sync_now(void) {
    float r = sync_amt;
    for (int L = 0; L < 3; L++)
        if (lfos[L].target == 6)
            r += sin_deg(now() * lfos[L].rate * 360.0f) * lfos[L].depth * 2.0f;
    return r;
}
// engine ratio: a slider at/under 1.0 = true OFF (bypass), else the sync ratio
float sync_ratio_eng(float r) { return r > 1.02f ? clamp(r, 1.0f, 6.0f) : 0.0f; }

void apply_synth(void) {
    for (int i = 0; i < 3; i++)
        program_slot(SLOT_OF[i], wave_instr(osc[i].wave), wave_duty(osc[i].wave));
    program_slot(SLN, INSTR_NOISE, 0.5f);
    instrument_sync(SL1, 0);                              // OSC1 = the clean master pitch (the fundamental)
    instrument_sync(SL2, sync_ratio_eng(sync_amt));      // OSC2 = the synced oscillator (the tearing)
    instrument_sync(SL3, 0);
}

// the drive LFO has no engine dest — it runs cart-side, slewed through note_drive
float drive_now(void) {
    float drv = drive_v;
    for (int L = 0; L < 3; L++)
        if (lfos[L].target == 5)
            drv += sin_deg(now() * lfos[L].rate * 360.0f) * lfos[L].depth * 0.5f;
    return clamp(drv, 0.0f, 1.0f);
}

// push every LIVE knob to one ringing voice this frame: filter (mode + tracked cutoff),
// drive, LFOs. `cut` is the per-voice tracked cutoff so each note sits where it should.
void drive_live(int h, int cut) {
    if (h < 0) return;
    note_filter(h, fmode);
    note_cutoff(h, cut);
    note_res(h, CLI(res + 0.5f, 0, 15));
    note_drive(h, drive_now());
    for (int L = 0; L < 3; L++) {
        if (lfos[L].target == 0 || lfos[L].target >= 5)
            { note_lfo(h, L, LFO_PITCH, 0, 0); continue; }
        int dest = lfos[L].target - 1;
        note_lfo(h, L, dest, lfos[L].rate, lfo_scaled(dest, lfos[L].depth));
    }
}

int vel07(void) { return CLI(vel * 7 / 127, 1, 7); }

// keybed.h is in UNMANAGED mode — it tells us which key went down/up and WE create
// every voice. Each press stacks the three oscillators + noise; each release gates them off.
void note_start(int midi, int v) {
    apply_synth();                              // snapshot wave + ADSR for this strike
    for (int i = 0; i < 3; i++) {
        if (osc[i].level <= 0) { h_osc[i][midi] = -1; continue; }
        int   base   = midi + osc[i].oct * 12;
        float dr     = (i == 0) ? 0.0f : rnd_float_between(-drift, drift);   // osc1 = pitch anchor
        float target = (float)base + osc[i].detune + dr;
        int   vol    = CLI((osc[i].level * v + 3) / 7, 1, 7);               // mixer level × velocity
        int   h = note_on(base, SLOT_OF[i], vol);
        if (glide_ms > 0 && have_last) {        // portamento: start at the last note, slide to this one
            note_glide(h, 0);            note_pitch(h, osc_last[i]);
            note_glide(h, (int)glide_ms); note_pitch(h, target);
        } else {
            note_glide(h, 0);            note_pitch(h, target);
        }
        osc_last[i] = target;
        h_osc[i][midi] = h;
    }
    have_last = 1;
    if (noise_l > 0.5f) h_nz[midi] = note_on(midi, SLN, CLI((noise_l * v + 3) / 7, 1, 7));
    else                h_nz[midi] = -1;
}

void note_stop(int midi) {
    for (int i = 0; i < 3; i++) if (h_osc[i][midi] >= 0) { note_off(h_osc[i][midi]); h_osc[i][midi] = -1; }
    if (h_nz[midi] >= 0) { note_off(h_nz[midi]); h_nz[midi] = -1; }
}

// ---- preset capture / apply ----
void capture_patch(Patch *p) {
    for (int i = 0; i < 3; i++) {
        p->w[i] = osc[i].wave; p->oc[i] = osc[i].oct; p->lv[i] = osc[i].level; p->det[i] = osc[i].detune;
        p->lt[i] = lfos[i].target; p->lr[i] = lfos[i].rate; p->ld[i] = lfos[i].depth;
    }
    p->fmode = fmode; p->pink = pink; p->cutoff = cutoff; p->res = res; p->drive_v = drive_v;
    p->noise_l = noise_l; p->glide_ms = glide_ms; p->drift = drift; p->ktrack = ktrack; p->sync_amt = sync_amt;
    p->attack = attack; p->decay = decay; p->sustain = sustain; p->release = release;
    p->fenv_amt = fenv_amt; p->fenv_atk = fenv_atk; p->fenv_dec = fenv_dec;
    p->penv_amt = penv_amt; p->penv_atk = penv_atk; p->penv_dec = penv_dec;
}
void apply_patch(const Patch *p) {
    for (int i = 0; i < 3; i++) {
        osc[i].wave = p->w[i]; osc[i].oct = p->oc[i]; osc[i].level = p->lv[i]; osc[i].detune = p->det[i];
        lfos[i].target = p->lt[i]; lfos[i].rate = p->lr[i]; lfos[i].depth = p->ld[i];
    }
    fmode = p->fmode; pink = p->pink; cutoff = p->cutoff; res = p->res; drive_v = p->drive_v;
    noise_l = p->noise_l; glide_ms = p->glide_ms; drift = p->drift; ktrack = p->ktrack; sync_amt = p->sync_amt;
    attack = p->attack; decay = p->decay; sustain = p->sustain; release = p->release;
    fenv_amt = p->fenv_amt; fenv_atk = p->fenv_atk; fenv_dec = p->fenv_dec;
    penv_amt = p->penv_amt; penv_atk = p->penv_atk; penv_dec = p->penv_dec;
    apply_synth();
}

// ---- widgets ----
int ui_btn(int x, int y, int w, int h, const char *label, int on, int col) {
    int hot = in_box(x, y, w, h);
    rectfill(x, y, w, h, on ? col : CLR_BLACK);
    rect(x, y, w, h, hot ? CLR_WHITE : CLR_DARK_GREY);
    print(label, x + 3, y + (h - 5) / 2, on ? CLR_BLACK : CLR_MEDIUM_GREY);
    return hot && ui_press;
}

float ui_slider(int id, int x, int y, int w, float val, float lo, float hi, int col) {
    if (ui_press && in_box(x - 2, y - 3, w + 4, 11)) ui_held = id;
    if (ui_held == id) val = lo + clamp((float)(mx - x) / w, 0, 1) * (hi - lo);
    rectfill(x, y + 1, w, 2, CLR_DARK_GREY);
    int kx = x + (int)((val - lo) / (hi - lo) * w);
    rectfill(kx - 2, y - 2, 4, 8, col);
    return val;
}

void ui_xy(int id, int x, int y, int w, int h, float *vx, float xlo, float xhi,
           float *vy, float ylo, float yhi, int col) {
    if (ui_press && in_box(x, y, w, h)) ui_held = id;
    if (ui_held == id) {
        *vx = xlo + clamp((float)(mx - x) / w, 0, 1) * (xhi - xlo);
        *vy = yhi - clamp((float)(my - y) / h, 0, 1) * (yhi - ylo);
    }
    rectfill(x, y, w, h, CLR_BLACK);
    for (int g = 1; g < 4; g++) line(x + g * w / 4, y, x + g * w / 4, y + h, CLR_DARKER_GREY);
    for (int g = 1; g < 3; g++) line(x, y + g * h / 3, x + w, y + g * h / 3, CLR_DARKER_GREY);
    rect(x, y, w, h, CLR_DARK_GREY);
    int kx = clamp(x + (int)((*vx - xlo) / (xhi - xlo) * w), x, x + w);
    int ky = clamp(y + (int)((yhi - *vy) / (yhi - ylo) * h), y, y + h);
    line(kx, y, kx, y + h, col);
    line(x, ky, x + w, ky, col);
    circfill(kx, ky, 3, CLR_WHITE);
}

void panel(int x, int y, int w, int h, const char *title, int col) {
    rect(x, y, w, h, CLR_DARK_GREY);
    if (title[0]) print(title, x + 4, y + 3, col);
}

void init() {
    keybed_config(SL1, 3, NWHITE);     // base C3, 11 white keys
    keybed_layout(10, KB_Y, NWHITE * 40, SCREEN_H - KB_Y);
    keybed_manage_voices(false);       // pure input — we own all the voices
    keybed_on_note(note_start);
    keybed_on_off(note_stop);
    for (int m = 0; m < 128; m++) { for (int i = 0; i < 3; i++) h_osc[i][m] = -1; h_nz[m] = -1; }
    // SHRK: an asymmetric (skewed) triangle — rises slow, drops fast = the tri-saw "shark"
    for (int i = 0; i < 64; i++) {
        float t = i / 64.0f;
        shark[i] = (t < 0.7f) ? (-1.0f + 2.0f * (t / 0.7f)) : (1.0f - 2.0f * ((t - 0.7f) / 0.3f));
    }
    wave_set(0, shark, 64);            // INSTR_USER0
    Patch p;                            // restore the user's saved patch if there is one
    if (load_bytes(&p, sizeof p) == (int)sizeof p) apply_patch(&p);
    else apply_synth();
}

void update() {
    keybed_velocity(vel07());
    keybed_update();
    if (octdn_x >= 0 && tapp(octdn_x - 2, 276, 44, 13)) keybed_octave_shift(-1);
    if (octup_x >= 0 && tapp(octup_x - 2, 276, 44, 13)) keybed_octave_shift(+1);
    if (keyp('C') || (veldn_x >= 0 && tapp(veldn_x - 2, 276, 44, 13))) vel = CLI(vel - 10, 1, 127);
    if (keyp('V') || (velup_x >= 0 && tapp(velup_x - 2, 276, 44, 13))) vel = CLI(vel + 10, 1, 127);
}

// the famous ADSR envelope editor, with three drag handles
void draw_adsr(int gx, int gy, int gw, int gh) {
    float aMax = gw * 0.28f, dMax = gw * 0.28f, hold = gw * 0.16f, rMax = gw * 0.28f;
    int ax  = gx + (int)(attack  / 1200.0f * aMax);
    int dsx = ax + (int)(decay   / 1200.0f * dMax);
    int sy  = gy + (int)((1 - sustain / 7.0f) * gh);
    int hx  = dsx + (int)hold;
    int rx  = hx + (int)(release / 1800.0f * rMax);
    int by  = gy + gh;

    if (ui_press) {
        if      (in_box(ax - 5, gy - 5, 10, 10))  ui_held = 11;
        else if (in_box(dsx - 5, sy - 5, 10, 10)) ui_held = 12;
        else if (in_box(rx - 5, by - 5, 10, 10))  ui_held = 13;
    }
    if (ui_held == 11) attack  = clamp((float)(mx - gx) / aMax * 1200, 0, 1200);
    if (ui_held == 12) { decay = clamp((float)(mx - ax) / dMax * 1200, 0, 1200);
                         sustain = clamp((1 - (float)(my - gy) / gh) * 7, 0, 7); }
    if (ui_held == 13) release = clamp((float)(mx - hx) / rMax * 1800, 0, 1800);

    ax  = gx + (int)(attack / 1200.0f * aMax);
    dsx = ax + (int)(decay / 1200.0f * dMax);
    sy  = gy + (int)((1 - sustain / 7.0f) * gh);
    hx  = dsx + (int)hold;
    rx  = hx + (int)(release / 1800.0f * rMax);

    rectfill(gx, gy, gw, gh, CLR_BLACK);
    line(gx, by, ax, gy, CLR_GREEN);
    line(ax, gy, dsx, sy, CLR_GREEN);
    line(dsx, sy, hx, sy, CLR_GREEN);
    line(hx, sy, rx, by, CLR_GREEN);
    rectfill(ax - 2, gy - 2, 5, 5, CLR_WHITE);
    rectfill(dsx - 2, sy - 2, 5, 5, CLR_WHITE);
    rectfill(rx - 2, by - 2, 5, 5, CLR_WHITE);
    print(str("A%d D%d S%d R%d", (int)attack, (int)decay, (int)(sustain + 0.5f), (int)release),
          gx, by + 4, CLR_MEDIUM_GREY);
}

// AD-with-amount editor for the mod-envelopes
void draw_adenv(int gx, int gy, int gw, int gh, float *atk, float atkmax,
                float *dec, float decmax, float *amt, float amtmax, int col) {
    int zy = gy + gh / 2;
    float aMax = gw * 0.40f, dMax = gw * 0.45f, half = gh / 2.0f;
    int px = gx + (int)(*atk / atkmax * aMax);
    int py = (int)clamp(zy - *amt / amtmax * half, gy, gy + gh);
    int ex = px + (int)(*dec / decmax * dMax);

    if (ui_press) {
        if      (in_box(px - 5, py - 5, 10, 10)) ui_held = 14;
        else if (in_box(ex - 5, zy - 5, 10, 10)) ui_held = 15;
    }
    if (ui_held == 14) { *atk = clamp((float)(mx - gx) / aMax * atkmax, 0, atkmax);
                         *amt = clamp((zy - my) / half * amtmax, -amtmax, amtmax); }
    if (ui_held == 15)   *dec = clamp((float)(mx - px) / dMax * decmax, 1, decmax);

    px = gx + (int)(*atk / atkmax * aMax);
    py = (int)clamp(zy - *amt / amtmax * half, gy, gy + gh);
    ex = px + (int)(*dec / decmax * dMax);

    rectfill(gx, gy, gw, gh, CLR_BLACK);
    for (int x = gx + 2; x < gx + gw - 2; x += 6) pset(x, zy, CLR_DARKER_GREY);
    line(gx, zy, px, py, col);
    line(px, py, ex, zy, col);
    line(ex, zy, gx + gw, zy, col);
    rectfill(px - 2, py - 2, 5, 5, CLR_WHITE);
    rectfill(ex - 2, zy - 2, 5, 5, CLR_WHITE);
    print(str("amt %+d  a%d d%d", (int)*amt, (int)*atk, (int)*dec), gx, gy + gh + 4, CLR_MEDIUM_GREY);
}

// one oscillator strip in the mixer
void draw_osc(int i, int x, int y) {
    print(str("%d", i + 1), x, y + 5, CLR_LIGHT_GREY);
    if (ui_btn(x + 10, y, 50, 14, OWN[osc[i].wave], 1, CLR_BLUE)) osc[i].wave = (osc[i].wave + 1) % 6;
    if (ui_btn(x + 64, y, 36, 14, str("%+d", osc[i].oct), osc[i].oct != 0, CLR_INDIGO))
        osc[i].oct = osc[i].oct >= 2 ? -2 : osc[i].oct + 1;
    osc[i].detune = ui_slider(40 + i, x + 106, y + 4, 78, osc[i].detune, -0.5f, 0.5f, CLR_PINK);
    float lv = ui_slider(50 + i, x + 192, y + 4, 78, (float)osc[i].level, 0, 7, CLR_GREEN);
    osc[i].level = CLI(lv + 0.5f, 0, 7);
}

void draw() {
    // resolve the UI pointer: stick with ui_finger while down, else adopt a touch
    // that began above the keyboard (y < KB_Y) this frame
    ui_press = 0; ui_down = 0;
    int fi = -1;
    for (int i = 0; i < touch_count(); i++)
        if (touch_id(i) == ui_finger) { fi = i; break; }
    if (fi < 0) {
        ui_finger = NOFINGER; ui_held = 0;
        for (int i = 0; i < touch_count(); i++)
            if (touch_y(i) < KB_Y && touch_began(touch_id(i))) {
                ui_finger = touch_id(i); ui_press = 1; fi = i; break;
            }
    }
    if (fi >= 0) { mx = touch_x(fi); my = touch_y(fi); ui_down = 1; }
    else         { mx = mouse_x();   my = mouse_y(); }

    cls(CLR_DARKER_GREY);
    print("DREAM SYNTH", 8, 4, CLR_WHITE);
    font(FONT_SMALL); print("MODEL D", 96, 5, CLR_ORANGE);

    // ---- PRESET bank (header, top right) ----
    for (int i = 0; i < 6; i++)
        if (ui_btn(146 + i * 29, 2, 27, 11, PNAME[i], 1, CLR_INDIGO)) apply_patch(&FACTORY[i]);
    if (ui_btn(324, 2, 31, 11, "SAVE", 1, CLR_GREEN)) { Patch p; capture_patch(&p); save_bytes(&p, sizeof p); }
    if (ui_btn(357, 2, 31, 11, "USER", 1, CLR_YELLOW)) { Patch p; if (load_bytes(&p, sizeof p) == (int)sizeof p) apply_patch(&p); }
    font(FONT_NORMAL);

    // ---- OSCILLATORS (the three VCOs + mixer) ----
    panel(6, 16, 300, 96, "OSCILLATORS", CLR_BLUE);
    font(FONT_TINY);
    print("WAVE",  22, 26, CLR_DARK_GREY);
    print("OCT",   76, 26, CLR_DARK_GREY);
    print("DETUNE",118, 26, CLR_DARK_GREY);
    print("LEVEL", 206, 26, CLR_DARK_GREY);
    font(FONT_NORMAL);
    for (int i = 0; i < 3; i++) draw_osc(i, 12, 34 + i * 23);

    // ---- GLOBAL (noise mixer · glide · drift · drive · keyboard tracking) ----
    panel(310, 16, 144, 96, "MIX / GLIDE", CLR_ORANGE);
    if (ui_btn(404, 17, 46, 11, pink ? "PINK" : "WHITE", 1, pink ? CLR_MAUVE : CLR_LIGHT_GREY)) pink = !pink;
    font(FONT_TINY);
    print("NOISE", 316, 30, CLR_MEDIUM_GREY);   noise_l  = ui_slider(60, 356, 30,  92, noise_l,  0, 7,    CLR_LIGHT_GREY);
    print("GLIDE", 316, 44, CLR_MEDIUM_GREY);   glide_ms = ui_slider(61, 356, 44,  92, glide_ms, 0, 1000, CLR_YELLOW);
    print("DRIFT", 316, 58, CLR_MEDIUM_GREY);   drift    = ui_slider(62, 356, 58,  92, drift,    0, 0.3f, CLR_PINK);
    print("DRIVE", 316, 72, CLR_MEDIUM_GREY);   drive_v  = ui_slider(63, 356, 72,  92, drive_v,  0, 1,    CLR_ORANGE);
    print("TRACK", 316, 86, CLR_MEDIUM_GREY);   ktrack   = ui_slider(64, 356, 86,  92, ktrack,   0, 1,    CLR_GREEN);
    print("SYNC",  316, 100, CLR_MEDIUM_GREY);  sync_amt = ui_slider(65, 356, 100, 92, sync_amt, 1, 5,    CLR_MAUVE);
    font(FONT_NORMAL);

    // ---- ENVELOPE (AMP ADSR / FILTER contour / PITCH env) ----
    const char *evn[3]  = { "AMP", "FILTER", "PITCH" };
    int         evcol[3] = { CLR_GREEN, CLR_DARK_PEACH, CLR_MAUVE };
    panel(6, 116, 296, 92, "", evcol[env_view]);
    for (int i = 0; i < 3; i++)
        if (ui_btn(10 + i * 56, 119, 52, 12, evn[i], env_view == i, evcol[i])) env_view = i;
    if      (env_view == 0) draw_adsr(14, 140, 280, 52);
    else if (env_view == 1) draw_adenv(14, 140, 280, 52, &fenv_atk, 600, &fenv_dec, 800, &fenv_amt, 3000, evcol[1]);
    else                    draw_adenv(14, 140, 280, 52, &penv_atk, 400, &penv_dec, 600, &penv_amt, 36,   evcol[2]);

    // ---- FILTER ----
    const char *fn[6] = { "OFF", "LP", "HP", "BP", "NF", "LAD" };
    panel(306, 116, 148, 92, "FILTER", CLR_ORANGE);
    for (int i = 0; i < 6; i++)
        if (ui_btn(310 + i * 24, 130, 22, 13, fn[i], fmode == i, CLR_ORANGE)) fmode = i;
    ui_xy(2, 312, 148, 136, 44, &cutoff, 80, 4000, &res, 0, 15, CLR_ORANGE);
    print("RES", 314, 150, CLR_DARK_GREY);
    print("CUT", 424, 184, CLR_DARK_GREY);
    print(str("cut %dhz  res %d", (int)cutoff, (int)(res + 0.5f)), 312, 196, CLR_MEDIUM_GREY);

    // ---- 3 LFOs ----
    const char *tn[7] = { "OFF", "PITCH", "DUTY", "VOL", "CUT", "DRIVE", "SYNC" };
    int lcol[3] = { CLR_PINK, CLR_YELLOW, CLR_INDIGO };
    for (int L = 0; L < 3; L++) {
        int x = 6 + L * 151, y = 212, w = 146;
        panel(x, y, w, 62, str("LFO %d", L + 1), lcol[L]);
        if (ui_btn(x + 6, y + 14, 64, 12, tn[lfos[L].target], lfos[L].target != 0, lcol[L]))
            lfos[L].target = (lfos[L].target + 1) % 7;
        if (lfos[L].target != 0) {
            int dotx = x + w - 16 + (int)(sin_deg(now() * lfos[L].rate * 360.0f) * 7);
            circfill(dotx, y + 20, 3, lcol[L]);
        }
        print("RATE", x + 6, y + 32, CLR_MEDIUM_GREY);
        lfos[L].rate = ui_slider(20 + L, x + 38, y + 32, w - 46, lfos[L].rate, 0.1f, 12, lcol[L]);
        print("DEPTH", x + 6, y + 50, CLR_MEDIUM_GREY);
        lfos[L].depth = ui_slider(30 + L, x + 44, y + 50, w - 52, lfos[L].depth, 0, 1, lcol[L]);
    }

    // ---- info line (tappable [..] labels) ----
    int ix = print(str("OCT C%d   ", keybed_octave()), 8, 280, CLR_LIGHT_GREY);
    octdn_x = ix; ix = print("[Z -]", ix, 280, CLR_LIGHT_GREY) + 4;
    octup_x = ix; print("[X +]", ix, 280, CLR_LIGHT_GREY);
    ix = print(str("VELOCITY %d   ", vel), 250, 280, CLR_LIGHT_GREY);
    veldn_x = ix; ix = print("[C -]", ix, 280, CLR_LIGHT_GREY) + 4;
    velup_x = ix; print("[V +]", ix, 280, CLR_LIGHT_GREY);

    // ---- keyboard ----
    int nw = keybed_white_count();
    for (int k = 0; k < nw; k++) {
        int x, y, w, h; keybed_white_rect(k, &x, &y, &w, &h);
        int midi = keybed_white_midi(k);
        rectfill(x, y, w - 1, h, keybed_held(midi) ? CLR_YELLOW : CLR_WHITE);
        rect(x, y, w - 1, h, CLR_DARK_GREY);
        print(str("%c", KEYBED_WHITE_KEYS[k]), x + w / 2 - 3, y + h - 12, CLR_DARK_GREY);
    }
    for (int k = 0; k < nw; k++) {
        int x, y, w, h, midi; if (!keybed_black_rect(k, &x, &y, &w, &h, &midi)) continue;
        rectfill(x, y, w, h, keybed_held(midi) ? CLR_ORANGE : CLR_BROWNISH_BLACK);
        if (KEYBED_BLACK_KEYS[k] != ' ') print(str("%c", KEYBED_BLACK_KEYS[k]), x + w / 2 - 2, y + h - 14, CLR_WHITE);
    }

    // LIVE: every ringing voice follows the filter, drive, tracking and LFO knobs this frame.
    // WHITE noise rides the main filter with the oscillators; PINK noise stays dark (a fixed
    // lowpass) so it reads as a separate rumble instead of sweeping with the cutoff.
    float sr = sync_ratio_eng(sync_now());               // OSC2's swept hard-sync ratio this frame
    for (int m = 0; m < 128; m++) if (keybed_held(m)) {
        int tc = tracked_cutoff(m);
        for (int i = 0; i < 3; i++) drive_live(h_osc[i][m], tc);
        if (h_osc[1][m] >= 0) note_sync(h_osc[1][m], sr);   // ride the sync sweep under your fingers
        if (h_nz[m] >= 0) {
            if (pink) { note_filter(h_nz[m], FILTER_LOW); note_cutoff(h_nz[m], 900); note_res(h_nz[m], 1); note_drive(h_nz[m], drive_now()); }
            else        drive_live(h_nz[m], tc);
        }
    }

    seen_n = 0;
    for (int i = 0; i < touch_count() && seen_n < 12; i++) seen_ids[seen_n++] = touch_id(i);
}
