/* de:meta
{
  "slug": "wba",
  "title": "wba fm",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing"
  ],
  "lineage": "An artist-station (stolen-playbook archetypes, kin to air); the first station to use the ampcab.h guitar amp/cab bundle and a clean-guitar pedalboard (slapback + small room + chorus), and a negative-space guitar-bass duet arrangement.",
  "homage": "The Whitest Boy Alive (Erlend Oye)",
  "description": "The Whitest Boy Alive station - Erlend Oye's band (Dreams / Rules), an ARTIST station: the dial plays recognizably DIFFERENT songs OF THEIRS, not one texture re-keyed. The whole identity is RESTRAINT and SPACE - every instrument is dry and has room, nothing crowds; machine-tight precision meets live-band warmth. Built on the stolen-playbook chord brain: FIVE SONG ARCHETYPES, each a cited WBA track that fixes its groove, tempo, lead and form. BURNING (aeolian four-on-floor disco, driving, clean guitar chops + a pumping bass); GOLDEN CAGE (a dorian melancholic mid-pulse where the BASS carries the hook); 1517 (bright major pop, bouncy, guitar/bass interplay); COURAGE (gentle major maj7, slower, Rhodes-forward, lots of air); DONE WITH YOU (a funky dorian vamp, choppy palm-mute guitar + syncopated bass). THE TEXTURE is the negative-space DUET: the clean guitar's palm-muted chops and the melodic co-lead bass interlock with GAPS, over a tight clean kit, with warm Rhodes + a soft vocal on top. The guitar is a clean INSTR_GUITAR (steel string) through the CLEAN amp/cab voicing (ampcab.h: soft-clip, scooped-bright, light power-amp sag) + a slapback echo + a small warm room + a whisper of chorus - NO distortion, the restraint is the sound; the whole band is glued with gentle tape + bus compression. The window is a bare Scandinavian stage: four instrument blocks (guitar/bass/drums/keys) lit only when each plays, in lots of black space. SPACE next song, R same tune, [ ] history, LEFT/RIGHT feel (bare/room/warm/full), UP/DOWN tempo, T tone, B band (amp clean/chime/crunch, keys rhodes/wurli, kit tight/brushed), M power, H help. Pin via WBA_SEED."
}
de:meta */
// ── WBA FM — The Whitest Boy Alive ───────────────────────────────────────────
// Erlend Øye's band (Dreams 2006 / Rules 2009). NOT a genre but an ARTIST: the dial
// plays recognizably DIFFERENT songs *of theirs* — SPACE rolls "the Burning one" or
// "the Golden Cage one". The whole identity is RESTRAINT and SPACE: every instrument
// is dry and has room, nothing crowds. Machine-tight precision + live-band warmth.
// Blind brief: docs/design/wba-blind-brief.md.
//
// THE BRAIN (stolen-playbook chord brain #4, game-music.md): five SONG ARCHETYPES,
// each a cited WBA track encoded as a template progression that ALSO fixes its groove,
// tempo, lead and form. The seed varies key/cell/patterns within an archetype.
//   BURNING   — four-on-floor disco, driving; clean guitar chops + pumping bass
//   GOLDEN    — "Golden Cage": melancholic mid pulse, the BASS carries the hook
//   _1517     — bouncy, bright major; guitar/bass interplay up front
//   COURAGE   — slower, gentle, Rhodes-forward, lots of air
//   DONE      — "Done With You": funky, choppy palm-mute guitar + syncopated bass
//
// THE TEXTURE: the clean guitar and the melodic bass INTERLOCK with gaps (a
// negative-space duet) over a tight clean kit; warm Rhodes + a soft vocal on top.
// The guitar is a clean INSTR_GUITAR through the CLEAN amp voicing (ampcab.h) + a
// slapback echo + a small warm room + a touch of chorus — no distortion (the restraint).
//
//   SPACE next song   R play it again   [ ] history   M power
//   LEFT/RIGHT feel (space)   UP/DOWN tempo   T tone   B band   H help

