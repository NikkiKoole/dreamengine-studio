/* de:meta
{
  "title": "Loupe",
  "status": "active",
  "kind": [
    "tech-demo"
  ],
  "teaches": [],
  "description": "Magnifier interaction for tiny touch targets on a phone. A mock 808 (two rows of 16 small step buttons + four knobs) that's brutal to hit on mobile. Drag the corner box and a fixed 3x rectangular lens appears above your finger showing whatever it's over, magnified; lift to park it. A SECOND finger reaches into the lens and edits the fat, easy version - the lens blocks touches underneath so the real tiny widgets stay safe. Grab a knob inside the lens and you can drag right out of it and keep turning (the grab already landed; knobs re-anchor at the lens edge so there's no jump - and inside the lens a full sweep needs 3x travel, so it doubles as fine-control). Tap empty space to dismiss, then poke the tiny widgets directly to feel the difference. Self-contained POC for a ui.h capability."
}
de:meta */
// loupe.c — magnifier-loupe interaction for small touch targets.
//
// The problem: a desktop layout (drum machine, here a mock 808 — two rows of
// 16 tiny step buttons + four small knobs) is fine with a mouse but the
// targets are brutal on a phone in portrait. The loupe is a target-ACQUISITION
// aid: drag the corner handle (⊕) and a fixed 3× rectangular lens appears just
// above your finger, showing whatever it's over, magnified. Lift to park it.
// A SECOND finger reaches into the lens and edits the fat, easy version; the
// lens blocks touches underneath so the tiny real widgets are safe.
//
// The key feel: once your second finger GRABS a widget inside the lens, you
// own it — drag right out of the lens and keep turning the knob the normal
// way (the grab already landed; the lens did its only job). Knobs are relative
// drags, so the moment the finger crosses the lens edge we re-anchor the grab
// baseline → no value jump. Inside the lens a full knob sweep needs 3× the
// travel, so the magnifier doubles as a fine-control mode.
//
// Tap empty space (not the lens, not the handle) to dismiss the lens, then
// you can poke the tiny widgets directly — feel the difference.
//
// This is a self-contained POC. The faithful version lifts the coordinate
// remap into ui.h (which reads the touch pool itself), so every widget-based
// cart gets the loupe for free.

#include "studio.h"

// ── board geometry (board space == canvas space at zoom 1) ────────────────
#define NROW 2
#define NSTEP 16
#define STEP_W 8
#define STEP_H 8
#define STEP_PITCH 11
#define STEP_X0 40
static const int rowY[NROW]  = { 70, 88 };
static const char *rowName[NROW] = { "KICK", "SNR" };

#define NKNOB 4
static const int   knobX[NKNOB] = { 60, 128, 196, 264 };
#define KNOB_Y 140
#define KNOB_R 7
static const char *knobName[NKNOB] = { "TEMPO", "VOL", "TONE", "DECAY" };

// ── loupe constants ───────────────────────────────────────────────────────
#define ZOOM    3.0f
#define LENS_W  120
#define LENS_H  84
#define KNOB_DRAG_PX 60.0f   // board px of finger travel for a full 0..1 sweep

// handle (the one thing that must be easy to hit)
#define HANDLE_W 20
#define HANDLE_H 20
#define HANDLE_X (SCREEN_W - HANDLE_W - 4)
#define HANDLE_Y (SCREEN_H - HANDLE_H - 4)

// ── state ─────────────────────────────────────────────────────────────────
static unsigned char step[NROW][NSTEP];
static float knob[NKNOB] = { 0.5f, 0.8f, 0.4f, 0.6f };

// touch ids can be NEGATIVE (the desktop mouse is id -2), so a "no finger"
// sentinel must be a value no real contact can take — not -1.
#define NO_FINGER 0x7fffffff

static int   lensOn;
static int   posFinger = NO_FINGER;   // finger id currently positioning the lens
static int   lensPanelX, lensPanelY;  // panel top-left, screen space
static float sampleX, sampleY;        // board point shown at panel center

