/* de:meta
{
  "slug": "mariachi",
  "collection": ["radio"],
  "title": "mariachi fm",
  "status": "active",
  "created": "2026-06-10",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing",
    "swing-timing",
    "analog-voice-modeling"
  ],
  "lineage": "Extends the dreamengine radio station pattern with its first INSTR_BOWED violin section and a fully modeled sesquialtera (6/8 vs 3/4 hemiola) groove engine; the functional harmony and call-and-response copla/respuesta form go further than any prior station.",
  "homage": "Mariachi / son jalisciense",
  "description": "Endless mariachi, live from the plaza - a charro ensemble on a Saturday night, and the FIRST RADIO STATION TO REACH INSTR_BOWED: the violin section is the real bowed-string waveguide (every fiddle on the dial before this was faked on saw, like tango's), two desks panned wide for the ensemble spread, with a portamento scoop INTO the note and vibrato. It's also the second station to reach real INSTR_BRASS - two trumpets voiced a third apart, bright and blatty, that ANSWER the violins call-and-response. The rhythm section (armonia) is three registers of one INSTR_GUITAR engine: a bright short-ringing VIHUELA chattering the manico, a mid GUITARRA, and the woody GUITARRON bass on its root-fifth boom. The brains, all cart-side over radio.h's clock: THE SESQUIALTERA - a 12-sixteenth bar read two ways at once, 6/8 (accents on 0,6) against 3/4 (accents on 0,4,8), with the guitarron anchoring one meter while the vihuela accents the other on hemiola bars - two against three, the son jalisciense's signature cross-rhythm; the GROOVE rolls son (the lilting sesquialtera), vals (a clean 3/4 waltz, no cross-rhythm) or huapango (fast, hard hemiola). FUNCTIONAL HARMONY - a major-or-minor key with a I-IV-V walk, a secondary dominant (V/V) and a FORCED V-to-I cadence, one 8-bar progression locked so every copla sings the same changes. THE COPLA + THE RESPUESTA - the violins state a diatonic melodic cell in parallel thirds (the verse), the trumpets answer it a section later (the figure echoed/inverted, also in thirds); they TRADE, never both on the melody at once. THE ADORNO - in the showy section a soloist (violin or trumpet, rolled) rips a scalar flourish, pure performance rnd so it is new every replay. FORM rolls corto / son / fiesta: an entrada fanfare, coplas, trumpet trades, the adorno, and a remate tag that actually ends the tune. Seeded per song: key, major/minor, the progression, groove, form, tempo, the cell and the trumpet figure. The window is the plaza at night - papel picado banners, a charro in the great sombrero, a trumpet bell flashing when the horns play. SPACE next song, R same tune (a new adorno), [ ] history, LEFT/RIGHT feel (suave/tibio/alegre/fiesta), UP/DOWN tempo, T tone, B band (violines seccion/solo/sinte - sinte swaps the bowed strings for smooth saw synth-strings; trompetas dos/una/off; armonia vihuela/nylon), M power, H help."
}
de:meta */
// ── MARIACHI FM ─────────────────────────────────────────────────────────────
// Live from the plaza: the Mariachi station — a charro ensemble on a Saturday
// night. A violin section singing the copla in parallel thirds, two trumpets
// that answer back, a vihuela chattering the manico over the guitarron's
// deep root-fifth boom — and the SESQUIALTERA, the 6/8-against-3/4 lilt that is
// the heartbeat of the son jalisciense.
//
// Designed INTENT-FIRST (radio-station-howto.md): the band was imagined from
// the music BEFORE shopping the palette, which is how it reaches an engine no
// other station does:
//   • FIRST RADIO INSTR_BOWED — the violin SECTION. Every fiddle on the dial so
//     far was faked (tango's saw violins); this is the bowed-string waveguide,
//     two desks panned wide for the ensemble spread, with the mariachi scoop
//     (a portamento slide INTO the note) and vibrato.
//   • INSTR_BRASS trumpets — the second station to reach real brass (afrobeat
//     was first): two horns voiced a third apart, bright and blatty, trading
//     call-and-response with the violins.
//   • INSTR_GUITAR ×3 — the rhythm section: a bright short-ringing VIHUELA
//     (the manico), a mid GUITARRA, and the GUITARRON bass (woody, fretless-
//     gut, root-fifth). One Karplus-Strong-plus-body engine, three registers.
//
// The brains (cart-side generative logic over radio.h's clock):
//   • THE SESQUIALTERA — a 12-sixteenth bar read two ways at once: 6/8 (accents
//     on steps 0,6) vs 3/4 (accents on 0,4,8). The guitarron anchors one meter
//     while the vihuela accents the other on hemiola bars — two against three,
//     the son's signature cross-rhythm. The GROOVE rolls son / vals / huapango.
//   • FUNCTIONAL HARMONY — a major-or-minor key with a I-IV-V walk, a secondary
//     dominant (V/V), and a FORCED V->I cadence; one 8-bar progression locked
//     for the song so the coplas all sing the same changes.
//   • THE COPLA + THE RESPUESTA — the violins state a diatonic melodic cell in
//     thirds (the verse); the trumpets answer it a section later (the figure
//     echoed/inverted, also in thirds). They TRADE — never both on the melody.
//   • THE ADORNO — in the showy section a soloist (violin or trumpet, rolled)
//     rips a scalar flourish; performance rnd, new every replay.
//   • FORM — intro fanfare -> coplas -> trumpet trades -> adorno -> remate tag.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density)   UP/DOWN tempo   T tone   B band   H help

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define MARI_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots (5..11) ───────────────────────────────────────────────
#define I_VLN1   5   // violin 1 — melody lead          (INSTR_BOWED, scoop+vib)
#define I_VLN2   6   // violin 2 — the third below       (INSTR_BOWED)
#define I_TPT1   7   // trumpet 1 — section + stabs       (INSTR_BRASS)
#define I_TPT2   8   // trumpet 2 — the third below       (INSTR_BRASS)
#define I_VIH    9   // vihuela — the manico strum        (INSTR_GUITAR, bright)
#define I_GTR   10   // guitarra de golpe — body strum    (INSTR_GUITAR, mid)
#define I_GTRON 11   // guitarron — root-fifth bass       (INSTR_GUITAR, low)