#include "studio.h"
#include "radio.h"
#include "ampcab.h"   // the guitar amp/cab voicings (CLEAN..LO-FI) — WBA wants CLEAN
#include <stdio.h>
#include <math.h>

#define WBA_SEED 0   // pin a favourite tune here (0 = free-roaming radio)

// ── instrument slots (5..12) ────────────────────────────────────────────────
#define I_GTR  5     // clean electric guitar (INSTR_GUITAR → CLEAN amp)
#define I_BASS 6     // the melodic co-lead bass (round, fingered)
#define I_EP   7     // Rhodes / Wurlitzer (INSTR_EPIANO)
#define I_LEAD 8     // soft vocal topline (INSTR_VOICE)
#define I_ORG  9     // a little analog synth / organ stab
#define I_KICK 10
#define I_SNR  11
#define I_HAT  12

// ── chord qualities + rootless 3-voice voicings (3rd / 7th / colour) ───────
enum { Q_MAJ, Q_MAJ7, Q_DOM7, Q_DOM9, Q_MIN, Q_MIN7, Q_MIN9, Q_MIN6, NQ };
static const char *QN[NQ] = { "", "maj7", "7", "9", "m", "m7", "m9", "m6" };
static const int QV[NQ][3] = {
    { 4, 7, 14 }, { 4, 11, 14 }, { 4, 10, 14 }, { 4, 10, 14 },
    { 3, 7, 14 }, { 3, 10, 14 }, { 3, 10, 14 }, { 3, 9, 14 },
};
static const int SCALES[3][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },    // major
    { 0, 2, 3, 5, 7, 8, 10 },    // aeolian
    { 0, 2, 3, 5, 7, 9, 10 },    // dorian
};

// ── the five song archetypes ────────────────────────────────────────────────
enum { A_BURN, A_GOLD, A_1517, A_COUR, A_DONE, NA };
enum { HM_GUITAR, HM_RHODES };
typedef struct { int off, q; } Ch;
static const Ch PROG[NA][4] = {
    // BURNING   i7 – bVII – bVImaj7 – bVII   (aeolian disco drive)
    { { 0, Q_MIN7 }, { 10, Q_MAJ }, { 8, Q_MAJ7 }, { 10, Q_MAJ } },
    // GOLDEN    i9 – bVImaj7 – bVII – i9      (dorian melancholy, bass hook)
    { { 0, Q_MIN9 }, { 8, Q_MAJ7 }, { 10, Q_DOM9 }, { 0, Q_MIN9 } },
    // 1517      Imaj7 – vi7 – IVmaj7 – V9     (bright major pop)
    { { 0, Q_MAJ7 }, { 9, Q_MIN7 }, { 5, Q_MAJ7 }, { 7, Q_DOM9 } },
    // COURAGE   Imaj7 – IVmaj7 – vi7 – V7     (gentle major)
    { { 0, Q_MAJ7 }, { 5, Q_MAJ7 }, { 9, Q_MIN7 }, { 7, Q_DOM7 } },
    // DONE      i9 – IV9 – i9 – IV9           (funky dorian vamp)
    { { 0, Q_MIN9 }, { 5, Q_DOM9 }, { 0, Q_MIN9 }, { 5, Q_DOM9 } },
};
static const struct {
    const char *name, *track, *vibe;
    int scale, harm, tlo, tspan, leadLo, leadHi;
} ARCH[NA] = {
    { "BURNING",   "~ Burning",        "four-on-floor", 1, HM_GUITAR, 116, 8, 64, 80 },
    { "GOLDEN",    "~ Golden Cage",    "the bass hook",  2, HM_RHODES, 106, 8, 62, 78 },
    { "1517",      "~ 1517",           "bouncy & bright",0, HM_GUITAR, 118, 8, 66, 82 },
    { "COURAGE",   "~ Courage",        "gentle, Rhodes", 0, HM_RHODES,  98, 8, 60, 76 },
    { "DONE",      "~ Done With You",  "funky chop",     2, HM_GUITAR, 110, 8, 64, 80 },
};

