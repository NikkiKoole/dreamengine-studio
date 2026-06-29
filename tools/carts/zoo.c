/* de:meta
{
  "title": "pixel zoo",
  "status": "active",
  "kind": [
    "toy",
    "tech-demo"
  ],
  "teaches": [
    "dithering-gradient",
    "adsr-envelope",
    "no-sprite-vehicles"
  ],
  "lineage": "The editor's welcome scene — animals drawn from primitives (the rectangular/no-sprite-art idiom), pairing fillp() dither gradients with synth ADSR pads and a lowpass-swept roar.",
  "description": "The editor's welcome scene — stroll a little zoo and visit the animals. Showcases fillp() dithered fills (the gradient sky, speckled grass, see-through grid fences), spr_rot()/sspr_ex() (a spinning sun, a pinwheel, a vendor's swaying balloons), and the synth: a soft ADSR pad, vibrato birds, and a lowpass-swept lion roar. Each enclosure answers as you walk up to it — water blips at the flamingo pond, the lion roars, the monkeys chatter, the elephant trumpets. The animals are drawn from primitives (ovals, circles, triangles). WASD walk, Z wave."
}
de:meta */
#include "studio.h"

// ============================================================
//  PIXEL ZOO  — a stroll through a little zoo.
//  Showcases: fillp() dither (the sky gradient + grass + fences),
//  spr_rot()/sspr_ex() (the spinning sun, pinwheel, swaying balloons),
//  and the synth (ADSR pad, vibrato birds, filter-swept lion roar).
//  The animals are drawn from primitives — no sprite art needed for them.
// ============================================================

#define WORLD_W (SCREEN_W * 2)   // world is twice as wide as the screen
#define GY 150                   // ground baseline — where animal feet rest

int   px   = 210;                // visitor position (world space)
int   py   = 164;
int   face = 1;                  // 1 = right, -1 = left
bool  roaring = false;           // lion is mid-roar (drives the open mouth)
float roar_t  = -9;

// ------------------------------------------------------------
//  animals — built from ovals, circles and triangles
// ------------------------------------------------------------

void flamingo(int x) {
    line(x,     GY, x,     GY - 12, CLR_ORANGE);          // legs
    line(x + 4, GY, x + 4, GY - 12, CLR_ORANGE);
    ovalfill(x + 2, GY - 15, 7, 4, CLR_PINK);             // body
    line(x + 4, GY - 17, x + 8, GY - 25, CLR_PINK);       // neck
    circfill(x + 9, GY - 26, 2, CLR_PINK);                // head
    trifill(x + 11, GY - 26, x + 14, GY - 25, x + 11, GY - 24, CLR_DARK_ORANGE); // beak
    pset(x + 9, GY - 27, CLR_BLACK);                      // eye
}

void lion(int cx) {
    ovalfill(cx + 10, GY - 9, 13, 7, CLR_ORANGE);         // body
    rectfill(cx + 3,  GY - 8, 3, 8, CLR_DARK_ORANGE);     // legs
    rectfill(cx + 17, GY - 8, 3, 8, CLR_DARK_ORANGE);
    line(cx + 22, GY - 12, cx + 28, GY - 18, CLR_DARK_ORANGE); // tail
    circfill(cx + 28, GY - 18, 2, CLR_BROWN);
    for (int a = 0; a < 360; a += 36)                     // mane
        circfill(cx - 6 + (int)dx(8, a), GY - 20 + (int)dy(8, a), 4, CLR_BROWN);
    circfill(cx - 6, GY - 20, 7, CLR_ORANGE);             // head
    circfill(cx - 11, GY - 25, 2, CLR_BROWN);             // ears
    circfill(cx - 1,  GY - 25, 2, CLR_BROWN);
    pset(cx - 9, GY - 21, CLR_BLACK);                     // eyes
    pset(cx - 3, GY - 21, CLR_BLACK);
    trifill(cx - 8, GY - 18, cx - 4, GY - 18, cx - 6, GY - 16, CLR_DARK_RED); // nose
    if (roaring) ovalfill(cx - 6, GY - 13, 3, 3, CLR_DARK_RED); // roar!
}

void palm(int cx) {
    rectfill(cx - 2, GY - 42, 5, 42, CLR_BROWN);          // trunk
    int ty = GY - 42;
    for (int a = -72; a <= 72; a += 24) {                 // fronds fan upward
        int d = 270 + a;
        trifill(cx, ty,
                cx + (int)dx(28, d),     ty + (int)dy(28, d),
                cx + (int)dx(26, d + 9), ty + (int)dy(26, d + 9), CLR_DARK_GREEN);
    }
    circfill(cx + 4, ty + 2, 2, CLR_BROWN);               // coconut
}

