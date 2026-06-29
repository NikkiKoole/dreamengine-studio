/* de:meta
{
  "title": "spatial",
  "status": "active",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "audio-occlusion",
    "positional-audio"
  ],
  "lineage": "Tech demo for the engine's spatial-audio API (v1 per-voice + v2 emitter buses); novel in demonstrating full-bus Doppler through a shimmer reverb tail alongside per-voice positioning and listener velocity.",
  "description": "Sound that lives in the world — the spatial-audio API (v1 per-voice + v2 emitter buses). You are the EAR (arrow keys to move). A UFO flies across the upper lane as a v2 EMITTER: a sustained tone through SHIMMER reverb, positioned via instrument_pos/instrument_motion so the WHOLE effected bus moves — you hear the Doppler bend the pitch through the entire pass and the glassy tail sweep with it. A CAR loops the lower lane (v1 note_motion) for the simpler per-voice whoosh; a SUITCASE RHODES (bottom-left) arpeggiates EPIANO via positioned hit_at; CLICK/TAP fires a one-shot blip AT that spot. listener()/note_pos()/note_motion()/hit_at() position voices; instrument_pos()/instrument_motion() position a whole effected bus. Dormant and byte-identical until you call listener()."
}
de:meta */
#include "studio.h"
#include <math.h>

// SPATIAL — sound that lives in the world (the spatial-audio API, v1 + v2).
//
// You are the EAR. Move with the ARROW KEYS. Around you:
//   • UFO (upper lane) — a SUSTAINED tone through SHIMMER reverb, positioned as a v2
//     EMITTER. Because it's continuous, you hear the Doppler bend the pitch through the
//     WHOLE pass (a clear rising→falling sweep), and the glassy shimmer tail sweeps with
//     it — the whole effected source moves as one object (instrument_pos/instrument_motion).
//   • CAR (lower lane) — a raw engine tone positioned per-voice (v1 note_motion): the
//     simpler whoosh, for contrast.
//   • SUITCASE RHODES (bottom-left) — a positioned EPIANO arpeggio (one hit_at per note),
//     voiced as a mellow Fender Rhodes suitcase with a tremolo wobble.
//   • CLICK / TAP anywhere — a one-shot blip fired AT that spot (positioned SFX).
//
// v1 (note_pos/note_motion/hit_at) positions individual voices; v2 (instrument_pos/
// instrument_motion) positions a whole instrument's effected bus — wet tail and all.

#define UFO_Y      58.0f
#define UFO_SPEED  2.4f          // px/frame  (×60 = world units/sec for Doppler)
#define CAR_Y      124.0f
#define CAR_SPEED  2.6f
#define RHODES_X   46.0f
#define RHODES_Y   162.0f

static float px, py, ppx, ppy;   // the player == the listener (+ previous, for velocity)
static float ufox, carx;
static int   ufo_h, car_h;       // held-note handles
static int   arp_t, arp_i;       // rhodes arpeggio clock
static int   blip_x = -1, blip_y = -1, blip_t = 0;

static const int ARP[] = { 52, 55, 59, 62, 66, 62, 59, 55 };   // Em9-ish rhodes

