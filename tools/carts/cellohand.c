/* de:meta
{
  "slug": "cellohand",
  "title": "cellohand",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "waveguide-synth",
    "analog-voice-modeling",
    "self-oscillation"
  ],
  "lineage": "The seventh humanized-playing spike (docs/design/humanized-playing.md §7.1), after strumhand, breathflute, tablahand, tablalab, reedhand and basshand: an arco cello, INSTR_BOWED with the cello body. breathflute's held-and-ridden mechanics, with a BOW model in place of the lungs. Asks the question the earlier spikes did not: an EVENT inside a held note (the bow change), which re-articulates without a new note.",
  "description": {
    "summary": "A slow cello line played by a bow arm instead of a keyboard: the bow has a length and runs out, so long notes change bow mid-note without a new attack, down-bows lean in and up-bows grow, bow speed and pressure shape each note's level and colour, the left hand slides on the big shifts and lets the vibrato bloom late. TAB flips to the keyboard version.",
    "detail": "The player keeps track of where the bow is (frog to tip) and which way it travels. Every new stroke turns it round, the player plans each phrase so its accent or longest note lands on a down-bow, a long note gets a full bow, and when a note outlasts the bow it changes direction mid-note: a short dip, a little bite of rosin, the pitch and the vibrato carrying straight through. Down-bows start heavy at the frog and lighten towards the tip, up-bows start light and grow. Bow speed and pressure are curves over each note (a swell on the long ones, an accent that digs in and lets go), and a stroke that must last longer than the bow allows is played slower with more weight. Some notes slide into place, more on big leaps. The time breathes: pushing in a phrase, broadening at its end, a little time taken before a shift. Keys 1-5 switch each layer on and off.",
    "controls": "SPACE play/stop · TAB bow arm / keyboard · 1 time 2 bow 3 changes 4 shifts 5 vib · UP/DOWN humanity · LEFT/RIGHT tempo · R new take"
  }
}
de:meta */
// cellohand — a spike: an arco cello whose notes come from a modeled BOW ARM.
//
// Held notes ridden every frame (breathflute's mechanics): note_vol, note_timbre (bow pressure),
// note_morph (bow speed), note_pitch + note_glide (fingers and shifts), note_lfo (our vibrato).
// INSTR_BOWED macros: harmonics = bow position (read at NOTE-ON only: it sets where the string is
// split), timbre = bow PRESSURE and morph = bow SPEED (both read every sample, slewed ~11 ms, so they
// move live on a held note: sound.h sound_bowed_sample + SR_NOTE_MACRO + the SLEW_MACRO slew).
//
// THE QUESTION THIS SPIKE ASKS: a BOW CHANGE is an event INSIDE a held note. The bow reverses, the
// string's motion catches again, there is a little dip and a scratch, but no new note and no pitch
// restart. Probes (DE_DEFINES=PROBE_BC=<method>) render one long D3 with two bow changes per method,
// for measuring; the cart then plays with BC_METHOD.

#include "studio.h"
#include <math.h>
#include <string.h>

#define CELLO   10
#define SCRATCH 11     // the rosin catch at a bow change: a short band-passed noise tick

// bow-change methods, measured by the probe (see the PROBE_BC block in init/update)
enum { BC_NONE, BC_VOL, BC_RIDE, BC_RIDE_SCRATCH, BC_RETRIG, BC_XFADE, BC_REATTACK, BC_NMETHOD };
#ifndef BC_METHOD
#define BC_METHOD BC_RIDE_SCRATCH
#endif
#ifndef SCRATCH_VOL
#define SCRATCH_VOL 2
#endif

