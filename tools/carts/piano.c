/* de:meta
{
  "slug": "piano",
  "title": "piano",
  "status": "active",
  "created": "2026-06-09",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "waveguide-synth",
    "analog-voice-modeling"
  ],
  "lineage": "INSTR_PIANO showcase and tuning rig; ported from navkit's StifKarp engine. Novel: exposes the dispersion allpass chain that adds inharmonicity (stretched upper partials) to Karplus-Strong, with six presets spanning grand piano through celesta as acceptance tests.",
  "description": "INSTR_PIANO showcase + tuning rig - the struck stiff string (StifKarp). A Karplus-Strong string PLUS a dispersion allpass chain that stretches the upper partials sharp - the inharmonic, slightly metallic shimmer your ear reads as a real piano (and as a dulcimer or clavichord) rather than a plain string - run through a grand-piano soundboard. Struck; rings down on its own, so give it a long hit(). One id covers grand / bright piano / harpsichord / dulcimer / clavichord / celesta. The three engine macros: instrument_harmonics = stiffness (0 = pure harmonic tone; 1 = stretched metallic shimmer - the dispersion depth and the number of active allpass stages), instrument_timbre = hammer (0 = soft felt, mellow; 1 = hard, bright plectrum strike - the excitation lowpass, grand to harpsichord), instrument_morph = pedal (0 = dry/damped staccato; 1 = long open sustain with the highs held, like leaning on the sustain pedal). Six HARDWARE PRESETS on the number row are the acceptance tests - if 1 grand / 2 bright / 3 harpsi / 4 dulcimer / 5 clavi / 6 celesta don't each sound like themselves, the macro mapping is wrong. A one-octave keyboard: white keys A S D F G H J K, black keys W E T Y U - play with the computer keys or click/tap. 1..6 preset, drag a slider (live, or LEFT/RIGHT pick a knob + UP/DOWN turn), SPACE chord, M autoplay on/off. Multitouch: hold a chord with one hand, drag a slider with another. And an optional LAYER (key L, off by default): a second slot ~7 cents away, darker and knockier, level-matched against the first - Reid's Synth Secrets Part 45 conclusion that a piano is two voices \"similar enough to be indistinguishable within the composite, but different enough to create a sound that is more interesting than either\", the beating standing in for the tricord coupling a single string does not have. With it off the voice is byte-identical to the single-string original. Row 2's DECAY and KNOCK sliders (double-decay depth, hammer-knock amount) are the harp-to-piano tuning controls; they were dead for a long time because the engine dropped their aux-channel indices in the setter, fixed 2026-07-29. Single-string v1 (double-string detune + prepared buzz deferred). Design + STEP-0/1: instrument-engines.md §8.8.9."
}
de:meta */
// piano — INSTR_PIANO showcase + tuning rig: a one-octave keyboard, the three engine macros,
// and six hardware presets (the acceptance tests).
//
// The struck stiff string (StifKarp, §8.8.9): a Karplus-Strong string plus a DISPERSION allpass
// chain that stretches the upper partials sharp — the inharmonic, slightly metallic shimmer your
// ear reads as a real piano (and as a dulcimer or clavichord) rather than a plain string — run
// through a grand-piano soundboard. Struck; rings down on its own. The same three 0..1 macros:
//   harmonics = stiffness (0 pure tone .. 1 stretched metallic shimmer)
//   timbre    = hammer    (0 soft felt/mellow .. 1 hard/bright plectrum)
//   morph     = pedal     (0 dry/damped staccato .. 1 long open sustain)
//
// controls: A W S E D  F T G Y H U J K  play the keyboard (white = A S D F G H J K,
//             black = W E  T Y U) — or just click/tap the keys
//           1..6  load a preset (grand/bright/harpsi/dulcimer/clavi/celesta) — the test:
//                 if "harpsichord" doesn't sound like one, the MAPPING is wrong, not the preset
//           drag a slider (auditions as you drag), or LEFT/RIGHT pick a knob + UP/DOWN turn it
//           SPACE chord   ·   M autoplay on/off ('P' is the runtime pause overlay)
//
// MULTITOUCH: every finger is its own pointer — hold a chord with one hand, drag a slider
// with another. The desktop mouse is one synthetic finger, same path.

