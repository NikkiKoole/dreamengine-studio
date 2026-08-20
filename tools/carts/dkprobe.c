/* de:meta
{
  "slug": "dkprobe",
  "title": "dk probe",
  "status": "wip",
  "created": "2026-08-20",
  "kind": ["probe"],
  "teaches": ["drum-synthesis"],
  "description": {
    "summary": "Throwaway balance probe: fires each drumkit.h role in turn at groovebook's own levels and velocities so their loudness can be compared.",
    "controls": "None. It plays itself."
  }
}
de:meta */
#include "studio.h"
#include "drumkit.h"
#include <stdio.h>

// mirrors groovebook.c exactly: same level table, same velocity per role
#define KIT_BASE 20
// PER KIT, because the two kits are not merely different sounds but different LOUDNESS SHAPES.
// Measured intrinsic loudness (peak with level and velocity divided out):
//   ELECTRO  hats -11, crash -20, sine toms -31, sine kick -33, band-noise snare/clap -38
//   ACOUSTIC hats -11, crash -13, membrane kick/toms -38 to -39, snare/clap/rim -36 to -37
// So a kick-led balance needs the hats pulled ~26 dB down in one kit and ~33 dB in the other, and
// a single table leaves whichever kit it was not tuned for badly wrong. The ELECTRO table used to
// be the only one, and on ACOUSTIC it left the toms 13 dB under a leading crash.
static const float LVL[2][DK_N] = {
    {   // ELECTRO
        [DK_KICK] = 1.00f, [DK_SNARE] = 1.00f, [DK_HHC] = 0.050f, [DK_HHO] = 0.060f,
        [DK_CLAP] = 1.00f, [DK_TOM_LO] = 0.55f, [DK_TOM_HI] = 0.55f, [DK_CRASH] = 0.200f,
    },
    {   // ACOUSTIC: membrane kick and toms are far quieter, its crash far louder
        [DK_KICK] = 1.00f, [DK_SNARE] = 0.575f, [DK_HHC] = 0.023f, [DK_HHO] = 0.028f,
        [DK_CLAP] = 0.50f, [DK_TOM_LO] = 0.70f, [DK_TOM_HI] = 0.70f, [DK_CRASH] = 0.045f,
    },
};
// the pitch and velocity each drumpat role fires with in groovebook
typedef struct { const char *lane; int role, midi, vel; } Shot;
static const Shot SHOT[] = {
    { "BD",  DK_KICK,   36, 7 }, { "SD",  DK_SNARE,  38, 7 },
    { "LT",  DK_TOM_LO, 41, 6 }, { "MT",  DK_TOM_LO, 48, 6 }, { "HT",  DK_TOM_HI, 55, 6 },
    { "CH",  DK_HHC,    42, 4 }, { "OH",  DK_HHO,    46, 4 }, { "CY",  DK_CRASH,  49, 4 },
    { "RS",  DK_SNARE,  50, 7 }, { "CB",  DK_TOM_HI, 62, 6 }, { "CPS", DK_CLAP,   39, 7 },
    { "TB",  DK_HHC,    54, 4 },
};
#define NSHOT ((int)(sizeof SHOT / sizeof SHOT[0]))
static int fired = -1;
static int probe_kit = 1;   // 0 electro, 1 acoustic (set below)

void init(void) {
    dk_use(probe_kit ? &DK_ACOUSTIC : &DK_ELECTRO, KIT_BASE);
    for (int i = 0; i < DK_N; i++) instrument_level(KIT_BASE + i, LVL[probe_kit][i]);
}
void update(void) {
    int slot = (int)(now() / 1.5f);            // 1.5 s apart: the crash rings ~1.1 s, and at
                                               // 600 ms every window was measuring the previous tail
    if (slot != fired && slot < NSHOT) {
        fired = slot;
        dk_fire(SHOT[slot].role, SHOT[slot].midi, SHOT[slot].vel);
    }
}
void draw(void) {
    cls(CLR_BLACK); font(FONT_SMALL);
    print("dk balance probe", 4, 4, CLR_WHITE);
    for (int i = 0; i < NSHOT; i++) {
        char b[48];
        snprintf(b, sizeof b, "%-4s %-8s midi %2d  vel %d", SHOT[i].lane,
                 dk_role_name(SHOT[i].role), SHOT[i].midi, SHOT[i].vel);
        print(b, 4, 16 + i * 10, i == fired ? CLR_YELLOW : CLR_DARK_GREY);
    }
}
