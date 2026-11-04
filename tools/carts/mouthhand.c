/* de:meta
{
  "slug": "mouthhand",
  "title": "mouthhand",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "subtractive-synth",
    "per-instrument-fx-bus"
  ],
  "lineage": "The eleventh humanized-playing spike (docs/design/humanized-playing.md): a MOUTH on any sound. A moving formant (instrument_formant, a vowel filter on the slot's FX bus) reads as a voice and a voice reads as alive: the talk box, the trumpet's plunger mute, a wah. Rides today's call, smoothed, with no engine change, to find out whether a slewed live vowel and a per-voice formant are worth building.",
  "description": {
    "summary": "A funky synth bass and a lead, each with its own MOUTH: a vowel filter that opens when a note is played harder, says 'wah' on every attack, drifts, and makes the lead's long notes talk. TAB freezes the mouth on one vowel to hear what the movement adds.",
    "detail": "Both parts are monophonic, so one formant per instrument (the engine's formant sits on the slot's effects bus: one mouth) can still shape every note. The vowel runs U, O, A, E, I. Follow: harder notes open toward A, softer ones close toward U, the way a voice and a wah pedal both behave. Wah: each attack starts closed and opens over ~0.1 s (a synth bass saying 'yow'). Drift: a slow wander so nothing sits on one frozen colour. Talk: the lead's long notes move through a little vowel line, a talk box. The vowel is smoothed and re-sent only when it moves, because the engine jumps to a new vowel with no slew of its own. Keys 1-5 switch each layer on and off.",
    "controls": "SPACE play/stop · TAB moving / frozen mouth · 1 mouth 2 follow 3 wah 4 drift 5 talk · UP/DOWN humanity · R new take"
  }
}
de:meta */
// mouthhand — a spike: a moving formant as a humanizer for ANY sound.
//
// instrument_formant(slot, vowel, q, mix) puts a vowel filter on the slot's FX bus. Its vowel can
// change any time (fx_set_formant only recomputes 4 bandpass coefficients) but it JUMPS: no slew.
// So this cart smooths the vowel itself and re-sends it only when it moved (fx-lint-ignore below:
// riding THIS effect per frame is the point of the spike, and it is a coefficient update, not a
// rebuild). Bass and lead are mono, held, frame-timed.

#include "studio.h"
#include <math.h>
#include <string.h>

#define BASS 10
#define LEAD 11
#define KICK 12
#define HAT  13

enum { L_MOUTH, L_FOLLOW, L_WAH, L_DRIFT, L_TALK, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "mouth", "follow", "wah", "drift", "talk" };

static bool  playing = true, human = true;
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 100;
static int   seed = 1;

// ---- the music (original): a 2-bar funk bass in E minor, 16ths; 0 = rest, '>' accent, 'g' ghost ----
typedef struct { int midi; char mark; } BN;
static const BN BASSL[32] = {
    {40,'>'},{0,' '},{52,'g'},{40,' '}, {0,' '},{43,' '},{0,' '},{45,'>'},
    {0,' '},{40,'g'},{47,' '},{0,' '},  {50,'>'},{0,' '},{47,'g'},{45,' '},
    {40,'>'},{0,' '},{52,'g'},{40,' '}, {0,' '},{43,' '},{45,'g'},{47,'>'},
    {0,' '},{50,' '},{0,' '},{52,'>'},  {50,' '},{47,'g'},{45,' '},{43,' '},
};
// lead: midi (0 rest), length in 16ths, the vowel it "says" (0 U .. 1 I) for the talk layer
typedef struct { int midi; int len; float say; } LN;
static const LN LEADL[] = {
    {0,8,0},{71,6,0.5f},{74,2,0.75f},{76,8,0.5f},{74,4,0.0f},{71,4,0.25f},
    {0,4,0},{69,4,0.5f},{71,4,1.0f},{67,12,0.5f},{0,8,0},
    {76,4,0.75f},{79,4,0.5f},{78,4,0.25f},{76,12,0.5f},{74,4,0.0f},{71,8,0.5f},{0,4,0},
};
#define NLEAD ((int)(sizeof LEADL / sizeof LEADL[0]))

static float t_now, t_step, t_lead;
static int   step16, lead_i;
static int   bass_h = 0, lead_h = 0;
static float bass_t0, bass_vel, lead_t0, lead_len, lead_say, lead_vel;

// the two mouths
typedef struct { float cur, sent; float note_target; float t0; } Mouth;
static Mouth mb = { 0.3f, -1, 0.3f, -10 }, ml = { 0.4f, -1, 0.4f, -10 };
static float mix_sent_b = -1, mix_sent_l = -1;
static float shown_vb, shown_vl;

