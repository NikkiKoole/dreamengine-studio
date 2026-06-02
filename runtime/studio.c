#define STUDIO_INTERNAL          // suppress studio.h's KEY_* macros — raylib.h provides those names
#include "studio.h"
#include "raylib.h"
#include "rlgl.h"      // immediate-mode triangles for tritex()
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
#include "dos_8x8_font.h"
#include "font4x6_data.h"
#include "font3x5_data.h"
#include "sprites_data.h"
#include "map_data.h"
#include "sound.h"

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
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
static long cart_mtime = 0;             // its mtime at load time

// Host-owned persistent cart state. Carts grab a zero-initialised block here
// (`extern void *de_state(int);  #define ST ((MyState*)de_state(sizeof(MyState)))`)
// and the host owns the memory, so it SURVIVES a hot reload — unlike a cart global,
// which lives in the libtcc module and is wiped when the module is replaced.
static unsigned char *de_state_mem = NULL;
static int            de_state_cap = 0;
static void *de_state(int bytes) {
    if (bytes > de_state_cap) {
        de_state_mem = realloc(de_state_mem, (size_t)bytes);
        memset(de_state_mem + de_state_cap, 0, (size_t)(bytes - de_state_cap));
        de_state_cap = bytes;
    }
    return de_state_mem;
}

static long file_mtime(const char *p) { struct stat st; return stat(p, &st) == 0 ? (long)st.st_mtime : 0; }

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
    DE_TCC_API_SYMBOLS(X)
    #undef X
    tcc_add_symbol(s, "de_state", (const void *)de_state);   // persistent-state hook
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
static void cart_reload_if_changed(void) {
    if (!cart_path_buf[0]) return;
    long m = file_mtime(cart_path_buf);
    if (!m || m == cart_mtime) return;
    char path[1024]; snprintf(path, sizeof path, "%s", cart_path_buf);
    if (cart_load(path) == 0) fprintf(stderr, "[tcc] hot-reloaded %s\n", path);
    else                      cart_mtime = m;   // don't re-attempt the broken file every frame
}
#endif // DE_TCC

// ------------------------------------------------------------
// internal state
// ------------------------------------------------------------

#define PALETTE_SIZE 32
#define SPRITE_SIZE  16
#define SPRITE_COUNT 64   // 8×8 grid of 16×16 sprites = 128×128 sheet
#ifndef SCALE
#define SCALE 4
#endif

static Texture2D       spritesheet;
static Image           spritesheet_img = {0};
static RenderTexture2D canvas;
static Font            game_font;
static Font            font_small = {0};
static Font            font_tiny  = {0};
static int             active_font_id = FONT_NORMAL;
static bool            custom_font = false;
static Color           palette[PALETTE_SIZE];
static Color           base_palette[PALETTE_SIZE];   // pristine copy, for pal_reset()
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
// internal patterned-fill helpers — the public fills call these when fillp() is on
static void rectfill_pat(int x, int y, int w, int h, int pattern, int c1, int c0);
static void circfill_pat(int x, int y, int radius, int pattern, int c1, int c0);
static void ovalfill_pat(int cx, int cy, int rx, int ry, int pattern, int c1, int c0);
static void plot_pat(int x, int y, int color);
static void poly_fill_cov(const int *xy, int n, int color);
static void poly_stroke_cov(const int *xy, int n, int color);
// pal()-on-sprites helpers (defined in the graphics section, used earlier in main/pal)
static void pal_shader_init(void);
static void pal_recompute(void);

#define BTN_COUNT 6
static bool            btn_curr[2][BTN_COUNT];
static bool            btn_prev[2][BTN_COUNT];

static uint8_t         map_data[MAP_W * MAP_H];
static int             map_scale_factor = 1;   // map_scale() — integer zoom for map drawing
static int             frame_count      = 0;
#ifdef PLATFORM_WEB
static bool            web_started      = false;  // true after the user clicks to start
#endif

// ------------------------------------------------------------
// debug harness — deterministic clock + input record/replay + trace.
// Off by default and entirely native: a normal run touches none of this.
// Enabled by CLI flags parsed in main(): --det, --record, --replay,
// --script, --trace, --frames, --dump/--dump-every. See loop_step().
// ------------------------------------------------------------
static int raylib_mouse_button(int button);   // defined in the mouse api section
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
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

static FILE   *rec_file       = NULL;    // --record: tagged "<frame> k|m|b ..." per change
static unsigned char key_rec_prev[KEYSTATE_N];     // last recorded down-state per key
static int           mouse_rec_px = -1, mouse_rec_py = -1;  // last recorded pointer
static unsigned char mbtn_rec_prev[3];             // last recorded button states

// kind: 0 = key (a=keycode, b=down), 1 = mouse-move (a=x, b=y),
//       2 = mouse-button (a=button index 0/1/2, b=down)
typedef struct { int frame; int kind; int a; int b; } InputEvent;
static InputEvent *replay_ev  = NULL;    // sorted by (frame, kind)
static int         replay_n   = 0;
static int         replay_i   = 0;       // next event to apply

static FILE   *trace_file     = NULL;    // --trace: one JSONL line of watch() state per frame
static int     dump_every     = 0;       // --dump-every: 0 = off, else export every Nth frame
static char    dump_dir[256]  = {0};     // --dump: directory for filmstrip PNGs
static int     max_frames     = 0;       // --frames: stop after N frames (0 = run until close)

// synthetic clock: deterministic runs read frame-derived time, not the wall clock
static double clk(void) { return det_mode ? det_clock : GetTime(); }

