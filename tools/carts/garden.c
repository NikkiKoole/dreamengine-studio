/* de:meta
{
  "title": "music garden",
  "status": "active",
  "kind": [
    "toy"
  ],
  "teaches": [
    "euclidean-rhythm",
    "generative-melody",
    "chord-voicing"
  ],
  "lineage": "A self-playing screensaver that couples a I–vi–IV–V chord loop with a pentatonic random-walk melody gated by euclid(); the visual (flowers blooming on each note) is purely cosmetic — the teaching content is the musical generation layer.",
  "description": "A screensaver that plays itself. A gentle chord progression strums under a pentatonic melody that improvises with chance() and a random walk — every note opens a flower that blooms and closes on a sine envelope. Fireflies drift, grass sways, the mood drifts to new keys and colors. Z shifts the mood by hand."
}
de:meta */
#include "studio.h"

// MUSIC GARDEN
// A screensaver that plays itself — a gentle chord progression with a
// pentatonic melody improvised over it (chord / strum / degree / chance).
// Every note opens a flower. Sit back and watch it bloom.
// Z: shift the mood (new key + colours).

#define MAX_BLOOMS  72
#define N_FLY       18
#define N_CHORDS    4

// ---- flower colour themes (center, petal, inner) ----
static const int TH_CENTER[5] = { CLR_RED,   CLR_ORANGE, CLR_DARK_PURPLE, CLR_TRUE_BLUE, CLR_DARK_GREEN };
static const int TH_PETAL [5] = { CLR_PINK,  CLR_YELLOW, CLR_INDIGO,      CLR_BLUE,      CLR_GREEN      };
static const int TH_INNER [5] = { CLR_LIGHT_PEACH, CLR_WHITE, CLR_PINK,   CLR_WHITE,     CLR_LIGHT_YELLOW };

// ---- a I–vi–IV–V-ish progression in C, transposed by key_off ----
static const int CH_ROOT[N_CHORDS] = { 60, 57, 53, 55 };           // C  A  F  G
static const int CH_TYPE[N_CHORDS] = { CHORD_MAJ7, CHORD_MIN7, CHORD_MAJ7, CHORD_DOM7 };
static const int KEYS[6] = { 0, 2, 5, 7, -3, -5 };

typedef struct {
    float x, y, birth, life, maxr, spin, rot0;
    int   petals, theme;
    bool  active;
} Bloom;

static Bloom blooms[MAX_BLOOMS];

static int   prog;          // index into the progression
static int   chord_count;   // chords played since start
static int   theme;         // current colour theme
static int   key_off;       // current transpose
static int   mel_n;         // melody random-walk position
static int   last_beat;

// ---- spawn a flower ----
static void spawn(float x, float y, float maxr, int petals, float life, int th) {
    for (int i = 0; i < MAX_BLOOMS; i++) {
        if (!blooms[i].active) {
            blooms[i] = (Bloom){ x, y, now(), life, maxr, rnd_float_between(-26, 26),
                                 (float)rnd(360), petals, th, true };
            return;
        }
    }
}

void init() {
    bpm(64);
    prog = 0; chord_count = 0; theme = 0; key_off = 0; mel_n = 4; last_beat = -1;

    // seed a few flowers mid-bloom so the garden isn't empty on the first frame
    for (int i = 0; i < 6; i++) {
        spawn(rnd_between(30, SCREEN_W - 30), rnd_between(24, SCREEN_H - 44),
              rnd_between(14, 30), 5 + rnd(3), 3.0f, rnd(5));
        blooms[i].birth = -blooms[i].life * rnd_float_between(0.2f, 0.7f);  // already partway open
        blooms[i].theme = rnd(5);
    }
}

