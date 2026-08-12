// sync.h — EXTERNAL CLOCK: play in time with something outside the cart.
// Engine-internal, compiled INSIDE studio.c exactly like sound.h / midi_input.h —
// never linked standalone. Must be included BEFORE midi_input.h (its MIDI-clock
// parser pushes ticks in here).
//
// WHY a seam instead of "read MIDI clock in the cart": there are three ways a cart
// can be handed someone else's tempo, and they arrive in two different shapes —
//
//   MIDI clock      24 ticks per quarter note + START/CONTINUE/STOP.  INCREMENTAL:
//                   no absolute position, tempo must be MEASURED from the tick rate.
//                   (a DAW's sync output, a drum machine's clock out, another app)
//   AUv3 host       musicalContextBlock → currentTempo + currentBeatPosition.
//                   ABSOLUTE: the host says exactly where the playhead is.
//   Ableton Link    session tempo + beat phase. ABSOLUTE, same shape as the host.
//
// A cart that wired MIDI clock directly would have to be rewritten for the other
// two. So all three push into the state below and every cart reads the SAME four
// functions, never learning which clock it's following:
//
//   bool  sync_active(void)   is an external clock driving us at all?
//   bool  sync_playing(void)  has it STARTED (and not stopped since)?
//   float sync_beats(void)    beats since it started — 0.25 = one 16th in 4/4
//   float sync_bpm(void)      its tempo
//
// sync_beats() is the common currency: an incremental source accumulates into it,
// an absolute source overwrites it. A cart drives its own step counter FROM that
// number rather than accumulating its own, so a host loop-jump or a re-START lands
// the pattern where the clock says instead of wherever the cart had drifted to.
//
// OPT-IN by design: this does NOT hijack the engine's own bpm()/beat() clock. A
// cart that never calls sync_* behaves exactly as before, tempo knob and all.
//
// Threading: producers may be another thread (the CoreMIDI read callback; later the
// AU render block). Every producer write is a single word, and the consumer
// (sync_frame, once per frame on the main thread) does all the derivation — the
// midi_input.h ring pattern, minus the ring, because a tick carries no payload.

#ifndef DE_SYNC_H
#define DE_SYNC_H

#include <stdint.h>
#include <stdbool.h>

#define SYNC_PPQN     24     // MIDI clock's fixed resolution: ticks per quarter note
#define SYNC_TIMEOUT 2.0f    // seconds of silence before we call the clock gone

// ── producer state (written by a backend, possibly off-thread) ────────────────
static volatile uint32_t sync_p_ticks = 0;   // 24-ppqn ticks since the last START
static volatile uint32_t sync_p_msgs  = 0;   // ANY transport message — liveness only
static volatile int      sync_p_run   = 0;   // 1 between START/CONTINUE and STOP
static volatile int      sync_p_abs   = 0;   // 1 = an absolute source owns the position
static volatile double   sync_p_beats = 0;   // absolute sources only (host / Link)
static volatile double   sync_p_bpm   = 0;   // absolute sources only

// ── producer API ─────────────────────────────────────────────────────────────
// INCREMENTAL (MIDI clock). from_zero: START rewinds to bar 1, CONTINUE resumes.
static void sync_push_tick (void)          { sync_p_ticks++; sync_p_msgs++; }
static void sync_push_start(int from_zero) { if (from_zero) sync_p_ticks = 0; sync_p_run = 1; sync_p_msgs++; }
static void sync_push_stop (void)          { sync_p_run = 0; sync_p_msgs++; }
// MIDI song-position pointer: jump the tick counter (so CONTINUE from bar 5 lands on bar 5).
static void sync_push_seek (uint32_t ticks) { sync_p_ticks = ticks; sync_p_msgs++; }
// ABSOLUTE (AUv3 musicalContextBlock, Ableton Link) — the whole position, every block.
static void sync_push_pos(double beats, double bpm, int playing) {
    sync_p_beats = beats; sync_p_bpm = bpm; sync_p_run = playing;
    sync_p_abs = 1; sync_p_msgs++;
}

// ── consumer state (main thread, all of it derived in sync_frame) ─────────────
static double   sync_c_beats = 0, sync_c_bpm = 0;
static bool     sync_c_active = false, sync_c_playing = false;
static uint32_t sync_c_last_msgs = 0, sync_c_last_ticks = 0;
static float    sync_c_quiet = SYNC_TIMEOUT * 2;      // seconds since the last message
static double   sync_c_win_t = 0;                     // tempo-measure span: seconds…
static double   sync_c_win_n = 0;                     // …and ticks over it (0 = no tick seen yet)
static double   sync_c_pend  = 0;                     // seconds since the last tick-bearing frame
// --midi-clock <bpm>: a synthetic clock for the harness, so a gate needs no DAW and no cable.
// It keeps its OWN tick counter rather than pushing into the producer state above — see the
// ambient-clock guard in sync_frame for why that separation is load-bearing.
static float    sync_synth_bpm = 0;
static double   sync_synth_acc = 0;
static uint32_t sync_synth_ticks = 0;

