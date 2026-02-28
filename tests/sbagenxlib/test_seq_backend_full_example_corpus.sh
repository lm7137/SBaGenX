#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT_DIR/dist/sbagenx-linux64"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: expected built binary at $BIN" >&2
  exit 1
fi

mapfile -t FILES < <(find "$ROOT_DIR/examples" -type f -name '*.sbg' | sort)

if [[ ${#FILES[@]} -eq 0 ]]; then
  echo "FAIL: no .sbg examples found under $ROOT_DIR/examples" >&2
  exit 1
fi

FAIL_COUNT=0

for file in "${FILES[@]}"; do
  if ! SBAGENX_SEQ_BACKEND=sbagenxlib "$BIN" -D "$file" >/tmp/sbx_full_corpus.out 2>/tmp/sbx_full_corpus.err; then
    echo "FAIL: $(realpath --relative-to="$ROOT_DIR" "$file")" >&2
    if [[ -s /tmp/sbx_full_corpus.err ]]; then
      head -n 1 /tmp/sbx_full_corpus.err >&2
    else
      head -n 1 /tmp/sbx_full_corpus.out >&2 || true
    fi
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi
done

if [[ $FAIL_COUNT -ne 0 ]]; then
  echo "FAIL: full example corpus regression detected ($FAIL_COUNT failures)" >&2
  exit 1
fi

echo "PASS: seq backend full example corpus smoke test (${#FILES[@]} examples)"
