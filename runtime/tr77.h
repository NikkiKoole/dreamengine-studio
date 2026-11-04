// tr77.h — the Roland Rhythm TR-77 (1972) VOICE BANK, tuned against real recordings
//
// The sideman.h move for the TR-77: one header owns the machine's voices, a cart owns the
// patterns (autorhythm plays the 18 rhythms off the service manual, runtime/rhythmbox.h).
//
//   void init(void) { tr77_build(TR77_BASE); }
//   tr77_fire(TR77_BASE, TR77_HB, 0, 0);            // voice, velocity boost (down-only), ms from now
//   int v[2], n = tr77_lane(lane_label, v);         // "CB+LB" -> cowbell AND low bongo
//
// HOW IT WAS VOICED (2026-11-04, tools/oneshot-compare.js, docs/guides/tuning-drum-voices.md).
// Two sample sets of the real machine, both local and for LEARNING ONLY, never shipped:
//   A  "Vintage Drums and Keys / Roland TR-77 Rhythm 77": 12 named one-shots (the reference here)
//   B  Reverb's "Roland Rhythm 77 Sample Pack": 11 one-shots + 41 loops (a second unit, or other
//      settings: its kick sits at 86 Hz and its hats at 12 kHz where A has 65 Hz and 8.5 kHz)
// What the measurements said, voice by voice:
//   BD  one round 65 Hz sine (exactly the CR-78's kick: the same circuit), a quiet 140 Hz partner
//       25 dB down, -20 dB in 115 ms
//   SD  a 355 Hz shell plus a broad noise rattle (half its energy above 3.7 kHz), -20 dB in 70 ms
//   RS  a CLICK, not a tone: one swing at ~1.15 kHz over a low knock, gone in 5 ms
//   HH  noise through a NARROW band at 8.5 kHz (the CR-78 "swish", 8.4 there), held ~25 ms and cut
//   CY  the same band held long: -9 dB at 90 ms, -20 dB at 255 ms
//   HB / LB / LC  damped sines settling at 361 / 218 / 145 Hz (each drifts UP as it dies), no DC
//       thud under them (a resonator rings around zero), -20 dB in 60 / 75 / 110 ms
//   C   a pure 2.25 kHz ping, -20 dB in 20 ms
//   TB  a very narrow ring at 4.3 kHz (CR-78: 4.65), -20 dB in ~190 ms
//   GU  a 6.2 kHz gourd scraped by teeth (INSTR_SCRAPE, the cr78 guiro's recipe)
//   CB, M  NO reference in either set: voiced as the CR-78's (its direct successor), and unmeasured
// Levels: neither set is a clean one-gain recording, but both put the tambourine ~9 dB and the
// hats 5-7 dB under the kick, so the balance below follows set A's peaks relative to its kick.
//
// The machine has no accent and no velocity (a diode matrix closes a trigger the same way every
// time), so `boost` is a DOWN-ONLY trim, as in sideman.h.
#ifndef TR77_H
#define TR77_H

enum { TR77_BD, TR77_SD, TR77_RS, TR77_HH, TR77_CY, TR77_HB, TR77_LB, TR77_LC,
       TR77_CB, TR77_CL, TR77_MA, TR77_GU, TR77_TB, TR77_TBS, TR77_ME, TR77_NV };

static const char *TR77_NAME[TR77_NV] = {
    "bass drum", "snare drum", "rim shot", "hi-hat", "cymbal", "high bongo", "low bongo",
    "low conga", "cowbell", "claves", "maracas", "guiro", "tambourine", "tambourine (soft)",
    "metronome",
};
static const char *TR77_SHORT[TR77_NV] = {
    "BASS", "SNARE", "RIM", "HAT", "CYMB", "BONG H", "BONG L", "CONGA", "COWB", "CLAV",
    "MARAC", "GUIRO", "TAMB", "TAMB'", "CLICK",
};

