/* de:meta
{
  "slug": "yachtrack",
  "title": "session desk",
  "status": "active",
  "created": "2026-07-02",
  "kind": [
    "instrument",
    "generative"
  ],
  "teaches": [
    "chord-voicing",
    "song-arrangement",
    "swing-timing",
    "step-sequencer"
  ],
  "lineage": "The yacht radio (yacht.c) turned inside-out — the first CHORD-FIRST tinyjam rack (docs/design/yacht-rack.md) and the first radio-to-rack seed handoff: its generator is yacht's new_song ported verbatim (same srnd draw sequence, so an 8-hex code from the radio dial reproduces the exact song here, opened up). Chart+interpreters model: the chord row / form row / drum skeleton / sax cell are editable truth; bass runs, comp anticipation, ghosts and swing stay session players behind feel knobs. Accordion shell from acidrack.c.",
  "homage": "Steely Dan / Hall & Oates (yacht rock); the LA session desk",
  "todo": [
    "UX DIRECTNESS PASS (maker, 2026-07-02, after first play): the desk feels a tad too INDIRECT — a lot of knobs/levers sit between the finger and the sound. They exist to support the radio's player model (the chart+interpreters split is right), but the FEEL isn't pleasant yet; maker is playing more and thinking about a more direct surface. Revisit before copying this UX pattern to rack #3. Design-side notes: yacht-rack.md open questions",
    "chord-cell root nudge is two small arrow zones — a vertical drag on the cell would feel better on phones",
    "per-section chord loops (verse vs chorus changes) — v2: a form slot gains a loop index (design doc open question)",
    "naming pass: 'session desk' is the working faceplate; cart name yachtrack parallels acidrack (maker call)",
    "device-adaptive redesign (Phase 3 — design/device-adaptive-layout.md): CHORD CHART stays primary; drums/hook/band collapse per finger-budget (accordion/tabs) on phone, more inline on iPad; transport + song-code pinned. Spec the section arrangements when picked up"
  ],
  "description": {
    "summary": "The yacht radio opened up on the producer's desk: the CHORD CHART is the star. Type the radio's 8-hex song code and the exact tune lands as an editable chart - tap the mu chords, reorder the form, edit the drum skeleton and the sax hook - while the session band keeps playing it like players, not a sequencer.",
    "detail": "The first chord-first tinyjam rack. The truth is a CHART, not a piano roll: a 4-slot chord row in the yacht idiom (mu / maj7 / m9 / 13 / 9sus - tap to reharmonize while it plays, arrows nudge the root, MU-IFY and SUS-MELT run the radio's own flavor rolls), an 8-slot FORM row (intro/verse/chorus/bridge/outro, tap to rearrange; GEAR arms the +2 truck-driver lift on the last chorus), a 5-lane drum SKELETON grid, and the sax hook as a 32-step cell lane. Everything else stays a PLAYER: the bass walks chromatic approach runs INTO whatever chord you just tapped in (RUNS knob), the FM epiano voice-leads its mu voicings and pushes the and-of-4 anticipation (ANTIC), ghost snares and the Purdie swing live on knobs (GHOST/SWING), the sax picks its own chord tones through your cell rhythm, a little behind the beat (LAID). Three DRUMMER chairs (session 16ths / Purdie half-time shuffle / CR-78 circuits) swap the kit voices and feel defaults; RECHART rewrites the skeleton in that drummer's hand. THE BAND rows swap the epiano/bass/lead/pad chairs live, same recipes as the radio. The 8-hex SONG CODE on the transport is the radio handoff: the generator is yacht.c's new_song verbatim, so a code seen on the yacht radio dial reproduces that exact song here, opened up for editing. Autosaves; WAV export via the engine's live capture.",
    "controls": "SPACE play/stop · SONG/LOOP toggles the form vs looping the chord row · UP/DOWN tempo · chord row: tap a cell to cycle its quality, tap the cell's arrows to nudge the root, KEY cycles the key · MU-IFY/SUS-MELT melt chords like the generator does · form row: tap a slot to cycle the section, GEAR arms the last-chorus lift · DRUMS: tap cells (off/on/accent), drummer buttons swap the kit's hands, RECHART rewrites the skeleton, GHOST/SWING/FILL knobs set the feel · HOOK: tap the 32-step sax cell, REG moves the register, LAID drags it behind the beat · BAND: tap a candidate to swap a chair, MUTE per player · DESK: NEW rolls a song code (also G), [ ] history, tap code digits to nudge, WAV exports, TONE/FEEL as on the radio, RUNS/ANTIC/STAB set the players' feel"
  }
}
de:meta */
// ── SESSION DESK (yachtrack) ──────────────────────────────────────────────
// The yacht radio turned inside-out — the first CHORD-FIRST tinyjam rack and
// the first radio→rack seed handoff (docs/design/yacht-rack.md). The model:
//
//   the generator writes the CHART; the PLAYERS read it;
//   the KNOBS set the feel; the SEED rolls defaults for all of it.
//
// CHART (editable truth, ~90 bytes, autosaved): chord row loop[4] in the
// five-quality mu vocabulary · form[8] sections · drum[5][16] skeleton ·
// saxOn[32] hook cell · feel knobs · chairs. PLAYERS (playback interpreters,
// ported from yacht.c's play_step): bass approach runs, epiano voice-led
// comping + the and-of-4 anticipation, strat 9th stabs, sax chord-tone
// melody, chorus pad — they read the chord row live, so tapping a chord
// re-aims the whole band mid-groove.
//
// THE SEED-COMPAT LAW (yacht-rack.md §3): new_song() below is yacht.c's,
// VERBATIM — the srnd draw sequence must never change (the timbre rolls and
// title words consume draws too). Chart derivation consumes ZERO srnd draws.
// Acceptance: song_sum() golden pairs harvested from the radio's trace live
// in spec() — node tools/spec.js yachtrack.
//
// Shell invariants inherited from acidrack.c: sound never depends on which
// strip is expanded (the accordion is pure view), and folded ≠ blind (mini
// playheads on every header). The shell is copied as an IDIOM, deliberately
// not extracted to a rack.h at customer #2 — the honest overlap with the
// cell-grid rack is ~80 lines of chrome, not code; the third rack decides
// (the judgment call is recorded in yacht-rack.md §5 / tinyjam-racks.md).

#include "studio.h"
#include "radio.h"     // opt-in toolkit: seed stream + clock + voice leading + chairs (not the chassis)
#include "spec.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

// ── instrument slots (yacht.c's plan, verbatim) ───────────────────────────
#define I_EP    5    // FM electric piano — the band's center (1:1 detent, tine on)
#define I_BASS  6    // fingered electric bass
#define I_GTR   7    // clean strat stabs
#define I_SAX   8    // the lead chair (sax / synth / guitar solo)
#define I_PAD   9    // strings / syn-brass under the chorus
#define SL_KICK 10
#define SL_SN   11
#define SL_HATC 12
#define SL_HATO 13
#define SL_RIDE 14

// ── harmony — the mu chord + the codified changes (yacht.c, verbatim) ─────
enum { Q_MU, Q_MAJ7, Q_MIN9, Q_DOM13, Q_SUS9, NQ };
static const char *QN[NQ] = { "mu", "maj7", "m9", "13", "9sus" };
static const int QV[NQ][3] = {
    { 4,  7, 14 },   // mu    — 3-5-9, the Steely voicing (no 7th!)
    { 4, 11, 14 },   // maj7  — with the 9, session lush
    { 3, 10, 14 },   // m9
    { 4, 10, 21 },   // 13    — 3-b7-13, the dominant that smiles
    { 5, 10, 14 },   // 9sus  — the V that never commits
};

