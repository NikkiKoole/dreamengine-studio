// probe.c — THE ACCEPTANCE TEST for session state: does a saved rack come back?
//
// The gap this closes: `ios/AU/TinyjamAU.swift` implements fullState on top of de_save_state /
// de_load_state, and the user-visible promise is "reopen the project and your rack is as you left
// it". Nothing else in the repo can check that — refactor-guard proves a state move changed nothing,
// instance-check proves two instances are strangers, and neither ever saves anything.
//
// FOUR NEGATIVE CONTROLS, because every assertion here has a way of passing for the wrong reason:
//   1. A FRESH instance must be at DEFAULTS, not at the saved values. Without this, "B matches A
//      after restore" would also pass if B had simply never differed from A — which is what would
//      happen if both instances shared one state block, i.e. the exact bug the split exists to fix.
//   2. The SCRATCH slice must NOT come back. Without this, a build that saved EVERYTHING (including
//      live voice handles and pointers) would look like a perfect pass.
//   3. A blob with a mangled FINGERPRINT must be REFUSED, and refusing must leave the rack alone.
//      "Refuse rather than misapply" is the whole safety story for a struct that changed shape.
//   4. A TRUNCATED blob must be refused too — length is checked, not assumed.
//
// de:engine-owner multi — this probe runs two engines on purpose, to save one and restore the other
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include "../../runtime/platform.h"

// the cart's handshake globals (tools/state-check/statecart.c)
extern int sc_write_knob, sc_fire, sc_seen_knob, sc_seen_p0, sc_seen_ticks;

static int failures = 0;
static void ok(int cond, const char *what, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char detail[512]; vsnprintf(detail, sizeof detail, fmt, ap); va_end(ap);
    if (cond) printf("  \033[32m✓\033[0m %s  — %s\n", what, detail);
    else { printf("  \033[31m✗\033[0m %s  — %s\n", what, detail); failures++; }
}

// Drive one engine, returning its published pixels and audio peak. Frames are what apply a pending
// restore (de_ss_apply runs at the top of de_frame), so "run one frame" IS the apply step.
static uint32_t *drive(DeInstance *in, int frames, int *npx, float *peak) {
    float chunk[735 * 2];
    float pk = 0.0f;
    for (int f = 0; f < frames; f++) {
        de_frame(in, f / 60.0);
        de_audio_render(in, chunk, 735);
        for (int i = 0; i < 735 * 2; i++) { float a = chunk[i] < 0 ? -chunk[i] : chunk[i]; if (a > pk) pk = a; }
    }
    if (peak) *peak = pk;
    int w = 0, h = 0;
    de_copy_frame(in, NULL, 0, &w, &h);
    if (w <= 0 || h <= 0) { if (npx) *npx = 0; return NULL; }
    uint32_t *buf = (uint32_t *)malloc((size_t)w * h * 4);
    if (!de_copy_frame(in, buf, w * h, &w, &h)) { free(buf); if (npx) *npx = 0; return NULL; }
    if (npx) *npx = w * h;
    return buf;
}

// Play ONE note and return its peak amplitude. The cart trims slot 0's level from the knob, and the
// note uses slot 0, so this reads back the level that travelled in ctx_log — the SOUND half of the
// restore, which the pixel comparison cannot see at all.
static float fire_peak(DeInstance *in) {
    sc_fire = 1;
    float pk = 0;
    uint32_t *px = drive(in, 10, NULL, &pk);
    free(px);
    return pk;
}

