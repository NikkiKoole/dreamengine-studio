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
// de:engine-owner multi — this probe EXISTS to run several engines at once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include "../../runtime/platform.h"

// The heap meter for the destroy section below. macOS's own allocator statistics rather than an
// external tool, so the assertion lives inside the gate and runs on every invocation.
// ⚠ LeakSanitizer is NOT the alternative here: it is unsupported on Darwin/arm64, which is the only
// platform this probe builds on (it links CoreMIDI). `leaks(1)` would work but needs a second
// process and would answer "did the process leak", not "does destroy give an instance back".
#ifdef __APPLE__
#include <malloc/malloc.h>
static size_t heap_in_use(void) {
    malloc_statistics_t st;
    malloc_zone_statistics(malloc_default_zone(), &st);
    return st.size_in_use;
}
#define HAVE_HEAP_METER 1
#else
static size_t heap_in_use(void) { return 0; }
#endif

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

// FNV-1a over the raw sample bits. The peak comparisons elsewhere in this probe are deliberately
// coarse — they answer "did it play at all" — and a coarse answer cannot see a voice whose
// modulator started from a different seed: same notes, same loudness, different waveform. This is
// the sample-exact one.
static unsigned long long mix_hash(unsigned long long h, const float *s, int n) {
    for (int i = 0; i < n; i++) {
        unsigned int bits;
        memcpy(&bits, &s[i], sizeof bits);
        h = (h ^ bits) * 1099511628211ull;
    }
    return h;
}

