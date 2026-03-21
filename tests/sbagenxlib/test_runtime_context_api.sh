#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

gcc -I. -Itests/sbagenxlib -Wall -Wextra \
  -o /tmp/test_runtime_context_api \
  tests/sbagenxlib/test_runtime_context_api.c \
  sbagenxlib.c -lm

/tmp/test_runtime_context_api
