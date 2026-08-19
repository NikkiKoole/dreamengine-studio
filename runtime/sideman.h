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
// temple blocks, claves) and only three are membranes. This box is mostly a wooden
// percussion section, which is why the era's rhythm sound is remembered as a
// "plock" and not as a beat.
//
// ── why these voices sound the way they do, and how we copy it ────────────────
//
//   the plock (wood block, temple blocks, claves)
//     A tube circuit kicked by one contact pulse rings its resonant network and dies.
//     No noise, no rattle, no layers: ONE nearly pure tone, a hard front, a fast
//     clean decay. That combination is what the ear reads as WOOD. Copy: a damped
//     TRI at the block's pitch through a resonant FILTER_BAND at the SAME frequency
//     (measured: the ring lands on the note, not between the note and the band, so
//     the two reinforce instead of fighting), plus a 5 ms bandpassed noise tick for
//     the front. MEASURED: each block's resonant peak lands on its note to within
//     0.1%, 91..97% of its energy sits within ±150 cents of that one resonance, the
//     -40 dB decays run 28 / 45 / 70 / 89 ms, and the tick puts the first 2.5 ms
//     2.1..4.4 dB ABOVE the ring that follows it, which is the hard front.
//     The whole wooden family is tuned as a SET so the four read as one section, and
//     the set is a consonant F# stack rather than four arbitrary sizes:
//       TEMPLE BLOCK II  F#5  740 Hz   89 ms   hollowest, longest
//       TEMPLE BLOCK I   B5   988 Hz   70 ms   a FOURTH above its twin
//       WOOD BLOCK       F#6 1480 Hz   45 ms   a fifth above that
//       CLAVES           C#7 2217 Hz   28 ms   hardest and driest
//     Both orderings are monotonic on purpose: higher wood is shorter and drier.
//
//   the fullness
//     Two things, and neither is reverb.
//     (1) Every one of these circuits is a single-ended TUBE stage, so it saturates
//     and fills in a harmonic ladder: a damped sine on its own is a beep, and the
//     same sine through the shaper has a body. MEASURED, and it corrects what an
//     earlier draft of this comment claimed: DRIVE_ASYM is *not* an even-harmonic
//     generator. It is a tanh (an ODD function) with an asymmetric pre-gain, so at
//     every amount the ODD partials lead by 25..30 dB. What the asymmetry genuinely
//     buys is the EVEN partials existing AT ALL: on the wood block h2 climbs from
//     -94 dB bypassed to -39 dB at the shipped amount, a 55 dB rise, which is the
//     honest version of the claim. The ladder is what you hear: bypassed, the block
//     is a pure tone (h2 -94, h3 -30, h5 -44 — a beep); driven, it carries h3 -12,
//     h5 -19, h2 -39, h4 -48. The even half of that is the difference between
//     DRIVE_ASYM and DRIVE_SOFT, and it is period-correct rather than a sweetener.
//     (2) The machine is band-limited, roughly 60 Hz to 6 kHz, so the mids carry
//     everything and nothing up top competes. That limit lives HERE and not in the
//     cabinet, because it is the circuit: a 1959 tube percussion generator has a
//     coupling capacitor at the bottom and no bandwidth at the top, and it physically
//     cannot make the 19 kHz hiss that a highpassed noise source does. So every noise
//     voice is a BANDPASS, never a highpass, and this is not a detail: the first cut
//     highpassed them and MEASURED 96% of the maracas and 97% of the cymbal ABOVE
//     6 kHz, peaking at 19 and 21 kHz. Now: every voice's band peak and spectral
//     centroid are inside 60 Hz..6 kHz, nothing sits below 60 Hz, and the seven tonal
//     voices put 0..3% of their energy over 6 kHz.
//     BE HONEST ABOUT THE LIMIT OF THAT: the engine's FILTER_BAND is 2-pole, so its
//     skirt is 6 dB/oct, and the band above 6 kHz is 16 kHz wide against the 6 kHz
//     below it. So the three NOISE voices still spill 11% (brush) / 22% (maracas) /
//     33% (cymbal) of their energy over 6 kHz — a real RC network spills too, and the
//     only way to push those to zero here is a resonance so narrow the noise turns
//     into a whistle (at resonance 15 the maracas measures 72% of its energy inside
//     ±150 cents, which is a pitched ping and not a rattle). Resonance 13 is that
//     trade: band peak and centroid in band, rattle intact.
//
//   the membranes (bass drum, two toms)
//     Same damped-ring circuit at low pitches, with a SMALL downward pitch tuck for
//     the strike. Small is the point and it is measured: the first draft dropped the
//     bass drum 624 cents over 4 cycles, which at 65 Hz is a chirp you can hum. A
//     bridged-T ringing network does not sweep; it rings. So the drop is now 119
//     cents (99 → 92 Hz, settled inside 20 ms), which reads as a firmer front and not
//     as a pitch move. Measuring it takes care: at 92 Hz one cycle is 10.8 ms, so a
//     whole-period zero-crossing reading averages the whole trajectory away and reads
//     0 cents on a tuck you can hear. Use half-periods, or compare the band peak of
//     the first 25 ms against the settled one (+100 cents here, +400 on the chirp).
//     The bass drum is SOFT and round, not a punch: 4 ms to peak, no click layer, and
//     tuned to F#2 (92 Hz) rather than down at 65, because this predates any idea of
//     a kick that hits and a tube network feeding an organ speaker has no sub.
//     The three membranes are the same F# stack an octave and a half down, and the two
//     toms are a FOURTH apart exactly like the two temple blocks:
//       BASS DRUM  F#2   92 Hz   TOM TOM II  C#3 139 Hz   TOM TOM I  F#3 185 Hz
//
//   the noise voices (brush, maracas, cymbal)
//     Bandpassed noise under an RC contour. The brush has a SOFT front (a swirl, not
//     a hit: 24 ms to peak, the only voice in the box that does not start at once),
//     the maracas is the shortest burst in the box (-40 dB in 24 ms, under even the
//     claves), and the cymbal is thin and splashy rather than metallic. A 1959 cymbal
//     is not an 808 metal bank: two squares a TRITONE apart (so their partials
//     interleave instead of stacking) ride under the noise for just enough clang.
//     "Just enough" is two measurements, because one of them alone is misleading:
//     the metal layer adds only 0.26 dB to the cymbal's RMS (about 6% of its energy,
//     so it is not dominant) while its partials stand 14..25 dB above the local noise
//     floor (so it is plainly audible). Raising it to volume 3 buys 4 dB more
//     prominence and turns the cymbal into a gong.
//
// ── what this header deliberately does NOT own: the box ───────────────────────
// The Side Man had no speaker. It fed the ORGAN's amplifier and came out of a wooden
// cabinet, and that stage is a real part of the remembered sound (mid-forward, top
// rolled off, gently saturated). That belongs to the CART, as an output chain:
// runtime/outboard.h is exactly that stage as a voicing table (EQ + IRON), so the
// cabinet is a rack the player can switch out rather than something baked in here.
// Keep this header's output HONEST and dry; let the cart put it in a box.
//
// The bank leaves the master room to work: MEASURED, all ten voices firing at once
// peak at -4.5 dBFS and a four-bar pattern at -7.0 dBFS peak, 0 clipped samples,
// DC 0.0001. That is
// deliberate — analog-outboard-chain.md §2c is the measured finding that a chain
// demonstrated on an already-clipped mix demonstrates nothing.
//
// Numbers, method and what each voice measures: docs/design/sideman-voices.md.
// Bench: tools/carts/smprobe.c.

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
// they cannot share one). Roles 0..9 map 1:1 onto SMS_BASS..SMS_CYMBAL; APPEND only.
enum { SMS_BASS, SMS_TOM1, SMS_TOM2, SMS_WOOD, SMS_TEMP1, SMS_TEMP2,
       SMS_CLAVES, SMS_BRUSH, SMS_MARACAS, SMS_CYMBAL,
       SMS_CLICK,      // the shared contact-pulse click: the front of a struck block
       SMS_CYMT };     // the cymbal's thin metallic layer (detuned squares)