// once per frame, before the cart's update(). dt = the frame delta the rest of the
// engine clock uses (fixed under --det, so a traced run is reproducible). det = the
// harness's deterministic mode.
static void sync_frame(float dt, int det) {
    // ── WHICH CLOCK ARE WE ON? Exactly three cases, in this order, and the order is the guard:
    //   1. --midi-clock given → the SYNTHETIC clock, and it is the ONLY source.
    //   2. otherwise a deterministic run (--det, script, replay) → NO external clock at all.
    //   3. otherwise → the real one (CoreMIDI now, an AUv3 host or Link later).
    //
    // Both guards exist because a real DAW on the dev machine leaks into the harness. Found the
    // hard way, twice: first a ui-audit run with no --midi-clock reported a string that only
    // draws while slaved (Ableton was playing), then two identical --midi-clock runs produced
    // DIFFERENT traces, because the synthetic clock was pushing into the same producer state the
    // CoreMIDI thread was feeding, so the real ticks simply added on top (+1 tick over 300
    // frames, +3 over 600, varying per run). A gate whose result depends on whether someone has
    // a DAW open is worse than no gate. Hence: the synthetic clock is a separate counter, and a
    // deterministic run never consults the real one. A plain `run` (the editor) still follows a
    // real DAW, which is the entire point of the feature.
    uint32_t msgs, ticks;
    int      run, absolute;
    if (sync_synth_bpm > 0) {
        sync_synth_acc += (double)dt * (sync_synth_bpm / 60.0) * SYNC_PPQN;
        while (sync_synth_acc >= 1.0) { sync_synth_acc -= 1.0; sync_synth_ticks++; }
        ticks = sync_synth_ticks; msgs = sync_synth_ticks + 1; run = 1; absolute = 0;
    } else if (det) {
        sync_c_active = false; sync_c_playing = false; sync_c_bpm = 0;
        return;
    } else {
        ticks = sync_p_ticks; msgs = sync_p_msgs; run = sync_p_run; absolute = sync_p_abs;
    }

    if (msgs != sync_c_last_msgs) { sync_c_last_msgs = msgs; sync_c_quiet = 0; sync_c_active = true; }
    else                            sync_c_quiet += dt;
    // A stopped-but-connected DAW sends nothing at all, so "active" has to survive a
    // silence — but not forever, or a cart stays slaved to a clock that went away and
    // its own tempo knob is dead. After SYNC_TIMEOUT we hand control back; the next
    // tick re-slaves instantly.
    if (sync_c_quiet > SYNC_TIMEOUT) {
        sync_c_active = false; sync_c_playing = false;
        sync_c_bpm = 0; sync_c_win_t = 0; sync_c_win_n = 0;
        return;
    }
    sync_c_playing = run != 0;

    if (absolute) { sync_c_beats = sync_p_beats; sync_c_bpm = sync_p_bpm; return; }

    // incremental: beats come straight off the tick count. 1/24 beat is FINER than the 1/4
    // beat a 16th-note step needs, so there is nothing to interpolate for step accuracy —
    // POSITION is exact, and that is the number a sequencer follows.
    uint32_t d = (ticks >= sync_c_last_ticks) ? ticks - sync_c_last_ticks : ticks;  // < = a START rewind
    sync_c_last_ticks = ticks;
    sync_c_beats = (double)ticks / SYNC_PPQN;

    // TEMPO, on the other hand, has to be MEASURED from the tick rate, and this is where a
    // clock slave gets it wrong. Two traps, both found by --midi-clock 120 reading back wrong:
    //   1. a frame sees 0, 1 or 2 ticks, so a short window is dominated by which side of a
    //      frame boundary a tick fell on. Counting 6 ticks and dividing read 122 for a true
    //      120 — not noise, a bias: the tick fraction pending at each end is worth ±2 BPM
    //      when the window is only 8 frames long. So the span has to be LONG.
    //   2. the time before the first tick, and after the last one, belongs to no interval.
    //      Counting it made a long span converge to 120 only after ten seconds, always from
    //      below. So measure between TICK-BEARING FRAMES, and divide by n-1 intervals, not n.
    // HALVING at the top keeps the ratio exactly while forgetting slowly, which is what lets a
    // long span still follow a tempo change; a big jump resets outright so a hard tempo edit
    // lands in a beat instead of a bar. (Frame-quantized arrival still costs ±1 frame on each
    // endpoint. Sub-0.1 BPM would want the CoreMIDI packet timestamps — see the header.)
    sync_c_pend += dt;
    if (d > 0) {
        if (sync_c_win_n <= 0) { sync_c_win_n = 1; sync_c_win_t = 0; }   // first tick starts the span
        else                   { sync_c_win_n += d; sync_c_win_t += sync_c_pend; }
        sync_c_pend = 0;
        if (sync_c_win_n > 1 && sync_c_win_t > 0.25) {                   // ≥ a beat at 240bpm
            double rate = 60.0 * ((sync_c_win_n - 1) / SYNC_PPQN) / sync_c_win_t;
            if (rate > 20 && rate < 400) {                               // ignore a stall / a burst
                bool jumped = sync_c_bpm > 0 && (rate > sync_c_bpm * 1.08 || rate < sync_c_bpm * 0.92);
                sync_c_bpm = rate;
                if (jumped) { sync_c_win_n = 1; sync_c_win_t = 0; }      // tempo edit → start over
            }
        }
        if (sync_c_win_t > 4.0) { sync_c_win_t *= 0.5; sync_c_win_n = 1 + (sync_c_win_n - 1) * 0.5; }
    }
}

// ── public API (declared in studio.h — the sound.h pattern) ───────────────────
bool  sync_active(void)  { return sync_c_active; }
bool  sync_playing(void) { return sync_c_active && sync_c_playing; }
float sync_beats(void)   { return (float)sync_c_beats; }
float sync_bpm(void)     { return (float)sync_c_bpm; }

#endif // DE_SYNC_H
