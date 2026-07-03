/* de:meta
{
  "title": "respond",
  "status": "active",
  "created": "2026-06-11",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "description": "A cart-land prototype of a responsive-layout primitive set, before any of it touches the engine. Drag the yellow lower-right corner to resize a 'virtual screen' rectangle - everything inside it is positioned with four candidate layout functions (flex, fluid clamp/%, anchor+inset, aspect-ratio) so you can watch a real layout REFLOW live as the screen changes size: a top bar stays pinned and fluid-height (docked with split), a notification badge stays anchored to the corner, a 16:9 viewport letterboxes itself into what's LEFT after the title + footer bands are split off, and the button bar flips from a row to a stacked column when the width drops under a breakpoint (wide -> phone). The candidate set: flex (cell), fluid (clamp/%), anchor+inset (at), aspect, auto-wrap grid, split (dock a fixed band off an edge), and per-side pad (asymmetric safe-area insets). The idea: prove the toolkit in cart-land against a fake screen first, then graduate it to a runtime/lay.h header that takes the real screen rect. Drag the corner to resize; no other controls."
}
de:meta */
// RESPOND — a cart-land prototype of the responsive-layout primitive set.
//
// The idea (docs/design/responsive-layout.md): before we ever make the
// engine's SCREEN_W/H runtime-variable (the scary part — breaks determinism +
// 45 carts), prototype the WHOLE responsive toolkit in cart-land against a
// FAKE screen. Here that fake screen is a rectangle you resize by dragging its
// lower-right corner. Everything inside it is positioned with the candidate
// primitives — flex, fluid (clamp/%), anchor+inset, aspect, wrap, split (dock a
// fixed band off an edge), pad (per-side / asymmetric safe-area inset) — so you
// can watch a layout reflow live as the "screen" changes size.
//
// If these four feel right here, they graduate to a runtime/lay.h header that
// takes the real screen rect instead of this toy `vs`. The API doesn't change;
// only what you pass as the container does.
//
//   Drag the YELLOW corner handle to resize the virtual screen.
//   Watch the button bar flip row→column under the width breakpoint.

#include "studio.h"

// The lay_* layout vocabulary was PROMOTED from this prototype to runtime/lay.h;
// this cart is now a live demo of that header. `binside(box,x,y)` is lay.h's
// point-in-box test (was the local `inside`).
#include "lay.h"

// ───────────────────────── the virtual screen + drag-to-resize ──────────────
#define GRAB    9          // size of the corner resize handle
#define MARGIN  10         // keep the virtual screen this far off the real edges
#define MIN_W   70
#define MIN_H   60
#define BP_W    150        // width breakpoint: below this, "phone" layout

static Box  vs;            // the virtual screen
static bool dragging;
static bool inited;

static void ensure_init(void) {
    if (inited) return;
    vs = box(MARGIN, MARGIN + 14, 230, 150);
    inited = true;
    dragging = false;
}

static Box handle_rect(void) { return box(vs.x + vs.w - GRAB, vs.y + vs.h - GRAB, GRAB, GRAB); }

void update(void) {
    ensure_init();

#ifdef DE_RESIZABLE
    // Phase 1c (device-adaptive-layout.md): built resizable, the REAL window IS the responsive
    // surface. No fake rect, no drag handle — drag the OS window and the engine reflows
    // screen_w()/screen_h() live; the identical lay.h code below re-flows against it.
    vs = box(0, 0, screen_w(), screen_h());
    return;
#endif

    // unified pointer: first touch on mobile, else the mouse
    int px = touch_count() > 0 ? touch_x(0) : mouse_x();
    int py = touch_count() > 0 ? touch_y(0) : mouse_y();
    Box h = handle_rect();

    bool began = (mouse_pressed(MOUSE_LEFT) && binside(h, mouse_x(), mouse_y()))
               || tapp((int)h.x - 3, (int)h.y - 3, GRAB + 6, GRAB + 6);   // fat-finger pad
    if (began) dragging = true;

    bool held = mouse_down(MOUSE_LEFT) || touch_count() > 0;
    if (dragging && held) {
        vs.w = lay_clamp(px - vs.x + GRAB / 2, MIN_W, SCREEN_W - vs.x - MARGIN);
        vs.h = lay_clamp(py - vs.y + GRAB / 2, MIN_H, SCREEN_H - vs.y - MARGIN);
    } else {
        dragging = false;
    }
}

