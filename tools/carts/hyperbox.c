/* de:meta
{
  "slug": "hyperbox",
  "title": "hyperbox",
  "status": "active",
  "created": "2026-07-26",
  "kind": ["instrument", "tech-demo"],
  "genre": null,
  "teaches": ["subtractive-synth", "drum-synthesis", "step-sequencer", "scale-quantize"],
  "homage": "ReBirth RB-338 (Propellerhead, 1997) — the four-box rack shape, and the idea that a rack IS a genre argument. The sound aimed at is hyperpop's, the 100 gecs / SOPHIE lineage: pitched-up vocals, seven detuned saws, and a master bus deliberately overdriven.",
  "lineage": "The tiny first build of docs/design/contemporary-rebirth.md: a CONTEMPORARY ReBirth clones TECHNIQUES, not machines, because modern genres were never made on gear. It is also the showcase cart for multiband() (audio-notes §17 #34, Rung A of that doc) — the OTT box, three bands squashed down AND pushed up, which is the sound of a modern master. The rack shape is acidcandy/acidrack's ACCORDION (slim strips, one device expanded, sound never depends on what is open); the supersaw box is supersaw's instrument_unison stack; the drums are tr909.h; the keybed is keybed.h. NOT a mic cart: the voice box is INSTR_VOICE (deterministic, no permission prompt) standing in until the formant/harmoniser dial lands (that doc's Rung B).",
  "description": {
    "summary": "A four-box hyperpop rack where the master chain has no bypass. A playable pitch-snapped vocal lead, seven detuned saws you can also play, always-clipped drums you can program, and OTT pinned on — everything is too loud on purpose. Tap a box to open it; the sound never depends on which one is open.",
    "detail": "Four devices in one screen, ReBirth-style, but cloning MOVES instead of machines: hard tune, the seven-saw wall, ratcheted drums, and a master that is permanently working. VOICE is INSTR_VOICE on a playable keybed (touch/mouse/QWERTY/MIDI) with RETUNE as the hard-tune speed — at 0 the pitch JUMPS between notes, turned up it slides; play legato and you hear the difference. Its three presets move the vocal-tract macros (CHIP small and pitched-up, ROBO pressed and flat, CHOIR stacks a fifth and an octave). SAW is one slot with instrument_unison(7) plus DETUNE/BRIGHT/TOY, where TOY crushes toward a cheap ringtone on purpose — and it has the SAME keybed, an octave lower, because one keybed FOLLOWS FOCUS between the two melodic boxes: one key there is all seven saws at once. DRUMS is four tr909.h lanes (kick/snare/hat/clap) over 16 steps, and a step cycles off → hit → RATCHET, a sample-accurate 1/32 burst via schedule_hit — plus a pad row you play by hand, with REC to land what you play in the grid at the NEAREST step (floor-quantizing would put every hand-played hit one step behind where you heard it). The pads light from every hit, sequenced or played, so the row doubles as the box's live readout. MASTER is multiband() with all four amounts on knobs plus a hard clipper, in that order, with no bypass drawn anywhere. Play while the pattern runs and it steps aside — but only the part you took: hold the vocal and the riff yields, hold the saw and the chord stab yields.",
    "controls": "It starts playing on load. SPACE stops/starts. Tap a device strip to expand it (the others stay audible). VOICE and SAW share one keybed, and opening either hands it over — the transport says which (keys VOICE / keys SAW), so a MIDI keyboard keeps playing while you program drums. Play it by touch, mouse, the QWERTY musical-typing rows, or a plugged-in MIDI keyboard; Z/X shift the octave. In VOICE: 1/2/3 pick a preset. In SAW: STAB fires the chord the pattern is on. In DRUMS: tap a step to cycle it off → hit → ratchet, or play the BD/SD/CH/CP pads by hand; arm REC and what you play lands in the grid on the nearest step (the written step flashes white so you can see where it went). Knobs drag vertically. RISER throws a sweep."
  },
  "todo": [
    "v3: swap the INSTR_VOICE stand-in for the real vocal chain (mic + autotune_mic + the formant/harmoniser dial) once contemporary-rebirth.md Rung B lands. The keybed then plays the mic'd voice instead of a synth one.",
    "The pattern is one bank with no arrangement, per the constraint-is-the-product thesis. If a generator is added it should deliberately overshoot (too fast, too loud), since in this genre the mistakes are the genre.",
    "REC can only ADD. The one way to erase is tapping grid cells off one at a time, so a bad take has to be un-picked by hand — a per-lane CLR (and maybe an undo of the last recorded pass) is the missing half of the pad row.",
    "v1 shipped with three LOW/MID/HI bars sized by hardcoded literals, which read as sliders that ignore you, and ten captions that DESCRIBED the constraints instead of being them. The maker's verdict was 'promising but feels very incomplete, lots of UI that doesn't do anything', which is the answer to §7's 'is no bypass honest or hostile' question: constraint-as-feature only reads as opinionated if what REMAINS is deep. Keep that bar for the amapiano rack."
  ]
}
de:meta */

