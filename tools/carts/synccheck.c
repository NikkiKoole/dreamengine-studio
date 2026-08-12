/* de:meta
{
  "slug": "synccheck",
  "title": "sync check",
  "status": "active",
  "created": "2026-08-12",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "algorithm-visualization"
  ],
  "lineage": "The external-clock self-test cart, the sync.h twin of soundcheck: it shows and sounds whatever clock is driving the engine (MIDI clock from a DAW or hardware today; an AUv3 host or Ableton Link later) so 'is the clock reaching us, and is my step counter locked to it' is one look and one listen instead of a guess.",
  "description": {
    "summary": "Shows and clicks along to an EXTERNAL clock — a DAW's MIDI sync, a drum machine's clock out, or the synthetic harness clock.",
    "detail": "The whole cart is one question: is something outside driving our tempo, and is our step counter on ITS grid? The readout gives sync_active/sync_playing/sync_transport/sync_bpm/sync_beats plus the bar:beat:16th they imply, next to the MIDI device we're listening to. THREE external states, and the orange one is the diagnostic that matters: EXT PLAYING and EXT STOPPED mean the clock drives our transport, while EXT TEMPO means a clock is here but never sent START/STOP (a bare clock, or we joined mid-flow) so we borrow its tempo and keep our own play/stop -- taking the transport there is what once left acidcandy unstartable. A 16-step playhead runs off sync_beats() when a clock is present and off the cart's own accumulator when it isn't (the panel says which), and each 16th clicks — accented on the downbeat — so the lock is audible, not just visible. Two ways to drive it: press PLAY in a DAW whose MIDI sync output is routed to the IAC bus, or run it headless with --midi-clock <bpm>. FALLBACK is the interesting state: pull the cable and it keeps playing at the last tempo it heard, because sync.h hands control back after a 2s silence.",
    "controls": "Nothing to press — it follows whatever clock it is given. SPACE toggles the internal fallback transport (for when no clock is present)."
  }
}
de:meta */
#include "studio.h"

// SYNC CHECK — the external-clock self-test (sync.h's twin of soundcheck).
//
// PASS, driven from a DAW: press play, EXT lights up, BPM reads the DAW's tempo within a
// beat or two, the playhead marches, the clicks land with the DAW's own metronome, and
// pressing stop in the DAW stops the playhead (not the cart).
//
// If the badge is ORANGE ("EXT TEMPO"), the DAW's clock is reaching us but its START never
// did — the usual cause is that it was ALREADY playing when this cart booted, so we joined
// mid-flow. Stop and re-start the DAW's transport and the badge should go green.
//
// PASS, headless + deterministic:
//   node tools/play.js synccheck script /dev/null --headless --frames 300 --midi-clock 120 --trace build/sync.jsonl
// then read the trace: `act` must be 1 throughout (after the first frame), `bpm` must settle
// on 120 ± 1, and `beats` must reach frames/60 * 120/60 = 10.0 at frame 300. That last one is
// the real assertion — it is the ONLY check that the tick count and the tempo agree, i.e. that
// we are not simply running our own clock while claiming to follow theirs.

static int   step = 0;         // 0..15, the 16th-note playhead
static int   last16 = -1;      // last 16th we fired a click on (edge detect)
static float own_phase = 0;    // the internal fallback clock, in 16ths
static bool  own_run = true;   // fallback transport (SPACE)
static float last_bpm = 120;   // remembered so pulling the cable doesn't lurch the tempo

void init(void) {
    bpm(120);
}

