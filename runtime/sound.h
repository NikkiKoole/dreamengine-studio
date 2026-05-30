// sound.h — tiny PICO-8-style synth for dreamengine
// Header-only. Include from studio.c only (defines non-static API symbols).

#ifndef SOUND_H
#define SOUND_H

#include "raylib.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define SOUND_SAMPLE_RATE  44100
#define SOUND_VOICES       8
#define SOUND_SFX_STEPS    32
#define SOUND_SFX_SLOTS    32
#define SOUND_MUSIC_SLOTS  16
#define SOUND_INSTR_SLOTS  16

// Waveform IDs (INSTR_*) come from studio.h.

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

// Music pattern: each channel plays one SFX simultaneously. -1 = silent.
typedef struct {
    int8_t  channels[4];
    uint8_t loop;
} Pattern;

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
} Instrument;

#define SOUND_LFOS 3

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
    bool   from_music;
    // ADSR + LFO snapshot (one-shot notes only; copied from the instrument at note-on)
    int    a_samp, d_samp, r_samp;
    float  sustain;
    float  duty;
    float  rel_start;          // envelope level at the moment the gate ends (release fades from here)
    int    lfo_dest[3];
    float  lfo_rate[3], lfo_depth[3], lfo_phase[3];
    int    flt_mode;
    float  flt_cutoff, flt_q;
    float  flt_low, flt_band;   // SVF running state
} Voice;

static Voice         voices[SOUND_VOICES];
static AudioStream   sound_stream;
static Sfx           sfx_bank[SOUND_SFX_SLOTS];
static Pattern       music_bank[SOUND_MUSIC_SLOTS];
static Instrument    instr_bank[SOUND_INSTR_SLOTS];
static int           music_current = -1;

// request ring buffer (main thread pushes → audio thread drains)
// kind: 0=sfx, 1=music, 2=note, 3=define instrument, 4=set duty. -1 in a/b/c slots = "stop" for sfx/music.
// delay_samples: 0 = fire immediately; >0 = audio thread holds it in `delayed[]` and fires when countdown expires.
// e0/e1/e2: extra payload (instrument attack/decay/release samples).
typedef struct { int kind, a, b, c; int delay_samples; int dur_samples; int e0, e1, e2; } SoundReq;
#define SOUND_REQ_QUEUE   32
#define SOUND_DELAYED_MAX 16

static SoundReq      req_queue[SOUND_REQ_QUEUE];
static volatile int  req_head = 0;   // written by main thread
static volatile int  req_tail = 0;   // written by audio thread

// audio-thread-owned holding pen for delayed requests (e.g. strum)
static SoundReq      delayed[SOUND_DELAYED_MAX];
static int           delayed_count = 0;

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
}

// dur_samples: 0 = use default 250ms (for note/schedule); >0 = custom note length (for hit).
static void sound_push_req(int kind, int a, int b, int c, int delay_samples, int dur_samples) {
    int h = req_head;
    int next = (h + 1) % SOUND_REQ_QUEUE;
    if (next == req_tail) return;   // full — drop request
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
static void sound_push_ctrl(int kind, int a, int b, int c, int e0, int e1, int e2) {
    int h = req_head;
    int next = (h + 1) % SOUND_REQ_QUEUE;
    if (next == req_tail) return;   // full — drop
    req_queue[h] = (SoundReq){ .kind = kind, .a = a, .b = b, .c = c,
                               .delay_samples = 0, .dur_samples = 0,
                               .e0 = e0, .e1 = e1, .e2 = e2 };
    req_head = next;
}

// ───────── helpers ─────────

static inline float sound_midi_to_freq(int midi) {
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
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
    }
    return 0.0f;
}

// ADSR amplitude during the gated (held) portion of a note. `s` = samples since note-on.
// Attack 0→1 over a, decay 1→sustain over d, then hold at sustain.
static inline float sound_adsr_gated(int s, int a, int d, float sustain) {
    if (a > 0 && s < a) return (float)s / (float)a;
    s -= a;
    if (d > 0 && s < d) return 1.0f - (1.0f - sustain) * ((float)s / (float)d);
    return sustain;
}

