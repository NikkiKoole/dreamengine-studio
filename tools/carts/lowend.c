/* de:meta
{
  "title": "low end radio",
  "status": "active",
  "created": "2026-06-04",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "chord-voicing",
    "swing-timing",
    "drum-synthesis"
  ],
  "lineage": "A Tribe Called Quest / Low End Theory homage; introduces chord brain #5 (modal-mixture maj7/maj9 pool with crate-digger loop rotation so the tonic drifts), a three-tier groove pocket (tight/swung/drunk ms offsets), and Q-Tip layered snare hits (noise crack + sine thump fired as one).",
  "homage": "A Tribe Called Quest - The Low End Theory",
  "description": "Endless jazz-rap boom bap - the A Tribe Called Quest / Low End Theory homage, fifth of the radio family. Chord brain #5, THE SAMPLED LOOP: generates a lush modal-mixture soul progression (borrowed bIII/bVI/bVII voiced maj7/maj9, a 9sus dominant, the bVI as maj9#11 - per the r/musictheory Electric Relaxation analysis), then cuts a 2/3/4-bar loop (3 favoured) and ROTATES it like a crate-digger dropping the needle mid-phrase, so the tonic moves. Upright-ish bass front and center leaning +12ms, full groove template (hats rush -8ms, snare drags +22ms, 16ths swung 57%), Q-Tip layered snare (noise crack + sine thump as one hit), tremolo rhodes stabs, vinyl dust, hook lead narrowed to each chord's tones. No tempo wobble - samplers don't drift. Seed on display (LOWEND_SEED / R / [ ] history), H help. Worked example #5 for docs/guides/game-music.md."
}
de:meta */
#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading, chrome)
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
//    together), and the groove template from docs/guides/game-music.md as a
//    ROLLED POCKET (tight / swung / drunk — the seed picks how hard the snare
//    drags and the 16ths swing; the old "half strength" is now just the middle
//    setting). WITHIN a song the machine is steady and the players lean — the
//    tempo only changes BETWEEN songs, coupled to the pocket (drunk drags slow).
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (layers)   UP/DOWN tempo   H or ? help
//
// Seed pins the COMPOSITION (key, progression, loop cut+rotation, drum
// pattern, the POCKET, the FORM — incl. whether there's a drums-only BREAK —
// and the title); the PERFORMANCE (ghost notes, fills, the lead's path,
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

// ── the pocket — THE groove axis, rolled per song (ms offsets added to dly) ─
// The kick always IS the grid (0). What the seed rolls is how hard everyone
// ELSE leans off it: hats rush early, the snare drags late, the bass leans
// toward the snare's time, the odd 16ths swing. "swung" is the shipped
// half-strength head-nod; "tight" is Premier-crisp on the grid; "drunk" is the
// guide's full Dilla drag (which read as stumbling at radio volume on its own,
// but as one option among three it earns its place).
enum { P_TIGHT, P_SWUNG, P_DRUNK, NP };
static const char *PNAME[NP] = { "tight", "swung", "drunk" };
static const struct { int hat, snare, bass; float swing; } POCKET[NP] = {
    { -2,  3,  2, 0.02f },   // tight — crisp, barely off the grid
    { -5, 12,  7, 0.08f },   // swung — the shipped head-nod lean
    { -7, 22, 12, 0.14f },   // drunk — the full behind-the-beat Dilla drag
};
#define PUSH_KICK 0          // the kick holds the grid in every pocket

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

// the form — 8-bar sections; the seed rolls one of three so a tune isn't
// forever the same 64 bars. The "cut" form drops a drums-only BREAK in the
// middle — the b-boy break, the bars that made these records sampleable
// (game-music's KPM note): bass/rhodes/lead all drop out, just the kit nods
// for 8 bars, then the section-end fill lifts back in. (The chord loop length
// still usually doesn't divide 8, so the sample rolls over section lines —
// that's authentic.)
enum { S_INTRO, S_V, S_H, S_BREAK, S_OUTRO };
#define MAXSECT 12
static const struct { int n; int s[MAXSECT]; const char *name; } FORMS[] = {
    { 6, { S_INTRO, S_V, S_H, S_V, S_H, S_OUTRO },                          "tape" },  // 48 bars
    { 8, { S_INTRO, S_V, S_V, S_H, S_V, S_V, S_H, S_OUTRO },                "loop" },  // 64, the classic
    { 9, { S_INTRO, S_V, S_V, S_H, S_BREAK, S_V, S_H, S_V, S_OUTRO },       "cut"  },  // 72, with the break
};
#define NFORMS 3

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    int  off[8], q[8], nCh;   // the loop: 4/6/8 chords, one per half-bar
    int  loopBars;            // 2, 3 (favoured), or 4
    int  kick;                // pattern index
    int  pocket;              // the rolled feel — tight / swung / drunk
    int  form;                // the rolled arrangement — tape / loop / cut(break)
    char title[24];
    float freq;
    unsigned seed;
} Song;

