#include "studio.h"
#include <stdio.h>   // snprintf for the live readouts

// ============================================================================
// DRIFT — three drummers, one pattern, ranked by clock.
//
// The same steady 16th-note pulse is played THREE ways at once, each on a worse
// clock. When the clocks agree the three collapse into one tight, centered hit;
// as timing slips you hear them flam apart and spread across the stereo field.
// A scrolling scope per voice plots each hit's error (ms) from the ideal grid.
//
//   TRUTH  (center, high click) — schedule_hit(): the note is queued a few
//          steps ahead with a sample-accurate delay, so the AUDIO THREAD places
//          it exactly on the grid. Frame jitter can't touch it. This is the
//          reference line — the radio.h "schedule-ahead" pattern.
//
//   TRIG   (left, hihat) — hit() fired the instant the 16th-note index changes
//          in update(). The onset is therefore whenever THIS FRAME happened to
//          run, so it wobbles by up to a frame (~16ms, more on web's jittery
//          requestAnimationFrame clock). The classic every()/beat path. Drunk.
//
//   FCNT   (right, cowbell) — hit() every N frames, where N was picked assuming
//          exactly 60fps. It references no real clock, so it CAN'T stay in time:
//          it's already a few ms off even at a steady 60fps, and on web (where
//          fps varies) it slides continuously — true accumulating drift.
//
// What to listen/look for: solo TRUTH = a metronome. Add TRIG = a faint flam on
// fast tempos. Add FCNT = a cowbell that walks off the beat. Tap GLITCH to stall
// a frame (a GC pause / scroll on web): TRUTH holds, the frame clocks stumble.
//
//   TAP the bottom buttons (mute each voice / tempo / glitch) — works on a phone
//   1 2 3   mute TRUTH / TRIG / FCNT          ← →   tempo down / up
//   SPACE   inject a frame hitch (GLITCH)
//
// Companion to docs/design/audio-timing.md. This cart is also the A/B rig for
// the web frame_dt clamp: build with/without it and tap GLITCH.
// ============================================================================

#define SL_TRUTH 5
#define SL_TRIG  6
#define SL_FC    7

#define HN        96       // scope history length per voice
#define LOOKAHEAD 3        // 16th-steps the schedule-ahead clock works in front
#define MS_SPAN   30.0f    // scope shows +/- this many ms full-scale

static int  tempo  = 138;
static bool mute[3] = { false, false, false };   // 0 TRUTH · 1 TRIG · 2 FCNT

// scope ring buffers: signed error in ms (+late / -early) at each hit
static float hist[3][HN];
static int   hh[3];
static float curErr[3];      // latest error, for the numeric readout
static float peak[3];        // decaying peak |error|
static int   flash[3] = { -99, -99, -99 };   // frame() of last hit (lane flash)

static long  truthSched = -1;   // last 16th index the schedule-ahead clock booked
static long  trigLast   = -1;   // last 16th index the frame-trigger fired
static int   fcLastFrame = 0;   // frame() of the frame-counter's last fire
static int   fcN         = 7;   // frames per 16th @60fps (recomputed from tempo)
static int   glitchReq   = 0;
static int   ratchetReq  = 0;

// formatting helpers (defined at the bottom)
static void print_to(char *b, int n, float ms);
static void fmt_peak(char *b, int n, float ms);
static void fmt_hdr(char *b, int n);
static void fmt_fps(char *b, int n, int f);

static void push(int v, float err) {
    hist[v][hh[v]] = err;
    hh[v] = (hh[v] + 1) % HN;
    curErr[v] = err;
    float a = err < 0 ? -err : err;
    if (a > peak[v]) peak[v] = a;
    flash[v] = frame();
}

// ── bottom transport row: geometry shared by the hit-test and the draw ──
#define BTN_Y 178
#define BTN_H 18
enum { B_M0, B_M1, B_M2, B_TDN, B_TUP, B_GLITCH, B_RATCHET, NBTN };
static const struct { int x, w, key; const char *label; } BTN[NBTN] = {
    {   4, 42, '1',       "TRUTH"  },
    {  49, 42, '2',       "TRIG"   },
    {  94, 42, '3',       "FCNT"   },
    { 139, 42, KEY_LEFT,  "BPM-"   },
    { 184, 42, KEY_RIGHT, "BPM+"   },
    { 229, 42, KEY_SPACE, "GLITCH" },
    { 274, 42, 'R',       "RATCHET"},
};

