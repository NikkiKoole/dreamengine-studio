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
  mkdir -p "gen/$2"; cp ../build/cart.c ../build/sprites_data.h ../build/map_data.h "gen/$2/"
}
echo "▸ staging carts (gen/app=$CART, gen/au=$AU_CART)…"
stage_cart "$CART" app
stage_cart "$AU_CART" au

echo "▸ generating + building (signed for device)…"
xcodegen generate --spec project.yml >/dev/null
xcodebuild -project "$SCHEME.xcodeproj" -scheme "$SCHEME" -configuration Debug \
  -destination 'generic/platform=iOS' -derivedDataPath build \
  -allowProvisioningUpdates DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic build >/dev/null

echo "▸ installing + launching on device…"
ios-deploy --id "$DEVICE_ID" --bundle "build/Build/Products/Debug-iphoneos/$SCHEME.app" --justlaunch
echo "✓ running on device"