// ---- the line (original), D minor, lengths in eighths ----
// art: 't' a new stroke with an attack (a fresh bow), '_' slurred (same bow, a finger change),
//      'c' connected (the bow changes direction ON the sounding string: no new attack)
// mark: '>' accent (digs in, lets go)
typedef struct { int midi; int len; char art; char mark; int end; } N;
static const N TUNE[] = {
    {50,2,'t',' ',0},{53,2,'_',' ',0},{57,4,'c',' ',0},{55,1,'_',' ',0},{53,1,'_',' ',0},
        {52,2,'c',' ',0},{53,2,'_',' ',0},{50,6,'c',' ',1},
    {45,2,'t',' ',0},{50,2,'_',' ',0},{53,2,'c',' ',0},{53,2,'c',' ',0},{55,2,'_',' ',0},
        {57,2,'c',' ',0},{62,10,'c','>',0},{60,1,'_',' ',0},{58,1,'_',' ',0},{57,4,'c',' ',0},
        {55,2,'_',' ',0},{57,6,'c',' ',1},
    {58,3,'t','>',0},{57,1,'_',' ',0},{55,2,'c',' ',0},{53,2,'_',' ',0},{52,2,'c',' ',0},
        {49,2,'_',' ',0},{50,4,'c',' ',0},{50,4,'c',' ',0},{45,2,'c',' ',0},{46,2,'_',' ',0},
        {49,2,'_',' ',0},{50,14,'c',' ',1},
};
#define NT ((int)(sizeof TUNE / sizeof TUNE[0]))

enum { L_TIME, L_BOW, L_CHANGE, L_SHIFT, L_VIB, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "time", "bow", "changes", "shifts", "vib" };

static bool  playing = true;
static bool human = true;
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 66;
static int   seed = 1;

// ---- clock + score position ----
static float t_now, t_next;
static int   idx;

// ---- the bow arm ----
static float bow_pos = 0.05f;   // 0 = frog .. 1 = tip
static int   bow_dir = 1;       // +1 down-bow (frog -> tip), -1 up-bow
static float bow_speed;         // planned, bow-lengths per second
static float bow_want;          // the speed the dynamics asked for (economy shows as the gap)
static float stroke_left;       // seconds left in the current stroke (re-planned on a change)
static float stroke_t0;         // when this stroke began (the attack curves count from here)
static int   n_changes, n_midchanges;

// the bow change event in flight
static float bc_t0 = -10, bc_depth;
static int   bc_kind;           // which method fired it

// ---- the sounding note ----
static int   h = 0, h_fade = 0;
static float fade_t0;
static float n_start, n_len, n_dyn, n_midi_f;
static float shift_t0 = -10, shift_len;   // an audible shift: the bow lightens while the finger slides
static int   n_i;
static float vib_delay, vib_depth, vib_rate;
static float last_vol = -1, last_timb = -1, last_mor = -1, last_vd = -1, last_vr = -1;
static float shown_vol, shown_pr, shown_sp, shown_cents;

// trace (pitch + level), like breathflute
#define TR 300
static float tr_pitch[TR], tr_vol[TR];
static unsigned char tr_bc[TR];
static int   tr_pos;

static float r1(void) { return rnd_float() * 2.0f - 1.0f; }
static float smooth01(float x) { x = clamp(x, 0, 1); return x * x * (3 - 2 * x); }
static float drift(float x, float k) { return noise(x * k + seed * 7.7f) * 2.0f - 1.0f; }

static void setup_sound(void) {
    // the cello preset of bowed.c (h0.55 t0.42 m0.55) on the cello-sized body
    instrument(CELLO, INSTR_BOWED, 60, 0, 7, 240);
    instrument_mode(CELLO, MODE_BOW_PIZZ, 0.0f);
    instrument_mode(CELLO, MODE_BOW_BODY, 0.85f);
    instrument_mode(CELLO, MODE_BOW_SIZE, BOW_SIZE_CELLO);
    instrument_harmonics(CELLO, 0.55f);
    instrument_timbre(CELLO, 0.42f);
    instrument_morph(CELLO, 0.55f);
    instrument_filter(CELLO, FILTER_LOW, 4200, 0);
    instrument_reverb(CELLO, 0.32f);
    // the rosin catch: a tick of band-passed noise, quiet, under the string
    instrument(SCRATCH, INSTR_NOISE, 1, 28, 0, 12);
    instrument_filter(SCRATCH, FILTER_BAND, 2600, 2);
    instrument_reverb(SCRATCH, 0.2f);
    reverb(0.6f, 0.5f);
}