// ── the form — 8-bar sections; the seed rolls a length ──────────────────────
enum { S_INTRO, S_A, S_B, S_BREAK, S_OUT };
#define MAXSECT 12
static const struct { int n; int s[MAXSECT]; const char *name; } FORMS[] = {
    { 5,  { S_INTRO, S_A, S_B, S_A, S_OUT }, "single" },
    { 8,  { S_INTRO, S_A, S_B, S_A, S_BREAK, S_B, S_A, S_OUT }, "album" },
    { 11, { S_INTRO, S_A, S_B, S_A, S_B, S_BREAK, S_A, S_B, S_A, S_B, S_OUT }, "extended" },
};
#define NFORMS 3
static const char *SECTNAME[5] = { "intro", "verse", "chorus", "break", "outro" };

// ── the generated song ──────────────────────────────────────────────────────
typedef struct {
    int   arch, keyPc, form;
    Ch    prog[4];
    int   cellOn[6], cellN;
    int   ampV;                 // amp voicing (CLEAN default; B can dirty it)
    char  title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 125.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)   rad_srnd(&rs, (n))

static int   tempo     = 112;
static int   intensity = 1;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 2;
static int   songCount = 0;
static float vu = 0;
static float gtrVu = 0, basVu = 0, drVu = 0, epVu = 0;   // per-voice meters for the stage window

static int   gvCmp[3] = { 60, 64, 67 }; static bool cmpInit = false;
static int   gvGtr[3] = { 60, 64, 67 }; static bool gtrInit = false;
static int   melPitch = 72, bassLast = 36;
static char  nowChord[4][8];
static int   leadCut = 2200, bassCut = 600;

static RadBand band;
static int chAmp, chKeys, chKit;

static int iabs(int v) { return v < 0 ? -v : v; }
static void apply_chair(int idx);

// ── song generation ─────────────────────────────────────────────────────────
static const char *TW1[] = { "Burning", "Golden", "Intentions", "Courage", "Keep",
    "Done", "Rollercoaster", "Fireworks", "Island", "High", "Promise", "Figures" };
static const char *TW2[] = { "Cage", "a Secret", "With You", "Coaster", "On", "Again",
    "in Love", "Dance", "Time", "Bow Down", "Bossanova", "Fox" };

static void roll_cell(int dense) {
    sng.cellN = 0;
    for (int s = 0; s < 31 && sng.cellN < 6; s += 2)
        if (srnd(100) < (s % 8 == 0 ? dense - 8 : dense)) sng.cellOn[sng.cellN++] = s;
    if (sng.cellN < 3) { sng.cellN = 3; sng.cellOn[0] = 4; sng.cellOn[1] = 14; sng.cellOn[2] = 24; }
}

static void setup_voices(int arch) {
    bassCut = (arch == A_GOLD) ? 720 : (arch == A_DONE ? 640 : 560);  // Golden's bass sings higher
    instrument_filter(I_BASS, FILTER_LOW, bassCut, 2);
    leadCut = 2200 + (arch == A_1517 ? 400 : 0);
    instrument_filter(I_LEAD, FILTER_LOW, leadCut, 2);
}

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);
    sng.arch  = srnd(NA);
    sng.keyPc = srnd(12);
    for (int k = 0; k < 4; k++) sng.prog[k] = PROG[sng.arch][k];
    if (srnd(100) < 30)                                    // sometimes lift a maj to maj7 (the soul colour)
        for (int k = 0; k < 4; k++) if (sng.prog[k].q == Q_MAJ) sng.prog[k].q = Q_MAJ7;
    roll_cell(sng.arch == A_COUR ? 24 : 32);               // Courage leaves more space
    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;
    sng.form = srnd(100) < 32 ? 0 : srnd(100) < 72 ? 1 : 2;
    tempo = ARCH[sng.arch].tlo + srnd(ARCH[sng.arch].tspan);
    bpm(tempo);
    setup_voices(sng.arch);
    songBase = (long)pos + 8;
    cmpInit = gtrInit = false;
    melPitch = (ARCH[sng.arch].leadLo + ARCH[sng.arch].leadHi) / 2; bassLast = 36;
    songCount++;
}
static void fresh_song(double pos) { new_song(pos, 0); rad_hist_log(&rs); }

