/* de:meta
{
  "slug": "wowflutter",
  "title": "wow & flutter",
  "status": "active",
  "created": "2026-06-11",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "verlet-integration",
    "soft-body",
    "sonification"
  ],
  "lineage": "Standalone tech-demo descended from the physics.c verlet core; novel in coupling the wow/flutter/saturation tape parameters bidirectionally — the same float drives both the jelly deformation and the real tape() master insert.",
  "description": "The tape() effect made physical: a soft-body jelly blob in a scene that is playing back on a worn tape, where the SAME wow/flutter/saturation values drive the picture AND the sound in lockstep - no screen shader, the wobble IS the verlet simulation. The tape WEARS DOWN as it plays (degradation creeps up on its own) so the longer it runs the woozier it gets: WOW (slow) sways the whole scene and drags the loop transport; FLUTTER (fast) shivers the jelly rim and stutters the timing; SATURATION blooms the palette warm, fattens the blobs, and drives the mix harder through the real tape() master insert. A Frippertronics-style pad+bell loop smears through the echo bus while the blobs throb on the beat; the character bumps are foley. Grain, scanlines and dropout bands thicken with the wear. Hit SPACE to REWIND & CLEAN - snap it fresh and watch/hear the decay begin again. Move the blob: arrows/WASD. Bounce: Z or Up. SPACE: rewind."
}
de:meta */
#include "studio.h"

// WOW & FLUTTER — a soft-body jelly blob in a scene that's playing back on a worn
// tape. There's no screen shader here: the wobble IS the simulation. The tape's
// three values drive the verlet jelly and the sound together —
//   • WOW  (slow) — the whole scene sways and breathes; the loop drags and lolls
//   • FLUTTER (fast) — everything trembles and shivers; the loop stutters
//   • SATURATION — colours bloom warm, the blobs fatten, the mix drives harder
// The tape WEARS DOWN as it plays: wear creeps up on its own, so the longer you let
// it run the woozier it gets — until you REWIND & CLEAN (SPACE) to snap it fresh.
// A Frippertronics-style loop plays under it; the blob's bumps are foley. Move the
// blob: arrows / WASD.   Bounce: Z or Up.   SPACE: rewind & clean.
//
// ─── THE TAPE LINK ───────────────────────────────────────────────────────────
// The master insert tape(wow, flutter, saturation) is LIVE: the same three values
// that wobble the picture drive the sound for real. On top of it the transport
// itself drags and stutters (the loop timing warbles with wow/flutter) and the
// echo bus smears each pass — so the audio falls apart in lockstep with the jelly.
// Set TAPE to 0 to mute just the master insert (the picture still wobbles).
#define TAPE 1
// ───────────────────────────────────────────────────────────────────────────────

// ── verlet core (reused from physics.c — points remember last position; sticks
//    hold a rest length; relax a few times per frame) ──────────────────────────
#define MAXP 96
#define MAXS 192
#define ITERS 5
#define GRAV  0.13f          // light: a floaty jelly tank, not a platformer
#define DAMP  0.99f
#define REST  0.62f
#define SUBSTEPS 2
#define SLOMO 0.5f           // global slow-motion: <1 = dreamier/slower whole sim.
                             // gravity scales by SLOMO² (it's an acceleration),
                             // every velocity-impulse + LFO sway-rate by SLOMO.

typedef struct { float x, y, px, py, r, w; int col; } Pt;
typedef struct { int a, b; float len, len0; } Stk;     // len0 = pristine rest length
typedef struct { int hub, rim0, n, col, eyecol; bool chr; } Blob;

static Pt   pts[MAXP];  static int npt;
static Stk  stks[MAXS]; static int nstk;
static Blob blob[8];    static int nblob;

// tape state
static float wear;                 // 0..1 — climbs on its own, SPACE resets
static float wow, flutter, sat;    // derived, 0..1 — drive picture AND (later) sound
static float wowph, flutph, beatph;// LFO phases (degrees) + breathing
static int   rewind;               // rewind-flash countdown

// music
static float songpos;              // position in beats (advances modulated by wow/flutter)
static int   lastbeat = -1, laststep = -1;
static int   padh[3];              // held pad-chord note handles
static int   beatpulse;            // frames since last beat (for the throb)

static float len2(float x, float y) { return fsqrt(x * x + y * y); }

