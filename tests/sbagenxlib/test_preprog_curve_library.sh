#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${SBAGENX_TEST_BIN:-$ROOT_DIR/dist/sbagenx-linux64}"

if [[ ! -x "$BIN" ]]; then
  echo "Missing binary: $BIN" >&2
  echo "Set SBAGENX_TEST_BIN or run: bash linux-build-sbagenx.sh" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

"$BIN" -D -p curve examples/basics/curve-sigmoid-like.sbgf 00ls:l=0.2:h=0 >"$tmpdir/sigmoid_like.txt" 2>&1
"$BIN" -D -p curve examples/basics/curve-expfit-solve-demo.sbgf 00ls:l=-0.15 >"$tmpdir/solve.txt" 2>&1

grep -q "^CURVE summary:$" "$tmpdir/sigmoid_like.txt" || {
  echo "FAIL: missing curve summary for sigmoid-like .sbgf" >&2
  exit 1
}
grep -q "^ Parameters: l=0.2 h=0$" "$tmpdir/sigmoid_like.txt" || {
  echo "FAIL: curve parameter overrides were not reflected in CLI summary" >&2
  exit 1
}
grep -q "^ Using function-driven curve from .sbgf for sliding mode$" "$tmpdir/sigmoid_like.txt" || {
  echo "FAIL: curve slide mode no longer reports exact function-driven runtime" >&2
  exit 1
}
grep -q "^1800.000000 sine:200+0.323984/1 linear$" "$tmpdir/sigmoid_like.txt" || {
  echo "FAIL: sigmoid-like .sbgf end keyframe mismatch" >&2
  exit 1
}

grep -q "^ Parameters: l=-0.15 A=" "$tmpdir/solve.txt" || {
  echo "FAIL: solved constants were not surfaced in CLI summary" >&2
  exit 1
}
grep -q "^0.000000 sine:205+10/1 linear$" "$tmpdir/solve.txt" || {
  echo "FAIL: solve-backed .sbgf start keyframe mismatch" >&2
  exit 1
}
grep -q "^1800.000000 sine:200+0.3/1 linear$" "$tmpdir/solve.txt" || {
  echo "FAIL: solve-backed .sbgf end keyframe mismatch" >&2
  exit 1
}

echo "PASS: -p curve uses sbagenxlib-backed .sbgf parsing and evaluation"
