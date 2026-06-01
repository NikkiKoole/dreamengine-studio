# The game you're building: DOOM

## Premise
A first-person corridor shooter in the Doom mold: stalk a maze of rooms, blast
billboard-sprite demons, grab keycards, find the exit. Built on a raycaster, with
distance-scaled enemy sprites, chunky weapons, and a thumping pace.

## Slice (locked)
**One level**, 2 weapons (pistol/shotgun), 2–3 enemy types, keycard + locked door,
health/armor/ammo pickups, an exit switch. *Extends the existing `raycaster.c` — read
it first.* Optional stretch: textured walls via the `tritex` idea (otherwise flat-shaded
walls are fine).

## Core mechanics
- **Raycaster world:** a grid map of walls; DDA one ray per column for wall strips with
  distance shading (the existing `raycaster.c`). Move/strafe/turn; `touching_map`
  collision; doors as toggling wall cells; keycards gate locked doors.
- **Enemies as billboards:** sprite imps/zombies placed in the world, drawn as
  **distance-scaled `sspr`** sorted back-to-front and clipped against the wall depth
  buffer (so they hide behind walls). They wake, approach, and attack (hitscan or a slow
  projectile) with a telegraph.
- **Weapons:** pistol (precise, infinite-ish) and shotgun (spread, ammo); a center-screen
  gun sprite with a fire/recoil frame; hitscan down the screen center / aim assist.
- **Pickups + HUD:** health, armor, ammo, keycard; classic bottom HUD bar with a face.
- **Goal:** find the keycard, open the exit door, hit the switch.

## Sprites & maps
Grid wall map (the raycaster's own map), enemies + pickups + the gun as sprites
(`pal()` for enemy variety / damage flash). Optional wall textures via `tritex`/textured
sampling (read `textured3d.c`). Floor/ceiling as flat bands or a simple `mode7` floor.

## Juice
Muzzle flash lighting the room, weapon recoil + screen `shake` on the shotgun, enemy
hit-flash (`pal` white) + a gory death frame + particle spray, full-screen red flash +
`shake` when you take damage, a low-health heartbeat/alarm, pickup blips, door rumble.
Audio: punchy gunshots (filtered noise), enemy growls/sighting roars, ambient dungeon
drone, a driving bpm metal-ish bed, switch/exit jingle.

## Data model
Wall grid + door states; `struct Enemy { float x,y; int type,hp,state; float wake; }`
pool; `struct Pickup`; a per-column wall-depth buffer for sprite occlusion; player
`x,y,ang,hp,armor,ammo,keys,weapon`.

## Controls
WASD/arrows move + strafe + turn (or mouse-look turn), **Z = fire**, X = switch weapon,
SPACE = open door/use switch. Keep it playable on keyboard.

## Lean into / read
`raycaster.c` (the FPS base — extend it), `textured3d.c`/`mode7.c` (optional wall/floor
texturing), `crowd.c` (pal enemy recolor), `effects.c`/`particles.c` (muzzle/gore/shake),
`22-filter.c` (gun/ambience sound). Skip: full episode, save system, complex AI/pathing
— one tight level.