// ── form / harmony lookups ─────────────────────────────────────────────────
static int  form_sects(void) { return FORMS[sng.form].n; }
static long song_bars(void)  { return (long)FORMS[sng.form].n * 8; }
static int  sect_of(long bar) { int x = (int)(bar / 8), n = FORMS[sng.form].n; return x < n ? FORMS[sng.form].s[x] : S_OUT; }
static Ch   chord_at(long bar) { return sng.prog[bar % 4]; }
static int  root_pc(Ch c)     { return (sng.keyPc + c.off) % 12; }
static void chord_label(char *out, int n, Ch c) { snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(c)], QN[c.q]); }
static int  level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUT) ? 0 : (s == S_A ? 1 : 2);
    return rad_level(base, intensity);
}
static void lead_comp(Ch c, int *gv, bool *init, int lo, int hi) { rad_lead_to(root_pc(c), QV[c.q], gv, 3, lo, hi, init); }

static int pick_mel(Ch c, int lo, int hi) {
    const int *sc = SCALES[ARCH[sng.arch].scale];
    int rt = root_pc(c), best = melPitch, bd = 999;
    for (int t = 0; t < 7; t++) {
        int pc = (sng.keyPc + sc[t]) % 12, rel = (pc - rt + 12) % 12;
        bool ct = (rel == 0 || rel == 3 || rel == 4 || rel == 7 || rel == 10);
        for (int o = lo / 12; o <= hi / 12 + 1; o++) {
            int m = o * 12 + pc; if (m < lo || m > hi) continue;
            int d = iabs(m - melPitch) + rnd(3) + (ct ? 0 : 4); if (m == melPitch) d += 3;
            if (d < bd) { bd = d; best = m; }
        }
    }
    melPitch = best; return best;
}
static int bass_peek(int pc, int lo, int hi) { return rad_bass_to(pc, bassLast, lo, hi); }
static int bass_near(int pc, int lo, int hi) { return bassLast = bass_peek(pc, lo, hi); }

