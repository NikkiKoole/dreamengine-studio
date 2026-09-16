/* de:meta
{
  "slug": "ladderface",
  "title": "ladder face",
  "status": "active",
  "created": "2026-09-16",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "subtractive-synth",
    "self-oscillation"
  ],
  "resizable": true,
  "collection": [
    "device-face",
    "responsive"
  ],
  "lineage": "Compound-blindspot §3 product cart: scope_read into a CPU shader so the rack's FACE is the machine. Diode-ladder ribbon (FILTER_DIODE cutoff/resonance as a living waterfall), not a teaching trilogy and not a HUD meter. pget_rgb on the live canvas — no second RGB buffer (cpu-shaders.md stand-in). Cloudhold already owns freeze-cloud identity. ADR-0015 / ADR-0022 / #31.",
  "description": {
    "summary": "A diode-ladder filter whose face is a living ribbon. Saw in, squelch out — the picture IS the machine.",
    "detail": "The product cart for scope + CPU-shader as rack identity. A saw through FILTER_DIODE (the TB-303 diode ladder) feeds scope_read into a shadermath.h fragment: the hero is a waterfall of the real post-filter mix, coloured by cutoff and resonance, falling through a four-rung ladder. Knobs at rest still read as a ladder — a quiet drone keeps the ribbon breathing. Play the keybed and the ribbon fattens; twist RES and the cutoff ridge screams. Not shadelab (a lesson), not wavecandy (a meter), not cloudhold (that picture is the freeze-cloud). Compose only: scope_read + pset_rgb / pget_rgb + existing FILTER_DIODE. No new API.",
    "controls": "Drag CUT / RES. Play the keybed (touch, A-K white / W-E-T-Y-U black, Z/X octave, MIDI). SPACE toggles the idle drone (on by default — the face stays alive). 1/2/3 cutoff lo/mid/hi, 4/5/6 resonance dry/sung/scream. M autoplay riff."
  }
}
de:meta */
#include "studio.h"
#include "shadermath.h"
#include "keybed.h"
#include "ui.h"
#include "face.h"
#include <math.h>

// LADDER FACE — compound-blindspot §3. The face IS the machine.
//
//   saw → FILTER_DIODE (cutoff + resonance you can touch)
//   scope_read → CPU shader (shadermath.h + pset_rgb / pget_rgb)
//   four copper rungs + a cutoff ridge = a stranger can name the filter
//
// Identity, not a lesson (shadelab) and not a meter (wavecandy). Cloudhold
// already owns the freeze-cloud picture; this one is the diode-ladder ribbon.
// pget_rgb on the live hero is the stand-in — no second RGB buffer.

#define SL       5
#define DESIGN_W 200
#define DESIGN_H 320
#define SCOPE_N  256
#define HIST     48
#define PS       2
#define DRONE_MIDI 36
#define DRONE_VEL  5
#define PLAY_VEL   6

static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.10f, "nav"   },
    { FACE_HERO, 0,           0.00f, "face"  },
    { FACE_BAND, EDGE_BOTTOM, 0.20f, "knobs" },
    { FACE_BAND, EDGE_BOTTOM, 0.18f, "keys"  },
};
#define NZ 4

static Face g_face;
static Box  z_nav, z_face, z_knobs, z_keys;

static float k_cut = 0.46f;   // ~500 Hz — mid ribbon, reads at rest
static float k_res = 0.52f;   // sung, not screaming
static int   drone = 1;
static int   autoplay = 0;
static int   drone_h = -1;
static int   a_cut = -1, a_res = -1;
static float scope[SCOPE_N];
static float hist[HIST][SCOPE_N];
static int   hist_head;
static float env;
static int   auto_i;
static int   auto_wait;

static const int RIFF[] = { 48, 48, 51, 55, 51, 58, 55, 51 };
#define NRIFF 8

static int cut_hz(void) {
    // 80 Hz .. 6 kHz, log — a linear slider would live in the squeal
    return (int)(80.0f * de_powf(6000.0f / 80.0f, sat(k_cut)));
}

static float res_amt(void) {
    return sat(k_res) * 15.0f;
}

static void apply_filter(void) {
    int hz = cut_hz();
    int rs = (int)(res_amt() + 0.5f);
    if (hz == a_cut && rs == a_res) return;
    instrument_filter(SL, FILTER_DIODE, hz, rs);
    a_cut = hz;
    a_res = rs;
    if (drone_h >= 0) {
        note_cutoff(drone_h, hz);
        note_res(drone_h, res_amt());
    }
}

static int keybed_busy(void) {
    for (int m = 0; m < 128; m++) if (keybed_held(m)) return 1;
    return 0;
}

