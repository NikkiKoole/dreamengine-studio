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
    "Open Q from slice 2: if the BASS per-hit filter feels too stepped in SMOOTH mode, give the mono bass a continuous note_cutoff glide as its one special case (needs a held note handle via note_on, not fire-and-forget hit()). Decide by ear.",
    "Melodic control for LEAD (currently a fixed 5-note cascade): a pitch lane + scale/root select so the topline is playable, not canned.",
    "Variable pattern LENGTH + resolution (16/32/triplet beats) with paging through >16 steps - the EMX 'beat + length', beyond the single 16-step bar.",
    "Portrait / device-adaptive phone layout (lay.h + docs/guides/responsive-instrument-ui.md) - it's currently landscape 320x200, but a phone is the target device for the grab-and-wiggle gesture."
  ],
  "description": {
    "summary": "The Korg-Electribe MOTION SEQUENCE: a 4-part kit (kick/hat/bass/lead) loops on a 16-step ribbon, and ONE row of four knobs REMAPS to whichever part you select - that scarcity (one row means four things) is what makes an EMX feel deep with so few controls. While it plays you GRAB a knob and wiggle it; the wiggle is recorded per-step into that part's motion lane and plays back locked to the bar, layered per knob, per part.",
    "detail": "Two ideas make the whole instrument. SCARCITY: select a part and the ribbon + the one knob row + the knob LABELS all remap to it (kick shows TUNE/DECAY/DRIVE/ACC; bass shows CUTOFF/RES/DECAY/ACC). MOTION: while REC is armed, dragging a knob writes its value into that part's 16-step lane at the playhead, replayed forever. Motion is sampled PER-HIT (each note reads its lanes as it fires) so every part gets its own automation without fighting one master filter. SMOOTH interpolates the lane between steps; TRIG holds per step. The mini-strip under each knob is the selected part's recorded motion with the playhead. CLEAR wipes the selected part's motion.",
    "controls": "TAP a part to select it (its ribbon + knob row appear); RE-TAP the selected part to MUTE it (drop it in/out live). TAP a step to toggle it. GRAB a knob and turn it - if REC is armed the turn is recorded as motion for the selected part. REC arms recording. SMOOTH/TRIG flips playback. CLEAR wipes the selected part's motion. The MASTER row: DRIVE = tube saturation over the whole mix, SWING = lag the off-beat 16ths. Left/Right arrows = tempo."
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
//   TAP a step      toggle it in the selected part's ribbon
//   GRAB a knob     turn it; if REC is armed the turn records as motion for that part
//   REC             arm / disarm motion recording
//   SMOOTH / TRIG   interpolate the lane (swoopy) vs hold per step (stair-step)
//   CLEAR           wipe the selected part's motion
//   DRIVE (master)  tube saturation over the whole mix — the EMX 'Valve Force' grit
//   SWING (master)  delay the off-beat 16ths for a triplet groove
//   < / >           tempo down / up
//
// Slice 3 adds the PERFORMANCE layer — the jump from a pattern that plays to a thing you
// jam on: live part MUTES (re-tap a part to drop it), a master DRIVE (drive_insert tube grit
// over the summed bus), and a SWING knob that lags the odd 16ths.

#define STEPS   16
#define NK      4     // knobs per part
#define NPARTS  4

// instrument slots + part identity
enum { KICK, HAT, BASS, LEAD };
enum { SL_KICK = 5, SL_HAT, SL_BASS, SL_LEAD };
static const int  SLOT [NPARTS] = { SL_KICK, SL_HAT, SL_BASS, SL_LEAD };
static const char *PNAME[NPARTS] = { "KICK", "HAT", "BASS", "LEAD" };
static const int  PCOL [NPARTS] = { CLR_RED, CLR_YELLOW, CLR_BLUE, CLR_GREEN };

// the one knob row — its four labels REMAP per selected part (scarcity = the instrument)
static const char *KLABEL[NPARTS][NK] = {
    { "TUNE", "DECAY", "DRIVE", "ACC" },   // KICK
    { "TONE", "DECAY", "RES",   "ACC" },   // HAT
    { "CUTOFF", "RES", "DECAY", "ACC" },   // BASS
    { "CUTOFF", "RES", "DECAY", "ACC" },   // LEAD
};

// a Param = one knob's motion-sequence state
typedef struct {
    float base;              // value when the lane is off
    float live;              // the knob's backing float (stable addr = its ui identity)
    float lane[STEPS];       // recorded motion, one value per step
    bool  motion;            // is this lane live?
} Param;

typedef struct {
    bool  trig[STEPS];       // the step ribbon
    Param k[NK];             // the four knobs for this part
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
static const float KBASE[NPARTS][NK] = {
    { 0.45f, 0.35f, 0.30f, 0.70f },   // KICK  tune/decay/drive/acc
    { 0.60f, 0.25f, 0.20f, 0.55f },   // HAT   tone/decay/res/acc
    { 0.55f, 0.30f, 0.45f, 0.60f },   // BASS  cutoff/res/decay/acc
    { 0.65f, 0.25f, 0.50f, 0.55f },   // LEAD  cutoff/res/decay/acc
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
    float v0 = cur_value(&P->k[0]), v1 = cur_value(&P->k[1]);
    float v2 = cur_value(&P->k[2]), v3 = cur_value(&P->k[3]);
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
            instrument_filter(SL_BASS, FILTER_LOW,
                              (int)(120 * powf(9000.0f / 120.0f, v0)), (int)(1 + v1 * 12));
            hit(33, SL_BASS, 2 + (int)(v3 * 5), 40 + (int)(v2 * 340));   // A1 acid root
        } break;
        case LEAD: {   // CUTOFF, RES, DECAY, ACC
            static const int TOPLINE[5] = { 57, 60, 64, 67, 72 };       // A-minor topline
            instrument_filter(SL_LEAD, FILTER_LOW,
                              (int)(200 * powf(9000.0f / 200.0f, v0)), (int)(1 + v1 * 10));
            hit(TOPLINE[leadArp % 5], SL_LEAD, 2 + (int)(v3 * 5), 40 + (int)(v2 * 340));
            leadArp++;
        } break;
    }
    flash[pi] = frame();
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
        for (int i = 0; i < NK; i++) {
            parts[p].k[i].base = KBASE[p][i];
            parts[p].k[i].live = KBASE[p][i];
        }
        flash[p] = -99;
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
        // motion dot — this part carries recorded motion
        bool hasMot = false;
        for (int i = 0; i < NK; i++) if (parts[p].k[i].motion) hasMot = true;
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

    print(rec ? "RECORDING - turn a knob" : "arm REC, then turn a knob",
          GX, 68, rec ? CLR_RED : CLR_MEDIUM_GREY);

    // ── the one knob row (edits the SELECTED part) + each knob's motion lane ──
    font(FONT_SMALL);
    for (int i = 0; i < NK; i++) {
        Param *P = &SP->k[i];
        // show the lane/base value UNLESS a finger is holding this knob
        if (!ui_cap_for(&P->live)) P->live = cur_value(P);

        ui_knob(&P->live, kx[i], KY, KLABEL[selPart][i]);

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

    if (ui_button(164, 176, 62, 16, "CLEAR")) {                     // clears the SELECTED part
        for (int i = 0; i < NK; i++) SP->k[i].motion = false;
    }
    ui_end();
}
