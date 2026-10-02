/* de:meta
{
  "slug": "citypop2",
  "collection": ["radio"],
  "title": "city pop radio II",
  "status": "active",
  "created": "2026-10-02",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing",
    "step-sequencer"
  ],
  "lineage": "A second city pop station, built beside the original citypop (kept as it was) with what lofi learned matching loficity (docs/design/radio-arranger-lessons.md): the band imagined from the genre first, then cast on the modeled engines (an EPIANO Rhodes or a DX FM piano, a Karplus fingered bass, INSTR_GUITAR cutting, INSTR_BRASS stabs, a chorused string pad, a morphdrum session kit with a plate on the snare); the song PLANNED before it plays (intro / verse / pre-chorus / chorus / sax solo / last chorus up a whole step / outro) and every bar planned knowing the next; a vocal-style HOOK that REPEATS (every chorus sings the same two phrases, every verse the same melody), with a breath bar the horns fill; loficity's voicing (no b9 inside a chord), the lead kept above the keys, passing tones + grace notes; and the sample-clock step grid.",
  "homage": "Tatsuro Yamashita / Mariya Takeuchi / Anri (city pop)",
  "description": "City pop again, done the way lofi FM learned to: the glossy Japanese pop-funk of Tatsuro Yamashita, Mariya Takeuchi's Plastic Love and Anri, as a radio that writes whole SONGS. Every song is planned before it plays: an intro, two verses, a pre-chorus that climbs, a chorus on the Royal Road (IVmaj7-V13-iii7-vi9) or the Just-the-Two-of-Us changes, an alto sax solo, the last chorus up a whole step (of course), an outro. The HOOK repeats like a real song: every chorus sings the same two phrases and every verse the same melody, stated, then a breath bar the horns fill, then answered. The band, imagined from the records first: a Rhodes (or a DX7 electric piano), a fingered electric bass popping octaves and running chromatically into each change, a clean chorused guitar playing 16th cutting, a brass section on the anticipations, a string pad under the chorus, a tight session kit with a plate on the snare plus shaker and congas. Session-tight timing, the glossy rack (plate reverb, chorus on the keys and guitar, a dotted-8th echo on the lead, bus glue). SPACE next song, R replay, [ ] history, LEFT/RIGHT feel (4am..neon density), UP/DOWN tempo, T tone, B band (keys rhodes/dx7, bass finger/dx, lead synth/voice/sax/off, horns on/off), M power, H help; the strip at the bottom is a chord-locked jam strip (the lead lays out while you play). Pin via CITYPOP2_SEED."
}
de:meta */
// ── CITY POP RADIO II ───────────────────────────────────────────────────────
// The original citypop stays as it was (the maker likes it, warts and all). This is the same genre
// rebuilt with what lofi learned matching loficity (docs/design/radio-arranger-lessons.md):
//
//   • THE BAND, imagined from the records first, then shopped (cart-authoring-prompt.md's firewall):
//     Rhodes / DX7 piano, fingered bass, clean chorused guitar cutting 16ths, a brass section, a string
//     pad, a tight session kit with a plate on the snare, shaker + congas. A glossy rack, set-and-hold.
//   • THE SONG, planned before it plays: intro / verse / pre / chorus / verse / pre / chorus / sax solo /
//     the last chorus (up a whole step most of the time) / outro. Every BAR planned knowing the next:
//     comp rhythms + the push onto the next chord, grooves + fills, the bass's octave pops and its
//     chromatic run into each change, horn stabs in the lead's breath bar.
//   • THE HOOK REPEATS: a phrase is seeded by its PLACE in the form (section type + phrase number), not
//     by its bar, so every chorus sings the same two phrases and the second verse sings the first's.
//   • Clean harmony by construction: loficity's voicing (no b9 between two voices), the lead's ladder
//     above the keys, strong beats on chord tones, passing tones + grace notes from the key's scale.
//   • Session-tight time on the sample clock (radio.h rad_audio_*), with a small per-part humanize.
//
//   SPACE next   R replay   [ ] history   LEFT/RIGHT feel   UP/DOWN tempo   T tone   B band   M power   H help

#include "studio.h"
#include "radio.h"
#include "solo.h"
#include "morphdrum.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define CITYPOP2_SEED 0   // pin a favourite song here (0 = free-roaming radio)

// ── slots ───────────────────────────────────────────────────────────────────
#define I_EP    5   // electric piano, short release: the comp
#define I_BASS  6   // fingered electric bass
#define I_LEAD  7   // the vocal hook: synth / voice / sax
#define I_GTR   8   // clean funk guitar: 16th cutting
#define I_BRASS 9   // the horn section (trumpets)
#define I_SAX   10  // alto sax: the solo
#define I_STR   11  // string pad under the chorus
#define I_SHK   12  // shaker
#define I_EPL   13  // electric piano again, long release: held chords
#define I_CONGA 14
#define I_TOM   15  // tom fills
#define I_SOLO  16  // the jam strip
#define KIT_BASE 20 // morphdrum slots 20..29

// ── chords ──────────────────────────────────────────────────────────────────
enum { Q_MAJ7, Q_MAJ9, Q_DOM9, Q_DOM13, Q_ALT, Q_MIN7, Q_MIN9, Q_MIN6, Q_SUS, NQ };
static const char *QN[NQ] = { "maj7", "maj9", "9", "13", "7alt", "m7", "m9", "m6", "sus" };
// four-voice rootless voicings (the bass owns the root)
static const int QV4[NQ][4] = {
    { 4, 7, 11, 12 }, { 4, 7, 11, 14 }, { 4, 7, 10, 14 }, { 4, 10, 14, 21 },
    { 4, 10, 15, 20 },                                    // 3 b7 #9 b13: the JTTOU crunch
    { 3, 7, 10, 12 }, { 3, 7, 10, 14 }, { 3, 7, 9, 14 },
    { 5, 7, 10, 14 },                                     // V9sus4 = IV/V, the city pop dominant
};
typedef struct { int off, q; } Ch;
typedef struct { int off, q, beats; } PCh;
typedef struct { int n, beats; PCh c[8]; } Prog;
// the canon, in the major key: verses, pre-choruses (they climb), choruses (four bars each, looped)
#define NVERSE 4
static const struct { int n; PCh c[6]; } VERSES[NVERSE] = {
    { 4, { { 2, Q_MIN9, 4 }, { 7, Q_DOM13, 4 }, { 0, Q_MAJ9, 4 }, { 9, Q_MIN9, 4 } } },                       // ii V I vi, smooth
    { 4, { { 0, Q_MAJ9, 4 }, { 0, Q_DOM9, 4 }, { 5, Q_MAJ7, 4 }, { 5, Q_MIN6, 4 } } },                        // the descent: I I7 IV iv
    { 5, { { 5, Q_MAJ7, 4 }, { 4, Q_ALT, 4 }, { 9, Q_MIN9, 4 }, { 7, Q_MIN7, 2 }, { 0, Q_DOM9, 2 } } },     // Just the Two of Us
    { 3, { { 2, Q_MIN7, 4 }, { 4, Q_ALT, 4 }, { 9, Q_MIN9, 8 } } },                                           // ii III7 vi: the Plastic Love minor
};
#define NPRE 3
static const struct { int n; PCh c[6]; } PRES[NPRE] = {
    { 4, { { 2, Q_MIN9, 4 }, { 4, Q_MIN7, 4 }, { 5, Q_MAJ7, 4 }, { 7, Q_SUS, 4 } } },                         // ii iii IV V: the climb
    { 4, { { 5, Q_MAJ9, 4 }, { 4, Q_MIN7, 4 }, { 2, Q_MIN9, 4 }, { 7, Q_DOM13, 4 } } },                       // IV iii ii V
    { 6, { { 5, Q_MAJ9, 4 }, { 7, Q_SUS, 4 }, { 4, Q_MIN7, 2 }, { 9, Q_MIN7, 2 }, { 2, Q_MIN9, 2 }, { 7, Q_SUS, 2 } } },
};
#define NCHORUS 3
static const struct { int n; PCh c[6]; int jttou; } CHORUSES[NCHORUS] = {
    { 4, { { 5, Q_MAJ9, 4 }, { 7, Q_DOM13, 4 }, { 4, Q_MIN7, 4 }, { 9, Q_MIN9, 4 } }, 0 },                    // the Royal Road
    { 5, { { 5, Q_MAJ9, 4 }, { 7, Q_DOM13, 4 }, { 4, Q_MIN7, 4 }, { 9, Q_MIN7, 2 }, { 7, Q_SUS, 2 } }, 0 },   // Royal Road, turned back to IV
    { 5, { { 5, Q_MAJ7, 4 }, { 4, Q_ALT, 4 }, { 9, Q_MIN9, 4 }, { 7, Q_MIN7, 2 }, { 0, Q_DOM9, 2 } }, 1 },   // JTTOU
};

// ── sections + roles ────────────────────────────────────────────────────────
enum { SC_INTRO, SC_VERSE, SC_PRE, SC_CHORUS, SC_SOLO, SC_OUTRO, NSC };
static const char *SC_NAME[NSC] = { "intro", "verse", "pre", "chorus", "solo", "outro" };
enum { FL_NONE, FL_ROLL, FL_TOMS, FL_STOP, FL_PICKUP };
static const char *FL_NAME[5] = { "", "roll", "toms", "stop", "pickup" };
typedef struct { int name, bars, n; } Sect;      // n = which verse / chorus this is (0, 1, 2)

