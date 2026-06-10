#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading, chrome)
#include "solo.h"    // the jam layer — a scale-locked lead over the progression
#include <stdio.h>
#include <math.h>

// ── POLO & PAN RADIO ──────────────────────────────────────────────────────
// An ARTIST station (radio-station-howto.md): the dial plays recognizably
// DIFFERENT songs of theirs, not one texture re-keyed. Polo & Pan's escapist
// French dream-pop is a mallet-and-percussion orchestra — marimba, balafon,
// glockenspiel, vibraphone — with pizzicato-string bounce, breathy flutes,
// vocoded "ah-ah" chants, a fat resonant analog mono-synth bass (their Korg
// MS-10), lush Solina pads, and a breezy Balearic house kit. Blind brief +
// the voice shop: docs/design/polopan-blind-brief.md.
//
// Five SONG ARCHETYPES (stolen-playbook chord brain #4 — game-music.md §"Same
// song every night?"). The seed rolls WHICH archetype plays; the archetype
// FIXES the groove, tempo, lead voice and form; then the seed varies key,
// progression and patterns WITHIN it. So "the Canopée one" sounds different
// every time but always like Canopée:
//
//   CANOPÉE  ~115  sunny tropical pluck-house  · STAR pizzicato+marimba bounce
//                  + vocoder "ah" · famous stacked-mallet outro
//   ANI KUNI ~122  driving Balearic chant-house · STAR vocoder chant cell +
//                  bright pluck arp build · big terraced drop / break
//   NANGA    ~100  dreamy downtempo exotica    · STAR breathy flute over
//                  balafon + congas · hypnotic A/B wander, no drop
//   TUNNEL   ~120  dark propulsive minimal     · STAR the fat resonant MS-10
//                  bass riff (the song IS the bass) + sparse glassy stabs
//   COEUR    ~108  warm romantic dream-pop     · STAR sung wordless lead over
//                  a lush Solina pad + glock twinkle + pizz comp
//
// Untapped engines this station reaches (no charted station used them before):
//   INSTR_PIPE (flute) · INSTR_VOICE (vocoder chant) · INSTR_BOWED (pizzicato).
// Plus the first station marimba/balafon on INSTR_MALLET.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (layers)   UP/DOWN tempo   T tone   J jam strip   H help
//
// Seed pins the COMPOSITION (archetype, key, progression, patterns, tempo,
// title); the PERFORMANCE (humanize, kit looseness, the jam) re-rolls live.

#define POLOPAN_SEED 0   // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_MAL   5   // MALLET — marimba / balafon / glockenspiel / vibraphone (per archetype)
#define I_PIZZ  6   // BOWED pizzicato — the bounce that IS the groove
#define I_STAR  7   // the archetype's topline STAR — engine swapped per song:
                    //   PIPE flute · VOICE vocoder · SINE glass / sung lead
#define I_BASS  8   // SAW — fat resonant MS-10 (riff) / round (round)
#define I_PAD   9   // SAW — the lush Solina string-machine wash
#define I_ARP   10  // PLUCK — the bright sampled-D50 arpeggio build
#define I_KICK  11  // SINE four-on-the-floor
#define I_CLAP  12  // NOISE backbeat clap
#define I_HAT   13  // NOISE hat + shaker swish
#define I_CONGA 14  // MEMBRANE conga / bongo (tropical hand percussion)
#define I_SOLO  15  // the jam-strip lead — celesta twinkle over the changes

// ── chords ── (the lush P&P voicing: every triad gets a 9 on top, like jingle)
enum { Q_MAJ, Q_MAJ7, Q_DOM7, Q_DOM9, Q_MIN, Q_MIN7, Q_MIN6, NQ };
static const char *QN[NQ] = { "", "maj7", "7", "9", "m", "m7", "m6" };
static const int QV[NQ][3] = {       // rootless 3-voice voicings (3rd / 7th / 9 color)
    { 4, 7, 14 },    // maj (add9)
    { 4, 11, 14 },   // maj7/9
    { 4, 10, 14 },   // 7/9
    { 4, 10, 14 },   // 9
    { 3, 7, 14 },    // m (add9)
    { 3, 10, 14 },   // m7/9
    { 3, 9, 14 },    // m6/9
};
static const int QT[NQ][4] = {       // chord tones (melodic accommodation + jam pad)
    { 0, 4, 7, 7 }, { 0, 4, 7, 11 }, { 0, 4, 7, 10 }, { 0, 4, 10, 14 },
    { 0, 3, 7, 7 }, { 0, 3, 7, 10 }, { 0, 3, 7, 9 },
};
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
static const int MAJSC[7] = { 0, 2, 4, 5, 7, 9, 11 };   // for diatonic melody roaming