// normalize a public MOUSE_* button to a 0/1/2 index (matches raylib_mouse_button)
static int mbtn_index(int button) {
    return button == MOUSE_RIGHT ? 1 : button == MOUSE_MIDDLE ? 2 : 0;
}

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
    if (inject_input) return clampi(mouse_inj_x, 0, SCREEN_W - 1);
    return clampi((int)(GetMousePosition().x / SCALE), 0, SCREEN_W - 1);
}
static int inp_mouse_y(void) {
    if (inject_input) return clampi(mouse_inj_y, 0, SCREEN_H - 1);
    return clampi((int)(GetMousePosition().y / SCALE), 0, SCREEN_H - 1);
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
#else
// web build: harness is a no-op, input goes straight to raylib
static double clk(void) { return GetTime(); }
static bool inp_down(int k)    { return IsKeyDown(k); }
static bool inp_pressed(int k) { return IsKeyPressed(k); }
static bool inp_released(int k){ return IsKeyReleased(k); }
static int  inp_mouse_x(void)  { return clampi((int)(GetMousePosition().x / SCALE), 0, SCREEN_W - 1); }
static int  inp_mouse_y(void)  { return clampi((int)(GetMousePosition().y / SCALE), 0, SCREEN_H - 1); }
static bool inp_mouse_down(int b)     { return IsMouseButtonDown(raylib_mouse_button(b)); }
static bool inp_mouse_pressed(int b)  { return IsMouseButtonPressed(raylib_mouse_button(b)); }
static bool inp_mouse_released(int b) { return IsMouseButtonReleased(raylib_mouse_button(b)); }
#endif

// ------------------------------------------------------------
// touch state (all coordinates in window pixels unless noted)
// ------------------------------------------------------------

#ifndef TOUCH_CONTROLS_DEFAULT
#define TOUCH_CONTROLS_DEFAULT 0
#endif

static bool show_touch_ui = TOUCH_CONTROLS_DEFAULT;

#define STICK_RADIUS    60.0f
#define STICK_DEADZONE  0.35f
#define BTN_RADIUS      44

static int   stick_touch_id = -1;
static float stick_base_x = 0, stick_base_y = 0;
static float stick_knob_x = 0, stick_knob_y = 0;

static int btn_a_cx, btn_a_cy;
static int btn_b_cx, btn_b_cy;

// virtual touch pool — merges raylib touches with mouse-as-touch on desktop.
// the mouse LMB is exposed as a synthetic touch with id MOUSE_TOUCH_ID.
#define MOUSE_TOUCH_ID  (-2)
#define VT_MAX           16
static int     vt_count = 0;
static Vector2 vt_pos[VT_MAX];
static int     vt_id[VT_MAX];

// camera + clip state
// the camera is a raylib Camera2D applied via BeginMode2D, so zoom/rotation are GPU
// matrix work (not per-primitive math). offset is pinned to the screen centre, so
// zoom/angle pivot there; target is back-computed from camera(x,y) to stay identical
// to the old "world (x,y) at top-left" translate at zoom 1 / angle 0.
static Camera2D cam = {
    .offset   = { SCREEN_W / 2.0f, SCREEN_H / 2.0f },
    .target   = { SCREEN_W / 2.0f, SCREEN_H / 2.0f },
    .rotation = 0.0f,
    .zoom     = 1.0f,
};
static bool  cam_active = false;   // true while inside BeginMode2D during draw()
static bool  clip_active = false;

// pget snapshot — refreshed at the start of every frame, so pget reads
// the previous frame's canvas (consistent state, no mid-frame RT readback)
static Image pget_snapshot     = (Image){0};
static bool  pget_snapshot_valid = false;

static void init_touch_layout(void) {
    int W = SCREEN_W * SCALE, H = SCREEN_H * SCALE;
    btn_a_cx = W -  80;  btn_a_cy = H -  80;
    btn_b_cx = W - 180;  btn_b_cy = H - 120;
}

static bool point_in_circle(float px, float py, float cx, float cy, float r) {
    float dx = px - cx, dy = py - cy;
    return dx*dx + dy*dy <= r*r;
}

static void poll_virtual_touches(void) {
    int n = GetTouchPointCount();
    if (n > VT_MAX - 1) n = VT_MAX - 1;
    for (int i = 0; i < n; i++) {
        vt_pos[i] = GetTouchPosition(i);
        vt_id[i]  = GetTouchPointId(i);
    }
    vt_count = n;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && vt_count < VT_MAX) {
        vt_pos[vt_count] = GetMousePosition();
        vt_id[vt_count]  = MOUSE_TOUCH_ID;
        vt_count++;
    }
}

static bool any_touch_in_circle(int cx, int cy, int r) {
    for (int i = 0; i < vt_count; i++) {
        if (point_in_circle(vt_pos[i].x, vt_pos[i].y, cx, cy, r)) return true;
    }
    return false;
}

