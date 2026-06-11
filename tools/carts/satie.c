#include "studio.h"
#include "radio.h"   // shared station chassis (PRNG, clock, voice-leading, chrome)
#include <stdio.h>
#include <math.h>

// ── SATIE RADIO ───────────────────────────────────────────────────────────
// The ninth station, and the family's patron saint: Erik Satie invented
// "musique d'ameublement" — FURNITURE MUSIC — in 1917. Music to be in a room
// with. Every radio on this dial descends from that idea; this one returns
// to the source. Endless gymnopédies and gnossiennes for solo piano.
//
// Three firsts for the engine:
//   • 3/4 TIME — twelve 16th-steps to the bar, not sixteen. The whole radio
//     family counted in four until now.
//   • ONE INSTRUMENT — solo piano, two hands. No kit, no bass synth, no
//     layers. The density curve works on touch and ornament instead.
//   • the ALTERNATING-PAIR chord brain (#6) — Satie neither progresses
//     (bossa) nor vamps (jangle) nor drifts by walk (ambient): he ROCKS
//     between two chords for bars on end (Gymnopédie No.1 is Gmaj7-Dmaj7,
//     alternating, for most of the piece) and lets the PAIR itself drift
//     between sections. Harmony as a slow pendulum.
//
// The two moods, rolled per piece:
//   GYMNOPEDIE — modal major, maj7 pairs a whole step apart, the suspended
//     lilt: bass note on 1, chord on 2, beat 3 empty. Lent et douloureux.
//   GNOSSIENNE — minor, the phrygian-dominant color (that augmented second),
//     mordent ornaments before the melody notes, chord restruck on 3,
//     rubato breathing in the tempo.
//
// The MELODY enters late (the accompaniment furnishes the room first),
// moves stepwise, holds long notes over the barline, and rests between
// phrases. Composition is seeded; the rubato and ornaments are performance.
//
//   SPACE next piece   R play it again   [ ] history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help

#define SATIE_SEED 0   // pin a favourite piece here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_PNO  5   // the piano — melody + chords
#define I_PNOB 6   // its left hand — longer, rounder

#define BARSTEPS 12   // 3/4: three beats of four 16ths

// ── harmony ───────────────────────────────────────────────────────────────
enum { Q_MAJ7, Q_MIN7, Q_MIN, NQ };
static const char *QN[NQ] = { "maj7", "m7", "m" };
static const int QV[NQ][3] = {
    { 4, 7, 11 },    // maj7 — the Gymnopédie chord
    { 3, 7, 10 },    // m7
    { 3, 7, 12 },    // m
};
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
// melody scales
static const int SC_GYM[7] = { 0, 2, 4, 5, 7, 9, 11 };   // major, lydian-tinged by the pair
static const int SC_GNO[7] = { 0, 1, 4, 5, 7, 8, 10 };   // phrygian dominant — the augmented 2nd

typedef struct { int pc, q; } Pc;

// the form: each section owns a PAIR; A-sections rock the home pair,
// B-sections a drifted one. intro/outro are accompaniment alone.
enum { S_INTRO, S_A, S_B, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_A, S_A, S_B, S_A, S_B, S_A, S_OUTRO };

// ── the generated piece ───────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    int  gnossienne;          // 0 = gymnopédie, 1 = gnossienne
    Pc   pairA[2], pairB[2];  // the two pendulums
    int  restrike;            // chord again on beat 3 (gnossienne lilt)
    int  cellOn[6], cellN;    // melody onsets, in BEATS across a 4-bar phrase (0..11)
    int  marking;             // the tempo direction on the display
    char title[24];
    float freq;
    unsigned seed;
} Song;

static const char *MARKING[6] = {
    "lent et grave", "lent, douloureux", "lent et triste",
    "tres luisant", "avec etonnement", "sans sourire",
};

static Song       sng;
static RadioSeed  rs;                       // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 238.0 };   // schedule-ahead step clock (radio.h)
// the clock's fields under their pre-migration names — keeps the body unchanged
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)   rad_srnd(&rs, (n))         // SAME xorshift stream — pinned seeds depend on it
static int    tempo     = 63;
static int    intensity = 1;     // feel: shifts the density curve (touch + ornament here)
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    songCount = 0;
static int    gv[3]     = { 64, 67, 71 };
static bool   gvInit    = false;
static float  vu        = 0;
static int    melWalk   = 76;    // the singing hand
static char   nowChord[2][8];

static int iabs(int v) { return v < 0 ? -v : v; }

