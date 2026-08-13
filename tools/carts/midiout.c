/* de:meta
{
  "slug": "midiout",
  "title": "midi out",
  "status": "active",
  "created": "2026-08-13",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization"
  ],
  "lineage": "The midi_output.h self-test cart, the OUT-direction twin of synccheck: it plays a pattern on some OTHER instrument -- another app, or gear on a cable -- and shows every byte as it leaves, so 'is my cart actually reaching the DAW, and on which channel' is one look instead of a guess. Also the worked example of the channel convention the racks will use: pitched parts get a channel each, a drum machine gets ONE channel with its voices as GM note numbers.",
  "description": {
    "summary": "Sends a pattern OUT to another instrument over MIDI -- a bassline on channel 1, drums as GM notes on channel 10 -- and logs every message as it goes.",
    "detail": "The cart makes no sound of its own; the point is what comes out the other end. It appears to the system as a MIDI device called 'dreamengine' that a DAW picks as a track input. Two parts play at once and they show the whole channel convention: a 16-step bassline on CHANNEL 1, where the notes are notes, and a drum pattern on CHANNEL 10, where each voice is a fixed GM note number (36 kick, 38 snare, 42 closed hat) rather than a channel of its own -- which is how every drum machine addresses a DAW's drum rack. A filter-cutoff sweep rides out as CC 74 on channel 1, and the transport is mirrored as real MIDI clock at 24 ticks per quarter note, so the receiving gear follows this cart's tempo. The log on the right shows the last dozen messages with their channel, which is the part worth watching: if a receiver is silent, this tells you whether the problem is us not sending or them not listening.",
    "controls": "SPACE plays/stops (and sends MIDI start/stop). LEFT/RIGHT change tempo. C toggles sending clock. D toggles the drum part."
  }
}
de:meta */
#include "studio.h"
#include <stdio.h>
#include <stdarg.h>

// MIDI OUT — the midi_output.h self-test (the out-direction twin of synccheck).
//
// PASS, driven into a DAW: open a MIDI track, choose "dreamengine" as its input, arm it.
// Press SPACE here and the DAW's transport follows, its input meter moves on every step,
// and a drum rack on the same track lights kick/snare/hat rather than three random pitches.
//
// PASS, headless + deterministic (this is what tools/midi-out-check/run.sh asserts):
//   node tools/play.js midiout script /dev/null --headless --frames 240
// The sequence is driven off frame() rather than wall time, so the same frames always emit
// the same messages, and the listener can diff against a fixed expectation. What it proves is
// narrow but real: the bytes left the process with the right channel, note and value on them.
// It cannot prove a DAW liked them — that is the eyeball step above.

#define STEPS 16

// The bassline: semitones above the root, -1 = rest. Deliberately sparse so the log is
// readable at 60fps rather than a wall of text.
static const int BASS[STEPS] = { 0, -1, 12, -1, 7, -1, 0, 3, -1, -1, 10, -1, 5, -1, 3, -1 };

// The drum part, as GM percussion note numbers on channel 10 — the convention this cart
// exists to demonstrate. These are not our voice indices; they are where a DAW's drum rack
// expects to find a kick, a snare and a closed hat, which is why a receiving kit just works.
#define GM_KICK  36
#define GM_SNARE 38
#define GM_HAT   42
static const int KICK[STEPS]  = { 1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,1,0 };
static const int SNARE[STEPS] = { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 };
static const int HAT[STEPS]   = { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,1 };

#define CH_BASS  1
#define CH_DRUM  10          // GM's reserved drum channel
#define ROOT     36          // C2 — bassline root
#define CC_CUT   74          // "filter cutoff" by GM convention

static int playing   = 0;
static int send_clk  = 1;
static int send_drum = 1;
static int tempo       = 120;

static int step      = -1;   // last step played (so a step fires once)
static int held      = -1;   // the bassline note currently sounding, -1 = none
static int last_cc   = -1;   // only send CC when the value CHANGES (a 0..127 knob at 60fps is a flood)
static int clk_sent  = 0;    // ticks emitted this run

// ── the IN direction, so this one cart covers both halves of the wire ──
// Not decoration: CC-in is the newer path of the two and the channel nibble is the part most
// likely to be silently wrong, so the gate reads these back out of a --trace.
static int in_ch = -1, in_cc = -1, in_val = -1;   // the last knob move that arrived
static int in_count = 0;                          // how many have arrived at all

