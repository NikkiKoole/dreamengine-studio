// ── ADDIS RADIO ───────────────────────────────────────────────────────────
// Swinging Addis: the ethio-jazz station — Mulatu Astatke, Mahmoud Ahmed,
// the 1969-74 Amha/Kaifa golden age. Hypnotic minor vamps, a vibraphone lead
// over Wurlitzer comping, congas instead of a trap kit, and a horn line that
// answers the vibes in unison.
//
// Three firsts on this station:
//   • SCALES AS DATA — the first station to leave the major/minor world. The
//     seed rolls one of four Ethiopian qñit (modes): tizita, bati, anchihoye,
//     ambassel — pentatonics whose big intervals sound like nothing else on
//     the dial. Every melodic voice (head, bass ostinato, both solos) is built
//     by walking the same qñit table — the MODE is the song's identity, not a
//     chord progression. (improv.h's mode7 contract eats any 7-entry scale.)
//   • THE MODAL VAMP — chord-brain variant #N: no ii-V changes, just a tonic
//     drone (and sometimes a second center, i↔iv / i↔bII) held for eight bars
//     while the bass ostinato hypnotizes. "One chord for eleven minutes."
//   • FIVE ENGINES IN ONE BAND — vibes = INSTR_MALLET, Wurli/organ = EPIANO/
//     ORGAN, the horn = INSTR_PD (synth-brass, the engine's nearest thing to a
//     reedy horn — no breath model exists), bass = INSTR_FM, and the congas /
//     bongos / kebero are all INSTR_MEMBRANE (the struck-drumhead engine doing
//     exactly what it shipped for). MEMBRANE + PD, both 2026-06-08 engines,
//     earning their first station slot.
//
// Per-song roll (game-music.md "the same band every night"): the seed picks
// the qñit, the key, the vamp, the head cell, the bass ostinato, the timbres —
// and, the three axes that stop every tune sounding the same: the GROOVE (a
// sparse ballad / the syncopated swing / a driving 6/8 — the ear's first lock),
// the FORM (a short sketch / the full set / a long jam — so songs differ in
// length and shape, not just notes), and the TEMPO (coupled to the groove, so a
// ballad really is slower). Two SPACE presses = two records. The solos are
// PERFORMANCE (engine rnd): R replays the song, never the same solo twice.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (density curve)   UP/DOWN tempo   T tone   B band   H help

#include "studio.h"
#include "radio.h"
#include "improv.h"
#include <stdio.h>
#include <math.h>

#define ADDIS_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_VIBE   5   // vibraphone lead + solo — INSTR_MALLET, the star
#define I_KEYS   6   // Wurlitzer/organ drone comp — INSTR_EPIANO / ORGAN
#define I_HORN   7   // the horn line — INSTR_PD synth-brass (head unison + solo)
#define I_BASS   8   // electric/upright bass — INSTR_FM, the ostinato
// percussion — all INSTR_MEMBRANE except the shaker
#define SL_KEB   9   // kebero / low drum on the one
#define SL_CONGA 10  // open conga, the syncopated heart
#define SL_BONGO 11  // bongo / edge slap accents
#define SL_SHAK  12  // shaker 8ths — INSTR_NOISE

// ── the qñit (Ethiopian pentatonic modes) — SCALES AS DATA ────────────────
// Each is a 7-entry ASCENDING table so improv.h's mode7 contract is satisfied;
// every entry reduces mod-12 to one of the mode's five pitch classes, so the
// improviser can only ever land in-scale (the extra two slots repeat the first
// two an octave up). Stylized interval sets — the qñit are an oral tradition
// with regional variants; these capture each one's characteristic colour.
enum { Q_TIZITA, Q_BATI, Q_ANCHI, Q_AMBASSEL, NQ };
static const int QENET[NQ][7] = {
    { 0, 2, 4, 7,  9, 12, 14 },   // tizita     — nostalgic major pentatonic
    { 0, 2, 5, 7, 10, 12, 14 },   // bati       — suspended / modal, b7 colour
    { 0, 1, 4, 5,  8, 12, 13 },   // anchihoye  — augmented-2nd, exotic
    { 0, 1, 5, 7,  8, 12, 13 },   // ambassel   — phrygian-dark
};
static const char *QNAME[NQ] = { "tizita", "bati", "anchihoye", "ambassel" };
static const bool  QMINORISH[NQ] = { false, false, true, true };   // display tag

