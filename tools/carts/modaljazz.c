/* de:meta
{
  "slug": "modaljazz",
  "collection": ["radio"],
  "title": "modal jazz radio",
  "status": "active",
  "created": "2026-07-21",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "chord-voicing",
    "swing-timing"
  ],
  "lineage": "The first radio station to GENERATE over a modal vocab from the shared harmony brain (runtime/harmony.h HB_DORIAN_STYLE) - the second consumer of the church-mode vocabs after the chordwise analyzer, the proof the brain is genuinely shared. Chassis copied from cocktail.c (piano-trio + improv.h soloist); the band and the modal chord brain are new.",
  "homage": "Miles Davis - So What / Kind of Blue (modal jazz)",
  "description": "An endless modal-jazz set - the Miles Davis 'So What' homage, and the first station that GENERATES its changes over a MODE instead of functional chords. The chord brain is runtime/harmony.h's HB_DORIAN_STYLE: it floats the i<->IV dorian vamp (that bright major IV is the whole tell), one chord held two bars so nothing's in a hurry, coming home to i at the top of each chorus - the same shared brain the chordwise analyzer names chords with, here running FORWARD to compose. Over an AABA form it lifts a half-step for the B section (the 'So What' key bump: Dm up to Ebm) and settles back. The band: a Harmon-muted trumpet lead (improv.h solos, spare and behind the beat), a Rhodes comping the rootless 3-7-9 voicings, a walking upright bass (chord tone on 1, motion through the middle, chromatic approach into the next root), and brushes at a whisper (swung ride ding ding-ga-ding, hat on 2 and 4, feathered kick). Set form: head, trumpet solo, Rhodes solo, trumpet trades back, head out - R replays the tune, the solos are always new. SPACE next tune, [ ] history, LEFT/RIGHT feel, UP/DOWN tempo, T tone, M power, B band (swap chairs live), H help."
}
de:meta */
// ── MODAL JAZZ RADIO ──────────────────────────────────────────────────────
// The first station that GENERATES over a MODE. Every other station's chord
// brain moves functionally (ii-V-I, secondary dominants, the pull home); this
// one FLOATS. The harmony is runtime/harmony.h's HB_DORIAN_STYLE run forward:
// the i<->IV dorian vamp (the bright MAJOR IV is dorian's whole identity),
// held two bars a chord, coming home to i at the top of each chorus. It's the
// second customer of the church-mode vocabs (chordwise ANALYZES with them; this
// COMPOSES with them) — the proof the brain is genuinely shared, not one-cart.
//
// The "So What" move: over an AABA form the B section lifts a half-step (Dm ->
// Ebm) and drops back. Miles' record in one gesture.
//
// The band (all new — the chassis is cocktail.c, the voices are not):
//   • a Harmon-MUTED TRUMPET lead — improv.h solos, spare, behind the beat
//   • a RHODES comping the rootless 3-7-9 voicings (rad_lead_to)
//   • a WALKING UPRIGHT — chord tone on 1, motion, chromatic approach on 4
//   • BRUSHES at a whisper — swung ride, hat on 2 and 4, feathered kick
//
//   SPACE next tune   R play it again   [ ] history   M radio on/off
//   LEFT/RIGHT feel   UP/DOWN tempo   T tone   B band   H or ? help

#include "studio.h"
#include "radio.h"
#include "harmony.h"   // the shared brain — HB_DORIAN_STYLE runs forward here
#include "improv.h"
#include <stdio.h>
#include <math.h>

#define MODALJAZZ_SEED 0   // pin a favourite set here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_TPT   5    // the muted trumpet — the head + the lead solo
#define I_RHO   6    // the Rhodes — comping, and its own solo section
#define I_BASS  7    // the upright — walking
#define SL_RIDE 8    // the brushes
#define SL_HAT  9
#define SL_BRSH 10   // the brush sweep
#define SL_KICK 11   // feathered, barely there

