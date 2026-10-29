#!/bin/bash
# Builds the OneMind core on the host (CMake, Unreal-like flags) and runs every test:
# unit tests, integration tests against disposable nats-servers (WebSocket), the
# 10,000-entity load, the C1/C2 end-to-end run with the omos mesh gateway and entity
# service built from their PR branches, and the M4 runs: C3/C8/B11 (gateway, enrollment stub,
# mTLS TAK-stream peer) and D3 (the omos policy service built from its PR branch).
# Usage: run-host-tests.sh [out-dir]   (default /Users/danielcarku/Desktop/work/.run/tmp-b-client)
set -uo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-/Users/danielcarku/Desktop/work/.run/tmp-b-client}
TOOLS=/Users/danielcarku/Desktop/work/.tools
OMOS=${OMOS:-/Users/danielcarku/Desktop/work/omos}
export PATH="$TOOLS/bin:$PATH"
mkdir -p "$OUT"
rc=0
for STD in 17 20; do
  B="$OUT/build-cxx$STD"
  echo "== configure + build (C++$STD, -Wall -Wextra -Wshadow -Werror -fno-exceptions -fno-rtti)"
  cmake -S "$HERE" -B "$B" -DCMAKE_CXX_STANDARD=$STD > "$B.configure.log" 2>&1 || { cat "$B.configure.log"; exit 1; }
  cmake --build "$B" -j8 2>&1 | grep -E 'error|warning' ; test ${PIPESTATUS[0]} -eq 0 || { echo "build failed"; exit 1; }
done
B="$OUT/build-cxx17"
echo "== unit tests"; "$B/om_unit_tests" || rc=1
echo "== integration tests (disposable nats-server, WebSocket)"; "$B/om_integration_tests" || rc=1
echo "== AddressSanitizer + UBSan build, unit and integration tests"
cmake -S "$HERE" -B "$OUT/build-asan" -DOM_SANITIZE=ON > "$OUT/build-asan.configure.log" 2>&1
cmake --build "$OUT/build-asan" -j8 2>&1 | grep -E 'error' ; 
"$OUT/build-asan/om_unit_tests" | tail -1 || rc=1
"$OUT/build-asan/om_integration_tests" | tail -1 || rc=1
echo "== 10,000-entity load (client core)"; "$HERE/scripts/load_10k.sh" "$B/onemind_headless" "$OUT/load10k" | grep -E "final layer|state=live" | tail -2 || rc=1
echo "== 10,000-entity load with the camera AOI of the default start (Austin, 8 km): a small direct layer keeps all"
"$HERE/scripts/load_10k.sh" "$B/onemind_headless" "$OUT/load10k-camera" --camera-aoi -97.7931,30.2240,-97.6931,30.3104 | grep -E "final layer|state=live" | tail -2 || rc=1
echo "== C1/C2 end to end"; "$TOOLS/py/bin/python" -I "$HERE/scripts/e2e_c1_c2.py" "$B/onemind_headless" "$OMOS" "$OUT/e2e" | grep -E 'PASS|FAIL|checks passed' || rc=1
echo "== C3/C8/B11 end to end (mesh gateway directed TCP, enrollment stub, mTLS TAK-stream peer)"
"$TOOLS/py/bin/python" -I "$HERE/scripts/e2e_c3_tak.py" "$B/onemind_tak" "$OMOS" "$OUT/e2e-c3" | grep -E 'PASS|FAIL|checks passed'; test ${PIPESTATUS[0]} -eq 0 || rc=1
echo "== D3 end to end (real policy service from omos feat/policy-service)"
"$TOOLS/py/bin/python" -I "$HERE/scripts/e2e_d3_policy.py" "$B/onemind_tak" "$OMOS" "$OUT/e2e-d3" | grep -E 'PASS|FAIL|checks passed'; test ${PIPESTATUS[0]} -eq 0 || rc=1
exit $rc
