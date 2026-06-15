#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <math.h>
#include <string.h>

// ── ROLAND SH-101 ─────────────────────────────────────────────────────────
// Roland's SH-101 (1982) — a monophonic lead/bass synth sold with a guitar
// strap and a built-in sequencer + arpeggiator. The cart models the real
// panel, section for section, left to right:
//
//   TUNE        master tune knob (±1 semitone)
//   MODULATOR   LFO — RATE slider + WAVE FORM knob: tri / square / random
//               (sample & hold) / noise. The rate slider is also the CLOCK
//               for the sequencer and arpeggiator, exactly like the real
//               LFO/CLK RATE slider.
//   VCO         MOD slider (LFO→pitch vibrato), RANGE rotary (16' 8' 4' 2'),
//               PULSE WIDTH slider + source switch LFO / MAN / ENV
//   SOURCE MIXER  the heart of the 101: pulse + saw + sub-osc + noise all
//               mixed SIMULTANEOUSLY with four faders. Each source is its
//               own engine voice (4 of 8 — fine for a monosynth); the
//               faders ride live on a held note via note_vol(). The sub
//               switch picks -1 oct square / -2 oct square / -2 oct pulse.
//   VCF         FREQ, RES, ENV (filter envelope amount), MOD (LFO→cutoff),
//               KYBD (keyboard tracking) — five sliders, all live.
//   VCA         ENV / GATE switch (ADSR shaped, or plain organ gate)
//   ENV         one ADSR shared by VCA and VCF env amount
//
// Below the panel, one strip: the SEQUENCER (LOAD records key presses, PLAY
// loops them at the clock rate), the ARPEGGIO buttons (DOWN / U&D / UP — tap
// one to run it on whatever keys you hold, tap again to stop), HOLD (latches
// keys: chords keep arpeggiating after you let go), then VOLUME, PORTAMENTO
// (+ AUTO/OFF/ON — AUTO glides only when you play legato) and TRANSPOSE L/M/H.
// The springy pitch BENDER stands vertically at the keybed's left end, where
// the real lever lives. The keybed itself gets the bottom half of the screen.
//
//   KEYBOARD  two manuals, every onscreen key covered:
//             lower:  Z X C V B N M , . /  whites + S D G H J L ; blacks
//             upper:  Q W E R T Y U I O P  whites + 2 3 5 6 7 9 0 blacks
//             top:    [ ] whites, = black      UP / DOWN arrows = octave
//   TOUCH     every finger is its own pointer — hold a chord and ride the
//             filter at the same time. Fader grab zones tile each section,
//             so a near-miss lands on the nearest fader; sliding a finger
//             across the keys hands the note over legato (AUTO porta glides).
//             The desktop mouse is one synthetic finger; wheel works on hover.
//   SPACE     toggle arpeggiator (last mode used)
//   ?         help overlay (bottom left corner)

// ── engine slots: one per mixer source ───────────────────────────────────
#define SL_PULSE 5
#define SL_SAW   6
#define SL_SUB   7
#define SL_NOISE 8
#define NSRC     4
static const int SRC_SLOT[NSRC] = { SL_PULSE, SL_SAW, SL_SUB, SL_NOISE };

#define NONE -999

// ── panel state (sliders/knobs hold 0..1, mapped at the point of use) ─────
static float tune_v   = 0.5f;   // ±1 semitone, 0.5 = center
static float rate_v   = 0.45f;  // MODULATOR rate — also the seq/arp clock
static int   lfo_wave = 0;      // 0=TRI 1=SQR 2=RND(S&H) 3=NOISE
static float vmod_v   = 0.0f;   // VCO MOD: LFO→pitch depth
static int   range_sel= 1;      // 0..3 → 16' 8' 4' 2'
static float pw_v     = 0.30f;  // pulse width amount
static int   pw_src   = 1;      // 0=LFO 1=MAN 2=ENV
static float lv[NSRC] = { 0.0f, 1.0f, 0.75f, 0.0f };   // pulse saw sub noise
static int   sub_mode = 0;      // 0=-1oct sqr  1=-2oct sqr  2=-2oct pulse
static float cut_v  = 0.55f, res_v = 0.15f, fenv_v = 0.35f, fmod_v = 0.0f, kyb_v = 0.5f;
static int   vca_gate = 0;      // 0=ENV 1=GATE
static float a_v = 0.02f, d_v = 0.40f, s_v = 0.50f, r_v = 0.20f;
static float mvol    = 0.8f;    // VOLUME knob
static float porta_v = 0.25f;   // PORTAMENTO knob
static int   porta_mode = 1;    // 0=AUTO 1=OFF 2=ON
static float bend    = 0.0f;    // bender, ±1 → ±2 semitones, springs back

// ── value mappings ────────────────────────────────────────────────────────
static float f_cut(float v)  { return 60.0f * powf(2.0f, v * 6.3f); }  // 60..4700 Hz
static int   f_res(float v)  { return (int)(v * 15.0f + 0.5f); }
static int   f_att(float v)  { return 1 + (int)(v * v * 1500.0f); }
static int   f_dec(float v)  { return 10 + (int)(v * v * 2000.0f); }
static int   f_sus(float v)  { return (int)(v * 7.0f + 0.5f); }
static int   f_rel(float v)  { return 5 + (int)(v * v * 2500.0f); }
static float f_rate(float v) { return 0.2f + v * v * 19.8f; }           // 0.2..20 Hz
static int   f_porta(float v){ return (int)(v * 500.0f); }              // ms
static float f_fenv(float v) { return v * 3800.0f; }                    // Hz
static float f_fmod(float v) { return v * 2500.0f; }                    // Hz
static float f_vmod(float v) { return v * 3.0f; }                       // semitones
static float tune_semi(void) { return (tune_v - 0.5f) * 2.0f; }
static float duty_man(void)  { return 0.5f - pw_v * 0.42f; }            // 0.5..0.08
static int   vol_of(int s)   { return (int)(lv[s] * mvol * 7.0f + 0.5f); }

// ── mono voice: 4 source voices for the one held note ─────────────────────
static int   v[NSRC] = { -1, -1, -1, -1 };
static int   cur_midi = NONE;

