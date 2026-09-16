/* de:meta
{
  "slug": "cloudhold",
  "title": "cloud hold",
  "status": "active",
  "created": "2026-09-15",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "granular-synth",
    "gesture-loop"
  ],
  "resizable": true,
  "collection": [
    "device-face",
    "responsive"
  ],
  "lineage": "Compound-blindspot §6 product cart: freeze as a performed gesture on existing FX_GRAINS. Hold a chord, lock the cloud, play over it, unfreeze. Boutique-pedal identity — not the grains lab, not grainchop's sampler spike, not a new FX_*. ADR-0015 / #29. CLOUD / GLASS / DUST are grain-recipe stomps (size/density/scatter/pitch-spread), not source pickers.",
  "description": {
    "summary": "Hold a chord, freeze, the room becomes a pad. Play over the cloud, then unfreeze.",
    "detail": "The product cart for grains_freeze: one gesture, existing FX_GRAINS. Tap a chord pad to hold a saw triad into the granular cloud (or hold 1-4). Stomp FREEZE — capture stops, the buffer loops, the dry chord drops away, and the room is now a pad. CLOUD / GLASS / DUST retune how that capture is scattered (wash / sparkle / smear) — same freeze, different room. Play the keybed over that cloud. Stomp again to recapture. SHIMMER transposes the frozen grains up an octave. Not a teaching demo with a hidden freeze knob: the stomp is the instrument. Cousin of grains (the lab) and grainchop (the sampler spike); this one is the boutique pedal.",
    "controls": "Tap a chord pad to hold it. CLOUD / GLASS / DUST pick the grain material. Stomp FREEZE to lock the cloud; stomp again to recapture. Play the keys over the pad. SHIMMER = octave-up grains. 1-4 hold chords, 5-7 materials, SPACE freeze, Q shimmer. Keybed: A-K white, W-E-T-Y-U-O-P black, Z/X octave (+ mouse/touch/MIDI)."
  }
}
de:meta */
#include "studio.h"
#include "keybed.h"
#include "ui.h"
#include "face.h"
#include <math.h>

// CLOUD HOLD — compound-blindspot §6. Freeze is something you perform.
//
//   hold a chord pad  →  the triad feeds instrument_grains on the pad slot
//   stomp FREEZE      →  instrument_grains_freeze: write head stops, dry chord drops,
//                        the cloud is now the room
//   play the keybed   →  a dry lead over the frozen pad (lead slot never enters the tank)
//   stomp again       →  recapture
//
// Boutique pedal, not a lab. CLOUD / GLASS / DUST are the three materials: they
// retune grain size / density / scatter / pitch-spread (same capture, different
// room). SHIMMER is the one extra: +12 grains_pitch. No new FX_*, no INSTR_* buffet.

#define SL_PAD   6
#define SL_LEAD  5
#define NCHORD   4
#define NMAT     3
#define NGRAIN   48
#define DESIGN_W 200
#define DESIGN_H 320

// The lead has to sit ON TOP of the room it plays over. It used to peak 6 dB
// UNDER the frozen cloud (-15.3 vs -9.3 dBFS), so playing the keybed added only
// about 1 dB to the sum and "play over the cloud" never landed. Both slots stay
// at UNITY and the balance comes from the one honest knob: once the chord is
// released the frozen cloud is pure wet, so the grain MIX *is* its volume.
#define LEAD_LEVEL 1.00f

enum { MAT_CLOUD = 0, MAT_GLASS, MAT_DUST };

typedef struct {
    float grain_ms, density, position, scatter, feedback, mix, spread;
} Material;

