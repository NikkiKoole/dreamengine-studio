// ── GAMELAN RADIO ─────────────────────────────────────────────────────────
// The bronze drift: endless generative Indonesian gamelan — tuned metallophones
// and kettle-gongs over hand drums, a cycle that a great gong closes, the whole
// ensemble shimmering on its own slightly-out-of-true tuning. Station #19, and
// the one that finally leaves the equal-tempered grid.
//
// Three engine firsts on this station:
//   • TUNING AS DATA — the first station whose pitch set is NOT 12-TET. The seed
//     rolls sléndro (5 tones) or pélog (7), as a CENT table — and then jitters
//     every degree, because no two village gamelans are tuned alike. Realized via
//     a BRONZE BANK: one INSTR_MALLET slot per scale degree, instrument_tune()'d
//     once per song to that degree's cent residual. Because an octave is exactly
//     12 integer semitones, a 5-note scale has only 5 distinct residuals — so the
//     whole pitched range plays through schedule_hit() on INTEGER midi (octave =
//     +12), staying sample-tight, while the slot tune bends each note off the
//     grid. The suling takes the other road: note_pitch() float, truly continuous.
//   • COLOTOMIC TIME — form as nested gong cycles, not bar→phrase→section. The
//     great gong (gong ageng) closes the gongan; kenong marks its quarters, kempul
//     the points between. The arrangement is concentric circles around the gong.
//   • KOTEKAN — melody as interlock: two voices (polos + sangsih) on COMPLEMENTARY
//     onset masks that sum to an unbroken 16th stream neither plays alone. Built
//     by dealing alternate sixteenths, so disjoint+full holds by construction
//     (trace-verified: polos & sangsih == 0, polos | sangsih == 0xFFFF).
//
// Plus OMBAK — the shimmer of paired bronze a few cents apart, beating against
// itself: the great gong is struck as a detuned PAIR (two slots, one tuned ~7¢
// off). And IRAMA as the feel knob: the gamelan inversion of "more" — turn it up
// and the core melody SLOWS while the elaboration thickens (Java end → Bali end).
//
// Per-song roll: the laras (sléndro/pélog) + pathet, the village tuning (overall
// pitch + per-degree jitter + ombak beat rate), the key, the gongan length, the
// balungan core melody, the kotekan figure, the region lean (Java/Bali), the
// bronze timbre. R replays the song; the suling phrases are PERFORMANCE (rnd).
//
//   SPACE next   R same gamelan   [ ] history   M power
//   LEFT/RIGHT irama (lancar→wiled)   UP/DOWN tempo   T tone   H help

#include "studio.h"
#include "radio.h"
#include <stdio.h>
#include <math.h>

#define GAMELAN_SEED 0   // pin a favourite ensemble here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
// THE BRONZE BANK: one MALLET slot per scale degree (pélog uses all 7, sléndro
// the first 5). Tuned per-song so integer schedule_hit() lands microtonal.
#define I_BRONZE  5            // bank base; degree d → slot I_BRONZE + d (5..11)
#define NDEG_MAX  7
#define I_GONG    12           // gong ageng — MALLET, long ring, struck as a pair…
#define I_GONG2   13           // …its ombak twin, tuned a few cents off → beating
#define I_KENL    14           // kendang lanang (high hand drum) — MEMBRANE
#define I_KENW    15           // kendang wadon (low hand drum)    — MEMBRANE
#define I_SUL     16           // suling (bamboo flute) — REED, note_pitch float

// ── the laras (tuning systems) — TUNING AS DATA ───────────────────────────
// Ideal cent offsets from the tonic. Real ensembles vary wildly; the seed jitters
// these per-village. Octaves are held at an exact 1200¢ (12 semis) so the bronze
// bank's per-degree slot trick covers every octave — real gamelans stretch the
// octave a touch; that refinement would want per-octave slots (a v2 item).
enum { L_SLENDRO, L_PELOG, NLARAS };
static const int  LARAS_N[NLARAS] = { 5, 7 };
static const char *LNAME[NLARAS]  = { "slendro", "pelog" };
static const int  IDEAL[NLARAS][NDEG_MAX] = {
    { 0, 228, 480, 707, 950,   0,   0 },   // sléndro — five near-but-not-equal steps
    { 0, 120, 270, 540, 670, 785, 945 },   // pélog   — seven unequal steps
};

