#!/usr/bin/env bash
# Tinyjam iOS — the whole agentic build loop in one command. No Xcode GUI, no
# signing, no Apple-account interaction (simulator only). Proven in spike 0.
#
#   ./build.sh                                  # generate, build, boot, launch
#   DEVICE="iPhone SE (3rd generation)" ./build.sh
#   SHOT=snap.png ./build.sh                    # also screenshot to snap.png
#
# Requires: active full Xcode (xcode-select -p must point at Xcode.app, NOT the
# Command Line Tools) + xcodegen (brew install xcodegen).
set -euo pipefail
cd "$(dirname "$0")"

DEVICE="${DEVICE:-iPhone 15}"
SHOT="${SHOT:-}"
SCHEME="TinyjamHello"
BUNDLE_ID="com.tinyjam.hello"
CART="${CART:-omnichord}"       # the standalone app's cart (touch instrument)
AU_CART="${AU_CART:-epiano}"    # the AUv3 extension's cart (a keybed instrument played by host MIDI)

# Phase 2: stage each target's cart — gen/<dir>/{cart.c,sprites_data.h,map_data.h}, generated
# from tools/carts/<name>.c via play.js. The app and the AUv3 host DIFFERENT carts, so each gets
# its own staged copy (studio.c #includes the data headers by fixed name; separate dirs avoid a
# collision). The "swap a cart" loop, extended to iOS: change CART / AU_CART, re-run.
stage_cart() {   # $1 = cart name, $2 = gen subdir
  echo "▸ staging cart '$1' → gen/$2/…"
  ( cd .. && node tools/play.js "$1" run --headless --frames 1 >/dev/null 2>&1 ) \
    || { echo "✗ cart '$1' generation failed — run: node tools/play.js $1 run --headless --frames 1"; exit 1; }
  mkdir -p "gen/$2"
  cp ../build/cart.c ../build/sprites_data.h ../build/map_data.h "gen/$2/"
}
stage_cart "$CART"    app
stage_cart "$AU_CART" au

echo "▸ generating xcodeproj from project.yml…"
xcodegen generate --spec project.yml >/dev/null

echo "▸ building for simulator (no signing)…"
xcodebuild -project "$SCHEME.xcodeproj" -scheme "$SCHEME" \
  -sdk iphonesimulator -configuration Debug \
  -derivedDataPath build CODE_SIGNING_ALLOWED=NO build >/dev/null
APP="build/Build/Products/Debug-iphonesimulator/$SCHEME.app"

echo "▸ booting '$DEVICE'…"
xcrun simctl boot "$DEVICE" 2>/dev/null || true
xcrun simctl bootstatus "$DEVICE" -b >/dev/null

echo "▸ installing + launching ${BUNDLE_ID}…"
xcrun simctl install "$DEVICE" "$APP"
xcrun simctl launch "$DEVICE" "$BUNDLE_ID"

if [ -n "$SHOT" ]; then
  sleep 2
  xcrun simctl io "$DEVICE" screenshot "$SHOT"
  echo "▸ screenshot → $SHOT"
fi
echo "✓ done"
