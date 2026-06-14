// sound.h — tiny PICO-8-style synth for dreamengine
// Header-only. Include from studio.c only (defines non-static API symbols).
//
// NB for tooling/agents: because this file is only ever compiled INSIDE studio.c (after
// studio.h), an IDE/analyzer parsing it standalone reports false errors ("raylib.h not
// found", "INSTR_SQUARE undeclared", …). Ignore those; trust the real cart build.
// After changing queue sizes, request kinds, or adding bulk APIs here, run the self-test:
//   node tools/play.js soundcheck script /dev/null --headless --frames 900 | grep "\[sound\]"
// (silence = PASS; any [sound] WARNING = requests were dropped — see sound_dropped below).

#ifndef SOUND_H
#define SOUND_H

#include "raylib.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdatomic.h>   // lock-free SPSC request queue: main (producer) ↔ audio (consumer) thread

#define SOUND_SAMPLE_RATE  44100
#define SOUND_VOICES       16
#define SOUND_SFX_STEPS    32
#define SOUND_SFX_SLOTS    32
#define SOUND_INSTR_SLOTS  32   // 0-4 = the raw waves; 5-31 cart-defined (rich patch carts like modrack want banks per wave)

// Waveform IDs (INSTR_*) come from studio.h.
// Wave ids below INSTR_ENGINE_BASE are wavetable oscillators (sound_osc); at/above they are
// modeled ENGINES — stateful per-note simulations that read the three macros (harm/timb/mor).
#define INSTR_ENGINE_BASE  INSTR_PLUCK
#define SOUND_KS_MAX       1024   // Karplus-Strong delay line cap (~4KB/voice) — bottoms out around 43Hz / MIDI 29
#define ORGAN_SCAN         64     // INSTR_ORGAN scanner-chorus delay taps (~1.5ms; borrows ks_buf's head — organ never uses the Karplus path)

// One step in an SFX. pitch=0 means silence; vol 0..7.
typedef struct {
    uint8_t pitch;   // MIDI note (0 = silent)
    uint8_t instr;   // waveform id
    uint8_t vol;     // 0..7
} SfxStep;

typedef struct {
    SfxStep steps[SOUND_SFX_STEPS];
    uint8_t step_dur;   // 10ms units (e.g. 6 = 60ms per step)
    uint8_t length;     // 1..32
    uint8_t loop;       // 1 = repeat when SFX ends
} Sfx;

// An instrument = a waveform + an ADSR envelope + pulse duty. `instr` ids in
// note()/hit()/chord()/tone() index this bank. Slots 0..4 are pre-filled with the
// five raw waves (near-instant envelope) so old carts keep working; carts define 5+.
typedef struct {
    int   wave;
    int   a_samp, d_samp, r_samp;   // attack / decay / release, in samples
    float sustain;                  // 0..1
    float duty;                     // 0..1 pulse width (only used by INSTR_SQUARE)
    float eng_p[2];                 // EXPERIMENTAL guitar/piano tuning: 0 = fundamental weight, 1 = attack click
    int   lfo_dest[3];              // LFO_PITCH / LFO_DUTY / LFO_VOLUME / LFO_CUTOFF, per LFO
    float lfo_rate[3];              // Hz
    float lfo_depth[3];             // 0 = off; units depend on dest
    int   flt_mode;                 // FILTER_OFF / LOW / HIGH / BAND / NOTCH
    float flt_cutoff;               // Hz
    float flt_q;                    // damping coefficient (1/Q); small = resonant
    int   env_dest[3];              // ENV_CUTOFF / ENV_PITCH / ENV_DUTY, per mod-envelope
    int   env_a_samp[3];            // attack, in samples
    int   env_d_samp[3];            // decay, in samples
    float env_amount[3];            // 0 = off; bipolar; units depend on dest (Hz / semitones / 0..1)
    int   flw_dest;                 // envelope FOLLOWER dest (LFO_CUTOFF/VOLUME/PITCH) — the 3rd mod source
    float flw_atk, flw_rel;         // per-sample smoothing coeffs (from attack/release ms)
    float flw_amount;               // 0 = off; Hz for cutoff, 0..1 for volume(duck), semitones for pitch
    float harmonics, timbre, morph; // engine macros 0..1 (INSTR_PLUCK+) — meaning is per-engine; default 0.5
    float drive;                    // post-filter saturation 0..1; 0 = clean bypass (default — old carts unchanged)
    int   drive_mode;               // waveshaper flavour DRIVE_SOFT(0)/HARD/FOLD/ASYM; 0 = tanh (default — old carts unchanged)
    float echo;                     // send to THE echo bus 0..1; 0 = dry (default — old carts unchanged)
    float reverb;                   // reverb send 0..1; 0 = dry (default — old carts unchanged). Target = rvb_tank below
    int   rvb_tank;                 // which reverb tank the send feeds: -1 = the master send (default), 1.. = a reverb_bus() send-bus
    int   sc_key;                   // sidechain trigger key this slot feeds (0..3), via sidechain_key()
    float sc_send;                  // how much this slot drives that key 0..1; 0 = not a trigger (default)
    int   fx_bus;                   // insert bus for chorus/flanger: 0 = master (default), 1.. = a private aux bus (instrument_chorus/flanger)
    float pan;                      // stereo position -1 L .. 0 center .. +1 R; default 0 = center (linear law, stereo.md)
    float tune_mul;                 // slot detune as a freq factor (2^(semis/12)); read LIVE by every
                                    // sounding voice each sample — fire-and-forget hits bend too. default 1
    uint32_t choke_mask;            // bitmask: bit N set = a new note on this slot kills active voices on slot N
} Instrument;

#define SOUND_LFOS 3
#define SOUND_ENVS 3   // routable mod-envelopes per instrument (the one-shot twin of the LFOs)

// An RBJ constant-skirt bandpass biquad — the body-resonator formant used by INSTR_GUITAR's
// 4 body modes. Coeffs set by sound_biquad_set(), one sample run by sound_biquad_run().
typedef struct { float b0, b1, b2, a1, a2, z1, z2; } SoundBiquad;

typedef struct {
    bool   active;
    int    sfx_idx;            // -1 if standalone note
    int    step;
    int    step_samples;
    int    step_len_samples;   // for a one-shot note this is the GATE length; release runs after it
    float  phase;
    float  freq;
    float  vol;                // 0..1
    int    wave;
    int    noise_state;
    // ADSR + LFO snapshot (one-shot notes only; copied from the instrument at note-on)
    int    a_samp, d_samp, r_samp;
    float  sustain;
    float  duty;
    float  rel_start;          // envelope level at the moment the gate ends (release fades from here)
    int    lfo_dest[3];
    float  lfo_rate[3], lfo_depth[3], lfo_phase[3];
    int    env_dest[3];              // mod-envelopes (AD; timer = step_samples, retriggered at note-on)
    int    env_a_samp[3], env_d_samp[3];
    float  env_amount[3];
    int    flw_dest;                 // envelope follower: tracks this voice's own amplitude → dest
    float  flw_atk, flw_rel, flw_amount, flw_amp;   // attack/release coeffs, depth, + the running level
    int    flt_mode;
    float  flt_cutoff, flt_q;
    float  flt_low, flt_band;   // SVF running state
    // held-note (note_on) state + per-param slew (the live voice glides toward these targets)
    bool   held;                   // sustained note_on voice — infinite gate until note_off
    int    owner_slot, owner_gen;  // which handle owns this voice (for stale-handle rejection)
    float  freq_target, vol_target, cutoff_target, duty_target, flt_q_target;
    float  freq_slew;              // pitch slew coefficient/sample — note_glide() sets it (default = snappy)
    // engine macros (current + slew target, riding the same machinery as cutoff/duty)
    float  harm, timb, mor;
    float  harm_target, timb_target, mor_target;
    float  drv, drv_target;        // post-filter drive (current + slew target, same machinery)
    int    drv_mode;               // waveshaper flavour (DRIVE_*), copied from the instrument at note-on
    float  drv_dc_x1, drv_dc_y1;   // DC blocker on the drive output (tanh of an asymmetric wave = DC = a thump)
    float  eko, eko_target;        // echo-bus send (current + slew target, same machinery — note_echo)
    float  rvb, rvb_target;        // reverb-bus send (current + slew target, same machinery — note_reverb)
    int    bus;                    // insert bus for chorus/flanger (snapshot of instr fx_bus at note-on); 0 = master
    int    rvb_bus;                // reverb SEND target bus: 0 = the master parallel send (legacy), 1.. = a reverb_bus() send-bus
    int    sc_key;                 // sidechain trigger key (0..3), snapshot of the instrument at note-on
    float  sc_send;                // how much this voice drives that key 0..1 (0 = not a trigger)
    float  pan, pan_target;        // stereo pan (current + slew target, same machinery — note_pan); -1..1, 0 = center
    int    instr_slot;             // instrument slot this voice was started from (for choke matching)
    float  last_outL, last_outR;   // this voice's previous PANNED contribution per channel — feeds the steal-declick tail
    // Karplus-Strong string state (INSTR_PLUCK): a write head + a FRACTIONAL read tap.
    // Pitch = the tap distance, recomputed every sample from freq*pitch_mul — so vibrato,
    // pitch envelopes, note_pitch and note_glide all bend the string live.
    float  ks_buf[SOUND_KS_MAX];
    int    ks_len, ks_widx;        // ks_len = ALLOCATED length (period*2 headroom for down-bends)
    float  ks_last;                // previous read (the damping average runs across it)
    // modal bar state (INSTR_MALLET): four decaying sine modes, buffer-free. Mode frequencies
    // derive from freq*pitch_mul EVERY sample — the whole pitch machinery bends the bar too.
    float  md_phase[4], md_amp[4]; // per-mode phase + decaying amplitude (the ring lives here)
    float  md_trem_ph;             // vibe motor tremolo phase (morph's top end switches the motor on)
    int    md_strike;              // strike-noise samples remaining (the mallet contact click)
    bool   md_on;                  // struck this note — guards an engine id without a note-on init
    // FM state (INSTR_FM): the carrier rides v->phase (advanced by the mix loop like any
    // wave); only the inaudible modulator needs its own phase + the feedback memory.
    // fm_tph is the DX tine ping (1:1 detent only — see sound_fm_sample).
    float  fm_mph, fm_fb, fm_tph;
    // tonewheel organ state (INSTR_ORGAN): 9 additive drawbar sines (buffer-free), plus a
    // key-click burst, a percussion ping, and the scanner chorus — whose short delay line
    // borrows the head of ks_buf (organ never touches the Karplus path), so it adds no buffer.
    float  org_ph[9];              // per-drawbar phase accumulators
    float  org_click;              // key-click noise-burst envelope (decays ~3ms)
    float  org_perc, org_perc_ph;  // percussion ping envelope + its 2nd-harmonic phase
    float  org_scan_ph;            // scanner LFO phase (6.9Hz, gear-locked to the motor)
    float  org_lp;                 // pre-drive lowpass state — driven organ loses its top (amp/Leslie)
    int    org_widx;               // scanner delay write index into ks_buf[0..ORGAN_SCAN)
    bool   org_on;                 // struck this note — guards an engine id without a note-on init
    // electric-piano state (INSTR_EPIANO): 12 decaying inharmonic sine modes through a pickup
    // nonlinearity + DC blocker, buffer-free (mallet family). Struck/self-decaying like mallet.
    float  ep_ph[12], ep_amp[12], ep_dec[12], ep_ratio[12]; // per-mode phase / amp / decay-frac / ratio
    float  ep_dc_prev, ep_dc_state;  // DC blocker taps (the sum^2 nonlinearity injects DC)
    float  ep_freqnorm;              // register position 0..1 (upper modes + bark fade out high up)
    int    ep_click;                 // tangent-click burst: samples remaining (the percussive chink)
    float  ep_click_amp;             // its peak amp (per-type base × strike velocity × hammer hardness)
    int    ep_type;                  // 0 Rhodes / 1 Wurli / 2 Clav (from harmonics at note-on)
    bool   ep_on;                    // struck this note — guards an engine id without a note-on init
    // membrane-drum state (INSTR_MEMBRANE): six decaying sine modes at circular-membrane
    // (Bessel) ratios, buffer-free (mallet family). Struck/self-decaying. The bayan pitch-bend
    // (morph) is derived from step_samples per sample — no extra state, like PD's DCW.
    float  mb_phase[6], mb_amp[6];   // per-mode phase + decaying amplitude (the head's ring)
    int    mb_strike;                // strike-noise samples remaining (the finger/slap contact)
    bool   mb_on;                    // struck this note — guards an engine id without a note-on init
    // reed-waveguide state (INSTR_REED): a bore delay line (REUSES ks_buf — reed is a distinct
    // wave id, never shares a voice with the Karplus pluck path) + a pressure-driven reed valve.
    // The first SELF-OSCILLATING held voice; every macro clamps to navkit's oscillation window
    // (it chokes/dies outside it — STEP-0, instrument-engines.md §8.8.7). Re-seeded on note-on.
    float  rd_lp;                    // bore-loss 1-pole LP state (bell radiation + wall damping)
    float  rd_dc_prev, rd_dc_state;  // DC blocker (steady blow pressure → large DC — essential)
    float  rd_vib_ph;                // lip-vibrato LFO phase
    float  rd_noise_lp;              // breath-turbulence noise LP (the "air" in the tone)
    float  rd_drift_ph;              // slow LFO phase — humanizes vibrato rate/depth (not a clean LFO)
    float  rd_drift;                 // slow breath-pressure random walk (kills the dead-flat steady state)
    int    rd_attack;                // attack-chiff samples remaining (the noisy tonguing onset)
    float  rd_tilt;                  // output-tilt LP (timbre brightness, built here — STEP-0)
    float  rd_initfreq;              // freq at note-on (bore sized for it; pitch tracking glides off it)
    int    rd_len;                   // bore delay length (half-wavelength); 0 = no note-on init
    int    rd_idx;                   // bore write index into ks_buf
    bool   rd_on;                    // note-on init guard (engine id hit without a strike → silent)
    // pipe/flute state (INSTR_PIPE): STK jet-drive flute — a bore delay (REUSES ks_buf, distinct
    // wave id from the Karplus/reed paths) + a short jet delay + a nonlinear jet deflection.
    // Held/self-oscillating; reuses reed's realism (breath turbulence, humanized vibrato, chiff). §8.8.8.
    float  pp_jet[64];               // jet delay line (lip→labium travel time)
    int    pp_jet_idx;               // jet write index
    int    pp_idx;                   // bore write index into ks_buf
    int    pp_len;                   // bore delay length; 0 = no note-on init
    float  pp_initfreq;              // freq at note-on (pitch tracking glides off it)
    float  pp_lp;                    // open-end reflection LP state (radiation impedance)
    float  pp_dc_prev, pp_dc_state;  // DC blocker
    float  pp_vib_ph, pp_drift_ph, pp_drift, pp_noise_lp;  // humanized vibrato + breath turbulence/drift
    int    pp_attack;                // attack-chiff samples remaining (the tongued "tu" onset)
    bool   pp_on;                    // note-on init guard
    // formant-voice state (INSTR_VOICE): navkit VoicForm port — a glottal pulse through four SVF
    // formants with a continuous vowel morph. EXPERIMENTAL: the raw params live FLAT in vox_p[]
    // (poked by note_aux() — the voxlab fat prototype) instead of riding the 3 macro knobs,
    // so we can audition which three deserve to become harmonics/timbre/morph. vowels-only for now.
    float  vox_p[20];                // raw param TARGETS 0..1 (note_aux idx, must hold VOX_NPARAM): vowel/size/breath/openQ/tilt/vibDepth/vibRate/nasality/openness/frontness + EXPERIMENTAL 10 buzz·11 jitter·12 shimmer·13 creak·14 nasalAF·15 reduce·16 measBW
    float  vox_s[20];                // slewed copies (per-sample one-pole → no zipper on slider moves)
    float  vox_glot_ph;              // glottal pulse phase
    float  vox_tilt_lp;              // spectral-tilt 1-pole state
    float  vox_vib_ph;               // vibrato LFO phase
    float  vox_jit_mul;              // per-period pitch-jitter multiplier (1.0 = none); redrawn each glottal cycle
    float  vox_shim_mul;             // per-period amplitude-shimmer multiplier (1.0 = none)
    int    vox_creak_skip;           // diplophonia/creak: attenuate this glottal period (vocal fry)
    float  vox_naf_low, vox_naf_band; // anti-formant nasality notch SVF state (navkit WAVE_VOICE model)
    float  vox_f_low[4], vox_f_band[4]; // 4 formant SVF states (low, band) — high recomputed per sample
    int    vox_cons;                 // consonant ONSET id (VC_*), -1 = none; set by voice_consonant()
    float  vox_cons_t;               // seconds since the consonant onset began (counts up to its duration)
    int    vox_coda;                 // consonant CODA id (VC_*), -1 = none; set by voice_coda() near release
    float  vox_coda_t;               // seconds since the coda morph began (vowel → consonant at note end)
    bool   vox_on;                   // note-on init guard (engine id hit without a start → silent)
    // guitar state (INSTR_GUITAR): the bodied plucked string — PLUCK's Karplus-Strong string
    // (REUSES ks_buf, distinct wave id from the bare pluck path) + 4 parallel body-resonator
    // bandpass formants + an output DC blocker. macros: harmonics = body (open/clear → resonant/
    // boxy — sets the formant voicing + dry/wet mix at note-on), timbre = string brightness (the
    // excitation lowpass), morph = mute (per-sample feedback: long open ring → tight pizzicato).
    // Buzz/jawari (sitar) deferred to a future preset. Design: instrument-engines.md §8.8.9.
    SoundBiquad gt_body[4];          // body formants (parallel bandpass), voiced from harmonics
    float  gt_bodymix;               // dry string ↔ wet body blend (from harmonics)
    float  gt_dc_prev, gt_dc_state;  // output DC blocker (long-sustain bodies build DC)
    bool   gt_on;                    // note-on init guard (engine id hit without a pluck → silent)
    // piano state (INSTR_PIANO): StifKarp stiff string — our KS string (REUSES ks_buf) + a
    // dispersion allpass chain (the inharmonic "stretched partial" piano shimmer, the engine's
    // defining feature) + a baked grand-piano soundboard (4 body biquads) + an output DC blocker.
    // macros: harmonics = stiffness (dispersion depth + stages), timbre = hammer (excitation
    // lowpass: soft felt → hard), morph = pedal (decay length + high-freq retention: damped
    // staccato → long open sustain). Double-string detune + prepared buzz deferred. §8.8.9.
    float  pn_disp_c[4], pn_disp_s[4];  // dispersion allpass coeffs (set from stiffness) + states
    int    pn_disp_n;                   // active dispersion stages (1..4, by stiffness)
    SoundBiquad pn_body[4];             // per-voicing soundboard formants (set at note-on)
    float  pn_bodymix;
    float  pn_loop_lp, pn_loopco;       // output tone-filter (1-pole LP) state + coeff (per voicing)
    float  pn_symp;                     // sympathetic-resonance level (per voicing)
    float  pn_detune;                   // 2nd-string detune ratio (per voicing; 1.0 = no 2nd string)
    float  pn_dc_prev, pn_dc_state;     // output DC blocker
    // navkit-verbatim KS string: near-lossless loop (decay comes from the AMP ENVELOPE, not the
    // loop — that's what keeps the upper harmonics alive), one-period buffer + fractional-delay
    // allpass tuning, per-voicing brightness/damping.
    float  pn_ksb, pn_ksd;              // ksBrightness (high-harmonic retention) + ksDamping (≈0.999)
    float  pn_ksb_cur;                  // brightness ENVELOPE: strikes bright, settles to pn_ksb (the piano "bloom")
    float  pn_apc, pn_aps;              // fractional-delay allpass coeff + state (pitch tuning)
    float  pn_initf;                    // freq at note-on (pitch-tracking reference)
    float  pn_dampg, pn_damps;          // damper gain (pedal) + slewed state
    float  pn_ks2[SOUND_KS_MAX];        // detuned 2nd string delay line (own loop)
    int    pn_ks2_widx, pn_ks2_len;
    float  pn_ks2_last, pn_ks2_apc, pn_ks2_aps;
    float  pn_ks2_disp[4];              // 2nd string dispersion states (reuses pn_disp_c coeffs)
    bool   pn_on;                       // note-on init guard
    // bowed state (INSTR_BOWED): navkit's Smith/McIntyre bowed-string waveguide (line-for-line port).
    // Two delay lines meet at the bow contact point — PACKED into ks_buf (distinct wave id, never
    // shares a voice with the Karplus/reed/pipe paths): nut half = ks_buf[0..bw_nutlen), bridge half
    // = ks_buf[bw_nutlen..bw_nutlen+bw_brlen). A hyperbolic stick-slip friction re-excites the
    // string → self-oscillation; held like reed/pipe. STEP-0 (navkit-bowsweep.c) found the Helmholtz
    // wedge — the macros are pinned inside it: harmonics = bow position β (note-on split), timbre =
    // bow pressure (0.15–0.42, the narrow axis), morph = bow velocity/swell (wider wedge as it grows).
    int    bw_nutlen, bw_brlen;         // delay-line split lengths (sum = full wavelength, ≤ KS_MAX-1)
    int    bw_nutidx, bw_bridx;         // write indices within each half (bridge offset by bw_nutlen)
    float  bw_nutrefl, bw_brrefl;       // reflection-filter states (nut end / bridge end)
    float  bw_initfreq;                 // freq at note-on (bore sized for it; pitch tracking glides off it)
    float  bw_vib_ph, bw_drift_ph, bw_drift, bw_noise_lp;  // humanized pitch-vibrato + bow-noise (rosin) + drift
    int    bw_attack;                   // bow-bite onset samples (the catch at note start)
    float  bw_dc_prev, bw_dc_state;     // output DC blocker (steady bow drives a large DC)
    bool   bw_on;                       // note-on init guard
    bool   bw_pizz;                     // PIZZICATO: seed the string with a pluck + bypass the bow
                                        // friction, so the SAME waveguide (string + body) rings down
                                        // instead of self-oscillating. Set from eng_p[0] >= 0.5 at note-on.
    // fundamental reinforcement (guitar + piano): a sub-oscillator at the note's pitch, envelope-
    // following the string, mixed under it — adds the low-end WEIGHT a bare KS string lacks (the
    // "thin" cure). Plus an onset noise CLICK (pick/hammer transient). Both amounts come from
    // eng_p[] (set via instrument_mode, note-on — the permanent per-engine aux channel, decision 0017; NOT baked to constants).
    float  eng_p[2];                    // copied from the instrument at note-on (0 weight · 1 attack)
    float  eng_subph, eng_env;          // sub-osc phase + envelope follower
    int    eng_click;                   // onset-click samples remaining
    // brass state (INSTR_BRASS): STK BrassInstrument (Cook/Scavone) — a bore delay (REUSES ks_buf,
    // distinct wave id, never shares a voice with the reed/pipe/bowed/Karplus paths) closed by an
    // inverting bell reflection, driven by a LIP VALVE modeled as a 2-pole resonant biquad (the
    // mass-spring lip; its resonance tracks just ABOVE the played pitch → lip-slur / harmonic lock).
    // The defining timbre is pressure-driven BRASSINESS: high blow + high timbre steepens the
    // circulating wave (a shockwave) → the overtones explode (quiet=round, loud=blatty). Held/
    // self-oscillating like reed; the amp ADSR gates it. STEP-0 + macros: §8.8.10.
    int    br_len;                      // bore delay length (half-wavelength one-way); 0 = no note-on init
    int    br_idx;                      // bore write index into ks_buf
    float  br_initfreq;                 // freq at note-on (bore sized for it; pitch tracking glides off it)
    float  br_lip_y1, br_lip_y2;        // lip biquad output history (the normalized resonant bandpass)
    float  br_lip_x1, br_lip_x2;        // lip biquad input history (b2 = -b0 bandpass needs x[n-2])
    float  br_lp;                       // bell-radiation 1-pole LP state
    float  br_dc_prev, br_dc_state;     // bore-return DC blocker (steady blow → large DC)
    float  br_out_prev, br_out_state;   // OUTPUT DC blocker (the asymmetric brassiness shaper injects DC)
    float  br_vib_ph, br_drift_ph, br_drift, br_noise_lp;  // humanized lip-vibrato + breath turbulence/drift
    int    br_attack;                   // speak-transient samples remaining (the "tah" onset)
    bool   br_on;                       // note-on init guard (engine id hit without a note-on → silent)
} Voice;

static Voice         voices[SOUND_VOICES];

// held-note handles. A handle packs slot (low SOUND_HANDLE_BITS) + generation
// (rest). The main thread owns hn_gen/hn_used (hands out handles); the audio
// thread owns held_voice (maps a handle slot → the voice currently serving it,
// or -1). A setter is honored only if the live voice still carries the handle's
// generation — so a stale handle (its voice was stolen or the slot reused)
// silently no-ops instead of hitting the wrong note. See docs/design/held-notes.md.
#define SOUND_HANDLE_BITS 4                      // slot field width — must hold SOUND_VOICES-1
#define SOUND_HANDLE_MASK ((1 << SOUND_HANDLE_BITS) - 1)
_Static_assert(SOUND_VOICES <= (1 << SOUND_HANDLE_BITS),
               "SOUND_VOICES no longer fits the handle slot field - bump SOUND_HANDLE_BITS");
static int           hn_gen[SOUND_VOICES];      // main thread: generation per handle slot
static bool          hn_used[SOUND_VOICES];     // main thread: slot handed out (between note_on/note_off)
static int           held_voice[SOUND_VOICES];  // audio thread: handle slot → voice index, or -1
#define SOUND_HELD_GATE 0x7FFFFFFF               // "infinite" gate length for a sustained note
static AudioStream   sound_stream;

// ── WAV capture (audio debugging — see docs/design/audio-notes.md §16) ────
// One tap point: the final `mix` float, captured right before it goes to the
// device. Two users: the .bake/wav_request trigger file (live capture — the
// audio thread fills the buffer, the main thread starts/finishes), and the
// --wav harness flag (synth mode: no audio device stream, the main thread
// pumps sound_callback synchronously per frame, byte-reproducible under --det).
static bool          sound_synth_mode = false;   // --wav: skip the device stream; main thread pumps
static float        *wavcap_buf   = NULL;        // capture buffer (mallocd by wavcap_begin)
static int           wavcap_total = 0;           // samples wanted
static volatile int  wavcap_pos   = 0;           // audio thread write cursor
static volatile int  wavcap_state = 0;           // 0 idle · 1 recording · 2 done (ready to write)
static char          wavcap_path[512];

// write a 16-bit PCM mono 44.1kHz WAV
static int sound_wav_write(const char *path, const float *buf, int n) {
    FILE *w = fopen(path, "wb");
    if (!w) return -1;
    int data_bytes = n * 2, riff = 36 + data_bytes;
    int sr = SOUND_SAMPLE_RATE, byterate = sr * 2;
    short block = 2, bits = 16, fmt = 1, ch = 1;
    int fmtlen = 16;
    fwrite("RIFF", 1, 4, w); fwrite(&riff, 4, 1, w); fwrite("WAVE", 1, 4, w);
    fwrite("fmt ", 1, 4, w); fwrite(&fmtlen, 4, 1, w);
    fwrite(&fmt, 2, 1, w); fwrite(&ch, 2, 1, w);
    fwrite(&sr, 4, 1, w); fwrite(&byterate, 4, 1, w);
    fwrite(&block, 2, 1, w); fwrite(&bits, 2, 1, w);
    fwrite("data", 1, 4, w); fwrite(&data_bytes, 4, 1, w);
    for (int i = 0; i < n; i++) {
        float s = buf[i];
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        short q = (short)(s * 32767.0f);
        fwrite(&q, 2, 1, w);
    }
    fclose(w);
    return 0;
}

// main thread: arm a live capture (audio thread starts filling on its next callback)
static void sound_wavcap_begin(const char *path, float seconds) {
    if (wavcap_state != 0) return;
    int n = (int)(seconds * SOUND_SAMPLE_RATE);
    if (n <= 0) return;
    free(wavcap_buf);
    wavcap_buf = (float *)malloc((size_t)n * sizeof(float));
    if (!wavcap_buf) return;
    snprintf(wavcap_path, sizeof wavcap_path, "%s", path);
    wavcap_total = n;
    wavcap_pos   = 0;
    wavcap_state = 1;
}

// main thread: if the audio thread finished, write the file and go idle
static void sound_wavcap_poll(void) {
    if (wavcap_state != 2) return;
    sound_wav_write(wavcap_path, wavcap_buf, wavcap_total);
    free(wavcap_buf);
    wavcap_buf   = NULL;
    wavcap_state = 0;
}
static Sfx           sfx_bank[SOUND_SFX_SLOTS];
static Instrument    instr_bank[SOUND_INSTR_SLOTS];
#define SOUND_USER_WAVES 4
#define SOUND_WAVE_LEN   64
static float         user_wave[SOUND_USER_WAVES][SOUND_WAVE_LEN];   // INSTR_USER0..3 single-cycle tables (filled via wave_set)

// ── THE echo bus (audio-notes §17 step 3, decisions/0015) ─────────────────
// ONE shared delay line — a bus with per-slot sends, not a per-voice effect
// (a 2s line is ~345 KB; nobody wants 16 private ones). Audio-thread-owned;
// params arrive via the request ring like everything else. Dormant until the
// first echo()/instrument_echo()/note_echo() call ever arrives (echo_used),
// so old carts pay nothing and stay bytes-identical.
//   • tone = a one-pole lowpass INSIDE the feedback loop → repeats get darker
//     each pass ("dark is a space property" — the thing scheduled-note echo
//     fundamentally can't do)
//   • feedback up to 1.1: a tanh soft-clip inside the loop turns >1.0 into a
//     controlled tape-style self-oscillation instead of an explosion
//   • the read tap is FRACTIONAL and the delay time slews toward its target
//     with a clamped per-sample step — sweeping echo() time live pitch-bends
//     the ringing tail exactly like varying tape speed (the RE-201 move)
#define SOUND_ECHO_MAX (SOUND_SAMPLE_RATE * 2)   // 2s cap on the delay line
static float echo_buf[SOUND_ECHO_MAX];
static int   echo_widx        = 0;
static float echo_time        = 0.375f * SOUND_SAMPLE_RATE;   // read-tap distance, samples (fractional)
static float echo_time_target = 0.375f * SOUND_SAMPLE_RATE;   // default 375ms = dotted-8th at 120bpm
static float echo_fb          = 0.35f;
static float echo_tone_coef   = 0.0f;            // one-pole LP coefficient (set from tone; default in sound_init)
static float echo_lp          = 0.0f;            // the loop filter's running state
static bool  echo_used        = false;           // flips true on first echo API call, never back

// ── delay INSERT (echo_insert) — an IN-LINE dry/wet delay, an honest reorderable pedal ──────────
// The echo above is a parallel SEND; this is the same tape-delay DSP placed IN the master fx_order
// chain, so its POSITION matters (delay→drive vs drive→delay). Master-only (one buffer), gated on
// b==0 in apply_insert. Its OWN buffer so it never collides with the send. Wet ADDS over the full
// dry (a delay pedal's MIX = repeat level), scaled by mix. Dormant until echo_insert() → byte-identical.
static float echo_ins_buf[SOUND_ECHO_MAX];
static int   echo_ins_widx        = 0;
static float echo_ins_time        = 0.375f * SOUND_SAMPLE_RATE;
static float echo_ins_time_target = 0.375f * SOUND_SAMPLE_RATE;
static float echo_ins_fb          = 0.35f;
static float echo_ins_tone_coef   = 0.0f;
static float echo_ins_lp          = 0.0f;
static float echo_ins_mix         = 0.0f;
static bool  echo_ins_used        = false;
static void echo_ins_process(float *mixL, float *mixR) {
    float dstep = (echo_ins_time_target - echo_ins_time) * 0.0003f;   // tape-speed time slew (RE-201 bend)
    if (dstep >  0.5f) dstep =  0.5f;
    if (dstep < -0.5f) dstep = -0.5f;
    echo_ins_time += dstep;
    float rp = (float)echo_ins_widx - echo_ins_time;
    if (rp < 0.0f) rp += (float)SOUND_ECHO_MAX;
    int   r0 = (int)rp, r1 = r0 + 1;
    if (r1 >= SOUND_ECHO_MAX) r1 = 0;
    float fr  = rp - (float)r0;
    float tap = echo_ins_buf[r0] + (echo_ins_buf[r1] - echo_ins_buf[r0]) * fr;
    echo_ins_lp += (tap - echo_ins_lp) * echo_ins_tone_coef;          // darker each repeat
    float in = (*mixL + *mixR) * 0.5f;                                // MONO core (v1), like the send + reverb insert
    echo_ins_buf[echo_ins_widx] = tanhf(in + echo_ins_lp * echo_ins_fb);   // fb >1 self-oscillates into a plateau, not a blowup
    if (++echo_ins_widx >= SOUND_ECHO_MAX) echo_ins_widx = 0;
    *mixL += echo_ins_lp * echo_ins_mix;                             // wet repeats sit on top of the full dry
    *mixR += echo_ins_lp * echo_ins_mix;
}

// master pan law (stereo.md): PAN_LINEAR (default, center=mix, byte-identical to mono) or
// PAN_POWER (constant-power, center=-3dB, equal loudness across the sweep). gated so the
// default never regresses centered carts; only a panning cart that opts in changes output.
static int   g_pan_law        = PAN_LINEAR;

// tone 0..1 → loop-filter cutoff 300 Hz .. ~6.8 kHz (each repeat passes through it once)
static float sound_echo_coef(float t) {
    if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
    float fc = 300.0f * powf(2.0f, t * 4.5f);
    return 1.0f - expf(-6.2831853f * fc / (float)SOUND_SAMPLE_RATE);
}

// ── THE reverb bus (audio-notes §17 / instrument-engines §8.10 — the first §8.10 effect) ──
// A SEND bus, exactly like the echo bus above: ONE shared reverberator with per-slot sends, not a
// per-voice effect (reverb is bus-only by design — decision 0015 / the placement sanity-check in
// sound-next-steps.md). The DSP is a line-for-line port of navkit's classic Schroeder core (4
// parallel comb filters + 2 series allpass + a pre-delay), the proven house pattern. Mono in v1
// (stereo width is later §8.10, like the echo bus). Dormant until the first reverb API call ever
// arrives (reverb_used), so old carts pay nothing and stay bytes-identical.
#define REVERB_COMB_1 1559         // comb delay times in samples @44100 Hz, mutually prime (navkit)
#define REVERB_COMB_2 1621
#define REVERB_COMB_3 1493
#define REVERB_COMB_4 1427
#define REVERB_ALLPASS_1 223
#define REVERB_ALLPASS_2 557
#define REVERB_PREDELAY  882       // fixed 20ms pre-delay (SOUND_SAMPLE_RATE/50)
#define REVERB_FEEDBACK_MIN   0.7f   // comb feedback at size 0 (short decay)
#define REVERB_FEEDBACK_RANGE 0.25f  // + size → longer decay (max 0.95)
#define REVERB_DAMP_SCALE     0.4f   // damping → comb-LP cutoff scale

// THE reverb tank pool (Increment C — effects-bus-architecture.md §5). Was a single shared
// reverberator; now SOUND_REVERB_TANKS independent tanks of identical DSP. Tank 0 = the legacy
// reverb() master SEND (routing unchanged → bytes-identical for old carts). Tanks 1..N-1 are
// reverb SEND-BUSES: reverb_bus() routes a slot's send onto a dedicated aux bus whose insert
// chain starts with FX_REVERB, so a cart can run effects AFTER the space (reverb→bitcrush). One
// tank ≈ 24KB of comb/allpass buffers; the whole struct is zero-init .bss (0 bytes of wasm).
#define SOUND_REVERB_TANKS 3
typedef struct {
    float comb1[REVERB_COMB_1], comb2[REVERB_COMB_2], comb3[REVERB_COMB_3], comb4[REVERB_COMB_4];
    float ap1[REVERB_ALLPASS_1], ap2[REVERB_ALLPASS_2];
    float predelay[REVERB_PREDELAY];
    int   cp1, cp2, cp3, cp4;        // comb write positions
    int   ap_p1, ap_p2;              // allpass positions
    int   pd_p;                      // predelay write position
    float clp1, clp2, clp3, clp4;    // per-comb damping LP states
    float fb, damp;                  // config from size/damping — set by reverb() (tank 0) / reverb_bus()
    float mix;                       // dry/wet blend at the insert: 1 = wet-replace (a dedicated send-bus), <1 = in-line (reverb_insert)
    bool  used;                      // per-tank dormancy: a dormant tank is skipped (costs zero)
} ReverbTank;
static ReverbTank rvb_tank[SOUND_REVERB_TANKS];
static bool  reverb_used = false;          // flips true on first reverb() call (tank 0 master send), never back

// one comb filter with lowpass damping (navkit processCombFilter — darker tails as damping rises)
static float rvb_comb(float input, float *buf, int *pos, int size, float *lp, float fb, float damp) {
    float output = buf[*pos];
    float dampCoef = 1.0f - damp * REVERB_DAMP_SCALE;
    *lp = output * dampCoef + *lp * (1.0f - dampCoef);
    buf[*pos] = input + *lp * fb;
    *pos = (*pos + 1) % size;
    return output;
}
// one Schroeder allpass (navkit processAllpass): H(z) = (-g + z^-N)/(1 - g·z^-N), feedback from output
static float rvb_allpass(float input, float *buf, int *pos, int size, float coef) {
    float delayed = buf[*pos];
    float output  = delayed - coef * input;
    buf[*pos] = input + coef * output;
    *pos = (*pos + 1) % size;
    return output;
}
// process one sample on tank t, returns the WET signal only (navkit _processReverbCore).
// Identical math/summation order to the old single-tank version → tank 0 is bytes-identical.
static float reverb_process(ReverbTank *t, float sample) {
    float pre = t->predelay[t->pd_p];                 // exactly REVERB_PREDELAY samples old
    t->predelay[t->pd_p] = sample;
    t->pd_p = (t->pd_p + 1) % REVERB_PREDELAY;
    // 4 parallel comb filters build a dense echo pattern
    float c1 = rvb_comb(pre, t->comb1, &t->cp1, REVERB_COMB_1, &t->clp1, t->fb, t->damp);
    float c2 = rvb_comb(pre, t->comb2, &t->cp2, REVERB_COMB_2, &t->clp2, t->fb, t->damp);
    float c3 = rvb_comb(pre, t->comb3, &t->cp3, REVERB_COMB_3, &t->clp3, t->fb, t->damp);
    float c4 = rvb_comb(pre, t->comb4, &t->cp4, REVERB_COMB_4, &t->clp4, t->fb, t->damp);
    float sum = (c1 + c2 + c3 + c4) * 0.25f;
    // 2 series allpass filters diffuse + smooth the tail
    float a1 = rvb_allpass(sum, t->ap1, &t->ap_p1, REVERB_ALLPASS_1, 0.5f);
    return rvb_allpass(a1, t->ap2, &t->ap_p2, REVERB_ALLPASS_2, 0.5f);
}

// ── THE shared modulated-delay buffer — chorus (the first use) ─────────────
// instrument-engines §8.10 / decision 0015: 0015 reserves ONE master "wow/flutter buffer" as the
// home where a real modulated-delay chorus + flanger + tape-wow all live — NOT three separate
// effects. This is that buffer; **chorus is its first use** (a MASTER-STAGE insert, not a third
// send bus, so 0015's two-send-bus cap holds). Flanger = this same buffer + feedback + a shorter
// delay; tape wow/flutter = this buffer + a slow LFO — both reuse it later.
// DSP = a line-for-line port of navkit's BBD chorus (the Juno-6 hardware model): one rounded-
// triangle LFO, ANTIPHASE taps (tap1 = +mod → L, tap2 = −mod → R: a mono mix fans out to a wide
// stereo chorus, the Juno's "two amps"), charge-well saturation for analog warmth. Dormant until
// the first chorus() call (chorus_used), so old carts pay nothing and stay bytes-identical.
// PER-BUS now (the §8.10 aux step): there are SOUND_FX_BUSES insert buses. Bus 0 = MASTER (the
// whole-mix insert, what chorus()/flanger() configure — unchanged). Buses 1.. = per-instrument:
// instrument_chorus/instrument_flanger(slot,…) hand a slot a private bus so you can flange one
// instrument and leave the rest dry. Each bus owns its own chorus + flanger state (its own comb
// buffer/LFO/feedback). Sends (echo/reverb) stay ONE shared tank — only inserts go per-bus.
#define SOUND_FX_BUSES   8         // bus 0 master + 7 aux (per-instrument). ~82KB of buffers total
#define CHORUS_BUF_LEN   2048      // ~46ms @44100 Hz
#define CHORUS_MIN_DELAY 0.005f    // 5ms
#define CHORUS_MAX_DELAY 0.030f    // 30ms
#define CHORUS_BASE_DELAY 0.0175f  // (MIN+MAX)/2 — base ± mod stays in range at depth 1
#define BBD_CHARGE_SAT   0.7f      // charge-well saturation threshold (warm even harmonics)
static int   fx_next_bus = 1;                  // next free aux bus to hand to instrument_chorus/flanger
// Increment C — the reverb send-bus indirection (effects-bus-architecture.md §C.1). A reverb-bus
// is an aux bus fed only by sends, with FX_REVERB as insert slot 0. bus_tank maps that bus to its
// reverb tank (the expensive 24KB pool above); tank_bus is the inverse, resolved at note-on so
// reverb_bus()/instrument_reverb_bus() can be called in either order.
static int8_t bus_tank[SOUND_FX_BUSES];        // bus → reverb tank index, or -1 = not a reverb-bus (default -1)
static int    tank_bus[SOUND_REVERB_TANKS];    // reverb tank → its dedicated aux bus, or 0 = unallocated
static int    rvb_bus_overflow = 0;            // count of reverb_bus() requests dropped (aux-bus pool exhausted)
static float cho_buf[SOUND_FX_BUSES][CHORUS_BUF_LEN];
static int   cho_widx [SOUND_FX_BUSES];        // per-bus write index
static float cho_phase[SOUND_FX_BUSES];        // per-bus LFO phase 0..1
static float cho_rate [SOUND_FX_BUSES];        // LFO Hz (0.1..5)
static float cho_depth[SOUND_FX_BUSES];        // sweep amount 0..1
static float cho_mix  [SOUND_FX_BUSES];        // dry/wet 0..1
static bool  cho_used [SOUND_FX_BUSES];        // per-bus: flips true when that bus's chorus is configured

// 4-point Hermite interpolation read from ANY mod-delay buffer (navkit hermiteInterp) — smoother
// than linear, so a swept fractional tap doesn't alias. THE shared read of the modulated-delay
// technique: chorus and flanger (and later tape) each instantiate their own buffer but read it
// through this one helper — "write the technique once."
static float moddel_hermite(const float *buf, int len, float readPos) {
    int pos0 = (int)readPos;
    float frac = readPos - (float)pos0;
    int pm1 = (pos0 - 1 + len) % len;
    int p0  =  pos0 % len;
    int p1  = (pos0 + 1) % len;
    int p2  = (pos0 + 2) % len;
    float xm1 = buf[pm1], x0 = buf[p0], x1 = buf[p1], x2 = buf[p2];
    float c0 = x0;
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}
// BBD charge-well saturation (navkit bbdSaturate): soft-clips above ±0.7, adding warm even harmonics
static float cho_bbdsat(float x) {
    if (x >  BBD_CHARGE_SAT) { float e = x - BBD_CHARGE_SAT;  return  BBD_CHARGE_SAT + e / (1.0f + e * 3.0f); }
    if (x < -BBD_CHARGE_SAT) { float e = -x - BBD_CHARGE_SAT; return -(BBD_CHARGE_SAT + e / (1.0f + e * 3.0f)); }
    return x;
}
// process one stereo sample on bus b IN PLACE: mono sum → mod-delay → antiphase L/R wet, dry preserved
static void chorus_process(int b, float *mixL, float *mixR) {
    float *buf = cho_buf[b];
    float mono = (*mixL + *mixR) * 0.5f;
    buf[cho_widx[b]] = mono;
    cho_widx[b] = (cho_widx[b] + 1) % CHORUS_BUF_LEN;
    // rounded-triangle LFO (cubic soft-clip at the peaks — models the BBD capacitor rounding)
    cho_phase[b] += cho_rate[b] / (float)SOUND_SAMPLE_RATE;
    if (cho_phase[b] >= 1.0f) cho_phase[b] -= 1.0f;
    float tri = cho_phase[b] < 0.5f ? cho_phase[b] * 4.0f - 1.0f : 3.0f - cho_phase[b] * 4.0f;
    tri = tri * (1.5f - 0.5f * tri * tri);
    float modAmount = cho_depth[b] * (CHORUS_MAX_DELAY - CHORUS_MIN_DELAY) * 0.5f;
    float d1 = CHORUS_BASE_DELAY + tri * modAmount;   // antiphase taps
    float d2 = CHORUS_BASE_DELAY - tri * modAmount;
    if (d1 < CHORUS_MIN_DELAY) d1 = CHORUS_MIN_DELAY; if (d1 > CHORUS_MAX_DELAY) d1 = CHORUS_MAX_DELAY;
    if (d2 < CHORUS_MIN_DELAY) d2 = CHORUS_MIN_DELAY; if (d2 > CHORUS_MAX_DELAY) d2 = CHORUS_MAX_DELAY;
    float rp1 = (float)cho_widx[b] - d1 * (float)SOUND_SAMPLE_RATE; if (rp1 < 0.0f) rp1 += CHORUS_BUF_LEN;
    float rp2 = (float)cho_widx[b] - d2 * (float)SOUND_SAMPLE_RATE; if (rp2 < 0.0f) rp2 += CHORUS_BUF_LEN;
    float wet1 = cho_bbdsat(moddel_hermite(buf, CHORUS_BUF_LEN, rp1));   // +mod tap → left
    float wet2 = cho_bbdsat(moddel_hermite(buf, CHORUS_BUF_LEN, rp2));   // −mod tap → right
    *mixL = *mixL * (1.0f - cho_mix[b]) + wet1 * cho_mix[b];
    *mixR = *mixR * (1.0f - cho_mix[b]) + wet2 * cho_mix[b];
}

// ── flanger — the SECOND use of the modulated-delay technique (§8.10 / 0015's "one true gap") ──
// 0015 deferred flanger until the master modulated-delay buffer existed ("if it ever lands for
// tape, flanger falls out of it free"); chorus landed that technique, so flanger is now a second
// instance of it — its own short buffer + the shared moddel_hermite read. A short (0.1–10ms)
// triangle-LFO-swept delay with FEEDBACK: feedback is the flanger's identity (subtle → jet whoosh
// → metallic resonance), negative = through-zero. Mono (the classic flange is mono — no antiphase
// trick). MASTER INSERT, master-wide, dormant until the first flanger() call.
#define FLANGER_BUF_LEN   512        // ~11ms @44100 Hz
#define FLANGER_MIN_DELAY 0.0001f    // 0.1ms
#define FLANGER_MAX_DELAY 0.010f     // 10ms
static float flg_buf[SOUND_FX_BUSES][FLANGER_BUF_LEN];
static int   flg_widx [SOUND_FX_BUSES];
static float flg_phase[SOUND_FX_BUSES];
static float flg_rate [SOUND_FX_BUSES];        // LFO Hz (0.05..5)
static float flg_depth[SOUND_FX_BUSES];        // sweep amount 0..1
static float flg_fb   [SOUND_FX_BUSES];        // feedback −0.95..0.95 (the jet/metallic knob)
static float flg_mix  [SOUND_FX_BUSES];        // dry/wet 0..1
static bool  flg_used [SOUND_FX_BUSES];        // per-bus: flips true when that bus's flanger is configured

// process one stereo sample on bus b IN PLACE: mono sum → swept short delay + feedback → wet, dry kept
static void flanger_process(int b, float *mixL, float *mixR) {
    float *buf = flg_buf[b];
    float mono = (*mixL + *mixR) * 0.5f;
    // triangle LFO 0→1→0
    flg_phase[b] += flg_rate[b] / (float)SOUND_SAMPLE_RATE;
    if (flg_phase[b] >= 1.0f) flg_phase[b] -= 1.0f;
    float lfo = flg_phase[b] < 0.5f ? flg_phase[b] * 2.0f : 2.0f - flg_phase[b] * 2.0f;
    float delaySec = FLANGER_MIN_DELAY + lfo * flg_depth[b] * (FLANGER_MAX_DELAY - FLANGER_MIN_DELAY);
    float ds = delaySec * (float)SOUND_SAMPLE_RATE;
    if (ds < 1.0f) ds = 1.0f; if (ds > FLANGER_BUF_LEN - 1) ds = FLANGER_BUF_LEN - 1;
    float rp = (float)flg_widx[b] - ds; if (rp < 0.0f) rp += FLANGER_BUF_LEN;
    float wet = moddel_hermite(buf, FLANGER_BUF_LEN, rp);
    float fb = mono + wet * flg_fb[b];         // feedback into the line = the resonant comb
    if (fb >  1.5f) fb =  1.5f;                // navkit's runaway guard
    if (fb < -1.5f) fb = -1.5f;
    buf[flg_widx[b]] = fb;
    flg_widx[b] = (flg_widx[b] + 1) % FLANGER_BUF_LEN;
    *mixL = *mixL * (1.0f - flg_mix[b]) + wet * flg_mix[b];   // mono wet, both channels
    *mixR = *mixR * (1.0f - flg_mix[b]) + wet * flg_mix[b];
}

// ── tape — the THIRD use of the modulated-delay technique (wow / flutter / saturation) ──
// 0015 reserved a master "wow/flutter buffer" for tape; here it is. A per-bus INSERT, STEREO (it's
// a master glue/character effect — must not mono-collapse a panned mix): warm saturation + a slow
// WOW + fast FLUTTER pitch warble (a modulated-delay read; ONE shared transport LFO so both
// channels' pitch drifts together) + a baked HF rolloff (tape loses highs, darker as you saturate).
// Plain-sine LFOs → fully deterministic (--det safe); navkit's noise-LFO + hiss skipped. The third
// instance of the mod-delay technique after chorus + flanger (read via the shared moddel_hermite).
#define TAPE_BUF_LEN     1024
#define TAPE_BASE_DELAY  320.0f     // ~7ms playback-head offset; > the max mod swing (read stays behind write)
#define TAPE_WOW_RATE    0.5f       // Hz — slow drift
#define TAPE_WOW_DEPTH   200.0f     // samples (±4.5ms)
#define TAPE_FLUT_RATE   6.0f       // Hz — fast warble
#define TAPE_FLUT_DEPTH  40.0f      // samples (±0.9ms)
static float tape_bufL[SOUND_FX_BUSES][TAPE_BUF_LEN];
static float tape_bufR[SOUND_FX_BUSES][TAPE_BUF_LEN];
static int   tape_widx[SOUND_FX_BUSES];
static float tape_wph [SOUND_FX_BUSES];    // wow LFO phase
static float tape_fph [SOUND_FX_BUSES];    // flutter LFO phase
static float tape_lpL [SOUND_FX_BUSES];    // HF-rolloff one-pole state (L)
static float tape_lpR [SOUND_FX_BUSES];    // … (R)
static float tape_wow [SOUND_FX_BUSES];
static float tape_flut[SOUND_FX_BUSES];
static float tape_sat [SOUND_FX_BUSES];
static bool  tape_used[SOUND_FX_BUSES];

static void tape_process(int b, float *mixL, float *mixR) {
    float sat = tape_sat[b];
    float L = *mixL, R = *mixR;
    if (sat > 0.0f) {                               // warm saturation (normalized: sat 0 = unity)
        float g = 1.0f + sat * 2.0f, ng = tanhf(g);
        L = tanhf(L * g) / ng;
        R = tanhf(R * g) / ng;
    }
    int widx = tape_widx[b];
    tape_bufL[b][widx] = L;                          // write the (saturated) signal to tape
    tape_bufR[b][widx] = R;
    tape_widx[b] = (widx + 1) % TAPE_BUF_LEN;
    if (tape_wow[b] > 0.0f || tape_flut[b] > 0.0f) { // wow + flutter pitch warble (one shared transport)
        tape_wph[b] += TAPE_WOW_RATE  / (float)SOUND_SAMPLE_RATE; if (tape_wph[b] >= 1.0f) tape_wph[b] -= 1.0f;
        tape_fph[b] += TAPE_FLUT_RATE / (float)SOUND_SAMPLE_RATE; if (tape_fph[b] >= 1.0f) tape_fph[b] -= 1.0f;
        float mod = sinf(tape_wph[b] * 6.2831853f) * tape_wow[b]  * TAPE_WOW_DEPTH
                  + sinf(tape_fph[b] * 6.2831853f) * tape_flut[b] * TAPE_FLUT_DEPTH;
        float rp = (float)widx - TAPE_BASE_DELAY + mod;
        while (rp < 0.0f) rp += TAPE_BUF_LEN;
        while (rp >= TAPE_BUF_LEN) rp -= TAPE_BUF_LEN;
        L = moddel_hermite(tape_bufL[b], TAPE_BUF_LEN, rp);   // shared transport → same read pos L/R
        R = moddel_hermite(tape_bufR[b], TAPE_BUF_LEN, rp);
    }
    float k = 1.0f - 0.45f * sat;                   // HF rolloff: k=1 (transparent) → 0.55 (warm) as sat rises
    tape_lpL[b] += k * (L - tape_lpL[b]); L = tape_lpL[b];
    tape_lpR[b] += k * (R - tape_lpR[b]); R = tape_lpR[b];
    *mixL = L; *mixR = R;
}

// configure a bus's chorus / flanger params (clamped) + mark it live. bus 0 = master.
static void fx_set_chorus(int b, float rate, float depth, float mix) {
    if (rate < 0.1f)  rate  = 0.1f;  if (rate > 5.0f)  rate  = 5.0f;
    if (depth < 0.0f) depth = 0.0f;  if (depth > 1.0f) depth = 1.0f;
    if (mix < 0.0f)   mix   = 0.0f;  if (mix > 1.0f)   mix   = 1.0f;
    cho_rate[b] = rate; cho_depth[b] = depth; cho_mix[b] = mix;
    cho_used[b] = true;
}
static void fx_set_flanger(int b, float rate, float depth, float fb, float mix) {
    if (rate < 0.05f) rate  = 0.05f; if (rate > 5.0f)  rate  = 5.0f;
    if (depth < 0.0f) depth = 0.0f;  if (depth > 1.0f) depth = 1.0f;
    if (fb < -0.95f)  fb    = -0.95f; if (fb > 0.95f)  fb    = 0.95f;   // signed: −=through-zero
    if (mix < 0.0f)   mix   = 0.0f;  if (mix > 1.0f)   mix   = 1.0f;
    flg_rate[b] = rate; flg_depth[b] = depth; flg_fb[b] = fb; flg_mix[b] = mix;
    flg_used[b] = true;
}
static void fx_set_tape(int b, float wow, float flut, float sat) {
    if (wow < 0.0f)  wow  = 0.0f; if (wow > 1.0f)  wow  = 1.0f;
    if (flut < 0.0f) flut = 0.0f; if (flut > 1.0f) flut = 1.0f;
    if (sat < 0.0f)  sat  = 0.0f; if (sat > 1.0f)  sat  = 1.0f;
    tape_wow[b] = wow; tape_flut[b] = flut; tape_sat[b] = sat;
    tape_used[b] = true;
}

// ── bitcrush — lo-fi quantizer: bit-depth reduction (floor to 2^bits levels) + sample-rate
// reduction (sample-and-hold every `rate` samples). navkit processBitcrusher, made stereo and
// per-bus. Cheap + stateless-ish (just the held sample + a counter). Dormant until first crush().
static float crush_bits [SOUND_FX_BUSES];  // bit depth 1..16
static float crush_rate [SOUND_FX_BUSES];  // sample-hold downsample factor 1..64
static float crush_mix  [SOUND_FX_BUSES];  // dry/wet 0..1
static float crush_holdL[SOUND_FX_BUSES];  // last sampled value (L)
static float crush_holdR[SOUND_FX_BUSES];  // … (R)
static int   crush_cnt  [SOUND_FX_BUSES];  // sample counter for rate reduction
static bool  crush_used [SOUND_FX_BUSES];
static void crush_process(int b, float *mixL, float *mixR) {
    float dryL = *mixL, dryR = *mixR;
    if (++crush_cnt[b] >= (int)crush_rate[b]) {        // sample-rate reduction: re-sample every `rate`
        crush_cnt[b] = 0;
        float levels = powf(2.0f, crush_bits[b]);      // bit-depth reduction: quantize to 2^bits steps
        crush_holdL[b] = floorf(*mixL * levels) / levels;
        crush_holdR[b] = floorf(*mixR * levels) / levels;
    }
    float mix = crush_mix[b];
    *mixL = dryL * (1.0f - mix) + crush_holdL[b] * mix;
    *mixR = dryR * (1.0f - mix) + crush_holdR[b] * mix;
}
static void fx_set_crush(int b, float bits, float rate, float mix) {
    if (bits < 1.0f) bits = 1.0f; if (bits > 16.0f) bits = 16.0f;
    if (rate < 1.0f) rate = 1.0f; if (rate > 64.0f) rate = 64.0f;
    if (mix < 0.0f)  mix  = 0.0f; if (mix > 1.0f)  mix  = 1.0f;
    crush_bits[b] = bits; crush_rate[b] = rate; crush_mix[b] = mix;
    crush_used[b] = (mix > 0.0f);   // mix 0 = off (like chorus/flanger/wah)
}

// ── auto-wah — a resonant bandpass SWEPT BY AN ENVELOPE FOLLOWER on the bus signal ──
// THE scar (0015 correction / §8.10): the realistic "woah-woah" auto-wah is a BUS effect, not
// per-voice — a per-voice filter can't sweep a chord coherently or pump with the groove. A bus-level
// envelope follower tracks the summed performance and opens a resonant SVF bandpass: louder playing →
// the filter opens = the funk-clav quack. navkit processWah (ENVELOPE mode); the SVF's bus-level use.
// Mono (like flanger). Closes the §8.10.1 PARKED auto-wah; the follower's "real home is bus-level".
#define WAH_FREQ_LOW   300.0f
#define WAH_FREQ_HIGH  2500.0f
static float wah_env [SOUND_FX_BUSES];     // envelope-follower state
static float wah_ic1 [SOUND_FX_BUSES];     // TPT SVF state
static float wah_ic2 [SOUND_FX_BUSES];
static float wah_sens[SOUND_FX_BUSES];     // envelope sensitivity (~0.3..5 internal)
static float wah_res [SOUND_FX_BUSES];     // resonance/Q 0..1 (the quack)
static float wah_mix [SOUND_FX_BUSES];     // dry/wet 0..1
static bool  wah_used[SOUND_FX_BUSES];
static float wah_lfo_rate [SOUND_FX_BUSES]; // navkit WAH_MODE_LFO: sweep rate Hz; 0 = follower (envelope) mode
static float wah_lfo_phase[SOUND_FX_BUSES]; // LFO phase 0..1

static void wah_process(int b, float *mixL, float *mixR) {
    float in = (*mixL + *mixR) * 0.5f;     // filter the mono sum
    float sweep;
    if (wah_lfo_rate[b] > 0.0f) {           // navkit WAH_MODE_LFO: a sine rocks the band (ignores dynamics)
        sweep = 0.5f + 0.5f * sinf(wah_lfo_phase[b] * 6.2831853f);
        wah_lfo_phase[b] += wah_lfo_rate[b] / (float)SOUND_SAMPLE_RATE;
        if (wah_lfo_phase[b] >= 1.0f) wah_lfo_phase[b] -= 1.0f;
    } else {                                // WAH_MODE_ENVELOPE: peak detector (fast attack, slow release)
        float level = fabsf(in) * wah_sens[b];
        if (level > wah_env[b]) wah_env[b] += 0.01f   * (level - wah_env[b]);   // attack
        else                    wah_env[b] += 0.0001f * (level - wah_env[b]);   // release
        sweep = wah_env[b]; if (sweep > 1.0f) sweep = 1.0f;
    }
    float freq = WAH_FREQ_LOW * powf(WAH_FREQ_HIGH / WAH_FREQ_LOW, sweep);  // exponential sweep (300→2500)
    if (freq > SOUND_SAMPLE_RATE * 0.45f) freq = SOUND_SAMPLE_RATE * 0.45f;
    // TPT state-variable bandpass (Zavalishin)
    float g = tanf(3.14159265f * freq / (float)SOUND_SAMPLE_RATE);
    float k = 2.0f - 2.0f * wah_res[b] * 0.99f;                            // resonance → narrow quack
    float a1 = 1.0f / (1.0f + g * (g + k));
    float a2 = g * a1, a3 = g * a2;
    float v3 = in - wah_ic2[b];
    float v1 = a1 * wah_ic1[b] + a2 * v3;
    float v2 = wah_ic2[b] + a2 * wah_ic1[b] + a3 * v3;
    wah_ic1[b] = 2.0f * v1 - wah_ic1[b];
    wah_ic2[b] = 2.0f * v2 - wah_ic2[b];
    if (wah_ic1[b] >  4.0f) wah_ic1[b] =  4.0f; if (wah_ic1[b] < -4.0f) wah_ic1[b] = -4.0f;  // runaway guard
    if (wah_ic2[b] >  4.0f) wah_ic2[b] =  4.0f; if (wah_ic2[b] < -4.0f) wah_ic2[b] = -4.0f;
    float bp = v1;                          // bandpass output (mono); dry kept per channel
    *mixL = *mixL * (1.0f - wah_mix[b]) + bp * wah_mix[b];
    *mixR = *mixR * (1.0f - wah_mix[b]) + bp * wah_mix[b];
}
static void fx_set_wah(int b, float sens, float res, float mix) {
    if (sens < 0.0f) sens = 0.0f; if (sens > 1.0f) sens = 1.0f;
    if (res < 0.0f)  res  = 0.0f; if (res > 1.0f)  res  = 1.0f;
    if (mix < 0.0f)  mix  = 0.0f; if (mix > 1.0f)  mix  = 1.0f;
    wah_sens[b] = 0.3f + sens * 4.7f;       // 0..1 → ~0.3..5.0 (navkit sensitivity range)
    wah_res[b] = res; wah_mix[b] = mix;
    wah_lfo_rate[b] = 0.0f;                 // follower (envelope) mode; LFO mode = fx_set_wah_lfo
    wah_used[b] = true;
}
// navkit WAH_MODE_LFO: same bus bandpass, swept by a sine at `rate` Hz instead of the follower.
static void fx_set_wah_lfo(int b, float rate, float res, float mix) {
    if (res < 0.0f) res = 0.0f; if (res > 1.0f) res = 1.0f;
    if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f;
    wah_lfo_rate[b] = rate < 0.05f ? 0.05f : rate;   // >0 selects LFO mode
    wah_res[b] = res; wah_mix[b] = mix;
    wah_used[b] = true;
}

// ── resonant FILTER — the DJ filter (studio.h filter()) ──
// The plain sweepable filter wah/formant are specialized cases of: one TPT state-variable filter
// (Zavalishin) in a selectable mode (LP/HP/BP/notch), cutoff + resonance set by the cart and RIDDEN
// live (the build-up / breakdown sweep). Per-channel state → preserves stereo. Cheap to re-call every
// frame (just stores 3 values — unlike the buffer effects, sweeping it live is fine). filt_used-gated.
static int   filt_mode[SOUND_FX_BUSES];     // FILTER_LOW / HIGH / BAND / NOTCH
static float filt_cut [SOUND_FX_BUSES];     // cutoff Hz
static float filt_res [SOUND_FX_BUSES];     // resonance 0..1 (peak height / the scream)
static float filt_ic1L[SOUND_FX_BUSES], filt_ic2L[SOUND_FX_BUSES];   // TPT integrator state, L
static float filt_ic1R[SOUND_FX_BUSES], filt_ic2R[SOUND_FX_BUSES];   //                        R
static bool  filt_used[SOUND_FX_BUSES];

static void filter_process(int b, float *mixL, float *mixR) {
    float freq = filt_cut[b];
    if (freq < 20.0f) freq = 20.0f;
    if (freq > SOUND_SAMPLE_RATE * 0.45f) freq = SOUND_SAMPLE_RATE * 0.45f;
    float g  = tanf(3.14159265f * freq / (float)SOUND_SAMPLE_RATE);
    float k  = 2.0f - 2.0f * filt_res[b] * 0.99f;       // small k = resonant peak
    float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    int   m  = filt_mode[b];
    // L channel
    float in = *mixL;
    float v3 = in - filt_ic2L[b];
    float v1 = a1 * filt_ic1L[b] + a2 * v3;
    float v2 = filt_ic2L[b] + a2 * filt_ic1L[b] + a3 * v3;
    filt_ic1L[b] = 2.0f * v1 - filt_ic1L[b];
    filt_ic2L[b] = 2.0f * v2 - filt_ic2L[b];
    if (filt_ic1L[b] >  4.0f) filt_ic1L[b] =  4.0f; if (filt_ic1L[b] < -4.0f) filt_ic1L[b] = -4.0f;
    if (filt_ic2L[b] >  4.0f) filt_ic2L[b] =  4.0f; if (filt_ic2L[b] < -4.0f) filt_ic2L[b] = -4.0f;
    *mixL = (m == FILTER_HIGH) ? in - k * v1 - v2 : (m == FILTER_BAND) ? v1
          : (m == FILTER_NOTCH) ? in - k * v1 : v2;     // v2 = lowpass (default)
    // R channel
    in = *mixR;
    v3 = in - filt_ic2R[b];
    v1 = a1 * filt_ic1R[b] + a2 * v3;
    v2 = filt_ic2R[b] + a2 * filt_ic1R[b] + a3 * v3;
    filt_ic1R[b] = 2.0f * v1 - filt_ic1R[b];
    filt_ic2R[b] = 2.0f * v2 - filt_ic2R[b];
    if (filt_ic1R[b] >  4.0f) filt_ic1R[b] =  4.0f; if (filt_ic1R[b] < -4.0f) filt_ic1R[b] = -4.0f;
    if (filt_ic2R[b] >  4.0f) filt_ic2R[b] =  4.0f; if (filt_ic2R[b] < -4.0f) filt_ic2R[b] = -4.0f;
    *mixR = (m == FILTER_HIGH) ? in - k * v1 - v2 : (m == FILTER_BAND) ? v1
          : (m == FILTER_NOTCH) ? in - k * v1 : v2;
}
static void fx_set_filter(int b, int mode, float cutoff, float res) {
    if (mode == FILTER_OFF) { filt_used[b] = false; return; }   // OFF = bypass → byte-identical
    if (res < 0.0f) res = 0.0f; if (res > 1.0f) res = 1.0f;
    filt_mode[b] = mode; filt_cut[b] = cutoff; filt_res[b] = res;
    filt_used[b] = true;
}

// ── formant filter — vowel resonance, the "push any sound through a voice" effect ──
// FOUR parallel TPT bandpasses tuned to the human vowel formants (F1..F4), summed by the vowel's
// relative amplitudes → whatever passes through takes on an "ooh / aah / eee" vocal colour (the
// synth-world formant/vowel filter; a wah pedal is the one-peak version of this). The vowel knob
// sweeps the U→O→A→E→I path; q narrows the peaks (broad → pronounced/nasal). REUSES navkit's
// measured vowel tables (vox_vowel_f/a/bw, shared with INSTR_VOICE) — the lookup lives in
// fx_set_formant (defined after those tables); formant_process here is table-free (just runs the
// SVFs from precomputed band targets) so it can sit before apply_insert. Filters the mono sum (the
// wet is mono, like wah); dry stays per-channel. Dormant until formant()/instrument_formant() →
// non-users byte-identical. design: docs/guides/effects-recipes.md → formant.
static float fmt_freq[SOUND_FX_BUSES][4];   // band centre freqs (Hz), from the vowel table
static float fmt_k   [SOUND_FX_BUSES][4];   // band damping = 1/Q = bw/fc (precomputed by fx_set_formant)
static float fmt_amp [SOUND_FX_BUSES][4];   // band relative amplitudes (F1 loudest)
static float fmt_ic1 [SOUND_FX_BUSES][4];   // TPT SVF state per band
static float fmt_ic2 [SOUND_FX_BUSES][4];
static float fmt_mix [SOUND_FX_BUSES];      // dry/wet 0..1
static bool  fmt_used[SOUND_FX_BUSES];
static void formant_process(int b, float *mixL, float *mixR) {
    float in = (*mixL + *mixR) * 0.5f;      // filter the mono sum (wet is mono, like wah)
    float out = 0.0f;
    for (int i = 0; i < 4; i++) {
        float fc = fmt_freq[b][i];
        if (fc > SOUND_SAMPLE_RATE * 0.45f) fc = SOUND_SAMPLE_RATE * 0.45f;
        float g = tanf(3.14159265f * fc / (float)SOUND_SAMPLE_RATE);   // TPT bandpass (Zavalishin), one per formant
        float k = fmt_k[b][i];
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1, a3 = g * a2;
        float v3 = in - fmt_ic2[b][i];
        float v1 = a1 * fmt_ic1[b][i] + a2 * v3;
        float v2 = fmt_ic2[b][i] + a2 * fmt_ic1[b][i] + a3 * v3;
        fmt_ic1[b][i] = 2.0f * v1 - fmt_ic1[b][i];
        fmt_ic2[b][i] = 2.0f * v2 - fmt_ic2[b][i];
        if (fmt_ic1[b][i] >  4.0f) fmt_ic1[b][i] =  4.0f; if (fmt_ic1[b][i] < -4.0f) fmt_ic1[b][i] = -4.0f;  // runaway guard
        if (fmt_ic2[b][i] >  4.0f) fmt_ic2[b][i] =  4.0f; if (fmt_ic2[b][i] < -4.0f) fmt_ic2[b][i] = -4.0f;
        out += v1 * fmt_amp[b][i];          // bandpass output = v1, weighted by the formant's amp
    }
    out *= 1.6f;                            // makeup for the narrow summed bands (dialled by ear)
    if (out > 1.0f || out < -1.0f) out = tanhf(out);   // soft-clip the wet (navkit filterbank does this) — a resonant peak can't spike
    *mixL = *mixL * (1.0f - fmt_mix[b]) + out * fmt_mix[b];
    *mixR = *mixR * (1.0f - fmt_mix[b]) + out * fmt_mix[b];
}

// ── EQ — 3-band tone control: BOOST or cut LOW / MID / HIGH, ±12 dB each. ──
// THE library's only BOOST (the SVF filters can only cut). navkit processMasterEQ, made stereo,
// per-bus, AND 3-band: two cascaded one-pole crossovers split the signal into low (<~80 Hz) / mid
// (~80 Hz–6 kHz) / top (>~6 kHz) bands, then each band is scaled by its own dB→linear gain. navkit
// left the mid at unity (a 2-band shelf); the split already computes it, so the mid knob is free.
// At 0/0/0 dB the three bands sum back to the input (gain 1.0 = exact reconstruction). Paired with
// DRIVE_ASYM (pre/post EQ around a clipper) → a real guitar-amp tone. Dormant until the first eq()
// call → non-users byte-identical.
#define EQ_LOW_FREQ   80.0f     // low/mid crossover (Hz) — reaches sub-bass / body
#define EQ_HIGH_FREQ  6000.0f   // mid/high crossover (Hz) — presence / air
static float eq_low_g [SOUND_FX_BUSES];   // low-band  linear gain = 10^(dB/20)
static float eq_mid_g [SOUND_FX_BUSES];   // mid-band  linear gain
static float eq_high_g[SOUND_FX_BUSES];   // high-band linear gain
static float eq_loL[SOUND_FX_BUSES];      // low-crossover one-pole state (L)
static float eq_loR[SOUND_FX_BUSES];      // … (R)
static float eq_hiL[SOUND_FX_BUSES];      // mid/top-crossover one-pole state (L)
static float eq_hiR[SOUND_FX_BUSES];      // … (R)
static bool  eq_used[SOUND_FX_BUSES];
static void eq_process(int b, float *mixL, float *mixR) {
    float lowCoeff  = EQ_LOW_FREQ  * 6.2831853f / (float)SOUND_SAMPLE_RATE;  if (lowCoeff  > 0.99f) lowCoeff  = 0.99f;
    float highCoeff = EQ_HIGH_FREQ * 6.2831853f / (float)SOUND_SAMPLE_RATE;  if (highCoeff > 0.99f) highCoeff = 0.99f;
    float lg = eq_low_g[b], mg = eq_mid_g[b], hg = eq_high_g[b];
    // L: low = LP at EQ_LOW_FREQ; remainder split again at EQ_HIGH_FREQ into mid + top
    eq_loL[b] += lowCoeff * (*mixL - eq_loL[b]);
    float lowL = eq_loL[b], hiL = *mixL - eq_loL[b];
    eq_hiL[b] += highCoeff * (hiL - eq_hiL[b]);
    float midL = eq_hiL[b], topL = hiL - eq_hiL[b];
    *mixL = lowL * lg + midL * mg + topL * hg;
    // R
    eq_loR[b] += lowCoeff * (*mixR - eq_loR[b]);
    float lowR = eq_loR[b], hiR = *mixR - eq_loR[b];
    eq_hiR[b] += highCoeff * (hiR - eq_hiR[b]);
    float midR = eq_hiR[b], topR = hiR - eq_hiR[b];
    *mixR = lowR * lg + midR * mg + topR * hg;
}
static void fx_set_eq(int b, float low_db, float mid_db, float high_db) {
    if (low_db  < -12.0f) low_db  = -12.0f; if (low_db  > 12.0f) low_db  = 12.0f;
    if (mid_db  < -12.0f) mid_db  = -12.0f; if (mid_db  > 12.0f) mid_db  = 12.0f;
    if (high_db < -12.0f) high_db = -12.0f; if (high_db > 12.0f) high_db = 12.0f;
    eq_low_g[b]  = powf(10.0f, low_db  / 20.0f);
    eq_mid_g[b]  = powf(10.0f, mid_db  / 20.0f);
    eq_high_g[b] = powf(10.0f, high_db / 20.0f);
    eq_used[b] = true;
}

// ── tremolo — volume LFO (Fender/Wurlitzer amp wobble) ───────────────────────────────────
// A VERBATIM port of navkit's processTremolo: one LFO ducks the bus level — `out = in·(1 −
// depth·(1 − mod))`, where `mod` is the chosen shape in 0..1 (so it only attenuates below unity,
// never boosts — the amp-tremolo character). Per-bus phase, so a per-instrument tremolo wobbles
// that instrument's WHOLE output in unison (the coherent amp wobble a per-voice LFO_VOLUME can't
// give — every note shares one phase, tails included). Stereo (same gain L/R). Dormant until the
// first tremolo()/instrument_tremolo() with depth>0 → non-users byte-identical.
#define TREM_SHAPE_SINE   0
#define TREM_SHAPE_SQUARE 1
#define TREM_SHAPE_TRI    2
static float trem_rate [SOUND_FX_BUSES];   // LFO rate Hz
static float trem_depth[SOUND_FX_BUSES];   // modulation depth 0..1
static int   trem_shape[SOUND_FX_BUSES];   // TREM_SHAPE_*
static float trem_phase[SOUND_FX_BUSES];   // LFO phase 0..1
static bool  trem_used [SOUND_FX_BUSES];
static void trem_process(int b, float *mixL, float *mixR) {
    float phase = trem_phase[b];
    float mod;
    switch (trem_shape[b]) {
        case TREM_SHAPE_SQUARE:
            mod = phase < 0.5f ? 1.0f : 0.0f;
            break;
        case TREM_SHAPE_TRI:
            mod = phase < 0.5f ? phase * 4.0f - 1.0f : 3.0f - phase * 4.0f;
            mod = mod * 0.5f + 0.5f;
            break;
        default: // sine
            mod = 0.5f + 0.5f * sinf(phase * 6.2831853f);
            break;
    }
    trem_phase[b] += trem_rate[b] * (1.0f / (float)SOUND_SAMPLE_RATE);
    if (trem_phase[b] >= 1.0f) trem_phase[b] -= 1.0f;
    float g = 1.0f - trem_depth[b] * (1.0f - mod);
    *mixL *= g; *mixR *= g;
}
static void fx_set_tremolo(int b, float rate, float depth, int shape) {
    if (rate  < 0.1f)  rate  = 0.1f;  if (rate  > 20.0f) rate  = 20.0f;
    if (depth < 0.0f)  depth = 0.0f;  if (depth > 1.0f)  depth = 1.0f;
    if (shape < 0 || shape > TREM_SHAPE_TRI) shape = TREM_SHAPE_SINE;
    trem_rate[b] = rate; trem_depth[b] = depth; trem_shape[b] = shape;
    trem_used[b] = (depth > 0.0f);   // depth 0 = off (identity gain), like mix 0 elsewhere
}

// ── auto-pan — the tremolo LFO applied ANTIPHASE to L/R: a stereo sweep (Rhodes suitcase vibrato /
// rotary-style motion). gL is the plain tremolo gain `1 − depth·(1 − mod)`; gR is its complement
// `1 − depth·mod`, so as the LFO rises the level shifts to the LEFT and as it falls to the RIGHT
// (depth 1 = a channel hits silence = full L↔R pan). Its OWN insert (FX_PAN) + OWN LFO state — so it
// stacks with tremolo on one bus (a fast throb AND a slow stereo drift at once), the whole reason it
// isn't a mode of tremolo. Reuses the TREM_SHAPE_* shapes. Only attenuates (never boosts), so the
// summed level never exceeds the dry input — no added clip risk. Dormant until autopan()/
// instrument_autopan() with depth>0 → non-users byte-identical.
static float pan_rate [SOUND_FX_BUSES];   // LFO rate Hz
static float pan_depth[SOUND_FX_BUSES];   // pan depth 0..1 (1 = full L↔R)
static int   pan_shape[SOUND_FX_BUSES];   // TREM_SHAPE_*
static float pan_phase[SOUND_FX_BUSES];   // LFO phase 0..1
static bool  pan_used [SOUND_FX_BUSES];
static void pan_process(int b, float *mixL, float *mixR) {
    float phase = pan_phase[b];
    float mod;
    switch (pan_shape[b]) {
        case TREM_SHAPE_SQUARE:
            mod = phase < 0.5f ? 1.0f : 0.0f;
            break;
        case TREM_SHAPE_TRI:
            mod = phase < 0.5f ? phase * 4.0f - 1.0f : 3.0f - phase * 4.0f;
            mod = mod * 0.5f + 0.5f;
            break;
        default: // sine
            mod = 0.5f + 0.5f * sinf(phase * 6.2831853f);
            break;
    }
    pan_phase[b] += pan_rate[b] * (1.0f / (float)SOUND_SAMPLE_RATE);
    if (pan_phase[b] >= 1.0f) pan_phase[b] -= 1.0f;
    float gL = 1.0f - pan_depth[b] * (1.0f - mod);   // L full at the LFO peak
    float gR = 1.0f - pan_depth[b] * mod;            // R full at the LFO trough (antiphase)
    *mixL *= gL; *mixR *= gR;
}
static void fx_set_autopan(int b, float rate, float depth, int shape) {
    if (rate  < 0.1f)  rate  = 0.1f;  if (rate  > 20.0f) rate  = 20.0f;
    if (depth < 0.0f)  depth = 0.0f;  if (depth > 1.0f)  depth = 1.0f;
    if (shape < 0 || shape > TREM_SHAPE_TRI) shape = TREM_SHAPE_SINE;
    pan_rate[b] = rate; pan_depth[b] = depth; pan_shape[b] = shape;
    pan_used[b] = (depth > 0.0f);   // depth 0 = off (identity gain), like tremolo
}

// ── ring modulator — multiply the bus by a sine CARRIER: metallic/clangorous sidebands ───────────
// out = in·((1−mix) + mix·carrier), carrier = sin(2π·f·t) in [−1,1]. A BIPOLAR multiply (unlike
// tremolo's unipolar 0..1 gain), so it generates inharmonic sum/difference tones — the robot/bell/
// Dalek clang, NOT just a wobble. |carrier|≤1 and the dry/wet blend keep |out|≤|in|, so no added clip
// risk. Per-bus carrier phase. Dormant until ringmod()/instrument_ringmod() with mix>0 → byte-identical.
static float rm_freq [SOUND_FX_BUSES];   // carrier frequency Hz
static float rm_mix  [SOUND_FX_BUSES];   // dry/wet 0..1
static float rm_phase[SOUND_FX_BUSES];   // carrier phase 0..1
static bool  rm_used [SOUND_FX_BUSES];
static void rm_process(int b, float *mixL, float *mixR) {
    float c = sinf(rm_phase[b] * 6.2831853f);
    rm_phase[b] += rm_freq[b] * (1.0f / (float)SOUND_SAMPLE_RATE);
    if (rm_phase[b] >= 1.0f) rm_phase[b] -= 1.0f;
    float m = rm_mix[b];
    *mixL = *mixL * (1.0f - m) + (*mixL * c) * m;
    *mixR = *mixR * (1.0f - m) + (*mixR * c) * m;
}
static void fx_set_ringmod(int b, float freq, float mix) {
    if (freq < 1.0f) freq = 1.0f;  if (freq > 8000.0f) freq = 8000.0f;
    if (mix  < 0.0f) mix  = 0.0f;  if (mix  > 1.0f)    mix  = 1.0f;
    rm_freq[b] = freq; rm_mix[b] = mix;
    rm_used[b] = (mix > 0.0f);   // mix 0 = off (dry), like the other inserts
}

// ── phaser — cascaded allpass chain swept by an LFO (the 70s Rhodes / Small Stone swirl) ─────────
// A VERBATIM port of navkit's bus processPhaser: N first-order allpass stages in series, all sharing
// one LFO-swept coefficient (coeff = 0.5 + lfo·depth·0.4, navkit's 0.1..0.9 range), with feedback
// from the last stage's state — the moving NOTCHES in the spectrum are the phaser sound (distinct
// from the flanger's swept comb). navkit's processor is mono; we run it PER CHANNEL (own allpass
// memory L/R) sharing one LFO phase, so a centered source matches navkit's mono exactly and a stereo
// source keeps its width. stages 2..8 (4 = the classic Phase-90). Dormant until mix>0 → byte-identical.
#define SOUND_PHASER_STAGES 8
static float phaser_rate [SOUND_FX_BUSES];   // LFO rate Hz
static float phaser_depth[SOUND_FX_BUSES];   // sweep depth 0..1
static float phaser_fb   [SOUND_FX_BUSES];   // feedback (resonance), signed
static float phaser_mix  [SOUND_FX_BUSES];   // dry/wet 0..1
static int   phaser_stages[SOUND_FX_BUSES];  // allpass stages 2..8
static float phaser_phase[SOUND_FX_BUSES];   // LFO phase 0..1 (shared L/R)
static float phaser_stL[SOUND_FX_BUSES][SOUND_PHASER_STAGES];  // per-stage allpass output (L)
static float phaser_pvL[SOUND_FX_BUSES][SOUND_PHASER_STAGES];  // per-stage allpass input  (L)
static float phaser_stR[SOUND_FX_BUSES][SOUND_PHASER_STAGES];  // … (R)
static float phaser_pvR[SOUND_FX_BUSES][SOUND_PHASER_STAGES];
static bool  phaser_used[SOUND_FX_BUSES];
static float phaser_chan(float in, float coeff, float fb, float mix, int stages, float *st, float *pv) {
    float dry = in;
    float pIn = in + st[stages - 1] * fb;
    if (pIn > 1.5f) pIn = 1.5f; if (pIn < -1.5f) pIn = -1.5f;
    for (int s = 0; s < stages; s++) {
        float ap = coeff * (pIn - st[s]) + pv[s];
        pv[s] = pIn;
        st[s] = ap;
        pIn = ap;
    }
    return dry * (1.0f - mix) + pIn * mix;
}
static void phaser_process(int b, float *mixL, float *mixR) {
    phaser_phase[b] += phaser_rate[b] * (1.0f / (float)SOUND_SAMPLE_RATE);
    if (phaser_phase[b] >= 1.0f) phaser_phase[b] -= 1.0f;
    float lfo = sinf(phaser_phase[b] * 6.2831853f);
    float coeff = 0.5f + lfo * phaser_depth[b] * 0.4f;   // navkit: cCenter 0.5 ± lfo·depth·cRange(0.4)
    int stages = phaser_stages[b];
    float fb = phaser_fb[b], mix = phaser_mix[b];
    *mixL = phaser_chan(*mixL, coeff, fb, mix, stages, phaser_stL[b], phaser_pvL[b]);
    *mixR = phaser_chan(*mixR, coeff, fb, mix, stages, phaser_stR[b], phaser_pvR[b]);
}
static void fx_set_phaser(int b, float rate, float depth, float feedback, float mix, int stages) {
    if (rate  < 0.0f)  rate  = 0.0f;  if (rate  > 10.0f) rate  = 10.0f;
    if (depth < 0.0f)  depth = 0.0f;  if (depth > 1.0f)  depth = 1.0f;
    if (feedback < -0.95f) feedback = -0.95f; if (feedback > 0.95f) feedback = 0.95f;
    if (mix < 0.0f)  mix  = 0.0f; if (mix > 1.0f) mix = 1.0f;
    if (stages < 2) stages = 2; if (stages > SOUND_PHASER_STAGES) stages = SOUND_PHASER_STAGES;
    phaser_rate[b] = rate; phaser_depth[b] = depth; phaser_fb[b] = feedback;
    phaser_mix[b] = mix; phaser_stages[b] = stages;
    phaser_used[b] = (mix > 0.0f);   // mix 0 = off, like chorus/flanger/tremolo
}

// ── Leslie — rotary speaker (the organ's spinning horn+drum cabinet) ─────────────────────────────
// A VERBATIM port of navkit's processLeslie (the Leslie 122 model): a 1-pole crossover at 800 Hz
// splits the signal into a DRUM band (bass, gentle sine AM) and a HORN band (treble, shaped AM +
// physical DOPPLER via a modulated delay line). The two rotors spin at INDEPENDENT rates with
// asymmetric spin-up/down inertia (horn light/fast, drum heavy/slow) — so flipping STOP→SLOW→FAST
// ramps them in over seconds, the iconic chorale↔tremolo swell. NOT a recipe of the 3 per-voice
// LFOs (decision 0015, 2026-06-12): an LFO can't split the band at a crossover, can't do a delay-
// line Doppler (LFO_PITCH is a pitch-bend, not a delay tap), and can't run two inertial rotors —
// so it clears the "prove it can't be a recipe" gate. navkit is mono; we run it per channel (own
// crossover + Doppler buffer L/R) sharing one rotor, so a centered source matches navkit's mono
// exactly and a stereo source keeps its width. Pinned LAST in the insert chain (the speaker/cabinet
// output stage, like the soft-clip — not a reorderable pedal). Dormant until mix>0 → byte-identical.
#define LESLIE_BUF        512        // ~11.6ms horn Doppler delay line (navkit LESLIE_BUFFER_SIZE)
#define LESLIE_XOVER_COEF 0.1074f    // 1-exp(-2π·800/44100), the 800 Hz drum/horn crossover (navkit, precomputed)
#define LESLIE_BASE_MS    3.0f       // horn base delay (center of the Doppler modulation)
#define LESLIE_DOPP_MS    0.5f       // max Doppler excursion ±0.5ms
#define LESLIE_HORN_SLOW  0.8f       // chorale horn rate Hz (48 RPM)   ·  FAST = tremolo
#define LESLIE_HORN_FAST  6.6f       // tremolo horn rate Hz (396 RPM)
#define LESLIE_DRUM_SLOW  0.7f       // chorale drum rate Hz (42 RPM)
#define LESLIE_DRUM_FAST  5.8f       // tremolo drum rate Hz (348 RPM)
#define LESLIE_HORN_ACC   0.7f       // horn spin-up time constant (s)  — light, ramps fast
#define LESLIE_HORN_DEC   1.2f       // horn spin-down
#define LESLIE_DRUM_ACC   4.0f       // drum spin-up — heavy, ramps slow
#define LESLIE_DRUM_DEC   5.0f
static int   leslie_speed[SOUND_FX_BUSES];   // 0=stop 1=slow(chorale) 2=fast(tremolo) — LESLIE_* in studio.h
static float leslie_drive[SOUND_FX_BUSES];   // pre-amp tube overdrive 0..1
static float leslie_bal  [SOUND_FX_BUSES];   // horn/drum balance 0=drum .. 0.5=equal .. 1=horn
static float leslie_dopp [SOUND_FX_BUSES];   // horn Doppler depth 0..1
static float leslie_mix  [SOUND_FX_BUSES];   // dry/wet 0..1
static bool  leslie_used [SOUND_FX_BUSES];
static float leslie_hornPh[SOUND_FX_BUSES], leslie_drumPh[SOUND_FX_BUSES];   // rotor phase (shared L/R)
static float leslie_hornRt[SOUND_FX_BUSES], leslie_drumRt[SOUND_FX_BUSES];   // rotor rate, slewed toward target
static float leslie_xoL[SOUND_FX_BUSES], leslie_xoR[SOUND_FX_BUSES];          // crossover LP state (per channel)
static int   leslie_wpos[SOUND_FX_BUSES];                                     // Doppler write pos (shared)
static float leslie_bufL[SOUND_FX_BUSES][LESLIE_BUF], leslie_bufR[SOUND_FX_BUSES][LESLIE_BUF];

// sidechain / bus-compression DYNAMICS (studio.h sidechain()/sidechain_key()/glue()). A gain stage
// on a victim bus driven by an envelope follower: keyed off a TRIGGER (sidechain_key → sc_key_lvl)
// or, for glue, off the bus's OWN level. Dormant (used=false) until configured → byte-identical.
#define N_SC_KEYS 4   // independent trigger buses (kick/snare/…); sidechain_key key index 0..3
typedef struct { bool used; int key; float amount, atk, rel, env; } SideChain;  // key<0 = glue (self-keyed)
static SideChain sc[SOUND_FX_BUSES];      // one keyed comp per victim bus (bus 0 = master)
static float     sc_key_lvl[N_SC_KEYS];   // this sample's summed trigger sends per key (reset each sample)
// duck gain for victim bus b. Updates the envelope follower; reads its trigger key (or the bus's own
// level when self-keyed/glue). Returns the gain to multiply the bus by (1 = open, <1 = ducked).
static float sc_apply(int b, float curL, float curR) {
    SideChain *s = &sc[b];
    if (!s->used) return 1.0f;
    float k = (s->key >= 0) ? fabsf(sc_key_lvl[s->key])
                            : (fabsf(curL) > fabsf(curR) ? fabsf(curL) : fabsf(curR));   // glue: self-key
    s->env += (k > s->env ? s->atk : s->rel) * (k - s->env);   // fast attack, slow release
    float e = s->env > 1.0f ? 1.0f : s->env;
    return 1.0f - s->amount * e;
}
static float leslie_pre(float s, float drive) {   // tube pre-amp: Padé tanh (navkit, no libm call)
    if (drive <= 0.001f) return s;
    s *= 1.0f + drive * 5.0f;
    float x2 = s * s;
    return s * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static void leslie_process(int b, float *mixL, float *mixR) {
    float dt = 1.0f / (float)SOUND_SAMPLE_RATE;
    // rotor rates slew toward their speed target (independent, asymmetric accel/decel)
    float hornT, drumT;
    switch (leslie_speed[b]) {
        case 2:  hornT = LESLIE_HORN_FAST; drumT = LESLIE_DRUM_FAST; break;   // fast / tremolo
        case 0:  hornT = 0.0f;             drumT = 0.0f;             break;   // stop / brake
        default: hornT = LESLIE_HORN_SLOW; drumT = LESLIE_DRUM_SLOW; break;   // slow / chorale
    }
    float hA = dt / (hornT > leslie_hornRt[b] ? LESLIE_HORN_ACC : LESLIE_HORN_DEC); if (hA > 1.0f) hA = 1.0f;
    float dA = dt / (drumT > leslie_drumRt[b] ? LESLIE_DRUM_ACC : LESLIE_DRUM_DEC); if (dA > 1.0f) dA = 1.0f;
    leslie_hornRt[b] += hA * (hornT - leslie_hornRt[b]);
    leslie_drumRt[b] += dA * (drumT - leslie_drumRt[b]);
    // advance the two rotors (shared across channels)
    leslie_drumPh[b] += leslie_drumRt[b] * dt; if (leslie_drumPh[b] >= 1.0f) leslie_drumPh[b] -= 1.0f;
    leslie_hornPh[b] += leslie_hornRt[b] * dt; if (leslie_hornPh[b] >= 1.0f) leslie_hornPh[b] -= 1.0f;
    float drumAM = 1.0f - 0.3f * (0.5f - 0.5f * cosf(leslie_drumPh[b] * 6.2831853f));
    float ang    = leslie_hornPh[b] * 6.2831853f;
    float hornAM = 0.5f + 0.5f * cosf(ang) + 0.12f * cosf(2.0f * ang);   // shaped: directional horn bell
    hornAM = hornAM * 0.75f + 0.15f;
    if (hornAM < 0.1f) hornAM = 0.1f; if (hornAM > 1.0f) hornAM = 1.0f;
    float delaySamples = LESLIE_BASE_MS * 0.001f * SOUND_SAMPLE_RATE
                       + LESLIE_DOPP_MS * 0.001f * SOUND_SAMPLE_RATE * leslie_dopp[b] * sinf(ang);
    float bal = leslie_bal[b];
    float drumLvl = 2.0f * (1.0f - bal); if (drumLvl > 1.0f) drumLvl = 1.0f;
    float hornLvl = 2.0f * bal;          if (hornLvl > 1.0f) hornLvl = 1.0f;
    float mix = leslie_mix[b];
    // per channel: pre-amp → crossover → drum AM + horn Doppler/AM → recombine
    float sL = leslie_pre(*mixL, leslie_drive[b]);
    float sR = leslie_pre(*mixR, leslie_drive[b]);
    leslie_xoL[b] += LESLIE_XOVER_COEF * (sL - leslie_xoL[b]);   float loL = leslie_xoL[b], hiL = sL - loL;
    leslie_xoR[b] += LESLIE_XOVER_COEF * (sR - leslie_xoR[b]);   float loR = leslie_xoR[b], hiR = sR - loR;
    leslie_bufL[b][leslie_wpos[b]] = hiL;
    leslie_bufR[b][leslie_wpos[b]] = hiR;
    leslie_wpos[b] = (leslie_wpos[b] + 1) % LESLIE_BUF;
    float rp = (float)leslie_wpos[b] - delaySamples; if (rp < 0.0f) rp += LESLIE_BUF;
    float wetL = (loL * drumAM) * drumLvl + (moddel_hermite(leslie_bufL[b], LESLIE_BUF, rp) * hornAM) * hornLvl;
    float wetR = (loR * drumAM) * drumLvl + (moddel_hermite(leslie_bufR[b], LESLIE_BUF, rp) * hornAM) * hornLvl;
    *mixL = *mixL * (1.0f - mix) + wetL * mix;
    *mixR = *mixR * (1.0f - mix) + wetR * mix;
}
static void fx_set_leslie(int b, int speed, float drive, float balance, float doppler, float mix) {
    if (speed < 0) speed = 0; if (speed > 2) speed = 2;
    if (drive   < 0.0f) drive   = 0.0f; if (drive   > 1.0f) drive   = 1.0f;
    if (balance < 0.0f) balance = 0.0f; if (balance > 1.0f) balance = 1.0f;
    if (doppler < 0.0f) doppler = 0.0f; if (doppler > 1.0f) doppler = 1.0f;
    if (mix     < 0.0f) mix     = 0.0f; if (mix     > 1.0f) mix     = 1.0f;
    leslie_speed[b] = speed; leslie_drive[b] = drive; leslie_bal[b] = balance;
    leslie_dopp[b] = doppler; leslie_mix[b] = mix;
    leslie_used[b] = (mix > 0.0f);   // mix 0 = off, like the other inserts
}

// ── granular delay (grains) — capture live audio, replay overlapping windowed grains ────────────
// A VERBATIM port of navkit's granular_delay.h: incoming audio (+ feedback) writes into a capture
// ring buffer; on a density schedule we spawn grains that read scattered windowed slices of the
// recent past and sum them with a Hanning envelope — a "Cosmos" evolving cloud. FREEZE stops the
// write so the captured buffer loops forever (held chord → infinite shimmer). Reads through the
// shared moddel_hermite (which IS navkit's hermiteInterpolate — already ported for chorus/flanger/
// tape; "write the technique once"). Big-buffer + many-grain, so it lives in a small POOL of tanks
// (like the reverb tanks) mapped per-bus on demand, NOT one buffer per bus. Dormant until grains()/
// instrument_grains() (mix>0) → byte-identical. Mono core (v1), like echo_insert / the reverb insert.
#define SOUND_GRAIN_TANKS  2                          // pool size: master + one instrument bus at once (~1MB total)
#define GRAIN_BUF_LEN      (SOUND_SAMPLE_RATE * 3)    // 3 s capture (navkit uses 5 s; 3 s is plenty of lookback, half the RAM)
#define GRAIN_MAX_GRAINS   24                         // max simultaneous grains (navkit GRAN_DELAY_MAX_GRAINS)
typedef struct {
    float readPos, posInc, envPhase, envInc, amp;   // navkit GranDelayGrain: read cursor, speed, Hanning phase+inc, level
    bool  active;
} GrainVoice;
typedef struct {
    float buf[GRAIN_BUF_LEN];
    GrainVoice grains[GRAIN_MAX_GRAINS];
    int    writePos;
    float  spawnTimer, lastOut;
    unsigned int noiseSeed;
    float  mix, feedback, grainSize, density, position, scatter;   // grainSize ms · density /s · position/scatter 0..1 · feedback 0..0.9
    bool   freeze, used;
} GrainTank;
static GrainTank grain_pool[SOUND_GRAIN_TANKS];
static int8_t    grain_tank_of[SOUND_FX_BUSES];   // bus → pool tank index, or -1 = none (default -1)
static int       grain_next     = 0;              // next free pool tank to hand out
static int       grain_overflow = 0;              // count of grains() requests dropped (pool exhausted)

// assign a pool tank to bus b on first use; -1 = pool exhausted (caller bails → that bus stays dry)
static int grain_tank_for_bus(int b) {
    if (grain_tank_of[b] >= 0) return grain_tank_of[b];
    if (grain_next >= SOUND_GRAIN_TANKS) { grain_overflow++; return -1; }
    int t = grain_next++;
    grain_tank_of[b] = (int8_t)t;
    return t;
}

static void _grain_spawn(GrainTank *gt) {
    int slot = -1;
    for (int i = 0; i < GRAIN_MAX_GRAINS; i++) if (!gt->grains[i].active) { slot = i; break; }
    if (slot < 0) return;
    GrainVoice *g = &gt->grains[slot];
    float grainSamples = (gt->grainSize / 1000.0f) * (float)SOUND_SAMPLE_RATE;
    if (grainSamples < 2.0f) grainSamples = 2.0f;
    float minLookback = grainSamples;                                  // position=1 → near live edge, 0 → deep past
    float maxLookback = (float)(GRAIN_BUF_LEN - 1) - minLookback;
    if (maxLookback < minLookback) maxLookback = minLookback;
    float lookback = minLookback + (1.0f - gt->position) * maxLookback;
    gt->noiseSeed = gt->noiseSeed * 1103515245u + 12345u;             // navkit LCG scatter
    float rnd = (float)((int)((gt->noiseSeed >> 16) & 0xFFFF)) / 32767.5f - 1.0f;
    float scatterSamples = rnd * gt->scatter * GRAIN_BUF_LEN * 0.25f;
    float readStart = (float)gt->writePos - lookback + scatterSamples;
    while (readStart < 0)              readStart += GRAIN_BUF_LEN;
    while (readStart >= GRAIN_BUF_LEN) readStart -= GRAIN_BUF_LEN;
    g->readPos = readStart; g->posInc = 1.0f; g->envPhase = 0.0f;
    g->envInc = 1.0f / grainSamples; g->amp = 1.0f; g->active = true;
}

// process one stereo sample on tank gt IN PLACE (mono core), navkit processGranularDelay
static void grains_process(GrainTank *gt, float *mixL, float *mixR) {
    const float dt = 1.0f / (float)SOUND_SAMPLE_RATE;
    float input = (*mixL + *mixR) * 0.5f;
    if (!gt->freeze) {                                                 // write incoming + feedback into the capture ring
        float toWrite = input + gt->lastOut * gt->feedback;
        if (toWrite >  1.5f) toWrite =  1.5f;
        if (toWrite < -1.5f) toWrite = -1.5f;
        gt->buf[gt->writePos] = toWrite;
        gt->writePos = (gt->writePos + 1) % GRAIN_BUF_LEN;
    }
    gt->spawnTimer += dt;                                              // spawn grains on the density schedule
    float spawnInterval = 1.0f / gt->density;
    while (gt->spawnTimer >= spawnInterval) { gt->spawnTimer -= spawnInterval; _grain_spawn(gt); }
    float wet = 0.0f;                                                  // sum active grains, Hanning-windowed
    for (int i = 0; i < GRAIN_MAX_GRAINS; i++) {
        GrainVoice *g = &gt->grains[i];
        if (!g->active) continue;
        float env = 0.5f * (1.0f - cosf(g->envPhase * 6.2831853f));
        wet += moddel_hermite(gt->buf, GRAIN_BUF_LEN, g->readPos) * env * g->amp;
        g->readPos += g->posInc;
        if (g->readPos >= GRAIN_BUF_LEN) g->readPos -= GRAIN_BUF_LEN;
        g->envPhase += g->envInc;
        if (g->envPhase >= 1.0f) g->active = false;
    }
    float expectedOverlap = gt->density * (gt->grainSize / 1000.0f);   // normalize by expected overlap → steady level
    if (expectedOverlap > 1.0f) wet /= sqrtf(expectedOverlap);
    gt->lastOut = wet;
    *mixL = *mixL * (1.0f - gt->mix) + wet * gt->mix;
    *mixR = *mixR * (1.0f - gt->mix) + wet * gt->mix;
}

static void grains_reset(GrainTank *gt) {   // empty the capture buffer + kill grains (no pop on first enable / hot-reload)
    memset(gt->buf, 0, sizeof(gt->buf));
    memset(gt->grains, 0, sizeof(gt->grains));
    gt->writePos = 0; gt->spawnTimer = 0.0f; gt->lastOut = 0.0f;
}

static void fx_set_grains(int b, float grain_ms, float density, float position, float scatter, float feedback, float mix) {
    int t = grain_tank_for_bus(b);
    if (t < 0) return;                                                 // pool exhausted → bus stays dry
    GrainTank *gt = &grain_pool[t];
    if (grain_ms < 5.0f)   grain_ms = 5.0f;   if (grain_ms > 1000.0f) grain_ms = 1000.0f;
    if (density  < 1.0f)   density  = 1.0f;    if (density  > 100.0f)  density  = 100.0f;
    if (position < 0.0f)   position = 0.0f;    if (position > 1.0f)    position = 1.0f;
    if (scatter  < 0.0f)   scatter  = 0.0f;    if (scatter  > 1.0f)    scatter  = 1.0f;
    if (feedback < 0.0f)   feedback = 0.0f;    if (feedback > 0.95f)   feedback = 0.95f;
    if (mix      < 0.0f)   mix      = 0.0f;    if (mix      > 1.0f)    mix      = 1.0f;
    if (!gt->used && mix > 0.0f) grains_reset(gt);                     // first enable: clean buffer, no startup pop
    gt->grainSize = grain_ms; gt->density = density; gt->position = position;
    gt->scatter = scatter; gt->feedback = feedback; gt->mix = mix;
    gt->used = (mix > 0.0f);                                           // mix 0 = off, like the other inserts
}

static void fx_set_grains_freeze(int b, bool on) {   // live toggle — does NOT reconfigure DSP (set-and-hold safe)
    int t = grain_tank_of[b];
    if (t >= 0) grain_pool[t].freeze = on;
}

// ── reorderable insert chain (fx_order) ──────────────────────────────────────────────────────
// The inserts above (FX_* in studio.h) run in a default order; fx_order() lets a cart rearrange
// them PER BUS. fx_order[b] is the visit list; default = the canonical order, so an un-reordered
// bus is byte-identical to the old hardcoded ladder. Each step still gates on its _used[b] flag,
// so the default-order case is the same work as before.
#define N_PEDALS  (FX_CRUSH + 1)            // the 8 reorderable PEDALS (FX_TREM..FX_CRUSH) — the default chain
#define N_INSERTS (FX_GRAINS + 1)           // array size / max chain length: 8 pedals + FORMANT + FILTER + PAN + RINGMOD (default chain) + FX_REVERB (reverb-bus) + FX_ECHO + FX_GRAINS (placed via fx_order)
static int insert_order  [SOUND_FX_BUSES][N_INSERTS];   // per-bus visit list (kept distinct from the fx_order() API)
static int insert_order_n[SOUND_FX_BUSES];  // populated slot count (default = 8 pedals + formant + filter + pan + ringmod; FX_REVERB only on a reverb-bus)
// dispatch ONE insert by kind on bus b, in place. The _used[b] gate keeps dormant inserts free.
static void apply_insert(int kind, int b, float *L, float *R) {
    switch (kind) {
        case FX_TREM:    if (trem_used[b])   trem_process(b, L, R);    break;
        case FX_WAH:     if (wah_used[b])    wah_process(b, L, R);     break;
        case FX_CHORUS:  if (cho_used[b])    chorus_process(b, L, R);  break;
        case FX_PHASER:  if (phaser_used[b]) phaser_process(b, L, R);  break;
        case FX_FLANGER: if (flg_used[b])    flanger_process(b, L, R); break;
        case FX_TAPE:    if (tape_used[b])   tape_process(b, L, R);    break;
        case FX_EQ:      if (eq_used[b])     eq_process(b, L, R);      break;
        case FX_CRUSH:   if (crush_used[b])  crush_process(b, L, R);   break;
        case FX_FORMANT: if (fmt_used[b])    formant_process(b, L, R); break;
        case FX_FILTER:  if (filt_used[b])   filter_process(b, L, R);  break;
        case FX_PAN:     if (pan_used[b])    pan_process(b, L, R);     break;
        case FX_RINGMOD: if (rm_used[b])     rm_process(b, L, R);      break;
        case FX_ECHO:    if (echo_ins_used && b == 0) echo_ins_process(L, R); break;  // in-line delay, master-only (single buffer)
        case FX_GRAINS: { int t = grain_tank_of[b]; if (t >= 0 && grain_pool[t].used) grains_process(&grain_pool[t], L, R); } break;
        // FX_REVERB: wet-REPLACE on a reverb-bus (a bus fed only by sends, bus_tank[b] >= 0), so any
        // inserts AFTER it in the chain chew on the wet tail (reverb→bitcrush). On any other bus
        // (master / a pedalboard's bus, bus_tank[b] == -1) it's a no-op pass-through — never zeroes a real mix.
        case FX_REVERB: {
            int t = bus_tank[b];
            if (t >= 0 && rvb_tank[t].used) {
                float m = rvb_tank[t].mix;                                   // 1 = wet-replace (send-bus), <1 = in-line blend
                float wet = reverb_process(&rvb_tank[t], (*L + *R) * 0.5f);   // MONO core, v1
                *L = *L * (1.0f - m) + wet * m;   // m=1 → *L = wet, byte-identical to the old wet-replace
                *R = *R * (1.0f - m) + wet * m;
            }
        } break;
    }
}

// envelope-follower smoothing coefficient from a time in ms (one-pole; 0 ms = instant)
static float sound_follow_coef(int ms) {
    if (ms <= 0) return 1.0f;
    float c = 1.0f - expf(-1.0f / ((float)ms * 0.001f * (float)SOUND_SAMPLE_RATE));
    return c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
}

// Request kinds for the ring buffer (main thread pushes → audio thread drains).
// -1 in the `a` slot = "stop" for SR_SFX.
// delay_samples: 0 = fire immediately; >0 = audio thread holds it in `delayed[]` and fires when countdown expires.
// e0/e1/e2: extra payload (instrument attack/decay/release samples).
// ⚠ ORDERING SUBTLETY: define kinds (SR_INSTR..SR_INSTR_FILTER, SR_INSTR_ENV, SR_WAVE_SET)
// apply when DRAINED (next callback), but a delayed note (delay_samples > 0) snapshots its
// instrument slot AT FIRE TIME — so per-step instrument changes for scheduled notes need a
// ROTATING slot per pending step (see the sfx editor cart's CUT lane), or the last define
// wins for every queued note.
typedef enum {
    SR_SFX          = 0,
    /* 1 retired — music track, cut 2026-06, see decisions/ */
    SR_NOTE         = 2,
    SR_INSTR        = 3,
    SR_INSTR_DUTY   = 4,
    SR_INSTR_LFO    = 5,
    SR_INSTR_FILTER = 6,
    SR_NOTE_ON      = 7,
    SR_NOTE_OFF     = 8,
    SR_NOTE_PITCH   = 9,
    SR_NOTE_VOL     = 10,
    SR_NOTE_CUTOFF  = 11,
    SR_NOTE_DUTY    = 12,
    SR_NOTE_OFF_ALL = 13,
    SR_NOTE_RES     = 14,
    SR_NOTE_LFO     = 15,
    SR_NOTE_FILTER  = 16,
    SR_NOTE_GLIDE   = 17,
    SR_INSTR_ENV    = 18,
    SR_NOTE_ENV     = 19,
    SR_WAVE_SET     = 20,
    SR_INSTR_MACRO  = 21,
    SR_NOTE_MACRO   = 22,
    SR_INSTR_CHOKE  = 23,
    SR_INSTR_DRIVE  = 24,
    SR_NOTE_DRIVE   = 25,
    SR_ECHO         = 26,
    SR_INSTR_ECHO   = 27,
    SR_NOTE_ECHO    = 28,
    SR_INSTR_TUNE   = 29,
    SR_INSTR_FOLLOW = 30,
    SR_NOTE_FOLLOW  = 31,
    SR_VOICE_PARAM  = 32,   // EXPERIMENTAL INSTR_VOICE raw-param poke (voxlab): a=idx, b=val*1000
    SR_VOICE_CONS   = 33,   // EXPERIMENTAL INSTR_VOICE consonant onset (voxlab): a=consonant id (VC_*), -1 = none
    SR_VOICE_CODA   = 34,   // EXPERIMENTAL INSTR_VOICE consonant coda (voxlab): a=consonant id (VC_*), -1 = none
    SR_VOICE_NASAL  = 39,   // INSTR_VOICE nasal color (public voice_nasal): a=amount*1000 → vox_p[7] (35-38 = the pan block)
    SR_INSTR_PAN    = 35,   // a=slot, b=pan*1000 (signed) — per-instrument stereo position (stereo.md)
    SR_NOTE_PAN     = 36,   // a=pan*1000 (signed, live/slewed), e0/e1=handle — live pan on a held note
    SR_ENG_TUNE     = 37,   // EXPERIMENTAL guitar/piano tuning poke: a=slot, b=idx (0 weight·1 attack), c=val*1000
    SR_PAN_LAW      = 38,   // a=law (PAN_LINEAR/PAN_POWER) — master pan law (stereo.md); gated, default LINEAR
    SR_REVERB       = 40,   // a=size*1000, b=damping*1000 — configure THE reverb bus (40-42; 39 = voice_nasal above)
    SR_INSTR_REVERB = 41,   // a=slot, b=send*1000 — per-slot reverb send
    SR_NOTE_REVERB  = 42,   // a=val*1000 (live, slewed), e0/e1=handle — live reverb send on a held note
    SR_CHORUS       = 43,   // a=rate*1000, b=depth*1000, c=mix*1000 — configure THE master chorus (mod-delay buffer)
    SR_FLANGER      = 44,   // a=rate*1000, b=depth*1000, c=feedback*1000 (signed), e0=mix*1000 — THE master flanger
    SR_INSTR_CHORUS = 45,   // a=slot, b=rate*1000, c=depth*1000, e0=mix*1000 — chorus on one instrument (auto-bus)
    SR_INSTR_FLANGER= 46,   // a=slot, b=rate*1000, c=depth*1000, e0=fb*1000 (signed), e1=mix*1000 — flanger on one instrument (auto-bus)
    SR_TAPE         = 47,   // a=wow*1000, b=flutter*1000, c=sat*1000 — THE master tape (bus 0)
    SR_INSTR_TAPE   = 48,   // a=slot, b=wow*1000, c=flutter*1000, e0=sat*1000 — tape on one instrument (auto-bus)
    SR_WAH          = 49,   // a=sens*1000, b=res*1000, c=mix*1000 — THE master auto-wah (bus 0)
    SR_INSTR_WAH    = 50,   // a=slot, b=sens*1000, c=res*1000, e0=mix*1000 — auto-wah on one instrument (auto-bus)
    SR_INSTR_DRIVE_MODE = 51,  // a=slot, b=mode (DRIVE_*) — set a slot's drive waveshaper
    SR_NOTE_DRIVE_MODE  = 52,  // a=mode (DRIVE_*), e0/e1=handle — live waveshaper switch on a held note
    SR_BITCRUSH     = 53,   // a=bits*100, b=rate*100, c=mix*1000 — THE master bitcrush (bus 0)
    SR_INSTR_BITCRUSH = 54, // a=slot, b=bits*100, c=rate*100, e0=mix*1000 — bitcrush on one instrument (auto-bus)
    SR_EQ           = 55,   // a=low_db*1000, b=mid_db*1000, c=high_db*1000 — THE master 3-band EQ (bus 0)
    SR_INSTR_EQ     = 56,   // a=slot, b=low_db*1000, c=mid_db*1000, e0=high_db*1000 — EQ on one instrument (auto-bus)
    SR_WAH_LFO      = 57,   // a=rate*1000, b=res*1000, c=mix*1000 — THE master LFO-wah (bus 0, navkit WAH_MODE_LFO)
    SR_INSTR_WAH_LFO= 58,   // a=slot, b=rate*1000, c=res*1000, e0=mix*1000 — LFO-wah on one instrument (auto-bus)
    SR_TREMOLO      = 59,   // a=rate*1000, b=depth*1000, c=shape — THE master tremolo (bus 0, navkit processTremolo)
    SR_INSTR_TREMOLO= 60,   // a=slot, b=rate*1000, c=depth*1000, e0=shape — tremolo on one instrument (auto-bus)
    SR_PHASER       = 61,   // a=rate*1000, b=depth*1000, c=fb*1000(signed), e0=mix*1000, e1=stages — THE master phaser (bus 0, navkit processPhaser)
    SR_INSTR_PHASER = 62,   // a=slot, b=rate*1000, c=depth*1000, e0=fb*1000(signed), e1=mix*1000, e2=stages — phaser on one instrument (auto-bus)
    SR_FX_ORDER     = 63,   // a=bus, b=packed lo (slots 0..7, 4 bits each, FX_*), c=count, e0=packed hi (slots 8..) — set a bus's insert visit order
    SR_LESLIE       = 64,   // a=speed, b=drive*1000, c=balance*1000, e0=doppler*1000, e1=mix*1000 — THE master Leslie (bus 0, navkit processLeslie)
    SR_INSTR_LESLIE = 65,   // a=slot, b=speed, c=drive*1000, e0=balance*1000, e1=doppler*1000, e2=mix*1000 — Leslie on one instrument (auto-bus)
    SR_REVERB_BUS   = 66,   // a=tank(1..N-1), b=size*1000, c=damp*1000 — configure a reverb send-bus (claims an aux bus, chain = [FX_REVERB])
    SR_INSTR_REVERB_BUS = 67, // a=slot, b=tank, c=mix*1000 — route a slot's reverb send into tank N's bus instead of the master send
    SR_REVERB_BUS_FX = 68,  // a=tank, b=fx (FX_*), c/e0/e1 = params*1000 — add/configure an insert AFTER the reverb on tank N's bus
    SR_REVERB_INSERT = 69,  // a=size*1000, b=damp*1000, c=mix*1000 — reverb as a dry/wet-MIX INSERT on the master bus (in the fx_order chain)
    SR_FORMANT      = 70,   // a=vowel*1000, b=q*1000, c=mix*1000 — THE master formant/vowel filter (bus 0)
    SR_INSTR_FORMANT= 71,   // a=slot, b=vowel*1000, c=q*1000, e0=mix*1000 — formant filter on one instrument (auto-bus)
    SR_SIDECHAIN    = 72,   // a=victim_bus, b=key, c=amount*1000, e0=attack_ms, e1=release_ms — duck a bus on a trigger key
    SR_SIDECHAIN_KEY= 73,   // a=slot, b=key, c=send*1000 — route a slot into a sidechain trigger key
    SR_GLUE         = 74,   // a=victim_bus, b=amount*1000, c=attack_ms, e0=release_ms — bus comp (self-keyed, no trigger)
    SR_FILTER       = 75,   // a=mode, b=cutoff_hz, c=resonance*1000 — THE master resonant filter (DJ filter), bus 0
    SR_AUTOPAN      = 76,   // a=rate*1000, b=depth*1000, c=shape — THE master auto-pan (bus 0, antiphase tremolo)
    SR_INSTR_AUTOPAN= 77,   // a=slot, b=rate*1000, c=depth*1000, e0=shape — auto-pan on one instrument (auto-bus)
    SR_RINGMOD      = 78,   // a=freq_hz, b=mix*1000 — THE master ring modulator (bus 0)
    SR_INSTR_RINGMOD= 79,   // a=slot, b=freq_hz, c=mix*1000 — ring mod on one instrument (auto-bus)
    SR_ECHO_INSERT  = 80,   // a=time_ms, b=fb*1000, c=tone*1000, e0=mix*1000 — echo as a dry/wet INSERT on the master bus (in the fx_order chain)
    SR_GRAINS       = 81,   // a=grain_ms, b=density*100, c=position*1000, e0=scatter*1000, e1=feedback*1000, e2=mix*1000 — THE master granular delay (bus 0)
    SR_INSTR_GRAINS = 82,   // a=slot, b=grain_ms, c=density*100, e0=position*1000, e1=scatter*1000, e2=PACK(feedback*100·*1001 + mix*1000) — granular on one instrument (auto-bus)
    SR_GRAINS_FREEZE       = 83,   // a=on (0/1) — freeze the master granular capture buffer (live toggle, no DSP reconfigure)
    SR_INSTR_GRAINS_FREEZE = 84,   // a=slot, b=on (0/1) — freeze one instrument's granular buffer
} SoundReqKind;
typedef struct { SoundReqKind kind; int a, b, c; int delay_samples; int dur_samples; int e0, e1, e2; } SoundReq;
#define SOUND_REQ_QUEUE   512   // generous: live held-voice control pushes many setters/frame, and a patch cart's
                                // init() can define dozens of slots + several wave_set tables in one burst
#define SOUND_DELAYED_MAX 64    // pending delayed notes (strum/schedule/schedule_hit) — fast sfx steps queue several ahead

static SoundReq      req_queue[SOUND_REQ_QUEUE];
// Lock-free SPSC ring indices. The main (game) thread is the sole producer (advances
// head); the audio thread is the sole consumer (advances tail). Atomic acquire/release
// gives the cross-thread memory ordering a REAL audio thread needs — native today, the
// web AudioWorklet later — so the consumer sees a fully-written entry before it sees the
// advanced index. (`volatile` alone is NOT a barrier.) See design/audio-threading.md.
static atomic_int    req_head = 0;   // produced by the main thread
static atomic_int    req_tail = 0;   // consumed by the audio thread

// audio-thread-owned holding pen for delayed requests (e.g. strum)
static SoundReq      delayed[SOUND_DELAYED_MAX];
static int           delayed_count = 0;

// tripwire: how many requests were silently dropped because a buffer was full. A nonzero
// count means sound calls were LOST (a wave_set flood, an init() define burst, …) — exactly
// the silent class of bug that once made every wav-knob position play the default square.
// sound_tick() screams about it via printh so it shows in the editor log / bake output.
static atomic_int sound_dropped = 0;   // incremented from BOTH threads → atomic RMW

#if defined(PLATFORM_WEB) && defined(DE_AUDIO_WORKLET)
// Master gain for the worklet mixer. raylib's SetMasterVolume only affects the
// ScriptProcessor device we bypass, so the pause overlay routes its mute through this
// instead (set on the main thread, read on the audio thread). See audio-threading.md §4.
static _Atomic float sound_master_gain = 1.0f;
static void sound_set_master_gain(float g) {
    atomic_store_explicit(&sound_master_gain, g, memory_order_relaxed);
}
#endif

// musical clock (main-thread state, ticked once per frame from studio.c)
static int   sound_bpm     = 120;
static float beat_accum    = 0.0f;
static int   beat_now      = 0;
static bool  beat_just_advanced = false;

// called once per frame from studio.c, before update()
static void sound_tick(float dt) {
    beat_accum += dt * (sound_bpm / 60.0f);
    int new_beat = (int)beat_accum;
    beat_just_advanced = (new_beat > beat_now);
    beat_now = new_beat;

    // scream when the tripwire fires: dropped requests mean sound calls were LOST
    // (notes that never play, instrument defines that never land). Shows in the
    // editor's log panel and in bake/play.js output — fail loud, not silent.
    static int dropped_reported = 0;
    int dropped = atomic_load_explicit(&sound_dropped, memory_order_relaxed);
    if (dropped > dropped_reported) {
        printh("[sound] WARNING: request queue overflow — %d sound call(s) DROPPED (notes/defines lost). Spread big bursts across frames or report this.", dropped);
        dropped_reported = dropped;
    }
}

// dur_samples: 0 = use default 250ms (for note/schedule); >0 = custom note length (for hit).
static void sound_push_req(SoundReqKind kind, int a, int b, int c, int delay_samples, int dur_samples) {
    int h = atomic_load_explicit(&req_head, memory_order_relaxed);   // producer owns head
    int next = (h + 1) % SOUND_REQ_QUEUE;
    if (next == atomic_load_explicit(&req_tail, memory_order_acquire)) {   // full — drop (trip the wire)
        atomic_fetch_add_explicit(&sound_dropped, 1, memory_order_relaxed); return;
    }
    req_queue[h].kind          = kind;
    req_queue[h].a             = a;
    req_queue[h].b             = b;
    req_queue[h].c             = c;
    req_queue[h].delay_samples = delay_samples;
    req_queue[h].dur_samples   = dur_samples;
    req_queue[h].e0 = req_queue[h].e1 = req_queue[h].e2 = 0;
    atomic_store_explicit(&req_head, next, memory_order_release);   // publish — entry writes happen-before the head advance
}

// Push a control request carrying the extra e0/e1/e2 payload (used by instrument()).
static void sound_push_ctrl(SoundReqKind kind, int a, int b, int c, int e0, int e1, int e2) {
    int h = atomic_load_explicit(&req_head, memory_order_relaxed);   // producer owns head
    int next = (h + 1) % SOUND_REQ_QUEUE;
    if (next == atomic_load_explicit(&req_tail, memory_order_acquire)) {   // full — drop (trip the wire)
        atomic_fetch_add_explicit(&sound_dropped, 1, memory_order_relaxed); return;
    }
    req_queue[h] = (SoundReq){ .kind = kind, .a = a, .b = b, .c = c,
                               .delay_samples = 0, .dur_samples = 0,
                               .e0 = e0, .e1 = e1, .e2 = e2 };
    atomic_store_explicit(&req_head, next, memory_order_release);   // publish — entry writes happen-before the head advance
}

// ───────── helpers ─────────

static inline float sound_midi_to_freq(int midi) {
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

static inline float sound_midi_to_freq_f(float midi) {
    return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f);
}

static inline float sound_osc(int wave, float phase, float duty, int *noise_state) {
    switch (wave) {
    case INSTR_SQUARE: return phase < duty ? 0.5f : -0.5f;
    case INSTR_SAW:    return phase * 2.0f - 1.0f;
    case INSTR_TRI:    return phase < 0.5f ? phase * 4.0f - 1.0f : 3.0f - phase * 4.0f;
    case INSTR_NOISE: {
        *noise_state = (*noise_state * 1103515245 + 12345) & 0x7fffffff;
        return ((*noise_state >> 16) & 0xff) / 127.5f - 1.0f;
    }
    case INSTR_SINE:   return sinf(phase * 6.2831853f);
    case INSTR_USER0: case INSTR_USER1: case INSTR_USER2: case INSTR_USER3: {
        // custom drawn single-cycle table, linear-interpolated (wave_set fills it)
        const float *w = user_wave[wave - INSTR_USER0];
        float fp = phase * (float)SOUND_WAVE_LEN;
        int   i0 = (int)fp & (SOUND_WAVE_LEN - 1), i1 = (i0 + 1) & (SOUND_WAVE_LEN - 1);
        float fr = fp - (float)(int)fp;
        return w[i0] + (w[i1] - w[i0]) * fr;
    }
    }
    return 0.0f;
}

// ───────── modeled engines (wave ids >= INSTR_ENGINE_BASE) ─────────
// An engine is a stateful per-note simulation, not a wavetable — sound_osc never sees these
// ids. Every engine reads the same three macros (v->harm / v->timb / v->mor); the mapping
// from macro to internal parameter is the engine's taste, curated here, never exposed as API.

// PLUCK note-on: excite the string. Karplus-Strong = a delay line excited with a noise
// burst, recirculated through a damping average (sound_engine_sample). The macros shape
// the EXCITATION here (timbre/morph) and the FEEDBACK there (harmonics). The buffer is
// allocated at 2x the note period (capped) and the excitation TILED across it, so the
// fractional read tap can swing a full octave downward without leaving valid signal.
static void sound_pluck_start(Voice *v) {
    int period = (int)((float)SOUND_SAMPLE_RATE / (v->freq > 20.0f ? v->freq : 20.0f));
    if (period < 2) period = 2;
    if (period > SOUND_KS_MAX) period = SOUND_KS_MAX;
    int alloc = period * 2 + 4;
    if (alloc > SOUND_KS_MAX) alloc = SOUND_KS_MAX;
    v->ks_len  = alloc;
    v->ks_widx = 0;
    v->ks_last = 0.0f;
    // timbre = pick brightness: one-pole lowpass over the noise burst
    // (0 = soft felt thud, 1 = hard pick, full spectrum)
    float k  = 0.04f + 0.96f * v->timb * v->timb;
    float lp = 0.0f;
    for (int i = 0; i < period; i++) {
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float n = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
        lp += k * (n - lp);
        v->ks_buf[i] = lp;
    }
    // morph = pick position: comb-filter the excitation (subtract a shifted copy) — notches
    // the harmonics a real pluck point cancels. 0 = near the bridge (full), 1 = mid-string (hollow)
    int pos = (int)(period * (0.04f + 0.46f * v->mor));
    if (pos > 0) {
        float tmp[SOUND_KS_MAX];
        memcpy(tmp, v->ks_buf, period * sizeof(float));
        for (int i = 0; i < period; i++) v->ks_buf[i] = tmp[i] - 0.55f * tmp[(i + pos) % period];
    }
    // remove DC (the feedback loop would sustain it forever) + normalize so every
    // macro setting plucks at the same loudness
    float mean = 0.0f;
    for (int i = 0; i < period; i++) mean += v->ks_buf[i];
    mean /= (float)period;
    float peak = 0.0f;
    for (int i = 0; i < period; i++) {
        v->ks_buf[i] -= mean;
        float a = fabsf(v->ks_buf[i]);
        if (a > peak) peak = a;
    }
    if (peak > 0.0001f) {
        float g = 0.9f / peak;
        for (int i = 0; i < period; i++) v->ks_buf[i] *= g;
    }
    for (int i = period; i < alloc; i++) v->ks_buf[i] = v->ks_buf[i % period];   // tile
}

// MALLET note-on: strike the bar. The voice's four modes get their initial energy here;
// sound_mallet_sample then rings them down. Macros at the strike:
//   timbre = MALLET HARDNESS — how much energy lands in the upper modes (0 = soft yarn,
//            almost pure fundamental; 1 = hard brass, full spectrum) + the contact click.
//   harmonics picks the BAR's amp profile (wood = fundamental-heavy, bell = partial-rich);
//            the partial RATIOS it also controls are read live in the sample fn (so
//            note_harmonics bends a ringing bar's partials — the drone trick).
// The strike is normalized so every macro setting hits at the same loudness (pluck lesson).
static void sound_mallet_start(Voice *v) {
    // wood bar (marimba, fundamental-dominant) → bell/metal (partial-rich) amp profiles,
    // numbers from navkit's measured presets (marimba / glockenspiel)
    static const float AW[4] = { 1.0f, 0.25f, 0.08f, 0.02f };
    static const float AB[4] = { 1.0f, 0.55f, 0.35f, 0.20f };
    float sum = 0.0f;
    for (int m = 0; m < 4; m++) {
        v->md_phase[m] = 0.0f;
        float base = AW[m] + (AB[m] - AW[m]) * v->harm;
        // hardness: soft mallets can't excite the high modes. pow-shaped so the knob's
        // bottom is a real felt thud, not just "slightly darker" (perceptual, §8.8.1)
        float soft = 0.12f + 0.88f * v->timb;
        float hw   = 1.0f;
        for (int k = 0; k < m; k++) hw *= soft;     // mode m scaled soft^m
        v->md_amp[m] = base * hw;
        sum += v->md_amp[m];
    }
    if (sum > 0.0001f) {
        float g = 1.0f / sum;                        // equal-loudness strike across the knobs
        for (int m = 0; m < 4; m++) v->md_amp[m] *= g;
    }
    v->md_trem_ph = 0.0f;
    // the contact click: ~1.5ms of noise, only audible with a hard mallet
    v->md_strike  = (int)(0.0015f * (float)SOUND_SAMPLE_RATE);
    v->md_on      = true;
}

// One MALLET sample: sum the four modes, decay them, add the strike click and the motor.
// Live macros: harmonics sweeps the partial RATIOS (tuned wood octaves → inharmonic
// ideal-bar clang), morph sets the RING as a perceptual T60 (≈0.09s dry tick → ≈8s vibe
// sustain, exponential so every quarter-turn is an audible step — the §8.8.1 rule) and
// fades the vibraphone motor tremolo in over its top third.
static inline float sound_mallet_sample(Voice *v, float pitch_mul) {
    if (!v->md_on) return 0.0f;                       // engine id without a note-on init
    const float dt = 1.0f / (float)SOUND_SAMPLE_RATE;
    // tuned-bar ratios (marimba undercut, octave-aligned) → ideal-bar ratios (inharmonic
    // metal clang). Sweeping harm GLISSES the upper partials between them.
    static const float RW[4] = { 1.0f, 4.0f, 10.0f, 20.0f };
    static const float RB[4] = { 1.0f, 2.756f, 5.404f, 8.933f };
    float f = v->freq * pitch_mul;
    if (f < 20.0f) f = 20.0f;
    float t60  = 0.09f * expf(v->mor * 4.5f);         // 0.09s → ~8.1s to -60dB
    float rate = 6.9078f / t60;                       // amp *= (1 - rate·dt) ≈ e^-6.9 at t60
    float out  = 0.0f;
    for (int m = 0; m < 4; m++) {
        float amp = v->md_amp[m];
        if (amp < 0.00002f) continue;
        float ratio = RW[m] + (RB[m] - RW[m]) * v->harm;
        float mf = f * ratio;
        // upper modes ring shorter than the fundamental (what makes it read as a struck bar)
        v->md_amp[m] = amp - amp * rate * (1.0f + 1.6f * (float)m) * dt;
        if (mf >= (float)SOUND_SAMPLE_RATE * 0.45f) continue;   // above Nyquist: decays silently
        v->md_phase[m] += mf * dt;
        if (v->md_phase[m] >= 1.0f) v->md_phase[m] -= 1.0f;
        out += sinf(v->md_phase[m] * 6.2831853f) * amp;
    }
    if (v->md_strike > 0) {                           // the mallet contact click (hardness-gated)
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float n = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
        out += n * 0.22f * v->timb * v->timb * ((float)v->md_strike * dt / 0.0015f);
        v->md_strike--;
    }
    float motor = (v->mor - 0.65f) / 0.35f;           // morph's top third = the vibe motor
    if (motor > 0.0f) {
        if (motor > 1.0f) motor = 1.0f;
        v->md_trem_ph += 5.2f * dt;                   // ~5Hz, the classic vibraphone rate
        if (v->md_trem_ph >= 1.0f) v->md_trem_ph -= 1.0f;
        out *= 1.0f - 0.45f * motor * (0.5f + 0.5f * sinf(v->md_trem_ph * 6.2831853f));
    }
    return out * 0.9f;
}

// One FM sample (2-op + feedback — §8.8.3 in audio-notes). The carrier is v->phase, which
// the mix loop already advances by freq*pitch_mul — so the whole pitch machinery works by
// construction; only the modulator phase lives here. The second oscillator is INAUDIBLE
// (it bends the carrier's phase) — this is NOT the deferred second-audible-osc plumbing.
static inline float sound_fm_sample(Voice *v, float pitch_mul) {
    // harmonics = carrier:modulator ratio, SNAPPED to a curated table — a continuous ratio
    // is out-of-tune clang everywhere except the integers. Each detent is a different
    // instrument: integers = harmonic (bass/epiano/brass), offs = bells/clang, 14 = DX tine.
    static const float RATIO[10] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 3.5f, 4.0f, 5.0f, 7.0f, 14.0f };
    int ri = (int)(v->harm * 9.999f);
    if (ri < 0) ri = 0; else if (ri > 9) ri = 9;
    float f = v->freq * pitch_mul;
    // timbre = peak mod index (brightness, beta in radians). The index DECAYS within the
    // note toward a 25% floor over ~0.9s — the DX strike-then-mellow signature; a static
    // index reads as cheap organ. Decay derives from step_samples (retriggers per note).
    float beta = v->timb * v->timb * 12.0f
               * (0.25f + 0.75f * expf(-(float)v->step_samples / (0.9f * (float)SOUND_SAMPLE_RATE)));
    // brightness follows the amp ATTACK: a slow-attack slot swells into brightness — FM
    // brass speaks instead of arriving pre-brightened (the §8.8.3 brass answer). Instant
    // attacks (epiano/bell/bass) are byte-identical to before.
    if (v->a_samp > 0 && v->step_samples < v->a_samp)
        beta *= (float)v->step_samples / (float)v->a_samp;
    if (f * RATIO[ri] >= (float)SOUND_SAMPLE_RATE * 0.45f) beta = 0.0f;   // mod above Nyquist → pure sine
    // morph = modulator feedback: 0 = pure two-sine FM, up = the modulator self-saturates
    // toward saw → growl → noisy clang at the top (useful percussion territory)
    float m = sinf(v->fm_mph * 6.2831853f + v->mor * 1.3f * v->fm_fb * 3.14159265f);
    v->fm_fb = m;
    v->fm_mph += f * RATIO[ri] / (float)SOUND_SAMPLE_RATE;
    if (v->fm_mph >= 1.0f) v->fm_mph -= 1.0f;
    float out = sinf(v->phase * 6.2831853f + m * beta);
    // the DX TINE: the E.PIANO 1 attack bell — a quiet 14x ping, ear-verdict driven
    // (audio-notes §8.5 phase 3: "close but not exactly DX Rhodes"). Triple-contained so
    // it can't leak into other presets: it only exists on the 1:1 detent, it dies in
    // ~75ms, and feedback fades it out (growly brass barely hears it). Scaled by timbre —
    // a soft strike has no tine, just like the hardware.
    if (ri == 1) {
        float td = expf(-(float)v->step_samples / (0.025f * (float)SOUND_SAMPLE_RATE));
        float tf = f * 14.0f;
        if (td > 0.002f && tf < (float)SOUND_SAMPLE_RATE * 0.45f) {
            v->fm_tph += tf / (float)SOUND_SAMPLE_RATE;
            if (v->fm_tph >= 1.0f) v->fm_tph -= 1.0f;
            float tm = 1.0f - v->mor; if (tm < 0.0f) tm = 0.0f;
            out += sinf(v->fm_tph * 6.2831853f) * 0.18f * v->timb * tm * td;
        }
    }
    return out;
}

// One PD (Casio CZ phase-distortion) sample — buffer-free, NO note-on init (phase rides
// v->phase like FM; the only per-note motion is the DCW envelope, which derives from
// step_samples — the FM beta-decay trick — so it needs zero Voice state). Design + the STEP 0
// render findings: docs/design/instrument-engines.md §8.8.6.
static inline float sound_pd_sample(Voice *v, float pitch_mul) {
    float phase = v->phase;                 // 0..1, advanced by the mix loop like any wave
    // (literals inline below — PI and TAU are raylib macros, so don't declare locals named that)
    // harmonics = wavetype, SNAPPED to 8 CZ detents (the FM ratio-table pattern; never
    // crossfaded — the formulas are discontinuous). 0..4 phase-warp family, 5..7 resonant.
    int wt = (int)(v->harm * 7.999f);
    if (wt < 0) wt = 0; else if (wt > 7) wt = 7;
    // timbre = static distortion (the DCW amount). morph = DCW-ENVELOPE depth: distortion
    // snaps toward 1 on the strike and settles to the timbre value over ~0.25s — the CZ
    // "wowww" navkit omits. morph 0 = static (navkit-flat); 1 = full sweep.
    float d_sus = v->timb;
    float dcw   = expf(-(float)v->step_samples / (0.25f * (float)SOUND_SAMPLE_RATE));
    float d = d_sus + (1.0f - d_sus) * v->mor * dcw;
    if (d > 0.999f) d = 0.999f; else if (d < 0.0f) d = 0.0f;
    float out = 0.0f;
    switch (wt) {
        case 0: {  // SAW: compress first half of the phase ramp, stretch the second
            float dp;
            if (phase < 0.5f) dp = phase * (1.0f + d);
            else { float t = (phase - 0.5f) * 2.0f; dp = 0.5f * (1.0f + d) + t * (1.0f - 0.5f * (1.0f + d)); }
            if (dp < 0.0f) dp = 0.0f; else if (dp > 1.0f) dp = 1.0f;
            out = cosf(dp * 3.14159265f);
            break;
        }
        case 1: {  // SQUARE: sharpen the transitions at 0.25 / 0.75
            float sh = 0.5f - d * 0.45f, dp;
            if      (phase < 0.25f) dp = phase / 0.25f * sh;
            else if (phase < 0.5f)  dp = sh + (phase - 0.25f) / 0.25f * (0.5f - sh);
            else if (phase < 0.75f) dp = 0.5f + (phase - 0.5f) / 0.25f * sh;
            else                    dp = 0.5f + sh + (phase - 0.75f) / 0.25f * (0.5f - sh);
            out = cosf(dp * 6.2831853f);
            break;
        }
        case 2: {  // PULSE: compress the active portion to a narrow pulse
            float w = 0.5f - d * 0.45f, dp;
            if (phase < w) dp = phase / w * 0.5f;
            else           dp = 0.5f + (phase - w) / (1.0f - w) * 0.5f;
            out = cosf(dp * 6.2831853f);
            break;
        }
        case 3: {  // DOUBLEPULSE: two peaks per cycle (sync-like, octave-up flavour)
            float dp = phase * 2.0f; if (dp >= 1.0f) dp -= 1.0f;
            float w = 0.5f - d * 0.4f;
            if (dp < w) dp = dp / w * 0.5f;
            else        dp = 0.5f + (dp - w) / (1.0f - w) * 0.5f;
            out = cosf(dp * 6.2831853f);
            break;
        }
        case 4: {  // SAWPULSE: saw + pulse blend
            float dp1;
            if (phase < 0.5f) dp1 = phase * (1.0f + d * 0.5f);
            else              dp1 = 0.5f * (1.0f + d * 0.5f) + (phase - 0.5f) * (1.0f - d * 0.25f);
            if (dp1 < 0.0f) dp1 = 0.0f; else if (dp1 > 1.0f) dp1 = 1.0f;
            float saw = cosf(dp1 * 3.14159265f);
            float w = 0.5f - d * 0.3f, dp2;
            if (phase < w) dp2 = phase / w * 0.5f;
            else           dp2 = 0.5f + (phase - w) / (1.0f - w) * 0.5f;
            out = (saw + cosf(dp2 * 6.2831853f)) * 0.5f;
            break;
        }
        default: { // 5/6/7 RESONANT: a window gates a cosine at the resonant peak (1 + d·7×).
            // STEP 0 finding (§8.8.6): the raw d·7 peak is an icepick at high notes (brightness
            // 0.022 at C3 → 0.938 at C6). Scale the multiplier DOWN as the note rises, EP-style.
            float f  = v->freq * pitch_mul;
            float fn = (f - 80.0f) / 1200.0f; if (fn < 0.0f) fn = 0.0f; else if (fn > 1.0f) fn = 1.0f;
            float resoFreq = 1.0f + d * 7.0f * (1.0f - fn * 0.7f);
            float window;
            if (wt == 5)      window = 1.0f - fabsf(2.0f * phase - 1.0f);                 // RESO1 triangle
            else if (wt == 6) window = (phase < 0.25f) ? phase * 4.0f                     // RESO2 trapezoid
                                     : (phase < 0.75f) ? 1.0f : (1.0f - phase) * 4.0f;
            else              window = 1.0f - phase;                                      // RESO3 saw (classic CZ)
            out = window * cosf(phase * resoFreq * 6.2831853f);
            break;
        }
    }
    return out;
}

// ORGAN note-on: organ tone is continuous (no struck excitation like pluck/mallet) — the
// drawbar sines simply start sounding. We only ARM the two attack transients here: the key
// click (rides timbre — a bright patch clicks harder) and the percussion ping (rides morph's
// top end — a lively B3 chips, a still combo organ doesn't), and clear the borrowed scanner
// tail so stale Karplus data can't click through.
static void sound_organ_start(Voice *v) {
    for (int i = 0; i < 9; i++) v->org_ph[i] = (float)i / 9.0f;   // spread phases: no coherent attack spike
    for (int i = 0; i < ORGAN_SCAN; i++) v->ks_buf[i] = 0.0f;     // clear borrowed scanner delay
    v->org_widx    = 0;
    v->org_scan_ph = 0.0f;
    v->org_lp      = 0.0f;
    v->org_click   = v->timb;                                     // brightness drives the click bite (§8.8.4)
    float perc = (v->mor - 0.55f) / 0.45f;                        // percussion fades in over morph's top ~45%
    v->org_perc    = perc < 0.0f ? 0.0f : (perc > 1.0f ? 1.0f : perc);
    v->org_perc_ph = 0.0f;
    v->org_on      = true;
}

// One ORGAN sample: 9 additive drawbar sines at the Hammond footage ratios, picked as a
// SNAPPED registration (harmonics — like FM's ratio table, each detent a different recipe),
// tilted bright/dark (timbre), then animated by the scanner chorus + percussion (morph).
// Pitch: every drawbar derives from freq*pitch_mul per sample, so vibrato/glide/pitch-env
// bend the whole stack together (§8.8.1). The scanner delay borrows ks_buf[0..ORGAN_SCAN).
static inline float sound_organ_sample(Voice *v, float pitch_mul) {
    if (!v->org_on) return 0.0f;                       // engine id without a note-on init
    const float dt = 1.0f / (float)SOUND_SAMPLE_RATE;
    static const float RAT[9] = { 0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f };       // 16'..1' footages
    static const float OCT[9] = { -1.0f, 0.585f, 0.0f, 1.0f, 1.585f, 2.0f, 2.322f, 2.585f, 3.0f }; // log2(ratio), precomputed
    // harmonics = registration, SNAPPED to a curated table of iconic recipes (thin → full).
    static const float REG[8][9] = {
        { 0.0f, 0.0f, 0.75f, 0.0f, 0.75f, 0.0f, 0.0f, 0.0f, 0.0f }, // reggae bubble — hollow upstroke
        { 0.0f, 0.0f, 0.75f, 0.75f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f }, // soft combo — cocktail
        { 1.0f, 1.0f, 0.75f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },  // booker t — 60s clean
        { 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // jimmy smith — fat jazz B3
        { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },   // larry young — modern jazz
        { 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f },   // ballad — sub+fund+sparkle
        { 1.0f, 1.0f, 1.0f, 0.75f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },  // jon lord — rock growl
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },   // gospel full — all bars out
    };
    int ri = (int)(v->harm * 7.999f);
    if (ri < 0) ri = 0; else if (ri > 7) ri = 7;
    const float *reg = REG[ri];
    float f = v->freq * pitch_mul;
    if (f < 20.0f) f = 20.0f;
    // timbre = brightness tilt across the drawbars (a spectral tilt by octave) + click bite
    float tilt = (v->timb - 0.5f) * 2.0f;              // -1 dark .. +1 bright
    float nyq  = (float)SOUND_SAMPLE_RATE * 0.45f;
    float dry = 0.0f, ampSum = 0.0f;
    for (int i = 0; i < 9; i++) {
        float lvl = reg[i];
        if (lvl < 0.001f) continue;
        float w = 1.0f + tilt * (OCT[i] - 1.2f) * 0.45f;
        if (w < 0.0f) w = 0.0f;
        lvl *= w;
        float df = f * RAT[i];
        if (df >= nyq) continue;                        // above Nyquist: drop this drawbar
        v->org_ph[i] += df * dt;
        if (v->org_ph[i] >= 1.0f) v->org_ph[i] -= 1.0f;
        dry += sinf(v->org_ph[i] * 6.2831853f) * lvl;
        ampSum += lvl;
    }
    if (ampSum > 0.0001f) dry /= ampSum;                // equal loudness across registration + tilt (§8.8.1)
    if (v->org_click > 0.0001f) {                       // key click: a short noise burst (~3ms)
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float n = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
        dry += n * v->org_click * 0.35f;
        v->org_click *= 1.0f - dt / 0.003f;
        if (v->org_click < 0.0001f) v->org_click = 0.0f;
    }
    if (v->org_perc > 0.0001f) {                        // percussion: a 2nd-harmonic ping, fast decay
        v->org_perc_ph += f * 2.0f * dt;
        if (v->org_perc_ph >= 1.0f) v->org_perc_ph -= 1.0f;
        dry += sinf(v->org_perc_ph * 6.2831853f) * v->org_perc * 0.4f;
        v->org_perc *= 1.0f - dt / 0.2f;                // ~200ms
        if (v->org_perc < 0.0001f) v->org_perc = 0.0f;
    }
    // morph = animation: the scanner CHORUS (C-mode, dry+wet) deepens with morph; 0 = a
    // stone-still combo organ. The 6.9Hz scanner LFO sweeps a fractional tap in the borrowed
    // ks_buf head — the comb shimmer that defines the gospel/jazz B3 (audio-notes §8.8.4).
    float out;
    float depth = v->mor * 30.0f;                       // up to ~0.7ms excursion (navkit C3)
    if (depth < 0.5f) {
        out = dry;                                      // still: dry only, no scanner
    } else {
        v->ks_buf[v->org_widx] = dry;
        v->org_scan_ph += 6.9f * dt;
        if (v->org_scan_ph >= 1.0f) v->org_scan_ph -= 1.0f;
        float lfo = sinf(v->org_scan_ph * 6.2831853f);
        float rp  = (float)v->org_widx - (32.0f + lfo * depth);  // tap centered 32 behind the write head
        if (rp < 0.0f) rp += (float)ORGAN_SCAN;
        int   r0 = (int)rp;
        float fr = rp - (float)r0;
        int   r1 = r0 + 1; if (r1 >= ORGAN_SCAN) r1 = 0;
        float wet = v->ks_buf[r0] + (v->ks_buf[r1] - v->ks_buf[r0]) * fr;
        v->org_widx++; if (v->org_widx >= ORGAN_SCAN) v->org_widx = 0;
        out = 0.5f * dry + 0.5f * wet;
    }
    // driven organ rolls off its top BEFORE the voice's tanh drive (a cranked amp/Leslie loses
    // highs). Without this, saturating a sparse bright registration — ballad's lone 1' sparkle
    // over a big gap — makes harsh intermodulation fizz; the rolloff keeps it a smooth growl.
    // Clean (drv=0) is bit-identical: the filter is bypassed entirely.
    if (v->drv > 0.01f) {
        float fc   = 6000.0f - v->drv * 4200.0f;        // ~6kHz light .. ~1.8kHz cranked
        float coef = 1.0f - expf(-6.2831853f * fc / (float)SOUND_SAMPLE_RATE);
        v->org_lp += (out - v->org_lp) * coef;
        out = v->org_lp;
    }
    return out * 0.9f;
}

// Rhodes per-mode decay (s). Rebuilt 2026-06-08 from measured spectra (Shear 2011 UCSB thesis
// §2.1.2/Fig 2.2-2.3; Münster & Pfeifle JASA 148(5) 2020): a real Rhodes tine settles into near
// SIMPLE HARMONIC motion — fundamental + INTEGER harmonics 2,3,4,5 (2nd often loudest, made by
// the nonlinear pickup), the upper ones rolling off faster. The cantilever's genuine INHARMONIC
// modes (6.27×, 17.55×, 34.4× — a clamped-free bar's 1:6.27:17.55 series) are SHORT-LIVED: a
// fast attack DING, gone in ~0.1s. The old model had this inside-out (a loud sustained 4.2×
// inharmonic partial = an "untuned bell"); now the harmonics are the body and the bell is sparse
// + fast. Indices: 0-5 = harmonics 1..6, 6 = 6.27× bell, 7-9 = harmonics 8/10/12, 10-11 = 17.55×/
// 34.4× bell. (audio-notes §8.8.5.)
static const float DEC_R[12] = {
    0.95f, 0.80f, 0.55f, 0.50f, 0.40f, 0.38f,   // harmonics 1,2,3,4,5,6 — body → mid sustain
    0.12f,                                        // 6.27× tine bell — fast attack ding
    0.30f, 0.22f, 0.18f,                          // harmonics 8,10,12 — bright, shorter
    0.12f, 0.10f,                                 // 17.55×, 34.4× tine bells — fast
};

// EP note-on: build the 12 modal sines from the instrument detent (harmonics), brightness
// (timbre = pickup position), register, and strike level. The modes then ring down (struck,
// self-decaying — mallet family); the sample fn sums them, runs the pickup nonlinearity
// (morph = bark, read live) and a DC blocker. Rhodes row measured + Wurli row octave-tuned (see
// the RAT note); Clav row still a navkit crib (initEPianoSettings + the tables).
static void sound_epiano_start(Voice *v) {
    // 12 mode ratios per instrument. Rhodes (row 0) = harmonics 1-6 + 6.27× bell + harmonics
    // 8/10/12 + 17.55×/34.4× bells (measured — see DEC_R note). Wurli (row 1) = full harmonic
    // series with boosted OCTAVE partials (2,4,8,16) riding over the reedy 3rd — the 200A's
    // fuller/punchier "secret" (Reed200 spectral-model note); the symmetric pickup adds the odd
    // bark live. Clav (row 2) = near-harmonic struck string w/ stiffness stretch — navkit crib
    // (synth_oscillators.h:3675).
    static const float RAT[3][12] = {
        { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 6.27f, 8.0f, 10.0f, 12.0f, 17.55f, 34.4f },
        { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 11.0f, 13.0f, 16.0f },
        { 1.0f, 2.003f, 3.012f, 4.028f, 5.15f, 6.35f, 7.6f, 8.9f, 10.2f, 11.6f, 13.0f, 14.5f },
    };
    // amp profiles, centered (mellow) -> offset (bright); timbre crossfades them (:3747).
    // Rhodes row: mellow = fundamental-dominant, steep rolloff; bright = 2nd harmonic dominant
    // with EVEN partials (2,4,6 → idx 1,3,5) over ODD (1,3,5 → idx 0,2,4) — the "voicing" effect
    // (Shear §2.2.2: tine near pickup axis attenuates fundamental + odd partials, 2nd dominates).
    static const float AC[3][12] = {
        { 1.0f, .28f, .10f, .05f, .03f, .015f, .10f, .008f, .005f, .003f, .04f, .01f },
        { 1.0f, .35f, .45f, .20f, .12f, .06f, .08f, .12f, .04f, .03f, .02f, .04f },   // Wurli: 2nd(octave) up near the reedy 3rd
        { 1.0f, .30f, .20f, .35f, .15f, .06f, 0,0,0,0,0,0 },
    };
    static const float AO[3][12] = {
        { .55f, 1.0f, .32f, .42f, .14f, .22f, .30f, .10f, .06f, .04f, .16f, .05f },
        { .60f, .55f, .60f, .35f, .25f, .12f, .18f, .22f, .10f, .08f, .06f, .10f },   // Wurli bright: octaves + upper odds for the bark
        { .60f, .55f, .50f, .20f, .30f, .10f, 0,0,0,0,0,0 },
    };
    static const float BASE_DEC[3] = { 3.5f, 1.8f, 0.90f };   // base sustain s (Wurli/Clav; Rhodes uses the tuning split). Clav 0.90 ≈ navkit epDecay 1.0
    int ty = (int)(v->harm * 2.999f); if (ty < 0) ty = 0; else if (ty > 2) ty = 2;
    v->ep_type = ty;
    float pp  = v->timb;                                     // pickup position / brightness
    float vel = v->vol;                                      // strike level 0..1
    float fn  = (v->freq - 80.0f) / 1200.0f; if (fn < 0.0f) fn = 0.0f; else if (fn > 1.0f) fn = 1.0f;
    v->ep_freqnorm = fn;
    const float dt = 1.0f / (float)SOUND_SAMPLE_RATE;
    float keep = 1.0f - fn;
    if (ty == 2) {
        // ── CLAV: VERBATIM navkit port (synth_oscillators.h initEPianoSettings, EP_PICKUP_CONTACT).
        // Rhodes/Wurli keep our own engine below — only the clav is navkit's. The pieces our old
        // crib was missing and that make navkit's clav sit right: (1) the RATIO BLEND that pulls the
        // modes toward INTEGER harmonics (so only the very top is inharmonic, not a clangy 5.15×/6.35×
        // stack); (2) bell emphasis; (3) the per-mode velocity curves; (4) the amp NORMALIZE so the
        // summed signal ≈ velocity (the pickup nonlinearity is amplitude-dependent). Macro mapping:
        // timbre = pickup position, morph = pickup distance (the honk), the rest are navkit's clav
        // preset constants (hardness 0.8, bell 0.15, bellTone 0.3, decay 1.0).
        static const float HARM[12]  = {1,2,3,4,5,6,7,8,9,10,11,12};
        static const float INH[12]   = {1.0f,2.003f,3.012f,4.028f,5.15f,6.35f,7.6f,8.9f,10.2f,11.6f,13.0f,14.5f};
        static const float BLEND[12] = {0,0,0.05f,0.2f,0.5f,0.75f,0.85f,0.9f,0.95f,1,1,1};
        static const float CEN[12]   = {1.0f,0.30f,0.20f,0.35f,0.15f,0.06f,0,0,0,0,0,0};   // neck pickup
        static const float OFF[12]   = {0.60f,0.55f,0.50f,0.20f,0.30f,0.10f,0,0,0,0,0,0};  // bridge pickup
        const float epHardness = 0.8f, epBell = 0.15f, epBellTone = 0.3f, epDecay = 1.0f;
        float pos = pp;                                      // pickup position = timbre macro
        float btEff = epBellTone + fn * 0.15f; if (btEff > 1.0f) btEff = 1.0f;
        float hard = epHardness + vel * (1.0f - epHardness) * 0.3f;
        float velSq = vel * vel, ampSum = 0.0f;
        for (int i = 0; i < 12; i++) {
            float modeBt = btEff * BLEND[i];
            float ratio  = HARM[i] * (1.0f - modeBt) + INH[i] * modeBt;
            v->ep_ratio[i] = ratio;
            v->ep_ph[i]    = 0.0f;
            float a = CEN[i] * (1.0f - pos) + OFF[i] * pos;
            a *= hard + (1.0f - hard) / (1.0f + (ratio - 1.0f) * 0.5f);   // hammer-hardness tilt
            if (i >= 3) a *= (1.0f + epBell * 1.5f);                      // bell emphasis (modes 4-6)
            if (i == 0)      a *= (1.0f - fn * 0.3f);                     // register
            else if (i >= 3) a *= (1.0f + fn * 0.4f);
            float velScale;                                              // per-mode velocity curves
            if      (i == 0) velScale = 0.05f + 0.95f * (velSq * (1.0f - epBell * 0.5f) + vel * epBell * 0.5f);
            else if (i == 1) velScale = 0.08f + 0.92f * (vel * 0.6f + velSq * 0.4f);
            else if (i == 2) velScale = 0.05f + 0.95f * (vel * 0.4f + velSq * 0.6f);
            else { float mix = 0.3f + epBell * 0.4f; velScale = 0.05f + 0.95f * (sqrtf(vel) * mix + vel * (1.0f - mix)); }
            a *= velScale;
            v->ep_amp[i] = a; ampSum += a;
            float dfac = 1.0f / (1.0f + (ratio - 1.0f) * (0.3f + btEff * 0.7f));
            float dec  = epDecay * (1.0f - fn * 0.5f) * dfac;
            if (dec < 0.02f) dec = 0.02f;
            v->ep_dec[i] = dt / dec;
        }
        if (ampSum > 0.001f) { float s = vel / ampSum; for (int i = 0; i < 12; i++) v->ep_amp[i] *= s; }   // peak sum ≈ vel
    } else if (ty == 1) {
        // ── WURLITZER: VERBATIM navkit port (initEPianoSettings, EP_PICKUP_ELECTROSTATIC). The reed's
        // signature is a WEAK 2nd + STRONG 3rd (the electrostatic pickup cancels evens) — our old crib
        // had H2 ~11dB too loud + a clangy intermod stack. Same fixes as the clav (ratio blend toward
        // integer harmonics + amp NORMALIZE), plus the electrostatic register rolloff + velocity curves.
        // navkit Wurlitzer preset: hardness 0.5, bell 0.15, bellTone 0.05, decay 1.8.
        static const float HARM[12]  = {1,2,3,4,5,6,7,8,9,10,11,12};
        static const float INH[12]   = {1.0f,2.02f,3.01f,5.04f,7.05f,9.08f,11.1f,13.1f,15.2f,17.2f,19.3f,21.3f};
        static const float BLEND[12] = {0,0.1f,0.2f,0.6f,0.9f,1,1,1,1,1,1,1};
        static const float CEN[12]   = {1.0f,0.08f,0.45f,0.12f,0.10f,0.04f,0,0,0,0,0,0};   // clean (centered)
        static const float OFF[12]   = {0.60f,0.15f,0.60f,0.20f,0.20f,0.08f,0,0,0,0,0,0};  // buzzy (offset)
        const float epHardness = 0.5f, epBell = 0.15f, epBellTone = 0.05f, epDecay = 1.8f;
        float pos = pp;                                      // pickup position = timbre macro
        float btEff = epBellTone + fn * 0.15f; if (btEff > 1.0f) btEff = 1.0f;
        float hard = epHardness + vel * (1.0f - epHardness) * 0.3f;
        float velSq = vel * vel, ampSum = 0.0f;
        for (int i = 0; i < 12; i++) {
            float modeBt = btEff * BLEND[i];
            float ratio  = HARM[i] * (1.0f - modeBt) + INH[i] * modeBt;
            v->ep_ratio[i] = ratio;
            v->ep_ph[i]    = 0.0f;
            float a = CEN[i] * (1.0f - pos) + OFF[i] * pos;
            a *= hard + (1.0f - hard) / (1.0f + (ratio - 1.0f) * 0.5f);   // hammer-hardness tilt
            if (i >= 3) a *= (1.0f + epBell * 1.5f);                      // bell emphasis
            { float keep = 1.0f - fn; keep *= keep;                       // electrostatic register rolloff
              if (i == 0)      a *= (1.0f - fn * 0.15f);
              else if (i <= 2) a *= keep;                                 // body modes thin quadratically
              else             a *= keep * keep; }                        // upper modes 4th-power steep
            float velScale;                                              // electrostatic velocity curves
            if      (i == 0) velScale = 0.10f + 0.90f * (vel * 0.6f + velSq * 0.4f);
            else if (i == 1) velScale = 0.08f + 0.92f * (vel * 0.6f + velSq * 0.4f);
            else if (i == 2) velScale = 0.12f + 0.88f * (vel * 0.6f + velSq * 0.4f);
            else             velScale = 0.05f + 0.95f * velSq;
            a *= velScale;
            v->ep_amp[i] = a; ampSum += a;
            float dfac = 1.0f / (1.0f + (ratio - 1.0f) * (0.3f + btEff * 0.7f));
            float dec  = epDecay * (1.0f - fn * 0.5f) * dfac;
            if (dec < 0.02f) dec = 0.02f;
            v->ep_dec[i] = dt / dec;
        }
        if (ampSum > 0.001f) { float s = vel / ampSum; for (int i = 0; i < 12; i++) v->ep_amp[i] *= s; }   // peak sum ≈ vel
    } else
    for (int i = 0; i < 12; i++) {
        v->ep_ratio[i] = RAT[ty][i];
        v->ep_ph[i]    = (float)i * 0.083f;                  // small spread (keep the attack edge, dodge a DC spike)
        float a = AC[ty][i] + (AO[ty][i] - AC[ty][i]) * pp;
        if (i == 0) {
            a *= (1.0f - 0.15f * fn);
        } else {
            float k = keep * keep;                           // upper modes fade with register
            if (i >= 4) k *= keep;                           // bell modes steeper (real Rhodes top ~ pure sine)
            a *= k * (0.3f + 0.7f * vel);                    // and brighter when struck harder
            a *= 0.45f + 1.1f * pp;                           // timbre = HAMMER HARDNESS: scales the upper
                                                             // modes so timbre always bites (the pickup
                                                             // crossfade alone is flat on wurli/clav)
        }
        v->ep_amp[i] = a;
        float dec;
        if (ty == 0) {                                       // Rhodes: per-mode decay — harmonics
            dec = DEC_R[i] * (1.0f - fn * 0.4f);             // sustain, inharmonic bells die fast (register-scaled)
        } else {
            float dfac = 1.0f / (1.0f + (RAT[ty][i] - 1.0f) * 0.25f);   // upper modes ring shorter
            dec = BASE_DEC[ty] * (1.0f - fn * 0.4f) * dfac;
        }
        if (dec < 0.02f) dec = 0.02f;
        v->ep_dec[i] = dt / dec;                             // per-sample decay fraction
    }
    v->ep_dc_prev = v->ep_dc_state = 0.0f;
    // the TANGENT CLICK — the key driving the string against the fret, the percussive chink that
    // IS the funky clav's attack. Loudest on the clav (a hard metal tangent), present-but-subtle
    // on Rhodes/Wurli hammers. Scales with strike velocity AND hammer hardness (timbre = pp).
    // navkit Clav-Funky: clickLevel 0.35 @ 2ms + velToClick (instrument_presets.h, preset 180).
    static const float CLICK[3] = { 0.10f, 0.13f, 0.42f };   // Rhodes / Wurli / Clav peak
    v->ep_click     = (int)(0.002f * (float)SOUND_SAMPLE_RATE);          // 2ms burst
    v->ep_click_amp = CLICK[ty] * (0.45f + 0.55f * vel) * (0.6f + 0.4f * pp);
    v->ep_on = true;
}

// One EP sample: sum the 12 decaying modes, then the PICKUP NONLINEARITY (the soul — a bare
// sum is a dull bell). Rhodes = asymmetric even-harmonic bark, Wurli = symmetric odd-harmonic
// buzz, Clav = mixed honk; bark amount = morph (live), register-scaled (high notes clean).
// A DC blocker follows (the sum^2 term injects DC). Pitch per sample (§8.8.1).
static inline float sound_epiano_sample(Voice *v, float pitch_mul) {
    if (!v->ep_on) return 0.0f;                       // engine id without a note-on init
    const float dt = 1.0f / (float)SOUND_SAMPLE_RATE;
    float f = v->freq * pitch_mul;
    if (f < 20.0f) f = 20.0f;
    float nyq = (float)SOUND_SAMPLE_RATE * 0.45f;
    float sum = 0.0f;
    for (int i = 0; i < 12; i++) {
        float a = v->ep_amp[i];
        if (a < 0.0001f) continue;
        v->ep_amp[i] = a - a * v->ep_dec[i];          // exponential decay
        float mf = f * v->ep_ratio[i];
        if (mf >= nyq) continue;                      // above Nyquist: decays silently
        v->ep_ph[i] += mf * dt;
        if (v->ep_ph[i] >= 1.0f) v->ep_ph[i] -= 1.0f;
        sum += sinf(v->ep_ph[i] * 6.2831853f) * a;
    }
    float regDist = (1.0f - v->ep_freqnorm) * (1.0f - v->ep_freqnorm);
    float bark = v->mor;                              // the dig-in growl — live (note_morph)
    float s2 = sum * sum, out;
    if (v->ep_type == 1) {                            // Wurli: VERBATIM navkit electrostatic nonlinearity
        // odd-only terms (sum³ + sum⁵) preserve the reed's odd-harmonic character; register-scaled
        // (high reeds are short, less nonlinear); symmetric tanh. pickupDist = morph, strikeVel = vol.
        float dist = bark;                            // morph = pickup distance (navkit Wurli 0.6)
        float velBoost = 1.0f + v->vol * dist;
        float k3 = dist * 1.5f * velBoost * regDist;
        float k5 = dist * 0.4f * velBoost * regDist;
        out = sum + k3 * sum * s2 + k5 * sum * s2 * s2;
        out = tanhf(out);
    } else if (v->ep_type == 2) {                     // Clav: VERBATIM navkit contact-pickup nonlinearity
        // pickupDist = morph (the honk macro); strikeVelocity = vol (captured). sum² = even
        // harmonics (honk/wah), sum³ = odd (bite), symmetric tanh*1.2 soft-clip.
        float dist = bark;                            // morph = pickup distance (navkit clav 0.6)
        float velBoost = 1.0f + v->vol * dist;
        float k2 = dist * 0.8f * velBoost;
        float k3 = dist * 1.0f * velBoost;
        out = sum + k2 * s2 + k3 * sum * s2;
        out = tanhf(out * 1.2f);
    } else {                                          // Rhodes: even harmonics, asymmetric clip.
        // The pickup nonlinearity is the REAL Rhodes harmonic source (Shear §2.2.1, Faraday's law
        // + non-uniform field), so it has an ALWAYS-ON floor (0.15) — even soft notes have grit —
        // and a GENTLER register gate than Wurli/Clav (present up the keyboard, not just low notes).
        float rbark = 0.15f + bark * 0.85f;
        float rdist = 1.0f - v->ep_freqnorm * 0.55f;
        float k  = (0.18f + rbark * 0.9f) * rdist;
        float k2 = (0.07f + rbark * 0.35f) * rdist;
        out = sum + k * s2 + k2 * sum * s2;
        float drive = 1.0f + rbark * 0.4f;
        out = (out >= 0.0f) ? tanhf(out * drive) : tanhf(out * drive * 0.85f) * 0.9f;
    }
    if (v->ep_click > 0) {                            // the tangent click burst (~2ms, linear decay)
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float n = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
        out += n * v->ep_click_amp * ((float)v->ep_click * dt / 0.002f);
        v->ep_click--;
    }
    float dcin = out;                                 // DC blocker (pickup AC coupling, ~7Hz)
    out = dcin - v->ep_dc_prev + 0.995f * v->ep_dc_state;
    v->ep_dc_prev  = dcin;
    v->ep_dc_state = out;
    return out * 0.8f;
}

// MEMBRANE note-on: strike the drumhead. The six modes get a generic membrane energy profile
// (upper modes weaker); the strike POSITION (timbre) reweights them LIVE in the sample fn, so
// note_timbre reshapes a ringing head. Head ratios (harmonics) and the bend (morph) are read
// live too. navkit crib: initMembranePreset / processMembraneOscillator (synth_oscillators.h).
static void sound_membrane_start(Voice *v) {
    // base mode energies — a generic struck head, the partials falling off with mode index.
    // Strike position (timbre) does the center/edge shaping live; harmonics does the ratios.
    static const float BASE[6] = { 1.0f, 0.60f, 0.45f, 0.32f, 0.22f, 0.13f };
    float sum = 0.0f;
    for (int m = 0; m < 6; m++) { v->mb_phase[m] = 0.0f; v->mb_amp[m] = BASE[m]; sum += BASE[m]; }
    if (sum > 0.0001f) { float g = 1.0f / sum; for (int m = 0; m < 6; m++) v->mb_amp[m] *= g; }
    // the contact transient: ~2ms of noise, only audible toward the edge (the slap snap)
    v->mb_strike = (int)(0.002f * (float)SOUND_SAMPLE_RATE);
    v->mb_on     = true;
}

// One MEMBRANE sample: sum the six modes, decay them (upper modes die fast — that's the
// "thump"), weight them by strike position, add the slap click, and bend the whole head.
// Live macros: harmonics crossfades the mode RATIOS (tuned-harmonic tabla ↔ ideal-membrane
// inharmonic djembe) and sets the ring length; timbre is the STRIKE POSITION (center thump ↔
// edge ring/slap); morph is the BEND DEPTH — pitch starts raised off the strike and settles
// over ~90ms (the membrane chirp / tabla bayan gliss), derived from step_samples so it needs
// no state. The bend on a SIX-mode head is what a one-sine 808/909 can't reach. Pitch per
// sample (§8.8.1).
static inline float sound_membrane_sample(Voice *v, float pitch_mul) {
    if (!v->mb_on) return 0.0f;                          // engine id without a note-on init
    const float dt = 1.0f / (float)SOUND_SAMPLE_RATE;
    // tuned head (tabla — its loaded skin pulls the modes to a near-HARMONIC series, a pitched
    // drum) ↔ ideal circular-membrane ratios (djembe/conga — Bessel zeros, inharmonic thud).
    static const float RT[6] = { 1.0f, 2.0f,   3.0f,   4.0f,   5.0f,   6.0f   };
    static const float RD[6] = { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f };  // navkit Bessel
    float f = v->freq * pitch_mul;
    if (f < 20.0f) f = 20.0f;
    // the bend: pitch starts raised off the strike (≈+morph fifth at the peak) and settles to
    // the base over ~90ms — navkit's membrane chirp; morph 0 = flat. 808/909 do this to ONE
    // sine, here it bends all six modes together.
    float t = (float)v->step_samples * dt;
    float bend = 1.0f + v->mor * 0.6f * expf(-t / 0.09f);
    // ring: a tuned tabla head SINGS — its loaded skin sustains a pitched tone for ~1.5s+ —
    // while a damped djembe is a short thud. harmonics tilts between the two.
    float t60  = 1.6f - 1.25f * v->harm;                 // ~1.6s tuned tabla ring → ~0.35s djembe thud
    float rate = 6.9078f / t60;                          // amp *= (1 - rate·dt) ≈ e^-6.9 at t60
    float nyq  = (float)SOUND_SAMPLE_RATE * 0.45f;
    float out  = 0.0f;
    for (int m = 0; m < 6; m++) {
        float amp = v->mb_amp[m];
        if (amp < 0.00002f) continue;
        // upper modes ring much shorter than the fundamental — the head dumps its highs fast,
        // which is exactly what reads as a struck drum rather than a sustained pad
        v->mb_amp[m] = amp - amp * rate * (1.0f + 1.4f * (float)m) * dt;
        float ratio = RT[m] + (RD[m] - RT[m]) * v->harm;
        float mf = f * ratio * bend;
        if (mf >= nyq) continue;                         // above Nyquist: decays silently
        v->mb_phase[m] += mf * dt;
        if (v->mb_phase[m] >= 1.0f) v->mb_phase[m] -= 1.0f;
        // strike position (timbre): center (0) damps the upper modes → round boom; edge (1)
        // lifts them progressively → bright ring/slap. navkit's circular-membrane weighting
        // (the fundamental, mode 0, is unaffected — it's there wherever you strike).
        float pos = 1.0f;
        if (m > 0) pos = (1.0f - v->timb) * (1.0f / (float)(m + 1)) + v->timb * (float)m * 0.15f;
        out += sinf(v->mb_phase[m] * 6.2831853f) * amp * pos;
    }
    if (v->mb_strike > 0) {                              // slap contact click (edge/slap-gated)
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float n = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
        out += n * 0.25f * v->timb * ((float)v->mb_strike * dt / 0.002f);
        v->mb_strike--;
    }
    return out * 0.9f;
}

// REED note-on: size the bore (a half-wavelength delay line one-way; the open-end reflection
// gives the return trip → full period) and seed it with tiny noise for fast startup (navkit's
// trick). REUSES ks_buf as the bore — reed is its own wave id, so it never collides with the
// Karplus path on one voice. Self-oscillates from here; the amp ADSR gates it like organ/PD.
static void sound_reed_start(Voice *v) {
    float f = v->freq; if (f < 20.0f) f = 20.0f;
    int len = (int)((float)SOUND_SAMPLE_RATE / f / 2.0f);   // half-wavelength, one-way bore
    if (len > SOUND_KS_MAX - 1) len = SOUND_KS_MAX - 1;     // bottoms out ~43Hz, below reed range
    if (len < 4) len = 4;
    for (int i = 0; i < len; i++) {                         // seed with tiny noise (faster than silence)
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        v->ks_buf[i] = (((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f) * 0.01f;
    }
    v->rd_len = len; v->rd_idx = 0; v->rd_initfreq = f;
    v->rd_lp = v->rd_dc_prev = v->rd_dc_state = v->rd_vib_ph = v->rd_tilt = v->rd_noise_lp = 0.0f;
    v->rd_drift_ph = v->rd_drift = 0.0f;
    v->rd_attack   = (int)(0.028f * (float)SOUND_SAMPLE_RATE);   // ~28ms breathy chiff onset
    v->rd_on = true;
}

// One REED sample: the McIntyre/Schumacher/Woodhouse waveguide — mouth pressure drives a
// nonlinear reed valve that recirculates through the bore delay; the bore's reflected pressure
// modulates the reed (self-oscillation). Live macros, ALL clamped to the oscillation window
// (STEP-0 §8.8.7 — outside it the model chokes silent): harmonics = BORE conicity (cylindrical
// clarinet, odd-only + dark ↔ conical sax, all harmonics + bright — drives the bell LP, the
// end-reflection, and the conical even-harmonic buzz, with per-position makeup gain since the
// conical bell is ~10dB quieter); timbre = REED EDGE (stiffness + aperture narrowing together,
// stiffness alone too weak — plus an output brightness tilt built here); morph = BREATH inside
// a safe window (blow pressure + lip vibrato deepening — the model can't overblow, this is the
// musical "lean-in" growl). Pitch: bore read length tracks freq*pitch_mul (§8.8.1).
static inline float sound_reed_sample(Voice *v, float pitch_mul) {
    if (!v->rd_on || v->rd_len <= 0) return 0.0f;          // engine id without a note-on init
    const float TWO_PI = 6.2831853f;
    const float SR = (float)SOUND_SAMPLE_RATE;
    // macros → physical params, every range pinned inside the self-oscillating region
    float bore  = v->harm * 0.95f;                         // harmonics = bore conicity (cyl→conical)
    float stiff = 0.20f + v->timb * 0.62f;                 // ≤0.82: stiffer = brighter (but weak alone)
    float apert = 0.70f - v->timb * 0.50f;                 // narrows with edge (the paired axis)
    float blow  = 0.52f + v->mor * 0.23f;                  // breath: [0.52,0.75], under the ~0.78 choke
    float vibd  = 0.10f + v->mor * 0.40f;                  // lean-in vibrato deepens with breath
    // HUMANIZED lip vibrato — a real player's vibrato is not a clean LFO: rate and depth wander,
    // and it lives mostly in PITCH with only a little pressure. A slow ~0.7Hz wobble drifts both.
    v->rd_drift_ph += 0.7f / SR; if (v->rd_drift_ph >= 1.0f) v->rd_drift_ph -= 1.0f;
    float wob = sinf(v->rd_drift_ph * TWO_PI);             // slow wander, shared by rate + depth
    v->rd_vib_ph += (5.2f + 0.9f * wob) / SR;              // vibrato rate drifts ~4.3..6.1 Hz
    if (v->rd_vib_ph >= 1.0f) v->rd_vib_ph -= 1.0f;
    float vib = sinf(v->rd_vib_ph * TWO_PI) * vibd * (0.8f + 0.2f * wob);   // depth breathes too
    // breath turbulence — the "air" in the tone, the #1 cue that this is a REAL wind instrument
    // (navkit omits it, so the bare port reads as a synth tooter). Lightly LP the white noise
    // (airy, not hissy), scale by breath pressure so it vanishes when not blowing, add to the
    // DRIVING pressure so the bore resonates it into breathy formants, not a hiss layer on top.
    v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
    float wn = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
    v->rd_noise_lp += 0.55f * (wn - v->rd_noise_lp);
    v->rd_drift += 0.0007f * (wn - v->rd_drift);          // very slow random walk → wandering breath
    float air = 0.10f + v->mor * 0.12f;                    // more air as the player leans in (breath)
    if (v->rd_attack > 0) {                                // the chiff: a breathy noise burst at onset
        air += 0.55f * (float)v->rd_attack / (0.028f * SR);
        v->rd_attack--;
    }
    // mouth pressure: steady blow + a little vibrato + a slow drift + the resonated breath noise
    float Pm = blow + vib * 0.045f + v->rd_drift * blow * 0.05f + blow * air * v->rd_noise_lp;
    // bore delay: fractional read length tracks the live pitch — plus our pitch vibrato (where a
    // real wind vibrato mostly lives), the lip drift, and any cart LFO/glide/pitch-env (§8.8.1)
    float curf = v->freq * pitch_mul * (1.0f + vib * 0.011f); if (curf < 20.0f) curf = 20.0f;
    float effLen = (float)v->rd_len * v->rd_initfreq / curf;
    if (effLen < 2.0f) effLen = 2.0f;
    if (effLen > (float)v->rd_len) effLen = (float)v->rd_len;
    float readPos = (float)v->rd_idx - effLen;
    while (readPos < 0.0f) readPos += (float)v->rd_len;
    int i0 = (int)readPos; if (i0 >= v->rd_len) i0 -= v->rd_len;
    int i1 = (i0 + 1 < v->rd_len) ? i0 + 1 : 0;
    float fr = readPos - floorf(readPos);
    float boreReturn = v->ks_buf[i0] + fr * (v->ks_buf[i1] - v->ks_buf[i0]);
    // bell-end LP (wall + radiation loss): cylindrical dark (heavy), conical bright (light)
    float lpCoeff = 0.55f + bore * 0.37f;
    v->rd_lp = v->rd_lp * (1.0f - lpCoeff) + boreReturn * lpCoeff;
    float endRefl = -0.95f + bore * 0.10f;                 // open-bell inversion; flared bell loses less
    // reed reflection: offset (aperture) + slope (stiffness) · pressure difference, clamped
    float pdiff    = endRefl * v->rd_lp - Pm;
    float reedRefl = (0.30f + apert * 0.40f) + (-0.40f - stiff * 0.50f) * pdiff;
    if (reedRefl < -1.0f) reedRefl = -1.0f; else if (reedRefl > 1.0f) reedRefl = 1.0f;
    float boreInput = Pm + pdiff * reedRefl;
    // conical bores support even harmonics (sax buzz); cylindrical suppresses them
    if (bore > 0.3f) {
        float cd = (bore - 0.3f) * 0.7f;
        boreInput = tanhf(boreInput * (1.0f + cd * 2.0f)) / (1.0f + cd * 2.0f);
        boreInput += cd * boreInput * boreInput * 0.3f;    // asymmetric → even harmonics
    }
    v->ks_buf[v->rd_idx] = boreInput;
    v->rd_idx++; if (v->rd_idx >= v->rd_len) v->rd_idx = 0;
    // DC block — the reed model carries a large DC from steady blow pressure
    float out = boreReturn;
    float dc  = out - v->rd_dc_prev + 0.995f * v->rd_dc_state;
    v->rd_dc_prev = out; v->rd_dc_state = dc;
    // output brightness tilt (timbre): the waveguide alone barely brightens (STEP-0), so
    // emphasize the high end against a slow LP of the output to guarantee audible edge travel
    v->rd_tilt += 0.40f * (dc - v->rd_tilt);
    float bright = dc + v->timb * 1.5f * (dc - v->rd_tilt);
    // makeup gain: the conical bell radiates ~10dB quieter (STEP-0) — lift it so sax isn't buried
    return bright * 1.5f * (1.0f + bore * 2.0f);
}

// PIPE note-on: size the bore and seed it with noise for fast startup. REUSES ks_buf as the
// bore (distinct wave id, never collides with reed/Karplus on a voice). Clear the jet delay;
// arm the breathy "tu" chiff.
//
// TUNING (fixed 2026-06-11; tune-check.js found it an octave low + flat). The open end is the
// only modeled reflection and it INVERTS (`-boreReturn`), so the loop resonates at SR/(2·delay)
// — a HALF wavelength, not a full one. Sizing the bore at SR/f rang the flute an octave too low.
// The loop also carries the jet sub-loop + the reflection/radiation-filter group delay, which
// rang it flat. That extra delay is pitch-independent but scales with the JET length (the morph/
// embouchure macro): ~0.31 sample per jet sample + a ~1.69 filter constant. So: half wavelength
// minus that jet-derived term, sized with the bowed-string fractional-read trick below to kill
// integer quantization. Result: in tune within ~±3¢ from C4 to ~E6 at typical embouchure (verified
// morph 0.70). A *constant* left morph≠0 sharp by up to a semitone — deriving from jetLen is what
// makes it morph-robust. CAVEAT: the extreme morph≈0 voice (jetLen 11 ≈ a ~20-sample top-octave
// bore) sits on the overblow edge and is seed-unstable in its top octave (tune-check's default
// sweep tests morph 0 and still flags PIPE A5); any real recipe uses morph ≳ 0.3. Sibling REED
// already sized its bore at SR/f/2 — PIPE was the outlier. (STATUS Open #31.)
static void sound_pipe_start(Voice *v) {
    float f = v->freq; if (f < 20.0f) f = 20.0f;
    // Loop delay = the jet sub-loop + reflection/radiation LP group delay. It's pitch-independent
    // but scales with the JET length (the morph/embouchure macro): ~0.31 sample per jet sample +
    // a ~1.69 filter constant (morph 0 → jetLen 11 → ~5.1; morph 0.7 → jetLen 5 → ~3.2). Deriving
    // from jetLen keeps it in tune across the whole embouchure range — a CONSTANT left morph≠0
    // sharp by up to a semitone. (jetLen0 must mirror the sample-loop jetLen below.)
    int   jetLen0 = 3 + (int)((1.0f - v->mor) * 8.0f);
    float loopDelay = 1.69f + 0.308f * (float)jetLen0;
    float targetBore = (float)SOUND_SAMPLE_RATE / (2.0f * f) - loopDelay;   // exact bore (float)
    if (targetBore < 3.0f) targetBore = 3.0f;
    // Buffer a hair LONGER than the target, then reference an effective init freq so the per-sample
    // fractional read SHORTENS the loop back onto the exact pitch — the bowed-string trick. Without
    // it the note-on bore is integer-quantized, and at a ~20-sample bore (A5) one sample is ~80¢, so
    // high notes could never land in tune. The fractional read removes that quantization entirely.
    int len = (int)targetBore + 2;
    if (len > SOUND_KS_MAX - 1) len = SOUND_KS_MAX - 1;
    if (len < 4) len = 4;
    for (int i = 0; i < len; i++) {                         // seed with noise (faster oscillation startup)
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        v->ks_buf[i] = (((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f) * 0.05f;
    }
    v->pp_len = len; v->pp_idx = 0;
    v->pp_initfreq = f * targetBore / (float)len;            // effLen at curf=f = targetBore (exact, fractional)
    for (int i = 0; i < 64; i++) v->pp_jet[i] = 0.0f;
    v->pp_jet_idx = 0;
    v->pp_lp = v->pp_dc_prev = v->pp_dc_state = 0.0f;
    v->pp_vib_ph = v->pp_drift_ph = v->pp_drift = v->pp_noise_lp = 0.0f;
    v->pp_attack = (int)(0.025f * (float)SOUND_SAMPLE_RATE);   // ~25ms tongued "tu" onset
    v->pp_on = true;
}

// One PIPE sample: the STK jet-drive flute (Cook/Scavone). An air jet (the bore's reflected wave,
// delayed by the lip→labium travel time) is deflected by a tanh nonlinearity; the deflected jet
// re-excites the bore → self-oscillation. Macros (§8.8.8): harmonics = OVERBLOW (jet gain — pure
// fundamental → octave flageolet → bright; navkit leaves this at 0, it's ours), timbre = BREATH
// AIR (excitation level + the reed-style turbulence — a flute is mostly air), morph = EMBOUCHURE
// (mouth-end feedback coupling + live jet length — hollow/dark ↔ focused, eases the overblow).
// Held/self-oscillating; reuses reed's breath turbulence + humanized pitch-vibrato + chiff + drift.
static inline float sound_pipe_sample(Voice *v, float pitch_mul) {
    if (!v->pp_on || v->pp_len <= 0) return 0.0f;          // engine id without a note-on init
    const float TWO_PI = 6.2831853f;
    const float SR = (float)SOUND_SAMPLE_RATE;
    // macros → physical params
    float gain   = 2.0f + v->harm * 8.0f;                  // harmonics = overblow (jet nonlinearity gain)
    float fbGain = 0.50f + v->mor * 0.40f;                 // morph = embouchure: mouth-end coupling
    int   jetLen = 3 + (int)((1.0f - v->mor) * 8.0f);      // longer jet (low embouchure) overblows easier
    if (jetLen < 2) jetLen = 2; if (jetLen > 63) jetLen = 63;
    float breath = 0.55f + v->timb * 0.35f;                // timbre = breath air: excitation energy
    // HUMANIZED pitch vibrato — wandering rate/depth, like reed (a flute's vibrato is pitch, not amp)
    v->pp_drift_ph += 0.7f / SR; if (v->pp_drift_ph >= 1.0f) v->pp_drift_ph -= 1.0f;
    float wob = sinf(v->pp_drift_ph * TWO_PI);
    v->pp_vib_ph += (5.0f + 0.8f * wob) / SR; if (v->pp_vib_ph >= 1.0f) v->pp_vib_ph -= 1.0f;
    float vib = sinf(v->pp_vib_ph * TWO_PI) * (0.6f + 0.4f * wob);
    // breath turbulence — the flute IS air; resonate filtered noise through the bore + a slow drift
    v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
    float wn = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
    v->pp_noise_lp += 0.6f * (wn - v->pp_noise_lp);
    v->pp_drift += 0.0007f * (wn - v->pp_drift);
    float airamt = 0.14f + v->timb * 0.16f;                // breathier as timbre opens up
    if (v->pp_attack > 0) { airamt += 0.6f * (float)v->pp_attack / (0.025f * SR); v->pp_attack--; }  // the chiff
    breath += breath * (airamt * v->pp_noise_lp + v->pp_drift * 0.06f);
    // bore delay: fractional read length tracks live pitch + the pitch vibrato (§8.8.1)
    float curf = v->freq * pitch_mul * (1.0f + vib * 0.010f); if (curf < 20.0f) curf = 20.0f;
    float effLen = (float)v->pp_len * v->pp_initfreq / curf;
    if (effLen < 2.0f) effLen = 2.0f;
    if (effLen > (float)v->pp_len) effLen = (float)v->pp_len;
    float readPos = (float)v->pp_idx - effLen;
    while (readPos < 0.0f) readPos += (float)v->pp_len;
    int i0 = (int)readPos; if (i0 >= v->pp_len) i0 -= v->pp_len;
    int i1 = (i0 + 1 < v->pp_len) ? i0 + 1 : 0;
    float fr = readPos - floorf(readPos);
    float boreReturn = v->ks_buf[i0] + fr * (v->ks_buf[i1] - v->ks_buf[i0]);
    // open-end reflection: invert + loss + radiation LP
    float openFiltered = -boreReturn * 0.9f;
    v->pp_lp = v->pp_lp * 0.15f + openFiltered * 0.85f;
    float reflected = v->pp_lp;
    // jet delay (lip→labium travel), then the nonlinear jet deflection
    v->pp_jet[v->pp_jet_idx] = reflected * fbGain;
    int jetRead = (v->pp_jet_idx - jetLen + 64) % 64;
    float jetOut = v->pp_jet[jetRead];
    v->pp_jet_idx = (v->pp_jet_idx + 1) % 64;
    float excitation = tanhf(jetOut * gain) * breath;      // tanh S-curve: self-oscillates when gain·fb > 1
    // bore input: jet excitation + the reflected wave; write back into the bore
    float boreInput = excitation + reflected * 0.5f;
    v->ks_buf[v->pp_idx] = boreInput;
    v->pp_idx++; if (v->pp_idx >= v->pp_len) v->pp_idx = 0;
    // output: radiated open-end + a little direct jet
    float out = boreReturn * 0.5f + excitation * 0.3f;
    float dc  = out - v->pp_dc_prev + 0.995f * v->pp_dc_state;
    v->pp_dc_prev = out; v->pp_dc_state = dc;
    return dc * 2.0f;
}

// ── INSTR_BOWED: bowed string (navkit processBowedOscillator / initBowed, line-for-line) ──
// BOWED note-on: size the string (full wavelength = nut + bridge halves), SPLIT it at the bow
// contact point β (from harmonics), and seed both halves with tiny noise so the friction loop
// starts. REUSES ks_buf packed as two sub-range circular buffers (distinct wave id — never
// collides with the Karplus/reed/pipe paths on a voice). Mirrors navkit's initBowed exactly,
// except β comes from the harmonics macro (mapped into the wedge) instead of a fixed preset.
static void sound_bowed_start(Voice *v) {
    float f = v->freq; if (f < 20.0f) f = 20.0f;
    // Round the wavelength UP (+1), not down: the buffer must be a hair LONGER than the true
    // length so the per-sample fractional read can SHORTEN it back exactly onto pitch (the read
    // clamps at the buffer length — it can shrink the delay but never grow it). Rounding down left
    // every note ringing sharp with no way to correct (up to ~24 cents on high notes).
    int totalLen = (int)((float)SOUND_SAMPLE_RATE / f) + 1; // full wavelength (bow→nut + bow→bridge)
    if (totalLen > SOUND_KS_MAX - 1) totalLen = SOUND_KS_MAX - 1;
    if (totalLen < 4) totalLen = 4;
    // harmonics = bow position β (sul ponticello → tasto). STEP-0 wedge: 0.05–0.25 all lock.
    float pos = 0.05f + v->harm * 0.20f;
    if (pos < 0.05f) pos = 0.05f; else if (pos > 0.95f) pos = 0.95f;   // navkit's clamp
    int nutLen = (int)(totalLen * pos);
    int brLen  = totalLen - nutLen;
    if (nutLen < 2) nutLen = 2;
    if (brLen  < 2) brLen  = 2;
    if (nutLen > SOUND_KS_MAX - 3) nutLen = SOUND_KS_MAX - 3;          // leave room for the bridge half
    if (nutLen + brLen > SOUND_KS_MAX) brLen = SOUND_KS_MAX - nutLen;
    v->bw_nutlen = nutLen; v->bw_brlen = brLen;
    v->bw_nutidx = 0; v->bw_bridx = 0;
    v->bw_nutrefl = 0.0f; v->bw_brrefl = 0.0f;
    // PIZZICATO vs ARCO is one difference: how energy enters the string. ARCO seeds tiny noise
    // (navkit's 0.005 startup) and lets the bow friction drive it. PIZZ seeds a real PLUCK burst —
    // a lowpassed noise excitation across the whole string (Karplus-Strong style) — and the friction
    // is bypassed per-sample, so the same waveguide just rings down. eng_p[0] >= 0.5 → pizz.
    v->bw_pizz = (v->eng_p[0] >= 0.5f);
    if (v->bw_pizz) {
        float lp = 0.0f, bright = 0.20f + v->timb * 0.45f;   // warmer (more LP) = a rounder finger pluck
        for (int i = 0; i < nutLen + brLen; i++) {
            v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
            float wn = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
            lp += bright * (wn - lp);
            v->ks_buf[i] = lp * 1.4f;                        // pluck amplitude (sets the attack level)
        }
    } else {
        for (int i = 0; i < nutLen + brLen; i++) {
            v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
            v->ks_buf[i] = (((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f) * 0.005f;
        }
    }
    // TUNING. The loop delay isn't just the buffer length: it's (nutLen+brLen) PLUS the group
    // delay of the two one-pole reflection filters (~0.5 sample, near-constant), PLUS the integer
    // rounding of totalLen. Left uncompensated the string rings flat — and flatter up high, where
    // half a sample is a bigger slice of the wavelength (measured ~22 cents flat at E6). Reference
    // an EFFECTIVE base freq that folds the filter delay in, so the per-sample fractional read
    // (ratio = curf/initfreq) lands the loop exactly on pitch across the whole range.
    const float BW_LOOP_DELAY = 0.5f;   // reflection-filter group delay, in samples
    v->bw_initfreq = ((float)SOUND_SAMPLE_RATE - BW_LOOP_DELAY * f) / (float)(nutLen + brLen);
    v->bw_vib_ph = v->bw_drift_ph = v->bw_drift = v->bw_noise_lp = 0.0f;
    v->bw_dc_prev = v->bw_dc_state = 0.0f;
    v->bw_attack = (int)(0.030f * (float)SOUND_SAMPLE_RATE);   // ~30ms bow-bite catch at onset
    v->bw_on = true;
}

// One BOWED sample: navkit's processBowedOscillator verbatim. Two fractional-delay reads (nut +
// bridge) sum to the string velocity at the bow; the differential bow↔string velocity drives a
// hyperbolic stick-slip friction (f = pres·dv·exp(-pres·dv²)) that re-excites both halves. The
// reflections (inverted + lossy LP at each end) close the loop → self-oscillation. Macros are
// pinned INSIDE the STEP-0 Helmholtz wedge (navkit-bowsweep.c): timbre = bow PRESSURE (low =
// clean leaning-sawtooth, high = scratchy surface — the narrow axis), morph = bow VELOCITY/swell
// (louder + wider wedge as it grows). Realism navkit omits (else it reads as a synth saw): a
// humanized PITCH vibrato (a violinist's left hand), light bow-NOISE on the velocity (rosin
// texture), and a brief attack BITE. Pitch tracks freq*pitch_mul like reed/pipe (§8.8.1).
static inline float sound_bowed_sample(Voice *v, float pitch_mul) {
    if (!v->bw_on || v->bw_nutlen <= 0 || v->bw_brlen <= 0) return 0.0f;   // engine id w/o note-on
    const float TWO_PI = 6.2831853f;
    const float SR = (float)SOUND_SAMPLE_RATE;
    // macros → physical params, pinned inside the wedge
    float pressure = 0.12f + v->timb * 0.20f;              // timbre = bow pressure [0.12,0.32] (lowered: less scratch)
    float bowSpeed = 0.30f + v->mor  * 0.60f;              // morph = bow speed [0.30,0.90]
    float velocity = bowSpeed * 0.2f;                      // navkit's physical-range scale
    // HUMANIZED pitch vibrato — wandering rate/depth, lives in PITCH (like a real bowed string)
    v->bw_drift_ph += 0.6f / SR; if (v->bw_drift_ph >= 1.0f) v->bw_drift_ph -= 1.0f;
    float wob = sinf(v->bw_drift_ph * TWO_PI);
    v->bw_vib_ph += (5.3f + 0.8f * wob) / SR; if (v->bw_vib_ph >= 1.0f) v->bw_vib_ph -= 1.0f;
    float vib = sinf(v->bw_vib_ph * TWO_PI) * (0.6f + 0.4f * wob);
    // bow noise (rosin grip texture) + a slow drift on the bow speed — navkit omits both
    v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
    float wn = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
    v->bw_noise_lp += 0.5f * (wn - v->bw_noise_lp);
    v->bw_drift += 0.0006f * (wn - v->bw_drift);
    velocity += velocity * (0.03f * v->bw_noise_lp + 0.05f * v->bw_drift);
    if (v->bw_attack > 0) {                                // the bow catch: a stronger bite at onset
        velocity += velocity * 0.5f * (float)v->bw_attack / (0.030f * SR);
        v->bw_attack--;
    }
    // pitch tracking: read length scales by initfreq/curf (arp/glide/pitch-LFO all bend it)
    float curf = v->freq * pitch_mul * (1.0f + vib * 0.009f); if (curf < 20.0f) curf = 20.0f;
    float ratio = curf / (v->bw_initfreq > 20.0f ? v->bw_initfreq : 20.0f);
    if (ratio < 0.25f) ratio = 0.25f; if (ratio > 4.0f) ratio = 4.0f;
    // nut-side fractional read (ks_buf[0..nutlen))
    float nutEffLen = (float)v->bw_nutlen / ratio;
    if (nutEffLen < 2.0f) nutEffLen = 2.0f;
    if (nutEffLen > (float)v->bw_nutlen) nutEffLen = (float)v->bw_nutlen;
    float nutReadPos = (float)v->bw_nutidx - nutEffLen;
    while (nutReadPos < 0.0f) nutReadPos += (float)v->bw_nutlen;
    int n0 = (int)nutReadPos; if (n0 >= v->bw_nutlen) n0 -= v->bw_nutlen;
    int n1 = (n0 + 1 < v->bw_nutlen) ? n0 + 1 : 0;
    float nfr = nutReadPos - floorf(nutReadPos);
    float nutReturn = v->ks_buf[n0] + nfr * (v->ks_buf[n1] - v->ks_buf[n0]);
    // bridge-side fractional read (ks_buf[nutlen..nutlen+brlen)), base-offset into the packed buffer
    int base = v->bw_nutlen;
    float brEffLen = (float)v->bw_brlen / ratio;
    if (brEffLen < 2.0f) brEffLen = 2.0f;
    if (brEffLen > (float)v->bw_brlen) brEffLen = (float)v->bw_brlen;
    float brReadPos = (float)v->bw_bridx - brEffLen;
    while (brReadPos < 0.0f) brReadPos += (float)v->bw_brlen;
    int b0 = (int)brReadPos; if (b0 >= v->bw_brlen) b0 -= v->bw_brlen;
    int b1 = (b0 + 1 < v->bw_brlen) ? b0 + 1 : 0;
    float bfr = brReadPos - floorf(brReadPos);
    float bridgeReturn = v->ks_buf[base + b0] + bfr * (v->ks_buf[base + b1] - v->ks_buf[base + b0]);
    // string velocity at the bow = sum of the returning traveling waves
    float vStringAtBow = nutReturn + bridgeReturn;
    float deltaV = velocity - vStringAtBow;
    // Smith hyperbolic stick-slip friction: linear at small dv (stick), falls off (slip).
    // PIZZICATO bypasses it entirely — no bow drive, so the seeded pluck just rings down through
    // the reflections (the string was plucked and released, not bowed).
    float friction;
    if (v->bw_pizz) {
        friction = 0.0f;
    } else {
        float pres = pressure * 5.0f + 0.5f;
        friction = pres * deltaV * expf(-pres * deltaV * deltaV);
    }
    // outgoing waves from the bow point toward each end
    float toNut = bridgeReturn + friction;
    float toBridge = nutReturn + friction;
    // bridge loss: arco is near-lossless (it self-oscillates); pizz damps faster so the pluck
    // decays in a musical ~1s instead of ringing on
    float brLoss = v->bw_pizz ? 0.990f : 0.995f;
    // nut-end reflection: fixed end → invert + one-pole LP (stiffness loss)
    float nutReflected = -toNut;
    v->bw_nutrefl = v->bw_nutrefl * 0.35f + nutReflected * 0.65f;
    // bridge-end reflection: invert + loss + stronger LP
    float bridgeReflected = -toBridge * brLoss;
    v->bw_brrefl = v->bw_brrefl * 0.15f + bridgeReflected * 0.85f;
    // write the reflected waves back at the current write indices (read again after a round-trip)
    v->ks_buf[v->bw_nutidx]    = v->bw_nutrefl;
    v->ks_buf[base + v->bw_bridx] = v->bw_brrefl;
    v->bw_nutidx = (v->bw_nutidx + 1) % v->bw_nutlen;
    v->bw_bridx  = (v->bw_bridx  + 1) % v->bw_brlen;
    // output: bridge-side signal (what the body radiates), DC-blocked + makeup gain
    float out = toBridge * 0.8f;
    float dc  = out - v->bw_dc_prev + 0.995f * v->bw_dc_state;
    v->bw_dc_prev = out; v->bw_dc_state = dc;
    return dc * 3.0f;
}

// ── INSTR_BRASS: lip-reed brass (STK BrassInstrument, Cook/Scavone) ─────────────
// BRASS note-on: size the bore (half-wavelength one-way, like reed — the inverting bell reflection
// gives the return trip) and seed it with tiny noise for a fast oscillation startup. REUSES ks_buf
// as the bore (distinct wave id 29 — never collides with reed/pipe/bowed/Karplus on a voice). Clear
// the lip biquad + bell LP + DC block; arm a short breathy "tah" speak transient. Self-oscillates
// from here; the amp ADSR gates it like organ/reed/pipe/bowed.
static void sound_brass_start(Voice *v) {
    float f = v->freq; if (f < 20.0f) f = 20.0f;
    int len = (int)((float)SOUND_SAMPLE_RATE / f / 2.0f);   // half-wavelength one-way bore (reed model)
    if (len > SOUND_KS_MAX - 1) len = SOUND_KS_MAX - 1;     // bottoms out ~43Hz, below brass range
    if (len < 4) len = 4;
    for (int i = 0; i < len; i++) {                         // seed with tiny noise (faster than silence)
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        v->ks_buf[i] = (((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f) * 0.01f;
    }
    v->br_len = len; v->br_idx = 0; v->br_initfreq = f;
    v->br_lip_y1 = v->br_lip_y2 = v->br_lip_x1 = v->br_lip_x2 = 0.0f;   // lip biquad state
    v->br_lp = v->br_dc_prev = v->br_dc_state = 0.0f;
    v->br_out_prev = v->br_out_state = 0.0f;
    v->br_vib_ph = v->br_drift_ph = v->br_drift = v->br_noise_lp = 0.0f;
    v->br_attack = (int)(0.018f * (float)SOUND_SAMPLE_RATE);   // ~18ms breath/"tah" speak onset
    v->br_on = true;
}

// One BRASS sample: a bore delay (REUSES ks_buf) closed by an inverting bell reflection; the
// returning pressure drives a LIP VALVE modeled as a 2-pole resonant biquad (the mass-spring lip —
// its resonance sits just ABOVE the played pitch, the mechanism behind lip-slurs and harmonic lock).
// Mouth pressure minus the bore's reflected pressure crosses a one-sided flow nonlinearity at the
// lip (a lip can only open outward); the result re-excites the bore. The DEFINING timbre is
// pressure-driven BRASSINESS: at high blow + high timbre the circulating wave steepens (a shockwave),
// exploding the overtones — quiet=round/mellow, loud=blatty. Macros (§8.8.10, the only three knobs):
// harmonics = INSTRUMENT/BORE (bright/tight trumpet → cornet → flugel → trombone → horn → dark tuba;
// drives the lip-resonance ratio + Q + bell LP, NEVER the delay length so it can't detune), timbre =
// BRASSINESS (the shockwave overtone explosion), morph = BREATH/LIP LEAN-IN (soft steady → growling
// breath + deeper vibrato — mirrors reed). Pitch: bore read length tracks freq*pitch_mul (§8.8.1), so
// vibrato / pitch-env / note_pitch / note_glide / the trombone slide all bend it live.
static inline float sound_brass_sample(Voice *v, float pitch_mul) {
    if (!v->br_on || v->br_len <= 0) return 0.0f;          // engine id without a note-on init
    const float TWO_PI = 6.2831853f;
    const float SR = (float)SOUND_SAMPLE_RATE;
    // macros → physical params (all pinned inside the self-oscillation window)
    float dark   = v->harm;                                // 0 bright trumpet → 1 dark tuba
    float bright = v->timb;                                // brassiness amount
    float blow   = 0.42f + v->mor * 0.20f;                 // breath pressure [0.42,0.62] (the oscillation window)
    float vibd   = 0.08f + v->mor * 0.35f;                 // lean-in vibrato deepens with breath
    float lpCoeff = 0.85f - dark * 0.35f;                  // bell radiation: bright trumpet (light LP) → dark tuba (heavy).
                                                           // FIXED per instrument — opening it with brassiness flipped the
                                                           // high register an octave down (subharmonic pop), so brightness
                                                           // is generated on the OUTPUT instead, where it can't destabilize.
    // the BRASS FORMANT (the lip+bell resonance, the fixed ~1–1.6kHz peak the ear reads as "horn").
    // Held STABLE per instrument — a formant that SWEEPS with the macro reads as a synth filter sweep
    // (a big part of the "synthy" tell); the brass bloom comes from harmonic GENERATION below, not a
    // moving resonance. Lower for the big dark horns.
    float fmtHz  = 900.0f + (1.0f - dark) * 700.0f;   // ~900 (tuba) .. 1600 (trumpet) Hz, fixed
    // HUMANIZED lip vibrato — wandering rate/depth, lives mostly in PITCH (reed/pipe/bowed pattern)
    v->br_drift_ph += 0.7f / SR; if (v->br_drift_ph >= 1.0f) v->br_drift_ph -= 1.0f;
    float wob = sinf(v->br_drift_ph * TWO_PI);             // slow wander, shared by rate + depth
    v->br_vib_ph += (5.4f + 0.9f * wob) / SR; if (v->br_vib_ph >= 1.0f) v->br_vib_ph -= 1.0f;
    float vib = sinf(v->br_vib_ph * TWO_PI) * vibd * (0.8f + 0.2f * wob);
    // breath turbulence — the "air" cue (reed pattern); resonate filtered noise through the bore + drift
    v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
    float wn = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
    v->br_noise_lp += 0.5f * (wn - v->br_noise_lp);
    v->br_drift += 0.0007f * (wn - v->br_drift);
    float air = 0.05f + v->mor * 0.08f;
    if (v->br_attack > 0) {                                // the speak transient: a breath burst at onset
        air += 0.35f * (float)v->br_attack / (0.018f * SR);
        v->br_attack--;
    }
    // mouth pressure: steady blow + a little vibrato + a slow drift + the resonated breath noise
    float Pm = blow + vib * 0.030f + v->br_drift * blow * 0.04f + blow * air * v->br_noise_lp;
    // bore delay: fractional read length tracks live pitch + pitch vibrato + cart glide/LFO (§8.8.1)
    float curf = v->freq * pitch_mul * (1.0f + vib * 0.010f); if (curf < 20.0f) curf = 20.0f;
    float effLen = (float)v->br_len * v->br_initfreq / curf;
    if (effLen < 2.0f) effLen = 2.0f;
    if (effLen > (float)v->br_len) effLen = (float)v->br_len;
    float readPos = (float)v->br_idx - effLen;
    while (readPos < 0.0f) readPos += (float)v->br_len;
    int i0 = (int)readPos; if (i0 >= v->br_len) i0 -= v->br_len;
    int i1 = (i0 + 1 < v->br_len) ? i0 + 1 : 0;
    float fr = readPos - floorf(readPos);
    float boreReturn = v->ks_buf[i0] + fr * (v->ks_buf[i1] - v->ks_buf[i0]);
    // ── LIP VALVE (self-oscillating core, the McIntyre/Schumacher pressure-controlled valve that
    // REED uses — it reliably self-oscillates, so brass borrows it). A bell-radiation LP smooths
    // the returning wave, the open bell inverts it, and the lip reflection is PRESSURE-DEPENDENT
    // (offset + slope·pressure-difference) — that pressure dependence is the negative resistance
    // that sustains oscillation. Brass differs from reed in the TIMBRE stages below, not here.
    v->br_lp += lpCoeff * (boreReturn - v->br_lp);         // bell radiation LP on the returning wave
    float endRefl = -0.96f - dark * 0.03f;                 // open-bell inversion (low loss → loop gain > 1, it sounds)
    float pdiff   = endRefl * v->br_lp - Pm;               // pressure across the lips
    float lipRefl = 0.50f - 0.70f * pdiff;                 // pressure-dependent lip reflection — the negative
    if (lipRefl < -1.0f) lipRefl = -1.0f; else if (lipRefl > 1.0f) lipRefl = 1.0f;   // resistance that self-oscillates
    float boreInput = Pm + pdiff * lipRefl;
    // amplitude limiter for the oscillation — a FIXED knee (timbre-INDEPENDENT) so the loop stays
    // stable and its amplitude is consistent at every macro position. All the brass timbre now
    // happens on the OUTPUT (below), where it can be aggressive without risking the oscillation.
    boreInput = tanhf(boreInput * 2.6f) / 2.6f;
    v->ks_buf[v->br_idx] = boreInput;
    v->br_idx++; if (v->br_idx >= v->br_len) v->br_idx = 0;
    // DC block — steady blow pressure carries a large DC (reed pattern, identical coeff)
    float out = boreReturn;
    float dc  = out - v->br_dc_prev + 0.995f * v->br_dc_state;
    v->br_dc_prev = out; v->br_dc_state = dc;
    // ── BRASS FORMANT: a resonant bandpass at the fixed fmtHz (the lip+bell resonance), reusing the
    // lip-biquad state. Zeros at ±1, peak gain ≈1 (b0 = 0.5−0.5·r², b2 = −b0). Emphasized strongly —
    // the prominent ~1.2kHz peak is a big part of what the ear hears as "a horn."
    float wf = TWO_PI * fmtHz / SR; if (wf > 2.6f) wf = 2.6f;     // Nyquist guard
    float rf = 0.972f;
    float a2f = rf * rf;
    float a1f = -2.0f * rf * cosf(wf);
    float b0f = 0.5f - 0.5f * a2f;
    float fmt = b0f * (dc - v->br_lip_x2) - a1f * v->br_lip_y1 - a2f * v->br_lip_y2;
    v->br_lip_x2 = v->br_lip_x1; v->br_lip_x1 = dc;
    v->br_lip_y2 = v->br_lip_y1; v->br_lip_y1 = fmt;
    // ── BRASSINESS: the bloom comes mostly from EMPHASIZING the brass formant (a peaky ~1.2kHz
    // resonance), which raises the spectral centroid and gives the "honk" while keeping the wave
    // PEAKY (high crest, like a real horn) — unlike a saturating waveshaper, which flattens the wave
    // into a hollow buzz (itself a synthy tell). The formant amount grows with brassiness, so pushing
    // TIMBRE blares harder. A GENTLE waveshaper on top adds a little buzz + lets level rise with breath.
    float voiced   = dc + fmt * (1.3f + bright * 2.6f);      // formant emphasis blooms with brassiness
    float driveOut = 1.0f + bright * (2.2f + blow * 4.0f);   // shaping grows w/ brassiness + breath
    // ASYMMETRIC steepening — a real brass shock wave is lopsided (the compression front is steeper
    // than the rarefaction). A plain tanh is an ODD nonlinearity, so it only ever makes ODD harmonics
    // → the spectrum stays clarinet-like (hollow, no even partials, doesn't read as "brass"). Biasing
    // the tanh makes it clip harder on one side, which FILLS IN the even harmonics that the ear hears
    // as brass. The bias grows with brassiness; subtracting tanh(asym) keeps it from adding a standing
    // DC offset (so a quiet/mellow voice stays clean).
    float shaped   = voiced * driveOut;
    float asym     = bright * 0.7f;
    float blaat    = tanhf(shaped + asym) - tanhf(asym);
    float comp     = 1.0f / (0.72f + 0.28f * driveOut);      // PARTIAL gain comp (not the old level-killing /drive)
    // block the DC the asymmetric shaper injects (a brass shock wave is lopsided → a standing offset →
    // a thump on note-on + wasted headroom). Same one-pole as the drive effect / epiano nonlinearity.
    float bdc = blaat - v->br_out_prev + 0.995f * v->br_out_state;
    v->br_out_prev = blaat; v->br_out_state = bdc;
    blaat = bdc;
    // makeup: keep loudness roughly even across the BORE axis (trumpet↔tuba) — §8.8.1 for harmonics
    return blaat * comp * (2.7f + dark * 0.7f);
}

// ── INSTR_VOICE: formant voice (navkit VoicForm port; vowels-only for the voxlab prototype) ──
// A glottal source (Rosenberg polynomial pulse + aspiration breath) through four parallel SVF
// formant filters whose centre frequencies trace a continuous vowel path. The raw params
// (note_aux idx 0..6) are exposed FLAT so the voxlab cart can audition which three deserve
// to become the public harmonics/timbre/morph macros. navkit crib: processVoicFormOscillator /
// vfPhonemeTable (navkit/soundsystem/engines/synth_oscillators.h). The formant SVF here is the
// same Chamberlin topology as navkit's processFormantFilter and dreamengine's own filter().
#define VOX_NPARAM 19   // 0 vowel·1 size·2 breath·3 openq·4 tilt·5 vibDepth·6 vibRate · 7 nasality·8 openness(F1)·9 frontness(F2) + EXPERIMENTAL 10 buzz·11 jitter·12 shimmer·13 creak·14 nasalAF·15 reduce·16 measBW·17 vow2(diphthong target 0..9)·18 glide(0=primary→1=at vow2)
_Static_assert(VOX_NPARAM <= 20, "grow Voice.vox_p[]/vox_s[] (sized [20]) before raising VOX_NPARAM");
// vowel table — navkit's full 10 vfPhonemeTable vowel rows (Klatt 1980 / Peterson-Barney 1952):
// F1..F4 (Hz) + relative amps + measured bandwidths. Rows 0..4 are the U→O→A→E→I path the idx-0
// `vowel` macro morphs along (close-back → open → close-front, UNCHANGED — vox/voxlab/say feel as
// before); rows 5..9 (AE AH AW UH ER) are the extra vowels, reachable as the diphthong/glide
// TARGET (vow2 idx 17 selects 0..9). Completing the table was the navkit copy-paste the audit flagged.
#define VOX_NVOWEL 10
static const float vox_vowel_f[VOX_NVOWEL][4] = {
    { 300.0f,  870.0f, 2240.0f, 3400.0f},   // 0 U  "oo" (boot)   ── path ──
    { 570.0f,  840.0f, 2410.0f, 3400.0f},   // 1 O  "oh" (go)
    { 730.0f, 1090.0f, 2440.0f, 3400.0f},   // 2 A  "ah" (father)
    { 530.0f, 1840.0f, 2480.0f, 3400.0f},   // 3 E  "eh" (bed)
    { 270.0f, 2290.0f, 3010.0f, 3400.0f},   // 4 I  "ee" (see)
    { 660.0f, 1720.0f, 2410.0f, 3400.0f},   // 5 AE "ae" (cat)    ── glide targets ──
    { 520.0f, 1190.0f, 2390.0f, 3400.0f},   // 6 AH "uh" (cup)
    { 570.0f,  840.0f, 2410.0f, 3400.0f},   // 7 AW "aw" (law)
    { 440.0f, 1020.0f, 2240.0f, 3400.0f},   // 8 UH "u"  (book)
    { 490.0f, 1350.0f, 1690.0f, 3400.0f},   // 9 ER "er" (bird, rhotic)
};
static const float vox_vowel_a[VOX_NVOWEL][4] = {
    {1.0f, 0.30f, 0.15f, 0.08f},  // U
    {1.0f, 0.40f, 0.20f, 0.10f},  // O
    {1.0f, 0.50f, 0.30f, 0.10f},  // A
    {1.0f, 0.70f, 0.30f, 0.10f},  // E
    {1.0f, 0.50f, 0.20f, 0.10f},  // I
    {1.0f, 0.60f, 0.30f, 0.10f},  // AE
    {1.0f, 0.45f, 0.25f, 0.10f},  // AH
    {1.0f, 0.40f, 0.20f, 0.10f},  // AW
    {1.0f, 0.40f, 0.20f, 0.08f},  // UH
    {1.0f, 0.50f, 0.40f, 0.10f},  // ER
};
// measured bandwidths (Hz) — navkit's BW rows. Used when measBW (idx 16) is up, to A/B against
// the derived bw = 60 + fc*0.08 approximation — a free vowel-authenticity audition.
static const float vox_vowel_bw[VOX_NVOWEL][4] = {
    { 65.0f,  90.0f, 130.0f, 170.0f},   // U
    { 80.0f, 100.0f, 140.0f, 180.0f},   // O
    { 90.0f, 110.0f, 140.0f, 180.0f},   // A
    { 70.0f, 100.0f, 130.0f, 170.0f},   // E
    { 60.0f,  90.0f, 120.0f, 170.0f},   // I
    { 80.0f, 110.0f, 140.0f, 180.0f},   // AE
    { 75.0f, 100.0f, 130.0f, 170.0f},   // AH
    { 80.0f, 100.0f, 140.0f, 180.0f},   // AW
    { 70.0f,  95.0f, 130.0f, 170.0f},   // UH
    { 75.0f, 100.0f, 120.0f, 170.0f},   // ER
};

// Configure a bus's FORMANT filter from the U→I vowel path (the public formant()/instrument_formant()).
// vowel 0..1 sweeps U→O→A→E→I (the first 5 rows above, same path INSTR_VOICE's vowel macro walks);
// q 0..1 narrows the formant peaks (broad → pronounced/nasal); mix 0..1 (0 = off). Interpolates the
// measured vowel table into the 4 band TARGETS that the table-free formant_process (near wah) reads.
// Lives HERE, after the tables — formant_process sits before apply_insert and stays table-free.
static void fx_set_formant(int b, float vowel, float q, float mix) {
    if (vowel < 0.0f) vowel = 0.0f; if (vowel > 1.0f) vowel = 1.0f;
    if (q < 0.0f) q = 0.0f; if (q > 1.0f) q = 1.0f;
    if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f;
    float vpos = vowel * 4.0f;                  // 0..4 along the first 5 vowels (U O A E I)
    int vi = (int)vpos; if (vi > 3) vi = 3; if (vi < 0) vi = 0;
    float fr = vpos - (float)vi;
    float bw_scale = 1.0f / (1.0f + q * 3.0f);  // q 0→1: bandwidth ×1.0 → ×0.25 (narrower = more vocal)
    for (int i = 0; i < 4; i++) {
        float fc = vox_vowel_f[vi][i]  * (1.0f - fr) + vox_vowel_f[vi + 1][i]  * fr;
        float am = vox_vowel_a[vi][i]  * (1.0f - fr) + vox_vowel_a[vi + 1][i]  * fr;
        float bw = vox_vowel_bw[vi][i] * (1.0f - fr) + vox_vowel_bw[vi + 1][i] * fr;
        bw *= bw_scale;
        float k = bw / fc; if (k < 0.02f) k = 0.02f; if (k > 2.0f) k = 2.0f;   // k = 1/Q (damping)
        fmt_freq[b][i] = fc;
        fmt_k[b][i]    = k;
        fmt_amp[b][i]  = am;
    }
    fmt_mix[b] = mix;
    fmt_used[b] = (mix > 0.0f);
}

// consonant ONSET table (articulation): a note can BEGIN with a brief consonant that morphs
// into the held vowel — "bah", "mah", "sss-ah". Each row: F1..F4 + amps (navkit vfPhonemeTable
// consonant rows), a noise gain (fricative hiss / plosive burst), a voiced flag (nasals/voiced
// plosives self-oscillate; unvoiced fricatives only voice IN as they open to the vowel), the
// onset duration (s), and a plosive flag (a sharp noise pop in the first ~12 ms). Set per note
// with voice_consonant(handle, id); idx -1 = none (a clean vowel attack). NOT a continuous axis
// — consonants are timed events, so they fire at note-on, not a held slider.
// ids 0..7 are FROZEN (vox/say/voxlab reference them by number); 8..21 are the rest of navkit's
// vfPhonemeTable consonant rows, appended so the set is complete (the audit's "fuller consonants").
enum { VC_B, VC_D, VC_G, VC_M, VC_N, VC_L, VC_S, VC_SH,
       VC_NG, VC_R, VC_W, VC_Y, VC_DH, VC_F, VC_V, VC_Z, VC_ZH, VC_TH, VC_P, VC_T, VC_K, VC_CH,
       VC_COUNT };
static const char *vox_cons_name[VC_COUNT] = { "b", "d", "g", "m", "n", "l", "s", "sh",
    "ng", "r", "w", "y", "dh", "f", "v", "z", "zh", "th", "p", "t", "k", "ch" };
static const float vox_cons_f[VC_COUNT][4] = {
    { 300.0f, 1100.0f, 2500.0f, 3400.0f},   // b — labial plosive
    { 300.0f, 1700.0f, 2500.0f, 3400.0f},   // d — alveolar plosive
    { 300.0f, 1700.0f, 2700.0f, 3400.0f},   // g — velar plosive
    { 200.0f, 1200.0f, 2500.0f, 3400.0f},   // m — labial nasal (low F1 hum)
    { 200.0f, 1700.0f, 2500.0f, 3400.0f},   // n — alveolar nasal
    { 350.0f, 1050.0f, 2400.0f, 3400.0f},   // l — lateral liquid
    { 300.0f, 1700.0f, 5000.0f, 7000.0f},   // s — alveolar fricative (high hiss)
    { 300.0f, 1700.0f, 3800.0f, 6000.0f},   // sh — postalveolar fricative
    { 200.0f, 1700.0f, 2700.0f, 3400.0f},   // ng — velar nasal
    { 420.0f, 1300.0f, 1600.0f, 3400.0f},   // r — rhotic approximant
    { 300.0f,  610.0f, 2200.0f, 3400.0f},   // w — labiovelar glide
    { 260.0f, 2070.0f, 3020.0f, 3400.0f},   // y — palatal glide
    { 300.0f, 1700.0f, 2500.0f, 3400.0f},   // dh — voiced dental fricative (this/that)
    { 300.0f, 1700.0f, 3500.0f, 5500.0f},   // f — labiodental fricative (unvoiced)
    { 300.0f, 1700.0f, 3500.0f, 5500.0f},   // v — labiodental fricative (voiced)
    { 300.0f, 1700.0f, 5000.0f, 7000.0f},   // z — alveolar fricative (voiced)
    { 300.0f, 1700.0f, 3800.0f, 6000.0f},   // zh — postalveolar fricative (voiced, "measure")
    { 300.0f, 1700.0f, 4500.0f, 6500.0f},   // th — dental fricative (unvoiced, "thin")
    { 300.0f, 1100.0f, 2500.0f, 3400.0f},   // p — labial plosive (unvoiced)
    { 300.0f, 1700.0f, 3500.0f, 5500.0f},   // t — alveolar plosive (unvoiced)
    { 300.0f, 1700.0f, 3000.0f, 4500.0f},   // k — velar plosive (unvoiced)
    { 300.0f, 1700.0f, 3800.0f, 6000.0f},   // ch — affricate (unvoiced)
};
static const float vox_cons_a[VC_COUNT][4] = {
    {0.30f, 0.20f, 0.10f, 0.05f}, // b
    {0.20f, 0.25f, 0.15f, 0.05f}, // d
    {0.20f, 0.20f, 0.20f, 0.05f}, // g
    {0.30f, 0.15f, 0.10f, 0.05f}, // m
    {0.30f, 0.20f, 0.10f, 0.05f}, // n
    {0.60f, 0.30f, 0.20f, 0.10f}, // l
    {0.05f, 0.05f, 0.30f, 0.40f}, // s
    {0.05f, 0.05f, 0.35f, 0.30f}, // sh
    {0.25f, 0.15f, 0.10f, 0.05f}, // ng
    {0.50f, 0.30f, 0.30f, 0.10f}, // r
    {0.50f, 0.20f, 0.10f, 0.05f}, // w
    {0.50f, 0.30f, 0.15f, 0.05f}, // y
    {0.40f, 0.20f, 0.15f, 0.05f}, // dh
    {0.10f, 0.10f, 0.20f, 0.15f}, // f
    {0.20f, 0.15f, 0.20f, 0.10f}, // v
    {0.15f, 0.10f, 0.25f, 0.30f}, // z
    {0.15f, 0.10f, 0.30f, 0.20f}, // zh
    {0.08f, 0.08f, 0.15f, 0.10f}, // th
    {0.10f, 0.10f, 0.05f, 0.02f}, // p
    {0.10f, 0.10f, 0.15f, 0.10f}, // t
    {0.10f, 0.15f, 0.15f, 0.08f}, // k
    {0.10f, 0.10f, 0.25f, 0.20f}, // ch
};
static const float vox_cons_noise[VC_COUNT] = { 0.30f, 0.40f, 0.35f, 0.0f, 0.0f, 0.0f, 0.90f, 0.85f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.15f, 0.50f, 0.35f, 0.60f, 0.50f, 0.40f, 0.60f, 0.70f, 0.60f, 0.80f };
static const int   vox_cons_voiced[VC_COUNT] = { 1, 1, 1, 1, 1, 1, 0, 0,
    1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0 };
static const float vox_cons_dur[VC_COUNT]    = { 0.055f, 0.055f, 0.055f, 0.090f, 0.090f, 0.070f, 0.110f, 0.110f,
    0.090f, 0.070f, 0.070f, 0.070f, 0.090f, 0.110f, 0.100f, 0.110f, 0.110f, 0.110f, 0.055f, 0.055f, 0.055f, 0.090f };
static const int   vox_cons_plos[VC_COUNT]   = { 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 };

// PUBLIC API mapping — the 3 macros → the voice's raw params (the locked lean API):
//   harmonics = VOWEL · timbre = SIZE · morph = EFFORT (one clean knob → breath + open-q + tilt).
// Written from the macro TARGETS whenever they change (note-on + note_harmonics/timbre/morph), so a
// macro-driven cart needs no note_aux(). The probe carts instead poke vox_p directly via
// note_aux() and leave the macros at default, so the two surfaces don't fight.
static void vox_apply_macros(Voice *v) {
    float e = v->mor_target;                            // EFFORT 0..1
    v->vox_p[0] = v->harm_target;                       // VOWEL  ← harmonics
    v->vox_p[1] = v->timb_target;                       // SIZE   ← timbre
    v->vox_p[2] = 0.22f - 0.22f * e;                    // breath  0.22 → 0    (light; the clean curve)
    v->vox_p[3] = 0.85f - 0.65f * e;                    // open-q  0.85 → 0.20
    v->vox_p[4] = 0.70f - 0.65f * e;                    // tilt    0.70 → 0.05
}

// note-on: seed a neutral "ah" so a bare note(60, INSTR_VOICE, …) speaks; note_aux overrides live
static void sound_voice_start(Voice *v) {
    const float def[VOX_NPARAM] = { 0.5f, 0.33f, 0.10f, 0.5f, 0.30f, 0.0f, 0.5f,
                                    0.0f, 0.5f, 0.5f,                              // nasality off · open/front neutral (×1.0)
                                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,      // buzz=full glottal · jitter/shimmer/creak/nasalAF/reduce/measBW off
                                    0.0f, 0.0f };                                  // vow2 target=U · glide off (no diphthong)
    for (int i = 0; i < VOX_NPARAM; i++) v->vox_p[i] = v->vox_s[i] = def[i];
    v->vox_glot_ph = v->vox_tilt_lp = v->vox_vib_ph = 0.0f;
    v->vox_jit_mul = v->vox_shim_mul = 1.0f; v->vox_creak_skip = 0;
    v->vox_naf_low = v->vox_naf_band = 0.0f;
    for (int i = 0; i < 4; i++) { v->vox_f_low[i] = 0.0f; v->vox_f_band[i] = 0.0f; }
    v->vox_cons = -1; v->vox_cons_t = 0.0f;
    v->vox_coda = -1; v->vox_coda_t = 0.0f;
    v->vox_on = true;
}

// One INSTR_VOICE sample. params (vox_s, all 0..1): 0 vowel (U→I morph) · 1 size (formant
// shift 0.5→2.0, the vocal-tract length / body) · 2 breath (aspiration noise) · 3 open-quotient
// (glottal pulse width: pressed→relaxed) · 4 spectral tilt (bright→dark) · 5 vibrato depth ·
// 6 vibrato rate (3→8 Hz). Pitch tracks freq*pitch_mul like every engine.
static inline float sound_voice_sample(Voice *v, float pitch_mul) {
    if (!v->vox_on) return 0.0f;
    const float SR = (float)SOUND_SAMPLE_RATE;
    const float PI_F = 3.14159265f, TWO_PI = 6.2831853f;
    // slew the raw params toward their targets (one-pole, ~5ms) — kills slider zipper
    for (int i = 0; i < VOX_NPARAM; i++) v->vox_s[i] += (v->vox_p[i] - v->vox_s[i]) * 0.004f;
    float vowel = v->vox_s[0] * 4.0f;            // 0..4 position along the 5-vowel path
    int vi = (int)vowel; if (vi > 3) vi = 3; if (vi < 0) vi = 0;
    float vf = vowel - (float)vi;                // morph fraction to the next vowel
    if (vf < 0.0f) vf = 0.0f; if (vf > 1.0f) vf = 1.0f;
    float shift  = 0.5f + v->vox_s[1] * 1.5f;    // formant shift 0.5..2.0
    float breath = v->vox_s[2];                  // aspiration 0..1
    float oq     = 0.25f + v->vox_s[3] * 0.62f;  // open quotient 0.25..0.87 (pressed/buzzy → relaxed/round)
    float tilt   = v->vox_s[4];                  // spectral tilt 0..1
    float vibd   = v->vox_s[5];                  // vibrato depth 0..1
    float vibr   = 3.0f + v->vox_s[6] * 5.0f;    // vibrato rate 3..8 Hz
    // EXPERIMENTAL roughness/character levers (10..16; auditioning in voxab — none wired to API):
    float buzz   = v->vox_s[10];                 // source: 1 = full glottal pulse (buzzy) → 0 = smooth sine
    float jitter = v->vox_s[11];                 // per-period pitch noise → roughness
    float shimmer= v->vox_s[12];                 // per-period amplitude noise
    float creak  = v->vox_s[13];                 // diplophonia: dropped/weak cycles → vocal fry
    float nasalAf= v->vox_s[14];                 // anti-formant nasality (350Hz notch; navkit WAVE_VOICE)
    float reduce = v->vox_s[15];                 // vowel reduction toward schwa (unstressed collapse)
    float bwmode = v->vox_s[16];                 // 0 = derived bandwidths → 1 = navkit measured BW table
    float glide  = v->vox_s[18];                 // diphthong: 0 = primary vowel → 1 = fully at vow2

    // vibrato → pitch (±~1 semitone at full depth)
    v->vox_vib_ph += vibr / SR; if (v->vox_vib_ph >= 1.0f) v->vox_vib_ph -= 1.0f;
    float vib = sinf(v->vox_vib_ph * TWO_PI) * vibd * 0.06f;
    float freq = v->freq * pitch_mul * powf(2.0f, vib);
    if (freq < 20.0f) freq = 20.0f;

    // vowel target formants (centre freq + amp + measured bandwidth, morphed along the path)
    float vfreq[4], vamp[4], vbw[4];
    for (int i = 0; i < 4; i++) {
        vfreq[i] = vox_vowel_f[vi][i]  * (1.0f - vf) + vox_vowel_f[vi+1][i]  * vf;
        vamp[i]  = vox_vowel_a[vi][i]  * (1.0f - vf) + vox_vowel_a[vi+1][i]  * vf;
        vbw[i]   = vox_vowel_bw[vi][i] * (1.0f - vf) + vox_vowel_bw[vi+1][i] * vf;
    }
    // EXPERIMENTAL diphthong (17/18): glide the primary vowel toward a second vowel from the FULL
    // 10-row table (vow2 selects 0..9). glide=1 sits fully on vow2 — so the extra vowels (AE AH AW
    // UH ER) are reachable as steady vowels too, not just as a within-note glide ("ai", "ou").
    if (glide > 0.001f) {
        float v2 = v->vox_s[17] * (float)(VOX_NVOWEL - 1);
        int v2i = (int)v2; if (v2i > VOX_NVOWEL - 2) v2i = VOX_NVOWEL - 2; if (v2i < 0) v2i = 0;
        float v2f = v2 - (float)v2i; if (v2f < 0.0f) v2f = 0.0f; if (v2f > 1.0f) v2f = 1.0f;
        for (int i = 0; i < 4; i++) {
            float f2 = vox_vowel_f[v2i][i]  * (1.0f - v2f) + vox_vowel_f[v2i+1][i]  * v2f;
            float a2 = vox_vowel_a[v2i][i]  * (1.0f - v2f) + vox_vowel_a[v2i+1][i]  * v2f;
            float b2 = vox_vowel_bw[v2i][i] * (1.0f - v2f) + vox_vowel_bw[v2i+1][i] * v2f;
            vfreq[i] = vfreq[i] * (1.0f - glide) + f2 * glide;
            vamp[i]  = vamp[i]  * (1.0f - glide) + a2 * glide;
            vbw[i]   = vbw[i]   * (1.0f - glide) + b2 * glide;
        }
    }
    // EXPERIMENTAL vowel reduction (15): collapse the vowel toward schwa (neutral central vowel) —
    // the unstressed-syllable lever the `say` probe wanted. 0 = full vowel, 1 = near-schwa.
    if (reduce > 0.001f) {
        const float sf[4] = { 490.0f, 1350.0f, 2400.0f, 3400.0f };   // schwa-ish (between UH and ER)
        float rb = reduce * 0.85f;
        for (int i = 0; i < 4; i++) vfreq[i] = vfreq[i] * (1.0f - rb) + sf[i] * rb;
    }
    // EXPERIMENTAL vowel modifiers (auditioning in voxlab; idx 7/8/9 — none wired to the API):
    //   nasality (7) — morph the vowel toward a nasal formant config (low F1, damped) → honk/hum.
    //   openness (8) → F1 and frontness (9) → F2: the 2D-vowel split, each on its own knob.
    //   0.5 = neutral (×1.0), so they leave the 1D-path vowel untouched until you push them —
    //   lets us hear whether the vowel wants to be a 2D plane and whether nasality earns an axis.
    float nasal = v->vox_s[7];
    if (nasal > 0.001f) {
        const float nf[4] = { 250.0f, 1100.0f, 2500.0f, 3400.0f };   // nasal target (M-ish, low F1)
        float nb = nasal * 0.75f;
        for (int i = 0; i < 4; i++) vfreq[i] = vfreq[i] * (1.0f - nb) + nf[i] * nb;
        vamp[0] *= (1.0f - nasal * 0.5f);                            // nasals damp the first formant
    }
    // octave-symmetric multiplier: ×1.0 at 0.5 (neutral, so the 1D vowel is untouched), out to
    // ≈×0.31 / ×3.25 at the edges (±1.7 octaves). Wide enough to reach inhuman/creature formant
    // placements, but controlled (centered, predictable) — not the old out-of-bounds chaos.
    vfreq[0] *= exp2f((v->vox_s[8] - 0.5f) * 3.4f);   // openness → F1
    vfreq[1] *= exp2f((v->vox_s[9] - 0.5f) * 3.4f);   // frontness → F2
    // consonant ONSET: for the first vox_cons_dur seconds of the note, blend the consonant's
    // formants → the vowel (smoothstep "opening"), inject its noise (hiss/burst), and gate the
    // glottal voicing in (unvoiced fricatives only voice up as they open). Then it's pure vowel.
    float gl_gain = 1.0f, con_noise = 0.0f;
    if (v->vox_cons >= 0 && v->vox_cons < VC_COUNT) {
        int c = v->vox_cons;
        float p = v->vox_cons_t / vox_cons_dur[c]; if (p > 1.0f) p = 1.0f;
        float op = p * p * (3.0f - 2.0f * p);             // 0 = full consonant → 1 = full vowel
        for (int i = 0; i < 4; i++) {
            vfreq[i] = vox_cons_f[c][i] * (1.0f - op) + vfreq[i] * op;
            vamp[i]  = vox_cons_a[c][i] * (1.0f - op) + vamp[i]  * op;
        }
        con_noise = vox_cons_noise[c] * (1.0f - op);                       // hiss fades as it opens
        if (vox_cons_plos[c] && v->vox_cons_t < 0.012f) con_noise += 0.8f; // plosive pop
        gl_gain = vox_cons_voiced[c] ? 1.0f
                : (p < 0.55f ? 0.0f : (p - 0.55f) / 0.45f);                // unvoiced: voice in late
        v->vox_cons_t += 1.0f / SR;
        if (v->vox_cons_t >= vox_cons_dur[c]) v->vox_cons = -1;            // onset done → pure vowel
    }
    // consonant CODA (the mirror of the onset): once voice_coda() fires near release, morph the
    // vowel → the consonant over its duration, so the note CLOSES on it — "ahh-m", "oo-d", "ah-ng".
    // Same table; op runs 1 (vowel) → 0 (consonant). Coda pool is voiced, so the glottis stays on.
    if (v->vox_coda >= 0 && v->vox_coda < VC_COUNT) {
        int c = v->vox_coda;
        float p = v->vox_coda_t / vox_cons_dur[c]; if (p > 1.0f) p = 1.0f;
        float op = 1.0f - p * p * (3.0f - 2.0f * p);     // 1 = full vowel → 0 = full consonant
        for (int i = 0; i < 4; i++) {
            vfreq[i] = vox_cons_f[c][i] * (1.0f - op) + vfreq[i] * op;
            vamp[i]  = vox_cons_a[c][i] * (1.0f - op) + vamp[i]  * op;
        }
        con_noise += vox_cons_noise[c] * (1.0f - op);    // closing hiss/voiced-burst, swells as it shuts
        v->vox_coda_t += 1.0f / SR;                      // (held at the consonant once p hits 1)
    }

    // glottal source: Rosenberg polynomial pulse (open-phase rise, closing fall, closed gap).
    // EXPERIMENTAL roughness: jitter perturbs the period pitch, creak attenuates the odd cycle
    // (diplophonia/fry), shimmer (applied at output) wobbles amplitude — all redrawn ONCE per
    // glottal period (on wrap) so they read as biological irregularity, not per-sample buzz.
    v->vox_glot_ph += freq * v->vox_jit_mul / SR;
    if (v->vox_glot_ph >= 1.0f) {
        v->vox_glot_ph -= 1.0f;
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float r1 = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;     // [-1,1]
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float r2 = ((v->noise_state >> 16) & 0xff) / 255.0f;            // [0,1]
        v->vox_jit_mul    = 1.0f + r1 * jitter * 0.055f;                // ±5.5% pitch at full
        v->vox_shim_mul   = 1.0f - r2 * shimmer * 0.6f;                 // amplitude dips
        v->vox_creak_skip = (creak > 0.01f && r2 < creak * 0.55f);      // weak/dropped cycle
    }
    float t = v->vox_glot_ph, gp;
    float te = oq + 0.1f; if (te > 0.95f) te = 0.95f;
    if (t < oq)      { float tn = t / oq;             gp = 3.0f*tn*tn - 2.0f*tn*tn*tn; }
    else if (t < te) { float tn = (t - oq)/(te - oq); gp = 0.5f*(1.0f + cosf(tn * PI_F)); }
    else             { gp = 0.0f; }
    if (v->vox_creak_skip) gp *= 0.12f;              // creak: this cycle nearly drops out
    // buzziness (10): blend the rich glottal pulse toward a smooth sine (navkit WAVE_VOICE knob)
    float src = sinf(v->vox_glot_ph * TWO_PI) * (1.0f - buzz) + gp * buzz;
    src *= gl_gain;                                  // consonant voicing gate
    // spectral tilt: 0 = bright (source bypass) → 1 = dark (heavy 1-pole LP). MONOTONIC —
    // the coefficient must DROP as tilt rises (lower cutoff = more darkening), then crossfade
    // fully to the filtered copy. (The earlier 0.2+tilt*0.75 form went transparent at tilt=1,
    // so it did nothing at the extremes — see the voxlab probe.)
    float tc = 1.0f - tilt * 0.93f;                 // 1.0 transparent (bright) → 0.07 heavy LP (dark)
    v->vox_tilt_lp += tc * (src - v->vox_tilt_lp);
    src = src * (1.0f - tilt) + v->vox_tilt_lp * tilt;
    // aspiration breath + consonant noise: mix white noise into the source
    v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
    float nz = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
    src = src * (1.0f - breath * 0.5f) + nz * (breath * 0.5f + con_noise);

    // 4 parallel SVF formants: centres (vowel, or consonant-blended) then size-shifted
    float out = 0.0f;
    for (int i = 0; i < 4; i++) {
        float fc_hz = vfreq[i] * shift;
        if (fc_hz > SR * 0.45f) fc_hz = SR * 0.45f;
        float amp = vamp[i];
        // bandwidth: derived (grows with centre) ↔ navkit's measured per-vowel BW (measBW idx 16)
        float bw  = (60.0f + fc_hz * 0.08f) * (1.0f - bwmode) + vbw[i] * bwmode;
        float f = 2.0f * sinf(PI_F * fc_hz / SR);
        if (f > 0.99f) f = 0.99f; else if (f < 0.001f) f = 0.001f;
        float q = fc_hz / (bw + 1.0f);
        if (q < 0.5f) q = 0.5f; else if (q > 20.0f) q = 20.0f;
        v->vox_f_low[i] += f * v->vox_f_band[i];
        float high = src - v->vox_f_low[i] - v->vox_f_band[i] / q;
        v->vox_f_band[i] += f * high;
        out += v->vox_f_band[i] * amp;
    }
    // EXPERIMENTAL anti-formant nasality (14): a ~350Hz notch + blend (navkit WAVE_VOICE's
    // physically-truer nasality, distinct from the formant-config morph on idx 7) — auditioning
    // which model earns the nasality axis.
    if (nasalAf > 0.001f) {
        float nfc = 350.0f * shift;
        float f = 2.0f * sinf(PI_F * nfc / SR); if (f > 0.99f) f = 0.99f; else if (f < 0.001f) f = 0.001f;
        float q = nfc / 101.0f; if (q < 0.5f) q = 0.5f; else if (q > 10.0f) q = 10.0f;
        v->vox_naf_low += f * v->vox_naf_band;
        float high = out - v->vox_naf_low - v->vox_naf_band / q;
        v->vox_naf_band += f * high;
        float notched = v->vox_naf_low + high;       // notch = low + high (the band removed)
        out = out * (1.0f - nasalAf) + notched * nasalAf;
    }
    return out * 0.8f * v->vox_shim_mul;              // shimmer: per-period amplitude wobble
}

// One engine sample — the dispatch (engine ids >= INSTR_ENGINE_BASE). The default body is
// PLUCK. pitch_mul carries the per-sample LFO/env pitch modulation; v->freq carries
// note_pitch/note_glide (slewed) — so the SAME pitch machinery every oscillator answers
// bends the string: the tap distance just tracks it. Linear-interpolated read; the
// interpolation's slight extra damping at fractional positions is natural string behavior.
// ── INSTR_GUITAR: bodied plucked string (§8.8.9) ─────────────────────────────
// PLUCK's Karplus-Strong string + a parallel body resonator (4 bandpass formants) + a DC
// blocker — the body is what bare PLUCK lacks. The string reuses ks_buf and the same fractional
// read tap as the pluck path, but GUITAR remaps the macros: harmonics = BODY, timbre = string
// brightness, morph = MUTE. navkit crib: processGuitarOscillator (synth_oscillators.h).

// RBJ constant-skirt bandpass — peak gain = `gain`, bandwidth `bw` in octaves. Resets state.
static void sound_biquad_set(SoundBiquad *bq, float fc, float bw, float gain) {
    float w0 = 6.2831853f * fc / (float)SOUND_SAMPLE_RATE;
    float sn = sinf(w0), cs = cosf(w0);
    float alpha = sn * sinhf(0.34657359f * bw * w0 / (sn > 1e-6f ? sn : 1e-6f));  // 0.3466 = ln2/2
    float a0 = 1.0f + alpha;
    bq->b0 = (alpha * gain) / a0;
    bq->b1 = 0.0f;
    bq->b2 = (-alpha * gain) / a0;
    bq->a1 = (-2.0f * cs) / a0;
    bq->a2 = (1.0f - alpha) / a0;
    bq->z1 = bq->z2 = 0.0f;
}
static inline float sound_biquad_run(SoundBiquad *bq, float in) {
    float out = bq->b0 * in + bq->z1;
    bq->z1 = bq->b1 * in - bq->a1 * out + bq->z2;
    bq->z2 = bq->b2 * in - bq->a2 * out;
    return out;
}

// note-on: excite the string (noise burst → timbre lowpass → fixed pick comb → DC-remove +
// normalize + tile, the pluck recipe) then voice the body resonator from harmonics.
static void sound_guitar_start(Voice *v) {
    int period = (int)((float)SOUND_SAMPLE_RATE / (v->freq > 20.0f ? v->freq : 20.0f));
    if (period < 2) period = 2;
    if (period > SOUND_KS_MAX) period = SOUND_KS_MAX;
    int alloc = period * 2 + 4;
    if (alloc > SOUND_KS_MAX) alloc = SOUND_KS_MAX;
    v->ks_len  = alloc;
    v->ks_widx = 0;
    v->ks_last = 0.0f;
    // timbre = string brightness: lowpass the noise burst (0 = warm nylon, 1 = bright steel)
    float k  = 0.04f + 0.96f * v->timb * v->timb;
    float lp = 0.0f;
    for (int i = 0; i < period; i++) {
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float n = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
        lp += k * (n - lp);
        v->ks_buf[i] = lp;
    }
    // fixed pick comb at ~1/4 string — pick position is baked here (morph carries mute, not pick pos)
    int pos = (int)(period * 0.25f);
    if (pos > 0) {
        float tmp[SOUND_KS_MAX];
        memcpy(tmp, v->ks_buf, period * sizeof(float));
        for (int i = 0; i < period; i++) v->ks_buf[i] = tmp[i] - 0.55f * tmp[(i + pos) % period];
    }
    float mean = 0.0f;
    for (int i = 0; i < period; i++) mean += v->ks_buf[i];
    mean /= (float)period;
    float peak = 0.0f;
    for (int i = 0; i < period; i++) { v->ks_buf[i] -= mean; float a = fabsf(v->ks_buf[i]); if (a > peak) peak = a; }
    if (peak > 0.0001f) { float g = 0.9f / peak; for (int i = 0; i < period; i++) v->ks_buf[i] *= g; }
    for (int i = period; i < alloc; i++) v->ks_buf[i] = v->ks_buf[i % period];   // tile
    // harmonics = body: lerp the 4 formant modes between an OPEN/dark anchor (harp/oud: broad, low,
    // quiet — minimal box) and a RESONANT/bright anchor (banjo/koto: sharp, high, loud).
    float h = v->harm;
    // mode 0 is the body Helmholtz (~110Hz, real guitar F#2–A2) — kept modest so it doesn't
    // become a sub lump; modes 2–3 carry the 1–3k presence (boosted vs the first pass).
    // mode 0–1 = body warmth (low-mid weight), modes 2–3 = 1–3k presence. Both kept up: warmth is
    // what stops it sounding thin; presence is what stops it sounding dull.
    static const float f_lo[4]  = { 110.0f, 220.0f, 420.0f,  800.0f };
    static const float f_hi[4]  = { 180.0f, 480.0f, 950.0f, 2000.0f };
    static const float bw_lo[4] = { 0.9f, 0.8f, 0.6f, 0.5f };
    static const float bw_hi[4] = { 0.7f, 0.4f, 0.3f, 0.4f };
    static const float g_lo[4]  = { 0.45f, 0.45f, 0.35f, 0.28f };
    static const float g_hi[4]  = { 0.60f, 0.65f, 0.55f, 0.45f };
    for (int i = 0; i < 4; i++)
        sound_biquad_set(&v->gt_body[i], f_lo[i] + (f_hi[i] - f_lo[i]) * h,
                         bw_lo[i] + (bw_hi[i] - bw_lo[i]) * h, g_lo[i] + (g_hi[i] - g_lo[i]) * h);
    v->gt_bodymix = 0.30f + 0.55f * h;       // open harp (some body) → boxy banjo (lots of body)
    v->gt_dc_prev = v->gt_dc_state = 0.0f;
    v->eng_subph  = v->eng_env = 0.0f;
    v->eng_click  = 700;
    v->gt_on = true;
}

// per-sample: KS string with morph-driven decay (mute) → parallel body resonator → DC blocker.
static inline float sound_guitar_sample(Voice *v, float pitch_mul) {
    int alloc = v->ks_len;
    if (alloc < 4 || !v->gt_on) return 0.0f;   // engine id without a note-on init → silent
    float f = v->freq * pitch_mul;
    if (f < 20.0f) f = 20.0f;
    float len_f = (float)SOUND_SAMPLE_RATE / f;          // the tap distance IS the pitch
    if (len_f > (float)alloc - 2.0f) len_f = (float)alloc - 2.0f;
    if (len_f < 2.0f) len_f = 2.0f;
    // morph = mute: T60 from a long open ring (mor=0, ~6s) to a tight pizzicato stop (mor=1, ~80ms)
    float t60 = 0.08f * expf((1.0f - v->mor) * 4.3f);
    float fb  = expf(-6.9078f / (t60 * f));
    float rpos = (float)v->ks_widx - len_f;
    if (rpos < 0.0f) rpos += (float)alloc;
    int   i0 = (int)rpos;        if (i0 >= alloc) i0 = 0;
    int   i1 = i0 + 1;           if (i1 >= alloc) i1 = 0;
    float fr  = rpos - (float)i0;
    float dry = v->ks_buf[i0] + (v->ks_buf[i1] - v->ks_buf[i0]) * fr;
    // loop filter: the KS damping average, but timbre keeps the upper harmonics (1–3k presence)
    // alive instead of nuking them every pass — the cure for the "thin/dull" spectral hump
    float lpf  = (dry + v->ks_last) * 0.5f;
    float loop = (lpf + (dry - lpf) * (0.40f + 0.50f * v->timb)) * fb;
    v->ks_buf[v->ks_widx] = loop;
    v->ks_last = dry;
    if (++v->ks_widx >= alloc) v->ks_widx = 0;
    // body resonator: 4 parallel bandpass formants, blended in by harmonics (gt_bodymix)
    float body = 0.0f;
    for (int i = 0; i < 4; i++) body += sound_biquad_run(&v->gt_body[i], dry);
    body *= 0.35f;
    float mix = dry + v->gt_bodymix * (body - dry * 0.3f);
    // DC blocker (long-sustain bodies + the body filters can drift DC)
    float dc = mix - v->gt_dc_prev + 0.990f * v->gt_dc_state;   // ~70Hz high-pass: trims sub mud
    v->gt_dc_prev  = mix;
    v->gt_dc_state = dc;
    // fundamental reinforcement: a sine at the note pitch, envelope-following the string, mixed
    // under it for low-end weight (anti-thin). weight = v->duty while tuning live, then baked.
    float aenv = fabsf(dry);
    v->eng_env = aenv > v->eng_env ? aenv : v->eng_env * 0.9997f;
    v->eng_subph += f / (float)SOUND_SAMPLE_RATE;
    if (v->eng_subph >= 1.0f) v->eng_subph -= 1.0f;
    float tri = 4.0f * fabsf(v->eng_subph - 0.5f) - 1.0f;   // triangle: fuller + less phase-cancel than a sine
    float out = dc + v->eng_p[0] * 0.8f * v->eng_env * tri;
    if (v->eng_click > 0) {                                 // onset pick transient (attack), eng_p[1]
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float n  = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
        float ce = (float)v->eng_click / 700.0f;
        out += v->eng_p[1] * 1.3f * n * ce * ce;
        v->eng_click--;
    }
    return out;
}

// ── INSTR_PIANO: struck stiff string (StifKarp, §8.8.9) ──────────────────────
// Our KS string + a DISPERSION allpass chain (stretches the upper partials sharp — the
// inharmonic metallic shimmer that reads as "piano", the stiff-string magic) + a baked
// grand-piano soundboard (4 body biquads) + a DC blocker. macros: harmonics = stiffness,
// timbre = hammer, morph = pedal. navkit crib: processStifKarpOscillator. Single-string v1
// (double-string detune + prepared-piano buzz deferred, like guitar's jawari).

// The StifKarp voicings, ported faithfully from navkit (initStifKarpPreset + the 218–222 patches).
// harmonics SNAPS to one of these (the "which keyboard" axis); timbre modulates the hammer, morph
// the pedal/sustain. Each field is a navkit param so each voicing reproduces navkit's sound.
typedef struct {
    float fF[4], fBW[4], fG[4];   // soundboard formants: center Hz, bandwidth (oct), gain
    float loopco;                 // output tone-filter coeff (dark → bright)
    float hammer;                 // base hammer hardness (timbre modulates around it)
    float strike;                 // strike position (excitation comb)
    float stiff;                  // dispersion / inharmonicity 0..1
    float bodymix;                // dry string ↔ soundboard
    float ring;                   // base ring time (s) at full pedal
    float symp;                   // sympathetic-resonance level
    float detune;                 // 2nd-string detune ratio (1.0 = single string)
    const char *name;
} PianoVoicing;
static const PianoVoicing PIANO_V[6] = {
    { {110,200,440,1800},{0.8f,0.7f,0.5f,0.4f}, {0.35f,0.55f,0.50f,0.25f}, 0.25f,0.45f,0.12f,0.25f,0.55f,4.0f,0.15f,1.000694f,"grand" },
    { {110,200,480,2200},{0.8f,0.7f,0.5f,0.4f}, {0.35f,0.60f,0.55f,0.35f}, 0.35f,0.65f,0.11f,0.30f,0.55f,5.0f,0.20f,1.000462f,"bright" },
    { {200,400,900,1800},{0.5f,0.4f,0.3f,0.3f}, {0.80f,0.60f,0.40f,0.20f}, 0.70f,0.92f,0.08f,0.15f,0.25f,1.4f,0.00f,1.0f,     "harpsi" },
    { {180,360,720,1500},{0.6f,0.5f,0.4f,0.3f}, {1.00f,0.70f,0.45f,0.25f}, 0.50f,0.70f,0.15f,0.40f,0.75f,3.0f,0.25f,1.0f,     "dulcimer" },
    { {150,300,600,1200},{0.8f,0.6f,0.4f,0.3f}, {0.50f,0.30f,0.15f,0.08f}, 0.10f,0.20f,0.50f,0.10f,0.18f,1.6f,0.05f,1.0f,     "clavichord" },
    { {400,800,1600,3200},{0.4f,0.3f,0.3f,0.2f},{0.80f,0.60f,0.40f,0.20f}, 0.60f,0.35f,0.25f,0.55f,0.45f,5.0f,0.10f,1.0f,     "celesta" },
};
// per-voicing string brightness (navkit pluckBrightness — high-harmonic RETENTION, the presence
// lever) + damping (pluckDamping ≈ 0.999 — a near-lossless string; the note decays via the AMP
// ENVELOPE/gate, NOT the loop, which is what keeps the upper harmonics alive).
static const float PIANO_BRIGHT[6] = { 0.55f, 0.70f, 0.85f, 0.65f, 0.35f, 0.60f };
static const float PIANO_DAMP[6]   = { 0.9992f, 0.9994f, 0.9970f, 0.9988f, 0.9980f, 0.9990f };
static const float PIANO_BODYB[6]  = { 0.50f, 0.65f, 0.70f, 0.60f, 0.35f, 0.50f };   // bodyBrightness (scales soundboard gains)

// note-on — navkit playStifKarp verbatim: ONE-period KS buffer + fractional-delay allpass tuning,
// hammer-shaped excitation, AVERAGING strike comb, per-voicing brightness/damping, dispersion,
// soundboard, optional detuned 2nd string.
static void sound_piano_start(Voice *v) {
    int vi = (int)(v->harm * 5.999f);
    if (vi < 0) vi = 0; if (vi > 5) vi = 5;
    const PianoVoicing *pv = &PIANO_V[vi];
    float freq = v->freq > 20.0f ? v->freq : 20.0f;

    float ideal = (float)SOUND_SAMPLE_RATE / freq;     // one period, allpass for the remainder
    int len = (int)ideal;
    if (len > SOUND_KS_MAX - 1) len = SOUND_KS_MAX - 1;
    if (len < 2) len = 2;
    float frac = ideal - (float)len;
    v->ks_len = len; v->ks_widx = 0; v->ks_last = 0.0f;
    v->pn_apc = (1.0f - frac) / (1.0f + frac); v->pn_aps = 0.0f;
    v->pn_initf = freq;
    v->pn_ksb   = PIANO_BRIGHT[vi];
    v->pn_ksb_cur = PIANO_BRIGHT[vi] + 0.35f;          // strike bright, then bloom down to pn_ksb
    if (v->pn_ksb_cur > 0.97f) v->pn_ksb_cur = 0.97f;
    v->pn_ksd   = PIANO_DAMP[vi];
    v->pn_dampg = v->pn_damps = v->mor;                // pedal

    // excitation: noise burst → hammer lowpass (timbre modulates the voicing's hardness) → normalize
    float hard = pv->hammer + (v->timb - 0.5f) * 0.6f;
    if (hard < 0.02f) hard = 0.02f; if (hard > 0.98f) hard = 0.98f;
    float cut = 0.05f + 0.85f * hard, lp = 0.0f;
    for (int i = 0; i < len; i++) {
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float n = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
        lp += cut * (n - lp);
        v->ks_buf[i] = lp;
    }
    float mean = 0.0f;                                 // remove excitation DC — else the near-lossless
    for (int i = 0; i < len; i++) mean += v->ks_buf[i]; // loop sustains it into a sub-bass boom
    mean /= (float)len;
    float peak = 0.0f;
    for (int i = 0; i < len; i++) { v->ks_buf[i] -= mean; float a = fabsf(v->ks_buf[i]); if (a > peak) peak = a; }
    if (peak > 0.001f) { float g = 1.0f / peak; for (int i = 0; i < len; i++) v->ks_buf[i] *= g; }
    int ps = (int)(pv->strike * (float)len);           // strike comb (AVERAGING — navkit applyPickPosition)
    if (ps < 1) ps = 1; if (ps >= len) ps = len - 1;
    { float tmp[SOUND_KS_MAX];
      memcpy(tmp, v->ks_buf, len * sizeof(float));
      for (int i = 0; i < len; i++) v->ks_buf[i] = (tmp[i] + tmp[(i + ps) % len]) * 0.5f; }

    // dispersion (inharmonicity) — per voicing
    float B = pv->stiff * pv->stiff * 0.015f;
    float fscale = 1.0f + (freq - 261.0f) / 2000.0f;
    if (fscale < 0.5f) fscale = 0.5f; if (fscale > 3.0f) fscale = 3.0f;
    B *= fscale;
    v->pn_disp_n = (pv->stiff < 0.1f) ? 1 : (pv->stiff < 0.4f) ? 2 : (pv->stiff < 0.7f) ? 3 : 4;
    for (int i = 0; i < 4; i++) {
        float pt = B * (float)(i + 1) * freq / (float)SOUND_SAMPLE_RATE;
        if (pt > 0.9f) pt = 0.9f;
        v->pn_disp_c[i] = (i < v->pn_disp_n) ? (1.0f - pt) / (1.0f + pt) : 0.0f;
        v->pn_disp_s[i] = 0.0f;
    }
    float bbg = 0.5f + 0.5f * PIANO_BODYB[vi];   // navkit scales soundboard gains by bodyBrightness
    for (int i = 0; i < 4; i++) sound_biquad_set(&v->pn_body[i], pv->fF[i], pv->fBW[i], pv->fG[i] * bbg);
    v->pn_bodymix = pv->bodymix;
    v->pn_loopco  = pv->loopco;
    v->pn_loop_lp = 0.0f;
    v->pn_symp    = pv->symp;
    v->pn_detune  = pv->detune;
    v->pn_dc_prev = v->pn_dc_state = 0.0f;
    if (pv->detune > 1.00001f) {                       // 2nd string: own detuned length + allpass
        float ideal2 = (float)SOUND_SAMPLE_RATE / (freq * pv->detune);
        int len2 = (int)ideal2;
        if (len2 > SOUND_KS_MAX - 1) len2 = SOUND_KS_MAX - 1;
        if (len2 < 2) len2 = 2;
        float frac2 = ideal2 - (float)len2;
        v->pn_ks2_len = len2;
        v->pn_ks2_apc = (1.0f - frac2) / (1.0f + frac2);
        v->pn_ks2_aps = 0.0f; v->pn_ks2_widx = 0; v->pn_ks2_last = 0.0f;
        for (int i = 0; i < len2; i++) v->pn_ks2[i] = (i < len) ? v->ks_buf[i] : 0.0f;
        for (int i = 0; i < 4; i++) v->pn_ks2_disp[i] = 0.0f;
    }
    v->eng_subph = v->eng_env = 0.0f;
    v->eng_click = 700;
    v->pn_on = true;
}

// per-sample — navkit processStifKarpOscillator verbatim: near-lossless KS loop (avg → brightness
// blend → effectiveDamping → dispersion → allpass tuning), one-period delay, optional 2nd string,
// tone-filter, soundboard crossfade, sympathetic tap. Decay is the amp envelope, not the loop.
static inline float sound_piano_sample(Voice *v, float pitch_mul) {
    int len = v->ks_len;
    if (len < 2 || !v->pn_on) return 0.0f;       // engine id without a note-on init → silent
    float f = v->freq * pitch_mul;
    if (f < 20.0f) f = 20.0f;
    float ratio = (v->pn_initf > 20.0f) ? (f / v->pn_initf) : 1.0f;   // pitch tracking (arp/glide/vibrato)
    if (ratio < 0.25f) ratio = 0.25f; if (ratio > 4.0f) ratio = 4.0f;
    float effLen = (float)len / ratio;
    if (effLen < 2.0f) effLen = 2.0f; if (effLen > (float)len) effLen = (float)len;
    v->pn_damps += (v->pn_dampg - v->pn_damps) * 0.0005f;            // damper (pedal) slews in
    float effDamp = v->pn_ksd * (0.992f + 0.008f * v->pn_damps);     // ≈ ksDamping (near-lossless)
    v->pn_ksb_cur += (v->pn_ksb - v->pn_ksb_cur) * 0.00008f;         // brightness bloom: bright strike → mellow (~0.3s)
    float ksb = v->pn_ksb_cur;
    // ---- string 1 ----
    float rpos = (float)v->ks_widx - effLen;
    while (rpos < 0.0f) rpos += (float)len;
    int i0 = (int)rpos; if (i0 >= len) i0 -= len;
    int i1 = (i0 + 1 < len) ? i0 + 1 : 0;
    float cur = v->ks_buf[i0] + (rpos - floorf(rpos)) * (v->ks_buf[i1] - v->ks_buf[i0]);
    float nxt = v->ks_buf[(v->ks_widx + 1) % len];
    float blended = (cur + nxt) * 0.5f;
    blended += (cur - blended) * ksb;        // brightness keeps the highs (the presence lever)
    float filt = blended * effDamp;
    v->ks_last = filt;
    for (int i = 0; i < v->pn_disp_n; i++) {        // dispersion (inharmonicity)
        float c = v->pn_disp_c[i];
        float ap = c * filt + v->pn_disp_s[i];
        v->pn_disp_s[i] = filt - c * ap;
        filt = ap;
    }
    float apo = v->pn_apc * filt + v->pn_aps;        // fractional-delay allpass tuning
    v->pn_aps = filt - v->pn_apc * apo;
    v->ks_buf[v->ks_widx] = apo;
    v->ks_widx = (v->ks_widx + 1) % len;
    float out = cur;
    // ---- string 2 (detuned unison) ----
    if (v->pn_detune > 1.00001f) {
        int len2 = v->pn_ks2_len;
        float effLen2 = (float)len2 / ratio;
        if (effLen2 < 2.0f) effLen2 = 2.0f; if (effLen2 > (float)len2) effLen2 = (float)len2;
        float rp2 = (float)v->pn_ks2_widx - effLen2;
        while (rp2 < 0.0f) rp2 += (float)len2;
        int j0 = (int)rp2; if (j0 >= len2) j0 -= len2;
        int j1 = (j0 + 1 < len2) ? j0 + 1 : 0;
        float c2 = v->pn_ks2[j0] + (rp2 - floorf(rp2)) * (v->pn_ks2[j1] - v->pn_ks2[j0]);
        float n2 = v->pn_ks2[(v->pn_ks2_widx + 1) % len2];
        float bl2 = (c2 + n2) * 0.5f;
        bl2 += (c2 - bl2) * ksb;
        float ft2 = bl2 * effDamp;
        v->pn_ks2_last = ft2;
        for (int i = 0; i < v->pn_disp_n; i++) {
            float c = v->pn_disp_c[i];
            float ap = c * ft2 + v->pn_ks2_disp[i];
            v->pn_ks2_disp[i] = ft2 - c * ap;
            ft2 = ap;
        }
        float ap2 = v->pn_ks2_apc * ft2 + v->pn_ks2_aps;
        v->pn_ks2_aps = ft2 - v->pn_ks2_apc * ap2;
        v->pn_ks2[v->pn_ks2_widx] = ap2;
        v->pn_ks2_widx = (v->pn_ks2_widx + 1) % len2;
        out = (cur + c2) * 0.7f;
    }
    // output tone-filter (per voicing): dark clavichord → bright harpsichord
    v->pn_loop_lp += v->pn_loopco * (out - v->pn_loop_lp);
    out = v->pn_loop_lp;
    // soundboard crossfade (navkit step 9)
    float body = 0.0f;
    for (int i = 0; i < 4; i++) body += sound_biquad_run(&v->pn_body[i], out);
    body *= 0.35f;
    out = out * (1.0f - v->pn_bodymix * 0.6f) + body * v->pn_bodymix;
    // sympathetic resonance: feed a touch back at the 3rd partial
    if (v->pn_symp > 0.001f) {
        int tap = len / 3;
        if (tap > 0) v->ks_buf[(v->ks_widx + tap) % len] += out * v->pn_symp * 0.015f;
    }
    float dc = out - v->pn_dc_prev + 0.995f * v->pn_dc_state;        // gentle DC safety
    v->pn_dc_prev  = out;
    v->pn_dc_state = dc;
    // optional fundamental weight + attack (instrument_mode; default 0 = off, so this A/Bs against navkit)
    float aenv = fabsf(out);
    v->eng_env = aenv > v->eng_env ? aenv : v->eng_env * 0.9997f;
    v->eng_subph += f / (float)SOUND_SAMPLE_RATE;
    if (v->eng_subph >= 1.0f) v->eng_subph -= 1.0f;
    float tri = 4.0f * fabsf(v->eng_subph - 0.5f) - 1.0f;
    float res = dc + v->eng_p[0] * 0.8f * v->eng_env * tri;
    if (v->eng_click > 0) {
        v->noise_state = (v->noise_state * 1103515245 + 12345) & 0x7fffffff;
        float n  = ((v->noise_state >> 16) & 0xff) / 127.5f - 1.0f;
        float ce = (float)v->eng_click / 700.0f;
        res += v->eng_p[1] * 1.3f * n * ce * ce;
        v->eng_click--;
    }
    return res;
}

static inline float sound_engine_sample(Voice *v, float pitch_mul) {
    if (v->wave == INSTR_MALLET) return sound_mallet_sample(v, pitch_mul);
    if (v->wave == INSTR_FM)     return sound_fm_sample(v, pitch_mul);
    if (v->wave == INSTR_ORGAN)  return sound_organ_sample(v, pitch_mul);
    if (v->wave == INSTR_EPIANO) return sound_epiano_sample(v, pitch_mul);
    if (v->wave == INSTR_PD)     return sound_pd_sample(v, pitch_mul);
    if (v->wave == INSTR_MEMBRANE) return sound_membrane_sample(v, pitch_mul);
    if (v->wave == INSTR_REED)   return sound_reed_sample(v, pitch_mul);
    if (v->wave == INSTR_PIPE)   return sound_pipe_sample(v, pitch_mul);
    if (v->wave == INSTR_VOICE)  return sound_voice_sample(v, pitch_mul);
    if (v->wave == INSTR_GUITAR) return sound_guitar_sample(v, pitch_mul);
    if (v->wave == INSTR_PIANO)  return sound_piano_sample(v, pitch_mul);
    if (v->wave == INSTR_BOWED)  return sound_bowed_sample(v, pitch_mul);
    if (v->wave == INSTR_BRASS)  return sound_brass_sample(v, pitch_mul);
    int alloc = v->ks_len;
    if (alloc < 4) return 0.0f;   // engine id without a note-on init (e.g. an sfx step) — stay silent
    float f = v->freq * pitch_mul;
    if (f < 20.0f) f = 20.0f;
    float len_f = (float)SOUND_SAMPLE_RATE / f;          // the tap distance IS the pitch
    if (len_f > (float)alloc - 2.0f) len_f = (float)alloc - 2.0f;
    if (len_f < 2.0f)                len_f = 2.0f;
    // harmonics = ring time, mapped PERCEPTUALLY: the knob sets a target decay time
    // (T60 ≈ 0.04s thunk → ~2min drone, exponential so every knob position is an audible
    // step) and the feedback coefficient is derived from it per note frequency. A raw
    // linear fb range sounds dead — at 0.985 a 220Hz string still rings a full second, so
    // the bottom three-quarters of the knob did nothing. Frequency compensation keeps the
    // knob honest across the neck; at the very top the 0.5 average below becomes the real
    // ceiling (it still darkens highs faster — which is what sells "string").
    float t60 = 0.04f * expf(v->harm * 8.0f);            // 0.04 * 2980^harm seconds to -60dB
    float fb  = expf(-6.9078f / (t60 * f));              // fb^(freq*t60) = 0.001
    float rpos = (float)v->ks_widx - len_f;
    if (rpos < 0.0f) rpos += (float)alloc;
    int   i0 = (int)rpos;        if (i0 >= alloc) i0 = 0;
    int   i1 = i0 + 1;           if (i1 >= alloc) i1 = 0;
    float fr  = rpos - (float)i0;
    float out = v->ks_buf[i0] + (v->ks_buf[i1] - v->ks_buf[i0]) * fr;
    v->ks_buf[v->ks_widx] = (out + v->ks_last) * 0.5f * fb;   // damping average + feedback
    v->ks_last = out;
    if (++v->ks_widx >= alloc) v->ks_widx = 0;
    return out;
}

// ADSR amplitude during the gated (held) portion of a note. `s` = samples since note-on.
// Attack 0→1 over a, decay 1→sustain over d, then hold at sustain.
static inline float sound_adsr_gated(int s, int a, int d, float sustain) {
    if (a > 0 && s < a) return (float)s / (float)a;
    s -= a;
    if (d > 0 && s < d) return 1.0f - (1.0f - sustain) * ((float)s / (float)d);
    return sustain;
}

// A one-shot AD modulation envelope (filter/pitch env): linear attack 0→1 over `a`, then
// EXPONENTIAL decay 1→~0 over `d`, then flat 0. `s` = samples since note-on. Exp decay
// (vs the amp ADSR's linear) is what makes the pluck "pew" and the drum punch feel snappy.
static inline float sound_ad_env(int s, int a, int d) {
    if (a > 0 && s < a) return (float)s / (float)a;   // attack ramp
    s -= a;
    if (d <= 0 || s >= d) return 0.0f;                // no decay set, or finished → no contribution
    return expf(-4.0f * (float)s / (float)d);         // e^-4 ≈ 0.018 by the end — punchy, near-zero
}

// Chamberlin state-variable filter — one sample. Updates the voice's running state and
// returns the lowpass/highpass/bandpass/notch tap. `flt_q` is damping (1/Q); smaller = more
// resonant. (A TPT/Cytomic SVF was A/B'd here 2026-06-08 — objectively + audibly identical at
// the resonances we use, so it wasn't worth two codepaths; the wah realism gap was the sweep.)
static inline float sound_svf(Voice *v, float in, float cutoff_hz) {
    float f = 2.0f * sinf(3.14159265f * cutoff_hz / (float)SOUND_SAMPLE_RATE);
    if (f > 0.99f)   f = 0.99f;          // keep the simple SVF stable
    if (f < 0.0005f) f = 0.0005f;
    v->flt_low += f * v->flt_band;
    float high = in - v->flt_low - v->flt_q * v->flt_band;
    v->flt_band += f * high;
    // clamp state so a high-resonance sweep can't blow up
    if      (v->flt_low  >  4.0f) v->flt_low  =  4.0f; else if (v->flt_low  < -4.0f) v->flt_low  = -4.0f;
    if      (v->flt_band >  4.0f) v->flt_band =  4.0f; else if (v->flt_band < -4.0f) v->flt_band = -4.0f;
    switch (v->flt_mode) {
        case FILTER_LOW:   return v->flt_low;
        case FILTER_HIGH:  return high;
        case FILTER_BAND:  return v->flt_band;
        case FILTER_NOTCH: return high + v->flt_low;
    }
    return in;
}

// Drop any held-note ownership a voice carries (it's about to be reused or has finished),
// so the handle that owned it goes stale and its setters start no-op'ing.
static void sound_unclaim_held(int vi) {
    if (voices[vi].held && voices[vi].owner_slot >= 0 && voices[vi].owner_slot < SOUND_VOICES)
        held_voice[voices[vi].owner_slot] = -1;
    voices[vi].held = false;
    voices[vi].owner_slot = -1;
}

// declick accumulator (audio-thread-owned): a stolen voice's last output lands here and
// fades over ~3ms instead of cutting to zero in one sample. See the mix loop.
static float steal_tailL = 0.0f, steal_tailR = 0.0f;   // stereo: a stolen voice pays its last PANNED output per channel

static int sound_find_voice(void) {
    int vi = -1;
    // prefer a fully free voice
    for (int i = 0; i < SOUND_VOICES; i++) if (!voices[i].active) { vi = i; goto done; }
    // else steal the QUIETEST non-held voice (a faded ring-out tail you won't hear) rather than the
    // first one — so a still-ringing chord isn't hard-cut ("clamped") when polyphony runs out. This
    // matters now that the string engines (pluck/guitar/piano) sustain for seconds.
    float quietest = 1e30f;
    for (int i = 0; i < SOUND_VOICES; i++) if (!voices[i].held) {
        float a = fabsf(voices[i].last_outL) + fabsf(voices[i].last_outR);
        if (a < quietest) { quietest = a; vi = i; }
    }
    if (vi < 0) vi = 0;   // everything is a held note — fall back to voice 0
done:
    if (voices[vi].active) { steal_tailL += voices[vi].last_outL; steal_tailR += voices[vi].last_outR; }   // pay the cut into the declick tail
    sound_unclaim_held(vi);
    return vi;
}

// Resolve a live held voice from a handle's (slot, generation), or NULL if the handle is
// stale — the one safety check every note_* setter leans on.
static Voice *sound_held_voice(int slot, int gen) {
    if (slot < 0 || slot >= SOUND_VOICES) return NULL;
    int vi = held_voice[slot];
    if (vi < 0 || vi >= SOUND_VOICES) return NULL;
    Voice *v = &voices[vi];
    if (v->active && v->held && v->owner_slot == slot && v->owner_gen == gen) return v;
    return NULL;
}

// Flip a held voice into its release phase, fading from wherever the envelope is now.
static void sound_begin_release(Voice *v) {
    v->rel_start        = sound_adsr_gated(v->step_samples, v->a_samp, v->d_samp, v->sustain);
    v->step_len_samples = v->step_samples;   // step_samples >= step_len → release branch in the mix loop
    v->held             = false;             // no longer modulatable
}

// Shared note setup for both one-shot note() (kind 2) and held note_on() — copies the
// instrument's timbre/ADSR/LFO/filter into the voice. gate_samples = the gated length
// (SOUND_HELD_GATE for a sustained note_on; a finite count for a one-shot).
static void sound_setup_note(Voice *v, int midi, int slot, int vol, int gate_samples) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) slot = 0;
    Instrument *ins = &instr_bank[slot];
    v->active      = true;
    v->held        = false;
    v->owner_slot  = -1;
    v->sfx_idx     = -1;
    v->instr_slot  = slot;
    v->phase      = 0.0f;
    v->freq       = v->freq_target   = sound_midi_to_freq(midi);
    v->vol        = v->vol_target    = (vol < 0 ? 0 : vol > 7 ? 7 : vol) / 7.0f;
    v->wave       = ins->wave;
    v->duty       = v->duty_target   = ins->duty;
    v->a_samp     = ins->a_samp;
    v->d_samp     = ins->d_samp;
    v->r_samp     = ins->r_samp;
    v->sustain    = ins->sustain;
    for (int L = 0; L < SOUND_LFOS; L++) {
        v->lfo_dest[L]  = ins->lfo_dest[L];
        v->lfo_rate[L]  = ins->lfo_rate[L];
        v->lfo_depth[L] = ins->lfo_depth[L];
        v->lfo_phase[L] = 0.0f;
    }
    for (int e = 0; e < SOUND_ENVS; e++) {        // mod-envelopes (timer is step_samples → retriggers here)
        v->env_dest[e]   = ins->env_dest[e];
        v->env_a_samp[e] = ins->env_a_samp[e];
        v->env_d_samp[e] = ins->env_d_samp[e];
        v->env_amount[e] = ins->env_amount[e];
    }
    v->flw_dest   = ins->flw_dest;        // envelope follower (the running level starts at 0)
    v->flw_atk    = ins->flw_atk;
    v->flw_rel    = ins->flw_rel;
    v->flw_amount = ins->flw_amount;
    v->flw_amp    = 0.0f;
    v->flt_mode       = ins->flt_mode;
    v->flt_cutoff     = v->cutoff_target = ins->flt_cutoff;
    v->flt_q          = v->flt_q_target  = ins->flt_q;
    v->flt_low        = 0.0f;
    v->flt_band       = 0.0f;
    v->freq_slew        = 0.006f;   // ~snappy by default; note_glide() slows it for portamento
    // engine macros ride the same current/target slew pattern as cutoff/duty
    v->harm = v->harm_target = ins->harmonics;
    v->timb = v->timb_target = ins->timbre;
    v->mor  = v->mor_target  = ins->morph;
    if (v->wave == INSTR_VOICE) vox_apply_macros(v);   // seed VOWEL/SIZE/EFFORT from the macros
    v->drv  = v->drv_target  = ins->drive;
    v->drv_mode = ins->drive_mode;
    v->eng_p[0] = ins->eng_p[0];
    v->eng_p[1] = ins->eng_p[1];
    v->drv_dc_x1 = v->drv_dc_y1 = 0.0f;
    v->eko  = v->eko_target  = ins->echo;
    v->rvb  = v->rvb_target  = ins->reverb;
    v->rvb_bus = (ins->rvb_tank >= 1) ? tank_bus[ins->rvb_tank] : 0;   // resolve tank → its send-bus now (0 = master send)
    v->sc_key  = ins->sc_key;   // sidechain trigger routing (sidechain_key); snapshot at note-on like the sends
    v->sc_send = ins->sc_send;
    v->bus  = ins->fx_bus;
    v->pan  = v->pan_target  = ins->pan;
    v->last_outL = v->last_outR = 0.0f;
    v->ks_len = 0;
    v->md_on  = false;
    v->org_on = false;
    v->ep_on  = false;
    v->mb_on  = false;
    v->rd_on  = false;
    v->pp_on  = false;
    v->vox_on = false;
    v->gt_on  = false;
    v->pn_on  = false;
    v->bw_on  = false;
    v->br_on  = false;
    v->fm_mph = v->fm_fb = v->fm_tph = 0.0f;   // FM needs no excitation, just deterministic phases
    if      (v->wave == INSTR_PLUCK)  sound_pluck_start(v);    // excite the string
    else if (v->wave == INSTR_MALLET) sound_mallet_start(v);   // strike the bar
    else if (v->wave == INSTR_ORGAN)  sound_organ_start(v);    // arm the click + perc, clear the scanner
    else if (v->wave == INSTR_EPIANO) sound_epiano_start(v);   // build the 12 modal sines
    else if (v->wave == INSTR_MEMBRANE) sound_membrane_start(v); // strike the drumhead
    else if (v->wave == INSTR_REED)   sound_reed_start(v);     // size + seed the bore (self-oscillates)
    else if (v->wave == INSTR_PIPE)   sound_pipe_start(v);     // size + seed the flute bore + jet
    else if (v->wave == INSTR_VOICE)  sound_voice_start(v);    // seed a neutral vowel (self-oscillates)
    else if (v->wave == INSTR_GUITAR) sound_guitar_start(v);   // excite the string + voice the body
    else if (v->wave == INSTR_PIANO)  sound_piano_start(v);    // hammer-strike + arm dispersion + soundboard
    else if (v->wave == INSTR_BOWED)  sound_bowed_start(v);    // size + split the string at the bow, seed it
    else if (v->wave == INSTR_BRASS)  sound_brass_start(v);    // size + seed the bore, clear the lip biquad
    v->step_samples     = 0;
    v->step_len_samples = gate_samples;
    v->rel_start        = sound_adsr_gated(gate_samples, v->a_samp, v->d_samp, v->sustain);
}

// Configure a voice to play one SFX step. Does not touch active / sfx_idx / step.
static void sound_set_step(Voice *v, SfxStep step, int step_dur_units) {
    v->phase            = 0.0f;
    v->step_samples     = 0;
    v->step_len_samples = (step_dur_units * SOUND_SAMPLE_RATE) / 100;
    if (v->step_len_samples < 1) v->step_len_samples = 1;
    v->duty = 0.5f;           // SFX steps use the declick envelope, not ADSR; plain square
    for (int L = 0; L < SOUND_LFOS; L++) v->lfo_depth[L] = 0.0f;   // and no LFOs
    v->flt_mode = FILTER_OFF; // and no filter
    if (step.pitch == 0 || step.vol == 0) {
        v->vol  = 0.0f;       // silent step — still advances time
    } else {
        v->freq = sound_midi_to_freq(step.pitch);
        v->vol  = step.vol / 7.0f;
        v->wave = step.instr;
    }
}

static int fx_bus_overflow = 0;   // count of per-instrument FX requests dropped (pool exhausted)
// Return the private insert bus for a slot, allocating one on first use. -1 = pool exhausted
// (caller bails → that slot stays dry; never falls back to bus 0, which would effect everything).
static int fx_bus_for(int slot) {
    if (instr_bank[slot].fx_bus != 0) return instr_bank[slot].fx_bus;
    if (fx_next_bus >= SOUND_FX_BUSES) { fx_bus_overflow++; return -1; }
    instr_bank[slot].fx_bus = fx_next_bus++;
    return instr_bank[slot].fx_bus;
}

// Fire a request now (called on the audio thread).
static void sound_fire_req(SoundReq r) {
    if (r.kind == SR_SFX) {
        int n = r.a;
        if (n == -1) {
            for (int i = 0; i < SOUND_VOICES; i++)
                if (!voices[i].held && voices[i].active) {
                    steal_tailL += voices[i].last_outL; steal_tailR += voices[i].last_outR;   // declick the hard stop
                    voices[i].active = false;                   // held notes survive sfx(-1)
                }
        } else if (n >= 0 && n < SOUND_SFX_SLOTS) {
            int vi = sound_find_voice();
            Voice *v = &voices[vi];
            v->active     = true;
            v->sfx_idx    = n;
            v->step       = 0;
            sound_set_step(v, sfx_bank[n].steps[0], sfx_bank[n].step_dur);
        }
    } else if (r.kind == SR_NOTE) {
        int gate = r.dur_samples > 0 ? r.dur_samples : SOUND_SAMPLE_RATE / 4;
        uint32_t cmask = (r.b >= 0 && r.b < SOUND_INSTR_SLOTS) ? instr_bank[r.b].choke_mask : 0;
        if (cmask) {
            for (int i = 0; i < SOUND_VOICES; i++) {
                if (voices[i].active && ((cmask >> voices[i].instr_slot) & 1)) {
                    if (voices[i].held && voices[i].owner_slot >= 0)
                        held_voice[voices[i].owner_slot] = -1;
                    steal_tailL += voices[i].last_outL; steal_tailR += voices[i].last_outR;
                    voices[i].active = false;
                }
            }
        }
        sound_setup_note(&voices[sound_find_voice()], r.a, r.b, r.c, gate);
    } else if (r.kind == SR_NOTE_ON) {    // held / sustained — e0 = handle slot, e1 = generation
        int slot = r.e0, gen = r.e1;
        if (slot >= 0 && slot < SOUND_VOICES) {
            uint32_t cmask = (r.b >= 0 && r.b < SOUND_INSTR_SLOTS) ? instr_bank[r.b].choke_mask : 0;
            if (cmask) {
                for (int i = 0; i < SOUND_VOICES; i++) {
                    if (voices[i].active && ((cmask >> voices[i].instr_slot) & 1)) {
                        if (voices[i].held && voices[i].owner_slot >= 0)
                            held_voice[voices[i].owner_slot] = -1;
                        steal_tailL += voices[i].last_outL; steal_tailR += voices[i].last_outR;
                        voices[i].active = false;
                    }
                }
            }
            if (held_voice[slot] >= 0) sound_begin_release(&voices[held_voice[slot]]);  // slot reused → fade the old one
            int vi = sound_find_voice();
            sound_setup_note(&voices[vi], r.a, r.b, r.c, SOUND_HELD_GATE);
            voices[vi].held = true;
            voices[vi].owner_slot = slot;
            voices[vi].owner_gen  = gen;
            held_voice[slot] = vi;
        }
    } else if (r.kind == SR_NOTE_OFF) {
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) { sound_begin_release(v); held_voice[r.e0] = -1; }
    } else if (r.kind == SR_NOTE_PITCH) {   // a = midi * 256 (fixed-point float)
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) v->freq_target = sound_midi_to_freq_f(r.a / 256.0f);
    } else if (r.kind == SR_NOTE_VOL) {
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) v->vol_target = (r.a < 0 ? 0 : r.a > 7 ? 7 : r.a) / 7.0f;
    } else if (r.kind == SR_NOTE_CUTOFF) {
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) v->cutoff_target = (float)r.a;
    } else if (r.kind == SR_VOICE_PARAM) {   // EXPERIMENTAL INSTR_VOICE raw-param poke (voxlab)
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v && r.a >= 0 && r.a < VOX_NPARAM) {
            float x = r.b / 1000.0f; if (x < 0.0f) x = 0.0f; else if (x > 1.0f) x = 1.0f;
            v->vox_p[r.a] = x;
        }
    } else if (r.kind == SR_VOICE_CONS) {     // EXPERIMENTAL INSTR_VOICE consonant onset (voxlab)
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) { v->vox_cons = (r.a >= 0 && r.a < VC_COUNT) ? r.a : -1; v->vox_cons_t = 0.0f; }
    } else if (r.kind == SR_VOICE_CODA) {     // EXPERIMENTAL INSTR_VOICE consonant coda (voxlab)
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) { v->vox_coda = (r.a >= 0 && r.a < VC_COUNT) ? r.a : -1; v->vox_coda_t = 0.0f; }
    } else if (r.kind == SR_VOICE_NASAL) {    // INSTR_VOICE nasal color (public voice_nasal)
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) { float x = r.a / 1000.0f; if (x < 0.0f) x = 0.0f; else if (x > 1.0f) x = 1.0f; v->vox_p[7] = x; }
    } else if (r.kind == SR_NOTE_DUTY) {
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) { float d = r.a / 1000.0f; v->duty_target = d < 0.01f ? 0.01f : d > 0.99f ? 0.99f : d; }
    } else if (r.kind == SR_NOTE_RES) {
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) { int res = r.a < 0 ? 0 : r.a > 15 ? 15 : r.a; v->flt_q_target = 2.0f - res * 0.13f; }
    } else if (r.kind == SR_NOTE_LFO) {   // a=which, b=dest, c=rate*1000, e2=depth*1000 (phase kept → no click)
        Voice *v = sound_held_voice(r.e0, r.e1);
        int L = r.a;
        if (v && L >= 0 && L < SOUND_LFOS) {
            v->lfo_dest[L]  = r.b;
            v->lfo_rate[L]  = r.c  / 1000.0f;
            v->lfo_depth[L] = r.e2 / 1000.0f;
        }
    } else if (r.kind == SR_NOTE_FILTER) {
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) v->flt_mode = r.a;
    } else if (r.kind == SR_NOTE_GLIDE) {
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) {
            float k = r.a <= 0 ? 1.0f : 1000.0f / ((float)r.a * (float)SOUND_SAMPLE_RATE);
            v->freq_slew = k > 1.0f ? 1.0f : k < 0.00001f ? 0.00001f : k;
        }
    } else if (r.kind == SR_NOTE_OFF_ALL) {
        for (int i = 0; i < SOUND_VOICES; i++)
            if (voices[i].active && voices[i].held) { sound_begin_release(&voices[i]); }
        for (int i = 0; i < SOUND_VOICES; i++) held_voice[i] = -1;
    } else if (r.kind == SR_INSTR) {
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        Instrument *ins = &instr_bank[slot];
        ins->wave    = r.b;
        ins->sustain = (r.c < 0 ? 0 : r.c > 7 ? 7 : r.c) / 7.0f;
        ins->a_samp  = r.e0;
        ins->d_samp  = r.e1;
        ins->r_samp  = r.e2;
        // duty is left untouched — set independently via instrument_duty()
    } else if (r.kind == SR_INSTR_DUTY) {
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        float duty = r.b / 1000.0f;
        if (duty < 0.01f) duty = 0.01f;
        if (duty > 0.99f) duty = 0.99f;
        instr_bank[slot].duty = duty;
    } else if (r.kind == SR_ENG_TUNE) {
        int slot = r.a, idx = r.b;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS || idx < 0 || idx >= 2) return;
        instr_bank[slot].eng_p[idx] = r.c / 1000.0f;
    } else if (r.kind == SR_INSTR_LFO) {
        int slot = r.a;
        int L = r.e1;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        if (L < 0 || L >= SOUND_LFOS) return;
        instr_bank[slot].lfo_dest[L]  = r.b;
        instr_bank[slot].lfo_rate[L]  = r.c  / 1000.0f;
        instr_bank[slot].lfo_depth[L] = r.e0 / 1000.0f;
    } else if (r.kind == SR_INSTR_FILTER) {
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        int res = r.e0 < 0 ? 0 : r.e0 > 15 ? 15 : r.e0;
        instr_bank[slot].flt_mode   = r.b;
        instr_bank[slot].flt_cutoff = (float)r.c;
        instr_bank[slot].flt_q      = 2.0f - res * 0.13f;   // res 0 → damped, 15 → resonant peak
    } else if (r.kind == SR_INSTR_ENV) {   // a=slot, b=which, c=dest, e0=attack, e1=decay, e2=amount*1000
        int slot = r.a, e = r.b;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        if (e < 0 || e >= SOUND_ENVS) return;
        instr_bank[slot].env_dest[e]   = r.c;
        instr_bank[slot].env_a_samp[e] = r.e0;
        instr_bank[slot].env_d_samp[e] = r.e1;
        instr_bank[slot].env_amount[e] = r.e2 / 1000.0f;
    } else if (r.kind == SR_NOTE_ENV) {    // a=(which<<4)|dest, b=attack, c=decay, e2=amount*1000
        Voice *v = sound_held_voice(r.e0, r.e1);
        int e = (r.a >> 4) & 0xf, dest = r.a & 0xf;
        if (v && e >= 0 && e < SOUND_ENVS) {
            v->env_dest[e]   = dest;
            v->env_a_samp[e] = r.b;
            v->env_d_samp[e] = r.c;
            v->env_amount[e] = r.e2 / 1000.0f;
        }
    } else if (r.kind == SR_WAVE_SET) {    // a=which, b=start index, c/e0/e1/e2 = values*32767
        if (r.a >= 0 && r.a < SOUND_USER_WAVES && r.b >= 0 && r.b + 3 < SOUND_WAVE_LEN) {
            user_wave[r.a][r.b]     = r.c  / 32767.0f;
            user_wave[r.a][r.b + 1] = r.e0 / 32767.0f;
            user_wave[r.a][r.b + 2] = r.e1 / 32767.0f;
            user_wave[r.a][r.b + 3] = r.e2 / 32767.0f;
        }
    } else if (r.kind == SR_INSTR_MACRO) {  // a=slot, b=which 0..2, c=val*1000
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        float x = r.c / 1000.0f;
        if (x < 0.0f) x = 0.0f; if (x > 1.0f) x = 1.0f;
        if      (r.b == 0) instr_bank[slot].harmonics = x;
        else if (r.b == 1) instr_bank[slot].timbre    = x;
        else if (r.b == 2) instr_bank[slot].morph     = x;
    } else if (r.kind == SR_NOTE_MACRO) {   // a=which 0..2, b=val*1000 (live, slewed)
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) {
            float x = r.b / 1000.0f;
            if (x < 0.0f) x = 0.0f; if (x > 1.0f) x = 1.0f;
            if      (r.a == 0) v->harm_target = x;
            else if (r.a == 1) v->timb_target = x;
            else if (r.a == 2) v->mor_target  = x;
            if (v->wave == INSTR_VOICE) vox_apply_macros(v);   // live VOWEL/SIZE/EFFORT
        }
    } else if (r.kind == SR_INSTR_CHOKE) {  // a=slot_a, b=slot_b
        if (r.a >= 0 && r.a < SOUND_INSTR_SLOTS && r.b >= 0 && r.b < SOUND_INSTR_SLOTS)
            instr_bank[r.a].choke_mask |= (1u << r.b);
    } else if (r.kind == SR_INSTR_DRIVE) {  // a=slot, b=val*1000
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        float x = r.b / 1000.0f;
        if (x < 0.0f) x = 0.0f; if (x > 1.0f) x = 1.0f;
        instr_bank[slot].drive = x;
    } else if (r.kind == SR_NOTE_DRIVE) {   // a=val*1000 (live, slewed)
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) {
            float x = r.a / 1000.0f;
            if (x < 0.0f) x = 0.0f; if (x > 1.0f) x = 1.0f;
            v->drv_target = x;
        }
    } else if (r.kind == SR_INSTR_DRIVE_MODE) {  // a=slot, b=mode (DRIVE_*)
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        int m = r.b;
        if (m < 0 || m > 3) m = 0;
        instr_bank[slot].drive_mode = m;
    } else if (r.kind == SR_NOTE_DRIVE_MODE) {   // a=mode (DRIVE_*), e0/e1=handle (live, snaps)
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) {
            int m = r.a;
            if (m < 0 || m > 3) m = 0;
            v->drv_mode = m;
        }
    } else if (r.kind == SR_ECHO) {         // a=time_ms, b=feedback*1000, c=tone*1000
        echo_used = true;
        float ms = (float)r.a;
        if (ms < 1.0f)    ms = 1.0f;
        if (ms > 2000.0f) ms = 2000.0f;
        echo_time_target = ms * (float)SOUND_SAMPLE_RATE / 1000.0f;
        if (echo_time_target > (float)(SOUND_ECHO_MAX - 4)) echo_time_target = (float)(SOUND_ECHO_MAX - 4);
        float fb = r.b / 1000.0f;
        if (fb < 0.0f) fb = 0.0f; if (fb > 1.1f) fb = 1.1f;   // >1.0 = self-osc zone (tanh-bounded)
        echo_fb = fb;
        echo_tone_coef = sound_echo_coef(r.c / 1000.0f);
    } else if (r.kind == SR_INSTR_ECHO) {   // a=slot, b=send*1000
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        float x = r.b / 1000.0f;
        if (x < 0.0f) x = 0.0f; if (x > 1.0f) x = 1.0f;
        instr_bank[slot].echo = x;
        echo_used = true;
    } else if (r.kind == SR_INSTR_TUNE) {   // a=slot, b=semitones*1000 (signed)
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        float semis = r.b / 1000.0f;
        if (semis < -24.0f) semis = -24.0f; if (semis > 24.0f) semis = 24.0f;
        instr_bank[slot].tune_mul = powf(2.0f, semis / 12.0f);
    } else if (r.kind == SR_INSTR_FOLLOW) {   // a=slot b=dest c=atk_ms e0=rel_ms e2=amount*1000
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        instr_bank[slot].flw_dest   = r.b;
        instr_bank[slot].flw_atk    = sound_follow_coef(r.c);
        instr_bank[slot].flw_rel    = sound_follow_coef(r.e0);
        instr_bank[slot].flw_amount = r.e2 / 1000.0f;
    } else if (r.kind == SR_NOTE_FOLLOW) {    // a=dest b=atk_ms c=rel_ms e0/e1=handle e2=amount*1000
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) {
            v->flw_dest   = r.a;
            v->flw_atk    = sound_follow_coef(r.b);
            v->flw_rel    = sound_follow_coef(r.c);
            v->flw_amount = r.e2 / 1000.0f;
        }
    } else if (r.kind == SR_NOTE_ECHO) {    // a=val*1000 (live, slewed)
        echo_used = true;
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) {
            float x = r.a / 1000.0f;
            if (x < 0.0f) x = 0.0f; if (x > 1.0f) x = 1.0f;
            v->eko_target = x;
        }
    } else if (r.kind == SR_REVERB) {       // a=size*1000, b=damping*1000 — configure tank 0 (the master send)
        reverb_used = true;
        float size = r.a / 1000.0f;
        if (size < 0.0f) size = 0.0f; if (size > 1.0f) size = 1.0f;
        rvb_tank[0].fb = REVERB_FEEDBACK_MIN + size * REVERB_FEEDBACK_RANGE;   // 0.7 .. 0.95
        float damp = r.b / 1000.0f;
        if (damp < 0.0f) damp = 0.0f; if (damp > 1.0f) damp = 1.0f;
        rvb_tank[0].damp = damp;
    } else if (r.kind == SR_INSTR_REVERB) { // a=slot, b=send*1000
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        float x = r.b / 1000.0f;
        if (x < 0.0f) x = 0.0f; if (x > 1.0f) x = 1.0f;
        instr_bank[slot].reverb = x;
        reverb_used = true;
    } else if (r.kind == SR_NOTE_REVERB) {  // a=val*1000 (live, slewed)
        reverb_used = true;
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) {
            float x = r.a / 1000.0f;
            if (x < 0.0f) x = 0.0f; if (x > 1.0f) x = 1.0f;
            v->rvb_target = x;
        }
    } else if (r.kind == SR_REVERB_BUS) {   // a=tank(1..N-1), b=size*1000, c=damp*1000 — make tank N a reverb send-bus
        int tank = r.a;
        if (tank < 1 || tank >= SOUND_REVERB_TANKS) return;   // tank 0 is reverb()'s master send — not a bus
        if (tank_bus[tank] == 0) {            // first call for this tank: claim a dedicated aux bus
            if (fx_next_bus >= SOUND_FX_BUSES) {   // aux-bus pool exhausted — surface it (no silent caps, §6)
                rvb_bus_overflow++;
                atomic_fetch_add_explicit(&sound_dropped, 1, memory_order_relaxed);
                return;
            }
            int bus = fx_next_bus++;
            tank_bus[tank] = bus;
            bus_tank[bus]  = (int8_t)tank;
            insert_order[bus][0] = FX_REVERB;  // the bus's chain starts with the reverb; fx_order() can add pedals after it
            insert_order_n[bus]  = 1;
        }
        float size = r.b / 1000.0f; if (size < 0.0f) size = 0.0f; if (size > 1.0f) size = 1.0f;
        float damp = r.c / 1000.0f; if (damp < 0.0f) damp = 0.0f; if (damp > 1.0f) damp = 1.0f;
        rvb_tank[tank].fb   = REVERB_FEEDBACK_MIN + size * REVERB_FEEDBACK_RANGE;
        rvb_tank[tank].damp = damp;
        rvb_tank[tank].mix  = 1.0f;   // a dedicated send-bus is full wet (wet-replace) — keeps reverbspace byte-identical
        rvb_tank[tank].used = true;
    } else if (r.kind == SR_INSTR_REVERB_BUS) { // a=slot, b=tank, c=mix*1000 — route a slot's send into tank N's bus
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        int tank = r.b;
        if (tank < 1 || tank >= SOUND_REVERB_TANKS) return;
        float mix = r.c / 1000.0f; if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f;
        instr_bank[slot].reverb   = mix;
        instr_bank[slot].rvb_tank = tank;
    } else if (r.kind == SR_REVERB_BUS_FX) {  // a=tank, b=fx, c/e0/e1=params*1000 — an insert AFTER the reverb on tank N's bus
        int tank = r.a;
        if (tank < 1 || tank >= SOUND_REVERB_TANKS) return;
        int bus = tank_bus[tank];
        if (bus == 0) return;                 // tank isn't a reverb-bus yet — call reverb_bus() first
        int fx = r.b;
        float a = r.c / 1000.0f, b = r.e0 / 1000.0f, c = r.e1 / 1000.0f;
        switch (fx) {                         // configure the insert on this bus (params per effect)
            case FX_CRUSH:  fx_set_crush(bus, a, b, c); break;   // bits, rate, mix (mix 0 = off)
            case FX_EQ:     fx_set_eq(bus, a, b, c);    break;   // low, mid, high dB
            case FX_TAPE:   fx_set_tape(bus, a, b, c);  break;   // wow, flutter, saturation
            case FX_CHORUS: fx_set_chorus(bus, a, b, c); break;  // rate, depth, mix (mix 0 = off)
            default: return;                  // only these 4 make sense (+fit 3 params) on a reverb tail
        }
        // ensure fx runs AFTER the reverb: append to the bus chain if not already present
        bool present = false;
        for (int s = 0; s < insert_order_n[bus]; s++) if (insert_order[bus][s] == fx) present = true;
        if (!present && insert_order_n[bus] < N_INSERTS) insert_order[bus][insert_order_n[bus]++] = fx;
    } else if (r.kind == SR_REVERB_INSERT) {  // a=size*1000, b=damp*1000, c=mix*1000 — master mix-reverb INSERT (bus 0, tank 1)
        float size = r.a / 1000.0f; if (size < 0.0f) size = 0.0f; if (size > 1.0f) size = 1.0f;
        float damp = r.b / 1000.0f; if (damp < 0.0f) damp = 0.0f; if (damp > 1.0f) damp = 1.0f;
        float mix  = r.c / 1000.0f; if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f;
        bus_tank[0] = 1;              // master bus runs FX_REVERB through tank 1 — the in-line insert tank
        rvb_tank[1].fb   = REVERB_FEEDBACK_MIN + size * REVERB_FEEDBACK_RANGE;
        rvb_tank[1].damp = damp;
        rvb_tank[1].mix  = mix;       // <1 = dry+wet blend in the chain (an honest reverb pedal)
        rvb_tank[1].used = true;
        // the cart places FX_REVERB in its fx_order(0, …) list to set the reverb's position in the master chain
    } else if (r.kind == SR_ECHO_INSERT) {  // a=time_ms, b=fb*1000, c=tone*1000, e0=mix*1000 — master in-line delay INSERT
        float ms = (float)r.a; if (ms < 1.0f) ms = 1.0f;
        echo_ins_time_target = ms * (float)SOUND_SAMPLE_RATE / 1000.0f;
        if (echo_ins_time_target > (float)(SOUND_ECHO_MAX - 4)) echo_ins_time_target = (float)(SOUND_ECHO_MAX - 4);
        float fb = r.b / 1000.0f; if (fb < 0.0f) fb = 0.0f; if (fb > 1.1f) fb = 1.1f;
        echo_ins_fb = fb;
        echo_ins_tone_coef = sound_echo_coef(r.c / 1000.0f);
        float mix = r.e0 / 1000.0f; if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f;
        if (!echo_ins_used && mix > 0.0f) echo_ins_time = echo_ins_time_target;   // snap time when first turning ON — no startup pitch-bend
        echo_ins_mix  = mix;
        echo_ins_used = (mix > 0.0f);   // mix 0 = off (dormant → byte-identical), like the other inserts
        // the cart places FX_ECHO in its fx_order(0, …) list to set the delay's position in the master chain
    } else if (r.kind == SR_GRAINS) {       // master granular delay (bus 0): a=grain_ms, b=density*100, c=position*1000, e0=scatter*1000, e1=feedback*1000, e2=mix*1000
        fx_set_grains(0, (float)r.a, r.b / 100.0f, r.c / 1000.0f, r.e0 / 1000.0f, r.e1 / 1000.0f, r.e2 / 1000.0f);
        bool present = false;   // auto-place FX_GRAINS in bus 0's chain (not in the default ladder, like FX_ECHO)
        for (int s = 0; s < insert_order_n[0]; s++) if (insert_order[0][s] == FX_GRAINS) present = true;
        if (!present && insert_order_n[0] < N_INSERTS) insert_order[0][insert_order_n[0]++] = FX_GRAINS;
    } else if (r.kind == SR_INSTR_GRAINS) { // per-instrument: a=slot, b=grain_ms, c=density*100, e0=position*1000, e1=scatter*1000, e2=PACK(feedback,mix)
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) {
            float feedback = (r.e2 / 1001) / 100.0f;   // unpack: e2 = feedback*100 *1001 + mix*1000
            float mix      = (r.e2 % 1001) / 1000.0f;
            fx_set_grains(b, (float)r.b, r.c / 100.0f, r.e0 / 1000.0f, r.e1 / 1000.0f, feedback, mix);
            bool present = false;
            for (int s = 0; s < insert_order_n[b]; s++) if (insert_order[b][s] == FX_GRAINS) present = true;
            if (!present && insert_order_n[b] < N_INSERTS) insert_order[b][insert_order_n[b]++] = FX_GRAINS;
        }
    } else if (r.kind == SR_GRAINS_FREEZE) {        // a=on
        fx_set_grains_freeze(0, r.a != 0);
    } else if (r.kind == SR_INSTR_GRAINS_FREEZE) {  // a=slot, b=on
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_grains_freeze(b, r.b != 0);
    } else if (r.kind == SR_CHORUS) {       // master chorus (bus 0): a=rate*1000, b=depth*1000, c=mix*1000
        fx_set_chorus(0, r.a / 1000.0f, r.b / 1000.0f, r.c / 1000.0f);
    } else if (r.kind == SR_INSTR_CHORUS) { // per-instrument: a=slot, b=rate*1000, c=depth*1000, e0=mix*1000
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_chorus(b, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f);
    } else if (r.kind == SR_FLANGER) {      // master flanger (bus 0): a=rate, b=depth, c=fb(signed), e0=mix (×1000)
        fx_set_flanger(0, r.a / 1000.0f, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f);
    } else if (r.kind == SR_INSTR_FLANGER) { // per-instrument: a=slot, b=rate, c=depth, e0=fb(signed), e1=mix (×1000)
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_flanger(b, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f, r.e1 / 1000.0f);
    } else if (r.kind == SR_TAPE) {         // master tape (bus 0): a=wow, b=flutter, c=sat (×1000)
        fx_set_tape(0, r.a / 1000.0f, r.b / 1000.0f, r.c / 1000.0f);
    } else if (r.kind == SR_INSTR_TAPE) {   // per-instrument: a=slot, b=wow, c=flutter, e0=sat (×1000)
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_tape(b, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f);
    } else if (r.kind == SR_WAH) {          // master auto-wah (bus 0): a=sens, b=res, c=mix (×1000)
        fx_set_wah(0, r.a / 1000.0f, r.b / 1000.0f, r.c / 1000.0f);
    } else if (r.kind == SR_INSTR_WAH) {    // per-instrument: a=slot, b=sens, c=res, e0=mix (×1000)
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_wah(b, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f);
    } else if (r.kind == SR_WAH_LFO) {      // master LFO-wah (bus 0): a=rate, b=res, c=mix (×1000)
        fx_set_wah_lfo(0, r.a / 1000.0f, r.b / 1000.0f, r.c / 1000.0f);
    } else if (r.kind == SR_INSTR_WAH_LFO) {// per-instrument: a=slot, b=rate, c=res, e0=mix (×1000)
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_wah_lfo(b, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f);
    } else if (r.kind == SR_TREMOLO) {      // master tremolo (bus 0): a=rate*1000, b=depth*1000, c=shape
        fx_set_tremolo(0, r.a / 1000.0f, r.b / 1000.0f, r.c);
    } else if (r.kind == SR_INSTR_TREMOLO) {// per-instrument: a=slot, b=rate*1000, c=depth*1000, e0=shape
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_tremolo(b, r.b / 1000.0f, r.c / 1000.0f, r.e0);
    } else if (r.kind == SR_AUTOPAN) {      // master auto-pan (bus 0): a=rate*1000, b=depth*1000, c=shape
        fx_set_autopan(0, r.a / 1000.0f, r.b / 1000.0f, r.c);
    } else if (r.kind == SR_INSTR_AUTOPAN) {// per-instrument: a=slot, b=rate*1000, c=depth*1000, e0=shape
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_autopan(b, r.b / 1000.0f, r.c / 1000.0f, r.e0);
    } else if (r.kind == SR_RINGMOD) {      // master ring mod (bus 0): a=freq_hz, b=mix*1000
        fx_set_ringmod(0, (float)r.a, r.b / 1000.0f);
    } else if (r.kind == SR_INSTR_RINGMOD) {// per-instrument: a=slot, b=freq_hz, c=mix*1000
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_ringmod(b, (float)r.b, r.c / 1000.0f);
    } else if (r.kind == SR_PHASER) {       // master phaser (bus 0): a=rate, b=depth, c=fb(signed), e0=mix (×1000), e1=stages
        fx_set_phaser(0, r.a / 1000.0f, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f, r.e1);
    } else if (r.kind == SR_INSTR_PHASER) { // per-instrument: a=slot, b=rate, c=depth, e0=fb(signed), e1=mix (×1000), e2=stages
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_phaser(b, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f, r.e1 / 1000.0f, r.e2);
    } else if (r.kind == SR_LESLIE) {       // master Leslie (bus 0): a=speed, b=drive, c=balance, e0=doppler, e1=mix (×1000 except speed)
        fx_set_leslie(0, r.a, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f, r.e1 / 1000.0f);
    } else if (r.kind == SR_INSTR_LESLIE) { // per-instrument: a=slot, b=speed, c=drive, e0=balance, e1=doppler, e2=mix
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_leslie(b, r.b, r.c / 1000.0f, r.e0 / 1000.0f, r.e1 / 1000.0f, r.e2 / 1000.0f);
    } else if (r.kind == SR_FX_ORDER) {     // set a bus's insert order: a=bus, b=packed lo (slots 0..7, 4 bits each), c=count, e0=packed hi (slots 8..)
        int bus = r.a;
        if (bus < 0 || bus >= SOUND_FX_BUSES) return;
        int n = r.c; if (n < 0) n = 0; if (n > N_INSERTS) n = N_INSERTS;
        for (int s = 0; s < n; s++) {
            int kind = (s < 8) ? ((r.b >> (4 * s)) & 15) : ((r.e0 >> (4 * (s - 8))) & 15);
            insert_order[bus][s] = (kind < N_INSERTS) ? kind : FX_TREM;
        }
        insert_order_n[bus] = n;
    } else if (r.kind == SR_BITCRUSH) {     // master bitcrush (bus 0): a=bits*100, b=rate*100, c=mix*1000
        fx_set_crush(0, r.a / 100.0f, r.b / 100.0f, r.c / 1000.0f);
    } else if (r.kind == SR_INSTR_BITCRUSH) { // per-instrument: a=slot, b=bits*100, c=rate*100, e0=mix*1000
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_crush(b, r.b / 100.0f, r.c / 100.0f, r.e0 / 1000.0f);
    } else if (r.kind == SR_EQ) {           // master EQ (bus 0): a=low_db*1000, b=mid_db*1000, c=high_db*1000
        fx_set_eq(0, r.a / 1000.0f, r.b / 1000.0f, r.c / 1000.0f);
    } else if (r.kind == SR_INSTR_EQ) {     // per-instrument: a=slot, b=low_db*1000, c=mid_db*1000, e0=high_db*1000
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_eq(b, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f);
    } else if (r.kind == SR_FORMANT) {      // master formant/vowel filter (bus 0): a=vowel, b=q, c=mix (×1000)
        fx_set_formant(0, r.a / 1000.0f, r.b / 1000.0f, r.c / 1000.0f);
    } else if (r.kind == SR_INSTR_FORMANT) { // per-instrument: a=slot, b=vowel, c=q, e0=mix (×1000)
        if (r.a < 0 || r.a >= SOUND_INSTR_SLOTS) return;
        int b = fx_bus_for(r.a);
        if (b >= 1) fx_set_formant(b, r.b / 1000.0f, r.c / 1000.0f, r.e0 / 1000.0f);
    } else if (r.kind == SR_SIDECHAIN) {    // a=victim_bus, b=key, c=amount*1000, e0=atk_ms, e1=rel_ms
        int vb = r.a; if (vb < 0 || vb >= SOUND_FX_BUSES) return;
        int key = r.b; if (key < 0) key = 0; if (key >= N_SC_KEYS) key = N_SC_KEYS - 1;
        float amount = r.c / 1000.0f; if (amount < 0.0f) amount = 0.0f; if (amount > 1.0f) amount = 1.0f;
        int atk = r.e0 < 1 ? 1 : r.e0, rel = r.e1 < 1 ? 1 : r.e1;
        sc[vb].key    = key;
        sc[vb].amount = amount;
        sc[vb].atk    = 1.0f - expf(-1.0f / (atk * 0.001f * (float)SOUND_SAMPLE_RATE));
        sc[vb].rel    = 1.0f - expf(-1.0f / (rel * 0.001f * (float)SOUND_SAMPLE_RATE));
        sc[vb].used   = (amount > 0.0005f);   // amount 0 → dormant (byte-identical)
    } else if (r.kind == SR_SIDECHAIN_KEY) { // a=slot, b=key, c=send*1000
        int slot = r.a; if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        int key = r.b; if (key < 0) key = 0; if (key >= N_SC_KEYS) key = N_SC_KEYS - 1;
        float send = r.c / 1000.0f; if (send < 0.0f) send = 0.0f; if (send > 1.0f) send = 1.0f;
        instr_bank[slot].sc_key  = key;
        instr_bank[slot].sc_send = send;
    } else if (r.kind == SR_GLUE) {          // a=victim_bus, b=amount*1000, c=atk_ms, e0=rel_ms
        int vb = r.a; if (vb < 0 || vb >= SOUND_FX_BUSES) return;
        float amount = r.b / 1000.0f; if (amount < 0.0f) amount = 0.0f; if (amount > 1.0f) amount = 1.0f;
        int atk = r.c < 1 ? 1 : r.c, rel = r.e0 < 1 ? 1 : r.e0;
        sc[vb].key    = -1;   // self-keyed: reads the bus's own level
        sc[vb].amount = amount;
        sc[vb].atk    = 1.0f - expf(-1.0f / (atk * 0.001f * (float)SOUND_SAMPLE_RATE));
        sc[vb].rel    = 1.0f - expf(-1.0f / (rel * 0.001f * (float)SOUND_SAMPLE_RATE));
        sc[vb].used   = (amount > 0.0005f);
    } else if (r.kind == SR_FILTER) {       // a=mode, b=cutoff_hz, c=res*1000 — master resonant filter (bus 0)
        fx_set_filter(0, r.a, (float)r.b, r.c / 1000.0f);
    } else if (r.kind == SR_INSTR_PAN) {    // a=slot, b=pan*1000 (signed)
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        float x = r.b / 1000.0f;
        if (x < -1.0f) x = -1.0f; if (x > 1.0f) x = 1.0f;
        instr_bank[slot].pan = x;
    } else if (r.kind == SR_NOTE_PAN) {     // a=pan*1000 (live, slewed)
        Voice *v = sound_held_voice(r.e0, r.e1);
        if (v) {
            float x = r.a / 1000.0f;
            if (x < -1.0f) x = -1.0f; if (x > 1.0f) x = 1.0f;
            v->pan_target = x;
        }
    } else if (r.kind == SR_PAN_LAW) {      // a=law (PAN_LINEAR/PAN_POWER)
        g_pan_law = (r.a == PAN_POWER) ? PAN_POWER : PAN_LINEAR;
    }
}

// ───────── audio callback (runs on audio thread) ─────────

static void sound_callback(void *buffer_data, unsigned int frames) {
    float *out = (float*)buffer_data;
#ifdef DE_AUDIO_WORKLET
    const float master_gain = atomic_load_explicit(&sound_master_gain, memory_order_relaxed);  // pause-mute (worklet)
#endif

    // 1) drain queued requests: fire immediate, hold delayed
    for (;;) {
        int tail = atomic_load_explicit(&req_tail, memory_order_relaxed);          // consumer owns tail
        if (tail == atomic_load_explicit(&req_head, memory_order_acquire)) break;  // acquire pairs with the producer's release → the entry writes are visible
        SoundReq r = req_queue[tail];
        atomic_store_explicit(&req_tail, (tail + 1) % SOUND_REQ_QUEUE, memory_order_release);  // free the slot for the producer
        if (r.delay_samples <= 0) {
            sound_fire_req(r);
        } else if (delayed_count < SOUND_DELAYED_MAX) {
            delayed[delayed_count++] = r;
        } else {
            atomic_fetch_add_explicit(&sound_dropped, 1, memory_order_relaxed);   // delayed pen full — a scheduled note was lost (tripwire)
        }
    }

    // 2) mix — delayed requests fire SAMPLE-ACCURATELY inside the loop, not at
    // block boundaries. With a 1024-sample buffer (~23ms) block-edge firing
    // quantized away exactly the micro-timing that makes grooves feel played:
    // strum staggers (8ms), swing pushes, laid-back snares (10-35ms). Walking
    // the pen per sample keeps every schedule_hit offset intact; the pen is
    // tiny (<64) so the cost is noise.
    for (unsigned int i = 0; i < frames; i++) {
        float busL[SOUND_FX_BUSES] = {0.0f}, busR[SOUND_FX_BUSES] = {0.0f};   // per-insert-bus stereo accumulators (bus 0 = master)
        float echo_in   = 0.0f; // this sample's summed sends into the echo bus   (MONO — centered in v1)
        float reverb_in = 0.0f; // this sample's summed sends into the reverb bus (MONO — centered in v1)
        for (int sk = 0; sk < N_SC_KEYS; sk++) sc_key_lvl[sk] = 0.0f;   // sidechain trigger sums, refilled below

        for (int di = 0; di < delayed_count; ) {
            if (--delayed[di].delay_samples < 0) {
                sound_fire_req(delayed[di]);
                delayed[di] = delayed[--delayed_count];   // swap-remove
            } else {
                di++;
            }
        }

        for (int vi = 0; vi < SOUND_VOICES; vi++) {
            Voice *v = &voices[vi];
            if (!v->active) continue;

            // per-param slew: note voices glide toward their targets so live writes
            // (note_pitch/vol/cutoff/duty) don't stairstep or zipper. One-shots keep
            // target == current, so this is a no-op for them; SFX set freq/vol
            // directly and are skipped.
            if (v->sfx_idx < 0) {
                v->freq       += (v->freq_target   - v->freq)       * v->freq_slew;  // glide rate (note_glide)
                v->vol        += (v->vol_target    - v->vol)        * 0.003f;   // anti-zipper on gating
                v->flt_cutoff += (v->cutoff_target - v->flt_cutoff) * 0.0015f;  // smooth filter sweep
                v->flt_q      += (v->flt_q_target  - v->flt_q)      * 0.0015f;  // smooth resonance sweep
                v->duty       += (v->duty_target   - v->duty)       * 0.003f;
                v->harm       += (v->harm_target   - v->harm)       * 0.002f;   // engine macros (note_harmonics/timbre/morph)
                v->timb       += (v->timb_target   - v->timb)       * 0.002f;
                v->mor        += (v->mor_target    - v->mor)        * 0.002f;
                v->drv        += (v->drv_target    - v->drv)        * 0.002f;   // drive (note_drive)
                v->eko        += (v->eko_target    - v->eko)        * 0.002f;   // echo send (note_echo)
                v->rvb        += (v->rvb_target    - v->rvb)        * 0.002f;   // reverb send (note_reverb)
                v->pan        += (v->pan_target    - v->pan)        * 0.002f;   // stereo pan (note_pan) — anti-zipper slew
            }

            // step advance? (SFX walk their step list; one-shots fall through to ADSR release)
            if (v->step_samples >= v->step_len_samples && v->sfx_idx >= 0) {
                Sfx *s = &sfx_bank[v->sfx_idx];
                v->step++;
                if (v->step >= s->length) {
                    if (s->loop) {
                        v->step = 0;
                    } else {
                        v->active = false;
                        continue;
                    }
                }
                sound_set_step(v, s->steps[v->step], s->step_dur);
            }

            // envelope: SFX use the per-step declick; one-shot notes use the instrument's ADSR
            float env;
            if (v->sfx_idx >= 0) {
                float t = (float)v->step_samples / (float)v->step_len_samples;
                if      (t < 0.05f) env = t / 0.05f;
                else if (t > 0.85f) env = (1.0f - t) / 0.15f;
                else                env = 1.0f;
                if (env < 0) env = 0;
            } else if (v->step_samples < v->step_len_samples) {
                env = sound_adsr_gated(v->step_samples, v->a_samp, v->d_samp, v->sustain);  // gated A/D/S
            } else {
                int rs = v->step_samples - v->step_len_samples;                              // release
                if (v->r_samp <= 0 || rs >= v->r_samp) { v->active = false; sound_unclaim_held(vi); continue; }
                env = v->rel_start * (1.0f - (float)rs / (float)v->r_samp);
            }

            // LFOs (one-shot notes only): up to 3 routable sines → pitch / duty / volume / cutoff
            float duty = v->duty, trem = 1.0f, pitch_mul = 1.0f, cutoff = v->flt_cutoff;
            float harm_mod = 0.0f, timb_mod = 0.0f, mor_mod = 0.0f;   // macro modulation (engine voices)
            float pan_mod = 0.0f;   // LFO_PAN offset (auto-pan), added to the slewed base pan below
            if (v->sfx_idx < 0) {
                for (int L = 0; L < SOUND_LFOS; L++) {
                    if (v->lfo_depth[L] <= 0.0f) continue;
                    float lfo = sinf(v->lfo_phase[L] * 6.2831853f);     // -1..1
                    v->lfo_phase[L] += v->lfo_rate[L] / (float)SOUND_SAMPLE_RATE;
                    if (v->lfo_phase[L] >= 1.0f) v->lfo_phase[L] -= 1.0f;
                    if      (v->lfo_dest[L] == LFO_PITCH)  pitch_mul *= powf(2.0f, (lfo * v->lfo_depth[L]) / 12.0f);
                    else if (v->lfo_dest[L] == LFO_DUTY)   duty += lfo * v->lfo_depth[L];
                    else if (v->lfo_dest[L] == LFO_VOLUME) trem *= 1.0f - 0.5f * v->lfo_depth[L] * (1.0f - lfo);
                    else if (v->lfo_dest[L] == LFO_CUTOFF) cutoff += lfo * v->lfo_depth[L];
                    else if (v->lfo_dest[L] == LFO_HARMONICS) harm_mod += lfo * v->lfo_depth[L];   // engine macros (INSTR_PLUCK+)
                    else if (v->lfo_dest[L] == LFO_TIMBRE)    timb_mod += lfo * v->lfo_depth[L];
                    else if (v->lfo_dest[L] == LFO_MORPH)     mor_mod  += lfo * v->lfo_depth[L];
                    else if (v->lfo_dest[L] == LFO_PAN)       pan_mod  += lfo * v->lfo_depth[L];   // auto-pan (depth 0..1 = full sweep)
                }
                // mod-envelopes (one-shot AD, timer = step_samples): same destinations as the
                // LFOs but a per-note contour instead of a cycle. The pitch env multiplies freq;
                // the cutoff/duty envs add to the same cutoff/duty the LFOs and filter already use.
                for (int e = 0; e < SOUND_ENVS; e++) {
                    if (v->env_amount[e] == 0.0f) continue;
                    float lvl = sound_ad_env(v->step_samples, v->env_a_samp[e], v->env_d_samp[e]);
                    if (lvl <= 0.0f) continue;
                    float m = lvl * v->env_amount[e];
                    if      (v->env_dest[e] == ENV_PITCH)  pitch_mul *= powf(2.0f, m / 12.0f);
                    else if (v->env_dest[e] == ENV_CUTOFF) cutoff    += m;
                    else if (v->env_dest[e] == ENV_DUTY)   duty      += m;
                    else if (v->env_dest[e] == ENV_HARMONICS) harm_mod += m;   // engine macros (one-shot contour)
                    else if (v->env_dest[e] == ENV_TIMBRE)    timb_mod += m;
                    else if (v->env_dest[e] == ENV_MORPH)     mor_mod  += m;
                }
                // envelope follower: the 3rd mod source — uses LAST sample's tracked level
                // (flw_amp, updated just after the oscillator below), so it modulates from the
                // voice's own amplitude. The touch-responsive auto-wah when dest = cutoff.
                if (v->flw_amount != 0.0f) {
                    float fm = v->flw_amp * v->flw_amount;
                    if      (v->flw_dest == LFO_CUTOFF) cutoff    += fm;
                    else if (v->flw_dest == LFO_PITCH)  pitch_mul *= powf(2.0f, fm / 12.0f);
                    else if (v->flw_dest == LFO_VOLUME) { float d = fm < 0.0f ? 0.0f : (fm > 1.0f ? 1.0f : fm); trem *= 1.0f - d; }
                }
                if (duty < 0.05f) duty = 0.05f;
                if (duty > 0.95f) duty = 0.95f;
                // slot detune (instrument_tune): read LIVE from the bank each sample,
                // so ringing voices — scheduled arp/seq hits included — bend with the knob
                if (v->instr_slot >= 0 && v->instr_slot < SOUND_INSTR_SLOTS)
                    pitch_mul *= instr_bank[v->instr_slot].tune_mul;
            }

            // engine fork: wavetable oscillators below INSTR_ENGINE_BASE, modeled engines at/above
            float s;
            if (v->wave >= INSTR_ENGINE_BASE) {
                // macro modulation: the engines read v->harm/timb/mor directly, so apply the
                // LFO/env offset by temporarily nudging them, then restore (the slewed base is
                // untouched, next sample re-applies). Zero-cost when nothing modulates a macro.
                float h0 = v->harm, t0 = v->timb, m0 = v->mor;
                if (harm_mod != 0.0f) v->harm = v->harm + harm_mod < 0 ? 0 : (v->harm + harm_mod > 1 ? 1 : v->harm + harm_mod);
                if (timb_mod != 0.0f) v->timb = v->timb + timb_mod < 0 ? 0 : (v->timb + timb_mod > 1 ? 1 : v->timb + timb_mod);
                if (mor_mod  != 0.0f) v->mor  = v->mor  + mor_mod  < 0 ? 0 : (v->mor  + mor_mod  > 1 ? 1 : v->mor  + mor_mod);
                s = sound_engine_sample(v, pitch_mul);
                v->harm = h0; v->timb = t0; v->mor = m0;
            } else {
                s = sound_osc(v->wave, v->phase, duty, &v->noise_state);
            }
            // envelope follower: track the PRE-filter amplitude (|s| scaled by velocity) with a
            // fast-attack/slow-release peak detector. Used by next sample's modulation above —
            // a 1-sample feedback, inaudible. This is what makes the auto-wah respond to touch.
            if (v->flw_amount != 0.0f) {
                float amp = (s < 0.0f ? -s : s) * v->vol;
                v->flw_amp += ((amp > v->flw_amp) ? v->flw_atk : v->flw_rel) * (amp - v->flw_amp);
            }
            if (v->sfx_idx < 0 && v->flt_mode != FILTER_OFF) {
                if (cutoff < 20.0f) cutoff = 20.0f;
                if (cutoff > SOUND_SAMPLE_RATE * 0.45f) cutoff = SOUND_SAMPLE_RATE * 0.45f;
                s = sound_svf(v, s, cutoff);
            }
            // drive: post-filter saturation — osc → SVF → drive → VCA, so resonance
            // screams INTO the saturation and quiet envelope tails don't pump it. The
            // pre-gain g grows from 0 (shape(s·g)/shape(g) → s as g → 0, so drive 0 is a
            // true bypass and a slewed sweep through 0 stays continuous) to wall-of-fuzz;
            // normalizing by shape(g) keeps full-scale at full-scale — character, not volume.
            // drv_mode picks the waveshaper (DRIVE_*); all four bypass at 0 and hold full-scale.
            if (v->sfx_idx < 0 && v->drv > 0.001f) {
                float dr = v->drv;
                float g  = dr * dr * 24.0f;
                switch (v->drv_mode) {
                    case 1: {  // DRIVE_HARD — hard clip, normalized so g<1 is bypass, g>=1 clips
                        float hg = g < 1.0f ? g : 1.0f;
                        float c  = s * g;
                        c = c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
                        s = c / hg;
                    } break;
                    case 2: {  // DRIVE_FOLD — sine wavefolder, dry/wet by amount (bounded, no divide)
                        float w = sinf(s * (1.0f + dr * 6.0f) * 1.2f);
                        s = s * (1.0f - dr) + w * dr;
                    } break;
                    case 3: {  // DRIVE_ASYM — even-harmonic tube: softer negative half, asymmetry grows with drive
                        float ng = tanhf(g);
                        s = (s >= 0.0f) ? tanhf(s * g) / ng
                                        : tanhf(s * g * (1.0f - 0.4f * dr)) / ng;
                    } break;
                    default:   // DRIVE_SOFT (0) — tanh soft-clip (bit-identical to pre-modes)
                        s = tanhf(s * g) / tanhf(g);
                }
                // DC blocker: tanh of an asymmetric wave (e.g. a driven organ registration)
                // injects a DC offset, which the per-note env ramp turns into a click/thump on
                // attack + release. One-pole HP ~7Hz removes it. Only runs when driven, so clean
                // voices stay bit-identical. (2nd customer for a DC blocker after INSTR_EPIANO.)
                float dcin = s;
                s = dcin - v->drv_dc_x1 + 0.999f * v->drv_dc_y1;
                v->drv_dc_x1 = dcin;
                v->drv_dc_y1 = s;
            }
            float contrib = s * v->vol * env * trem * 0.2f;
            // stereo pan (stereo.md): clamp the slewed base pan + any LFO_PAN offset to [-1,1],
            // then split into L/R gains by the master pan law.
            float pan = v->pan + pan_mod;
            if (pan < -1.0f) pan = -1.0f; else if (pan > 1.0f) pan = 1.0f;
            float pL, pR;
            if (g_pan_law == PAN_POWER) {
                // constant-power: pan [-1,1] → angle [0, π/2]; pL²+pR²=1, center = 0.707 (-3dB).
                // equal loudness across the sweep. NOT byte-identical to mono — opt-in via pan_law().
                float th = (pan + 1.0f) * 0.78539816f;   // (pan+1)·π/4
                pL = cosf(th);
                pR = sinf(th);
            } else {
                // linear (default): pan 0 → pL=pR=1 (a centered voice is byte-identical to mono).
                pL = pan <= 0.0f ? 1.0f : 1.0f - pan;
                pR = pan >= 0.0f ? 1.0f : 1.0f + pan;
            }
            float cL = contrib * pL, cR = contrib * pR;
            v->last_outL = cL; v->last_outR = cR;   // panned contributions feed the steal declick
            busL[v->bus] += cL; busR[v->bus] += cR;   // route into this voice's insert bus (0 = master)
            if (v->sfx_idx < 0 && v->eko > 0.0005f) echo_in   += contrib * v->eko;   // echo send   — MONO, pre-pan
            if (v->sfx_idx < 0 && v->rvb > 0.0005f) {                                // reverb send — MONO, pre-pan
                if (v->rvb_bus == 0) reverb_in += contrib * v->rvb;                  // tank 0 = master parallel send (legacy, unchanged)
                else { float rs = contrib * v->rvb; busL[v->rvb_bus] += rs; busR[v->rvb_bus] += rs; }  // into a reverb send-bus (FX_REVERB wet-replaces it)
            }
            if (v->sfx_idx < 0 && v->sc_send > 0.0005f) sc_key_lvl[v->sc_key] += contrib * v->sc_send;  // sidechain trigger send (MONO, pre-pan)

            v->phase += v->freq * pitch_mul / (float)SOUND_SAMPLE_RATE;
            if (v->phase >= 1.0f) v->phase -= 1.0f;
            v->step_samples++;
        }

        // ── aux insert buses (1..N-1): run each live bus's chorus→flanger chain, then return to
        // master (bus 0). A slot routed here (instrument_chorus/flanger) is effected in isolation;
        // everything on bus 0 bypasses. All-bus-0 carts skip this entirely → byte-identical.
        for (int b = 1; b < SOUND_FX_BUSES; b++) {
            // run this bus's inserts in its (reorderable) order, then fold to master. Default order
            // = the old fixed ladder (trem→wah→cho→phaser→flg→tape→eq→crush); fx_order() rearranges.
            for (int s = 0; s < insert_order_n[b]; s++) apply_insert(insert_order[b][s], b, &busL[b], &busR[b]);
            if (leslie_used[b]) leslie_process(b, &busL[b], &busR[b]);   // rotary speaker — pinned LAST (cabinet output stage, not a reorderable pedal)
            if (sc[b].used) { float g = sc_apply(b, busL[b], busR[b]); busL[b] *= g; busR[b] *= g; }   // sidechain/glue duck (pinned after inserts)
            busL[0] += busL[b]; busR[0] += busR[b];
        }

        // steal-declick: when an audibly-ringing voice is stolen, its output step-discontinuity
        // is paid into this tail (→ master), which fades over ~3ms — the pop becomes a soft tick.
        busL[0] += steal_tailL; busR[0] += steal_tailR;
        steal_tailL *= 0.992f; steal_tailR *= 0.992f;
        if (steal_tailL > -1e-5f && steal_tailL < 1e-5f) steal_tailL = 0.0f;
        if (steal_tailR > -1e-5f && steal_tailR < 1e-5f) steal_tailR = 0.0f;

        float mixL = busL[0], mixR = busR[0];   // master bus → the send-returns + master inserts + clip below

        // ════ MASTER FX SECTION (instrument-engines §8.10) ════════════════════════════════════
        // The summed dry mix (mixL/mixR + steal tail) above is bus 0's input. Two ordered stages:
        //   1. SEND RETURNS — shared parallel processors (echo, reverb). Each got a per-slot send
        //      accumulated during the voice loop (echo_in/reverb_in, MONO pre-pan); its wet adds
        //      back here. Add another send (e.g. a chorus bus) by mirroring these two blocks.
        //   2. INSERT CHAIN — series processors the whole mix passes THROUGH, ending in the
        //      soft-clip limiter (always last). Add a master insert (tape, comp, bitcrush) as a
        //      new line just BEFORE the soft-clip — see decision 0015 / sound-next-steps.md.
        // Each send is dormant until its API is first called, so a cart using neither is byte-
        // identical to pre-FX output. Side-chain seam (future vocoder / sidechain-comp, §8.10):
        // a control input would tap a source bus's level HERE, before the insert chain — not built.
        // Per-slot AUX routing (instrument_bus) is the deferred next increment; v1 is master-only.

        // SEND RETURN 1 — THE echo bus (dormant until the first echo API call)
        if (echo_used) {
            // tape-speed time slew: the read tap glides toward its target with a clamped
            // per-sample step, so a live time sweep pitch-bends the ringing tail (RE-201)
            float dstep = (echo_time_target - echo_time) * 0.0003f;
            if (dstep >  0.5f) dstep =  0.5f;
            if (dstep < -0.5f) dstep = -0.5f;
            echo_time += dstep;
            // fractional read tap behind the write head
            float rp = (float)echo_widx - echo_time;
            if (rp < 0.0f) rp += (float)SOUND_ECHO_MAX;
            int   r0 = (int)rp, r1 = r0 + 1;
            if (r1 >= SOUND_ECHO_MAX) r1 = 0;
            float fr  = rp - (float)r0;
            float tap = echo_buf[r0] + (echo_buf[r1] - echo_buf[r0]) * fr;
            // the loop filter: every repeat passes through it once → darker each pass
            echo_lp += (tap - echo_lp) * echo_tone_coef;
            // write input + feedback through a tanh: feedback >1.0 saturates into a
            // self-oscillation plateau instead of blowing up — the tape echo behaviour
            echo_buf[echo_widx] = tanhf(echo_in + echo_lp * echo_fb);
            if (++echo_widx >= SOUND_ECHO_MAX) echo_widx = 0;
            mixL += echo_lp; mixR += echo_lp;   // echo is a MONO bus in v1 — wet adds to both channels equally (centered)
        }

        // SEND RETURN 2 — THE reverb bus (dormant until the first reverb API call)
        if (reverb_used) {
            float wet = reverb_process(&rvb_tank[0], reverb_in);   // tank 0 = the master send; navkit Schroeder core, MONO in v1
            mixL += wet; mixR += wet;                // wet adds to both channels equally (centered)
        }

        // INSERT CHAIN — MASTER (bus 0) inserts, on the whole mix (chorus()/flanger()/… configure
        // these). Per-instrument inserts already ran on their aux buses above. Run in bus 0's
        // (reorderable) order — default = the old fixed ladder; fx_order(0, …) rearranges. Ends in
        // the soft-clip below (pinned last — a safety limiter, not a reorderable pedal).
        for (int s = 0; s < insert_order_n[0]; s++) apply_insert(insert_order[0][s], 0, &mixL, &mixR);
        if (leslie_used[0]) leslie_process(0, &mixL, &mixR);   // rotary speaker — pinned after the inserts, before the soft-clip (cabinet output stage)
        if (sc[0].used) { float g = sc_apply(0, mixL, mixR); mixL *= g; mixR *= g; }   // master sidechain/glue duck — the summed-bus pump (before the clip)

        // master soft-clip: linear below the ±0.8 knee (quiet mixes stay bit-identical),
        // tanh-shaped above, asymptote at ±1.0 — a hot sum folds over smoothly instead of
        // slamming a hard wall. STEREO (stereo.md bite #5): derive ONE gain from the PEAK of the
        // two channels and apply it to both, so the L/R ratio (the pan position) is preserved —
        // clipping each channel independently would make a hard-panned hot signal's apparent pan
        // wander under load. Centered (|mixL| == |mixR| == |mix|) this reduces to the old scalar
        // soft-clip exactly: gain·mix = clipped, so each channel is byte-identical to mono.
        float pk = fabsf(mixL) > fabsf(mixR) ? fabsf(mixL) : fabsf(mixR);
        if (pk > 0.8f) {
            float clamped = 0.8f + 0.2f * tanhf((pk - 0.8f) * 5.0f);
            float g = clamped / pk;
            mixL *= g; mixR *= g;
        }
#ifdef DE_AUDIO_WORKLET
        out[2 * i]     = mixL * master_gain;           // worklet: pause-mute via the mixer
        out[2 * i + 1] = mixR * master_gain;
#else
        out[2 * i]     = mixL;
        out[2 * i + 1] = mixR;
#endif
        if (wavcap_state == 1) {                       // WAV capture tap (wav_request) — MONO downmix
            wavcap_buf[wavcap_pos++] = (mixL + mixR) * 0.5f;   // (L+R)/2 == the old mono mix when centered
            if (wavcap_pos >= wavcap_total) wavcap_state = 2;
        }
    }
}

// ───────── built-in demo sfx ─────────
// Slots 0–5 ship pre-filled so sfx(0) makes a sound on first contact. Carts don't
// author these banks — cart sound is written as code (note/hit/schedule, or the
// "sfx editor" cart's export-as-C). That's settled: see decisions/ (music() cut).

static void sound_load_demo_data(void) {
    // sfx 0 — ascending blip (coin pickup)
    sfx_bank[0] = (Sfx){
        .step_dur = 3, .length = 4, .loop = 0,
        .steps = { {72, INSTR_SQUARE, 6}, {76, INSTR_SQUARE, 6}, {79, INSTR_SQUARE, 6}, {84, INSTR_SQUARE, 7} },
    };
    // sfx 1 — descending zap (hurt)
    sfx_bank[1] = (Sfx){
        .step_dur = 3, .length = 6, .loop = 0,
        .steps = { {84, INSTR_SAW, 7}, {78, INSTR_SAW, 6}, {72, INSTR_SAW, 5}, {66, INSTR_SAW, 4}, {60, INSTR_SAW, 3}, {54, INSTR_SAW, 2} },
    };
    // sfx 2 — jump (rising tri)
    sfx_bank[2] = (Sfx){
        .step_dur = 2, .length = 5, .loop = 0,
        .steps = { {60, INSTR_TRI, 6}, {64, INSTR_TRI, 6}, {67, INSTR_TRI, 5}, {72, INSTR_TRI, 4}, {76, INSTR_TRI, 3} },
    };
    // sfx 3 — explosion (noise burst)
    sfx_bank[3] = (Sfx){
        .step_dur = 4, .length = 5, .loop = 0,
        .steps = { {60, INSTR_NOISE, 7}, {60, INSTR_NOISE, 6}, {60, INSTR_NOISE, 5}, {60, INSTR_NOISE, 3}, {60, INSTR_NOISE, 1} },
    };
    // sfx 4 — bass loop (loop-flag demo)
    sfx_bank[4] = (Sfx){
        .step_dur = 12, .length = 4, .loop = 1,
        .steps = { {36, INSTR_TRI, 5}, {36, INSTR_TRI, 5}, {43, INSTR_TRI, 5}, {41, INSTR_TRI, 5} },
    };
    // sfx 5 — hihat loop (loop-flag demo)
    sfx_bank[5] = (Sfx){
        .step_dur = 6, .length = 4, .loop = 1,
        .steps = { {60, INSTR_NOISE, 3}, {0,0,0}, {60, INSTR_NOISE, 3}, {0,0,0} },
    };
}

// ───────── public API ─────────

void sfx(int n) {
    if (n < -1 || n >= SOUND_SFX_SLOTS) return;
    sound_push_req(SR_SFX, n, 0, 0, 0, 0);
}

// Look up chord intervals (used by chord() and strum()).
static int sound_chord_intervals(int type, const int8_t **out) {
    static const int8_t maj[]   = { 0, 4, 7 };
    static const int8_t min[]   = { 0, 3, 7 };
    static const int8_t dim[]   = { 0, 3, 6 };
    static const int8_t aug[]   = { 0, 4, 8 };
    static const int8_t maj7[]  = { 0, 4, 7, 11 };
    static const int8_t min7[]  = { 0, 3, 7, 10 };
    static const int8_t dom7[]  = { 0, 4, 7, 10 };
    static const int8_t sus4[]  = { 0, 5, 7 };
    static const int8_t power[] = { 0, 7 };
    switch (type) {
        case 0: *out = maj;   return 3;
        case 1: *out = min;   return 3;
        case 2: *out = dim;   return 3;
        case 3: *out = aug;   return 3;
        case 4: *out = maj7;  return 4;
        case 5: *out = min7;  return 4;
        case 6: *out = dom7;  return 4;
        case 7: *out = sus4;  return 3;
        case 8: *out = power; return 2;
        default: *out = maj;  return 3;
    }
}

void chord(int root, int type, int instr, int vol) {
    const int8_t *ivl;
    int n = sound_chord_intervals(type, &ivl);
    for (int i = 0; i < n; i++) note(root + ivl[i], instr, vol);
}

void strum(int root, int type, int instr, int vol, int delay_ms) {
    const int8_t *ivl;
    int n = sound_chord_intervals(type, &ivl);
    int dir = delay_ms < 0 ? -1 : 1;            // negative = down-strum (high → low)
    int abs_ms = delay_ms < 0 ? -delay_ms : delay_ms;
    for (int i = 0; i < n; i++) {
        int idx   = (dir > 0) ? i : (n - 1 - i);
        int delay = (i * abs_ms * SOUND_SAMPLE_RATE) / 1000;
        sound_push_req(SR_NOTE, root + ivl[idx], instr, vol, delay, 0);
    }
}

// Look up scale intervals (used by tone() and degree()).
static int sound_scale_table(int scale_id, const uint8_t **out) {
    static const uint8_t major[]      = { 0, 2, 4, 5, 7, 9, 11 };
    static const uint8_t minor[]      = { 0, 2, 3, 5, 7, 8, 10 };
    static const uint8_t penta[]      = { 0, 2, 4, 7, 9 };
    static const uint8_t penta_min[]  = { 0, 3, 5, 7, 10 };
    static const uint8_t blues[]      = { 0, 3, 5, 6, 7, 10 };
    static const uint8_t chromatic[]  = { 0,1,2,3,4,5,6,7,8,9,10,11 };
    switch (scale_id) {
        case 0: *out = major;     return 7;
        case 1: *out = minor;     return 7;
        case 2: *out = penta;     return 5;
        case 3: *out = penta_min; return 5;
        case 4: *out = blues;     return 6;
        case 5: *out = chromatic; return 12;
        default: *out = penta;    return 5;
    }
}

void tone(int scale, int octave, int instr, int vol) {
    const uint8_t *s;
    int n = sound_scale_table(scale, &s);
    int midi = 12 * (octave + 1) + s[rnd(n)];
    note(midi, instr, vol);
}

int degree(int scale, int octave, int n) {
    const uint8_t *s;
    int len = sound_scale_table(scale, &s);
    // wrap n into [0, len), tracking octave displacement for negative or large n
    int oct_off;
    if (n >= 0) {
        oct_off = n / len;
        n       = n % len;
    } else {
        oct_off = -((-n + len - 1) / len);
        n       = ((n % len) + len) % len;
    }
    return 12 * (octave + 1 + oct_off) + s[n];
}

bool euclid(int hits, int steps, int b) {
    if (steps <= 0 || hits <= 0) return false;
    if (hits >= steps)           return true;
    int k = ((b % steps) + steps) % steps;
    return (k * hits) % steps < hits;
}

bool chance(int percent) {
    if (percent <= 0)   return false;
    if (percent >= 100) return true;
    return rnd(100) < percent;
}

float beat_pos(void) {
    return beat_accum - (float)beat_now;
}

void note(int midi, int instr, int vol) {
    sound_push_req(SR_NOTE, midi, instr, vol, 0, 0);
}

void hit(int midi, int instr, int vol, int dur_ms) {
    int dur = (dur_ms * SOUND_SAMPLE_RATE) / 1000;
    if (dur < 1) dur = 1;
    sound_push_req(SR_NOTE, midi, instr, vol, 0, dur);
}

// ── held notes: start a sustained voice, drive it live, release it (see docs/design/held-notes.md) ──

int note_on(int midi, int instr, int vol) {
    int slot = -1;
    for (int i = 0; i < SOUND_VOICES; i++) if (!hn_used[i]) { slot = i; break; }
    if (slot < 0) slot = 0;                      // all slots live → reuse slot 0 (oldest is stolen)
    hn_used[slot] = true;
    if (++hn_gen[slot] <= 0) hn_gen[slot] = 1;   // generations start at 1, stay positive
    int gen = hn_gen[slot];
    sound_push_ctrl(SR_NOTE_ON, midi, instr, vol, slot, gen, 0);
    return slot | (gen << SOUND_HANDLE_BITS);    // handle: slot in the low bits, generation above
}

void note_off(int handle) {
    if (handle <= 0) return;
    int slot = handle & SOUND_HANDLE_MASK;
    hn_used[slot] = false;
    sound_push_ctrl(SR_NOTE_OFF, 0, 0, 0, slot, handle >> SOUND_HANDLE_BITS, 0);
}

void note_pitch(int handle, float midi) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_PITCH, (int)(midi * 256.0f), 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

void note_vol(int handle, int vol) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_VOL, vol, 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

void note_cutoff(int handle, int hz) {
    if (handle <= 0) return;
    if (hz < 20) hz = 20;
    sound_push_ctrl(SR_NOTE_CUTOFF, hz, 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

void note_res(int handle, int resonance) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_RES, resonance, 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

void note_lfo(int handle, int which, int dest, float rate_hz, float depth) {
    if (handle <= 0) return;
    if (which < 0 || which >= SOUND_LFOS) return;
    if (rate_hz < 0) rate_hz = 0;
    if (depth   < 0) depth   = 0;
    sound_push_ctrl(SR_NOTE_LFO, which, dest, (int)(rate_hz * 1000.0f), handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, (int)(depth * 1000.0f));
}

void note_filter(int handle, int mode) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_FILTER, mode, 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

void note_glide(int handle, int ms) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_GLIDE, ms, 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

void note_duty(int handle, float duty) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_DUTY, (int)(duty * 1000.0f), 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

void note_off_all(void) {
    for (int i = 0; i < SOUND_VOICES; i++) hn_used[i] = false;
    sound_push_ctrl(SR_NOTE_OFF_ALL, 0, 0, 0, 0, 0, 0);
}

void instrument(int slot, int wave, int attack_ms, int decay_ms, int sustain, int release_ms) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    int a = (attack_ms  * SOUND_SAMPLE_RATE) / 1000;
    int d = (decay_ms   * SOUND_SAMPLE_RATE) / 1000;
    int r = (release_ms * SOUND_SAMPLE_RATE) / 1000;
    if (a < 0) a = 0;
    if (d < 0) d = 0;
    if (r < 0) r = 0;
    sound_push_ctrl(SR_INSTR, slot, wave, sustain, a, d, r);
}

void wave_set(int which, const float *samples, int n) {
    if (which < 0 || which >= SOUND_USER_WAVES || !samples || n < 2) return;
    // resample the drawn cycle (any length) to the 64-entry table and ride the request queue
    // like every other mutation (no tearing). Packed 4 samples per request — 16 requests per
    // wave — so an init() that sets all four tables plus dozens of instruments fits the queue.
    for (int i = 0; i < SOUND_WAVE_LEN; i += 4) {
        int q[4];
        for (int k = 0; k < 4; k++) {
            float fp = (i + k) * (float)n / (float)SOUND_WAVE_LEN;
            int   i0 = (int)fp;
            float fr = fp - (float)i0;
            float a = samples[i0 % n], b = samples[(i0 + 1) % n];   // wrap: it's one cycle
            float v = a + (b - a) * fr;
            if (v >  1.0f) v =  1.0f;
            if (v < -1.0f) v = -1.0f;
            q[k] = (int)(v * 32767.0f);
        }
        sound_push_ctrl(SR_WAVE_SET, which, i, q[0], q[1], q[2], q[3]);
    }
}

void instrument_duty(int slot, float duty) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_DUTY, slot, (int)(duty * 1000.0f), 0, 0, 0, 0);
}

void instrument_mode(int slot, int idx, float value) {   // per-engine aux channel, note-on face (was eng_tune; decision 0017)
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS || idx < 0 || idx >= 2) return;
    if (value < 0.0f) value = 0.0f; if (value > 1.0f) value = 1.0f;
    sound_push_ctrl(SR_ENG_TUNE, slot, idx, (int)(value * 1000.0f), 0, 0, 0);
}

void instrument_pan(int slot, float pan) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_PAN, slot, (int)(pan * 1000.0f), 0, 0, 0, 0);
}

void note_pan(int handle, float pan) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_PAN, (int)(pan * 1000.0f), 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

void pan_law(int law) {
    sound_push_ctrl(SR_PAN_LAW, law, 0, 0, 0, 0, 0);
}

void instrument_lfo(int slot, int which, int dest, float rate_hz, float depth) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    if (which < 0 || which >= SOUND_LFOS) return;
    if (rate_hz < 0) rate_hz = 0;
    if (depth   < 0) depth   = 0;
    sound_push_ctrl(SR_INSTR_LFO, slot, dest, (int)(rate_hz * 1000.0f), (int)(depth * 1000.0f), which, 0);
}

void instrument_filter(int slot, int mode, int cutoff_hz, int resonance) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    if (cutoff_hz < 20) cutoff_hz = 20;
    sound_push_ctrl(SR_INSTR_FILTER, slot, mode, cutoff_hz, resonance, 0, 0);
}

void instrument_env(int slot, int which, int dest, int attack_ms, int decay_ms, float amount) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    if (which < 0 || which >= SOUND_ENVS) return;
    int a = (attack_ms * SOUND_SAMPLE_RATE) / 1000;
    int d = (decay_ms  * SOUND_SAMPLE_RATE) / 1000;
    if (a < 0) a = 0;
    if (d < 0) d = 0;
    sound_push_ctrl(SR_INSTR_ENV, slot, which, dest, a, d, (int)(amount * 1000.0f));
}

void instrument_follow(int slot, int dest, int attack_ms, int release_ms, float amount) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_FOLLOW, slot, dest, attack_ms, release_ms, 0, (int)(amount * 1000.0f));
}
void note_follow(int handle, int dest, int attack_ms, int release_ms, float amount) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_FOLLOW, dest, attack_ms, release_ms, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, (int)(amount * 1000.0f));
}

// ── engine macros: three 0..1 knobs every modeled engine answers — six functions, forever
//    (the count never grows with the engine roster; see audio-notes §8.1.1) ──

static void sound_macro_slot(int slot, int which, float x) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_MACRO, slot, which, (int)(x * 1000.0f), 0, 0, 0);
}
static void sound_macro_note(int handle, int which, float x) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_MACRO, which, (int)(x * 1000.0f), 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}
void instrument_harmonics(int slot, float x) { sound_macro_slot(slot, 0, x); }
void instrument_timbre(int slot, float x)    { sound_macro_slot(slot, 1, x); }
void instrument_morph(int slot, float x)     { sound_macro_slot(slot, 2, x); }
void instrument_choke(int slot_a, int slot_b) {
    if (slot_a < 0 || slot_a >= SOUND_INSTR_SLOTS) return;
    if (slot_b < 0 || slot_b >= SOUND_INSTR_SLOTS) return;
    sound_push_req(SR_INSTR_CHOKE, slot_a, slot_b, 0, 0, 0);
}
void note_harmonics(int handle, float x)     { sound_macro_note(handle, 0, x); }
void note_timbre(int handle, float x)        { sound_macro_note(handle, 1, x); }
void note_morph(int handle, float x)         { sound_macro_note(handle, 2, x); }

// INSTR_VOICE nasal color (the public 4th axis): 0 = open vowel → 1 = hummed/nasal (the honk, the
// chant). Set live on a held note, alongside the 3 macros (harmonics=vowel/timbre=size/morph=effort).
void voice_nasal(int handle, float amount) {
    if (handle <= 0) return;
    if (amount < 0.0f) amount = 0.0f; else if (amount > 1.0f) amount = 1.0f;
    sound_push_ctrl(SR_VOICE_NASAL, (int)(amount * 1000.0f), 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

// LOW-LEVEL / EXPERIMENTAL — poke a raw INSTR_VOICE param by index on a held note. The public
// surface is the 3 macros + voice_nasal(); this raw poke is the probe carts' fat control surface
// (voxlab/voxab/voxpad/say). idx 0..18 (see VOX_NPARAM), value 0..1. Not advertised in the docs.
void note_aux(int handle, int idx, float value) {   // per-engine aux channel, live face (was voice_param; decision 0017)
    if (handle <= 0 || idx < 0 || idx >= VOX_NPARAM) return;
    sound_push_ctrl(SR_VOICE_PARAM, idx, (int)(value * 1000.0f), 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

// Begin a held INSTR_VOICE note with a consonant articulation that morphs into the vowel
// ("bah", "mah", "sss-ah"). Call right after note_on; id 0..21 (b d g m n l s sh ng r w y dh f v
// z zh th p t k ch), -1 = none. A timed onset, not a held param. Pair with voice_coda for VC/CVC.
void voice_consonant(int handle, int id) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_VOICE_CONS, id, 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

// Close a held INSTR_VOICE note ON a consonant: the vowel morphs into it ("ahh-m", "oo-d"). Call
// right BEFORE note_off; id 0..21, -1 = none. The mirror of voice_consonant — a coda not an onset;
// use voiced ids (b d g m n l ng r w y) so it stays sung.
void voice_coda(int handle, int id) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_VOICE_CODA, id, 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

// ── tune: per-slot detune in semitones (fractions are the point) — applies LIVE to every
//    sounding voice on the slot, fire-and-forget hits included (the SH-101 TUNE trimmer,
//    unison-detune pairs, gamelan ombak) ──

void instrument_tune(int slot, float semitones) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_TUNE, slot, (int)(semitones * 1000.0f), 0, 0, 0, 0);
}

// ── drive: post-filter tanh saturation, per slot (audio-notes §17 — the missing nonlinearity) ──

void instrument_drive(int slot, float x) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_DRIVE, slot, (int)(x * 1000.0f), 0, 0, 0, 0);
}

void note_drive(int handle, float x) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_DRIVE, (int)(x * 1000.0f), 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

void instrument_drive_mode(int slot, int mode) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_DRIVE_MODE, slot, mode, 0, 0, 0, 0);
}

void note_drive_mode(int handle, int mode) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_DRIVE_MODE, mode, 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

// ── echo: ONE shared bus with per-slot sends (audio-notes §17 step 3, decisions/0015) ──

void echo(int time_ms, float feedback, float tone) {
    sound_push_ctrl(SR_ECHO, time_ms, (int)(feedback * 1000.0f), (int)(tone * 1000.0f), 0, 0, 0);
}

void instrument_echo(int slot, float send) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_ECHO, slot, (int)(send * 1000.0f), 0, 0, 0, 0);
}

void note_echo(int handle, float x) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_ECHO, (int)(x * 1000.0f), 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

// ── delay insert: the in-line dry/wet delay PEDAL (echo as a reorderable FX_ECHO insert) ──

void echo_insert(int time_ms, float feedback, float tone, float mix) {
    sound_push_ctrl(SR_ECHO_INSERT, time_ms, (int)(feedback * 1000.0f), (int)(tone * 1000.0f), (int)(mix * 1000.0f), 0, 0);
}

// ── grains: granular delay — a capture-and-scatter texture/freeze cloud (navkit port) ──
void grains(float grain_ms, float density, float position, float scatter, float feedback, float mix) {
    sound_push_ctrl(SR_GRAINS, (int)grain_ms, (int)(density * 100.0f), (int)(position * 1000.0f),
                    (int)(scatter * 1000.0f), (int)(feedback * 1000.0f), (int)(mix * 1000.0f));
}
void instrument_grains(int slot, float grain_ms, float density, float position, float scatter, float feedback, float mix) {
    if (feedback < 0.0f) feedback = 0.0f; if (feedback > 0.95f) feedback = 0.95f;
    if (mix < 0.0f) mix = 0.0f; if (mix > 1.0f) mix = 1.0f;
    int packed = (int)(feedback * 100.0f) * 1001 + (int)(mix * 1000.0f);   // e2 = PACK(feedback, mix) — 6-int request budget
    sound_push_ctrl(SR_INSTR_GRAINS, slot, (int)grain_ms, (int)(density * 100.0f),
                    (int)(position * 1000.0f), (int)(scatter * 1000.0f), packed);
}
void grains_freeze(int on) {
    sound_push_ctrl(SR_GRAINS_FREEZE, on ? 1 : 0, 0, 0, 0, 0, 0);
}
void instrument_grains_freeze(int slot, int on) {
    sound_push_ctrl(SR_INSTR_GRAINS_FREEZE, slot, on ? 1 : 0, 0, 0, 0, 0);
}

// ── reverb: ONE shared bus with per-slot sends (the first §8.10 effect; decisions/0015) ──

void reverb(float size, float damping) {
    sound_push_ctrl(SR_REVERB, (int)(size * 1000.0f), (int)(damping * 1000.0f), 0, 0, 0, 0);
}

void instrument_reverb(int slot, float send) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_REVERB, slot, (int)(send * 1000.0f), 0, 0, 0, 0);
}

void note_reverb(int handle, float x) {
    if (handle <= 0) return;
    sound_push_ctrl(SR_NOTE_REVERB, (int)(x * 1000.0f), 0, 0, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, 0);
}

// reverb SEND-BUSES (Increment C). reverb_bus() turns tank 1..N-1 into a real space on its own aux
// bus; instrument_reverb_bus() points a slot's send at it. Then fx_order(that bus, {FX_REVERB, …})
// runs pedals on the wet tail. Tank 0 stays reverb()'s master send.
void reverb_bus(int tank, float size, float damp) {
    sound_push_ctrl(SR_REVERB_BUS, tank, (int)(size * 1000.0f), (int)(damp * 1000.0f), 0, 0, 0);
}

void instrument_reverb_bus(int slot, int tank, float mix) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_REVERB_BUS, slot, tank, (int)(mix * 1000.0f), 0, 0, 0);
}

// reverb as a dry/wet-MIX INSERT on the master bus — a real reorderable pedal (unlike reverb(), a
// parallel send whose chain position is cosmetic). Put FX_REVERB in your fx_order(0, …) list to set
// where it sits; mix 0..1 (0 = bypass dry). For pedalboards: drag it before/after crush and hear it.
void reverb_insert(float size, float damp, float mix) {
    sound_push_ctrl(SR_REVERB_INSERT, (int)(size * 1000.0f), (int)(damp * 1000.0f), (int)(mix * 1000.0f), 0, 0, 0);
}

// sidechain & bus compression — summed-signal DYNAMICS (studio.h). sidechain ducks a victim bus on
// a trigger key (the pump); sidechain_key routes a slot (the kick) into a key; glue is the trigger-
// less bus compressor. amount 0 = dormant → byte-identical.
void sidechain(int victim_bus, int key, float amount, int attack_ms, int release_ms) {
    sound_push_ctrl(SR_SIDECHAIN, victim_bus, key, (int)(amount * 1000.0f), attack_ms, release_ms, 0);
}
void sidechain_key(int slot, int key, float send) {
    sound_push_ctrl(SR_SIDECHAIN_KEY, slot, key, (int)(send * 1000.0f), 0, 0, 0);
}
void glue(int victim_bus, float amount, int attack_ms, int release_ms) {
    sound_push_ctrl(SR_GLUE, victim_bus, (int)(amount * 1000.0f), attack_ms, release_ms, 0, 0);
}
// THE master resonant filter (the DJ filter) — mode FILTER_*, cutoff Hz, resonance 0..1. Cheap to
// re-call live (ride the cutoff for the build-up/breakdown sweep). FILTER_OFF = bypass.
void filter(int mode, float cutoff_hz, float resonance) {
    sound_push_ctrl(SR_FILTER, mode, (int)cutoff_hz, (int)(resonance * 1000.0f), 0, 0, 0);
}

// add an effect AFTER the reverb on tank N's bus (effects-after-reverb — reverb→crush/eq/tape).
// fx = FX_CRUSH/FX_EQ/FX_TAPE/FX_CHORUS; a/b/c are that effect's own params (×1000 packed, same
// scale as its dedicated API). Appends to the bus chain in call order; call reverb_bus(tank,…) first.
void reverb_bus_fx(int tank, int fx, float a, float b, float c) {
    sound_push_ctrl(SR_REVERB_BUS_FX, tank, fx, (int)(a * 1000.0f), (int)(b * 1000.0f), (int)(c * 1000.0f), 0);
}

// ── chorus: THE master mod-delay (BBD) — first use of the shared modulated-delay buffer ──

void chorus(float rate, float depth, float mix) {
    sound_push_ctrl(SR_CHORUS, (int)(rate * 1000.0f), (int)(depth * 1000.0f), (int)(mix * 1000.0f), 0, 0, 0);
}

// ── flanger: THE master flanger — second use of the shared modulated-delay technique ──

void flanger(float rate, float depth, float feedback, float mix) {
    sound_push_ctrl(SR_FLANGER, (int)(rate * 1000.0f), (int)(depth * 1000.0f), (int)(feedback * 1000.0f), (int)(mix * 1000.0f), 0, 0);
}

// ── per-instrument inserts: chorus/flanger on ONE instrument (auto-assigns it a private FX bus) ──

void instrument_chorus(int slot, float rate, float depth, float mix) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_CHORUS, slot, (int)(rate * 1000.0f), (int)(depth * 1000.0f), (int)(mix * 1000.0f), 0, 0);
}

void instrument_flanger(int slot, float rate, float depth, float feedback, float mix) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_FLANGER, slot, (int)(rate * 1000.0f), (int)(depth * 1000.0f), (int)(feedback * 1000.0f), (int)(mix * 1000.0f), 0);
}

// ── tape: THE master tape (wow/flutter/saturation) — third use of the modulated-delay technique ──

void tape(float wow, float flutter, float saturation) {
    sound_push_ctrl(SR_TAPE, (int)(wow * 1000.0f), (int)(flutter * 1000.0f), (int)(saturation * 1000.0f), 0, 0, 0);
}

void instrument_tape(int slot, float wow, float flutter, float saturation) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_TAPE, slot, (int)(wow * 1000.0f), (int)(flutter * 1000.0f), (int)(saturation * 1000.0f), 0, 0);
}

// ── auto-wah: THE master auto-wah (envelope-following resonant bandpass — the funk quack) ──

void wah(float sensitivity, float resonance, float mix) {
    sound_push_ctrl(SR_WAH, (int)(sensitivity * 1000.0f), (int)(resonance * 1000.0f), (int)(mix * 1000.0f), 0, 0, 0);
}

void instrument_wah(int slot, float sensitivity, float resonance, float mix) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_WAH, slot, (int)(sensitivity * 1000.0f), (int)(resonance * 1000.0f), (int)(mix * 1000.0f), 0, 0);
}

// LFO-driven auto-wah — the SAME bus bandpass swept by a sine instead of the envelope follower
// (navkit's WAH_MODE_LFO). The rhythmic "wah-wah" that doesn't depend on how hard you play.
void wah_lfo(float rate_hz, float resonance, float mix) {
    sound_push_ctrl(SR_WAH_LFO, (int)(rate_hz * 1000.0f), (int)(resonance * 1000.0f), (int)(mix * 1000.0f), 0, 0, 0);
}

void instrument_wah_lfo(int slot, float rate_hz, float resonance, float mix) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_WAH_LFO, slot, (int)(rate_hz * 1000.0f), (int)(resonance * 1000.0f), (int)(mix * 1000.0f), 0, 0);
}

// ── bitcrush: THE master lo-fi quantizer (bit-depth + sample-rate reduction) ──

void crush(float bits, float rate, float mix) {
    sound_push_ctrl(SR_BITCRUSH, (int)(bits * 100.0f), (int)(rate * 100.0f), (int)(mix * 1000.0f), 0, 0, 0);
}

void instrument_crush(int slot, float bits, float rate, float mix) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_BITCRUSH, slot, (int)(bits * 100.0f), (int)(rate * 100.0f), (int)(mix * 1000.0f), 0, 0);
}

// ── EQ: THE master 3-band tone (low/mid/high) — the library's only BOOST (filters only cut) ──

void eq(float low_gain, float mid_gain, float high_gain) {
    sound_push_ctrl(SR_EQ, (int)(low_gain * 1000.0f), (int)(mid_gain * 1000.0f), (int)(high_gain * 1000.0f), 0, 0, 0);
}

void instrument_eq(int slot, float low_gain, float mid_gain, float high_gain) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_EQ, slot, (int)(low_gain * 1000.0f), (int)(mid_gain * 1000.0f), (int)(high_gain * 1000.0f), 0, 0);
}

void formant(float vowel, float q, float mix) {
    sound_push_ctrl(SR_FORMANT, (int)(vowel * 1000.0f), (int)(q * 1000.0f), (int)(mix * 1000.0f), 0, 0, 0);
}

void instrument_formant(int slot, float vowel, float q, float mix) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_FORMANT, slot, (int)(vowel * 1000.0f), (int)(q * 1000.0f), (int)(mix * 1000.0f), 0, 0);
}

// ── tremolo: THE master tremolo (volume LFO — the Fender/Wurlitzer amp wobble) ──

void tremolo(float rate, float depth, int shape) {
    sound_push_ctrl(SR_TREMOLO, (int)(rate * 1000.0f), (int)(depth * 1000.0f), shape, 0, 0, 0);
}

void instrument_tremolo(int slot, float rate, float depth, int shape) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_TREMOLO, slot, (int)(rate * 1000.0f), (int)(depth * 1000.0f), shape, 0, 0);
}

// ── auto-pan: the tremolo LFO run ANTIPHASE on L/R — the stereo sweep (its own insert, stacks with tremolo) ──

void autopan(float rate, float depth, int shape) {
    sound_push_ctrl(SR_AUTOPAN, (int)(rate * 1000.0f), (int)(depth * 1000.0f), shape, 0, 0, 0);
}

void instrument_autopan(int slot, float rate, float depth, int shape) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_AUTOPAN, slot, (int)(rate * 1000.0f), (int)(depth * 1000.0f), shape, 0, 0);
}

// ── ring modulator: signal × sine carrier — the metallic/clang sidebands ──

void ringmod(float freq_hz, float mix) {
    sound_push_ctrl(SR_RINGMOD, (int)freq_hz, (int)(mix * 1000.0f), 0, 0, 0, 0);
}

void instrument_ringmod(int slot, float freq_hz, float mix) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_RINGMOD, slot, (int)freq_hz, (int)(mix * 1000.0f), 0, 0, 0);
}

// ── phaser: THE master phaser (LFO-swept allpass chain — the 70s Rhodes / Small Stone swirl) ──

void phaser(float rate, float depth, float feedback, float mix, int stages) {
    sound_push_ctrl(SR_PHASER, (int)(rate * 1000.0f), (int)(depth * 1000.0f), (int)(feedback * 1000.0f), (int)(mix * 1000.0f), stages, 0);
}

void instrument_phaser(int slot, float rate, float depth, float feedback, float mix, int stages) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_PHASER, slot, (int)(rate * 1000.0f), (int)(depth * 1000.0f), (int)(feedback * 1000.0f), (int)(mix * 1000.0f), stages);
}

// ── Leslie: THE master rotary speaker (spinning horn+drum cabinet — the organ's voice) ──

void leslie(int speed, float drive, float balance, float doppler, float mix) {
    sound_push_ctrl(SR_LESLIE, speed, (int)(drive * 1000.0f), (int)(balance * 1000.0f), (int)(doppler * 1000.0f), (int)(mix * 1000.0f), 0);
}

void instrument_leslie(int slot, int speed, float drive, float balance, float doppler, float mix) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(SR_INSTR_LESLIE, slot, speed, (int)(drive * 1000.0f), (int)(balance * 1000.0f), (int)(doppler * 1000.0f), (int)(mix * 1000.0f));
}

// set a bus's insert visit order (FX_* kinds). bus 0 = master, 1.. = an instrument's private bus.
// Pack the n kinds at 4 bits each (FX_* now reach 8 = FX_REVERB, so 3 bits no longer fit). 9 slots
// × 4 bits = 36 > 32, so split across two payload ints: slots 0..7 → `b`, slots 8.. → `e0`.
void fx_order(int bus, const int *kinds, int n) {
    if (bus < 0 || bus >= SOUND_FX_BUSES) return;
    if (n < 0) n = 0; if (n > N_INSERTS) n = N_INSERTS;
    int lo = 0, hi = 0;
    for (int s = 0; s < n; s++) {
        int k = kinds[s];
        if (k < 0 || k >= N_INSERTS) k = 0;
        if (s < 8) lo |= (k & 15) << (4 * s);
        else       hi |= (k & 15) << (4 * (s - 8));
    }
    sound_push_ctrl(SR_FX_ORDER, bus, lo, n, hi, 0, 0);
}

void note_env(int handle, int which, int dest, int attack_ms, int decay_ms, float amount) {
    if (handle <= 0) return;
    if (which < 0 || which >= SOUND_ENVS) return;
    int a = (attack_ms * SOUND_SAMPLE_RATE) / 1000;
    int d = (decay_ms  * SOUND_SAMPLE_RATE) / 1000;
    if (a < 0) a = 0;
    if (d < 0) d = 0;
    sound_push_ctrl(SR_NOTE_ENV, ((which & 0xf) << 4) | (dest & 0xf), a, d, handle & SOUND_HANDLE_MASK, handle >> SOUND_HANDLE_BITS, (int)(amount * 1000.0f));
}

void schedule(int delay_ms, int midi, int instr, int vol) {
    int ds = (delay_ms * SOUND_SAMPLE_RATE) / 1000;
    if (ds < 0) ds = 0;
    sound_push_req(SR_NOTE, midi, instr, vol, ds, 0);
}

void schedule_hit(int delay_ms, int midi, int instr, int vol, int dur_ms) {
    int ds  = (delay_ms * SOUND_SAMPLE_RATE) / 1000;
    int dur = (dur_ms   * SOUND_SAMPLE_RATE) / 1000;
    if (ds  < 0) ds  = 0;
    if (dur < 1) dur = 1;
    sound_push_req(SR_NOTE, midi, instr, vol, ds, dur);
}

void bpm(int rate) {
    if (rate < 1)   rate = 1;
    if (rate > 999) rate = 999;
    sound_bpm = rate;
}

int beat(void) {
    return beat_now;
}

bool every(int n) {
    if (n <= 0) n = 1;
    return beat_just_advanced && (beat_now % n) == 0;
}

// ───────── web AudioWorklet backend (Stage 2 — design/audio-threading.md) ─────────
// Compiled only into the worklet build (-DDE_AUDIO_WORKLET, which also implies
// -sAUDIO_WORKLET=1 -sWASM_WORKERS=1 + shared memory). That build only ever runs in a
// cross-origin-isolated context (shared memory won't instantiate otherwise), so when this
// is defined we are ALWAYS isolated and always use the worklet. The plain (no-flag) build
// keeps the ScriptProcessor path below as the runtime fallback (picked by the HTML loader).
#if defined(PLATFORM_WEB) && defined(DE_AUDIO_WORKLET)
#include <emscripten/webaudio.h>

static uint8_t              sound_aw_stack[8192];   // the audio worklet thread's stack
static EMSCRIPTEN_WEBAUDIO_T sound_aw_ctx = 0;

// AUDIO THREAD: bridge emscripten's PLANAR 128-frame quantum to our INTERLEAVED mixer.
// sound_callback is reused verbatim — it drains req_queue (now atomic) and writes
// out[2i]=L, out[2i+1]=R; we split that into the worklet's planar L/R channels.
static bool sound_aw_process(int ni, const AudioSampleFrame *in, int no, AudioSampleFrame *out,
                             int np, const AudioParamFrame *params, void *u) {
    if (no < 1 || out[0].numberOfChannels < 1) return true;
    static float itl[256];                 // 128 frames × 2ch interleaved scratch
    sound_callback(itl, 128);              // our existing mixer, untouched
    float *L = &out[0].data[0];
    if (out[0].numberOfChannels >= 2) {
        float *R = &out[0].data[128];
        for (int i = 0; i < 128; i++) { L[i] = itl[2*i]; R[i] = itl[2*i+1]; }
    } else {
        for (int i = 0; i < 128; i++) L[i] = 0.5f * (itl[2*i] + itl[2*i+1]);
    }
    return true;                           // keep the processor alive
}

static void sound_aw_proc_created(EMSCRIPTEN_WEBAUDIO_T ctx, bool ok, void *u) {
    if (!ok) return;
    int counts[1] = { 2 };                 // stereo out
    EmscriptenAudioWorkletNodeCreateOptions o = {
        .numberOfInputs = 0, .numberOfOutputs = 1, .outputChannelCounts = counts
    };
    EMSCRIPTEN_AUDIO_WORKLET_NODE_T node =
        emscripten_create_wasm_audio_worklet_node(ctx, "destudio", &o, &sound_aw_process, 0);
    emscripten_audio_node_connect(node, ctx, 0, 0);
}

static void sound_aw_thread_ready(EMSCRIPTEN_WEBAUDIO_T ctx, bool ok, void *u) {
    if (!ok) return;
    WebAudioWorkletProcessorCreateOptions opts = { .name = "destudio" };
    emscripten_create_wasm_audio_worklet_processor_async(ctx, &opts, sound_aw_proc_created, 0);
}

// kicks off the async chain: create context → start audio thread → create processor →
// create node → connect. Audio flows once the node connects (a few ms later); the
// req_queue buffers any pushes in the meantime.
static void sound_worklet_init(void) {
    sound_aw_ctx = emscripten_create_audio_context(0);
    emscripten_start_wasm_audio_worklet_thread_async(
        sound_aw_ctx, sound_aw_stack, sizeof sound_aw_stack, sound_aw_thread_ready, 0);
}

// the context starts suspended; resume it from a user gesture (studio.c's click gate).
static void sound_worklet_resume(void) {
    if (sound_aw_ctx) emscripten_resume_audio_context_sync(sound_aw_ctx);
}
#endif

// ───────── lifecycle (called by studio.c) ─────────

static void sound_init(void) {
    memset(voices,     0, sizeof(voices));
    memset(sfx_bank,   0, sizeof(sfx_bank));
    for (int i = 0; i < SOUND_VOICES;     i++) { voices[i].noise_state = 12345 + i; voices[i].owner_slot = -1; }
    for (int i = 0; i < SOUND_VOICES;     i++) held_voice[i] = -1;

    // Instrument bank defaults: slots 0..4 are the raw waves with a near-instant
    // declick-style envelope (so existing carts sound the same); 5..15 start as a
    // plain square until a cart redefines them with instrument().
    for (int i = 0; i < SOUND_INSTR_SLOTS; i++) {
        instr_bank[i].wave    = (i < 5) ? i : INSTR_SQUARE;
        instr_bank[i].a_samp  = SOUND_SAMPLE_RATE / 200;   // ~5ms attack
        instr_bank[i].d_samp  = 0;
        instr_bank[i].r_samp  = SOUND_SAMPLE_RATE / 50;    // ~20ms release
        instr_bank[i].sustain = 1.0f;
        instr_bank[i].duty    = 0.5f;
        for (int L = 0; L < SOUND_LFOS; L++) {
            instr_bank[i].lfo_dest[L]  = LFO_PITCH;
            instr_bank[i].lfo_rate[L]  = 0.0f;
            instr_bank[i].lfo_depth[L] = 0.0f;   // off until instrument_lfo() is called
        }
        for (int e = 0; e < SOUND_ENVS; e++) {
            instr_bank[i].env_dest[e]   = ENV_CUTOFF;
            instr_bank[i].env_a_samp[e] = 0;
            instr_bank[i].env_d_samp[e] = 0;
            instr_bank[i].env_amount[e] = 0.0f;  // off until instrument_env() is called
        }
        instr_bank[i].flw_dest   = LFO_CUTOFF;
        instr_bank[i].flw_atk    = 1.0f;
        instr_bank[i].flw_rel    = 1.0f;
        instr_bank[i].flw_amount = 0.0f;   // off until instrument_follow() is called
        instr_bank[i].flt_mode   = FILTER_OFF;
        instr_bank[i].flt_cutoff = 1000.0f;
        instr_bank[i].flt_q      = 1.0f;
        instr_bank[i].harmonics  = 0.5f;   // engine macros: center detent, Plaits-style
        instr_bank[i].timbre     = 0.5f;
        instr_bank[i].morph      = 0.5f;
        instr_bank[i].tune_mul   = 1.0f;   // no detune (instrument_tune 0)
        instr_bank[i].drive      = 0.0f;   // clean until instrument_drive() — old carts unchanged
        instr_bank[i].drive_mode = 0;      // DRIVE_SOFT (tanh) until instrument_drive_mode()
        instr_bank[i].echo       = 0.0f;   // dry until instrument_echo() — old carts unchanged
        instr_bank[i].reverb     = 0.0f;   // dry until instrument_reverb() — old carts unchanged
        instr_bank[i].rvb_tank   = -1;     // -1 = the master send; instrument_reverb_bus() points it at a tank
        instr_bank[i].sc_key     = 0;      // sidechain trigger routing — off until sidechain_key()
        instr_bank[i].sc_send    = 0.0f;
        instr_bank[i].fx_bus     = 0;      // master until instrument_chorus/flanger() assigns a private bus
        instr_bank[i].eng_p[0]   = 0.0f;   // guitar/piano fundamental weight (instrument_mode) — off until set
        instr_bank[i].eng_p[1]   = 0.0f;   // guitar/piano attack click (instrument_mode) — off until set
    }

    // echo bus: clean slate (matters for libtcc hot-reload + --det reproducibility)
    memset(echo_buf, 0, sizeof(echo_buf));
    echo_widx        = 0;
    echo_time        = 0.375f * SOUND_SAMPLE_RATE;   // dotted-8th at 120bpm
    echo_time_target = echo_time;
    echo_fb          = 0.35f;
    echo_tone_coef   = sound_echo_coef(0.5f);
    echo_lp          = 0.0f;
    echo_used        = false;
    memset(echo_ins_buf, 0, sizeof(echo_ins_buf));
    echo_ins_widx        = 0;
    echo_ins_time        = 0.375f * SOUND_SAMPLE_RATE;
    echo_ins_time_target = echo_ins_time;
    echo_ins_fb          = 0.35f;
    echo_ins_tone_coef   = sound_echo_coef(0.5f);
    echo_ins_lp          = 0.0f;
    echo_ins_mix         = 0.0f;
    echo_ins_used        = false;

    // reverb tank pool: clean slate (matters for libtcc hot-reload + --det reproducibility).
    // Every tank zeroed; each defaults to size-0.5 feedback / 0.5 damping so a reverb()-before-
    // config render matches the old single-tank default exactly (byte-identical).
    memset(rvb_tank, 0, sizeof(rvb_tank));
    for (int t = 0; t < SOUND_REVERB_TANKS; t++) {
        rvb_tank[t].fb   = REVERB_FEEDBACK_MIN + 0.5f * REVERB_FEEDBACK_RANGE;
        rvb_tank[t].damp = 0.5f;
        rvb_tank[t].mix  = 1.0f;   // full wet by default (wet-replace); reverb_insert lowers it for an in-line blend
        rvb_tank[t].used = false;
    }
    reverb_used = false;
    for (int i = 0; i < SOUND_REVERB_TANKS; i++) tank_bus[i] = 0;   // tank → aux bus, 0 = unallocated
    for (int b = 0; b < SOUND_FX_BUSES; b++)     bus_tank[b] = -1;  // bus → tank, -1 = not a reverb-bus
    for (int b = 0; b < SOUND_FX_BUSES; b++)     grain_tank_of[b] = -1;   // bus → grain tank, -1 = none
    for (int t = 0; t < SOUND_GRAIN_TANKS; t++) { grains_reset(&grain_pool[t]); grain_pool[t].used = false; grain_pool[t].freeze = false; grain_pool[t].noiseSeed = 55555u; }
    grain_next = 0; grain_overflow = 0;

    // per-bus chorus + flanger + tape inserts (bus 0 master + aux): clean slate (libtcc hot-reload + --det)
    memset(cho_buf, 0, sizeof(cho_buf));
    memset(flg_buf, 0, sizeof(flg_buf));
    memset(tape_bufL, 0, sizeof(tape_bufL));
    memset(tape_bufR, 0, sizeof(tape_bufR));
    fx_next_bus = 1;
    fx_bus_overflow = 0;
    rvb_bus_overflow = 0;
    for (int b = 0; b < SOUND_FX_BUSES; b++) {
        cho_widx[b] = 0; cho_phase[b] = 0.0f;
        cho_rate[b] = 1.5f; cho_depth[b] = 0.4f; cho_mix[b] = 0.5f; cho_used[b] = false;
        flg_widx[b] = 0; flg_phase[b] = 0.0f;
        flg_rate[b] = 0.3f; flg_depth[b] = 0.7f; flg_fb[b] = 0.7f; flg_mix[b] = 0.5f; flg_used[b] = false;
        tape_widx[b] = 0; tape_wph[b] = 0.0f; tape_fph[b] = 0.0f; tape_lpL[b] = 0.0f; tape_lpR[b] = 0.0f;
        tape_wow[b] = 0.3f; tape_flut[b] = 0.2f; tape_sat[b] = 0.4f; tape_used[b] = false;
        wah_env[b] = 0.0f; wah_ic1[b] = 0.0f; wah_ic2[b] = 0.0f;
        wah_sens[b] = 0.3f + 0.5f * 4.7f; wah_res[b] = 0.5f; wah_mix[b] = 0.7f; wah_used[b] = false;
        wah_lfo_rate[b] = 0.0f; wah_lfo_phase[b] = 0.0f;   // follower mode until fx_set_wah_lfo()
        crush_bits[b] = 8.0f; crush_rate[b] = 4.0f; crush_mix[b] = 1.0f;
        crush_holdL[b] = 0.0f; crush_holdR[b] = 0.0f; crush_cnt[b] = 0; crush_used[b] = false;
        eq_low_g[b] = 1.0f; eq_mid_g[b] = 1.0f; eq_high_g[b] = 1.0f;
        eq_loL[b] = 0.0f; eq_loR[b] = 0.0f; eq_hiL[b] = 0.0f; eq_hiR[b] = 0.0f; eq_used[b] = false;
        for (int i = 0; i < 4; i++) { fmt_freq[b][i] = 700.0f; fmt_k[b][i] = 0.2f; fmt_amp[b][i] = 0.0f; fmt_ic1[b][i] = 0.0f; fmt_ic2[b][i] = 0.0f; }
        fmt_mix[b] = 0.0f; fmt_used[b] = false;   // dormant; fx_set_formant() fills the bands from the vowel table
        filt_mode[b] = FILTER_LOW; filt_cut[b] = 8000.0f; filt_res[b] = 0.3f;
        filt_ic1L[b] = filt_ic2L[b] = filt_ic1R[b] = filt_ic2R[b] = 0.0f; filt_used[b] = false;
        for (int s = 0; s < N_PEDALS; s++) insert_order[b][s] = s;   // default insert order = the 8 pedals, canonical (identity)
        insert_order[b][N_PEDALS]     = FX_FORMANT;                  // + formant as the 9th pedal (dormant until formant() → byte-identical)
        insert_order[b][N_PEDALS + 1] = FX_FILTER;                   // + filter as the 10th pedal (dormant until filter() → byte-identical)
        insert_order[b][N_PEDALS + 2] = FX_PAN;                      // + auto-pan as the 11th pedal (dormant until autopan() → byte-identical)
        insert_order[b][N_PEDALS + 3] = FX_RINGMOD;                  // + ring mod as the 12th pedal (dormant until ringmod() → byte-identical)
        insert_order_n[b] = N_PEDALS + 4;                            // FX_REVERB is never in the default chain — reverb_bus() places it
    }

    // user waves default to a sine, so playing INSTR_USER* before wave_set isn't silence
    for (int w = 0; w < SOUND_USER_WAVES; w++)
        for (int i = 0; i < SOUND_WAVE_LEN; i++)
            user_wave[w][i] = sinf(i / (float)SOUND_WAVE_LEN * 6.2831853f);

    sound_load_demo_data();

    if (!sound_synth_mode) {       // --wav: no device stream; the main thread pumps
        // The request queue drains ONCE per callback, at the buffer boundary (see
        // sound_callback step 1) — so a scheduled note's timing ORIGIN snaps to that
        // boundary, and the worst-case swing it can suffer ≈ one buffer length. On web
        // there's no AudioWorklet (build ships no -sAUDIO_WORKLET), so the callback runs
        // on the MAIN thread (ScriptProcessorNode): a SMALLER buffer = tighter timing
        // but more underrun risk; a bigger buffer = fewer dropouts but audible swing
        // (4096≈93ms ate a whole 16th). 512≈11.6ms is the safe catalog-wide default:
        // tighter than the old 1024, zero crackle on-device, and enough headroom for
        // heavy carts on weak phones (256, the ScriptProcessor floor, tightens further
        // but risks underrun on dense carts). Residual drift at 256 was the main-thread
        // jitter only an AudioWorklet removes — the real fix. Native keeps 1024.
        // See design/audio-timing.md §3.
#if defined(PLATFORM_WEB) && defined(DE_AUDIO_WORKLET)
        sound_worklet_init();   // real audio thread (bypasses raylib's ScriptProcessor) — audio-threading.md Stage 2
#else
#ifdef PLATFORM_WEB
        SetAudioStreamBufferSizeDefault(512);
#else
        SetAudioStreamBufferSizeDefault(1024);
#endif
        sound_stream = LoadAudioStream(SOUND_SAMPLE_RATE, 32, 2);   // stereo (centered until pan API; stereo.md)
        SetAudioStreamCallback(sound_stream, sound_callback);
        PlayAudioStream(sound_stream);
#endif
    }
}

static void sound_shutdown(void) {
#if !defined(DE_AUDIO_WORKLET)
    if (!sound_synth_mode) UnloadAudioStream(sound_stream);   // worklet build has no raylib stream
#endif
}

#endif // SOUND_H
