/* de:meta
{
  "slug": "tablahand",
  "title": "tablahand",
  "status": "wip",
  "created": "2026-11-04",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "drum-synthesis",
    "step-sequencer",
    "swing-timing"
  ],
  "lineage": "The third humanized-playing spike for Playtime, after strumhand (guitar: the attack) and breathflute (flute: during the note). A tabla is neither one sound nor one hand: a VOCABULARY of strokes (bols) from two hands on two drums, open or damped, with the left hand's wrist bending the bayan's pitch. INSTR_MEMBRANE voiced after the tabla cart's preset (instrument-recipes membrane/tabla). Teental, the 16-mbeat cycle, is the standard public theka.",
  "description": {
    "summary": "Teental on a tabla played by two HANDS instead of a step grid: every bol is a stroke with its own force, timing and spot on the head, dha lands as two hands a few ms apart, the bayan's pitch rises when the wrist presses and sinks when it lets go. TAB flips to the grid version.",
    "detail": "The vocabulary: na/ta ring on the rim, tin rings on the head, ti/te/ra are closed strokes that stop at once, ge is the open bass whose pitch the wrist bends, ke is the flat closed bass slap; dha = na + ge, dhin = tin + ge. THE LEFT HAND keeps playing after it strikes: each ge gets a wrist gesture (press: up and back; wail: the gumki, up-down-up while it rings; dip: struck pressed, then let go so the pitch falls; meend: a late slow rise; or flat) and a palm that comes down to damp it, soon or late, lightly or fully, and lifts again just before the next stroke. The right hand's resting fingers press a little differently each stroke, so the ring length varies. The hands: the left and right of a dha almost never land together, a fast run like ti-ra-ki-ta is uneven (long-short) with its inner strokes softer, a hand striking twice quickly is a little late and softer the second time, and where each finger lands drifts slowly. The cycle: sam (beat 1) leans hardest, the khali section is lighter and has no bass, every fourth cycle plays a fill, a tihai (one figure three times, landing on sam) or a ke-heavy variation, in turn. Keys 1-5 switch each layer on and off.",
    "controls": "SPACE play/stop · TAB hands / grid · 1 time 2 force 3 touch 4 left 5 hands · UP/DOWN humanity · LEFT/RIGHT tempo · R new take"
  }
}
de:meta */
// tablahand — a spike: does a tabla theka sound more human when the strokes come from two
// modeled hands instead of a step grid?
//
// Engine side (no new API): four INSTR_MEMBRANE slots, because a stroke is OPEN (rings) or
// CLOSED (damped at once) and a slot's release is per slot:
//   dayan (right, treble) open + closed · bayan (left, bass) open + closed
// Every stroke is booked on the audio clock (schedule_note_at) with per-stroke gain, strike
// position (timbre offset) and chirp (morph offset). instrument_choke makes a closed stroke
// stop the ringing open one on the same drum, the way a hand does.
//
// The bayan GLIDE happens after the stroke, so it can't be booked: the cart rides
// instrument_tune on the bayan slot every frame (the slot tune is live and bends a sounding
// voice). Works because there is one bayan; a pool of slots (Playtime) would need a booked
// per-note pitch curve. Same finding as breathflute.

#include "studio.h"
#include <math.h>
#include <string.h>

#define D_OPEN  10
#define D_SHUT  11
#define B_OPEN  12
#define B_SHUT  13
#define LOOKAHEAD 0.12

#define DAYAN 62        // the treble drum tuned to D
#define BAYAN 38        // the bass drum, two octaves and a bit under

// ---- strokes ----
enum { R_NONE, R_NA, R_TIN, R_TE, R_RA };      // right hand on the dayan
enum { L_NONE, L_GE, L_KE };                   // left hand on the bayan
typedef struct { const char *name; int r, l; } Bol;
static const Bol BOLS[] = {
    { "dha",  R_NA,  L_GE }, { "dhin", R_TIN, L_GE }, { "ta", R_NA, L_NONE }, { "na", R_NA, L_NONE },
    { "tin",  R_TIN, L_NONE }, { "ge", R_NONE, L_GE }, { "ke", R_NONE, L_KE }, { "ki", R_NONE, L_KE },
    { "ka",   R_NONE, L_KE }, { "ti", R_TE, L_NONE }, { "te", R_TE, L_NONE }, { "ra", R_RA, L_NONE },
};
#define NBOL ((int)(sizeof BOLS / sizeof BOLS[0]))

