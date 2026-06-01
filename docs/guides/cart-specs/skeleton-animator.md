# The game you're building: BONES (a pixel-man skeleton animator — a TOOL)

## What it is
A tiny **stick-figure keyframe animator**. The pixel-man is an 18-bone skeleton of 1px
lines; you pose him by setting each bone's **rotation** (1 of 16 directions, 22.5° apart),
and you author motion in a **sequencer grid** — bones down the rows, time across the
columns. Hit play and a playhead sweeps the columns, animating the little guy. It's the
drum machine's grid, but every cell is a *rotation* instead of on/off, with a live
skeleton preview beside it. A creative TOOL: no win/lose, just instantly satisfying to
puppeteer a walk cycle into being.

## Slice (locked) — 🟢  (lean core)
A **bigger side-by-side canvas** (the cart requests ~**384×240, scale 3** via its
`de:settings`): the **sequencer grid** fills the left, a **live character preview** sits
on the right. 18 bone-rows × 16 frame-columns; each cell shows the bone's rotation as a
**single hex digit `0–f`**. A cell **marker** you drive with the arrow keys; rotate the
selected bone CW/CCW; a playhead that loops the frames at an adjustable **FPS**; set the
**loop length** (use fewer than 16); **copy the previous frame** to keep animating from
where you were; **save/load** the whole animation with `save_bytes`. Skip (deliberately
cut for v1): onion-skinning, mirror L↔R, copy/paste arbitrary columns, tweened
in-betweens, editable bone lengths, per-frame root travel.

## The skeleton (forward kinematics)
Hierarchical FK — every bone's stored rotation is **relative to its parent**, so rotating
the torso swings everything attached to it ("attached the way you expect"). Root is the
pivot **between upper & lower torso**, kept **centred & fixed** on screen (pose in place).

```
root (mid-spine pivot)
├─ upper torso ─┬─ neck ─ head                       (head = a small circle, not a line)
│               └─ shoulder (crossbar) ─┬─ L upper arm ─ L lower arm ─ L hand
│                                       └─ R upper arm ─ R lower arm ─ R hand
└─ lower torso ─ hip (crossbar) ─┬─ L upper leg ─ L lower leg ─ L foot
                                 └─ R upper leg ─ R lower leg ─ R foot
```
18 bones / rows: `head, neck, upper torso, lower torso, hip, shoulder` (6 singles) +
`upper arm, lower arm, hand` ×L/R + `upper leg, lower leg, foot` ×L/R. `hip` and
`shoulder` are single rotatable crossbars that both limbs hang off.

**The math (give the implementer the recipe):** walk the tree from the root each frame;
`worldAngle(bone) = worldAngle(parent) + rot[bone]*22.5°` (plus an optional per-bone REST
offset so rotation values read naturally from a default standing pose). Step each bone its
fixed length with `dx()`/`dy()` from its parent joint to get the child joint; draw a 1px
`line()` joint→joint (head is a `circ`/`circfill` at the neck's far end). Angles in
degrees throughout. No skeletal/animation system exists in the engine — this is all your
own arrays + trig, exactly the point of the tool.

## Sequencer grid & preview
- **Grid:** 18 rows (one per bone, short labels in a left gutter — `hd nk ut lt hp sh
  Lua Lla Lha Rua…`), 16 columns (frames). Each tiny cell renders one **hex digit `0–f`**
  for `rot[bone][frame]`. Tint the digit by **body region** (spine / left arm / right arm
  / left leg / right leg) so the grid reads as coloured bands. The **cell marker** is a
  bright box; the **current playhead column** is highlighted (downbeat-style tint every
  4th frame for orientation). Group the row order by region so symmetric limbs sit
  together.
- **Preview:** the assembled pixel-man for the **selected/playing frame**, drawn big and
  centred on the right. Joints as little dots; the **selected bone highlighted** (e.g.
  drawn in white / a contrasting colour) so you always see which line you're turning.

## Input (keys + mouse + drag-pose)
- **Keyboard (sequencer):** arrows move the cell marker (↑↓ = bone, ←→ = frame). `Z`/`X`
  (or `,`/`.`) rotate the selected bone **CCW/CW** in the marker's frame. `C` = copy the
  previous frame's whole column into this one. `SPACE` = play/stop. `[`/`]` = FPS down/up.
  `-`/`=` = shrink/grow the loop length. `S` = save, `L` = load.
- **Mouse:** click a cell to select it; **mouse-wheel** spins the selected bone's rotation.
- **Drag-pose (the delight):** grab a bone's **tip in the preview** and drag — compute the
  desired world angle `angle_to(jointStart, mouse)`, subtract the parent's world angle,
  **snap to the nearest of the 16** directions, and write that into the current frame's
  cell. Posing by dragging the limb, with the grid updating live underneath.

## Feel (the juice — keep it tasteful for a tool)
The pixel-man **eases** between frames at playback (lerp the drawn angle toward the target
across the FPS interval) so motion looks smooth even though data is quantised to 16 steps.
A soft tick when the marker moves, a brighter blip when a bone rotates (pitch could rise
with the rotation index), a little confirmation chime on copy/save. The selected bone
pulses; the playhead column glows; joints pop slightly on the frame they're keyed. A
small "FRAME 3/16 · 8 FPS · LOOP 12" readout. Restrained palette — this is a workbench,
let the figure be the star.

## Data model
```c
#define NB 18            // bones (rows)
#define NF 16            // frames (cols)
unsigned char rot[NB][NF];                  // 0..15, shown as hex 0-f
int   selBone, selFrame;                    // the cell marker
int   playFrame, fps, loopLen; bool playing;
struct Bone { int parent; int len; int restIdx; int region; const char *tag; }; // static table
// joints computed per draw by walking the tree; save_bytes the rot[][] block + {fps,loopLen}
```
Persist with `save_bytes` (the 18×16 block plus a tiny header); load on `init()` so an
animation survives between runs, seeding a neutral standing pose if nothing's saved.

## Lean into / read
`drummachine.c` (the bones-rows × frames-cols **grid + playhead** idiom this is built on —
read it first), `mario-paint-sound.c` / `deluxe-paint.c` (sibling TOOL carts: mouse UI +
`save_bytes` toy structure), the trig helpers (`dx`/`dy`/`angle_to`/`sin_deg` — degrees),
`lerp`/`ease_*` (smooth playback between quantised keys), `16-spirograph.c`/`elite.c`
(turning angles into line art), `typesave.c` (`save_bytes` blob round-trip). Skip: any
engine entity/animation system (there is none — own arrays + FK by hand), editable bone
lengths, per-frame root translation, and the cut helpers above.
