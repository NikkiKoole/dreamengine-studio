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

// ------------------------------------------------------------
// buttons
// ------------------------------------------------------------

#define BTN_UP     0
#define BTN_DOWN   1
#define BTN_LEFT   2
#define BTN_RIGHT  3
#define BTN_A      4   // z / gamepad a
#define BTN_B      5   // x / gamepad b

// ------------------------------------------------------------
// palette (32 fixed colors, 0-31) — the PICO-8 palette
// names per https://pico-8.fandom.com/wiki/Palette
// ------------------------------------------------------------

// standard 16 (0-15)
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
int  touch_ceiling(void);                // max fingers this device tracks: 5 iPhone, 10 iPad, 0 desktop/unknown — one MORE than this cancels ALL touches on iOS

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
void font(int f);        // set active font for all print calls; font(FONT_NORMAL) resets to the default 8×8
int  text_width(const char *text);                                 // pixel width using the active font — for centering in your own boxes
int  print(const char *text, int x, int y, int color);             // returns x after the last char (so you can chain or check if text went offscreen)
int  print_centered(const char *text, int x, int y, int color);    // draw text centered on x; returns x after last char
int  print_right(const char *text, int right_x, int y, int color); // right-align text at right_x; returns x after last char
int  print_scaled(const char *text, int x, int y, int color, int scale); // bigger text for titles/menus (scale 2 = double size); returns x after last char
int  print_outline(const char *text, int x, int y, int color, int outline_color); // text with a 1px outline in all 8 directions; maximum legibility
void line(int x1, int y1, int x2, int y2, int color);
void bezier(int x0, int y0, int cx, int cy, int x1, int y1, int color);                                                    // quadratic Bezier: smooth curve from (x0,y0) to (x1,y1) pulled toward control point (cx,cy)
void pset(int x, int y, int color);                     // set a single pixel (pairs with pget)
void rect(int x, int y, int w, int h, int color);       // rectangle border
void rectfill(int x, int y, int w, int h, int color);   // filled rectangle
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
int  pget(int x, int y);                                // palette index at (x,y), or 0 if no match
int  sget(int sx, int sy);                              // palette index of the spritesheet pixel at (sx,sy) — read sprites as data (paint a level, lookup tables). 0 if out of range
void camera(int x, int y);                              // plain camera: shifts all drawing by (-x,-y), no zoom/rotation. camera(0,0) resets
void camera_ex(int x, int y, float zoom, float angle);  // camera with zoom (1=normal, 2=2x in) and angle (degrees), pivoting on screen center. for a screen-space HUD after zooming, call camera(0,0)
void follow(int tx, int ty, int world_w, int world_h); // center camera on (tx,ty), clamped so world edges don't show
void clip(int x, int y, int w, int h);                  // scissor rect. clip(0,0,0,0) disables
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

void sfx(int n);                              // play sfx slot n; -1 stops all sfx
void note(int midi, int instr, int vol);                  // one-shot note (250ms). vol 0..7. `instr` is an instrument slot (0..4 are the waves above; define 5..31 yourself)
void hit(int midi, int instr, int vol, int dur_ms);       // note with custom duration — closed hihat ~30ms, open ~200ms

// held notes — start a sustained voice and drive it live; the opposite of fire-and-forget note()
int  note_on(int midi, int instr, int vol);               // start a sustained note → returns a handle. vol 0 = start silent. note_off it later
void note_off(int handle);                                // release a held note (lets its envelope fade out)
void note_pitch(int handle, float midi);                  // slide a held note to a new pitch (float → between notes); no retrigger
void note_vol(int handle, int vol);                       // change a held note's volume 0..7 live; 0 = silent but still alive
void note_cutoff(int handle, int hz);                     // sweep a held note's filter cutoff live (needs a filter on its instrument slot)
void note_res(int handle, int resonance);                 // sweep a held note's filter resonance 0..15 live (pairs with note_cutoff for the acid squelch)
void note_lfo(int handle, int which, int dest, float rate_hz, float depth);  // retune a held note's LFO `which` (0..2) live — dest LFO_PITCH/DUTY/VOLUME/CUTOFF; depth 0 = off
void note_env(int handle, int which, int dest, int attack_ms, int decay_ms, float amount);  // set a held note's mod-envelope `which` (0..1) live — same shape as instrument_env(); amount 0 = off
void note_filter(int handle, int mode);                   // switch a held note's filter mode live (FILTER_OFF/LOW/HIGH/BAND/NOTCH)
void note_glide(int handle, int ms);                      // portamento: make note_pitch slide over `ms` instead of snapping (0 = snap)
void note_duty(int handle, float duty);                   // change a held note's pulse width 0.0..1.0 live (pulse/square slots only)
void note_off_all(void);                                  // release every held note at once (panic / cleanup)

// instruments — give a slot a waveform + ADSR envelope, then play it like any wave: note(midi, slot, vol)
void instrument(int slot, int wave, int attack_ms, int decay_ms, int sustain, int release_ms); // define slot 5..31: ms timings, sustain 0..7. pluck = fast attack+short release; pad = slow attack+long release
void wave_set(int which, const float *samples, int n);    // fill custom wave INSTR_USER0+which (which 0..3) with one drawn cycle: n samples in -1..1, resampled to 64. Live — a ringing note morphs as you redraw
void instrument_duty(int slot, float duty);               // pulse width 0.0..1.0 for a square-wave slot (0.5 = square, 0.12 = thin/nasal). no effect on other waves

