#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

usage() {
  cat <<USAGE
Usage:
  $0 --sbglib-root /path/to/sbglib
  $0 --sbglib-zip /path/to/sbglib.zip

Optional env:
  CC=<compiler>    default: gcc

The benchmark is optional and not part of the normal automated test suite.
It compares a low-amplitude 1000 Hz sine fixture between SBGLIB and sbagenxlib.
USAGE
}

SBGLIB_ROOT=""
SBGLIB_ZIP=""
CC_BIN="${CC:-gcc}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sbglib-root)
      SBGLIB_ROOT="$2"
      shift 2
      ;;
    --sbglib-zip)
      SBGLIB_ZIP="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ -n "$SBGLIB_ROOT" && -n "$SBGLIB_ZIP" ]]; then
  echo "Choose either --sbglib-root or --sbglib-zip, not both" >&2
  exit 2
fi

if [[ -n "$SBGLIB_ZIP" ]]; then
  unzip -q "$SBGLIB_ZIP" -d "$WORKDIR/unzip"
  mapfile -t api_hits < <(find "$WORKDIR/unzip" -path '*/src/api.h' | sort)
  if [[ "${#api_hits[@]}" -ne 1 ]]; then
    echo "Unable to locate unique src/api.h inside $SBGLIB_ZIP" >&2
    exit 1
  fi
  SBGLIB_ROOT="$(cd "$(dirname "${api_hits[0]}")/.." && pwd)"
fi

if [[ -z "$SBGLIB_ROOT" ]]; then
  echo "Missing SBGLIB source location" >&2
  usage >&2
  exit 2
fi

if [[ ! -f "$SBGLIB_ROOT/src/api.h" || ! -f "$SBGLIB_ROOT/src/sbglib.c" ]]; then
  echo "Invalid SBGLIB root: $SBGLIB_ROOT" >&2
  exit 1
fi

SBG_GEN="$WORKDIR/sbg_quality_gen"
SBX_GEN="$WORKDIR/sbx_quality_gen"

COMMON_WARN=(-Wall -Wextra)
SBG_WARN=(-Wno-unused-parameter -Wno-unused-function)
SBX_WARN=(-Wno-unused-function -Wno-clobbered)

echo "Using SBGLIB root: $SBGLIB_ROOT"

"$CC_BIN" -O2 "${COMMON_WARN[@]}" "${SBG_WARN[@]}"   -I"$SBGLIB_ROOT/src"   "$ROOT_DIR/tests/sbagenxlib/benchmark_sbglib_quality_sbg.c"   "$SBGLIB_ROOT/src/sbglib.c" -lm   -o "$SBG_GEN"

"$CC_BIN" -O2 "${COMMON_WARN[@]}" "${SBX_WARN[@]}"   -I"$ROOT_DIR"   "$ROOT_DIR/tests/sbagenxlib/benchmark_sbglib_quality_sbx.c"   "$ROOT_DIR/sbagenxlib.c" -lm -ldl   -o "$SBX_GEN"

"$SBG_GEN" "$WORKDIR/sbg16.raw" "$WORKDIR/sbg32.raw"
"$SBX_GEN" "$WORKDIR/sbx_f32.raw" "$WORKDIR/sbx_s16.raw"

python3 "$ROOT_DIR/tests/sbagenxlib/benchmark_sbglib_quality_compare.py" \
  "$WORKDIR/sbg16.raw" \
  "$WORKDIR/sbg32.raw" \
  "$WORKDIR/sbx_f32.raw" \
  "$WORKDIR/sbx_s16.raw"
