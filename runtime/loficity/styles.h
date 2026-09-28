// runtime/loficity/styles.h — the style ports (each style's plan / voicing / bass, line for line) + the hooks table.
#include "style_piano.h"
#include "style_synth.h"
#include "style_ambient.h"
#include "style_sad.h"
#include "style_bossa.h"
#include "style_guitar.h"
#include "style_house.h"
#include "style_medieval.h"

static const StyleHooks HOOKS[NSTYLE] = {
    [S_JAZZHOP] = { NULL, NULL, NULL },
    [S_PIANO]   = { pno_plan, pno_voicing, pno_bass },
    [S_SYNTH]   = { syn_plan, NULL, syn_bass },
    [S_AMBIENT] = { amb_plan, amb_voicing, amb_bass },
    [S_SAD]     = { sad_plan, sad_voicing, sad_bass },
    [S_BOSSA]   = { bos_plan, bos_voicing, bos_bass },
    [S_GUITAR]  = { gtr_plan, gtr_voicing, gtr_bass },
    [S_HOUSE]   = { hou_plan, hou_voicing, hou_bass },
    [S_MEDIEVAL] = { med_plan, med_voicing, med_bass },
};
static const StyleHooks *style_hooks(int style) { return style >= 0 && style < NSTYLE ? &HOOKS[style] : NULL; }
