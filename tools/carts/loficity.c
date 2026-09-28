/* de:meta
{
  "slug": "loficity",
  "title": "lofi city",
  "status": "active",
  "created": "2026-09-28",
  "kind": [
    "toy",
    "instrument"
  ],
  "teaches": [
    "song-arrangement",
    "chord-voicing",
    "generative-melody",
    "swing-timing"
  ],
  "homage": "Lofi Cities (loficities.com, Safa Elmali) - endless lofi composed live in the browser",
  "lineage": "A study port of Lofi Cities' arranger, all nine styles (jazzhop, lofi piano, ambient, bossa nova, synth city pop, lofi house, chill guitar, sad lofi, medieval), verified bit-identical against the site's own planner run headless in node (540 runs x 3 chained tracks x every style/energy/band/city: 1,045,164 events to 9 decimals). The arrangement is theirs, line for line (runtime/loficity/); the SOUND is ours - each style cast on our modeled engines. Sibling of lofi.c (the radio station), which it shares no voices with.",
  "todo": [
    "Style change: the master eq (each style's overall gain) switches at once, so the OLD style's ringing tail swells or ducks for a moment (ambient -> bossa peaked -0.4 dBFS). A per-style gain that rides smoothly would need levels that can boost.",
    "Voice nuances not yet cast: medieval's recorder grace notes (cuts/taps from medieval.mode), sad's horn lean + fall-off, house's per-note random vox vowel, synth/house lead legato by interval, ambient pad's per-note L/R pan.",
    "VINYL is too heavy (heard 2026-09-28): the hiss bed (I_HISS level 0.30 x note_vol 2.2*vinylG) and the crackle ticks (9/s x dust x vinylG, level 0.60) sit well above theirs (their hiss 0.006, crackle 0.05 x dust). Tone both down, and make the intro/outro vinyl swell (VINYL_BOOST) gentler.",
    "The 2-bar wow drift (their fx 'wow' events) is not ridden: tape() rebuilds its DSP. Per-track wow only.",
    "Velocity is quantised to our 0..7 vol, so their +-8% humanise mostly vanishes; timing jitter survives intact.",
    "Ear pass per style: the casting and the per-style mix (STYLE_MIX) were set by measurement (per-part RMS vs jazzhop's proportions), not yet by listening."
  ],
  "description": {
    "summary": "Endless generated lofi in all nine Lofi Cities styles, arranged exactly the way loficities.com arranges it - played on our own engines.",
    "detail": "Every track is one seed, and the seed plans everything the way loficities.com does, for whichever of its nine styles you pick: a key (each track moves to a related key), two progressions from the style's bank, 1 or 2 bars per chord, a tempo/swing/snare-lag from the style's ENERGY table, a drum groove from the style's own grids, a form (T1-T3, grown to 2.5-4 minutes) whose every section switches layers on and off. Per bar: fills, B-section variations, the keys comping by the style's own cells (fingerpicking patterns, bossa batida, house stabs, piano rolls) and PUSHING the next chord, the style's own bass line (a piano left hand, a viol walk, a synth bass cell, a house cell), a motif lead, and the master tone automated (the intro opening from 900 Hz, the break dipping, the outro closing). Each style has its own band on our engines: Rhodes, felt grand, unison-saw pads + drone, nylon and electric guitars with held figures and ghost strums, a driven organ with a pump, a lute with courses over a bowed viol and a drone, a muted brass horn, recorders, FM bells, a morphing kit with style-specific percussion. The screen is the arrangement made visible: the form strip with the playhead, what each part is doing, the chord, fills and pushes, and what's up next.",
    "controls": "S style (from the next track) . N / SPACE next track . E energy (chill/balanced/upbeat, next track) . B band (full / no drums / chords only, next bar) . C city (the words the titles are made of) . 1-5 fx off/on (tone . tape . bus . trem . vinyl) . H help"
  }
}
de:meta */
// ── LOFI CITY ─────────────────────────────────────────────────────────────────
// Lofi Cities' arranger (all nine styles), ported line for line, played on dreamengine's engines.
//
// The ARRANGEMENT half (planTrack / planBar / barHits / phrase / voice) is a port of
// the site's own JavaScript — same RNG (their hash + rng, as uint32), the same five
// seeded streams per track and the same draw ORDER, so a seed plans the same track the
// site would. Verified bit-identical against their bundle run headless in node
// (300 seeds × 4 chained tracks × every energy/band/city, 643,600 events to 9 decimals).
// Build with -DLC_DUMP for a standalone binary that prints every planned event, which is
// how that check was made (the site's code itself is not in this repo).
// The SOUND half is ours: see "THE BAND" below.
//
// Our clock is the SOUND clock, audio_time(), and every note is booked on it with schedule_at()
// 100 ms ahead (their lookahead). Not beat() or summed dt(): those advance by the clamped frame dt, and
// a schedule_hit delay counts from whichever audio callback drains it, which swung notes by up to a
// 23 ms buffer on native (gated by tools/schedule-check).
//
//   S style   N / SPACE next   E energy   B band   C city   1-5 fx   H help

#define LOFI_SEED 0      // pin a seed (0 = a random one each boot)
#define LOFI_STYLE -1    // pin the boot style (0..8 = jazzhop..medieval; -1 = jazzhop)
#define LC_WOW 0.5f      // tape wow scale: 1 = their depth (+-3..12 cents at ~0.5 Hz), which read as seasick
#define LC_FLUTTER 0.03f // tape flutter: ~1.6 cents at 6 Hz, their depth (0.12 was 4x that - an audible warble)
#define LC_SAT 0.0f      // tape saturation: OFF. tape()'s curve is tanh(g*x)/tanh(g), normalised so full scale stays full
                         // scale, so even 0.02 has +2.5 dB small-signal gain and bends the whole range: the drums drove it
                         // into squashing the Rhodes/bass on every hit (mix vs sum-of-parts residual -3.7 dB; at 0: -44 dB)
#define LC_GLUE 0.25f    // bus compressor amount
#define LC_PUMP 1.0f     // house's sidechain pump amount scale (A/B)
#define LC_KSWEEP 1800   // house kick: their sweep TIME CONSTANT (s) -> our LINEAR pitch-env length (ms). x3000 held
                         // the pitch up ~150 ms (a slide, not a thump); theirs is exponential, most of it in ~50 ms
#define LC_HUMAN 1       // the humanize layer on at boot (toggle 6): grace notes, passing tones, bass walks, suspensions
#define LC_TREM 0.5f     // the Rhodes suitcase tremolo + autopan, scaled from the plan's depth (1 = theirs; it read as
                         // a sine wobble on the whole mix because the keys are the loudest part)
#ifndef LC_DUMP
#include "studio.h"
#include "ui.h"
#include "morphdrum.h"
#ifdef DE_SPEC
#include "spec.h"
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// ── the arranger: runtime/loficity/ (a private module, like lockup/): the port of their planner ──
#include "loficity/arranger.h"

#ifdef LC_DUMP
// the oracle dump: every planned value + event of N chained tracks, in the canonical text the
// verification compares against their own planner (clang -DLC_DUMP ... ; ./a.out seed energy band city n style)
static int kvcmp(const void *a, const void *b) { return strcmp(((const KV *)a)->k, ((const KV *)b)->k); }
static void pr_plan(const Plan *P) {
    const StyleDef *S = STYLES[P->style];
    printf("PLAN seed=%u next=%u title=%s key=%d/%s bpm=%d swing=%.4f lag=%.4f pat=%s form=T%d bars=%d A=%s B=%s cb=%d lead=%d reg=%d tone=%.0f wow=%.1f dust=%.2f legato=%d drumLevel=%.4f\n",
        P->seed, P->nextSeed, P->title, P->key.tonic, P->key.major ? "major" : "minor", P->bpm, P->swing, P->snareLag, S->grids[P->pattern].id, P->form + 1, P->bars,
        P->progA.id, P->progB.id, P->chordBars, P->hasLead, P->reg, P->tone, P->wowCents, P->dust, P->legato, P->drumLevel);
    printf("EP %.0f %.2f %.3f %.4f %.3f %.2f comp", P->ep.lp, P->ep.tremRate, P->ep.tremDepth, P->ep.strum, P->ep.vel, P->ep.push);
    for (int i = 0; i < P->ep.ncomp; i++) printf(" %s=%.2f", S->comps ? S->comps[P->ep.compIdx[i]].id : (const char *[]){ "hold", "charleston", "pulse" }[i], P->ep.comp[i]);
    printf("\nSECS");
    for (int i = 0; i < P->nsec; i++) { const Layers *L = &P->sec[i].L;
        printf(" %s%d[%d%d%d%d%d%d]", SEC_NAME[P->sec[i].name], P->sec[i].bars, L->ep, L->bass, L->drums, L->lead, L->hatsFirst, L->dipLast); }
    printf("\n");
    KV kvs_[KV_MAX]; memcpy(kvs_, P->kv, sizeof(KV) * P->nkv); qsort(kvs_, P->nkv, sizeof(KV), kvcmp);
    for (int i = 0; i < P->nkv; i++) { if (kvs_[i].isStr) printf("SP %s=%s\n", kvs_[i].k, kvs_[i].s); else printf("SP %s=%.6f\n", kvs_[i].k, kvs_[i].v + 0.0 == 0 ? 0.0 : kvs_[i].v); }
}
int main(int argc, char **argv) {
    uint32_t seed = (uint32_t)strtoul(argv[1], 0, 10);
    int energy = argc > 2 ? atoi(argv[2]) : 1, band = argc > 3 ? atoi(argv[3]) : 0, city = argc > 4 ? atoi(argv[4]) : 0;
    int ntracks = argc > 5 ? atoi(argv[5]) : 1, style = argc > 6 ? atoi(argv[6]) : 0;
    Key prev; int hasPrev = 0;
    for (int tr = 0; tr < ntracks; tr++) {
        Plan P = plan_track(seed, hasPrev ? &prev : NULL, style, energy, band, city);
        pr_plan(&P);
        static BarState st; bar_state(&P, &st);
        static Evs ev;
        for (int i = 0; i < P.bars; i++) {
            plan_bar(&P, i, &st, &ev);
            for (int k = 0; k < ev.n; k++) { const Ev *e = &ev.e[k];
                printf("E %d %s %.9f", e->bar, K_NAME[e->k], e->t);
                if (e->k == K_FX) printf(" %s %.6f %.4f", FXN[e->fx], e->v, e->tau);
                else if (e->k == K_EP) { printf(" ["); for (int q = 0; q < e->nn; q++) printf("%s%d", q ? "," : "", e->notes[q]); printf("] %.9f %.9f %.4f %.2f", e->vel, e->dur, e->strum, e->rel); }
                else if (e->k == K_BASS) printf(" %d %.9f %.9f %d", e->midi, e->vel, e->dur, e->slide);
                else if (e->k == K_LEAD) printf(" %d %.9f %.9f %d", e->midi, e->vel, e->dur, e->glide);
                else printf(" %.9f %d %d", e->vel, e->open, e->ghost);
                printf("\n"); }
        }
        prev = P.key; hasPrev = 1; seed = P.nextSeed;
    }
    return 0;
}
#else

