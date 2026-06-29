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
