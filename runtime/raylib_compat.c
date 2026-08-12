// AUTO-GENERATED stub bodies for the DE_NO_RAYLIB shim (phase D.2).
// GPU-path / device calls that only need to LINK (never run in software mode).
// Real bodies (timing + the software text path) are hand-written at the bottom.
#include "raylib_compat.h"
#include <math.h>   // cosf/sinf — the Camera2D transforms below are REAL, not stubs
#include <stdatomic.h>   // host→engine input ring: main (producer) ↔ the thread running de_frame

double de_host_time = 0.0; float de_host_dt = 1.0f/60.0f;

void BeginDrawing(void) { }
void BeginMode2D(Camera2D camera) { }
void BeginScissorMode(int x, int y, int width, int height) { }
void BeginShaderMode(Shader shader) { }
void BeginTextureMode(RenderTexture2D target) { }
void ClearBackground(Color color) { }
void CloseAudioDevice(void) { }
void CloseWindow(void) { }
void DrawCircle(int centerX, int centerY, float radius, Color color) { }
void DrawCircleLines(int centerX, int centerY, float radius, Color color) { }
void DrawCircleV(Vector2 center, float radius, Color color) { }
void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color) { }
void DrawPixel(int posX, int posY, Color color) { }
void DrawRectangle(int posX, int posY, int width, int height, Color color) { }
void DrawRectangleLines(int posX, int posY, int width, int height, Color color) { }
void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color) { }
void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint) { }
void DrawTextPro(Font font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float spacing, Color tint) { }
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint) { }
void EndDrawing(void) { }
void EndMode2D(void) { }
void EndScissorMode(void) { }
void EndShaderMode(void) { }
void EndTextureMode(void) { }
bool ExportImage(Image image, const char *fileName) { return 0; }
int GetCharPressed(void) { return 0; }
// real FPS from the host clock (de_host_time). Averaged over a ≥0.5s window and refreshed each
// window, like raylib's GetFPS — "holds at 60, sags under load." Updated in de_input_endframe().
static int    de_fps = 0;
static double de_fps_winstart = 0;
static int    de_fps_frames = 0;
int GetFPS(void) { return de_fps; }
// touch → MOUSE synthesis. A touch device has no mouse, but a huge class of carts read the mouse
// (mouse_x/mouse_pressed/...). The PRIMARY finger (the first one down) drives the mouse API, exactly
// as a browser/OS synthesizes mouse events from touch — so mouse-driven carts play on iOS, and the
// headless harness can drive them by injecting de_touch_*. (Desktop does the reverse, mouse→touch.)
static float de_mouse_x = 0, de_mouse_y = 0;     // primary-finger position, framebuffer px (SCALE=1 on iOS)
static bool  de_mouse_down = false, de_mouse_prev = false;   // current + last-frame button state (edge detect)
static int   de_mouse_id = -999;                 // touch id currently driving the mouse (-999 = none)
#define DE_NKEY 512
static unsigned char de_key_now[DE_NKEY], de_key_was[DE_NKEY];   // key state: current + last frame
Vector2 GetMousePosition(void) { Vector2 r = { de_mouse_x, de_mouse_y }; return r; }
// snapshot button + key state at FRAME END (de_frame), so an event arriving before the next frame
// reads as IsMouseButtonPressed/IsKeyPressed for exactly one frame — matching raylib's PollInputEvents
// prev/current copy.
void de_input_endframe(void) {
    de_mouse_prev = de_mouse_down;
    for (int i = 0; i < DE_NKEY; i++) de_key_was[i] = de_key_now[i];
    double now = GetTime();   // == de_host_time, the real host clock (synthetic 1/60 on the headless harness)
    // GetFrameTime: the real per-frame delta (was a fixed-1/60 stub → delta-timed carts ignored drops).
    // Deterministic on the synthetic clock (steps are exactly 1/60); clamp guards the first frame / hitches.
    static double de_dt_last = 0;
    if (de_dt_last > 0) { float dt = (float)(now - de_dt_last); if (dt > 0 && dt < 0.25f) de_host_dt = dt; }
    de_dt_last = now;
    // FPS: count frames per ≥0.5s window (raylib-style — holds at 60, sags under load).
    if (de_fps_winstart == 0) de_fps_winstart = now;
    de_fps_frames++;
    double el = now - de_fps_winstart;
    if (el >= 0.5) { de_fps = (int)(de_fps_frames / el + 0.5); de_fps_frames = 0; de_fps_winstart = now; }
}
float GetMouseWheelMove(void) { return 0; }
Vector2 GetMouseWheelMoveV(void) { Vector2 v = {0, 0}; return v; }   // no-Raylib host: no wheel
// real: rnd()/rnd_float()/shake and procedural carts need varied values (a 0-stub
// collapses positions/cameras). Deterministic LCG — NOT Raylib's exact sequence, so
// a no-Raylib render won't be pixel-identical to a seeded Raylib run, just sane.
static unsigned int de_rng_state = 0x2545F491u;
int GetRandomValue(int min, int max) {
    if (max < min) { int t = min; min = max; max = t; }
    de_rng_state = de_rng_state * 1103515245u + 12345u;
    unsigned int r = (de_rng_state >> 16) & 0x7fffu;
    return min + (int)(r % (unsigned int)(max - min + 1));
}
// NOT a stub: poly_clamp_scan derives the visible-world scan box from this — a {0,0}
// stub collapsed every poly/ngon/star/tritex scan box to nothing (invisible geometry on
// iOS; found via infiniminer). mouse_world_x/y ride it too. Raylib rcore math, inverted:
// world = R(-rot) · ((screen − offset) / zoom) + target.
Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera) {
    float rad = camera.rotation * (3.14159265358979323846f / 180.0f);
    float c = cosf(rad), s = sinf(rad);
    float z = (camera.zoom != 0.0f) ? camera.zoom : 1.0f;
    float dx = (position.x - camera.offset.x) / z;
    float dy = (position.y - camera.offset.y) / z;
    Vector2 r = {  dx * c + dy * s + camera.target.x,
                  -dx * s + dy * c + camera.target.y };
    return r;
}
int GetShaderLocation(Shader shader, const char *uniformName) { return 0; }
// touch is fed by the host via de_touch_begin/moved/ended (platform.h). The engine's
// input layer polls these once per frame (studio.c, vt_pos = GetTouchPosition(i)). We
// store positions in WINDOW pixels, which touch_x()/touch_y() divide by SCALE — so an
// iOS build (SCALE=1) gets framebuffer coords straight through (see de_touch_* below).
#define DE_MAX_TOUCH 16
typedef struct { int id; float x, y; bool active; } DeTouchPoint;
static DeTouchPoint de_touch[DE_MAX_TOUCH];
int GetTouchPointCount(void) {
    int n = 0;
    for (int i = 0; i < DE_MAX_TOUCH; i++) if (de_touch[i].active) n++;
    return n;
}
static int de_touch_nth(int index) {   // index over the ACTIVE points (compact view)
    int n = 0;
    for (int i = 0; i < DE_MAX_TOUCH; i++) if (de_touch[i].active) { if (n == index) return i; n++; }
    return -1;
}
int GetTouchPointId(int index) { int s = de_touch_nth(index); return s < 0 ? 0 : de_touch[s].id; }
Vector2 GetTouchPosition(int index) {
    int s = de_touch_nth(index);
    if (s < 0) { Vector2 z = {0}; return z; }
    Vector2 r = { de_touch[s].x, de_touch[s].y };
    return r;
}
// NOT a stub (see GetScreenToWorld2D): the forward Camera2D transform, raylib rcore math:
// screen = R(rot) · (world − target) · zoom + offset.
Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera) {
    float rad = camera.rotation * (3.14159265358979323846f / 180.0f);
    float c = cosf(rad), s = sinf(rad);
    float dx = position.x - camera.target.x, dy = position.y - camera.target.y;
    Vector2 r = { (dx * c - dy * s) * camera.zoom + camera.offset.x,
                  (dx * s + dy * c) * camera.zoom + camera.offset.y };
    return r;
}
void HideCursor(void) { }
void ImageColorReplace(Image *image, Color color, Color replace) { }
Image ImageCopy(Image image) { Image r = {0}; return r; }
void ImageCrop(Image *image, Rectangle crop) { (void)image; (void)crop; }   // harness-only (--resize); unused on device
void ImageFlipVertical(Image *image) { }
void ImageFormat(Image *image, int newFormat) { }
void InitAudioDevice(void) { }
void InitWindow(int width, int height, const char *title) { }
bool IsFileDropped(void) { return 0; }
FilePathList LoadDroppedFiles(void) { FilePathList r = {0}; return r; }
// keyboard: fed by the host via de_key_event (a future on-screen keyboard; the headless harness
// today). Same prev/current edge model as the mouse — de_input_endframe() snapshots prev each frame.
// (de_key_now/was + DE_NKEY declared up with the mouse state, so de_input_endframe can see them.)
bool IsKeyDown(int key)     { return (unsigned)key < DE_NKEY &&  de_key_now[key]; }
bool IsKeyPressed(int key)  { return (unsigned)key < DE_NKEY &&  de_key_now[key] && !de_key_was[key]; }
bool IsKeyReleased(int key) { return (unsigned)key < DE_NKEY && !de_key_now[key] &&  de_key_was[key]; }
// de_key_event itself lives down with the input RING: like touch, it is called from the host's
// thread, so it appends an event rather than writing de_key_now directly.
bool IsMouseButtonDown(int button)     { return button == MOUSE_BUTTON_LEFT &&  de_mouse_down; }
bool IsMouseButtonPressed(int button)  { return button == MOUSE_BUTTON_LEFT &&  de_mouse_down && !de_mouse_prev; }
bool IsMouseButtonReleased(int button) { return button == MOUSE_BUTTON_LEFT && !de_mouse_down &&  de_mouse_prev; }
AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize, unsigned int channels) { AudioStream r = {0}; return r; }
Font LoadFontFromImage(Image image, Color key, int firstChar) { Font r = {0}; return r; }
Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize) { Image r = {0}; return r; }
Image LoadImageFromTexture(Texture2D texture) { Image r = {0}; return r; }
Image LoadImageFromScreen(void) { Image r = {0}; return r; }
RenderTexture2D LoadRenderTexture(int width, int height) { RenderTexture2D r = {0}; return r; }
Shader LoadShaderFromMemory(const char *vsCode, const char *fsCode) { Shader r = {0}; return r; }
Texture2D LoadTextureFromImage(Image image) { Texture2D r = {0}; return r; }
// real: text_width()/centering/clip layout depend on this. Mirrors sw_print's
// advance (advanceX, or recs.width when 0) so measured width == drawn width.
Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing) {
    if (!font.glyphs || !font.recs || font.glyphCount <= 0) return (Vector2){0, 0};   // unloaded font → don't deref NULL glyph/rec arrays
    float scale = (font.baseSize > 0) ? fontSize / (float)font.baseSize : 1.0f;
    float w = 0, maxw = 0; int lines = 1;
    for (int i = 0; text[i]; ) {
        int sz; int cp = GetCodepointNext(&text[i], &sz); i += sz;
        if (cp == '\n') { if (w > maxw) maxw = w; w = 0; lines++; continue; }
        int gi = GetGlyphIndex(font, cp);
        int adv = font.glyphs[gi].advanceX; if (adv == 0) adv = (int)font.recs[gi].width;
        w += (float)adv + spacing;
    }
    if (w > maxw) maxw = w;
    Vector2 r = { maxw * scale, (float)lines * fontSize };
    return r;
}
void PlayAudioStream(AudioStream stream) { }
void SetAudioStreamBufferSizeDefault(int size) { }
void SetAudioStreamCallback(AudioStream stream, AudioCallback callback) { }
void SetExitKey(int key) { }
void SetMasterVolume(float volume) { }
void SetMouseCursor(int cursor) { }
void SetRandomSeed(unsigned int seed) { de_rng_state = seed ? seed : 1u; }
void SetShaderValue(Shader shader, int locIndex, const void *value, int uniformType) { }
void SetShaderValueV(Shader shader, int locIndex, const void *value, int uniformType, int count) { }
void SetShaderValueTexture(Shader shader, int locIndex, Texture2D texture) { }
void SetTargetFPS(int fps) { }
void SetTextureFilter(Texture2D texture, int filter) { }
void SetTextureWrap(Texture2D texture, int wrap) { }
void SetTraceLogLevel(int logLevel) { }
void SetWindowState(unsigned int flags) { }
extern int de_screen_w(void); extern int de_screen_h(void);   // defined in studio.c (DE_NO_RAYLIB)
int  GetScreenWidth(void)  { return de_screen_w(); }          // no OS window → the active canvas IS the screen
int  GetScreenHeight(void) { return de_screen_h(); }
bool IsWindowState(unsigned int flags) { (void)flags; return false; }   // no window states on the software build
void SetWindowSize(int width, int height) { (void)width; (void)height; }
void ShowCursor(void) { }
void UnloadAudioStream(AudioStream stream) { }
void UnloadDroppedFiles(FilePathList files) { }
void UnloadFont(Font font) { }
void UnloadImage(Image image) { }
void UnloadRenderTexture(RenderTexture2D target) { }
void UnloadShader(Shader shader) { }
void UnloadTexture(Texture2D texture) { }
void UpdateTexture(Texture2D texture, const void *pixels) { }
bool WindowShouldClose(void) { return 0; }
void rlBegin(int mode) { }
void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { }
void rlEnd(void) { }
unsigned int rlGetTextureIdDefault(void) { return 0; }
void rlSetTexture(unsigned int id) { }
void rlTexCoord2f(float x, float y) { }
void rlVertex2f(float x, float y) { }