// ── the kit's grooves (straight 16ths, session-tight). x hit · g ghost ──
typedef struct { const char *k, *s; } Groove;
#define NGROOVE 4
static const Groove GROOVES[NGROOVE] = {
    { "x...x...x...x...", "....x.......x..." },     // disco four
    { "x.....x...x.....", "....x..g.g..x..g" },     // funk, ghost notes
    { "x.......x.x.....", "....x.......x..." },     // the push
    { "x..x....x.x..x..", "....x.......x..." },     // the skip
};
// the bass's figure per song: x root · o octave pop · f fifth · g dead-note ghost
#define NBPAT 3
static const char *BPAT[NBPAT] = {
    "x.....o.x.o...o.",   // octave disco
    "x..x..o.x..x..o.",   // syncopated
    "x.g...o.x.g...o.",   // ghosted
};

// the keys' COMP RHYTHMS: step, length, vol, top (= only the top two voices, a lighter stab)
typedef struct { int s, len, v, top; } Cell;
typedef struct { int n; Cell c[5]; } Comp;
enum { CP_HOLD, CP_CHARL, CP_ANT, CP_FUNK, CP_PULSE, NCP };
static const Comp COMPS[NCP] = {
    { 1, { { 0, 16, 4, 0 } } },                                                    // hold the bar
    { 2, { { 0, 3, 4, 0 }, { 6, 10, 3, 0 } } },                                    // charleston
    { 3, { { 0, 6, 4, 0 }, { 6, 2, 3, 1 }, { 10, 6, 3, 0 } } },                    // 16th anticipations
    { 5, { { 0, 2, 4, 0 }, { 3, 2, 2, 1 }, { 6, 2, 3, 0 }, { 10, 2, 2, 1 }, { 12, 4, 3, 0 } } },   // the funk stabs
    { 2, { { 0, 8, 4, 0 }, { 8, 8, 3, 0 } } },                                     // two halves
};
static const int COMPW[NSC][NCP] = {   // each section's comping taste
    { 6, 1, 1, 0, 2 },   // intro: held
    { 2, 3, 3, 1, 2 },   // verse
    { 1, 2, 3, 2, 3 },   // pre: busier
    { 1, 2, 3, 3, 2 },   // chorus
    { 1, 2, 2, 4, 1 },   // solo: funk under the sax
    { 5, 1, 1, 0, 2 },   // outro
};
static const int PUSHODDS[NSC] = { 0, 25, 45, 45, 35, 0 };
// the guitar's cutting: 1 = muted chuck, 2 = open stab (nothing on 13-15: the horns + the bass run own the turn)
#define NCHUCK 3
static const int CHUCK[NCHUCK][16] = {
    { 0,0,1,0, 0,0,2,0, 0,0,1,0, 0,0,0,0 },
    { 0,0,1,1, 0,0,2,0, 0,0,1,0, 1,0,0,0 },
    { 0,0,2,0, 0,1,1,0, 0,0,2,0, 0,0,0,0 },
};

// ── the song ────────────────────────────────────────────────────────────────
typedef struct {
    int keyPc;
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
#define srnd(n)   rad_srnd(&rs, (n))

static int   tempo     = 110;
static int   intensity = 1;          // feel: 4am / cruise / downtown / neon
static int   toneSel   = 2;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   songCount = 0;
static float vu = 0;
static int   gvEP[4] = { 60, 64, 67, 71 }; static bool epInit = false;
static int   bassLast = 38;
static char  nowChord[4][8]; static int nowN = 1, nowIdx = 0;
static RadBand band;
static int chKeys, chBass, chLead, chHorns;

static void apply_fx(void);
static void apply_tone(void);
static void plan_song(unsigned seed);
static void plan_reset(void);

static const char *TW1[] = { "Plastic", "Midnight", "Magic", "Summer", "Crystal",
    "Tokyo", "Sparkle", "Velvet", "Downtown", "Neon", "Pacific", "Windy" };
static const char *TW2[] = { "Love", "Drive", "Wave", "Lady", "Night", "City",
    "Motion", "Breeze", "Heart", "Line", "Story", "Lights" };

static void new_song(double pos, unsigned seed) {
    sng.seed  = rad_seed_begin(&rs, seed);
    sng.keyPc = srnd(12);
    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;
    tempo = 104 + srnd(15);                       // 104..118
    bpm(tempo);
    songBase = (long)pos + 4;
    plan_song(sng.seed); plan_reset(); apply_fx();
    epInit = false; bassLast = 38;
    songCount++;
}
static void fresh_song(double pos) { new_song(pos, 0); rad_hist_log(&rs); }

// ── THE ARRANGEMENT ─────────────────────────────────────────────────────────
// Planned per SONG (plan_song): the form, the three progressions, the groove, the bass figure, the guitar's
// cutting, and per bar the push / fill / comp rhythm. Played per BAR (plan_bar): every note of the bar,
// knowing the next. All of it on DERIVED streams (arr_seed), so a pinned seed keeps its key and title.
#define MAXSECT 12
#define MAXBAR  96
static struct {
    Sect s[MAXSECT]; int n, bars;
    Prog V, P, C;
    int groove, bpat, chuck, truck, modBar, compW[NSC][NCP];
    unsigned char sec[MAXBAR], j[MAXBAR], push[MAXBAR], fill[MAXBAR], comp[MAXBAR];
} arr;
static unsigned arr_rng = 1;
static void arr_seed(unsigned seed, unsigned k) {
    unsigned h = seed * 2654435761u ^ (k * 0x9E3779B9u); h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    arr_rng = h ? h : 1;
}
static int arnd(int n) { arr_rng ^= arr_rng << 13; arr_rng ^= arr_rng >> 17; arr_rng ^= arr_rng << 5; return (int)(arr_rng % (unsigned)n); }
static int wpick(const int *w, int n) { int t = 0; for (int i = 0; i < n; i++) t += w[i]; int r = arnd(t > 0 ? t : 1); for (int i = 0; i < n; i++) { if (r < w[i]) return i; r -= w[i]; } return 0; }
static void prog_load(Prog *p, const PCh *c, int n) { p->n = n; p->beats = 0; for (int i = 0; i < n; i++) { p->c[i] = c[i]; p->beats += c[i].beats; } }

static const Prog *sect_prog(int si) {
    int nm = arr.s[si].name;
    return nm == SC_VERSE ? &arr.V : nm == SC_PRE ? &arr.P : &arr.C;   // intro, chorus, solo, outro ride the chorus
}
static int key_at(long bar) { return (sng.keyPc + (arr.truck && bar >= arr.modBar ? 2 : 0)) % 12; }
static int root_pc(Ch c, long bar) { return (key_at(bar) + c.off) % 12; }
static void chord_label(char *out, int n, Ch c, long bar) { snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(c, bar)], QN[c.q]); }

// the chords of one bar (a chord can change mid-bar): start step, length in steps
typedef struct { int s, len, off, q; } BCh;
static int bar_chords(long bar, BCh *out) {
    if (bar < 0) bar = 0; if (bar >= arr.bars) bar = arr.bars - 1;
    const Prog *p = sect_prog(arr.sec[bar]);
    int b0 = (arr.j[bar] * 4) % p->beats, n = 0, at = 0;
    for (int i = 0; i < p->n; i++) {
        int c0 = at, c1 = at + p->c[i].beats; at = c1;
        int lo = c0 > b0 ? c0 : b0, hi = c1 < b0 + 4 ? c1 : b0 + 4;
        if (lo < hi && n < 6) { BCh x = { (lo - b0) * 4, (hi - lo) * 4, p->c[i].off, p->c[i].q }; out[n++] = x; }
    }
    if (!n) { BCh x = { 0, 16, p->c[0].off, p->c[0].q }; out[n++] = x; }
    return n;
}