#define SIDEMAN_NSLOT 12
#define SIDEMAN_BASE   9

// ── the voice table ───────────────────────────────────────────────────────────
// midi = the resonant network's pitch, dur = how long the gate holds (the ring is
// governed by the slot's own decay, so dur only needs to outlast it), vol 0..7 is
// the coarse step and `level` the fine trim that balances the ten against each
// other (measured solo peaks, see the doc). `tube` scales SIDEMAN_TUBE per voice:
// the tube is the same everywhere, but how hard each circuit DRIVES it is not, and
// a high voice's drive harmonics leave the 6 kHz band while a low voice's do not.
// Kept as one table so the whole bank can be read, and re-voiced, in one place.
typedef struct {
    int   midi;      // resonant pitch
    int   vol;       // base volume 0..7 (the coarse step: 1 step = 1.9 dB)
    int   dur;       // gate length ms
    float level;     // fine output trim 0..1 (instrument_level), for bank balance
    float tube;      // how hard this circuit drives the shared tube, 0..1 of SIDEMAN_TUBE
    int   click;     // shared contact-click volume 0..7 riding the front (0 = none)
} SmVoice;

static const SmVoice SIDEMAN_V[SM_NV] = {
    /* BASS    */ { 42, 5, 200, 0.77f, 1.00f, 0 },   // F#2  92 Hz, soft and round, no front
    /* TOM I   */ { 54, 5, 180, 0.72f, 1.00f, 1 },   // F#3 185 Hz, a fourth over its twin
    /* TOM II  */ { 49, 5, 200, 0.71f, 1.00f, 1 },   // C#3 139 Hz
    /* WOOD    */ { 90, 6,  70, 0.88f, 0.85f, 4 },   // F#6 1480 Hz, the plock
    /* TEMP I  */ { 83, 6,  95, 0.86f, 0.90f, 3 },   // B5   988 Hz, hollow
    /* TEMP II */ { 78, 6, 120, 0.85f, 0.95f, 3 },   // F#5  740 Hz, hollowest, a fourth down
    /* CLAVES  */ { 97, 6,  45, 0.92f, 0.60f, 5 },   // C#7 2217 Hz, hardest and driest
    /* BRUSH   */ { 72, 6, 190, 0.89f, 0.50f, 0 },   // soft swirl, no front
    /* MARACAS */ { 84, 6,  40, 0.77f, 0.50f, 0 },   // shortest burst in the box
    /* CYMBAL  */ { 90, 6, 560, 0.75f, 0.40f, 0 },   // thin and splashy
};

