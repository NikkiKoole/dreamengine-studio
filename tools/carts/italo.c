/* de:meta
{
  "slug": "italo",
  "collection": ["radio"],
  "title": "italo disco radio",
  "status": "active",
  "created": "2026-06-09",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing",
    "step-sequencer",
    "swing-timing"
  ],
  "lineage": "Recombines ymo's arp brain, house's 808 pump, and citypop's gear-change into a minor-key Italo disco station — no new engine tricks, pure assembly of existing radio.h machinery with new harmonic-minor chord templates.",
  "homage": "Gazebo / Den Harrow (Italo disco)",
  "description": "Endless Italo disco - spaghetti disco, the lovelorn machine-pop of early-80s Italy (Gazebo, Den Harrow, Ryan Paris, the Moroder lineage). Citypop's machine cousin, but neon-at-midnight MINOR instead of sunlit major: dramatic, desperate, unashamedly cheesy. PURE RECOMBINATION (the parking-lot prediction: 0 new brains, dessert) - ymo's arp + house's pump + the citypop gear change + FM keys + new minor templates. THE SEQUENCER BASS is the spine: a relentless 16th-note synth bass that bounces octaves (root low, root high, forever - the Moroder pulse) with seeded chord-tone spice, a resonant lowpass + per-note cutoff env giving each note an analog-sequencer blip. Harmony is a stolen playbook of five dramatic minor formulas - i-bVI-bVII-V, the descending i-bVII-bVI-V, i-iv-bVI-V (the harmonic-minor V, the tear) - all with the borrowed dominant that makes Italo weep. FM BRASS STABS (INSTR_FM, bright integer ratio) punctuate the offbeats; EPIANO KEYS (INSTR_EPIANO, the 'FM keys') comp the bed; a PD soaring lead (INSTR_PD, glided, minor-pentatonic) owns the chorus. THE SIMMONS TOM FILL - the unmistakable descending electronic-tom roll into every chorus - is the genre's signature gesture, and THE TRUCK DRIVER'S GEAR CHANGE lifts the final chorus + outro up a semitone (of course). 808 kit + sidechain pump on loan from house.c / tr808.c. Seed on display (ITALO_SEED / R / [ ] history), LEFT/RIGHT feel, UP/DOWN tempo, T tone, M power, H help."
}
de:meta */
#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading, chrome)
#include <stdio.h>
#include <math.h>

// ── ITALO DISCO RADIO ───────────────────────────────────────────────────────
// The twentieth station: endless Italo disco — spaghetti disco, the lovelorn
// machine-pop of early-80s Italy (Gazebo, Den Harrow, Ryan Paris, Valerie
// Dore, Kano, the Moroder lineage). Citypop's machine cousin: where citypop is
// sunlit major, Italo is neon-at-midnight MINOR — dramatic, desperate, and
// unashamedly cheesy. This station is PURE RECOMBINATION (future-stations.md:
// "0 new brains — schedule it as dessert"): ymo's arp + house's pump + the
// citypop gear change + FM keys + new minor templates. No new engine trick;
// the joy is the assembly.
//
//   • THE SEQUENCER BASS — the genre's spine. A relentless 16th-note synth
//     bass that BOUNCES OCTAVES (root low, root high, forever — the Moroder
//     pulse), with seeded chord-tone spice. A resonant lowpass + per-note
//     cutoff env gives each note the "blip" of an analog sequencer. This is
//     ymo's arp brain, pointed down at the bass register.
//   • DRAMATIC MINOR FORMULAS — five transcribed Italo progressions, all in
//     the minor key the genre lives in: i-bVI-bVII-V, the descending
//     i-bVII-bVI-V, i-iv-bVI-V (the harmonic-minor V, the tear), and two
//     more. A stolen playbook (house.c / jingle.c style), not a generator.
//   • FM BRASS STABS — the dramatic orchestra-hit punctuation, on INSTR_FM
//     (bright integer ratio = brassy). Voice-led, on the offbeats and the
//     bar anticipations.
//   • EPIANO KEYS — the harmonic bed comped on INSTR_EPIANO (a Rhodes with a
//     touch of bark): the "FM keys" of the recombination note.
//   • THE PD LEAD — a soaring topline on INSTR_PD (the resonant Casio-CZ
//     wow), glided, minor-pentatonic, chorus/drop only. The melodrama.
//   • THE SIMMONS TOM FILL — the unmistakable descending electronic-tom roll
//     into every chorus (INSTR_SINE + a steep downward pitch env — the
//     "dewww-dewww" of the hexagonal pads). The genre's signature gesture.
//   • THE TRUCK DRIVER'S GEAR CHANGE — citypop's move, of course: the final
//     chorus + outro lift up a semitone. The cheesiest, most reliable thrill
//     in the genre.
//   • 808 DRUMS + THE PUMP — kick/clap/hats on loan from house.c (the
//     measured tr808.c circuits); the strings duck against the kick.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help
//
// Density = arrangement curve x feel shift (game-music.md). Seed pins the
// COMPOSITION (key, templates, bass spice, lead cell, title); the PERFORMANCE
// (lead path, ghost stabs, fill touch) re-rolls every playthrough.

