// ── BRAINDANCE FM ───────────────────────────────────────────────────────────
// Aphex Twin's drill'n'bass / braindance, generated forever. Path B of the Aphex
// problem (he's a RANGE, not a genre): this station builds ONE mode — the chopped-
// break chaos with a sweet, slightly-wrong melody floating over it. Beauty over
// brutality: strip the melody and it's jungle, strip the chaos and it's a music
// box. The station IS the tension between the two layers.
// Full design: docs/design/braindance-blind-brief.md.
//
// The brains (cart-side, over radio.h's clock):
//   • RHYTHM AS GRAMMAR + a VOLATILITY knob — the kit is not a pattern: anchors
//     persist (kick on 1, snare backbeat), everything else RE-ROLLS every bar on
//     engine rnd(). volatility 0 = a near-stable loop, 1 = never the same bar
//     twice. The first brain for rhythm, not harmony.
//   • RATCHETS — a hit becomes 3-8 sub-hits at 1/32-1/64 with vol/pitch ramps
//     (the impossible drill rolls); sample-exact via schedule_hit().
//   • TWO CROSSED DENSITY CURVES — the serene melodic top and the violent drums
//     ride INDEPENDENT curves (the "Flim" move). The vibe roll bends the two
//     against each other: sweet = lush top / wild drums, drill = sparse top /
//     max drums.
//   • THE LURCH (form) — not 8x8 sections: sudden DROP events strip to the pad
//     for a bar, then slam back with a fill. Mutate, don't repeat.
//   • MISTUNING AS TIMBRE — every melodic voice detuned a few cents + a slow
//     sub-Hz pitch LFO (tape wow): the machine is alive and slightly broken.
//
// The chopped Amen is FAKED (we have no sampler) — a synth break kit shredded by
// the grammar + ratchets + per-hit pitch drift. Evocative, not faithful; the
// granular/time-stretch want is logged in sound-next-steps.md.
//
//   SPACE next track   R replay (drums shred anew)   [ ] history   M radio on/off
//   LEFT/RIGHT density   UP/DOWN tempo   T tone   B band   H help

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define BD_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots (5..13) ────────────────────────────────────────────────
#define I_KICK  5   // kick — sine + pitch drop
#define I_SNARE 6   // snare crack / the ratchet voice (NOISE)
#define I_HAT   7   // hat (NOISE)
#define I_TOM   8   // pitched shred tom — the roll fills (SINE + pitch)
#define I_SUB   9   // sub bass (SINE)
#define I_ACID 10   // 303 acid line (SAW, resonant)
#define I_PAD  11   // warm string-machine bed (SAW, detuned, held)
#define I_AIR  12   // high "choir" air wash (SAW, detuned, soft)
#define I_BELL 13   // music-box / celesta melody (MALLET)

#define BARSTEPS 16

// ── the harmony bed — static, not functional (a frozen beautiful pool) ──────
// minor / dorian / lydian-nostalgic; the motion is RHYTHMIC, the harmony holds.
enum { MO_MINOR, MO_DORIAN, MO_LYDIAN, NMODE };
static const int MODE7[NMODE][7] = {
    { 0, 2, 3, 5, 7, 8, 10 },   // aeolian minor — the eerie one
    { 0, 2, 3, 5, 7, 9, 10 },   // dorian — the funky one
    { 0, 2, 4, 6, 7, 9, 11 },   // lydian — the sweet nostalgic one
};
static const char *MNAME[NMODE] = { "minor", "dorian", "lydian" };
// the held vamp: a tonic and one neighbour chord (mode degree offsets, root)
static const int NEIGH[NMODE] = { 5, 3, 4 };   // iv / bIII / IV-ish — the second pool

static int deg_to_midi(int deg, int center, const int *m) {
    int oct = 0;
    while (deg < 0)  { deg += 7; oct--; }
    while (deg >= 7) { deg -= 7; oct++; }
    return center + m[deg] + oct * 12;
}

