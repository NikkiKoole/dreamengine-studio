#define STUDIO_INTERNAL          // suppress studio.h's KEY_* macros — raylib.h provides those names
#if defined(_WIN32) && !defined(DE_NO_RAYLIB)
// net.h (lockstep netplay) uses Winsock, whose winsock2.h MUST precede windows.h
// or the old winsock v1 baked into windows.h collides. winsock2.h pulls windows.h,
// though — and its GDI/USER symbols (Rectangle, LoadImage, DrawText, CloseWindow,
// ShowCursor) + the near/far/min/max macros clash with raylib.h + studio.h. We
// only need Winsock here (raylib is precompiled), so strip windows.h down with the
// NO* guards and undo the legacy near/far macros. See docs/design/multiplayer-research.md.
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#undef near
#undef far
#endif
#include "studio.h"
#ifdef DE_NO_RAYLIB
#include "raylib_compat.h"   // the no-Raylib shim (iOS / Switch / headless software build)
#else
#include "raylib.h"
#include "rlgl.h"      // immediate-mode triangles for tritex()
#endif
#include "color.h"
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>                       // _mkdir (Windows mkdir takes no mode arg)
#define de_mkdir(p) _mkdir(p)
#else
#define de_mkdir(p) mkdir((p), 0755)
#endif
#include "dos_8x8_font.h"
#include "font4x6_data.h"
#include "font3x5_data.h"
#include "fontcomic10x20_data.h"
#include "fontthin8x8_data.h"
#include "sprites_data.h"
#include "map_data.h"
#include "sound.h"
#include "midi_input.h"
#include "game_rect.h"   // window↔canvas placement transform (touch-controls Phase 1.5 chokepoint)

// where the canvas sits in the window + the single window↔canvas transform (see game_rect.h).
// Phase 1.5 pins it to the full window (origin 0,0; scale = SCALE) → identity, byte-identical to
// the old bare /SCALE. Phase 2 placement just assigns a different value here and coords follow.
// Declared up here because the input readers (inp_mouse_x/y, below) are the first to use it.
static GameRect game_rect = { 0.0f, 0.0f, (float)SCALE };

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#include <GLES2/gl2.h>   // for our own glReadPixels in the pget snapshot — dodges raylib's ES3-only format probe (WebGL1 INVALID_ENUM)
#endif

// render cadence (Lever A-lite, keeps phones cool): present only every RENDER_EVERY-th
// tick. update()/sound_tick()/input still run every tick, so logic+audio+input stay
// full-rate; only the GPU present drops to displayHz/RENDER_EVERY. 1 = present every tick
// (default, unchanged). Web-only — native pacing lives in EndDrawing (SetTargetFPS), so
// skipping the present there would un-cap update(). See docs/design/frame-pacing.md.
#ifndef RENDER_EVERY
#define RENDER_EVERY 1
#endif

// ------------------------------------------------------------
// DE_TCC: load the cart as a libtcc-JIT'd module instead of static-linking it.
// studio.c becomes a persistent host; the cart's init/update/draw are resolved at
// runtime and the whole studio API is handed to the cart via tcc_add_symbol. This
// is the "cart-as-script" / hot-reload path. A normal build never sees any of it.
// ------------------------------------------------------------
#ifdef DE_TCC
#include "libtcc.h"
#include "studio_tcc_symbols.h"   // generated: DE_TCC_API_SYMBOLS(X) over studio.h
#include <sys/stat.h>
#ifndef DE_TCC_INCDIR
#define DE_TCC_INCDIR "."          // where studio.h lives (override via -D)
#endif
#ifndef DE_TCC_LIBDIR
#define DE_TCC_LIBDIR ""           // tcc runtime/config dir (libtcc1.a)
#endif

static TCCState *cart_state = NULL;
static void (*cart_init)(void)   = NULL;
static void (*cart_update)(void) = NULL;
static void (*cart_draw)(void)   = NULL;
static char cart_path_buf[1024] = "";   // path of the loaded cart (for file-watch)
static long long cart_mtime = 0;        // its mtime at load time (nanoseconds)
// de_state() (the persistent cart-state block) is now a first-class studio.h API,
// implemented unconditionally below and exposed to carts via the generated symbol
// table — so it works under every backend, not just DE_TCC.

// Nanosecond resolution: whole-second mtime missed sub-second rewrites — an edit + Run
// within the same second as the prior load kept cart_mtime equal, so the hot-reload
// silently no-op'd while the editor still logged "↻ live reload" (ran old code).
static long long file_mtime(const char *p) { struct stat st; return stat(p, &st) == 0 ? (long long)st.st_mtimespec.tv_sec * 1000000000LL + st.st_mtimespec.tv_nsec : 0; }

static void cart_tcc_error(void *u, const char *msg) { (void)u; fprintf(stderr, "[tcc] %s\n", msg); }

// (Re)compile the cart at `path` into memory and rebind the entry points. Returns
// 0 on success; on failure the previously loaded cart (if any) stays live, so a
// hot-reload with a syntax error doesn't kill the running game.
static int cart_load(const char *path) {
    double _t0 = GetTime();
    TCCState *s = tcc_new();
    if (!s) return -1;
    tcc_set_error_func(s, NULL, cart_tcc_error);
    if (DE_TCC_LIBDIR[0]) tcc_set_lib_path(s, DE_TCC_LIBDIR);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tcc_add_include_path(s, DE_TCC_INCDIR);
    // screen geometry normally arrives as -D flags at cart compile time
    { char b[32];
      snprintf(b, sizeof b, "%d", SCREEN_W); tcc_define_symbol(s, "SCREEN_W", b);
      snprintf(b, sizeof b, "%d", SCREEN_H); tcc_define_symbol(s, "SCREEN_H", b);
      snprintf(b, sizeof b, "%d", SCALE);    tcc_define_symbol(s, "SCALE",    b); }
    // expose the entire studio API to the cart
    #define X(n) tcc_add_symbol(s, #n, (const void *)n);
    DE_TCC_API_SYMBOLS(X)   // includes de_state (a studio.h API) — no manual add needed
    #undef X
    if (tcc_add_file(s, path) < 0) { tcc_delete(s); return -1; }
    if (tcc_relocate(s)       < 0) { tcc_delete(s); return -1; }
    void (*ci)(void) = (void (*)(void))tcc_get_symbol(s, "init");
    void (*cu)(void) = (void (*)(void))tcc_get_symbol(s, "update");
    void (*cd)(void) = (void (*)(void))tcc_get_symbol(s, "draw");
    if (!cd) { fprintf(stderr, "[tcc] cart '%s' defines no draw()\n", path); tcc_delete(s); return -1; }
    if (cart_state) tcc_delete(cart_state);   // free the old module (hot reload)
    cart_state = s; cart_init = ci; cart_update = cu; cart_draw = cd;
    snprintf(cart_path_buf, sizeof cart_path_buf, "%s", path);
    cart_mtime = file_mtime(path);
    fprintf(stderr, "[tcc] compiled %s in %.2f ms\n", path, (GetTime() - _t0) * 1000.0);
    return 0;
}

// Called once per frame: if the cart file changed on disk, recompile and hot-swap
// the entry points — WITHOUT re-running init(), so de_state (and thus game state)
// carries across the reload. A compile error leaves the running cart untouched.
static void key_claims_reset(void);   // defined with the pause state below
static void cart_reload_if_changed(void) {
    if (!cart_path_buf[0]) return;
    long long m = file_mtime(cart_path_buf);
    if (!m || m == cart_mtime) return;
    char path[1024]; snprintf(path, sizeof path, "%s", cart_path_buf);
    if (cart_load(path) == 0) {
        fprintf(stderr, "[tcc] hot-reloaded %s\n", path);
        key_claims_reset();   // the new cart re-claims whatever it reads
    }
    else                      cart_mtime = m;   // don't re-attempt the broken file every frame
}
#endif // DE_TCC

// ------------------------------------------------------------
// internal state
// ------------------------------------------------------------

// 64 since the palette experiment (palette-and-color.md Layer 1b): slots 32-63
// default to a MIRROR of 0-31, so every existing cart renders byte-identically
// (color % 64 lands on the copy; pget/shader nearest-match scan low-first).
// Only palette_hex() writes the upper half today. The named CLR_* stay 0-31.
#define PALETTE_SIZE 64
#define SPRITE_SIZE  16
#define SPRITE_COUNT 64   // 8×8 grid of 16×16 sprites = 128×128 sheet
#ifndef SCALE
#define SCALE 4
#endif

static Texture2D       spritesheet;
static Image           spritesheet_img = {0};
#ifdef SPRITES_MULTI
// per-cart sprite sheets (multi-cart bundles, from build-app.js), pre-loaded at init and
// indexed by de_switch_cart ctx; de_sheet_select() swaps `spritesheet`/`spritesheet_img`.
static Image           sheet_imgs[SPRITES_MULTI] = {0};
#ifndef DE_NO_RAYLIB
static Texture2D       sheet_texs[SPRITES_MULTI] = {0};
#endif
#endif
static void de_sheet_select(int ctx);   // fwd: swap the active sprite sheet on a cart switch
static RenderTexture2D canvas;
static RenderTexture2D canvas_snap;   // scratch copy for zoom_rect (avoids sampling the live target)
static Font            game_font;
static Font            font_small = {0};
static Font            font_tiny  = {0};
static Font            font_comic = {0};
static Font            font_thin = {0};
static int             active_font_id = FONT_NORMAL;
// CPU copies of each font atlas (for the software-canvas glyph blit; filled at load via
// LoadImageFromTexture). Same index meaning as active_font_id via cur_font_img().
static Image           game_font_img = {0}, font_small_img = {0}, font_tiny_img = {0}, font_comic_img = {0}, font_thin_img = {0};
static bool            custom_font = false;
static DeColor           palette[PALETTE_SIZE];
static DeColor           base_palette[PALETTE_SIZE];   // pristine copy, for pal_reset()
// pal()-on-sprites: a palette-swap shader. Sprite texels are exact palette RGBs,
// so the shader finds each texel's base-palette index and outputs the (possibly
// remapped) current palette color — reproducing pal() for spr()/sspr() just like
// it already works for the shape primitives. Gated on pal_active so it costs
// nothing until a swap is live.
static Shader          pal_shader;
static bool            pal_shader_ok = false;
static int             loc_base_pal  = -1;
static int             loc_cur_pal   = -1;
static bool            pal_active    = false;          // any palette[i] != base_palette[i]?
static float           cur_pal_rgb[PALETTE_SIZE * 3];  // current palette, normalized — uploaded to the shader
static bool            cur_pal_dirty = true;
// runtime colorkey() on the software canvas: the GPU path bakes the keyed colour into the
// `spritesheet` texture (alpha 0); the canvas samples the pristine `spritesheet_img`, so it
// must skip the key itself. Snapshot the RGB at colorkey() time (matches when the GPU bakes it).
static bool            sw_colorkey_on  = false;
static DeColor           sw_colorkey_rgb = {0};

// present-scaling filter, picked in settings → "scaling" (machine-local, -D flag):
//   0 crisp (nearest, default) · 1 bilinear (smooth) · 2 sharp-bilinear · 3 sharp+gamma
// modes 2/3 use scale_shader at the final blit; only matters at NON-integer scale
// (web viewport fit / resizable / fullscreen) — at integer SCALE all look crisp.
#ifndef SCALE_FILTER
#define SCALE_FILTER 0
#endif
static Shader          scale_shader;
static bool            scale_shader_ok   = false;
static int             loc_scale_texsize = -1;
static int             loc_scale_gamma   = -1;

// smooth_zoom(): render the camera_ex world layer at 1:1 into an oversized offscreen,
// then scale it to the canvas in one blit (sharp-bilinear) — so a fractional camera
// zoom no longer re-rasterizes thin world lines every frame (no crawl). The offscreen
// is 2× the canvas so any zoom >= 0.5 has coverage. Toggle with one call; default off.
// the smooth-zoom supersample offscreen is 2× the framebuffer. Keyed off the runtime fb_w/fb_h so it
// tracks a resizable cart's grown canvas (== SCREEN_W/H*2 for a fixed cart → unchanged). Expanded at
// use sites (all after fb_w's declaration); smooth_rt is reallocated to match in de_grow_gpu.
#define SMOOTH_W (fb_w * 2)
#define SMOOTH_H (fb_h * 2)
static RenderTexture2D  smooth_rt;
static bool            smooth_on        = false;   // smooth_zoom() enabled?
static bool            smooth_rt_ok     = false;   // offscreen allocated?
static bool            smooth_capturing = false;   // mid-capture (between camera_ex and camera())
static float           smooth_zoom_amt  = 1.0f;    // the zoom to apply at composite
static float           fade_amt   = 0.0f;            // fade()  — 0 normal .. 1 black
static float           shake_amt  = 0.0f;            // shake() — pixels, self-decaying
static float           frame_dt   = 0.0f;            // dt()    — seconds since last frame
static double          last_time  = 0.0;
static char            text_buf[32] = {0};           // text_input() — chars typed this frame
static bool            fp_on      = false;            // fillp() global pattern active?
static int             fp_global  = 0;                // current fillp() pattern
static int             fp_hole    = -1;               // fillp() 1-bit color (-1 = transparent)
static int             fp_anc_x   = 0;                // fillp() lattice origin (world coords)
static int             fp_anc_y   = 0;                // — shifts the pattern phase; anchor to a
static bool            poly_fill_fast = true;        // false → legacy per-pixel polygon fill (A/B; env DE_POLY_FILL=legacy)
static bool            disc_fill_fast = true;        // false → legacy per-pixel circle/oval fill (A/B; env DE_DISC_FILL=legacy)
static bool            clamp_cache_on = true;        // false → recompute the fill scan-box every call (A/B; env DE_CLAMP_CACHE=off)
static bool            blit_fast_on   = true;        // false → legacy per-pixel sw_blit (A/B; env DE_BLIT_FAST=off). Fast path: direct uint32 row-copy for the axis-aligned, unscaled, unflipped, zoom==1, no-pal blit
static bool            tritex_fast_on = true;        // false → legacy sw_tritex (A/B; env DE_TRITEX_FAST=off). Fast path: bbox clamped to the visible region + hoisted row-write plot (zoom==1 canvas)
// NB: a branchless masked-blend inner loop was tried here (write EVERY pixel, blend by an alpha
// mask, no per-pixel branch → auto-vectorizable) and MEASURED 1.5× SLOWER on bunnymark (37 vs 24
// ms/frame, bit-identical). It's a TRADE, not a dead end — which side wins depends on sprite
// density:
//   • SPARSE sprites (mostly-transparent cells: bunnies, glyphs, most game art) → the transparent-
//     SKIP below WINS — it touches no memory for the skipped pixels; the blend's read-modify-write
//     of the whole cell costs more than the branch + SIMD saves. This is the common case → keep skip.
//   • DENSE / mostly-OPAQUE sprites (full tiles, backgrounds, fullscreen images) → the blend would
//     likely win (few pixels skipped, so the wasted writes aren't wasted, and it vectorizes).
// So if a future workload is opaque-blit-bound, revisit — ideally as a per-blit choice keyed on
// opacity, not a global swap. Full write-up: docs/design/ios-profiling.md "Optimization attempts".
#ifndef DE_BATCH_PSET_DEFAULT
#define DE_BATCH_PSET_DEFAULT 0                        // web has no env vars → compile-time toggle (-DDE_BATCH_PSET_DEFAULT=1)
#endif
static bool            pset_batch     = DE_BATCH_PSET_DEFAULT; // true → batched pset (skip DrawPixel's per-pixel rlSetTexture toggle) (A/B; env DE_BATCH_PSET=on, or -DDE_BATCH_PSET_DEFAULT=1 for web)
// --- software canvas (Phase 0 probe): write pixels into a CPU RGBA buffer, upload once/frame with
// one UpdateTexture → no per-pixel rlVertex3f. A/B with env DE_SOFTWARE_CANVAS=on (mirrors
// DE_BATCH_PSET); -DSW_CANVAS_DEFAULT=1 for web. Plan: docs/design/software-canvas-phase0-plan.md.
#ifndef SW_CANVAS_DEFAULT
#define SW_CANVAS_DEFAULT 0
#endif
static bool            sw_canvas_enabled = SW_CANVAS_DEFAULT; // env DE_SOFTWARE_CANVAS=on (the master toggle)
static bool            sw_canvas_active   = SW_CANVAS_DEFAULT; // PER-FRAME: enabled AND this cart hasn't used a zoom/rotation camera_ex. Primitives check this.
static bool            sw_force_gpu     = false;            // sticky: cart used camera_ex with zoom!=1 or angle!=0 → that cart falls back to the GPU path (Fork-2/C)
// DE_CPU_RASTER (default off): route the primitives that GL rasterizes DIFFERENTLY from the software
// canvas through the SAME CPU rasterizer even off-canvas — `line()`/`bezier`/2-pt poly (→ de_cpu_line)
// and `rectfill_rot` (→ the inverse-map fill), via pset so they hit the GPU buffer too. Purpose is A/B
// hygiene: with it set on BOTH the GPU and software-canvas builds, those primitives draw the SAME
// pixels on each, so a canvas A/B isn't tripped up by GL-vs-CPU rasterization diffs (the line/rotated-
// edge noise). Grows as more rotated primitives port. NOT a direction-1 commitment (the coverage-vs-DDA
// line() decision is still open): docs/design/{software-canvas,rasterization-consistency}.md.
#ifndef CPU_RASTER_DEFAULT
#define CPU_RASTER_DEFAULT 0
#endif
static bool            cpu_raster_enabled = CPU_RASTER_DEFAULT;   // env DE_CPU_RASTER=on
// DE_AUDIO=off — skip audio entirely (no InitAudioDevice/sound_init, no miniaudio callback thread).
// The effects bus (reverb tail, set-and-hold inserts) processes the master every callback even on
// silence, so a silent or sound-free cart still pays for it; this removes it. Wins: cleaner CPU
// profiles (the audio thread stops polluting `sample`), real savings on sound-free carts, a low-end
// lever. Cart audio calls become harmless no-ops (queues are never drained). Default on.
static bool            audio_off = false;
// CPU framebuffer (Fork 1: RGBA on desktop). Phase 1b(B): a HEAP buffer sized to fb_w×fb_h so a
// resizable cart can GROW it past the compile-time SCREEN_W/H (de_ensure_fb). Allocated at boot to
// SCREEN_W×SCREEN_H; a fixed cart never grows it, so fb_w==SCREEN_W and everything is byte-identical.
static uint32_t       *sw_cbuf = NULL;

// device-adaptive-layout.md: the PHYSICAL framebuffer dimensions — the row STRIDE for every sw_cbuf
// index. Grows (grow-only, high-water-mark) when a resizable cart's active size exceeds it; the
// active de_sw×de_sh region sits at the buffer's bottom-left and the present samples that sub-rect.
static int fb_w = SCREEN_W, fb_h = SCREEN_H;

// device-adaptive-layout.md Phase 1a: the ACTIVE canvas dims are a runtime value, read wherever a
// render EXTENT / centering / clip / camera / present rect is computed. SCREEN_W/SCREEN_H are the
// cart's DEFAULT/boot size (and the fb's initial size). Pinned for a fixed cart → byte-identical;
// a `resizable` cart updates them (reflow) + grows the fb to fit. Outside DE_NO_RAYLIB — all builds.
static int de_sw = SCREEN_W, de_sh = SCREEN_H;
// device-adaptive-layout.md Phase 1b: the per-cart RESIZABLE opt-in. A cart compiled with
// -DDE_RESIZABLE (clang builds cart.c + studio.c together, so the one flag reaches both TUs)
// gets a resizable window AND de_sw/de_sh that reflow live to (window / SCALE) on every resize —
// so lay.h re-flows against the real screen_w()/screen_h(). OFF (every existing cart): the globals
// are pinned to the boot size forever and the whole engine behaves byte-identically to pre-1b.
// Reflow is thus opt-in per rack (the determinism guard) — the phasing lever, no "flag day".
#ifdef DE_RESIZABLE
static const bool de_reflow = true;
#else
static const bool de_reflow = false;
#endif
// cart-facing live canvas size (Phase 1b). SCREEN_W/H are the compile-time MAX; these are the
// ACTIVE dims, which a resizable cart watches to reflow (lay.h). On a fixed cart they equal
// SCREEN_W/H forever. Defined unconditionally — carts run on every build.
int screen_w(void) { return de_sw; }   // active canvas width  in px (== SCREEN_W unless resizable)
int screen_h(void) { return de_sh; }   // active canvas height in px (== SCREEN_H unless resizable)
#ifdef DE_NO_RAYLIB
// Software CAMERA ROTATION (no GPU to fall back to — iOS/Switch). The world layer is captured
// into an offscreen buffer at zoom+translate (rotation NOT applied), then rotated about the
// screen centre into sw_cbuf at the camera-reset / present boundary. Exact for uniform zoom:
// screen = R·(worldbuf − centre) + centre. HUD drawn after a camera() reset goes straight to
// sw_cbuf, un-rotated. Primitives write sw_dst — the framebuffer, or the world buffer while a
// rotated camera_ex is active. (det-probes/rotfill is the per-primitive study; this is the
// whole-layer composite, which keeps every primitive on its fast axis-aligned path.)
static uint32_t       *sw_world_buf = NULL;   // heap, sized to fb_w×fb_h alongside sw_cbuf (grows with it)
static uint32_t       *sw_dst = NULL;         // → sw_cbuf (or sw_world_buf mid camera_ex); set at alloc time
static bool            sw_rot_active = false;
static float           sw_rot_angle  = 0.0f;
static void            sw_rot_composite(void);            // defined near camera(); called from de_frame()
#else
#define sw_dst sw_cbuf                                          // desktop/web: straight to the framebuffer (byte-identical)
#endif

// device-adaptive-layout.md: (re)allocate the CPU framebuffer(s) to at least need_w×need_h. Grow-only
// (high-water-mark) — never shrinks, so dragging a window smaller then bigger doesn't thrash — and
// clamped to DE_MAX_DIM so a runaway window can't allocate gigabytes. Called at boot with SCREEN_W/H
// and whenever a resizable cart's active size CHANGES (de_set_canvas). Reallocs to EXACTLY the active
// size (fb==de always) — see the note below on why not grow-only. A fixed cart allocates ONCE at boot
// and fb_w/fb_h stay SCREEN_W/H forever → byte-identical to the old static arrays.
#define DE_MAX_DIM 4096
static void de_grow_gpu(void);   // realloc the GPU canvas/canvas_snap to fb_w×fb_h (no-op on the SW-only build)
static void de_ensure_fb(int need_w, int need_h) {
    if (need_w < 1) need_w = 1; else if (need_w > DE_MAX_DIM) need_w = DE_MAX_DIM;
    if (need_h < 1) need_h = 1; else if (need_h > DE_MAX_DIM) need_h = DE_MAX_DIM;
    if (sw_cbuf && need_w == fb_w && need_h == fb_h) return;   // already EXACTLY this size
    // realloc the framebuffer to EXACTLY the active size — NOT grow-only. Grow-only leaves fb taller
    // than de after a shrink, and the GPU draws the cart at the buffer top while the present samples a
    // fixed-height sub-rect → the image slides up by (fb_h-de_sh). Keeping fb==de sidesteps all of it.
    fb_w = need_w; fb_h = need_h;
    size_t n = (size_t)fb_w * (size_t)fb_h * sizeof(uint32_t);
    sw_cbuf = (uint32_t *)realloc(sw_cbuf, n);
#ifdef DE_NO_RAYLIB
    sw_world_buf = (uint32_t *)realloc(sw_world_buf, n);
    sw_dst = sw_cbuf;
#endif
    de_grow_gpu();   // resize the GPU render targets to match (skips at boot before the canvas exists)
}
// set the ACTIVE canvas size (a reflow / resize): clamp to the safety max, grow the framebuffer to
// fit, then publish de_sw/de_sh. The one funnel every size change goes through (boot, reflow, sweep).
static void de_set_canvas(int w, int h) {
    if (w < 1) w = 1; else if (w > DE_MAX_DIM) w = DE_MAX_DIM;
    if (h < 1) h = 1; else if (h > DE_MAX_DIM) h = DE_MAX_DIM;
    de_ensure_fb(w, h);
    de_sw = w; de_sh = h;
}
#ifdef DE_NO_RAYLIB
static void de_grow_gpu(void) {}   // software-only build has no GPU render targets
#else
// resize the GPU render targets (canvas + canvas_snap) to EXACTLY fb_w×fb_h. No-op at boot (the
// canvas isn't created yet — its creation reads fb_w/fb_h directly) and whenever they already match
// fb. Called between frames from de_ensure_fb (never inside a BeginTextureMode), so unload/reload is
// safe. The fresh canvas starts black; the cart repaints it fully next frame.
static void de_grow_gpu(void) {
    if (canvas.id == 0) return;                                             // not created yet (boot)
    if (canvas.texture.width == fb_w && canvas.texture.height == fb_h) return;   // already exact
    UnloadRenderTexture(canvas);
    canvas = LoadRenderTexture(fb_w, fb_h);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
#if SCALE_FILTER == 1
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
#endif
    BeginTextureMode(canvas); ClearBackground(palette[0]); EndTextureMode();
    UnloadRenderTexture(canvas_snap);
    canvas_snap = LoadRenderTexture(fb_w, fb_h);
    SetTextureFilter(canvas_snap.texture, TEXTURE_FILTER_POINT);
    if (smooth_rt_ok &&   // resize the smooth-zoom supersample offscreen too (only if a cart allocated it)
        (smooth_rt.texture.width != fb_w * 2 || smooth_rt.texture.height != fb_h * 2)) {
        UnloadRenderTexture(smooth_rt);
        smooth_rt = LoadRenderTexture(SMOOTH_W, SMOOTH_H);
        SetTextureFilter(smooth_rt.texture, TEXTURE_FILTER_BILINEAR);
        smooth_rt_ok = (smooth_rt.id != 0);
    }
}
#endif
static inline uint32_t sw_pack(DeColor c) { return (uint32_t)c.r | ((uint32_t)c.g<<8) | ((uint32_t)c.b<<16) | 0xFF000000u; }
// internal patterned-fill helpers — the public fills call these when fillp() is on
static void rectfill_pat(int x, int y, int w, int h, int pattern, int c1, int c0);
static void circfill_pat(int x, int y, int radius, int pattern, int c1, int c0);
static void ovalfill_pat(int cx, int cy, int rx, int ry, int pattern, int c1, int c0);
static void plot_pat(int x, int y, int color);
static void poly_clamp_scan(int *x0, int *y0, int *x1, int *y1);   // fwd: circfill (below) clamps like the poly path
static void poly_fill_cov(const int *xy, int n, int color);
static void poly_stroke_cov(const int *xy, int n, int color);
// pal()-on-sprites helpers (defined in the graphics section, used earlier in main/pal)
static void pal_shader_init(void);
static void pal_recompute(void);
static void scale_shader_init(void);
static void scale_shader_ensure(void);
static void smooth_composite(void);

#define BTN_COUNT 8
static bool            btn_curr[2][BTN_COUNT];
static bool            btn_prev[2][BTN_COUNT];

static uint8_t         map_data[MAP_W * MAP_H];
static int             map_scale_factor = 1;   // map_scale() — integer zoom for map drawing
static int             frame_count      = 0;
#ifdef PLATFORM_WEB
static bool            web_started      = false;  // true after the user clicks to start
#endif

// The AudioWorklet backend (DE_AUDIO_WORKLET) runs its own AudioContext and never touches
// raylib's audio (raudio.c needs pthreads, which the WASM_WORKERS build doesn't link). So
// in that build, raylib-audio calls compile out — and master volume (the pause-mute) is
// routed through the worklet mixer's own gain instead. See design/audio-threading.md §4.
#ifdef DE_AUDIO_WORKLET
#define de_master_volume(v) sound_set_master_gain(v)
#else
#define de_master_volume(v) SetMasterVolume(v)
#endif

// pause key — default P; overridden by -DPAUSE_KEY=<raylib keycode> from the editor's settings → controls
#ifndef PAUSE_KEY
#define PAUSE_KEY KEY_P
#endif

// pause overlay — P/ENTER opens it; ESC or selecting Continue closes it
static bool  pause_active = false;
static int   pause_sel    = 0;    // 0 = Continue, 1 = Restart
static char **restart_argv = NULL;

// EXPERIMENTAL (not committed API): --data <file> lets a data-driven cart load a
// JSON blob at runtime (parse it with runtime/json.h) instead of baking geometry
// into its source. de_data_path() returns the path, falling back to $DE_DATA so a
// cart is testable via env without threading the flag through every harness. If the
// data-cart experiment is dropped, delete this + the --data line + the studio.h decl.
static const char *de_data_path_v = NULL;
// EXPERIMENTAL companion: drag a file onto the window → de_dropped_file() returns its
// path for that one frame (captured each tick before update()). Lets a data cart reload
// a new JSON by drag & drop. de_open_path() reveals a folder in Finder/Explorer so you
// can find where the data lives. Same revert story as --data above.
static char de_drop_buf[1024];
static int  de_drop_valid = 0;

// keys the cart reads via key()/keyp()/keyr() are "claimed" — the pause
// hotkey skips them, so a cart using the whole keyboard (sh101's two-manual
// piano takes P) doesn't fight the overlay. Claims are sticky per cart run.
#define KEY_CLAIM_MAX 512
static bool key_claimed[KEY_CLAIM_MAX];
static void key_claim(int k) { if (k >= 0 && k < KEY_CLAIM_MAX) key_claimed[k] = true; }
#ifdef DE_TCC
static void key_claims_reset(void) { memset(key_claimed, 0, sizeof key_claimed); }
#endif

// ------------------------------------------------------------
// debug harness — deterministic clock + input record/replay + trace.
// Off by default and entirely native: a normal run touches none of this.
// Enabled by CLI flags parsed in main(): --det, --record, --replay,
// --script, --trace, --frames, --dump/--dump-every. See loop_step().
// ------------------------------------------------------------
static int raylib_mouse_button(int button);   // defined in the mouse api section
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// normalize a public MOUSE_* button to a 0/1/2 index (matches raylib_mouse_button)
static int mbtn_index(int button) {
    return button == MOUSE_RIGHT ? 1 : button == MOUSE_MIDDLE ? 2 : 0;
}

#ifdef PLATFORM_WEB
// web touch→mouse synthesis. Emscripten's GLFW shim emulates the mouse from
// touches through a primaryTouchId LATCH (libglfw.js): touchstart only fires
// mousedown when the latch is free, and only the primary's own touchend/cancel
// clears it. iOS Safari is known to DROP touchcancel (WebKit bug 153064 —
// system edge-swipes, gesture takeovers), and then the latch sticks forever:
// every later tap is ignored, "touch stops working" until reload. Found on
// iPad day one (2026-06-06), tap-as-mouse carts only.
// Same medicine as the touch mirror: once a real touch is seen, this device's
// "mouse" is an emulation — synthesize it from the self-healing mirror instead
// (vt pool, rebuilt every event) and never read GLFW's mouse again.
static bool    web_tm_active = false;  // latched on first real touch
static bool    web_tm_down = false, web_tm_prev = false;
static int     web_tm_id   = -1;       // sticky primary finger
static Vector2 web_tm_pos  = { 0, 0 }; // window pixels; persists after release
#endif

#ifndef PLATFORM_WEB
#define DET_DT      (1.0 / 60.0)         // fixed timestep when det_mode is on
#define KEYSTATE_N  512                  // raylib MAX_KEYBOARD_KEYS

static bool    det_mode      = false;    // fixed dt + seeded RNG -> reproducible runs
static double  det_clock     = 0.0;      // synthetic now() seconds, advances DET_DT/frame

static bool    inject_input  = false;    // drive input from replay_ev instead of the hardware
static unsigned char key_inject[KEYSTATE_N];       // 1 = key down this frame
static unsigned char key_inject_prev[KEYSTATE_N];  // previous frame (for press edges)
static int           mouse_inj_x = 0, mouse_inj_y = 0;   // injected canvas-space pointer
static unsigned char mbtn_inj[3], mbtn_inj_prev[3];      // injected mouse buttons (L/R/M)
static float         wheel_inj = 0.0f;                   // injected wheel delta (transient: this frame only)

static FILE   *rec_file       = NULL;    // --record: tagged "<frame> k|m|b ..." per change
static unsigned char key_rec_prev[KEYSTATE_N];     // last recorded down-state per key
static int           mouse_rec_px = -1, mouse_rec_py = -1;  // last recorded pointer
static unsigned char mbtn_rec_prev[3];             // last recorded button states

// kind: 0 = key (a=keycode, b=down), 1 = mouse-move (a=x, b=y),
//       2 = mouse-button (a=button index 0/1/2, b=down), 3 = mouse-wheel (a=delta ticks)
typedef struct { int frame; int kind; int a; int b; } InputEvent;
static InputEvent *replay_ev  = NULL;    // sorted by (frame, kind)
static int         replay_n   = 0;
static int         replay_i   = 0;       // next event to apply

static FILE   *trace_file     = NULL;    // --trace: one JSONL line of watch() state per frame
static int     dump_every     = 0;       // --dump-every: 0 = off, else export every Nth frame
static char    dump_dir[256]  = {0};     // --dump: directory for filmstrip PNGs
static int     max_frames     = 0;       // --frames: stop after N frames (0 = run until close)
// --resize "WxH,WxH,…": a scripted canvas-size SWEEP for testing device-adaptive layouts
// (device-adaptive-layout.md). Each size is held RESIZE_HOLD frames (so the reflow + a little
// animation settle), then one PNG cropped to the ACTIVE region is dumped, named by size — a
// deterministic "resize → look → resize → look" filmstrip. Best paired with -DDE_RESIZABLE.
#define RESIZE_HOLD 8
static int     resize_w[16], resize_h[16], resize_n = 0;
static int     resize_last_seg = -1;

// synthetic clock: deterministic runs read frame-derived time, not the wall clock
static double clk(void) { return det_mode ? det_clock : GetTime(); }

// input indirection — every key()/keyp()/btn()/mouse_*() read funnels through these
// so a replay/script can inject state and a recorder can observe it.
static bool inp_down(int k) {
    if (inject_input) return (k >= 0 && k < KEYSTATE_N) ? key_inject[k] != 0 : false;
    return IsKeyDown(k);
}
static bool inp_pressed(int k) {
    if (inject_input)
        return (k >= 0 && k < KEYSTATE_N) ? (key_inject[k] && !key_inject_prev[k]) : false;
    return IsKeyPressed(k);
}
static bool inp_released(int k) {
    if (inject_input)
        return (k >= 0 && k < KEYSTATE_N) ? (!key_inject[k] && key_inject_prev[k]) : false;
    return IsKeyReleased(k);
}
static int inp_mouse_x(void) {
    if (inject_input) return clampi(mouse_inj_x, 0, de_sw - 1);
    return clampi(gr_win_to_canvas_x(game_rect, GetMousePosition().x), 0, de_sw - 1);
}
static int inp_mouse_y(void) {
    if (inject_input) return clampi(mouse_inj_y, 0, de_sh - 1);
    return clampi(gr_win_to_canvas_y(game_rect, GetMousePosition().y), 0, de_sh - 1);
}
static bool inp_mouse_down(int button) {
    int b = mbtn_index(button);
    if (inject_input) return mbtn_inj[b] != 0;
    return IsMouseButtonDown(raylib_mouse_button(button));
}
static bool inp_mouse_pressed(int button) {
    int b = mbtn_index(button);
    if (inject_input) return mbtn_inj[b] && !mbtn_inj_prev[b];
    return IsMouseButtonPressed(raylib_mouse_button(button));
}
static bool inp_mouse_released(int button) {
    int b = mbtn_index(button);
    if (inject_input) return !mbtn_inj[b] && mbtn_inj_prev[b];
    return IsMouseButtonReleased(raylib_mouse_button(button));
}
static float inp_mouse_wheel(void) {
    if (inject_input) return wheel_inj;
    return GetMouseWheelMove();
}
#else
// web build: harness is a no-op. Mouse reads go straight to raylib UNTIL a
// real touch is seen — from then on the mouse is synthesized from the touch
// mirror (web_tm_*), bypassing GLFW's stuck-latch touch emulation (see the
// web_tm_* block above). Touch devices only ever have a LEFT button.
static double clk(void) { return GetTime(); }
static bool inp_down(int k)    { return IsKeyDown(k); }
static bool inp_pressed(int k) { return IsKeyPressed(k); }
static bool inp_released(int k){ return IsKeyReleased(k); }
static int  inp_mouse_x(void)  {
    if (web_tm_active) return clampi(gr_win_to_canvas_x(game_rect, web_tm_pos.x), 0, de_sw - 1);
    return clampi(gr_win_to_canvas_x(game_rect, GetMousePosition().x), 0, de_sw - 1);
}
static int  inp_mouse_y(void)  {
    if (web_tm_active) return clampi(gr_win_to_canvas_y(game_rect, web_tm_pos.y), 0, de_sh - 1);
    return clampi(gr_win_to_canvas_y(game_rect, GetMousePosition().y), 0, de_sh - 1);
}
static bool inp_mouse_down(int b)     {
    if (web_tm_active) return mbtn_index(b) == 0 && web_tm_down;
    return IsMouseButtonDown(raylib_mouse_button(b));
}
static bool inp_mouse_pressed(int b)  {
    if (web_tm_active) return mbtn_index(b) == 0 && web_tm_down && !web_tm_prev;
    return IsMouseButtonPressed(raylib_mouse_button(b));
}
static bool inp_mouse_released(int b) {
    if (web_tm_active) return mbtn_index(b) == 0 && !web_tm_down && web_tm_prev;
    return IsMouseButtonReleased(raylib_mouse_button(b));
}
static float inp_mouse_wheel(void) { return GetMouseWheelMove(); }
#endif

