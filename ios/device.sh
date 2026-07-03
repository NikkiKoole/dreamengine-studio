#!/usr/bin/env bash
# Build, sign, and run the app on a connected physical iPhone/iPad — no Xcode GUI.
# Uses ios-deploy (classic protocol) because devicectl/CoreDevice doesn't enroll older
# devices (e.g. iOS 15) without a GUI "prepare" step; ios-deploy just works over USB.
#
#   ./device.sh                       # build signed → install → launch on the device
#   TEAM=XXXXXXXXXX ./device.sh        # override the signing team
#
# One-time prerequisites:
#   - a signing cert in the keychain: Xcode → Settings → Accounts → add your Apple ID
#     (the cert is minted on first signed build; this script triggers that).
#   - brew install ios-deploy
#   - iPhone connected + UNLOCKED (and "Trust This Computer" accepted once).
# First launch may need: iPhone → Settings → General → VPN & Device Management → trust the
# "Apple Development: <you>" profile.
set -euo pipefail
cd "$(dirname "$0")"

TEAM="${TEAM:-JH2ZCZH58D}"
SCHEME="TinyjamHello"
CART="${CART:-omnichord}"        # the standalone app's cart
AU_CART="${AU_CART:-epiano}"     # the AUv3 extension's cart
DEVICE_ID="${DEVICE_ID:-$(xcrun xctrace list devices 2>&1 \
  | sed -n '/== Devices ==/,/== Simulators ==/p' \
  | awk -F'[()]' '/iPhone|iPad/ {print $4; exit}')}"

[ -z "$DEVICE_ID" ] && { echo "no physical device found — connect + unlock it"; exit 1; }
echo "▸ device: $DEVICE_ID  ·  team: $TEAM  ·  cart: $CART (AU: $AU_CART)"

# stage each target's cart (same as build.sh — the gen/ dirs project.yml references)
stage_cart() {
  ( cd .. && node tools/play.js "$1" run --headless --frames 1 >/dev/null 2>&1 ) \
    || { echo "✗ cart '$1' generation failed"; exit 1; }
  mkdir -p "gen/$2"; rm -f "gen/$2"/*.c   # clear any stale multi-cart wrappers first
  cp ../build/cart.c ../build/sprites_data.h ../build/map_data.h "gen/$2/"
}
# EDITOR=1: deploy the editor's LIVE buffer — the editor already wrote build/{cart.c,sprites_data.h,
# map_data.h} (prepareCart), so use those as the app cart instead of re-staging a saved cart via play.js.
# The dims come in via DE_* env (any size — see GCC_PREPROCESSOR_DEFINITIONS below). The AU is always a
# saved cart (epiano); it's audio-only so its dims don't matter.
# APP=<manifest>: deploy a MULTI-CART app (apps/<name>/app.json) — build-app.js --ios stages
# the dispatcher shim + per-cart wrappers + baked data into gen/app, and writes gen/app.dims
# (the app's screen/grid size) which we source so the -D override below matches.
if [ -n "${APP:-}" ]; then
  echo "▸ staging MULTI-CART app '$APP' → gen/app…"
  ( cd .. && node tools/build-app.js "$APP" --ios ) || { echo "✗ build-app.js --ios failed"; exit 1; }
  [ -f gen/app.dims ] && { set -a; . ./gen/app.dims; set +a; }
elif [ -n "${EDITOR:-}" ]; then
  echo "▸ staging editor cart from build/ → gen/app…"
  mkdir -p gen/app; rm -f gen/app/*.c
  cp ../build/cart.c ../build/sprites_data.h ../build/map_data.h gen/app/ \
    || { echo "✗ no editor build/ output to deploy"; exit 1; }
else
  echo "▸ staging carts (gen/app=$CART, gen/au=$AU_CART)…"
  stage_cart "$CART" app
fi
stage_cart "$AU_CART" au

# cart dims → preprocessor override (replaces project.yml's; default = the standard 320×200 console).
# SCALE=1 keeps touches 1:1 with framebuffer pixels. Applies to both targets; harmless for the AU.
SW="${DE_SCREEN_W:-320}"; SH="${DE_SCREEN_H:-200}"
MW="${DE_MAP_W:-128}"; MH="${DE_MAP_H:-64}"; CWv="${DE_CELL_W:-16}"; CHv="${DE_CELL_H:-16}"
DEFS="\$(inherited) DE_NO_RAYLIB=1 SCREEN_W=$SW SCREEN_H=$SH SCALE=1 MAP_W=$MW MAP_H=$MH CELL_W=$CWv CELL_H=$CHv"

CONFIG="${CONFIG:-Debug}"   # CONFIG=Release for the optimized engine (real perf; no #if DEBUG perf overlay)
echo "▸ generating + building (signed for device, $CONFIG, ${SW}x${SH})…"
xcodegen generate --spec project.yml >/dev/null
xcodebuild -project "$SCHEME.xcodeproj" -scheme "$SCHEME" -configuration "$CONFIG" \
  -destination 'generic/platform=iOS' -derivedDataPath build \
  GCC_PREPROCESSOR_DEFINITIONS="$DEFS" \
  -allowProvisioningUpdates DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic build >/dev/null

echo "▸ installing + launching on device…"
ios-deploy --id "$DEVICE_ID" --bundle "build/Build/Products/$CONFIG-iphoneos/$SCHEME.app" --justlaunch
echo "✓ running on device ($CONFIG)"