// held keys: phys = physically down, latch = what HOLD keeps alive
#define MAX_HELD 16
static int phys[MAX_HELD], nphys = 0;
static int latch[MAX_HELD], nlatch = 0;
static int hold_on = 0;

// ── sequencer / arpeggiator ───────────────────────────────────────────────
#define MAX_SEQ 64
static int    seq[MAX_SEQ], seq_n = 0, seq_idx = 0;
static int    seq_load = 0, seq_play = 0;
static int    arp_mode = -1;             // -1=off 0=DOWN 1=U&D 2=UP
static int    arp_last = 2;              // last mode, for SPACE toggle
static int    arp_idx = 0, arp_dir = 1;
static double next_step = -1.0;
static float  step_flash = 0.0f;         // PLAY-LED blink
static int    base = 48;                 // leftmost white key MIDI (C3)
static int    show_help = 0;

// ── UI pointer state — every finger is its own pointer ───────────────────
// A finger lands on a widget and CAPTURES it (generously: fader grab boxes
// tile the whole section, so a near-miss grabs the nearest fader); after
// capture only vertical motion matters. Other fingers are free to hold keys,
// ride other faders, lean on the bender — all at once. The desktop mouse
// arrives as one synthetic finger, same code path; the wheel stays a
// desktop-only bonus on hover.
typedef struct {
    int id;        // touch id, PTR_NONE = slot free
    int widget;    // 0 = free, fader/knob id, 98 = keyboard, 99 = bender
    int cx, cy;    // current position, refreshed each frame
    int m0y;       // y at capture
    float v0;      // widget value at capture
    int midi;      // note this finger is sounding (keyboard), or NONE
    int fresh;     // landed this frame
} Ptr;             // id MUST be first (pointer.h)
static Ptr ptrs[PTR_MAX];
static int mx, my;             // mouse position — hover highlights + wheel only

static void key_up(int midi);  // fwd (touch-end releases notes)

// sync the pointer table with the engine's touch list: refresh positions,
// admit new fingers (fresh), release whatever lifted fingers were doing
static void ptrs_begin_frame(void) {
    for (int j = 0; j < PTR_MAX; j++) ptrs[j].fresh = 0;
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptrs, id, &fresh);
        if (!p) continue;
        if (fresh) *p = (Ptr){ id, 0, 0, 0, 0, 0.0f, NONE, 1 };
        p->cx = touch_x(i); p->cy = touch_y(i);
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptrs, touch_ended_id(i));
        if (!p) continue;
        if (p->midi != NONE) key_up(p->midi);
        p->id = PTR_NONE;
    }
}

// a fresh, uncommitted finger inside this box — the widget capture test
static Ptr *grab_fresh(int x, int y, int w, int h) {
    for (int j = 0; j < PTR_MAX; j++)
        if (ptrs[j].id != PTR_NONE && ptrs[j].fresh && ptrs[j].widget == 0 &&
            ptrs[j].cx >= x && ptrs[j].cx < x + w &&
            ptrs[j].cy >= y && ptrs[j].cy < y + h)
            return &ptrs[j];
    return 0;
}

// tap targets that sit INSIDE a knob's grab box (wave icons, range feet) must
// run first and CONSUME the finger, or the knob steals the tap and reverts it
static int claim_tap(int x, int y, int w, int h) {
    Ptr *g = grab_fresh(x, y, w, h);
    if (g) { g->widget = -1; return 1; }
    return 0;
}

// ── colors ────────────────────────────────────────────────────────────────
#define C_BODY   CLR_DARK_GREY
#define C_DARK   CLR_BROWNISH_BLACK
#define C_LABEL  CLR_LIGHT_GREY
#define C_TICK   CLR_LIGHT_GREY
#define C_ACT    CLR_ORANGE

// ─────────────────────────────────────────────────────────────────────────
// instrument definitions — re-run whenever a panel value changes
// ─────────────────────────────────────────────────────────────────────────

static const int RANGE_SEMI[4] = { -12, 0, 12, 24 };

static float src_pitch(int s, int midi) {
    float m = midi + RANGE_SEMI[range_sel];
    if (s == 2) m -= (sub_mode == 0) ? 12 : 24;
    return m;
}

static void define_slots(void) {
    int att = vca_gate ? 2  : f_att(a_v);
    int dec = vca_gate ? 10 : f_dec(d_v);
    int sus = vca_gate ? 7  : f_sus(s_v);
    int rel = vca_gate ? 12 : f_rel(r_v);
    int cut = (int)f_cut(cut_v);
    int rs  = f_res(res_v);
    // tri(0)/square(1)/S&H(2) now run on the ENGINE LFO (sample-accurate) via the unified shapes;
    // only NOISE(3) stays a software LFO in update() (no engine white-noise LFO). lfo_wave 0 keeps
    // the historical engine SINE so the default patch is unchanged.
    int eng = (lfo_wave <= 2);
    int eng_shape = (lfo_wave == 1) ? LFO_SHAPE_SQUARE : (lfo_wave == 2) ? LFO_SHAPE_SH : LFO_SHAPE_SINE;
    float rate = f_rate(rate_v);

    for (int s = 0; s < NSRC; s++) {
        int slot = SRC_SLOT[s];
        int wave = (s == 1) ? INSTR_SAW : (s == 3) ? INSTR_NOISE : INSTR_SQUARE;
        instrument(slot, wave, att, dec, sus, rel);
        instrument_tune(slot, tune_semi());   // the TUNE trimmer — slot-level, so
                                              // arp/seq hits and ringing notes all bend
        instrument_filter(slot, FILTER_LOW, cut, rs);
        // filter envelope (shared ADSR shapes its A/D)
        instrument_env(slot, 0, ENV_CUTOFF, f_att(a_v), f_dec(d_v), f_fenv(fenv_v));
        // LFO → pitch (vibrato) + cutoff, engine-driven for sine/square/S&H (the chosen waveform)
        instrument_lfo(slot, 0, LFO_PITCH,  rate, (eng && s != 3) ? f_vmod(vmod_v) : 0.0f);
        instrument_lfo(slot, 1, LFO_CUTOFF, rate, eng ? f_fmod(fmod_v) : 0.0f);
        lfo_shape(slot, 0, eng_shape);
        lfo_shape(slot, 1, eng_shape);
        instrument_echo(slot, 0.08f);   // the delay pedal on the guitar-strap synth — barely-there slapback
    }
    // pulse-specific: width + its modulation source
    instrument_duty(SL_PULSE, duty_man());
    instrument_lfo(SL_PULSE, 2, LFO_DUTY, rate,
                   (eng && pw_src == 0) ? pw_v * 0.4f : 0.0f);
    lfo_shape(SL_PULSE, 2, eng_shape);
    instrument_env(SL_PULSE, 1, ENV_DUTY, f_att(a_v), f_dec(d_v),
                   (pw_src == 2) ? pw_v * 0.45f : 0.0f);
    instrument_duty(SL_SUB, (sub_mode == 2) ? 0.25f : 0.5f);
}

