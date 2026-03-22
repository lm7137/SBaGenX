#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cc -I"$ROOT_DIR" -I"$ROOT_DIR/tests/sbagenxlib" -Wall -Wextra \
  -o /tmp/test_program_plot_sampling_api \
  "$ROOT_DIR/tests/sbagenxlib/test_program_plot_sampling_api.c" \
  "$ROOT_DIR/sbagenxlib.c" -lm
/tmp/test_program_plot_sampling_api
