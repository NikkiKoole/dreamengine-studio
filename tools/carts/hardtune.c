/* de:meta
{
  "slug": "hardtune",
  "title": "hardtune",
  "status": "active",
  "kind": ["instrument"],
  "genre": null,
  "teaches": ["audio-input", "vocoder"],
  "created": "2026-07-17",
  "lineage": "Robot auto-tune (the T-Pain flavour), vein-3 sibling of the audio-input frontier (docs/design/audio-input-frontier.md). Built on voxbox's chassis (vocoder + vocoder_mic + a saw carrier) but the carrier is a SINGLE voice whose pitch is locked to snap_scale(mic_pitch) — so instead of your vowels on a chord, you hear your melody CORRECTED to the nearest scale note. The RETUNE slider is Antares' famous retune-speed knob: instant = hard robot, slow = natural correction. NOT voxroll (a Melodyne-style EDITOR of synthesised INSTR_VOICE) and NOT transparent auto-tune (that keeps your natural timbre and needs formant-preserving PCM pitch-shift — the frontier doc's flavour B, unbuilt).",
  "homage": "Auto-Tune as an instrument — Cher's 'Believe' (1998) and T-Pain: the retune knob cranked to instant until pitch jumps become the sound.",
  "description": {
    "summary": "Sing and hear yourself auto-tuned. Your voice is snapped to the nearest note of a scale and resynthesized robotic; a RETUNE slider takes it from hard T-Pain robot to a smooth, natural correction.",
    "detail": "Robot auto-tune. The mic's spectral envelope (your vowels + words) is imposed by the 12-band vocoder onto a single saw CARRIER whose pitch is locked to snap_scale(mic_pitch) — so your melody comes out corrected to the chosen scale, in the vocoder's robotic timbre. The RETUNE slider is the retune-speed control: at max the carrier snaps instantly between scale notes (the hard T-Pain/Cher robot jump), lower down it glides toward each target (natural pitch-correction). S cycles the scale (major/minor pentatonic, major, natural minor, chromatic). A live pitch scope draws your RAW pitch against the scale grid with the CORRECTED pitch snapping onto the lines, so you see the tuning happen. LIVE by nature (the mic drives it in real time) — a hardtune take does not replay deterministically (ADR-0032).",
    "controls": "MIC button: enable/disable the mic. RETUNE slider: hard robot (right) ↔ natural glide (left). S or the SCALE button: cycle the scale. Then just sing."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// HARDTUNE — robot auto-tune. A saw carrier locked to snap_scale(mic_pitch), vocoded by the live
// mic, so your melody comes out corrected to a scale in the vocoder's robotic voice. RETUNE = the
// retune-speed knob (instant robot .. natural glide). LIVE (ADR-0032). Built on voxbox's chassis.

#define CAR       10               // carrier instrument slot
#define GATE      0.012f
#define MIDI_LO   45.0f            // scope range ~A2
#define MIDI_HI   81.0f            // ~A5
#define VOLSCALE  24.0f            // mic_level -> carrier vol 0..7
#define BAR_H     20

static const char *NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

typedef struct { const char *name; int n; int deg[12]; } Scale;
static const Scale SCALES[] = {
    { "penta+", 5, {0, 2, 4, 7, 9} },
    { "penta-", 5, {0, 3, 5, 7, 10} },
    { "major",  7, {0, 2, 4, 5, 7, 9, 11} },
    { "minor",  7, {0, 2, 3, 5, 7, 8, 10} },
    { "chroma", 12,{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11} },
};
#define NSCALES ((int)(sizeof(SCALES) / sizeof(SCALES[0])))
static int   scale_i = 0;

static int   carrier = -1;
static int   live    = 0;          // vocoder engaged?
static float retune  = 1.0f;       // 0 = slow natural glide, 1 = instant hard robot
static float cur_p   = 60.0f;      // carrier pitch (MIDI), slewed toward the target
static float tgt_p   = 60.0f;      // the corrected (snapped) target
static float vol      = 0.0f;
static int   have_tgt = 0;

// scope history (one sample/frame, ring the screen width)
static float sc_raw[SCREEN_W];     // raw incoming pitch (MIDI), -1 = unvoiced
static float sc_cor[SCREEN_W];     // corrected pitch
static int   sc_pos = 0;

static float snap_scale(float midi) {
    const Scale *sc = &SCALES[scale_i];
    int best = (int)lroundf(midi); float bestd = 1e9f;
    for (int oct = -1; oct <= 8; oct++)
        for (int i = 0; i < sc->n; i++) {
            int cand = oct * 12 + sc->deg[i];
            float d = fabsf(midi - cand);
            if (d < bestd) { bestd = d; best = cand; }
        }
    return (float)best;
}

static int pitch_y(float midi) {
    float f = (midi - MIDI_LO) / (MIDI_HI - MIDI_LO);
    if (f < 0) f = 0; if (f > 1) f = 1;
    return BAR_H + 2 + (int)((1.0f - f) * (SCREEN_H - BAR_H - 12));
}
static void note_name(int m, char *b, int cap) {
    snprintf(b, cap, "%s%d", NOTES[((m % 12) + 12) % 12], m / 12 - 1);
}

static void mic_toggle(void) {
    if (mic_active()) mic_stop();
    else              mic_start();
}

void init(void) {
    instrument(CAR, INSTR_SAW, 6, 120, 7, 200);            // rich broadband carrier — held
    carrier = note_on(60, CAR, 0);                         // silent until the mic opens
    for (int i = 0; i < SCREEN_W; i++) { sc_raw[i] = -1.0f; sc_cor[i] = -1.0f; }
}

void update(void) {
    if (keyp('S')) scale_i = (scale_i + 1) % NSCALES;

    int active = mic_active();
    if (active && !live) {                                 // mic opened → carrier up + vocoder on
        vocoder(0.97f); vocoder_mic(1.0f); live = 1;       // set-and-hold (only on the transition)
    } else if (!active && live) {                          // mic closed → silence + vocoder off
        vocoder(0.0f); vocoder_mic(0.0f); note_vol(carrier, 0.0f); live = 0; vol = 0.0f;
    }

    if (active) {
        float hz = mic_pitch(), lvl = mic_level();
        int voiced = lvl > GATE && hz > 60.0f && hz < 1100.0f;
        if (voiced) {
            float in = 69.0f + 12.0f * log2f(hz / 440.0f);
            if (in < MIDI_LO) in = MIDI_LO; if (in > MIDI_HI) in = MIDI_HI;
            tgt_p = snap_scale(in);
            if (!have_tgt) { cur_p = tgt_p; have_tgt = 1; }
            sc_raw[sc_pos] = in;
        } else {
            sc_raw[sc_pos] = -1.0f;                         // unvoiced — hold pitch, fade volume
        }
        float speed = 0.12f + retune * retune * 0.88f;     // retune-speed curve: instant .. slow glide
        cur_p += (tgt_p - cur_p) * speed;
        note_pitch(carrier, cur_p);

        float tvol = voiced ? lvl * VOLSCALE : 0.0f; if (tvol > 7.0f) tvol = 7.0f;
        vol += (tvol - vol) * (tvol > vol ? 0.4f : 0.12f);
        note_vol(carrier, vol);
        sc_cor[sc_pos] = (vol > 0.1f) ? cur_p : -1.0f;
        sc_pos = (sc_pos + 1) % SCREEN_W;
    }
}

static void draw_scope(void) {
    // scale grid — a faint horizontal line on every note IN the scale (the "tune grid")
    for (int m = (int)MIDI_LO; m <= (int)MIDI_HI; m++) {
        int pc = ((m % 12) + 12) % 12, in = 0;
        for (int i = 0; i < SCALES[scale_i].n; i++) if (SCALES[scale_i].deg[i] == pc) in = 1;
        if (!in) continue;
        int y = pitch_y((float)m), c = (pc == 0) ? CLR_DARK_BLUE : CLR_DARKER_BLUE;
        for (int x = 0; x < SCREEN_W; x += (pc == 0 ? 4 : 7)) pset(x, y, c);
    }
    // raw pitch (dim) + corrected pitch (bright), oldest → newest left→right
    int praw = -1, pry = 0, pcor = -1, pcy = 0;
    for (int x = 0; x < SCREEN_W; x++) {
        int idx = (sc_pos + x) % SCREEN_W;
        if (sc_raw[idx] >= 0) { int y = pitch_y(sc_raw[idx]);
            if (praw >= 0) line(praw, pry, x, y, CLR_MEDIUM_GREY); praw = x; pry = y; } else praw = -1;
        if (sc_cor[idx] >= 0) { int y = pitch_y(sc_cor[idx]);
            if (pcor >= 0) line(pcor, pcy, x, y, CLR_GREEN); pcor = x; pcy = y; } else pcor = -1;
    }
}

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();

    print("HARDTUNE", 6, 7, CLR_ORANGE);
    if (ui_button(74, 2, 50, 15, SCALES[scale_i].name)) scale_i = (scale_i + 1) % NSCALES;
    ui_slider(&retune, 130, 4, 78, "retune");
    if (ui_button(SCREEN_W - 66, 2, 62, 15, mic_active() ? "\x07 MIC ON" : "ENABLE")) mic_toggle();

    if (!mic_active()) {
        draw_scope();
        font(FONT_COMIC); print("sing - hear yourself in tune", SCREEN_W/2 - 112, SCREEN_H/2 - 4, CLR_INDIGO); font(FONT_NORMAL);
        print("tap ENABLE, then sing a melody", SCREEN_W/2 - 92, SCREEN_H/2 + 18, CLR_MEDIUM_GREY);
        ui_end(); return;
    }

    draw_scope();

    // the current corrected note + a hint at the retune character
    if (vol > 0.1f) {
        char nm[8]; note_name((int)lroundf(cur_p), nm, sizeof nm);
        int oy = pitch_y(cur_p);
        circfill(SCREEN_W - 16, oy, 3 + (int)vol, CLR_WHITE);
        font(FONT_COMIC); print(nm, SCREEN_W - 44, oy - 12, CLR_WHITE); font(FONT_NORMAL);
    }
    print(retune > 0.75f ? "hard robot" : retune > 0.35f ? "tuned" : "natural glide",
          6, SCREEN_H - 12, CLR_MEDIUM_GREY);
    ui_end();
}
