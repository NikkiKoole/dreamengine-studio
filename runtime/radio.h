// radio.h — the shared chassis for the radio-station carts (bossa, ambient, jangle,
// jingle, lowend, citypop, dub, ymo, satie, house, …).
//
// A TOOLKIT, NOT A FRAMEWORK (game-music.md → "radio.h — the shared chassis"): the
// header exports blocks the cart calls — it never owns update()/draw() and never calls
// back into the cart. Stations opt into pieces: ambient takes the PRNG + history +
// voice leading and skips the step clock entirely; satie runs 3/4 bars through the
// same clock; every cart draws its OWN window art between the chassis calls.
//
// ⚠ THE SEED-COMPATIBILITY RULE (the only dangerous part): a pinned seed IS the song,
// and a composition is exactly the SEQUENCE of rad_srnd() calls. These functions are
// byte-for-byte the block that shipped in ten carts — do not "improve" the xorshift,
// the seed derivation, or the voice-leading tie-breaks; any change silently breaks
// every pinned seed in the library. Acceptance test per migrated cart:
//   node tools/play.js <name> run --headless --trace a.jsonl --frames 3000 --seed 7
//   (migrate, rerun to b.jsonl)  diff <(jq -c .w a.jsonl) <(jq -c .w b.jsonl)
//
// Header-only static functions: compiles into the cart's own TU — no engine surface,
// no studio_tcc_symbols.h regeneration.

#ifndef RADIO_H
#define RADIO_H

#include "studio.h"
#include "ui.h"      // cross-input widget plumbing — the draggable control knobs
                     // below reuse its per-finger capture (carts bracket draw()
                     // with ui_begin()/ui_end(); see rad_knob_int/_sel/_float)
#include <stdio.h>
#include <math.h>

// ── small shared bits ─────────────────────────────────────────────────────

static int rad_iabs(int v) { return v < 0 ? -v : v; }

