// pm.c — PATCH MATCH: a sample in, eight dreamengine patches out.
//
//   node-free:  bash tools/patch-match/build.sh tools/patch-match/pm.c build/pm
//               ./build/pm "some sample.wav"
//
// THE SHAPE, and why it is staged rather than one big search. A patch here is not
// a smooth blob of knobs: the biggest decision is WHICH ENGINE, and that is
// discrete. Throwing every dimension at one optimizer would make it rediscover
// that choice through a wall of continuous parameters that mean different things
// per engine. So:
//
//   stage 1  ENGINE RACE   — every engine gets a short, cheap search on a small
//                            window. The output is a RANKING, which is often the
//                            more useful answer: "that is a MALLET, then EPIANO".
//   stage 2  VOICE REFINE  — the top engines get the real budget, seeded with
//                            their stage-1 winner, several seeds each. This is
//                            where the eight candidates come from.
//   stage 3  FX FIT        — the voice is FROZEN and only effects move. Freezing
//                            is the point: an optimizer allowed to move both will
//                            happily smear reverb over a wrong voice to fake a
//                            decay, and then neither number means anything. Held
//                            apart, you can read which half did the work.
//
// Every candidate renders on a FRESH ENGINE INSTANCE (measured: cheaper than
// flushing the old one, because it skips the flush render). That is what makes a
// score depend on the patch alone and not on what was evaluated before it.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "platform.h"
#include "pmpatch.h"
#include "patchmatch.h"
#include "pmwav.h"

extern PmPatch pm_req;
extern int pm_fire, pm_silence, pm_midi, pm_hold_ms, pm_vol;

#define SR 44100

// ── the render seam ─────────────────────────────────────────────────────────
static float *g_stereo = NULL;
static int    g_cap = 0;

static void render_patch(const PmPatch *p, float *mono, int n, int midi, int hold_ms)
{
    if (n > g_cap) { free(g_stereo); g_stereo = (float*)malloc(sizeof(float) * n * 2); g_cap = n; }
    DeInstance *in = de_instance_create(DE_RENDERER_SOFTWARE);
    pm_req = *p; pm_fire = 1; pm_midi = midi; pm_hold_ms = hold_ms; pm_vol = 5;
    de_frame(in, 0.0);
    de_audio_render(in, g_stereo, n);
    for (int i = 0; i < n; i++) mono[i] = 0.5f * (g_stereo[i*2] + g_stereo[i*2+1]);
    de_instance_destroy(in);
}

// ── differential evolution over a SUBSET of the patch vector ────────────────
#define PM_FX_BASE 1000   // a dim >= this indexes f[]; below it indexes v[]
static inline float pm_get(const PmPatch *p, int d) { return d >= PM_FX_BASE ? p->f[d - PM_FX_BASE] : p->v[d]; }
static inline void  pm_put(PmPatch *p, int d, float x) { if (d >= PM_FX_BASE) p->f[d - PM_FX_BASE] = x; else p->v[d] = x; }
#define FXD(x) ((x) + PM_FX_BASE)

typedef struct {
    PmTarget *t;
    PmPatch   base;          // frozen coordinates; the search only writes `dims`
    const int *dims; int ndims;
    int  fx;                 // 1 = this is the fx stage (turns on the sparsity cost)
    int  midi, hold_ms;
    float sparsity;          // fx stage only: cost per effect switched on
    long  evals;
} Ctx;

// The fx stage answers "which effects EARN their place", not "what is the lowest
// number". Without a cost per effect the optimizer leaves all eight slightly on,
// which scores a hair better and tells you nothing you can act on.
static const int FX_AMOUNT_DIM[9] = { FXD(F_DRIVE), FXD(F_TAPEWOW), FXD(F_CRUSHMIX), FXD(F_CHMIX),
                                      FXD(F_TREMDEP), FXD(F_ECHOSEND), FXD(F_RVBSEND), FXD(F_TAPESAT),
                                      V_VIBDEP };

static float eval_patch(Ctx *c, const float *x, float *scratch)
{
    PmPatch p = c->base;
    for (int i = 0; i < c->ndims; i++) pm_put(&p, c->dims[i], x[i]);
    render_patch(&p, scratch, c->t->len, c->midi, c->hold_ms);
    c->evals++;
    float d = pm_distance(c->t, scratch);
    if (c->fx && d < PM_BAD) {
        int on = 0;
        for (int i = 0; i < 9; i++) if (pm_get(&p, FX_AMOUNT_DIM[i]) > 0.02f) on++;
        d += c->sparsity * on;
    }
    return d;
}

