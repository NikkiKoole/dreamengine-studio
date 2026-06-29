/* de:meta
{
  "title": "plantasia radio",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "analog-voice-modeling"
  ],
  "lineage": "Homage to Mort Garson's Plantasia (1976); introduces melody brain #3 — a seeded theme-and-variation generator with an A-A′-B-A songform and key lift — and models the Moog signal chain (INSTR_SAW → FILTER_LADDER + ENV_CUTOFF wow + DRIVE) four ways across the band.",
  "homage": "Mort Garson - Mother Earth's Plantasia (1976)",
  "description": "Warm earth music for plants, after Mort Garson's Mother Earth's Plantasia (1976): bright, bouncy, whimsical little Moog SONGS, each for a different houseplant. The family's first MELODY-FORWARD station - the lead is the protagonist, not buried in the mix. Two new things: THE SONGWRITER (melody brain #3), a SEEDED theme-and-variation generator - a singable hook invented from the seed (the SAME every replay, unlike a solo) and developed across an A-A'-B-A songform with a final key-lift, sung by the mono Moog lead with portamento glide + vibrato (a held voice driven per-frame so it can slide); and the TRACK FEELS - the seed rolls one of five named feels (sprightly / swing / waltz / rubato / green), each bundling tempo, meter, groove, kit and density. The band is the dream-synth (moog) signal path four ways: gliding lead, springy filter-pluck bass, burbling sequencer arp, warm pad bed, celesta sparkle, a light synth kit. The window GROWS A HOUSEPLANT as the song plays - sprout, leaves unfurling, a flower opening on the outro; species seed-rolled, the seed on a nursery price-tag. SPACE next, R same seed, [ ] history, LEFT/RIGHT density, UP/DOWN tempo, T tone, M power, B band, H help. Pin via PLANTASIA_SEED."
}
de:meta */
// ── PLANTASIA RADIO — "warm earth music for plants" ────────────────────────
// After Mort Garson's *Mother Earth's Plantasia* (1976): bright, bouncy, whimsical
// little Moog SONGS, each one for a different houseplant. The family's first
// MELODY-FORWARD station — the lead is the protagonist, not buried in the mix.
//
// Two new things make this station:
//   • THE SONGWRITER (melody brain #3) — a SEEDED theme-and-variation generator.
//     A singable hook is invented from rad_srnd (so it's the song's identity, the
//     SAME every replay — unlike improv.h's per-take solos), then developed across
//     an A-A'-B-A songform: state it, restate up an octave, a contrasting bridge,
//     recap, and a final key-LIFT on the outro. The mono Moog lead sings it with
//     portamento glide + vibrato (a HELD voice driven per-frame, so it can slide —
//     schedule_hit can't).
//   • LEAN ON THE TRACK FEELS — the seed rolls one of five named feels drawn from
//     the album (SPRIGHTLY / SWING / WALTZ / RUBATO / GREEN); each bundles tempo,
//     meter, groove, kit, density and articulation into one coherent character.
//
// The band is the Moog four ways — the dream-synth (moog.c) signal path on loan,
// one slot per role: INSTR_SAW -> FILTER_LADDER + per-note ENV_CUTOFF "wow" + DRIVE.
// Lead (gliding, singing), springy filter-pluck bass, burbling sequencer arp, a
// warm pad bed; a celesta MALLET sparkles on top; a light synth kit per feel.
//
// The window GROWS A HOUSEPLANT as the song plays — sprout, leaves unfurling, a
// flower opening on the outro; the species is seed-rolled, the seed on a price-tag.
//
//   SPACE/tune next   R same seed   [ ] history   LEFT/RIGHT density
//   UP/DOWN tempo   T tone   M power   B the band   H or ? help
//
// Blind brief + palette shop: docs/design/plantasia-blind-brief.md.

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define PLANTASIA_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_LEAD 5   // the Moog lead (held mono, gliding) — moog.c LEAD patch
#define I_BASS 6   // the springy filter-pluck bass     — moog.c BASS patch
#define I_ARP  7   // burbling sequencer line
#define I_PAD  8   // warm chord bed (slow filter swell)
#define I_BELL 9   // celesta sparkle (MALLET)
#define SL_KICK 10
#define SL_HAT  11
#define SL_WOOD 12 // woodblock / clave tick

// ── scales — bright, never dark ────────────────────────────────────────────
static const int MODESC[4][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },   // major
    { 0, 2, 4, 6, 7, 9, 11 },   // lydian
    { 0, 2, 4, 5, 7, 9, 10 },   // mixolydian
    { 0, 2, 3, 5, 7, 9, 10 },   // dorian (the swing/bluesy feel)
};
static const char *MODENAME[4] = { "major", "lydian", "mixolyd", "dorian" };

