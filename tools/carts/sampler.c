/* de:meta
{
  "slug": "sampler",
  "title": "sampler",
  "status": "active",
  "created": "2026-07-13",
  "kind": [
    "instrument",
    "probe"
  ],
  "genre": null,
  "teaches": [
    "gesture-loop"
  ],
  "lineage": "The first cart on the REAL PCM sampler (engine piece 1 + INSTR_SAMPLE + a movable chop, mic-and-sampling.md): record_arm() opens an always-on rolling capture of the console's OWN output, record_grab() freezes it into a PCM slot (peak-normalized), sample_peaks() draws the waveform, and instrument_sample_region() carves a single slice you play back chromatically on keybed.h. Ported from navkit's DAW sampler (rolling-capture buffer + sample-playback voice). Replaces the grainchop granular spike.",
  "description": {
    "summary": "A real sampler with a clean record flow. Press REC and play the keys — you watch the live waveform go in. Press STOP and the recording appears as a waveform with two draggable handles: slide them to carve a single CHOP. Then the keys play just that slice back, pitched. The source is the console's own sound, never a mic.",
    "detail": "Proves the engine feature end to end with a legible UI. PLAY: the keybed drives a synth (cycle with ENGINE) whose output feeds an always-on capture ring (record_arm); the top panel is a live oscilloscope of what's being recorded. REC starts a timed take; STOP calls record_grab() (peak-normalized so playback is as loud as the source) and switches to CHOP. CHOP: the captured buffer is drawn via sample_peaks(); drag the START and END handles to isolate a slice (instrument_sample_region, live), and the keys play that slice — C4 at original speed, higher notes faster/up. RE-REC goes back. One-shot forward playback for now; loop/reverse come next.",
    "controls": "REC / STOP button (or R) — record a take of what you play. Drag the two handles on the waveform to set the chop (desktop: mouse). Keybed A-K etc plays the synth while recording, the chop after. [ ] or ENGINE cycles the source synth. RE-REC records again."
  }
}
de:meta */
#include "studio.h"
#include "keybed.h"
#include "ui.h"
#include <stdio.h>

// ── SAMPLER ──────────────────────────────────────────────────────────────────
// The real PCM sampler (mic-and-sampling.md). REC a take of what you play → the
// recording appears as a waveform → drag two handles to carve a CHOP → the keys
// play that slice back, pitched. No mic — you sample the console's own output.

#define SYN   5          // source synth slot (feeds the capture ring)
#define SMP   6          // sampler slot (plays the grabbed buffer)
#define SND   0          // PCM sample slot
#define ROOT  60         // C4 plays the chop at original speed
#define MAXREC 7.5f      // cap a take under the ring's 8s

#define PX 6
#define PY 28
#define PW (SCREEN_W - 12)
#define PH 50

enum { ST_PLAY, ST_REC, ST_CHOP };

typedef struct { int instr; const char *name; int a, d, s, r; } Voice;
static const Voice SRC[] = {
    { INSTR_SAW,    "SAW",   8, 140, 6, 220 },
    { INSTR_FM,     "FM",    4, 200, 5, 300 },
    { INSTR_PLUCK,  "PLUCK", 1,   0, 7, 700 },   // self-decaying: amp SUSTAINS (7), the string ring dies on its own
    { INSTR_EPIANO, "EPIANO",1,   0, 7, 450 },   // same — s=0 would gate it silent instantly
};
#define NSRC ((int)(sizeof SRC / sizeof *SRC))

static int   eng   = 0;
static int   state = ST_PLAY;
static float rec_t0 = 0.0f;      // now() when REC started
static float rec_secs = 0.0f;    // length of the grabbed take
static int   rec_len = 0;        // samples grabbed
static float chop_a = 0.0f, chop_b = 1.0f;   // chop handles (fractions 0..1)
static int   grab_h = 0;         // dragging: 0 none, 1 start, 2 end
static float wf_lo[PW], wf_hi[PW];   // captured waveform peaks
static float sc[PW];                 // live scope buffer