// ── the tube stage ────────────────────────────────────────────────────────────
// Every voice runs through the same single-ended asymmetric saturation, because
// every voice on the real machine ran through the same kind of tube. This is the
// fullness; do not swap it for reverb.
//
// 0.45 is MEASURED, not guessed. The shaper's pre-gain is amount² × 24, so the curve
// saturates fast and everything happens below 0.6. On the wood block, across amounts
// 0.00 / 0.30 / 0.45 / 0.60:
//   h3       -30 / -17 / -12 / -10 dB      the ladder arriving
//   h5       -44 / -31 / -19 / -16 dB
//   in-band  100 /  98 /  92 /  88 %       energy within ±150 cents of the resonance
//   claves      0 /   0 /   3 /   7 %      energy ABOVE the machine's 6 kHz limit
// 0.30 is still thin (h5 20 dB down on the shipped value). Past 0.45 the ladder gains
// 2 dB and the claves' out-of-band energy DOUBLES, because a 2.2 kHz voice's third
// harmonic already sits at 6.7 kHz — that is the mush, and it is the band limit
// breaking rather than anything getting fuller. Pushed to 1.0 the shaper measures
// within 1 dB of 0.5 and you have paid for nothing. So: enough to fill the ladder,
// stopped before the band breaks.
#define SIDEMAN_TUBE 0.45f

// ── the bank's output trim ────────────────────────────────────────────────────
// ONE bank-wide gain, so SmVoice.level stays a readable RELATIVE balance and the
// HEADROOM is a single number you can A/B. 0.78 is measured: it puts a four-bar
// pattern at -7.0 dBFS peak and all ten voices at once at -4.5, which is the room the
// master chain needs (analog-outboard-chain.md §2c).
#define SIDEMAN_TRIM 0.78f

