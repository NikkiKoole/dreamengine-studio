#include "studio.h"
#include "raylib.h"
#include <stddef.h>

// ------------------------------------------------------------
// internal state
// ------------------------------------------------------------

#define PALETTE_SIZE 32
#define SPRITE_SIZE  16
#define SPRITE_COUNT 256
#ifndef SCALE
#define SCALE 4
#endif

static Texture2D       spritesheet;
static RenderTexture2D canvas;
static Font            game_font;
static bool            custom_font = false;
static Color           palette[PALETTE_SIZE];

// ------------------------------------------------------------
// palette — placeholder until finalised
// ------------------------------------------------------------

static void load_palette() {
    palette[0]  = (Color){ 0,   0,   0,   255 }; // CLR_BLACK
    palette[1]  = (Color){ 255, 255, 255, 255 }; // CLR_WHITE
    palette[2]  = (Color){ 204, 51,  51,  255 }; // CLR_RED
    palette[3]  = (Color){ 68,  170, 85,  255 }; // CLR_GREEN
    palette[4]  = (Color){ 51,  102, 204, 255 }; // CLR_BLUE
    // ... rest TBD
}

// ------------------------------------------------------------
// weak stub — user can omit update() and it still compiles
// ------------------------------------------------------------

__attribute__((weak)) void update(void) {}

// ------------------------------------------------------------
// main + runtime loop
// ------------------------------------------------------------

int main(void) {
    InitWindow(SCREEN_W * SCALE, SCREEN_H * SCALE, "eventually");
    InitAudioDevice();
    SetTargetFPS(60);

    load_palette();

    // low-res canvas — all drawing goes here, then scaled up
    canvas = LoadRenderTexture(SCREEN_W, SCREEN_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);

    if (FileExists("dos_8x8.png")) {
        Image fontImage = LoadImage("dos_8x8.png");
        // yellow separator lines between glyphs
        game_font   = LoadFontFromImage(fontImage, (Color){ 255, 255, 0, 255 }, 0);
        SetTextureFilter(game_font.texture, TEXTURE_FILTER_POINT);
        UnloadImage(fontImage);
        custom_font = true;
    }
    if (!custom_font) game_font = GetFontDefault();

    if (FileExists("sprites.png")) {
        spritesheet = LoadTexture("sprites.png");
        SetTextureFilter(spritesheet, TEXTURE_FILTER_POINT);
    }

    while (!WindowShouldClose()) {
        update();

        // draw into the low-res canvas
        BeginTextureMode(canvas);
            draw();
        EndTextureMode();

        // scale up to the window — RenderTexture is flipped in Y
        BeginDrawing();
            DrawTexturePro(
                canvas.texture,
                (Rectangle){ 0, 0,  SCREEN_W, -SCREEN_H },
                (Rectangle){ 0, 0,  SCREEN_W * SCALE, SCREEN_H * SCALE },
                (Vector2){ 0, 0 },
                0.0f,
                WHITE
            );
        EndDrawing();
    }

    if (custom_font) UnloadFont(game_font);
    if (spritesheet.width > 0) UnloadTexture(spritesheet);
    UnloadRenderTexture(canvas);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

// ------------------------------------------------------------
// api implementation
// ------------------------------------------------------------

bool btn(int player, int button) {
    if (player == 0) {
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

void cls(int color) {
    ClearBackground(palette[color % PALETTE_SIZE]);
}

void spr(int index, int x, int y) {
    if (spritesheet.width == 0) return;
    int cols = spritesheet.width / SPRITE_SIZE;
    Rectangle src = {
        .x      = (index % cols) * SPRITE_SIZE,
        .y      = (index / cols) * SPRITE_SIZE,
        .width  = SPRITE_SIZE,
        .height = SPRITE_SIZE,
    };
    DrawTextureRec(spritesheet, src, (Vector2){ x, y }, WHITE);
}

void print(const char *text, int x, int y, int color) {
    DrawTextEx(game_font, text, (Vector2){ x, y }, 8, 0, palette[color % PALETTE_SIZE]);
}

void rect(int x, int y, int w, int h, int color) {
    DrawRectangleLines(x, y, w, h, palette[color % PALETTE_SIZE]);
}

void rectfill(int x, int y, int w, int h, int color) {
    DrawRectangle(x, y, w, h, palette[color % PALETTE_SIZE]);
}

void circle(int x, int y, int radius, int color) {
    DrawCircleLines(x, y, (float)radius, palette[color % PALETTE_SIZE]);
}

void circlefill(int x, int y, int radius, int color) {
    DrawCircle(x, y, radius, palette[color % PALETTE_SIZE]);
}

void line(int x1, int y1, int x2, int y2, int color) {
    DrawLine(x1, y1, x2, y2, palette[color % PALETTE_SIZE]);
}

void pixel(int x, int y, int color) {
    DrawPixel(x, y, palette[color % PALETTE_SIZE]);
}
