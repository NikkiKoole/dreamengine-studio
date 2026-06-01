# The game you're building: ZAK McKRACKEN

## Premise
A SCUMM-style point-and-click adventure — the tabloid-reporter caper that the LucasArts
classic was named for. You're Zak, a writer for the *National Inquisitor*, woken at his
desk by a phone tip and trying to get out the door and onto a flight. The headline
deliverable is the **scene engine** itself: a verb bar, clickable hotspots, an inventory
strip, walk-to-floor navigation, and **one item-combination puzzle** that gates progress
between hand-drawn rooms. This is the base engine Larry and Monkey Island build on — so
it has to be clean, legible, and atmosphere-first, not mechanic-heavy.

## Slice (locked — build exactly this)
**Three hand-drawn walkable scenes** linked by exits:
1. **APARTMENT** — Zak's messy SF flat: desk, ringing phone, a fishbowl, a locked
   door, an exit-arrow to the hall.
2. **HALLWAY** — landing with a janitor's nook: a stuck vending machine, a fire-axe
   behind glass, the elevator (exit back + exit forward to the cab).
3. **TAXI / STREET** — the cab to the airport: the finale once you hold the plane
   ticket.

**Verb bar** (Look / Use / Take / Talk) along the bottom; **inventory strip** beside it.
**Walk-to:** click a floor point, Zak walks there (framerate-independent). **Hotspots**
highlight on hover with a label; click = run the selected verb against the hotspot.

**The one combination puzzle (gates Scene 1 → onward):** the phone tip says the ticket
is locked in the desk. TAKE the **paperclip** off the desk; the desk drawer is locked.
USE paperclip → bent into a **lockpick** (the combine — item transforms in inventory).
USE lockpick on the drawer → it pops open, revealing the **plane ticket**. The exit to
the taxi only fires once the ticket is in inventory; trying it early gives "not without
my ticket." Picking up the ticket is the win condition for the slice.

## Engine features it leans into (and why)
- **Mouse-first input** (`mouse_*`): the whole UI is point-and-click — verb selection,
  hotspot clicks, floor walk-to, inventory clicks. This is the genre's spine.
- **Hand-drawn scenes via primitives** (`rectfill`/`tri`/`circfill`/`line`): each room
  is composed at draw time so the three scenes stay distinct without spending the
  64-slot sheet — atmosphere by palette and silhouette, not pixel art.
- **`fillp` dither panels** for the verb/inventory bar and dialogue boxes — the SCUMM
  chrome look.
- **Walk path with `lerp`/`dt`** so Zak glides to the clicked point framerate-independent.
- **`pal()`/`fade`** for the scene-transition wipe between rooms.
- **Reactive synth SFX** — a phone ring (square + LFO), a pickup blip, the combine
  "clink," a locked-door buzz, a triumphant strum on the win. Sound where it earns it,
  silence otherwise.
- **Verb→hotspot grammar**: each hotspot answers each of the four verbs with its own
  line, so LOOK/USE/TAKE/TALK all feel authored — the adventure-game voice.

## Data model
- `struct Hotspot { x,y,w,h; const char *name; }` per scene, fixed arrays.
- `static int scene` (0..2), `static float px` walk position + `tx` target.
- `inv[]` bitset of held items (paperclip / lockpick / ticket); the combine swaps
  paperclip→lockpick in place.
- `flags`: `drawer_open`, `has_ticket` gate the exit.
- A small message box (`msg`, timed) shows verb responses; dialogue is one tip call.

## Controls
Mouse: click a verb in the bar to arm it (Look default), then click a hotspot to run
that verb on it; click the floor to walk there; click an inventory item to select it,
then click a hotspot to USE it on the thing. Exits are arrow hotspots. Keyboard: ESC /
SPACE dismisses a message box.

## Lean into / read
`adventure.c` (the scene/hotspot/walk-to/inventory skeleton — closest sibling),
`papers.c` (panel/box UI + state machine), `pet.c` (reactive blips). Skip: combat,
real-time anything, a big world — three tight, moody rooms and one satisfying combine.
