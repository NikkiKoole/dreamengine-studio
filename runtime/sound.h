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
    int   lfo_dest[3];              // LFO_PITCH / LFO_DUTY / LFO_VOLUME / LFO_CUTOFF, per LFO
    float lfo_rate[3];              // Hz
    float lfo_depth[3];             // 0 = off; units depend on dest
    int   flt_mode;                 // FILTER_OFF / LOW / HIGH / BAND / NOTCH
    float flt_cutoff;               // Hz
    float flt_q;                    // damping coefficient (1/Q); small = resonant
    int   env_dest[2];              // ENV_CUTOFF / ENV_PITCH / ENV_DUTY, per mod-envelope
    int   env_a_samp[2];            // attack, in samples
    int   env_d_samp[2];            // decay, in samples
    float env_amount[2];            // 0 = off; bipolar; units depend on dest (Hz / semitones / 0..1)
    int   flw_dest;                 // envelope FOLLOWER dest (LFO_CUTOFF/VOLUME/PITCH) — the 3rd mod source
    float flw_atk, flw_rel;         // per-sample smoothing coeffs (from attack/release ms)
    float flw_amount;               // 0 = off; Hz for cutoff, 0..1 for volume(duck), semitones for pitch
    float harmonics, timbre, morph; // engine macros 0..1 (INSTR_PLUCK+) — meaning is per-engine; default 0.5
    float drive;                    // post-filter saturation 0..1; 0 = clean bypass (default — old carts unchanged)
    float echo;                     // send to THE echo bus 0..1; 0 = dry (default — old carts unchanged)
    float tune_mul;                 // slot detune as a freq factor (2^(semis/12)); read LIVE by every
                                    // sounding voice each sample — fire-and-forget hits bend too. default 1
    uint32_t choke_mask;            // bitmask: bit N set = a new note on this slot kills active voices on slot N
} Instrument;

#define SOUND_LFOS 3
#define SOUND_ENVS 2   // routable mod-envelopes per instrument (the one-shot twin of the LFOs)

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
    int    env_dest[2];              // mod-envelopes (AD; timer = step_samples, retriggered at note-on)
    int    env_a_samp[2], env_d_samp[2];
    float  env_amount[2];
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
    float  drv_dc_x1, drv_dc_y1;   // DC blocker on the drive output (tanh of an asymmetric wave = DC = a thump)
    float  eko, eko_target;        // echo-bus send (current + slew target, same machinery — note_echo)
    int    instr_slot;             // instrument slot this voice was started from (for choke matching)
    float  last_out;               // this voice's previous mixed contribution — feeds the steal-declick tail
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
    int    ep_type;                  // 0 Rhodes / 1 Wurli / 2 Clav (from harmonics at note-on)
    bool   ep_on;                    // struck this note — guards an engine id without a note-on init
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

// tone 0..1 → loop-filter cutoff 300 Hz .. ~6.8 kHz (each repeat passes through it once)
static float sound_echo_coef(float t) {
    if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
    float fc = 300.0f * powf(2.0f, t * 4.5f);
    return 1.0f - expf(-6.2831853f * fc / (float)SOUND_SAMPLE_RATE);
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
} SoundReqKind;
typedef struct { SoundReqKind kind; int a, b, c; int delay_samples; int dur_samples; int e0, e1, e2; } SoundReq;
#define SOUND_REQ_QUEUE   512   // generous: live held-voice control pushes many setters/frame, and a patch cart's
                                // init() can define dozens of slots + several wave_set tables in one burst
#define SOUND_DELAYED_MAX 64    // pending delayed notes (strum/schedule/schedule_hit) — fast sfx steps queue several ahead

static SoundReq      req_queue[SOUND_REQ_QUEUE];
static volatile int  req_head = 0;   // written by main thread
static volatile int  req_tail = 0;   // written by audio thread

// audio-thread-owned holding pen for delayed requests (e.g. strum)
static SoundReq      delayed[SOUND_DELAYED_MAX];
static int           delayed_count = 0;

