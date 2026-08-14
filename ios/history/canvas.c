#include "canvas.h"
#include "audio.h"
#include "tinyjam_store.h"
#include "save.h"
#include <math.h>

extern int AppGroup_UnlockedCount(void);   // implemented in AppGroup.swift (@_cdecl)

static uint8_t fb[DE_W * DE_H * 4];

int            de_width(void)       { return DE_W; }
int            de_height(void)      { return DE_H; }
const uint8_t* de_framebuffer(void) { return fb; }

static inline void pset(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if ((unsigned)x >= (unsigned)DE_W || (unsigned)y >= (unsigned)DE_H) return;
    uint8_t *p = &fb[(y * DE_W + x) * 4];
    p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
}

static void clear(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < DE_W * DE_H; i++) { fb[i*4]=r; fb[i*4+1]=g; fb[i*4+2]=b; fb[i*4+3]=255; }
}

static void fillcirc(int cx, int cy, int rad, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = -rad; y <= rad; y++)
        for (int x = -rad; x <= rad; x++)
            if (x*x + y*y <= rad*rad) pset(cx + x, cy + y, r, g, b);
}

void de_ready(void) { clear(20, 18, 28); de_record_launch(); }

void de_update(double t) {
    clear(20, 18, 28);
    // bouncing pink ball — proves frame-to-frame animation (the loop runs)
    int cx = (int)(DE_W * 0.5 + cos(t * 2.0) * (DE_W * 0.35));
    int cy = (int)(DE_H * 0.62 - fabs(sin(t * 3.0)) * (DE_H * 0.40));
    fillcirc(cx, cy, 14, 255, 110, 181);
    // scrolling checker strip — proves per-pixel writes across the whole buffer
    for (int x = 0; x < DE_W; x++) {
        int phase = ((x + (int)(t * 30)) / 8) & 1;
        for (int y = DE_H - 12; y < DE_H; y++)
            pset(x, y, phase ? 60 : 40, phase ? 52 : 34, phase ? 72 : 48);
    }
    // VU meter (top) — driven by the AUDIO thread's RMS, so a moving bar is visual
    // proof the CoreAudio render callback is actually being pulled.
    int vu = (int)(de_audio_level() * 6.0f * DE_W);   // ~0.18 peak → near full width
    if (vu > DE_W) vu = DE_W;
    for (int x = 0; x < DE_W; x++)
        for (int y = 4; y < 10; y++)
            pset(x, y, x < vu ? 120 : 36, x < vu ? 230 : 30, x < vu ? 140 : 40);
    // store gate indicator (top-right): green if the Rebirth rack is unlocked, else red.
    // Proves the C→Swift StoreKit bridge round-trips, reflecting live entitlements (a purchase
    // from the headless test persists in the simulator's StoreKit-test store → shows green).
    int unlocked = Store_IsModuleUnlocked("com.mipolai.tinyjam.acidrack");
    for (int y = 14; y < 22; y++)
        for (int x = DE_W - 10; x < DE_W - 2; x++)
            pset(x, y, unlocked ? 90 : 220, unlocked ? 220 : 60, unlocked ? 110 : 60);
    // app-group indicator (left of the store gate): magenta if the shared group reports any
    // unlocked rack — i.e. the entitlement an AUv3 extension would read has propagated.
    int agc = AppGroup_UnlockedCount();
    for (int y = 14; y < 22; y++)
        for (int x = DE_W - 22; x < DE_W - 14; x++)
            pset(x, y, agc > 0 ? 200 : 40, agc > 0 ? 70 : 30, agc > 0 ? 200 : 50);
    // launch counter — one cyan square per app launch, persisted via de_save_bytes to the
    // iOS Documents dir. Re-running ./build.sh adds a square → proves saves survive relaunch.
    int lc = de_launch_count();
    for (int k = 0; k < lc && k < 36; k++) {
        int bx = 4 + k * 4;
        for (int yy = 26; yy < 31; yy++)
            for (int xx = bx; xx < bx + 3; xx++)
                pset(xx, yy, 90, 200, 230);
    }
}
