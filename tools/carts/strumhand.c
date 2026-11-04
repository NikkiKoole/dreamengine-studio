/* de:meta
{
  "slug": "strumhand",
  "title": "strumhand",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "waveguide-synth",
    "analog-voice-modeling",
    "swing-timing"
  ],
  "lineage": "A spike for Playtime's music (vector-sketch experiments/playtime): can a strummed guitar sound played by a hand instead of a keyboard? Six INSTR_GUITAR slots as six mono strings (the guitar cart's engine), every note booked on the audio clock with schedule_note_at's per-note macro offsets. Close to an Elektron p-lock, but the per-note values come from a model of the hand (stroke energy, a grip with a weak finger, pick angle drift) instead of being written per step.",
  "description": {
    "summary": "A guitarist's HAND, not a keyboard: the same four chords strummed by a model of a player, where every string of every stroke gets its own timing, force, brightness, mute and a few cents of finger pressure. TAB flips to the keyboard strum to hear the difference.",
    "detail": "Six strings are six mono slots, so a string rings until it is struck again or a finger damps it, like a real one. The hand has state that drifts slowly instead of new dice per note: stroke energy over the phrase, a timing feel that leans ahead or behind, a pick angle, and a GRIP per chord where each finger presses a little differently (a weak finger is slightly muted and less sharp for the whole chord). Downstrokes dig into the bass strings, upstrokes catch only the top few and brighter. On a chord change the fretting hand lifts early (the last upstroke rings open strings), sometimes a wound string squeaks, and strings the new shape does not use get damped. Keys 1-5 switch each layer on and off so you can hear which one matters.",
    "controls": "SPACE play/stop · TAB human / keyboard · 1 time 2 force 3 tone 4 pitch 5 hand extras · UP/DOWN humanity · LEFT/RIGHT tempo · R new take (seed)"
  }
}
de:meta */
// strumhand — a spike: does a strummed guitar sound more human when every string of every
// stroke is shaped by a model of the hand?
//
// The engine side is what dreamengine already has (no new API):
//   - six INSTR_GUITAR slots = six strings, each instrument_mono (a string rings once)
//   - schedule_note_at + NoteOpts: per-note gain, timbre (brightness) and morph (mute) offsets
//   - per-string slot state set just before booking: instrument_tune (cents of intonation and
//     finger pressure), MODE_STRING_CLICK (pick noise), ENV_PITCH (a hard pluck starts sharp)
// The cart side is the HAND: stroke, grip, drift. That's what this spike is testing.
//
// Two things we know are approximations (see the findings at the bottom):
//   - tune / click / pitch-env are SLOT state, set ~100ms before the pluck, so the tail of the
//     previous note on that string hears the change too (a few cents, the string is about to
//     be re-struck anyway)
//   - a booked note can only be shaped at its onset (no bend or vibrato later in the note)

#include "studio.h"
#include <math.h>
#include <string.h>

#define NSTR    6
#define SL0     10              // string slots 10..15
#define SL_SQK  16              // finger squeak (noise)
#define LOOKAHEAD 0.12          // book strokes this far ahead on the audio clock

static const int OPEN[NSTR] = { 40, 45, 50, 55, 59, 64 };   // E A D G B E

// chord shapes as frets, -1 = not played (the hand damps it)
#define NCH 4
static const char *CH_NAME[NCH] = { "C", "Am", "F", "G" };
static const int SHAPE[NCH][NSTR] = {
    { -1, 3, 2, 0, 1, 0 },     // C
    { -1, 0, 2, 2, 1, 0 },     // Am
    { -1,-1, 3, 2, 1, 1 },     // F (small)
    {  3, 2, 0, 0, 0, 3 },     // G
};

// the strum pattern: 8 eighths a bar, D = down, U = up, . = the hand passes without touching
static const char PATTERN[8] = { 'D','.','D','U','.','U','D','U' };

enum { L_TIME, L_DYN, L_TONE, L_PITCH, L_EXTRA, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "time", "force", "tone", "pitch", "hand" };

static bool  playing = true;
static bool  human = true;
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 92;
static int   seed = 1;

// clock
static double t_next;           // audio time of the next eighth (nominal)
static int    step;             // eighths since start

// the hand's slow state
static float intonation[NSTR];  // cents, per string, fixed per take (strings a bit out)
static float grip[NSTR];        // finger pressure per string for the current chord 0.6..1
static int   grip_chord = -1;

// per-string display
static double pluck_t[NSTR];
static float  pluck_amp[NSTR], pluck_bright[NSTR], pluck_mute[NSTR], pluck_cents[NSTR];
static int    pluck_fret[NSTR];
static double sqk_t = -1;