int main(void) {
    printf("\n▸ save a rack, restore it into a DIFFERENT instance\n");

    DeInstance *A = de_instance_create(DE_RENDERER_SOFTWARE);
    if (!A) { printf("  could not create instance A\n"); return 1; }

    int npxA = 0; float pkA = 0;
    uint32_t *fA = drive(A, 4, &npxA, &pkA);
    ok(sc_seen_knob == 3, "A boots at the cart's template default",
       "knob = %d (expected 3 — a ZERO here would mean the slice was never seeded)", sc_seen_knob);
    ok(npxA > 0, "A publishes a frame", "%d px", npxA);

    // the "player" turns a knob and the pattern flips
    sc_write_knob = 6;
    free(fA); fA = drive(A, 6, &npxA, &pkA);
    int knobA = sc_seen_knob, p0A = sc_seen_p0, ticksA = sc_seen_ticks;
    ok(knobA == 6, "A's knob moved", "knob = %d", knobA);
    float pkA6 = fire_peak(A);
    ok(pkA6 > 0.0f, "A is audible at its new level",
       "peak %.4f — a silent probe cannot test the sound half at all", pkA6);

    // ── save ──────────────────────────────────────────────────────────────────────────────────────
    int need = de_save_state(A, NULL, 0);
    ok(need > 0, "the size probe reports a length without writing", "%d bytes", need);
    unsigned char *blob = (unsigned char *)malloc((size_t)need);
    int n = de_save_state(A, blob, need);
    ok(n == need, "the fill call writes exactly that", "wrote %d of %d", n, need);

    // ── NEGATIVE CONTROL 1: a fresh instance must NOT already look like A ─────────────────────────
    printf("\n▸ NEGATIVE CONTROL: a fresh instance is at defaults, not at A's values\n");
    DeInstance *B = de_instance_create(DE_RENDERER_SOFTWARE);
    if (!B) { printf("  could not create instance B\n"); return 1; }
    int npxB = 0; float pkB = 0;
    uint32_t *fB = drive(B, 4, &npxB, &pkB);
    ok(sc_seen_knob == 3, "B boots at the default, not A's 6",
       "B knob = %d, A knob = %d%s", sc_seen_knob, knobA,
       sc_seen_knob == knobA ? "  ← they SHARE a state block; nothing below means anything" : "");
    ok(npxB > 0 && npxA > 0 && memcmp(fA, fB, (size_t)npxB * 4) != 0,
       "and B's frame differs from A's", "so a later match is a real restore");
    int ticksB_before = sc_seen_ticks;
    // B never had a knob written, so it never called instrument_level and slot 0 is at UNITY — while
    // A trimmed it to 6/8. So B is LOUDER here, and the direction is not the point: what matters is
    // that the level is audibly different before the restore, or "B plays at A's level" afterwards
    // would prove nothing.
    float pkB_fresh = fire_peak(B);
    float d0 = pkB_fresh > pkA6 ? pkB_fresh - pkA6 : pkA6 - pkB_fresh;
    ok(pkB_fresh > 0.0f && d0 > pkA6 * 0.05f,
       "and B is audibly at a DIFFERENT level from A",
       "B peak %.4f (untrimmed, unity) vs A %.4f (trimmed to 6/8)", pkB_fresh, pkA6);

    // ── restore ───────────────────────────────────────────────────────────────────────────────────
    printf("\n▸ restore\n");
    ok(de_load_state(B, blob, n) == 1, "the blob is accepted", "%d bytes", n);
    free(fB); fB = drive(B, 1, &npxB, &pkB);   // the apply lands at the top of this frame
    ok(sc_seen_knob == knobA, "B's knob is A's knob", "%d == %d", sc_seen_knob, knobA);
    ok(sc_seen_p0 == p0A, "and so is the second saved field", "pattern[0] %d == %d", sc_seen_p0, p0A);
    free(fB); fB = drive(B, 1, &npxB, &pkB);
    ok(npxA == npxB && memcmp(fA, fB, (size_t)npxB * 4) == 0,
       "B now renders A's frame", "%d px compared", npxB);

    // ── the SOUND half: the config log travelled, not just the cart's slice ───────────────────────
    printf("\n▸ the sound configuration came back too (ctx_log, not just the slice)\n");
    float pkB6 = fire_peak(B);
    float d = pkB6 > pkA6 ? pkB6 - pkA6 : pkA6 - pkB6;
    ok(d < pkA6 * 0.02f, "B now plays at A's level",
       "B peak %.4f vs A %.4f (was %.4f before the restore)", pkB6, pkA6, pkB_fresh);

    // ── NEGATIVE CONTROL 2: the scratch slice must have stayed B's own ────────────────────────────
    printf("\n▸ NEGATIVE CONTROL: the SCRATCH slice did not travel\n");
    ok(sc_seen_ticks != ticksA, "B's scratch counter is not A's",
       "B %d vs A %d%s", sc_seen_ticks, ticksA,
       sc_seen_ticks == ticksA ? "  ← EVERYTHING is being saved, incl. what must never be" : "");
    ok(sc_seen_ticks > ticksB_before, "it kept counting from B's own value",
       "%d → %d", ticksB_before, sc_seen_ticks);

    // ── NEGATIVE CONTROL 3: a mangled fingerprint must be refused, and change nothing ─────────────
    printf("\n▸ NEGATIVE CONTROL: a blob from another build is REFUSED, not misapplied\n");
    int knob_before = sc_seen_knob;
    unsigned char *bad = (unsigned char *)malloc((size_t)n);
    memcpy(bad, blob, (size_t)n);
    bad[8] ^= 0xFF;                       // the fingerprint word (magic, version, fingerprint, …)
    ok(de_load_state(B, bad, n) == 0, "refused", "fingerprint mismatch");
    free(fB); fB = drive(B, 1, &npxB, &pkB);
    ok(sc_seen_knob == knob_before, "and the rack was left alone", "knob still %d", sc_seen_knob);

    // ── NEGATIVE CONTROL 4: a truncated blob ──────────────────────────────────────────────────────
    ok(de_load_state(B, blob, n - 1) == 0, "a truncated blob is refused", "%d of %d bytes", n - 1, n);
    ok(de_load_state(B, blob, 3) == 0, "and so is one shorter than the header", "3 bytes");

    // ── the round trip is stable: saving the restored rack reproduces the blob ────────────────────
    printf("\n▸ the restored rack saves back to the same bytes\n");
    ok(de_load_state(B, blob, n) == 1, "reload", "");
    free(fB); fB = drive(B, 2, &npxB, &pkB);
    unsigned char *again = (unsigned char *)malloc((size_t)need);
    int n2 = de_save_state(B, again, need);
    ok(n2 == n && memcmp(again, blob, (size_t)n) == 0,
       "save(restore(save(A))) == save(A)", "%d vs %d bytes", n2, n);

    free(fA); free(fB); free(blob); free(bad); free(again);
    de_instance_destroy(B);
    if (failures) printf("\n\033[31mFAILED\033[0m — %d assertion(s)\n", failures);
    else          printf("\n\033[32mPASS\033[0m — a saved rack comes back, and scratch does not\n");
    return failures ? 1 : 0;
}
