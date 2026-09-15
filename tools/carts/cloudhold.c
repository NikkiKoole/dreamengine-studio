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
  "lineage": "Compound-blindspot §6 product cart: freeze as a performed gesture on existing FX_GRAINS. Hold a chord, lock the cloud, play over it, unfreeze. Boutique-pedal identity — not the grains lab, not grainchop's sampler spike, not a new FX_*. ADR-0015 / #29.",
  "description": {
    "summary": "Hold a chord, freeze, the room becomes a pad. Play over the cloud, then unfreeze.",
    "detail": "The product cart for grains_freeze: one gesture, existing FX_GRAINS. Tap a chord pad to hold a soft triad into the granular cloud (or hold 1-4). Stomp FREEZE — capture stops, the buffer loops, the dry chord drops away, and the room is now a pad. Play the keybed over that cloud. Stomp again to recapture. SHIMMER transposes the frozen grains up an octave. Not a teaching demo with a hidden freeze knob: the stomp is the instrument. Cousin of grains (the lab) and grainchop (the sampler spike); this one is the boutique pedal.",
    "controls": "Tap a chord pad to hold it. Stomp FREEZE to lock the cloud; stomp again to recapture. Play the keys over the pad. SHIMMER = octave-up grains. 1-4 hold chords, SPACE freeze, Q shimmer. Keybed: A-K white, W-E-T-Y-U-O-P black, Z/X octave (+ mouse/touch/MIDI)."
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
// Boutique pedal, not a lab. SIZE/DENS/POSITION stay pinned to a shimmer-cloud
// voicing (the pedalboard GRAINS recipe). SHIMMER is the one extra: +12 grains_pitch.
// No new FX_*.

#define SL_PAD   6
#define SL_LEAD  5
#define NCHORD   4
#define NGRAIN   48
#define DESIGN_W 200
#define DESIGN_H 320

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
static int   pad_h[3] = { -1, -1, -1 };
static int   sounding = -1;     // dry chord currently on, or -1
static int   latch    = -1;     // tap-to-latch (one thumb)
static int   cloud_i  = 0;      // last chord that fed the tank (the frozen identity)
static int   a_frozen = -1, a_lift = -1;

static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.12f, "nav"    },
    { FACE_HERO, 0,           0.00f, "cloud"  },
    { FACE_BAND, EDGE_BOTTOM, 0.14f, "chords" },
    { FACE_BAND, EDGE_BOTTOM, 0.18f, "stomp"  },
};
#define NZ 4
static Face g_face;
static Box  z_nav, z_cloud, z_chords, z_stomp;
static Box  pad_box[NCHORD];

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
    for (int i = 0; i < 3; i++) pad_h[i] = note_on(CHORD[ci][i], SL_PAD, 5);
    sounding = ci;
    cloud_i  = ci;
}

static void set_frozen(int on) {
    if (frozen == on) return;
    frozen = on;
    instrument_grains_freeze(SL_PAD, on ? 1 : 0);
    if (on) release_pad();
}

static void apply_voice(void) {
    if (a_frozen != frozen) {
        instrument_grains_freeze(SL_PAD, frozen ? 1 : 0);
        a_frozen = frozen;
    }
    if (a_lift != lift) {
        instrument_grains_pitch(SL_PAD, lift ? 12.0f : 0.0f, 0.18f, 0);
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
    z_stomp  = g_face.box[3];

    float gap = 3.0f;
    float pw  = (z_chords.w - gap * (NCHORD + 1)) / NCHORD;
    if (pw < 16.0f) pw = 16.0f;
    for (int i = 0; i < NCHORD; i++) {
        pad_box[i] = box(z_chords.x + gap + i * (pw + gap),
                         z_chords.y + 2.0f, pw, z_chords.h - 4.0f);
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
        speck[i].vx   = sx * 10.0f;
        speck[i].vy   = -8.0f - (frozen ? 4.0f : 0.0f);
        speck[i].age  = 0.0f;
        speck[i].life = 0.45f + (lift ? 0.25f : 0.0f);
        speck[i].on   = 1;
        return;
    }
}

void init(void) {
    instrument(SL_PAD,  INSTR_TRI, 180, 400, 6, 900);
    instrument(SL_LEAD, INSTR_SAW,   6,  90, 5, 220);
    instrument_level(SL_LEAD, 0.75f);
    // pinned shimmer-cloud voicing (pedalboard GRAINS recipe: pos/scatter/fb fixed)
    // near the live edge, tight scatter — a short held chord still fills the
    // read window (the 3 s tank is otherwise half silence if you freeze early)
    instrument_grains(SL_PAD, 180.0f, 20.0f, 0.90f, 0.18f, 0.32f, 0.68f);
    instrument_grains_pitch(SL_PAD, 0.0f, 0.18f, 0);
    keybed_config(SL_LEAD, 4, 8);   // one octave of whites — fat enough on a phone
}

void update(void) {
    relayout();
    keybed_update();

    if (keyp(' ')) set_frozen(!frozen);   // SPACE only — F is a keybed white key
    if (keyp('Q')) lift = !lift;

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

    apply_voice();

    // visual swarm — denser while a chord is feeding or the cloud is locked
    float dt = 1.0f / 60.0f;
    float dens = (frozen || sounding >= 0) ? 22.0f : 4.0f;
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
        int c = frozen
            ? (t > 0.55f ? CLR_YELLOW : t > 0.25f ? CLR_ORANGE : CLR_BROWN)
            : (t > 0.55f ? CLR_WHITE  : t > 0.25f ? CLR_MAUVE  : CLR_INDIGO);
        int r = t > 0.5f ? 2 : 1;
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
