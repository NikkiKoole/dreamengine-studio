#!/bin/zsh
# run.sh — the MIDI gate: phase A = out, phase B = CC in, phase C = cart-to-cart. Does the engine actually put the right bytes on the wire?
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
#
# PHASE C (cart to cart) runs epianojam INTO epiano and asserts on the receiver's rendered audio:
#   · the receiver SOUNDS, playing notes it was sent — the only end-to-end cover of the note
#     input path (parse → ring → midi_get → keybed.h → a voice), which A and B never touch
#   · and it is SILENT with no sender (epiano's autoplay switched off), without which "it made
#     noise" would prove nothing at all

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
# ⚠ THE SENDER'S LIFETIME IS A SAFETY NET, NOT A SCHEDULE — and getting that wrong is what made
# this phase FLAKY for weeks (diagnosed 2026-08-14; it "failed once and passed twice on identical
# code", and a throwaway worktree at HEAD was wrongly read as proof the engine was fine).
#
# The race, measured: the CC arrives at the cart's FRAME 1, so the only thing this phase needs is
# for the sender to still exist when the cart's engine initialises. But the sender used to live a
# fixed 12s of WALL CLOCK, while the cart's start time is a VARIABLE: play.js recompiles the engine
# on every invocation, and that compile was measured at 3.9s idle, 8.2s under one concurrent
# `build-all`, and 13.7s under three. Past ~11.3s the cart boots after the sender has exited and
# ALL SIX assertions fail together — which reads exactly like a broken CC parser.
#
# This repo runs several agents on one working tree, so "somebody else is compiling" is the normal
# state, not an unusual one. Hence: the sender now outlives any plausible compile, and the `kill`
# below is what actually ends it on the happy path. The bound only stops an orphan if run.sh dies.
"$OUT/send-cc" 180 > "$OUT/send.txt" 2>&1 &
SPID=$!
sleep 0.7   # publish before the cart scans for sources

# No --headless, same det-turbo reason as phase A: the cart must be alive in WALL time long enough
# to overlap the sender. --trace is where the assertions come from.
node tools/play.js midiout script /dev/null --frames 240 --trace "$TRACE" \
     > "$OUT/in-cart.txt" 2>&1 || { echo "FAIL: the reader cart did not run"; cat "$OUT/in-cart.txt"; kill $SPID 2>/dev/null; exit 1; }
# Was the sender STILL ALIVE when the cart finished? With the bound above this should always be
# true — and if it ever is not, say so instead of reporting six parse failures. A gate whose
# failure is indistinguishable from a real one will eventually be believed wrongly.
SENDER_ALIVE=0; kill -0 $SPID 2>/dev/null && SENDER_ALIVE=1
kill $SPID 2>/dev/null; wait $SPID 2>/dev/null

# watch() values are STRINGS in a trace line (debug-harness.md), hence the quoted patterns.
ckt() {  # ckt <description> <json-fragment>
  if grep -q -- "$2" "$TRACE" 2>/dev/null; then printf '  \033[32m✓\033[0m %-46s\n' "$1"
  else printf '  \033[31m✗\033[0m %-46s (no %s)\n' "$1" "$2"; fail=1; fi
}
if (( ! SENDER_ALIVE )); then
  # The gate raced; the engine was never asked. Distinguishing this from a real failure is the
  # whole point — every assertion below would go red and blame the CC parser.
  printf '  \033[31m✗\033[0m %-46s\n' "THE GATE RACED, not the engine"
  printf '      the sender exited before the cart booted (a slow compile under load — see the\n'
  printf '      note above the sender). Nothing was measured. Re-run on a quieter machine.\n'
  fail=1
elif [[ ! -s "$TRACE" ]]; then
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

