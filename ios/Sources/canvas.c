#include "canvas.h"
#include <math.h>

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

void de_ready(void) { clear(20, 18, 28); }

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
}
