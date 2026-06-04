#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── LOW END RADIO ─────────────────────────────────────────────────────────
// The night-bus sibling of the radio family: endless jazz-rap boom bap, the
// A Tribe Called Quest / Low End Theory homage. Built from two sources:
//
// 1. The r/musictheory analysis of the "Electric Relaxation" loop (sampled
//    from Ronnie Foster's "Mystic Brew"):
//      Bmaj7 - D#sus/A# - G#maj9/D# - G#maj9 - F#sus2 - Emaj9#11
//    i.e. bIII - Vsus - I - bVII - bVI in G#, mixing Aeolian and Ionian.
//    The lessons, encoded below:
//      • MODAL MIXTURE LOOPS — borrowed chords (bIII, bVI, bVII) voiced LUSH:
//        maj7/maj9, never dark. A 9sus stands in for the dominant.
//        The bVI gets the maj9#11 (that Emaj9#11 is the prettiest chord
//        on the record).
//      • ODD LOOP LENGTHS — Electric Relaxation is a THREE-bar loop. 3-bar
//        loops keep the head nodding because the ear never quite squares it.
//      • "their sampling moved the tonic" — loop a fragment of a longer
//        progression and home moves. We generate a progression with a real
//        Vsus→I cadence inside it, then ROTATE the loop like a crate-digger
//        dropping the needle mid-phrase. Where the loop starts is not
//        where the song resolves. That ambiguity IS the vibe.
//
// 2. The Low End Theory production playbook: bass first (Ron Carter upright,
//    front and center), minimalism (bass + drums + one or two elements, no
//    more), crisp drum programming with LAYERED hits (Q-Tip stacked up to
//    three snares into one sound — here: noise crack + sine thump fired
//    together), and the groove template from docs/guides/game-music.md:
//    hats rush −8ms, snare drags +22ms, bass leans +12ms, kick holds the
//    grid, 16ths swung ~57%. Samplers don't drift: NO tempo wobble here,
//    unlike jangle/jingle — the machine is steady and the players lean.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (layers)   UP/DOWN tempo   H or ? help
//
// Seed pins the COMPOSITION (key, progression, loop cut+rotation, drum
// pattern, title); the PERFORMANCE (ghost notes, fills, the lead's path,
// micro-jitter) re-rolls every playthrough.

#define LOWEND_SEED 0   // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_RHODES 5   // tremolo ep stabs
#define I_BASS   6   // the star — upright-ish, deep
#define I_LEAD   7   // sparse hook lead
#define I_KICK   8
#define I_SNARE  9
#define I_HAT    10
#define I_VINYL  11  // the dust

// ── the groove template (ms, added to dly) ────────────────────────────────
#define PUSH_KICK   0    // the kick IS the grid
#define PUSH_HAT   -8    // hats rush, eager
#define PUSH_SNARE 22    // the snare drags — the head-nod
#define PUSH_BASS  12    // bass leans toward the snare's time
#define SWING      0.14f // odd 16ths land 57% late

// ── chords — everything voiced lush ──────────────────────────────────────
enum { Q_MAJ7, Q_MAJ9, Q_MAJ9S11, Q_SUS9, Q_MIN9, NQ };
static const char *QN[NQ] = { "maj7", "maj9", "maj9+", "9sus", "m9" };
static const int QV[NQ][3] = {     // rootless 3-voice rhodes voicings
    { 4, 11, 14 },   // maj7(9)
    { 4, 11, 14 },   // maj9
    { 4, 11, 18 },   // maj9#11 — the Emaj9#11 move
    { 5, 10, 14 },   // 9sus
    { 3, 10, 14 },   // m9
};
static const int QT[NQ][4] = {     // chord tones for the lead (accommodation everywhere)
    { 0, 4, 7, 11 }, { 0, 4, 11, 14 }, { 0, 4, 11, 18 }, { 0, 5, 10, 14 }, { 0, 3, 10, 14 },
};
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

// modal-mixture pool: degree offset, quality, weight (repeats)
static const int POOL_OFF[8]  = { 3, 3, 8, 8, 10, 10, 5, 2 };   // bIII bIII bVI bVI bVII bVII IV ii
static const int POOL_Q[8]    = { Q_MAJ7, Q_MAJ9, Q_MAJ9, Q_MAJ9S11, Q_MAJ9, Q_MAJ7, Q_MAJ9, Q_MIN9 };

// boom-bap kick patterns (2 bars = 32 steps); snare lives on 4/12/20/28 always
static const int KICKP[4][6] = {
    { 0, 10, 16, 22, -1, -1 },
    { 0, 6, 16, 26, -1, -1 },
    { 0, 10, 14, 16, 26, -1 },
    { 0, 7, 16, 23, 26, -1 },
};

