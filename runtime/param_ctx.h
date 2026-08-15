/* param_ctx.h — the per-instance HOST PARAMETER table (runtime/param.h).
 *
 * Same shape and the same reason as midi_ctx.h: an AUv3 puts every plug-in instance in one process,
 * so a process-wide table would mean two DAW tracks sharing one set of knobs — turn cutoff on track
 * one and track two follows. It is also the exact thing an automation lane must NOT do.
 *
 * WHAT A PARAMETER IS HERE: a pointer to a float the CART already owns. The parameter IS the knob,
 * not a copy of it, so a host write and a finger drag land in the same place and neither has to be
 * mirrored into the other. That is what makes "the panel moves and the host's automation lane
 * follows" fall out instead of needing a sync path.
 *
 * ⚠ WHICH MEANS THE POINTERS ARE INTO CART STATE, and are only valid for the instance that bound
 * them. They are re-bound every frame by the cart (param_bind is idempotent), which is also what
 * keeps them correct after a session-state restore rewrites that memory in place.
 */
#ifndef DE_PARAM_CTX_H
#define DE_PARAM_CTX_H

#include <stdint.h>

#define DE_PARAM_MAX  64   /* per instance. A host lists every one of these in its automation menu,
                            * so this is a budget, not a limit to grow at the first pinch. */
#define DE_PARAM_RING 64   /* power of two — host writes waiting for the next frame */

typedef struct {
    int    addr;        /* the cart's STABLE id. A saved project's automation references this number
                         * and nothing else, so changing what an addr means silently re-points
                         * somebody's automation lane at another knob. Append; never renumber. */
    float *slot;        /* → the cart's own float */
    float  lo, hi;      /* the range the host shows and normalises against */
    float  def;         /* captured from the slot on the FIRST bind = the cart's own initial value */
    char   name[16];    /* what the host's automation menu shows */
    uint8_t used;
} DeParamDef;

typedef struct { int addr; float v; } DeParamEv;

typedef struct {
    DeParamDef def[DE_PARAM_MAX];
    int        n;

    /* HOST → ENGINE. A host may set a parameter from its render thread (automation) or its main
     * thread (a mouse on the generic view), and the cart reads its knobs on the frame thread. So
     * writes queue here and are applied at the top of de_frame — the same single-producer ring the
     * input path uses, for the same reason (tools/input-ring-check). Writing straight through the
     * pointer would be a cross-thread write into cart state, which is the one thing that seam
     * exists to stop. */
    DeParamEv         ring[DE_PARAM_RING];
    volatile unsigned w, r;

    /* ENGINE → HOST. Not a ring: the host POLLS what changed and we diff the live slots against
     * what it was last told. A ring would overflow on a knob dragged for a second; a diff coalesces
     * that into one report and cannot lose the final value, which is the only one that matters. */
    float last[DE_PARAM_MAX];
    uint8_t last_valid[DE_PARAM_MAX];
} DeParam;

/* The DEFAULT instance's table, and the thread-local that everything above expands through. Same
 * fork as midi_ctx.h: instance 0 keeps this storage, so the desktop path is byte-identical and a
 * cart that binds nothing pays for one zeroed struct. */
static DeParam de_param_default;
static _Thread_local DeParam *de_param = &de_param_default;

#endif
