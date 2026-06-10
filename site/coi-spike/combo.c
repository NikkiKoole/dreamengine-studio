// Stage-0 RUNTIME combo spike (see docs/design/audio-threading.md): a raylib GL window
// AND an AudioWorklet in ONE shared-memory build. Proves graphics + a real audio thread
// coexist at runtime (link was already verified). Built with the LEAN stub:
//   -sAUDIO_WORKLET=1 -sWASM_WORKERS=1 --js-library stub.js  (emscripten_thread_sleep no-op)
// If the window animates AND you hear a dead-even roll AND CPU stays low, Stage 0 is done.
#include "raylib.h"
#include <emscripten/webaudio.h>
#include <emscripten/emscripten.h>
#include <math.h>
#include <stdbool.h>

static uint8_t audioStack[8192];
static EMSCRIPTEN_WEBAUDIO_T g_ctx = 0;
static volatile int g_running = 0;   // set when the context is resumed

// ---- audio thread: a sample-counted click ROLL (even by construction) ----
#define CLICK_INTERVAL 2205
#define CLICK_LEN       800
static int g_count = 0, g_clickPos = -1;

static bool Proc(int ni, const AudioSampleFrame *in, int no, AudioSampleFrame *out,
                 int np, const AudioParamFrame *p, void *u) {
    for (int i = 0; i < 128; ++i) {
        if (g_count <= 0) { g_clickPos = 0; g_count = CLICK_INTERVAL; }
        g_count--;
        float s = 0.f;
        if (g_clickPos >= 0) {
            float env = expf(-(float)g_clickPos / 120.0f);
            s = sinf((float)g_clickPos * 0.21f) * 0.35f * env;
            if (++g_clickPos >= CLICK_LEN) g_clickPos = -1;
        }
        for (int ch = 0; ch < out[0].numberOfChannels; ++ch) out[0].data[ch*128+i] = s;
    }
    return true;
}
static void ProcCreated(EMSCRIPTEN_WEBAUDIO_T c, bool ok, void *u) {
    if (!ok) return; int cc[1] = {1};
    EmscriptenAudioWorkletNodeCreateOptions o = {.numberOfInputs=0,.numberOfOutputs=1,.outputChannelCounts=cc};
    EMSCRIPTEN_AUDIO_WORKLET_NODE_T n = emscripten_create_wasm_audio_worklet_node(c,"roll",&o,&Proc,0);
    emscripten_audio_node_connect(n, c, 0, 0);
}
static void ThreadInit(EMSCRIPTEN_WEBAUDIO_T c, bool ok, void *u) {
    if (!ok) return; WebAudioWorkletProcessorCreateOptions o = {.name="roll"};
    emscripten_create_wasm_audio_worklet_processor_async(c, &o, ProcCreated, 0);
}
EMSCRIPTEN_KEEPALIVE void resume_audio(void) {
    if (g_ctx) { emscripten_resume_audio_context_sync(g_ctx); g_running = 1; }
}

// ---- game thread: a live raylib scene so we can SEE the GL context is fine ----
static int g_frame = 0;
static void frame(void) {
    g_frame++;
    BeginDrawing();
        ClearBackground((Color){18,18,30,255});
        // a bouncing box proves the render loop is live
        int x = (int)((sinf(g_frame * 0.04f) * 0.5f + 0.5f) * 280);
        DrawRectangle(x, 90, 32, 32, (Color){90,200,120,255});
        DrawText("raylib window OK", 10, 8, 20, RAYWHITE);
        DrawText(g_running ? "audio: RUNNING (even roll = worklet OK)"
                           : "audio: tap START button below", 10, 40, 10,
                 g_running ? (Color){90,230,120,255} : (Color){255,170,80,255});
        DrawFPS(10, 170);
    EndDrawing();
}

int main(void) {
    InitWindow(320, 200, "combo");
    g_ctx = emscripten_create_audio_context(0);
    emscripten_start_wasm_audio_worklet_thread_async(
        g_ctx, audioStack, sizeof audioStack, ThreadInit, 0);
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