// Chamberlin state-variable filter — one sample. Updates the voice's running state and
// returns the lowpass/highpass/bandpass/notch tap. `q` is damping (1/Q); smaller = more resonant.
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

static int sound_find_voice(void) {
    // prefer fully free; else steal a non-music voice; else steal voice 0
    for (int i = 0; i < SOUND_VOICES; i++)
        if (!voices[i].active) return i;
    for (int i = 0; i < SOUND_VOICES; i++)
        if (!voices[i].from_music) return i;
    return 0;
}

// Configure a voice to play one SFX step. Does not touch active / sfx_idx / step / from_music.
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
    if (r.kind == 0) {              // sfx
        int n = r.a;
        if (n == -1) {
            for (int i = 0; i < SOUND_VOICES; i++)
                if (!voices[i].from_music) voices[i].active = false;
        } else if (n >= 0 && n < SOUND_SFX_SLOTS) {
            int vi = sound_find_voice();
            Voice *v = &voices[vi];
            v->active     = true;
            v->from_music = false;
            v->sfx_idx    = n;
            v->step       = 0;
            sound_set_step(v, sfx_bank[n].steps[0], sfx_bank[n].step_dur);
        }
    } else if (r.kind == 1) {       // music
        int n = r.a;
        for (int i = 0; i < SOUND_VOICES; i++)
            if (voices[i].from_music) voices[i].active = false;
        if (n == -1) {
            music_current = -1;
        } else if (n >= 0 && n < SOUND_MUSIC_SLOTS) {
            music_current = n;
            Pattern *m = &music_bank[n];
            for (int ch = 0; ch < 4; ch++) {
                int s = m->channels[ch];
                if (s < 0 || s >= SOUND_SFX_SLOTS) continue;
                Voice *v = &voices[ch];
                v->active     = true;
                v->from_music = true;
                v->sfx_idx    = s;
                v->step       = 0;
                sound_set_step(v, sfx_bank[s].steps[0], sfx_bank[s].step_dur);
            }
        }
    } else if (r.kind == 2) {       // note (one-shot)
        int midi = r.a, slot = r.b, vol = r.c;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) slot = 0;
        Instrument *ins = &instr_bank[slot];
        int vi = sound_find_voice();
        Voice *v = &voices[vi];
        v->active           = true;
        v->from_music       = false;
        v->sfx_idx          = -1;
        v->phase            = 0.0f;
        v->freq             = sound_midi_to_freq(midi);
        v->vol              = (vol < 0 ? 0 : vol > 7 ? 7 : vol) / 7.0f;
        v->wave             = ins->wave;
        v->duty             = ins->duty;
        v->a_samp           = ins->a_samp;
        v->d_samp           = ins->d_samp;
        v->r_samp           = ins->r_samp;
        v->sustain          = ins->sustain;
        for (int L = 0; L < SOUND_LFOS; L++) {
            v->lfo_dest[L]  = ins->lfo_dest[L];
            v->lfo_rate[L]  = ins->lfo_rate[L];
            v->lfo_depth[L] = ins->lfo_depth[L];
            v->lfo_phase[L] = 0.0f;
        }
        v->flt_mode         = ins->flt_mode;
        v->flt_cutoff       = ins->flt_cutoff;
        v->flt_q            = ins->flt_q;
        v->flt_low          = 0.0f;
        v->flt_band         = 0.0f;
        v->step_samples     = 0;
        v->step_len_samples = r.dur_samples > 0 ? r.dur_samples : SOUND_SAMPLE_RATE / 4;
        v->rel_start        = sound_adsr_gated(v->step_len_samples, v->a_samp, v->d_samp, v->sustain);
    } else if (r.kind == 3) {       // define instrument (wave + ADSR)
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        Instrument *ins = &instr_bank[slot];
        ins->wave    = r.b;
        ins->sustain = (r.c < 0 ? 0 : r.c > 7 ? 7 : r.c) / 7.0f;
        ins->a_samp  = r.e0;
        ins->d_samp  = r.e1;
        ins->r_samp  = r.e2;
        // duty is left untouched — set independently via instrument_duty()
    } else if (r.kind == 4) {       // set instrument pulse duty
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        float duty = r.b / 1000.0f;
        if (duty < 0.01f) duty = 0.01f;
        if (duty > 0.99f) duty = 0.99f;
        instr_bank[slot].duty = duty;
    } else if (r.kind == 5) {       // set one of an instrument's LFOs
        int slot = r.a;
        int L = r.e1;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        if (L < 0 || L >= SOUND_LFOS) return;
        instr_bank[slot].lfo_dest[L]  = r.b;
        instr_bank[slot].lfo_rate[L]  = r.c  / 1000.0f;
        instr_bank[slot].lfo_depth[L] = r.e0 / 1000.0f;
    } else if (r.kind == 6) {       // set instrument filter
        int slot = r.a;
        if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
        int res = r.e0 < 0 ? 0 : r.e0 > 15 ? 15 : r.e0;
        instr_bank[slot].flt_mode   = r.b;
        instr_bank[slot].flt_cutoff = (float)r.c;
        instr_bank[slot].flt_q      = 2.0f - res * 0.13f;   // res 0 → damped, 15 → resonant peak
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
        }
    }

    // 2) tick delayed requests; fire any whose countdown has run out
    for (int i = 0; i < delayed_count; ) {
        delayed[i].delay_samples -= (int)frames;
        if (delayed[i].delay_samples <= 0) {
            sound_fire_req(delayed[i]);
            delayed[i] = delayed[--delayed_count];   // swap-remove
        } else {
            i++;
        }
    }

    // 2) mix
    for (unsigned int i = 0; i < frames; i++) {
        float mix = 0.0f;

        for (int vi = 0; vi < SOUND_VOICES; vi++) {
            Voice *v = &voices[vi];
            if (!v->active) continue;

            // step advance? (SFX/music walk their step list; one-shots fall through to ADSR release)
            if (v->step_samples >= v->step_len_samples && v->sfx_idx >= 0) {
                Sfx *s = &sfx_bank[v->sfx_idx];
                v->step++;
                if (v->step >= s->length) {
                    if (s->loop || v->from_music) {
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
                if (v->r_samp <= 0 || rs >= v->r_samp) { v->active = false; continue; }
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
                if (duty < 0.05f) duty = 0.05f;
                if (duty > 0.95f) duty = 0.95f;
            }

            float s = sound_osc(v->wave, v->phase, duty, &v->noise_state);
            if (v->sfx_idx < 0 && v->flt_mode != FILTER_OFF) {
                if (cutoff < 20.0f) cutoff = 20.0f;
                if (cutoff > SOUND_SAMPLE_RATE * 0.45f) cutoff = SOUND_SAMPLE_RATE * 0.45f;
                s = sound_svf(v, s, cutoff);
            }
            mix += s * v->vol * env * trem * 0.2f;

            v->phase += v->freq * pitch_mul / (float)SOUND_SAMPLE_RATE;
            if (v->phase >= 1.0f) v->phase -= 1.0f;
            v->step_samples++;
        }

        if (mix >  1.0f) mix =  1.0f;
        if (mix < -1.0f) mix = -1.0f;
        out[i] = mix;
    }
}

// ───────── demo data (replaces eventually with cart-authored data) ─────────

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
    // sfx 4 — bass loop (music)
    sfx_bank[4] = (Sfx){
        .step_dur = 12, .length = 4, .loop = 1,
        .steps = { {36, INSTR_TRI, 5}, {36, INSTR_TRI, 5}, {43, INSTR_TRI, 5}, {41, INSTR_TRI, 5} },
    };
    // sfx 5 — hihat loop (music)
    sfx_bank[5] = (Sfx){
        .step_dur = 6, .length = 4, .loop = 1,
        .steps = { {60, INSTR_NOISE, 3}, {0,0,0}, {60, INSTR_NOISE, 3}, {0,0,0} },
    };

    // music 0 — bass + hihat
    music_bank[0] = (Pattern){ .channels = { 4, 5, -1, -1 }, .loop = 1 };
}

