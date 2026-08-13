// listen.c — the ORACLE half of the MIDI-out gate: connect to the engine's virtual source
// and print every message it sends, DECODED, one per line, in a form a script can diff.
//
// Why its own listener instead of tools/sync-spike/midimon.c: midimon prints raw bytes and
// names only the transport singles, because it was built to answer "what is Ableton actually
// sending". A gate needs the other thing — channel, note, velocity, controller number —
// since the whole point of this API is that CHANNELS are part of it, and a byte dump cannot
// tell you that note 36 went out on channel 10 rather than channel 1.
//
// It connects ONLY to sources whose display name matches (default "dreamengine"), so a
// keyboard plugged into the same machine, or a DAW idling on the IAC bus, cannot pollute the
// run. That matters here: this repo has already been bitten by an ambient clock leaking into
// a supposedly-isolated run (docs/design/external-clock-sync.md → "The ambient clock").
//
//   clang -o listen listen.c -framework CoreMIDI -framework CoreFoundation
//   ./listen <seconds> [source-name-substring]
//
// Output lines (stable — run.sh matches on these):
//   src <name>              a matching source was found and connected
//   note on  ch=N note=N vel=N
//   note off ch=N note=N vel=N
//   cc       ch=N cc=N val=N
//   bend     ch=N val=N          (-8192..8191)
//   clock | start | continue | stop
//   done clocks=N

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static unsigned long clocks = 0;

// Milliseconds since the first message. Printed on every line because a MIDI defect can live
// entirely in the TIMING — a note whose off arrives in the same millisecond as its on is legal,
// balanced, and still wrong, and a gate that only counts on/off pairs cannot see it.
static double t0 = -1;
static double now_ms(void) {
    struct timeval tv; gettimeofday(&tv, NULL);
    double t = tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
    if (t0 < 0) t0 = t;
    return t - t0;
}

// Decode a packet's bytes. Same shape as midi_input.h's parser (system-realtime first,
// because those bytes arrive INTERLEAVED inside other messages), so if this disagrees with
// the engine's own parser about a byte, that disagreement is itself worth knowing.
static void decode(const unsigned char *d, unsigned n) {
    double ms = now_ms();
    unsigned k = 0;
    while (k < n) {
        if (d[k] >= 0xF8) {
            switch (d[k]) {
                case 0xF8: clocks++; break;                 // counted, not printed (24/quarter is a flood)
                case 0xFA: printf("[%8.1f] start\n", ms);    break;
                case 0xFB: printf("[%8.1f] continue\n", ms); break;
                case 0xFC: printf("[%8.1f] stop\n", ms);     break;
                default: break;                             // active-sensing / reset: ignored
            }
            k += 1; continue;
        }
        unsigned char status = d[k] & 0xF0;
        int ch = (d[k] & 0x0F) + 1;                         // print 1..16, like the gear does
        if ((status == 0x90 || status == 0x80) && k + 2 < n) {
            // A note-on with velocity 0 is a note-off on the wire. Print what it MEANS, so a
            // sender that uses either form reads the same here — the gate is about intent.
            int on = (status == 0x90 && d[k + 2] > 0);
            printf("[%8.1f] note %s ch=%d note=%d vel=%d\n", ms, on ? "on " : "off", ch, d[k + 1], d[k + 2]);
            k += 3;
        } else if (status == 0xB0 && k + 2 < n) {
            printf("[%8.1f] cc       ch=%d cc=%d val=%d\n", ms, ch, d[k + 1], d[k + 2]);
            k += 3;
        } else if (status == 0xE0 && k + 2 < n) {
            printf("[%8.1f] bend     ch=%d val=%d\n", ms, ch, (((int)d[k + 2] << 7) | d[k + 1]) - 8192);
            k += 3;
        } else if (status == 0xC0 || status == 0xD0) { k += 2;
        } else { k += 1;
        }
    }
    fflush(stdout);
}

