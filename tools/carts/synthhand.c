/* de:meta
{
  "slug": "synthhand",
  "title": "synthhand",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "subtractive-synth",
    "analog-voice-modeling"
  ],
  "lineage": "The tenth humanized-playing spike (docs/design/humanized-playing.md), the first where the INSTRUMENT is imperfect and not only the player: a Prophet-style analog polysynth pad and a mono lead. Two halves: the player (keys, and a hand on the cutoff knob, the mod wheel and the pitch bend) and the machine (every voice slightly different like an Oberheim/Prophet voice card, oscillators drifting, the supply sagging on big chords). The voice cards are faked cart-side by rotating notes through eight slots with their own offsets: the evidence for or against an engine 'analog' amount.",
  "description": {
    "summary": "An analog polysynth that is ALIVE: a hand rides the filter and the mod wheel, chords land a little apart, and the machine itself is imperfect: every voice slightly different, the oscillators drifting, the power sagging when the chord is big. TAB flips to the perfect digital version.",
    "detail": "Pad and lead on a saw-and-ladder-filter voice. The player: chords spread a few ms, velocity varies, notes overlap into the next chord; the left hand sweeps the cutoff slowly and unevenly through a phrase (overshooting a little), brings the mod-wheel vibrato in late on long lead notes, bends into some notes, and glides on some. The machine: eight VOICE CARDS, each with its own small tuning, cutoff and envelope offsets, handed out in rotation so the same note twice sounds twice; every card drifts slowly in pitch (more while the synth is 'cold' at the start); a big chord sags the level and the filter a touch. Keys 1-5 switch each layer on and off.",
    "controls": "SPACE play/stop · TAB analog / digital · 1 keys 2 knobs 3 cards 4 drift 5 sag · UP/DOWN humanity · R new take"
  }
}
de:meta */
// synthhand — a spike: humanizing a SYNTH, the player and the machine.
//
// Pad: held notes on eight VOICE-CARD slots (10..17), one patch with per-card offsets, notes handed
// out round-robin. Lead: one mono slot (18), held, ridden every frame. Everything live (note_on +
// note_pitch / note_cutoff / note_vol / note_lfo), so the frame clock bounds the timing (fine for a
// pad; the booked twin is the §6 need). The digital version: one slot, no offsets, no rides.

#include "studio.h"
#include <math.h>
#include <string.h>

#define CARD0  10
#define NCARD  8
#define LEAD   18
#define NPV    16          // pad voices we track

enum { L_KEYS, L_KNOBS, L_CARDS, L_DRIFT, L_SAG, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "keys", "knobs", "cards", "drift", "sag" };

static bool  playing = true, human = true;
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 92;
static int   seed = 1;

// ---- the music (original): four chords a phrase, a lead over them ----
static const int CHORDS[8][4] = {
    { 53, 57, 60, 64 }, { 52, 55, 59, 62 }, { 50, 57, 60, 65 }, { 48, 55, 59, 64 },
    { 46, 53, 57, 62 }, { 45, 52, 55, 60 }, { 43, 50, 57, 59 }, { 48, 52, 55, 60 },
};
// lead: midi (0 = rest), length in eighths, '_' = legato (glide) from the previous
typedef struct { int midi; int len; char art; } LN;
static const LN LEADL[] = {
    {72,3,'t'},{74,1,'_'},{76,4,'t'},{79,2,'t'},{77,2,'_'},{76,4,'t'},
    {0,2,' '},{74,2,'t'},{72,2,'_'},{71,2,'t'},{72,8,'t'},{0,4,' '},
    {76,2,'t'},{77,2,'_'},{79,6,'t'},{81,2,'_'},{79,4,'t'},{76,4,'_'},
    {74,2,'t'},{72,2,'_'},{74,4,'t'},{67,8,'t'},{0,4,' '},
};
#define NLEAD ((int)(sizeof LEADL / sizeof LEADL[0]))

// ---- the machine: per-card offsets ----
static float card_cents[NCARD], card_cut[NCARD], card_env[NCARD], card_lvl[NCARD], card_ph[NCARD];
static int   next_card;

// ---- pad voices ----
typedef struct { int h, card, midi; float vel; float t_on, t_rel; } PV;   // t_rel > 0: let go at that time
static PV   pv[NPV];
static float t_now, t_chord, t_lead;
static int   chord_i, lead_i;
static int   lead_h = 0;
static float lead_t0, lead_end, lead_vel, lead_bend0, lead_vib;
static int   lead_midi;

