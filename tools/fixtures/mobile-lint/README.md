# mobile-lint fixture — known answers

`node tools/mobile-lint.js --selfcheck` re-runs the tool as a child with `DE_MOBILE_CARTS_DIR` /
`DE_MOBILE_RUNTIME_DIR` / `DE_MOBILE_CART_EXT` pointed here, and asserts 27 known answers over 12
fixture carts. `.c.txt`, never `.c`, for the usual reason (never compiled; a real `.c` gets indexed
by clangd and read as a cart by anything globbing for sources).

**Why it needs one.** The verdict is a **precedence chain** — a cart reading touch *and* mouse
*and* `btn` *and* `key` must rank by the best path a phone can use, not the worst — and the whole
thing is regexes over source that passes through **three transforms** first:

1. comment stripping (a commented-out `touch_x()` must not count),
2. library-header inlining (a cart whose whole input story is in `ui.h` widgets must lint green),
3. a **`studio.h` skip** — without which its prototypes, which name every input function, make
   *every cart in the repo* read `touch-ready`.

Get #3 wrong and the tool cheerfully reports a green shelf. Nothing measured any of it.

## One cart per judgement

| cart | verdict | pins |
|---|---|---|
| `touchy` | touch-ready | `touch_*()` reads |
| `mousey` | tap-as-mouse | a tap is a click. Reads mouse **position too**, so the `hover` exemption is testable |
| `btnonly` | fixable | one line of config away |
| `keyonly` | keyboard-only | + `keys(A,SPACE)` |
| `silent` | no-input | a radio/generative cart |
| `precedence` | touch-ready | reads **all four** paths; best wins, and the dead ones become warnings |
| `hovery` | no-input + `hover` | mouse position, no button, no touch |
| `warnings` | touch-ready | `wheel` · `right/middle` · `touch>5` · `tiny-target(8x8)` · `text-input` |
| `commented` | no-input, zero warnings | transform #1 |
| `viaui` | touch-ready | transform #2 — its own body reads nothing |
| `viastudio` | no-input | transform #3 |
| `cfgtouch` | touch-ready | `.cart.js` `touchControls` promotes a `btn()`-only cart |

`runtime/studio.h` here is deliberately stuffed with input prototypes so the skip has something to
skip, and `--selfcheck` asserts those prototypes are still present — otherwise trimming it makes
the `viastudio` assertion vacuous.

## Four fixture shapes forced by mutation-testing

Every one of these started as an assertion that could not fail:

- **`mousey` reads `mouse_x()` as well as the button.** `hover` fires on
  `mousePos && !mouseBtn && !touch`; a cart with no `mouse_x()` at all can never trigger it, so
  "hover does not fire when a button is read" was vacuous and widening the guard to
  `if (reads.mousePos)` still scored 26/26.
- **`precedence` reads `key('Q')` on the same line as a `tap()`, and `key('Z')` alone.**
  `keys-untapped` skips lines that already offer a tap, so without a key sharing a line with one,
  deleting the whole filter changed nothing.
- **`runtime/fakeui.h` and `runtime/fakegest.h` include each other.** A real cycle, so the `seen`
  recursion guard has something to break; without it, removing the guard changed nothing (with it,
  removing the guard now blows the stack and 22 of 27 go red).
- **`cfgtouch.cart.js` must actually load.** See below.

## What the fixture found

`lint()` did `require(cfgFile)` on a path built from `CARTS_DIR`. `require()` treats a bare
**relative** path as a module *name*, throws `MODULE_NOT_FOUND`, and the surrounding `catch {}`
swallows it — so a relative carts dir silently reads `touchControls` as false and downgrades a
touch-ready cart to `fixable`. Production always passed an absolute path, so this only became
reachable once the override existed; it is now `require(path.resolve(cfgFile))`.

Confirmed by mutation: reversing the precedence chain, dropping comment stripping, inlining
`studio.h`, not inlining library headers, dropping the recursion guard, ignoring `touchControls`,
widening the `hover` guard, flattening the `touch>5` and `tiny-target` thresholds, dropping
`also-reads-keys`, ignoring `touchControls` in `btn-without-overlay`, dropping the `keys()` list,
and dropping the `keys-untapped` line filter — each turns 1–22 assertions red.

See `docs/guides/checks-and-oracles.md` → "Self-test the checker".
