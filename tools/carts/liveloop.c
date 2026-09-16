/* de:meta
{
  "slug": "liveloop",
  "title": "live loop",
  "status": "active",
  "created": "2026-09-16",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "audio-input"
  ],
  "resizable": true,
  "collection": [
    "device-face",
    "responsive"
  ],
  "lineage": "Audio-input-frontier ★1 product cart: capture-then-freeze mic layers under live input_monitor. loopstation chrome (REC / mute / clear / loop ring), micfuzz path (no new public API). Overdub = another freeze take, not a circulating PCM ring. ADR-0015 / ADR-0032 / #17.",
  "todo": [
    "2026-09-16 ear: FX/WASH not heard clearly enough — maybe more tapeloop DNA / WASH more present. Not now.",
    "Parked: bars/quantize count-in; mix-down UX for 5th-layer fold; pedalboard hook later. Continuous overdub ring stays ADR-0015-gated (freeze-stack passed ear)."
  ],
  "description": {
    "summary": "Hold, clap, sing, guitar — freeze a bar, stack another, mute and clear. Live monitor while you play.",
    "detail": "The product cart for the pedal-tier live looper. Arm the mic, stomp REC, a bar (or 2 / 4) freezes into a sample slot and loops under you. Stomp again and a new layer stacks while the old ones play — capture-then-freeze, not a circulating overdub ring. Hear yourself the whole time via input_monitor (the micfuzz / GUITAR IN path). Mute or clear a layer; when all four slots are full the next take bounces the unmuted stack down so you can keep going. Optional WASH is a light echo+tape on the stack (tapeloop DNA), off by default. LIVE monitor does not replay (ADR-0032); frozen layers are plain PCM and do. No mic? D drops a demo layer so the ring still moves.",
    "controls": "TAP ENABLE then REC (or SPACE) to freeze a bar. REC again to stack. Tap a layer, MUTE / CLR. BARS 1/2/4 (empty stack only). CLICK = metronome. WASH = light echo+tape. -/+ gain. 1-4 select, M mute, BACKSPACE clear, N click, B bars, T wash, D demo layer. Phone: big REC at the thumb."
  }
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "face.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// LIVE LOOP — capture-then-freeze mic layers under live monitor.
//
//   arm mic        →  input_monitor (hear yourself, micfuzz path)
//   stomp REC      →  mic_record one loop, sample_load, SAMPLE_LOOP
//   stomp again    →  another take while layers play (overdub = a new layer)
//   mute / clear   →  note_vol 0 / drop the slot
//
// Four sample slots. A fifth take mixes the unmuted stack down (slot budget).
// No new API. Continuous PCM overdub into one circulating ring is ADR-0015
// only if freeze-stack fails ear (2026-09-16: it passed).
//
// TODO — maker ear 2026-09-16: leave liveloop alone for now. Parked:
//   - FX / WASH not heard clearly enough — maybe more tapeloop DNA / WASH
//     more present (not now)
//   - bars / quantize count-in
//   - mix-down UX for the 5th-layer fold
//   - pedalboard hook later

#define DESIGN_W 200
#define DESIGN_H 320
#define NLAY     4
#define ISLOT    10          // instrument slots 10..13
#define SL_CLICK 14
#define BUFMAX   (44100 * 8)
#define TEMPO    120
#define ROOT     60
#define VEL      5

enum { ST_IDLE, ST_ARM, ST_WAIT, ST_REC };

static const int LCOL[NLAY] = { CLR_ORANGE, CLR_BLUE, CLR_PINK, CLR_GREEN };
static const int LRAD[NLAY] = { 18, 24, 30, 36 };

static int   bars = 1;                 // 1 / 2 / 4 — locked once a layer exists
static int   metro = 1;
static int   wash = 0;
static float gain = 1.2f;
static int   st = ST_IDLE;
static int   sel = 0;
static int   filled[NLAY];
static int   muted[NLAY];
static int   voice[NLAY];
static int   rec_target = -1;
static int   fail = 0;
static int   demo_pend = 0;
static float peak = 0.0f;
static int   lamp = 0;
static int   loop_f0 = -1;
static int   g_frame = 0;
static float prev_lp = 0.0f;
static int   ap_mon = -1;
static float ap_gain = -1.0f;
static int   ap_wash = -1;

static float take[BUFMAX];
static float mixbuf[BUFMAX];
static float tmp[BUFMAX];
static float wf_lo[80], wf_hi[80];

static FaceZone ZONES[] = {
    { FACE_BAND, EDGE_TOP,    0.11f, "nav"    },
    { FACE_HERO, 0,           0.00f, "ring"   },
    { FACE_BAND, EDGE_BOTTOM, 0.22f, "layers" },
    { FACE_BAND, EDGE_BOTTOM, 0.11f, "chips"  },
    { FACE_BAND, EDGE_BOTTOM, 0.16f, "stomp"  },
};
#define NZ 5
static Face g_face;
static Box  z_nav, z_ring, z_layers, z_chips, z_stomp;
static Box  lay_box[NLAY];
static Box  chip_box[4];

static float loop_secs(void) {
    float s = (float)(bars * 4) * 60.0f / (float)TEMPO;
    if (s > 8.0f) s = 8.0f;
    if (s < 0.25f) s = 0.25f;
    return s;
}

static int nfilled(void) {
    int n = 0;
    for (int i = 0; i < NLAY; i++) if (filled[i]) n++;
    return n;
}

static int next_empty(void) {
    for (int i = 0; i < NLAY; i++) if (!filled[i]) return i;
    return -1;
}

static float loop_phase(void) {
    float len = loop_secs();
    if (len <= 0.0f) return 0.0f;
    if (loop_f0 < 0) {
        float beats = (float)(bars * 4);
        return fmodf(beat() + beat_pos(), beats) / beats;
    }
    float t = (float)(g_frame - loop_f0) / 60.0f;
    float p = t / len;
    return p - floorf(p);
}

static void bind_layer(int i, const float *data, int n) {
    if (i < 0 || i >= NLAY || n < 64) return;
    sample_load(i, data, n);
    instrument(ISLOT + i, INSTR_SAMPLE, 2, 0, 7, 40);
    instrument_sample(ISLOT + i, i, ROOT);
    instrument_sample_mode(ISLOT + i, SAMPLE_LOOP);
    instrument_level(ISLOT + i, 0.85f);
    if (voice[i] >= 0) note_off(voice[i]);
    voice[i] = note_on(ROOT, ISLOT + i, VEL);
    filled[i] = 1;
    muted[i] = 0;
    sel = i;
    if (loop_f0 < 0) loop_f0 = g_frame;
}

static void mute_layer(int i) {
    if (i < 0 || i >= NLAY || !filled[i]) return;
    muted[i] = !muted[i];
    if (voice[i] >= 0) note_vol(voice[i], muted[i] ? 0.0f : (float)VEL);
}

static void clear_layer(int i) {
    if (i < 0 || i >= NLAY) return;
    if (voice[i] >= 0) note_off(voice[i]);
    voice[i] = -1;
    filled[i] = 0;
    muted[i] = 0;
    if (nfilled() == 0) loop_f0 = -1;
}

static void mix_down(void) {
    int nmax = 0;
    memset(mixbuf, 0, sizeof mixbuf);
    int any = 0;
    for (int i = 0; i < NLAY; i++) {
        if (!filled[i] || muted[i]) continue;
        int n = sample_read(i, tmp, BUFMAX);
        if (n <= 0) continue;
        if (n > nmax) nmax = n;
        for (int s = 0; s < n; s++) mixbuf[s] += tmp[s];
        any = 1;
    }
    for (int i = 0; i < NLAY; i++) clear_layer(i);
    if (!any || nmax < 64) return;
    float pk = 1e-6f;
    for (int s = 0; s < nmax; s++) {
        float a = fabsf(mixbuf[s]);
        if (a > pk) pk = a;
    }
    float g = (pk > 0.9f) ? (0.9f / pk) : 1.0f;
    for (int s = 0; s < nmax; s++) mixbuf[s] *= g;
    bind_layer(0, mixbuf, nmax);
}

static void synth_take(int layer, float *out, int n) {
    float f = 196.0f * powf(1.33484f, (float)(layer % 4));   // stacked fifths
    float beat_s = 60.0f / (float)TEMPO;
    for (int s = 0; s < n; s++) {
        float t = (float)s / 44100.0f;
        float ph = t / beat_s;
        ph = ph - floorf(ph);
        float env = expf(-ph * 7.0f);
        float x = 6.2831853f * f * t;
        out[s] = 0.62f * env * (sinf(x) + 0.35f * sinf(2.0f * x));
    }
}

static void finish_take(const float *data, int n) {
    int slot = rec_target;
    rec_target = -1;
    st = ST_IDLE;
    demo_pend = 0;
    if (slot < 0 || slot >= NLAY) return;
    if (n < 2048) { fail = 1; return; }
    float pk = 1e-6f;
    for (int i = 0; i < n; i++) {
        float a = fabsf(data[i]);
        if (a > pk) pk = a;
    }
    if (pk < 0.015f) { fail = 1; return; }
    float g = 0.90f / pk;
    if (n > BUFMAX) n = BUFMAX;
    for (int i = 0; i < n; i++) take[i] = data[i] * g;
    bind_layer(slot, take, n);
    fail = 0;
}

static void begin_record(void) {
    if (nfilled() == NLAY) mix_down();
    int slot = next_empty();
    if (slot < 0) return;
    rec_target = slot;
    fail = 0;
    demo_pend = 0;
    mic_start();                 /* keep de_mic_wanted high for the host */
    mic_record(loop_secs());
    st = ST_REC;
}

