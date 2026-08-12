// midimon — throwaway CoreMIDI listener: print every packet's raw bytes, naming the
// transport messages. Answers "what is Live ACTUALLY sending" without touching the engine.
//   clang -o midimon midimon.c -framework CoreMIDI -framework CoreFoundation && ./midimon 40
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static unsigned long clocks = 0;
static const char *name_of(unsigned char b) {
    switch (b) {
        case 0xF8: return "CLOCK";
        case 0xFA: return "START";
        case 0xFB: return "CONTINUE";
        case 0xFC: return "STOP";
        case 0xF2: return "SONG-POSITION";
        case 0xFE: return "active-sensing";
        case 0xFF: return "reset";
        default:   return NULL;
    }
}

static void cb(const MIDIPacketList *pkts, void *a, void *b) {
    (void)a; (void)b;
    const MIDIPacket *p = &pkts->packet[0];
    for (unsigned i = 0; i < pkts->numPackets; i++) {
        for (unsigned k = 0; k < p->length; k++) {
            const char *n = name_of(p->data[k]);
            if (p->data[k] == 0xF8) { clocks++; continue; }       // too many to print; counted
            if (n) printf("  %-14s (0x%02X)   [clocks so far: %lu]\n", n, p->data[k], clocks);
            else   printf("  byte 0x%02X\n", p->data[k]);
        }
        fflush(stdout);
        p = MIDIPacketNext(p);
    }
}

int main(int argc, char **argv) {
    int secs = argc > 1 ? atoi(argv[1]) : 30;
    MIDIClientRef client; MIDIPortRef port;
    MIDIClientCreate(CFSTR("midimon"), NULL, NULL, &client);
    MIDIInputPortCreate(client, CFSTR("midimon in"), cb, NULL, &port);
    ItemCount n = MIDIGetNumberOfSources();
    printf("listening on %lu source(s) for %ds:\n", (unsigned long)n, secs);
    for (ItemCount i = 0; i < n; i++) {
        MIDIEndpointRef src = MIDIGetSource(i);
        CFStringRef cf = NULL; char nm[128] = {0};
        if (MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &cf) == noErr && cf) {
            CFStringGetCString(cf, nm, sizeof nm, kCFStringEncodingUTF8); CFRelease(cf);
        }
        printf("  [%lu] %s\n", (unsigned long)i, nm);
        MIDIPortConnectSource(port, src, NULL);
    }
    printf("--- press PLAY then STOP in Live now ---\n"); fflush(stdout);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, secs, false);
    printf("done. total clock ticks: %lu\n", clocks);
    return 0;
}
