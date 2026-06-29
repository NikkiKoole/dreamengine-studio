/* de:meta
{
  "title": "plaid fm",
  "status": "active",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "generative-melody",
    "polymeter",
    "generative-sequencer",
    "fm-synth"
  ],
  "homage": "Plaid (Andy Turner & Ed Handley) - warm melodic Warp IDM",
  "description": "Plaid's warm, melodic IDM, generated forever - the GENTLE pole of Warp, the anti-braindance/squarepusher: intricate but tender, never aggressive. The brains, cart-side over radio.h's clock: INTERLOCKING ARPEGGIOS (the headline) - 3-4 little bell/mallet/pluck cells, each cycling at its OWN length (5/7/8/9/11 steps) against the bar, so they phase and weave and the melody EMERGES from the overlap (the melodic cousin of gamelan's kotekan + eno's coprime loops, but pitched arps over harmony; all in one lush mode so every overlap is consonant). ODD METERS THAT FLOW - the bar is a rolled odd length (5/8, 7/8, 5/4...) and the soft broken-beat kit smooths it so it lilts rather than limps. MODAL DRIFT - a slow root walk under the weave (pad chord + bass) gently re-colours the fixed arps, the Plaid re-harmonisation, bittersweet maj7/9. FORM = ACCRETION - arps enter and recede on an arc, the weave thickening to a peak mid-track and thinning out, with a breakdown. The window WEAVES A PLAID: each arp is a coloured thread (its cycle length sets the stripe spacing), warp crossing weft, brightening where they overlap and when the arp sounds - the interlocking-arp brain drawn as the emerging tartan (the name). SPACE next track, R replay, [ ] history, LEFT/RIGHT weave density (sparse/warm/woven/full), UP/DOWN tempo, T tone, B band (arps bells/softer, kit broken/brushed), M power, H help. Pin via PLAID_SEED."
}
de:meta */
// ── PLAID FM ─────────────────────────────────────────────────────────────────
// Plaid's warm, melodic IDM, generated forever — the GENTLE pole of Warp, the
// anti-braindance/squarepusher: intricate but tender, never aggressive. Full
// design: docs/design/plaid-blind-brief.md.
//
// The brains (cart-side, over radio.h's clock):
//   • INTERLOCKING ARPEGGIOS (the headline, new brain) — 3-4 little bell/mallet/
//     pluck cells, each cycling at its OWN length (5/7/8/9/11 steps) against the
//     bar, so they phase and weave and the melody EMERGES from the overlap. The
//     melodic cousin of gamelan's kotekan + eno's coprime loops, but pitched arps
//     over a slow modal harmony. All in one lush mode → every overlap is consonant.
//   • ODD METERS THAT FLOW — the bar is a rolled odd length (5/8, 7/8, 5/4…); the
//     soft broken-beat kit smooths it so it lilts rather than limps.
//   • MODAL DRIFT — a slow root walk under the weave (pad chord + bass), gently
//     re-colouring the fixed arps (the Plaid re-harmonisation). Bittersweet maj7/9.
//   • FORM = ACCRETION — arps enter and recede on an arc; the weave thickens to a
//     peak mid-track and thins out. No verse/chorus.
//
// The window WEAVES A PLAID: each arp is a coloured thread (its cycle length sets
// the stripe spacing), warp crossing weft, brightening where they overlap and when
// the arp sounds — the interlocking-arp brain drawn as the emerging tartan (the name).
//
//   SPACE next track   R replay   [ ] history   LEFT/RIGHT weave (density)
//   UP/DOWN tempo   T tone   B band   H help

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define PLAID_SEED 0   // pin a favourite here (0 = free-roaming radio)

// ── instrument slots (5..14) ────────────────────────────────────────────────
#define I_ARP1 5    // glassy FM bell
#define I_ARP2 6    // MALLET (marimba/celesta)
#define I_ARP3 7    // soft PLUCK
#define I_ARP4 8    // high FM sparkle
#define I_PAD  9    // lush detuned pad bed
#define I_BASS 10   // round gentle bass
#define I_KICK 11
#define I_SNARE 12
#define I_HAT  13
#define I_WOOD 14   // woodblock / rim accent

