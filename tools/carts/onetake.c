/* de:meta
{
  "title": "onetake",
  "kind": ["toy", "tech-demo"],
  "teaches": [],
  "created": "2026-07-08",
  "resizable": true,
  "description": "The 'one take, many ratios' idea made watchable. Every element is positioned by a PERCENTAGE of the live screen (screen_w()/screen_h()), and every control is a KEYBOARD shortcut - which has no coordinates, so one recorded scenario (keys only) replays truthfully at ANY canvas size. Record it once, then bake the SAME take at a landscape, a square and a 9:16 vertical and watch it reflow. Keys: 1-4 send the ball to 12/38/62/88% of the width, LEFT/RIGHT nudge it, C recenters; SPACE slides the title in/out, T cycles its text; W wipes a panel across. A centre crosshair (50%,50%) and a 75%-width marker stay proportionally put through every reflow - the position-free, resolution-portable ideal from docs/design/export-ratios.md + resolution-portable-input.md."
}
de:meta */
// ONETAKE — the "one take, many ratios" proof, made playable.
// Everything is positioned relative to screen_w()/screen_h() (a % of the live
// canvas), and every control is a KEYBOARD shortcut (position-free). So a single
// recorded input track replays identically at any canvas size — record once,
// bake every ratio. docs/design/export-ratios.md (render side) +
// docs/design/resolution-portable-input.md (input side).
#include "studio.h"

static float ballX = 0.5f, ballTX = 0.5f;   // ball x + target, as a fraction of width
static float titleA = 0.0f, wipe = 0.0f;     // eased 0..1 drivers (slide the title / the wipe panel)
static int   titleShown = 0, wipeOn = 0, titleIdx = 0;   // note: `frame` is a built-in — use frame()

static const char *TITLES[3] = { "ONE TAKE", "MANY RATIOS", "KEYS = PORTABLE" };
static float toward(float v, float target, float rate) { return v + (target - v) * rate; }

void update(void) {
  if (keyp('1')) ballTX = 0.12f;
  if (keyp('2')) ballTX = 0.38f;
  if (keyp('3')) ballTX = 0.62f;
  if (keyp('4')) ballTX = 0.88f;
  if (keyp('C')) ballTX = 0.50f;
  if (key(KEY_LEFT))  ballTX -= 0.012f;
  if (key(KEY_RIGHT)) ballTX += 0.012f;
  if (ballTX < 0.04f) ballTX = 0.04f;
  if (ballTX > 0.96f) ballTX = 0.96f;
  if (keyp(KEY_SPACE)) titleShown = !titleShown;
  if (keyp('T')) { titleIdx = (titleIdx + 1) % 3; titleShown = 1; }
  if (keyp('W')) wipeOn = !wipeOn;
  ballX  = toward(ballX,  ballTX,               0.16f);
  titleA = toward(titleA, titleShown ? 1.f : 0.f, 0.14f);
  wipe   = toward(wipe,   wipeOn ? 1.f : 0.f,     0.14f);
}

void draw(void) {
  int W = screen_w(), H = screen_h();
  int cx = W / 2, cy = H / 2;
  cls(CLR_DARK_BLUE);

  // proportional guides — a % of the screen, so they stay put through any reflow
  line(cx, 0, cx, H, CLR_DARKER_GREY);
  line(0, cy, W, cy, CLR_DARKER_GREY);
  print_centered("center 50%", cx, cy + 5, CLR_MEDIUM_GREY);
  int mx = (int)(0.75f * W);
  line(mx, 0, mx, H, CLR_DARK_GREY);
  print_centered("75%", mx, H - 26, CLR_MEDIUM_GREY);

  // the ball — eased toward a %-of-width target set by the number keys
  int bx = (int)(ballX * W);
  int r  = (W < H ? W : H) / 12; if (r < 4) r = 4;
  circfill(bx, cy, r, CLR_YELLOW);
  circfill(bx, cy, r / 2, CLR_ORANGE);

  // pulsing beat dot (top-right, anchored) so there's always motion — clippable
  circfill(W - 12, 12, 3 + (frame() / 6) % 4, CLR_RED);

  // wipe-panel transition (W) — slides in from the left, covering wipe * W
  if (wipe > 0.001f) {
    int ww = (int)(wipe * W);
    rectfill(0, 0, ww, H, CLR_INDIGO);
    if (ww > 44) print_centered("WIPE", ww / 2, cy, CLR_WHITE);
  }

  // title (SPACE toggles, T cycles) — slides down from above the top edge
  int ty = (int)(-16 + titleA * (H * 0.20f + 16));
  if (titleA > 0.02f) print_centered(TITLES[titleIdx], cx, ty, CLR_WHITE);

  // live dims + the shortcut legend — anchored so you SEE the size each bake ran at
  print(str("%dx%d", W, H), 6, 6, CLR_LIGHT_GREY);
  print("1-4 ball  C centre  </> nudge  SPACE title  T text  W wipe", 6, H - 12, CLR_LIGHT_GREY);
}
