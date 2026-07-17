#ifndef STUDIO_H
#define STUDIO_H

#include <stdbool.h>

// ------------------------------------------------------------
// screen
// ------------------------------------------------------------

#ifndef SCREEN_W
#define SCREEN_W  320
#endif
#ifndef SCREEN_H
#define SCREEN_H  200
#endif

// SCREEN_W/H are the compile-time MAX canvas size. A cart built with -DDE_RESIZABLE gets a
// resizable window whose ACTIVE size reflows live — read it with these instead of the macros,
// and lay out your UI against them so it reflows (see runtime/lay.h). On a normal cart they
// just return SCREEN_W/SCREEN_H forever.
int  screen_w(void);   // active canvas width in px  (== SCREEN_W unless the cart is -DDE_RESIZABLE)
int  screen_h(void);   // active canvas height in px (== SCREEN_H unless the cart is -DDE_RESIZABLE)
void safe_rect(int *x, int *y, int *w, int *h);   // usable area after device safe-area insets (notch/home-bar); lay controls inside it, bleed the background to screen_w/h. All-screen (0,0,screen_w,screen_h) on desktop.
int  finger_px(void);       // logical px for a comfortable ~44pt touch target (from the device backing scale; 22 at the iOS 2× chunk). Size finger controls to this — never a raw px guess (that only works by SCALE=1 coincidence).
int  device_class(void);    // classify the live screen: 0 = TALL (phone portrait) · 1 = WIDE (phone landscape) · 2 = ROOMY (tablet). Pick arrangements off this, not a device name.

// ------------------------------------------------------------
// buttons
// ------------------------------------------------------------

#define BTN_UP     0
#define BTN_DOWN   1
#define BTN_LEFT   2
#define BTN_RIGHT  3
#define BTN_A      4   // z / gamepad a
#define BTN_B      5   // x / gamepad b
#define BTN_X      6   // c / gamepad x — 3rd face button (numbered to append, not insert — room for
#define BTN_Y      7   // v / gamepad y — 4th face button    L1/R1/L2/R2/stick-clicks later, unnumbered here)

// ------------------------------------------------------------
// palette (32 fixed colors, 0-31) — the PICO-8 palette
// names per https://pico-8.fandom.com/wiki/Palette
// ------------------------------------------------------------

// standard 16 (0-15)
// NB: the greys are British-spelled — GREY, not GRAY (CLR_*_GRAY won't compile).
#define CLR_BLACK          0   // #000000
#define CLR_DARK_BLUE      1   // #1d2b53
#define CLR_DARK_PURPLE    2   // #7e2553
#define CLR_DARK_GREEN     3   // #008751
#define CLR_BROWN          4   // #ab5236
#define CLR_DARK_GREY      5   // #5f574f
#define CLR_LIGHT_GREY     6   // #c2c3c7
#define CLR_WHITE          7   // #fff1e8
#define CLR_RED            8   // #ff004d
#define CLR_ORANGE         9   // #ffa300
#define CLR_YELLOW         10  // #ffec27
#define CLR_GREEN          11  // #00e436
#define CLR_BLUE           12  // #29adff
#define CLR_INDIGO         13  // #83769c
#define CLR_PINK           14  // #ff77a8
#define CLR_LIGHT_PEACH    15  // #ffccaa

// extended 16 (16-31) — undocumented "secret" colors, named arbitrarily
#define CLR_BROWNISH_BLACK 16  // #291814
#define CLR_DARKER_BLUE    17  // #111d35
#define CLR_DARKER_PURPLE  18  // #422136
#define CLR_BLUE_GREEN     19  // #125359
#define CLR_DARK_BROWN     20  // #742f29
#define CLR_DARKER_GREY    21  // #49333b
#define CLR_MEDIUM_GREY    22  // #a28879
#define CLR_LIGHT_YELLOW   23  // #f3ef7d
#define CLR_DARK_RED       24  // #be1250
#define CLR_DARK_ORANGE    25  // #ff6c24
#define CLR_LIME_GREEN     26  // #a8e72e
#define CLR_MEDIUM_GREEN   27  // #00b543
#define CLR_TRUE_BLUE      28  // #065ab5
#define CLR_MAUVE          29  // #754665
#define CLR_DARK_PEACH     30  // #ff6e59
#define CLR_PEACH          31  // #ff9d81

// ------------------------------------------------------------
// callbacks — you implement these (update is optional)
// ------------------------------------------------------------

void init(void);    // called once after the window opens — optional
void draw(void);
void update(void);
int  de_players(void);   // return 2+ from your cart to enable the multiplayer menu/lobby (default 1 = solo)

// ------------------------------------------------------------
// api
// ------------------------------------------------------------

// input
bool btn(int player, int button);   // true while button is held
bool btnp(int player, int button);  // true only on the frame the button was pressed
bool btnr(int player, int button);  // true only on the frame the button was released

// touch
int  touch_count(void);                  // active touches (0..10)
int  touch_x(int i);                     // canvas-space x, or -1 if i invalid
int  touch_y(int i);                     // canvas-space y, or -1
int  touch_id(int i);                    // stable id of touch i, or -1 — indices shuffle when a finger lifts, ids follow the finger
bool tap(int x, int y, int w, int h);    // any touch inside this canvas-space rect?
bool tapp(int x, int y, int w, int h);   // did a touch BEGIN inside this rect this frame? (edge-triggered, like btnp)
int  touch_ended_count(void);            // fingers that lifted since last frame — a lifted finger has no touch_x index, read it here
int  touch_ended_id(int i);              // stable id of lifted finger i, or -1 — pair it with the id you tracked while it was down
int  touch_ended_x(int i);               // canvas-space x where lifted finger i was last seen, or -1
int  touch_ended_y(int i);               // canvas-space y where lifted finger i was last seen, or -1
bool tapr(int x, int y, int w, int h);   // did a touch END inside this rect this frame? (edge-triggered, like btnr)
void touch_controls(bool on);            // show/hide on-screen stick + A/B (overrides default)
#define TOUCH_ANALOG      0              // floating stick: base spawns where the thumb lands — great for frantic free movement
#define TOUCH_ANALOG_FIX  1              // fixed stick: base stays put at a home spot — steadier for precise play
#define TOUCH_DPAD4       2              // 4-way d-pad: up/down/left/right only — grid & precise games (sokoban, puzzle)
#define TOUCH_DPAD8       3              // 8-way d-pad: diagonals too — fighters, 8-way action
void touch_layout(int move_mode, int n_buttons); // opt in to the on-screen controls AND pick the move style (TOUCH_ANALOG / TOUCH_ANALOG_FIX / TOUCH_DPAD4 / TOUCH_DPAD8) + button count
int  touch_ceiling(void);                // max fingers this device tracks: 5 iPhone, 10 iPad, 0 desktop/unknown — one MORE than this cancels ALL touches on iOS
#define TOUCH_LAYOUT_OVERLAY  0          // controls hug a game-rect corner, on top (no spare letterbox to place them into)
#define TOUCH_LAYOUT_DECK     1          // controls live in a band below the game (portrait)
#define TOUCH_LAYOUT_RAILS    2          // controls live in bands beside the game (landscape)
int   touch_layout_mode(void);           // which placement the engine picked this frame — one of TOUCH_LAYOUT_*
float touch_ctrl_scale(void);            // 0..1 — how much the on-screen controls are shrunk to fit a tight band (1 = full size)
void  touch_debug(bool on);              // draw the control band + grab-zone outlines (a dev aid — leave off in shipped carts)

// analog stick (only nonzero while a finger is on the on-screen stick)
float stick_x(void);   // -1.0 .. 1.0
float stick_y(void);   // -1.0 .. 1.0  (negative = up)

// mouse (desktop) — unlike touch, position is always known, even with no button down
#define MOUSE_LEFT    0
#define MOUSE_RIGHT   1
#define MOUSE_MIDDLE  2
int   mouse_x(void);                 // canvas-space x of the pointer (clamped to screen)
int   mouse_y(void);                 // canvas-space y of the pointer (clamped to screen)
bool  mouse_down(int button);        // true while the button is held
bool  mouse_pressed(int button);     // true only on the frame the button was pressed
bool  mouse_released(int button);    // true only on the frame the button was released
float mouse_wheel(void);             // scroll this frame: + up / - down, 0 if no scroll
int   mouse_world_x(void);           // mouse x in world space — undoes the active camera() shift (handy for click-on-world)
int   mouse_world_y(void);           // mouse y in world space — undoes the active camera() shift

// mouse cursor shape — set it when it changes, not every frame (works on web too: maps to the canvas CSS cursor)
#define CURSOR_DEFAULT   0   // normal arrow
#define CURSOR_HAND      1   // pointing hand — for clickable / pressable things
#define CURSOR_CROSSHAIR 2   // crosshair — for precise placement / drawing
#define CURSOR_MOVE      3   // four-way move arrows — for dragging / panning
#define CURSOR_TEXT      4   // I-beam — for text-entry fields
#define CURSOR_NO        5   // not-allowed — for an illegal action
#define CURSOR_RESIZE_H  6   // left-right resize arrows — for dragging an edge/endpoint horizontally (timeline, slider)
void  mouse_cursor(int kind);        // set the pointer shape (CURSOR_*); persists until you change it
void  mouse_hide(void);              // hide the OS pointer — e.g. to draw your own cursor sprite. scoped to the cart window
void  mouse_show(void);              // show the OS pointer again

// keyboard (desktop) — for letters/digits just pass the character: key('A'), key('0')
#ifndef STUDIO_INTERNAL              // these match raylib's KeyboardKey values; hidden from studio.c to avoid clashing with them
#define KEY_SPACE      32
#define KEY_ESCAPE     256
#define KEY_ENTER      257
#define KEY_TAB        258
#define KEY_BACKSPACE  259
#define KEY_RIGHT      262
#define KEY_LEFT       263
#define KEY_DOWN       264
#define KEY_UP         265
#endif
bool key(int k);                     // true while key k is held. letters/digits: key('A'), key('5'); specials: KEY_SPACE, KEY_LEFT...
bool keyp(int k);                    // true only on the frame key k was pressed
bool keyr(int k);                    // true only on the frame key k was released
const char *text_input(void);        // characters typed this frame (for name entry / word games); "" if none
const char *paste(void);             // clipboard text on the frame Ctrl/Cmd+V is pressed, else "" — append it to your text_input() buffer

