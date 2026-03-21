#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

gcc -I"$ROOT_DIR" -I"$ROOT_DIR/tests/sbagenxlib" \
  -o /tmp/test_audio_writer_api \
  "$ROOT_DIR/tests/sbagenxlib/test_audio_writer_api.c" \
  "$ROOT_DIR/sbagenxlib.c" -lm

/tmp/test_audio_writer_api
