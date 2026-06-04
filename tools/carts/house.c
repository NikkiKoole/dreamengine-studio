#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── HOUSE RADIO ───────────────────────────────────────────────────────────
// The tenth station: endless French house / filter disco — the Daft Punk /
// Stardust / Cassius homage. The genre that made one trick into a movement:
// take a disco loop, put a resonant lowpass on it, and RIDE THE FILTER for
// six minutes. Here that trick is finally native machinery:
//
//   • THE FILTER RIDE IS THE SONG — note_cutoff()/note_res() swept live on
//     held string-machine voices, plus the stab/bass filters re-aimed every
//     step from the same arrangement curve. Each 8-bar phrase has a cutoff
//     arc (intro closed → build sweeps up with resonance climbing → drop
//     wide open → breakdown half-lit). Harmony never develops; the FILTER
//     is the form. This is the parking-lot prediction from game-music.md:
//     "the filter ride IS the song" — note_cutoff was made for this.
//   • the SIDECHAIN PUMP — French house breathes against the kick. The
//     strings' cutoff ducks at every kick and blooms across the beat
//     (beat_pos() drives it): pumping compression as a filter gesture,
//     continuous in Hz, no FX engine.
//   • THE VOID — the last beat before every drop, the whole band cuts to
//     dead silence; the drop slams in from nothing. One `return` statement,
//     the genre's most reliable thrill.
//   • THE LOOP — a STOLEN PLAYBOOK (jingle.c style): 4-bar template loops
//     transcribed from the records French house actually sampled — Modjo's
//     Chic loop (i9-bVImaj7-iv9-bVII9), Stardust's Neapolitan bIImaj7,
//     Braxe/Falke's i-bIII-iv, One More Time's tonic-avoiding IVmaj7-V-iii7
//     (Chilly Gonzales: the unresolved progression IS the longing), the
//     Digital Love slash-bass IVmaj7-I/3-ii9-V9sus — plus flavor rolls (the
//     Lost in Music iv->IV9 dorian flip, the V melting to 9sus) and the
//     needle-drop ROTATION (lowend.c's move). Sources at the chord tables.
//     The arrangement does all the work; the loop just spins.
//   • the DA FUNK LEAD — a seeded riff on a held mono square with
//     note_glide portamento, gated with note_vol. Drops only.
//   • 808 DRUMS ON LOAN from tools/carts/tr808.c — the measured circuits:
//     bridged-T kick boom (+26st drop), the three-retrigger handclap, the
//     metal-bank hats, the long cymbal. House was born on this box.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help
//
// Density = arrangement curve x feel shift (game-music.md). Seed pins the
// COMPOSITION (key, loop, stab mask, bassline, riff, title); the PERFORMANCE
// (ride wobble, hat touch, ghost stabs) re-rolls every playthrough.

#define HOUSE_SEED 0   // pin a favourite track here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_STAB 5    // the chopped sample — saw stabs through the ride
#define I_BASS 6    // disco bass
#define I_LEAD 7    // the Da Funk mono lead
#define I_STR  8    // string machine — 2 held voices, the ride's main canvas
// drum slots 9..14 — circuits from tr808.c
#define SL_KICK 9
#define SL_CP   10
#define SL_HATC 11
#define SL_HATO 12
#define SL_CYM  13
#define SL_MAR  14

// ── chords — the stolen playbook ──────────────────────────────────────────
// French house harmony is SAMPLED late-70s disco, so the chord brain is a
// catalogue, not a generator (the jingle.c / lowend.c approach): template
// loops transcribed from the actual records, with flavor rolls. Sources:
//   Stardust "Music Sounds Better With You" (<- Chaka Khan "Fate"):
//     Em7 Fmaj7 Cmaj7 Fmaj7 = i7 bIImaj7 bVImaj7 bIImaj7 — the Neapolitan
//   Modjo "Lady" (<- Chic "Soup for One"): Bbm9 Gbmaj7 Ebm9 Ab9
//   Braxe/Falke "Intro": Fm Ab Bbm7 = i bIII iv7
//   Daft Punk "One More Time" (<- Eddie Johns): IVmaj7 V iii7 V — the major
//     loops AVOID the root-position tonic (Chilly Gonzales on Digital Love:
//     the unresolved progression IS the longing); alt transcription
//     Fmaj7 Cmaj7/E Em7 G adds the stepwise SLASH-CHORD bass
//   Sister Sledge "Lost in Music" (Attack Magazine, Lessons From Disco
//     Chords): Dm7 C Gm->G Bbmaj7 — the iv flipping MAJOR (the dorian move,
//     same engine as "Good Times"' Em9-A13 vamp)
enum { Q_MAJ7, Q_MIN7, Q_MIN9, Q_DOM9, Q_SUS9, NQ };
static const char *QN[NQ] = { "maj7", "m7", "m9", "9", "9sus" };
static const int QV[NQ][3] = {
    { 4, 11, 14 },   // maj7  — voiced with the 9, disco-session lush
    { 3, 10, 12 },   // m7    — plain (the iii7: a 9th would leave the key)
    { 3, 10, 14 },   // m9    — the Chic color, the default minor
    { 4, 10, 14 },   // 9     — dominant with the 9
    { 5, 10, 14 },   // 9sus  — the suspended V, longing unresolved
};
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
static const int PENTMIN[5] = { 0, 3, 5, 7, 10 };
static const int PENTMAJ[5] = { 0, 2, 4, 7, 9 };

