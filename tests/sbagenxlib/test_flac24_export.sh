#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

if ! command -v metaflac >/dev/null 2>&1; then
  echo "SKIP: metaflac not found"
  exit 0
fi

"$ROOT_DIR/dist/sbagenx-linux64" -Q \
  -o "$tmpdir/out.flac" \
  -L 0:00:01 \
  -i 200+4/20 >/dev/null 2>&1

bps="$(metaflac --show-bps "$tmpdir/out.flac")"
if [ "$bps" != "24" ]; then
  echo "Expected 24-bit FLAC export, got $bps-bit" >&2
  exit 1
fi

echo "PASS: FLAC export uses 24-bit PCM path"
