# Building a new radio station — the ordered runbook

**START HERE if you've been asked to add a radio channel.** This is the *spine* — the
phases in the order you do them, from a genre name to a shipped, registered cart. It owns
no recipes or API detail: each phase is a few lines + a link to the authoritative deep doc.
Follow it top to bottom; don't skip ahead (Phase 0 exists because the natural instinct —
"open a similar cart and copy it" — is the one thing that quietly wrecks the result).

> **What a station IS:** an *auto-composing* cart built on [`radio.h`](../../runtime/radio.h)
> — it plays itself forever from a seed, and the player tunes it (SPACE = new song, feel/
> tempo/tone knobs, the B band panel). Twenty-one ship today; the newest, **`afrobeat.c`**,
> is the worked example referenced throughout below.

> **Key term — a "brain":** the family's word for one *reusable unit of generative logic*.
> A station isn't a monolith; it's a **pick of interchangeable brains** — a **chord brain**
> (what the harmony does: functional changes / modal drift / a one-chord vamp), a **time
> feel** (swing, push/drag, the broken-funk pocket), a **melody brain** (the re-pitched cell,
> or the [`improv.h`](../../runtime/improv.h) soloist), a **bass brain**, and a **form brain**
> (the arrangement — sections, the build, the break) — plus the **band** (the voices from
> Phases 1–2). You mix and match: e.g. a modal-vamp chord brain + a broken-funk feel +
> `improv.h` solos + a terraced form = afrobeat. **Phase 1 decides which brains the genre
> needs; Phase 4 wires them.** The full menu — every existing brain, where it lives, and how
> to combine them — is the **brain catalog** in [`game-music.md`](game-music.md) §"The brain
> catalog". When this runbook says "the brains," it means these.

> Building a sound cart that is **not** a self-playing station (a synth, a drum machine, a
> playable instrument)? This runbook is station-specific — go to the shelf
> [`instrument-carts.md`](instrument-carts.md) and pick the matching shape instead. Phases
> 0–2 (design the voices intent-first) still apply to you; Phases 3–4 (the `radio.h` chassis
> + generative brains) don't.

---

## Phase 0 — The firewall (the one rule that matters most)

**Do NOT open any existing station's source yet.** Reading a cousin cart first anchors you
to *its* instrument lineup, and that inertia is exactly how the library homogenized (the same
synth kit copied into 6 stations, every piano faked on TRI, near-clone basses). You'll read a
cousin in Phase 3 — but only for its *wiring*, never before you've designed your own band.

→ The why, in full: [`cart-authoring-prompt.md`](cart-authoring-prompt.md) §"Designing a
sound cart's voices — intent-first, then shop". The cost of skipping it, measured across all
21 stations: [`../design/radio-genre-fidelity.md`](../design/radio-genre-fidelity.md).

## Phase 1 — Design the band BLIND

Imagine the ideal record in this genre and write the lineup **from the music**, not from our
palette: the instruments, the harmony/rhythm/form **brains**, the signature moves. Wikipedia /
musicology / your ears — not `tools/carts/`. Produce a written brief before touching code.

→ The brief format + worked discipline: [`cart-authoring-prompt.md`](cart-authoring-prompt.md).
(Example: afrobeat's blind brief named two interlocking guitars, a horn section, a euclidean
bell timeline, and the Tony Allen broken-funk feel — *before* any palette check.)

> **Genre station or ARTIST station?** A *genre* (mariachi, ambient, dub) wants one coherent
> texture that the seed re-keys — every song is "the band doing its thing." A specific
> *artist with a recognizable catalog* (AIR, Daft Punk, Mac DeMarco) wants the dial to play
> recognizably DIFFERENT songs *of theirs* — "the Sexy Boy one" vs "the La Femme d'Argent
> one." For an artist station your blind brief must also name **4–6 actual tracks** and what
> makes each one *itself* (its bass tone, lead voice, groove, form), not just the band's
> general sound. Those named tracks become the song-templates you roll in Phase 4.

## Phase 2 — Shop the palette (gaps = your new recipes)

Now, and only now, match each imagined voice to what we have:
1. **Is there a recipe for it?** → [`instrument-recipes.md`](instrument-recipes.md) (the supply
   side — every named recipe, by engine, incl. the **untapped shelves**
   `GUITAR`/`PIPE`/`BOWED`/`PIANO`/`VOICE` where the richest new sound lives).
2. **Has a station already voiced it?** → [`instrument-presets.md`](instrument-presets.md) (the
   demand side). Reuse it *on purpose* — add your cart to its **used by** line, don't mint a
   duplicate.
3. **Whatever the genre wants that we lack is your new recipe** — usually the most valuable
   thing the cart adds. (afrobeat reached `INSTR_GUITAR`/`INSTR_BRASS` — engines no station
   had used.)
4. **Missing something? SAVE THE GAP — don't fake it badly.** Both kinds of gap go to the
   wants roster [`../design/sound-next-steps.md`](../design/sound-next-steps.md), so the engine
   knows what to build next; a silently-faked voice is a lost signal.
   - *An instrument/timbre no engine voices convincingly* — you reached for it in Phase 1 and
     the palette has no honest match → record it under §"New instruments", with the song/role
     that wanted it.
   - *An effect we don't have* (wah, reverb, chorus, leslie, tape, compression) → record it
     under §"Engine & effects gaps". Per-cart pattern:
     [`../design/afrobeat-effects-wants.md`](../design/afrobeat-effects-wants.md).

## Phase 3 — Copy the CHASSIS (not the voices)