// ═════════════════════════════════════════════════════════════════════════════
// THE BAND — the planner's events played on our engines. This half is ours: each of
// the nine styles gets its own casting on modeled engines, fed by that style's own plan
// values (their kit rolls, synth/house/guitar settings, the lead's timbre), plus the
// few behaviours their voices perform INTERNALLY (the arranger does not plan them):
//   held-chord figures re-plucked every half bar (bossa / guitar / lute), guitar ghost
//   strums + pinches + up/down strums, lute COURSES (each note doubled), stab-vs-pad by
//   note length (synth / house), piano rolls, sad's pad under the piano, ambient's ties
//   and its thinned bell, the section-driven drones (ambient / medieval), house's
//   section filter sweep + pump, synth's step-pitched tom runs.
// Casting per style (sound notes from their voice modules, recast):
//   jazzhop  EPIANO Rhodes + suitcase trem · BOWED-pizz upright · morphdrum · MALLET vibes / PIPE flute
//   piano    PIANO felt grand (both hands + the melody) · brush kit
//   ambient  unison-SAW pad + an octave shimmer · held SINE/TRI drone · SINE sub · FM bell · felt kit
//   bossa    GUITAR nylon, thumb + fingers · BOWED-pizz upright · brush kit, MODAL clave, MEMBRANE surdo
//   synth    SAW poly (stab + pad slots) + master chorus · SAW mono bass · SQUARE / FM-glass lead · clap, toms
//   house    ORGAN (stab + pad) + section sweep + sidechain pump · SINE bass · VOICE vox / PLUCK · METAL ride
//   guitar   GUITAR electric, driven + chorused + a spring tank · PLUCK finger bass · GUITAR lead
//   sad      PIANO felt + a SAW pad, both through a slot TAPE warble · SINE sub · BRASS muted horn
//   medieval GUITAR lute with courses · BOWED arco viol · SAW drone · PIPE recorder · MEMBRANE frame drums
// Mix: the plan's per-bar TONE automation rides the master lowpass (filter() is built to
// be ridden live); tape wow + echo are per track; a vinyl hiss bed + crackle ticks.
// ═════════════════════════════════════════════════════════════════════════════
// Every modulator a style sets on a slot is RECORDED, so a style change resets only what was
// actually set: a blanket reset of all fifteen slots is ~330 engine calls, and one frame's request
// queue holds 512 (it overflowed on boot). These macros share the functions' names; C does not
// re-expand a macro inside its own expansion, so the inner call is the real function.
static unsigned lc_dirty[48];
#define LC_D(s, bit) (lc_dirty[(s) & 47] |= 1u << (bit))
#define instrument_lfo(s, w, ...)    (LC_D(s, w),      instrument_lfo(s, w, __VA_ARGS__))
#define instrument_env(s, w, ...)    (LC_D(s, 3 + (w)), instrument_env(s, w, __VA_ARGS__))
#define instrument_drive(s, ...)     (LC_D(s, 6),      instrument_drive(s, __VA_ARGS__))
#define instrument_echo(s, ...)      (LC_D(s, 7),      instrument_echo(s, __VA_ARGS__))
#define instrument_reverb(s, ...)    (LC_D(s, 8),      instrument_reverb(s, __VA_ARGS__))
#define instrument_glide(s, ...)     (LC_D(s, 9),      instrument_glide(s, __VA_ARGS__))
#define instrument_tune(s, ...)      (LC_D(s, 10),     instrument_tune(s, __VA_ARGS__))
#define instrument_unison(s, ...)    (LC_D(s, 11),     instrument_unison(s, __VA_ARGS__))
#define instrument_pan(s, ...)       (LC_D(s, 12),     instrument_pan(s, __VA_ARGS__))
#define instrument_level(s, ...)     (LC_D(s, 13),     instrument_level(s, __VA_ARGS__))
#define instrument_filter(s, ...)    (LC_D(s, 14),     instrument_filter(s, __VA_ARGS__))
#define instrument_mode(s, ...)      (LC_D(s, 15),     instrument_mode(s, __VA_ARGS__))
#define instrument_tape(s, ...)      (LC_D(s, 16),     instrument_tape(s, __VA_ARGS__))
#define instrument_chorus(s, ...)    (LC_D(s, 17),     instrument_chorus(s, __VA_ARGS__))
#define instrument_eq(s, ...)        (LC_D(s, 18),     instrument_eq(s, __VA_ARGS__))
#define I_KEYS   5    // keys, short release
#define I_KEYSL  6    // keys, long release (held / outro chords)
#define I_BASS   7
#define I_BASSS  8    // the slid-into bass note (a pitch scoop / glide)
#define I_RIM    9
#define I_LEAD   10
#define I_LEAD2  11   // the lead's other timbre
#define I_HISS   12
#define I_CRK    13
#define I_KEYS2  14   // a second keys layer: guitar thumb, lute course, pad shimmer
#define I_KEYS2L 15   // ...its long-release / muted twin
#define I_PAD    16   // the pad-length chord slot (synth / house) · sad's pad · the ghost strum
#define I_DRONE  17
#define I_CLAP   18
#define I_SHAKER 19
#define KIT_BASE 20   // morphdrum slots 20..29
#define I_TOM    30
#define I_BLOCK  31   // ride / tambourine / slap
#define I_NHAT   32   // house's hats: plain filtered NOISE, as theirs (the METAL bank rang a pitched chuff on every offbeat)
#define I_NHATO  33
static const int STYLE_SLOTS[] = { I_KEYS, I_KEYSL, I_BASS, I_BASSS, I_RIM, I_LEAD, I_LEAD2, I_KEYS2, I_KEYS2L, I_PAD,
                                   I_DRONE, I_CLAP, I_SHAKER, I_TOM, I_BLOCK, I_NHAT, I_NHATO };
#define NSTYLE_SLOTS ((int)(sizeof STYLE_SLOTS / sizeof *STYLE_SLOTS))

static MorphKit kit;
static double clk = 0;                 // our clock, seconds: audio_time() (see the header)
static int    energySel = EN_BALANCED, bandSel = BAND_FULL, citySel = 1, styleSel = S_JAZZHOP;   // tokyo
static bool   showHelp = false;
// FX TOGGLES (keys 1-5 / the buttons top-right): switch each master stage off to hear what it is doing.
// Each re-applies ONLY when flipped (set-and-hold).
enum { FXT_TONE, FXT_TAPE, FXT_BUS, FXT_TREM, FXT_VINYL, FXT_HUMAN, NFXT };
static const char *FXT_NAME[NFXT] = { "tone", "tape", "bus", "trem", "vinyl", "human" };
static bool  fxOn[NFXT] = { true, true, true, true, true, LC_HUMAN };
static float curWow = 0.3f;
static int   lastTone = -1;

typedef struct {
    Plan P; BarState st;
    double start;                      // clock time of bar 0
    int nextBar;
    Ev q[480]; int nq;                 // planned, not yet dispatched (time-sorted)
    Rng hr;                            // the HUMANIZE layer's own stream (never the planner's)
    bool live;
} Track;
static Track cur;
static Plan  upNext[3];                // the queue: the next three tracks of the chain
static double toneHz = 8000, toneTgt = 8000, toneTau = 0.05;
static double vinylG = 1, vinylTgt = 1, vinylTau = 0.5, dustAmt = 1;
static double keysLevel = 0.71, keysBase = 0.71;
static int    hissH = -1, droneH[2] = { -1, -1 };
static float  flash[12];               // per-kind hit flash for the display
static int    fillShow = F_NONE; static float fillT = 0;
static int    pushShow = 0; static float pushT = 0;
static int    songCount = 0;
static double lastLeadAt = -10;        // ambient's bell drops notes that crowd the previous one
static double tieEnd[128];             // ambient pad ties: when each pitch's note ends

static int v2vol(double v, double k) { int x = (int)lround(v * k); return x < 1 ? 1 : x > 7 ? 7 : x; }
static double ftom(double f) { return 69 + 12 * log2(f / 440.0); }
static float c01(double x) { return (float)(x < 0 ? 0 : x > 1 ? 1 : x); }
static int is(const Plan *P, int s) { return P->style == s; }
static double stepDur(const Plan *P) { return 60.0 / P->bpm / 4; }

static void refresh_queue(void) {
    const Plan *prev = &cur.P;
    for (int i = 0; i < 3; i++) { upNext[i] = plan_track(prev->nextSeed, &prev->key, styleSel, energySel, bandSel, citySel); prev = &upNext[i]; }
}

// A slot keeps every modulator it ever had when it is redefined (the instrument() contract), and the
// slots are reused across styles, so a style change wipes each one back to a plain voice first.
static void slot_reset(int s) {
    unsigned d = lc_dirty[s];
    for (int w = 0; w < 3; w++) {
        if (d & (1u << w))       (instrument_lfo)(s, w, LFO_PITCH, 1, 0);
        if (d & (1u << (3 + w))) (instrument_env)(s, w, ENV_PITCH, 0, 0, 0);
    }
    if (d & (1u << 6))  (instrument_drive)(s, 0);
    if (d & (1u << 7))  (instrument_echo)(s, 0);
    if (d & (1u << 8))  (instrument_reverb)(s, 0);
    if (d & (1u << 9))  (instrument_glide)(s, 0);
    if (d & (1u << 10)) (instrument_tune)(s, 0);
    if (d & (1u << 11)) (instrument_unison)(s, 1, 0);
    if (d & (1u << 12)) (instrument_pan)(s, 0);
    if (d & (1u << 13)) (instrument_level)(s, 1);
    if (d & (1u << 14)) (instrument_filter)(s, FILTER_OFF, 20000, 0);
    if (d & (1u << 15)) for (int m = 0; m < 7; m++) (instrument_mode)(s, m, 0);
    if (d & (1u << 16)) (instrument_tape)(s, 0, 0, 0);
    if (d & (1u << 17)) (instrument_chorus)(s, 1, 0, 0);
    if (d & (1u << 18)) (instrument_eq)(s, 0, 0, 0);
    lc_dirty[s] = 0;
}
static void inst(int s, int wave, int a, int d, int sus, int r, float h, float t, float m) {
    instrument(s, wave, a, d, sus, r); instrument_harmonics(s, h); instrument_timbre(s, t); instrument_morph(s, m);
}

// the kit's three morphdrum voices, from the style's own kit roll (every style replaces drums.kit)
static void voice_kit(const Plan *P) {
    double F0 = kv_has(P, "drums.kit.kickF0") ? kv(P, "drums.kit.kickF0") : 110, F1 = kv_has(P, "drums.kit.kickF1") ? kv(P, "drums.kit.kickF1") : 48;
    double kd = kv_has(P, "drums.kit.kickDecay") ? kv(P, "drums.kit.kickDecay") : 0.18;
    double sd = kv_has(P, "drums.kit.snareDecay") ? kv(P, "drums.kit.snareDecay") : kv_has(P, "drums.kit.brush") ? kv(P, "drums.kit.brush") : 0.1;
    double hp = kv_has(P, "drums.kit.hatHP") ? kv(P, "drums.kit.hatHP") : 6000, hc = kv_has(P, "drums.kit.hatClosed") ? kv(P, "drums.kit.hatClosed") : 0.02;
    if (is(P, S_AMBIENT)) { F0 = 95; F1 = 48; kd = 0.18; sd = 0.11; hp = 7000; hc = 0.025; }
    int brush = is(P, S_PIANO) || is(P, S_BOSSA) || is(P, S_SAD) || is(P, S_AMBIENT);
    float *k = kit.p[MD_KICK];
    k[MD_CHAR] = is(P, S_HOUSE) ? 0.45f : 0.2f; k[MD_LEVEL] = 1;
    k[MD_TUNE]  = c01((ftom(F1) - 19) / 33.0);
    k[MD_PUNCH] = c01(12 * log2(F0 / F1) / 48.0);
    k[MD_SNAP]  = c01(((is(P, S_HOUSE) ? kv(P, "drums.kit.kickSweep") * LC_KSWEEP : 90) - 8) / 142.0);
    k[MD_DECAY] = c01((kd * 4000 - 40) / 1060.0);
    k[MD_CUT] = brush ? 0.30f : 0.36f; k[MD_CLICK] = brush ? 0.10f : 0.22f; k[MD_SUB] = 0.22f;
    k[MD_DRIVE] = is(P, S_HOUSE) ? 0.28f : 0.18f;
    float *s = kit.p[MD_SNARE];
    s[MD_CHAR] = 0.5f; s[MD_LEVEL] = 1; s[MD_TUNE] = 0.32f; s[MD_DECAY] = brush ? 0.25f : 0.45f; s[MD_PUNCH] = 0.25f;
    s[MD_SNAP] = 0.8f; s[MD_TONE] = brush ? 0.85f : 0.62f; s[MD_CUT] = brush ? 0.30f : 0.45f; s[MD_DRIVE] = 0.1f;
    s[MD_ODEC] = c01((sd * (brush ? 3000 : 4000) - 30) / 390.0);
    float *h = kit.p[MD_HAT];
    h[MD_CHAR] = 0.2f; h[MD_LEVEL] = 1; h[MD_TUNE] = 0.53f; h[MD_TONE] = 0.25f; h[MD_SUB] = 0.6f; h[MD_RES] = 0.0f;
    h[MD_CUT]   = c01(log2(hp / 3000.0) / 2.0);
    h[MD_DECAY] = c01((hc * 2500 - 10) / 210.0);                       // their hats are 12-35 ms ticks: keep ours short
    // the OPEN hat: house plays one on EVERY offbeat with no closed hat to choke it, and x4000 rang each for
    // >0.5 s into the next — boom-chuff boom-chuff, a steam train. Their open hat is a short exponential tick.
    h[MD_ODEC]  = c01(((is(P, S_HOUSE) ? kv(P, "drums.kit.openDecay") * 1200 : 0.12 * 4000) - 80) / 720.0);
    morph_ride(&kit);
    // (kit levels: set by the style MIX block at the end of voice_track — after the ride, which resets the hats)
    // the percussion slots every style shares a shape of
    inst(I_RIM, INSTR_MODAL, 0, 0, 7, 30, 0.55f, 0.70f, 0.04f);
    instrument_level(I_RIM, 0.28f); instrument_filter(I_RIM, FILTER_HIGH, 300, 0); instrument_reverb(I_RIM, 0.30f); instrument_pan(I_RIM, -0.15f);
    inst(I_SHAKER, INSTR_NOISE, 4, 40, 0, 20, 0.5f, 0.5f, 0.5f);
    instrument_filter(I_SHAKER, FILTER_HIGH, is(P, S_SYNTH) ? (int)kv(P, "drums.perc.shaker.hp") : 5200, 1);
    instrument_level(I_SHAKER, 0.35f); instrument_pan(I_SHAKER, 0.3f);
    inst(I_CLAP, INSTR_NOISE, 0, 26, 0, 20, 0.5f, 0.5f, 0.5f);
    instrument_filter(I_CLAP, FILTER_BAND, is(P, S_HOUSE) ? (int)kv(P, "drums.kit.clapF") : is(P, S_SYNTH) ? (int)kv(P, "drums.perc.clap.f") : 1300, 2);
    instrument_level(I_CLAP, 0.45f); instrument_reverb(I_CLAP, 0.35f); instrument_pan(I_CLAP, -0.1f);
    inst(I_TOM, INSTR_MEMBRANE, 0, 0, 7, 60, 0.15f, 0.25f, 0.2f);
    instrument_level(I_TOM, 0.5f); instrument_pan(I_TOM, -0.2f); instrument_reverb(I_TOM, 0.15f);
    if (is(P, S_HOUSE)) {        // the hats: white noise above hatHP, closed / open decay (their createHouseKit)
        int hp = (int)kv(P, "drums.kit.hatHP");
        inst(I_NHAT, INSTR_NOISE, 0, (int)(kv(P, "drums.kit.hatClosed") * 2300), 0, 20, 0.5f, 0.5f, 0.5f);
        inst(I_NHATO, INSTR_NOISE, 0, (int)(kv(P, "drums.kit.openDecay") * 2300), 0, 60, 0.5f, 0.5f, 0.5f);
        for (int q = I_NHAT; q <= I_NHATO; q++) { instrument_filter(q, FILTER_HIGH, hp, 1); instrument_pan(q, 0.2f); instrument_reverb(q, 0.08f); }
        instrument_level(I_NHAT, 0.40f); instrument_level(I_NHATO, 0.34f);
    }
    if (is(P, S_HOUSE)) {        // the ride: the six-square bank on cymbal ratios
        inst(I_BLOCK, INSTR_METAL, 0, 0, 7, 60, 0.85f, 0.4f, 0.2f);
        instrument_mode(I_BLOCK, MODE_METAL_DECAY, (float)fmin(1, log2(kv(P, "drums.kit.rideDecay") / 0.02) / log2(100)));
        instrument_filter(I_BLOCK, FILTER_HIGH, 4500, 0); instrument_level(I_BLOCK, 0.30f); instrument_pan(I_BLOCK, 0.25f);
    } else if (is(P, S_MEDIEVAL)) {   // the tambourine: jingles
        inst(I_BLOCK, INSTR_METAL, 0, 0, 7, 40, 0.95f, 0.65f, 0.8f);
        instrument_mode(I_BLOCK, MODE_METAL_DECAY, 0.25f);
        instrument_filter(I_BLOCK, FILTER_HIGH, 6500, 0); instrument_level(I_BLOCK, 0.28f); instrument_pan(I_BLOCK, 0.3f);
    } else {                           // a woodblock
        inst(I_BLOCK, INSTR_MODAL, 0, 0, 7, 30, 0.62f, 0.75f, 0.04f);
        instrument_level(I_BLOCK, 0.28f); instrument_pan(I_BLOCK, 0.15f);
    }
    if (is(P, S_BOSSA)) { // the clave (rimF) + the surdo
        inst(I_RIM, INSTR_MODAL, 0, 0, 7, 25, 0.62f, 0.55f, 0.04f); instrument_level(I_RIM, 0.30f); instrument_pan(I_RIM, -0.18f);
        inst(I_TOM, INSTR_MEMBRANE, 0, 0, 7, 120, 0.1f, 0.15f, 0.35f); instrument_level(I_TOM, 0.55f);
    }
    if (is(P, S_MEDIEVAL)) { // the frame drum: doum (centre) on the tom slot, the slap on the clap slot
        inst(I_TOM, INSTR_MEMBRANE, 0, 0, 7, 120, 0.35f, 0.10f, 0.0f); instrument_level(I_TOM, 0.7f); instrument_pan(I_TOM, 0);
        inst(I_CLAP, INSTR_MEMBRANE, 0, 0, 7, 40, 0.55f, 0.90f, 0.0f); instrument_level(I_CLAP, 0.45f);
        instrument_filter(I_CLAP, FILTER_OFF, 20000, 0); instrument_reverb(I_CLAP, 0.25f);
    }
}

