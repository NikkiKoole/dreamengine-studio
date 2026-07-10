# Cart specs (per-game prompts)

Each file here is the **per-game half** of a cartridge-authoring prompt. To build a
cart, paste into a fresh context:

1. the reusable brief — the fenced block in
   [`cart-authoring-prompt.md`](cart-authoring-prompt.md)
   (it ends with a `# The game you're building` placeholder), then
2. the contents of the spec file below it.

```bash
# assemble brief + a spec into one prompt (run from the repo root)
{ sed -n '/^```markdown$/,/^```$/p' docs/guides/cart-authoring-prompt.md | sed '1d;$d'; \
  echo; cat docs/guides/cart-specs/druglord.md; } > /tmp/prompt.md
```

These are design specs, not code — the downstream context reads the engine source and
writes the actual `tools/carts/<name>.c` (+ `.cart.js`).

### Building many in parallel

The whole point of the spec/brief split is to fan a batch out across many agents at once.
Two ways, same shape — every agent **builds its cart fully but skips the two shared-file
steps that race the others** (the `--run` thumbnail bake + the `index.json` edit), and a
**coordinator does those serially** at the end to avoid clobbering the shared `build/`
dir and `index.json`.

- **By hand (copy-paste):** paste [`AGENT-MESSAGE.md`](AGENT-MESSAGE.md) into each session
  alongside its `brief + spec`. It instructs the session to build, run only the
  parallel-safe CREATE step, and end with just its `index.json` JSON blob. You play
  coordinator: bake each thumbnail (`make-cart.js --run`) and merge the blobs.