static const char *RAD_PCNAME[12] =
    { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

// the tone knob (T cycles) — every station scales its filter reach by RAD_TONEMUL
static const char *RAD_TONENAME[4] = { "mellow", "warm", "clear", "bright" };
static const float RAD_TONEMUL[4]  = { 0.55f, 0.78f, 1.0f, 1.28f };

// density = arrangement curve + feel shift (game-music.md): layers key off this,
// never off intensity directly — the chorus stays fuller than the verse at every feel
static int rad_level(int sectionBase, int intensity) {
    int lvl = sectionBase + intensity - 1;
    return lvl < 0 ? 0 : lvl > 3 ? 3 : lvl;
}

// ── composition seed + session history ────────────────────────────────────
// The split that makes a song a shareable artifact: COMPOSITION randomness comes from
// this seeded xorshift32 (same seed = same tune, always); PERFORMANCE randomness
// (humanize, ghost notes) stays on the engine's rnd(). [ and ] walk the history.

typedef struct {
    unsigned rngState, seed;
    unsigned hist[64];
    int      histN, histPos;
} RadioSeed;

static unsigned rad_srnd_u(RadioSeed *r) {
    r->rngState ^= r->rngState << 13;
    r->rngState ^= r->rngState >> 17;
    r->rngState ^= r->rngState << 5;
    return r->rngState;
}
static int rad_srnd(RadioSeed *r, int n) { return (int)(rad_srnd_u(r) % (unsigned)n); }

// start a composition: seed 0 derives a fresh one (engine rnd ×2 + frame hash — the
// exact expression every station shipped with; changing it orphans noted seeds)
static unsigned rad_seed_begin(RadioSeed *r, unsigned seed) {
    if (!seed) seed = ((unsigned)rnd(0x10000) << 16) ^ (unsigned)rnd(0x10000)
                      ^ (unsigned)frame() * 2654435761u;
    if (!seed) seed = 1;
    r->rngState = r->seed = seed;
    return seed;
}

static void rad_hist_log(RadioSeed *r) {           // call after a FRESH song only
    if (r->histN == 64) {
        for (int i = 1; i < 64; i++) r->hist[i - 1] = r->hist[i];
        r->histN--;
    }
    r->hist[r->histN++] = r->seed;
    r->histPos = r->histN - 1;
}
static unsigned rad_hist_back(RadioSeed *r) {      // 0 = nothing earlier
    return r->histPos > 0 ? r->hist[--r->histPos] : 0;
}
static unsigned rad_hist_fwd(RadioSeed *r) {       // 0 = nothing later
    return r->histPos < r->histN - 1 ? r->hist[++r->histPos] : 0;
}

// ── nearest-tone voice leading ────────────────────────────────────────────
// The single biggest "sounds composed" trick, shipped UNCHANGED in every station:
// each voice moves to the nearest unused chord tone of the next chord. rootpc = the
// chord's root pitch class, iv = the quality's 3 voicing intervals (rootless: 3rd/
// 7th/color — the bass owns the root), v[n] = the voices (state, led in place),
// lo..hi = the instrument's register window.

static void rad_lead_to(int rootpc, const int *iv, int *v, int n,
                        int lo, int hi, bool *init) {
    int pcs[3];
    for (int k = 0; k < 3; k++) pcs[k] = (rootpc + iv[k]) % 12;
    if (!*init) {
        for (int k = 0; k < n; k++) {
            int target = lo + 5 + k * 5;
            int dd = ((pcs[k] - target) % 12 + 18) % 12 - 6;
            v[k] = target + dd;
        }
        *init = true;
    } else {
        bool used[3] = { false, false, false };
        for (int vi = 0; vi < n; vi++) {
            int bestJ = -1, bestC = 0, bestD = 99;
            for (int j = 0; j < 3; j++) {
                if (used[j]) continue;
                int dd = ((pcs[j] - v[vi]) % 12 + 18) % 12 - 6;
                if (rad_iabs(dd) < bestD) { bestD = rad_iabs(dd); bestJ = j; bestC = v[vi] + dd; }
            }
            used[bestJ] = true;
            v[vi] = bestC;
        }
    }
    for (int k = 0; k < n; k++) {
        while (v[k] < lo) v[k] += 12;
        while (v[k] > hi) v[k] -= 12;
    }
}

// nearest-octave voice leading for a single bass line: step `from` to the nearest
// instance of pitch-class `pc` (±6 semitones), then fold into the [lo,hi] register.
// Pure — no commit: it's the "peek". A cart wraps it with its own range and decides
// whether to commit (bassLast = rad_bass_to(...)) or just look ahead. Sibling of
// rad_lead_to, for the bass that owns the root.
static int rad_bass_to(int pc, int from, int lo, int hi) {
    int d = ((pc - from) % 12 + 18) % 12 - 6;
    int n = from + d;
    while (n < lo) n += 12;
    while (n > hi) n -= 12;
    return n;
}

// ── the schedule-ahead step clock ─────────────────────────────────────────
// Never trigger on the frame (up to 16ms of jitter = a drunk drummer): run a step
// counter against the beat clock and schedule one step ahead with schedule_hit().
//   long st; while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
// Beatless stations (ambient) skip this block entirely.

typedef struct {
    long   scheduled;   // last absolute 16th-step scheduled (init: (long)pos at boot)
    long   songBase;    // absolute step the current song starts on
    double stepMs;      // 60000 / (tempo * 4) — refresh each frame for live tempo
} RadioClock;

static bool rad_clock_step(RadioClock *c, double pos, long *step_out) {
    if (c->scheduled >= (long)pos + 1) return false;
    *step_out = ++c->scheduled;
    return true;
}
// ms until absolute step `abs` actually lands — every feel offset adds to this
static int rad_step_dly(const RadioClock *c, long abs, double pos) {
    int dly = (int)(((double)abs - pos) * c->stepMs);
    return dly < 1 ? 1 : dly;
}

// ── the input block ───────────────────────────────────────────────────────
// SPACE next · R replay · [ ] history · LEFT/RIGHT feel · UP/DOWN tempo · T tone ·
// M power · H or the ?-button help. Handles feel/tempo/tone/help itself (incl. the
// bpm() call); returns an event mask for what the CART must react to.

enum {
    RAD_EV_NEW    = 1,    // SPACE — roll a fresh song
    RAD_EV_REPLAY = 2,    // R — same seed again
    RAD_EV_BACK   = 4,    // [ — rad_hist_back
    RAD_EV_FWD    = 8,    // ] — rad_hist_fwd
    RAD_EV_POWER  = 16,   // M — radioOn just flipped (stop held voices / rescan)
    RAD_EV_TEMPO  = 32,   // UP/DOWN — tempo changed (bpm() already called)
    RAD_EV_TONE   = 64,   // T — toneSel cycled (re-aim filters if not done per frame)
};

// ── the TUNER: tap-to-tune-to-the-next-song (replaces "press SPACE") ────────
// A big tune button beside the dial. Tap it (or SPACE) and the radio "tunes":
// the needle sweeps from the current station to the new one over ~0.5s while a
// burst of static fades through, masking the song swap. Touch-native — no
// keyboard needed. radio.h owns the whole transition; rad_dial draws the button
// + animates the needle, rad_input handles the tap + advances the sweep, so the
// 7 carts that call rad_dial get it for free. The new song is rolled at the
// instant of the tap (RAD_EV_NEW), then the needle eases to its dial position.
#define RAD_TUNE_SECS  0.5f
#define RAD_TUNE_BTN_X 252
#define RAD_TUNE_BTN_Y 28
#define RAD_TUNE_BTN_W 36
#define RAD_TUNE_BTN_H 16
#define RAD_DIAL_SP    11            // px per MHz (was 13; tightened to fit the button)
// help (?) + band (B) button centres and the popup-panel box — SHARED by draw + hit-test so the
// clickable area can't drift from the drawn one (draw r=6 vs hit r=9 differ on purpose: fat-finger pad).
#define RAD_HELP_BTN_X 288
#define RAD_BAND_BTN_X 32
#define RAD_BTN_Y      172
#define RAD_BTN_HIT_R2 81
#define RAD_PANEL_X    44
#define RAD_PANEL_Y    40
#define RAD_PANEL_W    232
#ifndef RAD_STATIC_SLOT
#define RAD_STATIC_SLOT 31           // reserved noise slot for the static (carts use <= ~15)
#endif

static bool  rad_tuning   = false;
static float rad_tune_t   = 0;       // 0..1 sweep progress
static float rad_from_freq = 98.0f;  // needle start (the station we're leaving)
static float rad_to_freq   = 98.0f;  // needle target (the new station)
static float rad_last_freq = 98.0f;  // last real freq drawn — the "from" for the next sweep
static int   rad_static_h  = -1;
static bool  rad_static_ready = false;

static bool rad_is_tuning(void) { return rad_tuning; }

// begin a tune: snapshot where we are, fire up the static. The cart rolls the
// new song this same frame (RAD_EV_NEW), so rad_dial captures its freq as the target.
static void rad_tune_start(void) {
    if (rad_tuning) return;
    rad_tuning = true;
    rad_tune_t = 0;
    rad_from_freq = rad_last_freq;
    if (!rad_static_ready) {                 // lazy: a short-tailed noise voice for the hiss
        instrument(RAD_STATIC_SLOT, INSTR_NOISE, 6, 40, 7, 90);
        rad_static_ready = true;
    }
    rad_static_h = note_on(60, RAD_STATIC_SLOT, 0);   // starts silent, bell-curve up
}

// advance the sweep one frame (called from rad_input). Shapes the static volume
// (fast in, slow out) and ends the tune when the needle has landed.
static void rad_tune_advance(void) {
    if (!rad_tuning) return;
    rad_tune_t += dt() / RAD_TUNE_SECS;
    float t = rad_tune_t;
    float b = t < 0.15f ? t / 0.15f : 1.0f - (t - 0.15f) / 0.85f;   // hiss envelope
    if (b < 0) b = 0; if (b > 1) b = 1;
    if (rad_static_h >= 0) note_vol(rad_static_h, (int)(b * 6 + 0.5f));
    if (rad_tune_t >= 1.0f) {
        rad_tuning = false;
        if (rad_static_h >= 0) { note_off(rad_static_h); rad_static_h = -1; }
        rad_last_freq = rad_to_freq;
    }
}

// the tune button, beside the dial — lit while a sweep is running
static void rad_tuner_button(int accent) {
    bool hot = rad_tuning || ui_hover(RAD_TUNE_BTN_X, RAD_TUNE_BTN_Y, RAD_TUNE_BTN_W, RAD_TUNE_BTN_H);
    rectfill(RAD_TUNE_BTN_X, RAD_TUNE_BTN_Y, RAD_TUNE_BTN_W, RAD_TUNE_BTN_H, hot ? accent : CLR_DARKER_GREY);
    rect(RAD_TUNE_BTN_X, RAD_TUNE_BTN_Y, RAD_TUNE_BTN_W, RAD_TUNE_BTN_H, accent);
    int tc = hot ? CLR_BLACK : accent;
    print("tune", RAD_TUNE_BTN_X + (RAD_TUNE_BTN_W - text_width("tune")) / 2,
          RAD_TUNE_BTN_Y + (RAD_TUNE_BTN_H - 6) / 2, tc);
}

// the freq the needle should DRAW at: animated (eased old→new) mid-sweep, else
// the real one — and it remembers the real value as the next sweep's start.
// rad_dial uses this; carts that hand-roll their own dial call it the same way.
// Range-agnostic: works for FM (88..107) or ambient's AM band alike.
static float rad_needle_freq(float real) {
    if (rad_tuning) {
        rad_to_freq = real;
        float e = rad_tune_t < 0 ? 0 : rad_tune_t > 1 ? 1 : rad_tune_t;
        e = e * e * (3 - 2 * e);                     // smoothstep ease
        return rad_from_freq + (rad_to_freq - rad_from_freq) * e;
    }
    rad_last_freq = real;
    return real;
}

static int rad_input(int *tempo, int tmin, int tmax, int tstep,
                     int *intensity, int *toneSel, int ntone,
                     bool *radioOn, bool *showHelp) {
    int ev = 0;
    rad_tune_advance();                              // keep any running sweep moving
    // the tuner: SPACE (desktop) or a tap on the tune button (mobile) starts a sweep
    // to the next song. RAD_EV_NEW fires now so the cart rolls it; the needle eases over.
    bool tunePress = keyp(KEY_SPACE);
    if (mouse_pressed(MOUSE_LEFT)) {
        int mx = mouse_x(), my = mouse_y();
        if (mx >= RAD_TUNE_BTN_X && mx < RAD_TUNE_BTN_X + RAD_TUNE_BTN_W &&
            my >= RAD_TUNE_BTN_Y && my < RAD_TUNE_BTN_Y + RAD_TUNE_BTN_H) tunePress = true;
    }
    if (tunePress && !rad_tuning) { rad_tune_start(); ev |= RAD_EV_NEW; }
    if (keyp('R'))       ev |= RAD_EV_REPLAY;
    if (keyp('['))       ev |= RAD_EV_BACK;
    if (keyp(']'))       ev |= RAD_EV_FWD;
    if (keyp(KEY_RIGHT) && *intensity < 3) (*intensity)++;
    if (keyp(KEY_LEFT)  && *intensity > 0) (*intensity)--;
    if (keyp(KEY_UP)   && *tempo < tmax) { *tempo += tstep; bpm(*tempo); ev |= RAD_EV_TEMPO; }
    if (keyp(KEY_DOWN) && *tempo > tmin) { *tempo -= tstep; bpm(*tempo); ev |= RAD_EV_TEMPO; }
    if (ntone > 0 && keyp('T')) { *toneSel = (*toneSel + 1) % ntone; ev |= RAD_EV_TONE; }
    if (keyp('M')) { *radioOn = !*radioOn; ev |= RAD_EV_POWER; }
    if (keyp('H')) *showHelp = !*showHelp;
    if (mouse_pressed(MOUSE_LEFT)) {                 // the ?-button, bottom right
        int hx = mouse_x() - RAD_HELP_BTN_X, hy = mouse_y() - RAD_BTN_Y;
        if (hx * hx + hy * hy < RAD_BTN_HIT_R2) *showHelp = !*showHelp;
    }
    return ev;
}

// ── the chassis draw blocks ───────────────────────────────────────────────
// The cart draws ONLY its window art (mirror ball, plant, gong rack…) between these.
// Coordinates are the family's fixed 320x200 chassis; the accent color is the
// station's identity.

// radio body — chrome shell, black face, accent pinstripe
static void rad_body(int shell, int accent) {
    rectfill(20, 16, 280, 168, shell);
    rectfill(24, 20, 272, 160, CLR_BLACK);
    rectfill(24, 20, 272, 2, accent);
}

// FM dial strip with the station needle + the tune button (right).
// While tuning, the needle sweeps from the old station to this freq (the new
// song's) — so the caller just passes its current freq and the animation rides on top.
static void rad_dial(float freq, int accent) {
    float shown = rad_needle_freq(freq);             // animated mid-sweep, else the real freq
    rectfill(32, 28, 218, 16, CLR_DARKER_GREY);      // strip stops short of the tune button
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * RAD_DIAL_SP;
        line(x, 38, x, 42, CLR_DARK_GREY);
        if (fq % 4 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 6, 30, CLR_LIGHT_GREY);
        }
    }
    int nx = 36 + (int)((shown - 88.0f) * RAD_DIAL_SP);
    line(nx, 29, nx, 43, accent);
    rad_tuner_button(accent);
}

