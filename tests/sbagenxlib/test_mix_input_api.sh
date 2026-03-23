#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

python3 - <<'PY' "$tmpdir/mix48.wav" "$tmpdir/mix.raw"
import struct
import sys
import wave

wav_path, raw_path = sys.argv[1], sys.argv[2]

with wave.open(wav_path, "wb") as wf:
    wf.setnchannels(2)
    wf.setsampwidth(2)
    wf.setframerate(48000)
    wf.writeframes(struct.pack("<hhhhhhhh", 0, 0, 1, -1, 2, -2, 3, -3))

with open(raw_path, "wb") as fh:
    fh.write(struct.pack("<hhhh", 1, -2, 3, -4))
    fh.write(struct.pack("<hhhh", 5, -6, 7, -8))
PY

gcc -I"$ROOT_DIR" -I"$ROOT_DIR/tests/sbagenxlib" \
  -o /tmp/test_mix_input_api \
  "$ROOT_DIR/tests/sbagenxlib/test_mix_input_api.c" \
  "$ROOT_DIR/sbagenxlib.c" -lm

/tmp/test_mix_input_api "$tmpdir/mix48.wav" "$tmpdir/mix.raw"