static unsigned s_rng = 1u;
static float frnd(void) { s_rng = s_rng * 1664525u + 1013904223u; return (float)((s_rng >> 8) & 0xFFFFFF) / (float)0x1000000; }
static int   irnd(int n) { return (int)(frnd() * n) % n; }
static float clamp01(float v) { if (v < 0) v = -v; if (v > 1) v = 2.0f - v; return v < 0 ? 0 : (v > 1 ? 1 : v); }

// rand/1/bin with an annealed step size: explore early, polish late. `seedvec`
// (may be NULL) is injected as individual 0 — the stage-1 winner handed to stage 2,
// which is the cheap local version of Genopatch seeding the population.
static float de_search(Ctx *c, float *best, int pop, int gens, unsigned seed, const float *seedvec)
{
    s_rng = seed ? seed : 1u;
    int np = c->ndims;
    float *X = (float*)malloc(sizeof(float) * pop * np);
    float *fit = (float*)malloc(sizeof(float) * pop);
    float *trial = (float*)malloc(sizeof(float) * np);
    float *scratch = (float*)malloc(sizeof(float) * c->t->len);

    for (int i = 0; i < pop; i++) {
        for (int j = 0; j < np; j++) X[i*np+j] = frnd();
        if (i == 0 && seedvec) memcpy(X, seedvec, sizeof(float) * np);
        fit[i] = eval_patch(c, &X[i*np], scratch);
    }
    for (int g = 0; g < gens; g++) {
        float F = 0.85f - 0.45f * (g / (float)gens);
        for (int i = 0; i < pop; i++) {
            int a, b, d;
            do { a = irnd(pop); } while (a == i);
            do { b = irnd(pop); } while (b == i || b == a);
            do { d = irnd(pop); } while (d == i || d == a || d == b);
            int jr = irnd(np);
            for (int j = 0; j < np; j++)
                trial[j] = (frnd() < 0.9f || j == jr) ? clamp01(X[a*np+j] + F * (X[b*np+j] - X[d*np+j])) : X[i*np+j];
            float f = eval_patch(c, trial, scratch);
            if (f < fit[i]) { memcpy(&X[i*np], trial, sizeof(float)*np); fit[i] = f; }
        }
    }
    int bi = 0;
    for (int i = 1; i < pop; i++) if (fit[i] < fit[bi]) bi = i;
    memcpy(best, &X[bi*np], sizeof(float) * np);
    float bf = fit[bi];
    free(X); free(fit); free(trial); free(scratch);
    return bf;
}

static int g_polish = 1;   // --no-polish, so the polish can be A/B'd rather than believed
// ── detent polish ───────────────────────────────────────────────────────────
// Differential evolution cannot resolve a stepped axis: it moves by differences,
// and a detent is a flat plateau with a cliff at each end, so a step that would
// land in the right position looks no better than one that does not until it
// arrives. Since the positions are known and few, just TRY them all, holding
// everything else. Costs n renders per snapped axis and cannot make things worse,
// because the value the search chose is the incumbent.
static float score_patch(Ctx *c, const PmPatch *p, float *scratch)
{
    render_patch(p, scratch, c->t->len, c->midi, c->hold_ms);
    c->evals++;
    return pm_distance(c->t, scratch);
}

static float polish_detents(Ctx *c, PmPatch *p, float loss, float *scratch)
{
    const int dims[3] = { V_HARM, V_TIMB, V_MORPH };
    if (!g_polish) return loss;
    for (int d = 0; d < 3; d++) {
        const PmDetents *det = pm_detents_for(p->engine, dims[d]);
        if (!det) continue;
        float keep = p->v[dims[d]], best = loss, bestv = keep;
        int snapped = 0;
        for (int i = 0; i < det->n; i++) {
            p->v[dims[d]] = det->centre[i];
            float s = score_patch(c, p, scratch);
            if (s < best || (!snapped && s == best)) { best = s; bestv = det->centre[i]; snapped = 1; }
        }
        p->v[dims[d]] = bestv;
        loss = best;
    }
    return loss;
}

// ── dimension sets ──────────────────────────────────────────────────────────
static const int DIMS_RACE[]  = { V_HARM, V_TIMB, V_MORPH, V_ATK, V_DEC, V_SUS, V_REL, V_FMODE, V_CUT, V_RES };
// The VOICE stage is the instrument standing still: what it is, and its envelope
// and filter. No wobble of any kind lives here.
static const int DIMS_VOICE[] = { V_HARM, V_TIMB, V_MORPH, V_ATK, V_DEC, V_SUS, V_REL, V_FMODE, V_CUT, V_RES,
                                  V_ENVAMT, V_ENVDEC };
