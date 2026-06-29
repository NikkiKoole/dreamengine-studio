/* de:meta
{
  "title": "afrobeat fm",
  "status": "active",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "euclidean-rhythm",
    "generative-melody",
    "chord-voicing",
    "swing-timing"
  ],
  "lineage": "Built intent-first from the Fela Kuti / Tony Allen playbook; first radio station to use INSTR_GUITAR and INSTR_BRASS, and the first to wire euclidean polyrhythm (gankogui 7-in-16, cross-conga 5-in-16) against a generative modal vamp with terraced song form.",
  "homage": "Fela Kuti & Africa 70 / Tony Allen (Afrobeat)",
  "description": "Endless Afrobeat, live from the Shrine - the Fela Kuti & Africa 70 / Tony Allen homage, and the station that finally reaches the engines no other one does. THREE radio firsts, all 'untapped shelf' engines: FIRST RADIO INSTR_GUITAR - the two interlocking rhythm guitars (a muted 'tenor' chuck panned left, a brighter rhythm guitar panned right; the Karplus-Strong-plus-body model, and the weave IS the Afrobeat signature); FIRST RADIO INSTR_BRASS - the trumpet of the horn section (only faked brass existed before); and the horn SECTION proper, INSTR_REED tenor sax plus that INSTR_BRASS trumpet voiced a third apart and panned wide to fake the ensemble spread. The brains, all cart-side over radio.h's clock: THE MODAL VAMP (no changes, one mode held - mixolydian I7 or dorian im, sometimes alternating to a second centre i-to-IV); EUCLIDEAN POLYRHYTHM (a gankogui bell timeline on euclid 7-in-16 and a cross-conga on euclid 5-in-16 riding against the straight kit - three cycles of different length, the West-African interlock); THE TONY ALLEN FEEL (a broken-funk kit, backbeat snare with scattered off-grid ghost notes plus a syncopated kick, all pure performance so R replays the song but never the same ghosts); and TERRACED FORM with THE BREAK - the intro layers the band in one part at a time, the BREAK section strips back to drums-and-bass before the horns rip back in. The sax solo is the shared improviser walking the mode. Seeded per song: the mode, key, vamp, bass ostinato, guitar interlock, horn riff, the GROOVE (laidback / driving / heavy halftime) and the FORM (cut / set / jam). The window is the Shrine stage, a saxophone in the sweeping lights. SPACE next song, R same tune (new solo), [ ] history, LEFT/RIGHT feel (cool/warm/hot/fire), UP/DOWN tempo, T tone, M power, B band (guitars/horns/comp - swap chairs live, horns can switch off), H help."
}
de:meta */
// ── AFROBEAT FM ─────────────────────────────────────────────────────────────
// Live from the Shrine: the Afrobeat station — Fela Kuti & Africa 70, Tony Allen
// on the kit. A single modal vamp held for minutes, two interlocking guitars, a
// horn section that calls and the band answers, and the long breakdown where it
// all drops to drums and bass before the horns rip back in.
//
// Designed INTENT-FIRST (cart-authoring-prompt.md): the band was imagined from
// the genre BEFORE shopping the palette — which is why this station finally
// reaches engines no other one does. Three radio firsts, all "untapped shelf"
// engines the gap-ledger (radio-genre-fidelity.md) kept flagging:
//   • FIRST RADIO INSTR_GUITAR — the two interlocking rhythm guitars (a muted
//     "tenor" chuck panned left, a brighter rhythm guitar panned right). The
//     Karplus-Strong + body model, no station had pulled it; the weave IS the
//     Afrobeat signature.
//   • FIRST RADIO INSTR_BRASS — the trumpet of the horn section (only faked
//     brass — italo's FM stabs, citypop's saw hits — existed before).
//   • The horn SECTION proper — INSTR_REED tenor sax + INSTR_BRASS trumpet
//     voiced a 3rd apart, panned slightly wide to fake the ensemble spread a
//     real chorus effect would give us (see afrobeat-effects-wants.md: the
//     wah guitar, room reverb, and section chorus all wait on the effects bus).
//
// The brains (all cart-side generative logic over radio.h's clock — the layer-3
// "cart's business", game-music.md's brain catalog):
//   • THE MODAL VAMP — no changes, one mode held: mixolydian I7 (the dominant
//     vamp) or dorian i (the minor one), sometimes alternating to a second
//     centre (i↔IV) every few bars. "One chord for eleven minutes."
//   • EUCLIDEAN POLYRHYTHM — the gankogui bell timeline (euclid 7-in-16, hi/lo)
//     and a cross-conga (euclid 5-in-16) ride against the straight kit: three
//     cycles of different length, the West-African interlock.
//   • THE TONY ALLEN FEEL — a broken-funk kit: backbeat snare with scattered
//     off-grid GHOST notes + a syncopated kick, all on PERFORMANCE rnd (R
//     replays the song, never the same ghosts twice).
//   • TERRACED FORM + THE BREAK — the intro layers the band in one part at a
//     time; the BREAK section strips back to drums+bass, then the horns return.
//
// Per-song roll (seed = the song): the mode, the key, the vamp, the bass
// ostinato, the guitar interlock, the horn riff, the GROOVE (laid-back / driving
// / heavy half-time), the FORM (cut / set / jam). Two SPACE presses = two records.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density)   UP/DOWN tempo   T tone   B band   H help