static void plan_song(unsigned seed) {
    arr_seed(seed, 1);
    int v = arnd(NVERSE), pr = arnd(NPRE), c;
    do c = arnd(NCHORUS); while (v == 2 && CHORUSES[c].jttou);      // never JTTOU twice
    prog_load(&arr.V, VERSES[v].c, VERSES[v].n);
    prog_load(&arr.P, PRES[pr].c, PRES[pr].n);
    prog_load(&arr.C, CHORUSES[c].c, CHORUSES[c].n);
    arr.groove = arnd(NGROOVE); arr.bpat = arnd(NBPAT); arr.chuck = arnd(NCHUCK);
    arr.truck = arnd(100) < 70;                                       // the truck driver's gear change
    for (int s = 0; s < NSC; s++) for (int i = 0; i < NCP; i++) arr.compW[s][i] = COMPW[s][i] + (COMPW[s][i] ? arnd(2) : 0);
    // the form: intro, V P C, V P C, solo, the last chorus, outro (a short second verse half the time)
    static const int F[10] = { SC_INTRO, SC_VERSE, SC_PRE, SC_CHORUS, SC_VERSE, SC_PRE, SC_CHORUS, SC_SOLO, SC_CHORUS, SC_OUTRO };
    int v2 = arnd(100) < 50 ? 4 : 8, nv = 0, nc = 0;
    arr.n = 0; arr.bars = 0;
    for (int i = 0; i < 10; i++) {
        Sect S = { F[i], 8, 0 };
        if (F[i] == SC_INTRO || F[i] == SC_PRE || F[i] == SC_OUTRO) S.bars = 4;
        if (F[i] == SC_VERSE) { S.n = nv++; if (S.n == 1) S.bars = v2; }
        if (F[i] == SC_CHORUS) S.n = nc++;
        if (F[i] == SC_CHORUS && S.n == 2) arr.modBar = arr.bars;
        arr.s[arr.n++] = S;
        for (int j = 0; j < S.bars && arr.bars < MAXBAR; j++) { arr.sec[arr.bars] = (unsigned char)i; arr.j[arr.bars] = (unsigned char)j; arr.bars++; }
    }
    // per bar, knowing the NEXT bar: the comp rhythm, the push, a fill
    for (int b = 0; b < arr.bars; b++) {
        const Sect *S = &arr.s[arr.sec[b]]; int j = arr.j[b], last = j == S->bars - 1;
        arr.comp[b] = (unsigned char)wpick(arr.compW[S->name], NCP);
        arr.push[b] = 0; arr.fill[b] = FL_NONE;
        if (b + 1 < arr.bars) {
            BCh a[6], n2[6]; int na = bar_chords(b, a); bar_chords(b + 1, n2);
            int change = a[na - 1].off != n2[0].off || a[na - 1].q != n2[0].q;
            if (change && arnd(100) < PUSHODDS[S->name]) arr.push[b] = 1;
            if (last && arr.s[arr.sec[b + 1]].name == SC_CHORUS) arr.push[b] = 1;   // every chorus is pushed into
            if (S->name != SC_INTRO && S->name != SC_OUTRO) {
                int odds = last ? 75 : (j + 1) % 4 == 0 ? 18 : 0;
                if (odds && arnd(100) < odds) {
                    int u = arnd(100);
                    arr.fill[b] = last && arr.s[arr.sec[b + 1]].name == SC_CHORUS ? (u < 55 ? FL_TOMS : FL_ROLL)
                                : u < 40 ? FL_ROLL : u < 60 ? FL_TOMS : u < 75 ? FL_STOP : FL_PICKUP;
                }
            } else if (S->name == SC_INTRO && last) arr.fill[b] = FL_ROLL;
        }
    }
}

// ── the bar planner: every note of one bar, as events the step clock books ──
enum { LN_KICK, LN_SNR, LN_HAT, LN_EP, LN_BASS, LN_LEAD, LN_GTR, LN_HORN, LN_STR, LN_PERC, NLANE };
typedef struct { signed char step, lane; short off, midi, instr, vol; int dur; } BEv;
static struct { BEv e[160]; int n; long bar; } bev = { .bar = -1 };
static void ev(int step, int lane, int off, int midi, int instr, int vol, int durMs) {
    if (bev.n >= 160 || step < 0 || step > 15) return;
    if (vol > 7) vol = 7; if (vol < 1) vol = 1;
    BEv x = { (signed char)step, (signed char)lane, (short)off, (short)midi, (short)instr, (short)vol, durMs < 20 ? 20 : durMs };
    bev.e[bev.n++] = x;
}

// THE VOICING: loficity's lc_voice (as lofi.c uses it). Every close rotation of the four rootless notes and its
// drop-2, bottom note in lo..lo+12, top ≤ hi, cheapest by voice motion; any b9 OR semitone between two voices refused.
// THE BAND RULE: every other harmony part (guitar, horns, strings) plays a SUBSET of these exact pitches, never an
// octave-shifted copy — an octave shift turns the chord's maj7 into a semitone and any semitone into a b9. The first
// build did shift them and measured 171 rubs a minute among the harmony parts (the original citypop: 14).
static int voiced[4];
static void voice_to(int rootpc, const int *iv, int lo, int hi, int *prev, bool *init) {
    int pcs[4], n = 0;
    for (int k = 0; k < 4; k++) { int p = (rootpc + iv[k]) % 12, dup = 0; for (int j = 0; j < n; j++) if (pcs[j] == p) dup = 1; if (!dup) pcs[n++] = p; }
    for (int a = 1; a < n; a++) { int v = pcs[a], b = a - 1; while (b >= 0 && pcs[b] > v) { pcs[b + 1] = pcs[b]; b--; } pcs[b + 1] = v; }
    int best[4] = { 0 }; double bc = 1e9; int any = 0;
    for (int k = 0; k < n; k++) for (int d2 = 0; d2 < 2; d2++) {
        int sh[4];
        for (int j = 0; j < n; j++) { int v = pcs[(k + j) % n]; while (j && v <= sh[j - 1]) v += 12; sh[j] = v; }
        if (d2) { if (n < 3) continue; sh[n - 2] -= 12; for (int a = 1; a < n; a++) { int v = sh[a], b = a - 1; while (b >= 0 && sh[b] > v) { sh[b + 1] = sh[b]; b--; } sh[b + 1] = v; } }
        for (int o = 2; o <= 7; o++) {
            int v[4]; for (int j = 0; j < n; j++) v[j] = sh[j] + 12 * o;
            if (v[0] < lo || v[0] > lo + 12 || v[n - 1] > hi) continue;
            int bad = 0;
            for (int a = 0; a < n && !bad; a++) for (int b = a + 1; b < n; b++) { int d = v[b] - v[a]; if (d == 1 || d == 13 || d == 25) bad = 1; }
            for (int a = 1; a < n && !bad; a++) if (v[a] - v[a - 1] < 3 && v[a - 1] < 52) bad = 1;   // no low clusters
            if (bad) continue;
            double mean = 0; for (int j = 0; j < n; j++) mean += v[j]; mean /= n;
            double c = 0.5 * fabs(mean - (lo + 14));
            if (*init) { for (int j = 0; j < n; j++) c += abs(v[j] - prev[j]); int t = abs(v[n - 1] - prev[3]); if (t > 4) c += 2 * (t - 4); }
            if (!any || c < bc) { bc = c; any = 1; for (int j = 0; j < n; j++) best[j] = v[j]; }
        }
    }
    if (any) { for (int j = 0; j < 4; j++) prev[j] = best[j < n ? j : n - 1]; *init = true; }
    for (int j = 0; j < 4; j++) voiced[j] = prev[j];
}
static void voice_keys_for(BCh c, long bar) { voice_to(root_pc((Ch){ c.off, c.q }, bar), QV4[c.q], 48, 76, gvEP, &epInit); }

// ── the lead's world: the key's major pentatonic (yonanuki: city pop's own scale), above the keys ──
static int curKey = 0;
static int ladder[24], nLadder = 0, ladderKey = -1;
static void build_ladder(int key) {
    static const int p[5] = { 0, 2, 4, 7, 9 }; nLadder = 0; ladderKey = key;
    for (int m = 67; m <= 88 && nLadder < 24; m++) for (int k = 0; k < 5; k++) if ((m - key + 120) % 12 == p[k]) ladder[nLadder++] = m;
}
static int ladder_near(int midi) { int bi = 0; for (int i = 1; i < nLadder; i++) if (abs(ladder[i] - midi) < abs(ladder[bi] - midi)) bi = i; return bi; }
static int is_chord_tone(int midi, BCh c, long bar) {
    int pc = (midi % 12 + 12) % 12, r = root_pc((Ch){ c.off, c.q }, bar);
    if (pc == r || (c.q != Q_ALT && pc == (r + 7) % 12)) return 1;
    for (int k = 0; k < 4; k++) if ((r + QV4[c.q][k]) % 12 == pc) return 1;
    return 0;
}
static int is_avoid(int midi, BCh c, long bar) { return !is_chord_tone(midi, c, bar) && is_chord_tone(midi - 1, c, bar); }
static BCh chord_at_step(long bar, int s) { BCh c[6]; int n = bar_chords(bar, c); for (int i = n - 1; i >= 0; i--) if (s >= c[i].s) return c[i]; return c[0]; }
// the chord the BAND is actually holding at a step: on a pushed bar the keys play the NEXT chord from the and-of-4
static BCh chord_eff(long bar, int s) {
    if (s >= 14 && bar >= 0 && bar + 1 < arr.bars && arr.push[bar]) { BCh c[6]; bar_chords(bar + 1, c); return c[0]; }
    return chord_at_step(bar, s);
}
static long bar_eff(long bar, int s) { return s >= 14 && bar >= 0 && bar + 1 < arr.bars && arr.push[bar] ? bar + 1 : bar; }
static int snap_chord(int li, BCh c, long bar) {
    for (int d = 0; d < 4; d++) {
        if (li - d >= 0 && is_chord_tone(ladder[li - d], c, bar)) return li - d;
        if (li + d < nLadder && is_chord_tone(ladder[li + d], c, bar)) return li + d;
    }
    return li;
}
static int in_key(int midi) { static const int MAJ[7] = { 0, 2, 4, 5, 7, 9, 11 }; int pc = (midi - curKey + 120) % 12; for (int i = 0; i < 7; i++) if (MAJ[i] == pc) return 1; return 0; }
static int key_step(int m, int dir) { for (int k = 1; k <= 2; k++) if (in_key(m + dir * k)) return m + dir * k; return m + dir; }