// ── the five archetypes ─────────────────────────────────────────────────────
enum { A_CANOPEE, A_ANIKUNI, A_NANGA, A_TUNNEL, A_COEUR, NARCH };
static const char *ARCHNAME[NARCH] =
    { "Canopee", "Ani Kuni", "Nanga", "Tunnel", "Coeur Croise" };
static const int TEMPO_LO[NARCH] = { 112, 118,  96, 116, 104 };
static const int TEMPO_HI[NARCH] = { 118, 126, 104, 122, 112 };

// per-archetype VERSE / CHORUS progressions { semitone off from key, quality, borrowed }
typedef struct { int off, q, brw; } Ch;
static const Ch PROG[NARCH][2][4] = {
    // CANOPÉE — bright major: I–V–vi–IV  /  Imaj7–iii–IV–V9
    { { { 0, Q_MAJ, 0 }, { 7, Q_MAJ, 0 }, { 9, Q_MIN7, 0 }, { 5, Q_MAJ7, 0 } },
      { { 0, Q_MAJ7, 0 }, { 4, Q_MIN7, 0 }, { 5, Q_MAJ, 0 }, { 7, Q_DOM9, 0 } } },
    // ANI KUNI — modal minor vamp: i–bVII  /  i7–bVII–bVImaj7–bVII
    { { { 0, Q_MIN, 0 }, { 10, Q_MAJ, 1 }, { 0, Q_MIN, 0 }, { 10, Q_MAJ, 1 } },
      { { 0, Q_MIN7, 0 }, { 10, Q_MAJ, 1 }, { 8, Q_MAJ7, 1 }, { 10, Q_MAJ, 1 } } },
    // NANGA — dorian wander i–IV  /  mixolydian I–bVII
    { { { 0, Q_MIN7, 0 }, { 5, Q_MAJ, 0 }, { 0, Q_MIN7, 0 }, { 5, Q_MAJ, 0 } },
      { { 0, Q_MAJ, 0 }, { 10, Q_MAJ, 1 }, { 0, Q_MAJ, 0 }, { 10, Q_MAJ, 1 } } },
    // TUNNEL — minor drive i–bVI  /  i7–bVII
    { { { 0, Q_MIN, 0 }, { 0, Q_MIN, 0 }, { 8, Q_MAJ7, 1 }, { 8, Q_MAJ7, 1 } },
      { { 0, Q_MIN7, 0 }, { 10, Q_MAJ, 1 }, { 0, Q_MIN7, 0 }, { 10, Q_MAJ, 1 } } },
    // COEUR CROISÉ — warm major: Imaj7–vi–IV–V9  /  vi–IV–I–V9
    { { { 0, Q_MAJ7, 0 }, { 9, Q_MIN7, 0 }, { 5, Q_MAJ7, 0 }, { 7, Q_DOM9, 0 } },
      { { 9, Q_MIN7, 0 }, { 5, Q_MAJ7, 0 }, { 0, Q_MAJ7, 0 }, { 7, Q_DOM9, 0 } } },
};

// the form: 8 sections of 8 bars — section MEANINGS vary per archetype (the
// rolled-form axis: Canopée peaks on the stacked-mallet outro, Ani Kuni/Tunnel
// drop then break, Nanga wanders with no peak, Coeur is a song)
enum { S_INTRO, S_LOW, S_HIGH, S_PEAK, S_BREAK, S_OUTRO };
static const int FORM[NARCH][8] = {
    { S_INTRO, S_LOW,  S_LOW,  S_HIGH, S_LOW,   S_HIGH, S_PEAK, S_OUTRO }, // CANOPÉE
    { S_INTRO, S_LOW,  S_HIGH, S_PEAK, S_BREAK, S_HIGH, S_PEAK, S_OUTRO }, // ANI KUNI
    { S_INTRO, S_LOW,  S_LOW,  S_HIGH, S_LOW,   S_HIGH, S_LOW,  S_OUTRO }, // NANGA
    { S_INTRO, S_HIGH, S_HIGH, S_PEAK, S_BREAK, S_HIGH, S_PEAK, S_OUTRO }, // TUNNEL
    { S_INTRO, S_LOW,  S_HIGH, S_LOW,  S_HIGH,  S_HIGH, S_PEAK, S_OUTRO }, // COEUR
};
static const int SECT_BASE[6] = { 0, 1, 2, 3, 0, 0 }; // density floor per section