// Same freeze gesture; only the scatter recipe changes.
//   CLOUD — mid grains, dense wet wash (the default room)
//   GLASS — tiny grains, high density, sparkle detune
//   DUST  — long grains, sparse, wide pitch wander
//
// MIX IS THE WHOLE GESTURE, and it used to be wrong. At 0.92 the insert had
// already replaced 92% of the dry triad BEFORE the stomp, so "the dry chord
// drops away" was inaudible: measured, frozen and never-frozen stayed within
// ±1.3 dB for 0.8 s and only parted ~1.6 s later, as an ABSENCE (the un-frozen
// one decays, this one doesn't). The screen flipped instantly and the ears
// heard nothing, which reads as broken.
//
// Around half the dry chord is genuinely in the room while LIVE, so taking it
// away IS the event, and the saw edge leaving is as loud a cue as the level
// step. Mix doubles as the frozen cloud's volume (post-release it is pure wet),
// which is what seats the lead above it.
//
// DENSITY x SIZE IS THE OVERLAP, and the overlap is whether you hear GRAINS at
// all. Above 1 the Hann windows sum back into a continuous tone and the engine
// then divides by sqrt(overlap), so it deliberately smooths what is left: at
// the shipped 32 grains/s x 170 ms = 5.4x this cart WAS a plain saw pad behind
// a lowpass, which is exactly what it sounded like. Measured spread of the
// short-window level, filter pinned so only the grains move:
//     overlap 5.44 (shipped) →  1.7 dB     a drone
//     overlap 1.68           →  2.3 dB
//     overlap 1.08           →  8.2 dB     breathing, still continuous
//     overlap 0.78           → 13.0 dB     pointillist, you hear each grain
// CLOUD sits at 1.08 because it still has to be a pad you play over; GLASS and
// DUST go under 1 because sparkle and smear ARE the separate grains.
//
// It also matters that the source is a SUSTAINED chord: scatter and position
// read a 3 s buffer holding the same steady tone, so they move phase and not
// texture. On a drone the recipe's audible axes are overlap and spread — and
// spread is the ONE the engine clamps, to 0..1 (a semitone), so 0.9 is as wide
// as a grain cloud here can be.
//
// ⚠ CALIBRATION BUILD, DELIBERATELY OVER THE TOP. The maker could still not hear
// grains at overlap 1.08, so these are set where the grains are separate EVENTS
// you cannot miss — measured duty cycle (how much of the time anything is
// sounding at all, frozen, 100% wet):
//     overlap 5.44 (shipped) → 94%   a continuous drone
//     overlap 1.08           → 80%
//     overlap 0.54           → 40%
//     overlap 0.18           → 13%   pointillist
//     overlap 0.09           →  8%   droplets
// Mix is up at 0.85 too, so the cloud is what you hear rather than the chord.
// That trades away most of the FREEZE step this same cart just gained, so once
// the grain rate is settled by ear, mix comes back down to ~0.5.
static const Material MAT[NMAT] = {
    {  60.0f,  3.0f, 0.86f, 0.80f, 0.30f, 0.85f, 1.00f },
    {  25.0f,  6.0f, 0.78f, 0.95f, 0.25f, 0.85f, 1.00f },
    { 400.0f,  1.2f, 0.32f, 0.90f, 0.18f, 0.85f, 0.80f },
};
static const char *MNAME[NMAT] = { "CLOUD", "GLASS", "DUST" };

static const int CHORD[NCHORD][3] = {
    { 60, 64, 67 },   // C
    { 57, 60, 64 },   // Am
    { 53, 57, 60 },   // F
    { 55, 59, 62 },   // G
};
static const char *CNAME[NCHORD] = { "C", "Am", "F", "G" };

typedef struct { float x, y, vx, vy, age, life; int on; } Speck;
static Speck speck[NGRAIN];
static float spawn_acc;

static int   frozen;
static int   lift;              // 1 = +12 grains_pitch (the SHIMMER stomp)
static int   material = MAT_CLOUD;
static int   pad_h[3] = { -1, -1, -1 };
static int   sounding = -1;     // dry chord currently on, or -1
static int   latch    = -1;     // tap-to-latch (one thumb)
static int   cloud_i  = 0;      // last chord that fed the tank (the frozen identity)
static int   a_frozen = -1, a_lift = -1, a_mat = -1;
static float stomp_t  = -1.0f;  // seconds since the last stomp, <0 = idle
static float duck     = 1.0f;   // stomp transient: scales BOTH legs of the pad
static float a_mix    = -1.0f;
static float a_pad_lv = -1.0f;

