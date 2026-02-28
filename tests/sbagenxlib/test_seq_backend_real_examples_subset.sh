#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

run_one() {
  local file="$1"
  local pattern="$2"
  local out
  out="/tmp/sbx_real_example_$$.out"
  SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$ROOT_DIR/$file" >"$out"
  grep -q "^# sbagenxlib keyframes" "$out" || {
    echo "FAIL: $file did not route through sbagenxlib" >&2
    rm -f "$out"
    exit 1
  }
  grep -q "$pattern" "$out" || {
    echo "FAIL: $file missing expected dump content" >&2
    rm -f "$out"
    exit 1
  }
  rm -f "$out"
}

run_one "examples/basics/prog-chakras-1.sbg" "^# voice-lanes: 5$"
run_one "examples/basics/prog-wave-custom-envelope-demo.sbg" "^0.000000 wave00:200+4/50 linear$"
run_one "examples/contrib/jim/prog-test-wave.sbg" "^0.000000 wave00:300+5/50 linear$"
run_one "examples/focus/wave-01.sbg" "^# voice-lanes: 3$"

echo "PASS: seq backend real examples subset smoke test"
