// probe.c — the thread-safety gate for the host→engine INPUT RING (runtime/raylib_compat.c).
//
//   bash tools/input-ring-check/run.sh      # exit 0 = PASS
//
// It #includes raylib_compat.c, so it tests the REAL ring rather than a copy of it. Nothing else in
// the repo can: the race only exists on a build where the host feeds input from one thread while
// de_frame() runs on another, which today means the AUv3 (its render block drives the frame from the
// AUDIO thread while UIKit delivers touches on the main thread) — and an AUv3 cannot be unit-tested
// from C. So the probe plays both parts itself, with a real second thread.
//
// WHY A DEDICATED PROBE AND NOT A CART. A cart cannot see this bug even when it is happening: the
// symptom is a tap landing on the wrong widget once in a while, on a device, in a plug-in. There is
// no assertion a cart could make, and "it felt fine when I tried it" is exactly the evidence that
// let the race live in the standalone app for months (harmlessly — same thread there).
//
// THE TEAR TRICK. The producer always sends a contact at (x, x + 1000). That makes a torn read
// self-evident: any consumer that ever observes y - x != 1000 has combined one event's x with
// another's y, which is precisely what unsynchronised float pairs do. Without an invariant like
// this a racing reader just sees plausible-looking numbers.
//
// WHAT IT ASSERTS:
//   1. TEAR: under a hammering producer thread, every contact the engine reports satisfies the
//      invariant — no x from one event paired with a y from the next.
//   2. COMPACT-VIEW SHIFT: walking 0..GetTouchPointCount()-1 (what ui.h does) never lands on a
//      slot that went inactive mid-walk. That was the real-world failure: the pool is indexed
//      through a compaction over `active`, so a release between the count and the read shifted
//      every later index down by one.
//   3. NO STUCK FINGERS: after the producer stops and one more frame drains, the pool is empty.
//      A stranded contact is a held note or a knob that never lets go.
//   4. THE ONE-FRAME TAP: a press and release arriving between two frames is visible to the cart
//      for exactly one frame, with mouse_pressed on that frame and mouse_released on the next.
//   5. KEYS: the same, for de_key_event (it shares the ring).
//   6. OVERFLOW SHEDS MOVES, NOT LIFTS: flood the ring without draining, and the contacts still
//      all lift once it is drained. Dropping an `ended` would strand a finger.
//
// THE NEGATIVE CONTROL. Build with -DDE_IN_RING_BYPASS and the producer thread writes the touch pool
// DIRECTLY, which is exactly what the code did before the ring existed. Checks 1 and 2 must then
// FAIL, and under -fsanitize=thread the race must be REPORTED. Without this the suite proves only
// that the assertions can pass, not that they could ever have caught anything — and these particular
// assertions are the easiest kind to fool yourself with, because a lucky race passes them.

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// raylib_compat.c is self-contained enough to include whole: it needs no studio.h and no raylib, and
// it brings its own GetTime(). Only the canvas dims come from studio.c, so the probe supplies those
// two — that the input seam needs nothing else is what makes this gate possible at all.
int de_screen_w(void) { return 320; }
int de_screen_h(void) { return 200; }
#include "../../runtime/raylib_compat.c"

static double probe_clock = 0;   // advanced per simulated frame; de_input_endframe reads GetTime()

static int failures = 0;
static void check(const char *name, int ok, const char *fmt, ...) {
    va_list ap; char detail[512];
    va_start(ap, fmt); vsnprintf(detail, sizeof detail, fmt, ap); va_end(ap);
    printf("  %s %s  — %s\n", ok ? "\xE2\x9C\x93" : "\xE2\x9C\x97", name, detail);
    if (!ok) failures++;
}

// one simulated engine frame: drain the ring, then read input the way a cart does
static void frame(void) { probe_clock += 1.0 / 60.0; de_input_beginframe(); }
static void endframe(void) { de_input_endframe(); }

// ── the hammering producer ───────────────────────────────────────────────────────────────────────
#define PROBE_FINGERS 8
#define PROBE_ROUNDS  4000
// atomic, not volatile: under -fsanitize=thread a plain volatile flag read across threads is itself
// reported as a data race — correctly, since volatile is not a barrier, which is the same point the
// ring's own comment makes. The probe must not be the only race left in the run.
static atomic_int producer_done = 0;
static void *producer(void *unused) {
    (void)unused;
#ifdef DE_IN_RING_BYPASS
    // the OLD design: the host thread mutates engine state directly. See "THE NEGATIVE CONTROL".
    #define PROBE_BEGIN de_apply_touch_begin
    #define PROBE_MOVED de_apply_touch_moved
    #define PROBE_ENDED de_apply_touch_ended
#else
    #define PROBE_BEGIN de_touch_begin
    #define PROBE_MOVED de_touch_moved
    #define PROBE_ENDED de_touch_ended
#endif
    for (int r = 0; r < PROBE_ROUNDS; r++) {
        for (int f = 0; f < PROBE_FINGERS; f++) {
            float x = (float)((r * 7 + f * 13) % 300);
            PROBE_BEGIN(f, x, x + 1000.0f);
        }
        for (int f = 0; f < PROBE_FINGERS; f++) {
            float x = (float)((r * 11 + f * 3) % 300);
            PROBE_MOVED(f, x, x + 1000.0f);
        }
        for (int f = 0; f < PROBE_FINGERS; f++) {
            float x = (float)((r * 5 + f * 17) % 300);
            PROBE_ENDED(f, x, x + 1000.0f);
        }
        de_key_event(65 + (r % 8), r & 1);
    }
    atomic_store_explicit(&producer_done, 1, memory_order_release);
    return 0;
}