// hyperbox — a tiny contemporary-ReBirth rack (hyperpop), and the showcase for multiband().
//
// The thesis (docs/design/contemporary-rebirth.md): ReBirth cloned MACHINES because in 1997 the
// genre lived in unobtainable boxes. Modern genres were never made on gear, so a contemporary
// version clones MOVES: hard tune, the seven-saw wall, always-clipped drums, and a master bus with
// no bypass. Four boxes, one screen, no arrangement view — the constraint IS the product.
//
// v2 note on what "constrained" has to mean. v1 took the constraint thesis literally and ended up
// with a rack that read as UNFINISHED rather than opinionated: three master bars sized by literals
// (so they could never move), ten captions that stated the constraints in words instead of enacting
// them, one drum lane, and no way to play the lead instrument at all. Nothing was dead code — every
// knob was wired — but almost nothing was DEEP. The fix is not more knobs, it is that each of the
// four boxes now rewards being opened: the voice is playable, the drums are programmable, and every
// master amount is real. The bypass is still absent, because that part IS the thesis.
//
// v3 note: ONE KEYBED, TWO BOXES. v2 left the saw box as knobs-plus-a-button — the only box you
// couldn't play — because the keybed was wired to VOX. Now keybed.h FOLLOWS FOCUS (kb_point) between
// the two melodic boxes, an octave lower for the wall, and each box's pattern part steps aside only
// for the box your hands are actually on. Two hazards that came with sharing it, both fixed here and
// worth copying into any other accordion rack:
//   1. keybed_layout() is STICKY and keybed_update() hit-tests it whether or not keys were drawn, so
//      the rect stayed live UNDER the next panel: v2's drum-grid taps also played vocal notes. draw()
//      retires it (0,0,0,0) when no melodic box is open. QWERTY/MIDI don't use the rect.
//   2. keybed_config() wipes the held-note table WITHOUT firing the off callbacks, so re-pointing
//      straight into it orphans a voice per held finger. kb_point() calls keybed_all_off() first.
//
// What each box is made of, engine-wise:
//   VOICE   INSTR_VOICE + keybed.h (touch/mouse/QWERTY/MIDI) + note_glide as the RETUNE knob — an
//           honest stand-in for a mic'd vocal chain: deterministic, no permission prompt
//   SAW     instrument_unison(7) + instrument_unison_detune (live bloom) + filter + crush ("toy"),
//           on the same keybed: one key = the whole seven-saw stack
//   DRUMS   tr909.h — tr909_fire for a hit, tr909_fire_stroke(TR9_ST_RATCHET) for a 1/32 burst,
//           both through ONE hit_lane() so a pad tap and a sequenced step are the same event
//           (v4: the pad row + REC, so the drums are played as well as programmed)
//   MASTER  multiband() → drive_insert(DRIVE_HARD) via fx_order, both pinned on
//
// The accordion is acidrack's: slim strips always visible, one device expanded, and the SOUND NEVER
// DEPENDS ON WHAT IS OPEN. That is what lets four deep devices share a 320x200 screen.
//
// The master effects are SET-AND-HOLD (CLAUDE.md): apply_fx() only fires when a value actually
// changed, because re-calling a bus effect every frame rebuilds its DSP and stutters silently.
// multiband() itself is ride-safe, but the clipper next to it is not, so the whole chain goes
// through one change-detector.

#include "studio.h"
#include "tr909.h"
#include "ui.h"
#include "keybed.h"
#include "cursor.h"

#define SAW    5          // the seven-saw wall
#define VOX    6          // the "vocal" lead (INSTR_VOICE)
#define RISER  7          // the pitch-riser noise sweep
// tr909.h owns slots TR909_BASE(9) .. 21 — do not reuse them here.

#define STEPS  16
#define BPM    160        // "160 bpm, everything too loud by design"
#define STEP_MS ((int)(15000.0f / (float)BPM))   // one 1/16 in ms (the ratchet's burst spacing)

// ── the four devices (accordion: one expanded, the rest slim) ─────────────────────────────────
enum { D_VOICE, D_SAW, D_DRUM, D_MASTER, D_N };
static const char *D_NAME[D_N] = { "VOICE ENGINE", "SUPERSAW BOX", "BLOWN-OUT DRUMS", "MASTER DESTRUCTION" };
static const int   D_HUE[D_N]  = { CLR_PINK, CLR_BLUE, CLR_RED, CLR_YELLOW };
static int focus = D_DRUM;              // open on the drums: the most obviously interactive box
static int muted[D_N] = { 0, 0, 0, 0 }; // MASTER's mute is the closest thing to a bypass, and it is a MUTE

#define TP_H     14                     // the persistent transport bar
#define STRIP_H  18                     // a collapsed device strip
#define BODY_H   (SCREEN_H - TP_H - (D_N - 1) * STRIP_H)

// ── the drum grid: four tr909 lanes, a step cycles off -> hit -> ratchet ───────────────────────
enum { ST_OFF, ST_HIT, ST_RATCHET };
#define LANES 4
static const int  LANE_V[LANES]    = { TR9_BD, TR9_SD, TR9_CH, TR9_CP };
static const char *LANE_NAME[LANES] = { "BD", "SD", "CH", "CP" };
static const int  LANE_VEL[LANES]  = { 1, 0, -2, -1 };   // the hat tucks under the kick
static unsigned char grid[LANES][STEPS] = {
    { ST_HIT,0,0,0, ST_HIT,0,0,ST_RATCHET, ST_HIT,0,0,0, ST_HIT,0,ST_RATCHET,ST_RATCHET },
    { 0,0,0,0, ST_HIT,0,0,0, 0,0,0,0, ST_HIT,0,0,ST_RATCHET },
    { 0,0,ST_HIT,0, 0,0,ST_HIT,0, 0,0,ST_HIT,0, 0,0,ST_HIT,ST_RATCHET },
    { 0,0,0,0, 0,0,0,0, 0,0,0,0, ST_HIT,0,0,0 },
};