// per-style MIX: overall loudness goes on the master eq (the one stage that boosts); the balance inside a
// style only CUTS. Measured per part, 40 s renders, seed 11 (2026-09-28), matched to jazzhop's proportions
// (keys -26 · bass -32 · kit -29 dB RMS): e.g. sad's sub sat 10 dB OVER its piano, medieval's frame drums 13 dB under.
static const struct { float masterDb, keys, bass, kit, leadDb, lead2Db; } STYLE_MIX[NSTYLE] = {
    [S_JAZZHOP] = {  0.0f, 1.00f, 1.00f, 1.00f, 0.0f, 0.0f },
    [S_PIANO]   = {  8.0f, 1.00f, 1.00f, 0.56f, 0.0f, 0.0f },
    [S_AMBIENT] = { -8.0f, 1.00f, 0.60f, 1.00f, 8.0f, 8.0f },
    [S_BOSSA]   = {  2.5f, 1.00f, 1.00f, 0.70f, 10.0f, -7.0f },
    [S_SYNTH]   = { -3.0f, 0.65f, 1.00f, 1.00f, 6.0f, 9.0f },
    [S_HOUSE]   = { -4.0f, 1.00f, 0.32f, 0.63f, 6.0f, 12.0f },
    [S_GUITAR]  = {  6.0f, 1.00f, 1.00f, 0.56f, 5.0f, 5.0f },
    [S_SAD]     = {  3.0f, 1.00f, 0.20f, 1.00f, 7.0f, 7.0f },
    [S_MEDIEVAL]= {  4.5f, 1.00f, 0.50f, 1.00f, -11.0f, -11.0f },
};
static float styleDb = 0, pumpAmt = 0; static int pumpRel = 150;
// glue() and sidechain() on one bus are ONE gain stage (the engine's sc[bus]): whichever is set last wins, so
// the bus glue silently disabled house's pump. A pumping style gets the pump INSTEAD of the glue.
static void apply_bus(void) {   // the BUS stage + the style's overall gain (both on the master eq)
    if (pumpAmt > 0) sidechain(0, 0, pumpAmt, 4, pumpRel);
    else glue(0, fxOn[FXT_BUS] ? LC_GLUE : 0, 8, 160);
    if (fxOn[FXT_BUS]) eq(1.5f + styleDb, 2.5f + styleDb, 0.0f + styleDb); else eq(styleDb, styleDb, styleDb);
}
// per-track voicing — the planner's rolled parameters land on our engines, one casting per style
static void voice_track(const Plan *P) {
    static int lastStyle = -1;
    if (P->style != lastStyle) {       // a new style: every style slot back to a plain voice first
        for (int i = 0; i < NSTYLE_SLOTS; i++) slot_reset(STYLE_SLOTS[i]);
        for (int i = 0; i < 2; i++) if (droneH[i] >= 0) { note_off(droneH[i]); droneH[i] = -1; }
        lastStyle = P->style;
    }
    for (int i = 0; i < 128; i++) tieEnd[i] = -1;
    double sdur = stepDur(P);
    float tr = fxOn[FXT_TREM] ? LC_TREM : 0.0f;
    float wet = 0.35f; int eMs = (int)fmin(1900, 3 * sdur * 1000); float eFb = 0.3f;
    float spring = 0, chorusMix = 0, chorusRate = 1, chorusDepth = 0.3f, sc = 0; int scRel = 150;
    keysBase = 0.71;
    const char *lt = kvs(P, "lead.timbre"), *lv = kvs(P, "lead.voice");
    switch (P->style) {
    default:
    case S_JAZZHOP:
        for (int s = I_KEYS; s <= I_KEYSL; s++) {
            inst(s, INSTR_EPIANO, 2, 0, 7, s == I_KEYS ? 320 : 2600, 0.10f, 0.36f, 0.18f);
            instrument_filter(s, FILTER_LOW, (int)P->ep.lp, 0);
            instrument_lfo(s, 0, LFO_VOLUME, (float)P->ep.tremRate, (float)P->ep.tremDepth * tr);
            instrument_lfo(s, 1, LFO_PAN, (float)P->ep.tremRate, 0.25f * tr);
            instrument_reverb(s, 0.30f);
        }
        goto upright;
    case S_BOSSA:
        for (int s = I_KEYS; s <= I_KEYS2L; s++) if (s == I_KEYS || s == I_KEYSL || s == I_KEYS2 || s == I_KEYS2L) {
            int thumb = s == I_KEYS2 || s == I_KEYS2L, lng = s == I_KEYSL || s == I_KEYS2L;
            inst(s, INSTR_GUITAR, 1, 0, 7, lng ? 1400 : 160, 0.45f, thumb ? 0.12f : 0.26f, 0.22f);
            instrument_filter(s, FILTER_LOW, (int)P->ep.lp, 0); instrument_reverb(s, 0.28f);
            instrument_pan(s, thumb ? -0.12f : 0.08f);
        }
        keysBase = 0.85;
        upright:
        for (int s = I_BASS; s <= I_BASSS; s++) {
            inst(s, INSTR_BOWED, 3, 0, 7, 90, 0.62f, 0.30f, 0.45f);
            instrument_mode(s, MODE_BOW_PIZZ, 1.0f); instrument_mode(s, MODE_BOW_BODY, 0.85f); instrument_mode(s, MODE_BOW_SIZE, BOW_SIZE_BASS);
            instrument_filter(s, FILTER_LOW, is(P, S_BOSSA) ? (int)(kv(P, "bass.lp") * 1.8) : 950, 0);
        }
        instrument_env(I_BASSS, 0, ENV_PITCH, 0, 60, -1.0f);
        if (is(P, S_BOSSA)) {
            inst(I_LEAD, INSTR_GUITAR, 1, 0, 7, 700, 0.45f, 0.40f, 0.22f); instrument_filter(I_LEAD, FILTER_LOW, (int)(P->ep.lp * 1.15), 0);
            inst(I_LEAD2, INSTR_PIPE, 30, 0, 5, 220, 0.0f, 0.40f, 0.62f); instrument_lfo(I_LEAD2, 0, LFO_PITCH, 4.8f, 0.11f); instrument_glide(I_LEAD2, 20);
        } else {
            inst(I_LEAD, INSTR_MALLET, 1, 0, 7, 1200, 0.22f, 0.45f, 0.85f); instrument_filter(I_LEAD, FILTER_LOW, 3200, 0);
            inst(I_LEAD2, INSTR_PIPE, 14, 0, 5, 220, 0.0f, 0.34f, 0.68f); instrument_lfo(I_LEAD2, 0, LFO_PITCH, 5.0f, 0.10f); instrument_glide(I_LEAD2, 13);
        }
        break;
    case S_PIANO: case S_SAD: {
        double hammer = kv(P, "piano.hammer"), bright = kv(P, "piano.bright");
        for (int s = I_KEYS; s <= I_KEYSL; s++) {
            inst(s, INSTR_PIANO, 2, 0, 7, s == I_KEYS ? 450 : 2800, 0.04f, c01(0.22 * hammer), 0.55f);
            instrument_filter(s, FILTER_LOW, (int)(P->ep.lp * bright), 0);
            instrument_tune(s, (float)(kv(P, "piano.detune") * 0.01));
            instrument_reverb(s, 0.32f);
        }
        keysBase = 0.9;
        if (is(P, S_SAD)) {   // the pad under the piano, and the warble on both
            inst(I_PAD, INSTR_SAW, (int)(kv(P, "sad.padAtk") * 1000), 0, 7, (int)(kv(P, "sad.padRel") * 1000), 0.5f, 0.5f, 0.5f);
            instrument_unison(I_PAD, 2, (float)(kv(P, "sad.det") / 100));
            instrument_filter(I_PAD, FILTER_LOW, (int)kv(P, "sad.padLp"), 0); instrument_level(I_PAD, (float)fmin(1, 0.28 * kv(P, "sad.pad")));
            instrument_reverb(I_PAD, 0.5f);
            float w = (float)fmin(0.6, kv(P, "sad.warble") / 25.0);
            for (int s = I_KEYS; s <= I_KEYSL; s++) instrument_tape(s, w, 0, 0);
            instrument_tape(I_PAD, w, 0, 0);
            inst(I_BASS, INSTR_SINE, 45, 1400, 5, 250, 0.5f, 0.5f, 0.5f); instrument_filter(I_BASS, FILTER_LOW, 520, 0); instrument_glide(I_BASS, 35);
            // the muted horn: brass through the cup-mute band, a wah on the attack, a slow vibrato
            inst(I_LEAD, INSTR_BRASS, 12, 0, 5, 260, 0.15f, 0.45f, 0.40f);
            instrument_filter(I_LEAD, FILTER_BAND, 1250, 2); instrument_env(I_LEAD, 0, ENV_CUTOFF, 12, 90, 600);
            instrument_lfo(I_LEAD, 0, LFO_PITCH, 4.6f, (float)(kv(P, "sad.vib") / 100)); instrument_glide(I_LEAD, 40);
            instrument_reverb(I_LEAD, 0.35f); instrument_echo(I_LEAD, 0.26f);
            eMs = (int)fmin(1900, 4 * sdur * 1000);
        }
        break;
    }
    case S_AMBIENT: {
        double atk = kv(P, "ep.pad.atk"), rel = kv(P, "ep.pad.rel");
        for (int s = I_KEYS; s <= I_KEYSL; s++) {
            inst(s, INSTR_SAW, (int)(atk * 1000), 0, 7, (int)((s == I_KEYS ? rel : fmax(rel, 3)) * 1000), 0.5f, 0.5f, 0.5f);
            instrument_unison(s, 2, (float)(kv(P, "ep.pad.det") / 100));
            instrument_filter(s, FILTER_LOW, (int)P->ep.lp, 0);
            instrument_lfo(s, 0, LFO_CUTOFF, (float)kv(P, "ep.pad.lfo"), (float)(P->ep.lp * kv(P, "ep.pad.depth")));
            instrument_reverb(s, 0.45f); instrument_echo(s, (float)kv(P, "ep.pad.wash"));
        }
        inst(I_KEYS2, INSTR_SINE, (int)(atk * 1000), 0, 7, (int)(rel * 1000), 0.5f, 0.5f, 0.5f);   // the octave shimmer
        instrument_level(I_KEYS2, (float)fmin(1, kv(P, "ep.pad.shim") * 1.2)); instrument_lfo(I_KEYS2, 0, LFO_PAN, 0.11f, (float)kv(P, "ep.pad.width"));
        instrument_reverb(I_KEYS2, 0.55f);
        inst(I_DRONE, INSTR_SINE, 1500, 0, 7, 2500, 0.5f, 0.5f, 0.5f); instrument_filter(I_DRONE, FILTER_LOW, 700, 0);
        instrument_lfo(I_DRONE, 0, LFO_VOLUME, 0.07f, 0.3f); instrument_level(I_DRONE, 0.5f); instrument_reverb(I_DRONE, 0.4f);
        inst(I_BASS, INSTR_SINE, 120, 0, 7, 450, 0.5f, 0.5f, 0.5f); instrument_filter(I_BASS, FILTER_LOW, 420, 0); instrument_glide(I_BASS, 30);
        instrument_level(I_BASS, 0.9f);
        inst(I_BASSS, INSTR_SINE, 120, 0, 7, 450, 0.5f, 0.5f, 0.5f); instrument_filter(I_BASSS, FILTER_LOW, 420, 0); instrument_glide(I_BASSS, 30);
        // the bell: FM on the 3.5 detent (vibes) or a rounder low ratio (soft)
        inst(I_LEAD, INSTR_FM, 1, 1300, 0, 900, 0.55f, 0.40f, 0.10f); instrument_filter(I_LEAD, FILTER_LOW, 2800, 0);
        inst(I_LEAD2, INSTR_FM, 1, 1300, 0, 900, 0.22f, 0.28f, 0.05f); instrument_filter(I_LEAD2, FILTER_LOW, 2800, 0);
        for (int s = I_LEAD; s <= I_LEAD2; s++) { instrument_reverb(s, 0.5f); instrument_echo(s, 0.4f); }
        eMs = (int)fmin(1900, 6 * sdur * 1000); eFb = 0.42f; keysBase = 0.8;
        break;
    }
    case S_SYNTH: {
        double cut = kv(P, "synth.cutoff"), env = kv(P, "synth.env");
        inst(I_KEYS, INSTR_SAW, 2, 180, 3, 90, 0.5f, 0.5f, 0.5f);                            // the stab
        inst(I_PAD, INSTR_SAW, (int)(kv(P, "synth.atk") * 1000), 0, 7, (int)(kv(P, "synth.rel") * 1000 + 200), 0.5f, 0.5f, 0.5f);
        inst(I_KEYSL, INSTR_SAW, 40, 0, 7, 1800, 0.5f, 0.5f, 0.5f);
        for (int s = I_KEYS; s <= I_PAD; s++) if (s == I_KEYS || s == I_KEYSL || s == I_PAD) {
            instrument_unison(s, 2, (float)(kv(P, "synth.detune") / 100));
            instrument_filter(s, FILTER_LOW, (int)(cut * 1.2), (int)fmin(4, kv(P, "synth.res")));
            instrument_env(s, 0, ENV_CUTOFF, 0, s == I_KEYS ? 160 : 600, (float)fmin(6500 - cut, cut * env * (s == I_KEYS ? 1.0 : 0.4)));
            instrument_reverb(s, 0.25f);
        }
        instrument_level(I_PAD, 0.8f);
        chorusMix = 0.35f; chorusRate = (float)kv(P, "synth.chorus.rate"); chorusDepth = (float)fmin(1, kv(P, "synth.chorus.depth") * 300);
        // the mono bass: a saw into a plucked lowpass, glide on a slide
        for (int s = I_BASS; s <= I_BASSS; s++) {
            inst(s, INSTR_SAW, 2, (int)(kv(P, "synth.bass.dec") * 3000), 4, 40, 0.5f, 0.5f, 0.5f);
            instrument_unison(s, 2, (float)(0.02 + 0.1 * kv(P, "synth.bass.sq")));
            instrument_filter(s, FILTER_LADDER, 400, (int)fmin(4, kv(P, "synth.bass.q")));
            instrument_env(s, 0, ENV_CUTOFF, 0, (int)(kv(P, "synth.bass.dec") * 1000), (float)fmin(2200, 700 * kv(P, "synth.bass.env")));
            instrument_level(s, 0.75f);
        }
        instrument_glide(I_BASSS, 30);
        inst(I_LEAD, INSTR_SQUARE, 3, 0, 6, 80, 0.5f, 0.5f, 0.5f);                             // "square"
        instrument_filter(I_LEAD, FILTER_LOW, 2000, 1); instrument_glide(I_LEAD, (int)(kv(P, "synth.lead.glide") * 1000));
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.3f, (float)(kv(P, "synth.lead.vib") / 100)); instrument_level(I_LEAD, 0.55f);
        inst(I_LEAD2, INSTR_FM, 1, 1000, 0, 300, 0.55f, 0.55f, 0.08f); instrument_filter(I_LEAD2, FILTER_LOW, 4500, 0);   // "glass"
        for (int s = I_LEAD; s <= I_LEAD2; s++) { instrument_reverb(s, 0.35f); instrument_echo(s, 0.35f); }
        eFb = 0.32f; keysBase = 0.62;
        break;
    }
    case S_HOUSE: {
        double rel = kv(P, "house.chords.rel");
        inst(I_KEYS, INSTR_ORGAN, 1, 140, 3, 50, 0.30f, 0.62f, 0.15f);                        // the stab
        inst(I_PAD, INSTR_ORGAN, (int)(kv(P, "house.chords.atk") * 1000), 1600, 5, (int)(rel * 1000), 0.30f, 0.50f, 0.35f);
        inst(I_KEYSL, INSTR_ORGAN, 60, 1600, 5, 1600, 0.30f, 0.50f, 0.35f);
        for (int s = I_KEYS; s <= I_PAD; s++) if (s == I_KEYS || s == I_KEYSL || s == I_PAD) {
            instrument_filter(s, FILTER_LOW, (int)P->ep.lp, (int)fmin(3, kv(P, "house.chords.q")));
            instrument_reverb(s, 0.3f); instrument_echo(s, (float)kv(P, "house.chords.echo"));
        }
        instrument_env(I_KEYS, 0, ENV_CUTOFF, 0, (int)(kv(P, "house.chords.pluckTau") * 3000), (float)(P->ep.lp * kv(P, "house.chords.pluck") * 0.6));
        for (int s = I_BASS; s <= I_BASSS; s++) {
            inst(s, INSTR_SINE, 4, 300, 6, 28, 0.5f, 0.5f, 0.5f);
            instrument_unison(s, 2, (float)(0.05 * kv(P, "house.bass.tri")));
            instrument_drive(s, c01((kv(P, "house.bass.drive") - 1) * 0.35));
            instrument_filter(s, FILTER_LOW, (int)kv(P, "house.bass.lp"), 0);
            instrument_env(s, 0, ENV_CUTOFF, 0, 240, (float)kv(P, "house.bass.lp"));
        }
        instrument_glide(I_BASSS, 25);
        inst(I_LEAD, INSTR_VOICE, 30, 0, 6, 180, 0.62f, 0.55f, 0.45f);                        // "vox"
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.2f, (float)(kv(P, "house.lead.vib") / 100)); instrument_glide(I_LEAD, (int)(kv(P, "house.lead.glide") * 1000));
        inst(I_LEAD2, INSTR_PLUCK, 1, 0, 7, 320, 0.45f, 0.55f, 0.3f);                           // "pluck"
        for (int s = I_LEAD; s <= I_LEAD2; s++) { instrument_reverb(s, 0.38f); instrument_echo(s, 0.3f); }
        sc = c01(1 - pow(10, -kv(P, "house.pump.chordDb") / 20)); scRel = (int)(kv(P, "house.pump.rec") * 1000 + 60);
        keysBase = 0.6;
        break;
    }
    case S_GUITAR: {
        for (int s = I_KEYS; s <= I_KEYS2L; s++) if (s == I_KEYS || s == I_KEYSL || s == I_KEYS2 || s == I_KEYS2L) {
            int thumb = s == I_KEYS2, mute = s == I_KEYS2L;
            inst(s, INSTR_GUITAR, 1, 0, 7, s == I_KEYSL ? 1600 : mute ? 30 : 260, 0.12f, thumb ? 0.35f : 0.62f, mute ? 0.9f : 0.12f);
            instrument_drive(s, c01((kv(P, "gtr.drive") - 1) * 0.3));
            instrument_filter(s, FILTER_LOW, (int)P->ep.lp, 1);
            instrument_pan(s, (float)(kv(P, "gtr.pan") + (thumb ? -0.17 : 0.1)));
            instrument_reverb(s, 0.2f);
        }
        chorusMix = 0.3f; chorusRate = (float)kv(P, "gtr.chorus.rate"); chorusDepth = (float)fmin(1, kv(P, "gtr.chorus.cents") / 20);
        spring = (float)fmin(1, kv(P, "gtr.spring") * 5);
        for (int s = I_BASS; s <= I_BASSS; s++) {      // the finger bass: a low plucked string, a growl band
            inst(s, INSTR_PLUCK, 1, 0, 7, 120, 0.72f, 0.35f, 0.25f);
            instrument_filter(s, FILTER_LOW, (int)(kv(P, "bass.lp") * 1.4), 1);
            instrument_env(s, 0, ENV_CUTOFF, 0, 110, (float)(kv(P, "bass.lp") * 1.8));
        }
        instrument_env(I_BASSS, 1, ENV_PITCH, 0, 30, -1.0f);
        int bright = !strcmp(lt, "bright");
        inst(I_LEAD, INSTR_GUITAR, 1, 0, 7, 120, 0.12f, bright ? 0.7f : 0.45f, 0.1f);
        instrument_drive(I_LEAD, c01((kv(P, "gtr.drive") - 1) * 0.3)); instrument_filter(I_LEAD, FILTER_LOW, bright ? 4200 : 2800, 0);
        instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.2f, 0.11f); instrument_glide(I_LEAD, 25);
        instrument_reverb(I_LEAD, 0.3f); instrument_echo(I_LEAD, 0.28f);
        eFb = 0.25f; keysBase = 0.75;
        break;
    }
    case S_MEDIEVAL: {
        for (int s = I_KEYS; s <= I_KEYS2L; s++) if (s == I_KEYS || s == I_KEYSL || s == I_KEYS2 || s == I_KEYS2L) {
            int course = s == I_KEYS2 || s == I_KEYS2L;
            inst(s, INSTR_GUITAR, 1, 0, 7, (s == I_KEYSL || s == I_KEYS2L) ? 1500 : 200, 0.55f, 0.30f, 0.20f);
            instrument_filter(s, FILTER_LOW, (int)P->ep.lp, 0); instrument_reverb(s, 0.30f);
            if (course) { instrument_tune(s, 0.07f); instrument_level(s, 0.65f); }
        }
        // the viol: BOWED arco, cello-sized, a slow bow and a small vibrato
        inst(I_BASS, INSTR_BOWED, 45, 0, 6, 110, 0.55f, 0.35f, 0.40f);
        instrument_mode(I_BASS, MODE_BOW_BODY, 0.85f); instrument_mode(I_BASS, MODE_BOW_SIZE, BOW_SIZE_CELLO);
        instrument_filter(I_BASS, FILTER_LOW, (int)kv(P, "viol.lp"), 0); instrument_lfo(I_BASS, 0, LFO_PITCH, 5.2f, 0.05f);
        instrument_level(I_BASS, 0.8f);
        inst(I_BASSS, INSTR_BOWED, 45, 0, 6, 110, 0.55f, 0.35f, 0.40f);
        instrument_mode(I_BASSS, MODE_BOW_BODY, 0.85f); instrument_mode(I_BASSS, MODE_BOW_SIZE, BOW_SIZE_CELLO);
        instrument_filter(I_BASSS, FILTER_LOW, (int)kv(P, "viol.lp"), 0);
        inst(I_DRONE, INSTR_SAW, 1200, 0, 7, 2500, 0.5f, 0.5f, 0.5f); instrument_filter(I_DRONE, FILTER_LOW, 480, 0);
        instrument_lfo(I_DRONE, 0, LFO_VOLUME, 0.09f, 0.25f); instrument_level(I_DRONE, 0.45f);
        // recorder (brighter, faster chiff) / wooden flute (darker, breathier)
        inst(I_LEAD, INSTR_PIPE, 18, 0, 5, 160, 0.05f, 0.30f, 0.80f); instrument_lfo(I_LEAD, 0, LFO_PITCH, 5.0f, 0.09f);
        inst(I_LEAD2, INSTR_PIPE, 34, 0, 5, 200, 0.0f, 0.50f, 0.55f); instrument_lfo(I_LEAD2, 0, LFO_PITCH, 4.5f, 0.12f);
        for (int s = I_LEAD; s <= I_LEAD2; s++) { instrument_glide(s, 20); instrument_reverb(s, 0.35f); instrument_echo(s, 0.2f); }
        eFb = 0.25f; keysBase = 0.8;
        break;
    }
    }
    (void)lv;
    voice_kit(P);
    for (int s = MDS_KICK; s <= MDS_KICKS; s++) sidechain_key(KIT_BASE + s, 0, sc > 0 ? 1.0f : 0.0f);
    pumpAmt = sc * 0.6f * LC_PUMP; pumpRel = scRel;
    reverb_spring(spring);
    chorus(chorusRate, chorusDepth, chorusMix);
    echo(eMs, eFb, 0.3f);
    instrument_pan(I_LEAD, (float)P->leadPan); instrument_pan(I_LEAD2, (float)P->leadPan);
    (void)wet;
    // tape: the plan's wow depth (cents) → our wow; saturation + flutter held
    float wow = (float)(P->wowCents / 25.0); if (wow > 0.6f) wow = 0.6f;
    curWow = wow * LC_WOW;
    if (fxOn[FXT_TAPE]) tape(curWow, LC_FLUTTER, LC_SAT);
    dustAmt = P->dust;
    keysLevel = keysBase * band_layers(P->sec[0].L, P->band, style_of(P)->keysLevel).level;
    keysBase *= STYLE_MIX[P->style].keys;
    keysLevel = keysBase * band_layers(P->sec[0].L, P->band, style_of(P)->keysLevel).level;
    for (int s = I_KEYS; s <= I_KEYSL; s++) instrument_level(s, (float)fmin(1, keysLevel));
    float bm = STYLE_MIX[P->style].bass, km = STYLE_MIX[P->style].kit;
    for (int s = I_BASS; s <= I_BASSS; s++) instrument_level(s, (is(P, S_SYNTH) ? 1.0f : is(P, S_AMBIENT) ? 0.9f : is(P, S_MEDIEVAL) ? 0.8f : 1.0f) * bm);
    for (int s = MDS_KICK; s <= MDS_KICKS; s++) instrument_level(KIT_BASE + s, 0.55f * km);
    instrument_level(KIT_BASE + MDS_SNB, 0.8f * km);
    instrument_level(KIT_BASE + MDS_SNN, (is(P, S_PIANO) || is(P, S_BOSSA) || is(P, S_SAD) || is(P, S_AMBIENT) ? 0.40f : 0.45f) * km);
    for (int q = MDS_HC; q <= MDS_HO; q++) instrument_level(kit.base + q, 0.55f * km);
    if (is(P, S_MEDIEVAL)) { instrument_level(I_TOM, 1.0f); instrument_level(I_CLAP, 1.0f); instrument_level(I_BLOCK, 0.7f); }
    // the lead, per timbre slot: a trim that BOOSTS goes on the slot's own eq (a private bus), a cut on its level
    //   (lead vs keys, 90 s renders on seeds whose lead enters early; jazzhop's -4 dB is the reference)
    float ld[2] = { STYLE_MIX[P->style].leadDb, STYLE_MIX[P->style].lead2Db };
    for (int i = 0; i < 2; i++) {
        int sl = i ? I_LEAD2 : I_LEAD;
        if (ld[i] > 0) instrument_eq(sl, ld[i], ld[i], ld[i]);
        else if (ld[i] < 0) instrument_level(sl, powf(10, ld[i] / 20));
    }
    styleDb = STYLE_MIX[P->style].masterDb; apply_bus();   // every track: the pump / glue + the style gain
    lastLeadAt = -10;
}

