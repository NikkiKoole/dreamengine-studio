#!/usr/bin/env bash
# mac-app.sh — bundle a dreamengine cart binary into a signed, notarized .app that
# opens on ANY Mac (no Gatekeeper "unidentified developer" / "damaged" warning).
#
# The bare exported binary fails on someone else's Mac because it's unsigned and
# not notarized (and a plain Mach-O double-clicks into Terminal). This wraps it:
#   .app bundle → codesign (Developer ID + hardened runtime) → notarize → staple.
#
# Prerequisites (one-time PER MACHINE, need YOUR Apple login — see docs/guides or the chat):
#   1. A "Developer ID Application" certificate in your keychain
#        No-Xcode route (proven 2026-07-03): Keychain Access → Certificate Assistant →
#        Request a Certificate From a CA… → "Saved to disk", then developer.apple.com/account
#        → Certificates → + → "Developer ID Application" → upload the CSR → download the .cer.
#        (Or: Xcode → Settings → Accounts → Manage Certificates → +. Requires paid Program.)
#        The private key lives on the machine that made the CSR — a .cer (or a p12 exported
#        from a Mac without the key) will NOT form an identity elsewhere; mint a cert per
#        machine instead (several can coexist). If Keychain Access errors on import (-25294),
#        use the CLI:  security import <file> -k ~/Library/Keychains/login.keychain-db
#      Verify:  security find-identity -v | grep "Developer ID Application"
#   2. An app-specific password: appleid.apple.com → Sign-In & Security
#        → App-Specific Passwords → +   (e.g. label it "notarytool")
#   3. Store notarization creds once (uses #2 + your Team ID):
#        xcrun notarytool store-credentials dreamengine-notary \
#          --apple-id "you@example.com" --team-id JH2ZCZH58D --password <app-specific-pw>
#
# Usage:
#   tools/mac-app.sh <binary> [--name "Pretty Name"] [--id com.you.cart] \
#                    [--profile dreamengine-notary] [--out <dir>] [--no-notarize] \
#                    [--icon <file.icns>]
#
# Icon: every app gets the shared dreamengine icon (tools/assets/mac-app.icns,
# regenerate via tools/assets/make-mac-icon.js) unless --icon points elsewhere.
#
# Env overrides: DEV_ID (signing identity), NOTARY_PROFILE, BUNDLE_ID_PREFIX.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="${1:-build/cart}"; shift || true
NAME=""; BUNDLE_ID=""; PROFILE="${NOTARY_PROFILE:-dreamengine-notary}"
OUT="."; NOTARIZE=1; ICON="$SCRIPT_DIR/assets/mac-app.icns"
while [ $# -gt 0 ]; do
  case "$1" in
    --name)    NAME="$2";      shift 2 ;;
    --id)      BUNDLE_ID="$2"; shift 2 ;;
    --profile) PROFILE="$2";   shift 2 ;;
    --out)     OUT="$2";       shift 2 ;;
    --icon)    ICON="$2";      shift 2 ;;
    --no-notarize) NOTARIZE=0; shift ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

[ -f "$BIN" ] || { echo "✗ binary not found: $BIN" >&2; exit 1; }
[ -z "$NAME" ] && NAME="$(basename "$BIN")"
SLUG="$(echo "$NAME" | tr '[:upper:] ' '[:lower:]-' | tr -cd 'a-z0-9-')"
[ -z "$BUNDLE_ID" ] && BUNDLE_ID="${BUNDLE_ID_PREFIX:-com.dreamengine}.${SLUG:-cart}"

# Signing identity: explicit DEV_ID, else the first Developer ID Application cert.
if [ -z "${DEV_ID:-}" ]; then
  DEV_ID="$(security find-identity -v | sed -n 's/.*"\(Developer ID Application:[^"]*\)".*/\1/p' | head -1)"
fi
if [ -z "${DEV_ID:-}" ]; then
  echo "✗ No 'Developer ID Application' certificate found in the keychain." >&2
  echo "  Create one: Xcode → Settings → Accounts → Manage Certificates → + → Developer ID Application." >&2
  echo "  (You currently have only:)" >&2
  security find-identity -v | sed 's/^/    /' >&2
  exit 1
fi

APP="$OUT/$NAME.app"
echo "── packaging $NAME.app  (id $BUNDLE_ID) ──"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BIN" "$APP/Contents/MacOS/$SLUG"
chmod +x "$APP/Contents/MacOS/$SLUG"
HAVE_ICON=0
if [ -f "$ICON" ]; then
  cp "$ICON" "$APP/Contents/Resources/AppIcon.icns"; HAVE_ICON=1
else
  echo "   (no icon at $ICON — app ships without one)"
fi

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>            <string>$NAME</string>
  <key>CFBundleDisplayName</key>     <string>$NAME</string>
  <key>CFBundleExecutable</key>      <string>$SLUG</string>
  <key>CFBundleIdentifier</key>      <string>$BUNDLE_ID</string>
  <key>CFBundlePackageType</key>     <string>APPL</string>
  <key>CFBundleVersion</key>         <string>1.0</string>
  <key>CFBundleShortVersionString</key> <string>1.0</string>
  <key>LSMinimumSystemVersion</key>  <string>10.13</string>
  <key>NSHighResolutionCapable</key> <true/>
$([ "$HAVE_ICON" -eq 1 ] && echo '  <key>CFBundleIconFile</key>       <string>AppIcon</string>')
</dict>
</plist>
PLIST

echo "── signing (Developer ID + hardened runtime) ──"
echo "   identity: $DEV_ID"
codesign --force --options runtime --timestamp \
         --sign "$DEV_ID" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

if [ "$NOTARIZE" -eq 0 ]; then
  echo "✓ signed (skipped notarization). NOTE: without notarization Gatekeeper still"
  echo "  warns on other Macs — good only for your own machines (right-click → Open)."
  echo "  $APP"
  exit 0
fi

echo "── notarizing (profile: $PROFILE — this uploads to Apple, ~1-5 min) ──"
ZIP="$OUT/$NAME.zip"
/usr/bin/ditto -c -k --keepParent "$APP" "$ZIP"
if ! xcrun notarytool submit "$ZIP" --keychain-profile "$PROFILE" --wait; then
  echo "✗ notarization failed. Is the '$PROFILE' credential stored? Run:" >&2
  echo "    xcrun notarytool store-credentials $PROFILE --apple-id <you> --team-id JH2ZCZH58D --password <app-specific-pw>" >&2
  rm -f "$ZIP"; exit 1
fi
rm -f "$ZIP"

echo "── stapling the ticket (so it verifies offline) ──"
xcrun stapler staple "$APP"
echo "── verifying Gatekeeper acceptance ──"
spctl -a -vvv --type exec "$APP" || true

echo "✓ done: $APP"
echo "  This opens on any Mac with a normal double-click — send it (zip it first to keep the bundle intact)."
