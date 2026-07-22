#!/usr/bin/env bash
# Archive + upload a TESTFLIGHT build to App Store Connect — no Xcode GUI. The STORE rung of
# the iOS ladder: build.sh (simulator) → device.sh (cable) → testflight.sh (over-the-air).
#
#   APP=tinyjam ./testflight.sh                 # stage → archive (Release, store identity) → upload
#   APP=tinyjam SKIP_UPLOAD=1 ./testflight.sh   # archive only (inspect build/<app>.xcarchive)
#   APP=tinyjam USE_AUTH_KEY=1 AUTH_KEY=ABC123 AUTH_ISSUER=uuid ./testflight.sh   # CI: upload via an ASC API key
#
# What it does differently from device.sh:
#   - STORE IDENTITY: the xcodeproj is generated from a sed-derived spec (project-store.yml,
#     gitignored) that swaps the dev-loop bundle id (com.tinyjam.hello) for the MANIFEST's
#     bundleId (apps/<name>/app.json — for tinyjam: com.mipolai.tinyjam, registered + reserved
#     on ASC 2026-07-06) and the manifest's version for MARKETING_VERSION.
#   - AU EXTENSION (opt-in): the manifest's "auCart" names a cart to ship as an AUv3 <bundleId>.TinyjamAU.
#     Absent → the AU target is STRIPPED from the derived spec (single-cart standalones like tinyacidjam
#     ship no AU; then cloud signing only needs the PARENT App ID, no .TinyjamAU child to register).
#   - DISPLAY NAME: the manifest's "name" → INFOPLIST_KEY_CFBundleDisplayName (home-screen
#     label; the AU keeps its own hand-authored Info.plist).
#   - BUILD NUMBER: CURRENT_PROJECT_VERSION = `date +%Y%m%d%H%M` — unique + monotonic per
#     upload (ASC rejects a reused build number for the same version).
#   - SIGNING: Apple Distribution via Xcode CLOUD signing — -allowProvisioningUpdates with the
#     signed-in Xcode account mints the managed distribution cert + App Store profile on first run.
#     Same account session device.sh uses; it CAN create a new app's first profile.
#   - UPLOAD: xcodebuild -exportArchive, method app-store-connect, destination upload. By default auth
#     rides the same Xcode ACCOUNT SESSION (signs + uploads) — do NOT pass an ASC API key for a new
#     app's first upload: a key lacking provisioning-profile management rights fails with "Cloud signing
#     permission error / No profiles found" (the session succeeds where the key can't). For headless/CI
#     with a fully-privileged key: mint one (ASC → Users and Access → Integrations → App Store Connect
#     API), drop the .p8 at ~/.appstoreconnect/private_keys/AuthKey_<KEYID>.p8, and pass USE_AUTH_KEY=1
#     AUTH_KEY=<id> AUTH_ISSUER=<uuid>.
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
# AU is OPT-IN: the manifest's "auCart" names the cart to expose as an AUv3 extension. Absent (single-cart
# standalones like tinyacidjam/pedalboard) → the AU target is stripped from the store spec, so cloud signing
# only mints the PARENT profile (no .TinyjamAU child App ID needed). Env AU_CART overrides. tinyjam keeps its
# AU by setting "auCart":"epiano" in its manifest.
AU_CART="${AU_CART:-$(node -p "require('../apps/$APP/app.json').auCart || ''")}"

BUNDLE_ID="$(node -p "require('../apps/$APP/app.json').bundleId")"
APP_NAME="$(node -p "require('../apps/$APP/app.json').name")"
VERSION="$(node -p "require('../apps/$APP/app.json').version || '0.1'")"
BUILD_NUM="$(date +%Y%m%d%H%M)"
[ "$BUNDLE_ID" = "undefined" ] && { echo "✗ manifest has no bundleId"; exit 1; }
echo "▸ $APP → $BUNDLE_ID  ·  \"$APP_NAME\" v$VERSION ($BUILD_NUM)  ·  team $TEAM"