static void begin_demo(void) {
    if (nfilled() == NLAY) mix_down();
    int slot = next_empty();
    if (slot < 0) return;
    rec_target = slot;
    int n = (int)(loop_secs() * 44100.0f);
    if (n > BUFMAX) n = BUFMAX;
    if (n < 2048) n = 2048;
    synth_take(slot, take, n);
    finish_take(take, n);
}

static void request_rec(int demo) {
    if (st == ST_REC) return;
    if (!demo && !mic_active()) {
        mic_start();
        st = ST_ARM;
        demo_pend = 0;
        return;
    }
    if (loop_f0 >= 0) {
        st = ST_WAIT;
        demo_pend = demo ? 1 : 0;
        return;
    }
    if (demo) begin_demo();
    else begin_record();
}

static void apply_monitor(void) {
    int live = mic_active() ? 1 : 0;
    float g = live ? gain : 0.0f;
    if (live != ap_mon || g != ap_gain) {
        input_monitor(g);
        ap_mon = live;
        ap_gain = g;
    }
}

static void apply_wash(void) {
    if (wash == ap_wash) return;
    if (wash) {
        echo_insert(320, 0.28f, 0.42f, 0.22f);
        tape(0.12f, 0.08f, 0.20f);
    } else {
        echo_insert(320, 0.0f, 0.5f, 0.0f);
        tape(0.0f, 0.0f, 0.0f);
    }
    ap_wash = wash;
}