Pick the closest cousin **by wiring shape**, copy its skeleton, and design your voices from
Phases 1–2 into it. Never inherit the cousin's instrument lineup.
- Find the cousin: [`instrument-carts.md`](instrument-carts.md) → the "auto-composing station"
  shelf row (`bossa` simplest; `cocktail`/`motorik` improv soloist; `dub`/`citypop` solo.h jam
  strip; **`addis`/`afrobeat`** real-engine band + `euclid()` polyrhythm).
- Learn the chassis contract — the PRNG/step-clock/voice-leading/chrome blocks, and **⚠ the
  seed-compatibility rule** (a pinned seed IS the song; never alter the shared block):
  [`../../runtime/radio.h`](../../runtime/radio.h) header + [`game-music.md`](game-music.md)
  §"`radio.h` — the shared chassis".
- Optional layers: `solo.h` (a player jam strip) / `improv.h` (an auto-soloist).

## Phase 4 — Write the brains

The cart's own generative logic, sitting on the chassis. Pick from the catalog by what the
music is:
- **chord/vamp brain · voice leading · time feel · melody · form** — the five layers, each
  with examples: [`game-music.md`](game-music.md) §"The five layers" + §"The brain catalog".
- **Seeds:** composition randomness on `rad_srnd` (the song); performance randomness (ghosts,
  humanize, solos) on engine `rnd()`. [`game-music.md`](game-music.md) §"Seeds".
- A new feel/brain is *your cart's business* (layer-3 logic); it graduates to a shared header
  if a second cart reuses it. (afrobeat's Tony Allen feel = a new time brain over the clock.)
- **ARTIST station? Roll SONG ARCHETYPES, not just a seed.** To make the dial play
  recognizably different *tracks* (not one texture re-keyed), use the **stolen-playbook chord
  brain (#4)** — encode the 4–6 cited songs from Phase 1 as template progressions — and
  bundle each into a named ARCHETYPE that *also* fixes its groove / tempo / lead-voice / form,
  then roll which archetype the seed plays. The seed still varies key/patterns *within* the
  archetype, so "the Sexy Boy one" sounds different every time but always like Sexy Boy.
  Reference carts: `jingle.c` (six cited DeMarco charts), `citypop.c`, `house.c`, `yacht.c`.
  The four bundle axes (groove · form · pocket · tempo): [`game-music.md`](game-music.md)
  §"Same song every night?"; chord brain #4: §"The brain catalog".

## Phase 5 — Build + verify

```bash
node tools/make-cart.js tools/carts/<name>.c editor/public/carts/<name>.cart.png   # build + embed source
node tools/make-cart.js --run editor/public/carts/<name>.cart.png                  # bake screenshot
```
Then prove the brains actually fire (don't trust a clean compile):
- **Trace:** `node tools/play.js <name> script /dev/null --headless --frames 16000 --seed 7
  --trace /tmp/t.jsonl` then `jq` the `.w` watch fields for every section/mode/groove/form.
- **Audio:** `--wav /tmp/a.wav --det` → `node tools/wav-analyze.js` (peak < 0 dBFS, 0 clipped).
- **A part drops out / a "solo gets cut off"?** `node tools/voice-trace.js /tmp/t.jsonl` reads
  the voice events (on/off/reuse/steal/choke) — is it real voice loss? And `--solo-slot <n>`
  renders that instrument slot alone — or is it just masked/quiet in the mix? See
  [`../design/audio-voice-debugging.md`](../design/audio-voice-debugging.md).
- Full how-to: [`debug-harness.md`](debug-harness.md). Build pipeline + the rebake trap:
  [`cart-authoring.md`](cart-authoring.md).

## Phase 6 — Register + keep the recipe docs current (a rule, not a nicety)

1. **Register** — add a tagged entry (`kind:["toy","instrument"]` + `homage`) to
   `editor/public/carts/index.json`; `node tools/lint-carts.js`. *(Shared file — check it's
   not dirty with another agent's edits first; CLAUDE.md hazard #2.)*
2. **Chart it** — add the station's voice chart (slot → role → preset → engine) to
   [`radio-voices.md`](radio-voices.md).
3. **Name the recipes** — new presets + **used by** lines in
   [`instrument-presets.md`](instrument-presets.md) (don't duplicate a reused one).
4. **Shelf row** — add the cart to [`instrument-carts.md`](instrument-carts.md).
5. **Mark shipped** — if it was a candidate in
   [`../design/future-stations.md`](../design/future-stations.md), strike it through.
6. **If anything was missing** (Phase 2.4 — an effect OR an instrument you couldn't honestly
   voice): record it in [`../design/sound-next-steps.md`](../design/sound-next-steps.md)
   (§"New instruments" + §"Engine & effects gaps"), and/or a per-cart wants note like
   [`../design/afrobeat-effects-wants.md`](../design/afrobeat-effects-wants.md). Don't let the
   gap vanish — it's the build-list for the next engine pass.
7. **Check status** — `node tools/cart-status.js` (NEEDS REBAKE = embedded source drifted;
   NOT PUBLISHED = no web build yet, optional via `tools/publish-cart.sh`).
8. **Commit by explicit pathspec** — list every file you touched; never a bare `git commit`
   (CLAUDE.md §Git).

---

## The firewall, restated

The whole runbook is really one idea with scaffolding: **design the band before you read how
anyone else built theirs.** Phases 0–2 buy you an original lineup; Phase 3 lets you copy the
boring shared plumbing without copying the interesting parts. Skip it and you get station #22
that sounds like station #6.

> Maintenance: this doc is the *order*; it deliberately holds no recipes or signatures, so it
> shouldn't need editing when those change — only when the *process* does (a new phase, a
> moved doc). The deep docs it links are the sources of truth.
