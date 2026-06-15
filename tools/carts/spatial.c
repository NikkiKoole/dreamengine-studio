#include "studio.h"
#include <math.h>

// SPATIAL — sound that lives in the world (the v1 spatial-audio API).
//
// You are the EAR. Move with the ARROW KEYS. Around you:
//   • RADIO (top-right) — a held ORGAN pad washed in SHIMMER reverb. Walk
//     around it: it pans L/R and rises/falls with distance, glassy tail and all.
//   • RHODES (bottom-left) — a positioned EPIANO arpeggio (one hit_at per note).
//     A second musical source, somewhere else in the world.
//   • CAR — drives across on a loop. Stand near its lane for the Doppler WHOOSH
//     + L→R sweep as it blows past.
//   • CLICK / TAP anywhere — a one-shot blip fired AT that spot (positioned SFX).
//
// listener() + note_pos() + note_motion() + hit_at() do the spatial work; the
// engine derives pan, distance-volume and Doppler from the geometry. (NB: the
// SHIMMER wet tail lives on the master bus, so it doesn't pan with the radio —
// that's the v1 per-voice limit; moving a whole effected mix is the v2 job.)

#define SPEAKER_X 252.0f
#define SPEAKER_Y 50.0f
#define RHODES_X  46.0f
#define RHODES_Y  158.0f
#define CAR_Y     112.0f
#define CAR_SPEED 2.6f            // px/frame  (×60 = world units/sec for Doppler)

static float px, py, ppx, ppy;   // the player == the listener (+ previous, for velocity)
static float carx;
static int   speaker_h, car_h;   // held-note handles
static int   arp_t, arp_i;       // rhodes arpeggio clock
static int   blip_x = -1, blip_y = -1, blip_t = 0;

// a lush, slightly bluesy rhodes loop (Em9-ish → up and back down)
static const int ARP[] = { 52, 55, 59, 62, 66, 62, 59, 55 };

void init(void) {
    px = SCREEN_W * 0.42f; py = SCREEN_H * 0.52f;
    ppx = px; ppy = py;
    carx = 92.0f;

    instrument(5, INSTR_ORGAN,  10, 0, 6, 160);   // radio: a held organ pad
    instrument(6, INSTR_SAW,     4, 0, 6,  40);    // car engine rumble
    instrument(7, INSTR_EPIANO,  2, 0, 0, 700);    // rhodes (struck, rings down)
    instrument_harmonics(7, 0.0f);                 // EPIANO voicing 0 = Rhodes
    instrument_timbre(7, 0.45f);                   // a touch of bark/brightness

    shimmer(0.85f, 0.42f, 0.5f, 0.36f);            // dreamy master wash (set ONCE)

    spatial_model(18.0f, 250.0f, 1.2f);            // distance falloff (reads on 320px)
    spatial_speed(420.0f);                         // Doppler tuned for a musical whoosh
    listener(px, py);

    speaker_h = note_on(55, 5, 4);                 // held G organ pad
    note_pos(speaker_h, SPEAKER_X, SPEAKER_Y);
    car_h = note_on(36, 6, 5);                     // low engine
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

    listener(px, py);
    listener_vel((px - ppx) * 60.0f, (py - ppy) * 60.0f);

    // car loops left→right; position + velocity → it Dopplers as it passes
    carx += CAR_SPEED;
    if (carx > SCREEN_W + 24) carx = -24.0f;
    note_pos(car_h, carx, CAR_Y);
    note_motion(car_h, CAR_SPEED * 60.0f, 0.0f);

    // rhodes arpeggio — a positioned one-shot per note, at the rhodes emitter
    if (++arp_t >= 30) {                            // ~0.5s/step
        arp_t = 0;
        hit_at(ARP[arp_i & 7], 7, 5, 650, RHODES_X, RHODES_Y);
        arp_i++;
    }

    // positioned one-shot at the cursor / finger
    int fired = 0, mx = 0, my = 0;
    if (mouse_pressed(0)) { mx = mouse_x(); my = mouse_y(); fired = 1; }
    else if (tapp(0, 0, SCREEN_W, SCREEN_H)) { mx = touch_x(0); my = touch_y(0); fired = 1; }
    if (fired) { hit_at(72, INSTR_NOISE, 6, 90, (float)mx, (float)my); blip_x = mx; blip_y = my; blip_t = 18; }
    if (blip_t > 0) blip_t--;

#ifdef DE_TRACE
    float dx = SPEAKER_X - px, dy = SPEAKER_Y - py;
    float d = sqrtf(dx*dx + dy*dy);
    watch("radio_dist", "%.0f", d);
    watch("radio_pan",  "%.2f", d > 0.1f ? dx / d : 0.0f);
    watch("carx",       "%.0f", carx);
#endif
}

// a sound-source marker with expanding rings
static void emitter(float x, float y, int col, int phase) {
    for (int r = 0; r < 3; r++) {
        int rr = (((int)(now() * 30) + phase + r * 9) % 27);
        if (rr > 4) circ((int)x, (int)y, rr, col);
    }
}

void draw(void) {
    cls(CLR_DARK_BLUE);

    for (int gx = 0; gx <= SCREEN_W; gx += 32) line(gx, 12, gx, SCREEN_H, CLR_DARK_GREY);
    for (int gy = 24; gy <= SCREEN_H; gy += 32) line(0, gy, SCREEN_W, gy, CLR_DARK_GREY);

    // car + lane
    line(0, (int)CAR_Y, SCREEN_W, (int)CAR_Y, CLR_DARKER_GREY);
    emitter(carx, CAR_Y, CLR_ORANGE, 13);
    rectfill((int)carx - 7, (int)CAR_Y - 4, 14, 8, CLR_ORANGE);
    rectfill((int)carx - 4, (int)CAR_Y - 7, 8, 5, CLR_RED);

    // radio speaker (organ + shimmer)
    emitter(SPEAKER_X, SPEAKER_Y, CLR_GREEN, 0);
    rectfill((int)SPEAKER_X - 5, (int)SPEAKER_Y - 6, 10, 12, CLR_GREEN);
    circfill((int)SPEAKER_X, (int)SPEAKER_Y, 3, CLR_DARK_BLUE);

    // rhodes emitter — a little keyboard, flashes on each arp note
    int hot = (arp_t < 6);
    emitter(RHODES_X, RHODES_Y, CLR_INDIGO, 9);
    rectfill((int)RHODES_X - 8, (int)RHODES_Y - 4, 16, 8, hot ? CLR_PINK : CLR_INDIGO);
    for (int k = -6; k <= 6; k += 3) line((int)RHODES_X + k, (int)RHODES_Y - 4, (int)RHODES_X + k, (int)RHODES_Y + 3, CLR_DARK_BLUE);

    // the one-shot ping
    if (blip_t > 0) circ(blip_x, blip_y, 18 - blip_t, CLR_YELLOW);

    // the listener (you): a head with two ears
    circfill((int)px, (int)py, 5, CLR_WHITE);
    circfill((int)px - 5, (int)py, 2, CLR_LIGHT_GREY);
    circfill((int)px + 5, (int)py, 2, CLR_LIGHT_GREY);

    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("SPATIAL  arrows=move  click=blip", 4, 2, CLR_WHITE);
}