static void phrase_of(int i, float *pos, int *lo, int *hi, int *peak) {
    int a = i; while (a > 0 && !TUNE[a - 1].end) a--;
    int b = i; while (b < NT - 1 && !TUNE[b].end) b++;
    *pos = (b > a) ? (i - a) / (float)(b - a) : 0;
    *lo = a; *hi = b;
    int pk = 0; for (int k = a; k <= b; k++) if (TUNE[k].midi > pk) pk = TUNE[k].midi;
    *peak = pk;
}

static float eighth_s(void) { return 60.0f / tempo / 2.0f; }

// a stroke = a 't' or 'c' note plus the slurred notes after it, in nominal seconds
static float stroke_len(int i) {
    float s = TUNE[i].len;
    for (int k = i + 1; k < NT && TUNE[k].art == '_'; k++) s += TUNE[k].len;
    return s * eighth_s();
}

// PLANNING: at a phrase's start, pick the bow direction so its most important stroke (an accent,
// else the longest) lands on a down-bow. Each stroke turns the bow round, so it is a parity count.
static int plan_phrase_dir(int lo, int hi) {
    int best = lo, nstrokes = 0, best_n = 0; float best_score = -1;
    for (int k = lo; k <= hi; k++) {
        if (TUNE[k].art == '_') continue;
        float sc = stroke_len(k) + (TUNE[k].mark == '>' ? 10.0f : 0.0f);
        if (sc > best_score) { best_score = sc; best = k; best_n = nstrokes; }
        nstrokes++;
    }
    (void)best;
    return (best_n % 2 == 0) ? 1 : -1;          // even count from the start = same direction
}

// the stroke after the one starting at note i (-1 if the phrase ends first: the bow lifts, no need)
static int next_stroke(int i) {
    int k = i;
    while (k < NT - 1 && TUNE[k + 1].art == '_') { if (TUNE[k].end) return -1; k++; }
    if (TUNE[k].end || k + 1 >= NT || TUNE[k + 1].art == 't') return -1;
    return k + 1;
}

// how much bow the player gives this stroke: the dynamics want a speed, a long stroke wants a full
// bow, and LOOKING AHEAD, it must end where the next stroke (going back the other way) has the bow
// it will want. If even the slowest bow (leaning in) will not last, the bow changes mid-note.
static void plan_speed(float dur, float dyn, int next_i) {
    float avail = bow_dir > 0 ? 1.0f - bow_pos : bow_pos;
    bow_want = 0.18f + 0.55f * dyn;                              // bow-lengths per second
    float use = bow_want * dur;
    if (dur > 1.2f) use = fmaxf(use, 0.9f * avail);              // a long note wants a full bow
    if (next_i >= 0 && human && layer[L_BOW]) {
        float need = fminf(0.92f, (0.18f + 0.55f * 0.5f) * stroke_len(next_i));
        use = fmaxf(use, need - (1.0f - avail));                 // travel far enough to hand it over
    }
    use = fminf(use, 0.94f * avail);                             // economy: save bow, lean in
    float floor_sp = 0.10f + 0.25f * dyn;                        // a loud note cannot be played on a crawling bow
    bow_speed = clamp(use / fmaxf(dur, 0.05f), floor_sp, 1.1f);
    stroke_left = dur;
}

// the event: the bow turns round on the sounding string
static void bow_change(int kind, int midi_now) {
    bc_t0 = t_now; bc_kind = kind;
    bc_depth = human ? (0.20f + 0.15f * rnd_float()) * fminf(1.0f, humanity) : 0.3f;
    n_changes++;
    bow_dir = -bow_dir;
    stroke_t0 = t_now;
    if (kind == BC_RIDE_SCRATCH) {
        NoteOpts o = { .gain = clamp(0.25f + 0.5f * bc_depth, 0.04f, 1.0f) };
        schedule_note_at(audio_time() + 0.025, 84, SCRATCH, SCRATCH_VOL, 25, &o);
    } else if (kind == BC_RETRIG) {
        note_retrig(h);
    } else if (kind == BC_XFADE) {
        if (h_fade) note_off(h_fade);
        h_fade = h; fade_t0 = t_now;
        h = note_on(midi_now, CELLO, 0);
        note_glide(h, 0);
        last_vol = last_timb = last_mor = last_vd = last_vr = -999;
    } else if (kind == BC_REATTACK) {
        note_off(h);
        h = note_on(midi_now, CELLO, 5);
        last_vol = last_timb = last_mor = last_vd = last_vr = -999;
    }
}

