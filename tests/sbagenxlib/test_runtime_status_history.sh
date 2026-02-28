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

"$BIN" -S -L 0:01:05 -o "$tmpdir/status.wav" -W -i 200+8/30 >/dev/null 2>"$tmpdir/err.txt"

grep -q "^SBAGENXLIB runtime active$" "$tmpdir/err.txt" || {
  echo "FAIL: missing sbagenxlib runtime banner" >&2
  exit 1
}

grep -q "^  00:01:" "$tmpdir/err.txt" || {
  echo "FAIL: missing newline status history snapshot after one runtime minute" >&2
  exit 1
}

echo "PASS: sbagenxlib runtime emits periodic newline status history"
