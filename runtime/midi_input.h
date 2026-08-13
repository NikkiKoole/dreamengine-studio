// midi_input.h — physical MIDI keyboard input. CoreMIDI on macOS; harmless
// stubs on the Windows cross-build and the web build (where the JS Web-MIDI
// backend will feed the same ring later). Engine-internal, compiled INSIDE
// studio.c exactly like sound.h — never linked standalone.
//
// Threading: the CoreMIDI read callback runs on its own high-priority thread
// (single producer). The main loop is the single consumer. Note events go
// through a lock-light power-of-two ring; midi_down[]/midi_bend live state is
// maintained by the callback so midi_held()/midi_bend() work whether or not
// anyone drains the ring (a cart that ignores MIDI just lets the ring overwrite
// its oldest entries — harmless).
//
// Public API (declared in studio.h, defined here — the sound.h pattern):
//   int  midi_get(int *note, int *vel)  drain one event: +1 note-on, -1 off, 0 none
//   bool midi_held(int note)            is this note currently down?
//   int  midi_bend(void)                last pitch-bend, -8192..8191 (0 = centre)
//   bool midi_present(void)             any MIDI source connected?
//   int  midi_cc(int ch, int cc)        last CC value 0..127, -1 = never seen (ch 0 = omni)
//   int  midi_cc_get(int*, int*, int*)  drain one CC event (the MIDI-learn primitive)
//
// Scope: note-on/off + velocity + pitch-bend + CC (knobs, channel-aware — see the CC block
// below for why the channel is kept there and dropped for notes), plus the CLOCK/TRANSPORT
// bytes, which this file only PARSES — the state they drive lives in sync.h (which must be
// included first), because a host clock and Ableton Link feed the same cart-facing API from
// somewhere else entirely. Still out: aftertouch (poly + channel), program change, and MPE
// (midi_bend is one GLOBAL bend, not per-channel, so the input layer is MPE-unaware) — see
// docs/design/midi-and-keybed.md. The OUTPUT direction is runtime/midi_output.h.

#ifndef DE_MIDI_INPUT_H
#define DE_MIDI_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ── platform-independent event ring + live state (also the web backend's target) ──
#define MIDI_RING 256   // power of two
typedef struct { int8_t type; uint8_t note; uint8_t vel; } MidiEv;   // type: +1 on, -1 off
static MidiEv            midi_ring[MIDI_RING];
static volatile unsigned midi_w = 0, midi_r = 0;
static volatile uint8_t  midi_down[128];
static volatile int      midi_bend_v = 0;     // -8192..8191
static volatile int      midi_dev_count = 0;
static char              midi_dev_name[64] = {0};   // name of the connected keyboard (CoreMIDI / Web MIDI)
static volatile int      midi_g_wanted = 0;   // a cart called a midi_* read fn → host may ask for MIDI access (web opt-in, mirrors mic_g_wanted). See de_midi_wanted() in studio.c + runtime/web_midi.js.

// ── CC (knobs) — channel-aware, unlike the note path above ──
// WHY the channel is kept here and thrown away for notes: a keybed cart has ONE voice, so
// which channel a note arrived on is noise. A knob mapping is the opposite — a rack with
// several machines wants "cutoff on channel 1" to reach a different machine than "cutoff on
// channel 10", which is exactly how a DAW automates a multi-timbral instrument. So notes
// stay omni (simple, and no cart asked otherwise) and CC carries the channel.
// See docs/design/midi-out.md for the channel map this mirrors.
#define MIDI_CCRING 128   // power of two
typedef struct { uint8_t ch, cc, val; } MidiCC;        // ch 0..15 on the wire (public API is 1..16)
static MidiCC            midi_ccring[MIDI_CCRING];
static volatile unsigned midi_ccw = 0, midi_ccr = 0;
static int16_t           midi_cc_v[16][128];           // last value per channel, -1 = never seen
static int16_t           midi_cc_any[128];             // last value on ANY channel (the omni read)
static volatile int      midi_cc_init_done = 0;

