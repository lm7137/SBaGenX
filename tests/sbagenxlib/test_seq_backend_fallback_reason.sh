#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

bin="${repo_root}/dist/sbagenx-linux64"
if [[ ! -x "$bin" ]]; then
  echo "FAIL: binary not found: $bin" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

out="$tmpdir/out.txt"

printf 'tone: 200+4/20\n00:00 tone\n' | "$bin" -D - >"$out" 2>&1

if ! grep -q "Falling back to legacy sequence parser/runtime: stdin sequence input is not yet routed through the sbagenxlib direct loader" "$out"; then
  echo "FAIL: expected explicit sbagenxlib fallback reason for stdin sequence input" >&2
  cat "$out" >&2
  exit 1
fi

if ! grep -q "\*\*\* Sequence duration: 00:00:00 (hh:mm:ss) \*\*\*" "$out"; then
  echo "FAIL: expected legacy dump to continue after reported fallback" >&2
  cat "$out" >&2
  exit 1
fi

echo "PASS: sequence fallback reports an explicit sbagenxlib rejection reason"