#define ITALO_SEED 0   // pin a favourite track here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_BASS  5    // the sequencer bass — octave-bouncing saw
#define I_STAB  6    // FM brass stabs
#define I_KEYS  7    // EPIANO comp — the harmonic bed
#define I_ARP   8    // the high sequencer pluck
#define I_LEAD  9    // the PD soaring lead
#define I_STR  10    // string pad — the pump canvas
// drum slots 11..16 — 808 circuits on loan from house.c / tr808.c
#define SL_KICK 11
#define SL_CP   12
#define SL_HATC 13
#define SL_HATO 14
#define SL_TOM  15   // the Simmons electronic tom
#define SL_CYM  16

// ── chords — the stolen Italo playbook (minor, always) ──────────────────────
// rootless voicing intervals (3rd / 7th / color) for the voice-led comp; the
// bass owns the root. QT = full chord tones for the bass spice.
enum { Q_MIN, Q_MIN7, Q_MAJ, Q_MAJ7, Q_DOM, NQ };
static const char *QN[NQ] = { "m", "m7", "", "maj7", "7" };
static const int QV[NQ][3] = {
    { 3, 7, 12 },    // m    — plain minor triad
    { 3, 7, 10 },    // m7
    { 4, 7, 12 },    // maj
    { 4, 7, 11 },    // maj7
    { 4, 7, 10 },    // 7    — the dominant V, borrowed from harmonic minor: the tear
};
static const int QT[NQ][4] = {
    { 0, 3, 7, 12 }, { 0, 3, 7, 10 }, { 0, 4, 7, 12 }, { 0, 4, 7, 11 }, { 0, 4, 7, 10 },
};
static const int PENTMIN[5] = { 0, 3, 5, 7, 10 };   // minor pentatonic — the topline

typedef struct { int off, q; } Ch;   // off = semitones above the (minor) key root
static const Ch TMPL[5][4] = {
    // the classic: i bVI bVII V  (the V dominant — harmonic-minor tear)
    { {0,Q_MIN}, {8,Q_MAJ}, {10,Q_MAJ}, {7,Q_DOM} },
    // the descending lament: i bVII bVI V
    { {0,Q_MIN}, {10,Q_MAJ}, {8,Q_MAJ}, {7,Q_DOM} },
    // the minor drive: i iv bVI V
    { {0,Q_MIN7}, {5,Q_MIN}, {8,Q_MAJ}, {7,Q_DOM} },
    // the mediant shimmer: i bIIImaj7 bVII iv
    { {0,Q_MIN}, {3,Q_MAJ7}, {10,Q_DOM}, {5,Q_MIN} },
    // the heartbreak vamp: i bVImaj7 bIIImaj7 bVII
    { {0,Q_MIN7}, {8,Q_MAJ7}, {3,Q_MAJ7}, {10,Q_DOM} },
};
static const int VERSE_POOL[3]  = { 1, 2, 3 };
static const int CHORUS_POOL[3] = { 0, 4, 2 };