void monkey(int cx, float t) {
    int sw = (int)(sin_deg(t) * 8);
    int mx = cx + 16 + sw, my = GY - 24;
    line(cx + 6, GY - 44, mx, my - 4, CLR_DARK_GREEN);    // vine
    line(mx, my - 2, cx + 6, GY - 44, CLR_BROWN);         // arm to vine
    ovalfill(mx, my + 4, 5, 7, CLR_BROWN);                // body
    line(mx, my + 11, mx + 6, my + 5, CLR_BROWN);         // tail
    circfill(mx, my - 4, 5, CLR_BROWN);                   // head
    ovalfill(mx, my - 3, 3, 3, CLR_DARK_ORANGE);          // face
    pset(mx - 2, my - 5, CLR_BLACK);
    pset(mx + 2, my - 5, CLR_BLACK);
}

void elephant(int cx) {
    ovalfill(cx + 6, GY - 13, 16, 11, CLR_LIGHT_GREY);    // body
    rectfill(cx - 4, GY - 9, 5, 9, CLR_MEDIUM_GREY);      // legs
    rectfill(cx + 6, GY - 9, 5, 9, CLR_MEDIUM_GREY);
    rectfill(cx + 16, GY - 9, 5, 9, CLR_MEDIUM_GREY);
    line(cx + 22, GY - 16, cx + 26, GY - 10, CLR_MEDIUM_GREY); // tail
    circfill(cx - 10, GY - 18, 9, CLR_LIGHT_GREY);        // head
    ovalfill(cx - 16, GY - 16, 5, 7, CLR_MEDIUM_GREY);    // ear
    for (int i = 0; i < 8; i++)                           // trunk
        circfill(cx - 16, GY - 22 + i * 3, i > 5 ? 2 : 3, CLR_LIGHT_GREY);
    line(cx - 14, GY - 12, cx - 18, GY - 8, CLR_WHITE);   // tusk
    pset(cx - 10, GY - 20, CLR_BLACK);                    // eye
}

void pond(int cx) {
    fillp(FILL_HLINES, CLR_BLUE);                         // rippled water
    ovalfill(cx, GY - 3, 42, 9, CLR_TRUE_BLUE);
    fillp_reset();
    flamingo(cx - 16);
    flamingo(cx + 6);
}

// ------------------------------------------------------------
//  enclosure dressing — a grid fence + a name plaque
// ------------------------------------------------------------

void fence(int cx) {
    fillp(FILL_GRID, -1);                                 // see-through grid railing
    rectfill(cx - 56, GY + 2, 112, 11, CLR_LIGHT_GREY);
    fillp_reset();
    line(cx - 56, GY + 2, cx + 56, GY + 2, CLR_WHITE);
}

void plaque(const char *s, int cx) {
    int w = text_width(s);
    rectfill(cx - w / 2 - 2, 166, w + 4, 9, CLR_DARK_PURPLE);
    print(s, cx - w / 2, 167, CLR_WHITE);
}

// ------------------------------------------------------------

void init() {
    colorkey(CLR_BLACK);                                  // sprite black = transparent

    instrument(5, INSTR_TRI,   250, 120, 4, 700);         // soft pad (slow attack, long tail)
    instrument(6, INSTR_SINE,    4,  30, 2,  90);         // bird
    instrument_lfo(6, 0, LFO_PITCH, 6, 0.6f);             //   …with a chirpy vibrato
    instrument(7, INSTR_SAW,    30, 250, 5, 500);         // lion
    instrument_filter(7, FILTER_LOW, 500, 11);            //   …warm, behind a lowpass
    instrument_lfo(7, 0, LFO_CUTOFF, 2, 600);             //   …that the LFO sweeps open (roar)
    lfo_shape(7, 0, LFO_SHAPE_RANDOM);   // organic drift, not a mechanical sine (STATUS #39)
    instrument(8, INSTR_SQUARE,  2,  50, 0,  50);         // water blip
    instrument_duty(8, 0.18f);                            //   …thin nasal pulse
}

void update() {
    bpm(56);

    int mvx = 0, mvy = 0;
    if (btn(0, BTN_RIGHT)) mvx =  2;
    if (btn(0, BTN_LEFT))  mvx = -2;
    if (btn(0, BTN_UP))    mvy = -1;
    if (btn(0, BTN_DOWN))  mvy =  1;
    px += mvx; py += mvy;
    if (mvx) face = sgn(mvx);
    px = mid(0, px, WORLD_W - 16);
    py = mid(150, py, 168);

    int hx = px + 8, hy = py + 8;   // visitor centre, for proximity

    // gentle backing pad — a slow chord that breathes (custom ADSR instrument 5)
    int prog[] = { 48, 55, 53, 57 };
    if (every(8)) strum(prog[(beat() / 8) % 4], CHORD_MAJ7, 5, 3, 100);

    // birds chirp anywhere — pentatonic so it's always pretty
    if (every(1) && chance(22)) note(degree(SCALE_PENTA, 5, rnd(6)), 6, 2);

    // each enclosure answers when you walk up to it
    roaring = false;
    if (near(hx, hy, 140, GY, 60) && every(2) && chance(60))      // pond: water blips
        note(degree(SCALE_PENTA, 6, rnd(4)), 8, 2);
    if (near(hx, hy, 290, GY, 55) && every(4)) {                  // lion: filter-swept roar
        note(31, 7, 6); roar_t = now();
    }
    if (now() - roar_t < 0.6f) roaring = true;
    if (near(hx, hy, 430, GY, 55) && every(2) && chance(50))      // monkey: chatter
        hit(degree(SCALE_PENTA, 7, rnd(5)), INSTR_SQUARE, 3, 40);
    if (near(hx, hy, 560, GY, 55) && every(4))                    // elephant: trumpet
        note(48, INSTR_SAW, 4);

    if (btnp(0, BTN_A)) note(degree(SCALE_PENTA, 6, rnd(6)), 6, 4);
}

