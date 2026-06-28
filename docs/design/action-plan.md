# Action plan — what to build next (synthesized 2026-06-29)

STATUS: LIVING ACTION LIST. Synthesizes the concrete, do-able asks scattered across the
field-notes journal and the three audio-product brainstorms into one prioritized, checkable list.
Sources, each linked at point of use:
- **field-notes/** — the research journal (`tools-we-need.md`, `distillator.md`, `about tags.md`,
  `ai-friendly-frontmatter.md`, notes `002`/`003`/`011`/`012`/`013`, and the cart-polish log
  `0000000-cgecking-all-carts.md`).
- [`tinydaws.md`](tinydaws.md) + [`tinydaws-followup.md`](tinydaws-followup.md) — the rack design.
- [`product-notes.md`](product-notes.md) + [`product-notes-followup.md`](product-notes-followup.md)
  — the commercial/distribution lens.

How to read it: **Tier 0** = cheap + clearly worth doing now. **Tier 1** = higher-leverage infra
(bigger, but unblocks parallel work and the agentic loop). **Tier 2** = gated — don't build until a
real trigger fires (the "sketch first" rule from `product-notes.md`). Then **maker decisions** and
the **cart-polish backlog**. Check items off here; promote anything substantial into its own doc/ADR
once it ships (the journal's own Promotion Principle).

Adjacency note: several wished-for tools overlap existing ones — **extend, don't duplicate**:
`cart-index.js` (computed technique index), `build-compendium.js`, `lint-carts.js` (validates
index.json tags), `cart-status.js` (what's out of date), `lint-docs.js` (doc cross-refs),
`build-site.js` (gallery), `cart-dupes.js` (duplication finder). None of the six journal tools exist
by name yet (verified 2026-06-29); index.json is still the central hand-maintained registry.

---

## Tier 0 — cheap, high-leverage, do now

- [ ] **Save/share codec — `lz-string` URL + seed codes.** One serialize/compress/Base64 helper that
  turns a cart's state into a `?song=…` (or `?state=…`) URL and back. *Why now:* the web gallery has
  no save/share today, it's near-free, and it's **the same machinery tinydaws needs** (the
  seed-as-song-code handoff — see `product-notes-followup.md` §1 + `tinydaws.md` §"seed is a song
  code"). Build the codec once, reuse for share-link *and* save. First step: a tiny JS lib in the
  editor/site layer + an example cart that round-trips its state through a URL. Namespace any
  `localStorage`/IndexedDB keys per-cart (`tinyjam:<cart>:…`) — the gallery is one origin.

- [ ] **`build-field-notes` — index the journal.** Read `field-notes/*.md`, emit `FIELD-NOTES.md`:
  chronological index, related-note graph (from each note's "Related notes"), by status, by
  confidence, syntheses vs observations. *Why:* the journal asked for this itself
  (`tools-we-need.md` #2, README.md) and it's small. Keeps the notebook navigable as it grows.

- [ ] **Rebake stale thumbnails.** The all-carts log notes the thumbnail goes blank for a beat after
  you run+escape a cart — i.e. the baked screenshot is stale/absent. Run `node tools/cart-status.js`
  to list what needs a rebake, then batch the `--run` bakes. (Likely already partly covered by
  `cart-status.js`; this is mostly executing it, not building anything.)

---

## Tier 1 — knowledge infrastructure (unblocks parallel agents + the agentic loop)

- [ ] **Per-cart metadata → generated `index.json` (`build-cart-index`). THE keystone.** Each cart
  owns its metadata (embedded in the `.c` docblock or a sidecar); a tool scans carts and *generates*
  `editor/public/carts/index.json`. *Why this is #1 infra:* it retires the **single worst recurring
  pain in the repo** — the shared-`index.json` multi-agent merge conflict that CLAUDE.md devotes a
  whole hazard (#2) + a stash/splice dance to. Field notes `013` + `tools-we-need.md` #4 + the
  all-carts log all independently flag it. First step: decide where cart metadata lives (the `.c`
  docblock is most in-keeping — carts already embed `de:source`/`de:settings`), write the generator,
  make `lint-carts.js`'s checks run against the per-cart source, regenerate index.json identically.
  *Risk to manage:* the generated file must stay byte-stable so the editor/launcher don't churn.

- [ ] **`cart-meta-check` — validate per-cart metadata.** Pairs with the above: required fields,
  valid `status`, missing descriptions, broken `replaces`/`successor`/`related`, retired-without-
  successor, duplicate/inconsistent tags. Probably an extension of `lint-carts.js` rather than a new
  tool. (`tools-we-need.md` #3.)

- [ ] **Cart lifecycle + curated views.** Add a single `status` field (`active` / `showcase` /
  `retired` / `archive` / `hidden` — *no* `status` field exists today) + relationships
  (`replaces`/`successor`/`related`, "stronger than tags"). Then **generate** curated collections
  ("Best of DreamEngine", Games, Sound Toys, Living Worlds, Arcade Homages, Technical Labs, Retired)
  rather than one flat list — feed `build-site.js`. Retiring a cart points visitors at its successor.
  *Why:* the shelf is ~200+ carts; a flat list no longer serves anyone (`003-curation`,
  `about tags.md`). Sequence after per-cart metadata exists (it's where `status` should live).

- [ ] **`build-context` — task-specific agent briefings.** `build-context roads` / `cart:coaster` /
  `task:retire-carts` → assembles the relevant design docs + ADRs + related carts + field notes +
  pitfalls + verification steps into one focused briefing. *Why:* "don't ask agents to read
  everything; tell them what matters" (`002-context-assembly`, `distillator.md`, `tools-we-need.md`
  #1). Bigger; do after the metadata + frontmatter give it structured inputs to assemble.

- [ ] **Finish the AI-friendly frontmatter convention.** `title` / `summary` / `concepts` / `status`
  YAML on design docs (partially adopted — some `docs/design/*.md` already have `---` blocks; make it
  consistent and documented). Powers `build-context` + `stale-doc-check` + search. Opt-in, fill in
  when a doc is touched (`ai-friendly-frontmatter.md`, `012-self-describing-artifacts`).

- [ ] **`stale-doc-check`.** Frontmatter `depends_on: [files]` + `updated:` → flag docs whose
  dependencies changed since. Extends `lint-docs.js` (which already does cross-refs).
  (`tools-we-need.md` #5.)

- [ ] **`promote-candidate`.** Scan carts/docs for repeated patterns ("state-machine in 19 carts",
  "graph routing in 5") to surface what's earned promotion into the engine/API. Adjacent to
  `cart-index.js` + `cart-dupes.js` — likely a report layered on them. (`tools-we-need.md` #6.)

- [ ] **Tool self-description headers.** `@tool / @summary / @category / @when / @safe` comment block
  per `tools/` script; a future generator emits a tool index. CLAUDE.md already enforces a one-line
  pointer per tool, so this formalizes what's half-there. (`011-tool-discovery`.)

---

## Tier 2 — the audio product (GATED: keep "sketch first")

Per `product-notes.md`: ship the rebirth-house pilot on the free web gallery and watch before
building any of this. Each item already has an un-park trigger in `product-notes.md` / `-followup`.
Listed so the plan exists, **not** so it gets built now.

- [ ] **JSON diff export tier (+ A/B-as-RLHF).** Highest-leverage *product-adjacent* item because dev
  here is agentic: a "developer export" button dumping the lane before/after diff (A/B-choice diff is
  the free first cut) to feed coding agents tuning the radios. A 4th export tier next to
  WAV/song-code/song.h. (`tinydaws-followup.md` §2.) *This one is arguably Tier-1-ish* — it serves
  engine development, not just the product. Pull it forward if the rack pilot lands.
- [ ] **Live bias knobs** (Complexity/Mood/Energy drive the generator's next bar) — fold into the rack
  control layout once the pilot generator is lane-native; keep seed-reproducible. (`-followup` §1.)
- [ ] **Visualizer + 9:16 social video export** (event-driven Game&Watch dancer; reuse
  `sprite-draw.js` + `make-gif.js`). Shared-chassis chrome. (`-followup` §3.)
- [ ] **Native iOS / AUv3 / StoreKit 2 / monetization / Module-of-Week / Fastlane pipeline** — the
  whole `product-notes-followup.md` §2–§7 stack. Gated on "a rack demonstrably holds attention." When
  it fires, that doc is the build plan.

---

## Maker decisions (not mine to make)

- [ ] **Naming: `Tinyjam` vs `tinydaws`.** Two brainstorms + you landed on *Tinyjam* (app singular,
  *tinyjams* = user creations, *Tinyjam: <Genre>* modules); the repo's working name is *tinydaws*.
  Pick one before it's baked into a public product. (`product-notes-followup.md` §0.)
- [ ] **Verify the market numbers** before citing anywhere ($150M / 5M users / Apple cut / free-tier
  limits are LLM-supplied). (`product-notes-followup.md` §10.)
- [ ] **Touch-input policy.** The all-carts log keeps asking for onscreen joystick/d-pad — decide the
  shared answer (see cart-polish below) since it's a per-cart × global call.

---

## Cart-polish backlog (the all-carts log, made systemic)

`field-notes/0000000-cgecking-all-carts.md` is the authoritative per-cart punch-list. It's mostly
*polish*, but the same fixes recur across many carts — handle those **once, systemically**, then
sweep:

- [ ] **Touch movement affordance.** A shared onscreen **joystick / d-pad** widget (joystick for free
  movement, d-pad for grid games), opt-in per cart **in code** (so a compiled cart on a touch device
  shows it) — the log explicitly wants this not via settings. Then mark each cart's ideal input type.
  Affects: boids, doom-fire, defender, galaga, vampire-survivors, rogue, build-a-world, enemies,
  catch-the-star, columns/puyo/dr-mario, rollswarm, sand-burrow, jumpstar, tower-defense, +more.
- [ ] **"Cute pixel buttons" widget** at larger sizes (24×24 / 32×32) — recurring ask. Decide whether
  it's a new shared `ui.h` helper or per-cart; the log floats both. Replaces text-label buttons that
  don't work on mouse/touch across dozens of carts.
- [ ] **Drag-interaction capture rule.** Starting a drag on a knob/XY-pad must *keep* that target even
  if the finger slides over another widget (fixed in **dubdesk**; apply to **all** musical carts with
  many knobs/sliders/pads — more-note-bass, etc.). Candidate for a lint or a shared `ui.h` behavior.
- [ ] **Adventure-game z-ordering.** Sort actors by y so the character draws in front of/behind props
  — monkey-island, leisure-suit-larry, zak-mckracken, escape-the-cave (the recurring "z layering" +
  walk-behind requests).
- [ ] **A nicer shared end-state.** The "fade to black / fade to half" at win/lose recurs and reads as
  unfinished — define one good shared end-screen treatment and apply it.
- [ ] **Text-overflow sweep.** Many carts overflow / want a smaller font (civ-tiny, puyo, dr-mario,
  connect-4, monkey-island, larry, football-manager, sokoban, papers-please, xcom, composer…).
- [ ] **Mouse support for the 3D + label-button carts.** wheel-zoom + drag-rotate + a toggle for
  auto-rotate, and pinch-zoom seams for touch — textured3d, solid3d, 3d-wireframe; plus clickable
  text labels across juice/particles/effects/etc.
- [ ] **Gamepad pass.** The repo started gamepad-first (arrows + A/B); re-verify carts still work on a
  controller and that the 2-onscreen-button carts also accept A/B. (Start by actually wiring + testing
  a controller — the log's "press a button" note.)
- [ ] Per-cart one-offs (sand-burrow teleport bug, pacman level-1 layout, l-system grow direction,
  outrun flicker, smooch double-render dancer, etc.) — work straight from the log, cart by cart.

Ordering for the sweep: ship the **touch-joystick** + **pixel-button** + **drag-capture** shared
pieces first (they unlock the most carts at once), tie a per-cart **`status` + ideal-touch-input +
resolution + `todo`** marker into the per-cart metadata work above, then grind the log.

---

## Suggested first three moves

1. **Save/share codec** (Tier 0) — cheapest win, and it's the tinydaws seed-code you'll need anyway.
2. **Per-cart metadata → generated index.json** (Tier 1 keystone) — retires the index.json conflict
   that taxes every multi-agent session, and it's where `status`/touch-input/resolution markers live.
3. **`build-field-notes`** (Tier 0) — small, and keeps this whole knowledge layer navigable.
