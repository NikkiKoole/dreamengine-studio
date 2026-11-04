/* de:meta
{
  "slug": "reedhand",
  "title": "reedhand",
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
  "lineage": "The fourth humanized-playing spike (docs/design/humanized-playing.md), after strumhand, breathflute and tablahand: breathflute's lungs model moved to a tenor sax (INSTR_REED, the reed cart's tenor preset) and given a jazz player's vocabulary. Uses instrument_vibrato to switch off the reed's always-on vibrato so the cart owns it.",
  "description": {
    "summary": "A tenor sax blues line played by a player instead of a sequencer: scoops into notes, ghost notes, a growl when it leans in, falls at the end of a phrase, a vibrato that blooms late on long notes, a swing that lays back and breathes. TAB flips to the sequenced version.",
    "detail": "The line is written with a jazz player's marks: tongued, slurred, ghosted (swallowed, barely there), accented with a growl, scooped from below, a long note to sing on, and a fall off the end. The lungs run down through a phrase (airier, softer) and a longer phrase ahead takes a deeper breath. Blowing harder brightens the reed and pushes the pitch a little sharp; leaning in growls. The swing ratio wanders and the whole line sits a little behind the beat, more on the long notes. Keys 1-5 switch each layer on and off.",
    "controls": "SPACE play/stop · TAB player / sequencer · 1 time 2 breath 3 pitch 4 vib 5 marks · UP/DOWN humanity · LEFT/RIGHT tempo · R new take"
  }
}
de:meta */
// reedhand — a spike: a sax line shaped during each note by a model of the player.
//
// Same mechanics as breathflute (held notes ridden every frame: note_vol, note_pitch,
// note_timbre, note_morph, note_lfo), so the same engine finding applies: none of this can be
// BOOKED today (humanized-playing.md §4.2). What this spike adds is a sax VOCABULARY: ghost
// notes, scoops, falls, growl, and swing that is felt rather than quantized.
//
// INSTR_REED macros: harmonics = bore (fixed: tenor), timbre = reed edge (brightness, ridden with
// the breath), morph = breath (lean-in growl, ridden on accents and long notes).

#include "studio.h"
#include <math.h>
#include <string.h>

#define SL 10

// ---- the line (original), F blues, lengths in swung eighths ----
// art: 't' tongued, '_' slurred, 'g' ghost, '!' accent + growl, 's' scooped from below,
//      '~' a long note to sing on, 'f' fall off the end
typedef struct { int midi; int len; char art; int breath; } N;
static const N TUNE[] = {
    {53,1,'t',0},{56,1,'_',0},{57,1,'_',0},{60,2,'t',0},{63,1,'_',0},{62,1,'_',0},{60,5,'~',1},
    {60,1,'g',0},{62,1,'t',0},{63,1,'_',0},{64,1,'_',0},{65,3,'!',0},{62,1,'_',0},{60,6,'f',1},
    {57,2,'s',0},{60,1,'t',0},{57,1,'_',0},{55,1,'g',0},{53,1,'t',0},{56,1,'_',0},{57,1,'_',0},
        {53,7,'~',1},
};
#define NT ((int)(sizeof TUNE / sizeof TUNE[0]))

enum { L_TIME, L_BREATH, L_PITCH, L_VIB, L_MARKS, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "time", "breath", "pitch", "vib", "marks" };

static bool  playing = true, human = true;
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 112;
static int   seed = 1;

static float t_now, t_next;
static int   idx;
static float air = 1.0f;

static int   h = 0;
static float n_start, n_end, n_vol, n_scoop, n_vib_delay, n_vib_depth, n_vib_rate, n_sharp, n_growl;
static int   n_midi, n_i;
static char  n_art;
static bool  n_slurred;
static float last_vol, last_pitch, last_timb, last_mor, last_vd, last_vr;

#define TR 300
static float tr_pitch[TR], tr_vol[TR];
static int   tr_pos;
static float shown_cents, shown_vib, shown_growl;

static float r1(void) { return rnd_float() * 2.0f - 1.0f; }
static float smooth01(float x) { x = clamp(x, 0, 1); return x * x * (3 - 2 * x); }

