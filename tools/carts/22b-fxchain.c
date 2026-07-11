/* de:meta
{
  "slug": "22b-fxchain",
  "title": "22b. the effects chain",
  "status": "active",
  "created": "2026-07-11",
  "kind": [
    "tutorial"
  ],
  "teaches": [],
  "description": "A note repeats every half-second; flip ECHO, REVERB and CRUSH on and off with keys 1/2/3 to hear the shared effects bus colour the SAME sound."
}
de:meta */
#include "studio.h"

// THE EFFECTS BUS. Instruments (carts 20-22) make a voice; effects COLOUR the
// whole mix after the fact. Here one plain triangle note repeats forever and you
// toggle three shared bus effects on top of it, so you hear ONLY what each adds:
//   ECHO   1  -> repeating tail (a delay pedal)
//   REVERB 2  -> a room/hall around the note
//   CRUSH  3  -> gritty lo-fi bit reduction
//
// *** THE ONE RULE: effects are SET-AND-HOLD. ***
// echo()/reverb()/crush() REBUILD the bus DSP each time you call them. Calling one
// every frame (60x/sec) doesn't crash — it STUTTERS, silently. So we only (re)apply
// an effect on the single frame its toggle CHANGES. That is the whole point of the
// `was[]` mirror below: compare now-vs-last, act only on the edge.

enum { ECHO, REVERB, CRUSH, N_FX };

bool on[N_FX];              // is each effect currently ON?
bool was[N_FX];             // what it was LAST frame — the edge detector
int   pulse;                // frames left of the "a note just played" flash

const char *FX_NAME[N_FX] = { "ECHO", "REVERB", "CRUSH" };

void init(void) {
    // a soft triangle patch to hear the effects clearly (slot 5, ours to define)
    instrument(5, INSTR_TRI, 4, 60, 3, 200);
    bpm(120);               // 120 bpm -> one beat = 0.5s, so every(1) = a note twice a second
    // start with everything OFF and set to a clean bypass so the first note is DRY.
    echo(300, 0.0f, 0.5f);  // feedback 0 = no repeats (bypass)
    reverb(0.0f, 0.5f);     // size 0    = no room     (bypass)
    crush(16.0f, 1.0f, 0.0f); // mix 0   = fully clean (bypass)
}

void update() {
    // toggle each effect with number keys 1 / 2 / 3
    if (keyp('1')) on[ECHO]   = !on[ECHO];
    if (keyp('2')) on[REVERB] = !on[REVERB];
    if (keyp('3')) on[CRUSH]  = !on[CRUSH];

    // APPLY ON CHANGE ONLY. Each `if` fires on exactly one frame: the moment the
    // toggle flipped. Turning an effect OFF = re-apply it in its bypass config.
    if (on[ECHO]   != was[ECHO])   echo(300, on[ECHO] ? 0.55f : 0.0f, 0.5f);
    if (on[REVERB] != was[REVERB]) reverb(on[REVERB] ? 0.8f : 0.0f, 0.5f);
    if (on[CRUSH]  != was[CRUSH])  crush(on[CRUSH] ? 4.0f : 16.0f, 1.0f, on[CRUSH] ? 0.8f : 0.0f);

    // remember this frame's state for next frame's edge test
    for (int i = 0; i < N_FX; i++) was[i] = on[i];

    // the repeating note — one plain triangle, unchanged, so only the FX vary the sound
    if (every(1)) { note(60, 5, 5); pulse = 10; }
    if (pulse > 0) pulse--;
}

void draw() {
    cls(CLR_DARK_BLUE);
    print("THE EFFECTS CHAIN", 8, 8, CLR_WHITE);
    print("one note, three effects on the bus", 8, 20, CLR_LIGHT_GREY);
    print("keys 1 2 3 toggle each effect", 8, 30, CLR_LIGHT_GREY);

    // the beating note — a dot that pulses each time note() fires
    circfill(SCREEN_W / 2, 60, pulse > 0 ? 8 : 4, pulse > 0 ? CLR_YELLOW : CLR_DARK_GREY);
    print("note", SCREEN_W / 2 - 10, 74, CLR_DARK_GREY);

    // three effect tiles: green + label when ON, grey when OFF
    for (int i = 0; i < N_FX; i++) {
        int x = 24 + i * 96, y = 108, w = 80, h = 60;
        int col = on[i] ? CLR_GREEN : CLR_DARK_GREY;
        rectfill(x, y, w, h, on[i] ? CLR_DARK_GREEN : CLR_DARKER_BLUE);
        rect(x, y, w, h, col);
        print(str("%d", i + 1), x + 6, y + 6, CLR_LIGHT_GREY);
        print(FX_NAME[i], x + 20, y + 24, col);
        print(on[i] ? "ON" : "off", x + 28, y + 40, on[i] ? CLR_WHITE : CLR_DARK_GREY);
    }

    print("reconfigure ONLY when a toggle CHANGES,", 8, 182, CLR_DARK_GREY);
    print("never every frame (60x = stutter).", 8, 192, CLR_DARK_GREY);
}