static void relayout(void) {
    face_resize_to(DESIGN_W, DESIGN_H);
    Box area = face_area(3);
    g_face   = face_layout(area, ZONES, NZ, 8);
    z_nav    = g_face.box[0];
    z_ring   = g_face.box[1];
    z_layers = g_face.box[2];
    z_chips  = g_face.box[3];
    z_stomp  = g_face.box[4];

    float gap = 2.0f;
    float lh  = (z_layers.h - gap * (NLAY + 1)) / NLAY;
    if (lh < 10.0f) lh = 10.0f;
    for (int i = 0; i < NLAY; i++) {
        lay_box[i] = box(z_layers.x + 3.0f,
                         z_layers.y + gap + i * (lh + gap),
                         z_layers.w - 6.0f, lh);
    }

    float cw = (z_chips.w - gap * 5) / 4.0f;
    if (cw < 28.0f) cw = 28.0f;
    for (int i = 0; i < 4; i++) {
        chip_box[i] = box(z_chips.x + gap + i * (cw + gap),
                          z_chips.y + 2.0f, cw, z_chips.h - 4.0f);
    }
}

void init(void) {
    bpm(TEMPO);
    for (int i = 0; i < NLAY; i++) voice[i] = -1;
    instrument(SL_CLICK, INSTR_TRI, 0, 28, 0, 18);
    apply_wash();
}

