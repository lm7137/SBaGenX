#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

TMP_SBG="/tmp/sbx_safe_preamble_$$.sbg"
TMP_OUT="/tmp/sbx_safe_preamble_$$.out"
TMP_REAL="/tmp/sbx_safe_preamble_real_$$.out"
trap 'rm -f "$TMP_SBG" "$TMP_OUT" "$TMP_REAL"' EXIT

cat >"$TMP_SBG" <<'EOF'
-SE
-T 0:00
duo: 180+0/20 260+0/15
00:00:01 duo
EOF

SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$TMP_SBG" >"$TMP_OUT"

grep -q "^# sbagenxlib keyframes" "$TMP_OUT" || {
  echo "FAIL: safe preamble subset file did not route through sbagenxlib" >&2
  exit 1
}
grep -q "^# voice-lanes: 2$" "$TMP_OUT" || {
  echo "FAIL: safe preamble subset file lost multivoice content" >&2
  exit 1
}
grep -q "^1.000000 sine:180+0/20 linear$" "$TMP_OUT" || {
  echo "FAIL: safe preamble subset file primary dump mismatch" >&2
  exit 1
}

SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$ROOT_DIR/examples/basics/prog-chakras-1.sbg" >"$TMP_REAL"

grep -q "^# sbagenxlib keyframes" "$TMP_REAL" || {
  echo "FAIL: real example with safe preamble did not route through sbagenxlib" >&2
  exit 1
}

echo "PASS: seq backend safe preamble subset smoke test"
