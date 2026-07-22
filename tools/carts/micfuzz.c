/* de:meta
{
  "slug": "micfuzz",
  "title": "mic fuzz (pedal-tier spike)",
  "status": "active",
  "created": "2026-07-22",
  "kind": [
    "tech-demo"
  ],
  "teaches": [
    "audio-input"
  ],
  "lineage": "The pedal-tier spike (audio-input-frontier.md ★#1): proves the live mic can be routed THROUGH the master fx chain in real time via the new input_monitor() primitive — the 'guitar into the pedalboard' path. Play/sing/clap into the mic and hear yourself fuzzed/filtered/chorused live. Precursor to a mic-in mode for the pedalboard cart.",
  "description": {
    "summary": "The engine can now hear you AND process it. Enable the mic and your own sound (voice, guitar, claps) runs through a bitcrush + filter + chorus chain in real time — the proof that live signal-in through the effects bus works.",
    "detail": "Rides input_monitor(): the host's mic ring is summed into master bus 0's DRY mix, so the reorderable fx_order chain (here crush/filter/chorus) processes it exactly like an instrument. LIVE-only (breaks .rec replay — decision 0032). This is the spike for feeling latency before wiring the same path into the pedalboard cart's real chain. Turn all three off for a clean-monitor A/B.",
    "controls": "TAP the MIC pad (or SPACE) to arm the microphone (pops the OS permission prompt the first time). Then TAP FUZZ / FILTER / CHORUS to toggle each pedal; TAP GAIN - / + to trim the input level. Make noise into your mic."
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// MIC FUZZ — the pedal-tier spike. input_monitor(gain) routes the live mic into the
// master fx chain; crush()/filter()/chorus() are ordinary master inserts, so they
// process your own sound the same way they'd process a synth voice. Effects are
// SET-AND-HOLD: (re)configured only when a toggle flips, never per frame (filter is
// the exception — it's built to be ridden live, so we sweep its cutoff each frame).

static bool  armed   = false;     // mic + monitor engaged
static float gain    = 1.2f;      // input_monitor gain
static bool  fuzz_on = false, filt_on = false, chor_on = false;
static float peak    = 0.0f;      // VU peak-hold
static float lfo     = 0.0f;      // filter sweep phase

// applied-state shadows so we only push fx config on a real change (set-and-hold rule)
static int   ap_armed = -1;
static float ap_gain  = -1.0f;
static int   ap_fuzz  = -1, ap_filt = -1, ap_chor = -1;

static bool button(int x, int y, int w, int h, const char *label, bool on, int accent) {
    bool hit = tapp(x, y, w, h);
    rrectfill(x, y, w, h, 3, on ? accent : CLR_DARKER_GREY);
    rrect(x, y, w, h, 3, on ? CLR_WHITE : CLR_DARK_GREY);
    print_centered(label, x + w / 2, y + h / 2 - 3, on ? CLR_BLACK : CLR_LIGHT_GREY);
    return hit;
}

void update(void) {
    // arm/disarm the mic (the permission prompt attaches to this user action)
    if (keyp(KEY_SPACE) || tapp(20, 30, 120, 46)) {
        armed = !armed;
        if (armed) mic_start(); else mic_stop();
    }
    if (armed) {
        if (tapp(20,  92, 92, 34)) fuzz_on = !fuzz_on;
        if (tapp(114, 92, 92, 34)) filt_on = !filt_on;
        if (tapp(208, 92, 92, 34)) chor_on = !chor_on;
        if (tapp(20,  138, 44, 30)) gain = fmaxf(0.0f, gain - 0.2f);
        if (tapp(96,  138, 44, 30)) gain = fminf(4.0f, gain + 0.2f);
    }

    // ── push audio config only on change (set-and-hold) ───────────────────────────
    int want_mon = armed ? 1 : 0;
    if (want_mon != ap_armed || (armed && gain != ap_gain)) {
        input_monitor(armed ? gain : 0.0f);
        ap_armed = want_mon; ap_gain = gain;
    }
    if ((int)fuzz_on != ap_fuzz) {                       // bitcrush: gnarly when on, mix 0 = off
        crush(4.0f, 3.0f, fuzz_on ? 1.0f : 0.0f);
        ap_fuzz = fuzz_on;
    }
    if ((int)chor_on != ap_chor) {
        chorus(1.4f, 0.6f, chor_on ? 0.6f : 0.0f);
        ap_chor = chor_on;
    }
    if ((int)filt_on != ap_filt) {                       // toggling OFF = one bypass call
        if (!filt_on) filter(FILTER_OFF, 20000.0f, 0.0f);
        ap_filt = filt_on;
    }
    if (filt_on) {                                       // filter is rideable live → sweep the cutoff
        lfo += 0.02f;
        float cut = 300.0f + (1.0f + sinf(lfo)) * 1600.0f;   // ~300..3500 Hz wah sweep
        filter(FILTER_LOW, cut, 0.5f);
    }

    float lvl = mic_level();
    if (lvl > peak) peak = lvl;
    peak *= 0.94f;
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    font(FONT_NORMAL);
    print("MIC -> FX   the pedal-tier spike", 20, 10, CLR_LIGHT_PEACH);

    // MIC arm pad
    bool live = armed && mic_active();
    rrectfill(20, 30, 120, 46, 4, live ? CLR_DARK_GREEN : (armed ? CLR_DARK_RED : CLR_DARKER_GREY));
    rrect(20, 30, 120, 46, 4, CLR_WHITE);
    print_centered(armed ? (live ? "MIC: LIVE" : "MIC: waiting") : "TAP TO ARM MIC", 80, 46, CLR_WHITE);
    font(FONT_SMALL);
    print_centered(armed ? "(tap to stop)" : "or press SPACE", 80, 60, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // VU meter (right of the pad)
    print("IN", 168, 34, CLR_MEDIUM_GREY);
    rrectfill(168, 46, 132, 16, 2, CLR_BLACK);
    int w = (int)(fminf(1.0f, mic_level() * 4.0f) * 128.0f);
    rectfill(170, 48, w, 12, mic_level() > 0.5f ? CLR_ORANGE : CLR_LIME_GREEN);
    int px = 170 + (int)(fminf(1.0f, peak * 4.0f) * 128.0f);
    line(px, 48, px, 59, CLR_WHITE);

    // pedals
    button(20,  92, 92, 34, "FUZZ",   fuzz_on, CLR_ORANGE);
    button(114, 92, 92, 34, "FILTER", filt_on, CLR_BLUE);
    button(208, 92, 92, 34, "CHORUS", chor_on, CLR_PINK);

    // gain trim
    print("GAIN", 20, 132, CLR_MEDIUM_GREY);
    button(20, 138, 44, 30, "-", false, CLR_DARKER_GREY);
    button(96, 138, 44, 30, "+", false, CLR_DARKER_GREY);
    print_centered(str("%.1f", gain), 80, 149, CLR_WHITE);

    font(FONT_SMALL);
    if (!armed) {
        print("Arm the mic, then make noise into it - claps, voice, a guitar.", 20, 182, CLR_MEDIUM_GREY);
    } else {
        print("Play into your mic. All 3 OFF = clean monitor (the A/B).", 20, 176, CLR_LIGHT_GREY);
        print("LIVE signal path - does NOT record/replay (decision 0032).", 20, 188, CLR_DARK_GREY);
    }
    font(FONT_NORMAL);
}
