#!/bin/bash
# OneMind on the owner's Apple Silicon Mac: host checks, Development editor build,
# package, run. One command for OWNER-STEPS.md S1/F1 (and the base for S5, the
# 10,000-entity check, C1 and C2).
#
#   UE="/Users/Shared/Epic Games/UE_5.8" ./build-and-run.sh [check|build|package|package-shipping|run|all]
#
# CONFIG picks the package configuration (default Development). The owner measurements
# (B1/B3/B4/C2) need Development: Shipping compiles out the console (`stat`, the
# OneMindMarker/OneMindChat commands), UE_LOG (the log lines OWNER-STEPS greps for) and
# trace. `package-shipping` (CONFIG=Shipping) is only for the F1 install/signing check.
# Each configuration is archived to its own folder: $OUT/<Config>/Mac*/OneMind.app.
#
# Nothing here needs a secret. NATS credentials, if the leaf needs them, are read by the app
# from ONEMIND_NATS_USER / ONEMIND_NATS_PASSWORD in the environment at run time.
set -euo pipefail

HERE=$(cd "$(dirname "$0")/../.." && pwd)          # client/OneMind
UPROJECT="$HERE/OneMind.uproject"
UE=${UE:-"/Users/Shared/Epic Games/UE_5.8"}
OUT=${OUT:-"$HOME/onemind-build/mac"}
CONFIG=${CONFIG:-Development}
STEP=${1:-all}

say() { printf '\n== %s\n' "$*"; }

check() {
  say "host checks (unreal-platform.md U0)"
  echo "macOS $(sw_vers -productVersion) on $(uname -m)  (need >= 14.5 on arm64)"
  XC=$(xcodebuild -version | head -1 | awk '{print $2}')
  echo "Xcode $XC  (UE 5.8 needs 26.0 to 26.3; 26.4 is not compatible)"
  case "$XC" in 26.0*|26.1*|26.2*|26.3*) ;; *) echo "STOP: install Xcode 26.1.1 beside this one and xcode-select it (OWNER-STEPS.md S1 step 1)"; exit 1;; esac
  echo "free disk: $(df -h "$HOME" | tail -1 | awk '{print $4}')  (need about 200 GiB for UE + build)"
  test -x "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" || { echo "STOP: Unreal 5.8 not found at $UE"; exit 1; }
  CES=$(grep -h '"VersionName"' "$UE"/Engine/Plugins/Marketplace/*/CesiumForUnreal.uplugin 2>/dev/null | head -1 || true)
  echo "Cesium for Unreal: ${CES:-NOT INSTALLED}  (need 2.30.0 or newer, from Fab)"
  test -n "$CES" || { echo "STOP: install Cesium for Unreal to engine 5.8 from Fab"; exit 1; }
}

build() {
  say "Development editor build"
  "$UE/Engine/Build/BatchFiles/Mac/Build.sh" OneMindEditor Mac Development -Project="$UPROJECT" -WaitMutex
}

package() {
  say "$CONFIG package for Mac -> $OUT/$CONFIG"
  "$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$UPROJECT" -platform=Mac \
    -clientconfig="$CONFIG" -build -cook -stage -pak -archive -archivedirectory="$OUT/$CONFIG" -utf8output
  ls -d "$OUT/$CONFIG"/Mac*/OneMind.app
}

run() {
  say "run $CONFIG (log: ~/Library/Logs/OneMind/OneMind.log; Shipping writes no log)"
  APP=$(ls -d "$OUT/$CONFIG"/Mac*/OneMind.app | head -1)
  open "$APP" --args ${ONEMIND_ARGS:-}
}

case "$STEP" in
  check) check ;;
  build) check; build ;;
  package) check; package ;;
  package-shipping) CONFIG=Shipping; check; package ;;
  run) run ;;
  all) check; build; package; run ;;
  *) echo "usage: [CONFIG=Development|Shipping] $0 [check|build|package|package-shipping|run|all]"; exit 2 ;;
esac