int main(void) {
    printf("\xE2\x96\xB8 input ring: a second thread hammering the host seam while frames drain\n");

    // ── 1 + 2: race the ring ────────────────────────────────────────────────────────────────────
    pthread_t th;
    pthread_create(&th, 0, producer, 0);
    long observed = 0, torn = 0, shifted = 0, frames = 0, quiet = 0;
    // Run until the producer is done AND the pool has drained, but bound the tail: a stuck finger
    // must fail the next check in a second, not spin here.
    while (!atomic_load_explicit(&producer_done, memory_order_acquire)
           || (GetTouchPointCount() > 0 && quiet++ < 200)) {
        frame();
        int n = GetTouchPointCount();
        for (int i = 0; i < n; i++) {
            Vector2 p = GetTouchPosition(i);
            // A shifted compact view hands back the zeroed Vector2 de_touch_nth returns for a slot
            // that is no longer active, which no producer event can ever look like (x is 0..299
            // paired with x+1000, so y is never 0).
            if (p.y == 0.0f && p.x == 0.0f) { shifted++; continue; }
            if (p.y - p.x != 1000.0f) torn++;
            observed++;
        }
        endframe();
        frames++;
        if (frames > 2000000) break;   // safety net; the loop is bounded by PROBE_ROUNDS
    }
    pthread_join(th, 0);
    check("no torn coordinate pairs under a racing producer", torn == 0,
          "%ld contacts observed over %ld frames, %ld torn", observed, frames, torn);
    check("the compact index view never shifts mid-walk", shifted == 0,
          "%ld reads landed on a slot that went inactive", shifted);
    // A floor, not a target: the count swings from a few hundred (under -fsanitize=thread, ~10x
    // slower, where the producer finishes first) to millions on a plain -O2 build. Either way, zero
    // would mean the consumer never saw a contact and checks 1-2 proved nothing.
    check("a hammering producer was actually observed (the test is not vacuous)", observed > 100,
          "%ld contact reads over %ld frames (want > 100)", observed, frames);

    // ── 3: nothing stuck down ───────────────────────────────────────────────────────────────────
    frame(); endframe(); frame(); endframe();
    check("no stuck fingers once the storm passes", GetTouchPointCount() == 0,
          "%d contacts still active, %d events dropped",
          GetTouchPointCount(), atomic_load(&de_in_dropped));

    // ── 4: the one-frame tap ────────────────────────────────────────────────────────────────────
    // Both events arrive BETWEEN frames, which on a phone is the common case: 60 Hz is 16ms.
    de_touch_begin(1, 40.0f, 1040.0f);
    de_touch_ended(1, 40.0f, 1040.0f);
    frame();
    int seen     = GetTouchPointCount();
    int pressed  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    endframe();
    frame();
    int after    = GetTouchPointCount();
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    endframe();
    check("a tap between two frames is visible for one frame", seen == 1 && pressed,
          "frame 1: %d contact(s), mouse_pressed=%d", seen, pressed);
    check("and lifts on the next frame", after == 0 && released,
          "frame 2: %d contact(s), mouse_released=%d", after, released);

    // ── 5: keys ride the same ring ──────────────────────────────────────────────────────────────
    // Key 100, deliberately OUTSIDE the 65..72 the storm above hammers: the first version used 70,
    // which the storm had left down, so IsKeyPressed was correctly false and the probe blamed the
    // ring. A shared-state test needs its own state.
    de_key_event(100, 1);
    de_key_event(100, 0);
    frame();
    int kdown = IsKeyDown(100), kpressed = IsKeyPressed(100);
    endframe();
    frame();
    int krel = IsKeyReleased(100);
    endframe();
    check("a keypress between two frames is not swallowed", kdown && kpressed,
          "frame 1: down=%d pressed=%d", kdown, kpressed);
    check("and its release lands the next frame", krel, "frame 2: released=%d", krel);

    // ── 6: overflow sheds MOVES, never lifts ────────────────────────────────────────────────────
    // Flood without draining, the way a host that paused our render block would. The pool must still
    // come back empty: a dropped `ended` is a finger held down forever.
    int before_drop = atomic_load(&de_in_dropped);
    for (int f = 0; f < PROBE_FINGERS; f++) de_touch_begin(f, 10.0f + f, 1010.0f + f);
    for (int r = 0; r < 4000; r++)
        for (int f = 0; f < PROBE_FINGERS; f++) de_touch_moved(f, (float)(r % 300), (float)(r % 300) + 1000.0f);
    for (int f = 0; f < PROBE_FINGERS; f++) de_touch_ended(f, 10.0f + f, 1010.0f + f);
    for (int i = 0; i < 40; i++) { frame(); endframe(); }
    int dropped = atomic_load(&de_in_dropped) - before_drop;
    check("an overflowing ring still lets every finger lift", GetTouchPointCount() == 0,
          "%d contacts stuck after %d dropped events", GetTouchPointCount(), dropped);
    check("and the flood really did overflow (the test is not vacuous)", dropped > 0,
          "%d events dropped", dropped);

    printf(failures == 0 ? "\nPASS — the input ring is safe across threads.\n"
                         : "\n%d check(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