// ── the gendhing (the piece) ──────────────────────────────────────────────
#define BALMAX 64              // balungan core: up to 64 beats of melody
#define KOTLEN 16              // kotekan figure: a 16-sixteenth window
typedef struct {
    int   laras, keyPc;        // tuning system + key (pitch class of the tonic)
    int   region;              // 0 = Java (slow, spacious) · 1 = Bali (fast, interlock)
    int   gonganBars;          // length of one gong cycle in bars (4/8/16)
    int   nGongan;             // how many cycles before the piece ends
    int   nearestSemi[NDEG_MAX]; // degree → nearest integer semitone (the schedule_hit base)
    float tuneCents[NDEG_MAX];   // degree → slot detune (village pitch + residual + jitter)
    int   allowed[NDEG_MAX], nAllowed;   // the pathet: which degrees this piece uses
    int   bal[BALMAX], balLen;           // the balungan core melody (degrees)
    int   kotDeg[KOTLEN];                // the kotekan contour (degrees)
    float ombakCents;          // gong twin detune → the beat rate
    char  title[24];
    char  village[16];
    float freq;
    unsigned seed;
} Gendhing;

static Gendhing    g;
static RadioSeed   rs;
static RadioClock  clk = { -1, 0, 125.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)

static int   tempo     = 120;
static int   intensity = 1;          // = irama: 0 lancar · 1 tanggung · 2 dadi · 3 wiled
static bool  radioOn   = true;
static bool  showHelp  = false;
static int   toneSel   = 1;          // gamelan likes warm, the metal can get glassy
static int   songCount = 0;
static float vu        = 0;
static float gongFlash = 0;          // window: the great gong ripples when it sounds
static int   sulH      = 0;          // the one held suling voice

static const unsigned KOT_POLOS   = 0x5555;   // even sixteenths
static const unsigned KOT_SANGSIH = 0xAAAA;   // odd sixteenths  → disjoint, union 0xFFFF

#define srnd(n) rad_srnd(&rs, (n))

// ── tuning helpers ────────────────────────────────────────────────────────
// degree (any int) → integer midi for schedule_hit + the bank slot it rides.
// regOct shifts the playing register (0 = the core octave); the per-degree slot
// tune supplies the microtonal residual, so this stays integer.
#define ROOT 60
static int bronze_midi(int degree, int regOct, int *slotOut) {
    int oct = regOct, n = LARAS_N[g.laras];
    while (degree < 0)   { degree += n; oct--; }
    while (degree >= n)  { degree -= n; oct++; }
    if (slotOut) *slotOut = I_BRONZE + degree;
    return ROOT + g.keyPc + g.nearestSemi[degree] + 12 * oct;
}
static void hit_deg(int dly, int degree, int regOct, int vol, int dur) {
    int sl, m = bronze_midi(degree, regOct, &sl);
    schedule_hit(dly, m, sl, vol, dur);
}
// the suling's continuous-pitch road: the EXACT microtonal float — the same
// pitch the bronze bank sounds (integer base + the slot's cent detune), so the
// flute floats perfectly in tune with the ensemble's own tuning.
static float deg_float(int degree, int regOct) {
    int oct = regOct, n = LARAS_N[g.laras];
    while (degree < 0)   { degree += n; oct--; }
    while (degree >= n)  { degree -= n; oct++; }
    return (float)(ROOT + g.keyPc + g.nearestSemi[degree] + 12 * oct) + g.tuneCents[degree];
}

static int rand_allowed(void) { return g.allowed[srnd(g.nAllowed)]; }

// ── the gamelan, rolled from the seed ─────────────────────────────────────
static const char *TW1[] = { "Ladrang", "Ketawang", "Gendhing", "Lancaran",
    "Udan", "Gambir", "Bubaran", "Ricik", "Puspa", "Gangsaran" };
