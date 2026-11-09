#!/usr/bin/env bash
# ref-render — render a note from somebody ELSE'S physical model, so ours has something to be
# wrong against. The tool that found the INSTR_BOWED friction bug (commit ad657323).
#
#   bash tools/ref-render/run.sh stk <Brass|Flute|Clarinet|Bowed|ModalBar> [hz] [amp] [lipCC|preset]
#   bash tools/ref-render/run.sh stk <TubeBell|Rhodey|Wurley|HevyMetl|BeeThree|PercFlut|FMVoices> [hz] [amp]
#   bash tools/ref-render/run.sh luthier <steel|nylon|gut|glass> [hz]
#   bash tools/ref-render/run.sh open303 [cutoff_hz res_pct envmod_pct decay_ms accent_pct]
#   bash tools/ref-render/run.sh mutable [biome_id secs [--solo n] [--dry]]   (Mutable Dreams, see mdcompare.js)
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
#   ModalBar  ✅ USABLE AS-IS (measured 2026-09-14). The modal reference for the exciter-into-
#                resonator engine (design/engine-reach-modal-research.md). Preset 0 (marimba) at
#                220 Hz puts its modes at 220 / 878 / 2343 Hz, which is its preset table's
#                1.0 / 3.99 / 10.65 to the Hz. TWO THINGS ONLY A RENDER SHOWS:
#                (a) ONE MODE DOES NOT TRANSPOSE. Render the same preset at 110 Hz and the first
#                    three modes halve to 110 / 439 / 1172 while a fourth sits at 2443 Hz in BOTH
#                    (-36 dB at 220, -48 dB at 110). That is STK's negative-ratio convention
#                    (a ratio < 0 means absolute Hz) audible and measurable: an engine that assumes
#                    every mode is a ratio cannot voice this preset.
#                (b) IT IS SHORTER THAN THE HARNESS. Pole radius 0.9996 is a ~0.39s T60, so the bar
#                    is silent by ~0.9s and the 6.0s noteOff damps nothing. Analyse in a window near
#                    the strike (0.52-0.82s works); the header's "give peaks.js 3s" rule is for
#                    SUSTAINED references and cannot apply to a struck one.
#                ⚠ Its exciter is a RECORDED STRIKE (rawwaves/marmstk1.raw through an envelope and a
#                one-pole), not a synthetic burst, so the attack is not something we can port.
#   luthier   ✅ USABLE. FDTD stiff string. `gut` is the voicing the maker preferred by ear.
#   open303   ✅ USABLE (measured 2026-11-08). Robin Schmidt's TB-303 model, MIT; its filter-env
#                mapping is fitted to measurements of a real unit. Against runtime/acid303.h in
#                acid_stock() voicing, every core knob at 0.5 on both sides (tools/carts/acidref.c):
#                (a) HELD NOTES: Open303's VCA DECAYS while the gate is held (0.93 → 0.11 in 2 s, the
#                    real 303's behaviour); ours HOLDS FLAT (acid_define's sustain 6 of 7).
#                (b) ACCENT LEVEL: +6.6 dB over a plain note on Open303, +2.6 dB on ours (vol 7 vs 5).
#                (c) ACCENT DECAY: Open303 uses a FIXED short decay (200 ms) on accents whatever the
#                    DECAY knob says, the documented 303 trait; our classic mode decays an accent
#                    like a normal note (a deliberate choice in acid303.h, now contradicted).
#                (d) RANGES: Open303 uses the stock pots, cutoff 314..2394 Hz and decay 200..2000 ms
#                    (exp); ours is cutoff 60..3840 Hz and decay 30..530 ms. At 12 o'clock the filter
#                    sweep dies ~3x sooner on ours (280 vs 632 ms) and sits darker (480 vs 867 Hz),
#                    which is why our SLID steps (no env retrigger) go dull: 417-506 Hz centroid vs
#                    963-1081 Hz on Open303.
#                The accented peak brightness MATCHES (1671 vs 1630 Hz centroid).
#                ✔ ADOPTED 2026-11-08, all four, by ear (build/ab303): acid303.h's STOCK voicing
#                (acid_stock / the `classic` flip) now does (a)-(d); the Devil Fish voicing renders
#                byte-identical to before (acidrack, acidtheremin, acidcandy checked). Together they land
#                at accent +6.4 dB, held note -9.7 dB at 1.7 s, slid step 894 Hz (Open303: +5.7, -12.0,
#                987). ⚠ The fade rides note_vol per frame (acid_ride) because the engine's own DECAY
#                stage is the wrong SHAPE for a VCA: near flat, then a drop. No decay time matched both
#                -4.9 dB at 0.7 s and -12 dB at 1.7 s; a 303 (an RC discharge) falls fastest at the start.
#                acidref now renders the header itself, so re-running this A/B measures what ships.
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
    src=(Stk BiQuad ADSR DelayA DelayL Envelope Noise OnePole OneZero PoleZero SineWave
         FileRead FileWvIn FileLoop TwoZero Brass Flute Clarinet Bowed Modal ModalBar
         FM TubeBell Rhodey Wurley HevyMetl BeeThree PercFlut FMVoices Phonemes)
    files=(); for s in "${src[@]}"; do files+=("$OUT/stk/src/$s.cpp"); done
    # NOTE the array. `for s in $LIST` does NOT word-split in zsh and hands clang one bogus path.
    clang++ -O2 -std=c++11 -I "$OUT/stk/include" -D__OS_MACOSX__ -D__LITTLE_ENDIAN__ \
            -o "$OUT/stkrender" tools/ref-render/stkrender.cpp "${files[@]}"
  fi
  # rawwaves path is relative to CWD inside stkrender.cpp
  # ModalBar voices NINE different bars off one class, so the preset goes in the filename or
  # every render overwrites the last one and an A/B compares a file with itself.
  TAG="$INST"; [ "$INST" = "ModalBar" ] && TAG="ModalBar${LIP/-1/0}"
  ( cd "$OUT" && ./stkrender "$INST" "$HZ" "$AMP" "$LIP" > "raw.f32" )
  ffmpeg -v error -y -f f32le -ar 44100 -ac 1 -i "$OUT/raw.f32" "$OUT/stk_$TAG.wav"
  rm -f "$OUT/raw.f32"
  echo "wrote $OUT/stk_$TAG.wav"
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
open303)
  # Robin Schmidt's Open303 (MIT), the TB-303 model X0X and schwung-303 port. NOT the note-on-at-0.5s
  # shape above: it plays the ACID PROBE PATTERN (a 16-step line twice, then a long plain note and a
  # long accented one) on the same 735-sample frame grid tools/carts/acidref.c plays through
  # runtime/acid303.h, so the two WAVs line up sample for sample. Ours:
  #   node tools/play.js acidref run --headless --frames 704 --wav build/ref-render/acidref.wav
  [ -d "$OUT/open303" ] || git clone --depth 1 -q https://github.com/RobinSchmidt/Open303.git "$OUT/open303"
  if [ ! -x "$OUT/open303render" ]; then
    D="$OUT/open303/Source/DSPCode"
    files=(); for f in "$D"/*.cpp; do files+=("$f"); done
    clang++ -O2 -std=c++11 -w -I "$D" -o "$OUT/open303render" tools/ref-render/open303render.cpp "${files[@]}"
  fi
  shift || true
  "$OUT/open303render" "$@" > "$OUT/raw.f32"
  ffmpeg -v error -y -f f32le -ar 44100 -ac 1 -i "$OUT/raw.f32" "$OUT/open303.wav"
  rm -f "$OUT/raw.f32"
  echo "wrote $OUT/open303.wav"
  ;;
mutable)
  # Mutable Dreams 1.4 (Kid Bunka; dreams.cpp MIT OR GPL-3.0, Plaits/Rings/Elements MIT), the
  # firmware the `dreamatlas` cart translates. Builds the UPSTREAM ENGINE natively (mdrender.cpp)
  # so a biome renders through the real voices. Then compare track by track with:
  #   node tools/ref-render/mdcompare.js <biome_id> [--secs 12] [--wet] [--tracks 0,3]
  # ⚠ Their app/compat/ REPLACES the C headers (math.h → their fastmath.h), so their sources build
  # with it on the path and mdrender.cpp WITHOUT it, or libc++'s <cstring> refuses to compile.
  R="$OUT/mutable-dreams"; A="$R/app"
  [ -d "$R" ] || git clone -q --depth 1 --branch v1.4 https://github.com/kidbee85-beep/mutable-dreams.git "$R"
  if [ ! -x "$OUT/mdrender" ]; then
    srcs=(third_party/elements/dsp/exciter.cc third_party/elements/dsp/resonator.cc
          third_party/elements/resources.cc third_party/plaits/dsp/engine/additive_engine.cc
          dsp/dreams.cpp third_party/plaits/dsp/engine/fm_engine.cc
          third_party/plaits/dsp/engine/virtual_analog_engine.cc third_party/plaits/resources.cc
          third_party/rings/dsp/resonator.cc third_party/rings/resources.cc
          third_party/stmlib/dsp/units.cc third_party/stmlib/utils/random.cc)   # = fm1.toml's o2 list
    mkdir -p "$OUT/mdobj"; objs=()
    for s in "${srcs[@]}"; do
      o="$OUT/mdobj/${s//\//_}.o"
      clang++ -O3 -ffp-contract=off -std=c++11 -fno-exceptions -fno-rtti -w -DPLAT_HOST -DTEST \
              -I "$A" -I "$R/base/include" -I "$R/base/lib" -I "$A/third_party" -I "$A/compat" -c "$A/$s" -o "$o"
      objs+=("$o")
    done
    clang++ -O2 -std=c++11 -I "$A" -o "$OUT/mdrender" tools/ref-render/mdrender.cpp "${objs[@]}"
  fi
  if [ -n "${2:-}" ]; then
    shift
    "$OUT/mdrender" "$@" > "$OUT/raw.f32"
    ffmpeg -v error -y -f f32le -ar 44100 -ac 2 -i "$OUT/raw.f32" -c:a pcm_s16le "$OUT/mutable_$1.wav"
    rm -f "$OUT/raw.f32"; echo "wrote $OUT/mutable_$1.wav"
  else echo "built $OUT/mdrender"; fi
  ;;
clean)
  rm -rf "$OUT"; echo "removed $OUT"
  ;;
*)
  sed -n '2,12p' "$0"; exit 1
  ;;
esac
