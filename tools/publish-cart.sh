#!/bin/sh
# publish-cart.sh — build cart(s) for the web, commit, push → live on
# https://nikkikoole.github.io/dreamengine/ a minute later.
#
#   tools/publish-cart.sh <name> [<name>...]
#
# TWO-REPO layout (2026-07-06, the private-code/public-site split —
# design/sharing-channels.md): site/ is gitignored here and is its OWN checkout
# of the public NikkiKoole/dreamengine repo, whose main branch GitHub Pages
# serves directly (no workflow). This code repo (dreamengine-studio) can flip
# private without touching the published site.
#
# Steps: node tools/build-site.js <names> (wasm + gallery into site/), then
# commit + push INSIDE site/ (its own repo — a plain add -A is safe there), then
# a write-back commit in THIS repo of just the published carts' sources, by
# pathspec (parallel-agent guard — see CLAUDE.md "parallel-agent commit hazards").
set -e
cd "$(dirname "$0")/.."

# --no-build: site/<name>/ is already compiled (the editor's publish button
# builds straight into it) — skip the build, just finish the git leg.
# -m "msg": override the commit subject (publish-all.js uses this so a big
# batch doesn't produce a thousand-cart subject line). Flags may come in any order.
NOBUILD=0
MSG=""
while :; do
  case "${1:-}" in
    --no-build) NOBUILD=1; shift ;;
    -m) MSG="$2"; shift 2 ;;
    *) break ;;
  esac
done

[ $# -ge 1 ] || { echo "usage: tools/publish-cart.sh [--no-build] [-m msg] <cart> [<cart>...]"; exit 1; }

[ $NOBUILD -eq 1 ] || node tools/build-site.js "$@"

[ -d site/.git ] || { echo "⚠ site/ is not a git checkout — clone the public site repo first:"; \
  echo "  git clone git@github.com:NikkiKoole/dreamengine.git site"; exit 1; }

# ── site leg: commit + push inside site/'s OWN repo. Everything under site/
# belongs to it, so add -A can't sweep foreign WIP (the shared-index hazard
# lives in the code repo, not here).
(
  cd site
  git add -A
  if git diff --cached --quiet; then
    echo "site: nothing new to publish"
  else
    git commit -m "${MSG:-publish $*}"
    git push
    echo "✓ site pushed — live in ~1 min: https://nikkikoole.github.io/dreamengine/"
  fi
)

# ── write-back leg (code repo): keep the published carts' SOURCES in sync.
# Stage only those files and commit by that exact pathspec — foreign staged
# WIP is never swept (CLAUDE.md commit hazard 1).
files=""
for n in "$@"; do
  for f in "tools/carts/$n.c" "tools/carts/$n.cart.js"; do
    [ -f "$f" ] && git add "$f" && files="$files $f"
  done
done
if [ -n "$files" ] && ! git diff --cached --quiet -- $files; then
  git commit -m "${MSG:-carts: publish write-back $*}" -- $files
  git push
fi
