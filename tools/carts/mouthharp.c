/* de:meta
{
  "title": "mouth harp",
  "status": "active",
  "created": "2026-06-08",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Standalone instrument cart; no direct cart parent. Novel: models the jaw-harp as a fixed-pitch saw through a high-Q resonant bandpass — sweeping note_cutoff is the mouth cavity picking harmonics, the inversion of otamatone's pitch-over-lowpass approach.",
  "description": "A jaw harp (Jew's harp / mondharp) — the instrument where the PITCH NEVER CHANGES and you play with your MOUTH instead. A buzzy fixed-pitch saw drone (every harmonic) runs through a TIGHT resonant BANDPASS (FILTER_BAND, resonance 14), and sweeping note_cutoff is your mouth cavity: the resonant peak hunts across the drone's overtones and picks out which one 'speaks' — the vocal boi-oi-oing / wah-yeoww talking quality, and THAT is the melody. The corner of the engine no other cart centres on: where otamatone moves pitch over a plain lowpass, here pitch is frozen and the high-Q formant sweep is the whole tune. Move the mouse up/down to play (a harmonic comb on the right glows on the overtone you're voicing). TAB switches two flavours: PLUCK (folk) — each twang decays, HOLD to auto-pluck locked to the tempo for the iconic boing-boing-a-boing, UP/DOWN sets the subdivision; DRONE (dan-moi) — one continuous buzz you shape forever, hold SPACE for a breath swell. SPACE or click = pluck, LEFT/RIGHT pick the harp's key, M autoplay."
}
de:meta */
#include "studio.h"
#include <math.h>

// mouth harp (jaw harp / Jew's harp / mondharp) — the instrument where the
// PITCH NEVER CHANGES and you play with your MOUTH instead.
//
// A real jaw harp is a metal tongue that twangs at ONE fixed note. You hold it
// against your teeth and the melody comes entirely from your mouth cavity: a
// sharp resonance that sweeps across the drone's harmonics, picking out which
// overtone "speaks" — the vocal "boi-oi-oing / wah-yeoww" talking quality.
//
// In engine terms that is: a buzzy fixed-pitch source (INSTR_SAW = all harmonics)
// through a TIGHT resonant BANDPASS (FILTER_BAND, resonance maxed). Sweeping
// note_cutoff is the mouth — the resonant peak hunts harmonics, and THAT is the
// tune. No cart on the shelf centres on this: otamatone moves PITCH over a plain
// lowpass; here pitch is frozen and the formant sweep is the whole melody.
//
// Two flavours, TAB to switch (juice.c-style live toggle):
//   PLUCK (folk)   — each twang decays; HOLD to auto-pluck locked to the tempo
//                    (the iconic boing-boing-a-boing). UP/DOWN = subdivision.
//   DRONE (dan-moi)— one continuous buzz you shape forever; meditative.
//
// CONTROLS: move the mouse up/down = mouth (the melody) · SPACE or click = pluck
// (PLUCK mode) / breath swell (DRONE mode) · HOLD for rhythmic auto-pluck ·
// TAB switch mode · LEFT/RIGHT pick the harp's key · UP/DOWN reiter rate ·
// M autoplay.

#define SAW_SLOT  5
#define TICK_SLOT 6

#define PLAY_TOP  30          // mouse-y range that maps to the mouth sweep
#define PLAY_BOT  186
#define COMB_X    206         // harmonic-comb visualiser
#define COMB_W    104

// jaw harps are single-key; pick from four. Low fundamentals on purpose —
// the lower the root, the more harmonics land inside the sweep range.
static const int   HARP_MIDI[4] = { 43, 45, 48, 50 };   // G2 A2 C3 D3
static const char *HARP_NAME[4] = { "G", "A", "C", "D" };
static int   harp = 1;

static int   voice   = -1;    // the one held drone voice; pitch is fixed
static float bend    = 0;     // semitone dip on each pluck, springs back = the boing
static float env     = 0;     // pluck amplitude envelope 0..1 (decays)
static float level   = 0;     // smoothed value fed to note_vol
static float fpos = 0.45f; // 0..1 mouth position (→ bandpass centre), smoothed
static int   mode    = 0;     // 0 = pluck (folk), 1 = drone (dan-moi)
static int   subdiv  = 4;     // reiter subdivisions per beat
static int   last_slot = -99999;
static bool  autoplay = true;
static int   twang_vis = 0;   // counts down after a pluck — flicks the tongue
static int   ap_t = 0;        // autoplay phase

static float root_hz(void) { return 440.0f * powf(2.0f, (HARP_MIDI[harp] - 69) / 12.0f); }

