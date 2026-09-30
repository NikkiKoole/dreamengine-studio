/* de:meta
{
  "slug": "lofi",
  "collection": [
    "radio"
  ],
  "title": "lofi fm",
  "status": "active",
  "created": "2026-06-22",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "chord-voicing",
    "swing-timing"
  ],
  "lineage": "The lo-fi/Nujabes/Dilla pole of jazzy hip-hop, distinct from lowend's boom-bap; novel in THE DRUNK POCKET - a dialable off-grid time feel (snare-late/lazy-kick/swing + humanize), the loose pocket lowend undersold. Reuses vapor's lo-fi rack. 2026-09-29: the first station on radio.h's sample-clock grid (rad_audio_*), and the phase-1 pilot of docs/design/radio-arranger-lessons.md: the song is PLANNED up front - a form with per-part roles, an A/B pair or a ii-V turnaround, pushes, fills, hats-first, tone dips and a tone ride, a song that ends into a related key - all on a derived stream, so pinned seeds keep their key/mood/loop/title. A/B it against loficity. Later that day the BAND was recast the way loficity casts jazzhop (tools/arrange-score.js --sound found the gap was the sound, not the notes): a clean tape instead of a saturated one that squashed the Rhodes under every hit, a pizzicato upright, a morphdrum kit + rim, vibes/flute on top, the Rhodes' suitcase tremolo, the mix balanced by measured stems, and the drag moved off the tune onto the backbeat.",
  "homage": "Lo-fi hip-hop (Nujabes / J Dilla)",
  "todo": [
    "NO WAY TO JAM: the player can only tune the dial, never play along. Add a solo.h scale-locked solo strip (J toggles it) - the strip locks to the station's current key/scale so anything you touch is in tune. Worked examples: air, polopan, jangle, jingle, citypop, dub."
  ],
  "description": "Lo-fi jazzy hip-hop - the Nujabes / J Dilla / beats-to-study-to pole, dreamier and hazier than lowend's hard boom-bap (we ship that too). Lush extended Rhodes jazz (maj9/m11/13 loops) over a dusty SWUNG kit, wrapped in vinyl crackle + tape warmth. The headline brain is THE DRUNK POCKET - the off-grid feel: the snare drags LATE, the kick is lazy, the hats swing, with a little seeded humanize wobble - and it is ADJUSTABLE: a POCKET knob (LEFT/RIGHT) from tight (on the grid) -> loose -> behind -> drunk, defaulting to a tasteful moderate drag (never seasick unless you crank it); the loose feel lowend undersold, here done right and under the player's control. Every song is now PLANNED before it plays (the loficity lesson): a form - intro / A / B / break / outro - where each section gives the keys, bass, drums and dab their own role (held chords in the intro and break, hats coming in first, a whole-note bass, the dab only from the second A), an A loop and a B that is its own lofi move (IV-III7-vi, IV-iv-I, bVI-V-I...) or the A loop with a ii-V turnaround, 1 or 2 bars a chord and changes mid-bar, and the master tone opening through the intro and closing in the outro. Then every BAR is planned knowing the next one: the Rhodes picks a comp rhythm (held, charleston, pulses, a late stab, the and-of-3 lift - each mood has its taste) in four-voice rootless voicings with a strum, and PUSHES the next chord onto the and-of-4; the kit plays the song's groove (one of five, half-time for the slow ones) with open hats and ghost snares in B, fills before a change and the odd dropped-kick breath; the bass lands on the kicks and the changes and walks into the next root from a half-step; and the vibes / flute / muted-horn lead grows the song's own dab cell into a PHRASE every four bars - stated, then answered, strong beats on chord tones, each phrase starting where the last ended, inverted in B. The song ends and the next moves to a related key; a form strip shows where you are. The band: a Rhodes with its suitcase tremolo and autopan, a pizzicato upright (a bowed-string model, plucked), a modeled drum kit with a rim click and accented hats, vibes or a flute on top. The lo-fi rack (set-and-hold, per mood) is a CLEAN tape (a light wow, a little flutter, no saturation) + a big soft room + an echo + a gentle bus glue. The seed rolls a MOOD: sleepy / dusty / rainy / sunny (each sets tempo, scale, the default pocket and the crackle). The window is a cozy night room: a rainy window, a breathing desk lamp, and a turntable whose platter spins with the tempo. SPACE next, R replay, [ ] history, LEFT/RIGHT pocket (tight..drunk), UP/DOWN tempo, T tone, B band (keys rhodes/wurli, bass upright/round, lead vibes/flute/horn/off), M power, H help. Pin via LOFI_SEED."
}
de:meta */
// ── LOFI FM — lo-fi jazzy hip-hop ─────────────────────────────────────────────
// The lo-fi / Nujabes / J Dilla pole of jazzy hip-hop — dreamier and hazier than
// lowend's hard boom-bap (we ship that already). Lush extended Rhodes jazz over a
// dusty swung kit, wrapped in vinyl crackle + tape warmth: "beats to study to".
// Blind brief: docs/design/lofi-blind-brief.md.
//
// The brains (cart-side over radio.h's clock):
//   • THE DRUNK POCKET — the off-grid feel: the snare drags LATE, the kick is lazy,
//     the hats swing, with a little seeded humanize wobble. ADJUSTABLE: a POCKET knob
//     (tight -> drunk), default moderate — never seasick unless you crank it. The loose
//     feel lowend undersold, here done right and under the player's control. Only the BACKBEAT drags:
//     the keys and the lead stay near the grid (dragging them read as sluggish, not laid back).
//   • THE ARRANGEMENT — a song planned up front (form, A/B harmony, per-part roles), then each
//     BAR planned knowing the next: comp rhythms + the push, groove + fills, a kick-locked bass,
//     a lead phrase grown from the dab cell. (docs/design/radio-arranger-lessons.md)
//   • THE LO-FI RACK (set-and-hold, per mood) — tape wow/flutter, gentle echo + reverb,
//     a high rolloff, and a held vinyl-crackle bed. (reused from vapor's playbook.)
//   • Mood roll: sleepy / dusty / rainy / sunny.
//
//   SPACE next   R replay   [ ] history   LEFT/RIGHT pocket (tight..drunk)
//   UP/DOWN tempo   T tone   B band   M power   H help

#include "studio.h"
#include "radio.h"
#include "morphdrum.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define LOFI_SEED 0

// ── slots ───────────────────────────────────────────────────────────────────
// The BAND is cast the way loficity casts its jazzhop style (2026-09-29): a Rhodes with the suitcase
// tremolo on a short + a long slot, a pizzicato upright (BOWED), a morphdrum kit + a modal rim, vibes or a
// flute on top, and a clean tape (no saturation, a light wow, a little flutter). The arrangement said
// the notes were comparable; the sound was what still separated the two (radio-arranger-lessons.md §6).
#define I_EP   5    // Rhodes (the harmonic core), short release: the comp
#define I_BASS 6    // pizzicato upright / round sine
#define I_DAB  7    // the lead: vibes / flute / muted horn
#define I_KICK 8    // (event tags only: the kit's voices live on KIT_BASE)
#define I_SNR  9
#define I_HAT  10
#define I_VINYL 11  // held vinyl-crackle bed (unused: it read as hard hiss)
#define I_RIM  12   // a modal rim click (the grooves' 'r' strokes)
#define I_EPL  13   // the Rhodes again, long release: held chords
#define KIT_BASE 20 // morphdrum slots 20..29

// ── chord qualities (the four-voice rootless voicings, QV4, live with the arrangement below) ──
enum { Q_MAJ9, Q_MIN9, Q_DOM9, Q_MAJ7, Q_MIN7, NQ };
static const char *QN[NQ] = { "maj9", "m9", "9", "maj7", "m7" };
// a mood's scale: 0 major, 1 dorian, 2 aeolian (the lead plays its major or minor pentatonic)

// ── the hazy jazz loops (off, quality) — slow, static ───────────────────────
typedef struct { int off, q; } Ch;
#define NLOOP 5
static const struct { int n; Ch c[4]; } LOOPS[NLOOP] = {
    { 2, { { 0, Q_MAJ9 }, { 9, Q_MIN9 } } },                                   // Imaj9 - vi9
    { 4, { { 2, Q_MIN9 }, { 7, Q_DOM9 }, { 0, Q_MAJ9 }, { 0, Q_MAJ9 } } },     // ii-V-I
    { 4, { { 9, Q_MIN9 }, { 2, Q_MIN9 }, { 7, Q_DOM9 }, { 0, Q_MAJ9 } } },     // vi-ii-V-I turnaround
    { 4, { { 0, Q_MAJ9 }, { 5, Q_MIN7 }, { 8, Q_DOM9 }, { 0, Q_MAJ9 } } },     // jazzy loop
    { 3, { { 0, Q_MAJ7 }, { 5, Q_MAJ9 }, { 7, Q_DOM9 } } },                    // I-IV-V haze
};

