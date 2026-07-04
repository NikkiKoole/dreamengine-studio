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

// One-time engine bring-up: decode/bind assets, sound_init(), run the cart's
// init(). `renderer` selects the path; a SOFTWARE backend needs no GPU context.
// Safe to call exactly once before the first de_frame().
void de_init(DeRenderer renderer);

// Advance and render ONE frame. `t` = seconds since launch (the host's clock —
// CACurrentMediaTime on iOS, GetTime on desktop). Runs input drain → update() →
// draw() into the active renderer. After this returns in SOFTWARE mode the frame
// is complete in de_framebuffer(); the host blits it.
void de_frame(double t);

// SOFTWARE renderer: pointer to the finished frame, RGBA8888, de_screen_w()*
// de_screen_h() pixels, row-major top-left origin. Returns NULL under DE_RENDERER_GPU.
const uint32_t *de_framebuffer(void);

// Active framebuffer dimensions (== SCREEN_W / SCREEN_H at boot; a resizable cart
// grows past them via de_resize — always read these, never the compile-time macros).
int de_screen_w(void);
int de_screen_h(void);

// Device-adaptive (Phase 2): the host hands the engine the device viewport so a resizable cart
// reflows to fill the screen. de_resize reallocs the framebuffer + republishes de_screen_w/h; call
// it when the view's bounds change (incl. rotation). Only act on it when de_is_resizable() is true —
// a fixed cart returns 0 and stays letterboxed at its compile-time size.
void de_resize(int w, int h);
int  de_is_resizable(void);

// ============================================================================
// (2) AUDIO — pulled by the host's audio backend (CoreAudio on iOS, Raylib's
//     AudioStream on desktop). Reentrant + lock-free: safe to call from the
//     audio thread while the main thread runs de_frame(). Fills `frames` stereo
//     interleaved float samples in [-1,1] at SOUND_SAMPLE_RATE (44100).
// ============================================================================
void de_audio_render(float *out, int frames);

// ============================================================================
// (3) INPUT — the host feeds events in (UIKit touches on iOS, controller on
//     Switch). Coordinates are in framebuffer pixels (0..screen_w, 0..screen_h).
//     [M3 — declared now so the seam is complete; wired after render+audio land.]
// ============================================================================
void de_touch_begin(int id, float x, float y);
void de_touch_moved(int id, float x, float y);
void de_touch_ended(int id, float x, float y);

// The PRIMARY finger also drives the mouse API (GetMousePosition/IsMouseButton*), so mouse-reading
// carts play from touch with no extra host work — same as a browser synthesizing mouse from touch.
// Keys have no touch equivalent; a host with a real/on-screen keyboard feeds them here (down=1/0).
void de_key_event(int key, int down);

#ifdef __cplusplus
}
#endif

#endif // DE_PLATFORM_H
