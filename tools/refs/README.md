# tools/refs: reference recordings, by manifest

Real recordings that synthesized voices are tuned AGAINST (`tools/oneshot-compare.js`). The audio lives
in `build/refs/<set>/`, which git ignores; what is committed here is the MANIFEST, one `<set>.txt` per
set, and `node tools/fetch-refs.js [set]` rebuilds the folder from it (re-checking every license on its
source page, refusing anything that is not CC0).

| set | what | used by |
|---|---|---|
| `drums-cc0.txt` | acoustic kit one-shots (lennartgreen "Vintage Hip Hop"), cymbals and hats (pjcohen, minorr, Ev3nIfItKillsME), four rides + a bell to choose from by ear, a cross-stick | drumhand (its synth voicing; the first six are also its RECORDED cymbal kit, key K) |

## Rule: pick the reference by EAR, then match it by numbers

A voice can match every measured number of a reference and still be wrong, because the reference was:
drumhand's ride was matched to a dark, modified "wash" ride and came out as "a train locomotive". The
fix was choosing a different ride by listening (Savannah Ride for the bow, a Meinl Byzance B2 bell).

## Leads (NOT checked, not in any manifest yet)

- **Barry Beats (aka Sispex)**, https://barrybeatsakasispex.bandcamp.com: many drum samples, also as
  separate one-shots; suggested by Nikki (2026-11-04) as a source for more kits. ⚠ Check the license
  before use: Bandcamp sample packs are usually licensed for use IN music, often not for redistribution,
  so they may suit tuning by ear locally but not a manifest that re-downloads them.
- **pjcohen's drum KITS** on freesound (many acoustic kits, one drum per file): CC BY 4.0, not CC0
  (his cymbals ARE CC0). Usable with attribution if a set ever allows CC BY; fetch-refs refuses them today.
- **Boxed Ear's free Roland CR-78 pack** (what cr78 was tuned against; copies in vector-sketch/experiments):
  terms unknown, see playtime's assets/sounds/README.md.
