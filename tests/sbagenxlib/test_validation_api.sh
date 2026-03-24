#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cc -I"$ROOT_DIR" -I"$ROOT_DIR/tests/sbagenxlib" -Wall -Wextra \
  -o /tmp/test_validation_api \
  "$ROOT_DIR/tests/sbagenxlib/test_validation_api.c" \
  "$ROOT_DIR/sbagenxlib.c" -lm
/tmp/test_validation_api
