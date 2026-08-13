#!/bin/zsh
# run.sh — the MIDI gate, BOTH directions (phase A = out, phase B = in). Does the engine actually put the right bytes on the wire?
#
# The twin of tools/sync-spike/run.sh (which gates the IN direction) and the reason there is
# "no excuse for shipping this one on a listen": a cart's own screen showing "sent note 36"
# proves only that the cart CALLED us. This runs a real listener in a SECOND PROCESS and
# asserts what arrived — channel, note, velocity, controller — through real CoreMIDI.
#
# Needs NO IAC bus and no DAW, unlike the sync spike: MIDISourceCreate publishes the engine as
# a source directly, so the listener just connects to it. macOS only; exits 0 (skip) elsewhere.
#
#   zsh tools/midi-check/run.sh          # the gate
#   zsh tools/midi-check/run.sh -v       # keep the raw listener log
#
# What it asserts (all on the ONE run of `midiout`):
#   · the source appears at all                     — the virtual source was published
#   · a bassline note-on lands on CHANNEL 1         — pitched part, its own channel
#   · GM kick 36 / snare 38 / hat 42 on CHANNEL 10  — drum voices are NOTES, not channels
#   · every note-on is matched by a note-off        — no stuck notes (the classic MIDI sin)
#   · CC 74 arrives on channel 1                    — the knob path
#   · clock ticks arrive, and START does            — transport out
#   · and the NEGATIVE control: the same run WITHOUT --midi-out publishes nothing at all
#
# PHASE B (in) drives the engine from send-cc.c and asserts out of the cart's --trace:
#   · CC arrives at all, and on THREE different channels (1, 10, 16)
#   · the omni read (ch 0) agrees with the per-channel one
#   · channel ISOLATION — cc 74 read on channel 2 is -1, which is the only check that can
#     actually see a thrown-away channel nibble (see the note at that assertion)

set -u
cd "$(dirname "$0")/../.."
ROOT="$(pwd)"
VERBOSE=0; [[ "${1:-}" == "-v" ]] && VERBOSE=1

if [[ "$(uname)" != "Darwin" ]]; then
  echo "midi-check: SKIP (macOS only — CoreMIDI)"; exit 0
fi

OUT="$ROOT/build/midi-check"
mkdir -p "$OUT"

echo "midi-check: building the listener…"
clang -O1 -o "$OUT/listen" tools/midi-check/listen.c \
      -framework CoreMIDI -framework CoreFoundation || { echo "FAIL: listener did not build"; exit 1; }

# ── the run: listener first (the source is created lazily, on the cart's first send) ──
# 420 frames at 60fps = 7s of cart time (6s of it playing), and NOTE THE MISSING --headless: that is deliberate
# and was the hardest part of getting this gate to work at all.
#
# A run that is deterministic AND hidden AND frame-bounded trips "det-turbo" (studio.c:3519):
# the loop uncaps and a light cart replays ~40× realtime. So `--headless --frames 240` finishes
# in about 100ms, the lazily-created virtual source is born and disposed inside that window, and
# a listener polling every 100ms sees nothing — while the engine was behaving perfectly. The
# first version of this gate failed exactly that way and the failure pointed at the engine.
#
# Dropping --headless keeps determinism (the script still implies --det) but restores the 60fps
# cap, so the port stays open for a real 4 seconds. Cost: a window appears briefly. Worth it —
# and it is also the more honest exercise, since a real app sends in real time too.
# The script plays from frame 4 via the pulse below.
SCRIPT="$OUT/play.script"
# a keyp() needs a down→up PULSE with a gap, not a single event (debug-harness.md).
#
# SPACE at frame 60, not frame 4, and that ONE SECOND of delay is load-bearing. The engine
# publishes its source lazily on the cart's first midi call (draw()'s midi_out_ready(), frame 1),
# and CoreMIDI then needs a moment to notify the listener so it can attach. Pressing play at
# frame 4 sends START ~66ms later — before the attach completes — so every other assertion
# passed while START alone was missing, which reads like a broken transport rather than a race.
printf '# press SPACE at 1s, giving the listener time to attach before the transport fires\ndown 60 SPACE\nup   66 SPACE\n' > "$SCRIPT"

# Pre-compile OUTSIDE the timed window. play.js rebuilds the cart on every invocation, and a
# cold -Os build of the engine can outlast the listener's attach window — which reads as "the
# engine sent nothing" when it simply had not started yet. No --midi-out here, so this warm-up
# publishes nothing (and doubles as a check that the negative control's build path is the same).
echo "midi-check: pre-compiling…"
node tools/play.js midiout script /dev/null --headless --frames 2 > "$OUT/precompile.txt" 2>&1 \
  || { echo "FAIL: midiout does not compile"; cat "$OUT/precompile.txt"; exit 1; }

