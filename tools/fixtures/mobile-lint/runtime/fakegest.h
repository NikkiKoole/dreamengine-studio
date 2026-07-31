// FIXTURE — a second cart-land header that includes fakeui.h, which includes this one back.
// A real CYCLE, on purpose: inlineRuntimeIncludes() carries a `seen` set to break it, and
// without a cycle in the fixture that guard is untestable (deleting it changes nothing).
// Mutation-tested: remove `seen.has(h)` and this recurses until the stack blows.
#include "fakeui.h"
static int fakegest_swipe(void) { return 0; }