// ── the generated song ──────────────────────────────────────────────────────
typedef struct {
    int  arch;
    int  keyPc;
    Ch   verse[4], chorus[4];
    int  cellOn[6], cellN;     // topline rhythm cell (2 bars), seeded
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;                        // composition PRNG + history (radio.h)
static RadioClock clk = { -1, 0, 130.0 };    // schedule-ahead step clock (radio.h)
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)    rad_srnd(&rs, (n))
static int    tempo     = 115;
static int    intensity = 2;     // feel: shifts the density curve
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    songCount = 0;
static int    gv[3]     = { 64, 67, 71 };    // mallet/pizz comp voices
static bool   gvInit    = false;
static int    pv[3]     = { 52, 55, 59 };    // pad voices
static bool   pvInit    = false;
static int    kitLag    = 5;
static float  vu        = 0;
static int    melPitch  = 79;
static int    bassLast  = 43;
static char   nowChord[4][12];
static RadBand band;          // live A/B band panel (B) — Nanga's flute voice
static int    chFlute = -1;   // the flute chair: 0 = INSTR_PIPE model · 1 = clean SINE

// ── titles ──────────────────────────────────────────────────────────────────
static const char *TW[] = { "Canopee", "Mexico", "Plage", "Mirage", "Lagon",
    "Soleil", "Oiseau", "Voyage", "Brise", "Corail", "Nanga", "Tunnel",
    "Cyclorama", "Dorothy", "Feel Good" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.arch  = srnd(NARCH);
    sng.keyPc = srnd(12);
    int va = srnd(2), ca = srnd(2);          // which progression variant for verse / chorus
    for (int k = 0; k < 4; k++) {
        sng.verse[k]  = PROG[sng.arch][va][k];
        sng.chorus[k] = PROG[sng.arch][ca][k];
    }

    // topline cell: 3..6 onsets across 2 bars (the chant/flute/sung phrase)
    int dens = (sng.arch == A_NANGA) ? 28 : (sng.arch == A_ANIKUNI) ? 40 : 34;
    sng.cellN = 0;
    for (int s = 0; s < 31 && sng.cellN < 6; s += 2)
        if (srnd(100) < (s % 8 == 0 ? dens - 8 : dens)) sng.cellOn[sng.cellN++] = s;
    if (sng.cellN < 3) { sng.cellN = 3; sng.cellOn[0] = 0; sng.cellOn[1] = 10; sng.cellOn[2] = 20; }

    snprintf(sng.title, sizeof sng.title, "%s", TW[srnd(15)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = TEMPO_LO[sng.arch] + srnd(TEMPO_HI[sng.arch] - TEMPO_LO[sng.arch] + 1);
    bpm(tempo);
    songBase = (long)pos + 8;
    gvInit   = false;  pvInit = false;
    melPitch = 79;     bassLast = 43;
    kitLag   = rnd_between(2, 8);             // performance jitter (engine rnd)
    songCount++;
}

// ── per-song voicing — the archetype's STAR engine + per-archetype macros ────
static int toneSel = 2;
static const float TONEMUL[4] = { 0.55f, 0.78f, 1.0f, 1.28f };

static void voice_song(void) {
    int a = sng.arch;
    float tm = TONEMUL[toneSel];

    // ── I_STAR — the topline, engine swapped per archetype ──
    instrument_lfo(I_STAR, 0, LFO_PITCH,  5.0f, 0.0f);    // clear stale modulators
    instrument_lfo(I_STAR, 1, LFO_VOLUME, 0.7f, 0.0f);
    if (a == A_CANOPEE || a == A_ANIKUNI) {               // vocoder "ah" chant — INSTR_VOICE
        instrument(I_STAR, INSTR_VOICE, 40, 90, 7, 180);
        instrument_harmonics(I_STAR, 0.52f);              // VOWEL ~ "A"
        instrument_timbre(I_STAR, 0.42f);                 // SIZE
        instrument_morph(I_STAR, 0.55f);                  // EFFORT
        instrument_lfo(I_STAR, 0, LFO_PITCH, 5.5f, 0.22f);
        instrument_filter(I_STAR, FILTER_LOW, (int)(2600 * tm), 1);
    } else if (a == A_NANGA) {                            // breathy flute — A/B: PIPE model vs clean SINE
        if (chFlute >= 0 && band.c[chFlute].sel == 1) {   // B — faked breathy flute on SINE (in tune)
            instrument(I_STAR, INSTR_SINE, 25, 120, 5, 240);
            instrument_lfo(I_STAR, 0, LFO_PITCH, 5.2f, 0.18f);
            instrument_filter(I_STAR, FILTER_LOW, (int)(2600 * tm), 2);
        } else {                                          // A — the INSTR_PIPE blown-pipe model
            instrument(I_STAR, INSTR_PIPE, 45, 0, 6, 240);
            instrument_harmonics(I_STAR, 0.0f);           // fundamental flute
            instrument_timbre(I_STAR, 0.42f);             // a little air
            instrument_morph(I_STAR, 0.68f);              // focused embouchure
            instrument_lfo(I_STAR, 0, LFO_PITCH, 5.0f, 0.14f);
            instrument_filter(I_STAR, FILTER_LOW, (int)(2600 * tm), 1);
        }
    } else if (a == A_TUNNEL) {                           // glassy stabs (ondes/Cristal) — SINE
        instrument(I_STAR, INSTR_SINE, 8, 240, 4, 300);
        instrument_lfo(I_STAR, 0, LFO_PITCH,  5.0f, 0.10f);
        instrument_lfo(I_STAR, 1, LFO_VOLUME, 0.7f, 0.16f);
        instrument_filter(I_STAR, FILTER_LOW, (int)(3000 * tm), 2);
    } else {                                              // COEUR — sung wordless lead — SINE
        instrument(I_STAR, INSTR_SINE, 16, 220, 5, 260);
        instrument_lfo(I_STAR, 0, LFO_PITCH, 5.2f, 0.13f);
        instrument_filter(I_STAR, FILTER_LOW, (int)(2400 * tm), 2);
    }

    // ── I_MAL — the mallet chair (marimba / balafon / glock / vibes) ──
    if (a == A_CANOPEE)       { instrument_harmonics(I_MAL, 0.00f); instrument_timbre(I_MAL, 0.45f); instrument_morph(I_MAL, 0.35f); } // marimba
    else if (a == A_NANGA)    { instrument_harmonics(I_MAL, 0.10f); instrument_timbre(I_MAL, 0.72f); instrument_morph(I_MAL, 0.18f); } // balafon (new recipe)
    else if (a == A_ANIKUNI)  { instrument_harmonics(I_MAL, 0.90f); instrument_timbre(I_MAL, 0.85f); instrument_morph(I_MAL, 0.60f); } // glockenspiel
    else                      { instrument_harmonics(I_MAL, 0.25f); instrument_timbre(I_MAL, 0.50f); instrument_morph(I_MAL, 0.90f); } // vibraphone (Tunnel/Coeur)
    instrument_filter(I_MAL, FILTER_LOW, (int)(3000 * tm), 1);

    // ── I_BASS — fat resonant MS-10 (riff) vs round ──
    if (a == A_TUNNEL || a == A_ANIKUNI) {                // the Korg MS-10 riff bass
        instrument_filter(I_BASS, FILTER_LOW, (int)(720 * tm), 7);
        instrument_env(I_BASS, 0, ENV_CUTOFF, 0, 70, 1500);
        instrument_drive(I_BASS, 0.22f);
    } else {                                              // round, warm
        instrument_filter(I_BASS, FILTER_LOW, (int)(620 * tm), 2);
        instrument_env(I_BASS, 0, ENV_CUTOFF, 0, 110, 900);
        instrument_drive(I_BASS, 0.08f);
    }

    instrument_filter(I_PAD, FILTER_LOW, (int)(950 * tm), 2);
    instrument_filter(I_ARP, FILTER_LOW, (int)(3600 * tm), 2);
    instrument_filter(I_PIZZ, FILTER_LOW, (int)(2600 * tm), 1);
}

// ── form / harmony lookups ────────────────────────────────────────────────
static int sect_of(long bar) { long s = bar / 8; return (int)(s < 8 ? FORM[sng.arch][s] : S_OUTRO); }
static const Ch *prog_of(long bar) {
    int s = sect_of(bar);
    return (s == S_HIGH || s == S_PEAK || s == S_BREAK) ? sng.chorus : sng.verse;
}
static Ch  chord_at(long bar) { return prog_of(bar)[bar % 4]; }
static int root_pc(Ch c)      { return (sng.keyPc + c.off) % 12; }
static void chord_label(char *out, int n, Ch c) {
    snprintf(out, n, "%s%s", PCNAME[root_pc(c)], QN[c.q]);
}

// the bass, voice-led: each root lands on the octave copy nearest the last one
static int bass_peek(int pc) {
    int d = ((pc - bassLast) % 12 + 18) % 12 - 6;
    int n = bassLast + d;
    while (n < 36) n += 12;
    while (n > 52) n -= 12;
    return n;
}
static int bass_near(int pc) { return bassLast = bass_peek(pc); }

// nearest-tone voice leading for the mallet/pizz comp (radio.h shared block)
static void lead_voices(Ch c) { rad_lead_to(root_pc(c), QV[c.q], gv, 3, 60, 84, &gvInit); }

// MELODIC ACCOMMODATION — over a borrowed chord, only its tones; else the key
static int pick_mel(Ch c) {
    int bestM = melPitch, bestScore = -999;
    if (c.brw) {
        for (int t = 0; t < 4; t++) {
            int pc = (root_pc(c) + QT[c.q][t]) % 12;
            for (int oct = 6; oct <= 7; oct++) {
                int m = oct * 12 + pc;
                if (m < 72 || m > 89) continue;
                int score = 10 - rad_iabs(m - melPitch) + rnd(4);
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
                int score = 10 - rad_iabs(m - melPitch) + rnd(4);
                int rel = (pc - root_pc(c) + 12) % 12;
                if (rel == 0 || rel == 3 || rel == 4 || rel == 7) score += 3;
                if (m == melPitch) score -= 3;
                if (score > bestScore) { bestScore = score; bestM = m; }
            }
        }
    }
    melPitch = bestM;
    return bestM;
}

// density: arrangement curve × feel knob (radio.h rad_level)
static const int GAPS[4] = { 30, 16, 8, 0 };
static int level_of(long bar) { return rad_level(SECT_BASE[sect_of(bar)], intensity); }

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;                            // song over; update() rolls the next
    Ch   c    = chord_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  a    = sng.arch;

    if (step == 0 && bar % 8 == 0 && bar > 0) bpm(tempo + rnd(3) - 1);   // tape breath

    bool fourFloor = (a != A_NANGA);
    if (lvl >= 1) lead_voices(c);

    // ── BASS ──
    if (a == A_TUNNEL || a == A_ANIKUNI) {            // the MS-10 driving riff
        static const int RIFF[16] = { 1,0,1,0, 1,1,0,1, 0,1,0,1, 1,0,1,1 };
        if (lvl >= 1 && RIFF[step]) {
            int b = bass_near(root_pc(c));
            int n = b;
            if (step % 4 == 2 && chance(45) && b + 12 <= 52) n = b + 12;
            int vol = (step % 4 == 0) ? 4 : 3;
            schedule_hit(dly, n, I_BASS, vol, (int)(stepMs * 1.7));
            vu += vol * 0.5f;
        }
    } else if (lvl >= 1 && (step == 0 || step == 6 || step == 8 || step == 14)) {
        int b = bass_near(root_pc(c)), n = b, vol = 4; bool play = true;
        if (step == 6)  { vol = 2; play = chance(45); }
        if (step == 8)  { vol = 3; if (chance(40)) n = (b + 7 <= 52) ? b + 7 : b - 5; }
        if (step == 14) {                            // chromatic approach to next root
            int nb = bass_peek(root_pc(chord_at(bar + 1)));
            play = nb != b && rad_iabs(nb - b) <= 2 && chance(60);
            n = nb > b ? nb - 1 : nb + 1; vol = 2;
        }
        if (play) { schedule_hit(dly, n, I_BASS, vol, (int)(stepMs * 4.5)); vu += vol * 0.5f; }
    }

    // ── KIT ──
    if (lvl >= 1) {
        int lag = dly + kitLag + rnd(3);
        if (fourFloor) {
            if (step % 4 == 0 && (lvl >= 2 || step == 0)) { schedule_hit(lag, 36, I_KICK, 4, 90); vu += 1.2f; }
        } else if (step == 0 || step == 10) { schedule_hit(lag, 36, I_KICK, 3, 100); vu += 1.0f; } // Nanga lope
        if (lvl >= 2 && (step == 4 || step == 12)) { schedule_hit(lag, 60, I_CLAP, 3, 50); vu += 1.3f; }
        if (step % 2 == 0) schedule_hit(lag, 80, I_HAT, (step % 4 == 0) ? 1 : 2, (a == A_TUNNEL) ? 14 : 26);
        if ((a == A_CANOPEE || a == A_NANGA) && step % 2 == 1 && lvl >= 2)
            schedule_hit(lag, 84, I_HAT, 1, 32);     // the breezy shaker swish
    }
    // congas — Nanga + Canopée tumbao
    if (lvl >= 2 && (a == A_NANGA || a == A_CANOPEE)) {
        static const int TUMBAO[16] = { 0,0,1,0, 0,1,0,0, 1,0,0,1, 0,0,1,0 };
        if (TUMBAO[step]) schedule_hit(dly + kitLag, (step % 8 == 2) ? 57 : 52, I_CONGA, 2, 200);
    }

    // ── MALLET + PIZZ bounce — the Polo & Pan signature ──
    if (a == A_CANOPEE && lvl >= 1) {                // pizz + marimba 16th bounce
        static const int BNC[16] = { 0,2,1,2, 0,1,2,1, 0,2,1,0, 1,2,0,1 };
        if (!chance(GAPS[lvl])) {
            int vi = gv[BNC[step] % 3];
            schedule_hit(dly + rnd(4), vi, I_PIZZ, (step % 4 == 0) ? 3 : 2, (int)(stepMs * 1.4));
            if (step % 2 == 0)
                schedule_hit(dly + rnd(5), vi, I_MAL, (step % 4 == 0) ? 3 : 2, (int)(stepMs * 2.2));
            vu += 1.0f;
        }
        if (sect == S_PEAK && step % 2 == 0)         // the famous stacked-mallet outro (octaves)
            schedule_hit(dly + rnd(6), gv[(step / 2) % 3] + 12, I_MAL, 2, (int)(stepMs * 2.4));
    } else if (a == A_NANGA && lvl >= 1) {           // balafon sparse syncopation
        static const int BAL[16] = { 1,0,0,1, 0,0,1,0, 1,0,1,0, 0,1,0,0 };
        if (BAL[step] && !chance(GAPS[lvl] / 2)) {
            int vi = gv[step % 3];
            schedule_hit(dly + rnd(5), vi, I_MAL, 3, (int)(stepMs * 2.6));
            vu += 0.8f;
        }
    } else if (a == A_COEUR && lvl >= 1) {           // pizz comp + glock/vibe twinkle on changes
        if (step == 0 || step == 4 || step == 8 || step == 12) {
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + k * 10 + rnd(5), gv[k], I_PIZZ, 2, (int)(stepMs * 3.2));
            vu += 1.4f;
        }
        if (step == 0 && (bar % 2 == 0) && lvl >= 2)
            schedule_hit(dly + rnd(8), gv[2] + 12, I_MAL, 2, (int)(stepMs * 6));
    } else if (a == A_ANIKUNI && lvl >= 2) {         // glock doubles the chant on the beat
        if (step % 4 == 0)
            schedule_hit(dly + rnd(4), gv[(step / 4) % 3] + 12, I_MAL, 2, (int)(stepMs * 1.6));
    }

    // ── ARP — the bright build (Ani Kuni), light sparkle (Tunnel peak) ──
    if (a == A_ANIKUNI && lvl >= 2) {
        if (step % 2 == 0 || lvl >= 3) {
            int vi = gv[step % 3] + 12;
            schedule_hit(dly + rnd(3), vi, I_ARP, 2, (int)(stepMs * 1.2));
        }
    } else if (a == A_TUNNEL && sect == S_PEAK && step % 4 == 2) {
        schedule_hit(dly + rnd(3), gv[(step / 4) % 3] + 12, I_ARP, 2, (int)(stepMs * 1.2));
    }

    // ── PAD — the lush Solina wash, held from the bar top ──
    if (step == 0) {
        bool padOn = (a == A_COEUR || a == A_TUNNEL || a == A_NANGA) ? (lvl >= 1) : (lvl >= 2);
        if (padOn) {
            rad_lead_to(root_pc(c), QV[c.q], pv, 3, 48, 67, &pvInit);
            for (int k = 0; k < 3; k++) schedule_hit(dly + k * 8, pv[k], I_PAD, 2, (int)(stepMs * 15));
            vu += 1.4f;
        }
    }

    // ── STAR topline — the seeded cell, re-pitched with accommodation ──
    // (lays out while the player has the jam strip open)
    if (lvl >= 2 && sect != S_BREAK && !solo_open()) {
        int s32 = (int)(s % 32);
        for (int i = 0; i < sng.cellN; i++)
            if (sng.cellOn[i] == s32 && chance(85)) {
                int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - s32 : 32 - s32;
                int dur = (int)(gap * stepMs * 0.85);
                int maxd = (a == A_NANGA || a == A_COEUR) ? 1600 : 1100;
                if (dur > maxd) dur = maxd;
                int vol = (a == A_TUNNEL) ? 2 : 3;
                schedule_hit(dly + 12 + rnd(8), pick_mel(c), I_STAR, vol, dur);
                vu += 1.4f;
            }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_MAL, INSTR_MALLET, 1, 0, 7, 1200);          // the mallet chair
    instrument_filter(I_MAL, FILTER_LOW, 3000, 1);

    instrument(I_PIZZ, INSTR_BOWED, 1, 0, 7, 400);           // pizzicato strings
    eng_tune(I_PIZZ, 0, 1.0f);                               // BOWED idx0>=0.5 = PIZZICATO mode
    instrument_harmonics(I_PIZZ, 0.30f);
    instrument_timbre(I_PIZZ, 0.42f);
    instrument_morph(I_PIZZ, 0.30f);
    instrument_filter(I_PIZZ, FILTER_LOW, 2600, 1);

    instrument(I_STAR, INSTR_SINE, 16, 220, 5, 260);         // overridden per archetype

    instrument(I_BASS, INSTR_SAW, 2, 160, 4, 120);           // MS-10 / round bass

    instrument(I_PAD, INSTR_SAW, 300, 500, 6, 900);          // Solina string pad
    instrument_filter(I_PAD, FILTER_LOW, 950, 2);

    instrument(I_ARP, INSTR_PLUCK, 0, 200, 0, 140);          // bright sampled-D50 arp
    instrument_harmonics(I_ARP, 0.45f);
    instrument_timbre(I_ARP, 0.70f);
    instrument_filter(I_ARP, FILTER_LOW, 3600, 2);

    instrument(I_KICK, INSTR_SINE, 1, 90, 0, 40);
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 45, 14);

    instrument(I_CLAP, INSTR_NOISE, 0, 110, 0, 50);
    instrument_filter(I_CLAP, FILTER_BAND, 1100, 5);

    instrument(I_HAT, INSTR_NOISE, 0, 18, 0, 30);
    instrument_filter(I_HAT, FILTER_HIGH, 7500, 2);

    instrument(I_CONGA, INSTR_MEMBRANE, 1, 0, 7, 200);
    instrument_harmonics(I_CONGA, 0.55f);
    instrument_timbre(I_CONGA, 0.35f);
    instrument_morph(I_CONGA, 0.15f);

    instrument(I_SOLO, INSTR_MALLET, 1, 0, 7, 1200);         // jam-strip celesta twinkle
    instrument_harmonics(I_SOLO, 0.50f);
    instrument_timbre(I_SOLO, 0.55f);
    instrument_morph(I_SOLO, 0.45f);
    instrument_filter(I_SOLO, FILTER_LOW, 3500, 2);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        chFlute = rad_chair(&band, "flute (Nanga)", "pipe", "sine", 0, 0);  // live A/B
        if (POLOPAN_SEED) new_song(pos, POLOPAN_SEED);
        else              new_song(pos, 0);
        rad_hist_log(&rs);
        voice_song();
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 88, 130, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    { new_song(pos, 0); rad_hist_log(&rs); voice_song(); }
    if (ev & RAD_EV_REPLAY) { new_song(pos, sng.seed);            voice_song(); }
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) { new_song(pos, s); voice_song(); } }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) { new_song(pos, s); voice_song(); } }
    if (ev & RAD_EV_POWER)  { if (!radioOn) note_off_all(); else scheduled = (long)pos; }
    if (ev & RAD_EV_TONE)   voice_song();
    if (rad_band_input(&band, &showHelp) >= 0) voice_song();   // B — flute A/B, re-voice live

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= 64L * 16) { new_song(pos, 0); rad_hist_log(&rs); voice_song(); }

        long bar = songStep >= 0 ? songStep / 16 : 0;
        const Ch *p = prog_of(bar);
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 12, p[i]);
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long bar = ss >= 0 ? ss / 16 : 0;
    static const char *SN[6] = { "intro", "low", "high", "peak", "break", "outro" };
    watch("song",  "%d", songCount);
    watch("arch",  "%s", ARCHNAME[sng.arch]);
    watch("tempo", "%d", tempo);
    watch("sect",  "%s", SN[sect_of(bar)]);
    watch("chord", "%s", nowChord[(int)(bar % 4)]);
