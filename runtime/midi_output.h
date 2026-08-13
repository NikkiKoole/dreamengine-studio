// midi_output.h — the engine SPEAKS MIDI: a cart emits notes/CC/clock to other apps
// and outboard gear. CoreMIDI virtual source on macOS + iOS; harmless stubs on the
// Windows cross-build and the web build. Engine-internal, compiled INSIDE studio.c
// exactly like sound.h and midi_input.h — never linked standalone.
//
// The output-direction sibling of midi_input.h. Read that file first: this one mirrors
// its shape (platform-independent core + a #if'd backend) but inverts the data flow, so
// the threading story is the opposite one and worth stating plainly.
//
// Threading: the PRODUCER is whoever calls midi_send_* — normally the cart, on the main
// loop, but on iOS/AUv3 the cart's update() runs on the audio thread (de_frame is sample-
// clocked in the render block). MIDISend/MIDIReceived are documented as callable from a
// realtime context, and we allocate nothing per call (a fixed packet-list buffer on the
// stack), so a cart may send from either thread. What is NOT safe is sending from two
// threads at once; no cart does, and the engine never sends on its own.
//
// Public API (declared in studio.h, defined here — the sound.h pattern):
//   void midi_send_note(int ch, int note, int vel, int on)  note on/off, ch 1..16
//   void midi_send_cc(int ch, int cc, int val)              controller change, val 0..127
//   void midi_send_bend(int ch, int v)                      pitch bend, -8192..8191
//   void midi_send_clock(void) / _start(int rewind) / _stop(void)   transport out
//   bool midi_out_ready(void)                               is the virtual source live?
//
// WHY A VIRTUAL SOURCE, not an output port to a destination. MIDISourceCreate publishes
// US as something the rest of the system can select as a MIDI *input* — which is what a
// DAW's track-input menu lists, and it is the SAME call on macOS and iOS, so one
// implementation covers desktop and the phone→Ableton case (over USB, network or BLE, all
// of which the OS layers underneath without us knowing). An output port (what
// tools/sync-spike/midisend.c uses) is the other shape: it pushes INTO a destination you
// picked, which means choosing one, and there is no good default. See
// docs/design/midi-out.md → "Two delivery models".
//
// CHANNELS ARE PART OF THE API, deliberately. midi_input.h masks the channel nibble off
// (`d[k] & 0xF0`) because a keybed cart has one voice and does not care. A generator cart
// does: acidcandy is four machines, and its two drum machines are many voices each, which
// is one channel apiece with the voices as NOTE NUMBERS on a GM percussion map — not a
// channel per voice. So every fn here takes `ch` first. docs/design/midi-out.md.
//
// Scope: notes, CC, bend, and the clock/transport bytes. Not sent: SysEx, program change,
// aftertouch, MPE (which needs per-channel bend allocation, and the input half is
// MPE-unaware too — see midi-out.md).

#ifndef DE_MIDI_OUTPUT_H
#define DE_MIDI_OUTPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>   // usleep, for the shutdown flush below

// A cart called a midi_send_* fn → the host may need to ask for MIDI access (mirrors
// midi_g_wanted on the input side; the web bridge polls it before prompting).
static volatile int midi_out_wanted = 0;
static volatile int midi_out_live   = 0;   // 1 once the virtual source exists

// ── may this run send at all? ──
// The mirror of sync.h's `automated` rule, pointed outward. An automated run (headless, a
// screenshot/gif bake, a scripted or ui-audited pass) must NOT publish a MIDI device and fire
// notes into whatever DAW the dev has open — a build-all sweep across 550 carts would be a
// stray-note machine. studio.c sets these two from the same operands sync_automated uses;
// `--midi-out` overrides, which is how the gate sends for real.
// ⚠ Kept OUTSIDE any #ifdef on purpose: sync_automated's equivalent assignment sat inside
// #ifdef DE_SPEC and was therefore inert in exactly the harness runs it was written to
// protect (see the note at its assignment in studio.c). Do not repeat that.
static int midi_out_automated = 0;   // set by studio.c after flag parsing
static int midi_out_force     = 0;   // --midi-out

static void midi_output_init(void);        // defined per-backend below; idempotent
static void midi_out_all_notes_off(void);  // defined with the public API below (needs midi_out_raw)

