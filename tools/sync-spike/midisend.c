// midisend — throwaway CoreMIDI clock GENERATOR, to reproduce "a DAW that was already playing".
// Sends bare 0xF8 ticks at <bpm> to the first destination whose name contains "IAC".
//   ./midisend <bpm> <secs> [start]      "start" = also send 0xFA first (the polite case)
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    double bpm = argc > 1 ? atof(argv[1]) : 120.0;
    double secs = argc > 2 ? atof(argv[2]) : 10.0;
    int send_start = argc > 3 && strcmp(argv[3], "start") == 0;

    MIDIClientRef client; MIDIPortRef port;
    MIDIClientCreate(CFSTR("midisend"), NULL, NULL, &client);
    MIDIOutputPortCreate(client, CFSTR("midisend out"), &port);

    MIDIEndpointRef dest = 0; char nm[128] = {0};
    ItemCount n = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < n; i++) {
        MIDIEndpointRef d = MIDIGetDestination(i);
        CFStringRef cf = NULL; char t[128] = {0};
        if (MIDIObjectGetStringProperty(d, kMIDIPropertyDisplayName, &cf) == noErr && cf) {
            CFStringGetCString(cf, t, sizeof t, kCFStringEncodingUTF8);
            CFRelease(cf);
        }
        if (strstr(t, "IAC")) { dest = d; snprintf(nm, sizeof nm, "%s", t); break; }
    }
    if (!dest) { printf("no IAC destination found (%lu destinations)\n", (unsigned long)n); return 1; }
    printf("sending %s clock at %.1f bpm for %.1fs to \"%s\"\n",
           send_start ? "START +" : "BARE (no START)", bpm, secs, nm);
    fflush(stdout);

    unsigned char buf[256];
    MIDIPacketList *pl = (MIDIPacketList *)buf;
    if (send_start) {
        MIDIPacket *p = MIDIPacketListInit(pl);
        unsigned char st = 0xFA;
        MIDIPacketListAdd(pl, sizeof buf, p, 0, 1, &st);
        MIDISend(port, dest, pl);
    }
    double tick_us = 60.0 / (bpm * 24.0) * 1e6;
    long ticks = (long)(secs * 1e6 / tick_us);
    unsigned char clk = 0xF8;
    for (long i = 0; i < ticks; i++) {
        MIDIPacket *p = MIDIPacketListInit(pl);
        MIDIPacketListAdd(pl, sizeof buf, p, 0, 1, &clk);
        MIDISend(port, dest, pl);
        usleep((useconds_t)tick_us);
    }
    if (send_start) {                      // …and a STOP at the end, so the whole start→stop arc is testable
        MIDIPacket *p = MIDIPacketListInit(pl);
        unsigned char sp = 0xFC;
        MIDIPacketListAdd(pl, sizeof buf, p, 0, 1, &sp);
        MIDISend(port, dest, pl);
        printf("sent STOP\n");
    }
    printf("sent %ld clock ticks\n", ticks);
    return 0;
}