// the hand on the knob: the cutoff swept slowly through a phrase
static float knob_cut = 900, knob_target = 900, knob_vel = 0;
static float shown_cut, shown_sag;

static float r1(void) { return rnd_float() * 2.0f - 1.0f; }
static float smooth01(float x) { x = clamp(x, 0, 1); return x * x * (3 - 2 * x); }

static void patch(int slot, int a, int d, int s7, int r, float detune) {
    instrument(slot, INSTR_SAW, a, d, s7, r);
    instrument_unison(slot, 2, detune);                   // two oscillators a voice, like a Prophet
    instrument_filter(slot, FILTER_LADDER, 900, 4);
    instrument_reverb(slot, 0.25f);
}

static void setup_sound(void) {
    rnd_seed(seed * 31 + 7);
    for (int c = 0; c < NCARD; c++) {
        // a VOICE CARD: its own small offsets, fixed for this instrument (seeded)
        card_cents[c] = r1() * 7.0f;                      // tuning: a few cents off
        card_cut[c]   = 1.0f + r1() * 0.10f;              // cutoff ±10%
        card_env[c]   = 1.0f + r1() * 0.15f;              // envelope speed ±15%
        card_lvl[c]   = 1.0f + r1() * 0.08f;              // level ±0.7 dB
        card_ph[c]    = rnd_float() * 100.0f;             // where its drift starts
        int cs = CARD0 + c;
        patch(cs, (int)(380 * card_env[c]), (int)(900 * card_env[c]), 5, (int)(1300 * card_env[c]), 0.08f + 0.04f * rnd_float());
        instrument_pan(cs, (c - 3.5f) * 0.08f);
    }
    patch(LEAD, 8, 300, 6, 180, 0.06f);
    instrument_mono(LEAD, 1);
    instrument_filter(LEAD, FILTER_LADDER, 1800, 6);
    instrument_reverb(LEAD, 0.2f);
    reverb(0.55f, 0.5f);
}

static void new_take(void) {
    rnd_seed(seed);
    for (int i = 0; i < NPV; i++) pv[i].h = 0;
    chord_i = 0; lead_i = 0; next_card = 0;
    t_chord = t_now + 0.3f; t_lead = t_chord;
    knob_cut = knob_target = 900;
}

static void release_old(float keep_after) {
    for (int i = 0; i < NPV; i++) if (pv[i].h && pv[i].t_on < keep_after) { note_off(pv[i].h); pv[i].h = 0; }
}


static int free_pv(void) { for (int i = 0; i < NPV; i++) if (!pv[i].h) return i; return 0; }

static void start_chord(int ci) {
    float H = humanity;
    bool K = human && layer[L_KEYS], C = human && layer[L_CARDS];
    float chord_t = t_now;
    // the keys: the old chord lets go a moment AFTER the new one lands (legato), or at once
    for (int k = 0; k < 4; k++) {
        int m = CHORDS[ci][k];
        int card = C ? next_card : 0;                     // round-robin through the cards
        if (C) next_card = (next_card + 1) % NCARD;
        int slot = (human && C) ? CARD0 + card : CARD0;   // digital: one slot, every voice the same
        float vel = K ? 0.70f + 0.15f * r1() * H + (k == 3 ? 0.08f : 0) : 0.75f;
        int i = free_pv();
        pv[i].h = note_on(m, slot, (int)(vel * 7.0f));
        pv[i].card = C ? card : -1; pv[i].midi = m; pv[i].vel = vel;
        // note_on cannot start inside a frame, so the KEYS layer spreads the chord by each voice's
        // loudness arriving at its own moment instead: lower notes land first
        pv[i].t_on = chord_t + (K ? (0.004f + 0.006f * k) * H + 0.003f * rnd_float() : 0);
        pv[i].t_rel = 0;
    }
}

