#ifndef DE_CANVAS_H
#define DE_CANVAS_H
#include <stdint.h>

// spike 1 — a stand-in software canvas. The real engine will fill this buffer via
// the det-probes/ rasterizer; here it's a few primitives, just to prove C-written
// pixels reach the iOS screen and that the OS-driven loop advances frames.
#define DE_W 160   // Game Boy-ish lo-fi; letterboxed to fit any device
#define DE_H 144

int            de_width(void);
int            de_height(void);
const uint8_t* de_framebuffer(void);   // RGBA8888, DE_W*DE_H pixels

// the INVERTED game loop: iOS (CADisplayLink) calls these — we never own main().
void de_ready(void);
void de_update(double t);               // t = seconds since launch
#endif