static void setup_sound(void) {
    instrument(SL, INSTR_REED, 8, 0, 7, 140);
    instrument_harmonics(SL, 0.82f);     // tenor sax (the reed cart's preset)
    instrument_timbre(SL, 0.30f);
    instrument_morph(SL, 0.62f);
    instrument_reverb(SL, 0.25f);
    reverb(0.5f, 0.5f);
}

static void new_take(void) { rnd_seed(seed); idx = 0; air = 1.0f; t_next = t_now + 0.3f; }

static void phrase_of(int i, float *pos, int *lo, int *hi, int *peak) {
    int a = i; while (a > 0 && !TUNE[a - 1].breath) a--;
    int b = i; while (b < NT - 1 && !TUNE[b].breath) b++;
    *pos = (b > a) ? (i - a) / (float)(b - a) : 0;
    *lo = a; *hi = b;
    int pk = 0; for (int k = a; k <= b; k++) if (TUNE[k].midi > pk) pk = TUNE[k].midi;
    *peak = pk;
}

static void release(void) { if (h) note_off(h); h = 0; }

// how long `len` swung eighths last when they start on an eighth `parity` (0 = on the beat)
static float swung(int len, int parity, float ratio) {
    float beat = 60.0f / tempo, t = 0;
    for (int k = 0; k < len; k++) t += ((parity + k) & 1) ? beat * (1 - ratio) : beat * ratio;
    return t;
}
static int eighth_pos;          // where in the bar (in eighths) the next note starts

static void start_note(int i) {
    const N *n = &TUNE[i];
    float H = humanity;
    float pos; int lo, hi, peak; phrase_of(i, &pos, &lo, &hi, &peak);
    bool M = human && layer[L_MARKS];
    bool slur = M && n->art == '_' && h;

    // TIME: swing that wanders, sitting behind the beat; long notes lean back more
    float ratio = 0.62f;
    if (human && layer[L_TIME]) ratio = 0.62f + 0.04f * (noise(t_now * 0.3f + seed) * 2 - 1) * H;
    float dur = swung(n->len, eighth_pos & 1, ratio);
    if (human && layer[L_TIME]) {
        dur *= 1.0f + r1() * 0.025f * H;
        if (n->breath) dur *= 1.0f + 0.12f * H;                 // the end of a phrase stretches
    }

    // BREATH: louder to the phrase top, ghosts almost nothing, the lungs run down
    float vol = 5.0f;
    if (human && layer[L_BREATH]) {
        float height = 1.0f - fminf(1.0f, (peak - n->midi) / 12.0f);
        vol = 4.0f + 2.0f * height * H + r1() * 0.2f * H - (1.0f - air) * 1.0f * H;
    }
    if (M && n->art == 'g') vol *= 0.45f;
    if (M && n->art == '!') vol += 0.8f;

    // PITCH: every tongued note scoops a little; an 's' scoops a lot; harder = sharper
    n_scoop = 0; n_sharp = 0;
    if (human && layer[L_PITCH]) {
        n_scoop = -(slur ? 8.0f : 18.0f + 14.0f * rnd_float()) * H;
        n_sharp = (vol - 5.0f) * 3.0f * H;
    }
    if (M && n->art == 's') n_scoop = -110.0f * (0.8f + 0.4f * rnd_float()) * fmaxf(H, 0.5f);

    // VIBRATO: only on long notes, late, wide on the '~' notes
    n_vib_depth = 0; n_vib_delay = 0; n_vib_rate = 5.4f;
    if (human && layer[L_VIB] && dur > 0.45f) {
        n_vib_delay = 0.2f + 0.25f * rnd_float();
        n_vib_depth = (0.12f + 0.08f * rnd_float() + (n->art == '~' ? 0.1f : 0)) * H;
        n_vib_rate = 5.0f + 0.9f * rnd_float() + (1.0f - air) * 0.5f;
    }

    // growl: lean in on accents and long sung notes
    n_growl = 0;
    if (M && (n->art == '!' || n->art == '~')) n_growl = (n->art == '!' ? 0.35f : 0.15f) * fmaxf(H, 0.5f);

    instrument_vibrato(SL, human && layer[L_VIB] ? 0.0f : 1.0f);
    if (slur) {
        note_glide(h, 30 + rnd(20));
        note_pitch(h, n->midi + n_scoop / 100.0f);
    } else {
        release();
        h = note_on(n->midi, SL, 0);
        note_glide(h, human ? 16 : 0);
        if (n_scoop) note_pitch(h, n->midi + n_scoop / 100.0f);
        last_vol = last_pitch = last_timb = last_mor = last_vd = last_vr = -999;
    }
    n_start = t_now; n_midi = n->midi; n_i = i; n_vol = vol; n_slurred = slur; n_art = M ? n->art : 't';
    float gap = (M && n->art == 'g') ? dur * 0.45f : (M && i + 1 < NT && TUNE[i + 1].art != '_' && !n->breath) ? 0.04f : 0.015f;
    n_end = t_now + dur - (n->breath ? 0 : gap);
    if (!human) n_end = t_now + dur * 0.9f;
    t_next = t_now + dur;
    eighth_pos += n->len;

    if (n->breath) {
        int nxt = (i + 1) % NT, lo2, hi2, pk2; float p2;
        phrase_of(nxt, &p2, &lo2, &hi2, &pk2);
        int ahead = 0; for (int k = lo2; k <= hi2; k++) ahead += TUNE[k].len;
        float rest = swung(2, eighth_pos & 1, ratio);           // a written two-eighth rest
        if (human && layer[L_BREATH]) rest += (0.05f + ahead * 0.01f) * H;
        t_next += rest; eighth_pos += 2;
    }
}

