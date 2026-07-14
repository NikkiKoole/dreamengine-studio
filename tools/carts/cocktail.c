/* de:meta
{
  "slug": "cocktail",
  "collection": ["radio"],
  "title": "cocktail radio",
  "status": "active",
  "created": "2026-06-05",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing",
    "swing-timing"
  ],
  "lineage": "Fourteenth radio station; Vince Guaraldi / Oscar Peterson piano-trio homage, cousin of bossa.c. Novel: the walking bass generator (chord-tone on 1, scale motion through the bar, chromatic approach into beat 4) and the bass-takes-a-solo arc via improv.h.",
  "homage": "Vince Guaraldi / Oscar Peterson (piano trio)",
  "description": "An endless piano-trio set - the Vince Guaraldi / Oscar Peterson lounge homage, station #14. Two blocks make it: THE WALKING BASS, the long-promised ~20-line generator (chord tone on beat 1, scale/arpeggio motion through the middle, a chromatic approach into the next bar's root on beat 4 - four quarters a bar, forever, never repeating), and THE IMPROVISER's second gig - runtime/improv.h, extracted from roadhouse.c by the second-customer rule: the piano solos over full tension arcs, and then THE BASS TAKES A SOLO (same brain, low register, two-thirds density, the room leans in - brushes drop to a whisper, the piano just nods). Harmony is a weighted ii-V-I functional walk with secondary dominants and the tritone sub, cadences FORCED into the last two bars of every 8 - the set always comes home. Swing at 62%: ride pattern ding ding-ga-ding, hat snapping 2 and 4, feathered kick, brush sweeps. Trio set form: head, piano solo, bass solo, piano trades back, head out. R replays the tune - the solos are always new. SPACE next tune, [ ] history, LEFT/RIGHT feel (last call/set one/set two/burner), UP/DOWN tempo, T tone, M power, B band (swap chairs live), H help."
}
de:meta */
// ── COCKTAIL RADIO ────────────────────────────────────────────────────────
// The fourteenth station: an endless piano-trio set — the Vince Guaraldi /
// Oscar Peterson lounge homage. Piano, upright bass, brushes; nobody sings,
// everybody swings, the bartender knows your order.
//
// Two blocks make this station:
//   • THE WALKING BASS — the generator the guide promised ("a beautiful
//     ~20-line algorithm"): chord tone on beat 1, scale/arpeggio motion
//     through the middle, and a CHROMATIC APPROACH into the next bar's root
//     on beat 4. Four quarter notes a bar, forever, and it never repeats.
//   • THE IMPROVISER'S SECOND GIG — runtime/improv.h (extracted from
//     roadhouse.c this very session, the radio.h second-customer rule):
//     the piano solos over two arcs... and then THE BASS SOLO — same brain,
//     low register, two-thirds density, brushes drop to a whisper. The trio
//     set form: head, piano solo, bass solo, trade back, head out.
//
// Harmony is the bossa brain's jazz cousin at full rate: a weighted walk
// over ii-V-I functions (secondary dominants resolving down a fifth, the
// tritone sub) with the CADENCE FORCED into the last two bars of every
// 8-bar section — the walk always pulls back home. Swing is 62%: the ride
// pattern (ding, ding-ga-ding) carries the time, the hat snaps 2 and 4.
//
//   SPACE next tune   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   H or ? help

#include "studio.h"
#include "radio.h"
#include "improv.h"
#include <stdio.h>
#include <math.h>

#define COCKTAIL_SEED 0   // pin a favourite set here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_PNO   5    // the piano — comping left hand + melody right hand
#define I_PSOLO 6    // the piano's solo stop (same family, brighter)
#define I_BASS  7    // the upright — walking
// the brushes
#define SL_RIDE 8
#define SL_HAT  9
#define SL_BRSH 10   // the brush sweep
#define SL_KICK 11   // feathered, barely there