#define NARP 4

// ── lush modes (one per track; everything stays in it → consonant overlaps) ──
enum { MO_LYDIAN, MO_MAJOR, MO_DORIAN, MO_MIXO, NMODE };
static const int MODESC[NMODE][7] = {
    { 0, 2, 4, 6, 7, 9, 11 },   // lydian   — bright, floating
    { 0, 2, 4, 5, 7, 9, 11 },   // major    — warm
    { 0, 2, 3, 5, 7, 9, 10 },   // dorian   — bittersweet
    { 0, 2, 4, 5, 7, 9, 10 },   // mixolyd  — amber
};
static const char *MNAME[NMODE] = { "lydian", "major", "dorian", "mixolyd" };

static int deg_to_midi(int deg, int center, const int *m) {
    int oct = 0;
    while (deg < 0)  { deg += 7; oct--; }
    while (deg >= 7) { deg -= 7; oct++; }
    return center + m[deg] + oct * 12;
}

// ── the meters (odd, flowing) ────────────────────────────────────────────────
static const int METER[5]   = { 10, 14, 20, 12, 16 };   // 5/8, 7/8, 5/4, 3/4, 4/4
static const char *METNM[5]  = { "5/8", "7/8", "5/4", "3/4", "4/4" };
// the modal root drift weights (degree steps) — gentle, neighbour-ish
static const int RWALK[8] = { 3, -3, 4, -4, 2, -2, 5, 0 };

// ── an interlocking arp ───────────────────────────────────────────────────────
typedef struct {
    int L;            // cycle length in steps (its own, against the bar)
    int on[12];       // onset mask over the L positions
    int deg[12];      // mode-degree at each position (a fixed in-mode figure)
    int slot, base, col, warp;   // voice, register, thread colour, warp/weft
} Arp;

