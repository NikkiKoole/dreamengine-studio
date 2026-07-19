/* de:meta
{
  "slug": "ribbonpad",
  "title": "Ribbon",
  "status": "active",
  "created": "2026-07-19",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "subtractive-synth"
  ],
  "description": {
    "summary": "A touchscreen MPE-style expressive pad: each finger is its own acid voice - slide left/right for pitch (snapped to a minor-pentatonic, gliding between notes), up/down for timbre (the TB-303 diode filter opening up). Polyphonic; mouse-playable too.",
    "detail": "The point (docs/design/midi-out.md - 'MPE as INPUT'): per-note expression with NO MIDI wire, because the cart drives its own voices. Each finger (pointer.h merges the desktop mouse in as one finger) grabs a note_on handle; horizontal drag rides note_pitch (scale-snapped + note_glide, so it always sounds musical and slides like a 303), vertical drag rides note_cutoff/note_res on a FILTER_DIODE saw - open + squelchy up top, dark + closed at the bottom, with a touch of note_drive for the scream near the top. That's exactly MPE's per-note pitch+timbre, addressed by a voice handle instead of a MIDI channel. iOS pressure isn't available from finger touch, so the Y axis carries timbre (the standard proxy). Each voice trails a comet ribbon of its recent glide.",
    "controls": "touch or click-drag anywhere on the pad = play a voice (up to 10 fingers). Horizontal = pitch, vertical = timbre. Release = note off."
  }
}
de:meta */
// ribbonpad — a touchscreen MPE-style expressive pad (polyphonic acid voices).
//
// One finger = one voice. No MIDI: the cart drives its OWN voices, so per-note
// expression is just note_on() + note_pitch()/note_cutoff()/note_res() on the
// handle — the same data MPE encodes, addressed by a voice handle instead of a
// channel. pointer.h folds the desktop mouse in as a synthetic finger, so this
// is mouse-playable in the editor and multitouch on iOS from one code path.
// Voice = a saw through FILTER_DIODE (the engine's real TB-303 diode-ladder).
// See docs/design/midi-out.md ("MPE as input").
#include "studio.h"
#include "pointer.h"
#include <string.h>
#include <stdio.h>

#define W SCREEN_W
#define H SCREEN_H
#define VOICE     5             // instrument slot for the acid voice
#define SCALE     SCALE_PENTA_MIN
#define BASE_OCT  3
#define NDEG      15            // scale-degree zones across the width (~3 octaves penta)
#define GLIDE_MS  55            // portamento between zones — the 303 slide
#define TRAIL     16            // comet-ribbon length (frames of history)

static const int PAD_TOP = 24;
#define PAD_BOT (H - 14)
#define PAD_H  (PAD_BOT - PAD_TOP)

static const char *NAMES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

// per-finger voice — id FIRST (the pointer.h contract)
typedef struct {
  int id, mode;
  int handle;                   // note_on handle, -1 = none
  int zone;                     // current scale-degree zone (for retrigger-free glide)
  int ci;                       // colour-ramp index
  float x, y;
  float tx[TRAIL], ty[TRAIL];   // trail ring buffer
  int th;                       // trail head
} Voice;
static Voice V[PTR_MAX];

// neon ramps: {trail(dark), outer, mid, core} — one per finger slot, cycled
static const int RAMP[4][4] = {
  { CLR_DARK_BLUE,   CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_WHITE },        // cyan
  { CLR_DARK_PURPLE, CLR_MAUVE,     CLR_PINK,       CLR_WHITE },        // pink
  { CLR_DARK_ORANGE, CLR_ORANGE,    CLR_YELLOW,     CLR_LIGHT_YELLOW }, // amber
  { CLR_DARK_GREEN,  CLR_MEDIUM_GREEN, CLR_LIME_GREEN, CLR_WHITE },     // green
};

static float zx(int z) { return (float)z * (float)W / (float)NDEG; }
static int zone_of(float x) {
  int z = (int)(x / W * NDEG);
  return z < 0 ? 0 : (z >= NDEG ? NDEG - 1 : z);
}
static int midi_of(int zone) { return degree(SCALE, BASE_OCT, zone); }
static float openness(float y) {                 // 0 at the bottom .. 1 at the top
  float o = 1.0f - (y - PAD_TOP) / (float)PAD_H;
  return o < 0 ? 0 : (o > 1 ? 1 : o);
}

// drive a held voice's timbre from its Y — the "pressure/timbre" axis
static void apply_timbre(Voice *v) {
  float o = openness(v->y);
  note_cutoff(v->handle, (int)(220 + o * o * 3600));   // 220Hz closed .. ~3.8kHz open
  note_res(v->handle, 7.0f + 5.0f * o);                // squelchy, more resonant open
  note_drive(v->handle, 0.20f + 0.45f * o);            // a little scream near the top
}

void init(void) {
  PTR_CLEAR(V);
  instrument(VOICE, INSTR_SAW, 4, 500, 6, 160);        // held saw: fast attack, sustain
  instrument_filter(VOICE, FILTER_DIODE, 800, 10);     // the acid filter (per-note overrides live)
  instrument_drive(VOICE, 0.25f);
}