static void drone_off(void) {
    if (drone_h >= 0) note_off(drone_h);
    drone_h = -1;
}

static void drone_on(void) {
    if (drone_h >= 0 || keybed_busy() || autoplay) return;
    drone_h = note_on(DRONE_MIDI, SL, DRONE_VEL);
    if (drone_h >= 0) {
        note_cutoff(drone_h, cut_hz());
        note_res(drone_h, res_amt());
    }
}

static void relayout(void) {
    face_resize_to(DESIGN_W, DESIGN_H);
    Box area = face_area(3);
    g_face  = face_layout(area, ZONES, NZ, 8);
    z_nav   = g_face.box[0];
    z_face  = g_face.box[1];
    z_knobs = g_face.box[2];
    z_keys  = g_face.box[3];
    keybed_layout((int)z_keys.x + 2, (int)z_keys.y + 2,
                  (int)z_keys.w - 4, (int)z_keys.h - 4);
}

static float hist_at(int row, float u) {
    float x = sat(u) * (float)(SCOPE_N - 1);
    int i = (int)x;
    if (i < 0) i = 0;
    if (i > SCOPE_N - 2) i = SCOPE_N - 2;
    float t = x - (float)i;
    return mix(hist[row][i], hist[row][i + 1], t);
}

// The living face. Each row is an older scope window (newest at the top) —
// a waterfall of the REAL post-filter mix, coloured by cutoff / resonance.
// pget_rgb adds a smear when the last-frame snapshot is live (Raylib / web);
// on DE_NO_RAYLIB the snapshot is empty, so the hist ring is the picture
// (cart-land floats, not a second RGB buffer — cpu-shaders.md idea #5 stays parked).
static void draw_ribbon(void) {
    int x0 = (int)z_face.x + 1, y0 = (int)z_face.y + 1;
    int x1 = (int)(z_face.x + z_face.w) - 1;
    int y1 = (int)(z_face.y + z_face.h) - 1;
    if (x1 - x0 < 8 || y1 - y0 < 8) return;

    float fw = (float)(x1 - x0), fh = (float)(y1 - y0);
    float ridge_v = mix(0.80f, 0.16f, sat(k_cut));   // closed = ridge low
    float hue = mix(0.32f, 0.07f, sat(k_res)) + (k_cut - 0.5f) * 0.07f;

    for (int sy = y0; sy < y1; sy += PS) {
        float v = ((float)(sy - y0) + 0.5f) / fh;
        int age = (int)(v * (float)(HIST - 1));
        int row = hist_head - 1 - age;
        while (row < 0) row += HIST;
        for (int sx = x0; sx < x1; sx += PS) {
            float u = ((float)(sx - x0) + 0.5f) / fw;
            int bw = (sx + PS <= x1) ? PS : x1 - sx;
            int bh = (sy + PS <= y1) ? PS : y1 - sy;

            float s = hist_at(row, u);
            float amp = fabs2(s);
            float heat = de_powf(amp, 0.50f);
            // the ribbon: waveform as a snake down the well
            float mid = 0.5f + s * (0.30f + k_res * 0.12f);
            float snake = smoothstep(0.085f + k_res * 0.04f, 0.0f, fabs2(u - mid));

            // optional last-frame smear — no-op when pget_rgb has no snapshot
            int prev = pget_rgb(sx, sy - PS);
            int c = rgb(0.05f + env * 0.08f, 0.03f, 0.07f);
            if (prev >= 0) c = cadd(scale_rgb(prev, 0.55f), c);

            float val = 0.10f + env * 0.35f + heat * 0.55f + snake * 0.85f;
            c = cadd(c, hsv(hue, 0.80f + k_res * 0.15f, val));

            // passband read: highs (above the ridge) thin out when the lid is shut
            float above = sat((ridge_v - v) / 0.16f);
            c = scale_rgb(c, mix(1.0f, 0.38f, above * (1.0f - k_cut * 0.60f)));

            // cutoff ridge — the standing glow. RES fattens and brightens it.
            float d = fabs2(v - ridge_v);
            float glow = sat(1.0f - d / (0.040f + k_res * 0.12f));
            glow *= glow;
            if (glow > 0.02f)
                c = cadd(c, hsv(mix(0.20f, 0.08f, k_res), 0.50f,
                                glow * (0.35f + k_res * 0.75f)));

            rectfill_rgb(sx, sy, bw, bh, c);
        }
    }

    // the ladder itself — two rails + four rungs. Persistent structure so a
    // stranger can name the machine even before the ribbon wakes.
    int rail_l = x0 + 5, rail_r = x1 - 8;
    // palette chrome on top of the RGB well so the ladder reads even when
    // the ribbon is screaming (true-colour glow can't wash a later pset)
    rectfill(rail_l, y0 + 2, 3, y1 - y0 - 4, CLR_BROWN);
    rectfill(rail_r, y0 + 2, 3, y1 - y0 - 4, CLR_BROWN);
    for (int r = 0; r < 4; r++) {
        float rv = 0.18f + r * 0.20f;
        int ry = y0 + (int)(rv * fh);
        rectfill(rail_l - 1, ry - 1, rail_r - rail_l + 5, 5, CLR_DARKER_GREY);
        rectfill(rail_l, ry, rail_r - rail_l + 3, 3, CLR_ORANGE);
        int mx = (rail_l + rail_r) / 2;
        rectfill(mx - 12, ry - 1, 4, 4, CLR_YELLOW);
        rectfill(mx + 8,  ry - 1, 4, 4, CLR_YELLOW);
    }
}