typedef struct { int off, q, bass; } Ch;   // bass: semitones above chord root
                                           // (4 = first inversion, the /3)
// side A — minor: the filtered funk crates
static const Ch TMPL_MIN[5][4] = {
    // "lady": i9 bVImaj7 iv9 bVII9 (Modjo / Chic)
    { {0,Q_MIN9,0}, {8,Q_MAJ7,0}, {5,Q_MIN9,0}, {10,Q_DOM9,0} },
    // "stardust": i7 bIImaj7 bVImaj7 bIImaj7 — the Neapolitan shimmer
    { {0,Q_MIN9,0}, {1,Q_MAJ7,0}, {8,Q_MAJ7,0}, {1,Q_MAJ7,0} },
    // "braxe": i9 bIIImaj7 iv9 i9
    { {0,Q_MIN9,0}, {3,Q_MAJ7,0}, {5,Q_MIN9,0}, {0,Q_MIN9,0} },
    // "lost in music": i9 bVII9 IV9(major! the dorian flip) bVImaj7
    { {0,Q_MIN9,0}, {10,Q_DOM9,0}, {5,Q_DOM9,0}, {8,Q_MAJ7,0} },
    // "good times": the two-chord dorian vamp, i9 to IV9 forever
    { {0,Q_MIN9,0}, {0,Q_MIN9,0}, {5,Q_DOM9,0}, {5,Q_DOM9,0} },
};
// side B — major: lush, and the tonic never sits at home
static const Ch TMPL_MAJ[3][4] = {
    // "one more time": IVmaj7 V9 iii7 V9sus
    { {5,Q_MAJ7,0}, {7,Q_DOM9,0}, {4,Q_MIN7,0}, {7,Q_SUS9,0} },
    // "royal road": IVmaj7 V9 iii7 vi9 (city pop's loop, a continent over)
    { {5,Q_MAJ7,0}, {7,Q_DOM9,0}, {4,Q_MIN7,0}, {9,Q_MIN9,0} },
    // "digital": IVmaj7 I/3 ii9 V9sus — the stepwise slash-chord bass
    { {5,Q_MAJ7,0}, {0,Q_MAJ7,4}, {2,Q_MIN9,0}, {7,Q_SUS9,0} },
};

// ── the form — 8 phrases x 8 bars; the RIDE keys off this, not the chords ─
enum { S_INTRO, S_GROOVE, S_BUILD, S_DROP, S_BREAK, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_GROOVE, S_BUILD, S_DROP,
                             S_BREAK, S_BUILD, S_DROP, S_OUTRO };
static const char *SECTNAME[6] = { "intro", "groove", "build", "drop", "break", "outro" };

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc, minorKey;
    Ch   loop[4];             // the disco loop, one chord per bar, spun forever
    int  bassOct;             // octave-pop disco bass vs masked funk walk
    int  bassDeg[8];          // funk mode: seeded semitone offsets per 8th
    int  bassMask;            // funk mode: which 8ths speak
    int  stOn[6], stN;        // the stab chops (16-step mask)
    int  rfMidi[6], rfOn[6], rfN, hasLead;   // the Da Funk riff
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song   sng;
static int    tempo     = 122;
static int    intensity = 1;     // feel: shifts the arrangement's density curve
static bool   radioOn   = true;
static bool   showHelp  = false;
static long   scheduled = -1;
static long   songBase  = 0;
static int    songCount = 0;
static double stepMs    = 123.0;
static float  vu        = 0;
static int    bassLast  = 38;
static char   nowChord[4][12];

// voice-led voicings: stabs and strings each keep their own fingers
static int  gvStab[3] = { 64, 67, 71 };
static int  gvStr[2]  = { 57, 62 };
static bool stabInit = false, strInit = false;

