/* de:meta
{
  "title": "motionbox",
  "status": "active",
  "created": "2026-07-09",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "genre": null,
  "teaches": [
    "step-sequencer",
    "motion-sequencing"
  ],
  "homage": "Korg Electribe EMX-1",
  "todo": [
    "Slice 4 - per-part LENGTH / polymeter: give each part its own last-step (e.g. hat loops every 16, bass every 12) so they drift against each other. One number per part, pure emergent groove - the authentic EMX per-part last-step.",
    "Slice 5 - PATTERN SLOTS (A/B/C/D) + chaining: a few patterns you flip between live (and chain into a song) - real arrangement / 'more than one bar'. The EMX pattern is the loop unit; the song is chained patterns.",
    "Slice 6 - a tempo-synced DELAY you can MOTION-RECORD (the dub 'throw' that smears the whole groove into the distance) + per-part FX sends. The engine already has echo/reverb; the win is making the send knob motion-able.",
    "ACCENT part / per-step accent lane - a shared accent row that boosts velocity on chosen steps for groove dynamics across the whole kit.",
    "ROLL / ratchet - hold a step to get step-repeats/rolls; pairs with SWING for feel.",
    "Per-LANE SMOOTH/TRIG (currently one global toggle), per-lane CLEAR (currently clears the whole selected part), and motion UNDO. Natural add: long-press a knob to clear just its lane.",
    "MONO GLIDE for BASS + LEAD (was a slice-2 open-Q): a continuous glide so the mono voices SLIDE between notes instead of stepping - needs a held-note handle via note_on + note_glide, not fire-and-forget hit() (that's the real work). The OSC page filled up with WAVE/ATK/TUNE/PWM, so glide needs a 4th page or a repurposed knob. A subtle fixed amount is a free stopgap. Decide depth by ear.",
    "Melodic control for LEAD (currently a fixed 5-note cascade): a pitch lane + scale/root select so the topline is playable, not canned.",
    "Variable pattern LENGTH + resolution (16/32/triplet beats) with paging through >16 steps - the EMX 'beat + length', beyond the single 16-step bar.",
    "Portrait / device-adaptive phone layout (lay.h + docs/guides/responsive-instrument-ui.md) - it's currently landscape 320x200, but a phone is the target device for the grab-and-wiggle gesture.",
    "SAVE / RECALL patterns (persistence via save_bytes) - the EMX stores its patterns; motionbox forgets everything on quit. Serialize each part's trig[] + lanes + base + the mutes/tempo/swing/drive, load on init. The biggest hole: it's what makes Slice 5's pattern slots actually meaningful rather than ephemeral.",
    "Per-part LEVEL + PAN - the EMX has a per-part level fader and pan (both motion-able); right now there's no way to BALANCE the kit or use stereo at all. instrument_pan exists; PAN is great motion-fodder (auto-panned hats). Fits scarcity - two more per-part params the knob row can remap to.",
    "REAL-TIME recording - the EMX's other input method beyond step-toggle: tap the ribbon in time and it captures + quantizes the hits to the nearest step. A whole authentic workflow that's absent; pairs with the existing motion REC (play a groove live, then wiggle the knobs over it).",
    "OSC PAGE - DONE (2026-07-09): the 3rd knob-row page = WAVE (oscillator model SIN/TRI/SAW/SQR/NOI/FM, live readout) / ATK (amp attack 0..300ms) / TUNE (+-12 semitone transpose) / PWM (pulse width), all set-and-hold via apply_part_osc, no new surface. Verified: WAVE saw->sine collapses the even harmonics ~37dB while the bass drive survives (SR_INSTR only touches wave+timings). REMAINING (CART-side, NOT engine work - see the UNISON/CHORD/SYNC todo): true UNISON (the mt70 multi-slot detune stack) + CHORD (the built-in chord()) + SYNC (instrument_sync) - a part owns one slot today, so these want a slot pool.",
    "Deeper voice knobs still wanting SPACE (all 3 pages full at 4 knobs each - needs a 4th page or a repurpose): a multimode filter select (LP/HP/BP/BP+), an LFO SHAPE knob (lfo_shape gives tri/square/S&H/random; currently sine), and a filter-EG ATTACK (currently fixed 3ms - the OSC ATK is the AMP attack, a different envelope).",
    "UNISON / CHORD / SYNC for the lead - NOT engine work (an earlier todo wrongly said 'no primitive'; there is): chord-from-one-note is the built-in chord(root, CHORD_*, instr, vol) (9 types); a fat supersaw is the 'mt70 MULTI-SLOT TRICK' - fire the same note on a small POOL of extra slots, each instrument_tune'd a few cents apart, summed (moog.c is the worked reference, zero engine code); and instrument_sync(slot, ratio) is a one-call hard-sync fat lead. The real cost is a CART refactor: a part owns ONE slot today, so a unison lead needs a slot pool + firing the note across it. Do after save/recall.",
    "Per-PAGE motion indicator (surfaced by the page axis): the part tab lights a green dot if the part has motion on ANY page, but you can't see WHICH page - so on TONE you see the dot yet flat lanes ('where's my motion?'). Put a small dot on each TONE/MOD/OSC tab that carries recorded motion for the selected part. Small, pure legibility.",
    "Universal knob VALUE readout: the quantized knobs (RATE/DEST/WAVE) now read out, but the continuous ones (DEPTH, EG>CUT, ATK, TUNE, PWM, all the TONE knobs) only show a pointer angle - precise dialing is guesswork. Show a brief value (or a small bar) while a knob is held. Completes the MOD/OSC legibility pass.",
    "Part / page COPY (the EMX has copy): with 3 pages x 4 knobs of state per part, dialing a second part from scratch is tedious. Copy a whole part, or one page (e.g. paste the lead's MOD onto the bass). Pairs with SAVE/RECALL and the pattern slots."
  ],
  "description": {
    "summary": "The Korg-Electribe MOTION SEQUENCE: a 4-part kit (kick/hat/bass/lead) loops on a 16-step ribbon, and ONE row of four knobs REMAPS to whichever part you select - that scarcity (one row means four things) is what makes an EMX feel deep with so few controls. While it plays you GRAB a knob and wiggle it; the wiggle is recorded per-step into that part's motion lane and plays back locked to the bar, layered per knob, per part.",
    "detail": "Two ideas make the whole instrument. SCARCITY: select a part and the ribbon + the one knob row + the knob LABELS all remap to it (kick shows TUNE/DECAY/DRIVE/ACC; bass shows CUTOFF/RES/DECAY/ACC). MOTION: while REC is armed, dragging a knob writes its value into that part's 16-step lane at the playhead, replayed forever. Motion is sampled PER-HIT (each note reads its lanes as it fires) so every part gets its own automation without fighting one master filter. SMOOTH interpolates the lane between steps; TRIG holds per step. The mini-strip under each knob is the selected part's recorded motion with the playhead. CLEAR wipes the selected part's motion.",
    "controls": "TAP a part to select it (its ribbon + knob row appear); RE-TAP the selected part to MUTE it (drop it in/out live). TAP a PAGE (TONE / MOD / OSC) to remap the SAME 4 knobs to a different bank - TONE = the kit, MOD = the modulation (LFO RATE/DEPTH/DEST + filter EG>CUT), OSC = the oscillator (WAVE model / ATK attack / TUNE transpose / PWM pulse-width). TAP a step to toggle it. GRAB a knob and turn it - if REC is armed the turn is recorded as motion for the selected part on the current page. REC arms recording. SMOOTH/TRIG flips playback. CLEAR wipes the current page's motion. The MASTER row: DRIVE = tube saturation over the whole mix, SWING = lag the off-beat 16ths. Left/Right arrows = tempo."
  }
}
de:meta */
#include "studio.h"
#include "pointer.h"     // multi-finger pool for the step ribbon
#include "ui.h"
#include <math.h>

