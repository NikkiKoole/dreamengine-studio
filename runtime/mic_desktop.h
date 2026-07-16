#ifndef DE_MIC_DESKTOP_H
#define DE_MIC_DESKTOP_H
// ============================================================================
// mic_desktop.h — the DESKTOP host's microphone capture for the raylib build
// (macOS dev machine). CoreAudio AudioQueue input → de_audio_input(). This is
// device code, so it lives in the HOST, never the engine core (the platform.h
// contract: the engine only analyzes pushed frames — see mic.h). It mirrors how
// the iOS host will capture via AVAudioSession and the web host via getUserMedia.
//
// Apple-only. On other desktop OSes mic_desktop_poll() is a no-op and the mic
// simply stays inactive (the SHIPPING targets are iOS + web; this is the
// dev-machine convenience path so a cart's mic works in the editor / play.js).
//
// Opening an input AudioQueue triggers the macOS microphone permission prompt
// (TCC) the first time, attributed to the running app — the same grant the
// mic-spike surfaced. Capture opens lazily, only once a cart calls mic_start().
// ============================================================================

#include "platform.h"   // de_mic_wanted / de_audio_input / de_mic_set_active

#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>

#define MICD_SR         44100   // request mono float @ engine rate; the queue converts from the device
#define MICD_NBUF       3       // triple-buffered capture
#define MICD_BUFFRAMES  1024    // ~23ms per buffer

static AudioQueueRef        micd_q = NULL;
static AudioQueueBufferRef  micd_bufs[MICD_NBUF];
static int                  micd_running = 0;

static void micd_cb(void *user, AudioQueueRef q, AudioQueueBufferRef buf,
                    const AudioTimeStamp *ts, UInt32 nPackets,
                    const AudioStreamPacketDescription *pd) {
    (void)user; (void)ts; (void)nPackets; (void)pd;
    int n = (int)(buf->mAudioDataByteSize / sizeof(float));
    if (n > 0) de_audio_input((const float *)buf->mAudioData, n, MICD_SR);   // mono float frames
    if (micd_running) AudioQueueEnqueueBuffer(q, buf, 0, NULL);              // recycle the buffer
}

static void micd_start(void) {
    AudioStreamBasicDescription fmt = {0};
    fmt.mSampleRate       = MICD_SR;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    fmt.mBitsPerChannel   = 32;
    fmt.mChannelsPerFrame = 1;
    fmt.mFramesPerPacket  = 1;
    fmt.mBytesPerFrame    = 4;
    fmt.mBytesPerPacket   = 4;
    if (AudioQueueNewInput(&fmt, micd_cb, NULL, NULL, NULL, 0, &micd_q) != noErr) {
        micd_q = NULL; return;   // no device / denied at create — stay inactive
    }
    micd_running = 1;
    for (int i = 0; i < MICD_NBUF; i++) {
        AudioQueueAllocateBuffer(micd_q, MICD_BUFFRAMES * sizeof(float), &micd_bufs[i]);
        AudioQueueEnqueueBuffer(micd_q, micd_bufs[i], 0, NULL);
    }
    if (AudioQueueStart(micd_q, NULL) != noErr) {
        micd_running = 0; AudioQueueDispose(micd_q, true); micd_q = NULL; return;
    }
    de_mic_set_active(1);   // NB: on macOS the queue may start yet deliver silence if permission is
                            // later denied — mic_active() reports open; mic_level() tells you it's silent.
}

static void micd_stop(void) {
    if (micd_q) { AudioQueueStop(micd_q, true); AudioQueueDispose(micd_q, true); micd_q = NULL; }
    micd_running = 0;
    de_mic_set_active(0);
}

// Called once per frame by the raylib host loop. Opens/closes the device lazily so
// it tracks the cart's mic_start()/mic_stop() (de_mic_wanted).
static inline void mic_desktop_poll(void) {
    int want = de_mic_wanted();
    if      (want  && !micd_running) micd_start();
    else if (!want &&  micd_running) micd_stop();
}

#else   // non-Apple desktop — mic stays inactive (shipping targets are iOS + web)
static inline void mic_desktop_poll(void) { }
#endif

#endif // DE_MIC_DESKTOP_H