// a labelled rotary knob, t = 0..1 — the READOUT form (vu/drift meters)
static void rad_knob(int x, int y, int r, float t, const char *label, int col) {
    circfill(x, y, r, CLR_DARK_GREY);
    circ(x, y, r, CLR_BLACK);
    float a = (-0.75f + t * 1.5f) * 3.14159f;
    line(x, y, x + (int)(sinf(a) * (r - 2)), y - (int)(cosf(a) * (r - 2)), col);
    print(label, x - text_width(label) / 2, y + r + 3, CLR_LIGHT_GREY);
}

// ── draggable CONTROL knobs (mouse + touch, built on ui.h) ─────────────────
// rad_knob above is a meter; these are live controls. They reuse ui.h's
// per-finger capture (so two knobs turn at once) but render with the rad_knob
// face — every station keeps its accent. The cart MUST bracket its draw() with
// ui_begin()/ui_end() or the capture never resolves. The keyboard path in
// rad_input still works: drag and the arrow keys write the same var and coexist.
// Vertical drag, UI_KNOB_DRAG_PX for a full sweep; mouse wheel fine-tunes on
// hover. Each helper returns true ONLY the frame the value actually changes, so
// the cart re-applies its side effect (bpm(), apply_voicing()) on that frame.

// the knob face with a hot tint while grabbed/hovered (rad_knob look, live)
static void rad_knob_face(int x, int y, int r, float t,
                          const char *label, int col, bool hot) {
    circfill(x, y, r, CLR_DARK_GREY);
    circ(x, y, r, hot ? CLR_WHITE : CLR_BLACK);
    float a = (-0.75f + t * 1.5f) * 3.14159f;
    line(x, y, x + (int)(sinf(a) * (r - 2)), y - (int)(cosf(a) * (r - 2)), col);
    print(label, x - text_width(label) / 2, y + r + 3, CLR_LIGHT_GREY);
}