#include "studio.h"
#include "radio.h"
#include "improv.h"
#include "ampcab.h"   // opt-in guitar amp/cab voicing for the "amp" guitars chair
#include <stdio.h>
#include <math.h>

#define AFRO_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots (5..15 — the full custom range) ──────────────────────
#define I_GTR1   5   // tenor guitar — muted interlock chuck (GUITAR, panned L)
#define I_GTR2   6   // rhythm guitar — the counter-pattern (GUITAR, panned R)
#define I_BASS   7   // upright bass ostinato (INSTR_TRI, woody — was a farty FM electric)
#define I_ORG    8   // combo organ comp (INSTR_ORGAN, jazz B3)
#define I_SAX    9   // tenor sax — section top + the solo (INSTR_REED)
#define I_TPT   10   // trumpet — section + stabs (INSTR_BRASS)  *plays oct +1*
#define SL_KICK 11   // kick — bespoke SINE + pitch-drop punch
#define SL_SNR  12   // snare + ghost notes (INSTR_NOISE)
#define SL_CONGA 13  // congas, open/slap (INSTR_MEMBRANE)
#define SL_SHK  14   // shekere 16ths (INSTR_NOISE)
#define SL_BELL 15   // gankogui / agogo timeline (INSTR_FM bell, hi/lo)

// ── the modes — SCALES AS DATA (7-entry ascending for improv.h's mode7) ────
// Afrobeat lives on a held mode, not a progression. Two centres of gravity:
// the dominant (mixolydian, the bright I7 vamp) and the minor (dorian, the
// "Zombie" / "Water" feel). The 7th degree is the colour either way.
enum { M_MIXO, M_DORIAN, NM };
static const int MODE7[NM][7] = {
    { 0, 2, 4, 5, 7, 9, 10 },   // mixolydian — dominant vamp (b7)
    { 0, 2, 3, 5, 7, 9, 10 },   // dorian     — minor vamp (b3, natural 6)
};
static const char *MNAME[NM] = { "mixolydian", "dorian" };
static const bool  MMINOR[NM] = { false, true };

// ── the form — 8-bar sections; the seed rolls one of three arrangements ────
// cut = a tight single (40 bars); set = the full shape with a breakdown (64);
// jam = the long Shrine workout, doubled solo + a second break (88).
enum { S_INTRO, S_GROOVE, S_HORNS, S_SOLO, S_BREAK, S_OUT };
#define MAXSECT 12
static const struct { int n; int s[MAXSECT]; const char *name; } FORMS[] = {
    { 5,  { S_INTRO, S_GROOVE, S_HORNS, S_SOLO, S_OUT }, "cut" },
    { 8,  { S_INTRO, S_GROOVE, S_HORNS, S_SOLO, S_BREAK, S_GROOVE, S_HORNS, S_OUT }, "set" },
    { 11, { S_INTRO, S_GROOVE, S_HORNS, S_SOLO, S_SOLO, S_BREAK,
            S_GROOVE, S_HORNS, S_SOLO, S_GROOVE, S_OUT }, "jam" },
};
#define NFORMS 3
static const char *SECTNAME[6] = { "intro", "groove", "horns", "sax solo", "break", "out" };

// ── the groove — the percussion feel; the ear locks here first ─────────────
enum { G_LAIDBACK, G_DRIVING, G_HALFTIME, NG };
static const char *GNAME[NG] = { "laidback", "driving", "halftime" };