static const int DIMS_MODE[]  = { V_MODE0, V_MODE1, V_MODE2, V_MODE3 };
// The FX stage owns EVERY periodic modulation, whichever API it belongs to.
// Vibrato and tape wow are the same physical claim about the sound (the pitch
// wobbles), so they must be offered to the search together and priced together:
// with vibrato in the earlier stage it always won by arriving first, and a
// cassette's flutter came out described as an 0.85-semitone vibrato.
static const int DIMS_FX[]    = { V_VIBDEP, V_VIBRATE, V_TREMDEP,
                                  FXD(F_DRIVE), FXD(F_DRIVEMODE), FXD(F_TAPEWOW), FXD(F_TAPEFLUT), FXD(F_TAPESAT),
                                  FXD(F_CRUSHBITS), FXD(F_CRUSHRATE), FXD(F_CRUSHMIX), FXD(F_CHRATE), FXD(F_CHDEP), FXD(F_CHMIX),
                                  FXD(F_TREMRATE), FXD(F_TREMDEP), FXD(F_ECHOTIME), FXD(F_ECHOFB), FXD(F_ECHOTONE), FXD(F_ECHOSEND),
                                  FXD(F_RVBSIZE), FXD(F_RVBDAMP), FXD(F_RVBSEND), FXD(F_EQLOW), FXD(F_EQMID), FXD(F_EQHIGH) };
#define NDIM(a) ((int)(sizeof(a)/sizeof(a[0])))

typedef struct { float loss; int engine; PmPatch p; } Result;
static int cmp_result(const void *a, const void *b)
{ float d = ((const Result*)a)->loss - ((const Result*)b)->loss; return d < 0 ? -1 : (d > 0 ? 1 : 0); }

