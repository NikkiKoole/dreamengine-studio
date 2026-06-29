/* de:meta
{
  "title": "sfx generator",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [
    "adsr-envelope",
    "step-sequencer"
  ],
  "lineage": "Dreamengine's take on sfxp/bfxr — three parametric lanes (amp/pitch/wave) each with hold/decay/LFO bake down to a 32-step sound, adding mutation and undo on top of the bfxr model.",
  "description": "Sliders make the sound — a dreamengine take on sfxp/bfxr. Three lanes (AMP, PITCH, WAVE), each with hold/decay and an LFO: 17 sliders GENERATE a 32-step sound, the colored curves show the result, and every change replays it. RANDOM for instant inspiration, MUTATE to refine what you almost like (the game-jam workflow), arrows undo/redo, Z replays. Press E to export the baked steps as paste-ready C code — the same format as the sfx editor cart, so either feeds your game the same way. S/L save and load."
}
de:meta */
#include "studio.h"
#include "ui.h"
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
// step speeds in ms. Game sfx are FAST — sfxp's default is ~8ms/step (a 32-step sound is a
// quarter-second zap). Steps are queued ahead with schedule_hit(), so the audio thread fires
// them sample-accurately — sub-frame speeds stay crisp. 8-22 = sfx land, 45+ = jingle land.
static const int SPDS[8] = { 8, 12, 16, 22, 30, 45, 70, 100 };
#define NSPD 8
static int spd_i = 1;
#define SPD_MS (SPDS[spd_i])

// the baked sound (what plays + what exports) + the curves for drawing
static unsigned char SP[NSTEP], SWV[NSTEP], SV[NSTEP];
static float CG[NSTEP], CK[NSTEP], CB[NSTEP];
// wave lane ordered dull -> bright, so the slider sweeps timbre
static const int WMAP[5] = { INSTR_SINE, INSTR_TRI, INSTR_SQUARE, INSTR_SAW, INSTR_NOISE };

static int   play_i = -1, head = -1;
static float play_t = 0;          // time until the next UNSCHEDULED step (the queue-ahead clock)
static float vis_t = -1;          // visual clock for the playhead; <0 = stopped
static const char *msg = ""; static int msg_t = 0;

// undo/redo of whole param sets (mutate too far -> step back)
static float hist[24][NP]; static int hist_n = 0, hist_i = 0;

// slider rows: the whole row is the slider (sfxp style)
#define RX 8
#define RW 240
#define RY 16
#define RH 9

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

static void replay(void) { gen(); play_i = 0; play_t = 0; vis_t = 0; }

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
    if (!mutate) { P[A_ATK] *= P[A_ATK] * P[A_ATK]; P[A_HOLD] *= P[A_HOLD]; P[P_DEC] *= P[P_DEC]; P[W_HOLD] *= P[W_HOLD];  // bias short/snappy
                   spd_i = rnd(4); }                                                                                        // 8/12/16/22ms — sfx land (sfxp rolls spd too)
    replay();
    msg = mutate ? "mutated" : "random!"; msg_t = 60;
}

// ---- sfxr-style category presets — each click rolls a FRESH random variant in-category ----
static const char *CATN[7] = { "COIN", "SHOT", "BOOM", "PWUP", "HURT", "JUMP", "BLIP" };
static float rr(float a, float b) { return rnd_float_between(a, b); }
static void category(int c) {
    remember();
    for (int i = 0; i < NP; i++) P[i] = 0;
    spd_i = rnd(2);                                            // 8/12ms — sfx speed
    switch (c) {
        case 0:  // COIN — a blip that jumps UP and rings
            P[A_HOLD] = rr(.3f, .45f); P[A_DEC] = rr(.18f, .3f);
            P[P_ST] = rr(.45f, .6f); P[P_EN] = P[P_ST] + rr(.15f, .3f);
            P[P_HOLD] = rr(.12f, .22f); P[P_DEC] = rr(.7f, .9f);
            P[W_ST] = P[W_EN] = rr(.42f, .58f);
            break;
        case 1:  // SHOT — descending laser
            P[A_HOLD] = rr(.2f, .35f); P[A_DEC] = rr(.3f, .5f);
            P[P_ST] = rr(.7f, .95f); P[P_EN] = rr(.15f, .35f); P[P_HOLD] = rr(0, .08f); P[P_DEC] = rr(.3f, .5f);
            P[W_ST] = P[W_EN] = rr(.45f, .75f);
            break;
        case 2:  // BOOM — noise rumble, longer
            P[A_ATK] = rr(0, .06f); P[A_HOLD] = rr(.3f, .5f); P[A_DEC] = rr(.1f, .25f);
            P[A_LR] = rr(.3f, .6f); P[A_LD] = rr(.2f, .5f);
            P[P_ST] = rr(.2f, .45f); P[P_EN] = rr(.05f, .2f); P[P_DEC] = rr(.2f, .4f);
            P[W_ST] = rr(.85f, .99f); P[W_EN] = rr(.85f, .99f);
            spd_i = rnd(2) + 2;                                // 16/22ms
            break;
        case 3:  // PWUP — rising warble
            P[A_HOLD] = rr(.5f, .7f); P[A_DEC] = rr(.2f, .35f);
            P[P_ST] = rr(.25f, .4f); P[P_EN] = rr(.65f, .85f); P[P_DEC] = rr(.18f, .32f);
            P[P_LR] = rr(.35f, .55f); P[P_LD] = rr(.1f, .25f);
            P[W_ST] = P[W_EN] = rr(.3f, .55f);
            spd_i = rnd(2) + 1;                                // 12/16ms
            break;
        case 4:  // HURT — short harsh drop
            P[A_HOLD] = rr(.08f, .2f); P[A_DEC] = rr(.45f, .65f);
            P[P_ST] = rr(.4f, .6f); P[P_EN] = rr(.12f, .3f); P[P_DEC] = rr(.5f, .8f);
            P[W_ST] = P[W_EN] = rr(.6f, .92f);
            break;
        case 5:  // JUMP — clean rise
            P[A_HOLD] = rr(.3f, .45f); P[A_DEC] = rr(.3f, .45f);
            P[P_ST] = rr(.32f, .48f); P[P_EN] = rr(.58f, .75f); P[P_DEC] = rr(.25f, .4f);
            P[W_ST] = P[W_EN] = rr(.28f, .5f);
            break;
        case 6:  // BLIP — UI tick
            P[A_HOLD] = rr(.06f, .14f); P[A_DEC] = rr(.75f, .95f);
            P[P_ST] = rr(.5f, .72f); P[P_EN] = P[P_ST];
            P[W_ST] = P[W_EN] = rr(.25f, .55f);
            break;
    }
    replay();
    msg = CATN[c]; msg_t = 50;
}

