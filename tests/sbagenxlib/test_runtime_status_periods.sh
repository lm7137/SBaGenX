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

python3 - <<'PY' "$tmpdir/mix.wav"
import math
import struct
import sys
import wave

path = sys.argv[1]
sr = 8000
dur = 4
n = sr * dur
with wave.open(path, "wb") as wf:
    wf.setnchannels(2)
    wf.setsampwidth(2)
    wf.setframerate(sr)
    frames = bytearray()
    for i in range(n):
        s = int(2000.0 * math.sin(2.0 * math.pi * 220.0 * (i / sr)))
        frames += struct.pack("<hh", s, s)
    wf.writeframes(frames)
PY

cat >"$tmpdir/periods.sbg" <<'EOF'
v0: 180+0/20 mix/70
v1: 260+0/20 mix/40
00:00 v0
00:00:02 v1
EOF

"$BIN" -S -L 0:00:04 -m "$tmpdir/mix.wav" -o "$tmpdir/out.wav" -W "$tmpdir/periods.sbg" >/dev/null 2>"$tmpdir/err.raw"
tr '\r' '\n' <"$tmpdir/err.raw" >"$tmpdir/err.txt"

grep -Fq "* 00:00:00" "$tmpdir/err.txt" || {
  echo "FAIL: missing initial keyframe period header" >&2
  exit 1
}

grep -Fq "  00:00:02" "$tmpdir/err.txt" || {
  echo "FAIL: missing next-period keyframe line" >&2
  exit 1
}

grep -Fq "* 00:00:00 sine:180+0/20 mix/70.00" "$tmpdir/err.txt" || {
  echo "FAIL: initial keyframe period header is missing mix state" >&2
  exit 1
}

grep -Fq "  00:00:02 sine:260+0/20 mix/40.00" "$tmpdir/err.txt" || {
  echo "FAIL: next-period keyframe line is missing mix state" >&2
  exit 1
}

echo "PASS: sbagenxlib keyframed runtime emits period history with mix state"