// ── harmony — the functional walk, jazz rate (one chord per bar) ──────────
enum { Q_MAJ7, Q_MIN7, Q_DOM7, Q_M7B5, Q_MIN6, NQ };
static const char *QN[NQ] = { "maj7", "m7", "7", "m7b5", "m6" };
static const int QV[NQ][3] = {
    { 4, 11, 14 },   // maj7 — 3 7 9, rootless (the bass owns the root)
    { 3, 10, 14 },   // m7
    { 4, 10, 14 },   // 7
    { 3,  6, 10 },   // m7b5
    { 3,  9, 14 },   // m6
};
// functions: scale-degree offset + quality (the bossa tables, one town over)
enum { F_I, F_ii, F_iii, F_IV, F_V, F_vi, F_II7, F_VI7, F_bII7, F_iv, NF };
static const int F_OFF[NF]  = { 0, 2, 4, 5, 7, 9, 2, 9, 1, 5 };
static const int F_QUAL[NF] = { Q_MAJ7, Q_MIN7, Q_MIN7, Q_MAJ7, Q_DOM7,
                                Q_MIN7, Q_DOM7, Q_DOM7, Q_DOM7, Q_MIN6 };
// where can each function go? repeats = more likely (the jazz cheat-sheet)
static const int FNEXT[NF][6] = {
    { F_vi, F_ii, F_IV, F_iii, F_VI7, F_ii },     // I
    { F_V, F_V, F_V, F_bII7, F_V, F_iii },        // ii -> V (or the tritone sub)
    { F_vi, F_VI7, F_ii, F_vi, F_IV, F_vi },      // iii
    { F_iv, F_ii, F_I, F_V, F_ii, F_iii },        // IV (-> iv = the borrowed dusk)
    { F_I, F_I, F_I, F_vi, F_iii, F_I },          // V -> home (sometimes deceptive)
    { F_ii, F_II7, F_ii, F_iv, F_ii, F_VI7 },     // vi
    { F_V, F_V, F_V, F_ii, F_V, F_V },            // II7 -> V
    { F_ii, F_ii, F_ii, F_ii, F_ii, F_ii },       // VI7 -> ii
    { F_I, F_I, F_I, F_I, F_iii, F_I },           // bII7 -> I (the sub resolves)
    { F_I, F_I, F_bII7, F_I, F_I, F_I },          // iv -> I (backdoor-ish)
};
static const int JAZZMAJ[7] = { 0, 2, 4, 5, 7, 9, 11 };   // the solos' scale

// ── the form — a trio set ─────────────────────────────────────────────────
enum { S_HEAD, S_PIANO, S_BASS, S_OUTRO };
static const int FORM[8] = { S_HEAD, S_HEAD, S_PIANO, S_PIANO,
                             S_BASS, S_PIANO, S_HEAD, S_OUTRO };
static const char *SECTNAME[4] = { "head", "piano solo", "bass solo", "outro" };

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    int  fn[64];               // the function walk, one per bar, pre-rolled
    int  blue;                 // 8..30 — lounge players bend politely
    int  melOn[6], melN, melDeg[6];   // the head (32-step cell)
    int  ballad;               // slow set vs burner
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

static int    tempo     = 120;
static int    intensity = 1;
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    toneSel   = 2;
static int    songCount = 0;
static float  vu        = 0;
static int    bassLast  = 33;
static char   nowChord[4][12];

static int  gvPno[3] = { 62, 65, 69 };
static bool pnoInit = false;
static Improv solo;

// THE BAND (B) — the chairs and their candidates, from radio-instrument-options.md.
// First station on the rad_band panel: the trio's third chair is the genre's real
// historical variable (Peterson's guitar trio, the MJQ's vibes), so the solo chair
// gets three candidates and improv.h just plays whoever sits down.
static RadBand band;
static int chPiano, chSolo, chBass, chKit;

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);   // defined with the chairs, below

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Velvet", "Manhattan", "Midnight", "Smoky", "Uptown",
    "Corner", "Blue", "Brandy", "Penthouse", "Olive", "Amber", "Last" };