// ---- real bodies ----
double GetTime(void)      { return de_host_time; }
float  GetFrameTime(void) { return de_host_dt; }
// software text path: real glyph lookup + atlas read (sw_print needs these).
int GetCodepointNext(const char *text, int *codepointSize) { *codepointSize = 1; return (unsigned char)text[0]; } // ASCII; UTF-8 TODO
int GetGlyphIndex(Font font, int codepoint) {
    for (int i = 0; i < font.glyphCount; i++) if (font.glyphs[i].value == codepoint) return i;
    return 0;
}
Color GetImageColor(Image image, int x, int y) {
    if (!image.data || x < 0 || y < 0 || x >= image.width || y >= image.height) { Color z = {0,0,0,0}; return z; }
    uint32_t p = ((const uint32_t*)image.data)[y*image.width + x];
    Color c = { (unsigned char)(p & 0xff), (unsigned char)((p>>8)&0xff), (unsigned char)((p>>16)&0xff), (unsigned char)((p>>24)&0xff) };
    return c;
}

// ---- touch input seam (platform.h) ----
// The host (iOS CanvasView, UIKit touches) feeds contacts in framebuffer pixels; the
// engine reads them next frame via GetTouchPointCount/GetTouchPosition above. Keyed by
// `id` so multitouch tracks per finger. A begin on a live id just updates it.
//
// ══ THE HOST→ENGINE EVENT RING ══════════════════════════════════════════════════════════════
// The host thread does NOT touch this state. It appends events; the engine applies them at the
// top of the frame (de_input_beginframe). That indirection exists because of the AUv3:
//
//   · standalone app  — UIKit delivers touches on the main thread, and de_frame() runs there too
//                       (CADisplayLink). Same thread, so direct writes were always safe.
//   · AUv3 plug-in    — de_frame() is driven by the render block, i.e. on the AUDIO THREAD, while
//                       touches still arrive on the main thread. Direct writes are a data race.
//
// Not a theoretical one. The pool is walked by index (de_touch_nth compacts over `active`), so a
// finger released between a cart's GetTouchPointCount() and its GetTouchPosition(i) shifted the
// compact view underneath the loop and ui.h hit-tested a stale slot — a tap landing on the wrong
// widget, with no error anywhere. An x could also be published from one event and its y from the
// next, putting a contact on a diagonal it never crossed.
//
// Atomic acquire/release, the same idiom as sound.h's request queue: the consumer must see a
// fully-written entry before it sees the advanced index, and `volatile` alone is NOT a barrier.
// See design/audio-threading.md. Latency is unchanged in the case that matters — an event arriving
// mid-frame was already only visible to the NEXT frame; now that is true consistently.
#define DE_IN_RING 512
#define DE_IN_RESERVE 48   // slots only a RELEASE may spend (> DE_MAX_TOUCH + held keys) — see de_in_push
enum { DE_IN_TOUCH_BEGIN = 1, DE_IN_TOUCH_MOVED, DE_IN_TOUCH_ENDED, DE_IN_KEY };
typedef struct { int kind, id; float x, y; } DeInEvent;
static DeInEvent  de_in_ring[DE_IN_RING];
static atomic_int de_in_head = 0;      // producer: the host's input thread (main, on iOS)
static atomic_int de_in_tail = 0;      // consumer: the engine, in de_input_beginframe
static atomic_int de_in_dropped = 0;   // ring full: the host outran the frame rate by 512 events