// THE MOTIF'S RHYTHM: groups of two to four notes a 16th or an 8th apart, then a breath (lofi's cells,
// plus a few pop ones: the pickup run, the long held note at the top)
static const struct { int n; unsigned char s[8], g[8]; } MOTIFS[] = {
    { 5, { 0, 2, 4, 10, 12 },        { 0, 0, 0, 1, 1 } },
    { 5, { 0, 3, 6, 8, 16 },         { 0, 0, 0, 0, 1 } },
    { 6, { 2, 4, 6, 12, 14, 20 },    { 0, 0, 0, 1, 1, 2 } },
    { 6, { 0, 2, 3, 6, 14, 16 },     { 0, 0, 0, 0, 1, 1 } },
    { 5, { 1, 4, 6, 8, 18 },         { 0, 0, 0, 0, 1 } },
    { 6, { 0, 4, 6, 8, 10, 22 },     { 0, 1, 1, 1, 1, 2 } },
    { 6, { 2, 3, 6, 8, 10, 14 },     { 0, 0, 0, 0, 0, 1 } },   // a pickup run into a held note
    { 5, { 0, 6, 8, 10, 24 },        { 0, 1, 1, 1, 2 } },      // a stab, a run, the long top note
};
#define NMOTIF ((int)(sizeof MOTIFS / sizeof *MOTIFS))
static const int ANS[5][5] = { { 0, 2, 4, 10, -1 }, { 2, 4, 6, 12, -1 }, { 0, 2, 6, 8, 14 }, { 4, 6, 8, 12, -1 }, { 0, 3, 6, 8, -1 } };
// per section type: a motif, the cell (the anchor degrees of its groups), where the melody sits
typedef struct { int m, n; unsigned char s[8], g[8]; signed char dir[8]; int deg[4], anchor; } Mot;
static Mot mot[NSC];
static void plan_motif(int type, unsigned k, int anchor) {
    arr_seed(sng.seed, k);
    Mot *M = &mot[type]; int m = arnd(NMOTIF);
    M->m = m; M->n = MOTIFS[m].n; M->anchor = anchor;
    for (int i = 0; i < M->n; i++) { M->s[i] = MOTIFS[m].s[i]; M->g[i] = MOTIFS[m].g[i]; M->dir[i] = 0; }
    for (int i = 1, run = 0, d = arnd(100) < 55 ? 1 : -1; i < M->n; i++) {
        if (M->g[i] != M->g[i - 1]) { run = 0; d = arnd(100) < 55 ? 1 : -1; continue; }
        run++; int u = arnd(100);
        M->dir[i] = (signed char)(run >= 3 || u < 18 ? -d : u < 36 ? 0 : d);
    }
    M->deg[0] = 0; for (int g = 1; g < 4; g++) M->deg[g] = M->deg[g - 1] + arnd(5) - 2;   // the cell: small moves
}
static struct { int n; unsigned char s[24], d[24], v[24], m[24], gm[24], gms[24]; long gbar; } phr = { .gbar = -1 };
static void phr_add(int s, int li, int v) { if (phr.n < 24) { phr.s[phr.n] = (unsigned char)s; phr.m[phr.n] = (unsigned char)ladder[li]; phr.v[phr.n] = (unsigned char)v; phr.d[phr.n] = 2; phr.gm[phr.n] = 0; phr.n++; } }

// PASSING TONES + GRACE NOTES (lofi §6.2 / §6.4, loficity's humanize moves), from the key's major scale
static void add_passing(long gbar, unsigned k, int odds) {
    arr_seed(sng.seed, k);
    for (int i = 0; i + 1 < phr.n; i++) {
        int a = phr.m[i], b = phr.m[i + 1], iv = b - a, gap = phr.s[i + 1] - phr.s[i];
        if (abs(iv) < 3 || abs(iv) > 4 || gap < 2 || gap > 8) continue;
        if (arnd(100) >= odds) continue;
        int dir = iv > 0 ? 1 : -1, p = a + dir; if (!in_key(p)) p += dir;
        if (p == b || !in_key(p)) continue;
        int s = phr.s[i] + gap / 2;
        if (s % 8 == 0 || is_avoid(p, chord_eff(gbar + s / 16, s % 16), bar_eff(gbar + s / 16, s % 16))) continue;
        if (phr.n >= 24) break;
        for (int q = phr.n; q > i + 1; q--) { phr.s[q] = phr.s[q - 1]; phr.m[q] = phr.m[q - 1]; phr.v[q] = phr.v[q - 1]; phr.d[q] = phr.d[q - 1]; }
        phr.s[i + 1] = (unsigned char)s; phr.m[i + 1] = (unsigned char)p; phr.v[i + 1] = (unsigned char)(phr.v[i] > 2 ? phr.v[i] - 1 : 2);
        phr.n++; i++;
    }
}
static void add_graces(unsigned k, int odds) {
    arr_seed(sng.seed, k);
    for (int i = 0; i < phr.n; i++) {
        phr.gm[i] = 0;
        double dur = phr.d[i] * stepMs * 0.9;
        if (dur < 300 || arnd(100) >= odds) continue;
        int g = (int)(dur * 0.2); if (g < 40) g = 40; if (g > 70) g = 70;
        if (i && (phr.s[i - 1] + phr.d[i - 1] * 0.9) * stepMs > phr.s[i] * stepMs - g) continue;
        phr.gm[i] = (unsigned char)key_step(phr.m[i], arnd(100) < 62 ? 1 : -1); phr.gms[i] = (unsigned char)g;
    }
}

// LONG NOTES MUST NOT RUB. The builder snaps strong beats to chord tones and refuses a note a semitone ABOVE one,
// but a long note a semitone BELOW a chord tone (a leading tone left hanging), or a note held into a chord where it
// clashes, slipped through: 153 of the 205 lead rubs measured were notes longer than a 16th. So, after the
// durations are known: a note of an 8th or longer that sits a semitone off the chord moves to the nearest chord tone on
// the ladder, and a note held across a change into a chord it rubs against is cut at the change.
// against the pitches the band HOLDS (the voicing), not the chord's pitch classes: the root on top of a maj7 voicing
// (C over the keys' B), the b3 over a held 9, the b7 over a 13 are all "chord tones" and all a semitone off
static int held_pc(int pc, BCh c, long bar) { int r = root_pc((Ch){ c.off, c.q }, bar); for (int k = 0; k < 4; k++) if ((r + QV4[c.q][k]) % 12 == pc) return 1; return 0; }
static int rubs(int m, BCh c, long bar) {
    int pc = (m % 12 + 12) % 12;
    return !held_pc(pc, c, bar) && (held_pc((pc + 1) % 12, c, bar) || held_pc((pc + 11) % 12, c, bar));
}
static void clean_long(long gbar) {
    for (int q = 0; q < phr.n; q++) {
        int s = phr.s[q], len = phr.d[q];
        BCh c = chord_eff(gbar + s / 16, s % 16); long cb = bar_eff(gbar + s / 16, s % 16);
        if (len >= 2 && rubs(phr.m[q], c, cb)) {
            int best = -1;
            for (int i = 0; i < nLadder; i++) if (is_chord_tone(ladder[i], c, cb) && !rubs(ladder[i], c, cb) && (best < 0 || abs(ladder[i] - phr.m[q]) < abs(ladder[best] - phr.m[q]))) best = i;
            if (best >= 0 && abs(ladder[best] - phr.m[q]) <= 4) phr.m[q] = (unsigned char)ladder[best];
        }
        for (int t = s + 1; t < s + len && t < 64; t++) {
            BCh ct = chord_eff(gbar + t / 16, t % 16);
            if ((ct.off != c.off || ct.q != c.q) && rubs(phr.m[q], ct, bar_eff(gbar + t / 16, t % 16))) { phr.d[q] = (unsigned char)(t - s); break; }
        }
    }
}

// ONE PHRASE per four bars, seeded by its PLACE: (section type, phrase number) — so every chorus sings the same
// two phrases and every verse the same melody. Stated over two bars, a breath bar (the horns fill it), answered
// in the fourth. The SOLO is the exception: the sax improvises, seeded per bar, with no breath.
static void build_phrase(long gbar) {
    const Sect *S = &arr.s[arr.sec[gbar]];
    int type = S->name, idx = arr.j[gbar] / 4, solo = type == SC_SOLO;
    phr.gbar = gbar; phr.n = 0;
    if (ladderKey != curKey) build_ladder(curKey);
    unsigned k = solo ? 5000 + (unsigned)gbar : 6000 + (unsigned)type * 64 + (unsigned)(idx % 2);
    arr_seed(sng.seed, k);
    if (type == SC_VERSE && idx % 2 == 1 && arnd(100) < 25) return;            // a verse may breathe a whole phrase
    const Mot *M = &mot[type];
    int li = ladder_near(M->anchor + (type == SC_PRE ? idx * 3 : 0)), x = li;
    int shift = solo ? arnd(3) * 2 : 0;
    for (int i = 0; i < M->n; i++) {
        int s = M->s[i] + shift; if (s >= 30) continue;
        int g = M->g[i], first = i == 0 || M->g[i - 1] != g;
        if (first) x = li + M->deg[g % 4] + (solo ? arnd(5) - 2 : 0) + (idx % 2 && g ? 1 : 0);
        else x += M->dir[i];
        if (x < 0) x = 1; if (x >= nLadder) x = nLadder - 2;
        BCh c = chord_eff(gbar + s / 16, s % 16); long cb = bar_eff(gbar + s / 16, s % 16);
        if (s % 8 == 0 || is_avoid(ladder[x], c, cb)) x = snap_chord(x, c, cb);
        phr_add(s, x, first ? 4 : 3);
    }
    int at = x;
    const int *A = ANS[arnd(5)]; int na = 0; while (na < 5 && A[na] >= 0) na++;
    int ans0 = solo ? 32 : 48;
    for (int q = 0; q < na; q++) {
        int s = ans0 + A[q];
        { int st = arnd(100) < 75 ? 1 : 2; at += arnd(100) < 60 ? -st : st; }
        if (q == na - 1 && idx % 2 == 1 && !solo) at = ladder_near(M->anchor - 2);      // the second phrase comes home
        if (at < 0) at = 1; if (at >= nLadder) at = nLadder - 2;
        BCh c = chord_eff(gbar + s / 16, s % 16); long cb = bar_eff(gbar + s / 16, s % 16);
        if (s % 8 == 0 || q == na - 1 || is_avoid(ladder[at], c, cb)) at = snap_chord(at, c, cb);
        phr_add(s, at, q == na - 1 ? 3 : 2);
    }
    add_passing(gbar, k + 100000, solo ? 70 : 55);
    for (int q = 0; q < phr.n; q++) {
        int nx = q + 1 < phr.n ? phr.s[q + 1] : 64, g = nx - phr.s[q];
        phr.d[q] = (unsigned char)(q + 1 < phr.n ? (g > 6 ? 6 : g) : (g > 10 ? 10 : g));
    }
    clean_long(gbar);
    add_graces(k + 200000, solo ? 35 : 45);
}

