/* de:meta
{
  "title": "voice stress",
  "status": "active",
  "kind": [
    "probe",
    "tool"
  ],
  "teaches": [],
  "description": "Hold up to SOUND_VOICES (32) of the hungriest engines at once and watch/listen for crackle. Crackle is a real-time audio-thread underrun, so the honest test is to RUN THIS AND LISTEN; the on-screen fps + voice count + a constant-speed sweep reveal CPU starvation too. UP/DOWN sets the held-voice count (starts MAXED — it's a stress test), [ / ] cycle the engine (the self-oscillating BOWED/BRASS/REED/PIPE waveguides are the true worst case — full delay-line DSP every sample, per voice), R retriggers. Built to verify the 16->32 voice bump: on a dev Mac the full 32-voice BOWED wall costs ~10% of one core."
}
de:meta */
#include "studio.h"
#include <math.h>
// VOICE STRESS — hold up to SOUND_VOICES of the HUNGRIEST engines at once and watch for crackle.
// Crackle is a REAL-TIME audio-thread underrun (the callback misses its buffer deadline), so the
// honest test is to RUN THIS IN THE EDITOR AND LISTEN. The on-screen fps + voice count tell the
// story too: if the audio thread starves the machine, the main-loop fps drops and the sweep stutters.
//   UP/DOWN = target voice count (starts MAXED — it's a stress test)   [ / ] = cycle engine   R = retrigger
// The default engines are the self-oscillating waveguides (BOWED/BRASS/REED/PIPE): they HOLD while
// gated and run a full delay-line + nonlinearity EVERY sample per voice — the true worst case.

#define NV 32                          // = SOUND_VOICES; the pool ceiling
int   handle[NV];
int   active = 0, target = NV, eng = 0;

const int   ENG[]  = { INSTR_BOWED, INSTR_BRASS, INSTR_REED, INSTR_PIPE, INSTR_PIANO, INSTR_GUITAR };
const char *ENGN[] = { "BOWED", "BRASS", "REED", "PIPE", "PIANO", "GUITAR" };
const int   DECAYS[] = { 0, 0, 0, 0, 1, 1 };   // 1 = struck/plucked: rings out & goes silent, so it must
#define NENG 6                                  // be RE-struck to stay audible (the self-osc ones just hold)
int   frames = 0;

void setup_engine(void) { instrument(5, ENG[eng], 4, 0, 7, 400); }   // sustaining; holds while gated

void kill_all(void) {
    for (int i = 0; i < NV; i++) { if (handle[i] > 0) note_off(handle[i]); handle[i] = 0; }
    active = 0;
}

void init(void) {
    for (int i = 0; i < NV; i++) handle[i] = 0;
    setup_engine();
}

void update(void) {
    if (keyp(KEY_UP))    target = target < NV ? target + 1 : NV;
    if (keyp(KEY_DOWN))  target = target > 0  ? target - 1 : 0;
    if (keyp(']') || keyp(KEY_RIGHT)) { eng = (eng + 1) % NENG; setup_engine(); kill_all(); }
    if (keyp('[') || keyp(KEY_LEFT))  { eng = (eng + NENG - 1) % NENG; setup_engine(); kill_all(); }
    if (keyp('R')) kill_all();

    // reconcile the held-voice count to target — each voice a distinct pitch (a fat cluster across
    // ~4 octaves) so all NV are genuinely sounding at once, not folded onto one note.
    while (active < target) { handle[active] = note_on(36 + (active * 5) % 48, 5, 2); active++; }
    while (active > target) { active--; if (handle[active] > 0) note_off(handle[active]); handle[active] = 0; }

    // plucked/struck engines ring out & fall silent — re-strike all held voices ~twice a second so
    // the test stays AUDIBLE and keeps ~target voices genuinely ringing (the self-osc engines hold,
    // so they're left alone). This is why PIANO/GUITAR looked "silent": they only got one pluck.
    frames++;
    if (DECAYS[eng] && active > 0 && frames % 30 == 0)
        for (int i = 0; i < active; i++) { if (handle[i] > 0) note_off(handle[i]); handle[i] = note_on(36 + (i * 5) % 48, 5, 2); }

#ifdef DE_TRACE
    watch("voices", "%d", active);
    watch("fps", "%d", fps());
#endif
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    font(FONT_NORMAL);
    print("VOICE STRESS", 8, 6, CLR_WHITE);
    font(FONT_SMALL);
    print(str("engine: %s   [ / ] cycle", ENGN[eng]), 8, 20, CLR_LIGHT_GREY);
    print(str("%d / %d voices   UP/DOWN", active, NV), 8, 32, CLR_YELLOW);
    int f = fps();
    print(str("fps %d", f), 8, 44, f >= 58 ? CLR_GREEN : f >= 45 ? CLR_ORANGE : CLR_RED);
    print("listen for crackle — that's the real test. R = retrigger", 8, SCREEN_H - 10, CLR_DARK_GREY);

    // one animated bar per active voice — a dense wall when maxed
    for (int i = 0; i < active; i++) {
        int x = 8 + (i % 16) * 19, y = 64 + (i / 16) * 56;
        float ph = now() * 120.0f + i * 37.0f;
        int h = 6 + (int)((sinf(ph * 0.0174533f) * 0.5f + 0.5f) * 40);
        rectfill(x, y + 46 - h, 15, h, i < NV / 2 ? CLR_GREEN : CLR_LIME_GREEN);
    }
    // a constant-speed sweep: if the main loop hitches (audio starving the CPU), it visibly jumps
    int sx = (int)(now() * 90.0f) % SCREEN_W;
    rectfill(sx, SCREEN_H - 16, 3, 4, CLR_PINK);
}