// LAZY INIT — the source is created on the first send, never at startup. Deliberate:
// MIDISourceCreate publishes "dreamengine" into every DAW's and monitor's input list, and
// doing that for every cart run would litter the user's MIDI setup with a port that most
// carts never write to. Worse, it would do it on HEADLESS harness runs, right next to the
// documented hazard where a DAW on the same machine leaks into a run (external-clock-sync.md
// → "The ambient clock"). So a cart that never sends never appears. Cost: the first send
// allocates (MIDIClientCreate), which is a realtime sin on exactly one call if that first
// send happens on the audio thread — send once from update() at startup to pay it early.
static void midi_out_ensure(void) {
    midi_out_wanted = 1;
    if (midi_out_automated && !midi_out_force) return;   // silent, and NOT an error: the cart runs fine
    if (!midi_out_live) midi_output_init();
}

// Calling this means the cart intends to send (it is how a panel draws a "MIDI OUT" lamp),
// so it opens the port too — otherwise it would answer false forever on a cart that only
// ever asks before sending.
bool midi_out_ready(void) { midi_out_ensure(); return midi_out_live != 0; }

// ── CoreMIDI backend (macOS desktop AND iOS — the virtual-source API is identical) ──
// Gated the same way midi_input.h gates its INPUT scan, with one deliberate difference:
// output stays enabled under DE_NO_RAYLIB. Input is off there because a portable host
// FEEDS us MIDI (the AUv3 render block hands us its event list, so scanning devices would
// be wrong). Nobody feeds output — a cart that wants to emit still has to reach CoreMIDI
// itself, and on iOS that is the same virtual source. An AUv3 emitting to its HOST is a
// different path again (the render block's MIDI output block), and is not this file.
#if defined(__APPLE__) && !defined(PLATFORM_WEB)

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>

// Same deprecation story as midi_input.h: MIDIPacketList/MIDIReceived are superseded by
// the UMP event-list API but fully functional, and far simpler for 3-byte messages.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static MIDIClientRef   midi_out_client = 0;
static MIDIEndpointRef midi_out_src    = 0;

static void midi_output_init(void) {
    if (midi_out_live) return;                       // idempotent: the AUv3 lesson (ios-plan.md)
    // Reuse the input client if it exists — one CoreMIDI client per process is the norm,
    // and midi_input.h creates one named "dreamengine". They are independent handles, so a
    // second client is legal and simpler than sharing across a #if boundary; the cost is
    // one extra name in Audio MIDI Setup, which only shows for apps that actually send.
    if (MIDIClientCreate(CFSTR("dreamengine out"), NULL, NULL, &midi_out_client) != noErr) return;
    if (MIDISourceCreate(midi_out_client, CFSTR("dreamengine"), &midi_out_src) != noErr) {
        MIDIClientDispose(midi_out_client);
        midi_out_client = 0;
        return;
    }
    midi_out_live = 1;
}

static void midi_output_shutdown(void) {
    midi_out_all_notes_off();   // BEFORE disposing the port — after it, there is nothing to send on
    // …and then WAIT, because MIDIReceived is asynchronous: it hands the packets to the server and
    // returns, so disposing the endpoint on the very next line can throw the note-offs away before
    // they leave. That makes "quitting never leaves a note droning" true only by luck — it passed
    // the gate once and failed the next run with exactly the two notes that were still held.
    // 20ms at process exit costs nothing and nobody is waiting on it.
    if (midi_out_live) usleep(20000);
    if (midi_out_src)    MIDIEndpointDispose(midi_out_src);
    if (midi_out_client) MIDIClientDispose(midi_out_client);
    midi_out_src = 0; midi_out_client = 0; midi_out_live = 0;
}

// The one send path everything below funnels through. n = 1..3 bytes.
// Allocation-free: the packet list lives on the stack, so this is safe from the audio
// thread (which is where a cart's update() runs inside the AUv3).
static void midi_out_raw(const uint8_t *bytes, int n) {
    if (!midi_out_live || n <= 0) return;
    uint8_t buf[256];
    MIDIPacketList *pl = (MIDIPacketList *)buf;
    MIDIPacket     *p  = MIDIPacketListInit(pl);
    // timestamp 0 = "as soon as possible", which is what we want: the cart already decided
    // WHEN by calling us on the step it wanted. Scheduling ahead would need the host clock.
    p = MIDIPacketListAdd(pl, sizeof buf, p, 0, (ByteCount)n, bytes);
    if (p) MIDIReceived(midi_out_src, pl);
}

#pragma clang diagnostic pop

#else  // ── Windows / web / anything else: harmless stubs ──
// Web could later drive Web-MIDI *output* from JS, mirroring runtime/web_midi.js on the
// input side. Until then a cart's midi_send_* calls are silently dropped, which is the
// same contract as midi_input.h's stubs: the cart still runs, it just talks to nobody.