#include "studio.h"
#define KEYBED_WHITE_KEYS "ASDFGHJK"   // one octave + the top C
#define KEYBED_BLACK_KEYS "WE TYU"
#include "keybed.h"
#include <math.h>

#define I_PNO  5
#define I_PNO_B 6                   // the LAYER slot (see pno_layer) — silent unless layering is on
// The two row-2 TUNING sliders (decay / knock) reach the engine through the per-engine aux channel.
// They were DEAD for as long as they existed, and the cause was not here — it was one bound in the engine.
// Kept written down because the shape of the bug is worth recognising again:
//
//   `instrument_mode(slot, idx, v)` guarded with `idx >= 2` in runtime/sound.h, so indices 2 and 3 were
//   dropped IN THE SETTER. Meanwhile the piano engine implements both end to end — sound_piano_start reads
//   eng_p[2] as the double-decay scale and eng_p[3] as the hammer-knock scale, they are copied to the voice
//   at note-on, and the instrument bank defaults them to 0.5 (= 1.0x). So nothing was missing and nothing
//   here was wrong: two finished engine parameters were simply unreachable, and the sliders moved nothing.
//
// Fixed 2026-07-29 (guard widened to `idx >= 4`, eng_p is four wide). It is a no-op at rest — a slider at
// 0.5 sends exactly the bank default it was already using, so the shipped voice is byte-identical
// (sha 25cb93583e73) — and only bites once someone moves one. Now proven live: sweeping idx 2 moves
// brightness 0.067 -> 0.107 -> 0.165 and the centroid 2127 -> 2419 -> 2755 Hz.
//
// TWO TRAPS THIS COST ME, both worth avoiding next time:
//   · The indices are NOT validated, so a wrong one is silently ignored. A dead control that compiles,
//     runs and looks fine is the failure mode; there is no error anywhere.
//   · My first probe "proved" idx 2 was dead by rendering byte-identical audio — but the probe set idx 2
//     and then push_knobs' own line immediately OVERWROTE it. The probe was measuring itself being
//     clobbered. When probing a value a cart also writes, replace the cart's write; do not add a second one.
#define NKEY   13                   // one octave of semitones, C..C (glow index)
#define NWHITE 8
static const char WKEY[NWHITE] = { 'A','S','D','F','G','H','J','K' };   // white-key QWERTY labels
static const char BLBL[NWHITE] = { 'W','E', 0 ,'T','Y','U', 0 , 0 };   // black-key label after white k

// row 1 = the 3 engine macros; row 2 = the TUNING controls, the harp→piano scaffolding: double-decay depth
// + hammer-knock amount (each 0..2x the voicing default at the 0.5 midpoint — LIVE since the engine guard
// fix, see the note above), plus VELO = strike velocity for the QWERTY/touch keys, where there is no MIDI
// velocity to read; it drives brightness and knock, not just loudness.
static const char *KNOB_NAME[6] = { "voicing", "hammer", "pedal", "decay", "knock", "velo" };

#define NPRESET 6
static const char *PRESET_NAME[NPRESET] = { "grand","bright","harpsi","dulcimer","clavi","celesta" };
// harmonics = voicing detent (snaps to PIANO_V[0..5]); timbre = hammer (0.5 = voicing default);
// morph = pedal. The 6 presets are the 6 voicings — the acceptance tests against navkit.
static const float PRESET[NPRESET][3] = {
    { 0.08f, 0.50f, 0.62f },   // grand
    { 0.25f, 0.55f, 0.55f },   // bright
    { 0.42f, 0.50f, 0.20f },   // harpsichord (hard plectrum, dry/short)
    { 0.58f, 0.50f, 0.50f },   // dulcimer
    { 0.75f, 0.50f, 0.30f },   // clavichord (soft, intimate)
    { 0.92f, 0.50f, 0.60f },   // celesta
};

static float amp[NKEY];        // visual key-press glow (per semitone 0..12), decays each frame
static int   sel = 0;
static int   preset = 0;
static bool  autoplay = true;
static int   apos = 0;

#define NPTR 10
#define NOID (-999)
enum { PTR_IDLE, PTR_DRAG, PTR_KEY };
typedef struct { int id, mode, k, key; } Ptr;
static Ptr   ptr[NPTR];