echo "midi-check: listening + running midiout…"
"$OUT/listen" 20 dreamengine > "$OUT/log.txt" 2>"$OUT/listen.err" &
LPID=$!
sleep 0.7   # let the listener install its port before the cart publishes

node tools/play.js midiout script "$SCRIPT" --frames 420 --midi-out \
     > "$OUT/cart.txt" 2>&1 || { echo "FAIL: the cart did not run"; cat "$OUT/cart.txt"; kill $LPID 2>/dev/null; exit 1; }

wait $LPID
LOG="$OUT/log.txt"

# ── assertions ──
fail=0
ck() {  # ck <description> <min-count> <grep-pattern>
  local n; n=$(grep -c -- "$3" "$LOG" 2>/dev/null || true)
  if (( n >= $2 )); then printf '  \033[32m✓\033[0m %-46s (%d)\n' "$1" "$n"
  else                   printf '  \033[31m✗\033[0m %-46s (%d, want >=%d)\n' "$1" "$n" "$2"; fail=1; fi
}

echo "midi-check: asserting…"
ck "virtual source published"              1  "src dreamengine"
ck "bassline note-on on channel 1"         4  "note on  ch=1 "
ck "GM kick 36 on channel 10"              4  "note on  ch=10 note=36"
ck "GM snare 38 on channel 10"             2  "note on  ch=10 note=38"
ck "GM closed hat 42 on channel 10"        8  "note on  ch=10 note=42"
ck "CC 74 (cutoff) on channel 1"           2  "cc       ch=1 cc=74"
ck "transport START"                       1  "start"