static void plan_reset(void) {
    plan_motif(SC_VERSE, 9, 71); plan_motif(SC_PRE, 10, 72); plan_motif(SC_CHORUS, 11, 77);
    plan_motif(SC_SOLO, 12, 74); mot[SC_OUTRO] = mot[SC_CHORUS];
    phr.gbar = -1; bev.bar = -1; ladderKey = -1;
}

static int bass_peek(int pc, int lo, int hi) {
    int dd = ((pc - bassLast) % 12 + 18) % 12 - 6, m = bassLast + dd;
    while (m < lo) m += 12; while (m > hi) m -= 12; return m;
}

static void plan_bar(long bar) {
    bev.n = 0; bev.bar = bar;
    const Sect *S = &arr.s[arr.sec[bar]];
    int j = arr.j[bar], name = S->name, lastBar = j == S->bars - 1, fill = arr.fill[bar];
    curKey = key_at(bar);
    BCh ch[6]; int nch = bar_chords(bar, ch);
    BCh nx[6]; int nnx = bar + 1 < arr.bars ? bar_chords(bar + 1, nx) : 0;
    arr_seed(sng.seed, 1000 + (unsigned)bar);
    int lvl = (name == SC_INTRO || name == SC_OUTRO ? 0 : name == SC_VERSE ? 1 : 2) + intensity - 1;   // the feel shifts the density
    if (lvl < 0) lvl = 0; if (lvl > 3) lvl = 3;
    int outroEnd = name == SC_OUTRO && j == S->bars - 1;
    int stop = fill == FL_STOP ? 12 : 16;

    // ── DRUMS ──
    int kicks[16], nk = 0;
    int drums = !(name == SC_INTRO && j < 2) && !outroEnd;
    int hatsOn = !(name == SC_OUTRO && j >= 2);
    if (drums || hatsOn) {
        const Groove *g = &GROOVES[arr.groove];
        for (int s = 0; s < stop; s++) {
            if (drums && g->k[s] == 'x') { ev(s, LN_KICK, 0, 0, 0, s == 0 ? 6 : 5, 120); kicks[nk++] = s; }
            if (drums) {
                if ((fill == FL_ROLL) && s >= 12) { static const int V[4] = { 3, 4, 4, 6 }; ev(s, LN_SNR, 0, 0, 0, V[s - 12], 90); }
                else if (fill == FL_TOMS && s >= 8) { static const int TN[8] = { 50, 50, 47, 47, 43, 43, 40, 40 }; if (s % 2 == 0 || s >= 12) ev(s, LN_PERC, 0, TN[s - 8], I_TOM, 5, 200); }
                else if (g->s[s] == 'x') ev(s, LN_SNR, 0, 0, 0, 6, 120);
                else if (g->s[s] == 'g' && lvl >= 1) ev(s, LN_SNR, 0, 0, 0, 2, 60);
            }
            if (hatsOn && !(fill == FL_TOMS && s >= 8)) {
                int sixteen = lvl >= 3 || (lvl >= 2 && name != SC_VERSE);
                int open = s == 14 && (name == SC_CHORUS || name == SC_SOLO) && j % 2 == 1;
                if (open) ev(s, LN_HAT, 0, 1, 0, 4, 200);
                else if (s % 2 == 0 || sixteen) ev(s, LN_HAT, 0, 0, 0, s % 4 == 2 ? 4 : s % 2 == 0 ? 3 : 2, 30);
            }
            // the percussion: a 16th shaker in the chorus and the solo, congas from the second verse
            if (drums && lvl >= 2 && (name == SC_CHORUS || name == SC_SOLO)) ev(s, LN_PERC, 0, 60, I_SHK, s % 4 == 0 ? 3 : s % 2 == 0 ? 2 : 1, 40);
            if (drums && (S->n >= 1 || name == SC_SOLO) && name != SC_PRE && (s == 3 || s == 6 || s == 11 || s == 14))
                ev(s, LN_PERC, 0, s == 6 || s == 14 ? 57 : 62, I_CONGA, 3, 200);
        }
        if (fill == FL_PICKUP) { ev(13, LN_KICK, 0, 0, 0, 4, 90); ev(15, LN_KICK, 0, 0, 0, 5, 90); }
    }
    if (outroEnd) ev(0, LN_KICK, 0, 0, 0, 6, 200);

    // ── KEYS — a comp rhythm clipped to the chords, held chords in the intro/outro, the PUSH ──
    typedef struct { int s, len, v, top; BCh c; } KHit;
    KHit kh[12]; int nh = 0, pushedInto = bar > 0 && arr.push[bar - 1];
    #define KH(s_, len_, v_, top_, c_) do { if (nh < 12) { KHit h_ = { s_, len_, v_, top_, c_ }; kh[nh++] = h_; } } while (0)
    const Comp *cp = &COMPS[arr.comp[bar]];
    for (int k = 0; k < nch; k++) {
        int c0 = ch[k].s, c1 = c0 + ch[k].len, own = 0;
        for (int q = 0; q < cp->n; q++) {
            int s = cp->c[q].s; if (s < c0 || s >= c1 || s >= stop || (pushedInto && s == 0)) continue;
            own++; KH(s, cp->c[q].len < c1 - s ? cp->c[q].len : c1 - s, cp->c[q].v, cp->c[q].top, ch[k]);
        }
        if (!own && !(pushedInto && c0 == 0) && c0 < stop) KH(c0, c1 - c0, 4, 0, ch[k]);
    }
    if (arr.push[bar] && nnx && stop == 16) {
        int k2 = 0;
        for (int k = 0; k < nh; k++) if (kh[k].s < 14) { if (kh[k].s + kh[k].len > 14) kh[k].len = 14 - kh[k].s; kh[k2++] = kh[k]; }
        nh = k2; KH(14, 2 + (nx[0].len < 8 ? nx[0].len : 8), 5, 0, nx[0]);
    }
    #undef KH
    int strum = 4;
    for (int k = 0; k < nh; k++) {
        KHit *h = &kh[k]; long cb = h->s >= 14 && arr.push[bar] ? bar + 1 : bar;
        voice_keys_for(h->c, cb);
        int dur = (int)(h->len * stepMs) - 25; if (dur < 90) dur = 90;
        int slot = h->len >= 12 ? I_EPL : I_EP;
        for (int v = h->top ? 2 : 0; v < 4; v++) ev(h->s, LN_EP, (v - (h->top ? 2 : 0)) * strum, voiced[v], slot, h->v + 1 + (v == 3), dur);
    }

    // ── STRINGS — the pad under the chorus + the pre's swell: the keys' own voicing, held per chord ──
    if (name == SC_CHORUS || (name == SC_PRE && j >= 2) || name == SC_INTRO) {
        for (int k = 0; k < nch; k++) {
            if (ch[k].s >= stop) continue;
            voice_keys_for(ch[k], bar);
            for (int v = 1; v < 4; v++) ev(ch[k].s, LN_STR, 0, voiced[v], I_STR, 3, (int)(ch[k].len * stepMs) - 40);
        }
    }

    // ── GUITAR — 16th cutting, from the verse at cruise and up; the chord's top voices, the keys' exact pitches ──
    if (lvl >= 1 && name != SC_INTRO && name != SC_OUTRO) {
        for (int s = 0; s < stop; s++) {
            int c = CHUCK[arr.chuck][s]; if (!c) continue;
            BCh bc = chord_at_step(bar, s); voice_keys_for(bc, bar);
            if (c == 1) { ev(s, LN_GTR, 0, voiced[2], I_GTR, 5, 50); ev(s, LN_GTR, 0, voiced[3], I_GTR, 5, 50); }
            else { ev(s, LN_GTR, 0, voiced[1], I_GTR, 6, 150); ev(s, LN_GTR, 6, voiced[2], I_GTR, 6, 150); ev(s, LN_GTR, 12, voiced[3], I_GTR, 7, 150); }
        }
    }

    // ── BASS — the song's figure: root, octave pops, the fifth, dead notes; the chromatic run into each change ──
    if (!(name == SC_INTRO && j < 2) && !outroEnd) {
        const char *bp = BPAT[arr.bpat];
        int nb = nnx ? bass_peek(root_pc((Ch){ nx[0].off, nx[0].q }, bar + 1), 33, 45) : -1;
        BCh last = ch[nch - 1];
        int run = nnx && (nx[0].off != last.off) && stop == 16 && name != SC_OUTRO;
        int pos[24], np = 0;
        for (int s = 0; s < stop; s++) {
            int isChange = 0; for (int q = 0; q < nch; q++) if (ch[q].s == s) isChange = 1;
            if (bp[s] != '.' || isChange) pos[np++] = s;
        }
        for (int i = 0; i < np; i++) {
            int s = pos[i]; if (run && s >= 14) continue;
            BCh c = chord_at_step(bar, s);
            int root = bass_peek(root_pc((Ch){ c.off, c.q }, bar), 33, 45);
            int isChange = 0; for (int q = 0; q < nch; q++) if (ch[q].s == s) isChange = 1;
            char t = isChange ? 'x' : bp[s];
            int m = t == 'o' ? root + 12 : t == 'f' ? root + 7 : root;
            int nxs = i + 1 < np ? pos[i + 1] : stop; if (run && nxs > 14) nxs = 14;
            int dur = t == 'o' ? 90 : t == 'g' ? 40 : (int)((nxs - s) * stepMs * 0.8);
            ev(s, LN_BASS, 0, m, I_BASS, t == 'g' ? 2 : t == 'o' ? 5 : s == 0 ? 6 : 5, dur);
            if (t == 'x') bassLast = root;
        }
        if (run) {                                    // two 16ths walking chromatically into the next root
            int dir = nb > bassLast ? 1 : -1;
            ev(14, LN_BASS, 0, nb - 2 * dir, I_BASS, 4, 80);
            ev(15, LN_BASS, 0, nb - dir, I_BASS, 4, 80);
        }
    }

    // ── THE HORNS — the stab into every chorus, and a figure in the lead's BREATH bar (bar 3 of 4) ──
    if (band.c[chHorns].sel == 0 && !outroEnd) {
        int breath = (name == SC_CHORUS || name == SC_SOLO) && j % 4 == 2;
        if (arr.push[bar] && nnx && (name == SC_PRE || (lastBar && name != SC_INTRO) || name == SC_CHORUS) && stop == 16) {
            voice_keys_for(nx[0], bar + 1);
            for (int v = 1; v < 4; v++) ev(14, LN_HORN, 0, voiced[v], I_BRASS, 5, 160);
        }
        if (breath && lvl >= 2) {
            static const int HF[3][3] = { { 2, 6, 10 }, { 3, 6, 8 }, { 0, 3, 6 } };
            const int *f = HF[(arr.groove + (int)bar / 4) % 3];
            for (int q = 0; q < 3; q++) {
                BCh c = chord_at_step(bar, f[q]); voice_keys_for(c, bar);
                for (int v = 1; v < 4; v++) ev(f[q], LN_HORN, 0, voiced[v], I_BRASS, q == 2 ? 5 : 4, q == 2 ? 220 : 110);
            }
        }
        if (outroEnd) {}
    }
    if (outroEnd) {                                 // the final hit: the band lands on the tonic chord
        BCh c = { 0, 16, 0, Q_MAJ9 }; voice_keys_for(c, bar);
        for (int v = 0; v < 4; v++) ev(0, LN_EP, v * 4, voiced[v], I_EPL, 5, 2200);
        ev(0, LN_BASS, 0, bass_peek(root_pc((Ch){ 0, Q_MAJ9 }, bar), 33, 45), I_BASS, 6, 1500);
        if (band.c[chHorns].sel == 0) for (int v = 1; v < 4; v++) ev(0, LN_HORN, 0, voiced[v], I_BRASS, 5, 600);
    }

    // ── THE LEAD — the hook (or the sax's solo); lays out while the player jams ──
    int lead = name == SC_VERSE || name == SC_PRE || name == SC_CHORUS || name == SC_SOLO || (name == SC_OUTRO && j < 2 && 0);
    if (lead && !solo_open()) {
        long gbar = bar - j % 4;
        if (phr.gbar != gbar) build_phrase(gbar);
        int bj = j % 4, slot = name == SC_SOLO ? I_SAX : I_LEAD;
        if (name == SC_SOLO || band.c[chLead].sel != 3) {
            for (int k = 0; k < phr.n; k++) if (phr.s[k] / 16 == bj) {
                int s = phr.s[k] % 16; if (s >= stop) continue;
                int vol = phr.v[k] + 2;
                // grace notes only on the synth: the reed and the voice take 30+ ms to speak, so a 40-70 ms flick is mush
                if (phr.gm[k] && slot == I_LEAD && band.c[chLead].sel == 0) ev(s, LN_LEAD, -phr.gms[k], phr.gm[k], slot, vol - 2, (int)(phr.gms[k] * 0.9));
                ev(s, LN_LEAD, 0, phr.m[k], slot, vol, (int)(phr.d[k] * stepMs * 0.92));
            }
        }
    }
}

