/* de:meta
{
  "title": "roadhouse radio",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing",
    "swing-timing"
  ],
  "lineage": "Extends the radio.h station family (first station to use wave_set + improv.h) — novel in that its improviser builds phrase-arc solos with tension/release rather than repeating a fixed cell.",
  "homage": "The Doors (modal psych-rock)",
  "description": "Endless modal psych-rock - the Doors homage, station #13, built to carry THE IMPROVISER (melody brain #2): the first station whose band can take a SOLO. The jam form is head, organ solo, guitar solo, head out - and each 16-bar solo is improvised phrase by phrase: a 3-4 note motif invented at solo start gets stated, answered (inverted), sequenced up the mode, and doubled at the peak of a tension arc (density rises, register climbs, release comes home long), with breath rests between phrases. The improviser is PURE PERFORMANCE - engine rnd, never the seed - so R replays the song but the solos are new every time. A seeded 'how blue' knob biases b3/b7/b5 intrusions over the major-side vamp (the deliberate clash). The band: Vox-style combo organ - the FIRST station to use wave_set (a drawn drawbar cycle on INSTR_USER0); Rhodes PIANO BASS = the FM epiano detent an octave down playing a seeded swung ostinato; Krieger guitar = INSTR_PLUCK with an open-string drone ringing under the solo line; trio kit rolled shuffle / latin / rock. Mixolydian, dorian, or phrygian two-chord vamps (B-A forever). SPACE next jam, R same song new solos, [ ] history, LEFT/RIGHT feel (seance/lounge/jam/storm), UP/DOWN tempo, T tone, M power, B band (swap chairs live), H help."
}
de:meta */
// ── ROADHOUSE RADIO ───────────────────────────────────────────────────────
// The thirteenth station: endless modal psych-rock — the Doors homage. One
// vamp, two keyboards played by one man, and a band that JAMS: the form is
// head → long instrumental middle (organ solo, then guitar solo, each with
// its own arc) → head out.
//
// The reason this station exists: THE IMPROVISER — melody brain #2. Every
// station so far repeats a cell or a fixed riff; nobody could take a SOLO.
// This one can:
//   • PHRASE-BASED — the solo is built two bars at a time: a 3-4 note MOTIF
//     (invented at solo start) gets DEVELOPED per phrase — stated, answered,
//     sequenced up the mode, doubled at the peak — with breath rests between
//     phrases. Question/answer, not noodling.
//   • A TENSION ARC across each 16-bar solo: density rises, the register
//     climbs, the peak doubles the time, the release comes home long.
//   • PURE PERFORMANCE — the improviser runs on engine rnd(), never the
//     seed: the band never played it the same twice, and neither does the
//     radio. R replays the SONG; the solos are new every time.
//   • BLUES DUALITY — a seeded "how blue" knob biases b3/b7/b5 substitutions
//     OVER the major-side vamp: the deliberate clash (the opposite of
//     jingle.c's accommodation rule).
//
// The band: Vox-style combo organ — the FIRST station to pull the wave_set
// lever (a drawn drawbar cycle on INSTR_USER0, the recipe from the guide);
// Rhodes PIANO BASS = INSTR_FM's epiano detent an octave down, playing a
// seeded ostinato (dub's riddim, swung); Krieger guitar = INSTR_PLUCK with
// an open-string DRONE held under the moving line; a live trio kit that
// rolls shuffle / latin / straight rock per song.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help

#include "studio.h"
#include "radio.h"
#include "improv.h"   // THE IMPROVISER — born here, extracted when the cocktail trio became customer #2
#include <stdio.h>
#include <math.h>

#define ROADHOUSE_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_ORG   5    // combo organ — INSTR_USER0 drawbar wave (wave_set, at last)
#define I_ORGL  6    // organ solo lead — same wave, brighter
#define I_PBASS 7    // Rhodes piano bass — INSTR_FM, the left hand
#define I_GTR   8    // Krieger guitar — INSTR_PLUCK line
#define I_DRONE 9    // the open-string pedal under the guitar
// kit
#define SL_KICK 10
#define SL_SN   11
#define SL_HATC 12
#define SL_RIDE 13
#define SL_TAMB 14