#define BARSTEPS 12  // a 12-sixteenth bar: read as 6/8 (0,6) OR 3/4 (0,4,8)

// ── scales as data (7-entry ascending) ──────────────────────────────────────
// Major sones, and a harmonic-minor option (the raised 7th matches the V chord
// and gives the augmented-second colour some minor sones carry).
enum { K_MAJOR, K_MINOR, NK };
static const int SCALE7[NK][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },   // major
    { 0, 2, 3, 5, 7, 8, 11 },   // harmonic minor
};
static const char *KNAME[NK] = { "mayor", "menor" };

// scale degree -> midi, walking the 7-note scale up/down from a center
static int deg_to_midi(int deg, int center, const int *scale) {
    int oct = 0;
    while (deg < 0)  { deg += 7; oct--; }
    while (deg >= 7) { deg -= 7; oct++; }
    return center + scale[deg] + oct * 12;
}

// ── harmonic functions — abstract ids, rendered per mode ────────────────────
// 0 tonic · 1 subdominant · 2 dominant · 3 secondary-dominant(V/V or bVII) ·
// 4 the sixth (vi or bVI) · 5 the supertonic (ii or, in minor, a second colour)
enum { FX_I, FX_IV, FX_V, FX_SEC, FX_VI, FX_II, NFUNC };
// semitone root offset from the key, per mode
static const int FOFF[NK][NFUNC] = {
    { 0, 5, 7, 2, 9, 2 },   // major:  I  IV  V   V/V  vi  ii
    { 0, 5, 7, 10, 8, 2 },  // minor:  i  iv  V   bVII bVI ii(o)
};
// is the rendered triad MAJOR? (false = minor triad)
static const bool FMAJ[NK][NFUNC] = {
    { true,  true,  true,  true,  false, false },
    { false, false, true,  true,  true,  false },
};

// weighted transition table — repeats = more likely (reads like a cheat-sheet)
// tonic roams; subdom -> dom/home; dom mostly home (sometimes deceptive to vi);
// V/V resolves to V; vi -> subdom/ii/dom; ii -> V.
static const int TRANS[NFUNC][8] = {
    { FX_IV, FX_V, FX_V, FX_VI, FX_II, FX_SEC, FX_IV, FX_I },  // I
    { FX_V,  FX_V, FX_I, FX_II, FX_V,  FX_I,   FX_V,  FX_IV }, // IV
    { FX_I,  FX_I, FX_I, FX_I,  FX_VI, FX_I,   FX_I,  FX_V },  // V
    { FX_V,  FX_V, FX_V, FX_V,  FX_V,  FX_V,   FX_V,  FX_V },  // V/V -> V
    { FX_IV, FX_II,FX_V, FX_IV, FX_II, FX_V,   FX_IV, FX_VI }, // vi
    { FX_V,  FX_V, FX_V, FX_I,  FX_V,  FX_V,   FX_V,  FX_II }, // ii
};

