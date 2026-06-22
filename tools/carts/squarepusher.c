// ── SQUAREPUSHER FM ──────────────────────────────────────────────────────────
// Tom Jenkinson's bass-virtuoso drill'n'bass, generated forever. The sibling of
// braindance.c (the Aphex station) but a DIFFERENT beast: here the BASS is the
// protagonist — a fretless/slap fusion bass tearing melodic lines over jazz-fusion
// changes and hyper-edited breaks — and the track LURCHES between manic drill chaos
// and tender fusion-ballad ("Tommib"). Full design: docs/design/squarepusher-blind-brief.md.
//
// The brains (cart-side, over radio.h's clock):
//   • THE BASS AS LEAD — a Hosono-style counterpoint walker (ymo.c) cranked to
//     virtuoso density: fast chord-tone runs with directional inertia, chromatic
//     approaches, octave pops, and a SLAP transient (a noise pop on the accents).
//     In SOLO sections the improviser (improv.h) takes over and shreds.
//   • JAZZ-FUSION HARMONY — cocktail.c's functional ii-V walk (maj7/m7/dom7/m7b5),
//     cadence-pinned per section. The bass improvises OVER moving changes, not a
//     frozen pool — the thing that separates this from braindance's modal float.
//   • RATCHETS + VOLATILITY GRAMMAR — braindance.c's rhythm brains, reused and
//     pushed hotter in the manic sections.
//   • THE TWO-MODE LURCH (form) — a track is a sequence of MODE blocks (tender /
//     fusion / solo / manic) that detonate into each other. The rolled FORM is the
//     balance: ballad-burst vs workout vs lurch.
//
//   SPACE next track   R replay (bass plays it anew)   [ ] history   M radio on/off
//   LEFT/RIGHT density   UP/DOWN tempo   T tone   B band   H help

#include "studio.h"
#include "radio.h"
#include "improv.h"
#include <stdio.h>
#include <math.h>

#define SP_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots (5..13) ────────────────────────────────────────────────
#define I_BASS   5   // the lead/slap fusion bass (SAW + ladder + fast cutoff env)
#define I_SLAP   6   // the slap-pop transient (NOISE click on accents)
#define I_KICK   7
#define I_SNARE  8
#define I_HAT    9
#define I_RHODES 10  // fusion comping (EPIANO)
#define I_PAD    11  // lush string bed (SAW, the tender interlude)
#define I_ACID   12  // 303 squelch (SAW resonant + per-note cutoff env)
#define I_TOM    13  // pitched shred tom (roll fills)

#define BARSTEPS 16

static int iabs(int v) { return v < 0 ? -v : v; }

// ── jazz-fusion harmony (cocktail.c's tables — proven moving changes) ────────
enum { Q_MAJ7, Q_MIN7, Q_DOM7, Q_M7B5, Q_MIN6, NQ };
static const char *QN[NQ] = { "maj7", "m7", "7", "m7b5", "m6" };
static const int QV[NQ][3] = {            // rootless comp voicing (3/7/9)
    { 4, 11, 14 }, { 3, 10, 14 }, { 4, 10, 14 }, { 3, 6, 10 }, { 3, 9, 14 },
};
static const int CT[NQ][4] = {            // chord tones for the bass walker (root/3/5/7)
    { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 4, 7, 10 }, { 0, 3, 6, 10 }, { 0, 3, 7, 9 },
};
enum { F_I, F_ii, F_iii, F_IV, F_V, F_vi, F_II7, F_VI7, F_bII7, F_iv, NF };
static const int F_OFF[NF]  = { 0, 2, 4, 5, 7, 9, 2, 9, 1, 5 };
static const int F_QUAL[NF] = { Q_MAJ7, Q_MIN7, Q_MIN7, Q_MAJ7, Q_DOM7,
                                Q_MIN7, Q_DOM7, Q_DOM7, Q_DOM7, Q_MIN6 };
