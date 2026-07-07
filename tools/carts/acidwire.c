/* de:meta
{
  "title": "acidwire",
  "status": "active",
  "created": "2026-07-07",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": {
    "summary": "The device-matrix WIREFRAME tool: flip acidrack's three-state-strip layout through every shape in device-matrix.md with one key, in the REAL logical size.",
    "detail": "A design tool for the acidrack redesign (device-adaptive-layout.md Phase 3, brief acidrack-layout-brief.md). The window is fixed (940x700); pressing a key calls de_resize() to shrink the CANVAS to the exact logical @ K=2 size of each device in device-matrix.md §2 (iPhone SE ... iPad 13 landscape). The engine letterboxes it, so you watch acidrack reflow at each TRUE shape - no fake nested device rect (the field-018 honesty win: lay.h + screen_w()/screen_h() see the real size, same path production acidrack uses). Because the canvas is already K=2 logical px, a 44pt finger is a constant 22 logical px - so every control's finger-comfort is honest. It draws the three-state strip model (folded/compact/expanded) and the per-shape arrangements from the brief: roomy=all-compact rack, tall=one expanded + compact + folded, short-wide=tabs. It's the vehicle for the brief's open compact-strip taste calls - tweak the compact layout here and eyeball it across all shapes.",
    "controls": "]/->/space/` (key left of 1) next shape - [/<- prev - 1-9 jump - w cycle which strip is expanded (also toggles the iPad all-compact rack) - m (un)mute the working strip - p cycle its pattern (6 per instrument) - s toggle the device SAFE-AREA skin (notch/Dynamic Island/rounded corners/home bar/status bar + the dashed keep-out boundary) - h hide the label"
  }
}
de:meta */
// ACIDWIRE — the wireframe tool for the acidrack device-adaptive redesign.
// See docs/design/device-matrix.md (§2 shapes) + acidrack-layout-brief.md (the
// three-state strip model this draws). Fixed window; de_resize() reshapes the
// CANVAS to each device's logical @ K=2 size, letterboxed by the engine.

#include "studio.h"
#include <math.h>
#include <string.h>
#include "lay.h"

extern void de_resize(int w, int h);   // engine seam (platform.h): set the active canvas size

#define KEY_GRAVE 96                    // the key left of `1` (a `§` on ISO Mac boards)

// ───────── device matrix (device-matrix.md §2) — logical @ K=2 + safe hardware ─────────
enum { TALL, WIDE, ROOMY };
enum { N_HOME, N_NOTCH, N_ISLAND, N_PAD };   // top hardware: home-button era · notch · Dynamic Island · none
typedef struct { const char *name; int w, h, cls, insT, insB, notch, cornR; } Dev;
// insT/insB = simulated safe-area top/bottom in logical px (status/notch · home bar); notch = the
// top cutout shape; cornR = screen corner radius. acidrack insets top+bottom only (landscape
// side-notch parked — brief §4). All device-honest so the wireframe shows WHERE controls can't go.
static Dev DEV[] = {
    { "iPhone SE",        188, 334, TALL,  10,  0, N_HOME,    3 },   // home button: status bar, square-ish corners
    { "iPhone 13 mini",   188, 406, TALL,  24, 16, N_NOTCH,  11 },
    { "iPhone 16 / 15",   196, 426, TALL,  24, 16, N_ISLAND, 14 },
    { "iPhone 16 Plus",   215, 466, TALL,  24, 16, N_ISLAND, 14 },
    { "iPhone 16 Pro Max", 220, 478, TALL, 26, 16, N_ISLAND, 15 },
    { "iPhone SE land",   334, 188, WIDE,   6,  0, N_HOME,    3 },
    { "iPhone 16 land",   426, 196, WIDE,   8, 12, N_ISLAND, 14 },   // island on the short side; we hint it top-center
    { "iPad mini",        372, 566, ROOMY, 12,  8, N_PAD,     9 },
    { "iPad 11\"",        417, 597, ROOMY, 12,  8, N_PAD,     9 },
    { "iPad 13\"",        516, 688, ROOMY, 12,  8, N_PAD,    10 },
    { "iPad 13\" land",   688, 516, ROOMY, 12,  8, N_PAD,    10 },
};
#define NDEV 11