// what the event does to the held voice this frame: level x, speed +, pressure +
static void bc_shape(float *gmul, float *mor_add, float *pr_add) {
    *gmul = 1; *mor_add = 0; *pr_add = 0;
    float a = t_now - bc_t0;
    if (a < 0 || a > 0.16f) return;
    int k = bc_kind;
    if (k == BC_VOL || k == BC_RIDE || k == BC_RIDE_SCRATCH) {
        // the bow slows to a stop and reverses: level dips over ~30 ms, recovers over ~90 ms
        float dip = a < 0.03f ? smooth01(a / 0.03f) : 1.0f - smooth01((a - 0.03f) / 0.09f);
        *gmul = 1.0f - bc_depth * dip;
        if (k != BC_VOL) {
            *mor_add = -0.20f * dip;                                  // bow speed through (near) zero
            if (a > 0.025f) *pr_add = 0.30f * bc_depth * expf(-(a - 0.025f) / 0.035f);   // the bite
        }
    } else if (k == BC_XFADE) {
        *gmul = smooth01(a / 0.06f);                                  // the new voice fades in
    }
}

static void release_all(void) {
    if (h) note_off(h);
    if (h_fade) note_off(h_fade);
    h = h_fade = 0;
}

// ---------------------------------------------------------------------------------------------
static void start_note(int i) {
    const N *n = &TUNE[i];
    float H = humanity, e8 = eighth_s();
    float pos; int lo, hi, peak; phrase_of(i, &pos, &lo, &hi, &peak);
    bool T = human && layer[L_TIME], B = human && layer[L_BOW], C = human && layer[L_CHANGE];
    bool S = human && layer[L_SHIFT], V = human && layer[L_VIB];
    int prev = (i > 0) ? TUNE[i - 1].midi : n->midi;
    int leap = n->midi - prev;

    // TIME: push in the phrase, broaden into its end, linger on a long note's arrival
    float dur = n->len * e8;
    if (T) {
        float push = 1.0f - 0.06f * sinf(3.14159f * fminf(pos * 1.25f, 1.0f)) * H;
        float broad = 1.0f + 0.20f * smooth01((pos - 0.75f) / 0.25f) * H;
        dur *= push * broad * (1.0f + r1() * 0.03f * H + drift(t_now, 0.3f) * 0.02f * H);
        if (n->len >= 6) dur *= 1.0f + 0.04f * H;
    }

    // DYNAMICS: towards the phrase's peak, accent up, drift
    float dyn = 0.5f;
    if (B) {
        float height = 1.0f - fminf(1.0f, (peak - n->midi) / 12.0f);
        dyn = 0.30f + 0.45f * height * H + (n->mark == '>' ? 0.15f : 0) + drift(t_now, 0.2f) * 0.06f * H;
        dyn = clamp(dyn, 0.15f, 1.0f);
    }

    bool phrase_start = (i == 0 || TUNE[i - 1].end);
    bool same_voice = h && n->art != 't';
    // an AUDIBLE shift is an expressive choice, not every leap (Nikki: "the cello sounds a bit more
    // synth with the way it glides"): mostly on bigger leaps, about half the time
    bool shift = S && same_voice && abs(leap) >= 4 && rnd_float() < 0.55f * fmaxf(H, 0.3f);

    // VIBRATO plan: only on notes with time for it, blooming late, wider on long notes
    vib_depth = 0; vib_delay = 0; vib_rate = 5.4f;
    if (V && dur > 0.45f) {
        vib_delay = (0.22f + 0.18f * rnd_float()) * (dur > 2.0f ? 1.4f : 1.0f);
        vib_depth = (0.10f + 0.05f * rnd_float() + fminf(0.10f, dur * 0.025f)) * H;
        vib_rate = 5.0f + 0.7f * rnd_float();
    }
    instrument_vibrato(CELLO, V ? 0.0f : 1.0f);

    if (!human) {
        // the KEYBOARD: every note a fresh attack at one level, the engine's own vibrato
        release_all();
        h = note_on(n->midi, CELLO, 5);
        last_vol = last_timb = last_mor = last_vd = last_vr = -999;
        n_start = t_now; n_len = dur * 0.92f; n_i = i; n_dyn = 0.5f; n_midi_f = n->midi;
        t_next = t_now + dur;
        return;
    }

    if (n->art == 't' || !h) {
        // a new stroke with an attack: at a phrase start the player sets the bow (planned direction)
        release_all();
        if (phrase_start && B) {
            bow_dir = plan_phrase_dir(lo, hi);
            bow_pos = bow_dir > 0 ? 0.04f + 0.04f * rnd_float() : 0.96f - 0.04f * rnd_float();
        } else bow_dir = -bow_dir;
        h = note_on(n->midi, CELLO, 0);
        note_glide(h, 0);
        last_vol = last_timb = last_mor = last_vd = last_vr = -999;
        stroke_t0 = t_now;
        plan_speed(stroke_len(i) * dur / (n->len * e8), dyn, next_stroke(i));
    } else if (n->art == 'c') {
        // connected: the bow turns on the sounding string (or, with the layer off, a fresh attack)
        if (C) bow_change(BC_METHOD, prev);
        else { release_all(); h = note_on(n->midi, CELLO, 0); bow_dir = -bow_dir; stroke_t0 = t_now;
               last_vol = last_timb = last_mor = last_vd = last_vr = -999; }
        plan_speed(stroke_len(i) * dur / (n->len * e8), dyn, next_stroke(i));
    }
    // the left hand: a finger LANDING is a step (no glide: a finger in position does not slide);
    // a shift is QUICK (40-90 ms, a little longer on bigger leaps) and the bow lightens while the
    // finger travels, so the slide is a colour under the arrival, not a portamento. Was: every
    // slurred note 14-24 ms of glide and every leap of 3+ a constant-speed 70-200 ms slide.
    if (same_voice && n->midi != prev) {
        int ms = shift ? (int)((40 + 5 * abs(leap)) * (0.8f + 0.4f * rnd_float())) : 0;
        if (ms > 90) ms = 90;
        note_glide(h, ms);
        note_pitch(h, (float)n->midi);
        if (shift) { shift_t0 = t_now; shift_len = ms / 1000.0f; }
        if (shift && T) dur += 0.03f * H;                         // a shift takes a little time
    } else if (h && n->art != 't') note_pitch(h, (float)n->midi);

    n_start = t_now; n_len = dur; n_i = i; n_dyn = dyn; n_midi_f = n->midi;
    t_next = t_now + dur;
    if (n->end) t_next += (B ? 0.35f + 0.1f * rnd_float() : 0) + e8;   // the bow lifts, a breath
}