// ── the groove — the percussion-less feel; the ear locks here first ─────────
enum { G_SON, G_VALS, G_HUAPANGO, NG };
static const char *GNAME[NG] = { "son", "vals", "huapango" };

// ── the form — 8-bar sections; the seed rolls one of three arrangements ─────
enum { S_INTRO, S_COPLA, S_TROMP, S_ADORNO, S_REMATE };
#define MAXSECT 10
static const struct { int n; int s[MAXSECT]; const char *name; } FORMS[] = {
    { 4,  { S_INTRO, S_COPLA, S_TROMP, S_REMATE }, "corto" },
    { 7,  { S_INTRO, S_COPLA, S_TROMP, S_COPLA, S_ADORNO, S_TROMP, S_REMATE }, "son" },
    { 9,  { S_INTRO, S_COPLA, S_TROMP, S_ADORNO, S_COPLA, S_TROMP,
            S_ADORNO, S_COPLA, S_REMATE }, "fiesta" },
};
#define NFORMS 3
static const char *SECTNAME[5] = { "entrada", "copla", "trompetas", "adorno", "remate" };

// ── the generated song ──────────────────────────────────────────────────────
typedef struct {
    int  key, keyPc;            // mode (major/minor) + key pitch class
    int  form, groove;
    int  prog[8];               // the 8-bar functional progression (locked)
    int  hemiKind;              // 0 = hemiola every 4th bar · 1 = every other
    int  soloVln;               // adorno soloist: 1 = violin, 0 = trumpet
    int  melOn[8], melDeg[8], melN;    // the copla cell (2-bar, 24-step grid)
    int  hrnOn[6], hrnDeg[6], hrnN;    // the trumpet figure (2-bar)
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 125.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)

static int   tempo     = 120;
static int   intensity = 1;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 3;        // mariachi wants bright, ringing horns
static int   songCount = 0;
static float vu        = 0;
static char  nowChord[8][12];      // the 8-bar chord readout

// held voice-leading state for the strummed triad
static int  gvVih[3] = { 56, 60, 64 };
static bool vihInit  = false;

// THE BAND (B) — three chairs the player can re-voice live.
static RadBand band;
static int chVln, chTpt, chRhy;

// horns can switch off via the trumpet chair (sel 2)
static bool horns_on(void) { return band.c[chTpt].sel != 2; }

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);
static void apply_chair(int idx);

// ── song generation — composition AND the band, all from the seed ─────────
static const char *TW1[] = { "El", "La", "Mi", "Cielito", "Guadalajara", "Jalisco",
    "Las", "Son", "Camino", "Paloma", "Corazon", "Volver" };
static const char *TW2[] = { "Rey", "Bamba", "Cucaracha", "Lindo", "de Jalisco",
    "Querido", "Mananitas", "Negra", "Real", "Bonita", "Espinado", "Volver" };

static void roll_progression(void) {
    // walk the table, then FORCE the cadence into the last two bars: V -> I.
    sng.prog[0] = FX_I;
    for (int b = 1; b < 8; b++) {
        const int *row = TRANS[sng.prog[b - 1]];
        sng.prog[b] = row[srnd(8)];
    }
    sng.prog[6] = FX_V;   // the dominant
    sng.prog[7] = FX_I;   // home — the cadence always pulls back
    // a secondary dominant likes to sit before the V (V/V -> V)
    if (srnd(100) < 55) sng.prog[5] = FX_SEC;
}

static void roll_cell(int *on, int *deg, int *n, int maxN, int density, int span) {
    // onsets over a 2-bar (24-step) grid, off-beat-leaning; degrees walk a small
    // contour over the scale. span = how far the contour ranges.
    static const int CAND[10] = { 0, 4, 6, 10, 12, 14, 16, 18, 20, 22 };
    *n = 0;
    int d = srnd(3);
    for (int i = 0; i < 10 && *n < maxN; i++)
        if (srnd(100) < density) {
            on[*n]  = CAND[i];
            d += srnd(5) - 2;                 // step the contour
            if (d < 0) d = 0;
            if (d > span) d = span;
            deg[*n] = d;
            (*n)++;
        }
    if (*n < 2) {                              // never empty
        *n = 2;
        on[0] = 0;  deg[0] = 0;
        on[1] = 12; deg[1] = 2;
    }
}

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.key   = srnd(100) < 68 ? K_MAJOR : K_MINOR;   // sones lean major
    sng.keyPc = srnd(12);
    sng.groove = srnd(NG);
    sng.hemiKind = srnd(2);
    sng.soloVln  = srnd(100) < 55;

    roll_progression();
    roll_cell(sng.melOn, sng.melDeg, &sng.melN, 8, 58, 6);   // the copla
    roll_cell(sng.hrnOn, sng.hrnDeg, &sng.hrnN, 6, 50, 5);   // the trumpet figure

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    int fr   = srnd(100);
    sng.form = fr < 32 ? 0 : fr < 74 ? 1 : 2;          // 32% corto · 42% son · 26% fiesta

    // tempo follows the groove — the waltz strolls, the huapango flies
    static const int TLO[NG] = { 116, 92, 138 }, TSPAN[NG] = { 24, 18, 24 };
    tempo = TLO[sng.groove] + srnd(TSPAN[sng.groove]);
    bpm(tempo);

    apply_band_overrides();
    apply_chair(chVln);          // re-aim the violins for THIS song's groove (attack/scoop)
    songBase = (long)pos + BARSTEPS;
    vihInit = false;
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
    return x < n ? FORMS[sng.form].s[x] : S_REMATE;
}

