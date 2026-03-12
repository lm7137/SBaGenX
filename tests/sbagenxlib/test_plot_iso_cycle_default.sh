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

(
  cd "$tmpdir"
  SBAGENX_PLOT_BACKEND=internal "$BIN" -P -i 200@1/100 >/dev/null 2>err.txt
  ls iso_cycle_*.png >/dev/null 2>&1
  ! grep -q "requires at least one isochronic" err.txt
)

echo "PASS: -P immediate isochronic cycle plot works with default envelope"
