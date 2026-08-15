#!/usr/bin/env bash
# ============================================================================
# midi-note-check — the gate for HOST MIDI NOTES reaching a cart.
#
#   bash tools/midi-note-check/run.sh          # the gate
#   bash tools/midi-note-check/run.sh -v       # keep the renders + print every number
#
# WHAT IT COVERS. `acidcandy` is an AUv3 instrument, so every host draws a keyboard above it, and
# until 2026-08-15 pressing a key did nothing: the notes reached the engine ring and no cart drained
# them. This asserts the mapping that fixed it (docs/design/host-midi-notes.md → the top row):
#
#   on a DRUM face  a note plays the KIT, through the General MIDI map the rack already SENDS on
#   anywhere else   a note TRANSPOSES the two acid lines, overriding the KEY panel while held
#
# WHY IT CAN RUN AT ALL: `--midi-note` (runtime/studio.c → midi_sched_add) pushes into the SAME ring
# an AUv3 host feeds through de_midi_event, so this exercises the shipping path with no DAW, no cable
# and no keyboard. Before that flag existed, nothing could put a note into a headless run and this
# whole feature was gateable only on a device.
#
# ⚠ EVERY RUN CLEARS build/saves/acidcandy FIRST. The rack AUTOSAVES, so run N+1 boots from run N's
# session and two "identical" renders come out different — which is not a flaky gate, it is the gate
# measuring two different racks. It cost two wrong conclusions while this was being written, and it
# will bite anyone who A/Bs this cart by hand.
#
# THREE NEGATIVE CONTROLS, because "the audio differs" is worthless without a floor. Each stops a
# different way of passing for the wrong reason:
#   · two untouched renders must be BYTE-IDENTICAL          (else every difference below is noise)
#   · holding the panel's OWN root must be byte-identical   (the override is exact, not just present)
#   · a note OUTSIDE the kit must change NOTHING            (else the kit test passes on any MIDI
#                                                            traffic at all, rather than on the map)
#
# PROVEN ABLE TO GO RED (2026-08-15): with the cart's `host_notes()` drain removed, 9 of the 16 go
# red — and the 7 that stay green are EXACTLY the controls plus the statements that are true when
# the feature is absent (the panel owns the root, the release hands it back, an off-map note does
# nothing). A gate whose controls survive its own mutation is a gate whose controls are controls.
# ============================================================================
set -u
cd "$(dirname "$0")/../.." || exit 1

VERBOSE=0; [ "${1:-}" = "-v" ] && VERBOSE=1
OUT=$(mktemp -d); trap '[ "$VERBOSE" = 1 ] || rm -rf "$OUT"' EXIT
PASS=0; FAIL=0

ok()   { PASS=$((PASS+1)); printf '  \033[32m✓\033[0m %s\n' "$1"; }
bad()  { FAIL=$((FAIL+1)); printf '  \033[31m✗\033[0m %s\n' "$1"; }
note() { [ "$VERBOSE" = 1 ] && printf '      %s\n' "$1"; return 0; }

# The UI seeds. acidcandy boots on the 303A face, and since 2026-08-15 303A boots UNMUTED, so the
# acid line is audible with no setup at all — that default was changed precisely because a silent 303
# made working features look broken four times, this gate's transpose section among them.
# ⚠ un303.script is now EMPTY ON PURPOSE, not vestigial. It used to tap the cartridge LED to unmute;
# after the default flipped, that same tap MUTES the line and every transpose assertion below goes
# red on a rack that is working perfectly. Keeping the (empty) file makes the two sections read the
# same and leaves one place to put a seed back if the default ever moves again.
: > "$OUT/un303.script"                          # 303A is audible at boot — nothing to do
printf 'click 5 70 8\n' > "$OUT/to808.script"    # 808 cartridge  → the drum face has focus

