#!/usr/bin/env bash
# au-identity.sh — the AU's identity, derived from an app manifest. SOURCED, not run.
#
#   . ./au-identity.sh
#   au_identity_load  <app>            # reads apps/<app>/app.json → AU_NAME/AU_SUBTYPE/AU_MANUF/AU_DISPLAY
#   au_identity_apply <spec.yml>       # substitutes them into a derived xcodegen spec + asserts it took
#
# WHY THIS IS A SHARED FILE and not two copies: `project.yml` is the dev-loop spec and it is SHARED by
# every app, so its hand-written AU identity used to ship on whatever app was being archived. With two
# apps setting auCart that is a COLLISION, not just an ugly name: a host addresses a plug-in by the
# triple (type, subtype, manufacturer), so two apps claiming `aumu tnyj Tnyj` means it loads whichever
# it registered first. Fixing that by giving testflight.sh and device.sh each their own copy of the
# substitution would recreate the same class of bug one level up, which is the whole reason the AU
# identity is derived rather than hand-written in the first place.
#
# ⚠ THE CODES ARE FOREVER, in the same sense a host parameter's address is. A DAW stores the triple in
# the saved project to re-instantiate the plug-in, so changing a SHIPPED subtype or manufacturer makes
# every project that used it come back with a missing plug-in and no way to reconnect it. Pick them
# once, per app, and never edit them again. The NAME strings are display only and are safe to improve
# whenever. (This is why tinyjam's manifest repeats its existing tnyj/Tnyj verbatim rather than being
# given nicer codes: it is already out there.)
#
# Design + what is still open: docs/design/auv3-plugin-types.md §6.

# Read + validate. Call with the app name; run from ios/.
au_identity_load() {
  local app="$1" m="../apps/$1/app.json" pair
  AU_NAME="$(node -p "require('$m').auName || ''")"
  AU_SUBTYPE="$(node -p "require('$m').auSubtype || ''")"
  AU_MANUF="$(node -p "require('$m').auManufacturer || ''")"
  AU_DISPLAY="$(node -p "require('$m').auDisplayName || require('$m').name")"
  for pair in "auName:$AU_NAME" "auSubtype:$AU_SUBTYPE" "auManufacturer:$AU_MANUF"; do
    [ -n "${pair#*:}" ] || { echo "✗ manifest sets auCart but no ${pair%%:*} — an AU needs its own identity."; \
      echo "  Add to apps/$app/app.json (the codes are FOREVER, see ios/au-identity.sh):"; \
      echo '    "auName": "Studio: My App", "auSubtype": "abcd", "auManufacturer": "Abcd"'; return 1; }
  done
  # Exactly 4 alphanumerics, or the component is silently unaddressable.
  # ⚠ POSIX CLASSES, NOT RANGES. `[A-Z]` in a shell bracket expression is a COLLATION range, and under
  # a UTF-8 locale that range contains the lowercase letters too — so the all-lowercase manufacturer
  # check below passed a plain `mpla` when it was first written as `*[A-Z]*`, i.e. the guard was
  # present, green, and blind. Caught by mutation-testing the validators, not by reading them.
  for pair in "auSubtype:$AU_SUBTYPE" "auManufacturer:$AU_MANUF"; do
    case "${pair#*:}" in
      [[:alnum:]][[:alnum:]][[:alnum:]][[:alnum:]]) ;;
      *) echo "✗ ${pair%%:*} must be exactly 4 alphanumeric chars, got '${pair#*:}'"; return 1 ;;
    esac
  done
  case "$AU_MANUF" in *[[:upper:]]*) ;; *) echo "✗ auManufacturer '$AU_MANUF' is all lowercase — Apple reserves those. Capitalise one letter."; return 1 ;; esac
  # these land inside a sed replacement and a YAML scalar; refuse what would corrupt either
  case "$AU_NAME$AU_DISPLAY" in *[\\\&\|\"]*) echo "✗ auName/auDisplayName cannot contain \\ & | or \" (they break the spec derivation)"; return 1 ;; esac
  echo "▸ AU identity: \"$AU_NAME\"  ·  aumu $AU_SUBTYPE $AU_MANUF  ·  shown as \"$AU_DISPLAY\""
}

# ── THE CARRIER (macOS only) ──────────────────────────────────────────────────────────────────────
# On macOS an AUv3 is registered by LAUNCHING the app bundle that contains it, so the dev loop needs a
# throwaway "carrier" app around the extension. project-mac.yml hardcoded ONE — `com.tinyjam.mac`,
# product name `TinyjamMac` — for every app, and mac.sh installed it to the single path
# ~/Applications/TinyjamMac.app with an `rm -rf` in front. So building app B silently DEREGISTERED
# app A's plug-in: it vanished from GarageBand with nothing said, and the fix looked like rebuilding
# A, which then evicted B. Renaming the .app alone would NOT have fixed it — LaunchServices keys apps
# by BUNDLE ID, so two copies of one id at two paths still means one wins, unpredictably. The id has
# to differ, which is why this derives both.
#
# Only the CARRIER's identity is derived here, not the AU target's PRODUCT_NAME: that would move
# PRODUCT_MODULE_NAME with it (it defaults from PRODUCT_NAME), and the AU's plist names its principal
# class as $(PRODUCT_MODULE_NAME).TinyjamAUViewController. Two appexes sharing a FILENAME inside two
# differently-identified carriers is fine; a renamed Swift module is a needless way to break the view.
#
#   au_carrier_load <app>   # → CARRIER_SLUG / CARRIER_APP_ID / CARRIER_APPEX_ID / CARRIER_APP_PATH
au_carrier_load() {
  local app="$1" m="../apps/$1/app.json" base
  base="$(node -p "require('$m').bundleId")"
  [ -n "$base" ] && [ "$base" != "undefined" ] || { echo "✗ apps/$app/app.json has no bundleId"; return 1; }
  # A Swift-identifier-safe CamelCase slug from the app's display name: it becomes the .app filename
  # and (via PRODUCT_NAME) the app target's module name, so anything but alphanumerics has to go.
  CARRIER_SLUG="$(node -p "(require('$m').name||'$app').replace(/[^A-Za-z0-9]/g,'')")Mac"
  case "$CARRIER_SLUG" in [0-9]*|Mac) echo "✗ app name '$app' yields an unusable carrier name '$CARRIER_SLUG'"; return 1 ;; esac
  CARRIER_APP_ID="$base.mac"
  CARRIER_APPEX_ID="$base.mac.AU"
  CARRIER_APP_PATH="$HOME/Applications/$CARRIER_SLUG.app"
  echo "▸ carrier: $CARRIER_SLUG.app  ·  $CARRIER_APP_ID  (extension $CARRIER_APPEX_ID)"
}

