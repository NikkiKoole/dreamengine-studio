# attract mode — carts that play themselves until you touch them

PROPOSAL (native-first prototype; not built). An arcade cabinet runs a demo loop when
nobody's playing and hands control over the instant you press a button. dreamengine can do
the same: a cart replays a bundled demo track, and on the first real input it drops the
replay and becomes live. The replay machinery already exists (it's the debug harness); the
genuinely new bit is the **handoff**.

## One feature, three payoffs

Ranked by where the value actually lands — which, notably, is *not* where the cost is:

1. **The console + gallery, on their own.** A cart idling into a demo loop *is* the arcade
   soul this project keeps reaching for — and it's an onboarding aid: watch it move, then
   take over. This payoff needs no web, no embed, nothing downstream. It's the reason to do
   the feature.
2. **Curated clip source.** The webm/gif baker (the clips work) already records demo tracks;
   attract mode makes those tracks a *player-facing* artifact, so the demo you author once is
   the demo that's shown — not whatever a debug script happened to do.
3. **Live embeds on the history/gallery pages.** The heaviest payoff and the last — a cart
   embedded as a moving, grabbable thumbnail (attract → "press start" → play). Most of the
   cost (web build, embed plumbing, focus/audio manners) lives here.

> Reweighting the receiving brief: it called the embed the main value. I'd flip that — the
> embed is the *expensive* payoff, and leaning on it to justify the feature couples a small,
> charming engine change to the whole web stack. Payoff 1 stands alone, which is exactly why
> the native-first scope below is the right call, not just a stepping stone.

## Why it's cheap to attempt

The deterministic-replay path in `runtime/studio.c` already does the hard half (verify names
against current `studio.c` before relying on them — the harness internals move):

- `inject_input` (a static flag) swaps hardware reads for injected state: `key()` returns
  `key_inject[k]` when it's set, else `IsKeyDown(k)`.
- `load_script()` parses a human-authored input plan into `replay_ev[]`; `ev_key()` queues
  injected key events; the `--replay`/`--script` flags set `inject_input = true`.
- And the demo track already exists as data: `tools/clips/<cart>/NN-label.{script,beats,rec}`
  — the same tracks the clip baker uses (see the clips convention + `tools/make-gif.js`).
  Attract mode is just a **third consumer** of that track (baker and harness are the others).

**The de-risker:** `studio.c` already has `inp_down(k) → IsKeyDown(k)`, a *raw* hardware read
that ignores `inject_input`. So replay and live-hardware reads already coexist — the handoff
doesn't need a new dual-input system, it needs to *poll the raw read while injecting*.

## The state machine (the one new mechanism)

```
        idle N seconds
  LIVE ───────────────▶ ATTRACT  (inject_input = true; replay the track, loop while idle)
   ▲                       │
   └───────────────────────┘
     first genuine input
     (inp_down / raw IsKeyDown fires)
```

The handoff, concretely: while in ATTRACT, each frame poll the *raw* hardware (`inp_down` /
true `IsKeyDown` / touch) alongside the injected replay. On the first genuine press: stop
advancing `replay_ev`, set `inject_input = false`, and let the cart run live from the current
state. Optionally a "DEMO — PRESS START" overlay that clears on takeover. True arcade
behaviour also re-enters ATTRACT after the player goes idle again (LIVE → idle → ATTRACT) —
and that re-entry is where the reset-to-boot wiring is needed (see the hard parts).

## Where the demo track lives — two homes, one source

The attract track and the video-making tracks are the *same authored data*, stored for two
different needs (this is the load-bearing decision):

- **Authored source → `tools/clips/<cart>/NN-label.{script,beats,rec}`.** One shared home:
  it's the clip baker's input (`make-gif.js`) *and* the debug harness's input. `sloop`'s
  `tools/clips/sloop/01-autodrive.script` is the first committed track — the native
  prototype's repro home.
- **The attract track → embedded *inside* the cart.** A cart should contain everything: hand
  someone the `.cart.png` and the demo comes with it, no sibling file to lose, no fetch in
  the web build. So the bake step (`make-cart.js`) embeds the designated attract track as a
  new zTXt chunk — `de:demo`, beside the existing `de:source` / `de:sprites` (/ `de:map` /
  `de:settings`) — and it compiles into the web build like the rest of the cart data.

**Single source, embedded copy at bake time — never two authored copies that can drift.** The
*rendered* clips (`editor/public/clips/<cart>/*.webm`) stay external: they're output, not
something the cart carries. So: video tracks live in the folder; the attract demo lives in
the cart.

## Design sketch

1. **The track ships inside the cart.** The attract demo is embedded as a `de:demo` zTXt
   chunk (and compiled into the web build) — self-contained, no sibling fetch. Authored once
   in `tools/clips/<cart>/`; the bake step embeds the chosen one. (See above.)
2. **Enter on idle.** On load / after N idle seconds, `load_script()` the track and replay it
   with a **pinned seed** (see below).
3. **The handoff** (the new part) — poll raw input each frame; first genuine press flips
   `inject_input` off and the cart continues live.
4. **Loop** the track while still idle; re-arm the idle timer after takeover.

## The hard parts (honest)

- **The handoff is the real work.** Today replay is sealed — it owns input and runs to
  completion. Attract mode needs the injected replay and a raw-hardware read to coexist for
  the takeover frame. `inp_down` makes that *possible*; wiring it into a clean
  enter/replay/handoff/re-idle loop is the actual change. Everything else is plumbing.
- **Determinism needs a seed pin.** A track recorded at seed S only replays faithfully at
  seed S — procedural carts (sloop's world, roadnet) especially. The demo must pin its seed.
- **The entry point: reset to a clean boot, then replay (later).** A demo recorded from the
  cart's start only makes sense replayed from the start — you can't drop a from-the-title
  track over a half-played game. So entering attract should re-init the cart to its
  boot/title state (fresh `init()`, cleared `de_state`, pinned seed) and replay `de:demo`
  from frame 0. For a cart that boots to a title screen, that *is* "attract plays from the
  title." Needs an engine-triggerable clean re-init + carts being re-init-safe. **Parked:**
  the native spike can replay once from boot (it's already clean on launch); the
  reset-on-loop / re-enter-attract-after-a-play-session wiring is the next rung up, not part
  of the first handoff prototype.
- **Where the track lives in the web build — resolved: embedded.** The attract track rides in
  the cart data (a `de:demo` zTXt chunk, compiled into the web build), so the cart is
  self-contained and there's nothing to fetch. The new work is the bake-step embed +
  reading it back at runtime; the *video* tracks stay external in `tools/clips/`.
- **Web manners** (embeds only): audio muted by default (autoplay policy + many carts), and
  the demo must not steal scroll/keyboard focus until the user clicks to activate.

## Which carts even get a demo

Not universal, so attract-ability is a per-cart property:

- **Games / toys with input** — the prime case; record a demo track. (`sloop`, platformers.)
- **Generative toys & radios** — already self-playing; "attract mode" is just their normal
  idle, no track needed.
- **Instrument / synth carts** — meaningless without a player; either skip, or a short
  scripted noodle purely for the thumbnail.

## Scope — prove the feel natively first

Don't start on the web. Build the native attract loop behind a flag, in **one cart** —
`sloop` is ideal: it's seeded and already has a committed track at
`tools/clips/sloop/01-autodrive.script`. That isolates the one genuinely new mechanism (the
replay→live handoff) from all the web/embed complexity. Because payoff 1 (the console/gallery
attract loop) is valuable on its own, the native prototype isn't a throwaway spike — it ships
something real, and tells you whether the *feel* justifies the web build before you commit to
that ladder.

## Honest take + sequencing

Worth doing, but the next rung, not the urgent one. The cheapest win — collecting demo tracks
and minting clips — is already in hand via the clips work. Attract mode's standalone value
(console + onboarding) is real and cheap to prototype; its biggest, flashiest value (live
embeds) is also its most expensive. So: a satisfying, self-contained native prototype in one
cart is the low-risk way to find out if the feel earns the rest — without buying the whole
ladder up front.

## Related

- [`input-recording-looper.md`](input-recording-looper.md) — record the player, loop it back
  (one corner of this).
- [`headless-autoplay.md`](headless-autoplay.md) — replay for windowless runs / bug-finding
  (the other corner). Attract mode is the missing third: the same replay, **player-facing**.
- The clips convention + `tools/make-gif.js` — the shared demo-track source (other workstream;
  this note only *consumes* those tracks, doesn't define them).
- [`../guides/debug-harness.md`](../guides/debug-harness.md) — the record/replay/script tooling.