// held voices
static int  strH[2] = { -1, -1 };
static int  leadH   = -1;
static int  strVolNow = -1;
static long strBar  = -1, leadStep = -1;
static int  curCut  = 1200;      // the ride's live cutoff, for display + trace

static int iabs(int v) { return v < 0 ? -v : v; }

// composition PRNG (xorshift32) — same contract as the other radios
static unsigned rngState = 1;
static unsigned srnd_u(void) {
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return rngState;
}
static int srnd(int n) { return (int)(srnd_u() % (unsigned)n); }

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Da", "Disco", "Midnight", "Cosmic", "Roller", "Velvet",
    "Crystal", "Super", "Phantom", "Mirror", "Neon", "Riviera" };
static const char *TW2[] = { "Funk", "Machine", "Boulevard", "Fever", "Memories",
    "Sensation", "Nuit", "Paradise", "Groove", "Carousel", "Express", "Soleil" };

static void new_song(double pos, unsigned seed) {
    if (!seed) seed = ((unsigned)rnd(0x10000) << 16) ^ (unsigned)rnd(0x10000)
                      ^ (unsigned)frame() * 2654435761u;
    if (!seed) seed = 1;
    rngState = sng.seed = seed;

    sng.keyPc    = srnd(12);
    sng.minorKey = srnd(100) < 60;

    // the loop: pick ONE transcribed template from the crate...
    {
        const Ch *t = sng.minorKey ? TMPL_MIN[srnd(5)] : TMPL_MAJ[srnd(3)];
        for (int i = 0; i < 4; i++) sng.loop[i] = t[i];
    }
    // ...then the flavor rolls (jingle.c style):
    if (sng.minorKey && srnd(100) < 25)            // the Lost in Music move —
        for (int i = 1; i < 4; i++)                // flip a minor iv to major IV9
            if (sng.loop[i].off == 5 && sng.loop[i].q == Q_MIN9)
                { sng.loop[i].q = Q_DOM9; break; }
    if (!sng.minorKey && srnd(100) < 30)           // melt the V into a 9sus
        for (int i = 1; i < 4; i++)
            if (sng.loop[i].off == 7 && sng.loop[i].q == Q_DOM9)
                { sng.loop[i].q = Q_SUS9; break; }
    if (srnd(100) < 35) {                 // the needle drop: rotate the loop —
        int r = 1 + srnd(3);              // the sample starts where the needle
        Ch tmp[4];                        // fell, not where home is
        for (int i = 0; i < 4; i++) tmp[i] = sng.loop[(i + r) % 4];
        for (int i = 0; i < 4; i++) sng.loop[i] = tmp[i];
    }

    // the bass: octave disco pop, or a seeded funk walk on masked 8ths
    sng.bassOct = srnd(100) < 55;
    static const int BDEG[8] = { 0, 0, 0, 7, 7, 12, 10, 5 };
    sng.bassMask = 1;
    for (int b = 1; b < 8; b++) if (srnd(100) < 55) sng.bassMask |= 1 << b;
    for (int b = 0; b < 8; b++) sng.bassDeg[b] = BDEG[srnd(8)];

    // the stab chops: 3..5 onsets, leaning hard into the offbeats
    sng.stN = 0;
    static const int SCAND[10] = { 2, 4, 6, 7, 8, 10, 11, 12, 14, 15 };
    for (int i = 0; i < 10 && sng.stN < 5; i++)
        if (srnd(100) < 38) sng.stOn[sng.stN++] = SCAND[i];
    if (sng.stN < 3) { sng.stN = 3; sng.stOn[0] = 2; sng.stOn[1] = 7; sng.stOn[2] = 10; }

    // the Da Funk riff: a seeded 1-bar cell in the key's pentatonic
    sng.hasLead = srnd(100) < 65;
    const int *pent = sng.minorKey ? PENTMIN : PENTMAJ;
    sng.rfN = 0;
    static const int RCAND[11] = { 0, 2, 3, 4, 6, 7, 8, 10, 11, 12, 14 };
    for (int i = 0; i < 11 && sng.rfN < 6; i++)
        if (srnd(100) < 40) sng.rfOn[sng.rfN++] = RCAND[i];
    if (sng.rfN < 3) { sng.rfN = 3; sng.rfOn[0] = 0; sng.rfOn[1] = 6; sng.rfOn[2] = 10; }
    for (int i = 0; i < sng.rfN; i++) {
        int m = 60 + sng.keyPc + pent[srnd(5)];
        while (m > 77) m -= 12;
        while (m < 58) m += 12;
        sng.rfMidi[i] = m;
    }

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    if (srnd(100) < 30) {                              // the disco edit
        size_t len = 0; while (sng.title[len]) len++;
        if (len + 5 < sizeof sng.title) snprintf(sng.title + len, 6, " edit");
    }
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 114 + srnd(13);                            // 114..126
    bpm(tempo);
    songBase = (long)pos + 8;
    stabInit = false;
    bassLast = 38;
    leadStep = -1;
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

// ── form / harmony ────────────────────────────────────────────────────────
static int sect_of(long bar)  { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }
static Ch  chord_at(long bar) { return sng.loop[bar % 4]; }
static int root_pc(Ch c)      { return (sng.keyPc + c.off) % 12; }
static int bass_pc(Ch c)      { return (sng.keyPc + c.off + c.bass) % 12; }  // the slash

static void chord_label(char *out, int n, Ch c) {
    if (c.bass)
        snprintf(out, n, "%s%s/%s", PCNAME[root_pc(c)], QN[c.q], PCNAME[bass_pc(c)]);
    else
        snprintf(out, n, "%s%s", PCNAME[root_pc(c)], QN[c.q]);
}

// drums sit out the break, and the last bars of the outro
static bool drums_on(long bar) {
    int s = sect_of(bar);
    if (s == S_BREAK) return false;
    if (s == S_OUTRO && bar % 8 >= 6) return false;
    return true;
}

// density = arrangement curve + feel shift (see game-music.md)
//   0: kick + hats + bass + strings    2: + 3-voice stabs and the lead
//   1: + the chops and the clap        3: + 16th shaker, hat doubles
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_DROP ? 2 : 1);
    int lvl = base + intensity - 1;
    return lvl < 0 ? 0 : lvl > 3 ? 3 : lvl;
}