static const int FNEXT[NF][6] = {
    { F_vi, F_ii, F_IV, F_iii, F_VI7, F_ii },
    { F_V, F_V, F_V, F_bII7, F_V, F_iii },
    { F_vi, F_VI7, F_ii, F_vi, F_IV, F_vi },
    { F_iv, F_ii, F_I, F_V, F_ii, F_iii },
    { F_I, F_I, F_I, F_vi, F_iii, F_I },
    { F_ii, F_II7, F_ii, F_iv, F_ii, F_VI7 },
    { F_V, F_V, F_V, F_ii, F_V, F_V },
    { F_ii, F_ii, F_ii, F_ii, F_ii, F_ii },
    { F_I, F_I, F_I, F_I, F_iii, F_I },
    { F_I, F_I, F_bII7, F_I, F_I, F_I },
};
static const int SOLOSCALE[7] = { 0, 2, 3, 5, 7, 9, 10 };   // dorian — the fusion solo scale

// ── the modes — the per-section character (the lurch) ────────────────────────
enum { M_TENDER, M_FUSION, M_SOLO, M_MANIC, NMODE };
static const char *MODENM[NMODE] = { "tender", "fusion", "solo", "manic" };
// drum density · volatility · bassMode(0 walk/1 shred/2 improv/3 lyrical) · comp · pad · acid · halftime
static const struct { float drum, volat; int bass, comp, pad, acid, half; } MD[NMODE] = {
    { 0.06f, 0.05f, 3, 1, 1, 0, 1 },   // tender — near-beatless, lyrical bass, Rhodes+pad
    { 0.60f, 0.30f, 0, 1, 0, 0, 1 },   // fusion — half-time pocket, walking bass, comping
    { 0.86f, 0.66f, 2, 1, 0, 1, 0 },   // solo   — busy, the improviser SHREDS over changes
    { 1.00f, 0.95f, 1, 0, 0, 1, 0 },   // manic  — max chaos, 16th shred bass, acid
};

// ── the form — a rolled sequence of mode blocks (4 bars each); the balance ───
#define MAXSECT 10
static const struct { int n; int s[MAXSECT]; const char *name; } FORMS[] = {
    { 6, { M_TENDER, M_FUSION, M_SOLO, M_MANIC, M_FUSION, M_TENDER }, "ballad-burst" },
    { 8, { M_FUSION, M_MANIC, M_SOLO, M_MANIC, M_TENDER, M_MANIC, M_SOLO, M_MANIC }, "workout" },
    { 7, { M_TENDER, M_FUSION, M_MANIC, M_SOLO, M_TENDER, M_MANIC, M_FUSION }, "lurch" },
};
#define NFORMS 3

// ── the generated track ──────────────────────────────────────────────────────
typedef struct {
    int   keyPc, form;
    int   fn[40];               // the fusion progression, one per bar
    int   acidOn[16], acidDeg[16], acidN;
    int   bassWave;             // INSTR_SAW or INSTR_SQUARE (fuzz)
    float fret;                 // glide amount (fretless 0..) — performance flavor
    char  title[24];
    float freq;
    unsigned seed;
} Track;

static Track      trk;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 125.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)   rad_srnd(&rs, (n))

static int   tempo     = 172;
static int   intensity = 2;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 2;
static int   songCount = 0;
static float vu = 0, bassVu = 0, drumVu = 0;

static int   gvComp[3] = { 60, 64, 67 };
static bool  compInit  = false;
static int   bassWalk  = 40, bassDir = 1;
static Improv solo;

static RadBand band;
static int chBass, chKit, chKeys;

static void apply_chair(int idx);

// ── the RATCHET (braindance.c) — a hit shredded into n sub-hits ──────────────
static void ratchet(int dly, int midi, int slot, int vol, int n, int subMs, int pitchStep) {
    if (n < 1) n = 1;
    for (int k = 0; k < n; k++) {
        int v = vol - (k * vol) / (n + 1); if (v < 1) v = 1;
        schedule_hit(dly + k * subMs, midi + k * pitchStep, slot, v, subMs + 12);
    }
}

