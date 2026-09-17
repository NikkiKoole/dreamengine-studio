/* DE_NO_RAYLIB spec() host — Linux twin of `node tools/spec.js fm4op`.
 *
 * play.js / spec.js need Raylib + xxd. This links the cart + studio.c with
 * -DDE_SPEC -DDE_NO_RAYLIB and calls spec() through de_instance_create.
 *
 *   bash tools/clips/fm4op/render-nr.sh   # runs this first, then the WAVs
 */
#include "platform.h"
#include "spec.h"
#include <stdio.h>

int main(void) {
    // de:engine-owner — the headless spec host owns its single engine, like tools/headless-nr.c
    DeInstance *in = de_instance_create(DE_RENDERER_SOFTWARE);
    if (!in) { fprintf(stderr, "de_instance_create failed\n"); return 1; }
    spec();
    return 0;
}
