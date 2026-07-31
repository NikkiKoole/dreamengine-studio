// ─────────────────────────────────────────────────────────────────────────────
// lockup/score.h — MODULE: score.  The prison's soundtrack, and everything it hears.
//
// The reference is Prison Architect's own music: sparse, melancholy, Americana —
// a lonely pedal-steel line, an upright bass, brushed drums, a sad suitcase
// Rhodes, a harmonica, a low drone under it all, and a LOT of space and reverb.
// So: no loops, no stingers-as-switches. One band, playing one endless tune,
// whose ARRANGEMENT is the prison's own arithmetic coming back at you.
//
// OWNS
//   • every sound the cart makes.  No other module calls the audio API.
//   • instrument slots 5..23 (see the slot map below).  Nothing else defines one.
//   • the master bus config (reverb tanks, spring, echo, EQ, master filter) —
//     ALL of it behind ONE change-gated lks_apply_fx(), because these rebuild bus
//     DSP and calling them per frame is silent stutter (CLAUDE.md, set-and-hold).
//   • the held voices: drone, air, and the lead (steel/harmonica) + the siren.
//
// READS (and nothing else): lk.tension, lk.alarm, lk.clock/hour, lk.speed,
//   lk.seed, lk.over, lk_regime[] (the timetable — see §3), lk_cam_x/lk_cam_y
//   (where the ears are), lk_t[].door (so a door sounds like the door it is).
//
// ── KEY ALGORITHMS ──────────────────────────────────────────────────────────
//
//  1. THE VOICE BUDGET IS THE DESIGN, NOT A FOOTNOTE.  The mix is capped at 6
//     concurrent music voices, leaving 2 for diegetic sfx, and every layer's
//     cost is fixed by CONSTRUCTION rather than hoped for:
//
//        drone   1  held      note_on once, alive for the whole session
//        air     1  held      note_on/note_off by level (calm + aftermath only)
//        lead    1  held      ONE handle; the steel and the harmonica share the
//                            chair, so "two lead instruments" costs one voice
//        bass    1  one-shot  ≤2 plucks/bar, gate 700ms < the 968ms beat
//        rhodes  2  one-shot  a rootless 2-note shell (3rd + 7th), gate 600ms
//        kit     1  one-shot  every kit hit is < 320ms
//        ─────────
//        lvl 0:  drone + air + lead                       = 3
//        lvl 1+: drone + lead + bass + rhodes(2) + kit     = 6   (air is OFF)
//        sfx:    2 reserved (the held siren takes one during a riot; while it
//                holds, the Rhodes shell drops to ONE note — see lks_ep_comp)
//
//     The air layer is the calm-only extra that swaps OUT exactly when bass +
//     rhodes + kit swap in, which is also the right arrangement: the drone and
//     the HVAC exist to hold the floor while nothing else does.
//
//  2. INTENSITY IS A SHIFT, NOT A GATE (game-music.md, the load-bearing rule).
//     Two orthogonal axes, combined the radio.h way:
//
//        base      = what the REGIME is doing now  (lockup/sleep 0, busy 1)
//        intensity = what the PRISON feels like     (calm 1 … riot 3)
//        lvl       = clamp(base + intensity - 1, 0, 3)
//
//     so a quiet night at peace is lvl 0 and a riot during yard time is lvl 3,
//     and every notch of tension changes something audible at every hour. The
//     second, cheaper axis is TONE: a master-brightness multiplier re-issued
//     over the music slots' filter cutoffs (×0.55..1.30), which reads instantly.
//     Level changes LAND ON 2-BAR BOUNDARIES, one step at a time (two steps when
//     the alarm is already screaming) — the score ramps, it never cuts.
//
//  3. THE SCORE FOLLOWS THE TIMETABLE.  `base` above comes from lk_regime[hour],
//     which means the band literally plays the regime the player wrote: the bass
//     and the brushes walk in for Yard/Work/Eat and walk out for Lockup and
//     Sleep. A prison with no Eat slot sounds as wrong as it plays. That is the
//     honest core (a machine for meeting needs ON A SCHEDULE) reaching the ears.
//
//  4. HARMONY IS A VOCAB SWAP, WHICH IS A FREE KEY CHANGE.  One tonic for the
//     whole session (derived from lk.seed — your prison has a key), and the
//     chord walk runs harmony.h's Markov tables: HB_FOLK (plagal, IV-forward,
//     Americana) while things are calm, HB_CINEMATIC (the minor VI-III-VII
//     climb) from lvl 2 up. Same tonic, parallel mode — the modulation into
//     unrest costs one pointer. One chord per 8 bars calm, 4 unrest, 2 riot.
//     Voicing: a rootless SHELL (3rd + 7th) voice-led by nearest tone, so the
//     Rhodes sounds composed and the bass owns the root.
//
//  5. SCHEDULE AHEAD, NEVER TRIGGER ON THE FRAME.  Percussive onsets (kit, bass,
//     Rhodes) go through schedule_hit() one 16th-step of lookahead, off the
//     engine's own beat clock — the radio.h RadioClock idiom, inlined here so
//     this module's include surface stays tiny. The lead's note_on/note_pitch
//     are NOT schedulable, so they take one frame of jitter; that is deliberate
//     and inaudible (a bend has no attack, and a lonely steel entrance every 4
//     bars has nothing within 100ms of it to be late against).
//
//  6. THE DESK IS THE LOCKDOWN (dub.c's move, for grief).  At AL_LOCKDOWN the
//     arrangement stops being a curve and becomes a mixing desk: every 4 bars
//     the stems are re-rolled on/off with a HARD FLOOR (the mix can never
//     empty), and any stem that LEAVES gets a THROW on the way out — its last
//     note re-struck into a cranked spring + echo tail — so a mute never sounds
//     like a dropout. Then the aftermath: an 8-bar rallentando through live
//     bpm(), the level easing 3→0, and the HVAC air coming back underneath.
//     After a riot, the sound of a prison is a building with nobody talking.
//
//  7. SFX ARE DIEGETIC AND POSITIONED.  listener() sits at the camera centre and
//     lk_sfx_at() goes through hit_at(), so a door on the far side of the block
//     is quiet and to the left. Every kind gets its OWN instrument slot (a fixed
//     patch, never retuned at call time) and a frame COOLDOWN, plus a global cap
//     of 3 sfx starts per frame — a busy sim cannot machine-gun the mixer. A
//     door even reads lk_t[].door and sounds like the door it actually is.
//
// ── IMPLEMENTATION NOTES ────────────────────────────────────────────────────
//   • DEVIATION (include surface): this file includes "harmony.h" as well as
//     studio.h + model.h. It is an ENGINE shelf header (not a lockup module),
//     it is named as a LOCKUP dependency by docs/design/lockup.md §3, and the
//     brief asked for hb_pick/HB_FOLK/HB_CINEMATIC by name. It is header-only,
//     include-guarded and pure — safe if another module includes it too.
//     radio.h is NOT included (it drags in ui.h); its two borrowed idioms — the
//     level shift and the schedule-ahead clock — are ~20 lines and are inlined
//     here as lks_level()/lks_clock, deliberately matching radio.h's formulas.
//   • DEVIATION (no <stdio.h>): the debug rows are formatted by hand
//     (lks_put/lks_puti) rather than snprintf, to keep the include surface at
//     three headers like every other lockup module. Nothing allocates.
//   • DEVIATION (the pedal-steel chord): the brief suggested holding a chord on
//     several slide handles and bending one. That costs 3 voices for one
//     gesture. The cry is done INSIDE one voice instead (hold, note_pitch up a
//     whole step, drift back) — same signature sound, 1/3 of the budget, and it
//     leaves room for the harmonica to share the chair.
//   • DEVIATION (held-voice retrig): the first design used one held voice per
//     stem with note_retrig() to re-articulate. sound.h's sound_retrig_voice()
//     deliberately does NOT re-excite resonators (Karplus/modal/bore), so a
//     retrigged EPIANO or pizz string re-swells silence. Struck stems therefore
//     use scheduled one-shots with the gate kept shorter than the note spacing,
//     which is what keeps the voice count deterministic anyway.
//   • FOUND (not acted on): SOUND_VOICES is 32 in runtime/sound.h, not the 8 the
//     briefing states, and held voices are never stolen (sound_find_voice()
//     prefers the quietest NON-held voice). The 6+2 budget above is kept anyway
//     — CPU is the real cost of a voice and this cart is a heavy sim — but there
//     is headroom if a later pass wants the 3-handle steel chord after all.
//   • NEEDS FROM OTHERS: lk_regime[] (actors) is read for the arrangement base;
//     lk_cam_x/lk_cam_y (hud) place the listener; lk_t[].door (grid) picks the
//     door timbre. All three are contract globals and all three degrade
//     gracefully (a zeroed regime = AC_SLEEP = a quiet score, camera 0,0 = ears
//     top-left, door 0 = the plain-door timbre). No module functions are called.
//   • The lead's phrase decisions, the chord walk and the key come from a
//     cart-local xorshift seeded off lk.seed (composition — same prison, same
//     tune); humanize/ghost/desk rolls use the engine's rnd()/chance()
//     (performance). That is the game-music.md split.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef LOCKUP_SCORE_H
#define LOCKUP_SCORE_H

