#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

bin="./dist/sbagenx-linux64"
if [ ! -x "$bin" ]; then
  echo "SKIP: $bin not built" >&2
  exit 0
fi

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

seq_debug="$tmpdir/customnn_r400_debug.sbg"
cat > "$seq_debug" <<EOF2
-SE
-o $tmpdir/test.flac
-Z 12
-R 400

custom00: e=2 0 0.1 0.9 1 1 0.9 0.1 0

base: custom00:400@2/20
alloff: -

NOW base
+00:05:00 base ->
+00:08:00 alloff
EOF2

if ! "$bin" -D "$seq_debug" > "$tmpdir/debug.txt" 2>&1; then
  echo "FAIL: customNN safe preamble with -R 400 did not parse" >&2
  cat "$tmpdir/debug.txt" >&2
  exit 1
fi

if ! grep -q "# sbagenxlib keyframes" "$tmpdir/debug.txt"; then
  echo "FAIL: expected sbagenxlib keyframe dump for safe preamble with -R 400" >&2
  cat "$tmpdir/debug.txt" >&2
  exit 1
fi

if ! grep -q "780.000000 - linear" "$tmpdir/debug.txt"; then
  echo "FAIL: expected final keyframe at 780s for relative +00:08:00 syntax" >&2
  cat "$tmpdir/debug.txt" >&2
  exit 1
fi

seq_wav="$tmpdir/customnn_r400_wav.sbg"
cat > "$seq_wav" <<EOF2
-SE
-Wo $tmpdir/test.wav
-R 400

custom00: e=2 0 0.1 0.9 1 1 0.9 0.1 0

base: custom00:400@2/20
alloff: -

NOW base
NOW+00:05:00 base ->
NOW+00:08:00 alloff
EOF2

if ! "$bin" "$seq_wav" > "$tmpdir/render.log" 2>&1; then
  echo "FAIL: safe preamble render with -R 400 failed" >&2
  cat "$tmpdir/render.log" >&2
  exit 1
fi

python3 - <<'PY' "$tmpdir/test.wav"
import sys, wave
p = sys.argv[1]
w = wave.open(p, 'rb')
dur = w.getnframes() / w.getframerate()
if abs(dur - 480.0) > 0.01:
    raise SystemExit(f"unexpected duration {dur}")
PY

echo "PASS: customNN safe preamble accepts -R/-W and preserves expected duration semantics"
