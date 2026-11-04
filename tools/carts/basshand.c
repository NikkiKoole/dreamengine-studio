/* de:meta
{
  "slug": "basshand",
  "title": "basshand",
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
  "lineage": "The fifth humanized-playing spike (docs/design/humanized-playing.md): a walking upright bass, pizzicato INSTR_BOWED voiced exactly as the upright and rootwalk carts voice it. Plucks are BOOKED on the audio clock (tight time), and so are the slides: a pluck under the note plus a legato booked note with glide_ms on the mono slot (the 303 slide), sample-accurate with no per-frame riding.",
  "description": {
    "summary": "A walking bass played by two hands instead of a step grid: each pluck has its own force and spot on the string, some notes slide in from below, the left hand lets notes ring or stops them short, ghost notes and rakes fall between the beats, open strings ring on. TAB flips to the grid version.",
    "detail": "Twelve-bar F blues, a walking line in quarter notes. The right hand: beat one leans in, two and four are felt a little more than three, the plucking finger wanders along the string (nearer the bridge = brighter), harder plucks are brighter. The left hand: a note is held into the next (legato) or lifted early, an open string rings longer because no finger stops it, and some notes slide in from a semitone or two below. Between the beats: a dead ghost note (the string muted, just the thump) or a rake into the next note. The time sits a hair ahead of the beat and drifts slowly. Keys 1-5 switch each layer on and off.",
    "controls": "SPACE play/stop · TAB hands / grid · 1 time 2 force 3 touch 4 slides 5 ghosts · UP/DOWN humanity · LEFT/RIGHT tempo · R new take"
  }
}
de:meta */
// basshand — a spike: a walking bass whose notes come from two modeled hands.
//
// Booked: every pluck is schedule_note_at (exact time) with per-note gain, harmonics (where the
// finger plucks) and timbre offsets, and its own gate (how long the left hand lets it ring).
// Slides are BOOKED too: a pluck a semitone or two under, then a legato booked note on the same
// mono slot (NoteOpts.glide_ms) that glides the sounding voice up to the note. A waveguide string
// bends UP cleanly and cannot go below the pitch it started at (the upright cart's finding), so a
// slide always starts low. (A first version rode instrument_tune per frame: audible zipper steps.)

#include "studio.h"
#include <math.h>
#include <string.h>

#define BASS  10
#define GHOST 11          // a muted, dead note: the same string, damped hard, low-passed
#define THUMP 12          // the pluck's wooden thump: the finger + the body knocked, gone in ~0.3 s
#define SNAP  13          // the bright ATTACK: the same string with the noise pluck, dying in ~0.3 s
#define LOOKAHEAD 0.12

// twelve-bar F blues, a walking line, four quarter notes a bar (MIDI, E1 = 28)
static const int LINE[48] = {
    29,33,36,38,   39,38,36,33,   29,31,33,36,   29,33,35,36,      // F7 | Bb7 | F7 | F7
    34,38,41,40,   39,38,36,34,   29,33,36,37,   38,36,33,31,      // Bb7 | Bb7 | F7 | D7
    31,34,38,37,   36,33,31,28,   29,33,34,35,   36,31,33,35,      // Gm7 | C7 | F7 D7 | Gm7 C7
};
static const int OPEN_STR[4] = { 28, 33, 38, 43 };   // E A D G

enum { L_TIME, L_FORCE, L_TOUCH, L_SLIDE, L_GHOST, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "time", "force", "touch", "slides", "ghosts" };

static bool  playing = true, human = true;
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 132;
static int   seed = 1;

static double t_beat;
static int    qn;                 // quarter-note index into LINE

// slides: the latest one, ridden on the slot's tuning every frame
typedef struct { double t; float from; float time; } Slide;
static Slide sl = { -10, 0, 0.1f };
static float tune_last = -99;
static int   n_slides, n_notes;

// display
#define NDOT 48
typedef struct { double t; int midi; float force, spot; int kind; } Dot;   // kind 0 note, 1 ghost, 2 slide, 3 open
static Dot dots[NDOT];
static int ndot;

static float r1(void) { return rnd_float() * 2.0f - 1.0f; }
static float drift(double x, float k) { return noise((float)x * k + seed * 7.7f) * 2.0f - 1.0f; }