#include "studio.h"
#include "lockup/model.h"
#include "harmony.h"   // the shared harmony brain: HB_FOLK / HB_CINEMATIC + hb_pick

// ── instrument slot map (5..23; nothing else in the cart defines a slot) ─────
#define LKS_I_DRONE    5   // INSTR_SINE   — the low felt drone ("felt, not heard")
#define LKS_I_AIR      6   // INSTR_NOISE  — cell-block HVAC / wind wash
#define LKS_I_STEEL    7   // INSTR_PLUCK  — the pedal-steel guitar (the signature)
#define LKS_I_HARM     8   // INSTR_REED   — the harmonica (shares the lead chair)
#define LKS_I_BASS     9   // INSTR_BOWED  — upright bass, pizzicato
#define LKS_I_EP      10   // INSTR_EPIANO — the sad suitcase Rhodes
#define LKS_I_RIDE    11   // brushed ride / stick
#define LKS_I_HAT     12   // hat
#define LKS_I_SWEEP   13   // brush sweep (wash) / stick tap
#define LKS_I_KICK    14   // soft kick
// ── sfx (each kind owns a slot with a FIXED patch — never retuned at call time)
#define LKS_I_SIREN   15   // held warbling square — the alarm
#define LKS_I_TICK    16   // dry high tick — UI, tray, scuffle
#define LKS_I_CLACK   17   // bandpassed metal clack — bolts, locks
#define LKS_I_CREAK   18   // plain-door squeak
#define LKS_I_ROLL    19   // jail-door slide (metal on metal)
#define LKS_I_THUD    20   // low body thud — door shut, fight, deny
#define LKS_I_WHIST   21   // guard whistle
#define LKS_I_CHIME   22   // cash / meal / grant
#define LKS_I_RUMBLE  23   // the intake bus diesel
#define LKS_SLOTS     24   // one past the last slot we own (shadow-table size)

// ── tuning ──────────────────────────────────────────────────────────────────
#define LKS_BPM_CALM     62      // the melancholy walking tempo
#define LKS_BPM_RIOT     69      // +12% (game-music.md: tempo IS a tension lever)
#define LKS_SFX_MAX     760.0f   // world px past which a sound is not worth a voice
#define LKS_SFX_REF      64.0f   // world px of full volume (4 tiles)
#define LKS_DESK_BARS     8      // the aftermath: 8 bars from lockdown back to calm

// stems the desk can mute.  drone + air are the floor and are never muted.
enum { LKS_ST_LEAD = 0, LKS_ST_BASS, LKS_ST_EP, LKS_ST_KIT, LKS_NSTEM };

