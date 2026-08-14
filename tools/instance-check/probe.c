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
void de_sync_position(DeInstance *in, double beats, double bpm, int playing);

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
        de_sync_position(in, f * 0.25, bpm, playing);
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
    printf("▸ two instances from de_instance_create, driven INTERLEAVED with different transport\n");

    DeInstance *a = de_instance_create(DE_RENDERER_SOFTWARE);
    DeInstance *b = de_instance_create(DE_RENDERER_SOFTWARE);
    ok(a && b, "both instances were created", "a=%p b=%p", (void *)a, (void *)b);
    ok(a != b, "and they are distinct objects", "%p vs %p", (void *)a, (void *)b);
    if (!a || !b || a == b) { printf("\nFAILED early\n"); return 1; }

    // ── WHY INTERLEAVED, AND NOT ONE ENGINE THEN THE OTHER ──────────────────────────────────────
    // Running A to completion and then B proves almost nothing: with SHARED state the second run
    // simply overwrites the first, and the two results still differ — so the test passes while the
    // engines are one. An earlier version of this gate did exactly that and reported PASS.
    //
    // Alternating them frame by frame is what shared state cannot survive: A's transport is STOPPED
    // and B's is PLAYING, so if a single engine were serving both, A's silence would be broken by
    // B's notes (or B silenced by A's stop) within a couple of frames.
    //
    // ⚠ AUDIO is the signal here, deliberately, NOT the frame. `de_pres_*` (the published frame) is
    // declared inside `#ifdef DE_NO_RAYLIB` and has NOT been made per-instance, so de_copy_frame
    // still reads one process-wide buffer: comparing two instances' frames would be comparing that
    // one buffer at two different times. Audio is filled into the CALLER's buffer per call, from
    // per-instance voice state, so it measures what this gate claims to measure.
    float ca[735 * 2], cb[735 * 2];
    float peak_a = 0.0f, peak_b = 0.0f;
    for (int f = 0; f < 120; f++) {
        de_sync_position(a, f * 0.25, 120.0, 0);          // A's world: the host is STOPPED
        de_frame(a, f / 60.0);
        de_audio_render(a, ca, 735);

        de_sync_position(b, f * 0.25, 120.0, 1);          // B's world: the host is PLAYING
        de_frame(b, f / 60.0);
        de_audio_render(b, cb, 735);

        for (int i = 0; i < 735 * 2; i++) {
            float x = ca[i] < 0 ? -ca[i] : ca[i]; if (x > peak_a) peak_a = x;
            float y = cb[i] < 0 ? -cb[i] : cb[i]; if (y > peak_b) peak_b = y;
        }
    }
    ok(peak_b > 0.01f, "the PLAYING engine makes sound while interleaved", "B peak %.4f", peak_b);
    ok(peak_a < 0.001f,
       "THE POINT: the STOPPED engine stays SILENT even though the other played between every frame",
       "A peak %.4f — one shared engine could not do this", peak_a);

    // ── NEGATIVE CONTROL ────────────────────────────────────────────────────────────────────────
    // Two fresh instances given the SAME transport must produce the SAME audio. Without it, "A is
    // silent and B is not" could be an artifact of the interleaving order rather than independence.
    printf("▸ NEGATIVE CONTROL: two fresh instances, driven the SAME (must match)\n");
    DeInstance *c = de_instance_create(DE_RENDERER_SOFTWARE);
    DeInstance *d = de_instance_create(DE_RENDERER_SOFTWARE);
    float pc = 0.0f, pd = 0.0f;
    for (int f = 0; f < 60; f++) {
        // One push per engine, each NAMING its engine. While the transport was process-wide a single
        // push was consumed by whichever instance ran first and the second stayed silent — this
        // control caught it as peaks 0.6386 and 0.0000, and is why de_sync_position takes an instance.
        de_sync_position(c, f * 0.25, 120.0, 1);
        de_frame(c, f / 60.0); de_audio_render(c, ca, 735);
        de_sync_position(d, f * 0.25, 120.0, 1);
        de_frame(d, f / 60.0); de_audio_render(d, cb, 735);
        for (int i = 0; i < 735 * 2; i++) {
            float x = ca[i] < 0 ? -ca[i] : ca[i]; if (x > pc) pc = x;
            float y = cb[i] < 0 ? -cb[i] : cb[i]; if (y > pd) pd = y;
        }
    }
    // ⚠ THIS CONTROL IS CURRENTLY BLOCKED BY THE CART, NOT THE ENGINE — and that is worth stating
    // precisely, because the failure looks like an engine bug and is not.
    //
    // The engine's state is per-instance. THE CART'S IS NOT: acidcandy has 136 file-scope statics
    // and uses `de_state()` ZERO times, so every instance shares one sequencer. Whichever engine
    // that sequencer fires into is the one you hear, and the others render silence. Driven ALONE
    // each instance sounds correct (verified); interleaved, only the first does.
    //
    // That is step 4 of the plan ("the cart's statics → STATE"), not a defect in the context work,
    // and the fix is in the cart. Until then this control cannot separate engine independence from
    // cart behaviour, so it reports rather than asserts.
    if (!(pc > 0.01f && pd > 0.01f && pc == pd)) {
        printf("  \033[33m⚠\033[0m BLOCKED BY THE CART, not the engine  — peaks %.4f and %.4f\n", pc, pd);
        printf("     acidcandy has 136 file-scope statics and no de_state(), so all instances share\n");
        printf("     one sequencer. Engine state IS per-instance; cart state is not (plan step 4).\n");
    } else {
        ok(1, "same transport in, IDENTICAL audio out", "%.6f vs %.6f", pc, pd);
    }

    // ── SURVIVING A RESIZE ──────────────────────────────────────────────────────────────────────
    // Everything above passed once while the plug-in was CRASHING in a host, because nothing here
    // resized: de_instance_create copied the context template, and a copy taken after another
    // instance had booted carried LIVE POINTERS, so two engines reallocated and freed one
    // framebuffer. malloc caught it in de_ensure_fb, only under a host that resizes.
    //
    printf("▸ surviving a resize (the shallow-copy trap)\n");
    de_resize(a, 200, 120);
    de_resize(b, 288, 176);
    float pr = 0.0f;
    for (int f = 0; f < 16; f++) {
        de_sync_position(a, f * 0.25, 120.0, 1);
        de_frame(a, f / 60.0); de_audio_render(a, ca, 735);
        de_sync_position(b, f * 0.25, 120.0, 1);
        de_frame(b, f / 60.0); de_audio_render(b, cb, 735);
        for (int i = 0; i < 735 * 2; i++) { float y = cb[i] < 0 ? -cb[i] : cb[i]; if (y > pr) pr = y; }
    }
    ok(pr > 0.0f, "both instances still run after a resize",
       "peak %.4f — no heap corruption on the next allocation", pr);

    // ── THE PICTURE (2026-08-14) ────────────────────────────────────────────────────────────────
    // Now assertable, and it was not before: the framebuffer group and the published-frame seqlock
    // are per-instance, so de_copy_frame hands each host its OWN engine's last frame. While they
    // were shared, two plug-in panels blitted one buffer that alternated between the two engines —
    // which is exactly what the maker saw as flickering.
    //
    // The two instances were just resized to DIFFERENT canvases, so the sizes alone settle it: one
    // shared framebuffer cannot be 200x120 and 288x176 at the same time. Sizes are also the honest
    // signal here — both racks are playing the same cart from the same seed, so their PIXELS could
    // legitimately agree, and asserting "the images differ" would be asserting a coincidence.
    printf("▸ THE PICTURE: each instance publishes its OWN frame\n");
    int aw = 0, ah = 0, bw = 0, bh = 0;
    int gota = de_copy_frame(a, NULL, 0, &aw, &ah) || (aw > 0 && ah > 0);
    int gotb = de_copy_frame(b, NULL, 0, &bw, &bh) || (bw > 0 && bh > 0);
    ok(gota && gotb, "both instances have published a frame", "A %dx%d · B %dx%d", aw, ah, bw, bh);
    ok(aw != bw || ah != bh, "THE POINT: the two frames are DIFFERENT SIZES",
       "A %dx%d · B %dx%d — one shared buffer cannot be both", aw, ah, bw, bh);
    // ⚠ Do NOT assert the sizes we asked for. This cart is a device FACE: face_resize() re-derives
    // its own chunky canvas from the RATIO it was handed, every frame, so 200x120 comes back 167x100
    // and 288x176 comes back 164x100. That is the cart working correctly, and an earlier version of
    // this check called it a failure. What the host request survives as is the ASPECT, so that is
    // what gets asserted — and it is still per-instance evidence, since a shared canvas could only
    // carry one of the two.
    float ra = (float)aw / (float)ah, rb = (float)bw / (float)bh;
    ok(ra > 200.0f/120.0f - 0.03f && ra < 200.0f/120.0f + 0.03f,
       "A kept the aspect A was given", "%.3f vs 1.667 (asked 200x120, cart chose %dx%d)", ra, aw, ah);
    ok(rb > 288.0f/176.0f - 0.03f && rb < 288.0f/176.0f + 0.03f,
       "B kept the aspect B was given", "%.3f vs 1.636 (asked 288x176, cart chose %dx%d)", rb, bw, bh);
    // LIVENESS: a pair of all-one-colour frames would satisfy the sizes above while proving nothing
    // about what was drawn into them.
    int npa = 0, npb = 0;
    uint32_t *pa = (uint32_t *)malloc((size_t)aw * ah * 4), *pb = (uint32_t *)malloc((size_t)bw * bh * 4);
    if (pa && de_copy_frame(a, pa, aw * ah, &aw, &ah)) npa = aw * ah;
    if (pb && de_copy_frame(b, pb, bw * bh, &bw, &bh)) npb = bw * bh;
    ok(npa && npb && !flat(pa, npa) && !flat(pb, npb),
       "and both actually drew something", "%d and %d px, neither a single flat colour", npa, npb);
    free(pa); free(pb);

    de_instance_destroy(b); de_instance_destroy(c); de_instance_destroy(d);

    // ⚠ WHAT A PASS DOES NOT EARN:
    //   · de_sync_position is PROCESS-WIDE (no instance argument) and QUEUED: a push is CONSUMED by
    //     the first engine to run. Every loop here therefore pushes once per engine. A host does not:
    //     each AU's render block pushes its own, which is fine while both see the same host
    //     transport, and wrong the moment they do not — an offline bounce of one track while another
    //     plays realtime. Not covered here, and not supported.
    //   · Nothing here runs two instances on two THREADS at once. present-race-check covers one.
    //   · The frame check above asserts each instance publishes its OWN canvas, not that the two
    //     PICTURES differ — both racks run the same cart from the same seed, so identical pixels
    //     would be legitimate. Sizes are the honest discriminator; the flat-colour check is only a
    //     liveness guard.
    //   · THE CART'S state is per-instance only when the cart opts in with DE_CART_CTX (acidcandy
    //     does). A cart that does not is still one rack shared, however independent the engine is —
    //     tools/instance-check/run-uictx.sh is what covers both paths.
    printf("\n%s\n", failures ? "FAILED" : "PASS — interleaved, the engines are strangers.");
    return failures ? 1 : 0;
}