// ───────── the rack inventory (acidfit's, + a compact middle state) ─────────
enum { KNOBS, DRUMS };
typedef struct { const char *name; int kind; int n; const char *const *labels; const char *const *compact; int nc; } Strip;
static const char *const K303[]  = { "CUT","RES","ENV","DEC","ACC","DRV","SQL" };
static const char *const K303c[] = { "CUT","RES","DRV" };                 // §2 guess: the knobs you ride live
static const char *const KMST[]  = { "VOL","TON","VRB" };
static const char *const KMSTc[] = { "DLY","FB","GLU" };
static const char *const V909[]  = { "BD","SD","LT","MT","HT","RS","CP","CH","OH","CC","RC" };
static const char *const V808[]  = { "BD","SD","LT","HT","CP","MA","CB","OH","CH" };
static const char *const VDUMc[] = { "TUNE","DEC" };                      // §2 guess: selected-voice knobs
static Strip STRIP[] = {
    { "303 A",  KNOBS, 7,  K303, K303c, 3 },
    { "303 B",  KNOBS, 7,  K303, K303c, 3 },
    { "909",    DRUMS, 11, V909, VDUMc, 2 },
    { "808",    DRUMS, 9,  V808, VDUMc, 2 },
    { "MASTER", KNOBS, 3,  KMST, KMSTc, 3 },
};
#define NSTRIP 5
#define STEPS 16

enum { FOLDED, COMPACT, EXPANDED };
#define FU 22.0f                 // a 44pt finger = 22 logical px at K=2 (constant — the point of the matrix)
#define NPAT 6                   // patterns per instrument (maker: "a couple, say 6")

static int cur = 0, applied = -1, work = NSTRIP, showlabel = 1, safehint = 1;
// work: 0..NSTRIP-1 = that strip expanded; NSTRIP = ALL COMPACT (the iPad §4 headline)
// per-instrument state the main screen must carry: mute + which of the 6 patterns is live
static int muted[NSTRIP] = { 0, 0, 0, 1, 0 };   // 808 muted by default (shows the silenced look)
static int patn[NSTRIP]  = { 0, 2, 1, 3, 0 };   // current pattern 0..NPAT-1