#endif
}

// ── draw — the radio on a sunny terrace ─────────────────────────────────────
// chassis from radio.h; the window art (a tinted sky, a sun, a palm canopy and
// a strip of sea — the mood tints per archetype) is polopan's own.
static const int SKY_TOP[NARCH] = { CLR_BLUE,        CLR_DARK_PURPLE, CLR_DARK_ORANGE,
                                    CLR_DARKER_BLUE, CLR_TRUE_BLUE };
static const int SKY_BOT[NARCH] = { CLR_PEACH,       CLR_PINK,        CLR_YELLOW,
                                    CLR_DARK_PURPLE, CLR_PEACH };

void draw(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int a = sng.arch;

    rad_body(CLR_BLUE_GREEN, CLR_ORANGE);    // teal terrace plastic, sunny-orange accent
    rad_dial(sng.freq, CLR_ORANGE);

    // the window — a tinted sky band + a sun + a palm canopy silhouette + sea
    int wx = 34, wy = 52, ww = 102, wh = 116;
    rectfill(wx, wy, ww, wh / 2, SKY_TOP[a]);
    rectfill(wx, wy + wh / 2, ww, wh / 2, SKY_BOT[a]);
    int sunx = wx + 70, suny = wy + 40 + (a == A_TUNNEL ? 18 : 0);
    circfill(sunx, suny, 11, a == A_TUNNEL ? CLR_LIGHT_PEACH : CLR_YELLOW);   // sun (moon for Tunnel)
    circfill(wx + 50, wy + wh - 20, 26, CLR_BLUE_GREEN);                      // lagoon
    rectfill(wx, wy + wh - 14, ww, 14, CLR_BLUE_GREEN);                       // sea
    // palm canopy fronds (the "Canopée") swaying with the beat
    float sway = sinf(frame() * 0.04f) * 3.0f;
    for (int i = 0; i < 5; i++) {
        int px = wx + 18 + i * 18, py = wy + 30;
        line(px, py, px + (int)(sway + (i - 2) * 6), py - 22, CLR_DARK_GREEN);
        circfill(px + (int)(sway + (i - 2) * 6), py - 22, 5, CLR_GREEN);
    }
    rect(wx, wy, ww, wh, CLR_DARKER_PURPLE);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_DARK_GREY);
    if (radioOn) {
        char l1[40];
        snprintf(l1, sizeof l1, "%s", sng.title);
        print(l1, 154, 58, CLR_ORANGE);
        print(ARCHNAME[a], 154, 70, CLR_LIGHT_PEACH);
        char l2[40];
        snprintf(l2, sizeof l2, "%s %dbpm #%06X", PCNAME[sng.keyPc], tempo, sng.seed & 0xFFFFFF);
        print(l2, 154, 82, CLR_DARK_PEACH);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        int ci = (int)(bar % 4), x = 152;
        const Ch *p = prog_of(bar);
        for (int i = 0; i < 4; i++) {
            char lbl[12]; chord_label(lbl, 12, p[i]);
            int cw = text_width(lbl);
            if (x + cw > 292) break;
            if (i == ci) { rectfill(x - 2, 102, cw + 4, 12, CLR_ORANGE); print(lbl, x, 104, CLR_BLACK); }
            else         print(lbl, x, 104, CLR_LIGHT_GREY);
            x += cw + 8;
        }
        static const char *SN[6] = { "intro", "verse", "hook", "peak", "break", "outro" };
        print(SN[sect_of(bar)], 152, 120, CLR_ORANGE);
        rad_phrase_dots(208, 124, 8, bar / 8, CLR_ORANGE);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_ORANGE);
    }

    // knobs + power LED
    static const char *FEEL[4] = { "hush", "breezy", "sunny", "aglow" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_ORANGE);
    if (rad_knob_int(&tempo, 88, 130, 2, 218, 148, 9, "tempo", CLR_ORANGE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_ORANGE)) voice_song();
    rad_power_led(radioOn, CLR_ORANGE, CLR_DARK_ORANGE);

    rad_help_button(CLR_ORANGE);
    rad_footer("SPACE next song   B band   J jam   H help");

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next song (rolls a new seed)" },
            { "R",          "same song again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - shifts the density curve" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "B",          "band - A/B the Nanga flute voice" },
            { "J / M / H",  "jam / power / this help" },
        };
        static const char *NOTES[3] = {
            "5 of their songs: Canopee, Ani Kuni,",
            "Nanga, Tunnel, Coeur Croise - the dial",
            "rolls which one, then varies it.",
        };
        rad_help_panel("POLO & PAN RADIO", HELP, 8, NOTES, 3, CLR_ORANGE);
    }
    rad_band_panel(&band, CLR_ORANGE);   // B — the flute A/B overlay (shares the help spot)

    // the jam strip — pentatonic over the key, the current chord's tones lit
    int chord[4]; {
        Ch c = chord_at(bar);
        for (int k = 0; k < 4; k++) chord[k] = (root_pc(c) + QT[c.q][k]) % 12;
    }
    static const int PENT[5] = { 0, 2, 4, 7, 9 };
    SoloCtx jc = { sng.keyPc, PENT, 5, chord, 4, I_SOLO, 72, 91, false, SOLO_Y_BRIGHT, 1400, 5500 };
    solo_strip(&jc, 28, 170, 250, 18, CLR_ORANGE);

    ui_end();
}