- **Orchestrated (recommended — what built batch 2):** drive it from a single
  **Workflow** instead. One agent per game, fanned out with `parallel()`, each doing the
  exact `AGENT-MESSAGE.md` job — except the "end with a JSON blob" convention becomes a
  **structured-output schema** (`{name, title, description, file, kind, genre?, teaches[],
  lineage}`) the workflow validates, so there's no parsing. Include `teaches`/`lineage`
  in the schema (teaches from the controlled vocab in `tools/teaches-vocab.js`) so the
  ★ techniques compendium is populated at build time, not backfilled later. The script
  body holds the game list (name + scope +
  locked slice + which exemplar carts to read) and assembles each agent's prompt from the
  brief. When the fan-out returns, the **main loop becomes the coordinator**: bake each
  `--run` thumbnail serially (fixing any compile error — this is where they surface, since
  CREATE doesn't compile) and merge the validated entries into `index.json`.

  Why the bake stays serial in the main loop, not in the workflow: `--run` fights over the
  shared `build/` dir, and compile-error fixing wants real read/edit/retry reasoning. (If
  you ever want it *fully* hands-off, give each build agent `isolation: 'worktree'` so it
  can `--run` in its own checkout — more setup per agent, only worth it at large scale.)

  Batch 2 (the 16 in [`BATCH-2.md`](BATCH-2.md)) was built this way: 16 agents in one
  workflow wrote spec + cart + CREATE, then the bake + merge ran serially. All 16 compiled
  on the first bake.

## Specs

Scope flags: 🟢 well-scoped · 🟡 sliced from a bigger game · 🔴 huge, cut hard.
Several "build on the existing cart" — read that cart, then surpass/differentiate.

| Spec | Game | Scope | Headline / locked slice |
|---|---|---|---|
| `druglord.md` | Druglord (Drugwars trading) | 🟢 | economy loop + per-district art |
| [`sensible-soccer.md`](cart-specs/sensible-soccer.md) | Sensible Soccer (top-down football) | 🟡 | after-touch ball curve + team recolor |
| [`operation-wolf.md`](cart-specs/operation-wolf.md) | Operation Wolf (light-gun shooter) | 🟢 | mouse light-gun + shoot-to-reload |
| [`final-fight.md`](cart-specs/final-fight.md) | Final Fight (belt brawler) | 🟢 | reusable brawler engine; 2 stages + boss |
| [`river-city.md`](cart-specs/river-city.md) | River City (brawler RPG) | 🟡 | builds on final-fight + shops/stats/zones |
| [`cannon-fodder.md`](cart-specs/cannon-fodder.md) | Cannon Fodder (squad run-gun) | 🟢 | mouse squad + permadeath + explosions |
| `outrun.md` | OutRun (pseudo-3D racer) | 🟡 | branching road forks + biomes; extends `racer` |
| [`pod-racer.md`](cart-specs/pod-racer.md) | Pod Racer (textured-3D racer) | 🔴 | tritex canyon + boost/heat; one track |
| `excitebike.md` | Excitebike (+ level editor) | 🟢 | landing-angle physics + mouse track builder |
| [`advance-wars.md`](cart-specs/advance-wars.md) | Advance Wars (turn tactics) | 🟡 | damage table + terrain + capture; vs `fourx` |
| `xcom.md` | XCOM (tactical battle) | 🔴 | battle + roster only; procedural map, cover/aim |
| `dune.md` | Dune (RTS base) | 🟡 | spice→credits→build loop + light defense |
| [`pizza-tycoon.md`](cart-specs/pizza-tycoon.md) | Pizza Tycoon (business sim) | 🔴 | buy shops + recipe/price + mafia cards |
| [`trade-winds.md`](cart-specs/trade-winds.md) | Trade Winds (sail trading) | 🟡 | reuses Druglord economy + storms/pirates |
| [`gta-turf.md`](cart-specs/gta-turf.md) | GTA turf war | 🟡 | territory-capture sandbox; extends `gta` |
| `doom.md` | Doom (raycaster FPS) | 🟡 | billboard demons + weapons; extends `raycaster` |
| [`blade-runner.md`](cart-specs/blade-runner.md) | Blade Runner (noir adventure) | 🟡 | scenes/hotspots/dialogue; atmosphere-first |
| [`mass-effect.md`](cart-specs/mass-effect.md) | Mass Effect (sci-fi action) | 🔴 | hub + planet select + 1 mission + dialogue wheel |
| [`heroes-of-might-magic.md`](cart-specs/heroes-of-might-magic.md) | Heroes of Might & Magic | 🔴 | adventure map + STACK battles + town; reuses `advancewars` |
| [`hotline-miami.md`](cart-specs/hotline-miami.md) | Hotline Miami (top-down action) | 🟡 | one-hit-kill + instant restart; synthwave bed |
| [`dungeon-keeper.md`](cart-specs/dungeon-keeper.md) | Dungeon Keeper (build/defend sim) | 🟡 | dig/claim + slap-hand + invasions; reuses `dune` |
| [`mario-paint-sound.md`](cart-specs/mario-paint-sound.md) | Mario Paint Sound (TOOL: composer) | 🟢 | melodic stamp grid + looping playhead |
| [`deluxe-paint.md`](cart-specs/deluxe-paint.md) | Deluxe Paint (TOOL: paint program) | 🟡 | fillp patterns + dithered gradients + color cycling |
| [`sky-strike.md`](cart-specs/sky-strike.md) | Sky Strike (top-down airplane shmup) | 🟢 | weapon-upgrade tiers + scrolling tile world; vs `galaga`/`xevious` |
| `pinball.md` | Pinball (one table) | 🟢 | segment-reflection ball physics + flippers/multiball |
| [`smooch-lounge.md`](cart-specs/smooch-lounge.md) | Smooch Lounge (saucy cabaret rhythm) | 🟢 | lane-rhythm off beat() + flirt-meter; PG-13 innuendo |
| [`skeleton-animator.md`](cart-specs/skeleton-animator.md) | Bones (TOOL: stick-figure FK animator) | 🟢 | 18-bone FK skeleton + sequencer grid; hex cells + drag-pose |

**Reuse clusters** (build within a cluster so later specs point at the cart you just made):
brawler (`final-fight` → `river-city`); economy (`druglord` → `trade-winds`, `pizza-tycoon`);
crowd/team-recolor (`sensible-soccer`, `gta-turf`, `cannon-fodder`); pseudo-3D
(`outrun`, `pod-racer`); tactics (`advance-wars`, `xcom`).

*Batch indexes: [`BATCH-2.md`](BATCH-2.md) · [`BATCH-3.md`](BATCH-3.md).*
