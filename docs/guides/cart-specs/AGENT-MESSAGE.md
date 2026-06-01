# Parallel-build agent message

Paste this into each spawned session (along with the assembled `brief + <spec>.md`).
It lets the session build the cart fully but stops it at the two shared-file steps that
race the other parallel sessions — it ends by emitting just the `index.json` JSON blob,
which the coordinator bakes + merges centrally.

See `README.md` here for the spec list and the `brief + spec` assembly command.

---

```
You're one of ~several cartridges being built IN PARALLEL in the same dreamengine repo
working tree. Build your cart fully, but DON'T do the two steps that fight the other
sessions — a coordinator handles those at the end.

DO:
  1. Read runtime/studio.h (the API truth — never invent a signature) and the exemplar
     carts your spec names, then write tools/carts/<name>.c  (+ tools/carts/<name>.cart.js
     if it uses sprites/maps). Write correct, COMPILING C: don't name variables after
     builtins (map/line/rect/spr/print/timer/now...), and #include <stddef.h> if you use NULL.
  2. Produce the cart PNG with the CREATE step only (this is parallel-safe — it writes
     just your own file, never the shared build/ dir):
        node tools/make-cart.js tools/carts/<name>.c editor/public/carts/<name>.cart.png
     (run `nvm use 22` first if node complains).

DO NOT:
  • do NOT run `node tools/make-cart.js --run ...`  — it uses the shared build/ dir +
    screenshot.png and will collide with the other sessions / cross-bake thumbnails.
    The coordinator compiles + bakes every thumbnail serially at the end (and fixes any
    compile error), so don't worry about the screenshot or verifying the build yourself.
  • do NOT edit editor/public/carts/index.json — concurrent appends clobber each other.

FINISH by ending your final message with ONLY your index.json entry as a JSON object —
nothing after it — like:

  { "title": "your title", "description": "what it is + the controls", "file": "<name>.cart.png" }

The coordinator will bake your thumbnail and merge that entry into index.json.
```