// ── THE RIDE — cutoff position 0..1 from the song position (in 16th steps) ─
static float ride_at(double sp) {
    if (sp < 0) sp = 0;
    if (sp > 1023) sp = 1023;
    long bar = (long)(sp / 16.0);
    long phraseBar = (bar / 8) * 8;                  // first bar of this phrase
    float t = (float)((sp / 16.0 - (double)phraseBar) / 8.0);   // 0..1 in phrase
    switch (sect_of(bar)) {
    case S_INTRO:  return 0.20f + 0.20f * t;
    case S_GROOVE: return 0.42f + 0.08f * t;
    case S_BUILD:  return 0.30f + 0.70f * t * t;     // the sweep
    case S_DROP:   return 1.00f;
    case S_BREAK:  return 0.35f + 0.30f * t;
    default:       return 0.50f - 0.42f * t;         // outro: close it down
    }
}
static int ride_res(double sp) {                     // resonance climbs the build
    long bar = (long)(sp / 16.0);
    if (sp < 0 || sect_of(bar) != S_BUILD) return 2;
    long phraseBar = (bar / 8) * 8;
    float t = (float)((sp / 16.0 - (double)phraseBar) / 8.0);
    return 2 + (int)(t * 7);
}

// ── the tone knob (T cycles) — scales the ride's reach, live ──────────────
static int toneSel = 2;
static const char *TONENAME[4] = { "mellow", "warm", "clear", "bright" };
static const float TONEMUL[4]  = { 0.55f, 0.78f, 1.0f, 1.28f };

// nearest-tone voice leading — tenth cart, same block, generalized so the
// stabs and the strings each lead their own fingers
static void lead_to(Ch c, int *v, int n, int lo, int hi, bool *init) {
    int pcs[3];
    for (int k = 0; k < 3; k++) pcs[k] = (root_pc(c) + QV[c.q][k]) % 12;
    if (!*init) {
        for (int k = 0; k < n; k++) {
            int target = lo + 5 + k * 5;
            int dd = ((pcs[k] - target) % 12 + 18) % 12 - 6;
            v[k] = target + dd;
        }
        *init = true;
    } else {
        bool used[3] = { false, false, false };
        for (int vi = 0; vi < n; vi++) {
            int bestJ = -1, bestC = 0, bestD = 99;
            for (int j = 0; j < 3; j++) {
                if (used[j]) continue;
                int dd = ((pcs[j] - v[vi]) % 12 + 18) % 12 - 6;
                if (iabs(dd) < bestD) { bestD = iabs(dd); bestJ = j; bestC = v[vi] + dd; }
            }
            used[bestJ] = true;
            v[vi] = bestC;
        }
    }
    for (int k = 0; k < n; k++) {
        while (v[k] < lo) v[k] += 12;
        while (v[k] > hi) v[k] -= 12;
    }
}

