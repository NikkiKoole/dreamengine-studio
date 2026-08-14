// A minimal CART for the ui.h seam probe. Everything that touches ui.h's state happens in update(),
// i.e. inside de_frame's scope, because that is what selects the instance — reading it from main()
// would resolve to whichever instance the thread last entered, which is exactly the mistake the
// seam exists to make impossible.
#include "studio.h"
#include "ui.h"
// the other opted-in headers, so the OPT-IN path is at least compile-checked for each of them.
// (Only ui.h's behaviour is asserted below; these prove their fork expands and builds both ways.)
#include "cursor.h"
#include "tr909.h"
#include "drumkit.h"
#include "tr808.h"
#include "gestures.h"
#include "solo.h"
#include "radio.h"

int probe_write = 0;    // set by the harness: write this into ui_wid_n on the next frame
int probe_seen  = -1;   // what ui_wid_n looked like on this frame, BEFORE any write

void init(void) {}
void update(void) {
    probe_seen = ui_wid_n;
    if (probe_write) { ui_wid_n = probe_write; probe_write = 0; }
}
void draw(void) {}