typedef struct { int off, q; } Ch;
static const Ch TMPL_MAJ[4][4] = {
    { {0,Q_MU},   {10,Q_MAJ7}, {5,Q_MAJ7}, {7,Q_SUS9} },   // "reeling": Imu bVII IV V9sus
    { {2,Q_MIN9}, {7,Q_DOM13}, {0,Q_MU},   {9,Q_MIN9} },   // "babylon": ii V13 Imu vi
    { {0,Q_MAJ7}, {3,Q_MAJ7},  {8,Q_MAJ7}, {1,Q_MAJ7} },   // "deacon": maj7 PLANING
    { {0,Q_MU},   {5,Q_MU},    {0,Q_MU},   {5,Q_MU} },     // "nineteen": the two-mu vamp
};
static const Ch TMPL_MIN[2][4] = {
    { {0,Q_MIN9}, {5,Q_DOM13}, {0,Q_MIN9}, {8,Q_MAJ7} },   // "royal": dorian i IV13 i bVI
    { {0,Q_MIN9}, {10,Q_MAJ7}, {8,Q_MAJ7}, {7,Q_SUS9} },   // "gaucho": i bVII bVI V9sus
};

enum { S_INTRO, S_VERSE, S_CHORUS, S_BRIDGE, S_OUTRO, NSECT };
static const char *SECTNAME[NSECT] = { "intro", "verse", "chorus", "bridge", "outro" };
static const char SECTCH[NSECT]    = { 'I', 'V', 'C', 'B', 'O' };
static const int FORM_DEFAULT[8] = { S_INTRO, S_VERSE, S_CHORUS, S_VERSE,
                                     S_CHORUS, S_BRIDGE, S_CHORUS, S_OUTRO };

enum { G_SESSION, G_PURDIE, G_CR78, NGROOVE };
static const char *GROOVENAME[NGROOVE] = { "session", "purdie", "cr-78" };

// ── the generated song (yacht.c's struct, verbatim — the generation record;
// the CHART below is derived from it and owns playback) ───────────────────
typedef struct {
    int  keyPc, minorKey;
    Ch   loop[4];
    int  groove;
    int  gearUp;
    int  melOn[6], melN;
    int  melReg;
    int  hasRide;
    char title[24];
    float freq;
    unsigned seed;
} Song;
static Song      sng;
static RadioSeed rs;
static RadioClock clk = { -1, 0, 142.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)

// ── THE CHART — the editable truth (yacht-rack.md §2) ─────────────────────
#define STEPS  16
#define HOOKN  32
#define NFORM  8
enum { L_KICK, L_SN, L_HATC, L_HATO, L_RIDE, NLANE };
static const char *LANENAME[NLANE] = { "kick", "snar", "hatc", "hato", "ride" };
enum { FEEL_GHOST, FEEL_SWING, FEEL_FILL, FEEL_RUNS, FEEL_ANTIC, FEEL_STAB, FEEL_LAID, NFEEL };

typedef struct {
    int  keyPc;
    Ch   loop[4];                       // the chord row — the star
    int  form[NFORM];                   // section per 8-bar slot
    int  gearUp;                        // +2 on the LAST chorus slot
    unsigned char drum[NLANE][STEPS];   // 0 off · 1 hit · 2 accent
    unsigned char saxOn[HOOKN];         // the hook cell
    int  melReg;
    int  drummer;                       // G_SESSION / G_PURDIE / G_CR78
    float feel[NFEEL];
    char title[24];
} Chart;
static Chart cht;

static int   tempo     = 104;
static int   intensity = 1;            // the radio's feel curve (dock..open sea)
static int   toneSel   = 2;
static bool  running   = false;
static bool  songmode  = true;         // form playback (else: loop the chord row, full band)
static int   playhead  = 0;            // 0..15 within the bar
static long  playbar   = 0;            // absolute bar within the 64-bar form
static float vu        = 0;
static int   bassLast  = 33;
static int   melLast   = 70;
static int   gvEp[3]   = { 62, 66, 69 };
static int   gvPad[3]  = { 55, 59, 62 };
static bool  epInit = false, padInit = false;

static RadBand band;
static int chEp, chBass, chLead, chPad;
static bool pmute[4];                  // per-chair mutes (ep, bass, lead, pad rows)
static bool kitmute = false, saxmute = false;

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);
static void apply_kit(int drummer);
static void mark_dirty(void);

// ── the acceptance-corpus hash — MUST match yacht.c's song_sum() ──────────
static unsigned song_sum(void) {
    unsigned h = 2166136261u;
    #define F(v) h = (h ^ (unsigned)(v)) * 16777619u
    F(sng.keyPc); F(sng.minorKey);
    for (int i = 0; i < 4; i++) { F(sng.loop[i].off); F(sng.loop[i].q); }
    F(sng.groove); F(sng.gearUp); F(sng.hasRide);
    F(sng.melN); for (int i = 0; i < sng.melN; i++) F(sng.melOn[i]);
    F(sng.melReg);
    for (const char *p = sng.title; *p; p++) F(*p);
    F((int)(sng.freq * 10 + 0.5f)); F(tempo);
    #undef F
    return h;
}

// ── song generation — yacht.c's new_song, VERBATIM srnd sequence ──────────
// (the seed-compat law: every draw below, in this order, including the band
// timbre rolls and the title words. Do not "improve" anything here.)
static const char *TW1[] = { "Marina", "Pacific", "Chrome", "Midnight", "Coastal",
    "Royal", "Century", "Harbor", "Velvet", "Crystal", "Golden", "Babylon" };
static const char *TW2[] = { "Standard", "Shuffle", "Avenue", "Mirage", "Lights",
    "Charade", "Tan", "Pier", "Nineteen", "Reflection", "Operator", "Cove" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.keyPc    = srnd(12);
    sng.minorKey = srnd(100) < 30;                  // the marina leans major
    {
        const Ch *t = sng.minorKey ? TMPL_MIN[srnd(2)] : TMPL_MAJ[srnd(4)];
        for (int i = 0; i < 4; i++) sng.loop[i] = t[i];
    }
    if (!sng.minorKey && srnd(100) < 35)            // the MU-IFY roll: melt a maj7
        for (int i = 0; i < 4; i++)                 // into the Steely chord
            if (sng.loop[i].q == Q_MAJ7 && sng.loop[i].off != 1)
                { sng.loop[i].q = Q_MU; break; }
    if (srnd(100) < 30)                             // melt the V into a 9sus
        for (int i = 1; i < 4; i++)
            if (sng.loop[i].q == Q_DOM13)
                { sng.loop[i].q = Q_SUS9; break; }

    sng.groove = srnd(100) < 25 ? G_CR78 : (srnd(100) < 45 ? G_PURDIE : G_SESSION);
    sng.gearUp = srnd(100) < 45;                    // the last-chorus lift
    sng.hasRide = srnd(100) < 60;

    // the sax cell: 3..5 onsets in 2 bars, leaning on the off-beats
    sng.melN = 0;
    static const int MCAND[12] = { 0, 2, 6, 10, 13, 16, 18, 22, 25, 27, 29, 30 };
    for (int i = 0; i < 12 && sng.melN < 5; i++)
        if (srnd(100) < 32) sng.melOn[sng.melN++] = MCAND[i];
    if (sng.melN < 3) { sng.melN = 3; sng.melOn[0] = 2; sng.melOn[1] = 13; sng.melOn[2] = 22; }
    sng.melReg = 67 + srnd(7);

    // THE BAND, rolled per song (these draws COUNT — verbatim from the radio)
    instrument_timbre(I_EP, 0.38f + srnd(22) * 0.01f);       // strike brightness
    instrument_morph(I_EP,  0.06f + srnd(10) * 0.01f);       // a touch of growl, never dirty
    instrument_timbre(I_GTR, 0.55f + srnd(30) * 0.01f);
    instrument_morph(I_GTR,  0.15f + srnd(25) * 0.01f);
    instrument_filter(I_BASS, FILTER_LOW, 520 + srnd(220), 2);

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 92 + srnd(23);                          // 92..114
    bpm(tempo);
    apply_band_overrides();          // picked chairs beat the per-song rolls above
    songBase = (long)pos + 4;
    epInit = padInit = false;
    bassLast = 33;
    melLast = sng.melReg;
#ifdef DE_TRACE
    watch("songsum", "%08X %08X", sng.seed, song_sum());
#endif
}