// active interaction captures (a finger that grabbed a widget inside the lens)
#define MAXCAP 4
typedef struct { int id, kind, idx, active, wasIn; float v0, baseY; } Cap; // kind:0 step,1 knob
static Cap caps[MAXCAP];

// previous frame's touch ids, to detect new presses
static int prevId[16], prevN;

// current draw transform (board → view): X = gOX + bx*gZ
static float gOX, gOY, gZ;

// ── helpers ───────────────────────────────────────────────────────────────
static int lensShown(void) { return lensOn || posFinger != NO_FINGER; }

static void lens_xform(float *ox, float *oy) {
    *ox = (lensPanelX + LENS_W / 2.0f) - sampleX * ZOOM;
    *oy = (lensPanelY + LENS_H / 2.0f) - sampleY * ZOOM;
}
static int in_panel(int sx, int sy) {
    return lensShown() && sx >= lensPanelX && sx < lensPanelX + LENS_W
                       && sy >= lensPanelY && sy < lensPanelY + LENS_H;
}
// screen → board, going through the lens when the point is over the panel
static void board_of(int sx, int sy, float *bx, float *by) {
    if (in_panel(sx, sy)) {
        float ox, oy; lens_xform(&ox, &oy);
        *bx = (sx - ox) / ZOOM; *by = (sy - oy) / ZOOM;
    } else { *bx = (float)sx; *by = (float)sy; }
}

static int hit_step(float bx, float by) {
    for (int r = 0; r < NROW; r++)
        for (int s = 0; s < NSTEP; s++) {
            int x = STEP_X0 + s * STEP_PITCH, y = rowY[r];
            if (bx >= x && bx < x + STEP_W && by >= y && by < y + STEP_H)
                return r * NSTEP + s;
        }
    return -1;
}
static int hit_knob(float bx, float by) {
    for (int k = 0; k < NKNOB; k++) {
        float dx = bx - knobX[k], dy = by - KNOB_Y;
        if (dx * dx + dy * dy <= (KNOB_R + 2) * (KNOB_R + 2)) return k;
    }
    return -1;
}
static int touch_find(int id, int *x, int *y) {
    for (int i = 0; i < touch_count(); i++)
        if (touch_id(i) == id) { *x = touch_x(i); *y = touch_y(i); return 1; }
    return 0;
}
static int was_prev(int id) {
    for (int i = 0; i < prevN; i++) if (prevId[i] == id) return 1;
    return 0;
}
static Cap *cap_for(int id) {
    for (int i = 0; i < MAXCAP; i++) if (caps[i].active && caps[i].id == id) return &caps[i];
    return 0;
}
static Cap *cap_free(void) {
    for (int i = 0; i < MAXCAP; i++) if (!caps[i].active) return &caps[i];
    return 0;
}

// place the panel just above the finger, clamped on-screen
static void park_panel(int fx, int fy) {
    sampleX = (float)fx; sampleY = (float)fy;
    lensPanelX = (int)clamp(fx - LENS_W / 2.0f, 0, SCREEN_W - LENS_W);
    lensPanelY = (int)clamp((float)(fy - LENS_H - 16), 0, SCREEN_H - LENS_H);
}

