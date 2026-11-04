/* de:meta
{
  "slug": "breathflute",
  "title": "breathflute",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "waveguide-synth",
    "analog-voice-modeling",
    "generative-melody"
  ],
  "lineage": "The second humanized-playing spike for Playtime, after strumhand (the guitar). Where the guitar was all about the ATTACK, a flute is all about what happens DURING the note, so this one drives held INSTR_PIPE notes live every frame: note_vol, note_pitch, note_timbre, note_lfo. Uses instrument_vibrato (added for these spikes) to switch off the engine's own always-on vibrato.",
  "description": {
    "summary": "A flute played by a pair of LUNGS instead of a keyboard: the same original melody with breath that runs out, notes that scoop up into pitch, a vibrato that blooms late, swells on long notes, slurs and tonguing, and a phrase that pushes and broadens. TAB flips to the keyboard version.",
    "detail": "The player has an air supply. Every note spends some, so late in a phrase the tone gets airier and quieter and the vibrato a little quicker, and a long phrase ahead means a deeper breath (which takes longer, so the time bends by itself). Each note starts a few cents flat and settles; blowing harder pushes it sharp, as a real flute does. Vibrato is off at the start of a note and blooms after a moment, only on notes long enough to have one, with a rate that wanders. Long notes swell and fade. Slurred notes glide on one breath, tongued notes restart it with a chiff. The tempo pushes through the middle of a phrase and broadens at the end. The trace shows the pitch (cents exaggerated) and loudness as it plays. Keys 1-5 switch each layer on and off.",
    "controls": "SPACE play/stop · TAB lungs / keyboard · B booked (NoteOpts) / held · 1 time 2 breath 3 pitch 4 vib 5 tongue · UP/DOWN humanity · LEFT/RIGHT tempo · R new take"
  }
}
de:meta */
// breathflute — a spike: does a flute line sound more human when each note is shaped DURING
// the note by a model of the player's breath?
//
// strumhand (the guitar) could book every note ahead, because a pluck is all onset. A flute
// isn't: the scoop, the late vibrato, the swell, the air running out all happen while the note
// sounds. So this cart holds notes live (note_on) and rides them every frame. That is exactly
// what a BOOKED note (schedule_note_at, what Playtime uses) cannot do today — the finding.
//
// Engine pieces: INSTR_PIPE with instrument_vibrato(SL, 0) (built-in vibrato off), then per held note
// note_vol (breath), note_pitch (+ note_glide, scoop and breath-sharp), note_timbre (air),
// note_harmonics (louder = brighter), note_lfo(LFO_PITCH) (our vibrato).

#include "studio.h"
#include <math.h>
#include <string.h>

#define SL 10

// ---- the melody (original), lengths in eighths ----
// art: 't' tongued, 'd' soft tongue, '_' slurred from the previous note, '!' tongued + accent
// breath: 1 = take a breath after this note (ends the phrase)
typedef struct { int midi; int len; char art; int breath; } N;
static const N TUNE[] = {
    {69,2,'t',0},{74,2,'_',0},{78,3,'_',0},{76,1,'_',0},{74,2,'t',0},{71,2,'_',0},{69,4,'t',1},
    {71,1,'t',0},{73,1,'_',0},{74,2,'_',0},{76,2,'t',0},{78,1,'_',0},{79,1,'_',0},{81,4,'!',0},
        {78,2,'_',0},{74,2,'_',0},{76,6,'t',1},
    {78,2,'d',0},{76,1,'_',0},{74,1,'_',0},{73,2,'d',0},{71,2,'_',0},{69,2,'d',0},{71,1,'_',0},
        {73,1,'_',0},{74,8,'t',1},
};
#define NT ((int)(sizeof TUNE / sizeof TUNE[0]))

enum { L_TIME, L_BREATH, L_PITCH, L_VIB, L_TONGUE, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "time", "breath", "pitch", "vib", "tongue" };