// ── the vibe — the per-song roll; sets the two crossed curves + tempo ───────
// drumBase/melBase = the two density curves; volat = how shredded; halftime =
// the funk pocket; the tempo band.
enum { VB_SWEET, VB_DRILL, VB_FUNK, VB_EERIE, NVIBE };
static const char *VNAME[NVIBE] = { "sweet", "drill", "funk", "eerie" };
static const struct { float drum, mel, volat; int tlo, tspan, halftime; } VIBE[NVIBE] = {
    // drum  mel   volat  tlo  span half
    { 0.62f, 0.95f, 0.50f, 162, 14, 0 },   // sweet  — lush top, busy drums (Flim)
    { 1.00f, 0.34f, 0.92f, 168, 16, 0 },   // drill  — drum-forward, max shred
    { 0.70f, 0.62f, 0.55f, 144, 16, 1 },   // funk   — half-time slink, acid up
    { 0.34f, 0.92f, 0.42f, 136, 16, 0 },   // eerie  — near-ambient, drums intermittent
};

// ── the form — the LURCH: build, run (with drops), out ──────────────────────
enum { S_INTRO, S_RUN, S_DROP, S_OUT };
#define MAXSECT 12
static const struct { int n; int s[MAXSECT]; const char *name; } FORMS[] = {
    { 5,  { S_INTRO, S_RUN, S_DROP, S_RUN, S_OUT }, "cut" },
    { 8,  { S_INTRO, S_RUN, S_RUN, S_DROP, S_RUN, S_DROP, S_RUN, S_OUT }, "track" },
    { 11, { S_INTRO, S_RUN, S_RUN, S_DROP, S_RUN, S_RUN, S_DROP,
            S_RUN, S_DROP, S_RUN, S_OUT }, "epic" },
};
#define NFORMS 3
static const char *SECTNAME[4] = { "intro", "run", "drop", "out" };