static const char *TW2[] = { "Martini", "Set", "Stroll", "Standard", "Snowfall",
    "Lights", "Bounce", "Nightcap", "Booth", "Avenue", "Slippers", "Call" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.keyPc  = srnd(12);
    sng.ballad = srnd(100) < 25;
    sng.blue   = 8 + srnd(23);

    // pre-roll the WHOLE walk (64 bars): weighted next-function steps with
    // the cadence forced into bars 6-7 of every 8 — the set always comes home
    int f = F_I;
    for (int b = 0; b < 64; b++) {
        int ib = b % 8;
        if (ib == 6)      f = F_ii;
        else if (ib == 7) f = (srnd(100) < 20) ? F_bII7 : F_V;
        else if (ib == 0 && b % 16 == 0) f = F_I;
        else              f = FNEXT[f][srnd(6)];
        sng.fn[b] = f;
    }

    // the head: a seeded cell, low diatonic degrees (the tune you can hum)
    sng.melN = 0;
    static const int HC[10] = { 0, 3, 6, 10, 13, 16, 19, 22, 26, 28 };
    for (int i = 0; i < 10 && sng.melN < 5; i++)
        if (srnd(100) < 38) sng.melOn[sng.melN++] = HC[i];
    if (sng.melN < 3) { sng.melN = 3; sng.melOn[0] = 0; sng.melOn[1] = 10; sng.melOn[2] = 19; }
    for (int i = 0; i < sng.melN; i++) sng.melDeg[i] = srnd(6);

    // the trio's roll: piano brightness, bass darkness, brush hiss
    instrument_filter(I_PNO,   FILTER_LOW, 1900 + srnd(700), 1);
    instrument_filter(I_PSOLO, FILTER_LOW, 2300 + srnd(800), 1);
    instrument_filter(I_BASS,  FILTER_LOW, 420 + srnd(160), 1);

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = sng.ballad ? 96 + srnd(13) : 116 + srnd(29);   // 96..108 / 116..144
    bpm(tempo);
    apply_band_overrides();          // picked chairs beat the filter roll above
    songBase = (long)pos + 8;
    pnoInit = false;
    bassLast = 33;
    songCount++;
}

static void fresh_song(double pos) {
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ────────────────────────────────────────────────────────
static int sect_of(long bar) { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }
static int fn_at(long bar)   { return sng.fn[bar < 0 ? 0 : bar % 64]; }
static int root_pc(int f)    { return (sng.keyPc + F_OFF[f]) % 12; }
static int qual(int f)       { return F_QUAL[f]; }

static void chord_label(char *out, int n, int f) {
    snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(f)], QN[qual(f)]);
}

static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_OUTRO) ? 0 : (s == S_HEAD ? 1 : (s == S_BASS ? 1 : 2));
    return rad_level(base, intensity);
}

// the swing: every off-beat 8th lands at 62%, not 50%
static int swing_ms(int step) {
    return (step % 4 == 2) ? (int)(stepMs * 4 * (0.62 - 0.50)) : 0;
}