// knobs (0..1, the ui.h contract)
static float k_retune = 0.15f;   // THE knob: 0 = instant robot jump .. 1 = a natural glide. Never reaches "off"
static float k_detune = 0.42f;   // the unison bloom
static float k_bright = 0.70f;   // filter cutoff
static float k_toy    = 0.18f;   // crush toward ringtone
static float k_up     = 0.45f;   // multiband's UPWARD amount — the macro that makes the master "always on"
// The three band DOWN amounts. In v1 these were literals inside draw()'s bar heights, so the LOW/MID/HI
// bars were sized by constants and could never move: three sliders that ignore you, in the one box
// labelled MASTER DESTRUCTION. multiband() always took them per band — they were never exposed.
static float k_low    = 0.60f;   // sub squash
static float k_mid    = 0.48f;   // body squash — lightest, so the riff still speaks
static float k_high   = 0.72f;   // air squash — hardest, the hyperpop sizzle

// transport — STARTS RUNNING. v1 booted stopped, which meant a headless render was pure silence and,
// worse, every knob felt dead until you found SPACE. A rack should be making noise when you meet it.
static int   playing = 1;
static int   step = -1;          // last step fired
static float clock_pos = 0.0f;   // 0..STEPS, our own step clock (bpm() drives the tempo)

// voices
static int   vox_h = -1;         // the pattern's held "vocal" note
static int   vox_stack[2] = { -1, -1 };   // CHOIR's two extra stacked voices
static int   saw_h[3] = { -1, -1, -1 };
static int   riser_h = -1;
static float riser_t = -1.0f;    // 0..1 while a riser is in flight, <0 = idle

// hand-played notes (keybed.h manages the gestures, this cart manages the voices so RETUNE applies)
static int hand_h[128];   /* handle per midi; NOT kb_h — keybed.h owns that name for its layout height (the `map` trap again) */
static int kb_held_n = 0;
// WHICH instrument your hands are on. v2 wired the keybed to VOX only, so the saw box was a knob
// panel with one button — the one box you couldn't play. Now the keybed FOLLOWS FOCUS between the
// two melodic boxes, and whichever one you're holding steps its own pattern part aside.
// Named hand_slot, not kb_slot: keybed.h already owns that global (same trap as kb_h above).
static int hand_slot = VOX;

// vocal presets — vowel / size / effort on INSTR_VOICE's three macros
enum { P_CHIP, P_ROBO, P_CHOIR, P_COUNT };
static const char *P_NAME[P_COUNT] = { "CHIP", "ROBO", "CHOIR" };
static int preset = P_CHIP;

// tr909 per-voice knobs (the cart owns these arrays; 0.5 = neutral)
static float ktune[TR9_NV], kdecay[TR9_NV], kcolor[TR9_NV];

// the pad row: play the drums with your hands, and REC writes what you play into the grid.
// pad_glow lights from EVERY hit, sequenced or hand-played (see hit_lane) — the pads are the box's
// live readout, which is also what tells you the sequencer is still running while you play over it.
static float pad_glow[LANES];
static int   rec = 0;                   // armed: a pad tap lands in the grid at the nearest step
static int   rec_flash = -1;            // the step just written…
static float rec_flash_t = 0.0f;        // …and how much of its highlight is left

// the melody: scale degrees, snapped — a minor-pentatonic riff, one note per 4 steps
static const int RIFF[4]   = { 0, 3, 5, 3 };
static const int PENTA[5]  = { 0, 3, 5, 7, 10 };
#define ROOT 69                     // A4 — pitched-up on purpose (the chipmunk register)

static int snap_penta(int degree) { // degree → midi, wrapping octaves
    int oct = degree / 5, i = degree % 5;
    if (i < 0) { i += 5; oct--; }
    return ROOT + 12 * oct + PENTA[i];
}

// ── the locked master chain ──────────────────────────────────────────────────────────────────
// multiband() squashes three bands and lifts the quiet detail; drive_insert(DRIVE_HARD) is the
// clipper. fx_order pins squash BEFORE clip: level everything, then slam the result.
static const int MASTER_CHAIN[] = { FX_MULTIBAND, FX_DRIVE };

static void apply_fx(int force) {
    static float p_up = -1.0f, p_toy = -1.0f, p_lo = -1.0f, p_md = -1.0f, p_hi = -1.0f;
    static int   p_mm = -1;
    if (!force && p_up == k_up && p_toy == k_toy && p_lo == k_low &&
        p_md == k_mid && p_hi == k_high && p_mm == muted[D_MASTER]) return;   // only on change
    p_up = k_up; p_toy = k_toy; p_lo = k_low; p_md = k_mid; p_hi = k_high; p_mm = muted[D_MASTER];
    if (muted[D_MASTER]) multiband(0, 0, 0, 0, 0.0f);          // mix 0 = byte-identical bypass
    else                 multiband(k_low, k_mid, k_high, k_up, 1.0f);
    drive_insert(0.30f, DRIVE_HARD, muted[D_MASTER] ? 0.0f : 0.85f);
    instrument_crush(SAW, 5.0f - k_toy * 3.0f, 1.0f - k_toy * 0.8f, k_toy);
    if (force) fx_order(0, MASTER_CHAIN, 2);
}