static void setup_band(void) {
    morph_build(&kit, KIT_BASE);
    instrument_reverb(KIT_BASE + MDS_SNB, 0.25f); instrument_reverb(KIT_BASE + MDS_SNN, 0.25f);
    instrument_reverb(KIT_BASE + MDS_HC, 0.08f); instrument_reverb(KIT_BASE + MDS_HO, 0.08f);
    instrument_pan(KIT_BASE + MDS_HC, 0.2f); instrument_pan(KIT_BASE + MDS_HO, 0.2f);
    // the vinyl: a pink-ish hiss bed + dust ticks
    instrument(I_HISS, INSTR_NOISE, 400, 0, 7, 400); instrument_filter(I_HISS, FILTER_HIGH, 2000, 0); instrument_level(I_HISS, 0.30f);
    instrument(I_CRK, INSTR_NOISE, 0, 5, 0, 3); instrument_filter(I_CRK, FILTER_BAND, 2600, 2); instrument_level(I_CRK, 0.60f);
    for (int s = MDS_KICK; s <= MDS_KICKS; s++) instrument_level(KIT_BASE + s, 0.55f);   // the kick sat 6 dB over the band
    reverb(0.52f, 0.55f);
    instrument_level(KIT_BASE + MDS_SNB, 0.8f);
    // the BUS stage (glue 0.25 + eq +1.5/+2.5 dB, the makeup their tape stage adds into a -3 dB limiter) is applied by apply_fx_toggle
}

