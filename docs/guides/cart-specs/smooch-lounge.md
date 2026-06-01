# The game you're building: SMOOCH LOUNGE (a saucy cabaret rhythm game)

## Premise
A cheeky **burlesque-cabaret rhythm game** played for laughs and winks. A pixel-art
chanteuse works the stage of a velvet speakeasy; falling **hearts and lips** drop down
lanes to the beat and you tap them on time to land kisses, fan-flutters and hip-shimmies.
The "naughty" is all **tone** — saucy double-entendre song titles, a flirty announcer, and
wink-wink judgment call-outs ("OOH LA LA!", "HUBBA HUBBA!", "*fans self*"). Tasteful and
silly: feather fans, blown kisses, raised eyebrows — innuendo, never explicit. Think
*Mr. & Mrs. Pac-Man meets a smoky cabaret*, rated cheeky, not adult.

## Slice (locked) — 🟢
**One stage, one song that ramps, a flirt-meter chase.** A 4-lane note highway scrolling
to a hand-authored beat chart; tap the matching lane key as each heart crosses the hit
line; timing windows give ratings (**SMOOCH / SMOOTH / CLUMSY / WHIFF**) that feed combo +
a **TEASE METER**; fill the meter to trigger a brief **"BIG FINISH"** crowd-go-wild mode
(double points, the dancer struts); a score + letter grade at the end; saved hi-score. One
difficulty, restartable. Skip: a song-select menu, custom-chart editor, multiple stages —
nail one tune that escalates.

## Core mechanics (the depth = tight timing)
- **The clock is the music.** Drive everything off `bpm`/`beat()`/`beat_pos()` like the
  drum machine — notes are placed on beats, the playhead *is* the song, no hand timer.
- **Beat chart:** a static array of `{beat, lane}` events. Notes spawn a few beats early at
  the top, fall at a fixed speed, and cross a **hit line** exactly on their beat.
- **Judging:** on a lane keypress, find the nearest unhit note in that lane and judge by
  how close `beat_pos()` is to its target — tight window = SMOOCH, looser = SMOOTH, late/
  early = CLUMSY, no note = WHIFF (breaks combo). Missed notes that fall past the line also
  break combo.
- **Tease meter & combo:** good hits fill the meter and grow a combo multiplier; misses
  drain it. Full meter → **BIG FINISH**: the chart doubles up, points ×2, the stage lights
  go wild, until it empties.
- **Hold/flourish notes (optional, if cheap):** a long "blow-a-kiss" note you hold across
  several beats for bonus — adds variety without a second system.

## Sprites & maps
A **dancer** (4–6 frames: pose / shimmy-left / shimmy-right / fan-flutter / wink /
big-finish kick — a 16×32 = two stacked slots), feather-fan and lips props, the falling
**heart** and **lips** note sprites, a "kiss" burst, a little **crowd** of bobbing
silhouette heads along the bottom. Backdrop drawn from primitives: velvet curtains
(`fillp` dither), a spotlight cone, stage footlights, a neon "SMOOCH LOUNGE" sign that
buzzes. `pal()` recolors the crowd + cycles the marquee. Keep everything clothed and
silhouette-cute — burlesque *suggestion*, feather fans and raised brows, nothing graphic.

## Juice
Lip-print stamps + heart bursts on each SMOOCH, the dancer **squashes into the pose** on
the beat, screen-edge spotlight sweep, the marquee + footlights pulse on every downbeat,
combo number bounces, the crowd cheers louder (more bob, more hearts) as the meter climbs,
a **BIG FINISH** confetti/heart shower with a `shake` and a flash, a comic "*womp*" + a
fan-snap on a WHIFF. Audio: a sultry lounge groove (walking bass + brushed hat off the
sequencer idiom + a `LFO`-vibratoed lead "voice"), a kiss-*mwah* SFX per hit pitched to the
note, a sax-stab on BIG FINISH, a sad muted-trumpet on a miss. The screen should sway.

## Data model
`struct Note { int beat, lane; bool hit, dead; }[]` chart pool; `float hitline_y, fall`;
`int combo, score; float tease;` `int dancer_frame; float poseT;` for the squash;
judgment windows as fractions of a beat. `save()` the hi-score (one int slot).

## Controls
Four lanes on **A S K L** (or D F J K) — tap as a note crosses the hit line; arrows/Z map
for gamepad. SPACE = start / restart. (Mouse optional for menus.) One-screen, instantly
legible: lane = key, key = kiss.

## Lean into / read
`drummachine.c` / `omnichord.c` (the `beat()`/`beat_pos()` transport + sequenced voices),
`geometrydash.c` (existing **beat-synced** cart — read it for note timing, then go
lane-rhythm + cabaret theme), `20-instruments.c` / `21-lfo.c` / `22-filter.c` (the sultry
lounge voices), `crowd.c` (`pal()`-recolored bobbing crowd), `particles.c` / `juice.c`
(bursts, shake, flash), `12-hiscore.c` (`save()`). Tone reference: keep it PG-13 saucy —
double-entendre and winks, never explicit. Skip: a chart editor, song select, anything
beyond suggestive (no nudity/explicit art) — the comedy is the innuendo, not the pixels.
