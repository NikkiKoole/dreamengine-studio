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
export JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@17}"
export ANDROID_HOME="${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}"
export PATH="$ANDROID_HOME/platform-tools:$PATH"

echo "==> regenerating cart '$CART' and staging into gen/"
( cd "$REPO" && node tools/play.js "$CART" run --headless --frames 1 >/dev/null 2>&1 || true )
GEN="app/src/main/cpp/gen"
mkdir -p "$GEN"
cp "$REPO/build/cart.c"          "$GEN/cart.c"
cp "$REPO/build/sprites_data.h"  "$GEN/sprites_data.h"
cp "$REPO/build/map_data.h"      "$GEN/map_data.h"

# cart dims (same source as tools/build-nr.sh)
dim() { node -e "const m=require('$REPO/tools/make-cart.js');const c=m.loadConfig('$REPO/tools/carts/$CART.c');process.stdout.write(String(c.$1??$2))"; }
SW=$(dim screenW 320); SH=$(dim screenH 200)
CW=$(dim cellW 16);   CH=$(dim cellH 16)
MW=$(dim mapW 128);   MH=$(dim mapH 64)
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