static void voice_macros(void) {   // the preset's vowel / size / effort
    float vowel  = preset == P_CHIP ? 0.85f : preset == P_ROBO ? 0.35f : 0.60f;
    float size   = preset == P_CHIP ? 0.08f : preset == P_ROBO ? 0.30f : 0.45f;  // small tract = pitched up
    float effort = preset == P_CHIP ? 0.80f : preset == P_ROBO ? 1.00f : 0.65f;
    instrument_harmonics(VOX, vowel);
    instrument_timbre(VOX, size);
    instrument_morph(VOX, effort);
}

// ── hand-played voices: RETUNE is the glide, so legato playing IS the hard-tune demo ───────────
static void kb_note(int midi, int vel) {
    if (midi < 0 || midi > 127 || hand_h[midi] >= 0) return;
    hand_h[midi] = note_on(midi, hand_slot, vel ? vel : 6);
    // RETUNE is the VOCAL knob, so only the vocal glides. On the saw a key is one strike of the
    // seven-saw stack — gliding it would just make the wall seasick, and hide what DETUNE does.
    if (hand_slot == VOX) note_glide(hand_h[midi], (int)(k_retune * 120.0f));
    kb_held_n++;
}
static void kb_off(int midi) {
    if (midi < 0 || midi > 127 || hand_h[midi] < 0) return;
    note_off(hand_h[midi]);
    hand_h[midi] = -1;
    if (kb_held_n > 0) kb_held_n--;
}

// Re-point the keybed at another instrument. keybed_all_off() FIRST, always: keybed_config()
// wipes its held-note table without firing the off callbacks, so re-pointing straight into it
// leaks every held voice (a stuck note per finger, on the box you just left).
static void kb_point(int slot, int base_oct, int nwhite) {
    if (hand_slot == slot) return;
    keybed_all_off();                       // fires kb_off per held note → our handles released
    hand_slot = slot;
    keybed_config(slot, base_oct, nwhite);   // slot is unused in unmanaged mode; passed for honesty
}

void init(void) {
    bpm(BPM);
    // the saw wall
    instrument(SAW, INSTR_SAW, 3, 210, 2, 150);   // a DECAYING stab, not a pad: the gap it leaves is what lets the kick punch
                                                  // through an always-on master (an OTT'd sustain fills every hole)
    instrument_unison(SAW, 7, 0.10f + k_detune * 0.60f);
    instrument_filter(SAW, FILTER_LOW, 900, 4);
    instrument_level(SAW, 0.26f);
    // the "vocal" lead
    instrument(VOX, INSTR_VOICE, 12, 40, 7, 90);
    instrument_level(VOX, 0.40f);
    voice_macros();
    // the riser
    instrument(RISER, INSTR_NOISE, 8, 200, 4, 120);
    instrument_filter(RISER, FILTER_BAND, 1200, 9);
    instrument_level(RISER, 0.28f);
    // drums
    for (int v = 0; v < TR9_NV; v++) { ktune[v] = kdecay[v] = kcolor[v] = 0.5f; }
    kdecay[TR9_BD] = 0.70f;                 // a long, loud kick
    tr909_build(TR909_BASE);
    tr909_metal(TR909_BASE, 0.40f, 0.33f);
    // the keybed — this cart owns the voices so RETUNE can ride them
    for (int i = 0; i < 128; i++) hand_h[i] = -1;
    keybed_config(VOX, 4, 14);              // 14 white keys = two octaves from C4
    keybed_manage_voices(false);
    keybed_on_note(kb_note);
    keybed_on_off(kb_off);
    apply_fx(1);
}

// ── one 1/16 step of the pattern ──────────────────────────────────────────────────────────────
// the wall as a chord: root-octave-down, a fourth under, the note. One place, so the pattern's
// half-bar stab and the STAB pad cannot drift apart.
static void saw_chord(int midi) {
    for (int i = 0; i < 3; i++) if (saw_h[i] >= 0) note_off(saw_h[i]);
    saw_h[0] = note_on(midi - 12, SAW, 5);
    saw_h[1] = note_on(midi - 5,  SAW, 4);
    saw_h[2] = note_on(midi,      SAW, 4);
}
static void saw_release(void) {
    for (int i = 0; i < 3; i++) if (saw_h[i] >= 0) { note_off(saw_h[i]); saw_h[i] = -1; }
}

// ONE path for a drum hit, whoever asked for it: the sequencer, or your finger on a pad. That is
// what makes the pads light up with the pattern instead of only under your own taps — the panel is
// then a live readout of the box, not four buttons that happen to sit near a grid.
static void hit_lane(int l, int ratchet, int step_ms) {
    if (l < 0 || l >= LANES) return;
    if (ratchet) tr909_fire_stroke(TR909_BASE, LANE_V[l], TR9_ST_RATCHET, LANE_VEL[l], 0, step_ms, ktune, kdecay, kcolor);
    else         tr909_fire(TR909_BASE, LANE_V[l], LANE_VEL[l], 0, ktune, kdecay, kcolor);
    pad_glow[l] = 1.0f;
}

