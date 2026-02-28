#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

TMP_SBG="/tmp/sbx_mixfx_requires_mix_$$.sbg"
TMP_OUT="/tmp/sbx_mixfx_requires_mix_$$.out"
trap 'rm -f "$TMP_SBG" "$TMP_OUT"' EXIT

cat >"$TMP_SBG" <<'EOF'
wash: mix/70 mixbeat:3/25 180+0/15
00:00 wash
EOF

set +e
SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" "$TMP_SBG" >"$TMP_OUT" 2>&1
RC=$?
set -e

if [[ $RC -eq 0 ]]; then
  echo "FAIL: expected native sbg loader path to reject mix effects without active mix input" >&2
  exit 1
fi

grep -q "sequence file mix effects require a mix input stream" "$TMP_OUT" || {
  echo "FAIL: missing mix-input error message from native sbg loader path" >&2
  exit 1
}

echo "PASS: seq backend mixfx requires mix smoke test"