// ── THE BASS — a Hosono walker (ymo.c) cranked to fusion-virtuoso ───────────
// walks chord tones with directional inertia; chromatic approaches and octave
// pops are the virtuosity. lo..hi = the register window for this mode.
static int bass_note(int croot, int q, int lo, int hi, int chromPct) {
    if (chance(20)) bassDir = -bassDir;
    int best = bassWalk, bestScore = -999;
    for (int t = 0; t < 4; t++) {
        int pc = (croot + CT[q][t]) % 12;
        for (int m = lo - (lo % 12) + pc; m <= hi; m += 12) {
            if (m < lo) continue;
            int score = 12 - iabs(m - (bassWalk + bassDir * 2)) + rnd(3);
            if (m == bassWalk) score -= 4;
            if (score > bestScore) { bestScore = score; best = m; }
        }
    }
    if (chance(chromPct)) best += (bassDir > 0 ? -1 : 1);      // chromatic approach
    if (chance(10))       best += (best > (lo + hi) / 2 ? -12 : 12);   // the octave pop
    while (best < lo) best += 12; while (best > hi) best -= 12;
    if (best >= bassWalk + 3) bassDir = 1; else if (best <= bassWalk - 3) bassDir = -1;
    bassWalk = best;
    return best;
}
// a bass hit with the slap-pop transient layered on the accent
static void bass_hit(int dly, int midi, int vol, int gateMs, bool slap) {
    schedule_hit(dly, midi, I_BASS, vol, gateMs);
    if (slap) schedule_hit(dly, 70, I_SLAP, vol >= 5 ? 4 : 3, 26);
    bassVu += 1.6f; vu += 1.4f;
}

// ── track generation ─────────────────────────────────────────────────────────
static const char *TW1[] = { "Beep", "Vic", "Dark", "Red Hot", "Iambic", "Greenways",
    "Dial", "Papalon", "Tommib", "Massif", "Hard", "Port" };
static const char *TW2[] = { "Street", "Acid", "Steering", "Car", "Poetry", "Trajectory",
    "M", "Rashana", "Normal", "Rotted", "Selector", "Plus" };

static void new_track(double pos, unsigned seed) {
    trk.seed = rad_seed_begin(&rs, seed);
    trk.keyPc   = srnd(12);
    int fr = srnd(100); trk.form = fr < 35 ? 0 : fr < 72 ? 1 : 2;
    trk.bassWave = srnd(100) < 70 ? INSTR_SAW : INSTR_SQUARE;
    trk.fret = 0.0f + srnd(5) * 0.01f;

    // pre-roll the fusion progression (cadence into the last 2 bars of each 4-bar section)
    int f = F_I;
    for (int b = 0; b < 40; b++) {
        int ib = b % 4;
        if (ib == 0 && b % 8 == 0) f = F_I;
        else if (ib == 2) f = F_ii;
        else if (ib == 3) f = (srnd(100) < 22) ? F_bII7 : F_V;
        else f = FNEXT[f][srnd(6)];
        trk.fn[b] = f;
    }
    // the acid line (1-bar, 16-step)
    trk.acidN = 0; int d = srnd(3);
    for (int s = 0; s < 16 && trk.acidN < 16; s++)
        if (srnd(100) < 52) {
            trk.acidOn[trk.acidN] = s; d += srnd(5) - 2;
            if (d < -2) d = -2; if (d > 7) d = 7;
            trk.acidDeg[trk.acidN] = d; trk.acidN++;
        }
    if (trk.acidN < 3) { trk.acidN = 3; trk.acidOn[0]=0; trk.acidOn[1]=6; trk.acidOn[2]=11;
                         trk.acidDeg[0]=0; trk.acidDeg[1]=3; trk.acidDeg[2]=5; }

    snprintf(trk.title, sizeof trk.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    trk.freq = 88.0f + srnd(190) * 0.1f;
    tempo = 150 + srnd(34);   // 150..183
    bpm(tempo);

    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);
    apply_chair(chBass);      // re-aim the bass wave/glide for this track
    songBase = (long)pos + BARSTEPS;
    compInit = false; bassWalk = 40; bassDir = 1;
    songCount++;
}
static void fresh_track(double pos) { new_track(pos, 0); rad_hist_log(&rs); }

