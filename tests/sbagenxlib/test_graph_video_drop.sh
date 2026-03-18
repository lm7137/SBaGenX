#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if ! command -v python3 >/dev/null 2>&1; then
  echo "SKIP: python3 unavailable"
  exit 0
fi

if ! python3 - <<'PY' >/dev/null 2>&1
import cairo
PY
then
  echo "SKIP: python3 cairo module unavailable"
  exit 0
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "SKIP: ffmpeg unavailable"
  exit 0
fi

encoders="$(ffmpeg -hide_banner -encoders 2>/dev/null || true)"
if ! printf '%s\n' "$encoders" | grep -q 'libx264'; then
  echo "SKIP: ffmpeg without libx264 support"
  exit 0
fi

if ! command -v ffprobe >/dev/null 2>&1; then
  echo "SKIP: ffprobe unavailable"
  exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

(
  cd "$tmpdir"
  SBAGENX_PLOT_BACKEND=python \
    "$BIN" -G --graph-video graph.mp4 --graph-video-fps 1 \
    -p drop t1,0,0 -01ds+ >/dev/null 2>err.txt
)

png_count="$(find "$tmpdir" -maxdepth 1 -name '*.png' | wc -l)"
if [ "$png_count" -lt 1 ]; then
  echo "FAIL: expected static graph PNG alongside MP4 output" >&2
  cat "$tmpdir/err.txt" >&2 || true
  exit 1
fi

if [ ! -s "$tmpdir/graph.mp4" ]; then
  echo "FAIL: expected MP4 graph video output" >&2
  cat "$tmpdir/err.txt" >&2 || true
  exit 1
fi

probe="$(ffprobe -v error \
  -show_entries format=duration \
  -show_entries stream=codec_type,codec_name \
  -of default=noprint_wrappers=1 "$tmpdir/graph.mp4")"
video_probe="$(ffprobe -v error -select_streams v:0 \
  -show_entries stream=codec_name \
  -of default=noprint_wrappers=1:nokey=1 "$tmpdir/graph.mp4")"

echo "$probe" | grep -q '^codec_type=video$' || {
  echo "FAIL: expected video stream in graph MP4" >&2
  echo "$probe" >&2
  exit 1
}

echo "$probe" | grep -q '^codec_type=audio$' || {
  echo "FAIL: expected audio stream in graph MP4" >&2
  echo "$probe" >&2
  exit 1
}

echo "$probe" | grep -q '^codec_name=alac$' || {
  echo "FAIL: expected ALAC audio stream in graph MP4" >&2
  echo "$probe" >&2
  exit 1
}

if [ "$video_probe" != "h264" ]; then
  echo "FAIL: expected H.264 video stream" >&2
  echo "$probe" >&2
  exit 1
fi

dur="$(printf '%s\n' "$probe" | awk -F= '/^duration=/{print $2; exit}')"
python3 - "$dur" <<'PY'
import sys
dur = float(sys.argv[1])
if not (59.0 <= dur <= 61.5):
    raise SystemExit(f"unexpected graph video duration: {dur}")
PY

echo "PASS: -G graph video export writes MP4 with ALAC audio alongside the static graph PNG"