// ── harmony — cheerful functional major + a little vaudeville color ────────
enum { Q_MAJ, Q_MIN, Q_DOM, NQ };
static const int QV[NQ][3] = {
    { 4, 7, 11 },   // maj7 (rootless-ish; the bass owns the root)
    { 3, 7, 10 },   // m7
    { 4, 7, 10 },   // 7
};
enum { F_I, F_ii, F_iii, F_IV, F_V, F_vi, F_II7, F_VI7, NF };
static const int F_OFF[NF]  = { 0, 2, 4, 5, 7, 9, 2, 9 };
static const int F_QUAL[NF] = { Q_MAJ, Q_MIN, Q_MIN, Q_MAJ, Q_DOM, Q_MIN, Q_DOM, Q_DOM };
// where each function likes to go (bright, home-pulling)
static const int FNEXT[NF][6] = {
    { F_IV, F_vi, F_ii, F_V, F_iii, F_IV },     // I
    { F_V,  F_V,  F_V,  F_V,  F_II7, F_iii },   // ii -> V
    { F_vi, F_IV, F_ii, F_vi, F_VI7, F_vi },    // iii
    { F_V,  F_I,  F_ii, F_V,  F_II7, F_iii },   // IV
    { F_I,  F_I,  F_vi, F_I,  F_iii, F_I },      // V -> home
    { F_ii, F_IV, F_II7,F_ii, F_VI7, F_ii },    // vi
    { F_V,  F_V,  F_V,  F_V,  F_V,   F_V },      // II7 -> V
    { F_ii, F_ii, F_ii, F_ii, F_ii,  F_ii },     // VI7 -> ii
};

// ── the form — a little Moog SONG (8 sections × 4 bars) ─────────────────────
enum { S_INTRO, S_A, S_A2, S_B, S_OUT };
static const int FORM[8] = { S_INTRO, S_A, S_A2, S_B, S_A, S_A2, S_B, S_OUT };
static const char *SECTNAME[5] = { "intro", "theme", "theme'", "bridge", "outro" };

// ── the feels — the variety spine (lean on the track feels) ─────────────────
enum { FEEL_SPRIGHTLY, FEEL_SWING, FEEL_WALTZ, FEEL_RUBATO, FEEL_GREEN, NFEEL };
typedef struct {
    const char *name, *anchor;
    int  tmin, tmax;    // tempo roll
    int  spb;           // steps per bar (16 = 4/4, 12 = 3/4 waltz)
    int  swing;         // 50 straight .. 62 swung
    int  kit;           // 0 none, 1 straight, 2 swing, 3 waltz, 4 bossa
    int  arp, pad;      // burble / warm bed present
    int  base;          // arrangement density base
    int  glide;         // lead portamento ms
} FeelDef;
static const FeelDef FEEL[NFEEL] = {
    // name        anchor                       tmin tmax spb swg kit arp pad base glide
    // glide = portamento ms, applied ONLY to connect consecutive close notes (see drive_lead):
    // leaps and phrase re-entries snap, so this is a gentle slur, not a constant swoop.
    { "sprightly", "Spider Plant",               124, 140, 16, 50, 1, 1, 0, 2, 14 },
    { "swing",     "Swingin' Spathiphyllums",    112, 130, 16, 62, 2, 0, 1, 1, 22 },
    { "waltz",     "Mellow Maranta",             100, 116, 12, 50, 3, 0, 1, 1, 28 },
    { "rubato",    "African Violet",              72,  86, 16, 50, 0, 0, 1, 0, 55 },
    { "green",     "Rhapsody in Green",           96, 112, 16, 54, 4, 1, 1, 1, 34 },
};

// ── plants (the window) ─────────────────────────────────────────────────────
typedef struct { const char *name; int leafShape; int leaf, leafDk, flower; int nLeaf; } Plant;
static const Plant PLANTS[6] = {
    { "spider plant",  0, CLR_LIME_GREEN,   CLR_MEDIUM_GREEN, CLR_WHITE,       7 },
    { "philodendron",  1, CLR_MEDIUM_GREEN, CLR_DARK_GREEN,   CLR_LIGHT_YELLOW,5 },
    { "african violet",0, CLR_MEDIUM_GREEN, CLR_DARK_GREEN,   CLR_MAUVE,       6 },
    { "snake plant",   2, CLR_DARK_GREEN,   CLR_MEDIUM_GREEN, CLR_LIGHT_YELLOW,4 },
    { "begonia",       1, CLR_LIME_GREEN,   CLR_MEDIUM_GREEN, CLR_PINK,        6 },
    { "maranta",       0, CLR_MEDIUM_GREEN, CLR_DARK_GREEN,   CLR_PEACH,       6 },
};