// teental: 16 beats in four sections of 4; beat 9-12 is the khali (empty) section.
// A beat holds 1-4 bols spread evenly through it; "-" is a rest.
static const char *THEKA[16] = {
    "dha","dhin","dhin","dha",  "dha","dhin","dhin","dha",
    "dha","tin","tin","ta",     "ta","dhin","dhin","dha",
};
static const char *FILL[16] = {
    "dha","dhin","dhin","dha",  "dha","dhin","dhin","dha",
    "dha","tin","tin","ta",     "ti ra ki ta","ti ra ki ta","dha ge na ge","dha -",
};
// a tihai: one figure three times, the last stroke landing on the next sam
static const char *TIHAI[16] = {
    "dha","dhin","dhin","dha",  "dha","dhin","dhin","dha",
    "dha","tin","tin","ta",     "ti ra ki ta","dha ti dha -","dha ti dha -","dha ti dha -",
};
static const char *KE_FILL[16] = {
    "dha","dhin","dhin","dha",  "dha","dhin","dhin","dha",
    "dha","tin","tin","ta",     "ta ke dhin -","na ke dhin -","ge ge ti ra","ki ta dha -",
};

enum { L_TIME, L_FORCE, L_TOUCH, L_GLIDE, L_HANDS, NLAYER };
static const char *LAYER_NAME[NLAYER] = { "time", "force", "touch", "left", "hands" };

static bool  playing = true, human = true;
static bool  layer[NLAYER] = { true, true, true, true, true };
static float humanity = 1.0f;
static int   tempo = 96;
static int   seed = 1;

static double t_beat;          // audio time of the next beat
static int    mbeat, cycle;

// the hands' memory: when each last struck (a quick second stroke is a little late and soft)
static double last_r = -1, last_l = -1;
static float  hand_pos_r, hand_pos_l;

// THE LEFT HAND on the bayan. Each open ge gets a GESTURE for the wrist (pitch) and a PALM plan
// (when it comes down to damp, how far). Both are ridden every frame on the bayan slot: pitch
// through instrument_tune, the palm through instrument_level. The strike itself stays booked.
enum { G_FLAT, G_PRESS, G_WAIL, G_DIP, G_MEEND, NGEST };
static const char *GEST_NAME[NGEST] = { "flat", "press", "wail", "dip", "meend" };
typedef struct {
    double t;             // the stroke's audio time
    int    g;             // gesture
    float  amount;        // semitones
    float  speed;         // seconds for the main movement
    float  damp_at;       // seconds after the stroke the palm comes down (big = lets it ring)
    float  damp_to;       // level the palm leaves (0.1 = almost stopped, 0.6 = just touching)
} LeftEv;
#define NLEFT 16
static LeftEv left[NLEFT];
static int    nleft;
static float  tune_last = -99, level_last = -99;

// display
#define NDOT 64
typedef struct { double t; int mbeat, hand; float force, pos; const char *bol; } Dot;
static Dot dots[NDOT];
static int ndot;
static float shown_glide, shown_palm;
static int   shown_gest;

static float smoothstep01(float x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return x * x * (3 - 2 * x); }
static float r1(void) { return rnd_float() * 2.0f - 1.0f; }
static float drift(double x, float k) { return noise((float)x * k + seed * 13.1f) * 2.0f - 1.0f; }