// ── form / harmony ──────────────────────────────────────────────────────────
static int  form_sects(void) { return FORMS[trk.form].n; }
static long song_bars(void)  { return (long)FORMS[trk.form].n * 4; }
static int  mode_of(long bar) {
    int x = (int)(bar / 4), n = FORMS[trk.form].n;
    return x < n ? FORMS[trk.form].s[x] : M_TENDER;
}
static int fn_at(long bar)  { return trk.fn[bar < 0 ? 0 : bar % 40]; }
static int root_pc(int f)   { return (trk.keyPc + F_OFF[f]) % 12; }

// ── the step player ─────────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  step = (int)(s % BARSTEPS);
    long bar  = s / BARSTEPS;
    if (bar >= song_bars()) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  mode = mode_of(bar);
    int  f    = fn_at(bar);
    int  q    = F_QUAL[f];
    int  croot = root_pc(f);
    float dens = MD[mode].drum * (0.55f + intensity * 0.16f); if (dens > 1) dens = 1;
    float V    = MD[mode].volat + (intensity - 2) * 0.08f; if (V < 0) V = 0; if (V > 1) V = 1;
    bool  half = MD[mode].half;
    int   hh   = (int)(stepMs / 2), hq = (int)(stepMs / 4), ht = (int)(stepMs / 3);

    // ── DRUMS — braindance grammar, scaled by the mode ──
    if (dens > 0.05f) {
        if (step == 0) { schedule_hit(dly, 36, I_KICK, 5, 150); drumVu += 2; vu += 1.5f; }
        if (!half && (step == 6 || step == 10) && chance((int)(V * 55)))
            schedule_hit(dly, 36, I_KICK, 5, 130);
        bool back = half ? (step == 8) : (step == 4 || step == 12);
        if (back) {
            if (chance((int)(V * 55)) && step >= 8) ratchet(dly, 60, I_SNARE, 5, 2 + rnd(4), hh, rnd(3));
            else schedule_hit(dly, 60, I_SNARE, 5, 70);
            drumVu += 2.5f;
        } else if (chance((int)(V * 45))) {
            if (chance((int)(V * 60))) ratchet(dly + rnd(8), 60, I_SNARE, 3, 2 + rnd(5), chance(50) ? hh : hq, rnd(4));
            else schedule_hit(dly + rnd(8), 60, I_SNARE, 2, 35);
            drumVu += 1.0f;
        }
        int hatEvery = (dens > 0.7f) ? 1 : 2;
        if (step % hatEvery == 0) {
            int hv = (step % 4 == 0) ? 2 : 1;
            if (chance((int)(V * 30))) ratchet(dly + rnd(3), 90, I_HAT, hv, 2 + rnd(3), hq, 0);
            else schedule_hit(dly + rnd(3), 90, I_HAT, hv, 22);
        }
        if (step >= 12 && chance((int)(V * 50)))
            ratchet(dly + rnd(4), 48 + rnd(6), I_TOM, 4, 3 + rnd(5), chance(50) ? hh : ht, chance(60) ? 2 : -2);
    }

    // ── THE BASS — the protagonist ──
    if (MD[mode].bass == 3) {                      // LYRICAL (tender): sparse, sung, higher
        if (step == 0 || (step == 8 && chance(60))) {
            int n = bass_note(croot, q, 40, 57, 0);
            bass_hit(dly + 4, n, 4, (int)(stepMs * (half ? 7 : 4)), false);
        }
    } else if (MD[mode].bass == 2) {               // SOLO: the improviser shreds over the changes
        int secBar = (int)(bar % 4);
        if (secBar == 0 && step == 0) improv_begin(&solo, 38, 4, 1.0f);
        if (step == 0 && bar % 2 == 0 && improv_due(&solo, secBar)) improv_render(&solo, secBar, SOLOSCALE);
        float arc = improv_arc(&solo, secBar);
        int cs = (int)(s % 32);
        for (int i = 0; i < solo.n; i++)
            if (solo.onset[i] == cs) {
                int mm = improv_midi(&solo, i, secBar, trk.keyPc, SOLOSCALE, 18);
                while (mm < 36) mm += 12; while (mm > 60) mm -= 12;
                bass_hit(dly + 6, mm, arc > 0.6f ? 5 : 3, (int)(stepMs * 1.6f), chance(35));
            }
    } else {                                       // 0 walk-groove / 1 shred
        bool shred = (MD[mode].bass == 1);
        int every = shred ? 1 : 2;                 // 16ths vs 8ths
        if (half && !shred) every = 4;             // the half-time fusion pocket = quarters + runs
        if (step % every == 0 && chance(shred ? 90 : 80)) {
            int n = bass_note(croot, q, 28, 52, shred ? 25 : 12);
            bool slap = (step % 4 == 0) || (shred && chance(30));
            bass_hit(dly + 4, n, slap ? 5 : 3, (int)(stepMs * (shred ? 0.9f : 1.6f)), slap);
            if (shred && chance((int)(V * 35)))    // a virtuoso burst
                ratchet(dly + hh, n + 12, I_BASS, 4, 2 + rnd(3), hq, chance(50) ? 1 : -1);
        }
        // a fusion fill: a fast run into the bar in the half-time pocket
        if (!shred && half && step >= 12 && chance(40)) {
            int n = bass_note(croot, q, 28, 52, 30);
            bass_hit(dly + 2, n, 3, hq + 10, false);
        }
    }

    // ── RHODES comping — fusion extended voicings on the changes ──
    if (MD[mode].comp && step == 0) {
        rad_lead_to(croot, QV[q], gvComp, 3, 54, 74, &compInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 7 + rnd(4), gvComp[k], I_RHODES, mode == M_TENDER ? 3 : 2,
                         (int)(stepMs * (half ? 10 : 6)));
        vu += 1.0f;
    }
    // a few comp chips off the beat in fusion (the Rhodes pushes)
    if (MD[mode].comp && mode == M_FUSION && (step == 6 || step == 11) && chance(45))
        for (int k = 0; k < 3; k++) schedule_hit(dly + k * 5, gvComp[k], I_RHODES, 2, (int)(stepMs * 2));

    // ── PAD bed — the tender interlude ──
    if (MD[mode].pad && step == 0 && bar % 2 == 0) {
        int iv[3] = { 0, CT[q][1], CT[q][2] };
        static int gvPad[3]; static bool padI = false;
        rad_lead_to(croot, iv, gvPad, 3, 48, 64, &padI);
        for (int k = 0; k < 3; k++) schedule_hit(dly + k * 9, gvPad[k], I_PAD, 3, (int)(stepMs * 28));
    }

    // ── ACID 303 — manic/solo, a per-bar squelch ──
    if (MD[mode].acid && intensity >= 1) {
        for (int i = 0; i < trk.acidN; i++)
            if (trk.acidOn[i] == step) {
                int mm = 36 + croot + SOLOSCALE[trk.acidDeg[i] % 7];
                while (mm > 55) mm -= 12;
                schedule_hit(dly + 6, mm, I_ACID, chance(30) ? 5 : 3, (int)(stepMs * 1.5f));
            }
    }
}