// ── the generated song ──────────────────────────────────────────────────────
#define MAXEV 192
typedef struct {
    int keyPc, mode, feel, plant;
    int fn[32];                 // one function per bar, pre-rolled
    int evStep[MAXEV], evMidi[MAXEV], evDur[MAXEV], evN;   // the whole-song melody
    int chrom;                  // vaudeville color amount
    char title[28];
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

static int   tempo     = 120;
static int   intensity = 1;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 2;
static int   songCount = 0;
static float vu        = 0;
static int   gvPad[3]  = { 60, 64, 67 };
static bool  padInit   = false;
static int   leadH     = -1;     // the held mono lead
static int   leadEvt   = -2;     // last melody event the lead was on
static bool  leadGap   = true;   // was the lead silent (a rest) right before this note?
static int   bassLast  = 33;

static RadBand band;
static int chLead, chBass, chBell;

static int spb(void)        { return FEEL[sng.feel].spb; }
static int total_bars(void) { return 32; }
static int total_steps(void){ return total_bars() * spb(); }

// ── deg -> midi in the song's scale ─────────────────────────────────────────
static int deg_to_midi(int deg, int base, const int *sc) {
    int oct = deg / 7, idx = deg % 7;
    if (idx < 0) { idx += 7; oct -= 1; }
    return base + oct * 12 + sc[idx];
}

// ── THE SONGWRITER — seeded theme-and-variation (melody brain #3) ───────────
// generate one 4-bar phrase (onsets + degrees + durs) into out arrays; the
// contour rises to a half-open midpoint then resolves to the tonic — a hook you
// can hum. Pure rad_srnd: the tune is the song, identical every replay.
static const int CELL16[6][7] = {
    { 0,4,8,12,-1,0,0 }, { 0,4,8,10,12,-1,0 }, { 0,6,8,12,-1,0,0 },
    { 0,4,8,-1,0,0,0 },  { 0,8,-1,0,0,0,0 },   { 0,2,4,8,12,-1,0 },
};
static const int CELL12[4][5] = {
    { 0,4,8,-1,0 }, { 0,8,-1,0,0 }, { 0,4,8,10,-1 }, { 0,-1,0,0,0 },
};
static const int STEPW[8] = { 1, -1, 2, -2, 1, -1, 3, -3 };   // contour steps (degrees)

static int gen_phrase(int *os, int *dg, int *du, int sparse) {
    int n = 0, bars = 4, sb = spb();
    int cur = 0;
    for (int b = 0; b < bars; b++) {
        const int *cell;
        if (sb == 12) cell = CELL12[sparse ? 3 - srnd(2) : srnd(4)];
        else          cell = CELL16[sparse ? 3 + srnd(3) : srnd(6)];
        for (int i = 0; cell[i] >= 0 && n < MAXEV - 1; i++) {
            os[n] = b * sb + cell[i];
            dg[n] = cur;
            cur += STEPW[srnd(8)];
            if (cur > 9) cur -= 7;
            if (cur < -2) cur += 7;
            n++;
        }
    }
    // durations = gap to the next onset (legato), capped
    for (int i = 0; i < n; i++) {
        int nx = (i + 1 < n) ? os[i + 1] : bars * sb;
        du[i] = nx - os[i]; if (du[i] > 8) du[i] = 8; if (du[i] < 1) du[i] = 1;
    }
    // shape the arc: half-open at the midpoint, resolve to tonic at the end
    if (n >= 2) {
        int mid = bars * sb / 2;
        int best = 0; for (int i = 1; i < n; i++) if (os[i] <= mid) best = i;
        dg[best] = (srnd(2) ? 4 : 1);     // half-close (the comma)
        dg[n - 1] = 0;                    // the full stop — home
    }
    return n;
}

// transform a theme degree for the section that's playing
static int xform_deg(int sect, int d) {
    if (sect == S_A2) return d + 7;       // restate up an octave
    return d;
}

static void build_melody(void) {
    int tOs[64], tDg[64], tDu[64], tN;        // the A theme
    int bOs[64], bDg[64], bDu[64], bN;        // the contrasting bridge
    tN = gen_phrase(tOs, tDg, tDu, 0);
    bN = gen_phrase(bOs, bDg, bDu, 1);
    for (int i = 0; i < bN; i++) bDg[i] = -bDg[i] + 2;   // invert the bridge around d=1

    const int *sc = MODESC[sng.mode];
    int base = 67 + sng.keyPc - 12;            // the lead register (~G4)
    int sbk = spb();
    sng.evN = 0;
    for (int s = 0; s < 8 && sng.evN < MAXEV; s++) {
        int sect = FORM[s];
        if (sect == S_INTRO) continue;         // intro: pad only, the tune holds back
        int useB = (sect == S_B);
        int n   = useB ? bN : tN;
        const int *os = useB ? bOs : tOs, *dg = useB ? bDg : tDg, *du = useB ? bDu : tDu;
        int lift = (sect == S_OUT) ? 2 : 0;    // the whimsical final key-lift
        for (int i = 0; i < n && sng.evN < MAXEV; i++) {
            int d  = xform_deg(sect, dg[i]);
            int m  = deg_to_midi(d, base, sc) + lift;
            while (m < 60) m += 12; while (m > 88) m -= 12;
            sng.evStep[sng.evN] = s * 4 * sbk + os[i];
            sng.evMidi[sng.evN] = m;
            sng.evDur[sng.evN]  = du[i];
            sng.evN++;
        }
    }
}

// ── instruments ─────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    // the Moog, four ways (moog.c signal path on loan)
    instrument(I_LEAD, INSTR_SAW, 6, 200, 6, 280);
    instrument_filter(I_LEAD, FILTER_LADDER, 1600, 4);
    instrument_drive(I_LEAD, 0.42f);
    instrument_env(I_LEAD, 0, ENV_CUTOFF, 6, 260, 1000);   // the gentle "wow"
    instrument_tune(I_LEAD, 0.05f);                        // a hair of Moog fatness

    instrument(I_BASS, INSTR_SAW, 2, 170, 3, 160);
    instrument_filter(I_BASS, FILTER_LADDER, 540, 6);
    instrument_drive(I_BASS, 0.40f);
    instrument_env(I_BASS, 0, ENV_CUTOFF, 2, 150, 1700);   // the springy pluck snap

    instrument(I_ARP, INSTR_SAW, 2, 90, 0, 110);
    instrument_filter(I_ARP, FILTER_LADDER, 1200, 7);
    instrument_drive(I_ARP, 0.30f);
    instrument_env(I_ARP, 0, ENV_CUTOFF, 1, 80, 900);

    instrument(I_PAD, INSTR_SAW, 60, 300, 5, 360);         // slow filter swell = the bed
    instrument_filter(I_PAD, FILTER_LADDER, 820, 3);
    instrument_drive(I_PAD, 0.20f);
    instrument_env(I_PAD, 0, ENV_CUTOFF, 80, 400, 1500);

    instrument(I_BELL, INSTR_MALLET, 0, 0, 7, 700);
    instrument_harmonics(I_BELL, 0.85f); instrument_timbre(I_BELL, 0.40f); instrument_morph(I_BELL, 0.50f);

    instrument(SL_KICK, INSTR_SINE, 0, 130, 0, 60);
    instrument_filter(SL_KICK, FILTER_LOW, 240, 1);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 32, 9.0f);
    instrument(SL_HAT, INSTR_NOISE, 0, 26, 0, 14);
    instrument_filter(SL_HAT, FILTER_HIGH, 8500, 3);
    instrument(SL_WOOD, INSTR_SINE, 0, 60, 0, 40);
    instrument_filter(SL_WOOD, FILTER_BAND, 1700, 6);
    instrument_env(SL_WOOD, 0, ENV_PITCH, 0, 24, 5.0f);

    reverb(0.42f, 0.30f);
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_LEAD, FILTER_LADDER, (int)(1600 * tm), 4);
    instrument_filter(I_ARP,  FILTER_LADDER, (int)(1200 * tm), 7);
    instrument_filter(I_PAD,  FILTER_LADDER, (int)(820  * tm), 3);
}