static void ride(void) {
    if (!h) return;
    float H = humanity;
    float age = t_now - n_start, len = n_end - n_start;
    float p = clamp(age / fmaxf(len, 0.01f), 0, 1);
    if (t_now >= n_end) {
        bool next_slur = human && layer[L_MARKS] && idx < NT && TUNE[idx].art == '_' && !TUNE[n_i].breath;
        if (!next_slur) release();
        return;
    }
    if (!human) { if (last_vol < 0) { note_vol(h, 5.0f); last_vol = 5; } return; }

    float v = n_vol;
    if (layer[L_BREATH]) {
        if (len > 0.5f) v *= 0.85f + 0.2f * sinf(3.14159f * powf(p, 0.7f)) * H;
        air -= dt() * (0.03f + v * 0.011f);
        if (air < 0.05f) air = 0.05f;
    }
    // a fall: the end of the note drops away in pitch and level
    float fall = 0;
    if (n_art == 'f') { float k = smooth01((p - 0.62f) / 0.38f); fall = -300.0f * k * k * fmaxf(H, 0.5f); v *= 1.0f - 0.7f * k; }
    v = clamp(v, 0.2f, 7.0f);

    float cents = fall;
    if (layer[L_PITCH]) cents += n_scoop * expf(-age / (n_art == 's' ? 0.09f : 0.05f)) + n_sharp * (v / fmaxf(n_vol, 0.5f));
    else if (n_art == 's') cents += n_scoop * expf(-age / 0.09f);
    float pitch = n_midi + cents / 100.0f;

    // reed edge: brighter when blown harder; a little air at the start and when tired
    float timb = 0.30f;
    if (layer[L_BREATH]) timb += (v - 5.0f) * 0.05f * H + (1.0f - air) * 0.08f * H;
    timb += 0.12f * expf(-age / 0.05f) * (n_slurred ? 0 : 1);
    timb = clamp(timb, 0, 1);
    // breath / growl: lean in after the attack on accents and long notes, ease off when soft
    float mor = 0.62f + (v - 5.0f) * 0.03f * H + n_growl * smooth01((age - 0.08f) / 0.25f);
    mor = clamp(mor, 0.4f, 1.0f);

    float vd = 0, vr = n_vib_rate;
    if (layer[L_VIB] && n_vib_depth > 0) {
        vd = n_vib_depth * smooth01((age - n_vib_delay) / 0.35f);
        vr += 0.3f * (noise(t_now * 0.8f + seed) * 2 - 1);
    }

    if (fabsf(v - last_vol) > 0.02f)       { note_vol(h, v); last_vol = v; }
    if (fabsf(pitch - last_pitch) > 0.002f) { note_pitch(h, pitch); last_pitch = pitch; }
    if (fabsf(timb - last_timb) > 0.005f)   { note_timbre(h, timb); last_timb = timb; }
    if (fabsf(mor - last_mor) > 0.005f)     { note_morph(h, mor); last_mor = mor; }
    if (fabsf(vd - last_vd) > 0.003f || fabsf(vr - last_vr) > 0.05f) { note_lfo(h, 0, LFO_PITCH, vr, vd); last_vd = vd; last_vr = vr; }
    shown_cents = cents; shown_vib = vd; shown_growl = mor - 0.62f;
}

