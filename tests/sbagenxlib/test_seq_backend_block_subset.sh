#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

TMP_SBG="/tmp/sbx_block_subset_$$.sbg"
TMP_OUT="/tmp/sbx_block_subset_$$.out"
trap 'rm -f "$TMP_SBG" "$TMP_OUT"' EXIT

cat >"$TMP_SBG" <<'EOF'
off: -
base: 180+0/35
burst: {
  +00:00 off ==
  +00:01 base ->
  +00:02 off
}
NOW burst
+00:03:00 burst
EOF

SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$TMP_SBG" >"$TMP_OUT"

grep -q "^# sbagenxlib keyframes" "$TMP_OUT" || {
  echo "FAIL: missing sbagenxlib keyframe header" >&2
  exit 1
}
grep -q "0.000000 - step" "$TMP_OUT" || {
  echo "FAIL: missing first off keyframe in block expansion" >&2
  exit 1
}
grep -q "180.000000 - step" "$TMP_OUT" || {
  echo "FAIL: missing repeated off keyframe from second block invocation" >&2
  exit 1
}
grep -q "240.000000 sine:180+0/35 linear" "$TMP_OUT" || {
  echo "FAIL: missing repeated base keyframe from second block invocation" >&2
  exit 1
}

echo "PASS: seq backend block subset bridge smoke test"