// ───────── public API ─────────

void sfx(int n) {
    if (n < -1 || n >= SOUND_SFX_SLOTS) return;
    sound_push_req(0, n, 0, 0, 0, 0);
}

void music(int n) {
    if (n < -1 || n >= SOUND_MUSIC_SLOTS) return;
    sound_push_req(1, n, 0, 0, 0, 0);
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
        sound_push_req(2, root + ivl[idx], instr, vol, delay, 0);
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
    sound_push_req(2, midi, instr, vol, 0, 0);
}

void hit(int midi, int instr, int vol, int dur_ms) {
    int dur = (dur_ms * SOUND_SAMPLE_RATE) / 1000;
    if (dur < 1) dur = 1;
    sound_push_req(2, midi, instr, vol, 0, dur);
}

void instrument(int slot, int wave, int attack_ms, int decay_ms, int sustain, int release_ms) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    int a = (attack_ms  * SOUND_SAMPLE_RATE) / 1000;
    int d = (decay_ms   * SOUND_SAMPLE_RATE) / 1000;
    int r = (release_ms * SOUND_SAMPLE_RATE) / 1000;
    if (a < 0) a = 0;
    if (d < 0) d = 0;
    if (r < 0) r = 0;
    sound_push_ctrl(3, slot, wave, sustain, a, d, r);
}

