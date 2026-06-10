// Throwaway AudioWorklet feasibility spike (see docs/design/audio-timing.md).
// A 440Hz sine generated on the dedicated audio-worklet thread. If you hear a
// clean tone after tapping START, the worklet path works in this context.
// Built with: emcc spike.c -sAUDIO_WORKLET=1 -sWASM_WORKERS=1 ...
#include <emscripten/webaudio.h>
#include <emscripten/emscripten.h>
#include <math.h>
#include <stdbool.h>

static uint8_t audioStack[8192];
static float   phase = 0.f;
static EMSCRIPTEN_WEBAUDIO_T g_ctx = 0;

// runs on the AUDIO THREAD — fills 128-sample render quanta
static bool ProcessAudio(int numInputs, const AudioSampleFrame *inputs,
                         int numOutputs, AudioSampleFrame *outputs,
                         int numParams, const AudioParamFrame *params, void *userData) {
    for (int i = 0; i < 128; ++i) {
        float s = sinf(phase) * 0.2f;
        phase += 2.f * 3.14159265f * 440.f / 48000.f;
        if (phase > 6.28318530f) phase -= 6.28318530f;
        for (int ch = 0; ch < outputs[0].numberOfChannels; ++ch)
            outputs[0].data[ch * 128 + i] = s;
    }
    return true;   // keep the processor alive
}

static void ProcessorCreated(EMSCRIPTEN_WEBAUDIO_T ctx, bool success, void *userData) {
    if (!success) { EM_ASM({ window.__spikeStatus && window.__spikeStatus('processor FAILED'); }); return; }
    int counts[1] = { 1 };
    EmscriptenAudioWorkletNodeCreateOptions o = {
        .numberOfInputs = 0, .numberOfOutputs = 1, .outputChannelCounts = counts
    };
    EMSCRIPTEN_AUDIO_WORKLET_NODE_T node =
        emscripten_create_wasm_audio_worklet_node(ctx, "sine", &o, &ProcessAudio, 0);
    emscripten_audio_node_connect(node, ctx, 0, 0);
    EM_ASM({ window.__spikeStatus && window.__spikeStatus('worklet ready — tap START'); });
}

static void ThreadInit(EMSCRIPTEN_WEBAUDIO_T ctx, bool success, void *userData) {
    if (!success) { EM_ASM({ window.__spikeStatus && window.__spikeStatus('audio thread FAILED'); }); return; }
    WebAudioWorkletProcessorCreateOptions opts = { .name = "sine" };
    emscripten_create_wasm_audio_worklet_processor_async(ctx, &opts, ProcessorCreated, 0);
}

// called from a user-gesture (tap) in JS → resumes the suspended context
EMSCRIPTEN_KEEPALIVE void resume_audio(void) {
    if (g_ctx) emscripten_resume_audio_context_sync(g_ctx);
}

int main(void) {
    g_ctx = emscripten_create_audio_context(0);
    emscripten_start_wasm_audio_worklet_thread_async(
        g_ctx, audioStack, sizeof(audioStack), ThreadInit, 0);
    EM_ASM({ window.__spikeStatus && window.__spikeStatus('wasm loaded (shared memory OK) — starting audio thread'); });
    return 0;
}