// ── the step player — the restraint duet: clean guitar chops ↔ melodic bass ──
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= song_bars()) return;
    int  cs   = (int)(s % 32);
    int  bib  = (int)(bar % 8);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  arch = sng.arch;
    Ch   c    = chord_at(bar);
    bool brk  = (sect == S_BREAK);
    int  bLo  = 31, bHi = 47;

    bool inDrums = (sect != S_INTRO || bib >= 1);
    bool inBass  = (sect != S_INTRO || bib >= 1);
    bool inHarm  = (sect != S_INTRO || bib >= 3) && !brk;
    bool inLead  = (sect != S_INTRO || bib >= 5) && !brk;

    // ── THE KIT — tight, clean, machine-precise ──
    if (inDrums) {
        bool kick, clap = (step == 4 || step == 12);
        if      (arch == A_BURN) kick = (step % 4 == 0);                  // four-on-floor
        else if (arch == A_1517) kick = (step == 0 || step == 6 || step == 8);
        else if (arch == A_DONE) kick = (step == 0 || step == 7 || step == 10);
        else if (arch == A_COUR) kick = (step == 0 || step == 8);
        else                     kick = (step == 0 || step == 10);       // Golden steady pulse
        if (kick) { schedule_hit(dly, 36, I_KICK, 5, 100); drVu += 1.6f; vu += 1.2f; }
        if (clap) { schedule_hit(dly + rnd(2), 60, I_SNR, arch == A_COUR ? 3 : 4, 80); drVu += 1.4f; vu += 1.0f; }
        if (lvl >= 1 && arch != A_COUR && step % 2 == 0) {               // tight hats on the offbeats
            int v = (step % 8 == 4) ? 2 : 1;
            schedule_hit(dly + 1, 90, I_HAT, v, 22); drVu += 0.4f;
        }
        if (arch == A_COUR && lvl >= 1 && (step == 6 || step == 14))
            schedule_hit(dly + rnd(4), 90, I_HAT, 1, 40);
    }

    // ── THE BASS — the melodic co-lead; Golden's is the busy hook ──
    if (inBass) {
        int rt = root_pc(c);
        if (arch == A_GOLD) {                                            // the rolling melodic hook
            static const int ROLL[8] = { 0, 4, 6, 10, 14, 16, 22, 26 };
            static const int DEG[8]  = { 0, 7, 10, 0, 5, 7, 0, 3 };
            for (int i = 0; i < 8; i++) if (ROLL[i] == cs) {
                bassLast = bass_peek((rt + DEG[i]) % 12, bLo, bHi + 7);
                schedule_hit(dly + 6, bassLast, I_BASS, i == 0 ? 5 : 3, (int)(stepMs * 1.8)); basVu += 1.3f; vu += 1.0f;
            }
        } else if (arch == A_DONE) {                                     // funky syncopated
            static const int RIFF[5] = { 0, 3, 6, 10, 14 };
            for (int i = 0; i < 5; i++) if (RIFF[i] == step) {
                int n = bass_near(rt, bLo, bHi);
                schedule_hit(dly + 5, n, I_BASS, i == 0 ? 5 : 4, (int)(stepMs * 1.8)); basVu += 1.2f; vu += 0.9f;
            }
        } else if (arch == A_BURN) {                                     // disco octave-ish pump
            if (step % 2 == 0) {
                int n = bass_near(rt, bLo, bHi);
                schedule_hit(dly + 4, n, I_BASS, step % 4 == 0 ? 5 : 3, (int)(stepMs * 1.5)); basVu += 1.0f; vu += 0.8f;
            }
        } else {                                                         // 1517 / Courage — rooted, with a passing note
            if (step == 0 || step == 8) {
                int n = bass_near(rt, bLo, bHi);
                schedule_hit(dly, n, I_BASS, step == 0 ? 5 : 4, (int)(stepMs * 4.5)); basVu += 0.9f; vu += 0.7f;
            } else if (step == 14 && chance(55)) {
                int nb = bass_peek(root_pc(chord_at(bar + 1)), bLo, bHi);
                if (iabs(nb - bassLast) <= 2 && nb != bassLast)
                    schedule_hit(dly, nb > bassLast ? nb - 1 : nb + 1, I_BASS, 2, (int)(stepMs * 1.5));
            }
        }
    }

    // ── HARMONY — clean guitar chops OR Rhodes comp; they leave GAPS for the bass ──
    if (inHarm && lvl >= 1) {
        if (ARCH[arch].harm == HM_GUITAR) {
            // palm-muted chops on the offbeats (the funk/disco move) — short, dry, spaced
            bool chop = (arch == A_DONE) ? (step == 2 || step == 6 || step == 11 || (step == 14 && chance(60)))
                      : (arch == A_BURN) ? (step == 2 || step == 6 || step == 10 || step == 14)
                                         : (step == 4 || step == 12 || (step == 7 && chance(50)));  // 1517
            if (chop) {
                lead_comp(c, gvGtr, &gtrInit, 55, 76);
                for (int k = 0; k < 3; k++)
                    schedule_hit(dly + rnd(2), gvGtr[k], I_GTR, lvl >= 2 ? 4 : 3, (int)(stepMs * 1.2));  // short = the mute
                gtrVu += 1.4f; vu += 0.9f;
            }
        } else {                                                         // HM_RHODES — soft stabs, room
            bool comp = (arch == A_COUR) ? (step == 0 || (step == 8 && chance(60)))
                                         : (step == 0 || step == 6 || (step == 11 && chance(45)));
            if (comp) {
                lead_comp(c, gvCmp, &cmpInit, 54, 74);
                for (int k = 0; k < 3; k++)
                    schedule_hit(dly + k * 8 + rnd(3), gvCmp[k], I_EP, lvl >= 2 ? 3 : 2, (int)(stepMs * (arch == A_COUR ? 8 : 4)));
                epVu += 1.2f; vu += 0.8f;
            }
            // a dry guitar single-note answer in the gaps (the duet) — Golden/Courage
            if (lvl >= 2 && (step == 4 || step == 12) && chance(40)) {
                lead_comp(c, gvGtr, &gtrInit, 60, 79);
                schedule_hit(dly + rnd(3), gvGtr[rnd(3)], I_GTR, 2, (int)(stepMs * 1.6)); gtrVu += 0.8f;
            }
        }
        // a quiet organ/synth pad on the chorus of the brighter tunes
        if ((arch == A_1517 || arch == A_BURN) && sect == S_B && step == 0 && bar % 2 == 0) {
            lead_comp(c, gvCmp, &cmpInit, 60, 76);
            for (int k = 0; k < 3; k++) schedule_hit(dly + k * 10, gvCmp[k], I_ORG, 2, (int)(stepMs * 26));
            epVu += 0.5f;
        }
    }

    // ── THE VOCAL — the soft topline (the seeded cell, re-pitched) ──
    if (inLead && lvl >= 2) {
        bool active = (sect == S_B) || chance(45);
        if (active)
            for (int i = 0; i < sng.cellN; i++)
                if (sng.cellOn[i] == cs && chance(82)) {
                    int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - cs : 32 - cs;
                    int dur = (int)(gap * stepMs * 0.9); if (dur > 1500) dur = 1500;
                    schedule_hit(dly + 12 + rnd(6), pick_mel(c, ARCH[arch].leadLo, ARCH[arch].leadHi), I_LEAD, 3, dur);
                    vu += 1.0f;
                }
    }
}