// voice-led bass register: E1..A2
static int bass_near(int pc) {
    int d = ((pc - bassLast) % 12 + 18) % 12 - 6;
    int n = bassLast + d;
    while (n < 28) n += 12;
    while (n > 45) n -= 12;
    return bassLast = n;
}

// ── 808 voices, on loan from tr808.c (the measured circuits) ──────────────
static void clap_808(int dly, int vol) {       // three retriggers + a room tail
    schedule_hit(dly,      60, SL_CP, vol, 12);
    schedule_hit(dly + 10, 60, SL_CP, vol, 12);
    schedule_hit(dly + 20, 60, SL_CP, vol, 12);
    schedule_hit(dly + 28, 60, SL_CP, vol - 1, 140);
}

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = (int)((abs - pos) * stepMs);
    if (dly < 1) dly = 1;
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;
    Ch   c    = chord_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    bool dr   = drums_on(bar);

    // THE VOID — the last beat before the drop: everybody out. The next
    // thing anyone hears is the downbeat, wide open.
    if (sect == S_BUILD && bar % 8 == 7 && step >= 12) return;

    // crash on the drop's downbeat (two of the metal bank, long ring)
    if (step == 0 && bar % 8 == 0 && sect == S_DROP) {
        schedule_hit(dly, 79, SL_CYM, 4, 900);
        schedule_hit(dly, 72, SL_CYM, 3, 900);
        vu += 3;
    }

    // KICK — four on the floor; the 808 boom holds the room
    if (dr && step % 4 == 0) {
        schedule_hit(dly + rnd(2), 31, SL_KICK, 6, 280);
        vu += 2.5f;
    }
    // build's last bar: the 8th-note kick roll climbing into the void
    if (sect == S_BUILD && bar % 8 == 7 && step < 12 && step % 2 == 0 && step % 4 != 0)
        schedule_hit(dly + rnd(2), 31, SL_KICK, 4 + step / 6, 130);

    // CLAP on 2 and 4 — the three-retrigger circuit
    if (dr && lvl >= 1 && (step == 4 || step == 12)) {
        clap_808(dly + rnd(2), 4);
        vu += 2;
    }

    // HATS — closed rides the floor, the open hat breathes the offbeat
    if (dr) {
        if (step % 4 == 0)
            schedule_hit(dly + rnd(2), 79, SL_HATC, 2, 50);
        if (step % 4 == 2) {
            schedule_hit(dly + rnd(2), 79, SL_HATO, 3, 280);
            if (lvl >= 3) schedule_hit(dly + rnd(2), 72, SL_HATO, 2, 280);
            vu += 0.8f;
        }
        if (lvl >= 3 && step % 2 == 1)                  // 16th maracas shimmer
            schedule_hit(dly + rnd(2), 90, SL_MAR, 1, 25);
    }

    // BASS — octave disco pop, or the masked funk walk (intro waits 4 bars)
    if (!(sect == S_INTRO && bar % 8 < 4)) {
        if (sng.bassOct) {
            int b = bass_near(bass_pc(c));               // the slash bass lives here
            if (step % 8 == 0)
                { schedule_hit(dly + rnd(2), b, I_BASS, 6, (int)(stepMs * 1.7)); vu += 2; }
            else if (step % 4 == 2)
                { schedule_hit(dly + rnd(2), b + 12, I_BASS, 4, 90); vu += 1; }   // the pop
            else if (step == 14) {                       // approach into the next bar
                int np = bass_pc(chord_at(bar + 1));
                if (np != bass_pc(c))
                    schedule_hit(dly + rnd(2), bass_near(np) + (chance(50) ? -1 : 1),
                                 I_BASS, 4, 80);
            }
        } else if (step % 2 == 0 && (sng.bassMask >> (step / 2)) & 1) {
            int b = (step == 0) ? bass_near(bass_pc(c)) : bassLast;
            int n = b + sng.bassDeg[step / 2];
            while (n > 45) n -= 12;
            schedule_hit(dly + rnd(2), n, I_BASS, 5, (int)(stepMs * 1.4));
            vu += 1.5f;
        }
    }

    // STABS — the chopped loop; always on in the break (the sample, exposed)
    if (lvl >= 1 || sect == S_BREAK) {
        for (int i = 0; i < sng.stN; i++)
            if (sng.stOn[i] == step) {
                lead_to(c, gvStab, 3, 57, 81, &stabInit);
                int nv = (i == 0 || lvl >= 2) ? 3 : 2;
                int vol = (sect == S_BREAK) ? 5 : 4;
                for (int k = 0; k < nv; k++)
                    schedule_hit(dly + k * 6 + rnd(3), gvStab[k], I_STAB, vol, 140);
                vu += 1.6f;
            }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_STAB, INSTR_SAW, 1, 130, 2, 60);            // the chopped sample
    instrument_filter(I_STAB, FILTER_LOW, 1400, 2);          // re-aimed by the ride

    instrument(I_BASS, INSTR_TRI, 1, 150, 4, 60);            // round disco bass
    instrument_filter(I_BASS, FILTER_LOW, 600, 2);
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 15, 4);          // the pluck snap

    instrument(I_LEAD, INSTR_SQUARE, 8, 140, 5, 100);        // the Da Funk mono
    instrument_duty(I_LEAD, 0.30f);
    instrument_filter(I_LEAD, FILTER_LOW, 2100, 4);
    instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.4f, 0.08f);

    instrument(I_STR, INSTR_SAW, 320, 500, 6, 900);          // string machine
    instrument_filter(I_STR, FILTER_LOW, 900, 2);            // the ride owns this

    // — 808 circuits, verbatim from tr808.c —
    instrument(SL_KICK, INSTR_SINE, 0, 480, 0, 60);          // bridged-T boom
    instrument_filter(SL_KICK, FILTER_LOW, 250, 3);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 50, 26.0f);

    instrument(SL_CP, INSTR_NOISE, 0, 110, 0, 50);           // handclap noise
    instrument_filter(SL_CP, FILTER_BAND, 1100, 5);

    instrument(SL_HATC, INSTR_SQUARE, 0, 42, 0, 16);         // metal bank, hp'd
    instrument_filter(SL_HATC, FILTER_HIGH, 7000, 3);
    instrument(SL_HATO, INSTR_SQUARE, 0, 340, 0, 90);
    instrument_filter(SL_HATO, FILTER_HIGH, 7000, 3);

    instrument(SL_CYM, INSTR_SQUARE, 0, 850, 0, 200);        // the long ring
    instrument_filter(SL_CYM, FILTER_HIGH, 3440, 3);

    instrument(SL_MAR, INSTR_NOISE, 0, 24, 0, 10);
    instrument_filter(SL_MAR, FILTER_HIGH, 6500, 2);
}