static bool  playing = true, human = true;
static bool  booked = false;   // B: play the line as BOOKED notes (NoteOpts step 2: .vib), no per-frame riding
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 76;
static int   seed = 1;

// ---- the player ----
static float t_now;             // seconds (frame clock)
static float t_next;            // when the next note (or the breath before it) starts
static int   idx;               // next note in TUNE
static float air = 1.0f;        // lungs 0..1

// the sounding note
static int   h = 0;             // held handle, 0 = none
static float n_start, n_end;    // this note's start and planned end
static int   n_midi, n_i;
static float n_vol, n_scoop, n_vib_delay, n_vib_depth, n_vib_rate, n_sharp;
static bool  n_slurred;
static float last_vol = -1, last_pitch = -999, last_timb = -1, last_harm = -1, last_vd = -1, last_vr = -1;
static float n_air0;

// trace
#define TR 300
static float tr_pitch[TR], tr_vol[TR];
static int   tr_pos;
static float shown_cents, shown_vib, shown_air;

static float r1(void) { return rnd_float() * 2.0f - 1.0f; }
static float smooth01(float x) { x = clamp(x, 0, 1); return x * x * (3 - 2 * x); }

static void setup_sound(void) {
    instrument(SL, INSTR_PIPE, 30, 0, 7, 110);
    instrument_harmonics(SL, 0.0f);
    instrument_timbre(SL, 0.38f);
    instrument_morph(SL, 0.70f);         // the in-tune flute embouchure
    instrument_reverb(SL, 0.30f);
    reverb(0.6f, 0.45f);
}

static void new_take(void) {
    rnd_seed(seed);
    idx = 0; air = 1.0f; t_next = t_now + 0.3f;
}

// where in its phrase a note sits: 0 = first .. 1 = last, plus the phrase's peak pitch
static void phrase_of(int i, float *pos, int *lo, int *hi, int *peak) {
    int a = i; while (a > 0 && !TUNE[a - 1].breath) a--;
    int b = i; while (b < NT - 1 && !TUNE[b].breath) b++;
    *pos = (b > a) ? (i - a) / (float)(b - a) : 0;
    *lo = a; *hi = b;
    int pk = 0; for (int k = a; k <= b; k++) if (TUNE[k].midi > pk) pk = TUNE[k].midi;
    *peak = pk;
}

static void release(void) {
    if (h) note_off(h);
    h = 0;
}