// tripwire: how many requests were silently dropped because a buffer was full. A nonzero
// count means sound calls were LOST (a wave_set flood, an init() define burst, …) — exactly
// the silent class of bug that once made every wav-knob position play the default square.
// sound_tick() screams about it via printh so it shows in the editor log / bake output.
static volatile int sound_dropped = 0;

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
    if (sound_dropped > dropped_reported) {
        printh("[sound] WARNING: request queue overflow — %d sound call(s) DROPPED (notes/defines lost). Spread big bursts across frames or report this.", sound_dropped);
        dropped_reported = sound_dropped;
    }
}

// dur_samples: 0 = use default 250ms (for note/schedule); >0 = custom note length (for hit).
static void sound_push_req(SoundReqKind kind, int a, int b, int c, int delay_samples, int dur_samples) {
    int h = req_head;
    int next = (h + 1) % SOUND_REQ_QUEUE;
    if (next == req_tail) { sound_dropped++; return; }   // full — drop request (and trip the wire)
    req_queue[h].kind          = kind;
    req_queue[h].a             = a;
    req_queue[h].b             = b;
    req_queue[h].c             = c;
    req_queue[h].delay_samples = delay_samples;
    req_queue[h].dur_samples   = dur_samples;
    req_queue[h].e0 = req_queue[h].e1 = req_queue[h].e2 = 0;
    req_head = next;
}