// ── the form — 8-bar sections; the seed rolls one of three arrangements so a
// tune isn't forever the same 64 bars. The sketch drops a solo (40 bars); the
// set is the classic head/solo/head shape (64); the jam DOUBLES each solo to
// 16 bars (88). Solo lengths are read off the form, so the tension arc stretches
// to fit. ───────────────────────────────────────────────────────────────────
enum { S_INTRO, S_HEAD, S_VSOLO, S_HSOLO, S_OUTRO };
#define MAXSECT 12
static const struct { int n; int s[MAXSECT]; const char *name; } FORMS[] = {
    { 5,  { S_INTRO, S_HEAD, S_VSOLO, S_HEAD, S_OUTRO },                                   "sketch" },
    { 8,  { S_INTRO, S_HEAD, S_HEAD, S_VSOLO, S_HEAD, S_HSOLO, S_HEAD, S_OUTRO },           "set" },
    { 11, { S_INTRO, S_HEAD, S_HEAD, S_VSOLO, S_VSOLO, S_HEAD,
            S_HSOLO, S_HSOLO, S_HEAD, S_HEAD, S_OUTRO },                                    "jam" },
};
#define NFORMS 3
static const char *SECTNAME[5] = { "intro", "head", "vibes solo", "horn solo", "outro" };

// ── the groove — the seed rolls the percussion feel (the ear locks onto this
// first): a sparse tizita ballad, the syncopated chik-chika swing, a driving 6/8.
enum { G_BALLAD, G_SWING, G_DRIVE, NG };
static const char *GNAME[NG] = { "ballad", "swing", "drive" };

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  qenet, keyPc;
    int  form, groove;         // rolled per song: the arrangement + the percussion feel
    int  secondOff;            // semitone offset of the 2nd vamp centre (qñit tone)
    int  vampKind;             // 0 = pure tonic drone · 1 = alt /8 bars · 2 = alt /4
    int  droneIv[3];           // the held keys voicing (3 qñit intervals)
    int  melOn[6], melDeg[6], melN;   // the head cell: onsets + qñit degrees (2-bar grid)
    int  melReg;
    int  bassOn[8], bassDeg[8], bassN;// the hypnotic ostinato (2-bar)
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 156.0 };
static Improv     solo;
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)

static int   tempo     = 96;
static int   intensity = 1;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 1;        // ethio-jazz wants warm, not bright
static int   songCount = 0;
static float vu        = 0;
static char  nowCtr[2][12];        // the vamp centre labels
static bool  hornOn    = true;     // the horn chair can switch off

static int  gvKeys[3] = { 55, 60, 64 };
static bool keysInit  = false;

// THE BAND (B) — three chairs: the lead, the keys, and the horn.
static RadBand band;
static int chLead, chKeys, chHorn;

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);

// ── song generation — composition AND the band, all from the seed ─────────
static const char *TW1[] = { "Tezeta", "Yegelle", "Addis", "Gold", "Selam", "Yene",
    "Heywete", "Almaz", "Ethio", "Soul", "Tizita", "Mela" };