// shared capture: id = the bound value's address; updates *t (0..1) from the
// drag/wheel and returns true if the contact moved it this frame. *hot out.
static bool rad_knob_drag(void *id, int x, int y, int r, float *t, bool *hot) {
    ui_reg(id, x - r, y - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(id);
    *hot = c != 0 || ui_hover(x - r, y - r, 2 * r + 1, 2 * r + 1);
    if (c) {
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *t; c->by = c->cy; }
        int py = c->released ? c->ry : c->cy;
        *t = clamp(c->v0 + (c->by - py) / (float)UI_KNOB_DRAG_PX, 0, 1);
        return true;
    }
    if (*hot && mouse_wheel() != 0) {
        *t = clamp(*t + mouse_wheel() * UI_WHEEL_STEP, 0, 1);
        return true;
    }
    return false;
}

// knob bound to a ranged int (tempo): lo..hi by step
static bool rad_knob_int(int *value, int lo, int hi, int step,
                         int x, int y, int r, const char *label, int col) {
    int span = hi - lo;
    float t = span > 0 ? (float)(*value - lo) / span : 0;
    bool hot, changed = false;
    if (rad_knob_drag(value, x, y, r, &t, &hot)) {
        int nv = lo + (int)(t * span / step + 0.5f) * step;
        nv = nv < lo ? lo : nv > hi ? hi : nv;
        changed = (nv != *value);
        *value = nv;
        t = span > 0 ? (float)(*value - lo) / span : 0;
    }
    rad_knob_face(x, y, r, t, label, col, hot);
    return changed;
}

