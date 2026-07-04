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
// Device-adaptive (Phase 2): the host hands the engine the device viewport (in framebuffer px;
// SCALE=1 on iOS → points) so a resizable cart reflows to fill the screen. de_resize reallocs +
// republishes de_screen_w/h; call it whenever the view's bounds change (incl. rotation). Only act on
// it when de_is_resizable() is true — a fixed cart returns 0 and should stay letterboxed at its size.
void            de_resize(int w, int h);
int             de_is_resizable(void);
// Safe-area insets (px; notch / home-bar / status bar). Report them alongside de_resize so a
// resizable cart keeps controls out of the chrome (it reads the usable rect via safe_rect()).
void            de_set_safe_area(int left, int top, int right, int bottom);
void            de_audio_render(float *out, int frames);

void de_touch_begin(int id, float x, float y);
void de_touch_moved(int id, float x, float y);
void de_touch_ended(int id, float x, float y);

// Host MIDI → engine (the AUv3 render block feeds these from its event list). type: +1
// note-on, -1 note-off; note 0..127; vel 1..127. de_midi_bend: -8192..8191 (0 = centre).
// A keybed cart (epiano/moog/…) drains them via the engine's midi_get() and plays notes.
void de_midi_event(int type, int note, int vel);
void de_midi_bend(int v);

#endif