// ── the generated song ──────────────────────────────────────────────────────
typedef struct {
    int  mode, keyPc;
    int  form, groove;
    int  secondOff;             // semitone offset of the 2nd vamp centre (i↔IV)
    int  vampKind;              // 0 = pure tonic vamp · 1 = alt /8 bars · 2 = alt /4
    int  compIv[3];             // the organ comp voicing (3 mode intervals)
    int  bassOn[8], bassDeg[8], bassN;   // the ostinato (2-bar grid)
    int  g1On[8], g2On[8], g1N, g2N;     // the two interlocking guitar patterns
    int  hornOn[6], hornDeg[6], hornN;   // the horn-section riff (2-bar)
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 125.0 };
static Improv     solo;
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)

static int   tempo     = 112;
static int   intensity = 1;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 2;        // Afrobeat wants clear/bright horns
static int   songCount = 0;
static float vu        = 0;
static char  nowCtr[2][12];
static bool  hornsOn   = true;     // the horn chair can switch off

static int  gvOrg[3] = { 55, 60, 64 };
static bool orgInit  = false;

// THE BAND (B) — three chairs the player can re-voice live.
static RadBand band;
static int chGtr, chHorn, chOrg;

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);

// ── song generation — composition AND the band, all from the seed ─────────
static const char *TW1[] = { "Lagos", "Kalakuta", "Shakara", "Zombie", "Water",
    "Sorrow", "Gentleman", "Roforofo", "Expensive", "Confusion", "Upside", "Mr" };
static const char *TW2[] = { "Fight", "Shrine", "Groove", "No", "Palaver", "Man",
    "Stop", "Vibration", "Power", "Trouble", "Down", "Follow" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.mode  = srnd(NM);
    sng.keyPc = srnd(12);
    const int *m = MODE7[sng.mode];

    // the organ comp voicing — three mode tones (rootless-ish cluster)
    sng.compIv[0] = m[2];      // 3rd (major in mixo, minor in dorian)
    sng.compIv[1] = m[4];      // 5th
    sng.compIv[2] = m[6];      // b7 — the Afrobeat colour

    // the vamp: a 2nd centre a 4th up (the i↔IV move), and how often it visits
    sng.secondOff = m[3];                          // the 4th
    sng.vampKind  = srnd(100) < 55 ? 0 : (srnd(100) < 65 ? 1 : 2);

    // the bass ostinato — syncopated, root-heavy, 2-bar, locked for the song
    sng.bassN = 0;
    static const int BCAND[10] = { 0, 3, 6, 8, 11, 14, 19, 22, 24, 27 };
    static const int BDEG[6]   = { 0, 0, 4, 0, 6, 2 };   // root-leaning, b7 spice
    for (int i = 0; i < 10 && sng.bassN < 7; i++)
        if (srnd(100) < 44) {
            sng.bassOn[sng.bassN]  = BCAND[i];
            sng.bassDeg[sng.bassN] = BDEG[srnd(6)];
            sng.bassN++;
        }
    if (sng.bassN < 3) {
        sng.bassN = 3;
        sng.bassOn[0]=0;  sng.bassDeg[0]=0;
        sng.bassOn[1]=6;  sng.bassDeg[1]=4;
        sng.bassOn[2]=19; sng.bassDeg[2]=0;
    }

    // the two interlocking guitars — tight muted 16ths that weave: GTR1 leans
    // on the off-beats, GTR2 fills the gaps. The seed nudges which slots fire.
    sng.g1N = sng.g2N = 0;
    for (int s = 2; s < 32; s += 4)                // GTR1: the "and" of each beat
        if (srnd(100) < 78 && sng.g1N < 8) sng.g1On[sng.g1N++] = s;
    for (int s = 0; s < 32; s += 4)                // GTR2: on the beat, sparser
        if (srnd(100) < 62 && sng.g2N < 8) sng.g2On[sng.g2N++] = s;
    if (sng.g1N < 2) { sng.g1N = 2; sng.g1On[0]=2; sng.g1On[1]=10; }
    if (sng.g2N < 2) { sng.g2N = 2; sng.g2On[0]=0; sng.g2On[1]=16; }

    // the horn-section riff — a short syncopated figure in a 2-bar grid
    sng.hornN = 0;
    static const int HCAND[8] = { 0, 3, 6, 8, 16, 19, 22, 24 };
    int hwalk = 4;
    for (int i = 0; i < 8 && sng.hornN < 6; i++)
        if (srnd(100) < 50) {
            sng.hornOn[sng.hornN] = HCAND[i];
            hwalk += srnd(3) - 1;
            if (hwalk < 0) hwalk = 0;
            if (hwalk > 7) hwalk = 7;
            sng.hornDeg[sng.hornN] = hwalk;
            sng.hornN++;
        }
    if (sng.hornN < 3) {
        sng.hornN = 3;
        sng.hornOn[0]=0;  sng.hornDeg[0]=4;
        sng.hornOn[1]=6;  sng.hornDeg[1]=2;
        sng.hornOn[2]=16; sng.hornDeg[2]=0;
    }

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    // the two big "different song" axes: the arrangement and the groove
    int fr     = srnd(100);
    sng.form   = fr < 30 ? 0 : fr < 72 ? 1 : 2;        // 30% cut · 42% set · 28% jam
    sng.groove = srnd(NG);

    // tempo follows the groove — halftime drags, driving burns
    static const int TLO[NG] = { 104, 118, 96 }, TSPAN[NG] = { 14, 14, 12 };
    tempo = TLO[sng.groove] + srnd(TSPAN[sng.groove]);
    bpm(tempo);
    apply_band_overrides();
    songBase = (long)pos + 8;
    orgInit = false;
    songCount++;
}