# render <wav> <script> [--midi-note ...]
render() {
  local wav=$1 scr=$2; shift 2
  rm -rf build/saves/acidcandy
  node tools/play.js acidcandy script "$scr" --headless --frames 240 --wav "$wav" "$@" >/dev/null 2>&1
}
# trace <script> [--midi-note ...] → build/acidcandy.trace.jsonl
trace() {
  local scr=$1; shift
  rm -rf build/saves/acidcandy
  node tools/play.js acidcandy script "$scr" --headless --frames 240 "$@" >/dev/null 2>&1
}
sha()      { shasum "$1" | cut -d' ' -f1; }
centroid() { node tools/wav-envelope.js "$1" 2>/dev/null | grep -o 'centroid [0-9]* Hz' | head -1 | tr -cd '0-9'; }
# w <frame> <key> — read one watch() value out of the last trace
w() { node -e '
const fs=require("fs");
const L=fs.readFileSync("build/acidcandy.trace.jsonl","utf8").trim().split("\n")
        .map(s=>{try{return JSON.parse(s)}catch(e){return null}}).filter(x=>x&&x.w);
const r=L.find(l=>l.f===+process.argv[1]);
process.stdout.write(r ? String(r.w[process.argv[2]]) : "?");' "$1" "$2"; }

echo "midi-note-check — host MIDI notes → acidcandy"
echo

# ── 1. THE FLOOR ────────────────────────────────────────────────────────────────────────────────
echo "1. no-op control (nothing below means anything without this)"
render "$OUT/a.wav" "$OUT/un303.script"
render "$OUT/b.wav" "$OUT/un303.script"
A=$(sha "$OUT/a.wav"); B=$(sha "$OUT/b.wav")
note "sha $A"
[ "$A" = "$B" ] && ok "two untouched renders are byte-identical" \
                || bad "two untouched renders DIFFER ($A vs $B) — the harness is not deterministic, stop here"

# ── 2. TRANSPOSE ────────────────────────────────────────────────────────────────────────────────
echo
echo "2. transpose — a held key is the acid lines' root"
render "$OUT/same.wav" "$OUT/un303.script" --midi-note 60@1-240   # C = pitch class 0 = the panel's own root
render "$OUT/moved.wav" "$OUT/un303.script" --midi-note 62@1-240  # D = pitch class 2
S=$(sha "$OUT/same.wav"); M=$(sha "$OUT/moved.wav")
[ "$S" = "$A" ] && ok "holding the panel's OWN root renders byte-identical (the override is exact)" \
                || bad "holding C changed the render — the override is not neutral at the same value"
[ "$M" != "$A" ] && ok "holding a different root changes the audio" \
                 || bad "holding D changed nothing — transpose never reached the notes"

# direction, in the audio rather than in the code: the acid filter sits at a fixed Hz, so a line
# played higher puts more energy above it. Across a rising scale the centroid must rise too.
#
# ⚠ ON THE 303 STEM (--solo-slot 6), not the mix. Measured on the full rack the centroid sits around
# 7 kHz — that is the HATS — and it drifts DOWNWARD as the acid line rises, because what moves is the
# balance between two sources and not the pitch of either. A whole-mix centroid cannot answer a
# question about one voice.
echo
echo "3. it moves pitch UP, in order (spectral centroid of the 303 stem, roots 0·2·4·5·7)"
PREV=0; MONO=1; SEQ=""
for n in 60 62 64 65 67; do
  render "$OUT/sw.wav" "$OUT/un303.script" --solo-slot 6 --midi-note $n@1-240
  C=$(centroid "$OUT/sw.wav"); SEQ="$SEQ $C"
  [ -n "$C" ] || { MONO=0; break; }
  [ "$C" -gt "$PREV" ] || MONO=0
  PREV=$C
done
note "centroid Hz:$SEQ"
[ "$MONO" = 1 ] && ok "centroid rises monotonically:$SEQ Hz" \
                || bad "centroid did not rise monotonically:$SEQ Hz"
# ⚠ NOT formant-check.js. It was tried first and reported the pitch going DOWN — its autocorrelation
# returns the ends of its own 80–501 Hz search range on a resonant acid saw, so both readings were
# the analyser failing, not a measurement. A broken oracle and a real answer print the same thing.

# ── 4. THE HELD-NOTE STACK ──────────────────────────────────────────────────────────────────────
echo
echo "4. last-note priority, and the fallback when it lifts"
trace "$OUT/un303.script" --midi-note 60@20-70 --midi-note 67@30-50
[ "$(w 19 hroot)" = "-1" ] && ok "no key held → the panel owns the root" || bad "hroot=$(w 19 hroot) before any key (want -1)"
[ "$(w 20 hroot)" = "0" ]  && ok "first key takes the root"              || bad "hroot=$(w 20 hroot) on the first key (want 0)"
[ "$(w 30 hroot)" = "7" ]  && ok "a second key WINS (last-note priority)" || bad "hroot=$(w 30 hroot) with two keys down (want 7)"
[ "$(w 50 hroot)" = "0" ]  && ok "lifting it falls back to the key still down" || bad "hroot=$(w 50 hroot) after the newer key lifted (want 0)"
[ "$(w 70 hroot)" = "-1" ] && ok "the last release hands the panel back"  || bad "hroot=$(w 70 hroot) after all keys up (want -1)"
[ "$(w 30 root0)" = "7" ]  && ok "and the LINE plays it (root0 follows hroot)" || bad "root0=$(w 30 root0) while hroot=7"

# ── 5. THE KIT ──────────────────────────────────────────────────────────────────────────────────
echo
echo "5. drum face — the GM map, read backwards"
trace "$OUT/to808.script" --midi-note 38@30-40 --midi-note 42@45-55
[ "$(w 10 face)" = "2" ]  && ok "the 808 face has focus"                  || bad "face=$(w 10 face) after the nav tap (want 2 = M_808)"
[ "$(w 31 dsel)" = "1" ]  && ok "GM 38 → voice 1 (snare)"                 || bad "GM 38 selected voice $(w 31 dsel) (want 1)"
[ "$(w 46 dsel)" = "15" ] && ok "GM 42 → voice 15 (closed hat)"           || bad "GM 42 selected voice $(w 46 dsel) (want 15)"
[ "$(w 31 hroot)" = "-1" ] && ok "and it does NOT transpose the 303s underneath you" \
                           || bad "hroot=$(w 31 hroot) while finger-drumming (want -1)"

echo
echo "6. the kit SOUNDS, and only for notes that are in it"
render "$OUT/d_ctl.wav" "$OUT/to808.script"
render "$OUT/d_hit.wav" "$OUT/to808.script" --midi-note 38@40-45 --midi-note 38@70-75 --midi-note 38@100-105
render "$OUT/d_off.wav" "$OUT/to808.script" --midi-note 100@40-45 --midi-note 100@70-75 --midi-note 100@100-105
DC=$(sha "$OUT/d_ctl.wav"); DH=$(sha "$OUT/d_hit.wav"); DO=$(sha "$OUT/d_off.wav")
[ "$DH" != "$DC" ] && ok "GM snares change the render" || bad "GM snares changed nothing — the kit never fired"
[ "$DO" = "$DC" ]  && ok "a note OUTSIDE the kit (100) changes nothing — it is the MAP, not the traffic" \
                   || bad "an off-map note changed the render — something fires for any note at all"

# ── 7. THE PITCH LENS (row 2) ───────────────────────────────────────────────────────────────────
# The same keyboard, re-pointed: OFF it names a SOUND, ON it names a PITCH. Reaching the latch needs
# three taps (focus the face → open its PERF screen → tap PTCH), and the middle one is easy to forget:
# without it the third tap lands on whatever the LCD was showing and the lens never engages, which
# looks exactly like a broken feature. Coordinates are the 160×100 canvas, read off ui-audit.
printf 'click 5 12 65\nclick 12 118 56\n'                  > "$OUT/p303.script"   # 303a → PERF → PTCH
printf 'click 5 70 8\nclick 12 145 54\nclick 20 118 43\n'  > "$OUT/p808.script"   # 808  → PERF → PTCH

echo
echo "7. PITCH lens, 303 — the keys play the line, and mono.h resolves them"
rm -rf build/saves/acidcandy
node tools/play.js acidcandy script "$OUT/p303.script" --headless --frames 120 \
     --midi-note 60@40-70 --midi-note 67@50-60 >/dev/null 2>&1
[ "$(w 10 pmdf)" = "0" ]   && ok "off by default"                          || bad "pmdf=$(w 10 pmdf) before the tap (want 0)"
[ "$(w 20 pmdf)" = "1" ]   && ok "the PTCH latch engages"                   || bad "pmdf=$(w 20 pmdf) after tapping PTCH (want 1)"
[ "$(w 39 psnd)" = "-1" ]  && ok "silent until a key goes down"             || bad "psnd=$(w 39 psnd) with no key held (want -1)"
[ "$(w 41 psnd)" = "60" ]  && ok "a key SOUNDS the line"                    || bad "psnd=$(w 41 psnd) on the first key (want 60)"
[ "$(w 55 psnd)" = "67" ]  && ok "a stacked key wins (LAST priority)"       || bad "psnd=$(w 55 psnd) with two keys down (want 67)"
[ "$(w 61 psnd)" = "60" ]  && ok "lifting it hands back to the key still down" || bad "psnd=$(w 61 psnd) after the newer key lifted (want 60)"
[ "$(w 75 psnd)" = "-1" ]  && ok "the last release stops the voice"         || bad "psnd=$(w 75 psnd) after all keys up (want -1)"
# ⚠ AND the two lenses must not both fire. Transpose is off while PITCH owns the line, or a key would
# both play a note AND re-key the pattern under it — the one combination that is certainly wrong.
[ "$(w 55 hroot)" = "-1" ] && ok "transpose stays OFF while PITCH owns the line" || bad "hroot=$(w 55 hroot) while playing (want -1)"

echo
echo "8. PITCH lens, drums — ONE voice across the keys (the MPC's 16 LEVELS)"
rm -rf build/saves/acidcandy
node tools/play.js acidcandy script "$OUT/p808.script" --headless --frames 200 \
     --midi-note 48@60-65 --midi-note 72@100-105 >/dev/null 2>&1
[ "$(w 25 pmd8)" = "1" ] && ok "the 808's PTCH latch engages"        || bad "pmd8=$(w 25 pmd8) after tapping PTCH (want 1)"
[ "$(w 61 dsel)" = "0" ] && ok "a note does NOT re-select the voice" || bad "dsel=$(w 61 dsel) — a pitched note moved the selection"

# does it actually PITCH? On the KICK'S OWN STEM (--solo-slot 9 = TR808_BASE), never the mix: measured
# on the full rack the centroid sits at ~9 kHz because that is the HATS, and it moves non-monotonically
# while the kick sweeps two octaves underneath. Same trap as the 303 sweep above, caught the same way.
PREV=0; PMONO=1; PSEQ=""
for n in 60 72 84; do
  rm -rf build/saves/acidcandy
  node tools/play.js acidcandy script "$OUT/p808.script" --headless --frames 200 --solo-slot 9 \
       --wav "$OUT/pk.wav" --midi-note $n@60-65 --midi-note $n@100-105 --midi-note $n@140-145 >/dev/null 2>&1
  C=$(centroid "$OUT/pk.wav"); PSEQ="$PSEQ $C"
  [ -n "$C" ] || { PMONO=0; break; }
  [ "$C" -gt "$PREV" ] || PMONO=0
  PREV=$C
done
note "kick-stem centroid Hz:$PSEQ"
[ "$PMONO" = 1 ] && ok "the kick's own centroid rises with the note:$PSEQ Hz" \
                 || bad "the kick did not rise with the note:$PSEQ Hz"
# +24 is PAST the ±12 the TUNE knob can reach, which is the whole reason tr808_fire_semi exists —
# so a pass here is also the evidence that the header's wider path is live and not just present.

echo
if [ "$FAIL" -gt 0 ]; then printf '\033[31mFAIL\033[0m  %d passed, %d failed\n' "$PASS" "$FAIL"; exit 1; fi
printf '\033[32mPASS\033[0m  %d assertions\n' "$PASS"
[ "$VERBOSE" = 1 ] && echo "renders kept in $OUT"
exit 0