// ── held-voice control: strings + the lead ────────────────────────────────
static void stop_held(void) {
    note_off_all();
    strH[0] = strH[1] = leadH = -1;
    strInit = false;
    strVolNow = -1;
    strBar = -1;
}

static void drive_strings(long bar, bool voidNow, int sect) {
    Ch c = chord_at(bar < 0 ? 0 : bar);
    if (strH[0] < 0) {
        lead_to(c, gvStr, 2, 50, 69, &strInit);
        for (int i = 0; i < 2; i++) {
            strH[i] = note_on(gvStr[i], I_STR, 3);
            note_glide(strH[i], 150);
            note_lfo(strH[i], 0, LFO_PITCH, rnd_float_between(0.10f, 0.22f),
                                            rnd_float_between(0.04f, 0.08f));  // chorus wow
            note_pitch(strH[i], gvStr[i] + rnd_float_between(-0.05f, 0.05f));
        }
        strBar = bar;
        strVolNow = 3;
    } else if (bar != strBar && bar >= 0) {              // new bar: slide the pad
        strBar = bar;
        lead_to(c, gvStr, 2, 50, 69, &strInit);
        for (int i = 0; i < 2; i++) note_pitch(strH[i], gvStr[i]);
    }
    int want = voidNow ? 0 : (sect == S_BREAK ? 5 : 3);  // the break is the swell
    if (want != strVolNow) {
        for (int i = 0; i < 2; i++) note_vol(strH[i], want);
        strVolNow = want;
    }
}