// ── the band chairs ──────────────────────────────────────────────────────────
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chBass) {
        // the lead/slap fusion bass — moog-ish resonant path with a fast cutoff env
        instrument(I_BASS, trk.bassWave, 2, 150, 4, 150);
        instrument_filter(I_BASS, FILTER_LADDER, sel == 0 ? 700 : 1100, sel == 0 ? 6 : 9);
        instrument_drive(I_BASS, sel == 0 ? 0.40f : 0.62f);     // sel1 = fuzz bass
        instrument_env(I_BASS, 0, ENV_CUTOFF, 2, 130, 1900);    // the slap/pluck snap
        instrument(I_SLAP, INSTR_NOISE, 0, 22, 0, 16);          // the pop transient
        instrument_filter(I_SLAP, FILTER_BAND, 2600, 5);
    } else if (idx == chKit) {
        if (sel == 0) {                                          // amen shred kit
            instrument(I_KICK, INSTR_SINE, 0, 95, 0, 35); instrument_env(I_KICK, 0, ENV_PITCH, 0, 50, 16);
            instrument(I_SNARE, INSTR_NOISE, 0, 85, 0, 40); instrument_filter(I_SNARE, FILTER_BAND, 1900, 6);
            instrument_env(I_SNARE, 0, ENV_PITCH, 0, 25, 12);
        } else {                                                 // 808 box
            instrument(I_KICK, INSTR_SINE, 0, 220, 0, 60); instrument_filter(I_KICK, FILTER_LOW, 250, 3);
            instrument_env(I_KICK, 0, ENV_PITCH, 0, 50, 24);
            instrument(I_SNARE, INSTR_NOISE, 0, 120, 0, 50); instrument_filter(I_SNARE, FILTER_BAND, 1500, 4);
        }
        instrument(I_HAT, INSTR_NOISE, 0, 18, 0, 14); instrument_filter(I_HAT, FILTER_HIGH, 7800, 3);
        instrument(I_TOM, INSTR_SINE, 0, 90, 0, 40); instrument_env(I_TOM, 0, ENV_PITCH, 0, 60, 8);
    } else if (idx == chKeys) {
        // fusion Rhodes (epiano) + lush string pad
        instrument(I_RHODES, INSTR_EPIANO, 2, 0, 6, 700);
        instrument_harmonics(I_RHODES, sel == 0 ? 0.0f : 0.5f);   // Rhodes vs Wurli
        instrument_timbre(I_RHODES, 0.4f); instrument_morph(I_RHODES, 0.35f);
        instrument_filter(I_RHODES, FILTER_LOW, 2400, 1);
        instrument(I_PAD, INSTR_SAW, 220, 400, 6, 900);
        instrument_filter(I_PAD, FILTER_LOW, 1700, 2);
        instrument_tune(I_PAD, 0.05f);                            // a touch of width
        instrument(I_ACID, trk.bassWave, 2, 60, 5, 60);
        instrument_filter(I_ACID, FILTER_LOW, 900, 12);
        instrument_env(I_ACID, 0, ENV_CUTOFF, 0, 110, 2300);
        instrument_drive(I_ACID, 0.35f);
    }
}