// ───────── strip-content wireframe bits ─────────
static void wf_knob(Box cell, int i, const char *label) {
    Box dial; Box lab = lay_split(cell, EDGE_BOTTOM, lay_clamp(FU * 0.28f, 4, 8), &dial);
    float r = lay_clamp((dial.w < dial.h ? dial.w : dial.h) * 0.42f, 3, 22);
    float cx = dial.x + dial.w / 2, cy = dial.y + dial.h / 2;
    circfill((int)cx, (int)cy, (int)r, CLR_MEDIUM_GREY); circ((int)cx, (int)cy, (int)r, CLR_DARK_GREY);
    float a = 2.3f + i * 0.7f;
    line((int)cx, (int)cy, (int)(cx + r * 0.8f * cosf(a)), (int)(cy + r * 0.8f * sinf(a)), CLR_ORANGE);
    if (r >= 5) { font(FONT_TINY); print_centered(label, (int)cx, (int)lab.y, CLR_LIGHT_GREY); }
}
static void wf_knobrow(Box area, const char *const *labels, int n) {
    float gap = lay_clamp(FU * 0.12f, 1, 3);
    for (int i = 0; i < n; i++) { Box c = lay_grid(area, n, n, i, gap); if (c.w > 4 && c.h > 6) wf_knob(lay_inset(c, 0.5f), i, labels[i]); }
}
static void wf_steplane(Box area, int seed, int mu) {   // mu = muted → grey, not green
    float gap = lay_clamp(FU * 0.06f, 1, 2);
    for (int i = 0; i < STEPS; i++) {
        Box s = lay_wrap(area, STEPS, i, FU * 0.42f, gap); if (s.w < 2) continue;
        int on = ((i + seed) % 4 == 0) || i == 6 || i == 11;
        boxfill(lay_inset(s, 0.5f), on ? (mu ? CLR_MEDIUM_GREY : CLR_LIME_GREEN) : CLR_DARK_GREY);
    }
}
static void wf_padgrid(Box area, const char *const *labels, int n, int mu) {
    float gap = lay_clamp(FU * 0.1f, 1, 3);
    for (int i = 0; i < n; i++) {
        Box c = lay_grid(area, 4, n, i, gap); if (c.w < 4 || c.h < 4) continue;
        Box pad = lay_inset(c, 0.5f); boxfill(pad, (i % 3 == 0) ? (mu ? CLR_MEDIUM_GREY : CLR_DARK_ORANGE) : CLR_DARKER_PURPLE); boxrect(pad, CLR_DARKER_BLUE);
        if (pad.h >= 8) { font(FONT_TINY); print_centered(labels[i], (int)(pad.x + pad.w / 2), (int)(pad.y + (pad.h - 5) / 2), CLR_LIGHT_PEACH); }
    }
}
static void wf_minipat(Box area, int seed, int mu) {   // folded: a row of tiny on/off dots
    for (int i = 0; i < STEPS; i++) { Box s = lay_wrap(area, STEPS, i, FU * 0.2f, 1); if (s.w < 1) continue;
        int on = ((i + seed) % 3 == 0); boxfill(lay_inset(s, 0.5f), on ? (mu ? CLR_MEDIUM_GREY : CLR_ORANGE) : CLR_DARKER_BLUE); }
}
// per-instrument PATTERN selector: NPAT numbered slots in `cols` columns, the live one lit
static void wf_patterns(Box area, int cur, int mu, int cols) {
    float gap = lay_clamp(FU * 0.09f, 1, 3);
    for (int i = 0; i < NPAT; i++) {
        Box c = lay_grid(area, cols, NPAT, i, gap); if (c.w < 3) continue;
        int on = (i == cur);
        boxfill(c, on ? (mu ? CLR_MEDIUM_GREY : CLR_ORANGE) : CLR_DARK_GREY); boxrect(c, CLR_DARKER_GREY);
        if (c.h >= 7) { font(FONT_TINY); print_centered(str("%d", i + 1), (int)(c.x + c.w / 2), (int)(c.y + (c.h - 5) / 2), on ? CLR_BLACK : CLR_MEDIUM_GREY); }
    }
}
// the pattern selector as its OWN little box (ReBirth's per-machine PATTERN panel): a titled,
// bordered unit clearly grouped with the instrument. 3×2 grid of the 6 patterns.
static void wf_patbox(Box b, int cur, int mu) {
    boxfill(b, CLR_BROWNISH_BLACK); boxrect(b, mu ? CLR_DARK_RED : CLR_MEDIUM_GREY);
    Box grid; Box lab = lay_split(lay_inset(b, 1), EDGE_TOP, lay_clamp(FU * 0.26f, 4, 7), &grid);
    font(FONT_TINY); print("PAT", (int)lab.x + 1, (int)lab.y, CLR_MEDIUM_GREY);
    wf_patterns(grid, cur, mu, 3);
}
static void wf_mute(Box hdr, int mu) {   // [M][fx] cluster at the header's right edge; M lit red when muted
    float bw = lay_clamp(FU * 0.7f, 8, 20);
    Box fx = lay_at(hdr, L_TR, bw, hdr.h - 2, 1); boxfill(fx, CLR_DARK_GREY); boxrect(fx, CLR_MEDIUM_GREY);
    font(FONT_TINY); print_centered("fx", (int)(fx.x + fx.w / 2), (int)(fx.y + (fx.h - 5) / 2), CLR_LIGHT_GREY);
    Box m = box(fx.x - bw * 0.65f - 2, fx.y, bw * 0.65f, fx.h); boxfill(m, mu ? CLR_RED : CLR_DARK_RED); boxrect(m, mu ? CLR_WHITE : CLR_MEDIUM_GREY);
    print_centered("M", (int)(m.x + m.w / 2), (int)(m.y + (m.h - 5) / 2), mu ? CLR_WHITE : CLR_LIGHT_PEACH);
}