static void setup_sound(void) {
    // dayan: tuned head, rings ~1.8s; the closed twin stops in 20ms
    instrument(D_OPEN, INSTR_MEMBRANE, 1, 0, 7, 260);
    instrument_harmonics(D_OPEN, 0.10f); instrument_timbre(D_OPEN, 0.45f); instrument_morph(D_OPEN, 0.12f);
    instrument(D_SHUT, INSTR_MEMBRANE, 1, 0, 7, 22);
    instrument_harmonics(D_SHUT, 0.45f); instrument_timbre(D_SHUT, 0.25f); instrument_morph(D_SHUT, 0.05f);
    // bayan: looser, a deeper chirp; the closed slap is a flat thud
    instrument(B_OPEN, INSTR_MEMBRANE, 1, 0, 7, 420);
    instrument_harmonics(B_OPEN, 0.22f); instrument_timbre(B_OPEN, 0.30f); instrument_morph(B_OPEN, 0.30f);
    instrument(B_SHUT, INSTR_MEMBRANE, 1, 0, 7, 25);
    instrument_harmonics(B_SHUT, 0.75f); instrument_timbre(B_SHUT, 0.55f); instrument_morph(B_SHUT, 0.0f);
    instrument_choke(D_SHUT, D_OPEN);       // a closed stroke stops the ringing head
    instrument_choke(B_SHUT, B_OPEN);
    instrument_pan(D_OPEN, 0.18f); instrument_pan(D_SHUT, 0.18f);
    instrument_pan(B_OPEN, -0.18f); instrument_pan(B_SHUT, -0.18f);
    for (int s = D_OPEN; s <= B_SHUT; s++) instrument_reverb(s, 0.16f);
    reverb(0.35f, 0.5f);
}

static void new_take(void) {
    rnd_seed(seed);
    nleft = 0; mbeat = 0; cycle = 0;
    t_beat = audio_time() + 0.25;
}

static const Bol *find_bol(const char *w, int n) {
    for (int i = 0; i < NBOL; i++)
        if ((int)strlen(BOLS[i].name) == n && !strncmp(BOLS[i].name, w, n)) return &BOLS[i];
    return NULL;
}

static void dot(double t, int hand, float force, float pos, const char *bol) {
    dots[ndot % NDOT] = (Dot){ t, mbeat, hand, force, pos, bol };
    ndot++;
}

static void stroke(int slot, int midi, double t, float force, float pos, float chirp, int gate, float press) {
    if (force < 0.04f) force = 0.04f;                        // NoteOpts gain 0 means FULL
    NoteOpts o = { .gain = force, .timbre = pos, .morph = chirp, .harmonics = press };
    schedule_note_at(t, midi, slot, 7, gate, &o);
}