static void fire_step(int s) {
    if (!muted[D_DRUM]) for (int l = 0; l < LANES; l++) {
        if (grid[l][s] == ST_HIT)          hit_lane(l, 0, STEP_MS);
        else if (grid[l][s] == ST_RATCHET) hit_lane(l, 1, STEP_MS);   // a 1/32 burst — the "too fast at the end"
    }

    if ((s & 3) == 0) {                              // one riff note + one chord stab per beat
        int deg = RIFF[(s >> 2) & 3];
        int midi = snap_penta(deg);
        int glide_ms = (int)(k_retune * 120.0f);
        // The pattern STEPS ASIDE while you play — but only the part you took. Nothing clever,
        // just: your hands win, on the box your hands are actually on.
        int riff_on = !muted[D_VOICE] && !(hand_slot == VOX && kb_held_n > 0);
        if (riff_on) {
            // RETUNE: 0 = the pitch JUMPS (hard tune), up = it slides there (natural correction)
            if (vox_h < 0) {
                vox_h = note_on(midi, VOX, 6);
                note_glide(vox_h, glide_ms);
            } else {
                note_glide(vox_h, glide_ms);
                note_pitch(vox_h, (float)midi);      // no retrigger — the snap you HEAR
            }
            if (preset == P_CHOIR) {                 // the stack: two more voices, an interval apart
                for (int i = 0; i < 2; i++) {
                    if (vox_stack[i] < 0) { vox_stack[i] = note_on(midi + (i ? 7 : 12), VOX, 4); note_glide(vox_stack[i], glide_ms); }
                    else { note_glide(vox_stack[i], glide_ms); note_pitch(vox_stack[i], (float)(midi + (i ? 7 : 12))); }
                }
            } else for (int i = 0; i < 2; i++) if (vox_stack[i] >= 0) { note_off(vox_stack[i]); vox_stack[i] = -1; }
        } else if (vox_h >= 0) {
            note_off(vox_h); vox_h = -1;
            for (int i = 0; i < 2; i++) if (vox_stack[i] >= 0) { note_off(vox_stack[i]); vox_stack[i] = -1; }
        }

        if ((s & 7) == 0) {                          // re-stab the wall every half bar
            if (!muted[D_SAW] && !(hand_slot == SAW && kb_held_n > 0)) saw_chord(midi);
            else saw_release();                      // your hands have the wall: get out of the way
        }
    }
}

// the STAB pad: the chord the pattern would play RIGHT NOW, not a fixed one. v2 hardcoded RIFF[0],
// so tapping it mid-bar fought the running riff instead of doubling it.
static void stab_now(void) {
    saw_chord(snap_penta(RIFF[step < 0 ? 0 : (step >> 2) & 3]));
}

static void all_off(void) {
    if (vox_h >= 0) { note_off(vox_h); vox_h = -1; }
    for (int i = 0; i < 2; i++) if (vox_stack[i] >= 0) { note_off(vox_stack[i]); vox_stack[i] = -1; }
    saw_release();
}

void update(void) {
    if (keyp(KEY_SPACE)) { playing = !playing; if (!playing) all_off(); else { clock_pos = 0.0f; step = -1; } }
    for (int i = 0; i < P_COUNT; i++)
        if (keyp('1' + i)) { preset = i; voice_macros(); }

    keybed_update();                                 // touch + mouse + QWERTY + MIDI → kb_note/kb_off

    if (playing) {                                   // our own 1/16 clock, off bpm()
        clock_pos += dt() * (float)BPM / 60.0f * 4.0f;
        while (clock_pos >= (float)STEPS) clock_pos -= (float)STEPS;
        int s = (int)clock_pos;
        if (s != step) { step = s; fire_step(s); }
    }

    // pad lights + the recorded-step highlight decay here, not in draw_drums: the pads must be
    // correct the moment you open the panel, and draw_drums only runs while it is open.
    for (int l = 0; l < LANES; l++) if (pad_glow[l] > 0.0f) pad_glow[l] -= dt() * 4.0f;
    if (rec_flash_t > 0.0f) rec_flash_t -= dt() * 5.0f;

    if (riser_t >= 0.0f) {                           // a rising sweep, then gone
        riser_t += dt() * 0.8f;
        if (riser_h >= 0) {
            note_pitch(riser_h, 48.0f + riser_t * 40.0f);
            note_cutoff(riser_h, (int)(600 + riser_t * 6000));
        }
        if (riser_t >= 1.0f) { if (riser_h >= 0) note_off(riser_h); riser_h = -1; riser_t = -1.0f; }
    }

    apply_fx(0);                                     // change-detected, never per-frame reconfigure

#ifdef DE_TRACE
    watch("step",  "%d", step);
    watch("focus", "%d", focus);
    watch("held",  "%d", kb_held_n);
    watch("hands", "%d", hand_slot);                 // VOX(6) or SAW(5) — the keybed's target
    { int n = 0; for (int l = 0; l < LANES; l++) for (int s = 0; s < STEPS; s++) if (grid[l][s]) n++;
      watch("gridn", "%d", n); }                     // live step count: proves a REC'd pad tap landed
    watch("rec",   "%d", rec);
    watch("saw0",  "%d", saw_h[0]);                  // the pattern's wall voice; -1 = it stepped aside
    watch("up",    "%.2f", k_up);
    watch("low",   "%.2f", k_low);
    watch("retune_ms", "%d", (int)(k_retune * 120.0f));
#endif
}