// A small ring of what we sent, for the on-screen log.
#define LOGN 12
static char logbuf[LOGN][40];
static int  logw = 0;
static void logmsg(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(logbuf[logw % LOGN], sizeof logbuf[0], fmt, ap);
    va_end(ap);
    logw++;
}

// Where are we in musical time? Driven off frame() so a headless run is reproducible —
// wall-clock would make the gate flaky for no benefit.
static double beats_now(void) { return frame() / 60.0 * (tempo / 60.0); }

static void all_off(void) {
    if (held >= 0) { midi_send_note(CH_BASS, held, 0, 0); held = -1; }
    // Drums are one-shots (note-off sent immediately), so nothing to release there.
}

void update(void) {
    // Drain every knob move that arrived. Drained rather than polled here because the readout
    // wants to name WHICH knob moved, which a poll cannot answer (that is midi_cc's job, and the
    // panel uses it below for the one CC we care about).
    int c, n, v;
    while (midi_cc_get(&c, &n, &v)) { in_ch = c; in_cc = n; in_val = v; in_count++; }

    if (keyp(' ')) {
        playing = !playing;
        if (playing) { midi_send_start(1); logmsg("START"); step = -1; }
        else         { all_off(); midi_send_stop(); logmsg("STOP"); }
    }
    if (keyp(KEY_RIGHT) && tempo < 200) tempo += 5;
    if (keyp(KEY_LEFT)  && tempo > 40)  tempo -= 5;
    if (keyp('C')) send_clk  = !send_clk;
    if (keyp('D')) send_drum = !send_drum;

    // Guarded rather than an early `return`, so the trace block at the bottom is reached even
    // when stopped — the IN-direction assertions need it on a run that never presses play.
    if (playing) {
    double beats = beats_now();

    // ── clock out: 24 ticks per quarter note, the same PPQN sync.h consumes ──
    // Catch up rather than send one per frame: at 60fps and 120bpm that is 48 ticks/sec,
    // which is not a whole number of frames, so a per-frame tick would drift the tempo we
    // are advertising away from the one we are playing.
    if (send_clk) {
        int want = (int)(beats * 24.0);
        while (clk_sent < want) { midi_send_clock(); clk_sent++; }
    }

    // ── the step grid ──
    int s = (int)(beats * 4.0) % STEPS;      // 16ths
    if (s != step) {
        step = s;

        // bassline on its own channel: release the previous note before starting the next,
        // so a receiver in mono/legato mode glides instead of stacking.
        if (held >= 0) { midi_send_note(CH_BASS, held, 0, 0); held = -1; }
        if (BASS[s] >= 0) {
            held = ROOT + BASS[s];
            midi_send_note(CH_BASS, held, 100, 1);
            logmsg("ch%-2d note %3d on", CH_BASS, held);
        }

        // drums: one channel, voices as GM note numbers. Fired as note-on then immediately
        // note-off — a drum hit has no length, and a receiver that waits for the off would
        // otherwise hold a sample forever.
        if (send_drum) {
            if (KICK[s])  { midi_send_note(CH_DRUM, GM_KICK, 110, 1);  midi_send_note(CH_DRUM, GM_KICK, 0, 0);  logmsg("ch%-2d KICK  %d", CH_DRUM, GM_KICK); }
            if (SNARE[s]) { midi_send_note(CH_DRUM, GM_SNARE, 100, 1); midi_send_note(CH_DRUM, GM_SNARE, 0, 0); logmsg("ch%-2d SNARE %d", CH_DRUM, GM_SNARE); }
            if (HAT[s])   { midi_send_note(CH_DRUM, GM_HAT, 70, 1);    midi_send_note(CH_DRUM, GM_HAT, 0, 0);   logmsg("ch%-2d HAT   %d", CH_DRUM, GM_HAT); }
        }
    }

    // ── a cutoff sweep as CC, sent only on CHANGE ──
    // The same set-and-hold discipline the engine's own effects want: a knob wired straight
    // into the frame would put 60 messages a second on a wire shared with the notes.
    int cut = (int)(63.0 + 63.0 * de_sin_turns(beats / 8.0));
    if (cut != last_cc) {
        midi_send_cc(CH_BASS, CC_CUT, cut);
        last_cc = cut;
    }
    }   // end if (playing)

#ifdef DE_TRACE
    watch("step", "%d", step);
    watch("clk",  "%d", clk_sent);
    watch("cut",  "%d", last_cc);
    // the IN half — tools/midi-check/run.sh asserts these. midi_cc() is read per CHANNEL on
    // purpose: three different channels prove the nibble survives the trip, which one would not.
    watch("in_n",   "%d", in_count);
    watch("in_ch",  "%d", in_ch);
    watch("in_cc",  "%d", in_cc);
    watch("cc1_74", "%d", midi_cc(1,  74));
    watch("cc10_7", "%d", midi_cc(10,  7));
    watch("cc16_1", "%d", midi_cc(16,  1));
    // the channel-ISOLATION probe: cc 74 is only ever sent on ch 1, so ch 2 must read -1.
    // This is the only one of these that can catch a parser which drops the channel nibble.
    watch("cc2_74", "%d", midi_cc(2,  74));
    // omni must agree with the per-channel read for a CC that only ever arrives on one channel
    watch("omni74", "%d", midi_cc(0,  74));
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    print("MIDI OUT", 8, 6, CLR_WHITE);

    // Is the port actually open? Worded as "not sending" rather than an error, because there are
    // three ways to be false and only one is a fault: not macOS/iOS · the system refused · or this
    // is an AUTOMATED run (headless, or a --run screenshot bake), where the engine suppresses
    // output on purpose so a batch sweep cannot fire notes into the dev's open DAW. That last one
    // is the common case, and it is also what this cart's own thumbnail is baked in — so a red
    // "NO MIDI OUT" there would be a lie about a system that is working exactly as designed.
    bool live = midi_out_ready();
    print(live ? "out: dreamengine" : "out: not sending", 8, 18,
          live ? CLR_GREEN : CLR_MEDIUM_GREY);

    char b[64];
    snprintf(b, sizeof b, "%s  %d bpm", playing ? "PLAYING" : "stopped", tempo);
    print(b, 8, 30, playing ? CLR_YELLOW : CLR_MEDIUM_GREY);
    snprintf(b, sizeof b, "clock %s   drums %s", send_clk ? "on" : "off", send_drum ? "on" : "off");
    print(b, 8, 42, CLR_LIGHT_GREY);

    // the step grid — bassline row + three drum rows
    int x0 = 8, y0 = 62, w = 9;
    for (int s = 0; s < STEPS; s++) {
        int on = (s == step && playing);
        rectfill(x0 + s * w, y0,      w - 2, 7, BASS[s]  >= 0 ? (on ? CLR_WHITE : CLR_ORANGE) : CLR_DARK_GREY);
        rectfill(x0 + s * w, y0 + 10, w - 2, 7, KICK[s]      ? (on ? CLR_WHITE : CLR_RED)    : CLR_DARK_GREY);
        rectfill(x0 + s * w, y0 + 20, w - 2, 7, SNARE[s]     ? (on ? CLR_WHITE : CLR_PINK)   : CLR_DARK_GREY);
        rectfill(x0 + s * w, y0 + 30, w - 2, 7, HAT[s]       ? (on ? CLR_WHITE : CLR_INDIGO) : CLR_DARK_GREY);
    }
    print("ch1", x0 + STEPS * w + 4, y0,      CLR_ORANGE);
    print("kik", x0 + STEPS * w + 4, y0 + 10, CLR_RED);
    print("snr", x0 + STEPS * w + 4, y0 + 20, CLR_PINK);
    print("hat", x0 + STEPS * w + 4, y0 + 30, CLR_INDIGO);

    snprintf(b, sizeof b, "cc%d cutoff %d", CC_CUT, last_cc < 0 ? 0 : last_cc);
    print(b, 8, y0 + 44, CLR_LIGHT_GREY);

    // the other direction: what a controller or DAW is sending US
    if (in_count > 0) snprintf(b, sizeof b, "in: ch%d cc%d = %d  (%d)", in_ch, in_cc, in_val, in_count);
    else              snprintf(b, sizeof b, "in: %s", midi_present() ? "waiting for a knob" : "no controller");
    print(b, 8, y0 + 52, in_count > 0 ? CLR_GREEN : CLR_DARK_GREY);

    // what actually went out
    print("sent:", 8, y0 + 60, CLR_MEDIUM_GREY);
    for (int i = 0; i < LOGN; i++) {
        int idx = logw - LOGN + i;
        if (idx < 0) continue;
        print(logbuf[idx % LOGN], 8, y0 + 70 + i * 9,
              i == LOGN - 1 ? CLR_WHITE : CLR_MEDIUM_GREY);
    }

    print("SPACE play  <> bpm  C clock  D drums", 8, SCREEN_H - 12, CLR_DARK_GREY);
}