// ── moods — tempo, scale, default pocket depth, the lo-fi rack amounts ──────
enum { MO_SLEEPY, MO_DUSTY, MO_RAINY, MO_SUNNY, NMO };
typedef struct {
    const char *name; int tlo, tspan, scale, pocket;
    float rev, wow, sat, echoFb; int echoMs;
} MoodDef;
static const MoodDef MOOD[NMO] = {
    //  name      tlo tsp sc pk  rev   wow   sat   efb   ems
    // wow + sat were 0.24-0.36 / 0.26-0.42 (with flutter at 0.6x the wow): the tape squashed the Rhodes
    // under every drum hit and warbled the pitch. loficity measured the same curve: even sat 0.02 squashes
    // (tape() is normalised, so it adds small-signal gain), and flutter 0.12 already warbles. Now theirs.
    { "sleepy",   72,  8, 0, 2, 0.54f,0.09f,0.00f,0.30f, 400 },
    { "dusty",    82,  8, 1, 2, 0.52f,0.07f,0.00f,0.28f, 340 },
    { "rainy",    78,  8, 2, 1, 0.56f,0.10f,0.00f,0.32f, 440 },
    { "sunny",    88, 10, 0, 1, 0.50f,0.05f,0.00f,0.26f, 300 },
};
// the POCKET dial — drag scale 0 (on the grid) .. 1.0 (deep Dilla)
static const float POCKETV[4] = { 0.0f, 0.45f, 0.75f, 1.0f };
static const char *POCKETN[4] = { "tight", "loose", "behind", "drunk" };

// ── the generated tune ───────────────────────────────────────────────────────
typedef struct {
    int mood, keyPc, scale, loop;
    int cellOn[5], cellDeg[5], cellN;
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 160.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)   rad_srnd(&rs, (n))

static int   tempo     = 82;
static int   pocketSel = 1;        // LEFT/RIGHT — THE feel dial (default moderate)
static int   toneSel   = 2;       // "clear" (1.0x): the old "warm" default took 22% off every filter
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   songCount = 0;
static float vu = 0, platter = 0;     // platter = the turntable angle (visual)
static int   gvEP[4] = { 60, 64, 67, 71 }; static bool epInit = false;
static int   bassLast = 40, vinylH = -1;
static char  nowChord[4][8]; static int nowN = 1, nowIdx = 0;

static void apply_fx(void);
static void voice_song(void);                  // the per-song sound rolls (below): Rhodes tone + tremolo, master tone
static void apply_tone(void);
static struct { int epLp; float tremRate, tremDepth; double tone; } snd = { 3200, 4.5f, 0.1f, 7000 };
static void fire_kick(double t, int v);        // the kit's layered voices (below)
static void fire_snare(double t, int v);
static void fire_hat(double t, int v, int open);
static void apply_chair(int idx);
static void plan_song(unsigned seed);   // the arrangement (below): planned at new_song
static void plan_reset(void);           // ...and the bar planner's per-song state

static RadBand band;
static int chKeys, chBass, chDab;

// ── song generation ─────────────────────────────────────────────────────────
static const char *TW1[] = { "Rainy", "Midnight", "Dusty", "Velour", "Tokyo", "Late",
    "Amber", "Window", "Coffee", "Mellow", "Feather", "Lantern" };
static const char *TW2[] = { "Window", "Tape", "Study", "Loop", "Nights", "Hours",
    "Mood", "Haze", "Cassette", "Dream", "Steps", "Rain" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);
    sng.mood  = srnd(NMO);
    sng.keyPc = srnd(12);
    sng.scale = MOOD[sng.mood].scale;
    sng.loop  = srnd(NLOOP);
    pocketSel = MOOD[sng.mood].pocket;          // the mood sets a tasteful default; the knob overrides
    // a sparse dab contour
    sng.cellN = 0; int d = srnd(4);
    for (int s = 0; s < 28 && sng.cellN < 5; s += 4 + srnd(4))
        if (srnd(100) < 60) { sng.cellOn[sng.cellN] = s; sng.cellDeg[sng.cellN] = d;
                              d += srnd(5) - 2; if (d > 8) d -= 7; if (d < 0) d += 7; sng.cellN++; }
    if (sng.cellN < 2) { sng.cellN = 2; sng.cellOn[0] = 6; sng.cellDeg[0] = 0; sng.cellOn[1] = 18; sng.cellDeg[1] = 4; }
    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;
    tempo = MOOD[sng.mood].tlo + srnd(MOOD[sng.mood].tspan);
    bpm(tempo);
    apply_fx();
    songBase = (long)pos + 4;
    plan_song(sng.seed); plan_reset(); voice_song();
    epInit = false; bassLast = 40;
    songCount++;
}
static void fresh_song(double pos) { new_song(pos, 0); rad_hist_log(&rs); }

// ── harmony ───────────────────────────────────────────────────────────────
static int  root_pc(Ch c)  { return (sng.keyPc + c.off) % 12; }
static void chord_label(char *out, int n, Ch c) { snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(c)], QN[c.q]); }
static int  bass_peek(int pc, int lo, int hi) {
    int dd = ((pc - bassLast) % 12 + 18) % 12 - 6, m = bassLast + dd;
    while (m < lo) m += 12; while (m > hi) m -= 12; return m;
}


// ── THE ARRANGEMENT — planned per SONG, then played per BAR (docs/design/radio-arranger-lessons.md) ──
// A step player can only ask "what happens NOW?", so every bar comes out the same. This plans twice:
//   • per SONG (plan_song, at new_song): a FORM (intro / A / B / break / outro, grown to ~2.5 min) where
//     each section gives every part a ROLE; an A/B pair of progressions (B = its own lofi move, or A with
//     a ii-V turnaround); the kit's GROOVE; the comping taste; whether the bass is legato; and per bar the
//     look-ahead gestures (a PUSH onto the next chord, a FILL before a change, a dropped-kick breath).
//   • per BAR (plan_bar, at the bar's first step): every note of the bar, knowing the NEXT bar: the keys
//     pick a COMP RHYTHM and clip it to the chords (which may change mid-bar); the drums play the groove
//     with the B section's open hats + ghost notes; the BASS lands on the KICKS and the changes and walks
//     into the next root from a half-step; the LEAD grows the song's own dab cell into a PHRASE per four
//     bars — the cell stated, then answered, strong beats on chord tones, each phrase starting where the
//     last one ended, inverted and shifted in B.
// The step clock then books the planned bar; THE DRUNK POCKET is applied at booking, so the knob is live.
// All composition draws from DERIVED streams (arr_seed), never rad_srnd: a pinned seed keeps its key,
// mood, A loop, dab cell and title (the seed rule), and gains an arrangement.
enum { SC_INTRO, SC_A, SC_B, SC_BREAK, SC_OUTRO };
static const char *SC_NAME[5] = { "intro", "A", "B", "break", "outro" };
enum { KY_COMP, KY_HOLD, KY_OUTRO };
enum { BA_ON, BA_WHOLE, BA_LAST, BA_OFF, BA_OUTRO };
enum { DR_FULL, DR_HATSIN, DR_NONE, DR_BREAK, DR_OUTRO };
enum { FL_NONE, FL_ROLL, FL_STOP, FL_PICKUP, FL_OPEN };
static const char *FL_NAME[5] = { "", "roll", "stop", "pickup", "open hat" };
typedef struct { int keys, bass, drums, lead, hatsFirst, dipLast; } Roles;
typedef struct { int name, bars; Roles r; } Sect;

// a progression: chords with lengths in BEATS, so a chord can change mid-bar
typedef struct { int off, q, beats; } PCh;
typedef struct { int n, beats; PCh c[12]; } Prog;
// B's own bank: the lofi moves the A loops never make (A stays LOOPS[sng.loop], the seed's loop)
#define NBLOOP 5
static const struct { int n; PCh c[5]; } BLOOPS[NBLOOP] = {
    { 5, { { 5, Q_MAJ9, 4 }, { 4, Q_DOM9, 4 }, { 9, Q_MIN9, 4 }, { 7, Q_MIN7, 2 }, { 0, Q_DOM9, 2 } } },  // IV III7 vi v-I7
    { 4, { { 5, Q_MAJ9, 4 }, { 5, Q_MIN9, 4 }, { 0, Q_MAJ9, 4 }, { 9, Q_MIN9, 4 } } },                   // IV iv I vi — the sigh
    { 4, { { 2, Q_MIN9, 4 }, { 7, Q_DOM9, 4 }, { 4, Q_MIN7, 4 }, { 9, Q_MIN9, 4 } } },                   // ii V iii vi
    { 3, { { 8, Q_MAJ9, 4 }, { 7, Q_DOM9, 4 }, { 0, Q_MAJ9, 8 } } },                                     // bVI V I
    { 5, { { 9, Q_MIN9, 4 }, { 8, Q_DOM9, 4 }, { 7, Q_MIN7, 2 }, { 0, Q_DOM9, 2 }, { 5, Q_MAJ9, 4 } } },  // vi bVI7 v-I7 IV, falling
};
// four-voice rootless voicings (the bass owns the root)
static const int QV4[NQ][4] = { { 4, 7, 11, 14 }, { 3, 7, 10, 14 }, { 4, 10, 14, 21 }, { 4, 7, 11, 12 }, { 3, 7, 10, 12 } };

