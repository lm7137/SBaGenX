#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

log="$tmpdir/run.log"
if ! "$ROOT_DIR/dist/sbagenx-linux64" \
  -o "$tmpdir/out.mp3" \
  -L 0:00:01 \
  -i 200+4/20 > /dev/null 2> "$log"; then
  if grep -q "libmp3lame runtime library is not available" "$log"; then
    echo "SKIP: libmp3lame runtime not available"
    exit 0
  fi
  cat "$log" >&2
  exit 1
fi

grep -q "Outputting MP3 encoded audio at .* (float encoder path)" "$log"
file "$tmpdir/out.mp3" | grep -qi "mpeg"

echo "PASS: MP3 export uses float encoder path when supported"