static int func_of(long bar)  { return sng.prog[(int)(bar % 8)]; }
static int root_pc(int f)     { return (sng.keyPc + FOFF[sng.key][f]) % 12; }

static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_REMATE) ? 0 :
               (s == S_TROMP || s == S_ADORNO) ? 2 : 1;
    return rad_level(base, intensity);
}

static void chord_label(char *out, int n, int f) {
    static const char *roman_maj[NFUNC] = { "I", "IV", "V", "II7", "vi", "ii" };
    static const char *roman_min[NFUNC] = { "i", "iv", "V", "bVII", "bVI", "iio" };
    snprintf(out, n, "%s", (sng.key == K_MAJOR ? roman_maj : roman_min)[f]);
}

// is THIS bar a hemiola bar (the strum flips to the opposing meter)?
static bool hemi_bar(long bar) {
    if (sng.groove == G_VALS) return false;            // the waltz never crosses
    if (sng.hemiKind == 0) return (bar % 4) == 3;      // the cadential turnaround
    return (bar % 2) == 1;                             // every other bar (huapango-busy)
}

// ── the step player ─────────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % BARSTEPS);            // 0..11 within the bar
    long bar  = s / BARSTEPS;
    if (bar >= song_bars()) return;
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  cs   = (int)(s % (BARSTEPS * 2));      // position in the 2-bar window
    long bar2 = bar / 2;                         // which 2-bar phrase
    const int *sc = SCALE7[sng.key];
    int  f    = func_of(bar);
    int  rpc  = root_pc(f);
    bool maj  = FMAJ[sng.key][f];
    bool hemi = hemi_bar(bar);
    bool intro = (sect == S_INTRO);
    int  bib   = (int)(bar % 8);                 // bar-in-section (intro build)

    // additive intro: the rhythm section layers in over the entrada
    bool inBass = !intro || bib >= 0;
    bool inRhy  = !intro || bib >= 2;
    bool inMel  = !intro || bib >= 4;

    // ── THE GUITARRON — root-fifth, anchoring the meter ──────────────────────
    if (inBass) {
        int rt = 28 + ((rpc - 28) % 12 + 12) % 12;   // root in the bass register
        int ft = rt + 7;                              // the fifth
        if (sng.groove == G_VALS) {
            // the waltz "oom": root on 1, the fifth lifts on beat 2
            if (step == 0) { schedule_hit(dly, rt, I_GTRON, 5, (int)(stepMs * 3)); vu += 1.4f; }
            if (step == 4) { schedule_hit(dly, ft, I_GTRON, 4, (int)(stepMs * 3)); vu += 0.8f; }
        } else if (hemi) {
            // the 3/4 reading: three even beats (0,4,8) — against the 6/8 strum
            if (step == 0) { schedule_hit(dly, rt, I_GTRON, 5, (int)(stepMs * 3)); vu += 1.4f; }
            if (step == 4) { schedule_hit(dly, ft, I_GTRON, 4, (int)(stepMs * 3)); vu += 0.9f; }
            if (step == 8) { schedule_hit(dly, rt, I_GTRON, 4, (int)(stepMs * 3)); vu += 0.9f; }
        } else {
            // the 6/8 reading: the two dotted-quarter beats (0,6) — the son boom
            if (step == 0) { schedule_hit(dly, rt, I_GTRON, 5, (int)(stepMs * 5)); vu += 1.5f; }
            if (step == 6) { schedule_hit(dly, ft, I_GTRON, 4, (int)(stepMs * 5)); vu += 1.0f; }
        }
    }

    // ── THE VIHUELA MANICO + GUITARRA — the strummed chord, the cross-rhythm ──
    if (inRhy && lvl >= 1) {
        // re-voice the triad on each chord change (bar start)
        if (step == 0) {
            int third = maj ? 4 : 3;
            int iv[3] = { 0, third, 7 };
            rad_lead_to(rpc, iv, gvVih, 3, 55, 67, &vihInit);
        }
        // accent steps: the meter the STRUM implies (opposite of the bass on
        // hemiola bars = sesquialtera). vals = pure 3/4 (beats 2 & 3 strum).
        bool accent, ghost;
        if (sng.groove == G_VALS) {
            accent = (step == 4 || step == 8);          // pah-pah
            ghost  = false;
        } else if (hemi) {
            accent = (step == 0 || step == 6);          // strum reads 6/8...
            ghost  = (step == 2 || step == 4 || step == 8 || step == 10);
        } else {
            accent = (step == 0 || step == 4 || step == 8);  // ...or strum reads 3/4
            ghost  = (step == 2 || step == 6 || step == 10);
        }
        bool busy = (sng.groove == G_HUAPANGO);
        if (accent) {
            // the full chord, strummed — voices a few ms apart (the rasgueo)
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + 3 + k * 4, gvVih[k], I_VIH, 4,
                             (int)(stepMs * 1.4f));
            // the guitarra doubles the accent an octave down for body
            if (lvl >= 2)
                schedule_hit(dly + 5, gvVih[0] - 12, I_GTR, 3, (int)(stepMs * 1.8f));
            vu += 0.9f;
        } else if (ghost && (busy || lvl >= 2)) {
            // the muted up-stroke chuck — a single short top note
            schedule_hit(dly + 2 + rnd(3), gvVih[2], I_VIH, 1, (int)(stepMs * 0.5f));
            vu += 0.3f;
        }
    }

    // ── THE COPLA — the violin section sings the cell, in thirds ─────────────
    // Violins lead in the copla; they REST when the trumpets take over.
    bool vlnLead = inMel && (sect == S_COPLA || sect == S_INTRO);
    if (vlnLead && lvl >= 1) {
        for (int i = 0; i < sng.melN; i++)
            if (sng.melOn[i] == cs) {
                int top = deg_to_midi(sng.melDeg[i], 67 + sng.keyPc, sc);
                int bot = deg_to_midi(sng.melDeg[i] - 2, 67 + sng.keyPc, sc);
                while (top > 88) top -= 12;
                while (bot < 60) bot += 12;
                int gat = (int)(stepMs * (sng.groove == G_VALS ? 4 : 3));
                schedule_hit(dly + 14, top, I_VLN1, 4, gat);          // the lead
                if (lvl >= 2)
                    schedule_hit(dly + 16, bot, I_VLN2, 3, gat);      // the third below
                vu += 1.2f;
            }
    }

    // ── THE RESPUESTA — the trumpets answer, call-and-response, in thirds ────
    // Trumpets own the trompetas section; sparse fanfare leaks into the entrada.
    bool tptLead = horns_on() && ((sect == S_TROMP) ||
                   (sect == S_INTRO && bib < 2));           // the opening fanfare
    if (tptLead && lvl >= 1) {
        bool answer = (sect == S_TROMP) && (bar2 & 1);      // echo/invert on odd phrases
        for (int i = 0; i < sng.hrnN; i++)
            if (sng.hrnOn[i] == cs) {
                int deg = answer ? (5 - sng.hrnDeg[i]) : sng.hrnDeg[i];
                int top = deg_to_midi(deg, 60 + sng.keyPc, sc);
                int bot = deg_to_midi(deg - 2, 60 + sng.keyPc, sc);
                while (top > 79) top -= 12;
                while (top < 58) top += 12;
                while (bot > top) bot -= 12;
                int gat = (int)(stepMs * 3);
                schedule_hit(dly + 10, top, I_TPT1, 4, gat);          // lead trumpet
                if (lvl >= 2)
                    schedule_hit(dly + 12, bot, I_TPT2, 3, gat);      // the harmony
                vu += 1.3f;
            }
    }

    // ── THE ADORNO — the showy flourish, a soloist ripping the scale ─────────
    // Performance rnd (engine rnd()): new every replay, like a real adorno.
    if (sect == S_ADORNO && step % 2 == 0) {
        bool tptSolo = !sng.soloVln;
        // a run on the even-eighth pulse, contour drifting over the scale. The
        // bowed violin gets SPACED (sparser) so the bow can speak between notes
        // instead of chirping; the trumpet can rip dense.
        int dens = sng.soloVln ? (lvl >= 2 ? 58 : 36) : (lvl >= 2 ? 82 : 55);
        if ((!tptSolo || horns_on()) && chance(dens)) {
            static int adeg = 0;
            adeg += rnd(5) - 2;
            if (adeg < 0) adeg = 0;
            if (adeg > 9) adeg = 9;
            int slot = sng.soloVln ? I_VLN1 : I_TPT1;
            int base = sng.soloVln ? 67 + sng.keyPc : 60 + sng.keyPc;
            int hi   = sng.soloVln ? 88 : 79;
            int mm   = deg_to_midi(adeg, base, sc);
            while (mm > hi) mm -= 12;
            schedule_hit(dly + 6 + rnd(4), mm, slot, 4 + (chance(35) ? 1 : 0),
                         (int)(stepMs * 1.6f));
            vu += 1.0f;
        }
    }

    // ── THE REMATE — the closing tag: a unison hit on the final downbeat ─────
    if (sect == S_REMATE && bar == song_bars() - 1 && step == 0) {
        int rt = 28 + ((sng.keyPc - 28) % 12 + 12) % 12;
        schedule_hit(dly, rt, I_GTRON, 6, (int)(stepMs * 6));
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 5, gvVih[k], I_VIH, 5, (int)(stepMs * 6));
        schedule_hit(dly + 4, deg_to_midi(7, 67 + sng.keyPc, sc), I_VLN1, 5, (int)(stepMs * 8));
        schedule_hit(dly + 4, deg_to_midi(4, 60 + sng.keyPc, sc), I_TPT1, 5, (int)(stepMs * 8));
        vu += 3.0f;
    }
}