// every frame: the bow travels, the curves shape the note
static void ride(void) {
    float H = humanity;
    if (h_fade && t_now - fade_t0 > 0.06f) { note_off(h_fade); h_fade = 0; }
    else if (h_fade) note_vol(h_fade, fmaxf(0, shown_vol * (1.0f - (t_now - fade_t0) / 0.06f)));
    if (!h) return;
    float age = t_now - n_start, p = clamp(age / fmaxf(n_len, 0.01f), 0, 1);
    const N *n = &TUNE[n_i];
    bool B = human && layer[L_BOW], C = human && layer[L_CHANGE];

    // the note ends: phrase-final notes lift, the rest hand over to the next note
    if (t_now >= n_start + n_len) {
        if (!human || n->end) { note_off(h); h = 0; }
        return;
    }
    if (!human) { if (last_vol < 0) { note_vol(h, 5); last_vol = 5; } return; }

    // the bow moves; when it runs out inside a note, change mid-note (or, layer off, it never ends)
    float sp = bow_speed;
    float note_curve = 1.0f;
    if (B && n->len >= 4) note_curve = 0.82f + 0.32f * sinf(3.14159f * powf(p, 0.85f)) * H;   // messa di voce
    if (B && n->end) note_curve *= 1.0f - 0.45f * smooth01((p - 0.55f) / 0.45f) * H;         // the last note fades
    sp *= note_curve;
    bow_pos += bow_dir * sp * dt();
    stroke_left -= dt();
    if (bow_pos > 0.985f || bow_pos < 0.015f) {
        bow_pos = clamp(bow_pos, 0.015f, 0.985f);
        if (C && stroke_left < 0.4f) {
            bow_speed = 0.10f;          // nearly at the end of the stroke anyway: squeeze the last bit, no change
        } else if (C) {
            n_midchanges++;
            bow_change(BC_METHOD, n->midi);
            float rest = fmaxf(0.2f, n_start + n_len - t_now);
            for (int k = n_i + 1; k < NT && TUNE[k].art == '_'; k++) rest += TUNE[k].len * eighth_s();
            plan_speed(rest, n_dyn, next_stroke(n_i));
        }
    }

    // PRESSURE: the dynamics, the arm's weight (heavy at the frog, light at the tip), the attack
    // (a down-bow digs in, an up-bow starts light), an accent, and economy (slow bow = more weight)
    float sa = t_now - stroke_t0;
    float pr = 0.40f, gm = 1.0f;
    if (B) {
        pr = 0.28f + 0.22f * n_dyn + 0.14f * (0.5f - bow_pos) * H;
        if (sa < 0.4f) {
            if (bow_dir > 0) pr += 0.16f * H * expf(-sa / 0.07f);
            else gm *= 0.86f + 0.14f * smooth01(sa / 0.3f);       // an up-bow grows into the note
        }
        if (n->mark == '>' && age < 0.5f) { pr += 0.22f * H * expf(-age / 0.12f); gm *= 1.0f + 0.25f * H * expf(-age / 0.15f); }
        if (bow_speed < bow_want) pr += (bow_want - bow_speed) * 0.5f * H;    // slow bow: more weight
        else pr -= (bow_speed - bow_want) * 0.25f * H;                       // a travelling bow: lighter
    }
    float gmul, mor_add, pr_add;
    bc_shape(&gmul, &mor_add, &pr_add);
    pr += pr_add;

    // SPEED -> engine morph and level; pressure -> engine timbre (kept in the clean wedge mostly)
    float spn = clamp((sp - 0.10f) / 0.6f, 0, 1);
    float mor = B ? clamp(0.30f + 0.50f * spn + mor_add, 0, 1) : clamp(0.55f + mor_add, 0, 1);
    float timb = clamp(B ? pr : 0.42f + pr_add, 0.10f, 0.75f);
    float loud = B ? clamp(0.22f + 0.25f * spn + 0.62f * n_dyn * note_curve, 0, 1.2f) : 0.6f;
    float vol = (0.8f + 5.6f * loud) * gm * gmul;
    { float sa = t_now - shift_t0;                                    // the bow lightens during a shift
      if (sa >= 0 && sa < shift_len + 0.03f) vol *= 1.0f - 0.35f * sinf(3.14159f * clamp(sa / (shift_len + 0.03f), 0, 1)); }
    if (n->end && !B) vol *= 1.0f;

    // VIBRATO: blooms after its delay; the rate wanders; it carries straight through a bow change
    float vd = 0, vr = vib_rate;
    if (vib_depth > 0) {
        vd = vib_depth * smooth01((age - vib_delay) / 0.5f);
        vr += 0.3f * drift(t_now, 0.6f);
    }

    if (fabsf(vol - last_vol) > 0.02f)  { note_vol(h, vol); last_vol = vol; }
    if (fabsf(timb - last_timb) > 0.004f) { note_timbre(h, timb); last_timb = timb; }
    if (fabsf(mor - last_mor) > 0.004f) { note_morph(h, mor); last_mor = mor; }
    if (fabsf(vd - last_vd) > 0.003f || fabsf(vr - last_vr) > 0.05f) { note_lfo(h, 0, LFO_PITCH, vr, vd); last_vd = vd; last_vr = vr; }
    shown_vol = vol; shown_pr = timb; shown_sp = sp; shown_cents = vd * 100;
    (void)H;
}