static const char *TW2[] = { "Sari", "Mas", "Wilujeng", "Manis", "Asri",
    "Pangkur", "Sukma", "Laras", "Warna", "Mijil" };
static const char *VIL[] = { "Kyai Kanyut", "Kyai Guntur", "Mendung", "Surak",
    "Manggala", "Pradapa", "Lokananta", "Sari Oneng" };

static void apply_bronze_tuning(void) {
    for (int d = 0; d < LARAS_N[g.laras]; d++)
        instrument_tune(I_BRONZE + d, g.tuneCents[d]);
    // the gong pair sits on the tonic (degree 0), one twin nudged for ombak
    instrument_tune(I_GONG,  g.tuneCents[0]);
    instrument_tune(I_GONG2, g.tuneCents[0] + g.ombakCents);
}

static void new_song(double pos, unsigned seed) {
    if (sulH) { note_off(sulH); sulH = 0; }   // no held flute bleeds into the next gamelan
    g.seed = rad_seed_begin(&rs, seed);

    g.laras  = srnd(NLARAS);
    g.keyPc  = srnd(12);
    g.region = srnd(2);
    int n    = LARAS_N[g.laras];

    // the village tuning: an overall pitch drift (gamelans rarely sit at A=440),
    // the per-degree nearest-semitone + residual, and a small per-degree jitter
    int village = srnd(120) - 30;                 // -30..+89 cents, whole ensemble
    for (int d = 0; d < n; d++) {
        g.nearestSemi[d] = (IDEAL[g.laras][d] + 50) / 100;   // round to nearest semitone
        int resid = IDEAL[g.laras][d] - 100 * g.nearestSemi[d];
        int jit   = srnd(31) - 15;                            // ±15¢ this village's own
        g.tuneCents[d] = (village + resid + jit) / 100.0f;
    }
    g.ombakCents = (srnd(7) + 4) / 100.0f;        // 4..10¢ → a slow, living beat

    // the pathet — sléndro uses all five; pélog drops two of seven to a 5-note mode
    g.nAllowed = 0;
    if (g.laras == L_SLENDRO) {
        for (int d = 0; d < 5; d++) g.allowed[g.nAllowed++] = d;
    } else {
        int drop1 = 1 + srnd(6), drop2;            // never drop the tonic
        do { drop2 = 1 + srnd(6); } while (drop2 == drop1);
        for (int d = 0; d < 7; d++)
            if (d != drop1 && d != drop2) g.allowed[g.nAllowed++] = d;
    }

    // the gong cycle + how long the piece runs
    static const int GBARS[3] = { 4, 8, 16 };
    g.gonganBars = GBARS[srnd(3)];
    g.nGongan    = 5 + srnd(7);                    // 5..11 cycles

    // the balungan — the slow core melody, one tone per beat, mostly stepwise
    // within the mode, landing on the gong tone (tonic) at the cycle's seam
    g.balLen = g.gonganBars * 4;
    if (g.balLen > BALMAX) g.balLen = BALMAX;
    int ai = srnd(g.nAllowed);                     // walk an index into allowed[]
    for (int i = 0; i < g.balLen; i++) {
        if (i == 0 || i == g.balLen - 1) { g.bal[i] = 0; continue; }   // seleh = tonic
        ai += srnd(3) - 1;                         // step up / hold / down
        if (ai < 0) ai = 0; if (ai >= g.nAllowed) ai = g.nAllowed - 1;
        if (srnd(100) < 18) ai = srnd(g.nAllowed); // an occasional leap
        g.bal[i] = g.allowed[ai];
    }

    // the kotekan contour — a tight figure near the lower mode tones
    for (int i = 0; i < KOTLEN; i++)
        g.kotDeg[i] = g.allowed[srnd(g.nAllowed < 4 ? g.nAllowed : 4)];

    // the bronze timbre, rolled per village (all bank slots share it)
    for (int d = 0; d < n; d++) {
        instrument_harmonics(I_BRONZE + d, 0.55f + srnd(30) * 0.01f);   // toward bell/metal
        instrument_timbre(I_BRONZE + d,    0.40f + srnd(30) * 0.01f);   // strike hardness
        instrument_morph(I_BRONZE + d,     0.30f + srnd(22) * 0.01f);   // ring, NO motor
    }
    apply_bronze_tuning();

    snprintf(g.title,   sizeof g.title,   "%s %s", TW1[srnd(10)], TW2[srnd(10)]);
    snprintf(g.village, sizeof g.village, "%s", VIL[srnd(8)]);
    g.freq = 88.0f + srnd(190) * 0.1f;

    // tempo follows region — Bali drives, Java breathes
    tempo = g.region ? 132 + srnd(28) : 104 + srnd(22);
    bpm(tempo);

    songBase = (long)pos + 8;
    songCount++;
}