// ---- one musical beat ----
static void on_beat(int b) {
    // chord change every 4 beats
    if (b % 4 == 0) {
        prog = (prog + 1) % N_CHORDS;
        chord_count++;
        int root = CH_ROOT[prog] + key_off;
        strum(root, CH_TYPE[prog], INSTR_TRI, 3, 35);   // soft up-strum
        note(root - 12, INSTR_SINE, 2);                  // bass

        // big background bloom in the chord's colour
        spawn(rnd_between(40, SCREEN_W - 40), rnd_between(30, SCREEN_H - 50),
              rnd_between(26, 40), 6 + rnd(3), rnd_float_between(3.5f, 4.5f), theme);

        // drift the mood every 4 chords
        if (chord_count % 4 == 0) {
            theme   = (theme + 1) % 5;
            key_off = KEYS[rnd(6)];
        }
    }

    // melody: a euclidean rhythm, pentatonic random walk — always pretty
    if (euclid(5, 8, b) && chance(78)) {
        mel_n += rnd_between(-2, 3);
        mel_n = mid(0, mel_n, 12);
        int midi = degree(SCALE_PENTA, 5, mel_n) + key_off;
        hit(midi, INSTR_SINE, 2, 240);
        spawn(rnd_between(20, SCREEN_W - 20), rnd_between(18, SCREEN_H - 40),
              rnd_between(9, 16), 5, rnd_float_between(2.0f, 2.6f), theme);
    }

    // occasional high sparkle
    if (chance(14)) {
        int midi = degree(SCALE_PENTA, 6, rnd(5)) + key_off;
        hit(midi, INSTR_TRI, 1, 130);
        spawn(rnd_between(16, SCREEN_W - 16), rnd_between(14, SCREEN_H / 2),
              rnd_between(4, 7), 4, 1.4f, (theme + 2) % 5);
    }
}

void update() {
    int b = beat();
    if (b != last_beat) {
        last_beat = b;
        on_beat(b);
    }
    if (btnp(0, BTN_A) || btnp(0, BTN_B)) {   // shift the mood by hand
        theme   = (theme + 1) % 5;
        key_off = KEYS[rnd(6)];
    }
}

// ---- one flower, opening and closing on a sine envelope ----
static void draw_bloom(Bloom *fl) {
    float age = now() - fl->birth;
    float t   = age / fl->life;
    if (t >= 1.0f) { fl->active = false; return; }

    float env = sin_deg(t * 180.0f);     // 0 → 1 → 0
    float r   = env * fl->maxr;
    if (r < 1) return;
    float rot = fl->rot0 + age * fl->spin;

    int petal = TH_PETAL[fl->theme];
    int cen   = TH_CENTER[fl->theme];
    int inn   = TH_INNER[fl->theme];

    for (int p = 0; p < fl->petals; p++) {
        float a  = rot + p * (360.0f / fl->petals);
        int   px = (int)(fl->x + dx(r * 0.55f, a));
        int   py = (int)(fl->y + dy(r * 0.55f, a));
        circfill(px, py, (int)(r * 0.5f), petal);
    }
    circfill((int)fl->x, (int)fl->y, (int)(r * 0.42f), cen);
    circfill((int)fl->x, (int)fl->y, (int)(r * 0.20f), inn);
}

void draw() {
    // night-garden sky
    cls(CLR_DARKER_BLUE);
    rectfill(0, SCREEN_H / 2, SCREEN_W, SCREEN_H / 2, CLR_DARK_BLUE);
    rectfill(0, SCREEN_H - 24, SCREEN_W, 24, CLR_DARKER_PURPLE);

    // drifting fireflies
    for (int i = 0; i < N_FLY; i++) {
        float t  = now() * 0.06f;
        int   fx = (int)(noise(i * 12.1f + t) * SCREEN_W);
        int   fy = (int)(noise(i * 12.1f + 50.0f + t * 0.8f) * SCREEN_H);
        bool  on = sin_deg(now() * 140.0f + i * 47.0f) > 0.2f;
        pset(fx, fy, on ? CLR_YELLOW : CLR_DARK_ORANGE);
    }

    // flowers
    for (int i = 0; i < MAX_BLOOMS; i++)
        if (blooms[i].active) draw_bloom(&blooms[i]);

    // swaying grass along the bottom
    for (int x = 0; x < SCREEN_W; x += 3) {
        int h    = 8 + (int)(noise(x * 0.08f) * 14);
        int sway = (int)(sin_deg(now() * 40.0f + x * 4.0f) * 2.0f);
        line(x, SCREEN_H, x + sway, SCREEN_H - h, CLR_DARK_GREEN);
    }

    print("music garden", 4, SCREEN_H - 9, CLR_DARKER_PURPLE);
}