// the kit: one GROOVE per song. x hit · g ghost · r rim-soft
typedef struct { const char *k, *s, *h; } Groove;
#define NGROOVE 5
static const Groove GROOVES[NGROOVE] = {
    { "x.......x.x.....", "....x.......x...", "x.x.x.x.x.x.x.x." },   // the lazy boom
    { "x..x......x.....", "....x.......x...", "x.x.x.x.x.x.x.x." },   // the skip
    { "x.......xx......", "....x..g....x...", "x.xxx.x.x.xxx.x." },   // the shuffle: a ghost, paired hats
    { "x.....x...x.....", "....x.......x..g", "xxxxxxxxxxxxxxxx" },   // soft sixteenths
    { "x.........x.....", "........x.......", "x.x.x.x.x.x.x.x." },   // half-time, for the slow ones
};
static const Groove BREAKGRV = { "x...............", "............r...", "..x...x...x...x." };

// the keys' COMP RHYTHMS: step, length, vol, top (= only the top two voices, a lighter stab)
typedef struct { int s, len, v, top; } Cell;
typedef struct { int n; Cell c[3]; } Comp;
enum { CP_HOLD, CP_CHARL, CP_PULSE, CP_LATE, CP_ANTIC, NCP };
static const Comp COMPS[NCP] = {
    { 1, { { 0, 16, 4, 0 } } },                                   // hold the bar
    { 2, { { 0, 3, 4, 0 }, { 6, 10, 3, 0 } } },                   // charleston
    { 2, { { 0, 6, 4, 0 }, { 8, 8, 3, 0 } } },                    // two pulses
    { 3, { { 0, 4, 4, 0 }, { 7, 2, 2, 1 }, { 10, 6, 3, 0 } } },   // the late stab
    { 2, { { 0, 10, 4, 0 }, { 10, 6, 3, 1 } } },                  // the and-of-3 lift
};
static const int COMPW[NMO][NCP] = {        // each mood's comping taste (a song jitters it)
    { 6, 2, 2, 1, 1 },   // sleepy: mostly held
    { 3, 3, 2, 3, 2 },   // dusty
    { 5, 2, 2, 2, 2 },   // rainy
    { 2, 3, 3, 3, 3 },   // sunny: busiest
};
static const int PUSHODDS[NMO] = { 20, 30, 25, 35 };

#define MAXSECT 14
#define MAXBAR  128
static struct {
    Sect s[MAXSECT]; int n, bars;
    Prog A, B;                              // A's loop; B's own move, or A with a ii-V turnaround
    int turnB, groove, legato, strum, compW[NCP];
    unsigned char sec[MAXBAR], j[MAXBAR], push[MAXBAR], fill[MAXBAR], comp[MAXBAR], drop[MAXBAR];
} arr;
static unsigned arr_rng = 1;
static void arr_seed(unsigned seed, unsigned k) {
    unsigned h = seed * 2654435761u ^ (k * 0x9E3779B9u); h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    arr_rng = h ? h : 1;
}
static int arnd(int n) { arr_rng ^= arr_rng << 13; arr_rng ^= arr_rng >> 17; arr_rng ^= arr_rng << 5; return (int)(arr_rng % (unsigned)n); }
static int wpick(const int *w, int n) { int t = 0; for (int i = 0; i < n; i++) t += w[i]; int u = arnd(t > 0 ? t : 1); for (int i = 0; i < n; i++) if ((u -= w[i]) < 0) return i; return 0; }

// lofi's own forms (not another station's numbers)
static const int FORMS[3][8][2] = {
    { { SC_INTRO, 4 }, { SC_A, 8 }, { SC_B, 8 }, { SC_A, 8 }, { SC_BREAK, 4 }, { SC_B, 8 }, { SC_OUTRO, 4 }, { -1, 0 } },
    { { SC_INTRO, 2 }, { SC_A, 8 }, { SC_A, 8 }, { SC_B, 8 }, { SC_BREAK, 8 }, { SC_A, 8 }, { SC_OUTRO, 4 }, { -1, 0 } },
    { { SC_INTRO, 4 }, { SC_A, 16 }, { SC_B, 8 }, { SC_BREAK, 4 }, { SC_A, 8 }, { SC_B, 8 }, { SC_OUTRO, 4 }, { -1, 0 } },
};

static void prog_add(Prog *p, int off, int q, int beats) { if (p->n < 12) { PCh c = { off % 12, q, beats }; p->c[p->n++] = c; p->beats += beats; } }
static void prog_turn(Prog *p) {           // replace the last 4 beats with a ii-V back to the top
    int cut = 4, t = p->c[0].off;
    while (cut > 0 && p->n) { PCh *l = &p->c[p->n - 1]; if (l->beats > cut) { l->beats -= cut; cut = 0; } else { cut -= l->beats; p->n--; } }
    p->beats -= 4; prog_add(p, t + 2, Q_MIN9, 2); prog_add(p, t + 7, Q_DOM9, 2);
}
static const Prog *sect_prog(int si) { return arr.s[si].name == SC_B ? &arr.B : &arr.A; }

typedef struct { int off, q, s, len; } BCh;   // a chord inside one bar: start step + length in steps
static long bar_clamp(long bar) { return bar < 0 ? 0 : bar >= arr.bars ? arr.bars - 1 : bar; }
static int bar_chords(long bar, BCh *out) {
    bar = bar_clamp(bar);
    const Sect *S = &arr.s[arr.sec[bar]]; int j = arr.j[bar];
    if (S->name == SC_OUTRO && j >= 2) { BCh h = { 0, Q_MAJ9, 0, 16 }; out[0] = h; return 1; }   // the outro rings on home
    const Prog *p = sect_prog(arr.sec[bar]);
    int b0 = (j * 4) % p->beats, n = 0, at = 0;
    for (int i = 0; i < p->n; i++) {
        int cs = at, ce = at + p->c[i].beats; at = ce;
        int s = cs > b0 ? cs : b0, e = ce < b0 + 4 ? ce : b0 + 4;
        if (e > s) { BCh c = { p->c[i].off, p->c[i].q, (s - b0) * 4, (e - s) * 4 }; out[n++] = c; }
    }
    return n;
}
static const Roles *arr_roles(long bar) { return &arr.s[arr.sec[bar_clamp(bar)]].r; }

static void plan_song(unsigned seed) {
    arr_seed(seed, 1);
    int f = arnd(3), nm[MAXSECT], nb[MAXSECT], n = 0;
    for (int i = 0; i < 8 && FORMS[f][i][0] >= 0; i++) { nm[n] = FORMS[f][i][0]; nb[n] = FORMS[f][i][1]; n++; }
    double barSec = 240.0 / tempo;
    for (;;) {                                               // grow to ~2.5 min with B + A pairs before the outro
        int tot = 0; for (int i = 0; i < n; i++) tot += nb[i];
        if (tot * barSec >= 150 || n + 2 > MAXSECT || tot + 16 > MAXBAR) break;
        nm[n + 1] = nm[n - 1]; nb[n + 1] = nb[n - 1]; nm[n - 1] = SC_B; nb[n - 1] = 8; nm[n] = SC_A; nb[n] = 8; n += 2;
    }
    // harmony: A = the seed's loop at 1 or 2 bars a chord; B = a lofi move of its own, or A + a ii-V turnaround
    int cbars = arnd(100) < 55 ? 2 : 1;
    memset(&arr.A, 0, sizeof arr.A); memset(&arr.B, 0, sizeof arr.B);
    for (int i = 0; i < LOOPS[sng.loop].n; i++) prog_add(&arr.A, LOOPS[sng.loop].c[i].off, LOOPS[sng.loop].c[i].q, 4 * cbars);
    arr.turnB = arnd(100) < 35;
    if (arr.turnB) { arr.B = arr.A; prog_turn(&arr.B); }
    else { int b = arnd(NBLOOP); for (int i = 0; i < BLOOPS[b].n; i++) prog_add(&arr.B, BLOOPS[b].c[i].off, BLOOPS[b].c[i].q, BLOOPS[b].c[i].beats); }
    // the band's taste for this song
    do arr.groove = arnd(NGROOVE); while (arr.groove == 4 && tempo >= 82);   // half-time only for the slow ones
    arr.legato = arnd(100) < 45;
    arr.strum  = 3 + arnd(8);    // ms per voice: a 9-30 ms roll over four notes (loficity: 10-30)
    for (int i = 0; i < NCP; i++) arr.compW[i] = COMPW[sng.mood][i] + arnd(3);
    int aCount = 0;
    for (int i = 0; i < n; i++) {
        Roles r = { KY_COMP, BA_ON, DR_FULL, 0, 0, 0 };
        switch (nm[i]) {
        case SC_INTRO: r.keys = KY_HOLD; r.bass = arnd(2) ? BA_LAST : BA_OFF; r.drums = arnd(100) < 45 ? DR_HATSIN : DR_NONE; break;
        case SC_A:     aCount++; r.lead = aCount >= 2; break;
        case SC_B:     r.lead = 1; r.hatsFirst = arnd(100) < 30; break;
        case SC_BREAK: r.keys = KY_HOLD; r.bass = arnd(2) ? BA_WHOLE : BA_OFF; r.drums = arnd(100) < 50 ? DR_BREAK : DR_NONE; r.lead = arnd(100) < 40; break;
        default:       r.keys = KY_OUTRO; r.bass = BA_OUTRO; r.drums = DR_OUTRO; break;
        }
        if ((nm[i] == SC_A || nm[i] == SC_B) && i < n - 1) r.dipLast = arnd(100) < 25;
        arr.s[i].name = nm[i]; arr.s[i].bars = nb[i]; arr.s[i].r = r;
    }
    arr.n = n; arr.bars = 0;
    for (int i = 0; i < n; i++) for (int j = 0; j < nb[i]; j++) { arr.sec[arr.bars] = (unsigned char)i; arr.j[arr.bars] = (unsigned char)j; arr.bars++; }
    // per bar, knowing the NEXT bar: the comp rhythm, the push, a fill, a dropped-kick breath
    for (int b = 0; b < arr.bars; b++) {
        const Sect *S = &arr.s[arr.sec[b]]; int j = arr.j[b], last = j == S->bars - 1;
        int w[NCP]; for (int i = 0; i < NCP; i++) w[i] = arr.compW[i];
        if (S->name == SC_B) w[CP_HOLD] /= 2;                                  // B moves more than A
        arr.comp[b] = (unsigned char)wpick(w, NCP);
        arr.push[b] = 0; arr.fill[b] = FL_NONE; arr.drop[b] = 0;
        if (b + 1 < arr.bars && S->r.keys == KY_COMP && arr_roles(b + 1)->keys == KY_COMP) {
            BCh a[6], c[6]; int na = bar_chords(b, a); bar_chords(b + 1, c);
            if ((a[na - 1].off != c[0].off || a[na - 1].q != c[0].q) && arnd(100) < PUSHODDS[sng.mood]) arr.push[b] = 1;
        }
        if (S->r.drums == DR_FULL && b + 1 < arr.bars) {
            int odds = last ? 55 : (j + 1) % 4 == 0 ? 15 : 0;
            if (odds && arnd(100) < odds) arr.fill[b] = 1 + arnd(4);
        }
        if (S->r.drums == DR_FULL && j > 0 && !arr.fill[b] && arnd(100) < 6) arr.drop[b] = 1;
    }
}