static float r1(void) { return rnd_float() * 2.0f - 1.0f; }
static float smooth01(float x) { x = clamp(x, 0, 1); return x * x * (3 - 2 * x); }

static void setup_sound(void) {
    instrument(BASS, INSTR_SAW, 2, 180, 4, 60);
    instrument_filter(BASS, FILTER_LADDER, 1400, 3);
    instrument_mono(BASS, 1);
    instrument(LEAD, INSTR_SQUARE, 12, 0, 7, 140);
    instrument_duty(LEAD, 0.35f);
    instrument_filter(LEAD, FILTER_LOW, 3500, 2);
    instrument_mono(LEAD, 1);
    instrument_reverb(LEAD, 0.25f);
    instrument_vibrato(LEAD, 0.0f);
    // a little groove under it so the bass has something to sit on
    instrument(KICK, INSTR_MEMBRANE, 1, 0, 7, 160);
    instrument_harmonics(KICK, 0.3f); instrument_timbre(KICK, 0.1f); instrument_morph(KICK, 0.5f);
    instrument(HAT, INSTR_NOISE, 0, 30, 0, 20);
    instrument_filter(HAT, FILTER_HIGH, 7000, 2);
    instrument_level(HAT, 0.35f);
    // the formant's resonant peaks add gain on a bright saw: the first render peaked at -0.1 dBFS
    instrument_level(BASS, 0.34f);
    instrument_level(LEAD, 0.30f);
    instrument_level(KICK, 0.8f);
    reverb(0.4f, 0.5f);
    mb.sent = ml.sent = -1; mix_sent_b = mix_sent_l = -1;
}

static void new_take(void) { rnd_seed(seed); step16 = 0; lead_i = 0; t_step = t_now + 0.3f; t_lead = t_step; }

static void bass_note(int s, float t) {
    const BN *n = &BASSL[s % 32];
    float H = humanity;
    if (n->midi == 0) { if (bass_h) note_off(bass_h); bass_h = 0; return; }
    bass_vel = (n->mark == '>' ? 0.95f : n->mark == 'g' ? 0.45f : 0.72f);
    if (human) bass_vel += r1() * 0.06f * H;
    if (bass_h) note_off(bass_h);
    bass_h = note_on(n->midi, BASS, (int)(clamp(bass_vel, 0.1f, 1) * 7));
    bass_t0 = t;
    // the mouth's target for this note: harder = more open (A..E), softer = closed (U..O)
    mb.note_target = (human && layer[L_FOLLOW]) ? 0.15f + 0.55f * bass_vel : 0.4f;
    mb.t0 = t;
}

static void lead_note(int i, float t) {
    const LN *n = &LEADL[i];
    if (n->midi == 0) { if (lead_h) note_off(lead_h); lead_h = 0; return; }
    float H = humanity;
    lead_vel = human ? 0.75f + 0.12f * r1() * H : 0.75f;
    if (lead_h) note_off(lead_h);
    lead_h = note_on(n->midi, LEAD, (int)(lead_vel * 7));
    lead_t0 = t; lead_len = n->len * 60.0f / tempo / 4.0f; lead_say = n->say;
    ml.note_target = (human && layer[L_FOLLOW]) ? 0.2f + 0.45f * lead_vel : 0.45f;
    ml.t0 = t;
}

// one mouth's vowel right now: follow + wah + drift + talk, then smoothed
static float mouth_vowel(Mouth *m, float note_age, bool is_lead, float seed_off) {
    float H = humanity;
    float v = m->note_target;
    if (!human) return 0.45f;                                         // the FROZEN mouth
    if (layer[L_TALK] && is_lead && lead_h) {                         // talk: the long note says its vowel, moving into it
        float k = smooth01((note_age - 0.12f) / fmaxf(0.25f, lead_len * 0.5f));
        v = v + (lead_say - v) * k * fminf(1, H);
    }
    if (layer[L_WAH]) {                                               // wah: starts closed, opens over ~0.1 s
        float open = smooth01(note_age / (is_lead ? 0.16f : 0.09f));
        v = 0.03f + (v - 0.03f) * (1.0f - (1.0f - open) * fminf(1, H));
    }
    if (layer[L_DRIFT]) v += 0.06f * (noise(t_now * 0.35f + seed_off + seed) * 2 - 1) * H;
    v = clamp(v, 0, 1);
    m->cur += (v - m->cur) * 0.45f;                                   // the engine jumps: smooth here
    return m->cur;
}

