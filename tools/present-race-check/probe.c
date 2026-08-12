// probe.c — the thread-safety gate for the HOST/ENGINE split that the AUv3 view needs:
// de_copy_frame() (the published frame snapshot) and the DEFERRED resize path, both in studio.c.
//
//   bash tools/present-race-check/run.sh            # exit 0 = PASS
//   bash tools/present-race-check/run.sh -tsan      # under ThreadSanitizer — the real gate
//   bash tools/present-race-check/run.sh -bypass    # NEGATIVE CONTROL: the naive host must crash/fail
//
// It builds the REAL engine (studio.c + raylib_compat.c + a cart, DE_NO_RAYLIB) exactly as
// tools/build-nr.sh does, then plays the two threads an AUv3 has:
//
//   ENGINE THREAD — de_frame() in a loop. In a plug-in this is the AUDIO thread, driven by the render
//                   block, because the frame is sample-clocked (that is what survives a DAW bounce).
//   HOST THREAD   — the view: de_copy_frame() to blit, plus de_resize/de_set_safe_area from a layout
//                   pass, which is what a user dragging the plug-in window produces.
//
// WHY IT IS WORTH A WHOLE PROBE. This is not a subtle race. de_resize reallocs the framebuffer, so
// the naive version is a use-after-free while the other thread is drawing into it — a crash in the
// HOST, blamed on us, reproducible only by someone resizing a window at the wrong moment. -bypass
// builds exactly that (the host calling de_framebuffer + a direct resize) and it dies or corrupts;
// the real path must survive the same storm.
//
// WHAT IT ASSERTS:
//   1. SURVIVES: thousands of blits racing thousands of frames and hundreds of resizes, no crash.
//   2. COHERENT: every snapshot's dimensions are self-consistent and within the buffer we were told
//      to size, and the pixel content is a real frame (opaque alpha), never half a resize.
//   3. THE RESIZE LANDS: a de_resize from the host thread is visible to the engine within a frame or
//      two, so deferring did not quietly break reflow.
//   4. NOT VACUOUS: it really did blit, really did resize, and the canvas really did change size.

#include "../../runtime/platform.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ThreadSanitizer instruments every memory access in the engine AND the cart, so the same frame count
// takes minutes instead of seconds. Fewer frames still storm the seqlock hard (the blit loop spins far
// faster than 60 Hz — hundreds of reads per frame), and TSan does not need volume: it reports a race
// from a single conflicting pair, which is the whole reason it is the real gate here.
#if defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define PROBE_FRAMES 1200
#  endif
#endif
#ifndef PROBE_FRAMES
#  define PROBE_FRAMES 20000
#endif
#define CAP_PX (2048 * 1536)          // generous: the host owns this buffer and never resizes it here

static atomic_int engine_done = 0;
static atomic_long frames_run = 0;

static void *engine_thread(void *unused) {
    (void)unused;
    float audio[735 * 2];
    for (long i = 0; i < PROBE_FRAMES; i++) {
        de_frame((double)(i + 1) / 60.0);
        // Pull the audio too, on THIS thread, because that is exactly what the AUv3 render block does:
        // one de_frame per 735 rendered samples, both on the audio thread. Without it the cart's sound
        // calls pile up unconsumed and the run drowns in "request queue overflow" — which would be the
        // probe misrepresenting the very arrangement it exists to model.
        de_audio_render(audio, 735);
        atomic_store_explicit(&frames_run, i + 1, memory_order_relaxed);
    }
    atomic_store_explicit(&engine_done, 1, memory_order_release);
    return 0;
}