// ── output ──────────────────────────────────────────────────────────────────
static void write_wav(const char *path, const float *x, int n)
{
    FILE *f = fopen(path, "wb"); if (!f) return;
    int br = SR * 2; unsigned d = n * 2, r = 36 + d;
    fwrite("RIFF", 1, 4, f); fwrite(&r, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    unsigned x16 = 16; unsigned short one = 1, ch = 1, bits = 16, align = 2; unsigned sr = SR;
    fwrite(&x16,4,1,f); fwrite(&one,2,1,f); fwrite(&ch,2,1,f); fwrite(&sr,4,1,f);
    fwrite(&br,4,1,f); fwrite(&align,2,1,f); fwrite(&bits,2,1,f);
    fwrite("data",1,4,f); fwrite(&d,4,1,f);
    float pk = 0; for (int i = 0; i < n; i++) { float a = fabsf(x[i]); if (a > pk) pk = a; }
    float g = pk > 1e-9f ? 0.89f / pk : 0.0f;
    for (int i = 0; i < n; i++) { int v = (int)lrintf(x[i] * g * 32767.0f); if (v > 32767) v = 32767; if (v < -32768) v = -32768; short s = (short)v; fwrite(&s,2,1,f); }
    fclose(f);
}

static void print_snippet(FILE *o, const PmPatch *p, int midi, int hold_ms, int with_fx)
{
    const int s = PM_SLOT;
    fprintf(o, "    instrument(%d, %s, %d, %d, %d, %d);\n", s, pm_engine_name(p->engine),
            pm_atk_ms(p->v[V_ATK]), pm_dec_ms(p->v[V_DEC]), pm_sus(p->v[V_SUS]), pm_rel_ms(p->v[V_REL]));
    fprintf(o, "    instrument_harmonics(%d, %.3ff);  instrument_timbre(%d, %.3ff);  instrument_morph(%d, %.3ff);\n",
            s, p->v[V_HARM], s, p->v[V_TIMB], s, p->v[V_MORPH]);
    int fb = pm_bin(p->v[V_FMODE], 4);
    if (fb) fprintf(o, "    instrument_filter(%d, %s, %d, %d);\n", s, PM_FILTER_NAME[fb], pm_cut_hz(p->v[V_CUT]), pm_res(p->v[V_RES]));
    if (p->v[V_ENVAMT] > 0.01f && fb)
        fprintf(o, "    instrument_env(%d, 0, ENV_CUTOFF_OCT, 0, %d, %.2ff);\n", s, pm_env_ms(p->v[V_ENVDEC]), pm_env_oct(p->v[V_ENVAMT]));
    if (pm_vib_semi(p->v[V_VIBDEP]) > 0.005f)
        fprintf(o, "    instrument_lfo(%d, 0, LFO_PITCH, %.2ff, %.3ff);\n", s, pm_vib_hz(p->v[V_VIBRATE]), pm_vib_semi(p->v[V_VIBDEP]));
    if (p->v[V_TREMDEP] > 0.01f)
        fprintf(o, "    instrument_lfo(%d, 1, LFO_VOLUME, %.2ff, %.3ff);\n", s, pm_vib_hz(p->v[V_VIBRATE]), p->v[V_TREMDEP]);
    int midx[4], nm = pm_engine_modes(p->engine, midx);
    for (int i = 0; i < nm; i++) fprintf(o, "    instrument_mode(%d, %d, %.3ff);\n", s, midx[i], p->v[V_MODE0+i]);

    if (with_fx) {
        const float *f = p->f;
        if (f[F_DRIVE] > 0.02f) {
            fprintf(o, "    instrument_drive(%d, %.3ff);  instrument_drive_mode(%d, %s);\n", s, f[F_DRIVE], s, PM_DRIVE_NAME[pm_bin(f[F_DRIVEMODE],4)]);
        }
        if (f[F_TAPEWOW] > 0.02f || f[F_TAPEFLUT] > 0.02f || f[F_TAPESAT] > 0.02f)
            fprintf(o, "    instrument_tape(%d, %.3ff, %.3ff, %.3ff);\n", s, f[F_TAPEWOW], f[F_TAPEFLUT], f[F_TAPESAT]);
        if (f[F_CRUSHMIX] > 0.02f)
            fprintf(o, "    instrument_crush(%d, %.1ff, %.1ff, %.3ff);\n", s, pm_crush_bits(f[F_CRUSHBITS]), pm_crush_rate(f[F_CRUSHRATE]), f[F_CRUSHMIX]);
        if (f[F_CHMIX] > 0.02f)
            fprintf(o, "    instrument_chorus(%d, %.2ff, %.3ff, %.3ff);\n", s, pm_ch_rate(f[F_CHRATE]), f[F_CHDEP], f[F_CHMIX]);
        if (f[F_TREMDEP] > 0.02f)
            fprintf(o, "    instrument_tremolo(%d, %.2ff, %.3ff, LFO_SHAPE_SINE);\n", s, pm_trem_rate(f[F_TREMRATE]), f[F_TREMDEP]);
        if (f[F_ECHOSEND] > 0.02f) {
            fprintf(o, "    echo(%d, %.3ff, %.3ff);  instrument_echo(%d, %.3ff);\n", pm_echo_ms(f[F_ECHOTIME]), pm_echo_fb(f[F_ECHOFB]), f[F_ECHOTONE], s, f[F_ECHOSEND]);
        }
        if (f[F_RVBSEND] > 0.02f)
            fprintf(o, "    reverb(%.3ff, %.3ff);  instrument_reverb(%d, %.3ff);   // reverb() is the MASTER tank\n", f[F_RVBSIZE], f[F_RVBDAMP], s, f[F_RVBSEND]);
        if (fabsf(pm_eq_db(f[F_EQLOW])) > 0.5f || fabsf(pm_eq_db(f[F_EQMID])) > 0.5f || fabsf(pm_eq_db(f[F_EQHIGH])) > 0.5f)
            fprintf(o, "    instrument_eq(%d, %.1ff, %.1ff, %.1ff);\n", s, pm_eq_db(f[F_EQLOW]), pm_eq_db(f[F_EQMID]), pm_eq_db(f[F_EQHIGH]));
    }
    fprintf(o, "    hit(%d, %d, 5, %d);\n", midi, s, hold_ms);
}

static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

// ── forked fan-out. The engine holds process-global state, so the workers are
// PROCESSES, not threads. Forking happens before this process ever creates an
// instance, so no child inherits a half-built engine.
static int run_forked(int njobs, int nitems, void (*work)(int item, FILE *out), Result *into)
{
    char tmpl[256];
    snprintf(tmpl, sizeof tmpl, "/tmp/pm-%d", (int)getpid());
    for (int j = 0; j < njobs; j++) {
        pid_t pid = fork();
        if (pid == 0) {
            char path[300]; snprintf(path, sizeof path, "%s-%d", tmpl, j);
            FILE *o = fopen(path, "wb");
            for (int i = j; i < nitems; i += njobs) work(i, o);
            fclose(o);
            _exit(0);
        }
    }
    int st, got = 0;
    while (wait(&st) > 0) { }
    for (int j = 0; j < njobs; j++) {
        char path[300]; snprintf(path, sizeof path, "%s-%d", tmpl, j);
        FILE *in = fopen(path, "rb");
        if (!in) continue;
        Result r;
        while (fread(&r, sizeof r, 1, in) == 1) into[got++] = r;
        fclose(in); unlink(path);
    }
    return got;
}

// ── globals the forked workers read (set before any fork) ───────────────────
static PmTarget g_short, g_full;
static int g_midi, g_hold_short, g_hold_full;
static int g_quick = 0;
static unsigned g_seed = 7;
static Result g_race[PM_NENGINES];
static int g_elist[PM_NENGINES], g_ne = PM_NENGINES;   // which engines the race runs
static int g_race_pop = 0, g_race_gens = 0, g_stage1_only = 0;            // 0 = the built-in budget
static Result g_cand[16];
static int g_ncand = 0;

static void work_race(int i, FILE *out)
{
    Ctx c = { .t = &g_short, .dims = DIMS_RACE, .ndims = NDIM(DIMS_RACE), .fx = 0,
              .midi = g_midi, .hold_ms = g_hold_short };
    pm_patch_default(&c.base, g_elist[i]);
    float x[NDIM(DIMS_RACE)];
    int pop = g_race_pop ? g_race_pop : (g_quick ? 12 : 16);
    int gens = g_race_gens ? g_race_gens : (g_quick ? 12 : 22);
    float loss = de_search(&c, x, pop, gens, g_seed + i * 101, NULL);
    Result r = { .loss = loss, .engine = g_elist[i] };
    r.p = c.base;
    for (int k = 0; k < c.ndims; k++) pm_put(&r.p, c.dims[k], x[k]);
    float *scratch = (float*)malloc(sizeof(float) * c.t->len);
    r.loss = polish_detents(&c, &r.p, r.loss, scratch);   // fairness: an engine with a
    free(scratch);                                        // stepped axis must not lose the race to it
    fwrite(&r, sizeof r, 1, out);
}

static int g_refine_engine[16], g_refine_seed[16];
static PmPatch g_refine_seedpatch[16];

static void work_refine(int i, FILE *out)
{
    int midx[4], nm = pm_engine_modes(g_refine_engine[i], midx);
    int dims[NDIM(DIMS_VOICE) + 4]; int nd = 0;
    for (int k = 0; k < NDIM(DIMS_VOICE); k++) dims[nd++] = DIMS_VOICE[k];
    for (int k = 0; k < nm; k++) dims[nd++] = DIMS_MODE[k];

    Ctx c = { .t = &g_full, .dims = dims, .ndims = nd, .fx = 0, .midi = g_midi, .hold_ms = g_hold_full };
    c.base = g_refine_seedpatch[i];
    c.base.nmode = nm;              // the refine stage is where these dials become the search's
    float x[NDIM(DIMS_VOICE) + 4], sv[NDIM(DIMS_VOICE) + 4];
    for (int k = 0; k < nd; k++) sv[k] = pm_get(&c.base, dims[k]);
    int pop = g_quick ? 20 : 30, gens = g_quick ? 30 : 70;
    float loss = de_search(&c, x, pop, gens, g_refine_seed[i], sv);
    Result r = { .loss = loss, .engine = g_refine_engine[i] };
    r.p = c.base;
    for (int k = 0; k < nd; k++) pm_put(&r.p, dims[k], x[k]);
    float *scratch = (float*)malloc(sizeof(float) * c.t->len);
    r.loss = polish_detents(&c, &r.p, r.loss, scratch);
    free(scratch);
    fwrite(&r, sizeof r, 1, out);
}

static void work_fx(int i, FILE *out)
{
    Ctx c = { .t = &g_full, .dims = DIMS_FX, .ndims = NDIM(DIMS_FX), .fx = 1,
              .midi = g_midi, .hold_ms = g_hold_full, .sparsity = 0.003f };
    c.base = g_cand[i].p;
    float x[NDIM(DIMS_FX)], sv[NDIM(DIMS_FX)];
    // Seed individual 0 with BYPASS. Every effect switched off is a point in this
    // space and it scores exactly the frozen voice, so with it in the population
    // the fx stage can only improve or tie. Without it the search starts from 23
    // random effect settings and its best may be WORSE than adding nothing --
    // which is what the first run did, and it reads as "the fx made it worse"
    // rather than as "the optimizer never found the off switch".
    PmPatch byp; pm_patch_default(&byp, c.base.engine);
    for (int k = 0; k < c.ndims; k++) sv[k] = pm_get(&byp, c.dims[k]);
    int pop = g_quick ? 24 : 34, gens = g_quick ? 25 : 55;
    float loss = de_search(&c, x, pop, gens, g_seed + 977 * i, sv);
    Result r = { .loss = loss, .engine = c.base.engine };
    r.p = c.base;
    for (int k = 0; k < c.ndims; k++) pm_put(&r.p, c.dims[k], x[k]);
    fwrite(&r, sizeof r, 1, out);
}

int main(int argc, char **argv)
{
    const char *path = NULL, *outdir = "build/patch-match";
    float wsecs = 1.2f, wshort = 0.5f;
    int jobs = 4, want = 8, forced_midi = 0;
    const char *selftest = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--quick")) g_quick = 1;
        else if (!strcmp(argv[i], "--window") && i+1 < argc) wsecs = atof(argv[++i]);
        else if (!strcmp(argv[i], "--jobs") && i+1 < argc) jobs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) g_seed = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--note") && i+1 < argc) forced_midi = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i+1 < argc) outdir = argv[++i];
        else if (!strcmp(argv[i], "--stage1")) g_stage1_only = 1;
        else if (!strcmp(argv[i], "--no-polish")) g_polish = 0;
        else if (!strcmp(argv[i], "--selftest") && i+1 < argc) selftest = argv[++i];
        else if (!strcmp(argv[i], "--race-pop") && i+1 < argc) g_race_pop = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--race-gens") && i+1 < argc) g_race_gens = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--engine") && i+1 < argc) {
            const char *want = argv[++i]; g_ne = 0;
            for (int k = 0; k < PM_NENGINES; k++)
                if (!strcasecmp(PM_ENGINE_NAME[k] + 6, want)) g_elist[g_ne++] = PM_ENGINE[k];
            if (!g_ne) for (int k = 0; k < PM_NENGINES; k++)
                if (strcasestr(PM_ENGINE_NAME[k], want)) g_elist[g_ne++] = PM_ENGINE[k];
            if (!g_ne) { fprintf(stderr, "pm: no engine matching '%s'\n", want); return 2; }
        }
        else if (argv[i][0] != '-') path = argv[i];
    }
    if (g_ne == PM_NENGINES) for (int k = 0; k < PM_NENGINES; k++) g_elist[k] = PM_ENGINE[k];
    if (!path && !selftest) { fprintf(stderr, "usage: pm <sample.wav> [--quick] [--window s] [--jobs n] [--seed n] [--note midi] [--out dir]\n"); return 2; }

    // ── target prep ─────────────────────────────────────────────────────────
    // THE ORACLE. --selftest renders a patch WE chose with the real engine, throws
    // the parameters away, and asks the search to find them again. It is the only
    // thing that separates "this engine genuinely cannot make that sample" from
    // "the search is broken", because here the answer is known to be reachable:
    // the target came out of the very engine being searched.
    float *raw = NULL, *rs = NULL; int sr = SR, m = 0, on = 0; float f0 = 0;
    PmPatch truth; int have_truth = 0;
    if (selftest) {
        int e = -1;   // exact first: "PIANO" must not resolve to EPIANO (see --engine)
        for (int k = 0; k < PM_NENGINES; k++) if (!strcasecmp(PM_ENGINE_NAME[k] + 6, selftest)) { e = PM_ENGINE[k]; break; }
        if (e < 0) for (int k = 0; k < PM_NENGINES; k++) if (strcasestr(PM_ENGINE_NAME[k], selftest)) { e = PM_ENGINE[k]; break; }
        if (e < 0) { fprintf(stderr, "pm: no engine matching '%s'\n", selftest); return 2; }
        pm_patch_default(&truth, e); have_truth = 1;
        truth.v[V_HARM]=0.62f; truth.v[V_TIMB]=0.28f; truth.v[V_MORPH]=0.74f;
        truth.v[V_ATK]=0.10f;  truth.v[V_DEC]=0.44f;  truth.v[V_SUS]=0.55f; truth.v[V_REL]=0.35f;
        truth.v[V_FMODE]=0.30f; truth.v[V_CUT]=0.68f; truth.v[V_RES]=0.22f;
        g_midi = forced_midi ? forced_midi : 60;
        m = (int)(wsecs * SR) + SR/4;
        rs = (float*)malloc(sizeof(float) * m);
        render_patch(&truth, rs, m, g_midi, (int)(wsecs * 1000));
        path = "(self-test)";
    } else {
        const char *err;
        int n = pmw_load(path, &raw, &sr, &err);
        if (!n) { fprintf(stderr, "pm: %s: %s\n", path, err); return 1; }
        m = pmw_resample(raw, n, sr, SR, &rs);
        on = pmw_onset(rs, m, SR);
        f0 = pmw_pitch(rs + on, m - on, SR);
        g_midi = forced_midi ? forced_midi : (f0 > 0 ? pmw_midi_of(f0) : 60);
    }

    int full = (int)(wsecs * SR), shortn = (int)(wshort * SR);
    if (on + full > m) full = m - on;
    if (full < SR / 4) { fprintf(stderr, "pm: sample too short after onset trim\n"); return 1; }
    if (shortn > full) shortn = full;
    g_hold_full = (int)(full * 1000.0f / SR);
    g_hold_short = (int)(shortn * 1000.0f / SR);

    const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
    printf("\n  \033[1m%s\033[0m\n", base);
    if (selftest) printf("  ORACLE: target rendered from %s, midi %d. The answer is reachable by construction.\n",
                         pm_engine_name(truth.engine), g_midi);
    else {
        printf("  %d Hz -> %d, onset %.3fs, ", sr, SR, on / (double)SR);
        if (f0 > 0) printf("f0 %.1f Hz = midi %d (%+.0f cents)\n", f0, g_midi, pmw_cents_off(f0, g_midi));
        else        printf("no clear pitch, playing midi %d\n", g_midi);
    }
    printf("  window %.2fs (race %.2fs), %d jobs%s\n\n", full / (double)SR, shortn / (double)SR, jobs, g_quick ? ", quick" : "");

    pm_target_init(&g_full,  rs + on, full,   SR);
    pm_target_init(&g_short, rs + on, shortn, SR);

    char cmd[512]; snprintf(cmd, sizeof cmd, "mkdir -p '%s'", outdir); if (system(cmd)) {}
    char p2[600]; snprintf(p2, sizeof p2, "%s/target.wav", outdir);
    write_wav(p2, rs + on, full);

    double t0 = now_s();

    // ── stage 1: the engine race ────────────────────────────────────────────
    printf("  \033[1mstage 1\033[0m  engine race (%d engine%s)\n", g_ne, g_ne == 1 ? "" : "s");
    int got = run_forked(jobs, g_ne, work_race, g_race);
    qsort(g_race, got, sizeof(Result), cmp_result);
    for (int i = 0; i < got; i++)
        printf("    %2d. %-15s %.5f\n", i + 1, pm_engine_name(g_race[i].engine), g_race[i].loss);

    if (g_stage1_only) { printf("\n  (--stage1: stopping after the race, %.0fs)\n\n", now_s() - t0); return 0; }

    // ── stage 2: refine the top engines, several seeds each ─────────────────
    // topk is 3 even in --quick, and that is not a rounding choice. MEASURED over
    // 24 oracle runs (6 engines x 4 seeds): the right engine placed in the top THREE
    // 24/24 times but placed FIRST only 21/24, and INSTR_PD never placed first at
    // all (its phase-distortion tones are reachable by SAW and friends, so it is
    // genuinely ambiguous rather than badly searched). It ranked 3rd on one seed,
    // so a --quick that refined only the top 2 would have thrown the correct answer
    // away before stage 2 ever saw it. Quick mode buys its speed from pop/gens,
    // never from cutting the shortlist.
    int topk = 3, seeds = g_quick ? 2 : 3, nref = 0;
    if (topk > got) topk = got;
    for (int e = 0; e < topk && e < got; e++)
        for (int s = 0; s < seeds; s++) {
            g_refine_engine[nref] = g_race[e].engine;
            g_refine_seed[nref] = g_seed + 31 * e + 7919 * s;
            g_refine_seedpatch[nref] = g_race[e].p;
            nref++;
        }
    printf("\n  \033[1mstage 2\033[0m  voice refine (top %d engines x %d seeds)\n", topk, seeds);
    Result ref[16];
    int nr = run_forked(jobs, nref, work_refine, ref);
    qsort(ref, nr, sizeof(Result), cmp_result);
    for (int i = 0; i < nr; i++) printf("    %-15s %.5f\n", pm_engine_name(ref[i].engine), ref[i].loss);

    g_ncand = nr < want ? nr : want;
    for (int i = 0; i < g_ncand; i++) g_cand[i] = ref[i];

    // ── stage 3: fx on top, voice frozen ────────────────────────────────────
    printf("\n  \033[1mstage 3\033[0m  fx fit (voice frozen)\n");
    Result fx[16];
    int nf = run_forked(jobs, g_ncand, work_fx, fx);

    // Re-score without the sparsity cost so the printed number is comparable
    // with stage 2's, and keep fx only where they actually helped.
    float *scratch = (float*)malloc(sizeof(float) * g_full.len);
    for (int i = 0; i < nf; i++) {
        render_patch(&fx[i].p, scratch, g_full.len, g_midi, g_hold_full);
        fx[i].loss = pm_distance(&g_full, scratch);
    }
    qsort(fx, nf, sizeof(Result), cmp_result);

    // ── report ──────────────────────────────────────────────────────────────
    double secs = now_s() - t0;
    printf("\n  \033[1mcandidates\033[0m  (%.0fs)\n\n", secs);
    char rpt[600]; snprintf(rpt, sizeof rpt, "%s/patches.txt", outdir);
    FILE *o = fopen(rpt, "w");
    fprintf(o, "// patch-match: %s\n// f0 %.1f Hz -> midi %d, window %.2fs\n\n", base, f0, g_midi, full / (double)SR);
    for (int i = 0; i < nf; i++) {
        // was this candidate better before the fx were added?
        float voice_loss = 0;
        for (int k = 0; k < g_ncand; k++)
            if (g_cand[k].p.engine == fx[i].p.engine &&
                memcmp(g_cand[k].p.v, fx[i].p.v, sizeof(float) * V_VIBDEP) == 0) voice_loss = g_cand[k].loss;
        printf("  %d. \033[1m%-15s\033[0m voice %.5f -> with fx \033[1m%.5f\033[0m\n", i+1, pm_engine_name(fx[i].engine), voice_loss, fx[i].loss);
        fprintf(o, "// ---- candidate %d: %s   voice %.5f -> fx %.5f\n", i+1, pm_engine_name(fx[i].engine), voice_loss, fx[i].loss);
        print_snippet(o, &fx[i].p, g_midi, g_hold_full, 1);
        fprintf(o, "\n");
        char wp[600]; snprintf(wp, sizeof wp, "%s/cand-%d.wav", outdir, i+1);
        render_patch(&fx[i].p, scratch, g_full.len, g_midi, g_hold_full);
        write_wav(wp, scratch, g_full.len);
    }
    fclose(o);
    printf("\n  wrote %s/  (target.wav, cand-1..%d.wav, patches.txt)\n", outdir, nf);

    int rc = 0;
    if (have_truth) {
        // Two questions, and the second is the one that matters. Finding the right
        // ENGINE is most of the job; the parameters only have to land close enough
        // that the render matches, and several settings can sound the same.
        int right = nf > 0 && fx[0].engine == truth.engine;
        int rank = -1;
        for (int i = 0; i < got; i++) if (g_race[i].engine == truth.engine) rank = i + 1;
        printf("\n  \033[1moracle\033[0m\n");
        printf("    truth engine      %s\n", pm_engine_name(truth.engine));
        printf("    race rank         %d of %d\n", rank, got);
        printf("    winner            %s  %s\n", pm_engine_name(fx[0].engine),
               right ? "\033[32mCORRECT\033[0m" : "\033[31mWRONG\033[0m");
        float best_wrong = 9e9f;
        for (int i = 0; i < got; i++) if (g_race[i].engine != truth.engine && g_race[i].loss < best_wrong) best_wrong = g_race[i].loss;
        printf("    loss              %.5f  %s\n", fx[0].loss,
               fx[0].loss < 0.15f ? "\033[32mrecovered\033[0m" : "\033[31mtoo far\033[0m");
        printf("    race margin       %.5f vs %.5f for the best wrong engine (%.1fx)\n",
               rank > 0 ? g_race[rank-1].loss : -1.0f, best_wrong,
               rank > 0 && g_race[rank-1].loss > 0 ? best_wrong / g_race[rank-1].loss : 0.0f);
        if (right) {
            printf("    %-12s %8s %8s\n", "param", "truth", "found");
            const char *nm[] = {"harmonics","timbre","morph","attack","decay","sustain","release"};
            int ix[] = {V_HARM,V_TIMB,V_MORPH,V_ATK,V_DEC,V_SUS,V_REL};
            for (int k = 0; k < 7; k++) {
                printf("    %-12s %8.3f %8.3f", nm[k], truth.v[ix[k]], fx[0].p.v[ix[k]]);
                // On a SNAPPED axis the raw difference is not an error and must not be
                // printed as one: two values inside one detent are the SAME setting and
                // render byte-identically. Compare the detent, which is the real quantity.
                const PmDetents *det = pm_detents_for(truth.engine, ix[k]);
                if (det) {
                    int a = 0, b = 0;
                    for (int i = 1; i < det->n; i++) {
                        if (fabsf(truth.v[ix[k]]     - det->centre[i]) < fabsf(truth.v[ix[k]]     - det->centre[a])) a = i;
                        if (fabsf(fx[0].p.v[ix[k]]   - det->centre[i]) < fabsf(fx[0].p.v[ix[k]]   - det->centre[b])) b = i;
                    }
                    printf("   detent %d/%d vs %d/%d  %s", a+1, det->n, b+1, det->n,
                           a == b ? "\033[32msame\033[0m" : "\033[31mdifferent\033[0m");
                }
                printf("\n");
            }
        }
        rc = (right && fx[0].loss < 0.15f) ? 0 : 1;
        printf("\n  %s\n", rc == 0 ? "\033[32m  ORACLE PASS\033[0m" : "\033[31m  ORACLE FAIL — the search, not the sample, is the problem\033[0m");
    }
    printf("\n");
    free(scratch); free(raw); free(rs);
    pm_target_free(&g_full); pm_target_free(&g_short);
    return rc;
}
