#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if ! command -v python3 >/dev/null 2>&1; then
  echo "SKIP: python3 unavailable"
  exit 0
fi

if ! python3 - <<'PY' >/dev/null 2>&1
import cairo
PY
then
  echo "SKIP: python3 cairo module unavailable"
  exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cat >"$tmpdir/custom-iso.sbg" <<'EOF'
custom00: e=2 0 0.1 0.9 1 1 0.9 0.1 0
base: custom00:200@1/100
00:00 base
EOF

(
  cd "$tmpdir"
  SBAGENX_PLOT_BACKEND=python SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -P custom-iso.sbg >/dev/null 2>err.txt
)

if ! ls "$tmpdir"/*.png >/dev/null 2>&1; then
  echo "FAIL: expected Python/Cairo customNN isochronic plot PNG" >&2
  cat "$tmpdir/err.txt" >&2 || true
  exit 1
fi

if grep -q "External Python/Cairo plot backend requested but unavailable or failed" "$tmpdir/err.txt"; then
  echo "FAIL: Python/Cairo backend fallback triggered unexpectedly" >&2
  cat "$tmpdir/err.txt" >&2 || true
  exit 1
fi

echo "PASS: -P sequence isochronic cycle plot works with customNN envelope via Python/Cairo"
