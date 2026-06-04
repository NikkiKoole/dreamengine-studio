#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── DUB RADIO ─────────────────────────────────────────────────────────────
// The soundsystem sibling of the radio family: endless roots dub, the King
// Tubby / Augustus Pablo homage. Seventh station, and the one where the
// engine's own machinery IS the genre:
//
//   • ECHO AS SCHEDULED NOTES — dub's defining effect needs no FX engine:
//     every skank chop, rim shot and melodica note schedules its own decaying
//     repeats (echo_hit: 2-6 taps, a dotted-8th apart, fading volume). The
//     delay line is just more notes in the future.
//   • THE DESK IS THE INSTRUMENT — a dub track is one riddim plus a mixing
//     engineer riding the faders. Every 4-bar phrase, the desk re-rolls which
//     layers are in (bass ~92%, drums ~90%, skank, organ, melodica). When the
//     skank LEAVES, its last chop gets thrown into a long echo tail — the
//     signature dub move. The desk is PERFORMANCE (engine rnd), so the same
//     seed mixes itself differently every playthrough, like a real dubplate.
//   • THE RIDDIM — harmony is 1-2 minor chords (i, iv, bVII); the real hook
//     is a seeded 2-bar BASSLINE in minor pentatonic, deep and full of rests,
//     transposed to each chord. The bassline is the song.
//   • one-drop / rockers / steppers — the three kits, rolled per song. In a
//     one-drop, beat ONE is empty. That emptiness is the genre.
//
// Density = arrangement curve x feel shift (game-music.md): the desk drops
// layers WITHIN what the curve allows. Tempo 66-80, head-nod deep.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help
//
// Seed pins the COMPOSITION (key, vamp, riddim bassline, kit, melodica cell,
// title); the PERFORMANCE (the desk rides, echo throws, fills, sirens)
// re-rolls every playthrough — every listen is a different mix of the same
// dubplate. That is exactly how dub worked.

#define DUB_SEED 0   // pin a favourite dubplate here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_SKANK 5   // the offbeat chop
#define I_BASS  6   // the riddim — deepest thing on the dial
#define I_MELO  7   // melodica (echoed)
#define I_ORG   8   // organ bubble
#define I_KICK  9
#define I_RIM   10
#define I_HAT   11
#define I_SIREN 12  // the meltdown toy

// ── harmony: minor vamps, 4-bar cycles ────────────────────────────────────
enum { Q_MIN7, Q_MAJ, NQ };
static const char *QN[NQ] = { "m7", "" };
static const int QV[NQ][3] = { { 3, 10, 14 }, { 4, 7, 11 } };
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
static const int PENT[5] = { 0, 3, 5, 7, 10 };   // minor pentatonic — the riddim's alphabet

typedef struct { int off, q; } Ch;
static const Ch VAMPS[4][4] = {     // one chord per bar, 4-bar cycle
    { { 0, Q_MIN7 }, { 0, Q_MIN7 }, { 0, Q_MIN7 }, { 0, Q_MIN7 } },   // pure one-chord
    { { 0, Q_MIN7 }, { 0, Q_MIN7 }, { 5, Q_MIN7 }, { 0, Q_MIN7 } },   // i i iv i
    { { 0, Q_MIN7 }, { 10, Q_MAJ }, { 0, Q_MIN7 }, { 10, Q_MAJ } },   // i bVII i bVII
    { { 0, Q_MIN7 }, { 5, Q_MIN7 }, { 0, Q_MIN7 }, { 10, Q_MAJ } },   // i iv i bVII
};

// the three kits (16 steps per bar)
enum { K_ONEDROP, K_ROCKERS, K_STEPPERS };
static const char *KITNAME[3] = { "one drop", "rockers", "steppers" };

