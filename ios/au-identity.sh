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
  # THE COMPONENT TYPE, and it is part of the FOREVER triple — a host addresses a plug-in by
  # (type, subtype, manufacturer), so changing a SHIPPED type orphans every project that used it just
  # as surely as changing the subtype would. Defaults to `aumu` because that is what every app built
  # before this key existed shipped as, and this file must not silently re-type them.
  # `aumf` (MUSIC EFFECT — the `m` is music, the `f` is effect) is the one to pick for an effect that
  # also wants notes; plain `aufx` has no reliable MIDI in. docs/design/auv3-plugin-types.md §1.
  AU_TYPE="$(node -p "require('$m').auType || 'aumu'")"
  # ── THE COMPANION COMPONENT (optional) ────────────────────────────────────────────────────────
  # A host lists a plug-in in the slot its TYPE names, and nowhere else: GarageBand's instrument list
  # is `aumu` only, so an `aumf` cart can process someone else's track and can never be PLAYED there,
  # however well it reads host notes (measured 2026-08-18 — GarageBand does not deliver MIDI to an
  # effect slot at all). The fix is not to re-type the cart, which would lose the insert: it is to
  # declare BOTH components in the one appex. `AudioComponents` is an ARRAY, and TinyjamAU derives
  # `isEffect` PER INSTANCE from componentDescription.componentType, so each instantiation configures
  # its own buses with no code change. auv3-plugin-types.md §4.1e.
  # ⚠ The companion's subtype is FOREVER too, exactly like the primary's — it is a second identity a
  # DAW will store in saved projects, not an alias of the first.
  AU_SUBTYPE_INST="$(node -p "require('$m').auSubtypeInstrument || ''")"
  if [ -n "$AU_SUBTYPE_INST" ]; then
    case "$AU_TYPE" in
      aumf|aufx) ;;
      *) echo "✗ auSubtypeInstrument is for an EFFECT app: it adds an aumu companion so the rack is";          echo "  playable in a host's instrument slot. auType is '$AU_TYPE', which already IS an";          echo "  instrument, so the companion would collide with it."; return 1 ;;
    esac
    case "$AU_SUBTYPE_INST" in
      [[:alnum:]][[:alnum:]][[:alnum:]][[:alnum:]]) ;;
      *) echo "✗ auSubtypeInstrument must be exactly 4 alphanumeric chars, got '$AU_SUBTYPE_INST'"; return 1 ;;
    esac
    # The two components differ ONLY by (type, subtype), so an equal subtype makes them the same
    # component declared twice — which is not an error a host reports, it just registers one.
    [ "$AU_SUBTYPE_INST" != "$AU_SUBTYPE" ] || {       echo "✗ auSubtypeInstrument '$AU_SUBTYPE_INST' equals auSubtype — the companion needs its OWN code."; return 1; }
  fi
  case "$AU_TYPE" in
    aumu|augn|aufx|aumf|aumi) ;;
    *) echo "✗ auType '$AU_TYPE' is not one of aumu/augn/aufx/aumf/aumi (docs/design/auv3-plugin-types.md §1)"; return 1 ;;
  esac
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
  echo "▸ AU identity: \"$AU_NAME\"  ·  $AU_TYPE $AU_SUBTYPE $AU_MANUF  ·  shown as \"$AU_DISPLAY\""
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
  # ⚠ TWO IDENTITIES, and picking the wrong one is permanent.
  #
  #   CARRIER_APP_ID (".mac")   = the LOCAL DEV CARRIER. On macOS the system only learns about an
  #                               AUv3 when the app containing it is launched once, so mac.sh builds
  #                               a throwaway app into ~/Applications purely to register the
  #                               extension. It must NOT collide with the shipping app, hence .mac.
  #
  #   CARRIER_STORE_APP_ID      = the SHIPPING Mac app, and it is the iOS bundle id UNCHANGED.
  #                               Apple's Universal Purchase is what makes one non-consumable cover
  #                               iPhone, iPad and Mac, and it keys on the bundle id being THE SAME
  #                               across platforms. Ship the Mac app as "$base.mac" and it is a
  #                               separate App Store record needing a SECOND purchase — forever,
  #                               because a shipped bundle id cannot be changed. That is the whole
  #                               "buy Pro on my phone, use the plug-in in Live" story, and it turns
  #                               on this one string. See docs/design/pro-unlock.md section 8.
  #
  # Nothing uses CARRIER_STORE_APP_ID yet: there is no Mac STORE pipeline (mac.sh is the dev loop,
  # testflight.sh is iOS-only, and tools/mac-app.sh is the Developer ID route, which cannot do
  # Universal Purchase or App Store IAP at all). It is defined here so the Mac store build is
  # written against it instead of reaching for the dev carrier that happens to be sitting there.
  CARRIER_APP_ID="$base.mac"
  CARRIER_APPEX_ID="$base.mac.AU"
  CARRIER_STORE_APP_ID="$base"
  CARRIER_STORE_APPEX_ID="$base.AU"
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
         -e "s|^( +)- type: [a-z]{4}$|\\1- type: $AU_TYPE|" \
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
  for want in "CFBundleDisplayName: \"$AU_DISPLAY\"" "- type: $AU_TYPE" "subtype: $AU_SUBTYPE" "manufacturer: $AU_MANUF" "name: \"$AU_NAME\""; do
    grep -qF -- "$want" "$spec" || { echo "✗ AU identity did not substitute: expected '$want'"; \
      echo "  $spec's AU block moved — fix the sed targets in ios/au-identity.sh."; return 1; }
  done
  # ── THE COMPANION aumu, APPENDED AS A SECOND ARRAY ITEM ────────────────────────────────────────
  # ⚠ THIS MUST RUN AFTER THE SEDS ABOVE, AND THAT ORDER IS THE WHOLE TRICK. Those seds are
  # line-oriented and unanchored to any one item, so with two component blocks present they would
  # rewrite BOTH `subtype:` lines to the primary's code — leaving two entries that differ only by
  # `type:` and silently registering one plug-in twice under one identity. Substituting first and
  # appending literal final values second means nothing can rewrite the companion.
  if [ -n "${AU_SUBTYPE_INST:-}" ]; then
    node -e '
      const fs = require("fs"), [spec, sub, manu, name] = process.argv.slice(1)
      const L = fs.readFileSync(spec, "utf8").split("\n")
      const key = L.findIndex(l => /^\s*AudioComponents:\s*$/.test(l))
      if (key < 0) { console.error("no AudioComponents: key in " + spec); process.exit(1) }
      const first = L.findIndex((l, i) => i > key && /^\s*- type:/.test(l))
      if (first < 0) { console.error("AudioComponents: has no `- type:` item in " + spec); process.exit(1) }
      const ind = L[first].match(/^(\s*)-/)[1]          // the SEQUENCE ITEM indent, read from the file
      // End of the first item = the next non-blank line indented no deeper than the item dash. Read
      // from the spec rather than assumed, so re-ordering or adding keys inside the block cannot
      // silently put the companion in the wrong place.
      let end = L.length
      for (let i = first + 1; i < L.length; i++) {
        if (!L[i].trim()) continue
        if (L[i].match(/^(\s*)/)[1].length <= ind.length) { end = i; break }
      }
      L.splice(end, 0, ...[
        ind + "- type: aumu",
        ind + "  subtype: " + sub,
        ind + "  manufacturer: " + manu,
        ind + "  name: \"" + name + "\"",
        ind + "  version: 1",
        ind + "  sandboxSafe: true",
        ind + "  tags:",
        ind + "    - Synthesizer",
      ])
      fs.writeFileSync(spec, L.join("\n"))
    ' "$spec" "$AU_SUBTYPE_INST" "$AU_MANUF" "$AU_NAME" || {
      echo "✗ could not append the aumu companion to $spec"; return 1; }
    # Assert the RESULT, same reason as the loop above: two components must be present, and the
    # companion's own subtype must have survived. A grep for `aumu` alone would pass on the primary.
    [ "$(grep -cE '^ +- type: (aumu|aumf|aufx)$' "$spec")" = "2" ] || { \
      echo "✗ expected exactly 2 AudioComponents entries in $spec after appending the companion"; return 1; }
    grep -qF -- "subtype: $AU_SUBTYPE_INST" "$spec" || { \
      echo "✗ the companion's subtype '$AU_SUBTYPE_INST' is not in $spec"; return 1; }
  fi
}
