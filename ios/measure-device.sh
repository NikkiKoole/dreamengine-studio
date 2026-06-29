#!/usr/bin/env bash
# measure-device.sh — on-device renderer FPS measurement (the renderer-decision gate).
# Stages a cart as the APP cart, builds signed for the connected iPhone, deploys, and streams
# the device console for a few seconds capturing CanvasView's [perf] lines (engine de_frame()
# time, blit time, actual displaylink fps). The numbers feed docs/design/engine-portability.md.
#
#   ./measure-device.sh <cart> [seconds]      # e.g. ./measure-device.sh podracer 10
# Prereqs: same as device.sh (signing cert, ios-deploy, iPhone connected + unlocked).
set -euo pipefail
cd "$(dirname "$0")"

CART="${1:?usage: ./measure-device.sh <cart> [seconds]}"
SECS="${2:-10}"
TEAM="${TEAM:-JH2ZCZH58D}"
SCHEME="TinyjamHello"
DEVICE_ID="${DEVICE_ID:-$(xcrun xctrace list devices 2>&1 \
  | sed -n '/== Devices ==/,/== Simulators ==/p' \
  | awk -F'[()]' '/iPhone|iPad/ {print $4; exit}')}"
[ -z "$DEVICE_ID" ] && { echo "no physical device found — connect + unlock it"; exit 1; }

stage() {  # $1 cart, $2 gen subdir
  ( cd .. && node tools/play.js "$1" run --headless --frames 1 >/dev/null 2>&1 ) \
    || { echo "✗ cart '$1' generation failed"; exit 1; }
  mkdir -p "gen/$2"; cp ../build/cart.c ../build/sprites_data.h ../build/map_data.h "gen/$2/"
}
echo "▸ staging app cart '$CART' (+ keeping an AU cart)…"
stage "$CART" app
[ -f gen/au/cart.c ] || stage epiano au

echo "▸ building signed for device…"
xcodegen generate --spec project.yml >/dev/null
xcodebuild -project "$SCHEME.xcodeproj" -scheme "$SCHEME" -configuration Debug \
  -destination 'generic/platform=iOS' -derivedDataPath build \
  -allowProvisioningUpdates DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic build >/dev/null

BUNDLE_ID="com.tinyjam.hello"
echo "▸ launching on device, letting it run ${SECS}s (cart=$CART)…"
ios-deploy --id "$DEVICE_ID" --bundle "build/Build/Products/Debug-iphoneos/$SCHEME.app" \
  --justlaunch >/dev/null 2>&1
sleep "$SECS"                          # the app logs one [perf] line/sec to Documents/perf.log

echo "▸ pulling Documents/perf.log off the device…"
DL="build/dl-$CART"; rm -rf "$DL"; mkdir -p "$DL"
ios-deploy --id "$DEVICE_ID" --bundle_id "$BUNDLE_ID" --download=/Documents/perf.log --to "$DL" >/dev/null 2>&1 || true
PERF="$DL/Documents/perf.log"

echo "▸ [perf] lines for $CART:"
if [ -s "$PERF" ]; then grep -a "\[perf\]" "$PERF" | sed 's/.*\[perf\]/[perf]/'
else echo "  (none captured — perf.log empty/missing at $PERF)"; fi