static void de_in_push(int kind, int id, float x, float y) {
    int h = atomic_load_explicit(&de_in_head, memory_order_relaxed);   // producer owns head
    int t = atomic_load_explicit(&de_in_tail, memory_order_acquire);
    int next = (h + 1) % DE_IN_RING;
    // Under pressure, WHICH event gets dropped is the whole design. A ring only fills when nobody is
    // draining it (a host that paused our render block, a stalled frame), and the three kinds are not
    // equally important:
    //   · a lost MOVE is invisible — the next move re-states the position
    //   · a lost BEGIN is a tap that never happened: annoying, self-correcting
    //   · a lost ENDED (or key-up) is a finger held down FOREVER: a stuck note, a knob that keeps
    //     tracking, a key that repeats. It never self-corrects, and the probe caught exactly this.
    // So: shed moves first, then begins, and keep a reserve that ONLY lifts may spend. With the
    // reserve larger than DE_MAX_TOUCH plus any plausible number of held keys, every input that is
    // currently down can always enqueue its release. That is a guarantee, not a hope.
    int depth = (h - t + DE_IN_RING) % DE_IN_RING;
    int shed = (kind == DE_IN_TOUCH_MOVED && depth > DE_IN_RING * 3 / 4) ||
               (kind == DE_IN_TOUCH_BEGIN && depth > DE_IN_RING - DE_IN_RESERVE);
    if (shed || next == t) {   // shed by policy, or genuinely full
        atomic_fetch_add_explicit(&de_in_dropped, 1, memory_order_relaxed); return;
    }
    de_in_ring[h].kind = kind; de_in_ring[h].id = id; de_in_ring[h].x = x; de_in_ring[h].y = y;
    atomic_store_explicit(&de_in_head, next, memory_order_release);   // publish AFTER the writes
}

