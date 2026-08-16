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

# Substitute into a derived spec (a COPY of project.yml — never project.yml itself).
au_identity_apply() {
  local spec="$1" want
  # CFBundleDisplayName is emitted QUOTED because an app name may contain a colon, which is a mapping
  # separator to a bare YAML scalar. `|` is the sed delimiter since a name contains `/` far more often
  # than a pipe, and both `|` and `&` are rejected by the loader anyway.
  sed -e "s|CFBundleDisplayName: Tinyjam Demo|CFBundleDisplayName: \"$AU_DISPLAY\"|" \
      -e "s|subtype: tnyj|subtype: $AU_SUBTYPE|" \
      -e "s|manufacturer: Tnyj|manufacturer: $AU_MANUF|" \
      -e "s|name: \"Tinyjam: Demo\"|name: \"$AU_NAME\"|" \
      "$spec" > "$spec.tmp"
  mv "$spec.tmp" "$spec"
  # A silently-missed substitution ships the WRONG IDENTITY, which is the entire bug this replaced, so
  # assert the RESULT rather than trusting four seds to have matched. Asserting the outcome (the values
  # we asked for are present) and not the absence of the dev-loop strings is deliberate: for tinyjam the
  # two are the same text, so an absence check would either fail on a correct build or be skipped for
  # it, and "skipped" is how a guard goes quietly blind.
  for want in "CFBundleDisplayName: \"$AU_DISPLAY\"" "subtype: $AU_SUBTYPE" "manufacturer: $AU_MANUF" "name: \"$AU_NAME\""; do
    grep -qF -- "$want" "$spec" || { echo "✗ AU identity did not substitute: expected '$want'"; \
      echo "  project.yml's AU block moved — fix the sed targets in ios/au-identity.sh."; return 1; }
  done
}