// one bol at time t. accent 0..1 (where in the cycle), inner = an inner stroke of a fast group
static void play_bol(const Bol *b, double t, float accent, bool inner, int sub, int nsub, double sub_len) {
    float H = humanity;
    bool T = human && layer[L_TIME], F = human && layer[L_FORCE], C = human && layer[L_TOUCH];
    bool G = human && layer[L_GLIDE], K = human && layer[L_HANDS];

    // force: the cycle's accent, softer inner strokes of a run, a little scatter
    float force = 0.75f;
    if (F) {
        force = 0.62f + 0.33f * accent * H + r1() * 0.06f * H;
        if (inner) force -= 0.22f * H;
    }

    // timing: fast runs are long-short, the feel drifts slowly
    double tt = t;
    if (T) {
        if (nsub == 4 && (sub & 1)) tt += sub_len * 0.10 * H;          // ti-RA-ki-TA: the 2nd and 4th late
        tt += (drift(t, 0.25f) * 0.008 + r1() * 0.003) * H;
    }

    // dha/dhin: two hands, never quite together
    double flam = (T && b->r && b->l) ? (0.003 + 0.009 * rnd_float()) * H * (rnd_float() < 0.5f ? 1 : -1) : 0;

    if (b->r) {
        double tr = tt + (flam > 0 ? flam : 0);
        float f = force;
        // the same hand twice in quick succession: a hair late and softer
        if (K && last_r > 0 && tr - last_r < 0.13) { tr += 0.006 * H; f *= 1.0f - 0.15f * H; }
        float pos = 0, chirp = 0, press = 0;
        if (C) {
            hand_pos_r = drift(t, 0.6f) * 0.06f;
            pos = (hand_pos_r + r1() * 0.04f) * H;
            chirp = (f - 0.75f) * 0.15f * H;                       // harder = more pitch off the strike
            // the resting fingers: pressing harder on the head shortens and dulls the ring
            press = (drift(t, 0.4f) * 0.06f + rnd_float() * 0.05f) * H;
        }
        switch (b->r) {
        case R_NA:  stroke(D_OPEN, DAYAN, tr, f, 0.38f + pos, chirp, 900, press); break;   // rim: bright ring
        case R_TIN: stroke(D_OPEN, DAYAN, tr, f * 0.9f, pos, chirp, 900, press); break;     // head: round ring
        case R_TE:  stroke(D_SHUT, DAYAN, tr, f * 0.75f, -0.15f + pos, 0, 30, 0); break;     // closed
        case R_RA:  stroke(D_SHUT, DAYAN, tr, f * 0.6f, -0.05f + pos, 0, 25, 0); break;
        }
        last_r = tr;
        dot(tr, 0, f, pos, b->name);
    }
    if (b->l) {
        double tl = tt + (flam < 0 ? -flam : 0);
        float f = force * 0.95f;
        if (K && last_l > 0 && tl - last_l < 0.13) { tl += 0.006 * H; f *= 1.0f - 0.15f * H; }
        float pos = C ? (drift(t, 0.5f) * 0.05f + r1() * 0.03f) * H : 0;
        if (b->l == L_GE) {
            stroke(B_OPEN, BAYAN, tl, f, pos, 0, 1400, 0);
            if (G) {
                // choose what the wrist does: strong beats press and wail, light ones stay flat
                float u = rnd_float();
                int g = u < 0.12f ? G_FLAT : u < 0.42f ? G_PRESS : u < 0.62f ? G_WAIL : u < 0.82f ? G_DIP : G_MEEND;
                if (accent >= 0.99f && rnd_float() < 0.6f) g = G_WAIL;          // sam: let it sing
                LeftEv e = { tl, g, (0.6f + 2.0f * rnd_float()) * (0.6f + 0.4f * accent) * H,
                             0.08f + 0.2f * rnd_float(),
                             0.12f + 0.6f * rnd_float() * rnd_float(),          // usually damped soon
                             0.08f + 0.5f * rnd_float() };
                if (g == G_WAIL) { e.damp_at += 0.4f; e.speed = 0.25f + 0.2f * rnd_float(); }
                left[nleft % NLEFT] = e; nleft++;
            }
        } else {
            stroke(B_SHUT, BAYAN + 7, tl, f * 0.8f, pos, 0, 30, 0);
            if (G) { LeftEv e = { tl, -1, 0, 0, 0, 0.05f }; left[nleft % NLEFT] = e; nleft++; }   // flat palm
        }
        last_l = tl;
        dot(tl, 1, f, pos, b->name);
    }
}

static void play_beat(int b, double t, double len) {
    const char *src = THEKA[b];
    if (human && layer[L_FORCE] && cycle % 4 == 3) {
        int k = (cycle / 4) % 3;
        src = k == 0 ? FILL[b] : k == 1 ? TIHAI[b] : KE_FILL[b];
    }
    if (!human) src = THEKA[b];
    // split into words
    const char *w[4]; int wl[4], n = 0;
    for (const char *p = src; *p && n < 4; ) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *s = p; while (*p && *p != ' ') p++;
        w[n] = s; wl[n] = (int)(p - s); n++;
    }
    // the cycle's shape: sam leans hardest, section starts next, khali (9-12) lighter
    float accent = (b == 0) ? 1.0f : (b % 4 == 0) ? 0.7f : 0.4f;
    if (b >= 8 && b < 12) accent -= 0.25f;
    for (int i = 0; i < n; i++) {
        if (wl[i] == 1 && w[i][0] == '-') continue;
        const Bol *bol = find_bol(w[i], wl[i]);
        if (!bol) continue;
        double sub = len / n;
        play_bol(bol, t + i * sub, i == 0 ? accent : 0.3f, n == 4 && i > 0 && i < 3, i, n, sub);
    }
}