// lockstep netplay (rung 1 — docs/design/multiplayer-research.md). Runtime-flag
// gated like the harness above: a run without --net-host/--net-join touches none
// of it. Native Raylib build only (browsers have no UDP; DE_NO_RAYLIB hosts own
// their loop, so the blocking barrier doesn't fit there yet).
#if !defined(PLATFORM_WEB) && !defined(DE_NO_RAYLIB)
#define DE_NET_BUILD 1
#include "net.h"
#endif

// ------------------------------------------------------------
// touch state (all coordinates in window pixels unless noted)
// ------------------------------------------------------------

#ifndef TOUCH_CONTROLS_DEFAULT
#define TOUCH_CONTROLS_DEFAULT 0
#endif

static bool show_touch_ui = TOUCH_CONTROLS_DEFAULT;
static int  touch_move_mode = TOUCH_ANALOG;   // TOUCH_ANALOG (floating) | TOUCH_ANALOG_FIX (fixed); set by touch_layout()
static int  touch_n_buttons = 2;              // how many action buttons the cart asked for (clamped 0..4)

#define STICK_RADIUS    60.0f
#define STICK_DEADZONE  0.35f
#define BTN_RADIUS      44

// controls shrink to fit a tight deck/rail band (floored at CTRL_SCALE_MIN — still clears a
// physical thumb target); recomputed each frame in layout_touch_controls() from the band size.
// 1.0 = full size (overlay, or plenty of room). eff_stick_r()/eff_btn_r() are what every
// hit-test and draw call actually uses — never the bare STICK_RADIUS/BTN_RADIUS constants —
// so the visible size and the tappable size never drift apart.
static float ctrl_scale = 1.0f;      // the STICK's scale (also reused for buttons in DECK/OVERLAY, where a fixed margin already keeps them safely on-screen)
static float ctrl_scale_btn = 1.0f;  // the BUTTON diamond's own scale — RAILS sizes this from the button rail's width, since it's a wider footprint than the stick and the two rails aren't always the same size
#define CTRL_SCALE_MIN  0.6f

// last computed placement (set once a frame alongside game_rect, below) — read back by
// touch_layout_mode()/touch_ctrl_scale() and the touch_debug() overlay.
static int  place_mode = PLACE_OVERLAY;
static int  band_x = 0, band_y = 0, band_w = 0, band_h = 0;
static bool touch_debug_on = false;

static inline float eff_stick_r(void) { return STICK_RADIUS * ctrl_scale; }
static inline float eff_btn_r(void)   { return BTN_RADIUS   * ctrl_scale_btn; }

static int   stick_touch_id = -1;
static float stick_base_x = 0, stick_base_y = 0;
static float stick_knob_x = 0, stick_knob_y = 0;
static float stick_home_x = 0, stick_home_y = 0;   // fixed-mode base position (window px)
static int   sgz_x = 0, sgz_y = 0, sgz_w = 0, sgz_h = 0;   // stick grab zone (window px), set by layout

// action button centres (window px), by index: 0=A, 1=B, 2=X, 3=Y — see layout_action_buttons().
// Only the first touch_n_buttons slots are drawn/hit-tested; the rest sit computed-but-unused.
static int btn_cx[4], btn_cy[4];

// d-pad move mode: same grab-zone as the stick, but resolves to direction booleans instead
// of a knob position. dpad_touch_id tracks the held finger; dpad_up/down/left/right are this
// frame's result, read by btn() and painted by the draw skin.
static int  dpad_touch_id = -1;
static bool dpad_up = false, dpad_down = false, dpad_left = false, dpad_right = false;
static int  dpad_sector = -1;   // 0..7 compass sector (0=up, clockwise by 45°), -1 = no direction held
#define DPAD_DEADZONE     (eff_stick_r() * 0.3f)
#define DPAD_GRAB_RADIUS  (eff_stick_r() * 1.4f)  // a d-pad is a fixed LOCAL widget (unlike the
                                                    // stick's whole-zone grab) — only a tap near its
                                                    // own graphic should acquire it

// virtual touch pool — merges raylib touches with mouse-as-touch on desktop.
// the mouse LMB is exposed as a synthetic touch with id MOUSE_TOUCH_ID.
#define MOUSE_TOUCH_ID  (-2)
#define VT_MAX           16
static int     vt_count = 0;
static Vector2 vt_pos[VT_MAX];
static int     vt_id[VT_MAX];
// previous frame's ids — lets tapp() spot a touch that BEGAN this frame
static int     vt_prev_count = 0;
static int     vt_prev_id[VT_MAX];
// previous frame's positions + this frame's lifted contacts — a released touch
// is a ghost (no index this frame), so touch_ended_*/tapr() read these instead
static Vector2 vt_prev_pos[VT_MAX];
static int     vt_ended_count = 0;
static int     vt_ended_id[VT_MAX];
static Vector2 vt_ended_pos[VT_MAX];

// camera + clip state
// the camera is a raylib Camera2D applied via BeginMode2D, so zoom/rotation are GPU
// matrix work (not per-primitive math). offset is pinned to the screen centre, so
// zoom/angle pivot there; target is back-computed from camera(x,y) to stay identical
// to the old "world (x,y) at top-left" translate at zoom 1 / angle 0.
static Camera2D cam = {   // static initializer needs compile-time consts → SCREEN_W/H (the max);
                          // camera() recomputes offset/target at runtime, so this is byte-identical
    .offset   = { SCREEN_W / 2.0f, SCREEN_H / 2.0f },
    .target   = { SCREEN_W / 2.0f, SCREEN_H / 2.0f },
    .rotation = 0.0f,
    .zoom     = 1.0f,
};
static bool  cam_active = false;   // true while inside BeginMode2D during draw()
static bool  clip_active = false;
static int   clip_cx = 0, clip_cy = 0, clip_cw = 0, clip_ch = 0;  // active scissor rect (valid while clip_active)

// world→screen under the active camera (rotation is always 0 on the software path — a rotation
// camera_ex makes the cart fall back to GPU, see sw_force_gpu). zoom==1 is the fast translation
// case; zoom!=1 is an axis-aligned scale about the camera offset (Option 2 — no staircase).
static inline void sw_w2s(int wx, int wy, int *sx, int *sy) {
    if (cam.zoom == 1.0f) { *sx = wx - (int)(cam.target.x - cam.offset.x); *sy = wy - (int)(cam.target.y - cam.offset.y); }
    else { *sx = (int)floorf((wx - cam.target.x) * cam.zoom + cam.offset.x);
           *sy = (int)floorf((wy - cam.target.y) * cam.zoom + cam.offset.y); }
}
// raw screen-pixel write: clip (screen space, like GPU scissor) + bottom-up store (matches the FBO
// layout, so the present's -de_sh flip + the screenshot path treat sw and GPU canvases the same).
// Phase 1b: bounds + flip origin follow the ACTIVE canvas (de_sw/de_sh); the row STRIDE stays the
// physical buffer width (SCREEN_W = the compile-time max), so a resizable cart renders into the
// bottom-left de_sw×de_sh sub-region and the present grabs it. Byte-identical when de == max.
static inline void sw_plot1(int sx, int sy, uint32_t p) {
    if (clip_active && (sx < clip_cx || sx >= clip_cx + clip_cw || sy < clip_cy || sy >= clip_cy + clip_ch)) return;
    if ((unsigned)sx < (unsigned)de_sw && (unsigned)sy < (unsigned)de_sh)
        sw_dst[(de_sh - 1 - sy) * fb_w + sx] = p;
}
// software-canvas pixel write. zoom==1: one texel (the hot path). zoom!=1: fill the world pixel's
// screen footprint (a zoom×zoom block) so a zoomed pset/blit/line stays gap-free.
static inline void sw_pset(int x, int y, DeColor c) {
    uint32_t p = sw_pack(c);
    if (cam.zoom == 1.0f) { int sx, sy; sw_w2s(x, y, &sx, &sy); sw_plot1(sx, sy, p); return; }
    int sx0, sy0, sx1, sy1; sw_w2s(x, y, &sx0, &sy0); sw_w2s(x + 1, y + 1, &sx1, &sy1);
    if (sx1 <= sx0) sx1 = sx0 + 1; if (sy1 <= sy0) sy1 = sy0 + 1;
    for (int yy = sy0; yy < sy1; yy++) for (int xx = sx0; xx < sx1; xx++) sw_plot1(xx, yy, p);
}
// software line: the validated reflection-symmetric per-axis DDA (sline), writing sw_pset.
static void sw_plot_minor(int maj, float v, float mid, int horiz, DeColor c) {
    float f = floorf(v); int fi = (int)f; float r = v - f; int m;
    if (r != 0.5f) m = (r < 0.5f) ? fi : fi + 1;
    else if (v == mid) { if (horiz) { sw_pset(maj,fi,c); sw_pset(maj,fi+1,c); } else { sw_pset(fi,maj,c); sw_pset(fi+1,maj,c); } return; }
    else m = (mid > v) ? fi + 1 : fi;
    if (horiz) sw_pset(maj, m, c); else sw_pset(m, maj, c);
}
static void sw_sline(int x0, int y0, int x1, int y1, DeColor c) {
    int dx = x1-x0, dy = y1-y0, adx = dx<0?-dx:dx, ady = dy<0?-dy:dy;
    if (adx == 0 && ady == 0) { sw_pset(x0, y0, c); return; }
    if (adx >= ady) { int lo = x0<x1?x0:x1, hi = x0<x1?x1:x0; float ymid = (y0+y1)*0.5f;
        for (int x = lo; x <= hi; x++) sw_plot_minor(x, y0 + (float)(x-x0)*dy/(float)dx, ymid, 1, c);
    } else { int lo = y0<y1?y0:y1, hi = y0<y1?y1:y0; float xmid = (x0+x1)*0.5f;
        for (int y = lo; y <= hi; y++) sw_plot_minor(y, x0 + (float)(y-y0)*dx/(float)dy, xmid, 0, c); }
}
// software filled rect: clip+camera-offset once, then memset cbuf rows (keeps the span speedup —
// per-pixel plot_pat here would be far slower than the GPU DrawRectangle it replaces).
static void sw_fillrect(int x, int y, int w, int h, DeColor c) {
    int x0, y0, x1, y1;
    sw_w2s(x, y, &x0, &y0); sw_w2s(x + w, y + h, &x1, &y1);   // camera translate + zoom-scale the rect
    if (clip_active) { if (x0<clip_cx) x0=clip_cx; if (y0<clip_cy) y0=clip_cy;
                       if (x1>clip_cx+clip_cw) x1=clip_cx+clip_cw; if (y1>clip_cy+clip_ch) y1=clip_cy+clip_ch; }
    if (x0<0) x0=0; if (y0<0) y0=0; if (x1>de_sw) x1=de_sw; if (y1>de_sh) y1=de_sh;
    uint32_t p = sw_pack(c);
    for (int yy = y0; yy < y1; yy++) { uint32_t *row = &sw_dst[(de_sh-1-yy)*fb_w]; for (int xx = x0; xx < x1; xx++) row[xx] = p; }
}
// one fill-scanline span: cbuf row write under the software canvas, else the GPU DrawRectangle.
// Lets the circ/oval/poly span fast-paths stay span-based (not per-pixel) on the canvas.
static inline void sw_span(int x, int y, int w, DeColor c) {
    if (sw_canvas_active) sw_fillrect(x, y, w, 1, c);
    else DrawRectangle(x, y, w, 1, c);
}
// CPU twin of PAL_FS: map a texel to the nearest base-palette entry, output the (remapped)
// current palette colour. Same nearest-by-squared-distance argmin as the shader (integer space
// is monotonic with the shader's normalized space, so the winner is identical). Sprite texels are
// exact palette RGBs, so "nearest" is "exact" in practice. Caller gates on pal_active.
static inline DeColor sw_recolor(DeColor c) {
    int best = 0, bestd = 1 << 30;
    for (int i = 0; i < PALETTE_SIZE; i++) {
        int dr = c.r - base_palette[i].r, dg = c.g - base_palette[i].g, db = c.b - base_palette[i].b;
        int d = dr*dr + dg*dg + db*db;
        if (d < bestd) { bestd = d; best = i; }   // strict < → first match wins, like the shader
    }
    DeColor o = palette[best]; o.a = c.a; return o;
}
// true if this texel is the runtime colorkey (drawn transparent), mirroring colorkey()'s baked hole.
static inline bool sw_keyed(DeColor c) {
    return sw_colorkey_on && c.r == sw_colorkey_rgb.r && c.g == sw_colorkey_rgb.g && c.b == sw_colorkey_rgb.b;
}
// fast texel read — direct typed load for the common RGBA8 case (the sheet/atlases), bypassing the
// non-inlined raylib GetImageColor call + its per-pixel format switch (the per-pixel sampling hotspot).
// Byte-identical to GetImageColor for RGBA8; falls back for any other format. OOB → blank (matches
// raylib). The blits sample inside valid regions, so the bounds check is a cheap guard, not the path.
static inline DeColor img_texel(const Image *img, int x, int y) {
    if ((unsigned)x >= (unsigned)img->width || (unsigned)y >= (unsigned)img->height) return (DeColor){0,0,0,0};
    if (img->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        const unsigned char *p = (const unsigned char *)img->data + ((size_t)y * img->width + x) * 4;
        return (DeColor){ p[0], p[1], p[2], p[3] };
    }
    return GetImageColor(*img, x, y);
}
// software sprite blit: nearest-sample spritesheet_img → cbuf via sw_pset (camera+clip), with
// optional flip and nearest scaling (src wxh → dst wxh). Alpha<128 = transparent (PNG colorkey);
// the runtime colorkey() colour is skipped too. use_pal applies the pal() swap (spr/sspr — NOT
// map(), which the GPU draws without the swap shader, so the canvas must match).
static void sw_blit(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, bool fx, bool fy, bool use_pal) {
    if (!spritesheet_img.data || dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    bool recolor = use_pal && pal_active;
    // FAST PATH — axis-aligned, unscaled, unflipped, zoom==1, no recolor, RGBA8 sheet (the spr()/map()
    // common case). Read the source row as uint32 (RGBA8 bytes pack to sw_pack()'s layout on LE) and
    // write straight into cbuf, alpha forced opaque — skipping the per-pixel img_texel/sw_keyed/sw_pset/
    // sw_w2s/sw_pack call chain + the i*sw/dw divide. Byte-identical to the loop below (same alpha<128
    // and colorkey skip, same opaque store); A/B via DE_BLIT_FAST=off. This is the sprite hotspot.
    if (blit_fast_on && !recolor && !fx && !fy && dw == sw && dh == sh
        && cam.zoom == 1.0f && spritesheet_img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        int ox, oy; sw_w2s(dx, dy, &ox, &oy);                          // screen origin (one camera translate)
        int xmin = 0, ymin = 0, xmax = de_sw, ymax = de_sh;
        if (clip_active) {                                             // intersect the clip rect, screen-space (== sw_plot1)
            if (clip_cx > xmin) xmin = clip_cx;
            if (clip_cy > ymin) ymin = clip_cy;
            if (clip_cx + clip_cw < xmax) xmax = clip_cx + clip_cw;
            if (clip_cy + clip_ch < ymax) ymax = clip_cy + clip_ch;
        }
        int i0 = xmin - ox; if (i0 < 0) i0 = 0;
        int i1 = xmax - ox; if (i1 > dw) i1 = dw;
        int j0 = ymin - oy; if (j0 < 0) j0 = 0;
        int j1 = ymax - oy; if (j1 > dh) j1 = dh;
        if (i0 >= i1 || j0 >= j1) return;
        const uint32_t *src = (const uint32_t *)spritesheet_img.data;
        int srcw = spritesheet_img.width;
        bool keyon = sw_colorkey_on;
        uint32_t key = (uint32_t)sw_colorkey_rgb.r | ((uint32_t)sw_colorkey_rgb.g << 8) | ((uint32_t)sw_colorkey_rgb.b << 16);
        for (int j = j0; j < j1; j++) {
            const uint32_t *srow = src + (size_t)(sy + j) * srcw + sx;
            uint32_t *drow = &sw_dst[(size_t)(de_sh - 1 - (oy + j)) * fb_w + ox];
            for (int i = i0; i < i1; i++) {
                uint32_t s = srow[i];
                if (!(s & 0x80000000u)) continue;                     // alpha < 128 → transparent
                if (keyon && (s & 0x00FFFFFFu) == key) continue;      // runtime colorkey hole
                drow[i] = s | 0xFF000000u;                            // store opaque (matches sw_pack)
            }
        }
        return;
    }
    // Nearest sampling at the DEST pixel CENTRE — floor((i+0.5)*sw/dw) — the GPU POINT-filter
    // convention DrawTexturePro rasterizes with (fragment centre → texcoord → floor). The old
    // top-left truncation (i*sw/dw) agreed at integer ratios (2× text/tiles) but phase-shifted
    // non-integer scales by up to one texel vs the GPU (drawall's 16→24 sspr: 109 differing px).
    // Unscaled (dw==sw) both formulas reduce to i, so flips/recolors at 1:1 are unchanged.
    // Flipped: texcoords run (s+size)→s, so the centre maps to (s+size) - (i+0.5)*size/dsize.
    for (int j = 0; j < dh; j++) {
        int syy = fy ? (int)((sy + sh) - (j + 0.5f) * sh / dh) : (sy + (int)((j + 0.5f) * sh / dh));
        for (int i = 0; i < dw; i++) {
            int sxx = fx ? (int)((sx + sw) - (i + 0.5f) * sw / dw) : (sx + (int)((i + 0.5f) * sw / dw));
            DeColor c = img_texel(&spritesheet_img, sxx, syy);
            if (c.a < 128 || sw_keyed(c)) continue;
            sw_pset(dx + i, dy + j, recolor ? sw_recolor(c) : c);
        }
    }
}
// software textured triangle (the stritex probe): edge-function rasterizer + top-left rule, affine
// barycentric u,v sampled from spritesheet_img → sw_pset. Adjacent tris tile gap-free (top-left).
static float sw_edge(float ax,float ay,float bx,float by,float px,float py){ return (bx-ax)*(py-ay)-(by-ay)*(px-ax); }
static int   sw_topleft(float ax,float ay,float bx,float by){ float dx=bx-ax,dy=by-ay; return (dy<0.f)||(dy==0.f&&dx<0.f); }
// ── A/B SWITCH (temporary, 2026-07) ─────────────────────────────────────────
// The original sw_tritex, kept verbatim while the clamped/hoisted path below soaks
// (DE_TRITEX_FAST=off routes here). Its flaw: the scan box is the RAW vertex bbox —
// a face projected near the 3D camera lands coordinates in the thousands, so one
// triangle scans millions of never-plotted pixels (infiniminer: 1.2s/frame).
// TODO: once the fast path is trusted, delete this + the flag.
static void sw_tritex_legacy(float x0,float y0,float u0,float v0, float x1,float y1,float u1,float v1, float x2,float y2,float u2,float v2){
    if (!spritesheet_img.data) return;
    float area = sw_edge(x0,y0,x1,y1,x2,y2);
    if (area == 0.f) return;
    if (area < 0.f) { float t; t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; t=u1;u1=u2;u2=t; t=v1;v1=v2;v2=t; area=-area; }
    int minx=(int)floorf(fminf(x0,fminf(x1,x2))), maxx=(int)ceilf(fmaxf(x0,fmaxf(x1,x2)));
    int miny=(int)floorf(fminf(y0,fminf(y1,y2))), maxy=(int)ceilf(fmaxf(y0,fmaxf(y1,y2)));
    int tl0=sw_topleft(x1,y1,x2,y2), tl1=sw_topleft(x2,y2,x0,y0), tl2=sw_topleft(x0,y0,x1,y1);
    for (int py=miny; py<=maxy; py++) for (int px=minx; px<=maxx; px++) {
        float fx=px+0.5f, fy=py+0.5f;
        float w0=sw_edge(x1,y1,x2,y2,fx,fy), w1=sw_edge(x2,y2,x0,y0,fx,fy), w2=sw_edge(x0,y0,x1,y1,fx,fy);
        int in0=(w0>0.f)||(w0==0.f&&tl0), in1=(w1>0.f)||(w1==0.f&&tl1), in2=(w2>0.f)||(w2==0.f&&tl2);
        if (in0 && in1 && in2) {
            float l0=w0/area, l1=w1/area, l2=w2/area;
            int iu=(int)(l0*u0+l1*u1+l2*u2), iv=(int)(l0*v0+l1*v1+l2*v2);
            if ((unsigned)iu<(unsigned)spritesheet_img.width && (unsigned)iv<(unsigned)spritesheet_img.height) {
                DeColor cc = img_texel(&spritesheet_img, iu, iv);
                // plot via pset_rgb (arbitrary sampled RGB, no palette index) → backend-agnostic:
                // on-canvas → sw_pset → cbuf; off-canvas (DE_CPU_RASTER) → DrawPixel. tritex uses the
                // keyed sheet; no pal swap (matches GPU).
                if (cc.a >= 128 && !sw_keyed(cc)) pset_rgb(px, py, (cc.r << 16) | (cc.g << 8) | cc.b);
            }
        }
    }
}
// The optimized path. Coverage + texel DECISIONS are the legacy float math verbatim
// (same sw_edge order, same w/area divisions), so every plotted pixel is byte-identical.
// Two changes, both to what's ITERATED / how an already-decided pixel is WRITTEN:
//   1. the scan box is intersected with the visible region (poly_clamp_scan — the same
//      camera-aware clamp every software fill uses), so a huge near-projected triangle
//      only scans pixels that could land on screen;
//   2. on the software canvas at zoom==1 the plot skips the per-pixel pset_rgb →
//      sw_pset → sw_w2s → sw_plot1 chain: camera offset + clip are hoisted into the
//      loop bounds once, and the write is a direct cbuf row store (sw_blit's pattern).
//      zoom!=1 (block footprint) and off-canvas DE_CPU_RASTER keep the pset_rgb plot.
static void sw_tritex(float x0,float y0,float u0,float v0, float x1,float y1,float u1,float v1, float x2,float y2,float u2,float v2){
    if (!tritex_fast_on) { sw_tritex_legacy(x0,y0,u0,v0, x1,y1,u1,v1, x2,y2,u2,v2); return; }
    if (!spritesheet_img.data) return;
    float area = sw_edge(x0,y0,x1,y1,x2,y2);
    if (area == 0.f) return;
    if (area < 0.f) { float t; t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; t=u1;u1=u2;u2=t; t=v1;v1=v2;v2=t; area=-area; }
    int minx=(int)floorf(fminf(x0,fminf(x1,x2))), maxx=(int)ceilf(fmaxf(x0,fmaxf(x1,x2)));
    int miny=(int)floorf(fminf(y0,fminf(y1,y2))), maxy=(int)ceilf(fmaxf(y0,fmaxf(y1,y2)));
    poly_clamp_scan(&minx, &miny, &maxx, &maxy);        // skip never-plotted off-screen cells
    if (maxx < minx || maxy < miny) return;
    int tl0=sw_topleft(x1,y1,x2,y2), tl1=sw_topleft(x2,y2,x0,y0), tl2=sw_topleft(x0,y0,x1,y1);
    if (sw_canvas_active && cam.zoom == 1.0f) {
        // hoist sw_pset's per-pixel work: sw_w2s at zoom==1 is a constant integer
        // offset, and sw_plot1's clip+bounds tests are monotonic in px/py — fold both
        // into the scan range, then the inner loop writes rows directly.
        int camdx = (int)(cam.target.x - cam.offset.x), camdy = (int)(cam.target.y - cam.offset.y);
        int sxlo = 0, sxhi = de_sw, sylo = 0, syhi = de_sh;          // half-open screen range
        if (clip_active) { if (clip_cx > sxlo) sxlo = clip_cx; if (clip_cy > sylo) sylo = clip_cy;
                           if (clip_cx + clip_cw < sxhi) sxhi = clip_cx + clip_cw;
                           if (clip_cy + clip_ch < syhi) syhi = clip_cy + clip_ch; }
        if (minx < sxlo + camdx) minx = sxlo + camdx;  if (maxx > sxhi - 1 + camdx) maxx = sxhi - 1 + camdx;
        if (miny < sylo + camdy) miny = sylo + camdy;  if (maxy > syhi - 1 + camdy) maxy = syhi - 1 + camdy;
        for (int py=miny; py<=maxy; py++) {
            uint32_t *row = &sw_dst[(de_sh - 1 - (py - camdy)) * fb_w];
            float fy = py + 0.5f;
            for (int px=minx; px<=maxx; px++) {
                float fx = px + 0.5f;
                float w0=sw_edge(x1,y1,x2,y2,fx,fy), w1=sw_edge(x2,y2,x0,y0,fx,fy), w2=sw_edge(x0,y0,x1,y1,fx,fy);
                int in0=(w0>0.f)||(w0==0.f&&tl0), in1=(w1>0.f)||(w1==0.f&&tl1), in2=(w2>0.f)||(w2==0.f&&tl2);
                if (!(in0 && in1 && in2)) continue;
                float l0=w0/area, l1=w1/area, l2=w2/area;
                int iu=(int)(l0*u0+l1*u1+l2*u2), iv=(int)(l0*v0+l1*v1+l2*v2);
                if ((unsigned)iu<(unsigned)spritesheet_img.width && (unsigned)iv<(unsigned)spritesheet_img.height) {
                    DeColor cc = img_texel(&spritesheet_img, iu, iv);
                    if (cc.a >= 128 && !sw_keyed(cc))
                        row[px - camdx] = sw_pack((DeColor){ cc.r, cc.g, cc.b, 255 });   // = sw_plot1's store
                }
            }
        }
        return;
    }
    for (int py=miny; py<=maxy; py++) for (int px=minx; px<=maxx; px++) {   // zoomed canvas / DE_CPU_RASTER
        float fx=px+0.5f, fy=py+0.5f;
        float w0=sw_edge(x1,y1,x2,y2,fx,fy), w1=sw_edge(x2,y2,x0,y0,fx,fy), w2=sw_edge(x0,y0,x1,y1,fx,fy);
        int in0=(w0>0.f)||(w0==0.f&&tl0), in1=(w1>0.f)||(w1==0.f&&tl1), in2=(w2>0.f)||(w2==0.f&&tl2);
        if (in0 && in1 && in2) {
            float l0=w0/area, l1=w1/area, l2=w2/area;
            int iu=(int)(l0*u0+l1*u1+l2*u2), iv=(int)(l0*v0+l1*v1+l2*v2);
            if ((unsigned)iu<(unsigned)spritesheet_img.width && (unsigned)iv<(unsigned)spritesheet_img.height) {
                DeColor cc = img_texel(&spritesheet_img, iu, iv);
                if (cc.a >= 128 && !sw_keyed(cc)) pset_rgb(px, py, (cc.r << 16) | (cc.g << 8) | cc.b);
            }
        }
    }
}

// ── --uiaudit: per-frame draw bounding-box log ───────────────────────────
// When --uiaudit <file> is set, every primitive records its bounds + the
// active clip rect here; uiaudit_flush() writes one JSONL line per frame after
// draw(). tools/ui-audit.js reads it to flag text/sprites past the screen edge,
// truncation inside a clip panel, and overlapping text. Off → zero cost (the
// UIAUDIT macro compiles to nothing in web/release builds).
#define UIAUDIT_MAX 4096
typedef struct { char kind; short x, y, w, h; unsigned char clip; char text[64]; } UiAuditRec;
static UiAuditRec uiaudit_recs[UIAUDIT_MAX];
static int        uiaudit_n = 0;
// declared here (NOT under #ifndef PLATFORM_WEB) so the audit fns below compile in web/release too —
// web never passes --uiaudit, so uiaudit_path stays NULL, the file is never opened, and every audit
// fn early-returns (a no-op). Keeps the web build green without web-guarding each call site.
static FILE       *uiaudit_file = NULL;   // --uiaudit JSONL output (native only; NULL elsewhere)
static const char *uiaudit_path = NULL;   // set by --uiaudit <file>

static void uiaudit_box(char kind, int x, int y, int w, int h, const char *text) {
    if (!uiaudit_file || uiaudit_n >= UIAUDIT_MAX) return;
    UiAuditRec *r = &uiaudit_recs[uiaudit_n++];
    r->kind = kind;
    r->x = (short)x; r->y = (short)y; r->w = (short)w; r->h = (short)h;
    r->clip = clip_active ? 1 : 0;
    r->text[0] = '\0';
    if (text) { strncpy(r->text, text, sizeof r->text - 1); r->text[sizeof r->text - 1] = '\0'; }
}
#if !defined(PLATFORM_WEB) && !defined(DE_RELEASE)
  #define UIAUDIT(k, x, y, w, h, t) uiaudit_box((k), (x), (y), (w), (h), (t))
#else
  #define UIAUDIT(k, x, y, w, h, t) ((void)0)
#endif

// ui.h calls this (only under -DDE_TRACE) once per registered widget in ui_end(),
// so the auditor knows which boxes are interactive targets — not just decoration.
// 'w' record; its text field carries the focusable flag ("1"/"0"). No-op when
// --uiaudit is off, so it's free in a normal harness run too.
void de_ui_audit(int x, int y, int w, int h, int focusable) {
    uiaudit_box('w', x, y, w, h, focusable ? "1" : "0");
}

// pget snapshot — refreshed at the start of every frame, so pget reads
// the previous frame's canvas (consistent state, no mid-frame RT readback)
static Image pget_snapshot     = (Image){0};
static bool  pget_snapshot_valid = false;
// opt-in (enable_pget): the canvas read-back is only worth its GPU stall + per-frame
// 256KB Image churn for carts that actually read pixels. Off by default; a cart that
// uses pget/pget_rgb/touching_color turns it on (once in init, or around a reading
// mode). pget_warned makes the "forgot to enable" hint fire only once.
static bool  pget_enabled = false;
static bool  pget_warned  = false;

// how much to shrink a control (0..1) so a ring of the given reference diameter fits inside a
// band of the given size — floored at CTRL_SCALE_MIN so it never shrinks below a tappable size,
// ceilinged at 1 so a roomy band doesn't blow the control up past its normal size.
static float scale_to_fit(int span, float ref_diameter) {
    float s = (float)span / ref_diameter;
    return s < CTRL_SCALE_MIN ? CTRL_SCALE_MIN : (s > 1.0f ? 1.0f : s);
}
static float band_ctrl_scale(int span) { return scale_to_fit(span, 2.0f * STICK_RADIUS); }

// the button cluster's own footprint: BTN_CLUSTER_SPREAD is how far (× eff_btn_r()) each
// button's CENTRE sits from the shared anchor; a diamond arm's full reach (centre-to-centre
// plus its own radius) is (spread+1) button radii — much wider than a single stick, so it needs
// its own fit check against whichever side it's on.
#define BTN_CLUSTER_SPREAD 1.6f
#define BTN_ARM_REACH      (BTN_CLUSTER_SPREAD + 1.0f)

// a small fixed gutter (window px, NOT scaled — a resting margin like DECK's fixed 145/20) so a
// control never sits flush against the rail/window edge.
#define RAIL_EDGE_MARGIN 8

// how far the button cluster reaches LEFT/RIGHT of its anchor, in BTN_RADIUS units, for the
// buttons ACTUALLY shown (not a worst-case 4-way diamond) — so a 1-2 button cart (the common
// case) gets a tighter, bigger-drawn cluster instead of being sized for buttons it never shows.
static void btn_cluster_reach(int n, float *left, float *right) {
    *right = (n >= 2) ? BTN_ARM_REACH : (n >= 1 ? 1.0f : 0.0f);   // B (right) needs the full arm; else just A's own width
    *left  = (n >= 3) ? BTN_ARM_REACH : (n >= 1 ? 1.0f : 0.0f);   // X (left) only if 3+
}

// lay out up to 4 action buttons (btn_cx/cy[0..3] = A/B/X/Y) in a diamond around an anchor point
// — A bottom, B right, X left, Y top, the classic face-button arrangement. Reads naturally at
// 1-4 buttons without an ever-growing horizontal strip. Radius scales with the button's own
// effective size (see eff_btn_r()) so the cluster never overlaps itself at any ctrl_scale_btn.
static void layout_action_buttons(int ax, int ay) {
    int rad = (int)(eff_btn_r() * BTN_CLUSTER_SPREAD);
    btn_cx[0] = ax;       btn_cy[0] = ay + rad;   // A: bottom
    btn_cx[1] = ax + rad; btn_cy[1] = ay;         // B: right
    btn_cx[2] = ax - rad; btn_cy[2] = ay;         // X: left
    btn_cx[3] = ax;       btn_cy[3] = ay - rad;   // Y: top
}

// place the stick + buttons (all window px) for the current placement. OVERLAY: corners of the
// game rect (matches the old fixed layout when the window == the game). DECK: stick left, buttons
// right, inside the band below the game. RAILS: stick in the left rail, buttons in the right.
// Recomputed each frame so a resized / device window re-lays-out for free.
static void layout_touch_controls(Placement p) {
    int gx = (int)p.game.x, gy = (int)p.game.y;
    int gw = (int)(de_sw * p.game.scale), gh = (int)(de_sh * p.game.scale);
    if (p.mode == PLACE_DECK) {
        int bw = p.band_w, by = p.band_y, bh = p.band_h, cy = by + bh / 2;
        ctrl_scale = ctrl_scale_btn = band_ctrl_scale(bh);                 // band HEIGHT is the tight dimension for both — safe: the 145px anchor margin below always clears the button footprint at scale<=1
        stick_home_x = bw * 0.20f;  stick_home_y = (float)cy;
        layout_action_buttons(bw - 145, cy);
        sgz_x = 0; sgz_y = by; sgz_w = bw / 2; sgz_h = bh;                 // left half of the band
    } else if (p.mode == PLACE_RAILS) {
        int ww = p.band_w, wh = p.band_h, rl = gx, rr0 = gx + gw, ry = (int)(wh * 0.62f);
        int rr = ww - rr0;                                                 // right-rail WIDTH — its own tight dimension, NOT rl (the stick's rail can be a different size)

        // stick/d-pad side: a d-pad's DEBUG/GRAB-ZONE circle (below) is 1.4x the drawn ring, so
        // it — not the plain ring — is what must fit the rail, or it bleeds past the window edge
        // (only the ring matters for analog: its real hit-zone is the separate whole-rail sgz rect).
        bool dpad_mode = (touch_move_mode == TOUCH_DPAD4 || touch_move_mode == TOUCH_DPAD8);
        float stick_reach = dpad_mode ? 1.4f : 1.0f;
        int   stick_avail = rl - 2 * RAIL_EDGE_MARGIN;
        ctrl_scale = scale_to_fit(stick_avail, 2.0f * STICK_RADIUS * stick_reach);
        float stick_r_reach = STICK_RADIUS * ctrl_scale * stick_reach;     // the widest circle actually drawn/hit-tested
        float stick_slack = stick_avail - 2.0f * stick_r_reach; if (stick_slack < 0) stick_slack = 0;
        stick_home_x = RAIL_EDGE_MARGIN + stick_r_reach + stick_slack * 0.5f;
        stick_home_y = (float)ry;

        // button side: size + place for the buttons ACTUALLY shown (not a worst-case 4-way
        // diamond), with the same margin-in-then-centre-the-slack approach as the stick above.
        float lu, ru; btn_cluster_reach(touch_n_buttons, &lu, &ru);
        int   btn_avail = rr - 2 * RAIL_EDGE_MARGIN;
        ctrl_scale_btn = (lu + ru) > 0 ? scale_to_fit(btn_avail, (lu + ru) * BTN_RADIUS) : 1.0f;
        float br = eff_btn_r();
        float btn_slack = btn_avail - (lu + ru) * br; if (btn_slack < 0) btn_slack = 0;
        int bx = rr0 + RAIL_EDGE_MARGIN + (int)(lu * br + btn_slack * 0.5f);
        int leftBound = rr0 + RAIL_EDGE_MARGIN + (int)(lu * br), rightBound = ww - RAIL_EDGE_MARGIN - (int)(ru * br);
        if (bx < leftBound)  bx = leftBound;                              // belt-and-suspenders: keep the cluster inside the
        if (bx > rightBound) bx = rightBound;                             // rail + margin even if CTRL_SCALE_MIN can't quite fit it
        layout_action_buttons(bx, ry);
        sgz_x = 0; sgz_y = 0; sgz_w = rl; sgz_h = wh;                      // the whole left rail
    } else {  // OVERLAY — hug the corners of the game rect; always full size, no band to shrink to
        ctrl_scale = ctrl_scale_btn = 1.0f;
        stick_home_x = gx + 80;   stick_home_y = gy + gh - 80;
        layout_action_buttons(gx + gw - 130, gy + gh - 100);
        sgz_x = gx; sgz_y = gy; sgz_w = (int)(gw * 0.55f); sgz_h = gh;     // left 55% of the game
    }
}

static void init_touch_layout(void) {
    Placement p = { PLACE_OVERLAY, { 0.0f, 0.0f, (float)SCALE }, 0, 0, de_sw * SCALE, de_sh * SCALE };
    layout_touch_controls(p);   // overlay defaults (window == game) until a frame sets a real placement
}

static bool point_in_circle(float px, float py, float cx, float cy, float r) {
    float dx = px - cx, dy = py - cy;
    return dx*dx + dy*dy <= r*r;
}

// is this point sitting on one of the currently-active action buttons? Used to exclude the
// button area from the stick/d-pad's move-zone grab test — a finger tapping a button shouldn't
// also grab the move control.
static bool touching_action_button(float x, float y) {
    for (int i = 0; i < touch_n_buttons; i++)
        if (point_in_circle(x, y, btn_cx[i], btn_cy[i], eff_btn_r())) return true;
    return false;
}

static bool vt_was_down(int id) {
    for (int i = 0; i < vt_prev_count; i++)
        if (vt_prev_id[i] == id) return true;
    return false;
}

#ifdef PLATFORM_WEB
// The web phantom-touch fix (docs/design/touch-notes.md §7): emscripten's
// touchend reports remaining+lifted contacts conflated (emscripten#4679,
// wontfix) and raylib's web backend removes at most ONE point per event —
// release two fingers in the same instant and a stale contact survives in
// GetTouchPosition()'s list forever. The shell (web_shell.html) mirrors the
// DOM's event.touches — spec-correct in every browser: exactly the fingers
// that are down, rebuilt on every touch event — into Module.deTouches as flat
// [id, x, y] triples in canvas pixels. Read that instead of raylib's list.
// Returns the contact count, or -1 if the mirror isn't installed (an older
// shell → caller falls back to raylib's list).
EM_JS(int, de_web_touch_read, (int *ids, float *xs, float *ys, int max), {
    var t = Module.deTouches;
    if (!t) return -1;
    var n = (t.length / 3) | 0;
    if (n > max) n = max;
    for (var i = 0; i < n; i++) {
        HEAP32[(ids >> 2) + i]  = t[i*3];
        HEAPF32[(xs >> 2) + i]  = t[i*3 + 1];
        HEAPF32[(ys >> 2) + i]  = t[i*3 + 2];
    }
    return n;
});
#endif

static void poll_virtual_touches(void) {
    vt_prev_count = vt_count;
    for (int i = 0; i < vt_count; i++) {
        vt_prev_id[i]  = vt_id[i];
        vt_prev_pos[i] = vt_pos[i];
    }
    int n = -1;
#ifdef PLATFORM_WEB
    // touch truth from the shell's DOM mirror, not raylib's web list (§7 fix)
    int   web_id[VT_MAX];
    float web_x[VT_MAX], web_y[VT_MAX];
    n = de_web_touch_read(web_id, web_x, web_y, VT_MAX - 1);
    for (int i = 0; i < n; i++) {
        vt_pos[i] = (Vector2){ web_x[i], web_y[i] };
        vt_id[i]  = web_id[i];
    }
    // touch→mouse synthesis (see web_tm_* block): button down while any finger
    // is down, position follows a sticky primary finger, edges from prev/cur.
    web_tm_prev = web_tm_down;
    if (n > 0) {
        web_tm_active = true;          // fingers exist → GLFW's emulated mouse is dead to us
        int idx = -1;
        for (int i = 0; i < n; i++)
            if (vt_id[i] == web_tm_id) { idx = i; break; }
        if (idx < 0) { idx = 0; web_tm_id = vt_id[0]; }   // primary lifted → adopt the next finger
        web_tm_pos  = vt_pos[idx];
        web_tm_down = true;
    } else {
        web_tm_down = false;
        web_tm_id   = -1;
    }
#endif
    if (n < 0) {                  // native — or a web shell without the mirror
        n = GetTouchPointCount();
        if (n > VT_MAX - 1) n = VT_MAX - 1;
        for (int i = 0; i < n; i++) {
            vt_pos[i] = GetTouchPosition(i);
            vt_id[i]  = GetTouchPointId(i);
        }
    }
    vt_count = n;
    // mouse-as-touch is a DESKTOP affordance. On a real touch device the browser
    // also EMULATES mouse events from touches (iOS fires a compatibility mousedown
    // around finger-release with no clean mouseup during multitouch) — appending
    // the mouse there creates a stationary ghost contact the moment one of several
    // fingers lifts (found on iPhone via touchlab, 2026-06-05). Once a real touch
    // has ever been seen, the device has fingers: stop synthesizing the mouse.
    static bool seen_real_touch = false;
    if (n > 0) seen_real_touch = true;
    // harness-aware mouse: under --replay/--script the injected pointer must
    // become the synthetic touch too, or touch-path carts (tap/tapp/touch_*)
    // are undriveable from a script. vt positions are window pixels, the
    // injected pointer is canvas space — scale back up.
#ifndef PLATFORM_WEB
    bool  syn_down = inp_mouse_down(MOUSE_LEFT);
    Vector2 syn_pos = inject_input
        ? (Vector2){ (float)(mouse_inj_x * SCALE), (float)(mouse_inj_y * SCALE) }
        : GetMousePosition();
#else
    bool  syn_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Vector2 syn_pos = GetMousePosition();
#endif
    if (!seen_real_touch && syn_down && vt_count < VT_MAX) {
        vt_pos[vt_count] = syn_pos;
        vt_id[vt_count]  = MOUSE_TOUCH_ID;
        vt_count++;
    }
    // contacts present last frame but gone now → this frame's ended list,
    // carrying their last-seen position (touch_ended_* / tapr read this)
    vt_ended_count = 0;
    for (int j = 0; j < vt_prev_count; j++) {
        bool still = false;
        for (int i = 0; i < vt_count; i++)
            if (vt_id[i] == vt_prev_id[j]) { still = true; break; }
        if (!still) {
            vt_ended_id[vt_ended_count]  = vt_prev_id[j];
            vt_ended_pos[vt_ended_count] = vt_prev_pos[j];
            vt_ended_count++;
        }
    }
}

static bool any_touch_in_circle(int cx, int cy, float r) {
    for (int i = 0; i < vt_count; i++) {
        if (point_in_circle(vt_pos[i].x, vt_pos[i].y, cx, cy, r)) return true;
    }
    return false;
}

static void update_stick(void) {
    if (!show_touch_ui) { stick_touch_id = -1; return; }

    bool fixed = (touch_move_mode == TOUCH_ANALOG_FIX);
    if (fixed) { stick_base_x = stick_home_x; stick_base_y = stick_home_y; }   // base pinned to its home

    bool still_active = false;
    if (stick_touch_id != -1) {
        for (int i = 0; i < vt_count; i++) {
            if (vt_id[i] == stick_touch_id) {
                Vector2 p = vt_pos[i];
                float dx = p.x - stick_base_x, dy = p.y - stick_base_y;
                float len = sqrtf(dx*dx + dy*dy);
                float r = eff_stick_r();
                if (len > r) { dx = dx/len*r; dy = dy/len*r; }
                stick_knob_x = stick_base_x + dx;
                stick_knob_y = stick_base_y + dy;
                still_active = true;
                break;
            }
        }
    }
    if (!still_active) stick_touch_id = -1;

    if (stick_touch_id == -1) {
        for (int i = 0; i < vt_count; i++) {
            Vector2 p = vt_pos[i];
            if (p.x < sgz_x || p.x >= sgz_x + sgz_w || p.y < sgz_y || p.y >= sgz_y + sgz_h) continue;  // outside the stick zone
            if (touching_action_button(p.x, p.y)) continue;
            stick_touch_id = vt_id[i];
            if (fixed) {                          // base stays at home; knob starts centred, deflects toward the finger
                stick_knob_x = stick_base_x;
                stick_knob_y = stick_base_y;
            } else {                              // floating: base + knob spawn under the finger
                stick_base_x = stick_knob_x = p.x;
                stick_base_y = stick_knob_y = p.y;
            }
            break;
        }
    }
}

// d-pad variant of update_stick(): identical grab-zone/finger-tracking, but resolves the held
// finger to direction booleans (angle-bucketed from the home centre) instead of a knob position.
// Both modes resolve to exactly ONE sector from the angle (never independent per-axis
// thresholds — that let almost any off-axis touch trip two unrelated directions at once).
// DPAD4 snaps to 4 sectors (dominant axis, no diagonals); DPAD8 snaps to 8 (adjacent cardinals
// combine into ONE diagonal sector, not a separate 3rd state layered on top of both cardinals).
static void update_dpad(void) {
    if (!show_touch_ui) {
        dpad_touch_id = -1;
        dpad_sector = -1;
        dpad_up = dpad_down = dpad_left = dpad_right = false;
        return;
    }

    bool  still_active = false;
    float dx = 0, dy = 0;
    if (dpad_touch_id != -1) {
        for (int i = 0; i < vt_count; i++) {
            if (vt_id[i] == dpad_touch_id) {
                dx = vt_pos[i].x - stick_home_x; dy = vt_pos[i].y - stick_home_y;
                still_active = true;
                break;
            }
        }
    }
    if (!still_active) dpad_touch_id = -1;

    if (dpad_touch_id == -1) {
        for (int i = 0; i < vt_count; i++) {
            Vector2 p = vt_pos[i];
            // local grab zone around the pad's own graphic — NOT the stick's whole-zone sgz;
            // a fixed d-pad shouldn't fire from a tap on the far side of the screen.
            if (!point_in_circle(p.x, p.y, stick_home_x, stick_home_y, DPAD_GRAB_RADIUS)) continue;
            if (touching_action_button(p.x, p.y)) continue;
            dpad_touch_id = vt_id[i];
            dx = p.x - stick_home_x; dy = p.y - stick_home_y;
            break;
        }
    }

    dpad_up = dpad_down = dpad_left = dpad_right = false;
    dpad_sector = -1;
    if (dpad_touch_id == -1) return;

    float len = sqrtf(dx*dx + dy*dy);
    if (len < DPAD_DEADZONE) return;

    if (touch_move_mode == TOUCH_DPAD8) {
        // one sector out of 8 (45° wide, centred on each compass point) — matches the draw
        // skin's node layout 1:1, so exactly one node lights per touch.
        float deg = atan2f(dy, dx) * RAD2DEG;                    // 0=right, 90=down (screen y-down)
        int sector = ((int)lroundf((deg + 90.0f) / 45.0f)) & 7;  // 0=up, clockwise; & 7 wraps negatives correctly
        static const bool SEC_UP[8]    = { 1,1,0,0,0,0,0,1 };
        static const bool SEC_RIGHT[8] = { 0,1,1,1,0,0,0,0 };
        static const bool SEC_DOWN[8]  = { 0,0,0,1,1,1,0,0 };
        static const bool SEC_LEFT[8]  = { 0,0,0,0,0,1,1,1 };
        dpad_sector = sector;
        dpad_up    = SEC_UP[sector];
        dpad_right = SEC_RIGHT[sector];
        dpad_down  = SEC_DOWN[sector];
        dpad_left  = SEC_LEFT[sector];
    } else {                                                    // DPAD4 → 4 sectors, dominant axis wins
        if (fabsf(dx) > fabsf(dy)) { if (dx < 0) { dpad_left = true; dpad_sector = 6; } else { dpad_right = true; dpad_sector = 2; } }
        else                       { if (dy < 0) { dpad_up   = true; dpad_sector = 0; } else { dpad_down  = true; dpad_sector = 4; } }
    }
}

// touch overlay — the desktop DRAW SKIN. Per the touch-controls architecture pivot the
// engine owns geometry (layout_touch_controls), hit-testing, and the btn()/stick synthesis;
// this just PAINTS smooth circles at those window-px positions, AFTER the blit. Controls are
// device-res chrome, not chunky in-canvas pixels. Web/iOS get their own thin skins later.
// Window-space ⇒ it does NOT appear in canvas-diff/dumps — eyeballed, not oracle-tested.
static void draw_touch_overlay_window(void) {
    if (!show_touch_ui) return;
    DeColor ring  = (DeColor){ 255, 255, 255,  70 };
    DeColor knob  = (DeColor){ 255, 255, 255, 160 };
    DeColor press = (DeColor){ 255, 255, 255, 110 };
    DeColor hint  = (DeColor){ 255, 255, 255,  40 };

    // movement control: analog stick (live base+knob while held) or d-pad (compass nodes;
    // exactly ONE lights, keyed off the resolved sector — never an AND-combo of booleans,
    // which used to light a diagonal's two cardinals AND the diagonal node all at once).
    float stick_r = eff_stick_r(), btn_r = eff_btn_r();
    if (touch_move_mode == TOUCH_DPAD4 || touch_move_mode == TOUCH_DPAD8) {
        DrawCircleLines((int)stick_home_x, (int)stick_home_y, stick_r, hint);
        int   n        = (touch_move_mode == TOUCH_DPAD8) ? 8 : 4;
        int   step     = 8 / n;              // 8 canonical compass positions; DPAD4 samples every other (the cardinals)
        float ring_r   = stick_r * 0.7f;
        // node dot radius as a fraction of stick_r (18/13 at full STICK_RADIUS=60) — scaling both
        // the ring and the dot together preserves the chord-vs-2r non-overlap margin at any size.
        float node_r   = stick_r * ((n <= 4) ? 0.3f : (13.0f / 60.0f));
        for (int i = 0; i < n; i++) {
            float deg = i * (360.0f / n) - 90.0f;   // 0 = up, clockwise
            float nx = stick_home_x + cosf(deg * DEG2RAD) * ring_r;
            float ny = stick_home_y + sinf(deg * DEG2RAD) * ring_r;
            DrawCircleV((Vector2){ nx, ny }, node_r, (dpad_sector == i * step) ? press : ring);
        }
    } else if (stick_touch_id != -1) {
        DrawCircleLines((int)stick_base_x, (int)stick_base_y, stick_r, ring);
        DrawCircleV((Vector2){ stick_knob_x, stick_knob_y }, stick_r * 0.45f, knob);
    } else {
        DrawCircleLines((int)stick_home_x, (int)stick_home_y, stick_r, hint);
        DrawCircleV((Vector2){ stick_home_x, stick_home_y }, stick_r * 0.45f, hint);
    }

    // action buttons — as many as the cart asked for (0-4: A/B/X/Y), diamond-laid by layout_action_buttons()
    static const char *btn_labels[4] = { "A", "B", "X", "Y" };
    float fs = 4 * SCALE;
    for (int i = 0; i < touch_n_buttons; i++) {
        bool down = any_touch_in_circle(btn_cx[i], btn_cy[i], btn_r);
        DrawCircle(btn_cx[i], btn_cy[i], btn_r, down ? press : ring);
        DrawCircleLines(btn_cx[i], btn_cy[i], btn_r, knob);
        DrawTextEx(game_font, btn_labels[i], (Vector2){ btn_cx[i] - fs/2, btn_cy[i] - fs/2 }, fs, 0, WHITE);
    }

    // dev aid (touch_debug(true)): the control BAND (deck/rail extent) and the current move
    // mode's GRAB ZONE — the stick's rectangular sgz, or the d-pad's local circular radius —
    // so placement/sizing/grab-zone work can be eyeballed without guessing at invisible rects.
    if (touch_debug_on) {
        DeColor band_col = (DeColor){   0, 255, 255, 160 };
        DeColor zone_col = (DeColor){ 255,   0, 255, 160 };
        if (place_mode != PLACE_OVERLAY) DrawRectangleLines(band_x, band_y, band_w, band_h, band_col);
        bool dpad = (touch_move_mode == TOUCH_DPAD4 || touch_move_mode == TOUCH_DPAD8);
        if (dpad) DrawCircleLines((int)stick_home_x, (int)stick_home_y, DPAD_GRAB_RADIUS, zone_col);
        else      DrawRectangleLines(sgz_x, sgz_y, sgz_w, sgz_h, zone_col);

        const char *mode_name = place_mode == PLACE_DECK ? "DECK" : place_mode == PLACE_RAILS ? "RAILS" : "OVERLAY";
        char dbg[80];
        snprintf(dbg, sizeof dbg, "touch_debug: %s  ctrl_scale=%.2f  btn=%.2f", mode_name, ctrl_scale, ctrl_scale_btn);
        DrawTextEx(game_font, dbg, (Vector2){ 6, 6 }, 4 * SCALE, 0, (DeColor){ 0, 255, 255, 255 });
    }
}

// ------------------------------------------------------------
// palette — the 32-color PICO-8 palette (matches the sprite editor's pico32)
// ------------------------------------------------------------

static void load_palette() {
    // standard 16 (0-15)
    palette[0]  = (DeColor){ 0,   0,   0,   255 }; // CLR_BLACK          #000000
    palette[1]  = (DeColor){ 29,  43,  83,  255 }; // CLR_DARK_BLUE      #1d2b53
    palette[2]  = (DeColor){ 126, 37,  83,  255 }; // CLR_DARK_PURPLE    #7e2553
    palette[3]  = (DeColor){ 0,   135, 81,  255 }; // CLR_DARK_GREEN     #008751
    palette[4]  = (DeColor){ 171, 82,  54,  255 }; // CLR_BROWN          #ab5236
    palette[5]  = (DeColor){ 95,  87,  79,  255 }; // CLR_DARK_GREY      #5f574f
    palette[6]  = (DeColor){ 194, 195, 199, 255 }; // CLR_LIGHT_GREY     #c2c3c7
    palette[7]  = (DeColor){ 255, 241, 232, 255 }; // CLR_WHITE          #fff1e8
    palette[8]  = (DeColor){ 255, 0,   77,  255 }; // CLR_RED            #ff004d
    palette[9]  = (DeColor){ 255, 163, 0,   255 }; // CLR_ORANGE         #ffa300
    palette[10] = (DeColor){ 255, 236, 39,  255 }; // CLR_YELLOW         #ffec27
    palette[11] = (DeColor){ 0,   228, 54,  255 }; // CLR_GREEN          #00e436
    palette[12] = (DeColor){ 41,  173, 255, 255 }; // CLR_BLUE           #29adff
    palette[13] = (DeColor){ 131, 118, 156, 255 }; // CLR_INDIGO         #83769c
    palette[14] = (DeColor){ 255, 119, 168, 255 }; // CLR_PINK           #ff77a8
    palette[15] = (DeColor){ 255, 204, 170, 255 }; // CLR_LIGHT_PEACH    #ffccaa
    // extended 16 (16-31) — undocumented "secret" colors
    palette[16] = (DeColor){ 41,  24,  20,  255 }; // CLR_BROWNISH_BLACK #291814
    palette[17] = (DeColor){ 17,  29,  53,  255 }; // CLR_DARKER_BLUE    #111d35
    palette[18] = (DeColor){ 66,  33,  54,  255 }; // CLR_DARKER_PURPLE  #422136
    palette[19] = (DeColor){ 18,  83,  89,  255 }; // CLR_BLUE_GREEN     #125359
    palette[20] = (DeColor){ 116, 47,  41,  255 }; // CLR_DARK_BROWN     #742f29
    palette[21] = (DeColor){ 73,  51,  59,  255 }; // CLR_DARKER_GREY    #49333b
    palette[22] = (DeColor){ 162, 136, 121, 255 }; // CLR_MEDIUM_GREY    #a28879
    palette[23] = (DeColor){ 243, 239, 125, 255 }; // CLR_LIGHT_YELLOW   #f3ef7d
    palette[24] = (DeColor){ 190, 18,  80,  255 }; // CLR_DARK_RED       #be1250
    palette[25] = (DeColor){ 255, 108, 36,  255 }; // CLR_DARK_ORANGE    #ff6c24
    palette[26] = (DeColor){ 168, 231, 46,  255 }; // CLR_LIME_GREEN     #a8e72e
    palette[27] = (DeColor){ 0,   181, 67,  255 }; // CLR_MEDIUM_GREEN   #00b543
    palette[28] = (DeColor){ 6,   90,  181, 255 }; // CLR_TRUE_BLUE      #065ab5
    palette[29] = (DeColor){ 117, 70,  101, 255 }; // CLR_MAUVE          #754665
    palette[30] = (DeColor){ 255, 110, 89,  255 }; // CLR_DARK_PEACH     #ff6e59
    palette[31] = (DeColor){ 255, 157, 129, 255 }; // CLR_PEACH          #ff9d81

    for (int i = 32; i < PALETTE_SIZE; i++) palette[i] = palette[i - 32];  // upper half mirrors 0-31 (see PALETTE_SIZE note)

    for (int i = 0; i < PALETTE_SIZE; i++) base_palette[i] = palette[i];   // keep an unmodified copy for pal_reset()
}

// ------------------------------------------------------------
// debug — printh() log + watch() overlay
// ------------------------------------------------------------

void printh(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);   // stderr: raylib's trace goes to stdout, so this stays clean
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);              // line-buffer through the pipe so the editor sees it promptly
}