int main(int argc, char **argv) {
    // -bypass: the NEGATIVE CONTROL for the destroy section — skip de_instance_destroy entirely and
    // require the heap meter to go red. Without it a broken meter and a clean destroy print the same
    // green, which is the failure mode gate-controls.js exists to name.
    const int bypass = (argc > 1 && strcmp(argv[1], "-bypass") == 0);

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

    // ── INTERLEAVING MUST NOT CHANGE AN ENGINE'S OUTPUT, SAMPLE FOR SAMPLE (2026-08-14) ─────────
    // The strongest form of the claim this probe exists to make, and the one the sections above
    // cannot reach: run an engine ALONE, then run an identical engine with ANOTHER engine stepping
    // between every one of its frames, and require the two renders to be bit-identical. Anything
    // still shared that the second engine touches shows up here and nowhere else.
    //
    // WHY IT WAS ADDED. `lfo_seed_ctr` — the per-voice seed counter for the S&H/random LFO shapes —
    // was a function-local static, i.e. one sequence shared by every engine. Moving it was checked
    // against both existing gates and both stayed green: `refactor-guard` by construction (one
    // instance cannot observe it), and this probe because every assertion it had compared PEAKS, and
    // a reseeded modulator does not move the peak.
    //
    // ⚠ AND IT STILL DOES NOT GATE THAT MOVE — measured, not assumed, so do not read this section as
    // covering it. Three perturbations were run against the default cart: the counter put back to a
    // shared static (hash unchanged), and the seed's initial value changed outright to 0x99999 (hash
    // unchanged again). acidcandy reaches LFO_SHAPE_RANDOM only through acid303.h's drift at 0.13
    // and 0.19 Hz, and evidently not audibly here. What this section DOES buy is a sample-exact
    // property where there were only peaks, which catches any shared buffer, table or cursor the
    // other engine disturbs. Gating a modulator SEED needs a cart that opts into DE_CART_CTX *and*
    // drives a stateful LFO fast enough to step — nothing on the shelf does both today.
    //
    // ⚠ IT IS ALSO ONLY MEANINGFUL FOR A `DE_CART_CTX` CART. Measured on three that are not
    // (epiano, dune: solo and interleaved hashes differ; dubsiren: both silent, which the liveness
    // assertion below catches): without the opt-in the CART's own statics are shared, so the
    // property fails for reasons that have nothing to do with the engine. acidcandy is the default
    // here precisely because it opts in.
    printf("▸ INTERLEAVING MUST NOT CHANGE AN ENGINE'S OUTPUT (sample-exact)\n");
    // ⚠ THE WINDOW HAS TO BE LONG ENOUGH FOR THE MODULATOR TO MOVE. At 60 frames (1s) this section
    // was green in BOTH directions: acid303.h drives its drift LFOs at 0.13 and 0.19 Hz, so the
    // first sample-and-hold step lands about 7.7s in, and before it the seed has not reached a
    // single sample. 600 frames = 10s covers it. A gate whose window is shorter than the period of
    // the thing it watches is not measuring that thing.
    enum { IL_FRAMES = 600 };
    DeInstance *solo = de_instance_create(DE_RENDERER_SOFTWARE);
    unsigned long long h_solo = 0;
    float p_solo = 0.0f;
    for (int f = 0; f < IL_FRAMES; f++) {
        de_sync_position(solo, f * 0.25, 120.0, 1);
        de_frame(solo, f / 60.0); de_audio_render(solo, ca, 735);
        h_solo = mix_hash(h_solo, ca, 735 * 2);
        for (int i = 0; i < 735 * 2; i++) { float x = ca[i] < 0 ? -ca[i] : ca[i]; if (x > p_solo) p_solo = x; }
    }
    DeInstance *e = de_instance_create(DE_RENDERER_SOFTWARE);
    DeInstance *g = de_instance_create(DE_RENDERER_SOFTWARE);
    unsigned long long h_inter = 0;
    for (int f = 0; f < IL_FRAMES; f++) {
        de_sync_position(e, f * 0.25, 120.0, 1);
        de_frame(e, f / 60.0); de_audio_render(e, ca, 735);
        h_inter = mix_hash(h_inter, ca, 735 * 2);
        de_sync_position(g, f * 0.25, 120.0, 1);   // the OTHER engine runs between e's frames
        de_frame(g, f / 60.0); de_audio_render(g, cb, 735);
    }
    // LIVENESS FIRST: two silent renders hash the same and would pass this forever.
    ok(p_solo > 0.01f, "the solo engine actually rendered something to compare",
       "peak %.4f", p_solo);
    ok(h_solo == h_inter, "THE POINT: an engine renders the same whether or not another runs between its frames",
       "%016llx vs %016llx — a shared modulator seed, counter or table breaks exactly this",
       h_solo, h_inter);
    de_instance_destroy(solo); de_instance_destroy(e); de_instance_destroy(g);

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

    // ── DESTROY GIVES THE MEMORY BACK (2026-08-14) ──────────────────────────────────────────────
    // Until today `de_instance_destroy` freed the DeInstance struct and nothing the instance had
    // allocated: the framebuffer, the rotation layer, the published frame, the cart's persistent
    // block and the sample slots all stayed on the heap. Every gate sat green through it, this one
    // included — it called destroy three times and asserted nothing about the result. A host that
    // opens and closes racks (every DAW session) leaks a canvas per open.
    //
    // The shape of the assertion matters. A single create/destroy pair proves nothing: the FIRST
    // instance warms up process-wide allocations (the audio stream, the decoded sheet, the map) that
    // are correctly not an instance's to free, so its footprint never comes back and should not. So
    // measure across ROUNDS after a warm-up round, where the only thing repeating is one instance's
    // own life. A leak shows up as a slope; process-wide warm-up does not.
    printf("▸ DESTROY GIVES THE MEMORY BACK%s\n", bypass ? "  (BYPASS: destroy skipped — must FAIL)" : "");
#ifdef HAVE_HEAP_METER
    const int ROUNDS = 8;
    size_t base = 0, after = 0;
    for (int r = 0; r <= ROUNDS; r++) {
        DeInstance *t = de_instance_create(DE_RENDERER_SOFTWARE);
        if (t) {
            de_resize(t, 200, 120);                       // force a framebuffer realloc, like a host does
            int n = 0; float pk = 0.0f;
            uint32_t *px = run(t, 8, 120.0, 1, &n, &pk);  // publish a frame → pres_buf, run the cart → de_state
            free(px);
            if (!bypass) de_instance_destroy(t);
        }
        if (r == 0) base = heap_in_use();                 // after the warm-up round: process-wide is paid for
    }
    after = heap_in_use();
    long grew = (long)after - (long)base;
    long per  = grew / ROUNDS;
    // One instance's own buffers at this canvas are ~200 KB (sw_cbuf + sw_world_buf + pres_buf +
    // the cart block); the struct on top is ~4 MB. 32 KB per round is comfortably below the first
    // and nowhere near the second, so it separates "gave it back" from either failure.
    ok((bypass ? per > 32 * 1024 : per < 32 * 1024),
       bypass ? "CONTROL: skipping destroy DOES show up (the meter works)"
              : "8 create/destroy rounds leave the heap flat",
       "%+ld B over %d rounds = %+ld B/round", grew, ROUNDS, per);
#else
    ok(1, "heap meter unavailable on this platform — destroy is UNCHECKED here", "not Darwin");
#endif

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
