#!/usr/bin/env bash
# Archive + upload a TESTFLIGHT build to App Store Connect — no Xcode GUI. The STORE rung of
# the iOS ladder: build.sh (simulator) → device.sh (cable) → testflight.sh (over-the-air).
#
#   APP=tinyjam ./testflight.sh                 # stage → archive (Release, store identity) → upload
#   APP=tinyjam SKIP_UPLOAD=1 ./testflight.sh   # archive only (inspect build/<app>.xcarchive)
#   APP=tinyjam AUTH_KEY=ABC123 AUTH_ISSUER=uuid ./testflight.sh   # upload with an ASC API key
#
# What it does differently from device.sh:
#   - STORE IDENTITY: the xcodeproj is generated from a sed-derived spec (project-store.yml,
#     gitignored) that swaps the dev-loop bundle id (com.tinyjam.hello) for the MANIFEST's
#     bundleId (apps/<name>/app.json — for tinyjam: com.mipolai.tinyjam, registered + reserved
#     on ASC 2026-07-06) and the manifest's version for MARKETING_VERSION. The AU extension
#     becomes <bundleId>.TinyjamAU automatically (same prefix swap).
#   - DISPLAY NAME: the manifest's "name" → INFOPLIST_KEY_CFBundleDisplayName (home-screen
#     label; the AU keeps its own hand-authored Info.plist).
#   - BUILD NUMBER: CURRENT_PROJECT_VERSION = `date +%Y%m%d%H%M` — unique + monotonic per
#     upload (ASC rejects a reused build number for the same version).
#   - SIGNING: Apple Distribution via Xcode CLOUD signing — -allowProvisioningUpdates with the
#     signed-in Xcode account mints the managed distribution cert + App Store profiles (incl.
#     the .TinyjamAU child App ID) on first run. Same account session device.sh uses.
#   - UPLOAD: xcodebuild -exportArchive, method app-store-connect, destination upload. Auth
#     rides the Xcode account; if Apple demands key auth, mint an ASC API key (ASC → Users and
#     Access → Integrations → App Store Connect API), drop the .p8 at
#     ~/.appstoreconnect/private_keys/AuthKey_<KEYID>.p8, and pass AUTH_KEY + AUTH_ISSUER.
#
# One-time prerequisites: Xcode signed into the Apple ID (Settings → Accounts — device.sh's
# prerequisite too) + the app record existing on ASC with the manifest's bundleId.
# After upload: ASC → TestFlight shows the build "Processing" for ~5–30 min, then it's
# installable via the TestFlight app (internal testers immediately, no review).
set -euo pipefail
cd "$(dirname "$0")"

APP="${APP:?usage: APP=<name> ./testflight.sh   (needs apps/<name>/app.json)}"
TEAM="${TEAM:-JH2ZCZH58D}"
SCHEME="TinyjamHello"
AU_CART="${AU_CART:-epiano}"

BUNDLE_ID="$(node -p "require('../apps/$APP/app.json').bundleId")"
APP_NAME="$(node -p "require('../apps/$APP/app.json').name")"
VERSION="$(node -p "require('../apps/$APP/app.json').version || '0.1'")"
BUILD_NUM="$(date +%Y%m%d%H%M)"
[ "$BUNDLE_ID" = "undefined" ] && { echo "✗ manifest has no bundleId"; exit 1; }
echo "▸ $APP → $BUNDLE_ID  ·  \"$APP_NAME\" v$VERSION ($BUILD_NUM)  ·  team $TEAM"

echo "▸ staging MULTI-CART app '$APP' → gen/app…"
( cd .. && node tools/build-app.js "$APP" --ios ) || { echo "✗ build-app.js --ios failed"; exit 1; }
[ -f gen/app.dims ] && { set -a; . ./gen/app.dims; set +a; }

