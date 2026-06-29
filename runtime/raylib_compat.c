// AUTO-GENERATED stub bodies for the DE_NO_RAYLIB shim (phase D.2).
// GPU-path / device calls that only need to LINK (never run in software mode).
// Real bodies (timing + the software text path) are hand-written at the bottom.
#include "raylib_compat.h"

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
int GetFPS(void) { return 0; }
Vector2 GetMousePosition(void) { Vector2 r = {0}; return r; }
float GetMouseWheelMove(void) { return 0; }
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
Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera) { Vector2 r = {0}; return r; }
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
Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera) { Vector2 r = {0}; return r; }
void HideCursor(void) { }
void ImageColorReplace(Image *image, Color color, Color replace) { }
Image ImageCopy(Image image) { Image r = {0}; return r; }
void ImageFlipVertical(Image *image) { }
void ImageFormat(Image *image, int newFormat) { }
void InitAudioDevice(void) { }
void InitWindow(int width, int height, const char *title) { }
bool IsFileDropped(void) { return 0; }
FilePathList LoadDroppedFiles(void) { FilePathList r = {0}; return r; }
bool IsKeyDown(int key) { return 0; }
bool IsKeyPressed(int key) { return 0; }
bool IsKeyReleased(int key) { return 0; }
bool IsMouseButtonDown(int button) { return 0; }
bool IsMouseButtonPressed(int button) { return 0; }
bool IsMouseButtonReleased(int button) { return 0; }
AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize, unsigned int channels) { AudioStream r = {0}; return r; }
Font LoadFontFromImage(Image image, Color key, int firstChar) { Font r = {0}; return r; }
Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize) { Image r = {0}; return r; }
Image LoadImageFromTexture(Texture2D texture) { Image r = {0}; return r; }
RenderTexture2D LoadRenderTexture(int width, int height) { RenderTexture2D r = {0}; return r; }
Shader LoadShaderFromMemory(const char *vsCode, const char *fsCode) { Shader r = {0}; return r; }
Texture2D LoadTextureFromImage(Image image) { Texture2D r = {0}; return r; }
// real: text_width()/centering/clip layout depend on this. Mirrors sw_print's
// advance (advanceX, or recs.width when 0) so measured width == drawn width.
Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing) {
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
void SetTargetFPS(int fps) { }
void SetTextureFilter(Texture2D texture, int filter) { }
void SetTextureWrap(Texture2D texture, int wrap) { }
void SetTraceLogLevel(int logLevel) { }
void SetWindowState(unsigned int flags) { }
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
static DeTouchPoint *de_touch_find(int id) {
    for (int i = 0; i < DE_MAX_TOUCH; i++) if (de_touch[i].active && de_touch[i].id == id) return &de_touch[i];
    return 0;
}
void de_touch_begin(int id, float x, float y) {
    DeTouchPoint *p = de_touch_find(id);
    if (!p) for (int i = 0; i < DE_MAX_TOUCH; i++) if (!de_touch[i].active) { p = &de_touch[i]; break; }
    if (!p) return;                       // pool full — drop the contact
    p->id = id; p->x = x; p->y = y; p->active = true;
}
void de_touch_moved(int id, float x, float y) {
    DeTouchPoint *p = de_touch_find(id);
    if (p) { p->x = x; p->y = y; }
}
void de_touch_ended(int id, float x, float y) {
    DeTouchPoint *p = de_touch_find(id);
    if (p) { p->x = x; p->y = y; p->active = false; }
}