// pitch-class names for the debug readout
static const char *const LKS_PCN[12] =
    { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
// the two scales the lead sings — major under HB_FOLK, natural minor under HB_CINEMATIC
static const int LKS_SC_MAJ[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int LKS_SC_MIN[7] = { 0, 2, 3, 5, 7, 8, 10 };
static const char *const LKS_LVLNAME[4] = { "calm", "unrest", "riot", "lockdown" };

// ── state ───────────────────────────────────────────────────────────────────
static int      lks_ready      = 0;      // init has run
static unsigned lks_rng        = 1u;     // composition PRNG (xorshift32)

// the clock (the radio.h RadioClock idiom, inlined)
static long   lks_sched   = -1;          // last absolute 16th-step scheduled
static long   lks_base    = 0;           // absolute step the score started on
static double lks_stepms  = 242.0;       // ms per 16th at the live tempo
static long   lks_bar     = -1;          // last bar we ran the bar-boundary work for

// tempo
static int    lks_bpm_now = LKS_BPM_CALM;   // what we last told bpm()
static float  lks_bpm_f   = (float)LKS_BPM_CALM;

// harmony
static int  lks_key     = 9;             // tonic pitch class (from lk.seed)
static int  lks_fn      = HB_I;          // current function index in the live vocab
static int  lks_minor   = 0;             // 0 = HB_FOLK (major), 1 = HB_CINEMATIC
static int  lks_pcs[4]  = { 9, 0, 4, 7 };// current chord tones as pitch classes
static int  lks_qual    = HBQ_MAJ7;      // its quality, for the readout
static long lks_chord_bar = -1;          // bar the current chord landed on

// arrangement
static int   lks_lvl        = 0;          // the live level 0..3 (ramped)
static float lks_tone       = 0.62f;      // master brightness multiplier
static float lks_tone_done  = -1.0f;      // last multiplier actually issued
static unsigned char lks_on[LKS_NSTEM] = { 1, 1, 1, 1 };

// the desk (lockdown + aftermath)
static int  lks_desk      = 0;            // 1 = the desk is running
static int  lks_desk_left = 0;            // bars of aftermath remaining
static long lks_desk_phase = -1;          // last 4-bar phase rolled
static int  lks_throw_f   = 0;            // frames left of a cranked throw tail

// held voices
static int   lks_drone_h = 0;
static int   lks_air_h   = 0;
static float lks_air_v   = 0.0f;          // the air's live volume (ramped)
static float lks_air_vp  = -1.0f;         // the last value actually pushed to note_vol
static int   lks_siren_h = 0;
static float lks_siren_v = 0.0f;          // the live ramp value
static float lks_siren_vp = -1.0f;        // the last value actually pushed to note_vol
static int   lks_siren_lock = -1;         // 1 = the slower lockdown warble is set
static int   lks_siren_hold = 0;          // frames of siren requested by lk_sfx()

// the lead chair
static int   lks_lead_h    = 0;           // held handle, 0 = resting
static int   lks_lead_slot = LKS_I_STEEL;
static float lks_lead_midi = 64.0f;
static int   lks_lead_pre  = 0;           // note to return to after a bend (0 = none)
static int   lks_lead_left = 0;           // moves remaining in this phrase
static long  lks_lead_next = 0;           // absolute step of the next move
static long  lks_lead_end  = 0;           // absolute step the phrase releases
static long  lks_lead_rest = 0;           // bar the lead may enter again

// voicing state
static int  lks_epv[2]   = { 60, 64 };    // the Rhodes shell (3rd + 7th), voice-led
static int  lks_ep_init  = 0;
static int  lks_bassm    = 40;            // last bass note (nearest-octave walk)
static int  lks_kit_stick = -1;           // -1 unset, 0 brushes, 1 sticks

// master filter — only engaged for the pause/game-over muffle
static float lks_muffle     = 0.0f;
static int   lks_muffle_idle = 1;

// the debug readout
#define LKS_NROW 9
static char lks_row[LKS_NROW][24];
static int  lks_row_dirty = 1;

// ── composition PRNG (xorshift32 — same shape as radio.h's, cart-local) ─────
static unsigned lks_rnd_u(void) {
    lks_rng ^= lks_rng << 13;
    lks_rng ^= lks_rng >> 17;
    lks_rng ^= lks_rng << 5;
    return lks_rng;
}
static int lks_srnd(int n) { return n > 0 ? (int)(lks_rnd_u() % (unsigned)n) : 0; }

// ── tiny helpers ────────────────────────────────────────────────────────────
static float lks_clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static int   lks_clampi(int v, int lo, int hi)        { return v < lo ? lo : v > hi ? hi : v; }
// a humanized delay can never land in the past — schedule_hit wants ms >= 1
static int   lks_dly(int ms)                          { return ms < 1 ? 1 : ms; }

// ease `cur` toward `target` at `rate` units per second
static float lks_glide(float cur, float target, float rate, float d) {
    float s = rate * d;
    if (s <= 0.0f) return target;
    if (cur < target) { cur += s; if (cur > target) cur = target; }
    else              { cur -= s; if (cur < target) cur = target; }
    return cur;
}

// nearest instance of pitch class `pc` to `from` (±6 semitones), folded into [lo,hi]
static int lks_near_pc(int pc, int from, int lo, int hi) {
    int d = ((pc - from) % 12 + 18) % 12 - 6;
    int n = from + d;
    while (n < lo) n += 12;
    while (n > hi) n -= 12;
    return n;
}

// ── set-and-hold shadows ────────────────────────────────────────────────────
// Every per-slot mixer write goes through these, so lks_apply_fx() can be a
// plain "recompute everything from the current state" pass called once a frame
// without ever re-issuing an unchanged value. This is also what restores a
// throw automatically: the throw is just a different computed value.
static float lks_sh_level[LKS_SLOTS];
static float lks_sh_rev[LKS_SLOTS];
static float lks_sh_echo[LKS_SLOTS];

static void lks_level_to(int slot, float g) {
    if (slot < 0 || slot >= LKS_SLOTS) return;
    g = lks_clampf(g, 0.0f, 1.0f);
    float d = g - lks_sh_level[slot];
    if (d > -0.01f && d < 0.01f) return;
    lks_sh_level[slot] = g;
    instrument_level(slot, g);
}
static void lks_rev_to(int slot, float s) {
    if (slot < 0 || slot >= LKS_SLOTS) return;
    s = lks_clampf(s, 0.0f, 1.0f);
    float d = s - lks_sh_rev[slot];
    if (d > -0.01f && d < 0.01f) return;
    lks_sh_rev[slot] = s;
    instrument_reverb(slot, s);
}
static void lks_rev_bus_to(int slot, int tank, float s) {
    if (slot < 0 || slot >= LKS_SLOTS) return;
    s = lks_clampf(s, 0.0f, 1.0f);
    float d = s - lks_sh_rev[slot];
    if (d > -0.01f && d < 0.01f) return;
    lks_sh_rev[slot] = s;
    instrument_reverb_bus(slot, tank, s);
}
static void lks_echo_to(int slot, float s) {
    if (slot < 0 || slot >= LKS_SLOTS) return;
    s = lks_clampf(s, 0.0f, 1.0f);
    float d = s - lks_sh_echo[slot];
    if (d > -0.01f && d < 0.01f) return;
    lks_sh_echo[slot] = s;
    instrument_echo(slot, s);
}

// ── THE FX DESK — every bus effect, in one change-gated place ───────────────
// reverb/reverb_bus/reverb_spring/echo/eq all rebuild bus DSP: calling any of
// them per frame is silent stutter (CLAUDE.md). Each has a last-applied shadow
// and only fires on a real change. Called once per frame from lk_score_update.
static void lks_apply_fx(void) {
    static float l_rs = -1.0f, l_rd = -1.0f;
    static float l_bs = -1.0f, l_bd = -1.0f;
    static float l_spr = -1.0f;
    static int   l_et = -1;
    static float l_efb = -1.0f, l_eto = -1.0f;
    static float l_qlo = -99.0f, l_qmid = -99.0f, l_qhi = -99.0f;

    int hot = lks_throw_f > 0;

    // ── tank 0: the room the whole prison sits in.  0.92/0.35 is the pinned
    // melancholy setting; a riot pulls the damping DOWN (a harder, brighter
    // tail), the aftermath pushes size and damping UP (a bigger, deader hall).
    float rs = lks_desk ? 0.95f : (lks_lvl >= 2 ? 0.86f : 0.92f);
    float rd = lks_desk ? 0.50f : (lks_lvl >= 2 ? 0.22f : 0.35f);
    if (rs < l_rs - 0.005f || rs > l_rs + 0.005f || rd < l_rd - 0.005f || rd > l_rd + 0.005f) {
        reverb(rs, rd); l_rs = rs; l_rd = rd;
    }
    // ── tank 1: the long plate the lead alone sits in (two tanks = the steel
    // rings for bars while the ambience stays a small room).
    float bs = 0.96f, bd = lks_lvl >= 2 ? 0.24f : 0.32f;
    if (bs < l_bs - 0.005f || bs > l_bs + 0.005f || bd < l_bd - 0.005f || bd > l_bd + 0.005f) {
        reverb_bus(1, bs, bd); l_bs = bs; l_bd = bd;
    }
    // ── the spring tank: Americana twang, not a concert hall.  A throw opens it.
    float spr = hot ? 0.58f : (lks_lvl >= 2 ? 0.16f : 0.32f);
    if (spr < l_spr - 0.01f || spr > l_spr + 0.01f) { reverb_spring(spr); l_spr = spr; }

    // ── echo: a tempo-synced dotted 8th (3 sixteenths), quantised to 10ms so a
    // gliding tempo doesn't re-issue it every frame.  A throw rides it near
    // runaway for the phrase — that is what makes a mute sound like a decision.
    int et = (int)(lks_stepms * 3.0);
    et = (lks_clampi(et, 60, 1400) / 10) * 10;
    float efb = hot ? 0.78f : (lks_lvl >= 2 ? 0.40f : 0.24f);
    float eto = lks_desk ? 0.18f : 0.30f;
    if (et != l_et || efb < l_efb - 0.01f || efb > l_efb + 0.01f
                   || eto < l_eto - 0.01f || eto > l_eto + 0.01f) {
        echo(et, efb, eto); l_et = et; l_efb = efb; l_eto = eto;
    }
    // ── EQ: warm and dark at rest, mids forward in a riot, very dark after one.
    float qlo = 2.0f, qmid = -1.0f, qhi = -2.0f;
    if (lks_lvl >= 2) { qlo = 0.0f; qmid = 3.0f; qhi = 1.0f; }
    if (lks_desk)     { qlo = 3.0f; qmid = -2.0f; qhi = -4.0f; }
    if (qlo != l_qlo || qmid != l_qmid || qhi != l_qhi) {
        eq(qlo, qmid, qhi); l_qlo = qlo; l_qmid = qmid; l_qhi = qhi;
    }

    // ── per-slot sends + levels (all shadow-gated).  The stem gains here are
    // what the desk actually mutes; the throw is a computed value, so it
    // restores itself when lks_throw_f runs out.
    float lead_g = lks_on[LKS_ST_LEAD] ? 1.0f : 0.0f;
    float bass_g = lks_on[LKS_ST_BASS] ? 1.0f : 0.0f;
    float ep_g   = lks_on[LKS_ST_EP]   ? 1.0f : 0.0f;
    float kit_g  = lks_on[LKS_ST_KIT]  ? 1.0f : 0.0f;
    if (lk.over) { bass_g = 0.0f; kit_g = 0.0f; ep_g *= 0.5f; }

    lks_level_to(LKS_I_DRONE, lks_desk ? 1.0f : 0.85f);
    lks_level_to(LKS_I_AIR,   0.8f);
    lks_level_to(LKS_I_STEEL, lead_g * 0.9f);
    lks_level_to(LKS_I_HARM,  lead_g * 0.8f);
    lks_level_to(LKS_I_BASS,  bass_g * 0.95f);
    lks_level_to(LKS_I_EP,    ep_g * 0.8f);
    lks_level_to(LKS_I_RIDE,  kit_g * 0.55f);
    lks_level_to(LKS_I_HAT,   kit_g * 0.45f);
    lks_level_to(LKS_I_SWEEP, kit_g * 0.6f);
    lks_level_to(LKS_I_KICK,  kit_g * 0.8f);

    lks_rev_to(LKS_I_DRONE, 0.35f);
    lks_rev_to(LKS_I_AIR,   0.25f);
    lks_rev_bus_to(LKS_I_STEEL, 1, hot ? 1.0f : 0.75f);
    lks_rev_bus_to(LKS_I_HARM,  1, hot ? 1.0f : 0.62f);
    lks_rev_to(LKS_I_BASS,  0.14f);
    lks_rev_to(LKS_I_EP,    hot ? 0.85f : 0.45f);
    lks_rev_to(LKS_I_SWEEP, 0.34f);
    lks_rev_to(LKS_I_RIDE,  0.22f);
    lks_rev_to(LKS_I_KICK,  0.08f);

    lks_echo_to(LKS_I_STEEL, hot ? 0.85f : 0.18f);
    lks_echo_to(LKS_I_HARM,  hot ? 0.85f : 0.14f);
    lks_echo_to(LKS_I_EP,    hot ? 0.70f : 0.10f);
    lks_echo_to(LKS_I_BASS,  hot ? 0.35f : 0.0f);
    lks_echo_to(LKS_I_SWEEP, hot ? 0.55f : 0.06f);
}

// ── the tone axis (the cheap second dimension) ───────────────────────────────
// Re-issue the MUSIC slots' filter cutoffs against a master-brightness
// multiplier, the RAD_TONEMUL move. Deliberately NOT the master filter — that
// would dull the sfx too. Quantised to 0.04 so a glide doesn't churn it.
static void lks_apply_voicing(float m) {
    if (m > lks_tone_done - 0.04f && m < lks_tone_done + 0.04f) return;
    lks_tone_done = m;
    instrument_filter(LKS_I_STEEL, FILTER_LOW,  (int)(2600.0f * m), 2);
    instrument_filter(LKS_I_HARM,  FILTER_LOW,  (int)(2200.0f * m), 2);
    instrument_filter(LKS_I_EP,    FILTER_LOW,  (int)(2400.0f * m), 2);
    instrument_filter(LKS_I_BASS,  FILTER_LOW,  (int)(760.0f * (0.75f + 0.25f * m)), 1);
    instrument_filter(LKS_I_DRONE, FILTER_LOW,  (int)(300.0f * (0.80f + 0.20f * m)), 1);
    instrument_filter(LKS_I_AIR,   FILTER_BAND, (int)(700.0f * m), 6);
}

// ── the kit, re-voiced in place (brushes ⇄ sticks) ──────────────────────────
// The calm→unrest→riot tell for free: the SAME four slots, a different pair of
// hands. Change-gated because instrument() rebuilds the patch.
static void lks_voice_kit(int sticks) {
    if (sticks == lks_kit_stick) return;
    lks_kit_stick = sticks;
    if (!sticks) {                                   // BRUSHES — wire, felt, air
        instrument(LKS_I_RIDE,  INSTR_SQUARE, 1, 260, 0, 160);
        instrument_filter(LKS_I_RIDE, FILTER_HIGH, 5400, 2);
        instrument(LKS_I_HAT,   INSTR_NOISE,  0, 30, 0, 14);
        instrument_filter(LKS_I_HAT, FILTER_HIGH, 8200, 3);
        instrument(LKS_I_SWEEP, INSTR_NOISE, 60, 300, 2, 200);   // the circular wash
        instrument_filter(LKS_I_SWEEP, FILTER_HIGH, 4800, 2);
        instrument(LKS_I_KICK,  INSTR_SINE,   0, 150, 0, 60);
        instrument_filter(LKS_I_KICK, FILTER_LOW, 220, 1);
    } else {                                         // STICKS — wood, and closer
        instrument(LKS_I_RIDE,  INSTR_SQUARE, 1, 240, 0, 140);
        instrument_filter(LKS_I_RIDE, FILTER_HIGH, 6400, 3);
        instrument(LKS_I_HAT,   INSTR_NOISE,  0, 22, 0, 12);
        instrument_filter(LKS_I_HAT, FILTER_HIGH, 9000, 4);
        instrument(LKS_I_SWEEP, INSTR_NOISE,  2, 60, 0, 40);     // the wash becomes a TAP
        instrument_filter(LKS_I_SWEEP, FILTER_BAND, 3200, 7);
        instrument(LKS_I_KICK,  INSTR_SINE,   0, 140, 0, 55);
        instrument_filter(LKS_I_KICK, FILTER_LOW, 300, 1);
    }
    instrument_env(LKS_I_KICK, 0, ENV_PITCH, 0, 55, 12.0f);      // the thumb on the head
    lks_sh_level[LKS_I_RIDE] = lks_sh_level[LKS_I_HAT] = -1.0f;  // patch reset ⇒ re-issue
    lks_sh_level[LKS_I_SWEEP] = lks_sh_level[LKS_I_KICK] = -1.0f;
}

// ── the level shift (radio.h's rad_level formula, on the game's two axes) ───
static int lks_regime_base(void) {
    int h = lks_clampi(lk.hour, 0, 23);
    int ac = lk_regime[h];
    if (ac == AC_EAT || ac == AC_YARD || ac == AC_WORK) return 1;   // the prison is up
    return 0;                                                      // sleep / lockup / quiet
}
static int lks_intensity(void) {
    float t = lks_clampf(lk.tension, 0.0f, 1.0f);
    int in = 1 + (t >= 0.32f ? 1 : 0) + (t >= 0.64f ? 1 : 0);
    if (lk.alarm >= AL_RIOT) in = 3;
    else if (lk.alarm == AL_INCIDENT && in < 2) in = 2;
    return in;
}
static int lks_level_want(void) {
    return lks_clampi(lks_regime_base() + lks_intensity() - 1, 0, 3);
}

// ── harmony ─────────────────────────────────────────────────────────────────
static const HbStyle *lks_style(void) { return lks_minor ? &HB_CINEMATIC : &HB_FOLK; }
static const HbVocab *lks_vocab(void) {
    const HbStyle *st = lks_style();
    return st->vocab ? st->vocab : &HB_MAJOR;
}
static int lks_chord_bars(void) { return lks_lvl <= 0 ? 8 : (lks_lvl == 1 ? 4 : 2); }

// the Rhodes SHELL: 3rd + 7th of the chord, each moved to its nearest instance,
// with the 2×2 assignment that travels least (the nearest-tone voice-leading
// tie-break). The bass owns the root, so the shell never doubles it.
static void lks_lead_shell(void) {
    int a = lks_pcs[1], b = lks_pcs[3];
    if (!lks_ep_init) {
        lks_epv[0] = lks_near_pc(a, 58, 55, 74);
        lks_epv[1] = lks_near_pc(b, 65, 55, 74);
        lks_ep_init = 1;
    } else {
        int a0 = lks_near_pc(a, lks_epv[0], 55, 74), b1 = lks_near_pc(b, lks_epv[1], 55, 74);
        int b0 = lks_near_pc(b, lks_epv[0], 55, 74), a1 = lks_near_pc(a, lks_epv[1], 55, 74);
        int c1 = abs(a0 - lks_epv[0]) + abs(b1 - lks_epv[1]);
        int c2 = abs(b0 - lks_epv[0]) + abs(a1 - lks_epv[1]);
        if (c1 <= c2) { lks_epv[0] = a0; lks_epv[1] = b1; }
        else          { lks_epv[0] = b0; lks_epv[1] = a1; }
    }
    if (lks_epv[0] == lks_epv[1]) {
        lks_epv[1] += 12;
        if (lks_epv[1] > 74) lks_epv[1] -= 24;
    }
}

// advance the chord walk one step (or seat the first chord)
static void lks_chord_next(int first) {
    if (!first) {
        int want_minor = (lks_lvl >= 2) ? 1 : 0;
        if (want_minor != lks_minor) {
            lks_minor = want_minor;
            lks_fn = 0;                 // index 0 is the tonic in BOTH vocabs (HB_I / HBm_i)
        }
        const HbStyle *st = lks_style();
        lks_fn = hb_pick(st, lks_fn, lks_srnd(hb_nopts(st, lks_fn)));
    } else {
        lks_fn = 0;
    }
    const HbVocab *vo = lks_vocab();
    hb_vocab_pcs(vo, lks_key, lks_fn, lks_pcs);
    lks_qual = vo->qual[lks_fn];
    lks_lead_shell();
    lks_row_dirty = 1;
}

// ── the lead chair: one held voice, two instruments, and a lot of space ─────
// The steel is a Karplus string picked ONCE per phrase and then DRAGGED with
// note_pitch (that is what a bar on a steel guitar does); the harmonica is a
// self-oscillating reed doing draw-bends through the same handle discipline.
// Density shapes TOUCH and ORNAMENT, not the number of layers (satie's lesson).
static int lks_lead_lo(void) { return lks_lead_slot == LKS_I_HARM ? 64 : 52; }
static int lks_lead_hi(void) { return lks_lead_slot == LKS_I_HARM ? 83 : 71; }

// a target for the lead: mostly a chord tone, sometimes a scale neighbour
static int lks_lead_pick(void) {
    int pc;
    if (lks_srnd(100) < 62) pc = lks_pcs[lks_srnd(4)];
    else {
        const int *sc = lks_minor ? LKS_SC_MIN : LKS_SC_MAJ;
        pc = (lks_key + sc[lks_srnd(7)]) % 12;
    }
    int cur = (int)(lks_lead_midi + 0.5f);
    int n = lks_near_pc(pc, cur, lks_lead_lo(), lks_lead_hi());
    if (n == cur) n = lks_near_pc(pc, cur + (lks_srnd(2) ? 5 : -5), lks_lead_lo(), lks_lead_hi());
    return n;
}

static void lks_lead_release(void) {
    if (lks_lead_h) { note_off(lks_lead_h); lks_lead_h = 0; }
    lks_lead_pre = 0;
    lks_lead_left = 0;
}

// start a phrase.  One note_on, then the whole phrase is note_pitch.
static void lks_lead_enter(long abs_step) {
    lks_lead_release();
    // the chair swap: the harmonica takes about a third of the phrases.  Same
    // handle, same budget — two lead instruments for the price of one voice.
    lks_lead_slot = (lks_srnd(100) < 32) ? LKS_I_HARM : LKS_I_STEEL;
    int lo = lks_lead_lo(), hi = lks_lead_hi();
    int cur = lks_clampi((int)(lks_lead_midi + 0.5f), lo, hi);
    int pc  = lks_pcs[lks_srnd(100) < 55 ? 1 : 2];         // enter on the 3rd or the 5th
    int m   = lks_near_pc(pc, cur, lo, hi);

    int vol   = lks_lvl <= 0 ? 3 : (lks_lvl == 1 ? 4 : 5);
    if (lks_desk) vol = 3;
    int glide = lks_lvl <= 0 ? 720 : (lks_lvl == 1 ? 520 : 300);
    float vib = lks_lvl <= 0 ? 0.06f : (lks_lvl == 1 ? 0.10f : 0.15f);

    lks_lead_h = note_on(m, lks_lead_slot, vol);
    if (lks_lead_h) {
        note_glide(lks_lead_h, glide);
        note_glide_scale(lks_lead_h, GLIDE_ANALOG);       // big leaps take a bit longer
        note_lfo(lks_lead_h, 0, LFO_PITCH, 4.6f, vib);    // the hand's vibrato
        if (lks_lead_slot == LKS_I_HARM)
            note_lfo(lks_lead_h, 1, LFO_CUTOFF, 0.35f, 260.0f);   // the reed breathing
    }
    lks_lead_midi = (float)m;

    // how many moves, and how far apart — the density curve, as phrasing
    int moves = lks_lvl <= 0 ? 1 + lks_srnd(2) : (lks_lvl == 1 ? 2 + lks_srnd(2) : 3 + lks_srnd(3));
    lks_lead_left = moves;
    lks_lead_next = abs_step + 4 * (2 + lks_srnd(3));                    // 2..4 beats in
    lks_lead_end  = abs_step + 16 * (lks_lvl <= 0 ? 4 : (lks_lvl == 1 ? 3 : 2));
    lks_row_dirty = 1;
}

// one move inside a phrase — either a new target or THE CRY (bend a whole step
// up and drift back, which is the pedal-steel gesture, done inside one voice)
static void lks_lead_move(long abs_step) {
    if (!lks_lead_h) return;
    if (lks_lead_pre) {                                   // come back down off the bend
        note_pitch(lks_lead_h, (float)lks_lead_pre);
        lks_lead_midi = (float)lks_lead_pre;
        lks_lead_pre = 0;
        lks_lead_next = abs_step + 4 * (2 + lks_srnd(3));
        return;
    }
    int cry = lks_srnd(100) < (lks_lvl >= 2 ? 34 : 26);
    if (cry) {
        lks_lead_pre = (int)(lks_lead_midi + 0.5f);
        float up = lks_lead_midi + (lks_srnd(100) < 70 ? 2.0f : 1.0f);
        if (up > (float)lks_lead_hi()) up = lks_lead_midi - 2.0f;
        note_pitch(lks_lead_h, up);
        lks_lead_midi = up;
        lks_lead_next = abs_step + 4 + lks_srnd(5);        // the cry hangs, then falls
        return;
    }
    int m = lks_lead_pick();
    note_pitch(lks_lead_h, (float)m);
    lks_lead_midi = (float)m;
    lks_lead_left--;
    lks_lead_next = abs_step + 4 * (1 + lks_srnd(4));
}

// ── the desk: the lockdown arrangement (dub.c's THE DESK, for grief) ────────
// A stem that LEAVES throws its last note into a cranked spring + echo tail, so
// a mute reads as a decision and never as a dropout.
static void lks_throw(int stem, int dly) {
    lks_throw_f = 150;                                    // ~2.5s of hot bus
    switch (stem) {
    case LKS_ST_LEAD:
        if (lks_lead_h) {
            note_echo(lks_lead_h, 0.9f);
            note_reverb(lks_lead_h, 1.0f);
            note_off(lks_lead_h);                         // released INTO the tail
            lks_lead_h = 0;
            lks_lead_left = 0;
        }
        break;
    case LKS_ST_EP:
        schedule_hit(dly, lks_epv[0], LKS_I_EP, 3, 900);
        schedule_hit(dly + 26, lks_epv[1], LKS_I_EP, 3, 900);
        break;
    case LKS_ST_BASS:
        schedule_hit(dly, lks_bassm, LKS_I_BASS, 3, 900);      // the root it last played
        break;
    case LKS_ST_KIT:
        schedule_hit(dly, 62, LKS_I_SWEEP, 3, 320);       // one last brush across the head
        break;
    default: break;
    }
}

// re-roll the mix on a 4-bar boundary, with a hard floor so it never empties
static void lks_desk_roll(long bar, int dly) {
    if (bar / 4 == lks_desk_phase) return;
    lks_desk_phase = bar / 4;
    unsigned char was[LKS_NSTEM];
    for (int i = 0; i < LKS_NSTEM; i++) was[i] = lks_on[i];
    lks_on[LKS_ST_LEAD] = chance(70);
    lks_on[LKS_ST_BASS] = chance(45);
    lks_on[LKS_ST_EP]   = chance(55);
    lks_on[LKS_ST_KIT]  = chance(28);
    if (!lks_on[LKS_ST_LEAD] && !lks_on[LKS_ST_EP]) lks_on[LKS_ST_LEAD] = 1;   // the floor
    for (int i = 0; i < LKS_NSTEM; i++)
        if (was[i] && !lks_on[i]) lks_throw(i, dly);
    lks_row_dirty = 1;
}

// ── the step player — everything with an ATTACK is scheduled from here ──────
static void lks_step(long abs_step, double pos) {
    long s = abs_step - lks_base;
    if (s < 0) return;
    int  step = (int)(s & 15);
    long bar  = s >> 4;

    int dly = (int)(((double)abs_step - pos) * lks_stepms);
    if (dly < 1) dly = 1;
    // brushed time is laid back: nudge the off-16ths late (less so when it hurries)
    int swing = (step & 1) ? (int)(lks_stepms * (lks_lvl >= 2 ? 0.06f : 0.12f)) : 0;

    // ── bar boundaries: the arrangement moves here, never mid-bar ───────────
    if (step == 0 && bar != lks_bar) {
        lks_bar = bar;

        // the desk / aftermath bookkeeping
        if (lk.alarm >= AL_LOCKDOWN) { lks_desk = 1; lks_desk_left = LKS_DESK_BARS; }
        else if (lks_desk) {
            lks_desk_left--;
            if (lks_desk_left <= 0) {
                lks_desk = 0;
                for (int i = 0; i < LKS_NSTEM; i++) lks_on[i] = 1;   // the band comes back
                lks_desk_phase = -1;
                lks_row_dirty = 1;
            }
        }
        if (lks_desk) lks_desk_roll(bar, dly);

        // the level lands on 2-bar boundaries — a ramp, never a cut.  A riot
        // that is already screaming gets to climb two notches at a time.
        if ((bar & 1) == 0) {
            int want = lks_level_want();
            if (lks_desk) want = lks_desk_left > 5 ? 3 : (lks_desk_left > 3 ? 2
                              : (lks_desk_left > 1 ? 1 : 0));
            else if (lks_lvl == 3 && want <= 1) {          // a riot ended on its own:
                lks_desk = 1;                              // play the aftermath anyway
                lks_desk_left = LKS_DESK_BARS;
                lks_desk_phase = -1;
                want = 3;
            }
            if (want > lks_lvl) {
                lks_lvl += (lk.alarm >= AL_RIOT) ? 2 : 1;
                if (lks_lvl > want) lks_lvl = want;
            } else if (want < lks_lvl) lks_lvl--;
            lks_row_dirty = 1;
        }

        // the chord walk
        if (lks_chord_bar < 0 || bar - lks_chord_bar >= lks_chord_bars()) {
            lks_chord_bar = bar;
            lks_chord_next(0);
            // at lvl 2+ the drone stops being a drone and follows the root — the
            // pedal starts moving, which is what "harmonic motion" sounds like
            if (lks_drone_h && lks_lvl >= 2)
                note_pitch(lks_drone_h, (float)lks_near_pc(lks_pcs[0], 30, 26, 34));
            else if (lks_drone_h)
                note_pitch(lks_drone_h, (float)lks_near_pc(lks_key, 30, 26, 34));
        }

        // the lead enters / leaves.  The RESTS are the music at lvl 0.
        if (lks_on[LKS_ST_LEAD] && !lk.over) {
            if (!lks_lead_h && bar >= lks_lead_rest) lks_lead_enter(abs_step);
        } else if (lks_lead_h) lks_lead_release();
    }

    if (lk.over && lks_lvl > 0) lks_lvl = 0;    // the endcard: drone + one long note

    int kit  = lks_on[LKS_ST_KIT]  && lks_lvl >= 1 && !lk.over;
    int bass = lks_on[LKS_ST_BASS] && lks_lvl >= 1 && !lk.over;
    int ep   = lks_on[LKS_ST_EP]   && lks_lvl >= 1 && !lk.over;
    int stick = lks_lvl >= 2 && !lks_desk;
    if (kit) lks_voice_kit(stick);

    // ── BASS — the upright, walking and sparse.  Root on 1, the 5th on 3, and
    // at riot a passing tone leaning into the next bar.  Two plucks a bar keep
    // it inside one voice.
    if (bass) {
        int hum = rnd(7) - 3;
        if (step == 0) {
            lks_bassm = lks_near_pc(lks_pcs[0], lks_bassm, 33, 45);
            schedule_hit(lks_dly(dly + hum), lks_bassm, LKS_I_BASS, 4, 700);
        } else if (step == 8 && !lks_desk) {
            int pc = (lks_lvl >= 2 && chance(45)) ? lks_pcs[2] : lks_pcs[0];
            int m = lks_near_pc(pc, lks_bassm, 33, 45);
            schedule_hit(lks_dly(dly + hum), m, LKS_I_BASS, 3, 620);
        } else if (step == 14 && lks_lvl >= 2 && !lks_desk && chance(55)) {
            int m = lks_clampi(lks_bassm + (chance(50) ? -1 : 1), 33, 45);
            schedule_hit(lks_dly(dly + hum), m, LKS_I_BASS, 2, 340);
        }
    }

    // ── RHODES — the shell on 2 and 4 (the sad backbeat), gate kept shorter
    // than the spacing so exactly two voices are ever ringing.  While the siren
    // holds it drops to ONE note, which is how the sfx budget stays honest.
    if (ep) {
        int comp = (step == 4 || step == 12);
        if (lks_desk) comp = (step == 4);
        if (lks_lvl >= 2 && step == 11 && chance(30)) comp = 1;   // the anticipation push
        if (comp) {
            int vol = lks_lvl >= 2 ? 4 : 3;
            int one = (lks_siren_h != 0);
            schedule_hit(dly + swing + rnd(5), lks_epv[0], LKS_I_EP, vol, 600);
            if (!one) schedule_hit(dly + swing + 9 + rnd(6), lks_epv[1], LKS_I_EP, vol, 600);
        }
    }

    // ── THE KIT — brushes, then sticks.  Same slots, different hands.
    if (kit) {
        int hum = rnd(9) - 4;
        if (!stick) {
            if (step == 0 || step == 4 || step == 8 || step == 12)
                schedule_hit(lks_dly(dly + hum), 74, LKS_I_RIDE, 2, 180);
            if ((step == 6 || step == 14) && lks_lvl >= 2)
                schedule_hit(lks_dly(dly + swing + hum), 74, LKS_I_RIDE, 1, 140);
            if (step == 0)                                     // the circular wash
                schedule_hit(lks_dly(dly + hum), 62, LKS_I_SWEEP, 2, 300);
            if (step == 8 && lks_lvl >= 2 && chance(60))
                schedule_hit(lks_dly(dly + hum), 62, LKS_I_SWEEP, 2, 260);
            if (step == 0) schedule_hit(lks_dly(dly + hum), 36, LKS_I_KICK, 3, 150);
            if (step == 10 && chance(70)) schedule_hit(lks_dly(dly + hum), 36, LKS_I_KICK, 2, 140);
            if ((step == 2 || step == 6 || step == 10 || step == 14) && lks_lvl >= 2)
                schedule_hit(lks_dly(dly + swing + hum), 90, LKS_I_HAT, 1, 30);
        } else {
            if ((step & 1) == 0) schedule_hit(lks_dly(dly + hum), 74, LKS_I_RIDE, 2, 160);
            if (step == 4 || step == 12)
                schedule_hit(lks_dly(dly + hum), 66, LKS_I_SWEEP, 3, 60);   // the backbeat tap
            if (step == 0 || step == 8) schedule_hit(lks_dly(dly + hum), 38, LKS_I_KICK, 4, 140);
            if (step == 6 && chance(60)) schedule_hit(lks_dly(dly + hum), 38, LKS_I_KICK, 3, 130);
            if ((step & 1) && chance(35))
                schedule_hit(lks_dly(dly + swing + hum), 92, LKS_I_HAT, 1, 22);
        }
    }

    // ── the lead's phrase machine ──────────────────────────────────────────
    // lks_lead_end is the HARD bound on a phrase (a cry deliberately does not
    // spend a move, so the move counter alone could never end one); running out
    // of moves early just means the last note is held, which is the mood anyway.
    if (lks_lead_h) {
        if (lks_lead_pre && abs_step >= lks_lead_next)
            lks_lead_move(abs_step);          // a bend in flight always comes back down
        else if (lks_lead_left > 0 && abs_step < lks_lead_end && abs_step >= lks_lead_next)
            lks_lead_move(abs_step);
        else if (abs_step >= lks_lead_end && !lks_lead_pre) {
            lks_lead_release();
            // silence is a layer.  At lvl 0 the steel rests for four bars.
            lks_lead_rest = bar + (lks_lvl <= 0 ? 4 : (lks_lvl == 1 ? 2 : 1));
        }
    }
}

// ── sfx: rate-limited, positioned, out of the music's register ──────────────
static int lks_sfx_cool[SFX_COUNT];      // frame stamp of the last start per kind
static int lks_sfx_frame = -1;           // which frame lks_sfx_budget belongs to
static int lks_sfx_budget = 0;           // starts left this frame

//                                    click deny place build door lock alarm whistle fight meal cash bus
static const short LKS_SFX_GAP[SFX_COUNT] = { 3, 8, 4, 6, 4, 6, 24, 30, 11, 12, 9, 48 };

static int lks_sfx_ok(int kind) {
    if (!lks_ready || kind < 0 || kind >= SFX_COUNT) return 0;
    int f = frame();
    if (f != lks_sfx_frame) { lks_sfx_frame = f; lks_sfx_budget = 3; }
    if (lks_sfx_budget <= 0) return 0;
    if (f - lks_sfx_cool[kind] < LKS_SFX_GAP[kind] && f >= lks_sfx_cool[kind]) return 0;
    lks_sfx_cool[kind] = f;
    lks_sfx_budget--;
    return 1;
}

// distance attenuation for the TAIL hits of a positioned sfx (the first hit goes
// through hit_at(), which does its own pan + distance).  0 = do not bother.
static float lks_sfx_gain(float wx, float wy) {
    float lx = (float)lk_cam_x + (float)screen_w() * 0.5f;
    float ly = (float)lk_cam_y + (float)screen_h() * 0.5f;
    float dx = wx - lx, dy = wy - ly;
    float d2 = dx * dx + dy * dy;
    if (d2 >= LKS_SFX_MAX * LKS_SFX_MAX) return 0.0f;
    if (d2 <= LKS_SFX_REF * LKS_SFX_REF) return 1.0f;
    // g = ref/d, so g² = ref²/d² — computable without a distance.  Then four
    // Newton steps for the root: this module deliberately does not include
    // <math.h> (no other lockup module does), and an sfx volume is an integer
    // 0..7 anyway, so the last bit of precision is unobservable.
    float g2 = (LKS_SFX_REF * LKS_SFX_REF) / d2;
    float g  = 0.5f * (1.0f + g2);
    for (int i = 0; i < 4; i++) g = 0.5f * (g + g2 / g);
    return lks_clampf(g, 0.0f, 1.0f);
}
static int lks_atten(int vol, float g) {
    int v = (int)((float)vol * g + 0.5f);
    return lks_clampi(v, 0, 7);
}

// the body of a sound.  `pos` = 1 → the primary hit is positioned at (wx,wy);
// tails are centred but distance-attenuated (pan on a 40ms tail is inaudible,
// and a whole sfx that is 45 tiles away is culled before it costs a voice).
// `g` is precomputed by the caller so a culled sound never spends the frame's
// sfx budget — otherwise a far-off riot could starve the door under your nose.
static void lks_sfx_body(int kind, int pos, float wx, float wy, float g) {
    // the primary hit: one call, either positioned or not
    #define LKS_P(midi, slot, vol, dur) do { \
        if (pos) hit_at((midi), (slot), (vol), (dur), wx, wy); \
        else     hit((midi), (slot), (vol), (dur)); } while (0)
    // a tail: scheduled, centred, attenuated
    #define LKS_T(ms, midi, slot, vol, dur) do { \
        int v_ = lks_atten((vol), g); if (v_ > 0) schedule_hit((ms), (midi), (slot), v_, (dur)); \
        } while (0)

    switch (kind) {
    case SFX_CLICK:                                 // the UI's own tick, dry and high
        LKS_P(94, LKS_I_TICK, 5, 14);
        break;
    case SFX_DENY:                                  // two notes DOWN — the universal no
        LKS_P(45, LKS_I_THUD, 6, 70);
        LKS_T(95, 40, LKS_I_THUD, 6, 90);
        break;
    case SFX_PLACE:                                 // a thing set down on concrete
        LKS_P(50, LKS_I_THUD, 5, 60);
        LKS_T(24, 90, LKS_I_TICK, 3, 18);
        break;
    case SFX_BUILD:                                 // three scrapes and it's standing
        LKS_P(86, LKS_I_TICK, 5, 26);
        LKS_T(70, 89, LKS_I_TICK, 4, 24);
        LKS_T(140, 78, LKS_I_CLACK, 5, 90);
        break;
    case SFX_DOOR: {                                // a door sounds like the door it is
        int jail = 0;
        if (pos) {
            int tx = (int)(wx / (float)LK_TS), ty = (int)(wy / (float)LK_TS);
            if (lk_in(tx, ty)) {
                int dr = lk_t[lk_idx(tx, ty)].door;
                jail = (dr == DR_JAIL || dr == DR_GATE);
            }
        }
        if (jail) {                                 // steel sliding, then the stop
            LKS_P(58, LKS_I_ROLL, 6, 240);
            LKS_T(230, 79, LKS_I_CLACK, 5, 110);
        } else {                                    // a hinge, complaining
            LKS_P(66, LKS_I_CREAK, 5, 180);
            LKS_T(170, 48, LKS_I_THUD, 3, 70);
        }
    } break;
    case SFX_LOCK:                                  // the bolt, twice
        LKS_P(80, LKS_I_CLACK, 6, 60);
        LKS_T(75, 77, LKS_I_CLACK, 5, 80);
        break;
    case SFX_ALARM:                                 // the klaxon stab; the hold is §siren
        LKS_P(81, LKS_I_SIREN, 6, 220);
        LKS_T(200, 76, LKS_I_SIREN, 6, 260);
        lks_siren_hold = 200;
        break;
    case SFX_WHISTLE:                               // two shrill chirps, up top
        LKS_P(100, LKS_I_WHIST, 6, 120);
        LKS_T(150, 103, LKS_I_WHIST, 6, 170);
        break;
    case SFX_FIGHT:                                 // a body, then a scuffle
        LKS_P(43, LKS_I_THUD, 7, 130);
        LKS_T(60, 88, LKS_I_TICK, 4, 40);
        LKS_T(120, 84, LKS_I_TICK, 4, 34);
        LKS_T(190, 41, LKS_I_THUD, 5, 120);
        break;
    case SFX_MEAL:                                  // tray on rail, ladle on tray
        LKS_P(86, LKS_I_CHIME, 5, 260);
        LKS_T(90, 92, LKS_I_TICK, 3, 22);
        LKS_T(150, 89, LKS_I_TICK, 3, 20);
        break;
    case SFX_CASH:                                  // the register, up a fifth
        LKS_P(84, LKS_I_CHIME, 6, 240);
        LKS_T(85, 91, LKS_I_CHIME, 5, 380);
        break;
    case SFX_BUS:                                   // diesel, then the air brake
        LKS_P(28, LKS_I_RUMBLE, 6, 1100);
        LKS_T(760, 96, LKS_I_TICK, 4, 300);
        LKS_T(980, 33, LKS_I_THUD, 5, 180);
        break;
    default: break;
    }
    #undef LKS_P
    #undef LKS_T
}

