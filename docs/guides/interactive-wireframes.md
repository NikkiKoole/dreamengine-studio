# Building an interactive wireframe (cart)

STATUS: GUIDE (2026-07-07). How to build a **clickable, device-honest wireframe as a cart** — the
way `tools/carts/acidwire.c` prototypes the acidrack redesign. Worked example throughout: acidwire.

> **Why a cart, not an HTML/Figma mock** (the [field-018](../field-notes/018-passing-the-gates-felt-like-done.md)
> lesson + [ADR: wireframe as a cart](../decisions/)): a cart has **no translation gap** — it *is* the
> production path. It renders the real fonts, the real `lay.h` reflow, real finger sizes, real
> safe-area insets. If a primitive is missing you add it to the engine, which is groundwork, not
> throwaway. An **interactive** one is far better to study than a static picture: you *click through*
> the states instead of imagining them.

## The recipe (what acidwire does, reusable)

1. **Fixed window, flip the canvas per device.** A design tool wants to compare shapes, so give it a
   big fixed window (`<name>.cart.js` → `{ screenW, screenH, scale }`) and call `de_resize(w,h)` to
   shrink the *canvas* to each device's logical size — the engine letterboxes it (centered for a
   control-less cart). Shapes come from [`device-matrix.md`](../design/device-matrix.md) §2. On
   device (built `-DDE_RESIZABLE`) you'd instead read the *real* `screen_w()/screen_h()`.
   *(`de_resize` is a `platform.h` seam; `extern void de_resize(int,int);` in the cart.)*

2. **One honest finger unit.** The iOS canvas is `points / K` (K=2 pixel-chunk), so a 44pt finger is
   a **constant 22 logical px**: `#define FU 22.0f`. Lay everything out in `FU` and control-comfort
   is honest at every shape. Add a **finger-grid overlay** (acidwire's `g`) — dotted `FU` cells + a
   solid swatch — so "does it fit for fingers?" is eyeballed, not guessed.

3. **Draw with `lay.h` + raw primitives, no real logic.** `lay_split/grid/wrap/at/pad/inset` for
   geometry; `boxfill/boxrect/circ/line/print` for the controls. Pattern/note data is faked with
   tiny deterministic generators (acidwire's `p303_note` etc.) — a wireframe shows *shape*, not state.

4. **Be device-honest: draw the safe-area as keep-out.** Simulate the notch / Dynamic Island /
   rounded corners / home-indicator / status bar per device + a dashed safe boundary (acidwire's
   `draw_safe_skin`, toggle `s`). It makes "don't put a control there" visible on the desktop.

5. **Make it clickable — an immediate-mode click layer.** This is what turns a picture into a study:
   ```c
   static int m_press, m_x, m_y;
   static int clicked(Box b) {   // true ONCE for the first control the press lands in, then consumes it
       if (m_press && m_x>=b.x && m_x<b.x+b.w && m_y>=b.y && m_y<b.y+b.h) { m_press = 0; return 1; }
       return 0;
   }
   // once per frame, top of draw():  m_press = mouse_pressed(0); m_x = mouse_x(); m_y = mouse_y();
   // at each control, right where you draw it:  if (clicked(box)) do_the_thing;
   ```
   Draw/check **specific controls before general** (a mute button before the strip catch-all) so the
   consume order resolves nesting. Single-touch taps synthesise the mouse, so this one path serves
   **desktop click AND device touch**.

6. **Mirror clicks on the keyboard.** Keep key handlers too — they drive `play.js --script` for
   deterministic renders and are handy on desktop. (Acidwire: tap a strip name *or* press `w`/`f`.)

7. **Render + verify from the CLI.** Walk the shapes with `play.js --dump` and `montage` into a
   matrix; **script clicks** (`click <frame> <x> <y>`) to prove interactions fire, then diff the
   before/after frames. No manual clicking needed to check it works.

## Gotchas (all bit acidwire)

- **Raylib letter keycodes are UPPERCASE** — `keyp('w')` (119) never matches `KEY_W` (87). Read
  `keyp('W')`. Digits and `]`/`[` match ASCII, so the bug hides. (`w`/`f`/`m`/`p` were silently dead.)
- **Don't name a helper `hit`** — it collides with an engine symbol; acidwire uses `clicked`.
- **`gr_place` DECK-pins a control-less canvas to the top** when the window/canvas aspect differ —
  the engine now centers when `show_touch_ui` is off, so a letterboxed wireframe sits centered.
- **`--dump` crops to the active canvas** (the device), not the window — great for per-shape shots.
- Verify a `studio.c` change on **both** build paths: `build-all` misses `DE_NO_RAYLIB` — also run
  `build-nr.sh`.

## The payoff loop

Tweak `lay.h`/`FU` numbers → `play.js --dump` + montage → look → repeat, and *click through* the
states in the editor (`▶ run`). The design decisions land as commits + a matrix PNG, judged against
the rack's brief (e.g. [`acidrack-layout-brief.md`](../design/acidrack-layout-brief.md)).

Related: [`device-matrix.md`](../design/device-matrix.md) (the shapes) ·
[`device-adaptive-layout.md`](../design/device-adaptive-layout.md) (the plan) ·
[`acidrack-layout-brief.md`](../design/acidrack-layout-brief.md) (a rack brief) ·
[`cart-authoring.md`](cart-authoring.md) · `runtime/lay.h`.
