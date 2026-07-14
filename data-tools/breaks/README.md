# breaks — ingest a drum loop → a chop-ready PCM artifact

Feeds the **break-chopper** cart (WIP): take *any* drum loop (a URL or a local file), freeze it into a
mono float32 PCM artifact, and the cart loads it at runtime via `sample_load()`, auto-slices it, and
lays the slices across a pad grid.

Source-**agnostic** on purpose — the product idea is *"throw in your OWN loop and it gets cut
correctly."* This is the build-time twin of the eventual on-device import; it mirrors
[`../roadview`](../roadview) (`osm-roads.js → .rvb`): acquire externally, **freeze**, the cart consumes
the frozen artifact deterministically (the [capture-then-freeze](../../docs/design/mic-and-sampling.md)
rule — no live dependency, replay-safe).

## Usage

```bash
node data-tools/breaks/breaks.js <url|file> [--name amen] [--bpm 136] [--bars 4] [--sr 44100]
```

Writes into `cache/` (gitignored):
- `<name>.f32` — raw little-endian **float32 mono** PCM at `--sr`, peak-normalized to 0.95 (−1..1,
  exactly what `sample_load()` wants).
- `<name>.json` — `{ name, samples, sampleRate, seconds, bpm, bars, source }`.

`--bpm`/`--bars` are optional metadata (the cart's auto-slicer can also detect the grid); giving them
makes tempo-locked slicing exact. Needs `ffmpeg` (already a repo dep). No node packages.

## The dev fixture

```bash
# the classic amen (The Winstons, 1969) — our slicer test loop: 4 bars @ 136 bpm = 16 beats
node data-tools/breaks/breaks.js \
  "https://www.orangefreesounds.com/wp-content/uploads/2014/11/Amen-break.mp3" \
  --name amen --bpm 136 --bars 4
```

## ⚠ Copyright / the release gate

The amen fixture is a **copyrighted recording** (an uploader's CC tag does not clear the master). It is
fine as a **local dev placeholder** — it must never ship. The guardrails:

- `cache/` is **gitignored** — artifacts are never committed and never baked into a `.cart.png`.
- the cart loads the artifact at **runtime** and falls back gracefully if it is absent.
- **Before any public release** (gallery / iOS): ship **no bundled audio** (loops are user-supplied on
  device), or re-run this tool with a **CC0 / public-domain** source. See the cart's `de:meta` todo.
