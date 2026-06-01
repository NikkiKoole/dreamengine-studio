# The game you're building: INFINIMINER (lo-fi)

## Premise
The proto-Minecraft idea, shrunk to a fantasy-console toy. A tiny voxel world —
**16×16 footprint, 6 blocks tall** — that you walk through in **first person** with
mouse-look. A **crosshair** in the screen center targets the block face you're looking
at; **left-click mines** that block out of existence, **right-click places** a block of
the currently-held type against the face you're aiming at. Four block types
(grass / dirt / stone / wood), each a `tritex`-textured cube. That's the whole game: a
calm, tactile build-and-dig sandbox. No goal, no clock — just the loop of looking,
mining, placing.

## The locked slice — build exactly this
- A **16×16×6** voxel grid, generated once: a couple of dirt/grass layers with a stone
  floor, plus a small hill and a tree or two so there's something to look at and dig
  into. (Bedrock floor row is unminable so you can't fall through the world.)
- **First-person camera**: position `(px,py,pz)` in block units, yaw + pitch from the
  mouse. WASD strafe/forward relative to yaw; you stay at eye height (no gravity — Cut).
- Render the world by **painter's-sorted, back-face-culled textured cubes** via
  `tritex`, exactly the `textured3d.c` idiom but per-voxel. Only draw faces that border
  air (hidden-face culling) so the cube count stays sane.
- A **raycast from the camera through the screen center** (DDA through the voxel grid)
  finds the targeted block + the face normal. Highlight that block's silhouette so the
  player sees what they're about to hit.
- **Left-click**: remove the targeted block. **Right-click**: place the held block type
  in the empty cell on the near side of the targeted face.
- **Number keys 1–4** pick the held block type; a small HUD swatch shows it.
- A satisfying **dig/place SFX** (a short filtered note, pitched per material).

## Cut (do not build)
Infinite / chunked world, terrain streaming, mining tools or durability,
gravity / jumping / physics, inventory counts, crafting, multiplayer, save/load.

## Engine features it leans into (and why)
- **`tritex`** — the textured-3D primitive. Each cube face is two textured triangles
  pulling from a 16×16 tile on the sheet; this is the whole look. Optional 2×2
  subdivision per face tames the affine warp on the faces closest to the camera.
- **Painter's sort + back-face cull** — same math as `textured3d.c`/`solid3d.c`, just
  fed one cube per solid voxel that has at least one exposed face. Sort cubes by camera
  distance, draw far→near; cull faces whose neighbour is solid (the big perf win).
- **Per-face Lambert dither shading** via `fillp(FILL_CHECKER/QUARTER, -1)` over each
  face — free depth cueing, the PS1/picoCAD texture-over-dither look.
- **`mouse_x/mouse_wheel` + relative look** — yaw from horizontal mouse travel, pitch
  clamped; a screen-center crosshair (`line`s) is the reticle. `mouse_pressed(MOUSE_LEFT/
  RIGHT)` drive mine/place.
- **A voxel raycast (DDA)** in cart code — not an engine feature, but the engine's
  degree-based trig (`dx`/`dy`/`sin_deg`/`cos_deg`) and `clamp`/`mid` make the camera
  basis and the step loop tidy.
- **Sound**: a custom plucky `instrument` with a lowpass `instrument_filter`; `note()`
  pitched per material on dig, a softer one on place. Quiet otherwise.
- **`.cart.js` raw-index textures** — four 16×16 procedurally-built tiles (grass top,
  dirt, stone, wood) as raw palette-index arrays so they can use extended colors.

## Controls
- **W / A / S / D** — move forward / strafe left / back / strafe right (relative to look)
- **Mouse** — look around (yaw + pitch); crosshair targets a block face
- **Left-click** — mine the targeted block
- **Right-click** — place the held block against the face you're aiming at
- **1 / 2 / 3 / 4** — select block type: grass / dirt / stone / wood
- **R** — regenerate the world

## Notes / open choices
- Cube budget: 16×16×6 = 1536 cells, but most are interior/hidden. Cap the per-frame
  drawn-cube list (e.g. a fixed array of ~512 visible cubes), sort + draw those. A small
  draw distance keeps it honest on the soft renderer.
- Mouse-look in a 320×200 window means accumulating yaw from `mouse_x` deltas (the
  pointer is clamped to the canvas). Keep a `last_mx` and integrate the delta; this is
  the engine-grain way without a pointer-lock API.