// ── harmony — the DORIAN vamp, one chord held two bars (modal = slow) ──────
// qualities live in harmony.h (HBQ_*); QV is the rootless comp voicing per
// quality (3rd / 7th / colour — the Rhodes leaves the root to the bass).
#define QN hb_qname
static const int QV[HB_NQUAL][3] = {
    { 4, 11, 14 },   // maj7 — 3 7 9
    { 3, 10, 14 },   // m7
    { 4, 10, 14 },   // 7
    { 3,  6, 10 },   // m7b5
    { 3,  9, 14 },   // m6
};
// the dorian functions come from harmony.h's HB_DORIAN vocab (HBd_*); the
// STYLE (HB_DORIAN_STYLE) is the weighted next-chord table we run forward.
#define D_OFF  (HB_DORIAN.off)
#define D_QUAL (HB_DORIAN.qual)
static const int DORIAN[7] = { 0, 2, 3, 5, 7, 9, 10 };   // the solos' scale

// ── the form — a modal set, AABA choruses ──────────────────────────────────
enum { S_HEAD, S_TPT, S_RHO, S_OUTRO };
static const int FORM[8] = { S_HEAD, S_HEAD, S_TPT, S_TPT,
                             S_RHO, S_TPT, S_HEAD, S_OUTRO };
static const char *SECTNAME[4] = { "head", "trumpet solo", "rhodes solo", "outro" };

// ── the generated song ──────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    int  fn[32];               // the dorian walk, one function per TWO bars
    int  lift;                 // the "So What" B-section key bump (semitones)
    int  blue;                 // 8..30 — Miles bends
    int  melOn[6], melN, melDeg[6];   // the head (32-step cell)
    int  ballad;               // a slow set vs a medium-up one
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

static int    tempo     = 132;
static int    intensity = 1;
static bool   radioOn   = true;
static bool   showHelp  = false;
static int    toneSel   = 2;
static int    songCount = 0;
static float  vu        = 0;
static int    bassLast  = 33;
static char   nowChord[4][12];

static int  gvRho[3] = { 62, 65, 69 };
static bool rhoInit = false;
static Improv solo;

static RadBand band;
static int chLead, chComp, chBass, chKit;

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);

// ── song generation ───────────────────────────────────────────────────────
static const char *TW1[] = { "Blue", "Modal", "Kind", "Cool", "Freddie", "So",
    "Milestone", "Flamenco", "Nardis", "Green", "Cannon", "Sketch" };
static const char *TW2[] = { "Mode", "What", "Sketches", "Freeway", "Blues", "Miles",
    "Dolphy", "Corner", "Freedom", "Nights", "Ballad", "Dorian" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.keyPc  = srnd(12);
    sng.ballad = srnd(100) < 30;
    sng.blue   = 8 + srnd(23);
    sng.lift   = srnd(100) < 70 ? 1 : 2;    // the So What bump: usually a half step

    // pre-roll the WHOLE walk (32 two-bar slots = 64 bars): the modal brain run
    // FORWARD. It floats i<->IV<->VII on its own weights; we only reset home to
    // the tonic at the top of each 32-bar (16-slot) AABA chorus.
    int f = HBd_i;
    for (int s = 0; s < 32; s++) {
        if (s % 16 == 0) f = HBd_i;
        else             f = hb_pick(&HB_DORIAN_STYLE, f, srnd(hb_nopts(&HB_DORIAN_STYLE, f)));
        sng.fn[s] = f;
    }

    // the head: a seeded cell, low dorian degrees (the tune you can hum)
    sng.melN = 0;
    static const int HC[10] = { 0, 3, 6, 10, 13, 16, 19, 22, 26, 28 };
    for (int i = 0; i < 10 && sng.melN < 5; i++)
        if (srnd(100) < 38) sng.melOn[sng.melN++] = HC[i];
    if (sng.melN < 3) { sng.melN = 3; sng.melOn[0] = 0; sng.melOn[1] = 10; sng.melOn[2] = 19; }
    for (int i = 0; i < sng.melN; i++) sng.melDeg[i] = srnd(7);

    // the band's per-song roll: trumpet air, Rhodes bark, bass darkness
    instrument_filter(I_TPT,  FILTER_LOW, 2400 + srnd(600), 2);
    instrument_filter(I_RHO,  FILTER_LOW, 2100 + srnd(600), 1);
    instrument_filter(I_BASS, FILTER_LOW, 420 + srnd(160), 1);

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = sng.ballad ? 76 + srnd(28) : 120 + srnd(40);   // 76..104 / 120..160
    bpm(tempo);
    apply_band_overrides();
    songBase = (long)pos + 8;
    rhoInit = false;
    bassLast = 33;
    songCount++;
}

