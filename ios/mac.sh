#!/usr/bin/env bash
# mac.sh — build the AUv3 as a MAC CATALYST plug-in and prove it registered, so it can be
# hosted in GarageBand / Logic on this Mac. Phase 1 of docs/design/external-clock-sync.md's
# AUv3 arc; the spec is project-mac.yml (separate from project.yml on purpose — see its header).
#
#   zsh ios/mac.sh                 # stage acidcandy → build → register → auval
#   CART=epiano zsh ios/mac.sh     # a different cart in the plug-in
#   zsh ios/mac.sh --no-auval      # skip validation (just build + register)
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
( cd .. && node tools/play.js "$CART" run --headless --frames 1 >/dev/null 2>&1 ) \
  || { echo "✗ cart '$CART' generation failed"; exit 1; }
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
echo "▸ building for Mac Catalyst, signed (team $TEAM)…"
xcodebuild -project TinyjamMac.xcodeproj -scheme TinyjamMac \
  -destination 'platform=macOS,variant=Mac Catalyst' -configuration Debug \
  -derivedDataPath build-mac -allowProvisioningUpdates \
  GCC_PREPROCESSOR_DEFINITIONS="$DEFS" \
  DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic \
  build 2>&1 | tail -6

APP="$(ls -d build-mac/Build/Products/Debug-maccatalyst/TinyjamMac.app 2>/dev/null || true)"
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
  echo "▸ auval -v aumu tacj Mpla  (Apple's validator — the real gate)"
  auval -v aumu tacj Mpla 2>&1 | tail -25
fi