void draw() {
    // --- sky (screen space) — dithered 3-step gradient ---
    camera(0, 0);
    cls(CLR_BLUE);
    fillp(FILL_CHECKER, -1);
    rectfill(0, 0, SCREEN_W, 60, CLR_TRUE_BLUE);   // dithered band → gradient
    fillp_reset();
    rectfill(0, 0, SCREEN_W, 28, CLR_TRUE_BLUE);   // solid darker at the very top

    // drifting clouds
    for (int i = 0; i < 3; i++) {
        int cx = ((int)(now() * 6) + i * 130) % (SCREEN_W + 120) - 60;
        int cy = 66 + i * 9;
        ovalfill(cx, cy, 18, 7, CLR_WHITE);
        ovalfill(cx + 14, cy + 3, 12, 5, CLR_WHITE);
    }
    // sun with slowly turning rays — sspr_ex scales the 16x16 sun up to 30px and spins it
    sspr_ex(48, 0, 16, 16, 24, 16, 30, 30, now() * 16, 15, 15);
    // birds flapping across the sky (two little strokes)
    for (int i = 0; i < 3; i++) {
        int bx = ((int)(now() * 22) + i * 120) % (SCREEN_W + 40) - 20;
        int by = 40 + i * 11 + (int)(sin_deg(now() * 200 + i * 90) * 2);
        line(bx, by, bx + 3, by - 2, CLR_BLACK);
        line(bx + 3, by - 2, bx + 6, by, CLR_BLACK);
    }

    // --- world (camera follows the visitor) ---
    int camX = mid(0, px - SCREEN_W / 2, WORLD_W - SCREEN_W);
    camera(camX, 0);

    // rolling hills on the horizon (dithered green)
    fillp(FILL_CHECKER, -1);
    for (int i = 0; i < 6; i++) ovalfill(i * 130, 132, 80, 46, CLR_DARK_GREEN);
    fillp_reset();

    // grass, speckled lighter green
    rectfill(0, 128, WORLD_W, SCREEN_H - 128, CLR_DARK_GREEN);
    fillp(FILL_DOTS, -1);
    rectfill(0, 128, WORLD_W, SCREEN_H - 128, CLR_GREEN);
    fillp_reset();
    // the path the visitor walks
    fillp(FILL_CHECKER, CLR_BROWN);
    rectfill(0, 176, WORLD_W, SCREEN_H - 176, CLR_MEDIUM_GREY);
    fillp_reset();

    // entrance arch
    rectfill(20, 96, 4, 56, CLR_DARK_BROWN);
    rectfill(92, 96, 4, 56, CLR_DARK_BROWN);
    rectfill(14, 84, 88, 16, CLR_DARK_RED);
    rect(14, 84, 88, 16, CLR_YELLOW);
    print("THE ZOO", 30, 89, CLR_YELLOW);
    // pinwheel on a post by the gate (fast spin)
    rectfill(108, 118, 3, 34, CLR_DARK_BROWN);
    spr_rot(2, 101, 104, now() * 220);

    // enclosures
    pond(140);     fence(140); plaque("FLAMINGOS", 140);
    lion(290);     fence(290); plaque("LION",      290);
    palm(430);     monkey(430, now() * 120);
                   fence(430); plaque("MONKEYS",   430);
    elephant(560); fence(560); plaque("ELEPHANT",  560);

    // balloon vendor — strings from one post, each balloon a color, all swaying (spr_rot)
    int postx = 356, balloons[] = { 1, 4, 5, 6, 7 };   // red, orange, green, blue, pink
    rectfill(postx, 118, 3, 34, CLR_DARK_BROWN);
    for (int i = 0; i < 5; i++) {
        float ph = now() * 60 + i * 72;
        int bcx = postx + (i - 2) * 11 + (int)(sin_deg(ph) * 4);
        int bcy = 70 + (i % 2) * 9;
        line(postx + 1, 120, bcx, bcy + 7, CLR_LIGHT_GREY);
        spr_rot(balloons[i], bcx - 8, bcy - 8, sin_deg(ph) * 16);
    }

    // the visitor (bobs as they walk), drawn in front of the fences
    int bob = ((int)(now() * 6)) % 2 ? 0 : -1;
    sprf(0, px, py + bob, face < 0, false);

    // --- HUD (screen space) ---
    camera(0, 0);
    print("PIXEL ZOO", 4, 4, CLR_WHITE);
    print("wasd walk   z wave", 4, SCREEN_H - 10, CLR_WHITE);
}