static void apply_fx_toggle(int i) {
    switch (i) {
    case FXT_TONE:  if (fxOn[i]) lastTone = -1; else filter(FILTER_OFF, 0, 0); break;
    case FXT_TAPE:  if (fxOn[i]) tape(curWow, LC_FLUTTER, LC_SAT); else tape(0, 0, 0); break;
    case FXT_BUS:   apply_bus(); break;
    case FXT_TREM:  if (is(&cur.P, S_JAZZHOP)) for (int s = I_KEYS; s <= I_KEYSL; s++) {
                        float tr = fxOn[i] ? LC_TREM : 0.0f;
                        instrument_lfo(s, 0, LFO_VOLUME, (float)cur.P.ep.tremRate, (float)cur.P.ep.tremDepth * tr);
                        instrument_lfo(s, 1, LFO_PAN, (float)cur.P.ep.tremRate, 0.25f * tr);
                    } break;
    default: break;   // vinyl is gated per frame in update()
    }
}
static void toggle_fx(int i) { fxOn[i] = !fxOn[i]; apply_fx_toggle(i); }

// the drum voices, fired at a planned velocity (morph_fire only takes a coarse boost)
static void fire_kick(double d, double vel) {
    MDRes r; md__resolve(&kit, MD_KICK, &r); int b = kit.base, v = v2vol(vel, 7.6);
    schedule_at(d, r.midi, b + MDS_KICK, v, r.dec);
    if (r.l1_vol) schedule_at(d, 60, b + MDS_KICKC, (r.l1_vol * v + 6) / 7, r.l1_dec);
    if (r.l2_vol) schedule_at(d, r.midi - 12, b + MDS_KICKS, (r.l2_vol * v + 6) / 7, r.l2_dec);
}
static void fire_snare(double d, double vel) {
    MDRes r; md__resolve(&kit, MD_SNARE, &r); int b = kit.base, v = v2vol(vel, 7.6);
    int body = (int)lround((1 - r.tone) * v * 1.5), snpy = v;   // both layers ride the velocity (morph_fire pins the noise)
    if (body > 7) body = 7;
    if (body) { schedule_at(d, r.midi, b + MDS_SNB, body, r.dec); schedule_at(d, r.midi + 10, b + MDS_SNB, body, r.dec); }
    schedule_at(d, 60, b + MDS_SNN, snpy ? snpy : 1, r.l1_dec);
}
static void fire_hat(double d, double vel, int open) {
    MDRes r; md__resolve(&kit, MD_HAT, &r); int b = kit.base, v = v2vol(vel, 7.6);
    schedule_at(d, r.midi, b + (open ? MDS_HO : MDS_HC), v, (open ? r.l2_dec : r.dec) * 6);
}

// one plucked/struck chord: strummed low→high over `spread` (or high→low for an up-strum),
// the top +6-10%, each note capped at its own remaining length
static void play_chord(int slot, double t, const int *m, int n, double vel, double dur, double spread, int up, double topBoost) {
    for (int k = 0; k < n; k++) {
        int idx = up ? n - 1 - k : k;
        double dt = n > 1 ? k * spread / (n - 1) : 0;
        double v = idx == n - 1 ? fmin(1, vel * topBoost) : vel;
        schedule_at(t + dt, m[idx], slot, v2vol(v, 10.5), (int)(fmax(0.05, dur - dt) * 1000));
    }
}
// the guitar family's HELD figure (bossa / guitar / lute): an intro/break/outro chord re-plucked every
// half bar, alternating a roll with a pinch (thumb + the top) — what their held() renders itself
static void held_figure(int slot, int thumbSlot, double t, const int *m, int n, double vel, double dur, double sd, int finalChord) {
    if (finalChord) { play_chord(slot, t, m, n, vel, dur, 0.14, 0, 1.06); return; }
    int reps = (int)fmin(4, floor(dur / (8 * sd) + 1e-6)); if (reps < 1) reps = 1;
    for (int r = 0; r < reps; r++) {
        double tr = t + r * 8 * sd, rem = dur - r * 8 * sd;
        if (r % 2 == 0) play_chord(slot, tr, m, n, vel * (r ? 0.9 : 1), rem, fmin(0.1, fmax(0.05, 0.5 * sd)) * (n - 1), 0, 1.06);
        else {
            schedule_at(tr, m[0], thumbSlot, v2vol(vel * 0.8, 10.5), (int)(rem * 1000));
            for (int k = n - 2; k < n; k++) if (k > 0) schedule_at(tr + 0.012 + 0.004 * k, m[k], slot, v2vol(vel * 0.66, 10.5), (int)(rem * 1000));
        }
    }
}

static int sec_at_bar(const Track *T, int bar) { return bar >= 0 && bar < T->st.nbars ? T->st.map[bar].sec : -1; }