// last stroke readout
static char  last_dir = ' ';
static float last_ms[NSTR], last_gain[NSTR];
static bool  last_hit[NSTR];

static float r1(void) { return rnd_float() * 2.0f - 1.0f; }          // -1..1
static float drift(float x, float k) { return noise(x * k + seed * 17.3f) * 2.0f - 1.0f; }

static void setup_sound(void) {
    for (int s = 0; s < NSTR; s++) {
        int sl = SL0 + s;
        instrument(sl, INSTR_GUITAR, 1, 0, 7, 900);
        instrument_harmonics(sl, 0.55f);      // steel acoustic box
        instrument_timbre(sl, 0.60f);
        instrument_morph(sl, 0.16f);
        instrument_mode(sl, MODE_STRING_WEIGHT, 0.45f);
        instrument_mode(sl, MODE_STRING_CLICK, 0.35f);
        instrument_mono(sl, 1);
        instrument_pan(sl, (s - 2.5f) * 0.10f);
        instrument_reverb(sl, 0.18f);
    }
    instrument(SL_SQK, INSTR_NOISE, 25, 90, 0, 40);
    instrument_filter(SL_SQK, FILTER_BAND, 1800, 9);
    instrument_env(SL_SQK, 0, ENV_CUTOFF, 70, 60, 1600);   // the squeak rises as the finger slides
    instrument_reverb(SL_SQK, 0.18f);
    reverb(0.45f, 0.4f);
}

static void new_take(void) {
    rnd_seed(seed);
    for (int s = 0; s < NSTR; s++) intonation[s] = r1() * 4.0f;   // ±4 cents
    grip_chord = -1;
}

static void new_grip(int ch) {
    // each finger presses a bit differently for the whole time this chord is held:
    // the correlated part. One finger is often the weak one.
    int weak = rnd(NSTR);
    for (int s = 0; s < NSTR; s++) {
        grip[s] = 0.85f + rnd_float() * 0.15f;
        if (s == weak && SHAPE[ch][s] > 0) grip[s] = 0.6f + rnd_float() * 0.15f;
    }
    grip_chord = ch;
}

// book one string. t = audio time, gain 0..1, bright/mute = macro offsets, cents, click 0..1
static void pluck(int s, int midi, double t, float gain, float bright, float mute,
                  float cents, float click, float sharp) {
    int sl = SL0 + s;
    instrument_tune(sl, cents / 100.0f);
    instrument_mode(sl, MODE_STRING_CLICK, click);
    instrument_env(sl, 0, ENV_PITCH, 0, 45, sharp);
    if (gain < 0.04f) gain = 0.04f;                    // NoteOpts gain 0 means FULL
    NoteOpts o = { .gain = gain, .timbre = bright, .morph = mute };
    schedule_note_at(t, midi, sl, 7, 2600, &o);
    pluck_t[s] = t; pluck_amp[s] = gain; pluck_bright[s] = bright;
    pluck_mute[s] = mute; pluck_cents[s] = cents;
}

static void squeak(double t, float gain) {
    NoteOpts o = { .gain = gain };
    schedule_note_at(t, 60, SL_SQK, 7, 120, &o);
    sqk_t = t;
}

// ---- the keyboard strum: what an engine strum does today ----
static void stroke_keyboard(int ch, bool down, double t) {
    last_dir = down ? 'D' : 'U';
    for (int i = 0; i < NSTR; i++) {
        int s = down ? i : NSTR - 1 - i;
        last_hit[s] = false;
        int f = SHAPE[ch][s];
        if (f < 0) continue;
        double ts = t + i * 0.012;
        pluck(s, OPEN[s] + f, ts, 0.8f, 0, 0, 0, 0.35f, 0);
        pluck_fret[s] = f;
        last_hit[s] = true; last_ms[s] = i * 12.0f; last_gain[s] = 0.8f;
    }
}