static void update_stick(void) {
    if (!show_touch_ui) { stick_touch_id = -1; return; }

    int W = SCREEN_W * SCALE;

    bool still_active = false;
    if (stick_touch_id != -1) {
        for (int i = 0; i < vt_count; i++) {
            if (vt_id[i] == stick_touch_id) {
                Vector2 p = vt_pos[i];
                float dx = p.x - stick_base_x, dy = p.y - stick_base_y;
                float len = sqrtf(dx*dx + dy*dy);
                if (len > STICK_RADIUS) { dx = dx/len*STICK_RADIUS; dy = dy/len*STICK_RADIUS; }
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
            if (p.x > W * 0.55f) continue;
            if (point_in_circle(p.x, p.y, btn_a_cx, btn_a_cy, BTN_RADIUS)) continue;
            if (point_in_circle(p.x, p.y, btn_b_cx, btn_b_cy, BTN_RADIUS)) continue;
            stick_touch_id = vt_id[i];
            stick_base_x = stick_knob_x = p.x;
            stick_base_y = stick_knob_y = p.y;
            break;
        }
    }
}

static void draw_touch_overlay(void) {
    if (!show_touch_ui) return;

    Color ring  = (Color){ 255, 255, 255,  70 };
    Color knob  = (Color){ 255, 255, 255, 160 };
    Color press = (Color){ 255, 255, 255, 110 };

    if (stick_touch_id != -1) {
        DrawCircleLines((int)stick_base_x, (int)stick_base_y, STICK_RADIUS, ring);
        DrawCircleV((Vector2){ stick_knob_x, stick_knob_y }, STICK_RADIUS * 0.45f, knob);
    } else {
        // resting hint — mirror of A's position on the bottom-left
        int hx = 80;
        int hy = SCREEN_H * SCALE - 80;
        Color hint = (Color){ 255, 255, 255, 40 };
        DrawCircleLines(hx, hy, STICK_RADIUS, hint);
        DrawCircleV((Vector2){ (float)hx, (float)hy }, STICK_RADIUS * 0.45f, hint);
    }

    bool a_down = any_touch_in_circle(btn_a_cx, btn_a_cy, BTN_RADIUS);
    bool b_down = any_touch_in_circle(btn_b_cx, btn_b_cy, BTN_RADIUS);
    DrawCircle(btn_a_cx, btn_a_cy, BTN_RADIUS, a_down ? press : ring);
    DrawCircle(btn_b_cx, btn_b_cy, BTN_RADIUS, b_down ? press : ring);
    DrawCircleLines(btn_a_cx, btn_a_cy, BTN_RADIUS, knob);
    DrawCircleLines(btn_b_cx, btn_b_cy, BTN_RADIUS, knob);

    float fs = 4 * SCALE;
    DrawTextEx(game_font, "A", (Vector2){ btn_a_cx - fs/2, btn_a_cy - fs/2 }, fs, 0, WHITE);
    DrawTextEx(game_font, "B", (Vector2){ btn_b_cx - fs/2, btn_b_cy - fs/2 }, fs, 0, WHITE);
}

// ------------------------------------------------------------
// palette — the 32-color PICO-8 palette (matches the sprite editor's pico32)
// ------------------------------------------------------------

static void load_palette() {
    // standard 16 (0-15)
    palette[0]  = (Color){ 0,   0,   0,   255 }; // CLR_BLACK          #000000
    palette[1]  = (Color){ 29,  43,  83,  255 }; // CLR_DARK_BLUE      #1d2b53
    palette[2]  = (Color){ 126, 37,  83,  255 }; // CLR_DARK_PURPLE    #7e2553
    palette[3]  = (Color){ 0,   135, 81,  255 }; // CLR_DARK_GREEN     #008751
    palette[4]  = (Color){ 171, 82,  54,  255 }; // CLR_BROWN          #ab5236
    palette[5]  = (Color){ 95,  87,  79,  255 }; // CLR_DARK_GREY      #5f574f
    palette[6]  = (Color){ 194, 195, 199, 255 }; // CLR_LIGHT_GREY     #c2c3c7
    palette[7]  = (Color){ 255, 241, 232, 255 }; // CLR_WHITE          #fff1e8
    palette[8]  = (Color){ 255, 0,   77,  255 }; // CLR_RED            #ff004d
    palette[9]  = (Color){ 255, 163, 0,   255 }; // CLR_ORANGE         #ffa300
    palette[10] = (Color){ 255, 236, 39,  255 }; // CLR_YELLOW         #ffec27
    palette[11] = (Color){ 0,   228, 54,  255 }; // CLR_GREEN          #00e436
    palette[12] = (Color){ 41,  173, 255, 255 }; // CLR_BLUE           #29adff
    palette[13] = (Color){ 131, 118, 156, 255 }; // CLR_INDIGO         #83769c
    palette[14] = (Color){ 255, 119, 168, 255 }; // CLR_PINK           #ff77a8
    palette[15] = (Color){ 255, 204, 170, 255 }; // CLR_LIGHT_PEACH    #ffccaa
    // extended 16 (16-31) — undocumented "secret" colors
    palette[16] = (Color){ 41,  24,  20,  255 }; // CLR_BROWNISH_BLACK #291814
    palette[17] = (Color){ 17,  29,  53,  255 }; // CLR_DARKER_BLUE    #111d35
    palette[18] = (Color){ 66,  33,  54,  255 }; // CLR_DARKER_PURPLE  #422136
    palette[19] = (Color){ 18,  83,  89,  255 }; // CLR_BLUE_GREEN     #125359
    palette[20] = (Color){ 116, 47,  41,  255 }; // CLR_DARK_BROWN     #742f29
    palette[21] = (Color){ 73,  51,  59,  255 }; // CLR_DARKER_GREY    #49333b
    palette[22] = (Color){ 162, 136, 121, 255 }; // CLR_MEDIUM_GREY    #a28879
    palette[23] = (Color){ 243, 239, 125, 255 }; // CLR_LIGHT_YELLOW   #f3ef7d
    palette[24] = (Color){ 190, 18,  80,  255 }; // CLR_DARK_RED       #be1250
    palette[25] = (Color){ 255, 108, 36,  255 }; // CLR_DARK_ORANGE    #ff6c24
    palette[26] = (Color){ 168, 231, 46,  255 }; // CLR_LIME_GREEN     #a8e72e
    palette[27] = (Color){ 0,   181, 67,  255 }; // CLR_MEDIUM_GREEN   #00b543
    palette[28] = (Color){ 6,   90,  181, 255 }; // CLR_TRUE_BLUE      #065ab5
    palette[29] = (Color){ 117, 70,  101, 255 }; // CLR_MAUVE          #754665
    palette[30] = (Color){ 255, 110, 89,  255 }; // CLR_DARK_PEACH     #ff6e59
    palette[31] = (Color){ 255, 157, 129, 255 }; // CLR_PEACH          #ff9d81

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

#define WATCH_MAX 16
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

    DrawRectangle(8, 8, w, h, (Color){ 0, 0, 0, 180 });
    DrawRectangleLines(8, 8, w, h, (Color){ 200, 200, 200, 120 });

    for (int i = 0; i < watch_count; i++) {
        char line[80];
        snprintf(line, sizeof(line), "%s: %s", watches[i].name, watches[i].value);
        DrawTextEx(game_font, line,
            (Vector2){ 8 + pad, 8 + pad + i * lh },
            12, 1, WHITE);
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
        while (replay_i < replay_n && replay_ev[replay_i].frame <= fno) {
            InputEvent e = replay_ev[replay_i++];
            if      (e.kind == 0) { if (e.a >= 0 && e.a < KEYSTATE_N) key_inject[e.a] = (unsigned char)(e.b ? 1 : 0); }
            else if (e.kind == 1) { mouse_inj_x = e.a; mouse_inj_y = e.b; }
            else if (e.kind == 2) { if (e.a >= 0 && e.a < 3) mbtn_inj[e.a] = (unsigned char)(e.b ? 1 : 0); }
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

static void harness_dump(int fno) {
    if (dump_every <= 0 || (fno % dump_every) != 0) return;
    Image shot = LoadImageFromTexture(canvas.texture);
    ImageFlipVertical(&shot);
    char path[320];
    snprintf(path, sizeof path, "%s/frame_%05d.png", dump_dir[0] ? dump_dir : ".", fno);
    ExportImage(shot, path);
    UnloadImage(shot);
}
#endif

// ── profiler instrumentation (compiled in only with -DDE_PROFILE) ─────────
// Counts draw-primitive calls and times the CPU work per frame (update+draw,
// excluding the vsync wait). A re-entrancy guard means only the OUTERMOST
// public call is counted, so circfill()→trifill() reads as one circfill, not
// also a trifill. Flushed to build/perf.json every 30 frames so the data
// survives the editor killing the process after sampling. The editor reads it
// back. A normal build defines PROF() as a no-op — zero cost, nothing emitted.
#ifdef DE_PROFILE
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

static void prof_write(void) {
    FILE *f = fopen("perf.json", "w");
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

static void loop_step(void) {
#ifdef PLATFORM_WEB
    if (!web_started) {
        bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
                    || IsKeyPressed(KEY_ENTER)
                    || IsKeyPressed(KEY_SPACE);
        if (clicked) {
            InitAudioDevice();
            sound_init();
            init();
            web_started = true;
        }
        BeginTextureMode(canvas);
        ClearBackground(palette[0]);
        const char *msg = "click to start";
        int tw = (int)(strlen(msg) * 8);
        DrawTextEx(game_font, msg,
            (Vector2){ (SCREEN_W - tw) / 2.0f, SCREEN_H / 2.0f - 4 },
            8, 0, palette[7]);
        EndTextureMode();
        BeginDrawing();
        DrawTexturePro(canvas.texture,
            (Rectangle){ 0, 0, SCREEN_W, -SCREEN_H },
            (Rectangle){ 0, 0, SCREEN_W * SCALE, SCREEN_H * SCALE },
            (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
        return;
    }
    frame_dt = GetFrameTime();
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
    poll_virtual_touches();
    update_stick();
    if (IsKeyPressed(KEY_F1)) watch_show = !watch_show;

    // snapshot last frame's canvas so pget() has stable pixels to read
    // (skipped on web — GPU readback is expensive and triggers GL errors on WebGL1)
#ifndef PLATFORM_WEB
    if (pget_snapshot.data) UnloadImage(pget_snapshot);
    pget_snapshot       = LoadImageFromTexture(canvas.texture);
    pget_snapshot_valid = pget_snapshot.data != NULL;
#endif
    // snapshot input edges before update so btnp() works
    for (int p = 0; p < 2; p++)
        for (int b = 0; b < BTN_COUNT; b++) {
            btn_prev[p][b] = btn_curr[p][b];
            btn_curr[p][b] = btn(p, b);
        }
    // characters typed this frame for text_input()
    { int n = 0, ch; while ((ch = GetCharPressed()) != 0 && n < 31) text_buf[n++] = (char)ch; text_buf[n] = 0; }
    // fade is immediate-mode like every other draw call: it clears each frame, so a
    // cart must re-assert fade() every frame it wants the screen dimmed. This is why
    // a conditional `if (gameover) fade(0.5f);` clears itself once the state ends —
    // carts never need to call fade(0) by hand.
    fade_amt = 0.0f;
#ifdef DE_PROFILE
    double prof_t0 = GetTime();
#endif
#ifdef DE_TCC
    if (cart_update) cart_update();
#else
    update();
#endif
    frame_count++;

    // draw into the low-res canvas, under the camera matrix (handles scroll + zoom +
    // rotation on the GPU). camera()/camera_ex() called inside draw() re-apply via
    // cam_reapply() while cam_active is true.
    BeginTextureMode(canvas);
        BeginMode2D(cam);
        cam_active = true;
#ifdef DE_TCC
        if (cart_draw) cart_draw();
#else
        draw();
#endif
        cam_active = false;
        EndMode2D();
    EndTextureMode();
#ifdef DE_PROFILE
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
        if (frame_count % 30 == 0) prof_write();          // periodic flush (survives kill)
    }
#endif
    if (clip_active) { EndScissorMode(); clip_active = false; }

    // scale up to the window — RenderTexture is flipped in Y
    float sox = 0, soy = 0;
    if (shake_amt > 0.2f) {
        sox = ((rand() % 201) - 100) / 100.0f * shake_amt * SCALE;
        soy = ((rand() % 201) - 100) / 100.0f * shake_amt * SCALE;
    }
    BeginDrawing();
        DrawTexturePro(
            canvas.texture,
            (Rectangle){ 0, 0,  SCREEN_W, -SCREEN_H },
            (Rectangle){ sox, soy,  SCREEN_W * SCALE, SCREEN_H * SCALE },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );
        if (fade_amt > 0.0f)
            DrawRectangle(0, 0, SCREEN_W * SCALE, SCREEN_H * SCALE,
                          (Color){ 0, 0, 0, (unsigned char)(fade_amt * 255) });
        draw_touch_overlay();
        draw_watch_overlay();
    EndDrawing();
    if (shake_amt > 0) { shake_amt *= 0.85f; if (shake_amt < 0.2f) shake_amt = 0; }

#ifndef PLATFORM_WEB
    harness_trace(fno);                    // structured state for this frame (before aging)
    harness_dump(fno);                     // filmstrip PNG every Nth frame
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
    }
    fclose(f);
    qsort(replay_ev, replay_n, sizeof(InputEvent), ev_cmp);
}

// --script: a human-authored input plan. One directive per line (# = comment):
//   down/up <frame> <key>          key press / release
//   tap     <frame> <key> [dur]    press then release dur frames later (default 6)
//   move    <frame> <x> <y>        move the pointer to canvas (x,y)
//   click   <frame> <x> <y> [btn]  move there, then a left-click (btn 1=right,2=mid)
// <key> is a single char (a,s,k,l) or a name (SPACE, LEFT, ...).
static void load_script(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "harness: cannot open script %s\n", path); return; }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char cmd[32], key[32]; int frame, p = 6, q = 0;
        if (line[0] == '#' || line[0] == '\n') continue;
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

int main(int argc, char **argv) {
    const char *window_title           = "dreamengine";
#ifndef PLATFORM_WEB
    int         screenshot_mode        = 0;
    int         screenshot_frames_done = 0;
    int         hide_window            = 0;
    unsigned    seed                   = 1;
    const char *rec_path = NULL, *replay_path = NULL, *script_path = NULL, *trace_path = NULL;
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
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) max_frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--dump")   == 0 && i + 1 < argc) { snprintf(dump_dir, sizeof dump_dir, "%s", argv[++i]); if (dump_every <= 0) dump_every = 1; }
        else if (strcmp(argv[i], "--dump-every") == 0 && i + 1 < argc) dump_every = atoi(argv[++i]);
#ifdef DE_TCC
        else if (strcmp(argv[i], "--cart") == 0 && i + 1 < argc) cart_path = argv[++i];
#endif
    }
    // replay/script drive input deterministically; both imply --det
    if (replay_path) { load_replay(replay_path); inject_input = true; det_mode = true; }
    if (script_path) { load_script(script_path); inject_input = true; det_mode = true; }
    if (rec_path)    rec_file   = fopen(rec_path,  "w");
    if (trace_path)  trace_file = fopen(trace_path, "w");
    if (screenshot_mode) hide_window = 1;
    signal(SIGSEGV, crash_handler);   // bad/null pointer
    signal(SIGFPE,  crash_handler);   // divide by zero, etc.
    signal(SIGABRT, crash_handler);   // abort()/assert
    signal(SIGBUS,  crash_handler);   // misaligned / bad memory access
#endif
#ifdef PLATFORM_WEB
    SetTraceLogLevel(LOG_ERROR);     // suppress NPOT/extension warnings — harmless on web
#else
    SetTraceLogLevel(LOG_WARNING);   // keep raylib's INFO chatter out of the runtime log panel
#endif
#ifndef PLATFORM_WEB
    if (hide_window) SetWindowState(FLAG_WINDOW_HIDDEN);
#endif
    InitWindow(SCREEN_W * SCALE, SCREEN_H * SCALE, window_title);
#ifndef PLATFORM_WEB
    if (det_mode) { SetRandomSeed(seed); srand(seed); }   // reproducible rnd()/rnd_float()/shake
#endif
#ifndef PLATFORM_WEB
    InitAudioDevice();
    sound_init();
#endif
    SetTargetFPS(60);

    load_palette();
    pal_shader_init();   // pal()-on-sprites swap shader (needs the GL context from InitWindow)
    init_touch_layout();

    if (MAP_DATA_LEN >= sizeof(map_data)) {
        memcpy(map_data, MAP_DATA, sizeof(map_data));
    } else {
        memset(map_data, 0, sizeof(map_data));
    }

    // low-res canvas — all drawing goes here, then scaled up
    canvas = LoadRenderTexture(SCREEN_W, SCREEN_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);

    {
        Image fontImage = LoadImageFromMemory(".png", DOS_8X8_FONT, DOS_8X8_FONT_LEN);
        game_font = LoadFontFromImage(fontImage, (Color){ 255, 255, 0, 255 }, 0);
        SetTextureFilter(game_font.texture, TEXTURE_FILTER_POINT);
        UnloadImage(fontImage);
        custom_font = true;
    }
    {
        Image img = LoadImageFromMemory(".png", FONT4X6_DATA, FONT4X6_DATA_LEN);
        font_small = LoadFontFromImage(img, (Color){ 255, 255, 0, 255 }, 32);
        SetTextureFilter(font_small.texture, TEXTURE_FILTER_POINT);
        UnloadImage(img);
    }
    {
        Image img = LoadImageFromMemory(".png", FONT3X5_DATA, FONT3X5_DATA_LEN);
        font_tiny = LoadFontFromImage(img, (Color){ 255, 255, 0, 255 }, 32);
        SetTextureFilter(font_tiny.texture, TEXTURE_FILTER_POINT);
        UnloadImage(img);
    }

    if (SPRITES_DATA_LEN > 0) {
        Image img = LoadImageFromMemory(".png", SPRITES_DATA, SPRITES_DATA_LEN);
        if (img.width > 0) {
            spritesheet_img = img;
            spritesheet = LoadTextureFromImage(img);
            SetTextureFilter(spritesheet, TEXTURE_FILTER_POINT);
        }
    }

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
    while (!WindowShouldClose()) {
        loop_step();
        if (screenshot_mode && ++screenshot_frames_done >= 3) break;
        if (max_frames > 0 && frame_count >= max_frames) break;   // bounded harness run
    }

    // save last frame as screenshot.png so the cart PNG thumbnail shows the game
    Image screenshot = LoadImageFromTexture(canvas.texture);
    ImageFlipVertical(&screenshot);
    ExportImage(screenshot, "screenshot.png");
    UnloadImage(screenshot);

    if (rec_file)   { fclose(rec_file);   rec_file   = NULL; }
    if (trace_file) { fclose(trace_file); trace_file = NULL; }
    free(replay_ev); replay_ev = NULL;

    if (custom_font) UnloadFont(game_font);
    if (font_small.texture.id > 0) UnloadFont(font_small);
    if (font_tiny.texture.id  > 0) UnloadFont(font_tiny);
    if (pal_shader_ok) UnloadShader(pal_shader);
    if (spritesheet.width > 0) UnloadTexture(spritesheet);
    if (spritesheet_img.data) UnloadImage(spritesheet_img);
    if (pget_snapshot.data) UnloadImage(pget_snapshot);
    UnloadRenderTexture(canvas);
    sound_shutdown();
    CloseAudioDevice();
    CloseWindow();
#endif
    return 0;
}

// ------------------------------------------------------------
// api implementation
// ------------------------------------------------------------

bool btn(int player, int button) {
    if (player == 0) {
        if (show_touch_ui) {
            float sx = stick_x(), sy = stick_y();
            switch (button) {
                case BTN_UP:    if (sy < -STICK_DEADZONE) return true; break;
                case BTN_DOWN:  if (sy >  STICK_DEADZONE) return true; break;
                case BTN_LEFT:  if (sx < -STICK_DEADZONE) return true; break;
                case BTN_RIGHT: if (sx >  STICK_DEADZONE) return true; break;
                case BTN_A:     if (any_touch_in_circle(btn_a_cx, btn_a_cy, BTN_RADIUS)) return true; break;
                case BTN_B:     if (any_touch_in_circle(btn_b_cx, btn_b_cy, BTN_RADIUS)) return true; break;
            }
        }
        switch (button) {
            case BTN_UP:    return inp_down(KEY_W);
            case BTN_DOWN:  return inp_down(KEY_S);
            case BTN_LEFT:  return inp_down(KEY_A);
            case BTN_RIGHT: return inp_down(KEY_D);
            case BTN_A:     return inp_down(KEY_Z);
            case BTN_B:     return inp_down(KEY_X);
        }
    } else if (player == 1) {
        switch (button) {
            case BTN_UP:    return inp_down(KEY_UP);
            case BTN_DOWN:  return inp_down(KEY_DOWN);
            case BTN_LEFT:  return inp_down(KEY_LEFT);
            case BTN_RIGHT: return inp_down(KEY_RIGHT);
            case BTN_A:     return inp_down(KEY_COMMA);
            case BTN_B:     return inp_down(KEY_PERIOD);
        }
    }
    return false;
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
    return (int)(vt_pos[i].x / SCALE);
}
int touch_y(int i) {
    if (i < 0 || i >= vt_count) return -1;
    return (int)(vt_pos[i].y / SCALE);
}

bool tap(int x, int y, int w, int h) {
    for (int i = 0; i < vt_count; i++) {
        int cx = (int)(vt_pos[i].x / SCALE), cy = (int)(vt_pos[i].y / SCALE);
        if (cx >= x && cx < x + w && cy >= y && cy < y + h) return true;
    }
    return false;
}

void touch_controls(bool on) { show_touch_ui = on; }

float stick_x(void) {
    if (stick_touch_id < 0) return 0.0f;
    return (stick_knob_x - stick_base_x) / STICK_RADIUS;
}
float stick_y(void) {
    if (stick_touch_id < 0) return 0.0f;
    return (stick_knob_y - stick_base_y) / STICK_RADIUS;
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
float mouse_wheel(void)         { return GetMouseWheelMove(); }
int mouse_world_x(void)         { return (int)GetScreenToWorld2D((Vector2){ (float)mouse_x(), (float)mouse_y() }, cam).x; }
int mouse_world_y(void)         { return (int)GetScreenToWorld2D((Vector2){ (float)mouse_x(), (float)mouse_y() }, cam).y; }

void cls(int color) {
    PROF("cls");
    ClearBackground(palette[color % PALETTE_SIZE]);
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
    "uniform vec3 basePal[32];\n"
    "uniform vec3 curPal[32];\n"
    "void main() {\n"
    "    vec4 texel = texture2D(texture0, fragTexCoord);\n"
    "    float bestD = 1e20;\n"
    "    vec3 outc = curPal[0];\n"
    "    for (int i = 0; i < 32; i++) {\n"
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
    "uniform vec3 basePal[32];\n"
    "uniform vec3 curPal[32];\n"
    "void main() {\n"
    "    vec4 texel = texture(texture0, fragTexCoord);\n"
    "    float bestD = 1e20;\n"
    "    vec3 outc = curPal[0];\n"
    "    for (int i = 0; i < 32; i++) {\n"
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
    Image tmp = ImageCopy(spritesheet_img);
    if (color >= 0 && color < PALETTE_SIZE) {
        ImageFormat(&tmp, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        Color key = palette[color];
        ImageColorReplace(&tmp, key, (Color){ key.r, key.g, key.b, 0 });
    }
    if (spritesheet.width > 0) UnloadTexture(spritesheet);
    spritesheet = LoadTextureFromImage(tmp);
    SetTextureFilter(spritesheet, TEXTURE_FILTER_POINT);
    UnloadImage(tmp);
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
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    pal_end();
}

void sspr(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    PROF("sspr");
    if (spritesheet.width == 0) return;
    Rectangle src = { sx, sy, sw, sh };
    Rectangle dst = { dx, dy, dw, dh };
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    pal_end();
}

void spr_rot(int index, int x, int y, float deg) {
    PROF("spr_rot");
    if (spritesheet.width == 0) return;
    int cols = spritesheet.width / SPRITE_SIZE;
    Rectangle src = { (index % cols) * SPRITE_SIZE, (index / cols) * SPRITE_SIZE, SPRITE_SIZE, SPRITE_SIZE };
    float h = SPRITE_SIZE / 2.0f;
    Rectangle dst = { x + h, y + h, SPRITE_SIZE, SPRITE_SIZE };  // origin maps here, so top-left stays (x,y)
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ h, h }, deg, WHITE);
    pal_end();
}

void sspr_ex(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, float deg, int ox, int oy) {
    PROF("sspr_ex");
    if (spritesheet.width == 0) return;
    Rectangle src = { sx, sy, sw, sh };
    Rectangle dst = { dx + ox, dy + oy, dw, dh };               // pivot (ox,oy) is in dest space, relative to (dx,dy)
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ (float)ox, (float)oy }, deg, WHITE);
    pal_end();
}

