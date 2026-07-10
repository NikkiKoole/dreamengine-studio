/* de:meta
{
  "slug": "dub",
  "title": "dub radio",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "schedule-driven-agents",
    "swing-timing"
  ],
  "lineage": "Seventh radio station; King Tubby / Augustus Pablo homage. Novel: echo as scheduled future notes (not an FX bus), and a per-phrase performance-level mixing desk that re-rolls arrangements from engine RNG so the same seed mixes itself differently every listen.",
  "homage": "King Tubby / Augustus Pablo (dub)",
  "description": "Endless roots dub - the King Tubby / Augustus Pablo homage, seventh of the radio family, and the one where the engine IS the genre. ECHO AS SCHEDULED NOTES: every skank chop, rim shot and melodica phrase schedules its own decaying repeats (dotted-8th apart) - no FX engine, the delay line is just more notes in the future. THE DESK IS THE INSTRUMENT: every 4-bar phrase a performance-level mixing brain re-rolls which layers are in (bass ~92%, drums ~90%, skank, organ bubble, melodica); when the skank leaves, its ghost gets thrown into a long echo tail. The desk is engine-rnd, so the same seed MIXES ITSELF DIFFERENTLY every listen - exactly how dubplates worked. Seeded 2-bar minor-pentatonic riddim bassline, one-drop/rockers/steppers kits, dub siren at meltdown feel. Seed on display (DUB_SEED / R / [ ] history), T tone, H help. Worked example #7 for docs/guides/game-music.md."
}
de:meta */
#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading, chrome)
#include "solo.h"    // the jam layer — a scale-locked melodica over the riddim
#include <stdio.h>
#include <math.h>

// ── DUB RADIO ─────────────────────────────────────────────────────────────
// The soundsystem sibling of the radio family: endless roots dub, the King
// Tubby / Augustus Pablo homage. Seventh station, and the one where the
// engine's own machinery IS the genre:
//
//   • ECHO AS SCHEDULED NOTES *PLUS* THE BUS — the rhythm of dub echo is
//     scheduled notes (echo_hit: 2-6 taps, a dotted-8th apart, fading
//     volume; the delay line is just more notes in the future). Under them
//     sits what scheduled notes could never do: the engine's echo bus
//     (same dotted-8th time) puts a diffuse, DARKENING tail beneath every
//     tap, and a throw rides the skank's send fader hot for the phrase.
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
#define I_SOLO  13  // the jam-strip melodica — sings into the tape echo

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

static Song       sng;
static RadioSeed  rs;                       // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 208.0 };   // schedule-ahead step clock (radio.h)
// the clock's fields under their pre-migration names — keeps the body textually
// unchanged (smallest possible diff over the original)
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)    rad_srnd(&rs, (n))
static void   apply_echo_bus(void);   // defined below echo_hit (new_song needs it early)
static int    tempo     = 72;
static int    intensity = 1;     // feel: shifts the arrangement's density curve
static bool   radioOn   = true;
static bool   showHelp  = false;
static RadBand band;                 // THE BAND (B): the skank chair — guitar chop / Hammond B3
static int    chSkank;
static int    songCount = 0;
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

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "King", "Lion", "Zion", "Roots", "Echo", "Heavy",
    "Iron", "Mystic", "Channel One", "Rockfort", "Midnight", "Satta" };
static const char *TW2[] = { "Fire", "Plate", "Riddim", "Revolution", "Organizer",
    "Excursion", "Chapter", "Warrior", "Vibration", "Meltdown", "Serenade", "Rockers" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);   // 0 = derive fresh (same expression as ever)

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
    apply_echo_bus();                                    // the tape loop follows the riddim
    songBase = (long)pos + 8;
    gvInit   = false;
    melPitch = 76;
    bassLast = 36;
    dkPhrase = -1;
    songCount++;
}

