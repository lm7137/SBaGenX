#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BIN="${SBAGENX_TEST_BIN:-$ROOT_DIR/dist/sbagenx-linux64}"

if [[ ! -x "$BIN" ]]; then
  echo "Missing binary: $BIN" >&2
  echo "Set SBAGENX_TEST_BIN or run: bash linux-build-sbagenx.sh" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "SKIP: python3 unavailable"
  exit 0
fi

if ! python3 - <<'PY' >/dev/null 2>&1
import cairo
PY
then
  echo "SKIP: python3 cairo module unavailable"
  exit 0
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cat >"$tmpdir/log_drop_plot.py" <<'PY'
#!/usr/bin/env python3
import os
import shutil
import sys

sample_out = os.environ["SBX_CAPTURED_SAMPLE"]
desc_out = os.environ["SBX_CAPTURED_DESC"]
real_script = os.environ["SBX_REAL_PLOT_SCRIPT"]

if "--sample-file" in sys.argv:
    sample_in = sys.argv[sys.argv.index("--sample-file") + 1]
    if os.path.exists(sample_in):
        shutil.copyfile(sample_in, sample_out)

if "--desc-file" in sys.argv:
    desc_in = sys.argv[sys.argv.index("--desc-file") + 1]
    if os.path.exists(desc_in):
        shutil.copyfile(desc_in, desc_out)

os.execvp("python3", ["python3", real_script, *sys.argv[1:]])
PY
chmod +x "$tmpdir/log_drop_plot.py"

(
  cd "$tmpdir"
  SBAGENX_PLOT_BACKEND=python \
  SBAGENX_PLOT_SCRIPT="$tmpdir/log_drop_plot.py" \
  SBX_CAPTURED_SAMPLE="$tmpdir/captured_drop_samples.dat" \
  SBX_CAPTURED_DESC="$tmpdir/captured_drop_desc.meta" \
  SBX_REAL_PLOT_SCRIPT="$ROOT_DIR/scripts/sbagenx_plot.py" \
  "$BIN" -G -p drop t1,0,0 -01ds+ >/dev/null 2>err.txt
)

if ! ls "$tmpdir"/drop_*.png >/dev/null 2>&1; then
  echo "FAIL: expected Python/Cairo drop plot PNG" >&2
  cat "$tmpdir/err.txt" >&2 || true
  exit 1
fi

if [[ ! -s "$tmpdir/captured_drop_samples.dat" || ! -s "$tmpdir/captured_drop_desc.meta" ]]; then
  echo "FAIL: expected captured sample and desc files from external drop plot" >&2
  cat "$tmpdir/err.txt" >&2 || true
  exit 1
fi

python3 - "$tmpdir/captured_drop_samples.dat" "$tmpdir/captured_drop_desc.meta" <<'PY'
import sys
from pathlib import Path

sample_lines = [line.strip() for line in Path(sys.argv[1]).read_text().splitlines() if line.strip()]
if len(sample_lines) < 10:
    raise SystemExit("captured drop sample file is unexpectedly short")

desc = {}
for line in Path(sys.argv[2]).read_text().splitlines():
    line = line.strip()
    if not line or "=" not in line:
        continue
    k, v = line.split("=", 1)
    desc[k.strip()] = v.strip()

if desc.get("x_label") != "TIME MIN":
    raise SystemExit(f"unexpected x_label: {desc.get('x_label')!r}")
if desc.get("y_label") != "FREQ HZ":
    raise SystemExit(f"unexpected y_label: {desc.get('y_label')!r}")
if "start=10.000Hz" not in desc.get("line1", ""):
    raise SystemExit(f"unexpected line1: {desc.get('line1')!r}")
if "continuous" not in desc.get("line2", ""):
    raise SystemExit(f"unexpected line2: {desc.get('line2')!r}")
PY

echo "PASS: external drop plotting uses library sample and metadata files"
