#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "Missing binary: $BIN" >&2
  echo "Run: bash linux-build-sbagenx.sh" >&2
  exit 1
fi

seq_file="$(mktemp)"
out_file="$(mktemp)"
render_out="$(mktemp --suffix=.flac)"
cleanup() {
  rm -f "$seq_file" "$out_file" "$render_out"
}
trap cleanup EXIT

cat > "$seq_file" <<'SEQ'
-SE
-Z 12
-o __OUT__

custom00: e=2 0 0.1 0.9 1 1 0.9 0.1 0

base: custom00:400@2/20
NOW base
SEQ
perl -0pi -e 's#__OUT__#'"$render_out"'#g' "$seq_file"

"$BIN" -D "$seq_file" > "$out_file"

grep -q "custom00:400@2/20 linear" "$out_file"

echo "PASS: safe sequence preamble supports customNN with -o/-Z"