// ── harmony — vamps, not progressions: the mode IS the song ──────────────
enum { M_MIXO, M_DORIAN, M_PHRYG, NMODE };
static const char *MODENAME[NMODE] = { "mixolydian", "dorian", "phrygian" };
static const int MODE[NMODE][7] = {
    { 0, 2, 4, 5, 7, 9, 10 },    // mixolydian — L.A. Woman territory
    { 0, 2, 3, 5, 7, 9, 10 },    // dorian — the minor-side jam
    { 0, 1, 4, 5, 7, 8, 10 },    // phrygian-dominant — "The End" excursions
};
// the vamp: TWO chords rocked forever (or one + its color). off/q per mode:
enum { Q_MAJ, Q_MIN, Q_DOM7, Q_SUS, NQ };
static const char *QN[NQ] = { "", "m", "7", "sus" };
static const int QV[NQ][3] = {
    { 4, 7, 12 },    // major triad (the organ plays triads, not jazz 9ths)
    { 3, 7, 12 },
    { 4, 10, 14 },
    { 5, 7, 12 },
};
typedef struct { int off, q; } Ch;
static const Ch VAMP[NMODE][2] = {
    { {0,Q_MAJ},  {10,Q_MAJ} },   // mixo:   I  bVII   (A-G over an A center)
    { {0,Q_MIN},  {5,Q_DOM7} },   // dorian: i  IV7    (the organ-jazz vamp)
    { {0,Q_MIN},  {1,Q_MAJ} },    // phryg:  i  bII    (the snake)
};

// ── the form — a JAM: head, two solos with arcs, head out ─────────────────
enum { S_HEAD, S_ORGAN, S_GUITAR, S_OUTRO };
static const int FORM[8] = { S_HEAD, S_HEAD, S_ORGAN, S_ORGAN,
                             S_GUITAR, S_GUITAR, S_HEAD, S_OUTRO };
static const char *SECTNAME[4] = { "head", "organ solo", "guitar solo", "outro" };

// ── the generated song ────────────────────────────────────────────────────
enum { K_SHUFFLE, K_LATIN, K_ROCK, NKIT };
static const char *KITNAME[NKIT] = { "shuffle", "latin", "rock" };

typedef struct {
    int  keyPc, mode;
    int  blue;                 // 15..55: how often the solo bends blue
    int  kit;
    int  pbDeg[8], pbMask;     // the piano-bass ostinato (1-bar cell, 8ths)
    int  headOn[6], headN;     // the head's riff cell (seeded, 32 steps)
    int  headDeg[6];
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 136.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)

static int    tempo     = 110;
static int    intensity = 1;
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    toneSel   = 2;
static int    songCount = 0;
static float  vu        = 0;
static char   nowChord[4][12];

static int  gvOrg[3] = { 60, 64, 67 };
static bool orgInit = false;

// THE BAND (B) — the chairs and their candidates, from radio-instrument-options.md.
// The organ chair is the WAVE chair: cycling it regenerates the wave_set(0, ...)
// drawbar table, retimbring BOTH I_ORG and I_ORGL (they share INSTR_USER0).
static RadBand band;
static int chOrgan, chPBass, chGtr;

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);   // defined with the chairs, below

// ══ THE IMPROVISER — now shared machinery (runtime/improv.h) ══════════════
// Born in this cart; extracted verbatim when the cocktail trio became its
// second customer. Pure performance rnd — extraction can't break any seed.
static Improv solo;