static void setup_sound(void) {
    // voiced exactly as upright.c / rootwalk.c
    instrument(BASS, INSTR_BOWED, 4, 0, 7, 475);
    instrument_mode(BASS, MODE_BOW_PIZZ, 1.0f);
#ifndef BASS_BODY
#define BASS_BODY 0.85f
#endif
    instrument_mode(BASS, MODE_BOW_BODY, BASS_BODY);
    instrument_mode(BASS, MODE_BOW_SIZE, BOW_SIZE_BASS);
#ifndef BASS_PLUCK
#define BASS_PLUCK 1.0f   // the finger pluck (MODE_BOW_PLUCK), matched to CC0 upright recordings
#endif
    instrument_mode(BASS, MODE_BOW_PLUCK, BASS_PLUCK);
#ifndef BASS_CUT          // DE_DEFINES overrides, for voicing sweeps against the recordings
#define BASS_CUT 350      // dark: the real G1 centroid is 177 Hz
#endif
#ifndef BASS_HARM
#define BASS_HARM 0.30f
#endif
#ifndef BASS_TIMB
#define BASS_TIMB 0.30f
#endif
    instrument_filter(BASS, FILTER_LOW, BASS_CUT, 3);
    instrument_harmonics(BASS, BASS_HARM);
    instrument_timbre(BASS, BASS_TIMB);
    instrument_morph(BASS, 0.45f);
    instrument_mono(BASS, 1);               // one string sounding at a time, like a walking line
    instrument_vibrato(BASS, 0.0f);         // no built-in wobble on plucked notes
    // the ghost: the same string, hard-damped and dark (just the thump of the finger)
    instrument(GHOST, INSTR_BOWED, 2, 0, 7, 35);
    instrument_mode(GHOST, MODE_BOW_PIZZ, 1.0f);
    instrument_mode(GHOST, MODE_BOW_BODY, 0.85f);
    instrument_mode(GHOST, MODE_BOW_SIZE, BOW_SIZE_BASS);
    instrument_filter(GHOST, FILTER_LOW, 500, 2);
    instrument_harmonics(GHOST, 0.2f); instrument_timbre(GHOST, 0.1f); instrument_morph(GHOST, 0.9f);
    instrument_vibrato(GHOST, 0.0f);
    instrument_choke(GHOST, BASS);          // a dead note stops the ringing one
    // THE THUMP (2026-11-04): a real pizz drops ~15 dB in its first half second and then rings on
    // steady (measured on CC0 upright recordings); ours only faded. A short, low, damped membrane hit
    // under every pluck gives that attack-then-bloom shape and some wood between the harmonics.
#ifndef BASS_THUMP
#define BASS_THUMP 1.0f
#endif
#ifndef BASS_ROOM
#define BASS_ROOM 0.30f
#endif
#ifndef BASS_LVL
#define BASS_LVL 0.25f    // the string sits under the thump: a real pizz is loud, then rings on ~15 dB down
#endif
#ifndef THUMP_MS
#define THUMP_MS 300
#endif
#ifndef THUMP_NOTE
#define THUMP_NOTE 38     // a fixed body pitch (~73 Hz); 0 = an octave under the note
#endif
    instrument_level(BASS, BASS_LVL);
    instrument(THUMP, INSTR_MEMBRANE, 1, 0, 7, THUMP_MS);
    instrument_harmonics(THUMP, 0.80f);      // inharmonic: wood, not a second pitch
    instrument_timbre(THUMP, 0.20f);         // near the centre: a round knock
    instrument_morph(THUMP, 0.25f);          // a little pitch drop off the hit
    instrument_filter(THUMP, FILTER_LOW, 700, 1);
    instrument_reverb(THUMP, BASS_ROOM);
    // THE SNAP: the old bright noise pluck on its own slot, decaying fast, over the dark finger
    // pluck that rings on: loud-then-bloom AND a colour that darkens as the note rings (measured:
    // every real harmonic decays at its own rate; ours all at one, a static spectrum)
#ifndef BASS_SNAP
#define BASS_SNAP 0.0f   // tried: makes it BRIGHTER (centroid 2x), the real attack is low, not bright
#endif
#ifndef BASS_SNAP_MS
#define BASS_SNAP_MS 280
#endif
    instrument(SNAP, INSTR_BOWED, 2, BASS_SNAP_MS, 0, 40);
    instrument_mode(SNAP, MODE_BOW_PIZZ, 1.0f);
    instrument_mode(SNAP, MODE_BOW_BODY, 0.85f);
    instrument_mode(SNAP, MODE_BOW_SIZE, BOW_SIZE_BASS);
    instrument_filter(SNAP, FILTER_LOW, 2500, 2);
    instrument_harmonics(SNAP, 0.30f); instrument_timbre(SNAP, 0.40f); instrument_morph(SNAP, 0.45f);
    instrument_vibrato(SNAP, 0.0f);
    instrument_reverb(SNAP, BASS_ROOM);
    // the room: a small, dark space (the recordings were made in one, a dry string never is)
    instrument_reverb(BASS, BASS_ROOM);
    instrument_reverb(GHOST, BASS_ROOM * 0.6f);
    reverb(0.22f, 0.7f);
    // LEVEL: the string sits low under the thump, so the whole cart came out ~10 dB quiet (Nikki: "a
    // bit softer"). No master volume exists; the master EQ in dB does the job, the cart is all bass.
#ifndef BASS_MASTER_DB
#define BASS_MASTER_DB 11.5f
#endif
    eq(BASS_MASTER_DB, BASS_MASTER_DB - 2.0f, 0.0f);
}