typedef struct {
    char name[24];
    char value[40];
    int  age;                    // frames since last touched
} WatchEntry;

#define WATCH_MAX 40
static WatchEntry watches[WATCH_MAX];
static int        watch_count = 0;
static bool       watch_show  = true;   // F1 toggles

void watch(const char *name, const char *fmt, ...) {
    int slot = -1;
    for (int i = 0; i < watch_count; i++) {
        if (strcmp(watches[i].name, name) == 0) { slot = i; break; }
    }
    if (slot < 0) {
        if (watch_count >= WATCH_MAX) return;   // silently drop overflow
        slot = watch_count++;
        snprintf(watches[slot].name, sizeof(watches[slot].name), "%s", name);
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(watches[slot].value, sizeof(watches[slot].value), fmt, ap);
    va_end(ap);
    watches[slot].age = 0;
}

void watch_visible(bool on) { watch_show = on; }

// age entries each frame; drop any untouched for ~1 second so conditional
// watches vanish on their own when their branch stops firing
static void age_watches(void) {
    for (int i = watch_count - 1; i >= 0; i--) {
        if (++watches[i].age > 60) {
            watches[i] = watches[--watch_count];   // swap-remove
        }
    }
}

// drawn in window space (after the canvas is scaled up), like the touch overlay
static void draw_watch_overlay(void) {
    if (!watch_show || watch_count == 0) return;
    int pad = 8, lh = 14;
    int w = 220, h = pad*2 + watch_count * lh;

    DrawRectangle(8, 8, w, h, (DeColor){ 0, 0, 0, 180 });
    DrawRectangleLines(8, 8, w, h, (DeColor){ 200, 200, 200, 120 });

    for (int i = 0; i < watch_count; i++) {
        char line[80];
        snprintf(line, sizeof(line), "%s: %s", watches[i].name, watches[i].value);
        DrawTextEx(game_font, line,
            (Vector2){ 8 + pad, 8 + pad + i * lh },
            12, 1, WHITE);
    }
}

// live canvas/window size readout for resizable-layout debugging — top-left, window space (crisp,
// scale-independent). Opt-in via env DE_SHOW_SIZE=1 (works from play.js or `DE_SHOW_SIZE=1 make`) and
// only for a resizable cart (de_reflow). de_sw×de_sh is what screen_w()/screen_h() (and lay.h) see;
// win is the raw window px — the pair pinpoints a reflow bug (de≠win/SCALE) vs a placement one.
static bool size_overlay_on = false;   // set from getenv at boot; off by default
static void draw_size_overlay(void) {
    if (!de_reflow || !size_overlay_on) return;
    char line[64];
    snprintf(line, sizeof line, "%dx%d  win %dx%d", de_sw, de_sh, GetScreenWidth(), GetScreenHeight());
    Vector2 m = MeasureTextEx(game_font, line, 12, 1);
    DrawRectangle(6, 6, (int)m.x + 10, 20, (DeColor){ 0, 0, 0, 170 });
    DrawTextEx(game_font, line, (Vector2){ 11, 9 }, 12, 1, (DeColor){ 0, 255, 180, 255 });
}

bool paused(void) { return pause_active; }

// draw the pause overlay into the canvas (native resolution) so it scales up
// with the game and looks pixel-perfect — same renderer, same font, same pixels.
// Build the active pause-menu items into `items` (cap 3), return the count. The
// same builder drives the draw + the input below, so they can't drift.
// MULTIPLAYER shows only on a net build AND only for a cart that declares 2+
// players via de_players() — a solo cart's pause menu stays CONTINUE/RESTART.
static int pause_menu_items(const char *items[3]) {
    int k = 0;
    items[k++] = "CONTINUE";
#ifdef DE_NET_BUILD
    if (de_players() >= 2) items[k++] = "MULTIPLAYER";
#endif
    items[k++] = "RESTART";
    return k;
}

#ifdef DE_NET_BUILD
// Relaunch this exact binary with --net-lobby appended, so the fresh process
// boots into the Host/Join/Solo menu. Netplay needs its startup order (the
// host's seed must reach the joiner before init()), so entering net mid-game is
// a clean self-restart — the same execv() the RESTART item uses. Two standalone
// launches on one machine can each pause → MULTIPLAYER → one Hosts, one Joins
// 127.0.0.1. docs/design/multiplayer-research.md (rung 2.5).
static void net_restart_into_lobby(void) {
    if (!restart_argv) return;
    int argc = 0; while (restart_argv[argc]) argc++;
    for (int i = 0; i < argc; i++)                       // already flagged? relaunch as-is
        if (strcmp(restart_argv[i], "--net-lobby") == 0) { execv(restart_argv[0], restart_argv); return; }
    char **nv = (char **)malloc(sizeof(char *) * (size_t)(argc + 2));
    if (!nv) { execv(restart_argv[0], restart_argv); return; }
    for (int i = 0; i < argc; i++) nv[i] = restart_argv[i];
    nv[argc]     = (char *)"--net-lobby";
    nv[argc + 1] = NULL;
    execv(nv[0], nv);
    free(nv);                                            // reached only if execv failed
}
#endif

static void draw_pause_canvas(void) {
    if (!pause_active) return;
    const char *items[3]; int n_items = pause_menu_items(items);
    const int bw = 120, bh = 26 + n_items * 12;          // grows with the item count
    const int bx = (de_sw - bw) / 2, by = (de_sh - bh) / 2;


    fillp(0xA5A5, -1);
    rectfill(0, 0, de_sw, de_sh, CLR_BLACK);
    fillp_reset();

    rectfill(bx, by, bw, bh, CLR_DARKER_BLUE);
    rect(bx, by, bw, bh, CLR_INDIGO);

    const char *title = "PAUSED";
    print(title, bx + (bw - text_width(title)) / 2, by + 5, CLR_BLUE);

    for (int i = 0; i < n_items; i++) {
        int col = (pause_sel == i) ? CLR_WHITE : CLR_DARK_GREY;
        int ix  = bx + (bw - text_width(items[i])) / 2;
        int iy  = by + 20 + i * 12;
        if (pause_sel == i) print("\x10", ix - 10, iy, CLR_WHITE);
        print(items[i], ix, iy, col);
    }
}

// crash capture — on a fatal signal, dump the cart's last watch() values to
// stderr (so they land in the editor's runtime log), then re-raise so the OS
// still kills the process and the editor sees the real signal. Uses raw write()
// because printf-family calls aren't safe inside a signal handler.
#ifndef PLATFORM_WEB
static void write_str(const char *s) { write(STDERR_FILENO, s, strlen(s)); }

static void crash_handler(int sig) {
    write_str("\n*** cart crashed: signal ");
    char nbuf[12]; int n = 0, s = sig < 0 ? 0 : sig;
    char rev[12]; int r = 0;
    do { rev[r++] = '0' + (s % 10); s /= 10; } while (s > 0);
    while (r > 0) nbuf[n++] = rev[--r];
    nbuf[n] = '\0';
    write_str(nbuf);
    write_str(" ***\n");

    if (watch_count > 0) write_str("last watched values:\n");
    for (int i = 0; i < watch_count; i++) {
        write_str("  ");
        write_str(watches[i].name);
        write_str(" = ");
        write_str(watches[i].value);
        write_str("\n");
    }

    signal(sig, SIG_DFL);   // restore default and re-raise so the process dies for real
    raise(sig);
}
#endif

// ------------------------------------------------------------
// weak stubs — user can omit init() and/or update() and it still compiles
// ------------------------------------------------------------

__attribute__((weak)) void init(void)   {}
__attribute__((weak)) void update(void) {}
__attribute__((weak)) int  de_players(void) { return 1; }   // carts override with `return 2;` to enable multiplayer UI

#ifdef DE_SPEC
// ── spec() — the cart-LOGIC safety net (docs/design/spec-harness.md, runtime/spec.h). Built ONLY under
//    -DDE_SPEC (the `node tools/spec.js` runner); a normal build compiles none of this. The cart defines
//    spec() and drives the headless loop with step()/expect(); we reuse the existing inject-input buffers
//    (the same machinery behind --replay/--script), so keyp()/keyr() edges fire correctly. ──
#include "spec.h"
__attribute__((weak)) void spec(void) {}             // no spec() in this cart ⇒ the runner skips it
static int  spec_mode = 0;                           // --spec: run spec() headless instead of the game loop
static int  spec_pass = 0, spec_fail = 0, spec_inited = 0;
static unsigned char key_pending[KEYSTATE_N];        // desired key state; applied at each step's frame start

void key_down(int k){ if (k >= 0 && k < KEYSTATE_N) key_pending[k] = 1; }
void key_up  (int k){ if (k >= 0 && k < KEYSTATE_N) key_pending[k] = 0; }

void step(int n){
    if (!spec_inited) { init(); spec_inited = 1; }
    for (int i = 0; i < n; i++){
        memcpy(key_inject_prev, key_inject, sizeof key_inject);   // prev = last frame's state (for keyp/keyr edges)
        memcpy(key_inject,      key_pending, sizeof key_inject);  // cur  = desired (a fresh key_down ⇒ a press edge)
        frame_dt = (float)DET_DT;          // spec steps tick the det_mode clock — a cart integrating
        det_clock += DET_DT;               // with dt()/now() advances 1/60 per step, not 0 (bit sloop's spec)
        update();
        frame_count++;
    }
}
void expect(int cond, const char *msg){
    if (cond) spec_pass++; else spec_fail++;
    printf("{\"pass\":%d,\"msg\":\"%s\"}\n", cond ? 1 : 0, msg ? msg : "");
    fflush(stdout);
}
void expect_eq(long got, long want, const char *msg){
    int ok = (got == want);
    if (ok) spec_pass++; else spec_fail++;
    printf("{\"pass\":%d,\"msg\":\"%s\",\"got\":%ld,\"want\":%ld}\n", ok ? 1 : 0, msg ? msg : "", got, want);
    fflush(stdout);
}
#endif

// ------------------------------------------------------------
// main + runtime loop
// ------------------------------------------------------------

#ifndef PLATFORM_WEB
// per-frame harness work. fno is the 0-based index of the frame about to run
// (== frame_count before its end-of-frame increment), so it lines up between a
// --record run and the --replay that feeds the events back.
static void harness_input(int fno) {
    if (inject_input) {                                  // replay/script drives keys + mouse
        memcpy(key_inject_prev, key_inject, sizeof key_inject);
        memcpy(mbtn_inj_prev,   mbtn_inj,   sizeof mbtn_inj);
        wheel_inj = 0.0f;                                // wheel is transient — reset, set only on its frame
        while (replay_i < replay_n && replay_ev[replay_i].frame <= fno) {
            InputEvent e = replay_ev[replay_i++];
            if      (e.kind == 0) { if (e.a >= 0 && e.a < KEYSTATE_N) key_inject[e.a] = (unsigned char)(e.b ? 1 : 0); }
            else if (e.kind == 1) { mouse_inj_x = e.a; mouse_inj_y = e.b; }
            else if (e.kind == 2) { if (e.a >= 0 && e.a < 3) mbtn_inj[e.a] = (unsigned char)(e.b ? 1 : 0); }
            else if (e.kind == 3) { wheel_inj += (float)e.a; }
        }
    }
    if (rec_file) {                                      // log live key + mouse changes for replay
        for (int k = 0; k < KEYSTATE_N; k++) {
            unsigned char d = IsKeyDown(k) ? 1 : 0;
            if (d != key_rec_prev[k]) { fprintf(rec_file, "%d k %d %d\n", fno, k, d); key_rec_prev[k] = d; }
        }
        int mx = inp_mouse_x(), my = inp_mouse_y();
        if (mx != mouse_rec_px || my != mouse_rec_py) {
            fprintf(rec_file, "%d m %d %d\n", fno, mx, my); mouse_rec_px = mx; mouse_rec_py = my;
        }
        static const int BTNS[3] = { MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE };
        for (int b = 0; b < 3; b++) {
            unsigned char d = IsMouseButtonDown(raylib_mouse_button(BTNS[b])) ? 1 : 0;
            if (d != mbtn_rec_prev[b]) { fprintf(rec_file, "%d b %d %d\n", fno, b, d); mbtn_rec_prev[b] = d; }
        }
        float wm = inp_mouse_wheel();                    // wheel is transient — log the frames it moves
        if (wm != 0.0f) fprintf(rec_file, "%d w %d 0\n", fno, (int)wm);
        fflush(rec_file);                                // per-frame flush so a live tail sees it
    }
}

static void json_str(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { fputc('\\', f); fputc(*s, f); }
        else if (*s == '\n')         { fputs("\\n", f); }
        else                           fputc(*s, f);
    }
    fputc('"', f);
}

// one JSONL line per frame: the auto fields plus every watch() value set this
// frame (age 0 — age_watches() runs after us). Reading a session = reading this.
static void harness_trace(int fno) {
    if (!trace_file) return;
    fprintf(trace_file, "{\"f\":%d,\"t\":%.4f,\"beat\":%d,\"bpos\":%.4f,\"w\":{",
            fno, clk(), beat(), beat_pos());
    int first = 1;
    for (int i = 0; i < watch_count; i++) {
        if (watches[i].age != 0) continue;
        if (!first) fputc(',', trace_file);
        first = 0;
        json_str(trace_file, watches[i].name);
        fputc(':', trace_file);
        json_str(trace_file, watches[i].value);
    }
    fputs("}}\n", trace_file);
    fflush(trace_file);
}

// --uiaudit: flush this frame's recorded draw boxes as one JSONL line, then
// reset the buffer. sw/sh = screen bounds (so the reader needs no -D flags);
// d[] carries {kind, x, y, w, h, clip-active} plus the string for text boxes.
static void uiaudit_flush(int fno) {
    if (!uiaudit_file) return;
    fprintf(uiaudit_file, "{\"f\":%d,\"sw\":%d,\"sh\":%d,\"d\":[", fno, de_sw, de_sh);
    for (int i = 0; i < uiaudit_n; i++) {
        UiAuditRec *r = &uiaudit_recs[i];
        if (i) fputc(',', uiaudit_file);
        fprintf(uiaudit_file, "{\"k\":\"%c\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"c\":%d",
                r->kind, r->x, r->y, r->w, r->h, r->clip);
        if (r->kind == 't' || r->kind == 'w') { fputs(",\"t\":", uiaudit_file); json_str(uiaudit_file, r->text); }
        fputc('}', uiaudit_file);
    }
    fputs("]}\n", uiaudit_file);
    fflush(uiaudit_file);
    uiaudit_n = 0;
}

static void harness_dump(int fno) {
    if (resize_n > 0) return;   // a --resize sweep owns the dumping (labeled captures below)
    if (dump_every <= 0 || (fno % dump_every) != 0) return;
    Image shot = LoadImageFromTexture(canvas.texture);
    ImageFlipVertical(&shot);
    char path[320];
    snprintf(path, sizeof path, "%s/frame_%05d.png", dump_dir[0] ? dump_dir : ".", fno);
    ExportImage(shot, path);
    UnloadImage(shot);
}

// --resize sweep: at the start of each segment set the active canvas to the scripted size (and
// resize the real window when one is shown, so you watch it live). de_sw/de_sh drive layout for a
// resizable cart; set directly so it's deterministic even --headless (where no resize event fires).
static void harness_resize_step(void) {
    if (resize_n <= 0) return;
    int seg = frame_count / RESIZE_HOLD;
    if (seg >= resize_n) seg = resize_n - 1;
    if (seg == resize_last_seg) return;
    resize_last_seg = seg;
    de_set_canvas(resize_w[seg], resize_h[seg]);   // clamp to DE_MAX + grow the framebuffer to fit
#ifndef PLATFORM_WEB
    if (!IsWindowState(FLAG_WINDOW_HIDDEN)) SetWindowSize(de_sw * SCALE, de_sh * SCALE);   // visible: resize the real window too
#endif
}

// capture one cropped, size-labeled frame at the LAST (settled) frame of each segment.
static void harness_resize_capture(int fno) {
    if (resize_n <= 0 || dump_dir[0] == 0) return;
    if ((fno % RESIZE_HOLD) != RESIZE_HOLD - 1) return;
    int seg = fno / RESIZE_HOLD;
    if (seg >= resize_n) return;
    Image shot = LoadImageFromTexture(canvas.texture);
    ImageFlipVertical(&shot);
    ImageCrop(&shot, (Rectangle){ 0, 0, (float)de_sw, (float)de_sh });   // just the on-screen (active) region
    char path[360];
    snprintf(path, sizeof path, "%s/resize_%02d_%dx%d.png", dump_dir, seg, de_sw, de_sh);
    ExportImage(shot, path);
    UnloadImage(shot);
}

