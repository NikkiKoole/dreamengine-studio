#include "studio.h"
#include <math.h>

// SPATIAL — sound that lives in the world (the v1 spatial-audio API).
//
// You are the EAR. Move with the ARROW KEYS. Around you:
//   • RADIO speaker (fixed, top-right) — a steady tone. Walk around it and hear
//     it pan left/right and rise/fall with distance.
//   • CAR — drives across on a loop. Stand near its lane and hear the Doppler
//     WHOOSH + the L→R sweep as it blows past you.
//   • CLICK / TAP anywhere — a one-shot blip fired AT that spot (positioned SFX).
//
// It's all four calls: listener() + note_pos() + note_motion() + hit_at().
// The engine derives pan, distance-volume and Doppler from the geometry.

#define SPEAKER_X 252.0f
#define SPEAKER_Y 54.0f
#define CAR_Y     132.0f
#define CAR_SPEED 2.6f            // px/frame  (×60 = world units/sec for Doppler)

static float px, py;             // the player == the listener
static float ppx, ppy;           // previous position (for listener velocity)
static float carx;
static int   speaker_h, car_h;   // held-note handles
static int   blip_x = -1, blip_y = -1, blip_t = 0;   // last one-shot, for a visual ping

void init(void) {
    px = SCREEN_W * 0.40f; py = SCREEN_H * 0.58f;
    ppx = px; ppy = py;
    carx = 88.0f;                 // start on-screen so the thumbnail shows it

    instrument(5, INSTR_TRI, 4, 0, 6, 40);   // radio tone
    instrument(6, INSTR_SAW, 4, 0, 6, 40);   // car engine rumble

    // exaggerate falloff so distance READS on a 320px screen; speed-of-sound tuned
    // so the car's pass bends ~a fifth up then a fourth down (a musical whoosh, not a
    // banshee — keep c well above the source speed or Doppler runs away near vr→−c)
    spatial_model(18.0f, 240.0f, 1.2f);
    spatial_speed(420.0f);
    listener(px, py);

    speaker_h = note_on(60, 5, 4);           // mid radio tone
    note_pos(speaker_h, SPEAKER_X, SPEAKER_Y);

    car_h = note_on(36, 6, 5);               // low engine
    note_pos(car_h, carx, CAR_Y);
}

void update(void) {
    ppx = px; ppy = py;
    float sp = 1.8f;
    if (btn(0, BTN_LEFT))  px -= sp;
    if (btn(0, BTN_RIGHT)) px += sp;
    if (btn(0, BTN_UP))    py -= sp;
    if (btn(0, BTN_DOWN))  py += sp;
    if (px < 6) px = 6; else if (px > SCREEN_W - 6) px = SCREEN_W - 6;
    if (py < 18) py = 18; else if (py > SCREEN_H - 6) py = SCREEN_H - 6;

    // listener follows the player; velocity in units/sec (px/frame × 60) for Doppler
    listener(px, py);
    listener_vel((px - ppx) * 60.0f, (py - ppy) * 60.0f);

    // the car loops left→right; feed it position + velocity so it Dopplers as it passes
    carx += CAR_SPEED;
    if (carx > SCREEN_W + 24) carx = -24.0f;
    note_pos(car_h, carx, CAR_Y);
    note_motion(car_h, CAR_SPEED * 60.0f, 0.0f);

    // positioned one-shot at the cursor / finger
    int fired = 0, mx = 0, my = 0;
    if (mouse_pressed(0)) { mx = mouse_x(); my = mouse_y(); fired = 1; }
    else if (tapp(0, 0, SCREEN_W, SCREEN_H)) { mx = touch_x(0); my = touch_y(0); fired = 1; }
    if (fired) {
        hit_at(72, INSTR_NOISE, 6, 90, (float)mx, (float)my);   // a positioned tick
        blip_x = mx; blip_y = my; blip_t = 18;
    }
    if (blip_t > 0) blip_t--;

#ifdef DE_TRACE
    float dx = SPEAKER_X - px, dy = SPEAKER_Y - py;
    float d = sqrtf(dx*dx + dy*dy);
    watch("radio_dist", "%.0f", d);
    watch("radio_pan",  "%.2f", d > 0.1f ? dx / d : 0.0f);
    watch("carx",       "%.0f", carx);
#endif
}

// a little sound-source marker with expanding rings
static void emitter(float x, float y, int col, int phase) {
    for (int r = 0; r < 3; r++) {
        int rr = ((((int)(now() * 30) + phase) + r * 9) % 27);
        if (rr > 4) circ((int)x, (int)y, rr, col);
    }
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    // ground grid → a sense of space
    for (int gx = 0; gx <= SCREEN_W; gx += 32) line(gx, 12, gx, SCREEN_H, CLR_DARK_GREY);
    for (int gy = 24; gy <= SCREEN_H; gy += 32) line(0, gy, SCREEN_W, gy, CLR_DARK_GREY);

    // the car + its lane
    line(0, (int)CAR_Y, SCREEN_W, (int)CAR_Y, CLR_DARKER_GREY);
    emitter(carx, CAR_Y, CLR_ORANGE, 13);
    rectfill((int)carx - 7, (int)CAR_Y - 4, 14, 8, CLR_ORANGE);
    rectfill((int)carx - 4, (int)CAR_Y - 7, 8, 5, CLR_RED);

    // the radio speaker
    emitter(SPEAKER_X, SPEAKER_Y, CLR_GREEN, 0);
    rectfill((int)SPEAKER_X - 5, (int)SPEAKER_Y - 6, 10, 12, CLR_GREEN);
    circfill((int)SPEAKER_X, (int)SPEAKER_Y, 3, CLR_DARK_BLUE);

    // the one-shot ping
    if (blip_t > 0) circ(blip_x, blip_y, 18 - blip_t, CLR_YELLOW);

    // the listener (you): a head with two ears
    circfill((int)px, (int)py, 5, CLR_WHITE);
    circfill((int)px - 5, (int)py, 2, CLR_LIGHT_GREY);
    circfill((int)px + 5, (int)py, 2, CLR_LIGHT_GREY);

    // HUD
    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("SPATIAL  arrows=move  click=blip", 4, 2, CLR_WHITE);
}
