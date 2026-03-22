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
cleanup() {
  rm -f "$seq_file" "$out_file"
}
trap cleanup EXIT

cat > "$seq_file" <<'SEQ'
wave00: 0 0.2 0.8 1 0.8 0.2 0

ts1: 200+8/10 white/10 triangle:300@4/10 square:bell280/10
ts2: wave00:210+4/10 spin:60+1.5/10 bspin:80-1/10 wspin:70+0.5/10 mix/10
ts3: mix/20 mixbeat:2/10 pink/10 brown/10

00:00:00 ts1
00:00:20 ts2
00:00:40 ts3
SEQ

"$BIN" -D "$seq_file" > "$out_file"

grep -q "triangle:300@4/10" "$out_file"
grep -q "square:bell280/10" "$out_file"
grep -q "wave00:210+4/10" "$out_file"
grep -q "sine:spin:60+1.5/10" "$out_file"
grep -q "sine:bspin:80-1/10" "$out_file"
grep -q "sine:wspin:70+0.5/10" "$out_file"
grep -q "sine:mixbeat:2/10" "$out_file"
grep -q "pink/10" "$out_file"
grep -q "brown/10" "$out_file"

echo "PASS: legacy sequence parser bridge smoke test"