// ── chart derivation — ZERO srnd draws (the seed-compat law's other half) ─
static void feel_defaults(int drummer) {
    cht.feel[FEEL_GHOST] = drummer == G_PURDIE ? 0.55f : 0.0f;
    cht.feel[FEEL_SWING] = drummer == G_PURDIE ? 0.66f : 0.0f;   // ×0.12 ≈ the 58% shuffle
    cht.feel[FEEL_FILL]  = drummer == G_PURDIE ? 0.40f : 0.30f;
    cht.feel[FEEL_RUNS]  = 1.0f;
    cht.feel[FEEL_ANTIC] = 1.0f;
    cht.feel[FEEL_STAB]  = 1.0f;
    cht.feel[FEEL_LAID]  = 0.70f;                                // ×20ms = the radio's +14
}

static void chart_kit_skeleton(int drummer, int hasRide) {
    memset(cht.drum, 0, sizeof cht.drum);
    if (drummer == G_PURDIE) {                      // half-time: kick 1, backbeat on 3
        cht.drum[L_KICK][0] = 1;
        cht.drum[L_SN][8]   = 2;                    // the backbeat leans in
        for (int st = 0; st < STEPS; st += 2)
            cht.drum[L_HATC][st] = (st % 8 == 0) ? 2 : 1;
    } else {                                        // straight session 16ths (cr-78 reads the same chart)
        cht.drum[L_KICK][0] = 1; cht.drum[L_KICK][7] = 1;
        cht.drum[L_SN][4]   = 1; cht.drum[L_SN][12] = 1;
        for (int st = 0; st < STEPS; st += 2)
            cht.drum[L_HATC][st] = (st % 4 == 0) ? 2 : 1;
    }
    if (hasRide)
        for (int st = 2; st < STEPS; st += 4) cht.drum[L_RIDE][st] = 1;
}

static void derive_chart(void) {
    cht.keyPc = sng.keyPc;
    for (int i = 0; i < 4; i++) cht.loop[i] = sng.loop[i];
    for (int i = 0; i < NFORM; i++) cht.form[i] = FORM_DEFAULT[i];
    cht.gearUp = sng.gearUp;
    chart_kit_skeleton(sng.groove, sng.hasRide);
    memset(cht.saxOn, 0, sizeof cht.saxOn);
    for (int i = 0; i < sng.melN; i++) cht.saxOn[sng.melOn[i]] = 1;
    cht.melReg  = sng.melReg;
    cht.drummer = sng.groove;
    feel_defaults(sng.groove);
    memcpy(cht.title, sng.title, sizeof cht.title);
}

static void gen_from_seed(double pos, unsigned seed) {
    new_song(pos, seed);
    if (!seed) rad_hist_log(&rs);                   // only FRESH rolls enter history
    derive_chart();
    apply_kit(cht.drummer);
    mark_dirty();
}

// ── form / harmony — reading the CHART ────────────────────────────────────
static int sect_of(long bar) {
    if (!songmode) return S_CHORUS;                 // LOOP mode: full band on the chord row
    return cht.form[(bar / 8) % NFORM];
}
static int last_chorus_slot(void) {
    for (int i = NFORM - 1; i >= 0; i--) if (cht.form[i] == S_CHORUS) return i;
    return -1;
}
static int key_shift(long bar) {
    if (!songmode) return 0;
    int s = sect_of(bar);
    if (s == S_BRIDGE) return 5;                    // the bridge lifts to the subdominant
    if (cht.gearUp && s == S_CHORUS && (bar / 8) % NFORM == last_chorus_slot()) return 2;
    return 0;
}
static Ch  chord_at(long bar) { return cht.loop[bar % 4]; }
static int root_pc_at(long bar) {
    Ch c = chord_at(bar);
    return (cht.keyPc + c.off + key_shift(bar)) % 12;
}
static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_CHORUS ? 2 : 1);
    return rad_level(base, intensity);
}
static int mel_note(long bar) {
    Ch c = chord_at(bar);
    int rp = root_pc_at(bar);
    int best = melLast, bestD = 99;
    for (int k = 0; k < 3; k++) {
        int pc = (rp + QV[c.q][k]) % 12;
        int d  = ((pc - melLast) % 12 + 18) % 12 - 6;
        if (rad_iabs(d) + rnd(2) < bestD) { bestD = rad_iabs(d); best = melLast + d; }
    }
    while (best < cht.melReg - 7) best += 12;
    while (best > cht.melReg + 9) best -= 12;
    return melLast = best;
}
static int bass_near(int pc) { return bassLast = rad_bass_to(pc, bassLast, 26, 43); }

// the swing knob: off-beat 8ths drag late (Purdie's 58% at the default .66)
static int swing_ms(int step) {
    if (cht.feel[FEEL_SWING] < 0.02f || step % 4 != 2) return 0;
    return (int)(stepMs * 4 * 0.12f * cht.feel[FEEL_SWING]);
}
static int fchance(int which) { return chance((int)(cht.feel[which] * 100)); }

// ── the step player — yacht's play_step reading the chart through players ─
// kit velocity/duration per drummer hand (from the radio's literal numbers)
static const int KVOL[NGROOVE][NLANE] = {           // base vol; accent cell = +1
    { 5, 4, 2, 2, 2 },   // session
    { 5, 5, 2, 2, 2 },   // purdie
    { 5, 4, 2, 2, 2 },   // cr-78 reads the session chart
};
static const int KDUR[NGROOVE][NLANE] = {
    { 110, 80, 35, 180, 200 },
    { 120, 90, 40, 180, 200 },
    { 110, 80, 35, 180, 200 },
};
static const int KMIDI[NLANE] = { 36, 60, 80, 80, 84 };

