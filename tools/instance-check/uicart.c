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
#include "keybed.h"

/* THE CART'S OWN STATE, in the exact shape tools/ctx-gen.js --target cart generates for a rack
   (see runtime/acidcandy_state.h). It is NOT the header shape above: a header's opt-in block starts
   ZEROED and runs an init function, while a cart's starts as a COPY of a compile-time template —
   because a cart's statics carry real defaults (acidcandy has 84 of them) and de_state_for hands
   back zeroed memory. A rack that booted with every tempo and level at 0 would be silently wrong in
   a way no single-instance gate can see, so `probe_boot` below is deliberately NON-ZERO. */
typedef struct { int probe_boot; int probe_n; int de_ctx_inited_; } ProbeCartState;
static ProbeCartState probe_cart_default = { .probe_boot = 42 };
#ifndef DE_CART_CTX
#define probe_cart (&probe_cart_default)
#else
static char probe_cart_key_;
static ProbeCartState *probe_cart_(void) {
    ProbeCartState *c = (ProbeCartState *)de_state_for(&probe_cart_key_, (int)sizeof(ProbeCartState));
    if (c && !c->de_ctx_inited_) { *c = probe_cart_default; c->de_ctx_inited_ = 1; }
    return c;
}
#define probe_cart probe_cart_()
#endif
#define probe_boot (probe_cart->probe_boot)
#define probe_n    (probe_cart->probe_n)

int probe_write = 0;    // set by the harness: write this into ui_wid_n on the next frame
int probe_seen  = -1;   // what ui_wid_n looked like on this frame, BEFORE any write

int cart_write = 0;     // the same handshake for the CART's own state
int cart_seen  = -1;
int cart_boot  = -1;    // what this instance's template copy left in probe_boot (must be 42)

void init(void) {}
void update(void) {
    probe_seen = ui_wid_n;
    if (probe_write) { ui_wid_n = probe_write; probe_write = 0; }
    cart_seen = probe_n;
    cart_boot = probe_boot;
    if (cart_write) { probe_n = cart_write; cart_write = 0; }
}
void draw(void) {}