// ---- the hand ----
static void stroke_human(int ch, int next_ch, int pos, bool down, double t) {
    float H = humanity;
    double now_s = t;
    bool T = layer[L_TIME], D = layer[L_DYN], C = layer[L_TONE], P = layer[L_PITCH], X = layer[L_EXTRA];
    if (grip_chord != ch) new_grip(ch);

    // stroke energy: a slow drift over the phrase + where in the bar it falls
    float bar_f = step / 8.0f;
    float energy = 0.82f + 0.12f * drift(bar_f, 0.35f) * H;
    float accent = (pos == 0) ? 1.0f : (pos == 4 || pos == 2 || pos == 6) ? 0.85f : 0.62f;
    if (!down) accent *= 0.8f;
    float strength = D ? energy * (1.0f + (accent - 0.8f) * H) + r1() * 0.06f * H : 0.8f;
    if (strength > 1.0f) strength = 1.0f;

    // timing: the feel leans ahead/behind slowly, plus a little jitter per stroke
    double off = 0;
    if (T) off = (drift(bar_f, 0.6f) * 0.010 + r1() * 0.004) * H;

    // the stroke sweeps: harder = faster. Up strokes are quicker and shallower.
    float sweep_ms = 12.0f;
    if (T) sweep_ms = (down ? 34.0f : 22.0f) - strength * 14.0f + r1() * 4.0f * H;
    if (sweep_ms < 6) sweep_ms = 6;

    // which strings the pick catches: up strokes only reach the top 2-4 strings
    int lo = 0;
    if (X && !down) lo = NSTR - (2 + (int)(strength * 2.4f + rnd_float() * H));
    if (lo < 0) lo = 0;

    // the last upstroke before a chord change: the fretting hand already lifts
    bool lift = X && next_ch != ch && pos == 7 && rnd_float() < 0.6f * H;

    float pick_angle = C ? drift(bar_f, 1.3f) * 0.07f * H : 0;
    last_dir = down ? 'D' : 'U';

    int order = 0, n_hit = 0;
    for (int s = 0; s < NSTR; s++) if (SHAPE[ch][s] >= 0 && s >= lo) n_hit++;
    for (int i = 0; i < NSTR; i++) {
        int s = down ? i : NSTR - 1 - i;
        last_hit[s] = false;
        int f = SHAPE[ch][s];
        if (lift) f = 0;                              // open strings ring through the change
        if (f < 0 || s < lo) continue;
        if (lift && s < 2) continue;

        // the pick accelerates through the strings, not an even comb
        float frac = n_hit > 1 ? order / (float)(n_hit - 1) : 0;
        float shaped = T ? powf(frac, 0.8f + 0.25f * r1() * H) : frac;
        double ts = now_s + off + shaped * sweep_ms / 1000.0 + (T ? r1() * 0.0015 * H : 0);
        order++;

        // force per string: the first strings in a stroke get the most (the pick digs in)
        float g = strength;
        if (D) {
            g *= 1.0f - 0.28f * frac * H;
            g *= 1.0f + r1() * 0.08f * H;
            if (lift) g *= 0.55f;
        }
        // tone: harder = brighter, upstroke = the thin side of the pick, wound strings darker
        float bright = 0, mute = 0;
        if (C) {
            bright = ((strength - 0.8f) * 0.35f + (down ? 0 : 0.06f) + pick_angle
                      + (s < 3 ? -0.04f : 0.03f) + r1() * 0.03f) * H;
            float press = (f > 0) ? grip[s] : 1.0f;            // open strings are always clean
            mute = ((1.0f - press) * 0.55f + (lift ? 0.18f : 0) + r1() * 0.02f) * H;
        }
        // pitch: strings a bit out, fretted notes pushed sharp by finger pressure, harder = sharper
        float cents = 0, sharp = 0;
        if (P) {
            cents = intonation[s] * H;
            if (f > 0) cents += (2.0f + 5.0f * grip[s] + f * 0.4f) * H;
            sharp = strength * 0.12f * H;
        }
        float click = 0.35f;
        if (C) click = (0.22f + strength * 0.35f + (down ? 0 : 0.08f)) ;

        pluck(s, OPEN[s] + f, ts, g, bright, mute, cents, click, sharp);
        pluck_fret[s] = f;
        last_hit[s] = true; last_ms[s] = (float)((ts - t) * 1000.0); last_gain[s] = g;
    }

    // chord change, on the next downbeat: damp strings the new shape doesn't use, maybe squeak
    if (X && pos == 7 && next_ch != ch) {
        double tc = t + 60.0 / tempo / 2.0 - 0.02;
        for (int s = 0; s < NSTR; s++)
            if (SHAPE[next_ch][s] < 0 && SHAPE[ch][s] >= 0)    // a finger touches it quiet
                pluck(s, OPEN[s], tc, 0.06f * H + 0.04f, -0.3f, 0.95f, 0, 0.1f, 0);
        if (rnd_float() < 0.45f * H) squeak(t + 0.03, 0.12f + 0.15f * rnd_float());
    }
}

static int chord_at(int st) { return (st / 8) % NCH; }

static void schedule_ahead(void) {
    double now_a = audio_time();
    double eighth = 60.0 / tempo / 2.0;
    while (t_next < now_a + LOOKAHEAD) {
        if (t_next < now_a) t_next = now_a + 0.01;
        int pos = step % 8;
        int ch = chord_at(step), nch = chord_at(step + 1);
        char p = PATTERN[pos];
        double swing = (human && layer[L_TIME] && (pos & 1)) ? eighth * 0.04 * humanity : 0;
        if (p != '.') {
            if (human) stroke_human(ch, nch, pos, p == 'D', t_next + swing);
            else       stroke_keyboard(ch, p == 'D', t_next);
        }
        t_next += eighth;
        step++;
    }
}