// draw one strip at `state` inside `rect`
static void draw_strip(Strip *s, Box rect, int state, int accent) {
    int idx = (int)(s - STRIP), mu = muted[idx], pc = patn[idx];
    boxfill(rect, CLR_DARKER_BLUE); boxrect(rect, mu ? CLR_RED : (accent ? CLR_TRUE_BLUE : CLR_DARK_GREY));
    Box body; float hh = lay_clamp(FU * 0.42f, 7, 12);
    Box hdr = lay_split(lay_inset(rect, 1), EDGE_TOP, hh, &body);
    boxfill(hdr, mu ? CLR_DARK_RED : (accent ? CLR_TRUE_BLUE : CLR_DARK_GREY));
    font(FONT_TINY); print(s->name, (int)hdr.x + 2, (int)(hdr.y + (hdr.h - 5) / 2), CLR_LIGHT_PEACH);
    body = lay_pad(body, 1, 1, 1, 1);
    wf_mute(hdr, mu);

    if (state == FOLDED) {   // header: name · framed pattern LEDs · [M][fx]; body: a mini step preview
        float muteW = FU * 1.6f;
        Box pb = box(hdr.x + FU * 1.7f, hdr.y + 1, hdr.w - FU * 1.7f - muteW, hdr.h - 2);
        boxrect(pb, mu ? CLR_DARK_RED : CLR_DARKER_GREY);          // frame → its own little box
        wf_patterns(lay_inset(pb, 1), pc, mu, NPAT);              // one row of 6
        wf_minipat(body, idx, mu);
        return;
    }
    // COMPACT + EXPANDED: the pattern selector is its OWN boxed panel on the LEFT (ReBirth-style),
    // the machine controls fill the rest to its right — clearly grouped with THIS instrument.
    float boxW = (state == EXPANDED) ? FU * 2.7f : FU * 2.4f;
    Box mach; Box pbox = lay_split(body, EDGE_LEFT, boxW, &mach);
    wf_patbox(pbox, pc, mu);
    mach = lay_pad(mach, 1, 0, 0, 0);   // gap between the box and the machine

    if (state == COMPACT) {                        // 2-3 knobs (or drum selector) + one step lane
        Box lane; Box top = lay_split(mach, EDGE_TOP, FU * 1.3f, &lane);
        if (s->kind == KNOBS) wf_knobrow(top, s->compact, s->nc);
        else { // drum: voice selector chips + selected-voice knobs
            Box kn; Box sel = lay_split(top, EDGE_TOP, FU * 0.7f, &kn);
            float g = 2; for (int i = 0; i < 6 && i < s->n; i++) { Box c = lay_grid(sel, 6, 6, i, g); boxfill(lay_inset(c,0.5f), i==0?CLR_DARK_ORANGE:CLR_DARK_GREY);
                font(FONT_TINY); print_centered(s->labels[i], (int)(c.x+c.w/2), (int)(c.y+(c.h-5)/2), CLR_LIGHT_GREY); }
            wf_knobrow(kn, s->compact, s->nc);
        }
        wf_steplane(lay_pad(lane, 0, 1, 0, 1), idx, mu);
        return;
    }
    // EXPANDED — the full editor to the right of the pattern box
    if (s->kind == KNOBS) { Box lane; Box kn = lay_split(mach, EDGE_BOTTOM, FU * 1.4f, &lane); wf_knobrow(kn, s->labels, s->n); wf_steplane(lay_pad(lane,0,1,0,1), idx, mu); }
    else wf_padgrid(mach, s->labels, s->n, mu);
}