static int add_pt(float x, float y, float r, int col) {
    if (npt >= MAXP) return npt - 1;
    pts[npt] = (Pt){ x, y, x, y, r, 1.0f, col };
    return npt++;
}
static void add_stick(int a, int b) {
    if (nstk >= MAXS) return;
    float L = len2(pts[b].x - pts[a].x, pts[b].y - pts[a].y);
    stks[nstk++] = (Stk){ a, b, L, L };
}

static void integrate(Pt *p) {
    float vx = (p->x - p->px) * DAMP;
    float vy = (p->y - p->py) * DAMP;
    p->px = p->x; p->py = p->y;
    p->x += vx;   p->y += vy + (GRAV * SLOMO * SLOMO) / SUBSTEPS;
}
static void relax_stick(Stk *s) {
    Pt *a = &pts[s->a], *b = &pts[s->b];
    float dx = b->x - a->x, dy = b->y - a->y;
    float d = len2(dx, dy);
    if (d < 0.0001f) return;
    float diff = (d - s->len) / d * 0.5f;
    a->x += dx * diff; a->y += dy * diff;
    b->x -= dx * diff; b->y -= dy * diff;
}
static void collide(Pt *a, Pt *b) {
    float dx = b->x - a->x, dy = b->y - a->y;
    float d = len2(dx, dy), mn = a->r + b->r;
    if (d >= mn || d < 0.0001f) return;
    float diff = (mn - d) / d * 0.5f;
    a->x -= dx * diff; a->y -= dy * diff;
    b->x += dx * diff; b->y += dy * diff;
}
static void clamp_bounds(Pt *p) {
    float r = p->r, v;
    if (p->x < r)            { v = p->x - p->px; p->x = r;            p->px = p->x + v * REST; }
    if (p->x > SCREEN_W - r) { v = p->x - p->px; p->x = SCREEN_W - r; p->px = p->x + v * REST; }
    if (p->y < r)            { v = p->y - p->py; p->y = r;            p->py = p->y + v * REST; }
    if (p->y > SCREEN_H - r) { v = p->y - p->py; p->y = SCREEN_H - r; p->py = p->y + v * REST; }
}
static void physics_step(void) {
    for (int sub = 0; sub < SUBSTEPS; sub++) {
        for (int i = 0; i < npt; i++) integrate(&pts[i]);
        for (int it = 0; it < ITERS; it++) {
            for (int s = 0; s < nstk; s++) relax_stick(&stks[s]);
            for (int i = 0; i < npt; i++)
                for (int j = i + 1; j < npt; j++) collide(&pts[i], &pts[j]);
            for (int i = 0; i < npt; i++) clamp_bounds(&pts[i]);
        }
    }
}

// a soft-body blob: hub + a ring of rim points. spokes keep it round, rim links
// hold it together. records itself in blob[] so we can fill + wobble it later.
static void make_blob(float cx, float cy, float rad, int n, int col, int eyecol, bool chr) {
    int hub = add_pt(cx, cy, 2.5f, col);
    int rim0 = npt, first = -1, prev = -1;
    for (int i = 0; i < n; i++) {
        float a = (float)i / (float)n * 360.0f;
        int p = add_pt(cx + cos_deg(a) * rad, cy + sin_deg(a) * rad, 3.5f, col);
        add_stick(hub, p);
        if (prev >= 0) add_stick(prev, p); else first = p;
        prev = p;
    }
    add_stick(prev, first);
    blob[nblob++] = (Blob){ hub, rim0, n, col, eyecol, chr };
}

static void build_scene(void) {
    npt = 0; nstk = 0; nblob = 0;
    // the character — a big friendly blob
    make_blob(SCREEN_W / 2, SCREEN_H / 2, 18, 14, CLR_PINK, CLR_DARK_PURPLE, true);
    // ambient companions that pulse with the music
    make_blob(60,  70,  12, 11, CLR_BLUE,       CLR_DARK_BLUE, false);
    make_blob(255, 60,  10, 10, CLR_LIME_GREEN, CLR_DARK_GREEN, false);
    make_blob(70,  150, 11, 11, CLR_ORANGE,     CLR_BROWN, false);
    make_blob(250, 150, 13, 12, CLR_MAUVE,      CLR_DARK_PURPLE, false);
}

void init(void) {
    build_scene();
    songpos = 0;
}