// degree index -> midi in this song's mode (the head riff still uses it)
static int deg_to_midi(int degIdx, int center) {
    return improv_deg_to_midi(degIdx, center, MODE[sng.mode]);
}

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Crystal", "Midnight", "Desert", "Spanish", "Neon",
    "Lizard", "Velvet", "Roadhouse", "Indian", "Cactus", "Mojave", "Twilight" };
static const char *TW2[] = { "Ride", "Storm", "Caravan", "Highway", "Serpent",
    "Mirage", "Dancer", "Crossing", "Canyon", "Queen", "Canticle", "Run" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.keyPc = srnd(12);
    sng.mode  = srnd(100) < 50 ? M_MIXO : (srnd(100) < 70 ? M_DORIAN : M_PHRYG);
    sng.blue  = 15 + srnd(41);                      // how blue this band plays
    sng.kit   = srnd(3);

    // the piano-bass ostinato: a 1-bar left hand (the riddim move, swung)
    static const int PBD[8] = { 0, 0, 0, 7, 7, 10, 12, 5 };
    sng.pbMask = 1 | (1 << 4);                      // 1 and the and-of-2 anchor
    for (int b = 1; b < 8; b++) if (b != 4 && srnd(100) < 45) sng.pbMask |= 1 << b;
    for (int b = 0; b < 8; b++) sng.pbDeg[b] = PBD[srnd(8)];

    // the head riff: a seeded cell — the COMPOSED melody the solos depart from
    sng.headN = 0;
    static const int HC[10] = { 0, 3, 6, 8, 12, 16, 19, 22, 24, 28 };
    for (int i = 0; i < 10 && sng.headN < 5; i++)
        if (srnd(100) < 40) sng.headOn[sng.headN++] = HC[i];
    if (sng.headN < 3) { sng.headN = 3; sng.headOn[0] = 0; sng.headOn[1] = 8; sng.headOn[2] = 19; }
    for (int i = 0; i < sng.headN; i++) sng.headDeg[i] = srnd(5);   // low mode degrees

    // the band's roll: organ brightness, guitar pick, piano-bass growl
    instrument_timbre(I_GTR,  0.40f + srnd(30) * 0.01f);
    instrument_morph(I_GTR,   0.15f + srnd(25) * 0.01f);
    instrument_timbre(I_PBASS, 0.45f + srnd(25) * 0.01f);
    instrument_morph(I_PBASS,  0.08f + srnd(14) * 0.01f);

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 96 + srnd(27);                          // 96..122
    bpm(tempo);
    apply_band_overrides();          // picked chairs beat the per-song roll above
    songBase = (long)pos + 8;
    orgInit = false;
    songCount++;
}

static void fresh_song(double pos) {
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ────────────────────────────────────────────────────────
static int sect_of(long bar)  { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }
static Ch  chord_at(long bar) { return VAMP[sng.mode][(bar / 2) % 2]; }   // rock 2 bars each
static int root_pc(Ch c)      { return (sng.keyPc + c.off) % 12; }

static void chord_label(char *out, int n, Ch c) {
    snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(c)], QN[c.q]);
}

