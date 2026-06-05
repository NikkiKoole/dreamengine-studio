#!/bin/sh
# publish-cart.sh — build cart(s) for the web, commit, push → live on
# https://nikkikoole.github.io/dreamengine/ a minute later.
#
#   tools/publish-cart.sh <name> [<name>...]
#
# Steps: node tools/build-site.js <names> (wasm + gallery into site/),
# then commit ONLY site/ and push. The pages.yml workflow does the deploy.
# Aborts if anything outside site/ is already staged (another agent's WIP —
# see CLAUDE.md "parallel-agent commit hazards").
set -e
cd "$(dirname "$0")/.."

[ $# -ge 1 ] || { echo "usage: tools/publish-cart.sh <cart> [<cart>...]"; exit 1; }

node tools/build-site.js "$@"

git add site/
strays=$(git diff --cached --name-only | grep -v '^site/' || true)
if [ -n "$strays" ]; then
  echo "⚠ refusing to commit — files outside site/ are staged (someone else's WIP?):"
  echo "$strays"
  exit 1
fi
if git diff --cached --quiet; then
  echo "nothing new to publish"
  exit 0
fi

git commit -m "site: publish $*"
git push
echo "✓ pushed — deploy: https://github.com/NikkiKoole/dreamengine/actions"
echo "  live in ~1 min: https://nikkikoole.github.io/dreamengine/"
