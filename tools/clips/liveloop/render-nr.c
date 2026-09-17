/* DE_NO_RAYLIB scripted WAV (+ optional PPM) host for liveloop.
 *
 * Linux proof path when there is no Raylib / no xxd / no window.
 * Twin of tools/clips/cloudhold/render-nr.c, plus a synthetic mic:
 * de_mic_set_active(1) and de_audio_input() each frame so SPACE/REC
 * exercises the real mic_record → sample_load freeze path.
 *
 *   bash tools/clips/liveloop/render-nr.sh [/opt/cursor/artifacts]
 *
 * Key parser uppercases letters so `down 30 a` matches keyp('A') / key('1').
 * BACKSPACE → 259 (studio.h KEY_BACKSPACE).
 */
#include "platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR        44100
#define PER_FRAME 735
#define TEMPO     120

static void put_u32(FILE *f, unsigned v) { fputc(v, f); fputc(v>>8, f); fputc(v>>16, f); fputc(v>>24, f); }
static void put_u16(FILE *f, unsigned v) { fputc(v, f); fputc(v>>8, f); }

typedef struct { int frame, down, key; } Ev;
static Ev ev[128];
static int nev;

static int parse_script(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char op[16], key[32];
        int fr;
        if (sscanf(line, "%15s %d %31s", op, &fr, key) != 3) continue;
        int k;
        if (!strcmp(key, "SPACE")) k = 32;
        else if (!strcmp(key, "BACKSPACE")) k = 259;
        else k = key[0] >= 'a' && key[0] <= 'z' ? key[0] - 32 : key[0];
        ev[nev++] = (Ev){ fr, !strcmp(op, "down"), k };
    }
    fclose(f);
    return 0;
}

static int write_ppm(const char *path, const uint32_t *pix, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            unsigned p = pix[y * w + x];
            fputc((p >> 16) & 255, f);
            fputc((p >>  8) & 255, f);
            fputc( p        & 255, f);
        }
    }
    fclose(f);
    return 0;
}

/* 120 BPM pulse train — enough energy in every 1-bar window that a freeze is audible. */
static void mic_chunk(float *out, int n, long pos) {
    float beat_s = 60.0f / (float)TEMPO;
    for (int i = 0; i < n; i++) {
        float t = (float)(pos + i) / (float)SR;
        float ph = t / beat_s;
        ph = ph - floorf(ph);
        float env = expf(-ph * 7.0f);
        float f = 220.0f + 55.0f * floorf(t / 2.0f);   /* new pitch every 2s */
        float x = 6.2831853f * f * t;
        out[i] = 0.70f * env * (sinf(x) + 0.30f * sinf(2.0f * x));
    }
}

int main(int argc, char **argv) {
    int frames = (argc > 1) ? atoi(argv[1]) : 360;
    const char *script = (argc > 2) ? argv[2] : NULL;
    const char *wav = (argc > 3) ? argv[3] : "/tmp/liveloop.wav";
    const char *ppm = (argc > 4) ? argv[4] : NULL;
    nev = 0;
    if (script && parse_script(script) != 0) { fprintf(stderr, "bad script\n"); return 1; }
    // de:engine-owner — the headless render host owns its single engine, like tools/headless-nr.c
    DeInstance *in = de_instance_create(DE_RENDERER_SOFTWARE);
    de_mic_set_active(1);
    short *pcm = malloc((size_t)frames * PER_FRAME * 2 * sizeof(short));
    if (!pcm) return 1;
    float scratch[PER_FRAME * 2];
    float mic[PER_FRAME];
    long npcm = 0;
    long micpos = 0;
    int ei = 0;
    uint32_t *snap = NULL;
    int sw = 0, sh = 0;
    for (int i = 0; i < frames; i++) {
        while (ei < nev && ev[ei].frame == i) {
            de_key_event(in, ev[ei].key, ev[ei].down);
            ei++;
        }
        mic_chunk(mic, PER_FRAME, micpos);
        de_audio_input(mic, PER_FRAME, SR);
        micpos += PER_FRAME;
        de_frame(in, (double)(i + 1) / 60.0);
        de_audio_render(in, scratch, PER_FRAME);
        for (int s = 0; s < PER_FRAME * 2; s++) {
            float v = scratch[s]; if (v > 1) v = 1; if (v < -1) v = -1;
            pcm[npcm++] = (short)(v * 32767.0f);
        }
        if (ppm && i == (frames * 3) / 4) {
            int need_w = 0, need_h = 0;
            de_copy_frame(in, NULL, 0, &need_w, &need_h);
            if (need_w > 0 && need_h > 0) {
                snap = malloc((size_t)need_w * need_h * sizeof(uint32_t));
                if (snap && de_copy_frame(in, snap, need_w * need_h, &need_w, &need_h)) {
                    sw = need_w; sh = need_h;
                }
            }
        }
    }
    FILE *a = fopen(wav, "wb");
    if (!a) { fprintf(stderr, "cannot write %s\n", wav); free(pcm); free(snap); return 1; }
    long bytes = npcm * sizeof(short);
    fwrite("RIFF", 1, 4, a); put_u32(a, 36 + bytes); fwrite("WAVE", 1, 4, a);
    fwrite("fmt ", 1, 4, a); put_u32(a, 16); put_u16(a, 1); put_u16(a, 2);
    put_u32(a, SR); put_u32(a, SR * 2 * 2); put_u16(a, 2 * 2); put_u16(a, 16);
    fwrite("data", 1, 4, a); put_u32(a, bytes);
    fwrite(pcm, sizeof(short), npcm, a);
    fclose(a);
    free(pcm);
    if (ppm && snap && sw > 0) write_ppm(ppm, snap, sw, sh);
    free(snap);
    fprintf(stderr, "wrote %s (%d frames, %ld samples)\n", wav, frames, npcm);
    return 0;
}