// ── slot layout (APPEND only) ────────────────────────────────────────────────
enum { T77S_BD, T77S_BD2, T77S_SDB, T77S_SDN, T77S_RS, T77S_RSK, T77S_HH, T77S_CY,
       T77S_HB, T77S_LB, T77S_LC, T77S_CB, T77S_CL, T77S_MA, T77S_GU, T77S_TB, T77S_ME };
#define TR77_NSLOT 17
#define TR77_BASE  23     // after sideman.h's 9..22, so one cart can hold both banks

// a short gate into an EXPONENTIAL release: a real hit falls in a straight line in dB
// (tuning-drum-voices.md, "the envelope SHAPE"); the release is ~2.2x the -20 dB time
static void tr77__exp(int slot, int wave, int rel) {
    instrument(slot, wave, 0, 0, 7, rel);
    instrument_release_shape(slot, RELEASE_EXP);
}

static void tr77_build(int base) {
    // BD: 65 Hz, a hair of pitch drop (67 -> 62 Hz measured), the 140 Hz partner 25 dB down
    tr77__exp(base + T77S_BD, INSTR_SINE, 310);
    instrument_filter(base + T77S_BD, FILTER_HIGH, 40, 2);    // no DC thud: see the toms below
    instrument_env(base + T77S_BD, 0, ENV_PITCH, 0, 30, 1.0f);
    tr77__exp(base + T77S_BD2, INSTR_SINE, 120);
    instrument_level(base + T77S_BD2, 0.12f);

    // SD: the 355 Hz shell + the rattle. A resonant LOWPASS for the rattle, not a band:
    // the real one's noise stops near 10 kHz (cr78's finding)
    tr77__exp(base + T77S_SDB, INSTR_SINE, 140);
    instrument_tune(base + T77S_SDB, 0.29f);           // F4 349 -> 355 Hz
    instrument_level(base + T77S_SDB, 0.40f);          // the rattle sits only ~10 dB under the shell
    tr77__exp(base + T77S_SDN, INSTR_NOISE, 150);
    instrument_filter(base + T77S_SDN, FILTER_LOW, 9000, 4);

    // RS: a click, the cr78 rim's recipe moved to ~1.15 kHz
    tr77__exp(base + T77S_RS, INSTR_TRI, 6);
    instrument_filter(base + T77S_RS, FILTER_BAND, 1100, 5);
    tr77__exp(base + T77S_RSK, INSTR_SINE, 8);         // the low knock under it

    // HH / CY: one narrow band, held short or long
    tr77__exp(base + T77S_HH, INSTR_NOISE, 14);
    instrument_filter(base + T77S_HH, FILTER_BAND, 8550, 15);
    tr77__exp(base + T77S_CY, INSTR_NOISE, 480);
    instrument_filter(base + T77S_CY, FILTER_BAND, 8650, 15);

    // bongos / conga: bare damped sines
    // Two things a bare damped sine gets wrong (2026-11-04, by ear: "a tonal thing"):
    //  - a RESONATOR's ring has no DC, a switched-on sine does: its first half-swing is never
    //    cancelled, a low thud under every hit (measured 5-8 dB more energy below 50 Hz than the
    //    real ones). A gentle highpass an octave under each note takes it out.
    //  - the real pitch DRIFTS UP as the ring dies (355 -> 361, 216 -> 218, 139 -> 145 Hz): tuned
    //    to where it settles, started a little flat.
    tr77__exp(base + T77S_HB, INSTR_SINE, 130);
    instrument_tune(base + T77S_HB, 0.58f);            // F4 349 -> 361 Hz, where it settles
    instrument_env(base + T77S_HB, 0, ENV_PITCH, 0, 100, -0.30f);
    instrument_filter(base + T77S_HB, FILTER_HIGH, 180, 2);
    tr77__exp(base + T77S_LB, INSTR_SINE, 165);
    instrument_tune(base + T77S_LB, -0.16f);           // A3 220 -> 218 Hz
    instrument_env(base + T77S_LB, 0, ENV_PITCH, 0, 120, -0.15f);
    instrument_filter(base + T77S_LB, FILTER_HIGH, 110, 2);
    tr77__exp(base + T77S_LC, INSTR_SINE, 280);
    instrument_tune(base + T77S_LC, 0.78f);            // C#3 138.6 -> 145 Hz
    instrument_env(base + T77S_LC, 0, ENV_PITCH, 0, 40, -0.50f);
    instrument_filter(base + T77S_LC, FILTER_HIGH, 75, 2);

    // CB: the CR-78 cowbell (a detuned square pair 554 / 784 Hz, lowpassed). NO TR-77 reference
    instrument(base + T77S_CB, INSTR_SQUARE, 0, 55, 0, 25);
    instrument_filter(base + T77S_CB, FILTER_LOW, 1100, 2);

    // C: a pure 2.25 kHz ping
    tr77__exp(base + T77S_CL, INSTR_SINE, 45);
    instrument_tune(base + T77S_CL, 0.25f);            // C#7 2217 -> 2250 Hz

    // M: the CR-78 maracas (a very short 8.7 kHz band). NO TR-77 reference
    instrument(base + T77S_MA, INSTR_NOISE, 0, 15, 0, 8);
    instrument_filter(base + T77S_MA, FILTER_BAND, 8700, 15);

    // GU: teeth through a gourd (cr78's fitted guiro)
    instrument(base + T77S_GU, INSTR_SCRAPE, 0, 0, 7, 30);
    instrument_harmonics(base + T77S_GU, 0.0f);         // BODY: gourd
    instrument_timbre(base + T77S_GU, 0.87f);           // RATE: ~75 teeth/s
    instrument_morph(base + T77S_GU, 0.0f);             // GESTURE: a steady scrape

    // TB: a very narrow ring at 4.3 kHz
    instrument(base + T77S_TB, INSTR_NOISE, 8, 200, 0, 60);
    instrument_filter(base + T77S_TB, FILTER_BAND, 4340, 15);

    // ME: the player's metronome, a short tick (not part of the kit; unmeasured)
    tr77__exp(base + T77S_ME, INSTR_SINE, 20);

    // the mix, relative to the kick (set A's peaks)
    instrument_level(base + T77S_HH, 0.13f);
    instrument_level(base + T77S_CY, 0.13f);
    instrument_level(base + T77S_TB, 0.10f);
    instrument_level(base + T77S_GU, 0.30f);
    instrument_level(base + T77S_CB, 0.58f);
    instrument_level(base + T77S_MA, 0.18f);
    instrument_level(base + T77S_ME, 0.20f);
}

