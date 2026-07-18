#!/usr/bin/env bash
# Build, sign, and run the app on a connected physical iPhone/iPad — no Xcode GUI.
# Uses ios-deploy (classic protocol) because devicectl/CoreDevice doesn't enroll older
# devices (e.g. iOS 15) without a GUI "prepare" step; ios-deploy just works over USB.
#
# EXPECTED, HARMLESS: on a new Xcode (26+) ios-deploy prints
#   "Unable to locate DeviceSupport directory with suffix 'Symbols' … logging output will
#    not be shown!"
# ios-deploy is abandonware (1.12.2, 2022 = the last release) and predates Xcode 26's
# DeviceSupport/Symbols layout, so it can't find the symbols dir. It does NOT block install
# or launch — and we launch with --justlaunch (no log streaming), so the missing symbols
# cost nothing here. Don't chase it. If you ever need device LOGS, skip ios-deploy for that:
# Console.app (filter by app name) or Xcode → Devices & Simulators → View Device Logs.
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
  # DERIVE the cart's screen/cell/map dims from its de:settings so the -D override below matches
  # WITHOUT hand-passing DE_* (mirrors the APP path's gen/app.dims). Explicit DE_* env still wins;
  # a cart with no settings chunk → cart-info exits 3, we keep the 320×200 default.
  if [ -z "${DE_SCREEN_W:-}" ]; then
    D="$( cd .. && node tools/cart-info.js "$CART" --dims 2>/dev/null || true )"
    [ -n "$D" ] && { set -a; eval "$D"; set +a; }
  fi
fi
stage_cart "$AU_CART" au

# cart dims → preprocessor override (replaces project.yml's; default = the standard 320×200 console).
# SCALE=1 keeps touches 1:1 with framebuffer pixels. Applies to both targets; harmless for the AU.
SW="${DE_SCREEN_W:-320}"; SH="${DE_SCREEN_H:-200}"
MW="${DE_MAP_W:-128}"; MH="${DE_MAP_H:-64}"; CWv="${DE_CELL_W:-16}"; CHv="${DE_CELL_H:-16}"
DEFS="\$(inherited) DE_NO_RAYLIB=1 SCREEN_W=$SW SCREEN_H=$SH SCALE=1 MAP_W=$MW MAP_H=$MH CELL_W=$CWv CELL_H=$CHv"
# RESIZABLE=1: build with -DDE_RESIZABLE so a resizable cart reflows to fill the device viewport
# (CanvasView calls de_resize) — same flag build.sh uses. e.g. RESIZABLE=1 CART=acidwire ./device.sh
[ -n "${RESIZABLE:-}" ] && DEFS="$DEFS DE_RESIZABLE=1"

CONFIG="${CONFIG:-Debug}"   # CONFIG=Release for the optimized engine (real perf; no #if DEBUG perf overlay)

# actool needs an AppIcon set (project.yml sets APPICON_NAME=AppIcon, and the optional
# catalog reference is always emitted). APP builds with a manifest "icon" already staged
# gen/Assets.xcassets; otherwise (single cart / editor / icon-less app) stage the repo's
# default placeholder so CompileAssetCatalog succeeds for ANY cart.
if [ ! -f gen/Assets.xcassets/AppIcon.appiconset/Contents.json ]; then
  mkdir -p gen/Assets.xcassets/AppIcon.appiconset
  printf '{ "info": { "author": "xcode", "version": 1 } }\n' > gen/Assets.xcassets/Contents.json
  cp default-icon.png gen/Assets.xcassets/AppIcon.appiconset/icon-1024.png
  printf '{ "images": [{ "filename": "icon-1024.png", "idiom": "universal", "platform": "ios", "size": "1024x1024" }], "info": { "author": "xcode", "version": 1 } }\n' > gen/Assets.xcassets/AppIcon.appiconset/Contents.json
fi

echo "▸ generating + building (signed for device, $CONFIG, ${SW}x${SH})…"
xcodegen generate --spec project.yml >/dev/null
xcodebuild -project "$SCHEME.xcodeproj" -scheme "$SCHEME" -configuration "$CONFIG" \
  -destination 'generic/platform=iOS' -derivedDataPath build \
  GCC_PREPROCESSOR_DEFINITIONS="$DEFS" \
  -allowProvisioningUpdates DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic build >/dev/null

echo "▸ installing + launching on device…"
ios-deploy --id "$DEVICE_ID" --bundle "build/Build/Products/$CONFIG-iphoneos/$SCHEME.app" --justlaunch
echo "✓ running on device ($CONFIG)"