void init() {
    // three short percussive voices, panned so TRUTH anchors the center and the
    // frame clocks spread out left/right — drift becomes a widening stereo image
    instrument(SL_TRUTH, INSTR_TRI,    0, 26, 0, 8);  instrument_pan(SL_TRUTH,  0.0f);
    instrument(SL_TRIG,  INSTR_NOISE,  0, 22, 0, 8);  instrument_pan(SL_TRIG,  -0.85f);
    instrument(SL_FC,    INSTR_SQUARE, 0, 42, 0, 8);  instrument_pan(SL_FC,    +0.85f);
    for (int v = 0; v < 3; v++) { for (int j = 0; j < HN; j++) hist[v][j] = 0; hh[v] = 0; }
}

void update() {
    bpm(tempo);
    float stepMs = 60000.0f / tempo / 4.0f;            // one 16th-note, ms
    float pos16  = beat() * 4.0f + beat_pos() * 4.0f;  // continuous 16th index (engine clock)
    fcN = (int)(900.0f / tempo + 0.5f);                // frames/16th @60fps = (60/bpm/4)*60
    if (fcN < 1) fcN = 1;

    // ── GLITCH: stall this frame to forge a hitch (GC/scroll on web). The big
    //    delta lands in next frame's beat clock; the clamp in studio.c keeps it
    //    from teleporting the sequence. Iteration cap guarantees we can't hang.
    if (glitchReq) {
        glitchReq = 0;
        double t0 = now(); volatile double x = 0;
        for (long i = 0; i < 400000000L && now() - t0 < 0.18; i++) x += (double)i;
    }

    // ── RATCHET: a perfectly-even rapid burst, batch-scheduled in ONE frame. Each hit
    //    carries a sample-accurate delay, so the audio thread fires them at exact sample
    //    offsets — the INTERNAL spacing stays rock-solid even on web (only the burst's
    //    downbeat snaps to the audio buffer). This is "a perfect ratchet, like native":
    //    the truth that schedule_hit's micro-timing survives the web backend.
    if (ratchetReq) {
        ratchetReq = 0;
        for (int i = 0; i < 24; i++)
            schedule_hit((int)(i * 20.0f), 84, SL_TRUTH, 5, 16);   // 24 hits · 20ms ≈ 50Hz roll
    }

    // ── TRUTH: book every step up to LOOKAHEAD ahead, sample-accurate ──
    if (truthSched < 0) truthSched = (long)pos16;
    while (truthSched < (long)pos16 + LOOKAHEAD) {
        truthSched++;
        int dly = (int)((truthSched - pos16) * stepMs);
        if (dly < 0) dly = 0;
        if (!mute[0]) schedule_hit(dly, 84, SL_TRUTH, 4, 30);
    }

    // ── TRIG: fire the instant the 16th index ticks over (frame-quantized) ──
    long s16 = (long)pos16;
    if (trigLast < 0) trigLast = s16;
    if (s16 > trigLast) {
        trigLast = s16;
        push(0, 0.0f);                       // TRUTH lands on the grid by construction
        float err = (pos16 - (float)s16) * stepMs;   // how late this frame ran past the boundary
        if (!mute[1]) hit(72, SL_TRIG, 4, 24);
        push(1, err);
    }

    // ── FCNT: fire every N frames — assumes 60fps, so it drifts ──
    if (frame() - fcLastFrame >= fcN) {
        fcLastFrame = frame();
        float nearest = (float)((long)(pos16 + 0.5f));
        float err = (pos16 - nearest) * stepMs;       // sweeps as the frame rate misses real tempo
        if (!mute[2]) hit(60, SL_FC, 4, 40);
        push(2, err);
    }

    // ── buttons (tap or key) ──
    for (int i = 0; i < NBTN; i++) {
        if (tapp(BTN[i].x, BTN_Y, BTN[i].w, BTN_H) || keyp(BTN[i].key)) {
            switch (i) {
                case B_M0: mute[0] = !mute[0]; break;
                case B_M1: mute[1] = !mute[1]; break;
                case B_M2: mute[2] = !mute[2]; break;
                case B_TDN: tempo = mid(40, tempo - 6, 300); break;
                case B_TUP: tempo = mid(40, tempo + 6, 300); break;
                case B_GLITCH: glitchReq = 1; break;
                case B_RATCHET: ratchetReq = 1; break;
            }
        }
    }

    for (int v = 0; v < 3; v++) peak[v] *= 0.99f;

#ifdef DE_TRACE
    watch("fps",     "%d",   fps());
    watch("trig_ms", "%.1f", curErr[1]);
    watch("fc_ms",   "%.1f", curErr[2]);
#endif
}

