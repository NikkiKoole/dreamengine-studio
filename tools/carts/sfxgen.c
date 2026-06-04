#include "studio.h"
#include <stdio.h>
#include <math.h>

// SFX GENERATOR — sliders, not steps (a dreamengine take on sfxp / bfxr). Three lanes —
// AMP, PITCH, WAVE — each with start/end-ish, hold, decay and an LFO; the 17 sliders
// GENERATE a 32-step sound, the colored curves show the result, and every change replays
// it. RANDOM for instant inspiration, MUTATE to refine what you almost like (the jam
// workflow: random until close, mutate until right). Z replays, arrows undo/redo,
// E exports the baked steps as the same paste-ready C code as the "sfx editor" cart.

#define NSTEP 32
enum { A_ATK, A_HOLD, A_DEC, A_LR, A_LD,
       P_ST, P_EN, P_HOLD, P_DEC, P_LR, P_LD,
       W_ST, W_EN, W_HOLD, W_DEC, W_LR, W_LD, NP };

static const char *PNAME[NP] = {
    "AMP ATTACK", "AMP HOLD", "AMP DECAY", "AMP LFO RATE", "AMP LFO DEPTH",
    "PITCH START", "PITCH END", "PITCH HOLD", "PITCH DECAY", "PITCH LFO RATE", "PITCH LFO DEPTH",
    "WAVE START", "WAVE END", "WAVE HOLD", "WAVE DECAY", "WAVE LFO RATE", "WAVE LFO DEPTH",
};
static float P[NP];
static int   spd_ms = 70;

// the baked sound (what plays + what exports) + the curves for drawing
static unsigned char SP[NSTEP], SWV[NSTEP], SV[NSTEP];
static float CG[NSTEP], CK[NSTEP], CB[NSTEP];
// wave lane ordered dull -> bright, so the slider sweeps timbre
static const int WMAP[5] = { INSTR_SINE, INSTR_TRI, INSTR_SQUARE, INSTR_SAW, INSTR_NOISE };

static int   play_i = -1, head = -1;
static float play_t = 0;
static int   ui_row = -1;          // slider row being dragged
static const char *msg = ""; static int msg_t = 0;

// undo/redo of whole param sets (mutate too far -> step back)
static float hist[24][NP]; static int hist_n = 0, hist_i = 0;

// slider rows: the whole row is the slider (sfxp style)
#define RX 8
#define RW 240
#define RY 16
#define RH 10

static float lfo_at(int d, float rate, float depth) {
    return sin_deg(d * rate * rate * rate * 120.0f) * powf(depth, 1.5f);
}

static void gen(void) {
    float x = 0, v = 1, y = 1;
    for (int d = 0; d < NSTEP; d++) {
        int atk = (int)(P[A_ATK] * 31);
        if (d <= atk) x = (d + 5) / (float)(atk + 5);                               // ramp up
        if (d > (P[A_ATK] + P[A_HOLD] * (1 - P[A_ATK])) * 31) x *= 1 - P[A_DEC] * P[A_DEC];
        float g = clamp(x * (1 - lfo_at(d, P[A_LR], P[A_LD])), 0, 0.999f);

        if (d > P[P_HOLD] * 31) v *= 1 - P[P_DEC] * P[P_DEC];                       // pitch: start decays toward end
        float k = clamp(v * (P[P_ST] - P[P_EN] + lfo_at(d, P[P_LR], P[P_LD])) + P[P_EN], 0, 0.999f);

        if (d > P[W_HOLD] * 31) y *= 1 - P[W_DEC] * P[W_DEC];                       // wave: same model, quantized
        float b = clamp(y * (P[W_ST] - P[W_EN] + lfo_at(d, P[W_LR], P[W_LD])) + P[W_EN], 0, 0.999f);

        CG[d] = g; CK[d] = k; CB[d] = b;
        SV[d]  = (unsigned char)(g * 7.99f);
        SP[d]  = (unsigned char)(36 + (int)(k * 60));        // MIDI 36..96
        SWV[d] = (unsigned char)WMAP[(int)(b * 5)];
    }
}

static void replay(void) { gen(); play_i = 0; play_t = 0; }