static void setup_instruments(void) {
    chBass = rad_chair(&band, "bass", "fretless", "fuzz", NULL, NULL);
    chKit  = rad_chair(&band, "kit",  "amen", "808", NULL, NULL);
    chKeys = rad_chair(&band, "keys", "rhodes", "wurli", NULL, NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_RHODES, FILTER_LOW, (int)(2400 * tm), 1);
    instrument_filter(I_PAD,    FILTER_LOW, (int)(1700 * tm), 2);
    instrument_filter(I_HAT,    FILTER_HIGH, (int)(7800 * tm), 3);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (SP_SEED) { new_track(pos, SP_SEED); rad_hist_log(&rs); }
        else fresh_track(pos);
        scheduled = (long)pos;
        apply_tone();
        booted = true;
    }

    int ev = rad_input(&tempo, 130, 188, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_track(pos);
    if (ev & RAD_EV_REPLAY) new_track(pos, trk.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_track(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_track(pos, s); }
    if (ev & RAD_EV_POWER)  { if (!radioOn) { note_off_all(); sfx(-1); } else scheduled = (long)pos; }
    if (ev & (RAD_EV_TONE | RAD_EV_NEW | RAD_EV_REPLAY | RAD_EV_BACK | RAD_EV_FWD)) apply_tone();

    int chair = rad_band_input(&band, &showHelp);
    if (chair >= 0) { apply_chair(chair); apply_tone(); }

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
        long songStep = scheduled - songBase;
        if (songStep >= song_bars() * BARSTEPS) fresh_track(pos);
    }
    vu *= 0.88f; if (vu > 12) vu = 12;
    bassVu *= 0.82f; if (bassVu > 12) bassVu = 12;
    drumVu *= 0.80f; if (drumVu > 12) drumVu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / BARSTEPS : 0;
    watch("track", "%d", songCount);
    watch("form", "%s", FORMS[trk.form].name);
    watch("mode", "%s", MODENM[mode_of(tbar)]);
    watch("key", "%s", RAD_PCNAME[trk.keyPc]);
    watch("chord", "%s%s", RAD_PCNAME[root_pc(fn_at(tbar))], QN[F_QUAL[fn_at(tbar)]]);
    watch("tempo", "%d", tempo);
#endif
}

