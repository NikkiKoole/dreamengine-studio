/* de:meta
{
  "slug": "glassharmonica",
  "title": "glass harmonica",
  "status": "active",
  "created": "2026-06-02",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "additive-synth",
    "chord-voicing"
  ],
  "description": "Ben Franklin's rubbed-wine-glass contraption, built on the HELD NOTE api. Seven nested glass rings each hold a sustained sine voice (note_on at vol 0 — alive but silent); you never trigger them, you RIDE their volume. HOLD the mouse and slide IN and OUT across the rings: whichever one your finger wets swells in with a slow attack (note_vol), neighbours bleed so sweeping between rings blooms a glassy chord, and it all fades slowly when you let go. Bigger ring = lower note; pitches are a major pentatonic so any combination stays consonant. Pure, ghostly, polyphonic — the opposite of a struck note."
}
de:meta */
#include "studio.h"
#include <math.h>

// Glass harmonica — Ben Franklin's rubbed-wine-glass contraption, built on the
// new HELD NOTE api. Seven nested glass rings each hold a sustained sine voice
// (note_on at vol 0 — alive but silent). You never trigger them; you RIDE their
// volume. Press and move the cursor in and out across the rings: whichever ring
// your finger is "wetting" swells in (note_vol ramps up with a slow attack),
// neighbours bleed a little so sweeping between them blooms a glassy chord, and
// everything fades out slowly when you let go. Pure, ghostly, polyphonic — the
// opposite of a struck note, and something fire-and-forget note() can't do.
//
// Bigger ring = lower note (more glass). Pitches are a major pentatonic so any
// combination that rings together stays consonant.
//
// CONTROLS: hold the mouse and slide IN / OUT across the rings to play.

#define SLOT     6
#define NRINGS   7
#define CX       160          // centre of the nested rings
#define CY       104
#define R0       16           // innermost (highest) ring radius
#define RSTEP    12           // gap between rings
#define BAND     14.0f        // how close the finger must be to a ring to wet it

static int   handle[NRINGS];      // one held sine voice per ring (-1 = none)
static float level[NRINGS];       // smoothed 0..1 loudness, what we feed note_vol
static int   midi[NRINGS];        // pitch of each ring

static int ring_r(int i) { return R0 + i * RSTEP; }   // i=0 inner/high .. outer/low

void init(void) {
    // a pure sine with a slow swell and a long tail — the wet-glass tone.
    instrument(SLOT, INSTR_SINE, 220, 200, 7, 900);   // slow attack, long release
    instrument_lfo(SLOT, 0, LFO_PITCH,  5.0f, 0.12f); // faint shimmer
    instrument_lfo(SLOT, 1, LFO_VOLUME, 0.7f, 0.18f); // slow breathing tremolo

    for (int i = 0; i < NRINGS; i++) {
        // outer ring = lowest: n counts up as we move inward
        midi[i]   = degree(SCALE_PENTA, 3, NRINGS - 1 - i);
        handle[i] = note_on(midi[i], SLOT, 0);   // start alive but silent
        level[i]  = 0;
    }
}

void update(void) {
    int down = mouse_down(MOUSE_LEFT);
    float dx = mouse_x() - CX, dy = mouse_y() - CY;
    float dist = sqrtf(dx * dx + dy * dy);

    for (int i = 0; i < NRINGS; i++) {
        // soft falloff: loudest right on the ring, bleeding into its neighbours
        float near = 1.0f - fabsf(dist - ring_r(i)) / BAND;
        float target = (down && near > 0) ? near : 0.0f;
        // slow attack, even slower release — the glass-harmonica swell
        float rate = (target > level[i]) ? 0.06f : 0.03f;
        level[i] = lerp(level[i], target, rate);
        if (handle[i] >= 0) note_vol(handle[i], level[i] * 7.0f);
    }
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    float dx = mouse_x() - CX, dy = mouse_y() - CY;
    float dist = sqrtf(dx * dx + dy * dy);

    // a slowly rotating glint angle so the rings read as spinning glass
    float glint = frame() * 1.7f;

    // draw outer rings first so the bright inner ones sit on top
    for (int i = NRINGS - 1; i >= 0; i--) {
        int r = ring_r(i);
        float L = level[i];
        // rim brightens with how loud the ring is singing
        int rim = L > 0.66f ? CLR_WHITE
                : L > 0.33f ? CLR_LIGHT_PEACH
                : L > 0.05f ? CLR_PINK
                            : CLR_DARKER_PURPLE;
        circ(CX, CY, r, rim);
        if (L > 0.05f) {                         // glow halo when active
            circ(CX, CY, r + 1, L > 0.4f ? CLR_LIGHT_PEACH : CLR_MAUVE);
            circ(CX, CY, r - 1, CLR_WHITE);
        }
        // the rotating highlight glint — a short bright arc on the rim
        arc(CX, CY, r, glint, glint + 26, L > 0.05f ? CLR_WHITE : CLR_DARK_PURPLE);
    }

    // the wet fingertip
    if (mouse_down(MOUSE_LEFT)) {
        circfill(mouse_x(), mouse_y(), 3, CLR_WHITE);
        circ(mouse_x(), mouse_y(), 5, CLR_LIGHT_PEACH);
    } else {
        // hint where the nearest touchable ring is, so it invites a touch
        int best = 0; float bd = 9999;
        for (int i = 0; i < NRINGS; i++) {
            float d = fabsf(dist - ring_r(i));
            if (d < bd) { bd = d; best = i; }
        }
        if (bd < BAND) circ(CX, CY, ring_r(best), CLR_INDIGO);
    }

    print_centered("GLASS HARMONICA", CX, 6, CLR_LIGHT_PEACH);
    print_centered("hold + slide in / out to play", CX, 190, CLR_MAUVE);
}