// ══ THE WALKING BASS — the promised ~20 lines ═════════════════════════════
// beat 1: the root (voice-led register). beats 2-3: chord/scale motion.
// beat 4: a CHROMATIC APPROACH into the next bar's root — the walk's engine.
static int bass_near(int pc) { return rad_bass_to(pc, bassLast, 26, 45); }
static int walk_note(long bar, int beat) {
    int f = fn_at(bar);
    int root = bass_near(root_pc(f));
    if (beat == 0) { bassLast = root; return root; }
    if (beat == 3) {                                // approach the NEXT root
        int nr = bass_near(root_pc(fn_at(bar + 1)));
        int ap = nr + (chance(55) ? -1 : 1);        // from below or above
        bassLast = ap;
        return ap;
    }
    int third = root + (qual(f) == Q_MAJ7 || qual(f) == Q_DOM7 ? 4 : 3);
    int fifth = root + (qual(f) == Q_M7B5 ? 6 : 7);
    int n = (beat == 1) ? (chance(60) ? third : root + 2)     // 3rd or a scale step
                        : (chance(60) ? fifth : third);       // 5th on beat 3
    while (n > 45) n -= 12;
    bassLast = n;
    return n;
}

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;
    int  f    = fn_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  sw   = swing_ms(step);
    bool quiet = sect == S_BASS;                    // the room leans in

    // ── THE BRUSHES — ride carries the time: ding, ding-ga-ding ──
    if (bar >= 1 && !(sect == S_OUTRO && bar % 8 >= 6)) {
        if (step % 4 == 0 && !quiet)
            { schedule_hit(dly + 1, 81, SL_RIDE, step % 8 == 0 ? 3 : 2, 220); vu += 0.8f; }
        if ((step == 6 || step == 14) && !quiet)    // the skip note, swung
            schedule_hit(dly + sw + 1, 81, SL_RIDE, 2, 120);
        if (step == 4 || step == 12)                // the hat snaps 2 and 4
            { schedule_hit(dly + 1, 60, SL_HAT, quiet ? 2 : 3, 40); vu += 0.7f; }
        if (!quiet && step == 0 && (bar % 2 == 0) && lvl >= 1)
            schedule_hit(dly + 2, 70, SL_BRSH, 1, (int)(stepMs * 7));   // the sweep
        if (!quiet && lvl >= 2 && step % 4 == 0 && chance(30))
            schedule_hit(dly + 1, 33, SL_KICK, 1, 60);                  // feathered
    }

    // ── THE WALK — four quarters a bar, every bar, never the same ──
    if (bar >= 1 && step % 4 == 0) {
        int beat = step / 4;
        int n = walk_note(bar, beat);
        int vol = sect == S_BASS ? 6 : 4;           // the solo: the bass steps UP
        schedule_hit(dly + 6, n, I_BASS, vol, (int)(stepMs * 3.4));
        vu += 1.4f;
    }

    // ── PIANO COMPING — Charleston cells, rootless, out of the way ──
    if (bar >= 1 && sect != S_BASS && lvl >= 1) {
        static const int CELLS[3][2] = { { 0, 6 }, { 3, 10 }, { 6, 12 } };
        int cell = (int)((bar * 7 + 3) % 3);        // varies by bar, not rnd: stays seeded-ish
        if (step == CELLS[cell][0] || step == CELLS[cell][1]) {
            rad_lead_to(root_pc(f), QV[qual(f)], gvPno, 3, 57, 76, &pnoInit);
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + sw + k * 5 + rnd(3), gvPno[k], I_PNO,
                             sect == S_PIANO ? 3 : 4, 420);
            vu += 1.3f;
        }
    }
    // during the bass solo the piano just nods: one soft pad per 2 bars
    if (sect == S_BASS && step == 0 && bar % 2 == 0) {
        rad_lead_to(root_pc(f), QV[qual(f)], gvPno, 3, 57, 76, &pnoInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 9, gvPno[k], I_PNO, 2, (int)(stepMs * 14));
    }

    // ── THE HEAD — the tune, right hand, swung and a touch behind ──
    if ((sect == S_HEAD || sect == S_OUTRO) && bar >= 1) {
        int cs = (int)(s % 32);
        for (int i = 0; i < sng.melN; i++)
            if (sng.melOn[i] == cs) {
                int m = improv_deg_to_midi(sng.melDeg[i], 72 + sng.keyPc - 12, JAZZMAJ);
                while (m < 65) m += 12;
                while (m > 84) m -= 12;
                schedule_hit(dly + sw + 12 + rnd(5), m, I_PSOLO, 5, (int)(stepMs * 4));
                vu += 1.4f;
            }
    }

    // ── THE SOLOS — the shared improviser, two registers, two densities ──
    if (sect == S_PIANO || sect == S_BASS) {
        // piano solo spans sections 2-3 (16 bars) and section 5 (8 bars, a
        // second helping); the bass solo is section 4 (8 bars, sparser)
        long soloBar; int reg, len; float dens;
        if (sect == S_PIANO) {
            bool second = bar >= 40;
            soloBar = second ? bar - 40 : bar - 16;
            reg = 74; len = second ? 8 : 16; dens = 1.0f;
        } else {
            soloBar = bar - 32;
            reg = 45; len = 8; dens = 0.62f;        // the bass talks slower
        }
        if (soloBar == 0 && step == 0)
            improv_begin(&solo, reg, len, dens);
        if (step == 0 && bar % 2 == 0 && improv_due(&solo, soloBar))
            improv_render(&solo, soloBar, JAZZMAJ);
        float arc = improv_arc(&solo, soloBar);
        int cs = (int)(s % 32);
        for (int i = 0; i < solo.n; i++)
            if (solo.onset[i] == cs) {
                int m   = improv_midi(&solo, i, soloBar, sng.keyPc, JAZZMAJ, sng.blue);
                int vol = (sect == S_BASS ? 6 : 4) + (arc > 0.6f ? 1 : 0);
                int gat = (int)(stepMs * (solo.dur[i] > 4 ? solo.dur[i] : 2) * 0.85f);
                schedule_hit(dly + sw + 10, m, sect == S_BASS ? I_BASS : I_PSOLO,
                             vol, gat > 1600 ? 1600 : gat);
                vu += 1.5f;
            }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
// Each chair's candidates are full recipes — switching re-aims the slot from
// scratch, so a swap mid-song is clean. sel 0 is always the shipped sound.
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chPiano) {
        if (sel == 0) {
            instrument(I_PNO, INSTR_TRI, 2, 600, 2, 240);    // the felt grand
            instrument_env(I_PNO, 0, ENV_CUTOFF, 0, 70, 700);
        } else {
            instrument(I_PNO, INSTR_SINE, 3, 700, 2, 260);   // the closed-lid ballad set
            instrument_filter(I_PNO, FILTER_LOW, 1400, 1);
        }
    } else if (idx == chSolo) {
        if (sel == 0) {
            instrument(I_PSOLO, INSTR_TRI, 1, 500, 3, 220);  // the piano's solo stop
            instrument_env(I_PSOLO, 0, ENV_CUTOFF, 0, 60, 900);
        } else if (sel == 1) {
            instrument(I_PSOLO, INSTR_MALLET, 1, 0, 7, 1200); // the MJQ night
            instrument_harmonics(I_PSOLO, 0.25f);             //   (the vibes preset
            instrument_timbre(I_PSOLO, 0.50f);                //    from the mallet cart)
            instrument_morph(I_PSOLO, 0.85f);
        } else {
            instrument(I_PSOLO, INSTR_PLUCK, 1, 0, 7, 700);  // the Herb Ellis night
            instrument_harmonics(I_PSOLO, 0.50f);            //   warm archtop, neck pickup
            instrument_timbre(I_PSOLO, 0.45f);
            instrument_morph(I_PSOLO, 0.5f);
            instrument_filter(I_PSOLO, FILTER_LOW, 2300, 1);
        }
    } else if (idx == chBass) {
        if (sel == 0) {
            instrument(I_BASS, INSTR_TRI, 3, 300, 5, 110);   // the upright
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 16, 2);  // the thumb
        } else {
            instrument(I_BASS, INSTR_SINE, 4, 340, 5, 130);  // darker — gut strings
            instrument_filter(I_BASS, FILTER_LOW, 380, 1);
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 16, 2);
        }
    } else if (idx == chKit) {
        if (sel == 0) {
            instrument(SL_RIDE, INSTR_SQUARE, 1, 260, 0, 160);   // brushed ride ping
            instrument_filter(SL_RIDE, FILTER_HIGH, 5400, 3);
            instrument(SL_HAT, INSTR_NOISE, 0, 30, 0, 14);
            instrument_filter(SL_HAT, FILTER_HIGH, 8200, 3);
            instrument(SL_BRSH, INSTR_NOISE, 60, 300, 2, 200);   // the circular sweep
            instrument_filter(SL_BRSH, FILTER_HIGH, 4800, 1);
            instrument(SL_KICK, INSTR_SINE, 0, 150, 0, 60);      // feathered, felt
            instrument_filter(SL_KICK, FILTER_LOW, 220, 1);
            instrument_env(SL_KICK, 0, ENV_PITCH, 0, 35, 9.0f);
        } else {
            instrument(SL_RIDE, INSTR_SQUARE, 0, 320, 0, 180);   // the sticks night
            instrument_filter(SL_RIDE, FILTER_HIGH, 6400, 4);    //   ride rings brighter
            instrument(SL_HAT, INSTR_NOISE, 0, 22, 0, 12);       //   hat snaps tighter
            instrument_filter(SL_HAT, FILTER_HIGH, 9000, 3);
            instrument(SL_BRSH, INSTR_NOISE, 2, 60, 0, 40);      //   the sweep becomes a tap
            instrument_filter(SL_BRSH, FILTER_BAND, 3200, 4);
            instrument(SL_KICK, INSTR_SINE, 0, 170, 0, 60);      //   kick sits forward
            instrument_filter(SL_KICK, FILTER_LOW, 300, 1);
            instrument_env(SL_KICK, 0, ENV_PITCH, 0, 38, 11.0f);
        }
    }
}

