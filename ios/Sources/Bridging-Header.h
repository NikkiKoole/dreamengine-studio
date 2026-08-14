// Exposes the C engine seam + save API to Swift.
// Phase 2: the REAL dreamengine (engine.h → studio.c/raylib_compat.c). The spike stand-ins
// (canvas.{c,h}/audio.{c,h}) now live in ios/history/ beside the screenshots they produced —
// still kept as spike history, just no longer sitting in Sources/ where a future spec could glob
// them back in. They had to move: canvas.h declared `const uint8_t *de_framebuffer(void)` against
// the real seam's `const uint32_t *de_framebuffer(DeInstance *)` — wrong arity AND return type,
// which is undefined behaviour no compiler can see across translation units, and is exactly how
// face.h's stale `extern de_resize` crashed with in = 0xa7.
#include "engine.h"
#include "save.h"
