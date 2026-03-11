#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${SBAGENX_TEST_BIN:-$ROOT_DIR/dist/sbagenx-linux64}"
MIX_FILE="$ROOT_DIR/assets/river1.ogg"

if [[ ! -x "$BIN" ]]; then
  echo "Missing binary: $BIN" >&2
  echo "Set SBAGENX_TEST_BIN or run: bash linux-build-sbagenx.sh" >&2
  exit 1
fi

if [[ ! -f "$MIX_FILE" ]]; then
  echo "Missing mix file: $MIX_FILE" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

timeout 8 "$BIN" -m "$MIX_FILE" -o "$tmpdir/out.ogg" \
  -p sigmoid t1,1,0 00ls+/3 mix/97 >"$tmpdir/out.txt" 2>"$tmpdir/err.raw" || true

tr '\r' '\n' <"$tmpdir/err.raw" >"$tmpdir/err.txt"

grep -q "sine:205+10/3" "$tmpdir/err.txt" || {
  echo "FAIL: built-in curve runtime did not preserve /3 program amplitude in status output" >&2
  exit 1
}

grep -q "mix/97.00" "$tmpdir/err.txt" || {
  echo "FAIL: built-in curve runtime did not preserve mix/97 base amplitude in status output" >&2
  exit 1
}

echo "PASS: built-in curve runtime status preserves program and mix amplitudes"
