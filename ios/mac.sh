#!/usr/bin/env bash
# mac.sh — build the AUv3 as a MAC CATALYST plug-in and prove it registered, so it can be
# hosted in GarageBand / Logic on this Mac. Phase 1 of docs/design/external-clock-sync.md's
# AUv3 arc; the spec is project-mac.yml (separate from project.yml on purpose — see its header).
#
#   zsh ios/mac.sh                 # stage acidcandy → build → register → auval
#   APP=pedalboard zsh ios/mac.sh  # a DIFFERENT APP's plug-in: its cart AND its AU identity
#   CART=epiano zsh ios/mac.sh     # a bare cart in the plug-in (keeps APP's identity)
#   RESIZABLE=1 APP=… zsh ios/mac.sh   # let a resizable cart REFLOW to the host's panel size
#   zsh ios/mac.sh --no-auval      # skip auval (still runs the host-transport gate)
#   ios/au-transport-check --free  # the NEGATIVE CONTROL: no transport blocks, must fail
#   ios/au-transport-check --loadable  # can a DAW load our code? (the gate GarageBand needed)
#
# ⚠ THE AU IDENTITY IS DERIVED FROM AN APP MANIFEST (ios/au-identity.sh), like testflight.sh and
# device.sh. This script was the LAST one hardcoding it: project-mac.yml's `tacj`/`Mpla` and a
# `grep -i "tacj\|Tiny Acid Jam"` + `auval -v aumu tacj Mpla` further down. That is worse here than
# a wrong name, because BOTH of those gates were pinned to Tiny Acid Jam's triple while `CART=` put
# whatever you asked for inside it — so auditioning another cart registered it under Tiny Acid Jam's
# FOREVER codes and then reported green on the wrong plug-in. APP= now picks both halves together.
# The component TYPE is derived too (`auType`, default aumu), so an EFFECT app (pedalboard, aumf)
# and an INSTRUMENT app (tinyacidjam, aumu) can share these specs without re-typing each other.
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

WANT_AUVAL=1
[ "${1:-}" = "--no-auval" ] && WANT_AUVAL=0

command -v xcodegen >/dev/null || { echo "✗ need xcodegen (brew install xcodegen)"; exit 1; }

# ── AU IDENTITY + CART, both from the app manifest ────────────────────────────────────────────────
# APP defaults to tinyacidjam so a bare `zsh ios/mac.sh` behaves exactly as it did before this was
# derived: acidcandy inside `aumu tacj Mpla` "Mipolai: Tiny Acid Jam". That default is the point —
# it means the hardcoding could be removed without changing a single existing invocation.
APP="${APP:-tinyacidjam}"
[ -f "../apps/$APP/app.json" ] || { echo "✗ no such app manifest: apps/$APP/app.json"; exit 1; }
. ./au-identity.sh
au_identity_load "$APP" || exit 1
au_carrier_load  "$APP" || exit 1
# EXPORTED for au-transport-check, which addresses the plug-in by this triple and the carrier by these
# paths, and used to hardcode all four — see the notes at the top of au-transport-check.swift for why a
# stale default is not benign.
export AU_SUBTYPE AU_MANUF AU_TYPE
export AU_CARRIER_APP="$CARRIER_APP_PATH" AU_APPEX_ID="$CARRIER_APPEX_ID"

# The cart in the plug-in: the manifest's auCart, else its first cart, else an explicit CART=.
# A single-cart app (pedalboard) sets no auCart, and for a LOCAL audition that should not be a
# blocker — auCart is the STORE opt-in (testflight.sh strips the AU target without it), and the
# question "is this worth shipping as a plug-in?" is exactly what you run this script to answer.
AU_CART="$(node -p "require('../apps/$APP/app.json').auCart || require('../apps/$APP/app.json').carts[0] || ''")"
CART="${CART:-$AU_CART}"
[ -n "$CART" ] || { echo "✗ apps/$APP/app.json names no cart (auCart or carts[0])"; exit 1; }
# ⚠ A MISMATCH IS THE BUG THIS SCRIPT USED TO HAVE SILENTLY, so say it out loud rather than refuse:
# CART= over an app is legitimate (drop any cart into a known-good plug-in shell to isolate whether
# a defect is the cart's or the AU's) — what is NOT acceptable is not KNOWING which cart the green
# gates below just judged, or which name it went into the machine's AU registry under.
if [ "$CART" != "$AU_CART" ]; then
  echo "⚠ CART=$CART is NOT $APP's cart ($AU_CART) — it will register as \"$AU_NAME\" ($AU_TYPE $AU_SUBTYPE $AU_MANUF)."
  echo "  Every gate below judges '$CART' while wearing $APP's identity. Deliberate? fine. Otherwise use APP=."
fi

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

# REFLOW, via the shared rule rather than a fourth copy of it (ios/app-flags.sh). A plug-in panel is
# the case that wants this most — §7 of docs/design/auv3-plugin-types.md: hosts resize the view to
# arbitrary shapes, sometimes a short wide strip — and TinyjamAUViewController's preferredContentSize
# is explicitly "a preference and not a constraint" because the cart is expected to reflow into it.
# app-flags' orientation-vs-reflow conflict cannot arise here: DE_ORIENT is a UIKit lock on the
# CARRIER APP's window, and the carrier is a throwaway that exists only to register the extension.
. ./app-flags.sh
DE_MIC_USAGE="$(node -p "require('../apps/$APP/app.json').micUsage || ''")"
export DE_MIC_USAGE
app_flags