// The host side: append only. Cheap enough to call from a UIKit touch handler with 10 fingers.
void de_touch_begin(int id, float x, float y) { de_in_push(DE_IN_TOUCH_BEGIN, id, x, y); }
void de_touch_moved(int id, float x, float y) { de_in_push(DE_IN_TOUCH_MOVED, id, x, y); }
void de_touch_ended(int id, float x, float y) { de_in_push(DE_IN_TOUCH_ENDED, id, x, y); }
void de_key_event(int key, int down)          { de_in_push(DE_IN_KEY, key, down ? 1.0f : 0.0f, 0); }

// The engine side: everything below runs on the thread that calls de_frame, and owns the pool.
static DeTouchPoint *de_touch_find(int id) {
    for (int i = 0; i < DE_MAX_TOUCH; i++) if (de_touch[i].active && de_touch[i].id == id) return &de_touch[i];
    return 0;
}
static void de_apply_touch_begin(int id, float x, float y) {
    DeTouchPoint *p = de_touch_find(id);
    if (!p) for (int i = 0; i < DE_MAX_TOUCH; i++) if (!de_touch[i].active) { p = &de_touch[i]; break; }
    if (!p) return;                       // pool full — drop the contact
    p->id = id; p->x = x; p->y = y; p->active = true;
    if (de_mouse_id == -999) {            // first finger down → it drives the mouse (left button)
        de_mouse_id = id; de_mouse_x = x; de_mouse_y = y; de_mouse_down = true;
    }
}
static void de_apply_touch_moved(int id, float x, float y) {
    DeTouchPoint *p = de_touch_find(id);
    if (p) { p->x = x; p->y = y; }
    if (id == de_mouse_id) { de_mouse_x = x; de_mouse_y = y; }
}
static void de_apply_touch_ended(int id, float x, float y) {
    DeTouchPoint *p = de_touch_find(id);
    if (p) { p->x = x; p->y = y; p->active = false; }
    if (id == de_mouse_id) { de_mouse_x = x; de_mouse_y = y; de_mouse_down = false; de_mouse_id = -999; }
}