// ── drawing ───────────────────────────────────────────────────────────────────────────────────
// captions ride the 4x6 font: at 8x8 a 320px screen only fits 40 characters, and ui-audit caught
// several off-screen/colliding strings on v1's first pass. text_width() measures, never eyeballs.
static void caption(const char *t, int x, int y, int c) { font(FONT_SMALL); print(t, x, y, c); font(FONT_NORMAL); }
static void caption_c(const char *t, int cx, int y, int c) { font(FONT_SMALL); print(t, cx - text_width(t) / 2, y, c); font(FONT_NORMAL); }
// right-aligned, measured with text_width() so a reworded string cannot silently run off the edge
static void caption_r(const char *t, int right, int y, int c) { font(FONT_SMALL); print(t, right - text_width(t), y, c); font(FONT_NORMAL); }

static int tapped(int x, int y, int w, int h) {
    return tapp(x, y, w, h) || (mouse_pressed(MOUSE_LEFT) &&
        mouse_x() >= x && mouse_x() < x + w && mouse_y() >= y && mouse_y() < y + h);
}

// a device's title row: name, a live activity pip, and its MUTE. Tapping the row focuses the device;
// tapping MUTE never changes focus, so you can silence a box without losing your place.
static void device_head(int d, int y, int h, int open) {
    int c = D_HUE[d];
    rectfill(0, y, SCREEN_W, h, muted[d] ? CLR_DARKER_GREY : CLR_BLACK);
    rect(0, y, SCREEN_W, h, open ? c : CLR_DARK_GREY);
    font(FONT_NORMAL);
    print(D_NAME[d], 5, y + (h - 8) / 2, muted[d] ? CLR_DARK_GREY : c);
    if (!open) {                                     // slim strips stay alive: a 16-tick mini pattern
        for (int s = 0; s < STEPS; s++) {
            int on = d == D_DRUM ? (grid[0][s] || grid[1][s] || grid[2][s] || grid[3][s])
                                 : ((s & 3) == 0);
            int px = 150 + s * 6, py = y + h / 2 - 2;
            rectfill(px, py, 4, 4, s == step ? CLR_WHITE : on ? D_HUE[d] : CLR_DARKER_GREY);
        }
    }
    int mx = SCREEN_W - 34;
    if (tapped(mx, y + 2, 32, h - 4)) { muted[d] = !muted[d]; if (muted[d] && d == D_VOICE) all_off(); }
    rect(mx, y + 2, 32, h - 4, muted[d] ? CLR_RED : CLR_DARK_GREY);
    caption_c(muted[d] ? "MUTED" : "MUTE", mx + 16, y + h / 2 - 3, muted[d] ? CLR_RED : CLR_DARK_GREY);
    // Opening a melodic box hands it the keybed. Opening drums/master leaves the keybed pointed
    // where it was, so a plugged-in MIDI keyboard keeps playing while you program a pattern.
    if (tapped(0, y, mx - 2, h)) {
        focus = d;
        if (d == D_VOICE)     kb_point(VOX, 4, 14);
        else if (d == D_SAW)  kb_point(SAW, 3, 14);   // the wall wants a lower register than the chipmunk lead
    }
}

static void draw_voice(int y, int h) {
    if (ui_knob_at(&k_retune, 24, y + 24, 10, 0)) { }
    caption_c("RETUNE", 24, y + 37, CLR_PINK);
    caption_c(k_retune < 0.34f ? "hard" : k_retune < 0.67f ? "harder" : "hardest", 24, y + 44, CLR_PINK);
    for (int i = 0; i < P_COUNT; i++)
        if (ui_button(58 + i * 56, y + 8, 52, 18, P_NAME[i])) { preset = i; voice_macros(); }
    caption(kb_held_n ? "your hands have it - the riff stepped aside" : "play it: touch / mouse / QWERTY / MIDI",
            58, y + 30, kb_held_n ? CLR_PINK : CLR_DARK_GREY);
    caption("Z X octave", 58, y + 38, CLR_DARK_GREY);
    int ky = y + 52;   // 52, not 48: RETUNE's value word sits at y+44 and FONT_SMALL is 6px tall, so
                       // at 48 the keys clipped its descenders — the same 5px/6px budgeting as the captions
    keybed_layout(2, ky, SCREEN_W - 4, h - (ky - y) - 3);
    keybed_draw();
}