static void new_take(void) {
    rnd_seed(seed);
    release_all();
    idx = 0; t_next = t_now + 0.4f; bow_pos = 0.05f; bow_dir = 1; bc_t0 = -10;
}

// ---------------------------------------------------------------------------------------------
#ifdef PROBE_BC
// one long D3, no vibrato at all (clean pitch tracks), bow changes at 1.5 s and 3.0 s by method
static float pr_t;
static void probe_update(void) {
    pr_t += dt(); t_now = pr_t;
    if (!h && pr_t > 0.2f && pr_t < 0.3f) { h = note_on(50, CELLO, 5); note_glide(h, 0); last_vol = -1; }
    if (!h) return;
    static int fired = 0;
    if ((fired == 0 && pr_t >= 1.5f) || (fired == 1 && pr_t >= 3.0f)) {
        fired++;
        if (PROBE_BC != BC_NONE) bow_change(PROBE_BC, 50);
    }
    float gmul, mor_add, pr_add; bc_shape(&gmul, &mor_add, &pr_add);
    if (h_fade && pr_t - fade_t0 > 0.06f) { note_off(h_fade); h_fade = 0; }
    else if (h_fade) note_vol(h_fade, 5.0f * (1.0f - (pr_t - fade_t0) / 0.06f));
    float vol = 5.0f * gmul;
    if (fabsf(vol - last_vol) > 0.01f) { note_vol(h, vol); last_vol = vol; }
    note_morph(h, clamp(0.55f + mor_add, 0, 1));
    note_timbre(h, clamp(0.42f + pr_add, 0, 1));
    if (pr_t > 4.6f) { note_off(h); h = 0; }
}
#endif

