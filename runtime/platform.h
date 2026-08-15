#ifndef DE_PLATFORM_H
#define DE_PLATFORM_H

// ============================================================================
// platform.h — the host seam for dreamengine.
//
// The engine CORE (update/draw, the 32-color primitives, the mixer) is meant to
// be host-agnostic. Each PLATFORM BACKEND drives that core through this one
// interface, so adding a target later ("support the Switch") is "implement one
// backend behind the seam", not "surgery on studio.c again".
//
//   backend          renderer            owns the loop via
//   ---------------  ------------------  -------------------------
//   raylib-desktop   GPU (default) / SW  studio.c main() while-loop   [today]
//   web (emscripten) GPU / SW            emscripten_set_main_loop     [today]
//   ios              SW now, Metal next  CADisplayLink → de_frame()   [building]
//   switch (future)  its GPU             devkitPro frame callback     [someday]
//
// TWO RENDERERS, ONE SEAM (settled 2026-06-29; see docs/design/engine-portability.md).
// The renderer is itself a backend choice, NOT a fixed "hand over a framebuffer"
// contract. Measurement showed the software canvas is ~30x under the 60fps budget
// for 2D/light carts but a GPU wins by FACTORS for heavy work (hundreds of sprites,
// tritex/3D — `podracer` was 19ms on a fast Mac CPU). So both stay first-class:
//   - DE_RENDERER_SOFTWARE — the engine rasterizes into a CPU RGBA framebuffer
//     (sw_cbuf); the host blits one texture. No GL/ANGLE. Best for portability +
//     simple carts (the iOS launch path).
//   - DE_RENDERER_GPU      — the engine issues GPU draw calls; the host owns a
//     GL/Metal context. Best for heavy carts. Raylib on desktop today; a Metal
//     backend is where iOS heavy carts will go (the GPU-path stubs are its seat).
// studio.c already forks `if (sw_canvas_active) {...} else {<GPU>}` per primitive,
// so the engine supports both already — this seam just lets the PLATFORM pick.
// ============================================================================

#include <stdint.h>
#include "studio.h"   // SCREEN_W / SCREEN_H and the public API

