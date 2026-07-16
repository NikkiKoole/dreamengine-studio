#!/usr/bin/env bash
# android/build.sh — the one-command Android run loop (twin of ios/build.sh).
#
#   ./build.sh                 # build omnichord, install + launch on the emulator/device, screenshot
#   CART=tb303 ./build.sh      # any tools/carts/<name>.c
#   SHOT=snap.png ./build.sh   # also save a screenshot to <repo>/build/android/out/snap.png
#   NO_RUN=1 ./build.sh        # build the APK only (no install/launch)
#
# It: (1) regenerates the cart's build/{cart.c,sprites_data.h,map_data.h} via play.js and
# IMMEDIATELY stages them into app/src/main/cpp/gen/ (isolating from the shared build/ dir
# that sibling processes overwrite), (2) writes the cart's screen/cell/map dims into
# gradle.properties, (3) Gradle-builds the arm64 APK, (4) installs + launches + screenshots.
# See docs/design/android-plan.md.
set -euo pipefail
cd "$(dirname "$0")"                     # android/
REPO="$(cd .. && pwd)"

CART="${CART:-omnichord}"

# Homebrew's prefix differs by machine: Apple Silicon = /opt/homebrew, Intel = /usr/local.
# So JAVA_HOME/ANDROID_HOME can't be hardcoded — this works on both, and when launched from the
# Electron editor (whose GUI env often has no `brew` on PATH). Order: explicit env override →
# HOMEBREW_PREFIX (set by a configured brew shell) → `brew --prefix` if on PATH → detect which
# known prefix actually has the JDK → default to Apple Silicon.
BREW_PREFIX="${HOMEBREW_PREFIX:-}"
[ -z "$BREW_PREFIX" ] && command -v brew >/dev/null 2>&1 && BREW_PREFIX="$(brew --prefix)"
if [ -z "$BREW_PREFIX" ]; then
  for p in /opt/homebrew /usr/local; do [ -d "$p/opt/openjdk@17" ] && BREW_PREFIX="$p" && break; done
fi
BREW_PREFIX="${BREW_PREFIX:-/opt/homebrew}"
export JAVA_HOME="${JAVA_HOME:-$BREW_PREFIX/opt/openjdk@17}"
export ANDROID_HOME="${ANDROID_HOME:-$BREW_PREFIX/share/android-commandlinetools}"
export PATH="$ANDROID_HOME/platform-tools:$PATH"

GEN="app/src/main/cpp/gen"
mkdir -p "$GEN"

if [ "${EDITOR:-}" = "1" ]; then
  # EDITOR mode: the Electron editor already wrote build/{cart.c,sprites_data.h,map_data.h} from
  # the LIVE buffer (main.cjs prepareCart) and passes dims via DE_* env — don't regenerate from a
  # committed cart. Mirrors ios/build.sh's editor path. build.sh's caller sets CART for the label.
  echo "==> EDITOR build: staging the live buffer from build/ into gen/"
  SW="${DE_SCREEN_W:-320}"; SH="${DE_SCREEN_H:-200}"
  CW="${DE_CELL_W:-16}";    CH="${DE_CELL_H:-16}"
  MW="${DE_MAP_W:-128}";    MH="${DE_MAP_H:-64}"
else
  echo "==> regenerating cart '$CART' and staging into gen/"
  ( cd "$REPO" && node tools/play.js "$CART" run --headless --frames 1 >/dev/null 2>&1 || true )
  # cart dims (same source as tools/build-nr.sh)
  dim() { node -e "const m=require('$REPO/tools/make-cart.js');const c=m.loadConfig('$REPO/tools/carts/$CART.c');process.stdout.write(String(c.$1??$2))"; }
  SW=$(dim screenW 320); SH=$(dim screenH 200)
  CW=$(dim cellW 16);   CH=$(dim cellH 16)
  MW=$(dim mapW 128);   MH=$(dim mapH 64)
fi

cp "$REPO/build/cart.c"          "$GEN/cart.c"
cp "$REPO/build/sprites_data.h"  "$GEN/sprites_data.h"
cp "$REPO/build/map_data.h"      "$GEN/map_data.h"
echo "    $CART: ${SW}x${SH}, cell ${CW}x${CH}, map ${MW}x${MH}"

# rewrite the DE_* dims in gradle.properties in place
python3 - "$SW" "$SH" "$CW" "$CH" "$MW" "$MH" "$CART" <<'PY'
import sys, re
sw, sh, cw, ch, mw, mh, cart = sys.argv[1:8]
vals = {"DE_CART":cart,"DE_SCREEN_W":sw,"DE_SCREEN_H":sh,"DE_CELL_W":cw,"DE_CELL_H":ch,"DE_MAP_W":mw,"DE_MAP_H":mh}
p = "gradle.properties"; t = open(p).read()
for k,v in vals.items():
    t = re.sub(rf"(?m)^{k}=.*$", f"{k}={v}", t)
open(p,"w").write(t)
PY

echo "==> gradle assembleDebug (arm64-v8a)"
./gradlew :app:assembleDebug --console=plain -q

APK="app/build/outputs/apk/debug/app-debug.apk"
if [ "${NO_RUN:-}" = "1" ]; then echo "built $APK (NO_RUN)"; exit 0; fi

echo "==> install + launch"
adb install -r "$APK" >/dev/null
adb shell am force-stop com.dreamengine.cart || true
adb shell am start -n com.dreamengine.cart/android.app.NativeActivity >/dev/null
echo "    launched com.dreamengine.cart ($CART)"

if [ -n "${SHOT:-}" ]; then
    sleep 3
    mkdir -p "$REPO/build/android/out"
    adb exec-out screencap -p > "$REPO/build/android/out/$SHOT"
    echo "    screenshot -> build/android/out/$SHOT"
fi