static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = songmode ? (s / 16) % 64 : (s / 16);   // SONG loops the 64-bar form
    Ch   c    = chord_at(bar);
    int  rp   = root_pc_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  sw   = swing_ms(step);
    bool kit  = sect != S_INTRO && !(sect == S_OUTRO && bar % 8 >= 6);
    int  g    = cht.drummer;

    // ── THE KIT — the written skeleton, played in the drummer's hand ──
    if (kit && !kitmute) {
        for (int ln = 0; ln < NLANE; ln++) {
            int cell = cht.drum[ln][step];
            if (!cell) continue;
            if (ln == L_RIDE && sect != S_CHORUS) continue;    // the ride rides the chorus
            int vol = KVOL[g][ln] + (cell == 2 ? 1 : 0);
            int d   = dly + 1 + (ln == L_HATC || ln == L_RIDE ? sw : 0);
            schedule_hit(d, KMIDI[ln], SL_KICK + ln, vol, KDUR[g][ln]);
            if (ln == L_KICK || ln == L_SN) vu += 2;
        }
        // the feel layer — what chance() was rolling in the radio
        if (lvl >= 1 && (step == 3 || step == 7 || step == 11 || step == 13) && fchance(FEEL_GHOST))
            schedule_hit(dly + sw + rnd(3), 60, SL_SN, 1, 40);          // the ghosts
        if (step == 10 && fchance(FEEL_FILL))
            { schedule_hit(dly + 1, 36, SL_KICK, KVOL[g][L_KICK], KDUR[g][L_KICK]); vu += 2; }
        if (g != G_PURDIE && step == 14 && chance((int)(cht.feel[FEEL_FILL] * 83)))
            schedule_hit(dly + 1, 80, SL_HATO, 2, 180);                 // the open-hat exhale
        if (g != G_PURDIE && lvl >= 3 && (step & 1))
            schedule_hit(dly + 1, 80, SL_HATC, 1, 25);                  // 16th hats when it opens up
    }

    // ── BASS — roots with the chromatic approach run INTO the change ──
    if (sect != S_INTRO && !pmute[1]) {
        if (step == 0)
            { schedule_hit(dly + 4, bass_near(rp), I_BASS, 5, (int)(stepMs * 3)); vu += 1.6f; }
        else if (step == 8 && g != G_PURDIE)
            schedule_hit(dly + 4, bassLast + (chance(50) ? 7 : 12), I_BASS, 4, (int)(stepMs * 2));
        else if (step == 14 && fchance(FEEL_RUNS)) {         // the run: approach by semitone
            int np = root_pc_at(bar + 1);
            if (np != rp) {
                int target = bass_near(np);
                schedule_hit(dly + 4, target + (chance(60) ? -1 : 1), I_BASS, 4, 90);
                schedule_hit(dly + 4 + (int)stepMs, target, I_BASS, 3, 80);
            }
        }
    }

    // ── THE EPIANO — comping with the citypop anticipation ──
    if ((lvl >= 1 || sect == S_INTRO) && !pmute[0]) {
        bool hit_now = (step == 0 && bar % 2 == 0) || step == 6;
        bool anticip = step == 14 && fchance(FEEL_ANTIC);    // pushes the NEXT bar's chord
        if (hit_now || anticip) {
            long cb = anticip ? bar + 1 : bar;
            Ch cc = chord_at(cb);
            rad_lead_to(root_pc_at(cb), QV[cc.q], gvEp, 3, 58, 76, &epInit);
            int vol = anticip ? 4 : (sect == S_CHORUS ? 5 : 4);
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + sw + k * 7 + rnd(3), gvEp[k], I_EP, vol, 700);
            vu += 1.8f;
        }
    }

    // ── STRAT — clean 9th-stab on the back half, never at bar turns ──
    if (lvl >= 2 && (step == 10 || (step == 5 && bar % 2 == 1)) && fchance(FEEL_STAB)) {
        int n = 64 + ((rp + QV[c.q][chance(50) ? 0 : 2]) % 12);
        schedule_hit(dly + sw + rnd(3), n, I_GTR, 3, 350);
        vu += 0.8f;
    }

    // ── SAX — the hook cell, re-pitched to the changes, a little behind ──
    if ((sect == S_CHORUS || (sect == S_BRIDGE && lvl >= 2)) && !saxmute && !pmute[2]) {
        int cs = (int)(s % HOOKN);
        if (cht.saxOn[cs] && chance(85)) {
            int m = mel_note(bar);
            int gap = 1;                             // distance to the next onset (wraps)
            while (gap < HOOKN && !cht.saxOn[(cs + gap) % HOOKN]) gap++;
            int laid = (int)(cht.feel[FEEL_LAID] * 20);
            schedule_hit(dly + sw + laid + rnd(6), m, I_SAX, 5,
                         gap >= 6 ? (int)(stepMs * 5) : (int)(stepMs * 2));
            vu += 1.4f;
        }
    }

    // ── PAD — strings under the chorus only ──
    if (sect == S_CHORUS && step == 0 && !pmute[3]) {
        rad_lead_to(root_pc_at(bar), QV[c.q], gvPad, 3, 50, 67, &padInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 11, gvPad[k], I_PAD, 2, (int)(stepMs * 15));
    }
}

// ── kit circuits + chairs (yacht.c, verbatim) ─────────────────────────────
static void setup_kit_session(void) {
    instrument(SL_KICK, INSTR_SINE, 0, 230, 0, 50);          // tight studio kick
    instrument_filter(SL_KICK, FILTER_LOW, 300, 2);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 35, 16.0f);
    instrument(SL_SN, INSTR_NOISE, 0, 120, 0, 60);           // dry session snare
    instrument_filter(SL_SN, FILTER_BAND, 1500, 4);
    instrument_env(SL_SN, 0, ENV_CUTOFF, 0, 80, 800.0f);
    instrument(SL_HATC, INSTR_NOISE, 0, 35, 0, 16);
    instrument_filter(SL_HATC, FILTER_HIGH, 8000, 3);
    instrument(SL_HATO, INSTR_NOISE, 0, 240, 0, 90);
    instrument_filter(SL_HATO, FILTER_HIGH, 7400, 3);
}
static void setup_kit_cr78(void) {
    // — CompuRhythm circuits, verbatim from cr78.c (the Hall & Oates move) —
    instrument(SL_KICK, INSTR_SINE, 0, 170, 0, 40);
    instrument_filter(SL_KICK, FILTER_LOW, 320, 2);
    instrument_env(SL_KICK, 0, ENV_PITCH, 0, 45, 13.0f);
    instrument(SL_SN, INSTR_NOISE, 0, 130, 0, 35);
    instrument_filter(SL_SN, FILTER_BAND, 1700, 3);
    instrument_env(SL_SN, 0, ENV_CUTOFF, 0, 90, 900.0f);
    instrument(SL_HATC, INSTR_NOISE, 0, 40, 0, 18);
    instrument_filter(SL_HATC, FILTER_BAND, 7800, 4);
    instrument(SL_HATO, INSTR_NOISE, 0, 320, 0, 80);
    instrument_filter(SL_HATO, FILTER_BAND, 7200, 4);
}
static void apply_kit(int drummer) {
    if (drummer == G_CR78) setup_kit_cr78();
    else setup_kit_session();
}

