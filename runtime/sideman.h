// sideman.h — the shared WURLITZER SIDE MAN (1959) VOICE BANK: ten vacuum-tube
// percussion circuits, the oldest drum sound in this studio by two decades.
//
// The acid303.h / tr808.h / tr909.h move again, for the FIRST drum machine ever sold.
// A cart owns the PATTERN, the contact disc and the UI; this header owns the SOUND.
//
//   #include "studio.h"
//   #include "sideman.h"
//   void init(void) { sideman_build(SIDEMAN_BASE); }
//   sideman_fire(SIDEMAN_BASE, SM_TEMP1, 0, 0);   // voice, velocity boost, ms from now
//
// The bank occupies `base` .. `base + SIDEMAN_NSLOT-1`. Slots 5..8 are the user waves,
// so 9 is the usual base (SIDEMAN_BASE).
//
// ── what the machine actually is ──────────────────────────────────────────────
// Rudolph Wurlitzer Co., 1959, model 4900/5000. ELECTRO-MECHANICAL: a motor spins a
// disc with metal contacts across its face, wipers read them, and each closure fires
// one of ten tube circuits. A rotary knob picks which of twelve rhythms the wipers
// read; a slider sets the motor speed, which is the tempo. Ten front-panel buttons
// play the voices by hand. There is no pattern memory, no accent, no per-voice knob:
// the disc IS the pattern and the tube IS the sound.
//
// Roster (all ten, as named on the panel):
//   BASS DRUM · TOM TOM I · TOM TOM II · WOOD BLOCK · TEMPLE BLOCK I ·
//   TEMPLE BLOCK II · CLAVES · BRUSH · MARACAS · CYMBAL
//
// Note what that list is: FOUR of the ten voices are struck wood (wood block, two
// temple blocks, claves) and only two are membranes. This box is mostly a wooden
// percussion section, which is why the era's rhythm sound is remembered as a
// "plock" and not as a beat.
//
// ── why these voices sound the way they do, and how we copy it ────────────────
//
//   the plock (wood block, temple blocks, claves)
//     A tube circuit kicked by one contact pulse rings its resonant network and dies.
//     No noise, no rattle, no layers: ONE nearly pure tone, a hard front, a fast
//     clean decay. That combination is what the ear reads as WOOD. Copy: a damped
//     SINE/TRI at the block's pitch through a resonant FILTER_BAND at the same
//     frequency, decay 25..90 ms, shortest and driest at the top of the family.
//     The whole wooden family is tuned as a SET so the four read as one section:
//     claves highest and driest, then wood block, then the two temple blocks (which
//     are hollow and ring longer, a fourth apart).
//
//   the fullness
//     Two things, and neither is reverb. (1) Every one of these circuits is a
//     single-ended TUBE stage, so it saturates ASYMMETRICALLY and brings even
//     harmonics with it: a damped sine through DRIVE_ASYM stops being a sine and
//     starts having a body. That is the whole trick, and it is period-correct
//     rather than a sweetener. (2) The machine is band-limited, roughly 60 Hz to
//     6 kHz, so the mids carry everything and nothing up top competes.
//
//   the membranes (bass drum, two toms)
//     Same damped-ring circuit at low pitches, with a fast downward pitch drop for
//     the strike. The bass drum is SOFT and round, not a punch: this predates any
//     idea of a kick that hits.
//
//   the noise voices (brush, maracas, cymbal)
//     Filtered noise under an RC contour. The brush has a SOFT front (a swirl, not
//     a hit), the maracas is the shortest burst in the box, and the cymbal is thin
//     and splashy rather than metallic. A 1959 cymbal is not an 808 metal bank.
//
// ── what this header deliberately does NOT own: the box ───────────────────────
// The Side Man had no speaker. It fed the ORGAN's amplifier and came out of a wooden
// cabinet, and that stage is a real part of the remembered sound (mid-forward, top
// rolled off, gently saturated). That belongs to the CART, as an output chain:
// runtime/outboard.h is exactly that stage as a voicing table (EQ + IRON), so the
// cabinet is a rack the player can switch out rather than something baked in here.
// Keep this header's output HONEST and dry; let the cart put it in a box.

#ifndef SIDEMAN_H
#define SIDEMAN_H
#include "studio.h"