// ── the form — verse/chorus, the gear change on the final chorus ───────────
enum { S_INTRO, S_V, S_C, S_OUTRO };
static const int FORM[8] = { S_INTRO, S_V, S_V, S_C, S_V, S_C, S_C, S_OUTRO };
static const char *SECTNAME[4] = { "intro", "verse", "chorus", "outro" };
#define MOD_BAR 48    // sections 6+7 (final chorus + outro) lift a semitone — the truck driver

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    Ch   verse[4], chorus[4];
    int  bassTone[8];         // per-8th chord-tone index (0=root mostly): the spice
    int  stOn[6], stN;        // the FM stab offbeat mask (16-step)
    int  cellMidi[6], cellOn[6], cellN, hasLead;   // the PD lead cell
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;                       // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 124.0 };   // schedule-ahead step clock (radio.h)
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)   rad_srnd(&rs, (n))
static int    tempo     = 120;
static int    intensity = 1;     // feel: shifts the arrangement's density curve
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    songCount = 0;
static float  vu        = 0;
static int    bassLast  = 36;
static char   nowChord[4][12];

// voice-led voicings — each comp voice keeps its own fingers
static int  gvStab[3] = { 67, 71, 74 };
static int  gvKeys[3] = { 55, 60, 64 };
static int  gvArp[3]  = { 72, 76, 79 };
static int  gvStr[2]  = { 50, 57 };
static bool stabInit = false, keysInit = false, arpInit = false, strInit = false;

// held voices
static int  strH[2] = { -1, -1 };
static int  leadH   = -1;
static int  strVolNow = -1;
static long strBar  = -1, leadStep = -1;

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Dolce", "Notte", "Neon", "Robot", "Electric", "Magic",
    "Cyber", "Disco", "Future", "Amore", "Vamos", "Tokyo" };
static const char *TW2[] = { "Vita", "Amore", "Fantasy", "Lover", "Machine", "Tonight",
    "Forever", "Dancer", "Mistero", "Boy", "Italo", "Notte" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);   // 0 = derive fresh

    sng.keyPc = srnd(12);

    int vi = VERSE_POOL[srnd(3)], ci;
    do { ci = CHORUS_POOL[srnd(3)]; } while (ci == vi);
    for (int k = 0; k < 4; k++) { sng.verse[k] = TMPL[vi][k]; sng.chorus[k] = TMPL[ci][k]; }

    // the sequencer-bass spice: mostly the root (0), occasionally the 5th (2)
    // or the 3rd (1) — the line that bounces octaves over the top of this
    for (int b = 0; b < 8; b++) {
        int r = srnd(100);
        sng.bassTone[b] = (r < 70) ? 0 : (r < 88) ? 2 : 1;
    }
    sng.bassTone[0] = 0;     // always land the bar on the root

    // the FM stabs: 3..5 onsets leaning into the offbeats and the bar turn
    sng.stN = 0;
    static const int SCAND[10] = { 2, 3, 6, 7, 10, 11, 12, 13, 14, 15 };
    for (int i = 0; i < 10 && sng.stN < 5; i++)
        if (srnd(100) < 38) sng.stOn[sng.stN++] = SCAND[i];
    if (sng.stN < 3) { sng.stN = 3; sng.stOn[0] = 2; sng.stOn[1] = 6; sng.stOn[2] = 14; }

    // the PD lead cell: a seeded 1-bar phrase in the minor pentatonic
    sng.hasLead = srnd(100) < 80;
    sng.cellN = 0;
    static const int LCAND[11] = { 0, 2, 4, 6, 7, 8, 10, 11, 12, 13, 14 };
    for (int i = 0; i < 11 && sng.cellN < 6; i++)
        if (srnd(100) < 42) sng.cellOn[sng.cellN++] = LCAND[i];
    if (sng.cellN < 3) { sng.cellN = 3; sng.cellOn[0] = 0; sng.cellOn[1] = 6; sng.cellOn[2] = 12; }
    for (int i = 0; i < sng.cellN; i++) {
        int m = 72 + sng.keyPc + PENTMIN[srnd(5)];
        while (m > 89) m -= 12;
        while (m < 70) m += 12;
        sng.cellMidi[i] = m;
    }

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 116 + srnd(13);                  // 116..128 — Italo's mid-tempo strut
    bpm(tempo);
    songBase = (long)pos + 8;
    stabInit = keysInit = arpInit = false;
    bassLast = 36;
    leadStep = -1;
    songCount++;
}