// ── piece generation ──────────────────────────────────────────────────────
static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);   // 0 = derive fresh (same expression as ever)

    sng.keyPc      = srnd(12);
    sng.gnossienne = srnd(100) < 40;

    if (!sng.gnossienne) {
        // gymnopédie: two maj7 chords a whole step apart (the No.1 pendulum);
        // the B pair drifts — relative minor center, or the pendulum slides
        sng.pairA[0] = (Pc){ sng.keyPc, Q_MAJ7 };
        sng.pairA[1] = (Pc){ (sng.keyPc + (srnd(2) ? 10 : 2)) % 12, Q_MAJ7 };
        if (srnd(2)) {
            sng.pairB[0] = (Pc){ (sng.keyPc + 9) % 12, Q_MIN7 };       // vi, the shadow
            sng.pairB[1] = (Pc){ (sng.keyPc + 2) % 12, Q_MIN7 };
        } else {
            sng.pairB[0] = (Pc){ (sng.keyPc + 5) % 12, Q_MAJ7 };       // the slide to IV
            sng.pairB[1] = (Pc){ sng.keyPc, Q_MAJ7 };
        }
        sng.restrike = 0;
    } else {
        // gnossienne: minor pendulum with the phrygian neighbour
        sng.pairA[0] = (Pc){ sng.keyPc, srnd(2) ? Q_MIN : Q_MIN7 };
        int nb = (int[]){ 10, 8, 1 }[srnd(3)];                          // bVII, bVI, bII
        sng.pairA[1] = (Pc){ (sng.keyPc + nb) % 12, nb == 10 ? Q_MIN : Q_MAJ7 };
        sng.pairB[0] = (Pc){ (sng.keyPc + 5) % 12, Q_MIN };             // iv, deeper in
        sng.pairB[1] = (Pc){ (sng.keyPc + 8) % 12, Q_MAJ7 };
        sng.restrike = 1;
    }

    // the melody's phrase mask: onsets in BEATS over a 4-bar phrase (12 beats),
    // entering late, holding long — first onset rarely before beat 3
    sng.cellN = 0;
    for (int b = 2; b < 11 && sng.cellN < 6; b++)
        if (srnd(100) < 40) sng.cellOn[sng.cellN++] = b;
    if (sng.cellN < 3) { sng.cellN = 3; sng.cellOn[0] = 3; sng.cellOn[1] = 6; sng.cellOn[2] = 8; }

    sng.marking = srnd(6);
    if (!sng.gnossienne)
        snprintf(sng.title, sizeof sng.title, "Gymnopedie No.%d", 4 + srnd(93));
    else
        snprintf(sng.title, sizeof sng.title, "Gnossienne No.%d", 8 + srnd(89));
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 56 + srnd(15);                               // 56..70 — lent
    bpm(tempo);
    songBase = (long)pos + 6;                            // half a 3/4 bar of air
    gvInit   = false;
    melWalk  = 76;
    songCount++;
}

static void fresh_song(double pos) {       // [ and ] walk the session history (radio.h)
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ────────────────────────────────────────────────────────
static int sect_of(long bar)  { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }
static Pc  chord_at(long bar) {
    const Pc *pair = (sect_of(bar) == S_B) ? sng.pairB : sng.pairA;
    if (bar % 8 == 7) return pair[0];                    // sections breathe out on P
    return pair[bar % 2];                                // the pendulum
}

static void chord_label(char *out, int n, Pc c) {
    snprintf(out, n, "%s%s", PCNAME[c.pc], QN[c.q]);
}

// density curve — with one instrument it shapes TOUCH, not layers
//   0: accompaniment alone (furniture music, as the man intended)
//   1: + the melody              3: + rolled chords, the octave shimmer
//   2: + ornaments, fuller touch
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_A ? 1 : 2);
    int lvl = base + intensity - 1;
    return lvl < 0 ? 0 : lvl > 3 ? 3 : lvl;
}

// ── the tone knob (T cycles) — the piano's felt ───────────────────────────
static int toneSel = 1;                                  // satie opens warm
static const char *TONENAME[4] = { "mellow", "warm", "clear", "bright" };
static const float TONEMUL[4]  = { 0.55f, 0.78f, 1.0f, 1.28f };

static void apply_voicing(void) {
    float m = TONEMUL[toneSel];
    instrument_filter(I_PNO,  FILTER_LOW, (int)(2400 * m), 1);
    instrument_filter(I_PNOB, FILTER_LOW, (int)(1300 * m), 1);
}

// nearest-tone voice leading — rad_lead_to (radio.h) is the shared block; the
// pianist's right hand lives in the 55..76 register.
static void lead_voices(Pc c) {
    rad_lead_to(c.pc, QV[c.q], gv, 3, 55, 76, &gvInit);
}