// keyboard geometry
#define KB_Y   44
#define KB_H   78
#define WKEYS  8                    // white keys across
#define WW     ((SCREEN_W - 20) / WKEYS)
// slider geometry
#define KNOB_W    88
#define KNOB_TOP  (SCREEN_H - 52)            // row 1 y; row 2 sits 26px below
#define KNOB_X(k) (14 + ((k) % 3) * 102)
#define KNOB_Y(k) (KNOB_TOP + ((k) < 3 ? 0 : 26))

static float knob[6] = { 0.08f, 0.50f, 0.62f, 0.5f, 0.5f, 0.70f };  // grand voicing; decay+knock 0.5 = engine default (1.0×); velo 0.7 = medium-firm

// ── LAYERING: two slots instead of one (audit §I9 / Synth Secrets plan 1.5) ────────────────────
// Part 45 ends the piano arc with what Reid calls the important secret, and it is a CART pattern, not an
// engine one: "the combination of two sounds that are similar enough to be indistinguishable within the
// composite, but different enough to create a sound that is more interesting than either of the components
// in isolation." The mechanism is beating — "the detuned harmonics … sweep in and out of phase with one
// another, reinforcing and then interfering with one another destructively, TO IMITATE THE ENERGY
// INTERACTIONS WITHIN AN ACOUSTIC PIANO." That last clause matters: the layer is standing in for the
// tricord coupling of §I3, which our single string model does not have.
//
// His role split, which is the part worth copying exactly: "Piano 1B supplies the initial thunk, while
// Piano 1A has the richer spectrum and provides more of the body … Then, towards the end of the note,
// Piano 1B dominates again (thanks to the longer Decay and Release in ENV2)." So the two layers TRADE
// PLACES across the note rather than simply stacking: B opens it, A carries the middle, B is left holding
// the tail. Both layers are the same engine on a small detune; the crossfade is done with envelopes.
//
//   0 = one slot, exactly as the cart shipped.
//   1 = the two-slot layer. Costs one extra voice per note (2 instead of 1).
static int pno_layer = 0;

// The B layer's offsets from A, all deliberately small — Reid's bar is "similar enough to be
// indistinguishable within the composite". Named so they can be swept with ab-render.
static float pno_b_detune = 0.07f;   // semitones (~7 cents): the beat rate IS the effect
static float pno_b_dark   = 0.18f;   // how much darker B's timbre sits than A's (the thunk, not the body)

static void push_knobs(void) {
    instrument_harmonics(I_PNO, knob[0]);
    instrument_timbre(I_PNO, knob[1]);
    instrument_morph(I_PNO, knob[2]);
    instrument_mode(I_PNO, MODE_PIANO_DECAY, knob[3]);   // double-decay depth (0.5 = the voicing default)
    instrument_mode(I_PNO, MODE_PIANO_KNOCK, knob[4]);   // hammer knock  (0.5 = the voicing default)

    // B tracks A's voicing but darker and knockier: it is the thunk and the tail, not the body.
    float b_tim = knob[1] - pno_b_dark; if (b_tim < 0.0f) b_tim = 0.0f;
    float b_kno = knob[4] + 0.22f;      if (b_kno > 1.0f) b_kno = 1.0f;
    // ⚠ REID'S ENV2 ROLE-TRADE IS NOT REACHABLE, and this is the honest limit of the item. He has B
    // OUTLIVE A so it is left holding the tail; measured by stem render (play.js --solo-slot), B dies
    // ~0.5s SOONER than A, and nothing available fixes it:
    //   · a longer amp release + a longer gate do NOT extend an INSTR_PIANO note — the engine's own string
    //     decay governs the ring-down, so the envelope's tail has nothing left to hold (the same lesson
    //     the 808 cymbal and the brass release both taught);
    //   · and there is NO aux param for string decay. INSTR_PIANO exposes exactly two mode indices,
    //     MODE_STRING_WEIGHT (0) and MODE_STRING_CLICK (1) — no decay control at all.
    // So this layer delivers Reid's PRIMARY mechanism (two near-identical voices beating, standing in for
    // the tricord coupling of §I3) but not his secondary one (the crossfade of roles across the note).
    // Getting that would need a per-slot string-decay aux param — an engine change, so it is Phase 3+ work
    // and deliberately not smuggled in here.
    instrument_harmonics(I_PNO_B, knob[0]);
    instrument_timbre(I_PNO_B, b_tim);
    instrument_morph(I_PNO_B, knob[2]);
    instrument_mode(I_PNO_B, MODE_PIANO_KNOCK, b_kno);   // B knocks harder — it is the thunk
    instrument_tune(I_PNO_B, pno_layer ? pno_b_detune : 0.0f);

    // Level-matched on purpose: two layers sum, so without this the layered version is simply louder and
    // wins an ear test on loudness alone (the trap 1.2 and 1.4 both had to be honest about). Trimming A
    // as well is what makes the SUM land on the single-slot peak; when layering is OFF, A is left at unity
    // so the shipped voice stays byte-identical.
    instrument_level(I_PNO,   pno_layer ? 0.74f : 1.0f);
    instrument_level(I_PNO_B, pno_layer ? 0.46f : 0.0f);
}