static void dispatch(Track *T, const Ev *e) {
    const Plan *P = &T->P;
    double d = T->start + e->t;   // an ABSOLUTE audio_time(): schedule_at lands it on its sample (see the header)
    double sd = stepDur(P), durSteps = e->dur / sd;
    switch (e->k) {
    case K_EP: {
        int lng = e->rel > 0.5, held = e->rel >= 0.095 && e->nn >= 3 && durSteps >= 7.5;
        int slot = lng ? I_KEYSL : I_KEYS;
        switch (P->style) {
        case S_PIANO: case S_SAD: {
            int roll = e->nn >= 3 && rnd_float() < kv(P, "piano.roll");
            play_chord(slot, d, e->notes, e->nn, e->vel, e->dur, roll ? 0.06 + 0.05 * rnd_float() : e->strum, 0, 1.1);
            if (is(P, S_SAD) && e->nn >= 3 && e->dur >= 1.5) {       // the pad under it: up to 3 chord tones in 48..67
                int pm[3], np = 0;
                for (int k = 0; k < e->nn && np < 3; k++) if (e->notes[k] >= 48 && e->notes[k] <= 67) pm[np++] = e->notes[k];
                for (int k = 0; k < np; k++) schedule_at(d + 0.02, pm[k], I_PAD, v2vol(e->vel, 9), (int)(e->dur * 1000));
            }
            break;
        }
        case S_AMBIENT:
            for (int k = 0; k < e->nn; k++) {
                int m = e->notes[k]; double dt = e->nn > 1 ? k * e->strum / (e->nn - 1) : 0;
                if (tieEnd[m] > d + dt && tieEnd[m] < d + dt + e->strum + 0.1) { tieEnd[m] = d + e->dur; continue; }   // a tie: let it ring on
                double v = k == e->nn - 1 ? fmin(1, e->vel * 1.1) : e->vel;
                schedule_at(d + dt, m, slot, v2vol(v, 10), (int)((e->dur - dt) * 1000));
                if (k == e->nn - 1) schedule_at(d + dt, m + 12, I_KEYS2, v2vol(v * 0.6, 9), (int)((e->dur - dt) * 1000));
                tieEnd[m] = d + e->dur;
            }
            break;
        case S_SYNTH: case S_HOUSE:
            if (!lng) slot = e->dur < 0.25 ? I_KEYS : I_PAD;          // their stab-vs-pad blend, split into two voices
            play_chord(slot, d, e->notes, e->nn, e->vel, e->dur, e->strum, 0, 1.08);
            break;
        case S_BOSSA: case S_GUITAR: case S_MEDIEVAL: {
            int thumbSlot = lng ? I_KEYS2L : I_KEYS2;
            if (is(P, S_MEDIEVAL)) {                                  // the lute: every note a COURSE of two strings
                if (held && !lng) { held_figure(slot, slot, d, e->notes, e->nn, e->vel, e->dur, sd, 0); held_figure(I_KEYS2, I_KEYS2, d + 0.001, e->notes, e->nn, e->vel * 0.6, e->dur, sd, 0); }
                else {
                    play_chord(slot, d, e->notes, e->nn, e->vel, e->dur, lng ? 0.13 : e->strum, 0, 1.06);
                    int oc[8]; for (int k = 0; k < e->nn; k++) oc[k] = e->notes[k] < 55 ? e->notes[k] + 12 : e->notes[k];
                    play_chord(lng ? I_KEYS2L : I_KEYS2, d + 0.0008, oc, e->nn, e->vel * 0.6, e->dur, lng ? 0.13 : e->strum, 0, 1.06);
                }
                break;
            }
            if (held) { held_figure(slot, thumbSlot, d, e->notes, e->nn, e->vel, e->dur, sd, lng); break; }
            if (is(P, S_GUITAR) && e->vel < 0.28 && durSteps <= 1.3) {       // a muted ghost strum
                for (int k = 0; k < e->nn; k++) schedule_at(d + k * 0.006, e->notes[k], I_KEYS2L, v2vol(e->vel * 1.4, 10.5), 30);
                break;
            }
            if (e->nn <= 2) {                                          // a finger, or a pinch
                for (int k = 0; k < e->nn; k++) schedule_at(d + k * 0.008, e->notes[k], e->notes[k] < 52 ? thumbSlot : slot, v2vol(e->vel * (k ? 0.92 : 1), 10.5), (int)(e->dur * 1000));
                break;
            }
            int up = e->nn >= 2 && e->notes[0] > e->notes[e->nn - 1];   // listed top-first = an up-strum
            if (up) { int tmp[8]; for (int k = 0; k < e->nn; k++) tmp[k] = e->notes[e->nn - 1 - k]; play_chord(slot, d, tmp, e->nn, e->vel * 0.85, e->dur, e->strum * 0.6, 1, 1.0); }
            else {
                schedule_at(d, e->notes[0], thumbSlot, v2vol(e->vel * 1.05, 10.5), (int)(e->dur * 1000));
                play_chord(slot, d + e->strum / (e->nn - 1), e->notes + 1, e->nn - 1, e->vel, e->dur, e->strum * (e->nn - 2) / (e->nn - 1), 0, 1.06);
            }
            break;
        }
        default:
            play_chord(slot, d, e->notes, e->nn, e->vel, e->dur, e->strum, 0, 1.1);
        }
        flash[K_EP] = 1; break;
    }
    case K_BASS: {
        if (is(P, S_PIANO)) { schedule_at(d, e->midi, I_KEYS, v2vol(e->vel, 10.5), (int)(e->dur * 1000)); flash[K_BASS] = 1; break; }   // the left hand
        int s = e->slide ? I_BASSS : I_BASS;
        schedule_at(d, e->midi, s, v2vol(e->vel, 8.6), (int)(e->dur * 1000)); flash[K_BASS] = 1; break;
    }
    case K_KICK:
        if (is(P, S_MEDIEVAL)) { schedule_at(d, (int)lround(ftom(kv(P, "drums.kit.kickF0"))), I_TOM, v2vol(e->vel, 10), (int)(kv(P, "drums.kit.kickDecay") * 3000)); }
        else fire_kick(d, e->vel);
        flash[K_KICK] = 1; break;
    case K_SNARE:
        if (is(P, S_MEDIEVAL)) schedule_at(d, (int)lround(ftom(kv(P, "drums.kit.slapBP") / 3)), I_CLAP, v2vol(e->vel, 10), 80);
        else fire_snare(d, e->vel);
        flash[K_SNARE] = 1; break;
    case K_HAT:
        if (is(P, S_HOUSE)) schedule_at(d, 60, e->open ? I_NHATO : I_NHAT, v2vol(e->vel, 7.6), e->open ? (int)(kv(P, "drums.kit.openDecay") * 4000) : 60);
        else if (is(P, S_MEDIEVAL)) { for (int b = 0; b < (e->open ? 5 : 3); b++) schedule_at(d + b * (e->open ? 0.022 : 0.012), 84, I_BLOCK, v2vol(e->vel * (b ? 0.7 : 1), 7), e->open ? 160 : 60); }
        else fire_hat(d, e->vel, e->open);
        flash[K_HAT] = 1; break;
    case K_RIM: {
        int m = is(P, S_BOSSA) && kv_has(P, "drums.kit.rimF") ? (int)lround(ftom(kv(P, "drums.kit.rimF"))) :
                is(P, S_GUITAR) ? (int)lround(ftom(kv(P, "drums.kit.rimF"))) + 12 : 77;
        schedule_at(d, m, I_RIM, v2vol(e->vel, 7.6), 60); flash[K_RIM] = 1; break;
    }
    case K_CLAP:
        for (int b = 0; b < 3; b++) schedule_at(d + b * 0.01, 60, I_CLAP, v2vol(e->vel * (b ? 0.8 : 1), 7.6), 8);
        schedule_at(d + 0.03, 60, I_CLAP, v2vol(e->vel * 0.7, 7.6), is(P, S_HOUSE) ? (int)(kv(P, "drums.kit.clapTail") * 1000) : 60);
        flash[K_SNARE] = 1; break;
    case K_SHAKER: schedule_at(d, 60, I_SHAKER, v2vol(e->vel, 7.6), 40); flash[K_HAT] = 1; break;
    case K_TOM: {
        int step = (int)lround((e->t - e->bar * 16 * sd) / sd), m = 43;
        if (is(P, S_SYNTH)) { int k = step - 10; if (k < 0) k = 0; m = (int)lround(ftom(240 * pow(2, -k / 5.0))); }   // a descending tom run
        else if (is(P, S_BOSSA)) m = 43;
        else if (is(P, S_MEDIEVAL)) m = 42;
        schedule_at(d, m, I_TOM, v2vol(e->vel, 8), 200); flash[K_KICK] = 1; break;
    }
    case K_BLOCK: schedule_at(d, is(P, S_HOUSE) ? 64 : 80, I_BLOCK, v2vol(e->vel, 7.6), is(P, S_HOUSE) ? 400 : 40); flash[K_HAT] = 1; break;
    case K_LEAD: {
        if (!P->hasLead) break;
        const char *lt = kvs(P, "lead.timbre"), *lv = kvs(P, "lead.voice");
        int s = I_LEAD;
        switch (P->style) {
        case S_JAZZHOP: s = !strcmp(lt, "soft") ? I_LEAD2 : I_LEAD; break;
        case S_BOSSA:   s = !strcmp(lt, "flute") ? I_LEAD2 : I_LEAD; break;
        case S_AMBIENT: s = !strcmp(lt, "soft") ? I_LEAD2 : I_LEAD;
            if (d - lastLeadAt < fmax(1.2, 5.5 * sd)) { flash[K_LEAD] = 1; return; }     // the bell drops notes that crowd the last
            lastLeadAt = d; break;
        case S_SYNTH:   s = !strcmp(lv, "glass") ? I_LEAD2 : I_LEAD; break;
        case S_HOUSE:   s = !strcmp(lv, "pluck") ? I_LEAD2 : I_LEAD; break;
        case S_MEDIEVAL: s = !strcmp(lt, "wood") ? I_LEAD2 : I_LEAD; break;
        case S_PIANO:   s = I_KEYS; break;                                            // the right hand
        default: break;
        }
        schedule_at(d, e->midi, s, v2vol(e->vel, s == I_KEYS ? 10.5 : 8.4), (int)(e->dur * 1000));
        flash[K_LEAD] = 1; break;
    }
    case K_FX:
        switch (e->fx) {
        case FX_TONE_:  toneTgt = e->v; toneTau = e->tau; break;
        case FX_VINYL_: vinylTgt = e->v; vinylTau = e->tau; break;
        case FX_DUST_:  dustAmt = e->v; break;
        case FX_WOW_:   break;   // per-track only: tape() rebuilds its DSP, so the 2-bar wow drift is not ridden
        case FX_LEVEL_: keysLevel = keysBase * e->v; for (int s = I_KEYS; s <= I_KEYSL; s++) instrument_level(s, (float)fmin(1, keysLevel)); break;
        }
        break;
    default: break;
    }
}
// ═══ HUMANIZE — OUR layer on top of their exact arranger (toggle 6 / "human") ═══
// Their planner is strict: a lead that jumps straight to its target, a bass that lands on the root with at
// most one approach note, keys that strike the chord as spelled. This adds what a player would: grace
// notes flicking into the longer lead notes, a passing tone across a leap of a third, a bass WALK into the
// next chord (their single approach note grown into two or three), and the odd keys suspension resolving a
// 16th late. It runs on each bar's events AFTER the planner, on its own stream (seed, 101), so the planner —
// and the oracle that verifies it bit-for-bit — never sees it. How much: a LOOSENESS per style x energy.
static const float LOOSE[NSTYLE] = { [S_JAZZHOP] = 0.8f, [S_PIANO] = 0.6f, [S_AMBIENT] = 0.3f, [S_BOSSA] = 0.7f, [S_SYNTH] = 0.2f,
                                     [S_HOUSE] = 0.15f, [S_GUITAR] = 0.6f, [S_SAD] = 0.5f, [S_MEDIEVAL] = 0.8f };
static const float LOOSE_EN[NENERGY] = { 1.2f, 1.0f, 0.7f };
static int in_scale(const Plan *P, int m) {
    static const int MAJ[7] = { 0, 2, 4, 5, 7, 9, 11 }, MIN[7] = { 0, 2, 3, 5, 7, 8, 10 };
    int pc = mod12(m - P->key.tonic);
    for (int i = 0; i < 7; i++) if ((P->key.major ? MAJ : MIN)[i] == pc) return 1;
    return 0;
}
static int scale_step(const Plan *P, int m, int dir) {   // the next in-key note above (dir +1) or below (-1)
    for (int k = 1; k <= 2; k++) if (in_scale(P, m + dir * k)) return m + dir * k;
    return m + dir;
}
static int ev_step(const Plan *P, const Ev *e) { return (int)lround((e->t - e->bar * 16 * stepDur(P)) / stepDur(P)); }
static void add_ev(Evs *E, const Ev *x) { if (E->n < (int)(sizeof E->e / sizeof *E->e)) E->e[E->n++] = *x; }
static int humN[4];   // what the layer added this session: grace · passing · walk · suspension (the trace shows it)
static void humanize_bar(Track *T, int bar, Evs *E) {
    const Plan *P = &T->P; Rng *r = &T->hr;
    double loose = LOOSE[P->style] * LOOSE_EN[P->energy], sd = stepDur(P);
    int n0 = E->n;
    // ── the lead: grace notes into long notes, a passing tone across a third ──
    for (int k = 0; k < n0; k++) {
        Ev *e = &E->e[k]; if (e->k != K_LEAD) continue;
        int nextK = -1; for (int q = k + 1; q < n0; q++) if (E->e[q].k == K_LEAD) { nextK = q; break; }
        if (nextK >= 0) {                                           // a leap of a third, with room: fill it
            Ev *nx = &E->e[nextK]; int iv = nx->midi - e->midi;
            if (abs(iv) >= 3 && abs(iv) <= 4 && e->dur >= 2 * sd && rn(r) < 0.7 * loose) {
                int pass = scale_step(P, e->midi, iv > 0 ? 1 : -1);
                if (pass != nx->midi && pass != e->midi) {
                    double half = e->dur / 2; Ev x = *e;
                    e->dur = half; x.t = e->t + half; x.dur = fmin(half, nx->t - x.t); x.vel = e->vel * 0.8; x.midi = pass; x.glide = 1;
                    if (x.dur > 0.04) { add_ev(E, &x); humN[1]++; }
                    continue;
                }
            }
        }
        if (e->dur >= 0.3 && !e->glide && rn(r) < 0.6 * loose) {   // a cut or a tap: a quick neighbour into the note
            int above = rn(r) < 0.62;
            double g = fmin(0.07, fmax(0.04, 0.2 * e->dur));
            Ev x = *e; x.t = e->t - g; x.dur = g * 0.9; x.vel = e->vel * 0.7; x.midi = scale_step(P, e->midi, above ? 1 : -1);
            int clash = 0;                                          // never across the previous note
            for (int q = 0; q < n0; q++) if (q != k && E->e[q].k == K_LEAD && E->e[q].t < e->t && E->e[q].t + E->e[q].dur > x.t) clash = 1;
            if (!clash && x.t > 0) { add_ev(E, &x); humN[0]++; }
        }
    }
    // ── the bass: a walk into the next chord ──
    if (!is(P, S_HOUSE) && !is(P, S_SYNTH) && !is(P, S_AMBIENT) && !is(P, S_MEDIEVAL) && bar + 1 < T->st.nbars) {
        Chord nc[14]; int nn = bar_chords(P, &T->st, bar + 1, nc);
        int last = -1, prev = -1;
        for (int k = 0; k < n0; k++) if (E->e[k].k == K_BASS) { prev = last; last = k; }
        if (nn && last >= 0 && rn(r) < 0.8 * loose) {
            Ev *L = &E->e[last]; int ls = ev_step(P, L);
            int target = L->midi + ((mod12(P->key.tonic + nc[0].root) - mod12(L->midi) + 18) % 12 - 6);   // the nearest next root
            if (ls >= 13 && prev >= 0 && ev_step(P, &E->e[prev]) <= 10) {   // their approach note on 14: add the step before it
                Ev *Pv = &E->e[prev]; int mid = scale_step(P, Pv->midi, L->midi > Pv->midi ? 1 : -1);
                if (mid != L->midi && mid != Pv->midi) {
                    Ev x = *Pv; x.t = Pv->t + (12 - ev_step(P, Pv)) * sd; x.midi = mid; x.vel = Pv->vel * 0.8; x.dur = 2 * sd * P->bassFactor; x.slide = 0;
                    Pv->dur = fmin(Pv->dur, x.t - Pv->t); add_ev(E, &x); humN[2]++;
                }
            } else if (ls <= 8 && L->t + L->dur > bar * 16 * sd + 12 * sd && abs(target - L->midi) >= 3) {   // a held root: walk 12 → 14
                int dir = target > L->midi ? 1 : -1, a = scale_step(P, L->midi, dir), b2 = scale_step(P, a, dir);
                if ((dir > 0 ? b2 < target : b2 > target)) {
                    Ev x = *L; double t12 = bar * 16 * sd + 12 * sd;
                    L->dur = t12 - L->t;
                    x.t = t12; x.midi = a; x.vel = L->vel * 0.78; x.dur = 2 * sd * 0.9; x.slide = 0; add_ev(E, &x);
                    x.t = t12 + 2 * sd; x.midi = b2; x.vel = L->vel * 0.72; add_ev(E, &x); humN[2]++;
                }
            }
        }
    }
    // ── the keys: the odd suspension, the top note a step high then resolving a 16th late ──
    for (int k = 0; k < n0; k++) {
        Ev *e = &E->e[k]; if (e->k != K_EP || e->nn < 3 || e->rel > 0.09 || e->dur < 4 * sd) continue;
        if (rn(r) >= 0.2 * loose) continue;
        int top = e->notes[e->nn - 1], sus = scale_step(P, top, 1);
        Ev x = *e; x.nn = 1; x.notes[0] = top; x.t = e->t + sd * (1 + (rn(r) < 0.5)); x.dur = e->dur - (x.t - e->t); x.vel = e->vel * 0.85; x.strum = 0;
        e->notes[e->nn - 1] = sus;
        if (x.dur > 0.1) { add_ev(E, &x); humN[3]++; }
    }
    // keep the bar time-sorted (stable)
    for (int a = 1; a < E->n; a++) { Ev x = E->e[a]; int j = a - 1; while (j >= 0 && E->e[j].t > x.t) { E->e[j + 1] = E->e[j]; j--; } E->e[j + 1] = x; }
}