// MOTIONBOX — the Korg-EMX "motion sequence" + its other soul, SCARCITY.
//
// Slice 1 proved the motion gesture on one part. This slice adds the second EMX pillar:
// ONE knob row that REMAPS to the selected part. There are still only four knobs and one
// ribbon in front of you, but they mean different things per part — that scarcity is why
// an EMX feels deep with so few controls. Now you can LAYER motion across the kit: sweep
// the bass filter, select the hat, record a decay stutter, select the kick, dip its tune.
//
// Data model (the design's Part[]): each part owns its own trig[] ribbon + four Params,
// and a Param is { base, lane[16], motion }. Recording is still one line (see the ui_knob
// block): while armed and a finger holds a knob, lane[cur_step] = the knob value — but now
// into parts[selPart]. Motion is applied PER-HIT: apply_part_hit() samples each lane as the
// note fires (via hit() args + a cheap per-hit instrument_filter/drive), so four parts each
// carry their own automation with no master-filter fight.
//
//   TAP a part      select it — the ribbon + knob row + labels remap to it
//   RE-TAP a part   mute it (drop it in/out of the mix live — the EMX part mute)
//   TAP a PAGE      TONE / MOD / OSC — remap the SAME knob row to a different bank (the page axis)
//   TAP a step      toggle it in the selected part's ribbon
//   GRAB a knob     turn it; if REC is armed the turn records as motion for that part
//   REC             arm / disarm motion recording
//   SMOOTH / TRIG   interpolate the lane (swoopy) vs hold per step (stair-step)
//   CLEAR           wipe the selected part's motion on the CURRENT page
//   DRIVE (master)  tube saturation over the whole mix — the EMX 'Valve Force' grit
//   SWING (master)  delay the off-beat 16ths for a triplet groove
//   < / >           tempo down / up
//
// Slice 3 adds the PERFORMANCE layer — live part MUTES, a master DRIVE, a SWING knob.
//
// The PAGE AXIS is the EMX's own answer to scarcity: the four knobs already remap per PART;
// now they ALSO remap per PAGE (TONE / MOD / OSC), so four knobs reach ~12 params with no new
// surface — 'one row means four things', one level deeper. TONE is the kit (per-part labels);
// MOD is one modulation bank for every part — a filter-EG depth (the lead's pluck) + a
// per-note-retriggered LFO (RATE = tempo-locked divisions / DEPTH / DEST = vibrato / cutoff
// wah / tremolo / auto-pan). RATE locks to the beat so short hits wobble WITH the groove (a
// free-running Hz barely fits a cycle in a percussive note). Applied SET-AND-HOLD
// (apply_part_mod): reconfigured only when a value changes, and motion rides it because a
// motion lane changes per step. Motion is now per (part x page x knob).
// OSC is the oscillator page — WAVE (model select) / ATK (amp attack) / TUNE (±12 transpose) /
// PWM (pulse width) — also set-and-hold (apply_part_osc): the instrument() re-issue swaps the
// wave + attack but keeps each part's decay/release AND its filter/LFO/EG (SR_INSTR touches only
// wave + amp timings), so a model swap never wipes the MOD-page modulation.

