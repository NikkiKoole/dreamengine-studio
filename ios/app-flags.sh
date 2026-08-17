#!/usr/bin/env bash
# app-flags.sh — turn a staged app's gen/app.dims into build flags. SOURCED, not run.
#
#   . ./app-flags.sh
#   app_flags            # reads DE_* from the environment (source gen/app.dims first)
#                        # → appends to DEFS, fills the ORIENT_SETTINGS array
#
# WHY THIS IS SHARED. build.sh honoured DE_RESIZABLE and DE_ORIENT; device.sh honoured only a
# RESIZABLE env var; testflight.sh — the STORE path — honoured NEITHER. So a manifest's
# "orientation": "landscape" reached the simulator and nothing else, and the build that went to
# review advertised all four orientations. Three copies of one rule, and the copy that mattered
# least was the only correct one.
#
# ⚠ THE TWO FLAGS ARE ONE DECISION. DO NOT PASS DE_RESIZABLE WITHOUT DE_ORIENT.
# GameHost.swift does this, once de_init() has run:
#
#     if de_is_resizable(engine) != 0 { AppDelegate.orientationLock = .all }
#     else { lock to the cart's own aspect — 160x100 is w>h, so .landscape }
#
# So a RESIZABLE cart asks UIKit for FREE ROTATION, deliberately: it can reflow to any viewport.
# An app manifest saying "landscape" is overriding that at the product level, and it does it through
# the PLIST, because UIKit intersects the plist's UISupportedInterfaceOrientations with the
# delegate's mask. Pass the resizable define on its own and the lock silently becomes .all — the app
# starts rotating into portrait, which is exactly what the manifest was trying to forbid. That is
# not hypothetical: it is why the shipped build IS landscape-locked today (no DE_RESIZABLE → the
# cart reads as fixed → locked to its aspect), i.e. the right behaviour by way of two bugs.
#
# Design: docs/design/device-adaptive-layout.md.

app_flags() {
  # reflow to fill the device viewport. RESIZABLE=1 forces it on for a single-cart build; for an app
  # build-app.js has already written DE_RESIZABLE=1 into gen/app.dims when every cart opts in.
  if [ -n "${RESIZABLE:-}" ] || [ -n "${DE_RESIZABLE:-}" ]; then
    DEFS="$DEFS DE_RESIZABLE=1"
  fi
  # Orientation lock from the manifest. Overrides project.yml's all-orientations default, and is the
  # half that survives a resizable cart (see the note above).
  ORIENT_SETTINGS=()
  if [ "${DE_ORIENT:-}" = "landscape" ]; then
    local o="UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight"
    ORIENT_SETTINGS+=("INFOPLIST_KEY_UISupportedInterfaceOrientations=$o")
    ORIENT_SETTINGS+=("INFOPLIST_KEY_UISupportedInterfaceOrientations~ipad=$o")
    echo "▸ orientation: landscape-locked"
  elif [ "${DE_ORIENT:-}" = "portrait" ]; then
    local o="UIInterfaceOrientationPortrait"
    ORIENT_SETTINGS+=("INFOPLIST_KEY_UISupportedInterfaceOrientations=$o")
    ORIENT_SETTINGS+=("INFOPLIST_KEY_UISupportedInterfaceOrientations~ipad=$o")
    echo "▸ orientation: portrait-locked"
  fi
}

# ── PREFLIGHT: does the archive match what the manifest asked for? ───────────────────────────────
# Called by testflight.sh with the built .app, BEFORE upload. Everything here is a mistake that has
# actually shipped, or was one flag away from shipping:
#   · ITMS-90473 — the AU carried CFBundleVersion 1 inside an app stamped with the build number,
#     because xcodegen bakes a literal into a hand-written plist and no build setting can override
#     it. Apple accepted the delivery and emailed the warning afterwards, so nothing local noticed.
#   · the orientation lock reaching only the simulator (see above).
# The point is to assert the ARTIFACT, not the intent: the spec can say anything, this reads what
# actually got built.
app_preflight() {
  local app="$1" bad=0 v sv
  v="$(plutil -extract CFBundleVersion raw -o - "$app/Info.plist" 2>/dev/null)"
  sv="$(plutil -extract CFBundleShortVersionString raw -o - "$app/Info.plist" 2>/dev/null)"
  echo "▸ preflight: app $sv ($v)"
  for ext in "$app"/PlugIns/*.appex; do
    [ -d "$ext" ] || continue
    local ev esv n; n="$(basename "$ext")"
    ev="$(plutil -extract CFBundleVersion raw -o - "$ext/Info.plist" 2>/dev/null)"
    esv="$(plutil -extract CFBundleShortVersionString raw -o - "$ext/Info.plist" 2>/dev/null)"
    if [ "$ev" != "$v" ]; then
      echo "  ✗ $n CFBundleVersion '$ev' != app '$v'  (ITMS-90473 — Apple will warn on delivery)"; bad=1
    elif [ "$esv" != "$sv" ]; then
      echo "  ✗ $n CFBundleShortVersionString '$esv' != app '$sv'"; bad=1
    else
      echo "  ✓ $n $esv ($ev)"
    fi
  done
  if [ -n "${DE_ORIENT:-}" ]; then
    local got; got="$(plutil -extract UISupportedInterfaceOrientations json -o - "$app/Info.plist" 2>/dev/null)"
    case "${DE_ORIENT}:$got" in
      landscape:*Portrait*) echo "  ✗ manifest says landscape but the plist allows portrait: $got"; bad=1 ;;
      portrait:*Landscape*) echo "  ✗ manifest says portrait but the plist allows landscape: $got"; bad=1 ;;
      *) echo "  ✓ orientation ${DE_ORIENT} honoured" ;;
    esac
  fi
  [ "$bad" = 0 ] || { echo "✗ preflight failed — fix before uploading (nothing was sent)"; return 1; }
}