static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chEp) {
        if (sel == 0) {
            instrument(I_EP, INSTR_FM, 2, 700, 3, 350);      // dx tine — the 1:1 detent
            instrument_harmonics(I_EP, 0.15f);
            instrument_lfo(I_EP, 0, LFO_VOLUME, 4.6f, 0.10f);
        } else if (sel == 1) {
            instrument(I_EP, INSTR_FM, 2, 700, 3, 350);      // soft rhodes
            instrument_harmonics(I_EP, 0.15f);
            instrument_timbre(I_EP, 0.30f);
            instrument_lfo(I_EP, 0, LFO_VOLUME, 4.6f, 0.14f);
        } else {
            instrument(I_EP, INSTR_PLUCK, 1, 0, 6, 300);     // clavinet
            instrument_harmonics(I_EP, 0.35f);
            instrument_timbre(I_EP, 0.85f);
            instrument_morph(I_EP, 0.15f);
            instrument_filter(I_EP, FILTER_LOW, 3000, 1);
        }
    } else if (idx == chBass) {
        if (sel == 0) {
            instrument(I_BASS, INSTR_TRI, 2, 220, 4, 90);    // round fingered electric
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 14, 3);
        } else if (sel == 1) {
            instrument(I_BASS, INSTR_SINE, 2, 220, 4, 90);   // round — pure low end
            instrument_filter(I_BASS, FILTER_LOW, 500, 2);
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 14, 3);
        } else {
            instrument(I_BASS, INSTR_SAW, 2, 220, 4, 90);    // bright — slap-adjacent
            instrument_filter(I_BASS, FILTER_LOW, 900, 2);
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 14, 3);
        }
    } else if (idx == chLead) {
        if (sel == 0) {
            instrument(I_SAX, INSTR_SQUARE, 25, 160, 5, 130); // breathy narrow pulse
            instrument_duty(I_SAX, 0.12f);
            instrument_filter(I_SAX, FILTER_LOW, 1900, 2);
            instrument_lfo(I_SAX, 0, LFO_PITCH, 5.1f, 0.16f);
        } else if (sel == 1) {
            instrument(I_SAX, INSTR_SQUARE, 25, 160, 5, 130); // synth — fatter pulse
            instrument_duty(I_SAX, 0.30f);
            instrument_filter(I_SAX, FILTER_LOW, 2200, 2);
            instrument_lfo(I_SAX, 0, LFO_PITCH, 5.1f, 0.10f);
        } else {
            instrument(I_SAX, INSTR_PLUCK, 1, 0, 6, 500);     // the session guitar solo
            instrument_harmonics(I_SAX, 0.5f);
            instrument_timbre(I_SAX, 0.75f);
            instrument_filter(I_SAX, FILTER_LOW, 2600, 1);
        }
    } else if (idx == chPad) {
        if (sel == 0) {
            instrument(I_PAD, INSTR_SAW, 260, 400, 5, 700);   // soft strings
            instrument_filter(I_PAD, FILTER_LOW, 850, 1);
        } else {
            instrument(I_PAD, INSTR_SAW, 10, 400, 5, 700);    // syn brass — short attack
            instrument_filter(I_PAD, FILTER_LOW, 1400, 1);
            instrument_env(I_PAD, 0, ENV_PITCH, 0, 40, -2);   // the citypop brass fall
        }
    }
}
static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}
static void setup_instruments(void) {
    chEp   = rad_chair(&band, "epiano", "dx tine", "soft rhodes", "clavinet", NULL);
    chBass = rad_chair(&band, "bass",   "fingered", "round", "bright", NULL);
    chLead = rad_chair(&band, "lead",   "sax", "synth", "guitar", NULL);
    chPad  = rad_chair(&band, "pad",    "strings", "syn brass", NULL, NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);

    instrument(I_GTR, INSTR_PLUCK, 1, 0, 7, 600);            // clean strat stabs
    instrument_harmonics(I_GTR, 0.35f);
    instrument_filter(I_GTR, FILTER_LOW, 2800, 1);

    instrument(SL_RIDE, INSTR_SQUARE, 0, 300, 0, 160);       // ride ping
    instrument_filter(SL_RIDE, FILTER_HIGH, 6000, 4);
    setup_kit_session();
}
static void apply_tone(void) {                               // set-on-change only
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_EP,  FILTER_LOW, (int)(3200 * tm), 1);
    instrument_filter(I_SAX, FILTER_LOW, (int)(1900 * tm), 2);
}

// ── save / load (autosaves the whole chart) ───────────────────────────────
#define SAVE_MAGIC 0x5EAD0001u   // sea-desk v1
typedef struct {
    unsigned magic;
    Chart    cht;
    int      tempo, intensity, toneSel;
    bool     songmode;
    int      chair[4];
    bool     pmute[4], kitmute, saxmute;
    unsigned seed;
} SaveBlob;
static int save_cooldown = 0;
static void mark_dirty(void) { save_cooldown = 45; }
static void save_song(void) {
    SaveBlob sb; memset(&sb, 0, sizeof sb);
    sb.magic = SAVE_MAGIC;
    sb.cht = cht;
    sb.tempo = tempo; sb.intensity = intensity; sb.toneSel = toneSel;
    sb.songmode = songmode;
    for (int i = 0; i < 4; i++) sb.chair[i] = band.c[i].sel;
    memcpy(sb.pmute, pmute, sizeof pmute);
    sb.kitmute = kitmute; sb.saxmute = saxmute;
    sb.seed = sng.seed;
    save_bytes(&sb, sizeof sb);
}
static bool load_song(void) {
    SaveBlob sb;
    if (load_bytes(&sb, sizeof sb) != sizeof sb || sb.magic != SAVE_MAGIC) return false;
    cht = sb.cht;
    tempo = sb.tempo; intensity = sb.intensity; toneSel = sb.toneSel;
    songmode = sb.songmode;
    for (int i = 0; i < 4; i++) band.c[i].sel = sb.chair[i];
    memcpy(pmute, sb.pmute, sizeof pmute);
    kitmute = sb.kitmute; saxmute = sb.saxmute;
    sng.seed = sb.seed;                             // the code the chart came from
    memcpy(sng.title, cht.title, sizeof sng.title);
    return true;
}

// ── WAV export — arms the engine's live capture ───────────────────────────
static int  export_flash = 0;
static char export_name[40];
static void export_wav(double pos) {
    float bar_s = 240.0f / tempo;
    float secs = 64 * bar_s + 2.0f;
    if (secs > 60.0f) secs = 60.0f;                 // the engine cap
    snprintf(export_name, sizeof export_name, "yachtrack-%08X.wav", sng.seed);
    FILE *f = fopen(".bake/wav_request", "w");
    if (!f) return;
    fprintf(f, "%s\n%.1f\n", export_name, secs);
    fclose(f);
    export_flash = 240;
    songBase = (long)pos + 4;                       // from the top for the capture
    scheduled = (long)pos;
    running = true;
}

// ── layout ────────────────────────────────────────────────────────────────
#define TB_H     22
#define CHORD_Y  24            // the chord row (always visible — the star)
#define FORM_Y   48            // the form row
#define STRIPS_Y 64
#define HDR_H    16
#define PANEL_H  100
enum { STRIP_DRUMS, STRIP_HOOK, STRIP_BAND, STRIP_DESK, NSTRIP };
static const char *SNAME[NSTRIP] = { "DRUMS", "HOOK", "BAND", "DESK" };
static int expanded = STRIP_DRUMS;
static int strip_y(int i) {
    int y = STRIPS_Y;
    for (int k = 0; k < i; k++) y += HDR_H + (k == expanded ? PANEL_H : 0);
    return y;
}
// drum grid geometry (shared by update taps + draw)
#define DG_X  36
#define DG_CW 13
#define DG_RH 10
// hook lane geometry
#define HK_X  12
#define HK_CW 9

// ── per-finger surface routing (the dubdesk fix, via acidrack) ────────────
#define NFING 12
static int  own_ids[NFING], own_n = 0;
static int  seen_ids[NFING], seen_n = 0;
static bool is_owned(int id)  { for (int i = 0; i < own_n; i++)  if (own_ids[i]  == id) return true; return false; }
static bool was_seen(int id)  { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) return true; return false; }
static void own_add(int id)   { if (!is_owned(id) && own_n < NFING) own_ids[own_n++] = id; }
static void seen_add(int id)  { if (!was_seen(id) && seen_n < NFING) seen_ids[seen_n++] = id; }
static void own_drop(int id)  { for (int i = 0; i < own_n; i++)  if (own_ids[i]  == id) { own_ids[i]  = own_ids[--own_n];   return; } }
static void seen_drop(int id) { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) { seen_ids[i] = seen_ids[--seen_n]; return; } }

// ── update ────────────────────────────────────────────────────────────────
static void start_transport(double pos) {
    running = true;
    songBase = (long)pos + 4;
    scheduled = (long)pos;
    epInit = padInit = false;
    bassLast = 33; melLast = cht.melReg;
}
static void stop_transport(void) { running = false; note_off_all(); }

