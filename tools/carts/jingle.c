#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── JINGLE RADIO ──────────────────────────────────────────────────────────
// The dusk sibling of jangle radio — the delicate side of the same songwriter.
// Where jangle.c loops a dumb happy vamp, jingle composes from the actual
// Mac DeMarco harmonic playbook, as catalogued by Reverb Machine's
// "Mac DeMarco Chord Theory" (reverbmachine.com/blog/mac-demarco-chord-theory):
//
//   • the bVII — his most frequent device, borrowed from the parallel minor;
//     voiced plain major, dominant 7 ("bluesier"), or dominant 9, per song.
//   • the bIII — an out-of-key major7 or minor6 that pulls minor-scale color
//     into a major tune ("Moonlight on the River", "The Stars Keep on Calling").
//   • ii-V cells — the jazz backbone, hiding inside pop progressions.
//   • chromatic bass descents — progressions chosen so the roots walk down by
//     semitone ("A Heart Like Hers": Dm7 | Dbmaj7 | Cm; "Another One":
//     F | Em | Eb | Em). The bass line IS the progression.
//   • extended voicings everywhere — maj7 / m7 / 9ths; even plain triads get
//     a 9 on top here, because this station is gentle.
//   • MELODIC ACCOMMODATION — the article's subtlest rule: over a borrowed
//     chord the melody narrows to that chord's own tones, so it never clashes
//     a semitone with the borrowed note. Over diatonic chords it roams the key.
//
// Each song picks a VERSE progression and a CHORUS progression from templates
// lifted from the cited songs, rolls the bVII/bIII flavors, and plays a real
// little form: intro · verse · verse · chorus · verse · chorus · chorus · outro.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (layers)   UP/DOWN tempo   H or ? help
//
// Seed pins the COMPOSITION (key, both progressions, flavors, picking pattern,
// tempo, title); the PERFORMANCE (finger roll, kit looseness, melody's path)
// re-rolls every playthrough.

#define JINGLE_SEED 0   // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_GTR  5   // fingerpicked, gently chorused
#define I_BASS 6
#define I_MEL  7   // soft singing lead
#define I_KICK 8
#define I_RIM  9
#define I_HAT  10

// ── chords ────────────────────────────────────────────────────────────────
enum { Q_MAJ, Q_MAJ7, Q_DOM7, Q_DOM9, Q_MIN, Q_MIN7, Q_MIN6, NQ };
static const char *QN[NQ] = { "", "maj7", "7", "9", "m", "m7", "m6" };
// rootless 3-voice guitar voicings — everything gets a 9 on top (delicate)
static const int QV[NQ][3] = {
    { 4, 7, 14 },    // maj (add9 color)
    { 4, 11, 14 },   // maj7/9
    { 4, 10, 14 },   // 7/9
    { 4, 10, 14 },   // 9
    { 3, 7, 14 },    // m (add9)
    { 3, 10, 14 },   // m7/9
    { 3, 9, 14 },    // m6/9
};
static const int QT[NQ][4] = {   // chord tones (for the accommodation rule)
    { 0, 4, 7, 7 }, { 0, 4, 7, 11 }, { 0, 4, 7, 10 }, { 0, 4, 10, 14 },
    { 0, 3, 7, 7 }, { 0, 3, 7, 10 }, { 0, 3, 7, 9 },
};
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
static const int MAJSC[7] = { 0, 2, 4, 5, 7, 9, 11 };   // the key, for diatonic melody

