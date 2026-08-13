// probe.c — the host half of the engine-as-dylib spike. See run.sh for what this decides.
//
// It plays the role the AUv3 extension would play: load the engine N times, hand each load its own
// transport, and check the engines are strangers to each other. The engine's own header is the whole
// interface (ios/Sources/engine.h) — nothing new is being invented, only loaded differently.
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/resource.h>

// The seam now names its instance (docs/design/engine-instance-seam.md). This probe still gets its
// separation from dyld — two COPIES of the dylib are two data segments — but it holds a handle per
// engine because the API requires one. When the context refactor lands, `create` stops being a
// per-dylib call and becomes two calls into ONE image, and every assertion below is unchanged.
typedef void *(*fn_create)(int);
typedef void (*fn_frame)(void *, double);
typedef int  (*fn_copy)(void *, uint32_t *, int, int *, int *);
typedef void (*fn_sync)(void *, double, double, int);
typedef void (*fn_audio)(void *, float *, int);
typedef int  (*fn_dim)(void *);

typedef struct {
    void *h; const char *label;
    void *in;   // this engine's instance handle
    fn_create create; fn_frame frame; fn_copy copy; fn_sync sync; fn_audio audio; fn_dim w, hgt;
} Engine;

static int load(Engine *e, const char *path, const char *label) {
    e->h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!e->h) { printf("  ✗ dlopen %s failed: %s\n", path, dlerror()); return 0; }
    e->label = label;
    e->create = (fn_create) dlsym(e->h, "de_instance_create");
    e->frame = (fn_frame) dlsym(e->h, "de_frame");
    e->copy  = (fn_copy)  dlsym(e->h, "de_copy_frame");
    e->sync  = (fn_sync)  dlsym(e->h, "de_sync_position");
    e->audio = (fn_audio) dlsym(e->h, "de_audio_render");
    e->w     = (fn_dim)   dlsym(e->h, "de_screen_w");
    e->hgt   = (fn_dim)   dlsym(e->h, "de_screen_h");
    if (!e->create || !e->frame || !e->copy || !e->sync || !e->audio || !e->w || !e->hgt) {
        printf("  ✗ %s: a symbol is missing from %s\n", label, path); return 0;
    }
    return 1;
}

static float peak(const float *b, int n);

// Drive one engine: its OWN transport, its OWN number of frames. This is the AUv3 two-track case —
// two hosts' worth of playhead arriving at two instances. Returns the peak audio level it produced.
//
// ⚠ It renders AUDIO EVERY FRAME, one frame's worth (735 samples @ 44.1k, the AU's
// SAMPLES_PER_FRAME), because that is the only faithful shape: sound.h has a bounded REQUEST QUEUE
// that de_audio_render drains. The first cut ticked 90 frames and rendered once at the end, which
// buried the result under a hundred "[sound] WARNING: request queue overflow — N sound call(s)
// DROPPED" lines and left B's peak pinned at 1.0000 as the whole backlog fired at once. That is a
// PROBE artefact, not an engine defect — but it is exactly the sort of scary output that gets
// reported as a finding, so the probe should not manufacture it.
static float drive(Engine *e, double beats, double bpm, int playing, int frames) {
    static float chunk[735 * 2];
    float p = 0;
    for (int f = 0; f < frames; f++) {
        e->sync(e->in, beats + f * 0.25, bpm, playing);
        e->frame(e->in, f / 60.0);
        e->audio(e->in, chunk, 735);
        float q = peak(chunk, 735 * 2);
        if (q > p) p = q;
    }
    return p;
}

// The published frame, as bytes. NOT the live canvas: de_copy_frame is the snapshot seam, which is
// what a plug-in view uses and what present-race-check gates.
static uint32_t *grab(Engine *e, int *npx) {
    int w = 0, h = 0;
    e->copy(e->in, NULL, 0, &w, &h);                      // dst == NULL: report the size, copy nothing
    if (w <= 0 || h <= 0) { *npx = 0; return NULL; }
    uint32_t *buf = malloc((size_t)w * h * 4);
    if (!e->copy(e->in, buf, w * h, &w, &h)) { free(buf); *npx = 0; return NULL; }
    *npx = w * h;
    return buf;
}

static int nonblank(const uint32_t *px, int n) {   // more than one distinct colour = something drew
    if (!px || n < 2) return 0;
    for (int i = 1; i < n; i++) if (px[i] != px[0]) return 1;
    return 0;
}

static float peak(const float *b, int n) {
    float p = 0; for (int i = 0; i < n; i++) { float a = b[i] < 0 ? -b[i] : b[i]; if (a > p) p = a; }
    return p;
}