// ── draw — the scope/mask window (the bass made visible) ─────────────────────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / BARSTEPS : 0;
    int  mode = mode_of(bar);

    rad_body(CLR_DARKER_GREY, CLR_BLUE_GREEN);          // chrome on near-black, cyan scope accent
    rad_dial(trk.freq, CLR_BLUE_GREEN);

    // the window — an oscilloscope the bass drives, drum hits tear it; a faint LED mask
    rectfill(34, 52, 102, 116, CLR_BLACK);
    int cx = 85, cy = 110;
    // the mask: two eye-slits that flash on the drums
    int eye = drumVu > 4 ? CLR_BLUE_GREEN : CLR_DARK_GREEN;
    rectfill(48, 74, 22, 6, eye); rectfill(100, 74, 22, 6, eye);
    // the bass scope trace (a Lissajous-ish glow, amplitude = bassVu)
    int amp = 8 + (int)(bassVu * 2.2f);
    int jit = (int)drumVu;
    int prevx = 36, prevy = cy;
    for (int x = 36; x < 134; x += 2) {
        float ph = (x - 36) * 0.16f + timer() * 5.0f;
        int y = cy + (int)(sinf(ph) * amp * (0.6f + 0.4f * sinf(ph * 0.5f)));
        if (jit > 3 && (x % 10 < 2)) y += rnd(jit * 2) - jit;     // glitch tears on the drums
        line(prevx, prevy, x, y, CLR_BLUE_GREEN);
        prevx = x; prevy = y;
    }
    // the bass level bar (the protagonist) + a drum bar
    int bh = (int)(bassVu / 12.0f * 40), dh = (int)(drumVu / 12.0f * 40);
    rectfill(40, 156 - bh, 6, bh, CLR_YELLOW);    print("B", 41, 158, CLR_YELLOW);
    rectfill(124, 156 - dh, 6, dh, CLR_RED);      print("D", 125, 158, CLR_RED);
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_BLUE_GREEN);
    if (radioOn) {
        print(trk.title, 154, 58, CLR_BLUE_GREEN);
        char l2[32];
        snprintf(l2, 32, "%s  %s", FORMS[trk.form].name, RAD_PCNAME[trk.keyPc]);
        print(l2, 154, 70, CLR_LIGHT_GREY);
        snprintf(l2, 32, "%d bpm #%08X", tempo, trk.seed);
        print(l2, 154, 82, CLR_LIGHT_GREY);
        float vt = vu / 12.0f; rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_BLUE_GREEN);
    } else print("- silence -", 178, 70, CLR_DARK_GREY);

    if (radioOn) {
        int mc = (mode == M_MANIC) ? CLR_RED : (mode == M_TENDER) ? CLR_BLUE_GREEN : CLR_YELLOW;
        print(MODENM[mode], 152, 104, mc);
        rad_phrase_dots(232, 108, form_sects(), bar / 4, CLR_BLUE_GREEN);
    }

    static const char *FEEL[4] = { "calm", "warm", "hot", "mental" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_BLUE_GREEN);
    if (rad_knob_int(&tempo, 130, 188, 2, 218, 148, 9, "tempo", CLR_BLUE_GREEN)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_BLUE_GREEN)) apply_tone();
    rad_power_led(radioOn, CLR_BLUE_GREEN, CLR_DARK_GREEN);

    rad_help_button(CLR_BLUE_GREEN);
    rad_band_button(CLR_BLUE_GREEN);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next track (new seed)" },
            { "R",          "replay - the bass plays anew" },
            { "[ / ]",      "back / forward history" },
            { "LEFT/RIGHT", "density - calm..mental" },
            { "UP/DOWN",    "tempo" },
            { "T",          "tone" },
            { "M",          "radio on / off" },
            { "B",          "band - bass/kit/keys" },
        };
        static const char *NOTES[3] = {
            "the BASS is the lead - it walks the fusion",
            "changes, shreds the solos, slaps the accents.",
            "the track LURCHES tender<->manic.",
        };
        rad_help_panel("SQUAREPUSHER FM", HELP, 8, NOTES, 3, CLR_BLUE_GREEN);
    }
    rad_band_panel(&band, CLR_BLUE_GREEN);
    ui_end();
}