static void fresh_song(double pos) {
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ──────────────────────────────────────────────────────────
static int  form_sects(void) { return FORMS[sng.form].n; }
static long song_bars(void)  { return (long)FORMS[sng.form].n * 8; }
static int  sect_of(long bar) {
    int x = (int)(bar / 8), n = FORMS[sng.form].n;
    return x < n ? FORMS[sng.form].s[x] : S_OUT;
}

// the contiguous solo run containing `bar` (the jam doubles a solo to 16 bars)
static void solo_span(long bar, long *startBar, int *lenBars) {
    int sect = sect_of(bar), x = (int)(bar / 8), x0 = x, x1 = x, n = FORMS[sng.form].n;
    while (x0 > 0     && sect_of((long)(x0 - 1) * 8) == sect) x0--;
    while (x1 + 1 < n && sect_of((long)(x1 + 1) * 8) == sect) x1++;
    *startBar = (long)x0 * 8;
    *lenBars  = (x1 - x0 + 1) * 8;
}

// which vamp centre this bar (semitone offset from the key root)
static int vamp_center(long bar) {
    if (sng.vampKind == 0) return 0;
    int period = sng.vampKind == 1 ? 8 : 4;
    return ((bar / period) & 1) ? sng.secondOff : 0;
}

static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUT) ? 0 :
               (s == S_SOLO || s == S_HORNS) ? 2 : 1;
    return rad_level(base, intensity);
}

static void center_label(char *out, int n, int off) {
    snprintf(out, n, "%s%s", RAD_PCNAME[(sng.keyPc + off) % 12],
             MMINOR[sng.mode] ? "m" : "7");
}