static void de_midi_push_cc(int ch, int cc, int val) {
    if (ch < 0 || ch > 15 || cc < 0 || cc > 127 || val < 0 || val > 127) return;
    if (!midi_cc_init_done) {                          // -1 (unseen) is not zero, so it needs filling
        for (int c = 0; c < 16; c++) for (int i = 0; i < 128; i++) midi_cc_v[c][i] = -1;
        for (int i = 0; i < 128; i++) midi_cc_any[i] = -1;
        midi_cc_init_done = 1;
    }
    midi_cc_v[ch][cc] = (int16_t)val;
    midi_cc_any[cc]   = (int16_t)val;
    unsigned w = midi_ccw;
    midi_ccring[w & (MIDI_CCRING - 1)] = (MidiCC){ (uint8_t)ch, (uint8_t)cc, (uint8_t)val };
    midi_ccw = w + 1;
}

// producer side — called from the CoreMIDI thread (or, later, the web JS bridge)
static void de_midi_push(int type, int note, int vel) {
    if (note < 0 || note > 127) return;
    unsigned w = midi_w;
    midi_ring[w & (MIDI_RING - 1)] = (MidiEv){ (int8_t)type, (uint8_t)note, (uint8_t)vel };
    midi_w = w + 1;
    if (type > 0)      midi_down[note] = 1;
    else if (type < 0) midi_down[note] = 0;
}

// ── public API (consumer side, main thread) ──
// Reading ANY of these means the cart wants MIDI input — so the first call raises the
// "wanted" flag the web bridge polls (web_midi.js) before it asks for the MIDI permission.
// A cart that never touches MIDI (e.g. acidcandy) never sets it → no spurious prompt.
int midi_get(int *note, int *vel) {
    midi_g_wanted = 1;
    if (midi_r == midi_w) return 0;
    MidiEv e = midi_ring[midi_r & (MIDI_RING - 1)];
    midi_r++;
    if (note) *note = e.note;
    if (vel)  *vel  = e.vel;
    return e.type;
}
bool midi_held(int note)  { midi_g_wanted = 1; return (note >= 0 && note < 128) && midi_down[note] != 0; }
int  midi_bend(void)      { midi_g_wanted = 1; return midi_bend_v; }
bool midi_present(void)   { midi_g_wanted = 1; return midi_dev_count > 0; }
const char *midi_name(void) { midi_g_wanted = 1; return midi_dev_name; }   // connected keyboard's name, or "" if none

// ── CC reads: one POLLED, one DRAINED, because knobs and MIDI-learn want different things ──
// midi_cc()     — "where is that knob now?" Survives a frame where nothing moved, which is
//                 what riding a filter needs. ch 0 = omni (any channel), the simple case.
// midi_cc_get() — "which knob did the user just touch?" The MIDI-learn primitive: you cannot
//                 build learn from the polled form, because the whole question is WHICH cc
//                 moved and a poll only answers about one you already named.
int midi_cc(int ch, int cc) {
    midi_g_wanted = 1;
    if (cc < 0 || cc > 127) return -1;
    if (ch == 0) return midi_cc_any[cc];               // omni
    if (ch < 1 || ch > 16) return -1;
    return midi_cc_v[ch - 1][cc];
}
int midi_cc_get(int *ch, int *cc, int *val) {
    midi_g_wanted = 1;
    if (midi_ccr == midi_ccw) return 0;
    MidiCC e = midi_ccring[midi_ccr & (MIDI_CCRING - 1)];
    midi_ccr++;
    if (ch)  *ch  = e.ch + 1;                          // public API is 1..16, like the gear prints it
    if (cc)  *cc  = e.cc;
    if (val) *val = e.val;
    return 1;
}

