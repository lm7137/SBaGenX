#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

TMP_SBG="/tmp/sbx_mix_dump_$$.sbg"
TMP_OUT="/tmp/sbx_mix_dump_$$.out"
trap 'rm -f "$TMP_SBG" "$TMP_OUT"' EXIT

cat >"$TMP_SBG" <<'EOF'
pulse: mix/65 mixbeat:3/25 180+0/15
wash: mix/40 mixbeat:2/30 mixspin:400+4/20 200+0/12
00:00 pulse
00:00:10 wash step
EOF

SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$TMP_SBG" >"$TMP_OUT"

grep -q "^# mix-keyframes: 2$" "$TMP_OUT" || {
  echo "FAIL: missing mix keyframe header" >&2
  exit 1
}
grep -q "^# timed-mixfx-keyframes: 2 slots:2$" "$TMP_OUT" || {
  echo "FAIL: missing timed mix-effect header" >&2
  exit 1
}
grep -q "^#   mix\\[0\\] 0.000000 mix/65.000000 linear$" "$TMP_OUT" || {
  echo "FAIL: missing first mix keyframe dump line" >&2
  exit 1
}
grep -q "^#   mix\\[1\\] 10.000000 mix/40.000000 step$" "$TMP_OUT" || {
  echo "FAIL: missing second mix keyframe dump line" >&2
  exit 1
}
grep -q "^#   mixfx\\[0\\]\\[0\\] 0.000000 sine:mixbeat:3/25 linear$" "$TMP_OUT" || {
  echo "FAIL: missing first timed mix-effect dump line" >&2
  exit 1
}
grep -q "^#   mixfx\\[1\\]\\[1\\] 10.000000 sine:mixspin:400+4/20 step$" "$TMP_OUT" || {
  echo "FAIL: missing second timed mix-effect dump line" >&2
  exit 1
}

echo "PASS: seq backend mix dump smoke test"
