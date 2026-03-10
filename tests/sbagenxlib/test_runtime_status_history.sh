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

if grep -q "^SBAGENXLIB runtime active$" "$tmpdir/err.txt"; then
  echo "FAIL: unexpected sbagenxlib runtime banner" >&2
  exit 1
fi

newline_count="$(tr -cd '\n' <"$tmpdir/err.txt" | wc -c)"
if [[ "$newline_count" -ne 2 ]]; then
  echo "FAIL: immediate sbagenxlib status should only contain the legacy setup newlines; got $newline_count newline(s)" >&2
  exit 1
fi

echo "PASS: sbagenxlib immediate runtime status matches legacy single-line cadence"
