/* de:meta
{
  "slug": "humtheremin",
  "title": "hum theremin",
  "status": "active",
  "created": "2026-07-17",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "audio-input"
  ],
  "lineage": "The first instrument you play with your VOICE — the payoff of the Tier-1 mic seam (docs/design/mic-and-sampling.md). Hum and a glowing synth voice follows your pitch (mic_pitch) and your loudness (mic_level), like Leon Theremin's 1920s instrument but conducted by a hum instead of a hand in the air.",
  "homage": "The theremin (Leon Theremin, 1928) — the ethereal, hands-free, continuously-pitched voice of Clara Rockmore and a thousand sci-fi soundtracks.",
  "description": {
    "summary": "Hum into the mic and a glowing theremin voice follows your pitch and volume. A continuous, hands-free synth you play with your voice — your hum's contour scrolls by as a ribbon of light.",
    "detail": "Reads the Tier-1 mic surface: mic_pitch() (YIN, octave-safe) drives a sine voice's pitch (a light singing glide, with vibrato + a long reverb tail) and mic_level() rides its volume, so louder humming = louder, and silence fades it out. TAB snaps to a forgiving pentatonic scale so any hum lands on notes; GLIDE mode is the raw continuous theremin swoop.",
    "controls": "CLICK or SPACE: enable/disable the mic. TAB: toggle GLIDE (continuous) vs SCALE (snap to pentatonic). Then just hum."
  }
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// HUM THEREMIN — the first instrument you play with your voice, on the Tier-1 mic seam.
// mic_pitch() -> a smoothed, vibrato'd, reverberant sine voice's PITCH; mic_level() -> its VOLUME.
// mic_pitch() is YIN-based (octave-safe), so a LIGHT glide is all it takes for a singing theremin
// swoop; a pentatonic SNAP (TAB) lets a stranger's hum land on notes and sound musical instantly.

#define MIDI_LO   45.0f          // lowest note shown/played (~A2)
#define MIDI_HI   84.0f          // highest (~C6) — the pitch axis spans this range
#define GATE      0.012f         // below this loudness we treat it as silence (breaths between hums)
#define SMOOTH    0.30f          // pitch glide toward the target (YIN is reliable, so this can be snappy)
#define MAXSTEP   6.0f           // max semitones/frame — a gentle de-glitch, rarely hit now YIN is octave-safe
#define VOLSCALE  26.0f          // mic_level (~0..0.27) -> note vol 0..7

static const char *NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const int   PENTA[5]  = {0, 2, 4, 7, 9};   // major pentatonic — every snap sounds good

static int   voice   = -1;       // the single held theremin note (alive while the mic is on)
static float cur     = 60.0f;    // smoothed pitch (MIDI, continuous)
static float vol     = 0.0f;     // smoothed volume 0..7
static float last_hz_midi = 60;  // last confident pitch (held through breaths)
static int   scale_mode = 0;     // 0 = GLIDE (continuous), 1 = SCALE (pentatonic snap)

// history ribbon: one (pitch, level) sample per frame, a ring the width of the screen
static float t_midi[SCREEN_W];
static float t_lvl [SCREEN_W];
static int   t_pos = 0;

static float snap_penta(float midi) {
    int best = (int)lroundf(midi); float bestd = 1e9f;
    for (int oct = -1; oct <= 8; oct++) {
        for (int i = 0; i < 5; i++) {
            int cand = oct * 12 + PENTA[i];
            float d = fabsf(midi - cand);
            if (d < bestd) { bestd = d; best = cand; }
        }
    }
    return (float)best;
}

static int pitch_y(float midi) {
    float f = (midi - MIDI_LO) / (MIDI_HI - MIDI_LO);
    if (f < 0) f = 0; if (f > 1) f = 1;
    return (int)((1.0f - f) * (SCREEN_H - 12)) + 6;   // high pitch = high on screen
}

static void note_name(float midi, char *buf, int cap) {
    int m = (int)lroundf(midi);
    snprintf(buf, cap, "%s%d", NOTES[((m % 12) + 12) % 12], m / 12 - 1);
}

void init(void) {
    reverb(0.78f, 0.35f);   // a big, bright hall — the theremin's ethereal tail
    for (int i = 0; i < SCREEN_W; i++) { t_midi[i] = 60; t_lvl[i] = 0; }
}

void update(void) {
    if (keyp(KEY_SPACE) || mouse_pressed(MOUSE_LEFT)) {
        if (mic_active()) { mic_stop(); if (voice >= 0) { note_off(voice); voice = -1; } }
        else                mic_start();
    }
    if (keyp(KEY_TAB)) scale_mode = !scale_mode;

    if (!mic_active()) return;

    // bring the voice up the first frame the mic is live
    if (voice < 0) {
        voice = note_on((int)cur, INSTR_SINE, 0);
        note_lfo(voice, 0, LFO_PITCH, 5.5f, 0.28f);   // gentle theremin vibrato
        note_reverb(voice, 0.55f);                    // send to the hall
    }

    static float pending = -999.0f;   // a big-jump candidate awaiting confirmation
    float lvl = mic_level();
    float hz  = mic_pitch();
    int   voiced = lvl > GATE;

    // PITCH — only trust a fresh estimate when it's loud + in the hum range; else hold the last one.
    // A continuous move is trusted instantly (slides); a BIG jump must repeat one window before we
    // follow it, so a lone subharmonic glitch (YIN's occasional octave-down blip) is held through
    // instead of dipping the note. Real octave leaps just take ~one extra window (~46ms) to register.
    if (voiced && hz > 70.0f && hz < 1100.0f) {
        float target = 69.0f + 12.0f * log2f(hz / 440.0f);
        if (target < MIDI_LO) target = MIDI_LO; if (target > MIDI_HI) target = MIDI_HI;
        if      (fabsf(target - last_hz_midi) <= 7.0f) { last_hz_midi = target; pending = -999.0f; }
        else if (fabsf(target - pending)      <= 1.5f) { last_hz_midi = target; pending = -999.0f; }
        else                                             pending = target;   // suspect — wait for a repeat
    }
    float step = (last_hz_midi - cur) * SMOOTH;        // heavy glide toward the target
    if (step >  MAXSTEP) step =  MAXSTEP;              // clamp per-frame jump (octave-error guard)
    if (step < -MAXSTEP) step = -MAXSTEP;
    cur += step;

    float play = scale_mode ? snap_penta(cur) : cur;
    note_pitch(voice, play);

    // VOLUME — mic loudness rides the note volume; silence fades it out. Faster attack than
    // release, so a brief loudness dip (a vibrato trough, a between-word breath) doesn't cut the note.
    float tvol = voiced ? lvl * VOLSCALE : 0.0f;
    if (tvol > 7.0f) tvol = 7.0f;
    vol += (tvol - vol) * (tvol > vol ? 0.35f : 0.10f);
    note_vol(voice, vol);

    // record the ribbon (store the PLAYED pitch so scale-snap shows as steps)
    t_midi[t_pos] = play; t_lvl[t_pos] = vol; t_pos = (t_pos + 1) % SCREEN_W;
}

static int lvl_color(float v) {           // 0..7 volume -> ethereal blue ramp
    if (v < 0.6f) return CLR_DARK_BLUE;
    if (v < 2.5f) return CLR_INDIGO;
    if (v < 5.0f) return CLR_BLUE;
    return CLR_WHITE;
}

void draw(void) {
    cls(CLR_BLACK);

    if (!mic_active()) {
        font(FONT_COMIC); print("hum theremin", SCREEN_W/2 - 66, SCREEN_H/2 - 40, CLR_INDIGO); font(FONT_NORMAL);
        // a faint idle ring, breathing
        int r = 10 + (int)(2.0f * sinf(now() * 2.0f));
        circ(SCREEN_W/2, SCREEN_H/2 + 6, r, CLR_DARK_BLUE);
        print("click / space to enable the mic", SCREEN_W/2 - 92, SCREEN_H/2 + 30, CLR_INDIGO);
        print("then hum", SCREEN_W/2 - 24, SCREEN_H/2 + 44, CLR_MEDIUM_GREY);
        return;
    }

    // faint octave guide lines (each C)
    for (float m = 48; m <= MIDI_HI; m += 12) {
        int y = pitch_y(m);
        for (int x = 0; x < SCREEN_W; x += 6) pset(x, y, CLR_DARK_BLUE);
    }

    // the scrolling pitch ribbon — oldest at left, newest at right
    int prevx = -1, prevy = 0;
    for (int x = 0; x < SCREEN_W; x++) {
        int idx = (t_pos + x) % SCREEN_W;
        float v = t_lvl[idx];
        if (v < 0.05f) { prevx = -1; continue; }       // silent gap — break the ribbon
        int y = pitch_y(t_midi[idx]);
        int col = lvl_color(v);
        int g = 1 + (int)(v * 0.4f);                   // glow thickness grows with loudness
        line(x, y - g, x, y + g, col);
        if (prevx >= 0) line(prevx, prevy, x, y, col);
        prevx = x; prevy = y;
    }

    // the current voice as a glowing orb at the right edge
    if (vol > 0.05f) {
        int oy = pitch_y(scale_mode ? snap_penta(cur) : cur);
        int ox = SCREEN_W - 14;
        int r  = 3 + (int)(vol * 1.3f);
        circfill(ox, oy, r, lvl_color(vol));
        circ(ox, oy, r + 1, CLR_WHITE);
        char nm[8]; note_name(scale_mode ? snap_penta(cur) : cur, nm, sizeof nm);
        font(FONT_COMIC); print(nm, ox - 30, oy - 10, CLR_WHITE); font(FONT_NORMAL);
    }

    // HUD
    print("HUM THEREMIN", 6, 6, CLR_INDIGO);
    print(scale_mode ? "SCALE" : "GLIDE", SCREEN_W - 42, 6, scale_mode ? CLR_BLUE : CLR_INDIGO);
    print("TAB: mode", SCREEN_W - 60, SCREEN_H - 12, CLR_MEDIUM_GREY);
    if (vol < 0.1f) print("hum to play", 6, SCREEN_H - 12, CLR_MEDIUM_GREY);
}