// ── the layer slots ───────────────────────────────────────────────────────────
// The contact click sits in the 3.4 kHz band where a struck block's front lives —
// a BAND, not a highpass, so it is a wooden tick and not a hiss (the band limit is
// the circuit, see the header notes).
#define SIDEMAN_CLICK_HZ  3400
// the cymbal's metal layer, deliberately UNDER the noise
#define SIDEMAN_CYMT_VOL  2
// how much of the contact pulse leaks through before the network settles (0 = no front)
#define SIDEMAN_CLICK_GAIN 1.00f
// The three noise channels share one kind of RC network, so they share its selectivity;
// only the centre frequency differs per voice. This is also the number that decides how
// much of a noise voice lands ABOVE the machine's 6 kHz limit.
#define SIDEMAN_NOISE_RES 13
// The strike TUCK, in semitones: how far above its resting pitch a membrane starts.
// A bridged-T ringing network does not sweep, so these are small on purpose (see the
// header note on the 624-cent chirp the first draft had). The bass needs the larger
// NUMBER for the smaller EFFECT, because its cycle is 10.8 ms long: 5 semitones over
// 26 ms measures 119 cents of actual trajectory, while the toms' 3.5 over 16 ms
// measures ~200.
#define SIDEMAN_BASS_TUCK 5.0f
#define SIDEMAN_TOM_TUCK  3.5f