// ── update: all input / state ─────────────────────────────────────────────
void update(void) {
    static int started = 0;
    if (!started) {                       // first-launch: a live-looking pattern + lens parked
        started = 1;
        for (int s = 0; s < NSTEP; s += 4) step[0][s] = 1;     // kick on the beat
        step[1][4] = step[1][12] = 1;                          // snare on 2 & 4
        lensOn = 1; park_panel(knobX[1], KNOB_Y);
    }

    // 1. positioning finger: lens follows it; lift parks it
    if (posFinger != NO_FINGER) {
        int fx, fy;
        if (touch_find(posFinger, &fx, &fy)) park_panel(fx, fy);
        else { posFinger = NO_FINGER; lensOn = 1; }   // lifted → park where it was
    }

    // 2. drive / resolve active captures
    for (int i = 0; i < MAXCAP; i++) {
        Cap *c = &caps[i];
        if (!c->active) continue;
        int fx, fy;
        if (!touch_find(c->id, &fx, &fy)) {            // finger lifted → resolve
            if (c->kind == 0) {                         // step: toggle if released on it
                int rx, ry, ok = 0;
                for (int e = 0; e < touch_ended_count(); e++)
                    if (touch_ended_id(e) == c->id) { rx = touch_ended_x(e); ry = touch_ended_y(e); ok = 1; }
                if (ok) {
                    float bx, by; board_of(rx, ry, &bx, &by);
                    if (hit_step(bx, by) == c->idx) {
                        int r = c->idx / NSTEP, s = c->idx % NSTEP;
                        step[r][s] = !step[r][s];
                    }
                }
            }
            c->active = 0;
            continue;
        }
        if (c->kind == 1) {                             // knob: relative drag, re-anchor at the lens edge
            int nowIn = in_panel(fx, fy);
            float bx, by; board_of(fx, fy, &bx, &by);
            if (nowIn != c->wasIn) { c->wasIn = nowIn; c->v0 = knob[c->idx]; c->baseY = by; }
            knob[c->idx] = clamp(c->v0 + (c->baseY - by) / KNOB_DRAG_PX, 0, 1);
        }
    }

    // 3. new presses this frame
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (was_prev(id)) continue;                     // not a new press
        if (cap_for(id) || id == posFinger) continue;
        int sx = touch_x(i), sy = touch_y(i);

        // handle → (re)position the lens
        if (sx >= HANDLE_X && sx < HANDLE_X + HANDLE_W && sy >= HANDLE_Y && sy < HANDLE_Y + HANDLE_H) {
            posFinger = id; lensOn = 1; park_panel(sx, sy);
            continue;
        }
        // press inside the parked lens → acquire the magnified widget
        if (lensShown() && in_panel(sx, sy)) {
            float bx, by; board_of(sx, sy, &bx, &by);
            int k = hit_knob(bx, by);
            int st = (k < 0) ? hit_step(bx, by) : -1;
            if (k >= 0) {
                Cap *c = cap_free();
                if (c) *c = (Cap){ id, 1, k, 1, 1, knob[k], by };
            } else if (st >= 0) {
                Cap *c = cap_free();
                if (c) *c = (Cap){ id, 0, st, 1, 1, 0, 0 };
            }
            continue;
        }
        // lens parked + tapped empty space → dismiss it
        if (lensOn) { lensOn = 0; continue; }

        // lens off → direct interaction with the tiny board (feel the difference)
        {
            float bx, by; board_of(sx, sy, &bx, &by);
            int k = hit_knob(bx, by);
            int st = (k < 0) ? hit_step(bx, by) : -1;
            if (k >= 0) { Cap *c = cap_free(); if (c) *c = (Cap){ id, 1, k, 1, 0, knob[k], by }; }
            else if (st >= 0) { Cap *c = cap_free(); if (c) *c = (Cap){ id, 0, st, 1, 0, 0, 0 }; }
        }
    }

    // 4. remember this frame's touch ids
    prevN = 0;
    for (int i = 0; i < touch_count() && prevN < 16; i++) prevId[prevN++] = touch_id(i);
}

