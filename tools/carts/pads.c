/* de:meta
{
  "slug": "pads",
  "title": "pads",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [],
  "description": {
    "summary": "The velocity-pad family from the control vocabulary, across three skins: a grid of candy velocity pads that flash-and-fade when you hit them, and a row of drum pads wired to a running sequencer — each carrying a corner TRIG LED that lights only when fired EXTERNALLY (by the clock), so you can see who hit it: you or the machine.",
    "detail": "The fifth control-vocabulary study cart. Row 1 VELOCITY PADS: an MPC-style 4x2 grid of candy pads; tap one and it flashes bright then fades (the velocity envelope). Row 2 TRIG PADS: KICK/SNARE/HAT/CLAP wired to a 16-step sequencer running at the tempo — each pad has a small corner LED that flashes ONLY on an EXTERNAL trigger (the clock firing its pattern, with accents = brighter), distinct from a manual tap which just flashes the face. That corner indicator is the point: on a device face you must be able to tell a machine-fired hit from a finger. Row 3 is the CLOCK: the 16-step playhead driving row 2. All three skins: TACTILE (candy bevel + velocity glow), FLAT (flat + brighten), PURE (hairline + filled-on, C64 register). Cycle with the SKIN button or B.",
    "controls": "Tap any pad to hit it (flash scales the fade). Row-2 pads also fire on their own from the sequencer — watch the corner TRIG LED: lit = the clock fired it, dark = only you can. SKIN button (top-right) or B cycles TACTILE / FLAT / PURE."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"

// PADS — the velocity-pad family (control-vocabulary.md §3 ⊙ pads). Candy pads
// that flash + fade on a hit, plus drum pads with a corner TRIG LED that marks
// an EXTERNAL (sequencer) hit vs a finger tap. Three skins, per §10 (ui_skin).

// SKIN axis
enum { SK_TACTILE, SK_FLAT, SK_PURE };
static int skin = SK_TACTILE;
static const char *SKIN_NAME[3] = { "SKIN:TACTILE", "SKIN:FLAT", "SKIN:PURE" };
#define bevel (skin == SK_TACTILE)

static void plabel(const char *s, int cx, int y, int col) { print(s, cx - text_width(s) / 2, y, col); }

static void faceplate(int x, int y, int w, int h) {
    if (skin == SK_PURE) { rect(x, y, w, h, CLR_MEDIUM_GREY); return; }
    rrectfill(x, y, w, h, 4, CLR_DARK_BROWN);
    if (skin == SK_TACTILE) {
        line(x + 3, y + 1, x + w - 4, y + 1, CLR_BROWN);
        blend(BLEND_AVG); line(x + 3, y + h - 1, x + w - 4, y + h - 1, CLR_BLACK); blend_reset();
    }
}

static int btn_input(unsigned seed, int x, int y, int w, int h, int *pr, int *hot) {
    void *wid = ui_wid_hash(seed, x, y, w, h);
    int foc = 0;
    return ui_button_core(wid, x, y, w, h, &foc, pr, hot);
}

// a velocity pad. `env` 0..1 = the current flash (velocity, decaying); `face` = the
// candy colour; `trig` 0..1 = the EXTERNAL-trigger corner LED (0 = never externally fired,
// so no lit LED). pressed = a finger is on it right now.
static void draw_pad(int x, int y, int w, int h, int face, float env, float trig,
                     const char *label, int pressed, int hot) {
    if (skin == SK_PURE) {                                   // native: coloured hairline, fills on hit
        int lit = env > 0.1f;
        rectfill(x, y, w, h, lit ? face : CLR_BROWNISH_BLACK);
        rect(x, y, w, h, hot ? CLR_WHITE : (lit ? CLR_BROWNISH_BLACK : face));   // colour = identity when idle
        if (label) plabel(label, x + w / 2, y + h - 8, lit ? CLR_BROWNISH_BLACK : face);
        if (trig >= 0) {                                     // external-trigger marker (trig<0 = none)
            if (trig > 0.02f) rectfill(x + w - 5, y + 2, 3, 3, CLR_LIGHT_YELLOW);
            else              rect(x + w - 5, y + 2, 3, 3, CLR_DARKER_GREY);
        }
        return;
    }
    // velocity glow halo (grows with env) — TACTILE only
    if (bevel && env > 0.05f) {
        int e = (int)(env * 3);
        blend(BLEND_AVG); rrectfill(x - e, y - e, w + 2 * e, h + 2 * e, 4, face); blend_reset();
    }
    int dy = pressed ? 1 : 0;
    rrectfill(x, y + dy, w, h, 3, face);                     // candy face
    if (env > 0.4f) { blend(BLEND_AVG); rrectfill(x + 3, y + 3 + dy, w - 6, h - 6, 2, CLR_WHITE); blend_reset(); }  // hot pop
    if (bevel) {                                             // top-left light on the rounded cap
        line(x + 3, y + 1 + dy, x + w - 4, y + 1 + dy, pressed ? CLR_DARK_BROWN : CLR_WHITE);
        line(x + 1, y + 3 + dy, x + 1, y + h - 4 + dy, pressed ? CLR_DARK_BROWN : CLR_LIGHT_PEACH);
        blend(BLEND_AVG);
        line(x + 3, y + h - 1 + dy, x + w - 4, y + h - 1 + dy, CLR_BLACK);
        line(x + w - 2, y + 3 + dy, x + w - 2, y + h - 4 + dy, CLR_BLACK);
        blend_reset();
    }
    rrect(x, y + dy, w, h, 3, hot ? CLR_WHITE : CLR_BLACK);
    if (label) plabel(label, x + w / 2, y + h - 8 + dy, CLR_BROWNISH_BLACK);
    // the EXTERNAL-trigger corner LED (top-right): dark socket, flashes amber when the clock
    // fires it — trig < 0 means this pad has no external source, so no indicator at all.
    if (trig >= 0) {
        int lx = x + w - 6, ly = y + 3;
        circfill(lx, ly, 2, trig > 0.02f ? CLR_LIGHT_YELLOW : CLR_BROWNISH_BLACK);
        if (trig > 0.5f) pset(lx - 1, ly - 1, CLR_WHITE);
        circ(lx, ly, 2, CLR_BLACK);
    }
}

// ── state ────────────────────────────────────────────────────────────────
#define NVEL 8
static float venv[NVEL];                       // velocity-pad flash envelopes
static int   vcol[NVEL] = { CLR_PINK, CLR_ORANGE, CLR_YELLOW, CLR_GREEN,
                            CLR_TRUE_BLUE, CLR_MAUVE, CLR_RED, CLR_LIME_GREEN };
#define NTRIG 4
static float tenv[NTRIG], tled[NTRIG];         // trig-pad face flash + corner-LED (external only)
static int   tcol[NTRIG] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_TRUE_BLUE };
static const char *tname[NTRIG] = { "KICK", "SNARE", "HAT", "CLAP" };
// a 16-step pattern per trig pad (0 = rest, 1 = hit, 2 = accent)
static int pat[NTRIG][16] = {
    { 2,0,0,0, 1,0,0,0, 2,0,0,0, 1,0,1,0 },    // kick
    { 0,0,0,0, 2,0,0,0, 0,0,0,0, 2,0,0,1 },    // snare
    { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,2,0 },    // hat
    { 0,0,0,0, 0,0,0,0, 0,0,2,0, 0,0,0,0 },    // clap
};
static int seqstep = 0, lastseq = -1;

void init(void) { bpm(120); }

void update(void) {
    // decay all envelopes
    for (int i = 0; i < NVEL; i++)  { venv[i] -= 0.06f; if (venv[i] < 0) venv[i] = 0; }
    for (int i = 0; i < NTRIG; i++) { tenv[i] -= 0.06f; if (tenv[i] < 0) tenv[i] = 0;
                                      tled[i] -= 0.08f; if (tled[i] < 0) tled[i] = 0; }
    // the CLOCK: advance the 16-step playhead and fire external triggers
    seqstep = ((int)(now() * 8)) % 16;                       // 8 steps/sec
    if (seqstep != lastseq) {
        lastseq = seqstep;
        for (int p = 0; p < NTRIG; p++) {
            int v = pat[p][seqstep];
            if (v) { float vel = v == 2 ? 1.0f : 0.6f; tenv[p] = vel; tled[p] = 1.0f; }  // EXTERNAL fire
        }
    }
#ifdef DE_TRACE
    watch("step", "%d", seqstep); watch("kick_led", "%.2f", tled[0]);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();

    print("PADS", 6, 3, CLR_WHITE);
    font(FONT_SMALL);
    if (ui_button(SCREEN_W - 74, 3, 70, 11, SKIN_NAME[skin]) || keyp('B')) skin = (skin + 1) % 3;

    // ── ROW 1 — velocity pads (manual): tap to flash + fade ────────────────
    print("1 VELOCITY PADS  tap to hit - flash fades (the velocity envelope)", 6, 18, CLR_DARK_GREY);
    faceplate(4, 26, 312, 66);
    {
        int pw = 72, ph = 26, x0 = 12, gap = 4;
        for (int i = 0; i < NVEL; i++) {
            int col = i % 4, row = i / 4;
            int x = x0 + col * (pw + gap), y = 36 + row * (ph + 4);
            int pr = 0, hot = 0;
            if (btn_input(0xA0u + i, x, y, pw, ph, &pr, &hot)) venv[i] = 1.0f;   // manual full-velocity
            float e = pr ? 1.0f : venv[i];
            draw_pad(x, y, pw, ph, vcol[i], e, -1, 0, pr, hot);                  // trig=-1: no ext LED here
        }
    }

    // ── ROW 2 — drum pads with the EXTERNAL-trigger corner LED ─────────────
    print("2 TRIG PADS  corner LED lit = fired by the CLOCK, not your finger", 6, 100, CLR_DARK_GREY);
    faceplate(4, 108, 312, 62);
    {
        int pw = 72, ph = 42, x0 = 12, gap = 4;
        for (int i = 0; i < NTRIG; i++) {
            int x = x0 + i * (pw + gap), y = 118;
            int pr = 0, hot = 0;
            if (btn_input(0xB0u + i, x, y, pw, ph, &pr, &hot)) tenv[i] = 1.0f;   // manual: face only, NO led
            float e = pr ? 1.0f : tenv[i];
            draw_pad(x, y, pw, ph, tcol[i], e, tled[i], tname[i], pr, hot);
        }
    }

    // ── ROW 3 — the CLOCK: the 16-step playhead driving row 2 ──────────────
    print("3 CLOCK  the 16-step sequencer (the external source)", 6, 178, CLR_DARK_GREY);
    faceplate(4, 186, 312, 34);
    {
        int n = 16, x0 = 12, cw = 18, y = 196, ch = 14;
        for (int s = 0; s < n; s++) {
            int x = x0 + s * cw;
            int here = s == seqstep;
            int anyhit = pat[0][s] || pat[1][s] || pat[2][s] || pat[3][s];
            int face = here ? CLR_LIGHT_YELLOW : anyhit ? CLR_DARK_ORANGE : CLR_DARKER_GREY;
            if (skin == SK_PURE) {
                rectfill(x, y, cw - 2, ch, here ? CLR_LIGHT_YELLOW : CLR_BROWNISH_BLACK);
                rect(x, y, cw - 2, ch, anyhit ? CLR_ORANGE : CLR_MEDIUM_GREY);
            } else {
                rrectfill(x, y, cw - 2, ch, 2, face);
                if (bevel && here) { blend(BLEND_AVG); rrectfill(x, y, cw - 2, 2, 1, CLR_WHITE); blend_reset(); }
                rrect(x, y, cw - 2, ch, 2, CLR_BLACK);
            }
        }
    }

    font(FONT_NORMAL);
    ui_end();
}