static const char *TW2[] = { "Nights", "Groove", "Mood", "Vamp", "Hills", "Step",
    "Walk", "Dub", "Soul", "Dream", "Sun", "Sketch" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);

    sng.qenet = srnd(NQ);
    sng.keyPc = srnd(12);
    const int *q = QENET[sng.qenet];

    // the keys drone voicing — three characteristic qñit tones (a modal cluster)
    sng.droneIv[0] = q[2];
    sng.droneIv[1] = q[3];
    sng.droneIv[2] = q[4];

    // the vamp: a 2nd centre drawn from the qñit (the 4th if present, else 5th),
    // and how often it visits — most songs just drone the tonic
    sng.secondOff = q[2];                       // a qñit tone above the root
    if (q[2] == 4 || q[2] == 5) sng.secondOff = q[2]; // 3rd/4th colour
    sng.vampKind  = srnd(100) < 45 ? 0 : (srnd(100) < 60 ? 1 : 2);

    // the head cell: 3..5 lazy onsets in a 2-bar grid, qñit degrees
    sng.melN = 0;
    static const int MCAND[12] = { 0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30 };
    int dwalk = srnd(3);
    for (int i = 0; i < 12 && sng.melN < 5; i++)
        if (srnd(100) < 32) {
            sng.melOn[sng.melN] = MCAND[i];
            dwalk += srnd(3) - 1;               // small motion through the mode
            if (dwalk < 0) dwalk = 0;
            if (dwalk > 6) dwalk = 6;
            sng.melDeg[sng.melN] = dwalk;
            sng.melN++;
        }
    if (sng.melN < 3) {
        sng.melN = 3;
        sng.melOn[0]=0;  sng.melDeg[0]=4;
        sng.melOn[1]=11; sng.melDeg[1]=2;
        sng.melOn[2]=22; sng.melDeg[2]=0;
    }
    sng.melReg = 71 + srnd(6);                  // the vibes live up high

    // the bass ostinato — root-heavy, hypnotic; 4..6 hits in 2 bars, mostly
    // the root + a strong qñit tone, locked for the whole song
    sng.bassN = 0;
    static const int BCAND[10] = { 0, 4, 6, 8, 11, 16, 19, 22, 24, 28 };
    static const int BDEG[5]   = { 0, 0, 4, 0, 2 };  // root-leaning degrees
    for (int i = 0; i < 10 && sng.bassN < 7; i++)
        if (srnd(100) < 42) {
            sng.bassOn[sng.bassN]  = BCAND[i];
            sng.bassDeg[sng.bassN] = BDEG[srnd(5)];
            sng.bassN++;
        }
    if (sng.bassN < 3) {
        sng.bassN = 3;
        sng.bassOn[0]=0;  sng.bassDeg[0]=0;
        sng.bassOn[1]=8;  sng.bassDeg[1]=4;
        sng.bassOn[2]=22; sng.bassDeg[2]=0;
    }

    // THE BAND, rolled per song:
    instrument_harmonics(I_VIBE, 0.15f + srnd(24) * 0.01f);   // wood->silvery bar
    instrument_timbre(I_VIBE,    0.42f + srnd(28) * 0.01f);   // mallet hardness
    instrument_morph(I_VIBE,     0.55f + srnd(30) * 0.01f);   // ring length, motor
    instrument_timbre(I_KEYS,    0.35f + srnd(30) * 0.01f);   // Wurli brightness
    instrument_filter(I_BASS, FILTER_LOW, 420 + srnd(160), 2);

    snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    // the two big "different song" axes: the arrangement and the groove
    int fr     = srnd(100);
    sng.form   = fr < 30 ? 0 : fr < 75 ? 1 : 2;       // 30% sketch · 45% set · 25% jam
    sng.groove = srnd(NG);                             // ballad / swing / drive

    // tempo follows the groove — a ballad really is slower, drive really faster
    static const int TLO[NG] = { 70, 88, 104 }, TSPAN[NG] = { 16, 18, 19 };
    tempo = TLO[sng.groove] + srnd(TSPAN[sng.groove]); // 70..85 / 88..105 / 104..122
    bpm(tempo);
    apply_band_overrides();
    songBase = (long)pos + 8;
    keysInit = false;
    songCount++;
}