static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_OUTRO) ? 0 : (s == S_HEAD ? 1 : 2);
    return rad_level(base, intensity);
}

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;
    Ch   c    = chord_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    bool kit  = !(sect == S_OUTRO && bar % 8 >= 5) && !(bar == 0);
    // the shuffle: swung off-beat 8ths
    int sw = (sng.kit == K_SHUFFLE && step % 4 == 2) ? (int)(stepMs * 4 * 0.09) : 0;

    // ── KIT — the trio, three personalities ──
    if (kit) {
        if (sng.kit == K_LATIN) {                   // Densmore's Break-on-Through bossa
            if (step == 0 || step == 10)
                { schedule_hit(dly + 1, 36, SL_KICK, 5, 110); vu += 1.8f; }
            if (step == 4 || step == 12)
                schedule_hit(dly + 1, 60, SL_SN, step == 12 ? 4 : 2, 60);
            if (step % 2 == 0)
                schedule_hit(dly + 1, 80, SL_HATC, 2, 30);
            if (lvl >= 2 && (step == 3 || step == 11))
                schedule_hit(dly + 1, 88, SL_TAMB, 2, 40);
        } else {
            if (step == 0 || (step == 8 && sng.kit == K_ROCK) || (step == 10 && chance(35)))
                { schedule_hit(dly + 1, 36, SL_KICK, 5, 120); vu += 1.8f; }
            if (step == 4 || step == 12)
                { schedule_hit(dly + sw + 1, 60, SL_SN, 4, 80); vu += 1.5f; }
            if (lvl >= 1 && (step == 6 || step == 14) && sng.kit == K_SHUFFLE && chance(50))
                schedule_hit(dly + sw + 1, 60, SL_SN, 1, 40);          // shuffle ghosts
            if (step % 2 == 0)
                schedule_hit(dly + sw + 1, 80, sect >= S_ORGAN && sect <= S_GUITAR ? SL_RIDE : SL_HATC,
                             step % 4 == 0 ? 3 : 2, sect >= S_ORGAN ? 160 : 35);
        }
    }

    // ── PIANO BASS — the seeded left hand, looping under everything ──
    if (bar >= 1 && step % 2 == 0 && (sng.pbMask >> (step / 2)) & 1) {
        int n = 36 + root_pc(c) + sng.pbDeg[step / 2];
        while (n > 48) n -= 12;
        schedule_hit(dly + sw + 3, n, I_PBASS, 5, (int)(stepMs * 1.6f));
        vu += 1.4f;
    }

    // ── ORGAN — held triads rocking the vamp (the right hand) ──
    if (step == 0 && bar % 2 == 0 && bar >= 1) {
        rad_lead_to(root_pc(c), QV[c.q], gvOrg, 3, 55, 74, &orgInit);
        int vol = sect == S_ORGAN ? 3 : 4;          // duck under its own solo
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 5, gvOrg[k], I_ORG, vol, (int)(stepMs * 30));
        vu += 1.5f;
    }

    // ── THE HEAD — the composed riff (organ lead), heads + outro ──
    if ((sect == S_HEAD || sect == S_OUTRO) && bar >= 1) {
        int cs = (int)(s % 32);
        for (int i = 0; i < sng.headN; i++)
            if (sng.headOn[i] == cs) {
                int m = deg_to_midi(sng.headDeg[i], 72 + sng.keyPc - 12);
                schedule_hit(dly + sw + 8, m, I_ORGL, 5, (int)(stepMs * 3));
                vu += 1.3f;
            }
    }

    // ── THE SOLOS — the improviser reads its rendered phrase ──
    if (sect == S_ORGAN || sect == S_GUITAR) {
        long soloBar = bar - (sect == S_ORGAN ? 16 : 32);
        if (soloBar == 0 && step == 0)              // a fresh idea per solo
            improv_begin(&solo, sect == S_ORGAN ? 67 : 62, 16, 1.0f);
        if (step == 0 && bar % 2 == 0 && improv_due(&solo, soloBar))
            improv_render(&solo, soloBar, MODE[sng.mode]);   // two new bars of thought
        float arc = improv_arc(&solo, soloBar);
        int cs = (int)(s % 32);
        for (int i = 0; i < solo.n; i++)
            if (solo.onset[i] == cs) {
                int m   = improv_midi(&solo, i, soloBar, sng.keyPc, MODE[sng.mode], sng.blue);
                int vol = 4 + (arc > 0.6f ? 1 : 0);
                int gat = (int)(stepMs * (solo.dur[i] > 4 ? solo.dur[i] : 2) * 0.9f);
                if (sect == S_ORGAN)
                    schedule_hit(dly + sw + 10, m, I_ORGL, vol, gat);
                else
                    schedule_hit(dly + sw + 10, m, I_GTR, vol, gat > 1400 ? 1400 : gat);
                vu += 1.6f;
            }
        // the Krieger DRONE: the open string rings under the guitar solo
        if (sect == S_GUITAR && step % 8 == 0)
            schedule_hit(dly + 2, 40 + sng.keyPc, I_DRONE, 3, (int)(stepMs * 9));
    }

    // tambourine lifts the solos' peaks
    if (lvl >= 3 && sect >= S_ORGAN && sect <= S_GUITAR && step % 4 == 2)
        schedule_hit(dly + sw + 1, 88, SL_TAMB, 1, 35);
}