// ── one-time setup ────────────────────────────────────────────────────────
static void setup_instruments(void) {
    chAmp  = rad_chair(&band, "amp",  "clean", "chime", "crunch", NULL);
    chKeys = rad_chair(&band, "keys", "rhodes", "wurli", NULL, NULL);
    chKit  = rad_chair(&band, "kit",  "tight", "brushed", NULL, NULL);

    // CLEAN ELECTRIC GUITAR — the steel string body through the CLEAN amp voicing
    instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 1100);
    instrument_harmonics(I_GTR, 0.55f);                 // resonant steel box
    instrument_morph(I_GTR, 0.38f);                     // a palm-mute chop (tight-ish stop)
    ampcab_apply(I_GTR, 0, 0.40f, 2, -1, 3, 0.18f);     // CLEAN: soft-clip, scooped-bright, light sag
    instrument_echo(I_GTR, 0.12f);                      // slapback
    instrument_chorus(I_GTR, 0.9f, 0.18f, 0.20f);       // a whisper of shimmer
    instrument_pan(I_GTR, -0.18f);

    // MELODIC BASS — round, fingered, dry, prominent
    instrument(I_BASS, INSTR_TRI, 2, 220, 5, 130);
    instrument_filter(I_BASS, FILTER_LOW, 600, 2);
    instrument_drive(I_BASS, 0.0f);

    // RHODES — warm, a touch of stereo chorus
    instrument(I_EP, INSTR_EPIANO, 2, 0, 6, 800);
    instrument_harmonics(I_EP, 0.15f); instrument_timbre(I_EP, 0.35f); instrument_morph(I_EP, 0.20f);
    instrument_filter(I_EP, FILTER_LOW, 2200, 2);
    instrument_chorus(I_EP, 0.7f, 0.28f, 0.26f);
    instrument_echo(I_EP, 0.10f);
    instrument_pan(I_EP, 0.16f);

    // a little analog organ/synth pad stab
    instrument(I_ORG, INSTR_SAW, 60, 300, 5, 360);
    instrument_filter(I_ORG, FILTER_LOW, 1500, 2);
    instrument_tune(I_ORG, 0.04f);

    // SOFT VOCAL TOPLINE (formant voice — warm "ah/oh", gentle)
    instrument(I_LEAD, INSTR_VOICE, 16, 80, 6, 260);
    instrument_harmonics(I_LEAD, 0.55f);                // an O→A vowel
    instrument_timbre(I_LEAD, 0.42f);
    instrument_morph(I_LEAD, 0.32f);                    // soft effort, not pressed
    voice_nasal(I_LEAD, 0.0f);
    instrument_filter(I_LEAD, FILTER_LOW, 2200, 2);
    instrument_echo(I_LEAD, 0.10f);

    // TIGHT CLEAN KIT
    instrument(I_KICK, INSTR_SINE, 0, 90, 0, 60);
    instrument_filter(I_KICK, FILTER_LOW, 260, 2);
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 40, 16);
    instrument(I_SNR, INSTR_NOISE, 1, 0, 0, 90);
    instrument_filter(I_SNR, FILTER_BAND, 1700, 4);
    instrument(I_HAT, INSTR_NOISE, 0, 18, 0, 14);
    instrument_filter(I_HAT, FILTER_HIGH, 7200, 3);

    // the room: a small warm space + gentle tape glue (the band committed to tape)
    reverb(0.35f, 0.50f);
    instrument_reverb(I_GTR, 0.22f);
    instrument_reverb(I_EP,  0.26f);
    instrument_reverb(I_LEAD, 0.28f);
    instrument_reverb(I_SNR, 0.14f);
    instrument_reverb(I_ORG, 0.30f);
    tape(0.10f, 0.07f, 0.22f);
    glue(0, 0.20f, 8, 150);                             // knit the dry band into one unit

    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);
}