// ───────────────────────── the responsive layout, drawn live ────────────────
void draw(void) {
    ensure_init();
    cls(CLR_BROWNISH_BLACK);

    bool narrow = vs.w < BP_W;
#ifndef DE_RESIZABLE
    // ---- outer guidance, on the REAL screen (not responsive) ----------------
    font(FONT_SMALL);
    print("RESPOND - a cart-land responsive-layout prototype", MARGIN, 4, CLR_LIGHT_GREY);
    print_right(str("virtual screen  %dx%d   %s",
                    (int)vs.w, (int)vs.h, narrow ? "[phone]" : "[wide]"),
                SCREEN_W - MARGIN, SCREEN_H - 10,
                narrow ? CLR_ORANGE : CLR_GREEN);
    print("drag the yellow corner >>", MARGIN, SCREEN_H - 10, CLR_DARK_GREY);

    // ---- the device bezel + screen fill -------------------------------------
    boxfill(lay_inset(vs, -2), CLR_DARK_GREY);          // bezel
    boxfill(vs, CLR_DARKER_BLUE);                        // the "screen"
#else
    // resizable build: the window itself is the screen — fill it, no bezel/handle
    boxfill(vs, CLR_DARKER_BLUE);
#endif

    // From here on, vs IS the screen. Clip so nothing spills past it, exactly
    // as the real engine clips a cart to SCREEN_W/H.
    clip((int)vs.x, (int)vs.y, (int)vs.w, (int)vs.h);

    Box c = lay_inset(vs, lay_fluid(0.04f, vs.w, 3, 8));   // fluid padding (% of width)

    // fluid type: pick a font by how wide the screen is (a breakpoint chain)
    int body_font = vs.w < 110 ? FONT_TINY : vs.w < 200 ? FONT_SMALL : FONT_NORMAL;

    // 1) ANCHOR + FLUID — a title bar pinned to the top, fluid height ---------
    float barH = lay_fluid(0.14f, c.h, 12, 22);
    Box title = lay_at(c, L_T, c.w, barH, 0);
    boxfill(title, CLR_TRUE_BLUE);
    font(body_font);
    print((int)title.h >= 8 ? "Dashboard" : "Dash",
          (int)title.x + 4, (int)(title.y + (title.h - 8) / 2), CLR_LIGHT_PEACH);

    // a notification badge anchored to the title's top-right corner (+inset) --
    float bs = lay_fluid(0.5f, barH, 6, 12);
    Box badge = lay_at(title, L_TR, bs, bs, 2);
    circfill((int)(badge.x + bs / 2), (int)(badge.y + bs / 2), (int)(bs / 2), CLR_RED);

    // 3) the button bar — FLEX, reflows row -> column under the breakpoint ---
    int nbtn = 3;
    float gap = lay_fluid(0.02f, c.w, 2, 5);
    float footH = narrow ? nbtn * 12 + (nbtn - 1) * gap   // stacked needs height
                         : lay_fluid(0.16f, c.h, 12, 20);
    footH = lay_clamp(footH, 10, c.h - barH - 18);        // never eat the whole screen
    Box foot = lay_at(c, L_B, c.w, footH, 0);
    const char *labels[3] = {"Play", "Share", "More"};
    int btn_clr[3] = {CLR_MEDIUM_GREEN, CLR_INDIGO, CLR_MAUVE};
    for (int i = 0; i < nbtn; i++) {
        Box b = lay_cell(foot, narrow ? 1 : 0, nbtn, i, gap);   // dir flips on breakpoint
        boxfill(b, btn_clr[i]);
        boxrect(b, CLR_BROWNISH_BLACK);
        font(body_font);
        if (b.w > text_width(labels[i]) + 2)
            print_centered(labels[i], (int)(b.x + b.w / 2), (int)(b.y + (b.h - 8) / 2), CLR_WHITE);
    }

    // 4) ASPECT — a 16:9 viewport filling the gap between title and buttons.
    // The middle is just what's LEFT after docking the title + footer bands
    // (with a gap each) off `c` — lay_split, not hand arithmetic.
    Box mid;
    lay_split(c,  EDGE_TOP,    title.h + gap, &mid);
    lay_split(mid, EDGE_BOTTOM, foot.h + gap, &mid);
    if (mid.h > 8) {
        boxfill(mid, CLR_DARKER_PURPLE);                 // the available area
        Box view = lay_aspect(mid, 16.0f / 9.0f);        // contained 16:9
        boxfill(view, CLR_BLUE_GREEN);
        boxrect(view, CLR_BLUE);

        // ...and inside the viewport, a WRAP grid: a palette of swatches that
        // auto-fits as many columns as the viewport width allows. Drag the
        // screen narrower and watch the columns fall 4→3→2→1 on their own.
        int sw_clr[8] = {CLR_RED, CLR_ORANGE, CLR_YELLOW, CLR_GREEN,
                         CLR_BLUE, CLR_INDIGO, CLR_PINK, CLR_PEACH};
        Box grid = lay_inset(view, 3);
        float sgap = 2, minItem = 13;
        if (grid.w > 8 && grid.h > 8) {
            for (int i = 0; i < 8; i++) {
                Box s = lay_wrap(grid, 8, i, minItem, sgap);
                if (s.w < 2 || s.h < 2) continue;         // too small to bother
                boxfill(lay_inset(s, 0.5f), sw_clr[i]);
                boxrect(s, CLR_DARKER_BLUE);
            }
        }
    }

    clip(0, 0, 0, 0);   // back to the real screen

#ifndef DE_RESIZABLE
    // ---- the resize handle, on top ------------------------------------------
    Box h = handle_rect();
    boxfill(h, dragging ? CLR_WHITE : CLR_YELLOW);
    // little grip diagonal
    line((int)h.x + 2, (int)(h.y + h.h - 2), (int)(h.x + h.w - 2), (int)h.y, CLR_BROWNISH_BLACK);
#endif

    font(FONT_NORMAL);
}