static int err_color(float err) {
    float a = err < 0 ? -err : err;
    if (a < 4.0f)  return CLR_GREEN;
    if (a < 12.0f) return CLR_YELLOW;
    return CLR_RED;
}

// one voice lane: label + scrolling error scope + live readout
static void draw_lane(int v, int top, const char *name, const char *clock,
                      const char *pan, int litColor) {
    int laneX = 64, laneW = 168;          // scope area
    int midY  = top + 18;
    float ysc = 17.0f / MS_SPAN;
    bool lit  = (frame() - flash[v]) < 5;

    // label block
    font(FONT_SMALL);
    print(name, 4, top + 2, mute[v] ? CLR_DARK_GREY : (lit ? CLR_WHITE : litColor));
    font(FONT_TINY);
    print(clock, 4, top + 12, CLR_MEDIUM_GREY);
    print(pan,   4, top + 24, CLR_DARK_GREY);
    if (mute[v]) print("MUTE", 4, top + 33, CLR_DARK_RED);

    // scope frame + zero line
    rect(laneX - 1, top, laneW + 2, 36, CLR_DARKER_GREY);
    line(laneX, midY, laneX + laneW, midY, CLR_DARK_GREY);

    // history trace: oldest at left, newest at right
    for (int k = 0; k < HN; k++) {
        int idx = (hh[v] + k) % HN;
        float e = hist[v][idx];
        int x = laneX + k * laneW / HN;
        int y = midY - (int)(e * ysc);
        if (y < top + 1) y = top + 1;
        if (y > top + 35) y = top + 35;
        int c = err_color(e);
        line(x, midY, x, y, c);
        pset(x, y, CLR_WHITE);
    }

    // live readout on the right
    font(FONT_SMALL);
    char buf[24];
    print_to(buf, sizeof buf, curErr[v]);
    print(buf, laneX + laneW + 4, top + 4, err_color(curErr[v]));
    font(FONT_TINY);
    char pk[24];
    fmt_peak(pk, sizeof pk, peak[v]);
    print(pk, laneX + laneW + 4, top + 16, CLR_MEDIUM_GREY);
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    // header
    font(FONT_NORMAL);
    print("DRIFT", 4, 2, CLR_WHITE);
    font(FONT_SMALL);
    char hdr[40];
    fmt_hdr(hdr, sizeof hdr);
    print(hdr, 60, 4, CLR_LIGHT_GREY);
    int f = fps();
    char fb[24];
    fmt_fps(fb, sizeof fb, f);
    print(fb, 232, 4, f < 55 ? CLR_RED : CLR_GREEN);

    draw_lane(0, 24, "TRUTH", "schedule_hit", "center", CLR_GREEN);
    draw_lane(1, 75, "TRIG",  "hit on frame", "left",   CLR_YELLOW);
    draw_lane(2, 126, "FCNT", "every N frame", "right",  CLR_ORANGE);

    // transport
    font(FONT_SMALL);
    for (int i = 0; i < NBTN; i++) {
        bool on = (i <= B_M2) ? !mute[i] : false;
        int bg = (i <= B_M2) ? (on ? CLR_DARK_GREEN : CLR_DARKER_PURPLE)
                             : (i == B_GLITCH ? CLR_DARK_RED : CLR_DARKER_GREY);
        rectfill(BTN[i].x, BTN_Y, BTN[i].w, BTN_H, bg);
        rect(BTN[i].x, BTN_Y, BTN[i].w, BTN_H, CLR_DARK_GREY);
        int tw = text_width(BTN[i].label);
        print(BTN[i].label, BTN[i].x + (BTN[i].w - tw) / 2, BTN_Y + 6, CLR_WHITE);
    }
}

// ── tiny formatting helpers (no sprintf-in-hot-path style; one place each) ──
static void print_to(char *b, int n, float ms) {
    int v = (int)(ms * 10.0f);
    int sign = v < 0 ? -1 : 1; if (v < 0) v = -v;
    snprintf(b, n, "%s%d.%dms", sign < 0 ? "-" : "+", v / 10, v % 10);
}
static void fmt_peak(char *b, int n, float ms) {
    snprintf(b, n, "pk %d", (int)(ms + 0.5f));
}
static void fmt_hdr(char *b, int n) {
    snprintf(b, n, "%d bpm  16th=%dms  fcN=%d", tempo, (int)(60000.0f / tempo / 4.0f + 0.5f), fcN);
}
static void fmt_fps(char *b, int n, int f) {
    snprintf(b, n, "%dfps", f);
}