// ───────── device safe-area SKIN (the point of this whole cart) ─────────
#define MINI(a,b) ((a) < (b) ? (a) : (b))
// black the pixels OUTSIDE the corner arc (the rounded screen corner is "off screen")
static void round_corner(int ox, int oy, int r, int sx, int sy) {
    for (int y = 0; y < r; y++) for (int x = 0; x < r; x++) {
        int dx = r - x, dy = r - y;
        if (dx * dx + dy * dy > r * r) pset(ox + sx * x, oy + sy * y, CLR_BLACK);
    }
}
static void hpill(float x, float y, float w, float h, int col) {   // rounded-end horizontal pill
    int r = (int)(h / 2);
    boxfill(box(x + r, y, w - 2 * r, h), col);
    circfill((int)x + r, (int)y + r, r, col); circfill((int)(x + w) - r, (int)y + r, r, col);
}
static void dashrect(Box b, int col) {   // dashed outline = "keep controls inside here"
    int d = 4, g = 3;
    for (int x = (int)b.x; x < b.x + b.w; x += d + g) { line(x, (int)b.y, (int)MINI(x + d, b.x + b.w), (int)b.y, col); line(x, (int)(b.y + b.h) - 1, (int)MINI(x + d, b.x + b.w), (int)(b.y + b.h) - 1, col); }
    for (int y = (int)b.y; y < b.y + b.h; y += d + g) { line((int)b.x, y, (int)b.x, (int)MINI(y + d, b.y + b.h), col); line((int)(b.x + b.w) - 1, y, (int)(b.x + b.w) - 1, (int)MINI(y + d, b.y + b.h), col); }
}
// draw the device hardware over the rendered rack: status band + notch/island + home bar + rounded
// corners + the safe-area boundary. Everything here is a KEEP-OUT the layout must dodge.
static void draw_safe_skin(int W, int H, Dev d) {
    // reserved status/home bands (faint) — iOS draws the clock/battery/home-affordance here
    if (d.insT > 0) boxfill(box(0, 0, W, d.insT), CLR_DARKER_PURPLE);
    if (d.insB > 0) boxfill(box(0, H - d.insB, W, d.insB), CLR_DARKER_PURPLE);
    // status-bar mock (shows the band is real estate the app doesn't own)
    if (d.insT >= 9) { font(FONT_TINY);
        print("9:41", 4, (int)(d.insT - 5) / 2, CLR_LIGHT_GREY);
        boxfill(box(W - 12, (d.insT - 5) / 2, 9, 5), CLR_DARK_GREY); boxfill(box(W - 12, (d.insT - 5) / 2 + 1, 7, 3), CLR_LIME_GREEN); }
    // top cutout hardware (black = a hole in the glass)
    int cx = W / 2;
    if (d.notch == N_NOTCH) {
        int nw = (int)(W * 0.46f), nh = d.insT;
        boxfill(box(cx - nw / 2, 0, nw, nh), CLR_BLACK);
        circfill(cx - nw / 2, nh, 3, CLR_BLACK); circfill(cx + nw / 2, nh, 3, CLR_BLACK);   // rounded bottom
        boxfill(box(cx - nw / 2 - 3, nh - 3, 3, 3), CLR_BLACK); boxfill(box(cx + nw / 2, nh - 3, 3, 3), CLR_BLACK);
    } else if (d.notch == N_ISLAND) {
        int iw = (int)(W * 0.30f), ih = (int)(d.insT * 0.55f); if (ih < 6) ih = 6;
        hpill(cx - iw / 2, 4, iw, ih, CLR_BLACK);
    }
    // home indicator (no home-button era)
    if (d.notch != N_HOME) { int hw = (int)(W * (d.cls == WIDE ? 0.22f : 0.34f));
        hpill(cx - hw / 2, H - 5, hw, 3, CLR_LIGHT_GREY); }
    // rounded screen corners
    int r = d.cornR;
    round_corner(0, 0, r, 1, 1); round_corner(W - 1, 0, r, -1, 1);
    round_corner(0, H - 1, r, 1, -1); round_corner(W - 1, H - 1, r, -1, -1);
    // the safe-area boundary — put every control INSIDE this
    dashrect(box(2, d.insT, W - 4, H - d.insT - d.insB), CLR_ORANGE);
}