// ── setup ─────────────────────────────────────────────────────────────────
// Each chair's candidates are full recipes — switching re-aims the slot from
// scratch, so a swap mid-song is clean. sel 0 is always the shipped sound.
// These apply functions NEVER call srnd — pinned seeds stay byte-identical.
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chOrgan) {
        // THE WAVE CHAIR — regenerate the INSTR_USER0 drawbar table, retimbring
        // BOTH I_ORG and I_ORGL (they share USER0). Roll the vibrato depth with it.
        float t[64];
        if (sel == 0) {
            // VOX night — the shipped drawbar cycle (8' + 4' + 2 2/3' + 2')
            for (int i = 0; i < 64; i++) {
                float ph = i / 64.0f;
                t[i] = 0.55f * sinf(ph *  6.2832f) + 0.28f * sinf(ph * 12.566f)
                     + 0.18f * sinf(ph * 18.850f) + 0.12f * sinf(ph * 25.133f);
            }
            wave_set(0, t, 64);
            instrument(I_ORG, INSTR_USER0, 18, 90, 6, 120);      // the combo organ
            instrument_filter(I_ORG, FILTER_LOW, 2600, 1);
            instrument_lfo(I_ORG, 0, LFO_PITCH, 6.2f, 0.05f);    // the cheesy vibrato tab
            instrument(I_ORGL, INSTR_USER0, 6, 60, 6, 90);       // solo stop: brighter
            instrument_filter(I_ORGL, FILTER_LOW, 3400, 2);
            instrument_lfo(I_ORGL, 0, LFO_PITCH, 6.2f, 0.07f);
        } else {
            // GIBSON G-101 night — reedier, brighter footage: more 2nd/4th, a 6th
            for (int i = 0; i < 64; i++) {
                float ph = i / 64.0f;
                t[i] = 0.48f * sinf(ph *  6.2832f) + 0.34f * sinf(ph * 12.566f)
                     + 0.10f * sinf(ph * 18.850f) + 0.22f * sinf(ph * 25.133f)
                     + 0.10f * sinf(ph * 37.699f);
            }
            wave_set(0, t, 64);
            instrument(I_ORG, INSTR_USER0, 18, 90, 6, 120);
            instrument_filter(I_ORG, FILTER_LOW, 3000, 1);       // filters up a bit
            instrument_lfo(I_ORG, 0, LFO_PITCH, 6.2f, 0.07f);    // vibrato a touch deeper
            instrument(I_ORGL, INSTR_USER0, 6, 60, 6, 90);
            instrument_filter(I_ORGL, FILTER_LOW, 3900, 2);
            instrument_lfo(I_ORGL, 0, LFO_PITCH, 6.2f, 0.09f);
        }
    } else if (idx == chPBass) {
        if (sel == 0) {
            instrument(I_PBASS, INSTR_FM, 2, 500, 4, 180);       // the Rhodes piano bass
            instrument_harmonics(I_PBASS, 0.15f);                // epiano detent, low register
        } else {
            instrument(I_PBASS, INSTR_TRI, 3, 300, 5, 110);      // the L.A. Woman session bassist
            instrument_filter(I_PBASS, FILTER_LOW, 480, 1);
            instrument_env(I_PBASS, 0, ENV_PITCH, 0, 16, 2);     // the upright thump
        }
    } else if (idx == chGtr) {
        if (sel == 0) {
            instrument(I_GTR, INSTR_PLUCK, 1, 0, 7, 800);        // Krieger's line
            instrument_harmonics(I_GTR, 0.55f);
            instrument_filter(I_GTR, FILTER_LOW, 2400, 2);
            instrument_drive(I_GTR, 0.0f);                       // clean
        } else if (sel == 1) {
            instrument(I_GTR, INSTR_PLUCK, 1, 0, 7, 800);        // the fuzz night
            instrument_harmonics(I_GTR, 0.55f);
            instrument_filter(I_GTR, FILTER_LOW, 1900, 2);       // darker, growling
            instrument_drive(I_GTR, 0.45f);
        } else {
            instrument(I_GTR, INSTR_PLUCK, 1, 0, 7, 800);        // the flatpick night
            instrument_harmonics(I_GTR, 0.55f);
            instrument_timbre(I_GTR, 0.75f);                     // bright pick
            instrument_filter(I_GTR, FILTER_LOW, 2400, 2);
            instrument_drive(I_GTR, 0.0f);                       // clean
        }
    }
}

