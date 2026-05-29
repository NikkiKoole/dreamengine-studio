# Cart authoring tools

Scripts in `tools/` for creating and updating `.cart.png` files without the editor UI.
Designed to be used by Claude in-session via the Bash tool.

---

## `tools/make-cart.js`

All cart operations go through this one script.

### Create a cart

```bash
node tools/make-cart.js <source.c> <output.cart.png>
```

Embeds the C source, a sprite sheet, and a map into a PNG.
If a `<source>.cart.js` config file exists alongside the `.c` file, sprites and map are
taken from there. Otherwise sprites are blank and the map is empty.

### Bake a real screenshot into an existing cart

```bash
node tools/make-cart.js --run <cart.png>
```

Extracts the cart's source/sprites/map, compiles with clang, runs the binary with
`--screenshot` (3 frames, window flashes briefly), reads `build/screenshot.png`, and
writes it back as the cart's visible thumbnail. The data chunks are untouched.

**This is the normal finishing step after creating a cart.**

### Update just the screenshot (manual)

```bash
node tools/make-cart.js --update <cart.png> <screenshot.png>
```

Replaces the thumbnail of an existing cart with any PNG you supply.
Useful if the user ran the cart themselves and you want to pull in `build/screenshot.png`.

---

## Typical workflow for a new tutorial cart

```bash
# 1. write the source
# (edit tools/XX-name.c and optionally tools/XX-name.cart.js)

# 2. generate the cart with sprites + map baked in
node tools/make-cart.js tools/XX-name.c editor/public/carts/XX-name.cart.png

# 3. compile, run 3 frames, bake real screenshot
node tools/make-cart.js --run editor/public/carts/XX-name.cart.png
```

Done. The cart now has the correct thumbnail and can be loaded in the editor.

---

## The `.cart.js` config file

Place a `XX-name.cart.js` file next to `XX-name.c` to supply sprites and/or a map.
It's a CommonJS module — `require()` works, comments work.

```js
module.exports = {
  // optional: custom char→palette-index mapping (merged with default)
  charMap: { 'X': 8 },   // add red as 'X'

  // optional: sprites by slot index
  sprites: {
    1: `...sprite as ASCII art...`,
    2: [/* flat 256-element array of palette indices */],
  },

  // optional: map
  map: {
    layout: [             // one string per row, any width up to MAP_W
      '##########',
      '#        #',
      '#        #',
      '##########',
    ],
    tiles: { '#': 1 },   // char → tile index  (default: '#'→1, ' '/'.'→0)
  },
}
```

---

## Sprite format

Sprites are 16×16 pixels. Each sprite occupies one slot in the 128×128 sheet
(8 columns × 8 rows of slots, slot 0 top-left).

**ASCII art string** (preferred — readable):

```js
sprites: {
  1: `
................
.....YYYYYY.....
....YYYYYYYY....
....YYYYYYYY....
.....YYYYYY.....
....BBBBBBBB....
..BBBBBBBBBBBB..
..BBBBBBBBBBBB..
....BBBBBBBB....
....BBBBBBBB....
....BBBBBBBB....
...BBB....BBB...
...BBB....BBB...
...BBB....BBB...
................
................
`,
}
```

**Flat array** (useful when generated programmatically):

```js
sprites: {
  1: new Array(256).fill(0).map((_, i) => i % 16),
}
```

### Default char map

| Char | Color | Index |
|------|-------|-------|
| `.` `0` | black / transparent | 0 |
| `1`–`9` | palette indices 1–9 | 1–9 |
| `a`–`f` | palette indices 10–15 | 10–15 |
| `K` | blacK | 0 |
| `B` | dark Blue | 1 |
| `P` | dark Purple | 2 |
| `G` | dark Green | 3 |
| `N` | browN | 4 |
| `S` | dark grey (Slate) | 5 |
| `L` | Light grey | 6 |
| `W` | White | 7 |
| `R` | Red | 8 |
| `O` | Orange | 9 |
| `Y` | Yellow | 10 |
| `g` | bright Green | 11 |
| `b` | bright Blue | 12 |
| `I` | Indigo | 13 |
| `k` | pinK | 14 |
| `p` | Peach | 15 |

Add your own via `charMap: { 'X': 8 }` in the config.

---

## Map format

The map is `MAP_W × MAP_H` bytes (default 128 × 64), one byte per cell = tile index.
Tile 0 is empty; tile 1+ are sprite slot numbers.

```js
map: {
  layout: [
    '##########',     // each string is one row
    '#        #',
    '#  ##    #',
    '#        #',
    '##########',
  ],
  tiles: {
    '#': 1,           // wall = sprite slot 1
    'w': 2,           // water = sprite slot 2
    // space and '.' are always 0 (empty)
  },
}
```

Rows shorter than `MAP_W` are zero-padded. Rows beyond `MAP_H` are ignored.

---

## Cart file locations

| Path | Purpose |
|------|---------|
| `tools/XX-name.c` | cart source code |
| `tools/XX-name.cart.js` | optional sprites + map config |
| `editor/public/carts/XX-name.cart.png` | finished cart (served to the tutorials page) |
| `editor/public/carts/index.json` | metadata list for the tutorials panel |
| `build/screenshot.png` | last screenshot from `--run` or from the editor |

---

## Adding a tutorial to the editor

1. Create `tools/XX-name.c` (and optionally `tools/XX-name.cart.js`)
2. Run the two-step build above
3. Add an entry to `editor/public/carts/index.json`:

```json
{
  "title": "X. short title",
  "description": "One sentence. What the student learns.",
  "file": "XX-name.cart.png"
}
```

The tutorials panel picks it up automatically on next reload — no code changes needed.