// the form: 8 sections of 8 bars — note the loop length usually does NOT
// divide 8, so the sample rolls over section lines. That's authentic.
enum { S_INTRO, S_V, S_H, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_V, S_V, S_H, S_V, S_V, S_H, S_OUTRO };

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    int  off[8], q[8], nCh;   // the loop: 4/6/8 chords, one per half-bar
    int  loopBars;            // 2, 3 (favoured), or 4
    int  kick;                // pattern index
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song   sng;
static int    tempo     = 92;
static int    intensity = 2;     // 0 bass+drums · 1 +rhodes · 2 +hook lead · 3 16th hats
static bool   radioOn   = true;
static bool   showHelp  = false;
static long   scheduled = -1;
static long   songBase  = 0;
static int    songCount = 0;
static double stepMs    = 163.0;
static int    gv[3]     = { 64, 67, 71 };
static bool   gvInit    = false;
static float  vu        = 0;
static int    melPitch  = 81;
static int    bassLast  = 38;
static int    cellOn[5], cellN = 0;
static char   nowChord[2][12];   // current / next

static int iabs(int v) { return v < 0 ? -v : v; }

// composition PRNG (xorshift32) — same contract as the other radios
static unsigned rngState = 1;
static unsigned srnd_u(void) {
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return rngState;
}
static int srnd(int n) { return (int)(srnd_u() % (unsigned)n); }

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Midnight", "Electric", "Velvet", "Smooth", "Liquid",
    "Dusty", "Mellow", "Concrete", "Uptown", "Hushed", "Amber", "Subway" };
static const char *TW2[] = { "Excursion", "Relaxation", "Movement", "Theory",
    "Marauder", "Vibes", "Rhythm", "Corners", "Low End", "Static", "Transit", "Hours" };