// redefine only when something actually moved
typedef struct {
    float lv[NSRC], cut, res, fenv, fmod, a, d, s, r, rate, vmod, pw, mvol, tune;
    int gate, lwave, pwsrc, submode;
} Params;
static Params prev_p;

static void sync_slots(void) {
    Params p;
    memcpy(p.lv, lv, sizeof lv);
    p.cut = cut_v; p.res = res_v; p.fenv = fenv_v; p.fmod = fmod_v;
    p.a = a_v; p.d = d_v; p.s = s_v; p.r = r_v;
    p.rate = rate_v; p.vmod = vmod_v; p.pw = pw_v; p.mvol = mvol;
    p.tune = tune_v;
    p.gate = vca_gate; p.lwave = lfo_wave; p.pwsrc = pw_src; p.submode = sub_mode;
    if (memcmp(&p, &prev_p, sizeof p) != 0) { prev_p = p; define_slots(); }
}

// ─────────────────────────────────────────────────────────────────────────
// voice control — one note, four source voices
// ─────────────────────────────────────────────────────────────────────────

static float eff_cut(int midi) {
    float c = f_cut(cut_v) * powf(2.0f, (midi - 60) / 12.0f * kyb_v);
    return clamp(c, 40.0f, 8000.0f);
}

static void stop_note(void) {
    for (int s = 0; s < NSRC; s++)
        if (v[s] >= 0) { note_off(v[s]); v[s] = -1; }
    cur_midi = NONE;
}

static void start_note(int midi) {
    stop_note();
    define_slots();
    for (int s = 0; s < NSRC; s++) {
        v[s] = note_on((int)src_pitch(s, midi), SRC_SLOT[s], vol_of(s));
        if (v[s] >= 0) {
            note_glide(v[s], 0);
            note_pitch(v[s], src_pitch(s, midi));   // slot tune carries the trimmer
            note_cutoff(v[s], (int)eff_cut(midi));
        }
    }
    cur_midi = midi;
}

static void glide_to(int midi) {
    int ms = f_porta(porta_v);
    for (int s = 0; s < NSRC; s++)
        if (v[s] >= 0) {
            note_glide(v[s], ms);
            note_pitch(v[s], src_pitch(s, midi));   // slot tune carries the trimmer
        }
    cur_midi = midi;
}

// ─────────────────────────────────────────────────────────────────────────
// key handling — last-note priority, HOLD latch, portamento modes
// ─────────────────────────────────────────────────────────────────────────

static void stack_push(int *st, int *n, int midi) {
    for (int i = 0; i < *n; i++) if (st[i] == midi) return;
    if (*n < MAX_HELD) st[(*n)++] = midi;
}
static void stack_pop(int *st, int *n, int midi) {
    for (int i = 0; i < *n; i++)
        if (st[i] == midi) {
            for (int j = i; j < *n - 1; j++) st[j] = st[j + 1];
            (*n)--; return;
        }
}

static int arp_running(void) { return arp_mode >= 0 && nlatch > 0 && !seq_play; }

static void key_down(int midi) {
    int legato = nphys > 0;
    if (hold_on && nphys == 0) nlatch = 0;     // new latch group
    stack_push(phys, &nphys, midi);
    stack_push(latch, &nlatch, midi);
    if (seq_load && seq_n < MAX_SEQ) seq[seq_n++] = midi;
    if (arp_mode >= 0 || seq_play) return;     // clocked modes own the voice
    int want_glide = cur_midi != NONE &&
                     (porta_mode == 2 || (porta_mode == 0 && legato));
    if (want_glide) glide_to(midi);
    else            start_note(midi);
}

static void key_up(int midi) {
    stack_pop(phys, &nphys, midi);
    if (!hold_on) stack_pop(latch, &nlatch, midi);
    if (arp_mode >= 0 || seq_play) return;
    if (hold_on) return;                        // latched: keep ringing
    if (nphys > 0) {
        int back = phys[nphys - 1];
        if (back == cur_midi) return;
        if (porta_mode != 1) glide_to(back);    // falling back is legato
        else                 start_note(back);
    } else stop_note();
}

static void arp_toggle(int mode) {
    if (arp_mode == mode) arp_mode = -1;
    else { arp_mode = mode; arp_last = mode; arp_idx = 0; arp_dir = 1; stop_note(); }
    next_step = -1.0;
}

// ─────────────────────────────────────────────────────────────────────────
// clock — sequencer + arpeggiator both step at the MODULATOR rate
// ─────────────────────────────────────────────────────────────────────────

