# The game you're building: FARM

## Premise
A pocket Harvest-Moon **day loop** on a single screen. You walk a small farm plot,
equip a tool, and work the soil: **till** a grass tile into furrows, **plant** a seed,
**water** it. Watered crops climb one **growth stage** each morning; once ripe you
**harvest** them into your bag, carry them to the **sell bin** for cash, and spend that
cash on more **seeds** at the shop. Every action drains an **energy** bar and a
**day clock** sweeps overhead — **sleep** in the bed to bank your growth, refill
energy, and roll to the next dawn. It's gentle, legible, and loops cleanly. NO NPCs,
mining, festivals, or seasons — just the satisfying till → plant → water → sleep →
harvest → sell → buy rhythm.

## The locked slice (build exactly this)
- A `GW×GH` tile grid of farm plot: each cell is grass, tilled soil, or a planted crop
  (growth stage 0–3). Three edge **fixtures**: BED, sell BIN, seed SHOP.
- **Tools** selected by number key: HOE (till grass / harvest ripe), SEED (plant on
  tilled soil, consumes a seed packet), watering CAN (wet a planted tile).
- **Growth**: on a `sleep` morning every *watered* planted tile advances one stage and
  all tiles dry out. Stage 3 = ripe (a bobbing red crop).
- **Energy**: each action costs energy; at 0 you can still walk but not act, and an
  un-slept day rollover (clock runs out) makes you **pass out** (half energy, no growth).
- **Economy**: harvest → BAG; sell whole bag at the BIN (+5g each); buy a seed at the
  SHOP (−8g). Start with a small stake + a few seeds.
- A win/lose loop by feel: thrive (cash climbs) or stall (out of seeds + out of cash);
  `save_int` records best cash. No hard game-over — it's a sandbox day loop.

## Engine features it leans into (and why)
- **Tile grid from primitives** (no sprite sheet): the plot, soil furrows, multi-stage
  crops, and fixtures are all drawn with `rectfill`/`line`/`circfill`/`ovalfill`. A
  farm's vocabulary is tiny and stage-driven, so primitives are cheaper and crisper
  than 64 sprite slots — the engine-grain move here.
- **`bar()`** for the energy meter; **faced-cell `rect` highlight** (`blink`) so the
  player always knows which tile a tool will hit — the core interaction-clarity trick
  borrowed from `sokoban.c`/`sims.c`.
- **A day-clock + sky tint**: `clockT` drives a `cls` color sweep (morning peach → noon
  blue → dusk purple) and a sun `circfill` arc across the HUD; `fade()` does the
  sleep/pass-out transition. Time-of-day for free.
- **Reactive synth SFX**: a noise *thunk* on tilling, a soft `INSTR_SINE` *hit* on
  watering, a square *pop* on planting, a `strum(CHORD_MAJ)` cash jingle on a sale, and
  a `CHORD_MAJ7` chime at dawn. Each action sounds like itself.
- **Juice at the earned moments**: a floating `+cash` / feedback pop above the farmer,
  a ripe-crop bob (`sin_deg`), a walk bob, and a `shake()` + red flash on passing out.
- **Framerate-independent** movement and clock via `dt()`; state lives in fixed static
  arrays (no heap).

## Controls
- **WASD / arrows** — walk (you face the last direction moved).
- **1 / 2 / 3** — select HOE / SEED / CAN.
- **Z** — use the held tool on the faced tile, or interact with the faced fixture
  (BED = sleep, BIN = sell bag, SHOP = buy a seed).
- **X** — sleep (at the BED) or sell (at the BIN) — convenience.
- **C** — buy a seed when facing the SHOP.
