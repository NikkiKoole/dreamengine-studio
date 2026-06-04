#include "studio.h"
#include <stdio.h>

// SFX EDITOR — draw a sound, PICO-8 style. Paint a pitch contour over 32 steps with the
// mouse (left = draw with the selected wave, right = erase), shape the volume lane below,
// pick a wave "pen", set the step speed, then SPACE to hear it. E exports the sound as
// C code (a step array + a tiny player) to the log — paste it straight into your game.
// S/L save/load inside this cart. No engine SFX banks involved: playback is just hit()
// fired per step, which is all your game needs too. (The prototype for audio-notes §5.6.)

#define NSTEP 32
#define P_LO  36          // C2
#define P_HI  84          // C6

// pitch grid + volume lane geometry
#define GX 8
#define SW 9              // px per step  (32 * 9 = 288)
#define GY 26
#define GH 100
#define VY 132
#define VH 24             // 8 vol levels * 3px

typedef struct { unsigned char pitch, wave, vol; } Step;   // vol 0 = silent step
static Step st[NSTEP];

static int   pen = 2;             // wave the mouse paints with (0..4 = SQR SAW TRI NOI SIN)
static int   spd_ms = 80;         // step length
static bool  loop_on = false;

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
typedef struct { int version, spd, loop; Step st[NSTEP]; } SaveData;
static void save_it(void) {
    SaveData d = { 1, spd_ms, loop_on, {{0}} };
    for (int i = 0; i < NSTEP; i++) d.st[i] = st[i];
    save_bytes(&d, sizeof d);
    msg = "SAVED"; msg_t = 80;
}
static void load_it(void) {
    SaveData d;
    if (load_bytes(&d, sizeof d) == (int)sizeof d && d.version == 1) {
        spd_ms = d.spd; loop_on = d.loop;
        for (int i = 0; i < NSTEP; i++) st[i] = d.st[i];
        msg = "LOADED";
    } else msg = "no save";
    msg_t = 80;
}

// ---- export: the sound as paste-into-your-cart C code, printed to the log ----
static void export_array(const char *name, int field) {   // field 0=pitch 1=wave 2=vol
    char b[512]; int n = snprintf(b, sizeof b, "static const unsigned char %s[%d] = {", name, NSTEP);
    for (int i = 0; i < NSTEP; i++) {
        int v = field == 0 ? (st[i].vol ? st[i].pitch : 0) : field == 1 ? st[i].wave : st[i].vol;
        n += snprintf(b + n, sizeof b - n, "%d%s", v, i < NSTEP - 1 ? "," : "");
    }
    snprintf(b + n, sizeof b - n, "};");
    printh("%s", b);
}
static void export_code(void) {
    printh("// ---- sfx (drawn in the sfx editor cart) — paste into your cart ----");
    export_array("SFX_P", 0);
    export_array("SFX_W", 1);
    export_array("SFX_V", 2);
    printh("#define SFX_SPD %d   // ms per step", spd_ms);
    printh("static int sfx_i = -1; static float sfx_t;");
    printh("void sfx_start(void) { sfx_i = 0; sfx_t = 0; }   // call to play the sound");
    printh("void sfx_tick(void) {                            // call every frame in update()");
    printh("    if (sfx_i < 0) return;");
    printh("    sfx_t -= dt();");
    printh("    while (sfx_i >= 0 && sfx_t < 0.034f) {       // queue ~2 frames ahead, sample-accurate");
    printh("        if (SFX_V[sfx_i]) schedule_hit(sfx_t > 0 ? (int)(sfx_t * 1000.0f) : 0,");
    printh("                                       SFX_P[sfx_i], SFX_W[sfx_i], SFX_V[sfx_i], SFX_SPD + 5);");
    printh("        sfx_t += SFX_SPD / 1000.0f;");
    printh("        if (++sfx_i >= %d) sfx_i = -1;", NSTEP);
    printh("    }");
    printh("}");
    msg = "EXPORTED to the log panel"; msg_t = 140;
}