// ── the band panel ───────────────────────────────────────────────────────────
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chLead) {
        int w = sel == 0 ? INSTR_SAW : sel == 1 ? INSTR_SQUARE : INSTR_TRI;
        instrument(I_LEAD, w, 6, 200, 6, 280);
        instrument_filter(I_LEAD, FILTER_LADDER, (int)(1600 * RAD_TONEMUL[toneSel]), 4);
        instrument_drive(I_LEAD, 0.42f);
        instrument_env(I_LEAD, 0, ENV_CUTOFF, 6, 260, 1000);
        instrument_tune(I_LEAD, 0.05f);
        if (leadH >= 0) { note_off(leadH); leadH = -1; leadEvt = -2; }   // restart on the new wave
    } else if (idx == chBass) {
        if (sel == 0) { instrument(I_BASS, INSTR_SAW, 2, 170, 3, 160);
                        instrument_env(I_BASS, 0, ENV_CUTOFF, 2, 150, 1700); }   // springy
        else          { instrument(I_BASS, INSTR_SINE, 3, 220, 4, 150);
                        instrument_env(I_BASS, 0, ENV_PITCH, 0, 16, 2); }        // round/sub
        instrument_filter(I_BASS, FILTER_LADDER, 540, 6);
        instrument_drive(I_BASS, 0.40f);
    } else if (idx == chBell) {
        /* on/off handled at play time via band.c[chBell].sel */
    }
}

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Ode to", "Concerto for", "Symphony for", "A Song for",
    "Rhapsody for", "Waltz for", "Lullaby for", "Reverie for" };