// forward declaration — prof_write is defined in the profiler block below
#ifndef PLATFORM_WEB
static void prof_write(const char *path);
#endif

// on-demand inspection: poll .bake/screenshot_request and .bake/state_request each
// frame. each file contains the desired output path; we capture, write, then delete
// the request file as the handshake — gone means fresh and ready to read.
//
// TARGETING (optional): any extra line "pid:<n>" addresses the request to ONE process.
// Every native cart runs with cwd=build/, so when several are alive at once (the editor's
// cart + a play.js run, or a multi-cart bundle next to anything) an untargeted request is
// served by WHICHEVER polls first — the wrong app's screenshot, with no error (bit the
// build-app.js verification, 2026-07-03). A request naming another pid is left untouched.
static int harness_take_request(const char *path, char *out, size_t outsz, char *extra, size_t extrasz) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    out[0] = '\0';
    if (extra && extrasz) extra[0] = '\0';
    long target = -1;
    int first = 1;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n\r")] = '\0';
        if (first) { snprintf(out, outsz, "%s", line); first = 0; }
        else if (strncmp(line, "pid:", 4) == 0) target = atol(line + 4);
        else if (extra && extrasz && !extra[0]) snprintf(extra, extrasz, "%s", line);
    }
    fclose(f);
    if (target >= 0 && target != (long)getpid()) return 0;   // addressed to another process — leave it
    remove(path);
    return out[0] != '\0';
}

static void harness_inspect(int fno) {
    char out[512];
    // screenshot
    if (harness_take_request(".bake/screenshot_request", out, sizeof out, NULL, 0)) {
        Image shot = LoadImageFromTexture(canvas.texture);
        ImageFlipVertical(&shot);
        ExportImage(shot, out);
        UnloadImage(shot);
    }
    // window screenshot — the window BACKBUFFER, post-blit, so it includes window-space draw
    // skins (e.g. the touch-controls overlay, drawn after the canvas blit) that screenshot_request
    // above (canvas.texture only) can never see. See docs/design/touch-controls.md PIVOT.
    if (harness_take_request(".bake/window_screenshot_request", out, sizeof out, NULL, 0)) {
        Image shot = LoadImageFromScreen();
        ExportImage(shot, out);
        UnloadImage(shot);
    }
    // state
    if (harness_take_request(".bake/state_request", out, sizeof out, NULL, 0)) {
        {
            FILE *w = fopen(out, "w");
            if (w) {
                fprintf(w, "{\"f\":%d,\"t\":%.4f,\"beat\":%d,\"bpos\":%.4f,\"w\":{",
                        fno, clk(), beat(), beat_pos());
                int first = 1;
                for (int i = 0; i < watch_count; i++) {
                    if (!first) fputc(',', w);
                    first = 0;
                    json_str(w, watches[i].name);
                    fputc(':', w);
                    json_str(w, watches[i].value);
                }
                fputs("}}\n", w);
                fclose(w);
            }
        }
    }
    // profiler snapshot
    if (harness_take_request(".bake/profiler_request", out, sizeof out, NULL, 0))
        prof_write(out);
    // WAV capture: line 1 = output path, optional extra line = seconds (default 5, cap 60).
    // The audio thread fills the buffer from its mix tap; we poll for completion below.
    char dur[64];
    if (harness_take_request(".bake/wav_request", out, sizeof out, dur, sizeof dur)) {
        float secs = (float)atof(dur);
        if (secs <= 0.0f) secs = 5.0f;
        if (secs > 60.0f) secs = 60.0f;
        sound_wavcap_begin(out, secs);
    }
    sound_wavcap_poll();   // recording finished → write the WAV, go idle
}

// ── --wav: deterministic audio render ────────────────────────────────────
// With --wav the device stream is never started (sound_synth_mode); instead the
// main loop pumps sound_callback() for exactly 44100/60 = 735 samples per frame
// and streams them to a WAV. Same frames + same script + same seed → the same
// bytes, which is what makes golden-WAV regression diffs possible (§16).
static FILE *wav_out     = NULL;
static int   wav_samples = 0;

static void wav_stream_open(const char *path) {
    wav_out = fopen(path, "wb");
    if (!wav_out) { fprintf(stderr, "harness: cannot open --wav %s\n", path); return; }
    int z = 0, sr = SOUND_SAMPLE_RATE, byterate = sr * 4, fmtlen = 16;
    short fmt = 1, ch = 2, block = 4, bits = 16;   // stereo (interleaved L,R; stereo.md)
    fwrite("RIFF", 1, 4, wav_out); fwrite(&z, 4, 1, wav_out); fwrite("WAVE", 1, 4, wav_out);
    fwrite("fmt ", 1, 4, wav_out); fwrite(&fmtlen, 4, 1, wav_out);
    fwrite(&fmt, 2, 1, wav_out); fwrite(&ch, 2, 1, wav_out);
    fwrite(&sr, 4, 1, wav_out); fwrite(&byterate, 4, 1, wav_out);
    fwrite(&block, 2, 1, wav_out); fwrite(&bits, 2, 1, wav_out);
    fwrite("data", 1, 4, wav_out); fwrite(&z, 4, 1, wav_out);
}

static void wav_stream_pump(void) {
    if (!wav_out) return;
    float scratch[735 * 2];   // stereo: the callback writes 2 interleaved samples per frame
    short pcm[735 * 2];
    sound_callback(scratch, 735);
    for (int i = 0; i < 735 * 2; i++) pcm[i] = (short)(scratch[i] * 32767.0f);
    fwrite(pcm, 2, 735 * 2, wav_out);
    wav_samples += 735 * 2;   // counts int16 SAMPLES written (2 per frame), not frames
}

static void wav_stream_close(void) {
    if (!wav_out) return;
    int data_bytes = wav_samples * 2, riff = 36 + data_bytes;
    fseek(wav_out, 4, SEEK_SET);  fwrite(&riff, 4, 1, wav_out);
    fseek(wav_out, 40, SEEK_SET); fwrite(&data_bytes, 4, 1, wav_out);
    fclose(wav_out);
    wav_out = NULL;
}
#endif

// ── profiler instrumentation (always compiled in for native builds) ──────
// Counts draw-primitive calls and times the CPU work per frame (update+draw,
// excluding the vsync wait). A re-entrancy guard means only the OUTERMOST
// public call is counted, so circfill()→trifill() reads as one circfill, not
// also a trifill. Flushed to build/perf.json every 30 frames; also written
// on demand via .bake/profiler_request (same trigger-file pattern as
// screenshot_request / state_request). -DDE_PROFILE adds the macOS `sample`
// hotspot pass the editor's Profile button uses; counters work without it.
#ifndef PLATFORM_WEB
typedef struct { const char *name; long calls; } ProfCounter;
static ProfCounter prof_counters[64];
static int    prof_counter_n   = 0;
static int    prof_depth       = 0;
static double prof_work_total  = 0.0;   // ms, summed update+draw (CPU work)
static double prof_work_max    = 0.0;
static double prof_frame_total = 0.0;   // ms, summed full frame (incl. vsync)
static long   prof_frames      = 0;
static float  prof_work_samples[4096];  // per-frame work ms, for the editor's p95
static int    prof_work_n      = 0;

static void prof_bump(const char *name) {
    // identical string literals share storage within this TU, so pointer
    // identity dedupes; the editor also merges by name, in case it doesn't.
    for (int i = 0; i < prof_counter_n; i++)
        if (prof_counters[i].name == name) { prof_counters[i].calls++; return; }
    if (prof_counter_n < 64) {
        prof_counters[prof_counter_n].name  = name;
        prof_counters[prof_counter_n].calls = 1;
        prof_counter_n++;
    }
}

typedef struct { int unused; } ProfGuard;
static ProfGuard prof_push(const char *name) {
    if (prof_depth == 0) prof_bump(name);   // only the outermost public call
    prof_depth++;
    return (ProfGuard){ 0 };
}
static void prof_pop(ProfGuard *g) { (void)g; prof_depth--; }

static void prof_write(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    long frames = prof_frames > 0 ? prof_frames : 1;
    fprintf(f, "{\"frames\":%ld,\"workMsAvg\":%.4f,\"workMsMax\":%.4f,\"frameMsAvg\":%.4f,\"calls\":[",
            prof_frames, prof_work_total / frames, prof_work_max, prof_frame_total / frames);
    for (int i = 0; i < prof_counter_n; i++)
        fprintf(f, "%s{\"name\":\"%s\",\"total\":%ld}",
                i ? "," : "", prof_counters[i].name, prof_counters[i].calls);
    // raw per-frame work times so the editor can report p95 (robust to one-off
    // stalls — e.g. the frame `sample` attaches and suspends our threads)
    fprintf(f, "],\"work\":[");
    for (int i = 0; i < prof_work_n; i++)
        fprintf(f, "%s%.3f", i ? "," : "", prof_work_samples[i]);
    fprintf(f, "]}\n");
    fclose(f);
}
#define PROF(n) ProfGuard _prof_g __attribute__((cleanup(prof_pop))) = prof_push(n)
#else
#define PROF(n) ((void)0)
#endif
// release build strips all profiler + inspection overhead
#ifdef DE_RELEASE
#undef PROF
#define PROF(n) ((void)0)
#endif

static void loop_step(void) {
#if !defined(PLATFORM_WEB) && !defined(DE_NO_RAYLIB)
    // Phase 1b: a resizable cart reflows the active canvas to (window / SCALE) each time the window
    // changes size. lay.h (immediate-mode) recomputes every rect from the new screen_w()/screen_h()
    // next frame, so the relayout is free; audio/knob state lives apart, untouched. No-op unless the
    // cart opted in with -DDE_RESIZABLE, so fixed carts never move.
    if (de_reflow && IsWindowResized())
        de_set_canvas(GetScreenWidth() / SCALE, GetScreenHeight() / SCALE);   // clamps to DE_MAX + grows the framebuffer
    harness_resize_step();   // --resize sweep overrides de_sw/de_sh for the current scripted segment
#endif
#ifdef PLATFORM_WEB
    if (!web_started) {
        bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
                    || IsKeyPressed(KEY_ENTER)
                    || IsKeyPressed(KEY_SPACE);
        if (clicked) {
#ifndef DE_AUDIO_WORKLET
            InitAudioDevice();        // raylib audio (ScriptProcessor); the worklet build uses its own context
#endif
            sound_init();
#ifdef DE_AUDIO_WORKLET
            sound_worklet_resume();   // resume the worklet's AudioContext within the click gesture
#endif
            init();
            web_started = true;
        }
        BeginTextureMode(canvas);
        ClearBackground(palette[0]);
        const char *msg = "click to start";
        int tw = (int)(strlen(msg) * 8);
        DrawTextEx(game_font, msg,
            (Vector2){ (de_sw - tw) / 2.0f, de_sh / 2.0f - 4 },
            8, 0, palette[7]);
        EndTextureMode();
        BeginDrawing();
        DrawTexturePro(canvas.texture,
            (Rectangle){ 0, 0, de_sw, -de_sh },
            (Rectangle){ 0, 0, de_sw * SCALE, de_sh * SCALE },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
        return;
    }
    frame_dt = GetFrameTime();
    // clamp hitches like the native path does (:1067). On web frame_dt is the rAF
    // delta and is otherwise unbounded — a GC pause / scroll / tab-work stall would
    // dump the whole gap into the beat clock at once (beat_accum += dt, sound.h),
    // lurching the sequence forward and skipping beats. See design/audio-timing.md.
    if (frame_dt > 0.1f) frame_dt = 0.1f; if (frame_dt < 0) frame_dt = 0;
    sound_tick(frame_dt);
#else
    // delta time for dt()/the musical clock. det_mode pins it to a fixed step so
    // the beat, the falling notes and the judge windows replay identically; a live
    // run uses the wall clock, clamped so a stalled frame can't teleport things.
    if (det_mode) {
        frame_dt = (float)DET_DT;
    } else {
        double tn = GetTime(); frame_dt = (float)(tn - last_time); last_time = tn;
        if (frame_dt > 0.1f) frame_dt = 0.1f; if (frame_dt < 0) frame_dt = 0;
    }
    sound_tick(frame_dt);
    int fno = frame_count;                 // 0-based index of the frame we're about to run
    harness_input(fno);                    // apply replay/script keys + record live keys
#endif
#ifdef DE_TCC
    cart_reload_if_changed();   // file-watch hot reload (cart re-JITs, state persists)
#endif
#ifndef DE_NO_RAYLIB
    // place the game in the window each frame (handles DE_WINDOW / a resized or device window) and
    // lay the controls into the resulting band. At the default size (window == game) the placement
    // is the identity overlay → game_rect + control positions are byte-identical to before.
    { Placement pl = gr_place(GetScreenWidth(), GetScreenHeight(), de_sw, de_sh);
      game_rect = pl.game; layout_touch_controls(pl);
      place_mode = pl.mode; band_x = pl.band_x; band_y = pl.band_y; band_w = pl.band_w; band_h = pl.band_h; }
#endif
    poll_virtual_touches();
    if (touch_move_mode == TOUCH_DPAD4 || touch_move_mode == TOUCH_DPAD8) update_dpad(); else update_stick();
    if (inp_pressed(KEY_F1)) watch_show = !watch_show;

    // pause overlay — PAUSE_KEY or ENTER toggles; when open ESC resumes instead
    // of closing the window. Keys the cart reads itself are claimed and skipped.
    SetExitKey(pause_active ? KEY_NULL : KEY_ESCAPE);
    bool pause_allowed = true;
#ifdef DE_NET_BUILD
    if (net_active) pause_allowed = false;   // pausing would stall the lockstep peer (rung-1 scope)
#endif
    bool pause_opened_now = false;
    if (pause_allowed
        && ((inp_pressed(PAUSE_KEY) && !key_claimed[PAUSE_KEY])
        || (!pause_active && inp_pressed(KEY_ENTER) && !key_claimed[KEY_ENTER]))) {
        pause_active = !pause_active;
        pause_sel = 0;
        pause_opened_now = pause_active;   // don't let the menu eat the same press
        de_master_volume(pause_active ? 0.0f : 1.0f);
    }
    if (pause_active && !pause_opened_now) {
        if (inp_pressed(KEY_ESCAPE)) {
            pause_active = false;
            de_master_volume(1.0f);
        } else if (inp_pressed(KEY_UP))   { const char *it[3]; int nn = pause_menu_items(it); pause_sel = (pause_sel + nn - 1) % nn; }
        else if (inp_pressed(KEY_DOWN))   { const char *it[3]; int nn = pause_menu_items(it); pause_sel = (pause_sel + 1) % nn; }
        else if (inp_pressed(KEY_ENTER) || inp_pressed(KEY_Z)) {
            const char *items[3]; int nn = pause_menu_items(items);
            const char *sel = pause_sel < nn ? items[pause_sel] : "CONTINUE";
            if (strcmp(sel, "CONTINUE") == 0) {          // Continue
                pause_active = false;
                de_master_volume(1.0f);
#ifdef DE_NET_BUILD
            } else if (strcmp(sel, "MULTIPLAYER") == 0) {  // relaunch into the Host/Join/Solo lobby
                net_restart_into_lobby();
#endif
            } else {                        // Restart
                if (restart_argv) execv(restart_argv[0], restart_argv);
            }
        }
    }
    frame_count++;                        // advance even while paused so --frames and dump filenames still work
#ifdef PLATFORM_WEB
    // render cadence: on off-ticks skip the canvas redraw + the present (the GPU/heat
    // cost) while update()/sound_tick()/input keep running. Never skip while paused (the
    // menu must stay responsive). 1 = every tick (default → always false here).
    bool skip_render = (RENDER_EVERY > 1) && !pause_active && (frame_count % RENDER_EVERY != 0);
#else
    const bool skip_render = false;       // native always renders (pacing lives in EndDrawing)
#endif
    if (pause_active) goto draw_window;   // skip update() + draw() — last frame stays frozen

    // snapshot last frame's canvas so pget() has stable pixels to read — but only if a
    // cart opted in via enable_pget(). Carts that never read pixels pay nothing. Skipped
    // on render-cadence off-ticks too: the canvas didn't change, so the last snapshot still
    // matches (pget/touching_color in update() read it fine).
    if (!skip_render && pget_enabled) {
#ifndef PLATFORM_WEB
        if (pget_snapshot.data) UnloadImage(pget_snapshot);
        pget_snapshot       = LoadImageFromTexture(canvas.texture);
        pget_snapshot_valid = pget_snapshot.data != NULL;
#else
        // web: raylib's LoadImageFromTexture runs an ES3-only framebuffer format probe
        // that throws a cosmetic INVALID_ENUM every frame on WebGL1. We don't need the
        // probe — the canvas is always RGBA8 — so read the FBO ourselves with a plain
        // glReadPixels (the universal GL subset, in every backend raylib supports) into a
        // reused buffer. No probe (clean console), no per-frame alloc (no churn). Bottom-up
        // RGBA, same orientation LoadImageFromTexture returns, so pget's Y-flip is unchanged.
        // Native paths (incl. any future native target) keep using raylib — this is web-only.
        {
            static unsigned char *web_px = NULL;
            int w = canvas.texture.width, h = canvas.texture.height;
            if (!web_px) web_px = (unsigned char *)malloc((size_t)w * h * 4);
            if (web_px) {
                rlEnableFramebuffer(canvas.id);                              // bind via rlgl (portable, no GL header)
                glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, web_px); // raw read — no format probe
                rlDisableFramebuffer();
                pget_snapshot = (Image){ .data = web_px, .width = w, .height = h,
                                         .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, .mipmaps = 1 };
                pget_snapshot_valid = true;
            } else {
                pget_snapshot_valid = false;
            }
        }
#endif
    } else if (!skip_render) {
        pget_snapshot_valid = false;
    }
#ifdef DE_NET_BUILD
    // lockstep barrier: exchange this frame's btn() bits with the peer — after
    // this, btn() reads the resolved bytes (so the edge snapshot below and the
    // cart's update() see identical input on both machines)
    if (net_active) net_frame_sync();
#endif
    // snapshot input edges before update so btnp() works
    for (int p = 0; p < 2; p++)
        for (int b = 0; b < BTN_COUNT; b++) {
            btn_prev[p][b] = btn_curr[p][b];
            btn_curr[p][b] = btn(p, b);
        }
    // characters typed this frame for text_input()
    { int n = 0, ch; while ((ch = GetCharPressed()) != 0 && n < 31) text_buf[n++] = (char)ch; text_buf[n] = 0; }
    // EXPERIMENTAL: capture a file dropped onto the window (valid this frame only)
    de_drop_valid = 0;
    if (IsFileDropped()) {
        FilePathList fl = LoadDroppedFiles();
        if (fl.count > 0) { snprintf(de_drop_buf, sizeof de_drop_buf, "%s", fl.paths[0]); de_drop_valid = 1; }
        UnloadDroppedFiles(fl);
    }
    // fade is immediate-mode like every other draw call: it clears each frame, so a
    // cart must re-assert fade() every frame it wants the screen dimmed. This is why
    // a conditional `if (gameover) fade(0.5f);` clears itself once the state ends —
    // carts never need to call fade(0) by hand.
    fade_amt = 0.0f;
#if !defined(PLATFORM_WEB) && !defined(DE_RELEASE)
    double prof_t0 = GetTime();
#endif
#ifdef DE_TCC
    if (cart_update) cart_update();
#else
    update();
#endif

    // per-frame software-canvas decision: enabled, AND this cart hasn't used a rotation camera_ex
    // (which falls back to the GPU path — Fork-2/C). Recomputed each frame after update() so a
    // camera_ex in update() is seen; a rotation in THIS frame's draw() flips it next frame.
    sw_canvas_active = sw_canvas_enabled && !sw_force_gpu;

    // draw into the low-res canvas, under the camera matrix (handles scroll + zoom +
    // rotation on the GPU). camera()/camera_ex() called inside draw() re-apply via
    // cam_reapply() while cam_active is true.
    if (!skip_render) {        // render-cadence off-tick: keep the canvas (retains last frame)
    if (sw_canvas_active) {
        // software canvas: primitives write sw_cbuf (no GPU draw, no BeginMode2D — camera()
        // translation is applied as a write offset in sw_pset). One UpdateTexture replaces the
        // whole canvas → zero rlVertex3f. Present (below) un-flips with a +SCREEN_H source rect.
        cam_active = true;     // so camera()/camera_ex() inside draw() still update `cam`
#ifdef DE_TCC
        if (cart_draw) cart_draw();
#else
        draw();
#endif
        cam_active = false;
        UpdateTexture(canvas.texture, sw_cbuf);
    } else {
    BeginTextureMode(canvas);
        BeginMode2D(cam);
        cam_active = true;
#ifdef DE_TCC
        if (cart_draw) cart_draw();
#else
        draw();
#endif
        if (smooth_capturing) smooth_composite();   // cart never called camera() — composite now
        cam_active = false;
        EndMode2D();
    EndTextureMode();
    }
    }   // end if (!skip_render) — canvas redraw
#if !defined(PLATFORM_WEB) && !defined(DE_RELEASE)
    {
        // skip the first few frames: they carry one-time costs (texture/font
        // loads, first GL submit) that would dominate the peak misleadingly.
        if (frame_count > 3) {
            double w = (GetTime() - prof_t0) * 1000.0;   // update+draw CPU, ms
            prof_work_total += w;
            if (w > prof_work_max) prof_work_max = w;
            if (prof_work_n < 4096) prof_work_samples[prof_work_n++] = (float)w;
            prof_frame_total += frame_dt * 1000.0;        // full frame incl. vsync
            prof_frames++;
        }
        if (frame_count % 30 == 0) prof_write("perf.json"); // periodic flush (survives kill)
    }
#endif
    if (clip_active) { EndScissorMode(); clip_active = false; }

    draw_window:;   // pause skips update+draw above; canvas shows last frozen frame
    if (pause_active) {
        if (sw_canvas_active) {
            // software canvas: the overlay's primitives write sw_cbuf, but the
            // per-frame UpdateTexture(canvas.texture, sw_cbuf) lives in the redraw
            // block we just skipped — so re-upload here or the present blits a
            // stale (overlay-less) texture. (GPU path draws straight to the texture.)
            draw_pause_canvas();
            UpdateTexture(canvas.texture, sw_cbuf);
        } else {
            BeginTextureMode(canvas);   // draw overlay at native resolution — scales up with the game
                draw_pause_canvas();
            EndTextureMode();
        }
    }
    // scale up to the window — RenderTexture is flipped in Y
    float sox = 0, soy = 0;
    if (shake_amt > 0.2f) {
        sox = ((rand() % 201) - 100) / 100.0f * shake_amt * SCALE;
        soy = ((rand() % 201) - 100) / 100.0f * shake_amt * SCALE;
    }
    if (!skip_render) {
    BeginDrawing();
        ClearBackground(BLACK);   // letterbox bars when the window is larger than the game rect (no-op at default size — the blit covers everything)
        bool present_sharp = scale_shader_ok && (SCALE_FILTER >= 2);
        if (present_sharp) {
            float ts[2] = { (float)de_sw, (float)de_sh };
            int   g = (SCALE_FILTER == 3) ? 1 : 0;
            SetShaderValue(scale_shader, loc_scale_texsize, ts, SHADER_UNIFORM_VEC2);
            SetShaderValue(scale_shader, loc_scale_gamma, &g, SHADER_UNIFORM_INT);
            BeginShaderMode(scale_shader);
        }
        DrawTexturePro(
            canvas.texture,
            (Rectangle){ 0, 0,  de_sw, -de_sh },   // sample only the active sub-region (top-left of the max-size canvas)
            (Rectangle){ game_rect.x + sox, game_rect.y + soy,
                         de_sw * game_rect.scale, de_sh * game_rect.scale },   // game_rect drives placement
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );
        if (present_sharp) EndShaderMode();
        if (fade_amt > 0.0f)
            DrawRectangle(0, 0, de_sw * SCALE, de_sh * SCALE,
                          (DeColor){ 0, 0, 0, (unsigned char)(fade_amt * 255) });
        draw_touch_overlay_window();   // the on-screen controls (window-space, after the blit)
        draw_watch_overlay();
        draw_size_overlay();           // resizable carts: live WxH readout, top-left
    EndDrawing();
    }   // end if (!skip_render) — present
#ifdef PLATFORM_WEB
    else PollInputEvents();   // present skipped: EndDrawing normally polls input, so do it ourselves or the loop goes deaf
#endif
    if (shake_amt > 0) { shake_amt *= 0.85f; if (shake_amt < 0.2f) shake_amt = 0; }

#ifndef PLATFORM_WEB
    harness_trace(fno);                    // structured state for this frame (before aging)
    uiaudit_flush(fno);                    // --uiaudit: this frame's draw bounding boxes
    harness_dump(fno);                     // filmstrip PNG every Nth frame
    harness_resize_capture(fno);           // --resize sweep: one labeled PNG per settled size
#ifndef DE_RELEASE
    harness_inspect(fno);                  // on-demand screenshot + state (trigger-file)
    wav_stream_pump();                     // --wav: render this frame's 735 samples
#endif
    if (det_mode) det_clock += DET_DT;     // advance the synthetic clock for now()/timer()
#endif
    age_watches();   // frame-end: expire watches whose branch stopped firing
}

#ifndef PLATFORM_WEB
// resolve a script token to a raylib key code: a single char is its uppercased
// ASCII value (letters/digits/punctuation line up with raylib's codes), or a few
// named keys for the ones with no obvious glyph.
static int key_code(const char *tok) {
    if (!tok || !tok[0]) return -1;
    if (!tok[1]) {                                   // single character
        int c = (unsigned char)tok[0];
        if (c >= 'a' && c <= 'z') c -= 32;           // raylib letter codes are uppercase
        return c;
    }
    if (!strcmp(tok, "SPACE")) return KEY_SPACE;
    if (!strcmp(tok, "ENTER")) return KEY_ENTER;
    if (!strcmp(tok, "TAB"))   return KEY_TAB;
    if (!strcmp(tok, "ESCAPE") || !strcmp(tok, "ESC")) return KEY_ESCAPE;
    if (!strcmp(tok, "BACKSPACE")) return KEY_BACKSPACE;
    if (!strcmp(tok, "LEFT"))  return KEY_LEFT;
    if (!strcmp(tok, "RIGHT")) return KEY_RIGHT;
    if (!strcmp(tok, "UP"))    return KEY_UP;
    if (!strcmp(tok, "DOWN"))  return KEY_DOWN;
    if (!strcmp(tok, "COMMA")) return KEY_COMMA;
    if (!strcmp(tok, "PERIOD"))return KEY_PERIOD;
    return -1;
}

static void ev_push(int frame, int kind, int a, int b) {
    static int cap = 0;
    if (replay_n >= cap) {
        cap = cap ? cap * 2 : 256;
        replay_ev = realloc(replay_ev, (size_t)cap * sizeof(InputEvent));
    }
    replay_ev[replay_n++] = (InputEvent){ frame, kind, a, b };
}
static void ev_key(int frame, int kc, int down) { if (kc >= 0) ev_push(frame, 0, kc, down); }

// sort by frame, then by kind so a mouse-move (1) applies before a button (2) in
// the same frame — the cursor is in place when the click registers.
static int ev_cmp(const void *a, const void *b) {
    const InputEvent *x = a, *y = b;
    return x->frame != y->frame ? x->frame - y->frame : x->kind - y->kind;
}

// --replay: the recorder's tagged format, one event per line:
//   <frame> k <keycode> <down>     key
//   <frame> m <x> <y>              mouse move (canvas space)
//   <frame> b <button 0|1|2> <down>   mouse button (L/R/M)
//   <frame> w <delta> 0            mouse wheel (+up / -down), transient on that frame
static void load_replay(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "harness: cannot open replay %s\n", path); return; }
    char line[128];
    while (fgets(line, sizeof line, f)) {
        int frame, x, y; char tag;
        if (sscanf(line, "%d %c %d %d", &frame, &tag, &x, &y) != 4) continue;
        if      (tag == 'k') ev_push(frame, 0, x, y);
        else if (tag == 'm') ev_push(frame, 1, x, y);
        else if (tag == 'b') ev_push(frame, 2, x, y);
        else if (tag == 'w') ev_push(frame, 3, x, y);
    }
    fclose(f);
    qsort(replay_ev, replay_n, sizeof(InputEvent), ev_cmp);
}

// --script: a human-authored input plan. One directive per line (# = comment):
//   down/up <frame> <key>          key press / release
//   tap     <frame> <key> [dur]    press then release dur frames later (default 6)
//   move    <frame> <x> <y>        move the pointer to canvas (x,y)
//   click   <frame> <x> <y> [btn]  move there, then a left-click (btn 1=right,2=mid)
//   wheel   <frame> <delta>        scroll the mouse wheel <delta> ticks (+up / -down) on that frame
// <key> is a single char (a,s,k,l) or a name (SPACE, LEFT, ...).
static void load_script(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "harness: cannot open script %s\n", path); return; }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char cmd[32], key[32]; int frame, p = 6, q = 0;
        if (line[0] == '#' || line[0] == '\n') continue;
        { int amt; if (sscanf(line, "%31s %d %d", cmd, &frame, &amt) == 3 && !strcmp(cmd, "wheel")) { ev_push(frame, 3, amt, 0); continue; } }
        if (sscanf(line, "%31s %d %31s %d", cmd, &frame, key, &p) >= 3 &&
            (!strcmp(cmd,"down")||!strcmp(cmd,"up")||!strcmp(cmd,"tap"))) {
            int kc = key_code(key);
            if      (!strcmp(cmd, "down")) ev_key(frame, kc, 1);
            else if (!strcmp(cmd, "up"))   ev_key(frame, kc, 0);
            else                           { ev_key(frame, kc, 1); ev_key(frame + p, kc, 0); }
            continue;
        }
        if (sscanf(line, "%31s %d %d %d", cmd, &frame, &p, &q) >= 4) {
            int btn = 0; sscanf(line, "%*s %*d %*d %*d %d", &btn);   // optional 4th arg for click
            if      (!strcmp(cmd, "move"))  ev_push(frame, 1, p, q);
            else if (!strcmp(cmd, "click")) {
                ev_push(frame, 1, p, q);              // pointer to (x,y)
                ev_push(frame, 2, btn & 3, 1);        // button down
                ev_push(frame + 3, 2, btn & 3, 0);    // button up 3 frames later
            }
        }
    }
    fclose(f);
    qsort(replay_ev, replay_n, sizeof(InputEvent), ev_cmp);
}
#endif

// ------------------------------------------------------------
// per-cart save directory — where cart.sav / cart.kv / cart.blob live.
// Defaults to "." (the cwd) so a bare binary behaves like before; spawners
// (editor, play.js) pass --save-dir saves/<cart> so every cart gets its own
// folder instead of all sharing build/cart.sav — the same isolation idea as
// build/.bake/<name>, but for persistence.
// ------------------------------------------------------------

static char save_dir[512] = ".";

// join save_dir + file into a static buffer ("cart.kv" → "saves/foo/cart.kv")
static const char *save_path(const char *file) {
    static char buf[600];
    snprintf(buf, sizeof buf, "%s/%s", save_dir, file);
    return buf;
}

#ifndef PLATFORM_WEB
static void save_dir_set(const char *dir) {
    snprintf(save_dir, sizeof save_dir, "%s", dir);
    // mkdir -p: create each path level (mkdir is silent if it already exists)
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", save_dir);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') { *p = '\0'; de_mkdir(tmp); *p = '/'; }
    de_mkdir(tmp);
}
#endif

