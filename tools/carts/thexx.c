/* de:meta
{
  "title": "the xx fm",
  "status": "active",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing"
  ],
  "lineage": "An artist-station (stolen-playbook archetypes, kin to air/wba); novel in the two-voice CALL-AND-RESPONSE brain (two INSTR_VOICE singers trading phrases, unison on the chorus) and space-as-structure. The dark/intimate twin of wba.",
  "homage": "The xx",
  "description": "The xx (Romy, Oliver, Jamie xx) station - an ARTIST station, the dark/intimate twin of wba. The whole identity is INTIMACY + NEGATIVE SPACE: the gaps ARE the song, dark-minor, nocturnal, everything drenched in a big reverb. THE NEW BRAIN is the BOY/GIRL CALL-AND-RESPONSE - two INSTR_VOICE singers, a low male (Oliver) and a higher female (Romy), TRADE sung phrases (one states, the other answers) and meet in unison on the chorus; who-sings-when, and the silence between turns, IS the structure. Each phrase is a moving melodic CONTOUR sung in short, articulated, breathing notes. Plus SPACE AS STRUCTURE - density stays low by design. Built on the stolen-playbook chord brain: five archetypes - CRYSTALISED (the guitar+bass interlock, voices trading), ISLANDS (a gentle UK-garage click), VCR (steel-pan sweet, Coexist), ANGELS (Romy near-solo, almost beatless), ON HOLD (brighter, a steel-pan hook). The band: the two voices, Romy's icy single-note reverbed guitar, Oliver's deep dub SUB bass, Jamie's pointillistic clicky kit + steel-pan/marimba dabs. The window is the stark white X with two glows - Oliver left, Romy right - brightening as each sings, fading on the reverb tail. SPACE next song, R same tune, [ ] history, LEFT/RIGHT space, UP/DOWN tempo, T tone, B band (voices duet/romy/oliver, perc kit/bare, guitar icy/warm), M power, H help. Pin via XX_SEED."
}
de:meta */
// ── THE XX FM ─────────────────────────────────────────────────────────────────
// The xx (Romy Madley Croft, Oliver Sim, Jamie xx) — xx / Coexist / I See You.
// An ARTIST station: the dial plays recognizably DIFFERENT songs of theirs. The whole
// identity is INTIMACY + NEGATIVE SPACE — the gaps ARE the song, dark-minor, nocturnal,
// everything drenched in reverb. Blind brief: docs/design/thexx-blind-brief.md.
//
// THE NEW BRAIN — the BOY/GIRL CALL-AND-RESPONSE: two INSTR_VOICE singers, a low male
// (Oliver) and a higher female (Romy), TRADE by phrase — one states, the other answers —
// and meet in close unison on the chorus. Who-sings-when (and the silence between turns)
// is the structure. Plus SPACE AS STRUCTURE: density stays low by design, few events,
// each into a huge reverb, rests carrying the weight (the parked Japanese *ma* idea).
//
// The band: the two voices · Romy's icy single-note reverbed guitar · Oliver's deep dub
// SUB bass · Jamie's pointillistic clicky kit + steel-pan/marimba dabs · a big reverb.
// Built on the stolen-playbook chord brain (#4): five archetypes from cited tracks.
//   CRYSTALISED — the guitar+bass interlock, the two voices trading (the icon)
//   ISLANDS     — a gentle UK-garage click, romantic, both voices
//   VCR         — sweet, steel-pan / glockenspiel forward (Coexist)
//   ANGELS      — Romy near-solo, almost beatless, bare and gorgeous
//   ON HOLD     — I See You: brighter, more produced, a steel-pan hook
//
//   SPACE next song   R play it again   [ ] history   M power
//   LEFT/RIGHT space (density)   UP/DOWN tempo   T tone   B band   H help

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define XX_SEED 0   // pin a favourite tune here (0 = free-roaming radio)

// ── instrument slots (5..12) ────────────────────────────────────────────────
#define I_VOXA 5     // Oliver — low male voice (INSTR_VOICE)
#define I_VOXB 6     // Romy — higher female voice (INSTR_VOICE)
#define I_GTR  7     // icy single-note guitar, drenched (INSTR_GUITAR)
#define I_SUB  8     // deep dub sub bass (INSTR_SINE)
#define I_PAN  9     // steel-pan / marimba dabs (INSTR_MALLET)
#define I_KICK 10    // sparse clicky kick
#define I_SNAP 11    // finger-snap / clap (INSTR_NOISE)
#define I_HAT  12    // a single pointillistic hat tick (INSTR_NOISE)

