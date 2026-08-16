// param.h — HOST PARAMETERS: the knobs a DAW can see, automate, and record.
//
// Why this exists (docs/design/host-parameters.md): the AUv3 exposed no AUParameterTree at all, so a
// host saw ZERO parameters. Nothing on the rack was automatable, nothing was recordable, and the
// preset/automation menus were empty. It is also why the mod wheel was mapped to the master filter —
// that was a workaround for having no parameters, not a design anyone would choose.
//
// ── THE MODEL: THE PARAMETER *IS* THE KNOB ───────────────────────────────────────────────────────
// A cart binds a parameter to a float it already owns:
//
//     param_bind(P_CUTOFF, &cutoff, "CUT", 0, 1);
//
// and changes nothing else. The panel keeps dragging `cutoff`, the DSP keeps reading `cutoff`, and
// the host now reads and writes that same float. Because there is one storage location and not two,
// "the host automates it" and "the panel follows the automation" and "a finger drag shows up in the
// host's lane" are all the same fact rather than three sync paths to keep honest.
//
// param_bind is IDEMPOTENT and safe to call every frame — the natural place is right where the knob
// is drawn, so a parameter cannot drift away from the control it names. The first bind for an addr
// captures the slot's current value as the DEFAULT (which is the cart's own initial value, by
// construction) and later calls only re-point the pointer.
//
// ── ADDRESSES ARE FOREVER ────────────────────────────────────────────────────────────────────────
// ⚠ `addr` is the ONLY thing a saved project's automation lane stores. Change what an addr means and
// every project someone already automated now moves a different knob — silently, with no version to
// refuse and no error to see. It is exactly the hazard tools/lint-saved-state.js exists for on the
// other side of the seam, and it has the same rule: APPEND, NEVER RENUMBER. Give them an enum with
// explicit values, and retire an addr by leaving the hole.
//
// ── THREADS ──────────────────────────────────────────────────────────────────────────────────────
// A host writes from wherever it likes (automation on the render thread, a generic-view slider on
// the main one) and the cart reads its knobs on the frame thread. So de_param_set QUEUES and
// de_param_drain applies at the top of de_frame — the same discipline as the input ring, and for the
// same reason: a cross-thread write into cart state is precisely what that seam exists to stop.
// de_param_get reads the slot directly and is deliberately NOT synchronised: the worst case is a
// host reading a value one frame stale, which is what every plug-in's meter already is.
#ifndef DE_PARAM_H
#define DE_PARAM_H

#include <string.h>
#include "param_ctx.h"

DeParam *de_instance_param(DeInstance *in);   // studio.c — this instance's parameter table

// ⚠ THE NATIVE BUILD HAS NO INSTANCES AT ALL. `struct DeInstance` and every resolver live inside
// studio.c's `#ifdef DE_NO_RAYLIB` block — the plug-in / headless-host path — so a native build
// cannot resolve one. That stayed invisible for as long as the only native caller was
// `de_param_set(NULL, …)`, where -O2 folds the `if (in)` branch away and never emits the reference.
// The moment a seam function grew a body the optimiser could not fold, the NATIVE build stopped
// LINKING with "Undefined symbols: _de_instance_param", on code that compiles and runs fine under a
// host. Worse than a build error, for what it implies about coverage: the headless gates exercise
// the DEFAULT shared table through the thread-local while a host exercises the per-instance one.
// One engine per native process is exactly what that build means, so the stub is not a compromise.
#ifndef DE_NO_RAYLIB
DeParam *de_instance_param(DeInstance *in) { (void)in; return &de_param_default; }
#endif

// The macros onto this thread's context, exactly as midi_input.h does it.
#define de_par_def        (de_param->def)
#define de_par_n          (de_param->n)
#define de_par_ring       (de_param->ring)
#define de_par_w          (de_param->w)
#define de_par_r          (de_param->r)
#define de_par_last       (de_param->last)
#define de_par_last_valid (de_param->last_valid)
#define de_par_want       (de_param->want)
#define de_par_want_valid (de_param->want_valid)

static DeParamDef *de_param_find(int addr) {
    for (int i = 0; i < de_par_n; i++) if (de_par_def[i].used && de_par_def[i].addr == addr) return &de_par_def[i];
    return 0;
}

// ── cart-land ────────────────────────────────────────────────────────────────────────────────────
void param_bind(int addr, float *slot, const char *name, float lo, float hi) {
    if (!slot || addr < 0) return;
    DeParamDef *p = de_param_find(addr);
    if (p) { p->slot = slot; return; }        // already known — just re-point (state may have moved)
    if (de_par_n >= DE_PARAM_MAX) return;     // budget spent; silently ignored rather than clobbering
    p = &de_par_def[de_par_n++];
    p->addr = addr; p->slot = slot; p->lo = lo; p->hi = hi;
    p->def  = *slot;                          // the cart's own initial value IS the default
    p->used = 1;
    int i = 0; if (name) for (; name[i] && i < (int)sizeof p->name - 1; i++) p->name[i] = name[i];
    p->name[i] = 0;
}

