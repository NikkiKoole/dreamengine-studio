/*
 * main.c — times the three access shapes against each other and prints the verdict.
 *
 * Reports ns per sample and, more usefully, what fraction of ONE CPU each shape costs to keep a
 * 44.1 kHz stream fed — because that is the number that decides whether option (b) is affordable.
 *
 * It also asserts all six builds return the SAME accumulated value. They run identical arithmetic
 * on identical state, so a difference would mean the harness is not measuring what it claims.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SR 44100
#define SAMPLES (SR * 5)     /* 5 seconds of audio per trial */
#define TRIALS  25

float run_plain(int);      void setup_plain(void);
float run_tls(int);        void setup_tls(void);
float run_arg(int);        void setup_arg(void);
float run_plain_i(int);    void setup_plain_i(void);
float run_tls_i(int);      void setup_tls_i(void);
float run_arg_i(int);      void setup_arg_i(void);

typedef struct { const char *name; float (*run)(int); void (*setup)(void); double ns; float out; } Variant;

static double now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static void measure(Variant *v) {
    v->setup();
    double best = 1e30;
    float out = 0;
    for (int t = 0; t < TRIALS; t++) {
        v->setup();                      /* same starting state every trial */
        double t0 = now_ns();
        out = v->run(SAMPLES);
        double dt = now_ns() - t0;
        if (dt < best) best = dt;        /* min-of-N: the run least disturbed by the scheduler */
    }
    v->ns = best / (double)SAMPLES;
    v->out = out;
}

int main(void) {
    Variant vs[] = {
        { "(today)  plain statics",        run_plain,   setup_plain   },
        { "(b)      thread-local ctx",     run_tls,     setup_tls     },
        { "(c)      ctx as a parameter",   run_arg,     setup_arg     },
        { "(today)  plain, inlinable",     run_plain_i, setup_plain_i },
        { "(b)      thread-local, inlinable", run_tls_i, setup_tls_i  },
        { "(c)      parameter, inlinable", run_arg_i,   setup_arg_i   },
    };
    const int n = (int)(sizeof vs / sizeof vs[0]);
    for (int i = 0; i < n; i++) measure(&vs[i]);

    printf("\nHOW THE CONTEXT IS REACHED — cost per sample, %d voices + 6 stage calls, min of %d trials\n",
           8, TRIALS);
    printf("(the arithmetic is byte-identical in all six; only the access mechanism differs)\n\n");
    printf("  %-34s %10s %10s %14s\n", "variant", "ns/sample", "vs today", "% of one CPU");
    printf("  %-34s %10s %10s %14s\n", "----------------------------------", "---------", "--------", "------------");
    double base = vs[0].ns, base_i = vs[3].ns;
    for (int i = 0; i < n; i++) {
        double b = (i < 3) ? base : base_i;
        double pct_cpu = vs[i].ns * SR / 1e9 * 100.0;
        char delta[32];
        if (vs[i].ns == b) snprintf(delta, sizeof delta, "%s", "-");
        else snprintf(delta, sizeof delta, "%+.1f%%", 100.0 * (vs[i].ns - b) / b);
        if (i == 3) printf("\n  ---- with the stage functions small enough to inline ----\n");
        printf("  %-34s %10.3f %10s %13.2f%%\n", vs[i].name, vs[i].ns, delta, pct_cpu);
    }

    /* The figure that transfers to the real engine. This benchmark makes 6 opaque calls per
     * sample; sound.h makes a different number, so quote the PER-CALL cost, not the percentage. */
    const int CALLS_PER_SAMPLE = 6;
    double per_call = (vs[1].ns - vs[0].ns) / CALLS_PER_SAMPLE;
    double per_call_arg = (vs[2].ns - vs[0].ns) / CALLS_PER_SAMPLE;
    printf("\n  WHAT TRANSFERS TO THE REAL ENGINE (this loop makes %d opaque calls per sample;\n"
           "  sound.h makes its own number, so scale by THIS, not by the %% above):\n\n", CALLS_PER_SAMPLE);
    printf("    option (b) thread-local: %+.2f ns per function entry  =  %+.1f us/sec per call-per-sample\n",
           per_call, per_call * SR / 1000.0);
    printf("    option (c) parameter:    %+.2f ns per function entry  =  %+.1f us/sec per call-per-sample\n",
           per_call_arg, per_call_arg * SR / 1000.0);
    printf("\n    so if the engine makes N function entries per sample, option (b) costs about\n"
           "    N x %.1f us of every second, i.e. N x %.4f%% of one core. At N=100: %.2f%% of a core.\n",
           per_call * SR / 1000.0, per_call * SR / 1e9 * 100.0, per_call * 100 * SR / 1e9 * 100.0);

    int same = 1;
    for (int i = 1; i < n; i++) if (memcmp(&vs[i].out, &vs[0].out, sizeof(float)) != 0) same = 0;
    printf("\n  all six produce identical output: %s (%.6f)\n", same ? "yes" : "NO — the harness is lying", vs[0].out);
    printf("\n  Read the %% of one CPU column, not the percentage delta: the question is not\n"
           "  'is it slower' but 'does it eat a budget we need'.\n\n");
    return same ? 0 : 1;
}