// graphics
void cls(int color);
void colorkey(int color);                           // set transparent color for sprites (palette index). -1 = no transparency. call when color changes, not every frame.
void spr(int index, int x, int y);
void sprf(int index, int x, int y, bool flip_x, bool flip_y);                  // sprite with flips
void sspr(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);     // sub-rect → dest rect (stretched)
void spr_rot(int index, int x, int y, float deg);                             // spin a whole sprite `deg` degrees around its center. (x,y) = top-left like spr(). the everyday rotate
void sspr_ex(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, float deg, int ox, int oy); // the works: source sub-rect → dest rect (scale), rotated `deg` around pivot (ox,oy) in the dest. flip with negative sw/sh
// active font — set once, all print calls use it until you change it
#define FONT_NORMAL  0   // default 8×8 DOS font
#define FONT_SMALL   1   // 4×6 font — fits ~64 chars across 320px
#define FONT_TINY    2   // 3×5 font — fits ~80 chars across 320px
#define FONT_COMIC   3   // Comic Mono Bold baked to pixels (18px, 10×20) — friendly rounded handwriting; great for titles/dialogue
#define FONT_THIN    4   // IBM CGA "thin" 8×8 — the narrow-stroke alternate CGA ROM font; lighter than the default 8×8
void font(int f);        // set active font for all print calls; font(FONT_NORMAL) resets to the default 8×8
int  text_width(const char *text);                                 // pixel width using the active font — for centering in your own boxes
int  print(const char *text, int x, int y, int color);             // returns x after the last char (so you can chain or check if text went offscreen)
int  print_centered(const char *text, int x, int y, int color);    // draw text centered on x; returns x after last char
int  print_right(const char *text, int right_x, int y, int color); // right-align text at right_x; returns x after last char
void hint(const char *text);                                       // bottom-anchored control-hint footer ("Z jump  X shoot"). auto-picks the largest of the 8×8/small/tiny fonts that fits the screen width, muted light-grey — so it never runs off the edge
int  print_scaled(const char *text, int x, int y, int color, int scale); // bigger text for titles/menus (scale 2 = double size); returns x after last char
int  print_outline(const char *text, int x, int y, int color, int outline_color); // text with a 1px outline in all 8 directions; maximum legibility
int  print_rot(const char *text, int x, int y, float deg, int color, int pivot); // text rotated `deg` degrees. pivot 0 = around (x,y)=top-left, 1 = around text center
int  print_rot_scaled(const char *text, int x, int y, float deg, int scale, int color, int pivot); // rotated AND scaled (scale 2 = double size) in one call; same pivot rule as print_rot. crisp at integer scale
void line(int x1, int y1, int x2, int y2, int color);
void bezier(int x0, int y0, int cx, int cy, int x1, int y1, int color);                                                    // quadratic Bezier: smooth curve from (x0,y0) to (x1,y1) pulled toward control point (cx,cy)
void pset(int x, int y, int color);                     // set a single pixel (pairs with pget)
void pset_rgb(int x, int y, int hex);                   // set a single pixel to a raw 0xRRGGBB colour, bypassing the palette — full 24-bit colour, for CPU shaders / true-colour gradients
void rect(int x, int y, int w, int h, int color);       // rectangle border
void rectfill(int x, int y, int w, int h, int color);   // filled rectangle
void rectfill_rgb(int x, int y, int w, int h, int hex);  // filled rectangle in a raw 0xRRGGBB colour, bypassing the palette — true-colour blocks (CPU shaders, gradients)
void rectfill_rot(int cx, int cy, int w, int h, float deg, int color);  // filled rect CENTRED at (cx,cy), rotated `deg`° — GPU geometry, so it stays HOLE-FREE under a rotated camera_ex() (unlike trifill/polyfill, which fill in software and gap when the view spins). oriented sprites-of-one-colour: cars, debris, blades
void bar(int x, int y, int w, int h, float pct, int fill, int bg); // progress/health bar: bg box + left-to-right fill, pct 0..1 (clamped)
// fill patterns — 16-bit 4×4 bitmasks for fillp(). bits read top-left→bottom-right (bit 15 = top-left).
// 0-bits take the draw color, 1-bits take fillp's hole_color. compatible with PICO-8 fillp hex values.
#define FILL_SOLID    0xFFFF
#define FILL_CHECKER  0xA5A5   // 50% checkerboard
#define FILL_DOTS     0x8020   // sparse dots
#define FILL_HLINES   0xF0F0   // horizontal stripes
#define FILL_VLINES   0xAAAA   // vertical stripes
#define FILL_DIAG     0x8421   // diagonal lines
#define FILL_GRID     0xF888   // grid / brick lines
// global fill pattern (PICO-8 fillp style): once set, the normal fills — rectfill, circfill,
// ovalfill, trifill — draw the 4×4 pattern. the draw COLOR fills the 0-bits; the 1-bits use
// `hole_color` (pass -1 to make them transparent so the background shows through).
// pass any 16-bit pattern (FILL_CHECKER etc, or your own). fillp_reset() goes back to solid.
void fillp(int pattern, int hole_color);
void fillp_reset(void);
// shift the fill-pattern lattice origin to (ox,oy). by default the 4×4 pattern is pinned to
// world (0,0), so a MOVING shape slides over a fixed lattice and the dither appears to crawl.
// anchor to a shape's own center before filling it and the pattern travels with the shape.
void fillp_anchor(int ox, int oy);
// blend modes — once set, everything drawn MIXES with what's already on screen (index-only, so the
// canvas still holds palette colors). the mix is snapped to the nearest of the palette, so a little
// banding is normal — that IS the look. blend_reset() (or BLEND_NONE) goes back to plain overwrite.
#define BLEND_NONE 0   // plain overwrite (the default)
#define BLEND_AVG  1   // 50% average — glass, water, soft shadow (AVG with black = a drop shadow)
#define BLEND_ADD  2   // additive — glow, light, fire, lasers (brightens whatever is behind)
#define BLEND_MUL  3   // multiply — fog, darken, tint (deepens + tints whatever is behind)
#define BLEND_SUB  4   // subtractive — carve light out toward black (cold shadow, scorch); the opposite of ADD
void blend(int mode);                                   // start mixing every following draw with the screen: BLEND_AVG / BLEND_ADD / BLEND_MUL / BLEND_SUB. persists until blend_reset()
void blend_reset(void);                                 // stop blending — back to plain overwrite (the default)
void circ(int x, int y, int radius, int color);         // circle border
void circfill(int x, int y, int radius, int color);     // filled circle
void oval(int x, int y, int rx, int ry, int color);     // ellipse border (rx,ry = half-width/height)
void ovalfill(int x, int y, int rx, int ry, int color); // filled ellipse — squashed circles, eyes, shadows
void arc(int x, int y, int radius, float start_deg, float end_deg, int color);           // arc outline — part of a circle's rim (degrees, 0=right 90=down, like dx/dy)
void arcfill(int x, int y, int radius, float start_deg, float end_deg, int color);       // filled pie wedge (sector) — cooldown sweeps, pie slices, half-circles
void arcoutline(int x, int y, int radius, float start_deg, float end_deg, int color);    // outline of a filled pie wedge — rim + the two straight radial edges (hugs arcfill)
void ring(int x, int y, int r_in, int r_out, float start_deg, float end_deg, int color); // filled thick arc (ring sector) — gauges, radial bars, dials
void ringoutline(int x, int y, int r_in, int r_out, float start_deg, float end_deg, int color); // outline of a ring/annulus sector — hugs ring()
void tri(int x1, int y1, int x2, int y2, int x3, int y3, int color);     // triangle border
void trifill(int x1, int y1, int x2, int y2, int x3, int y3, int color); // filled triangle (any winding)
void ngon(int x, int y, int r, int sides, float rot, int color);          // regular n-sided polygon outline. rot=0 → first point right, degrees
void ngonfill(int x, int y, int r, int sides, float rot, int color);      // filled regular polygon — respects fillp(). ngonfill(x,y,r,6,0,c) = hexagon
void star(int x, int y, int r_out, int r_in, int points, float rot, int color);     // star outline. r_out=tip radius, r_in=inner radius
void starfill(int x, int y, int r_out, int r_in, int points, float rot, int color); // filled star — respects fillp()
void poly(int *xy, int n, int color);      // polygon outline through n points; xy = {x0,y0, x1,y1, ...}, auto-closed
void polyfill(int *xy, int n, int color);  // filled polygon (even-odd coverage) — handles concave; respects fillp()
void thickline(int x1, int y1, int x2, int y2, int w, int color); // line with pixel width w — wider strokes, drawings, cables
void thicklineoutline(int x1, int y1, int x2, int y2, int w, int color); // outline of a thick line (capsule boundary) — hugs thickline
void rrect(int x, int y, int w, int h, int r, int color);         // rounded rectangle outline. r = corner radius
void rrectfill(int x, int y, int w, int h, int r, int color);     // filled rounded rectangle — panels, buttons, speech bubbles
void gradient(int x, int y, int w, int h, int c_a, int c_b, float angle_deg); // dithered gradient at any angle — 0=left→right, 90=top→bottom, any value in between
void vgradient(int x, int y, int w, int h, int c_top, int c_bot);    // vertical dithered gradient from c_top to c_bot — skies, backdrops
void hgradient(int x, int y, int w, int h, int c_left, int c_right); // horizontal dithered gradient — side lighting, panels
void tritex(int x1, int y1, float u1, float v1, int x2, int y2, float u2, float v2, int x3, int y3, float u3, float v3); // texture-mapped triangle: each corner maps a screen point to a sprite-sheet pixel (u,v). affine (PS1-style). the textured-3D primitive
int  pget(int x, int y);                                // palette index at (x,y), or 0 if no match (true-colour pset_rgb/rectfill_rgb pixels rarely match — use pget_rgb to read those back)
int  pget_rgb(int x, int y);                            // raw 0xRRGGBB at (x,y), or -1 if off-screen — the true-colour read-back (pairs with pset_rgb for feedback shaders); reads last frame, like pget
void enable_pget(bool on);                              // turn canvas read-back ON so pget/pget_rgb/touching_color work — OFF by default; call enable_pget(true) in init() if your cart reads pixels (others pay nothing)
int  sget(int sx, int sy);                              // palette index of the spritesheet pixel at (sx,sy) — read sprites as data (paint a level, lookup tables). 0 if out of range
void camera(int x, int y);                              // plain camera: shifts all drawing by (-x,-y), no zoom/rotation. camera(0,0) resets
void camera_ex(int x, int y, float zoom, float angle);  // camera with zoom (1=normal, 2=2x in) and angle (degrees), pivoting on screen center. for a screen-space HUD after zooming, call camera(0,0)
void smooth_zoom(bool on);                              // render camera_ex's fractional-zoom world at 1:1 then scale it in one blit — stops thin lines crawling under a fractional zoom. one toggle, default off; affects only the world between camera_ex and camera(0,0) (HUD stays crisp)
void follow(int tx, int ty, int world_w, int world_h); // center camera on (tx,ty), clamped so world edges don't show
void clip(int x, int y, int w, int h);                  // scissor rect. clip(0,0,0,0) disables
void zoom_rect(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh); // copy the canvas region (sx,sy,sw,sh) scaled into (dx,dy,dw,dh) — nearest-neighbor, so zoom shows crisp pixels. samples the frame drawn so far (call it after that content). magnifier loupe, picture-in-picture, minimap
void pal(int c0, int c1);                               // remap: everything drawn as c0 now draws as c1 — primitives AND sprites (hit-flash, team/clothes recolor, fades). persists until pal_reset()
void pal_reset(void);                                   // undo all pal() swaps — back to the normal palette
void fade(float amount);                                // darken the whole screen toward black for ONE frame: 0 = normal, 1 = black. clears automatically — call it every frame you want it (e.g. inside a game-over overlay)
void shake(float amount);                               // kick the screen by `amount` pixels; decays on its own. call on impacts

// ------------------------------------------------------------
// map — 128 × 64 grid of sprite indices. cell value 0 = empty.
// ------------------------------------------------------------

#ifndef MAP_W
#define MAP_W  128
#endif
#ifndef MAP_H
#define MAP_H   64
#endif

// map cell size in pixels — how big a rect each cell pulls from the spritesheet.
// independent of the sprite editor's tile size; defaults to it (16). set via -DCELL_W/-DCELL_H.
#ifndef CELL_W
#define CELL_W 16
#endif
#ifndef CELL_H
#define CELL_H 16
#endif

int  mget(int cx, int cy);                              // sprite index at cell (cx,cy); 0 if out of bounds
void mset(int cx, int cy, int n);                       // write sprite index n into cell (cx,cy)
void map(int cx, int cy, int sx, int sy, int cw, int ch); // draw a cw×ch chunk of the map starting at cell (cx,cy) to screen (sx,sy). each cell is a CELL_W×CELL_H rect of the sheet. value-0 cells skipped. respects camera() + map_scale().
void map_scale(int n);                                  // integer zoom for map drawing; default 1 (cells drawn at native CELL_W×CELL_H)