void init(void) { setup_sound(); new_take(); }

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; if (!playing) release(); t_next = t_now + 0.2f; }
    if (keyp(KEY_TAB)) { human = !human; release(); }
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) layer[i] = !layer[i];
    if (keyp(KEY_UP))    humanity = fminf(1.5f, humanity + 0.25f);
    if (keyp(KEY_DOWN))  humanity = fmaxf(0.0f, humanity - 0.25f);
    if (keyp(KEY_RIGHT)) tempo = min(180, tempo + 6);
    if (keyp(KEY_LEFT))  tempo = max(60, tempo - 6);
    if (keyp('R')) { seed++; release(); new_take(); }

    t_now += dt();
    if (playing) {
        if (t_now >= t_next) {
            if (idx == 0 || TUNE[idx - 1].breath) air = 1.0f;
            if (idx == 0) eighth_pos = 0;
            start_note(idx);
            idx = (idx + 1) % NT;
        }
        ride();
    }
    tr_pitch[tr_pos] = h ? n_midi + (human ? shown_cents / 100.0f + shown_vib * sinf(t_now * 6.28f * n_vib_rate) : 0) : 0;
    tr_vol[tr_pos] = h ? (human ? last_vol : 5) : 0;
    tr_pos = (tr_pos + 1) % TR;
#ifdef DE_TRACE
    watch("idx", "%d", idx);
    watch("air", "%.2f", air);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print(human ? "PLAYER" : "SEQUENCER", 4, 4, human ? CLR_GREEN : CLR_RED);
    print(str("tenor  %d bpm  take %d", tempo, seed), 100, 4, CLR_WHITE);
    if (!playing) print("stopped", 260, 4, CLR_LIGHT_GREY);

    int x0 = 10, top = 22, bot = 122;
    rectfill(x0, top, TR, bot - top, CLR_DARKER_GREY);
    for (int m = 51; m <= 67; m += 2) {
        int y = bot - (m - 50) * (bot - top) / 18;
        line(x0, y, x0 + TR - 1, y, (m == 53 || m == 65) ? CLR_DARK_GREY : CLR_DARKER_BLUE);
    }
    for (int k = 0; k < TR; k++) {
        int i = (tr_pos + k) % TR;
        if (tr_pitch[i] <= 0) continue;
        float m = tr_pitch[i], base = floorf(m + 0.5f), dev = (m - base) * 3.0f;
        int y = bot - (int)((base + dev - 50) * (bot - top) / 18.0f);
        int th = 1 + (int)(tr_vol[i] / 2.5f);
        rectfill(x0 + k, y - th / 2, 1, th, tr_vol[i] > 5.5f ? CLR_YELLOW : tr_vol[i] < 3.0f ? CLR_INDIGO : CLR_WHITE);
    }
    print("pitch (cents x3), thickness = breath", x0, bot + 3, CLR_LIGHT_GREY);

    int ly = 140;
    print("air", 4, ly, CLR_LIGHT_GREY);
    rect(36, ly, 102, 8, CLR_LIGHT_GREY);
    rectfill(37, ly + 1, (int)(100 * air), 6, air > 0.35f ? CLR_BLUE : CLR_ORANGE);
    if (human && h) print(str("%+4.0fc %s", shown_cents, shown_growl > 0.08f ? "growl" : ""), 150, ly, CLR_INDIGO);

    int y2 = 176, lx = 4;
    for (int i = 0; i < NLAYER; i++) {
        int c = !human ? CLR_DARK_GREY : layer[i] ? CLR_GREEN : CLR_DARK_GREY;
        const char *t = str("%d %s", i + 1, LAYER_NAME[i]);
        print(t, lx, y2, c);
        lx += (int)strlen(t) * 8 + 6;
    }
    print(str("humanity %.2f  TAB  R  SPACE  </>", humanity), 4, y2 + 12, CLR_LIGHT_GREY);
}