// re-assert any non-default chair (new_song re-rolls timbre/morph over the
// slots; default chairs keep the per-song roll, picked chairs win over it)
static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}

static void setup_instruments(void) {
    chOrgan = rad_chair(&band, "organ", "vox", "gibson", NULL, NULL);
    chPBass = rad_chair(&band, "piano bass", "rhodes", "upright", NULL, NULL);
    chGtr   = rad_chair(&band, "guitar", "krieger", "fuzz", "flatpick", NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);

    // the open-string pedal under the guitar — not a chair, the string stays
    instrument(I_DRONE, INSTR_PLUCK, 1, 0, 7, 1400);         // the open string
    instrument_harmonics(I_DRONE, 0.75f);                    // long ring
    instrument_timbre(I_DRONE, 0.25f);

    instrument(SL_KICK, INSTR_SINE, 0, 240, 0, 50);
    instrument_filter(SL_KICK, FILTER_LOW, 300, 2);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 40, 15.0f);
    instrument(SL_SN, INSTR_NOISE, 0, 110, 0, 55);
    instrument_filter(SL_SN, FILTER_BAND, 1600, 4);
    instrument(SL_HATC, INSTR_NOISE, 0, 32, 0, 15);
    instrument_filter(SL_HATC, FILTER_HIGH, 7800, 3);
    instrument(SL_RIDE, INSTR_SQUARE, 0, 280, 0, 150);
    instrument_filter(SL_RIDE, FILTER_HIGH, 5800, 4);
    instrument(SL_TAMB, INSTR_NOISE, 0, 80, 0, 28);
    instrument_filter(SL_TAMB, FILTER_BAND, 8200, 9);
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_ORG,  FILTER_LOW, (int)(2600 * tm), 1);
    instrument_filter(I_ORGL, FILTER_LOW, (int)(3400 * tm), 2);
    instrument_filter(I_GTR,  FILTER_LOW, (int)(2400 * tm), 2);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (ROADHOUSE_SEED) { new_song(pos, ROADHOUSE_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 88, 132, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) { note_off_all(); sfx(-1); }
        else scheduled = (long)pos;
    }
    if (ev & (RAD_EV_TONE | RAD_EV_NEW | RAD_EV_REPLAY | RAD_EV_BACK | RAD_EV_FWD)) {
        apply_tone();
    }

    int chair = rad_band_input(&band, &showHelp);   // THE BAND — B, then click/number
    if (chair >= 0) apply_chair(chair);

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= 64L * 16) fresh_song(pos);
        for (int i = 0; i < 2; i++) chord_label(nowChord[i], 12, VAMP[sng.mode][i]);
        nowChord[2][0] = nowChord[3][0] = 0;
    }

    vu *= 0.88f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("chord", "%s", nowChord[(int)((tbar / 2) % 2)]);
    watch("mode", "%s", MODENAME[sng.mode]);
    watch("phrase", "%d", solo.phraseNo);
    watch("blue", "%d", sng.blue);