// ------------------------------------------------------------
// sound — PICO-8-style 4-channel synth
// ------------------------------------------------------------

#define INSTR_SQUARE  0
#define INSTR_SAW     1
#define INSTR_TRI     2
#define INSTR_NOISE   3
#define INSTR_SINE    4
// custom DRAWN waveforms — four single-cycle tables you fill with wave_set(), then play
// like any wave: note(60, INSTR_USER0, 5). Draw one in the "wave editor" cart.
#define INSTR_USER0   5
#define INSTR_USER1   6
#define INSTR_USER2   7
#define INSTR_USER3   8
// modeled ENGINES — wave ids 16+. An engine computes its sound per note (a tiny physical
// simulation, not a wavetable). Wrap one in a slot like any wave — instrument(5, INSTR_PLUCK, …)
// — and shape it with the three macro knobs below (instrument_harmonics/timbre/morph).
#define INSTR_PLUCK   16  // Karplus-Strong plucked string — guitar/harp/koto. Decays on its own (give it a long hit() or release); vibrato/glide/pitch-env all bend it live. macros: harmonics = ring time, timbre = pick brightness, morph = pick position
#define INSTR_MALLET  17  // struck bar — marimba/celesta/vibraphone/glockenspiel. Four decaying sine modes; rings on its own like PLUCK. macros: harmonics = bar material (wood→bell), timbre = mallet hardness, morph = ring length (top = motor tremolo)
#define INSTR_FM      18  // 2-op FM — DX-style epiano/bells/bass/brass/clang. Brightness decays within each note like a real DX strike. macros: harmonics = carrier:mod ratio (snapped detents — integers harmonic, offs = bells), timbre = brightness (mod index), morph = feedback (clean → growl → clang)
#define INSTR_ORGAN   19  // tonewheel organ — Hammond/gospel/jazz drawbar registrations. 9 additive sines, holds while sustained. macros: harmonics = registration (snapped drawbar recipes — thin combo → full gospel), timbre = brightness tilt + key click, morph = animation (scanner chorus + percussion; 0 = still combo organ)
#define INSTR_EPIANO  20  // electric piano — Rhodes/Wurlitzer/Clavinet. 12 decaying sine modes through a pickup nonlinearity; struck, rings down on its own (give a long hit()). macros: harmonics = instrument (snapped Rhodes/Wurli/Clav), timbre = brightness (pickup + hammer), morph = bark (dig-in growl)
#define INSTR_PD      21  // phase-distortion (Casio CZ) — buzzy basses, resonant leads, the CZ "wowww" sweep. Holds while sustained. macros: harmonics = wavetype (snapped: saw/square/pulse/doublepulse/sawpulse + 3 resonant), timbre = distortion (brightness / resonant-peak), morph = DCW sweep (distortion snaps bright on the strike, settles — the CZ envelope)
#define INSTR_MEMBRANE 22 // struck drumhead — tabla/conga/bongo/djembe/tom. Six decaying sine modes at circular-membrane ratios; rings on its own like MALLET. macros: harmonics = head (tuned tabla → inharmonic djembe), timbre = strike position (center thump → edge ring/slap), morph = pitch-bend (the tabla bayan gliss; 0 = flat)
#define INSTR_REED    23  // blown reed — clarinet/sax/oboe. A self-oscillating bore waveguide; holds while sustained (like ORGAN). macros: harmonics = bore (cylindrical clarinet → conical sax), timbre = reed edge (dark → bright/nasal), morph = breath (soft → leaning growl + vibrato)
#define INSTR_PIPE    25  // blown flute — flute/recorder/pan pipe. A self-oscillating STK jet-drive bore; holds while sustained, breathy like a real flute. macros: harmonics = overblow (fundamental → octave flageolet + bright), timbre = breath air (pure → airy), morph = embouchure (hollow/dark → focused/bright). ⚠ TUNING: intonation tracks the morph (embouchure) macro — in tune for focused embouchure (morph ≳ 0.5, e.g. the showcase flute m0.70), but a low/hollow embouchure (morph ≲ 0.4) and overblow (harmonics) drift flat/unstable at the top. VERIFY a new flute voice's register: node tools/tune-check.js --engine PIPE --macros h,t,m --range lo-hi
#define INSTR_VOICE   24  // formant voice (navkit VoicForm port) — sing & speak. Holds while sustained. macros: harmonics = VOWEL (U→O→A→E→I), timbre = SIZE (vocal-tract length, giant→child), morph = EFFORT (soft/dark/relaxed → bright/pressed). voice_nasal() adds the nasal/hummed color; voice_consonant()/voice_coda() articulate syllables (full phoneme set). vibrato = note_lfo(LFO_PITCH)
#define INSTR_GUITAR  26  // plucked string + body — acoustic/nylon/banjo/koto/harp/uke/pizzicato. A Karplus-Strong string through a resonant body (richer than bare PLUCK); decays on its own (give it a long hit()/release). macros: harmonics = body (open harp → resonant box → bright banjo), timbre = string brightness (warm nylon → bright steel), morph = mute (long ring → tight pizzicato stop)
#define INSTR_PIANO   27  // struck stiff string (StifKarp) — grand/bright piano, harpsichord, dulcimer, clavichord, celesta. A near-lossless Karplus-Strong string + dispersion (inharmonic shimmer) + soundboard; struck, rings down on its own (give it a long hit()). macros: harmonics = voicing (snaps grand → bright → harpsichord → dulcimer → clavichord → celesta), timbre = hammer (soft felt → hard plectrum), morph = pedal (damped staccato → long open sustain)
#define INSTR_BOWED   28  // bowed string — violin/cello/viola. A Smith/McIntyre stick-slip friction waveguide; self-oscillates and HOLDS while sustained (like a real bow). Speaks with an attack scratch and swells. macros: harmonics = bow position (sul ponticello bright → sul tasto soft), timbre = bow pressure (clean leaning-sawtooth → scratchy surface), morph = bow speed/swell (gentle → digging in, louder + cleaner). PIZZICATO: instrument_mode(slot,MODE_BOW_PIZZ,1) plucks the same string + body instead of bowing it (rings down on its own — give it a long hit())
#define INSTR_BRASS   29  // lip-reed brass — trumpet/cornet/flugelhorn/trombone/french horn/tuba. An STK BrassInstrument lip-valve waveguide (a bore + a resonant lip biquad that self-oscillates); HOLDS while sustained (like REED), bends/slurs/glisses live (the trombone slide). macros: harmonics = instrument/bore (bright tight trumpet → dark tuba), timbre = brassiness (round/mellow → loud/blatty shockwave — blow harder, get blattier), morph = breath/lip lean-in (soft steady → growling breath + deeper vibrato)
#define INSTR_SAMPLE  30  // PCM sample playback — plays a RECORDED buffer (record_arm/record_grab capture the console's own output) back at pitch on a keybed. Bind a buffer with instrument_sample(); press the root note for original speed, higher = faster/up (mic-and-sampling.md)
#define SAMPLE_NORMAL   0  // INSTR_SAMPLE playback mode: one-shot forward (instrument_sample_mode)
#define SAMPLE_REVERSE  1  // one-shot backward
#define SAMPLE_LOOP     2  // forward, wrapping to the chop start — sustains while the note is held
#define SAMPLE_PINGPONG 3  // bounce forward/backward between the chop ends while held

void sfx(int n);                              // play sfx slot n; -1 stops all sfx
void note(int midi, int instr, int vol);                  // one-shot note (250ms). vol 0..7. `instr` is an instrument slot (0..4 are the waves above; define 5..47 yourself)
void hit(int midi, int instr, int vol, int dur_ms);       // note with custom duration — closed hihat ~30ms, open ~200ms

// held notes — start a sustained voice and drive it live; the opposite of fire-and-forget note()
int  note_on(int midi, int instr, int vol);               // start a sustained note → returns a handle. vol 0 = start silent. note_off it later
void note_off(int handle);                                // release a held note (lets its envelope fade out)
void note_pitch(int handle, float midi);                  // slide a held note to a new pitch (float → between notes); no retrigger
void note_vol(int handle, float vol);                     // change a held note's volume 0..7 live (fractions OK — smooth fades/dynamics); 0 = silent but still alive
void note_cutoff(int handle, int hz);                     // sweep a held note's filter cutoff live (needs a filter on its instrument slot)
void note_res(int handle, float resonance);               // sweep a held note's filter resonance 0..15 live, fractions OK (pairs with note_cutoff for the acid squelch)
void note_lfo(int handle, int which, int dest, float rate_hz, float depth);  // retune a held note's LFO `which` (0..2) live — dest LFO_PITCH/DUTY/VOLUME/CUTOFF; depth 0 = off
void note_env(int handle, int which, int dest, int attack_ms, int decay_ms, float amount);  // set a held note's mod-envelope `which` (0..2) live — same shape as instrument_env(); amount 0 = off
void note_filter(int handle, int mode);                   // switch a held note's filter mode live (FILTER_OFF/LOW/HIGH/BAND/NOTCH)
void note_glide(int handle, int ms);                      // portamento: make note_pitch slide over `ms` instead of snapping (0 = snap)
void note_duty(int handle, float duty);                   // change a held note's pulse width 0.0..1.0 live (pulse/square slots only)
void note_pan(int handle, float pan);                     // change a held note's stereo position live -1 L..0 center..+1 R (slewed). pair with LFO_PAN for auto-pan
void note_off_all(void);                                  // release every held note at once (panic / cleanup)

// MIDI keyboard input — plug in a USB MIDI keyboard and play any sound cart (macOS now; Chrome on web later)
int  midi_get(int *note, int *vel);                       // drain one event this frame: returns +1 note-on, -1 note-off, 0 none; fills *note (0..127) + *vel (1..127). call in a while loop
bool midi_held(int note);                                 // is this MIDI note currently held down? (poll style, like btn())
int  midi_bend(void);                                     // last pitch-bend wheel value, -8192..+8191 (0 = centred) — for ribbons/glides
bool midi_present(void);                                  // is any MIDI keyboard connected? (false = no device / web Safari / not macOS)
const char *midi_name(void);                              // name of the connected MIDI keyboard (e.g. "Arturia KeyStep"), or "" if none — for a "connected to …" readout