static int arp_next(void) {
    if (nlatch == 0) return -1;
    int pool[MAX_HELD];
    memcpy(pool, latch, nlatch * sizeof(int));
    for (int i = 0; i < nlatch - 1; i++)        // sort ascending
        for (int j = i + 1; j < nlatch; j++)
            if (pool[j] < pool[i]) { int t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    if (arp_idx >= nlatch) arp_idx = 0;
    int p = pool[arp_idx];
    if (arp_mode == 2)      arp_idx = (arp_idx + 1) % nlatch;            // UP
    else if (arp_mode == 0) arp_idx = (arp_idx - 1 + nlatch) % nlatch;   // DOWN
    else if (nlatch > 1) {                                               // U&D
        arp_idx += arp_dir;
        if (arp_idx >= nlatch - 1) { arp_idx = nlatch - 1; arp_dir = -1; }
        if (arp_idx <= 0)          { arp_idx = 0;          arp_dir =  1; }
    }
    return p;
}

static void clock_tick(void) {
    int running = seq_play || arp_running();
    if (!running) { next_step = -1.0; return; }
    double t  = now();
    double sp = 1.0 / f_rate(rate_v);
    if (sp < 0.03) sp = 0.03;
    if (next_step < 0) next_step = t + 0.02;
    while (next_step < t + 0.10) {
        int dly = (int)((next_step - t) * 1000.0); if (dly < 1) dly = 1;
        int midi;
        if (seq_play) { midi = seq[seq_idx]; seq_idx = (seq_idx + 1) % seq_n; }
        else            midi = arp_next();
        if (midi >= 0) {
            sync_slots();
            int dur = (int)(sp * 700.0);        // 70% gate, like the real seq
            for (int s = 0; s < NSRC; s++)
                if (lv[s] > 0.04f)
                    schedule_hit(dly, (int)src_pitch(s, midi), SRC_SLOT[s], vol_of(s), dur);
            step_flash = 1.0f;
        }
        next_step += sp;
    }
}

// ─────────────────────────────────────────────────────────────────────────
// live drive — sliders ride the held voices; software LFO for SQR/RND/NOISE
// ─────────────────────────────────────────────────────────────────────────

static int pitch_override = 0;

static void drive_live(void) {
    float rate = f_rate(rate_v);

    // square/S&H now run on the engine LFO (sample-accurate); NOISE is the only software LFO left
    // (no engine white-noise shape) — full-rate random pushed per frame as before.
    int eng = (lfo_wave <= 2);
    int eng_shape = (lfo_wave == 1) ? LFO_SHAPE_SQUARE : (lfo_wave == 2) ? LFO_SHAPE_SH : LFO_SHAPE_SINE;
    float lval = (lfo_wave == 3) ? rnd_float_between(-1.0f, 1.0f) : 0.0f;
    int soft = (lfo_wave == 3);

    if (cur_midi == NONE) { pitch_override = 0; return; }

    float fcut = eff_cut(cur_midi);
    if (soft) fcut = clamp(fcut + lval * f_fmod(fmod_v), 40.0f, 8000.0f);

    for (int s = 0; s < NSRC; s++) {
        if (v[s] < 0) continue;
        note_cutoff(v[s], (int)fcut);
        note_res(v[s], res_v * 15.0f);   // smooth live resonance (f_res stays int for instrument_filter)
        note_vol(v[s], vol_of(s));
        if (eng) {                                         // retune engine LFOs live (sine/square/S&H)
            note_lfo(v[s], 0, LFO_PITCH,  rate, (s != 3) ? f_vmod(vmod_v) : 0.0f);
            note_lfo(v[s], 1, LFO_CUTOFF, rate, f_fmod(fmod_v));
            note_lfo_shape(v[s], 0, eng_shape);
            note_lfo_shape(v[s], 1, eng_shape);
        }
    }
    // pulse width, live
    if (v[0] >= 0) {
        float d = duty_man();
        if (soft && pw_src == 0) d = clamp(0.5f + lval * pw_v * 0.4f, 0.05f, 0.95f);
        note_duty(v[0], d);
        if (eng) {
            note_lfo(v[0], 2, LFO_DUTY, rate, (pw_src == 0) ? pw_v * 0.4f : 0.0f);
            note_lfo_shape(v[0], 2, eng_shape);
        }
    }

    // pitch: bender + software vibrato share one per-frame write. (TUNE no
    // longer lives here — instrument_tune() carries it at the slot level, so
    // ringing notes AND scheduled arp/seq hits bend with the knob.)
    int need = (bend != 0.0f) || (soft && vmod_v > 0.01f);
    if (need || pitch_override) {
        float off = bend * 2.0f + (soft ? lval * f_vmod(vmod_v) : 0.0f);
        for (int s = 0; s < NSRC - 1; s++)                 // not the noise
            if (v[s] >= 0) {
                note_glide(v[s], 0);
                note_pitch(v[s], src_pitch(s, cur_midi) + off);
            }
    }
    pitch_override = need;
}

// ─────────────────────────────────────────────────────────────────────────
// UI widgets — modeled on the SH-101's hardware
// ─────────────────────────────────────────────────────────────────────────

static int in_box(int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

// vertical fader with tick scale and a colored cap line. gw = grab half-width:
// capture boxes tile the section (half the fader pitch each side), so a tap
// anywhere in the column grabs the nearest fader — after capture only vertical
// motion matters, horizontal precision stops mattering
static float ui_fader(int id, int x, int y, int h, float val, int capcol, int ticks, int gw) {
    Ptr *g = grab_fresh(x - gw, y - 8, gw * 2, h + 16);
    if (g) { g->widget = id; g->m0y = g->cy; g->v0 = val; }
    for (int j = 0; j < PTR_MAX; j++)
        if (ptrs[j].id != PTR_NONE && ptrs[j].widget == id)
            val = clamp(ptrs[j].v0 + (ptrs[j].m0y - ptrs[j].cy) / (float)h, 0.0f, 1.0f);
    if (in_box(x - 7, y, 15, h) && mouse_wheel() != 0)
        val = clamp(val + mouse_wheel() * 0.04f, 0.0f, 1.0f);
    if (ticks)
        for (int i = 0; i <= 10; i++) {
            int ty = y + h - i * h / 10;
            line(x - 7, ty, x - 5, ty, C_TICK);
            line(x + 5, ty, x + 7, ty, C_TICK);
        }
    rectfill(x - 1, y, 2, h, CLR_BLACK);                    // slot
    int ky = y + h - (int)(val * h);
    ky = (int)clamp(ky, y, y + h - 1);
    rectfill(x - 4, ky - 4, 9, 9, C_DARK);                  // cap
    rect(x - 4, ky - 4, 9, 9, CLR_BLACK);
    rectfill(x - 4, ky, 9, 1, capcol);                      // indicator line
    return val;
}

// rotary knob, optional detents — capture box is finger-padded, drag is vertical
static float ui_knob(int id, int x, int y, int r, float val, int detents) {
    Ptr *g = grab_fresh(x - r - 6, y - r - 6, r * 2 + 12, r * 2 + 12);
    if (g) { g->widget = id; g->m0y = g->cy; g->v0 = val; }
    for (int j = 0; j < PTR_MAX; j++)
        if (ptrs[j].id != PTR_NONE && ptrs[j].widget == id)
            val = clamp(ptrs[j].v0 + (ptrs[j].m0y - ptrs[j].cy) / 60.0f, 0.0f, 1.0f);
    if (in_box(x - r - 2, y - r - 2, r * 2 + 4, r * 2 + 4) && mouse_wheel() != 0)
        val = clamp(val + mouse_wheel() * 0.04f, 0.0f, 1.0f);
    float shown = val;
    if (detents > 1) shown = (float)(int)(val * (detents - 1) + 0.5f) / (detents - 1);
    circfill(x, y, r, C_DARK);
    circ(x, y, r, CLR_BLACK);
    float a = 3.7f - shown * 4.25f;   // 7:30 → 12:00 → 4:30, like a real pot
    line(x, y, x + (int)(cosf(a) * (r - 2)), y - (int)(sinf(a) * (r - 2)), CLR_WHITE);
    return val;
}

// small option box (the panel's switch positions) — tapp = touch-began edge,
// covers the synthetic mouse finger too
static int ui_opt(int x, int y, int w, int h, const char *label, int active) {
    int hot = in_box(x, y, w, h);
    rectfill(x, y, w, h, active ? C_ACT : C_DARK);
    rect(x, y, w, h, hot ? CLR_WHITE : CLR_BLACK);
    print(label, x + (w - text_width(label)) / 2, y + (h - 7) / 2 + 1,
          active ? CLR_BLACK : CLR_MEDIUM_GREY);
    return tapp(x - 2, y - 2, w + 4, h + 4);
}

// cream panel button with a red LED above it
static int ui_led_btn(int x, int y, const char *label, int led) {
    int w = 32, h = 12;
    int hot = in_box(x, y, w, h);
    circfill(x + w / 2, y - 5, 2, led ? CLR_RED : CLR_DARKER_GREY);
    rectfill(x, y, w, h, CLR_LIGHT_GREY);
    rect(x, y, w, h, hot ? CLR_WHITE : CLR_BLACK);
    print(label, x + (w - text_width(label)) / 2, y + 3, CLR_BLACK);
    return tapp(x - 2, y - 4, w + 4, h + 6);
}

// tiny LFO waveform icons
static void icon_wave(int which, int x, int y, int col) {
    if (which == 0) {                                       // tri
        line(x, y + 6, x + 3, y, col); line(x + 3, y, x + 6, y + 6, col);
        line(x + 6, y + 6, x + 9, y, col);
    } else if (which == 1) {                                // square
        line(x, y, x + 4, y, col); line(x + 4, y, x + 4, y + 6, col);
        line(x + 4, y + 6, x + 8, y + 6, col); line(x + 8, y + 6, x + 8, y, col);
    } else if (which == 2) {                                // random steps
        line(x, y + 4, x + 3, y + 4, col); line(x + 3, y + 1, x + 6, y + 1, col);
        line(x + 6, y + 6, x + 9, y + 6, col);
        line(x + 3, y + 1, x + 3, y + 4, col); line(x + 6, y + 1, x + 6, y + 6, col);
    } else if (which == 3) {                                // noise
        pset(x, y + 3, col); pset(x + 2, y + 5, col); pset(x + 3, y + 1, col);
        pset(x + 5, y + 4, col); pset(x + 6, y, col); pset(x + 8, y + 5, col);
        pset(x + 9, y + 2, col);
    } else {                                                // saw
        line(x, y + 6, x + 4, y, col); line(x + 4, y, x + 4, y + 6, col);
        line(x + 4, y + 6, x + 8, y, col);
    }
}

// chunky slanted logo lettering
static void big_glyph(const unsigned char rows[5], int x, int y, int cell, int col) {
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 5; c++)
            if (rows[r] & (1 << (4 - c)))
                rectfill(x + c * cell + (4 - r), y + r * cell, cell, cell, col);
}

static void draw_logo(int x, int y, int cell, int spacing) {
    static const unsigned char S[5] = { 0x1F, 0x10, 0x1F, 0x01, 0x1F };
    static const unsigned char H[5] = { 0x11, 0x11, 0x1F, 0x11, 0x11 };
    static const unsigned char D[5] = { 0x00, 0x00, 0x0E, 0x00, 0x00 };
    static const unsigned char N1[5] = { 0x06, 0x0E, 0x06, 0x06, 0x06 };
    static const unsigned char N0[5] = { 0x1F, 0x11, 0x11, 0x11, 0x1F };
    const unsigned char *g[6] = { S, H, D, N1, N0, N1 };
    print("Roland", x + 2, y - 11, CLR_WHITE);
    for (int i = 0; i < 6; i++) big_glyph(g[i], x + i * spacing, y, cell, CLR_WHITE);
}

// ─────────────────────────────────────────────────────────────────────────
// keyboard (19 white + 13 black = 2.5 octaves, like the real 32 keys)
// ─────────────────────────────────────────────────────────────────────────
#define NWHITE 19
#define NBLACK 13
#define KBX   20
#define KBY  152
#define KBH  146
#define KBW   23
#define BKW   13          // black key width
#define BKH   90          // black key height
static const int  W_SEMI[NWHITE]  = { 0,2,4,5,7,9,11,12,14,16,17,19,21,23,24,26,28,29,31 };
static const int  B_SEMI[NBLACK]  = { 1,3,6,8,10,13,15,18,20,22,25,27,30 };
static const int  B_AFTER[NBLACK] = { 0,1,3,4,5,7,8,10,11,12,14,15,17 };

// two-manual computer keyboard: Z row + S row = lower octave, Q row + number
// row = upper octave (overlapping, like a DAW), [ ] = the top white keys.
// Every one of the 32 onscreen keys has a computer key.
static const struct { char k; int semi; } KMAP[] = {
    { 'Z', 0 }, { 'X', 2 }, { 'C', 4 }, { 'V', 5 }, { 'B', 7 }, { 'N', 9 },
    { 'M', 11 }, { ',', 12 }, { '.', 14 }, { '/', 16 },
    { 'S', 1 }, { 'D', 3 }, { 'G', 6 }, { 'H', 8 }, { 'J', 10 }, { 'L', 13 }, { ';', 15 },
    { 'Q', 12 }, { 'W', 14 }, { 'E', 16 }, { 'R', 17 }, { 'T', 19 }, { 'Y', 21 },
    { 'U', 23 }, { 'I', 24 }, { 'O', 26 }, { 'P', 28 },
    { '2', 13 }, { '3', 15 }, { '5', 18 }, { '6', 20 }, { '7', 22 }, { '9', 25 }, { '0', 27 },
    { '[', 29 }, { ']', 31 }, { '=', 30 },
};
#define NKMAP ((int)(sizeof KMAP / sizeof KMAP[0]))
static const char WLBL[NWHITE + 1] = "ZXCVBNM,./RTYUIOP[]";
static const char BLBL[NBLACK + 1] = "SDGHJL;56790=";

// the MIDI note each computer key is currently sounding — releases must pop
// the note that was actually pressed, not base+semi recomputed (the octave
// may have shifted mid-hold, which used to leave a stuck note)
static int kmap_midi[NKMAP];

static int white_kx(int i) { return KBX + i * KBW; }
static int black_kx(int j) { return white_kx(B_AFTER[j]) + KBW - BKW / 2 - 1; }

// which key sits at this point? black keys win in their upper region
static int key_at(int x, int y) {
    if (y < KBY || y >= KBY + KBH || x < KBX || x >= KBX + NWHITE * KBW) return NONE;
    if (y < KBY + BKH)
        for (int j = 0; j < NBLACK; j++)
            if (x >= black_kx(j) && x < black_kx(j) + BKW) return base + B_SEMI[j];
    for (int i = 0; i < NWHITE; i++)
        if (x >= white_kx(i) && x < white_kx(i) + KBW - 1) return base + W_SEMI[i];
    return NONE;
}

static int sh_held(int midi) {
    for (int i = 0; i < nlatch; i++) if (latch[i] == midi) return 1;
    for (int i = 0; i < nphys;  i++) if (phys[i]  == midi) return 1;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────
// init / update / draw
// ─────────────────────────────────────────────────────────────────────────

void init(void) {
    define_slots();
    memset(&prev_p, 0xFF, sizeof prev_p);   // force first sync_slots
    for (int i = 0; i < NKMAP; i++) kmap_midi[i] = NONE;
    PTR_CLEAR(ptrs);
}

void update(void) {
    // ── keybed.h EXCEPTION ───────────────────────────────────────────────────
    // This cart does NOT use runtime/keybed.h (the shared keybed). Its keyboard is
    // drawn as TWO stacked rows (two octaves of one mono synth), which keybed.h's
    // single-row layout can't render yet. So the keyboard stays hand-rolled and MIDI
    // is wired here cart-side: drain the engine's midi_get() into key_down/key_up.
    // Full keybed.h adoption is deferred — see docs/design/midi-and-keybed.md →
    // "Deferred — exceptional cases". MIDI plays absolute pitches, independent of the
    // on-screen octave, like a real keyboard. (No velocity yet.)
    // Mono-sounding, but key_down/key_up push/pop the held stack (phys/latch), so holding
    // a MIDI CHORD feeds the ARPEGGIATOR + HOLD — poly-held even though it's mono-voiced.
    int mn, mv, mt;
    while ((mt = midi_get(&mn, &mv)) != 0) { if (mt > 0) key_down(mn); else key_up(mn); }

    for (int i = 0; i < NKMAP; i++) {
        if (keyp(KMAP[i].k)) { kmap_midi[i] = base + KMAP[i].semi; key_down(kmap_midi[i]); }
        if (keyr(KMAP[i].k) && kmap_midi[i] != NONE) { key_up(kmap_midi[i]); kmap_midi[i] = NONE; }
    }
    if (keyp(KEY_DOWN)) base = (int)clamp(base - 12, 12, 84);
    if (keyp(KEY_UP))   base = (int)clamp(base + 12, 12, 84);
    if (keyp(' ')) arp_toggle(arp_mode >= 0 ? arp_mode : arp_last);

    sync_slots();
    clock_tick();
    drive_live();
    step_flash *= 0.82f;

#ifdef DE_TRACE
    watch("cutoff", "%d", (int)f_cut(cut_v));
    watch("nlatch", "%d", nlatch);
    watch("arp", "%d", arp_mode);
    watch("seq", "%d/%d %s", seq_idx, seq_n, seq_play ? "PLAY" : seq_load ? "LOAD" : "-");
    watch("clock", "%.1fHz", f_rate(rate_v));
    watch("tune", "%+.2f", tune_semi());
#endif
}

void draw(void) {
    mx = mouse_x(); my = mouse_y();
    ptrs_begin_frame();

    cls(C_BODY);

    // ── top edge: jack row flavor ───────────────────────────────────────
    rectfill(0, 0, SCREEN_W, 7, C_DARK);
    for (int i = 0; i < 8; i++) {
        int jx = 330 + i * 16;
        circfill(jx, 3, 2, CLR_BLACK);
        circ(jx, 3, 2, CLR_MEDIUM_GREY);
    }
    print("MODULATION GRIP", 4, 0, CLR_DARKER_GREY);

    // ── section header band ─────────────────────────────────────────────
    line(0, 8, SCREEN_W, 8, C_DARK);
    line(0, 20, SCREEN_W, 20, C_DARK);
    line(0, 114, SCREEN_W, 114, C_DARK);
    static const int SEC_X[8] = { 0, 44, 122, 212, 288, 378, 416, 460 };
    static const char *SEC_N[7] = { "TUNE", "MODULATOR", "VCO", "MIXER", "VCF", "VCA", "ENV" };
    for (int i = 0; i < 7; i++) {
        int x0 = SEC_X[i], x1 = SEC_X[i + 1];
        print(SEC_N[i], x0 + (x1 - x0 - text_width(SEC_N[i])) / 2, 10, CLR_WHITE);
        if (i) line(x0, 8, x0, 114, C_DARK);
    }

    int SLY = 40, SLH = 58;

    // ── TUNE ────────────────────────────────────────────────────────────
    tune_v = ui_knob(1, 22, 58, 9, tune_v, 0);
    print("-", 6, 68, C_LABEL); print("+", 32, 68, C_LABEL);

    // ── MODULATOR ───────────────────────────────────────────────────────
    print("RATE", 48, 26, C_LABEL);
    rate_v = ui_fader(2, 64, SLY, SLH, rate_v, CLR_GREEN, 1, 8);
    print("WAVE", 86, 26, C_LABEL);
    {
        for (int w = 0; w < 4; w++)                         // taps first — see claim_tap
            if (claim_tap(92, 62 + w * 12, 18, 12)) lfo_wave = w;
        float wv = ui_knob(3, 100, 50, 8, lfo_wave / 3.0f, 4);
        lfo_wave = (int)(wv * 3.0f + 0.5f);
        for (int w = 0; w < 4; w++)
            icon_wave(w, 96, 64 + w * 12, lfo_wave == w ? C_ACT : CLR_MEDIUM_GREY);
    }

    // ── VCO ─────────────────────────────────────────────────────────────
    print("MOD", 126, 24, C_LABEL);
    vmod_v = ui_fader(4, 134, SLY, SLH, vmod_v, CLR_GREEN, 1, 7);
    print("RANGE", 141, 33, C_LABEL);
    {
        static const char *FT[4] = { "16", "8", "4", "2" };
        static const int FX[4] = { 142, 148, 164, 170 }, FY[4] = { 74, 44, 44, 74 };
        for (int f = 0; f < 4; f++)                         // taps first — see claim_tap
            if (claim_tap(FX[f] - 2, FY[f] - 2, 14, 11)) range_sel = f;
        float rv = ui_knob(5, 158, 58, 9, range_sel / 3.0f, 4);
        range_sel = (int)(rv * 3.0f + 0.5f);
        for (int f = 0; f < 4; f++)
            print(FT[f], FX[f], FY[f], range_sel == f ? C_ACT : C_LABEL);
    }
    print("PW", 172, 24, C_LABEL);
    pw_v = ui_fader(6, 178, SLY, SLH, pw_v, C_ACT, 1, 7);
    if (ui_opt(186, 40, 25, 11, "LFO", pw_src == 0)) pw_src = 0;
    if (ui_opt(186, 54, 25, 11, "MAN", pw_src == 1)) pw_src = 1;
    if (ui_opt(186, 68, 25, 11, "ENV", pw_src == 2)) pw_src = 2;

    // ── SOURCE MIXER ────────────────────────────────────────────────────
    icon_wave(1, 217, 26, C_LABEL);                 // pulse
    icon_wave(4, 232, 26, C_LABEL);                 // saw
    print("SUB", 242, 35, C_LABEL);
    icon_wave(3, 265, 26, C_LABEL);                 // noise
    lv[0] = ui_fader(7,  222, SLY, SLH, lv[0], CLR_GREEN, 1, 8);
    lv[1] = ui_fader(8,  238, SLY, SLH, lv[1], CLR_GREEN, 1, 8);
    lv[2] = ui_fader(9,  254, SLY, SLH, lv[2], C_ACT,     1, 8);
    lv[3] = ui_fader(10, 270, SLY, SLH, lv[3], CLR_WHITE, 1, 8);
    if (ui_opt(216, 102, 22, 10, "-1", sub_mode == 0)) sub_mode = 0;
    if (ui_opt(240, 102, 22, 10, "-2", sub_mode == 1)) sub_mode = 1;
    if (ui_opt(264, 102, 22, 10, "2P", sub_mode == 2)) sub_mode = 2;

    // ── VCF ─────────────────────────────────────────────────────────────
    print("FRQ", 285, 24, C_LABEL);
    print("RES", 301, 33, C_LABEL);
    print("ENV", 317, 24, C_LABEL);
    print("MOD", 333, 33, C_LABEL);
    print("KYB", 349, 24, C_LABEL);
    cut_v  = ui_fader(11, 296, SLY, SLH, cut_v,  C_ACT,     1, 8);
    res_v  = ui_fader(12, 312, SLY, SLH, res_v,  CLR_WHITE, 1, 8);
    fenv_v = ui_fader(13, 328, SLY, SLH, fenv_v, C_ACT,     1, 8);
    fmod_v = ui_fader(14, 344, SLY, SLH, fmod_v, CLR_WHITE, 1, 8);
    kyb_v  = ui_fader(15, 360, SLY, SLH, kyb_v,  CLR_WHITE, 1, 8);
    print(str("%dHz", (int)f_cut(cut_v)), 290, 104, CLR_MEDIUM_GREY);

    // ── VCA ─────────────────────────────────────────────────────────────
    if (ui_opt(380, 46, 34, 12, "ENV",  !vca_gate)) vca_gate = 0;
    if (ui_opt(380, 62, 34, 12, "GATE",  vca_gate)) vca_gate = 1;

    // ── ENV ─────────────────────────────────────────────────────────────
    print("A", 417, 26, C_LABEL); print("D", 428, 26, C_LABEL);
    print("S", 439, 26, C_LABEL); print("R", 450, 26, C_LABEL);
    a_v = ui_fader(16, 420, SLY, SLH, a_v, CLR_GREEN, 0, 5);
    d_v = ui_fader(17, 431, SLY, SLH, d_v, CLR_GREEN, 0, 5);
    s_v = ui_fader(18, 442, SLY, SLH, s_v, CLR_GREEN, 0, 5);
    r_v = ui_fader(19, 453, SLY, SLH, r_v, CLR_GREEN, 0, 5);

    // ── mid strip: power, sequencer, arpeggio, hold, vol/porta/transpose,
    //    compact logo — everything that used to live left of the keybed ────
    rectfill(6, 120, 8, 12, CLR_BLACK);
    rectfill(7, 122, 6, 4, CLR_RED);
    font(FONT_SMALL);
    print("POWER", 2, 135, C_LABEL);
    print(str("OCT C%d", base / 12 - 1), 2, 144, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    if (ui_led_btn(56, 124, "LOAD", seq_load)) {
        seq_load = !seq_load;
        if (seq_load) { seq_n = 0; seq_play = 0; }
    }
    if (ui_led_btn(92, 124, "PLAY", seq_play && step_flash > 0.4f)) {
        if (seq_n > 0) { seq_play = !seq_play; seq_load = 0; seq_idx = 0; stop_note(); next_step = -1.0; }
    }
    if (ui_led_btn(140, 124, "DOWN", arp_mode == 0)) arp_toggle(0);
    if (ui_led_btn(176, 124, "U&D",  arp_mode == 1)) arp_toggle(1);
    if (ui_led_btn(212, 124, "UP",   arp_mode == 2)) arp_toggle(2);
    if (ui_led_btn(248, 124, "HOLD", hold_on)) {
        hold_on = !hold_on;
        if (!hold_on) { nlatch = nphys; memcpy(latch, phys, nphys * sizeof(int));
                        if (nphys == 0 && arp_mode < 0 && !seq_play) stop_note(); }
    }
    font(FONT_SMALL);
    print("SEQUENCER", 60, 140, C_LABEL);
    print("ARPEGGIO + HOLD", 168, 140, C_LABEL);
    font(FONT_NORMAL);
    if (seq_load) print(str("%d", seq_n), 126, 127, C_ACT);

    // volume + portamento knobs (live where the logo used to be)
    mvol    = ui_knob(20, 294, 129, 8, mvol, 0);
    porta_v = ui_knob(21, 320, 129, 8, porta_v, 0);
    font(FONT_SMALL);
    print("VOL", 287, 142, C_LABEL);
    print("PORTA", 311, 142, C_LABEL);
    font(FONT_NORMAL);
    if (ui_opt(338, 116, 26, 10, "AUT", porta_mode == 0)) porta_mode = 0;
    if (ui_opt(338, 128, 26, 10, "OFF", porta_mode == 1)) porta_mode = 1;
    if (ui_opt(338, 140, 26, 10, "ON",  porta_mode == 2)) porta_mode = 2;
    if (ui_opt(368, 116, 16, 10, "L", base == 36)) base = 36;
    if (ui_opt(368, 128, 16, 10, "M", base == 48)) base = 48;
    if (ui_opt(368, 140, 16, 10, "H", base == 60)) base = 60;

    draw_logo(386, 126, 2, 11);

    // ── BENDER: vertical, left of the keybed like the real lever ────────
    {
        int bx = 2, bw = 14, by = KBY, bh = KBH - 22;
        int bcy = by + bh / 2, range = bh / 2 - 8;
        Ptr *g = grab_fresh(bx - 2, by, bw + 6, bh);
        if (g) g->widget = 99;
        bend = 0.0f;                                  // springs back untouched
        for (int j = 0; j < PTR_MAX; j++)
            if (ptrs[j].id != PTR_NONE && ptrs[j].widget == 99)
                bend = clamp((bcy - ptrs[j].cy) / (float)range, -1.0f, 1.0f);
        rectfill(bx + bw / 2 - 1, by, 2, bh, CLR_BLACK);
        int ty = bcy - (int)(bend * range);
        rectfill(bx, ty - 4, bw, 9, C_DARK);
        rect(bx, ty - 4, bw, 9, CLR_BLACK);
        rectfill(bx, ty - 1, bw, 2, CLR_LIGHT_GREY);
    }
    if (ui_opt(2, KBY + KBH - 14, 14, 13, "?", show_help)) show_help = !show_help;

    // ── keyboard ────────────────────────────────────────────────────────
    for (int i = 0; i < NWHITE; i++) {
        int x = white_kx(i);
        int lit = sh_held(base + W_SEMI[i]);
        rectfill(x, KBY, KBW - 1, KBH, lit ? C_ACT : CLR_WHITE);
        rect(x, KBY, KBW - 1, KBH, CLR_BLACK);
        print(str("%c", WLBL[i]), x + (KBW - 9) / 2, KBY + KBH - 10, CLR_MEDIUM_GREY);
    }
    for (int j = 0; j < NBLACK; j++) {
        int x = black_kx(j);
        int lit = sh_held(base + B_SEMI[j]);
        rectfill(x, KBY, BKW, BKH, lit ? C_ACT : CLR_BLACK);
        print(str("%c", BLBL[j]), x + (BKW - 7) / 2, KBY + BKH - 12, CLR_DARK_GREY);
    }
    // per-finger keys: a fresh finger sounds its key; sliding onto another
    // key hands the note over (down-then-up = legato, so AUTO porta glides)
    for (int j = 0; j < PTR_MAX; j++) {
        Ptr *p = &ptrs[j];
        if (p->id == PTR_NONE) continue;
        if (p->fresh && p->widget == 0) {
            int m = key_at(p->cx, p->cy);
            if (m != NONE) { p->widget = 98; p->midi = m; key_down(m); }
        } else if (p->widget == 98) {
            int m = key_at(p->cx, p->cy);
            if (m != NONE && m != p->midi) {
                key_down(m);
                key_up(p->midi);
                p->midi = m;
            }
        }
    }

    // ── help overlay ─────────────────────────────────────────────────────
    if (show_help) {
        rectfill(60, 30, 340, 196, CLR_BLACK);
        rect(60, 30, 340, 196, CLR_WHITE);
        print("SH-101", 200, 38, CLR_WHITE);
        static const char *HL[] = {
            "ZXCVBNM,./ + SDGHJL; = lower manual",
            "QWERTYUIOP + 23 5679 0 = upper manual",
            "[ ] = top keys   UP/DOWN arrows = octave",
            "MIXER: pulse+saw+sub+noise, all at once",
            "MODULATOR: LFO tri/sqr/random/noise",
            "  RATE also clocks the seq + arpeggio",
            "VCO: MOD=vibrato  RANGE=16'8'4'2'",
            "  PW + source switch LFO/MAN/ENV",
            "VCF: FRQ RES ENV MOD KYB - all live",
            "VCA: ADSR envelope or plain gate",
            "SEQ: LOAD then play notes, then PLAY",
            "ARPEGGIO: DOWN/U&D/UP + HOLD latch",
            "PORTA: AUTO glides only when legato",
            "BENDER: the strip left of the keys",
            "Multitouch: chord + faders at once",
            "SPACE arp on/off    ? close help",
        };
        for (int i = 0; i < 16; i++)
            print(HL[i], 70, 46 + i * 11, i < 3 ? CLR_YELLOW : CLR_LIGHT_GREY);
    }
}