static void new_take(void) { rnd_seed(seed); qn = 0; sl.t = -10; t_beat = audio_time() + 0.25; }

static void dot(double t, int midi, float force, float spot, int kind) {
    dots[ndot % NDOT] = (Dot){ t, midi, force, spot, kind }; ndot++;
}

static bool is_open(int midi) { for (int s = 0; s < 4; s++) if (OPEN_STR[s] == midi) return true; return false; }

static void play_quarter(int q, double t, double beat_len) {
    float H = humanity;
    int midi = LINE[q % 48], next = LINE[(q + 1) % 48];
    int beat = q % 4;
    bool T = human && layer[L_TIME], F = human && layer[L_FORCE], C = human && layer[L_TOUCH];
    bool S = human && layer[L_SLIDE], G = human && layer[L_GHOST];

    // time: a hair ahead of the beat, drifting
    double tt = t;
    if (T) tt += (-0.006 + drift(t, 0.2f) * 0.007 + r1() * 0.003) * H;

    // force: one leans in, two and four a touch more than three
    float force = 0.8f;
    if (F) force = (beat == 0 ? 0.92f : beat == 2 ? 0.74f : 0.82f) + r1() * 0.06f * H + drift(t, 0.15f) * 0.05f * H;

    // touch: where the finger plucks wanders; harder = brighter; the left hand decides the length
    float spot = 0, bright = 0, gate_frac = 0.92f;
    bool open = is_open(midi);
    if (C) {
        spot = drift(t, 0.4f) * 0.08f * H;                       // nearer the bridge = brighter
        bright = ((force - 0.8f) * 0.4f + (open ? 0.05f : 0)) * H;
        gate_frac = 0.55f + 0.5f * rnd_float();                  // lifted early .. held into the next
        if (open) gate_frac = 1.4f;                              // nothing stops an open string
    }
    int gate = (int)(beat_len * gate_frac * 1000.0);

    // slide: some notes come in from below (only up, the string can't bend under its start)
    bool slide = S && !open && midi > 30 && rnd_float() < 0.30f * H;
    n_notes++;
    if (slide) {
        n_slides++;
        sl.t = tt; sl.from = -(1.0f + (rnd_float() < 0.4f ? 1.0f : 0.0f)); sl.time = 0.06f + 0.06f * rnd_float();
    }

    NoteOpts o = { .gain = force, .harmonics = spot, .timbre = bright };
    if (slide) {
        // a booked slide: pluck under the note, then a LEGATO booked note glides the same voice up
        // (schedule_note_at .glide_ms on a mono slot: no new attack, sample-accurate, smooth). The
        // string bends UP cleanly, so the slide always starts low.
        int ms = (int)(sl.time * 1000.0f);
        schedule_note_at(tt, midi + (int)sl.from, BASS, 7, 40, &o);
        NoteOpts g = o; g.glide_ms = ms;
        schedule_note_at(tt + 0.012, midi, BASS, 7, gate, &g);
    } else {
        schedule_note_at(tt, midi, BASS, 7, gate, &o);
    }
    if (BASS_SNAP > 0) {
        NoteOpts k = { .gain = force * BASS_SNAP, .harmonics = spot };
        schedule_note_at(tt, midi, SNAP, 7, BASS_SNAP_MS, &k);
    }
    if (BASS_THUMP > 0) {
        // the knock follows the note an octave down-ish but stays low (the body, not the string)
        int tm = THUMP_NOTE > 0 ? THUMP_NOTE : (midi - 12 < 24 ? 24 : midi - 12);
        float tg = force * 0.85f, tt_spot = spot, tmorph = 0, tharm = 0;
        int   tgate = 90;
        if (C) {
            // the thump is the HAND too (Nikki: "the thump sounds very identical"): it follows the
            // force squared (soft plucks barely knock), sometimes a light finger hardly knocks at
            // all, the strike spot and knock character move, the body knock sits lower under low
            // notes, and each one lasts its own time
            tg = force * force * 1.05f * (0.75f + 0.25f * rnd_float());
            if (rnd_float() < 0.15f * H) tg *= 0.35f;
            tt_spot = spot + r1() * 0.15f * H;
            tmorph = r1() * 0.15f * H;
            tharm = r1() * 0.10f * H;
            tm += (midi - 36) / 6 + (rnd_float() < 0.3f * H ? (rnd_float() < 0.5f ? -1 : 1) : 0);
            tgate = 60 + rnd(70);
        }
        NoteOpts t = { .gain = tg * BASS_THUMP, .timbre = tt_spot, .morph = tmorph, .harmonics = tharm };
        schedule_note_at(tt, tm, THUMP, 7, tgate, &t);
    }
    dot(tt, midi, force, spot, slide ? 2 : open ? 3 : 0);

    // between the beats: a ghost (dead thump) or a rake into the next note, on the swung "and"
    if (G && rnd_float() < 0.22f * H) {
        double ta = t + beat_len * 0.66 + r1() * 0.004 * H;
        if (rnd_float() < 0.6f) {
            NoteOpts g = { .gain = 0.35f + 0.15f * rnd_float() };
            schedule_note_at(ta, next, GHOST, 7, 40, &g);
            dot(ta, next, g.gain, 0, 1);
        } else {
            // a rake: a quick soft note a step above or below, into the next beat
            int rk = next + (rnd_float() < 0.5f ? 1 : -1);
            NoteOpts g = { .gain = 0.45f, .timbre = -0.1f };
            schedule_note_at(ta, rk, BASS, 7, (int)(beat_len * 300), &g);
            dot(ta, rk, 0.45f, 0, 1);
        }
    }
}

