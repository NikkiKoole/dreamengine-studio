/* de:meta
{
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
  "lineage": "The lo-fi/Nujabes/Dilla pole of jazzy hip-hop, distinct from lowend's boom-bap; novel in THE DRUNK POCKET - a dialable off-grid time feel (snare-late/lazy-kick/swing + humanize), the loose pocket lowend undersold. Reuses vapor's lo-fi rack.",
  "homage": "Lo-fi hip-hop (Nujabes / J Dilla)",
  "description": "Lo-fi jazzy hip-hop - the Nujabes / J Dilla / beats-to-study-to pole, dreamier and hazier than lowend's hard boom-bap (we ship that too). Lush extended Rhodes jazz (maj9/m11/13 loops) over a dusty SWUNG kit, wrapped in vinyl crackle + tape warmth. The headline brain is THE DRUNK POCKET - the off-grid feel: the snare drags LATE, the kick is lazy, the hats swing, with a little seeded humanize wobble - and it is ADJUSTABLE: a POCKET knob (LEFT/RIGHT) from tight (on the grid) -> loose -> behind -> drunk, defaulting to a tasteful moderate drag (never seasick unless you crank it); the loose feel lowend undersold, here done right and under the player's control. The harmony is a slow static 2-4 chord extended-jazz loop; a muted-horn / vibe sampled-feel dab floats over it, filtered + reverbed and laid back. The lo-fi rack (set-and-hold, per mood) is tape wow/flutter + gentle echo + reverb + a held vinyl-crackle bed (reused from vapor). The seed rolls a MOOD: sleepy / dusty / rainy / sunny (each sets tempo, scale, the default pocket and the crackle). The window is a cozy night room: a rainy window, a breathing desk lamp, and a turntable whose platter spins with the tempo. SPACE next, R replay, [ ] history, LEFT/RIGHT pocket (tight..drunk), UP/DOWN tempo, T tone, B band (keys rhodes/wurli, bass upright/round, dab horn/vibe/off), M power, H help. Pin via LOFI_SEED."
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
//     feel lowend undersold, here done right and under the player's control.
//   • A HAZY JAZZ-LOOP chord brain — a short 2-4 chord extended loop (maj9/m11/13), slow.
//   • THE LO-FI RACK (set-and-hold, per mood) — tape wow/flutter, gentle echo + reverb,
//     a high rolloff, and a held vinyl-crackle bed. (reused from vapor's playbook.)
//   • Mood roll: sleepy / dusty / rainy / sunny.
//
//   SPACE next   R replay   [ ] history   LEFT/RIGHT pocket (tight..drunk)
//   UP/DOWN tempo   T tone   B band   M power   H help

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define LOFI_SEED 0

// ── slots ───────────────────────────────────────────────────────────────────
#define I_EP   5    // Rhodes (the harmonic core)
#define I_BASS 6    // round / upright bass
#define I_DAB  7    // muted-horn / vibe sampled dab
#define I_KICK 8
#define I_SNR  9
#define I_HAT  10
#define I_VINYL 11  // held vinyl-crackle bed

// ── lush extended-jazz voicings (rootless 3rd/7th/9th-ish) ──────────────────
enum { Q_MAJ9, Q_MIN9, Q_DOM9, Q_MAJ7, Q_MIN7, NQ };
static const char *QN[NQ] = { "maj9", "m9", "9", "maj7", "m7" };
static const int QV[NQ][3] = {
    { 4, 11, 14 }, { 3, 10, 14 }, { 4, 10, 14 }, { 4, 11, 7 }, { 3, 10, 7 },
};
static const int SCALES[3][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },    // major
    { 0, 2, 3, 5, 7, 9, 10 },    // dorian
    { 0, 2, 3, 5, 7, 8, 10 },    // aeolian
};

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
    { "sleepy",   72,  8, 0, 2, 0.50f,0.34f,0.34f,0.26f, 400 },
    { "dusty",    82,  8, 1, 2, 0.48f,0.30f,0.42f,0.22f, 340 },
    { "rainy",    78,  8, 2, 1, 0.52f,0.36f,0.30f,0.32f, 440 },
    { "sunny",    88, 10, 0, 1, 0.44f,0.24f,0.26f,0.18f, 300 },
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
static int   toneSel   = 1;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   songCount = 0;
static float vu = 0, platter = 0;     // platter = the turntable angle (visual)
static int   gvEP[3] = { 60, 64, 67 }; static bool epInit = false;
static int   bassLast = 40, vinylH = -1;
static char  nowChord[4][8];

static void apply_fx(void);
static void apply_chair(int idx);

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
    epInit = false; bassLast = 40;
    songCount++;
}
static void fresh_song(double pos) { new_song(pos, 0); rad_hist_log(&rs); }

// ── harmony ───────────────────────────────────────────────────────────────
static int  loop_len(void) { return LOOPS[sng.loop].n; }
static Ch   chord_at(long bar) { return LOOPS[sng.loop].c[(bar / 2) % loop_len()]; }
static int  root_pc(Ch c)  { return (sng.keyPc + c.off) % 12; }
static void chord_label(char *out, int n, Ch c) { snprintf(out, n, "%s%s", RAD_PCNAME[root_pc(c)], QN[c.q]); }
static int  bass_peek(int pc, int lo, int hi) {
    int dd = ((pc - bassLast) % 12 + 18) % 12 - 6, m = bassLast + dd;
    while (m < lo) m += 12; while (m > hi) m -= 12; return m;
}
static int deg_midi(int deg, int lo, int hi) {
    const int *sc = SCALES[sng.scale];
    int o = 5; while (deg < 0) { deg += 7; o--; } while (deg >= 7) { deg -= 7; o++; }
    int m = o * 12 + sng.keyPc + sc[deg];
    while (m < lo) m += 12; while (m > hi) m -= 12; return m;
}

// ── the step player — THE DRUNK POCKET lives in the dly offsets ─────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  step = (int)(s % 16);
    long bar  = s / 16;
    int  cs   = (int)(s % 32);
    Ch   c    = chord_at(bar);
    int  rt   = root_pc(c);

    // the pocket: drag amounts in ms, scaled by the knob (and a little humanize wobble)
    float pk = POCKETV[pocketSel];
    int   snDrag = (int)(pk * stepMs * 0.24f) + (pk > 0 ? rnd((int)(pk * 14) + 1) : 0);   // snare LATE
    int   kkDrag = (int)(pk * stepMs * 0.06f);                                            // kick lazy
    int   epDrag = (int)(pk * stepMs * 0.12f) + (pk > 0 ? rnd((int)(pk * 10) + 1) : 0);   // keys behind
    int   swing  = (step % 2) ? (int)(pk * stepMs * 0.30f) : 0;                           // offbeat hats swung

    // ── THE DUSTY KIT — soft boom kick, fat lazy snare, swung hats ──
    if (step == 0 || (step == 8 && bar % 2 == 0)) { schedule_hit(dly + kkDrag, 34, I_KICK, 6, 120); vu += 1.0f; }
    if (step == 10 && chance(35)) schedule_hit(dly + kkDrag, 34, I_KICK, 4, 100);          // a ghost kick
    if (step == 4 || step == 12)  { schedule_hit(dly + snDrag, 60, I_SNR, 5, 120); vu += 0.9f; }   // the dragged backbeat
    if (step % 2 == 0 || pk > 0.5f)
        schedule_hit(dly + swing + rnd(2), 90, I_HAT, step % 4 == 0 ? 2 : 1, 26);

    // ── RHODES — lush jazz comp, a touch behind, re-voiced on the change ──
    if (step == 0 || step == 8) {
        rad_lead_to(rt, QV[c.q], gvEP, 3, 54, 74, &epInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + epDrag + k * 9 + rnd(3), gvEP[k], I_EP, step == 0 ? 4 : 3, (int)(stepMs * 7));
        vu += 1.0f;
    }

    // ── BASS — round, walking, gently behind ──
    if (step == 0) { int n = bass_peek(rt, 31, 45); bassLast = n;
        schedule_hit(dly + (int)(pk * stepMs * 0.08f) + 4, n, I_BASS, 5, (int)(stepMs * 6)); vu += 0.8f; }
    else if (step == 10 && chance(50)) {                                                   // a passing note
        int nb = bass_peek(root_pc(chord_at(bar + 1)), 31, 45);
        schedule_hit(dly + 4, nb, I_BASS, 3, (int)(stepMs * 2));
    }

    // ── THE SAMPLED DAB — sparse muted horn / vibe, filtered + reverbed, laid back ──
    for (int i = 0; band.c[chDab].sel != 2 && i < sng.cellN; i++)
        if (sng.cellOn[i] == cs && chance(62)) {
            int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - cs : 32 - cs;
            int dur = (int)(gap * stepMs * 0.7); if (dur > 1500) dur = 1500;
            schedule_hit(dly + epDrag + 14 + rnd(8), deg_midi(sng.cellDeg[i], 64, 80), I_DAB, 3, dur);
            vu += 0.7f;
        }
}

// ── the lo-fi rack — SET-AND-HOLD (per song/mood) ────────────────────────────
static void apply_fx(void) {
    const MoodDef *m = &MOOD[sng.mood];
    reverb(m->rev, 0.42f);
    tape(m->wow, m->wow * 0.6f, m->sat);
    echo(m->echoMs, m->echoFb, 0.45f);
    chorus(0.5f, 0.28f, 0.22f);              // a little Rhodes width
}

// ── one-time setup ────────────────────────────────────────────────────────
static void setup_instruments(void) {
    chKeys = rad_chair(&band, "keys", "rhodes", "wurli", NULL, NULL);
    chBass = rad_chair(&band, "bass", "upright", "round", NULL, NULL);
    chDab  = rad_chair(&band, "dab",  "horn", "vibe", "off", NULL);

    instrument(I_EP, INSTR_EPIANO, 2, 0, 6, 1000);
    instrument_harmonics(I_EP, 0.15f); instrument_timbre(I_EP, 0.30f); instrument_morph(I_EP, 0.22f);
    instrument_filter(I_EP, FILTER_LOW, 1900, 1);
    instrument_chorus(I_EP, 0.6f, 0.28f, 0.26f);
    instrument_pan(I_EP, -0.12f);

    instrument(I_BASS, INSTR_TRI, 3, 280, 5, 150);          // upright-ish (chair → round sine)
    instrument_filter(I_BASS, FILTER_LOW, 560, 1);
    instrument_env(I_BASS, 0, ENV_PITCH, 0, 16, 3);

    instrument(I_DAB, INSTR_REED, 2, 0, 4, 900);            // muted horn (chair → vibe)
    instrument_harmonics(I_DAB, 0.78f); instrument_timbre(I_DAB, 0.28f); instrument_morph(I_DAB, 0.5f);
    instrument_filter(I_DAB, FILTER_LOW, 2000, 1);
    instrument_pan(I_DAB, 0.16f);

    instrument(I_KICK, INSTR_SINE, 0, 150, 0, 70); instrument_filter(I_KICK, FILTER_LOW, 220, 2);
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 44, 12);
    instrument(I_SNR, INSTR_NOISE, 1, 0, 0, 130); instrument_filter(I_SNR, FILTER_BAND, 1300, 3);  // fat, dark
    instrument(I_HAT, INSTR_NOISE, 0, 22, 0, 16); instrument_filter(I_HAT, FILTER_HIGH, 6400, 2);

    instrument_reverb(I_EP, 0.26f); instrument_reverb(I_DAB, 0.40f); instrument_reverb(I_SNR, 0.22f);
    instrument_echo(I_DAB, 0.16f);

    instrument(I_VINYL, INSTR_NOISE, 200, 400, 6, 600);     // a FAINT thin tape-hiss floor
    instrument_filter(I_VINYL, FILTER_HIGH, 7200, 2);       // high-pass = airy hiss, not midrange rain

    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);
}

static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chKeys) {
        if (sel == 0) { instrument_harmonics(I_EP, 0.15f); instrument_timbre(I_EP, 0.30f); }   // Rhodes
        else          { instrument_harmonics(I_EP, 0.50f); instrument_timbre(I_EP, 0.42f); }   // Wurli
    } else if (idx == chBass) {
        if (sel == 0) { instrument(I_BASS, INSTR_TRI, 3, 280, 5, 150); instrument_env(I_BASS, 0, ENV_PITCH, 0, 16, 3); }
        else          { instrument(I_BASS, INSTR_SINE, 4, 300, 5, 200); }
        instrument_filter(I_BASS, FILTER_LOW, 560, 1);
    } else if (idx == chDab) {
        if (sel == 0) { instrument(I_DAB, INSTR_REED, 2, 0, 4, 900);                            // horn
                        instrument_harmonics(I_DAB, 0.78f); instrument_timbre(I_DAB, 0.28f); instrument_morph(I_DAB, 0.5f); }
        else if (sel == 1) { instrument(I_DAB, INSTR_MALLET, 1, 0, 7, 900);                     // vibe
                        instrument_harmonics(I_DAB, 0.30f); instrument_timbre(I_DAB, 0.5f); instrument_morph(I_DAB, 0.6f); }
        instrument_filter(I_DAB, FILTER_LOW, 2000, 1);
        instrument_reverb(I_DAB, 0.40f); instrument_echo(I_DAB, 0.16f);
        // sel 2 = off — gated at play time
    }
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_EP,  FILTER_LOW, (int)(1900 * tm), 1);
    instrument_filter(I_DAB, FILTER_LOW, (int)(2000 * tm), 1);
    instrument_filter(I_HAT, FILTER_HIGH, (int)(6400 * tm), 2);
}

// ── update ──────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

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
        long st; while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
        if (scheduled - songBase >= 64L * 16) fresh_song(pos);
        for (int i = 0; i < 4; i++) chord_label(nowChord[i], 8, LOOPS[sng.loop].c[i % loop_len()]);
        platter += dt() * (tempo / 60.0f) * 1.2f;       // the turntable spins with the tempo
    }
    // (cassette wobble now lives in tape()'s wow/flutter — no varispeed: holding it off-speed
    //  keeps the resample ring engaged and drifts the timing; tape's wow is built for this.)
    vu *= 0.90f; if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase; long tbar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("mood", "%s", MOOD[sng.mood].name);
    watch("key", "%s", RAD_PCNAME[sng.keyPc]);
    watch("chord", "%s", nowChord[(int)((tbar / 2) % loop_len())]);
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
        int ci = (int)((bar / 2) % loop_len()), x = 152;
        for (int i = 0; i < loop_len(); i++) {
            int cw = text_width(nowChord[i]); if (x + cw > 292) break;
            if (i == ci) { rectfill(x - 2, 104, cw + 4, 12, ACC); print(nowChord[i], x, 106, CLR_BLACK); }
            else print(nowChord[i], x, 106, CLR_DARK_ORANGE);
            x += cw + 8;
        }
        rad_phrase_dots(232, 124, 8, (bar / 8) % 8, ACC);
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