static void fresh_song(double pos) {
    new_song(pos, 0);
    rad_hist_log(&rs);
}

// ── form helpers ──────────────────────────────────────────────────────────
static int  gonganSteps(void) { return g.gonganBars * 16; }
static long song_steps(void)  { return (long)gonganSteps() * g.nGongan; }
static int  cur_gongan(long s) { return (int)(s / gonganSteps()); }

// irama: the feel knob. Turn it up and the balungan SLOWS (longer stride) while
// the elaboration fills in — the gamelan meaning of "more", inverted from every
// other station. intensity 0/1 → stride 4 (one tone per beat); 2/3 → stride 8.
static int bal_stride(void) { return intensity >= 2 ? 8 : 4; }

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    if (s >= song_steps()) return;
    int  dly  = rad_step_dly(&clk, abs, pos);
    int  N     = gonganSteps();
    long gs    = s % N;                       // position within the gong cycle
    int  step  = (int)(s % 16);               // position within the bar
    int  gi    = cur_gongan(s);
    bool intro = s < 32;                       // the buka — a short two-bar opening
    bool last  = gi == g.nGongan - 1;          // the final cycle thins to the gong
    int  idx16 = (int)(gs % 16);

    // ── COLOTOMIC PUNCTUATION — the nested gongs ──
    if (gs == 0) {                              // GONG AGENG — closes & opens the cycle
        schedule_hit(dly, ROOT + g.keyPc - 24, I_GONG,  6, (int)(stepMs * 28));
        schedule_hit(dly, ROOT + g.keyPc - 24, I_GONG2, 5, (int)(stepMs * 28));  // ombak twin
        gongFlash = 1.0f;
        vu += 3.0f;
    } else if (!last) {
        if (gs % (N / 4) == 0)                  // KENONG — the quarter marks
            { hit_deg(dly, 0, -1, 4, (int)(stepMs * 10)); vu += 1.2f; }
        else if (N >= 32 && gs % (N / 8) == 0)  // KEMPUL — the points between
            { hit_deg(dly, g.allowed[g.nAllowed / 2], -1, 3, (int)(stepMs * 8)); vu += 0.9f; }
    }

    // ── BALUNGAN — the slow bronze core (saron/slenthem) ──
    {
        int stride = bal_stride();
        if (gs % stride == 0) {
            int bi  = (int)((gs / 4) % g.balLen);
            int dur = (int)(stepMs * stride * (intensity >= 2 ? 3.0f : 1.6f));
            hit_deg(dly, g.bal[bi], 0, intro ? 3 : last ? 4 : 5, dur);   // buka plays soft
            vu += 1.4f;
        }
    }

    // ── BONANG PANERUSAN — octave-up doubling of the core (tanggung+) ──
    if (intensity >= 1 && !intro && !last && gs % 2 == 0) {
        int bi = (int)((gs / 4) % g.balLen);
        hit_deg(dly + 6, g.bal[bi], 1, 3, (int)(stepMs * 2));
        vu += 0.6f;
    }

    // ── KOTEKAN — the interlock (Bali, dadi+). Two voices, complementary masks ──
    if (g.region == 1 && intensity >= 2 && !intro && !last) {
        int d = g.kotDeg[idx16];
        if (KOT_POLOS   & (1u << idx16))         // polos — even sixteenths, lower
            { hit_deg(dly + 4, d, 1, 3, (int)(stepMs * 1.4f)); vu += 0.7f; }
        if (KOT_SANGSIH & (1u << idx16))         // sangsih — odd sixteenths, an octave up
            { hit_deg(dly + 4, d, 2, 3, (int)(stepMs * 1.4f)); vu += 0.7f; }
    }

    // ── KENDANG — the hand drums conduct (entering after the first bar) ──
    if (s >= 16) {
        if (step == 0)                           // wadon (low) on the one
            { schedule_hit(dly, 52, I_KENW, intensity >= 1 ? 5 : 4, 160); vu += 1.0f; }
        if (step == 6 || step == 12)             // lanang (high) answers
            schedule_hit(dly + rnd(3), 64, I_KENL, 3, 90);
        if (intensity >= 2 && (step == 3 || step == 9 || step == 14) && chance(70))
            schedule_hit(dly + rnd(3), 60 + rnd(5), I_KENL, 2, 70);   // busier at irama dadi+
    }

    // ── SULING — the bamboo flute floats free, off the grid, truly microtonal ──
    // note_on + note_pitch float: the one voice that isn't quantized to the bank.
    bool sulingOn = (intensity >= 2) || (g.region == 0 && intensity >= 1);
    if (sulingOn && !intro && gs % (N / 2) == 0 && chance(70)) {
        int d  = rand_allowed();
        float mf = deg_float(d, 1);
        if (sulH) note_off(sulH);
        sulH = note_on((int)(mf + 0.5f), I_SUL, 4);
        note_glide(sulH, 120);
        note_pitch(sulH, mf);
        vu += 1.0f;
    } else if (sulH && (last || !sulingOn) && gs % (N / 2) == 0) {
        note_off(sulH); sulH = 0;
    }
}