static void remember(void) {                                  // push current params for undo
    if (hist_i < 24) { for (int i = 0; i < NP; i++) hist[hist_i][i] = P[i]; hist_i++; if (hist_i > hist_n) hist_n = hist_i; }
    else { for (int h = 0; h < 23; h++) for (int i = 0; i < NP; i++) hist[h][i] = hist[h + 1][i];
           for (int i = 0; i < NP; i++) hist[23][i] = P[i]; }
}
static void randomize(bool mutate) {
    remember();
    for (int i = 0; i < NP; i++) {
        float r = rnd_float();
        P[i] = mutate ? 0.9f * P[i] + 0.1f * r : r;
    }
    if (!mutate) { P[A_ATK] *= P[A_ATK] * P[A_ATK]; P[A_HOLD] *= P[A_HOLD]; P[P_DEC] *= P[P_DEC]; P[W_HOLD] *= P[W_HOLD]; }  // bias short/snappy
    replay();
    msg = mutate ? "mutated" : "random!"; msg_t = 60;
}

// ---- save / export (same code format as the sfx editor cart) ----
typedef struct { int version, spd; float p[NP]; } SaveData;
static void save_it(void) { SaveData d = { 1, spd_ms, {0} }; for (int i = 0; i < NP; i++) d.p[i] = P[i]; save_bytes(&d, sizeof d); msg = "SAVED"; msg_t = 80; }
static void load_it(void) {
    SaveData d;
    if (load_bytes(&d, sizeof d) == (int)sizeof d && d.version == 1) { spd_ms = d.spd; for (int i = 0; i < NP; i++) P[i] = d.p[i]; replay(); msg = "LOADED"; }
    else msg = "no save";
    msg_t = 80;
}
static void export_array(const char *name, const unsigned char *a) {
    char b[512]; int n = snprintf(b, sizeof b, "static const unsigned char %s[%d] = {", name, NSTEP);
    for (int i = 0; i < NSTEP; i++) n += snprintf(b + n, sizeof b - n, "%d%s", a[i], i < NSTEP - 1 ? "," : "");
    snprintf(b + n, sizeof b - n, "};");
    printh("%s", b);
}
static void export_code(void) {
    gen();
    printh("// ---- sfx (from the sfx generator cart) — paste into your cart ----");
    export_array("SFX_P", SP); export_array("SFX_W", SWV); export_array("SFX_V", SV);
    printh("#define SFX_SPD %d   // ms per step", spd_ms);
    printh("static int sfx_i = -1; static float sfx_t;");
    printh("void sfx_start(void) { sfx_i = 0; sfx_t = 0; }   // call to play the sound");
    printh("void sfx_tick(void) {                            // call every frame in update()");
    printh("    if (sfx_i < 0) return;");
    printh("    sfx_t -= dt();");
    printh("    while (sfx_i >= 0 && sfx_t <= 0) {");
    printh("        if (SFX_V[sfx_i]) hit(SFX_P[sfx_i], SFX_W[sfx_i], SFX_V[sfx_i], SFX_SPD + 25);");
    printh("        sfx_t += SFX_SPD / 1000.0f;");
    printh("        if (++sfx_i >= %d) sfx_i = -1;", NSTEP);
    printh("    }");
    printh("}");
    msg = "EXPORTED to the log panel"; msg_t = 140;
}

void init(void) {
    // seed a classic descending laser-zap so the first Z is satisfying
    P[A_ATK] = 0.02f; P[A_HOLD] = 0.35f; P[A_DEC] = 0.35f;
    P[P_ST] = 0.85f; P[P_EN] = 0.25f; P[P_HOLD] = 0.08f; P[P_DEC] = 0.30f;
    P[W_ST] = 0.55f; P[W_EN] = 0.55f;
    gen();
}