// footprint of a strip in a given state, in logical px (height)
static float strip_h(Strip *s, int state) {
    if (state == FOLDED)  return FU * 1.15f;              // patterns live in the header row
    if (state == COMPACT) return FU * 2.9f;              // pattern box is a LEFT column, not a row
    return (s->kind == DRUMS ? FU * 5.8f : FU * 5.0f);   // EXPANDED
}

void update(void) {
    int i;
    if (keyp(KEY_RIGHT) || keyp(']') || keyp(KEY_SPACE) || keyp(KEY_GRAVE)) cur = (cur + 1) % NDEV;
    if (keyp(KEY_LEFT)  || keyp('[')) cur = (cur + NDEV - 1) % NDEV;
    for (i = 0; i < 9 && i < NDEV; i++) if (keyp('1' + i)) cur = i;
    if (keyp('w') || keyp(KEY_DOWN)) work = (work + 1) % (NSTRIP + 1);
    if (keyp('h')) showlabel = !showlabel;
    if (keyp('s')) safehint = !safehint;
    // per-instrument controls, acting on the working strip (or 303-A when all-compact)
    int sel = (work < NSTRIP) ? work : 0;
    if (keyp('m')) muted[sel] = !muted[sel];              // (un)mute
    if (keyp('p')) patn[sel] = (patn[sel] + 1) % NPAT;    // cycle its 6 patterns
    if (cur != applied) { de_resize(DEV[cur].w, DEV[cur].h); applied = cur; }
}