// instruments — give a slot a waveform + ADSR envelope, then play it like any wave: note(midi, slot, vol)
void instrument(int slot, int wave, int attack_ms, int decay_ms, int sustain, int release_ms); // define slot 5..47: ms timings, sustain 0..7. pluck = fast attack+short release; pad = slow attack+long release
void wave_set(int which, const float *samples, int n);    // fill custom wave INSTR_USER0+which (which 0..3) with one drawn cycle: n samples in -1..1, resampled to 64. Live — a ringing note morphs as you redraw
void instrument_duty(int slot, float duty);               // pulse width 0.0..1.0 for a square-wave slot (0.5 = square, 0.12 = thin/nasal). no effect on other waves
void instrument_pan(int slot, float pan);                 // stereo position for a slot: -1 left .. 0 center (default) .. +1 right. voices inherit at note-on. sweep live with note_pan or LFO_PAN
void instrument_level(int slot, float gain);              // per-slot output LEVEL 0..1 — 1 = unity (default, byte-identical), 0.5 ≈ half, 0 = silent. balance a multi-part mix; ride it live like drive/echo. the level leg of the per-slot mixer (drive/echo/reverb/pan)
void record_arm(void);                                    // PCM SAMPLER: begin the always-on rolling capture of the master output (idempotent; off + byte-identical until called). Call once, then record_grab() any time to snapshot recent audio
int  record_grab(int sample_slot, float seconds);         // snapshot the last `seconds` of captured audio into PCM sample slot 0..7; peak-normalized + leading/trailing SILENCE TRIMMED (starts at the first audible sample). Returns samples grabbed after trim (0 = not armed / nothing yet). Pair with instrument_sample() + INSTR_SAMPLE
void instrument_sample(int slot, int sample_slot, int root_midi); // bind an INSTR_SAMPLE instrument slot to a recorded buffer; root_midi = the note that plays it at original speed (e.g. 60 = C4). Higher notes play faster/up in pitch
void instrument_sample_region(int slot, float start, float end); // set the CHOP: play the bound buffer only from start..end (fractions 0..1; default 0,1 = whole). Move the marks live to carve a single slice
void instrument_sample_mode(int slot, int mode);                 // INSTR_SAMPLE playback: SAMPLE_NORMAL / REVERSE / LOOP / PINGPONG (loop + pingpong sustain while the note is held)
int  sample_peaks(int slot, float *lo, float *hi, int n);       // waveform readout: downsample PCM slot 0..7 to n columns of min/max (-1..1) into lo[]/hi[] for drawing. Returns buffer length (0 = empty). Draw-time
int  sample_read(int slot, float *out, int max);               // read a sample slot's RAW PCM (mono, -1..1) into out[] (up to max floats); returns count (0 = empty). For SAVING a recorded buffer — the full-resolution twin of sample_peaks
void sample_load(int slot, const float *data, int n);          // load RAW PCM (mono, -1..1, n samples) INTO a sample slot — the inverse of sample_read, so a saved buffer restores without re-recording. No normalize/trim. Then instrument_sample() plays it
int  record_peaks(float seconds, float *lo, float *hi, int n);  // LIVE waveform of the capture ring while recording (before a grab): downsample the last `seconds` captured into n min/max columns. Returns samples covered. Draw the take "filling in" as a waveform
float instrument_playhead(int slot);                            // current playback position 0..1 of the INSTR_SAMPLE voice on `slot` (-1 if none playing) — draw a live playhead over the waveform

// microphone INPUT — let a cart HEAR (Tier-1: the mic as a controller, not a recorder). Dormant until
// mic_start(): nothing captures and no permission prompt fires until a cart asks. See docs/design/mic-and-sampling.md
void mic_start(void);                                     // ask the host to open the microphone (pops the OS permission prompt the first time). Call it once, e.g. on a button press — do not spam it
void mic_stop(void);                                      // release the microphone — the host closes its capture device (mic_level/mic_pitch go quiet)
int  mic_active(void);                                    // 1 once capture is live AND permission was granted, else 0 — poll it after mic_start() to know the mic is really open
float mic_level(void);                                    // current input loudness 0..1 (RMS) — the beatbox / envelope-follower input. 0 until mic_active(). Solid + responsive
float mic_pitch(void);                                    // pitch of the input in Hz (0 = no clear pitch) — the hum-to-theremin input. YIN detector: tracks a hummed voice cleanly + octave-safe; reports 0 rather than guess when unvoiced. ~21 readings/sec (a melody/controller axis, not a sample-accurate tuner)
// mic RECORD — capture a few seconds of mic input, then read it out into a sample slot (sample_load) to
// chop/play it (capture-then-freeze). Needs mic_start() + permission first; read only after it finishes.
void  mic_record(float seconds);                          // arm a capture of the mic input (up to 8s). Resets any prior take
int   mic_recording(void);                                // 1 while still capturing, 0 when the take is done (or none armed)
float mic_record_progress(void);                          // capture fill 0..1 — for a REC progress bar
int   mic_record_rate(void);                              // sample rate of the captured audio (pass to sample_load's caller; = engine rate on desktop)
int   mic_record_read(float *out, int max);               // copy the captured PCM (mono, -1..1) into out[] (up to max); returns count. Then sample_load() it

// pan law — how a pan position maps to L/R gain (master-wide; set once in init(), affects every panned sound)
#define PAN_LINEAR  0   // default — center keeps full gain (L=R=mix); byte-identical to mono. a centered sound is +3dB vs hard-panned
#define PAN_POWER   1   // constant-power — center is -3dB; equal perceived loudness right across the sweep (smoother, more pronounced motion)
void pan_law(int law);                                    // pick the stereo pan law: PAN_LINEAR (default) or PAN_POWER. only matters when sounds actually pan

// spatial audio — place sounds in the world; pan, distance-volume & Doppler derive automatically.
// Dormant until you call listener(): a cart that never positions a sound is byte-identical.
void listener(float x, float y);                          // where the player's ears are (world units); sources position relative to this
void listener_vel(float vx, float vy);                    // listener velocity (units/sec) for Doppler; 0 = still (default)
void spatial_model(float ref_dist, float max_dist, float rolloff);  // distance falloff: full volume within ref_dist, silent past max_dist; rolloff = steepness (1 = natural)
void spatial_speed(float c);                              // speed of sound (world units/sec) — Doppler strength; 0 = Doppler off (default ~340)
void note_pos(int handle, float x, float y);              // place a held note (note_on handle) in the world → auto pan + distance-volume, slewed
void note_motion(int handle, float vx, float vy);         // held note's velocity (units/sec) → Doppler pitch as it moves past the listener
void hit_at(int midi, int instr, int vol, int dur_ms, float x, float y);  // one-shot note positioned in the world (pan+volume+Doppler snapshot at trigger) — no handle needed
// emitter buses (v2): position an instrument's WHOLE effected output (dry + its FX tail — shimmer/reverb)
// as one object, so the tail moves WITH it. note_pos positions dry voices only; this positions the finished bus.
void instrument_pos(int slot, float x, float y);          // place instrument `slot`'s effected bus in the world (auto pan + distance)
void instrument_motion(int slot, float vx, float vy);     // that bus's velocity (units/sec) → Doppler on the whole effected source

// one routable LFO per instrument — a slow sine that wobbles one parameter
#define LFO_PITCH   0   // vibrato — depth in semitones (0.3 subtle, 2 wide)
#define LFO_DUTY    1   // PWM / duty sweep — depth 0.0..0.5 (square-wave slots only)
#define LFO_VOLUME  2   // tremolo — depth 0.0..1.0
#define LFO_CUTOFF  3   // filter sweep / wah — depth in Hz (needs a filter on the slot)
#define LFO_HARMONICS 4 // sweep the harmonics macro (engine voices, INSTR_PLUCK+). SNAPPED — STEPS through detents (wavetype/ratio/instrument). depth 0..1
#define LFO_TIMBRE  5   // sweep the timbre macro (brightness; on PD-reso = a resonant filter sweep with no filter). depth 0..1
#define LFO_MORPH   6   // sweep the morph macro (the engine's 3rd axis — PD DCW depth, FM feedback, organ chorus, EP bark). depth 0..1
#define LFO_PAN     7   // auto-pan — sweep the stereo position. depth 0..1 (1 = full L↔R). the declarative path for tremolo-pan / rotary-style motion
#define LFO_DETUNE  8   // sweep the unison spread — the breathing/chorusing width wobble. depth in semitones. needs instrument_unison on
void instrument_lfo(int slot, int which, int dest, float rate_hz, float depth);  // attach LFO `which` (0..2 — a slot has 3) to a slot. dest: LFO_PITCH/DUTY/VOLUME/CUTOFF or the macro dests LFO_HARMONICS/TIMBRE/MORPH (engine voices). rate 4–8 Hz typical. depth 0 = off. Shape defaults SINE — set via lfo_shape()
void lfo_shape(int slot, int which, int shape);   // set the WAVEFORM of a slot's LFO `which` (0..2) to an LFO_SHAPE_* (default SINE). Call alongside instrument_lfo(); persists across instrument_lfo() retunes. SH on LFO_PITCH = a random-step arp; SQUARE on LFO_CUTOFF = a stepped filter
void note_lfo_shape(int handle, int which, int shape);   // same, for a live held note (note_on handle) — set its LFO `which` waveform to LFO_SHAPE_*
float lfo_value(int shape, float phase);   // evaluate an LFO_SHAPE_* at phase 0..1 → -1..1 (the engine's own dispatcher). For STATELESS shapes (SINE..OPTICAL); S&H/RANDOM need per-instance state so they read as SINE here. Use it to drive your own CV / draw a waveform without re-rolling the math
void scope_read(float *dst, int n);        // OSCILLOSCOPE: copy the latest n mono output samples (newest last, -1..1) into dst — the actual post-FX mix, for drawing a waveform. n up to 2048; dst must hold n floats. Find a rising zero-crossing yourself to "freeze" the trace. Zero cost until first called.
void scope_read2(float *l, float *r, int n);   // STEREO scope: the latest n LEFT and RIGHT output samples (newest last, same-index pairs) — the vectorscope/goniometer feed; a mono mix plots a line, autopan/chorus open it up. n up to 2048; each buffer holds n floats. Zero cost until first called.

// resonant filter per instrument — sculpts the tone (the SID-style knob)
#define FILTER_OFF   0
#define FILTER_LOW   1   // lowpass — keep lows, muffle highs (warm)
#define FILTER_HIGH  2   // highpass — keep highs, thin out lows (tinny)
#define FILTER_BAND  3   // bandpass — keep only a band around cutoff (vowel/wah)
#define FILTER_NOTCH 4   // notch — scoop OUT a band around cutoff (phasey)
#define FILTER_LADDER 5  // the Moog 4-pole transistor-ladder lowpass — steeper (24dB/oct) & creamier than FILTER_LOW, loses bass as resonance climbs, self-oscillates near the top
#define FILTER_STEINER 6 // Steiner-Parker-style nonlinear 2-pole LOWPASS (the Behringer Neutron's voice) — rawer & more aggressive than FILTER_LOW, with a dirty, screaming high-resonance bite
#define FILTER_STEINER_HP 7 // Steiner-Parker highpass — the aggressive filter's HP response
#define FILTER_STEINER_BP 8 // Steiner-Parker bandpass — the aggressive filter's BP response
#define FILTER_STEINER_NF 9 // Steiner-Parker notch — the aggressive filter's notch response
#define FILTER_DIODE 10  // the TB-303 diode-ladder lowpass — THE acid filter: ~18dB/oct (between FILTER_LOW's 12 and the Moog ladder's 24), drains bass as resonance climbs, and the resonance saturates INSIDE the loop (the diodes) so it growls instead of ringing clean. Self-oscillates at the top. Lowpass-only
void instrument_filter(int slot, int mode, int cutoff_hz, int resonance);  // mode FILTER_*, cutoff in Hz (e.g. 800), resonance 0..15 (high = whistly peak). sweep cutoff with LFO_CUTOFF

