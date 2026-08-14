#ifndef DE_ANDROID_ENGINE_H
#define DE_ANDROID_ENGINE_H
#include <stdint.h>

// The dreamengine platform seam, as the Android host sees it.
//
// ⚠⚠ THIS WHOLE FILE IS STALE AND THE PORT WILL NOT BUILD (noticed 2026-08-14). It was written
// against the pre-per-instance seam, and EVERY declaration below is now wrong: the engine takes a
// `DeInstance *` first argument on de_frame/de_framebuffer/de_screen_w/de_screen_h/de_resize/
// de_is_resizable/de_set_safe_area/de_set_backing_scale/de_set_save_dir/de_audio_render/
// de_touch_*/de_midi_*, and `de_init(DeRenderer)` no longer exists at all — it is
// `DeInstance *de_instance_create(DeRenderer)`. main.c calls the old shapes throughout.
//
// It says "byte-for-byte the same contract as ios/Sources/engine.h" below. That was true when
// written and is exactly the kind of sentence this repo has learned to distrust: nothing checked
// it, because tools/lint-engine-seam.js only walks ios/ and runtime/. Do NOT patch individual
// lines here — a half-migrated header reads as done and is worse than an obviously broken one.
// Migrating the port is one job: this header, main.c's call sites, and one engine owner
// (`de:engine-owner`) holding the instance main.c currently keeps implicitly.
//
// Was: byte-for-byte the same contract as ios/Sources/engine.h (mirrors runtime/platform.h) —
// studio.c + raylib_compat.c under DE_NO_RAYLIB provide the bodies.
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

// AUDIO INPUT (platform.h §4) — the mirror of de_audio_render, inverted. The host owns the
// capture device + permission and PUSHES captured MONO frames in; the engine only analyzes them
// (mic_level/mic_pitch). de_mic_wanted() tells the host when a cart asked for the mic (mic_start())
// so it opens capture + prompts for RECORD_AUDIO lazily; de_mic_set_active reports capture is live.
void de_audio_input(const float *mono, int n, int sample_rate);
int  de_mic_wanted(void);
void de_mic_set_active(int on);

void de_touch_begin(int id, float x, float y);
void de_touch_moved(int id, float x, float y);
void de_touch_ended(int id, float x, float y);

void de_midi_event(int type, int note, int vel);
void de_midi_bend(int v);

#endif