// ── the bar planner: every note of one bar, as events the step clock books ──
enum { LN_KICK, LN_SNR, LN_HAT, LN_EP, LN_BASS, LN_LEAD };
typedef struct { signed char step, lane; short off, midi, instr, vol; int dur; } BEv;
static struct { BEv e[112]; int n; long bar; } bev = { .bar = -1 };
static void ev(int step, int lane, int off, int midi, int instr, int vol, int durMs) {
    if (bev.n >= 112) return;
    BEv x = { (signed char)step, (signed char)lane, (short)off, (short)midi, (short)instr, (short)vol, durMs < 20 ? 20 : durMs };
    bev.e[bev.n++] = x;
}
static void lead_to4(int rootpc, const int *iv, int lo, int hi) {   // rad_lead_to, for four voices
    int pcs[4]; for (int k = 0; k < 4; k++) pcs[k] = (rootpc + iv[k]) % 12;
    if (!epInit) {
        for (int k = 0; k < 4; k++) { int t = lo + 4 + k * 5, dd = ((pcs[k] - t) % 12 + 18) % 12 - 6; gvEP[k] = t + dd; }
        epInit = true;
    } else {
        bool used[4] = { false, false, false, false };
        for (int vi = 0; vi < 4; vi++) {
            int bj = 0, bc = gvEP[vi], bd = 99;
            for (int j = 0; j < 4; j++) { if (used[j]) continue; int dd = ((pcs[j] - gvEP[vi]) % 12 + 18) % 12 - 6; if (abs(dd) < bd) { bd = abs(dd); bj = j; bc = gvEP[vi] + dd; } }
            used[bj] = true; gvEP[vi] = bc;
        }
    }
    for (int k = 0; k < 4; k++) { while (gvEP[k] < lo) gvEP[k] += 12; while (gvEP[k] > hi) gvEP[k] -= 12; }
    for (int a = 1; a < 4; a++) { int v = gvEP[a], b = a - 1; while (b >= 0 && gvEP[b] > v) { gvEP[b + 1] = gvEP[b]; b--; } gvEP[b + 1] = v; }
}
typedef struct { int s, len, v, top; BCh c; } KHit;
static void key_hit(const KHit *h) {
    lead_to4(root_pc((Ch){ h->c.off, h->c.q }), QV4[h->c.q], 52, 76);
    int dur = (int)(h->len * stepMs) - 30; if (dur < 100) dur = 100;
    int slot = h->len >= 16 ? I_EPL : I_EP, v = h->v + 2 > 7 ? 7 : h->v + 2;   // a Rhodes played this soft never barks
    for (int k = h->top ? 2 : 0; k < 4; k++) ev(h->s, LN_EP, (k - (h->top ? 2 : 0)) * arr.strum, gvEP[k], slot, k == 3 && v < 7 ? v + 1 : v, dur);
}

