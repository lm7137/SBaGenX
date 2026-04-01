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
trap 'rm -rf "$tmpdir"' EXIT
ran=0

python3 - <<'PY' "$tmpdir/mix_source.wav"
import math
import struct
import sys
import wave

path = sys.argv[1]
sr = 48000
frames = sr
pcm = bytearray()
for i in range(frames):
    sample = int(14000 * math.sin(2.0 * math.pi * 220.0 * i / sr))
    pcm += struct.pack("<hh", sample, sample)
with wave.open(path, "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(sr)
    w.writeframes(pcm)
PY

check_duration() {
  python3 - <<'PY' "$1" "$2" "$3"
import sys
import wave

path, expect_full, label = sys.argv[1], sys.argv[2] == "1", sys.argv[3]
with wave.open(path, "rb") as wf:
    frames = wf.getnframes()
    rate = wf.getframerate()
    duration = frames / rate
    if expect_full:
        if abs(duration - 3.0) > 0.05:
            raise SystemExit(
                f"FAIL: {label}: expected about 3.0s of looped output, got {duration:.3f}s"
            )
    else:
        if duration >= 2.0:
            raise SystemExit(
                f"FAIL: {label}: expected non-looping mix to stop early, got {duration:.3f}s"
            )
PY
}

run_one() {
  local format="$1"
  local source="$2"
  local no_loop="$tmpdir/${format}-no-loop.wav"
  local with_loop="$tmpdir/${format}-with-loop.wav"
  local seq="$tmpdir/${format}.sbg"

  cat >"$seq" <<'EOF'
base: mix/100 200+0/0
00:00 base step
00:00:03 -
EOF

  "$BIN" -Q -W -m "$source" -o "$no_loop" -E "$seq" >/dev/null 2>"$tmpdir/${format}.err"
  check_duration "$no_loop" 0 "$format without --looper"

  "$BIN" -Q -W -m "$source" --looper "s1 f0" -o "$with_loop" -E "$seq" \
    >/dev/null 2>"$tmpdir/${format}.err"
  check_duration "$with_loop" 1 "$format with --looper"
  ran=1
}

run_embedded() {
  local format="$1"
  local source="$2"
  local out="$tmpdir/${format}-embedded.wav"
  local seq="$tmpdir/${format}-embedded.sbg"

  cat >"$seq" <<'EOF'
base: mix/100 200+0/0
00:00 base step
00:00:03 -
EOF

  "$BIN" -Q -W -m "$source" -o "$out" -E "$seq" >/dev/null 2>"$tmpdir/${format}-embedded.err"
  check_duration "$out" 1 "$format with embedded SBAGEN_LOOPER"
  ran=1
}

if command -v ffmpeg >/dev/null 2>&1; then
  ffmpeg -y -hide_banner -loglevel error -i "$tmpdir/mix_source.wav" \
    -c:a libvorbis -fflags +bitexact -flags:a +bitexact \
    "$tmpdir/mix.ogg" >/dev/null 2>&1
  run_one "ogg" "$tmpdir/mix.ogg"

  ffmpeg -y -hide_banner -loglevel error -i "$tmpdir/mix_source.wav" \
    -c:a libmp3lame "$tmpdir/mix.mp3" >/dev/null 2>&1
  run_one "mp3" "$tmpdir/mix.mp3"

  ffmpeg -y -hide_banner -loglevel error -i "$tmpdir/mix_source.wav" \
    -c:a libmp3lame -id3v2_version 3 -metadata "TXXX:SBAGEN_LOOPER=s1 f0" \
    "$tmpdir/mix-tagged.mp3" >/dev/null 2>&1
  run_embedded "mp3" "$tmpdir/mix-tagged.mp3"
fi

if command -v flac >/dev/null 2>&1; then
  flac -f "$tmpdir/mix_source.wav" -o "$tmpdir/mix.flac" >/dev/null 2>&1
  run_one "flac" "$tmpdir/mix.flac"
fi

if [[ "$ran" -eq 0 ]]; then
  echo "SKIP: neither ffmpeg nor flac is available for looper override generation"
  exit 0
fi

echo "PASS: explicit --looper override drives OGG/MP3/FLAC mix looping"