// ── the step player ─────────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= song_bars()) return;
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  cs   = (int)(s % 32);                  // position in the 2-bar window
    int  bib  = (int)(bar % 8);                 // bar-in-section (intro build)
    const int *m = MODE7[sng.mode];
    int  center  = vamp_center(bar);
    int  croot   = (sng.keyPc + center) % 12;
    bool brk     = (sect == S_BREAK);

    // additive intro: the band layers in over the 8-bar intro
    bool inDrums = sect != S_INTRO || bib >= 0;
    bool inPerc  = sect != S_INTRO || bib >= 2;
    bool inBass  = sect != S_INTRO || bib >= 2;
    bool inGtr   = (sect != S_INTRO || bib >= 4) && !brk;
    bool inOrg   = (sect != S_INTRO || bib >= 6) && !brk;

    // ── THE BELL TIMELINE — gankogui, euclid 7-in-16, hi/lo alternating ──
    if (inPerc && lvl >= 1) {
        if (euclid(7, 16, step)) {
            static int bellc = 0; bellc++;
            int hi = (bellc & 1);
            schedule_hit(dly + rnd(3), hi ? 86 : 79, SL_BELL, hi ? 3 : 4,
                         hi ? 120 : 180);
            vu += 0.5f;
        }
        // shekere — continuous 16ths, accented on the beat
        if (step % 2 == 0)
            schedule_hit(dly + rnd(6), 90, SL_SHK, step % 4 == 0 ? 2 : 1,
                         step % 4 == 0 ? 30 : 20);
    }

    // ── THE KIT — Tony Allen broken funk (kick + snare + ghosts) ──
    if (inDrums) {
        // kick — syncopated, groove-dependent (PERFORMANCE feel on top)
        bool kick = false;
        if (sng.groove == G_HALFTIME) kick = (step == 0 || step == 10);
        else if (sng.groove == G_DRIVING) kick = (step == 0 || step == 6 || step == 11);
        else kick = (step == 0 || step == 7 || (step == 14 && chance(45)));
        if (kick) { schedule_hit(dly, 36, SL_KICK, 5, 120); vu += 1.5f; }

        // snare — backbeat on 4 & 12 (beats 2 & 4), plus off-grid GHOST notes
        if (step == 4 || step == 12) { schedule_hit(dly + rnd(3), 50, SL_SNR, 5, 110); vu += 1.2f; }
        else if (lvl >= 1 && (step % 2) && chance(sng.groove == G_DRIVING ? 38 : 28))
            schedule_hit(dly + rnd(5), 50, SL_SNR, 1, 50);     // ghost: low vel, scattered

        // congas — open/slap interplay + a euclid 5-in-16 cross-ride
        if (inPerc) {
            if (step == 2 || step == 8 || step == 13)
                { schedule_hit(dly + rnd(4), step == 8 ? 57 : 52, SL_CONGA, 3, 150); vu += 0.7f; }
            if (lvl >= 2 && euclid(5, 16, (step + 2) % 16) && chance(80))
                schedule_hit(dly + rnd(4), 60 + rnd(4), SL_CONGA, 2, 90);
        }
    }

    // ── BASS ostinato — the riff, locked and looping (drops only at intro top) ──
    if (inBass)
        for (int i = 0; i < sng.bassN; i++)
            if (sng.bassOn[i] == cs) {
                int mm = improv_deg_to_midi(sng.bassDeg[i], 36 + croot, m);
                while (mm < 28) mm += 12;
                while (mm > 47) mm -= 12;
                schedule_hit(dly + 10, mm, I_BASS, 5, (int)(stepMs * 4));
                vu += 1.4f;
            }

    // ── THE TWO GUITARS — interlocking muted chucks (panned L / R) ──
    if (inGtr && lvl >= 1) {
        for (int i = 0; i < sng.g1N; i++)
            if (sng.g1On[i] == cs) {
                int mm = improv_deg_to_midi(2 + (i & 1) * 2, 48 + croot, m);  // 3rd/5th dyad
                schedule_hit(dly + 6 + rnd(3), mm, I_GTR1, 3, (int)(stepMs * 0.9f));
                vu += 0.6f;
            }
        if (lvl >= 2)
            for (int i = 0; i < sng.g2N; i++)
                if (sng.g2On[i] == cs) {
                    int mm = improv_deg_to_midi(4 + (i & 1) * 2, 48 + croot, m); // 5th/b7
                    schedule_hit(dly + 8 + rnd(3), mm, I_GTR2, 3, (int)(stepMs * 0.8f));
                    vu += 0.6f;
                }
    }

    // ── ORGAN comp — held cluster, re-voiced each 2 bars / centre change ──
    if (inOrg && step == 0 && bar % 2 == 0) {
        rad_lead_to(croot, sng.compIv, gvOrg, 3, 52, 74, &orgInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 6, gvOrg[k], I_ORG, lvl >= 1 ? 3 : 2, (int)(stepMs * 28));
        vu += 0.9f;
    }

    // ── THE HORN SECTION — sax (top) + trumpet (a 3rd up, oct+1), the riff ──
    // call-and-response: states the riff on even 2-bar windows, answers it (the
    // motif inverted, lower) on odd ones. Sparse stabs leak into the groove too.
    if (hornsOn && !brk) {
        bool hornBar = (sect == S_HORNS) ||
                       (sect == S_GROOVE && lvl >= 2 && bar % 4 == 3);    // groove stabs
        if (hornBar) {
            bool answer = (sect == S_HORNS) && ((bar / 2) & 1);
            for (int i = 0; i < sng.hornN; i++)
                if (sng.hornOn[i] == cs) {
                    int deg = answer ? (6 - sng.hornDeg[i]) : sng.hornDeg[i];
                    int mm  = improv_deg_to_midi(deg, 58 + sng.keyPc, m);
                    while (mm < 56) mm += 12;
                    while (mm > 74) mm -= 12;
                    schedule_hit(dly + 12, mm, I_SAX, 4, (int)(stepMs * 3));    // sax top
                    schedule_hit(dly + 12, mm + 12 - (MMINOR[sng.mode] ? 3 : 4), // tpt a 3rd below, oct+1
                                 I_TPT, 3, (int)(stepMs * 3));
                    vu += 1.4f;
                }
        }
    }

    // ── THE SAX SOLO — the improviser, walking the mode over the vamp ──
    if (sect == S_SOLO && hornsOn) {
        long startBar; int lenBars;
        solo_span(bar, &startBar, &lenBars);
        long soloBar = bar - startBar;
        if (soloBar == 0 && step == 0)
            improv_begin(&solo, 62, lenBars, 1.0f);
        if (step == 0 && bar % 2 == 0 && improv_due(&solo, soloBar))
            improv_render(&solo, soloBar, m);
        float arc = improv_arc(&solo, soloBar);
        for (int i = 0; i < solo.n; i++)
            if (solo.onset[i] == cs) {
                int mm  = improv_midi(&solo, i, soloBar, sng.keyPc, m, 12); // a little blues bend
                int gat = (int)(stepMs * (solo.dur[i] > 4 ? solo.dur[i] : 2) * 0.85f);
                schedule_hit(dly + 12, mm, I_SAX, 4 + (arc > 0.6f ? 1 : 0), gat > 1600 ? 1600 : gat);
                vu += 1.4f;
            }
    }
}

