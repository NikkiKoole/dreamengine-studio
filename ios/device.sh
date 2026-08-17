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
AU_CART="${AU_CART:-acidcandy}"  # the AUv3 extension's cart — the same rack the macOS plug-in ships
# ── FINDING THE DEVICE: TWO PROTOCOLS, AND WHICH ONE DEPENDS ON THE DEVICE ──────────────────────
# Modern iOS/iPadOS (17+) devices speak CoreDevice and are driven by `xcrun devicectl`. Older ones
# (the iOS 15 iPhone this script was written for) speak the classic protocol and are driven by
# ios-deploy. NEITHER TOOL SEES BOTH, and the failure is confusing in both directions:
#   · `xctrace list devices` lists a CoreDevice-only iPad under "== Devices Offline ==", so the old
#     discovery below found nothing and exited with "connect + unlock it" — on a device that was
#     connected, unlocked and trusted. (Bit us on an iPadOS 26.5 iPad, 2026-08-14.)
#   · ios-deploy (1.12.2, last release 2022) predates CoreDevice entirely and cannot install to it.
# So: ask devicectl first, fall back to the classic path. DEVICE_ID= still forces either.
# ⚠ PARSE THE JSON, NOT THE TABLE. devicectl's text table has variable-width columns and the MODEL
# field contains spaces and parentheses — "iPad Pro (12.9-inch) (3rd generation) (iPad8,5)" — so an
# awk positional grab returns "(12.9-inch)" as the device id, and the install then fails with
# "The specified device was not found", naming a device nobody asked for. It happened to work while
# the model was the bare "iPad8,5". Same trap as every other regex-over-structured-text in this repo.
DC_ID=""; DC_DEVMODE=""
if [ -z "${DEVICE_ID:-}" ] && xcrun devicectl list devices --json-output /tmp/de-devices.json >/dev/null 2>&1; then
  # ⚠ `.get('name','')`, NOT `['name']`, and it is not defensive habit — it is the third silent-exit
  # trap in this block. A device that is REMEMBERED but not reachable (an old phone, paired once,
  # `tunnelState: unavailable` / `pairingState: unsupported`) has NO `name` KEY AT ALL: its whole
  # deviceProperties is {bootState, ddiServicesAvailable, providerSpecificValues}. So `['name']`
  # raises KeyError, python exits nonzero, the `2>/dev/null` here swallows the traceback, and
  # `set -e` kills device.sh with EXIT 1 AND ZERO OUTPUT — on a machine where `devicectl list
  # devices` prints your iPad, available and paired, one line below the nameless one. Indistinguishable
  # from "no device found" and it does not name the device that broke it. (Cost a run 2026-08-15.)
  # Prefer a PAIRED device too, so a live iPad wins over anything half-remembered next to it.
  # ⚠ AND IT MUST BE REACHABLE, not merely remembered — the fourth trap in this block, and the one
  # that survived the other three. `devicectl list devices` reports every device it has EVER paired
  # with, each carrying `tunnelState: unavailable` when it is not actually there. Filtering on the
  # NAME (above) is not enough: a remembered iPad HAS a name, so it wins the sort, and its udid then
  # goes to xcodebuild as the destination — which fails with "Unable to find a destination matching
  # the provided destination specifier", listing only simulators. The connected device meanwhile is
  # sitting right there in `xctrace`, invisible to this branch, because an iOS 15 phone is
  # classic-protocol and CoreDevice does not drive it AT ALL (see the note above). So the script
  # confidently targeted an offline iPad in another room while the iPhone it was asked for was
  # plugged in. Requiring a live tunnel makes DC_ID empty in that case, which is what lets the
  # ios-deploy fallback below do its job. (Bit an iPhone SE deploy, 2026-08-16.)
  DC_ID="$(/usr/bin/python3 -c "import json
d=[x for x in json.load(open('/tmp/de-devices.json'))['result']['devices']
   if ('iPad' in x['deviceProperties'].get('name','') or 'iPhone' in x['deviceProperties'].get('name',''))
   and x.get('connectionProperties',{}).get('tunnelState','') != 'unavailable']
