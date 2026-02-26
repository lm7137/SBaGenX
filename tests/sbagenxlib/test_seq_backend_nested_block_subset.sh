#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

TMP_SBG="/tmp/sbx_nested_block_subset_$$.sbg"
TMP_OUT="/tmp/sbx_nested_block_subset_$$.out"
trap 'rm -f "$TMP_SBG" "$TMP_OUT"' EXIT

cat >"$TMP_SBG" <<'EOF'
off: -
pulse: {
  +00:00 off ==
  +00:01 180+0/35 ->
}
double: {
  +00:00 pulse
  +00:02 pulse
}
NOW double
EOF

SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$TMP_SBG" >"$TMP_OUT"

grep -q "^# sbagenxlib keyframes" "$TMP_OUT" || {
  echo "FAIL: missing sbagenxlib keyframe header" >&2
  exit 1
}
grep -q "0.000000 - step" "$TMP_OUT" || {
  echo "FAIL: missing nested block first off keyframe" >&2
  exit 1
}
grep -q "60.000000 sine:180+0/35 linear" "$TMP_OUT" || {
  echo "FAIL: missing nested block first pulse keyframe" >&2
  exit 1
}
grep -q "120.000000 - step" "$TMP_OUT" || {
  echo "FAIL: missing nested block second off keyframe" >&2
  exit 1
}
grep -q "180.000000 sine:180+0/35 linear" "$TMP_OUT" || {
  echo "FAIL: missing nested block second pulse keyframe" >&2
  exit 1
}

echo "PASS: seq backend nested-block subset bridge smoke test"