static void apply_src(void) { const Voice *v = &SRC[eng]; instrument(SYN, v->instr, v->a, v->d, v->s, v->r); }
static void point_keybed(int slot) { keybed_config(slot, 4, 14); keybed_layout(0, 100, SCREEN_W, SCREEN_H - 100); }

void init(void) {
    apply_src();
    instrument(SMP, INSTR_SAMPLE, 1, 0, 7, 150);   // full-level while held; plays the PCM chop
    record_arm();                                  // start capturing the console's own output
    point_keybed(SYN);
}

static void stop_take(void) {
    rec_secs = now() - rec_t0;
    if (rec_secs > MAXREC) rec_secs = MAXREC;
    if (rec_secs < 0.05f)  rec_secs = 0.05f;
    rec_len = record_grab(SND, rec_secs);
    if (rec_len > 0) {
        chop_a = 0.0f; chop_b = 1.0f;
        instrument_sample(SMP, SND, ROOT);
        instrument_sample_region(SMP, chop_a, chop_b);
        sample_peaks(SND, wf_lo, wf_hi, PW);
        state = ST_CHOP;
        point_keybed(SMP);
    } else {
        state = ST_PLAY;                            // nothing captured — back to play
    }
}

static void drag_handles(void) {
    int mx = mouse_x(), my = mouse_y();
    int ax = PX + (int)(chop_a * PW), bx = PX + (int)(chop_b * PW);
    if (mouse_pressed(0) && my >= PY - 4 && my <= PY + PH + 4) {
        int da = mx - ax < 0 ? ax - mx : mx - ax;
        int db = mx - bx < 0 ? bx - mx : mx - bx;
        if (da <= 7 || db <= 7) grab_h = (da <= db) ? 1 : 2;
    }
    if (!mouse_down(0)) grab_h = 0;
    if (grab_h) {
        float f = (mx - PX) / (float)PW;
        if (f < 0) f = 0; if (f > 1) f = 1;
        if (grab_h == 1) chop_a = (f < chop_b - 0.01f) ? f : chop_b - 0.01f;
        else             chop_b = (f > chop_a + 0.01f) ? f : chop_a + 0.01f;
        if (chop_a < 0) chop_a = 0;
        instrument_sample_region(SMP, chop_a, chop_b);   // ride the chop live
    }
}

void update(void) {
    keybed_update();
    if (keyp('[') || keyp(']')) { eng = (eng + (keyp(']') ? 1 : NSRC - 1)) % NSRC; apply_src(); }

    if (state == ST_PLAY) {
        if (keyp('R')) { state = ST_REC; rec_t0 = now(); }
    } else if (state == ST_REC) {
        if (keyp('R') || now() - rec_t0 >= MAXREC) stop_take();
    } else { // ST_CHOP
        if (keyp('R')) { state = ST_PLAY; point_keybed(SYN); }
        drag_handles();
    }
}