// the form: intro/outro thin, "version" sections get the heavy desk treatment
enum { S_INTRO, S_RID, S_VER, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_RID, S_RID, S_VER, S_RID, S_VER, S_VER, S_OUTRO };

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    int  vamp, kit;
    int  dblSkank;            // skank also chops the off-8ths
    int  rdOn[6], rdDeg[6], rdN;   // the riddim bassline: onsets (32-step grid) + pent degrees
    int  cellOn[5], cellN;    // melodica cell
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song   sng;
static int    tempo     = 72;
static int    intensity = 1;     // feel: shifts the arrangement's density curve
static bool   radioOn   = true;
static bool   showHelp  = false;
static long   scheduled = -1;
static long   songBase  = 0;
static int    songCount = 0;
static double stepMs    = 208.0;
static int    gv[3]     = { 64, 67, 71 };
static bool   gvInit    = false;
static float  vu        = 0;
static int    melPitch  = 76;
static int    bassLast  = 36;
static char   nowChord[4][8];

// THE DESK — performance state, re-rolled every 4-bar phrase
static bool dkBass = true, dkDrums = true, dkSkank = true, dkOrg = false, dkMelo = false;
static long dkPhrase = -1;

static int iabs(int v) { return v < 0 ? -v : v; }

// composition PRNG (xorshift32) — same contract as the other radios
static unsigned rngState = 1;
static unsigned srnd_u(void) {
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return rngState;
}
static int srnd(int n) { return (int)(srnd_u() % (unsigned)n); }

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "King", "Lion", "Zion", "Roots", "Echo", "Heavy",
    "Iron", "Mystic", "Channel One", "Rockfort", "Midnight", "Satta" };
static const char *TW2[] = { "Fire", "Plate", "Riddim", "Revolution", "Organizer",
    "Excursion", "Chapter", "Warrior", "Vibration", "Meltdown", "Serenade", "Rockers" };

static void new_song(double pos, unsigned seed) {
    if (!seed) seed = ((unsigned)rnd(0x10000) << 16) ^ (unsigned)rnd(0x10000)
                      ^ (unsigned)frame() * 2654435761u;
    if (!seed) seed = 1;
    rngState = sng.seed = seed;

    sng.keyPc    = srnd(12);
    sng.vamp     = srnd(4);
    sng.kit      = srnd(3);
    sng.dblSkank = srnd(10) < 4;

    // the riddim: a 2-bar bassline. root on the one, then 3-5 more onsets on
    // the 8th grid, heavy on root/fifth/b7, full of rests. THE hook.
    sng.rdOn[0] = 0; sng.rdDeg[0] = 0; sng.rdN = 1;
    static const int CAND[9] = { 4, 6, 10, 12, 16, 20, 22, 26, 28 };
    static const int DEGW[8] = { 0, 0, 0, 3, 3, 1, 2, 4 };   // weighted pent degrees
    int want = 3 + srnd(3);
    for (int i = 0; i < 9 && sng.rdN < 1 + want; i++)
        if (srnd(100) < 45) {
            sng.rdOn[sng.rdN]  = CAND[i];
            sng.rdDeg[sng.rdN] = DEGW[srnd(8)];
            sng.rdN++;
        }
    if (sng.rdN < 3) {                                   // fallback riddim
        sng.rdN = 4;
        sng.rdOn[1] = 6;  sng.rdDeg[1] = 0;
        sng.rdOn[2] = 16; sng.rdDeg[2] = 4;
        sng.rdOn[3] = 22; sng.rdDeg[3] = 3;
    }

    sng.cellN = 0;                                       // melodica cell, sparse
    for (int s = 0; s < 31 && sng.cellN < 4; s += 2)
        if (srnd(100) < 22) sng.cellOn[sng.cellN++] = s;
    if (sng.cellN < 2) { sng.cellN = 2; sng.cellOn[0] = 4; sng.cellOn[1] = 18; }

    if (srnd(100) < 70) snprintf(sng.title, sizeof sng.title, "%s Dub", TW1[srnd(12)]);
    else                snprintf(sng.title, sizeof sng.title, "Dub %s", TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 66 + srnd(15);                               // 66..80
    bpm(tempo);
    songBase = (long)pos + 8;
    gvInit   = false;
    melPitch = 76;
    bassLast = 36;
    dkPhrase = -1;
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
static Ch  chord_at(long bar) { return VAMPS[sng.vamp][bar % 4]; }
static int root_pc(Ch c)      { return (sng.keyPc + c.off) % 12; }

static void chord_label(char *out, int n, Ch c) {
    snprintf(out, n, "%s%s", PCNAME[root_pc(c)], QN[c.q]);
}

// density = arrangement curve + feel shift; the desk rides WITHIN this
//   0: bass + drums          2: + organ bubble and melodica
//   1: + the skank           3: + heavier echo, throws, the siren
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_RID ? 1 : 2);
    int lvl = base + intensity - 1;
    return lvl < 0 ? 0 : lvl > 3 ? 3 : lvl;
}