// ── the kit (morphdrum, an 80s session voicing) ──
static MorphKit kit;
static void fire_kick(double t, int v) {
    MDRes r; md__resolve(&kit, MD_KICK, &r); int b = kit.base;
    schedule_at(t, r.midi, b + MDS_KICK, v, r.dec);
    if (r.l1_vol) schedule_at(t, 60, b + MDS_KICKC, (r.l1_vol * v + 6) / 7, r.l1_dec);
    if (r.l2_vol) schedule_at(t, r.midi - 12, b + MDS_KICKS, (r.l2_vol * v + 6) / 7, r.l2_dec);
}
static void fire_snare(double t, int v) {
    MDRes r; md__resolve(&kit, MD_SNARE, &r); int b = kit.base;
    int body = (int)lround((1 - r.tone) * v * 1.5); if (body > 7) body = 7;
    if (body) { schedule_at(t, r.midi, b + MDS_SNB, body, r.dec); schedule_at(t, r.midi + 10, b + MDS_SNB, body, r.dec); }
    schedule_at(t, 60, b + MDS_SNN, v ? v : 1, r.l1_dec);
}
static void fire_hat(double t, int v, int open) {
    MDRes r; md__resolve(&kit, MD_HAT, &r);
    schedule_at(t, r.midi, kit.base + (open ? MDS_HO : MDS_HC), v, (open ? r.l2_dec : r.dec) * 6);
}

// ── the step player: books the planned bar on the sample clock. Session-tight: a small per-part humanize ──
static double hum(double sigma) { return ((rnd(1001) + rnd(1001) + rnd(1001)) / 1000.0 - 1.5) * 2 * sigma; }
static void play_step(long abs, double pos) {
    (void)pos;
    long s = abs - songBase;
    if (s < 0) return;
    int step = (int)(s % 16); long bar = s / 16;
    if (bar >= arr.bars) return;
    if (bev.bar != bar) plan_bar(bar);
    static const double SIG[NLANE] = { 1.0, 1.5, 1.5, 2.5, 1.5, 4.0, 2.0, 3.0, 2.0, 2.5 };
    static const float VU[NLANE] = { 1.0f, 0.9f, 0.1f, 0.3f, 0.8f, 0.7f, 0.2f, 0.6f, 0.1f, 0.2f };
    for (int i = 0; i < bev.n; i++) {
        const BEv *e = &bev.e[i];
        if (e->step != step) continue;
        double t = rad_step_time(&clk, abs) + (e->off + hum(SIG[e->lane])) * 0.001;
        if (e->lane == LN_KICK) fire_kick(t, e->vol);
        else if (e->lane == LN_SNR) fire_snare(t, e->vol);
        else if (e->lane == LN_HAT) fire_hat(t, e->vol, e->midi == 1);
        else schedule_at(t, e->midi, e->instr, e->vol, e->dur);
        vu += VU[e->lane];
    }
}

// ── the glossy rack — SET-AND-HOLD (per song) ──
static void apply_fx(void) {
    reverb(0.62f, 0.35f); reverb_plate(1.0f); reverb_plate_width(0.7f);   // the plate: dense, bright, wide
    echo((int)(60000.0 / tempo * 0.75), 0.28f, 0.55f);                   // a dotted 8th, on the lead's send
    chorus(1.5f, 0.4f, 0.0f);                                            // master chorus OFF: it lives per part
    glue(0, 0.3f, 10, 150);
    eq(4.5f, 3.5f, 5.5f);                                                // +3.5 dB makeup (rms was -25 vs lofi -21.7) + a little air on top
}