static void schedule_ahead(void) {
    double now_a = audio_time();
    double len = 60.0 / tempo;
    while (t_beat < now_a + LOOKAHEAD) {
        if (t_beat < now_a) t_beat = now_a + 0.01;
        play_quarter(qn, t_beat, len);
        t_beat += len;
        qn = (qn + 1) % 48;
    }
}

// every frame: the slide on the slot's tuning (starts low, rises into the note)
static void ride_slide(void) {
    double now_a = audio_time();
    float semi = 0;
    if (false) {   // superseded: slides are booked glides now (see play_quarter); kept for the A/B note in the doc
        float a = (float)(now_a - sl.t);
        if (a >= -0.04f && a < 0) semi = sl.from;   // already down before the pluck (wider than a frame; the ringing note cannot bend below its start, so this is harmless)
        else if (a >= 0 && a < sl.time) { float k = a / sl.time; semi = sl.from * (1 - k) * (1 - k); }
    }
    if (fabsf(semi - tune_last) > 0.01f) { instrument_tune(BASS, semi); tune_last = semi; }
}

void init(void) {
    setup_sound(); new_take();
#ifdef PROBE_NOTE    // DE_DEFINES=PROBE_NOTE=<midi>: one plain pluck, for the A/B against a recording
    playing = false;
    NoteOpts o = { .gain = 0.9f };
    schedule_note_at(audio_time() + 0.2, PROBE_NOTE, BASS, 7, 3000, &o);
    if (BASS_SNAP > 0) {
        NoteOpts k = { .gain = 0.9f * BASS_SNAP };
        schedule_note_at(audio_time() + 0.2, PROBE_NOTE, SNAP, 7, BASS_SNAP_MS, &k);
    }
    if (BASS_THUMP > 0) {
        NoteOpts t = { .gain = 0.9f * 0.85f * BASS_THUMP };
        schedule_note_at(audio_time() + 0.2, THUMP_NOTE > 0 ? THUMP_NOTE : (PROBE_NOTE - 12 < 24 ? 24 : PROBE_NOTE - 12), THUMP, 7, 90, &t);
    }
#endif
}

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; t_beat = audio_time() + 0.15; }
    if (keyp(KEY_TAB)) human = !human;
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) layer[i] = !layer[i];
    if (keyp(KEY_UP))    humanity = fminf(1.5f, humanity + 0.25f);
    if (keyp(KEY_DOWN))  humanity = fmaxf(0.0f, humanity - 0.25f);
    if (keyp(KEY_RIGHT)) tempo = min(220, tempo + 6);
    if (keyp(KEY_LEFT))  tempo = max(70, tempo - 6);
    if (keyp('R')) { seed++; new_take(); }
    if (playing) schedule_ahead();
    ride_slide();