// the fx events land on the frame their time arrives (they are automation, not notes)
static void insert_ev(Track *T, const Ev *e) {
    if (T->nq >= (int)(sizeof T->q / sizeof *T->q)) return;
    int j = T->nq++;
    while (j > 0 && T->q[j - 1].t > e->t) { T->q[j] = T->q[j - 1]; j--; }
    T->q[j] = *e;
}
static void build_track(const Plan *P, double t0) {
    memset(&cur, 0, sizeof cur);
    cur.P = *P; bar_state(&cur.P, &cur.st);
    cur.hr = lc_stream(cur.P.seed, 101);
    cur.start = t0; cur.live = true;
    voice_track(&cur.P);
    refresh_queue();
    songCount++;
}
static void start_seed(uint32_t seed, const Key *prevKey, double t0) {
    Plan P = plan_track(seed, prevKey, styleSel, energySel, bandSel, citySel);
    build_track(&P, t0);
}
static void skip_next(void) {
    // their skip: fade out, dip the tone to 400 Hz, vinyl swell, the next intro at +0.6 s
    Plan nx = upNext[0];
    note_off_all(); hissH = -1; droneH[0] = droneH[1] = -1;
    toneTgt = 400; toneTau = 0.13; vinylTgt = VINYL_BOOST; vinylTau = 0.3;
    build_track(&nx, clk + 0.6);
}
#define LOOK 0.1
static void advance(void) {
    if (!cur.live) return;
    double until = clk + LOOK, barDur = 16 * 60.0 / cur.P.bpm / 4;
    if (cur.nq && cur.start + cur.q[0].t < clk - 0.05) {           // a hitch: re-anchor, drop what's late
        int bar = cur.q[0].bar, k = 0;
        for (int i = 0; i < cur.nq; i++) if (cur.q[i].bar > bar) cur.q[k++] = cur.q[i];
        cur.nq = k; cur.start = clk + 0.1 - (bar + 1) * barDur;
    }
    while (cur.nextBar < cur.P.bars && cur.start + cur.nextBar * barDur < until + LOOK) {
        static Evs ev; int pb = cur.nextBar; plan_bar(&cur.P, cur.nextBar++, &cur.st, &ev);
        if (fxOn[FXT_HUMAN]) humanize_bar(&cur, pb, &ev);
        for (int i = 0; i < ev.n; i++) insert_ev(&cur, &ev.e[i]);
        if (cur.st.lastFill != F_NONE) { fillShow = cur.st.lastFill; fillT = 1.6f; }
        if (cur.st.lastPush) { pushShow = 1; pushT = 1.2f; }
    }
    int k = 0;
    while (k < cur.nq && cur.start + cur.q[k].t < until) dispatch(&cur, &cur.q[k++]);
    if (k) { memmove(cur.q, cur.q + k, sizeof cur.q[0] * (cur.nq - k)); cur.nq -= k; }
    if (cur.nextBar >= cur.P.bars && cur.nq == 0) {
        double end = cur.start + cur.P.bars * barDur, nextStart = end + 0.6 * barDur;
        if (nextStart < until + LOOK) { Plan nx = upNext[0]; build_track(&nx, nextStart); }
    }
}
// per-section automation their VOICES do (not planned events): the drones' level, house's filter sweep
static void ride_sections(void) {
    static int lastBar = -2, lastStyleR = -1;
    const Plan *P = &cur.P;
    double barDur = 16 * stepDur(P); int bar = (int)floor((clk - cur.start) / barDur);
    if (bar == lastBar && P->style == lastStyleR) return;
    lastBar = bar; lastStyleR = P->style;
    int sec = sec_at_bar(&cur, bar < 0 ? 0 : bar);
    const BarInfo *b = bar >= 0 && bar < cur.st.nbars ? &cur.st.map[bar] : NULL;
    int wantDrone = is(P, S_AMBIENT) ? (int)kv(P, "ep.pad.drone") : is(P, S_MEDIEVAL) ? kv_has(P, "medieval.drone.0") : 0;   // [0,7] / [0] / null
    if (wantDrone && droneH[0] < 0 && bar >= 0 && bar < P->bars) {
        int root = is(P, S_AMBIENT) ? 48 + mod12(P->key.tonic - 48) : 36 + mod12(P->key.tonic);
        droneH[0] = note_on(root, I_DRONE, 0);
        if (is(P, S_AMBIENT) || kv_has(P, "medieval.drone.1")) droneH[1] = note_on(root + 7, I_DRONE, 0);
    }
    if (droneH[0] >= 0) {
        double lvl = 1;
        if (sec == S_INTRO && b) lvl = (b->j + 1.0) / b->n * (is(P, S_MEDIEVAL) ? 0.7 : 1);
        else if (sec == S_BREAK) lvl = is(P, S_MEDIEVAL) ? 1.33 : 1.25;
        else if (sec == S_OUTRO && b) lvl = fmax(0, 1 - (b->j - (is(P, S_MEDIEVAL) ? 1 : 0)) / (double)b->n);
        if (bar >= P->bars) lvl = 0;
        note_vol(droneH[0], (float)(3.5 * lvl));
        if (droneH[1] >= 0) note_vol(droneH[1], (float)(3.5 * lvl * (is(P, S_AMBIENT) ? 0.3 : 0.6)));
    }
    if (is(P, S_HOUSE) && b) {                                    // the chord bus's section sweep
        double f = 1;
        switch (sec) {
        case S_INTRO: f = 0.5 + 0.35 * (b->j + 1.0) / b->n; break;
        case S_B:     f = kv(P, "house.chords.open"); break;
        case S_BREAK: f = b->j < b->n / 2 ? 0.45 : 0.45 + (kv(P, "house.chords.build") - 0.45) * (b->j - b->n / 2 + 1.0) / (b->n - b->n / 2); break;
        case S_OUTRO: f = 0.3; break;
        default: f = 1;
        }
        int q = (int)fmin(3, kv(P, "house.chords.q"));
        for (int s = I_KEYS; s <= I_PAD; s++) if (s == I_KEYS || s == I_KEYSL || s == I_PAD) instrument_filter(s, FILTER_LOW, (int)(P->ep.lp * f), q);
    }
}
static void set_band(int band) {
    bandSel = band; cur.P.band = band;
    for (int i = cur.nextBar; i < cur.st.nbars; i++) cur.st.map[i].L = band_layers(cur.P.sec[cur.st.map[i].si].L, band, style_of(&cur.P)->keysLevel);
    refresh_queue();
}

// ── display helpers ──
static int now_bar(void) {
    double barDur = 16 * 60.0 / cur.P.bpm / 4; int b = (int)floor((clk - cur.start) / barDur);
    return b < 0 ? -1 : b >= cur.P.bars ? cur.P.bars - 1 : b;
}
static void chord_name(const Plan *P, const Chord *c, char *o, int n) {
    snprintf(o, n, "%s%s", NOTE_NAMES[mod12(P->key.tonic + c->root)], QUAL[c->q].name);
}
static const char *layer_ep(int m) { static const char *N[] = { "comp", "intro", "whole", "outro" }; return N[m]; }
static const char *layer_bass(int m) { static const char *N[] = { "on kick", "last bar", "--", "whole", "outro" }; return N[m]; }
static const char *layer_drums(const Plan *P, int m) {
    switch (m) { case DR_PATTERN: return style_of(P)->grids[P->pattern].name; case DR_HATS2: return "hats in";
    case DR_NONE: return "--"; case DR_P5: return "rim break"; default: return "outro"; }
}

void update(void) {
    static bool booted = false;
    if (!booted) {
        setup_band();
        uint32_t seed = LOFI_SEED ? LOFI_SEED : (uint32_t)(rnd(1 << 30)) * 4u + (uint32_t)rnd(4);
        clk = 0;
        if (LOFI_STYLE >= 0) styleSel = LOFI_STYLE;
        start_seed(seed, NULL, 0.15);
        apply_fx_toggle(FXT_BUS);
        booted = true;
    }
    // the clock IS the sound clock: audio_time() counts the samples actually rendered, and every note
    // is booked on it with schedule_at, so a stalled frame or a callback boundary cannot move a note
    clk = audio_time();
    // ── input (their keys: N next · V vibe · C city) ──
    if (keyp('N') || keyp(KEY_SPACE)) skip_next();
    if (keyp('S')) { styleSel = (styleSel + 1) % NSTYLE; refresh_queue(); }       // from the next track (theirs: the Vibe style)
    if (keyp('E')) { energySel = (energySel + 1) % NENERGY; refresh_queue(); }     // from the next track
    if (keyp('B')) set_band((bandSel + 1) % NBAND);                                  // from the next bar
    if (keyp('C')) { citySel = (citySel + 1) % LC_NCITY; refresh_queue(); }
    if (keyp('H')) showHelp = !showHelp;
    for (int i = 0; i < NFXT; i++) if (keyp('1' + i)) toggle_fx(i);
    advance();
    ride_sections();
    // the master tone + vinyl, ridden like setTargetAtTime (first-order, per frame)
    double a = 1 - exp(-dt() / fmax(0.005, toneTau)); toneHz += (toneTgt - toneHz) * a;
    int th = (int)toneHz;
    if (fxOn[FXT_TONE] && abs(th - lastTone) > 2) { filter(FILTER_LOW, (float)th, 0.05f); lastTone = th; }
    vinylG += (vinylTgt - vinylG) * (1 - exp(-dt() / fmax(0.005, vinylTau)));
    if (hissH < 0) hissH = note_on(60, I_HISS, 0);
    note_vol(hissH, fxOn[FXT_VINYL] ? (float)(2.2 * vinylG) : 0.0f);
    if (fxOn[FXT_VINYL] && rnd_float() < dustAmt * vinylG * 9.0 * dt()) schedule_hit(rnd(16), 60 + rnd(24), I_CRK, 1 + rnd(3), 3);
    for (int i = 0; i < 12; i++) flash[i] *= 0.86f;
    if (fillT > 0) fillT -= dt(); if (pushT > 0) pushT -= dt();
#ifdef DE_TRACE
    int bb = now_bar();
    watch("song", "%d", songCount);
    watch("title", "%s", cur.P.title);
    watch("bar", "%d", bb);
    watch("section", "%s", bb >= 0 ? SEC_NAME[cur.st.map[bb].sec] : "-");
    watch("tone", "%d", (int)toneHz);
    watch("style", "%s", STYLES[cur.P.style]->id);
    watch("human", "grace %d pass %d walk %d sus %d", humN[0], humN[1], humN[2], humN[3]);
    watch("lead", "%s", cur.P.hasLead ? (kvs(&cur.P, "lead.voice")[0] ? kvs(&cur.P, "lead.voice") : kvs(&cur.P, "lead.timbre")) : "none");
#endif
}

