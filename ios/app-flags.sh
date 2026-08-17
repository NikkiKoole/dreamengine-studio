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
# ⚠ ORIENTATION AND REFLOW ARE ONE DECISION, and today you can only have one of them.
# GameHost.swift, once de_init() has run:
#
#     if de_is_resizable(engine) != 0 { AppDelegate.orientationLock = .all }
#     else { lock to the cart's own aspect — 160x100 is w>h, so .landscape }
#
# A RESIZABLE cart asks UIKit for FREE ROTATION, deliberately: it can reflow to any viewport. So an
# app whose manifest says "landscape" must NOT be built resizable, or the lock silently becomes .all
# and it rotates into portrait — the opposite of what the manifest asked. The plist is not a way out:
# see the note in app_flags. This is the behaviour that already shipped and was confirmed on device;
# the code below makes it deliberate rather than the accident of two bugs cancelling out.
#
# Design: docs/design/device-adaptive-layout.md.

app_flags() {
  # reflow to fill the device viewport. RESIZABLE=1 forces it on for a single-cart build; for an app
  # build-app.js has already written DE_RESIZABLE=1 into gen/app.dims when every cart opts in.
  #
  # ⚠ A DECLARED ORIENTATION WINS OVER REFLOW, and the two cannot both be had today. GameHost.swift:
  #     if de_is_resizable(engine) != 0 { AppDelegate.orientationLock = .all }   // free rotation
  #     else { lock to the cart's own aspect }                                   // 160x100 → landscape
  # so passing the resizable define to an app whose manifest says "landscape" would UNLOCK it and let
  # it rotate into portrait — the opposite of what the manifest asked for. The plist is not an escape
  # hatch either: restricting UISupportedInterfaceOrientations restricts iPad too (the ~ipad variant
  # is silently ignored by Xcode's INFOPLIST_KEY_ mechanism — it never reaches the built plist), and a
  # restricted iPad set without UIRequiresFullScreen is the upload rejection project.yml records
  # against 2026-07-06. UIRequiresFullScreen itself is being phased out on modern iPadOS, so buying
  # the lock with it trades a known rejection for an unknown one.
  # Therefore: declared orientation → no reflow, and the RUNTIME lock does the work. That is exactly
  # what shipped and what the maker confirmed on device; this makes it deliberate instead of the
  # accident of two bugs cancelling. Lifting the restriction needs a runtime orientation signal
  # carried from the manifest, so a resizable cart can be locked too — not wired yet.
  if [ -n "${DE_ORIENT:-}" ] && [ -n "${DE_RESIZABLE:-}" ]; then
    echo "▸ orientation: ${DE_ORIENT} (runtime lock) — reflow withheld, see ios/app-flags.sh"
  elif [ -n "${RESIZABLE:-}" ] || [ -n "${DE_RESIZABLE:-}" ]; then
    DEFS="$DEFS DE_RESIZABLE=1"
  fi
  # NO plist orientation keys. See the long note above: restricting them restricts iPad, which is a
  # documented upload rejection, and the runtime lock already covers every device.
  ORIENT_SETTINGS=()
  # The microphone purpose string, present ONLY when a cart in this app actually opens the mic
  # (build-app.js derives DE_MIC_USAGE from mic_start() calls and refuses to build a mic app without
  # wording). A single-cart dev build has no manifest, so it keeps a generic string: the dev loop
  # must work for any cart, including the mic ones, and its identity is throwaway anyway.
  if [ -n "${DE_MIC_USAGE:-}" ]; then
    ORIENT_SETTINGS+=("INFOPLIST_KEY_NSMicrophoneUsageDescription=$DE_MIC_USAGE")
    echo "▸ microphone: \"$DE_MIC_USAGE\""
  elif [ -z "${APP:-}" ]; then
    ORIENT_SETTINGS+=("INFOPLIST_KEY_NSMicrophoneUsageDescription=Lets a cart hear you — live audio through the effects, or an instrument you play by humming or clapping.")
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
  # A declared orientation is enforced at RUNTIME (AppDelegate.orientationLock), and that only holds
  # while the cart reads as NON-resizable — so assert the precondition, not the plist. If the binary
  # ever ships with both a declared orientation and the resizable define, the lock silently becomes
  # .all and the app rotates freely. Nothing else in the build would say so.
  if [ -n "${DE_ORIENT:-}" ] && printf '%s' "${DEFS:-}" | grep -q "DE_RESIZABLE"; then
    echo "  ✗ manifest declares orientation ${DE_ORIENT} but the build passes DE_RESIZABLE —"
    echo "    GameHost.swift sets orientationLock = .all for a resizable cart, so the lock is GONE"; bad=1
  elif [ -n "${DE_ORIENT:-}" ]; then
    echo "  ✓ orientation ${DE_ORIENT} — runtime lock intact (no DE_RESIZABLE)"
  fi
  # The mic key must be present EXACTLY when a cart opens the mic: absent on a mic app is a crash at
  # the permission prompt, present on a mic-free app is a false capability claim on the store page.
  local mic; mic="$(plutil -extract NSMicrophoneUsageDescription raw -o - "$app/Info.plist" 2>/dev/null || true)"
  if [ -n "${DE_MIC_USAGE:-}" ] && [ -z "$mic" ]; then
    echo "  ✗ a cart opens the mic but the bundle has no NSMicrophoneUsageDescription (iOS would kill it)"; bad=1
  elif [ -z "${DE_MIC_USAGE:-}" ] && [ -n "$mic" ]; then
    echo "  ✗ no cart opens the mic, yet the bundle declares NSMicrophoneUsageDescription: \"$mic\""; bad=1
  else
    echo "  ✓ microphone: ${DE_MIC_USAGE:+declared}${DE_MIC_USAGE:-not declared, and not used}"
  fi
  [ "$bad" = 0 ] || { echo "✗ preflight failed — fix before uploading (nothing was sent)"; return 1; }
}
