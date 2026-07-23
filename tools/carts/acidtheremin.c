/* de:meta
{
  "title": "Acid Theremin",
  "slug": "acidtheremin",
  "kind": ["instrument"],
  "teaches": [],
  "status": "wip",
  "created": "2026-07-23",
  "lineage": "acidcandy",
  "homage": "the TB-303 + the theremin",
  "description": {
    "summary": "A frets-off acid theremin: smear a finger across a dark pad and one held TB-303 voice CHASES it with portamento. X = pitch (continuous glissando, no keys), Y = filter cutoff (bottom = closed/dark, top = screaming open). Resonance is cranked toward self-oscillation and the drive runs through a wavefolder, so it squelches and screams — gnarly by design. Built on the shared runtime/acid303.h voice.",
    "detail": "One Devil-Fish Acid voice (acid303.h) held open the whole time you touch. Touch-down triggers the filter-env kick with an accent stab, then note_pitch rides the finger's FRACTIONAL pitch every frame (note_glide smears it — the acid slide as a continuous gesture); note_off on release. Y drives ACID_CUT live via acid_ride AND pushes ACID_DRV up as you open the filter, so the top of the pad is where it screams. A sub-osc an octave down adds weight; analog DRIFT keeps it from freezing digital. Keys: 1/2/3/4 pick the drive waveshaper (SOFT/HARD/FOLD/ASYM — FOLD is the metallic default), S toggles free glide vs minor-pentatonic scale-snap. A phosphor trail follows the finger; the orb runs hot as the filter opens.",
    "controls": "Drag anywhere (mouse or touch): X = pitch, Y = filter cutoff (up = brighter + more drive). Release to let the note decay. 1/2/3/4 = drive mode SOFT/HARD/FOLD/ASYM. S = snap pitch to a minor-pentatonic scale (off = true theremin glide)."
  }
}
de:meta */

// Acid Theremin — the TB-303's resonant filter + portamento slide IS a theremin gesture.
// A full-bleed XY pad: X = continuous pitch (the note chases your finger), Y = cutoff.
// Gnarly by design: high resonance, a wavefolder drive, sub-osc, analog drift.

#include "studio.h"
#include "acid303.h"
#include <math.h>

static Acid a;
static int  touching = 0;
static int  fx = 0, fy = 0;        // last finger position (canvas px)
static int  snap = 0;              // 0 = free glide (theremin) · 1 = minor-pentatonic snap
static float curpitch = 40.0f;     // the pitch we're currently commanding (for the readout)