static const char *TW2[] = { "a Spider Plant", "a Philodendron", "an African Violet",
    "a Snake Plant", "a Fern", "a Pothos", "a Begonia", "the Maranta", "a Fiddle Fig",
    "a Spathiphyllum" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.feel  = srnd(NFEEL);
    sng.mode  = (sng.feel == FEEL_SWING) ? (srnd(2) ? 3 : 0) : srnd(3);  // swing leans dorian
    sng.keyPc = srnd(12);
    sng.plant = srnd(6);
    sng.chrom = 8 + srnd(22);

    // pre-roll the function walk (32 bars), cadence into the last 2 bars of each 4-bar section
    int f = F_I;
    for (int b = 0; b < 32; b++) {
        int ib = b % 4;
        if (ib == 0 && (b % 8 == 0)) f = F_I;
        else if (ib == 2)            f = F_ii;
        else if (ib == 3)            f = F_V;
        else                         f = FNEXT[f][srnd(6)];
        sng.fn[b] = f;
    }

    build_melody();

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(8)], TW2[srnd(10)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = FEEL[sng.feel].tmin + srnd(FEEL[sng.feel].tmax - FEEL[sng.feel].tmin + 1);
    bpm(tempo);

    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);   // re-assert picks
    if (leadH >= 0) { note_off(leadH); leadH = -1; leadEvt = -2; }
    songBase = (long)pos + 8;
    padInit = false; bassLast = 33;
    songCount++;
}

static void fresh_song(double pos) { new_song(pos, 0); rad_hist_log(&rs); }

// ── form / harmony helpers ───────────────────────────────────────────────────
static int sect_of(long bar) { long x = bar / 4; return (int)(x < 8 ? FORM[x] : S_OUT); }
static int fn_at(long bar)   { return sng.fn[bar < 0 ? 0 : bar % 32]; }
static int root_pc(int f)    { return (sng.keyPc + F_OFF[f]) % 12; }
static int lift_of(long bar) { return sect_of(bar) == S_OUT ? 2 : 0; }

// ── the springy bass ─────────────────────────────────────────────────────────
static int bass_near(int pc, int lo, int hi) { return rad_bass_to(pc, bassLast, lo, hi); }

