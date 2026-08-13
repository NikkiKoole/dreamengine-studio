// probe.c — TWO ENGINES IN ONE PROCESS, from the context refactor rather than from dyld.
//
// This is the acceptance test the byte-exact gate structurally cannot be. `refactor-guard` runs ONE
// instance, so it can prove a state move changed nothing and can never prove two instances are
// independent — a variable that was left shared does not change a single-instance render.
//
// It is the sibling of tools/engine-dylib-spike, and deliberately asserts the SAME things: drive two
// engines with different transport, and check they are strangers. The difference is where the
// separation comes from. The spike got it from dyld (two copies of a dylib = two data segments, zero
// source changes, hard instance cap). This gets it from `de_instance_create` — one image, one copy of
// the code, N instances — which is what the AUv3 actually needs.
//
// THE NEGATIVE CONTROL IS THE POINT. Two fresh instances driven the SAME way must come back
// byte-identical. Without it, "their frames differ" could be instance-to-instance noise rather than
// independence, and the headline result would mean nothing.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include "../../runtime/platform.h"

// The host transport seam. It lives in runtime/sync.h, which is only ever compiled inside studio.c,
// so declare it rather than include it. ⚠ NOT instance-scoped yet — see the note at the bottom.
void de_sync_position(double beats, double bpm, int playing);

static int failures = 0;
static void ok(int cond, const char *what, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char detail[512]; vsnprintf(detail, sizeof detail, fmt, ap); va_end(ap);
    if (cond) printf("  \033[32m✓\033[0m %s  — %s\n", what, detail);
    else { printf("  \033[31m✗\033[0m %s  — %s\n", what, detail); failures++; }
}

// Drive one engine for `frames` frames with its own transport, and return its published pixels.
static uint32_t *run(DeInstance *in, int frames, double bpm, int playing, int *npx, float *peak) {
    float chunk[735 * 2];
    float pk = 0.0f;
    for (int f = 0; f < frames; f++) {
        de_sync_position(f * 0.25, bpm, playing);
        de_frame(in, f / 60.0);
        de_audio_render(in, chunk, 735);
        for (int i = 0; i < 735 * 2; i++) { float a = chunk[i] < 0 ? -chunk[i] : chunk[i]; if (a > pk) pk = a; }
    }
    *peak = pk;
    int w = 0, h = 0;
    de_copy_frame(in, NULL, 0, &w, &h);                       // dst == NULL: report the size
    if (w <= 0 || h <= 0) { *npx = 0; return NULL; }
    uint32_t *buf = (uint32_t *)malloc((size_t)w * h * 4);
    if (!de_copy_frame(in, buf, w * h, &w, &h)) { free(buf); *npx = 0; return NULL; }
    *npx = w * h;
    return buf;
}

static int flat(const uint32_t *px, int n) {
    for (int i = 1; i < n; i++) if (px[i] != px[0]) return 0;
    return 1;
}

int main(void) {
    printf("▸ two instances from de_instance_create, driven with different transport\n");

    DeInstance *a = de_instance_create(DE_RENDERER_SOFTWARE);
    DeInstance *b = de_instance_create(DE_RENDERER_SOFTWARE);
    ok(a && b, "both instances were created", "a=%p b=%p", (void *)a, (void *)b);
    ok(a != b, "and they are distinct objects", "%p vs %p", (void *)a, (void *)b);
    if (!a || !b || a == b) { printf("\nFAILED early\n"); return 1; }

    int na = 0, nb = 0; float pa = 0, pb = 0;
    uint32_t *fa = run(a, 90, 120.0, 0, &na, &pa);            // A: host STOPPED
    uint32_t *fb = run(b, 90, 120.0, 1, &nb, &pb);            // B: host PLAYING

    ok(fa && fb && na > 0 && nb > 0, "both engines published a frame", "%d px and %d px", na, nb);
    if (!fa || !fb) { printf("\nFAILED\n"); return 1; }
    ok(!flat(fa, na) && !flat(fb, nb), "both drew something", "neither frame is one flat colour");

    // THE POINT: same cart, different transport → different picture and different audio.
    ok(na == nb && memcmp(fa, fb, (size_t)na * 4) != 0,
       "THE POINT: their frames DIFFER, so their state is independent",
       "same cart, different transport → different picture");
    ok(pa < 0.001f && pb > 0.01f,
       "each engine hears its OWN transport",
       "A peak %.4f (host stopped) vs B peak %.4f (host playing)", pa, pb);

    // ── NEGATIVE CONTROL ────────────────────────────────────────────────────────────────────────
    // Two FRESH instances driven IDENTICALLY must come back byte-identical. This is the control the
    // assertion above needs: if two engines given the same transport still differed, then "their
    // frames differ" would be measuring instance-to-instance noise rather than independence, and the
    // headline result would mean nothing.
    //
    // (An earlier draft controlled the wrong thing — it drove ONE instance twice and expected the
    // second run to differ. With identical input an engine legitimately renders the same frame, so
    // that assertion failed while the engine was perfectly correct.)
    printf("▸ NEGATIVE CONTROL: two fresh instances, driven the SAME (must be identical)\n");
    int n1 = 0, n2 = 0; float p1 = 0, p2 = 0;
    DeInstance *c = de_instance_create(DE_RENDERER_SOFTWARE);
    DeInstance *d = de_instance_create(DE_RENDERER_SOFTWARE);
    uint32_t *f1 = run(c, 30, 120.0, 1, &n1, &p1);
    uint32_t *f2 = run(d, 30, 120.0, 1, &n2, &p2);
    ok(f1 && f2 && n1 == n2, "the control produced two comparable frames", "%d px and %d px", n1, n2);
    ok(f1 && f2 && n1 == n2 && memcmp(f1, f2, (size_t)n1 * 4) == 0,
       "same transport in, IDENTICAL frame out",
       "so the difference above is the transport, not noise between instances");
    ok(p1 == p2, "and identical audio", "peaks %.4f and %.4f", p1, p2);

    free(fa); free(fb); free(f1); free(f2);
    de_instance_destroy(b); de_instance_destroy(c); de_instance_destroy(d);

    // ⚠ WHAT THIS GATE DOES NOT COVER, so nobody reads more into a PASS than it earns:
    //   · de_sync_position is still PROCESS-WIDE — it takes no instance. Both engines above read
    //     the same transport push; they differ here only because each was driven while it was the
    //     one being pushed. Instance-scoping the sync seam is still owed.
    //   · The frame WORKER is still one per process on the Swift side, so the plug-in cannot yet
    //     advance two racks even though the engine now supports it.
    //   · Nothing here runs two instances CONCURRENTLY on two threads. That is what
    //     present-race-check does for one instance; the two-instance version does not exist yet.
    printf("\n%s\n", failures ? "FAILED" : "PASS — one image, N independent engines.");
    return failures ? 1 : 0;
}