static int failures = 0;
static void check(const char *name, int ok, const char *fmt, ...) {
    va_list ap; char detail[512];
    va_start(ap, fmt); vsnprintf(detail, sizeof detail, fmt, ap); va_end(ap);
    printf("  %s %s  — %s\n", ok ? "✓" : "✗", name, detail);
    if (!ok) failures++;
}

static long rss_kb(void) {
    struct rusage ru; getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss / 1024;                    // darwin reports bytes
}

int main(int argc, char **argv) {
    const char *a = argc > 1 ? argv[1] : "./libengine_a.dylib";
    const char *b = argc > 2 ? argv[2] : "./libengine_b.dylib";
    const char *c = argc > 3 ? argv[3] : NULL;     // optional: a DIFFERENT cart

    long rss0 = rss_kb();

    // ── 1. THE SHIPPING SHAPE: one engine built once, shipped as K pre-signed copies ──────────────
    printf("▸ two COPIES of one engine dylib, driven with different transport\n");
    Engine ea = {0}, eb = {0};
    if (!load(&ea, a, "A") || !load(&eb, b, "B")) return 1;
    check("the two loads are distinct dyld images", ea.h != eb.h,
          "handles %p vs %p", ea.h, eb.h);
    ea.in = ea.create(0); eb.in = eb.create(0);     // 0 == DE_RENDERER_SOFTWARE
    long rss2 = rss_kb();
    float pa = drive(&ea, 0.0,   90.0, 0, 40);     // stopped at the top
    float pb = drive(&eb, 64.0, 160.0, 1, 90);     // mid-song, playing, faster, more frames
    int na = 0, nb = 0;
    uint32_t *fa = grab(&ea, &na), *fb = grab(&eb, &nb);
    check("both engines published a frame", fa && fb && na > 0 && nb == na,
          "%d px and %d px (%dx%d)", na, nb, ea.w(ea.in), ea.hgt(ea.in));
    check("both drew something", nonblank(fa, na) && nonblank(fb, nb),
          "neither frame is one flat colour");
    check("THE POINT: their frames DIFFER, so their state is independent",
          fa && fb && na == nb && memcmp(fa, fb, (size_t)na * 4) != 0,
          "same cart, different transport → different picture");

    // The AUDIO half of independence, and it is a stronger signal than the picture: A was handed a
    // STOPPED transport and B a playing one, so a shared engine could not have produced both.
    check("each engine hears its own transport", pb > 0.01f && pa < pb * 0.5f,
          "A peak %.4f (host stopped) vs B peak %.4f (host playing)", pa, pb);

    // ── 2. THE NEGATIVE CONTROL ──────────────────────────────────────────────────────────────────
    // The same dylib PATH twice. dyld refcounts by file, so this must come back as ONE image with
    // ONE set of globals — i.e. exactly today's bug. Without this, "the frames differed" could mean
    // the probe simply cannot see sharing, and the whole result would be worthless.
    printf("▸ NEGATIVE CONTROL: the same path twice (must SHARE state)\n");
    Engine sa = {0}, sb = {0};
    if (!load(&sa, a, "S1") || !load(&sb, a, "S2")) return 1;
    check("dyld hands back the SAME image for one path", sa.h == sb.h,
          "handles %p and %p", sa.h, sb.h);
    drive(&sa, 0.0, 90.0, 0, 5);
    int ns1 = 0, ns2 = 0;
    uint32_t *s1 = grab(&sa, &ns1);
    uint32_t *s2 = grab(&sb, &ns2);               // no driving: sees whatever S1 left behind
    check("and therefore ONE engine: both reads are byte-identical",
          s1 && s2 && ns1 == ns2 && memcmp(s1, s2, (size_t)ns1 * 4) == 0,
          "this is the defect, reproduced on purpose — the check above can go red");

    // ── 3. BONUS: two DIFFERENT carts alive at once ───────────────────────────────────────────────
    if (c) {
        printf("▸ BONUS: a SECOND CART in the same process\n");
        Engine ec = {0};
        if (load(&ec, c, "C")) {
            ec.in = ec.create(0);
            drive(&ec, 0.0, 120.0, 1, 40);
            int nc = 0; uint32_t *fc = grab(&ec, &nc);
            check("the other cart runs alongside, drawing its own frame",
                  fc && nc > 0 && nonblank(fc, nc),
                  "%d px (%dx%d) — a different cart's canvas size and contents", nc, ec.w(ec.in), ec.hgt(ec.in));
        }
    }

    printf("\nRSS: %ld MB before any load → %ld MB with two engines up (peak %ld MB)\n",
           rss0 / 1024, rss2 / 1024, rss_kb() / 1024);
    printf("%s\n", failures == 0
        ? "PASS — one engine dylib, K copies, K independent engines in one process."
        : "FAIL — see the ✗ rows above.");
    return failures == 0 ? 0 : 1;
}