static void start_note(int i) {
    const N *n = &TUNE[i];
    float H = humanity, eighth = 60.0f / tempo / 2.0f;
    float pos; int lo, hi, peak; phrase_of(i, &pos, &lo, &hi, &peak);
    bool slur = human && layer[L_TONGUE] && n->art == '_' && h;

    // TIME: push through the middle of a phrase, broaden into its end
    float dur = n->len * eighth;
    if (human && layer[L_TIME]) {
        float push = 1.0f - 0.07f * sinf(3.14159f * fminf(pos * 1.25f, 1.0f)) * H;
        float broad = 1.0f + 0.22f * smooth01((pos - 0.75f) / 0.25f) * H;
        dur *= push * broad * (1.0f + r1() * 0.03f * H);
    }

    // BREATH: louder toward the phrase's peak, higher notes speak louder, the lungs run down
    float vol = 5.0f;
    if (human && layer[L_BREATH]) {
        float height = 1.0f - fminf(1.0f, (peak - n->midi) / 10.0f);
        vol = 4.2f + 1.9f * height * H + (n->art == '!' ? 0.6f : 0) + r1() * 0.15f * H;
        vol -= (1.0f - air) * 1.2f * H;
        if (n->breath) vol -= 0.3f * H;                      // the last note before a breath tapers
    }

    // PITCH: start flat and settle; blowing harder pushes a flute sharp
    n_scoop = 0; n_sharp = 0;
    if (human && layer[L_PITCH]) {
        n_scoop = -(slur ? 6.0f : 14.0f + 10.0f * rnd_float()) * H;    // cents
        n_sharp = (vol - 5.0f) * 4.0f * H + r1() * 2.0f * H;
    }

    // VIBRATO: none on short notes; on long ones it blooms after a moment, rate wanders
    n_vib_depth = 0; n_vib_delay = 0; n_vib_rate = 5.2f;
    if (human && layer[L_VIB] && dur > 0.32f) {
        n_vib_delay = (0.16f + 0.2f * rnd_float()) * (dur > 1.0f ? 1.3f : 1.0f);
        n_vib_depth = (0.13f + 0.08f * rnd_float() + (n->midi == peak ? 0.05f : 0)) * H;
        n_vib_rate = 4.9f + 0.8f * rnd_float() + (1.0f - air) * 0.5f * H;
    }

    // TONGUE: slurred notes move the same breath; tongued ones restart it
    instrument_vibrato(SL, human && layer[L_VIB] ? 0.0f : 1.0f);
    if (slur) {
        note_glide(h, 35 + rnd(25));
        note_pitch(h, n->midi + n_scoop / 100.0f);
    } else {
        release();
        h = note_on(n->midi, SL, 0);
        note_glide(h, human ? 18 : 0);
        if (human && layer[L_PITCH]) note_pitch(h, n->midi + n_scoop / 100.0f);
        if (human && layer[L_TONGUE]) {
            float chiff = n->art == '!' ? 0.95f : n->art == 'd' ? 0.45f : 0.75f;
            note_timbre(h, chiff);                         // a burst of air on the tongue, ridden down
        }
        last_vol = last_pitch = last_timb = last_harm = last_vd = last_vr = -999;
    }
    n_start = t_now; n_midi = n->midi; n_i = i; n_vol = vol; n_slurred = slur; n_air0 = air;
    // the note sounds a little less than its slot when it is tongued (the tongue stops the air)
    float gap = (human && layer[L_TONGUE] && i + 1 < NT && TUNE[i + 1].art != '_' && !n->breath) ? 0.05f : 0.02f;
    n_end = t_now + dur - (n->breath ? 0 : gap);
    if (!human) n_end = t_now + dur * 0.9f;
    t_next = t_now + dur;
    if (booked && human) {
        // BOOKED (the acceptance test for NoteOpts step 2): the same plan, carried BY the note. Level,
        // breath-sharp cents, the slur and the late-blooming vibrato travel with it; nothing is ridden.
        // What booking cannot carry yet (step 3, curves): the scoop settling in, the swell, the chiff decay.
        release();
        NoteOpts o = { .gain = clamp(vol / 7.0f, 0.05f, 1.0f), .cents = n_sharp,
                       .glide_ms = (slur && layer[L_TONGUE]) ? 40 : 0,
                       .vib = { .cents = n_vib_depth * 100.0f, .rate_hz = n_vib_rate,
                                .delay_ms = (int)(n_vib_delay * 1000.0f), .fade_ms = 400, .wander = 0.5f } };
        schedule_note_at(audio_time() + 0.03, n->midi, SL, 7, (int)((n_end - n_start) * 1000.0f), &o);
        h = 0;                                   // nothing to ride
    }

    // breath before the next phrase: a longer phrase ahead = a deeper breath = more time
    if (n->breath) {
        int nxt = (i + 1) % NT, lo2, hi2, pk2; float p2;
        phrase_of(nxt, &p2, &lo2, &hi2, &pk2);
        int ahead = 0; for (int k = lo2; k <= hi2; k++) ahead += TUNE[k].len;
        float gasp = human && layer[L_BREATH] ? (0.22f + ahead * 0.012f) * (0.7f + 0.3f * H) : eighth * 2;
        t_next += gasp;
    }
}