// ── voice roles, in panel order (index-stable) ────────────────────────────────
enum { SM_BASS, SM_TOM1, SM_TOM2, SM_WOOD, SM_TEMP1, SM_TEMP2,
       SM_CLAVES, SM_BRUSH, SM_MARACAS, SM_CYMBAL, SM_NV };

static const char *SIDEMAN_NAME[SM_NV] = {
    "BASS DRUM", "TOM TOM I", "TOM TOM II", "WOOD BLOCK", "TEMPLE BLOCK I",
    "TEMPLE BLOCK II", "CLAVES", "BRUSH", "MARACAS", "CYMBAL",
};
// short panel labels, for a UI with no room (the disc face)
static const char *SIDEMAN_SHORT[SM_NV] = {
    "BASS", "TOM I", "TOM II", "WOOD", "TEMP I", "TEMP II",
    "CLAV", "BRUSH", "MARAC", "CYMB",
};

// ── instrument-slot layout ────────────────────────────────────────────────────
// One slot per voice, plus the two layer slots the noise voices need. The wooden
// family gets a slot each (they are tuned differently AND damped differently, so
// they cannot share one).
enum { SMS_BASS, SMS_TOM1, SMS_TOM2, SMS_WOOD, SMS_TEMP1, SMS_TEMP2,
       SMS_CLAVES, SMS_BRUSH, SMS_MARACAS, SMS_CYMBAL,
       SMS_CLICK,      // the shared contact-pulse click: the front of a struck block
       SMS_CYMT };     // the cymbal's thin metallic layer (detuned squares)
#define SIDEMAN_NSLOT 12
#define SIDEMAN_BASE   9

// ── the voice table ───────────────────────────────────────────────────────────
// midi = the resonant network's pitch, dur = how long the gate holds (the ring is
// governed by the slot's own decay, so dur only needs to outlast it), vol 0..7.
// Kept as one table so the whole bank can be read, and re-voiced, in one place.
typedef struct {
    int   midi;      // resonant pitch
    int   vol;       // base volume 0..7
    int   dur;       // gate length ms
    float click;     // how much shared contact-click rides on the front (0 = none)
} SmVoice;

static const SmVoice SIDEMAN_V[SM_NV] = {
    /* BASS    */ { 36, 5, 220, 0.00f },   //  ~65 Hz, soft and round
    /* TOM I   */ { 53, 4, 180, 0.00f },   // ~175 Hz
    /* TOM II  */ { 49, 4, 200, 0.00f },   // ~139 Hz
    /* WOOD    */ { 91, 4,  60, 0.55f },   // ~1.6 kHz, the plock
    /* TEMP I  */ { 83, 4,  90, 0.45f },   //  ~1.0 kHz, hollow
    /* TEMP II */ { 78, 4, 110, 0.45f },   // ~740 Hz, hollower, a fourth down
    /* CLAVES  */ { 99, 4,  45, 0.70f },   // ~2.5 kHz, hardest and driest
    /* BRUSH   */ { 72, 3, 160, 0.00f },   // soft swirl, no front
    /* MARACAS */ { 84, 3,  40, 0.00f },   // shortest burst in the box
    /* CYMBAL  */ { 90, 3, 520, 0.00f },   // thin and splashy
};

// ── the tube stage ────────────────────────────────────────────────────────────
// Every voice runs through the same single-ended asymmetric saturation, because
// every voice on the real machine ran through the same kind of tube. This is the
// fullness; do not swap it for reverb.
#define SIDEMAN_TUBE 0.26f