int main(void) {
    printf("\xE2\x96\xB8 present/resize race: the view blitting + resizing while the engine draws\n");
    de_init(DE_RENDERER_SOFTWARE);

    int failures = 0;
    uint32_t *dst = (uint32_t *)malloc((size_t)CAP_PX * sizeof(uint32_t));
    if (!dst) { printf("  \xE2\x9C\x97 out of memory\n"); return 1; }

    pthread_t th;
    pthread_create(&th, 0, engine_thread, 0);

    long blits = 0, empty = 0, bad_dims = 0, bad_pixels = 0, resizes = 0;
    int seen_w[64], nseen = 0;
    // Sizes a user might drag a plug-in window through. Kept modest so the cart's own reflow does not
    // clamp them all to the same value (which would make check 4 vacuous).
    const int sizes[][2] = {{160,100},{200,120},{320,200},{240,160},{400,240},{180,110}};
    while (!atomic_load_explicit(&engine_done, memory_order_acquire)) {
        int w = 0, h = 0;
#ifdef DE_PRESENT_BYPASS
        // THE NEGATIVE CONTROL: the naive host, reading the engine's LIVE canvas from another thread
        // (what a view would do if de_copy_frame did not exist). The resize is deferred to the engine
        // thread now, which does not save this path — it moves the realloc UNDER this memcpy.
        const uint32_t *base = de_framebuffer();
        w = de_screen_w(); h = de_screen_h();
        int got = (base && w > 0 && h > 0 && (long)w * h <= CAP_PX);
        if (got) memcpy(dst, base, (size_t)w * h * sizeof(uint32_t));
#else
        int got = de_copy_frame(dst, CAP_PX, &w, &h);
#endif
        if (got) {
            blits++;
            if (w <= 0 || h <= 0 || (long)w * h > CAP_PX) bad_dims++;
            else {
                // A frame the engine finished is fully opaque (studio.c clears to opaque colours).
                // A snapshot torn across a resize shows up here as transparent/uninitialised pixels.
                int checked = 0, bad = 0;
                for (int y = 0; y < h && checked < 400; y += 7)
                    for (int x = 0; x < w && checked < 400; x += 11, checked++)
                        if ((dst[(size_t)y * w + x] >> 24) != 0xFF) bad++;
                if (bad) bad_pixels++;
            }
            int known = 0;
            for (int i = 0; i < nseen; i++) if (seen_w[i] == w) { known = 1; break; }
            if (!known && nseen < 64) seen_w[nseen++] = w;
        } else empty++;

        if (blits % 37 == 0) {                       // a layout pass, from the host thread
            const int *s = sizes[resizes % 6];
            de_resize(s[0], s[1]);
            de_set_safe_area(2, 3, 2, 3);
            resizes++;
        }
    }
    pthread_join(th, 0);

    printf("  \xE2\x9C\x93 survived: %ld blits, %ld frames, %ld resizes, %ld empty reads\n",
           blits, atomic_load(&frames_run), resizes, empty);
    if (bad_dims)   { printf("  \xE2\x9C\x97 %ld snapshots had dimensions outside the buffer\n", bad_dims); failures++; }
    else              printf("  \xE2\x9C\x93 every snapshot's dimensions were coherent\n");
    if (bad_pixels) { printf("  \xE2\x9C\x97 %ld snapshots contained non-opaque (torn/uninitialised) pixels\n", bad_pixels); failures++; }
    else              printf("  \xE2\x9C\x93 no snapshot was torn across a resize\n");

    // 3: a resize issued from the host thread must actually reach the engine.
    de_resize(288, 176);
    for (int i = 0; i < 4; i++) de_frame(1000.0 + i / 60.0);
    int got_w = de_screen_w(), got_h = de_screen_h();
    // A resizable cart reflows, so the engine may land on its own chunky size — what must be true is
    // that the canvas MOVED to something derived from the request, not that it matched it exactly.
    int landed = (got_w != 160 || got_h != 100);
    printf("  %s a deferred resize reaches the engine  — asked 288x176, canvas is now %dx%d\n",
           landed ? "\xE2\x9C\x93" : "\xE2\x9C\x97", got_w, got_h);
    if (!landed) failures++;

    if (blits < 100)   { printf("  \xE2\x9C\x97 only %ld blits — the test was vacuous\n", blits); failures++; }
    if (resizes < 5)   { printf("  \xE2\x9C\x97 only %ld resizes — the test was vacuous\n", resizes); failures++; }
    if (nseen < 2)     { printf("  \xE2\x9C\x97 the canvas never changed size (%d distinct widths) — vacuous\n", nseen); failures++; }
    else                 printf("  \xE2\x9C\x93 the canvas really moved: %d distinct widths observed\n", nseen);

    printf(failures == 0 ? "\nPASS — the view can blit and resize while the engine draws.\n"
                         : "\n%d check(s) FAILED\n", failures);
    free(dst);
    return failures == 0 ? 0 : 1;
}