// ── CoreMIDI backend (DESKTOP macOS only) ───────────────────────────────────────
// Gated off under DE_NO_RAYLIB: a portable host (iOS AUv3, Switch) is fed MIDI by the
// HOST (the render block's event list), not by scanning CoreMIDI device sources — same
// model as the web build. The iOS feed export lives in the #else branch (de_midi_event).
#if defined(__APPLE__) && !defined(PLATFORM_WEB) && !defined(DE_NO_RAYLIB)

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>

// MIDIInputPortCreate / MIDIPacketList are deprecated in favour of the UMP/event-list
// API, but still fully functional and far simpler for our note+bend needs. Silence the
// deprecation noise (no -Werror in the build, but keep the log clean).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static MIDIClientRef    midi_client;
static MIDIPortRef      midi_port;
static MIDIEndpointRef  midi_connected[32];   // sources we've already wired (avoid double-connect on hot-plug)
static int              midi_nconnected = 0;

// parse one packet's raw bytes — handles 0x80/0x90 (note), 0xE0 (bend) and the clock/
// transport messages (0xF8/0xFA/0xFB/0xFC/0xF2 → sync.h); skips the rest by message
// length. Best-effort on running status (a stray data byte is skipped).
static void midi_parse(const uint8_t *d, unsigned n) {
    unsigned k = 0;
    while (k < n) {
        // SYSTEM REALTIME (0xF8..0xFF) first: these are single bytes with no channel
        // nibble, so `& 0xF0` can't tell them apart from each other (they all read as
        // 0xF0). They also arrive INTERLEAVED inside a packet, between the bytes of
        // other messages, which is why they're checked before anything else.
        if (d[k] >= 0xF8) {
            if      (d[k] == 0xF8) sync_push_tick();      // clock, 24 per quarter note
            else if (d[k] == 0xFA) sync_push_start(1);    // START    = rewind to the top
            else if (d[k] == 0xFB) sync_push_start(0);    // CONTINUE = resume where we stopped
            else if (d[k] == 0xFC) sync_push_stop();      // STOP
            k += 1; continue;                             // 0xFE active-sensing / 0xFF reset: ignored
        }
        if (d[k] == 0xF2 && k + 2 < n) {                  // song position pointer: 14-bit, in 16ths
            unsigned spp = ((unsigned)d[k + 2] << 7) | d[k + 1];
            sync_push_seek(spp * (SYNC_PPQN / 4));
            k += 3; continue;
        }
        uint8_t status = d[k] & 0xF0;
        if ((status == 0x90 || status == 0x80) && k + 2 < n) {
            uint8_t note = d[k + 1], vel = d[k + 2];
            if (status == 0x90 && vel > 0) de_midi_push(+1, note, vel);
            else                           de_midi_push(-1, note, vel);
            k += 3;
        } else if (status == 0xE0 && k + 2 < n) {
            midi_bend_v = (((int)d[k + 2] << 7) | d[k + 1]) - 8192;
            k += 3;
        } else if (status == 0xB0 && k + 2 < n) {
            de_midi_push_cc(d[k] & 0x0F, d[k + 1], d[k + 2]);   // CC — channel KEPT (see below)
            k += 3;
        } else if (status == 0xA0) {                    k += 3;  // poly-AT — skip (future)
        } else if (status == 0xC0 || status == 0xD0) {  k += 2;  // program / channel-AT
        } else {                                        k += 1;  // unknown / running-status data byte
        }
    }
}

static void midi_read_cb(const MIDIPacketList *pkts, void *a, void *b) {
    (void)a; (void)b;
    const MIDIPacket *p = &pkts->packet[0];
    for (unsigned i = 0; i < pkts->numPackets; i++) {
        midi_parse(p->data, p->length);
        p = MIDIPacketNext(p);
    }
}