void init(void) {
    setup_sound(); new_take();
#ifdef FROZEN        // DE_DEFINES=FROZEN: start on the frozen mouth (for stem A/B renders)
    human = false;
#endif
}

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; if (!playing) { if (bass_h) note_off(bass_h); if (lead_h) note_off(lead_h); bass_h = lead_h = 0; } t_step = t_lead = t_now + 0.2f; }
    if (keyp(KEY_TAB)) human = !human;
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) layer[i] = !layer[i];
    if (keyp(KEY_UP))   humanity = fminf(1.5f, humanity + 0.25f);
    if (keyp(KEY_DOWN)) humanity = fmaxf(0.0f, humanity - 0.25f);
    if (keyp('R')) { seed++; new_take(); }
    t_now += dt();
    if (!playing) return;
    float s16 = 60.0f / tempo / 4.0f;

    while (t_now >= t_step) {
        float swing = (human && (step16 & 1)) ? s16 * 0.12f * humanity : 0;
        bass_note(step16, t_now + swing);
        if ((step16 & 7) == 0) hit(36, KICK, 6, 120);
        if ((step16 & 1) == 0) hit(60, HAT, (step16 & 3) ? 3 : 5, 25);
        step16++;
        t_step += s16;
    }
    while (t_now >= t_lead) {
        lead_note(lead_i, t_now);
        t_lead += LEADL[lead_i].len * s16;
        lead_i = (lead_i + 1) % NLEAD;
    }
    if (lead_h && t_now - lead_t0 > lead_len * 0.92f) { note_off(lead_h); lead_h = 0; }

    // ---- ride the two mouths (re-send only when the vowel moved) ----
    float mixb = layer[L_MOUTH] ? 0.70f : 0.0f, mixl = layer[L_MOUTH] ? 0.85f : 0.0f;
    float vb = mouth_vowel(&mb, t_now - mb.t0, false, 0.0f);
    float vl = mouth_vowel(&ml, t_now - ml.t0, true, 40.0f);
    if (fabsf(vb - mb.sent) > 0.004f || mixb != mix_sent_b) { instrument_formant(BASS, vb, 0.45f, mixb); mb.sent = vb; mix_sent_b = mixb; }   // fx-lint-ignore: riding the vowel IS the spike
    if (fabsf(vl - ml.sent) > 0.004f || mixl != mix_sent_l) { instrument_formant(LEAD, vl, 0.65f, mixl); ml.sent = vl; mix_sent_l = mixl; }   // fx-lint-ignore: riding the vowel IS the spike
    shown_vb = vb; shown_vl = vl;
#ifdef DE_TRACE
    watch("vb", "%.3f", vb);
    watch("vl", "%.3f", vl);
#endif
}

static void draw_mouth(int cx, int cy, float v, const char *name, bool on) {
    // a mouth: open by the vowel's openness (A most open, U/I narrowest), wide on I/E, round on U/O
    float open = 1.0f - fabsf(v - 0.5f) * 1.6f; if (open < 0.15f) open = 0.15f;
    float wide = 0.6f + 0.6f * v;
    int w = (int)(26 * wide), h = (int)(22 * open);
    circfill(cx, cy, 30, CLR_DARKER_GREY);
    if (on) { rectfill(cx - w / 2, cy - h / 2, w, h, CLR_DARK_RED); rect(cx - w / 2, cy - h / 2, w, h, CLR_PINK); }
    else line(cx - 12, cy, cx + 12, cy, CLR_DARK_GREY);
    const char *VOW = "UOAEI";
    int vi = (int)(v * 4.0f + 0.5f); if (vi > 4) vi = 4;
    print(str("%s  %c", name, VOW[vi]), cx - 20, cy + 36, CLR_WHITE);
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print(human ? "MOVING MOUTH" : "FROZEN MOUTH", 4, 4, human ? CLR_GREEN : CLR_RED);
    print(str("%d bpm  take %d", tempo, seed), 140, 4, CLR_WHITE);
    draw_mouth(90, 80, shown_vb, "bass", layer[L_MOUTH]);
    draw_mouth(230, 80, shown_vl, "lead", layer[L_MOUTH]);
    print("vowel  U  O  A  E  I", 90, 140, CLR_LIGHT_GREY);
    int y2 = 176, lx = 4;
    for (int i = 0; i < NLAYER; i++) {
        int c = !human ? CLR_DARK_GREY : layer[i] ? CLR_GREEN : CLR_DARK_GREY;
        const char *t = str("%d %s", i + 1, LAYER_NAME[i]);
        print(t, lx, y2, c);
        lx += (int)strlen(t) * 8 + 6;
    }
    print(str("humanity %.2f  TAB  R  SPACE", humanity), 4, y2 + 12, CLR_LIGHT_GREY);
}