// ── the step player (everything but the lead, which is driven per-frame) ─────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int   sb   = spb();
    int   step = (int)(s % sb);
    long  bar  = s / sb;
    if (bar >= total_bars()) return;
    const FeelDef *fe = &FEEL[sng.feel];
    int   f    = fn_at(bar);
    int   sect = sect_of(bar);
    int   lvl  = rad_level(fe->base + (sect == S_INTRO || sect == S_OUT ? -1 : 0), intensity);
    int   dly  = rad_step_dly(&clk, abs, pos);
    int   lift = lift_of(bar);
    int   sw   = (step % 4 == 2) ? (int)(stepMs * 4 * (fe->swing - 50) / 100.0) : 0;
    int   rpc  = root_pc(f);

    // ── THE BASS — bouncy, per feel ──
    if (bar >= 1) {
        int blo = 28, bhi = 45;
        if (fe->spb == 12) {                                  // waltz: oom on 1
            if (step == 0) { int n = bass_near(rpc, blo, bhi) + lift;
                schedule_hit(dly + 4, n, I_BASS, 5, (int)(stepMs * 3)); vu += 1.2f; }
        } else if (sng.feel == FEEL_SPRIGHTLY) {              // octave bounce
            if (step % 4 == 0) {
                int lo = bass_near(rpc, blo, bhi) + lift;
                int n = (step % 8 == 0) ? lo : lo + 12;
                schedule_hit(dly + 4, n, I_BASS, 5, (int)(stepMs * 1.6)); vu += 1.1f;
            }
        } else if (sng.feel == FEEL_GREEN) {                  // bossa-ish root/fifth
            if (step == 0 || step == 6 || step == 11) {
                int n = bass_near(step == 6 ? (rpc + 7) % 12 : rpc, blo, bhi) + lift;
                schedule_hit(dly + sw + 4, n, I_BASS, 4, (int)(stepMs * 3)); vu += 1.0f;
            }
        } else if (sng.feel == FEEL_RUBATO) {                 // sparse roots
            if (step == 0) { int n = bass_near(rpc, blo, bhi) + lift;
                schedule_hit(dly + 4, n, I_BASS, 4, (int)(stepMs * 12)); vu += 0.9f; }
        } else {                                              // SWING — walking quarters
            if (step % 4 == 0) {
                int beat = step / 4, n;
                if (beat == 0) n = bass_near(rpc, blo, bhi);
                else if (beat == 3) n = bass_near(root_pc(fn_at(bar + 1)), blo, bhi) + (chance(50) ? -1 : 1);
                else n = bass_near((rpc + (chance(60) ? 7 : 4)) % 12, blo, bhi);
                bassLast = n;
                schedule_hit(dly + sw + 5, n + lift, I_BASS, 4, (int)(stepMs * 3.4)); vu += 1.1f;
            }
        }
    }

    // ── THE PAD — warm chord bed on changes ──
    if (fe->pad && bar >= 1 && lvl >= 1 && step == 0) {
        rad_lead_to(rpc, QV[F_QUAL[f]], gvPad, 3, 55, 74, &padInit);
        int hold = (int)(stepMs * sb * (fe->spb == 12 ? 1.0 : 1.0));
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 6, gvPad[k] + lift, I_PAD, sect == S_OUT ? 2 : 3, hold);
        vu += 0.8f;
    }
    // waltz: pad chord also chips in on beats 2 & 3 (the "pah pah")
    if (fe->spb == 12 && fe->pad && bar >= 1 && lvl >= 1 && (step == 4 || step == 8)) {
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 4, gvPad[k] + lift, I_PAD, 2, (int)(stepMs * 3));
    }

    // ── THE BURBLE — sequencer arp over the chord tones ──
    if (fe->arp && bar >= 1 && lvl >= 1 && (step % 2 == 0) && sect != S_OUT) {
        int idx = (step / 2) % 3;
        int pc  = (rpc + QV[F_QUAL[f]][idx]) % 12;
        int m   = 60 + pc; if (chance(40)) m += 12;
        schedule_hit(dly + sw + 2, m + lift, I_ARP, sect == S_INTRO ? 2 : 3, (int)(stepMs * 1.4));
        vu += 0.5f;
    }

    // ── BELLS — sparse celesta sparkle answering the lead (performance) ──
    if (band.c[chBell].sel == 0 && bar >= 1 && step % 4 == 0 && lvl >= 2 && chance(22)) {
        int pc = (rpc + QV[F_QUAL[f]][rnd(3)]) % 12;
        schedule_hit(dly + 8 + rnd(40), 84 + pc + lift, I_BELL, 3, 700);
    }

    // ── THE KIT — per feel, light ──
    if (fe->kit && bar >= 1 && lvl >= 1 && !(sect == S_OUT && bar % 4 >= 2)) {
        if (fe->kit == 1) {                              // SPRIGHTLY straight
            if (step % 8 == 0) schedule_hit(dly + 1, 36, SL_KICK, 3, 60);
            if (step == 4 || step == 12) schedule_hit(dly + 1, 60, SL_HAT, 2, 30);
            if (step % 4 == 2 && lvl >= 2) schedule_hit(dly + 1, 72, SL_WOOD, 2, 40);
        } else if (fe->kit == 2) {                       // SWING ride+hat
            if (step % 4 == 0) schedule_hit(dly + 1, 60, SL_HAT, step % 8 == 0 ? 2 : 1, 24);
            if (step == 6 || step == 14) schedule_hit(dly + sw + 1, 60, SL_HAT, 1, 20);
            if (step == 4 || step == 12) schedule_hit(dly + 1, 70, SL_WOOD, 2, 36);
        } else if (fe->kit == 3) {                       // WALTZ 1-2-3
            if (step == 0) schedule_hit(dly + 1, 36, SL_KICK, 3, 60);
            if (step == 4 || step == 8) schedule_hit(dly + 1, 60, SL_HAT, 1, 24);
        } else {                                         // GREEN bossa clave
            if (step == 0 || step == 6) schedule_hit(dly + 1, 36, SL_KICK, 2, 60);
            if (step == 3 || step == 10 || step == 12) schedule_hit(dly + sw + 1, 74, SL_WOOD, 2, 40);
            if (step % 2 == 0) schedule_hit(dly + 1, 60, SL_HAT, 1, 18);
        }
        vu += 0.5f;
    }
}