// ── the band chairs — full recipes; a swap re-aims the slot from scratch ────
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chVln) {
        // The violin section, three ways. sel 0 = BOWED section (two desks,
        // wide) · 1 = BOWED solo (one desk, drier, centred) · 2 = SINTE: smooth
        // SAW synth-strings (no bow-friction scratch — tango's faked-violin
        // recipe), the soft fallback when the real bow is too aggressive.
        if (sel == 2) {
            instrument(I_VLN1, INSTR_SAW, 55, 200, 6, 320);   // gentle swell, sustains
            instrument_lfo(I_VLN1, 0, LFO_PITCH, 5.6f, 0.09f);            // vibrato
            instrument_env(I_VLN1, 0, ENV_PITCH, 0, 1, 0);               // no scoop
            instrument(I_VLN2, INSTR_SAW, 55, 200, 6, 320);
            instrument_lfo(I_VLN2, 0, LFO_PITCH, 5.2f, 0.08f);
            instrument_env(I_VLN2, 0, ENV_PITCH, 0, 1, 0);
            instrument_pan(I_VLN1, -0.35f); instrument_pan(I_VLN2, 0.35f);
        } else {
            // The bow can't bloom inside a fast son gate, and a deep scoop on a
            // staccato note just squeals — so attack + scoop FOLLOW THE GROOVE:
            // the vals sings (slow bow, full portamento), the son/huapango speak
            // quickly with only a hint of scoop. (Re-applied per song in new_song.)
            bool slow = (sng.groove == G_VALS);
            int   atk = slow ? 70 : 24;             // fast passages: the bow speaks now
            int   sdk = slow ? 80 : 38;             // scoop decay
            float sc1 = slow ? -1.0f : -0.25f;      // scoop depth — subtle on staccato
            float sc2 = slow ? -0.9f : -0.22f;
            instrument(I_VLN1, INSTR_BOWED, atk, 0, 7, 340);
            instrument_harmonics(I_VLN1, 0.45f);          // mid bow, the bowed/violin recipe
            instrument_timbre(I_VLN1, 0.30f);             // (engine now maps this to lower pressure)
            instrument_morph(I_VLN1, 0.70f);
            instrument_lfo(I_VLN1, 0, LFO_PITCH, 5.6f, 0.10f);             // vibrato
            instrument_env(I_VLN1, 0, ENV_PITCH, 0, sdk, sc1);            // the scoop INTO the note
            instrument(I_VLN2, INSTR_BOWED, atk, 0, 7, 340);
            instrument_harmonics(I_VLN2, 0.48f);
            instrument_timbre(I_VLN2, 0.32f);
            instrument_morph(I_VLN2, 0.62f);
            instrument_lfo(I_VLN2, 0, LFO_PITCH, 5.2f, 0.09f);
            instrument_env(I_VLN2, 0, ENV_PITCH, 0, sdk, sc2);
            if (sel == 0) { instrument_pan(I_VLN1, -0.40f); instrument_pan(I_VLN2, 0.40f); }
            else          { instrument_pan(I_VLN1, -0.08f); instrument_pan(I_VLN2, 0.08f); }
        }
    } else if (idx == chTpt) {
        // INSTR_BRASS — the trumpets. sel 0 = two trumpets · 1 = one (mellower
        // second drops to a flugel) · 2 = off (hornsOn gates via the section flag).
        instrument(I_TPT1, INSTR_BRASS, 1, 0, 4, 1200);
        instrument_harmonics(I_TPT1, 0.15f);          // tight bright bore — the lead horn
        instrument_timbre(I_TPT1, 0.62f);             // plenty of brassy blat
        instrument_morph(I_TPT1, 0.42f);
        instrument(I_TPT2, INSTR_BRASS, 1, 0, 4, 1200);
        if (sel == 1) {                                // the mellow second (flugel)
            instrument_harmonics(I_TPT2, 0.46f);
            instrument_timbre(I_TPT2, 0.30f);
            instrument_morph(I_TPT2, 0.45f);
        } else {                                       // a matched second trumpet
            instrument_harmonics(I_TPT2, 0.18f);
            instrument_timbre(I_TPT2, 0.56f);
            instrument_morph(I_TPT2, 0.42f);
        }
        instrument_pan(I_TPT1, -0.25f);
        instrument_pan(I_TPT2,  0.25f);
    } else if (idx == chRhy) {
        // INSTR_GUITAR ×3 — the armonia. sel 0 = vihuela+guitarra+guitarron,
        // sel 1 = the guitarra leans nylon (warmer, the bolero feel).
        instrument(I_VIH, INSTR_GUITAR, 1, 0, 7, 700);
        instrument_harmonics(I_VIH, 0.70f);           // bright, high body
        instrument_timbre(I_VIH, 0.80f);
        instrument_morph(I_VIH, 0.55f);               // short ring (the chuck)
        instrument_pan(I_VIH, 0.30f);
        instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 900);
        if (sel == 1) {                                // warm nylon golpe
            instrument_harmonics(I_GTR, 0.45f);
            instrument_timbre(I_GTR, 0.20f);
            instrument_morph(I_GTR, 0.30f);
        } else {                                       // brighter steel-ish body
            instrument_harmonics(I_GTR, 0.55f);
            instrument_timbre(I_GTR, 0.55f);
            instrument_morph(I_GTR, 0.30f);
        }
        instrument_pan(I_GTR, -0.25f);
        // the guitarron — woody, low, quick stop (fretless gut, root-fifth boom)
        instrument(I_GTRON, INSTR_GUITAR, 1, 0, 7, 400);
        instrument_harmonics(I_GTRON, 0.35f);
        instrument_timbre(I_GTRON, 0.18f);
        instrument_morph(I_GTRON, 0.50f);
    }
}