void init(void) {
    setup_sound();
    new_take();
    t_next = audio_time() + 0.2;
}

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; t_next = audio_time() + 0.1; }
    if (keyp(KEY_TAB)) human = !human;
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) layer[i] = !layer[i];
    if (keyp(KEY_UP))    humanity = fminf(1.5f, humanity + 0.25f);
    if (keyp(KEY_DOWN))  humanity = fmaxf(0.0f, humanity - 0.25f);
    if (keyp(KEY_RIGHT)) tempo = min(160, tempo + 4);
    if (keyp(KEY_LEFT))  tempo = max(60, tempo - 4);
    if (keyp('R')) { seed++; new_take(); }
    if (playing) schedule_ahead();
#ifdef DE_TRACE
    watch("step", "%d", step);
    watch("human", "%d", human);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    double at = audio_time();

    // fretboard: six strings, plucks drawn as a decaying wobble
    int x0 = 20, x1 = 300, y0 = 40, gap = 14;
    rectfill(x0 - 6, y0 - 10, x1 - x0 + 12, gap * (NSTR - 1) + 20, CLR_BROWN);
    for (int s = 0; s < NSTR; s++) {
        int y = y0 + gap * (NSTR - 1 - s);              // high E on top, like tab
        float age = (float)(at - pluck_t[s]);
        float a = 0;
        if (age >= 0) a = pluck_amp[s] * expf(-age * (3.0f + pluck_mute[s] * 18.0f));
        int col = s < 3 ? CLR_LIGHT_GREY : CLR_WHITE;
        if (a > 0.02f) col = pluck_bright[s] > 0.03f ? CLR_YELLOW : pluck_bright[s] < -0.03f ? CLR_ORANGE : CLR_WHITE;
        int prev_y = y;
        for (int x = x0; x <= x1; x += 4) {
            float env = sinf((x - x0) / (float)(x1 - x0) * 3.14159f);
            int yy = y + (int)(sinf(x * 0.25f + at * 90.0f + s) * a * 4.0f * env);
            line(x - 4 < x0 ? x0 : x - 4, prev_y, x, yy, col);
            prev_y = yy;
        }
        // per-string readout: fret, cents, mute
        if (age >= 0 && age < 2.0f) {
            print(str("%d", pluck_fret[s]), 4, y - 3, CLR_LIGHT_GREY);
            if (pluck_mute[s] > 0.12f) print("m", x1 + 8, y - 3, CLR_PINK);
        }
    }
    if (at - sqk_t >= 0 && at - sqk_t < 0.15) print("squeak", x1 - 44, y0 - 22, CLR_PINK);

    // header
    int ch = chord_at(step > 0 ? step - 1 : 0);
    print(human ? "HUMAN HAND" : "KEYBOARD", 4, 4, human ? CLR_GREEN : CLR_RED);
    print(str("%s   %d bpm   take %d", CH_NAME[ch], tempo, seed), 100, 4, CLR_WHITE);
    print(playing ? "" : "stopped", 260, 4, CLR_LIGHT_GREY);

    // last stroke: when each string landed (ms after the beat) and how hard
    int ry = 130;
    print(str("last stroke %c   ms / force / cents", last_dir), 4, ry, CLR_LIGHT_GREY);
    for (int s = 0; s < NSTR; s++) {
        int x = 4 + s * 52;
        if (!last_hit[s]) { print(str("%c  --", "EADGBe"[s]), x, ry + 12, CLR_DARK_GREY); continue; }
        print(str("%c%+3.0f", "EADGBe"[s], last_ms[s]), x, ry + 12, CLR_WHITE);
        rectfill(x, ry + 22, (int)(last_gain[s] * 40), 3, CLR_GREEN);
        print(str("%+.0fc", pluck_cents[s]), x, ry + 28, CLR_INDIGO);
    }

    // layers + controls
    int ly = 176;
    for (int i = 0; i < NLAYER; i++) {
        int c = !human ? CLR_DARK_GREY : layer[i] ? CLR_GREEN : CLR_DARK_GREY;
        static int lx; if (i == 0) lx = 4;
        const char *t = str("%d %s", i + 1, LAYER_NAME[i]);
        print(t, lx, ly, c);
        lx += (int)strlen(t) * 8 + 6;
    }
    print(str("humanity %.2f  TAB  R  SPACE  </>", humanity), 4, ly + 12, CLR_LIGHT_GREY);
}