echo "▸ staging MULTI-CART app '$APP' → gen/app…"
( cd .. && node tools/build-app.js "$APP" --ios ) || { echo "✗ build-app.js --ios failed"; exit 1; }
[ -f gen/app.dims ] && { set -a; . ./gen/app.dims; set +a; }

if [ -n "$AU_CART" ]; then
  echo "▸ staging AU cart '$AU_CART' → gen/au…"
  ( cd .. && node tools/play.js "$AU_CART" run --headless --frames 1 >/dev/null 2>&1 ) \
    || { echo "✗ AU cart '$AU_CART' generation failed"; exit 1; }
  mkdir -p gen/au; rm -f gen/au/*.c
  cp ../build/cart.c ../build/sprites_data.h ../build/map_data.h gen/au/
else
  echo "▸ no AU extension (manifest sets no auCart) — single-cart standalone build"
fi

# cart dims → preprocessor override (same as device.sh)
SW="${DE_SCREEN_W:-320}"; SH="${DE_SCREEN_H:-200}"
MW="${DE_MAP_W:-128}"; MH="${DE_MAP_H:-64}"; CWv="${DE_CELL_W:-16}"; CHv="${DE_CELL_H:-16}"
DEFS="\$(inherited) DE_NO_RAYLIB=1 SCREEN_W=$SW SCREEN_H=$SH SCALE=1 MAP_W=$MW MAP_H=$MH CELL_W=$CWv CELL_H=$CHv"

echo "▸ deriving store spec (project-store.yml: $BUNDLE_ID, v$VERSION${AU_CART:+, AU=$AU_CART})…"
sed -e "s/com\.tinyjam\.hello/$BUNDLE_ID/g" \
    -e "s/MARKETING_VERSION: \"[^\"]*\"/MARKETING_VERSION: \"$VERSION\"/g" \
    project.yml > project-store.yml
if [ -z "$AU_CART" ]; then
  # opt-in AU: strip the AU target + the app's embed dependency (both fenced de:au-begin/end in project.yml)
  awk '/# de:au-begin/{skip=1} !skip{print} /# de:au-end/{skip=0}' project-store.yml > project-store.yml.tmp
  mv project-store.yml.tmp project-store.yml
fi
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
# Signing + upload BOTH ride the signed-in Xcode ACCOUNT SESSION (the same session the archive used):
# it can CREATE the App Store distribution profile on a NEW app's first upload. An ASC API key is NOT
# passed by default — a key without provisioning-profile management rights fails a first upload with
# "Cloud signing permission error / No profiles for '<bundleId>' were found" (hit on tinyacidjam
# 2026-07-22; the archive signs via the session and succeeds, then the key-authed export can't mint the
# profile). For a headless/CI run with a fully-privileged key, set USE_AUTH_KEY=1 to pass it through.
AUTH_ARGS=""
[ -n "${USE_AUTH_KEY:-}" ] && [ -n "${AUTH_KEY:-}" ] && AUTH_ARGS="-authenticationKeyID $AUTH_KEY -authenticationKeyIssuerID ${AUTH_ISSUER:?AUTH_ISSUER required with AUTH_KEY} -authenticationKeyPath $HOME/.appstoreconnect/private_keys/AuthKey_$AUTH_KEY.p8"
# shellcheck disable=SC2086  # AUTH_ARGS is deliberately word-split
xcodebuild -exportArchive -archivePath "$ARCHIVE" \
  -exportOptionsPlist build/ExportOptions.plist -exportPath build/export \
  -allowProvisioningUpdates $AUTH_ARGS > build/upload.log 2>&1 \
  || { echo "✗ upload failed — tail of build/upload.log:"; tail -30 build/upload.log; exit 1; }
echo "✓ uploaded ($BUNDLE_ID v$VERSION build $BUILD_NUM)"
echo "  → App Store Connect → TestFlight: 'Processing' for ~5–30 min, then installable."
