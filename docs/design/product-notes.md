# product-notes — the audio line as a (maybe) product

STATUS: IDEA / exploration (2026-06-07). Nothing decided beyond "sketch first."
Product-stage detail (save/share codec, native-iOS/AUv3 build plan, the pricing model + marketing
channel that answer the open questions below): [`product-notes-followup.md`](product-notes-followup.md).
Companion reading: [`tinyjam-racks.md`](tinyjam-racks.md) (the product spec, even though it
wasn't written as one), [`packaging-distribution.md`](packaging-distribution.md)
(why native distribution is hard; web is the public path today),
[`mobile-web-notes.md`](mobile-web-notes.md) + [`touch-notes.md`](touch-notes.md)
(the device-readiness work this rides on).

## The thesis

Music-making tools are one of the few consumer-software categories where adults
pay without blinking — and they pay for exactly this *vibe*: constrained,
opinionated, toy-like, one-genre-per-box. Precedents:

- **Teenage Engineering Pocket Operators** — the hardware proof of "a teeny DAW
  per genre" as a product line. Cheap, constrained, playful — a mass hit with
  grown-ups.
- **Koala Sampler** — one developer, a few dollars, huge. The existence proof
  that a solo indie music app can win on charm + focus.
- **Korg / Moog iOS apps** ($15–30 each) — the homage-instrument market is real
  and priced well above games.

The dreamengine angle: the tinyjam rack table IS this product line, the pixel
faceplates ARE the aesthetic, and the 263-cart web gallery is a ready-made free
funnel. The radio → seed-code → rack handoff (tinyjam-racks.md) doubles as a
conversion path: generate free on the radio, type the code into the paid rack to
open the song up.

## The decision: sketch first

Before any commercial infrastructure (app shells, MIDI, AUv3 — the parking lot
below): **get the rebirth-house pilot into strangers' hands via the existing web
gallery and watch what happens.** The gallery already ships to any phone with a
URL; that's a distribution channel most indie music apps would kill for, and it
costs zero new work. Everything in the parking lot has a *trigger* — a signal
from real users that justifies the spend. Until a trigger fires, build racks,
not infrastructure.

## The builder's parking lot

Each item: what it actually is, rough cost, and the trigger that un-parks it.

| Item | What it is | Cost | Trigger |
|---|---|---|---|
| **MIDI file export** | `tools/` script converting the lane blob → `.mid` (already tinyjam racks export tier 3's footnote) | small | a user asks to open their rack song in a DAW |
| **MIDI input (web)** | Web MIDI API — hardware controllers in the browser. Chrome/Edge only; **Safari/iOS has never shipped it**, so it can't be the phone path | small, desktop-only | users asking to play racks with a controller |
| **MIDI input (native iOS)** | CoreMIDI in a native app | medium, app-stage | native app exists |
| **AUv3** | *the* iOS inter-app audio standard (Audio Unit v3 — the rack as a plugin inside AUM/Loopy/GarageBand). Audiobus is the legacy third-party router; Apple's Inter-App Audio is deprecated. This is what the serious iOS-musician crowd asks for first | **high** — a real native app + an extension target; the engine becomes a render callback. Feasible: `sound.h` already renders headless to a buffer (the `--wav` path proves it), but this is a product-stage investment | a paid native app exists *and* reviews ask for it |
| **Ableton Link** | tempo-sync over WiFi (open-source C++ lib). The cheap crowd-pleaser — jam with other apps/hardware at the same BPM. Native-app only (no web) | small–medium | native app stage |
| **Native iOS app** | the cash register (STATUS item 11, iPad runtime, becomes commercial). Wrap the wasm build or build raylib-iOS native | the big one | a rack on the web gallery demonstrably holds people's attention |

Rule of thumb the table encodes: **everything that needs a native app waits for
the native app, and the native app waits for evidence.**

## The latency investigation (do this one — it gates the web-vs-app question)

The feeling that web audio latency is bad needs numbers, because the answer
decides whether "paid web rack" is even viable or the racks must go native.

The key split:

- **Sequencer playback is latency-immune.** The beat clock schedules hits ahead
  (`schedule_hit`); a fixed 50ms output delay shifts the whole groove uniformly
  and nobody can hear it. The grid, mutes, knob sweeps — all fine at any sane
  latency.
- **Live tapping is latency-bound.** The XY play-pad and finger-drumming feel
  output latency directly. Rough bands: <30ms feels like an instrument, 30–60ms
  playable, >100ms is a toy.

So the worst case is already known to be confined to *one* control (the
play-pad), and a sluggish pad degrades to "smear it, don't drum it" — the racks
survive. But measure anyway:

1. Probe cart: big pad, each tap = sharp click + `watch()` the tap timestamp.
2. On device, screen-record with sound; in the video, count ms between the
   visual tap flash and the click. (Or: `wav_request` capture + the touch
   trace timestamps, diffed offline — the harness already records both.)
3. Compare: macOS native build / desktop Chrome / iOS Safari / installed PWA.

Known iOS quirks already on file: the mute-switch silences WebAudio
(touch-notes), audio needs a user-gesture unlock. Both shipped-around already.

## The trademark flag 🚩 (cheap to respect now, expensive later)

As free fantasy-console homage carts, tb303/tr808/tr909/sh101 are normal art
practice. As a **paid product**, Roland trade dress (TR/TB names, the faceplate
look) is actively defended territory, and Reason Studios still holds ReBirth
marks. The rule, adopted now so naming never has to be unwound:

> **Nothing Roland-named or Roland-skinned crosses a paywall.** Paid racks get
> original names + original pixel faceplates (the *silver-box-with-colored-knobs
> vibe* communicates itself without the logotype). The loving homage stays in
> the free gallery. Conveniently, the strongest half of the rack table (Western,
> Fairground, Radiophonic, Toy-Town…) is original IP already.

### ⚠ The rule is currently BROKEN by our own live listing (found 2026-08-13)

Researched because the maker asked what happened to ReBirth. **It was pulled from the App Store on
15 June 2017 after Roland claimed IP infringement** — Propellerhead chose not to contest it and
discontinued the product outright ([Fact](https://www.factmag.com/2017/06/13/propellerhead-rebirth-ipad-discontinued-roland-ip-infringement/),
[Synthtopia](https://www.synthtopia.com/content/2017/06/12/rebirth-is-dead-for-real-this-time/),
[MusicRadar](https://www.musicradar.com/news/propellerheads-rebirth-is-saying-its-final-goodbye-as-roland-says-that-it-infringes-its-ip-rights)).
So this is not a theoretical risk in this exact product category: it is the documented cause of death
of the app Tiny Acid Jam is an homage to.

Meanwhile `apps/tinyacidjam/app.json` — a **paid** ($1.99) listing, already pushed to App Store
Connect — reads:

| field | value |
|---|---|
| title | Tiny Acid Jam: **303** Groovebox |
| subtitle | **808**, **909**, house in your pocket |
| keywords | `808,909,emulator,techno,bassline,drum,synth,sequencer,rave,`**`rebirth`**`,`**`338`**`,`**`tb303`**`,dance,operator,loop` |

That is Roland's model numbers in the title and subtitle, plus **`rebirth` and `338`** — Reason
Studios' marks — in the keyword field. By our own rule, none of it should cross a paywall.

**How the market solves it, and it is instructive:** Bram Bos's **Troublemaker** is marketed as
explicitly *"not a 303"* and sells fine on the sound alone
([Synthtopia](https://www.synthtopia.com/content/2017/01/09/new-troublemaker-synth-not-a-303-it-just-sounds-like-one-and-works-in-your-ios-daw/)).
**Pure Acid** says "inspired by … like the legendary TB-303" rather than claiming to be one. The
pattern is: describe the SOUND and the LINEAGE, never wear the trademark.

**Not fixed here** — renaming a listing is the maker's call and touches the store metadata, the ASO
brief and the icon. But `aso-lint`/`aso-compose` happily packed those keywords because they optimise
for reach, not for legal exposure: **a trademark screen belongs in that pipeline**, and today no tool
enforces the rule this doc wrote down.

## Open questions

- Pricing shape: one app with rack IAPs (Pocket-Operator-catalog feel) vs an app
  per rack (Korg feel)? Decide at app stage, not before.
- Does "paid web" exist at all (itch.io / Gumroad unlock codes), or is web
  strictly the free funnel? The latency numbers + pilot reception feed this.
- Where does feedback from gallery players actually arrive? (There's no channel
  today — maybe that's the cheapest infrastructure worth building *before* the
  pilot ships: even just a link in the cart footer.)