// Engine-owned PNG decode — the single entry point for turning an embedded .png
// blob into an Image (platform seam, phase B/C). Both backends decode with the
// SAME stb_image code (Raylib uses it internally), so the result is bit-identical.
#ifdef DE_NO_RAYLIB
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG               // the engine only embeds PNG blobs
#include "stb_image.h"
static Image de_image_decode(const unsigned char *png, int len) {
    int w = 0, h = 0, ch = 0;
    stbi_uc *px = stbi_load_from_memory(png, len, &w, &h, &ch, 4);   // force RGBA8888
    Image img = { px, w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    return img;
}
#else
static Image de_image_decode(const unsigned char *png, int len) {
    return LoadImageFromMemory(".png", png, len);
}
#endif

#ifdef DE_NO_RAYLIB
// ============================================================================
// DE_NO_RAYLIB entry points (platform.h) — the host (iOS CADisplayLink, a headless
// harness, Switch) drives the engine through these instead of studio.c owning main().
// ============================================================================
#include "platform.h"
#include "fonts_baked.h"
extern double de_host_time;   // the host clock (raylib_compat.c); GetTime() returns it

// bind a baked ROM font (fonts_baked.h) into the existing Font/Image globals, so
// cur_font()/cur_font_img()/sw_print work unchanged — no GPU, no LoadFontFromImage.
static void de_bind_font(Font *fnt, Image *img, const DeBakedFont *b) {
    fnt->baseSize = b->baseSize; fnt->glyphCount = b->glyphCount;
    fnt->recs   = (Rectangle*)malloc(sizeof(Rectangle) * b->glyphCount);
    fnt->glyphs = (GlyphInfo*)malloc(sizeof(GlyphInfo) * b->glyphCount);
    for (int i = 0; i < b->glyphCount; i++) {
        fnt->recs[i] = (Rectangle){ b->glyphs[i].x, b->glyphs[i].y, b->glyphs[i].w, b->glyphs[i].h };
        fnt->glyphs[i].value    = b->glyphs[i].value;
        fnt->glyphs[i].offsetX  = b->glyphs[i].offX;
        fnt->glyphs[i].offsetY  = b->glyphs[i].offY;
        fnt->glyphs[i].advanceX = b->glyphs[i].advX;
    }
    *img = (Image){ (void*)b->atlas, b->atlasW, b->atlasH, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
}
static void de_setup_baked_fonts(void) {
    de_bind_font(&game_font,  &game_font_img,  &DE_BAKED_FONTS[DE_FONT_GAME]);
    de_bind_font(&font_small, &font_small_img, &DE_BAKED_FONTS[DE_FONT_SMALL]);
    de_bind_font(&font_tiny,  &font_tiny_img,  &DE_BAKED_FONTS[DE_FONT_TINY]);
    de_bind_font(&font_comic, &font_comic_img, &DE_BAKED_FONTS[DE_FONT_COMIC]);
    de_bind_font(&font_thin,  &font_thin_img,  &DE_BAKED_FONTS[DE_FONT_THIN]);
}

void de_init(DeRenderer renderer) {
    (void)renderer;                              // software canvas only for now
    sw_canvas_enabled = sw_canvas_active = true;
    de_ensure_fb(SCREEN_W, SCREEN_H);            // heap framebuffer(s), grows if a resizable cart enlarges
    load_palette();
    init_touch_layout();
    if (MAP_DATA_LEN >= sizeof(map_data)) memcpy(map_data, MAP_DATA, sizeof map_data);
    else                                  memset(map_data, 0, sizeof map_data);
    de_setup_baked_fonts();
#ifdef SPRITES_MULTI
    for (int i = 0; i < SPRITES_MULTI; i++) {   // per-cart sheets on the SW path (iOS): pre-load all
        if (SPRITES_SHEET_LENS[i] == 0) continue;
        Image s = de_image_decode(SPRITES_SHEETS[i], SPRITES_SHEET_LENS[i]);
        if (s.width > 0) sheet_imgs[i] = s;     // sw_blit/img_texel read this CPU image (no GPU texture)
    }
    de_sheet_select(0);                         // boot cart is ctx 0
#else
    if (SPRITES_DATA_LEN > 0) {                 // decode the embedded sheet (stb_image) for the SW sprite path
        Image s = de_image_decode(SPRITES_DATA, SPRITES_DATA_LEN);
        if (s.width > 0) {
            spritesheet_img = s;                // sw_blit/img_texel read this CPU image (no GPU texture)
            spritesheet.width = s.width;        // the sprite/map guards check spritesheet.width and derive
            spritesheet.height = s.height;      // cols = spritesheet.width/SPRITE_SIZE — set dims, id stays 0
        }
    }
#endif
    sound_synth_mode = true; sound_init();
    init();                                      // the cart's init()
    last_time = GetTime();
}
void de_input_endframe(void);   // raylib_compat.c — snapshots mouse button state for next frame's edge detect
void de_frame(double t) { de_host_time = t; loop_step(); if (sw_rot_active) sw_rot_composite(); de_input_endframe(); }   // loop_step draws (sw_cbuf or the rotated world buffer) + advances the sequencer; composite any rotated world a cart didn't close with camera()
// pulled by the host audio backend (CoreAudio on iOS) — fills `frames` interleaved
// stereo floats in [-1,1] @ SOUND_SAMPLE_RATE. The same mixer the Raylib AudioStream
// drives; reentrant/lock-free, safe from the audio thread while de_frame runs on main.
void de_audio_render(float *out, int frames) { sound_callback((void *)out, (unsigned)frames); }
const uint32_t *de_framebuffer(void) { return (const uint32_t*)sw_cbuf; }
int de_screen_w(void) { return de_sw; }   // active canvas dims (Phase 1a: == SCREEN_W until 1b)
int de_screen_h(void) { return de_sh; }

#else  // !DE_NO_RAYLIB — the Raylib desktop/web build owns main()

#ifdef DE_NET_BUILD
// ── netplay boot lobby (rung 2.5 — docs/design/multiplayer-research.md) ───────
// An in-window Host / Join / Solo menu so a STANDALONE build (no editor, just a
// double-clicked exe) can start multiplayer with no CLI flags. Shown when
// --net-lobby is passed, or when DE_NET_LOBBY_DEFAULT is compiled in (the
// exported "send to a friend" build). Runs after fonts load but BEFORE the
// cart's init(), because the host's rnd() seed must be exchanged before any
// cart code rolls it. Draws straight to the window like the pause overlay.
static void net_lobby_center(const char *s, float cy, float px, DeColor col) {
    Vector2 m = MeasureTextEx(game_font, s, px, 0);
    DrawTextEx(game_font, s, (Vector2){ GetScreenWidth() / 2.0f - m.x / 2.0f, cy }, px, 0, col);
}

// One status frame drawn right before the (blocking) handshake, so the window
// shows what it's doing instead of freezing on black while it connects.
static void net_lobby_status_frame(void) {
    char line[96];
    if (net_is_host) {
        char ip[INET_ADDRSTRLEN]; net_local_ipv4(ip, sizeof ip);
        snprintf(line, sizeof line, "HOSTING at %s", ip);
    } else {
        snprintf(line, sizeof line, "connecting to %s ...", net_join_ip ? net_join_ip : "?");
    }
    float px = GetScreenHeight() / 22.0f, cy = GetScreenHeight() / 2.0f - px;
    BeginDrawing();
    ClearBackground(palette[1]);
    net_lobby_center(line, cy, px, palette[7]);
    if (net_is_host) net_lobby_center("tell the other player to Join this address", cy + px * 2, px * 0.6f, palette[13]);
    EndDrawing();
}

// The menu loop. Sets net_is_host / net_join_ip / net_requested on a host/join
// pick, or returns with net still off for solo. Blocks until the user chooses.
static void net_lobby_menu(const char *title) {
    int  mode = 0;                        // 0 = choosing, 1 = joining (auto-discover + type)
    char ip[INET_ADDRSTRLEN]    = { 0 };  // what the player typed
    char found[INET_ADDRSTRLEN] = { 0 };  // a host auto-discovered on the LAN (rung 3)
    int  n = 0;
    while (!WindowShouldClose()) {
        if (mode == 0) {
            if (IsKeyPressed(KEY_H)) { net_is_host = true; net_requested = true; return; }
            if (IsKeyPressed(KEY_J)) { mode = 1; found[0] = 0; net_discover_begin(); }  // start listening for LAN games
            if (IsKeyPressed(KEY_S)) return;                        // play solo — net stays off
        } else {
            net_discover_poll(found, sizeof found);                 // fill `found` if a host is shouting on the LAN
            int ch;
            while ((ch = GetCharPressed()) != 0)                    // digits + dots only
                if (n < (int)sizeof(ip) - 1 && ((ch >= '0' && ch <= '9') || ch == '.')) { ip[n++] = (char)ch; ip[n] = 0; }
            if (IsKeyPressed(KEY_BACKSPACE) && n > 0) ip[--n] = 0;
            if (IsKeyPressed(KEY_ESCAPE)) { mode = 0; n = 0; ip[0] = 0; net_discover_end(); }
            // ENTER joins: the typed IP if you typed one, else the auto-discovered host
            if (IsKeyPressed(KEY_ENTER) && (n > 0 || found[0])) {
                net_join_ip = strdup(n > 0 ? ip : found);
                net_is_host = false; net_requested = true; net_discover_end(); return;
            }
        }
        float px = GetScreenHeight() / 22.0f, cy = GetScreenHeight() / 2.0f - px * 4;  // screen-relative so it reads on any cart
        BeginDrawing();
        ClearBackground(palette[1]);
        net_lobby_center(title, cy - px * 2, px * 0.75f, palette[6]);
        if (mode == 0) {
            net_lobby_center("M U L T I P L A Y E R", cy, px, palette[7]);
            net_lobby_center("[H]   host a game", cy + px * 2.5f, px, palette[7]);
            net_lobby_center("[J]   join a game", cy + px * 4.0f, px, palette[7]);
            net_lobby_center("[S]   play solo",   cy + px * 5.5f, px, palette[13]);
        } else {
            net_lobby_center("JOIN A GAME", cy, px, palette[7]);
            if (found[0] && n == 0) {                               // auto-discovered a LAN host
                char l[96]; snprintf(l, sizeof l, "found a game at %s", found);
                net_lobby_center(l, cy + px * 2.3f, px * 0.85f, palette[11]);   // green
                net_lobby_center("ENTER to join   (or type an IP)", cy + px * 4.2f, px * 0.7f, palette[13]);
            } else {
                char l[96]; snprintf(l, sizeof l, "%s_", ip);
                net_lobby_center(n > 0 ? "host IP:" : "searching for a game — or type the host's IP:", cy + px * 2.3f, px * 0.7f, palette[13]);
                net_lobby_center(l, cy + px * 3.6f, px, palette[7]);
                net_lobby_center("ENTER to connect    ESC to go back", cy + px * 5.5f, px * 0.7f, palette[13]);
            }
        }
        EndDrawing();
        // live-inspection: same .bake/window_screenshot_request hook the game loop
        // honors, so the lobby is capturable headlessly (dev preview + this rung's gate)
        FILE *sf = fopen(".bake/window_screenshot_request", "r");
        if (sf) {
            char out[512] = { 0 };
            if (fgets(out, sizeof out, sf)) out[strcspn(out, "\n\r")] = '\0';
            fclose(sf); remove(".bake/window_screenshot_request");
            if (out[0]) { Image shot = LoadImageFromScreen(); ExportImage(shot, out); UnloadImage(shot); }
        }
    }
    exit(0);   // window closed during the menu
}
#endif // DE_NET_BUILD

int main(int argc, char **argv) {
    { const char *pf = getenv("DE_POLY_FILL");          // A/B the polygon fill without recompiling:
      if (pf && strcmp(pf, "legacy") == 0) poly_fill_fast = false; }   // DE_POLY_FILL=legacy → old per-pixel path
    { const char *df = getenv("DE_DISC_FILL");          // A/B the circle/oval fill:
      if (df && strcmp(df, "legacy") == 0) disc_fill_fast = false; }   // DE_DISC_FILL=legacy → old per-pixel path
    { const char *cc = getenv("DE_CLAMP_CACHE");        // A/B the per-frame clamp-box cache:
      if (cc && strcmp(cc, "off") == 0) clamp_cache_on = false; }      // DE_CLAMP_CACHE=off → recompute every call
    { const char *bp = getenv("DE_BATCH_PSET");         // A/B the batched-pset path:
      if (bp && strcmp(bp, "on") == 0) pset_batch = true; }            // DE_BATCH_PSET=on → coalesce psets into one draw call
    { const char *bf = getenv("DE_BLIT_FAST");          // A/B the software-canvas sprite-blit fast path:
      if (bf && strcmp(bf, "off") == 0) blit_fast_on = false; }        // DE_BLIT_FAST=off → legacy per-pixel sw_blit
    { const char *tf = getenv("DE_TRITEX_FAST");        // A/B the software textured-triangle fast path:
      if (tf && strcmp(tf, "off") == 0) tritex_fast_on = false; }      // DE_TRITEX_FAST=off → legacy unclamped sw_tritex
    { const char *sc = getenv("DE_SOFTWARE_CANVAS");    // A/B the software canvas (Phase 0 probe):
      if (sc && strcmp(sc, "on") == 0) { sw_canvas_enabled = true; sw_canvas_active = true; } }  // DE_SOFTWARE_CANVAS=on
    { const char *cl = getenv("DE_CPU_RASTER");         // CPU rasterizers off-canvas too (A/B hygiene, see decl):
      if (cl && strcmp(cl, "on") == 0) cpu_raster_enabled = true; }   // DE_CPU_RASTER=on → line()/rectfill_rot → CPU everywhere
    { const char *ao = getenv("DE_AUDIO");              // DE_AUDIO=off → skip all audio (see decl):
      if (ao && strcmp(ao, "off") == 0) audio_off = true; }
    { const char *ss = getenv("DE_SHOW_SIZE");          // DE_SHOW_SIZE=1 → live WxH overlay (resizable carts)
      if (ss && ss[0] && strcmp(ss, "0") != 0) size_overlay_on = true; }
#ifndef DE_WINDOW_TITLE            // exports bake the cart name in (a double-clicked app gets no argv)
#define DE_WINDOW_TITLE "dreamengine"
#endif
    const char *window_title           = DE_WINDOW_TITLE;
#ifndef PLATFORM_WEB
    int         screenshot_mode        = 0;
    int         screenshot_frames_done = 0;
    int         hide_window            = 0;
    unsigned    seed                   = 1;
    const char *rec_path = NULL, *replay_path = NULL, *script_path = NULL, *trace_path = NULL;
    const char *wav_path = NULL;
#ifdef DE_TCC
    const char *cart_path = "cart.c";   // libtcc-loaded cart source (--cart <path>)
#endif
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--screenshot") == 0) screenshot_mode = 1;
        else if (strcmp(argv[i], "--title")  == 0 && i + 1 < argc) window_title = argv[++i];
        else if (strcmp(argv[i], "--det")    == 0) det_mode = true;
        else if (strcmp(argv[i], "--headless") == 0) hide_window = 1;
        else if (strcmp(argv[i], "--seed")   == 0 && i + 1 < argc) seed = (unsigned)atoi(argv[++i]);
        else if (strcmp(argv[i], "--record") == 0 && i + 1 < argc) rec_path = argv[++i];
        else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) replay_path = argv[++i];
        else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) script_path = argv[++i];
        else if (strcmp(argv[i], "--trace")  == 0 && i + 1 < argc) trace_path = argv[++i];
        else if (strcmp(argv[i], "--uiaudit") == 0 && i + 1 < argc) uiaudit_path = argv[++i];
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) max_frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--dump")   == 0 && i + 1 < argc) { snprintf(dump_dir, sizeof dump_dir, "%s", argv[++i]); if (dump_every <= 0) dump_every = 1; }
        else if (strcmp(argv[i], "--dump-every") == 0 && i + 1 < argc) dump_every = atoi(argv[++i]);
        else if (strcmp(argv[i], "--resize") == 0 && i + 1 < argc) {   // "WxH,WxH,…" canvas-size sweep
            const char *p = argv[++i];
            while (resize_n < 16 && p && *p) {
                int w, h;
                if (sscanf(p, "%dx%d", &w, &h) == 2) { resize_w[resize_n] = w; resize_h[resize_n] = h; resize_n++; }
                const char *c = strchr(p, ','); p = c ? c + 1 : NULL;
            }
            if (resize_n > 0 && max_frames <= 0) max_frames = resize_n * RESIZE_HOLD;   // stop when the sweep ends
        }
        else if (strcmp(argv[i], "--save-dir") == 0 && i + 1 < argc) save_dir_set(argv[++i]);
        else if (strcmp(argv[i], "--wav")    == 0 && i + 1 < argc) wav_path = argv[++i];
        else if (strcmp(argv[i], "--data")   == 0 && i + 1 < argc) de_data_path_v = argv[++i];  // EXPERIMENTAL (see de_data_path_v)
#ifdef DE_NET_BUILD
        else if (strcmp(argv[i], "--net-host") == 0) { net_is_host = true; net_requested = true; }
        else if (strcmp(argv[i], "--net-join") == 0 && i + 1 < argc) { net_join_ip = argv[++i]; net_requested = true; }
        else if (strcmp(argv[i], "--net-port") == 0 && i + 1 < argc) net_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--net-lobby") == 0) net_lobby_requested = true;  // show the in-window Host/Join/Solo menu
#endif
#ifdef DE_SPEC
        else if (strcmp(argv[i], "--spec")   == 0) spec_mode = 1;
#endif
#ifdef DE_TCC
        else if (strcmp(argv[i], "--cart") == 0 && i + 1 < argc) cart_path = argv[++i];
#endif
    }
    // replay/script drive input deterministically; both imply --det
    if (replay_path) { load_replay(replay_path); inject_input = true; det_mode = true; }
    if (script_path) { load_script(script_path); inject_input = true; det_mode = true; }
#ifdef DE_NET_BUILD
#ifdef DE_NET_LOBBY_DEFAULT
    if (!net_requested) net_lobby_requested = true;   // exported standalone build: boot straight into the menu
#endif
    // Direct --net-host/--net-join (editor 🌐 button, CLI, netdemo): handshake
    // before the window opens, exactly as rung 1/2 shipped. The lobby path
    // (--net-lobby) instead handshakes AFTER the window is up — see below.
    if (net_requested && !net_lobby_requested) {
        det_mode = true;        // lockstep = fixed timestep + a shared rnd() seed
        net_handshake(&seed);   // blocks until both sides connect; joiner adopts the host's seed
        { static char nt[256];  // tag whichever title we have, so the two windows are tellable apart
          snprintf(nt, sizeof nt, "%s — %s", window_title, net_is_host ? "P1 (host)" : "P2");
          window_title = nt; }
    }
#endif
#ifdef DE_SPEC
    if (spec_mode) { inject_input = true; det_mode = true; hide_window = 1; }   // headless, deterministic
#endif
    if (rec_path)    rec_file   = fopen(rec_path,  "w");
    if (trace_path)  trace_file = fopen(trace_path, "w");
    if (uiaudit_path) uiaudit_file = fopen(uiaudit_path, "w");
    if (screenshot_mode) hide_window = 1;
    signal(SIGSEGV, crash_handler);   // bad/null pointer
    signal(SIGFPE,  crash_handler);   // divide by zero, etc.
    signal(SIGABRT, crash_handler);   // abort()/assert
#ifdef SIGBUS
    signal(SIGBUS,  crash_handler);   // misaligned / bad memory access (no SIGBUS on Windows)
#endif
    restart_argv = argv;
    de_mkdir(".bake");                // ensure inspect request dir exists (silent if already there)
#endif
#ifdef PLATFORM_WEB
    SetTraceLogLevel(LOG_ERROR);     // suppress NPOT/extension warnings — harmless on web
#else
    SetTraceLogLevel(LOG_WARNING);   // keep raylib's INFO chatter out of the runtime log panel
#endif
#ifndef PLATFORM_WEB
    if (hide_window) SetWindowState(FLAG_WINDOW_HIDDEN);
#endif
    // window defaults to the game's exact size — desktop is unchanged (no letterbox, identity
    // placement). DE_WINDOW=WxH (opt-in, dev/preview only) forces a phone-shaped window so the
    // deck/rails placement can be built + eyeballed on desktop without a device. On iOS/web the
    // shell supplies the real device size; game_rect (set each frame from gr_place) maps the
    // canvas into whatever size the window is, so any size letterboxes correctly.
    int win_w = SCREEN_W * SCALE, win_h = SCREEN_H * SCALE;