#ifdef PROBE_MAC
// one held D3, one macro ramped 0 -> 1 over 4 s on the HELD note (1 harmonics, 2 timbre, 3 morph),
// to see which ones move a sounding bowed voice
static float pm_t;
static void probe_mac(void) {
    pm_t += dt();
    if (!h && pm_t > 0.2f && pm_t < 0.3f) { h = note_on(50, CELLO, 5); }
    if (!h) return;
    float x = clamp((pm_t - 1.0f) / 4.0f, 0, 1);
    if (PROBE_MAC == 1) note_harmonics(h, x);
    if (PROBE_MAC == 2) note_timbre(h, x);
    if (PROBE_MAC == 3) note_morph(h, x);
    if (pm_t > 5.6f) { note_off(h); h = 0; }
}
#endif

void init(void) {
    setup_sound();
    new_take();
#ifdef PROBE_MAC
    instrument_vibrato(CELLO, 0.0f);
    playing = false;
#endif
#ifdef PROBE_BC
    instrument_vibrato(CELLO, 0.0f);
    playing = false;
#endif
}

void update(void) {
#ifdef PROBE_MAC
    probe_mac();
    return;
#endif
#ifdef PROBE_BC
    probe_update();
    return;
#endif
    if (keyp(KEY_SPACE)) { playing = !playing; if (!playing) release_all(); t_next = t_now + 0.2f; }
    if (keyp(KEY_TAB)) { human = !human; release_all(); }
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) layer[i] = !layer[i];
    if (keyp(KEY_UP))    humanity = fminf(1.5f, humanity + 0.25f);
    if (keyp(KEY_DOWN))  humanity = fmaxf(0.0f, humanity - 0.25f);
    if (keyp(KEY_RIGHT)) tempo = min(100, tempo + 4);
    if (keyp(KEY_LEFT))  tempo = max(44, tempo - 4);
    if (keyp('R')) { seed++; new_take(); }

    t_now += dt();
    if (playing) {
        if (t_now >= t_next) { start_note(idx); idx = (idx + 1) % NT; }
        ride();
    }

    tr_pitch[tr_pos] = h ? n_midi_f + (human ? (shown_cents / 100.0f) * sinf(t_now * 6.28f * vib_rate) : 0) : 0;
    tr_vol[tr_pos] = h ? (human ? last_vol : 5) : 0;
    tr_bc[tr_pos] = (t_now - bc_t0) < dt() * 1.5f;
    tr_pos = (tr_pos + 1) % TR;