// ── chord qualities + rootless 3-voice voicings (3rd / 7th / colour) ───────
enum { Q_MAJ, Q_MAJ7, Q_DOM7, Q_MIN, Q_MIN7, Q_MIN9, NQ };
static const char *QN[NQ] = { "", "maj7", "7", "m", "m7", "m9" };
static const int QV[NQ][3] = {
    { 4, 7, 14 }, { 4, 11, 14 }, { 4, 10, 14 }, { 3, 7, 14 }, { 3, 10, 14 }, { 3, 10, 14 },
};
// dark scales — aeolian / dorian (the nocturnal minor)
static const int SCALES[2][7] = {
    { 0, 2, 3, 5, 7, 8, 10 },    // aeolian
    { 0, 2, 3, 5, 7, 9, 10 },    // dorian
};

// ── the five song archetypes ────────────────────────────────────────────────
enum { A_CRYS, A_ISL, A_VCR, A_ANG, A_HOLD, NA };
enum { VX_TRADE, VX_SOLO };          // both voices trade, or Romy near-solo
enum { BK_CLICK, BK_GARAGE, BK_NONE }; // the percussion feel
typedef struct { int off, q; } Ch;
static const Ch PROG[NA][4] = {
    // CRYSTALISED  i – v – bVImaj7 – bVII
    { { 0, Q_MIN }, { 7, Q_MIN }, { 8, Q_MAJ7 }, { 10, Q_MAJ } },
    // ISLANDS      im9 – iv – bVImaj7 – bIII
    { { 0, Q_MIN9 }, { 5, Q_MIN7 }, { 8, Q_MAJ7 }, { 3, Q_MAJ } },
    // VCR          im7 – bIIImaj7 – bVImaj7 – v
    { { 0, Q_MIN7 }, { 3, Q_MAJ7 }, { 8, Q_MAJ7 }, { 7, Q_MIN } },
    // ANGELS       im9 – bVImaj7 (two-chord, held — almost beatless)
    { { 0, Q_MIN9 }, { 8, Q_MAJ7 }, { 0, Q_MIN9 }, { 8, Q_MAJ7 } },
    // ON HOLD      im – bVII – bVI – bIII
    { { 0, Q_MIN }, { 10, Q_MAJ }, { 8, Q_MAJ }, { 3, Q_MAJ } },
};
static const struct {
    const char *name, *track, *vibe;
    int scale, vox, beat, perc, tlo, tspan;
} ARCH[NA] = {
    { "CRYSTALISED", "~ Crystalised", "the trading icon", 0, VX_TRADE,  BK_CLICK,  0, 100, 12 },
    { "ISLANDS",     "~ Islands",     "garage romance",   1, VX_TRADE,  BK_GARAGE, 0, 108, 10 },
    { "VCR",         "~ VCR",         "steel-pan sweet",  1, VX_TRADE,  BK_CLICK,  1,  92, 10 },
    { "ANGELS",      "~ Angels",      "Romy, near-bare",  0, VX_SOLO,   BK_NONE,   0,  84, 10 },
    { "ON HOLD",     "~ On Hold",     "brighter, a hook", 0, VX_TRADE,  BK_GARAGE, 1, 110, 10 },
};

// ── the form — 8-bar sections; the seed rolls a length ──────────────────────
enum { S_INTRO, S_A, S_B, S_BREAK, S_OUT };
#define MAXSECT 12
static const struct { int n; int s[MAXSECT]; const char *name; } FORMS[] = {
    { 5,  { S_INTRO, S_A, S_B, S_A, S_OUT }, "single" },
    { 8,  { S_INTRO, S_A, S_B, S_A, S_BREAK, S_B, S_A, S_OUT }, "album" },
    { 10, { S_INTRO, S_A, S_B, S_A, S_B, S_BREAK, S_A, S_B, S_A, S_OUT }, "extended" },
};
#define NFORMS 3
static const char *SECTNAME[5] = { "intro", "verse", "chorus", "break", "outro" };