// ── progression templates, straight from the article's citations ─────────
// { semitone offset from key, quality, borrowed? }
typedef struct { int off, q, brw; } Ch;
static const Ch TMPL[7][4] = {
    // "Blue Boy"            I | ii | vi | bVII
    { { 0, Q_MAJ, 0 },  { 2, Q_MIN7, 0 }, { 9, Q_MIN7, 0 }, { 10, Q_MAJ, 1 } },
    // "Dreaming"            I | V | bVII | IV
    { { 0, Q_MAJ, 0 },  { 7, Q_MAJ, 0 },  { 10, Q_MAJ, 1 }, { 5, Q_MAJ, 0 } },
    // "Stars Keep Calling"  I | bIIIm6 | ii | V
    { { 0, Q_MAJ7, 0 }, { 3, Q_MIN6, 1 }, { 2, Q_MIN7, 0 }, { 7, Q_DOM7, 0 } },
    // "Moonlight" verse     Imaj7 | vi | ii | V9
    { { 0, Q_MAJ7, 0 }, { 9, Q_MIN7, 0 }, { 2, Q_MIN7, 0 }, { 7, Q_DOM9, 0 } },
    // "Moonlight" chorus    bIIImaj7 | vi | ii | V9
    { { 3, Q_MAJ7, 1 }, { 9, Q_MIN7, 0 }, { 2, Q_MIN7, 0 }, { 7, Q_DOM9, 0 } },
    // "Another One"         IV | iii | bIII | iii   (bass walks F-E-Eb-E)
    { { 5, Q_MAJ, 0 },  { 4, Q_MIN, 0 },  { 3, Q_MAJ, 1 },  { 4, Q_MIN, 0 } },
    // "A Heart Like Hers"   vi | bVImaj7 | v | I7   (bass walks down chromatically)
    { { 9, Q_MIN7, 0 }, { 8, Q_MAJ7, 1 }, { 7, Q_MIN, 1 },  { 0, Q_DOM7, 0 } },
};
static const int VERSE_POOL[4]  = { 0, 1, 3, 5 };   // gentler, more diatonic
static const int CHORUS_POOL[4] = { 2, 4, 5, 6 };   // borrowed-forward

// the form: 8 sections of 8 bars (two passes of a 4-chord progression each)
enum { S_INTRO, S_V, S_C, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_V, S_V, S_C, S_V, S_C, S_C, S_OUTRO };

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    Ch   verse[4], chorus[4];
    int  pick;            // fingerpicking pattern variant
    int  strumChorus;     // 1 = chorus strums gently instead of picking
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song   sng;
static int    tempo     = 82;
static int    intensity = 2;     // 0 gtr+bass · 1 +kit · 2 +melody · 3 fuller
static bool   radioOn   = true;
static bool   showHelp  = false;
static long   scheduled = -1;
static long   songBase  = 0;
static int    songCount = 0;
static double stepMs    = 183.0;
static int    gv[3]     = { 64, 67, 71 };
static bool   gvInit    = false;
static int    kitLag    = 5;
static float  vu        = 0;
static int    melPitch  = 79;
static int    bassLast  = 43;
static int    cellOn[5], cellN = 0;   // melody rhythm cell (2 bars), seeded
static char   nowChord[4][8];

static int iabs(int v) { return v < 0 ? -v : v; }

// composition PRNG (xorshift32) — same contract as the other radios
static unsigned rngState = 1;
static unsigned srnd_u(void) {
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return rngState;
}
static int srnd(int n) { return (int)(srnd_u() % (unsigned)n); }

// ── song generation ───────────────────────────────────────────────────────
static const char *TW[] = { "Paper Moon", "Wallflower", "Last Light", "Nightcap",
    "Slow Dance", "Dust Motes", "Goodnight Boy", "For Her", "Moonbeam",
    "Half Asleep", "Kitchen Light", "Stay a While" };

