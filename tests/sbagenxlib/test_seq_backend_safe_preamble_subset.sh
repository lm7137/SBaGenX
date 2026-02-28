#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

TMP_SBG="/tmp/sbx_safe_preamble_$$.sbg"
TMP_SBG_INLINE="/tmp/sbx_safe_preamble_inline_$$.sbg"
TMP_SBG_RUNTIME="/tmp/sbx_safe_preamble_runtime_$$.sbg"
TMP_OUT="/tmp/sbx_safe_preamble_$$.out"
TMP_OUT_INLINE="/tmp/sbx_safe_preamble_inline_$$.out"
TMP_OUT_RUNTIME="/tmp/sbx_safe_preamble_runtime_$$.out"
TMP_REAL="/tmp/sbx_safe_preamble_real_$$.out"
TMP_REAL2="/tmp/sbx_safe_preamble_real2_$$.out"
TMP_REAL3="/tmp/sbx_safe_preamble_real3_$$.out"
TMP_REAL4="/tmp/sbx_safe_preamble_real4_$$.out"
trap 'rm -f "$TMP_SBG" "$TMP_SBG_INLINE" "$TMP_SBG_RUNTIME" "$TMP_OUT" "$TMP_OUT_INLINE" "$TMP_OUT_RUNTIME" "$TMP_REAL" "$TMP_REAL2" "$TMP_REAL3" "$TMP_REAL4"' EXIT

cat >"$TMP_SBG" <<'EOF'
-SE
-T 0:00
duo: 180+0/20 260+0/15
00:00:01 duo
EOF

cat >"$TMP_SBG_INLINE" <<'EOF'
-SE -m river1.ogg
off: -
base: 205+10/20 mix/100
00:00:00 off ->
00:00:05 base
EOF

cat >"$TMP_SBG_RUNTIME" <<'EOF'
-SE
-r 44100
-F 10000
-q 1
base: 180+0/20
00:00:01 base
EOF

SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$TMP_SBG" >"$TMP_OUT"
SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$TMP_SBG_INLINE" >"$TMP_OUT_INLINE"
SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$TMP_SBG_RUNTIME" >"$TMP_OUT_RUNTIME"

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

grep -q "^# sbagenxlib keyframes" "$TMP_OUT_INLINE" || {
  echo "FAIL: inline safe preamble subset file did not route through sbagenxlib" >&2
  exit 1
}
grep -q "^# mix-keyframes: 2$" "$TMP_OUT_INLINE" || {
  echo "FAIL: inline safe preamble subset file lost mix keyframes" >&2
  exit 1
}
grep -q "^5.000000 sine:205+10/20 linear$" "$TMP_OUT_INLINE" || {
  echo "FAIL: inline safe preamble subset file primary dump mismatch" >&2
  exit 1
}
grep -q "^# sbagenxlib keyframes" "$TMP_OUT_RUNTIME" || {
  echo "FAIL: runtime-option safe preamble subset file did not route through sbagenxlib" >&2
  exit 1
}
grep -q "^1.000000 sine:180+0/20 linear$" "$TMP_OUT_RUNTIME" || {
  echo "FAIL: runtime-option safe preamble subset file primary dump mismatch" >&2
  exit 1
}

SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$ROOT_DIR/examples/basics/prog-chakras-1.sbg" >"$TMP_REAL"
SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$ROOT_DIR/examples/contrib/ch-awakened-mind.sbg" >"$TMP_REAL2"
SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$ROOT_DIR/examples/basics/prog-drop-old-demo.sbg" >"$TMP_REAL3"
SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$ROOT_DIR/examples/contrib/xota-powermed1-pink.sbg" >"$TMP_REAL4"

grep -q "^# sbagenxlib keyframes" "$TMP_REAL" || {
  echo "FAIL: real example with safe preamble did not route through sbagenxlib" >&2
  exit 1
}
grep -q "^# voice-lanes: 5$" "$TMP_REAL2" || {
  echo "FAIL: real multivoice safe-preamble example did not preserve voice lanes" >&2
  exit 1
}
grep -q "^# mix-keyframes: 14$" "$TMP_REAL3" || {
  echo "FAIL: real inline-preamble example did not preserve mix keyframes" >&2
  exit 1
}
grep -q "^# voice-lanes: 2$" "$TMP_REAL4" || {
  echo "FAIL: real extended-preamble example did not preserve voice lanes" >&2
  exit 1
}

echo "PASS: seq backend safe preamble subset smoke test"