static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.11f, "nav"    },
    { FACE_HERO, 0,           0.00f, "cloud"  },
    { FACE_BAND, EDGE_BOTTOM, 0.13f, "chords" },
    { FACE_BAND, EDGE_BOTTOM, 0.12f, "mats"   },
    { FACE_BAND, EDGE_BOTTOM, 0.16f, "stomp"  },
};
#define NZ 5
static Face g_face;
static Box  z_nav, z_cloud, z_chords, z_mats, z_stomp;
static Box  pad_box[NCHORD];
static Box  mat_box[NMAT];

static void release_pad(void) {
    for (int i = 0; i < 3; i++) {
        if (pad_h[i] >= 0) note_off(pad_h[i]);
        pad_h[i] = -1;
    }
    sounding = -1;
}

static void play_chord(int ci) {
    if (ci < 0 || ci >= NCHORD) return;
    release_pad();
    for (int i = 0; i < 3; i++) pad_h[i] = note_on(CHORD[ci][i], SL_PAD, 6);
    sounding = ci;
    cloud_i  = ci;
}

static void set_frozen(int on) {
    if (frozen == on) return;
    frozen = on;
    instrument_grains_freeze(SL_PAD, on ? 1 : 0);
    if (on) release_pad();
    stomp_t = 0.0f;   // the stomp gets a transient of its own (ride_stomp)
}

// A real pedal makes a noise when you step on it. Freezing is a state change and
// not an attack, so the pad DUCKS on the switch and blooms back: that is what
// turns "a label changed" into "I did that".
//
// It has to duck BOTH legs, and neither one alone works — that cost two rounds:
//   instrument_level scales the VOICE contribution, upstream of the insert, so
//     it cannot touch a frozen cloud at all (the tank is looping, not being fed).
//   the grain MIX is the cloud's volume once the chord is gone, but on its own a
//     duck just trades wet for DRY, and the pad's release is 1200 ms, so the
//     triad is still right there — measured, ducking mix alone made the stomp
//     1.8 dB LOUDER.
// Scaled together they take the dry leg and the wet leg down at once, which is
// the gap you hear. fx_set_grains only assigns fields once the tank exists and
// both writes are change-gated, so this stays set-and-hold safe.
static void ride_stomp(float dt) {
    duck = 1.0f;
    if (stomp_t >= 0.0f) {
        stomp_t += dt;
        if      (stomp_t < 0.07f) duck = 1.00f - 0.88f * (stomp_t / 0.07f);
        else if (stomp_t < 0.42f) duck = 0.12f + 0.88f * ((stomp_t - 0.07f) / 0.35f);
        else                      stomp_t = -1.0f;
    }
    if (fabsf(duck - a_pad_lv) > 0.003f) { instrument_level(SL_PAD, duck); a_pad_lv = duck; }
}

static void apply_voice(void) {
    const Material *m = &MAT[material];
    if (a_mat != material) {
        // slight pad colour into the next capture (does not recapture a freeze)
        instrument_filter(SL_PAD, FILTER_LOW,
            material == MAT_GLASS ? 4200 : material == MAT_DUST ? 1200 : 2400, 1);
        a_mat  = material;
        a_lift = -1;   // re-apply pitch so the new spread lands
        a_mix  = -1.0f;   // and re-push the recipe below
    }
    float mix = m->mix * duck;
    if (fabsf(mix - a_mix) > 0.003f) {
        instrument_grains(SL_PAD, m->grain_ms, m->density, m->position,
                          m->scatter, m->feedback, mix);
        a_mix = mix;
    }
    if (a_frozen != frozen) {
        instrument_grains_freeze(SL_PAD, frozen ? 1 : 0);
        a_frozen = frozen;
    }
    if (a_lift != lift) {
        instrument_grains_pitch(SL_PAD, lift ? 12.0f : 0.0f, MAT[material].spread, 0);
        a_lift = lift;
    }
}