#define STEPS   16
#define NK      4     // knobs per row
#define NPARTS  4
#define NPAGES  3     // knob-row PAGES: the SECOND remap axis (TONE / MOD / OSC) — scarcity, one level deeper

// instrument slots + part identity
enum { KICK, HAT, BASS, LEAD };
enum { SL_KICK = 5, SL_HAT, SL_BASS, SL_LEAD };
enum { PG_TONE, PG_MOD, PG_OSC };                       // the knob-row pages
static const int  SLOT [NPARTS] = { SL_KICK, SL_HAT, SL_BASS, SL_LEAD };
static const char *PNAME[NPARTS] = { "KICK", "HAT", "BASS", "LEAD" };
static const int  PCOL [NPARTS] = { CLR_RED, CLR_YELLOW, CLR_BLUE, CLR_GREEN };
static const char *PGNAME[NPAGES] = { "TONE", "MOD", "OSC" };
static int  curPage = PG_TONE;

// the OSC page: pick the oscillator MODEL + shape it (all set-and-hold, no firing-path change).
static const int   WAVES [6] = { INSTR_SINE, INSTR_TRI, INSTR_SAW, INSTR_SQUARE, INSTR_NOISE, INSTR_FM };
static const char *WAVENM[6] = { "SIN", "TRI", "SAW", "SQR", "NOI", "FM" };
static const char *OSCLABEL[NK] = { "WAVE", "ATK", "TUNE", "PWM" };   // MODEL / amp attack / transpose / pulse-width
// each part's amp-envelope timings from init() — re-issued verbatim when WAVE/ATK change so a
// model swap keeps the part's decay/release (instrument() sets ONLY wave+timings; filter/LFO/EG survive).
static const int   RECIPE[NPARTS][3] = {   // decay_ms, sustain, release_ms (attack comes from the ATK knob)
    { 280, 0, 60 },   // KICK
    {  40, 0, 30 },   // HAT
    { 240, 0, 180 },  // BASS
    {  90, 0, 220 },  // LEAD
};

// the one knob row REMAPS per (part x page). TONE labels are per-part (the original scarcity);
// the MOD page is one modulation bank (LFO + filter-EG depth) shared by every part.
static const char *KLABEL[NPARTS][NK] = {
    { "TUNE", "DECAY", "DRIVE", "ACC" },   // KICK  TONE
    { "TONE", "DECAY", "RES",   "ACC" },   // HAT   TONE
    { "CUTOFF", "RES", "DECAY", "ACC" },   // BASS  TONE
    { "CUTOFF", "RES", "DECAY", "ACC" },   // LEAD  TONE
};
static const char *MODLABEL[NK] = { "RATE", "DEPTH", "DEST", "EG>CUT" };   // the MOD page (all parts)
static const char *RATEDIV[5] = { "1/2", "1/4", "1/8", "1/16", "1/32" };   // RATE knob → the shown division
static const char *DESTNM [4] = { "VIB", "WAH", "TREM", "PAN" };           // DEST knob → the shown target
static const int   LFO_DESTS[4] = { LFO_PITCH, LFO_CUTOFF, LFO_VOLUME, LFO_PAN };  // MOD DEST knob → dest
static const float LFO_DIV[5]   = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };  // MOD RATE knob → LFO cycles PER BEAT (tempo-locked)

