// send-cc.c — the DRIVER for the MIDI-*IN* half of the gate: a tiny standalone CoreMIDI
// virtual source that emits a known CC sequence, so the engine's brand-new CC parse path can be
// asserted end-to-end through real CoreMIDI instead of by reading the code and hoping.
//
// WHY A VIRTUAL SOURCE and not an output port (which is what tools/sync-spike/midisend.c uses):
// the engine's input side scans for MIDI *sources* and connects its input port to each of them
// (midi_input.h → midi_connect_sources). It is never a *destination*, so a sender that pushes into
// a destination has nothing to push into. Publishing ourselves as a source is what makes the engine
// pick us up — and it also means this needs no IAC bus, same as the out-direction listener.
//
// WHY IT SENDS THE SAME VALUE MANY TIMES: the engine's midi_cc() is a POLLED read of the last
// value seen. A single message could be missed if the cart's first frame lands after it, so the
// sequence is repeated for the whole window; the gate asserts the value ARRIVED, not that exactly
// one did. The three CCs are deliberately on DIFFERENT channels, because the channel nibble is
// the part of this path most likely to be silently wrong.
//
//   clang -o send-cc send-cc.c -framework CoreMIDI -framework CoreFoundation
//   ./send-cc <seconds>
//
// The sequence (repeated ~10×/sec for <seconds>):
//   ch 1  cc 74 = 100      the "cutoff" convention, on the pitched channel
//   ch 10 cc 7  = 55       a drum-channel level — proves the channel nibble survives
//   ch 16 cc 1  = 127      the top channel + mod wheel — proves 1..16 maps to 0..15 correctly

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static MIDIEndpointRef src;

static void send3(unsigned char a, unsigned char b, unsigned char c) {
    unsigned char bytes[3] = { a, b, c };
    unsigned char buf[256];
    MIDIPacketList *pl = (MIDIPacketList *)buf;
    MIDIPacket     *p  = MIDIPacketListInit(pl);
    p = MIDIPacketListAdd(pl, sizeof buf, p, 0, 3, bytes);
    if (p) MIDIReceived(src, pl);
}

int main(int argc, char **argv) {
    int secs = argc > 1 ? atoi(argv[1]) : 5;
    MIDIClientRef client;
    if (MIDIClientCreate(CFSTR("midi-check-send"), NULL, NULL, &client) != noErr) {
        fprintf(stderr, "send-cc: MIDIClientCreate failed\n"); return 2;
    }
    // The name matters: the engine reports the connected device via midi_name(), and the gate
    // prints it, so a human reading a failure can tell "nothing arrived" from "the wrong thing did".
    if (MIDISourceCreate(client, CFSTR("midi-check-send"), &src) != noErr) {
        fprintf(stderr, "send-cc: MIDISourceCreate failed\n"); return 2;
    }
    printf("send-cc: publishing for %ds\n", secs); fflush(stdout);

    for (int i = 0; i < secs * 10; i++) {
        send3(0xB0 | 0,  74, 100);   // channel 1  (wire nibble 0)
        send3(0xB0 | 9,   7,  55);   // channel 10 (wire nibble 9)
        send3(0xB0 | 15,  1, 127);   // channel 16 (wire nibble 15)
        usleep(100000);
    }
    printf("send-cc: done\n");
    MIDIEndpointDispose(src);
    MIDIClientDispose(client);
    return 0;
}

#pragma clang diagnostic pop
