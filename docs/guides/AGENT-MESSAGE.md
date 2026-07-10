# Parallel-build agent message

Paste this into each spawned session (along with the assembled `brief + <spec>.md` — see [`cart-specs-index.md`](cart-specs-index.md)).
It lets the session build the cart fully — including baking its own thumbnail, now that
`--run` is isolated per cart — and stops it only at the one shared-file step that races the
other sessions: the `index.json` merge. It ends by emitting just the `index.json` JSON blob,
which the coordinator merges centrally.

See `README.md` here for the spec list and the `brief + spec` assembly command. This
copy-paste flow is the manual path; the **orchestrated** alternative (a single Workflow
fanning out one agent per game, with the JSON blob below replaced by a validated
structured-output schema) is described under "Building many in parallel" in `README.md`
and is how batch 2 was built.

---

```
You're one of ~several cartridges being built IN PARALLEL in the same dreamengine repo
working tree. Build your cart fully (write it, make the PNG, bake its thumbnail), but
DON'T touch index.json — that's the one shared file a coordinator merges at the end.

DO:
  1. Read runtime/studio.h (the API truth — never invent a signature) and the exemplar
     carts your spec names, then write tools/carts/<name>.c  (+ tools/carts/<name>.cart.js
     if it uses sprites/maps). Write correct, COMPILING C: don't name variables after
     builtins (map/line/rect/spr/print/timer/now...), and #include <stddef.h> if you use NULL.
  2. Produce the cart PNG with the CREATE step (parallel-safe — writes just your own file):
        node tools/make-cart.js tools/carts/<name>.c editor/public/carts/<name>.cart.png
     (run `nvm use 22` first if node complains).
  3. Bake your own thumbnail — also parallel-safe now: `--run` builds each cart in its
     OWN scratch dir (build/.bake/<name>/), never the shared build/ dir, so it can't
     collide with other sessions, cross-bake a thumbnail, or hijack a live editor session:
        node tools/make-cart.js --run editor/public/carts/<name>.cart.png
     This compiles your cart, so it also surfaces any compile error — fix it and re-run
     the CREATE step + this one until both pass.

DO NOT:
  • do NOT edit editor/public/carts/index.json — concurrent appends clobber each other.
    Leave the merge to the coordinator (see FINISH).

FINISH by ending your final message with ONLY your index.json entry as a JSON object —
nothing after it — like:

  { "title": "your title", "description": "what it is + the controls", "file": "<name>.cart.png",
    "kind": ["game"], "genre": "arcade",
    "teaches": ["<conceptual techniques from tools/teaches-vocab.js>"],
    "lineage": "what it descends from / what's new (one line)" }

teaches/lineage feed the ★ techniques compendium. teaches is a CONTROLLED vocabulary
(tools/teaches-vocab.js) — reuse existing tags; lint-carts.js rejects an off-vocabulary
tag. teaches is REQUIRED — use [] if the cart teaches nothing conceptually distinctive.
lineage is OPTIONAL but encouraged — one prose line on what the cart descends from and
what's new (name the cart you copied a chassis from); omit it if there's no real story.
Do NOT put these as // TEACHES: comments in the .c — the index.json entry is the only home.
The coordinator will merge that entry into index.json (your thumbnail is already baked).
```