void update(void) {
    relayout();
    g_frame++;

    if (st == ST_ARM && mic_active()) request_rec(0);

    float lp = loop_phase();
    int wrap = (lp < prev_lp);
    if (st == ST_WAIT && wrap) {
        if (demo_pend) begin_demo();
        else begin_record();
    }

    if (st == ST_REC && g_frame > 3 && !mic_recording()) {
        int n = mic_record_read(take, BUFMAX);
        finish_take(take, n);
    }

    if (keyp(KEY_SPACE)) request_rec(0);
    if (keyp('D')) request_rec(1);
    for (int i = 0; i < NLAY; i++) if (keyp('1' + i)) sel = i;
    if (keyp('M')) mute_layer(sel);
    if (keyp(KEY_BACKSPACE)) clear_layer(sel);
    if (keyp('N')) metro = !metro;
    if (keyp('T')) wash = !wash;
    if (keyp('B') && nfilled() == 0) {
        bars = (bars == 1) ? 2 : (bars == 2) ? 4 : 1;
    }
    if (keyp('-') || keyp('[')) { gain -= 0.2f; if (gain < 0.0f) gain = 0.0f; }
    if (keyp('=') || keyp(']')) { gain += 0.2f; if (gain > 4.0f) gain = 4.0f; }

    if (metro && every(1)) {
        int acc = ((int)(beat()) % 4) == 0;
        hit(acc ? 96 : 84, SL_CLICK, acc ? 2 : 1, 18);
        lamp = 4;
    }
    if (lamp > 0) lamp--;

    float lvl = mic_level();
    if (lvl > peak) peak = lvl;
    peak *= 0.94f;

    apply_monitor();
    apply_wash();
    prev_lp = lp;

#ifdef DE_TRACE
    watch("st", "%d", st);
    watch("nlay", "%d", nfilled());
    watch("sel", "%d", sel);
    watch("bars", "%d", bars);
    watch("rec", "%d", rec_target);
    watch("mute", "%d%d%d%d", muted[0], muted[1], muted[2], muted[3]);
    watch("fill", "%d%d%d%d", filled[0], filled[1], filled[2], filled[3]);
    watch("lp", "%.3f", lp);
    watch("mic", "%d", mic_active());
#endif
}

