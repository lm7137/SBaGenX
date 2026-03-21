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
  local pattern1="$2"
  local pattern2="$3"
  local out
  out="/tmp/sbx_wrapper_$$.out"
  SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$ROOT_DIR/$file" >"$out"
  grep -q "^# sbagenxlib keyframes" "$out" || {
    echo "FAIL: $file did not emit sbagenxlib keyframes" >&2
    rm -f "$out"
    exit 1
  }
  grep -q "$pattern1" "$out" || {
    echo "FAIL: $file missing expected preprogram dump content" >&2
    rm -f "$out"
    exit 1
  }
  grep -q "$pattern2" "$out" || {
    echo "FAIL: $file missing expected extras dump content" >&2
    rm -f "$out"
    exit 1
  }
  rm -f "$out"
}

run_one "examples/basics/prog-drop-00d.sbg" "^0.000000 205+10/1 mix/100 linear$" "^# extras: mix/100$"
run_one "examples/basics/prog-slide-alpha-10.sbg" "^0.000000 200+10/1 mix/100 linear$" "^# extras: mix/100$"

echo "PASS: seq backend option-wrapper example smoke test"