static void fresh_song(double pos) {
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form / harmony ────────────────────────────────────────────────────────
static int  form_sects(void) { return FORMS[sng.form].n; }
static long song_bars(void)  { return (long)FORMS[sng.form].n * 8; }
static int  sect_of(long bar) {
    int x = (int)(bar / 8), n = FORMS[sng.form].n;
    return x < n ? FORMS[sng.form].s[x] : S_OUTRO;
}

// the contiguous solo run containing `bar` (the jam doubles a solo to 16 bars):
// where it starts and how many bars long — the improviser's arc stretches to fit
static void solo_span(long bar, long *startBar, int *lenBars) {
    int sect = sect_of(bar), x = (int)(bar / 8), x0 = x, x1 = x, n = FORMS[sng.form].n;
    while (x0 > 0     && sect_of((long)(x0 - 1) * 8) == sect) x0--;
    while (x1 + 1 < n && sect_of((long)(x1 + 1) * 8) == sect) x1++;
    *startBar = (long)x0 * 8;
    *lenBars  = (x1 - x0 + 1) * 8;
}

// which vamp centre is active this bar (semitone offset from the key root)
static int drone_center(long bar) {
    if (sng.vampKind == 0) return 0;
    int period = sng.vampKind == 1 ? 8 : 4;
    return ((bar / period) & 1) ? sng.secondOff : 0;
}

static int level_of(long bar) {
    int s = sect_of(bar);
    int base = (s == S_INTRO || s == S_OUTRO) ? 0 : (s == S_VSOLO || s == S_HSOLO) ? 2 : 1;
    return rad_level(base, intensity);
}

static void center_label(char *out, int n, int off) {
    snprintf(out, n, "%s%s", RAD_PCNAME[(sng.keyPc + off) % 12],
             QMINORISH[sng.qenet] ? "m" : "");
}

// ── the step player ───────────────────────────────────────────────────────
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
    const int *q = QENET[sng.qenet];
    int  center  = drone_center(bar);
    int  croot   = (sng.keyPc + center) % 12;

    // ── PERCUSSION — congas, not a trap kit; the seed picks the feel ──
    bool kit = sect != S_INTRO || bar % 8 >= 4;
    if (kit) {
        if (sng.groove == G_BALLAD) {                           // sparse, spacious tizita
            if (step == 0)                                      // kebero on the one, ringing
                { schedule_hit(dly, 50, SL_KEB, lvl >= 1 ? 5 : 4, 240); vu += 1.4f; }
            if (step == 6 || step == 14)                        // one lazy conga answer per bar
                { schedule_hit(dly + rnd(4), 54, SL_CONGA, 3, 170); vu += 0.7f; }
            if (lvl >= 2 && step == 10 && chance(60))
                schedule_hit(dly + rnd(4), 64 + rnd(4), SL_BONGO, 2, 80);
            if (lvl >= 1 && step % 4 == 0)                      // shaker on the quarters only
                schedule_hit(dly + rnd(6), 90, SL_SHAK, 1, 24);
        } else if (sng.groove == G_DRIVE) {                     // driving 6/8 — the fire
            if (step == 0 || step == 8)                         // two-feel kebero pulse
                { schedule_hit(dly, 50, SL_KEB, lvl >= 1 ? 5 : 4, 200); vu += 1.3f; }
            if (step==2||step==5||step==8||step==11||step==14)  // rolling congas
                { schedule_hit(dly + rnd(3), (step==5||step==11) ? 57 : 52, SL_CONGA, 3, 130); vu += 0.8f; }
            if (lvl >= 1 && (step==3||step==7||step==10||step==13) && chance(75))
                schedule_hit(dly + rnd(3), 64 + rnd(5), SL_BONGO, 2, 70);   // busy chatter
            if (step % 2 == 0)                                  // shaker 8ths, always on
                schedule_hit(dly + rnd(5), 90, SL_SHAK, 1, 30);
        } else {                                                // G_SWING — the chik-chika heart
            if (step == 0)                                      // the kebero on the one
                { schedule_hit(dly, 50, SL_KEB, lvl >= 1 ? 5 : 4, 220); vu += 1.4f; }
            if (step == 3 || step == 6 || step == 11 || step == 14) // open conga, syncopated
                { schedule_hit(dly + rnd(4), step == 6 ? 57 : 52, SL_CONGA, 3, 150); vu += 0.8f; }
            if (lvl >= 2 && (step == 2 || step == 7 || step == 10 || step == 13) && chance(70))
                schedule_hit(dly + rnd(4), 64 + rnd(4), SL_BONGO, 2, 80);   // bongo chatter
            if (lvl >= 1 && step % 2 == 0)                      // shaker 8ths
                schedule_hit(dly + rnd(6), 90, SL_SHAK, 1, 28);
        }
    }

    // ── BASS ostinato — the hypnotic vamp, locked and looping ──
    if (sect != S_INTRO) {
        for (int i = 0; i < sng.bassN; i++)
            if (sng.bassOn[i] == cs) {
                int m = improv_deg_to_midi(sng.bassDeg[i], 36 + croot, q);
                while (m < 28) m += 12;
                while (m > 47) m -= 12;
                schedule_hit(dly + 12, m, I_BASS, 5, (int)(stepMs * 5));
                vu += 1.4f;
            }
    }

    // ── KEYS drone — held cluster, re-voiced on each 2-bar (and centre change) ──
    if (sect != S_INTRO && step == 0 && bar % 2 == 0) {
        rad_lead_to(croot, sng.droneIv, gvKeys, 3, 52, 74, &keysInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 8, gvKeys[k], I_KEYS, lvl >= 1 ? 3 : 2, (int)(stepMs * 30));
        vu += 1.0f;
    }

    // ── HEAD — vibes melody, doubled by the horn in unison (the ethio move) ──
    if (sect == S_HEAD || sect == S_OUTRO) {
        for (int i = 0; i < sng.melN; i++)
            if (sng.melOn[i] == cs && chance(90)) {
                int m = improv_deg_to_midi(sng.melDeg[i], 60 + sng.keyPc, q);
                while (m < sng.melReg - 7) m += 12;
                while (m > sng.melReg + 9) m -= 12;
                schedule_hit(dly + 14 + rnd(6), m, I_VIBE, 5, 2200);
                if (hornOn && lvl >= 1)                          // the horn line answers
                    schedule_hit(dly + 14, m - 12, I_HORN, 3, (int)(stepMs * 4));
                vu += 1.6f;
            }
    }

    // ── THE SOLOS — the shared improviser, walking the qñit ──
    if (sect == S_VSOLO || sect == S_HSOLO) {
        bool horn    = (sect == S_HSOLO) && hornOn;
        int  slot    = horn ? I_HORN : I_VIBE;
        long startBar; int lenBars;
        solo_span(bar, &startBar, &lenBars);                    // 8 bars, or 16 in the jam
        long soloBar = bar - startBar;
        int  reg     = horn ? 60 : 74;
        if (soloBar == 0 && step == 0)
            improv_begin(&solo, reg, lenBars, 1.0f);
        if (step == 0 && bar % 2 == 0 && improv_due(&solo, soloBar))
            improv_render(&solo, soloBar, q);
        float arc = improv_arc(&solo, soloBar);
        for (int i = 0; i < solo.n; i++)
            if (solo.onset[i] == cs) {
                int m   = improv_midi(&solo, i, soloBar, sng.keyPc, q, 0); // no blue: the mode IS the colour
                int gat = (int)(stepMs * (solo.dur[i] > 4 ? solo.dur[i] : 2) * 0.85f);
                schedule_hit(dly + 12, m, slot, 4 + (arc > 0.6f ? 1 : 0), gat > 1800 ? 1800 : gat);
                vu += 1.5f;
            }
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
// Each chair's candidates are full recipes — switching re-aims the slot from
// scratch, so a swap mid-song is clean. sel 0 is always the shipped sound.
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chLead) {
        if (sel == 0) {                                  // "vibes" — MALLET (new_song rolls macros)
            instrument(I_VIBE, INSTR_MALLET, 1, 0, 7, 1500);
        } else {                                         // "marimba" — wood bar, drier
            instrument(I_VIBE, INSTR_MALLET, 1, 0, 7, 1200);
            instrument_harmonics(I_VIBE, 0.06f);
            instrument_timbre(I_VIBE, 0.50f);
            instrument_morph(I_VIBE, 0.28f);
        }
    } else if (idx == chKeys) {
        if (sel == 0) {                                  // "wurli" — EPIANO (the Mulatu sound)
            instrument(I_KEYS, INSTR_EPIANO, 4, 0, 6, 900);
            instrument_harmonics(I_KEYS, 0.55f);         // toward the Wurlitzer bark
            instrument_morph(I_KEYS, 0.30f);
        } else {                                         // "organ" — Hammond drawbars, gospel-ish
            instrument(I_KEYS, INSTR_ORGAN, 6, 0, 7, 200);
            instrument_harmonics(I_KEYS, 0.55f);
            instrument_morph(I_KEYS, 0.30f);             // a little scanner chorus
        }
    } else if (idx == chHorn) {
        hornOn = (sel != 2);
        if (sel == 0) {                                  // "synth brass" — PD, the horn line
            instrument(I_HORN, INSTR_PD, 26, 0, 6, 220);
            instrument_harmonics(I_HORN, 0.56f);         // sawpulse
            instrument_timbre(I_HORN, 0.70f);            // buzzy
            instrument_morph(I_HORN, 0.50f);             // a little DCW swell on the attack
            instrument_filter(I_HORN, FILTER_LOW, 2600, 2);  // tame the icepick toward "reedy"
        } else if (sel == 1) {                           // "reso lead" — the brighter CZ horn
            instrument(I_HORN, INSTR_PD, 14, 0, 6, 200);
            instrument_harmonics(I_HORN, 0.94f);
            instrument_timbre(I_HORN, 0.55f);
            instrument_morph(I_HORN, 0.40f);
            instrument_filter(I_HORN, FILTER_LOW, 3200, 3);
        }
        // sel == 2 ("off") — leave the slot defined, hornOn gates every hit
    }
}