static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}

static void setup_instruments(void) {
    chVln = rad_chair(&band, "violines", "seccion", "solo",  "sinte", NULL);
    chTpt = rad_chair(&band, "trompetas", "dos", "una", "off", NULL);
    chRhy = rad_chair(&band, "armonia",  "vihuela", "nylon", NULL, NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);     // base sounds (sel 0)
}

// (horns_on defined up by the band globals — play_step needs it)

// the tone knob — the whole band brightens/mellows (re-aim, keeping song rolls)
static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_VLN1, FILTER_LOW, (int)(3400 * tm), 1);
    instrument_filter(I_VLN2, FILTER_LOW, (int)(3400 * tm), 1);
    instrument_filter(I_TPT1, FILTER_LOW, (int)(3800 * tm), 2);
    instrument_filter(I_TPT2, FILTER_LOW, (int)(3600 * tm), 2);
    instrument_filter(I_VIH,  FILTER_LOW, (int)(4200 * tm), 1);
    instrument_filter(I_GTR,  FILTER_LOW, (int)(3200 * tm), 1);
    instrument_filter(I_GTRON,FILTER_LOW, (int)(1400 * tm), 1);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (MARI_SEED) { new_song(pos, MARI_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        apply_tone();
        booted = true;
    }

    int ev = rad_input(&tempo, 80, 168, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
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
    if (chair >= 0) {
        apply_chair(chair);
        if (chair == chTpt && !horns_on()) note_off_all();   // hush the held horns
        apply_tone();
    }

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= song_bars() * BARSTEPS) fresh_song(pos);
        for (int b = 0; b < 8; b++) chord_label(nowChord[b], 12, sng.prog[b]);
    }

    vu *= 0.88f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / BARSTEPS : 0;
    watch("song", "%d", songCount);
    watch("key", "%s %s", RAD_PCNAME[sng.keyPc], KNAME[sng.key]);
    watch("groove", "%s", GNAME[sng.groove]);
    watch("form", "%s", FORMS[sng.form].name);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("func", "%d", func_of(tbar));
    watch("hemi", "%d", hemi_bar(tbar) ? 1 : 0);