// ── rendering ─────────────────────────────────────────────────────────────
// draws the board under the current (gOX,gOY,gZ) transform
static void draw_board(int withLabels) {
    // faint checkerboard in BOARD space — at zoom 1 it's a subtle texture; in the
    // lens the cells blow up to 3x, so the magnification is always visible even
    // over empty canvas ("am I zooming anything?" → yes, you can see the pixels).
    int cell = 6;
    for (int by = 0; by < SCREEN_H; by += cell)
        for (int bx = 0; bx < SCREEN_W; bx += cell)
            if (((bx / cell) ^ (by / cell)) & 1) {
                int X = (int)(gOX + bx * gZ), Y = (int)(gOY + by * gZ);
                rectfill(X, Y, (int)(cell * gZ + 0.5f), (int)(cell * gZ + 0.5f), CLR_DARK_PURPLE);
            }

    int heldStep = -1;
    for (int i = 0; i < MAXCAP; i++)
        if (caps[i].active && caps[i].kind == 0) heldStep = caps[i].idx;

    for (int r = 0; r < NROW; r++) {
        if (withLabels) print(rowName[r], 8, rowY[r], CLR_LIGHT_GREY);
        for (int s = 0; s < NSTEP; s++) {
            int X = (int)(gOX + (STEP_X0 + s * STEP_PITCH) * gZ);
            int Y = (int)(gOY + rowY[r] * gZ);
            int W = (int)(STEP_W * gZ), H = (int)(STEP_H * gZ);
            int on = step[r][s];
            int beat = (s % 4 == 0);
            rectfill(X, Y, W, H, on ? CLR_ORANGE : (beat ? CLR_DARK_GREY : CLR_DARKER_GREY));
            rect(X, Y, W, H, (r * NSTEP + s == heldStep) ? CLR_WHITE : CLR_BLACK);
        }
    }
    for (int k = 0; k < NKNOB; k++) {
        int cx = (int)(gOX + knobX[k] * gZ);
        int cy = (int)(gOY + KNOB_Y * gZ);
        int rr = (int)(KNOB_R * gZ);
        circfill(cx, cy, rr, CLR_DARKER_BLUE);
        circ(cx, cy, rr, CLR_LIGHT_GREY);
        float a = 225.0f - knob[k] * 270.0f;
        line(cx, cy, cx + (int)(cos_deg(a) * (rr - 2)), cy - (int)(sin_deg(a) * (rr - 2)), CLR_YELLOW);
        if (withLabels) print(knobName[k], knobX[k] - text_width(knobName[k]) / 2, KNOB_Y + KNOB_R + 4, CLR_MEDIUM_GREY);
    }
}

void draw(void) {
    cls(CLR_DARKER_PURPLE);

    // full board (incl. checker texture), identity transform
    gOX = 0; gOY = 0; gZ = 1.0f;
    draw_board(1);

    // header on top of the texture so it stays readable
    print("808 \x1b LOUPE DEMO", 8, 6, CLR_WHITE);
    print("tiny targets - drag the corner box", 8, 18, CLR_MEDIUM_GREY);

    // the magnified region outline on the board (what the lens is sampling)
    if (lensShown()) {
        int rw = (int)(LENS_W / ZOOM), rh = (int)(LENS_H / ZOOM);
        rect((int)sampleX - rw / 2, (int)sampleY - rh / 2, rw, rh, CLR_DARK_BLUE);
    }

    // the lens panel
    if (lensShown()) {
        rectfill(lensPanelX + 3, lensPanelY + 3, LENS_W, LENS_H, CLR_BLACK);   // shadow
        clip(lensPanelX, lensPanelY, LENS_W, LENS_H);
        rectfill(lensPanelX, lensPanelY, LENS_W, LENS_H, CLR_DARKER_PURPLE);
        float ox, oy; lens_xform(&ox, &oy);
        gOX = ox; gOY = oy; gZ = ZOOM;
        draw_board(0);
        clip(0, 0, 0, 0);
        rect(lensPanelX, lensPanelY, LENS_W, LENS_H, CLR_WHITE);
        rect(lensPanelX - 1, lensPanelY - 1, LENS_W + 2, LENS_H + 2, CLR_DARK_GREY);
        print("3\x0f  2nd finger edits here", lensPanelX + 3, lensPanelY + 2, CLR_LIGHT_GREY);
    }

    // the handle
    int hot = (posFinger != NO_FINGER);
    rrectfill(HANDLE_X, HANDLE_Y, HANDLE_W, HANDLE_H, 4, hot ? CLR_ORANGE : CLR_BLUE);
    rect(HANDLE_X, HANDLE_Y, HANDLE_W, HANDLE_H, CLR_WHITE);
    int hcx = HANDLE_X + HANDLE_W / 2, hcy = HANDLE_Y + HANDLE_H / 2;
    line(hcx - 5, hcy, hcx + 5, hcy, CLR_WHITE);
    line(hcx, hcy - 5, hcx, hcy + 5, CLR_WHITE);

    if (lensOn && posFinger == NO_FINGER)
        print("tap empty space to dismiss", 8, SCREEN_H - 10, CLR_DARK_GREY);
}
