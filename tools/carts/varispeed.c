/* de:meta
{
  "slug": "varispeed",
  "title": "varispeed",
  "status": "active",
  "created": "2026-06-15",
  "kind": [
    "instrument"
  ],
  "teaches": [],
  "description": "The showcase for varispeed() - variable TAPE playback speed of the whole mix (the Chase Bliss MOOD 'clock' / a turntable brake), ported from navkit's half_speed. A self-playing riff runs the whole time; varispeed re-reads the final output at a chosen speed: 1.0 = normal, <1 = slower (pitch DOWN + time-stretch), >1 = faster (chipmunk). The applied speed GLIDES (tape inertia), so it dives and spins back smoothly. Hold SPACE for a TAPE-STOP divebomb (the speed brakes toward a halt; release to spin back up); the SPEED slider bends it by hand (0.25x..4x, center 1x); 1..4 change the riff. Two reels spin at the live speed - watch them crawl on the dive. H for help. The last item from the boutique-pedal lists."
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ── VARISPEED ─────────────────────────────────────────────────────────────────
// The showcase for varispeed() — variable TAPE playback speed of the whole mix (the
// Chase Bliss MOOD "clock" / a turntable brake). A self-playing loop runs the whole
// time; varispeed re-reads the final output at a chosen speed:
//   1.0 = normal · <1 = slower (pitch DOWN + time-stretch) · >1 = faster (chipmunk).
// The applied speed GLIDES (tape inertia), so it dives and spins back smoothly.
//
//   SPACE / hold   TAPE STOP — divebomb the speed toward a halt; release to spin back up
//   SPEED slider   bend the speed by hand (0.25× .. 4×, center = 1×)
//   1 2 3 4        change the riff   ·   H help
//
// The reels spin at the live speed — watch them slow to a crawl on the dive.

#define SL_LEAD 8

static float k_speed = 0.5f;   // slider 0..1 → exp speed 0.25..4 (0.5 = 1.0×)
static bool  diving  = false;  // SPACE held → tape-stop dive
static bool  show_help = false;

static int   riff = 0;
static int   step = 0;
static float steptmr = 0.0f;
static float reel = 0.0f;      // reel rotation (advances at the live speed)
static float a_speed = -1.0f;  // last-applied varispeed target (SET-AND-HOLD)

// four little riffs (MIDI), bright so the pitch-bend is obvious
static const int RIFF[4][8] = {
    { 60, 64, 67, 72, 71, 67, 64, 67 },   // C major run
    { 57, 60, 64, 60, 67, 64, 60, 55 },   // Am
    { 62, 65, 69, 65, 74, 69, 65, 62 },   // Dm-ish
    { 55, 59, 62, 67, 71, 67, 62, 59 },   // G
};
static const char *RIFF_NAME[4] = { "C", "Am", "Dm", "G" };

static float speed_of(float k) { return 0.25f * powf(16.0f, k); }   // 0→0.25, 0.5→1.0, 1→4.0

void update(void) {
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) riff = i;
    if (keyp('H')) show_help = !show_help;
    diving = key(KEY_SPACE);   // hold to dive

    // self-playing loop
    steptmr += dt;
    if (steptmr >= 0.16f) {
        steptmr -= 0.16f;
        hit(RIFF[riff][step], SL_LEAD, 5, 200);
        step = (step + 1) & 7;
    }

    // SET-AND-HOLD: target = the dive (when SPACE held) or the slider; re-push only on change.
    // The engine slews the applied speed (tape inertia) → smooth dive + spin-up.
    float target = diving ? 0.28f : speed_of(k_speed);
    if (fabsf(target - a_speed) > 0.001f) { varispeed(target); a_speed = target; }

    reel += target * dt * 3.5f;   // visual reel spins at the (target) speed
}

static void reel_at(int cx, int cy, float spin) {
    circ(cx, cy, 13, CLR_MEDIUM_GREY);
    circfill(cx, cy, 4, CLR_LIGHT_GREY);
    for (int i = 0; i < 3; i++) {
        float a = spin + i * 2.094f;
        line(cx, cy, cx + (int)(cosf(a) * 11), cy + (int)(sinf(a) * 11), CLR_DARK_GREY);
    }
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    print("VARISPEED", 8, 6, CLR_LIGHT_YELLOW);
    print(diving ? "TAPE STOP" : "PLAY", SCREEN_W - 72, 6, diving ? CLR_ORANGE : CLR_MEDIUM_GREY);

    // two reels turning at the live speed
    reel_at(SCREEN_W / 2 - 34, 50, reel);
    reel_at(SCREEN_W / 2 + 34, 50, reel * 1.6f);
    line(SCREEN_W / 2 - 34, 38, SCREEN_W / 2 + 34, 38, CLR_DARK_GREY);   // tape across the top

    char buf[40];
    float spd = diving ? 0.28f : speed_of(k_speed);
    snprintf(buf, sizeof buf, "riff %s   speed %.2fx", RIFF_NAME[riff], spd);
    print(buf, 8, SCREEN_H - 56, CLR_LIGHT_GREY);

    ui_begin();
    ui_slider(&k_speed, 8, SCREEN_H - 40, SCREEN_W - 16, "SPEED  (slow <- 1x -> fast)");
    ui_end();

    font(FONT_SMALL);
    print("SPACE = tape-stop dive   SPEED bends   1-4 riff   H help", 8, SCREEN_H - 7, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    if (show_help) {
        rectfill(22, 22, SCREEN_W - 44, 96, CLR_BLACK);
        rect(22, 22, SCREEN_W - 44, 96, CLR_LIGHT_YELLOW);
        print("varispeed(): tape playback speed", 30, 30, CLR_WHITE);
        print("re-reads the whole mix faster or", 30, 44, CLR_LIGHT_GREY);
        print("slower. <1 = pitch DOWN + slower", 30, 54, CLR_LIGHT_GREY);
        print("(a tape-stop dive); >1 = chipmunk.", 30, 64, CLR_LIGHT_GREY);
        print("It GLIDES (tape inertia) — sweep", 30, 78, CLR_LIGHT_GREY);
        print("it; hold SPACE to brake to a halt.", 30, 88, CLR_ORANGE);
        print("H to close", 30, 102, CLR_DARK_GREY);
    }
}

void init(void) {
    instrument(SL_LEAD, INSTR_SAW, 2, 0, 5, 180);
}
