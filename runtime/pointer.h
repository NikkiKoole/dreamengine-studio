// pointer.h — a multi-finger pointer pool, cart-land, on touch_id() bookkeeping.
//
// Why this exists (design/multitouch-and-generalization-audit.md, Part 2): ~15
// instrument carts hand-roll the SAME six lines — a `Ptr` struct with an `id`,
// a fixed pool (`NPTR`/`NOID`), and a per-frame "find this finger's slot, or
// claim a free one" loop over touch_count(), plus a release loop over
// touch_ended_*(). pluck/mallet/tabla/handpan/fm/pd/bowed/guitar/brass/spacecho/
// pedalboard/drummachine/tb303/groovebox/sh101 all carry a near-verbatim copy.
// This header owns that bookkeeping so the cart writes only what a finger DOES.
//
// It's the layer BELOW ui.h: ui.h is for widgets (knob/slider/button); pointer.h
// is the raw finger→arbitrary-target pool a cart needs when a finger grabs a
// string to bend, sweeps a row of bars, or picks open space. A cart whose
// fingers only drive widgets should use ui.h instead. Engine deliberately does
// NOT own this (decision-0006 lane); carts #include it. Everything is static —
// each cart compiles its own copy (the gestures.h / improv.h pattern).
//
// Built on the virtual touch pool: the desktop mouse arrives as one synthetic
// finger, so a cart written against this is mouse-and-touch capable for free.
//
// THE ONE CONTRACT: your per-finger struct's FIRST field is `int id`. The pool
// only ever reads/writes that field (a free slot has .id == PTR_NONE); every
// other field is yours. Conventionally `int mode` comes second, but the pool
// never touches it.
//
//   #include "pointer.h"
//   enum { PT_IDLE, PT_DRAG, PT_SWEEP };
//   typedef struct { int id, mode, target; float x, y; } Ptr;  // id FIRST
//   static Ptr ptr[PTR_MAX];
//
//   void init(void) { PTR_CLEAR(ptr); }
//
//   void update(void) {
//       for (int i = 0; i < touch_count(); i++) {
//           int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
//           bool fresh;
//           Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);   // existing slot, or a new claim
//           if (!p) continue;                        // pool full (>PTR_MAX fingers)
//           if (fresh) { *p = (Ptr){ id, PT_IDLE };  /* a finger just landed */ }
//           else if (p->mode == PT_DRAG) { /* drive its target from tx/ty */ }
//           p->x = tx; p->y = ty;
//       }
//       for (int i = 0; i < touch_ended_count(); i++) {   // lifted fingers
//           Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
//           if (p) { /* note_off etc. */ p->id = PTR_NONE; }
//       }
//   }
//
// Tunable — #define BEFORE the #include to override:
//   PTR_MAX  (10)  pool size = max simultaneous fingers tracked

#ifndef POINTER_H
#define POINTER_H

#include "studio.h"

#ifndef PTR_MAX
#define PTR_MAX 10
#endif

#define PTR_NONE (-999)   // a free slot's .id

// --- core (void* + stride: works on any struct whose first field is `int id`) ---

static void pointer_clear(void *pool, int n, int stride) {
    for (int i = 0; i < n; i++) *(int *)((char *)pool + (long)i * stride) = PTR_NONE;
}

// the slot currently owned by touch `id`, or 0.
static void *pointer_find(void *pool, int n, int stride, int id) {
    for (int i = 0; i < n; i++) {
        void *e = (char *)pool + (long)i * stride;
        if (*(int *)e == id) return e;
    }
    return 0;
}

// touch `id`'s slot — claiming the first free slot when this finger is new.
// *fresh (may be 0) is set true on a fresh claim. Returns 0 when the pool is full.
static void *pointer_acquire(void *pool, int n, int stride, int id, bool *fresh) {
    void *freeE = 0;
    for (int i = 0; i < n; i++) {
        void *e = (char *)pool + (long)i * stride;
        int eid = *(int *)e;
        if (eid == id) { if (fresh) *fresh = false; return e; }
        if (eid == PTR_NONE && !freeE) freeE = e;
    }
    if (!freeE) return 0;
    *(int *)freeE = id;             // claim
    if (fresh) *fresh = true;
    return freeE;
}

// --- typed wrappers: pass the pool ARRAY (not a pointer); count + stride inferred ---

#define PTR_CLEAR(arr) \
    pointer_clear((arr), (int)(sizeof(arr) / sizeof((arr)[0])), (int)sizeof((arr)[0]))
#define PTR_FIND(arr, id) \
    pointer_find((arr), (int)(sizeof(arr) / sizeof((arr)[0])), (int)sizeof((arr)[0]), (id))
#define PTR_ACQUIRE(arr, id, fresh_ptr) \
    pointer_acquire((arr), (int)(sizeof(arr) / sizeof((arr)[0])), (int)sizeof((arr)[0]), (id), (fresh_ptr))

#endif // POINTER_H
