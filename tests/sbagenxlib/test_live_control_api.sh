#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
cc -I. -Itests/sbagenxlib -Wall -Wextra \
  -o /tmp/test_live_control_api \
  tests/sbagenxlib/test_live_control_api.c \
  sbagenxlib.c -lm -ldl
/tmp/test_live_control_api