static void lead_start(int li) {
    const LN *n = &LEADL[li];
    float H = humanity;
    bool K = human && layer[L_KEYS], N = human && layer[L_KNOBS];
    if (n->midi == 0) { if (lead_h) note_off(lead_h); lead_h = 0; return; }
    lead_vel = K ? 0.72f + 0.12f * r1() * H : 0.75f;
    bool legato = n->art == '_' && lead_h;
    if (legato && (human ? (!K || rnd_float() < 0.8f) : true)) {
        note_glide(lead_h, (human && N) ? 40 + rnd(50) : 0);
        note_pitch(lead_h, (float)n->midi);
    } else {
        if (lead_h) note_off(lead_h);
        lead_h = note_on(n->midi, LEAD, (int)(lead_vel * 7));
        note_glide(lead_h, 14);
    }
    // the bend: some notes are bent into from a semitone or a whole tone under
    lead_bend0 = (N && !legato && rnd_float() < 0.3f * H) ? -(rnd_float() < 0.5f ? 1.0f : 2.0f) : 0;
    if (lead_bend0 != 0) note_pitch(lead_h, n->midi + lead_bend0);
    lead_t0 = t_now; lead_midi = n->midi;
    float e8 = 60.0f / tempo / 2.0f;
    lead_end = t_now + n->len * e8 * (K ? 0.85f + 0.12f * rnd_float() : 0.9f);
    lead_vib = (N && n->len >= 4) ? 0.14f + 0.06f * rnd_float() : 0;
}

void init(void) {
    setup_sound(); new_take();
#ifdef LAYERS_OFF    // DE_DEFINES=LAYERS_OFF=<bitmask>: start with layers off (bit 0 = keys .. bit 4 = sag), for A/B renders
    for (int i = 0; i < NLAYER; i++) if (LAYERS_OFF & (1 << i)) layer[i] = false;
#endif
}

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; if (!playing) { release_old(1e9); if (lead_h) note_off(lead_h); lead_h = 0; } t_chord = t_lead = t_now + 0.2f; }
    if (keyp(KEY_TAB)) { human = !human; release_old(1e9); }
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) layer[i] = !layer[i];
    if (keyp(KEY_UP))   humanity = fminf(1.5f, humanity + 0.25f);
    if (keyp(KEY_DOWN)) humanity = fmaxf(0.0f, humanity - 0.25f);
    if (keyp('R')) { seed++; release_old(1e9); setup_sound(); new_take(); }
    t_now += dt();
    if (!playing) return;
    float H = humanity;
    float bar = 4 * 60.0f / tempo;

    // ---- the score ----
    if (t_now >= t_chord) {
        // the keys: the old chord lets go a moment AFTER the new one lands (legato), the grid at once
        float hold = (human && layer[L_KEYS]) ? 0.05f + 0.07f * rnd_float() : 0;
        for (int i = 0; i < NPV; i++) if (pv[i].h && pv[i].t_rel == 0) pv[i].t_rel = t_now + hold + 0.0001f;
        start_chord(chord_i);
        chord_i = (chord_i + 1) % 8;
        t_chord += bar;
    }
    for (int i = 0; i < NPV; i++) if (pv[i].h && pv[i].t_rel > 0 && t_now >= pv[i].t_rel) { note_off(pv[i].h); pv[i].h = 0; }
    if (t_now >= t_lead) {
        float e8 = 60.0f / tempo / 2.0f;
        lead_start(lead_i);
        t_lead += LEADL[lead_i].len * e8 * ((human && layer[L_KEYS]) ? 1.0f + 0.03f * r1() * H : 1.0f);
        lead_i = (lead_i + 1) % NLEAD;
    }
    if (lead_h && t_now >= lead_end && !(lead_i < NLEAD && LEADL[lead_i].art == '_')) { note_off(lead_h); lead_h = 0; }

    // ---- the hand on the cutoff knob: a slow uneven sweep through the phrase, overshooting ----
    float phase = fmodf(t_now / (bar * 4), 1.0f);
    if (human && layer[L_KNOBS]) {
        knob_target = 500 + 1900 * smooth01(sinf(3.14159f * phase)) + 250 * (noise(t_now * 0.4f + seed) * 2 - 1) * H;
        float k = 6.0f, d = 3.2f;                         // a hand: springy, a little overshoot
        knob_vel += (knob_target - knob_cut) * k * dt() - knob_vel * d * dt();
        knob_cut += knob_vel * dt();
    } else knob_cut = 1100;

    // ---- the machine: power sag on big chords ----
    int sounding = 0; for (int i = 0; i < NPV; i++) if (pv[i].h) sounding++;
    float sag = (human && layer[L_SAG]) ? fminf(1.0f, sounding / 10.0f) * 0.12f * H : 0;
    float warm = (human && layer[L_DRIFT]) ? 1.0f + 2.0f * expf(-t_now / 20.0f) : 1.0f;   // cold synth drifts more

    for (int i = 0; i < NPV; i++) {
        if (!pv[i].h) continue;
        float cents = 0, cut = knob_cut, lvl = pv[i].vel * 7.0f;
        int c = pv[i].card;
        if (c >= 0) {
            cents += card_cents[c] * H;
            cut *= 1.0f + (card_cut[c] - 1.0f) * H;
            lvl *= 1.0f + (card_lvl[c] - 1.0f) * H;
            if (layer[L_DRIFT]) cents += 4.0f * warm * (noise(t_now * 0.15f + card_ph[c]) * 2 - 1) * H;
        }
        // the chord spread (KEYS): each voice's level arrives at its own moment
        if (human && layer[L_KEYS]) lvl *= smooth01((t_now - pv[i].t_on) / 0.012f + 0.4f);
        lvl *= 1.0f - sag; cut *= 1.0f - sag * 0.8f;
        note_pitch(pv[i].h, pv[i].midi + cents / 100.0f);
        note_cutoff(pv[i].h, (int)cut);
        note_vol(pv[i].h, clamp(lvl, 0, 7));
    }
    if (lead_h) {
        float a = t_now - lead_t0;
        float p = (float)lead_midi;
        if (lead_bend0 != 0) p += lead_bend0 * expf(-a / 0.07f);
        if (human && layer[L_DRIFT]) p += 0.03f * warm * (noise(t_now * 0.2f + 50) * 2 - 1) * H;
        note_pitch(lead_h, p);
        note_cutoff(lead_h, (int)(knob_cut * 1.8f));
        // the mod wheel: vibrato pushed in late on long notes
        note_lfo(lead_h, 0, LFO_PITCH, 5.6f, lead_vib * smooth01((a - 0.35f) / 0.5f));
    }
    shown_cut = knob_cut; shown_sag = sag;