// a Param = one knob's motion-sequence state
typedef struct {
    float base;              // value when the lane is off
    float live;              // the knob's backing float (stable addr = its ui identity)
    float lane[STEPS];       // recorded motion, one value per step
    bool  motion;            // is this lane live?
} Param;

typedef struct {
    bool  trig[STEPS];       // the step ribbon
    Param k[NPAGES][NK];     // four knobs PER PAGE — the page axis (motion is per part x page x knob)
} Part;

static Part parts[NPARTS];
static int  selPart = BASS;  // start on the bass — the sweep sings

// default kit patterns + tasteful starting knob positions
static const char *PRESET[NPARTS] = {
    "x...x...x...x...",   // KICK  four-on-the-floor
    "..x...x...x...x.",   // HAT   off-beat tick
    "x.x.x.x.x.x.x.x.",   // BASS  rolling 8ths
    "....x.......x...",   // LEAD  a sparse topline
};
// per-page starting positions. TONE = the old kit; MOD = { RATE, DEPTH, DEST, EG>CUT } with
// DEPTH 0 (LFO off) so nothing moves until dialed; LEAD keeps its pluck (EG>CUT 0.5).
// TONE = the old kit; MOD = {RATE,DEPTH,DEST,EG>CUT} (DEPTH 0 = LFO off); OSC = {WAVE,ATK,TUNE,PWM}
// with WAVE defaulted to each part's init model, TUNE 0.5 = centre (no detune), so the defaults are silent.
static const float KBASE[NPARTS][NPAGES][NK] = {
    { { 0.45f, 0.35f, 0.30f, 0.70f }, { 0.50f, 0.00f, 0.00f, 0.00f }, { 0.00f, 0.00f, 0.50f, 0.50f } },   // KICK (WAVE SIN)
    { { 0.60f, 0.25f, 0.20f, 0.55f }, { 0.50f, 0.00f, 0.00f, 0.00f }, { 0.75f, 0.00f, 0.50f, 0.50f } },   // HAT  (WAVE NOI)
    { { 0.55f, 0.30f, 0.45f, 0.60f }, { 0.50f, 0.00f, 0.00f, 0.35f }, { 0.40f, 0.00f, 0.50f, 0.50f } },   // BASS (WAVE SAW; EG>CUT 0.35 = acid pluck)
    { { 0.65f, 0.25f, 0.50f, 0.55f }, { 0.50f, 0.00f, 0.00f, 0.50f }, { 0.40f, 0.00f, 0.50f, 0.50f } },   // LEAD (WAVE SAW; EG>CUT 0.5 = pluck)
};

static bool  smooth = true;   // SMOOTH (lerp between steps) vs TRIG (hold per step)
static bool  rec    = false;  // motion-record armed?

static int   cur_step = 0, last_16 = -1;
static float g_phase  = 0;    // continuous loop position 0..16 (for SMOOTH interpolation)
static int   tempo    = 124;
static int   flash[NPARTS];   // frame() each part last fired
static int   leadArp  = 0;    // cascading-lead position

// slice 3 — the performance layer (global, not per-part)
static bool  mute[NPARTS] = { false, false, false, false };  // live part mutes (drop in/out)
static float masterDrive  = 0.0f;   // master tube saturation over the summed bus ('Valve Force')
static float swing        = 0.0f;   // groove: lag the off-beat 16ths (0 = straight)

// per-finger ribbon paint
typedef struct { int id, paint, last; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];

// sticky widget-ownership (ported from dubdesk): once a finger is captured by a ui knob/
// button it stays "owned" until it LIFTS, so a knob drag that wanders up into the ribbon
// never toggles a step. ui_captured() alone can drop for a frame mid-drag, so we latch it.
static int  own_ids[8], own_n = 0;
static bool is_owned(int id) { for (int i = 0; i < own_n; i++) if (own_ids[i] == id) return true; return false; }
static void own(int id)      { if (!is_owned(id) && own_n < 8) own_ids[own_n++] = id; }
static void unown(int id)    { for (int i = 0; i < own_n; i++) if (own_ids[i] == id) { own_ids[i] = own_ids[--own_n]; return; } }

// ── layout ──
#define GX 12       // ribbon left
#define GY 40       // ribbon top
#define SX 19       // step stride
#define CW 16
#define CH 24
static const int kx[NK] = { 44, 124, 204, 284 };   // knob centres
#define KY  100
#define LSX 24      // lane strip half-width
#define LSY 120     // lane strip top
#define LSH 14
#define MKY 149     // master-knob row (DRIVE / SWING) centre
static const int mkx[2] = { 248, 294 };   // DRIVE, SWING knob centres

