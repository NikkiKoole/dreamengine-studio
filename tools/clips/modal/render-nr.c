/* DE_NO_RAYLIB scripted WAV host — injects a play.js-style .script then dumps audio.
 *
 * Linux proof path for carts when there is no Raylib / no xxd / no window
 * (play.js and build-nr.sh both need those). Sibling of tools/headless-nr.c,
 * with key injection so the committed clips/modal script files drive the cart.
 *
 *   clang -O2 tools/clips/modal/render-nr.c runtime/studio.c runtime/raylib_compat.c \
 *         build/cart.c -I runtime -I build -DDE_NO_RAYLIB=1 \
 *         -DSCALE=1 -DSCREEN_W=320 -DSCREEN_H=200 \
 *         -DMAP_W=128 -DMAP_H=64 -DCELL_W=16 -DCELL_H=16 \
 *         -lm -lpthread -o build/modal-nr-script
 *   build/modal-nr-script 180 tools/clips/modal/01-marimba-strike.script out.wav
 *
 * Or: bash tools/clips/modal/render-nr.sh [/opt/cursor/artifacts]
 *
 * Key parser uppercases letters so `down 30 a` matches the cart's keyp('A').
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

int main(int argc, char **argv) {
    int frames = (argc > 1) ? atoi(argv[1]) : 180;
    const char *script = (argc > 2) ? argv[2] : NULL;
    const char *wav = (argc > 3) ? argv[3] : "/tmp/modal.wav";
    nev = 0;
    if (script && parse_script(script) != 0) { fprintf(stderr, "bad script\n"); return 1; }
    // de:engine-owner — the headless render host owns its single engine, like tools/headless-nr.c
    DeInstance *in = de_instance_create(DE_RENDERER_SOFTWARE);
    short *pcm = malloc((size_t)frames * PER_FRAME * 2 * sizeof(short));
    if (!pcm) return 1;
    float scratch[PER_FRAME * 2];
    long npcm = 0;
    int ei = 0;
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
    }
    FILE *a = fopen(wav, "wb");
    if (!a) { fprintf(stderr, "cannot write %s\n", wav); free(pcm); return 1; }
    long bytes = npcm * sizeof(short);
    fwrite("RIFF", 1, 4, a); put_u32(a, 36 + bytes); fwrite("WAVE", 1, 4, a);
    fwrite("fmt ", 1, 4, a); put_u32(a, 16); put_u16(a, 1); put_u16(a, 2);
    put_u32(a, SR); put_u32(a, SR * 2 * 2); put_u16(a, 2 * 2); put_u16(a, 16);
    fwrite("data", 1, 4, a); put_u32(a, bytes);
    fwrite(pcm, sizeof(short), npcm, a);
    fclose(a);
    free(pcm);
    fprintf(stderr, "wrote %s (%d frames, %ld samples)\n", wav, frames, npcm);
    return 0;
}