// ── setup ─────────────────────────────────────────────────────────────────
static void setup_instruments(void) {
    // the bronze bank — identical struck-metal voices, one per scale degree;
    // new_song() rolls the timbre macros and the per-degree tuning
    for (int d = 0; d < NDEG_MAX; d++)
        instrument(I_BRONZE + d, INSTR_MALLET, 1, 0, 7, 1100);

    // the great gong pair — full bell ratios, soft strike, the longest ring the
    // struck engine will hold (sustain 7 + ~3.5s release). The honest stand-in for
    // a true sustained-metal voice (see audio-notes §12, the gong note).
    instrument(I_GONG,  INSTR_MALLET, 1, 0, 7, 3500);
    instrument_harmonics(I_GONG,  0.95f);        // deep inharmonic bell
    instrument_timbre(I_GONG,     0.20f);        // soft, round strike
    instrument_morph(I_GONG,      0.30f);        // long ring, no motor
    instrument(I_GONG2, INSTR_MALLET, 1, 0, 7, 3500);
    instrument_harmonics(I_GONG2, 0.95f);
    instrument_timbre(I_GONG2,    0.20f);
    instrument_morph(I_GONG2,     0.30f);
    instrument_echo(I_GONG,  0.18f);             // a little pavilion air
    instrument_echo(I_GONG2, 0.18f);

    // the kendang — the struck-drumhead engine (tabla.c recipes)
    instrument(I_KENW, INSTR_MEMBRANE, 1, 0, 7, 220);   // wadon — low
    instrument_harmonics(I_KENW, 0.45f);
    instrument_timbre(I_KENW, 0.28f);
    instrument_morph(I_KENW, 0.18f);
    instrument(I_KENL, INSTR_MEMBRANE, 1, 0, 7, 120);   // lanang — high
    instrument_harmonics(I_KENL, 0.70f);
    instrument_timbre(I_KENL, 0.60f);
    instrument_morph(I_KENL, 0.12f);

    // the suling — REED is the only continuous-blow voice; tuned hollow/breathy
    instrument(I_SUL, INSTR_REED, 40, 0, 6, 220);
    instrument_harmonics(I_SUL, 0.30f);          // toward a hollow pipe
    instrument_timbre(I_SUL, 0.35f);
    instrument_morph(I_SUL, 0.45f);              // breath swell + a little vibrato
    instrument_filter(I_SUL, FILTER_LOW, 2400, 1);
}

