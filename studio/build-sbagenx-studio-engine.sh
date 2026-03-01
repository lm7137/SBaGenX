#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/studio/bin"
OUT_BIN="$OUT_DIR/sbagenx-studio-engine"
CC_BIN="${CC:-gcc}"

mkdir -p "$OUT_DIR"

"$CC_BIN" -O2 -Wall -Wextra -I"$ROOT_DIR" \
  "$ROOT_DIR/studio/tools/sbagenx-studio-engine.c" \
  "$ROOT_DIR/sbagenxlib.c" \
  -lm \
  -o "$OUT_BIN"

echo "$OUT_BIN"