// ── the band chairs — full recipes; a swap re-aims the slot from scratch ───
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chGtr) {
        if (sel == 0) {                                  // "guitars" — muted tenor + steel
            instrument(I_GTR1, INSTR_GUITAR, 1, 0, 7, 1200);
            instrument_harmonics(I_GTR1, 0.20f); instrument_timbre(I_GTR1, 0.60f);
            instrument_morph(I_GTR1, 0.85f);             // heavy mute, quick stop (pizz)
            instrument(I_GTR2, INSTR_GUITAR, 1, 0, 7, 1200);
            instrument_harmonics(I_GTR2, 0.52f); instrument_timbre(I_GTR2, 0.70f);
            instrument_morph(I_GTR2, 0.35f);             // brighter, a touch more ring
        } else {                                         // "clean" — both more open (less mute)
            instrument(I_GTR1, INSTR_GUITAR, 1, 0, 7, 1200);
            instrument_harmonics(I_GTR1, 0.45f); instrument_timbre(I_GTR1, 0.40f);
            instrument_morph(I_GTR1, 0.30f);
            instrument(I_GTR2, INSTR_GUITAR, 1, 0, 7, 1200);
            instrument_harmonics(I_GTR2, 0.55f); instrument_timbre(I_GTR2, 0.55f);
            instrument_morph(I_GTR2, 0.25f);
        }
        instrument_pan(I_GTR1, -0.55f);                  // the weave, spread wide
        instrument_pan(I_GTR2,  0.55f);
        // sel 2 = "amp": run the clean voicing through the CHIME amp/cab (presence + a
        // touch of even-harmonic warmth, glued) — opt-in, the default (sel 0) is untouched.
        if (sel == 2) {
            ampcab_apply(I_GTR1, 1, 0.35f, AMP_VC[1].lo, AMP_VC[1].mid, AMP_VC[1].hi, 0.20f);
            ampcab_apply(I_GTR2, 1, 0.35f, AMP_VC[1].lo, AMP_VC[1].mid, AMP_VC[1].hi, 0.20f);
        } else {                                         // bare again — clear any amp residue
            instrument_drive(I_GTR1, 0); instrument_drive(I_GTR2, 0);
            instrument_eq(I_GTR1, 0, 0, 0); instrument_eq(I_GTR2, 0, 0, 0);
            glue(0, 0.0f, 8, 120);
        }
    } else if (idx == chHorn) {
        hornsOn = (sel != 2);
        if (sel == 0) {                                  // "sax+tpt" — the full section
            instrument(I_SAX, INSTR_REED, 1, 0, 4, 1200);
            instrument_harmonics(I_SAX, 0.82f); instrument_timbre(I_SAX, 0.30f);
            instrument_morph(I_SAX, 0.62f);              // breathy tenor
            instrument(I_TPT, INSTR_BRASS, 1, 0, 4, 1200);
            instrument_harmonics(I_TPT, 0.15f); instrument_timbre(I_TPT, 0.60f);
            instrument_morph(I_TPT, 0.42f);              // bright blatty trumpet
        } else if (sel == 1) {                           // "sax only" — drop the trumpet to alto
            instrument(I_SAX, INSTR_REED, 1, 0, 4, 1200);
            instrument_harmonics(I_SAX, 0.78f); instrument_timbre(I_SAX, 0.45f);
            instrument_morph(I_SAX, 0.55f);              // alto, warmer
            instrument(I_TPT, INSTR_BRASS, 1, 0, 4, 1200);
            instrument_harmonics(I_TPT, 0.46f); instrument_timbre(I_TPT, 0.28f); // mellow flugel, sits back
            instrument_morph(I_TPT, 0.45f);
        }
        instrument_pan(I_SAX, -0.28f);                   // the section, slightly wide
        instrument_pan(I_TPT,  0.28f);
        // sel == 2 ("off") — slots stay defined, hornsOn gates every hit
    } else if (idx == chOrg) {
        if (sel == 0) {                                  // "organ" — jazz B3 comp
            instrument(I_ORG, INSTR_ORGAN, 6, 0, 7, 200);
            instrument_harmonics(I_ORG, 0.44f); instrument_timbre(I_ORG, 0.55f);
            instrument_morph(I_ORG, 0.75f);              // percussion + chorus (the Hammond move)
        } else {                                         // "rhodes" — EPIANO comp instead
            instrument(I_ORG, INSTR_EPIANO, 4, 0, 6, 900);
            instrument_harmonics(I_ORG, 0.15f); instrument_timbre(I_ORG, 0.45f);
            instrument_morph(I_ORG, 0.25f);
        }
    }
}

