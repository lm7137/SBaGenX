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

cat >"$tmpdir/log_iso_plot.py" <<'PY'
#!/usr/bin/env python3
import os
import shutil
import sys

sample_out = os.environ["SBX_CAPTURED_SAMPLE"]
real_script = os.environ["SBX_REAL_PLOT_SCRIPT"]

if "--sample-file" in sys.argv:
    sample_in = sys.argv[sys.argv.index("--sample-file") + 1]
    if os.path.exists(sample_in):
        shutil.copyfile(sample_in, sample_out)

os.execvp("python3", ["python3", real_script, *sys.argv[1:]])
PY
chmod +x "$tmpdir/log_iso_plot.py"

(
  cd "$tmpdir"
  SBAGENX_PLOT_BACKEND=python \
  SBAGENX_PLOT_SCRIPT="$tmpdir/log_iso_plot.py" \
  SBX_CAPTURED_SAMPLE="$tmpdir/captured_iso_samples.dat" \
  SBX_REAL_PLOT_SCRIPT="$ROOT_DIR/scripts/sbagenx_plot.py" \
  "$BIN" -P -I s=0:d=1:a=0.5:r=0.5:e=3 -i 200@1/100 \
    >/dev/null 2>err.txt
)

if ! ls "$tmpdir"/iso_cycle_*.png >/dev/null 2>&1; then
  echo "FAIL: expected Python/Cairo isochronic plot PNG" >&2
  cat "$tmpdir/err.txt" >&2 || true
  exit 1
fi

if [[ ! -s "$tmpdir/captured_iso_samples.dat" ]]; then
  echo "FAIL: expected captured sample file from Python/Cairo iso-cycle plot" >&2
  cat "$tmpdir/err.txt" >&2 || true
  exit 1
fi

python3 - "$tmpdir/captured_iso_samples.dat" <<'PY'
import sys
from pathlib import Path

rows = []
for line in Path(sys.argv[1]).read_text().splitlines():
    line = line.strip()
    if not line:
        continue
    t_s, env_s, wave_s = line.split()
    rows.append((float(t_s), float(env_s), float(wave_s)))

if len(rows) < 8:
    raise SystemExit("captured iso sample file is unexpectedly short")

for i, (_, env, _) in enumerate(rows[:8]):
    if abs(env - 1.0) > 1e-9:
        raise SystemExit(f"expected env=1.0 at row {i}, got {env}")
PY

echo "PASS: -P immediate isochronic plot applies -I override on sbagenxlib path"