void instrument_duty(int slot, float duty) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    sound_push_ctrl(4, slot, (int)(duty * 1000.0f), 0, 0, 0, 0);
}

void instrument_lfo(int slot, int which, int dest, float rate_hz, float depth) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    if (which < 0 || which >= SOUND_LFOS) return;
    if (rate_hz < 0) rate_hz = 0;
    if (depth   < 0) depth   = 0;
    sound_push_ctrl(5, slot, dest, (int)(rate_hz * 1000.0f), (int)(depth * 1000.0f), which, 0);
}

void instrument_filter(int slot, int mode, int cutoff_hz, int resonance) {
    if (slot < 0 || slot >= SOUND_INSTR_SLOTS) return;
    if (cutoff_hz < 20) cutoff_hz = 20;
    sound_push_ctrl(6, slot, mode, cutoff_hz, resonance, 0, 0);
}

void schedule(int delay_ms, int midi, int instr, int vol) {
    int ds = (delay_ms * SOUND_SAMPLE_RATE) / 1000;
    if (ds < 0) ds = 0;
    sound_push_req(2, midi, instr, vol, ds, 0);
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
    memset(music_bank, 0, sizeof(music_bank));
    for (int i = 0; i < SOUND_VOICES;     i++) voices[i].noise_state = 12345 + i;
    for (int i = 0; i < SOUND_MUSIC_SLOTS; i++)
        for (int c = 0; c < 4; c++) music_bank[i].channels[c] = -1;

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
        instr_bank[i].flt_mode   = FILTER_OFF;
        instr_bank[i].flt_cutoff = 1000.0f;
        instr_bank[i].flt_q      = 1.0f;
    }

    sound_load_demo_data();

    SetAudioStreamBufferSizeDefault(1024);
    sound_stream = LoadAudioStream(SOUND_SAMPLE_RATE, 32, 1);
    SetAudioStreamCallback(sound_stream, sound_callback);
    PlayAudioStream(sound_stream);
}

static void sound_shutdown(void) {
    UnloadAudioStream(sound_stream);
}

#endif // SOUND_H
