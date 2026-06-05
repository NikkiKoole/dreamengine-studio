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

static int rad_input(int *tempo, int tmin, int tmax, int tstep,
                     int *intensity, int *toneSel, int ntone,
                     bool *radioOn, bool *showHelp) {
    int ev = 0;
    if (keyp(KEY_SPACE)) ev |= RAD_EV_NEW;
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
        int hx = mouse_x() - 288, hy = mouse_y() - 172;
        if (hx * hx + hy * hy < 81) *showHelp = !*showHelp;
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

// FM dial strip with the station needle
static void rad_dial(float freq, int accent) {
    rectfill(32, 28, 256, 16, CLR_DARKER_GREY);
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * 13;
        line(x, 38, x, 42, CLR_DARK_GREY);
        if (fq % 4 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 6, 30, CLR_LIGHT_GREY);
        }
    }
    int nx = 36 + (int)((freq - 88.0f) * 13.0f);
    line(nx, 29, nx, 43, accent);
}

// a labelled rotary knob, t = 0..1
static void rad_knob(int x, int y, int r, float t, const char *label, int col) {
    circfill(x, y, r, CLR_DARK_GREY);
    circ(x, y, r, CLR_BLACK);
    float a = (-0.75f + t * 1.5f) * 3.14159f;
    line(x, y, x + (int)(sinf(a) * (r - 2)), y - (int)(cosf(a) * (r - 2)), col);
    print(label, x - text_width(label) / 2, y + r + 3, CLR_LIGHT_GREY);
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
    circfill(282, 28, 2, on && beat_pos() < 0.25f ? accent : dim);
}

// the ?-button (its hit test lives in rad_input) + the one-line footer hint
static void rad_help_button(int accent) {
    circfill(288, 172, 6, CLR_DARKER_GREY);
    circ(288, 172, 6, CLR_BLACK);
    print("?", 285, 169, accent);
}
static void rad_footer(const char *hint) {
    print(hint, 8, 190, CLR_DARK_GREY);
}

// the help panel scaffold: key/desc rows + accent note lines at the bottom
static void rad_help_panel(const char *title, const char *(*rows)[2], int nrows,
                           const char **notes, int nnotes, int accent) {
    rectfill(44, 40, 232, 122, CLR_BLACK);
    rect(44, 40, 232, 122, accent);
    print(title, 52, 46, accent);
    font(FONT_SMALL);
    for (int i = 0; i < nrows; i++) {
        print(rows[i][0], 52, 58 + i * 9, CLR_YELLOW);
        print(rows[i][1], 106, 58 + i * 9, CLR_WHITE);
    }
    for (int i = 0; i < nnotes; i++)
        print(notes[i], 52, 132 + i * 9, accent);
    font(FONT_NORMAL);
}

#endif // RADIO_H