// knob bound to a discrete selector 0..ndetent-1 (feel, tone): snaps to detents
static bool rad_knob_sel(int *sel, int ndetent,
                         int x, int y, int r, const char *label, int col) {
    float t = ndetent > 1 ? (float)*sel / (ndetent - 1) : 0;
    bool hot, changed = false;
    if (rad_knob_drag(sel, x, y, r, &t, &hot)) {
        int nv = (int)(t * (ndetent - 1) + 0.5f);
        nv = nv < 0 ? 0 : nv > ndetent - 1 ? ndetent - 1 : nv;
        changed = (nv != *sel);
        *sel = nv;
        t = ndetent > 1 ? (float)*sel / (ndetent - 1) : 0;
    }
    rad_knob_face(x, y, r, t, label, col, hot);
    return changed;
}

// knob bound to a float in [lo,hi] (ambient's pace); step 0 = continuous
static bool rad_knob_float(float *value, float lo, float hi, float step,
                           int x, int y, int r, const char *label, int col) {
    float span = hi - lo;
    float t = span > 0 ? (*value - lo) / span : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    bool hot, changed = false;
    if (rad_knob_drag(value, x, y, r, &t, &hot)) {
        float nv = lo + t * span;
        if (step > 0) nv = lo + roundf((nv - lo) / step) * step;
        nv = nv < lo ? lo : nv > hi ? hi : nv;
        changed = (nv != *value);
        *value = nv;
        t = span > 0 ? (*value - lo) / span : 0;
    }
    rad_knob_face(x, y, r, t, label, col, hot);
    return changed;
}