echo "▸ staging AU cart '$AU_CART' → gen/au…"
( cd .. && node tools/play.js "$AU_CART" run --headless --frames 1 >/dev/null 2>&1 ) \
  || { echo "✗ AU cart '$AU_CART' generation failed"; exit 1; }
mkdir -p gen/au; rm -f gen/au/*.c
cp ../build/cart.c ../build/sprites_data.h ../build/map_data.h gen/au/

# cart dims → preprocessor override (same as device.sh)
SW="${DE_SCREEN_W:-320}"; SH="${DE_SCREEN_H:-200}"
MW="${DE_MAP_W:-128}"; MH="${DE_MAP_H:-64}"; CWv="${DE_CELL_W:-16}"; CHv="${DE_CELL_H:-16}"
DEFS="\$(inherited) DE_NO_RAYLIB=1 SCREEN_W=$SW SCREEN_H=$SH SCALE=1 MAP_W=$MW MAP_H=$MH CELL_W=$CWv CELL_H=$CHv"

echo "▸ deriving store spec (project-store.yml: $BUNDLE_ID, v$VERSION)…"
sed -e "s/com\.tinyjam\.hello/$BUNDLE_ID/g" \
    -e "s/MARKETING_VERSION: \"[^\"]*\"/MARKETING_VERSION: \"$VERSION\"/g" \
    project.yml > project-store.yml
xcodegen generate --spec project-store.yml >/dev/null

ARCHIVE="build/$APP.xcarchive"
echo "▸ archiving (Release, ${SW}x${SH}, cloud signing — first run mints the distribution cert)…"
xcodebuild -project "$SCHEME.xcodeproj" -scheme "$SCHEME" -configuration Release \
  -destination 'generic/platform=iOS' -archivePath "$ARCHIVE" -derivedDataPath build \
  GCC_PREPROCESSOR_DEFINITIONS="$DEFS" \
  CURRENT_PROJECT_VERSION="$BUILD_NUM" \
  INFOPLIST_KEY_CFBundleDisplayName="$APP_NAME" \
  INFOPLIST_KEY_ITSAppUsesNonExemptEncryption=NO \
  -allowProvisioningUpdates DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic \
  archive > build/archive.log 2>&1 \
  || { echo "✗ archive failed — tail of build/archive.log:"; tail -30 build/archive.log; exit 1; }
echo "  ✓ archived → $ARCHIVE"

cat > build/ExportOptions.plist <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>method</key><string>app-store</string> <!-- Xcode ≤15.1 name; 15.3+ renamed it app-store-connect -->

  <key>destination</key><string>upload</string>
  <key>teamID</key><string>$TEAM</string>
  <key>manageAppVersionAndBuildNumber</key><false/>
</dict></plist>
PLIST

[ -n "${SKIP_UPLOAD:-}" ] && { echo "✓ done (SKIP_UPLOAD set — no upload)"; exit 0; }

echo "▸ exporting + uploading to App Store Connect…"
AUTH_ARGS=""
[ -n "${AUTH_KEY:-}" ] && AUTH_ARGS="-authenticationKeyID $AUTH_KEY -authenticationKeyIssuerID ${AUTH_ISSUER:?AUTH_ISSUER required with AUTH_KEY} -authenticationKeyPath $HOME/.appstoreconnect/private_keys/AuthKey_$AUTH_KEY.p8"
# shellcheck disable=SC2086  # AUTH_ARGS is deliberately word-split
xcodebuild -exportArchive -archivePath "$ARCHIVE" \
  -exportOptionsPlist build/ExportOptions.plist -exportPath build/export \
  -allowProvisioningUpdates $AUTH_ARGS > build/upload.log 2>&1 \
  || { echo "✗ upload failed — tail of build/upload.log:"; tail -30 build/upload.log; exit 1; }
echo "✓ uploaded ($BUNDLE_ID v$VERSION build $BUILD_NUM)"
echo "  → App Store Connect → TestFlight: 'Processing' for ~5–30 min, then installable."