// ── the held siren (the pinned recipe: a SQUARE warbled by note_lfo LFO_PITCH)
static void lks_siren_update(float d) {
    int want = (lk.alarm >= AL_RIOT) || lks_siren_hold > 0;
    if (lks_siren_hold > 0) lks_siren_hold--;
    if (want && !lks_siren_h) {
        lks_siren_h = note_on(81, LKS_I_SIREN, 0);
        if (lks_siren_h) {
            note_lfo(lks_siren_h, 0, LFO_PITCH, 0.75f, 3.2f);
            lks_siren_lock = -1;
        }
        lks_siren_v = 0.0f;
    }
    if (lks_siren_h) {
        // lockdown swaps the warble for something slower and heavier
        int lock = (lk.alarm >= AL_LOCKDOWN) ? 1 : 0;
        if (lock != lks_siren_lock) {
            lks_siren_lock = lock;
            note_lfo(lks_siren_h, 0, LFO_PITCH, lock ? 0.38f : 0.75f, lock ? 4.6f : 3.2f);
            note_pitch(lks_siren_h, lock ? 74.0f : 81.0f);
        }
        // the ramp ALWAYS advances; only the note_vol() push is change-gated, so
        // the fade can actually reach 0 instead of stalling an epsilon above it
        lks_siren_v = lks_glide(lks_siren_v, want ? 5.5f : 0.0f, 4.0f, d);
        if (lks_siren_v > lks_siren_vp + 0.02f || lks_siren_v < lks_siren_vp - 0.02f) {
            note_vol(lks_siren_h, lks_siren_v);
            lks_siren_vp = lks_siren_v;
        }
        if (!want && lks_siren_v <= 0.001f) {
            note_off(lks_siren_h);
            lks_siren_h = 0;
            lks_siren_vp = -1.0f;
        }
    }
}

