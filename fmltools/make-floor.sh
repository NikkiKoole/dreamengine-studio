#!/usr/bin/env bash
# make-floor.sh — one-shot: Floorplanner .fml -> playable floorwalker-style cart.
#
#   fmltools/make-floor.sh <file.fml> <cartname> [floor] [scale] [maxfurn]
#
#   <file.fml>   path to a Floorplanner export (quote it if it has spaces)
#   <cartname>   cart id, e.g. "seinelaan"  -> tools/carts/seinelaan.c
#   [floor]      floor index to use         (default 0)
#   [scale]      cm per pixel               (default 8)
#   [maxfurn]    cm; items larger are skipped as surfaces (default 280)
#
# Runs the full pipeline (geometry -> bake sprites -> embed -> build -> thumbnail),
# registers the cart in index.json, and prints the play command. Re-running with the
# same name regenerates in place. Tweak the bake look via env vars:
#   SATURATE=2.2 POSTERIZE=5 OUTLINE=1 MAX=24 fmltools/make-floor.sh ...
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

FML="${1:-}"; NAME="${2:-}"; FLOOR="${3:-0}"; SCALE="${4:-8}"; MAXFURN="${5:-280}"
if [ -z "$FML" ] || [ -z "$NAME" ]; then
  sed -n '2,18p' "$0"; exit 1
fi
[ -f "$FML" ] || { echo "error: no such file: $FML" >&2; exit 1; }
case "$NAME" in *[!a-z0-9_-]*) echo "error: cartname must be [a-z0-9_-]: $NAME" >&2; exit 1;; esac

MAX="${MAX:-24}"; SATURATE="${SATURATE:-2.2}"; POSTERIZE="${POSTERIZE:-5}"
# outline OFF by default: the cart draws a crisp render-time silhouette outline,
# so a baked-in one would just double up (and breaks under downscaling). Set
# OUTLINE=1 only if you want it baked into the sprite instead.
OUTLINE="${OUTLINE:-0}"; OUTFLAG=""; [ "$OUTLINE" = "1" ] && OUTFLAG="--outline"

CART="tools/carts/$NAME.c"
PNG="editor/public/carts/$NAME.cart.png"
ASSETS="build/.fml-assets-$NAME"
TEMPLATE="tools/carts/floorwalker.c"

if [ "$CART" = "$TEMPLATE" ]; then
  echo "▸ template  (regenerating the template cart in place)"
else
  echo "▸ template  $TEMPLATE -> $CART"
  cp "$TEMPLATE" "$CART"
fi

echo "▸ geometry  (floor $FLOOR, ${SCALE}cm/px, maxfurn ${MAXFURN}cm)"
node fmltools/fml2cart.js "$FML" --out "$CART" --scale "$SCALE" --maxfurn "$MAXFURN" --floor "$FLOOR"

echo "▸ bake art  (max ${MAX}px, saturate ${SATURATE}, posterize ${POSTERIZE}, outline ${OUTLINE})"
node fmltools/fml-assets.js "$FML" --max "$MAX" --saturate "$SATURATE" --posterize "$POSTERIZE" $OUTFLAG --out "$ASSETS"

echo "▸ embed sprites"
node fmltools/fml-sprites.js --manifest "$ASSETS/manifest.json" --out "$CART"

# HUD label -> cart name (uppercased)
UPPER="$(printf '%s' "$NAME" | tr '[:lower:]' '[:upper:]')"
sed -i '' "s/FLOORWALKER  furn/$UPPER  furn/" "$CART" 2>/dev/null || \
  sed -i "s/FLOORWALKER  furn/$UPPER  furn/" "$CART"

echo "▸ build + thumbnail"
node tools/make-cart.js "$CART" "$PNG" >/dev/null
node tools/make-cart.js --run "$PNG" >/dev/null

echo "▸ register in index.json"
node -e '
const fs=require("fs"), p="editor/public/carts/index.json";
const name=process.argv[1], file=name+".cart.png";
const idx=JSON.parse(fs.readFileSync(p,"utf8"));
if(!idx.some(e=>e.file===file)){
  idx.push({title:name, description:"A Floorplanner .fml walked as a top-down level (built by fmltools/make-floor.sh). Walls with walkable doorways and real door/window sprites, each room a distinct floor colour, furniture as baked top-down pixel sprites. WASD/arrows move, T toggles sprites vs boxes.", file, kind:["tech-demo","toy"], teaches:[]});
  fs.writeFileSync(p, JSON.stringify(idx,null,2)+"\n");
  console.log("  registered "+file);
} else console.log("  already registered");
' "$NAME"
if node tools/lint-carts.js >/dev/null 2>&1; then echo "  lint ok"
else echo "  (lint reported issues — likely pre-existing/foreign carts; check: node tools/lint-carts.js)"; fi

echo ""
echo "✓ done.  play it:  node tools/play.js $NAME run"