// the lead's pitch ladder: the key's MAJOR pentatonic, in the dab's register. Always major, whatever the
// mood's scale: every loop is written in the major key (Imaj9, ii-V-I, vi9), so a minor pentatonic on the
// same tonic put Db/Ab over Bbmaj9/F9 (the "wrong scale" solo). Major pentatonic = the relative minor's
// pentatonic, so the dusty/rainy moods keep their minor colour from the same five notes.
static int ladder[24], nLadder = 0;
static void build_ladder(void) {
    static const int p[5] = { 0, 2, 4, 7, 9 }; nLadder = 0;
    for (int m = 62; m <= 81 && nLadder < 24; m++) for (int k = 0; k < 5; k++) if ((m - sng.keyPc + 120) % 12 == p[k]) ladder[nLadder++] = m;
}
static int ladder_near(int midi) { int bi = 0; for (int i = 1; i < nLadder; i++) if (abs(ladder[i] - midi) < abs(ladder[bi] - midi)) bi = i; return bi; }
static int is_chord_tone(int midi, BCh c) {
    int pc = (midi % 12 + 12) % 12, r = root_pc((Ch){ c.off, c.q });
    if (pc == r || pc == (r + 7) % 12) return 1;
    for (int k = 0; k < 4; k++) if ((r + QV4[c.q][k]) % 12 == pc) return 1;
    return 0;
}
// an AVOID note: a semitone above a chord tone (a b9 against it). The borrowed chords (bVI, iv, III7) take
// the pentatonic outside the key, so every lead note is checked against the chord it lands on.
static int is_avoid(int midi, BCh c) {
    if (is_chord_tone(midi, c)) return 0;
    return is_chord_tone(midi - 1, c);
}
static BCh chord_at_step(long bar, int s) { BCh c[6]; int n = bar_chords(bar, c); for (int i = n - 1; i >= 0; i--) if (s >= c[i].s) return c[i]; return c[0]; }
static int snap_chord(int li, BCh c) {      // the nearest ladder step that is a chord tone (else stay)
    for (int d = 0; d < 4; d++) {
        if (li - d >= 0 && is_chord_tone(ladder[li - d], c)) return li - d;
        if (li + d < nLadder && is_chord_tone(ladder[li + d], c)) return li + d;
    }
    return li;
}
static struct { int n; unsigned char s[16], d[16], v[16]; signed char li[16]; long gbar; } phr = { .gbar = -1 };
static int leadLast = -1, statement = 0;
// the answer's rhythms: grouped like the motif (they were a beat apart, so the answer broke into single notes too)
static const int ANS[5][5] = { { 0, 2, 4, 10, -1 }, { 2, 4, 6, 12, -1 }, { 0, 2, 6, 8, 14 }, { 4, 6, 8, 12, -1 }, { 0, 3, 6, 8, -1 } };
static void phr_add(int s, int li, int v) { if (phr.n < 16) { phr.s[phr.n] = (unsigned char)s; phr.li[phr.n] = (signed char)li; phr.v[phr.n] = (unsigned char)v; phr.d[phr.n] = 2; phr.n++; } }
// THE MOTIF'S RHYTHM. The dab cell's own onsets were 4-7 sixteenths apart and each kept only 60% of the time,
// so the lead was single notes a beat or two apart: 1.4 notes a phrase vs loficity's 2.5 (arrange-score).
// A motif is GROUPS: two to four notes a 16th or an 8th apart, then a breath. The cell keeps its PITCHES
// (rad_srnd: a pinned seed's melody), each anchoring one group; the rhythm and the inner steps come from a
// derived stream. lofi's own cells, not loficity's table.
static const struct { int n; unsigned char s[8], g[8]; } MOTIFS[] = {
    { 5, { 0, 2, 4, 10, 12 },        { 0, 0, 0, 1, 1 } },          // three up, two answer
    { 5, { 0, 3, 6, 8, 16 },         { 0, 0, 0, 0, 1 } },          // a four-note run, one long note
    { 6, { 2, 4, 6, 12, 14, 20 },    { 0, 0, 0, 1, 1, 2 } },       // off the beat, three groups
    { 6, { 0, 2, 3, 6, 14, 16 },     { 0, 0, 0, 0, 1, 1 } },       // a turn, then a pair
    { 5, { 1, 4, 6, 8, 18 },         { 0, 0, 0, 0, 1 } },          // a pickup into the run
    { 6, { 0, 4, 6, 8, 10, 22 },     { 0, 1, 1, 1, 1, 2 } },       // a stab, a run, a tail
    { 5, { 4, 6, 8, 14, 16 },        { 0, 0, 0, 1, 1 } },          // late: the space first
};
#define NMOTIF ((int)(sizeof MOTIFS / sizeof *MOTIFS))
static struct { int n; unsigned char s[8], g[8]; signed char dir[8]; } mot;
static void plan_motif(void) {
    arr_seed(sng.seed, 9);
    int m = arnd(NMOTIF); mot.n = MOTIFS[m].n;
    for (int i = 0; i < mot.n; i++) { mot.s[i] = MOTIFS[m].s[i]; mot.g[i] = MOTIFS[m].g[i]; mot.dir[i] = 0; }
    for (int i = 1, run = 0, d = arnd(100) < 55 ? 1 : -1; i < mot.n; i++) {
        if (mot.g[i] != mot.g[i - 1]) { run = 0; d = arnd(100) < 55 ? 1 : -1; continue; }   // a new group, a new direction
        run++;
        int u = arnd(100);                                                                   // a run turns back at its end,
        mot.dir[i] = (signed char)(run >= 3 || u < 18 ? -d : u < 36 ? 0 : d);             // and sometimes repeats a note
    }
}
// one phrase per four bars: the song's own dab cell STATED over two bars, then ANSWERED over two
static void build_phrase(long gbar) {
    const Sect *S = &arr.s[arr.sec[gbar]];
    phr.gbar = gbar; phr.n = 0;
    arr_seed(sng.seed, 5000 + (unsigned)gbar);
    if ((arr.j[gbar] / 4) % 2 == 1 && arnd(100) < 30) return;         // a group of space
    statement++;
    int inv = S->name == SC_B, shift = S->name == SC_B ? 2 : 0;
    int li = ladder_near(leadLast > 0 ? leadLast : 70);              // start where the last phrase ended
    int x = li;
    for (int i = 0; i < mot.n; i++) {
        int s = mot.s[i] + shift; if (s >= 30) continue;
        int g = mot.g[i], first = i == 0 || mot.g[i - 1] != g;
        if (first) { int d = sng.cellDeg[g % sng.cellN] - sng.cellDeg[0]; x = li + (inv ? -d : d); }   // the cell's pitch anchors the group
        else x += inv ? -mot.dir[i] : mot.dir[i];                                                        // inside a group: a scale step
        if (x < 0) x = 1; if (x >= nLadder) x = nLadder - 2;
        BCh c = chord_at_step(gbar + s / 16, s % 16);
        if (s % 8 == 0 || is_avoid(ladder[x], c)) x = snap_chord(x, c);
        phr_add(s, x, first ? 4 : 3);
    }
    int at = phr.n ? phr.li[phr.n - 1] : li;
    if (statement % 3 == 0 && at > 0) {                               // every third statement: a pickup
        int x = at - 1; BCh c = chord_at_step(gbar + 1, 15);
        if (is_avoid(ladder[x], c)) x = snap_chord(x, c);
        phr_add(31, x, 2);
    }
    const int *A = ANS[arnd(5)]; int na = 0; while (na < 5 && A[na] >= 0) na++;
    for (int k = 0; k < na; k++) {
        int s = 32 + A[k];
        { int st = arnd(100) < 75 ? 1 : 2; at += arnd(100) < 60 ? -st : st; }   // answers mostly fall, mostly by step
        if (at < 0) at = 1; if (at >= nLadder) at = nLadder - 2;
        BCh c = chord_at_step(gbar + s / 16, s % 16);
        if (s % 8 == 0 || k == na - 1 || is_avoid(ladder[at], c)) at = snap_chord(at, c);
        phr_add(s, at, k == na - 1 ? 3 : 2);
    }
    for (int k = 0; k < phr.n; k++) {                                 // each note lasts to the next (the last one rings)
        int nx = k + 1 < phr.n ? phr.s[k + 1] : 64, g = nx - phr.s[k];
        phr.d[k] = (unsigned char)(k + 1 < phr.n ? (g > 6 ? 6 : g) : (g > 10 ? 10 : g));
    }
    if (phr.n) leadLast = ladder[phr.li[phr.n - 1]];
}

static void plan_reset(void) { build_ladder(); plan_motif(); phr.gbar = -1; bev.bar = -1; leadLast = -1; statement = 0; }

static void plan_bar(long bar) {
    bev.n = 0; bev.bar = bar;
    const Sect *S = &arr.s[arr.sec[bar]]; const Roles *R = &S->r;
    int j = arr.j[bar], lastBar = j == S->bars - 1, fill = arr.fill[bar];
    BCh ch[6]; int nch = bar_chords(bar, ch);
    BCh nx[6]; int nnx = bar + 1 < arr.bars ? bar_chords(bar + 1, nx) : 0;
    arr_seed(sng.seed, 1000 + (unsigned)bar);

    // ── THE DUSTY KIT — the song's groove, its role per section, B's variations, the fills ──
    int kicks[16], nk = 0, stop = 16;
    int drums = R->drums;
    if (drums == DR_HATSIN && j < S->bars - 2) drums = DR_NONE;      // the intro's hats come in for its last 2 bars
    if (drums == DR_OUTRO && j >= 2) drums = DR_NONE;
    if (drums != DR_NONE) {
        const Groove *g = drums == DR_BREAK ? &BREAKGRV : &GROOVES[arr.groove];
        int hatsOnly = drums == DR_HATSIN || (R->hatsFirst && j == 0);
        int noKick = hatsOnly || drums == DR_OUTRO || arr.drop[bar];
        int bvar = drums == DR_FULL && S->name == SC_B, openBar = (bvar && j % 2) || fill == FL_OPEN;
        if (fill == FL_STOP) stop = 12;
        for (int s = 0; s < stop; s++) {
            if (!noKick && g->k[s] == 'x') { ev(s, LN_KICK, 0, 34, I_KICK, drums == DR_BREAK ? 4 : s == 0 ? 6 : 5, 120); kicks[nk++] = s; }
            if (!hatsOnly) {
                char sn = g->s[s];
                if (fill == FL_ROLL && s >= 12) { static const int V[4] = { 2, 3, 3, 5 }; ev(s, LN_SNR, 0, 60, I_SNR, V[s - 12], 90); }
                else if (sn == 'x') ev(s, LN_SNR, 0, 60, I_SNR, 5, 120);
                else if (sn == 'g') ev(s, LN_SNR, 0, 60, I_SNR, 2, 60);
                else if (sn == 'r') ev(s, LN_SNR, 0, 72, I_RIM, 5, 30);                  // a rim click, not a thin snare
                else if (bvar && (s == 7 || s == 15) && arnd(100) < 30) ev(s, LN_SNR, 0, 60, I_SNR, 2, 60);   // B's ghost notes
            }
            if (openBar && s == 14) ev(s, LN_HAT, 0, 90, I_HAT, 4, 160);          // an open hat into the next bar
            else if (g->h[s] == 'x' && !(openBar && s == 15)) ev(s, LN_HAT, 0, 90, I_HAT, s % 4 == 0 ? 4 : s % 2 == 0 ? 3 : 2, 26);   // an accent shape, not a flat tick
        }
        if (fill == FL_PICKUP && !noKick) { ev(13, LN_KICK, 0, 34, I_KICK, 4, 90); ev(15, LN_KICK, 0, 34, I_KICK, 4, 90); kicks[nk++] = 13; kicks[nk++] = 15; }
    }

    // ── RHODES — a comp rhythm clipped to the chords, held chords, the PUSH onto the next chord ──
    KHit kh[12]; int nh = 0, pushedInto = bar > 0 && arr.push[bar - 1];
    #define KH(s_, len_, v_, top_, c_) do { if (nh < 12) { KHit h_ = { s_, len_, v_, top_, c_ }; kh[nh++] = h_; } } while (0)
    if (R->keys == KY_HOLD || (R->keys == KY_OUTRO && j < 2)) {
        for (int k = 0; k < nch; k++) KH(ch[k].s, ch[k].len, 4, 0, ch[k]);
    } else if (R->keys == KY_OUTRO && j == 2) {
        KH(0, (S->bars - 2) * 16, 4, 0, ch[0]);
    } else if (R->keys == KY_COMP) {
        const Comp *cp = &COMPS[arr.comp[bar]];
        for (int k = 0; k < nch; k++) {
            int c0 = ch[k].s, c1 = c0 + ch[k].len, own = 0;
            for (int q = 0; q < cp->n; q++) {
                int s = cp->c[q].s; if (s < c0 || s >= c1 || (pushedInto && s == 0)) continue;
                own++; KH(s, cp->c[q].len < c1 - s ? cp->c[q].len : c1 - s, cp->c[q].v, cp->c[q].top, ch[k]);
            }
            if (!own && !(pushedInto && c0 == 0)) KH(c0, c1 - c0, 4, 0, ch[k]);   // a mid-bar change always sounds
        }
        if (arr.push[bar] && nnx) {                                   // anticipate: the next chord on the and-of-4
            int k2 = 0;
            for (int k = 0; k < nh; k++) if (kh[k].s < 14) { if (kh[k].s + kh[k].len > 14) kh[k].len = 14 - kh[k].s; kh[k2++] = kh[k]; }
            nh = k2;
            KH(14, 2 + (nx[0].len < 8 ? nx[0].len : 8), 4, 0, nx[0]);
        }
    }
    #undef KH
    for (int k = 0; k < nh; k++) key_hit(&kh[k]);

    // ── BASS — on the kicks and the changes, walking into the next root; whole notes in the break ──
    int bm = R->bass;
    int on = bm == BA_ON || bm == BA_WHOLE || (bm == BA_LAST && lastBar) || (bm == BA_OUTRO && j < 3);
    if (on) {
        int st[24], ns = 0, approach = 0;
        if (bm == BA_ON) for (int k = 0; k < nk; k++) st[ns++] = kicks[k];
        for (int k = 0; k < nch; k++) st[ns++] = ch[k].s;
        for (int a = 1; a < ns; a++) { int v = st[a], b = a - 1; while (b >= 0 && st[b] > v) { st[b + 1] = st[b]; b--; } st[b + 1] = v; }
        { int k2 = 0; for (int k = 0; k < ns; k++) if ((!k2 || st[k2 - 1] != st[k]) && st[k] < stop) st[k2++] = st[k]; ns = k2; }
        if (bm == BA_ON && nnx && nx[0].off != ch[nch - 1].off && stop == 16 && arnd(100) < 45) {
            int k2 = 0; for (int k = 0; k < ns; k++) if (st[k] < 14) st[k2++] = st[k]; ns = k2; st[ns++] = 14; approach = 1;
        }
        for (int k = 0; k < ns; k++) {
            int s = st[k]; BCh c = chord_at_step(bar, s);
            int isChange = 0; for (int q = 0; q < nch; q++) if (ch[q].s == s) isChange = 1;
            int root = bass_peek(root_pc((Ch){ c.off, c.q }), 31, 45), m = root;
            bassLast = root;
            if (approach && s == 14) {
                int tgt = bass_peek(root_pc((Ch){ nx[0].off, nx[0].q }), 31, 45);
                m = arnd(100) < 60 ? tgt - 1 : tgt + 1;                   // a half-step into the next root
            } else if (!isChange && s % 8 != 0 && arnd(100) < 25) {
                m = arnd(100) < 60 ? (root + 7 <= 50 ? root + 7 : root - 5) : (root + 12 <= 52 ? root + 12 : root);
            }
            int nxs = k + 1 < ns ? st[k + 1] : stop;
            int dur = (int)((nxs - s) * stepMs * (bm == BA_WHOLE ? 0.95 : arr.legato ? 0.9 : 0.55));
            ev(s, LN_BASS, 0, m, I_BASS, s == 0 ? 6 : 5, dur);
        }
    }

    // ── THE LEAD — the dab cell grown into a phrase per four bars ──
    int lead = R->lead || (S->name == SC_A && S->bars >= 16 && j >= 8);   // a long first A lets the tune in halfway
    if (lead && nLadder) {
        long gbar = bar - j % 4;
        if (phr.gbar != gbar) build_phrase(gbar);
        int bj = j % 4;
        for (int k = 0; k < phr.n; k++) if (phr.s[k] / 16 == bj)
            ev(phr.s[k] % 16, LN_LEAD, 0, ladder[phr.li[k]], I_DAB, phr.v[k] + 3 > 7 ? 7 : phr.v[k] + 3, (int)(phr.d[k] * stepMs * 0.9));
    }
}

