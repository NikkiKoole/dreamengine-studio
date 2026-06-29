/* de:meta
{
  "title": "sfx editor",
  "status": "active",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "step-sequencer",
    "adsr-envelope",
    "save-load-persistence"
  ],
  "lineage": "Spiritual port of PICO-8's sfx editor; novel in the per-step rotating-slot filter trick that lets scheduled steps each carry their own cutoff, and the export-to-C workflow that embeds the sound directly in a cart with no engine banks.",
  "description": "Draw a sound, PICO-8 style. Paint a pitch contour over 32 steps with the mouse (left = draw, right = erase), shape the volume lane and the filter CUT lane (top = wide open, lower = darker; a RES slider adds squelch), pick a wave per step, set the speed, SPACE to hear it. A SCALE picker (major/minor/penta/blues/hexatonic/whole-tone + a KEY root; click cycles, right-click cycles back) snaps painted notes in key, with faint row guides showing where notes can land — OFF for chromatic freehand. Press E to EXPORT the sound as C code — a step array plus a tiny player function — printed to the log, ready to paste into your game (no engine sound banks needed: playback is just hit() per step). S/L save and load, LOOP toggles looping, BACKSPACE clears. The prototype for cart-authored sfx (audio-notes §5.6)."
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <math.h>

// SFX EDITOR — draw a sound, PICO-8 style. Paint a pitch contour over 32 steps with the
// mouse (left = draw with the selected wave, right = erase), shape the volume and filter
// CUT lanes below, pick a wave "pen", set the step speed + resonance, SPACE to hear it.
// E exports the sound as C code (a step array + a tiny player) to the log — paste it
// straight into your game. S/L save/load. No engine SFX banks involved. (audio-notes §5.6.)
//
// The CUT lane drives a per-step lowpass: top = wide open (no filter), lower = darker.
// Per-step filter needs a trick: steps are SCHEDULED ahead (schedule_hit), but filter
// defines apply immediately — so each queued step gets its own ROTATING instrument slot
// (10-15) carrying its wave + cutoff until it fires. Six slots > the ~5-step queue.

#define NSTEP 32
#define P_LO  36          // C2
#define P_HI  84          // C6

// pitch grid + volume/cut lane geometry
#define GX 8
#define SW 9              // px per step  (32 * 9 = 288)
#define GY 26
#define GH 82
#define VY 112
#define VH 18             // 8 vol levels * 2px
#define CUY 134
#define CUH 18            // 16 cut levels

typedef struct { unsigned char pitch, wave, vol, cut; } Step;   // vol 0 = silent; cut 15 = filter wide open
static Step st[NSTEP];

static int   pen = 2;             // wave the mouse paints with (0..4 = SQR SAW TRI NOI SIN)
static int   spd_ms = 80;         // step length
static int   res = 6;             // filter resonance 0..15 (applies when a step's CUT < 15)
static bool  loop_on = false;
static int   sched_k = 0;         // rotating-slot counter (slots 10-15)

// scale snap — painted pitches land on in-key notes; OFF = chromatic freehand.
// Interval tables match the engine's SCALE_* (sound.h), root transposes them.
static int scl = 0;               // 0 = OFF, then major/minor/penta/penta-/blues/hexa/whole
static int root = 0;              // 0 = C .. 11 = B
#define NSCL 8
static const char *SCLNAME[NSCL] = { "OFF", "MAJOR", "MINOR", "PENTA", "PENTA-", "BLUES", "HEXA", "WHOLE" };
static const unsigned char SCL_IV[NSCL][7] = { {0}, {0,2,4,5,7,9,11}, {0,2,3,5,7,8,10},
                                               {0,2,4,7,9}, {0,3,5,7,10}, {0,3,5,6,7,10},
                                               {0,2,4,5,7,9},            // major hexatonic (no 7th)
                                               {0,2,4,6,8,10} };         // whole-tone
static const int SCL_LEN[NSCL] = { 0, 7, 7, 5, 5, 6, 6, 6 };

static bool in_scale(int m) {
    if (!scl) return true;
    int pc = ((m - root) % 12 + 12) % 12;
    for (int i = 0; i < SCL_LEN[scl]; i++) if (SCL_IV[scl][i] == pc) return true;
    return false;
}
static int snap_midi(int m) {                     // nearest in-scale pitch; ties prefer down
    for (int d = 0; d < 12; d++) {
        if (m - d >= P_LO && in_scale(m - d)) return m - d;
        if (m + d <= P_HI && in_scale(m + d)) return m + d;
    }
    return m;
}

// CUT level 0..14 → lowpass Hz (exponential); 15 = FILTER_OFF
static int cut_hz(int c) { return (int)(150.0f * powf(2.0f, c / 15.0f * 5.0f)); }

static int   play_i = -1;         // step about to fire; -1 = stopped
static int   head = -1;           // the playhead column
static float play_t = 0;          // time until the next UNSCHEDULED step (the queue-ahead clock)
static float vis_t = -1;          // visual clock for the playhead; <0 = stopped

static int   ui_held = 0;
static const char *msg = ""; static int msg_t = 0;

static const char *WNAME[5] = { "SQR", "SAW", "TRI", "NOI", "SIN" };
static const int   WCOL[5]  = { CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_LIGHT_GREY, CLR_BLUE };
static const char *NN[12]   = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

static int midi_y(int m) { return GY + GH - (m - P_LO) * GH / (P_HI - P_LO); }
static int y_midi(int y) { return (int)clamp(P_LO + (GY + GH - y) * (P_HI - P_LO) / (float)GH + 0.5f, P_LO, P_HI); }
static int step_at(int x) { int i = (x - GX) / SW; return (x >= GX && i >= 0 && i < NSTEP) ? i : -1; }
static int in_box(int x, int y, int w, int h) { return mouse_x() >= x && mouse_x() < x + w && mouse_y() >= y && mouse_y() < y + h; }

// ---- persistence ----
typedef struct { int version, spd, loop, res; Step st[NSTEP]; int scl, root; } SaveData;
#define SAVE_V2_SIZE (sizeof(SaveData) - 2 * sizeof(int))   // v2 lacked the trailing scl/root
static void save_it(void) {
    SaveData d = { 3, spd_ms, loop_on, res, {{0}}, scl, root };
    for (int i = 0; i < NSTEP; i++) d.st[i] = st[i];
    save_bytes(&d, sizeof d);
    msg = "SAVED"; msg_t = 80;
}
static void load_it(void) {
    SaveData d;
    int got = load_bytes(&d, sizeof d);
    if ((got == (int)sizeof d && d.version == 3) || (got == (int)SAVE_V2_SIZE && d.version == 2)) {
        spd_ms = d.spd; loop_on = d.loop; res = d.res;
        for (int i = 0; i < NSTEP; i++) st[i] = d.st[i];
        if (d.version == 3) { scl = d.scl; root = d.root; }
        msg = "LOADED";
    } else msg = "no save";
    msg_t = 80;
}

// does any audible step actually use the filter? (decides which player we export)
static bool uses_filter(void) {
    for (int i = 0; i < NSTEP; i++) if (st[i].vol && st[i].cut < 15) return true;
    return false;
}

// ---- export: the sound as paste-into-your-cart C code, printed to the log ----
static void export_array(const char *name, int field) {   // field 0=pitch 1=wave 2=vol 3=cut(Hz, 0=off)
    char b[512]; int n = snprintf(b, sizeof b, "static const %s %s[%d] = {", field == 3 ? "short" : "unsigned char", name, NSTEP);
    for (int i = 0; i < NSTEP; i++) {
        int v = field == 0 ? (st[i].vol ? st[i].pitch : 0)
              : field == 1 ? st[i].wave
              : field == 2 ? st[i].vol
              :              (st[i].cut < 15 ? cut_hz(st[i].cut) : 0);
        n += snprintf(b + n, sizeof b - n, "%d%s", v, i < NSTEP - 1 ? "," : "");
    }
    snprintf(b + n, sizeof b - n, "};");
    printh("%s", b);
}
static void export_code(void) {
    bool filt = uses_filter();
    printh("// ---- sfx (drawn in the sfx editor cart) — paste into your cart ----");
    export_array("SFX_P", 0);
    export_array("SFX_W", 1);
    export_array("SFX_V", 2);
    if (filt) export_array("SFX_C", 3);
    printh("#define SFX_SPD %d   // ms per step", spd_ms);
    if (filt) printh("#define SFX_RES %d   // filter resonance 0-15", res);
    printh("static int sfx_i = -1; static float sfx_t;");
    if (filt) printh("static int sfx_k;   // rotating slot 10-15 (each queued step carries its own filter)");
    printh("void sfx_start(void) { sfx_i = 0; sfx_t = 0; }   // call to play the sound");
    printh("void sfx_tick(void) {                            // call every frame in update()");
    printh("    if (sfx_i < 0) return;");
    printh("    sfx_t -= dt();");
    printh("    while (sfx_i >= 0 && sfx_t < 0.034f) {       // queue ~2 frames ahead, sample-accurate");
    if (filt) {
        printh("        if (SFX_V[sfx_i]) {");
        printh("            int sl = 10 + (sfx_k++ %% 6);     // NB: the player owns slots 10-15");
        printh("            instrument(sl, SFX_W[sfx_i], 5, 0, 7, 20);");
        printh("            if (SFX_C[sfx_i]) instrument_filter(sl, FILTER_LOW, SFX_C[sfx_i], SFX_RES);");
        printh("            else              instrument_filter(sl, FILTER_OFF, 1000, 0);");
        printh("            schedule_hit(sfx_t > 0 ? (int)(sfx_t * 1000.0f) : 0,");
        printh("                         SFX_P[sfx_i], sl, SFX_V[sfx_i], SFX_SPD + 5);");
        printh("        }");
    } else {
        printh("        if (SFX_V[sfx_i]) schedule_hit(sfx_t > 0 ? (int)(sfx_t * 1000.0f) : 0,");
        printh("                                       SFX_P[sfx_i], SFX_W[sfx_i], SFX_V[sfx_i], SFX_SPD + 5);");
    }
    printh("        sfx_t += SFX_SPD / 1000.0f;");
    printh("        if (++sfx_i >= %d) sfx_i = -1;", NSTEP);
    printh("    }");
    printh("}");
    msg = "EXPORTED to the log panel"; msg_t = 140;
}

void init(void) {
    // every step starts with the filter wide open (cut 15 = off)
    for (int i = 0; i < NSTEP; i++) st[i].cut = 15;
    // seed a little pickup-coin arp so SPACE makes a sound immediately
    const int seed_p[8] = { 64, 69, 72, 76, 81, 84, 88, 88 };
    const int seed_v[8] = { 6, 6, 5, 5, 4, 4, 3, 2 };
    for (int i = 0; i < 8; i++) st[i] = (Step){ (unsigned char)clamp(seed_p[i], P_LO, P_HI), 2, (unsigned char)seed_v[i], 15 };
}

void update(void) {
    if (keyp(KEY_SPACE)) {
        if (play_i < 0) { play_i = 0; play_t = 0; vis_t = 0; } else { play_i = -1; head = -1; vis_t = -1; }
    }
    if (keyp('E')) export_code();
    if (keyp('S')) save_it();
    if (keyp('L')) load_it();
    if (keyp(KEY_BACKSPACE)) { for (int i = 0; i < NSTEP; i++) st[i].vol = 0; msg = "cleared"; msg_t = 60; }
    if (msg_t > 0) msg_t--;

    // step playback — queue steps ~2 frames ahead with schedule_hit(), so the audio thread
    // fires them sample-accurately. Each queued step gets a ROTATING slot (10-15) carrying
    // its wave + per-step filter until it fires (defines apply immediately; hits are
    // delayed — six slots outlast the ~5-step queue). Same as the exported player.
    if (play_i >= 0) {
        play_t -= dt();
        while (play_i >= 0 && play_t < 0.034f) {
            if (st[play_i].vol) {
                int sl = 10 + (sched_k++ % 6);
                instrument(sl, st[play_i].wave, 5, 0, 7, 20);                       // raw-wave-style declick
                if (st[play_i].cut < 15) instrument_filter(sl, FILTER_LOW, cut_hz(st[play_i].cut), res);
                else                     instrument_filter(sl, FILTER_OFF, 1000, 0);
                schedule_hit(play_t > 0 ? (int)(play_t * 1000.0f) : 0,
                             st[play_i].pitch, sl, st[play_i].vol, spd_ms + 5);
            }
            play_t += spd_ms / 1000.0f;
            play_i = (play_i + 1 >= NSTEP) ? (loop_on ? 0 : -1) : play_i + 1;
        }
    }
    if (vis_t >= 0) {                                       // playhead runs on its own clock
        vis_t += dt();
        int s = (int)(vis_t * 1000.0f / spd_ms);
        if (loop_on) head = s % NSTEP;
        else if (s >= NSTEP) { head = -1; vis_t = -1; }
        else head = s;
    }

    // painting (skipped while a slider is held)
    if (ui_held == 0) {
        int i = step_at(mouse_x());
        if (i >= 0 && in_box(GX, GY, NSTEP * SW, GH)) {
            if (mouse_down(MOUSE_LEFT))  { st[i].pitch = (unsigned char)snap_midi(y_midi(mouse_y())); st[i].wave = (unsigned char)pen; if (!st[i].vol) st[i].vol = 5; }
            if (mouse_down(MOUSE_RIGHT))   st[i].vol = 0;
        }
        if (i >= 0 && in_box(GX, VY, NSTEP * SW, VH) && mouse_down(MOUSE_LEFT))
            st[i].vol = (unsigned char)clamp((VY + VH - mouse_y()) / 2.0f, 0, 7);
        if (i >= 0 && in_box(GX, CUY, NSTEP * SW, CUH) && mouse_down(MOUSE_LEFT))
            st[i].cut = (unsigned char)clamp((float)(CUY + CUH - 1 - mouse_y()) * 16.0f / CUH, 0, 15);
    }
}

static float slider(int id, int x, int y, int w, float val, float lo, float hi, int col) {
    if (mouse_pressed(MOUSE_LEFT) && in_box(x - 2, y - 3, w + 4, 11)) ui_held = id;
    if (ui_held == id) val = lo + clamp((float)(mouse_x() - x) / w, 0, 1) * (hi - lo);
    rectfill(x, y + 1, w, 2, CLR_DARK_GREY);
    rectfill(x + (int)((val - lo) / (hi - lo) * w) - 2, y - 2, 4, 8, col);
    return val;
}

void draw(void) {
    if (!mouse_down(MOUSE_LEFT)) ui_held = 0;
    cls(CLR_DARKER_BLUE);
    print("SFX EDITOR", 8, 4, CLR_WHITE);
    font(FONT_SMALL);
    if (msg_t > 0) print(msg, 96, 6, CLR_LIGHT_PEACH);
    else           print_right("draw a sound, export it as code", 312, 6, CLR_MAUVE);
    font(FONT_NORMAL);

    // ---- scale picker (L = next, R = previous; KEY transposes the root) ----
    font(FONT_SMALL);
    bool sc_hot = in_box(8, 13, 62, 10);
    rectfill(8, 13, 62, 10, scl ? CLR_DARK_GREEN : CLR_BLACK);
    rect(8, 13, 62, 10, sc_hot ? CLR_WHITE : CLR_DARK_GREY);
    print(str("SCALE:%s", SCLNAME[scl]), 12, 15, scl ? CLR_WHITE : CLR_MEDIUM_GREY);
    if (sc_hot && mouse_pressed(MOUSE_LEFT))  scl = (scl + 1) % NSCL;
    if (sc_hot && mouse_pressed(MOUSE_RIGHT)) scl = (scl + NSCL - 1) % NSCL;
    if (scl) {                                             // root only matters with a scale on
        bool k_hot = in_box(74, 13, 24, 10);
        rectfill(74, 13, 24, 10, CLR_BLACK);
        rect(74, 13, 24, 10, k_hot ? CLR_WHITE : CLR_DARK_GREY);
        print(str("KEY%s", NN[root]), 77, 15, CLR_LIGHT_PEACH);
        if (k_hot && mouse_pressed(MOUSE_LEFT))  root = (root + 1) % 12;
        if (k_hot && mouse_pressed(MOUSE_RIGHT)) root = (root + 11) % 12;
    }
    font(FONT_NORMAL);

    // ---- pitch grid ----
    rectfill(GX, GY, NSTEP * SW, GH, CLR_BROWNISH_BLACK);
    for (int m = P_LO; m <= P_HI; m += 12) {               // octave guides at each C
        int y = midi_y(m);
        for (int x = GX; x < GX + NSTEP * SW; x += 4) pset(x, y, CLR_DARKER_GREY);
        font(FONT_SMALL); print(str("C%d", m / 12 - 1), GX + NSTEP * SW + 3, y - 2, CLR_DARK_GREY); font(FONT_NORMAL);
    }
    if (scl)                                               // faint guides on in-scale rows
        for (int m = P_LO; m <= P_HI; m++)
            if (m % 12 != 0 && in_scale(m)) {
                int y = midi_y(m);
                for (int x = GX + 2; x < GX + NSTEP * SW; x += 8) pset(x, y, CLR_DARKER_GREY);
            }
    for (int i = 0; i < NSTEP; i++) {                      // the bars
        int x = GX + i * SW;
        if (i % 4 == 0) line(x, GY, x, GY + GH, CLR_DARKER_GREY);
        if (st[i].vol) {
            int py = midi_y(st[i].pitch);
            rectfill(x + 1, py, SW - 2, GY + GH - py, WCOL[st[i].wave]);
            rectfill(x + 1, py - 1, SW - 2, 2, CLR_WHITE);                  // the note "head"
        }
        if (i == head) rect(x, GY, SW, GH, CLR_WHITE);     // playhead
    }
    rect(GX, GY, NSTEP * SW, GH, CLR_DARK_GREY);

    // hovered note readout (snapped — shows the note a click would actually paint)
    if (in_box(GX, GY, NSTEP * SW, GH)) {
        int m = snap_midi(y_midi(mouse_y()));
        print_right(str("%s%d", NN[m % 12], m / 12 - 1), 312, 16, CLR_LIGHT_GREY);
    }

    // ---- volume lane ----
    rectfill(GX, VY, NSTEP * SW, VH, CLR_BROWNISH_BLACK);
    for (int i = 0; i < NSTEP; i++) {
        int x = GX + i * SW;
        if (st[i].vol) rectfill(x + 1, VY + VH - st[i].vol * 2, SW - 2, st[i].vol * 2, i == head ? CLR_WHITE : CLR_MEDIUM_GREEN);
    }
    rect(GX, VY, NSTEP * SW, VH, CLR_DARK_GREY);
    font(FONT_SMALL); print("VOL", GX + NSTEP * SW + 3, VY + 6, CLR_DARK_GREY); font(FONT_NORMAL);

    // ---- filter CUT lane (top = wide open / no filter, lower = darker) ----
    rectfill(GX, CUY, NSTEP * SW, CUH, CLR_BROWNISH_BLACK);
    for (int i = 0; i < NSTEP; i++) {
        int x = GX + i * SW;
        if (st[i].vol) {
            int h = st[i].cut + 1;                          // 1..16 px
            rectfill(x + 1, CUY + CUH - h, SW - 2, h,
                     i == head ? CLR_WHITE : st[i].cut == 15 ? CLR_DARK_ORANGE : CLR_ORANGE);
        }
    }
    rect(GX, CUY, NSTEP * SW, CUH, CLR_DARK_GREY);
    font(FONT_SMALL); print("CUT", GX + NSTEP * SW + 3, CUY + 6, CLR_DARK_GREY); font(FONT_NORMAL);

    // ---- controls ----
    for (int w = 0; w < 5; w++) {                          // wave pen
        int x = 8 + w * 38;
        rectfill(x, 156, 34, 13, pen == w ? WCOL[w] : CLR_BLACK);
        rect(x, 156, 34, 13, in_box(x, 156, 34, 13) ? CLR_WHITE : CLR_DARK_GREY);
        print(WNAME[w], x + 5, 159, pen == w ? CLR_BLACK : CLR_MEDIUM_GREY);
        if (in_box(x, 156, 34, 13) && mouse_pressed(MOUSE_LEFT)) pen = w;
    }
    bool lp_hot = in_box(200, 156, 44, 13);
    rectfill(200, 156, 44, 13, loop_on ? CLR_MEDIUM_GREEN : CLR_BLACK);
    rect(200, 156, 44, 13, lp_hot ? CLR_WHITE : CLR_DARK_GREY);
    print("LOOP", 206, 159, loop_on ? CLR_BLACK : CLR_MEDIUM_GREY);
    if (lp_hot && mouse_pressed(MOUSE_LEFT)) loop_on = !loop_on;
    bool pl_hot = in_box(250, 156, 62, 13);
    rectfill(250, 156, 62, 13, play_i >= 0 ? CLR_YELLOW : CLR_BLACK);
    rect(250, 156, 62, 13, pl_hot ? CLR_WHITE : CLR_DARK_GREY);
    print(play_i >= 0 ? "STOP" : "PLAY", 266, 159, play_i >= 0 ? CLR_BLACK : CLR_MEDIUM_GREY);
    if (pl_hot && mouse_pressed(MOUSE_LEFT)) { if (play_i < 0) { play_i = 0; play_t = 0; vis_t = 0; } else { play_i = -1; head = -1; vis_t = -1; } }

    print("SPD", 8, 176, CLR_MEDIUM_GREY);
    spd_ms = (int)slider(1, 34, 176, 92, (float)spd_ms, 8, 200, CLR_PINK);   // 8ms = true sfx speed
    print(str("%dms", spd_ms), 130, 176, CLR_LIGHT_GREY);
    print("RES", 170, 176, CLR_MEDIUM_GREY);
    res = (int)slider(2, 196, 176, 80, (float)res, 0, 15, CLR_ORANGE);       // resonance for filtered (CUT<15) steps
    print(str("%d", res), 282, 176, CLR_LIGHT_GREY);

    font(FONT_SMALL);
    print("L draw   R erase   CUT lane: top = open   SPACE play   E export   S/L save   BKSP clear", 8, 190, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