static void draw_ring(void) {
    int cx = (int)(z_ring.x + z_ring.w * 0.42f);
    int cy = (int)(z_ring.y + z_ring.h * 0.52f);
    int rad = (int)(z_ring.h * 0.38f);
    if (rad > (int)(z_ring.w * 0.36f)) rad = (int)(z_ring.w * 0.36f);
    if (rad < 22) rad = 22;

    int ticks = bars * 4;
    if (ticks < 4) ticks = 4;
    for (int i = 0; i < ticks; i++) {
        float a = (float)i / (float)ticks * 360.0f - 90.0f;
        int big = (i % 4) == 0;
        int r0 = rad - (big ? 3 : 1);
        line(cx + (int)dx((float)r0, a), cy + (int)dy((float)r0, a),
             cx + (int)dx((float)rad, a), cy + (int)dy((float)rad, a),
             big ? CLR_LIGHT_GREY : CLR_DARKER_GREY);
    }

    for (int i = 0; i < NLAY; i++) {
        int r = 8 + (rad - 10) * (i + 1) / NLAY;
        int col = !filled[i] ? CLR_DARKER_GREY
                : (muted[i] ? CLR_DARK_GREY : LCOL[i]);
        circ(cx, cy, r, col);
        if (filled[i] && !muted[i]) {
            int n = sample_peaks(i, wf_lo, wf_hi, 32);
            if (n > 0) {
                for (int k = 0; k < 32; k++) {
                    float a = (float)k / 32.0f * 360.0f - 90.0f;
                    float amp = fabsf(wf_hi[k]);
                    if (amp < 0.08f) continue;
                    int rr = r + (amp > 0.4f ? 2 : 1);
                    pset(cx + (int)dx((float)rr, a), cy + (int)dy((float)rr, a), col);
                }
            }
        }
    }

    float ph = loop_phase() * 360.0f - 90.0f;
    line(cx, cy, cx + (int)dx((float)rad, ph), cy + (int)dy((float)rad, ph), CLR_WHITE);

    if (st == ST_REC) {
        float p = mic_record_progress();
        int steps = (int)(p * 48.0f);
        for (int k = 0; k <= steps; k++) {
            float a = (float)k / 48.0f * 360.0f - 90.0f;
            pset(cx + (int)dx((float)(rad + 2), a), cy + (int)dy((float)(rad + 2), a), CLR_RED);
        }
    }

    int rec_lit = (st == ST_REC && blink(10)) || (st == ST_WAIT && blink(16));
    circfill(cx, cy, 3, rec_lit ? CLR_RED : (metro && lamp ? CLR_ORANGE : CLR_LIGHT_GREY));

    // VU + gain, right of the ring
    int vx = (int)(z_ring.x + z_ring.w * 0.78f);
    int vy = (int)z_ring.y + 8;
    int vh = (int)z_ring.h - 16;
    if (vh < 20) vh = 20;
    int vw = 10;
    rectfill(vx, vy, vw, vh, CLR_BLACK);
    rect(vx, vy, vw, vh, CLR_DARK_GREY);
    float lvl = mic_level();
    if (lvl > 1.0f) lvl = 1.0f;
    int fillh = (int)(lvl * (float)(vh - 2));
    int fy = vy + vh - 1 - fillh;
    rectfill(vx + 1, fy, vw - 2, fillh, lvl > 0.5f ? CLR_ORANGE : CLR_LIME_GREEN);
    int py = vy + vh - 1 - (int)(fminf(1.0f, peak) * (float)(vh - 2));
    line(vx, py, vx + vw, py, CLR_WHITE);

    font(FONT_TINY);
    print(str("%.1f", gain), vx - 2, (int)z_ring.y + 2, CLR_MEDIUM_GREY);
    font(FONT_SMALL);
    const char *hint = !mic_active() && st != ST_ARM ? "TAP ENABLE"
                     : (st == ST_REC ? "HOLD IT"
                     : (st == ST_WAIT ? "WAIT LOOP"
                     : (nfilled() ? "STOMP TO STACK" : "STOMP REC")));
    print(hint, (int)z_ring.x + 4, (int)z_ring.y + 3,
          st == ST_REC ? CLR_RED : CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    rectfill(0, 0, screen_w(), screen_h(), CLR_DARKER_GREY);
    rect(1, 1, screen_w() - 2, screen_h() - 2, CLR_DARK_GREY);

    font(FONT_SMALL);
    print("LIVE LOOP", (int)z_nav.x + 4, (int)z_nav.y + 3, CLR_LIGHT_PEACH);
    print(str("%d BAR  %d", bars, TEMPO),
          (int)(z_nav.x + z_nav.w) - 62, (int)z_nav.y + 3, CLR_INDIGO);
    int live = mic_active();
    print(live ? "MIC" : (st == ST_ARM ? "…" : "OFF"),
          (int)z_nav.x + 4, (int)z_nav.y + (int)z_nav.h - 10,
          live ? CLR_LIME_GREEN : CLR_DARK_GREY);
    if (fail) print("quiet take", (int)z_nav.x + 28, (int)z_nav.y + (int)z_nav.h - 10, CLR_ORANGE);

    draw_ring();

    ui_begin();

    int gbx = (int)(z_nav.x + z_nav.w) - 52;
    int gby = (int)z_nav.y + (int)z_nav.h - 14;
    if (ui_button(gbx, gby, 22, 12, "-")) { gain -= 0.2f; if (gain < 0.0f) gain = 0.0f; }
    if (ui_button(gbx + 24, gby, 22, 12, "+")) { gain += 0.2f; if (gain > 4.0f) gain = 4.0f; }

    for (int i = 0; i < NLAY; i++) {
        Box b = lay_box[i];
        int x = (int)b.x, y = (int)b.y, w = (int)b.w, h = (int)b.h;
        int on = filled[i] && !muted[i];
        char lab[12];
        if (!filled[i]) snprintf(lab, sizeof lab, "L%d  --", i + 1);
        else if (muted[i]) snprintf(lab, sizeof lab, "L%d MUTE", i + 1);
        else snprintf(lab, sizeof lab, "L%d  ON", i + 1);
        if (ui_button(x, y, w, h, lab)) sel = i;
        if (sel == i) rect(x - 1, y - 1, w + 2, h + 2, CLR_WHITE);
        if (filled[i]) {
            circfill(x + 6, y + h / 2, 2, on ? LCOL[i] : CLR_DARK_GREY);
        }
    }

    // MUTE / CLR / CLICK / BARS
    if (ui_button_cell(chip_box[0], filled[sel] && muted[sel] ? "UNMUTE" : "MUTE"))
        mute_layer(sel);
    if (ui_button_cell(chip_box[1], "CLR"))
        clear_layer(sel);
    if (ui_button_cell(chip_box[2], metro ? "CLICK" : "clk"))
        metro = !metro;
    if (ui_button_cell(chip_box[3], str("%d BAR", bars)) && nfilled() == 0)
        bars = (bars == 1) ? 2 : (bars == 2) ? 4 : 1;

    font(FONT_NORMAL);
    int fx = (int)z_stomp.x + 6;
    int fy = (int)z_stomp.y + 3;
    int fw = (int)z_stomp.w - 44;
    int fh = (int)z_stomp.h - 6;
    if (fh < 22) fh = 22;
    const char *rec_l = !mic_active() && st != ST_ARM && st != ST_REC ? "ENABLE"
                      : (st == ST_REC ? "REC" : (st == ST_WAIT ? "WAIT" : "REC"));
    if (ui_button(fx, fy, fw, fh, rec_l)) request_rec(0);
    font(FONT_SMALL);
    int wx = fx + fw + 4;
    if (ui_button(wx, fy, 30, fh, wash ? "WASH" : "wash")) wash = !wash;
    ui_end();

    int rec_fill = (st == ST_REC) ? CLR_RED
                 : (st == ST_WAIT) ? CLR_ORANGE
                 : (mic_active() ? CLR_DARK_RED : CLR_DARKER_BLUE);
    rectfill(fx, fy, fw, fh, rec_fill);
    font(FONT_NORMAL);
    print_centered(rec_l, fx + fw / 2, fy + fh / 2 - 3, CLR_WHITE);
    int rec_ring = (st == ST_REC) ? CLR_WHITE
                 : (st == ST_WAIT) ? CLR_YELLOW
                 : CLR_DARK_RED;
    rect(fx - 1, fy - 1, fw + 2, fh + 2, rec_ring);
    if (st == ST_REC) {
        int pw = (int)(mic_record_progress() * (float)fw);
        rectfill(fx, fy + fh - 3, pw, 2, CLR_WHITE);
    }
}