#ifdef __cplusplus
extern "C" {
#endif

// ---- which renderer a backend wants ----------------------------------------
typedef enum {
    DE_RENDERER_SOFTWARE = 0,   // CPU framebuffer (sw_cbuf); de_framebuffer() valid
    DE_RENDERER_GPU      = 1,   // host owns a GL/Metal context; de_framebuffer() == NULL
} DeRenderer;

// ============================================================================
// (1) ENGINE ENTRY POINTS — the host backend calls these.
//     On raylib-desktop, studio.c's own main() is the "host" and calls them in
//     its while-loop. On iOS, CanvasView/CADisplayLink calls them per vsync tick.
// ============================================================================

// ============================================================================
// THE INSTANCE HANDLE.
//
// An AUv3 puts every plug-in instance in ONE process, so "the engine" cannot be a
// singleton: two racks on two DAW tracks each need their own state. Every entry
// point below therefore names WHICH engine it is talking to.
//
// Why an explicit parameter and not an internal "current instance": the same
// instance is driven from FOUR threads (the audio render block, the frame worker,
// the host's layout pass, and the view thread that copies the frame), while one
// host thread may serve MANY instances. There is no "current" for the engine to
// infer, and a mutable global holding one would be a data race between the UI and
// audio threads — the very thing the deferred-resize and seqlock machinery below
// exists to avoid. A parameter is also compiler-checked: a missed call site fails
// to build instead of silently driving the wrong rack.
//
// Design + the rejected alternatives: docs/design/engine-instance-seam.md
// ============================================================================
typedef struct DeInstance DeInstance;

// Bring up one engine instance: decode/bind assets, sound_init(), run the cart's
// init(). `renderer` selects the path; a SOFTWARE backend needs no GPU context.
// Returns NULL if the instance cannot be created.
DeInstance *de_instance_create(DeRenderer renderer);

// Tear one down. Safe to call with NULL.
void de_instance_destroy(DeInstance *in);

// Advance and render ONE frame. `t` = seconds since launch (the host's clock —
// CACurrentMediaTime on iOS, GetTime on desktop). Runs input drain → update() →
// draw() into the active renderer. After this returns in SOFTWARE mode the frame
// is complete in de_framebuffer(); the host blits it.
void de_frame(DeInstance *in, double t);

// SOFTWARE renderer: pointer to the finished frame, RGBA8888, de_screen_w()*
// de_screen_h() pixels, row-major and BOTTOM-UP — row 0 is the BOTTOM of the picture, so the
// blit must flip Y (iOS does it with a row-wise copy, Android in the GLES shader). This line
// said "top-left origin" until 2026-08-14, which is upside down and cost a new backend author
// their first render; the store it describes is studio.c:777, `(de_sh - 1 - sy) * fb_w + sx`.
// Returns NULL under DE_RENDERER_GPU.
// ONLY SAFE ON THE THREAD THAT CALLS de_frame — it is the engine's live canvas, not a snapshot.
const uint32_t *de_framebuffer(DeInstance *in);

// The same frame, for a host that blits from a DIFFERENT thread than the one calling de_frame — an
// AUv3, where the render block drives the frame on the audio thread while the view draws on main.
// Copies the last COMPLETED frame into `dst` (same layout as de_framebuffer) and returns 1, setting
// *w/*h. Returns 0 if no frame exists yet or `cap_px` is too small — *w/*h still report the size
// needed, so grow the buffer and call again. Internally a seqlock: it may briefly retry, and it never
// makes the audio thread wait. A single-threaded host can keep using de_framebuffer().
int de_copy_frame(DeInstance *in, uint32_t *dst, int cap_px, int *w, int *h);

// Active framebuffer dimensions (== SCREEN_W / SCREEN_H at boot; a resizable cart
// grows past them via de_resize — always read these, never the compile-time macros).
int de_screen_w(DeInstance *in);
int de_screen_h(DeInstance *in);

// Device-adaptive (Phase 2): the host hands the engine the device viewport so a resizable cart
// reflows to fill the screen. de_resize reallocs the framebuffer + republishes de_screen_w/h; call
// it when the view's bounds change (incl. rotation). Only act on it when de_is_resizable() is true —
// a fixed cart returns 0 and stays letterboxed at its compile-time size.
void de_resize(DeInstance *in, int w, int h);
int  de_is_resizable(DeInstance *in);
// Safe-area insets (px) — notch / home-bar / status bar. The host reports them; a cart reads the
// usable rect via safe_rect() and keeps its controls inside it (background can still bleed full).
void de_set_safe_area(DeInstance *in, int left, int top, int right, int bottom);
// Backing scale — points per logical canvas px (iOS pixelChunk, e.g. 2). Feeds finger_px() so a
// finger control is sized physically, not by a raw-px coincidence. Host reports it; default 2.
void de_set_backing_scale(DeInstance *in, float k);
// Persistence root — the host's writable app-private dir (Android internalDataPath, iOS Documents).
// save()/save_int()/save_bytes() write under here; call BEFORE de_instance_create() so a cart's init() can
// load_int(). Twin of the desktop --save-dir flag; unset it defaults to "." (the cwd), where a
// sandboxed OS can't write, so saves silently no-op.
void de_set_save_dir(DeInstance *in, const char *dir);

// SESSION STATE — back the host's fullState with these, so a reopened project is the rack the player
// left rather than factory defaults. What travels is INTENT (the sound config log + the cart slices
// marked with de_state_for_saved), NOT the ~4 MB context struct, which is mostly pointers and DSP
// scratch.
//
// WHAT A RESTORE GUARANTEES, stated narrowly because the loose version misleads: the ENGINE comes back
// with NO HELD VOICES and NOTHING SCHEDULED (de_load_state replays the config over a reset, so a stale
// voice handle can never be pointed at a live slot). It says nothing about where a CART's sequencer
// sits — that depends on the cart. A cart storing its step in a SAVED slice gets it back; one that
// derives its position from host transport (as a well-behaved plug-in cart does — see
// docs/design/external-clock-sync.md) is put wherever the host's playhead is, one frame later,
// whatever the blob said.
//
// de_save_state: writes at most `max` bytes into `out` and returns the length written. Pass out=NULL
// (or too small a `max`) to get the REQUIRED size back without writing — call it twice, size then fill.
// Call on any thread that is not driving de_frame; it only reads.
// HOST PARAMETERS (runtime/param.h) — the knobs a DAW can see, automate and record. A cart binds
// floats it already owns (param_bind), so the parameter IS the knob: a host write and a finger drag
// land in the same place, which is why "the panel follows automation" and "a drag shows up in the
// host's lane" need no sync path at all.
//   de_param_count / de_param_info  build the host's parameter tree ONCE, at init, by index.
//   de_param_get                    read a live value (unsynchronised by design — worst case one
//                                   frame stale, which is what every plug-in meter already is).
//   de_param_set                    QUEUED, applied at the top of the next de_frame. Safe from any
//                                   thread, which is the point: a host writes automation from its
//                                   render thread and cart state must only be written on the frame
//                                   thread (the rule tools/input-ring-check exists to hold).
//   de_param_changed                POLL what the PANEL moved, so a host lane follows the glass.
//                                   Loop until it returns 0. A host write does NOT come back out of
//                                   here, or an automation lane would fight its own value.
// ⚠ `addr` is the cart's own id and is FOREVER — a saved project's automation stores nothing else.
int   de_param_count(DeInstance *in);
int   de_param_info(DeInstance *in, int i, int *addr, const char **name, float *lo, float *hi, float *def);
float de_param_get(DeInstance *in, int addr);
void  de_param_set(DeInstance *in, int addr, float v);
int   de_param_changed(DeInstance *in, int *addr, float *v);

int de_save_state(DeInstance *in, void *out, int max);
// de_load_state: 1 = ACCEPTED · 2 = ACCEPTED WITH MIGRATION · 0 = REFUSED.
//
// MIGRATION is what stops an app update from throwing away everyone's saved songs. A slice that GREW
// since the blob was written (you added a knob) is restored as a PREFIX — the saved bytes land, the new
// fields keep their template defaults — and the call returns 2 so the host can say so. A slice that
// SHRANK, a different saved-slice set, a foreign ABI, or a version from the future are all REFUSED, and
// refused means the rack stays at defaults rather than being filled with mismatched bytes. Nothing is
// ever half-applied.
//
// ⚠ The one thing this CANNOT check is a REORDER or RETYPE of existing fields: the size does not move,
// so every runtime test passes and every value lands in the wrong field. That is enforced at build time
// by tools/lint-saved-state.js, whose rule is APPEND-ONLY. Runtime migration and that lint are one
// design — neither is sufficient alone.
// ⚠ The blob is copied and applied at the top of the NEXT de_frame, not inline: the host sets state
// on its own thread while the frame worker runs, and the cart's state belongs to de_frame. So a
// return of 1 means "accepted", and the rack changes one frame later. `blob` is yours again on return.
int de_load_state(DeInstance *in, const void *blob, int len);

// ============================================================================
// (2) AUDIO — pulled by the host's audio backend (CoreAudio on iOS, Raylib's
//     AudioStream on desktop). Reentrant + lock-free: safe to call from the
//     audio thread while the main thread runs de_frame(). Fills `frames` stereo
//     interleaved float samples in [-1,1] at SOUND_SAMPLE_RATE (44100).
// ============================================================================
void de_audio_render(DeInstance *in, float *out, int frames);

// ============================================================================
// (3) INPUT — the host feeds events in (UIKit touches on iOS, controller on
//     Switch). Coordinates are in framebuffer pixels (0..screen_w, 0..screen_h).
//     [M3 — declared now so the seam is complete; wired after render+audio land.]
// ============================================================================
void de_touch_begin(DeInstance *in, int id, float x, float y);
void de_touch_moved(DeInstance *in, int id, float x, float y);
void de_touch_ended(DeInstance *in, int id, float x, float y);

// The PRIMARY finger also drives the mouse API (GetMousePosition/IsMouseButton*), so mouse-reading
// carts play from touch with no extra host work — same as a browser synthesizing mouse from touch.
// Keys have no touch equivalent; a host with a real/on-screen keyboard feeds them here (down=1/0).
void de_key_event(DeInstance *in, int key, int down);

// ============================================================================
// (4) AUDIO INPUT — the mirror of (2), inverted. The engine NEVER opens a capture
//     device; each host owns its mic (CoreAudio/AudioQueue desktop, AVAudioSession
//     iOS, getUserMedia web), runs the platform's permission prompt, and PUSHES
//     captured MONO frames in from its capture callback. The engine only ANALYZES
//     them (mic_level()/mic_pitch(), see mic.h). Lazy: a host opens/closes its
//     device based on de_mic_wanted(), so nothing captures — and no permission
//     prompt fires — until a cart calls mic_start().
// ============================================================================
void de_audio_input(const float *mono, int n, int sample_rate);  // host → engine: captured frames
int  de_mic_wanted(void);        // engine → host: does the running cart want the mic on?
int  de_midi_wanted(void);       // engine → host: has the cart read MIDI input? (web asks for MIDI permission only when so — same lazy opt-in as the mic)
void de_mic_set_active(int on);  // host → engine: capture is live + permission granted (drives mic_active())

#ifdef __cplusplus
}
#endif

#endif // DE_PLATFORM_H