static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chAmp) {
        ampcab_apply(I_GTR, sel, 0.40f, AMP_VC[sel].lo, AMP_VC[sel].mid, AMP_VC[sel].hi, 0.18f);
        glue(0, 0.20f, 8, 150);                         // restore the band glue (ampcab set its own)
    } else if (idx == chKeys) {
        if (sel == 0) { instrument_harmonics(I_EP, 0.15f); instrument_timbre(I_EP, 0.35f); instrument_morph(I_EP, 0.20f); }
        else          { instrument_harmonics(I_EP, 0.50f); instrument_timbre(I_EP, 0.42f); instrument_morph(I_EP, 0.35f); }
    } else if (idx == chKit) {
        if (sel == 0) { instrument(I_SNR, INSTR_NOISE, 1, 0, 0, 90); instrument_filter(I_SNR, FILTER_BAND, 1700, 4); }
        else          { instrument(I_SNR, INSTR_NOISE, 2, 60, 0, 60); instrument_filter(I_SNR, FILTER_BAND, 1300, 3); }   // brushed
        instrument_reverb(I_SNR, 0.14f);
    }
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_GTR,  FILTER_LOW, (int)(3000 * tm), 1);
    instrument_filter(I_EP,   FILTER_LOW, (int)(2200 * tm), 2);
    instrument_filter(I_LEAD, FILTER_LOW, (int)(leadCut * tm), 2);
    instrument_filter(I_ORG,  FILTER_LOW, (int)(1500 * tm), 2);
}

// ── update ──────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (WBA_SEED) { new_song(pos, WBA_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        apply_tone();
        booted = true;
    }

    int ev = rad_input(&tempo, 80, 140, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  { if (!radioOn) { note_off_all(); sfx(-1); } else scheduled = (long)pos; }
    if (ev & (RAD_EV_TONE | RAD_EV_NEW | RAD_EV_REPLAY | RAD_EV_BACK | RAD_EV_FWD)) apply_tone();

    int chair = rad_band_input(&band, &showHelp);
    if (chair >= 0) { apply_chair(chair); apply_tone(); }

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
        if (scheduled - songBase >= song_bars() * 16) fresh_song(pos);
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 8, sng.prog[i]);
    }
    vu *= 0.87f; if (vu > 12) vu = 12;
    gtrVu *= 0.86f; basVu *= 0.86f; drVu *= 0.86f; epVu *= 0.86f;