// the singing hand: stepwise above all, in the piece's scale
static int pick_mel(Pc c) {
    const int *sc = sng.gnossienne ? SC_GNO : SC_GYM;
    int bestM = melWalk, bestScore = -999;
    for (int d = 0; d < 7; d++) {
        int pc = (sng.keyPc + sc[d]) % 12;
        for (int oct = 5; oct <= 7; oct++) {
            int m = oct * 12 + pc;
            if (m < 69 || m > 88) continue;
            int dist = iabs(m - melWalk);
            int score = 14 - dist * 2 + rnd(3);          // stepwise, strongly
            int rel = (pc - c.pc + 12) % 12;
            if (rel == 0 || rel == 3 || rel == 4 || rel == 7) score += 2;
            if (m == melWalk) score -= 5;
            if (score > bestScore) { bestScore = score; bestM = m; }
        }
    }
    melWalk = bestM;
    return bestM;
}

// ── the step player — twelve steps to the bar ─────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % BARSTEPS);
    long bar  = s / BARSTEPS;
    if (bar >= 64) return;
    Pc   c    = chord_at(bar);
    int  lvl  = level_of(bar);

    // rubato — the gnossienne breathes; the gymnopédie barely sways
    if (step == 0 && bar % 4 == 0 && bar > 0)
        bpm(tempo + rnd(sng.gnossienne ? 5 : 3) - (sng.gnossienne ? 2 : 1));

    // LEFT HAND — bass on ONE (the bar's root, deep and pedaled)...
    if (step == 0) {
        int b = 36 + c.pc;
        while (b > 48) b -= 12;
        schedule_hit(dly, b, I_PNOB, 4, (int)(stepMs * BARSTEPS * 0.95));
        vu += 2;
    }
    // ...chord on TWO (and on three, if this piece restrikes)
    if (step == 4 || (sng.restrike && step == 8)) {
        lead_voices(c);
        int dur = (int)(stepMs * (sng.restrike ? 3.6 : 7.2));
        int roll = (lvl >= 3) ? 15 : 0;                  // rolled, when ardent
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * roll + rnd(4), gv[k], I_PNO,
                         step == 4 ? 3 : 2, dur);
        vu += 1.5f;
    }

    // THE MELODY — beats only; enters late; holds long notes over the bar
    if (lvl >= 1 && step % 4 == 0) {
        int phraseBeat = (int)((bar % 4) * 3 + step / 4);   // 0..11 in the phrase
        for (int i = 0; i < sng.cellN; i++)
            if (sng.cellOn[i] == phraseBeat && chance(85)) {
                int gapBeats = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - phraseBeat
                                                   : 12 - phraseBeat + 2;
                int dur = (int)(gapBeats * stepMs * 4 * 0.9);
                if (dur > 2600) dur = 2600;
                int n = pick_mel(c);
                int mdly = dly + 10 + rnd(8);            // sings a breath behind
                // the mordent: a quick upper neighbour, gnossienne's signature
                if (lvl >= 2 && chance(sng.gnossienne ? 35 : 12)) {
                    schedule_hit(mdly, n + (sng.gnossienne ? 1 : 2), I_PNO, 2, 70);
                    mdly += 80;
                }
                schedule_hit(mdly, n, I_PNO, lvl >= 2 ? 4 : 3, dur);
                if (lvl >= 3)                            // the octave shimmer
                    schedule_hit(mdly + rnd(4), n + 12 <= 91 ? n + 12 : n, I_PNO, 2, dur);
                vu += 2;
            }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_PNO, INSTR_TRI, 1, 700, 1, 500);            // the right hand
    instrument_env(I_PNO, 0, ENV_CUTOFF, 0, 200, 600);       // hammer softness

    instrument(I_PNOB, INSTR_TRI, 2, 1100, 1, 700);          // the left, rounder
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        apply_voicing();
        if (SATIE_SEED) { new_song(pos, SATIE_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    // the shared input block (radio.h): feel/tempo/tone/help handled inside, the
    // cart reacts to the events. tone has 4 felts; re-aim the piano filters on T.
    int ev = rad_input(&tempo, 48, 80, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_TONE)   apply_voicing();
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) note_off_all();
        else scheduled = (long)pos;
    }

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= 64L * BARSTEPS) fresh_song(pos);

        long bar = songStep >= 0 ? songStep / BARSTEPS : 0;
        chord_label(nowChord[0], 8, chord_at(bar));
        chord_label(nowChord[1], 8, chord_at(bar + 1));
    }

    vu *= 0.9f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long bar = ss >= 0 ? ss / BARSTEPS : 0;
    static const char *SN[4] = { "intro", "A", "B", "outro" };
    watch("song", "%d", songCount);
    watch("mode", "%s", sng.gnossienne ? "gnossienne" : "gymnopedie");
    watch("sect", "%s", SN[sect_of(bar)]);
    watch("chord", "%s", nowChord[0]);
#endif
}