// ── the step player — books the planned bar; THE DRUNK POCKET lives in the booking offsets ──
static double toneTgt = 8000, toneTau = 0.3, toneHz = 8000;   // the master tone the plan rides (around snd.tone)
static double hum(double sigma) { return ((rnd(1001) + rnd(1001) + rnd(1001)) / 1000.0 - 1.5) * 2 * sigma; }
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    (void)pos;
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= arr.bars) return;                              // the song has ended (update rolls the next)
    const Sect *S = &arr.s[arr.sec[bar]];
    int  j = arr.j[bar], lastBar = j == S->bars - 1;
    if (bev.bar != bar) plan_bar(bar);

    // ── the master tone, ridden by the plan ──
    if (step == 0) {
        if (S->name == SC_INTRO) { toneTgt = 900 * pow(snd.tone / 900.0, (j + 1) / (double)S->bars); toneTau = 240.0 / tempo / 3; }
        else if (S->name == SC_BREAK && j == 0) { toneTgt = 1800; toneTau = 0.4; }
        else if (S->name == SC_OUTRO && j == S->bars - 2) { toneTgt = 700; toneTau = 240.0 / tempo * 0.6; }
        else if (j == 0 && S->name != SC_OUTRO) { toneTgt = snd.tone; toneTau = arr.s[arr.sec[bar] > 0 ? arr.sec[bar] - 1 : 0].name == SC_BREAK ? 0.8 : 0.3; }
        else if (j % 4 == 0 && (S->name == SC_A || S->name == SC_B)) { toneTgt = snd.tone * (0.88 + rnd(25) * 0.01); toneTau = 240.0 / tempo; }   // a slow drift
        if (S->r.dipLast && lastBar) { toneTgt = 500; toneTau = 0.25; }
    }

    // the pocket: drag amounts in ms, scaled by the knob (live: applied as the bar is booked)
    float pk = POCKETV[pocketSel];
    int swing  = (step % 2) ? (int)(pk * stepMs * 0.30f) : 0;                              // every offbeat swung
    // Only the BACKBEAT drags (loficity's snareLag, 8-18 ms). The tune stays near the grid: dragging the keys
    // + lead (+34 / +37 ms before, vs +21 / +4 there) made the whole song sound sluggish rather than laid back.
    int drag[6] = {
        (int)(pk * stepMs * 0.02f),                                                        // kick, a hair lazy
        (int)(pk * stepMs * 0.16f) + (pk > 0 ? rnd((int)(pk * 8) + 1) : 0),                // snare LATE
        rnd(2),                                                                            // hats on the swing
        (int)(pk * stepMs * 0.03f),                                                        // keys, near the grid
        3,                                                                                 // bass, 3 ms (theirs)
        (int)(pk * stepMs * 0.02f),                                                        // the lead, near the grid
    };
    static const double SIG[6] = { 1.5, 3, 2.5, 4, 4, 8 };                                 // humanize, ms per part (theirs)
    static const float VU[6] = { 1.0f, 0.9f, 0.1f, 0.35f, 0.8f, 0.7f };
    for (int i = 0; i < bev.n; i++) {
        const BEv *e = &bev.e[i];
        if (e->step != step) continue;
        if (e->lane == LN_LEAD && band.c[chDab].sel == 3) continue;   // the lead chair switched off
        int off = drag[e->lane] + swing + e->off + (int)hum(SIG[e->lane]);
        double t = rad_step_time(&clk, abs) + off * 0.001;
        if (e->lane == LN_KICK) fire_kick(t, e->vol);                  // the kit's layered voices
        else if (e->lane == LN_SNR && e->instr == I_SNR) fire_snare(t, e->vol);
        else if (e->lane == LN_HAT) fire_hat(t, e->vol, e->dur >= 100);
        else schedule_at(t, e->midi, e->instr, e->vol, e->dur);
        vu += VU[e->lane];
    }
}

// ── the lo-fi rack — SET-AND-HOLD (per song/mood) ────────────────────────────
// loficity's bus: a big soft room, a clean tape (wow only, sat OFF, flutter 0.03), the echo on the lead's
// send, a gentle glue and the +1.5/+2.5 dB makeup their tape stage adds. No master chorus.
#define LOFI_FLUTTER 0.03f
static void apply_fx(void) {
    const MoodDef *m = &MOOD[sng.mood];
    reverb(m->rev, 0.55f);
    tape(m->wow, LOFI_FLUTTER, m->sat);
    echo(m->echoMs, m->echoFb, 0.3f);
    chorus(1.0f, 0.3f, 0.0f);
    glue(0, 0.25f, 8, 160);
    eq(1.5f, 2.5f, 0.0f);
}

