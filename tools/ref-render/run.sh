#!/usr/bin/env bash
# ref-render — render a note from somebody ELSE'S physical model, so ours has something to be
# wrong against. The tool that found the INSTR_BOWED friction bug (commit ad657323).
#
#   bash tools/ref-render/run.sh stk <Brass|Flute|Clarinet|Bowed> [hz] [amp] [lipCC]
#   bash tools/ref-render/run.sh luthier <steel|nylon|gut|glass> [hz]
#   bash tools/ref-render/run.sh clean
#
# Output lands in build/ref-render/<name>.wav (44.1k mono, note ON at 0.5s, OFF at 6.0s, 7s long)
# — deliberately the SAME shape tools/carts/bowprobe.c renders, so the two drop straight into the
# same oracles with no re-timing:
#
#   node tools/harmonic-spec.js <wav> 220 --n 12       how loud each partial is
#   node tools/ref-render/peaks.js <wav> 2.5 5.5       WHERE each partial is, high resolution
#   node tools/wav-envelope.js <wav>                   level + centroid over time
#
# WHY THIS EXISTS. Every gate we own checks an OUTPUT PROPERTY — in tune, right loudness, no
# clicks, reconverges. Not one asks whether the MECHANISM is physically sane, so BOWED ran for
# months with a friction coefficient above 1.0 (the bow returning more energy than arrived, every
# sample, at every pressure) with a fully green board. What exposed it was not a cleverer gate, it
# was having a second implementation to put beside it. Reach for this whenever an engine "sounds
# a bit off" and the gates disagree.
#
# ⚠ USE THE LONG WINDOW. `inharm-spec` reads 0.3s, which ALIASES our engines' own humanised
# vibrato (BOWED: 5.3 Hz, ±15.5 cents) and reports a partial stretch that is not there. That cost
# a full round of wrong conclusions. peaks.js takes an explicit window; give it 3s.
#
# ── STATE OF EACH REFERENCE, measured 2026-08-25. Read before trusting one. ──────────────────
#   Clarinet  ✅ USABLE AS-IS. h1 at 220.00 Hz (0.0 cents), evens down 60 dB — textbook odd-only
#                cylindrical pipe. The reference for our INSTR_REED.
#   Flute     ◐  SPECTRUM ONLY. Harmonically locked but a uniform +28 cents sharp, because STK
#                sets its delay from a hand-tuned constant its own source calls a "Fudge
#                correction for filter delays" — the same class of hack as our BW_LOOP_DELAY.
#                Do NOT use it to judge our PIPE's tuning residuals; it has the same disease.
#   Bowed     ◐  OCTAVE QUESTION UNRESOLVED. Sustains, and its waveform is a beautifully clean
#                two-slope Helmholtz shape — but it sounds an OCTAVE ABOVE the requested pitch
#                here (h1 numerically zero, h2 carries the fundamental, six cycles where three
#                belong). Either STK's Bowed has an octave convention or this harness drives it
#                wrong. Resolve before any quantitative A/B.
#   Brass     ✗  DOES NOT SELF-OSCILLATE. Blips and dies at every amplitude 0.8-1.2 × lip CC
#                64/96 tried. Its bore loses 15% per round trip so the lip must make up 18%.
#                Unsolved; this is why the brass question is still open.
#   luthier   ✅ USABLE. FDTD stiff string. `gut` is the voicing the maker preferred by ear.
#
# LICENCES. STK is Cook/Scavone, MIT-equivalent ("without restriction") — borrowing a constant is
# fine WITH ATTRIBUTION, which is how runtime/sound.h's BOW_BODY_HZ table cites luthier (also MIT).
# ⚠ timowest/flute-lv2 is a good flute model and is GPL-2.0: measure against it if you like, never
# copy a line or a constant into this repo.
#
# Neither dependency is vendored — both are fetched into build/ref-render/ on first use and are
# gitignored. `clean` removes them.
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT=build/ref-render
mkdir -p "$OUT"

case "${1:-}" in
stk)
  INST="${2:-Clarinet}"; HZ="${3:-220}"; AMP="${4:-0.8}"; LIP="${5:--1}"
  [ -d "$OUT/stk" ] || git clone --depth 1 -q https://github.com/thestk/stk.git "$OUT/stk"
  if [ ! -x "$OUT/stkrender" ]; then
    src=(Stk BiQuad ADSR DelayA DelayL Envelope Noise OnePole OneZero PoleZero SineWave FileRead Brass Flute Clarinet Bowed)
    files=(); for s in "${src[@]}"; do files+=("$OUT/stk/src/$s.cpp"); done
    # NOTE the array. `for s in $LIST` does NOT word-split in zsh and hands clang one bogus path.
    clang++ -O2 -std=c++11 -I "$OUT/stk/include" -D__OS_MACOSX__ -D__LITTLE_ENDIAN__ \
            -o "$OUT/stkrender" tools/ref-render/stkrender.cpp "${files[@]}"
  fi
  # rawwaves path is relative to CWD inside stkrender.cpp
  ( cd "$OUT" && ./stkrender "$INST" "$HZ" "$AMP" "$LIP" > "raw.f32" )
  ffmpeg -v error -y -f f32le -ar 44100 -ac 1 -i "$OUT/raw.f32" "$OUT/stk_$INST.wav"
  rm -f "$OUT/raw.f32"
  echo "wrote $OUT/stk_$INST.wav"
  ;;
luthier)
  MAT="${2:-gut}"; HZ="${3:-220}"
  [ -f "$OUT/luthier.html" ] || curl -sL -o "$OUT/luthier.html" \
      https://raw.githubusercontent.com/chrisjz/luthier/main/index.html
  [ -f "$OUT/phys.js" ] || python3 tools/ref-render/luthier-extract.py \
      "$OUT/luthier.html" "$OUT/phys.js" "$OUT/tune.js"
  # NOBODY=1 renders the BARE STRING (its four body resonators off) — the honest comparison
  # against a cart with MODE_BOW_BODY at 0.
  ( cd "$OUT" && node ../../tools/ref-render/luthier-render.js "$HZ" "$MAT" 0.20 1050 "luthier_$MAT.wav" )
  echo "wrote $OUT/luthier_$MAT.wav"
  ;;
clean)
  rm -rf "$OUT"; echo "removed $OUT"
  ;;
*)
  sed -n '2,12p' "$0"; exit 1
  ;;
esac
