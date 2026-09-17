/* DE_NO_RAYLIB scripted WAV (+ optional PPM) host for cloudhold.
 *
 * Linux proof path when there is no Raylib / no xxd / no window
 * (play.js and build-nr.sh both need those). Twin of tools/clips/modal/render-nr.c.
 *
 *   bash tools/clips/cloudhold/render-nr.sh [/opt/cursor/artifacts]
 *
 * Key parser uppercases letters so `down 30 a` matches the cart's keyp('A') / key('1').
 */
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR        44100
#define PER_FRAME 735

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
    // published frame is bottom-up (studio.c blit store); flip so the PPM is top-left
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

int main(int argc, char **argv) {
    int frames = (argc > 1) ? atoi(argv[1]) : 240;
    const char *script = (argc > 2) ? argv[2] : NULL;
    const char *wav = (argc > 3) ? argv[3] : "/tmp/cloudhold.wav";
    const char *ppm = (argc > 4) ? argv[4] : NULL;
    nev = 0;
    if (script && parse_script(script) != 0) { fprintf(stderr, "bad script\n"); return 1; }
    // de:engine-owner — the headless render host owns its single engine, like tools/headless-nr.c
    DeInstance *in = de_instance_create(DE_RENDERER_SOFTWARE);
    short *pcm = malloc((size_t)frames * PER_FRAME * 2 * sizeof(short));
    if (!pcm) return 1;
    float scratch[PER_FRAME * 2];
    long npcm = 0;
    int ei = 0;
    uint32_t *snap = NULL;
    int sw = 0, sh = 0;
    for (int i = 0; i < frames; i++) {
        while (ei < nev && ev[ei].frame == i) {
            de_key_event(in, ev[ei].key, ev[ei].down);
            ei++;
        }
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