// modulation envelopes per instrument — a one-shot AD contour (the envelope twin of the LFO).
// fires once per note: ramps up over attack_ms, then decays back over decay_ms. amount is
// bipolar and its units depend on dest. 3 per slot (which 0..2), so a filter sweep + a pitch
// blip + a macro contour can run together. there is NO ENV_VOLUME — that's the amp envelope (instrument()).
#define ENV_CUTOFF  0   // sweep filter cutoff — the pluck "pew"/"dwow". amount in Hz (+ opens then closes). needs a filter on the slot
#define ENV_PITCH   1   // pitch blip — drum punch / attack snap / zap. amount in semitones (+ starts sharp, settles to the note)
#define ENV_DUTY    2   // pulse-width sweep (square/pulse slots only). amount 0.0..1.0
#define ENV_HARMONICS 3 // one-shot sweep of the harmonics macro (engine voices; SNAPPED — steps detents). amount 0..1
#define ENV_TIMBRE  4   // one-shot sweep of the timbre macro (a per-note brightness contour). amount 0..1
#define ENV_MORPH   5   // one-shot sweep of the morph macro (e.g. a per-note PD DCW shape on top of the engine's own). amount 0..1
#define ENV_DETUNE  6   // one-shot sweep of the unison spread — THE bloom: one thin saw opening into a wall of N on the attack. amount in semitones. needs instrument_unison on
void instrument_env(int slot, int which, int dest, int attack_ms, int decay_ms, float amount);  // attach mod-envelope `which` (0..2) to a slot. dest ENV_CUTOFF/PITCH/DUTY or the macro dests ENV_HARMONICS/TIMBRE/MORPH. pluck: ENV_CUTOFF amount 1500, attack 0, decay 120. amount 0 = off
void instrument_follow(int slot, int dest, int attack_ms, int release_ms, float amount);  // envelope FOLLOWER: tracks the slot's own amplitude (fast attack, slow release) → dest LFO_CUTOFF/VOLUME/PITCH. The touch-responsive auto-wah (FILTER_BAND + this) — play harder, it opens more. amount = Hz (cutoff); 0 = off
void note_follow(int handle, int dest, int attack_ms, int release_ms, float amount);  // set a held note's envelope follower live — same shape as instrument_follow(); amount 0 = off

// engine macros — three 0..1 knobs that EVERY modeled engine answers; what each knob sweeps
// is per-engine (documented on the INSTR_PLUCK/MALLET/FM lines above), but the API never
// grows when a new engine lands. Defaults: 0.5.
void instrument_harmonics(int slot, float x);  // engine macro 1 of 3 — e.g. PLUCK ring time · MALLET bar material · FM carrier:mod ratio · GUITAR body · PIANO voicing · BRASS instrument/bore (trumpet→tuba)
void instrument_timbre(int slot, float x);     // engine macro 2 of 3 — e.g. PLUCK pick brightness · MALLET mallet hardness · FM brightness · GUITAR string brightness · PIANO hammer · BRASS brassiness
void instrument_morph(int slot, float x);      // engine macro 3 of 3 — e.g. PLUCK pick position · MALLET ring length · FM feedback growl · GUITAR mute · PIANO pedal · BRASS breath/lip lean-in
void instrument_choke(int slot_a, int slot_b); // declare that a new note on slot_a instantly kills any sounding voice on slot_b (open/closed hat choke)
void note_harmonics(int handle, float x);      // live macro on a held note, slewed — ring/partial/ratio changes reach a sounding voice (PLUCK ring, MALLET partials, FM ratio, BRASS instrument/bore)
void note_timbre(int handle, float x);         // live macro on a held note, slewed — strike-shaping macros (PLUCK/MALLET timbre) apply at the next note; FM brightness + BRASS brassiness move live
void note_morph(int handle, float x);          // live macro on a held note, slewed — MALLET ring/motor, FM feedback + BRASS breath move live; PLUCK position applies at the next note
void instrument_mode(int slot, int idx, float value);  // per-engine aux channel, NOTE-ON face — a slot's structure/mode params past the 3 macros, read at the next note_on. idx is per-engine; use the MODE_* names. GUITAR/PIANO: MODE_STRING_WEIGHT/MODE_STRING_CLICK. BOWED: MODE_BOW_PIZZ (>=0.5 plucks instead of bows). value 0..1. (decision 0017)
#define MODE_STRING_WEIGHT 0   // instrument_mode idx — INSTR_GUITAR/PIANO: fundamental-reinforcement weight (0 = pure string .. 1 = body-thick)
#define MODE_STRING_CLICK  1   // instrument_mode idx — INSTR_GUITAR/PIANO: attack click / pick noise amount
#define MODE_BOW_PIZZ      0   // instrument_mode idx — INSTR_BOWED: >= 0.5 = PIZZICATO (pluck the same string), < 0.5 = arco (bow, self-oscillating hold)
void voice_nasal(int handle, float amount);    // INSTR_VOICE nasal color on a held note: 0 = open vowel .. 1 = hummed/nasal (the honk, the chant). The voice's 4th axis, alongside the harmonics/timbre/morph macros (vowel/size/effort)
void voice_consonant(int handle, int id);      // begin a held INSTR_VOICE note with a consonant onset that morphs into the vowel ("bah"/"mah"/"sss-ah"). Call right after note_on; id 0..21 (b d g m n l s sh ng r w y dh f v z zh th p t k ch), -1 = none. A timed onset
void voice_coda(int handle, int id);           // close a held INSTR_VOICE note ON a consonant: the vowel morphs into it ("ahh-m"/"oo-d"). Call right before note_off; id 0..21, -1 = none. Mirror of voice_consonant; voiced ids (b d g m n l ng r w y) stay sung
void note_aux(int handle, int idx, float value); // per-engine aux channel, LIVE face — raw per-engine param poke by index on a held note. idx is per-engine; INSTR_VOICE uses the probe carts' VP_* map (voxlab/voxab/voxpad/say). Kept off the beginner surface by design (the advertised VOICE controls are the 3 macros + voice_nasal()). (decision 0017)

// drive — saturation AFTER the filter, so resonance screams into it. The grit knob.
// the MODE picks the waveshaper's flavour; instrument_drive() is still the amount (0 = clean bypass).
#define DRIVE_SOFT    0   // tanh soft-clip — warm, tube-like overdrive (the default; same as before modes existed)
#define DRIVE_HARD    1   // hard clip — buzzy, square-edged digital fuzz (the harshest)
#define DRIVE_FOLD    2   // sine wavefolder — metallic, glassy, ring-mod-ish as you push it
#define DRIVE_ASYM    3   // asymmetric tube — adds EVEN harmonics (the round, fat, single-ended-amp grit)
void instrument_tune(int slot, float semitones); // detune a slot ±24 semitones (fractions are the point: 0.06 = unison shimmer, ±1 = a tuning trimmer). LIVE — every sounding voice on the slot bends, scheduled arp/seq hits included. 0 = off (default)
void instrument_unison(int slot, int voices, float detune);  // UNISON: sum `voices` (1..7) detuned copies of the slot's wave = the supersaw wall. detune = spread in semitones (0.1 shimmer .. 0.7 wide). voices applies at the next note; detune rides LIVE. wavetable slots only (saw/square/tri/sine). 1 = off (default)
void instrument_unison_detune(int slot, float detune);  // ride the unison spread alone, LIVE — 0 = thin single osc, wider = the wall blooms open. the detune-BLOOM gesture. sweep it, or drive it with LFO_DETUNE / ENV_DETUNE
void instrument_drive(int slot, float x);      // overdrive a slot 0.0..1.0 — 0 = clean (default), 0.3 = warm, 1 = fuzz. loudness stays put; character changes
void instrument_drive_mode(int slot, int mode); // pick the waveshaper: DRIVE_SOFT (default) / DRIVE_HARD / DRIVE_FOLD / DRIVE_ASYM. amount stays instrument_drive()
void note_drive(int handle, float x);          // sweep a held note's drive live, slewed — ride it up mid-phrase for the acid scream
void instrument_sync(int slot, float ratio);   // oscillator HARD SYNC on a wavetable slot: a slave osc runs at ratio×pitch, reset every cycle. 0 = off (default), 1 = unison, 1.5–4 = the bright tearing sweep. Sweep it for the classic sync lead
void note_sync(int handle, float ratio);       // sweep a held note's hard-sync ratio live, slewed — the screaming sync sweep under your fingers
void note_drive_mode(int handle, int mode);    // switch a held note's drive waveshaper live (DRIVE_*) — not slewed, snaps
void drive_insert(float amount, int mode, float mix);  // MIX-BUS SATURATION: drive the whole summed mix as a reorderable INSERT (put FX_DRIVE in fx_order(0,…)). The bus sibling of instrument_drive — tube/tape glue low, a lo-fi wall cranked, grit on drums too. amount 0..1, mode DRIVE_*, mix 0..1 (0 = bypass → byte-identical)
void drive_insert_inst(int instance, float amount, int mode, float mix);  // a 2nd mix-bus drive INSTANCE (0..1) — pair with FX_INST(FX_DRIVE, instance) for two bus drives in one chain (e.g. an overdrive pedal INTO the amp's drive)

// echo — THE shared echo bus (there is exactly one): each slot chooses how much to send
// into it. Repeats get darker every pass (tone), and feedback past 1.0 self-oscillates
// like a tape echo instead of exploding. Sweeping the time live pitch-bends the tail.
void echo(int time_ms, float feedback, float tone);  // configure the bus: delay 1..2000ms, feedback 0..1.1 (>1 = runaway), tone 0..1 (dark..bright repeats)
void instrument_echo(int slot, float send);    // how much this slot feeds the bus 0.0..1.0 — 0 = dry (default), 0.15 = slapback, 0.8 = dub throw
void note_echo(int handle, float x);           // sweep a held note's echo send live, slewed — throw a single phrase into the tail
void echo_insert(int time_ms, float feedback, float tone, float mix);  // echo as a dry/wet INSERT on the master bus — a REAL reorderable DELAY pedal (put FX_ECHO in fx_order(0,…) to place it). Unlike echo() (a send), its chain position is audible (delay→drive vs drive→delay). time 1..2000ms, feedback 0..1.1, tone 0..1, mix 0..1 (0 = bypass)

// grains — granular delay: captures the live signal then replays scattered overlapping windowed
// grains of the recent past into an evolving cloud. FREEZE loops what's captured forever (hold a
// chord → infinite shimmer). The texture pedal — feed it a rich source (pads/chords) to shine.
// Reorderable insert (FX_GRAINS); a small pool means master + one instrument bus can run it at once.
void grains(float grain_ms, float density, float position, float scatter, float feedback, float mix);  // master granular cloud. grain_ms 5..1000 (size), density 1..100 grains/sec, position 0..1 (0=deep past, 1=live edge), scatter 0..1 (random spread), feedback 0..0.95 (self-feeding cloud), mix 0..1 (0=off). defaults try 120/12/0.8/0.3/0.2/0.5
void instrument_grains(int slot, float grain_ms, float density, float position, float scatter, float feedback, float mix);  // granular cloud on just one instrument (its own bus) — grain the pads while the drums stay dry. Same args + a slot
void grains_freeze(int on);             // freeze the master granular buffer: stop capturing, loop what's there (1 = frozen, 0 = live). The performance toggle — hold a chord, freeze, play over the cloud
void instrument_grains_freeze(int slot, int on);  // freeze one instrument's granular buffer (1/0)
void grains_pitch(float semitones, float spread, int reverse);  // transpose the master grain cloud: semitones -24..24 (12 = octave up), spread 0..1 (random per-grain detune — a shimmer/chord cloud), reverse 1 = grains play backwards. Call grains() first. Sweep live for a falling/rising cloud
void instrument_grains_pitch(int slot, float semitones, float spread, int reverse);  // transpose one instrument's grain cloud (same args + a slot)

// reverb — THE master reverb send: each slot chooses how much to send into it. A real room/hall
// (a chord blooms into space), not repeating taps like echo. instrument_reverb() alone already
// sounds right (defaults size 0.5). reverb()/instrument_reverb() use TANK 0 (the master send).
void reverb(float size, float damping);        // configure the master send (tank 0): size 0..1 (small room → endless hall), damping 0..1 (bright → dark tail)
void instrument_reverb(int slot, float send);  // how much this slot feeds the master send 0.0..1.0 — 0 = dry (default), 0.3 = roomy, 0.8 = drenched
void note_reverb(int handle, float x);         // sweep a held note's reverb send live, slewed — bloom one phrase into the hall