// ── the generated song ──────────────────────────────────────────────────────
typedef struct {
    int   arch, keyPc, form;
    Ch    prog[4];
    int   cellOn[8], cellDeg[8], cellN;   // the shared sung cell — onsets + a melodic CONTOUR (2-bar grid)
    int   gtrOn[4], gtrN;       // the icy guitar motif onsets
    char  title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 130.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)   rad_srnd(&rs, (n))

static int   tempo     = 100;
static int   intensity = 1;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 1;        // the xx wants dark, not bright
static int   songCount = 0;
static float vu = 0, voxAvu = 0, voxBvu = 0;     // the two trading glows

static int   gvCmp[3] = { 60, 63, 67 }; static bool cmpInit = false;
static int   bassLast = 36;
static char  nowChord[4][8];

static RadBand band;
static int chVox, chPerc, chGtr;

static int iabs(int v) { return v < 0 ? -v : v; }
static void apply_chair(int idx);

// ── song generation ─────────────────────────────────────────────────────────
static const char *TW1[] = { "Crystal", "Islands", "Heart", "Night", "Shelter", "Angels",
    "Fiction", "Sunset", "Reunion", "Performance", "Replica", "Hold" };
static const char *TW2[] = { "ised", "Skipped", "Time", "Out", "& Held", "On Hold",
    "in Blue", "Alone", "Reconsider", "VCR", "Together", "Stars" };

static void roll_cell(int *on, int *n, int maxN, int dense, int grid) {
    *n = 0;
    for (int s = 0; s < grid - 1 && *n < maxN; s += 2)
        if (srnd(100) < dense) on[(*n)++] = s;
    if (*n < 2) { *n = 2; on[0] = 0; on[1] = grid / 2; }
}

// the sung cell — onsets WITH a melodic contour that actually MOVES (steps + the odd
// leap), and finer rhythm (some quick notes), so the line isn't a boring held drone.
static const int VSTEP[8] = { 1, -1, 2, -1, 2, -2, 3, -2 };
static void roll_vox_cell(int dense) {
    sng.cellN = 0;
    int d = srnd(4) - 1;                                  // start low-mid
    for (int s = 0; s < 31 && sng.cellN < 8; s += (srnd(100) < 40 ? 1 : 2)) {
        if (srnd(100) < dense) {
            sng.cellOn[sng.cellN]  = s;
            sng.cellDeg[sng.cellN] = d;
            d += VSTEP[srnd(8)];
            if (d > 8) d -= 7; if (d < -2) d += 7;        // wander, but stay in a singable octave
            sng.cellN++;
        }
    }
    if (sng.cellN < 4) {                                  // a fallback phrase that moves
        sng.cellN = 4;
        sng.cellOn[0]=0;  sng.cellDeg[0]=0;
        sng.cellOn[1]=6;  sng.cellDeg[1]=2;
        sng.cellOn[2]=12; sng.cellDeg[2]=4;
        sng.cellOn[3]=20; sng.cellDeg[3]=1;
    }
}

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);
    sng.arch  = srnd(NA);
    sng.keyPc = srnd(12);
    for (int k = 0; k < 4; k++) sng.prog[k] = PROG[sng.arch][k];
    roll_vox_cell(sng.arch == A_ANG ? 42 : 55);                              // a moving contour, more notes
    roll_cell(sng.gtrOn, &sng.gtrN, 4, 26, 16);                              // a 2-3 note icy motif
    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;
    sng.form = srnd(100) < 34 ? 0 : srnd(100) < 74 ? 1 : 2;
    tempo = ARCH[sng.arch].tlo + srnd(ARCH[sng.arch].tspan);
    bpm(tempo);
    songBase = (long)pos + 8;
    cmpInit = false; bassLast = 36;
    songCount++;
}
static void fresh_song(double pos) { new_song(pos, 0); rad_hist_log(&rs); }

// ── form / harmony ──────────────────────────────────────────────────────────
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

// sing a CONTOUR degree (relative to the key) folded into a voice's register window —
// the two voices share the cell's contour an octave+ apart (close when both sing).
static int deg_midi(int deg, int lo, int hi) {
    const int *sc = SCALES[ARCH[sng.arch].scale];
    int o = 4; while (deg < 0) { deg += 7; o--; } while (deg >= 7) { deg -= 7; o++; }
    int m = o * 12 + sng.keyPc + sc[deg];
    while (m < lo) m += 12; while (m > hi) m -= 12;
    return m;
}
static int bass_peek(int pc, int lo, int hi) {
    int d = ((pc - bassLast) % 12 + 18) % 12 - 6, n = bassLast + d;
    while (n < lo) n += 12; while (n > hi) n -= 12; return n;
}