void update(void) {
  // active fingers → drive a voice each
  for (int i = 0; i < touch_count(); i++) {
    int id = touch_id(i);
    float tx = touch_x(i), ty = touch_y(i);
    if (ty < PAD_TOP) ty = PAD_TOP;                     // keep play inside the pad
    if (ty > PAD_BOT) ty = PAD_BOT;
    bool fresh;
    Voice *v = PTR_ACQUIRE(V, id, &fresh);
    if (!v) continue;                                  // pool full
    if (fresh) {
      int slot = (int)(v - V);
      *v = (Voice){ .id = id, .ci = slot % 4, .handle = -1, .th = 0 };
      v->x = tx; v->y = ty;
      for (int k = 0; k < TRAIL; k++) { v->tx[k] = tx; v->ty[k] = ty; }
      v->zone = zone_of(tx);
      v->handle = note_on(midi_of(v->zone), VOICE, 5);
      note_glide(v->handle, GLIDE_MS);                 // slide, don't retrigger, between zones
      apply_timbre(v);
    } else {
      v->x = tx; v->y = ty;
      int z = zone_of(tx);
      if (z != v->zone) { v->zone = z; note_pitch(v->handle, (float)midi_of(z)); }
      apply_timbre(v);                                 // note_* are live-ride (fx-lint exempt)
    }
    v->th = (v->th + 1) % TRAIL;                        // push trail
    v->tx[v->th] = tx; v->ty[v->th] = ty;
  }
  // lifted fingers → release the voice
  for (int i = 0; i < touch_ended_count(); i++) {
    Voice *v = PTR_FIND(V, touch_ended_id(i));
    if (v) { if (v->handle >= 0) note_off(v->handle); v->id = PTR_NONE; }
  }

#ifdef DE_TRACE
  int live = 0; for (int i = 0; i < PTR_MAX; i++) if (V[i].id != PTR_NONE) live++;
  watch("voices", "%d", live);
#endif
}

static void glow(int x, int y, float r, int outer, int mid, int core) {
  circfill(x, y, (int)(r * 1.7f), outer);
  circfill(x, y, (int)r,          outer);
  circfill(x, y, (int)(r * 0.60f), mid);
  circfill(x, y, (int)(r * 0.30f), core);
}

void draw(void) {
  cls(CLR_BLACK);

  // --- soft surface: faint scale-degree lines, brighter octave roots + labels ---
  for (int z = 0; z <= NDEG; z++) {
    int x = (int)zx(z);
    if (z < NDEG) {
      int midi = midi_of(z);
      bool root = (midi % 12) == (midi_of(0) % 12);    // same pitch-class as the low root
      line(x, PAD_TOP, x, PAD_BOT, root ? CLR_INDIGO : CLR_DARKER_BLUE);
      if (root) {
        char lbl[8]; snprintf(lbl, sizeof lbl, "%s%d", NAMES[midi % 12], midi / 12 - 1);
        font(FONT_TINY);
        print(lbl, x + 2, PAD_BOT - 6, CLR_DARK_GREY);
      }
    } else {
      line(x - 1, PAD_TOP, x - 1, PAD_BOT, CLR_DARKER_BLUE);
    }
  }

  // --- highlight the zone each live finger is over ---
  for (int i = 0; i < PTR_MAX; i++) {
    if (V[i].id == PTR_NONE) continue;
    int z = V[i].zone, x0 = (int)zx(z);
    rectfill(x0, PAD_TOP, (int)zx(z + 1) - x0, PAD_H, CLR_DARKER_PURPLE);
  }

  // --- the voices: comet ribbon then the glowing orb + note label ---
  for (int i = 0; i < PTR_MAX; i++) {
    Voice *v = &V[i];
    if (v->id == PTR_NONE) continue;
    const int *rp = RAMP[v->ci];
    for (int k = 1; k <= TRAIL; k++) {
      int idx = (v->th - TRAIL + k + TRAIL * 2) % TRAIL; // oldest → newest
      float g = (float)k / TRAIL;                        // 0 far .. 1 near
      circfill((int)v->tx[idx], (int)v->ty[idx], (int)(2 + 4 * g), (k > TRAIL / 2) ? rp[1] : rp[0]);
    }
    float o = openness(v->y);
    float r = 5.0f + 11.0f * o;
    glow((int)v->x, (int)v->y, r, rp[1], rp[2], rp[3]);
    int midi = midi_of(v->zone);
    char nm[8]; snprintf(nm, sizeof nm, "%s%d", NAMES[midi % 12], midi / 12 - 1);
    font(FONT_SMALL);
    print(nm, (int)v->x - (int)strlen(nm) * 2, (int)v->y - (int)r - 8, rp[3]);
  }

  // --- idle hint ---
  bool any = false;
  for (int i = 0; i < PTR_MAX; i++) if (V[i].id != PTR_NONE) any = true;
  if (!any) {
    font(FONT_SMALL);
    const char *h = "touch or drag to play";
    print(h, W / 2 - (int)strlen(h) * 2, PAD_TOP + PAD_H / 2 - 3, CLR_MEDIUM_GREY);
  }

  // --- chrome: title + axis hints ---
  rectfill(0, 0, W, PAD_TOP - 2, CLR_BLACK);
  font(FONT_COMIC);
  print("RIBBON", 6, 3, CLR_LIGHT_GREY);
  font(FONT_TINY);
  print("expressive pad  -  one finger = one voice", 90, 8, CLR_MEDIUM_GREY);
  print("open", 2, PAD_TOP + 2, CLR_DARK_GREY);
  print("timbre", 2, (PAD_TOP + PAD_BOT) / 2, CLR_DARK_GREY);
  print("dark", 2, PAD_BOT - 8, CLR_DARK_GREY);
  font(FONT_SMALL);
  const char *ax = "<  slide = pitch  >";
  print(ax, W / 2 - (int)strlen(ax) * 2, H - 8, CLR_MEDIUM_GREY);
}