// Push a control request carrying the extra e0/e1/e2 payload (used by instrument()).
static void sound_push_ctrl(SoundReqKind kind, int a, int b, int c, int e0, int e1, int e2) {
    int h = req_head;
    int next = (h + 1) % SOUND_REQ_QUEUE;
    if (next == req_tail) { sound_dropped++; return; }   // full — drop (and trip the wire)
    req_queue[h] = (SoundReq){ .kind = kind, .a = a, .b = b, .c = c,
                               .delay_samples = 0, .dur_samples = 0,
                               .e0 = e0, .e1 = e1, .e2 = e2 };
    req_head = next;
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
        for (int i = 0; i < period; i++) v->ks_buf[i] = tmp[i] - tmp[(i + pos) % period];
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

// Rhodes envelope split (tuned by ear + the navkit render, 2026-06-08). The fundamental is a
// long warm BODY; the inharmonic tine/bell modes are a short DING over it. Without this split
// the bell modes rang on as a drone — un-Rhodesy, and it made the 3 Rhodes presets sound alike.
// RHO_BLVL boosts the (intentionally quiet) bell modes so the tine is audible — most of what
// reads as "Rhodes" is this fast bright ding over a sustaining body. (audio-notes §8.8.5.)
static const float RHO_BODY = 0.7f;    // Rhodes fundamental (body) decay, s
static const float RHO_BELL = 0.13f;   // Rhodes bell/tine decay, s — the ding
static const float RHO_BLVL = 12.0f;   // Rhodes bell-mode level

// EP note-on: build the 12 modal sines from the instrument detent (harmonics), brightness
// (timbre = pickup position), register, and strike level. The modes then ring down (struck,
// self-decaying — mallet family); the sample fn sums them, runs the pickup nonlinearity
// (morph = bark, read live) and a DC blocker. navkit crib: initEPianoSettings + the tables.
static void sound_epiano_start(Voice *v) {
    // 12 mode ratios per instrument (navkit synth_oscillators.h:3675 — the inharmonicity is
    // the bell attack): Rhodes tine+spring, Wurli reed (odd-ish), Clav string (near-harmonic)
    static const float RAT[3][12] = {
        { 1.0f, 4.2f, 9.5f, 16.3f, 24.8f, 35.0f, 47.0f, 61.0f, 77.0f, 95.0f, 115.0f, 137.0f },
        { 1.0f, 2.02f, 3.01f, 5.04f, 7.05f, 9.08f, 11.1f, 13.1f, 15.2f, 17.2f, 19.3f, 21.3f },
        { 1.0f, 2.003f, 3.012f, 4.028f, 5.15f, 6.35f, 7.6f, 8.9f, 10.2f, 11.6f, 13.0f, 14.5f },
    };
    // amp profiles, centered (mellow) -> offset (bright); timbre crossfades them (:3747)
    static const float AC[3][12] = {
        { 1.0f, .04f, .03f, .06f, .03f, .02f, .015f, .012f, .010f, .008f, .006f, .004f },
        { 1.0f, .08f, .45f, .12f, .10f, .04f, 0,0,0,0,0,0 },
        { 1.0f, .30f, .20f, .35f, .15f, .06f, 0,0,0,0,0,0 },
    };
    static const float AO[3][12] = {
        { .60f, .35f, .08f, .20f, .08f, .05f, .04f, .03f, .025f, .02f, .015f, .01f },
        { .60f, .15f, .60f, .20f, .20f, .08f, 0,0,0,0,0,0 },
        { .60f, .55f, .50f, .20f, .30f, .10f, 0,0,0,0,0,0 },
    };
    static const float BASE_DEC[3] = { 3.5f, 1.8f, 0.65f };   // base sustain s (Wurli/Clav; Rhodes uses the tuning split)
    int ty = (int)(v->harm * 2.999f); if (ty < 0) ty = 0; else if (ty > 2) ty = 2;
    v->ep_type = ty;
    float pp  = v->timb;                                     // pickup position / brightness
    float vel = v->vol;                                      // strike level 0..1
    float fn  = (v->freq - 80.0f) / 1200.0f; if (fn < 0.0f) fn = 0.0f; else if (fn > 1.0f) fn = 1.0f;
    v->ep_freqnorm = fn;
    const float dt = 1.0f / (float)SOUND_SAMPLE_RATE;
    float keep = 1.0f - fn;
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
            if (ty == 0) a *= RHO_BLVL;                      // Rhodes bell-mode level
        }
        v->ep_amp[i] = a;
        float dec;
        if (ty == 0) {                                       // TEMP Rhodes ring: body (mode 0) vs bell (>=1, the ding)
            dec = (i == 0) ? RHO_BODY : RHO_BELL;
            if (i >= 1) dec *= 1.0f / (1.0f + (RAT[0][i] - 1.0f) * 0.04f);  // higher tines a touch shorter
            dec *= (1.0f - fn * 0.4f);                       // register
        } else {
            float dfac = 1.0f / (1.0f + (RAT[ty][i] - 1.0f) * 0.25f);   // upper modes ring shorter
            dec = BASE_DEC[ty] * (1.0f - fn * 0.4f) * dfac;
        }
        if (dec < 0.02f) dec = 0.02f;
        v->ep_dec[i] = dt / dec;                             // per-sample decay fraction
    }
    v->ep_dc_prev = v->ep_dc_state = 0.0f;
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
    if (v->ep_type == 1) {                            // Wurli: odd harmonics, symmetric clip
        float k3 = (0.40f + bark * 1.5f) * regDist;
        float k5 = (0.10f + bark * 0.4f) * regDist;
        out = sum + k3 * sum * s2 + k5 * sum * s2 * s2;
        out = tanhf(out);
    } else if (v->ep_type == 2) {                     // Clav: mixed even+odd, harder clip
        float k2 = 0.30f + bark * 0.5f;
        float k3 = 0.40f + bark * 0.6f;
        out = sum + k2 * s2 + k3 * sum * s2;
        out = tanhf(out * 1.2f);
    } else {                                          // Rhodes: even harmonics, asymmetric clip
        float k  = (0.25f + bark * 1.2f) * regDist;
        float k2 = (0.10f + bark * 0.4f) * regDist;
        out = sum + k * s2 + k2 * sum * s2;
        float drive = 1.0f + bark * 0.5f * regDist;
        out = (out >= 0.0f) ? tanhf(out * drive) : tanhf(out * drive * 0.85f) * 0.9f;
    }
    float dcin = out;                                 // DC blocker (pickup AC coupling, ~7Hz)
    out = dcin - v->ep_dc_prev + 0.995f * v->ep_dc_state;
    v->ep_dc_prev  = dcin;
    v->ep_dc_state = out;
    return out * 0.8f;
}