#endif
}

// ── draw — the roadhouse chassis; window art = the desert highway at night ─
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_DARK_BROWN, CLR_DARK_ORANGE);    // roadhouse wood, neon trim
    rad_dial(sng.freq, CLR_DARK_ORANGE);

    // the window — a desert highway at night, storm far off
    rectfill(34, 52, 102, 116, CLR_DARKER_BLUE);             // night sky
    for (int i = 0; i < 14; i++) {                           // stars
        int sx2 = 38 + (i * 37) % 96, sy2 = 56 + (i * 23) % 48;
        if ((frame() / 30 + i) % 7) pset(sx2, sy2, CLR_LIGHT_GREY);
    }
    circfill(118, 64, 7, CLR_LIGHT_YELLOW);                  // the moon
    circfill(115, 62, 6, CLR_DARKER_BLUE);                   // ...a crescent
    rectfill(34, 120, 102, 48, CLR_BROWNISH_BLACK);          // the desert floor
    // the highway, vanishing
    {
        int vx = 86, vy = 120;
        line(46, 168, vx, vy, CLR_DARK_GREY);
        line(126, 168, vx, vy, CLR_DARK_GREY);
        for (int k = 0; k < 4; k++) {                        // dashes, crawling
            float tt = fmodf(timer() * 0.35f + k * 0.25f, 1.0f);
            int y = vy + (int)(tt * tt * 48);
            if (y < 166) line(vx - (y - vy) / 8, y, vx - (y - vy) / 8, y + 2 + (y - vy) / 12, CLR_LIGHT_YELLOW);
        }
    }
    // heat lightning over the solos
    if (radioOn && (sect == S_ORGAN || sect == S_GUITAR) && vu > 7 && frame() % 90 < 3)
        line(50 + frame() % 30, 54, 58 + frame() % 30, 76, CLR_WHITE);
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_DARK_ORANGE);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_ORANGE);
        char l2[36];
        snprintf(l2, 36, "%.1f FM  %s", sng.freq, MODENAME[sng.mode]);
        print(l2, 154, 70, CLR_DARK_ORANGE);
        snprintf(l2, 36, "%d bpm %s #%08X", tempo, KITNAME[sng.kit], sng.seed);
        font(FONT_SMALL);
        print(l2, 154, 82, CLR_DARK_ORANGE);
        font(FONT_NORMAL);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_ORANGE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        rad_chord_row(nowChord, 2, (int)((bar / 2) % 2), 152, 104, CLR_ORANGE);
        print(SECTNAME[sect], 152, 120, CLR_ORANGE);
        rad_phrase_dots(232, 124, 8, bar / 8, CLR_ORANGE);
    }

    static const char *FEEL[4] = { "seance", "lounge", "jam", "storm" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_ORANGE);
    if (rad_knob_int(&tempo, 88, 132, 2, 218, 148, 9, "tempo", CLR_ORANGE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_ORANGE)) apply_tone();
    rad_power_led(radioOn, CLR_ORANGE, CLR_DARK_BROWN);

    rad_help_button(CLR_ORANGE);
    rad_band_button(CLR_ORANGE);

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next jam (rolls a new seed)" },
            { "R",          "same song - the SOLOS are new" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - seance/lounge/jam/storm" },
            { "UP/DOWN",    "tempo of this jam" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "B",          "the band - swap chairs live" },
        };
        static const char *NOTES[3] = {
            "THE IMPROVISER: solos are phrases - a motif",
            "stated, answered, sequenced, doubled at the",
            "peak. never seeded. pin: ROADHOUSE_SEED 0x..",
        };
        rad_help_panel("ROADHOUSE RADIO", HELP, 8, NOTES, 3, CLR_ORANGE);
    }
    rad_band_panel(&band, CLR_ORANGE);
    ui_end();
}
