#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

tmp_bin="/tmp/live_control_demo.$$"
cleanup() {
  rm -f "$tmp_bin"
}
trap cleanup EXIT

cc -I. -Itests/sbagenxlib -Wall -Wextra -o "$tmp_bin" \
  tests/sbagenxlib/live_control_demo.c sbagenxlib.c -lm -ldl

"$tmp_bin" "$@"