// ---- save / export (same code format as the sfx editor cart) ----
typedef struct { int version, spd; float p[NP]; } SaveData;
static void save_it(void) { SaveData d = { 1, SPD_MS, {0} }; for (int i = 0; i < NP; i++) d.p[i] = P[i]; save_bytes(&d, sizeof d); msg = "SAVED"; msg_t = 80; }
static void load_it(void) {
    SaveData d;
    if (load_bytes(&d, sizeof d) == (int)sizeof d && d.version == 1) {
        spd_i = 0; for (int i = 1; i < 6; i++) if (SPDS[i] <= d.spd) spd_i = i;   // nearest saved speed
        for (int i = 0; i < NP; i++) P[i] = d.p[i]; replay(); msg = "LOADED"; }
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
    printh("#define SFX_SPD %d   // ms per step", SPD_MS);
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
    for (int c = 0; c < 7; c++) if (keyp('1' + c)) category(c);
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

    // (sliders + buttons live in draw() now — ui.h widgets; this cart is
    // the header's second customer, ui-widgets-notes.md §5)

    // playback — queue steps ~2 frames ahead with schedule_hit(), so the AUDIO thread fires
    // them sample-accurately: 8ms steps stay crisp instead of colliding in the 60fps frame
    // loop. Exactly what the exported player does.
    if (play_i >= 0) {
        play_t -= dt();
        while (play_i >= 0 && play_t < 0.034f) {
            if (SV[play_i]) schedule_hit(play_t > 0 ? (int)(play_t * 1000.0f) : 0,
                                         SP[play_i], SWV[play_i], SV[play_i], SPD_MS + 5);
            play_t += SPD_MS / 1000.0f;
            play_i = play_i + 1 >= NSTEP ? -1 : play_i + 1;
        }
    }
    if (vis_t >= 0) {                                       // playhead runs on its own clock
        vis_t += dt();
        head = (int)(vis_t * 1000.0f / SPD_MS);
        if (head >= NSTEP) { head = -1; vis_t = -1; }
    }

#ifdef DE_TRACE
    watch("pst", "%.2f", P[P_ST]);   // pitch-start slider (retrofit test target)
    watch("spd_i", "%d", spd_i);
    watch("hist", "%d", hist_i);
    watch("caps", "%d", ui_cap_n);
    watch("playing", "%d", play_i >= 0 ? 1 : 0);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();
    print("SFX GENERATOR", 8, 3, CLR_WHITE);
    font(FONT_SMALL);
    if (msg_t > 0) print(msg, 120, 5, CLR_LIGHT_PEACH);
    else           print_right("arrows undo  E export  S/L save", 312, 5, CLR_MAUVE);

    // 17 slider rows (the row IS the slider, sfxp style) — ui.h widgets:
    // grab = undo point (fires before the press-jump lands), drag regenerates,
    // release replays. Multi-finger: every finger rides its own row.
    for (int i = 0; i < NP; i++) {
        if (ui_grabbed(&P[i])) remember();
        if (ui_slider(&P[i], RX, RY + i * RH, RW, PNAME[i])) gen();
        if (ui_released(&P[i])) replay();
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

    // sfxr-style category row — each click = a fresh variant (keys 1-7)
    int py2 = RY + NP * RH + 2;
    for (int c = 0; c < 7; c++)
        if (ui_button(8 + c * 43, py2, 41, 10, CATN[c])) category(c);

    // main bar: play / random / mutate / speed
    int by = py2 + 12;
    const char *bn[4] = { "PLAY (Z)", "RANDOM (R)", "MUTATE (M)", str("SPD %dms", SPD_MS) };
    for (int b = 0; b < 4; b++) {
        int x = 8 + b * 78;
        if (ui_button(x, by, 72, 11, bn[b])) {
            if (b == 0) replay();
            if (b == 1) randomize(false);
            if (b == 2) randomize(true);
            if (b == 3) { spd_i = (spd_i + 1) % NSPD; replay(); }
        }
        if (b == 1 && ui_hover(x, by, 72, 11) && mouse_pressed(MOUSE_RIGHT))
            randomize(true);   // sfxp tradition: rclick random = mutate
    }
    font(FONT_NORMAL);
    ui_end();
}
