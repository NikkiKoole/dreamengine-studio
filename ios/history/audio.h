#ifndef DE_AUDIO_H
#define DE_AUDIO_H

// spike 2 — a stand-in synth. The real engine will swap de_audio_render's body for
// sound.h's mixer; here it's a clean arpeggio, just to prove C samples reach the iOS
// speaker via a CoreAudio render callback (AVAudioSourceNode).
void  de_audio_init(int sample_rate);
void  de_audio_render(float* out, int frames);  // mono, -1..1; called on the audio thread
float de_audio_level(void);                      // recent RMS (for a VU meter on the canvas)
#endif