static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}

static void setup_instruments(void) {
    chGtr  = rad_chair(&band, "guitars", "muted", "clean",  "amp", NULL);
    chHorn = rad_chair(&band, "horns",   "sax+tpt", "sax only", "off", NULL);
    chOrg  = rad_chair(&band, "comp",    "organ", "rhodes", NULL, NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);     // base sounds (sel 0)

    // the bass — INSTR_FM, a round electric with a short pitch-blip attack
    // an UPRIGHT bass — was a farty FM electric; now a round INSTR_TRI with a
    // finger-pluck thump (ENV_PITCH) and a warm lowpass: woody, deep, no buzz.
    instrument(I_BASS, INSTR_TRI, 3, 280, 5, 130);
    instrument_filter(I_BASS, FILTER_LOW, 700, 1);
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 18, 3);

    // KICK — bespoke punchy synth kick (sine + fast pitch drop)
    instrument(SL_KICK, INSTR_SINE, 1, 60, 0, 90);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 45, 28);    // thwack down to pitch
    // SNARE — a noise crack, band-shaped
    instrument(SL_SNR, INSTR_NOISE, 1, 0, 0, 120);
    instrument_filter(SL_SNR, FILTER_BAND, 1900, 4);
    // CONGAS — the struck-drumhead engine (tabla.c conga recipe)
    instrument(SL_CONGA, INSTR_MEMBRANE, 1, 0, 7, 200);
    instrument_harmonics(SL_CONGA, 0.55f);
    instrument_timbre(SL_CONGA, 0.35f);
    instrument_morph(SL_CONGA, 0.15f);
    // SHEKERE — high filtered noise
    instrument(SL_SHK, INSTR_NOISE, 1, 30, 0, 22);
    instrument_filter(SL_SHK, FILTER_HIGH, 6000, 3);
    // GANKOGUI / AGOGO bell — a SOFT struck-bar metallic (was a harsh bright FM bell:
    // high mod-index FM clang on a piercing register). INSTR_MALLET's decaying sine
    // modes are far mellower; a gentle mallet + a rolled-off top = a warm agogo.
    instrument(SL_BELL, INSTR_MALLET, 1, 0, 7, 220);
    instrument_harmonics(SL_BELL, 0.60f);            // bell-ish bar material, rounded
    instrument_timbre(SL_BELL, 0.32f);               // soft mallet — gentle attack, no clang
    instrument_morph(SL_BELL, 0.30f);                // a short, contained ring
    // THE LOW-PASS GATE — base cutoff nearly shut; the strike snaps it open then it
    // CLOSES as the note rings down (ENV_CUTOFF decays back to base), while the MALLET's
    // own amp decay drops the level: brightness + volume fall together = the vactrol
    // LPG bonk (the easel/dubdesk west-coast voicing), here per struck hit so the
    // euclid timeline stays sample-tight.
    instrument_filter(SL_BELL, FILTER_LOW, 2000, 2);
    instrument_env(SL_BELL, 0, ENV_CUTOFF, 0, 170, 3200);
    instrument_pan(SL_BELL, 0.35f);
    instrument_pan(SL_CONGA, -0.30f);
    instrument_pan(SL_SHK, 0.45f);
}