#ifdef DE_TRACE
    watch("cut", "%.0f", knob_cut);
    watch("voices", "%d", sounding);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print(human ? "ANALOG" : "DIGITAL", 4, 4, human ? CLR_GREEN : CLR_RED);
    print(str("%d bpm  take %d", tempo, seed), 120, 4, CLR_WHITE);
    // the eight voice cards: lit when sounding, their offsets as little marks
    for (int c = 0; c < NCARD; c++) {
        int x = 12 + c * 37, y = 30;
        bool on = false; for (int i = 0; i < NPV; i++) if (pv[i].h && pv[i].card == c) on = true;
        rectfill(x, y, 30, 50, on ? CLR_DARK_GREEN : CLR_DARKER_GREY);
        rect(x, y, 30, 50, CLR_DARK_GREY);
        print(str("%d", c + 1), x + 12, y + 3, CLR_WHITE);
        int ty = y + 25 - (int)(card_cents[c] * 2);
        line(x + 4, ty, x + 10, ty, CLR_YELLOW);                      // tuning
        int cy = y + 25 - (int)((card_cut[c] - 1) * 100);
        line(x + 13, cy, x + 19, cy, CLR_ORANGE);                     // cutoff
        int ey = y + 25 - (int)((card_env[c] - 1) * 60);
        line(x + 22, ey, x + 27, ey, CLR_PINK);                       // envelope
    }
    print("voice cards: tune / cutoff / env", 12, 84, CLR_LIGHT_GREY);
    // the cutoff knob
    int kx = 60, ky = 125; float ang = -225 + 270 * clamp((shown_cut - 300) / 2400, 0, 1);
    circ(kx, ky, 18, CLR_LIGHT_GREY);
    line(kx, ky, kx + (int)(cos_deg(ang) * 16), ky + (int)(sin_deg(ang) * 16), CLR_YELLOW);
    print(str("cutoff %4.0f", shown_cut), 90, 120, CLR_WHITE);
    if (shown_sag > 0.02f) print(str("sag %.0f%%", shown_sag * 100), 200, 120, CLR_ORANGE);
    int y2 = 176, lx = 4;
    for (int i = 0; i < NLAYER; i++) {
        int c = !human ? CLR_DARK_GREY : layer[i] ? CLR_GREEN : CLR_DARK_GREY;
        const char *t = str("%d %s", i + 1, LAYER_NAME[i]);
        print(t, lx, y2, c);
        lx += (int)strlen(t) * 8 + 6;
    }
    print(str("humanity %.2f  TAB  R  SPACE", humanity), 4, y2 + 12, CLR_LIGHT_GREY);
}
