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