// composition PRNG + session history live in radio.h (RadioSeed rs); srnd is the
// SAME xorshift stream as before the migration — pinned seeds depend on it byte
// for byte. performance jitter keeps engine rnd().
static Song       sng;
static RadioSeed  rs;                       // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 163.0 };   // schedule-ahead step clock (radio.h)
// the clock's fields under their pre-migration names — keeps the body textually
// unchanged (smallest possible diff over the original)
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)   rad_srnd(&rs, (n))
static int    tempo     = 92;
static int    intensity = 1;     // feel: shifts the arrangement's density curve (headnod = as composed)
static bool   radioOn   = true;
static bool   showHelp  = false;
static RadBand band;                 // THE BAND (B): the lead chair — TRI hook / vibraphone
static int    chLead;
static int    songCount = 0;
static int    gv[3]     = { 64, 67, 71 };
static bool   gvInit    = false;
static float  vu        = 0;
static int    melPitch  = 81;
static int    bassLast  = 38;
static int    cellOn[5], cellN = 0;
static char   nowChord[2][12];   // current / next
static int    toneSel   = 0;     // lowend has no tone knob — rad_input gets ntone=0

static int iabs(int v) { return v < 0 ? -v : v; }

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Midnight", "Electric", "Velvet", "Smooth", "Liquid",
    "Dusty", "Mellow", "Concrete", "Uptown", "Hushed", "Amber", "Subway" };
static const char *TW2[] = { "Excursion", "Relaxation", "Movement", "Theory",
    "Marauder", "Vibes", "Rhythm", "Corners", "Low End", "Static", "Transit", "Hours" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);   // 0 = derive fresh (same expression as ever)

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

    // the two structural axes: the pocket (how hard everyone leans) and the form
    sng.pocket = srnd(NP);                               // tight / swung / drunk
    int fr = srnd(100);
    sng.form = fr < 30 ? 0 : fr < 75 ? 1 : 2;            // 30% tape · 45% loop · 25% cut(break)

    cellN = 0;                                           // hook lead cell
    for (int s = 0; s < 31 && cellN < 5; s += 2)
        if (srnd(100) < (s % 8 == 0 ? 25 : 35)) cellOn[cellN++] = s;
    if (cellN < 3) { cellN = 3; cellOn[0] = 2; cellOn[1] = 10; cellOn[2] = 22; }

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    // tempo follows the pocket — drunk drags slow, tight snaps faster
    static const int TLO[NP] = { 90, 86, 82 }, TSPAN[NP] = { 11, 10, 9 };
    tempo = TLO[sng.pocket] + srnd(TSPAN[sng.pocket]);   // 90..100 / 86..95 / 82..90
    bpm(tempo);
    songBase = (long)pos + 8;
    gvInit   = false;
    melPitch = 81;
    bassLast = 38;
    songCount++;
}

static void fresh_song(double pos) {       // [ and ] walk the session history (radio.h)
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── harmony lookups ───────────────────────────────────────────────────────
static int chord_idx(long s)  { return (int)((s / 8) % sng.nCh); }   // half-bar chords
static int root_pc(int ci)    { return (sng.keyPc + sng.off[ci]) % 12; }
static long song_bars(void)   { return (long)FORMS[sng.form].n * 8; }
static int  form_sects(void)  { return FORMS[sng.form].n; }
static int  sect_of(long bar) {
    int x = (int)(bar / 8), n = FORMS[sng.form].n;
    return x < n ? FORMS[sng.form].s[x] : S_OUTRO;
}

// density = arrangement curve + feel shift, two separate dimensions: the hook
// stays fuller than the verse at ANY knob position; the knob moves the whole
// envelope. (Bass and drums are the foundation — they never leave.)
//   lvl 0: bass + drums only   lvl 2: + the hook lead
//   lvl 1: + rhodes stabs      lvl 3: + 16th hats, busier ghosts
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_V ? 1 : 2);
    int lvl = base + intensity - 1;
    return lvl < 0 ? 0 : lvl > 3 ? 3 : lvl;
}

