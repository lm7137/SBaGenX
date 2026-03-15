#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${SBAGENX_TEST_BIN:-$ROOT_DIR/dist/sbagenx-linux64}"

if [[ ! -x "$BIN" ]]; then
  echo "Missing binary: $BIN" >&2
  echo "Set SBAGENX_TEST_BIN or run: bash linux-build-sbagenx.sh" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

python3 - <<'PY' "$tmpdir/mix48.wav"
import struct
import sys
import wave

path = sys.argv[1]
sr = 48000
dur = 5
n = sr * dur
with wave.open(path, "wb") as wf:
    wf.setnchannels(2)
    wf.setsampwidth(2)
    wf.setframerate(sr)
    wf.writeframes(b"\x00\x00\x00\x00" * n)
PY

cat >"$tmpdir/bell.sbg" <<'EOF'
quiet: 200+0/0
cue: bell300/100
00:00 quiet
00:00:03 cue
00:00:04 quiet
EOF

"$BIN" -S -L 0:00:05 -m "$tmpdir/mix48.wav" -o "$tmpdir/out.wav" -W "$tmpdir/bell.sbg" >/dev/null 2>"$tmpdir/err.raw"

python3 - <<'PY' "$tmpdir/out.wav"
import struct
import sys
import wave

path = sys.argv[1]
with wave.open(path, "rb") as wf:
    sr = wf.getframerate()
    if sr != 48000:
        print(f"FAIL: expected output WAV rate 48000, got {sr}", file=sys.stderr)
        sys.exit(1)
    nch = wf.getnchannels()
    sw = wf.getsampwidth()
    if nch != 2 or sw != 2:
        print(f"FAIL: expected 16-bit stereo WAV, got channels={nch} sampwidth={sw}", file=sys.stderr)
        sys.exit(1)
    raw = wf.readframes(wf.getnframes())
    n = len(raw) // (sw * nch)
    if n <= 0:
        print("FAIL: rendered WAV contains no audio payload", file=sys.stderr)
        sys.exit(1)

vals = struct.unpack("<" + "h" * (n * nch), raw)
win = sr // 100
energies = []
for i in range(0, n, win):
    acc = 0.0
    cnt = 0
    for j in range(i, min(i + win, n)):
        l = vals[j * 2]
        r = vals[j * 2 + 1]
        acc += abs(l) + abs(r)
        cnt += 1
    energies.append((i / sr, acc / max(cnt, 1)))

baseline = sum(e for _, e in energies[:50]) / 50.0
peak = max(e for _, e in energies)
threshold = baseline + (peak - baseline) * 0.35
first = next((t for t, e in energies if t >= 2.5 and e >= threshold), None)
if first is None:
    print("FAIL: bell onset not detected in rendered WAV", file=sys.stderr)
    sys.exit(1)
if abs(first - 3.0) > 0.05:
    print(f"FAIL: bell onset at {first:.3f}s, expected about 3.000s", file=sys.stderr)
    sys.exit(1)
PY

echo "PASS: mix-input sample rate is applied before sbagenxlib runtime timing"