// every frame: ride the held note
static void ride(void) {
    if (!h) return;
    float H = humanity;
    float age = t_now - n_start, len = n_end - n_start;
    float p = clamp(age / fmaxf(len, 0.01f), 0, 1);
    if (t_now >= n_end) {
        bool next_slur = human && layer[L_TONGUE] && idx < NT && TUNE[idx].art == '_' && !TUNE[n_i].breath;
        if (!next_slur) release();
        return;
    }
    if (!human) { if (last_vol < 0) { note_vol(h, 5.5f); last_vol = 5.5f; } return; }

    // breath: a swell on long notes (messa di voce), a soft fade before a breath
    float v = n_vol;
    if (layer[L_BREATH]) {
        if (len > 0.5f) v *= 0.86f + 0.18f * sinf(3.14159f * powf(p, 0.8f)) * H;
        if (TUNE[n_i].breath) v *= 1.0f - 0.35f * smooth01((p - 0.6f) / 0.4f) * H;
        air -= dt() * (0.035f + v * 0.012f);               // breathing out
        if (air < 0.05f) air = 0.05f;
    }
    v = clamp(v, 0.3f, 7.0f);

    // pitch: settle from the scoop in ~80ms, plus breath-sharp
    float cents = 0;
    if (layer[L_PITCH]) cents = n_scoop * expf(-age / 0.045f) + n_sharp * (v / fmaxf(n_vol, 0.5f));
    float pitch = n_midi + cents / 100.0f;

    // air: the chiff decays, the lungs running out make it breathier
    float timb = 0.38f;
    if (layer[L_TONGUE] && !n_slurred) timb += 0.4f * expf(-age / 0.04f);
    if (layer[L_BREATH]) timb += (1.0f - air) * 0.30f * H + (5.0f - v) * 0.03f * H;
    timb = clamp(timb, 0, 1);
    float harm = layer[L_BREATH] ? clamp((v - 4.5f) * 0.05f * H, 0, 0.15f) : 0;

    // vibrato: blooms after its delay, over ~0.4s; the rate wanders
    float vd = 0, vr = n_vib_rate;
    if (layer[L_VIB] && n_vib_depth > 0) {
        vd = n_vib_depth * smooth01((age - n_vib_delay) / 0.4f);
        vr += 0.35f * (noise(t_now * 0.7f + seed) * 2 - 1);
    }

    if (fabsf(v - last_vol) > 0.02f)       { note_vol(h, v); last_vol = v; }
    if (fabsf(pitch - last_pitch) > 0.002f) { note_pitch(h, pitch); last_pitch = pitch; }
    if (fabsf(timb - last_timb) > 0.005f)   { note_timbre(h, timb); last_timb = timb; }
    if (fabsf(harm - last_harm) > 0.005f)   { note_harmonics(h, harm); last_harm = harm; }
    if (fabsf(vd - last_vd) > 0.003f || fabsf(vr - last_vr) > 0.05f) {
        note_lfo(h, 0, LFO_PITCH, vr, vd); last_vd = vd; last_vr = vr;
    }
    shown_cents = cents; shown_vib = vd; shown_air = air;
}

void init(void) {
    setup_sound();
    new_take();
#ifdef BOOKED
    booked = true;
    instrument_mono(SL, 1);
#endif
#ifdef PROBE_STILL   // DE_DEFINES=PROBE_STILL=0|1: one held A4, 1 = instrument_vibrato 0, to measure the switch
    instrument_vibrato(SL, PROBE_STILL ? 0.0f : 1.0f);
    note_on(69, SL, 5);
    playing = false;
#endif
}