// the tone knob — the whole band brightens/mellows (re-aim, keeping song rolls)
static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_GTR1, FILTER_LOW, (int)(3400 * tm), 1);
    instrument_filter(I_GTR2, FILTER_LOW, (int)(3800 * tm), 1);
    instrument_filter(I_ORG,  FILTER_LOW, (int)(3000 * tm), 1);
    if (hornsOn) {
        instrument_filter(I_SAX, FILTER_LOW, (int)(3600 * tm), 2);
        instrument_filter(I_TPT, FILTER_LOW, (int)(3800 * tm), 2);
    }
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (AFRO_SEED) { new_song(pos, AFRO_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        apply_tone();
        booted = true;
    }

    int ev = rad_input(&tempo, 92, 136, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) { note_off_all(); sfx(-1); }
        else scheduled = (long)pos;
    }
    if (ev & (RAD_EV_TONE | RAD_EV_NEW | RAD_EV_REPLAY | RAD_EV_BACK | RAD_EV_FWD))
        apply_tone();

    int chair = rad_band_input(&band, &showHelp);
    if (chair >= 0) { apply_chair(chair); apply_tone(); }

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= song_bars() * 16) fresh_song(pos);
        center_label(nowCtr[0], 12, 0);
        center_label(nowCtr[1], 12, sng.secondOff);
    }

    vu *= 0.88f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("mode", "%s", MNAME[sng.mode]);
    watch("groove", "%s", GNAME[sng.groove]);
    watch("form", "%s", FORMS[sng.form].name);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("center", "%d", vamp_center(tbar));
    watch("phrase", "%d", solo.phraseNo);
#endif
}

// ── draw — the chassis; window art = the Shrine stage, a sax in the lights ──
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar  = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_DARK_PURPLE, CLR_ORANGE);         // dim purple shell, hot orange trim
    rad_dial(sng.freq, CLR_ORANGE);

    // the window — the Shrine: a hazy stage, beams, a saxophone in silhouette
    rectfill(34, 52, 102, 116, CLR_DARK_BLUE);               // the smoky room
    // stage light beams sweeping (animated)
    for (int b = 0; b < 3; b++) {
        int bx = 60 + b * 28 + (int)(sinf(timer() * 0.7f + b) * 6);
        for (int yy = 52; yy < 120; yy += 3)
            pset(bx + (yy - 52) / 6, yy, b == 1 ? CLR_ORANGE : CLR_DARK_RED);
    }
    rectfill(34, 138, 102, 30, CLR_DARKER_GREY);             // the stage floor
    // a saxophone silhouette, centre stage
    int sxx = 84, sxy = 96;
    circfill(sxx + 6, sxy + 34, 9, CLR_YELLOW);              // the bell
    circ(sxx + 6, sxy + 34, 9, CLR_ORANGE);
    for (int k = 0; k < 22; k++) {                           // the body curve
        int yy = sxy + 14 + k;
        int xx = sxx + 2 + (int)(sinf(k * 0.13f) * 3);
        pset(xx, yy, CLR_YELLOW); pset(xx + 1, yy, CLR_ORANGE);
    }
    rectfill(sxx, sxy, 3, 16, CLR_YELLOW);                   // the neck
    circfill(sxx + 1, sxy - 1, 2, CLR_LIGHT_GREY);           // the mouthpiece
    // VU-reactive crowd glow at the foot of the stage
    int glow = (int)(vu);
    if (glow > 0) rectfill(34, 166, 102, 2, glow > 6 ? CLR_ORANGE : CLR_DARK_RED);
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_ORANGE);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_ORANGE);
        char l2[32];
        snprintf(l2, 32, "%s  %s", MNAME[sng.mode], GNAME[sng.groove]);
        print(l2, 154, 70, CLR_YELLOW);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_YELLOW);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_ORANGE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        int ncenters = sng.vampKind == 0 ? 1 : 2;
        rad_chord_row(nowCtr, ncenters, vamp_center(bar) ? 1 : 0, 152, 104, CLR_ORANGE);
        print(SECTNAME[sect], 152, 120, CLR_ORANGE);
        rad_phrase_dots(232, 124, form_sects(), bar / 8, CLR_ORANGE);
    }

    static const char *FEEL[4] = { "cool", "warm", "hot", "fire" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_ORANGE);
    if (rad_knob_int(&tempo, 92, 136, 2, 218, 148, 9, "tempo", CLR_ORANGE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_ORANGE)) apply_tone();
    rad_power_led(radioOn, CLR_ORANGE, CLR_DARK_PURPLE);

    rad_help_button(CLR_ORANGE);
    rad_band_button(CLR_ORANGE);

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tune (rolls a new seed)" },
            { "R",          "same tune - the solo is new" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - cool/warm/hot/fire" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "B",          "the band - guitars/horns/comp" },
        };
        static const char *NOTES[3] = {
            "one mode held for the whole song: mixo",
            "(I7) or dorian (im). the BREAK drops to",
            "drums+bass. guitars=GUITAR horns=REED+BRASS",
        };
        rad_help_panel("AFROBEAT FM", HELP, 8, NOTES, 3, CLR_ORANGE);
    }
    rad_band_panel(&band, CLR_ORANGE);
    ui_end();
}