// ── the generated track ──────────────────────────────────────────────────────
typedef struct {
    int   mode, keyPc, meterIdx;
    Arp   arp[NARP];
    int   rootSeq[8];          // the slow modal root drift (per 2 bars)
    int   nbars;
    int   breakAt;             // a breakdown section (bar/2 index)
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
#define srnd(n)   rad_srnd(&rs, (n))

static int   tempo     = 104;
static int   intensity = 2;
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 2;
static int   songCount = 0;
static float vu = 0;
static float arpGlow[NARP];
static int   gvPad[3] = { 55, 59, 62 };
static bool  padInit  = false;
static int   bassMidi = 40;

static RadBand band;
static int chArps, chKit;

static void apply_chair(int idx);   // defined below; new_track re-asserts band picks

static int barSteps(void)  { return METER[trk.meterIdx]; }
static long song_steps(void) { return (long)trk.nbars * barSteps(); }

// ── the woven-thread palette ───────────────────────────────────────────────
static const int THREAD[8] = {
    CLR_DARK_RED, CLR_BLUE_GREEN, CLR_ORANGE, CLR_INDIGO,
    CLR_MEDIUM_GREEN, CLR_MAUVE, CLR_DARK_BLUE, CLR_PINK };

// ── track generation ─────────────────────────────────────────────────────────
static const char *TW1[] = { "Eyen", "Itsu", "Ralome", "Cedar", "Zn", "Tak", "Crumax",
    "Lambed", "Twin", "Light", "Even", "Dang" };
static const char *TW2[] = { "Rhomb", "Spokes", "Recall", "Maker", "Locus", "Air",
    "Thus", "Zero", "Bowls", "Ti", "Lin", "Ome" };

static void gen_arps(void) {
    static const int LSET[6]  = { 5, 6, 7, 8, 9, 11 };
    static const int OCT[NARP] = { 72, 60, 48, 79 };          // bells/mallet/pluck/sparkle
    static const int SLOT[NARP] = { I_ARP1, I_ARP2, I_ARP3, I_ARP4 };
    int used = 0, cstart = srnd(8);
    for (int i = 0; i < NARP; i++) {
        int L; do { L = LSET[srnd(6)]; } while (used & (1 << L)); used |= 1 << L;
        Arp *a = &trk.arp[i];
        a->L = L; a->slot = SLOT[i]; a->base = OCT[i] + trk.keyPc;
        a->col = THREAD[(cstart + i * 2) % 8]; a->warp = i % 2;
        int d = srnd(5) - 2;
        for (int p = 0; p < L; p++) {
            a->on[p]  = (srnd(100) < (i == 0 ? 82 : 68)) ? 1 : 0;
            d += srnd(5) - 2; if (d > 6) d -= 7; if (d < -2) d += 7;
            a->deg[p] = d;
        }
        a->on[0] = 1;                                         // a phase anchor
    }
}

static void new_track(double pos, unsigned seed) {
    trk.seed = rad_seed_begin(&rs, seed);
    trk.mode  = srnd(NMODE);
    trk.keyPc = srnd(12);
    int mr = srnd(100); trk.meterIdx = mr < 30 ? 1 : mr < 52 ? 0 : mr < 70 ? 2 : mr < 86 ? 4 : 3; // bias odd
    gen_arps();
    // the modal root drift (gentle walk over mode degrees)
    int rd = 0; trk.rootSeq[0] = 0;
    for (int i = 1; i < 8; i++) { rd = ((rd + RWALK[srnd(8)]) % 7 + 7) % 7; trk.rootSeq[i] = rd; }
    trk.nbars   = 24 + srnd(9);                               // 24..32
    trk.breakAt = 4 + srnd(trk.nbars / 2 - 4 > 1 ? trk.nbars / 2 - 4 : 1);   // a breakdown
    snprintf(trk.title, sizeof trk.title, "%s %s", TW1[srnd(12)], TW2[srnd(12)]);
    trk.freq = 88.0f + srnd(190) * 0.1f;
    tempo = 92 + srnd(28);                                    // 92..120
    bpm(tempo);
    for (int i = 0; i < band.n; i++) if (band.c[i].sel) apply_chair(i);  // (re-assert picks)
    songBase = (long)pos + 16;
    padInit = false; bassMidi = 40;
    songCount++;
}
static void fresh_track(double pos) { new_track(pos, 0); rad_hist_log(&rs); }

// ── form ──────────────────────────────────────────────────────────────────────
static bool breakdown(long bar) { return (bar / 2) == trk.breakAt || (bar / 2) == trk.breakAt + 1; }
static int layer_at(long bar) {                              // 1..NARP active arps (the accretion arc)
    float p = trk.nbars > 0 ? (float)bar / trk.nbars : 0;
    int base = 1 + (int)(sinf(p * 3.14159f) * 2.6f + 0.5f);  // arcs 1 -> ~3.6 -> 1
    base += (intensity - 2);
    if (breakdown(bar)) base = 1;                            // the gentle drop
    if (base < 1) base = 1; if (base > NARP) base = NARP;
    return base;
}

// ── the step player ─────────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int  bs   = barSteps();
    int  step = (int)(s % bs);
    long bar  = s / bs;
    if (bar >= trk.nbars) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    const int *m = MODESC[trk.mode];
    int  layer = layer_at(bar);
    int  rootDeg = trk.rootSeq[(int)((bar / 2) % 8)];
    int  croot = (trk.keyPc + m[rootDeg]) % 12;

    // ── THE INTERLOCKING ARPS — each cycles on the GLOBAL step at its own L ──
    for (int i = 0; i < layer; i++) {
        Arp *a = &trk.arp[i];
        int lp = (int)(s % a->L);
        if (!a->on[lp]) continue;
        int midi = deg_to_midi(a->deg[lp], a->base, m);
        int vol = (i == 0 ? 4 : 3);
        schedule_hit(dly + (i * 2), midi, a->slot, vol, (int)(stepMs * 3));
        arpGlow[i] = 1.0f; vu += 0.9f;
    }

    // ── MODAL PAD — the bittersweet chord on the current root (every 2 bars) ──
    if (step == 0 && (bar % 2 == 0) && !breakdown(bar)) {
        int iv[3] = { 0, m[(rootDeg + 2) % 7] - m[rootDeg], m[(rootDeg + 4) % 7] - m[rootDeg] };
        rad_lead_to(croot, iv, gvPad, 3, 52, 70, &padInit);
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 9, gvPad[k], I_PAD, 3, (int)(stepMs * bs * 2));
        vu += 0.7f;
    }

    // ── THE BASS — roots the downbeat (locks with the kick), a fifth mid-bar ──
    if (step == 0) {
        bassMidi = 28 + croot; while (bassMidi < 28) bassMidi += 12;
        schedule_hit(dly + 2, bassMidi, I_BASS, 5, (int)(stepMs * (bs > 12 ? 5 : 4)));
        vu += 1.0f;
    } else if (step == bs / 2 && layer >= 2 && !breakdown(bar)) {
        schedule_hit(dly + 4, bassMidi + 7, I_BASS, 3, (int)(stepMs * 3));
    }

    // ── THE BROKEN-BEAT KIT + the kick/bass pocket — present almost throughout
    // (Plaid is beat-driven; only the breakdown strips back to arps-only) ──────
    if (layer >= 1 && !breakdown(bar)) {
        if (step == 0) { schedule_hit(dly, 36, I_KICK, 5, 90); vu += 1.0f; }            // the downbeat, with the bass
        if (step == (bs * 3) / 5) schedule_hit(dly, 36, I_KICK, 4, 80);                 // the broken 2nd kick
        if (step == bs / 2) { schedule_hit(dly, 60, I_SNARE, 4, 60); vu += 0.7f; }      // off-centre backbeat
        if (layer >= 2 && step % 2 == 0)                                               // hats fill in with the weave
            schedule_hit(dly + rnd(3), 90, I_HAT, (step % 4 == 0) ? 2 : 1, 18);
        if (layer >= 2 && (step == bs - 3 || step == bs - 1) && chance(35))
            schedule_hit(dly, 74, I_WOOD, 2, 40);
    }
}