static void fresh_song(double pos) {
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ──────────────────────────────────────────────────────────
static int sect_of(long bar) { long x = bar / 8; return (int)(x < 8 ? FORM[x] : S_OUTRO); }
static int fn_at(long bar)   { return sng.fn[(bar < 0 ? 0 : bar / 2) % 32]; }   // 2 bars/chord

// the "So What" key: the B of each AABA (bars 16-23 of every 32) lifts up
static int bar_key(long bar) {
    long cb = (bar < 0 ? 0 : bar) % 32;
    return (sng.keyPc + (cb >= 16 && cb < 24 ? sng.lift : 0)) % 12;
}
static int root_pc(long bar, int f) { return (bar_key(bar) + D_OFF[f]) % 12; }
static int qual(int f)              { return D_QUAL[f]; }

// the current chord as four pitch classes (R 3 5 7) — the solo's snap targets,
// spelled off the DORIAN vocab (NOT the major hb_chord_pcs).
static int chord_pcs(long bar, int f, int *out) {
    return hb_vocab_pcs(&HB_DORIAN, bar_key(bar), f, out);
}
static void chord_label(char *out, int n, long bar, int f) {
    snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(bar, f)], QN[qual(f)]);
}

static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_OUTRO) ? 0 : 1;
    return rad_level(base, intensity);
}

// the swing: every off-beat 8th lands at 62%, not 50%
static int swing_ms(int step) {
    return (step % 4 == 2) ? (int)(stepMs * 4 * (0.62 - 0.50)) : 0;
}

