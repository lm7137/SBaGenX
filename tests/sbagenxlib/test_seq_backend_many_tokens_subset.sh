#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

TMP_SBG="/tmp/sbx_many_tokens_subset_$$.sbg"
TMP_OUT="/tmp/sbx_many_tokens_subset_$$.out"
trap 'rm -f "$TMP_SBG" "$TMP_OUT"' EXIT

cat >"$TMP_SBG" <<'EOF'
00:00 120+0/12 140+0/12 160+0/12 180+0/12 200+0/12 220+0/12 240+0/12
EOF

SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$TMP_SBG" >"$TMP_OUT"

grep -q "^# sbagenxlib keyframes" "$TMP_OUT" || {
  echo "FAIL: missing sbagenxlib keyframe header" >&2
  exit 1
}
grep -q "0.000000 sine:120+0/12 linear" "$TMP_OUT" || {
  echo "FAIL: missing primary tone dump from many-token sbg line" >&2
  exit 1
}

echo "PASS: seq backend many-token subset bridge smoke test"