// re-assert any non-default chair (new_song re-rolls filters over the slots;
// default chairs keep the trio's per-song roll, picked chairs win over it)
static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}

static void setup_instruments(void) {
    chPiano = rad_chair(&band, "piano", "felt grand", "closed lid", NULL, NULL);
    chSolo  = rad_chair(&band, "solo chair", "piano", "vibes", "guitar", NULL);
    chBass  = rad_chair(&band, "bass", "upright", "gut strings", NULL, NULL);
    chKit   = rad_chair(&band, "drums", "brushes", "sticks", NULL, NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_PNO,   FILTER_LOW, (int)(2200 * tm), 1);
    instrument_filter(I_PSOLO, FILTER_LOW, (int)(2700 * tm), 1);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (COCKTAIL_SEED) { new_song(pos, COCKTAIL_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 92, 152, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) { note_off_all(); sfx(-1); }
        else scheduled = (long)pos;
    }
    if (ev & RAD_EV_TONE) {
        apply_tone();
    }

    int chair = rad_band_input(&band, &showHelp);   // THE BAND — B, then click/number
    if (chair >= 0) apply_chair(chair);

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= 64L * 16) fresh_song(pos);
        long bar = songStep >= 0 ? songStep / 16 : 0;
        for (int i = 0; i < 4; i++)
            chord_label(nowChord[i], 12, fn_at((bar / 4) * 4 + i));
    }

    vu *= 0.88f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("chord", "%s", nowChord[(int)(tbar % 4)]);
    watch("phrase", "%d", solo.phraseNo);
    watch("blue", "%d", sng.blue);
