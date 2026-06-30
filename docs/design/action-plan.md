# Action plan — what to build next (synthesized 2026-06-29)

STATUS: LIVING ACTION LIST. Synthesizes the concrete, do-able asks scattered across the
field-notes journal and the three audio-product brainstorms into one prioritized, checkable list.
Sources, each linked at point of use:
- **field-notes/** — the research journal (`about-tags.md`, notes `002`/`003`/`011`/`012`/`013`).
- **design/** — `tools-we-need.md` (the tool backlog), `distillator.md`, `ai-friendly-frontmatter.md` (moved here from field-notes 2026-06-30).
- **`cart-polish-punchlist.md`** (docs root) — the cart-polish log.
- [`tinyjam-racks.md`](tinyjam-racks.md) + [`tinyjam-racks-followup.md`](tinyjam-racks-followup.md) — the rack design.
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

- [ ] **Save/share codec — share songs as URL links.** *Designed, PARKED — ready to build, see
  [`song-codec.md`](song-codec.md).* The channel is the URL (`…/house/?seed=A3F90C12`), not an in-app
  text field — click a link, the cart boots playing that song. Keystone half (makes shared links
  *work*): add an engine `start_seed()` (parse a `--seed` arg in `main()`; the gallery shell reads
  `location.search` → `Module.arguments`), then one line in `radio.h` boots `new_song(pos,
  start_seed())` and all 34 radios inherit link-loading for free. Nicety half: a copy-link button
  (`EM_ASM` clipboard write of the current `?seed=`). Show-code already ships (bare 8-hex `u32`).
  *Decision:* keep the bare `u32` seed now; a `?song=<lz-string blob>` sibling joins **only** when
  the rack ships editing (length/param-distinguished so old links never break). Namespace any
  `localStorage`/IndexedDB keys per-cart (`tinyjam:<cart>:…`) — the gallery is one origin.

- [x] **`build-field-notes` — index the journal.** *Done 2026-06-29 (`tools/build-field-notes.js` →
  `docs/field-notes/FIELD-NOTES.md`, `--check` gates staleness).* Reads every note and derives a
  navigable index: a **lifecycle board** (Hypothesis → Observed → Review → Working Theory →
  Incorporated → Superseded), a **timeline**, the **related-note graph** (from each note's "Related
  notes"), a **concepts** map, the **working-material** list (unnumbered docs kept distinct), and a
  **conformance** report flagging notes that don't match `standard-header.md` (flagged, never
  rewritten — honours the append-only rule). Tolerant of the three metadata styles in the wild.

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
  `about-tags.md`). Sequence after per-cart metadata exists (it's where `status` should live).

- [~] **`build-context` — task-specific agent briefings.** `build-context roads` / `cart:coaster` /
  `task:retire-carts` → assembles the relevant design docs + ADRs + related carts + field notes +
  pitfalls + verification steps into one focused briefing. *Why:* "don't ask agents to read
  everything; tell them what matters" (`002-context-assembly`, `distillator.md`, `tools-we-need.md`
  #1). Bigger; do after the metadata + frontmatter give it structured inputs to assemble.
  **Shipped: the `cart:<name>` slice** (`tools/build-context.js`) — de:meta + todos + a prose
  mention-graph of related carts + every doc/note that names the cart (with the matching line, so
  the differently-named home like `external-data-carts.md` surfaces itself). Needs no doc-frontmatter.
  Still TODO: the `api:` / `task:` / `topic:` subjects, and pulling in ADRs + field notes + the
  matching oracle from `checks-and-oracles.md` once doc-frontmatter (`depends_on`/`updated`) exists.

- [ ] **`stale-doc-check` — catch docs that have drifted from the code (the real prize).** Flag a
  doc when a file it documents changed after the doc's own last update — the one blind spot nothing
  covers today (a doc can name a renamed function and no gate notices). Extends `lint-docs.js` (which
  already does cross-refs). It needs exactly two machine-readable fields per doc — `depends_on:
  [files]` + `updated:` — so **adopt frontmatter only as far as this consumer needs it**, doc by doc,
  when a doc is touched. (`tools-we-need.md` #5, `012-self-describing-artifacts`.)

  *Don't roll out descriptive frontmatter (`title`/`summary`/`concepts`/`status`).* Verified
  2026-06-29: **0 of 127 `docs/design/*.md` carry it** (only field-notes `011`–`013` do; the `---`
  lines in design docs are mid-doc horizontal rules, not frontmatter). And it would mostly *duplicate*
  signal that already works — the `STATUS:` line, the one-line pointers in `docs/README.md`'s layout
  tree, the prose intros, grep — which `ai-friendly-frontmatter.md` itself forbids ("if it can be
  inferred, do not duplicate it; metadata should help discovery, not become maintenance"). The value
  is in *relationships + freshness* (`depends_on`/`updated`), not in re-encoding the title/status. So
  the consumer (`stale-doc-check`, later `build-context`) is the thing worth building; frontmatter is
  just its minimal input.

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
  WAV/song-code/song.h. (`tinyjam-racks-followup.md` §2.) *This one is arguably Tier-1-ish* — it serves
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

- [x] **Naming: settled on `Tinyjam`** (2026-06-29). Two brainstorms landed on *Tinyjam* (app singular,
  *tinyjams* = user creations, *Tinyjam: <Genre>* modules). The legacy working name *tinydaws* was
  retired and the rack docs renamed to `tinyjam-racks.md` / `tinyjam-racks-followup.md`.
  (`product-notes-followup.md` §0.)
- [ ] **Verify the market numbers** before citing anywhere ($150M / 5M users / Apple cut / free-tier
  limits are LLM-supplied). (`product-notes-followup.md` §10.)
- [ ] **Touch-input policy.** The all-carts log keeps asking for onscreen joystick/d-pad — decide the
  shared answer (see cart-polish below) since it's a per-cart × global call.

---

## Cart-polish backlog (the all-carts log, made systemic)

The per-cart punch-list now lives **in the carts** — each cart's `de:meta.todo[]` block (query
with `node tools/cart-todos.js`); [`../cart-polish-punchlist.md`](../cart-polish-punchlist.md) is
now just the pointer + the maker's framing notes. It's mostly *polish*, but the same fixes recur
across many carts — handle those **once, systemically**, then sweep:

- [ ] **Touch movement affordance — the recommended NEXT for iOS (2026-06-29).** A shared onscreen
  **joystick / d-pad** widget (joystick for free movement, d-pad for grid games), opt-in per cart **in
  code** (so a compiled cart on a touch device shows it) — the log explicitly wants this not via
  settings. Then mark each cart's ideal input type. Affects: boids, doom-fire, defender, galaga,
  vampire-survivors, rogue, build-a-world, enemies, catch-the-star, columns/puyo/dr-mario, rollswarm,
  sand-burrow, jumpstar, tower-defense, +more.
  *iOS readiness:* the Phase-2 input seam now exists — the primary finger drives the mouse API
  (mouse carts already play from touch) and `de_key_event` feeds `IsKeyDown/Pressed/Released`; the
  engine already has an on-screen d-pad/buttons (`show_touch_ui`/`TOUCH_CONTROLS`). So this is now
  "wire the existing virtual gamepad + a key overlay into the iOS shell," not new engine work — and
  it's the gate to **raw `key()` carts (WASD, etc.) becoming playable on the phone**. **Full design +
  7-step plan: [`touch-controls.md`](touch-controls.md)** (the deck/rails placement model + the
  `touch_layout()` API); see also [`ios-plan.md`](ios-plan.md) → "Phase 2" follow-ups.
- [ ] **"Cute pixel buttons" retrofit** — replaces text-label buttons that don't work on mouse/touch
  across dozens of carts. **The widget already exists** — `ui.h`'s `ui_spr_button` /
  `ui_spr_button_styled` (sprite face + press/capture/hit-pad/focus/selected styling); size is just
  the `w,h` args (24×24 / 32×32 need no new API). So this is **per-cart retrofit, not a new helper** —
  copy the worked pattern: **`boom`** (two toolbar rows, icons in `boom.cart.js`) and **`gridcity`**
  (brush toolbar, icons in `gridcity.cart.js`, `colorkey(0)` for transparency + hover tooltips).
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
- [ ] **Control-hint overflow (ui-audit, ~47 carts).** A fleet `ui-audit` sweep found that nearly
  every sound-toy/lab cart's bottom one-line control hint (`"drag X · up/dn Y · Z/X oct"`) runs past
  the 320px right edge and is clipped. Systemic fix: a shorter / auto-fit / smaller-font (or wrapped)
  convention for the hint line. Each affected cart carries a soft `ui-audit?:` marker in its
  `de:meta.todo[]` (query: `node tools/cart-todos.js --grep "ui-audit?"`); fixing the convention once
  lets the markers be cleared in a sweep. (Confident per-cart HUD findings carry a plain `ui-audit:`
  prefix instead.)
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

1. **Save/share codec** (Tier 0) — cheapest win, and it's the tinyjam racks seed-code you'll need anyway.
2. **Per-cart metadata → generated index.json** (Tier 1 keystone) — retires the index.json conflict
   that taxes every multi-agent session, and it's where `status`/touch-input/resolution markers live.
3. **`build-field-notes`** (Tier 0) — small, and keeps this whole knowledge layer navigable.