// Setup changed (a source appeared/vanished). We only need the notification to EXIST so the
// client refreshes its cached view; the polling loop below does the actual looking.
static void notify(const MIDINotification *msg, void *ctx) { (void)msg; (void)ctx; }

static void cb(const MIDIPacketList *pkts, void *a, void *b) {
    (void)a; (void)b;
    const MIDIPacket *p = &pkts->packet[0];
    for (unsigned i = 0; i < pkts->numPackets; i++) { decode(p->data, p->length); p = MIDIPacketNext(p); }
}

int main(int argc, char **argv) {
    int   secs = argc > 1 ? atoi(argv[1]) : 5;
    const char *want = argc > 2 ? argv[2] : "dreamengine";

    MIDIClientRef client; MIDIPortRef port;
    // A notify proc is REQUIRED, not optional decoration. A long-lived CoreMIDI client keeps a
    // CACHED view of the MIDI system and only refreshes it while its run loop is pumped and a
    // notification is delivered — so with NULL here (and no pumping) MIDIGetNumberOfSources()
    // answers 0 forever, even as a brand-new source is plainly visible to any freshly-launched
    // process. That is precisely how this gate failed twice: a one-shot `ls-src` saw the engine's
    // port while this listener, polling in a loop, never did. runtime/midi_input.h has always
    // done this correctly (midi_notify → midi_connect_sources on kMIDIMsgSetupChanged); the
    // listener just had to learn the same lesson.
    if (MIDIClientCreate(CFSTR("midi-check-listen"), notify, NULL, &client) != noErr) {
        fprintf(stderr, "listen: MIDIClientCreate failed\n"); return 2;
    }
    if (MIDIInputPortCreate(client, CFSTR("in"), cb, NULL, &port) != noErr) {
        fprintf(stderr, "listen: MIDIInputPortCreate failed\n"); return 2;
    }

    // The engine creates its source LAZILY (first send), so it may not exist yet when we
    // start — which is the normal case, since run.sh starts the listener first to avoid
    // missing the opening messages. So poll for it instead of scanning once.
    //
    // ⚠ The delay here MUST be a real sleep. The first cut used
    // CFRunLoopRunInMode(…, 0.1, false) as the pause, which returns IMMEDIATELY with
    // kCFRunLoopRunFinished when the run loop has no sources installed yet — so the whole
    // "wait up to N seconds" loop burned through in under a millisecond and the gate reported
    // "no source" while the engine was publishing one perfectly well. A busy-wait that looks
    // like a timeout is a nasty shape: the failure names the wrong component.
    int found = 0;
    for (int pass = 0; pass < secs * 10 && !found; pass++) {
        ItemCount n = MIDIGetNumberOfSources();
        for (ItemCount i = 0; i < n; i++) {
            MIDIEndpointRef src = MIDIGetSource(i);
            CFStringRef cf = NULL; char nm[128] = {0};
            if (MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &cf) == noErr && cf) {
                CFStringGetCString(cf, nm, sizeof nm, kCFStringEncodingUTF8); CFRelease(cf);
            }
            if (strstr(nm, want)) {
                if (MIDIPortConnectSource(port, src, NULL) == noErr) {
                    printf("src %s\n", nm); fflush(stdout);
                    found = 1;
                }
                break;
            }
        }
        if (!found) {
            // BOTH halves are needed, and each fixes a different bug:
            //   CFRunLoopRunInMode  — pumps the run loop so the setup-change notification is
            //                         delivered and the client's cached source list refreshes.
            //   usleep              — guarantees time actually passes, because the call above
            //                         returns INSTANTLY (kCFRunLoopRunFinished) whenever the
            //                         loop has no sources left to service.
            // Using only the first makes the wait window evaporate; only the second makes the
            // client permanently blind. Both failures print the same "no source" line.
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
            usleep(50000);
        }
    }
    if (!found) { printf("done clocks=0\n"); fprintf(stderr, "listen: no source matching \"%s\"\n", want); return 1; }

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, secs, false);
    printf("done clocks=%lu\n", clocks);
    return 0;
}

#pragma clang diagnostic pop