#ifdef DE_TRACE
    long ss = scheduled - songBase; long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("arch", "%s", ARCH[sng.arch].name);
    watch("form", "%s", FORMS[sng.form].name);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("key", "%s", RAD_PCNAME[sng.keyPc]);
    watch("chord", "%s", nowChord[(int)(tbar % 4)]);
    watch("tempo", "%d", tempo);
#endif
}

// ── draw — minimal Scandinavian stage: four dry instrument blocks in black space ──
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar  = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);
    int  ACC  = CLR_LIGHT_GREY;

    rad_body(CLR_DARK_GREY, ACC);
    rad_dial(sng.freq, ACC);

    // the window — a bare stage: four instrument blocks, lit only when playing
    rectfill(34, 52, 102, 116, CLR_BLACK);
    {
        const char *L[4] = { "GTR", "BAS", "DRM", "KEY" };
        float mv[4] = { gtrVu, basVu, drVu, epVu };
        int   col[4] = { CLR_LIGHT_PEACH, CLR_ORANGE, CLR_BLUE_GREEN, CLR_MAUVE };
        for (int i = 0; i < 4; i++) {
            int bx = 46 + i * 22, bw = 14;
            int lit = radioOn ? (int)(mv[i] * 9) : 0; if (lit > 64) lit = 64;
            int by = 138 - lit, bh = lit + 2;
            rectfill(bx, 60, bw, 78, CLR_DARKER_GREY);                 // the dim slot
            if (lit > 1) rectfill(bx, by, bw, bh, col[i]);             // the lit bar grows up
            rect(bx, 60, bw, 78, CLR_DARK_GREY);
            font(FONT_SMALL); print(L[i], bx + 1, 142, radioOn ? col[i] : CLR_DARK_GREY); font(FONT_NORMAL);
        }
    }
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, ACC);
    if (radioOn) {
        print(sng.title, 154, 58, ACC);
        char l2[32];
        snprintf(l2, 32, "%s  %s", ARCH[sng.arch].name, ARCH[sng.arch].vibe);
        font(FONT_SMALL); print(l2, 154, 70, CLR_LIGHT_PEACH); font(FONT_NORMAL);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 81, CLR_LIGHT_PEACH);
        float vt = vu / 12.0f; rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, ACC);
    } else print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        int ci = (int)(bar % 4), x = 152;
        for (int i = 0; i < 4; i++) {
            int cw = text_width(nowChord[i]); if (x + cw > 292) break;
            if (i == ci) { rectfill(x - 2, 102, cw + 4, 12, ACC); print(nowChord[i], x, 104, CLR_BLACK); }
            else print(nowChord[i], x, 104, CLR_MEDIUM_GREY);
            x += cw + 8;
        }
        print(SECTNAME[sect], 152, 120, ACC);
        print(ARCH[sng.arch].track, 196, 120, CLR_MEDIUM_GREY);
        rad_phrase_dots(232, 124, form_sects(), bar / 8, ACC);
    }

    static const char *FEEL[4] = { "bare", "room", "warm", "full" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], ACC);
    if (rad_knob_int(&tempo, 80, 140, 2, 218, 148, 9, "tempo", ACC)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], ACC)) apply_tone();
    rad_power_led(radioOn, ACC, CLR_DARK_GREY);

    rad_help_button(ACC);
    rad_band_button(ACC);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tune (rolls a new song)" },
            { "R",          "same tune - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - bare..full (space)" },
            { "UP/DOWN",    "tempo" },
            { "T",          "tone" },
            { "M",          "radio on / off" },
            { "B",          "the band - amp/keys/kit" },
        };
        static const char *NOTES[3] = {
            "the dial plays different WBA songs: Burning,",
            "Golden Cage, 1517, Courage, Done With You.",
            "clean guitar + melodic bass, all space.",
        };
        rad_help_panel("WBA FM", HELP, 8, NOTES, 3, ACC);
    }
    rad_band_panel(&band, ACC);
    ui_end();
}