// ══ THE WALKING BASS ══════════════════════════════════════════════════════
// beat 1: the root (voice-led register). beats 2-3: chord/scale motion.
// beat 4: a CHROMATIC APPROACH into the next bar's root — the walk's engine.
static int bass_near(int pc) { return rad_bass_to(pc, bassLast, 26, 45); }
static int walk_note(long bar, int beat) {
    int f = fn_at(bar);
    int root = bass_near(root_pc(bar, f));
    if (beat == 0) { bassLast = root; return root; }
    if (beat == 3) {                                // approach the NEXT root
        int nr = bass_near(root_pc(bar + 1, fn_at(bar + 1)));
        int ap = nr + (chance(55) ? -1 : 1);
        bassLast = ap;
        return ap;
    }
    int third = root + (qual(f) == HBQ_MAJ7 || qual(f) == HBQ_DOM7 ? 4 : 3);
    int fifth = root + (qual(f) == HBQ_M7B5 ? 6 : 7);
    int n = (beat == 1) ? (chance(60) ? third : root + 2)
                        : (chance(60) ? fifth : third);
    while (n > 45) n -= 12;
    bassLast = n;
    return n;
}

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    if (bar >= 64) return;
    int  f    = fn_at(bar);
    int  sect = sect_of(bar);
    int  lvl  = level_of(bar);
    int  sw   = swing_ms(step);
    bool quiet = sect == S_RHO;                     // the room leans in for the Rhodes

    // ── THE BRUSHES — ride carries the time: ding, ding-ga-ding ──
    if (bar >= 1 && !(sect == S_OUTRO && bar % 8 >= 6)) {
        if (step % 4 == 0)
            { schedule_hit(dly + 1, 81, SL_RIDE, step % 8 == 0 ? 3 : 2, 220); vu += 0.8f; }
        if (step == 6 || step == 14)                // the skip note, swung
            schedule_hit(dly + sw + 1, 81, SL_RIDE, 2, 120);
        if (step == 4 || step == 12)                // the hat snaps 2 and 4
            { schedule_hit(dly + 1, 60, SL_HAT, 3, 40); vu += 0.7f; }
        if (step == 0 && (bar % 2 == 0) && lvl >= 1)
            schedule_hit(dly + 2, 70, SL_BRSH, 1, (int)(stepMs * 7));   // the sweep
        if (lvl >= 2 && step % 4 == 0 && chance(30))
            schedule_hit(dly + 1, 33, SL_KICK, 1, 60);                  // feathered
    }

    // ── THE WALK — four quarters a bar, every bar ──
    if (bar >= 1 && step % 4 == 0) {
        int beat = step / 4;
        int n = walk_note(bar, beat);
        schedule_hit(dly + 6, n, I_BASS, 4, (int)(stepMs * 3.4));
        vu += 1.4f;
    }

    // ── THE RHODES — comping cells, rootless, out of the way ──
    if (bar >= 1 && lvl >= 1) {
        static const int CELLS[3][2] = { { 0, 6 }, { 3, 10 }, { 6, 12 } };
        int cell = (int)((bar * 7 + 3) % 3);
        bool go = (step == CELLS[cell][0] || step == CELLS[cell][1]);
        if (sect == S_RHO) go = (step == 0 || step == 8);   // the Rhodes solo: sparse pad
        if (go) {
            rad_lead_to(root_pc(bar, f), QV[qual(f)], gvRho, 3, 57, 76, &rhoInit);
            for (int k = 0; k < 3; k++)
                schedule_hit(dly + sw + k * 5 + rnd(3), gvRho[k], I_RHO,
                             sect == S_RHO ? 2 : 4, sect == S_RHO ? (int)(stepMs * 10) : 420);
            vu += 1.3f;
        }
    }

    // ── THE HEAD — the tune, muted trumpet, swung and a touch behind ──
    if ((sect == S_HEAD || sect == S_OUTRO) && bar >= 1) {
        int cs = (int)(s % 32);
        for (int i = 0; i < sng.melN; i++)
            if (sng.melOn[i] == cs) {
                int m = improv_deg_to_midi(sng.melDeg[i], 72 + bar_key(bar) - 12, DORIAN);
                while (m < 63) m += 12;
                while (m > 82) m -= 12;
                schedule_hit(dly + sw + 12 + rnd(5), m, I_TPT, 5, (int)(stepMs * 4));
                vu += 1.4f;
            }
    }

    // ── THE SOLOS — the shared improviser: trumpet lead, then Rhodes ──
    if (sect == S_TPT || sect == S_RHO) {
        long soloBar; int reg, len; float dens;
        if (sect == S_TPT) {
            bool second = bar >= 40;
            soloBar = second ? bar - 40 : bar - 16;
            reg = 70; len = second ? 8 : 16; dens = 0.9f;    // trumpet: spare, lots of air
        } else {
            soloBar = bar - 32;
            reg = 64; len = 8; dens = 1.0f;                  // Rhodes: busier, mid register
        }
        if (soloBar == 0 && step == 0)
            improv_begin(&solo, reg, len, dens);
        if (step == 0 && bar % 2 == 0 && improv_due(&solo, soloBar))
            improv_render(&solo, soloBar, DORIAN);
        float arc = improv_arc(&solo, soloBar);
        int cs = (int)(s % 32);
        int cpc[4]; int ncp = chord_pcs(bar, fn_at(bar), cpc);
        for (int i = 0; i < solo.n; i++)
            if (solo.onset[i] == cs) {
                int m   = improv_midi_chord(&solo, i, soloBar, bar_key(bar), DORIAN,
                                            sng.blue, cpc, ncp, IMPROV_SNAP_RESOLVE);
                int vol = 4 + (arc > 0.6f ? 1 : 0);
                int gat = (int)(stepMs * (solo.dur[i] > 4 ? solo.dur[i] : 2) * 0.85f);
                schedule_hit(dly + sw + 10, m, sect == S_TPT ? I_TPT : I_RHO,
                             vol, gat > 1600 ? 1600 : gat);
                vu += 1.5f;
            }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chLead) {
        if (sel == 0) {                                       // the Harmon-muted trumpet
            instrument(I_TPT, INSTR_BRASS, 1, 0, 4, 1100);
            instrument_harmonics(I_TPT, 0.15f);
            instrument_timbre(I_TPT, 0.55f);                  // buzzy, a touch nasal
            instrument_morph(I_TPT, 0.42f);
            instrument_filter(I_TPT, FILTER_BAND, 1700, 2);   // the mute: thin, vocal
        } else if (sel == 1) {                                // open bell — the horn rings
            instrument(I_TPT, INSTR_BRASS, 1, 0, 4, 1200);
            instrument_harmonics(I_TPT, 0.15f);
            instrument_timbre(I_TPT, 0.60f);
            instrument_morph(I_TPT, 0.42f);
            instrument_filter(I_TPT, FILTER_LOW, 3600, 1);
        } else {                                              // flugel — dark, round
            instrument(I_TPT, INSTR_BRASS, 2, 0, 4, 1300);
            instrument_harmonics(I_TPT, 0.46f);
            instrument_timbre(I_TPT, 0.30f);
            instrument_morph(I_TPT, 0.42f);
            instrument_filter(I_TPT, FILTER_LOW, 2600, 1);
        }
    } else if (idx == chComp) {
        if (sel == 0) {                                       // the Rhodes
            instrument(I_RHO, INSTR_EPIANO, 2, 0, 6, 900);
            instrument_harmonics(I_RHO, 0.15f);
            instrument_timbre(I_RHO, 0.40f);
            instrument_morph(I_RHO, 0.30f);
            instrument_filter(I_RHO, FILTER_LOW, 2400, 2);
        } else {                                              // the Wurli — barkier
            instrument(I_RHO, INSTR_EPIANO, 2, 0, 6, 700);
            instrument_harmonics(I_RHO, 0.50f);
            instrument_timbre(I_RHO, 0.55f);
            instrument_morph(I_RHO, 0.35f);
            instrument_filter(I_RHO, FILTER_LOW, 2600, 2);
        }
    } else if (idx == chBass) {
        if (sel == 0) {
            instrument(I_BASS, INSTR_TRI, 3, 300, 5, 110);    // the upright
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 16, 2);   // the thumb
        } else {
            instrument(I_BASS, INSTR_SINE, 4, 340, 5, 130);   // gut strings — darker
            instrument_filter(I_BASS, FILTER_LOW, 380, 1);
            instrument_env(I_BASS, 0, ENV_PITCH, 0, 16, 2);
        }
    } else if (idx == chKit) {
        if (sel == 0) {                                       // the brushes
            instrument(SL_RIDE, INSTR_SQUARE, 1, 260, 0, 160);
            instrument_filter(SL_RIDE, FILTER_HIGH, 5400, 3);
            instrument(SL_HAT, INSTR_NOISE, 0, 30, 0, 14);
            instrument_filter(SL_HAT, FILTER_HIGH, 8200, 3);
            instrument(SL_BRSH, INSTR_NOISE, 60, 300, 2, 200);
            instrument_filter(SL_BRSH, FILTER_HIGH, 4800, 1);
            instrument(SL_KICK, INSTR_SINE, 0, 150, 0, 60);
            instrument_filter(SL_KICK, FILTER_LOW, 220, 1);
            instrument_env(SL_KICK, 0, ENV_PITCH, 0, 35, 9.0f);
        } else {                                              // the sticks night
            instrument(SL_RIDE, INSTR_SQUARE, 0, 320, 0, 180);
            instrument_filter(SL_RIDE, FILTER_HIGH, 6400, 4);
            instrument(SL_HAT, INSTR_NOISE, 0, 22, 0, 12);
            instrument_filter(SL_HAT, FILTER_HIGH, 9000, 3);
            instrument(SL_BRSH, INSTR_NOISE, 2, 60, 0, 40);
            instrument_filter(SL_BRSH, FILTER_BAND, 3200, 4);
            instrument(SL_KICK, INSTR_SINE, 0, 170, 0, 60);
            instrument_filter(SL_KICK, FILTER_LOW, 300, 1);
            instrument_env(SL_KICK, 0, ENV_PITCH, 0, 38, 11.0f);
        }
    }
}