// How many parameters this cart declared. A cart that binds none costs nothing and shows the host an
// empty tree, which is what every cart did before this existed.
int param_count(void) { return de_par_n; }

// ── the host seam ────────────────────────────────────────────────────────────────────────────────
int de_param_count(DeInstance *in) {
    DeParam *prev = de_param; if (in) de_param = de_instance_param(in);
    int n = de_par_n;
    de_param = prev; return n;
}

// Read one declaration by INDEX (0..count-1), for a host building its parameter tree once at init.
// Every out-pointer is optional. Returns 0 if the index is out of range.
int de_param_info(DeInstance *in, int i, int *addr, const char **name,
                  float *lo, float *hi, float *def) {
    DeParam *prev = de_param; if (in) de_param = de_instance_param(in);
    int ok = (i >= 0 && i < de_par_n && de_par_def[i].used);
    if (ok) {
        DeParamDef *p = &de_par_def[i];
        if (addr) *addr = p->addr;
        if (name) *name = p->name;
        if (lo)   *lo   = p->lo;
        if (hi)   *hi   = p->hi;
        if (def)  *def  = p->def;
    }
    de_param = prev; return ok;
}

// Prefers a value the host has SET but the frame thread has not applied yet — see `want` in
// param_ctx.h. Without that preference a host caches the pre-write value and shows it forever.
float de_param_get(DeInstance *in, int addr) {
    DeParam *prev = de_param; if (in) de_param = de_instance_param(in);
    DeParamDef *p = de_param_find(addr);
    float v = 0.0f;
    if (p && p->slot) {
        int i = (int)(p - de_par_def);
        v = de_par_want_valid[i] ? de_par_want[i] : *p->slot;
    }
    de_param = prev; return v;
}

// QUEUED, not applied — see the thread note in the header. Safe from any thread; a full ring drops
// the OLDEST rather than the newest, because on a parameter the newest value is the one that matters.
void de_param_set(DeInstance *in, int addr, float v) {
    DeParam *prev = de_param; if (in) de_param = de_instance_param(in);
    // record the INTENT first, CLAMPED exactly as the drain will clamp it, so a host reading back
    // an out-of-range write is told what it will actually get rather than what it asked for.
    DeParamDef *p = de_param_find(addr);
    if (p) {
        float c = v;
        if (p->hi > p->lo) { if (c < p->lo) c = p->lo; if (c > p->hi) c = p->hi; }
        int i = (int)(p - de_par_def);
        de_par_want[i] = c; de_par_want_valid[i] = 1;
    }
    unsigned w = de_par_w;
    de_par_ring[w & (DE_PARAM_RING - 1)] = (DeParamEv){ addr, v };
    de_par_w = w + 1;
    de_param = prev;
}

// Apply everything the host queued. Called at the top of de_frame, so a cart's update() and draw()
// see one settled set of values for the whole frame rather than values moving under them mid-frame.
static void de_param_drain(void) {
    unsigned r = de_par_r, w = de_par_w;
    if (w - r > DE_PARAM_RING) r = w - DE_PARAM_RING;   // overran: keep the newest ring-full
    for (; r != w; r++) {
        DeParamEv e = de_par_ring[r & (DE_PARAM_RING - 1)];
        DeParamDef *p = de_param_find(e.addr);
        if (!p || !p->slot) continue;
        float v = e.v;
        if (p->hi > p->lo) { if (v < p->lo) v = p->lo; if (v > p->hi) v = p->hi; }
        *p->slot = v;
        // and remember it as ALREADY REPORTED, so a host write does not bounce straight back at the
        // host as if the panel had moved. That feedback loop is what makes an automation lane fight
        // the value it is writing.
        // ⚠ TESTED, not assumed: removing this does NOT fix the out-of-process read-back gap below,
        // so it is kept because it is right, not because it is load-bearing for that.
        int i = (int)(p - de_par_def);
        de_par_last[i] = v; de_par_last_valid[i] = 1;
        // the slot now HOLDS the intent, so stop preferring it — from here a get reads the live
        // value again and a panel move is visible to the host immediately.
        de_par_want_valid[i] = 0;
    }
    de_par_r = r;
}

// POLL what the PANEL moved, so a host's automation lane follows a finger on the glass. Returns 1
// and fills *addr/*v for one changed parameter; call in a while loop until it returns 0.
int de_param_changed(DeInstance *in, int *addr, float *v) {
    DeParam *prev = de_param; if (in) de_param = de_instance_param(in);
    int found = 0;
    for (int i = 0; i < de_par_n; i++) {
        DeParamDef *p = &de_par_def[i];
        if (!p->used || !p->slot) continue;
        float cur = *p->slot;
        if (de_par_last_valid[i] && cur == de_par_last[i]) continue;
        de_par_last[i] = cur; de_par_last_valid[i] = 1;
        if (addr) *addr = p->addr;
        if (v)    *v    = cur;
        found = 1; break;
    }
    de_param = prev; return found;
}

#endif