// ── draw — the parlour radio, rain on the window (shared chrome knobs/help
// from radio.h; the porcelain body, white FM dial and rainy-Paris window stay
// satie's own — a light radio doesn't fit the dark chassis face) ───────────
void draw(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();                                         // knobs are touch-draggable
    float t = timer();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / BARSTEPS : 0;

    // body — porcelain and brass
    rectfill(20, 16, 280, 168, CLR_MEDIUM_GREY);
    rectfill(24, 20, 272, 160, CLR_LIGHT_GREY);
    line(24, 22, 295, 22, CLR_YELLOW);                  // a thin brass line

    // dial strip
    rectfill(32, 26, 218, 18, CLR_WHITE);            // short of the tune button
    rect(32, 26, 218, 18, CLR_MEDIUM_GREY);
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * RAD_DIAL_SP;
        line(x, 38, x, 42, CLR_MEDIUM_GREY);
        if (fq % 4 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 6, 29, CLR_DARK_GREY);
        }
    }
    int nx = 36 + (int)((rad_needle_freq(sng.freq) - 88.0f) * RAD_DIAL_SP);
    line(nx, 27, nx, 43, CLR_DARK_RED);
    rad_tuner_button(CLR_DARK_RED);

    // the window — rain over Paris at dusk
    rectfill(34, 52, 102, 116, CLR_DARK_BLUE);
    // the tower, far off
    line(85, 90, 78, 150, CLR_BLACK);
    line(85, 90, 92, 150, CLR_BLACK);
    line(80, 122, 90, 122, CLR_BLACK);
    line(78, 138, 92, 138, CLR_BLACK);
    line(85, 82, 85, 90, CLR_BLACK);
    // rooftops
    rectfill(34, 144, 30, 24, CLR_BLACK);
    rectfill(60, 152, 24, 16, CLR_BROWNISH_BLACK);
    rectfill(100, 148, 36, 20, CLR_BLACK);
    pset(46, 150, CLR_YELLOW);                          // a lit garret
    pset(112, 154, CLR_YELLOW);
    // a gas lamp
    line(122, 168, 122, 134, CLR_BLACK);
    circfill(122, 132, 3, CLR_LIGHT_YELLOW);
    // the rain, falling slow
    for (int k = 0; k < 26; k++) {
        int rx = 36 + (k * 53 + (int)(t * 6) * (k % 3 + 1)) % 98;
        int ry = 54 + (int)(fmodf(t * (34 + (k % 5) * 9) + k * 31, 110.0f));
        if (ry < 164) line(rx, ry, rx - 1, ry + 4, CLR_INDIGO);
    }
    rect(34, 52, 102, 116, CLR_MEDIUM_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_MEDIUM_GREY);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_LIGHT_YELLOW);
        char l2[32];
        snprintf(l2, 32, "%s", MARKING[sng.marking]);
        print(l2, 154, 70, CLR_MEDIUM_GREY);
        snprintf(l2, 32, "3/4  %d  #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_MEDIUM_GREY);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_LIGHT_YELLOW);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // the pendulum: this bar's chord boxed, next swinging in
    if (radioOn) {
        int cw = text_width(nowChord[0]);
        rectfill(150, 102, cw + 6, 13, CLR_LIGHT_YELLOW);
        print(nowChord[0], 153, 105, CLR_DARKER_GREY);
        print("~", 164 + cw, 106, CLR_MEDIUM_GREY);
        print(nowChord[1], 178 + cw, 106, CLR_MEDIUM_GREY);
        static const char *SN[4] = { "intro", "", "(drift)", "outro" };
        print(sng.gnossienne ? "gnossienne" : "gymnopedie", 152, 120, CLR_DARK_GREY);
        print(SN[sect_of(bar)], 234, 120, CLR_DARK_GREY);
        for (int i = 0; i < 8; i++)
            circfill(152 + i * 7, 132, 1, i <= bar / 8 ? CLR_DARK_GREY : CLR_LIGHT_GREY);
    }

    // knobs + power LED (dark-red on porcelain; the LED still beats the bar)
    static const char *FEEL[4] = { "meuble", "reverie", "salon", "ardent" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_DARK_RED);
    if (rad_knob_int(&tempo, 48, 80, 2, 218, 148, 9, "tempo", CLR_DARK_RED)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, TONENAME[toneSel], CLR_DARK_RED)) apply_voicing();
    rad_power_led(radioOn, CLR_DARK_RED, CLR_MEDIUM_GREY);

    rad_help_button(CLR_DARK_RED);
    rad_footer("H help");

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next piece (rolls a new seed)" },
            { "R",          "same piece again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - touch and ornament" },
            { "UP/DOWN",    "tempo (it stays lent)" },
            { "T",          "tone - the piano's felt" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
        };
        static const char *NOTES[3] = {
            "the #number on the display IS the piece.",
            "pin it for good: #define SATIE_SEED 0x...",
            "furniture music, as the man intended. 1917.",
        };
        rad_help_panel("SATIE RADIO", HELP, 8, NOTES, 3, CLR_LIGHT_YELLOW);
    }

    ui_end();
}