static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}

static void setup_instruments(void) {
    chLead = rad_chair(&band, "lead", "muted trumpet", "open bell", "flugel", NULL);
    chComp = rad_chair(&band, "comp", "rhodes", "wurli", NULL, NULL);
    chBass = rad_chair(&band, "bass", "upright", "gut strings", NULL, NULL);
    chKit  = rad_chair(&band, "drums", "brushes", "sticks", NULL, NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_TPT, FILTER_BAND, (int)(1700 * tm), 2);
    instrument_filter(I_RHO, FILTER_LOW,  (int)(2400 * tm), 2);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (MODALJAZZ_SEED) { new_song(pos, MODALJAZZ_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 76, 168, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, sng.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER)  {
        if (!radioOn) { note_off_all(); sfx(-1); }
        else scheduled = (long)pos;
    }
    if (ev & RAD_EV_TONE) apply_tone();

    int chair = rad_band_input(&band, &showHelp);
    if (chair >= 0) apply_chair(chair);

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);

        long songStep = scheduled - songBase;
        if (songStep >= 64L * 16) fresh_song(pos);
        long bar = songStep >= 0 ? songStep / 16 : 0;
        for (int i = 0; i < 4; i++) {
            long b = (bar / 4) * 4 + i;
            chord_label(nowChord[i], 12, b, fn_at(b));
        }
    }

    vu *= 0.88f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("chord", "%s", nowChord[(int)(tbar % 4)]);
    watch("lift", "%d", (int)((tbar % 32) >= 16 && (tbar % 32) < 24 ? sng.lift : 0));
    watch("phrase", "%d", solo.phraseNo);