# Substitute the carrier identity into a derived spec. Separate from au_identity_apply because the
# PLUG-IN's identity is shared by the iOS and Mac specs while the carrier exists only on the Mac.
au_carrier_apply() {
  local spec="$1" want
  sed -E -e "s|^( +)PRODUCT_BUNDLE_IDENTIFIER: com\.tinyjam\.mac$|\\1PRODUCT_BUNDLE_IDENTIFIER: $CARRIER_APP_ID|" \
         -e "s|^( +)PRODUCT_BUNDLE_IDENTIFIER: com\.tinyjam\.mac\.AU$|\\1PRODUCT_BUNDLE_IDENTIFIER: $CARRIER_APPEX_ID|" \
      "$spec" > "$spec.tmp"
  mv "$spec.tmp" "$spec"
  # PRODUCT_NAME is INSERTED rather than substituted — project-mac.yml does not set it (the product
  # takes the target's name by default), so there is no line to rewrite. Anchored to the app target's
  # own bundle-id line, which au_carrier_apply has just made unique.
  awk -v id="$CARRIER_APP_ID" -v pn="$CARRIER_SLUG" '
    { print }
    $0 ~ ("PRODUCT_BUNDLE_IDENTIFIER: " id "$") {
      match($0, /^ +/); printf "%sPRODUCT_NAME: %s\n", substr($0, 1, RLENGTH), pn }
  ' "$spec" > "$spec.tmp"
  mv "$spec.tmp" "$spec"
  for want in "PRODUCT_BUNDLE_IDENTIFIER: $CARRIER_APP_ID" "PRODUCT_BUNDLE_IDENTIFIER: $CARRIER_APPEX_ID" "PRODUCT_NAME: $CARRIER_SLUG"; do
    grep -qF -- "$want" "$spec" || { echo "✗ carrier identity did not substitute: expected '$want'"; \
      echo "  $spec's carrier block moved — fix the sed/awk targets in ios/au-identity.sh."; return 1; }
  done
}

# Substitute into a derived spec (a COPY of project.yml / project-mac.yml — never the spec itself).
#
# ⚠ MATCHED BY KEY, NOT BY CURRENT VALUE. This used to sed the literal dev-loop strings
# (`subtype: tnyj`, `name: "Tinyjam: Demo"`, …), which silently made it project.yml-ONLY: the Mac
# spec carries a different set of literals (`tacj`/`Mpla`/`"Mipolai: Tiny Acid Jam"`), so pointing
# this at project-mac.yml matched nothing and the assert below fired. Keying off the FIELD NAME
# makes one derivation serve both specs, which is the whole reason this file is shared — a
# per-spec copy is the two-sources-of-truth bug the header warns about, one level down.
au_identity_apply() {
  local spec="$1" want
  # ANCHORS, because `name:` is not unique in either spec: the project's own top-level `name:` sits at
  # column 0 and unquoted, while the AudioComponents entry is deeply indented and quoted. Requiring
  # BOTH leading whitespace and the quotes is what keeps this off `name: TinyjamMac` — renaming the
  # xcodeproj would break every build in a way no assert here could explain.
  # CFBundleDisplayName is emitted QUOTED because an app name may contain a colon, which is a mapping
  # separator to a bare YAML scalar. `|` is the sed delimiter since a name contains `/` far more often
  # than a pipe, and both `|` and `&` are rejected by the loader anyway.
  sed -E -e "s|^( +)CFBundleDisplayName: .*$|\\1CFBundleDisplayName: \"$AU_DISPLAY\"|" \
         -e "s|^( +)subtype: .*$|\\1subtype: $AU_SUBTYPE|" \
         -e "s|^( +)manufacturer: .*$|\\1manufacturer: $AU_MANUF|" \
         -e "s|^( +)name: \".*\"$|\\1name: \"$AU_NAME\"|" \
      "$spec" > "$spec.tmp"
  mv "$spec.tmp" "$spec"
  # A silently-missed substitution ships the WRONG IDENTITY, which is the entire bug this replaced, so
  # assert the RESULT rather than trusting four seds to have matched. Asserting the outcome (the values
  # we asked for are present) and not the absence of the dev-loop strings is deliberate: for tinyjam the
  # two are the same text, so an absence check would either fail on a correct build or be skipped for
  # it, and "skipped" is how a guard goes quietly blind.
  for want in "CFBundleDisplayName: \"$AU_DISPLAY\"" "subtype: $AU_SUBTYPE" "manufacturer: $AU_MANUF" "name: \"$AU_NAME\""; do
    grep -qF -- "$want" "$spec" || { echo "✗ AU identity did not substitute: expected '$want'"; \
      echo "  $spec's AU block moved — fix the sed targets in ios/au-identity.sh."; return 1; }
  done
}