static void relayout(void) {
    face_resize_to(DESIGN_W, DESIGN_H);
    Box area = face_area(3);
    g_face   = face_layout(area, ZONES, NZ, 8);
    z_nav    = g_face.box[0];
    z_cloud  = g_face.box[1];
    z_chords = g_face.box[2];
    z_mats   = g_face.box[3];
    z_stomp  = g_face.box[4];

    float gap = 3.0f;
    float pw  = (z_chords.w - gap * (NCHORD + 1)) / NCHORD;
    if (pw < 16.0f) pw = 16.0f;
    for (int i = 0; i < NCHORD; i++) {
        pad_box[i] = box(z_chords.x + gap + i * (pw + gap),
                         z_chords.y + 2.0f, pw, z_chords.h - 4.0f);
    }

    float mw = (z_mats.w - gap * (NMAT + 1)) / NMAT;
    if (mw < 28.0f) mw = 28.0f;
    for (int i = 0; i < NMAT; i++) {
        mat_box[i] = box(z_mats.x + gap + i * (mw + gap),
                         z_mats.y + 2.0f, mw, z_mats.h - 4.0f);
    }

    // keybed sits in the lower half of the cloud window — always playable,
    // visually the "play over" surface once the room is frozen
    int kh = (int)z_cloud.h / 2;
    if (kh < 28) kh = (int)z_cloud.h - 8;
    if (kh < 20) kh = 20;
    keybed_layout((int)z_cloud.x + 2, (int)z_cloud.y + (int)z_cloud.h - kh - 2,
                  (int)z_cloud.w - 4, kh);
}

static void spawn_speck(void) {
    for (int i = 0; i < NGRAIN; i++) if (!speck[i].on) {
        float sx = (float)((i * 2654435761u) >> 8 & 1023) / 1023.0f - 0.5f;
        speck[i].x    = z_cloud.x + z_cloud.w * 0.5f + sx * z_cloud.w * 0.55f;
        speck[i].y    = z_cloud.y + z_cloud.h * 0.35f + (float)((i * 40503u) >> 4 & 31);
        speck[i].vx   = sx * (material == MAT_DUST ? 16.0f : 10.0f);
        speck[i].vy   = -8.0f - (frozen ? 4.0f : 0.0f)
                      - (material == MAT_GLASS ? 6.0f : 0.0f);
        speck[i].age  = 0.0f;
        speck[i].life = (material == MAT_DUST ? 0.70f : 0.45f)
                      + (lift ? 0.25f : 0.0f);
        speck[i].on   = 1;
        return;
    }
}

void init(void) {
    // saw triad — louder / more characterful than the old quiet TRI so the tank
    // captures a room, not a thin tone. Warm lowpass keeps it pad, not lead.
    instrument(SL_PAD,  INSTR_SAW,  50, 280, 8, 1200);
    instrument_filter(SL_PAD, FILTER_LOW, 2400, 1);
    instrument(SL_LEAD, INSTR_SAW,   6,  90, 5, 220);
    instrument_level(SL_LEAD, LEAD_LEVEL);
    keybed_config(SL_LEAD, 4, 8);   // one octave of whites — fat enough on a phone
    keybed_velocity(7);             // the lead plays OVER the room, so it leads
    apply_voice();                  // CLOUD recipe + pitch
}

