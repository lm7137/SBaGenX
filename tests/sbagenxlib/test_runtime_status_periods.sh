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

cat >"$tmpdir/periods.sbg" <<'EOF'
v0: 180+0/20
v1: 260+0/20
00:00 v0
00:00:02 v1
EOF

"$BIN" -S -L 0:00:04 -o "$tmpdir/out.wav" -W "$tmpdir/periods.sbg" >/dev/null 2>"$tmpdir/err.raw"
tr '\r' '\n' <"$tmpdir/err.raw" >"$tmpdir/err.txt"

grep -Fq "* 00:00:00" "$tmpdir/err.txt" || {
  echo "FAIL: missing initial keyframe period header" >&2
  exit 1
}

grep -Fq "  00:00:02" "$tmpdir/err.txt" || {
  echo "FAIL: missing next-period keyframe line" >&2
  exit 1
}

grep -Fq "* 00:00:02" "$tmpdir/err.txt" || {
  echo "FAIL: missing runtime period transition header" >&2
  exit 1
}

echo "PASS: sbagenxlib keyframed runtime emits legacy-style period history"