// reverb SEND-BUSES (tanks 1..2) — independent reverb spaces you can route per instrument AND put
// effects after. reverb_bus() makes tank N a real space on its own aux bus (chain starts FX_REVERB);
// fx_order(that bus, {FX_REVERB, FX_CRUSH, …}) then runs pedals on the wet tail. Drums in a tight
// room + keys in a long plate at once = two tanks. (Tank 0 is reverb()'s master send — use 1..2 here.)
void reverb_bus(int tank, float size, float damp);          // configure reverb tank 1..2 as a send-bus: size 0..1, damp 0..1 (claims an aux bus on first call)
void instrument_reverb_bus(int slot, int tank, float mix);  // route this slot's reverb send into tank 1..2 instead of the master send; mix 0..1
void reverb_bus_fx(int tank, int fx, float a, float b, float c);  // add an effect AFTER the reverb on tank 1..2 — fx = FX_CRUSH/FX_EQ/FX_TAPE/FX_CHORUS; a/b/c = that effect's params (crush:bits,rate,mix · eq:lo,mid,hi dB · tape:wow,flut,sat · chorus:rate,depth,mix). crush/chorus mix 0 = off
void reverb_insert(float size, float damp, float mix);            // reverb as a dry/wet INSERT on the master bus — a REAL reorderable pedal (put FX_REVERB in fx_order(0,…) to place it). size/damp 0..1, mix 0..1 (0 = bypass). Unlike reverb() (a send), its chain position is audible

// chorus — THE master chorus (there is exactly one), applied to the whole mix: a BBD/Juno-style
// modulated delay that thickens + widens everything into a lush shimmer. Master-wide (not per-slot).
void chorus(float rate, float depth, float mix);  // rate 0.1..5 Hz (wobble speed), depth 0..1 (sweep), mix 0..1 (dry..wet). 0 mix = off. defaults 1.5/0.4/0.5

// flanger — THE master flanger (one, whole mix): a short swept delay with feedback. The jet-plane
// whoosh / metallic comb sweep. Needs a rich source (chords, noise) to hear. Master-wide.
void flanger(float rate, float depth, float feedback, float mix);  // rate 0.05..5 Hz, depth 0..1, feedback -0.95..0.95 (more = jet/metallic; <0 = through-zero), mix 0..1 (0 = off). defaults 0.3/0.7/0.7/0.5

// per-instrument chorus/flanger — put the effect on ONE instrument (flange the guitar, not the
// rhythm), unlike the master chorus()/flanger() which hit the whole mix. Same args + a slot. Up to
// 7 instruments can have their own at once. (echo/reverb are already per-instrument: see above.)
void instrument_chorus(int slot, float rate, float depth, float mix);            // chorus on just this slot
void instrument_flanger(int slot, float rate, float depth, float feedback, float mix);  // flanger on just this slot

// tape — vintage analog warmth over the whole mix: wow (slow pitch drift) + flutter (fast warble)
// + saturation (warm soft-clip + HF rolloff). Master, or per-instrument (lo-fi just the drums).
void tape(float wow, float flutter, float saturation);                  // wow/flutter/sat 0..1 (0,0,0 = off). defaults 0.3/0.2/0.4
void tape_inst(int instance, float wow, float flutter, float saturation); // tape on a 2nd master INSTANCE (0..1) — pair with FX_INST(FX_TAPE, instance)
void instrument_tape(int slot, float wow, float flutter, float saturation);  // tape on just this slot

// auto-wah — a resonant bandpass that OPENS with how hard you play (an envelope follower on the
// summed signal): the funky talking-clavinet quack. Best on ONE rich/percussive instrument.
void wah(float sensitivity, float resonance, float mix);                // sensitivity 0..1 (how much dynamics open it), resonance 0..1 (the quack), mix 0..1 (0 = off). defaults 0.5/0.5/0.7
void instrument_wah(int slot, float sensitivity, float resonance, float mix);  // auto-wah on just this slot
void wah_lfo(float rate_hz, float resonance, float mix);                // LFO auto-wah — a sine rocks the SAME bus bandpass (rhythmic "wah-wah", not dynamics). rate 0.5..10 Hz, resonance 0..1, mix 0..1 (0 = off)
void instrument_wah_lfo(int slot, float rate_hz, float resonance, float mix);  // LFO auto-wah on just this slot

// bitcrush — lo-fi quantization: drop the bit depth + the sample rate for crunchy 8-bit/retro
// grit. Master (whole mix), or per-instrument (crush just the drums, the rest hi-fi). bits 1..16
// = bit depth (low = gnarlier steps), rate = sample-hold downsampling 1..64x (high = more
// aliasing whine), mix 0..1 (0 = off). Defaults 8 / 4 / 1.0.
void crush(float bits, float rate, float mix);                          // THE master bitcrush, on the whole mix
void crush_inst(int instance, float bits, float rate, float mix);       // bitcrush on a 2nd master INSTANCE (0..1) — pair with FX_INST(FX_CRUSH, instance) in fx_order() for two crushers in one chain
void instrument_crush(int slot, float bits, float rate, float mix);     // bitcrush on just this slot (auto-grabs a private FX bus)

// EQ — 3-band tone: BOOST or cut LOW (<~80 Hz) / MID (~80 Hz–6 kHz) / HIGH (>~6 kHz). The ONLY
// effect that can boost a band (the filters only cut). Master (whole mix), or per-instrument. Pair
// with DRIVE_ASYM (EQ around a clipper) for a guitar-amp tone. Gains in dB, ±12; 0/0/0 = flat (off).
void eq(float low_gain, float mid_gain, float high_gain);               // THE master 3-band EQ, on the whole mix
void instrument_eq(int slot, float low_gain, float mid_gain, float high_gain);  // EQ on just this slot (auto-grabs a private FX bus)
void eq_inst(int instance, float low_gain, float mid_gain, float high_gain);    // master EQ on a 2nd INSTANCE (0..1) — pair with FX_INST(FX_EQ, instance) in fx_order() for two EQs in one chain

// formant — a VOWEL filter: 4 bandpasses at the human formant frequencies make any sound take on
// an "ooh/aah/eee" vocal colour (the talkbox/vowel-filter; a wah is the one-peak version). vowel
// 0..1 sweeps U→O→A→E→I; q 0..1 narrows the peaks (broad → nasal/pronounced); mix 0..1 (0 = off).
void formant(float vowel, float q, float mix);                          // THE master formant/vowel filter, on the whole mix
void instrument_formant(int slot, float vowel, float q, float mix);     // formant filter on just this slot (auto-grabs a private FX bus)

// tremolo — a volume LFO that ducks the level up and down: the Fender/Wurlitzer amp wobble (the
// "electric piano" throb). One shared phase per bus, so a per-instrument tremolo wobbles that
// instrument's whole output in unison — the coherent amp wobble a per-voice LFO_VOLUME can't give.
// Master (whole mix), or per-instrument. Only attenuates (never boosts), like a real amp.
// LFO_SHAPE_* — the ONE modulator-waveform vocabulary, shared by tremolo/autopan (shape arg),
// fx_lfo (shape arg), and the voice LFOs via lfo_shape()/note_lfo_shape(). SINE/SQUARE/TRI keep the
// old TREM_* values (0/1/2) so existing carts are unaffected; TREM_* are now aliases.
#define LFO_SHAPE_SINE    0   // smooth sine — the default everywhere
#define LFO_SHAPE_SQUARE  1   // hard on/off — chop / stutter; on cutoff = a stepped 2-state filter
#define LFO_SHAPE_TRI     2   // linear up/down ramp — sharper than sine
#define LFO_SHAPE_SAW     3   // ramp DOWN then snap up (1→-1) — sawtooth
#define LFO_SHAPE_RAMP    4   // ramp UP then snap down (-1→1) — reverse saw
#define LFO_SHAPE_OPTICAL 5   // asymmetric bulb throb (slow brighten, fast dim) — the Univibe glow
#define LFO_SHAPE_SH      6   // sample & hold — random value, stepped, held for one cycle (random-step arp on pitch)
#define LFO_SHAPE_RANDOM  7   // smooth filtered random walk — drifting/"human" wander
#define TREM_SINE    LFO_SHAPE_SINE     // back-compat aliases — tremolo()/autopan() take any LFO_SHAPE_* now
#define TREM_SQUARE  LFO_SHAPE_SQUARE
#define TREM_TRI     LFO_SHAPE_TRI
void tremolo(float rate, float depth, int shape);                       // rate 0.1..20 Hz, depth 0..1 (0 = off), shape LFO_SHAPE_* (TREM_* still valid). THE master tremolo
void instrument_tremolo(int slot, float rate, float depth, int shape);  // tremolo on just this slot (auto-grabs a private FX bus)

// auto-pan — the tremolo LFO applied ANTIPHASE to L/R: a stereo sweep that moves the sound side to
// side (the Rhodes suitcase vibrato; rotary-style motion). It's its OWN insert, so it stacks with
// tremolo on the same bus — a fast amplitude throb AND a slow stereo drift at once. Reuses the TREM_*
// shapes: SINE = smooth glide, SQUARE = hard L/R ping-pong, TRI = linear sweep. Master, or per-slot.
void autopan(float rate, float depth, int shape);                       // rate 0.1..20 Hz, depth 0..1 (0 = off, 1 = full L↔R), shape TREM_*. THE master auto-pan
void instrument_autopan(int slot, float rate, float depth, int shape);  // auto-pan on just this slot (auto-grabs a private FX bus)

// ring modulator — multiply the signal by a sine CARRIER at freq_hz, creating inharmonic sum/
// difference sidebands: the metallic, clangorous, robotic/bell texture (the Dalek voice, the sci-fi
// clang). Unlike tremolo (a unipolar gain LFO that only wobbles volume), the carrier is BIPOLAR, so
// it adds NEW frequencies. Low freq (~2..30 Hz) = a throbby AM; high (~100..2000 Hz) = the atonal clang.
void ringmod(float freq_hz, float mix);                                 // freq 1..8000 Hz, mix 0..1 (0 = off). THE master ring modulator
void instrument_ringmod(int slot, float freq_hz, float mix);            // ring mod on just this slot (auto-grabs a private FX bus)

// phaser — a chain of allpass filters swept by an LFO carves moving NOTCHES in the spectrum: the
// 70s electric-piano / Small Stone swirl (vocal, hollow, "jet-like" but softer than a flanger's
// metallic comb). stages = how many notches (4 = the classic Phase-90; more = thicker/deeper).
// feedback adds resonance around the notches. Master (whole mix), or per-instrument. mix 0 = off.
void phaser(float rate, float depth, float feedback, float mix, int stages);                       // rate 0..10 Hz, depth 0..1, feedback -0.95..0.95, mix 0..1 (0 = off), stages 2..8. THE master phaser
void instrument_phaser(int slot, float rate, float depth, float feedback, float mix, int stages);  // phaser on just this slot (auto-grabs a private FX bus)

// univibe — the 60s photocell vibe: the phaser's allpass chain swept by an OPTICAL LFO (slow-bright,
// fast-dim "bulb throb") instead of a sine — liquid + asymmetric where the phaser is even. Classic
// 4-stage, no feedback. Shares the phaser insert (don't run both on one bus). Lush on organ/EP/chords.
void univibe(float rate, float depth, float mix);                 // rate 0..10 Hz (throb speed), depth 0..1 (sweep), mix 0..1 (0 = off). THE master univibe. try 4/0.7/0.5
void instrument_univibe(int slot, float rate, float depth, float mix);  // univibe on just this slot (auto-grabs a private FX bus)

