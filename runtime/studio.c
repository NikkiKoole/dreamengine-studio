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
#include "sprites_data.h"
#include "map_data.h"
#include "sound.h"

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

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
// internal patterned-fill helpers — the public fills call these when fillp() is on
static void rectfill_pat(int x, int y, int w, int h, int pattern, int c1, int c0);
static void circfill_pat(int x, int y, int radius, int pattern, int c1, int c0);
static void ovalfill_pat(int cx, int cy, int rx, int ry, int pattern, int c1, int c0);
static void trifill_pat(int x1, int y1, int x2, int y2, int x3, int y3, int pattern, int c1, int c0);
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
    sound_tick(GetFrameTime());
#else
    sound_tick(GetFrameTime());
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
    // delta time for dt() — clamped so a stalled frame can't teleport things
    { double tn = GetTime(); frame_dt = (float)(tn - last_time); last_time = tn;
      if (frame_dt > 0.1f) frame_dt = 0.1f; if (frame_dt < 0) frame_dt = 0; }
    // characters typed this frame for text_input()
    { int n = 0, ch; while ((ch = GetCharPressed()) != 0 && n < 31) text_buf[n++] = (char)ch; text_buf[n] = 0; }
    update();
    frame_count++;

    // draw into the low-res canvas, under the camera matrix (handles scroll + zoom +
    // rotation on the GPU). camera()/camera_ex() called inside draw() re-apply via
    // cam_reapply() while cam_active is true.
    BeginTextureMode(canvas);
        BeginMode2D(cam);
        cam_active = true;
        draw();
        cam_active = false;
        EndMode2D();
    EndTextureMode();
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

    age_watches();   // frame-end: expire watches whose branch stopped firing
}

int main(int argc, char **argv) {
    const char *window_title           = "dreamengine";
#ifndef PLATFORM_WEB
    int         screenshot_mode        = 0;
    int         screenshot_frames_done = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0) screenshot_mode = 1;
        else if (strcmp(argv[i], "--title") == 0 && i + 1 < argc) window_title = argv[++i];
    }
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
    if (screenshot_mode) SetWindowState(FLAG_WINDOW_HIDDEN);
#endif
    InitWindow(SCREEN_W * SCALE, SCREEN_H * SCALE, window_title);
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

    if (SPRITES_DATA_LEN > 0) {
        Image img = LoadImageFromMemory(".png", SPRITES_DATA, SPRITES_DATA_LEN);
        if (img.width > 0) {
            spritesheet_img = img;
            spritesheet = LoadTextureFromImage(img);
            SetTextureFilter(spritesheet, TEXTURE_FILTER_POINT);
        }
    }

#ifndef PLATFORM_WEB
    init();
#endif

    last_time = GetTime();   // seed dt()

#ifdef PLATFORM_WEB
    emscripten_set_main_loop(loop_step, 0, 1);
#else
    while (!WindowShouldClose()) {
        loop_step();
        if (screenshot_mode && ++screenshot_frames_done >= 3) break;
    }

    // save last frame as screenshot.png so the cart PNG thumbnail shows the game
    Image screenshot = LoadImageFromTexture(canvas.texture);
    ImageFlipVertical(&screenshot);
    ExportImage(screenshot, "screenshot.png");
    UnloadImage(screenshot);

    if (custom_font) UnloadFont(game_font);
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
            case BTN_UP:    return IsKeyDown(KEY_W);
            case BTN_DOWN:  return IsKeyDown(KEY_S);
            case BTN_LEFT:  return IsKeyDown(KEY_A);
            case BTN_RIGHT: return IsKeyDown(KEY_D);
            case BTN_A:     return IsKeyDown(KEY_Z);
            case BTN_B:     return IsKeyDown(KEY_X);
        }
    } else if (player == 1) {
        switch (button) {
            case BTN_UP:    return IsKeyDown(KEY_UP);
            case BTN_DOWN:  return IsKeyDown(KEY_DOWN);
            case BTN_LEFT:  return IsKeyDown(KEY_LEFT);
            case BTN_RIGHT: return IsKeyDown(KEY_RIGHT);
            case BTN_A:     return IsKeyDown(KEY_COMMA);
            case BTN_B:     return IsKeyDown(KEY_PERIOD);
        }
    }
    return false;
}