static void fresh_song(double pos) {         // [ and ] walk the history
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ──────────────────────────────────────────────────────────
static int sect_of(long bar)  { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }
static const Ch *prog_of(long bar) { return sect_of(bar) == S_C ? sng.chorus : sng.verse; }
static Ch  chord_at(long bar) { return prog_of(bar)[bar % 4]; }
// the gear change lives here: everything past MOD_BAR is a semitone up
static int gear(long bar)     { return bar >= MOD_BAR ? 1 : 0; }
static int root_pc_b(Ch c, long bar) { return (sng.keyPc + gear(bar) + c.off) % 12; }

static void chord_label(char *out, int n, Ch c, long bar) {
    snprintf(out, n, "%s%s", RAD_PCNAME[root_pc_b(c, bar)], QN[c.q]);
}

// density = arrangement curve + feel shift (rad_level, see game-music.md)
//   0: bass + kick + hats + strings      2: + FM stabs and the PD lead
//   1: + arp, clap, EPIANO keys          3: + 16th arp, hat doubles
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_C ? 2 : 1);
    return rad_level(base, intensity);
}

// ── the tone knob (T cycles) — master brightness, re-issued live ───────────
static int toneSel = 2;
static void apply_voicing(void) {
    float m = RAD_TONEMUL[toneSel];
    instrument_filter(I_STAB, FILTER_LOW, (int)(3200 * m), 3);
    instrument_filter(I_KEYS, FILTER_LOW, (int)(2400 * m), 2);
    instrument_filter(I_ARP,  FILTER_LOW, (int)(3600 * m), 2);
    instrument_filter(I_LEAD, FILTER_LOW, (int)(2600 * m), 5);
}

// voice-led bass register: C1..C3, nearest the last note (the octave bounce
// rides on top of this — bassLast tracks the LOW octave)
static int bass_near(int pc) { return bassLast = rad_bass_to(pc, bassLast, 24, 40); }

// ── 808 voices, on loan from house.c / tr808.c ─────────────────────────────
static void clap_808(int dly, int vol) {       // three retriggers + a room tail
    schedule_hit(dly,      60, SL_CP, vol, 12);
    schedule_hit(dly + 10, 60, SL_CP, vol, 12);
    schedule_hit(dly + 20, 60, SL_CP, vol, 12);
    schedule_hit(dly + 28, 60, SL_CP, vol - 1, 140);
}