#endif
}

// ── draw — the cool-blue chassis; window art = a spotlit horn ─────────────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_DARKER_BLUE, CLR_BLUE);          // midnight shell, cool-blue trim
    rad_dial(sng.freq, CLR_BLUE);

    // the window — a spotlit stage, Kind of Blue
    rectfill(34, 52, 102, 116, CLR_DARK_BLUE);
    {   // the spotlight cone from up-left
        for (int y = 0; y < 100; y++) {
            int half = 4 + y * 26 / 100;
            int cx = 74;
            int col = (y < 40) ? CLR_TRUE_BLUE : CLR_DARK_BLUE;
            if (radioOn && vu > 3) line(cx - half, 58 + y, cx + half, 58 + y,
                                        y % 2 ? CLR_TRUE_BLUE : col);
            else if (y % 3 == 0) line(cx - half, 58 + y, cx + half, 58 + y, CLR_DARK_BLUE);
        }
    }
    {   // a muted-trumpet silhouette in the light
        int hx = 60, hy = 118;
        line(hx, hy, hx + 22, hy - 6, CLR_BLACK);          // the lead pipe
        line(hx, hy + 1, hx + 22, hy - 5, CLR_BLACK);
        for (int r = 0; r < 8; r++)                          // the bell, flaring
            line(hx + 22, hy - 6 - r, hx + 30, hy - 12 - r * 2, CLR_BLACK);
        rectfill(hx + 4, hy - 4, 12, 3, CLR_BLACK);         // the valve casing
        for (int v = 0; v < 3; v++)                          // the valves
            rectfill(hx + 5 + v * 4, hy - 8, 2, 4, CLR_DARKER_BLUE);
        // it glints on the loud hits
        if (radioOn && vu > 5 && frame() % 8 < 4)
            circfill(hx + 29, hy - 20, 1, CLR_WHITE);
    }
    // rising notes when the horn's up
    if (radioOn && (sect == S_TPT || sect == S_HEAD)) {
        for (int i = 0; i < 3; i++) {
            int ny = 110 - (frame() / 2 + i * 20) % 56;
            int nx = 96 + (i * 13 + frame() / 8) % 24;
            print("*", nx, ny, CLR_TRUE_BLUE);
        }
    }
    rect(34, 52, 102, 116, CLR_TRUE_BLUE);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_BLUE);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_BLUE);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s", sng.freq, sng.ballad ? "ballad" : "swing");
        print(l2, 154, 70, CLR_INDIGO);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_INDIGO);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_BLUE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        rad_chord_row(nowChord, 4, (int)(bar % 4), 152, 104, CLR_BLUE);
        print(SECTNAME[sect], 152, 120, CLR_BLUE);
        rad_phrase_dots(232, 124, 8, bar / 8, CLR_BLUE);
    }

    static const char *FEEL[4] = { "hush", "the set", "cooking", "burner" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_BLUE);
    if (rad_knob_int(&tempo, 76, 168, 2, 218, 148, 9, "tempo", CLR_BLUE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_BLUE)) apply_tone();
    rad_power_led(radioOn, CLR_BLUE, CLR_INDIGO);

    rad_help_button(CLR_BLUE);
    rad_band_button(CLR_BLUE);

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tune (rolls a new seed)" },
            { "R",          "same tune - the solos are new" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - hush ... burner" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "B",          "the band - swap chairs live" },
        };
        static const char *NOTES[3] = {
            "the FIRST station to GENERATE over a",
            "MODE: the dorian i<->IV vamp (harmony.h",
            "HB_DORIAN_STYLE), with the So What lift.",
        };
        rad_help_panel("MODAL JAZZ RADIO", HELP, 8, NOTES, 3, CLR_BLUE);
    }
    rad_band_panel(&band, CLR_BLUE);
    ui_end();
}