// ── the tone knob (T cycles) — master brightness, re-issued live ──────────
static int toneSel = 2;
static const char *TONENAME[4] = { "mellow", "warm", "clear", "bright" };
static const float TONEMUL[4]  = { 0.55f, 0.78f, 1.0f, 1.28f };

static void apply_voicing(void) {
    float m = TONEMUL[toneSel];
    instrument_filter(I_SKANK, FILTER_LOW, (int)(1500 * m), 2);
    instrument_filter(I_MELO,  FILTER_LOW, (int)(1700 * m), 2);
    instrument_filter(I_ORG,   FILTER_LOW, (int)(1100 * m), 2);
    instrument_filter(I_BASS,  FILTER_LOW, (int)(420 * (0.75f + 0.25f * m)), 1);
}

// ── ECHO — dub's whole identity, as scheduled notes ───────────────────────
// taps repeats, a dotted-8th apart (3 sixteenth-steps), each one quieter.
// Every tap occupies a slot in the engine's 64-deep delayed pen, so keep
// taps modest — the throws (5-6) are the ceiling.
static void echo_hit(int dly, int midi, int instr, int vol, int dur, int taps) {
    int t = (int)(stepMs * 3);                          // the dub delay time
    schedule_hit(dly, midi, instr, vol, dur);
    for (int k = 1; k <= taps; k++) {
        int v = vol - 1 - k;
        if (v <= 0) break;
        schedule_hit(dly + k * t, midi, instr, v, dur);
    }
}

// voice-led bass register, deep: G1..G2
static int bass_peek(int pc) {
    int d = ((pc - bassLast) % 12 + 18) % 12 - 6;
    int n = bassLast + d;
    while (n < 31) n += 12;
    while (n > 43) n -= 12;
    return n;
}
static int bass_near(int pc) { return bassLast = bass_peek(pc); }

// nearest-tone voice leading — seventh cart, same block
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
        while (gv[k] > 79) gv[k] -= 12;
    }
}