// ── the loop: a slow evolving pad + a pentatonic bell sparkle (Frippertronics-ish,
//    smeared by the echo bus). Warble in the advance rate fakes wow/flutter now. ──
static const int CHORD_A[3] = { 48, 55, 60 };   // Cm-ish low pad
static const int CHORD_B[3] = { 51, 58, 63 };   // moves up a minor third
static const int PENTA[5]   = { 72, 75, 77, 79, 82 };

static void seq_step(int step) {
    laststep = step;
    if (step == 0 || step == 8) {                       // change pad chord every half-loop
        const int *ch = (step == 0) ? CHORD_A : CHORD_B;
        for (int i = 0; i < 3; i++) {
            if (padh[i]) note_off(padh[i]);
            padh[i] = note_on(ch[i], INSTR_PD, 3);
            note_echo(padh[i], 0.35f);
        }
    }
    // sparse bell sparkle
    static const int hit_on[6] = { 0, 3, 6, 9, 11, 14 };
    for (int i = 0; i < 6; i++)
        if (step == hit_on[i]) {
            int n = PENTA[(step + i) % 5];
            instrument_echo(INSTR_MALLET, 0.45f);
            int vol = 3 + (int)(sat * 2.0f);             // saturation drives a touch louder
            hit(n, INSTR_MALLET, vol, 260);
            if (sat > 0.5f) hit(n - 12, INSTR_SAW, 1, 120);  // faint grit as it saturates
        }
}

void update(void) {
    // ── tape wear: creeps up on its own; SPACE rewinds & cleans ──────────────
    wear += 0.0016f;                  // ~10 s to fully degrade
    if (wear > 1) wear = 1;
    if (keyp(KEY_SPACE) || btnp(0, BTN_B)) { wear = 0; rewind = 8; }
    if (rewind > 0) rewind--;

    // LFOs + the three tape values
    wowph   += 1.4f  * SLOMO;          // slow sway
    flutph  += 23.0f * SLOMO;          // fast tremble
    sat     = wear;
    wow     = clamp(wear * 1.2f, 0, 1);
    flutter = wear * wear;            // the real falling-apart comes late

    // ── music clock: advance through the loop, dragged + jittered by the tape ──
    float advance = (1.0f / 50.0f);                      // ~72 BPM at 60 fps
    advance *= (1.0f - wow * 0.28f);                     // wow drags the transport slower
    advance *= (1.0f + (noise(songpos * 9.0f) - 0.5f) * flutter * 1.6f);  // flutter stutters it
    songpos += advance;
    int beat = (int)songpos;
    int step = beat % 16;
    if (beat != lastbeat) {
        lastbeat = beat;
        beatpulse = 0;
        seq_step(step);
        // every beat: a soft sub thump + a radial PULSE so the blobs throb in time
        hit(36, INSTR_SINE, 2 + (int)(sat * 2), 90);
        for (int b = 0; b < nblob; b++) {
            Blob *bl = &blob[b];
            for (int i = 0; i < bl->n; i++) {
                Pt *hub = &pts[bl->hub], *p = &pts[bl->rim0 + i];
                float dx = p->x - hub->x, dy = p->y - hub->y, d = len2(dx, dy);
                if (d > 0.01f) { p->x += dx / d * 2.2f * SLOMO; p->y += dy / d * 2.2f * SLOMO; }
            }
        }
    }
    beatpulse++;

    // ── breathing + flutter tremble: WOW slowly swells every blob's rest size,
    //    FLUTTER kicks the rim with fast random jitter. This is the wobble. ──
    float breath = 1.0f + sin_deg(wowph) * 0.10f * wow;
    for (int s = 0; s < nstk; s++) stks[s].len = stks[s].len0 * breath;
    if (flutter > 0.01f)
        for (int b = 0; b < nblob; b++) {
            Blob *bl = &blob[b];
            for (int i = 0; i < bl->n; i++) {
                Pt *p = &pts[bl->rim0 + i];
                p->x += rnd_float_between(-1, 1) * flutter * 2.4f * SLOMO;
                p->y += rnd_float_between(-1, 1) * flutter * 2.4f * SLOMO;
            }
        }

    // ── control the character blob: nudge all its points (impulse = move x w/o px) ──
    Blob *c = &blob[0];
    float ax = ((btn(0, BTN_RIGHT) ? 1 : 0) - (btn(0, BTN_LEFT) ? 1 : 0)) * 0.45f * SLOMO;
    float ay = ((btn(0, BTN_DOWN)  ? 1 : 0) - (btn(0, BTN_UP)   ? 1 : 0)) * 0.30f * SLOMO;
    bool hop = btnp(0, BTN_A);
    for (int i = 0; i <= c->n; i++) {
        Pt *p = &pts[c->hub + i];                        // hub + its rim are contiguous
        p->x += ax;
        if (hop) p->y -= 3.2f * SLOMO; else p->y += ay;
    }

    // ── foley: a soft bomp when the character squishes hard against a wall ──
    Pt *hub = &pts[c->hub];
    float spd = len2(hub->x - hub->px, hub->y - hub->py);
    static float lastspd;
    if (spd < lastspd - 1.4f * SLOMO && lastspd > 2.0f * SLOMO)   // sudden deceleration = an impact
        hit(rnd_between(40, 46), INSTR_MEMBRANE, 2, 70);
    lastspd = spd;

#if TAPE
    tape(wow, flutter, sat);                             // ← the real master insert, when it lands
#endif
    // echo gives the Frippertronics tape-loop smear; it opens up as the tape wears
    echo(300 + (int)(wow * 240), 0.30f + wear * 0.30f, 0.5f - wear * 0.3f);

    physics_step();
}

