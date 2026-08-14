#!/usr/bin/env bash
# mac.sh — build the AUv3 as a MAC CATALYST plug-in and prove it registered, so it can be
# hosted in GarageBand / Logic on this Mac. Phase 1 of docs/design/external-clock-sync.md's
# AUv3 arc; the spec is project-mac.yml (separate from project.yml on purpose — see its header).
#
#   zsh ios/mac.sh                 # stage acidcandy → build → register → auval
#   CART=epiano zsh ios/mac.sh     # a different cart in the plug-in
#   zsh ios/mac.sh --no-auval      # skip auval (still runs the host-transport gate)
#   ios/au-transport-check --free  # the NEGATIVE CONTROL: no transport blocks, must fail
#   ios/au-transport-check --loadable  # can a DAW load our code? (the gate GarageBand needed)
#
# WHAT "REGISTER" MEANS: on macOS an AUv3 lives in an app bundle's PlugIns/ and the system only
# learns about it when that app is LAUNCHED ONCE. So this opens the app, waits, and quits it. The
# app is a carrier; the extension is the product.
#
# WHY auval: it is Apple's own AU validator (/usr/bin/auval), and it exercises the plug-in far
# harder than a DAW does — instantiating repeatedly, rendering at multiple sample rates and buffer
# sizes. Two of our known risks show up here FIRST, before GarageBand is ever opened:
#   · the engine is compile-time 44.1 kHz (SOUND_SAMPLE_RATE in sound.h, and the render block's
#     735 samples/frame). A host at 48 kHz would run the sequencer at the wrong rate.
#   · one engine per PROCESS (studio.c file-scope globals). On macOS, unlike iOS, a host MAY load
#     several AUv3 instances in one process — auval's repeated instantiation pokes at this.
set -euo pipefail
cd "$(dirname "$0")"

CART="${CART:-acidcandy}"
WANT_AUVAL=1
[ "${1:-}" = "--no-auval" ] && WANT_AUVAL=0

command -v xcodegen >/dev/null || { echo "✗ need xcodegen (brew install xcodegen)"; exit 1; }

# Stage into gen/mac + gen/macau, NOT the iOS spec's gen/app + gen/au: those are shared with
# whatever iOS build another agent is running in this tree, and gen/app can legitimately hold a
# MULTI-cart staging (app_main.c + per-cart TUs) that a lone cart.c duplicate-symbols against.
echo "▸ staging cart '$CART' → gen/mac + gen/macau…"
# The generate-then-copy is a RACE, and it has already bitten: play.js writes ../build/cart.c, that
# path is shared with every other agent in this tree, and a sibling compiling its own cart in the gap
# leaves us copying THEIRS. It cost a baffling red gate (the plug-in rendered silence at 44.1k because
# it was hosting `tenement`), and unnoticed it would sign the wrong cart into a shipping bundle. So:
# generate, VERIFY the slug we got is the cart we asked for, and retry a few times before giving up.
staged=0
for attempt in 1 2 3; do
  ( cd .. && node tools/play.js "$CART" run --headless --frames 1 >/dev/null 2>&1 ) \
    || { echo "  attempt $attempt: cart '$CART' generation failed"; sleep 2; continue; }
  got=$(grep -m1 '"slug"' ../build/cart.c | sed 's/.*"slug"[^"]*"\([^"]*\)".*/\1/')
  if [ "$got" = "$CART" ]; then staged=1; break; fi
  echo "  attempt $attempt: build/cart.c holds '$got', not '$CART' — another agent won the race; retrying"
  sleep 2
done
[ "$staged" = "1" ] || { echo "✗ could not stage '$CART' (build/cart.c kept coming back as someone else's)"; exit 1; }
for d in gen/mac gen/macau; do
  mkdir -p "$d"
  # `find -delete`, not `rm -f "$d"/*.c`: this file is invoked as `zsh ios/mac.sh`, and zsh ABORTS
  # on a glob with no matches ("no matches found") where bash would pass it through — so the very
  # first run, with the dir freshly created and empty, killed the script.
  find "$d" -maxdepth 1 -type f \( -name '*.c' -o -name '*.h' \) -delete
  cp ../build/cart.c ../build/sprites_data.h ../build/map_data.h "$d/"
