#!/bin/zsh
# reddit-gaps-drip.sh — the scheduled-drip runner (loaded by the LaunchAgent
# com.dreamengine.reddit-gaps-drip). Fetches the STALEST sub in reddit-gaps-subs.txt, one per
# run, so successive fires round-robin the list without bursting Reddit's rate limit.
# Run it by hand anytime to advance the rotation: `zsh tools/reddit-gaps-drip.sh`.
# Logs (stdout/stderr) are captured by the plist into tools/reddit-gaps-cache/drip.log (gitignored).

REPO="/Users/nikkikoole/Projects/dreamengine"
cd "$REPO" || exit 1

# launchd runs with a minimal env — resolve node ourselves (nvm-managed here).
NODE="/Users/nikkikoole/.nvm/versions/node/v22.22.0/bin/node"
if [ ! -x "$NODE" ]; then
  export NVM_DIR="$HOME/.nvm"
  [ -s "$NVM_DIR/nvm.sh" ] && . "$NVM_DIR/nvm.sh" >/dev/null 2>&1
  NODE="$(command -v node)"
fi
[ -x "$NODE" ] || { echo "[drip] cannot find node"; exit 1; }

"$NODE" tools/reddit-gaps.js --drip --delay 5000