// ── the band chairs ──────────────────────────────────────────────────────────
static void apply_chair(int idx) {
    int sel = band.c[idx].sel;
    if (idx == chArps) {
        // arp 1 — glassy FM bell
        instrument(I_ARP1, INSTR_FM, 1, 360, 0, 500);
        instrument_harmonics(I_ARP1, sel == 0 ? 0.62f : 0.5f);   // bell ratio
        instrument_timbre(I_ARP1, 0.38f); instrument_morph(I_ARP1, 0.12f);
        instrument_filter(I_ARP1, FILTER_LOW, 2900, 1);   // tame the glassy top
        // arp 2 — MALLET marimba/celesta
        instrument(I_ARP2, INSTR_MALLET, 1, 0, 7, 520);
        instrument_harmonics(I_ARP2, sel == 0 ? 0.35f : 0.7f);   // wood -> bell
        instrument_timbre(I_ARP2, 0.5f); instrument_morph(I_ARP2, 0.3f);
        // arp 3 — soft PLUCK
        instrument(I_ARP3, INSTR_PLUCK, 1, 0, 7, 600);
        instrument_harmonics(I_ARP3, 0.5f); instrument_timbre(I_ARP3, 0.35f); instrument_morph(I_ARP3, 0.4f);
        instrument_filter(I_ARP3, FILTER_LOW, 2400, 1);
        // arp 4 — high FM sparkle
        instrument(I_ARP4, INSTR_FM, 0, 300, 0, 420);
        instrument_harmonics(I_ARP4, 0.7f); instrument_timbre(I_ARP4, 0.40f); instrument_morph(I_ARP4, 0.1f);
        instrument_filter(I_ARP4, FILTER_LOW, 3300, 1);   // the harsh sparkle, rolled off
        instrument_tune(I_ARP1, 0.04f); instrument_tune(I_ARP4, -0.04f);   // a hair of warmth
    } else if (idx == chKit) {
        if (sel == 0) {                                          // soft broken-beat
            instrument(I_KICK, INSTR_SINE, 0, 150, 0, 55); instrument_filter(I_KICK, FILTER_LOW, 280, 2);
            instrument_env(I_KICK, 0, ENV_PITCH, 0, 48, 17);
            instrument(I_SNARE, INSTR_NOISE, 0, 60, 0, 50); instrument_filter(I_SNARE, FILTER_BAND, 1700, 4);
        } else {                                                 // brushed / softer
            instrument(I_KICK, INSTR_SINE, 0, 150, 0, 60); instrument_filter(I_KICK, FILTER_LOW, 220, 1);
            instrument_env(I_KICK, 0, ENV_PITCH, 0, 50, 10);
            instrument(I_SNARE, INSTR_NOISE, 2, 90, 0, 70); instrument_filter(I_SNARE, FILTER_BAND, 1300, 3);
        }
        instrument(I_HAT, INSTR_NOISE, 0, 16, 0, 12); instrument_filter(I_HAT, FILTER_HIGH, 6000, 2);
        instrument(I_WOOD, INSTR_SINE, 0, 50, 0, 36); instrument_filter(I_WOOD, FILTER_BAND, 1500, 6);
        instrument_env(I_WOOD, 0, ENV_PITCH, 0, 22, 5);
    }
}

