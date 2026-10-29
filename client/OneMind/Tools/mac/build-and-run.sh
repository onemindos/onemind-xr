#!/bin/bash
# OneMind on the owner's Apple Silicon Mac: host checks, Development editor build,
# Shipping package, run. One command for OWNER-STEPS.md S1/F1 (and the base for S5,
# the 10,000-entity check, C1 and C2).
#
#   UE="/Users/Shared/Epic Games/UE_5.8" ./build-and-run.sh [check|build|package|run|all]
#
# Nothing here needs a secret. NATS credentials, if the leaf needs them, are read by the app
# from ONEMIND_NATS_USER / ONEMIND_NATS_PASSWORD in the environment at run time.
set -euo pipefail

HERE=$(cd "$(dirname "$0")/../.." && pwd)          # client/OneMind
UPROJECT="$HERE/OneMind.uproject"
UE=${UE:-"/Users/Shared/Epic Games/UE_5.8"}
OUT=${OUT:-"$HOME/onemind-build/mac"}
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
  say "Shipping package for Mac -> $OUT"
  "$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$UPROJECT" -platform=Mac \
    -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="$OUT" -utf8output
  ls -d "$OUT"/Mac*/OneMind.app
}

run() {
  say "run (log: ~/Library/Logs/OneMind/OneMind.log)"
  APP=$(ls -d "$OUT"/Mac*/OneMind.app | head -1)
  open "$APP" --args ${ONEMIND_ARGS:-}
}

case "$STEP" in
  check) check ;;
  build) check; build ;;
  package) check; package ;;
  run) run ;;
  all) check; build; package; run ;;
  *) echo "usage: $0 [check|build|package|run|all]"; exit 2 ;;
esac
