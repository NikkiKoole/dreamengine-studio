# Flank — tactical enemy AI notes

Living design notes for the **`flank`** cart (`tools/carts/flank.c`): a real-time
top-down firefight built as a **prototype enemy brain for `sloop`** (the GTA1 /
Hotline-Miami project). The goal is a squad that *isn't dumb* — it flanks, takes
cover, communicates, suppresses, and hunts you when you sneak. Everything here is
deliberately **emergent from a few interacting rules** rather than scripted
special-cases (per the owner's standing preference).

It reuses the session's roguelike toolkit: Dijkstra flow fields (`dijkstra` cart),
line-of-sight (`sightlines`), and influence/heat maps.

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
- ☐ **Overwatch** — hold an arc, instant reaction shot when you cross LOS (best on campers / chokepoints; smarter than XCOM by watching your last-seen route)
- ☐ **Grenades + cover destruction** — flush a turtling player (full→low→gone)
- ☐ **More archetypes** — melee dasher (dog), shotgunner, sniper (laser telegraph), shielded heavy, grenadier, leader/officer
- ☐ **Morale / panic** — squad breaks when decimated / leader dies; surrender/retreat
- ☐ **Coordinated search** — fan out to sweep rooms instead of converging on one cell
- ☐ **Evidence discovery** — spotting a corpse raises the alarm
- ☐ **Reinforcements / alarm** — an alerted enemy triggers more

> Explicit "backpedal/kite" was **declined** — it already emerges from range-keeping.
> Prefer the emergent version; only special-case what genuinely can't emerge.

## Tuning knobs
- **Archetype feel** — the `TY[]` table (one row per type).
- **Detection** — `132*ss` (sight) and `48*sh` (hearing) in `enemy_update`; sneak factors `ss/sh`.
- **Alertness** — thresholds 30 (suspect) / 70 (alarm), decay `0.22`/frame, the `see=100` / `heard+=3` rates.
- **Noise** — gun `noise_at(...,155,60)`; knife makes none.
- **Suppression** — camper `gap=5`, spread `26`, reload `85`; player pin slow `1-0.6*pinned`, aim spread `+pinned*16`.
- **Cover absorb** — the `rnd(2)` (50%) in the two bullet-hit blocks (use `rnd(3)` for ~33%, XCOM half-cover).
- **Vision** — `VIS_R` (player fog radius).

## Porting to sloop
The systems are modular and engine-agnostic (plain grid + float positions). Liftable
nearly verbatim: `noise_at`, the alertness model, `low_facing`/cover tests, the
`E_ENGAGE` flank-scoring loop (the "personality"), and the flow-field `reflow`/`relax`.
The `TY[]` table is where you'd express sloop's actual enemy roster.
