#ifndef DE_ANDROID_ENGINE_H
#define DE_ANDROID_ENGINE_H
#include <stdint.h>

// The dreamengine platform seam, as the Android host sees it. The same contract as
// ios/Sources/engine.h and runtime/platform.h — studio.c + raylib_compat.c under
// DE_NO_RAYLIB provide the bodies.
//
// ⚠ THIS FILE IS A HAND-COPY OF runtime/platform.h AND NOTHING USED TO CHECK IT. It went
// completely stale once already: written against the pre-per-instance seam, it kept declaring
// `de_init(DeRenderer)` (a function that no longer exists) and every entry point without its
// `DeInstance *`, while main.c called those shapes — so the port could not link, and said so
// nowhere. tools/lint-engine-seam.js would have caught it on day one but only walked ios/ and
// runtime/. If you add or change an entry point here, run that lint.
//
// Render: SOFTWARE — de_frame() fills a CPU framebuffer; de_framebuffer() returns it
//   (RGBA8888 = bytes R,G,B,A per sw_pack, de_screen_w()*de_screen_h(), BOTTOM-UP —
//   the GLES blit flips Y in the shader).
// Audio:  de_audio_render() fills STEREO INTERLEAVED floats @ 44100 (audio thread).
// Touch:  the host feeds motion events in framebuffer pixels (SCALE=1 -> 1:1).

typedef enum { DE_RENDERER_SOFTWARE = 0, DE_RENDERER_GPU = 1 } DeRenderer;

// THE INSTANCE HANDLE. Android runs exactly one engine per process today, so this looks like
// ceremony — it is not. It is the same handle the AUv3 needs because a plug-in host puts several
// instances in ONE process, and keeping the two hosts on one signature is what stops this file
// drifting again. Swift/Kotlin never sees it; main.c holds it in one place.
typedef struct DeInstance DeInstance;

DeInstance     *de_instance_create(DeRenderer renderer);
void            de_instance_destroy(DeInstance *in);
void            de_frame(DeInstance *in, double t);
const uint32_t *de_framebuffer(DeInstance *in);   // the LIVE canvas — only safe on the thread calling de_frame
int             de_screen_w(DeInstance *in);
int             de_screen_h(DeInstance *in);
// Device-adaptive: the host hands the engine the device viewport (in framebuffer px) so a
// resizable cart reflows to fill the screen. Only act on it when de_is_resizable() is true —
// a fixed cart returns 0 and should stay letterboxed at its own size.
void            de_resize(DeInstance *in, int w, int h);
int             de_is_resizable(DeInstance *in);
// Safe-area insets (px; cutout / gesture bar / status bar), so a resizable cart keeps controls
// out of the chrome (it reads the usable rect via safe_rect()).
void            de_set_safe_area(DeInstance *in, int left, int top, int right, int bottom);
// Backing scale — points per logical canvas px. Feeds finger_px() so finger controls are sized
// physically rather than by a raw-px coincidence.
void            de_set_backing_scale(DeInstance *in, float k);
// Persistence root — a writable app-private dir (Android internalDataPath). ⚠ Call BEFORE
// de_instance_create() so the cart's init() can load_int() — and therefore with `in` = NULL,
// which resolves to the context the engine is about to boot instance 0 into. That reads odd and
// is correct: the pristine snapshot every later instance copies is taken inside create, so a
// save dir set beforehand propagates to all of them.
void            de_set_save_dir(DeInstance *in, const char *dir);
void            de_audio_render(DeInstance *in, float *out, int frames);

// AUDIO INPUT (platform.h §4) — the mirror of de_audio_render, inverted. The host owns the
// capture device + permission and PUSHES captured MONO frames in; the engine only analyzes them
// (mic_level/mic_pitch). de_mic_wanted() tells the host when a cart asked for the mic (mic_start())
// so it opens capture + prompts for RECORD_AUDIO lazily; de_mic_set_active reports capture is live.
// seam-lint-ignore: these three name no instance BY DESIGN — one capture device per process, the
// same call recorded for iOS in tools/ctx-classification.json (runtime/mic.h stays shared).
void de_audio_input(const float *mono, int n, int sample_rate);
int  de_mic_wanted(void);
void de_mic_set_active(int on);

void de_touch_begin(DeInstance *in, int id, float x, float y);
void de_touch_moved(DeInstance *in, int id, float x, float y);
void de_touch_ended(DeInstance *in, int id, float x, float y);

// Host MIDI → engine. type: +1 note-on, -1 note-off; note 0..127; vel 1..127.
// de_midi_bend: -8192..8191 (0 = centre). de_midi_cc keeps the channel (0..15 as it arrives).
// Unused by this host so far — Android MIDI would feed these from AMidiDevice.
void de_midi_event(DeInstance *in, int type, int note, int vel);
void de_midi_bend(DeInstance *in, int v);
void de_midi_cc(DeInstance *in, int ch, int cc, int val);

#endif