static void draw_panel(void) {
    rectfill(PX, PY, PW, PH, CLR_DARKER_GREY);
    int mid = PY + PH / 2;
    line(PX, mid, PX + PW - 1, mid, CLR_DARK_GREY);

    if (state == ST_CHOP && rec_len > 0) {
        int ax = PX + (int)(chop_a * PW), bx = PX + (int)(chop_b * PW);
        for (int x = 0; x < PW; x++) {
            int sx = PX + x;
            int inchop = (sx >= ax && sx <= bx);
            int y0 = mid - (int)(wf_hi[x] * (PH / 2 - 1));
            int y1 = mid - (int)(wf_lo[x] * (PH / 2 - 1));
            line(sx, y0, sx, y1, inchop ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);
        }
        // handles
        line(ax, PY - 3, ax, PY + PH + 3, CLR_YELLOW);  rectfill(ax - 3, PY - 4, 7, 5, CLR_YELLOW);
        line(bx, PY - 3, bx, PY + PH + 3, CLR_ORANGE);  rectfill(bx - 3, PY - 4, 7, 5, CLR_ORANGE);
    } else if (state == ST_REC) {
        // the take FILLS IN left→right as a waveform (fixed tape scale: full panel = MAXREC),
        // so it reads exactly like the chop view — not a scrolling scope.
        float elapsed = now() - rec_t0;
        if (elapsed > MAXREC) elapsed = MAXREC;
        int cols = (int)(elapsed / MAXREC * PW);
        if (cols > PW) cols = PW; if (cols < 1) cols = 1;
        if (record_peaks(elapsed, wf_lo, wf_hi, cols) > 0) {
            float peak = 0.05f;                          // display-normalize so it fills vertically like the chop
            for (int x = 0; x < cols; x++) {
                float a = wf_hi[x] > 0 ? wf_hi[x] : -wf_hi[x], b = wf_lo[x] > 0 ? wf_lo[x] : -wf_lo[x];
                if (a > peak) peak = a; if (b > peak) peak = b;
            }
            float g = 0.9f / peak;
            for (int x = 0; x < cols; x++) {
                int y0 = mid - (int)(wf_hi[x] * g * (PH / 2 - 1));
                int y1 = mid - (int)(wf_lo[x] * g * (PH / 2 - 1));
                line(PX + x, y0, PX + x, y1, CLR_RED);
            }
            int hx = PX + cols;                          // write head
            line(hx, PY, hx, PY + PH - 1, CLR_LIGHT_GREY);
        }
    } else {
        // idle monitor: a live oscilloscope of the synth you're noodling before REC
        scope_read(sc, PW);
        int py = mid - (int)(sc[0] * (PH / 2 - 1));
        for (int x = 1; x < PW; x++) {
            int y = mid - (int)(sc[x] * (PH / 2 - 1));
            line(PX + x - 1, py, PX + x, y, CLR_BLUE_GREEN);
            py = y;
        }
    }
    rect(PX, PY, PW, PH, CLR_MEDIUM_GREY);
}

void draw(void) {
    cls(CLR_BLACK);
    char buf[96];

    print("SAMPLER", 4, 2, CLR_WHITE);
    const char *sn = state == ST_PLAY ? "PLAY" : state == ST_REC ? "REC" : "CHOP";
    print(sn, SCREEN_W - text_width(sn) - 4, 2,
          state == ST_REC ? CLR_RED : state == ST_CHOP ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);

    if (state == ST_PLAY)      snprintf(buf, sizeof buf, "press REC, then play the keys");
    else if (state == ST_REC)  snprintf(buf, sizeof buf, "recording... %.1fs   (STOP to keep)", now() - rec_t0);
    else                       snprintf(buf, sizeof buf, "chop %.2f-%.2fs  drag handles  keys play it",
                                        chop_a * rec_secs, chop_b * rec_secs);
    print(buf, 4, 14, state == ST_REC ? CLR_RED : CLR_LIGHT_GREY);

    draw_panel();

    ui_begin();
    int by = PY + PH + 6;
    if (state == ST_CHOP) { if (ui_button(4, by, 96, 20, "RE-REC")) { state = ST_PLAY; point_keybed(SYN); } }
    else                  { if (ui_button(4, by, 96, 20, state == ST_REC ? "STOP" : "REC")) {
                              if (state == ST_PLAY) { state = ST_REC; rec_t0 = now(); } else stop_take();
                          } }
    if (ui_button(SCREEN_W - 74, by, 70, 20, SRC[eng].name)) { eng = (eng + 1) % NSRC; apply_src(); }
    ui_end();

    print(state == ST_CHOP ? "keys play the CHOP  (C4=original)  R re-rec"
                           : "keys play + record the SYNTH   R rec/stop",
          4, by + 24, state == ST_CHOP ? CLR_LIME_GREEN : CLR_DARK_GREY);

    keybed_draw();
}