static void schedule_ahead(void) {
    double now_a = audio_time();
    double len = 60.0 / tempo;
    while (t_beat < now_a + LOOKAHEAD) {
        if (t_beat < now_a) t_beat = now_a + 0.01;
        double l = len;
        // a slight push into sam through the last section
        if (human && layer[L_TIME] && mbeat >= 12) l *= 1.0 - 0.012 * humanity;
        play_beat(mbeat, t_beat, l);
        t_beat += l;
        mbeat++;
        if (mbeat == 16) { mbeat = 0; cycle++; }
    }
}

// the wrist's pitch a seconds after a stroke with this gesture
static float wrist(const LeftEv *e, float a) {
    float A = e->amount, s = e->speed;
    switch (e->g) {
    case G_PRESS: return a < s ? A * smoothstep01(a / s) : A * expf(-(a - s - 0.15f > 0 ? a - s - 0.15f : 0) / 0.35f);
    case G_WAIL:  return A * (0.5f - 0.5f * cosf(6.2832f * a / (2.0f * s))) * expf(-a / 1.6f);     // up, down, up
    case G_DIP:   return A * expf(-a / (s * 0.8f));                    // struck pressed, let go: it falls
    case G_MEEND: return a < 0.12f ? 0 : A * smoothstep01((a - 0.12f) / (s * 2.5f));  // a late, slow rise
    }
    return 0;
}

// every frame: the left hand on the bayan
static void ride_left(void) {
    double now_a = audio_time();
    float semi = 0, level = 1;
    shown_gest = -1;
    if (human && layer[L_GLIDE] && nleft > 0) {
        // the last stroke at or before now, and the next one after it
        const LeftEv *cur = NULL, *nxt = NULL;
        for (int k = 0; k < NLEFT && k < nleft; k++) {
            const LeftEv *e = &left[k];
            if (e->t <= now_a) { if (!cur || e->t > cur->t) cur = e; }
            else if (!nxt || e->t < nxt->t) nxt = e;
        }
        if (cur && cur->g >= 0) {
            float a = (float)(now_a - cur->t);
            semi = wrist(cur, a);
            float d = smoothstep01((a - cur->damp_at) / 0.09f);         // the palm comes down over ~90ms
            level = 1.0f - (1.0f - cur->damp_to) * d;
            shown_gest = cur->g;
        } else if (cur) level = 0.05f;                                 // after a ke the palm stays flat
        // before the next stroke the palm lifts and the wrist gets ready (a dip is struck pressed)
        if (nxt && nxt->g >= 0) {
            float until = (float)(nxt->t - now_a);
            if (until < 0.035f) {
                float k = 1.0f - until / 0.035f;
                level += (1.0f - level) * k;
                float ready = nxt->g == G_DIP ? nxt->amount : 0;
                semi += (ready - semi) * k;
            }
        }
    }
#ifdef PROBE_NO_PALM   // DE_DEFINES probes: isolate the palm / the wrist for click-check
    level = 1;
#endif
#ifdef PROBE_NO_WRIST
    semi = 0;
#endif
    if (fabsf(semi - tune_last) > 0.01f)   { instrument_tune(B_OPEN, semi); tune_last = semi; }
    if (fabsf(level - level_last) > 0.01f) { instrument_level(B_OPEN, level); level_last = level; }
    shown_glide = semi; shown_palm = 1.0f - level;
}

void init(void) {
    setup_sound();
    new_take();
}

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; t_beat = audio_time() + 0.15; }
    if (keyp(KEY_TAB)) human = !human;
    for (int i = 0; i < NLAYER; i++) if (keyp('1' + i)) layer[i] = !layer[i];
    if (keyp(KEY_UP))    humanity = fminf(1.5f, humanity + 0.25f);
    if (keyp(KEY_DOWN))  humanity = fmaxf(0.0f, humanity - 0.25f);
    if (keyp(KEY_RIGHT)) tempo = min(180, tempo + 6);
    if (keyp(KEY_LEFT))  tempo = max(50, tempo - 6);
    if (keyp('R')) { seed++; new_take(); }
    if (playing) schedule_ahead();
    ride_left();