void update(void) {
    if (keyp(KEY_SPACE)) own_run = !own_run;

    bool ext = sync_active();
    if (ext && sync_bpm() > 0) last_bpm = sync_bpm();

    // THE POINT OF THE WHOLE CART: with an external clock, the step counter is DERIVED from
    // its beat position (so a re-start or a loop jump lands us where the clock says); without
    // one, we accumulate our own. Same counter, two sources, and the cart barely notices.
    float sixteenths;
    if (ext) {
        sixteenths = sync_beats() * 4.0f;
    } else {
        if (own_run) own_phase += dt() * (last_bpm / 60.0f) * 4.0f;
        sixteenths = own_phase;
    }

    // A clock that drives TRANSPORT owns our play/stop; a TEMPO-ONLY one (bare MIDI clock that
    // never sent START, or that we joined mid-flow) does not, and taking it anyway is what left
    // acidcandy unstartable in the first real Live session. Follow the same rule here.
    bool running = ext ? (sync_transport() ? sync_playing() : own_run) : own_run;
    int  s16 = (int)sixteenths;
    step = ((s16 % 16) + 16) % 16;

    // click on each new 16th, accented on the downbeat — the lock has to be AUDIBLE, because
    // a playhead that looks right and drifts by 30ms is exactly the bug this cart exists to catch
    if (running && s16 != last16) {
        last16 = s16;
        if (step == 0)          hit(84, INSTR_SQUARE, 4, 40);
        else if (step % 4 == 0) hit(72, INSTR_SQUARE, 2, 25);
        else                    hit(72, INSTR_NOISE,  1, 15);
    }
    if (!running) last16 = -1;

#ifdef DE_TRACE
    watch("act",   "%d",   ext ? 1 : 0);
    watch("play",  "%d",   running ? 1 : 0);
    watch("xport", "%d",   sync_transport() ? 1 : 0);   // 0 while active = a TEMPO-ONLY clock
    watch("bpm",   "%.2f", sync_bpm());
    watch("beats", "%.4f", sync_beats());
    watch("step",  "%d",   step);
#endif
}

void draw(void) {
    bool ext = sync_active();
    bool running = ext ? (sync_transport() ? sync_playing() : own_run) : own_run;   // same rule as update()
    cls(CLR_DARK_BLUE);

    print("EXTERNAL CLOCK", 8, 8, CLR_LIGHT_GREY);

    // the source badge — the one thing you look at from across the room. THREE ext states, not
    // two: "EXT TEMPO" is the diagnostic that matters, it means a clock is here but it never sent
    // START/STOP, so we borrow its tempo and keep our own transport.
    bool xport = sync_transport();
    const char *tag = !ext ? "FALLBACK" : !xport ? "EXT TEMPO" : running ? "EXT PLAYING" : "EXT STOPPED";
    int badge = !ext ? CLR_DARK_GREY : !xport ? CLR_ORANGE : running ? CLR_GREEN : CLR_YELLOW;
    rectfill(8, 22, 96, 18, badge);
    print(tag, 14, 28, ext ? CLR_BLACK : CLR_LIGHT_GREY);

    // the numbers
    int y = 52;
    print(str("bpm    %.1f", ext ? sync_bpm() : last_bpm), 8, y, CLR_WHITE);              y += 10;
    print(str("beats  %.3f", sync_beats()), 8, y, ext ? CLR_WHITE : CLR_DARK_GREY);       y += 10;
    // bar:beat:16th, 4/4 — the form a musician can check against their DAW's own display
    int b16 = (int)(sync_beats() * 4.0f);
    print(str("pos    %d:%d:%d", b16 / 16 + 1, (b16 / 4) % 4 + 1, b16 % 4 + 1),
          8, y, ext ? CLR_WHITE : CLR_DARK_GREY);                                         y += 14;
    print(str("midi   %s", midi_present() ? midi_name() : "(nothing connected)"),
          8, y, midi_present() ? CLR_LIGHT_GREY : CLR_DARK_GREY);

    // 16-step playhead
    for (int i = 0; i < 16; i++) {
        int x = 8 + i * 19;
        bool on = running && i == step;
        rectfill(x, 120, 16, 16, on ? CLR_WHITE : (i % 4 == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY));
    }

    print(!ext ? "no clock: own tempo (SPACE)" : xport ? "following an outside clock"
                                                        : "tempo only: no start/stop sent",
          8, 150, CLR_MEDIUM_GREY);
    print("DAW: MIDI sync out -> IAC bus", 8, 164, CLR_DARK_GREY);
    print("or: play.js --midi-clock 120", 8, 174, CLR_DARK_GREY);
}
