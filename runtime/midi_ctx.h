/* midi_ctx.h — the per-instance MIDI INPUT context (docs/design/engine-context.md).
 *
 * HAND-WRITTEN, unlike sound_ctx.h / studio_ctx.h / sync_ctx.h, which tools/ctx-gen.js produced.
 * It is the same shape by hand because ctx-gen refuses to re-run on a processed target and
 * midi_input.h was never one of its targets — the file was missing from engine-statics.js's
 * ENGINE_FILES for the whole refactor, so its state was never measured, never classified, and
 * never offered to the generator. Keep the shape identical anyway; the next reader should not
 * have to learn a second convention.
 *
 * WHAT WAS WRONG. `midi_r` and `midi_ccr` are CONSUMER cursors: midi_get() returns 0 when
 * midi_r == midi_w, and otherwise reads one event and advances. One reader drains the event and
 * moves the cursor past it, so with two racks in one process each host note-on reached exactly
 * ONE of them, at random. This is the defect runtime/sync.h already fixed for transport, and its
 * note on de_sync_position records what it cost: "the FIRST instance swallowed the START edge and
 * every other one joined mid-flow and stayed SILENT. Two DAW tracks, one playing."
 *
 * WHY PER-INSTANCE RATHER THAN A SHARED RING WITH PER-INSTANCE CURSORS ("broadcast"). Broadcast is
 * right when one physical source feeds every rack — which is the DESKTOP CoreMIDI case, one port
 * for the process. But the case that is actually broken is the AUv3, where the events do not come
 * from a shared port at all: each instance's render block hands the engine ITS OWN track's events,
 * so instance A's notes must never be visible to B. Making the whole thing per-instance is correct
 * for both, and it is the simpler of the two.
 *
 * THE DESKTOP PATH IS UNTOUCHED, and the reason is the same one sync_ctx.h states: the Raylib
 * build owns its own main() and never calls de_instance_create, so it runs on the default context
 * below — and the CoreMIDI callback thread, which never entered a seam call either, defaults to
 * that same object. Producer and consumer still meet on exactly the storage they met on before.
 */
#ifndef DE_MIDI_CTX_H
#define DE_MIDI_CTX_H

#include <stdint.h>

/* MOVED here from midi_input.h: the sizes and the event types the members are written in. They
 * have to precede the struct (the sync_ctx.h note applies — a #define is only text until it is
 * expanded, so moving one earlier is always safe). */
#define MIDI_RING   256   // power of two
#define MIDI_CCRING 128   // power of two

typedef struct { int8_t type; uint8_t note; uint8_t vel; } MidiEv;   // type: +1 on, -1 off
typedef struct { uint8_t ch, cc, val; } MidiCC;        // ch 0..15 on the wire (public API is 1..16)

typedef struct {
    /* notes: single-producer ring + the live key state the producer maintains, so midi_held()
     * answers whether or not anyone drains */
    MidiEv            midi_ring[MIDI_RING];
    volatile unsigned midi_w, midi_r;
    volatile uint8_t  midi_down[128];
    volatile int      midi_bend_v;          // -8192..8191
    volatile int      midi_dev_count;
    char              midi_dev_name[64];    // name of the connected keyboard (CoreMIDI / Web MIDI)
    volatile int      midi_g_wanted;        // a cart read a midi_* fn → the host may ask for access

    /* CC (knobs) — channel-aware, unlike the note path */
    MidiCC            midi_ccring[MIDI_CCRING];
    volatile unsigned midi_ccw, midi_ccr;
    int16_t           midi_cc_v[16][128];   // last value per channel, -1 = never seen
    int16_t           midi_cc_any[128];     // last value on ANY channel (the omni read)
    volatile int      midi_cc_init_done;
} DeMidi;

/* Every member's default is ZERO, so unlike its siblings this template needs no designated
 * initialisers — the two arrays whose "unset" is -1 rather than 0 (midi_cc_v, midi_cc_any) are
 * filled lazily on the first CC, guarded by midi_cc_init_done, which is itself part of the
 * context and so runs once PER INSTANCE. */
static DeMidi de_midi_default;

/* THE POINTER THE MACROS EXPAND THROUGH — thread-local for the reason sync_ctx.h gives: the seam
 * points it at the instance on entry and restores it on the way out, so two racks on two threads
 * never see each other, and a plain global would reintroduce the race the refactor removed. */
static _Thread_local DeMidi *de_midi = &de_midi_default;

/* the access block: every name midi_input.h already used, pointed at the context. */
#define midi_ring         (de_midi->midi_ring)
#define midi_w            (de_midi->midi_w)
#define midi_r            (de_midi->midi_r)
#define midi_down         (de_midi->midi_down)
#define midi_bend_v       (de_midi->midi_bend_v)
#define midi_dev_count    (de_midi->midi_dev_count)
#define midi_dev_name     (de_midi->midi_dev_name)
#define midi_g_wanted     (de_midi->midi_g_wanted)
#define midi_ccring       (de_midi->midi_ccring)
#define midi_ccw          (de_midi->midi_ccw)
#define midi_ccr          (de_midi->midi_ccr)
#define midi_cc_v         (de_midi->midi_cc_v)
#define midi_cc_any       (de_midi->midi_cc_any)
#define midi_cc_init_done (de_midi->midi_cc_init_done)

#endif