void init(void) {
    enable_pget(true);
    instrument(SL, INSTR_SAW, 6, 160, 6, 320);
    instrument_level(SL, 0.85f);
    keybed_config(SL, 3, 8);   // C3, one octave of whites — fat enough on a phone
    apply_filter();
}

void update(void) {
    relayout();
    keybed_update();

    if (keyp(' ')) drone = !drone;
    if (keyp('M')) autoplay = !autoplay;
    if (keyp('1')) k_cut = 0.18f;
    if (keyp('2')) k_cut = 0.48f;
    if (keyp('3')) k_cut = 0.82f;
    if (keyp('4')) k_res = 0.12f;
    if (keyp('5')) k_res = 0.55f;
    if (keyp('6')) k_res = 0.92f;

    apply_filter();

    if (autoplay) {
        drone_off();
        if (--auto_wait <= 0) {
            hit(RIFF[auto_i], SL, PLAY_VEL, 140);
            auto_i = (auto_i + 1) % NRIFF;
            auto_wait = 10;   // ~6.5 Hz at 60 fps — a walking 303 line
        }
    } else {
        auto_wait = 0;
        if (keybed_busy()) drone_off();
        else if (drone) drone_on();
        else            drone_off();
    }

    scope_read(scope, SCOPE_N);
    for (int i = 0; i < SCOPE_N; i++) hist[hist_head][i] = scope[i];
    hist_head = (hist_head + 1) % HIST;
    float e = 0.0f;
    for (int i = 0; i < SCOPE_N; i++) e += scope[i] * scope[i];
    env = env * 0.82f + de_powf(e / (float)SCOPE_N, 0.5f) * 0.18f;

#ifdef DE_TRACE
    watch("cut", "%.2f", k_cut);
    watch("res", "%.2f", k_res);
    watch("hz", "%d", cut_hz());
    watch("drone", "%d", drone);
    watch("auto", "%d", autoplay);
    watch("busy", "%d", keybed_busy());
#endif
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    rectfill(0, 0, screen_w(), screen_h(), CLR_DARKER_GREY);
    rect(1, 1, screen_w() - 2, screen_h() - 2, CLR_DARK_GREY);

    font(FONT_SMALL);
    print("LADDER FACE", (int)z_nav.x + 4, (int)z_nav.y + 3, CLR_ORANGE);
    print(drone ? "DRONE" : "MUTE",
          (int)(z_nav.x + z_nav.w) - 44, (int)z_nav.y + 3,
          drone ? CLR_LIME_GREEN : CLR_DARK_GREY);

    // well
    rectfill((int)z_face.x, (int)z_face.y, (int)z_face.w, (int)z_face.h, CLR_BLACK);
    draw_ribbon();
    rect((int)z_face.x, (int)z_face.y, (int)z_face.w, (int)z_face.h, CLR_BROWN);

    ui_begin();
    int dbx = (int)z_nav.x + 4;
    int dby = (int)z_nav.y + (int)z_nav.h - 16;
    if (dby < (int)z_nav.y + 12) dby = (int)z_nav.y + 12;
    if (ui_button(dbx, dby, 56, 14, drone ? ">DRONE" : " DRONE")) drone = !drone;
    int abx = dbx + 60;
    if (ui_button(abx, dby, 44, 14, autoplay ? ">AUTO" : " AUTO")) autoplay = !autoplay;

    Box cutb = box(z_knobs.x + 8.0f, z_knobs.y + 2.0f,
                   z_knobs.w * 0.42f, z_knobs.h - 4.0f);
    Box resb = box(z_knobs.x + z_knobs.w * 0.52f, z_knobs.y + 2.0f,
                   z_knobs.w * 0.42f, z_knobs.h - 4.0f);
    if (ui_knob_cell(cutb, &k_cut, "CUT")) apply_filter();
    if (ui_knob_cell(resb, &k_res, "RES")) apply_filter();
    ui_end();

    keybed_draw();
}
