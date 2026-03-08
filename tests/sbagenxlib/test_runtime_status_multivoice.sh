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

cat >"$tmpdir/multivoice.sbg" <<'EOF'
duo: 180+0/20 260+0/15
00:00 duo
EOF

"$BIN" -S -L 0:00:02 -o "$tmpdir/out.wav" -W "$tmpdir/multivoice.sbg" >/dev/null 2>"$tmpdir/err.raw"
tr '\r' '\n' <"$tmpdir/err.raw" >"$tmpdir/err.txt"

grep -Fq "180+0/20" "$tmpdir/err.txt" || {
  echo "FAIL: missing primary multivoice lane in runtime status" >&2
  exit 1
}

grep -Fq "260+0/15" "$tmpdir/err.txt" || {
  echo "FAIL: missing secondary multivoice lane in runtime status" >&2
  exit 1
}

echo "PASS: sbagenxlib runtime status shows all active multivoice lanes"