#endif
}

// ── draw — the chassis; window art = a plaza, a sombrero, a guitarron ───────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar  = songStep >= 0 ? songStep / BARSTEPS : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_DARK_RED, CLR_ORANGE);            // deep charro red, gold trim
    rad_dial(sng.freq, CLR_ORANGE);

    // the window — the plaza at night: papel picado banners, a sombrero, stars
    rectfill(34, 52, 102, 116, CLR_DARKER_BLUE);             // the night sky
    for (int i = 0; i < 14; i++) {                            // stars
        int sx = 38 + (i * 37) % 96, sy = 56 + (i * 53) % 40;
        pset(sx, sy, (i % 3) ? CLR_LIGHT_GREY : CLR_WHITE);
    }
    // papel picado — festive banner triangles strung across the top
    static const int PAPEL[5] = { CLR_GREEN, CLR_WHITE, CLR_RED, CLR_YELLOW, CLR_PINK };
    for (int t = 0; t < 8; t++) {
        int bx = 36 + t * 13;
        int sway = (int)(sinf(timer() * 1.2f + t * 0.6f) * 2);
        int col = PAPEL[t % 5];
        for (int r = 0; r < 7; r++)
            line(bx + r, 60 + r + sway, bx + 12 - r, 60 + r + sway, col);
    }
    // the stage floor
    rectfill(34, 150, 102, 18, CLR_DARK_BROWN);
    // a charro silhouette: the great sombrero + a body
    int hx = 85, hy = 128;
    circfill(hx, hy + 18, 4, CLR_DARK_GREY);                 // the head
    rectfill(hx - 6, hy + 22, 13, 18, CLR_DARK_GREY);        // the body/serape
    rectfill(hx - 6, hy + 30, 13, 3, CLR_ORANGE);            // serape stripe
    ovalfill(hx, hy + 8, 22, 5, CLR_BROWN);                  // the wide brim
    circfill(hx, hy + 2, 8, CLR_BROWN);                      // the crown
    circ(hx, hy + 2, 8, CLR_YELLOW);
    line(hx - 22, hy + 8, hx + 22, hy + 8, CLR_YELLOW);      // brim trim glints
    // a trumpet bell flashing on the right when the horns play
    if (horns_on() && (sect == S_TROMP || sect == S_INTRO)) {
        int flash = (int)(vu) > 4;
        circfill(118, 110, 5, flash ? CLR_YELLOW : CLR_ORANGE);
        for (int k = 0; k < 12; k++) pset(106 + k, 110 - k / 3, CLR_YELLOW);
    }
    // VU-reactive footlights
    int glow = (int)(vu);
    if (glow > 0) rectfill(34, 166, 102, 2, glow > 6 ? CLR_YELLOW : CLR_ORANGE);
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_ORANGE);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_ORANGE);
        char l2[32];
        snprintf(l2, 32, "%s %s  %s", RAD_PCNAME[sng.keyPc], KNAME[sng.key], GNAME[sng.groove]);
        print(l2, 154, 70, CLR_YELLOW);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_YELLOW);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_ORANGE);
    } else
        print("- radio apagada -", 162, 70, CLR_DARK_GREY);

    if (radioOn) {
        // the 8-bar chord row, current bar boxed
        rad_chord_row(nowChord, 8, (int)(bar % 8), 152, 104, CLR_ORANGE);
        print(SECTNAME[sect], 152, 120, CLR_ORANGE);
        rad_phrase_dots(232, 124, form_sects(), bar / 8, CLR_ORANGE);
    }

    static const char *FEEL[4] = { "suave", "tibio", "alegre", "fiesta" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_ORANGE);
    if (rad_knob_int(&tempo, 80, 168, 2, 218, 148, 9, "tempo", CLR_ORANGE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_ORANGE)) apply_tone();
    rad_power_led(radioOn, CLR_ORANGE, CLR_DARK_RED);

    rad_help_button(CLR_ORANGE);
    rad_band_button(CLR_ORANGE);

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tune (rolls a new seed)" },
            { "R",          "same tune - the adorno is new" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - suave/tibio/alegre/fiesta" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "B",          "the band - violins/trumpets/armonia" },
        };
        static const char *NOTES[3] = {
            "the sesquialtera: 6/8 vs 3/4 at once -",
            "guitarron holds one meter, the vihuela",
            "accents the other. violins BOWED, horns BRASS",
        };
        rad_help_panel("MARIACHI FM", HELP, 8, NOTES, 3, CLR_ORANGE);
    }
    rad_band_panel(&band, CLR_ORANGE);
    ui_end();
}
