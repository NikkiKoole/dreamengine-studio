// FIXTURE — a stand-in cart-land library header for mobile-lint --selfcheck. Never compiled.
// mobile-lint INLINES quote-included runtime/ headers before scanning, so a cart whose entire
// input story lives in a library's widgets still ranks touch-ready (the mobile-web-notes §5.4
// contract: an all-ui.h cart lints green by construction). This header supplies that touch read.
#include "fakegest.h"   // completes the include CYCLE — see fakegest.h
static int fakeui_button(int x, int y, int w, int h) {
  for (int i = 0; i < touch_count(); i++) {
    int tx = touch_x(i), ty = touch_y(i);
    if (tx >= x && ty >= y) return 1;
  }
  return 0;
}
