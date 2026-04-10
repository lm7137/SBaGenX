#!/usr/bin/env python3
import math
import statistics
import struct
import sys
from pathlib import Path

RATE = 44100.0
FREQ = 1000.0
FRAMES = 32768
TAU = 2.0 * math.pi


def read_left_i16(path: Path):
    data = path.read_bytes()
    vals = struct.unpack("<" + "h" * (len(data) // 2), data)
    return [vals[i] / 32768.0 for i in range(0, len(vals), 2)]


def read_left_i32(path: Path):
    data = path.read_bytes()
    vals = struct.unpack("<" + "i" * (len(data) // 4), data)
    return [vals[i] / 2147483648.0 for i in range(0, len(vals), 2)]


def read_left_f32(path: Path):
    data = path.read_bytes()
    vals = struct.unpack("<" + "f" * (len(data) // 4), data)
    return [float(vals[i]) for i in range(0, len(vals), 2)]


def fit_sine(samples):
    w = TAU * FREQ / RATE
    ss = cc = sc = xs = xc = 0.0
    for n, x in enumerate(samples):
        s = math.sin(w * n)
        c = math.cos(w * n)
        ss += s * s
        cc += c * c
        sc += s * c
        xs += x * s
        xc += x * c
    det = ss * cc - sc * sc
    a = (xs * cc - xc * sc) / det
    b = (xc * ss - xs * sc) / det
    amp = math.hypot(a, b)
    phase = math.atan2(b, a)
    fit = [a * math.sin(w * n) + b * math.cos(w * n) for n in range(len(samples))]
    err = [x - y for x, y in zip(samples, fit)]
    return amp, phase, err


def fft_iterative_real(samples):
    n = len(samples)
    a = [complex(x, 0.0) for x in samples]
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j ^= bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    m = 2
    while m <= n:
        ang = -2.0 * math.pi / m
        wm = complex(math.cos(ang), math.sin(ang))
        half = m >> 1
        for k in range(0, n, m):
            w = 1.0 + 0.0j
            for j2 in range(half):
                t = w * a[k + j2 + half]
                u = a[k + j2]
                a[k + j2] = u + t
                a[k + j2 + half] = u - t
                w *= wm
        m <<= 1
    return a


def residual_metrics(err):
    mid = 0.5 * (FRAMES - 1)
    fact = math.pi / mid
    win = []
    tot = 0.0
    for i in range(FRAMES):
        theta = (i - mid) * fact
        val = 0.42 + 0.5 * math.cos(theta) + 0.08 * math.cos(2.0 * theta)
        win.append(val)
        tot += val
    win_err = [e * w for e, w in zip(err, win)]
    adj = (2.0 / FRAMES) / (tot / FRAMES)
    spec = fft_iterative_real(win_err)
    mags = [abs(spec[k]) * adj for k in range(FRAMES // 2 + 1)]
    mags_no_dc = mags[1:]
    max_bin = max(mags_no_dc)
    mean_bin = sum(mags_no_dc) / len(mags_no_dc)
    median_bin = statistics.median(mags_no_dc)
    rms = math.sqrt(sum(e * e for e in err) / len(err))
    return {
        "err_rms": rms,
        "max_bin": max_bin,
        "mean_bin": mean_bin,
        "median_bin": median_bin,
        "max_bin_dbfs": 20.0 * math.log10(max_bin),
        "mean_bin_dbfs": 20.0 * math.log10(mean_bin),
        "median_bin_dbfs": 20.0 * math.log10(median_bin),
        "rms_dbfs": 20.0 * math.log10(rms),
        "bits_from_max": -(20.0 * math.log10(max_bin)) / 6.020599913,
        "bits_from_rms": -(20.0 * math.log10(rms)) / 6.020599913,
    }


def analyze(name, samples):
    amp, phase, err = fit_sine(samples)
    metrics = residual_metrics(err)
    metrics.update({
        "name": name,
        "fit_amp": amp,
        "fit_phase": phase,
        "max_bin_dbc": 20.0 * math.log10(metrics["max_bin"] / amp),
        "err_rms_dbc": 20.0 * math.log10(metrics["err_rms"] / amp),
    })
    return metrics


def main(argv):
    if len(argv) != 5:
        raise SystemExit(
            "usage: benchmark_sbglib_quality_compare.py <sbg16.raw> <sbg32.raw> <sbx_f32.raw> <sbx_s16.raw>"
        )

    data = {
        "sbglib16": read_left_i16(Path(argv[1])),
        "sbglib32": read_left_i32(Path(argv[2])),
        "sbx_f32": read_left_f32(Path(argv[3])),
        "sbx_s16": read_left_i16(Path(argv[4])),
    }
    for name, arr in data.items():
        if len(arr) != FRAMES:
            raise SystemExit(f"{name}: expected {FRAMES} frames, got {len(arr)}")

    results = [analyze(name, arr) for name, arr in data.items()]
    for r in results:
        print(r["name"])
        print(f"  fit_amp={r['fit_amp']:.12g} phase={r['fit_phase']:.12g}")
        print(
            f"  err_rms={r['err_rms']:.12g} ({r['rms_dbfs']:.2f} dBFS, ~{r['bits_from_rms']:.2f} bits, {r['err_rms_dbc']:.2f} dBc)"
        )
        print(
            f"  max_residual_bin={r['max_bin']:.12g} ({r['max_bin_dbfs']:.2f} dBFS, ~{r['bits_from_max']:.2f} bits, {r['max_bin_dbc']:.2f} dBc)"
        )
        print(f"  mean_residual_bin={r['mean_bin']:.12g} ({r['mean_bin_dbfs']:.2f} dBFS)")
        print(f"  median_residual_bin={r['median_bin']:.12g} ({r['median_bin_dbfs']:.2f} dBFS)")

    lookup = {r["name"]: r for r in results}
    print("\nRelative comparisons:")
    for a, b in (("sbglib16", "sbx_s16"), ("sbglib32", "sbx_f32")):
        ra, rb = lookup[a], lookup[b]
        print(f"  {a} vs {b}:")
        print(
            f"    max residual bin delta = {rb['max_bin_dbfs'] - ra['max_bin_dbfs']:+.2f} dB (negative favors {b})"
        )
        print(
            f"    residual RMS delta     = {rb['rms_dbfs'] - ra['rms_dbfs']:+.2f} dB (negative favors {b})"
        )


if __name__ == "__main__":
    main(sys.argv)