void init(void) {
    px = SCREEN_W * 0.5f; py = SCREEN_H * 0.52f;
    ppx = px; ppy = py;
    ufox = 70.0f; carx = 150.0f;

    instrument(5, INSTR_SINE, 120, 0, 7, 400);     // UFO: a pure sustained tone
    instrument(6, INSTR_SAW,    4, 0, 6,  40);     // car engine rumble
    // SUITCASE RHODES (per the epiano cart's "suitcase" preset): Rhodes tine voicing, mellow,
    // gentle bark, rings down. The real Suitcase's stereo vibrato is auto-pan — but that fights
    // the positional pan here, so a mono amplitude tremolo gives the wobble, spatial-safe.
    instrument(7, INSTR_EPIANO, 1, 0, 7, 450);     // struck, rings down
    instrument_harmonics(7, 0.0f);                 // voicing 0 = Rhodes tine
    instrument_timbre(7, 0.20f);                   // mellow suitcase brightness
    instrument_morph(7, 0.12f);                    // gentle dig-in bark
    instrument_tremolo(7, 5.0f, 0.35f, LFO_SHAPE_SINE); // the Suitcase wobble (amplitude, not stereo pan)

    spatial_model(18.0f, 260.0f, 1.2f);            // distance falloff (reads on 320px)
    spatial_speed(420.0f);                         // Doppler tuned for a musical whoosh
    listener(px, py);

    // UFO = a v2 EMITTER carrying SHIMMER. instrument_shimmer gives slot 5 its own bus
    // (tone + glassy tail); instrument_pos/_motion (in update) fly that whole bus past the
    // listener, so the produced sound — wet tail included — Dopplers and pans as one object.
    instrument_shimmer(5, 0.85f, 0.40f, 0.70f, 0.55f);
    instrument_pos(5, ufox, UFO_Y);                // claim the bus + place it
    ufo_h = note_on(72, 5, 5);                     // a held tone → into slot 5's shimmer bus
    note_lfo(ufo_h, 0, LFO_PITCH, 5.5f, 0.4f);     // gentle vibrato — alive, but steady enough to read the Doppler

    car_h = note_on(36, 6, 5);                     // low engine (v1 per-voice spatialization)
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

    // UFO (v2): fly the whole shimmer bus across — position + velocity each frame
    ufox += UFO_SPEED;
    if (ufox > SCREEN_W + 30) ufox = -30.0f;
    instrument_pos(5, ufox, UFO_Y);
    instrument_motion(5, UFO_SPEED * 60.0f, 0.0f);

    // CAR (v1): per-voice positioned engine, Dopplers as it passes
    carx += CAR_SPEED;
    if (carx > SCREEN_W + 24) carx = -24.0f;
    note_pos(car_h, carx, CAR_Y);
    note_motion(car_h, CAR_SPEED * 60.0f, 0.0f);

    // RHODES (v1): a positioned one-shot per arp note
    if (++arp_t >= 30) { arp_t = 0; hit_at(ARP[arp_i & 7], 7, 5, 650, RHODES_X, RHODES_Y); arp_i++; }

    // positioned one-shot at the cursor / finger
    int fired = 0, mx = 0, my = 0;
    if (mouse_pressed(0)) { mx = mouse_x(); my = mouse_y(); fired = 1; }
    else if (tapp(0, 0, SCREEN_W, SCREEN_H)) { mx = touch_x(0); my = touch_y(0); fired = 1; }
    if (fired) { hit_at(72, INSTR_NOISE, 6, 90, (float)mx, (float)my); blip_x = mx; blip_y = my; blip_t = 18; }
    if (blip_t > 0) blip_t--;

#ifdef DE_TRACE
    watch("ufox", "%.0f", ufox);
    watch("carx", "%.0f", carx);
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
    line(0, (int)UFO_Y, SCREEN_W, (int)UFO_Y, CLR_DARKER_GREY);
    line(0, (int)CAR_Y, SCREEN_W, (int)CAR_Y, CLR_DARKER_GREY);

    // UFO — the shimmer emitter: a saucer (dome + hull) with running lights
    emitter(ufox, UFO_Y, CLR_PINK, 5);
    ovalfill((int)ufox, (int)UFO_Y + 1, 11, 3, CLR_INDIGO);   // hull (center, half-extents)
    ovalfill((int)ufox, (int)UFO_Y - 2, 5, 4, CLR_PINK);      // dome
    pset((int)ufox - 7, (int)UFO_Y + 1, CLR_WHITE);
    pset((int)ufox + 7, (int)UFO_Y + 1, CLR_WHITE);

    // CAR — v1 engine (orange)
    emitter(carx, CAR_Y, CLR_ORANGE, 13);
    rectfill((int)carx - 7, (int)CAR_Y - 4, 14, 8, CLR_ORANGE);
    rectfill((int)carx - 4, (int)CAR_Y - 7, 8, 5, CLR_RED);

    // RHODES — v1 hit_at arpeggio (green keys, flash on a note)
    int hot = (arp_t < 6);
    emitter(RHODES_X, RHODES_Y, CLR_GREEN, 9);
    rectfill((int)RHODES_X - 8, (int)RHODES_Y - 4, 16, 8, hot ? CLR_WHITE : CLR_GREEN);
    for (int k = -6; k <= 6; k += 3) line((int)RHODES_X + k, (int)RHODES_Y - 4, (int)RHODES_X + k, (int)RHODES_Y + 3, CLR_DARK_BLUE);

    if (blip_t > 0) circ(blip_x, blip_y, 18 - blip_t, CLR_YELLOW);

    // the listener (you): a head with two ears
    circfill((int)px, (int)py, 5, CLR_WHITE);
    circfill((int)px - 5, (int)py, 2, CLR_LIGHT_GREY);
    circfill((int)px + 5, (int)py, 2, CLR_LIGHT_GREY);

    rectfill(0, 0, SCREEN_W, 11, CLR_BLACK);
    print("SPATIAL  arrows=move  UFO=shimmer doppler", 4, 2, CLR_WHITE);
}