static const char *NOTE[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static const char *DRVN[4]  = { "SOFT","HARD","FOLD","ASYM" };

// pitch range across the pad: ~3 octaves
#define P_LO 28.0f
#define P_HI 64.0f

// snap a fractional midi pitch to the nearest minor-pentatonic degree (A-rooted)
static float snap_pent(float p) {
    static const int deg[5] = { 0, 3, 5, 7, 10 };   // minor pentatonic
    int base = (int)floorf(p / 12.0f) * 12;          // octave floor
    float best = p; float bd = 1e9f;
    for (int o = -1; o <= 1; o++) for (int k = 0; k < 5; k++) {
        float cand = base + o * 12 + deg[k] + 9;     // +9 → A root (A = midi 9 in octave)
        float d = fabsf(cand - p); if (d < bd) { bd = d; best = cand; }
    }
    return best;
}

// ── phosphor trail ────────────────────────────────────────────────────────────
#define TRAIL 40
static int tX[TRAIL], tY[TRAIL], tHead = 0, tLen = 0;
static void trail_push(int x, int y) {
    tX[tHead] = x; tY[tHead] = y; tHead = (tHead + 1) % TRAIL; if (tLen < TRAIL) tLen++;
}

// read the active finger (first touch, else the held mouse). Returns 1 if down.
static int finger(int *x, int *y) {
    if (touch_count() > 0) { int tx = touch_x(0), ty = touch_y(0); if (tx >= 0) { *x = tx; *y = ty; return 1; } }
    if (mouse_down(MOUSE_LEFT)) { *x = mouse_x(); *y = mouse_y(); return 1; }
    return 0;
}

void init(void) {
    acid_init(&a, 6, 34);              // voice slot 6, sub-osc slot 34
    a.drvmode = DRIVE_FOLD;            // metallic wavefolder — the gnarl
    a.drift   = 0.25f;                 // a little analog wander
    a.p[ACID_RES]  = 0.92f;            // near self-oscillation — the squelch/scream
    a.p[ACID_ENV]  = 0.55f;
    a.p[ACID_DEC]  = 0.65f;
    a.p[ACID_DRV]  = 0.60f;
    a.p[ACID_SUB]  = 0.55f;            // octave-down weight
    a.p[ACID_SLDT] = 0.85f;            // long glide time
    a.p[ACID_ACC]  = 0.80f;
    a.p[ACID_ATK]  = 0.08f;
    acid_define(&a);
}

void update(void) {
    int W = screen_w(), H = screen_h();

    // drive-mode + snap keys
    for (int k = 0; k < 4; k++) if (keyp('1' + k)) { a.drvmode = k; acid_define(&a); if (a.h >= 0) note_drive_mode(a.h, a.drvmode); }
    if (keyp('s') || keyp('S')) snap = !snap;

    int px, py, down = finger(&px, &py);
    if (down) {
        if (px < 0) px = 0; if (px >= W) px = W - 1;
        if (py < 0) py = 0; if (py >= H) py = H - 1;
        float p = P_LO + (px / (float)W) * (P_HI - P_LO);
        if (snap) p = snap_pent(p);
        curpitch = p;
        float cut = 1.0f - py / (float)H; if (cut < 0.05f) cut = 0.05f; if (cut > 0.98f) cut = 0.98f;
        a.p[ACID_CUT] = cut;
        a.p[ACID_DRV] = 0.35f + cut * 0.55f;      // opening the filter also pushes the drive → the scream lives up top

        if (!touching) {                          // touch-DOWN → trigger the held voice with an accent stab
            acid_note(&a, (int)(p + 0.5f), 1, 0);
            note_glide(a.h, 6);                   // snap onto the first pitch
            if (a.hsub >= 0) note_glide(a.hsub, 6);
        }
        note_pitch(a.h, p);                       // ride the FRACTIONAL pitch — true continuous glissando
        note_glide(a.h, 45);                      // smear toward it (the acid slide, as a gesture)
        if (a.hsub >= 0) { note_pitch(a.hsub, p - 12); note_glide(a.hsub, 45); }
        touching = 1; fx = px; fy = py;
        trail_push(px, py);
    } else if (touching) {                        // release → let it decay
        acid_off(&a);
        touching = 0;
    }
    acid_ride(&a);                                // cutoff/res/drive follow a.p[] into the ring
}

// hot→cold colour for the filter cutoff (top of the pad runs hot)
static int cut_col(float cut) {
    if (cut > 0.82f) return CLR_WHITE;
    if (cut > 0.62f) return CLR_LIGHT_YELLOW;
    if (cut > 0.42f) return CLR_ORANGE;
    if (cut > 0.24f) return CLR_PINK;
    return CLR_MAUVE;
}

void draw(void) {
    int W = screen_w(), H = screen_h();
    cls(CLR_BLACK);
    // deep vertical wash so the pad reads as a field (dark top→blacker bottom)
    for (int y = 0; y < H; y += 2) {
        int c = (y < H / 3) ? CLR_DARK_PURPLE : (y < 2 * H / 3) ? CLR_DARKER_PURPLE : CLR_BLACK;
        line(0, y, W - 1, y, c);
    }

    // pitch grid — a faint line per semitone, octaves brighter (the "frets you can ignore")
    for (float p = P_LO; p <= P_HI + 0.01f; p += 1.0f) {
        int x = (int)((p - P_LO) / (P_HI - P_LO) * W);
        int oct = ((int)p % 12) == 9;              // A = octave marker (root of the pentatonic)
        line(x, 0, x, H, oct ? CLR_DARK_BLUE : CLR_BROWNISH_BLACK);
        if (oct) { font(FONT_TINY); print(NOTE[9], x + 1, H - 8, CLR_DARK_BLUE); }
    }

    // the phosphor trail — newest bright, fading back through pink to purple
    for (int i = 0; i < tLen; i++) {
        int idx = (tHead - 1 - i + TRAIL * 2) % TRAIL;
        int age = i;                                // 0 = newest
        int c = age < 4 ? CLR_WHITE : age < 10 ? CLR_PINK : age < 20 ? CLR_MAUVE : CLR_DARK_PURPLE;
        int r = age < 4 ? 2 : 1;
        circfill(tX[idx], tY[idx], r, c);
    }

    // the orb — glows hot as the filter opens; a ring pulse when it's really screaming
    if (touching) {
        float cut = a.p[ACID_CUT];
        int c = cut_col(cut);
        int r = 4 + (int)(cut * 6);
        blend(BLEND_AVG); circfill(fx, fy, r + 3, c); blend_reset();
        circfill(fx, fy, r, c);
        circ(fx, fy, r, CLR_WHITE);
        if (a.p[ACID_RES] > 0.85f && cut > 0.6f) circ(fx, fy, r + 4, CLR_LIGHT_YELLOW);  // self-osc halo
        // crosshair guides
        line(0, fy, W, fy, CLR_BROWNISH_BLACK);
        line(fx, 0, fx, H, CLR_BROWNISH_BLACK);
    }

    // HUD
    font(FONT_TINY);
    print("ACID THEREMIN", 4, 4, CLR_LIME_GREEN);
    {   // note name + octave of the commanded pitch
        int m = (int)(curpitch + 0.5f);
        char buf[8]; int oct = m / 12 - 1, k = 0;
        const char *nn = NOTE[((m % 12) + 12) % 12];
        buf[k++] = nn[0]; if (nn[1]) buf[k++] = nn[1];
        buf[k++] = (char)('0' + (oct < 0 ? 0 : oct));
        buf[k] = 0;
        print(buf, 4, 12, touching ? cut_col(a.p[ACID_CUT]) : CLR_MEDIUM_GREY);
    }
    print(DRVN[a.drvmode & 3], W - 34, 4, CLR_ORANGE);
    print(snap ? "SNAP" : "GLIDE", W - 34, 12, snap ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);
    if (!touching) print("drag: X=pitch  Y=filter", W / 2 - 44, H / 2, CLR_DARKER_GREY);

    // scanlines for the gnarly CRT feel
    blend(BLEND_AVG);
    for (int y = 0; y < H; y += 3) line(0, y, W - 1, y, CLR_BLACK);
    blend_reset();
}
