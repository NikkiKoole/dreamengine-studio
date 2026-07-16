#ifndef DE_ANDROID_ENGINE_H
#define DE_ANDROID_ENGINE_H
#include <stdint.h>

// The dreamengine platform seam, as the Android host sees it. Byte-for-byte the same
// contract as ios/Sources/engine.h (mirrors runtime/platform.h) — studio.c +
// raylib_compat.c under DE_NO_RAYLIB provide the bodies.
//
// Render: SOFTWARE — de_frame() fills a CPU framebuffer; de_framebuffer() returns it
//   (RGBA8888 = bytes R,G,B,A per sw_pack, de_screen_w()*de_screen_h(), BOTTOM-UP —
//   the GLES blit flips Y in the shader).
// Audio:  de_audio_render() fills STEREO INTERLEAVED floats @ 44100 (audio thread).
// Touch:  the host feeds motion events in framebuffer pixels (SCALE=1 -> 1:1).

typedef enum { DE_RENDERER_SOFTWARE = 0, DE_RENDERER_GPU = 1 } DeRenderer;

void            de_init(DeRenderer renderer);
void            de_frame(double t);
const uint32_t *de_framebuffer(void);
int             de_screen_w(void);
int             de_screen_h(void);
void            de_resize(int w, int h);
int             de_is_resizable(void);
void            de_set_safe_area(int left, int top, int right, int bottom);
void            de_set_backing_scale(float k);
// Persistence root — a writable app-private dir (Android internalDataPath). Call BEFORE de_init()
// so a cart's init() can load_int(). Unset it defaults to "." (cwd, not writable on Android).
void            de_set_save_dir(const char *dir);
void            de_audio_render(float *out, int frames);

void de_touch_begin(int id, float x, float y);
void de_touch_moved(int id, float x, float y);
void de_touch_ended(int id, float x, float y);

void de_midi_event(int type, int note, int vel);
void de_midi_bend(int v);

#endif