// ── the generated track ──────────────────────────────────────────────────────
typedef struct {
    int   vibe, mode, keyPc, form;
    int   acidWave;             // INSTR_SAW or INSTR_SQUARE
    int   melOn[8], melDeg[8], melN;   // the sweet cell (2-bar, 32-step)
    int   acidOn[16], acidDeg[16], acidN;  // the 303 line (1-bar, 16-step)
    int   bassDeg;              // the sub's root degree of the held chord
    float detune;               // mistuning amount (cents-ish) for this track
    float wow;                  // tape-wow LFO depth
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

static int   tempo     = 168;
static int   intensity = 2;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 2;
static int   songCount = 0;
static float vu        = 0;
static float drumVu    = 0;        // the two meters, for the crossed-curve display
static float melVu     = 0;

// held voice-leading state for the pad chord
static int  gvPad[3] = { 50, 55, 59 };
static bool padInit  = false;

static RadBand band;
static int chKit, chBell, chTop;

#define srnd(n) rad_srnd(&rs, (n))

static void apply_band_overrides(void);
static void apply_chair(int idx);

// ── the RATCHET — a hit shredded into n sub-hits (the drill roll) ───────────
// sub-hits at subMs spacing with a volume fade and an optional pitch ramp.
static void ratchet(int dly, int midi, int slot, int vol, int n, int subMs, int pitchStep) {
    if (n < 1) n = 1;
    for (int k = 0; k < n; k++) {
        int v = vol - (k * vol) / (n + 1);
        if (v < 1) v = 1;
        schedule_hit(dly + k * subMs, midi + k * pitchStep, slot, v, subMs + 12);
    }
}

// ── track generation ─────────────────────────────────────────────────────────
static const char *TW1[] = { "Rhubarb", "Flim", "Cock", "Ver", "Bucephalus", "Vordhosbn",
    "Avril", "Mt", "Windowlicker", "Alberto", "Nannou", "Gwely" };
static const char *TW2[] = { "ver10", "Bouncing", "Mnemonic", "Balls", "47", "14",
    "Selected", "Drukqs", "Balsam", "Meng£", "Donkey", "Rhubarb" };

static void roll_cell(int *on, int *deg, int *n, int maxN, int dens, int span, int gridN) {
    *n = 0;
    int d = srnd(3);
    for (int s = 0; s < gridN && *n < maxN; s += (gridN > 16 ? 2 : 1))
        if (srnd(100) < dens) {
            on[*n]  = s;
            d += srnd(5) - 2;
            if (d < -2) d = -2;
            if (d > span) d = span;
            deg[*n] = d;
            (*n)++;
        }
    if (*n < 2) { *n = 2; on[0] = 0; deg[0] = 0; on[1] = gridN / 2; deg[1] = 2; }
}

static void new_track(double pos, unsigned seed) {
    trk.seed = rad_seed_begin(&rs, seed);

    // weighted toward the genre — this is a DRILL'N'BASS station; eerie is the
    // rare near-ambient palate-cleanser, not the default.
    int vr = srnd(100);
    trk.vibe = vr < 34 ? VB_DRILL : vr < 60 ? VB_SWEET : vr < 88 ? VB_FUNK : VB_EERIE;
    trk.mode  = srnd(NMODE);
    trk.keyPc = srnd(12);
    trk.acidWave = srnd(100) < 60 ? INSTR_SAW : INSTR_SQUARE;
    trk.bassDeg  = 0;

    roll_cell(trk.melOn, trk.melDeg, &trk.melN, 8, 42, 7, 32);    // the sweet cell, 2 bars
    roll_cell(trk.acidOn, trk.acidDeg, &trk.acidN, 16, 55, 7, 16); // the acid line, 1 bar

    // mistuning amounts (seeded — part of the track's identity)
    trk.detune = 0.02f + srnd(8) * 0.01f;     // 2..10 cents-ish
    trk.wow    = 0.04f + srnd(8) * 0.01f;

    snprintf(trk.title, sizeof trk.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    trk.freq = 88.0f + srnd(190) * 0.1f;

    int fr = srnd(100);
    trk.form = fr < 32 ? 0 : fr < 74 ? 1 : 2;

    tempo = VIBE[trk.vibe].tlo + srnd(VIBE[trk.vibe].tspan);
    bpm(tempo);

    apply_band_overrides();
    apply_chair(chTop);      // re-aim the mistuning for this track's detune/wow
    songBase = (long)pos + BARSTEPS;
    padInit = false;
    songCount++;
}

static void fresh_track(double pos) { new_track(pos, 0); rad_hist_log(&rs); }

// ── form / harmony ──────────────────────────────────────────────────────────
static int  form_sects(void) { return FORMS[trk.form].n; }
static long song_bars(void)  { return (long)FORMS[trk.form].n * 8; }
static int  sect_of(long bar) {
    int x = (int)(bar / 8), n = FORMS[trk.form].n;
    return x < n ? FORMS[trk.form].s[x] : S_OUT;
}
// which held chord this bar: tonic, or the neighbour for the 2nd half of a block
static int chord_root_deg(long bar) {
    return ((bar / 4) & 1) ? NEIGH[trk.mode] : 0;
}

// a DROP bar: drums cut to near-silence (just the pad), then the next bar slams.
static bool drop_bar(long bar) {
    int s = sect_of(bar);
    if (s == S_DROP) return (bar % 8) < 2;             // the strip
    if (s == S_RUN)  return (bar % 8 == 7);            // the pre-section breath
    return false;
}
static bool slam_bar(long bar) { return drop_bar(bar - 1); }   // the bar after a drop

// the two crossed density curves, 0..1
static float drum_curve(long bar) {
    int s = sect_of(bar);
    float base = VIBE[trk.vibe].drum;
    if (s == S_INTRO) base *= (float)(bar % 8) / 8.0f;     // layer the drums in
    if (s == S_OUT)   base *= 0.5f;
    float lvl = 0.5f + intensity * 0.18f;                  // density knob shifts it
    float v = base * lvl;
    return v < 0 ? 0 : v > 1 ? 1 : v;
}
static float mel_curve(long bar) {
    float base = VIBE[trk.vibe].mel;
    if (sect_of(bar) == S_DROP) base = 1.0f;               // the melody OWNS the drop
    float v = base * (0.7f + intensity * 0.10f);
    return v < 0 ? 0 : v > 1 ? 1 : v;
}
static float volatility(long bar) {
    float v = VIBE[trk.vibe].volat + (intensity - 2) * 0.10f;
    if (sect_of(bar) == S_INTRO) v *= 0.5f;
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

// ── the step player ─────────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % BARSTEPS);
    long bar  = s / BARSTEPS;
    if (bar >= song_bars()) return;
    int  cs   = (int)(s % 32);                  // 2-bar window for the melody cell
    const int *m = MODE7[trk.mode];
    int  croot = (trk.keyPc + MODE7[trk.mode][chord_root_deg(bar)]) % 12;
    float drumD = drum_curve(bar);
    float melD  = mel_curve(bar);
    float V     = volatility(bar);
    bool  half  = VIBE[trk.vibe].halftime;
    bool  drop  = drop_bar(bar);
    bool  slam  = slam_bar(bar);

    int hh = (int)(stepMs / 2);    // 1/32
    int ht = (int)(stepMs / 3);    // 1/48
    int hq = (int)(stepMs / 4);    // 1/64

    // ── THE DRUM GRAMMAR — anchors persist, the rest re-rolls every bar ──────
    if (!drop && drumD > 0.05f) {
        int kstep = half ? 0 : 0;                          // kick anchor on the 1
        // KICK — anchor on 0; syncopated extra kicks scale with volatility
        if (step == kstep) { schedule_hit(dly, 36, I_KICK, 6, 150); vu += 2; drumVu += 2; }
        if (!half && (step == 6 || step == 10) && chance((int)(V * 55)))
            { schedule_hit(dly, 36, I_KICK, 5, 130); drumVu += 1.5f; }
        if (step == 8 && chance((int)(drumD * 40)))        // the "&" kick
            { schedule_hit(dly + rnd(6), 36, I_KICK, 4, 120); drumVu += 1; }

        // SNARE — backbeat anchor (4 & 12, or just 8 in half-time); + shred
        bool back = half ? (step == 8) : (step == 4 || step == 12);
        if (back) {
            // the backbeat can itself ratchet into a roll at high volatility
            if (chance((int)(V * 55)) && step >= 8)
                ratchet(dly, 60, I_SNARE, 6, 2 + rnd(4), hh, rnd(3));
            else { schedule_hit(dly, 60, I_SNARE, 6, 70); schedule_hit(dly, 50, I_KICK, 2, 40); }
            drumVu += 2.5f;
        }
        // ghost / shred snares off the grid — the braindance scatter
        else if (chance((int)(V * 45))) {
            if (chance((int)(V * 60)))
                ratchet(dly + rnd(8), 60, I_SNARE, 3, 2 + rnd(5), chance(50) ? hh : hq, rnd(4));
            else
                schedule_hit(dly + rnd(8), 60, I_SNARE, 2, 35);
            drumVu += 1.2f;
        }

        // HATS — 16ths, density and ratchet from the curve
        int hatEvery = (drumD > 0.7f) ? 1 : 2;
        if (step % hatEvery == 0) {
            int hv = (step % 4 == 0) ? 3 : 2;
            if (chance((int)(V * 30)))
                ratchet(dly + rnd(3), 90, I_HAT, hv, 2 + rnd(3), hq, 0);
            else
                schedule_hit(dly + rnd(3), 90, I_HAT, hv, 22);
        }

        // PITCHED SHRED TOM — the climbing roll fills, performance-only
        if (step >= 12 && chance((int)(V * 50))) {
            int up = chance(60) ? 2 : -2;
            ratchet(dly + rnd(4), 48 + rnd(6), I_TOM, 4, 3 + rnd(5),
                    chance(50) ? hh : ht, up);
            drumVu += 1.5f;
        }
    }
    // THE SLAM — the bar after a drop kicks off with a downbeat fill
    if (slam && step == 0) {
        schedule_hit(dly, 36, I_KICK, 6, 160);
        ratchet(dly + hh, 60, I_SNARE, 5, 4 + rnd(4), hh, rnd(3));   // offset off the kick = headroom
        drumVu += 4;
    }

    // ── SUB BASS — deep, simple, on the chord root (rests in the eerie vibe) ──
    if (trk.vibe != VB_EERIE || drop) {
        bool subHit = half ? (step == 0 || step == 8) : (step == 0 || step == 10);
        if (subHit && (!drop || step == 0)) {
            int mm = 24 + croot;
            schedule_hit(dly + 4, mm, I_SUB, 6, (int)(stepMs * (half ? 6 : 4)));
            vu += 1.5f;
        }
    }

    // ── THE 303 ACID LINE — funk/drill, a per-bar squelch line ───────────────
    if ((trk.vibe == VB_FUNK || trk.vibe == VB_DRILL) && !drop && intensity >= 1) {
        for (int i = 0; i < trk.acidN; i++)
            if (trk.acidOn[i] == step) {
                int mm = deg_to_midi(trk.acidDeg[i], 36 + croot, m);
                while (mm > 55) mm -= 12;
                schedule_hit(dly + 6, mm, I_ACID, chance(30) ? 6 : 4, (int)(stepMs * 1.5f));
                vu += 0.8f;
            }
    }

    // ── THE SWEET TOP — the pad bed + the music-box melody (the OTHER curve) ──
    // PAD: re-articulated every 2 bars (so the filter sweep re-fires = motion,
    // not a flat drone), shorter gate, voice-led on chord changes.
    if (step == 0 && bar % 2 == 0) {
        int iv[3] = { 0, MODE7[trk.mode][2], MODE7[trk.mode][4] };  // root/3rd/5th
        rad_lead_to(croot, iv, gvPad, 3, 48, 64, &padInit);
        for (int k = 0; k < 3; k++) {
            schedule_hit(dly + k * 10, gvPad[k], I_PAD, 3, (int)(stepMs * 30));
            if (melD > 0.6f)                                  // the high "choir" doubles it
                schedule_hit(dly + k * 10, gvPad[k] + 12, I_AIR, 2, (int)(stepMs * 30));
        }
        melVu += 2;
    }
    // MUSIC-BOX MELODY: the re-pitched sweet cell, gated by the mel curve
    for (int i = 0; i < trk.melN; i++)
        if (trk.melOn[i] == cs && chance((int)(melD * 90))) {
            int mm = deg_to_midi(trk.melDeg[i], 72 + trk.keyPc, m);
            while (mm > 92) mm -= 12;
            schedule_hit(dly + 10, mm, I_BELL, melD > 0.7f ? 4 : 3, (int)(stepMs * 3));
            if (melD > 0.85f && chance(30))                   // a little high sparkle octave
                schedule_hit(dly + 12, mm + 12, I_BELL, 2, (int)(stepMs * 2));
            melVu += 1.4f;
        }
}

// ── the band chairs ──────────────────────────────────────────────────────────
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chKit) {
        if (sel == 0) {                                  // "amen" — shred break kit
            instrument(I_KICK, INSTR_SINE, 0, 95, 0, 35);
            instrument_env(I_KICK, 0, ENV_PITCH, 0, 50, 16);
            instrument(I_SNARE, INSTR_NOISE, 0, 85, 0, 40);
            instrument_filter(I_SNARE, FILTER_BAND, 1900, 6);
            instrument_env(I_SNARE, 0, ENV_PITCH, 0, 25, 12);
        } else {                                         // "808" — punchy clean box
            instrument(I_KICK, INSTR_SINE, 0, 220, 0, 60);
            instrument_filter(I_KICK, FILTER_LOW, 250, 3);
            instrument_env(I_KICK, 0, ENV_PITCH, 0, 50, 24);
            instrument(I_SNARE, INSTR_NOISE, 0, 120, 0, 50);
            instrument_filter(I_SNARE, FILTER_BAND, 1500, 4);
        }
        instrument(I_HAT, INSTR_NOISE, 0, 18, 0, 14);
        instrument_filter(I_HAT, FILTER_HIGH, 7800, 3);
        instrument(I_TOM, INSTR_SINE, 0, 90, 0, 40);     // pitched shred tom
        instrument_env(I_TOM, 0, ENV_PITCH, 0, 60, 8);
    } else if (idx == chBell) {
        // the music-box / celesta melody (mallet.c presets)
        instrument(I_BELL, INSTR_MALLET, 1, 0, 7, 600);  // SHORT ring — a music box, not a vibe
        if (sel == 0) { instrument_harmonics(I_BELL, 0.55f); instrument_timbre(I_BELL, 0.72f); instrument_morph(I_BELL, 0.16f); } // glassy detuned music-box
        else          { instrument_harmonics(I_BELL, 0.92f); instrument_timbre(I_BELL, 0.90f); instrument_morph(I_BELL, 0.22f); } // bright glock
    } else if (idx == chTop) {
        // the sweet bed + air wash + sub + acid — and THE MISTUNING glue
        instrument(I_PAD, INSTR_PD, 500, 620, 4, 700);   // Casio CZ sweep-pad (pd.c) —
        instrument_harmonics(I_PAD, 0.69f);              // the resonant-triangle "wowww":
        instrument_timbre(I_PAD, 0.38f);                 // a full DCW sweep is BUILT IN, so
        instrument_morph(I_PAD, 0.95f);                  // it evolves on its own — never static
        instrument(I_AIR, INSTR_SAW, 120, 300, 6, 700);  // high airy "choir" wash
        instrument_filter(I_AIR, FILTER_LOW, 2600, 2);
        instrument(I_SUB, INSTR_SINE, 1, 120, 0, 120);
        instrument_filter(I_SUB, FILTER_LOW, 700, 1);
        instrument_env(I_SUB, 0, ENV_PITCH, 0, 35, 6);
        instrument(I_ACID, trk.acidWave, 2, 60, 5, 60);  // the 303
        instrument_duty(I_ACID, 0.48f);
        instrument_filter(I_ACID, FILTER_LOW, 900, 11);
        instrument_env(I_ACID, 0, ENV_CUTOFF, 0, 110, 2200);
        instrument_drive(I_ACID, 0.3f);
        // MISTUNING — the tape-warm "alive machine": detune the melodic voices a
        // few cents and add a slow sub-Hz pitch wow. sel 1 = "clean" (off).
        float dt = (sel == 1) ? 0.0f : trk.detune;
        float wow = (sel == 1) ? 0.0f : trk.wow;
        instrument_tune(I_PAD, dt);
        instrument_tune(I_AIR, -dt * 1.4f);              // opposite-detuned pair = width
        instrument_tune(I_BELL, dt * 1.1f);             // the music box runs a touch "wrong" — childlike
        instrument_lfo(I_PAD, 1, LFO_PITCH, 0.18f, wow);
        instrument_lfo(I_AIR, 1, LFO_PITCH, 0.13f, wow * 1.2f);
        instrument_lfo(I_BELL, 1, LFO_PITCH, 0.22f, wow * 0.8f);
    }
}