d.sort(key=lambda x: x.get('connectionProperties',{}).get('pairingState','') != 'paired')
print(d[0]['identifier'] if d else '')" 2>/dev/null)"
  # ⚠ ASK THE DEVICE, NOT THE CACHE. `list devices` reports the LAST KNOWN developerModeStatus, and
  # while the tunnel is disconnected that is whatever it was before you changed it — so a device with
  # Developer Mode freshly enabled still lists as "disabled". Gating on that told the maker to go and
  # do a thing they had already done, twice. `device info details` OPENS the tunnel and answers live.
  # ⚠ NO `exit` IN THESE AWKS, and it is not a style preference. `awk '…{print; exit}'` closes the
  # pipe the moment it matches, and the tool upstream is still writing — so it takes SIGPIPE, the
  # pipeline reports 141, and `set -euo pipefail` kills the whole script. It does that AFTER awk has
  # already captured the right answer, and BEFORE the first echo, so the symptom is `device.sh`
  # exiting 141 with ZERO output on a device that is connected, unlocked and in Developer Mode.
  # (Cost a baffling run 2026-08-15. `devicectl device info details` prints pages, so the race is
  # near-certain here; the xctrace one below is the same trap with a smaller writer, hence flakier.)
  # Reading all of stdin and keeping the FIRST match costs nothing and cannot signal.
  [ -n "$DC_ID" ] && DC_DEVMODE="$(xcrun devicectl device info details --device "$DC_ID" 2>/dev/null \
    | awk -F': ' '/developerModeStatus/ && !seen {print $2; seen=1}')"
  # The HARDWARE udid, which is a different identifier from the CoreDevice uuid above. xcodebuild
  # wants this one; devicectl wants the other. Passing the wrong one fails in confusing ways.
  DC_UDID="$(/usr/bin/python3 -c "import json;d=[x for x in json.load(open('/tmp/de-devices.json'))['result']['devices'] if x['identifier']=='$DC_ID'];print(d[0].get('hardwareProperties',{}).get('udid','') if d else '')" 2>/dev/null)"
fi
DEVICE_ID="${DEVICE_ID:-$(xcrun xctrace list devices 2>&1 \
  | sed -n '/== Devices ==/,/== Simulators ==/p' \
  | awk -F'[()]' '/iPhone|iPad/ && !seen {print $4; seen=1}')}"   # no `exit` — see the SIGPIPE note above

if [ -z "$DEVICE_ID" ] && [ -z "$DC_ID" ]; then
  echo "no physical device found — connect + unlock it, and accept 'Trust This Computer'"
  echo "  what each tool sees (they disagree by design — see the note above):"
  xcrun devicectl list devices 2>&1 | sed 's/^/    devicectl: /'
  xcrun xctrace list devices 2>&1 | sed -n '/== Devices/,/== Simulators/p' | sed 's/^/    xctrace:   /'
  exit 1
fi
if [ -n "$DC_ID" ]; then VIA="devicectl"; else VIA="ios-deploy"; fi
echo "▸ device: ${DC_ID:-$DEVICE_ID} (via $VIA)  ·  team: $TEAM  ·  cart: $CART (AU: $AU_CART)"
# printed because the FIRST build against a new device fails with "isn't registered in your developer
# account", and fixing that needs this exact string pasted into developer.apple.com → Devices.
[ -n "${DC_UDID:-}" ] && echo "  udid: $DC_UDID   (register this if the build fails on provisioning)"
# Fail BEFORE the 1-2 minute signed build if the device cannot possibly accept it.
if [ -n "$DC_ID" ] && [ "$DC_DEVMODE" = "disabled" ]; then
  echo "✗ Developer Mode is DISABLED on this device — the install would fail after the build."
  echo "  iPad: Settings → Privacy & Security → Developer Mode → ON → RESTART → confirm 'Turn On'."
  echo "  ⚠ The reboot is required; the toggle alone does not take effect, and the device reports"
  echo "    'disabled' until you confirm on the far side of the restart."
  exit 1
fi

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
# Resizable define + the manifest's orientation lock, via the SHARED helper (app-flags.sh).
# ⚠ This used to read only the RESIZABLE env var and ignore DE_ORIENT entirely, so `APP=<name>`
# deployed a build that did not honour its own manifest — the lock reached the simulator and nothing
# else. e.g. RESIZABLE=1 CART=acidwire ./device.sh still works for a single cart.
. ./app-flags.sh
app_flags

CONFIG="${CONFIG:-Debug}"   # CONFIG=Release for the optimized engine (real perf; no #if DEBUG perf overlay)

# actool needs an AppIcon set (project.yml sets APPICON_NAME=AppIcon, and the optional
# catalog reference is always emitted). APP builds with a manifest "icon" already staged
# gen/Assets.xcassets; otherwise (single cart / editor / icon-less app) stage the repo's
# default placeholder so CompileAssetCatalog succeeds for ANY cart.
# ⚠ THE CONDITION IS `no APP`, NOT `no catalog`. It used to test whether Contents.json existed —
# but gen/Assets.xcassets was COMMITTED (so a manual Xcode build still got an icon), so the test
# was never true and this branch was dead. Nothing ever RESET the icon, so a single-cart or editor
# build silently inherited whatever the last app build had staged: you got the pedalboard icon on
# an acidcandy build, depending only on what you had built last. Reported by the maker as "I see
# the wrong icons"; it is last-writer-wins on a tracked build artifact. The catalog is untracked
# now (ios/.gitignore) AND this resets every non-APP build, because either fix alone leaves the
# repeat case broken: untracking only helps until your first app build recreates the file.
if [ -z "${APP:-}" ]; then
  mkdir -p gen/Assets.xcassets/AppIcon.appiconset
  printf '{ "info": { "author": "xcode", "version": 1 } }\n' > gen/Assets.xcassets/Contents.json
  cp default-icon.png gen/Assets.xcassets/AppIcon.appiconset/icon-1024.png
  printf '{ "images": [{ "filename": "icon-1024.png", "idiom": "universal", "platform": "ios", "size": "1024x1024" }], "info": { "author": "xcode", "version": 1 } }\n' > gen/Assets.xcassets/AppIcon.appiconset/Contents.json