#ifdef DE_TRACE
    watch("beat", "%d", mbeat);
    watch("glide", "%.2f", shown_glide);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    double at = audio_time();
    print(human ? "HANDS" : "GRID", 4, 4, human ? CLR_GREEN : CLR_RED);
    print(str("teental  %d bpm  cycle %d  take %d", tempo, cycle + 1, seed), 60, 4, CLR_WHITE);

    // the cycle: 16 beats round a circle, sam at the top; strokes land as dots
    int cx = 92, cy = 92, R = 58;
    circ(cx, cy, R, CLR_DARK_GREY);
    for (int b = 0; b < 16; b++) {
        float a = b / 16.0f * 360.0f - 90.0f;
        int x = cx + (int)(cos_deg(a) * R), y = cy + (int)(sin_deg(a) * R);
        int c = b == 0 ? CLR_YELLOW : (b >= 8 && b < 12) ? CLR_DARK_GREY : CLR_LIGHT_GREY;
        circfill(x, y, b % 4 == 0 ? 3 : 1, c);
    }
    print("sam", cx - 11, cy - R - 12, CLR_YELLOW);
    print("khali", cx - 19, cy + R + 4, CLR_DARK_GREY);
    for (int k = 0; k < NDOT && k < ndot; k++) {
        Dot *d = &dots[k];
        float age = (float)(at - d->t);
        if (age < 0 || age > 1.2f) continue;
        float a = (d->mbeat + 0.0f) / 16.0f * 360.0f - 90.0f;
        int r = R - 10 - d->hand * 14;
        int x = cx + (int)(cos_deg(a) * r), y = cy + (int)(sin_deg(a) * r);
        int sz = 1 + (int)(d->force * 4 * (1.0f - age / 1.2f));
        circfill(x, y, sz, d->hand ? CLR_ORANGE : CLR_PINK);
    }

    // the two drums: where the last strokes landed, the bayan's bend
    int dx = 230, dy = 60, bx = 230, by = 130;
    circfill(dx, dy, 24, CLR_BROWN); circfill(dx, dy, 9, CLR_BLACK);
    circfill(bx, by, 30, CLR_DARK_GREY); circfill(bx - 8, by, 10, CLR_BLACK);
    print("dayan", dx - 19, dy - 36, CLR_PINK);
    print("bayan", bx - 19, by - 42, CLR_ORANGE);
    for (int k = 0; k < NDOT && k < ndot; k++) {
        Dot *d = &dots[k];
        float age = (float)(at - d->t);
        if (age < 0 || age > 0.25f) continue;
        int x = (d->hand ? bx : dx) + (int)((0.5f + d->pos * 4) * (d->hand ? 22 : 18));
        int y = d->hand ? by : dy;
        circfill(x, y, 3, CLR_WHITE);
        print(d->bol, (d->hand ? bx : dx) + 30, y - 3, d->hand ? CLR_ORANGE : CLR_PINK);
    }
    // the wrist meter
    rect(bx - 30, by + 36, 60, 6, CLR_LIGHT_GREY);
    rectfill(bx - 29, by + 37, (int)(58 * fminf(1, shown_glide / 3.0f)), 4, CLR_ORANGE);
    rect(bx - 30, by + 45, 60, 6, CLR_LIGHT_GREY);
    rectfill(bx - 29, by + 46, (int)(58 * fminf(1, shown_palm)), 4, CLR_INDIGO);
    if (human) {
        print(str("wrist +%.1f", shown_glide), bx - 30 - 92, by + 35, CLR_ORANGE);
        print("palm", bx - 30 - 36, by + 45, CLR_INDIGO);
        if (shown_gest >= 0) print(GEST_NAME[shown_gest], bx + 34, by + 40, CLR_ORANGE);
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