// ── one-time setup ────────────────────────────────────────────────────────
static void inst(int s, int wave, int a, int d, int sus, int r, float h, float t, float m) {
    instrument(s, wave, a, d, sus, r); instrument_harmonics(s, h); instrument_timbre(s, t); instrument_morph(s, m);
}
static void voice_keys(int sel) {             // the Rhodes (or a Wurli) on the comp + the held slot
    for (int s = I_EP; s <= I_EPL; s += I_EPL - I_EP) {
        if (sel == 0) inst(s, INSTR_EPIANO, 2, 0, 7, s == I_EP ? 320 : 2600, 0.10f, 0.36f, 0.18f);
        else          inst(s, INSTR_EPIANO, 2, 0, 7, s == I_EP ? 280 : 2200, 0.50f, 0.42f, 0.22f);
        instrument_reverb(s, 0.30f); instrument_level(s, 0.60f); instrument_pan(s, -0.08f);
    }
}
static void voice_bass(int sel) {
    if (sel == 0) {                            // the pizzicato upright: a plucked bowed-string model
        inst(I_BASS, INSTR_BOWED, 3, 0, 7, 90, 0.62f, 0.30f, 0.45f);
        instrument_mode(I_BASS, MODE_BOW_PIZZ, 1.0f); instrument_mode(I_BASS, MODE_BOW_BODY, 0.85f);
        instrument_mode(I_BASS, MODE_BOW_SIZE, BOW_SIZE_BASS);
        instrument_filter(I_BASS, FILTER_LOW, 950, 0);
    } else {                                   // round: a sine with a soft release
        inst(I_BASS, INSTR_SINE, 4, 300, 5, 200, 0.5f, 0.5f, 0.5f);
        for (int m = 0; m < 7; m++) instrument_mode(I_BASS, m, 0);
        instrument_filter(I_BASS, FILTER_LOW, 520, 0);
    }
    instrument_level(I_BASS, 1.0f); instrument_eq(I_BASS, 2.0f, 2.0f, 2.0f);   // measured: 4 dB under loficity's balance
}
static void voice_lead(int sel) {
    instrument_lfo(I_DAB, 0, LFO_PITCH, 5.0f, 0.0f); instrument_glide(I_DAB, 0); instrument_echo(I_DAB, 0.0f);
    if (sel == 0) {                            // vibes
        inst(I_DAB, INSTR_MALLET, 1, 0, 7, 1200, 0.22f, 0.45f, 0.85f); instrument_filter(I_DAB, FILTER_LOW, 3200, 0);
    } else if (sel == 1) {                     // a breathy flute with a slow vibrato
        inst(I_DAB, INSTR_PIPE, 14, 0, 5, 220, 0.0f, 0.34f, 0.68f); instrument_filter(I_DAB, FILTER_LOW, 6000, 0);
        instrument_lfo(I_DAB, 0, LFO_PITCH, 5.0f, 0.10f); instrument_glide(I_DAB, 13);
    } else if (sel == 2) {                     // the old muted horn, filtered + echoed
        inst(I_DAB, INSTR_REED, 2, 0, 4, 900, 0.78f, 0.28f, 0.5f); instrument_filter(I_DAB, FILTER_LOW, 2000, 1);
        instrument_echo(I_DAB, 0.16f);
    }
    instrument_reverb(I_DAB, 0.35f); instrument_pan(I_DAB, 0.16f); instrument_level(I_DAB, 0.8f);
}
// the kit: loficity's jazzhop voicing of the morphing drum bank (its "balanced" kit roll, mid-range values)
static MorphKit kit;
static void voice_kit(void) {
    const double F0 = 160, F1 = 51, kd = 0.13, sd = 0.065, hp = 7250, hc = 0.016;
    float *k = kit.p[MD_KICK];
    k[MD_CHAR] = 0.2f; k[MD_LEVEL] = 1;
    k[MD_TUNE]  = (float)fmin(1, fmax(0, (69 + 12 * log2(F1 / 440.0) - 19) / 33.0));
    k[MD_PUNCH] = (float)fmin(1, 12 * log2(F0 / F1) / 48.0);
    k[MD_SNAP]  = (90 - 8) / 142.0f; k[MD_DECAY] = (float)((kd * 4000 - 40) / 1060.0);
    k[MD_CUT] = 0.36f; k[MD_CLICK] = 0.22f; k[MD_SUB] = 0.22f; k[MD_DRIVE] = 0.18f;
    float *n = kit.p[MD_SNARE];
    n[MD_CHAR] = 0.5f; n[MD_LEVEL] = 1; n[MD_TUNE] = 0.32f; n[MD_DECAY] = 0.45f; n[MD_PUNCH] = 0.25f;
    n[MD_SNAP] = 0.8f; n[MD_TONE] = 0.62f; n[MD_CUT] = 0.45f; n[MD_DRIVE] = 0.1f; n[MD_ODEC] = (float)((sd * 4000 - 30) / 390.0);
    float *h = kit.p[MD_HAT];
    h[MD_CHAR] = 0.2f; h[MD_LEVEL] = 1; h[MD_TUNE] = 0.53f; h[MD_TONE] = 0.25f; h[MD_SUB] = 0.6f; h[MD_RES] = 0.0f;
    h[MD_CUT] = (float)(log2(hp / 3000.0) / 2.0); h[MD_DECAY] = (float)((hc * 2500 - 10) / 210.0);
    h[MD_ODEC] = (float)((0.12 * 4000 - 80) / 720.0);
    morph_ride(&kit);
    for (int s = MDS_KICK; s <= MDS_KICKS; s++) instrument_level(KIT_BASE + s, 0.55f);
    instrument_level(KIT_BASE + MDS_SNB, 0.8f); instrument_level(KIT_BASE + MDS_SNN, 0.45f);
    for (int s = MDS_HC; s <= MDS_HO; s++) { instrument_level(KIT_BASE + s, 0.55f); instrument_pan(KIT_BASE + s, 0.2f); instrument_reverb(KIT_BASE + s, 0.08f); }
    instrument_reverb(KIT_BASE + MDS_SNB, 0.25f); instrument_reverb(KIT_BASE + MDS_SNN, 0.25f);
    inst(I_RIM, INSTR_MODAL, 0, 0, 7, 30, 0.55f, 0.70f, 0.04f);
    instrument_level(I_RIM, 0.28f); instrument_filter(I_RIM, FILTER_HIGH, 300, 0); instrument_reverb(I_RIM, 0.30f); instrument_pan(I_RIM, -0.15f);
}
// the kit's layered voices at a planned volume (morph_fire only takes a coarse boost), on the sample clock
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

static void setup_instruments(void) {
    chKeys = rad_chair(&band, "keys", "rhodes", "wurli", NULL, NULL);
    chBass = rad_chair(&band, "bass", "upright", "round", NULL, NULL);
    chDab  = rad_chair(&band, "lead", "vibes", "flute", "horn", "off");
    morph_build(&kit, KIT_BASE);
    voice_kit(); voice_keys(0); voice_bass(0); voice_lead(0);
    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);
}

static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chKeys) { voice_keys(sel); voice_song(); }
    else if (idx == chBass) voice_bass(sel);
    else if (idx == chDab && sel < 3) voice_lead(sel);          // sel 3 = off, gated at play time
}

// per SONG, from a derived stream (the seed rule): the Rhodes' tone + its suitcase tremolo + autopan, and the
// master tone the plan rides. loficity's balanced jazzhop ranges: ep lp 2400-4500, trem 3.5-5.5 Hz at
// 0.06-0.18 depth (scaled 0.5: at full depth it reads as a wobble on the whole mix), tone 5500-9000.
static void voice_song(void) {
    arr_seed(sng.seed, 7);
    snd.epLp = 2400 + arnd(2101); snd.tremRate = 3.5f + arnd(201) * 0.01f; snd.tremDepth = 0.06f + arnd(121) * 0.001f;
    snd.tone = 5500 + arnd(3501);
    apply_tone();
    for (int s = I_EP; s <= I_EPL; s += I_EPL - I_EP) {
        instrument_lfo(s, 0, LFO_VOLUME, snd.tremRate, snd.tremDepth * 0.5f);
        instrument_lfo(s, 1, LFO_PAN, snd.tremRate, 0.25f * 0.5f);
    }
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    for (int s = I_EP; s <= I_EPL; s += I_EPL - I_EP) instrument_filter(s, FILTER_LOW, (int)(snd.epLp * tm), 0);
    if (band.c[chDab].sel == 0) instrument_filter(I_DAB, FILTER_LOW, (int)(3200 * tm), 0);
}