static void new_song(double pos, unsigned seed) {
    if (!seed) seed = ((unsigned)rnd(0x10000) << 16) ^ (unsigned)rnd(0x10000)
                      ^ (unsigned)frame() * 2654435761u;
    if (!seed) seed = 1;
    rngState = sng.seed = seed;

    sng.keyPc = srnd(12);
    int vi = VERSE_POOL[srnd(4)], ci;
    do { ci = CHORUS_POOL[srnd(4)]; } while (ci == vi);
    for (int k = 0; k < 4; k++) { sng.verse[k] = TMPL[vi][k]; sng.chorus[k] = TMPL[ci][k]; }

    // roll the borrowed-chord flavors, per the article
    int bviiQ = (int[]){ Q_MAJ, Q_DOM7, Q_DOM9 }[srnd(3)];      // bVII: maj / 7 / 9
    int biiiQ = (srnd(10) < 6) ? Q_MAJ7 : Q_MIN6;               // bIII: maj7 / m6
    for (int k = 0; k < 4; k++) {
        if (sng.verse[k].off  == 10) sng.verse[k].q  = bviiQ;
        if (sng.chorus[k].off == 10) sng.chorus[k].q = bviiQ;
        if (sng.verse[k].off  == 3 && sng.verse[k].brw)  sng.verse[k].q  = biiiQ;
        if (sng.chorus[k].off == 3 && sng.chorus[k].brw) sng.chorus[k].q = biiiQ;
    }

    sng.pick        = srnd(2);
    sng.strumChorus = srnd(10) < 5;

    // melody cell: 3..5 soft onsets across 2 bars, leaning on the 8ths
    cellN = 0;
    for (int s = 0; s < 31 && cellN < 5; s += 2)
        if (srnd(100) < (s % 8 == 0 ? 25 : 35)) cellOn[cellN++] = s;
    if (cellN < 3) { cellN = 3; cellOn[0] = 2; cellOn[1] = 12; cellOn[2] = 20; }

    if (srnd(100) < 30)
        snprintf(sng.title, sizeof sng.title, "%04d%02d%02d",
                 2017 + srnd(5), 1 + srnd(12), 1 + srnd(28));
    else
        snprintf(sng.title, sizeof sng.title, "%s", TW[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 72 + srnd(21);                              // 72..92, gentle
    bpm(tempo);
    songBase = (long)pos + 8;
    gvInit   = false;
    melPitch = 79;
    bassLast = 43;
    kitLag   = rnd_between(2, 9);                       // performance
    songCount++;
}

// session history — [ and ] walk back through everything the radio played
static unsigned hist[64];
static int histN = 0, histPos = -1;

static void fresh_song(double pos) {
    new_song(pos, 0);
    if (histN == 64) { for (int i = 1; i < 64; i++) hist[i - 1] = hist[i]; histN--; }
    hist[histN++] = sng.seed;
    histPos = histN - 1;
}

// ── form / harmony lookups ────────────────────────────────────────────────
static int sect_of(long bar)  { long s = bar / 8; return (int)(s < 8 ? FORM[s] : S_OUTRO); }
static const Ch *prog_of(long bar) {
    return sect_of(bar) == S_C ? sng.chorus : sng.verse;   // intro/outro pick verse
}
static Ch chord_at(long bar)  { return prog_of(bar)[bar % 4]; }
static int root_pc(Ch c)      { return (sng.keyPc + c.off) % 12; }

static void chord_label(char *out, int n, Ch c) {
    snprintf(out, n, "%s%s", PCNAME[root_pc(c)], QN[c.q]);
}

// the bass is voice-led too: each root lands on the octave copy nearest the
// last one, clamped to D2..D3 — chromatic descents survive any key
static int bass_peek(int pc) {
    int d = ((pc - bassLast) % 12 + 18) % 12 - 6;
    int n = bassLast + d;
    while (n < 36) n += 12;
    while (n > 50) n -= 12;
    return n;
}
static int bass_near(int pc) { return bassLast = bass_peek(pc); }

// nearest-tone voice leading — the block that has now shipped in four carts
static void lead_voices(Ch c) {
    int pcs[3];
    for (int k = 0; k < 3; k++) pcs[k] = (root_pc(c) + QV[c.q][k]) % 12;
    if (!gvInit) {
        for (int k = 0; k < 3; k++) {
            int target = 62 + k * 5;
            int dd = ((pcs[k] - target) % 12 + 18) % 12 - 6;
            gv[k] = target + dd;
        }
        gvInit = true;
    } else {
        bool used[3] = { false, false, false };
        for (int v = 0; v < 3; v++) {
            int bestJ = -1, bestC = 0, bestD = 99;
            for (int j = 0; j < 3; j++) {
                if (used[j]) continue;
                int dd = ((pcs[j] - gv[v]) % 12 + 18) % 12 - 6;
                if (iabs(dd) < bestD) { bestD = iabs(dd); bestJ = j; bestC = gv[v] + dd; }
            }
            used[bestJ] = true;
            gv[v] = bestC;
        }
    }
    for (int k = 0; k < 3; k++) {
        while (gv[k] < 57) gv[k] += 12;
        while (gv[k] > 81) gv[k] -= 12;
    }
}

// MELODIC ACCOMMODATION — over a borrowed chord, only its own tones (+9);
// over diatonic chords, the whole key. The article's rule, verbatim in code.
static int pick_mel(Ch c) {
    int bestM = melPitch, bestScore = -999;
    if (c.brw) {
        for (int t = 0; t < 4; t++) {
            int pc = (root_pc(c) + QT[c.q][t]) % 12;
            for (int oct = 6; oct <= 7; oct++) {
                int m = oct * 12 + pc;
                if (m < 72 || m > 89) continue;
                int score = 10 - iabs(m - melPitch) + rnd(4);
                if (m == melPitch) score -= 3;
                if (score > bestScore) { bestScore = score; bestM = m; }
            }
        }
    } else {
        for (int d = 0; d < 7; d++) {
            int pc = (sng.keyPc + MAJSC[d]) % 12;
            for (int oct = 6; oct <= 7; oct++) {
                int m = oct * 12 + pc;
                if (m < 72 || m > 89) continue;
                int score = 10 - iabs(m - melPitch) + rnd(4);
                int rel = (pc - root_pc(c) + 12) % 12;
                if (rel == 0 || rel == 3 || rel == 4 || rel == 7) score += 3;  // chord tones pull
                if (m == melPitch) score -= 3;
                if (score > bestScore) { bestScore = score; bestM = m; }
            }
        }
    }
    melPitch = bestM;
    return bestM;
}

// ── layers per section, gated by the feel knob ────────────────────────────
static bool kit_on(long bar) {
    int s = sect_of(bar);
    long sect = bar / 8;
    return intensity >= 1 && (s == S_C || (s == S_V && sect >= 4));   // kit joins after first chorus
}
static bool mel_on(long bar) {
    int s = sect_of(bar);
    return intensity >= 2 && (s == S_C || sect_of(bar + 8) == S_OUTRO || (intensity >= 3 && s == S_V));
}

// ── the step player ───────────────────────────────────────────────────────
static const int PICKV[2][8] = {     // fingerpicking voice order, on the 8ths
    { 0, 2, 1, 2, 0, 2, 1, 2 },
    { 0, 1, 2, 1, 0, 2, 1, 0 },
};

static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = (int)((abs - pos) * stepMs);
    if (dly < 1) dly = 1;
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;                              // song over; update() rolls the next
    Ch   c    = chord_at(bar);
    int  sect = sect_of(bar);

    // tape breath, gentler than jangle's
    if (step == 0 && bar % 8 == 0 && bar > 0) bpm(tempo + rnd(3) - 1);

    // BASS — voice-led roots (nearest octave copy), so the chromatic templates
    // actually walk down by semitone in every key — the line IS the progression.
    if (step == 0 || step == 8 || step == 14) {
        int b = bass_near(root_pc(c));
        int n = b, vol = 4;
        bool play = true;
        if (step == 8) { vol = 3; if (chance(40)) n = (b + 7 <= 50) ? b + 7 : b - 5; }
        if (step == 14) {                               // chromatic approach to the next root
            int nb = bass_peek(root_pc(chord_at(bar + 1)));
            play = nb != b && iabs(nb - b) <= 2 && chance(70);
            n = nb > b ? nb - 1 : nb + 1;
            vol = 2;
        }
        if (play) {
            schedule_hit(dly, n, I_BASS, vol, (int)(stepMs * 5.5));
            vu += vol * 0.6f;
        }
    }

    // GUITAR — fingerpicked 8ths (delicate), or gentle strums in the chorus
    bool strum = sng.strumChorus && sect == S_C;
    if (strum) {
        if (step == 0 || step == 8 || (step == 6 && chance(40))) {
            lead_voices(c);
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + k * 14 + rnd(6), gv[k], I_GTR, 3, (int)(stepMs * 4));
            vu += 2.2f;
        }
    } else if (step % 2 == 0) {
        if (chance(10)) return;                         // delicate gaps
        lead_voices(c);
        int v = gv[PICKV[sng.pick][(step / 2) % 8]];
        schedule_hit(dly + rnd(7), v, I_GTR, (step == 0) ? 3 : 2, (int)(stepMs * 2.4));
        vu += 1.0f;
    }

    // KIT — soft: kick, cross-stick rim, quiet hats; loose behind the bass
    if (kit_on(bar) && sect != S_INTRO && sect != S_OUTRO) {
        int lag = dly + kitLag + rnd(4);
        if (step == 0 || (step == 8 && chance(70)))
            { schedule_hit(lag, 40, I_KICK, 4, 100); vu += 1.5f; }
        if (step == 4 || step == 12)
            { schedule_hit(lag, 72, I_RIM, 3, 35); vu += 1.5f; }
        if (step % 2 == 0)
            schedule_hit(lag, 88, I_HAT, (step % 8 == 4) ? 2 : 1, 22);
    }

    // MELODY — the seeded cell, re-pitched with the accommodation rule
    if (mel_on(bar)) {
        int s32 = (int)(s % 32);
        for (int i = 0; i < cellN; i++)
            if (cellOn[i] == s32 && chance(85)) {
                int gap = (i + 1 < cellN) ? cellOn[i + 1] - s32 : 32 - s32;
                int dur = (int)(gap * stepMs * 0.85);
                if (dur > 1100) dur = 1100;
                schedule_hit(dly + 12 + rnd(10), pick_mel(c), I_MEL, 3, dur);
                vu += 1.5f;
            }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_GTR, INSTR_TRI, 1, 420, 2, 260);            // soft, ringing
    instrument_filter(I_GTR, FILTER_LOW, 2000, 2);
    instrument_lfo(I_GTR, 0, LFO_PITCH, 4.6f, 0.08f);        // gentler warble
    instrument_lfo(I_GTR, 1, LFO_VOLUME, 3.9f, 0.06f);

    instrument(I_BASS, INSTR_SINE, 3, 280, 4, 110);
    instrument_filter(I_BASS, FILTER_LOW, 520, 1);

    instrument(I_MEL, INSTR_SINE, 14, 200, 4, 240);          // soft singing lead
    instrument_lfo(I_MEL, 0, LFO_PITCH, 5.1f, 0.12f);
    instrument_filter(I_MEL, FILTER_LOW, 2300, 2);

    instrument(I_KICK, INSTR_SINE, 1, 85, 0, 40);
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 40, 12);

    instrument(I_RIM, INSTR_NOISE, 0, 26, 0, 18);            // cross-stick
    instrument_filter(I_RIM, FILTER_BAND, 1500, 8);
    instrument_env(I_RIM, 0, ENV_PITCH, 0, 18, 16);

    instrument(I_HAT, INSTR_NOISE, 0, 20, 0, 14);
    instrument_filter(I_HAT, FILTER_HIGH, 7000, 3);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (JINGLE_SEED) { new_song(pos, JINGLE_SEED); hist[histN++] = sng.seed; histPos = 0; }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    if (keyp(KEY_SPACE)) fresh_song(pos);
    if (keyp('R')) new_song(pos, sng.seed);
    if (keyp('[') && histPos > 0)         new_song(pos, hist[--histPos]);
    if (keyp(']') && histPos < histN - 1) new_song(pos, hist[++histPos]);
    if (keyp(KEY_RIGHT) && intensity < 3) intensity++;
    if (keyp(KEY_LEFT)  && intensity > 0) intensity--;
    if (keyp(KEY_UP)   && tempo < 100) { tempo += 2; bpm(tempo); }
    if (keyp(KEY_DOWN) && tempo > 64)  { tempo -= 2; bpm(tempo); }
    if (keyp('M')) {
        radioOn = !radioOn;
        if (!radioOn) note_off_all();
        else scheduled = (long)pos;
    }
    if (keyp('H')) showHelp = !showHelp;
    if (mouse_pressed(MOUSE_LEFT)) {
        int hx = mouse_x() - 288, hy = mouse_y() - 172;
        if (hx * hx + hy * hy < 81) showHelp = !showHelp;
    }

    if (radioOn) {
        long target = (long)pos + 1;
        while (scheduled < target) { scheduled++; play_step(scheduled, pos); }

        long songStep = scheduled - songBase;
        if (songStep >= 64L * 16) fresh_song(pos);      // 8 sections x 8 bars

        long bar = songStep >= 0 ? songStep / 16 : 0;
        const Ch *p = prog_of(bar);
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 8, p[i]);
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long bar = ss >= 0 ? ss / 16 : 0;
    static const char *SN[4] = { "intro", "verse", "chorus", "outro" };
    watch("song", "%d", songCount);
    watch("sect", "%s", SN[sect_of(bar)]);
    watch("chord", "%s", nowChord[(int)(bar % 4)]);