void update(void) {
#ifdef PROBE_STILL
    return;
#endif
    if (keyp(KEY_SPACE)) { playing = !playing; if (!playing) release(); t_next = t_now + 0.2f; }
    if (keyp(KEY_TAB)) { human = !human; release(); }
    if (keyp('B')) { booked = !booked; release(); instrument_mono(SL, booked ? 1 : 0); }   // booked slurs glide one mono voice
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) layer[i] = !layer[i];
    if (keyp(KEY_UP))    humanity = fminf(1.5f, humanity + 0.25f);
    if (keyp(KEY_DOWN))  humanity = fmaxf(0.0f, humanity - 0.25f);
    if (keyp(KEY_RIGHT)) tempo = min(120, tempo + 4);
    if (keyp(KEY_LEFT))  tempo = max(50, tempo - 4);
    if (keyp('R')) { seed++; release(); new_take(); }

    t_now += dt();
    if (playing) {
        if (t_now >= t_next) {
            if (idx == 0 || TUNE[idx - 1].breath) air = 1.0f;   // the breath filled the lungs
            start_note(idx);
            idx = (idx + 1) % NT;
        }
        ride();
    }

    // trace: what the note is doing right now
    float pv = 0, vv = 0;
    if (h) { pv = n_midi + (human ? shown_cents / 100.0f : 0); vv = human ? last_vol : 5; }
    tr_pitch[tr_pos] = h ? pv + (human ? shown_vib * sinf(t_now * 6.28f * n_vib_rate) : 0) : 0;
    tr_vol[tr_pos] = h ? vv : 0;
    tr_pos = (tr_pos + 1) % TR;
#ifdef DE_TRACE
    watch("idx", "%d", idx);
    watch("air", "%.2f", air);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print(human ? "LUNGS" : "KEYBOARD", 4, 4, human ? CLR_GREEN : CLR_RED);
    if (human) print(booked ? "B: BOOKED (step 2)" : "B: held + ridden", 150, 160, booked ? CLR_YELLOW : CLR_DARK_GREY);
    print(str("%d bpm   take %d", tempo, seed), 100, 4, CLR_WHITE);
    if (!playing) print("stopped", 260, 4, CLR_LIGHT_GREY);

    // the trace: pitch over time, with cents drawn 6x larger so a scoop and a vibrato show
    int x0 = 10, top = 22, bot = 122;
    rectfill(x0, top, TR, bot - top, CLR_DARKER_GREY);
    for (int m = 69; m <= 81; m++) {
        int y = bot - (m - 67) * (bot - top) / 16;
        line(x0, y, x0 + TR - 1, y, (m == 69 || m == 74 || m == 81) ? CLR_DARK_GREY : CLR_DARKER_BLUE);
    }
    for (int k = 0; k < TR; k++) {
        int i = (tr_pos + k) % TR;
        if (tr_pitch[i] <= 0) continue;
        float m = tr_pitch[i];
        float base = floorf(m + 0.5f), dev = (m - base) * 6.0f;
        int y = bot - (int)((base + dev - 67) * (bot - top) / 16.0f);
        int th = 1 + (int)(tr_vol[i] / 2.5f);
        rectfill(x0 + k, y - th / 2, 1, th, tr_vol[i] > 5.5f ? CLR_YELLOW : CLR_WHITE);
    }
    print("pitch (cents x6), thickness = breath", x0, bot + 3, CLR_LIGHT_GREY);

    // lungs
    int ly = 140;
    print("air", 4, ly, CLR_LIGHT_GREY);
    rect(36, ly, 102, 8, CLR_LIGHT_GREY);
    rectfill(37, ly + 1, (int)(100 * air), 6, air > 0.35f ? CLR_BLUE : CLR_ORANGE);
    if (human && h) print(str("%+3.0fc  vib %.2f", shown_cents, shown_vib), 150, ly, CLR_INDIGO);

    int y2 = 176, lx = 4;
    for (int i = 0; i < NLAYER; i++) {
        int c = !human ? CLR_DARK_GREY : layer[i] ? CLR_GREEN : CLR_DARK_GREY;
        const char *t = str("%d %s", i + 1, LAYER_NAME[i]);
        print(t, lx, y2, c);
        lx += (int)strlen(t) * 8 + 6;
    }
    print(str("humanity %.2f  TAB  R  SPACE  </>", humanity), 4, y2 + 12, CLR_LIGHT_GREY);
}
