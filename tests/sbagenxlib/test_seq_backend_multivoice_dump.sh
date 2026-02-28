#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

TMP_SBG="/tmp/sbx_multivoice_dump_$$.sbg"
TMP_OUT="/tmp/sbx_multivoice_dump_$$.out"
trap 'rm -f "$TMP_SBG" "$TMP_OUT"' EXIT

cat >"$TMP_SBG" <<'EOF'
duo: 180+0/20 260+0/15
00:00 duo
EOF

SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$TMP_SBG" >"$TMP_OUT"

grep -q "^# sbagenxlib keyframes" "$TMP_OUT" || {
  echo "FAIL: missing sbagenxlib keyframe header" >&2
  exit 1
}
grep -q "^# voice-lanes: 2$" "$TMP_OUT" || {
  echo "FAIL: missing multivoice lane count in dump output" >&2
  exit 1
}
grep -q "^0.000000 sine:180+0/20 linear$" "$TMP_OUT" || {
  echo "FAIL: missing primary voice dump line" >&2
  exit 1
}
grep -q "^#   lane\\[1\\] 0.000000 sine:260+0/15 linear$" "$TMP_OUT" || {
  echo "FAIL: missing secondary voice dump line" >&2
  exit 1
}

echo "PASS: seq backend multivoice dump smoke test"
