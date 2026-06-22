// runtime/spec.h — the cart-LOGIC spec harness API (the gameplay twin of tune-check).
//
// Dev-only test scaffolding, live ONLY under -DDE_SPEC (the `node tools/spec.js` runner compiles with
// it; a normal build has none of this and pays nothing). A cart defines `void spec(void)` — weak-stubbed
// otherwise, so carts without one are SKIPPED, not failed — and drives the loop HEADLESSLY with step()
// + expect(). It reuses the engine's existing inject-input machinery (the same buffers behind
// --replay/--script). Logic only: no pixel/audio asserts (ui-audit + the audio gates own those).
//
// This is deliberately NOT part of the player-facing studio.h API: it gets no autocomplete / hover /
// help-tab wiring (the four-place treatment) because it only exists in the spec build. Documented here
// and in docs/design/spec-harness.md.
//
// v1 surface (built when streetlab needed it). NOT yet built — add when a cart first needs them:
//   frame(int n)  — update + a headless DRAW, for probe asserts on procedural generators
//   btn_down/up   — press via the default button binding (step reads key_inject directly for now)
#ifndef DE_SPEC_H
#define DE_SPEC_H

void spec(void);                                       // the cart overrides this; weak stub in studio.c
void step(int n);                                      // advance n frames: init() (lazily, once) + update(), NO draw
void expect(int cond, const char *msg);                // record one assertion (prints JSONL pass/fail)
void expect_eq(long got, long want, const char *msg);  // like expect(), with a got/want failure message
void key_down(int k);                                  // hold a raw key (studio KEY_* or a char) from the next step on
void key_up(int k);                                    // release it

#endif