// ── the scene: a night city by the water, lit windows breathing with the band ──
static void draw_city(float t) {
    static const int SKY[5] = { CLR_BLACK, CLR_DARKER_BLUE, CLR_DARK_BLUE, CLR_DARKER_PURPLE, CLR_DARK_PURPLE };
    for (int i = 0; i < 5; i++) rectfill(0, i * 18, 320, 18, SKY[i]);
    for (int i = 0; i < 40; i++) {                                   // stars
        int x = (i * 73 + 11) % 320, y = (i * 37 + 5) % 60;
        if (((int)(t * 1.3f) + i) % 7) pset(x, y, i % 3 ? CLR_INDIGO : CLR_LIGHT_PEACH);
    }
    // three skyline layers, far → near; windows light up with the keys + bass
    for (int layer = 0; layer < 3; layer++) {
        int base = 88 + layer * 4, col = layer == 0 ? CLR_DARKER_PURPLE : layer == 1 ? CLR_DARKER_BLUE : CLR_BLACK;
        unsigned hsh = 2166136261u ^ (unsigned)(layer * 977 + citySel * 131);
        for (int x = -4; x < 320;) {
            hsh = hsh * 16777619u + 7; int w = 10 + (int)(hsh % 18); hsh = hsh * 16777619u + 7;
            int h = 14 + (int)(hsh % (26 + layer * 12));
            rectfill(x, base - h, w, h, col);
            if (layer) for (int wy = base - h + 3; wy < base - 3; wy += 4) for (int wx = x + 2; wx < x + w - 2; wx += 3) {
                unsigned q = (unsigned)(wx * 73856093) ^ (unsigned)(wy * 19349663) ^ (unsigned)(layer * 83492791 + citySel);
                q ^= q >> 13; q *= 0x5bd1e995u; q ^= q >> 15;
                if (q % 4 == 0) {
                    float glow = (q % 3 == 0 ? flash[K_EP] : q % 3 == 1 ? flash[K_BASS] : flash[K_LEAD]);
                    pset(wx, wy, glow > 0.4f && q % 2 ? CLR_LIGHT_YELLOW : (q % 7 ? CLR_DARK_ORANGE : CLR_PEACH));
                }
            }
            x += w + 1;
        }
    }
    rectfill(0, 96, 320, 12, CLR_DARKER_BLUE);                       // the water, with reflections
    for (int i = 0; i < 24; i++) {
        int x = (i * 53 + (int)(t * 6)) % 320, y = 98 + (i * 5) % 9;
        line(x, y, x + 3 + i % 4, y, i % 3 ? CLR_DARK_ORANGE : CLR_DARK_BLUE);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    float t = timer();
    draw_city(t);
    const Plan *P = &cur.P;
    int bar = now_bar(); if (bar < 0) bar = 0;   // before the first downbeat, show bar 1
    // title card
    font(FONT_NORMAL);
    print(P->title, 8, 6, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print(str("%s  -  %s %s  -  %d bpm  -  swing %.0f%%", LC_CITY[P->city].name, NOTE_NAMES[P->key.tonic], P->key.major ? "major" : "minor", P->bpm, P->swing),
          8, 17, CLR_PEACH);
    print(str("A %s   B %s   %s", P->progA.id, P->progB.id, P->chordBars == 2 ? "2 bars/chord" : "1 bar/chord"), 8, 25, CLR_INDIGO);
    {   const char *sn = STYLES[P->style]->name;
        const char *lab = styleSel != P->style ? str("%s  (next: %s)", sn, STYLES[styleSel]->name) : sn;
        print(lab, 312 - text_width(lab), 25, CLR_DARK_ORANGE); }
    font(FONT_NORMAL);

    // ── the FORM strip — every section, sized by its bars, the playhead crawling through ──
    int fx = 8, fw = 304, fy = 112, fh = 14;
    rectfill(fx - 2, fy - 10, fw + 4, fh + 50, CLR_BROWNISH_BLACK);
    static const int SC[5] = { CLR_DARK_BLUE, CLR_TRUE_BLUE, CLR_MAUVE, CLR_DARK_PURPLE, CLR_DARKER_BLUE };
    int x = fx, acc = 0;
    for (int i = 0; i < P->nsec; i++) {
        int w = (int)lround((double)(acc + P->sec[i].bars) * fw / P->bars) - (int)lround((double)acc * fw / P->bars);
        bool on = bar >= 0 && cur.st.map[bar].si == i;
        rectfill(x, fy, w - 1, fh, on ? CLR_PEACH : SC[P->sec[i].name]);
        font(FONT_SMALL);
        const char *nm = P->sec[i].name == S_BREAK ? "brk" : P->sec[i].name == S_INTRO ? "in" : P->sec[i].name == S_OUTRO ? "out" : SEC_NAME[P->sec[i].name];
        if (text_width(nm) < w - 2) print(nm, x + 2, fy + 4, on ? CLR_BLACK : CLR_LIGHT_PEACH);
        font(FONT_NORMAL);
        x += w; acc += P->sec[i].bars;
    }
    if (bar >= 0) {
        double barDur = 16 * 60.0 / P->bpm / 4, pos = (clk - cur.start) / barDur;
        int px = fx + (int)(pos * fw / P->bars); if (px > fx + fw) px = fx + fw;
        line(px, fy - 3, px, fy + fh + 2, CLR_WHITE);
    }
    font(FONT_SMALL);
    print(str("bar %d/%d", bar + 1, P->bars), fx, fy - 8, CLR_INDIGO);
    print(str("form T%d", P->form + 1), fx + fw - 34, fy - 8, CLR_INDIGO);

    // ── what each part is doing right now (the section's LAYERS) ──
    if (bar >= 0) {
        const BarInfo *b = &cur.st.map[bar]; const Layers *L = &b->L;
        int ly = fy + fh + 6;
        struct { const char *lab, *val; int k; bool on; } R[4] = {
            { "keys",  layer_ep(L->ep), K_EP, true },
            { "bass",  layer_bass(L->bass), K_BASS, L->bass != BS_NONE },
            { "drums", L->kit ? layer_drums(P, L->drums) : "off", K_KICK, L->kit && L->drums != DR_NONE },
            { "lead",  !P->hasLead ? "none" : L->lead ? (!strcmp(kvs(P, "lead.timbre"), "soft") ? "flute" : "vibes") : "rest", K_LEAD, P->hasLead && L->lead },
        };
        for (int i = 0; i < 4; i++) {
            int cx = fx + i * 76;
            float fl = R[i].k == K_KICK ? fmaxf(flash[K_KICK], fmaxf(flash[K_SNARE], flash[K_HAT])) : flash[R[i].k];
            circfill(cx + 3, ly + 3, 2, R[i].on ? (fl > 0.3f ? CLR_LIGHT_YELLOW : CLR_DARK_ORANGE) : CLR_DARKER_PURPLE);
            print(R[i].lab, cx + 9, ly, CLR_INDIGO);
            print(R[i].val, cx + 9, ly + 8, R[i].on ? CLR_LIGHT_PEACH : CLR_DARK_PURPLE);
        }
        // the chord under the playhead + what's next
        Chord cs[12]; int n = bar_chords(P, &cur.st, bar, cs);
        double barDur = 16 * 60.0 / P->bpm / 4, beatIn = fmod((clk - cur.start) / barDur, 1.0) * 4;
        int ci = 0; for (int q = 0; q < n; q++) if (beatIn >= cs[q].start) ci = q;
        char nm[16]; chord_name(P, &cs[ci], nm, sizeof nm);
        font(FONT_NORMAL);
        print(nm, fx, ly + 22, CLR_WHITE);
        font(FONT_SMALL);
        print(str("%s  %d/%d", SEC_NAME[b->sec], b->j + 1, b->n), fx + text_width(nm) + 40, ly + 24, CLR_PEACH);
        if (fillT > 0) print(str("fill: %s", style_of(&cur.P)->fills[fillShow].id), fx + 150, ly + 24, CLR_YELLOW);
        if (pushT > 0) print("push!", fx + 230, ly + 24, CLR_PINK);
    }

    // ── the VIBE: energy (next track) · band (next bar) · city (titles) · next ──
    int by = 178; font(FONT_SMALL);
    {   // the VIBE row: style + energy (next track) · band (next bar) · city (titles) · next
        const int bw = 58, gap = 3; int bx = 8;
        if (ui_button(bx, by, bw, 14, str("S %s", STYLES[styleSel]->id))) { styleSel = (styleSel + 1) % NSTYLE; refresh_queue(); } bx += bw + gap;
        if (ui_button(bx, by, bw, 14, str("E %s", ENERGY_NAME[energySel]))) { energySel = (energySel + 1) % NENERGY; refresh_queue(); } bx += bw + gap;
        if (ui_button(bx, by, bw, 14, str("B %s", (const char *[]){ "full", "no drums", "chords" }[bandSel]))) set_band((bandSel + 1) % NBAND); bx += bw + gap;
        if (ui_button(bx, by, bw, 14, str("C %s", LC_CITY[citySel].id))) { citySel = (citySel + 1) % LC_NCITY; refresh_queue(); } bx += bw + gap;
        if (ui_button(bx, by, bw, 14, "N next >>")) skip_next();
    }
    {   // the fx toggles, top-right: a struck-through label = that stage is OFF
        int bx = 312 - NFXT * 32 + 2;
        for (int i = 0; i < NFXT; i++, bx += 32) {
            if (ui_button(bx, 36, 30, 11, FXT_NAME[i])) toggle_fx(i);
            if (!fxOn[i]) line(bx + 2, 41, bx + 27, 41, CLR_PINK);
        }
    }
    print(str("up next: %s  (%s %s)", upNext[0].title, NOTE_NAMES[upNext[0].key.tonic], upNext[0].key.major ? "maj" : "min"), 8, 168, CLR_INDIGO);
    font(FONT_NORMAL);
    if (showHelp) {
        rectfill(40, 40, 240, 92, CLR_BROWNISH_BLACK); rect(40, 40, 240, 92, CLR_PEACH);
        font(FONT_SMALL);
        static const char *H[] = {
            "LOFI CITY - endless generated lofi, the Lofi Cities way",
            "",
            "N / SPACE   next track (their skip: tone dips, vinyl swells)",
            "S           style (9, from the next track)",
            "E           energy chill / balanced / upbeat (next track)",
            "B           band full / no drums / chords only (next bar)",
            "C           city - the words the titles are made of",
            "1-6         off/on: tone . tape . bus . trem . vinyl . human",
            "H           this help",
            "",
            "each track = one seed: key (moves to a related key),",
            "2 progressions, a form (T1-T3), per-section layers,",
            "fills, pushes, bass approaches, a motif lead.",
        };
        for (int i = 0; i < (int)(sizeof H / sizeof *H); i++) print(H[i], 46, 46 + i * 7, i == 0 ? CLR_PEACH : CLR_LIGHT_PEACH);
        font(FONT_NORMAL);
    }
    ui_end();
}

#ifdef DE_SPEC
// Known answers taken from the site's OWN planner (their bundle run headless in node,
// 2026-09-28) — so a regression in the port shows up as a different track, not a vibe.
void spec(void) {
    Plan P = plan_track(12345, NULL, S_JAZZHOP, EN_BALANCED, BAND_FULL, 0);
    expect(!strcmp(P.title, "the quais city lights"), "seed 12345 in paris is 'the quais city lights'");
    expect_eq(P.key.tonic, 2, "seed 12345 is in D");
    expect_eq(P.key.major, 1, "... major");
    expect_eq(P.bpm, 78, "at 78 bpm");
    expect_eq(P.bars, 52, "52 bars long");
    expect_eq(P.form, 0, "form T1");
    expect(!strcmp(P.progA.id, "M1") && !strcmp(P.progB.id, "M1+ii-V"), "A = M1, B = M1 with the ii-V turnaround");
    expect(!strcmp(style_of(&P)->grids[P.pattern].id, "P6"), "groove P6 (shuf)");
    expect_eq(P.nextSeed, 2748599489u, "the chain's next seed");
    BarState st; bar_state(&P, &st); Evs ev; int n = 0, eps = 0;
    for (int i = 0; i < P.bars; i++) { plan_bar(&P, i, &st, &ev); n += ev.n; for (int k = 0; k < ev.n; k++) eps += ev.e[k].k == K_EP; }
    expect_eq(n, 1264, "1264 events across the track");
    expect_eq(eps, 80, "80 keys hits");
    // a track WITH a lead, and the chain into the next (the related-key move)
    Plan L = plan_track(999, NULL, S_JAZZHOP, EN_BALANCED, BAND_FULL, 1);
    expect(!strcmp(L.title, "last noodle bar, shibuya"), "seed 999 in tokyo is 'last noodle bar, shibuya'");
    expect(L.hasLead && !strcmp(kvs(&L, "lead.timbre"), "vibes"), "... with a vibes lead");
    BarState s2; bar_state(&L, &s2); int nl = 0, all = 0, firstBar = -1, firstMidi = -1;
    for (int i = 0; i < L.bars; i++) { plan_bar(&L, i, &s2, &ev); all += ev.n;
        for (int k = 0; k < ev.n; k++) if (ev.e[k].k == K_LEAD) { if (firstBar < 0) { firstBar = i; firstMidi = ev.e[k].midi; } nl++; } }
    expect_eq(all, 802, "802 events");
    expect_eq(nl, 37, "37 lead notes");
    expect_eq(firstBar, 10, "the lead enters at bar 10 (the second A)");
    expect_eq(firstMidi, 80, "on midi 80");
    Plan N = plan_track(L.nextSeed, &L.key, S_JAZZHOP, EN_BALANCED, BAND_FULL, 1);
    expect(!strcmp(N.title, "lost ginza at midnight"), "the next track is 'lost ginza at midnight'");
    expect(N.key.tonic == 8 && N.key.major && N.bpm == 87, "in Ab major at 87 bpm");
    // every style's arranger, pinned by the site's own answers for seed 12345 (paris, balanced, full band)
    static const struct { int style; const char *title; int bpm, bars, events; } STY[] = {
        { S_JAZZHOP, "the quais city lights", 78, 52, 1264 }, { S_PIANO, "the quais city lights", 71, 54, 919 },
        { S_AMBIENT, "the quais city lights", 62, 52, 338 },  { S_BOSSA, "the quais city lights", 79, 52, 1841 },
        { S_SYNTH, "the quais city lights", 85, 68, 1493 },   { S_HOUSE, "the quais city lights", 117, 84, 2094 },
        { S_GUITAR, "the quais city lights", 77, 54, 1225 },  { S_SAD, "the quais city lights", 65, 52, 292 },
        { S_MEDIEVAL, "the quais city lights", 75, 52, 780 },
    };
    for (int q = 0; q < (int)(sizeof STY / sizeof *STY); q++) {
        Plan S_ = plan_track(12345, NULL, STY[q].style, EN_BALANCED, BAND_FULL, 0);
        static BarState s3; bar_state(&S_, &s3); int all3 = 0;
        for (int i = 0; i < S_.bars; i++) { plan_bar(&S_, i, &s3, &ev); all3 += ev.n; }
        expect(!strcmp(S_.title, STY[q].title) && S_.bpm == STY[q].bpm && S_.bars == STY[q].bars && all3 == STY[q].events,
               str("style %s: %d bpm, %d bars, %d events (got %d / %d / %d)", STYLES[STY[q].style]->id, STY[q].bpm, STY[q].bars, STY[q].events, S_.bpm, S_.bars, all3));
    }
}
#endif
#endif