// the value a param is CURRENTLY producing — from its lane if motion is live, else base
static float cur_value(Param *p) {
    if (!p->motion) return p->base;
    if (!smooth)    return p->lane[cur_step];
    int   a = (int)g_phase % STEPS;
    int   b = (a + 1) % STEPS;
    float f = g_phase - floorf(g_phase);
    return p->lane[a] + (p->lane[b] - p->lane[a]) * f;
}

static int ribbon_col(int x, int y) {
    if (y < GY || y > GY + CH) return -1;
    int c = (x - GX) / SX;
    return (c < 0 || c >= STEPS) ? -1 : c;
}

// sample a part's four lanes and fire the note — per-hit motion application.
// Most knobs map to hit() args (no reconfigure); CUTOFF/RES/TONE/DRIVE do a cheap
// per-hit instrument_filter/drive (fires ~8x/sec, well within set-and-hold budget).
static void apply_part_hit(int pi) {
    Part *P = &parts[pi];
    float v0 = cur_value(&P->k[PG_TONE][0]), v1 = cur_value(&P->k[PG_TONE][1]);
    float v2 = cur_value(&P->k[PG_TONE][2]), v3 = cur_value(&P->k[PG_TONE][3]);
    switch (pi) {
        case KICK: {   // TUNE, DECAY, DRIVE, ACC
            instrument_drive(SL_KICK, v2 * 0.6f);
            int note = 30 + (int)(v0 * 20);
            hit(note, SL_KICK, 3 + (int)(v3 * 4), 60 + (int)(v1 * 220));
        } break;
        case HAT: {    // TONE, DECAY, RES, ACC
            instrument_filter(SL_HAT, FILTER_HIGH, (int)(3000 + v0 * 9000), (int)(1 + v2 * 10));
            hit(90, SL_HAT, 2 + (int)(v3 * 4), 18 + (int)(v1 * 170));
        } break;
        case BASS: {   // CUTOFF, RES, DECAY, ACC
            int base = (int)(120 * powf(9000.0f / 120.0f, v0));         // CUTOFF knob
            // ACC also OPENS the filter — accent = louder AND brighter (parity with the lead)
            instrument_filter(SL_BASS, FILTER_LOW, base + (int)(v3 * 3000), (int)(1 + v1 * 12));
            hit(33, SL_BASS, 2 + (int)(v3 * 5), 40 + (int)(v2 * 340));   // A1 acid root
        } break;
        case LEAD: {   // CUTOFF, RES, DECAY, ACC
            static const int TOPLINE[5] = { 57, 60, 64, 67, 72 };       // A-minor topline
            int base  = (int)(200 * powf(9000.0f / 200.0f, v0));        // CUTOFF knob
            int decMs = 40 + (int)(v2 * 340);                           // DECAY knob = note length
            // ACC also OPENS the filter — accent = louder AND brighter (the EMX accent, no new knob)
            instrument_filter(SL_LEAD, FILTER_LOW, base + (int)(v3 * 4000), (int)(1 + v1 * 10));
            hit(TOPLINE[leadArp % 5], SL_LEAD, 2 + (int)(v3 * 5), decMs);   // filter-EG pluck is set-and-hold (MOD page)
            leadArp++;
        } break;
    }
    flash[pi] = frame();
}

// the MOD page → the filter-EG (pluck depth) + the LFO, applied SET-AND-HOLD: reconfigure a
// part's voice ONLY when a MOD value changes, never every frame (motion still rides it — a
// motion lane changes per step, which reconfigures then). Per-voice params, so it's outside
// the bus set-and-hold rule, but we still gate to be clean + phase-stable.
static float lastEg[NPARTS], lastRate[NPARTS], lastDepth[NPARTS];
static int   lastDec[NPARTS], lastDest[NPARTS];
static void apply_part_mod(int pi) {
    Part *P = &parts[pi];
    int   slot = SLOT[pi];
    // filter EG: a per-note cutoff sweep (the 'pew'); depth = EG>CUT knob, decay tracks TONE DECAY
    float egHz  = cur_value(&P->k[PG_MOD][3]) * 9000.0f;
    int   decMs = 40 + (int)(cur_value(&P->k[PG_TONE][2]) * 340);
    if (egHz != lastEg[pi] || decMs != lastDec[pi]) {
        instrument_env(slot, 0, ENV_CUTOFF, 3, decMs, egHz);   // amount 0 = off (a flat filter)
        lastEg[pi] = egHz; lastDec[pi] = decMs;
    }
    // LFO: RATE (tempo-locked division) / DEPTH / DEST — vibrato / cutoff wah / tremolo / auto-pan.
    // The engine retriggers the LFO per note (phase resets at note-on), so a tempo-locked rate
    // makes short hits wobble WITH the groove; fast divisions fit >1 cycle inside a short note.
    int   dest   = LFO_DESTS[(int)(cur_value(&P->k[PG_MOD][2]) * 3.999f)];
    float rate   = (tempo / 60.0f) * LFO_DIV[(int)(cur_value(&P->k[PG_MOD][0]) * 4.999f)];
    float depthK = cur_value(&P->k[PG_MOD][1]);
    float depth  = depthK == 0.0f      ? 0.0f              // 0 = off, whatever the dest
                 : dest == LFO_PITCH   ? depthK * 5.0f     // semitones — wide vibrato
                 : dest == LFO_CUTOFF  ? depthK * 6000.0f  // Hz — dramatic wah
                 :                       depthK;           // 0..1 — full tremolo / auto-pan
    if (rate != lastRate[pi] || depth != lastDepth[pi] || dest != lastDest[pi]) {
        instrument_lfo(slot, 0, dest, rate, depth);
        lastRate[pi] = rate; lastDepth[pi] = depth; lastDest[pi] = dest;
    }
}

