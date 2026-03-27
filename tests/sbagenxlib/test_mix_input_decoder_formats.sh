#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
ran=0

check_wav() {
  python3 - <<'PY' "$1"
import sys, wave
path = sys.argv[1]
with wave.open(path, "rb") as w:
    if w.getframerate() != 48000:
        raise SystemExit(f"unexpected sample rate {w.getframerate()}")
    if w.getnframes() <= 0:
        raise SystemExit("empty wav output")
PY
}

python3 - <<'PY' "$tmpdir/mix_source.wav"
import math, struct, sys, wave
path = sys.argv[1]
sr = 48000
frames = sr
pcm = bytearray()
for i in range(frames):
    sample = int(12000 * math.sin(2.0 * math.pi * 220.0 * i / sr))
    pcm += struct.pack("<hh", sample, sample)
with wave.open(path, "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(sr)
    w.writeframes(pcm)
PY

if command -v flac >/dev/null 2>&1; then
  flac -f "$tmpdir/mix_source.wav" -o "$tmpdir/mix.flac" >/dev/null 2>&1
  "$ROOT_DIR/dist/sbagenx-linux64" -Q -W -m "$tmpdir/mix.flac" \
    -o "$tmpdir/from_flac.wav" -L 0:00:01 -i 210+5/20 mix/10 \
    >/dev/null 2>"$tmpdir/render.log"
  check_wav "$tmpdir/from_flac.wav"
  ran=1
fi

if command -v ffmpeg >/dev/null 2>&1; then
  ffmpeg -y -hide_banner -loglevel error -i "$tmpdir/mix_source.wav" \
    -c:a libvorbis -fflags +bitexact -flags:a +bitexact \
    "$tmpdir/mix.ogg" >/dev/null 2>&1
  "$ROOT_DIR/dist/sbagenx-linux64" -Q -W -m "$tmpdir/mix.ogg" \
    -o "$tmpdir/from_ogg.wav" -L 0:00:01 -i 210+5/20 mix/10 \
    >/dev/null 2>"$tmpdir/render.log"
  check_wav "$tmpdir/from_ogg.wav"

  ffmpeg -y -hide_banner -loglevel error -i "$tmpdir/mix_source.wav" \
    "$tmpdir/mix.mp3" >/dev/null 2>&1
  "$ROOT_DIR/dist/sbagenx-linux64" -Q -W -m "$tmpdir/mix.mp3" \
    -o "$tmpdir/from_mp3.wav" -L 0:00:01 -i 210+5/20 mix/10 \
    >/dev/null 2>"$tmpdir/render.log"
  check_wav "$tmpdir/from_mp3.wav"
  ran=1
fi

if [ "$ran" -eq 0 ]; then
  echo "SKIP: neither flac nor ffmpeg is available for decoder smoke generation"
  exit 0
fi

echo "PASS: library-owned mix-input decoders inherit sample rate for available encoded formats"