static void setup_instruments(void) {
    chArps = rad_chair(&band, "arps", "bells", "softer", NULL, NULL);
    chKit  = rad_chair(&band, "kit",  "broken", "brushed", NULL, NULL);
    // the pad + bass aren't chairs — set once
    instrument(I_PAD, INSTR_SAW, 240, 400, 6, 900);
    instrument_filter(I_PAD, FILTER_LOW, 1500, 2);
    instrument_tune(I_PAD, 0.05f);
    instrument(I_BASS, INSTR_SINE, 4, 260, 4, 220);
    instrument_filter(I_BASS, FILTER_LOW, 600, 1);
    for (int i = 0; i < band.n; i++) apply_chair(i);
    reverb(0.6f, 0.4f);                                          // a gentle, spacious room
    tape(0.26f, 0.14f, 0.16f);                                  // warm drift — wow/flutter for the tape wobble, light sat (no squash)
}

static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    instrument_filter(I_PAD,  FILTER_LOW, (int)(1500 * tm), 2);
    instrument_filter(I_ARP3, FILTER_LOW, (int)(2400 * tm), 1);
    instrument_filter(I_HAT,  FILTER_HIGH, (int)(6000 * tm), 2);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (PLAID_SEED) { new_track(pos, PLAID_SEED); rad_hist_log(&rs); }
        else fresh_track(pos);
        scheduled = (long)pos;
        apply_tone();
        booted = true;
    }

    int ev = rad_input(&tempo, 80, 140, 2, &intensity, &toneSel, 4, &radioOn, &showHelp);
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
        if (scheduled - songBase >= song_steps()) fresh_track(pos);
    }
    vu *= 0.88f; if (vu > 12) vu = 12;
    for (int i = 0; i < NARP; i++) { arpGlow[i] -= dt() * 3.0f; if (arpGlow[i] < 0) arpGlow[i] = 0; }

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long tbar = ss >= 0 ? ss / barSteps() : 0;
    watch("track", "%d", songCount);
    watch("mode", "%s", MNAME[trk.mode]);
    watch("key", "%s", RAD_PCNAME[trk.keyPc]);
    watch("meter", "%s", METNM[trk.meterIdx]);
    watch("layer", "%d", layer_at(tbar));
    watch("Ls", "%d/%d/%d/%d", trk.arp[0].L, trk.arp[1].L, trk.arp[2].L, trk.arp[3].L);
    watch("tempo", "%d", tempo);
#endif
}