#endif
}

// ── draw — the lounge chassis; window art = the bar at last call ──────────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_DARKER_PURPLE, CLR_PEACH);       // velvet shell, brass trim
    rad_dial(sng.freq, CLR_PEACH);

    // the window — the bar at last call: city bokeh, a neon martini
    rectfill(34, 52, 102, 116, CLR_BROWNISH_BLACK);
    for (int i = 0; i < 12; i++) {                           // bokeh lights
        int bx = 40 + (i * 31) % 90, by = 58 + (i * 17) % 36;
        circfill(bx, by, (i % 3) ? 1 : 2,
                 (i % 4 == 0) ? CLR_DARK_ORANGE : (i % 4 == 1) ? CLR_MAUVE : CLR_DARKER_GREY);
    }
    {   // the martini, neon
        int cx = 85, cy = 100;
        line(cx - 16, cy - 14, cx + 16, cy - 14, CLR_PEACH);           // rim
        line(cx - 16, cy - 14, cx, cy + 4, CLR_PEACH);                 // bowl
        line(cx + 16, cy - 14, cx, cy + 4, CLR_PEACH);
        line(cx, cy + 4, cx, cy + 26, CLR_PEACH);                      // stem
        line(cx - 8, cy + 27, cx + 8, cy + 27, CLR_PEACH);             // foot
        circfill(cx - 4, cy - 9, 2, CLR_LIME_GREEN);                   // the olive
        line(cx - 4, cy - 11, cx + 4, cy - 17, CLR_LIGHT_GREY);        // the pick
        // it shimmers on the ride hits
        if (radioOn && vu > 4 && frame() % 8 < 4)
            line(cx - 13, cy - 16, cx - 7, cy - 16, CLR_WHITE);
    }
    // piano keys along the sill
    for (int k = 0; k < 14; k++) {
        rectfill(36 + k * 7, 152, 6, 14, CLR_LIGHT_GREY);
        if (k % 7 != 2 && k % 7 != 6)
            rectfill(40 + k * 7, 152, 3, 8, CLR_BLACK);
    }
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_PEACH);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_PEACH);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s", sng.freq, sng.ballad ? "ballad" : "swing");
        print(l2, 154, 70, CLR_MAUVE);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_MAUVE);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_PEACH);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        rad_chord_row(nowChord, 4, (int)(bar % 4), 152, 104, CLR_PEACH);
        print(SECTNAME[sect], 152, 120, CLR_PEACH);
        rad_phrase_dots(232, 124, 8, bar / 8, CLR_PEACH);
    }

    static const char *FEEL[4] = { "last call", "set one", "set two", "burner" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_PEACH);
    if (rad_knob_int(&tempo, 92, 152, 2, 218, 148, 9, "tempo", CLR_PEACH)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_PEACH)) apply_tone();
    rad_power_led(radioOn, CLR_PEACH, CLR_MAUVE);

    rad_help_button(CLR_PEACH);
    rad_band_button(CLR_PEACH);

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tune (rolls a new seed)" },
            { "R",          "same tune - the solos are new" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - last call ... burner" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "B",          "the band - swap chairs live" },
        };
        static const char *NOTES[3] = {
            "walking bass: root, motion, CHROMATIC",
            "approach - never repeats. the bass takes",
            "a solo too. pin: COCKTAIL_SEED 0x...",
        };
        rad_help_panel("COCKTAIL RADIO", HELP, 8, NOTES, 3, CLR_PEACH);
    }
    rad_band_panel(&band, CLR_PEACH);
    ui_end();
}