static void drive_lead(double pos) {
    long stepNow = (long)pos;
    if (stepNow == leadStep) return;
    leadStep = stepNow;
    long s = stepNow - songBase;
    long bar = s >= 0 ? s / 16 : 0;
    bool voidNow = s >= 0 && sect_of(bar) == S_BUILD && bar % 8 == 7 && s % 16 >= 12;
    bool want = sng.hasLead && s >= 0 && bar < 64 && !voidNow
                && sect_of(bar) == S_DROP && level_of(bar) >= 2;
    if (!want) {
        if (leadH >= 0) { note_off(leadH); leadH = -1; }
        return;
    }
    if (leadH < 0) {
        leadH = note_on(sng.rfMidi[0], I_LEAD, 0);
        note_glide(leadH, 70);                           // the portamento IS the hook
    }
    int st16 = (int)(s % 16);
    for (int i = 0; i < sng.rfN; i++)
        if (sng.rfOn[i] == st16 && chance(92)) {
            note_pitch(leadH, sng.rfMidi[i]);
            note_vol(leadH, 4);
            vu += 1.2f;
        }
    if (st16 == 15 && chance(35)) note_vol(leadH, 0);    // breathe between bars
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (HOUSE_SEED) { new_song(pos, HOUSE_SEED); hist[histN++] = sng.seed; histPos = 0; }
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
    if (keyp(KEY_UP)   && tempo < 132) { tempo += 2; bpm(tempo); }
    if (keyp(KEY_DOWN) && tempo > 108) { tempo -= 2; bpm(tempo); }
    if (keyp('T')) toneSel = (toneSel + 1) % 4;
    if (keyp('M')) {
        radioOn = !radioOn;
        if (!radioOn) stop_held();
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
        if (songStep >= 64L * 16) fresh_song(pos);

        long bar = songStep >= 0 ? songStep / 16 : 0;
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 12, sng.loop[i]);

        // ── THE RIDE — every frame: aim the filters from the arrangement ──
        double sp = pos - songBase;
        float ride = ride_at(sp) + 0.04f * sinf(timer() * 0.31f);   // perf wobble
        if (ride < 0.10f) ride = 0.10f;
        if (ride > 1.0f)  ride = 1.0f;
        int res = ride_res(sp);
        float tm = 0.7f + 0.3f * TONEMUL[toneSel];
        curCut = (int)((240.0f + ride * ride * 4800.0f) * tm);

        int  sect    = sp >= 0 ? sect_of(bar) : S_INTRO;
        bool voidNow = sp >= 0 && sect == S_BUILD && bar % 8 == 7
                       && ((long)sp) % 16 >= 12;

        // the strings: pump against the kick (duck at the beat, bloom after)
        float pump = (sp >= 0 && drums_on(bar) && !voidNow)
                     ? 0.55f + 0.45f * (float)beat_pos() : 1.0f;
        drive_strings(sp >= 0 ? bar : -1, voidNow, sect);
        for (int i = 0; i < 2; i++)
            if (strH[i] >= 0) {
                note_cutoff(strH[i], (int)(curCut * pump));
                note_res(strH[i], res);
            }

        // the stabs + bass read the same curve (each hit picks it up)
        instrument_filter(I_STAB, FILTER_LOW, curCut, res);
        instrument_filter(I_BASS, FILTER_LOW, 380 + (int)(ride * 500.0f * tm), 2);
        instrument_filter(I_LEAD, FILTER_LOW, (int)(2100 * tm), 4);

        drive_lead(pos);
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("chord", "%s", nowChord[(int)(tbar % 4)]);
    watch("cut", "%d", curCut);
#endif
}