static void inst(int s, int wave, int a, int d, int sus, int r, float h, float t, float m) {
    instrument(s, wave, a, d, sus, r); instrument_harmonics(s, h); instrument_timbre(s, t); instrument_morph(s, m);
}
static int epLp = 4200, leadLp = 3400;
static void voice_keys(int sel) {
    for (int s = I_EP; s <= I_EPL; s += I_EPL - I_EP) {
        if (sel == 0) { inst(s, INSTR_EPIANO, 1, 0, 7, s == I_EP ? 260 : 700, 0.15f, 0.62f, 0.40f); epLp = 4200; }    // a stage Rhodes with bark (held release 700: 2200 rang into the next chord)
        else          { inst(s, INSTR_FM, 2, 700, 3, s == I_EP ? 300 : 600, 0.15f, 0.45f, 0.10f); epLp = 5200; }       // the DX7 electric piano
        instrument_lfo(s, 0, LFO_VOLUME, 4.6f, 0.04f);
        instrument_chorus(s, 0.8f, 0.45f, 0.45f);
        instrument_reverb(s, 0.22f); instrument_level(s, 0.55f); instrument_pan(s, -0.12f);
    }
}
static void voice_bass(int sel) {
    for (int m = 0; m < 7; m++) instrument_mode(I_BASS, m, 0);
    if (sel == 0) {                                // fingered electric
        // yacht's fingered recipe (TRI + a little pitch snap). A Karplus PLUCK read a 1.3 kHz centroid even behind
        // LP 650 (736 Hz): the string model loses its fundamental at bass pitches. This one reads 179 Hz.
        inst(I_BASS, INSTR_TRI, 2, 220, 4, 90, 0.5f, 0.5f, 0.5f); instrument_env(I_BASS, 0, ENV_PITCH, 0, 14, 3);
        instrument_filter(I_BASS, FILTER_LOW, 1000, 0);
    } else {                                       // the DX bass: punchy FM with a little feedback growl
        instrument_env(I_BASS, 0, ENV_PITCH, 0, 14, 0);
        inst(I_BASS, INSTR_FM, 1, 245, 5, 121, 0.0f, 0.70f, 0.25f);
        instrument_filter(I_BASS, FILTER_LOW, 1800, 0);
    }
    instrument_level(I_BASS, 1.0f);
}
static void voice_lead(int sel) {
    float leadLvl = 0.53f;                         // the synth sat +4.9 dB over the keys at 0.75
    instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.2f, 0.0f); instrument_glide(I_LEAD, 0);
    if (sel == 0) {                                // the glossy synth (the original station's chair, voiced)
        inst(I_LEAD, INSTR_SQUARE, 6, 160, 6, 180, 0.5f, 0.5f, 0.5f);
        instrument_duty(I_LEAD, 0.50f); instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.2f, 0.08f); instrument_glide(I_LEAD, 18);   // 0.5: a 0.3 pulse carries DC (-0.014 on the mix)
        leadLp = 3400;
    } else if (sel == 1) {                         // a sung "ah" (the formant voice)
        inst(I_LEAD, INSTR_VOICE, 30, 60, 7, 200, 0.62f, 0.45f, 0.55f);
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.4f, 0.10f); instrument_glide(I_LEAD, 25);
        leadLp = 4200; leadLvl = 0.21f;                  // the voice peaked 10 dB over the synth at the same level
    } else if (sel == 2) {                         // the alto sax on the tune
        inst(I_LEAD, INSTR_REED, 2, 0, 5, 160, 0.78f, 0.45f, 0.55f);
        leadLp = 3800; leadLvl = 0.45f;
    }
    instrument_chorus(I_LEAD, 0.6f, 0.25f, 0.25f);
    instrument_echo(I_LEAD, 0.22f); instrument_reverb(I_LEAD, 0.30f); instrument_pan(I_LEAD, 0.06f); instrument_level(I_LEAD, leadLvl);
}
static void voice_band(void) {
    // guitar: a clean single-coil, mostly muted, chorused, panned right
    inst(I_GTR, INSTR_GUITAR, 1, 0, 7, 90, 0.45f, 0.78f, 0.50f);
    instrument_filter(I_GTR, FILTER_HIGH, 220, 0);
    instrument_chorus(I_GTR, 1.1f, 0.5f, 0.55f); instrument_reverb(I_GTR, 0.15f); instrument_pan(I_GTR, 0.38f); instrument_level(I_GTR, 1.0f); instrument_eq(I_GTR, 0.0f, 6.0f, 2.0f);   // it sat 22 dB under the keys at 0.55
    // the horns: a trumpet section, wide
    inst(I_BRASS, INSTR_BRASS, 1, 0, 5, 90, 0.15f, 0.62f, 0.40f);
    instrument_env(I_BRASS, 0, ENV_PITCH, 0, 60, -1.5f);                 // the fall off the end of a stab
    instrument_reverb(I_BRASS, 0.25f); instrument_pan(I_BRASS, -0.25f); instrument_level(I_BRASS, 0.5f);
    // the sax: the solo
    inst(I_SAX, INSTR_REED, 2, 0, 5, 180, 0.78f, 0.45f, 0.58f);
    instrument_lfo(I_SAX, 0, LFO_PITCH, 5.0f, 0.05f);
    instrument_echo(I_SAX, 0.18f); instrument_reverb(I_SAX, 0.30f); instrument_pan(I_SAX, 0.1f); instrument_level(I_SAX, 0.75f);
    // the strings: a soft saw section, slow in, chorused wide, low in the mix
    instrument(I_STR, INSTR_SAW, 260, 0, 6, 260);     // a short release: a 700 ms tail rang the old chord into the new one
    instrument_filter(I_STR, FILTER_LOW, 2600, 0);
    instrument_chorus(I_STR, 0.5f, 0.7f, 0.7f); instrument_reverb(I_STR, 0.45f); instrument_level(I_STR, 0.22f);
    // percussion
    instrument(I_SHK, INSTR_NOISE, 1, 45, 0, 25); instrument_filter(I_SHK, FILTER_HIGH, 5600, 3);
    instrument_level(I_SHK, 0.35f); instrument_pan(I_SHK, 0.45f);
    inst(I_CONGA, INSTR_MEMBRANE, 1, 0, 7, 200, 0.55f, 0.35f, 0.15f);
    instrument_level(I_CONGA, 0.45f); instrument_pan(I_CONGA, -0.4f); instrument_reverb(I_CONGA, 0.15f);
    inst(I_TOM, INSTR_MEMBRANE, 1, 0, 7, 260, 0.40f, 0.30f, 0.20f);
    instrument_level(I_TOM, 0.7f); instrument_reverb(I_TOM, 0.3f);
    // the jam strip's voice
    instrument(I_SOLO, INSTR_SQUARE, 6, 170, 6, 220); instrument_duty(I_SOLO, 0.32f);
    instrument_lfo(I_SOLO, 0, LFO_PITCH, 5.6f, 0.16f); instrument_filter(I_SOLO, FILTER_LOW, 4200, 3);
}
static void voice_kit(void) {
    float *k = kit.p[MD_KICK];                          // a tight, punchy session kick
    k[MD_CHAR] = 0.55f; k[MD_LEVEL] = 1; k[MD_TUNE] = 0.30f; k[MD_PUNCH] = 0.45f; k[MD_SNAP] = 0.55f;
    k[MD_DECAY] = 0.22f; k[MD_CUT] = 0.45f; k[MD_CLICK] = 0.40f; k[MD_SUB] = 0.25f; k[MD_DRIVE] = 0.12f;
    float *n = kit.p[MD_SNARE];                         // a crisp snare, the plate does the rest
    n[MD_CHAR] = 0.6f; n[MD_LEVEL] = 1; n[MD_TUNE] = 0.42f; n[MD_DECAY] = 0.40f; n[MD_PUNCH] = 0.35f;
    n[MD_SNAP] = 0.85f; n[MD_TONE] = 0.58f; n[MD_CUT] = 0.62f; n[MD_DRIVE] = 0.08f; n[MD_ODEC] = 0.45f;
    float *h = kit.p[MD_HAT];                           // lofi's measured hat (a CHAR 0.5 hat read -9 dBFS rms on its own: a wall)
    h[MD_CHAR] = 0.2f; h[MD_LEVEL] = 1; h[MD_TUNE] = 0.53f; h[MD_TONE] = 0.25f; h[MD_SUB] = 0.6f; h[MD_RES] = 0.0f;
    h[MD_CUT] = 0.636f; h[MD_DECAY] = 0.143f; h[MD_ODEC] = 0.556f;
    morph_ride(&kit);
    for (int s = MDS_KICK; s <= MDS_KICKS; s++) instrument_level(KIT_BASE + s, 0.6f);
    instrument_level(KIT_BASE + MDS_SNB, 0.75f); instrument_level(KIT_BASE + MDS_SNN, 0.5f);
    instrument_reverb(KIT_BASE + MDS_SNB, 0.40f); instrument_reverb(KIT_BASE + MDS_SNN, 0.45f);   // the big 80s plate snare
    for (int s = MDS_HC; s <= MDS_HO; s++) { instrument_level(KIT_BASE + s, 0.45f); instrument_pan(KIT_BASE + s, 0.22f); instrument_reverb(KIT_BASE + s, 0.06f); }
}
// THE MASTER TONE is a low-pass, and it is not optional: morphdrum's hat (INSTR_METAL, six raw squares) puts a
// loud component right at Nyquist — its solo stem read -9 dBFS rms at an 18 kHz centroid, a third of the whole
// mix's level, inaudible but feeding the bus glue. lofi never showed it because its ridden tone filter sits at
// 5.5-9 kHz. 15 kHz takes the hat stem from -3.6 to -19.4 dBFS peak (centroid 6.7 kHz) and keeps the gloss;
// capped at 16 kHz on "bright", since above ~20 kHz it starts letting the whine back through.
static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    float lp = 15000 * tm; if (lp > 16000) lp = 16000;
    filter(FILTER_LOW, lp, 0.05f);
    for (int s = I_EP; s <= I_EPL; s += I_EPL - I_EP) instrument_filter(s, FILTER_LOW, (int)(epLp * tm), 0);
    instrument_filter(I_LEAD, FILTER_LOW, (int)(leadLp * tm), 0);
    instrument_filter(I_STR, FILTER_LOW, (int)(2600 * tm), 0);
}
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chKeys) voice_keys(sel);
    else if (idx == chBass) voice_bass(sel);
    else if (idx == chLead && sel < 3) voice_lead(sel);
    apply_tone();
}
static void setup_instruments(void) {
    chKeys  = rad_chair(&band, "keys", "rhodes", "dx7", NULL, NULL);
    chBass  = rad_chair(&band, "bass", "finger", "dx", NULL, NULL);
    chLead  = rad_chair(&band, "lead", "synth", "voice", "sax", "off");
    chHorns = rad_chair(&band, "horns", "on", "off", NULL, NULL);
    morph_build(&kit, KIT_BASE);
    voice_kit(); voice_band(); voice_keys(0); voice_bass(0); voice_lead(0);
    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);
    apply_tone();
}