static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}

static void setup_instruments(void) {
    chLead = rad_chair(&band, "lead",  "vibes", "marimba", NULL, NULL);
    chKeys = rad_chair(&band, "keys",  "wurli", "organ",   NULL, NULL);
    chHorn = rad_chair(&band, "horn",  "synth brass", "reso lead", "off", NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);     // base sounds (sel 0)

    // the bass — INSTR_FM, a round electric/upright with a short pitch blip attack
    instrument(I_BASS, INSTR_FM, 2, 240, 5, 160);
    instrument_harmonics(I_BASS, 0.25f);                 // low harmonic ratio = fundamental-heavy
    instrument_timbre(I_BASS, 0.30f);
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 16, 2);

    // PERCUSSION — the struck-drumhead engine (tabla.c recipes)
    instrument(SL_KEB, INSTR_MEMBRANE, 1, 0, 7, 280);    // low kebero/tom
    instrument_harmonics(SL_KEB, 0.45f);
    instrument_timbre(SL_KEB, 0.30f);                    // near-centre thump
    instrument_morph(SL_KEB, 0.20f);
    instrument(SL_CONGA, INSTR_MEMBRANE, 1, 0, 7, 200);  // open conga
    instrument_harmonics(SL_CONGA, 0.55f);
    instrument_timbre(SL_CONGA, 0.35f);
    instrument_morph(SL_CONGA, 0.15f);
    instrument(SL_BONGO, INSTR_MEMBRANE, 1, 0, 7, 120);  // bright bongo/slap
    instrument_harmonics(SL_BONGO, 0.72f);
    instrument_timbre(SL_BONGO, 0.65f);
    instrument_morph(SL_BONGO, 0.10f);
    instrument(SL_SHAK, INSTR_NOISE, 1, 36, 0, 20);      // shaker
    instrument_filter(SL_SHAK, FILTER_HIGH, 5600, 3);
}