// one routable LFO per instrument — a slow sine that wobbles one parameter
#define LFO_PITCH   0   // vibrato — depth in semitones (0.3 subtle, 2 wide)
#define LFO_DUTY    1   // PWM / duty sweep — depth 0.0..0.5 (square-wave slots only)
#define LFO_VOLUME  2   // tremolo — depth 0.0..1.0
#define LFO_CUTOFF  3   // filter sweep / wah — depth in Hz (needs a filter on the slot)
void instrument_lfo(int slot, int which, int dest, float rate_hz, float depth);  // attach sine LFO `which` (0..2 — a slot has 3) to a slot. dest: LFO_PITCH/DUTY/VOLUME/CUTOFF. rate 4–8 Hz typical. depth 0 = off

// resonant filter per instrument — sculpts the tone (the SID-style knob)
#define FILTER_OFF   0
#define FILTER_LOW   1   // lowpass — keep lows, muffle highs (warm)
#define FILTER_HIGH  2   // highpass — keep highs, thin out lows (tinny)
#define FILTER_BAND  3   // bandpass — keep only a band around cutoff (vowel/wah)
#define FILTER_NOTCH 4   // notch — scoop OUT a band around cutoff (phasey)
void instrument_filter(int slot, int mode, int cutoff_hz, int resonance);  // mode FILTER_*, cutoff in Hz (e.g. 800), resonance 0..15 (high = whistly peak). sweep cutoff with LFO_CUTOFF

// modulation envelopes per instrument — a one-shot AD contour (the envelope twin of the LFO).
// fires once per note: ramps up over attack_ms, then decays back over decay_ms. amount is
// bipolar and its units depend on dest. 2 per slot (which 0..1), so a filter sweep + a pitch
// blip can run together. there is NO ENV_VOLUME — that's the amp envelope (instrument()).
#define ENV_CUTOFF  0   // sweep filter cutoff — the pluck "pew"/"dwow". amount in Hz (+ opens then closes). needs a filter on the slot
#define ENV_PITCH   1   // pitch blip — drum punch / attack snap / zap. amount in semitones (+ starts sharp, settles to the note)
#define ENV_DUTY    2   // pulse-width sweep (square/pulse slots only). amount 0.0..1.0
void instrument_env(int slot, int which, int dest, int attack_ms, int decay_ms, float amount);  // attach mod-envelope `which` (0..1) to a slot. dest ENV_*. pluck: ENV_CUTOFF amount 1500, attack 0, decay 120. amount 0 = off

// engine macros — three 0..1 knobs that EVERY modeled engine answers; what each knob sweeps
// is per-engine (documented on the INSTR_PLUCK/MALLET/FM lines above), but the API never
// grows when a new engine lands. Defaults: 0.5.
void instrument_harmonics(int slot, float x);  // engine macro 1 of 3 — e.g. PLUCK ring time · MALLET bar material · FM carrier:mod ratio
void instrument_timbre(int slot, float x);     // engine macro 2 of 3 — e.g. PLUCK pick brightness · MALLET mallet hardness · FM brightness
void instrument_morph(int slot, float x);      // engine macro 3 of 3 — e.g. PLUCK pick position · MALLET ring length · FM feedback growl
void instrument_choke(int slot_a, int slot_b); // declare that a new note on slot_a instantly kills any sounding voice on slot_b (open/closed hat choke)
void note_harmonics(int handle, float x);      // live macro on a held note, slewed — ring/partial/ratio changes reach a sounding voice (PLUCK ring, MALLET partials, FM ratio)
void note_timbre(int handle, float x);         // live macro on a held note, slewed — strike-shaping macros (PLUCK/MALLET timbre) apply at the next note; FM brightness moves live
void note_morph(int handle, float x);          // live macro on a held note, slewed — MALLET ring/motor and FM feedback move live; PLUCK position applies at the next note

// drive — saturation AFTER the filter, so resonance screams into it. The grit knob.
void instrument_tune(int slot, float semitones); // detune a slot ±24 semitones (fractions are the point: 0.06 = unison shimmer, ±1 = a tuning trimmer). LIVE — every sounding voice on the slot bends, scheduled arp/seq hits included. 0 = off (default)
void instrument_drive(int slot, float x);      // overdrive a slot 0.0..1.0 — 0 = clean (default), 0.3 = warm, 1 = fuzz. loudness stays put; character changes
void note_drive(int handle, float x);          // sweep a held note's drive live, slewed — ride it up mid-phrase for the acid scream

// echo — THE shared echo bus (there is exactly one): each slot chooses how much to send
// into it. Repeats get darker every pass (tone), and feedback past 1.0 self-oscillates
// like a tape echo instead of exploding. Sweeping the time live pitch-bends the tail.
void echo(int time_ms, float feedback, float tone);  // configure the bus: delay 1..2000ms, feedback 0..1.1 (>1 = runaway), tone 0..1 (dark..bright repeats)
void instrument_echo(int slot, float send);    // how much this slot feeds the bus 0.0..1.0 — 0 = dry (default), 0.15 = slapback, 0.8 = dub throw
void note_echo(int handle, float x);           // sweep a held note's echo send live, slewed — throw a single phrase into the tail

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