static void midi_connect_sources(void) {
    ItemCount n = MIDIGetNumberOfSources();
    int count = 0;
    char name[64] = {0};
    for (ItemCount i = 0; i < n; i++) {
        MIDIEndpointRef src = MIDIGetSource(i);
        if (!src) continue;
        count++;
        // remember a device name to show ("connected to …") — the display name (e.g. "Arturia KeyStep 32")
        CFStringRef cf = NULL;
        if (MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &cf) == noErr && cf) {
            CFStringGetCString(cf, name, sizeof name, kCFStringEncodingUTF8);
            CFRelease(cf);
        }
        bool already = false;
        for (int j = 0; j < midi_nconnected; j++) if (midi_connected[j] == src) { already = true; break; }
        if (!already && midi_nconnected < 32) {
            if (MIDIPortConnectSource(midi_port, src, NULL) == noErr)
                midi_connected[midi_nconnected++] = src;
        }
    }
    midi_dev_count = count;
    if (count == 0) midi_dev_name[0] = 0;
    else            strncpy(midi_dev_name, name, sizeof midi_dev_name - 1);
}

// hot-plug: re-scan sources whenever the MIDI setup changes (keyboard plugged in
// AFTER launch is the common case).
static void midi_notify(const MIDINotification *msg, void *ctx) {
    (void)ctx;
    if (msg->messageID == kMIDIMsgSetupChanged) midi_connect_sources();
}

static void midi_input_init(void) {
    if (MIDIClientCreate(CFSTR("dreamengine"), midi_notify, NULL, &midi_client) != noErr) return;
    if (MIDIInputPortCreate(midi_client, CFSTR("dreamengine in"), midi_read_cb, NULL, &midi_port) != noErr) return;
    midi_connect_sources();
}

static void midi_input_shutdown(void) {
    if (midi_port)   MIDIPortDispose(midi_port);
    if (midi_client) MIDIClientDispose(midi_client);
    midi_port = 0; midi_client = 0; midi_nconnected = 0;
}

#pragma clang diagnostic pop

#else  // ── non-macOS / web: stubs (the web JS bridge calls the exports below) ──

static void midi_input_init(void)     {}
static void midi_input_shutdown(void) {}

#ifdef DE_NO_RAYLIB
// Host-MIDI feed for portable backends (the iOS AUv3 render block; Switch later). Same
// target as the web bridge: push host MIDI into the ring the cart drains via midi_get().
// On iOS we sample-clock de_frame() in the AU render block, so producer (these) and
// consumer (midi_get in the cart's update) are the SAME audio thread — no cross-thread race.
//   type: +1 note-on, -1 note-off ; note 0..127 ; vel 1..127
void de_midi_event(int type, int note, int vel) { de_midi_push(type, note, vel); }
void de_midi_bend(int v)                         { midi_bend_v = v; }          // -8192..8191
void de_midi_cc(int ch, int cc, int val)         { de_midi_push_cc(ch, cc, val); }   // ch 0..15, as it arrives on the wire
#endif

#ifdef PLATFORM_WEB
// Web MIDI bridge: runtime/web_midi.js (emcc --post-js) drives navigator.requestMIDIAccess()
// and calls these to feed the same ring/state the cart drains via midi_get(). KEEPALIVE so
// the linker keeps them; called from JS via Module.ccall.
#include <emscripten.h>
EMSCRIPTEN_KEEPALIVE void de_midi_web_push(int type, int note, int vel) { de_midi_push(type, note, vel); }
EMSCRIPTEN_KEEPALIVE void de_midi_web_bend(int v)     { midi_bend_v = v; }
EMSCRIPTEN_KEEPALIVE void de_midi_web_cc(int ch, int cc, int val) { de_midi_push_cc(ch, cc, val); }
EMSCRIPTEN_KEEPALIVE void de_midi_web_present(int n)  { midi_dev_count = n; if (n == 0) midi_dev_name[0] = 0; }
EMSCRIPTEN_KEEPALIVE void de_midi_web_name(const char *s) { strncpy(midi_dev_name, s ? s : "", sizeof midi_dev_name - 1); midi_dev_name[sizeof midi_dev_name - 1] = 0; }
#endif

#endif

#endif // DE_MIDI_INPUT_H