// the tone knob — the whole ensemble brightens/mellows
static void apply_tone(void) {
    float tm = RAD_TONEMUL[toneSel];
    for (int d = 0; d < NDEG_MAX; d++)
        instrument_filter(I_BRONZE + d, FILTER_LOW, (int)(4200 * tm), 0);
    instrument_filter(I_GONG,  FILTER_LOW, (int)(2400 * tm), 0);
    instrument_filter(I_GONG2, FILTER_LOW, (int)(2400 * tm), 0);
    instrument_filter(I_SUL,   FILTER_LOW, (int)(2400 * tm), 1);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (GAMELAN_SEED) { new_song(pos, GAMELAN_SEED); rad_hist_log(&rs); }
        else fresh_song(pos);
        apply_tone();
        scheduled = (long)pos;
        booted = true;
    }

    int ev = rad_input(&tempo, 96, 168, 4, &intensity, &toneSel, 4, &radioOn, &showHelp);
    if (ev & RAD_EV_NEW)    fresh_song(pos);
    if (ev & RAD_EV_REPLAY) new_song(pos, g.seed);
    if (ev & RAD_EV_BACK)   { unsigned s = rad_hist_back(&rs); if (s) new_song(pos, s); }
    if (ev & RAD_EV_FWD)    { unsigned s = rad_hist_fwd(&rs);  if (s) new_song(pos, s); }
    if (ev & RAD_EV_POWER) {
        if (!radioOn) { note_off_all(); sfx(-1); sulH = 0; }
        else scheduled = (long)pos;
    }
    if (ev & (RAD_EV_TONE | RAD_EV_NEW | RAD_EV_REPLAY | RAD_EV_BACK | RAD_EV_FWD))
        apply_tone();
    if (ev & (RAD_EV_NEW | RAD_EV_REPLAY | RAD_EV_BACK | RAD_EV_FWD))
        { if (sulH) note_off(sulH); sulH = 0; }

    if (radioOn) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
        if (scheduled - songBase >= song_steps()) fresh_song(pos);
    }

    vu *= 0.88f;        if (vu > 16) vu = 16;
    gongFlash *= 0.94f;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    watch("song", "%d", songCount);
    watch("laras", "%s", LNAME[g.laras]);
    watch("region", "%s", g.region ? "bali" : "java");
    watch("irama", "%d", intensity);
    watch("gongan", "%d", ss >= 0 ? cur_gongan(ss) : 0);
    watch("polos", "%u", KOT_POLOS);
    watch("sangsih", "%u", KOT_SANGSIH);
    watch("kot_disjoint", "%u", KOT_POLOS & KOT_SANGSIH);     // must be 0
    watch("kot_union", "%u", KOT_POLOS | KOT_SANGSIH);        // must be 0xFFFF (65535)
#endif
}