void update(void) {
    if (keyp('Z') || keyp('X')) replay();
    if (keyp('R')) randomize(false);
    if (keyp('M')) randomize(true);
    if (keyp('E')) export_code();
    if (keyp('S')) save_it();
    if (keyp('L')) load_it();
    if (keyp(KEY_LEFT) && hist_i > 0) {                       // undo
        if (hist_i == hist_n) remember(), hist_i--;           // stash present so redo can return
        hist_i--; for (int i = 0; i < NP; i++) P[i] = hist[hist_i][i]; replay();
    }
    if (keyp(KEY_RIGHT) && hist_i < hist_n - 1) {             // redo
        hist_i++; for (int i = 0; i < NP; i++) P[i] = hist[hist_i][i]; replay();
    }
    if (msg_t > 0) msg_t--;

    // slider rows — press in a row, drag horizontally
    if (mouse_pressed(MOUSE_LEFT)) {
        int r = (mouse_y() - RY) / RH;
        if (mouse_x() >= RX - 2 && mouse_x() < RX + RW + 2 && r >= 0 && r < NP && mouse_y() >= RY) { remember(); ui_row = r; }
    }
    if (!mouse_down(MOUSE_LEFT)) { if (ui_row >= 0) replay(); ui_row = -1; }
    if (ui_row >= 0) { P[ui_row] = clamp((mouse_x() - RX) / (float)RW, 0, 1); gen(); }

    // playback — hit() per baked step, exactly what the exported player does
    play_t -= dt();
    while (play_i >= 0 && play_t <= 0) {
        if (SV[play_i]) hit(SP[play_i], SWV[play_i], SV[play_i], spd_ms + 25);
        head = play_i;
        play_t += spd_ms / 1000.0f;
        play_i = play_i + 1 >= NSTEP ? -1 : play_i + 1;
    }
    if (play_i < 0 && play_t < -0.3f) head = -1;
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("SFX GENERATOR", 8, 3, CLR_WHITE);
    font(FONT_SMALL);
    if (msg_t > 0) print(msg, 120, 5, CLR_LIGHT_PEACH);
    else           print_right("sliders make the sound", 312, 5, CLR_MAUVE);

    // 17 slider rows (the row IS the slider, sfxp style)
    for (int i = 0; i < NP; i++) {
        int y = RY + i * RH;
        bool hot = ui_row == i || (mouse_y() >= y && mouse_y() < y + RH && mouse_x() >= RX && mouse_x() < RX + RW);
        rectfill(RX, y + 1, RW, RH - 3, CLR_DARKER_BLUE);
        rectfill(RX, y + 1, (int)(P[i] * RW), RH - 3, hot ? CLR_BLUE : CLR_DARKER_PURPLE);
        print(PNAME[i], RX + 3, y + 2, hot ? CLR_WHITE : CLR_LIGHT_GREY);
    }

    // the generated curves, overlaid to the right: amp / pitch / wave per step
    int cx0 = RX + RW + 6, cw = 312 - cx0, cy0 = RY, ch = NP * RH - 4;
    rectfill(cx0, cy0, cw, ch, CLR_BLACK);
    rect(cx0, cy0, cw, ch, CLR_DARKER_GREY);
    for (int d = 0; d < NSTEP - 1; d++) {
        int xa = cx0 + d * cw / (NSTEP - 1), xb = cx0 + (d + 1) * cw / (NSTEP - 1);
        line(xa, cy0 + ch - (int)(CG[d] * ch), xb, cy0 + ch - (int)(CG[d + 1] * ch), CLR_MAUVE);
        line(xa, cy0 + ch - (int)(CK[d] * ch), xb, cy0 + ch - (int)(CK[d + 1] * ch), CLR_ORANGE);
        line(xa, cy0 + ch - (int)(CB[d] * ch), xb, cy0 + ch - (int)(CB[d + 1] * ch), CLR_GREEN);
    }
    if (head >= 0) { int hx = cx0 + head * cw / (NSTEP - 1); line(hx, cy0, hx, cy0 + ch, CLR_WHITE); }
    print("amp", cx0 + 3, cy0 + 2, CLR_MAUVE);
    print("pitch", cx0 + 3, cy0 + 9, CLR_ORANGE);
    print("wave", cx0 + 3, cy0 + 16, CLR_GREEN);

    // bottom bar: buttons + speed
    int by = RY + NP * RH + 3;
    const char *bn[4] = { "PLAY (Z)", "RANDOM (R)", "MUTATE (M)", str("SPD %dms", spd_ms) };
    for (int b = 0; b < 4; b++) {
        int x = 8 + b * 78;
        bool hot = mouse_x() >= x && mouse_x() < x + 72 && mouse_y() >= by && mouse_y() < by + 11;
        rectfill(x, by, 72, 11, CLR_DARKER_BLUE);
        rect(x, by, 72, 11, hot ? CLR_WHITE : CLR_DARK_GREY);
        print(bn[b], x + 5, by + 3, CLR_LIGHT_GREY);
        if (hot && mouse_pressed(MOUSE_LEFT)) {
            if (b == 0) replay();
            if (b == 1) randomize(false);
            if (b == 2) randomize(true);
            if (b == 3) { spd_ms = spd_ms >= 130 ? 40 : spd_ms + 30; replay(); }
        }
        if (b == 1 && hot && mouse_pressed(MOUSE_RIGHT)) randomize(true);   // sfxp tradition: rclick random = mutate
    }
    print("arrows undo/redo   E export code   S/L save/load", 8, by + 14, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}