bool btnp(int player, int button) {
    if (player < 0 || player > 1) return false;
    if (button < 0 || button >= BTN_COUNT) return false;
    return btn_curr[player][button] && !btn_prev[player][button];
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
int mouse_x(void) {
    int x = (int)(GetMousePosition().x / SCALE);
    return mid(0, x, SCREEN_W - 1);
}
int mouse_y(void) {
    int y = (int)(GetMousePosition().y / SCALE);
    return mid(0, y, SCREEN_H - 1);
}
bool mouse_down(int button)     { return IsMouseButtonDown(raylib_mouse_button(button)); }
bool mouse_pressed(int button)  { return IsMouseButtonPressed(raylib_mouse_button(button)); }
bool mouse_released(int button) { return IsMouseButtonReleased(raylib_mouse_button(button)); }
float mouse_wheel(void)         { return GetMouseWheelMove(); }
int mouse_world_x(void)         { return (int)GetScreenToWorld2D((Vector2){ (float)mouse_x(), (float)mouse_y() }, cam).x; }
int mouse_world_y(void)         { return (int)GetScreenToWorld2D((Vector2){ (float)mouse_x(), (float)mouse_y() }, cam).y; }

void cls(int color) {
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
    sprf(index, x, y, false, false);
}

void sprf(int index, int x, int y, bool flip_x, bool flip_y) {
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
    if (spritesheet.width == 0) return;
    Rectangle src = { sx, sy, sw, sh };
    Rectangle dst = { dx, dy, dw, dh };
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    pal_end();
}

void spr_rot(int index, int x, int y, float deg) {
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
    if (spritesheet.width == 0) return;
    Rectangle src = { sx, sy, sw, sh };
    Rectangle dst = { dx + ox, dy + oy, dw, dh };               // pivot (ox,oy) is in dest space, relative to (dx,dy)
    pal_begin();
    DrawTexturePro(spritesheet, src, dst, (Vector2){ (float)ox, (float)oy }, deg, WHITE);
    pal_end();
}

void print(const char *text, int x, int y, int color) {
    DrawTextEx(game_font, text, (Vector2){ x, y }, 8, 0, palette[color % PALETTE_SIZE]);
}

void rect(int x, int y, int w, int h, int color) {
    Color c = palette[color % PALETTE_SIZE];
    int rx = x, ry = y;
    // 1px DrawRectangle slices — no line caps, exact pixel coverage
    DrawRectangle(rx,     ry,     w,   1,   c);  // top
    DrawRectangle(rx,     ry+h-1, w,   1,   c);  // bottom
    DrawRectangle(rx,     ry+1,   1,   h-2, c);  // left
    DrawRectangle(rx+w-1, ry+1,   1,   h-2, c);  // right
}

void rectfill(int x, int y, int w, int h, int color) {
    if (fp_on) { rectfill_pat(x, y, w, h, fp_global, fp_hole, color); return; }   // 1-bits = holes, 0-bits = color
    DrawRectangle(x, y, w, h, palette[color % PALETTE_SIZE]);
}

void bar(int x, int y, int w, int h, float pct, int fill, int bg) {
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
    // src origin = world pos, so the 4×4 lattice tiles in world space (sticks to what
    // you draw) and stays seamless with the circ/tri pattern fills (same world origin).
    // The camera matrix then scales/rotates the whole quad.
    Rectangle src = { (float)x, (float)y, (float)w, (float)h };
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

static void trifill_pat(int x1, int y1, int x2, int y2, int x3, int y3, int pattern, int c1, int c0) {
    int t;                                            // sort vertices by y ascending
    if (y1 > y2) { t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; }
    if (y1 > y3) { t=x1;x1=x3;x3=t; t=y1;y1=y3;y3=t; }
    if (y2 > y3) { t=x2;x2=x3;x3=t; t=y2;y2=y3;y3=t; }
    if (y3 == y1) return;
    for (int y = y1; y <= y3; y++) {
        float xa = x1 + (float)(x3 - x1) * (y - y1) / (float)(y3 - y1);          // long edge
        float xb = (y < y2 && y2 != y1) ? x1 + (float)(x2 - x1) * (y - y1) / (float)(y2 - y1)
                 : (y2 != y3)           ? x2 + (float)(x3 - x2) * (y - y2) / (float)(y3 - y2)
                                        : (float)x2;
        int xl = (int)(xa < xb ? xa : xb), xr = (int)(xa < xb ? xb : xa);
        rectfill_pat(xl, y, xr - xl + 1, 1, pattern, c1, c0);
    }
}

// ── global fill pattern (PICO-8 fillp style) ──────────────────────────────
// when on, the normal fills draw the pattern: the draw COLOR fills the 0-bits,
// the 1-bits are transparent (holes) — exactly like PICO-8 fillp.
void fillp(int pattern, int hole_color) { fp_on = true; fp_global = pattern; fp_hole = hole_color; }
void fillp_reset(void)  { fp_on = false; }

void circ(int x, int y, int radius, int color) {
    DrawCircleLines(x, y, (float)radius, palette[color % PALETTE_SIZE]);
}

void circfill(int x, int y, int radius, int color) {
    if (fp_on) { circfill_pat(x, y, radius, fp_global, fp_hole, color); return; }
    DrawCircle(x, y, radius, palette[color % PALETTE_SIZE]);
}

void tri(int x1, int y1, int x2, int y2, int x3, int y3, int color) {
    Color c = palette[color % PALETTE_SIZE];
    DrawLine(x1, y1, x2, y2, c);
    DrawLine(x2, y2, x3, y3, c);
    DrawLine(x3, y3, x1, y1, c);
}

void trifill(int x1, int y1, int x2, int y2, int x3, int y3, int color) {
    if (fp_on) { trifill_pat(x1, y1, x2, y2, x3, y3, fp_global, fp_hole, color); return; }
    Color c = palette[color % PALETTE_SIZE];
    Vector2 v1 = {(float)x1, (float)y1};
    Vector2 v2 = {(float)x2, (float)y2};
    Vector2 v3 = {(float)x3, (float)y3};
    // Raylib needs counter-clockwise winding in Y-down screen coords.
    // In Y-down space, cross > 0 means clockwise visually → swap to fix.
    float cross = (v2.x-v1.x)*(v3.y-v1.y) - (v2.y-v1.y)*(v3.x-v1.x);
    if (cross > 0) DrawTriangle(v1, v3, v2, c);
    else           DrawTriangle(v1, v2, v3, c);
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
    DrawLine(x1, y1, x2, y2, palette[color % PALETTE_SIZE]);
}

void pset(int x, int y, int color) {
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
    return (float)GetTime();
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

float timer(void)       { return (float)(GetTime() - timer_base); }
void  timer_reset(void) { timer_base = GetTime(); }

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

const char *text_input(void) { return text_buf; }
bool key(int k)  { return IsKeyDown(k); }
bool keyp(int k) { return IsKeyPressed(k); }

void pal(int c0, int c1)  { if (c0 >= 0 && c0 < PALETTE_SIZE && c1 >= 0 && c1 < PALETTE_SIZE) { palette[c0] = base_palette[c1]; pal_recompute(); } }
void pal_reset(void)      { for (int i = 0; i < PALETTE_SIZE; i++) palette[i] = base_palette[i]; pal_recompute(); }
void fade(float a)        { fade_amt  = a < 0 ? 0 : (a > 1 ? 1 : a); }
void shake(float a)       { if (a > shake_amt) shake_amt = a; }

int  text_width(const char *t) { return (int)strlen(t) * 8; }

void print_scaled(const char *t, int x, int y, int color, int scale) {
    if (scale < 1) scale = 1;
    DrawTextEx(game_font, t, (Vector2){ (float)x, (float)y },
               8.0f * scale, 0, palette[color % PALETTE_SIZE]);
}

void ovalfill(int cx, int cy, int rx, int ry, int color) {
    if (rx < 0) rx = -rx; if (ry < 0) ry = -ry; if (ry == 0) ry = 1;
    if (fp_on) { ovalfill_pat(cx, cy, rx, ry, fp_global, fp_hole, color); return; }
    Color c = palette[color % PALETTE_SIZE];
    for (int yy = -ry; yy <= ry; yy++) {
        float f = 1.0f - (float)(yy * yy) / (float)(ry * ry);
        if (f < 0) f = 0;
        int hw = (int)(rx * sqrtf(f) + 0.5f);
        DrawRectangle(cx - hw, cy + yy, hw * 2 + 1, 1, c);
    }
}
void oval(int cx, int cy, int rx, int ry, int color) {
    if (rx < 0) rx = -rx; if (ry < 0) ry = -ry;
    Color c = palette[color % PALETTE_SIZE];
    int steps = (rx > ry ? rx : ry) * 4; if (steps < 16) steps = 16;
    int px = cx + rx, py = cy;
    for (int i = 1; i <= steps; i++) {
        float a = (float)i / steps * 6.2831853f;
        int nx = cx + (int)(rx * cosf(a) + 0.5f);
        int ny = cy + (int)(ry * sinf(a) + 0.5f);
        DrawLine(px, py, nx, ny, c);
        px = nx; py = ny;
    }
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

void print_centered(const char *text, int y, int color) {
    int w = (int)(strlen(text) * 8);
    print(text, (SCREEN_W - w) / 2, y, color);
}

void print_right(const char *text, int right_x, int y, int color) {
    int w = (int)(strlen(text) * 8);
    print(text, right_x - w, y, color);
}

// ------------------------------------------------------------
// turtle graphics
// ------------------------------------------------------------

static struct {
    float x, y;
    float heading;   // degrees: 0 = right, 90 = down
    bool  pen;
    int   color;
    int   size;
} turtle = { 0 };

static bool turtle_inited = false;

static void turtle_ensure_init(void) {
    if (!turtle_inited) {
        turtle.x       = SCREEN_W / 2.0f;
        turtle.y       = SCREEN_H / 2.0f;
        turtle.heading = 0.0f;
        turtle.pen     = false;
        turtle.color   = CLR_WHITE;
        turtle.size    = 1;
        turtle_inited  = true;
    }
}

void turtle_home(void) {
    turtle_ensure_init();
    turtle.x       = SCREEN_W / 2.0f;
    turtle.y       = SCREEN_H / 2.0f;
    turtle.heading = 0.0f;
    turtle.pen     = false;
}

void turtle_move(float steps) {
    turtle_ensure_init();
    float nx = turtle.x + steps * cosf(turtle.heading * DEG2RAD);
    float ny = turtle.y + steps * sinf(turtle.heading * DEG2RAD);
    if (turtle.pen) {
        Color c = palette[turtle.color % PALETTE_SIZE];
        if (turtle.size <= 1) {
            DrawLine((int)turtle.x, (int)turtle.y,
                     (int)nx,        (int)ny,        c);
        } else {
            DrawLineEx(
                (Vector2){ turtle.x, turtle.y },
                (Vector2){ nx,       ny       },
                (float)turtle.size, c);
        }
    }
    turtle.x = nx;
    turtle.y = ny;
}

void turtle_turn(float degrees) {
    turtle_ensure_init();
    turtle.heading += degrees;
}

void turtle_face(float degrees) {
    turtle_ensure_init();
    turtle.heading = degrees;
}

void turtle_at(int x, int y) {
    turtle_ensure_init();
    turtle.x = (float)x;
    turtle.y = (float)y;
}

void pen_down(void)          { turtle_ensure_init(); turtle.pen   = true; }
void pen_up(void)            { turtle_ensure_init(); turtle.pen   = false; }
void pen_color(int color)    { turtle_ensure_init(); turtle.color = color; }
void pen_size(int size)      { turtle_ensure_init(); turtle.size  = size > 0 ? size : 1; }