// dropout — the VHS / Generation-Loss "FAILURE" knob: random momentary tape-catches on the whole mix
// (the signal stumbles — drops in level AND goes dull — then recovers). A master-stage effect; pair
// with crush()/tape()/filter() for the full degraded-cassette sound. amount 0 = off → byte-identical.
void dropout(float amount, float depth);   // amount 0..1 = how often the tape catches, depth 0..1 = how hard it drops (level + HF loss). try 0.4/0.7

// shallow water — a filtered-random ("K-field") short delay + a Low Pass Gate (Fairfield Shallow
// Water): a gentle, UNPREDICTABLE pitch wobble (a random walk drifts the delay, not a sine LFO — the
// warped-tape / reflection-on-water shimmer) that goes dark + soft when quiet and blooms back. A
// reorderable insert (FX_SHALLOW). Lovely on pads, keys, clean guitar. mix 0 = off.
void shallow(float rate, float depth, float mix);                 // rate 0.2..8 Hz (drift speed), depth 0..1 (warble amount), mix 0..1. THE master shallow water. try 1/0.6/0.5
void instrument_shallow(int slot, float rate, float depth, float mix);  // shallow water on just this slot (auto-grabs a private FX bus)

// amp_noise — an OPTIONAL rig-noise floor: broadband hiss + 50/60 Hz mains hum, the "an electric
// guitar is never truly silent" character. Entirely opt-in — hiss 0 AND hum 0 = off (the console is
// pristine by default). A constant master-output floor (never ducked by the mix). Pair with a noise
// gate to clamp it between notes. For realism, drive `hiss` up with your amp's gain.
void amp_noise(float hiss, float hum, int mains_hz);   // hiss 0..1 (broadband floor), hum 0..1 (mains buzz), mains_hz 50 (EU) or 60 (US). 0,0 = silent. try 0.3/0.2/60

// shimmer — a reverb with an OCTAVE-UP pitch-shifter in its feedback loop: the tail keeps climbing
// into a glassy, ascending crystalline pad (the Strymon/Eno shimmer). Hold a chord and it blooms
// upward forever. Master-stage whole-mix space. mix 0 = off. Gorgeous on pads, organ, bowed strings.
void shimmer(float size, float damp, float shimmer_amt, float mix);   // size 0..1 (decay length), damp 0..1 (dark tail), shimmer_amt 0..1 (how much octave-up recirculates — the climb), mix 0..1 (0 = off). try 0.8/0.4/0.6/0.5
void instrument_shimmer(int slot, float size, float damp, float shimmer_amt, float mix);  // shimmer on just ONE instrument's bus (claims an aux tank) — shimmer the pad, leave the drums dry. master shimmer() = bus 0; up to 1 instrument at once. Same args + a slot

// varispeed — variable TAPE playback speed of the whole mix: 1.0 = normal (bypass), <1 = slower (pitch
// DOWN + time-stretch — the tape-slowdown dive / turntable brake), >1 = faster (chipmunk). SWEEP it for
// tape bends; the speed glides (tape inertia). Built for sweeps, not holding a fixed off-speed forever.
void varispeed(float speed);   // 0.25..4 (2 octaves down..up); 1.0 = bypass. Sweep down to ~0.3 for a tape-stop dive, back to 1 to spin up

// fx_mod / fx_lfo — the MODULATION layer over a curated set of sweep-safe effect params (FXMOD_* below).
// Configure the effect first (filter()/drive_insert()/grains()/shimmer()/tremolo()/autopan()); these RIDE
// one of its params on top, like LFO_TIMBRE rides an instrument macro. fx_mod() is the per-frame CV sink
// (drive it from your own LFO/envelope/sequencer — modrack); fx_lfo() is a fire-and-forget engine sine.
// Only cheap-to-sweep params are exposed — it is impossible to modulate a buffer effect into a stutter.
void fx_mod(int bus, int target, float value);                       // bus 0 = master, 1.. = an instrument's bus; target = FXMOD_*; value 0..1 (mapped per target). Call per frame; engine slews → no zipper
void instrument_fx_mod(int slot, int target, float value);           // same, addressed by instrument slot (resolves to its FX bus)
void fx_lfo(int bus, int target, float rate_hz, float depth, float center, int shape);  // engine LFO on a target: rate Hz, depth 0..1 (peak deviation; 0 = DETACH), center 0..1, shape LFO_SHAPE_*. e.g. fx_lfo(0,FXMOD_FILTER_CUT,0.3,0.4,0.5,LFO_SHAPE_SINE)
// gate — a NOISE GATE: clamps the signal shut when it falls below `threshold`, opens above it. The
// classic rig pedal (tame a noisy/driven part's hiss + tails between notes); place it AFTER reverb in
// fx_order for the iconic 80s GATED REVERB (the tail chops off). A reorderable insert (FX_GATE).
void gate(float threshold, int attack_ms, int release_ms);                  // threshold 0..1 (0 = always open/bypass), attack_ms (open, ~1-10), release_ms (close, ~30-300 = how fast the tail cuts). THE master gate
void instrument_gate(int slot, float threshold, int attack_ms, int release_ms);  // noise gate on just this slot (auto-grabs a private FX bus)

// filter — a sweepable resonant FILTER on the whole mix: the DJ-filter / build-up sweep. A plain
// state-variable filter (low/high/band/notch) you RIDE live — close it to a muffled thump on the
// breakdown, open it back up (crank resonance for the scream) on the build. THE electronic-music
// gesture. A reorderable insert (FX_FILTER); unlike the buffer effects it's cheap to sweep every frame.
void filter(int mode, float cutoff_hz, float resonance);   // mode FILTER_LOW/HIGH/BAND/NOTCH (FILTER_OFF = bypass), cutoff 20..~20000 Hz (ride it live), resonance 0..1 (the peak/scream). THE master filter
void filter_inst(int instance, int mode, float cutoff_hz, float resonance);  // filter on a 2nd master INSTANCE (0..1) — pair with FX_INST(FX_FILTER, instance) for filter A/B in one chain

// sidechain & bus compression — DYNAMICS on the SUMMED signal (not a per-voice insert): the
// "pumping" duck and the glue. sidechain() ducks a victim bus's level whenever a TRIGGER fires,
// keyed off a slot you route in with sidechain_key() — the kick is the classic trigger → the
// house/EDM pump (the pad/bass breathe against the beat). glue() is the same gain stage with NO
// trigger — a bus compressor reading its OWN level, so the whole mix moves as one lump.
void sidechain(int victim_bus, int key, float amount, int attack_ms, int release_ms);  // duck victim_bus (0 = master) by up to amount 0..1 on every hit in trigger `key` (0..3). amount 0 = off. attack ~1ms, release ~80–250ms = the pump
void sidechain_key(int slot, int key, float send);   // route a slot into trigger key 0..3 — its level drives any sidechain() keyed there (kick → key 0). send 0..1 (0 = not a trigger)
void glue(int victim_bus, float amount, int attack_ms, int release_ms);  // bus COMPRESSOR: duck victim_bus (0 = master) by up to amount 0..1 from its own level (no trigger) — the mix glued together. amount 0 = off

// insert-chain ORDER — the inserts above run in a fixed default order; fx_order() rearranges them
// per bus (e.g. bitcrush BEFORE vs AFTER eq — audibly different). These are the insert kinds, in
// default order; pass an array of them to fx_order. Sends (echo/reverb) are parallel, not in the chain.
#define FX_TREM     0   // tremolo
#define FX_WAH      1   // auto-wah
#define FX_CHORUS   2   // chorus
#define FX_PHASER   3   // phaser
#define FX_FLANGER  4   // flanger
#define FX_TAPE     5   // tape
#define FX_EQ       6   // EQ
#define FX_CRUSH    7   // bitcrush
#define FX_REVERB   8   // reverb — ONLY valid on a reverb_bus() send-bus; a no-op on any other bus. Put it first, then FX_* after it for reverb→effect
#define FX_FORMANT  9   // formant/vowel filter (a reorderable pedal, like the others — in every bus's default chain)
#define FX_FILTER   10  // resonant filter — the DJ filter (a reorderable pedal, in every bus's default chain)
#define FX_PAN      11  // auto-pan — antiphase tremolo (a reorderable pedal, in every bus's default chain)
#define FX_RINGMOD  12  // ring modulator — signal × sine carrier (a reorderable pedal, in every bus's default chain)
#define FX_ECHO     13  // delay/echo INSERT — in-line dry/wet delay (master only, via echo_insert(); place in fx_order(0,…))
#define FX_GRAINS   14  // granular delay INSERT — capture-and-scatter texture/freeze cloud (via grains()/instrument_grains(); auto-placed, reorderable via fx_order)
#define FX_DRIVE    15  // mix-bus saturation INSERT — drive the summed mix (via drive_insert(); place in fx_order(0,…)). Kinds 16..31 are free (fx_order packs 5 bits of kind per slot)
#define FX_SHALLOW  16  // shallow-water INSERT — a filtered-random short delay + Low Pass Gate (via shallow(); place in fx_order). First kind past the old 16-kind ceiling
#define FX_GATE     17  // noise-gate INSERT — clamps the signal shut below a threshold (via gate()). Put AFTER FX_REVERB in fx_order for gated reverb
#define FX_INST(kind, inst) ((kind) | ((inst) << 5))   // tag a kind with an INSTANCE for fx_order() — two of one effect in a chain (e.g. EQ before AND after a dirt stage). Configure instance n via eq_inst(n,…). Plain FX_* = instance 0. instance 0..1 (two per effect).
void fx_order(int bus, const int *kinds, int n);   // set a bus's insert order: bus 0 = master, 1.. = an instrument's bus; kinds[] of FX_* (or FX_INST(FX_*, n)), n ≤ 16 slots

// fx_mod()/fx_lfo() targets — the curated, sweep-safe params (the API can only name these, so it can't
// be driven into the SET-AND-HOLD stutter). 0..6; reverb/delay sends + wah are deferred (need new knobs).
#define FXMOD_FILTER_CUT   0   // filter() cutoff — 40Hz..18kHz exponential (THE DJ-filter sweep). Call filter() first to engage the filter
#define FXMOD_FILTER_RES   1   // filter() resonance 0..1
#define FXMOD_DRIVE        2   // drive_insert() amount 0..1 (great wobbling under a slow LFO). Needs drive_insert() mix>0
#define FXMOD_TREM_DEPTH   3   // tremolo() depth 0..1
#define FXMOD_PAN_DEPTH    4   // autopan() depth 0..1
#define FXMOD_GRAINS_MIX   5   // grains() dry/wet 0..1 (call grains() first to allocate the tank)
#define FXMOD_SHIMMER_MIX  6   // shimmer() dry/wet 0..1

// leslie — a rotary-speaker cabinet (a spinning treble HORN + bass DRUM): the organ's voice. The
// horn adds pitch wobble (Doppler) + a swirling volume; the two rotors spin at independent speeds
// and take seconds to spin up/down when you flip the speed — the iconic chorale↔tremolo swell.
// THE classic organ effect; great on electric piano, guitar, or a whole psychedelic mix too.
// Runs at the END of the chain (the speaker/cabinet output stage — not a reorderable FX_* pedal).
#define LESLIE_STOP  0   // brake — rotors coast to a halt (a static, slightly-doubled tone)
#define LESLIE_SLOW  1   // chorale — slow, gentle, hymn-like sway (~0.7 Hz)
#define LESLIE_FAST  2   // tremolo — fast, shimmering swirl (~6 Hz). flip SLOW↔FAST for the swell
void leslie(int speed, float drive, float balance, float doppler, float mix);                      // speed LESLIE_*, drive 0..1 (tube preamp), balance 0=drum..1=horn, doppler 0..1, mix 0..1 (0 = off). THE master Leslie
void instrument_leslie(int slot, int speed, float drive, float balance, float doppler, float mix); // Leslie on just this slot (auto-grabs a private FX bus)