// gate scales with pedal (morph): dry staccato lets go fast, a held pedal rings long
static int gate_ms(void) { return 500 + (int)(knob[2] * knob[2] * 16000.0f); }

// strike one note at an absolute MIDI pitch (struck — rings down on its own via gate_ms)
static void play_midi(int midi, int vol) {
    int slot = midi - keybed_base_midi();                                  // 0..12 within the octave
    instrument_pan(I_PNO, (slot / 12.0f - 0.5f) * 0.9f);                   // gentle fixed pan-by-pitch (knob[5] is now velo)
    hit(midi, I_PNO, vol, gate_ms());
    if (pno_layer) {
        // the same note on the B layer, detuned. Its longer gate is Reid's ENV2: B outlives A, so it is
        // left holding the tail after A has gone — the second half of the role trade (§I9).
        instrument_pan(I_PNO_B, (slot / 12.0f - 0.5f) * 0.9f);
        hit(midi, I_PNO_B, vol, gate_ms() * 3 / 2);
    }
    if (slot >= 0 && slot < NKEY) amp[slot] = 1.0f;
}
static void play_key(int slot, int vol) { play_midi(keybed_base_midi() + slot, vol); }   // by octave-relative slot
// keybed.h fires this on each key press (manual-voice mode); a piano key is struck, not held.
// VELOCITY now drives TIMBRE (brightness + knock), not just loudness — play soft vs hard to hear it.
// No MIDI vel here, so the velo knob (knob[5]) sets it for QWERTY/touch keys (0..1 → vol 1..7).
void kb_strike(int midi, int vel) { (void)vel; play_midi(midi, 1 + (int)(knob[5] * 6.0f + 0.5f)); }

static void load_preset(int p) {
    preset  = p;
    knob[0] = PRESET[p][0]; knob[1] = PRESET[p][1]; knob[2] = PRESET[p][2];
    push_knobs();
}

void init(void) {
    instrument(I_PNO, INSTR_PIANO, 1, 0, 7, 2000);   // long release — never chop a ringing note
    // the B layer: same engine, LONGER release (Reid's ENV2) so it dominates the tail after A has faded
    instrument(I_PNO_B, INSTR_PIANO, 1, 0, 7, 3400);
    push_knobs();
    keybed_config(I_PNO, 4, NWHITE);                 // one octave, base C4
    keybed_layout(10, KB_Y, NWHITE * WW, KB_H);
    keybed_manage_voices(false);                     // struck (hit()), not held — WE play the note
    keybed_glissando(false);                         // a finger slide shouldn't retrigger
    keybed_on_note(kb_strike);
    for (int i = 0; i < NPTR; i++) ptr[i].id = NOID;
    bpm(96);
}