static int tr77__vv(int base, int boost) {
    int v = base + (boost < 0 ? boost : 0);            // down-only: no accent on this machine
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire one voice `delay` ms from now. `len_ms` > 0 holds a GATE voice (the guiro) that long;
// 0 = the voice's own length.
static void tr77_fire_len(int base, int v, int boost, int delay, int len_ms) {
    switch (v) {
    case TR77_BD:
        schedule_hit(delay, 36, base + T77S_BD,  tr77__vv(7, boost), 5);
        schedule_hit(delay, 49, base + T77S_BD2, tr77__vv(7, boost), 5);
        break;
    case TR77_SD:
        schedule_hit(delay, 65, base + T77S_SDB, tr77__vv(5, boost), 5);
        schedule_hit(delay, 60, base + T77S_SDN, tr77__vv(6, boost), 5);
        break;
    case TR77_RS:
        schedule_hit(delay, 86, base + T77S_RS,  tr77__vv(7, boost), 2);
        schedule_hit(delay, 33, base + T77S_RSK, tr77__vv(3, boost), 1);
        break;
    case TR77_HH: schedule_hit(delay, 60, base + T77S_HH, tr77__vv(6, boost), 30); break;
    case TR77_CY: schedule_hit(delay, 60, base + T77S_CY, tr77__vv(6, boost), 90); break;
    case TR77_HB: schedule_hit(delay, 65, base + T77S_HB, tr77__vv(7, boost), 1); break;   // settles at 361 Hz
    case TR77_LB: schedule_hit(delay, 57, base + T77S_LB, tr77__vv(7, boost), 1); break;   // settles at 218 Hz
    case TR77_LC: schedule_hit(delay, 49, base + T77S_LC, tr77__vv(7, boost), 1); break;   // settles at 145 Hz
    case TR77_CB:
        schedule_hit(delay, 79, base + T77S_CB, tr77__vv(4, boost), 150);
        schedule_hit(delay, 73, base + T77S_CB, tr77__vv(3, boost), 150);
        break;
    case TR77_CL: schedule_hit(delay, 97, base + T77S_CL, tr77__vv(6, boost), 1); break;
    case TR77_MA: schedule_hit(delay, 90, base + T77S_MA, tr77__vv(4, boost), 30); break;
    case TR77_GU:
        schedule_hit(delay, 60, base + T77S_GU, tr77__vv(5, boost), len_ms > 0 ? len_ms : 520);
        break;
    case TR77_TB:  schedule_hit(delay, 80, base + T77S_TB, tr77__vv(5, boost), 220); break;
    case TR77_TBS: schedule_hit(delay, 80, base + T77S_TB, tr77__vv(3, boost), 220); break;  // via 330K
    case TR77_ME:  schedule_hit(delay, 96, base + T77S_ME, tr77__vv(5, boost), 1); break;
    }
}
static void tr77_fire(int base, int v, int boost, int delay) { tr77_fire_len(base, v, boost, delay, 0); }

// One printed TR-77 lane label -> the voices its trigger line fires (a label like "CB+LB" is
// ONE diode line driving two instruments). Returns how many (0..2); unknown tokens are skipped.
static int tr77__tok(const char *t, int n, int primed) {
    char a = (char)(t[0] | 32), b = n > 1 ? (char)(t[1] | 32) : 0;
    if (n == 2) {
        if (a=='b' && b=='d') return TR77_BD;
        if (a=='s' && b=='d') return TR77_SD;
        if (a=='r' && b=='s') return TR77_RS;
        if (a=='h' && b=='h') return TR77_HH;
        if (a=='c' && b=='y') return TR77_CY;
        if (a=='h' && b=='b') return TR77_HB;
        if (a=='l' && b=='b') return TR77_LB;
        if (a=='l' && b=='c') return TR77_LC;
        if (a=='c' && b=='b') return TR77_CB;
        if (a=='g' && b=='u') return TR77_GU;
        if (a=='t' && b=='b') return primed ? TR77_TBS : TR77_TB;
        if (a=='m' && b=='e') return TR77_ME;
    }
    if (n == 1) {
        if (a=='c') return TR77_CL;
        if (a=='m') return TR77_MA;
    }
    return -1;
}
static int tr77_lane(const char *lab, int out[2]) {
    int k = 0, primed = 0;
    for (const char *p = lab; *p; p++) if (*p == '\'') primed = 1;
    const char *s = lab;
    while (*s && k < 2) {
        int n = 0;
        while (s[n] && s[n] != '+' && s[n] != '\'' && s[n] != ' ') n++;
        if (n) { int v = tr77__tok(s, n, primed); if (v >= 0) out[k++] = v; }
        s += n;
        while (*s == '+' || *s == '\'' || *s == ' ') s++;
    }
    return k;
}

static int tr77_slot(int base, int v) {
    static const int S[TR77_NV] = { T77S_BD, T77S_SDB, T77S_RS, T77S_HH, T77S_CY, T77S_HB,
        T77S_LB, T77S_LC, T77S_CB, T77S_CL, T77S_MA, T77S_GU, T77S_TB, T77S_TB, T77S_ME };
    return base + ((v < 0 || v >= TR77_NV) ? 0 : S[v]);
}

#endif
