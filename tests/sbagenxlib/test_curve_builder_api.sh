#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
gcc -I"$ROOT_DIR" -I"$ROOT_DIR/tests/sbagenxlib" -Wall -Wextra \
  -o /tmp/test_curve_builder_api \
  "$ROOT_DIR/tests/sbagenxlib/test_curve_builder_api.c" \
  "$ROOT_DIR/sbagenxlib.c" -lm

/tmp/test_curve_builder_api