// the tone knob: the whole band brightens/mellows (re-aim, keeping song rolls)
static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_VIBE, FILTER_LOW, (int)(4000 * tm), 0);
    instrument_filter(I_KEYS, FILTER_LOW, (int)(2800 * tm), 0);
    if (hornOn) instrument_filter(I_HORN, FILTER_LOW, (int)(2600 * tm), 2);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (ADDIS_SEED) { new_song(pos, ADDIS_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 76, 120, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
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
    if (chair >= 0) apply_chair(chair);

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
    watch("qenet", "%s", QNAME[sng.qenet]);
    watch("groove", "%s", GNAME[sng.groove]);
    watch("form", "%s", FORMS[sng.form].name);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("center", "%d", drone_center(tbar));
    watch("phrase", "%d", solo.phraseNo);
#endif
}

// ── draw — the warm chassis; window art = the coffee-ceremony jebena ───────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();                                    // knobs are mouse/touch-draggable
    long songStep = scheduled - songBase;
    long bar  = songStep >= 0 ? songStep / 16 : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_DARK_GREEN, CLR_YELLOW);          // green shell, gold trim — the flag
    rad_dial(sng.freq, CLR_YELLOW);

    // the window — a coffee ceremony: warm dusk, the black jebena pot, steam
    rectfill(34, 52, 102, 116, CLR_DARK_RED);                // warm ochre dusk
    rectfill(34, 130, 102, 38, CLR_DARK_BROWN);              // the low table
    // the jebena (clay coffee pot): round belly, long neck, spout, lid knob
    circfill(78, 132, 16, CLR_BLACK);                        // belly
    rectfill(75, 108, 6, 18, CLR_BLACK);                     // neck
    line(81, 112, 92, 104, CLR_BLACK);                       // spout
    line(81, 114, 93, 106, CLR_BLACK);
    circfill(78, 106, 3, CLR_BLACK);                         // lid knob
    circ(78, 132, 16, CLR_DARK_GREY);
    // a couple of little cups (sini) on the table
    rectfill(50, 138, 6, 6, CLR_PEACH);
    rectfill(102, 138, 6, 6, CLR_PEACH);
    // steam — curls rising from the spout, animated
    for (int k = 0; k < 3; k++) {
        int sx = 90 + (int)(sinf(timer() * 1.6f + k * 1.7f) * 4);
        for (int y = 100; y > 64; y -= 6) {
            int wob = (int)(sinf(timer() * 2.0f + y * 0.2f + k) * 3);
            pset(sx + wob, y, (y < 80) ? CLR_DARK_GREY : CLR_LIGHT_GREY);
        }
    }
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_YELLOW);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_YELLOW);
        char l2[32];
        snprintf(l2, 32, "%s  %s", QNAME[sng.qenet], GNAME[sng.groove]);
        print(l2, 154, 70, CLR_ORANGE);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_ORANGE);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_YELLOW);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        int ncenters = sng.vampKind == 0 ? 1 : 2;
        rad_chord_row(nowCtr, ncenters, drone_center(bar) ? 1 : 0, 152, 104, CLR_YELLOW);
        print(SECTNAME[sect], 152, 120, CLR_YELLOW);
        rad_phrase_dots(232, 124, form_sects(), bar / 8, CLR_YELLOW);
    }

    static const char *FEEL[4] = { "alem", "mellow", "swing", "fire" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_YELLOW);
    if (rad_knob_int(&tempo, 76, 120, 2, 218, 148, 9, "tempo", CLR_YELLOW)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_YELLOW)) apply_tone();
    rad_power_led(radioOn, CLR_YELLOW, CLR_DARK_GREEN);

    rad_help_button(CLR_YELLOW);
    rad_footer("B band   H help");

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next tune (rolls a new seed)" },
            { "R",          "same tune - the solos are new" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - alem/mellow/swing/fire" },
            { "UP/DOWN",    "tempo of this tune" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
            { "B",          "the band - vibes/keys/horn" },
        };
        static const char *NOTES[3] = {
            "the qenit is the song: tizita / bati /",
            "anchihoye / ambassel - pentatonic modes.",
            "vibes=MALLET horn=PD congas=MEMBRANE",
        };
        rad_help_panel("ADDIS RADIO", HELP, 8, NOTES, 3, CLR_YELLOW);
    }
    rad_band_panel(&band, CLR_YELLOW);
    ui_end();
}