static void apply_band_overrides(void) {
    for (int i = 0; i < band.n; i++)
        if (band.c[i].sel) apply_chair(i);
}

static void setup_instruments(void) {
    chKit  = rad_chair(&band, "kit",   "amen", "808",  NULL, NULL);
    chBell = rad_chair(&band, "topline","musicbox", "glock", NULL, NULL);
    chTop  = rad_chair(&band, "tape",  "warped", "clean", NULL, NULL);
    for (int i = 0; i < band.n; i++) apply_chair(i);
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_PAD, FILTER_LOW, (int)(2200 * tm), 2);
    instrument_filter(I_AIR, FILTER_LOW, (int)(2600 * tm), 2);
    instrument_filter(I_HAT, FILTER_HIGH, (int)(7800 * tm), 3);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (BD_SEED) { new_track(pos, BD_SEED); rad_hist_log(&rs); }
        else fresh_track(pos);
        scheduled = (long)pos;
        apply_tone();
        booted = true;
    }

    int ev = rad_input(&tempo, 120, 184, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
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

    vu *= 0.88f;    if (vu > 12) vu = 12;
    drumVu *= 0.80f; if (drumVu > 12) drumVu = 12;
    melVu *= 0.82f;  if (melVu > 12) melVu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / BARSTEPS : 0;
    watch("track", "%d", songCount);
    watch("vibe", "%s", VNAME[trk.vibe]);
    watch("mode", "%s", MNAME[trk.mode]);
    watch("form", "%s", FORMS[trk.form].name);
    watch("sect", "%s", SECTNAME[sect_of(tbar)]);
    watch("volat", "%.2f", volatility(tbar));
    watch("drumD", "%.2f", drum_curve(tbar));
    watch("melD", "%.2f", mel_curve(tbar));
    watch("drop", "%d", drop_bar(tbar) ? 1 : 0);
#endif
}