#ifndef PLATFORM_WEB
    { const char *ws = getenv("DE_WINDOW"); int w, h;
      if (ws && sscanf(ws, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) { win_w = w; win_h = h; } }
    // DE_RESIZABLE=1 (opt-in, dev/preview only) lets you drag the window live to explore
    // deck/rails/overlay placement without relaunching for every DE_WINDOW size — gr_place()
    // already recomputes from GetScreenWidth/Height every frame, so resizing just works.
    { const char *rs = getenv("DE_RESIZABLE");
      if (rs && rs[0] && strcmp(rs, "0") != 0) SetConfigFlags(FLAG_WINDOW_RESIZABLE); }
    // -DDE_RESIZABLE cart opt-in: resizable window + live reflow (de_reflow). The env above only
    // resizes the window (dev-preview window-fill); the compile flag additionally reflows the canvas.
    if (de_reflow) SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
    InitWindow(win_w, win_h, window_title);
#if !defined(PLATFORM_WEB) && !defined(DE_NO_RAYLIB)
    // seed the active dims from the initial window (DE_WINDOW may have overridden it); a resizable
    // cart launched at a non-default size reflows from frame 0, not only after the first drag.
    if (de_reflow) {
        de_sw = clampi(GetScreenWidth()  / SCALE, 1, DE_MAX_DIM);
        de_sh = clampi(GetScreenHeight() / SCALE, 1, DE_MAX_DIM);
    }
#endif
#ifndef PLATFORM_WEB
    if (det_mode) { SetRandomSeed(seed); srand(seed); }   // reproducible rnd()/rnd_float()/shake
#endif
#ifdef DE_NET_BUILD
    // netplay QoL: both windows land on one screen side by side — host left, joiner right
    if (net_active && !hide_window) SetWindowPosition(net_is_host ? 60 : 80 + win_w, 120);
#endif
#ifndef PLATFORM_WEB
    if (wav_path) { sound_synth_mode = true; wav_stream_open(wav_path); audio_off = false; }  // --wav needs the synth
    if (!audio_off) {                                   // DE_AUDIO=off skips audio entirely
        InitAudioDevice();
        sound_init();
        midi_input_init();   // CoreMIDI keyboard input (no-op if no device / non-macOS)
    }
#endif
    SetTargetFPS(60);

    de_ensure_fb(de_sw, de_sh);   // heap framebuffer sized to the (possibly window-seeded) canvas; == SCREEN_W/H for a fixed cart
    load_palette();
    pal_shader_init();   // pal()-on-sprites swap shader (needs the GL context from InitWindow)
    scale_shader_init(); // sharp-bilinear present filter (modes 2/3; no-op otherwise)
    init_touch_layout();

    if (MAP_DATA_LEN >= sizeof(map_data)) {
        memcpy(map_data, MAP_DATA, sizeof(map_data));
    } else {
        memset(map_data, 0, sizeof(map_data));
    }

    // low-res canvas — all drawing goes here, then scaled up. Sized to fb_w×fb_h (== SCREEN_W/H for a
    // fixed cart; the window-seeded size for a resizable one). Grows later via de_grow_gpu on resize.
    canvas = LoadRenderTexture(fb_w, fb_h);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
    // One-time clear: LoadRenderTexture leaves the texture as uninitialised GPU memory.
    // A cart that never cls()es and doesn't paint every pixel would show that garbage on
    // the GPU path — while the software canvas (zero-init sw_cbuf) shows black there. Clear
    // once to palette[0] so both backends start from the same black, without per-frame
    // clearing (which would kill intentional no-cls trail/feedback effects).
    BeginTextureMode(canvas); ClearBackground(palette[0]); EndTextureMode();
#if SCALE_FILTER == 1
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);  // mode 1: smooth present
#endif
    canvas_snap = LoadRenderTexture(fb_w, fb_h);
    SetTextureFilter(canvas_snap.texture, TEXTURE_FILTER_POINT);

    {
        Image fontImage = de_image_decode(DOS_8X8_FONT, DOS_8X8_FONT_LEN);
        game_font = LoadFontFromImage(fontImage, (DeColor){ 255, 255, 0, 255 }, 0);
        SetTextureFilter(game_font.texture, TEXTURE_FILTER_POINT);
        UnloadImage(fontImage);
        custom_font = true;
    }
    {
        Image img = de_image_decode(FONT4X6_DATA, FONT4X6_DATA_LEN);
        font_small = LoadFontFromImage(img, (DeColor){ 255, 255, 0, 255 }, 32);
        SetTextureFilter(font_small.texture, TEXTURE_FILTER_POINT);
        UnloadImage(img);
    }
    {
        Image img = de_image_decode(FONT3X5_DATA, FONT3X5_DATA_LEN);
        font_tiny = LoadFontFromImage(img, (DeColor){ 255, 255, 0, 255 }, 32);
        SetTextureFilter(font_tiny.texture, TEXTURE_FILTER_POINT);
        UnloadImage(img);
    }
    {
        Image img = de_image_decode(FONTCOMIC10X20_DATA, FONTCOMIC10X20_DATA_LEN);
        font_comic = LoadFontFromImage(img, (DeColor){ 255, 255, 0, 255 }, 0);   // Comic Mono Bold @ 18px, 10×20 cells
        SetTextureFilter(font_comic.texture, TEXTURE_FILTER_POINT);
        UnloadImage(img);
    }
    {
        Image img = de_image_decode(FONTTHIN8X8_DATA, FONTTHIN8X8_DATA_LEN);
        font_thin = LoadFontFromImage(img, (DeColor){ 255, 255, 0, 255 }, 0);    // IBM CGA "thin" 8×8, narrow-stroke alternate
        SetTextureFilter(font_thin.texture, TEXTURE_FILTER_POINT);
        UnloadImage(img);
    }

    // CPU copies of the font atlases for the software-canvas glyph blit (sw_print). RGBA so
    // GetImageColor is a plain read; alpha is the glyph mask (LoadFontFromImage keyed the bg out).
    game_font_img  = LoadImageFromTexture(game_font.texture);  ImageFormat(&game_font_img,  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    font_small_img = LoadImageFromTexture(font_small.texture); ImageFormat(&font_small_img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    font_tiny_img  = LoadImageFromTexture(font_tiny.texture);  ImageFormat(&font_tiny_img,  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    font_comic_img = LoadImageFromTexture(font_comic.texture); ImageFormat(&font_comic_img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    font_thin_img  = LoadImageFromTexture(font_thin.texture);  ImageFormat(&font_thin_img,  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

#ifdef SPRITES_MULTI
    for (int i = 0; i < SPRITES_MULTI; i++) {          // per-cart sheets: pre-load all, boot cart is ctx 0
        if (SPRITES_SHEET_LENS[i] == 0) continue;
        Image img = de_image_decode(SPRITES_SHEETS[i], SPRITES_SHEET_LENS[i]);
        if (img.width > 0) {
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            sheet_imgs[i] = img;
            sheet_texs[i] = LoadTextureFromImage(img);
            SetTextureFilter(sheet_texs[i], TEXTURE_FILTER_POINT);
        }
    }
    de_sheet_select(0);
#else
    if (SPRITES_DATA_LEN > 0) {
        Image img = de_image_decode(SPRITES_DATA, SPRITES_DATA_LEN);
        if (img.width > 0) {
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);   // so img_texel()'s fast path always applies
            spritesheet_img = img;
            spritesheet = LoadTextureFromImage(img);
            SetTextureFilter(spritesheet, TEXTURE_FILTER_POINT);
        }
    }
#endif

#ifdef DE_NET_BUILD
    // Netplay lobby path (--net-lobby / exported build): the window + fonts are
    // up, so show the Host/Join/Solo menu and, on a pick, handshake here —
    // BEFORE init() below, so the host's seed reaches the joiner before any
    // rnd(). The direct-flag path already handshook before the window (above),
    // so `net_active` is set there and this block is skipped for it.
    if (net_lobby_requested && !net_requested && de_players() >= 2) net_lobby_menu(window_title);
    if (net_requested && !net_active) {
        det_mode = true;
        net_lobby_status_frame();   // draw "HOSTING…/connecting…" so the wait isn't a black window
        net_handshake(&seed);       // blocking; joiner adopts the host's seed
        SetRandomSeed(seed); srand(seed);
        { static char nt[256];      // window is already up here, so push the tag to the OS too
          snprintf(nt, sizeof nt, "%s — %s", window_title, net_is_host ? "P1 (host)" : "P2");
          window_title = nt;
          SetWindowTitle(window_title); }
    }
#endif

#ifndef PLATFORM_WEB
#ifdef DE_TCC
    if (cart_load(cart_path) != 0) {
        fprintf(stderr, "[tcc] could not load cart '%s' — aborting\n", cart_path);
        return 1;
    }
    if (cart_init) cart_init();
#else
    init();
#endif
#endif

    last_time = GetTime();   // seed dt()

#ifdef PLATFORM_WEB
    emscripten_set_main_loop(loop_step, 0, 1);
#else
#ifdef DE_SPEC
    if (spec_mode) {                              // run the cart's spec() headless, then report + exit
        spec_inited = 1;                          // init() already ran above; don't re-init in step()
        frame_count = 0;
        spec();
        printf("{\"done\":1,\"pass\":%d,\"fail\":%d}\n", spec_pass, spec_fail);
        fflush(stdout);
        return spec_fail > 0 ? 1 : 0;
    }
#endif
    while (!WindowShouldClose()) {
        loop_step();
        if (screenshot_mode && ++screenshot_frames_done >= 3) break;
        if (max_frames > 0 && frame_count >= max_frames) break;   // bounded harness run
    }
#ifdef DE_NET_BUILD
    net_shutdown();   // tell the peer we're gone — they exit cleanly instead of timing out
#endif

    // save last frame as screenshot.png so the cart PNG thumbnail shows the game
    Image screenshot = LoadImageFromTexture(canvas.texture);
    ImageFlipVertical(&screenshot);
    ExportImage(screenshot, "screenshot.png");
    UnloadImage(screenshot);

    if (rec_file)   { fclose(rec_file);   rec_file   = NULL; }
    if (trace_file) { fclose(trace_file); trace_file = NULL; }
    if (uiaudit_file) { fclose(uiaudit_file); uiaudit_file = NULL; }
    free(replay_ev); replay_ev = NULL;

    if (custom_font) UnloadFont(game_font);
    if (font_small.texture.id > 0) UnloadFont(font_small);
    if (font_tiny.texture.id  > 0) UnloadFont(font_tiny);
    if (font_comic.texture.id > 0) UnloadFont(font_comic);
    if (font_thin.texture.id > 0) UnloadFont(font_thin);
    if (pal_shader_ok) UnloadShader(pal_shader);
    if (scale_shader_ok) UnloadShader(scale_shader);
    if (smooth_rt_ok) UnloadRenderTexture(smooth_rt);
    if (spritesheet.width > 0) UnloadTexture(spritesheet);
    if (spritesheet_img.data) UnloadImage(spritesheet_img);
    if (pget_snapshot.data) UnloadImage(pget_snapshot);
    UnloadRenderTexture(canvas);
    wav_stream_close();    // --wav: patch RIFF sizes and finish the file
    if (!audio_off) {                                   // mirror the gated init
        midi_input_shutdown();
        sound_shutdown();
        CloseAudioDevice();
    }
    CloseWindow();
#endif
    return 0;
}
#endif // !DE_NO_RAYLIB — main() (Raylib build only)

// ------------------------------------------------------------
// api implementation
// ------------------------------------------------------------

// Default key bindings for btn(). Each is overridable at compile time via -D
// (the editor's settings → controls panel passes the saved layout as raylib
// keycodes, e.g. -DP0_BTN_A=90). When no flag is passed these defaults apply, so
// the runtime works standalone and old build commands keep compiling.
//   Player 0: Arrows + Z/X/C/V      Player 1: WASD + J/K/L/;
#ifndef P0_BTN_UP
#define P0_BTN_UP    KEY_UP
#endif
#ifndef P0_BTN_DOWN
#define P0_BTN_DOWN  KEY_DOWN
#endif
#ifndef P0_BTN_LEFT
#define P0_BTN_LEFT  KEY_LEFT
#endif
#ifndef P0_BTN_RIGHT
#define P0_BTN_RIGHT KEY_RIGHT
#endif
#ifndef P0_BTN_A
#define P0_BTN_A     KEY_Z
#endif
#ifndef P0_BTN_B
#define P0_BTN_B     KEY_X
#endif
#ifndef P0_BTN_X
#define P0_BTN_X     KEY_C
#endif
#ifndef P0_BTN_Y
#define P0_BTN_Y     KEY_V
#endif
#ifndef P1_BTN_UP
#define P1_BTN_UP    KEY_W
#endif
#ifndef P1_BTN_DOWN
#define P1_BTN_DOWN  KEY_S
#endif
#ifndef P1_BTN_LEFT
#define P1_BTN_LEFT  KEY_A
#endif
#ifndef P1_BTN_RIGHT
#define P1_BTN_RIGHT KEY_D
#endif
#ifndef P1_BTN_A
#define P1_BTN_A     KEY_J
#endif
#ifndef P1_BTN_B
#define P1_BTN_B     KEY_K
#endif
#ifndef P1_BTN_X
#define P1_BTN_X     KEY_L
#endif
#ifndef P1_BTN_Y
#define P1_BTN_Y     KEY_SEMICOLON
#endif
#ifndef PAUSE_KEY
#define PAUSE_KEY    KEY_P
#endif

// action-button index for BTN_A/B/X/Y — 0..3 into btn_cx[]/btn_cy[], or -1 for a non-action button.
static int action_btn_index(int button) {
    switch (button) {
        case BTN_A: return 0;
        case BTN_B: return 1;
        case BTN_X: return 2;
        case BTN_Y: return 3;
        default:    return -1;
    }
}

// btn()'s hardware read — THIS machine's keymaps + touch overlay. Under netplay
// this becomes "sample my local input" (net.h ORs both players' keymaps into MY
// packed byte); the public btn() below answers from the lockstep bytes instead.
static bool btn_local(int player, int button) {
    if (player == 0) {
        if (show_touch_ui) {
            bool dpad = (touch_move_mode == TOUCH_DPAD4 || touch_move_mode == TOUCH_DPAD8);
            float sx = dpad ? 0 : stick_x(), sy = dpad ? 0 : stick_y();
            switch (button) {
                case BTN_UP:    if (dpad ? dpad_up    : (sy < -STICK_DEADZONE)) return true; break;
                case BTN_DOWN:  if (dpad ? dpad_down  : (sy >  STICK_DEADZONE)) return true; break;
                case BTN_LEFT:  if (dpad ? dpad_left  : (sx < -STICK_DEADZONE)) return true; break;
                case BTN_RIGHT: if (dpad ? dpad_right : (sx >  STICK_DEADZONE)) return true; break;
                default: {
                    int i = action_btn_index(button);
                    if (i >= 0 && i < touch_n_buttons && any_touch_in_circle(btn_cx[i], btn_cy[i], eff_btn_r()))
                        return true;
                    break;
                }
            }
        }
        switch (button) {
            case BTN_UP:    return inp_down(P0_BTN_UP);
            case BTN_DOWN:  return inp_down(P0_BTN_DOWN);
            case BTN_LEFT:  return inp_down(P0_BTN_LEFT);
            case BTN_RIGHT: return inp_down(P0_BTN_RIGHT);
            case BTN_A:     return inp_down(P0_BTN_A);
            case BTN_B:     return inp_down(P0_BTN_B);
            case BTN_X:     return inp_down(P0_BTN_X);
            case BTN_Y:     return inp_down(P0_BTN_Y);
        }
    } else if (player == 1) {
        switch (button) {
            case BTN_UP:    return inp_down(P1_BTN_UP);
            case BTN_DOWN:  return inp_down(P1_BTN_DOWN);
            case BTN_LEFT:  return inp_down(P1_BTN_LEFT);
            case BTN_RIGHT: return inp_down(P1_BTN_RIGHT);
            case BTN_A:     return inp_down(P1_BTN_A);
            case BTN_B:     return inp_down(P1_BTN_B);
            case BTN_X:     return inp_down(P1_BTN_X);
            case BTN_Y:     return inp_down(P1_BTN_Y);
        }
    }
    return false;
}

bool btn(int player, int button) {
#ifdef DE_NET_BUILD
    if (net_active) {   // lockstep: player 0/1 = host/joiner machine, local or remote
        if (player < 0 || player > 1 || button < 0 || button >= BTN_COUNT) return false;
        return (net_bits[player] >> button) & 1;
    }
#endif
    return btn_local(player, button);
}

bool btnp(int player, int button) {
    if (player < 0 || player > 1) return false;
    if (button < 0 || button >= BTN_COUNT) return false;
    return btn_curr[player][button] && !btn_prev[player][button];
}

bool btnr(int player, int button) {
    if (player < 0 || player > 1) return false;
    if (button < 0 || button >= BTN_COUNT) return false;
    return !btn_curr[player][button] && btn_prev[player][button];
}

// ------------------------------------------------------------
// touch + stick api
// ------------------------------------------------------------

int touch_count(void) { return vt_count; }

int touch_x(int i) {
    if (i < 0 || i >= vt_count) return -1;
    return gr_win_to_canvas_x(game_rect, vt_pos[i].x);
}
int touch_y(int i) {
    if (i < 0 || i >= vt_count) return -1;
    return gr_win_to_canvas_y(game_rect, vt_pos[i].y);
}
int touch_id(int i) {
    if (i < 0 || i >= vt_count) return -1;
    return vt_id[i];
}

bool tap(int x, int y, int w, int h) {
    for (int i = 0; i < vt_count; i++) {
        int cx = gr_win_to_canvas_x(game_rect, vt_pos[i].x), cy = gr_win_to_canvas_y(game_rect, vt_pos[i].y);
        if (cx >= x && cx < x + w && cy >= y && cy < y + h) return true;
    }
    return false;
}

bool tapp(int x, int y, int w, int h) {
    for (int i = 0; i < vt_count; i++) {
        if (vt_was_down(vt_id[i])) continue;   // finger was already down last frame
        int cx = gr_win_to_canvas_x(game_rect, vt_pos[i].x), cy = gr_win_to_canvas_y(game_rect, vt_pos[i].y);
        if (cx >= x && cx < x + w && cy >= y && cy < y + h) return true;
    }
    return false;
}

int touch_ended_count(void) { return vt_ended_count; }

int touch_ended_id(int i) {
    if (i < 0 || i >= vt_ended_count) return -1;
    return vt_ended_id[i];
}
int touch_ended_x(int i) {
    if (i < 0 || i >= vt_ended_count) return -1;
    return gr_win_to_canvas_x(game_rect, vt_ended_pos[i].x);
}
int touch_ended_y(int i) {
    if (i < 0 || i >= vt_ended_count) return -1;
    return gr_win_to_canvas_y(game_rect, vt_ended_pos[i].y);
}

bool tapr(int x, int y, int w, int h) {
    for (int i = 0; i < vt_ended_count; i++) {
        int cx = gr_win_to_canvas_x(game_rect, vt_ended_pos[i].x), cy = gr_win_to_canvas_y(game_rect, vt_ended_pos[i].y);
        if (cx >= x && cx < x + w && cy >= y && cy < y + h) return true;
    }
    return false;
}

void touch_controls(bool on) { show_touch_ui = on; }

void touch_layout(int move_mode, int n_buttons) {
    show_touch_ui   = true;                                   // declaring a layout opts the controls in
    touch_move_mode = (move_mode >= TOUCH_ANALOG && move_mode <= TOUCH_DPAD8) ? move_mode : TOUCH_ANALOG;
    touch_n_buttons = n_buttons < 0 ? 0 : (n_buttons > 4 ? 4 : n_buttons);
}

int   touch_layout_mode(void) { return place_mode; }   // PlaceMode and TOUCH_LAYOUT_* share 0/1/2 numbering
float touch_ctrl_scale(void)  { return ctrl_scale; }
void  touch_debug(bool on)    { touch_debug_on = on; }

#ifdef PLATFORM_WEB
// computed once at boot by web_shell.html (Module.deTouchCeiling) — see the
// detection traps in docs/design/mobile-web-notes.md §5 (iPad pretends to be
// a Mac; navigator.maxTouchPoints reports 5 on BOTH iPhone and iPad).
EM_JS(int, de_web_touch_ceiling, (void), {
    return (Module.deTouchCeiling | 0);
});
#endif

int touch_ceiling(void) {
#ifdef PLATFORM_WEB
    return de_web_touch_ceiling();
#else
    return 0;   // native desktop: no touch digitizer (mouse-as-touch is one contact anyway)
#endif
}

float stick_x(void) {
    if (stick_touch_id == -1) return 0.0f;   // -1 = no stick; the mouse-as-touch uses id -2 (still active)
    return (stick_knob_x - stick_base_x) / eff_stick_r();   // knob is clamped to eff_stick_r(), not the raw constant
}
float stick_y(void) {
    if (stick_touch_id == -1) return 0.0f;   // -1 = no stick; the mouse-as-touch uses id -2 (still active)
    return (stick_knob_y - stick_base_y) / eff_stick_r();
}

// ------------------------------------------------------------
// mouse api — canvas-space pointer, always available on desktop
// ------------------------------------------------------------
static int raylib_mouse_button(int button) {
    switch (button) {
        case MOUSE_RIGHT:  return MOUSE_BUTTON_RIGHT;
        case MOUSE_MIDDLE: return MOUSE_BUTTON_MIDDLE;
        default:           return MOUSE_BUTTON_LEFT;
    }
}
int mouse_x(void) { return inp_mouse_x(); }
int mouse_y(void) { return inp_mouse_y(); }
bool mouse_down(int button)     { return inp_mouse_down(button); }
bool mouse_pressed(int button)  { return inp_mouse_pressed(button); }
bool mouse_released(int button) { return inp_mouse_released(button); }
float mouse_wheel(void)         { return inp_mouse_wheel(); }
int mouse_world_x(void)         { return (int)GetScreenToWorld2D((Vector2){ (float)mouse_x(), (float)mouse_y() }, cam).x; }
int mouse_world_y(void)         { return (int)GetScreenToWorld2D((Vector2){ (float)mouse_x(), (float)mouse_y() }, cam).y; }
void mouse_cursor(int kind) {
    int rl;
    switch (kind) {
        case CURSOR_HAND:      rl = MOUSE_CURSOR_POINTING_HAND; break;
        case CURSOR_CROSSHAIR: rl = MOUSE_CURSOR_CROSSHAIR;     break;
        case CURSOR_MOVE:      rl = MOUSE_CURSOR_RESIZE_ALL;    break;
        case CURSOR_TEXT:      rl = MOUSE_CURSOR_IBEAM;         break;
        case CURSOR_NO:        rl = MOUSE_CURSOR_NOT_ALLOWED;   break;
        default:               rl = MOUSE_CURSOR_DEFAULT;       break;
    }
    SetMouseCursor(rl);
}
void mouse_hide(void) { HideCursor(); }
void mouse_show(void) { ShowCursor(); }

static int last_cls_color = 0;   // remembered so smooth_zoom's offscreen clears to the same bg
void cls(int color) {
    PROF("cls");
    last_cls_color = color % PALETTE_SIZE;
    if (sw_canvas_active) {
        uint32_t p = sw_pack(palette[last_cls_color]);
        for (int i = 0; i < fb_w * fb_h; i++) sw_dst[i] = p;
        return;
    }
    ClearBackground(palette[last_cls_color]);
}

// palette-swap fragment shader. The vertex stage is raylib's default (we pass NULL).
// For each texel: find the nearest base-palette color (sprites use exact palette
// RGBs, so "nearest" is really "exact"), then output that index's CURRENT palette
// color — which pal() may have remapped. Alpha is preserved so colorkey() holes
// stay transparent. The loop indexes the uniform arrays with the loop variable
// only (no computed index) so it also compiles under GLSL ES 100 on web.
#ifdef PLATFORM_WEB
static const char *PAL_FS =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 basePal[64];\n"
    "uniform vec3 curPal[64];\n"
    "void main() {\n"
    "    vec4 texel = texture2D(texture0, fragTexCoord);\n"
    "    float bestD = 1e20;\n"
    "    vec3 outc = curPal[0];\n"
    "    for (int i = 0; i < 64; i++) {\n"
    "        vec3 dd = texel.rgb - basePal[i];\n"
    "        float dist = dot(dd, dd);\n"
    "        if (dist < bestD) { bestD = dist; outc = curPal[i]; }\n"
    "    }\n"
    "    gl_FragColor = vec4(outc, texel.a) * colDiffuse * fragColor;\n"
    "}\n";
#else
static const char *PAL_FS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 basePal[64];\n"
    "uniform vec3 curPal[64];\n"
    "void main() {\n"
    "    vec4 texel = texture(texture0, fragTexCoord);\n"
    "    float bestD = 1e20;\n"
    "    vec3 outc = curPal[0];\n"
    "    for (int i = 0; i < 64; i++) {\n"
    "        vec3 dd = texel.rgb - basePal[i];\n"
    "        float dist = dot(dd, dd);\n"
    "        if (dist < bestD) { bestD = dist; outc = curPal[i]; }\n"
    "    }\n"
    "    finalColor = vec4(outc, texel.a) * colDiffuse * fragColor;\n"
    "}\n";
#endif

// compile the swap shader + push the (constant) base palette. Call once, after the
// GL context exists. On failure pal_shader_ok stays false and sprites draw normally.
static void pal_shader_init(void) {
    pal_shader = LoadShaderFromMemory(0, PAL_FS);
    if (pal_shader.id == 0) return;
    loc_base_pal = GetShaderLocation(pal_shader, "basePal");
    loc_cur_pal  = GetShaderLocation(pal_shader, "curPal");
    if (loc_base_pal < 0 || loc_cur_pal < 0) { UnloadShader(pal_shader); return; }
    float base_rgb[PALETTE_SIZE * 3];
    for (int i = 0; i < PALETTE_SIZE; i++) {
        base_rgb[i*3+0] = base_palette[i].r / 255.0f;
        base_rgb[i*3+1] = base_palette[i].g / 255.0f;
        base_rgb[i*3+2] = base_palette[i].b / 255.0f;
    }
    SetShaderValueV(pal_shader, loc_base_pal, base_rgb, SHADER_UNIFORM_VEC3, PALETTE_SIZE);
    pal_shader_ok = true;
}

// "sharp bilinear" present filter. Keeps each source texel a FLAT colour and
// confines the blend to a 1-output-pixel seam at texel edges (fwidth sizes the
// seam to exactly one screen pixel, so it's correct at any scale). gammaCorrect
// blends the seam in linear light so a bright edge keeps its true brightness.
// Manual 4-tap (canvas stays POINT) so it matches the pixelperfect demo exactly.
// Desktop (GLSL 330) only — fwidth needs derivatives; web falls back to the
// chosen texture filter. Reads the canvas, so it runs at the final window blit.
#ifndef PLATFORM_WEB
static const char *SCALE_FS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec2 texSize;\n"
    "uniform int  gammaCorrect;\n"
    "void main() {\n"
    "    vec2 texel = fragTexCoord * texSize - 0.5;\n"   // texel-centre coords
    "    vec2 ii = floor(texel);\n"
    "    vec2 f  = texel - ii;\n"
    "    vec2 ppt = 1.0 / max(fwidth(fragTexCoord * texSize), vec2(1e-4));\n"  // output px per texel
    "    f = clamp((f - 0.5) * ppt + 0.5, 0.0, 1.0);\n"  // flat except a 1-px seam
    "    vec2 tx = 1.0 / texSize;\n"
    "    vec2 uv = (ii + 0.5) * tx;\n"
    "    vec3 c00 = texture(texture0, uv).rgb;\n"
    "    vec3 c10 = texture(texture0, uv + vec2(tx.x, 0.0)).rgb;\n"
    "    vec3 c01 = texture(texture0, uv + vec2(0.0, tx.y)).rgb;\n"
    "    vec3 c11 = texture(texture0, uv + vec2(tx.x, tx.y)).rgb;\n"
    "    vec3 col;\n"
    "    if (gammaCorrect == 1) {\n"
    "        c00 = pow(c00, vec3(2.2)); c10 = pow(c10, vec3(2.2));\n"
    "        c01 = pow(c01, vec3(2.2)); c11 = pow(c11, vec3(2.2));\n"
    "        col = pow(mix(mix(c00,c10,f.x), mix(c01,c11,f.x), f.y), vec3(1.0/2.2));\n"
    "    } else {\n"
    "        col = mix(mix(c00,c10,f.x), mix(c01,c11,f.x), f.y);\n"
    "    }\n"
    "    finalColor = vec4(col, 1.0) * colDiffuse * fragColor;\n"
    "}\n";
#endif

// load the sharp-bilinear shader on demand (present modes 2/3 OR smooth_zoom). On
// failure scale_shader_ok stays false and callers fall back to a plain bilinear blit.
// texSize/gamma uniforms are set per-use (the present and smooth use different sizes).
static void scale_shader_ensure(void) {
#ifndef PLATFORM_WEB
    if (scale_shader_ok || scale_shader.id != 0) return;
    scale_shader = LoadShaderFromMemory(0, SCALE_FS);
    if (scale_shader.id == 0) return;
    loc_scale_texsize = GetShaderLocation(scale_shader, "texSize");
    loc_scale_gamma   = GetShaderLocation(scale_shader, "gammaCorrect");
    scale_shader_ok = true;
#endif
}
static void scale_shader_init(void) {
#ifndef PLATFORM_WEB
    if (SCALE_FILTER >= 2) scale_shader_ensure();   // present filter wants it from the start
#endif
}

// smooth_zoom(on): toggle the 1:1-render → scaled-blit path for camera_ex's fractional
// zoom (see the SMOOTH_* note up top). One call; default off. Lazily allocates the
// offscreen + ensures the sharp shader (used for the magnify case; bilinear fallback).
void smooth_zoom(bool on) {
#ifndef PLATFORM_WEB
    smooth_on = on;
    if (on && !smooth_rt_ok) {
        smooth_rt = LoadRenderTexture(SMOOTH_W, SMOOTH_H);
        SetTextureFilter(smooth_rt.texture, TEXTURE_FILTER_BILINEAR);
        smooth_rt_ok = (smooth_rt.id != 0);
        scale_shader_ensure();
    }
#else
    (void)on;
#endif
}

// rebuild the normalized current-palette array + pal_active flag from palette[].
// called whenever pal()/pal_reset() changes a mapping.
static void pal_recompute(void) {
    pal_active = false;
    for (int i = 0; i < PALETTE_SIZE; i++) {
        cur_pal_rgb[i*3+0] = palette[i].r / 255.0f;
        cur_pal_rgb[i*3+1] = palette[i].g / 255.0f;
        cur_pal_rgb[i*3+2] = palette[i].b / 255.0f;
        if (palette[i].r != base_palette[i].r ||
            palette[i].g != base_palette[i].g ||
            palette[i].b != base_palette[i].b) pal_active = true;
    }
    cur_pal_dirty = true;
}

// wrap a sprite blit in the swap shader when any pal() mapping is live
static void pal_begin(void) {
    if (!pal_active || !pal_shader_ok) return;
    if (cur_pal_dirty) {
        SetShaderValueV(pal_shader, loc_cur_pal, cur_pal_rgb, SHADER_UNIFORM_VEC3, PALETTE_SIZE);
        cur_pal_dirty = false;
    }
    BeginShaderMode(pal_shader);
}
static void pal_end(void) {
    if (pal_active && pal_shader_ok) EndShaderMode();
}

void colorkey(int color) {
    if (!spritesheet_img.data) return;
    // canvas twin: remember the key (or clear it for an out-of-range colour, matching the
    // GPU path, which then rebuilds the sheet with no hole). Snapshot the RGB now — the GPU
    // bakes palette[color] into the texture at this moment, even if pal() changes it later.
    sw_colorkey_on = (color >= 0 && color < PALETTE_SIZE);
    if (sw_colorkey_on) sw_colorkey_rgb = palette[color];
#ifndef DE_NO_RAYLIB
    // GPU path: bake the key out into the sheet texture. The software path needs
    // none of this — it keys per-pixel at blit time via sw_colorkey_on/_rgb (set
    // above) — and these Raylib Image/Texture ops are stubs under DE_NO_RAYLIB
    // that would clobber spritesheet to 0x0 and trip the spritesheet.width guards.
    Image tmp = ImageCopy(spritesheet_img);
    if (color >= 0 && color < PALETTE_SIZE) {
        ImageFormat(&tmp, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        DeColor key = palette[color];
        ImageColorReplace(&tmp, key, (DeColor){ key.r, key.g, key.b, 0 });
    }
    if (spritesheet.width > 0) UnloadTexture(spritesheet);
    spritesheet = LoadTextureFromImage(tmp);
    SetTextureFilter(spritesheet, TEXTURE_FILTER_POINT);
    UnloadImage(tmp);
#endif
}

void spr(int index, int x, int y) {
    PROF("spr");
    sprf(index, x, y, false, false);
}

void sprf(int index, int x, int y, bool flip_x, bool flip_y) {
    PROF("sprf");
    if (spritesheet.width == 0) return;
    int cols = spritesheet.width / SPRITE_SIZE;
    Rectangle src = {
        .x      = (index % cols) * SPRITE_SIZE,
        .y      = (index / cols) * SPRITE_SIZE,
        .width  = flip_x ? -SPRITE_SIZE : SPRITE_SIZE,
        .height = flip_y ? -SPRITE_SIZE : SPRITE_SIZE,
    };
    Rectangle dst = { x, y, SPRITE_SIZE, SPRITE_SIZE };
    if (sw_canvas_active) {
        sw_blit((int)(index % cols) * SPRITE_SIZE, (int)(index / cols) * SPRITE_SIZE,
                SPRITE_SIZE, SPRITE_SIZE, x, y, SPRITE_SIZE, SPRITE_SIZE, flip_x, flip_y, true);
        UIAUDIT('s', x, y, SPRITE_SIZE, SPRITE_SIZE, NULL); return;
    }
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    pal_end();
    UIAUDIT('s', x, y, SPRITE_SIZE, SPRITE_SIZE, NULL);
}

void sspr(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    PROF("sspr");
    if (spritesheet.width == 0) return;
    Rectangle src = { sx, sy, sw, sh };
    Rectangle dst = { dx, dy, dw, dh };
    if (sw_canvas_active) { sw_blit(sx, sy, sw, sh, dx, dy, dw, dh, false, false, true); UIAUDIT('s', dx, dy, dw, dh, NULL); return; }
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    pal_end();
    UIAUDIT('s', dx, dy, dw, dh, NULL);
}

// CPU rotated sprite blit: INVERSE-map nearest — for each dest screen pixel, rotate it back about the
// pivot to unrotated dest-local coords, map to source (dw→sw, dh→sh scale), nearest-sample the sheet.
// Convention proven in tools/det-probes/rotspr.c: nearest == today's GPU point-filter quality + it's
// device-deterministic (quantized matrix). (RotSprite is the opt-in ≥16px upgrade — not built yet.)
// Reuses the sheet sampling + colorkey + pal() recolor from sw_blit; plots via pset_rgb so ONE impl
// serves canvas (→ sw_pset → cbuf) and the off-canvas DE_CPU_RASTER reference (→ DrawPixel). Pivot is
// world (dx+ox, dy+oy); rotation matrix [[c,-s],[s,c]] matches raylib DrawTexturePro's direction.
// generalized inverse-map rotated blit from any CPU Image. fonttint < 0 = SPRITE mode (sample the
// texel colour; honour colorkey + pal() recolor). fonttint >= 0 = FONT mode (sample the atlas alpha,
// write palette[fonttint] — a tinted glyph mask). Shared by spr_rot/sspr_ex (spritesheet) and
// print_rot (font atlas, per glyph). Pivot world (dx+ox, dy+oy); matrix [[c,-s],[s,c]] = raylib's.
static void de_cpu_img_rot(Image *img, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                           float deg, int ox, int oy, bool use_pal, int fonttint) {
    if (!img->data || dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    float a = deg * DEG2RAD;
    float c = roundf(cosf(a) * 4096.f) / 4096.f, s = roundf(sinf(a) * 4096.f) / 4096.f;
    float px0 = dx + ox, py0 = dy + oy;                       // pivot (world)
    bool recolor = use_pal && pal_active;
    bool oncanvas = sw_canvas_active;                          // hoist: plot straight to cbuf vs the pset dispatch
    DeColor tint = fonttint >= 0 ? palette[fonttint % PALETTE_SIZE] : (DeColor){0,0,0,0};
    // screen bbox = the 4 dest corners rotated forward about the pivot
    float cxx[4] = { dx - px0, dx + dw - px0, dx + dw - px0, dx - px0 };
    float cyy[4] = { dy - py0, dy - py0, dy + dh - py0, dy + dh - py0 };
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    for (int k = 0; k < 4; k++) {
        float rx = c * cxx[k] - s * cyy[k] + px0, ry = s * cxx[k] + c * cyy[k] + py0;
        if (rx < minx) minx = rx; if (rx > maxx) maxx = rx;
        if (ry < miny) miny = ry; if (ry > maxy) maxy = ry;
    }
    int x0 = (int)floorf(minx), x1 = (int)ceilf(maxx), y0 = (int)floorf(miny), y1 = (int)ceilf(maxy);
    // FAST PATH (canvas, zoom 1): the camera is a constant offset and the cbuf row base is constant
    // per output row — hoist both out of the per-pixel plot, so the inner write is just row[sx]=pack.
    // Byte-identical to the sw_pset path (same camera + clip + bottom-up store). zoom!=1 keeps sw_pset
    // (it fills each world pixel's screen footprint); off-canvas keeps pset_rgb (the A/B reference).
    bool fast = oncanvas && cam.zoom == 1.0f;
    int camdx = (int)(cam.target.x - cam.offset.x), camdy = (int)(cam.target.y - cam.offset.y);
    for (int py = y0; py <= y1; py++) {
        uint32_t *row = NULL;
        if (fast) {
            int syc = py - camdy;
            if ((unsigned)syc >= (unsigned)de_sh) continue;
            if (clip_active && (syc < clip_cy || syc >= clip_cy + clip_ch)) continue;
            row = &sw_dst[(de_sh - 1 - syc) * fb_w];
        }
        for (int px = x0; px <= x1; px++) {
            float ddx = px + 0.5f - px0, ddy = py + 0.5f - py0;
            float lx = c * ddx + s * ddy, ly = -s * ddx + c * ddy;     // rotate dest by -a
            float fxu = lx + ox, fyu = ly + oy;                        // dest-local (0..dw, 0..dh)
            if (fxu < 0 || fxu >= dw || fyu < 0 || fyu >= dh) continue;
            int ssx, ssy;                                              // nearest source texel
            if (sw == dw && sh == dh) { ssx = sx + (int)fxu;          ssy = sy + (int)fyu; }          // no scale (spr_rot)
            else                      { ssx = sx + (int)(fxu*sw/dw);  ssy = sy + (int)(fyu*sh/dh); }  // scaled
            DeColor cc = img_texel(img, ssx, ssy);
            if (fonttint >= 0) { if (cc.a < 128) continue; cc = tint; }
            else { if (cc.a < 128 || sw_keyed(cc)) continue; if (recolor) cc = sw_recolor(cc); }
            if (fast) {
                int sxc = px - camdx;
                if ((unsigned)sxc >= (unsigned)de_sw) continue;
                if (clip_active && (sxc < clip_cx || sxc >= clip_cx + clip_cw)) continue;
                row[sxc] = sw_pack(cc);                                       // hoisted cbuf write
            } else if (oncanvas) sw_pset(px, py, cc);                         // canvas zoom!=1 (footprint)
            else pset_rgb(px, py, (cc.r << 16) | (cc.g << 8) | cc.b);         // off-canvas A/B reference
        }
    }
}
// sprite wrapper (spritesheet, sprite mode)
static void de_cpu_sspr_rot(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                            float deg, int ox, int oy, bool use_pal) {
    de_cpu_img_rot(&spritesheet_img, sx, sy, sw, sh, dx, dy, dw, dh, deg, ox, oy, use_pal, -1);
}

void spr_rot(int index, int x, int y, float deg) {
    PROF("spr_rot");
    if (spritesheet.width == 0) return;
    int cols = spritesheet.width / SPRITE_SIZE;
    int srcx = (index % cols) * SPRITE_SIZE, srcy = (index / cols) * SPRITE_SIZE;
    int h = SPRITE_SIZE / 2;
    if (sw_canvas_active || cpu_raster_enabled) {   // SW: inverse-map (no GPU fallback); also the A/B reference
        de_cpu_sspr_rot(srcx, srcy, SPRITE_SIZE, SPRITE_SIZE, x, y, SPRITE_SIZE, SPRITE_SIZE, deg, h, h, true); return;
    }
    Rectangle src = { srcx, srcy, SPRITE_SIZE, SPRITE_SIZE };
    Rectangle dst = { x + h, y + h, SPRITE_SIZE, SPRITE_SIZE };  // origin maps here, so top-left stays (x,y)
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ (float)h, (float)h }, deg, WHITE);
    pal_end();
}

void sspr_ex(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, float deg, int ox, int oy) {
    PROF("sspr_ex");
    if (spritesheet.width == 0) return;
    if (deg != 0.0f && (sw_canvas_active || cpu_raster_enabled)) {   // SW rotated: inverse-map + A/B reference
        de_cpu_sspr_rot(sx, sy, sw, sh, dx, dy, dw, dh, deg, ox, oy, true); return;
    }
    if (sw_canvas_active) { sw_blit(sx, sy, sw, sh, dx, dy, dw, dh, false, false, true); return; }   // deg==0 on canvas
    Rectangle src = { sx, sy, sw, sh };
    Rectangle dst = { dx + ox, dy + oy, dw, dh };               // pivot (ox,oy) is in dest space, relative to (dx,dy)
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ (float)ox, (float)oy }, deg, WHITE);
    pal_end();
}

// ------------------------------------------------------------
// font state + print helpers
// ------------------------------------------------------------

void font(int f) { active_font_id = (f == FONT_SMALL || f == FONT_TINY || f == FONT_COMIC || f == FONT_THIN) ? f : FONT_NORMAL; }

static Font cur_font(void) {
    if (active_font_id == FONT_SMALL) return font_small;
    if (active_font_id == FONT_TINY)  return font_tiny;
    if (active_font_id == FONT_COMIC) return font_comic;
    if (active_font_id == FONT_THIN) return font_thin;
    return game_font;
}
static Image *cur_font_img(void) {
    if (active_font_id == FONT_SMALL) return &font_small_img;
    if (active_font_id == FONT_TINY)  return &font_tiny_img;
    if (active_font_id == FONT_COMIC) return &font_comic_img;
    if (active_font_id == FONT_THIN)  return &font_thin_img;
    return &game_font_img;
}
// software text: blit each glyph from the font atlas (tinted to `color`), mirroring DrawTextEx's
// glyph iteration at scale 1 (these bitmap fonts draw at baseSize). Returns the pen x after the text.
static int sw_print(const char *text, int x, int y, DeColor tint) {
    Font f = cur_font(); Image *img = cur_font_img();
    if (!img->data) return x;
    int penx = x, peny = y;
    for (int i = 0; text[i]; ) {
        int sz; int cp = GetCodepointNext(&text[i], &sz); i += sz;
        if (cp == '\n') { peny += f.baseSize; penx = x; continue; }
        int gi = GetGlyphIndex(f, cp);
        Rectangle r = f.recs[gi];
        int ox = f.glyphs[gi].offsetX, oy = f.glyphs[gi].offsetY;
        for (int gy = 0; gy < (int)r.height; gy++)
            for (int gx = 0; gx < (int)r.width; gx++) {
                DeColor a = GetImageColor(*img, (int)r.x + gx, (int)r.y + gy);
                if (a.a >= 128) sw_pset(penx + ox + gx, peny + oy + gy, tint);
            }
        int adv = f.glyphs[gi].advanceX; if (adv == 0) adv = (int)r.width;
        penx += adv;
    }
    return penx;
}

static float cur_font_size(void) {
    Font f = cur_font();
    return (float)f.baseSize;
}

int text_width(const char *t) {
    return (int)MeasureTextEx(cur_font(), t, cur_font_size(), 0).x;
}

int print(const char *text, int x, int y, int color) {
    PROF("print");
    if (sw_canvas_active) { sw_print(text, x, y, palette[color % PALETTE_SIZE]); return x + text_width(text); }
    DrawTextEx(cur_font(), text, (Vector2){ (float)x, (float)y },
               cur_font_size(), 0, palette[color % PALETTE_SIZE]);
    int w = text_width(text);
    UIAUDIT('t', x, y, w, (int)cur_font_size(), text);
    return x + w;
}


int print_outline(const char *text, int x, int y, int color, int outline_color) {
    PROF("print_outline");
    DeColor oc = palette[outline_color % PALETTE_SIZE];
    float sz = cur_font_size();
    Font  f  = cur_font();
    static const int offsets[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
    for (int i = 0; i < 8; i++) {
        if (sw_canvas_active) sw_print(text, x + offsets[i][0], y + offsets[i][1], oc);
        else DrawTextEx(f, text,
                        (Vector2){ (float)(x + offsets[i][0]), (float)(y + offsets[i][1]) },
                        sz, 0, oc);
    }
    return print(text, x, y, color);
}

// CPU rotated text: a glyph is a tiny sprite (det-probes/textrot.c), so lay the string out in scaled
// text-local space and inverse-map-rotate each glyph about the shared pivot (x,y) via de_cpu_img_rot
// in FONT mode. Mirrors DrawTextPro: screen = (x,y) + R·(local − org). No GPU fallback; also the
// DE_CPU_RASTER reference path. (RotSprite is the future ≥16px opt-in; tiny glyphs ship nearest.)
static void de_cpu_print_rot(const char *text, int x, int y, float deg, int scale, int color, int pivot) {
    Font f = cur_font(); Image *img = cur_font_img();
    if (!img->data) return;
    int ox0 = 0, oy0 = 0;                                       // pivot offset (org), scaled
    if (pivot) { ox0 = (text_width(text) * scale) / 2; oy0 = ((int)cur_font_size() * scale) / 2; }
    int penx = 0, peny = 0;                                     // scaled-local pen, text origin (0,0)
    for (int i = 0; text[i]; ) {
        int sz; int cp = GetCodepointNext(&text[i], &sz); i += sz;
        if (cp == '\n') { peny += f.baseSize * scale; penx = 0; continue; }
        int gi = GetGlyphIndex(f, cp);
        Rectangle r = f.recs[gi];
        int gw = (int)r.width, gh = (int)r.height;
        int dstTLx = x + penx + f.glyphs[gi].offsetX * scale - ox0;   // unrotated (deg 0) glyph TL
        int dstTLy = y + peny + f.glyphs[gi].offsetY * scale - oy0;
        de_cpu_img_rot(img, (int)r.x, (int)r.y, gw, gh, dstTLx, dstTLy, gw * scale, gh * scale,
                       deg, x - dstTLx, y - dstTLy, false, color);   // pivot = (x,y); font mode
        int adv = f.glyphs[gi].advanceX; if (adv == 0) adv = gw;
        penx += adv * scale;
    }
}

int print_rot(const char *text, int x, int y, float deg, int color, int pivot) {
    PROF("print_rot");
    if (deg != 0.0f && (sw_canvas_active || cpu_raster_enabled)) {   // SW rotated text + A/B reference
        de_cpu_print_rot(text, x, y, deg, 1, color, pivot); return x + text_width(text);
    }
    if (sw_canvas_active) return sw_print(text, x, y, palette[color % PALETTE_SIZE]);   // deg==0 on canvas
    Vector2 org = { 0, 0 };
    if (pivot) { org.x = text_width(text) / 2.0f; org.y = cur_font_size() / 2.0f; }
    DrawTextPro(cur_font(), text, (Vector2){ (float)x, (float)y }, org,
                deg, cur_font_size(), 0, palette[color % PALETTE_SIZE]);
    return x + text_width(text);
}

int print_rot_scaled(const char *text, int x, int y, float deg, int scale, int color, int pivot) {
    PROF("print_rot_scaled");
    if (scale < 1) scale = 1;
    if ((deg != 0.0f || scale != 1) && (sw_canvas_active || cpu_raster_enabled)) {   // SW rotated/scaled + A/B reference
        de_cpu_print_rot(text, x, y, deg, scale, color, pivot); return x + text_width(text) * scale;
    }
    if (sw_canvas_active) return sw_print(text, x, y, palette[color % PALETTE_SIZE]);   // deg==0 & scale==1 on canvas
    float sz = cur_font_size() * scale;        // DrawTextPro scales by fontSize; POINT filter keeps it crisp
    Vector2 org = { 0, 0 };
    if (pivot) { org.x = (text_width(text) * scale) / 2.0f; org.y = sz / 2.0f; }
    DrawTextPro(cur_font(), text, (Vector2){ (float)x, (float)y }, org,
                deg, sz, 0, palette[color % PALETTE_SIZE]);
    return x + text_width(text) * scale;
}

void rect(int x, int y, int w, int h, int color) {
    PROF("rect");
    if (sw_canvas_active) {   // border via plot_pat → pset → sw_pset
        for (int xx = x; xx < x + w; xx++) { plot_pat(xx, y, color); plot_pat(xx, y + h - 1, color); }
        for (int yy = y + 1; yy < y + h - 1; yy++) { plot_pat(x, yy, color); plot_pat(x + w - 1, yy, color); }
        UIAUDIT('R', x, y, w, h, NULL); return;
    }
    DeColor c = palette[color % PALETTE_SIZE];
    int rx = x, ry = y;
    // 1px DrawRectangle slices — no line caps, exact pixel coverage
    DrawRectangle(rx,     ry,     w,   1,   c);  // top
    DrawRectangle(rx,     ry+h-1, w,   1,   c);  // bottom
    DrawRectangle(rx,     ry+1,   1,   h-2, c);  // left
    DrawRectangle(rx+w-1, ry+1,   1,   h-2, c);  // right
    UIAUDIT('R', x, y, w, h, NULL);
}

void rectfill(int x, int y, int w, int h, int color) {
    PROF("rectfill");
    if (sw_canvas_active) {
        if (fp_on) { for (int yy = y; yy < y + h; yy++) for (int xx = x; xx < x + w; xx++) plot_pat(xx, yy, color); }  // dither: per-pixel
        else sw_fillrect(x, y, w, h, palette[color % PALETTE_SIZE]);   // solid: fast cbuf row memset
        UIAUDIT('f', x, y, w, h, NULL); return;
    }
    if (fp_on) { rectfill_pat(x, y, w, h, fp_global, fp_hole, color); UIAUDIT('f', x, y, w, h, NULL); return; }   // 1-bits = holes, 0-bits = color
    DrawRectangle(x, y, w, h, palette[color % PALETTE_SIZE]);
    UIAUDIT('f', x, y, w, h, NULL);
}

// raw 24-bit filled block: 0xRRGGBB straight to the canvas, no palette. one call
// per shader cell beats a pset_rgb pixel-loop (chunky CPU shaders, true-colour bars).
void rectfill_rgb(int x, int y, int w, int h, int hex) {
    PROF("rectfill_rgb");
    DeColor c = { (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, 255 };
    if (sw_canvas_active) { sw_fillrect(x, y, w, h, c); return; }   // cbuf row memset (the true-colour CPU-shader path)
    DrawRectangle(x, y, w, h, c);
}

// filled rect centred at (cx,cy), rotated `deg`° — emitted as real GPU geometry (two
// triangles via DrawRectanglePro), NOT a software scanline/coverage fill. That's the whole
// point: under a rotated camera_ex() the software fills (trifill/polyfill/thickline) rasterise
// in world space and the rotation staircases them into a dotted lattice of holes; a GPU quad
// is filled by the rasteriser in screen space after the transform, so it stays solid at any
// angle. The pivot is the rect's own centre, so (cx,cy) holds still as it spins.
// CPU rotated fill: INVERSE mapping (visit each dest pixel in the rotated rect's world bbox, rotate
// it back to rect-local, test inside) → gap-free at every angle by construction, device-deterministic
// via the 1/4096-quantized matrix. Proven in tools/det-probes/rotfill.c (gap-free all 360°,
// bit-identical arm64/x86/wasm). Matches raylib DrawRectanglePro's forward matrix [[c,-s],[s,c]], so
// rotation direction agrees with the GPU path. Plots via the public pset() so ONE impl serves both:
// on-canvas → sw_pset → cbuf; off-canvas (DE_CPU_RASTER) → DrawPixel — identical pixel set either way,
// so a canvas A/B of a rotated-fill cart is byte-exact. Camera translate/zoom + clip come for free.
static void de_cpu_rectfill_rot(int cx, int cy, int w, int h, float deg, int color) {
    float a = deg * DEG2RAD;
    float c = roundf(cosf(a) * 4096.f) / 4096.f, s = roundf(sinf(a) * 4096.f) / 4096.f;
    float hw = w * 0.5f, hh = h * 0.5f;
    float ex = fabsf(c * hw) + fabsf(s * hh), ey = fabsf(s * hw) + fabsf(c * hh);   // world bbox half-extents
    int x0 = (int)floorf(cx - ex), x1 = (int)ceilf(cx + ex);
    int y0 = (int)floorf(cy - ey), y1 = (int)ceilf(cy + ey);
    for (int py = y0; py <= y1; py++) for (int px = x0; px <= x1; px++) {
        float dx = px + 0.5f - cx, dy = py + 0.5f - cy;
        float lx = c * dx + s * dy, ly = -s * dx + c * dy;     // rotate dest by -a → rect-local
        if (fabsf(lx) <= hw && fabsf(ly) <= hh) pset(px, py, color);
    }
}

void rectfill_rot(int cx, int cy, int w, int h, float deg, int color) {
    PROF("rectfill_rot");
    if (sw_canvas_active || cpu_raster_enabled) { de_cpu_rectfill_rot(cx, cy, w, h, deg, color); return; }   // SW: inverse-map (no GPU fallback); also the A/B reference path
    DrawRectanglePro((Rectangle){ (float)cx, (float)cy, (float)w, (float)h },
                     (Vector2){ w * 0.5f, h * 0.5f }, deg, palette[color % PALETTE_SIZE]);
}

void bar(int x, int y, int w, int h, float pct, int fill, int bg) {
    PROF("bar");
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    rectfill(x, y, w, h, bg);
    int fw = (int)(w * pct + 0.5f);
    if (fw > 0) rectfill(x, y, fw, h, fill);
}

// patterned fill: each (pattern,c1,c0) bakes a tiny 4×4 POT texture, tiled in one
// draw via REPEAT wrap. We CACHE textures (never unload one mid-frame) so multiple
// different patterns in the same frame don't corrupt raylib's draw batch.
// 17 unique threshold patterns × N vgradient calls per frame; 32 was too small for 2
// stacked vgradients (34 needed) — eviction caused OpenGL to reuse a texture ID still
// referenced by a pending batch entry, drawing the wrong gradient colour at the top row.
#define FP_CACHE 64
static struct { int pat, c1, c0; Texture2D tex; bool used; } fp_cache[FP_CACHE];
static int fp_round = 0;

static Texture2D fp_texture(int pat, int c1, int c0) {
    for (int i = 0; i < FP_CACHE; i++)
        if (fp_cache[i].used && fp_cache[i].pat == pat && fp_cache[i].c1 == c1 && fp_cache[i].c0 == c0)
            return fp_cache[i].tex;
    DeColor px[16];
    for (int i = 0; i < 16; i++) {
        int on = (pat >> (15 - i)) & 1;                       // bit 15 = top-left, row-major
        px[i] = on ? (c1 < 0 ? (DeColor){ 0, 0, 0, 0 } : palette[c1 % PALETTE_SIZE])
                   : (c0 < 0 ? (DeColor){ 0, 0, 0, 0 } : palette[c0 % PALETTE_SIZE]);
    }
    Image img = { px, 4, 4, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    int slot = fp_round++ % FP_CACHE;
    if (fp_cache[slot].used) UnloadTexture(fp_cache[slot].tex);   // safe: prior frames already flushed
    fp_cache[slot].pat = pat; fp_cache[slot].c1 = c1; fp_cache[slot].c0 = c0;
    fp_cache[slot].tex = t;   fp_cache[slot].used = true;
    return t;
}
static void rectfill_pat(int x, int y, int w, int h, int pattern, int c1, int c0) {
    if (w <= 0 || h <= 0) return;
    Texture2D t = fp_texture(pattern, c1, c0);
    // src origin = world pos minus the fillp anchor, so the 4×4 lattice tiles in world space
    // (sticks to what you draw) and stays seamless with the circ/tri pattern fills (same
    // origin). Anchor to a moving shape's center to make the pattern travel with it instead
    // of the shape sliding over a fixed lattice. The camera matrix then scales/rotates the quad.
    Rectangle src = { (float)(x - fp_anc_x), (float)(y - fp_anc_y), (float)w, (float)h };
    Rectangle dst = { (float)x, (float)y, (float)w, (float)h };
    DrawTexturePro(t, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

// circles/triangles are filled as horizontal scanlines of rectfill_pat — reusing
// the proven (and screen-aligned) rect path, so the lattice stays seamless.
static void ovalfill_pat(int cx, int cy, int rx, int ry, int pattern, int c1, int c0) {
    if (rx <= 0 || ry <= 0) return;
    for (int dy = -ry; dy <= ry; dy++) {
        float f = 1.0f - (float)(dy * dy) / (float)(ry * ry);
        if (f < 0) f = 0;
        int hw = (int)(rx * sqrtf(f) + 0.5f);
        rectfill_pat(cx - hw, cy + dy, hw * 2 + 1, 1, pattern, c1, c0);
    }
}

static void circfill_pat(int cx, int cy, int r, int pattern, int c1, int c0) {
    ovalfill_pat(cx, cy, r, r, pattern, c1, c0);
}

// ── global fill pattern (PICO-8 fillp style) ──────────────────────────────
// when on, the normal fills draw the pattern: the draw COLOR fills the 0-bits,
// the 1-bits are transparent (holes) — exactly like PICO-8 fillp.
void fillp(int pattern, int hole_color) { fp_on = true; fp_global = pattern; fp_hole = hole_color; }
void fillp_reset(void)  { fp_on = false; }
void fillp_anchor(int ox, int oy) { fp_anc_x = ox; fp_anc_y = oy; }

// Pixel-center coverage tests — one definition shared by fill, outline, and dither.
// Using x+0.5 / y+0.5 so the disc boundary is between pixels, not on them.
// circ and circfill both call disc_inside → outline is always the outermost
// ring of the fill, never a pixel outside it.
static bool disc_inside(int x, int y, int cx, int cy, int r) {
    float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
    return dx*dx + dy*dy <= (float)r * r;
}
static bool ellipse_inside(int x, int y, int cx, int cy, int rx, int ry) {
    float dx = (x + 0.5f - cx) / (float)rx, dy = (y + 0.5f - cy) / (float)ry;
    return dx*dx + dy*dy <= 1.0f;
}
// rounded rect = all pixels within r of the inner rect of corner centres.
// Straight edges → distance 0 → inside; corners reduce to disc_inside against
// the nearest corner centre (circfill convention), so fill/outline/dither agree.
static bool rrect_inside(int px, int py, int x, int y, int w, int h, int r) {
    float cx = px + 0.5f, cy = py + 0.5f;
    float l = x + r, t = y + r, rt = x + w - 1 - r, b = y + h - 1 - r;
    float dx = cx < l ? l - cx : (cx > rt ? cx - rt : 0.0f);
    float dy = cy < t ? t - cy : (cy > b ? cy - b : 0.0f);
    return dx*dx + dy*dy <= (float)r * r;
}

void circ(int cx, int cy, int r, int color) {
    PROF("circ");
    // outline = pixels inside the disc that have at least one outside 4-neighbour.
    // SEAM for an O(perimeter) outline (the fills got the span treatment; the STROKES
    // didn't). This scans the whole O(r²) bbox testing disc_inside 5×/pixel for an O(r)
    // ring — a span version would, per row, emit just the endpoints [a,b] of the disc
    // span (from dx=√(r²-dy²), like circfill) plus the extra cap pixels where the row
    // above/below is narrower. Same idea fits `oval` and `poly_stroke_cov` below. PARKED —
    // measured low-leverage: it was 74% of `orbit` (R=120 planet ring) but orbit is now
    // 1.6ms/9% budget after the fill fix, and outlines are small/rare fleet-wide (survey:
    // circ ~109/frame across 9 carts). Finish-the-set tidiness, not a fleet win. See
    // docs/guides/engine-optimization.md → "Outline strokes (parked)".
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++)
            if (disc_inside(x,y,cx,cy,r) &&
                (!disc_inside(x-1,y,cx,cy,r) || !disc_inside(x+1,y,cx,cy,r) ||
                 !disc_inside(x,y-1,cx,cy,r) || !disc_inside(x,y+1,cx,cy,r)))
                pset(x, y, color);
    UIAUDIT('c', cx - r, cy - r, 2 * r + 1, 2 * r + 1, NULL);
}

void circfill(int cx, int cy, int r, int color) {
    PROF("circfill");
    // SPAN fast path: each row is one contiguous disc span [cx-dx, cx+dx], dx=√(r²-dy²),
    // drawn as ONE DrawRectangle instead of per-pixel pset — same villain we span-filled
    // for polygons. Byte-identical to the per-pixel disc_inside scan: candidate span is
    // generous (floor/ceil) then trimmed inward by the exact disc_inside predicate (a disc
    // row is a single contiguous interval, so no cross-span bleed). Gated on rotation==0 +
    // solid fill; ZOOM is fine (rotation-0 camera is axis-aligned affine → the run of 1×1
    // world quads tiles to exactly the same screen pixels as one w×1 quad; only rotation
    // breaks that). A rotated camera or fillp() dither falls back to per-pixel.
    if (disc_fill_fast && r >= 1 && cam.rotation == 0.0f && !fp_on) {
        int x0 = cx - r, y0 = cy - r, x1 = cx + r, y1 = cy + r;
        poly_clamp_scan(&x0, &y0, &x1, &y1);            // skip off-screen rows/cols (the poly-path win)
        DeColor col = palette[color % PALETTE_SIZE];
        float r2 = (float)r * r;
        for (int y = y0; y <= y1; y++) {
            float dy = y + 0.5f - cy;
            float h2 = r2 - dy * dy;
            if (h2 < 0) continue;
            float hx = sqrtf(h2);
            int a = (int)floorf(cx - hx - 0.5f);        // generous, then trim to the exact predicate
            int b = (int)ceilf (cx + hx - 0.5f);
            while (a <= b && !disc_inside(a, y, cx, cy, r)) a++;
            while (b >= a && !disc_inside(b, y, cx, cy, r)) b--;
            if (a < x0) a = x0;  if (b > x1) b = x1;
            if (a <= b) sw_span(a, y, b - a + 1, col);
        }
    } else {
        // legacy / fallback — all pixels inside the disc; plot_pat handles solid + fillp dither
        for (int y = cy - r; y <= cy + r; y++)
            for (int x = cx - r; x <= cx + r; x++)
                if (disc_inside(x, y, cx, cy, r)) plot_pat(x, y, color);
    }
    UIAUDIT('c', cx - r, cy - r, 2 * r + 1, 2 * r + 1, NULL);
}

// A triangle is just a 3-vertex polygon, so tri/trifill share the same
// pixel-center coverage as ngon/star/poly — fill, dither and outline all agree,
// and the outline is exactly the boundary of the fill (no GPU line-vs-fill drift).
// Winding-independent: poly_inside is even-odd, so either vertex order fills.
void tri(int x1, int y1, int x2, int y2, int x3, int y3, int color) {
    PROF("tri");
    int pts[6] = {x1,y1, x2,y2, x3,y3};
    poly_stroke_cov(pts, 3, color);
}

void trifill(int x1, int y1, int x2, int y2, int x3, int y3, int color) {
    PROF("trifill");
    // SEAM for a dedicated triangle rasterizer. Today a triangle is just a 3-vertex
    // polygon → poly_fill_cov already span-fills it (one DrawRectangle per row). A
    // hand-rolled path (sort verts by y, walk the two active edges with incremental
    // x += inv_slope — no per-row division, cheap bbox clamp instead of poly_clamp_scan's
    // matrix inverts) would slot in HERE behind a flag. PARKED — measured not worth it:
    // tristress BIG 0.33ms / MANY (421 tris) 0.97ms, no real cart trifill-bound; the per-
    // call overhead it'd cut is shared by all fills and better solved by a per-frame
    // clamp-box cache. Rig: tools/carts/tristress.c. See guides/engine-optimization.md.
    int pts[6] = {x1,y1, x2,y2, x3,y3};
    poly_fill_cov(pts, 3, color);
}

// ── arcs / sectors ── angles in degrees, 0 = right, 90 = down (same as dx/dy).
// all per-pixel off one circle definition, so the outline (arc) exactly caps
// the fills (arcfill/ring) — draw the fill, then arc() on top, no gap.
static void sector_fill(int cx, int cy, int r_in, int r_out, float s, float e, int color);  // fwd

void arc(int x, int y, int radius, float start_deg, float end_deg, int color) {
    PROF("arc");
    sector_fill(x, y, radius > 1 ? radius - 1 : 0, radius, start_deg, end_deg, color);  // 1px-thick rim
}

// fill one pixel honoring the active fillp() pattern (palette/camera/clip come
// from pset). 0-bits take the draw color, 1-bits the hole color (skip if < 0).
static void plot_pat(int x, int y, int color) {
    if (!fp_on) { pset(x, y, color); return; }
    // anchor the 4×4 lattice to (fp_anc_x,fp_anc_y) so a moving shape can carry its
    // pattern instead of sliding over a world-pinned grid. (& 3 == mod 4, also for negatives.)
    int ax = (x - fp_anc_x) & 3, ay = (y - fp_anc_y) & 3;
    int bit = (fp_global >> (15 - ((ay * 4) + ax))) & 1;
    if (bit) { if (fp_hole >= 0) pset(x, y, fp_hole); }
    else     pset(x, y, color);
}

// exact per-pixel sector coverage: a pixel is in iff its distance is within
// [r_in, r_out] AND its angle is in the swept range. One definition shared by
// the fill and the outline (boundary ring), so the outline always hugs the fill.
static bool sector_inside(int xx, int yy, int cx, int cy, float ri2, float ro2,
                          float lo, float hi, int full) {
    float dxp = xx + 0.5f - cx, dyp = yy + 0.5f - cy, d2 = dxp*dxp + dyp*dyp;
    if (d2 > ro2 || d2 < ri2) return false;
    if (full) return true;
    float pa = atan2f(dyp, dxp) * RAD2DEG;                              // 0=right, 90=down
    while (pa < lo)           pa += 360.0f;
    while (pa >= lo + 360.0f) pa -= 360.0f;
    return pa <= hi;
}
// fill (stroke_only=false) or boundary ring (stroke_only=true) of a sector.
static void sector_draw(int cx, int cy, int r_in, int r_out, float s, float e,
                        int color, bool stroke_only) {
    float lo = s < e ? s : e, hi = s < e ? e : s;
    int full = (hi - lo) >= 360.0f;
    float ri2 = (float)r_in * r_in, ro2 = (float)r_out * r_out;
    for (int yy = cy - r_out; yy <= cy + r_out; yy++)
        for (int xx = cx - r_out; xx <= cx + r_out; xx++) {
            if (!sector_inside(xx,yy,cx,cy,ri2,ro2,lo,hi,full)) continue;
            if (stroke_only &&                                         // skip interior pixels
                sector_inside(xx-1,yy,cx,cy,ri2,ro2,lo,hi,full) &&
                sector_inside(xx+1,yy,cx,cy,ri2,ro2,lo,hi,full) &&
                sector_inside(xx,yy-1,cx,cy,ri2,ro2,lo,hi,full) &&
                sector_inside(xx,yy+1,cx,cy,ri2,ro2,lo,hi,full)) continue;
            if (stroke_only) pset(xx, yy, color); else plot_pat(xx, yy, color);
        }
}
static void sector_fill(int cx, int cy, int r_in, int r_out, float s, float e, int color) {
    sector_draw(cx, cy, r_in, r_out, s, e, color, false);
}

void arcfill(int x, int y, int radius, float start_deg, float end_deg, int color) {
    PROF("arcfill");
    sector_draw(x, y, 0, radius, start_deg, end_deg, color, false);
}
// outline of a filled pie wedge — outer arc + the two radial edges (boundary ring
// of arcfill). Distinct from arc(), which strokes only the curved rim.
void arcoutline(int x, int y, int radius, float start_deg, float end_deg, int color) {
    PROF("arcoutline");
    sector_draw(x, y, 0, radius, start_deg, end_deg, color, true);
}

void ring(int x, int y, int r_in, int r_out, float start_deg, float end_deg, int color) {
    PROF("ring");
    sector_draw(x, y, r_in, r_out, start_deg, end_deg, color, false);
}
// outline of a filled ring/annulus sector — boundary ring of ring().
void ringoutline(int x, int y, int r_in, int r_out, float start_deg, float end_deg, int color) {
    PROF("ringoutline");
    sector_draw(x, y, r_in, r_out, start_deg, end_deg, color, true);
}

// affine texture-mapped triangle — the PS1 workhorse. Each corner carries a
// screen position (x,y) AND a spot on the sprite sheet (u,v, in sheet pixels).
// The GPU interpolates the texture across the triangle in SCREEN space with no
// perspective correction — that's the authentic "swimmy"/warping PS1 look. For
// 3D, project your model to 2D yourself (see the textured-cube cart) then feed
// the screen coords here. Sampling is nearest-neighbour; sheet-alpha shows through.
void tritex(int x1, int y1, float u1, float v1,
            int x2, int y2, float u2, float v2,
            int x3, int y3, float u3, float v3) {
    PROF("tritex");
    if (spritesheet.width == 0) return;
    if (sw_canvas_active || cpu_raster_enabled) { sw_tritex(x1,y1,u1,v1, x2,y2,u2,v2, x3,y3,u3,v3); return; }   // SW canvas + the A/B reference path
    float tw = (float)spritesheet.width, th = (float)spritesheet.height;
    typedef struct { float x, y, u, v; } TV;
    TV a = { x1, y1, u1, v1 };
    TV b = { x2, y2, u2, v2 };
    TV c = { x3, y3, u3, v3 };
    // raylib batches want CCW winding in Y-down screen space; swap if clockwise
    float cross = (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
    if (cross > 0) { TV t = b; b = c; c = t; }
    // emit as a degenerate QUAD (4th vertex repeats the 3rd) — this mirrors the
    // proven DrawTexturePro path; texturing via RL_TRIANGLES doesn't sample in
    // raylib's default batch.
    rlSetTexture(spritesheet.id);
    rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        rlTexCoord2f(a.u / tw, a.v / th); rlVertex2f(a.x, a.y);
        rlTexCoord2f(b.u / tw, b.v / th); rlVertex2f(b.x, b.y);
        rlTexCoord2f(c.u / tw, c.v / th); rlVertex2f(c.x, c.y);
        rlTexCoord2f(c.u / tw, c.v / th); rlVertex2f(c.x, c.y);
    rlEnd();
    rlSetTexture(0);
}

// SEAM: de_cpu_line is the pset-dispatched twin of sw_sline (above) — SAME reflection-symmetric
// per-axis DDA math (ported from tools/carts/{linesym,streetlab}.c's sline), but it plots via the
// public pset() so it works on BOTH backends: off-canvas pset → DrawPixel (GPU), on-canvas pset →
// sw_pset → cbuf. sw_sline writes the cbuf directly (the canvas hot path, no per-pixel pset branch),
// so we keep both: line() stays on sw_sline when the canvas is active, and only falls to de_cpu_line
// for the off-canvas DE_CPU_RASTER case. Why this exists: docs/design/software-canvas.md §"DDA vs
// coverage for the line" + rasterization-consistency.md. Interim A/B hygiene, not a direction-1 commit.
static void de_cpu_plot_minor(int maj, float v, float mid, int horiz, int c) {
    float f = floorf(v); int fi = (int)f; float r = v - f; int m;
    if (r != 0.5f) m = (r < 0.5f) ? fi : fi + 1;
    else if (v == mid) { if (horiz) { pset(maj,fi,c); pset(maj,fi+1,c); } else { pset(fi,maj,c); pset(fi+1,maj,c); } return; }
    else m = (mid > v) ? fi + 1 : fi;
    if (horiz) pset(maj, m, c); else pset(m, maj, c);
}
static void de_cpu_line(int x0, int y0, int x1, int y1, int c) {
    int dx = x1-x0, dy = y1-y0, adx = dx<0?-dx:dx, ady = dy<0?-dy:dy;
    if (adx == 0 && ady == 0) { pset(x0, y0, c); return; }
    if (adx >= ady) { int lo = x0<x1?x0:x1, hi = x0<x1?x1:x0; float ymid = (y0+y1)*0.5f;
        for (int x = lo; x <= hi; x++) de_cpu_plot_minor(x, y0 + (float)(x-x0)*dy/(float)dx, ymid, 1, c);
    } else { int lo = y0<y1?y0:y1, hi = y0<y1?y1:y0; float xmid = (x0+x1)*0.5f;
        for (int y = lo; y <= hi; y++) de_cpu_plot_minor(y, x0 + (float)(y-y0)*dx/(float)dy, xmid, 0, c); }
}

void line(int x1, int y1, int x2, int y2, int color) {
    PROF("line");
    if (sw_canvas_active) { sw_sline(x1, y1, x2, y2, palette[color % PALETTE_SIZE]); return; }
    if (cpu_raster_enabled) { de_cpu_line(x1, y1, x2, y2, color); return; }   // DE_CPU_RASTER: CPU line off-canvas too
    DrawLine(x1, y1, x2, y2, palette[color % PALETTE_SIZE]);
}

static int bez_steps(float len) {
    int n = (int)(len * 0.5f);
    return n < 4 ? 4 : n > 200 ? 200 : n;
}
void bezier(int x0, int y0, int cx, int cy, int x1, int y1, int color) {
    PROF("bezier");
    float len = sqrtf((float)((cx-x0)*(cx-x0)+(cy-y0)*(cy-y0)))
              + sqrtf((float)((x1-cx)*(x1-cx)+(y1-cy)*(y1-cy)));
    int n = bez_steps(len);
    float px = (float)x0, py = (float)y0;
    for (int i = 1; i <= n; i++) {
        float t = i / (float)n, mt = 1.0f - t;
        float nx = mt*mt*x0 + 2.0f*mt*t*cx + t*t*x1;
        float ny = mt*mt*y0 + 2.0f*mt*t*cy + t*t*y1;
        line((int)roundf(px), (int)roundf(py), (int)roundf(nx), (int)roundf(ny), color);
        px = nx; py = ny;
    }
}
// Batched pixel (env DE_BATCH_PSET=on). raylib's DrawPixel binds the shapes (white)
// texture, emits a 1px quad, then resets the texture to 0 *per pixel* — that trailing
// rlSetTexture(0) forces a draw-call flush for every single pixel, defeating rlgl's
// vertex batcher (the rlSetTexture churn the fleet survey measured next to rlVertex3f).
// px_emit draws the same 1px quad (same TL,BL,BR,TR winding → byte-identical pixels) but
// leaves the white texture bound, so a run of consecutive psets coalesces into one draw
// call. Order-safe: any other primitive that changes texture/mode flushes our pending
// pixels first, exactly like normal rlgl batching. docs/design/software-canvas.md →
// "Cheaper alternatives".
static inline void px_emit(int x, int y, DeColor c) {
    rlSetTexture(rlGetTextureIdDefault());   // 1x1 white; rlgl no-ops when already bound
    rlBegin(RL_QUADS);
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlTexCoord2f(0, 0); rlVertex2f((float)x,       (float)y);
        rlTexCoord2f(0, 1); rlVertex2f((float)x,       (float)y + 1);
        rlTexCoord2f(1, 1); rlVertex2f((float)x + 1,   (float)y + 1);
        rlTexCoord2f(1, 0); rlVertex2f((float)x + 1,   (float)y);
    rlEnd();
}

void pset(int x, int y, int color) {
    PROF("pset");
    DeColor c = palette[color % PALETTE_SIZE];
    if (sw_canvas_active) { sw_pset(x, y, c); return; }
    if (pset_batch) px_emit(x, y, c);
    else            DrawPixel(x, y, c);
}

// raw 24-bit pixel: 0xRRGGBB straight to the canvas, bypassing the palette. the
// canvas is RGBA, so this is the same cost as pset — it's how a CPU shader paints.
void pset_rgb(int x, int y, int hex) {
    PROF("pset");
    DeColor c = { (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, 255 };
    if (sw_canvas_active) { sw_pset(x, y, c); return; }
    if (pset_batch) px_emit(x, y, c);
    else            DrawPixel(x, y, c);
}

// enable_pget(true) keeps the canvas read-back warm so pget/pget_rgb/touching_color can
// read last frame's pixels; enable_pget(false) stops paying for it. Toggle once in init()
// for a cart that always reads, or around a reading mode to pay only while it's active.
void enable_pget(bool on) { pget_enabled = on; }

static void pget_warn_once(void) {
    if (pget_warned) return;
    pget_warned = true;
    fprintf(stderr, "[pget] read-back is OFF — pget/pget_rgb/touching_color return empty. "
                    "Call enable_pget(true) in init() (or around your reading mode).\n");
}

int pget(int x, int y) {
    if (!pget_enabled)        { pget_warn_once(); return 0; }
    if (!pget_snapshot_valid) return 0;   // enabled but no snapshot yet (first frame, or web)
    // pget(x,y) takes a WORLD coord and reads the screen pixel it landed on, so run it
    // through the camera matrix. exact under translate; approximate under zoom/rotation
    // (it samples the rendered canvas, which is the documented limitation).
    Vector2 s = GetWorldToScreen2D((Vector2){ (float)x, (float)y }, cam);
    int rx = (int)s.x;
    int ry = (int)s.y;
    if (rx < 0 || rx >= de_sw || ry < 0 || ry >= de_sh) return 0;   // in the active canvas?
    // RT data is bottom-up in raylib; flip Y to match draw coords. pget_snapshot is the FULL-size
    // GPU render texture (SCREEN_W×SCREEN_H), so its flip origin stays SCREEN_H, not de_sh.
    DeColor c = GetImageColor(pget_snapshot, rx, SCREEN_H - 1 - ry);
    for (int i = 0; i < PALETTE_SIZE; i++) {
        if (palette[i].r == c.r && palette[i].g == c.g && palette[i].b == c.b) return i;
    }
    return 0;
}

// true-colour read-back: the raw 0xRRGGBB at (x,y), skipping pget's palette match.
// pairs with pset_rgb for feedback shaders (read your own true-colour canvas, write it
// back). returns -1 off-screen so a real black pixel (0x000000) isn't ambiguous.
int pget_rgb(int x, int y) {
    if (!pget_enabled)        { pget_warn_once(); return -1; }
    if (!pget_snapshot_valid) return -1;   // enabled but no snapshot yet (first frame, or web)
    Vector2 s = GetWorldToScreen2D((Vector2){ (float)x, (float)y }, cam);
    int rx = (int)s.x;
    int ry = (int)s.y;
    if (rx < 0 || rx >= de_sw || ry < 0 || ry >= de_sh) return -1;   // in the active canvas?
    DeColor c = GetImageColor(pget_snapshot, rx, SCREEN_H - 1 - ry);   // RT is bottom-up; flip Y (full-size snapshot → SCREEN_H)
    return (c.r << 16) | (c.g << 8) | c.b;
}

int sget(int sx, int sy) {
    // reads the spritesheet directly (not the canvas), so it's a stable data lookup —
    // e.g. paint a level in the sprite editor and read the blocks back here. The
    // sheet is loaded straight from PNG (top-down), so no Y flip like pget needs.
    if (!spritesheet_img.data) return 0;
    if (sx < 0 || sx >= spritesheet_img.width || sy < 0 || sy >= spritesheet_img.height) return 0;
    DeColor c = GetImageColor(spritesheet_img, sx, sy);
    for (int i = 0; i < PALETTE_SIZE; i++) {
        if (palette[i].r == c.r && palette[i].g == c.g && palette[i].b == c.b) return i;
    }
    return 0;
}

// push the current cam to the GPU. if we're mid-draw (inside BeginMode2D), restart
// the 2D mode so a camera()/camera_ex() call takes effect for the rest of the frame.
static void cam_reapply(void) {
    if (cam_active) { EndMode2D(); BeginMode2D(cam); }
}

// finish a smooth_zoom capture: close the offscreen, blit its centred (SCREEN/zoom)
// region onto the canvas scaled to full screen (sharp shader if loaded, else bilinear),
// and resume canvas drawing. Called by camera()/camera_ex() and at draw() end.
static void smooth_composite(void) {
#ifndef PLATFORM_WEB
    if (!smooth_capturing) return;
    if (cam_active) EndMode2D();
    EndTextureMode();                          // close smooth_rt
    BeginTextureMode(canvas);                  // back to the real canvas
    float z  = smooth_zoom_amt;
    float sw = de_sw / z, sh = de_sh / z;                  // captured world region (px @ 1:1) — active canvas
    float sx0 = SMOOTH_W / 2.0f - sw / 2.0f;
    float sy0 = SMOOTH_H / 2.0f - sh / 2.0f;              // centred in the (physical) offscreen
    Rectangle src = { sx0, SMOOTH_H - sy0 - sh, sw, -sh }; // RT is bottom-up → flip (cf. zoom_rect)
    Rectangle dst = { 0, 0, de_sw, de_sh };               // active canvas region
    if (scale_shader_ok) {
        float ts[2] = { (float)SMOOTH_W, (float)SMOOTH_H };
        int   g = 1;                                       // gamma-correct seam (the good one)
        SetShaderValue(scale_shader, loc_scale_texsize, ts, SHADER_UNIFORM_VEC2);
        SetShaderValue(scale_shader, loc_scale_gamma, &g, SHADER_UNIFORM_INT);
        BeginShaderMode(scale_shader);
        DrawTexturePro(smooth_rt.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndShaderMode();
    } else {
        DrawTexturePro(smooth_rt.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
    smooth_capturing = false;
    if (cam_active) BeginMode2D(cam);          // resume normal canvas drawing
#endif
}

#ifdef DE_NO_RAYLIB
// rotate the captured world layer (sw_world_buf) about the screen centre into sw_cbuf, by the
// active camera angle. Nearest-sample inverse map (dest→src). Transparent (alpha 0) world texels
// are skipped, so the cls() background shows in the rotated-out corners — matching the GPU.
static void sw_rot_composite(void) {
    if (!sw_rot_active) return;
    float a  = sw_rot_angle * 0.01745329252f;          // deg→rad; sign matches raylib Camera2D
    float cs = cosf(a), sn = sinf(a);
    float ox = cam.offset.x, oy = cam.offset.y;         // pivot = screen centre (camera_ex pins it there)
    for (int dy = 0; dy < de_sh; dy++) {
        for (int dx = 0; dx < de_sw; dx++) {
            float rx = dx - ox, ry = dy - oy;            // inverse-rotate dest → source (rotate by −a)
            int sx = (int)floorf( cs*rx + sn*ry + ox + 0.5f);
            int sy = (int)floorf(-sn*rx + cs*ry + oy + 0.5f);
            if ((unsigned)sx < (unsigned)de_sw && (unsigned)sy < (unsigned)de_sh) {
                uint32_t p = sw_world_buf[(de_sh - 1 - sy) * fb_w + sx];
                if (p & 0xFF000000u) sw_cbuf[(de_sh - 1 - dy) * fb_w + dx] = p;
            }
        }
    }
    sw_rot_active = false; sw_dst = sw_cbuf;
}
#endif

void camera(int x, int y) {
    if (smooth_capturing) smooth_composite();   // end the smooth world layer first
#ifdef DE_NO_RAYLIB
    if (sw_rot_active) sw_rot_composite();       // end the rotated world layer → HUD now draws un-rotated
#endif
    cam.offset   = (Vector2){ de_sw / 2.0f, de_sh / 2.0f };
    cam.target   = (Vector2){ x + de_sw / 2.0f, y + de_sh / 2.0f };
    cam.zoom     = 1.0f;   // plain camera: camera() always means no zoom / no rotation
    cam.rotation = 0.0f;
    cam_reapply();
}

void camera_ex(int x, int y, float zoom, float angle) {
    if (zoom < 0.01f) zoom = 0.01f;   // guard a degenerate / inverted matrix
    // software canvas handles translation + zoom (axis-aligned scale); only ROTATION makes this cart
    // fall back to the GPU path (Fork-2/C) — rotation breaks the span fast-paths. Sticky: clean
    // switch after one transitional frame. (TODO Option 3: render world 1:1 → GPU-transform at
    // present would keep rotation on the canvas too, but breaks world-then-HUD frames.)
#ifndef DE_NO_RAYLIB
    if (angle != 0.0f) sw_force_gpu = true;
#else
    // no GPU to fall back to (iOS/Switch). Capture the world into sw_world_buf at zoom+translate
    // (sw_w2s ignores rotation), then rotate-composite it about the screen centre at the next
    // camera() reset / present. Renders correctly rotated, fully on the software canvas.
    if (angle != 0.0f) {
        if (!sw_rot_active) {                            // begin the rotated world layer
            for (int i = 0; i < fb_w * fb_h; i++) sw_world_buf[i] = 0;   // transparent
            sw_dst = sw_world_buf; sw_rot_active = true;
        }
        sw_rot_angle = angle;
    } else if (sw_rot_active) {                          // angle 0 ends the rotated layer
        sw_rot_composite();
    }
#endif
    float zd = zoom > 1.0f ? zoom - 1.0f : 1.0f - zoom;
#ifndef PLATFORM_WEB
    // smooth_zoom is a GPU-path device: its EndTextureMode/BeginTextureMode(smooth_rt) dance is
    // INVALID during a software-canvas frame (the sw path never opens a render target — cf.
    // sw_zoom_rect), and the leaked BeginTextureMode(canvas) from smooth_composite then swallows
    // the present, freezing the window (bit sloop: smooth_zoom + speed-zoom under
    // DE_SOFTWARE_CANVAS). The canvas doesn't need it anyway — sw_w2s scales fractional zoom
    // axis-aligned (Option 2, no staircase), so just skip the capture on the sw path.
    if (!sw_canvas_active && smooth_on && smooth_rt_ok && zd > 0.002f) {   // fractional zoom → capture at 1:1
        if (!smooth_capturing) {
            if (cam_active) EndMode2D();
            EndTextureMode();                              // leave the canvas
            BeginTextureMode(smooth_rt);
            ClearBackground(palette[last_cls_color]);   // match the cart's cls() bg, not black
            smooth_capturing = true;
        }
        smooth_zoom_amt = zoom;
        cam.offset   = (Vector2){ SMOOTH_W / 2.0f, SMOOTH_H / 2.0f };
        cam.target   = (Vector2){ x + de_sw / 2.0f, y + de_sh / 2.0f };
        cam.zoom     = 1.0f;                               // world rasterizes at 1:1 (stable)
        cam.rotation = angle;
        BeginMode2D(cam);
        cam_active = true;
        return;
    }
#endif
    cam.offset   = (Vector2){ de_sw / 2.0f, de_sh / 2.0f };
    cam.target   = (Vector2){ x + de_sw / 2.0f, y + de_sh / 2.0f };
    cam.zoom     = zoom;
    cam.rotation = angle;
    cam_reapply();
}

// copy the (sw x sh) canvas region at (sx,sy) into the (dw x dh) rect at (dx,dy),
// scaled (nearest-neighbor, so a zoom shows crisp doubled pixels). Samples the
// frame drawn SO FAR — call it after the content you want to magnify. Used by
// ui.h's loupe; also good for picture-in-picture, minimaps, magnifier toys.
// Snapshots the canvas to a scratch texture first, so reading and writing the
// same target never collide.
// software zoom_rect: nearest-magnify a screen region of the cbuf back into the cbuf — the canvas
// twin of the GPU snapshot+blit. The GPU path's EndTextureMode/BeginTextureMode dance is INVALID
// under the canvas (the frame loop never opened a render target), and corrupts the whole frame; this
// is a pure CPU op. Screen space, no camera (a post-effect, like the GPU version). NB: no snapshot —
// assumes dest doesn't overlap source (the common "magnify a corner elsewhere" use).
static void sw_zoom_rect(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    // dest-pixel-CENTRE nearest sampling (floor((j+0.5)*sh/dh)) — the same GPU POINT-filter
    // convention as sw_blit's scaled path, matching the GPU zoom_rect's DrawTexturePro magnify.
    // Integer magnifications (drawall's 2×) are identical under both formulas.
    for (int j = 0; j < dh; j++) {
        int syy = sy + (int)((j + 0.5f) * sh / dh);
        if ((unsigned)syy >= (unsigned)de_sh) continue;
        const uint32_t *srow = &sw_cbuf[(de_sh - 1 - syy) * fb_w];
        for (int i = 0; i < dw; i++) {
            int sxx = sx + (int)((i + 0.5f) * sw / dw);
            if ((unsigned)sxx >= (unsigned)de_sw) continue;
            sw_plot1(dx + i, dy + j, srow[sxx]);
        }
    }
}

void zoom_rect(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    if (sw_canvas_active) { sw_zoom_rect(sx, sy, sw, sh, dx, dy, dw, dh); return; }   // CPU cbuf magnify (no GL state)
    // Snapshot the frame-so-far into canvas_snap (so we never sample the live
    // target). We're called mid-draw inside BeginTextureMode + BeginMode2D —
    // unwind both, copy, restore.
    bool wascam = cam_active;
    if (wascam) EndMode2D();
    EndTextureMode();
    BeginTextureMode(canvas_snap);                     // -de_sh flips the active canvas upright into snap
    DrawTexturePro(canvas.texture,
                   (Rectangle){ 0, 0, de_sw, -de_sh },
                   (Rectangle){ 0, 0, de_sw, de_sh },   // 1:1 into snap's top-left (no stretch when de < max)
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndTextureMode();
    BeginTextureMode(canvas);                          // resume live canvas (screen-space blit)
    // snap.texture is the FULL-size (SCREEN_H) bottom-up RT; consistent with the engine's full
    // {0,0,W,-H} blit, logical rows [sy,sy+sh] read from texture y = SCREEN_H-sy-sh with -sh flip.
    DrawTexturePro(canvas_snap.texture,
                   (Rectangle){ (float)sx, (float)(SCREEN_H - sy - sh), (float)sw, (float)-sh },
                   (Rectangle){ (float)dx, (float)dy, (float)dw, (float)dh },
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
    if (wascam) BeginMode2D(cam);
}

void clip(int x, int y, int w, int h) {
    if (clip_active) { EndScissorMode(); clip_active = false; }
    if (w <= 0 || h <= 0) return;
    BeginScissorMode(x, y, w, h);
    clip_active = true;
    clip_cx = x; clip_cy = y; clip_cw = w; clip_ch = h;
}

int mget(int cx, int cy) {
    if (cx < 0 || cx >= MAP_W || cy < 0 || cy >= MAP_H) return 0;
    return map_data[cy * MAP_W + cx];
}

void mset(int cx, int cy, int n) {
    if (cx < 0 || cx >= MAP_W || cy < 0 || cy >= MAP_H) return;
    map_data[cy * MAP_W + cx] = (uint8_t)(n & 0xFF);
}

void map_scale(int n) {
    map_scale_factor = (n < 1) ? 1 : n;
}

void map(int cx, int cy, int sx, int sy, int cw, int ch) {
    PROF("map");
    if (spritesheet.width == 0) return;
    int cols = spritesheet.width / CELL_W;
    if (cols < 1) cols = 1;
    int dw = CELL_W * map_scale_factor;   // on-screen size of one cell
    int dh = CELL_H * map_scale_factor;
    for (int yi = 0; yi < ch; yi++) {
        for (int xi = 0; xi < cw; xi++) {
            int v = mget(cx + xi, cy + yi);
            if (v == 0) continue;  // cell 0 = empty (skipped)
            // pull a CELL_W×CELL_H rect from the sheet, blit it scaled to dw×dh (no smoothing)
            int srcx = (v % cols) * CELL_W, srcy = (v / cols) * CELL_H;
            int dstx = sx + xi * dw,        dsty = sy + yi * dh;
            if (sw_canvas_active) {   // same tiled blit as spr/sspr, just onto the cbuf
                sw_blit(srcx, srcy, CELL_W, CELL_H, dstx, dsty, dw, dh, false, false, false); continue;
            }
            Rectangle src = { (float)srcx, (float)srcy, (float)CELL_W, (float)CELL_H };
            Rectangle dst = { (float)dstx, (float)dsty, (float)dw, (float)dh };
            DrawTexturePro(spritesheet, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
        }
    }
}

int rnd(int n) {
    if (n <= 0) return 0;
    return GetRandomValue(0, n - 1);
}

float now(void) {
    return (float)clk();
}

int epoch(void) {
    return (int)time(NULL);     // real-world Unix seconds — persists across runs
}

int sgn(int n) {
    return (n > 0) - (n < 0);
}

int mid(int a, int b, int c) {
    int lo = a < b ? a : b;
    int hi = a > b ? a : b;
    return c < lo ? lo : (c > hi ? hi : c);
}

// ------------------------------------------------------------
// timer — a resettable stopwatch on top of GetTime()
// ------------------------------------------------------------

static double timer_base = 0.0;

float timer(void)       { return (float)(clk() - timer_base); }
void  timer_reset(void) { timer_base = clk(); }

// ------------------------------------------------------------
// math — angles in degrees (0 = right, 90 = down)
// (abs() comes from libc; we only declare it in studio.h)
// ------------------------------------------------------------

int min(int a, int b) { return a < b ? a : b; }
int max(int a, int b) { return a > b ? a : b; }

float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float remap(float v, float a, float b, float c, float d) {
    if (b == a) return c;                 // empty source range — avoid divide-by-zero
    return c + (v - a) * (d - c) / (b - a);
}

float distance(int x1, int y1, int x2, int y2) {
    float ddx = (float)(x2 - x1), ddy = (float)(y2 - y1);
    return sqrtf(ddx*ddx + ddy*ddy);
}

float length(int x, int y) {
    float fx = (float)x, fy = (float)y;
    return sqrtf(fx*fx + fy*fy);
}

float fsqrt(float v) {
    return v <= 0.0f ? 0.0f : sqrtf(v);
}

float angle_to(int x1, int y1, int x2, int y2) {
    return atan2f((float)(y2 - y1), (float)(x2 - x1)) * RAD2DEG;
}

float dx(float steps, float degrees) {
    return steps * cosf(degrees * DEG2RAD);
}

float dy(float steps, float degrees) {
    return steps * sinf(degrees * DEG2RAD);
}

float sin_deg(float degrees) { return sinf(degrees * DEG2RAD); }
float cos_deg(float degrees) { return cosf(degrees * DEG2RAD); }

// ------------------------------------------------------------
// 3D helpers
// ------------------------------------------------------------

V3 rot3(V3 p, float yaw, float pitch) {
    // yaw around Y, then pitch around X — the order every solid-3D cart used
    float x  =  p.x * cos_deg(yaw) + p.z * sin_deg(yaw);
    float z  = -p.x * sin_deg(yaw) + p.z * cos_deg(yaw);
    float y  =  p.y * cos_deg(pitch) - z * sin_deg(pitch);
    float z2 =  p.y * sin_deg(pitch) + z * cos_deg(pitch);
    return (V3){ x, y, z2 };
}

bool project3(V3 p, float focal, float zoom, int *sx, int *sy) {
    if (focal + p.z <= 0.0f) return false;            // behind the camera — skip it
    float f = focal / (focal + p.z);
    if (sx) *sx = de_sw / 2 + (int)(p.x * f * zoom);
    if (sy) *sy = de_sh / 2 + (int)(p.y * f * zoom);
    return true;
}

void zsort(float *key, int *order, int n) {
    for (int i = 0; i < n; i++) order[i] = i;
    // insertion sort, descending by key — far (big key) drawn first
    for (int i = 1; i < n; i++) {
        int oi = order[i];
        float k = key[oi];
        int j = i - 1;
        while (j >= 0 && key[order[j]] < k) { order[j + 1] = order[j]; j--; }
        order[j + 1] = oi;
    }
}

void quadfill(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int color) {
    trifill(x0, y0, x1, y1, x2, y2, color);
    trifill(x0, y0, x2, y2, x3, y3, color);
}

// ------------------------------------------------------------
// geometry helpers — ngon, star, poly, thickline, rrect, gradient
// ------------------------------------------------------------

// ── polygon coverage — one definition for ngon/star/poly fill + outline ──
// Even-odd crossing test from the pixel centre (matches the disc/rrect
// convention). Handles convex AND concave (star points, arbitrary poly);
// fill, dither and outline all read from it, so the outline is always the
// boundary ring of the fill — never the lines-meet-fill disagreement the old
// fan + line() versions had.
// NOTE: this is a SOFTWARE per-pixel fill (in whatever coords the caller passes). Under a
// rotated camera_ex() the world-space pixel grid staircases into a dotted lattice of holes —
// trifill/polyfill/ngonfill/starfill all inherit this. For rotated-in-world shapes use a
// GPU primitive (rectfill / rectfill_rot / tritex). See docs/design/api-notes.md.
static bool poly_inside(float fx, float fy, const int *xy, int n) {
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float yi = xy[i*2+1], yj = xy[j*2+1];
        if ((yi > fy) != (yj > fy)) {
            float xi = xy[i*2], xj = xy[j*2];
            if (fx < (xj - xi) * (fy - yi) / (yj - yi) + xi) in = !in;
        }
    }
    return in;
}
static void poly_bbox(const int *xy, int n, int *minx, int *miny, int *maxx, int *maxy) {
    *minx = *maxx = xy[0]; *miny = *maxy = xy[1];
    for (int i = 1; i < n; i++) {
        int x = xy[i*2], y = xy[i*2+1];
        if (x < *minx) *minx = x; if (x > *maxx) *maxx = x;
        if (y < *miny) *miny = y; if (y > *maxy) *maxy = y;
    }
}
// The software scan only needs to visit pixels that can actually land on screen.
// A primitive whose bbox runs far off-screen (e.g. huge thin triangles) would
// otherwise iterate millions of cells that plot nothing. Intersect the scan box
// with the on-screen region, expressed in the primitive's OWN coordinate space —
// which the camera (translation/zoom/rotation, a GPU Camera2D) shifts. We map the
// four screen corners back through the camera and take their AABB: a conservative
// superset under rotation, so no visible cell is ever dropped and the image stays
// byte-identical; only never-plotted off-screen cells are skipped.
static void poly_clamp_scan(int *x0, int *y0, int *x1, int *y1) {
    // viewport = the CURRENT render target, not always the canvas: under smooth_zoom
    // the world is rasterized into the SMOOTH_W×SMOOTH_H offscreen (camera centred on
    // it), so the visible-world AABB must be mapped from those corners. Using the
    // canvas corners here put the camera-centred shape at the rect's edge and clamped
    // it to one quadrant (the "only a quarter of the car shows under smooth zoom" bug).
    float vw = de_sw, vh = de_sh;
#ifndef PLATFORM_WEB
    if (smooth_capturing) { vw = SMOOTH_W; vh = SMOOTH_H; }
#endif
    // The visible-world box depends ONLY on the camera + viewport, which are constant for
    // a whole frame unless the cart moves the camera mid-frame. Caching it turns the 4
    // GetScreenToWorld2D (matrix inverses) PER FILL into 4 per camera-CHANGE — a big win
    // for carts that issue hundreds of small fills (clampstress). Byte-identical: the
    // signature captures every input to the computation, so a hit returns the exact box a
    // recompute would; any camera/viewport change misses and recomputes.
    static int   c_lox = 0, c_loy = 0, c_hix = 0, c_hiy = 0;
    static bool  c_valid = false;
    static float s_tx = 0, s_ty = 0, s_ox = 0, s_oy = 0, s_rot = 0, s_zoom = 0, s_vw = 0, s_vh = 0;
    int lo_x, lo_y, hi_x, hi_y;
    if (clamp_cache_on && c_valid &&
        cam.target.x == s_tx && cam.target.y == s_ty &&
        cam.offset.x == s_ox && cam.offset.y == s_oy &&
        cam.rotation == s_rot && cam.zoom == s_zoom && vw == s_vw && vh == s_vh) {
        lo_x = c_lox; lo_y = c_loy; hi_x = c_hix; hi_y = c_hiy;   // cache hit — no matrix inverts
    } else {
        Vector2 c0 = GetScreenToWorld2D((Vector2){ 0,  0  }, cam);
        Vector2 c1 = GetScreenToWorld2D((Vector2){ vw, 0  }, cam);
        Vector2 c2 = GetScreenToWorld2D((Vector2){ 0,  vh }, cam);
        Vector2 c3 = GetScreenToWorld2D((Vector2){ vw, vh }, cam);
        lo_x = (int)floorf(fminf(fminf(c0.x, c1.x), fminf(c2.x, c3.x)));
        lo_y = (int)floorf(fminf(fminf(c0.y, c1.y), fminf(c2.y, c3.y)));
        hi_x = (int)ceilf (fmaxf(fmaxf(c0.x, c1.x), fmaxf(c2.x, c3.x)));
        hi_y = (int)ceilf (fmaxf(fmaxf(c0.y, c1.y), fmaxf(c2.y, c3.y)));
        c_lox = lo_x; c_loy = lo_y; c_hix = hi_x; c_hiy = hi_y; c_valid = true;
        s_tx = cam.target.x; s_ty = cam.target.y; s_ox = cam.offset.x; s_oy = cam.offset.y;
        s_rot = cam.rotation; s_zoom = cam.zoom; s_vw = vw; s_vh = vh;
    }
    if (*x0 < lo_x) *x0 = lo_x;  if (*y0 < lo_y) *y0 = lo_y;
    if (*x1 > hi_x) *x1 = hi_x;  if (*y1 > hi_y) *y1 = hi_y;
}
// ── A/B SWITCH (temporary, 2026-06) ──────────────────────────────────────────
// The original per-pixel fill, kept beside the new scanline span fill below so we
// can flip back instantly while the new path soaks. Set poly_fill_fast = false to
// route EVERY polygon fill through this exact old code (the reference behaviour).
// poly_fill_fast lives up by the other globals (env DE_POLY_FILL=legacy flips it at
// startup). TODO: once the span fill is trusted, delete poly_fill_cov_legacy + the flag.
static void poly_fill_cov_legacy(const int *xy, int n, int color) {
    if (n < 3) return;
    int x0, y0, x1, y1; poly_bbox(xy, n, &x0, &y0, &x1, &y1);
    poly_clamp_scan(&x0, &y0, &x1, &y1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (poly_inside(x + 0.5f, y + 0.5f, xy, n)) plot_pat(x, y, color);
}

// Scanline span fill: the SAME even-odd coverage as poly_inside (pixel-centre
// x+0.5 / y+0.5), but found per ROW as sorted edge crossings instead of tested per
// pixel — so the per-pixel point-in-polygon cost is gone, and a solid run becomes ONE
// DrawRectangle instead of N DrawPixel (the per-pixel rlVertex3f spam). Byte-identical
// to the old per-pixel scan, by construction:
//   • y+0.5 never equals an integer vertex y (verts are int), so the line never grazes
//     a vertex: crossings are clean and always come in even-count pairs;
//   • a pixel is inside iff its centre x+0.5 lies in the half-open [cross[t], cross[t+1])
//     of an even-odd pair (poly_inside's strict `<` makes it left-closed/right-open),
//     bounded by THAT pair's own crossings so a span can never bleed across a sub-pixel
//     gap into its neighbour — matches poly_inside pixel-for-pixel (verified exhaustively);
//   • the DrawRectangle fast path is gated on an axis-aligned camera (rotation 0,
//     zoom 1: a run of 1×1 world quads tiles to exactly the same target pixels as one
//     w×1 quad — true under pure translation, NOT under zoom/rotation) AND a solid
//     fill. A rotated/zoomed camera or an active fillp() dither falls back to per-pixel
//     plot_pat, preserving the documented rotated-fill staircase and the dither lattice
//     exactly. (Both paths still skip the per-pixel poly_inside, so both get faster.)
static void poly_fill_cov(const int *xy, int n, int color) {
    if (!poly_fill_fast) { poly_fill_cov_legacy(xy, n, color); return; }
    if (n < 3) return;
    int x0, y0, x1, y1; poly_bbox(xy, n, &x0, &y0, &x1, &y1);
    poly_clamp_scan(&x0, &y0, &x1, &y1);
    if (x1 < x0 || y1 < y0) return;
    enum { MAXCROSS = 512 };
    if (n > MAXCROSS) {                         // pathological vertex count: exact per-pixel fallback
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                if (poly_inside(x + 0.5f, y + 0.5f, xy, n)) plot_pat(x, y, color);
        return;
    }
    bool fast = (cam.rotation == 0.0f) && (cam.zoom == 1.0f) && !fp_on;
    DeColor col = palette[color % PALETTE_SIZE];
    float cross[MAXCROSS];
    for (int y = y0; y <= y1; y++) {
        float yc = y + 0.5f;
        int m = 0;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            float yi = xy[i*2+1], yj = xy[j*2+1];
            if ((yi > yc) != (yj > yc)) {
                float xi = xy[i*2], xj = xy[j*2];
                cross[m++] = (xj - xi) * (yc - yi) / (yj - yi) + xi;
            }
        }
        for (int a = 1; a < m; a++) {           // insertion sort — m is tiny
            float v = cross[a]; int b = a - 1;
            while (b >= 0 && cross[b] > v) { cross[b+1] = cross[b]; b--; }
            cross[b+1] = v;
        }
        for (int t = 0; t + 1 < m; t += 2) {
            // integer pixels whose CENTRE x+0.5 falls strictly inside this pair's
            // (cross[t], cross[t+1]) — the exact even-odd interval. Bounded by THIS
            // pair's own crossings, so it can never spill into an adjacent span across
            // a sub-pixel gap (the star-notch bug); matches poly_inside pixel-for-pixel.
            // poly_inside's strict `<` makes the interval half-open [cL, cR): a pixel
            // is inside iff cL <= x+0.5 < cR. (Matches it pixel-for-pixel — verified
            // exhaustively; the naive floor+1 differs when a crossing is a half-integer.)
            int a = (int)ceilf(cross[t]   - 0.5f);        // first centre >= cross[t]
            int b = (int)ceilf(cross[t+1] - 0.5f) - 1;    // last  centre <  cross[t+1]
            if (a < x0) a = x0;  if (b > x1) b = x1;
            if (a > b) continue;
            if (fast) sw_span(a, y, b - a + 1, col);
            else for (int x = a; x <= b; x++) plot_pat(x, y, color);
        }
    }
}
// poly/ngon/star OUTLINE = boundary ring of the even-odd fill. Same O(r²)-bbox-scan /
// O(perimeter)-result seam as `circ` (see its note) — a span version would emit the
// per-row span endpoints + cap pixels. PARKED, same low-leverage rationale.
static void poly_stroke_cov(const int *xy, int n, int color) {
    if (n < 3) return;
    int x0, y0, x1, y1; poly_bbox(xy, n, &x0, &y0, &x1, &y1);
    poly_clamp_scan(&x0, &y0, &x1, &y1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (poly_inside(x+0.5f,y+0.5f,xy,n) &&
                (!poly_inside(x-0.5f,y+0.5f,xy,n) || !poly_inside(x+1.5f,y+0.5f,xy,n) ||
                 !poly_inside(x+0.5f,y-0.5f,xy,n) || !poly_inside(x+0.5f,y+1.5f,xy,n)))
                pset(x, y, color);
}
// build the vertex ring for an n-gon / star into pts (capacity >= 2*count)
static int ngon_verts(int x, int y, int r, int sides, float rot, int *pts) {
    if (sides > 64) sides = 64;
    float step = 360.0f / sides;
    for (int i = 0; i < sides; i++) {
        float a = rot + step * i;
        pts[i*2] = x + (int)roundf(cos_deg(a) * r);
        pts[i*2+1] = y + (int)roundf(sin_deg(a) * r);
    }
    return sides;
}
static int star_verts(int x, int y, int r_out, int r_in, int points, float rot, int *pts) {
    if (points > 64) points = 64;
    int n = points * 2; float step = 180.0f / points;
    for (int i = 0; i < n; i++) {
        float a = rot + step * i;
        int rad = (i & 1) ? r_in : r_out;
        pts[i*2] = x + (int)roundf(cos_deg(a) * rad);
        pts[i*2+1] = y + (int)roundf(sin_deg(a) * rad);
    }
    return n;
}

void ngon(int x, int y, int r, int sides, float rot, int color) {
    PROF("ngon");
    if (sides < 3) return;
    int pts[128]; int n = ngon_verts(x, y, r, sides, rot, pts);
    poly_stroke_cov(pts, n, color);
}

void ngonfill(int x, int y, int r, int sides, float rot, int color) {
    PROF("ngonfill");
    if (sides < 3) return;
    int pts[128]; int n = ngon_verts(x, y, r, sides, rot, pts);
    poly_fill_cov(pts, n, color);
}

void star(int x, int y, int r_out, int r_in, int points, float rot, int color) {
    PROF("star");
    if (points < 2) return;
    int pts[256]; int n = star_verts(x, y, r_out, r_in, points, rot, pts);
    poly_stroke_cov(pts, n, color);
}

void starfill(int x, int y, int r_out, int r_in, int points, float rot, int color) {
    PROF("starfill");
    if (points < 2) return;
    int pts[256]; int n = star_verts(x, y, r_out, r_in, points, rot, pts);
    poly_fill_cov(pts, n, color);
}

void poly(int *xy, int n, int color) {
    PROF("poly");
    if (!xy || n < 2) return;
    if (n == 2) { line(xy[0], xy[1], xy[2], xy[3], color); return; }
    poly_stroke_cov(xy, n, color);
}

void polyfill(int *xy, int n, int color) {
    PROF("polyfill");
    if (!xy || n < 3) return;
    poly_fill_cov(xy, n, color);
}

// capsule coverage: a pixel is in iff its centre is within w/2 of the segment.
// One definition (clamped projection gives the round caps for free) — no quad+caps
// seam, so the body and caps can't leave a 1px crack. Shared by fill + outline.
static bool capsule_inside(int px, int py, int x1, int y1, float dx, float dy,
                           float len2, float hw2) {
    float fx = px + 0.5f - x1, fy = py + 0.5f - y1;
    float t = len2 > 0.0f ? (fx*dx + fy*dy) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;       // clamp → capsule, not infinite line
    float qx = fx - t*dx, qy = fy - t*dy;
    return qx*qx + qy*qy <= hw2;
}
static void thick_draw(int x1, int y1, int x2, int y2, int w, int color, bool stroke_only) {
    if (w <= 1) { line(x1, y1, x2, y2, color); return; }
    float hw = w * 0.5f, dx = (float)(x2 - x1), dy = (float)(y2 - y1);
    float len2 = dx*dx + dy*dy, hw2 = hw*hw;
    int r = (int)hw + 1;
    int x0 = (x1 < x2 ? x1 : x2) - r, x9 = (x1 > x2 ? x1 : x2) + r;
    int y0 = (y1 < y2 ? y1 : y2) - r, y9 = (y1 > y2 ? y1 : y2) + r;
    for (int py = y0; py <= y9; py++)
        for (int px = x0; px <= x9; px++) {
            if (!capsule_inside(px,py,x1,y1,dx,dy,len2,hw2)) continue;
            if (stroke_only &&                                  // skip interior pixels
                capsule_inside(px-1,py,x1,y1,dx,dy,len2,hw2) &&
                capsule_inside(px+1,py,x1,y1,dx,dy,len2,hw2) &&
                capsule_inside(px,py-1,x1,y1,dx,dy,len2,hw2) &&
                capsule_inside(px,py+1,x1,y1,dx,dy,len2,hw2)) continue;
            if (stroke_only) pset(px, py, color); else plot_pat(px, py, color);
        }
}
void thickline(int x1, int y1, int x2, int y2, int w, int color) {
    PROF("thickline");
    thick_draw(x1, y1, x2, y2, w, color, false);
}
// outline of a thick line — boundary ring of the capsule (hugs the fill exactly).
void thicklineoutline(int x1, int y1, int x2, int y2, int w, int color) {
    PROF("thicklineoutline");
    thick_draw(x1, y1, x2, y2, w, color, true);
}

void rrect(int x, int y, int w, int h, int r, int color) {
    PROF("rrect");
    if (r <= 0) { rect(x, y, w, h, color); return; }
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    // outline = pixels inside that have at least one outside 4-neighbour
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++)
            if (rrect_inside(px,py,x,y,w,h,r) &&
                (!rrect_inside(px-1,py,x,y,w,h,r) || !rrect_inside(px+1,py,x,y,w,h,r) ||
                 !rrect_inside(px,py-1,x,y,w,h,r) || !rrect_inside(px,py+1,x,y,w,h,r)))
                pset(px, py, color);
}

void rrectfill(int x, int y, int w, int h, int r, int color) {
    PROF("rrectfill");
    if (r <= 0) { rectfill(x, y, w, h, color); return; }
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    // fill = all pixels inside; plot_pat handles both solid and fillp dither
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++)
            if (rrect_inside(px, py, x, y, w, h, r)) plot_pat(px, py, color);
}

// dithered gradient: blend c_top→c_bot (or c_left→c_right) using fillp() checker.
// at t=0 solid c_a, at t=1 solid c_b, in between a dithered mix — stays in palette.
static void gradient_band(int bx, int by, int bw, int bh, int ca, int cb, float t) {
    if (t <= 0.0f)      { rectfill(bx, by, bw, bh, ca); return; }
    if (t >= 1.0f)      { rectfill(bx, by, bw, bh, cb); return; }
    // encode t as a 4×4 dither threshold — scale 0..1 to 0..16 on-bits
    static const int thresholds[17] = {
        0x0000,0x8000,0x8020,0xA020,0xA0A0,0xA4A0,0xA4A4,0xA5A4,
        0xA5A5,0xB5A5,0xB5B5,0xF5B5,0xF5F5,0xFDF5,0xFDFD,0xFFfd,0xFFFF
    };
    int bits = (int)(t * 16.0f + 0.5f);
    if (bits < 0) bits = 0; if (bits > 16) bits = 16;
    int pat = thresholds[bits];
    fillp(pat, cb);
    rectfill(bx, by, bw, bh, ca);
    fillp_reset();
}

static const int BAYER4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 },
};
void gradient(int x, int y, int w, int h, int c_a, int c_b, float angle_deg) {
    PROF("gradient");
    if (w <= 0 || h <= 0) return;
    float ca = cosf(angle_deg * 3.14159265f / 180.0f);
    float sa = sinf(angle_deg * 3.14159265f / 180.0f);
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float half_ext = fabsf(ca) * w * 0.5f + fabsf(sa) * h * 0.5f;
    if (half_ext < 0.5f) half_ext = 0.5f;
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            float proj = ca * (px + 0.5f - cx) + sa * (py + 0.5f - cy);
            float t = (proj + half_ext) / (2.0f * half_ext);
            if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
            float thr = (BAYER4[py & 3][px & 3] + 0.5f) / 16.0f;
            pset(px, py, t > thr ? c_b : c_a);
        }
    }
}
void vgradient(int x, int y, int w, int h, int c_top, int c_bot) {
    PROF("vgradient");
    if (h <= 0 || w <= 0) return;
    for (int row = 0; row < h; row++)
        gradient_band(x, y+row, w, 1, c_top, c_bot, (float)row / (h - 1 > 0 ? h-1 : 1));
}

void hgradient(int x, int y, int w, int h, int c_left, int c_right) {
    PROF("hgradient");
    if (h <= 0 || w <= 0) return;
    for (int col = 0; col < w; col++)
        gradient_band(x+col, y, 1, h, c_left, c_right, (float)col / (w - 1 > 0 ? w-1 : 1));
}

// ------------------------------------------------------------
// collision
// ------------------------------------------------------------

bool boxes_touch(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

bool point_in_box(int px, int py, int bx, int by, int bw, int bh) {
    return px >= bx && px < bx + bw && py >= by && py < by + bh;
}

bool circles_touch(int ax, int ay, int ar, int bx, int by, int br) {
    float ddx = (float)(ax - bx), ddy = (float)(ay - by);
    float r  = (float)(ar + br);
    return ddx*ddx + ddy*ddy <= r*r;
}

bool near(int ax, int ay, int bx, int by, int d) {
    float ddx = (float)(ax - bx), ddy = (float)(ay - by);
    return ddx*ddx + ddy*ddy <= (float)d * (float)d;
}

bool touching_map(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    int sw = CELL_W * map_scale_factor, sh = CELL_H * map_scale_factor;
    int cx0 = x / sw, cx1 = (x + w - 1) / sw;
    int cy0 = y / sh, cy1 = (y + h - 1) / sh;
    for (int cy = cy0; cy <= cy1; cy++)
        for (int cx = cx0; cx <= cx1; cx++)
            if (mget(cx, cy) != 0) return true;
    return false;
}

int tile_at(int px, int py) {
    return mget(px / (CELL_W * map_scale_factor), py / (CELL_H * map_scale_factor));
}

bool touching_color(int x, int y, int w, int h, int color) {
    int target = color % PALETTE_SIZE;
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++)
            if (pget(px, py) == target) return true;
    return false;
}

// ------------------------------------------------------------
// animation
// ------------------------------------------------------------

int anim(int n_frames, float fps, float phase) {
    if (n_frames <= 0) return 0;
    int f = (int)(now() * fps + phase * n_frames) % n_frames;
    if (f < 0) f += n_frames;
    return f;
}

bool blink(int period) {
    if (period < 1) return true;
    return (frame_count / period) % 2 == 0;
}

// ------------------------------------------------------------
// strings — printf into a small ring of reusable buffers
// ------------------------------------------------------------

const char *str(const char *fmt, ...) {
    #define STR_BUFS 8
    #define STR_LEN  128
    static char bufs[STR_BUFS][STR_LEN];
    static int  which = 0;
    char *b = bufs[which];
    which = (which + 1) % STR_BUFS;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, STR_LEN, fmt, ap);
    va_end(ap);
    return b;
}

// ------------------------------------------------------------
// camera helpers
// ------------------------------------------------------------

void follow(int tx, int ty, int world_w, int world_h) {
    int cx = (int)fmaxf(0.0f, fminf((float)(tx - de_sw / 2), (float)(world_w - de_sw)));
    int cy = (int)fmaxf(0.0f, fminf((float)(ty - de_sh / 2), (float)(world_h - de_sh)));
    camera(cx, cy);
}

// ------------------------------------------------------------
// persistence
// ------------------------------------------------------------

static int  sav_data[64]  = {0};
static bool sav_loaded    = false;

static void sav_ensure(void) {
    if (sav_loaded) return;
    FILE *f = fopen(save_path("cart.sav"), "rb");
    if (f) { fread(sav_data, sizeof(int), 64, f); fclose(f); }
    sav_loaded = true;
}

void save(int slot, int value) {
    if (slot < 0 || slot >= 64) return;
    sav_ensure();
    sav_data[slot] = value;
    FILE *f = fopen(save_path("cart.sav"), "wb");
    if (f) { fwrite(sav_data, sizeof(int), 64, f); fclose(f); }
}

int load(int slot) {
    if (slot < 0 || slot >= 64) return 0;
    sav_ensure();
    return sav_data[slot];
}

// Engine-owned scratch state for the cart: one zero-initialised block that lives for
// the whole process. Under the live (libtcc) backend the host owns this memory, so it
// SURVIVES a hot reload — unlike a cart global, which is wiped when the module reloads.
// Grows on demand; never shrinks. Bytes added by a grow are zeroed.
static unsigned char *de_state_mem = NULL;
static int            de_state_cap = 0;
void *de_state(int bytes) {
    if (bytes > de_state_cap) {
        de_state_mem = realloc(de_state_mem, (size_t)bytes);
        memset(de_state_mem + de_state_cap, 0, (size_t)(bytes - de_state_cap));
        de_state_cap = bytes;
    }
    return de_state_mem;
}

// EXPERIMENTAL (see de_data_path_v): the --data <file> path, or $DE_DATA, or NULL.
const char *de_data_path(void) { return de_data_path_v ? de_data_path_v : getenv("DE_DATA"); }

// EXPERIMENTAL: path of a file dropped on the window this frame (drag & drop), else NULL.
const char *de_dropped_file(void) { return de_drop_valid ? de_drop_buf : NULL; }

// EXPERIMENTAL: reveal a file/folder in the OS file manager (so you can find the data dir).
void de_open_path(const char *path) {
    if (!path || !*path) return;
#ifdef DE_NO_RAYLIB
    (void)path;   // no shell-out on portable targets (iOS: system() is unavailable); a
                  // host-specific open (UIApplication.openURL) would live in the backend
#else
    char cmd[1100];
#if defined(__APPLE__)
    snprintf(cmd, sizeof cmd, "open \"%s\"", path);
#elif defined(_WIN32)
    snprintf(cmd, sizeof cmd, "explorer \"%s\"", path);
#else
    snprintf(cmd, sizeof cmd, "xdg-open \"%s\"", path);
#endif
    int rc = system(cmd); (void)rc;
#endif
}

void save_bytes(const void *data, int len) {
    if (len <= 0) return;
    FILE *f = fopen(save_path("cart.blob"), "wb");
    if (f) { fwrite(data, 1, len, f); fclose(f); }
}
int load_bytes(void *out, int max) {
    FILE *f = fopen(save_path("cart.blob"), "rb");
    if (!f) return 0;
    int n = (int)fread(out, 1, max, f);
    fclose(f);
    return n;
}

// named persistence — a small key→int table in cart.kv, parallel to the numbered slots above
#define KV_MAX     64
#define KV_KEYLEN  24
static struct { char key[KV_KEYLEN]; int value; } kv_data[KV_MAX];
static int  kv_count  = 0;
static bool kv_loaded = false;

static void kv_ensure(void) {
    if (kv_loaded) return;
    kv_loaded = true;
    FILE *f = fopen(save_path("cart.kv"), "rb");
    if (!f) return;
    int n = 0;
    if (fread(&n, sizeof(int), 1, f) == 1 && n >= 0 && n <= KV_MAX) {
        kv_count = (int)fread(kv_data, sizeof(kv_data[0]), n, f);
    }
    fclose(f);
}

static int kv_find(const char *key) {
    for (int i = 0; i < kv_count; i++)
        if (strncmp(kv_data[i].key, key, KV_KEYLEN) == 0) return i;
    return -1;
}

void save_int(const char *key, int value) {
    if (!key || !key[0]) return;
    kv_ensure();
    int i = kv_find(key);
    if (i < 0) {
        if (kv_count >= KV_MAX) return;   // table full — silently drop, like an out-of-range slot
        i = kv_count++;
        strncpy(kv_data[i].key, key, KV_KEYLEN - 1);
        kv_data[i].key[KV_KEYLEN - 1] = '\0';
    }
    kv_data[i].value = value;
    FILE *f = fopen(save_path("cart.kv"), "wb");
    if (f) {
        fwrite(&kv_count, sizeof(int), 1, f);
        fwrite(kv_data, sizeof(kv_data[0]), kv_count, f);
        fclose(f);
    }
}

int load_int(const char *key, int def) {
    if (!key || !key[0]) return def;
    kv_ensure();
    int i = kv_find(key);
    return i < 0 ? def : kv_data[i].value;
}

// ------------------------------------------------------------
// frame timing, input, palette, fade/shake, extra drawing
// ------------------------------------------------------------

float dt(void) { return frame_dt; }
int   fps(void) { return GetFPS(); }

const char *text_input(void) { return text_buf; }
bool key(int k)  { key_claim(k); return inp_down(k); }
bool keyp(int k) { key_claim(k); return inp_pressed(k); }
bool keyr(int k) { key_claim(k); return inp_released(k); }

void pal(int c0, int c1)  { if (c0 >= 0 && c0 < PALETTE_SIZE && c1 >= 0 && c1 < PALETTE_SIZE) { palette[c0] = base_palette[c1]; pal_recompute(); } }
void pal_reset(void)      { for (int i = 0; i < PALETTE_SIZE; i++) palette[i] = base_palette[i]; pal_recompute(); }

// ── cart-switch: video state reset (the video twin of the sound context, ADR-0027) ──
// Reset the set-and-hold VIDEO state to boot defaults so an outgoing cart's
// pal()/fillp()/font()/camera() can't bleed into the next cart. (clip already resets
// at frame-end; this engine has no palt.) de_switch_cart runs during update() — OUTSIDE
// the draw/GL context — so this must touch state only: pal_reset/fillp_reset/font just
// write arrays or flags (no GL), and camera(0,0)'s cam_reapply() no-ops while
// cam_active is false. Reset-only (not record+replay like sound): every cart re-sets its
// video modes in draw(), so a clean slate is enough; see share-panel.md.
static void de_gfx_reset(void) {
    pal_reset();            // palette remap → identity
    fillp_reset();          // fill pattern off
    fillp_anchor(0, 0);     // fill lattice origin
    font(FONT_NORMAL);      // active font → default
    camera(0, 0);           // camera offset/zoom/rotation → identity
    sw_force_gpu = false;   // clear the sticky camera_ex-rotation GPU fallback
}

// swap the active sprite sheet to context `ctx`'s (per-cart sheets, multi-cart bundles).
// Sheets are pre-loaded at init, so this is a cheap struct copy — safe from update()/
// outside GL. No-op unless the bundle baked multiple sheets (SPRITES_MULTI, build-app.js).
static void de_sheet_select(int ctx) {
#ifdef SPRITES_MULTI
    if (ctx < 0 || ctx >= SPRITES_MULTI || sheet_imgs[ctx].width <= 0) return;
    spritesheet_img = sheet_imgs[ctx];
#ifndef DE_NO_RAYLIB
    spritesheet = sheet_texs[ctx];
#else
    spritesheet.width  = sheet_imgs[ctx].width;   // SW path: id stays 0, guards read .width
    spritesheet.height = sheet_imgs[ctx].height;
#endif
#else
    (void)ctx;
#endif
}

// the public UMBRELLA cart switch: swaps the whole cart world for context `ctx` —
// sound (de_sound_switch_cart, sound.h) + video set-and-hold reset (de_gfx_reset) +
// the per-cart sprite sheet (de_sheet_select). The dispatcher shim calls only this.
void de_switch_cart(int ctx) {
    de_sound_switch_cart(ctx);
    de_gfx_reset();
    de_sheet_select(ctx);
}

// EXPERIMENTAL (palette probe — see docs/design/palette-and-color.md). Write an
// arbitrary 0xRRGGBB into LIVE palette slot i: primitives/text use it immediately,
// and the pal() shader recolors existing sprite art to match (base_palette stays
// the texel-matching key, exactly like pal()). pal_reset() restores the shipped
// palette. Caveat: pal(c0,c1) under a custom palette injects the SHIPPED c1 color.
// Resolves into a real palette_set() (palette-and-color.md Layer 2) or gets deleted.
void palette_hex(int i, int hex) {
    if (i < 0 || i >= PALETTE_SIZE) return;
    palette[i] = (DeColor){ (unsigned char)((hex >> 16) & 0xFF),
                          (unsigned char)((hex >> 8)  & 0xFF),
                          (unsigned char)( hex        & 0xFF), 255 };
    pal_recompute();
}
void fade(float a)        { fade_amt  = a < 0 ? 0 : (a > 1 ? 1 : a); }
void shake(float a)       { if (a > shake_amt) shake_amt = a; }

int print_scaled(const char *t, int x, int y, int color, int scale) {
    PROF("print_scaled");
    if (scale < 1) scale = 1;
    if (sw_canvas_active || cpu_raster_enabled) {   // SW/CPU scaled-glyph blit (deg 0) — else GPU text is lost on the canvas
        if (scale != 1) { de_cpu_print_rot(t, x, y, 0.0f, scale, color, 0); return x + text_width(t) * scale; }
        return sw_print(t, x, y, palette[color % PALETTE_SIZE]);
    }
    float sz = cur_font_size() * scale;
    DrawTextEx(cur_font(), t, (Vector2){ (float)x, (float)y },
               sz, 0, palette[color % PALETTE_SIZE]);
    return x + (int)MeasureTextEx(cur_font(), t, sz, 0).x;
}

void ovalfill(int cx, int cy, int rx, int ry, int color) {
    PROF("ovalfill");
    if (rx < 0) rx = -rx; if (ry < 0) ry = -ry;
    if (rx == 0 || ry == 0) return;
    // SPAN fast path — per row, hx = rx·√(1-(dy/ry)²) → one DrawRectangle (see circfill).
    if (disc_fill_fast && cam.rotation == 0.0f && !fp_on) {
        int x0 = cx - rx, y0 = cy - ry, x1 = cx + rx, y1 = cy + ry;
        poly_clamp_scan(&x0, &y0, &x1, &y1);
        DeColor col = palette[color % PALETTE_SIZE];
        for (int y = y0; y <= y1; y++) {
            float dyn = (y + 0.5f - cy) / (float)ry;
            float t = 1.0f - dyn * dyn;
            if (t < 0) continue;
            float hx = rx * sqrtf(t);
            int a = (int)floorf(cx - hx - 0.5f);
            int b = (int)ceilf (cx + hx - 0.5f);
            while (a <= b && !ellipse_inside(a, y, cx, cy, rx, ry)) a++;
            while (b >= a && !ellipse_inside(b, y, cx, cy, rx, ry)) b--;
            if (a < x0) a = x0;  if (b > x1) b = x1;
            if (a <= b) sw_span(a, y, b - a + 1, col);
        }
    } else {
        for (int y = cy - ry; y <= cy + ry; y++)
            for (int x = cx - rx; x <= cx + rx; x++)
                if (ellipse_inside(x, y, cx, cy, rx, ry)) plot_pat(x, y, color);
    }
}

void oval(int cx, int cy, int rx, int ry, int color) {
    PROF("oval");
    // Same O(perimeter)-outline seam as `circ` (see its note); PARKED, low-leverage.
    if (rx < 0) rx = -rx; if (ry < 0) ry = -ry;
    if (rx == 0 || ry == 0) return;
    for (int y = cy - ry; y <= cy + ry; y++)
        for (int x = cx - rx; x <= cx + rx; x++)
            if (ellipse_inside(x,y,cx,cy,rx,ry) &&
                (!ellipse_inside(x-1,y,cx,cy,rx,ry) || !ellipse_inside(x+1,y,cx,cy,rx,ry) ||
                 !ellipse_inside(x,y-1,cx,cy,rx,ry) || !ellipse_inside(x,y+1,cx,cy,rx,ry)))
                pset(x, y, color);
}

// ------------------------------------------------------------
// noise — smooth value noise, range 0..1
// ------------------------------------------------------------

static unsigned int noise_hash(int x) {
    unsigned int u = (unsigned int)x;
    u = ((u >> 16) ^ u) * 0x45d9f3b;
    u = ((u >> 16) ^ u) * 0x45d9f3b;
    u = (u >> 16) ^ u;
    return u;
}

static float noise_val(int ix, int iy, int iz) {
    return (float)(noise_hash(ix + noise_hash(iy + noise_hash(iz))) & 0xFFFF) / 65535.0f;
}

static float smooth(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

float noise(float x) {
    int   ix = (int)floorf(x);
    float fx = smooth(x - (float)ix);
    return lerpf(noise_val(ix, 0, 0), noise_val(ix + 1, 0, 0), fx);
}

float noise2(float x, float y) {
    int   ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = smooth(x - (float)ix), fy = smooth(y - (float)iy);
    return lerpf(
        lerpf(noise_val(ix, iy, 0), noise_val(ix+1, iy,   0), fx),
        lerpf(noise_val(ix, iy+1, 0), noise_val(ix+1, iy+1, 0), fx),
        fy);
}

float noise3(float x, float y, float z) {
    int   ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z);
    float fx = smooth(x - (float)ix), fy = smooth(y - (float)iy), fz = smooth(z - (float)iz);
    float v000 = noise_val(ix,   iy,   iz),   v100 = noise_val(ix+1, iy,   iz);
    float v010 = noise_val(ix,   iy+1, iz),   v110 = noise_val(ix+1, iy+1, iz);
    float v001 = noise_val(ix,   iy,   iz+1), v101 = noise_val(ix+1, iy,   iz+1);
    float v011 = noise_val(ix,   iy+1, iz+1), v111 = noise_val(ix+1, iy+1, iz+1);
    return lerpf(
        lerpf(lerpf(v000, v100, fx), lerpf(v010, v110, fx), fy),
        lerpf(lerpf(v001, v101, fx), lerpf(v011, v111, fx), fy),
        fz);
}

// ------------------------------------------------------------
// easing
// ------------------------------------------------------------

float ease_in(float t)     { return t * t; }
float ease_out(float t)    { float u = 1.0f - t; return 1.0f - u * u; }
float ease_in_out(float t) { return t * t * (3.0f - 2.0f * t); }
float ease_back(float t)   { const float s = 1.70158f; float u = t - 1.0f; return 1.0f + (s + 1.0f) * u * u * u + s * u * u; }

// ------------------------------------------------------------
// random helpers
// ------------------------------------------------------------

int   rnd_between(int lo, int hi)            { return lo + rnd(hi - lo); }
float rnd_float(void)                        { return (float)rand() / ((float)RAND_MAX + 1.0f); }
float rnd_float_between(float lo, float hi)  { return lo + rnd_float() * (hi - lo); }

// ------------------------------------------------------------
// frame counter
// ------------------------------------------------------------

int frame(void) { return frame_count; }

// ------------------------------------------------------------
// print alignment
// ------------------------------------------------------------

int print_centered(const char *text, int x, int y, int color) {
    PROF("print_centered");
    return print(text, x - text_width(text) / 2, y, color);
}

int print_right(const char *text, int right_x, int y, int color) {
    PROF("print_right");
    return print(text, right_x - text_width(text), y, color);
}