// ── spec — the modal soloist's deterministic oracle (spec-harness.md) ───────
// Solos run on rnd() (performance, never pinned), so we don't assert pitches;
// we assert the PURE bridge: improv.h's chord-snap math + the DORIAN chord-tone
// feed (the proof this really spells the mode, not the major vocab).
#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    improv_selfcheck();

    sng.keyPc = 0; sng.lift = 1;                 // work in C dorian
    int cpc[4]; int n = chord_pcs(0, HBd_i, cpc);
    expect_eq(n, 4, "chord_pcs: four tones");
    expect(cpc[0]==0 && cpc[1]==3 && cpc[2]==7 && cpc[3]==10,
           "dorian i in C = Cm7 (C Eb G Bb)");
    chord_pcs(0, HBd_IV, cpc);
    expect(cpc[0]==5 && cpc[1]==9 && cpc[2]==0 && cpc[3]==3,
           "dorian IV in C = F7, the BRIGHT major IV (F A C Eb)");
    // the So What lift: the B section (bar 16) sounds a half-step up
    expect_eq(bar_key(0), 0,  "bar 0 in C = key 0");
    expect_eq(bar_key(16), 1, "bar 16 (the B section) lifts to Db (the So What bump)");
    // end to end: a solo E (64) over the i chord snaps to Eb, the b3 (nearest tone)
    n = chord_pcs(0, HBd_i, cpc);
    expect_eq(improv_snap(64, cpc, 4), 63, "E over Cm7 snaps to Eb (the minor 3rd)");
}
#endif