static void draw_saw(int y, int h) {
    float *kk[3]; kk[0] = &k_detune; kk[1] = &k_bright; kk[2] = &k_toy;
    static const char *kn[3] = { "DETUNE", "BRIGHT", "TOY" };
    for (int i = 0; i < 3; i++) {
        int x = 26 + i * 46;
        if (ui_knob_at(kk[i], x, y + 18, 10, 0)) {
            if (i == 0) instrument_unison_detune(SAW, 0.10f + k_detune * 0.60f);
            if (i == 1) instrument_filter(SAW, FILTER_LOW, (int)(300 + k_bright * 5200), 4);
            if (i == 2) apply_fx(1);                 // crush lives in the change-detector
        }
        caption_c(kn[i], x, y + 30, CLR_BLUE);
    }
    if (ui_button(148, y + 6, 44, 22, "STAB")) stab_now();
    // the wall, drawn: seven saws, spread by DETUNE, brightness by BRIGHT
    for (int i = 0; i < 7; i++) {
        int x = 200 + i * 13, spread = (int)(k_detune * 5.0f);
        int hh = (int)(6 + k_bright * 12.0f);
        int c = k_toy > 0.5f ? CLR_GREEN : CLR_BLUE;
        for (int j = 0; j < 3; j++) line(x + j * 4 + (i - 3) * spread / 3, y + 30,
                                        x + j * 4 + 3 + (i - 3) * spread / 3, y + 30 - hh, c);
    }
    // NOTE ON CAPTION WIDTH: FONT_SMALL advances 5px per char, not 4 ("~64 chars across 320px"), which
    // is how v2's first pass put four captions off the right edge. ui-audit missed them because it only
    // ever saw the DEFAULT panel — a focus-model cart needs --explore, or it audits one quarter of itself.
    caption("7 saws, 1 key", 200, y + 32, CLR_DARK_GREY);
    // kept short on purpose: the wall drawing's caption sits one row up at x=200, and a 41-char
    // string here ran under it. text_width() says 5px/char — budget the row, don't eyeball it.
    caption(kb_held_n ? "the wall is yours" : "one key = all 7 saws",
            26, y + 38, kb_held_n ? CLR_BLUE : CLR_DARK_GREY);
    int ky = y + 46;                                 // the same keybed, re-pointed at the saw slot
    keybed_layout(2, ky, SCREEN_W - 4, h - (ky - y) - 3);
    keybed_draw();
}

static void draw_drums(int y, int h) {
    int x0 = 26, cw = (SCREEN_W - x0 - 6) / STEPS, top = y + 4;
    int lh = (h - 14) / LANES; if (lh > 20) lh = 20;
    for (int l = 0; l < LANES; l++) {
        int ly = top + l * lh;
        caption(LANE_NAME[l], 6, ly + lh / 2 - 3, CLR_RED);
        for (int s = 0; s < STEPS; s++) {
            int cx = x0 + s * cw, ch = lh - 3;
            int st = grid[l][s];
            int col = st == ST_HIT ? CLR_RED : st == ST_RATCHET ? CLR_ORANGE : CLR_DARKER_GREY;
            rectfill(cx, ly, cw - 2, ch, col);
            if (st == ST_RATCHET)                    // three ticks = the 1/32 burst
                for (int t = 0; t < 3; t++) rectfill(cx + 2 + t * 4, ly + 2, 2, 2, CLR_WHITE);
            if (s == step) rect(cx, ly, cw - 2, ch, CLR_WHITE);
            // a step the pad row just wrote: outlined for a fifth of a second, because REC QUANTIZES
            // and you need to see WHERE your tap landed, not just hear that it did
            if (s == rec_flash && rec_flash_t > 0.0f) rect(cx - 1, ly - 1, cw, ch + 2, CLR_WHITE);
            if (tapped(cx, ly, cw - 2, ch)) grid[l][s] = (unsigned char)((st + 1) % 3);
        }
    }

    // ── the pad row: the drums were the one box you could only PROGRAM ────────────────────────────
    // Nearest-step quantize, not floor: a hand tap lands a few ms LATE by definition, so writing to
    // (int)clock_pos puts every recorded hit one step behind where you heard yourself play it.
    int py = top + LANES * lh + 2, ph = 18;
    int q = playing ? (int)(clock_pos + 0.5f) % STEPS : -1;
    for (int l = 0; l < LANES; l++) {
        int px = x0 + l * 46, pw = 44;
        int lit = pad_glow[l] > 0.25f;
        rectfill(px, py, pw, ph, lit ? CLR_RED : CLR_BLACK);
        rect(px, py, pw, ph, pad_glow[l] > 0.02f ? CLR_ORANGE : CLR_DARK_GREY);
        font(FONT_NORMAL);
        print(LANE_NAME[l], px + pw / 2 - 8, py + ph / 2 - 4, lit ? CLR_BLACK : CLR_RED);
        if (tapped(px, py, pw, ph)) {
            hit_lane(l, 0, STEP_MS);
            if (rec && q >= 0) { grid[l][q] = ST_HIT; rec_flash = q; rec_flash_t = 1.0f; }
        }
    }
    // REC is hand-rolled, not ui_button, for the same reason MUTE is: it has to read ARMED (red) at a
    // glance, and its light blinks on the beat so an armed rack never looks idle.
    int rx = x0 + LANES * 46 + 6;
    if (tapped(rx, py, 46, ph)) rec = !rec;
    rect(rx, py, 46, ph, rec ? CLR_RED : CLR_DARK_GREY);
    if (rec && (step & 2) == 0) circfill(rx + 9, py + ph / 2, 3, CLR_RED);
    caption(rec ? "REC" : "rec", rx + 17, py + ph / 2 - 3, rec ? CLR_RED : CLR_DARK_GREY);

    caption(rec ? "armed: play the pads, they land on the nearest step"
                : "tap a step: off - hit - RATCHET. or play the pads",
            x0, y + h - 8, rec ? CLR_RED : CLR_DARK_GREY);
}

