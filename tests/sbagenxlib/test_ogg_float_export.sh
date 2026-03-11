#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

log="$tmpdir/run.log"
"$ROOT_DIR/dist/sbagenx-linux64" \
  -o "$tmpdir/out.ogg" \
  -L 0:00:01 \
  -i 200+4/20 > /dev/null 2> "$log"

grep -q "Outputting OGG/Vorbis encoded audio at .* (float encoder path)" "$log"
file "$tmpdir/out.ogg" | grep -q "Ogg data"

echo "PASS: OGG export uses float encoder path"