// ── THE LEAD — a held mono Moog voice, driven per frame so it can GLIDE ──────
static void drive_lead(double pos) {
    if (!radioOn) { if (leadH >= 0) { note_off(leadH); leadH = -1; leadEvt = -2; } return; }
    if (leadH < 0) {
        leadH = note_on(67, I_LEAD, 0);                  // starts silent
        note_lfo(leadH, 0, LFO_PITCH, 5.5f, 0.18f);      // the singing vibrato
        leadEvt = -2;
    }
    double sp = pos - songBase;
    if (sp < 0) { note_vol(leadH, 0); return; }
    if (sp >= total_steps()) { note_vol(leadH, 0); return; }

    // find the latest melody event at or before now
    int e = -1;
    for (int i = 0; i < sng.evN; i++) {
        if (sng.evStep[i] <= sp) e = i; else break;
    }
    if (e < 0) { note_vol(leadH, 0); return; }

    // are we still inside that note, or in the gap after it?
    double endAt = sng.evStep[e] + sng.evDur[e];
    bool sounding = sp < endAt;
    if (e != leadEvt && sounding) {                      // a new note
        // GLIDE ONLY TO CONNECT CLOSE, LEGATO NOTES — snap on a leap (>4 semis) or
        // when re-entering after a rest (don't swoop across the silence). This is the
        // gentle Moog slur, not the seasick constant-portamento it was.
        int prev = (leadEvt >= 0 && leadEvt < sng.evN) ? sng.evMidi[leadEvt] : sng.evMidi[e];
        int leap = prev - sng.evMidi[e]; if (leap < 0) leap = -leap;
        note_glide(leadH, (leadGap || leap > 4) ? 0 : FEEL[sng.feel].glide);
        note_pitch(leadH, (float)sng.evMidi[e]);
        note_vol(leadH, 5);
        leadEvt = e;
        leadGap = false;
        vu += 1.6f;
    } else if (!sounding) {
        note_vol(leadH, 0);                              // the gap between phrases
        leadGap = true;
    }
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        chLead = rad_chair(&band, "lead",  "saw", "square", "tri", NULL);
        chBass = rad_chair(&band, "bass",  "springy", "round", NULL, NULL);
        chBell = rad_chair(&band, "bells", "on", "off", NULL, NULL);
        setup_instruments();
        if (PLANTASIA_SEED) { new_song(pos, PLANTASIA_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 64, 152, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  { if (!radioOn) { note_off_all(); leadH = -1; leadEvt = -2; }
                              else scheduled = (long)pos; }
    if (ev & RAD_EV_TONE)   apply_tone();

    int chair = rad_band_input(&band, &showHelp);
    if (chair >= 0) apply_chair(chair);

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
        long songStep = scheduled - songBase;
        if (songStep >= total_steps()) fresh_song(pos);
    }
    drive_lead(pos);

    vu *= 0.88f; if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / spb() : 0;
    watch("song", "%d", songCount);
    watch("feel", "%s", FEEL[sng.feel].name);
    watch("mode", "%s", MODENAME[sng.mode]);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("melN", "%d", sng.evN);
    watch("tempo", "%d", tempo);
#endif
}

// ── the growing houseplant ───────────────────────────────────────────────────
static void draw_plant(float p) {        // p = 0..1 song progress
    const Plant *pl = &PLANTS[sng.plant];
    int wx = 34, wy = 52, ww = 102, wh = 116;
    // sky gradient
    for (int y = 0; y < wh; y++) {
        int c = y < wh * 0.45f ? CLR_DARK_BLUE : y < wh * 0.7f ? CLR_INDIGO : CLR_DARKER_PURPLE;
        line(wx, wy + y, wx + ww, wy + y, c);
    }
    int potW = 34, potX = wx + ww / 2 - potW / 2, potY = wy + wh - 22;
    int soilY = potY + 2;
    int baseX = wx + ww / 2;
    int maxH  = 76;
    int stemH = (int)(p * maxH);

    // the stem (a gentle S-wave), growing up from the soil
    int prevx = baseX, prevy = soilY;
    for (int h = 1; h <= stemH; h++) {
        int y = soilY - h;
        int x = baseX + (int)(sinf(h * 0.12f) * 4.0f);
        line(prevx, prevy, x, y, pl->leafDk);
        prevx = x; prevy = y;
    }
    int topx = prevx, topy = prevy;

    // leaves unfurl as the stem passes their anchor height
    for (int i = 0; i < pl->nLeaf; i++) {
        float hh = (float)(i + 1) / (pl->nLeaf + 1);      // anchor up the stem
        if (p < hh * 0.95f) continue;
        int ly = soilY - (int)(hh * maxH);
        int lx = baseX + (int)(sinf(hh * maxH * 0.12f) * 4.0f);
        int side = (i % 2) ? 1 : -1;
        float grown = clamp((p - hh * 0.95f) * 6.0f, 0.0f, 1.0f);
        int len = (int)(grown * 13) + 3;
        int tipx = lx + side * len, tipy = ly - len / 2;
        if (pl->leafShape == 2) {                          // snake plant — upright spikes
            tipx = lx + side * (len / 2); tipy = ly - len - 4;
            line(lx, ly, tipx, tipy, pl->leaf);
            line(lx, ly, tipx + side, tipy, pl->leafDk);
        } else if (pl->leafShape == 1) {                   // pointed
            line(lx, ly, tipx, tipy, pl->leafDk);
            circfill(tipx, tipy, 2 + (int)(grown * 2), pl->leaf);
        } else {                                           // round
            line(lx, ly, tipx, tipy, pl->leafDk);
            circfill(tipx, tipy, 2 + (int)(grown * 3), pl->leaf);
        }
    }

    // the flower opens on the outro (top ~15%)
    if (p > 0.82f) {
        float bloom = clamp((p - 0.82f) / 0.18f, 0.0f, 1.0f);
        int r = (int)(bloom * 5) + 1;
        for (int a = 0; a < 6; a++) {
            float ang = a * 1.0472f;
            int px = topx + (int)(cosf(ang) * (r + 1));
            int py = topy + (int)(sinf(ang) * (r + 1));
            circfill(px, py, r, pl->flower);
        }
        circfill(topx, topy, 2, CLR_LIGHT_YELLOW);
    }

    // the pot + soil + nursery price-tag
    rectfill(potX, soilY, potW, 4, CLR_DARK_BROWN);                 // soil
    rectfill(potX, potY, potW, 18, CLR_DARK_ORANGE);
    rectfill(potX, potY, potW, 4, CLR_ORANGE);
    rect(potX, potY, potW, 18, CLR_BROWNISH_BLACK);
    font(FONT_TINY);
    char tag[16]; snprintf(tag, sizeof tag, "#%04X", sng.seed & 0xFFFF);
    rectfill(potX + potW - 2, potY + 5, 26, 8, CLR_LIGHT_YELLOW);
    print(tag, potX + potW, potY + 6, CLR_DARK_BROWN);
    print(pl->name, wx + 3, wy + 3, CLR_WHITE);
    font(FONT_NORMAL);
    rect(wx, wy, ww, wh, CLR_DARK_GREY);
}

// ── draw — the conservatory chassis ──────────────────────────────────────────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar  = songStep >= 0 ? songStep / spb() : 0;
    int  sect = sect_of(bar);
    float p   = songStep > 0 ? clamp((float)songStep / total_steps(), 0, 1) : 0;

    rad_body(CLR_DARK_GREEN, CLR_LIME_GREEN);
    rad_dial(sng.freq, CLR_LIME_GREEN);

    if (radioOn) draw_plant(p);
    else { rectfill(34, 52, 102, 116, CLR_DARKER_PURPLE);
           print("- radio off -", 56, 108, CLR_DARK_GREY);
           rect(34, 52, 102, 116, CLR_DARK_GREY); }

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_LIME_GREEN);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_LIME_GREEN);
        char l2[36];
        snprintf(l2, sizeof l2, "%.1f FM  %s", sng.freq, FEEL[sng.feel].name);
        print(l2, 154, 70, CLR_MEDIUM_GREEN);
        snprintf(l2, sizeof l2, "%d bpm  #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_MEDIUM_GREEN);
        float vt = vu / 12.0f; rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_LIME_GREEN);
    } else print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        print(SECTNAME[sect], 152, 104, CLR_LIME_GREEN);
        print(MODENAME[sng.mode], 230, 104, CLR_MEDIUM_GREEN);
        rad_phrase_dots(152, 124, 8, bar / 4, CLR_LIME_GREEN);
    }

    static const char *DENS[4] = { "sparse", "tend", "bloom", "full" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, DENS[intensity], CLR_LIME_GREEN);
    if (rad_knob_int(&tempo, 64, 152, 2, 218, 148, 9, "tempo", CLR_LIME_GREEN)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_LIME_GREEN)) apply_tone();
    rad_power_led(radioOn, CLR_LIME_GREEN, CLR_DARK_GREEN);

    rad_help_button(CLR_LIME_GREEN);
    rad_band_button(CLR_LIME_GREEN);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next song (a new plant)" },
            { "R",          "same seed - the tune is fixed" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "density - sparse .. full" },
            { "UP/DOWN",    "tempo of this song" },
            { "T",          "tone - mellow .. bright" },
            { "M",          "radio power on / off" },
            { "B",          "the band - lead/bass/bells" },
        };
        static const char *NOTES[3] = {
            "a seeded HOOK, developed A-A'-B-A + a key lift.",
            "five track-feels roll the character. the Moog",
            "sings the tune. pin: PLANTASIA_SEED 0x...",
        };
        rad_help_panel("PLANTASIA RADIO", HELP, 8, NOTES, 3, CLR_LIME_GREEN);
    }
    rad_band_panel(&band, CLR_LIME_GREEN);
    ui_end();
}
