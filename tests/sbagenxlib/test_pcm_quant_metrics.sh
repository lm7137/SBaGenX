#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

gcc -I"$ROOT_DIR" -Wall -Wextra -O2 \
  "$ROOT_DIR/tests/sbagenxlib/test_pcm_quant_metrics.c" \
  "$ROOT_DIR/sbagenxlib.c" -lm \
  -o "$tmpdir/test_pcm_quant_metrics"

"$tmpdir/test_pcm_quant_metrics"