// ------------------------------------------------------------
// font state + print helpers
// ------------------------------------------------------------

void font(int f) { active_font_id = (f == FONT_SMALL || f == FONT_TINY) ? f : FONT_NORMAL; }

static Font cur_font(void) {
    if (active_font_id == FONT_SMALL) return font_small;
    if (active_font_id == FONT_TINY)  return font_tiny;
    return game_font;
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
    DrawTextEx(cur_font(), text, (Vector2){ (float)x, (float)y },
               cur_font_size(), 0, palette[color % PALETTE_SIZE]);
    return x + text_width(text);
}


int print_outline(const char *text, int x, int y, int color, int outline_color) {
    PROF("print_outline");
    Color oc = palette[outline_color % PALETTE_SIZE];
    float sz = cur_font_size();
    Font  f  = cur_font();
    static const int offsets[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
    for (int i = 0; i < 8; i++)
        DrawTextEx(f, text,
                   (Vector2){ (float)(x + offsets[i][0]), (float)(y + offsets[i][1]) },
                   sz, 0, oc);
    return print(text, x, y, color);
}

void rect(int x, int y, int w, int h, int color) {
    PROF("rect");
    Color c = palette[color % PALETTE_SIZE];
    int rx = x, ry = y;
    // 1px DrawRectangle slices — no line caps, exact pixel coverage
    DrawRectangle(rx,     ry,     w,   1,   c);  // top
    DrawRectangle(rx,     ry+h-1, w,   1,   c);  // bottom
    DrawRectangle(rx,     ry+1,   1,   h-2, c);  // left
    DrawRectangle(rx+w-1, ry+1,   1,   h-2, c);  // right
}

void rectfill(int x, int y, int w, int h, int color) {
    PROF("rectfill");
    if (fp_on) { rectfill_pat(x, y, w, h, fp_global, fp_hole, color); return; }   // 1-bits = holes, 0-bits = color
    DrawRectangle(x, y, w, h, palette[color % PALETTE_SIZE]);
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
#define FP_CACHE 32
static struct { int pat, c1, c0; Texture2D tex; bool used; } fp_cache[FP_CACHE];
static int fp_round = 0;

static Texture2D fp_texture(int pat, int c1, int c0) {
    for (int i = 0; i < FP_CACHE; i++)
        if (fp_cache[i].used && fp_cache[i].pat == pat && fp_cache[i].c1 == c1 && fp_cache[i].c0 == c0)
            return fp_cache[i].tex;
    Color px[16];
    for (int i = 0; i < 16; i++) {
        int on = (pat >> (15 - i)) & 1;                       // bit 15 = top-left, row-major
        px[i] = on ? (c1 < 0 ? (Color){ 0, 0, 0, 0 } : palette[c1 % PALETTE_SIZE])
                   : (c0 < 0 ? (Color){ 0, 0, 0, 0 } : palette[c0 % PALETTE_SIZE]);
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
    // outline = pixels inside the disc that have at least one outside 4-neighbour
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++)
            if (disc_inside(x,y,cx,cy,r) &&
                (!disc_inside(x-1,y,cx,cy,r) || !disc_inside(x+1,y,cx,cy,r) ||
                 !disc_inside(x,y-1,cx,cy,r) || !disc_inside(x,y+1,cx,cy,r)))
                pset(x, y, color);
}

void circfill(int cx, int cy, int r, int color) {
    PROF("circfill");
    // fill = all pixels inside the disc; plot_pat handles both solid and fillp dither
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++)
            if (disc_inside(x, y, cx, cy, r)) plot_pat(x, y, color);
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

void line(int x1, int y1, int x2, int y2, int color) {
    PROF("line");
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
void bezier_cubic(int x0, int y0, int cx0, int cy0, int cx1, int cy1, int x1, int y1, int color) {
    PROF("bezier_cubic");
    float len = sqrtf((float)((cx0-x0)*(cx0-x0)+(cy0-y0)*(cy0-y0)))
              + sqrtf((float)((cx1-cx0)*(cx1-cx0)+(cy1-cy0)*(cy1-cy0)))
              + sqrtf((float)((x1-cx1)*(x1-cx1)+(y1-cy1)*(y1-cy1)));
    int n = bez_steps(len);
    float px = (float)x0, py = (float)y0;
    for (int i = 1; i <= n; i++) {
        float t = i / (float)n, mt = 1.0f - t;
        float nx = mt*mt*mt*x0 + 3.0f*mt*mt*t*cx0 + 3.0f*mt*t*t*cx1 + t*t*t*x1;
        float ny = mt*mt*mt*y0 + 3.0f*mt*mt*t*cy0 + 3.0f*mt*t*t*cy1 + t*t*t*y1;
        line((int)roundf(px), (int)roundf(py), (int)roundf(nx), (int)roundf(ny), color);
        px = nx; py = ny;
    }
}

void pset(int x, int y, int color) {
    PROF("pset");
    DrawPixel(x, y, palette[color % PALETTE_SIZE]);
}

int pget(int x, int y) {
    if (!pget_snapshot_valid) return 0;
    // pget(x,y) takes a WORLD coord and reads the screen pixel it landed on, so run it
    // through the camera matrix. exact under translate; approximate under zoom/rotation
    // (it samples the rendered canvas, which is the documented limitation).
    Vector2 s = GetWorldToScreen2D((Vector2){ (float)x, (float)y }, cam);
    int rx = (int)s.x;
    int ry = (int)s.y;
    if (rx < 0 || rx >= SCREEN_W || ry < 0 || ry >= SCREEN_H) return 0;
    // RT data is bottom-up in raylib; flip Y to match draw coords
    Color c = GetImageColor(pget_snapshot, rx, SCREEN_H - 1 - ry);
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

void camera(int x, int y) {
    cam.offset   = (Vector2){ SCREEN_W / 2.0f, SCREEN_H / 2.0f };
    cam.target   = (Vector2){ x + SCREEN_W / 2.0f, y + SCREEN_H / 2.0f };
    cam.zoom     = 1.0f;   // plain camera: camera() always means no zoom / no rotation
    cam.rotation = 0.0f;
    cam_reapply();
}

void camera_ex(int x, int y, float zoom, float angle) {
    if (zoom < 0.01f) zoom = 0.01f;   // guard a degenerate / inverted matrix
    cam.offset   = (Vector2){ SCREEN_W / 2.0f, SCREEN_H / 2.0f };
    cam.target   = (Vector2){ x + SCREEN_W / 2.0f, y + SCREEN_H / 2.0f };
    cam.zoom     = zoom;
    cam.rotation = angle;
    cam_reapply();
}

void clip(int x, int y, int w, int h) {
    if (clip_active) { EndScissorMode(); clip_active = false; }
    if (w <= 0 || h <= 0) return;
    BeginScissorMode(x, y, w, h);
    clip_active = true;
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
            Rectangle src = { (float)((v % cols) * CELL_W), (float)((v / cols) * CELL_H), (float)CELL_W, (float)CELL_H };
            Rectangle dst = { (float)(sx + xi * dw), (float)(sy + yi * dh), (float)dw, (float)dh };
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
    if (sx) *sx = SCREEN_W / 2 + (int)(p.x * f * zoom);
    if (sy) *sy = SCREEN_H / 2 + (int)(p.y * f * zoom);
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
// fan + line() versions had. trifill stays GPU (the 3D hot path).
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
    Vector2 c0 = GetScreenToWorld2D((Vector2){ 0,        0        }, cam);
    Vector2 c1 = GetScreenToWorld2D((Vector2){ SCREEN_W, 0        }, cam);
    Vector2 c2 = GetScreenToWorld2D((Vector2){ 0,        SCREEN_H }, cam);
    Vector2 c3 = GetScreenToWorld2D((Vector2){ SCREEN_W, SCREEN_H }, cam);
    int lo_x = (int)floorf(fminf(fminf(c0.x, c1.x), fminf(c2.x, c3.x)));
    int lo_y = (int)floorf(fminf(fminf(c0.y, c1.y), fminf(c2.y, c3.y)));
    int hi_x = (int)ceilf (fmaxf(fmaxf(c0.x, c1.x), fmaxf(c2.x, c3.x)));
    int hi_y = (int)ceilf (fmaxf(fmaxf(c0.y, c1.y), fmaxf(c2.y, c3.y)));
    if (*x0 < lo_x) *x0 = lo_x;  if (*y0 < lo_y) *y0 = lo_y;
    if (*x1 > hi_x) *x1 = hi_x;  if (*y1 > hi_y) *y1 = hi_y;
}
static void poly_fill_cov(const int *xy, int n, int color) {
    if (n < 3) return;
    int x0, y0, x1, y1; poly_bbox(xy, n, &x0, &y0, &x1, &y1);
    poly_clamp_scan(&x0, &y0, &x1, &y1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (poly_inside(x + 0.5f, y + 0.5f, xy, n)) plot_pat(x, y, color);
}
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
    fillp(pat, ca);
    rectfill(bx, by, bw, bh, cb);
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

void bounce_at_edges(int *x, int *y, int *vx, int *vy, int w, int h) {
    if (!x || !y || !vx || !vy) return;
    if      (*x < 0)            { *x = 0;            if (*vx < 0) *vx = -*vx; }
    else if (*x + w > SCREEN_W) { *x = SCREEN_W - w; if (*vx > 0) *vx = -*vx; }
    if      (*y < 0)            { *y = 0;            if (*vy < 0) *vy = -*vy; }
    else if (*y + h > SCREEN_H) { *y = SCREEN_H - h; if (*vy > 0) *vy = -*vy; }
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

int anim_once(int n_frames, float fps, float start_t) {
    if (n_frames <= 0) return 0;
    int f = (int)((now() - start_t) * fps);
    if (f < 0) f = 0;
    if (f >= n_frames) f = n_frames - 1;
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
    int cx = (int)fmaxf(0.0f, fminf((float)(tx - SCREEN_W / 2), (float)(world_w - SCREEN_W)));
    int cy = (int)fmaxf(0.0f, fminf((float)(ty - SCREEN_H / 2), (float)(world_h - SCREEN_H)));
    camera(cx, cy);
}

// ------------------------------------------------------------
// persistence
// ------------------------------------------------------------

static int  sav_data[64]  = {0};
static bool sav_loaded    = false;

static void sav_ensure(void) {
    if (sav_loaded) return;
    FILE *f = fopen("cart.sav", "rb");
    if (f) { fread(sav_data, sizeof(int), 64, f); fclose(f); }
    sav_loaded = true;
}

void save(int slot, int value) {
    if (slot < 0 || slot >= 64) return;
    sav_ensure();
    sav_data[slot] = value;
    FILE *f = fopen("cart.sav", "wb");
    if (f) { fwrite(sav_data, sizeof(int), 64, f); fclose(f); }
}

int load(int slot) {
    if (slot < 0 || slot >= 64) return 0;
    sav_ensure();
    return sav_data[slot];
}

void save_bytes(const void *data, int len) {
    if (len <= 0) return;
    FILE *f = fopen("cart.blob", "wb");
    if (f) { fwrite(data, 1, len, f); fclose(f); }
}
int load_bytes(void *out, int max) {
    FILE *f = fopen("cart.blob", "rb");
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
    FILE *f = fopen("cart.kv", "rb");
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
    FILE *f = fopen("cart.kv", "wb");
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
bool key(int k)  { return inp_down(k); }
bool keyp(int k) { return inp_pressed(k); }
bool keyr(int k) { return inp_released(k); }

void pal(int c0, int c1)  { if (c0 >= 0 && c0 < PALETTE_SIZE && c1 >= 0 && c1 < PALETTE_SIZE) { palette[c0] = base_palette[c1]; pal_recompute(); } }
void pal_reset(void)      { for (int i = 0; i < PALETTE_SIZE; i++) palette[i] = base_palette[i]; pal_recompute(); }
void fade(float a)        { fade_amt  = a < 0 ? 0 : (a > 1 ? 1 : a); }
void shake(float a)       { if (a > shake_amt) shake_amt = a; }

int print_scaled(const char *t, int x, int y, int color, int scale) {
    PROF("print_scaled");
    if (scale < 1) scale = 1;
    float sz = cur_font_size() * scale;
    DrawTextEx(cur_font(), t, (Vector2){ (float)x, (float)y },
               sz, 0, palette[color % PALETTE_SIZE]);
    return x + (int)MeasureTextEx(cur_font(), t, sz, 0).x;
}

void ovalfill(int cx, int cy, int rx, int ry, int color) {
    PROF("ovalfill");
    if (rx < 0) rx = -rx; if (ry < 0) ry = -ry;
    if (rx == 0 || ry == 0) return;
    for (int y = cy - ry; y <= cy + ry; y++)
        for (int x = cx - rx; x <= cx + rx; x++)
            if (ellipse_inside(x, y, cx, cy, rx, ry)) plot_pat(x, y, color);
}

void oval(int cx, int cy, int rx, int ry, int color) {
    PROF("oval");
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