#ifdef DE_TRACE
    watch("idx", "%d", idx);
    watch("bow", "%.2f", bow_pos);
    watch("dir", "%d", bow_dir);
    watch("changes", "%d", n_changes);
    watch("mid", "%d", n_midchanges);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print(human ? "BOW ARM" : "KEYBOARD", 4, 4, human ? CLR_GREEN : CLR_RED);
    print(str("%d bpm   take %d", tempo, seed), 112, 4, CLR_WHITE);
    if (!playing) print("stopped", 260, 4, CLR_LIGHT_GREY);

    // pitch trace (cents x6) with the level as thickness; bow changes marked
    int x0 = 10, top = 20, bot = 104;
    rectfill(x0, top, TR, bot - top, CLR_DARKER_GREY);
    for (int m = 45; m <= 62; m += 1) {
        if (m != 45 && m != 50 && m != 57 && m != 62) continue;
        int y = bot - (m - 43) * (bot - top) / 21;
        line(x0, y, x0 + TR - 1, y, CLR_DARK_GREY);
    }
    for (int k = 0; k < TR; k++) {
        int i = (tr_pos + k) % TR;
        if (tr_bc[i]) line(x0 + k, top, x0 + k, bot - 1, CLR_ORANGE);
        if (tr_pitch[i] <= 0) continue;
        float m = tr_pitch[i], base = floorf(m + 0.5f), dev = (m - base) * 6.0f;
        int y = bot - (int)((base + dev - 43) * (bot - top) / 21.0f);
        int th = 1 + (int)(tr_vol[i] / 2.0f);
        rectfill(x0 + k, y - th / 2, 1, th, tr_vol[i] > 5.0f ? CLR_YELLOW : CLR_WHITE);
    }
    print("pitch x6c  width=level", x0, bot + 3, CLR_LIGHT_GREY);
    print("| bow change", x0 + 192, bot + 3, CLR_ORANGE);

    // the bow: frog (left) to tip (right), where it touches the string, which way it travels
    int by = 128, bx0 = 42, bw = 228;
    print("frog", 4, by - 3, CLR_LIGHT_GREY);
    print("tip", 290, by - 3, CLR_LIGHT_GREY);
    rectfill(bx0, by - 1, bw, 3, CLR_BROWN);
    rectfill(bx0 - 4, by - 4, 6, 9, CLR_DARK_GREY);
    int cx = bx0 + (int)(bow_pos * bw);
    bool flash = (t_now - bc_t0) < 0.15f;
    line(cx, by - 9, cx, by + 9, flash ? CLR_ORANGE : CLR_WHITE);
    if (human) {
        int ax = cx + bow_dir * 10;
        line(cx, by + 12, ax, by + 12, CLR_YELLOW);
        line(ax, by + 12, ax - bow_dir * 3, by + 9, CLR_YELLOW);
        line(ax, by + 12, ax - bow_dir * 3, by + 15, CLR_YELLOW);
        print(bow_dir > 0 ? "down-bow" : "up-bow", 4, by + 18, CLR_YELLOW);
        print(str("speed %.2f  pressure %.2f  vib %2.0fc", shown_sp, shown_pr, shown_cents), 4, by + 28, CLR_INDIGO);
        print(str("bow changes %d (mid-note %d)", n_changes, n_midchanges), 88, by + 18, CLR_LIGHT_GREY);
    }

    int y2 = 176, lx = 4;
    for (int i = 0; i < NLAYER; i++) {
        int c = !human ? CLR_DARK_GREY : layer[i] ? CLR_GREEN : CLR_DARK_GREY;
        const char *t = str("%d %s", i + 1, LAYER_NAME[i]);
        print(t, lx, y2, c);
        lx += (int)strlen(t) * 8 + 6;
    }
    print(str("humanity %.2f  TAB  R  SPACE  </>", humanity), 4, y2 + 12, CLR_LIGHT_GREY);
}