// ── the step player — the trade + the space ──────────────────────────────────
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
    long phrase = s / 32;                         // 2-bar phrase index

    bool inBass = (sect != S_INTRO || bib >= 1);
    bool inKit  = (sect != S_INTRO || bib >= 2) && !brk;
    bool inVox  = (sect != S_INTRO || bib >= 4) && !brk;
    bool inGtr  = (sect != S_INTRO || bib >= 2);

    // ── THE SUB BASS — deep, round, the anchor; on the chord, long ──
    if (inBass && (step == 0 || (step == 10 && arch != A_ANG && chance(40)))) {
        int n = bass_peek(root_pc(c), 28, 43); bassLast = n;
        schedule_hit(dly + 6, n, I_SUB, 5, (int)(stepMs * (step == 0 ? 7 : 3)));
        vu += 1.0f;
    }

    bool percOn = (band.c[chPerc].sel == 0);     // "bare" chair strips all percussion (full space)

    // ── THE KIT — pointillistic, drenched; sparse by archetype ──
    if (inKit && percOn && ARCH[arch].beat != BK_NONE && lvl >= 1) {
        if (ARCH[arch].beat == BK_GARAGE) {       // a shuffled 2-step click
            if (step == 0 || step == 10) { schedule_hit(dly, 36, I_KICK, 4, 90); vu += 0.8f; }
            if (step == 4 || step == 12) schedule_hit(dly + rnd(3), 64, I_SNAP, 3, 60);
            if (step == 7 || step == 14) schedule_hit(dly + rnd(4), 92, I_HAT, 1, 26);
        } else {                                  // CLICK — bare downbeat + a snap
            if (step == 0) { schedule_hit(dly, 36, I_KICK, 3, 90); vu += 0.6f; }
            if (step == 8) schedule_hit(dly + rnd(3), 64, I_SNAP, 3, 60);
            if (lvl >= 2 && step == 12 && chance(40)) schedule_hit(dly + rnd(4), 92, I_HAT, 1, 24);
        }
    }

    // ── THE ICY GUITAR — a sparse single-note motif, drenched ──
    if (inGtr && lvl >= 1 && !brk) {
        const int *sc = SCALES[ARCH[arch].scale];
        for (int i = 0; i < sng.gtrN; i++)
            if (sng.gtrOn[i] == step && chance(arch == A_ANG ? 55 : 72)) {
                int pc = (sng.keyPc + sc[(i * 2 + (int)(bar)) % 7]) % 12;
                int m = 64 + pc; if (chance(35)) m += 12;
                schedule_hit(dly + rnd(4), m, I_GTR, 2, (int)(stepMs * 3));
                vu += 0.5f;
            }
    }

    // ── STEEL-PAN / MARIMBA dabs — Jamie's melodic percussion (rolled archetypes) ──
    if (ARCH[arch].perc && percOn && inGtr && lvl >= 1 && (step == 6 || step == 11) && chance(45)) {
        int pcs[3]; for (int k = 0; k < 3; k++) pcs[k] = (root_pc(c) + QV[c.q][k]) % 12;
        int m = 72 + pcs[rnd(3)];
        schedule_hit(dly + 8, m, I_PAN, 2, (int)(stepMs * 4));
        vu += 0.4f;
    }

    // ── THE VOCAL CALL-AND-RESPONSE (the headline) — who sings this phrase ──
    if (inVox && lvl >= 1) {
        // ANGELS = Romy near-solo; else trade A/B by phrase, both on the chorus (unison)
        int who;                                       // 1 = A (Oliver), 2 = B (Romy), 3 = both
        if (ARCH[arch].vox == VX_SOLO)      who = 2;
        else if (sect == S_B)               who = 3;   // the chorus, together
        else                                who = (phrase & 1) ? 1 : 2;
        if (band.c[chVox].sel == 1)         who = 2;   // chair: Romy only
        else if (band.c[chVox].sel == 2)    who = 1;   // chair: Oliver only
        for (int i = 0; i < sng.cellN; i++)
            if (sng.cellOn[i] == cs && chance(86)) {
                int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - cs : 32 - cs;
                // SHORT, articulated — sing ~half the gap then breathe (not the old held drone)
                int dur = (int)(gap * stepMs * 0.5); if (dur > 900) dur = 900; if (dur < 130) dur = 130;
                int deg = sng.cellDeg[i];
                if (who & 1) { schedule_hit(dly + 14 + rnd(6), deg_midi(deg, 47, 60), I_VOXA, 4, dur); voxAvu += 1.6f; vu += 1.0f; }
                if (who & 2) { schedule_hit(dly + 16 + rnd(6), deg_midi(deg, 64, 79), I_VOXB, 4, dur); voxBvu += 1.6f; vu += 1.0f; }
            }
    }
}