// ── draw — the woven plaid (the interlocking-arp brain made visible) ─────────
#define WX 34
#define WY 52
#define WW 102
#define WH 116

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / barSteps() : 0;
    int  layer = radioOn ? layer_at(bar) : 0;

    rad_body(CLR_DARK_GREY, CLR_MAUVE);
    rad_dial(trk.freq, CLR_MAUVE);

    // the window — the plaid weave
    rectfill(WX, WY, WW, WH, CLR_BLACK);
    for (int i = 0; i < layer; i++) {
        Arp *a = &trk.arp[i];
        int spacing = 5 + a->L;                              // its cycle length = the stripe spacing
        int lp = (int)(songStep >= 0 ? (songStep % a->L) : 0);
        float g = arpGlow[i];
        int col = a->col;
        int bg  = (g > 0.5f) ? col : CLR_DARK_GREY;          // dim thread, bright on a hit
        if (a->warp) {                                       // vertical threads (warp)
            int off = (lp * spacing) / (a->L > 0 ? a->L : 1);
            for (int x = WX + 1 + off % spacing; x < WX + WW; x += spacing) {
                line(x, WY + 1, x, WY + WH - 1, bg);
                if (g > 0.2f) line(x + 1, WY + 1, x + 1, WY + WH - 1, col);   // thicken on glow
            }
        } else {                                             // horizontal threads (weft)
            int off = (lp * spacing) / (a->L > 0 ? a->L : 1);
            for (int y = WY + 1 + off % spacing; y < WY + WH; y += spacing) {
                line(WX + 1, y, WX + WW - 1, y, bg);
                if (g > 0.2f) line(WX + 1, y + 1, WX + WW - 1, y + 1, col);
            }
        }
    }
    rect(WX, WY, WW, WH, CLR_DARK_GREY);

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_MAUVE);
    if (radioOn) {
        print(trk.title, 154, 58, CLR_MAUVE);
        char l2[32];
        snprintf(l2, 32, "%s %s  %s", RAD_PCNAME[trk.keyPc], MNAME[trk.mode], METNM[trk.meterIdx]);
        print(l2, 154, 70, CLR_LIGHT_GREY);
        snprintf(l2, 32, "%d bpm #%08X", tempo, trk.seed);
        print(l2, 154, 82, CLR_LIGHT_GREY);
        float vt = vu / 12.0f; rectfill(154, 91, (int)((vt > 1 ? 1 : vt) * 80), 2, CLR_MAUVE);
    } else print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        char l3[24]; snprintf(l3, 24, "weave %d", layer);
        print(l3, 152, 104, CLR_MAUVE);
        // the four thread Ls, lit by their glow — the interlock, legible
        for (int i = 0; i < NARP; i++) {
            int on = i < layer;
            char ll[6]; snprintf(ll, 6, "%d", trk.arp[i].L);
            print(ll, 220 + i * 16, 104, on ? (arpGlow[i] > 0.3f ? CLR_WHITE : trk.arp[i].col) : CLR_DARK_GREY);
        }
        rad_phrase_dots(232, 120, 8, (bar / 2) % 8, CLR_MAUVE);
    }

    static const char *FEEL[4] = { "sparse", "warm", "woven", "full" };
    rad_knob_sel(&intensity, 4, 168, 148, 9, FEEL[intensity], CLR_MAUVE);
    if (rad_knob_int(&tempo, 80, 140, 2, 218, 148, 9, "tempo", CLR_MAUVE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_MAUVE)) apply_tone();
    rad_power_led(radioOn, CLR_MAUVE, CLR_DARK_GREY);

    rad_help_button(CLR_MAUVE);
    rad_band_button(CLR_MAUVE);
    if (showHelp) {
        static const char *HELP[8][2] = {
            { "SPACE",      "next track (new seed)" },
            { "R",          "replay this one" },
            { "[ / ]",      "back / forward history" },
            { "LEFT/RIGHT", "weave - sparse..full" },
            { "UP/DOWN",    "tempo" },
            { "T",          "tone" },
            { "M",          "radio on / off" },
            { "B",          "band - arps/kit" },
        };
        static const char *NOTES[3] = {
            "3-4 arps cycle at DIFFERENT lengths and",
            "interlock - the tune emerges from the weave,",
            "drawn as the plaid. odd meters that flow.",
        };
        rad_help_panel("PLAID FM", HELP, 8, NOTES, 3, CLR_MAUVE);
    }
    rad_band_panel(&band, CLR_MAUVE);
    ui_end();
}