static void fresh_song(double pos) {       // [ and ] walk the session history (radio.h)
    new_song(pos, 0);
    rad_hist_log(&rs);
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
// RAD_TONENAME / RAD_TONEMUL (radio.h) are the same four values dub shipped with
static int toneSel = 2;

static void apply_voicing(void) {
    float m = RAD_TONEMUL[toneSel];
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

// ── THE BUS — the diffuse tail under the taps (audio-notes §17 step 3) ────
// The scheduled taps above ARE the rhythm of dub echo and they stay. What
// they could never do is the diffuse, darkening wash a real tape loop puts
// under them — that's the engine's echo bus: same dotted-8th time, repeats
// losing brightness every pass. And the bus is PERFORMED, not just set:
// a throw cranks the feedback toward runaway for the whole phrase (fbHot),
// and sometimes the engineer grabs the tape speed mid-throw (bendF) — the
// delay time sweeps down and back while the tail rings, so the repeats
// pitch-bend. That warble is the King Tubby signature.
static bool  fbHot = false;    // feedback ridden hot through a throw phrase
static float bendF = -1.0f;    // tape-bend phase 0..1 (-1 = idle)

static void apply_echo_bus(void) {
    echo((int)(60000.0 / (tempo * 4) * 3.0), fbHot ? 0.8f : 0.45f, 0.22f);
}

// voice-led bass register, deep: G1..G2
static int bass_peek(int pc) { return rad_bass_to(pc, bassLast, 31, 43); }
static int bass_near(int pc) { return bassLast = bass_peek(pc); }

// nearest-tone voice leading — rad_lead_to (radio.h) is the shared block;
// the wrapper just resolves the chord into root + the quality's intervals.
// skank/organ register window: 57..79. NOTE: dub's pre-migration init used
// target = 62 + k*5; the shared block uses lo+5 = 57+5 = 62 — identical.
static void lead_voices(Ch c) {
    rad_lead_to(root_pc(c), QV[c.q], gv, 3, 57, 79, &gvInit);
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
    int dly = rad_step_dly(&clk, abs, pos);
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
        bool throwNow = skankWas && !dkSkank && lvl >= 1;
        if (throwNow) {
            lead_voices(c);
            for (int k = 1; k < 3; k++)
                echo_hit(dly, gv[k], I_SKANK, 5, 70, lvl >= 3 ? 6 : 4);
            vu += 3;
        }
        // the desk's hands on the bus: a throw cranks send AND feedback for
        // the phrase — the ghost rings near runaway for bars, not 4 repeats —
        // and sometimes grabs the tape speed too (the pitch-bending warble)
        instrument_echo(I_SKANK, throwNow ? 0.9f : 0.18f);
        fbHot = throwNow;
        apply_echo_bus();
        if (throwNow && lvl >= 2 && chance(50)) bendF = 0.0f;
        // the meltdown toy — siren + a tape bend, the full Tubby meltdown
        if (lvl >= 3 && chance(18)) {
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + k * (int)(stepMs * 2), 80 - k * 5, I_SIREN, 3, 240);
            bendF = 0.0f;
        }
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

    // MELODICA — far-east phrases, always swimming in echo. Unlike the lay-out
    // stations, dub TRADES: the melodica drops out only while the player is
    // holding a strip note (solo_playing) and fills the gaps when they rest —
    // and the tape echo tail bridges every handoff. Call-and-response, not silence.
    if (lvl >= 2 && dkMelo && !solo_playing())
        for (int i = 0; i < sng.cellN; i++)
            if (sng.cellOn[i] == s32 && chance(80)) {
                int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - s32 : 32 - s32;
                int dur = (int)(gap * stepMs * 0.7);
                if (dur > 700) dur = 700;
                echo_hit(dly + 8 + rnd(6), pick_mel(), I_MELO, 4, dur, 2);
                vu += 1.5f;
            }
}

// ── the skank chair — A/B them live with G ────────────────────────────────
// The offbeat chop is the reggae heartbeat, and the genre plays it on EITHER a
// choppy guitar OR a Hammond — both authentic. dub shipped the guitar (TRI +
// a fast cutoff-env upstroke). INSTR_ORGAN (shipped 2026-06-07) is the real
// organ skank: the "reggae" preset (organ.c #0 — hollow, thin, NO scanner
// motion; the upstroke chop IS the rhythm). Same evidence-gathering step as
// jangle/bossa's guitar chair: once both sound right, the song seed rolls which
// skank shows up. apply_voicing() re-aims the tone-knob filter over whichever
// engine is in the slot; the desk re-asserts the echo send every phrase, and
// setup re-applies the base send so a mid-song toggle keeps its wash.
static int skankOrgan = 0;   // 0 = TRI guitar chop (shipped), 1 = INSTR_ORGAN reggae

static void setup_skank(void) {
    if (!skankOrgan) {
        instrument(I_SKANK, INSTR_TRI, 1, 90, 0, 50);           // the choppy guitar
        instrument_env(I_SKANK, 0, ENV_CUTOFF, 0, 50, 800);     //   filtered upstroke
    } else {
        instrument(I_SKANK, INSTR_ORGAN, 1, 90, 0, 50);         // a real Hammond chop
        instrument_harmonics(I_SKANK, 0.06f);                   // "reggae" registration — hollow, thin
        instrument_timbre(I_SKANK, 0.55f);                      //   (organ.c preset 0)
        instrument_morph(I_SKANK, 0.00f);                       //   no motion — the chop is the rhythm
    }
    instrument_echo(I_SKANK, 0.18f);                            // base wash; the desk rides it on throws
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    setup_skank();                                          // the chop — A/B'd via the band panel (guitar vs Hammond)
    chSkank = rad_chair(&band, "skank", "guitar", "B3", NULL, NULL);   // the one A/B chair

    instrument(I_BASS, INSTR_SINE, 3, 260, 5, 130);          // deep and round
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 30, 3);

    instrument(I_MELO, INSTR_SQUARE, 12, 160, 5, 180);       // reedy melodica
    instrument_duty(I_MELO, 0.38f);
    instrument_lfo(I_MELO, 0, LFO_PITCH, 5.0f, 0.18f);
    instrument_echo(I_MELO, 0.25f);                          // melodica always sings into the tape

    instrument_echo(I_RIM, 0.25f);                           // rim throws carry a real tail too

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

    instrument(I_SOLO, INSTR_SQUARE, 12, 170, 6, 200);       // the jam melodica — reedy, present
    instrument_duty(I_SOLO, 0.40f);
    instrument_lfo(I_SOLO, 0, LFO_PITCH, 5.0f, 0.20f);       // the reed wobble
    instrument_filter(I_SOLO, FILTER_LOW, 2200, 2);          // a touch brighter than the comp melodica
    instrument_echo(I_SOLO, 0.30f);                          // sings into the tape (the strip rides the send)
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    // the tape bend: sweep the delay time down 60% and back over ~1.5s while
    // the tail rings — the bus's read tap glides, so the repeats pitch-bend
    // up the way a sped-up tape loop does. THE King Tubby move.
    if (bendF >= 0.0f) {
        bendF += 0.011f;
        if (bendF >= 1.0f) { bendF = -1.0f; apply_echo_bus(); }
        else {
            float warp = 1.0f - 0.6f * sinf(bendF * 3.14159f);
            echo((int)(stepMs * 3.0 * warp), fbHot ? 0.8f : 0.45f, 0.22f);
        }
    }

    if (!booted) {
        setup_instruments();
        apply_voicing();
        if (DUB_SEED) { new_song(pos, DUB_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    // the shared input block (radio.h): feel/tempo/tone/help handled inside,
    // the cart reacts to the events. ntone=4 — dub has the brightness knob.
    int ev = rad_input(&tempo, 60, 88, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_TEMPO)  apply_echo_bus();          // the tape loop follows the tempo
    if (ev & RAD_EV_TONE)   apply_voicing();           // re-aim the filters live
    int chair = rad_band_input(&band, &showHelp);          // THE BAND (B) — A/B the skank chair
    if (chair == chSkank) { skankOrgan = band.c[chSkank].sel; setup_skank(); apply_voicing(); }  // mid-song
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) note_off_all();
        else scheduled = (long)pos;
    }

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

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
    watch("bus", "fb%s bend%.2f", fbHot ? "HOT" : "0.45", bendF);
#endif
}

// ── draw — the soundsystem stack (shared chassis from radio.h; the window art
// — the speaker stack breathing with the VU, the desk faders — stays dub's) ──
void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();                                         // knobs are touch-draggable
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;

    rad_body(CLR_BLACK, CLR_MEDIUM_GREEN);   // road-case black, soundsystem green
    // dub's own identity — the Rasta tricolor stripe, painted over the pinstripe
    rectfill(24, 20, 272, 3, CLR_RED);
    rectfill(24, 23, 272, 3, CLR_YELLOW);
    rectfill(24, 26, 272, 3, CLR_MEDIUM_GREEN);
    rad_dial(sng.freq, CLR_MEDIUM_GREEN);

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
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_MEDIUM_GREEN);
    if (rad_knob_int(&tempo, 60, 88, 2, 218, 148, 9, "tempo", CLR_MEDIUM_GREEN)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_MEDIUM_GREEN)) apply_voicing();
    rad_power_led(radioOn, CLR_RED, CLR_DARK_RED);

    rad_help_button(CLR_MEDIUM_GREEN);
    rad_band_button(CLR_MEDIUM_GREEN);

    if (showHelp) {
        static const char *HELP[10][2] = {
            { "SPACE",      "next dubplate (rolls a new seed)" },
            { "R",          "same plate - the desk mixes anew" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - shifts the density curve" },
            { "UP/DOWN",    "tempo of this riddim" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "B",          "the band - swap the skank timbre" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
            { "L",          "jam ch/sc: chord-lock / free" },
        };
        static const char *NOTES[3] = {
            "the #number IS the riddim; the MIX is live -",
            "the desk re-rides the faders every listen.",
            "pin it: #define DUB_SEED 0x...",
        };
        rad_help_panel("DUB RADIO", HELP, 10, NOTES, 3, CLR_MEDIUM_GREEN);
    }
    rad_band_panel(&band, CLR_MEDIUM_GREEN);

    // the jam strip — a melodica on the riddim's minor pentatonic, the vamp
    // chord's tones lit. vertical RIDES THE ECHO SEND: lean up and the note
    // drenches into the tape (J or tap the corner)
    int chord[4]; {
        Ch c = chord_at(bar);
        chord[0] = root_pc(c);
        for (int k = 0; k < 3; k++) chord[k + 1] = (root_pc(c) + QV[c.q][k]) % 12;
    }
    SoloCtx jc = { sng.keyPc, PENT, 5, chord, 4, I_SOLO, 64, 84, false, SOLO_Y_SEND, 0.12f, 0.9f };
    solo_strip(&jc, 28, 170, 250, 18, CLR_MEDIUM_GREEN);

    ui_end();
}