// ── the step player ─────────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;
    Ch   c    = chord_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  rpc  = root_pc_b(c, bar);

    bool fillBar = (sect == S_V || sect == S_C) && (bar % 8 == 7);   // last bar of a phrase

    // CRASH on the chorus / drop downbeat (long ring)
    if (step == 0 && bar % 8 == 0 && sect == S_C) {
        schedule_hit(dly, 79, SL_CYM, 4, 800);
        vu += 3;
    }

    // KICK — four on the floor (the fill bar drops the kick on its back half)
    if (step % 4 == 0 && !(fillBar && step >= 8)) {
        schedule_hit(dly + rnd(2), 31, SL_KICK, 6, 280);
        vu += 2.5f;
    }

    // CLAP on 2 and 4 (the three-retrigger circuit) — sits out the fill back half
    if (lvl >= 1 && (step == 4 || step == 12) && !(fillBar && step >= 8)) {
        clap_808(dly + rnd(2), 4);
        vu += 2;
    }

    // HATS — closed on the floor, open breathing the offbeat
    if (step % 2 == 0)
        schedule_hit(dly + rnd(2), 79, SL_HATC, step % 4 == 0 ? 3 : 2, 45);
    if (step % 4 == 2) {
        schedule_hit(dly + rnd(2), 79, SL_HATO, 3, 260);
        vu += 0.6f;
    }
    if (lvl >= 3 && step % 2 == 1)
        schedule_hit(dly + rnd(2), 79, SL_HATC, 1, 30);

    // THE SIMMONS TOM FILL — the descending electronic-tom roll into the next
    // section. Eight pads tumbling down across the back half of the fill bar.
    if (fillBar && step >= 8) {
        int k = step - 8;                       // 0..7 down the toms
        int midi = 60 - k * 3;                  // each pad lower than the last
        schedule_hit(dly + rnd(2), midi, SL_TOM, 5, 150);
        vu += 1.6f;
    }

    // THE SEQUENCER BASS — relentless 16ths, octave-bouncing. Even step = low
    // octave (the chord tone), odd step = the same note +12: the Moroder pulse.
    if (!(sect == S_INTRO && bar % 8 < 2)) {       // intro waits two bars
        int eighth = step / 2;
        int tone   = QT[c.q][sng.bassTone[eighth]];
        int base;
        if (step % 2 == 0)                          // low-octave chord tone
            base = bass_near((rpc + tone) % 12);
        else                                        // the octave pop
            base = bassLast + 12;
        int v = (sect == S_INTRO) ? 4 : 5;
        schedule_hit(dly + rnd(2), base, I_BASS, v, (int)(stepMs * 0.95));
        vu += step % 2 == 0 ? 1.4f : 0.7f;
    }

    // EPIANO KEYS — comp the chord on the bar downbeat and beat 3
    if (lvl >= 1 && (step == 0 || step == 8) && !fillBar) {
        rad_lead_to(rpc, QV[c.q], gvKeys, 3, 52, 74, &keysInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 4 + rnd(2), gvKeys[k], I_KEYS, 3, (int)(stepMs * 7));
        vu += 1.4f;
    }

    // THE HIGH SEQUENCER PLUCK — chord tones on 8ths (16ths when cooking)
    if (lvl >= 1 && !fillBar) {
        int every = (lvl >= 3) ? 1 : 2;
        if (step % every == 0) {
            rad_lead_to(rpc, QV[c.q], gvArp, 3, 64, 88, &arpInit);
            int v = gvArp[(step / every) % 3];
            schedule_hit(dly + rnd(2), v, I_ARP, 2, (int)(stepMs * 1.1));
            vu += 0.4f;
        }
    }

    // FM BRASS STABS — the dramatic punctuation; always on in the chorus
    if (lvl >= 2 && !fillBar) {
        for (int i = 0; i < sng.stN; i++)
            if (sng.stOn[i] == step) {
                rad_lead_to(rpc, QV[c.q], gvStab, 3, 60, 84, &stabInit);
                int vol = (sect == S_C) ? 4 : 3;
                for (int k = 0; k < 3; k++)
                    schedule_hit(dly + k * 5 + rnd(2), gvStab[k], I_STAB, vol, 180);
                vu += 1.8f;
            }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    // the sequencer bass — a saw through a resonant lowpass with a per-note
    // cutoff snap: the "blip" of an analog sequencer
    instrument(I_BASS, INSTR_SAW, 1, 120, 5, 50);
    instrument_filter(I_BASS, FILTER_LOW, 700, 5);
    instrument_env(I_BASS, 0, ENV_CUTOFF, 0, 60, 1400);
    instrument_drive(I_BASS, 0.25f);

    // FM brass stabs — low integer ratio + bright = brassy orchestra hit
    instrument(I_STAB, INSTR_FM, 2, 200, 3, 120);
    instrument_harmonics(I_STAB, 0.18f);     // near 1:1 / 2:1 — harmonic, brassy
    instrument_timbre(I_STAB, 0.68f);        // bright
    instrument_morph(I_STAB, 0.22f);         // a little feedback edge
    instrument_filter(I_STAB, FILTER_LOW, 3200, 3);

    // EPIANO comp — a Rhodes with a touch of bark (the "FM keys")
    instrument(I_KEYS, INSTR_EPIANO, 2, 0, 6, 900);
    instrument_harmonics(I_KEYS, 0.15f);     // Rhodes detent
    instrument_timbre(I_KEYS, 0.4f);
    instrument_morph(I_KEYS, 0.3f);
    instrument_filter(I_KEYS, FILTER_LOW, 2400, 2);

    // the high sequencer pluck — Karplus-Strong, bright pick
    instrument(I_ARP, INSTR_PLUCK, 0, 200, 0, 120);
    instrument_harmonics(I_ARP, 0.45f);
    instrument_timbre(I_ARP, 0.7f);
    instrument_filter(I_ARP, FILTER_LOW, 3600, 2);

    // the PD soaring lead — the resonant Casio-CZ "wow"
    instrument(I_LEAD, INSTR_PD, 4, 160, 6, 140);
    instrument_harmonics(I_LEAD, 0.6f);      // a resonant wavetype
    instrument_timbre(I_LEAD, 0.55f);
    instrument_morph(I_LEAD, 0.5f);          // the DCW sweep on the strike
    instrument_filter(I_LEAD, FILTER_LOW, 2600, 5);

    // the string pad — the ride / pump canvas
    instrument(I_STR, INSTR_SAW, 280, 500, 6, 800);
    instrument_filter(I_STR, FILTER_LOW, 1100, 2);

    // — 808 circuits, on loan from house.c / tr808.c —
    instrument(SL_KICK, INSTR_SINE, 0, 480, 0, 60);          // bridged-T boom
    instrument_filter(SL_KICK, FILTER_LOW, 250, 3);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 50, 26.0f);
    instrument_drive(SL_KICK, 0.30f);

    instrument(SL_CP, INSTR_NOISE, 0, 110, 0, 50);           // handclap
    instrument_filter(SL_CP, FILTER_BAND, 1100, 5);

    instrument(SL_HATC, INSTR_SQUARE, 0, 42, 0, 16);         // metal bank, hp'd
    instrument_filter(SL_HATC, FILTER_HIGH, 7000, 3);
    instrument(SL_HATO, INSTR_SQUARE, 0, 340, 0, 90);
    instrument_filter(SL_HATO, FILTER_HIGH, 7000, 3);
    instrument_choke(SL_HATC, SL_HATO);

    // the Simmons electronic tom — a sine with a steep downward pitch sweep
    instrument(SL_TOM, INSTR_SINE, 0, 220, 0, 40);
    instrument_filter(SL_TOM, FILTER_LOW, 1800, 2);
    instrument_env(SL_TOM, 0, ENV_PITCH, 0, 110, 10.0f);     // the "dewww"
    instrument_drive(SL_TOM, 0.2f);

    instrument(SL_CYM, INSTR_SQUARE, 0, 850, 0, 200);        // the crash ring
    instrument_filter(SL_CYM, FILTER_HIGH, 3440, 3);
}

