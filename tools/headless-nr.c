// headless-nr.c — drive the DE_NO_RAYLIB engine with no OS; dump a frame + a WAV.
//
// Proves the portable software path (studio.c + raylib_compat.c stubs + baked fonts
// + stb sprites + sound_callback) renders AND sounds on desktop WITHOUT Raylib — so
// we can A/B it against the Raylib build before the iOS shell. The iOS app does the
// same: de_init → per tick de_frame(t) + blit de_framebuffer(); CoreAudio pulls
// de_audio_render() on its own thread.
//
//   build/headless-nr <frames> <out.ppm> [out.wav]
// Audio is pumped per frame (735 stereo samples @ 44100 = one 60Hz frame), mirroring
// the engine's --wav path, so the WAV lines up frame-for-frame with a Raylib --wav.

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR        44100
#define PER_FRAME 735            // stereo sample-pairs per 60Hz frame (44100/60)

static void put_u32(FILE *f, unsigned v) { fputc(v, f); fputc(v>>8, f); fputc(v>>16, f); fputc(v>>24, f); }
static void put_u16(FILE *f, unsigned v) { fputc(v, f); fputc(v>>8, f); }

int main(int argc, char **argv) {
    int frames = (argc > 1) ? atoi(argv[1]) : 1;
    const char *ppm = (argc > 2) ? argv[2] : "build/headless_nr.ppm";
    const char *wav = (argc > 3) ? argv[3] : NULL;

    de_init(DE_RENDERER_SOFTWARE);

    short *pcm = wav ? malloc((size_t)frames * PER_FRAME * 2 * sizeof(short)) : NULL;
    float scratch[PER_FRAME * 2];
    long npcm = 0;

    for (int i = 0; i < frames; i++) {
        de_frame((double)(i + 1) / 60.0);        // advances draw + the sequencer; +1 so frame 0 gets a full 1/60 dt (matches det_mode)
        if (pcm) {
            de_audio_render(scratch, PER_FRAME);  // pull one frame of audio (host audio-thread's job)
            for (int s = 0; s < PER_FRAME * 2; s++) {
                float v = scratch[s]; if (v > 1) v = 1; if (v < -1) v = -1;
                pcm[npcm++] = (short)(v * 32767.0f);
            }
        }
    }

    // ---- frame → PPM (sw_cbuf is bottom-up; flip for a normal image) ----
    const uint32_t *fb = de_framebuffer();
    int w = de_screen_w(), h = de_screen_h();
    FILE *f = fopen(ppm, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", ppm); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--)
        for (int x = 0; x < w; x++) {
            uint32_t px = fb[y * w + x];
            unsigned char rgb[3] = { (unsigned char)(px & 0xff), (unsigned char)((px >> 8) & 0xff), (unsigned char)((px >> 16) & 0xff) };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);

    // ---- audio → 16-bit stereo WAV ----
    if (wav) {
        FILE *a = fopen(wav, "wb");
        if (a) {
            long bytes = npcm * sizeof(short);
            fwrite("RIFF", 1, 4, a); put_u32(a, 36 + bytes); fwrite("WAVE", 1, 4, a);
            fwrite("fmt ", 1, 4, a); put_u32(a, 16); put_u16(a, 1); put_u16(a, 2);
            put_u32(a, SR); put_u32(a, SR * 2 * 2); put_u16(a, 2 * 2); put_u16(a, 16);
            fwrite("data", 1, 4, a); put_u32(a, bytes);
            fwrite(pcm, sizeof(short), npcm, a);
            fclose(a);
        }
        free(pcm);
        fprintf(stderr, "headless-nr: %d frame(s), %dx%d -> %s ; %ld samples -> %s\n", frames, w, h, ppm, npcm, wav);
    } else {
        fprintf(stderr, "headless-nr: %d frame(s), %dx%d -> %s\n", frames, w, h, ppm);
    }
    return 0;
}