// fill a blob as a triangle-fan from hub to rim, with rounded rim bumps
static void draw_blob(Blob *bl) {
    Pt *hub = &pts[bl->hub];

    // CHROMA FRINGE — faint red/blue rim ghosts shifted sideways, drawn BEHIND the
    // real fill so only the silhouette edges peek out: chromatic aberration that
    // grows with flutter. true-colour (rectfill_rgb), so it stays pure under sepia.
    int caf = (int)(flutter * 5.0f);
    if (caf > 0)
        for (int i = 0; i < bl->n; i++) {
            int x = (int)pts[bl->rim0 + i].x, y = (int)pts[bl->rim0 + i].y;
            rectfill_rgb(x - 4 + caf, y - 4, 8, 8, 0xff0030);   // red, shifted right
            rectfill_rgb(x - 4 - caf, y - 4, 8, 8, 0x3048ff);   // blue, shifted left
        }

    for (int i = 0; i < bl->n; i++) {
        Pt *a = &pts[bl->rim0 + i];
        Pt *b = &pts[bl->rim0 + (i + 1) % bl->n];
        trifill((int)hub->x, (int)hub->y, (int)a->x, (int)a->y, (int)b->x, (int)b->y, bl->col);
    }
    for (int i = 0; i < bl->n; i++)
        circfill((int)pts[bl->rim0 + i].x, (int)pts[bl->rim0 + i].y, 4, bl->col);
    // eyes (offset toward travel direction) — only the character gets a face
    if (bl->chr) {
        float dx = hub->x - hub->px, dy = hub->y - hub->py;
        int ex = (int)hub->x + (int)clamp(dx * 1.5f, -4, 4);
        int ey = (int)hub->y + (int)clamp(dy * 1.5f, -3, 3);
        circfill(ex - 5, ey - 2, 3, CLR_WHITE);
        circfill(ex + 5, ey - 2, 3, CLR_WHITE);
        circfill(ex - 5 + (int)clamp(dx, -1, 1), ey - 2, 1, bl->eyecol);
        circfill(ex + 5 + (int)clamp(dx, -1, 1), ey - 2, 1, bl->eyecol);
    }
}

// the 32 base palette colours (pico-8) — so we can grade toward sepia AND back.
static const int PAL0[32] = {
    0x000000, 0x1d2b53, 0x7e2553, 0x008751, 0xab5236, 0x5f574f, 0xc2c3c7, 0xfff1e8,
    0xff004d, 0xffa300, 0xffec27, 0x00e436, 0x29adff, 0x83769c, 0xff77a8, 0xffccaa,
    0x291814, 0x111d35, 0x422136, 0x125359, 0x742f29, 0x49333b, 0xa28879, 0xf3ef7d,
    0xbe1250, 0xff6c24, 0xa8e72e, 0x00b543, 0x065ab5, 0x754665, 0xff6e59, 0xff9d81 };

static int mix8(int a, int b, float t) { return (int)(a + (b - a) * t); }