// ── held-voice control: strings + the PD lead ──────────────────────────────
static void stop_held(void) {
    note_off_all();
    strH[0] = strH[1] = leadH = -1;
    strInit = false;
    strVolNow = -1;
    strBar = -1;
}

static void drive_strings(long bar, int sect) {
    Ch c = chord_at(bar < 0 ? 0 : bar);
    long b = bar < 0 ? 0 : bar;
    int rpc = root_pc_b(c, b);
    if (strH[0] < 0) {
        rad_lead_to(rpc, QV[c.q], gvStr, 2, 48, 67, &strInit);
        for (int i = 0; i < 2; i++) {
            strH[i] = note_on(gvStr[i], I_STR, 3);
            note_glide(strH[i], 160);
            note_lfo(strH[i], 0, LFO_PITCH, rnd_float_between(0.10f, 0.22f),
                                            rnd_float_between(0.04f, 0.08f));   // chorus wow
            note_pitch(strH[i], gvStr[i] + rnd_float_between(-0.05f, 0.05f));
        }
        strBar = bar;
        strVolNow = 3;
    } else if (bar != strBar && bar >= 0) {
        strBar = bar;
        rad_lead_to(rpc, QV[c.q], gvStr, 2, 48, 67, &strInit);
        for (int i = 0; i < 2; i++) note_pitch(strH[i], gvStr[i]);
    }
    int want = (sect == S_INTRO || sect == S_OUTRO) ? 2 : (sect == S_C ? 4 : 3);
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
    bool want = sng.hasLead && s >= 0 && bar < 64
                && sect_of(bar) == S_C && level_of(bar) >= 2;
    if (!want) {
        if (leadH >= 0) { note_off(leadH); leadH = -1; }
        return;
    }
    int g = gear(bar) ;                    // the lead lifts with the gear change too
    if (leadH < 0) {
        leadH = note_on(sng.cellMidi[0] + g, I_LEAD, 0);
        note_glide(leadH, 80);             // the portamento — the Italo swoon
    }
    int st16 = (int)(s % 16);
    for (int i = 0; i < sng.cellN; i++)
        if (sng.cellOn[i] == st16 && chance(90)) {
            note_pitch(leadH, sng.cellMidi[i] + g);
            note_vol(leadH, 4);
            vu += 1.2f;
        }
    if (st16 == 15 && chance(40)) note_vol(leadH, 0);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        apply_voicing();
        if (ITALO_SEED) { new_song(pos, ITALO_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 108, 134, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_TONE)   apply_voicing();
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) stop_held();
        else scheduled = (long)pos;
    }

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= 64L * 16) fresh_song(pos);

        long bar = songStep >= 0 ? songStep / 16 : 0;
        const Ch *p = prog_of(bar);
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 12, p[i], bar);

        int sect = songStep >= 0 ? sect_of(bar) : S_INTRO;

        // the strings: pump against the kick (duck on the beat, bloom after)
        bool playing = songStep >= 0;
        float pump = playing ? 0.55f + 0.45f * (float)beat_pos() : 1.0f;
        drive_strings(playing ? bar : -1, sect);
        float tm = 0.7f + 0.3f * RAD_TONEMUL[toneSel];
        for (int i = 0; i < 2; i++)
            if (strH[i] >= 0)
                note_cutoff(strH[i], (int)((900 + (sect == S_C ? 600 : 0)) * tm * pump));

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
    watch("geared", "%d", gear(tbar));