done

# Cart dims → the same -D override the iOS scripts pass (studio.h has #ifndef fallbacks, but a
# mismatch between the app and the extension would give them different canvases).
DIMS=$(cd .. && node -e '
const mk = require("./tools/make-cart.js");
const c = mk.loadConfig("tools/carts/'"$CART"'.c");
process.stdout.write(`${c.screenW ?? 320} ${c.screenH ?? 200}`)')
SW=$(echo "$DIMS" | cut -d" " -f1); SH=$(echo "$DIMS" | cut -d" " -f2)
DEFS="\$(inherited) DE_NO_RAYLIB=1 SCALE=1 SCREEN_W=$SW SCREEN_H=$SH"
echo "▸ cart dims ${SW}x${SH}"

echo "▸ generating TinyjamMac.xcodeproj…"
xcodegen generate --spec project-mac.yml >/dev/null

# REAL signing, not ad-hoc. macOS refuses to REGISTER an app extension from an ad-hoc-signed app:
# the build succeeds, the .appex embeds, and the system then silently ignores it (codesign shows
# Signature=adhoc / TeamIdentifier=not set, and pluginkit lists nothing). That cost a whole round.
TEAM="${TEAM:-JH2ZCZH58D}"
# RELEASE by default. This built Debug until 2026-08-13, and for a plug-in that software-rasterises a
# whole UI that is not a detail: -O0 makes the frame 3-4x more expensive (0.4-0.9 ms vs 0.13-0.25 ms
# measured on acidcandy), and the frame used to run on the audio thread. `CONFIG=Debug zsh ios/mac.sh`
# when you actually want to attach a debugger.
CONFIG="${CONFIG:-Release}"
echo "▸ building for Mac Catalyst, $CONFIG, signed (team $TEAM)…"
xcodebuild -project TinyjamMac.xcodeproj -scheme TinyjamMac \
  -destination 'platform=macOS,variant=Mac Catalyst' -configuration "$CONFIG" \
  -derivedDataPath build-mac -allowProvisioningUpdates \
  GCC_PREPROCESSOR_DEFINITIONS="$DEFS" \
  DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic \
  build 2>&1 | tail -6

APP="$(ls -d "build-mac/Build/Products/$CONFIG-maccatalyst/TinyjamMac.app" 2>/dev/null || true)"
[ -n "$APP" ] || { echo "✗ no .app produced — see the build output above"; exit 1; }
echo "▸ built $APP"
if [ -d "$APP/Contents/PlugIns" ]; then
  echo "▸ embedded plug-ins: $(ls "$APP/Contents/PlugIns")"
else
  echo "✗ no PlugIns/ in the bundle — the extension did not embed"; exit 1
fi

# Install to ~/Applications before launching. Two reasons: `open` needs a real path (not `open -a`,
# which takes an app NAME and fails with "Unable to find application named"), and macOS is far more
# willing to register an app extension from a stable location than from deep inside derivedData.
DEST="$HOME/Applications/TinyjamMac.app"
echo "▸ installing → $DEST"
mkdir -p "$HOME/Applications"
rm -rf "$DEST"
ditto "$APP" "$DEST"          # ditto, not cp -R: preserves the code signature
echo "▸ launching once so macOS registers the extension…"
open "$DEST" || { echo "✗ could not launch the carrier app"; exit 1; }
sleep 8
osascript -e 'tell application "TinyjamMac" to quit' >/dev/null 2>&1 || true

echo "▸ is it in the Audio Unit registry?"
if auval -a 2>/dev/null | grep -i "tacj\|Tiny Acid Jam"; then
  echo "  ✓ registered"
else
  echo "  ✗ NOT registered — macOS has not picked up the extension."
  echo "    Usual causes: the app has not been launched from a stable location (try moving the"
  echo "    .app to /Applications), or the ad-hoc signature is rejected for extension loading."
  exit 1
fi

if [ "$WANT_AUVAL" = "1" ]; then
  echo "▸ auval -v aumu tacj Mpla  (Apple's validator)"
  auval -v aumu tacj Mpla 2>&1 | tail -25
fi

# HOST TRANSPORT gate. auval cannot cover this: it never SETS musicalContextBlock, so it only ever
# exercises the "host supplies no transport" path. This is a tiny AUv3 HOST that does set it.
# SAMPLE-RATE gate. Runs first because it needs no plug-in, no host and no cart: a 220 Hz sine
# straight through the real AU/RateConvert.swift. The engine is compile-time 44.1k and an AUv3 gets
# called at the HOST's rate, so this is what keeps the rack in tune at 48k. See ios-plan.md.
echo "▸ rate-converter gate (ios/rate-convert-check.swift)"
xcrun swiftc -O -o rate-convert-check rate-convert-check.swift AU/RateConvert.swift
./rate-convert-check | tail -3

# LOADABILITY gate, and it runs FIRST because it is the cheapest and the only one that can see the
# fatal class: on 2026-08-13 all five gates below were green while GarageBand refused to open the
# plug-in entirely (orange !), because AudioComponentBundle named a Mac Catalyst framework and a native
# host cannot dlopen Catalyst code. Everything else here instantiates through AVAudioUnit, which
# SILENTLY FALLS BACK to out-of-process when in-process loading fails — so the plug-in kept working for
# us and died in the DAW. This one instantiates nothing; it checks the declaration is honest.
echo "▸ loadability gate (--loadable) — can a DAW load our code at all?"
./au-transport-check --loadable

echo "▸ host-transport gate (ios/au-transport-check.swift)"
xcrun swiftc -O -o au-transport-check au-transport-check.swift -framework AVFoundation -framework CoreAudioKit
./au-transport-check
# Again at 48k. The engine is compile-time 44.1k, so this is NOT a duplicate run: it guards the
# property that the sequencer stays on the HOST's grid at any rate (the step comes from sync_beats,
# which the rate never touches). Both runs pass; what 48k DOES break is pitch, measured separately by
# ./au-transport-check --pitch — deliberately not run here, see ios-plan.md "the sample-rate risk".
./au-transport-check --rate 48000
# VIEW gate (phase 3). Narrow on purpose: it proves the UI extension is WIRED — the -UI extension
# point, an AUViewController that is also the factory, and the view loading when a host asks. Get any
# of that wrong and the plug-in still passes auval and still plays, while every DAW quietly shows its
# own generic sliders. Whether the picture is RIGHT needs eyes in GarageBand; the pixel path itself is
# gated by tools/present-race-check.
# ...and once PACED TO THE WALL CLOCK, which is the only mode that exercises the FRAME WORKER. The
# runs above take the inline path, because AVAudioEngine's manual rendering declares itself offline.
# Costs ~12s and it is worth it: the worker is what keeps the cart's update+draw off the audio thread.
echo "▸ frame-worker gate (--realtime, ~12s)"
./au-transport-check --realtime

echo "▸ plug-in view gate (--view)"
./au-transport-check --view

# PANEL gate. --view proves a host can OBTAIN the view; this proves the view is attached to the audio
# unit that RENDERS — the question the whole "out-of-process wall" fork turned on, and which was
# previously answered by a diagnostic whose "connected" branch was unreachable. Reads the extension's
# own verdict back out of the unified log, and requires BOTH verdicts in one run so the check is known
# to be able to go red. ~15s (it waits for the extension's re-reads).
echo "▸ panel-is-the-audible-engine gate (--panel, ~15s)"
./au-transport-check --panel

# The Swift half of session state. tools/state-check/run.sh gates the ENGINE half far more thoroughly,
# but it runs the desktop DE_NO_RAYLIB build and so never touches TinyjamAU's fullState — nor the thing
# that only a real host reveals: the dictionary is written into the project file as a property list.
echo "▸ session-state gate (--state)"
./au-transport-check --state

echo "  (negative control: ./au-transport-check --free must FAIL the tempo check — ratio ~0.5)"
