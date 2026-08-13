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

// THE INSTANCE HANDLE. An AUv3 puts every plug-in instance in ONE process, so every entry point
// names WHICH engine it means. Explicit rather than an internal "current instance" because the same
// instance is driven from four threads while one host thread may serve many instances — there is
// nothing for the engine to infer, and a global holding it would be a UI-thread/audio-thread race.
// Design: docs/design/engine-instance-seam.md. Swift sees this as an OpaquePointer.
typedef struct DeInstance DeInstance;

DeInstance     *de_instance_create(DeRenderer renderer);
void            de_instance_destroy(DeInstance *in);
void            de_frame(DeInstance *in, double t);
const uint32_t *de_framebuffer(DeInstance *in);   // the LIVE canvas — only safe on the thread calling de_frame
// The same frame as a SNAPSHOT, for a view that blits from a different thread than the one ticking
// the engine. That is the AUv3: its render block drives de_frame on the AUDIO thread (the frame is
// sample-clocked, which is what survives an offline bounce) while the view draws on main. Copies the
// last completed frame into `dst` and returns 1, setting *w/*h; returns 0 if nothing is published yet
// or cap_px is too small — *w/*h still report the size needed, so grow and ask again.
int             de_copy_frame(DeInstance *in, uint32_t *dst, int cap_px, int *w, int *h);
int             de_screen_w(DeInstance *in);
int             de_screen_h(DeInstance *in);
// Device-adaptive (Phase 2): the host hands the engine the device viewport (in framebuffer px;
// SCALE=1 on iOS → points) so a resizable cart reflows to fill the screen. de_resize reallocs +
// republishes de_screen_w/h; call it whenever the view's bounds change (incl. rotation). Only act on
// it when de_is_resizable() is true — a fixed cart returns 0 and should stay letterboxed at its size.
void            de_resize(DeInstance *in, int w, int h);
int             de_is_resizable(DeInstance *in);
// Safe-area insets (px; notch / home-bar / status bar). Report them alongside de_resize so a
// resizable cart keeps controls out of the chrome (it reads the usable rect via safe_rect()).
void            de_set_safe_area(DeInstance *in, int left, int top, int right, int bottom);
// Backing scale — points per logical canvas px (= pixelChunk). Feeds finger_px() so finger controls
// are sized physically, not by a raw-px coincidence. Report it alongside de_resize.
void            de_set_backing_scale(DeInstance *in, float k);
// Persistence root — a writable app-private dir (iOS Documents). Call BEFORE de_instance_create() so a cart's
// init() can load_int(). Unset it defaults to "." (cwd). [declared for parity; iOS host wiring TBD]
void            de_set_save_dir(DeInstance *in, const char *dir);
void            de_audio_render(DeInstance *in, float *out, int frames);

// AUDIO INPUT (platform.h §4) — the mirror of de_audio_render, inverted. The host owns the
// capture device + permission flow and PUSHES captured MONO frames in; the engine only analyzes
// them (mic_level/mic_pitch). de_mic_wanted() tells the host when a cart asked for the mic (via
// mic_start()) so it opens capture + prompts for permission lazily; de_mic_set_active reports back.
void de_audio_input(const float *mono, int n, int sample_rate);
int  de_mic_wanted(void);
void de_mic_set_active(int on);

void de_touch_begin(DeInstance *in, int id, float x, float y);
void de_touch_moved(DeInstance *in, int id, float x, float y);
void de_touch_ended(DeInstance *in, int id, float x, float y);

// Host MIDI → engine (the AUv3 render block feeds these from its event list). type: +1
// note-on, -1 note-off; note 0..127; vel 1..127. de_midi_bend: -8192..8191 (0 = centre).
// A keybed cart (epiano/moog/…) drains them via the engine's midi_get() and plays notes.
void de_midi_event(int type, int note, int vel);
void de_midi_bend(int v);

// HOST TRANSPORT (runtime/sync.h) — the AUv3 render block pushes the host's playhead here every
// block, and a cart reads it through sync_active()/sync_playing()/sync_beats()/sync_bpm(). beats =
// the host's absolute beat position, bpm = its tempo, playing = its transport state.
// ⚠ NAMES ITS INSTANCE. A transport push is CONSUMED by the engine that drains it, so while this
// was process-wide the FIRST instance swallowed the START edge and every other one joined mid-flow
// and stayed silent — two DAW tracks, one playing.
void de_sync_position(DeInstance *in, double beats, double bpm, int playing);

#endif
