#include "studio.h"
#include "ui.h"
#include <math.h>

// EUCLID — a Euclidean rhythm box, after Mutable Instruments Grids / the Euclidean
// sequencer (Toussaint's "the Euclidean algorithm generates traditional rhythms").
//
// The whole magic is ONE idea: spread k hits as EVENLY as possible across n steps and
// you get tresillo, the clave, the world's drum patterns — for free, just from numbers.
// So there's no step grid to draw by hand; you DIAL each drum's DENSITY and ROTATION and
// the groove blooms and dissolves generatively. Per-track step counts differ, so the
// rings drift against each other (polymeter) — the loop never sits still.
//
//   • 5 drum rings — KICK · SNARE · HAT · OPEN HAT · PERC, nested; the playhead sweeps each.
//   • 1-5 (or tap the strip) pick the track · ↑/↓ density (hits) · ←/→ rotation
//   • the HITS / STEPS / ROT knobs shape the selected track · TEMPO rides it
//   • M mute the selected track · SPACE = MUTATE (nudge every density — shuffle the groove)
//
//   Built on the engine's euclid() Bjorklund helper. The rhythm companion to kaoss/onenote;
//   the loop it makes is exactly what kaoss is built to mangle.

// TEACHES: euclidean-rhythm, generative-sequencer, polymeter, drum-synthesis, nested-ring-ui
#define NT 5                         // tracks / rings
enum { SL_KICK = 5, SL_SNARE, SL_CHAT, SL_OHAT, SL_PERC };

static const char *TNAME[NT] = { "KICK", "SNARE", "HAT", "OPEN HAT", "PERC" };
static const int   TCOL [NT] = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN, CLR_BLUE };
static const int   TSLOT[NT] = { SL_KICK, SL_SNARE, SL_CHAT, SL_OHAT, SL_PERC };
static const int   RAD  [NT] = { 88, 71, 54, 37, 21 };      // ring radii (outer→inner)
#define CX 104
#define CY 104

// source of truth = normalized knob floats per track; integers derived each frame.
typedef struct { float kh, ks, kr; bool mute; int flash; } Trk;
static Trk trk[NT];
static int  sel = 0;                 // selected track
static int  gstep = -1;              // global 16th counter (monotonic)
static int  last_16 = -1;
static int  tempo = 124;
static float k_tempo = (124 - 80) / 100.0f;   // → 80..180 BPM

// derive a track's integer params from its knob floats
static int t_steps(int t) { return 2 + (int)(trk[t].ks * 14 + 0.5f); }            // 2..16
static int t_hits (int t) { int n = t_steps(t); return (int)(trk[t].kh * n + 0.5f); }   // 0..n
static int t_rot  (int t) { int n = t_steps(t); return n > 1 ? (int)(trk[t].kr * (n - 1) + 0.5f) : 0; }

// write an integer hit-count back into the knob float (for keyboard / mutate)
static void set_hits(int t, int k) { int n = t_steps(t); if (k < 0) k = 0; if (k > n) k = n; trk[t].kh = (float)k / n; }
static void set_rot (int t, int r) { int n = t_steps(t); if (n < 2) { trk[t].kr = 0; return; } r = ((r % n) + n) % n; trk[t].kr = (float)r / (n - 1); }

static void play_row(int t) {
    switch (t) {
        case 0: hit(72, INSTR_NOISE, 2, 12); hit(34, SL_KICK, 6, 250); break;          // click + sine boom
        case 1: hit(58, SL_SNARE, 5, 110); hit(53, INSTR_TRI, 3, 45); break;           // noise bark + body
        case 2: hit(92, SL_CHAT, 3, 24); break;                                        // closed hat tick
        case 3: hit(92, SL_OHAT, 2, 170); break;                                       // open hat sizzle
        case 4: hit(79, SL_PERC, 4, 40); break;                                        // wooden clave tok
    }
    trk[t].flash = frame();
}

// preset the classic Euclidean defaults — instantly a groove (kick 4/16, snare backbeat,
// hat 8ths, open-hat lift, perc the 3-in-8 tresillo)
static void preset(int t, int k, int n, int r) {
    trk[t].ks = (float)(n - 2) / 14.0f;
    set_hits(t, k);
    set_rot(t, r);
}

void init() {
    instrument(SL_KICK, INSTR_SINE, 0, 280, 0, 60);
    instrument_env(SL_KICK, 1, ENV_PITCH, 0, 55, 30);
    instrument_drive(SL_KICK, 0.26f);
    instrument(SL_SNARE, INSTR_NOISE, 0, 130, 0, 50);  instrument_filter(SL_SNARE, FILTER_BAND, 1400, 3);
    instrument(SL_CHAT,  INSTR_NOISE, 0, 24,  0, 14);  instrument_filter(SL_CHAT,  FILTER_HIGH, 7000, 2);
    instrument(SL_OHAT,  INSTR_NOISE, 0, 180, 0, 90);  instrument_filter(SL_OHAT,  FILTER_HIGH, 6500, 2);
    instrument(SL_PERC,  INSTR_TRI,   0, 55,  0, 30);  instrument_filter(SL_PERC,  FILTER_BAND, 2200, 4);

    preset(0, 4, 16, 0);   // KICK   — four on the floor
    preset(1, 2, 16, 4);   // SNARE  — backbeat
    preset(2, 8, 16, 0);   // HAT    — straight 8ths
    preset(3, 2, 16, 2);   // OHAT   — lift before the turn
    preset(4, 3,  8, 0);   // PERC   — the 3-in-8 tresillo
}

