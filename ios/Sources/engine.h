#ifndef TINYJAM_ENGINE_H
#define TINYJAM_ENGINE_H
#include <stdint.h>

// The dreamengine platform seam, as Swift sees it (Phase 2 — the REAL engine).
// Mirrors runtime/platform.h, but standalone so the bridging header doesn't drag in
// studio.h (which needs the DE_NO_RAYLIB shim ordering). studio.c/raylib_compat.c
// provide the bodies; this is just the host↔engine contract for CanvasView + AudioEngine.
//
// Render: SOFTWARE renderer — de_frame() fills a CPU framebuffer; de_framebuffer()
// returns it (RGBA8888, de_screen_w()*de_screen_h(), BOTTOM-UP — the blit must flip).
// Audio:  de_audio_render() fills STEREO INTERLEAVED floats @ 44100 (audio thread).
// Touch:  the host feeds UIKit touches in framebuffer pixels (SCALE=1 on iOS → 1:1).

typedef enum { DE_RENDERER_SOFTWARE = 0, DE_RENDERER_GPU = 1 } DeRenderer;

void            de_init(DeRenderer renderer);
void            de_frame(double t);
const uint32_t *de_framebuffer(void);
int             de_screen_w(void);
int             de_screen_h(void);
void            de_audio_render(float *out, int frames);

void de_touch_begin(int id, float x, float y);
void de_touch_moved(int id, float x, float y);
void de_touch_ended(int id, float x, float y);

#endif