static void new_song(double pos, unsigned seed) {
    if (!seed) seed = ((unsigned)rnd(0x10000) << 16) ^ (unsigned)rnd(0x10000)
                      ^ (unsigned)frame() * 2654435761u;
    if (!seed) seed = 1;
    rngState = sng.seed = seed;

    sng.keyPc = srnd(12);
    int r = srnd(10);
    sng.loopBars = r < 3 ? 2 : (r < 7 ? 3 : 4);          // the 3-bar loop favoured
    sng.nCh = sng.loopBars * 2;                          // a chord every half bar

    // 1. write a progression with a real cadence inside it: Vsus -> I somewhere
    int home = srnd(sng.nCh);
    for (int i = 0; i < sng.nCh; i++) {
        if (i == home) { sng.off[i] = 0; sng.q[i] = Q_MAJ9; continue; }
        if (i == (home - 1 + sng.nCh) % sng.nCh && srnd(10) < 6) {
            sng.off[i] = 7; sng.q[i] = Q_SUS9; continue;  // the sus dominant
        }
        int p;
        do { p = srnd(8); } while (i > 0 && POOL_OFF[p] == sng.off[i - 1]);
        sng.off[i] = POOL_OFF[p];
        sng.q[i]   = POOL_Q[p];
    }
    // 2. drop the needle mid-phrase: rotate the loop — the tonic moves
    int rot = srnd(sng.nCh);
    int toff[8], tq[8];
    for (int i = 0; i < sng.nCh; i++) { toff[i] = sng.off[(i + rot) % sng.nCh]; tq[i] = sng.q[(i + rot) % sng.nCh]; }
    for (int i = 0; i < sng.nCh; i++) { sng.off[i] = toff[i]; sng.q[i] = tq[i]; }

    sng.kick = srnd(4);

    cellN = 0;                                           // hook lead cell
    for (int s = 0; s < 31 && cellN < 5; s += 2)
        if (srnd(100) < (s % 8 == 0 ? 25 : 35)) cellOn[cellN++] = s;
    if (cellN < 3) { cellN = 3; cellOn[0] = 2; cellOn[1] = 10; cellOn[2] = 22; }

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 88 + srnd(10);                               // 88..97
    bpm(tempo);
    songBase = (long)pos + 8;
    gvInit   = false;
    melPitch = 81;
    bassLast = 38;
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

// ── harmony lookups ───────────────────────────────────────────────────────
static int chord_idx(long s)  { return (int)((s / 8) % sng.nCh); }   // half-bar chords
static int root_pc(int ci)    { return (sng.keyPc + sng.off[ci]) % 12; }
static int sect_of(long bar)  { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }

static void chord_label(char *out, int n, int ci) {
    snprintf(out, n, "%s%s", PCNAME[root_pc(ci)], QN[sng.q[ci]]);
}

// voice-led bass (jingle.c's block, lower): nearest octave copy, G1..G2
static int bass_peek(int pc) {
    int d = ((pc - bassLast) % 12 + 18) % 12 - 6;
    int n = bassLast + d;
    while (n < 31) n += 12;
    while (n > 43) n -= 12;
    return n;
}
static int bass_near(int pc) { return bassLast = bass_peek(pc); }

// nearest-tone voice leading — fifth cart, same block
static void lead_voices(int ci) {
    int pcs[3];
    for (int k = 0; k < 3; k++) pcs[k] = (root_pc(ci) + QV[sng.q[ci]][k]) % 12;
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

// hook lead: accommodation EVERYWHERE — over a mixture loop every chord is
// "borrowed", so the melody always narrows to the chord's own tones
static int pick_mel(int ci) {
    int bestM = melPitch, bestScore = -999;
    for (int t = 0; t < 4; t++) {
        int pc = (root_pc(ci) + QT[sng.q[ci]][t]) % 12;
        for (int oct = 6; oct <= 7; oct++) {
            int m = oct * 12 + pc;
            if (m < 74 || m > 90) continue;
            int score = 10 - iabs(m - melPitch) + rnd(4);
            if (m == melPitch) score -= 3;
            if (score > bestScore) { bestScore = score; bestM = m; }
        }
    }
    melPitch = bestM;
    return bestM;
}

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = (int)((abs - pos) * stepMs);
    if (dly < 1) dly = 1;
    int  step = (int)(s % 16);
    int  s32  = (int)(s % 32);
    long bar  = s / 16;
    if (bar >= 64) return;
    int  ci   = chord_idx(s);
    int  sect = sect_of(bar);
    int  sw   = (step % 2 == 1) ? (int)(stepMs * SWING) : 0;   // swung 16ths
    int  inSlot = (int)(s % 8);                                 // step within the chord

    // BASS — the star of the record. Root on every chord, fat and forward;
    // approach runs lean +12ms with the groove.
    if (inSlot == 0 || (inSlot == 4 && chance(40)) || (inSlot == 6 && chance(45))) {
        int b = bass_near(root_pc(ci));
        int n = b, vol = 6;
        bool play = true;
        if (inSlot == 4) { n = chance(50) ? b + 7 : b + 12; vol = 4; }
        if (inSlot == 6) {                                  // chromatic run at the turn
            int nb = bass_peek(root_pc(chord_idx(s + 8)));
            play = nb != b;
            n = nb > b ? nb - 1 : nb + 1;
            vol = 3;
        }
        if (play) {
            schedule_hit(dly + PUSH_BASS + sw + rnd(5) - 2, n, I_BASS, vol, (int)(stepMs * 4.5));
            vu += vol * 0.7f;
        }
    }

    // DRUMS — crisp, layered, leaning
    if (sect != S_OUTRO || bar % 2 == 0) {                 // outro: drums thin out
        // kick holds the grid
        for (int i = 0; i < 6; i++)
            if (KICKP[sng.kick][i] == s32) {
                schedule_hit(dly + PUSH_KICK + sw + rnd(3) - 1, 38, I_KICK, 6, 95);
                vu += 2;
            }
        // snare drags — and it's LAYERED, the Q-Tip move: crack + thump as one
        if (step == 4 || step == 12) {
            int lag = dly + PUSH_SNARE + rnd(4) - 2;
            schedule_hit(lag, 62, I_SNARE, 5, 70);
            schedule_hit(lag, 55, I_KICK, 2, 40);          // the body under the crack
            vu += 2.5f;
        }
        if ((s32 == 11 || s32 == 27) && chance(18))        // ghost snare before the drag
            schedule_hit(dly + PUSH_SNARE + sw, 62, I_SNARE, 2, 35);
        // hats rush
        int hatEvery = (intensity >= 3) ? 1 : 2;
        if (step % hatEvery == 0) {
            int hv = (step % 4 == 2) ? 3 : 2;
            if (hatEvery == 1 && step % 2 == 1) hv = 1;
            int hd = dly + PUSH_HAT + sw + rnd(3) - 1;
            schedule_hit(hd < 1 ? 1 : hd, 90, I_HAT, hv, 22);
        }
        // section-end fill (performance, never seeded)
        if (bar % 8 == 7 && step >= 12 && chance(55))
            schedule_hit(dly + PUSH_SNARE, 62, I_SNARE, step == 12 ? 3 : 2 + (step - 12), 30);
    }

    // RHODES — sparse stabs on the chord changes, tremolo doing the rest
    if (intensity >= 1 && sect != S_INTRO) {
        if (inSlot == 0 && chance(75)) {
            lead_voices(ci);
            int dur = (sng.off[ci] == 0) ? 700 : rnd_between(280, 480);   // home rings
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + k * 9 + rnd(4), gv[k], I_RHODES, 3, dur);
            vu += 2;
        } else if ((inSlot == 3 || inSlot == 5) && chance(30)) {
            lead_voices(ci);
            schedule_hit(dly + sw + rnd(4), gv[rnd(3)], I_RHODES, 2, 200);
            vu += 0.8f;
        }
    }

    // HOOK LEAD — the cell, narrowed to each chord's tones
    if (intensity >= 2 && sect == S_H)
        for (int i = 0; i < cellN; i++)
            if (cellOn[i] == s32 && chance(80)) {
                int gap = (i + 1 < cellN) ? cellOn[i + 1] - s32 : 32 - s32;
                int dur = (int)(gap * stepMs * 0.8);
                if (dur > 900) dur = 900;
                schedule_hit(dly + sw + 10 + rnd(8), pick_mel(ci), I_LEAD, 3, dur);
                vu += 1.5f;
            }

    // VINYL — the dust never stops
    if (chance(10))
        schedule_hit(dly + rnd((int)stepMs), 88 + rnd(8), I_VINYL, 1, 12);
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_RHODES, INSTR_TRI, 8, 300, 3, 200);
    instrument_filter(I_RHODES, FILTER_LOW, 1600, 2);
    instrument_lfo(I_RHODES, 0, LFO_VOLUME, 4.2f, 0.10f);    // tremolo
    instrument_env(I_RHODES, 0, ENV_CUTOFF, 0, 110, 900);    // bark

    instrument(I_BASS, INSTR_SINE, 4, 220, 5, 120);          // upright-ish
    instrument_filter(I_BASS, FILTER_LOW, 480, 1);
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 35, 4);          // the thump

    instrument(I_LEAD, INSTR_TRI, 10, 180, 4, 200);
    instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.4f, 0.12f);
    instrument_filter(I_LEAD, FILTER_LOW, 2200, 2);

    instrument(I_KICK, INSTR_SINE, 0, 95, 0, 35);            // boom
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 50, 16);

    instrument(I_SNARE, INSTR_NOISE, 0, 85, 0, 40);          // bap
    instrument_filter(I_SNARE, FILTER_BAND, 1700, 7);
    instrument_env(I_SNARE, 0, ENV_PITCH, 0, 25, 14);

    instrument(I_HAT, INSTR_NOISE, 0, 18, 0, 14);
    instrument_filter(I_HAT, FILTER_HIGH, 7500, 3);

    instrument(I_VINYL, INSTR_NOISE, 0, 10, 0, 8);           // dust
    instrument_filter(I_VINYL, FILTER_HIGH, 4000, 2);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (LOWEND_SEED) { new_song(pos, LOWEND_SEED); hist[histN++] = sng.seed; histPos = 0; }
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
    if (keyp(KEY_UP)   && tempo < 104) { tempo += 2; bpm(tempo); }
    if (keyp(KEY_DOWN) && tempo > 82)  { tempo -= 2; bpm(tempo); }
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
        if (songStep >= 64L * 16) fresh_song(pos);

        long ss = songStep >= 0 ? songStep : 0;
        chord_label(nowChord[0], 12, chord_idx(ss));
        chord_label(nowChord[1], 12, (chord_idx(ss) + 1) % sng.nCh);
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    static const char *SN[4] = { "intro", "verse", "hook", "outro" };
    watch("song", "%d", songCount);
    watch("sect", "%s", SN[sect_of(ss >= 0 ? ss / 16 : 0)]);
    watch("chord", "%s", nowChord[0]);
    watch("loopBars", "%d", sng.loopBars);