// One engine sample — the dispatch (engine ids >= INSTR_ENGINE_BASE). The default body is
// PLUCK. pitch_mul carries the per-sample LFO/env pitch modulation; v->freq carries
// note_pitch/note_glide (slewed) — so the SAME pitch machinery every oscillator answers
// bends the string: the tap distance just tracks it. Linear-interpolated read; the
// interpolation's slight extra damping at fractional positions is natural string behavior.
static inline float sound_engine_sample(Voice *v, float pitch_mul) {
    if (v->wave == INSTR_MALLET) return sound_mallet_sample(v, pitch_mul);
    if (v->wave == INSTR_FM)     return sound_fm_sample(v, pitch_mul);
    if (v->wave == INSTR_ORGAN)  return sound_organ_sample(v, pitch_mul);
    if (v->wave == INSTR_EPIANO) return sound_epiano_sample(v, pitch_mul);
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
static float steal_tail = 0.0f;

static int sound_find_voice(void) {
    int vi = 0;
    // prefer fully free; else a non-held voice; else voice 0.
    // (held notes are stolen only after plain event voices — they're meant to last.)
    for (int i = 0; i < SOUND_VOICES; i++) if (!voices[i].active)  { vi = i; goto done; }
    for (int i = 0; i < SOUND_VOICES; i++) if (!voices[i].held)    { vi = i; goto done; }
done:
    if (voices[vi].active) steal_tail += voices[vi].last_out;   // pay the cut into the declick tail
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
    v->drv  = v->drv_target  = ins->drive;
    v->drv_dc_x1 = v->drv_dc_y1 = 0.0f;
    v->eko  = v->eko_target  = ins->echo;
    v->last_out = 0.0f;
    v->ks_len = 0;
    v->md_on  = false;
    v->org_on = false;
    v->ep_on  = false;
    v->fm_mph = v->fm_fb = v->fm_tph = 0.0f;   // FM needs no excitation, just deterministic phases
    if      (v->wave == INSTR_PLUCK)  sound_pluck_start(v);    // excite the string
    else if (v->wave == INSTR_MALLET) sound_mallet_start(v);   // strike the bar
    else if (v->wave == INSTR_ORGAN)  sound_organ_start(v);    // arm the click + perc, clear the scanner
    else if (v->wave == INSTR_EPIANO) sound_epiano_start(v);   // build the 12 modal sines
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

// Fire a request now (called on the audio thread).
static void sound_fire_req(SoundReq r) {
    if (r.kind == SR_SFX) {
        int n = r.a;
        if (n == -1) {
            for (int i = 0; i < SOUND_VOICES; i++)
                if (!voices[i].held && voices[i].active) {
                    steal_tail += voices[i].last_out;           // declick the hard stop
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
                    steal_tail += voices[i].last_out;
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
                        steal_tail += voices[i].last_out;
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
    }
}

// ───────── audio callback (runs on audio thread) ─────────

static void sound_callback(void *buffer_data, unsigned int frames) {
    float *out = (float*)buffer_data;

    // 1) drain queued requests: fire immediate, hold delayed
    while (req_tail != req_head) {
        SoundReq r = req_queue[req_tail];
        req_tail = (req_tail + 1) % SOUND_REQ_QUEUE;
        if (r.delay_samples <= 0) {
            sound_fire_req(r);
        } else if (delayed_count < SOUND_DELAYED_MAX) {
            delayed[delayed_count++] = r;
        } else {
            sound_dropped++;   // delayed pen full — a scheduled note was lost (tripwire)
        }
    }

    // 2) mix — delayed requests fire SAMPLE-ACCURATELY inside the loop, not at
    // block boundaries. With a 1024-sample buffer (~23ms) block-edge firing
    // quantized away exactly the micro-timing that makes grooves feel played:
    // strum staggers (8ms), swing pushes, laid-back snares (10-35ms). Walking
    // the pen per sample keeps every schedule_hit offset intact; the pen is
    // tiny (<64) so the cost is noise.
    for (unsigned int i = 0; i < frames; i++) {
        float mix = 0.0f;
        float echo_in = 0.0f;   // this sample's summed sends into the echo bus

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
            float s = (v->wave >= INSTR_ENGINE_BASE) ? sound_engine_sample(v, pitch_mul)
                                                     : sound_osc(v->wave, v->phase, duty, &v->noise_state);
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
            // drive: post-filter tanh saturation — osc → SVF → drive → VCA, so resonance
            // screams INTO the saturation and quiet envelope tails don't pump it. The
            // pre-gain g grows from 0 (tanh(s·g)/tanh(g) → s as g → 0, so drive 0 is a
            // true bypass and a slewed sweep through 0 stays continuous) to wall-of-fuzz;
            // normalizing by tanh(g) keeps full-scale at full-scale — character, not volume.
            if (v->sfx_idx < 0 && v->drv > 0.001f) {
                float g = v->drv * v->drv * 24.0f;
                s = tanhf(s * g) / tanhf(g);
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
            v->last_out = contrib;        // remembered so a steal can declick (see steal_tail)
            mix += contrib;
            if (v->sfx_idx < 0 && v->eko > 0.0005f) echo_in += contrib * v->eko;   // post-everything send (dry stays full)

            v->phase += v->freq * pitch_mul / (float)SOUND_SAMPLE_RATE;
            if (v->phase >= 1.0f) v->phase -= 1.0f;
            v->step_samples++;
        }

        // steal-declick: when an audibly-ringing voice is stolen, its output step-discontinuity
        // is paid into this tail, which fades over ~3ms — the pop becomes a soft tick. Long
        // full-amplitude pluck voices made the old hard cut newly audible.
        mix += steal_tail;
        steal_tail *= 0.992f;
        if (steal_tail > -1e-5f && steal_tail < 1e-5f) steal_tail = 0.0f;

        // THE echo bus (dormant until the first echo API call — old carts skip this entirely)
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
            mix += echo_lp;   // the filtered tap IS the audible echo (wet adds to dry)
        }

        // master soft-clip: linear below the ±0.8 knee (quiet mixes stay bit-identical),
        // tanh-shaped above, asymptote at ±1.0 — a hot 16-voice sum folds over smoothly
        // instead of slamming the old hard wall. Slope is continuous at the knee.
        if      (mix >  0.8f) mix =  0.8f + 0.2f * tanhf((mix - 0.8f) * 5.0f);
        else if (mix < -0.8f) mix = -0.8f - 0.2f * tanhf((-mix - 0.8f) * 5.0f);
        out[i] = mix;
        if (wavcap_state == 1) {                  // WAV capture tap (wav_request)
            wavcap_buf[wavcap_pos++] = mix;
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
        instr_bank[i].echo       = 0.0f;   // dry until instrument_echo() — old carts unchanged
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

    // user waves default to a sine, so playing INSTR_USER* before wave_set isn't silence
    for (int w = 0; w < SOUND_USER_WAVES; w++)
        for (int i = 0; i < SOUND_WAVE_LEN; i++)
            user_wave[w][i] = sinf(i / (float)SOUND_WAVE_LEN * 6.2831853f);

    sound_load_demo_data();

    if (!sound_synth_mode) {       // --wav: no device stream; the main thread pumps
        SetAudioStreamBufferSizeDefault(1024);
        sound_stream = LoadAudioStream(SOUND_SAMPLE_RATE, 32, 1);
        SetAudioStreamCallback(sound_stream, sound_callback);
        PlayAudioStream(sound_stream);
    }
}

static void sound_shutdown(void) {
    if (!sound_synth_mode) UnloadAudioStream(sound_stream);
}

#endif // SOUND_H