void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (!load_song()) gen_from_seed(pos, 0x0A183744);   // first boot: a stock chart
        else { apply_kit(cht.drummer); apply_band_overrides(); }
        bpm(tempo);
        apply_tone();
        booted = true;
    }

    // keys
    if (keyp(KEY_SPACE)) { if (running) stop_transport(); else start_transport(pos); }
    if (keyp(KEY_UP))    { tempo += 2; if (tempo > 140) tempo = 140; bpm(tempo); mark_dirty(); }
    if (keyp(KEY_DOWN))  { tempo -= 2; if (tempo < 70)  tempo = 70;  bpm(tempo); mark_dirty(); }
    if (keyp('G'))       gen_from_seed(pos, 0);
    if (keyp('['))       { unsigned sd = rad_hist_back(&rs); if (sd) gen_from_seed(pos, sd); }
    if (keyp(']'))       { unsigned sd = rad_hist_fwd(&rs);  if (sd) gen_from_seed(pos, sd); }
    if (keyp('T'))       { toneSel = (toneSel + 1) % 4; apply_tone(); mark_dirty(); }

    // ── raw-tap surfaces (chord row / form row / grids / code digits) ─────
    for (int i = 0; i < touch_count(); i++) if (ui_captured(touch_id(i))) own_add(touch_id(i));
    for (int i = 0; i < touch_ended_count(); i++) {
        own_drop(touch_ended_id(i)); seen_drop(touch_ended_id(i));
    }
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (is_owned(id)) continue;
        int px = touch_x(i), py = touch_y(i);
        bool tap = !was_seen(id);
        seen_add(id);
        if (!tap) continue;                          // chart surfaces are tap, not drag

        // song-code digits on the transport: tap to nudge (regenerates live)
        if (py >= 3 && py < TB_H - 2 && px >= 196 && px < 196 + 64) {
            int d = (px - 196) / 8, shift = (7 - d) * 4;
            unsigned nib = ((sng.seed >> shift) + 1u) & 15u;
            gen_from_seed(pos, (sng.seed & ~(15u << shift)) | (nib << shift));
        }

        // the chord row: arrows nudge the root, the cell cycles the quality
        if (py >= CHORD_Y && py < CHORD_Y + 22 && px >= 2 && px < 2 + 4 * 56) {
            int cell = (px - 2) / 56, cx = (px - 2) % 56;
            if (cx < 12) {                           // the arrow column
                int d = (py < CHORD_Y + 11) ? 1 : -1;
                cht.loop[cell].off = (cht.loop[cell].off + d + 12) % 12;
            } else
                cht.loop[cell].q = (cht.loop[cell].q + 1) % NQ;
            mark_dirty();
        }
        // flavor buttons are ui_buttons (drawn in draw); KEY too

        // the form row: tap a slot to cycle its section
        if (py >= FORM_Y && py < FORM_Y + 14 && px >= 2 && px < 2 + NFORM * 26) {
            int slot = (px - 2) / 26;
            cht.form[slot] = (cht.form[slot] + 1) % NSECT;
            mark_dirty();
        }

        // strip headers: tap to expand (left zone only — buttons live right)
        for (int k = 0; k < NSTRIP; k++) {
            int y = strip_y(k);
            if (py >= y && py < y + HDR_H && px < 240) { expanded = k; break; }
        }

        // DRUMS grid: tap cycles off → hit → accent → off
        if (expanded == STRIP_DRUMS) {
            int gy = strip_y(STRIP_DRUMS) + HDR_H + 4;
            if (px >= DG_X && px < DG_X + STEPS * DG_CW && py >= gy && py < gy + NLANE * DG_RH) {
                int st = (px - DG_X) / DG_CW, ln = (py - gy) / DG_RH;
                cht.drum[ln][st] = (unsigned char)((cht.drum[ln][st] + 1) % 3);
                mark_dirty();
            }
        }
        // HOOK lane: tap toggles an onset
        if (expanded == STRIP_HOOK) {
            int gy = strip_y(STRIP_HOOK) + HDR_H + 16;
            if (px >= HK_X && px < HK_X + HOOKN * HK_CW && py >= gy && py < gy + 16) {
                int cs = (px - HK_X) / HK_CW;
                cht.saxOn[cs] = !cht.saxOn[cs];
                mark_dirty();
            }
        }
    }

    if (save_cooldown > 0 && --save_cooldown == 0) save_song();
    if (export_flash > 0) export_flash--;
    vu *= 0.9f;

#ifdef DE_TRACE
    watch("step", "%d", playhead);
    watch("bar",  "%d", (int)playbar);
    watch("sect", "%s", SECTNAME[sect_of(playbar)]);
#endif

    if (!running) return;
    long st;
    while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
    long s = scheduled - songBase;
    if (s >= 0) { playhead = (int)(s % 16); playbar = songmode ? (s / 16) % 64 : s / 16; }
}

// ── draw ──────────────────────────────────────────────────────────────────
static const int SECTCLR[NSECT] = { CLR_DARK_GREY, CLR_TRUE_BLUE, CLR_ORANGE, CLR_INDIGO, CLR_DARK_GREY };