# no stuck notes: every on has an off. Counted rather than matched pairwise — a leak shows as
# a mismatch, which is the failure that matters (a receiver left droning after the cart quits).
# GATE LENGTH — a note-off must arrive some milliseconds AFTER its note-on, not in the same
# instant. This check exists because the maker found the defect it catches by playing into
# GarageBand, while the on/off balance check below sat green: a zero-length note is perfectly
# balanced. Live-monitoring one is an inaudible blip, but a DAW that RECORDS it stores two events
# at the same timestamp and normalises them on playback, so the recording gains notes you never
# heard. The pair-counting check cannot see that; only the clock can.
MINGATE=$(awk '
  /note on / { split($0,a,"["); split(a[2],b,"]"); t=b[1]+0
               n=$0; sub(/.*ch=/,"",n); split(n,f," "); key=f[1]; sub(/note=/,"",f[2]); key=key" "f[2]
               on[key]=t; next }
  /note off/ { split($0,a,"["); split(a[2],b,"]"); t=b[1]+0
               n=$0; sub(/.*ch=/,"",n); split(n,f," "); key=f[1]; sub(/note=/,"",f[2]); key=key" "f[2]
               if (key in on) { d=t-on[key]; if (min=="" || d<min) min=d; delete on[key] } }
  END { if (min=="") print -1; else printf "%.0f", min }' "$LOG")
if [[ "$MINGATE" != "-1" ]] && (( MINGATE >= 20 )); then
  printf '  \033[32m✓\033[0m %-46s (%sms)\n' "shortest note has a real gate length" "$MINGATE"
else
  printf '  \033[31m✗\033[0m %-46s (%sms, want >=20)\n' "ZERO-LENGTH NOTE (records wrong)" "$MINGATE"; fail=1
fi

ON=$(grep -c "note on " "$LOG" || true); OFF=$(grep -c "note off" "$LOG" || true)
if [[ "$ON" == "$OFF" ]]; then printf '  \033[32m✓\033[0m %-46s (%s=%s)\n' "every note-on matched by a note-off" "$ON" "$OFF"
else                           printf '  \033[31m✗\033[0m %-46s (on=%s off=%s)\n' "STUCK NOTES: on/off mismatch" "$ON" "$OFF"; fail=1; fi

# clock: ~6s of playing at 120bpm = 12 beats = ~288 ticks. Allow slack at the tail (the listener may
# close a hair early); the point is that ticks flow at roughly the advertised rate, not that
# the last one arrived.
CLK=$(sed -n 's/^done clocks=//p' "$LOG" | tail -1)
CLK=${CLK:-0}
if (( CLK >= 150 )); then printf '  \033[32m✓\033[0m %-46s (%s)\n' "clock ticks at ~24ppqn" "$CLK"
else                      printf '  \033[31m✗\033[0m %-46s (%s, want >=150)\n' "clock ticks too few" "$CLK"; fail=1; fi

# ── NEGATIVE CONTROL — the half that makes the gate mean something ──
# Every check above would also pass if the engine sent MIDI unconditionally, which is exactly
# the behaviour --midi-out exists to prevent. So run the SAME cart with the flag removed and
# assert the wire stays silent. Without this the gate cannot tell "correctly gated" from
# "not gated at all" — the same blindness that let sync_automated sit inert inside an #ifdef.
echo "midi-check: negative control (no --midi-out → must publish nothing)…"
"$OUT/listen" 12 dreamengine > "$OUT/log-neg.txt" 2>/dev/null &
NPID=$!
sleep 0.7
node tools/play.js midiout script "$SCRIPT" --frames 420 > /dev/null 2>&1
wait $NPID
if grep -q "src dreamengine" "$OUT/log-neg.txt"; then
  printf '  \033[31m✗\033[0m %-46s\n' "NOT GATED: headless run published a source"; fail=1
else
  printf '  \033[32m✓\033[0m %-46s\n' "silent without --midi-out"
fi

# ══ PHASE B: the IN direction — does the engine RECEIVE CC, with the channel intact? ══
# The mirror of everything above, and it needs its own driver because the engine listens to MIDI
# *sources* and is never a *destination* — so send-cc.c publishes itself as a source (which also
# means no IAC bus here either). Three CCs on three different channels (1, 10, 16), because the
# channel nibble is the part of a fresh CC parser most likely to be quietly wrong, and a
# single-channel test cannot see it.
echo "midi-check: phase B — CC in…"
clang -O1 -o "$OUT/send-cc" tools/midi-check/send-cc.c \
      -framework CoreMIDI -framework CoreFoundation || { echo "FAIL: sender did not build"; exit 1; }

TRACE="$OUT/in.trace.jsonl"
"$OUT/send-cc" 12 > "$OUT/send.txt" 2>&1 &
SPID=$!
sleep 0.7   # publish before the cart scans for sources

# No --headless, same det-turbo reason as phase A: the cart must be alive in WALL time long enough
# to overlap the sender. --trace is where the assertions come from.
node tools/play.js midiout script /dev/null --frames 240 --trace "$TRACE" \
     > "$OUT/in-cart.txt" 2>&1 || { echo "FAIL: the reader cart did not run"; cat "$OUT/in-cart.txt"; kill $SPID 2>/dev/null; exit 1; }
kill $SPID 2>/dev/null; wait $SPID 2>/dev/null

# watch() values are STRINGS in a trace line (debug-harness.md), hence the quoted patterns.
ckt() {  # ckt <description> <json-fragment>
  if grep -q -- "$2" "$TRACE" 2>/dev/null; then printf '  \033[32m✓\033[0m %-46s\n' "$1"
  else printf '  \033[31m✗\033[0m %-46s (no %s)\n' "$1" "$2"; fail=1; fi
}
if [[ ! -s "$TRACE" ]]; then
  printf '  \033[31m✗\033[0m %-46s\n' "reader produced no trace"; fail=1
else
  ckt "CC arrived at all"                    '"in_n":"[1-9]'
  ckt "ch 1  cc 74 = 100 (pitched channel)"  '"cc1_74":"100"'
  ckt "ch 10 cc 7  = 55  (drum channel)"     '"cc10_7":"55"'
  ckt "ch 16 cc 1  = 127 (top channel)"      '"cc16_1":"127"'
  ckt "omni read agrees with per-channel"    '"omni74":"100"'
  # THE DISCRIMINATOR that makes the three above mean something. An implementation that ignored
  # the channel and wrote every CC into one per-cc slot would still pass all three, because each
  # cc NUMBER is distinct — so those checks alone cannot see a dropped nibble. cc 74 is only ever
  # sent on channel 1, so reading it on channel 2 MUST be -1 (never seen). If that shows 100, the
  # channel was thrown away and every "channel-aware" claim in the docs is false.
  ckt "channel isolation: ch2 cc74 unseen (-1)" '"cc2_74":"-1"'
fi

(( VERBOSE )) && { echo "--- listener log ---"; cat "$LOG"; echo "--- reader trace (last line) ---"; tail -1 "$TRACE" 2>/dev/null; }
if (( fail )); then echo "midi-check: \033[31mFAIL\033[0m  (logs: $OUT)"; exit 1; fi
echo "midi-check: \033[32mPASS\033[0m  (out + in)"