// ── the air layer (cell-block HVAC) — on at rest, and back for the aftermath ─
static void lks_air_update(float d) {
    if (!lks_air_h) return;
    int night = (lk.clock < 6.0f || lk.clock >= 21.0f);
    float tv = 0.0f;
    if (lks_lvl <= 0 || lks_desk) tv = night ? 2.6f : 2.0f;
    if (lk.over) tv = 2.2f;
    lks_air_v = lks_glide(lks_air_v, tv, 1.2f, d);      // the ramp always advances…
    if (lks_air_v > lks_air_vp + 0.02f || lks_air_v < lks_air_vp - 0.02f) {
        note_vol(lks_air_h, lks_air_v);                 // …only the push is gated
        lks_air_vp = lks_air_v;
    }
}

// ── the debug readout rows (hand-formatted; no stdio, no allocation) ────────
static int lks_put(char *dst, int at, int cap, const char *s) {
    while (*s && at < cap - 1) dst[at++] = *s++;
    dst[at] = 0;
    return at;
}
static int lks_puti(char *dst, int at, int cap, int v) {
    char tmp[12];
    int n = 0;
    if (v < 0) { if (at < cap - 1) dst[at++] = '-'; v = -v; }
    do { tmp[n++] = (char)('0' + (v % 10)); v /= 10; } while (v > 0 && n < 11);
    while (n > 0) { n--; if (at < cap - 1) dst[at++] = tmp[n]; }
    dst[at] = 0;
    return at;
}
static void lks_row_stem(int i, const char *label, int on, const char *how) {
    int at = lks_put(lks_row[i], 0, 24, label);
    while (at < 7) { lks_row[i][at++] = ' '; lks_row[i][at] = 0; }
    lks_put(lks_row[i], at, 24, on ? (how ? how : "on") : "--");
}
static void lks_rows_build(void) {
    lks_row_dirty = 0;
    int at = lks_put(lks_row[0], 0, 24, "lvl ");
    at = lks_puti(lks_row[0], at, 24, lks_lvl);
    at = lks_put(lks_row[0], at, 24, "  ");
    lks_put(lks_row[0], at, 24, LKS_LVLNAME[lks_clampi(lks_lvl, 0, 3)]);

    at = lks_puti(lks_row[1], 0, 24, lks_bpm_now);
    at = lks_put(lks_row[1], at, 24, "bpm ");
    at = lks_put(lks_row[1], at, 24, LKS_PCN[lks_pcs[0] % 12]);
    at = lks_put(lks_row[1], at, 24, hb_qname[lks_clampi(lks_qual, 0, HB_NQUAL - 1)]);
    at = lks_put(lks_row[1], at, 24, lks_minor ? " cin" : " folk");

    lks_row_stem(2, "drone", 1, lks_lvl >= 2 ? "moves" : "hum");
    lks_row_stem(3, "air",   (lks_lvl <= 0 || lks_desk), "hvac");
    lks_row_stem(4, "lead",  lks_on[LKS_ST_LEAD] && lks_lead_h,
                 lks_lead_slot == LKS_I_HARM ? "harp" : "steel");
    lks_row_stem(5, "bass",  lks_on[LKS_ST_BASS] && lks_lvl >= 1, "pizz");
    lks_row_stem(6, "keys",  lks_on[LKS_ST_EP] && lks_lvl >= 1, "rhodes");
    lks_row_stem(7, "kit",   lks_on[LKS_ST_KIT] && lks_lvl >= 1,
                 lks_kit_stick == 1 ? "sticks" : "brush");
    if (!lks_desk) lks_row_stem(8, "desk", 0, 0);
    else {
        at = lks_put(lks_row[8], 0, 24, "desk   ");
        at = lks_puti(lks_row[8], at, 24, lks_desk_left);
        lks_put(lks_row[8], at, 24, lks_throw_f > 0 ? " throw" : " bars");
    }
}