static void chord_label(char *out, int n, int ci) {
    snprintf(out, n, "%s%s", PCNAME[root_pc(ci)], QN[sng.q[ci]]);
}

// voice-led bass (jingle.c's block, lower): nearest octave copy, G1..G2
static int bass_peek(int pc) { return rad_bass_to(pc, bassLast, 31, 43); }
static int bass_near(int pc) { return bassLast = bass_peek(pc); }

// nearest-tone voice leading — rad_lead_to (radio.h) is the shared block, the
// single biggest "sounds composed" trick. rhodes register window: 57..81.
static void lead_voices(int ci) {
    rad_lead_to(root_pc(ci), QV[sng.q[ci]], gv, 3, 57, 81, &gvInit);
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
    int dly = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    int  s32  = (int)(s % 32);
    long bar  = s / 16;
    if (bar >= song_bars()) return;
    int  ci   = chord_idx(s);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    const int   pHat = POCKET[sng.pocket].hat, pSnare = POCKET[sng.pocket].snare,
                pBass = POCKET[sng.pocket].bass;
    int  sw   = (step % 2 == 1) ? (int)(stepMs * POCKET[sng.pocket].swing) : 0;  // swung 16ths
    int  inSlot = (int)(s % 8);                                 // step within the chord
    bool brk  = (sect == S_BREAK);                              // drums-only b-boy break

    // BASS — the star of the record. Root on every chord, fat and forward;
    // approach runs lean with the groove. Drops out entirely for the break.
    if (!brk && (inSlot == 0 || (inSlot == 4 && chance(40)) || (inSlot == 6 && chance(45)))) {
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
            schedule_hit(dly + pBass + sw + rnd(5) - 2, n, I_BASS, vol, (int)(stepMs * 4.5));
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
            int lag = dly + pSnare + rnd(4) - 2;
            schedule_hit(lag, 62, I_SNARE, 5, 70);
            schedule_hit(lag, 55, I_KICK, 2, 40);          // the body under the crack
            vu += 2.5f;
        }
        if ((s32 == 11 || s32 == 27) && chance(brk ? 35 : 18))   // ghost snare before the drag (busier in the break)
            schedule_hit(dly + pSnare + sw, 62, I_SNARE, 2, 35);
        // hats rush
        int hatEvery = (lvl >= 3 || brk) ? 1 : 2;          // the break rides 16ths
        if (step % hatEvery == 0) {
            int hv = (step % 4 == 2) ? 3 : 2;
            if (hatEvery == 1 && step % 2 == 1) hv = 1;
            int hd = dly + pHat + sw + rnd(3) - 1;
            schedule_hit(hd < 1 ? 1 : hd, 90, I_HAT, hv, 22);
        }
        // section-end fill (performance, never seeded) — also lifts out of the break
        if (bar % 8 == 7 && step >= 12 && chance(brk ? 80 : 55))
            schedule_hit(dly + pSnare, 62, I_SNARE, step == 12 ? 3 : 2 + (step - 12), 30);
    }

    // RHODES — sparse stabs on the chord changes, tremolo doing the rest
    if (lvl >= 1 && !brk) {
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

    // HOOK LEAD — the cell, narrowed to each chord's tones (out for the break)
    if (lvl >= 2 && !brk)
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

// the lead chair has two candidates — A/B them live with G. Same evidence-gathering
// step as jangle/bossa's guitar chair (per-song timbre roll): once both sound right,
// the song seed rolls WHICH lead shows up. The vibraphone is the Nujabes sound —
// radio-instrument-options.md calls it the single highest-value engine swap.
static int leadVibes = 0;   // 0 = TRI+vibrato (shipped sound), 1 = INSTR_MALLET vibraphone

static void setup_lead(void) {
    if (!leadVibes) {
        instrument(I_LEAD, INSTR_TRI, 10, 180, 4, 200);          // the sparse-hook fake
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.4f, 0.12f);
        instrument_filter(I_LEAD, FILTER_LOW, 2200, 2);
    } else {
        instrument(I_LEAD, INSTR_MALLET, 1, 0, 7, 1200);         // struck bar — rings on its own
        instrument_harmonics(I_LEAD, 0.25f);                     // the vibes preset from the
        instrument_timbre(I_LEAD, 0.50f);                        //   mallet cart (key 4):
        instrument_morph(I_LEAD, 0.90f);                         //   morph's top = the motor
        instrument_filter(I_LEAD, FILTER_LOW, 2600, 2);          // sit it back in the lofi mix
    }
}