void update(void) {
    relayout();
    keybed_update();

    if (keyp(' ')) set_frozen(!frozen);   // SPACE only — F is a keybed white key
    if (keyp('Q')) lift = !lift;
    if (keyp('5')) material = MAT_CLOUD;
    if (keyp('6')) material = MAT_GLASS;
    if (keyp('7')) material = MAT_DUST;

    // keys 1-4 = momentary hold (the scripted "hold a chord" path).
    // pad taps latch in draw() so one thumb can tap a chord then stomp FREEZE.
    int want = -1;
    if      (key('1')) want = 0;
    else if (key('2')) want = 1;
    else if (key('3')) want = 2;
    else if (key('4')) want = 3;
    if (want < 0 && latch >= 0) want = latch;

    // freeze = the cloud is the pad. dry triad drops so you can hear the lock.
    int feed = frozen ? -1 : want;
    if (feed != sounding) {
        if (feed >= 0) play_chord(feed);
        else           release_pad();
    }
    if (!frozen && want >= 0) cloud_i = want;

    float dt = 1.0f / 60.0f;
    ride_stomp(dt);   // before apply_voice: it sets the duck the recipe is scaled by
    apply_voice();

    // visual swarm — denser while a chord is feeding or the cloud is locked;
    // GLASS sprays faster, DUST hangs fewer specks
    float dens = (frozen || sounding >= 0) ? 22.0f : 4.0f;
    if (material == MAT_GLASS) dens *= 1.6f;
    if (material == MAT_DUST)  dens *= 0.55f;
    spawn_acc += dens * dt;
    while (spawn_acc >= 1.0f) { spawn_acc -= 1.0f; spawn_speck(); }
    for (int i = 0; i < NGRAIN; i++) if (speck[i].on) {
        speck[i].age += dt;
        speck[i].x += speck[i].vx * dt;
        speck[i].y += speck[i].vy * dt;
        if (speck[i].age >= speck[i].life) {
            if (frozen) {
                speck[i].age = 0.0f;
                speck[i].y = z_cloud.y + z_cloud.h * 0.35f;
            } else {
                speck[i].on = 0;
            }
        }
    }

#ifdef DE_TRACE
    watch("frozen", "%d", frozen);
    watch("lift", "%d", lift);
    watch("material", "%d", material);
    watch("sounding", "%d", sounding);
    watch("cloud", "%d", cloud_i);
    watch("latch", "%d", latch);
#endif
}