// ═══ PUBLIC ═════════════════════════════════════════════════════════════════

void lk_score_init(void) {
    // deterministic composition seed: the same prison always has the same tune
    lks_rng = (unsigned)lk.seed * 2654435761u ^ 0x10CC0DEu;
    if (!lks_rng) lks_rng = 1u;
    lks_key = lks_srnd(12);

    lks_sched = -1;
    lks_base  = 0;
    lks_bar   = -1;
    lks_chord_bar = -1;
    lks_lvl   = 0;
    lks_minor = 0;
    lks_ep_init = 0;
    lks_bassm = 40;
    lks_lead_h = 0;
    lks_lead_slot = LKS_I_STEEL;
    lks_lead_midi = 64.0f;
    lks_lead_rest = 0;
    lks_lead_pre = 0;
    lks_desk = 0;
    lks_desk_left = 0;
    lks_desk_phase = -1;
    lks_throw_f = 0;
    lks_siren_h = 0;
    lks_siren_v = 0.0f;
    lks_siren_vp = -1.0f;
    lks_siren_hold = 0;
    lks_siren_lock = -1;
    lks_air_v = 0.0f;
    lks_air_vp = -1.0f;
    lks_kit_stick = -1;
    lks_tone = 0.62f;
    lks_tone_done = -1.0f;
    lks_muffle = 0.0f;
    lks_muffle_idle = 1;
    for (int i = 0; i < LKS_NSTEM; i++) lks_on[i] = 1;
    for (int i = 0; i < SFX_COUNT; i++) lks_sfx_cool[i] = -999;
    // shadows start impossible so the first apply_fx issues everything once
    for (int i = 0; i < LKS_SLOTS; i++) {
        lks_sh_level[i] = -1.0f; lks_sh_rev[i] = -1.0f; lks_sh_echo[i] = -1.0f;
    }

    lks_bpm_now = LKS_BPM_CALM;
    lks_bpm_f   = (float)LKS_BPM_CALM;
    bpm(LKS_BPM_CALM);

    // ── the band ────────────────────────────────────────────────────────────
    // the drone: felt, not heard.  A long swell, a long tail, no top at all.
    instrument(LKS_I_DRONE, INSTR_SINE, 900, 400, 6, 2000);
    instrument_pan(LKS_I_DRONE, 0.0f);
    // the air: band-passed weather, indoors
    instrument(LKS_I_AIR, INSTR_NOISE, 2000, 800, 5, 2500);
    instrument_lfo(LKS_I_AIR, 0, LFO_CUTOFF, 0.06f, 180.0f);   // the building breathing
    instrument_pan(LKS_I_AIR, -0.15f);
    // the pedal steel: a real string with a very long ring, picked once a phrase
    instrument(LKS_I_STEEL, INSTR_PLUCK, 1, 0, 7, 1400);
    instrument_harmonics(LKS_I_STEEL, 0.75f);      // ring time — it must sing for bars
    instrument_timbre(LKS_I_STEEL, 0.45f);         // a bar, not a hard pick
    instrument_morph(LKS_I_STEEL, 0.15f);          // near the bridge
    instrument_pan(LKS_I_STEEL, -0.22f);
    // the harmonica: a reed near clarinet, pushed a little for the buzz
    instrument(LKS_I_HARM, INSTR_REED, 40, 0, 6, 260);
    instrument_harmonics(LKS_I_HARM, 0.05f);
    instrument_timbre(LKS_I_HARM, 0.42f);
    instrument_morph(LKS_I_HARM, 0.40f);
    instrument_pan(LKS_I_HARM, 0.24f);
    // the upright: the bowed string, PLUCKED, with its body up — woody, and it rings
    instrument(LKS_I_BASS, INSTR_BOWED, 6, 0, 7, 520);
    instrument_mode(LKS_I_BASS, MODE_BOW_PIZZ, 1.0f);
    instrument_mode(LKS_I_BASS, MODE_BOW_BODY, 0.70f);
    instrument_harmonics(LKS_I_BASS, 0.35f);
    instrument_timbre(LKS_I_BASS, 0.25f);
    instrument_morph(LKS_I_BASS, 0.40f);
    instrument_pan(LKS_I_BASS, 0.05f);
    // the suitcase: a Rhodes, and the tremolo is ours to run
    instrument(LKS_I_EP, INSTR_EPIANO, 1, 0, 7, 700);
    instrument_harmonics(LKS_I_EP, 0.15f);
    instrument_timbre(LKS_I_EP, 0.20f);
    instrument_morph(LKS_I_EP, 0.12f);
    instrument_lfo(LKS_I_EP, 0, LFO_VOLUME, 5.0f, 0.45f);
    instrument_pan(LKS_I_EP, 0.20f);
    lks_voice_kit(0);                              // brushes to start

    // ── sfx patches (fixed; never retuned at call time) ────────────────────
    instrument(LKS_I_SIREN, INSTR_SQUARE, 24, 0, 6, 240);
    instrument_duty(LKS_I_SIREN, 0.34f);
    instrument_filter(LKS_I_SIREN, FILTER_LOW, 2400, 3);
    instrument(LKS_I_TICK, INSTR_NOISE, 0, 18, 0, 12);
    instrument_filter(LKS_I_TICK, FILTER_HIGH, 6800, 3);
    instrument(LKS_I_CLACK, INSTR_SQUARE, 0, 40, 0, 50);
    instrument_filter(LKS_I_CLACK, FILTER_BAND, 2600, 9);
    instrument_env(LKS_I_CLACK, 0, ENV_PITCH, 0, 22, 9.0f);
    instrument(LKS_I_CREAK, INSTR_TRI, 20, 120, 1, 90);
    instrument_filter(LKS_I_CREAK, FILTER_BAND, 1500, 7);
    instrument_env(LKS_I_CREAK, 0, ENV_PITCH, 30, 180, 5.0f);   // the hinge complaining up
    instrument(LKS_I_ROLL, INSTR_NOISE, 8, 200, 2, 120);
    instrument_filter(LKS_I_ROLL, FILTER_BAND, 2200, 8);
    instrument_env(LKS_I_ROLL, 0, ENV_CUTOFF, 10, 220, 1600.0f);
    instrument(LKS_I_THUD, INSTR_SINE, 0, 110, 0, 70);
    instrument_filter(LKS_I_THUD, FILTER_LOW, 400, 2);
    instrument_env(LKS_I_THUD, 0, ENV_PITCH, 0, 60, 10.0f);
    instrument(LKS_I_WHIST, INSTR_SQUARE, 8, 0, 6, 90);
    instrument_duty(LKS_I_WHIST, 0.18f);
    instrument_filter(LKS_I_WHIST, FILTER_BAND, 3400, 6);
    instrument_lfo(LKS_I_WHIST, 0, LFO_PITCH, 12.0f, 0.35f);    // the pea in the whistle
    instrument(LKS_I_CHIME, INSTR_MALLET, 0, 0, 7, 500);
    instrument_harmonics(LKS_I_CHIME, 0.70f);
    instrument_timbre(LKS_I_CHIME, 0.55f);
    instrument_morph(LKS_I_CHIME, 0.35f);
    instrument(LKS_I_RUMBLE, INSTR_NOISE, 260, 500, 4, 700);
    instrument_filter(LKS_I_RUMBLE, FILTER_LOW, 220, 4);

    // ── the space.  Two tanks: a long spring plate for the lead, a room for the
    // rest, both with the melancholy setting; the sfx stay mostly dry.
    reverb(0.92f, 0.35f);
    reverb_bus(1, 0.96f, 0.32f);
    reverb_spring(0.32f);            // Americana twang, not a concert hall
    reverb_spring_tone(0.62f);
    echo(560, 0.24f, 0.30f);
    eq(2.0f, -1.0f, -2.0f);
    spatial_model(LKS_SFX_REF, LKS_SFX_MAX, 1.0f);

    lks_apply_voicing(lks_tone);
    lks_chord_next(1);                              // seat the tonic
    lks_ready = 1;
    lks_apply_fx();

    // the two forever-voices
    lks_drone_h = note_on(lks_near_pc(lks_key, 30, 26, 34), LKS_I_DRONE, 3);
    if (lks_drone_h) {
        note_glide(lks_drone_h, 2400);
        note_glide_scale(lks_drone_h, GLIDE_ANALOG);
        note_lfo(lks_drone_h, 0, LFO_PITCH, 0.05f, 0.05f);      // a slow, tired wow
    }
    lks_air_h = note_on(43, LKS_I_AIR, 0);
    lks_air_v = 0.0f;
    lks_rows_build();
}

