#!/usr/bin/env bash
# proof-sound.sh — build + run the de_switch_cart round-trip proof (see proof-sound.c),
# then judge the WAV: the same note on the same slot must sound IDENTICAL before the
# switch-away and after the switch-back (corr ≥ 0.99), and genuinely DIFFERENT while
# the other cart's context is active (corr ≤ 0.80). Deterministic; exits nonzero on fail.
#
#   tools/bundle-spike/proof-sound.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
S="$ROOT/build/.bundle-spike"; mkdir -p "$S"
RAYLIB="$(brew --prefix raylib)"

node -e '
const mk = require(process.argv[1] + "/tools/make-cart.js"), fs = require("fs")
fs.writeFileSync(process.argv[2] + "/sprites.png", mk.makeBlankSpritePng())
fs.writeFileSync(process.argv[2] + "/map.dat", Buffer.alloc(8192))
' "$ROOT" "$S"
(cd "$S" && xxd -i sprites.png) | sed \
  -e 's/unsigned char sprites_png\[\]/static const unsigned char SPRITES_DATA[]/' \
  -e 's/unsigned int sprites_png_len/static const unsigned int  SPRITES_DATA_LEN/' > "$S/sprites_data.h"
(cd "$S" && xxd -i map.dat) | sed \
  -e 's/unsigned char map_dat\[\]/static const unsigned char MAP_DATA[]/' \
  -e 's/unsigned int map_dat_len/static const unsigned int  MAP_DATA_LEN/' > "$S/map_data.h"

clang "$(dirname "$0")/proof-sound.c" "$ROOT/runtime/studio.c" \
  -I"$ROOT/runtime" -I"$S" -I"$RAYLIB/include" \
  -DSCREEN_W=320 -DSCREEN_H=240 -DSCALE=3 -DMAP_W=128 -DMAP_H=64 -DCELL_W=16 -DCELL_H=16 \
  -DTOUCH_CONTROLS_DEFAULT=0 -DSCALE_FILTER=0 -O2 -fno-delete-null-pointer-checks \
  "$RAYLIB/lib/libraylib.a" \
  -framework OpenGL -framework Cocoa -framework IOKit \
  -framework CoreVideo -framework CoreFoundation -framework CoreMIDI \
  -Wl,-dead_strip -o "$S/proof-sound" 2> >(grep -v "was built for newer macOS version" >&2 || true)

(cd "$S" && ./proof-sound --headless --frames 270 --wav "$S/proof.wav" >/dev/null)

node -e '
const fs = require("fs"), b = fs.readFileSync(process.argv[1]);
let p = 12, sr = 44100, ch = 1, off = 44, len = 0;
while (p + 8 <= b.length) {
  const id = b.toString("ascii", p, p + 4), sz = b.readUInt32LE(p + 4);
  if (id === "fmt ") { ch = b.readUInt16LE(p + 10); sr = b.readUInt32LE(p + 12); }
  if (id === "data") { off = p + 8; len = sz; break; }
  p += 8 + sz + (sz & 1);
}
const stride = 2 * ch, n = Math.floor(len / stride), x = new Float32Array(n);
for (let i = 0; i < n; i++) x[i] = b.readInt16LE(off + i * stride) / 32768;
const seg = (s, N) => {                       // zero-mean, unit-norm window
  const y = Float32Array.from(x.slice(s, s + N));
  let m = 0; for (const v of y) m += v; m /= y.length;
  let e = 0; for (let i = 0; i < y.length; i++) { y[i] -= m; e += y[i] * y[i]; }
  const q = Math.sqrt(e) || 1; for (let i = 0; i < y.length; i++) y[i] /= q;
  return y;
};
// note fire snaps to the audio buffer boundary, so allow a generous lag search
const corr = (sA, sB, N) => {
  const a = seg(sA, N); let best = -1;
  for (let lag = -1536; lag <= 1536; lag += 8) {
    const c = seg(sB + lag, N); let d = 0;
    for (let i = 0; i < N; i++) d += a[i] * c[i];
    if (d > best) best = d;
  }
  return best;
};
const N = Math.round(sr * 0.45);
const A = Math.round(sr * 0.5), B = Math.round(sr * 2.0), A2 = Math.round(sr * 3.5);
const same = corr(A, A2, N), diff = corr(A, B, N);
console.log(`corr(A, A_restored) = ${same.toFixed(4)}   (want >= 0.99)`);
console.log(`corr(A, B_other)    = ${diff.toFixed(4)}   (want <= 0.80 — proves the sounds differ)`);
if (same >= 0.99 && diff <= 0.80) { console.log("PASS — de_switch_cart round-trip restored the sound world"); }
else { console.log("FAIL"); process.exit(1); }
' "$S/proof.wav"