void update() {
    if (btnp(0, BTN_LEFT))  set_rot(sel, t_rot(sel) - 1);
    if (btnp(0, BTN_RIGHT)) set_rot(sel, t_rot(sel) + 1);
    if (btnp(0, BTN_DOWN))  set_hits(sel, t_hits(sel) - 1);
    if (btnp(0, BTN_UP))    set_hits(sel, t_hits(sel) + 1);
    for (int i = 0; i < NT; i++) if (keyp('1' + i)) sel = i;
    if (keyp('M')) trk[sel].mute = !trk[sel].mute;
    if (keyp(KEY_SPACE))                                   // MUTATE — shuffle every groove a touch
        for (int t = 0; t < NT; t++) { set_hits(t, t_hits(t) + rnd(3) - 1); set_rot(t, t_rot(t) + rnd(3) - 1); }

    tempo = 80 + (int)(k_tempo * 100 + 0.5f);
    bpm(tempo);

    // transport: a monotonic 16th counter; each track fires on its OWN step count (polymeter)
    float pos16 = beat() * 4 + beat_pos() * 4.0f;
    int sixteenth = (int)pos16;
    if (sixteenth != last_16) {
        last_16 = sixteenth;
        gstep++;
        for (int t = 0; t < NT; t++) {
            int n = t_steps(t), k = t_hits(t);
            int local = ((gstep % n) + t_rot(t)) % n;
            if (!trk[t].mute && euclid(k, n, local)) play_row(t);
        }
    }

#ifdef DE_TRACE
    watch("sel",   "%s", TNAME[sel]);
    watch("hits",  "%d", t_hits(sel));
    watch("steps", "%d", t_steps(sel));
    watch("rot",   "%d", t_rot(sel));
    watch("tempo", "%d", tempo);
#endif
}

static void draw_ring(int t) {
    int n = t_steps(t), k = t_hits(t), rot = t_rot(t), r = RAD[t];
    int local = gstep < 0 ? -1 : gstep % n;          // this ring's playhead step
    bool selr = (t == sel);
    bool hot  = (frame() - trk[t].flash) < 6;
    int col = trk[t].mute ? CLR_DARK_GREY : TCOL[t];

    if (selr) circ(CX, CY, r, CLR_DARKER_GREY);       // faint guide ring on the selected track
    for (int i = 0; i < n; i++) {
        float a = -1.5707963f + (float)i / n * 6.2831853f;
        int x = CX + (int)(cosf(a) * r), y = CY + (int)(sinf(a) * r);
        bool is_hit = euclid(k, n, (i + rot) % n);
        bool head   = (i == local);
        if (is_hit) {
            int c = (head && hot && !trk[t].mute) ? CLR_WHITE : col;
            circfill(x, y, head ? 4 : 3, c);
            if (head) circ(x, y, 4, CLR_WHITE);
        } else {
            circfill(x, y, head ? 2 : 1, head ? CLR_MEDIUM_GREY : CLR_DARKER_GREY);
        }
    }
}

void draw() {
    cls(CLR_BROWNISH_BLACK);
    print("EUCLID", 2, 4, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM", tempo), 318, 4, CLR_LIGHT_GREY);

    for (int t = NT - 1; t >= 0; t--) draw_ring(t);    // inner first so outer dots sit on top
    // center beat pulse
    int bp = (gstep >= 0 && (frame() - trk[0].flash) < 6) ? 5 : 3;
    circfill(CX, CY, bp, CLR_DARK_GREY);

    // ── right panel: the track strip (tap to select) ──
    int x0 = 200, sy = 24, rh = 15;
    for (int t = 0; t < NT; t++) {
        int y = sy + t * rh;
        if (tapp(x0, y, 118, rh - 1)) sel = t;
        bool s = (t == sel);
        rectfill(x0, y, 118, rh - 1, s ? CLR_DARKER_BLUE : CLR_DARKER_GREY);
        if (s) rect(x0, y, 118, rh - 1, CLR_MEDIUM_GREY);
        circfill(x0 + 7, y + 7, 4, trk[t].mute ? CLR_DARK_GREY : TCOL[t]);
        font(FONT_SMALL);
        print(TNAME[t], x0 + 16, y + 4, trk[t].mute ? CLR_DARK_GREY : CLR_LIGHT_GREY);
        print_right(str("%d/%d", t_hits(t), t_steps(t)), x0 + 114, y + 4, s ? CLR_WHITE : CLR_MEDIUM_GREY);
        if (trk[t].mute) print("M", x0 + 78, y + 4, CLR_ORANGE);
        font(FONT_NORMAL);
    }

    // ── selected-track knobs ──
    ui_begin();
    int ky = 122;
    font(FONT_SMALL);
    ui_knob(&trk[sel].kh, 216, ky, "HITS");
    ui_knob(&trk[sel].ks, 250, ky, "STEPS");
    ui_knob(&trk[sel].kr, 284, ky, "ROT");
    ui_knob(&k_tempo,     216, ky + 40, "TEMPO");
    if (ui_button(248, ky + 32, 64, 18, trk[sel].mute ? "MUTED" : "MUTE")) trk[sel].mute = !trk[sel].mute;
    print("1-5 sel  up/dn hits", 200, 184, CLR_DARK_GREY);
    print("l/r rot  SPACE mutate", 200, 191, CLR_DARK_GREY);
    font(FONT_NORMAL);
    ui_end();
}