fi

# NAMES, on the DEV LOOP. The bundle id stays the throwaway com.tinyjam.hello (auto-registered, and
# deliberately NOT the shipping id — see project.yml), but a cable install used to be called
# "TinyjamHello" whatever app you staged, and its AU listed itself as "Tinyjam: Demo" in every host.
# With APP=<manifest> we now derive the same names the store build does (ios/au-identity.sh), into a
# COPY of the spec — project.yml itself is never rewritten, so a bare `xcodegen generate` still gives
# the plain dev-loop project. ⚠ The AU subtype/manufacturer follow the manifest here too, which means
# a cable build registers the SAME component triple the store build will: that is on purpose (you are
# testing the real plug-in identity), and it is why a dev install can shadow a store install of the
# same app in a host's plug-in list. Install one or the other, not both.
SPEC=project.yml
if [ -n "${APP:-}" ]; then
  APP_DISPLAY="$(node -p "require('../apps/$APP/app.json').name")"
  SPEC=project-dev.yml
  cp project.yml "$SPEC"
  if [ -n "$(node -p "require('../apps/$APP/app.json').auCart || ''")" ]; then
    . ./au-identity.sh
    au_identity_load "$APP" || exit 1
    au_identity_apply "$SPEC" || exit 1
  fi
fi

echo "▸ generating + building (signed for device, $CONFIG, ${SW}x${SH})…"
xcodegen generate --spec "$SPEC" >/dev/null
# ⚠ BUILD FOR THE ACTUAL DEVICE, NOT `generic/platform=iOS`. With a generic destination Xcode has no
# device to provision FOR, so -allowProvisioningUpdates happily reuses an existing profile and never
# registers this one — and the install then dies at the very end with
#   "Failed to install embedded profile … 0xe8008012 (This provisioning profile cannot be installed
#    on this device.)"
# which names the profile, not the missing registration. Naming the device is what makes automatic
# signing add it to the team and mint a profile that includes it. (Bit us on a new iPad, 2026-08-14.)
DEST="generic/platform=iOS"
[ -n "${DC_UDID:-}" ] && DEST="platform=iOS,id=$DC_UDID"
# On the ios-deploy path there is no DC_UDID, but xctrace already gave us the hardware udid — use it
# for the same reason the note above names the CoreDevice one: a generic destination provisions for
# nobody, so a device that has never been registered fails at install with 0xe8008012 rather than
# being added to the team here.
[ -z "${DC_UDID:-}" ] && [ -n "${DEVICE_ID:-}" ] && DEST="platform=iOS,id=$DEVICE_ID"
xcodebuild -project "$SCHEME.xcodeproj" -scheme "$SCHEME" -configuration "$CONFIG" \
  -destination "$DEST" -derivedDataPath build \
  GCC_PREPROCESSOR_DEFINITIONS="$DEFS" \
  ${APP_DISPLAY:+INFOPLIST_KEY_CFBundleDisplayName="$APP_DISPLAY"} \
  "${ORIENT_SETTINGS[@]}" \
  -allowProvisioningUpdates DEVELOPMENT_TEAM="$TEAM" CODE_SIGN_STYLE=Automatic build >/dev/null

echo "▸ installing + launching on device…"
APP_PATH="build/Build/Products/$CONFIG-iphoneos/$SCHEME.app"
if [ -n "$DC_ID" ]; then
  # CoreDevice path. install + launch are separate verbs here, unlike ios-deploy's --justlaunch.
  BUNDLE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$APP_PATH/Info.plist")"
  if ! xcrun devicectl device install app --device "$DC_ID" "$APP_PATH"; then
    # ⚠ THE ONE-TIME DEVICE SETUP, and the error names the setting but not where it lives:
    # "The operation failed because Developer Mode is disabled." Developer Mode only APPEARS in
    # Settings after a Mac has attempted an install (which the line above just did), so on a fresh
    # device the toggle does not exist until this fails once. It needs a REBOOT to take effect.
    echo ""
    echo "  ▸ On the iPad: Settings → Privacy & Security → Developer Mode → ON → restart →"
    echo "    confirm 'Turn On' after it reboots. Then re-run this script."
    exit 1
  fi
  xcrun devicectl device process launch --device "$DC_ID" "$BUNDLE_ID"
else
  ios-deploy --id "$DEVICE_ID" --bundle "$APP_PATH" --justlaunch
fi
echo "✓ running on device ($CONFIG)"