// the OSC page → oscillator MODEL + amp attack + transpose + pulse-width, also SET-AND-HOLD.
// The instrument() re-issue keeps the part's decay/release (RECIPE) and does NOT disturb the
// filter/LFO/EG set elsewhere — SR_INSTR touches only wave + amp timings.
static int   lastWave[NPARTS], lastAtk[NPARTS];
static float lastTune[NPARTS], lastDuty[NPARTS];
static void apply_part_osc(int pi) {
    Part *P = &parts[pi];
    int   slot = SLOT[pi];
    int   wave = WAVES[(int)(cur_value(&P->k[PG_OSC][0]) * 5.999f)];   // MODEL select
    int   atk  = (int)(cur_value(&P->k[PG_OSC][1]) * 300);            // 0..300ms amp attack (soft pad ↔ pluck)
    if (wave != lastWave[pi] || atk != lastAtk[pi]) {
        instrument(slot, wave, atk, RECIPE[pi][0], RECIPE[pi][1], RECIPE[pi][2]);
        lastWave[pi] = wave; lastAtk[pi] = atk;
    }
    float tune = (cur_value(&P->k[PG_OSC][2]) - 0.5f) * 24.0f;         // ±12 semitones transpose
    if (tune != lastTune[pi]) { instrument_tune(slot, tune); lastTune[pi] = tune; }
    float duty = cur_value(&P->k[PG_OSC][3]);                          // pulse width (square only)
    if (duty != lastDuty[pi]) { instrument_duty(slot, duty); lastDuty[pi] = duty; }
}

void init() {
    // static instrument recipes — the per-hit apply only nudges filter/drive/note/vol/dur
    instrument(SL_KICK, INSTR_SINE, 0, 280, 0, 60);
    instrument_env(SL_KICK, 1, ENV_PITCH, 0, 55, 30);       // the donk
    instrument(SL_HAT, INSTR_NOISE, 0, 40, 0, 30);
    instrument_filter(SL_HAT, FILTER_HIGH, 7000, 2);
    instrument(SL_BASS, INSTR_SAW, 0, 240, 0, 180);
    instrument_filter(SL_BASS, FILTER_LOW, 16000, 1);
    instrument_drive(SL_BASS, 0.35f);                       // acid growl
    instrument(SL_LEAD, INSTR_SAW, 1, 90, 0, 220);
    instrument_filter(SL_LEAD, FILTER_LOW, 3000, 4);
    instrument_echo(SL_LEAD, 0.22f);                        // dreamy topline tail

    // place the master saturator in the mix chain — drive_insert() only SETS params;
    // FX_DRIVE isn't in the default pedal ladder, so without this the DRIVE knob is silent.
    int master_fx[] = { FX_DRIVE };
    fx_order(0, master_fx, 1);

    for (int p = 0; p < NPARTS; p++) {
        for (int c = 0; c < STEPS; c++) parts[p].trig[c] = (PRESET[p][c] == 'x');
        for (int pg = 0; pg < NPAGES; pg++)
            for (int i = 0; i < NK; i++) {
                parts[p].k[pg][i].base = KBASE[p][pg][i];
                parts[p].k[pg][i].live = KBASE[p][pg][i];
            }
        flash[p] = -99;
        lastEg[p] = lastRate[p] = lastDepth[p] = -1.0f;   // sentinels → apply_part_mod fires once
        lastDec[p] = lastDest[p] = -1;
        lastWave[p] = lastAtk[p] = -1; lastTune[p] = lastDuty[p] = -999.0f;   // apply_part_osc fires once
    }
    PTR_CLEAR(ptr);
}