static void draw_master(int y, int h) {
    float *bk[4]; bk[0] = &k_up; bk[1] = &k_low; bk[2] = &k_mid; bk[3] = &k_high;
    static const char *bn[4] = { "UP", "LOW", "MID", "HI" };
    for (int b = 0; b < 4; b++) {
        int x = 24 + b * 34;
        if (ui_knob_at(bk[b], x, y + 20, 10, 0)) apply_fx(1);
        caption_c(bn[b], x, y + 33, b ? CLR_DARK_GREY : CLR_YELLOW);
    }
    // The three bands, drawn BIG: this is the one picture that shows what the box actually does, and it
    // is driven entirely by real values (no meter — the engine exposes no master output level, and a
    // fake one would be the exact defect v1 was pulled up for).
    int by = y + 42, bh = h - 58;   // -58 leaves the band labels room: at -52 they clipped the bottom edge
    for (int b = 0; b < 3; b++) {
        float v = b == 0 ? k_low : b == 1 ? k_mid : k_high;
        int x = 22 + b * 36, w = 28;
        rect(x, by, w, bh, CLR_DARKER_GREY);
        int down = (int)(v * (float)(bh - 4));
        rectfill(x + 1, by + 1, w - 2, down, CLR_YELLOW);                    // squashed DOWN from above
        int lift = (int)(k_up * (float)(bh - 6));
        rectfill(x + 1, by + bh - 2 - lift, w - 2, 2, CLR_WHITE);            // the quiet detail, pushed UP
        caption_c(bn[b + 1], x + w / 2, by + bh + 2, CLR_DARK_GREY);
    }
    caption("down", 132, by + 2, CLR_YELLOW);
    caption("up", 132, by + bh - 8, CLR_WHITE);
    // the chain, in the order fx_order pins it. Both stages are on, and there is nothing to switch off.
    if (ui_button(190, y + 8, 60, 22, "RISER") && riser_t < 0.0f) { riser_t = 0.0f; riser_h = note_on(48, RISER, 5); }
    rect(190, by, 124, 16, CLR_YELLOW);
    caption("MULTIBAND", 196, by + 5, CLR_YELLOW);
    caption("always on", 262, by + 5, CLR_DARK_GREY);
    rect(190, by + 20, 124, 16, CLR_YELLOW);
    caption("HARD CLIP", 196, by + 25, CLR_YELLOW);
    caption("always on", 262, by + 25, CLR_DARK_GREY);
    caption("no bypass", 190, by + 42, CLR_YELLOW);
    caption("that is the point", 190, by + 50, CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_BLACK);

    // ── transport (always reachable, whatever is open) ────────────────────────────────────────
    rectfill(0, 0, SCREEN_W, TP_H, CLR_DARKER_GREY);
    if (ui_button(2, 1, 44, TP_H - 2, playing ? "STOP" : "PLAY")) {
        playing = !playing;
        if (!playing) all_off(); else { clock_pos = 0.0f; step = -1; }
    }
    caption("160", 50, 4, CLR_LIGHT_GREY);
    for (int s = 0; s < STEPS; s++)                  // the beat, so the screen is never still
        rectfill(70 + s * 7, 5, 5, 4, s == step ? CLR_WHITE : (s & 3) ? CLR_DARK_GREY : CLR_MEDIUM_GREY);
    caption_c(P_NAME[preset], 198, 4, CLR_PINK);
    // where your hands / a MIDI keyboard are pointed. It follows focus, so with the drums open this
    // is the only thing that says which box QWERTY will play.
    caption(hand_slot == VOX ? "keys VOICE" : "keys SAW", 220, 4, hand_slot == VOX ? CLR_PINK : CLR_BLUE);
    caption_r("too loud", SCREEN_W - 4, 4, CLR_YELLOW);

    // ── the accordion ────────────────────────────────────────────────────────────────────────
    int y = TP_H;
    for (int d = 0; d < D_N; d++) {
        int open = d == focus;
        int h = open ? BODY_H : STRIP_H;
        device_head(d, y, open ? 16 : h, open);
        if (open) {
            int by = y + 16, bh = h - 16;
            rect(0, y, SCREEN_W, h, D_HUE[d]);
            if (d == D_VOICE)       draw_voice(by, bh);
            else if (d == D_SAW)    draw_saw(by, bh);
            else if (d == D_DRUM)   draw_drums(by, bh);
            else                    draw_master(by, bh);
        }
        y += h;
    }

    // Retire the keybed's hit rect when neither melodic box is open. keybed_layout() is STICKY and
    // keybed_update() hit-tests it whether or not anything drew keys — so v2's rect stayed live under
    // the drum grid, and tapping a step also played a note. QWERTY/MIDI don't use the rect, so a
    // plugged-in keyboard still works while you're editing drums; only the phantom keys go away.
    if (focus != D_VOICE && focus != D_SAW) keybed_layout(0, 0, 0, 0);

    cursor_draw(CUR_ARROW);
}