echo "▸ deriving Mac spec (project-mac-dev.yml) — project-mac.yml is never rewritten…"
# A DERIVED COPY, exactly as device.sh does with project-dev.yml: the spec is shared by every app in
# a tree several agents work in at once, so an in-place sed is both a foreign edit and a race.
cp project-mac.yml project-mac-dev.yml
au_identity_apply project-mac-dev.yml || exit 1
au_carrier_apply  project-mac-dev.yml || exit 1

echo "▸ generating TinyjamMac.xcodeproj…"
xcodegen generate --spec project-mac-dev.yml >/dev/null

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
  ${ORIENT_SETTINGS[@]+"${ORIENT_SETTINGS[@]}"} \
  DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic \
  build 2>&1 | tail -6

# APP_BUNDLE, not APP: $APP is the app NAME (the manifest this build's identity came from) and
# reusing it here used to be harmless only because nothing read the name again afterwards.
APP_BUNDLE="$(ls -d "build-mac/Build/Products/$CONFIG-maccatalyst/$CARRIER_SLUG.app" 2>/dev/null || true)"
[ -n "$APP_BUNDLE" ] || { echo "✗ no .app produced — see the build output above"; exit 1; }
echo "▸ built $APP_BUNDLE"
if [ -d "$APP_BUNDLE/Contents/PlugIns" ]; then
  echo "▸ embedded plug-ins: $(ls "$APP_BUNDLE/Contents/PlugIns")"
else
  echo "✗ no PlugIns/ in the bundle — the extension did not embed"; exit 1
fi

# Install to ~/Applications before launching. Two reasons: `open` needs a real path (not `open -a`,
# which takes an app NAME and fails with "Unable to find application named"), and macOS is far more
# willing to register an app extension from a stable location than from deep inside derivedData.
# PER-APP DEST. This was ~/Applications/TinyjamMac.app for every app, with the rm -rf below, so each
# build DEREGISTERED the previous app's plug-in — silently, and looking like the plug-in had broken.
# The rm -rf stays (a stale bundle must not survive a rename inside it) but it now only ever destroys
# THIS app's own carrier. See au_carrier_load in ios/au-identity.sh.
DEST="$CARRIER_APP_PATH"
echo "▸ installing → $DEST"
mkdir -p "$HOME/Applications"
rm -rf "$DEST"
ditto "$APP_BUNDLE" "$DEST"          # ditto, not cp -R: preserves the code signature
echo "▸ launching once so macOS registers the extension…"
open "$DEST" || { echo "✗ could not launch the carrier app"; exit 1; }
sleep 8
osascript -e "tell application \"$CARRIER_SLUG\" to quit" >/dev/null 2>&1 || true

echo "▸ is it in the Audio Unit registry?"
if auval -a 2>/dev/null | grep -F "$AU_SUBTYPE $AU_MANUF"; then
  echo "  ✓ registered"
else
  echo "  ✗ NOT registered — macOS has not picked up the extension."
  echo "    Usual causes: the app has not been launched from a stable location (try moving the"
  echo "    .app to /Applications), or the ad-hoc signature is rejected for extension loading."
  exit 1
fi

if [ "$WANT_AUVAL" = "1" ]; then
  echo "▸ auval -v $AU_TYPE $AU_SUBTYPE $AU_MANUF  (Apple's validator)"
  auval -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANUF" 2>&1 | tail -25
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
# ⚠ THE COMPILE BELONGS ABOVE THE FIRST GATE THAT RUNS THE BINARY, not next to the second one.
# It used to sit below --loadable, which meant this gate ran the PREVIOUS build of its own source:
# edit --loadable, run mac.sh, and you have gated the old code — green, and blind. On a fresh clone
# it fails differently and louder (the binary is gitignored, so `set -euo pipefail` kills the script
# here before any gate runs at all). Keep every ./au-transport-check invocation after this line.
xcrun swiftc -O -o au-transport-check au-transport-check.swift -framework AVFoundation -framework CoreAudioKit
./au-transport-check --loadable

echo "▸ host-transport gate (ios/au-transport-check.swift)"
./au-transport-check
# Again at 48k. The engine is compile-time 44.1k, so this is NOT a duplicate run: it guards the
# property that the sequencer stays on the HOST's grid at any rate (the step comes from sync_beats,
# which the rate never touches). Both runs pass. PITCH at 48k is a separate question and is gated by
# the rate-converter run at the top of this file — NOT by a flag here: `--pitch` was deleted, because
# its zero-crossing estimator moved with the sample rate and so reported the rate ratio as if it were
# a detuning. au-transport-check.swift's header tells that story in full; rate-convert-check.swift is
# the oracle that replaced it (a 220 Hz sine, where the answer is 220.000 at every rate).
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

# The host's CONTINUOUS controls. Until 2026-08-14 the AU's event switch handled only note-on/off and
# pitch-bend, so NO CC reached the engine — the mod wheel AND every DAW automation lane were dead.
echo "▸ mod-wheel gate (--wheel)"
./au-transport-check --wheel

# the PARAMETER gate: a DAW seeing/riding/reading the rack's knobs (docs/design/host-parameters.md).
# The only place the whole chain is proven through a real out-of-process plug-in — cart param_bind →
# the engine table → the seam → AUParameterTree. Headless gates stop at the engine.
echo "▸ parameter gate (--params)"
./au-transport-check --params

echo "  (negative control: ./au-transport-check --free must FAIL the tempo check — ratio ~0.5)"