# ══ PHASE C: CART TO CART — one cart's notes played by another cart's voice ══
# The maker's idea, and it closes the one real hole in this gate: phases A and B cover sending
# and CC-in, but NOTHING exercised the note INPUT path end to end (midi_input.h's note parse →
# the ring → midi_get() → keybed.h → an actual voice). Here `epianojam` sends and `epiano`
# receives, through real CoreMIDI, across two processes, with no DAW anywhere.
#
# The oracle is AUDIO, which is the honest one: the receiver renders to a WAV and it must be
# LOUD. What makes that mean something is the control — `epiano` autoplays a triad every two
# beats by default (autoplay = true), so a naive run makes plenty of sound while proving
# nothing. The script below presses M to turn autoplay OFF, which leaves the cart with no way
# to produce a note it was not sent: measured, that renders peak -inf, every second 0.000.
# So loud-vs-silent has exactly one explanation left.
echo "midi-check: phase C — cart to cart (epianojam → epiano)…"
CSCRIPT="$OUT/noauto.script"
printf '# M turns epiano AUTOPLAY off, so the only notes it can play are ones it RECEIVES\ndown 10 M\nup   16 M\n' > "$CSCRIPT"

peak_of() { node tools/wav-analyze.js "$1" 2>/dev/null | sed -n 's/.*peak .*(\([0-9.]*\)).*/\1/p' | head -1; }

# ⚠ CONTROL FIRST, and never `kill` the sender. play.js SPAWNS the cart as a child process, so
# killing the node parent leaves the cart binary orphaned and still sending — which is exactly how
# the first version of this phase failed: the "control" ran while a supposedly-dead sender was
# still playing, measured 0.407 instead of silence, and would have reported the whole phase as
# meaningless. Running the control before any sender exists removes the race instead of managing
# it, and the sender below is sized to finish on its own and simply waited for.
node tools/play.js epiano script "$CSCRIPT" --frames 600 --wav "$OUT/c-ctrl.wav" > "$OUT/c-ctrl.txt" 2>&1

# now the paired run. Sender first, with time to COMPILE before the receiver starts compiling —
# both play.js invocations write build/cart.c, so two simultaneous compiles clobber each other.
# 2100 frames = 35s, enough to cover the receiver's compile (~6s) and its 10s render from +12s.
node tools/play.js epianojam script "$SCRIPT" --frames 2100 --midi-out > "$OUT/c-sender.txt" 2>&1 &
SEND_PID=$!
sleep 12
node tools/play.js epiano script "$CSCRIPT" --frames 600 --wav "$OUT/c-recv.wav" > "$OUT/c-recv.txt" 2>&1
wait $SEND_PID 2>/dev/null   # let it end on its own — see the warning above

RECV=$(peak_of "$OUT/c-recv.wav"); CTRL=$(peak_of "$OUT/c-ctrl.wav")
RECV=${RECV:-0}; CTRL=${CTRL:-0}
if awk -v v="$RECV" 'BEGIN{exit !(v > 0.05)}'; then
  printf '  \033[32m✓\033[0m %-46s (peak %s)\n' "receiver SOUNDED on received notes" "$RECV"
else
  printf '  \033[31m✗\033[0m %-46s (peak %s, want >0.05)\n' "receiver heard nothing" "$RECV"; fail=1
fi
if awk -v v="$CTRL" 'BEGIN{exit !(v < 0.001)}'; then
  printf '  \033[32m✓\033[0m %-46s (peak %s)\n' "silent with no sender (control)" "$CTRL"
else
  printf '  \033[31m✗\033[0m %-46s (peak %s, want <0.001)\n' "CONTROL NOT SILENT: proves nothing" "$CTRL"; fail=1
fi

(( VERBOSE )) && { echo "--- listener log ---"; cat "$LOG"; echo "--- reader trace (last line) ---"; tail -1 "$TRACE" 2>/dev/null; }
if (( fail )); then echo "midi-check: \033[31mFAIL\033[0m  (logs: $OUT)"; exit 1; fi
echo "midi-check: \033[32mPASS\033[0m  (out + CC in + cart-to-cart)"
