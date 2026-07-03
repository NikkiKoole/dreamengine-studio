// bundle.c — TWO CARTS, ONE BINARY spike (docs/design/share-panel.md, engine seam #1/#5).
//
// acidrack + yachtrack compiled side by side (each TU built with -Ddraw=<slug>_draw
// -Dupdate=<slug>_update so their entry points don't collide; everything else in a
// cart is `static`, so it links clean). This shim IS the cart from studio.c's point
// of view — it owns the real init/update/draw and dispatches to the active rack.
//
// TAB switches racks (neither cart claims it). note_off_all() is the band panic on
// switch so hanging notes don't bleed across. DE_BUNDLE_AUTOSWITCH=<frame> switches
// once at that frame — the deterministic headless proof hook.
#include "studio.h"
#include <stdlib.h>

void acid_init(void);   void acid_update(void);  void acid_draw(void);
void yacht_update(void); void yacht_draw(void);
static void yacht_init(void) {}   // yachtrack defines no init()

static int active     = 0;        // 0 = acidrack, 1 = yachtrack
static int booted[2]  = { 0, 0 };
static int autoswitch = 0;

static void activate(int i) {
    if (i == active) return;
    note_off_all();
    active = i;
    if (!booted[i]) { booted[i] = 1; if (i == 1) yacht_init(); else acid_init(); }
}

// ── slot partition: yacht plays in engine slots 30..39 ──────────────────────
// Both racks define instrument slots by number and the ranges collide (acid 5..29,
// yacht 5..14) — the engine's slot bank is global, so the last rack to configure a
// slot wins and the OTHER rack triggers the wrong sounds (heard in the first build:
// acid's 909 came back as yacht's strat). Fix: yacht's TU compiles with every
// slot-taking call renamed to these wrappers, which shift cart-defined slots (≥5)
// up by 25. Raw waves 0..4 pass through. The manifest generator would emit these.
static int yoff(int s) { return s < 5 ? s : s + 25; }
void yacht_instrument(int s, int w, int a, int d, int su, int r)          { instrument(yoff(s), w, a, d, su, r); }
void yacht_instrument_duty(int s, float d)                                { instrument_duty(yoff(s), d); }
void yacht_instrument_lfo(int s, int wh, int de, float r, float dp)       { instrument_lfo(yoff(s), wh, de, r, dp); }
void yacht_instrument_filter(int s, int m, int c, int r)                  { instrument_filter(yoff(s), m, c, r); }
void yacht_instrument_env(int s, int wh, int de, int a, int d, float am)  { instrument_env(yoff(s), wh, de, a, d, am); }
void yacht_instrument_harmonics(int s, float x)                           { instrument_harmonics(yoff(s), x); }
void yacht_instrument_timbre(int s, float x)                              { instrument_timbre(yoff(s), x); }
void yacht_instrument_morph(int s, float x)                               { instrument_morph(yoff(s), x); }
void yacht_schedule_hit(int dly, int midi, int in, int vol, int dur)      { schedule_hit(dly, midi, yoff(in), vol, dur); }

void init(void) {
    const char *a = getenv("DE_BUNDLE_AUTOSWITCH");
    if (a) autoswitch = atoi(a);
    booted[0] = 1;
    acid_init();
}

void update(void) {
    if (keyp(KEY_TAB)) activate(!active);
    if (autoswitch && frame() == autoswitch) activate(!active);
    if (active == 0) acid_update(); else yacht_update();
}

void draw(void) {
    if (active == 0) acid_draw(); else yacht_draw();
}