static void draw_transport(void) {
    rectfill(0, 0, 320, TB_H, CLR_BLACK);
    if (ui_button(2, 2, 26, TB_H - 4, running ? "||" : ">")) {
        double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
        if (running) stop_transport(); else start_transport(pos);
    }
    if (ui_button(30, 2, 40, TB_H - 4, songmode ? "SONG" : "LOOP")) { songmode = !songmode; mark_dirty(); }
    if (ui_button(72, 2, 14, TB_H - 4, "-")) { tempo -= 2; if (tempo < 70) tempo = 70; bpm(tempo); mark_dirty(); }
    char b[16]; snprintf(b, sizeof b, "%d", tempo);
    print(b, 90, 7, CLR_WHITE);
    if (ui_button(114, 2, 14, TB_H - 4, "+")) { tempo += 2; if (tempo > 140) tempo = 140; bpm(tempo); mark_dirty(); }
    // the song code — the radio handoff (tap a digit to nudge; raw surface)
    font(FONT_SMALL);
    print("CODE", 136, 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    char code[12]; snprintf(code, sizeof code, "%08X", sng.seed);
    for (int d = 0; d < 8; d++) {
        rectfill(196 + d * 8, 5, 7, 12, CLR_DARKER_BLUE);
        char cc[2] = { code[d], 0 };
        print(cc, 197 + d * 8, 7, CLR_TRUE_BLUE);
    }
    font(FONT_SMALL);
    char tb[11]; snprintf(tb, sizeof tb, "%s", cht.title);   // clipped to the bar
    print(tb, 266, 8, CLR_BLUE);
    font(FONT_NORMAL);
}

static void draw_chart(void) {
    // the chord row — the star gets the fixed real estate
    for (int i = 0; i < 4; i++) {
        int x = 2 + i * 56;
        bool now = running && (playbar % 4) == i;
        rectfill(x, CHORD_Y, 54, 22, now ? CLR_DARKER_GREY : CLR_DARKER_BLUE);
        // the root-nudge arrows
        print("+", x + 2, CHORD_Y + 1, CLR_MEDIUM_GREY);
        print("-", x + 2, CHORD_Y + 12, CLR_MEDIUM_GREY);
        int lc = cht.loop[i].q == Q_MU ? CLR_YELLOW : CLR_WHITE;
        print(RAD_PCNAME[(cht.keyPc + cht.loop[i].off) % 12], x + 16, CHORD_Y + 3, lc);
        font(FONT_SMALL);
        print(QN[cht.loop[i].q], x + 16, CHORD_Y + 14, lc);
        font(FONT_NORMAL);
        if (now) rect(x, CHORD_Y, 54, 22, CLR_WHITE);
    }
    if (ui_button(230, CHORD_Y, 42, 10, "MU-IFY")) {         // the generator's melt rolls, on demand
        for (int i = 0; i < 4; i++)
            if (cht.loop[i].q == Q_MAJ7 && cht.loop[i].off != 1) { cht.loop[i].q = Q_MU; break; }
        mark_dirty();
    }
    if (ui_button(230, CHORD_Y + 12, 42, 10, "SUSML")) {
        for (int i = 1; i < 4; i++)
            if (cht.loop[i].q == Q_DOM13) { cht.loop[i].q = Q_SUS9; break; }
        mark_dirty();
    }
    char kb[8]; snprintf(kb, sizeof kb, "KEY %s", RAD_PCNAME[cht.keyPc]);
    if (ui_button(276, CHORD_Y, 42, 22, kb)) { cht.keyPc = (cht.keyPc + 1) % 12; mark_dirty(); }

    // the form row
    int curslot = (int)((playbar / 8) % NFORM);
    for (int i = 0; i < NFORM; i++) {
        int x = 2 + i * 26;
        int s = cht.form[i];
        rectfill(x, FORM_Y, 24, 14, SECTCLR[s]);
        char sc[2] = { SECTCH[s], 0 };
        print(sc, x + 9, FORM_Y + 3, s == S_INTRO || s == S_OUTRO ? CLR_LIGHT_GREY : CLR_BLACK);
        if (running && songmode && i == curslot) rect(x, FORM_Y, 24, 14, CLR_WHITE);
        if (cht.gearUp && s == S_CHORUS && i == last_chorus_slot())
            print("+", x + 18, FORM_Y + 3, CLR_RED);
    }
    if (ui_button(214, FORM_Y, 36, 14, cht.gearUp ? "GEAR!" : "gear")) { cht.gearUp = !cht.gearUp; mark_dirty(); }
    font(FONT_SMALL);
    print(songmode ? SECTNAME[sect_of(playbar)] : "loop", 256, FORM_Y + 4, CLR_BLUE);
    font(FONT_NORMAL);
}

static void draw_header(int i, int y) {
    bool open = (i == expanded);
    rectfill(2, y, 316, HDR_H - 2, open ? CLR_DARKER_GREY : CLR_DARK_GREY);
    print(open ? "v" : ">", 8, y + 4, CLR_LIGHT_GREY);
    bool muted = (i == STRIP_DRUMS && kitmute) || (i == STRIP_HOOK && saxmute);
    print(SNAME[i], 18, y + 4, muted ? CLR_MEDIUM_GREY : CLR_WHITE);
    // folded ≠ blind: a live 16-tick mini lane with the playhead
    for (int s = 0; s < STEPS; s++) {
        int tx = 78 + s * 6;
        bool onn = false;
        if      (i == STRIP_DRUMS) { for (int ln = 0; ln < NLANE; ln++) if (cht.drum[ln][s]) { onn = true; break; } }
        else if (i == STRIP_HOOK)  onn = cht.saxOn[s] || cht.saxOn[s + 16];
        else if (i == STRIP_BAND)  onn = (s == 0 || s == 6 || s == 14);      // the comp hits
        else                       onn = (s % 4 == 0);
        int c = onn ? (muted ? CLR_MEDIUM_GREY : CLR_ORANGE) : CLR_DARKER_GREY;
        if (running && s == playhead) c = CLR_WHITE;
        rectfill(tx, y + 5, 4, 6, c);
    }
    if (i == STRIP_DRUMS && ui_button(252, y + 1, 34, HDR_H - 4, kitmute ? "MUTED" : "mute")) { kitmute = !kitmute; mark_dirty(); }
    if (i == STRIP_HOOK  && ui_button(252, y + 1, 34, HDR_H - 4, saxmute ? "MUTED" : "mute")) { saxmute = !saxmute; mark_dirty(); }
}

static void draw_drums_panel(int y0) {
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    int gy = y0 + 4;
    font(FONT_SMALL);
    for (int ln = 0; ln < NLANE; ln++) {
        print(LANENAME[ln], 8, gy + ln * DG_RH + 2, CLR_MEDIUM_GREY);
        for (int st = 0; st < STEPS; st++) {
            int cx = DG_X + st * DG_CW;
            int cell = cht.drum[ln][st];
            int c = cell == 2 ? CLR_RED : cell ? CLR_TRUE_BLUE
                  : ((st & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
            if (running && st == playhead && cell) c = CLR_WHITE;
            rectfill(cx, gy + ln * DG_RH, DG_CW - 1, DG_RH - 1, c);
        }
    }
    print("ride plays the chorus", DG_X, gy + NLANE * DG_RH + 2, CLR_DARK_GREY);
    font(FONT_NORMAL);
    // the drummer chairs — swap the hands + feel; RECHART rewrites the skeleton
    for (int g = 0; g < NGROOVE; g++)
        if (ui_button(248, y0 + 4 + g * 15, 66, 13, GROOVENAME[g])) {
            cht.drummer = g;
            apply_kit(g);
            feel_defaults(g);
            mark_dirty();
        }
    if (cht.drummer >= 0) rect(246, y0 + 2 + cht.drummer * 15, 70, 17, CLR_ORANGE);
    if (ui_button(248, y0 + 52, 66, 13, "RECHART")) {
        chart_kit_skeleton(cht.drummer, 1);          // rewrite in this drummer's hand (keeps the ride)
        mark_dirty();
    }
    if (ui_knob(&cht.feel[FEEL_GHOST], 70,  y0 + 78, "GHOST")) mark_dirty();
    if (ui_knob(&cht.feel[FEEL_SWING], 120, y0 + 78, "SWING")) mark_dirty();
    if (ui_knob(&cht.feel[FEEL_FILL],  170, y0 + 78, "FILL"))  mark_dirty();
}

static void draw_hook_panel(int y0) {
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    font(FONT_SMALL);
    print("tap where the hook lands - the sax picks the notes", 8, y0 + 4, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
    int gy = y0 + 16;
    for (int cs = 0; cs < HOOKN; cs++) {
        int cx = HK_X + cs * HK_CW;
        int c = cht.saxOn[cs] ? CLR_ORANGE : ((cs & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
        if (running && (int)((scheduled - songBase) % HOOKN) == cs && cht.saxOn[cs]) c = CLR_WHITE;
        rectfill(cx, gy, HK_CW - 1, 16, c);
    }
    if (rad_knob_int(&cht.melReg, 60, 74, 1, 60, y0 + 62, 10, "register", CLR_BLUE)) mark_dirty();
    if (ui_knob(&cht.feel[FEEL_LAID], 130, y0 + 62, "LAID")) mark_dirty();
    font(FONT_SMALL);
    print("LAID = behind the beat", 170, y0 + 58, CLR_DARK_GREY);
    print("plays chorus + bridge", 170, y0 + 68, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

static void draw_band_panel(int y0) {
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    font(FONT_SMALL);
    for (int i = 0; i < band.n; i++) {
        int ry = y0 + 4 + i * 18;
        print(band.c[i].name, 8, ry + 5, CLR_MEDIUM_GREY);
        for (int j = 0; j < band.c[i].ncand; j++) {
            if (ui_button(58 + j * 68, ry, 66, 15, band.c[i].cand[j])) {
                band.c[i].sel = j;
                apply_chair(i);
                apply_tone();                        // chair swaps re-aim slots; re-assert the tone reach
                mark_dirty();
            }
            if (band.c[i].sel == j) rect(57 + j * 68, ry - 1, 68, 17, CLR_ORANGE);
        }
        if (ui_button(284, ry, 32, 15, pmute[i] ? "MUTED" : "mute")) { pmute[i] = !pmute[i]; mark_dirty(); }
    }
    print("same recipes as the yacht radio's band panel", 8, y0 + 80, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

static void draw_desk_panel(int y0) {
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    if (ui_button(8,   y0 + 6, 44, 16, "NEW"))  gen_from_seed(pos, 0);
    if (ui_button(56,  y0 + 6, 24, 16, "["))    { unsigned sd = rad_hist_back(&rs); if (sd) gen_from_seed(pos, sd); }
    if (ui_button(84,  y0 + 6, 24, 16, "]"))    { unsigned sd = rad_hist_fwd(&rs);  if (sd) gen_from_seed(pos, sd); }
    if (ui_button(112, y0 + 6, 44, 16, "WAV"))  export_wav(pos);
    static const char *FEELNAME[4] = { "dock", "cruise", "regatta", "open sea" };
    if (rad_knob_sel(&intensity, 4, 190, y0 + 16, 10, FEELNAME[intensity], CLR_BLUE)) mark_dirty();
    if (rad_knob_sel(&toneSel, 4, 250, y0 + 16, 10, RAD_TONENAME[toneSel], CLR_BLUE)) { apply_tone(); mark_dirty(); }
    // the players' feel — the chance() layer, grabbable
    if (ui_knob(&cht.feel[FEEL_RUNS],  40,  y0 + 66, "RUNS"))  mark_dirty();
    if (ui_knob(&cht.feel[FEEL_ANTIC], 100, y0 + 66, "ANTIC")) mark_dirty();
    if (ui_knob(&cht.feel[FEEL_STAB],  160, y0 + 66, "STAB"))  mark_dirty();
    font(FONT_SMALL);
    print("RUNS bass approach", 200, y0 + 52, CLR_DARK_GREY);
    print("ANTIC and-of-4 push", 200, y0 + 62, CLR_DARK_GREY);
    print("STAB strat 9ths", 200, y0 + 72, CLR_DARK_GREY);
    font(FONT_NORMAL);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    ui_begin();
    draw_transport();
    draw_chart();
    for (int i = 0; i < NSTRIP; i++) {
        int y = strip_y(i);
        draw_header(i, y);
        if (i == expanded) {
            int y0 = y + HDR_H;
            if      (i == STRIP_DRUMS) draw_drums_panel(y0);
            else if (i == STRIP_HOOK)  draw_hook_panel(y0);
            else if (i == STRIP_BAND)  draw_band_panel(y0);
            else                       draw_desk_panel(y0);
        }
    }
    // the status line
    font(FONT_SMALL);
    if (export_flash > 0) {
        char eb[64]; snprintf(eb, sizeof eb, "exporting %s (60s cap)", export_name);
        print(eb, 6, 231, CLR_YELLOW);
    } else if (save_cooldown > 0) {
        print("...", 6, 231, CLR_DARK_GREY);
    } else {
        print("SPACE play  G new code  [ ] history  T tone", 6, 231, CLR_DARK_GREY);
    }
    float vt = vu / 12.0f;
    rectfill(250, 232, (int)((vt > 1 ? 1 : vt) * 64), 3, CLR_BLUE);
    font(FONT_NORMAL);
    ui_end();
}

// ── the seed-compat spec — golden (seed, songsum) pairs harvested from the
// yacht radio's DE_TRACE (yacht-rack.md §3: a Song-dump diff, not a trace
// diff — the performance layer is unseeded by design). Regenerating each
// seed here must reproduce the radio's composed state exactly.
#ifdef DE_SPEC
void spec(void) {
    step(2);                                        // boot (init + first update)
    static const unsigned GOLD[][2] = {
        // harvested from the radio's DE_TRACE songsum watch (44 songs, --seed 1 + --seed 7
        // scripted SPACE presses). FROZEN: the radio's draw sequence must never change
        // (the radio.h seed-compat law), so these pairs are valid forever.
        { 0x37F20AB1u, 0x296D578Cu },
        { 0x0A183744u, 0x2A2BD0ECu },
        { 0xA5D98D3Bu, 0xC63F328Cu },
        { 0x7BEDD6EAu, 0x18A98E73u },
        { 0x98DCAD2Du, 0x7CA5F36Fu },
        { 0xEF1E3246u, 0x90B822E8u },
        { 0xE75BA499u, 0x826E1CBBu },
        { 0x74DC4BF1u, 0x06650A3Eu },
        { 0x842A4677u, 0x5C85D7FDu },
        { 0xDBCF3520u, 0x262FD005u },
        { 0x8075D11Du, 0xF9093F5Fu },
        { 0x18AED737u, 0xC1831D3Fu },
        { 0x59BD678Cu, 0xF8589AF7u },
        { 0x85FC09D2u, 0x0628A80Du },
        { 0xBC820100u, 0x19312E7Cu },
        { 0xC5F65539u, 0x32D17403u },
        { 0x1DB4C809u, 0x89DD86CFu },
        { 0xC78F083Au, 0x5BFBD0A6u },
        { 0x0CA33E8Du, 0x6FE4B9A4u },
        { 0x4AE8779Du, 0x793BBA7Eu },
        { 0xC5FE19C4u, 0x0AAC23BAu },
        { 0xFF3D2A94u, 0x6114A9DAu },
        { 0x60654197u, 0xF64A2DFDu },
        { 0xAD8981DBu, 0xE744C69Du },
        { 0xD8487998u, 0xE94A61F8u },
        { 0x9675341Fu, 0xED4D8235u },
        { 0xB5B22167u, 0xBDD67BE5u },
        { 0xFF8EA0DAu, 0x49F1570Bu },
        { 0x4C26AA2Du, 0x8DFFA6BCu },
        { 0x14D0D4ACu, 0x5D6B618Du },
        { 0x8B5AD64Cu, 0xF79096EBu },
        { 0xA6354B91u, 0x0AD0FB48u },
        { 0x0001DF71u, 0x3CB76096u },
        { 0x1989595Fu, 0x7295F723u },
        { 0xD7022668u, 0xB100A046u },
        { 0xA553DA35u, 0x47C7704Au },
        { 0x22063643u, 0x94DA1FD1u },
        { 0x480FB05Au, 0xF15BFD9Au },
        { 0xBE29A71Au, 0xBB49437Au },
        { 0x0EB20564u, 0x0DC35020u },
        { 0x2CCE14B0u, 0x38848112u },
        { 0xDA4127D1u, 0x2E10F467u },
        { 0xE673D78Fu, 0xD29E0708u },
        { 0x9865AEC4u, 0xD169872Du },
    };
    for (int i = 0; i < (int)(sizeof GOLD / sizeof GOLD[0]); i++) {
        new_song(0, GOLD[i][0]);
        expect_eq((long)song_sum(), (long)GOLD[i][1], "songsum matches the yacht radio");
    }
    // derivation invariants
    new_song(0, GOLD[0][0]);
    derive_chart();
    expect(cht.drum[L_KICK][0] == 1, "skeleton: kick on the one");
    expect(cht.form[0] == S_INTRO && cht.form[7] == S_OUTRO, "default form is the radio's");
    int onsets = 0; for (int i = 0; i < HOOKN; i++) onsets += cht.saxOn[i] ? 1 : 0;
    expect(onsets >= 3 && onsets <= 5, "sax cell 3..5 onsets");
    expect(sng.seed == GOLD[0][0], "seed survives generation");
}
#endif