void lk_score_update(float d) {
    if (!lks_ready) return;
    if (d < 0.0f) d = 0.0f;
    if (d > 0.10f) d = 0.10f;      // the sim may hand us a speed-scaled delta; music is wall-clock

    // the ears sit at the middle of the view, so every positioned sfx is placed
    // relative to what the player is looking at.  Called FIRST, before any sfx.
    listener((float)lk_cam_x + (float)screen_w() * 0.5f,
             (float)lk_cam_y + (float)screen_h() * 0.5f);

    if (lks_throw_f > 0) lks_throw_f--;

    // ── tempo: a live lever, glided so a tension change is a push and not a cut.
    float tt = (float)LKS_BPM_CALM;
    if (lks_lvl >= 2) tt = (float)LKS_BPM_RIOT;
    if (lks_desk) {                                 // the rallentando, then the revival
        int L = lks_desk_left;
        tt = L > 5 ? 56.0f : (L > 3 ? 50.0f : (L > 2 ? 52.0f : (L > 1 ? 56.0f : 60.0f)));
    }
    if (lk.over) tt = 48.0f;
    lks_bpm_f = lks_glide(lks_bpm_f, tt, 5.0f, d);
    int bi = (int)(lks_bpm_f + 0.5f);
    if (bi != lks_bpm_now) { lks_bpm_now = bi; bpm(bi); lks_row_dirty = 1; }

    // ── the tone axis: mellow at rest, open in a riot, shut down afterwards.
    float ttone = 0.62f + 0.21f * (float)lks_lvl;   // 0.62 · 0.83 · 1.04 · 1.25
    if (lks_desk) ttone = 0.55f;
    if (lk.clock < 6.0f || lk.clock >= 21.0f) ttone *= 0.90f;   // night is duller
    if (lk.over) ttone = 0.50f;
    lks_tone = lks_glide(lks_tone, lks_clampf(ttone, 0.45f, 1.30f), 0.35f, d);
    lks_apply_voicing(lks_tone);

    // ── the schedule-ahead clock.  Never trigger on the frame.
    double pos = (double)beat() * 4.0 + (double)beat_pos() * 4.0;
    lks_stepms = 60000.0 / ((double)lks_bpm_now * 4.0);
    if (lks_sched < 0) { lks_sched = (long)pos; lks_base = (long)pos + 8; }
    long target = (long)pos + 1;
    if (target - lks_sched > 32) lks_sched = target - 4;   // resync after a stall, don't machine-gun
    while (lks_sched < target) { lks_sched++; lks_step(lks_sched, pos); }

    lks_siren_update(d);
    lks_air_update(d);
    lks_apply_fx();

    // ── the master filter is used for ONE thing: the muffle.  Paused or over,
    // the whole mix (sfx included) goes behind glass; otherwise it is bypassed
    // outright so it never dulls a click.  filter() is a live-ride lever.
    float mt = (lk.speed == 0) ? 1.0f : 0.0f;
    if (lk.over) mt = 0.70f;
    lks_muffle = lks_glide(lks_muffle, mt, 2.2f, d);
    if (lks_muffle > 0.004f) {
        filter(FILTER_LOW, 18000.0f - 16600.0f * lks_muffle, 0.05f);
        lks_muffle_idle = 0;
    } else if (!lks_muffle_idle) {
        filter(FILTER_OFF, 18000.0f, 0.0f);
        lks_muffle_idle = 1;
    }

    if (lks_row_dirty) lks_rows_build();

#ifdef DE_TRACE
    watch("mus_lvl",  "%d", lks_lvl);
    watch("mus_bpm",  "%d", lks_bpm_now);
    watch("mus_fn",   "%d", lks_fn);
    watch("mus_min",  "%d", lks_minor);
    watch("mus_desk", "%d", lks_desk);
    watch("mus_lead", "%d", lks_lead_h ? lks_lead_slot : 0);
#endif
}

void lk_sfx(int kind) {
    if (!lks_sfx_ok(kind)) return;
    lks_sfx_body(kind, 0, 0.0f, 0.0f, 1.0f);
}

void lk_sfx_at(int kind, float wx, float wy) {
    if (!lks_ready) return;
    float g = lks_sfx_gain(wx, wy);          // cull BEFORE spending the budget
    if (g <= 0.02f) return;
    if (!lks_sfx_ok(kind)) return;
    lks_sfx_body(kind, 1, wx, wy, g);
}

const char *lk_score_layer_name(int i) {
    if (i < 0 || i >= LKS_NROW) return "";
    return lks_row[i];
}

int lk_score_layers(void) { return LKS_NROW; }

#endif // LOCKUP_SCORE_H
