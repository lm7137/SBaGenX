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

cat >"$tmpdir/custom-iso.sbg" <<'EOF'
custom00: 0 0 1 1
base: custom00:200@1/100
00:00 base
EOF

(
  cd "$tmpdir"
  SBAGENX_PLOT_BACKEND=internal SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -P custom-iso.sbg >/dev/null 2>err.txt
  ls iso_cycle_*.png >/dev/null 2>&1
  ! grep -q "requires at least one isochronic" err.txt
)

echo "PASS: -P sequence isochronic cycle plot works with customNN envelope"