// SEPIA — grade the WHOLE palette from its true colours toward an aged sepia by
// `amt` (0 = pristine, 1 = full sepia): luminance mapped onto a warm brown ramp.
// Everything drawn through the palette ages at once, and snaps back on rewind.
// (palette_hex paints raw RGB into the live palette — the true-colour primitive
// for global grading; the RGB glitch below uses pset_rgb/rectfill_rgb over it.)
static void apply_sepia(float amt) {
    for (int i = 0; i < 32; i++) {
        int c = PAL0[i];
        int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
        float lum = 0.299f * r + 0.587f * g + 0.114f * b;
        int R = mix8(r, (int)clamp(lum * 1.10f, 0, 255), amt);
        int G = mix8(g, (int)clamp(lum * 0.78f, 0, 255), amt);
        int B = mix8(b, (int)clamp(lum * 0.50f, 0, 255), amt);
        palette_hex(i, (R << 16) | (G << 8) | B);
    }
}

void draw(void) {
    // WOW sways the whole view in a slow circle; FLUTTER jitters it fast.
    int camx = (int)(sin_deg(wowph) * 4.0f * wow + rnd_float_between(-1, 1) * flutter * 3.0f * SLOMO);
    int camy = (int)(sin_deg(wowph * 0.7f + 40) * 3.0f * wow + rnd_float_between(-1, 1) * flutter * 3.0f * SLOMO);
    camera(camx, camy);

    apply_sepia(sat);                 // the whole scene ages to sepia as it wears
    cls(CLR_DARK_BLUE);
    // a soft horizon glow so the warm wash reads
    vgradient(0, 0, SCREEN_W, SCREEN_H, CLR_DARK_BLUE, CLR_DARK_PURPLE);

    for (int b = 0; b < nblob; b++) draw_blob(&blob[b]);

    camera(0, 0);

    // ── RGB GLITCH (true colour, painted OVER the sepia scene — pset_rgb /
    //    rectfill_rgb bypass the palette, so the channels stay pure) ──
    static const int SPECK[5] = { 0xff0033, 0x00ff66, 0x3399ff, 0xffffff, 0xff00ff };
    int grains = (int)(wear * 300);
    for (int i = 0; i < grains; i++)
        pset_rgb(rnd(SCREEN_W), rnd(SCREEN_H), SPECK[rnd(5)]);
    // chroma tear streaks — more, longer, the harder the tape flutters
    if (wear > 0.3f) {
        static const int TEAR[4] = { 0xff0040, 0x00ffe0, 0xff00ff, 0x40ff40 };
        int streaks = (int)(flutter * 7) + (blink(5) ? 2 : 0);
        for (int k = 0; k < streaks; k++) {
            int yy = rnd(SCREEN_H), hh = rnd_between(1, 4);
            int ww = rnd_between(24, SCREEN_W);
            rectfill_rgb(rnd(SCREEN_W) - ww / 2, yy, ww, hh, TEAR[rnd(4)]);
        }
    }
    // ── scanlines, thicker as the tape wears ──
    if (wear > 0.25f) {
        fillp(FILL_HLINES, -1);
        rectfill(0, 0, SCREEN_W, SCREEN_H, CLR_BLACK);
        fillp_reset();
    }
    // an occasional dropout band when it's really worn
    if (wear > 0.6f && blink(7))
        rectfill(0, rnd(SCREEN_H), SCREEN_W, 2, CLR_BLACK);
    if (rewind > 0) cls(CLR_WHITE);     // the rewind snap

    // ── HUD: the three tape values as meters ──
    rectfill(0, 0, SCREEN_W, 9, CLR_DARKER_BLUE);
    print("WOW & FLUTTER", 4, 2, CLR_LIGHT_PEACH);
    struct { const char *l; float v; int rgb; } m[3] = {
        { "wow", wow, 0x29adff }, { "flt", flutter, 0xff77a8 }, { "sat", sat, 0xffa300 } };
    for (int i = 0; i < 3; i++) {
        int mx = 132 + i * 54;
        print(m[i].l, mx, 2, CLR_DARK_GREY);
        rect(mx + 18, 2, 30, 5, CLR_DARK_GREY);
        rectfill_rgb(mx + 19, 3, (int)(28 * m[i].v), 3, m[i].rgb);  // true colour: stays vivid through sepia
    }
    print_right(wear > 0.85f ? "SPACE: rewind!" : "SPACE: rewind", SCREEN_W - 4, SCREEN_H - 8,
                wear > 0.85f ? CLR_YELLOW : CLR_DARK_GREY);
}