// ── draw — the window: a glitchy scope + the two crossed density bars ────────
void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / BARSTEPS : 0;
    int  sect = sect_of(bar);

    rad_body(CLR_DARKER_PURPLE, CLR_LIME_GREEN);     // black-light rave green on near-black
    rad_dial(trk.freq, CLR_LIME_GREEN);

    // the window — a shattered oscilloscope, jittering with the drums
    rectfill(34, 52, 102, 116, CLR_BLACK);
    rect(34, 52, 102, 116, CLR_DARK_GREY);
    int cy = 110;
    int jit = (int)(drumVu);
    int prevx = 36, prevy = cy;
    for (int x = 36; x < 134; x += 2) {
        float ph = (x - 36) * 0.18f + timer() * 6.0f;
        int amp = 14 + (int)(melVu);
        int y = cy + (int)(sinf(ph) * amp);
        if (jit > 3 && (x % 8 < 2)) y += rnd(jit * 2) - jit;   // glitch tears
        line(prevx, prevy, x, y, CLR_LIME_GREEN);
        prevx = x; prevy = y;
    }
    // the two CROSSED density bars — the station's whole idea, made visible
    int dh = (int)(drum_curve(bar) * 44);
    int mh = (int)(mel_curve(bar) * 44);
    rectfill(40, 156 - dh, 6, dh, CLR_RED);          // drums (brutality)
    rectfill(124, 156 - mh, 6, mh, CLR_BLUE_GREEN);  // melody (beauty)
    print("D", 41, 158, CLR_RED);
    print("M", 125, 158, CLR_BLUE_GREEN);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_LIME_GREEN);
    if (radioOn) {
        print(trk.title, 154, 58, CLR_LIME_GREEN);
        char l2[32];
        snprintf(l2, 32, "%s  %s %s", VNAME[trk.vibe], RAD_PCNAME[trk.keyPc], MNAME[trk.mode]);
        print(l2, 154, 70, CLR_LIGHT_GREY);
        snprintf(l2, 32, "%d bpm #%08X", tempo, trk.seed);
        print(l2, 154, 82, CLR_LIGHT_GREY);
        float vt = vu / 12.0f;
        rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_LIME_GREEN);
    } else
        print("- silence -", 178, 70, CLR_DARK_GREY);

    if (radioOn) {
        print(SECTNAME[sect], 152, 104, drop_bar(bar) ? CLR_RED : CLR_LIME_GREEN);
        rad_phrase_dots(232, 108, form_sects(), bar / 8, CLR_LIME_GREEN);
    }

    static const char *FEEL[4] = { "calm", "warm", "hot", "mental" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_LIME_GREEN);
    if (rad_knob_int(&tempo, 120, 184, 2, 218, 148, 9, "tempo", CLR_LIME_GREEN)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_LIME_GREEN)) apply_tone();
    rad_power_led(radioOn, CLR_LIME_GREEN, CLR_DARKER_PURPLE);

    rad_help_button(CLR_LIME_GREEN);
    rad_footer("B band   H help");

    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next track (new seed)" },
            { "R",          "replay - drums shred anew" },
            { "[ / ]",      "back / forward history" },
            { "LEFT/RIGHT", "density - calm..mental" },
            { "UP/DOWN",    "tempo" },
            { "T",          "tone" },
            { "M",          "radio on / off" },
            { "B",          "band - kit/topline/tape" },
        };
        static const char *NOTES[3] = {
            "drums = a GRAMMAR (volatility), not a loop;",
            "ratchet rolls; the sweet melody and the",
            "violent drums ride two CROSSED curves.",
        };
        rad_help_panel("BRAINDANCE FM", HELP, 8, NOTES, 3, CLR_LIME_GREEN);
    }
    rad_band_panel(&band, CLR_LIME_GREEN);
    ui_end();
}