static void setup_instruments(void) {
    chLead = rad_chair(&band, "lead", "TRI", "VIBES", NULL, NULL);   // the one A/B chair
    instrument(I_RHODES, INSTR_TRI, 8, 300, 3, 200);
    instrument_filter(I_RHODES, FILTER_LOW, 1600, 2);
    instrument_lfo(I_RHODES, 0, LFO_VOLUME, 4.2f, 0.10f);    // tremolo
    instrument_env(I_RHODES, 0, ENV_CUTOFF, 0, 110, 900);    // bark

    instrument(I_BASS, INSTR_SINE, 4, 220, 5, 120);          // upright-ish
    instrument_filter(I_BASS, FILTER_LOW, 480, 1);
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 35, 4);          // the thump
    instrument_drive(I_BASS, 0.20f);                         // tape warmth, not fuzz — the Low End is SATURATED low end

    setup_lead();

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
        if (LOWEND_SEED) { new_song(pos, LOWEND_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    // the shared input block (radio.h): feel/tempo/help handled inside, the cart
    // reacts to the events. ntone=0 — lowend has no tone knob.
    int ev = rad_input(&tempo, 82, 104, 2, &intensity, &toneSel, 0, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) note_off_all();
        else scheduled = (long)pos;
    }
    int chair = rad_band_input(&band, &showHelp);          // THE BAND (B) — A/B the lead chair
    if (chair == chLead) { leadVibes = band.c[chLead].sel; setup_lead(); }   // mid-song swap

    if (radioOn) {
        long st;                           // schedule one step ahead of the clock
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= song_bars() * 16) fresh_song(pos);

        long ss = songStep >= 0 ? songStep : 0;
        chord_label(nowChord[0], 12, chord_idx(ss));
        chord_label(nowChord[1], 12, (chord_idx(ss) + 1) % sng.nCh);
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    static const char *SN[5] = { "intro", "verse", "hook", "break", "outro" };
    watch("song", "%d", songCount);
    watch("sect", "%s", SN[sect_of(ss >= 0 ? ss / 16 : 0)]);
    watch("chord", "%s", nowChord[0]);
    watch("loopBars", "%d", sng.loopBars);
    watch("pocket", "%s", PNAME[sng.pocket]);
    watch("form", "%s", FORMS[sng.form].name);
#endif
}

// ── draw — the boombox under the skyline (shared chassis from radio.h; the
// window art — the seed-lit city skyline at night — stays lowend's own) ─────
void draw(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();                                         // knobs are touch-draggable
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;

    rad_body(CLR_BROWNISH_BLACK, CLR_ORANGE);   // matte black box, neon-orange edge
    rad_dial(sng.freq, CLR_ORANGE);

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
    rect(148, 52, 142, 44, CLR_ORANGE);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_ORANGE);
        char l2[32];
        snprintf(l2, 32, "key %s   %s", PCNAME[sng.keyPc], PNAME[sng.pocket]);
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
        static const char *SN[5] = { "intro", "verse", "hook", "break", "outro" };
        print(SN[sect_of(bar)], 240, 120, CLR_MEDIUM_GREY);
        rad_phrase_dots(208, 132, form_sects(), bar / 8, CLR_ORANGE);
    }

    // knobs + power LED
    static const char *FEEL[4] = { "basement", "headnod", "cypher", "banger" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_ORANGE);
    if (rad_knob_int(&tempo, 82, 104, 2, 218, 148, 9, "tempo", CLR_ORANGE)) bpm(tempo);
    float vt = vu / 12.0f;
    rad_knob(262, 148, 11, vt > 1 ? 1 : vt, "low", CLR_RED);   // meter
    rad_power_led(radioOn, CLR_RED, CLR_DARK_RED);

    rad_help_button(CLR_ORANGE);
    rad_band_button(CLR_ORANGE);

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next song (rolls a new seed)" },
            { "R",          "same song again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - how many layers play" },
            { "UP/DOWN",    "tempo of this tune" },
            { "M",          "radio power on / off" },
            { "B",          "the band - swap the lead timbre" },
            { "H or ?",     "show / hide this help" },
        };
        static const char *NOTES[3] = {
            "the #number on the display IS the song.",
            "pin it for good: #define LOWEND_SEED 0x...",
            "seeded composition, played fresh every time",
        };
        rad_help_panel("LOW END RADIO", HELP, 8, NOTES, 3, CLR_ORANGE);
    }
    rad_band_panel(&band, CLR_ORANGE);

    ui_end();
}