// ── one-time setup ────────────────────────────────────────────────────────
static void setup_instruments(void) {
    chVox  = rad_chair(&band, "voices", "duet", "romy", "oliver", NULL);
    chPerc = rad_chair(&band, "perc",  "kit", "bare", NULL, NULL);
    chGtr  = rad_chair(&band, "guitar", "icy", "warm", NULL, NULL);

    // VOICE A — Oliver: low, dark, large vocal tract, soft effort
    instrument(I_VOXA, INSTR_VOICE, 70, 0, 6, 900);
    instrument_harmonics(I_VOXA, 0.30f);             // O→U dark vowel
    instrument_timbre(I_VOXA, 0.72f);                // large tract = lower/darker
    instrument_morph(I_VOXA, 0.24f);                 // soft, relaxed
    voice_nasal(I_VOXA, 0.06f);
    instrument_filter(I_VOXA, FILTER_LOW, 2000, 1);
    instrument_pan(I_VOXA, -0.30f);
    // VOICE B — Romy: higher, breathy, smaller tract
    instrument(I_VOXB, INSTR_VOICE, 60, 0, 6, 900);
    instrument_harmonics(I_VOXB, 0.55f);             // A vowel
    instrument_timbre(I_VOXB, 0.38f);                // smaller tract
    instrument_morph(I_VOXB, 0.30f);                 // soft breath
    voice_nasal(I_VOXB, 0.04f);
    instrument_filter(I_VOXB, FILTER_LOW, 2600, 1);
    instrument_pan(I_VOXB, 0.30f);

    // icy guitar — clean single note, drenched
    instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 1200);
    instrument_harmonics(I_GTR, 0.30f); instrument_timbre(I_GTR, 0.62f); instrument_morph(I_GTR, 0.20f);
    instrument_filter(I_GTR, FILTER_LOW, 3200, 1);
    instrument_echo(I_GTR, 0.22f);

    // dub sub bass — deep round sine
    instrument(I_SUB, INSTR_SINE, 6, 300, 5, 300);
    instrument_filter(I_SUB, FILTER_LOW, 500, 1);

    // steel-pan / marimba dabs
    instrument(I_PAN, INSTR_MALLET, 1, 0, 7, 600);
    instrument_harmonics(I_PAN, 0.55f); instrument_timbre(I_PAN, 0.40f); instrument_morph(I_PAN, 0.28f);
    instrument_pan(I_PAN, 0.20f);

    // the clicky kit
    instrument(I_KICK, INSTR_SINE, 0, 110, 0, 60);
    instrument_filter(I_KICK, FILTER_LOW, 240, 2);
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 40, 14);
    instrument(I_SNAP, INSTR_NOISE, 0, 50, 0, 50);
    instrument_filter(I_SNAP, FILTER_BAND, 1800, 4);
    instrument(I_HAT, INSTR_NOISE, 0, 16, 0, 14);
    instrument_filter(I_HAT, FILTER_HIGH, 7600, 3);

    // the big nocturnal hall — everything blooms into it; the low end stays tight
    reverb(0.82f, 0.30f);
    instrument_reverb(I_VOXA, 0.42f);
    instrument_reverb(I_VOXB, 0.46f);
    instrument_reverb(I_GTR,  0.50f);
    instrument_reverb(I_PAN,  0.48f);
    instrument_reverb(I_SNAP, 0.30f);
    instrument_reverb(I_SUB,  0.10f);
    tape(0.10f, 0.07f, 0.20f);

    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);
}

