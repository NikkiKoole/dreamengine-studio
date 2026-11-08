/* de:meta
{
  "slug": "acidref",
  "title": "acid ref probe",
  "status": "active",
  "created": "2026-11-08",
  "kind": ["probe"],
  "teaches": ["analog-voice-modeling"],
  "lineage": "The acid303.h half of an A/B against Robin Schmidt's Open303 (MIT): plays the exact pattern tools/ref-render/open303render.cpp plays, on the same sample grid, so the two WAVs drop into the same oracles. Prompted by X0X (charlesvestal/fm1-x0x), whose 303 is an Open303 port tested against the original.",
  "description": "Not a toy: a measurement probe. Plays a fixed 16-step acid line twice (accents on 3/7/10/14, slides into 5/8/13), then one long plain note and one long accented note, through acid303.h. Render with node tools/play.js acidref run --headless --frames 704 --wav out.wav and compare with bash tools/ref-render/run.sh open303. DE_DEFINES: ACID_KNOB=0.5 sets the five core knobs; ACID_DF=1 uses the Devil Fish voicing instead of acid_stock."
}
de:meta */
#include "studio.h"
#include "acid303.h"

// ── the pattern: keep IDENTICAL to tools/ref-render/open303render.cpp ────
static const int N[16] = { 0, 0, 12, 0, 3, 0, 7, 5, 0, 12, 10, 0, 3, 15, 0, 7 };
static const int A[16] = { 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0 };
static const int S[16] = { 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0 };
#define BASE 36
#define LONG_A 40
#define LONG_B 64
#define LONG_LEN 15
#ifndef ACID_KNOB
#define ACID_KNOB 0.5f
#endif

static Acid a;
static int f;

void init(void) {
    acid_init(&a, 6, -1);
#ifndef ACID_DF
    acid_stock(&a);
#endif
    a.drift = 0.0f;                  // dead-flat: a measurement, not a performance
    a.echo_send = 0.0f;              // dry: Open303 has no slapback
    a.p[ACID_CUT] = a.p[ACID_RES] = a.p[ACID_ENV] = a.p[ACID_DEC] = a.p[ACID_ACC] = ACID_KNOB;
    a.p[ACID_DRV] = 0.0f;
    a.p[ACID_ADEC] = a.p[ACID_DEC];
    acid_define(&a);
}

void update(void) {
    int st = f / 8, sub = f % 8;
    acid_ride(&a);
    if (st < 32) {
        int k = st % 16;
        if (sub == 0) acid_note(&a, BASE + N[k], A[k], S[k]);
        if (sub == 5 && (!S[k] || st == 31)) acid_off(&a);
    } else if ((st == LONG_A || st == LONG_B) && sub == 0) {
        acid_note(&a, BASE + 12, st == LONG_B, 0);
    } else if ((st == LONG_A + LONG_LEN || st == LONG_B + LONG_LEN) && sub == 5) {
        acid_off(&a);
    }
    f++;
}

void draw(void) {
    cls(CLR_BLACK);
    print("acidref: a measurement probe", 8, 8, CLR_LIGHT_GREY);
    print("see tools/ref-render/run.sh open303", 8, 20, CLR_DARK_GREY);
}