#ifdef DE_TRACE
    watch("qn", "%d", qn);
    watch("tune", "%.2f", tune_last);
    watch("slides", "%d/%d", n_slides, n_notes);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    double at = audio_time();
    print(human ? "HANDS" : "GRID", 4, 4, human ? CLR_GREEN : CLR_RED);
    print(str("F blues walk  %d bpm  bar %d  take %d", tempo, (qn / 4) % 12 + 1, seed), 52, 4, CLR_WHITE);

    // the fingerboard: four strings, notes as dots where they land (pitch = x), fading
    int x0 = 16, x1 = 304, y0 = 40, gap = 22;
    rectfill(x0 - 6, y0 - 12, x1 - x0 + 12, gap * 3 + 24, CLR_BLACK);
    for (int s = 0; s < 4; s++) {
        int y = y0 + gap * (3 - s);
        line(x0, y, x1, y, s == 0 ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY);
        print(str("%c", "EADG"[s]), x0 - 12, y - 3, CLR_DARK_GREY);
    }
    for (int k = 0; k < NDOT && k < ndot; k++) {
        Dot *d = &dots[k];
        float age = (float)(at - d->t);
        if (age < 0 || age > 0.9f) continue;
        int s = 0; while (s < 3 && d->midi >= OPEN_STR[s + 1]) s++;
        int fret = d->midi - OPEN_STR[s];
        int x = x0 + 8 + fret * 22, y = y0 + gap * (3 - s);
        int r = 2 + (int)(d->force * 5 * (1.0f - age / 0.9f));
        int c = d->kind == 1 ? CLR_DARK_GREY : d->kind == 2 ? CLR_YELLOW : d->kind == 3 ? CLR_GREEN : CLR_WHITE;
        circfill(x, y, r, c);
    }
    print("white note  yellow slide  green open  grey ghost", 4, 130, CLR_LIGHT_GREY);
    if (human && tune_last < -0.05f) print(str("slide %.1f", tune_last), 240, 142, CLR_YELLOW);

    int y2 = 176, lx = 4;
    for (int i = 0; i < NLAYER; i++) {
        int c = !human ? CLR_DARK_GREY : layer[i] ? CLR_GREEN : CLR_DARK_GREY;
        const char *t = str("%d %s", i + 1, LAYER_NAME[i]);
        print(t, lx, y2, c);
        lx += (int)strlen(t) * 8 + 6;
    }
    print(str("humanity %.2f  TAB  R  SPACE  </>", humanity), 4, y2 + 12, CLR_LIGHT_GREY);
}