static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chVox) {
        // duet (both), or force a single singer (romy / oliver) — handled at play time
        // via ARCH vox + this override; here we just keep both slots voiced.
    } else if (idx == chPerc) {
        // snaps / steel-pan / none — gated at play time; nothing to re-voice
    } else if (idx == chGtr) {
        if (sel == 0) { instrument_harmonics(I_GTR, 0.30f); instrument_timbre(I_GTR, 0.62f); instrument_echo(I_GTR, 0.22f); }
        else          { instrument_harmonics(I_GTR, 0.45f); instrument_timbre(I_GTR, 0.40f); instrument_echo(I_GTR, 0.14f); }  // warmer
    }
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_VOXA, FILTER_LOW, (int)(2000 * tm), 1);
    instrument_filter(I_VOXB, FILTER_LOW, (int)(2600 * tm), 1);
    instrument_filter(I_GTR,  FILTER_LOW, (int)(3200 * tm), 1);
}

// ── update ──────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (XX_SEED) { new_song(pos, XX_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        apply_tone();
        booted = true;
    }

    int ev = rad_input(&tempo, 72, 124, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
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
    vu *= 0.90f; if (vu > 12) vu = 12;
    voxAvu *= 0.955f; voxBvu *= 0.955f;   // slow decay = the long reverb-tail glow

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

// ── draw — the X, with two trading glows ─────────────────────────────────────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar  = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);
    int  ACC  = CLR_WHITE;

    rad_body(CLR_DARK_GREY, ACC);
    rad_dial(sng.freq, ACC);

    // the window — black field, the two trading glows, the stark X
    int wx = 34, wy = 52, ww = 102, wh = 116, cx = wx + ww / 2, cy = wy + wh / 2;
    rectfill(wx, wy, ww, wh, CLR_BLACK);
    if (radioOn) {
        // the glows: Oliver left (cool), Romy right (warm) — brighten as each sings,
        // fade on the reverb tail. The call-and-response, made visible.
        int ga = (int)(voxAvu * 6); if (ga > 30) ga = 30;
        int gb = (int)(voxBvu * 6); if (gb > 30) gb = 30;
        for (int r = ga; r > 0; r -= 3) circ(cx - 24, cy, r, r < ga / 2 ? CLR_BLUE : CLR_DARK_BLUE);
        for (int r = gb; r > 0; r -= 3) circ(cx + 24, cy, r, r < gb / 2 ? CLR_PINK : CLR_DARK_PURPLE);
        if (ga > 2) circfill(cx - 24, cy, 3, CLR_WHITE);
        if (gb > 2) circfill(cx + 24, cy, 3, CLR_WHITE);
    }
    // the X — two thick diagonals, dim, brightening faintly with the mix
    int xb = (radioOn && vu > 3) ? CLR_LIGHT_GREY : CLR_DARK_GREY;
    for (int o = -1; o <= 1; o++) {
        line(wx + 16, wy + 18 + o, wx + ww - 16, wy + wh - 18 + o, xb);
        line(wx + ww - 16, wy + 18 + o, wx + 16, wy + wh - 18 + o, xb);
    }
    rect(wx, wy, ww, wh, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, ACC);
    if (radioOn) {
        print(sng.title, 154, 58, ACC);
        char l2[32];
        snprintf(l2, 32, "%s  %s", ARCH[sng.arch].name, ARCH[sng.arch].vibe);
        font(FONT_SMALL); print(l2, 154, 70, CLR_LIGHT_GREY); font(FONT_NORMAL);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 81, CLR_MEDIUM_GREY);
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

    static const char *FEEL[4] = { "bare", "hush", "dusk", "full" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], ACC);
    if (rad_knob_int(&tempo, 72, 124, 2, 218, 148, 9, "tempo", ACC)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], ACC)) apply_tone();
    rad_power_led(radioOn, ACC, CLR_DARK_GREY);

    rad_help_button(ACC);
    rad_band_button(ACC);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next song (rolls a new tune)" },
            { "R",          "same tune - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "space - bare..full" },
            { "UP/DOWN",    "tempo" },
            { "T",          "tone" },
            { "M",          "radio on / off" },
            { "B",          "the band - voices/perc/guitar" },
        };
        static const char *NOTES[3] = {
            "two voices TRADE lines (call & response),",
            "together on the chorus. dark, minimal, all",
            "space. songs: Crystalised, Islands, Angels...",
        };
        rad_help_panel("THE XX FM", HELP, 8, NOTES, 3, ACC);
    }
    rad_band_panel(&band, ACC);
    ui_end();
}