#endif
}

// ── draw — the boombox under the skyline ──────────────────────────────────
static void knob(int x, int y, int r, float t, const char *label, int col) {
    circfill(x, y, r, CLR_DARK_GREY);
    circ(x, y, r, CLR_BLACK);
    float a = (-0.75f + t * 1.5f) * 3.14159f;
    line(x, y, x + (int)(sinf(a) * (r - 2)), y - (int)(cosf(a) * (r - 2)), col);
    print(label, x - text_width(label) / 2, y + r + 3, CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;

    // body — matte black box with a chrome lip
    rectfill(20, 16, 280, 168, CLR_BROWNISH_BLACK);
    rectfill(24, 20, 272, 160, CLR_DARKER_GREY);
    line(24, 22, 295, 22, CLR_LIGHT_GREY);                  // chrome
    line(24, 178, 295, 178, CLR_LIGHT_GREY);

    // dial strip
    rectfill(32, 26, 256, 18, CLR_BLACK);
    rect(32, 26, 256, 18, CLR_DARK_GREY);
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * 13;
        line(x, 38, x, 42, CLR_DARK_GREY);
        if (fq % 4 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 6, 29, CLR_MEDIUM_GREY);
        }
    }
    int nx = 36 + (int)((sng.freq - 88.0f) * 13.0f);
    line(nx, 27, nx, 43, CLR_RED);

    // the window — city skyline at night, windows lit from the seed
    rectfill(34, 52, 102, 116, CLR_DARK_BLUE);
    circfill(120, 64, 6, CLR_LIGHT_YELLOW);                 // moon
    unsigned h = sng.seed;
    int bx = 36;
    while (bx < 130) {
        h = h * 1664525u + 1013904223u;
        int bw = 10 + (int)(h % 14);
        int bh = 30 + (int)((h >> 8) % 60);
        if (bx + bw > 134) bw = 134 - bx;
        rectfill(bx, 168 - bh, bw, bh, CLR_BLACK);
        for (int wy = 168 - bh + 4; wy < 162; wy += 7)
            for (int wx = bx + 2; wx < bx + bw - 2; wx += 5) {
                unsigned ww = (unsigned)(wx * 31 + wy * 17) ^ sng.seed;
                bool lit = (ww % 5 == 0) && ((ww + (unsigned)(frame() / 90)) % 7 != 0);
                if (lit) rectfill(wx, wy, 2, 3, CLR_YELLOW);
            }
        bx += bw + 2;
    }
    rect(34, 52, 102, 116, CLR_BROWNISH_BLACK);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_DARK_GREY);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_ORANGE);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  key %s", sng.freq, PCNAME[sng.keyPc]);
        print(l2, 154, 70, CLR_DARK_ORANGE);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_DARK_ORANGE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // the loop: current chord boxed -> next, plus the loop-length badge
    if (radioOn) {
        int cw = text_width(nowChord[0]);
        rectfill(150, 102, cw + 6, 13, CLR_ORANGE);
        print(nowChord[0], 153, 105, CLR_BROWNISH_BLACK);
        print("->", 162 + cw, 106, CLR_DARK_GREY);
        print(nowChord[1], 180 + cw, 106, CLR_MEDIUM_GREY);
        char lb[16];
        snprintf(lb, 16, "%d-bar loop", sng.loopBars);
        print(lb, 152, 120, CLR_ORANGE);
        static const char *SN[4] = { "intro", "verse", "hook", "outro" };
        print(SN[sect_of(bar)], 240, 120, CLR_MEDIUM_GREY);
        for (int i = 0; i < 8; i++)
            circfill(208 + i * 7, 132, 1, i <= bar / 8 ? CLR_ORANGE : CLR_DARK_GREY);
    }

    // knobs + power LED
    static const char *FEEL[4] = { "basement", "headnod", "cypher", "banger" };
    knob(168, 148, 9, intensity / 3.0f, FEEL[intensity], CLR_ORANGE);
    knob(218, 148, 9, (tempo - 82) / 22.0f, "tempo", CLR_ORANGE);
    float vt = vu / 12.0f;
    knob(262, 148, 11, vt > 1 ? 1 : vt, "low", CLR_RED);
    circfill(282, 28, 2, radioOn && beat_pos() < 0.25f ? CLR_RED : CLR_DARK_RED);

    // help button + hint
    circfill(288, 172, 6, CLR_DARK_GREY);
    circ(288, 172, 6, CLR_BLACK);
    print("?", 285, 169, CLR_ORANGE);
    print("SPACE next song   H help", 8, 190, CLR_DARK_GREY);

    if (showHelp) {
        rectfill(44, 40, 232, 122, CLR_BLACK);
        rect(44, 40, 232, 122, CLR_ORANGE);
        print("LOW END RADIO", 52, 46, CLR_ORANGE);
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
        print("the #number on the display IS the song.", 52, 128, CLR_ORANGE);
        print("pin it for good: #define LOWEND_SEED 0x...", 52, 137, CLR_ORANGE);
        print("seeded composition, played fresh every time", 52, 146, CLR_ORANGE);
        font(FONT_NORMAL);
    }
}