// mouth 0..1 (closed/dark .. open/bright) → bandpass centre Hz, log-spaced
static int formant_hz(float m) {
    return (int)expf(lerp(logf(170.0f), logf(3600.0f), clamp(m, 0, 1)));
}

// a Hz value → its y on the comb (log axis, high = up)
static int hz_to_y(float hz) {
    float t = (logf(hz) - logf(150.0f)) / (logf(4000.0f) - logf(150.0f));
    return (int)lerp(PLAY_BOT, PLAY_TOP, clamp(t, 0, 1));
}

static void pluck(void) {
    env  = 1.0f;
    bend = -1.7f;                              // reed dips then springs back
    twang_vis = 9;
    hit(HARP_MIDI[harp] + 31, TICK_SLOT, 3, 35);  // dry tongue snap on the attack
}

static void set_harp(int h) {
    harp = ((h % 4) + 4) % 4;
    if (voice >= 0) note_pitch(voice, HARP_MIDI[harp]);
}

void init(void) {
    // buzzy saw (every harmonic, so the vowel filter has plenty to pick from)
    // through a tight resonant bandpass — that peak IS the mouth cavity.
    instrument(SAW_SLOT, INSTR_SAW, 2, 0, 7, 500);
    instrument_filter(SAW_SLOT, FILTER_BAND, 700, 14);   // res 14 → vocal formant
    instrument(TICK_SLOT, INSTR_NOISE, 0, 25, 0, 28);    // percussive tongue tick
    voice = note_on(HARP_MIDI[harp], SAW_SLOT, 0);       // one fixed-pitch voice
    note_res(voice, 14);
    bpm(96);
}

void update(void) {
    if (keyp(KEY_TAB))   { mode = !mode; env = 0; }
    if (keyp('M'))       autoplay = !autoplay;
    if (keyp(KEY_LEFT))  set_harp(harp - 1);
    if (keyp(KEY_RIGHT)) set_harp(harp + 1);
    if (keyp(KEY_UP))    subdiv = subdiv >= 8 ? 8 : subdiv + 1;
    if (keyp(KEY_DOWN))  subdiv = subdiv <= 1 ? 1 : subdiv - 1;

    bool act = key(KEY_SPACE) || mouse_down(MOUSE_LEFT);   // held = sustain/auto
    bool actp = keyp(KEY_SPACE) || mouse_pressed(MOUSE_LEFT);

    // ---- the mouth: mouse-y sweeps the formant (the melody), in BOTH modes ----
    if (!autoplay) {
        float t = clamp((float)(mouse_y() - PLAY_TOP) / (PLAY_BOT - PLAY_TOP), 0, 1);
        fpos = lerp(fpos, 1.0f - t, 0.30f);          // up = bright/open
    } else {
        // gentle self-playing vowel wander + plucks, so it's alive at rest/bake
        ap_t++;
        fpos = lerp(fpos, 0.5f + 0.45f * sinf(ap_t * 0.045f), 0.08f);
        if (mode == 0) {
            int slot = (int)((beat() + beat_pos()) * subdiv);
            if (slot != last_slot && (slot & 1 || chance(60))) pluck();
            last_slot = slot;
        }
    }

    // ---- amplitude: this is the ONLY thing the mode changes ----
    if (mode == 0) {                       // PLUCK: re-triggered decay envelope
        if (!autoplay) {
            if (actp) pluck();
            if (act) {                     // held → auto-pluck locked to tempo
                int slot = (int)((beat() + beat_pos()) * subdiv);
                if (slot != last_slot) pluck();
                last_slot = slot;
            }
        }
        env  = lerp(env, 0.0f, 0.085f);    // the twang rings down
        bend = lerp(bend, 0.0f, 0.20f);    // pitch springs back up
        level = env;
    } else {                               // DRONE: continuous, breath = swell
        float target = 0.55f + (act ? 0.45f : 0.0f);
        level = lerp(level, autoplay ? 0.7f : target, 0.12f);
        bend  = lerp(bend, 0.0f, 0.2f);
    }

    // ---- drive the one voice ----
    note_pitch(voice, HARP_MIDI[harp] + bend);
    note_cutoff(voice, formant_hz(fpos));
    note_vol(voice, clamp(level, 0, 1) * 7.0f);

    if (twang_vis > 0) twang_vis--;

#ifdef DE_TRACE
    watch("formant", "%.2f", fpos);
    watch("env",     "%.2f", env);
    watch("mode",    "%d",   mode);
#endif
}