// melodica: minor pentatonic of the key, nearest the walker
static int pick_mel(void) {
    int bestM = melPitch, bestScore = -999;
    for (int d = 0; d < 5; d++) {
        int pc = (sng.keyPc + PENT[d]) % 12;
        for (int oct = 5; oct <= 7; oct++) {
            int m = oct * 12 + pc;
            if (m < 69 || m > 86) continue;
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
    Ch   c    = chord_at(bar);
    int  lvl  = level_of(bar);

    // THE DESK — re-roll the mix every 4-bar phrase (performance, never seed)
    if (step == 0 && bar % 4 == 0 && bar / 4 != dkPhrase) {
        dkPhrase = bar / 4;
        bool skankWas = dkSkank;
        dkBass  = chance(92);
        dkDrums = chance(90);
        dkSkank = chance(72);
        dkOrg   = chance(50);
        dkMelo  = chance(lvl >= 3 ? 55 : 35);
        if (!dkBass && !dkDrums) dkDrums = true;        // someone holds the floor
        // the THROW: the skank leaves and its ghost echoes into the dark
        if (skankWas && !dkSkank && lvl >= 1) {
            lead_voices(c);
            for (int k = 1; k < 3; k++)
                echo_hit(dly, gv[k], I_SKANK, 5, 70, lvl >= 3 ? 6 : 4);
            vu += 3;
        }
        // the meltdown toy
        if (lvl >= 3 && chance(18))
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + k * (int)(stepMs * 2), 80 - k * 5, I_SIREN, 3, 240);
    }

    // BASS — the riddim, transposed to the chord. The deepest thing here.
    if (dkBass)
        for (int i = 0; i < sng.rdN; i++)
            if (sng.rdOn[i] == s32) {
                int b = bass_near(root_pc(c));
                int n = b + PENT[sng.rdDeg[i]];
                while (n > 45) n -= 12;
                int gap = (i + 1 < sng.rdN) ? sng.rdOn[i + 1] - s32 : 32 - s32;
                int dur = (int)(gap * stepMs * 0.85);
                if (dur > 1100) dur = 1100;
                schedule_hit(dly + rnd(3), n, I_BASS, 6, dur);
                vu += 3;
            }

    // DRUMS — one drop / rockers / steppers. In a one-drop, the ONE is empty.
    if (dkDrums) {
        bool kick = (sng.kit == K_ONEDROP)  ? (step == 8)
                  : (sng.kit == K_ROCKERS)  ? (step == 0 || step == 8)
                  :                           (step % 4 == 0);
        if (kick) { schedule_hit(dly + rnd(3), 36, I_KICK, 6, 110); vu += 2; }
        if (step == 8) {                                 // the drop: kick + rim together
            int rd = dly + rnd(3);
            schedule_hit(rd, 70, I_RIM, 4, 40);
            if (lvl >= 2 && chance(25)) echo_hit(rd, 70, I_RIM, 4, 40, 3);   // rim throw
            vu += 1.5f;
        }
        if (step % 2 == 0 && step != 8)
            schedule_hit(dly + rnd(3), 88, I_HAT, (step % 4 == 2) ? 2 : 1, 20);
        if (step == 14 && chance(18))                    // open hat breath
            schedule_hit(dly + rnd(3), 84, I_HAT, 3, 110);
        if (bar % 8 == 7 && step >= 12 && chance(45))    // tumbling fill into the phrase
            schedule_hit(dly + rnd(4), 64 - (step - 12) * 3, I_RIM, 2 + (step - 12) / 2, 35);
    }

    // SKANK — the offbeat chop, the reggae heartbeat (echoed when it's hot)
    if (lvl >= 1 && dkSkank) {
        bool chop = (step == 4 || step == 12);
        bool ghost = sng.dblSkank && (step == 6 || step == 14);
        if (chop || ghost) {
            lead_voices(c);
            int taps = (lvl >= 3 && chop && chance(30)) ? 3 : 0;
            for (int k = 1; k < 3; k++)
                echo_hit(dly + rnd(3), gv[k], I_SKANK, ghost ? 2 : 4, 70, taps);
            vu += ghost ? 0.6f : 1.5f;
        }
    }

    // ORGAN BUBBLE — quiet off-16th gurgle under the skank
    if (lvl >= 2 && dkOrg && (step == 2 || step == 6 || step == 10 || step == 14)) {
        lead_voices(c);
        schedule_hit(dly + rnd(3), gv[step % 4 == 2 ? 0 : 1] - 12, I_ORG, 2, 60);
        vu += 0.4f;
    }

    // MELODICA — far-east phrases, always swimming in echo
    if (lvl >= 2 && dkMelo)
        for (int i = 0; i < sng.cellN; i++)
            if (sng.cellOn[i] == s32 && chance(80)) {
                int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - s32 : 32 - s32;
                int dur = (int)(gap * stepMs * 0.7);
                if (dur > 700) dur = 700;
                echo_hit(dly + 8 + rnd(6), pick_mel(), I_MELO, 4, dur, 2);
                vu += 1.5f;
            }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_SKANK, INSTR_TRI, 1, 90, 0, 50);            // the chop
    instrument_env(I_SKANK, 0, ENV_CUTOFF, 0, 50, 800);

    instrument(I_BASS, INSTR_SINE, 3, 260, 5, 130);          // deep and round
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 30, 3);

    instrument(I_MELO, INSTR_SQUARE, 12, 160, 5, 180);       // reedy melodica
    instrument_duty(I_MELO, 0.38f);
    instrument_lfo(I_MELO, 0, LFO_PITCH, 5.0f, 0.18f);

    instrument(I_ORG, INSTR_TRI, 4, 70, 3, 50);              // the bubble
    instrument_lfo(I_ORG, 0, LFO_VOLUME, 6.5f, 0.12f);

    instrument(I_KICK, INSTR_SINE, 0, 110, 0, 45);           // soft and deep
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 55, 12);

    instrument(I_RIM, INSTR_NOISE, 0, 30, 0, 25);            // cross-stick crack
    instrument_filter(I_RIM, FILTER_BAND, 1600, 7);
    instrument_env(I_RIM, 0, ENV_PITCH, 0, 20, 16);

    instrument(I_HAT, INSTR_NOISE, 0, 14, 2, 60);
    instrument_filter(I_HAT, FILTER_HIGH, 7500, 2);

    instrument(I_SIREN, INSTR_SQUARE, 5, 200, 4, 150);       // the meltdown toy
    instrument_duty(I_SIREN, 0.25f);
    instrument_env(I_SIREN, 0, ENV_PITCH, 0, 220, 12);
    instrument_filter(I_SIREN, FILTER_LOW, 1800, 4);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        apply_voicing();
        if (DUB_SEED) { new_song(pos, DUB_SEED); hist[histN++] = sng.seed; histPos = 0; }
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
    if (keyp(KEY_UP)   && tempo < 88) { tempo += 2; bpm(tempo); }
    if (keyp(KEY_DOWN) && tempo > 60) { tempo -= 2; bpm(tempo); }
    if (keyp('T')) { toneSel = (toneSel + 1) % 4; apply_voicing(); }
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

        long bar = songStep >= 0 ? songStep / 16 : 0;
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 8, VAMPS[sng.vamp][i]);
        (void)bar;
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long bar = ss >= 0 ? ss / 16 : 0;
    static const char *SN[4] = { "intro", "riddim", "version", "outro" };
    watch("song", "%d", songCount);
    watch("sect", "%s", SN[sect_of(bar)]);
    watch("chord", "%s", nowChord[(int)(bar % 4)]);
    watch("desk", "%d%d%d%d%d", dkBass, dkDrums, dkSkank, dkOrg, dkMelo);