void update() {
    bpm(tempo);

    // ── transport: playhead off the synth clock (SWING lags the off-beat 16ths) ──
    float pos16 = beat() * 4 + beat_pos() * 4.0f;   // monotonic loop position in 16ths
    g_phase = fmodf(pos16, STEPS);
    float s = swing * 0.6f;                         // 0 = straight … ~2/3 = hard triplet swing
    if (pos16 > last_16 + STEPS + 1) last_16 = (int)pos16 - 1;   // resync after a stall, no burst
    for (int n = last_16 + 1;                                    // fire every 16th we've crossed
         pos16 >= n + ((n & 1) ? s : 0.0f);                      // odd steps arrive late by s
         n = last_16 + 1) {
        last_16  = n;
        cur_step = ((n % STEPS) + STEPS) % STEPS;
        for (int p = 0; p < NPARTS; p++)
            if (parts[p].trig[cur_step] && !mute[p]) apply_part_hit(p);   // muted parts drop out
    }

    // MOD + OSC pages → each part's LFO/EG + oscillator (set-and-hold; motion rides it via the lanes)
    for (int p = 0; p < NPARTS; p++) { apply_part_mod(p); apply_part_osc(p); }

    // ── tempo ──
    if (btnp(1, BTN_LEFT))  tempo = max(80,  tempo - 4);
    if (btnp(1, BTN_RIGHT)) tempo = min(170, tempo + 4);

    // fingers the ui grabbed (a knob/button) stay owned until lift → the ribbon ignores them
    for (int i = 0; i < touch_count(); i++) if (ui_captured(touch_id(i))) own(touch_id(i));
    for (int i = 0; i < touch_ended_count(); i++) unown(touch_ended_id(i));

    // ── touch on the ribbon: tap a step to toggle (drag to paint) — edits the SELECTED part ──
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        if (is_owned(id)) { Ptr *op = PTR_FIND(ptr, id); if (op) op->id = PTR_NONE; continue; }  // knob/button finger — not the ribbon's
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        int c = ribbon_col(tx, ty);
        if (fresh) {
            if (c < 0) { p->id = PTR_NONE; continue; }        // outside ribbon → knobs/buttons own it
            bool nv = !parts[selPart].trig[c];
            parts[selPart].trig[c] = nv;
            *p = (Ptr){ id, nv, c };
        } else if (c >= 0 && c != p->last) {
            p->last = c; parts[selPart].trig[c] = p->paint;
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

#ifdef DE_TRACE
    watch("part", "%s", PNAME[selPart]);
    watch("step", "%d", cur_step);
    watch("rec",  "%d", rec);
#endif
}

void draw() {
    cls(CLR_BROWNISH_BLACK);

    print("MOTIONBOX", 2, 3, CLR_LIGHT_YELLOW);
    print_right(str("%d BPM", tempo), 318, 3, CLR_LIGHT_GREY);

    ui_begin();

    // ── the part selector: one knob row means whichever of these is lit ──
    font(FONT_SMALL);
    int pw = 74, px0 = 8;
    for (int p = 0; p < NPARTS; p++) {
        int bx = px0 + p * (pw + 2), by = 15, bh = 15;
        if (ui_button(bx, by, pw, bh, PNAME[p])) {
            if (p == selPart) mute[p] = !mute[p];   // re-tap the selected part → drop it in/out
            else              selPart = p;
        }
        if (p == selPart) rect(bx, by, pw, bh, PCOL[p]);
        if (mute[p]) line(bx + 3, by + bh / 2, bx + pw - 4, by + bh / 2, CLR_RED);  // struck = muted
        // motion dot — this part carries recorded motion on ANY page
        bool hasMot = false;
        for (int pg = 0; pg < NPAGES; pg++)
            for (int i = 0; i < NK; i++) if (parts[p].k[pg][i].motion) hasMot = true;
        if (hasMot) circfill(bx + pw - 6, by + 4, 2, CLR_GREEN);
    }
    font(FONT_NORMAL);

    // ── the selected part's ribbon (coloured by part) ──
    Part *SP = &parts[selPart];
    rectfill(GX + cur_step * SX, GY - 4, CW, 2, CLR_WHITE);         // playhead
    bool hot = (frame() - flash[selPart]) < 5;
    for (int c = 0; c < STEPS; c++) {
        int x = GX + c * SX, y = GY;
        if (SP->trig[c]) {
            rectfill(x, y, CW, CH, (c == cur_step && hot) ? CLR_WHITE : PCOL[selPart]);
            if (c == cur_step) rect(x, y, CW, CH, CLR_WHITE);
        } else {
            rectfill(x, y, CW, CH, c == cur_step ? CLR_DARK_GREY
                                 : c % 4 == 0    ? CLR_DARKER_BLUE : CLR_DARKER_GREY);
        }
    }

    // ── the PAGE selector: the SECOND remap axis — same 4 knobs, a different bank ──
    font(FONT_SMALL);
    for (int pg = 0; pg < NPAGES; pg++) {
        int bx = GX + pg * 48, by = 67, bw = 46, bh = 12;
        if (ui_button(bx, by, bw, bh, PGNAME[pg])) curPage = pg;
        if (pg == curPage) rect(bx, by, bw, bh, CLR_LIGHT_YELLOW);
    }
    print_right(rec ? "REC: turn a knob" : "arm REC, then turn",
                318, 69, rec ? CLR_RED : CLR_MEDIUM_GREY);

    // ── the one knob row (edits the SELECTED part on the CURRENT page) + each knob's lane ──
    for (int i = 0; i < NK; i++) {
        Param *P = &SP->k[curPage][i];
        // show the lane/base value UNLESS a finger is holding this knob
        if (!ui_cap_for(&P->live)) P->live = cur_value(P);

        // label: TONE = per-part names; MOD/OSC = names, but the QUANTIZED knobs read out live
        const char *lbl;
        if (curPage == PG_TONE)
            lbl = KLABEL[selPart][i];
        else if (curPage == PG_MOD)
            lbl = (i == 0) ? str("RATE %s", RATEDIV[(int)(P->live * 4.999f)])   // tempo division
                : (i == 2) ? str("DEST %s", DESTNM [(int)(P->live * 3.999f)])   // vibrato/wah/trem/pan
                :            MODLABEL[i];
        else /* PG_OSC */
            lbl = (i == 0) ? str("WAVE %s", WAVENM[(int)(P->live * 5.999f)])    // oscillator model
                :            OSCLABEL[i];

        ui_knob(&P->live, kx[i], KY, lbl);

        // record / set-base: the motion-sequence gesture, into the selected part
        if (ui_cap_for(&P->live)) {
            if (rec) {
                if (!P->motion) { for (int j = 0; j < STEPS; j++) P->lane[j] = P->base; P->motion = true; }
                P->lane[cur_step] = P->live;                        // ← finger writes the playhead
            } else if (!P->motion) {
                P->base = P->live;                                 // no motion yet → knob sets the static value
            }
        }

        // the motion-lane strip
        int sx = kx[i] - LSX, sw = 2 * LSX;
        rectfill(sx, LSY, sw, LSH, CLR_DARKER_GREY);
        if (P->motion) {
            for (int c = 0; c < STEPS; c++) {
                int bx = sx + c * sw / STEPS;
                int bh = 1 + (int)(P->lane[c] * (LSH - 2));
                rectfill(bx, LSY + LSH - bh, max(1, sw / STEPS - 1), bh,
                         c == cur_step ? CLR_WHITE : CLR_GREEN);
            }
        } else {
            int by = LSY + LSH - 1 - (int)(P->base * (LSH - 2));
            line(sx, by, sx + sw - 1, by, CLR_DARK_GREY);          // flat = static
        }
        rectfill(sx + cur_step * sw / STEPS, LSY, 1, LSH, CLR_LIGHT_GREY);
        rect(sx, LSY, sw, LSH, P->motion ? CLR_GREEN : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);

    // ── the master row: DRIVE (mix tube grit) + SWING (groove) — global, not per-part ──
    font(FONT_SMALL);
    print("MASTER", mkx[0] - text_width("MASTER") / 2, MKY - 15, CLR_DARK_GREY);
    static float lastDrive = -1.0f;
    ui_knob(&masterDrive, mkx[0], MKY, "DRIVE");
    if (masterDrive != lastDrive) {                 // set-and-hold: reconfigure only on change
        drive_insert(masterDrive, DRIVE_SOFT, masterDrive);   // 0 = clean bypass (byte-identical)
        lastDrive = masterDrive;
    }
    ui_knob(&swing, mkx[1], MKY, "SWING");          // read by the transport next frame
    font(FONT_NORMAL);

    // ── transport: REC · SMOOTH/TRIG · CLEAR ──
    bool blink = rec && (frame() / 15) % 2 == 0;
    if (ui_button(8, 176, 62, 16, "REC")) rec = !rec;
    if (rec) rect(8, 176, 62, 16, blink ? CLR_RED : CLR_DARK_RED);

    if (ui_button(78, 176, 78, 16, smooth ? "SMOOTH" : "TRIG")) smooth = !smooth;

    if (ui_button(164, 176, 62, 16, "CLEAR")) {                     // clears the CURRENT PAGE's lanes
        for (int i = 0; i < NK; i++) SP->k[curPage][i].motion = false;
    }
    ui_end();
}