// the loop readout: chord labels in a row, current one boxed in the accent
static void rad_chord_row(char labels[][12], int n, int cur, int x, int y, int accent) {
    for (int i = 0; i < n; i++) {
        int cw = text_width(labels[i]);
        if (x + cw > 292) break;
        if (i == cur) {
            rectfill(x - 2, y - 2, cw + 4, 12, accent);
            print(labels[i], x, y, CLR_BLACK);
        } else
            print(labels[i], x, y, CLR_LIGHT_GREY);
        x += cw + 8;
    }
}

// phrase progress dots (filled up to and including cur)
static void rad_phrase_dots(int x, int y, int n, long cur, int accent) {
    for (int i = 0; i < n; i++)
        circfill(x + i * 7, y, 1, i <= cur ? accent : CLR_DARK_GREY);
}

// power LED — blinks on the beat while playing
static void rad_power_led(bool on, int accent, int dim) {
    circfill(292, 36, 2, on && beat_pos() < 0.25f ? accent : dim);  // right of the tune button
}

// the ?-button (its hit test lives in rad_input), bottom-right
static void rad_help_button(int accent) {
    circfill(RAD_HELP_BTN_X, RAD_BTN_Y, 6, CLR_DARKER_GREY);
    circ(RAD_HELP_BTN_X, RAD_BTN_Y, 6, CLR_BLACK);
    print("?", 285, 169, accent);
}
// the B-button (its hit test lives in rad_band_input), bottom-left — mirror of
// the ?-button. Draw it only on stations that register a band panel; it's the
// sole affordance for THE BAND now that the footer hint is gone.
static void rad_band_button(int accent) {
    circfill(RAD_BAND_BTN_X, RAD_BTN_Y, 6, CLR_DARKER_GREY);
    circ(RAD_BAND_BTN_X, RAD_BTN_Y, 6, CLR_BLACK);
    print("B", 29, 169, accent);
}
// footer retired — the ? and B buttons replace the one-line hint, freeing the
// bottom strip for the ribbon. Kept as a no-op so existing calls compile during
// the per-cart cleanup; remove the calls and this symbol once they're all gone.
static void rad_footer(const char *hint) { (void)hint; }

// the help panel scaffold: key/desc rows + accent note lines at the bottom
static void rad_help_panel(const char *title, const char *(*rows)[2], int nrows,
                           const char **notes, int nnotes, int accent) {
    // panel auto-sizes to its content (was a fixed 122-tall box that capped out at
    // 8 rows + 3 notes; the 9th row collided with the notes). notesY = 132 for the
    // common 8-row case, so existing panels stay pixel-identical.
    int notesY = 58 + nrows * 9 + 2;            // notes sit one line below the last control row
    int panelH = (notesY + nnotes * 9 + 2) - 40;
    rectfill(RAD_PANEL_X, RAD_PANEL_Y, RAD_PANEL_W, panelH, CLR_BLACK);
    rect(RAD_PANEL_X, RAD_PANEL_Y, RAD_PANEL_W, panelH, accent);
    print(title, 52, 46, accent);
    font(FONT_SMALL);
    for (int i = 0; i < nrows; i++) {
        print(rows[i][0], 52, 58 + i * 9, CLR_YELLOW);
        print(rows[i][1], 106, 58 + i * 9, CLR_WHITE);
    }
    for (int i = 0; i < nnotes; i++)
        print(notes[i], 52, notesY + i * 9, accent);
    font(FONT_NORMAL);
}

