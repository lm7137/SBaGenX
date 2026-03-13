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
if ! command -v flac >/dev/null 2>&1; then
  echo "SKIP: flac decoder not found"
  exit 0
fi

"$ROOT_DIR/dist/sbagenx-linux64" -Q \
  -o "$tmpdir/out.flac" \
  -L 0:00:01 \
  -i 200+4/20 >/dev/null 2>&1

"$ROOT_DIR/dist/sbagenx-linux64" -Q \
  -W -b 16 -o "$tmpdir/ref.wav" \
  -L 0:00:01 \
  -i 200+4/20 >/dev/null 2>&1

bps="$(metaflac --show-bps "$tmpdir/out.flac")"
if [ "$bps" != "24" ]; then
  echo "Expected 24-bit FLAC export, got $bps-bit" >&2
  exit 1
fi

flac -d -f "$tmpdir/out.flac" -o "$tmpdir/out-dec.wav" >/dev/null 2>&1

python3 - "$tmpdir/ref.wav" "$tmpdir/out-dec.wav" <<'PY'
import math
import sys
import wave

def peak_ratio(path):
    with wave.open(path, "rb") as w:
        sw = w.getsampwidth()
        frames = w.readframes(w.getnframes())
    peak = 0
    if sw == 2:
        for i in range(0, len(frames), 2):
            v = int.from_bytes(frames[i:i+2], "little", signed=True)
            if abs(v) > peak:
                peak = abs(v)
        full = 32767.0
    elif sw == 3:
        for i in range(0, len(frames), 3):
            b = frames[i:i+3]
            ext = b'\xff' if (b[2] & 0x80) else b'\x00'
            v = int.from_bytes(b + ext, "little", signed=True)
            if abs(v) > peak:
                peak = abs(v)
        full = 8388607.0
    else:
        raise SystemExit(f"unsupported sample width {sw} for {path}")
    return peak / full if peak else 0.0

ref = peak_ratio(sys.argv[1])
dec = peak_ratio(sys.argv[2])
if ref <= 0.0 or dec <= 0.0:
    raise SystemExit("peak detection failed")
db = 20.0 * math.log10(dec / ref)
if abs(db) > 0.2:
    raise SystemExit(f"decoded FLAC level drift too large: {db:.3f} dB")
PY

echo "PASS: FLAC export uses 24-bit PCM path at the correct level"
