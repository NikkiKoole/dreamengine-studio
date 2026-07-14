/* de:meta
{
  "slug": "rotaries",
  "title": "rotaries",
  "status": "active",
  "created": "2026-07-14",
  "kind": [
    "tech-demo",
    "probe"
  ],
  "teaches": [],
  "description": {
    "summary": "The turning family from the control vocabulary, in beveled lo-fi: value models (pot / endless encoder / selector / push / dual), a trim-to-jumbo size range, all five LED-ring modes, and a LIVE layer of software-only knob tricks.",
    "detail": "The workshop cart for docs/design/control-vocabulary.md — one set of knob renderers (rot_body/rot_ring/rot_pointer/rot_knurl/draw_pot) exercised four ways so the look never drifts. Row 1 VALUE MODELS: POT (bounded — pointer + min/max dial), ENC (endless — bare knurled cap, NO pointer, value in a full-circle ring because the cap angle is a lie; the grip spins with the turn), SEL (discrete — snaps between 5 detents), PUSH (turn + tap-to-toggle a centre LED), DUAL (two pots on one shaft). Row 2 SIZE RANGE: the same pot at r=6..22, proving the 1-2px bevel scales. Row 3 LED-RING MODES: DOT/FILL/BIPOLAR/SPREAD/PULSE. Row 4 LIVE — what a physical knob can't do: MOD HALO (cap = your base, a live mark shows the LFO-modulated value — the value in two places at once), GLOW (value-reactive colour cold->hot), GHOST (a remembered default, tap to snap home), JOG (a weighted finger-dimple spinner with momentum + acceleration you flick to fly through a library list). Every cap is an extruded body in a recessed socket with a drop shadow + concentric sheen/shade — the icon's tactility as pixel-honest bevels.",
    "controls": "Drag any rotary vertically to turn it (every finger is its own pointer — turn two at once). Mouse wheel fine-tunes the hovered one. Tap PUSH to toggle its LED, tap GHOST to snap it home. DUAL: drag the ring for outer, the centre for inner. JOG: flick it and let go — it coasts through the list, faster spin = bigger jumps."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <math.h>   // floorf (jog momentum wraps a negative position)

// ROTARIES — the turning family (control-vocabulary.md §3 ↻) rendered in
// beveled lo-fi. The teaching point is that the VALUE MODEL dictates the
// indicator: a bounded pot earns a pointer; an endless encoder must NOT have
// one (its truth is the ring). Reuses ui.h's capture machinery (ui_reg +
// ui_cap_for) exactly like ui_knob, so drags are real multi-touch for free.

#define A0 150.0f      // dial start angle (0=right, 90=down); 7-8 o'clock
#define SW 240.0f      // dial sweep; leaves a 120° dead-zone at the bottom
#define R  15          // main knob radius
#define RR 13          // ring-encoder body radius (leaves room for the halo)

enum { RING_DOT, RING_FILL, RING_BIPOLAR, RING_SPREAD, RING_PULSE };

// ── state ──────────────────────────────────────────────────────────────────
static float pot_v = 0.35f;            // bounded pot
static float enc_v = 0.20f;            // endless encoder (accumulates, unbounded)
static float sel_v = 0.5f;             // discrete selector 0..1 → 5 detents
static float push_v = 0.6f;  static int push_on = 1;   // push-encoder + its LED
static float dual_o = 0.7f, dual_i = 0.3f;             // concentric outer/inner
static float ring_v[5] = { 0.30f, 0.65f, 0.30f, 0.5f, 0.8f };  // the 5 ring modes
static float size_v[5] = { 0.4f, 0.55f, 0.35f, 0.7f, 0.5f };   // the size range

// ── LIVE section (the software layer — behaviours hardware can't do) ─────────
static float halo_base = 0.45f;                 // MOD HALO: the base value your hand sets
static float glow_v    = 0.55f;                 // GLOW: value-reactive skin
static float ghost_v   = 0.65f;                 // GHOST: has a default it can snap home to
#define GHOST_DEF 0.5f
static float jog_pos = 0, jog_vel = 0, jog_ang = 0;   // JOG: list position, momentum, dimple spin
static int   jog_last_y = 0, jog_live = 0;
static const char *LIB[] = { "808 KICK","RIMSHOT","CLAP","OPEN HAT","CONGA","COWBELL",
                             "SNARE","CLAVE","TOM LOW","SHAKER","CRASH","RIDE" };
#define NLIB 12

// ── beveled-lo-fi body: recessed socket + soft shadow + extruded cap ─────────
static void rot_shadow(int cx, int cy, int r) {
    blend(BLEND_AVG);
    circfill(cx + 1, cy + (r >= 16 ? 3 : 2), r, CLR_BLACK);   // AVG with black = a drop shadow
    blend_reset();
}

// a slightly-raised faceplate the controls are set INTO (so the sockets read as holes)
static void faceplate(int x, int y, int w, int h) {
    rrectfill(x, y, w, h, 4, CLR_DARK_BROWN);
    line(x + 3, y + 1, x + w - 4, y + 1, CLR_BROWN);          // top sheen
    blend(BLEND_AVG);
    line(x + 3, y + h - 1, x + w - 4, y + h - 1, CLR_BLACK);  // bottom shade
    blend_reset();
}

// face = cap colour, hi = top sheen, lo = bottom shade. Edge-lit bevel.
// EVERYTHING concentric + from the arc/ring family (same pixel-center metric as
// circfill), so the sheen band, shade band and rim sit on exactly aligned pixels
// (studio.c:4508 "draw the fill, then arc() on top, no gap"). Never mix circ()
// (a different rim rule) or an offset centre — that's what made them drift.
static void rot_body(int cx, int cy, int r, int face, int hi, int lo, int hot) {
    int t = r >= 16 ? 2 : 1;                         // bevel thickness scales with size
    circfill(cx, cy, r + 3, CLR_BROWNISH_BLACK);     // hole punched in the faceplate
    rot_shadow(cx, cy, r);
    circfill(cx, cy, r, face);                       // flat cap face
    ring(cx, cy, r - 1 - t, r - 1, 205, 335, hi);    // top sheen — just inside the rim
    ring(cx, cy, r - 1 - t, r - 1,  25, 155, lo);    // bottom shade — same band, opposite side
    arc(cx, cy, r, 0, 360, hot ? CLR_WHITE : CLR_BLACK);   // rim (arc family → aligns)
}

// short radial grip ticks around a bare cap (an encoder has no pointer, so the
// spinning grip IS the turn feedback). `spin` rotates the whole texture with the cap.
static void rot_knurl(int cx, int cy, int r, int col, float spin) {
    for (float a = 0; a < 360; a += 30)
        line(cx + (int)dx(r - 4, a + spin), cy + (int)dy(r - 4, a + spin),
             cx + (int)dx(r - 1, a + spin), cy + (int)dy(r - 1, a + spin), col);
}

// the pointer a BOUNDED control earns (its angle is real)
static void rot_pointer(int cx, int cy, int r, float ang, int col) {
    int ex = cx + (int)dx(r - 2, ang), ey = cy + (int)dy(r - 2, ang);
    line(cx, cy, ex, ey, col);
    circfill(ex, ey, 1, col);
}

// the value halo (control-vocabulary.md §4) around a ring-encoder
static void rot_ring(int cx, int cy, int r, int mode, float v, int col) {
    int ri = r + 4, ro = r + 6;
    ring(cx, cy, ri, ro, A0, A0 + SW, CLR_DARKER_GREY);     // track
    float av = A0 + v * SW, mid = A0 + SW / 2.0f;
    if (mode == RING_DOT) {
        ring(cx, cy, ri, ro, av - 6, av + 6, col);
        ring(cx, cy, ri, ro, av - 2, av + 2, CLR_WHITE);       // bright core
    } else if (mode == RING_FILL) {
        ring(cx, cy, ri, ro, A0, av, col);
    } else if (mode == RING_BIPOLAR) {
        if (av >= mid) ring(cx, cy, ri, ro, mid, av, col);
        else           ring(cx, cy, ri, ro, av, mid, col);
        line(cx + (int)dx(ri, mid), cy + (int)dy(ri, mid),         // centre tick
             cx + (int)dx(ro + 1, mid), cy + (int)dy(ro + 1, mid), CLR_WHITE);
    } else if (mode == RING_SPREAD) {
        float hw = v * (SW / 2.0f);
        ring(cx, cy, ri, ro, mid - hw, mid + hw, col);
    } else { // RING_PULSE — a breathing glow
        int pc = (0.5f + 0.5f * sin_deg(now() * 220) > 0.5f) ? CLR_WHITE : col;
        ring(cx, cy, ri, ro, A0, av, pc);
    }
}

// a complete bounded pot at any radius: value ring + beveled body + pointer + min/max ticks.
// One routine, so the size row is literally the same pot drawn at r = 6 … 22.
static void draw_pot(int cx, int cy, int r, float v, int hot) {
    int ri = r + 3, ro = r + 5;
    ring(cx, cy, ri, ro, A0, A0 + v * SW, CLR_INDIGO);            // fill
    ring(cx, cy, ri, ro, A0 + v * SW, A0 + SW, CLR_DARKER_GREY);  // track
    for (int k = 0; k < 2; k++) {                                 // min/max ticks
        float a = k ? A0 + SW : A0;
        line(cx + (int)dx(ro + 1, a), cy + (int)dy(ro + 1, a),
             cx + (int)dx(ro + 3, a), cy + (int)dy(ro + 3, a), CLR_LIGHT_GREY);
    }
    rot_body(cx, cy, r, CLR_INDIGO, CLR_MAUVE, CLR_DARKER_PURPLE, hot);
    rot_pointer(cx, cy, r, A0 + v * SW, CLR_WHITE);
}

static void led(int x, int y, int on, int col) {
    circfill(x, y, 2, on ? col : CLR_DARKER_GREY);
    if (on) pset(x - 1, y - 1, CLR_WHITE);           // glint
}

static void plabel(const char *s, int cx, int y, int col) {
    print(s, cx - text_width(s) / 2, y, col);
}

// bounded rotary input: vertical relative drag (mirrors ui_knob). Returns hot.
static int rot_drag(void *id, int cx, int cy, int r, float *v, int clampit) {
    ui_reg(id, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
    UiCap *c = ui_cap_for(id);
    if (c) {
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }
        int py = c->released ? c->ry : c->cy;
        *v = c->v0 + (c->by - py) / 60.0f;
        if (clampit) *v = clamp(*v, 0, 1);
    }
    int hot = c != 0 || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
    if (!c && hot && mouse_wheel() != 0)
        *v = clamp(*v + mouse_wheel() * 0.03f, 0, 1);
    return hot;
}

void init(void) { bpm(120); }

void update(void) {
#ifdef DE_TRACE
    watch("pot", "%.2f", pot_v); watch("enc", "%.2f", enc_v);
    watch("sel", "%.2f", sel_v); watch("push_on", "%d", push_on);
    watch("caps", "%d", ui_cap_n);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    ui_begin();

    print("ROTARIES", 6, 3, CLR_WHITE);
    font(FONT_SMALL);
    print_right("the turning family - beveled lo-fi", 314, 5, CLR_MAUVE);

    int col[5] = { 32, 96, 160, 224, 288 };       // 5 column centres
    int r1 = 64, r2 = 152, r3 = 244;                // three row centres

    // ── ROW 1 — the value-model spine ──────────────────────────────────────
    print("1 VALUE MODELS  bounded / endless / discrete / push / dual", 6, 22, CLR_DARK_GREY);
    faceplate(4, 30, 312, 72);

    // (1) POT — absolute + bounded: pointer + min/max dial, hard stops
    {
        int cx = col[0], cy = r1;
        int hot = rot_drag(&pot_v, cx, cy, R, &pot_v, 1);
        draw_pot(cx, cy, R, pot_v, hot);
        plabel("POT", cx, cy + R + 6, CLR_LIGHT_GREY);
        plabel(str("%d%%", (int)(pot_v * 100)), cx, cy + R + 13, CLR_DARK_GREY);
    }

    // (2) ENC — relative + unbounded: bare knurled cap, NO pointer, full ring
    {
        int cx = col[1], cy = r1;
        void *id = &enc_v;
        ui_reg(id, cx - R, cy - R, 2 * R + 1, 2 * R + 1, 0);
        UiCap *c = ui_cap_for(id);
        if (c) {
            if (!c->has_v0) { c->has_v0 = 1; c->v0 = enc_v; c->by = c->cy; }
            int py = c->released ? c->ry : c->cy;
            enc_v = c->v0 + (c->by - py) / 60.0f;                       // never clamped
        }
        int hot = c != 0 || ui_hover(cx - R, cy - R, 2 * R + 1, 2 * R + 1);
        ring(cx, cy, R + 4, R + 6, 0, 360, CLR_DARKER_GREY);           // full = no ends
        float da = enc_v * 360;
        ring(cx, cy, R + 4, R + 6, da - 6, da + 6, CLR_LIGHT_PEACH);   // just a dot
        rot_body(cx, cy, R, CLR_DARK_GREY, CLR_LIGHT_GREY, CLR_BROWNISH_BLACK, hot);
        rot_knurl(cx, cy, R, CLR_MEDIUM_GREY, enc_v * 360);             // grip spins with the turn
        plabel("ENC", cx, cy + R + 6, CLR_LIGHT_GREY);
        plabel(str("%d", ((int)(enc_v * 100) % 100 + 100) % 100), cx, cy + R + 13, CLR_DARK_GREY);
    }

    // (3) SEL — discrete: snaps between 5 detents
    {
        int cx = col[2], cy = r1;
        int hot = rot_drag(&sel_v, cx, cy, R, &sel_v, 1);
        int k = (int)(sel_v * 4.999f);                                 // 0..4
        for (int i = 0; i < 5; i++) {                                  // detent pips
            float a = A0 + (i / 4.0f) * SW;
            led(cx + (int)dx(R + 7, a), cy + (int)dy(R + 7, a), i == k, CLR_LIME_GREEN);
        }
        rot_body(cx, cy, R, CLR_DARK_GREEN, CLR_LIME_GREEN, CLR_BROWNISH_BLACK, hot);
        rot_pointer(cx, cy, R, A0 + (k / 4.0f) * SW, CLR_WHITE);       // snapped
        plabel("SEL", cx, cy + R + 6, CLR_LIGHT_GREY);
        plabel(str("%d/5", k + 1), cx, cy + R + 13, CLR_DARK_GREY);
    }

    // (4) PUSH — turn + tap-to-toggle a centre LED
    {
        int cx = col[3], cy = r1;
        void *id = &push_v;
        ui_reg(id, cx - R, cy - R, 2 * R + 1, 2 * R + 1, 0);
        UiCap *c = ui_cap_for(id);
        int hot = 0;
        if (c) {
            if (!c->has_v0) { c->has_v0 = 1; c->v0 = push_v; c->by = c->cy; }
            int py = c->released ? c->ry : c->cy;
            push_v = clamp(c->v0 + (c->by - py) / 60.0f, 0, 1);
            int dd = c->ry - c->by; if (dd < 0) dd = -dd;
            if (c->released && dd < 3) push_on = !push_on;             // it was a tap
            hot = 1;
        }
        hot = hot || ui_hover(cx - R, cy - R, 2 * R + 1, 2 * R + 1);
        ring(cx, cy, R + 4, R + 6, A0, A0 + push_v * SW, CLR_TRUE_BLUE);
        rot_body(cx, cy, R, CLR_DARK_BLUE, CLR_BLUE, CLR_DARKER_BLUE, hot);
        rot_pointer(cx, cy, R, A0 + push_v * SW, CLR_WHITE);
        led(cx, cy, push_on, CLR_RED);                                 // the push state
        plabel("PUSH", cx, cy + R + 6, CLR_LIGHT_GREY);
        plabel(push_on ? "on" : "off", cx, cy + R + 13, CLR_DARK_GREY);
    }

    // (5) DUAL — concentric: two pots on one shaft (inner reg'd LAST → centre taps hit it)
    {
        int cx = col[4], cy = r1;
        int hot = rot_drag(&dual_o, cx, cy, R, &dual_o, 1);            // outer: reg first
        rot_drag(&dual_i, cx, cy, 7, &dual_i, 1);                      // inner: reg last
        rot_body(cx, cy, R, CLR_BROWN, CLR_ORANGE, CLR_DARK_BROWN, hot);
        rot_pointer(cx, cy, R, A0 + dual_o * SW, CLR_LIGHT_PEACH);     // outer pointer
        rot_body(cx, cy, 7, CLR_ORANGE, CLR_LIGHT_YELLOW, CLR_DARK_ORANGE, 0);
        rot_pointer(cx, cy, 7, A0 + dual_i * SW, CLR_BROWNISH_BLACK);  // inner pointer
        plabel("DUAL", cx, cy + R + 6, CLR_LIGHT_GREY);
    }

    // ── ROW 2 — the SIZE range: the same pot from trim to jumbo ────────────
    print("2 SIZE RANGE  one pot, r = 6 .. 22  (the bevel scales)", 6, 110, CLR_DARK_GREY);
    faceplate(4, 118, 312, 78);
    int srad[5] = { 6, 9, 13, 17, 22 };
    const char *sname[5] = { "TRIM", "SMALL", "MED", "LARGE", "JUMBO" };
    for (int i = 0; i < 5; i++) {
        int cx = col[i], cy = r2;
        int hot = rot_drag(&size_v[i], cx, cy, srad[i], &size_v[i], 1);
        draw_pot(cx, cy, srad[i], size_v[i], hot);
        plabel(sname[i], cx, 182, CLR_LIGHT_GREY);                     // common baseline
        plabel(str("r%d", srad[i]), cx, 189, CLR_DARK_GREY);
    }

    // ── ROW 3 — the five LED-ring modes on one encoder body ────────────────
    print("3 LED-RING MODES  dot / fill / bipolar / spread / pulse", 6, 202, CLR_DARK_GREY);
    faceplate(4, 210, 312, 68);
    const char *mname[5] = { "DOT", "FILL", "BIPOLAR", "SPREAD", "PULSE" };
    int mcol[5] = { CLR_ORANGE, CLR_TRUE_BLUE, CLR_LIME_GREEN, CLR_PINK, CLR_RED };
    for (int i = 0; i < 5; i++) {
        int cx = col[i], cy = r3;
        int hot = rot_drag(&ring_v[i], cx, cy, RR, &ring_v[i], 1);
        rot_ring(cx, cy, RR, i, ring_v[i], mcol[i]);
        rot_body(cx, cy, RR, CLR_DARK_GREY, CLR_LIGHT_GREY, CLR_BROWNISH_BLACK, hot);
        rot_knurl(cx, cy, RR, CLR_MEDIUM_GREY, ring_v[i] * SW);        // grip spins; no pointer
        plabel(mname[i], cx, cy + RR + 8, CLR_LIGHT_GREY);
    }

    // ── ROW 4 — the LIVE layer: things a physical knob can't do ────────────
    print("4 LIVE  what software knobs do that hardware can't", 6, 286, CLR_DARK_GREY);
    faceplate(4, 294, 312, 150);
    float lfo = sin_deg(now() * 120);                                  // the shared modulation source

    // (a) MOD HALO — cap = your base setting; a live mark shows the MODULATED value
    {
        int cx = 58, cy = 330, r = 18;
        int hot = rot_drag(&halo_base, cx, cy, r, &halo_base, 1);
        float depth = 0.28f;
        float mod = clamp(halo_base + depth * lfo, 0, 1);              // where the LFO pushes it
        ring(cx, cy, r + 3, r + 5, A0 + (halo_base - depth) * SW,      // faint mod-range band
             A0 + (halo_base + depth) * SW, CLR_DARKER_GREY);
        rot_body(cx, cy, r, CLR_DARK_BLUE, CLR_BLUE, CLR_DARKER_BLUE, hot);
        rot_pointer(cx, cy, r, A0 + halo_base * SW, CLR_LIGHT_GREY);   // your hand's base
        float ma = A0 + mod * SW;                                      // the live modulated mark
        ring(cx, cy, r + 3, r + 5, ma - 4, ma + 4, CLR_TRUE_BLUE);
        plabel("MOD HALO", cx, cy + r + 6, CLR_LIGHT_GREY);
        plabel("LFO moves it", cx, cy + r + 13, CLR_DARK_GREY);
    }

    // (b) GLOW — value-reactive skin: colour goes cold -> hot, glows brighter with value
    {
        int cx = 160, cy = 330, r = 18;
        int hot = rot_drag(&glow_v, cx, cy, r, &glow_v, 1);
        int heat[6] = { CLR_TRUE_BLUE, CLR_BLUE_GREEN, CLR_GREEN, CLR_YELLOW, CLR_ORANGE, CLR_RED };
        int face = heat[(int)(glow_v * 5.99f)];
        if (glow_v > 0.15f)                                            // a glow halo that grows with value
            ring(cx, cy, r + 2, r + 4, A0, A0 + glow_v * SW, face);
        rot_body(cx, cy, r, face, CLR_WHITE, CLR_BROWNISH_BLACK, hot);
        rot_pointer(cx, cy, r, A0 + glow_v * SW, CLR_WHITE);
        plabel("GLOW", cx, cy + r + 6, CLR_LIGHT_GREY);
        plabel("value = colour", cx, cy + r + 13, CLR_DARK_GREY);
    }

    // (c) GHOST — a default it remembers; tap the cap to snap home (instant recall)
    {
        int cx = 262, cy = 330, r = 18;
        void *id = &ghost_v;
        ui_reg(id, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
        UiCap *c = ui_cap_for(id);
        int hot = 0;
        if (c) {
            if (!c->has_v0) { c->has_v0 = 1; c->v0 = ghost_v; c->by = c->cy; }
            int py = c->released ? c->ry : c->cy;
            ghost_v = clamp(c->v0 + (c->by - py) / 60.0f, 0, 1);
            int dd = c->ry - c->by; if (dd < 0) dd = -dd;
            if (c->released && dd < 3) ghost_v = GHOST_DEF;            // tap = snap home
            hot = 1;
        }
        hot = hot || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
        float ga = A0 + GHOST_DEF * SW;                               // the ghost default tick
        line(cx + (int)dx(r + 3, ga), cy + (int)dy(r + 3, ga),
             cx + (int)dx(r + 6, ga), cy + (int)dy(r + 6, ga), CLR_MEDIUM_GREY);
        ring(cx, cy, r + 3, r + 5, A0, A0 + ghost_v * SW, CLR_MAUVE);
        rot_body(cx, cy, r, CLR_DARK_PURPLE, CLR_MAUVE, CLR_BROWNISH_BLACK, hot);
        rot_pointer(cx, cy, r, A0 + ghost_v * SW, CLR_WHITE);
        plabel("GHOST", cx, cy + r + 6, CLR_LIGHT_GREY);
        plabel("tap = reset", cx, cy + r + 13, CLR_DARK_GREY);
    }

    // (d) JOG — a weighted spinner with a finger dimple: flick it to fly through a library
    {
        int cx = 44, cy = 405, r = 26;
        void *id = &jog_pos;
        ui_reg(id, cx - r, cy - r, 2 * r + 1, 2 * r + 1, 0);
        UiCap *c = ui_cap_for(id);
        int hot = 0;
        if (c && !c->released) {
            int dyp = jog_last_y - c->cy;                             // drag up = forward
            if (jog_live) jog_vel = dyp * (0.06f + 0.004f * (dyp < 0 ? -dyp : dyp));  // accel: fast = big jumps
            jog_last_y = c->cy; jog_live = 1; hot = 1;
        } else {
            jog_live = 0;
            jog_vel *= 0.92f;                                          // momentum + friction after release
            if (jog_vel < 0.002f && jog_vel > -0.002f) jog_vel = 0;
        }
        jog_pos += jog_vel; jog_ang += jog_vel * 40;
        hot = hot || ui_hover(cx - r, cy - r, 2 * r + 1, 2 * r + 1);
        int idx = (((int)floorf(jog_pos) % NLIB) + NLIB) % NLIB;

        rot_body(cx, cy, r, CLR_DARK_GREY, CLR_LIGHT_GREY, CLR_BROWNISH_BLACK, hot);
        rot_knurl(cx, cy, r, CLR_MEDIUM_GREY, jog_ang);               // grip spins with the throw
        int dxp = cx + (int)dx(r * 0.5f, jog_ang), dyp = cy + (int)dy(r * 0.5f, jog_ang);
        circfill(dxp, dyp, 5, CLR_BROWNISH_BLACK);                    // the finger dimple (orbits as it spins)
        arc(dxp, dyp, 5, 200, 340, CLR_MEDIUM_GREY);
        plabel("JOG", cx, cy + r + 6, CLR_LIGHT_GREY);

        // the mock library — a scrolling list the jog flies through
        int lx = 94, ly = 380, rows = 7;
        rectfill(lx, ly, 218, 52, CLR_BROWNISH_BLACK);
        rect(lx, ly, 218, 52, CLR_DARKER_GREY);
        for (int k = -3; k <= 3; k++) {
            int li = (((idx + k) % NLIB) + NLIB) % NLIB;
            int yy = ly + 26 + k * 7 - 3;
            if (k == 0) { rectfill(lx + 2, yy - 1, 214, 8, CLR_DARK_BLUE);
                          print(">", lx + 4, yy, CLR_WHITE); }
            print(LIB[li], lx + 12, yy, k == 0 ? CLR_WHITE : CLR_DARK_GREY);
        }
    }

    font(FONT_NORMAL);
    ui_end();
}