#endif
}

// ── draw — the soundsystem stack ──────────────────────────────────────────
static void knob(int x, int y, int r, float t, const char *label, int col) {
    circfill(x, y, r, CLR_DARK_GREY);
    circ(x, y, r, CLR_BLACK);
    float a = (-0.75f + t * 1.5f) * 3.14159f;
    line(x, y, x + (int)(sinf(a) * (r - 2)), y - (int)(cosf(a) * (r - 2)), col);
    print(label, x - text_width(label) / 2, y + r + 3, CLR_LIGHT_GREY);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;

    // body — road-case black with the tricolor stripe
    rectfill(20, 16, 280, 168, CLR_BLACK);
    rectfill(24, 20, 272, 160, CLR_DARKER_GREY);
    rectfill(24, 20, 272, 3, CLR_RED);
    rectfill(24, 23, 272, 3, CLR_YELLOW);
    rectfill(24, 26, 272, 3, CLR_MEDIUM_GREEN);

    // dial strip
    rectfill(32, 31, 256, 13, CLR_BLACK);
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * 13;
        line(x, 38, x, 42, CLR_DARK_GREY);
        if (fq % 4 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 6, 32, CLR_MEDIUM_GREEN);
        }
    }
    int nx = 36 + (int)((sng.freq - 88.0f) * 13.0f);
    line(nx, 32, nx, 43, CLR_RED);

    // the window — the speaker stack, breathing with the vu
    rectfill(34, 52, 102, 116, CLR_BLACK);
    float breathe = vu / 12.0f;
    for (int row = 0; row < 2; row++)
        for (int col2 = 0; col2 < 2; col2++) {
            int cx = 40 + col2 * 50, cy = 58 + row * 56;
            rectfill(cx, cy, 44, 50, CLR_DARKER_GREY);     // cabinet
            rect(cx, cy, 44, 50, CLR_DARK_GREY);
            int r = 14 + (int)(breathe * 3) + ((row + col2) % 2);
            circfill(cx + 22, cy + 25, r, CLR_BLACK);      // cone
            circ(cx + 22, cy + 25, r, CLR_DARK_GREY);
            circfill(cx + 22, cy + 25, 4 + (int)(breathe * 2), CLR_DARK_GREY);
        }
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_MEDIUM_GREEN);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_MEDIUM_GREEN);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s", sng.freq, KITNAME[sng.kit]);
        print(l2, 154, 70, CLR_DARK_GREEN);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_DARK_GREEN);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_MEDIUM_GREEN);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // the vamp + the desk readout (which faders are up RIGHT NOW)
    if (radioOn) {
        int ci = (int)(bar % 4);
        int x = 152;
        for (int i = 0; i < 4; i++) {
            int cw = text_width(nowChord[i]);
            if (x + cw > 292) break;
            if (i == ci) {
                rectfill(x - 2, 102, cw + 4, 12, CLR_MEDIUM_GREEN);
                print(nowChord[i], x, 104, CLR_BLACK);
            } else
                print(nowChord[i], x, 104, CLR_DARK_GREY);
            x += cw + 10;
        }
        // the desk: five little faders
        static const char *FD[5] = { "bas", "drm", "skk", "org", "mel" };
        bool on[5] = { dkBass, dkDrums, dkSkank, dkOrg, dkMelo };
        for (int i = 0; i < 5; i++) {
            int fx = 152 + i * 27;
            print(FD[i], fx, 126, on[i] ? CLR_MEDIUM_GREEN : CLR_DARKER_GREY);
            rectfill(fx + 2, 120, 3, 5, on[i] ? CLR_YELLOW : CLR_DARKER_GREY);
        }
    }

    // knobs + power LED
    static const char *FEEL[4] = { "skeleton", "riddim", "version", "meltdown" };
    knob(168, 148, 9, intensity / 3.0f, FEEL[intensity], CLR_MEDIUM_GREEN);
    knob(218, 148, 9, (tempo - 60) / 28.0f, "tempo", CLR_MEDIUM_GREEN);
    knob(262, 148, 11, toneSel / 3.0f, TONENAME[toneSel], CLR_MEDIUM_GREEN);
    circfill(282, 28, 2, radioOn && beat_pos() < 0.25f ? CLR_RED : CLR_DARK_RED);

    // help button + hint
    circfill(288, 172, 6, CLR_DARKER_GREY);
    circ(288, 172, 6, CLR_BLACK);
    print("?", 285, 169, CLR_MEDIUM_GREEN);
    print("SPACE next song   H help", 8, 190, CLR_DARK_GREY);

    if (showHelp) {
        rectfill(44, 40, 232, 122, CLR_BLACK);
        rect(44, 40, 232, 122, CLR_MEDIUM_GREEN);
        print("DUB RADIO", 52, 46, CLR_MEDIUM_GREEN);
        font(FONT_SMALL);
        static const char *HELP[8][2] = {
            { "SPACE",      "next dubplate (rolls a new seed)" },
            { "R",          "same plate - the desk mixes anew" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - shifts the density curve" },
            { "UP/DOWN",    "tempo of this riddim" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
        };
        for (int i = 0; i < 8; i++) {
            print(HELP[i][0], 52, 58 + i * 9, CLR_YELLOW);
            print(HELP[i][1], 106, 58 + i * 9, CLR_WHITE);
        }
        print("the #number IS the riddim; the MIX is live -", 52, 132, CLR_MEDIUM_GREEN);
        print("the desk re-rides the faders every listen.", 52, 141, CLR_MEDIUM_GREEN);
        print("pin it: #define DUB_SEED 0x...", 52, 150, CLR_MEDIUM_GREEN);
        font(FONT_NORMAL);
    }
}