void draw(void) {
    Dev d = DEV[cur];
    int W = screen_w(), H = screen_h();
    cls(CLR_BROWNISH_BLACK);

    // safe area (simulated status/notch/home; brief §4 — top/bottom only). The device SKIN that
    // shows WHERE we can't draw is rendered last (draw_safe_skin), over the rack.
    Box full = box(0, 0, W, H);
    Box safe = lay_pad(full, d.insT, 0, d.insB, 0);

    // pinned chrome: transport (top) + song row (bottom) — never disclosed away
    float trH = lay_clamp(FU * 1.2f, 12, 30), sgH = lay_clamp(FU * 1.0f, 10, 26);
    Box afterTr, afterSg;
    Box transport = lay_split(safe, EDGE_TOP, trH, &afterTr);
    Box song      = lay_split(afterTr, EDGE_BOTTOM, sgH, &afterSg);
    Box bodyarea  = lay_pad(afterSg, 2, 1, 2, 1);

    boxfill(transport, CLR_TRUE_BLUE);
    font(FONT_SMALL); print("\x10 STOP  139", (int)transport.x + 3, (int)(transport.y + (transport.h - 6) / 2), CLR_LIGHT_PEACH);
    { const char *bk = "A B C D"; print_right(bk, (int)(transport.x + transport.w - 3), (int)(transport.y + (transport.h - 6) / 2), CLR_LIGHT_PEACH); }
    boxfill(song, CLR_DARKER_BLUE); boxrect(song, CLR_DARK_GREY);
    for (int i = 0; i < 16; i++) { Box c = lay_wrap(lay_inset(song, 1.5f), 16, i, FU * 0.4f, 1); if (c.w < 2) continue; boxfill(lay_inset(c, 0.5f), (i % 4 == 0) ? CLR_ORANGE : CLR_DARK_GREY); }
    font(FONT_TINY); print("SONG", (int)song.x + 2, (int)song.y - 6 < transport.y ? (int)song.y + 1 : (int)song.y + 1, CLR_DARK_GREY);

    float gap = lay_clamp(FU * 0.18f, 1, 4);
    const char *mode;

    if (d.cls == WIDE) {
        // ─── short-wide: TABS (accordions degenerate short — acidfit finding) ───
        mode = "tabs";
        int sel = (work >= NSTRIP) ? 0 : work;
        Box panel; Box tabs = lay_split(bodyarea, EDGE_TOP, FU * 1.1f, &panel);
        for (int i = 0; i < NSTRIP; i++) { Box t = lay_grid(tabs, NSTRIP, NSTRIP, i, gap);
            boxfill(t, i == sel ? CLR_TRUE_BLUE : CLR_DARK_GREY); boxrect(t, CLR_MEDIUM_GREY);
            font(FONT_TINY); print_centered(STRIP[i].name, (int)(t.x + t.w / 2), (int)(t.y + (t.h - 5) / 2), CLR_LIGHT_PEACH); }
        draw_strip(&STRIP[sel], lay_pad(panel, 0, gap, 0, 0), EXPANDED, 1);

    } else if (d.cls == ROOMY) {
        // ─── roomy: the ALL-COMPACT rack (§4 headline) — tap one → expands in place ───
        mode = (work >= NSTRIP) ? "roomy · all compact" : "roomy · one expanded";
        // assign states, measure total, then lay out top-down
        int st[NSTRIP]; float tot = 0;
        for (int i = 0; i < NSTRIP; i++) { st[i] = (i == work) ? EXPANDED : COMPACT; tot += strip_h(&STRIP[i], st[i]) + gap; }
        float scale = (tot > bodyarea.h) ? bodyarea.h / tot : 1.0f;   // never overflow
        Box cur2 = bodyarea;
        for (int i = 0; i < NSTRIP; i++) { float rh = strip_h(&STRIP[i], st[i]) * scale;
            Box row = lay_split(cur2, EDGE_TOP, rh + gap, &cur2); draw_strip(&STRIP[i], lay_pad(row, 0, 0, gap, 0), st[i], i == work); }

    } else {
        // ─── tall (phone portrait): working EXPANDED + compact + folded, by budget ───
        mode = "tall · expand+compact+fold";
        int sel = (work >= NSTRIP) ? 0 : work;
        int st[NSTRIP]; for (int i = 0; i < NSTRIP; i++) st[i] = FOLDED;
        st[sel] = EXPANDED;
        float budget = bodyarea.h; for (int i = 0; i < NSTRIP; i++) budget -= strip_h(&STRIP[i], st[i]) + gap;
        // promote the others to COMPACT in order while there's room
        for (int i = 0; i < NSTRIP; i++) { if (i == sel) continue; float extra = strip_h(&STRIP[i], COMPACT) - strip_h(&STRIP[i], FOLDED);
            if (budget >= extra) { st[i] = COMPACT; budget -= extra; } }
        Box cur2 = bodyarea;
        for (int i = 0; i < NSTRIP; i++) { float rh = strip_h(&STRIP[i], st[i]);
            Box row = lay_split(cur2, EDGE_TOP, rh + gap, &cur2); draw_strip(&STRIP[i], lay_pad(row, 0, 0, gap, 0), st[i], i == sel); }
    }

    // ─── device safe-area skin over the rack (notch/island/corners/home bar) — toggle s ───
    if (safehint) draw_safe_skin(W, H, d);

    // ─── tool label (inside the device; toggle with h) ───
    if (showlabel) {
        font(FONT_TINY);
        const char *l1 = str("%s  %dx%d  %s", d.name, d.w, d.h, mode);
        const char *l2 = str("%.1fx%.1f fingers  ]/[ shape  w strip  h hide", d.w / FU, d.h / FU);
        int lw = 8 + (int)(strlen(l1) > strlen(l2) ? strlen(l1) : strlen(l2)) * 4;
        Box chip = box(2, H - d.insB - 20, lw < W - 4 ? lw : W - 4, 16);   // bottom-left: keeps transport + notch visible up top
        boxfill(chip, CLR_BLACK); boxrect(chip, CLR_ORANGE);
        print(l1, (int)chip.x + 3, (int)chip.y + 2, CLR_LIGHT_PEACH);
        print(l2, (int)chip.x + 3, (int)chip.y + 9, CLR_ORANGE);
    }
    font(FONT_NORMAL);
}
