// mdrender — render a Mutable Dreams 1.4 BIOME through the upstream engine itself
// (app/dsp/dreams.cpp + its vendored Plaits/Rings/Elements, MIT), so the `dreamatlas` cart has
// the real thing to be compared against. Built by `tools/ref-render/run.sh mutable`.
//
//   mdrender <biome_id> <seconds> [--solo <track 0..5>] [--dry]     → stereo f32le on stdout
//
//   --solo  mutes the other five tracks (per-VOICE comparison inside a real scene)
//   --dry   fx return, grain tape and every send to 0 (the voice with no room around it)
//
// The cart's twins of both flags are -DDA_SOLO=<n> / -DDA_DRY (see tools/carts/dreamatlas.c), and
// -DDA_ID=<id> opens it on the same biome. Upstream starts its first step at sample 0; the cart
// books its first step ~0.1 s in, so the two line up in CHARACTER (spectrum, envelope, density),
// never sample for sample. Compare with wav-envelope / harmonic-spec / oneshot-compare, not
// wav-correlate.
#include "dsp/dreams.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: mdrender <biome_id> <seconds> [--solo n] [--dry]\n"); return 1; }
    int id = atoi(argv[1]); double secs = atof(argv[2]); int solo = -1, dry = 0;
    for (int a = 3; a < argc; a++) {
        if (!strcmp(argv[a], "--solo") && a + 1 < argc) solo = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--dry")) dry = 1;
    }
    if (id < 0 || id >= MD_BIOMES) { fprintf(stderr, "biome id 0..%d\n", MD_BIOMES - 1); return 1; }
    float lr[256];
    md_init();
    md_command(MD_SCENE, 0, id);
    md_render(lr, 1);                         // commands are applied at the audio owner, in render
    if (solo >= 0) for (int t = 0; t < MD_TRACKS; t++) md_command(MD_MUTE, t, t != solo);
    if (dry) {
        md_command(MD_FX_LEVEL, 0, 0);
        md_command(MD_MACRO, 3, 0);           // the grain tape's return
        for (int t = 0; t < MD_TRACKS; t++) {
            md_command(MD_SEND, t, 0); md_command(MD_REVERB_SEND, t, 0); md_command(MD_DELAY_SEND, t, 0);
        }
    }
    md_command(MD_PLAY, 0, 1);
    md_render(lr, 1);
    if (md_state()->biome_id != id) { fprintf(stderr, "biome %d did not load\n", id); return 1; }
    fprintf(stderr, "%s (%s), %d BPM\n", md_biome_name(id), md_biome_family(id), md_state()->tempo);
    unsigned total = (unsigned)(secs * MD_RATE), done = 0, bad = 0;
    while (done < total) {
        unsigned n = total - done > 128 ? 128 : total - done;
        md_render(lr, n);
        for (unsigned k = 0; k < 2 * n; k++) if (!std::isfinite(lr[k])) { lr[k] = 0; bad++; }
        fwrite(lr, sizeof(float), 2 * n, stdout);
        done += n;
    }
    if (bad) fprintf(stderr, "WARNING: %u non-finite samples zeroed\n", bad);
    return 0;
}
