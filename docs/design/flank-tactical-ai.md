# Flank — tactical enemy AI notes

Living design notes for the **`flank`** cart (`tools/carts/flank.c`): a real-time
top-down firefight built as a **prototype enemy brain for `sloop`** (the GTA1 /
Hotline-Miami project). The goal is a squad that *isn't dumb* — it flanks, takes
cover, communicates, suppresses, and hunts you when you sneak. Everything here is
deliberately **emergent from a few interacting rules** rather than scripted
special-cases (per the owner's standing preference).

It reuses the session's roguelike toolkit: Dijkstra flow fields (`dijkstra` cart),
line-of-sight (`sightlines`), and influence/heat maps. It's the designated **combat /
enemy-AI prototype for sloop's v2** ([`sloop.md`](sloop.md) — combat is cut from v1).

## Building blocks (where each lives in `flank.c`)

| System | Code | What it does |
|---|---|---|
| **Line of sight** | `los_px()` | FULL cover blocks sight; you see *over* low cover. Used for detection + shooting. |
| **Flow field** | `reflow()` / `relax()` / `flow[][]` | Dijkstra flood from your last-known cell; alarmed enemies roll downhill to approach **around** cover. |
| **Danger heatmap** | `compute_heat()` / `heat[][]` | Your aim projects a ~55° **danger cone**; enemies score firing spots to AVOID it → they flank. (Toggle `H`.) |
| **Comms blackboard** | `known`, `kx/ky`, `kage` | One sighting broadcasts your position to the whole squad; decays (`kage`) so contact goes stale. |
| **Enemy types** | `TY[]` table, `EType` | Per-type range / cover-weight / heat-fear / speed / strafe / spread. One row = one archetype's feel. |
| **Cover model** | `cell[][]` (0/1/2), `opaquepx` vs `wallpx`, `low_facing()` | FULL (blocks move+LOS+bullets) vs LOW/crate (blocks move only; shoot over; ~50% to absorb a shot from the side it faces). |
| **Suppression** | camper branch in `E_ENGAGE`, `pl.pinned` | Campers anchor + spray to PIN you (slows you, shakes your aim). Their reload is your window. |
| **Graded alertness** | `e->alert` (0..100), `e->invx/invy`, states | calm→suspicious→alarmed. Stimuli raise it, time decays it. Suspicious = investigate + give up. |
| **Noise** | `noise_at()` | A sound bumps nearby enemies' alert and points them at it. Loud gun = big noise; **knife = none**. |
| **Stealth / fog** | `pl.sneak`, `VIS_R`, `e->everseen/lsx/lsy`, `reveal` | You only see enemies in your LOS/vision; unseen → last-seen ghost. Sneak (Shift) cuts their sight/hearing. |
| **Player weapons** | `player_update()` | LMB = loud gun (makes noise), RMB = silent knife (close kill, no noise). |
| **Difficulty panel** | `O` key, `ui.h` widgets, `sl_*` / `d_*` + `recompute_difficulty()` | easy/normal/hard presets + live sliders → runtime multipliers applied each frame (damage/fire-rate/accuracy/sight/suppression). Count applies on apply+restart. Sim pauses while open. |
| **Healing tiers** | `sl_heal` → `d_packs`/`d_regen`, `pack[]` | one slider, three tiers: hard = none · normal = green health packs (35hp, respawn) · easy = packs + slow out-of-combat regen. |

### Enemy states (alertness-driven)
`E_PATROL` (calm, drift) · `E_SUSPECT` (investigate `inv`, then give up) · `E_HUNT`
(alarmed, no LOS — flow toward you / a noise) · `E_ENGAGE` (alarmed + LOS — type
firing stance, or camper suppression). Glyph = type (`R`/`C`/`F`), colour = state;
`?` over suspicious, `!` over alarmed.

## Behaviours — done ✓ / open ☐

- ✓ Flanking (avoid the aim cone) · cover-seeking · spread (no kill-funnel)
- ✓ Communication (shared blackboard, "contact!" callout)
- ✓ Flow-field approach around cover
- ✓ Three types: rusher / camper / flanker
- ✓ Full + low (XCOM-style) cover, with absorb
- ✓ Suppression → emergent fire-and-maneuver (camper pins, others move)
- ✓ Graded alertness (calm/suspicious/alarmed) + investigate-then-give-up
- ✓ Noise-driven attention (loud gun draws; silent knife doesn't)
- ✓ Stealth: sneak, player fog-of-war, last-seen ghosts, last-known investigation
- ✓ Difficulty panel (`ui.h`): presets + live sliders for all the knobs
- ✓ Healing tiers: hard none / normal packs / easy packs+regen
- ☐ **Weapon abstraction** (gun / melee / grenade / sniper-shot) — prerequisite for brawl/knife/dasher scenarios
- ☐ **Scenario presets** — composition + loadout + arena, on top of the difficulty panel (see below)
- ☐ **Ammo economy** — finite player magazine + reserve; reload = a beat of vulnerability + small noise; out of reserve → knife-only (an emergent stealth scenario). Generalize the camper's reload to *every* type (a reloading enemy is briefly out of the fight, so staggered reloads open readable push windows — no new AI). Ammo crates reuse the `pack[]` respawn machinery; one more `O`-panel tier slider (`sl_ammo`) like healing. Leans straight into the loud-vs-quiet axis: scarce bullets *force* knife play, so "knife the isolated one, save rounds for the group" emerges as fire discipline.
- ☐ **Per-enemy skill rating** — one `skill` float (per enemy, or per type + jitter) scaling things already in the engine: reaction lag on first LOS, aim spread, `alert` climb rate, how well they read your last-seen route. Green conscripts panic-spray and lose you; veterans pre-aim your cover exit. No new systems — a multiplier on existing detection/spread/alert rates → a whole "green vs veteran" squad from one number. Pairs with morale (low-skill breaks first) and slots into the difficulty panel as a slider.
- ☐ **Reaction time** (the headline feel-lever) — a short delay between "I have LOS" and "I fire," scaled by skill + alertness. This is what makes peeking / baiting / quick-peek-retreat work, and it's the main *player-skill expression*: veterans near-zero lag, a startled patroller gives a free half-second. Tiny code, big difference. (Overlaps overwatch — overwatch is the zero-lag held-arc case.)
- ☐ **Lean / quick-peek** (player skill ceiling) — a lean-from-cover action (tap toward a wall) exposes only your aim, shrinking your hittable profile; rewards positioning over twitch. Reuses `low_facing()` geometry.
- ☐ **Aggression / discipline trait** — orthogonal to skill: some enemies over-push (chase when smart not to), some hold. Feeds morale and makes a squad feel like individuals, not clones — just another `TY[]` row-or-jitter.
- ☐ **Overwatch** — hold an arc, instant reaction shot when you cross LOS (best on campers / chokepoints; smarter than XCOM by watching your last-seen route)
- ☐ **Grenades + cover destruction** — flush a turtling player (full→low→gone)
- ☐ **More archetypes** — melee dasher (dog), shotgunner, sniper (laser telegraph), shielded heavy, grenadier, leader/officer
- ☐ **Morale / panic** — squad breaks when decimated / leader dies; surrender/retreat
- ☐ **Coordinated search** — fan out to sweep rooms instead of converging on one cell
- ☐ **Peacetime → combat posture** (a squad-level state above per-enemy alertness) — the room *starts unaware you're a threat at all*: enemies run normal routines (patrol routes, idle, chatter, maybe non-combat tasks) at a relaxed "peacetime" alert floor with sight/hearing dialled down, weapons not up. The first hard stimulus — a body found, a shot heard, a confirmed sighting that sticks — **flips the whole squad to "combat time"**: alert floor jumps, sight/hearing open up, they draw and start the flow-field/flank engine we already have. This is the framing that makes the opening of a fight a *stealth phase*: while it's peacetime you can slip around, pick position, knife the isolated one (no noise) — and the moment you're loud, or they find a corpse, normal time becomes fighting time and the squad we built switches on. Builds directly on the existing alertness model (it's a global floor + a one-way latch on top of the per-enemy `alert`), the comms blackboard (the flip broadcasts), and **evidence discovery / noise** as the triggers. Reuses the recent roguelike experiments' calm-world feel for the peacetime layer. One more scenario knob: how alert the room *starts* (sleepy patrol ↔ already-twitchy).
- ☐ **Evidence discovery** — spotting a corpse raises the alarm (a primary trigger for the peacetime→combat flip above)
- ☐ **Reinforcements / alarm** — an alerted enemy triggers more

> Explicit "backpedal/kite" was **declined** — it already emerges from range-keeping.
> Prefer the emergent version; only special-case what genuinely can't emerge.

## Fight engine — scenarios (the direction)

The three threads — "tune flank into a fight engine", "do grenades/sniper get their
own strategies?", and "emergent is king" — all point at the same thing.

**The difficulty panel is the first 7 knobs of exactly that.** A *scenario* is just a
bigger preset that also sets **composition + loadout + arena**, not only the combat
numbers:

| Scenario | What the preset sets |
|---|---|
| **Gunfight** (current) | ranged types, cover-heavy arena, suppression on |
| **Street brawl** | everyone melee, no guns, tight arena, fast, no cover-camping |
| **Sneaky murder** | few *unaware* patrollers, sight way down, packs off, you have the knife |
| **Stealth** | sparse guards, long patrol routes, alarm/reinforcements matter |
| **Knife fight** | both sides melee-only, open floor, no ranged |

Crucially — the **"emergent is king"** part — **none of those are new AI.** They're the
*same* alertness + flow-field + flank-scoring engine with different **parameters and a
weapon flag**. A street brawl is "rushers, weapon=melee, sight short, arena tight."
Stealth is "count low, sight low, all start PATROL." The fight *flavor* emerges from
the knobs; we just expose more knobs.

**Grenades / sniper — own strategies?** Yes, but the honest version: each is **one more
row in the `TY[]` table**, not a bespoke script. The existing scoring already gives them
strategy for free:

- **Sniper** = `range` huge + `speed` low + `coverW` high → it *already* hangs back at
  max range and holds. The only genuinely-new bit is the **telegraph** (a laser line
  before the shot) — a tiny mechanic, not a brain.
- **Grenadier** = a type whose "fire" lobs an arcing AoE instead of a bullet → the
  **flush** behavior emerges because it already prefers line-of-sight on your position;
  the grenade just makes camping behind cover bad. New mechanic = the projectile + cover
  damage; the *decision* to use it is the same engage logic.
- **Melee dasher** = rusher with weapon=melee → it already charges (low range); swap
  "shoot bullet" for "swing when adjacent."

So the rule holds: **only genuinely new *mechanics*** (grenade arc, laser telegraph,
melee swing, shield) need code; **the *strategies* emerge** from the shared scoring + a
params row. That keeps it one engine, not twelve scripts — which is the whole point.

**Build path** (when wanted): (1) a **weapon abstraction** (gun / melee / grenade /
sniper-shot) since several scenarios need melee, then (2) a **scenario preset** layer on
top of the difficulty panel. Both are small because the engine's already doing the thinking.

## Tuning knobs
- **Difficulty (live)** — the `O` panel; `recompute_difficulty()` maps the `sl_*` sliders to
  the `d_*` multipliers, `set_preset()` holds easy/normal/hard. Most numbers below are now
  *multiplied* by these at runtime.
- **Archetype feel** — the `TY[]` table (one row per type): hp/mag/gap/range/coverW/heatW/speed/strafe/spread.
- **Detection** — `132 * d_sight * ss` (sight) and `48 * sh` (hearing) in `enemy_update`; sneak factors `ss/sh`.
- **Alertness** — thresholds 30 (suspect) / 70 (alarm), decay `0.22`/frame, `see=100` / `heard+=3` rates.
- **Noise** — gun `noise_at(...,260,70)` (carries across most of the room; nearby enemies investigate on one shot, farther ones after a few); knife makes none.
- **Suppression** — camper `gap=7*d_gap`, spread `30*d_spread`, reload `100`; player pin accrues `*d_supp`,
  slow `1-0.45*pinned`, aim spread `+pinned*11`.
- **Healing** — `sl_heal` tiers (`d_packs`/`d_regen`); pack heal `+35`, respawn cd `720`; regen `tick%14` after `calm>120`.
- **Cover absorb** — the `rnd(2)` (50%) in the two bullet-hit blocks (use `rnd(3)` for ~33%, XCOM half-cover).
- **Vision** — `VIS_R` (player fog radius).

## Porting to sloop
The systems are modular and engine-agnostic (plain grid + float positions). Liftable
nearly verbatim: `noise_at`, the alertness model, `low_facing`/cover tests, the
`E_ENGAGE` flank-scoring loop (the "personality"), and the flow-field `reflow`/`relax`.
The `TY[]` table is where you'd express sloop's actual enemy roster.