// ── draw — the gong rack; the great gong ripples on the cycle seam ─────────
static const char *IRAMA[4] = { "lancar", "tanggung", "dadi", "wiled" };

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();
    long ss   = scheduled - songBase;
    long s    = ss >= 0 ? ss : 0;
    int  N     = gonganSteps();
    long gs    = s % N;
    int  gi    = cur_gongan(s);

    rad_body(CLR_DARK_BROWN, CLR_ORANGE);         // carved teak, gold leaf
    rad_dial(g.freq, CLR_ORANGE);

    // the window — a gong rack in a pavilion at dusk
    rectfill(34, 52, 102, 116, CLR_INDIGO);                  // deep dusk behind the rack
    rectfill(34, 150, 102, 18, CLR_DARK_BROWN);              // the rack base
    // the carved frame uprights
    rectfill(38, 60, 5, 92, CLR_BROWN);
    rectfill(122, 60, 5, 92, CLR_BROWN);
    rectfill(38, 60, 89, 5, CLR_BROWN);
    // the great gong (gong ageng) hanging at centre — ripples when struck
    int gx = 84, gy = 108;
    int flare = (int)(gongFlash * 6);
    if (radioOn && gongFlash > 0.05f) {
        circ(gx, gy, 26 + flare, CLR_YELLOW);
        circ(gx, gy, 22 + flare/2, CLR_ORANGE);
    }
    circfill(gx, gy, 22, CLR_DARK_GREY);
    circfill(gx, gy, 20, CLR_BROWN);
    circ(gx, gy, 22, CLR_LIGHT_GREY);
    circfill(gx, gy, 7, CLR_ORANGE);                         // the boss (pencu)
    line(gx, 65, gx, 86, CLR_DARK_GREY);                     // the cord
    // the bonang kettles — a little row, flash as the elaboration plays
    for (int k = 0; k < 5; k++) {
        int kx = 50 + k * 14, ky = 142;
        bool lit = radioOn && intensity >= 1 && ((int)(s / 2) % 5) == k;
        circfill(kx, ky, 5, lit ? CLR_YELLOW : CLR_DARK_GREY);
        circfill(kx, ky, 2, CLR_ORANGE);
    }
    rect(34, 52, 102, 116, CLR_DARK_GREY);

    // the cycle ring — colotomic time as a clock face, the dot rides to the gong
    int cx = 84, cy = 108, cr = 30;
    if (radioOn) {
        for (int q = 0; q < 4; q++) {                        // the kenong quarter marks
            float a = -1.5708f + q * 1.5708f;
            pset(cx + (int)(cosf(a) * cr), cy + (int)(sinf(a) * cr), CLR_BROWN);
        }
        float ph = (float)gs / (float)N;
        float a  = -1.5708f + ph * 6.2832f;
        circfill(cx + (int)(cosf(a) * cr), cy + (int)(sinf(a) * cr), 2, CLR_YELLOW);
    }

    // display
    rectfill(148, 52, 142, 44, CLR_BLACK);
    rect(148, 52, 142, 44, CLR_ORANGE);
    if (radioOn) {
        print(g.title, 154, 58, CLR_ORANGE);
        char l2[32];
        snprintf(l2, 32, "%s %s", LNAME[g.laras], g.region ? "bali" : "java");
        print(l2, 154, 70, CLR_YELLOW);
        snprintf(l2, 32, "%d bpm #%08X", tempo, g.seed);
        print(l2, 154, 82, CLR_YELLOW);
        float vt = vu / 16.0f; if (vt > 1) vt = 1;
        rectfill(154, 91, (int)(vt * 80), 2, CLR_ORANGE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    if (radioOn) {
        print(g.village, 152, 104, CLR_LIGHT_GREY);          // the ensemble's name
        char l3[24];
        snprintf(l3, 24, "gongan %d/%d", gi + 1, g.nGongan);
        print(l3, 152, 120, CLR_ORANGE);
        rad_phrase_dots(232, 124, g.nGongan, gi, CLR_ORANGE);
    }

    rad_knob_sel(&intensity, 4, 168, 148, 9, IRAMA[intensity], CLR_ORANGE);
    if (rad_knob_int(&tempo, 96, 168, 4, 218, 148, 9, "tempo", CLR_ORANGE)) bpm(tempo);
    if (rad_knob_sel(&toneSel, 4, 262, 148, 11, RAD_TONENAME[toneSel], CLR_ORANGE)) apply_tone();
    rad_power_led(radioOn, CLR_ORANGE, CLR_DARK_BROWN);

    rad_help_button(CLR_ORANGE);
    rad_footer("SPACE next gamelan   LEFT/RIGHT irama   H help");

    if (showHelp) {
        static const char *HELP[7][2] = {
            { "SPACE",      "next gamelan (a new village)" },
            { "R",          "same gamelan - suling is new" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "irama - lancar/tanggung/dadi/wiled" },
            { "UP/DOWN",    "tempo of this piece" },
            { "T",          "tone - mellow/warm/clear/bright" },
            { "M",          "radio power on / off" },
        };
        static const char *NOTES[3] = {
            "tuning is the song: slendro / pelog,",
            "every village's bronze tuned anew.",
            "bronze=MALLET gong=ombak pair suling=REED",
        };
        rad_help_panel("GAMELAN RADIO", HELP, 7, NOTES, 3, CLR_ORANGE);
    }
    ui_end();
}