// ── update ──────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    // the step grid runs on the SAMPLE clock (radio.h rad_audio_*): every hit lands on its sample
    double pos = rad_audio_pos(&clk, tempo);

    if (!booted) {
        setup_instruments();
        if (LOFI_SEED) { new_song(pos, LOFI_SEED); rad_hist_log(&rs); } else fresh_song(pos);
        scheduled = (long)pos; apply_tone();
        booted = true;   // (no continuous noise bed — it read as hard hiss; tape sat + the wobble carry the lo-fi)
    }

    int ev = rad_input(&tempo, 64, 104, 2, &pocketSel, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  { if (!radioOn) { note_off_all(); sfx(-1); vinylH = -1; }
                              else { scheduled = (long)pos; apply_fx(); } }
    if (ev & RAD_EV_TONE)   apply_tone();

    int chair = rad_band_input(&band, &showHelp);
    if (chair >= 0) apply_chair(chair);

    if (radioOn) {
        long st; while (rad_audio_step(&clk, &st)) play_step(st, pos);
        if (scheduled - songBase >= (long)arr.bars * 16 + 8) {      // the song ENDED: the next moves to a related key
            int prevKey = sng.keyPc;
            fresh_song(pos);
            arr_seed(sng.seed, 2);
            int u = arnd(100);
            if (u < 35) sng.keyPc = (prevKey + 5) % 12;               // up a fourth
            else if (u < 60) sng.keyPc = (prevKey + 7) % 12;          // up a fifth
            else if (u < 75) sng.keyPc = prevKey;                     // stay home
            else sng.keyPc = (prevKey + 9) % 12;                      // to the relative minor's home
            build_ladder();                                           // the lead follows the key
        }
        {   // the chord row: the progression of the section being played (A's loop, B's, a turnaround)
            long sb = scheduled - songBase; long tb = sb >= 0 ? sb / 16 : 0; if (tb >= arr.bars) tb = arr.bars - 1;
            const Sect *S = &arr.s[arr.sec[tb]]; const Prog *p = sect_prog(arr.sec[tb]);
            int b0 = (arr.j[tb] * 4) % p->beats, cur = 0;
            for (int i = 0, at = 0; i < p->n; at += p->c[i].beats, i++) if (b0 >= at) cur = i;
            nowN = p->n < 4 ? p->n : 4; nowIdx = cur < 3 ? cur : 3;
            for (int i = 0; i < nowN; i++) chord_label(nowChord[i], 8, (Ch){ p->c[i].off, p->c[i].q });
            if (cur >= 4) chord_label(nowChord[3], 8, (Ch){ p->c[cur].off, p->c[cur].q });
            if (S->name == SC_OUTRO && arr.j[tb] >= 2) { nowN = 1; nowIdx = 0; chord_label(nowChord[0], 8, (Ch){ 0, Q_MAJ9 }); }
        }
        platter += dt() * (tempo / 60.0f) * 1.2f;       // the turntable spins with the tempo
    }
    // (cassette wobble now lives in tape()'s wow/flutter — no varispeed: holding it off-speed
    //  keeps the resample ring engaged and drifts the timing; tape's wow is built for this.)
    vu *= 0.90f; if (vu > 12) vu = 12;
    {   // the master tone the plan rides (filter() is built to be ridden; first-order, per frame)
        static int lastHz = -1;
        toneHz += (toneTgt - toneHz) * (1 - exp(-dt() / fmax(0.005, toneTau)));
        int hz = (int)toneHz; if (radioOn && abs(hz - lastHz) > 2) { filter(FILTER_LOW, (float)hz, 0.05f); lastHz = hz; }
    }

#ifdef DE_TRACE
    long ss = scheduled - songBase; long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("mood", "%s", MOOD[sng.mood].name);
    watch("key", "%s", RAD_PCNAME[sng.keyPc]);
    if (tbar >= arr.bars) tbar = arr.bars - 1;
    watch("chord", "%s", nowChord[nowIdx]);
    watch("comp", "%d", arr.comp[tbar]);
    watch("section", "%s", SC_NAME[arr.s[arr.sec[tbar]].name]);
    watch("fill", "%s", FL_NAME[arr.fill[tbar]]);
    watch("pocket", "%s", POCKETN[pocketSel]);
    watch("tempo", "%d", tempo);
#endif
}

// ── draw — the cozy lo-fi scene: rainy window, lamp, a spinning turntable ────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  ACC = CLR_PEACH;
    float t = timer();

    rad_body(CLR_DARK_BROWN, ACC);
    rad_dial(sng.freq, ACC);

    // the window — a dim warm room at night
    int wx = 34, wy = 52, ww = 102, wh = 116;
    rectfill(wx, wy, ww, wh, CLR_BROWNISH_BLACK);
    // the rainy window (upper-left): a pane with running droplets
    rectfill(wx + 6, wy + 6, 44, 40, CLR_DARKER_BLUE);
    rect(wx + 6, wy + 6, 44, 40, CLR_DARK_BROWN);
    if (radioOn) for (int i = 0; i < 10; i++) {
        int rx = wx + 9 + (i * 13) % 40;
        int ry = wy + 8 + (int)(fmodf(t * (18 + i * 3) + i * 11, 36.0f));
        line(rx, ry, rx, ry + 3, CLR_BLUE);
    }
    // the desk lamp glow (upper-right), amber, gently breathing
    int lx = wx + ww - 22, ly = wy + 16;
    float breathe = 0.6f + 0.4f * sinf(t * 0.7f);
    for (int r = 14; r > 0; r -= 3) circ(lx, ly, r, r < 8 ? CLR_LIGHT_YELLOW : CLR_DARK_ORANGE);
    circfill(lx, ly, 3, radioOn && breathe > 0.5f ? CLR_LIGHT_YELLOW : CLR_ORANGE);
    rectfill(lx - 1, ly, 2, 26, CLR_DARK_BROWN);                      // the stem
    // the turntable on the desk — a platter that spins with the tempo, a record + tonearm
    int tx = wx + 34, ty = wy + 82;
    circfill(tx, ty, 22, CLR_DARKER_GREY);                           // platter
    circfill(tx, ty, 18, CLR_BLACK);                                 // the vinyl
    circ(tx, ty, 12, CLR_DARK_GREY); circ(tx, ty, 7, CLR_DARK_GREY); // grooves
    circfill(tx, ty, 3, ACC);                                        // the label
    {   // a spot on the record so you see it spin
        float a = platter;
        pset(tx + (int)(cosf(a) * 14), ty + (int)(sinf(a) * 14), CLR_LIGHT_GREY);
        pset(tx + (int)(cosf(a) * 9),  ty + (int)(sinf(a) * 9),  CLR_MEDIUM_GREY);
    }
    line(tx + 20, ty - 18, tx + 6, ty - 4, CLR_LIGHT_GREY);          // the tonearm
    rect(wx, wy, ww, wh, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, ACC);
    if (radioOn) {
        print(sng.title, 154, 58, ACC);
        char l2[32]; snprintf(l2, 32, "%s  %s", MOOD[sng.mood].name, RAD_PCNAME[sng.keyPc]);
        font(FONT_SMALL); print(l2, 154, 70, CLR_LIGHT_PEACH); font(FONT_NORMAL);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 81, CLR_DARK_ORANGE);
        float vt = vu / 12.0f; rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, ACC);
    } else print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        long bb = bar < arr.bars ? bar : arr.bars - 1;
        int ci = nowIdx, x = 152;
        for (int i = 0; i < nowN; i++) {
            int cw = text_width(nowChord[i]); if (x + cw > 292) break;
            if (i == ci) { rectfill(x - 2, 104, cw + 4, 12, ACC); print(nowChord[i], x, 106, CLR_BLACK); }
            else print(nowChord[i], x, 106, CLR_DARK_ORANGE);
            x += cw + 8;
        }
        // the FORM strip: every section sized by its bars, the playhead crawling through (the plan made visible)
        int fx = 152, fw = 136, fy = 121, acc = 0;
        for (int i = 0; i < arr.n; i++) {
            int x0 = fx + acc * fw / arr.bars, x1 = fx + (acc + arr.s[i].bars) * fw / arr.bars;
            static const int SCOL[5] = { CLR_DARK_BLUE, CLR_DARK_ORANGE, CLR_BROWN, CLR_DARK_PURPLE, CLR_DARKER_BLUE };
            rectfill(x0, fy, x1 - x0 - 1, 5, arr.sec[bb] == i ? ACC : SCOL[arr.s[i].name]);
            acc += arr.s[i].bars;
        }
        int ph = fx + (int)(bb * fw / arr.bars); line(ph, fy - 2, ph, fy + 6, CLR_WHITE);
        font(FONT_SMALL);
        print(str("%s %d/%d%s", SC_NAME[arr.s[arr.sec[bb]].name], arr.j[bb] + 1, arr.s[arr.sec[bb]].bars, arr.fill[bb] ? str("  fill: %s", FL_NAME[arr.fill[bb]]) : arr.push[bb] ? "  push" : ""), fx, fy + 8, CLR_DARK_ORANGE);
        font(FONT_NORMAL);
    }

    rad_knob_sel(&pocketSel, 4, 168, 148, 9, POCKETN[pocketSel], ACC);
    if (rad_knob_int(&tempo, 64, 104, 2, 218, 148, 9, "tempo", ACC)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], ACC)) apply_tone();
    rad_power_led(radioOn, ACC, CLR_DARK_BROWN);

    rad_help_button(ACC);
    rad_band_button(ACC);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tune (rolls a new mood)" },
            { "R",          "replay this one" },
            { "[ / ]",      "back / forward history" },
            { "LEFT/RIGHT", "POCKET - tight..drunk (the drag)" },
            { "UP/DOWN",    "tempo" },
            { "T",          "tone" },
            { "M",          "radio on / off" },
            { "B",          "band - keys/bass/dab" },
        };
        static const char *NOTES[3] = {
            "lush Rhodes jazz over a dusty SWUNG kit, drenched",
            "in tape + vinyl crackle. the POCKET knob drags the",
            "snare late / the kick lazy - tight to drunk. moods roll.",
        };
        rad_help_panel("LOFI FM", HELP, 8, NOTES, 3, ACC);
    }
    rad_band_panel(&band, ACC);
    ui_end();
}