#endif
}

// ── draw — the neon-sunset console (shared chassis from radio.h; the window
// art — the outrun sun over a vanishing-point grid — is Italo's own) ────────
static int sun_band(int row) {                 // the sun's stacked neon bands
    static const int B[5] = { CLR_YELLOW, CLR_ORANGE, CLR_DARK_ORANGE, CLR_PINK, CLR_DARK_RED };
    return B[row < 0 ? 0 : row > 4 ? 4 : row];
}

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_DARKER_PURPLE, CLR_PINK);   // midnight chrome, a hot-pink edge
    rad_dial(sng.freq, CLR_PINK);

    // the window — the outrun sun over a perspective grid
    rectfill(34, 52, 102, 116, CLR_DARKER_BLUE);
    float breathe = vu / 12.0f;
    int cx = 85, cy = 96, R = 24 + (int)(breathe * 2);
    // the sun: filled bands, sliced by horizontal slits low down
    for (int dy = -R; dy <= R; dy++) {
        int half = (int)sqrtf((float)(R * R - dy * dy));
        int y = cy + dy;
        int band = (dy + R) * 5 / (2 * R + 1);
        bool slit = (dy > 2) && (((dy + frame() / 8) % 4) == 0);   // the neon venetian slits
        if (!slit)
            for (int x = cx - half; x <= cx + half; x++)
                pset(x, y, sun_band(band));
    }
    // the vanishing-point grid below the horizon
    int hor = cy + R - 2;
    for (int i = -5; i <= 5; i++)
        line(cx, hor, cx + i * 16, 168, CLR_INDIGO);   // verticals fanning out
    for (int yy = hor + 2, stepy = 3; yy < 168; yy += stepy, stepy += 2)
        line(35, yy, 135, yy, CLR_TRUE_BLUE);
    rectfill(34, 52, 102, 4, CLR_DARKER_BLUE);     // re-cap the top edge
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_PINK);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_PINK);
        char l2[32];
        int dispKey = (sng.keyPc + gear(bar)) % 12;
        snprintf(l2, 32, "%.1f FM  %s min%s", sng.freq, RAD_PCNAME[dispKey],
                 gear(bar) ? "+" : "");
        print(l2, 154, 70, CLR_LIGHT_PEACH);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_LIGHT_PEACH);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_PINK);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        rad_chord_row(nowChord, 4, (int)(bar % 4), 152, 104, CLR_PINK);
        print(gear(bar) && sect == S_C ? "chorus+!" : SECTNAME[sect], 152, 120, CLR_PINK);
        rad_phrase_dots(232, 124, 8, bar / 8, CLR_PINK);
    }

    // knobs + power LED
    static const char *FEEL[4] = { "afterdark", "lounge", "floor", "peaktime" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_PINK);
    if (rad_knob_int(&tempo, 108, 134, 2, 218, 148, 9, "tempo", CLR_PINK)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_PINK)) apply_voicing();
    rad_power_led(radioOn, CLR_PINK, CLR_DARK_RED);

    rad_help_button(CLR_PINK);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next track (rolls a new seed)" },
            { "R",          "same track again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - shifts the density curve" },
            { "UP/DOWN",    "tempo of this track" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "H or ?",     "show / hide this help" },
        };
        static const char *NOTES[3] = {
            "spaghetti disco: minor-key, sequenced, doomed.",
            "the last chorus shifts up a semitone. of course.",
            "drums on loan from the tr-808 / house carts",
        };
        rad_help_panel("ITALO DISCO", HELP, 8, NOTES, 3, CLR_PINK);
    }
    ui_end();
}