// ── the band panel ────────────────────────────────────────────────────────
// The G-key A/B toggle generalized (radio-instrument-options.md): a second
// overlay besides help where every CHAIR (instrument seat) lists its timbre
// candidates — click a row or press its number to cycle, live, mid-song.
// The toolkit owns the registry + input + draw; the CART applies the change:
// rad_band_input returns the chair index that just cycled, the cart re-aims
// that instrument slot. The toolkit never calls back into the cart, and the
// panel never touches rad_srnd — pinned seeds stay intact. Candidates per
// chair come from docs/design/radio-instrument-options.md; once a station's
// candidates all sound right, the song seed rolls the default and this panel
// becomes the live override.

#define RAD_MAXCHAIR 6
#define RAD_MAXCAND  4

typedef struct {
    const char *name;                  // the chair: "piano", "bass", "drums"…
    const char *cand[RAD_MAXCAND];     // candidate labels
    int ncand, sel;
} RadChair;

typedef struct {
    RadChair c[RAD_MAXCHAIR];
    int  n;
    bool show;
} RadBand;

// register a chair (pass NULL for unused candidate slots); returns its index
static int rad_chair(RadBand *b, const char *name,
                     const char *c0, const char *c1, const char *c2, const char *c3) {
    RadChair *ch = &b->c[b->n];
    const char *cs[RAD_MAXCAND] = { c0, c1, c2, c3 };
    ch->name = name; ch->sel = 0; ch->ncand = 0;
    for (int i = 0; i < RAD_MAXCAND && cs[i]; i++) ch->cand[ch->ncand++] = cs[i];
    return b->n++;
}

// B toggles the panel (closing help — the two overlays share the spot);
// number keys 1..n and row clicks cycle a chair. Returns the chair index
// that just cycled (apply it in the cart) or -1.
static int rad_band_input(RadBand *b, bool *showHelp) {
    if (keyp('B')) { b->show = !b->show; if (b->show) *showHelp = false; }
    if (mouse_pressed(MOUSE_LEFT)) {                 // the B-button, bottom left
        int dx = mouse_x() - RAD_BAND_BTN_X, dy = mouse_y() - RAD_BTN_Y;
        if (dx * dx + dy * dy < RAD_BTN_HIT_R2) { b->show = !b->show; if (b->show) *showHelp = false; }
    }
    if (*showHelp) b->show = false;
    if (!b->show) return -1;
    int hit = -1;
    for (int i = 0; i < b->n; i++)
        if (keyp('1' + i)) hit = i;
    if (mouse_pressed(MOUSE_LEFT)) {
        int mx = mouse_x(), my = mouse_y();
        for (int i = 0; i < b->n; i++) {
            int ry = 58 + i * 13;
            if (mx >= 48 && mx <= 272 && my >= ry - 3 && my < ry + 10) hit = i;
        }
    }
    if (hit >= 0) b->c[hit].sel = (b->c[hit].sel + 1) % b->c[hit].ncand;
    return hit;
}

// draw the panel — same chassis spot as the help overlay
static void rad_band_panel(const RadBand *b, int accent) {
    if (!b->show) return;
    rectfill(RAD_PANEL_X, RAD_PANEL_Y, RAD_PANEL_W, 122, CLR_BLACK);
    rect(RAD_PANEL_X, RAD_PANEL_Y, RAD_PANEL_W, 122, accent);
    print("THE BAND", 52, 46, accent);
    font(FONT_SMALL);
    for (int i = 0; i < b->n; i++) {
        const RadChair *ch = &b->c[i];
        int ry = 58 + i * 13;
        char num[4]; snprintf(num, 4, "%d", i + 1);
        print(num, 52, ry, CLR_YELLOW);
        print(ch->name, 62, ry, CLR_WHITE);
        int cw = text_width(ch->cand[ch->sel]);          // current pick, boxed
        rectfill(148, ry - 2, cw + 6, 11, accent);
        print(ch->cand[ch->sel], 151, ry, CLR_BLACK);
        for (int k = 0; k < ch->ncand; k++)              // position dots
            circfill(162 + cw + k * 5, ry + 3, 1,
                     k == ch->sel ? accent : CLR_DARK_GREY);
    }
    print("click a row / press its number to cycle", 52, 137, accent);
    print("the swap lands live, mid-song", 52, 146, accent);
    font(FONT_NORMAL);
}

#endif // RADIO_H