void update(void) {
    keybed_update();    // keys: touch + QWERTY (ASDF../WETYU) + MIDI + Z/X octave → kb_strike

    for (int p = 0; p < NPRESET; p++)
        if (keyp('1' + p)) { load_preset(p); play_key(0, 6); play_key(4, 5); play_key(7, 5); }

    if (keyp(KEY_LEFT))  sel = (sel + 5) % 6;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 6;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        push_knobs();
    }

    if (keyp(KEY_SPACE)) { play_key(0, 6); play_key(4, 5); play_key(7, 5); }   // a triad
    if (keyp('M')) autoplay = !autoplay;
    // L = Reid's two-slot LAYER (§I9 / Part 45). push_knobs() re-applies the levels + detune, which are
    // read live at mix, so the change lands on the next note without touching what is already ringing.
    if (keyp('L')) { pno_layer = !pno_layer; push_knobs(); }

    // touch: the keys are keybed's; this pool handles the autoplay toggle + slider drags
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) {
            if (ptr[j].id == id) { p = &ptr[j]; break; }
            if (ptr[j].id == NOID && !freeP) freeP = &ptr[j];
        }
        if (!p) {
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -1, -1 };
            if (point_in_box(tx, ty, SCREEN_W - 112, 2, 108, 12)) { autoplay = !autoplay; continue; }
            for (int k = 0; k < 6; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_Y(k) - 6, KNOB_W + 4, 18)) { p->mode = PTR_DRAG; p->k = sel = k; }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - KNOB_X(p->k)) / (float)KNOB_W, 0.0f, 1.0f);
            push_knobs();
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)
        for (int j = 0; j < NPTR; j++)
            if (ptr[j].id == touch_ended_id(i)) ptr[j].id = NOID;

    // autoplay: a gentle arpeggiated noodle
    if (autoplay && every(1)) {
        static const int seq[16] = { 0,4,7,4, 9,7,4,0, 2,5,9,5, 7,4,2,0 };
        play_key(seq[apos % 16], 5);
        if (chance(30)) schedule_hit(280, keybed_base_midi() + ((seq[apos % 16] + 7) % NKEY) + 12, I_PNO, 3, 1500);
        apos++;
    }

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
#endif
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    print("PIANO", 8, 6, CLR_WHITE);
    font(FONT_SMALL);
    print("stiff-string (StifKarp) engine", 60, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M autoplay: on" : "M autoplay: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    for (int s = 0; s < NKEY; s++) amp[s] *= 0.90f;   // decay the strike glow (per semitone)
    int nw = keybed_white_count();
    // white keys (keybed.h geometry; glow from our own amp[], indexed by semitone)
    for (int wi = 0; wi < nw; wi++) {
        int x, y, w, h; keybed_white_rect(wi, &x, &y, &w, &h);
        int slot = keybed_white_midi(wi) - keybed_base_midi();
        int c = (slot >= 0 && slot < NKEY && amp[slot] > 0.1f) ? CLR_LIGHT_YELLOW : CLR_WHITE;
        rectfill(x + 1, y, w - 2, h, c);
        rect(x, y, w, h, CLR_DARK_GREY);
        print(str("%c", WKEY[wi]), x + w / 2 - 3, y + h - 12, CLR_DARK_GREY);
    }
    // black keys on top
    for (int wi = 0; wi < nw; wi++) {
        int x, y, w, h, midi; if (!keybed_black_rect(wi, &x, &y, &w, &h, &midi)) continue;
        int slot = midi - keybed_base_midi();
        int c = (slot >= 0 && slot < NKEY && amp[slot] > 0.1f) ? CLR_ORANGE : CLR_BLACK;
        rectfill(x, y, w, h, c);
        rect(x, y, w, h, CLR_DARKER_GREY);
        if (BLBL[wi]) print(str("%c", BLBL[wi]), x + 1, y + 3, CLR_LIGHT_GREY);
    }

    // two rows of knobs: row 1 = macros, row 2 = weight / attack / width (tuning)
    for (int k = 0; k < 6; k++) {
        int x = KNOB_X(k), y = KNOB_Y(k);
        bool on = (k == sel);
        font(FONT_SMALL);
        print(KNOB_NAME[k], x, y - 8, on ? CLR_YELLOW : k < 3 ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
        font(FONT_NORMAL);
        bar(x, y, KNOB_W, 6, knob[k], on ? CLR_ORANGE : k < 3 ? CLR_BROWN : CLR_DARKER_GREY, CLR_DARKER_GREY);
        if (on) print(">", x - 9, y - 1, CLR_YELLOW);
    }

    // preset row (1..6) — the named acceptance tests
    font(FONT_TINY);
    int prx = 10;
    for (int p = 0; p < NPRESET; p++)
        prx = print(str("%d %s  ", p + 1, PRESET_NAME[p]), prx, KB_Y + KB_H + 6,
                    p == preset ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    // the footer carries the LAYER state — without it the L toggle is invisible, which makes the whole
    // A/B unverifiable for whoever is listening (plan §1: check the state is actually visible)
    print(str("A..K play  Z/X oct (C%d)  1..6 preset  drag slider  SPACE chord  L layer:%s",
              keybed_octave(), pno_layer ? "2" : "1"),
          10, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
