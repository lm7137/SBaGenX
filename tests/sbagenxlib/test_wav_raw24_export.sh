#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

"$ROOT_DIR/dist/sbagenx-linux64" -Q \
  -b 24 -W -o "$tmpdir/out.wav" \
  -L 0:00:01 \
  -i 200+4/20 >/dev/null 2>&1

file "$tmpdir/out.wav" | grep -q "24 bit"

"$ROOT_DIR/dist/sbagenx-linux64" -Q \
  -b 24 -o "$tmpdir/out.raw" \
  -L 0:00:01 \
  -i 200+4/20 >/dev/null 2>&1

raw_sz="$(wc -c < "$tmpdir/out.raw")"
if [ "$raw_sz" != "264600" ]; then
  echo "Expected 24-bit raw size 264600 bytes, got $raw_sz" >&2
  exit 1
fi

echo "PASS: WAV/raw export use 24-bit PCM path"