// musical scales (C root)
#define SCALE_MAJOR      0   // do re mi fa sol la ti
#define SCALE_MINOR      1   // natural minor
#define SCALE_PENTA      2   // major pentatonic — always pretty
#define SCALE_PENTA_MIN  3   // minor pentatonic — bluesy, moody
#define SCALE_BLUES      4   // minor penta + flat 5
#define SCALE_CHROMATIC  5   // all 12 notes

void tone(int scale, int octave, int instr, int vol);  // play a random note from scale

// chord types — pass to chord()
#define CHORD_MAJ    0   // major triad
#define CHORD_MIN    1   // minor triad
#define CHORD_DIM    2   // diminished
#define CHORD_AUG    3   // augmented
#define CHORD_MAJ7   4   // major 7th
#define CHORD_MIN7   5   // minor 7th
#define CHORD_DOM7   6   // dominant 7th
#define CHORD_SUS4   7   // suspended 4th
#define CHORD_POWER  8   // power chord (root + fifth)

void chord(int root, int type, int instr, int vol);   // play a chord stacked on root (MIDI note)
void strum(int root, int type, int instr, int vol, int delay_ms);  // chord with delay between notes; negative = down-strum
void strum_notes(const int *midis, int n, int instr, int vol, int delay_ms);  // strum an explicit list of MIDI notes (not a chord type); negative delay_ms = down-strum

// musical timing
void  schedule(int delay_ms, int midi, int instr, int vol);  // play a note in the future
void  schedule_hit(int delay_ms, int midi, int instr, int vol, int dur_ms);  // schedule() + hit() in one: a custom-length note at a sample-accurate future time. THE tool for fast sfx/arp steps — no frame-rate jitter
void  bpm(int rate);                                          // set tempo (default 120)
int   beat(void);                                             // current beat counter (advances based on bpm)
float beat_pos(void);                                         // fractional position within current beat: 0.0 → 1.0
bool  every(int n);                                           // true once per n beats — fire it on the beat
bool  euclid(int hits, int steps, int b);                    // Bjorklund: true if step b is one of `hits` spread evenly across `steps`. b can be ANY counter (beat(), frame()/10, a wave index) — evenly distributes spawns/pickups/blinks, not just beats
bool  chance(int percent);                                    // true with the given probability (0–100) — a weighted coin flip for drops, variety, branching, glitches, not just music
int   degree(int scale, int octave, int n);                  // MIDI note for the nth degree of a scale (n can wrap octaves)

// ------------------------------------------------------------
// utility
// ------------------------------------------------------------

int   rnd(int n);                              // random int in [0, n); rnd(6) → 0..5
int   rnd_between(int lo, int hi);             // random int in [lo, hi); rnd_between(3, 8) → 3..7
float rnd_float(void);                         // random float 0..1
float rnd_float_between(float lo, float hi);   // random float in [lo, hi)
float now(void);           // seconds since startup
float dt(void);            // seconds since the last frame (already clamped for hitches). multiply movement/decay by this for framerate-independent speed
int   epoch(void);         // real-world clock: Unix time in whole seconds. unlike now() it keeps counting between runs — store it with save() to tell how long the player was away
int   frame(void);         // frame count since startup (increments once per update)
int   fps(void);           // measured frames per second right now (smoothed over the last second). 60 = running smooth; a lower number means the cart is too slow this frame. show it with watch() while tuning performance
int   sgn(int n);          // -1 if n<0, 0 if n==0, 1 if n>0
int   mid(int a, int b, int c);  // middle value of three. use for clamp: mid(lo, val, hi)
float timer(void);         // seconds since the last timer_reset() (or startup) — a resettable stopwatch
void  timer_reset(void);   // reset timer() back to 0

// ------------------------------------------------------------
// math — angles are in degrees everywhere (0 = right, 90 = down)
// ------------------------------------------------------------

int   abs(int v);                                          // absolute value — drops the minus sign
int   min(int a, int b);                                   // smaller of two
int   max(int a, int b);                                   // bigger of two
float clamp(float v, float lo, float hi);                  // keep v between lo and hi
float lerp(float a, float b, float t);                     // mix a and b: t=0 → a, t=1 → b
float remap(float v, float a, float b, float c, float d);  // remap v from range a..b into range c..d

float distance(int x1, int y1, int x2, int y2);            // distance between two points (pixels)
float length(int x, int y);                                // length of vector (x,y)
float fsqrt(float v);                                      // square root; returns 0 for v<=0 (no need to include math.h)
float angle_to(int x1, int y1, int x2, int y2);            // direction in degrees from point 1 to point 2
float dx(float steps, float degrees);                      // x movement of `steps` pixels in `degrees` direction (keep position in a float)
float dy(float steps, float degrees);                      // y movement of `steps` pixels in `degrees` direction (keep position in a float)
float sin_deg(float degrees);                              // sine of an angle in degrees
float cos_deg(float degrees);                              // cosine of an angle in degrees

// ------------------------------------------------------------
// 3D helpers — leaf primitives for the rotate→project→sort→fill pipeline.
// the cart owns its vertex/face lists; these just remove the math every
// solid-3D cart re-derives. build your scene on these + trifill/tritex.
// ------------------------------------------------------------

typedef struct { float x, y, z; } V3;   // a 3D point/vector — pass to rot3/project, or read the fields directly

V3   rot3(V3 p, float yaw, float pitch);                         // rotate a point: yaw (around Y) then pitch (around X), both degrees. the spin-a-model workhorse
bool project3(V3 p, float focal, float zoom, int *sx, int *sy);  // perspective-divide a point to screen pixels (origin = screen centre). returns false (and leaves *sx/*sy untouched) if p is behind the camera
void zsort(float *key, int *order, int n);                       // painter's sort: fills order[0..n-1] with indices ordered far→near by key[]. reuse for faces, sprites, billboards — draw via order[i]
void quadfill(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int color); // filled quad from 4 corners (two trifills sharing the 0–2 diagonal). corners in order around the face

// ------------------------------------------------------------
// easing — shape a 0..1 value into a curve
// ------------------------------------------------------------

float ease_in(float t);      // start slow, end fast  (t²)
float ease_out(float t);     // start fast, end slow
float ease_in_out(float t);  // slow → fast → slow  (smoothstep)
float ease_back(float t);    // overshoot past the end, then settle back — a snappy "pop"

// ------------------------------------------------------------
// collision
// ------------------------------------------------------------

bool boxes_touch(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh); // do two rectangles overlap? (AABB)
bool point_in_box(int px, int py, int bx, int by, int bw, int bh);                 // is the point inside the box?
bool circles_touch(int ax, int ay, int ar, int bx, int by, int br);               // do two circles overlap?
bool near(int ax, int ay, int bx, int by, int d);                                  // are the two points within d pixels?
bool touching_map(int x, int y, int w, int h);    // does this pixel-space box overlap any non-empty map cell?
int  tile_at(int px, int py);                      // map sprite index at pixel (px,py); same as mget(px/16, py/16)
bool touching_color(int x, int y, int w, int h, int color); // does this box cover any pixel of this palette color?

// ------------------------------------------------------------
// animation
// ------------------------------------------------------------

int anim(int n_frames, float fps, float phase);        // current frame 0..n_frames-1, looping forever. phase 0..1 offsets the cycle start — use (float)i/count to stagger multiple entities
bool blink(int period);                                // true for `period` frames, then false for `period` frames — flashing/blinking via frame(). blink(3) ~ a fast flash

// ------------------------------------------------------------
// strings
// ------------------------------------------------------------

const char *str(const char *fmt, ...);  // printf into a reusable buffer: print(str("score %d", n), x, y, c)

// ------------------------------------------------------------
// persistence — 64 integer slots saved per cart.
// Files (cart.sav / cart.kv / cart.blob) live in a per-cart folder:
// the editor and play.js pass --save-dir saves/<cart> at launch, so
// every cart keeps its own saves under build/saves/ (no flag = cwd).
// ------------------------------------------------------------

void save(int slot, int value);  // store an int that survives between runs (slots 0–63)
int  load(int slot);             // read it back — returns 0 if never saved
void save_int(const char *key, int value);   // like save() but keyed by name — save_int("hiscore", 500). no slot-numbering to remember
int  load_int(const char *key, int def);     // read a named value back — returns `def` if that key was never saved
void save_bytes(const void *data, int len);  // save a whole block of bytes (a struct/array) — for big state the 64 int slots can't hold
int  load_bytes(void *out, int max);          // read it back into `out` (up to max bytes); returns bytes read, 0 if never saved
void *de_state(int bytes);                    // a zero-filled, engine-owned block of `bytes` bytes — keep your whole cart's state in it. Unlike a plain global it survives a live code-reload. The starter cart wraps this as STATE { ... }; (declare once) + S->field (use), so you rarely call it directly.
const char *de_data_path(void);               // path given at launch with --data <file> (or $DE_DATA), else NULL — a data-driven cart loads its world from this file at startup (sloop/roadview drive real OSM cities this way)
const char *de_dropped_file(void);            // path of a file dropped onto the window THIS frame (drag & drop), else NULL — swap a data cart's world by dropping a new file on it
void        de_open_path(const char *path);   // reveal a file or folder in the OS file manager (Finder/Explorer) — show the player where the cart's data files live
void        de_switch_cart(int ctx);          // multi-cart bundles: switch the WHOLE cart world to context `ctx` (0–7) — sound (instruments/effects/bpm, kept & restored per context), video set-and-hold state (pal/fillp/font/camera reset to defaults so the outgoing cart's state can't bleed in), and the per-cart sprite sheet. Same ctx = no-op. The dispatcher shim calls this when changing carts (see share-panel.md)

// ------------------------------------------------------------
// noise — smooth random values (0..1). nearby inputs → similar outputs.
// ------------------------------------------------------------

float noise(float x);                    // 1D: organic motion, wobble, pulses
float noise2(float x, float y);          // 2D: terrain, fog, flow fields
float noise3(float x, float y, float z); // 3D: animated 2D noise (pass now() as z)

// ------------------------------------------------------------
// debug
// ------------------------------------------------------------

void printh(const char *fmt, ...);                   // printf to the editor's runtime log panel (not the game window)
void watch(const char *name, const char *fmt, ...);  // show a named live value in the corner of the game window
void watch_visible(bool on);                         // hide/show the watch overlay (default: on; F1 toggles it)
bool paused(void);                                   // true while the runtime pause overlay is open (P/ENTER to toggle; a key the cart reads via key()/keyp()/keyr() is claimed by the cart and won't trigger pause)

// ------------------------------------------------------------
// EXPERIMENTAL — may change or vanish without notice. Documented in the help
// tab under "experimental — testing only" so they're discoverable, but NOT for
// games: these exist to gather evidence for an open design question, then
// resolve into real API or get deleted. Current experiments + their sunset
// rules live in docs/design/palette-and-color.md and docs/design/probe-carts.md.
// ------------------------------------------------------------

void palette_hex(int i, int hex);  // EXPERIMENT, testing only: write raw 0xRRGGBB into live palette slot i (0-63). Primitives + existing sprite art recolor immediately; pal_reset() restores the shipped palette. For palette try-outs, not games — see palette-and-color.md

#endif // STUDIO_H