void draw(void) {
    cls(CLR_DARKER_GREY);

    // chassis
    rectfill(0, 0, screen_w(), screen_h(), CLR_DARKER_GREY);
    rect(1, 1, screen_w() - 2, screen_h() - 2, CLR_DARK_GREY);

    // ── nameplate ──
    font(FONT_SMALL);
    print("CLOUD HOLD", (int)z_nav.x + 4, (int)z_nav.y + 3, CLR_LIGHT_YELLOW);
    print(frozen ? "FROZEN" : "LIVE",
          (int)(z_nav.x + z_nav.w) - 48, (int)z_nav.y + 3,
          frozen ? CLR_YELLOW : CLR_DARK_GREY);

    ui_begin();
    int sbx = (int)z_nav.x + 4;
    int sby = (int)z_nav.y + (int)z_nav.h - 16;
    if (sby < (int)z_nav.y + 12) sby = (int)z_nav.y + 12;
    if (ui_button(sbx, sby, 56, 14, lift ? ">SHIM" : " SHIM")) lift = !lift;

    // ── cloud window ──
    rectfill((int)z_cloud.x, (int)z_cloud.y, (int)z_cloud.w, (int)z_cloud.h,
             frozen ? CLR_DARK_BLUE : CLR_BLACK);
    rect((int)z_cloud.x, (int)z_cloud.y, (int)z_cloud.w, (int)z_cloud.h,
         frozen ? CLR_YELLOW : CLR_INDIGO);

    for (int i = 0; i < NGRAIN; i++) if (speck[i].on) {
        float t = 1.0f - speck[i].age / speck[i].life;
        int c;
        if (material == MAT_GLASS)
            c = frozen
                ? (t > 0.55f ? CLR_WHITE : t > 0.25f ? CLR_BLUE : CLR_DARK_BLUE)
                : (t > 0.55f ? CLR_WHITE : t > 0.25f ? CLR_LIGHT_GREY : CLR_INDIGO);
        else if (material == MAT_DUST)
            c = frozen
                ? (t > 0.55f ? CLR_ORANGE : t > 0.25f ? CLR_BROWN : CLR_DARK_GREY)
                : (t > 0.55f ? CLR_PEACH  : t > 0.25f ? CLR_BROWN : CLR_DARK_GREY);
        else
            c = frozen
                ? (t > 0.55f ? CLR_YELLOW : t > 0.25f ? CLR_ORANGE : CLR_BROWN)
                : (t > 0.55f ? CLR_WHITE  : t > 0.25f ? CLR_MAUVE  : CLR_INDIGO);
        int r = material == MAT_GLASS ? 1 : (t > 0.5f ? 2 : 1);
        circfill((int)speck[i].x, (int)speck[i].y, r, c);
    }

    // capture head — sweeps while LIVE, halts when FROZEN (you see the write stop)
    int bar_y = (int)z_cloud.y + 3;
    int bar_x = (int)z_cloud.x + 4;
    int bar_w = (int)z_cloud.w - 8;
    rect(bar_x, bar_y, bar_w, 4, CLR_DARK_GREY);
    if (!frozen) {
        float ph = now() * 0.35f;
        ph = ph - floorf(ph);
        rectfill(bar_x + 1 + (int)(ph * (bar_w - 3)), bar_y + 1, 2, 2, CLR_GREEN);
    } else {
        rectfill(bar_x + 1, bar_y + 1, bar_w - 2, 2, CLR_YELLOW);
    }

    font(FONT_TINY);
    const char *hint = frozen ? "PLAY OVER THE CLOUD"
                     : (sounding >= 0 ? "STOMP FREEZE" : "HOLD A CHORD");
    print(hint, (int)z_cloud.x + 4, bar_y + 6, frozen ? CLR_YELLOW : CLR_DARK_GREY);

    keybed_draw();

    // ── chord pads ──
    for (int i = 0; i < NCHORD; i++) {
        Box b = pad_box[i];
        int lit = (sounding == i) || (frozen && cloud_i == i);
        if (ui_button((int)b.x, (int)b.y, (int)b.w, (int)b.h, CNAME[i])) {
            // tap = latch so one thumb can hit a chord then stomp FREEZE
            if (!frozen) latch = (latch == i) ? -1 : i;
        }
        if (lit) {
            rect((int)b.x - 1, (int)b.y - 1, (int)b.w + 2, (int)b.h + 2,
                 frozen ? CLR_YELLOW : CLR_MAUVE);
        }
    }

    // ── material stomps — same freeze, different scatter ──
    for (int i = 0; i < NMAT; i++) {
        Box b = mat_box[i];
        if (ui_button((int)b.x, (int)b.y, (int)b.w, (int)b.h, MNAME[i]))
            material = i;
        if (material == i) {
            int ring = (i == MAT_GLASS) ? CLR_BLUE
                     : (i == MAT_DUST)  ? CLR_ORANGE
                     : CLR_YELLOW;
            rect((int)b.x - 1, (int)b.y - 1, (int)b.w + 2, (int)b.h + 2, ring);
        }
    }

    // ── freeze stomp — the product ──
    int fx = (int)z_stomp.x + 6;
    int fy = (int)z_stomp.y + 3;
    int fw = (int)z_stomp.w - 12;
    int fh = (int)z_stomp.h - 6;
    if (fh < 22) fh = 22;
    if (ui_button(fx, fy, fw, fh, frozen ? "FROZEN" : "FREEZE")) set_frozen(!frozen);
    ui_end();

    // stomp ring after ui_end so it sits on top of the button chrome
    rect(fx - 1, fy - 1, fw + 2, fh + 2, frozen ? CLR_YELLOW : CLR_DARK_GREY);
}