#endif
}

// ── draw — the radio at dusk ──────────────────────────────────────────────
static void knob(int x, int y, int r, float t, const char *label, int col) {
    circfill(x, y, r, CLR_DARKER_PURPLE);
    circ(x, y, r, CLR_BLACK);
    float a = (-0.75f + t * 1.5f) * 3.14159f;
    line(x, y, x + (int)(sinf(a) * (r - 2)), y - (int)(cosf(a) * (r - 2)), col);
    print(label, x - text_width(label) / 2, y + r + 3, CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);                               // early night outside
    float t = timer();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;

    for (int k = 0; k < 24; k++) {                      // first stars
        int sx = (k * 67 + 11) % 320, sy = (k * 41 + 5) % 48;
        if ((k + frame() / 50) % 7) pset(sx, sy, CLR_DARK_GREY);
    }

    // body — mauve evening plastic
    rectfill(20, 16, 280, 168, CLR_DARKER_PURPLE);
    rectfill(24, 20, 272, 160, CLR_MAUVE);

    // FM dial strip
    rectfill(32, 26, 256, 18, CLR_LIGHT_PEACH);
    rect(32, 26, 256, 18, CLR_DARKER_PURPLE);
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * 13;
        line(x, 38, x, 42, CLR_DARK_PURPLE);
        if (fq % 4 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 6, 29, CLR_DARK_PURPLE);
        }
    }
    int nx = 36 + (int)((sng.freq - 88.0f) * 13.0f);
    line(nx, 27, nx, 43, CLR_RED);

    // the window — sunset fading, string lights that follow the chords
    rectfill(34, 52, 102, 116, CLR_DARK_PURPLE);        // dusk sky
    rectfill(34, 96, 102, 28, CLR_DARK_RED);            // sunset band
    rectfill(34, 116, 102, 16, CLR_DARK_ORANGE);
    circfill(85, 124, 9, CLR_ORANGE);                   // sun, half gone
    rectfill(34, 132, 102, 36, CLR_DARKER_BLUE);        // the dark yard
    rect(34, 52, 102, 116, CLR_DARKER_PURPLE);
    // sagging string of lights; the lit bulb walks with the chord
    int litB = (int)(bar % 4);
    for (int i = 0; i < 7; i++) {
        int lx = 42 + i * 14;
        int ly = 140 + (int)(6.0f * sinf((i - 3) * 0.5f));
        if (i < 6) {
            int nx2 = 42 + (i + 1) * 14, ny2 = 140 + (int)(6.0f * sinf((i - 2) * 0.5f));
            line(lx, ly, nx2, ny2, CLR_DARK_GREY);
        }
        bool lit = (i % 4 == litB) || ((i + 2) % 4 == litB);
        circfill(lx, ly + 2, lit ? 2 : 1, lit ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    }

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_DARK_GREY);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_PINK);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  key %s", sng.freq, PCNAME[sng.keyPc]);
        print(l2, 154, 70, CLR_DARK_PEACH);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_DARK_PEACH);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // the active progression, current chord boxed
    if (radioOn) {
        int ci = (int)(bar % 4);
        int x = 152;
        for (int i = 0; i < 4; i++) {
            int cw = text_width(nowChord[i]);
            if (x + cw > 292) break;
            if (i == ci) {
                rectfill(x - 2, 102, cw + 4, 12, CLR_PINK);
                print(nowChord[i], x, 104, CLR_DARKER_PURPLE);
            } else
                print(nowChord[i], x, 104, CLR_LIGHT_GREY);
            x += cw + 8;
        }
        static const char *SN[4] = { "intro", "verse", "chorus", "outro" };
        print(SN[sect_of(bar)], 152, 120, CLR_PINK);
        for (int i = 0; i < 8; i++)                     // form dots
            circfill(208 + i * 8, 124, 1, i <= bar / 8 ? CLR_PINK : CLR_DARK_GREY);
    }

    // knobs + power LED
    static const char *FEEL[4] = { "hush", "dusky", "tender", "aglow" };
    knob(168, 148, 9, intensity / 3.0f, FEEL[intensity], CLR_PINK);
    knob(218, 148, 9, (tempo - 64) / 36.0f, "tempo", CLR_PINK);
    float vt = vu / 12.0f;
    knob(262, 148, 11, vt > 1 ? 1 : vt, "wow", CLR_RED);
    circfill(282, 28, 2, radioOn && beat_pos() < 0.25f ? CLR_RED : CLR_DARK_RED);

    // help button + hint
    circfill(288, 172, 6, CLR_DARKER_PURPLE);
    circ(288, 172, 6, CLR_BLACK);
    print("?", 285, 169, CLR_PINK);
    print("SPACE next song   H help", 8, 190, CLR_DARK_GREY);

    if (showHelp) {
        rectfill(44, 40, 232, 122, CLR_BLACK);
        rect(44, 40, 232, 122, CLR_PINK);
        print("JINGLE RADIO", 52, 46, CLR_PINK);
        font(FONT_SMALL);
        static const char *HELP[7][2] = {
            { "SPACE",      "next song (rolls a new seed)" },
            { "R",          "same song again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - how many layers play" },
            { "UP/DOWN",    "tempo of this tune" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
        };
        for (int i = 0; i < 7; i++) {
            print(HELP[i][0], 52, 60 + i * 9, CLR_YELLOW);
            print(HELP[i][1], 106, 60 + i * 9, CLR_WHITE);
        }
        print("the #number on the display IS the song.", 52, 128, CLR_PINK);
        print("pin it for good: #define JINGLE_SEED 0x...", 52, 137, CLR_PINK);
        print("seeded composition, played fresh every time", 52, 146, CLR_PINK);
        font(FONT_NORMAL);
    }
}
