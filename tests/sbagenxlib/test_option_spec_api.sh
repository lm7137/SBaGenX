#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

gcc -I"$ROOT_DIR" -I"$ROOT_DIR/tests/sbagenxlib" \
  -o /tmp/test_option_spec_api \
  "$ROOT_DIR/tests/sbagenxlib/test_option_spec_api.c" \
  "$ROOT_DIR/sbagenxlib.c" -lm

/tmp/test_option_spec_api