void init(void) {
    // seed a little pickup-coin arp so SPACE makes a sound immediately
    const int seed_p[8] = { 64, 69, 72, 76, 81, 84, 88, 88 };
    const int seed_v[8] = { 6, 6, 5, 5, 4, 4, 3, 2 };
    for (int i = 0; i < 8; i++) st[i] = (Step){ (unsigned char)clamp(seed_p[i], P_LO, P_HI), 2, (unsigned char)seed_v[i] };
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
    // fires them sample-accurately (sub-frame speeds stay crisp). Same as the exported player.
    if (play_i >= 0) {
        play_t -= dt();
        while (play_i >= 0 && play_t < 0.034f) {
            if (st[play_i].vol) schedule_hit(play_t > 0 ? (int)(play_t * 1000.0f) : 0,
                                             st[play_i].pitch, st[play_i].wave, st[play_i].vol, spd_ms + 5);
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
            if (mouse_down(MOUSE_LEFT))  { st[i].pitch = (unsigned char)y_midi(mouse_y()); st[i].wave = (unsigned char)pen; if (!st[i].vol) st[i].vol = 5; }
            if (mouse_down(MOUSE_RIGHT))   st[i].vol = 0;
        }
        if (i >= 0 && in_box(GX, VY, NSTEP * SW, VH) && mouse_down(MOUSE_LEFT))
            st[i].vol = (unsigned char)clamp((VY + VH - mouse_y()) / 3.0f, 0, 7);
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

    // ---- pitch grid ----
    rectfill(GX, GY, NSTEP * SW, GH, CLR_BROWNISH_BLACK);
    for (int m = P_LO; m <= P_HI; m += 12) {               // octave guides at each C
        int y = midi_y(m);
        for (int x = GX; x < GX + NSTEP * SW; x += 4) pset(x, y, CLR_DARKER_GREY);
        font(FONT_SMALL); print(str("C%d", m / 12 - 1), GX + NSTEP * SW + 3, y - 2, CLR_DARK_GREY); font(FONT_NORMAL);
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

    // hovered note readout
    if (in_box(GX, GY, NSTEP * SW, GH)) {
        int m = y_midi(mouse_y());
        print_right(str("%s%d", NN[m % 12], m / 12 - 1), 312, 16, CLR_LIGHT_GREY);
    }

    // ---- volume lane ----
    rectfill(GX, VY, NSTEP * SW, VH, CLR_BROWNISH_BLACK);
    for (int i = 0; i < NSTEP; i++) {
        int x = GX + i * SW;
        if (st[i].vol) rectfill(x + 1, VY + VH - st[i].vol * 3, SW - 2, st[i].vol * 3, i == head ? CLR_WHITE : CLR_MEDIUM_GREEN);
    }
    rect(GX, VY, NSTEP * SW, VH, CLR_DARK_GREY);
    font(FONT_SMALL); print("VOL", GX + NSTEP * SW + 3, VY + 8, CLR_DARK_GREY); font(FONT_NORMAL);

    // ---- controls ----
    for (int w = 0; w < 5; w++) {                          // wave pen
        int x = 8 + w * 38;
        rectfill(x, 162, 34, 13, pen == w ? WCOL[w] : CLR_BLACK);
        rect(x, 162, 34, 13, in_box(x, 162, 34, 13) ? CLR_WHITE : CLR_DARK_GREY);
        print(WNAME[w], x + 5, 165, pen == w ? CLR_BLACK : CLR_MEDIUM_GREY);
        if (in_box(x, 162, 34, 13) && mouse_pressed(MOUSE_LEFT)) pen = w;
    }
    bool lp_hot = in_box(200, 162, 44, 13);
    rectfill(200, 162, 44, 13, loop_on ? CLR_MEDIUM_GREEN : CLR_BLACK);
    rect(200, 162, 44, 13, lp_hot ? CLR_WHITE : CLR_DARK_GREY);
    print("LOOP", 206, 165, loop_on ? CLR_BLACK : CLR_MEDIUM_GREY);
    if (lp_hot && mouse_pressed(MOUSE_LEFT)) loop_on = !loop_on;
    bool pl_hot = in_box(250, 162, 62, 13);
    rectfill(250, 162, 62, 13, play_i >= 0 ? CLR_YELLOW : CLR_BLACK);
    rect(250, 162, 62, 13, pl_hot ? CLR_WHITE : CLR_DARK_GREY);
    print(play_i >= 0 ? "STOP" : "PLAY", 266, 165, play_i >= 0 ? CLR_BLACK : CLR_MEDIUM_GREY);
    if (pl_hot && mouse_pressed(MOUSE_LEFT)) { if (play_i < 0) { play_i = 0; play_t = 0; } else { play_i = -1; head = -1; } }

    print("SPD", 8, 180, CLR_MEDIUM_GREY);
    spd_ms = (int)slider(1, 36, 180, 120, (float)spd_ms, 8, 200, CLR_PINK);   // 8ms = true sfx speed (schedule_hit keeps it crisp)
    print(str("%dms", spd_ms), 162, 180, CLR_LIGHT_GREY);

    font(FONT_SMALL);
    print("L draw   R erase   SPACE play   E export code   S/L save/load   BKSP clear", 8, 193, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