// the lyre-shaped frame + the vibrating trigger tongue
static void draw_harp(int cx, int cy) {
    int flick = twang_vis > 0 ? (int)(sinf(twang_vis * 1.4f) * (twang_vis - 1)) : 0;
    // the round bow / frame
    ovalfill(cx, cy, 30, 38, CLR_DARK_GREY);
    ovalfill(cx, cy, 22, 30, CLR_DARKER_BLUE);
    rect(cx - 30, cy - 38, 60, 76, CLR_LIGHT_GREY);
    // the two prongs reaching right (where it meets the teeth)
    rectfill(cx + 22, cy - 16, 34, 5, CLR_MEDIUM_GREY);
    rectfill(cx + 22, cy + 11, 34, 5, CLR_MEDIUM_GREY);
    // the springy trigger tongue down the middle — flicks on each pluck
    int tx = cx + 56 + flick;
    line(cx - 4, cy, cx + 40, cy, CLR_LIGHT_GREY);
    line(cx + 40, cy, tx, cy + 30 + flick, CLR_WHITE);
    circfill(tx, cy + 30 + flick, 3, CLR_YELLOW);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("MOUTH HARP", 8, 6, CLR_PEACH);
    font(FONT_SMALL);
    print(mode ? "drone (dan-moi)" : "pluck (folk)", 92, 8,
          mode ? CLR_TRUE_BLUE : CLR_ORANGE);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 8, 8,
                autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    // ---- the mouth strip (left): a vertical bar, the cursor = current vowel ----
    int mx = 22, my = (int)lerp(PLAY_BOT, PLAY_TOP, fpos);
    rectfill(mx - 8, PLAY_TOP, 16, PLAY_BOT - PLAY_TOP, CLR_DARKER_GREY);
    rect(mx - 8, PLAY_TOP, 16, PLAY_BOT - PLAY_TOP, CLR_DARK_GREY);
    // vowel labels up the mouth (dark/closed at the bottom, open/bright at top)
    font(FONT_TINY);
    print("aah", mx + 12, PLAY_TOP + 2,  CLR_LIGHT_PEACH);
    print("ooh", mx + 12, (PLAY_TOP + PLAY_BOT) / 2, CLR_PEACH);
    print("mm",  mx + 12, PLAY_BOT - 8,  CLR_DARK_BROWN);
    font(FONT_NORMAL);
    circfill(mx, my, 6, CLR_YELLOW);
    circfill(mx, my, 3, CLR_WHITE);

    // ---- the harp, breathing with the output level ----
    draw_harp(118, 108);

    // ---- the harmonic comb (right): the drone's overtones, the one the formant
    //      is voicing glows. Move the mouth and watch the tune climb the comb. ----
    float rh = root_hz();
    int   fy = hz_to_y((float)formant_hz(fpos));
    rect(COMB_X - 4, PLAY_TOP - 2, COMB_W + 8, PLAY_BOT - PLAY_TOP + 4, CLR_DARK_BROWN);
    for (int h = 1; h <= 24; h++) {
        float hz = rh * h;
        if (hz > 4000) break;
        int y = hz_to_y(hz);
        int near = abs(y - fy) < 5;                 // is the formant on this harmonic?
        int len = near ? COMB_W : 14 + (COMB_W - 14) * (h == 1);
        int col = near ? CLR_LIGHT_YELLOW : (h == 1 ? CLR_ORANGE : CLR_DARK_GREY);
        line(COMB_X, y, COMB_X + len, y, col);
        if (near) {
            circfill(COMB_X + COMB_W, y, 3, CLR_WHITE);
            font(FONT_TINY);
            print(str("h%d", h), COMB_X + COMB_W - 18, y - 8, CLR_LIGHT_YELLOW);
            font(FONT_NORMAL);
        }
    }
    // the sweeping formant cursor
    line(COMB_X - 4, fy, COMB_X + COMB_W + 4, fy, CLR_RED);

    // ---- HUD ----
    font(FONT_SMALL);
    print(str("key: %s   (LEFT/RIGHT)", HARP_NAME[harp]), 8, 168, CLR_MEDIUM_GREY);
    if (mode == 0)
        print(str("reiter: 1/%d   (UP/DOWN, HOLD to roll)", subdiv), 8, 178, CLR_MEDIUM_GREY);
    else
        print("hold SPACE = breath swell", 8, 178, CLR_MEDIUM_GREY);
    print_right("move mouse = mouth   TAB = mode", SCREEN_W - 8, 190, CLR_DARK_GREY);
    print("SPACE/click = pluck", 8, 190, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