// ── update ──────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = rad_audio_pos(&clk, tempo);
    if (!booted) {
        setup_instruments();
        if (CITYPOP2_SEED) { new_song(pos, CITYPOP2_SEED); rad_hist_log(&rs); } else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }
    int evf = rad_input(&tempo, 96, 124, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (evf & RAD_EV_NEW)    fresh_song(pos);
    if (evf & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (evf & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (evf & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (evf & RAD_EV_TONE)   apply_tone();
    if (evf & RAD_EV_POWER)  { if (!radioOn) note_off_all(); else { scheduled = (long)pos; apply_fx(); } }
    int chair = rad_band_input(&band, &showHelp);
    if (chair >= 0) apply_chair(chair);

    if (radioOn) {
        long st; while (rad_audio_step(&clk, &st)) play_step(st, pos);
        if (scheduled - songBase >= (long)arr.bars * 16 + 12) fresh_song(pos);
        long sb = scheduled - songBase; long tb = sb >= 0 ? sb / 16 : 0; if (tb >= arr.bars) tb = arr.bars - 1;
        const Prog *p = sect_prog(arr.sec[tb]);
        int b0 = (arr.j[tb] * 4) % p->beats, cur = 0;
        for (int i = 0, at = 0; i < p->n; at += p->c[i].beats, i++) if (b0 >= at) cur = i;
        nowN = p->n < 4 ? p->n : 4; nowIdx = cur < 3 ? cur : 3;
        for (int i = 0; i < nowN; i++) chord_label(nowChord[i], 8, (Ch){ p->c[i].off, p->c[i].q }, tb);
        if (cur >= 4) chord_label(nowChord[3], 8, (Ch){ p->c[cur].off, p->c[cur].q }, tb);
    }
    vu *= 0.86f; if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase; long tbar = ss >= 0 ? ss / 16 : 0; if (tbar >= arr.bars) tbar = arr.bars - 1;
    static const char *LEADN[4] = { "synth", "voice", "sax", "off" };
    watch("song", "%d", songCount);
    watch("key", "%s", RAD_PCNAME[key_at(tbar)]);
    watch("chord", "%s", nowChord[nowIdx]);
    watch("section", "%s", SC_NAME[arr.s[arr.sec[tbar]].name]);
    watch("fill", "%s", FL_NAME[arr.fill[tbar]]);
    watch("tempo", "%d", tempo);
    watch("lead", "%s", LEADN[band.c[chLead].sel]);
    watch("geared", "%d", arr.truck && tbar >= arr.modBar ? 1 : 0);
#endif
}

// ── draw — the original station's white 80s plastic and its vaporwave window, plus the form strip ──
void draw(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    long bb = bar < arr.bars ? bar : arr.bars - 1;

    rad_body(CLR_LIGHT_GREY, CLR_PINK);
    rectfill(24, 20, 272, 160, CLR_WHITE);
    line(24, 22, 295, 22, CLR_PINK);
    line(24, 24, 295, 24, CLR_BLUE);

    rectfill(32, 28, 218, 16, CLR_BLACK);
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * RAD_DIAL_SP;
        line(x, 38, x, 42, CLR_DARK_GREY);
        if (fq % 4 == 0) { char tx[8]; snprintf(tx, 8, "%d", fq); print(tx, x - 6, 30, CLR_PINK); }
    }
    int nxp = 36 + (int)((rad_needle_freq(sng.freq) - 88.0f) * RAD_DIAL_SP);
    line(nxp, 29, nxp, 43, CLR_BLUE);
    rad_tuner_button(CLR_PINK);

    // the window: vaporwave dusk, a banded sun, the skyline, a palm — the sun sinks as the song plays
    float prog = arr.bars ? (float)bb / arr.bars : 0;
    rectfill(34, 52, 102, 116, CLR_DARK_PURPLE);
    rectfill(34, 84, 102, 24, CLR_DARK_RED);
    rectfill(34, 104, 102, 20, CLR_DARK_ORANGE);
    int sunY = 100 + (int)(prog * 14);
    circfill(85, sunY + 4, 22, CLR_ORANGE);
    circfill(85, sunY, 20, CLR_YELLOW);
    for (int i = 0; i < 4; i++) rectfill(50, sunY - 6 + i * 7, 70, 2 + i, CLR_DARK_ORANGE);
    unsigned hsh = sng.seed; int sx = 36;
    while (sx < 130) {
        hsh = hsh * 1664525u + 1013904223u;
        int bw = 8 + (int)(hsh % 12), bh = 18 + (int)((hsh >> 8) % 26);
        if (sx + bw > 134) bw = 134 - sx;
        rectfill(sx, 144 - bh, bw, bh + 24, CLR_DARKER_BLUE);
        if ((hsh >> 16) % 3 == 0) {
            int lit = radioOn && ((hsh >> 20) % 4 != 0 || (int)(timer() * 2 + (hsh >> 22)) % 3);
            rectfill(sx + 2, 146 - bh, bw - 4, 2, lit ? ((hsh >> 18) % 2 ? CLR_PINK : CLR_BLUE) : CLR_DARK_BLUE);
        }
        sx += bw + 2;
    }
    line(118, 166, 121, 138, CLR_BLACK);
    for (int f = 0; f < 5; f++) { float fa = -0.4f - f * 0.55f; line(121, 138, 121 + (int)(cosf(fa) * 12), 138 + (int)(sinf(fa) * 7), CLR_BLACK); }
    rect(34, 52, 102, 116, CLR_LIGHT_GREY);

    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_PINK);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_PINK);
        int geared = arr.truck && bb >= arr.modBar;
        char l2[32]; snprintf(l2, 32, "%.1f FM  key %s%s", sng.freq, RAD_PCNAME[key_at(bb)], geared ? "!" : "");
        print(l2, 154, 70, CLR_BLUE);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_BLUE);
        float vt = vu / 12.0f; rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_PINK);
    } else print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        int x = 152;
        for (int i = 0; i < nowN; i++) {
            int cw = text_width(nowChord[i]); if (x + cw > 292) break;
            if (i == nowIdx) { rectfill(x - 2, 102, cw + 4, 12, CLR_PINK); print(nowChord[i], x, 104, CLR_BLACK); }
            else print(nowChord[i], x, 104, CLR_DARK_GREY);
            x += cw + 8;
        }
        // the FORM strip: every section sized by its bars, the playhead crawling through
        int fx = 152, fw = 136, fy = 119, acc = 0;
        static const int SCOL[NSC] = { CLR_DARK_BLUE, CLR_BLUE, CLR_INDIGO, CLR_PINK, CLR_ORANGE, CLR_DARK_BLUE };
        for (int i = 0; i < arr.n; i++) {
            int x0 = fx + acc * fw / arr.bars, x1 = fx + (acc + arr.s[i].bars) * fw / arr.bars;
            rectfill(x0, fy, x1 - x0 - 1, 5, arr.sec[bb] == i ? CLR_BLACK : SCOL[arr.s[i].name]);
            acc += arr.s[i].bars;
        }
        int ph = fx + (int)(bb * fw / arr.bars); line(ph, fy - 2, ph, fy + 6, CLR_DARK_PURPLE);
        font(FONT_SMALL);
        const Sect *S = &arr.s[arr.sec[bb]];
        int geared = arr.truck && bb >= arr.modBar;
        print(str("%s %d/%d%s%s", SC_NAME[S->name], arr.j[bb] + 1, S->bars, geared && S->name == SC_CHORUS ? " +2!" : "",
                  arr.fill[bb] ? str("  fill: %s", FL_NAME[arr.fill[bb]]) : arr.push[bb] ? "  push" : ""), fx, fy + 8, CLR_DARK_PURPLE);
        font(FONT_NORMAL);
    }

    static const char *FEEL[4] = { "4am", "cruise", "downtown", "neon" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_PINK);
    if (rad_knob_int(&tempo, 96, 124, 2, 218, 148, 9, "tempo", CLR_PINK)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_PINK)) apply_tone();
    rad_power_led(radioOn, CLR_RED, CLR_DARK_RED);

    rad_help_button(CLR_PINK);
    rad_band_button(CLR_PINK);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next song (rolls a new seed)" },
            { "R",          "same song again" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - 4am .. neon density" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "B",          "band - keys/bass/lead/horns" },
            { "M",          "radio power on / off" },
        };
        static const char *NOTES[3] = {
            "every chorus sings the same hook; the horns",
            "answer in its breath. the last chorus goes up",
            "a whole step. of course it does.",
        };
        rad_help_panel("CITY POP RADIO II", HELP, 8, NOTES, 3, CLR_PINK);
    }
    rad_band_panel(&band, CLR_PINK);

    // the jam strip: chord-locked, the live chord's own tones (the lead lays out while you play)
    int chord[4]; {
        BCh c = chord_at_step(bb, (int)(songStep >= 0 ? songStep % 16 : 0));
        int r = root_pc((Ch){ c.off, c.q }, bb);
        chord[0] = r; for (int k = 0; k < 3; k++) chord[k + 1] = (r + QV4[c.q][k]) % 12;
    }
    static const int PENT[5] = { 0, 2, 4, 7, 9 };
    SoloCtx jc = { key_at(bb), PENT, 5, chord, 4, I_SOLO, 72, 91, false, SOLO_Y_OFF, 0, 0, false, true, true };
    solo_strip(&jc, 28, 170, 250, 18, CLR_PINK);
    ui_end();
}