static void midi_output_init(void)     {}
static void midi_output_shutdown(void) {}
static void midi_out_raw(const uint8_t *bytes, int n) { (void)bytes; (void)n; }

#endif

// ── public API (platform-independent; every one funnels into midi_out_raw) ──
// Channel is 1..16 as printed on gear and in every DAW, NOT the 0..15 on the wire — the
// conversion happens here, once, so no cart ever writes `ch - 1`. Out-of-range channels
// are clamped rather than dropped: a cart with an off-by-one still makes sound on an
// adjacent channel, which is debuggable, where silence is not.
static inline uint8_t midi_ch_nib(int ch) {
    if (ch < 1)  ch = 1;
    if (ch > 16) ch = 16;
    return (uint8_t)(ch - 1);
}

// Which notes we have started and not yet stopped, so shutdown can release them. 2KB, and it
// buys the one guarantee a cart cannot make for itself: QUITTING NEVER LEAVES A NOTE DRONING in
// whatever instrument you were driving. A cart can always exit on a frame where a note is held —
// the window closes, the process dies, and the note-off it was going to send next frame never
// happens. Found by the gate's gate-length work: the drum hits gained a real length, and two of
// them were still held when the run ended (on=68, off=66).
static uint8_t midi_out_on[16][128];

void midi_send_note(int ch, int note, int vel, int on) {
    midi_out_ensure();
    if (note < 0 || note > 127) return;
    if (vel < 0) vel = 0;
    if (vel > 127) vel = 127;
    // A note-on with velocity 0 IS a note-off on the wire, and plenty of gear only sends
    // that form — but we emit a real 0x80 so a receiver that distinguishes them (and a
    // human reading a MIDI monitor) sees the intent. Both are legal.
    uint8_t nib = midi_ch_nib(ch);
    midi_out_on[nib][note] = on ? 1 : 0;
    uint8_t b[3] = { (uint8_t)((on ? 0x90 : 0x80) | nib), (uint8_t)note, (uint8_t)vel };
    midi_out_raw(b, 3);
}

// Release every note we started. Called on shutdown; also worth exposing one day as a cart-facing
// panic, but no cart has needed it yet, so it stays internal rather than growing the API.
static void midi_out_all_notes_off(void) {
    if (!midi_out_live) return;
    for (int c = 0; c < 16; c++)
        for (int n = 0; n < 128; n++)
            if (midi_out_on[c][n]) {
                uint8_t b[3] = { (uint8_t)(0x80 | c), (uint8_t)n, 0 };
                midi_out_raw(b, 3);
                midi_out_on[c][n] = 0;
            }
}

void midi_send_cc(int ch, int cc, int val) {
    midi_out_ensure();
    if (cc < 0 || cc > 127) return;
    if (val < 0) val = 0;
    if (val > 127) val = 127;
    uint8_t b[3] = { (uint8_t)(0xB0 | midi_ch_nib(ch)), (uint8_t)cc, (uint8_t)val };
    midi_out_raw(b, 3);
}

void midi_send_bend(int ch, int v) {
    midi_out_ensure();
    if (v < -8192) v = -8192;
    if (v >  8191) v =  8191;
    unsigned u = (unsigned)(v + 8192);              // 0..16383, centre 8192
    uint8_t b[3] = { (uint8_t)(0xE0 | midi_ch_nib(ch)), (uint8_t)(u & 0x7F), (uint8_t)((u >> 7) & 0x7F) };
    midi_out_raw(b, 3);
}

// ── transport out (the mirror of what sync.h consumes) ──
// These are SYSTEM REALTIME: single bytes, no channel. A cart driving outboard gear sends
// midi_send_clock() 24× per quarter note — which is the cart's job, not ours, because only
// the cart knows its own beat. sync.h's SYNC_PPQN is the same 24, so a cart that already
// derives steps from sync_beats() has the tick boundary in hand.
void midi_send_clock(void)         { midi_out_ensure(); uint8_t b = 0xF8; midi_out_raw(&b, 1); }
void midi_send_start(int rewind)   { midi_out_ensure(); uint8_t b = rewind ? 0xFA : 0xFB; midi_out_raw(&b, 1); }  // START rewinds, CONTINUE resumes
void midi_send_stop(void)          { midi_out_ensure(); uint8_t b = 0xFC; midi_out_raw(&b, 1); }

#endif // DE_MIDI_OUTPUT_H
