/*
 * body.h — ONE copy of a mixing loop shaped like the engine's, compiled THREE ways.
 *
 * (Not a standalone translation unit: it is written against BARE NAMES and only compiles when a
 * variant file has defined them first. Your editor will flag it; that is expected.)
 *
 * This file reads exactly as runtime/sound.h does today: `f_cut`, `echo_buf[…]`, plain globals.
 * The three variant files define those names differently before including it:
 *
 *   bench_plain.c   they are file-scope statics                       (the engine today)
 *   bench_tls.c     #define f_cut (ctx->f_cut), ctx = a _Thread_local   (option b)
 *   bench_arg.c     #define f_cut (ctx->f_cut), ctx = a parameter       (option c)
 *
 * That is the whole point: the DSP text below is byte-identical in all three builds, so any
 * timing difference IS the access mechanism and nothing else. It doubles as a demonstration that
 * the refactor never touches the code doing the work.
 *
 * Shape copied from the real per-sample block in sound.h (~line 7200): an outer sample loop, an
 * inner voice loop of bare arithmetic, and a handful of stage functions called ONCE PER SAMPLE
 * (in the engine: apply_insert, leslie_process, sc_apply, shimmer_process, emit_process). Those
 * per-sample calls are the thing under test, because under option (b) each pays its own
 * thread-local lookup on entry, ~300k times a second. NOINLINE models the real stages, which are
 * far too big for clang to inline; building with INLINE_OK=1 shows the best case instead.
 */

NOINLINE static void stage_filter(CTX_DECL float in, float *out) {
    f_z1 += f_cut * (in - f_z1 + f_res * (f_z1 - f_z2));
    f_z2 += f_cut * (f_z1 - f_z2);
    *out = f_z2;
}

NOINLINE static void stage_echo(CTX_DECL float *mix) {
    float d = echo_buf[echo_pos & 4095];
    echo_z += echo_tone * (d - echo_z);
    echo_buf[echo_pos & 4095] = *mix + echo_z * echo_fb;
    echo_pos++;
    *mix += echo_z * 0.5f;
}

NOINLINE static void stage_chorus(CTX_DECL float *mix) {
    cho_ph += 0.0001f;
    if (cho_ph >= 1.0f) cho_ph -= 1.0f;
    *mix *= 1.0f + cho_depth * (cho_ph - 0.5f);
}

NOINLINE static void stage_drive(CTX_DECL float *mix) {
    float x = *mix * (1.0f + drive_amt * 8.0f);
    *mix = (x - x * x * x * 0.16666f) * drive_tone;
}

NOINLINE static void stage_sidechain(CTX_DECL float *mix) {
    sc_env += 0.001f * (0.5f - sc_env);
    *mix *= 1.0f - sc_env * sc_ratio;
}

NOINLINE static void stage_master(CTX_DECL float *mix) {
    *mix *= master_gain * (1.0f + pan_law_x * 0.01f);
}

/* the per-sample loop */
static float body_run(CTX_DECL int nsamples) {
    float acc = 0.0f;
    for (int i = 0; i < nsamples; i++) {
        float mix = 0.0f;
        for (int v = 0; v < 8; v++) {          /* inner voice loop: no calls, like the real one */
            vphase[v] += vfreq[v];
            if (vphase[v] >= 1.0f) vphase[v] -= 1.0f;
            mix += vphase[v] - 0.5f;
        }
        float filtered;                        /* the per-sample stage calls */
        stage_filter(CTX_PASS mix, &filtered);
        mix = filtered;
        stage_echo(CTX_PASS &mix);
        stage_chorus(CTX_PASS &mix);
        stage_drive(CTX_PASS &mix);
        stage_sidechain(CTX_PASS &mix);
        stage_master(CTX_PASS &mix);
        acc += mix;
    }
    return acc;
}

/* the public entry point — this is the BOUNDARY, the one place that resolves "which instance" */
float ENTRY(int nsamples) {
    CTX_LOCAL
    return body_run(CTX_PASS nsamples);
}