static void sideman_build(int base) {
    // BASS DRUM — a low damped sine with a SMALL tuck (measured 119 cents, settled in
    // 20 ms). Soft, round, short: a 1959 organ bass drum, not a punch — the 4 ms amp
    // attack is the coupling cap, a tube stage cannot start instantly, and it measures
    // its peak 5.6 dB BELOW the ring that follows, the opposite of every other voice.
    instrument(base + SMS_BASS, INSTR_SINE, 4, 175, 0, 45);
    instrument_filter(base + SMS_BASS, FILTER_LOW, 320, 2);
    instrument_env(base + SMS_BASS, 0, ENV_PITCH, 0, 26, SIDEMAN_BASS_TUCK);

    // TOM TOM I / II — the same circuit tuned up, with a faster tuck than the bass
    // and a shorter ring. Two fixed tunings, a fourth apart. They take the smallest
    // click in the bank (volume 1), which is the stick and not a block's front.
    instrument(base + SMS_TOM1, INSTR_SINE, 1, 150, 0, 40);
    instrument_filter(base + SMS_TOM1, FILTER_LOW, 850, 3);
    instrument_env(base + SMS_TOM1, 0, ENV_PITCH, 0, 16, SIDEMAN_TOM_TUCK);
    instrument(base + SMS_TOM2, INSTR_SINE, 1, 170, 0, 45);
    instrument_filter(base + SMS_TOM2, FILTER_LOW, 700, 3);
    instrument_env(base + SMS_TOM2, 0, ENV_PITCH, 0, 18, SIDEMAN_TOM_TUCK);

    // THE WOODEN FAMILY — one damped ring each through a resonant band at its own
    // pitch. Decay is the only thing separating them apart from tuning: shorter is
    // harder wood. TRI rather than SINE, because a struck block has a little odd
    // harmonic content above the ring. Resonance 10 across the family so all four
    // are the same KIND of resonance and only the tuning and damping differ.
    instrument(base + SMS_WOOD, INSTR_TRI, 0, 45, 0, 14);
    instrument_filter(base + SMS_WOOD, FILTER_BAND, 1480, 10);
    instrument(base + SMS_TEMP1, INSTR_TRI, 0, 70, 0, 20);
    instrument_filter(base + SMS_TEMP1, FILTER_BAND, 988, 10);
    instrument(base + SMS_TEMP2, INSTR_TRI, 0, 90, 0, 24);
    instrument_filter(base + SMS_TEMP2, FILTER_BAND, 740, 10);
    instrument(base + SMS_CLAVES, INSTR_TRI, 0, 28, 0, 10);
    instrument_filter(base + SMS_CLAVES, FILTER_BAND, 2217, 10);

    // the contact CLICK — the front of a struck block is the pulse itself leaking
    // through before the network settles. One shared very short noise slot, mixed
    // in per voice by SmVoice.click.
    instrument(base + SMS_CLICK, INSTR_NOISE, 0, 5, 0, 3);
    instrument_filter(base + SMS_CLICK, FILTER_BAND, SIDEMAN_CLICK_HZ, 2);

    // BRUSH — a swirl, not a hit: the one voice in the box with a SOFT front, so it
    // gets the only slow amp attack in the bank. The cutoff contour is the sweep of
    // the brush across the head, small enough that the front stays soft.
    instrument(base + SMS_BRUSH, INSTR_NOISE, 24, 150, 0, 55);
    instrument_filter(base + SMS_BRUSH, FILTER_BAND, 1500, SIDEMAN_NOISE_RES - 2);
    instrument_env(base + SMS_BRUSH, 0, ENV_CUTOFF, 24, 140, 500.0f);

    // MARACAS — the shortest burst in the box, in a narrow band up top. A BAND and
    // not a highpass: highpassed white noise is a digital hiss reaching 19 kHz, and
    // the machine's channel stops at 6.
    instrument(base + SMS_MARACAS, INSTR_NOISE, 0, 24, 0, 10);
    instrument_filter(base + SMS_MARACAS, FILTER_BAND, 3600, SIDEMAN_NOISE_RES);

    // CYMBAL — thin and splashy. Bandpassed noise carries it; two squares a tritone
    // apart ride underneath (so their partials interleave instead of stacking) for
    // just enough clang to read as metal, nothing like an 808.
    instrument(base + SMS_CYMBAL, INSTR_NOISE, 0, 500, 0, 150);
    instrument_filter(base + SMS_CYMBAL, FILTER_BAND, 4200, SIDEMAN_NOISE_RES - 1);
    instrument(base + SMS_CYMT, INSTR_SQUARE, 0, 320, 0, 100);
    instrument_filter(base + SMS_CYMT, FILTER_BAND, 4200, SIDEMAN_NOISE_RES - 2);

    // the shared tube stage on every slot: the ladder that turns a beep into a body.
    // Amount is SIDEMAN_TUBE scaled by how hard each circuit drives it.
    for (int i = 0; i < SIDEMAN_NSLOT; i++) instrument_drive_mode(base + i, DRIVE_ASYM);
    for (int v = 0; v < SM_NV; v++) {
        instrument_drive(base + SMS_BASS + v, SIDEMAN_TUBE * SIDEMAN_V[v].tube);
        instrument_level(base + SMS_BASS + v, SIDEMAN_TRIM * SIDEMAN_V[v].level);
    }
    instrument_drive(base + SMS_CLICK, SIDEMAN_TUBE * 0.40f);   // noise + saturation = mush
    instrument_drive(base + SMS_CYMT,  SIDEMAN_TUBE * 0.70f);
    instrument_level(base + SMS_CLICK, SIDEMAN_TRIM * 0.85f * SIDEMAN_CLICK_GAIN);
    instrument_level(base + SMS_CYMT,  SIDEMAN_TRIM * 0.55f);
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
    if (s->click > 0)                        // the contact pulse on the front of a block
        schedule_hit(delay, 96, base + SMS_CLICK, sideman__vv(s->click, boost), 8);
    if (v == SM_CYMBAL) {                    // the thin metallic layer, a tritone apart
        schedule_hit(delay, 91, base + SMS_CYMT, sideman__vv(SIDEMAN_CYMT_VOL, boost), 320);
        schedule_hit(delay, 97, base + SMS_CYMT, sideman__vv(SIDEMAN_CYMT_VOL, boost), 320);
    }
}

// the primary output slot of a voice, for a cart that wants to pan it or send it to
// a reverb/plate (outboard.h's ObSend needs slot numbers).
static int sideman_slot(int base, int v) {
    return base + SMS_BASS + ((v < 0 || v >= SM_NV) ? 0 : v);
}

#endif