// ── draw — the discotheque chassis ────────────────────────────────────────
static void knob(int x, int y, int r, float t, const char *label, int col) {
    circfill(x, y, r, CLR_DARK_GREY);
    circ(x, y, r, CLR_BLACK);
    float a = (-0.75f + t * 1.5f) * 3.14159f;
    line(x, y, x + (int)(sinf(a) * (r - 2)), y - (int)(cosf(a) * (r - 2)), col);
    print(label, x - text_width(label) / 2, y + r + 3, CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_BLACK);
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    // body — chrome with a neon edge
    rectfill(20, 16, 280, 168, CLR_LIGHT_GREY);
    rectfill(24, 20, 272, 160, CLR_BLACK);
    rectfill(24, 20, 272, 2, CLR_PINK);

    // dial strip
    rectfill(32, 28, 256, 16, CLR_DARKER_GREY);
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * 13;
        line(x, 38, x, 42, CLR_DARK_GREY);
        if (fq % 4 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 6, 30, CLR_LIGHT_GREY);
        }
    }
    int nx = 36 + (int)((sng.freq - 88.0f) * 13.0f);
    line(nx, 29, nx, 43, CLR_PINK);

    // the window — mirror ball over the filter readout
    rectfill(34, 52, 102, 116, CLR_BLACK);
    float breathe = vu / 12.0f;
    line(85, 53, 85, 64, CLR_DARK_GREY);                     // the wire
    circfill(85, 92, 27, CLR_DARKER_GREY);                   // the ball
    for (int gy2 = 66; gy2 <= 118; gy2 += 4)
        for (int gx2 = 59; gx2 <= 111; gx2 += 4) {
            int dx = gx2 - 85, dy2 = gy2 - 92;
            if (dx * dx + dy2 * dy2 > 27 * 27) continue;
            int tw = (gx2 * 7 + gy2 * 13 + frame() / 5) % 23;
            pset(gx2, gy2, tw < 2 ? CLR_WHITE
                         : tw < 5 ? CLR_LIGHT_GREY
                         : ((gx2 + gy2) / 4) % 2 ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
        }
    if (radioOn && sect == S_DROP && breathe > 0.4f)         // beams at the drop
        for (int k = 0; k < 4; k++) {
            float a = 0.6f + k * 0.55f + timer() * 0.7f;
            line(85, 92, 85 + (int)(sinf(a) * 46), 92 + (int)(cosf(a) * 46),
                 k % 2 ? CLR_PINK : CLR_DARK_PURPLE);
        }

    // THE FILTER — the ride, live (this slider is the whole genre)
    rectfill(40, 134, 90, 26, CLR_DARKER_GREY);
    rect(40, 134, 90, 26, CLR_DARK_GREY);
    print("filter", 44, 137, CLR_LIGHT_GREY);
    line(46, 152, 124, 152, CLR_DARK_GREY);
    if (radioOn) {
        float rt = (curCut - 240.0f) / 5000.0f;
        if (rt < 0) rt = 0;
        if (rt > 1) rt = 1;
        int fx = 46 + (int)(rt * 78);
        rectfill(fx - 1, 147, 3, 11, CLR_PINK);
        for (int x = 46; x < fx; x += 3) pset(x, 152, CLR_PINK);
    }
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_PINK);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_PINK);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s", sng.freq,
                 sng.minorKey ? "funk" : "lush");
        print(l2, 154, 70, CLR_DARK_PURPLE);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_DARK_PURPLE);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_PINK);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // the loop, current chord boxed
    if (radioOn) {
        int ci = (int)(bar % 4);
        int x = 152;
        for (int i = 0; i < 4; i++) {
            int cw = text_width(nowChord[i]);
            if (x + cw > 292) break;
            if (i == ci) {
                rectfill(x - 2, 102, cw + 4, 12, CLR_PINK);
                print(nowChord[i], x, 104, CLR_BLACK);
            } else
                print(nowChord[i], x, 104, CLR_LIGHT_GREY);
            x += cw + 8;
        }
        print(SECTNAME[sect], 152, 120, CLR_PINK);
        for (int i = 0; i < 8; i++)
            circfill(232 + i * 7, 124, 1, i <= bar / 8 ? CLR_PINK : CLR_DARK_GREY);
    }

    // knobs + power LED
    static const char *FEEL[4] = { "afterhours", "warmup", "club", "peak" };
    knob(168, 148, 9, intensity / 3.0f, FEEL[intensity], CLR_PINK);
    knob(218, 148, 9, (tempo - 108) / 24.0f, "tempo", CLR_PINK);
    knob(262, 148, 11, toneSel / 3.0f, TONENAME[toneSel], CLR_PINK);
    circfill(282, 28, 2, radioOn && beat_pos() < 0.25f ? CLR_PINK : CLR_DARK_PURPLE);

    // help button + hint
    circfill(288, 172, 6, CLR_DARKER_GREY);
    circ(288, 172, 6, CLR_BLACK);
    print("?", 285, 169, CLR_PINK);
    print("SPACE next song   H help", 8, 190, CLR_DARK_GREY);

    if (showHelp) {
        rectfill(44, 40, 232, 122, CLR_BLACK);
        rect(44, 40, 232, 122, CLR_PINK);
        print("HOUSE RADIO", 52, 46, CLR_PINK);
        font(FONT_SMALL);
        static const char *HELP[8][2] = {
            { "SPACE",      "next track (rolls a new seed)" },
            { "R",          "same track again - a fresh ride" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - shifts the density curve" },
            { "UP/DOWN",    "tempo of this track" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
        };
        for (int i = 0; i < 8; i++) {
            print(HELP[i][0], 52, 58 + i * 9, CLR_YELLOW);
            print(HELP[i][1], 106, 58 + i * 9, CLR_WHITE);
        }
        print("one loop, forever - the FILTER is the song.", 52, 132, CLR_PINK);
        print("pin it: #define HOUSE_SEED 0x...", 52, 141, CLR_PINK);
        print("drums on loan from the tr-808 cart", 52, 150, CLR_PINK);
        font(FONT_NORMAL);
    }
}