static void sideman_build(int base) {
    // BASS DRUM — a low damped sine with a quick drop from ~+11 semitones. Soft,
    // round, short: a 1959 organ bass drum, not a punch.
    instrument(base + SMS_BASS, INSTR_SINE, 0, 190, 0, 45);
    instrument_filter(base + SMS_BASS, FILTER_LOW, 280, 2);
    instrument_env(base + SMS_BASS, 0, ENV_PITCH, 0, 40, 11.0f);

    // TOM TOM I / II — the same circuit tuned up, with less pitch drop and a
    // longer ring than the bass. Two fixed tunings, a third apart.
    instrument(base + SMS_TOM1, INSTR_SINE, 0, 155, 0, 40);
    instrument_filter(base + SMS_TOM1, FILTER_LOW, 800, 3);
    instrument_env(base + SMS_TOM1, 0, ENV_PITCH, 0, 30, 6.0f);
    instrument(base + SMS_TOM2, INSTR_SINE, 0, 175, 0, 45);
    instrument_filter(base + SMS_TOM2, FILTER_LOW, 700, 3);
    instrument_env(base + SMS_TOM2, 0, ENV_PITCH, 0, 32, 6.0f);

    // THE WOODEN FAMILY — one damped ring each through a resonant band at its own
    // pitch. Decay is the only thing separating them apart from tuning: shorter is
    // harder wood. TRI rather than SINE, because a struck block has a little odd
    // harmonic content above the ring.
    instrument(base + SMS_WOOD, INSTR_TRI, 0, 48, 0, 14);
    instrument_filter(base + SMS_WOOD, FILTER_BAND, 1580, 9);
    instrument(base + SMS_TEMP1, INSTR_TRI, 0, 80, 0, 20);
    instrument_filter(base + SMS_TEMP1, FILTER_BAND, 1000, 10);
    instrument(base + SMS_TEMP2, INSTR_TRI, 0, 98, 0, 24);
    instrument_filter(base + SMS_TEMP2, FILTER_BAND, 745, 10);
    instrument(base + SMS_CLAVES, INSTR_TRI, 0, 34, 0, 10);
    instrument_filter(base + SMS_CLAVES, FILTER_BAND, 2500, 8);

    // the contact CLICK — the front of a struck block is the pulse itself leaking
    // through before the network settles. One shared very short noise slot, mixed
    // in per voice by SmVoice.click.
    instrument(base + SMS_CLICK, INSTR_NOISE, 0, 8, 0, 4);
    instrument_filter(base + SMS_CLICK, FILTER_HIGH, 2600, 1);

    // BRUSH — a swirl, not a hit: the one voice in the box with a SOFT front.
    instrument(base + SMS_BRUSH, INSTR_NOISE, 22, 140, 0, 50);
    instrument_filter(base + SMS_BRUSH, FILTER_BAND, 1250, 3);
    instrument_env(base + SMS_BRUSH, 0, ENV_CUTOFF, 10, 120, 700.0f);

    // MARACAS — the shortest burst in the box, highpassed.
    instrument(base + SMS_MARACAS, INSTR_NOISE, 0, 30, 0, 12);
    instrument_filter(base + SMS_MARACAS, FILTER_HIGH, 5200, 2);

    // CYMBAL — thin and splashy. Highpassed noise carries it; two detuned squares
    // ride underneath for just enough clang to read as metal, nothing like an 808.
    instrument(base + SMS_CYMBAL, INSTR_NOISE, 0, 480, 0, 140);
    instrument_filter(base + SMS_CYMBAL, FILTER_HIGH, 5600, 2);
    instrument(base + SMS_CYMT, INSTR_SQUARE, 0, 300, 0, 90);
    instrument_filter(base + SMS_CYMT, FILTER_HIGH, 4800, 4);

    // the shared tube stage on every slot: asymmetric = EVEN harmonics = the body.
    for (int i = 0; i < SIDEMAN_NSLOT; i++) {
        instrument_drive_mode(base + i, DRIVE_ASYM);
        instrument_drive(base + i, SIDEMAN_TUBE);
    }
}

static int sideman__vv(int base, int boost) {
    int v = base + boost;
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire voice `v` at velocity boost `boost`, sample-accurately `delay` ms from now.
static void sideman_fire(int base, int v, int boost, int delay) {
    if (v < 0 || v >= SM_NV) return;
    const SmVoice *s = &SIDEMAN_V[v];
    int slot = base + SMS_BASS + v;          // roles 0..9 map 1:1 onto SMS_BASS..SMS_CYMBAL
    schedule_hit(delay, s->midi, slot, sideman__vv(s->vol, boost), s->dur);
    if (s->click > 0.01f)                    // the contact pulse on the front of a block
        schedule_hit(delay, 96, base + SMS_CLICK,
                     sideman__vv((int)(s->click * 4.0f), boost), 8);
    if (v == SM_CYMBAL) {                    // the thin metallic layer, detuned pair
        schedule_hit(delay, 90, base + SMS_CYMT, sideman__vv(2, boost), 300);
        schedule_hit(delay, 95, base + SMS_CYMT, sideman__vv(1, boost), 300);
    }
}

// the primary output slot of a voice, for a cart that wants to pan it or send it to
// a reverb/plate (outboard.h's ObSend needs slot numbers).
static int sideman_slot(int base, int v) {
    return base + SMS_BASS + ((v < 0 || v >= SM_NV) ? 0 : v);
}

#endif