// Called at the TOP of de_frame (studio.c), before the cart reads any input.
void de_input_beginframe(void) {
    // What went DOWN during this drain: touch ids, and key codes. Both need the rule below.
    int began[DE_MAX_TOUCH], nbegan = 0;
    int keyed[16], nkeyed = 0;
    for (;;) {
        int t = atomic_load_explicit(&de_in_tail, memory_order_relaxed);   // consumer owns tail
        if (t == atomic_load_explicit(&de_in_head, memory_order_acquire)) break;   // empty
        DeInEvent e = de_in_ring[t];
        // ── THE ONE-FRAME PRESS RULE ────────────────────────────────────────────────────────
        // A release that ends a press applied in this same drain is left for NEXT frame. A quick
        // tap can otherwise begin and end between two frames, and applying both here would set
        // `active` and clear it again before the cart ever looked: the contact never existed, and
        // de_mouse_down goes true→false inside one frame so IsMouseButtonPressed and
        // IsMouseButtonReleased are BOTH never true. That is the oldest "my tap did nothing" bug
        // there is, and on a phone it is common — 60 Hz is 16ms and fingers are faster than that.
        // Deferring cannot starve: the press was consumed, so every drain makes progress.
        //
        // KEYS NEED IT TOO, which is not obvious and cost a red check to notice: a host that
        // synthesises a keystroke as down-then-up back to back (an on-screen keyboard, a test
        // harness, a MIDI-to-key bridge) hits precisely the same window, and IsKeyPressed never
        // fires. Same rule, separate list.
        if (e.kind == DE_IN_TOUCH_ENDED) {
            int fresh = 0;
            for (int i = 0; i < nbegan; i++) if (began[i] == e.id) { fresh = 1; break; }
            if (fresh) break;
        }
        if (e.kind == DE_IN_KEY && e.x <= 0.5f) {
            int fresh = 0;
            for (int i = 0; i < nkeyed; i++) if (keyed[i] == e.id) { fresh = 1; break; }
            if (fresh) break;
        }
        switch (e.kind) {
            case DE_IN_TOUCH_BEGIN: de_apply_touch_begin(e.id, e.x, e.y);
                                    if (nbegan < DE_MAX_TOUCH) began[nbegan++] = e.id; break;
            case DE_IN_TOUCH_MOVED: de_apply_touch_moved(e.id, e.x, e.y); break;
            case DE_IN_TOUCH_ENDED: de_apply_touch_ended(e.id, e.x, e.y); break;
            case DE_IN_KEY:
                if ((unsigned)e.id < DE_NKEY) de_key_now[e.id] = e.x > 0.5f ? 1 : 0;
                if (e.x > 0.5f && nkeyed < 16) keyed[nkeyed++] = e.id;
                break;
        }
        atomic_store_explicit(&de_in_tail, (t + 1) % DE_IN_RING, memory_order_release);
    }
}
