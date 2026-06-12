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
//
// Scope (v1): note-on/off + velocity + pitch-bend only. CC (knobs), MIDI-learn,
// aftertouch, and clock are deliberately out — see docs/design/midi-and-keybed.md.

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
int midi_get(int *note, int *vel) {
    if (midi_r == midi_w) return 0;
    MidiEv e = midi_ring[midi_r & (MIDI_RING - 1)];
    midi_r++;
    if (note) *note = e.note;
    if (vel)  *vel  = e.vel;
    return e.type;
}
bool midi_held(int note)  { return (note >= 0 && note < 128) && midi_down[note] != 0; }
int  midi_bend(void)      { return midi_bend_v; }
bool midi_present(void)   { return midi_dev_count > 0; }
const char *midi_name(void) { return midi_dev_name; }   // connected keyboard's name, or "" if none

// ── CoreMIDI backend (macOS only) ──────────────────────────────────────────────
#if defined(__APPLE__) && !defined(PLATFORM_WEB)

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

// parse one packet's raw bytes — handles 0x80/0x90 (note) and 0xE0 (bend); skips the
// rest by message length. Best-effort on running status (a stray data byte is skipped).
static void midi_parse(const uint8_t *d, unsigned n) {
    unsigned k = 0;
    while (k < n) {
        uint8_t status = d[k] & 0xF0;
        if ((status == 0x90 || status == 0x80) && k + 2 < n) {
            uint8_t note = d[k + 1], vel = d[k + 2];
            if (status == 0x90 && vel > 0) de_midi_push(+1, note, vel);
            else                           de_midi_push(-1, note, vel);
            k += 3;
        } else if (status == 0xE0 && k + 2 < n) {
            midi_bend_v = (((int)d[k + 2] << 7) | d[k + 1]) - 8192;
            k += 3;
        } else if (status == 0xB0 || status == 0xA0) {  k += 3;  // CC / poly-AT — skip (future)
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

#ifdef PLATFORM_WEB
// Web MIDI bridge: runtime/web_midi.js (emcc --post-js) drives navigator.requestMIDIAccess()
// and calls these to feed the same ring/state the cart drains via midi_get(). KEEPALIVE so
// the linker keeps them; called from JS via Module.ccall.
#include <emscripten.h>
EMSCRIPTEN_KEEPALIVE void de_midi_web_push(int type, int note, int vel) { de_midi_push(type, note, vel); }
EMSCRIPTEN_KEEPALIVE void de_midi_web_bend(int v)     { midi_bend_v = v; }
EMSCRIPTEN_KEEPALIVE void de_midi_web_present(int n)  { midi_dev_count = n; if (n == 0) midi_dev_name[0] = 0; }
EMSCRIPTEN_KEEPALIVE void de_midi_web_name(const char *s) { strncpy(midi_dev_name, s ? s : "", sizeof midi_dev_name - 1); midi_dev_name[sizeof midi_dev_name - 1] = 0; }
#endif

#endif

#endif // DE_MIDI_INPUT_H
