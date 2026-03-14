#!/usr/bin/env python3
"""
High-quality SBaGenX plot renderer (Python + Cairo).

Modes:
  - sigmoid:   beat/pulse curve used by -G
  - drop:      beat/pulse curve used by -G with -p drop
  - curve:     beat/pulse curve used by -G with -p curve
  - iso-cycle: one-cycle isochronic envelope + waveform used by -P
  - mixam-cycle: one-cycle mixam envelope + gain used by -P -H
  - wave-lr:   one-cycle custom waveNN envelope + L/R waveforms
"""

import argparse
import math
import os
import sys

try:
    import cairo
except Exception as exc:
    sys.stderr.write(f"Python/Cairo backend unavailable: {exc}\n")
    sys.exit(2)


PI = math.pi


def _fmt_tick(value: float) -> str:
    if abs(value) < 5e-4:
        value = 0.0
    text = f"{value:.2f}"
    while text.endswith("0"):
        text = text[:-1]
    if text.endswith("."):
        text = text[:-1]
    if text == "-0":
        text = "0"
    return text


def _setup_canvas(width: int, height: int):
    surface = cairo.ImageSurface(cairo.FORMAT_RGB24, width, height)
    ctx = cairo.Context(surface)
    ctx.set_antialias(cairo.ANTIALIAS_BEST)
    ctx.set_source_rgb(1.0, 1.0, 1.0)
    ctx.paint()
    return surface, ctx


def _draw_grid_box(ctx, x0, y0, w, h, x_divs, y_vals, y_map):
    # soft background
    ctx.set_source_rgb(0.98, 0.98, 0.98)
    ctx.rectangle(x0, y0, w, h)
    ctx.fill()

    # vertical grid
    ctx.set_source_rgb(0.93, 0.93, 0.93)
    ctx.set_line_width(1.0)
    for i in range(x_divs + 1):
        x = x0 + (w - 1) * i / x_divs
        ctx.move_to(x, y0)
        ctx.line_to(x, y0 + h - 1)
        ctx.stroke()

    # horizontal grid
    for yv in y_vals:
        y = y_map(yv)
        ctx.move_to(x0, y)
        ctx.line_to(x0 + w - 1, y)
        ctx.stroke()

    # border
    ctx.set_source_rgb(0.35, 0.35, 0.35)
    ctx.set_line_width(1.2)
    ctx.rectangle(x0, y0, w, h)
    ctx.stroke()


def _build_integer_ticks(max_v: float):
    if max_v <= 0:
        return [0.0, 1.0]

    step = max(1, int(round(max_v / 10.0)))
    ticks = []
    value = 0
    while value <= max_v + 1e-9:
        ticks.append(float(value))
        value += step
    if not ticks:
        ticks = [0.0]
    if abs(ticks[-1] - max_v) > 1e-9:
        ticks.append(float(max_v))
    return ticks


def _build_linear_ticks(max_v: float, n: int = 10):
    if n <= 0:
        return [0.0, max_v]
    return [max_v * i / float(n) for i in range(n + 1)]


def _parse_wave_samples(spec: str):
    vals = []
    for tok in spec.replace(",", " ").split():
        vals.append(float(tok))
    if len(vals) < 2:
        raise ValueError("Need at least two samples in --samples")
    vmin = min(vals)
    vmax = max(vals)
    if abs(vmax - vmin) < 1e-12:
        raise ValueError("Wave samples must not all be identical")
    return vals


def _build_wave_env_table(samples, table_size: int = 16384):
    # Match C implementation (sinc_interpolate) used by waveNN runtime.
    if table_size <= 0 or (table_size & (table_size - 1)) != 0:
        raise ValueError("table_size must be a power of 2")

    vmin = min(samples)
    vmax = max(samples)
    norm = [(v - vmin) / (vmax - vmin) for v in samples]
    np_samp = len(norm)

    sinc = [0.0] * table_size
    sinc[0] = 1.0
    half = table_size // 2
    for a in range(half, 0, -1):
        tt = a / float(table_size)
        t2 = tt * tt
        adj = 1.0 - 4.0 * t2
        xx = 2.0 * np_samp * PI * tt
        vv = adj * math.sin(xx) / xx
        sinc[a] = vv
        sinc[table_size - a] = vv

    out = [0.0] * table_size
    for b, val in enumerate(norm):
        off = (b * table_size) // np_samp
        for a, sv in enumerate(sinc):
            out[(a + off) & (table_size - 1)] += sv * val

    dmin = min(out)
    dmax = max(out)
    span = dmax - dmin
    if span < 1e-12:
        raise ValueError("Interpolated waveform has near-zero dynamic range")

    env = []
    for v in out:
        u = (v - dmin) / span
        if u < 0.0:
            u = 0.0
        elif u > 1.0:
            u = 1.0
        env.append(u)
    return env


def _sigmoid_eval(t_min, d_min, beat_target, sig_l, sig_h, sig_a, sig_b):
    if t_min >= d_min:
        return beat_target
    return sig_a * math.tanh(sig_l * (t_min - d_min / 2.0 - sig_h)) + sig_b


def _drop_eval(t_min, d_min, beat_start, beat_target, slide, n_step, step_len_sec):
    if d_min <= 0:
        return beat_target
    t_min = min(max(t_min, 0.0), d_min)
    if beat_start <= 0.0:
        beat_start = 10.0

    if not slide and n_step > 1 and step_len_sec > 0:
        idx = int((t_min * 60.0) / step_len_sec)
        idx = max(0, min(idx, n_step - 1))
        return beat_start * math.exp(math.log(beat_target / beat_start) * idx / (n_step - 1))

    return beat_start * math.exp(math.log(beat_target / beat_start) * (t_min / d_min))


def render_sigmoid(args):
    width, height = 1200, 700
    ml, mr, mt, mb = 150, 40, 40, 120
    pw, ph = width - ml - mr, height - mt - mb

    d_min = args.drop_min
    if d_min <= 0:
        raise ValueError("drop-min must be > 0")

    # determine y range
    y_min = float("inf")
    y_max = float("-inf")
    for i in range(2001):
        t = d_min * i / 2000.0
        y = _sigmoid_eval(t, d_min, args.beat_target, args.sig_l, args.sig_h, args.sig_a, args.sig_b)
        y_min = min(y_min, y)
        y_max = max(y_max, y)
    y_min = min(y_min, args.beat_start, args.beat_target)
    y_max = max(y_max, args.beat_start, args.beat_target)

    if abs(y_max - y_min) < 1e-6:
        y_min -= 1.0
        y_max += 1.0

    y_pad = max((y_max - y_min) * 0.08, 0.1)
    y_min -= y_pad
    y_max += y_pad
    y_span = y_max - y_min

    # Use whole-number Y ticks for readability.
    y_tick_step = max(1.0, round(y_span / 8.0))

    y_tick_first = math.ceil((y_min - 1e-9) / y_tick_step) * y_tick_step
    y_ticks = []
    yv = y_tick_first
    while yv <= y_max + y_tick_step * 0.25:
        y_ticks.append(yv)
        yv += y_tick_step
    x_ticks = _build_integer_ticks(d_min)

    surface, ctx = _setup_canvas(width, height)

    def x_map(t):
        return ml + (pw - 1) * (t / d_min)

    def y_map(v):
        return mt + (y_max - v) * (ph - 1) / y_span

    _draw_grid_box(ctx, ml, mt, pw, ph, 10, y_ticks, y_map)

    # curve
    ctx.set_source_rgb(0.13, 0.37, 0.88)
    ctx.set_line_width(2.0)
    first = True
    for i in range(2001):
        t = d_min * i / 2000.0
        y = _sigmoid_eval(t, d_min, args.beat_target, args.sig_l, args.sig_h, args.sig_a, args.sig_b)
        px = x_map(t)
        py = y_map(y)
        if first:
            ctx.move_to(px, py)
            first = False
        else:
            ctx.line_to(px, py)
    ctx.stroke()

    # endpoint markers
    for t in (0.0, d_min):
        y = _sigmoid_eval(t, d_min, args.beat_target, args.sig_l, args.sig_h, args.sig_a, args.sig_b)
        px = x_map(t)
        py = y_map(y)
        ctx.set_source_rgb(0.86, 0.16, 0.16)
        ctx.arc(px, py, 2.8, 0.0, 2.0 * PI)
        ctx.fill()

    # tick marks + labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(16)
    ctx.set_source_rgb(0.15, 0.15, 0.15)

    # x ticks
    for t in x_ticks:
        x = x_map(t)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.set_line_width(1.0)
        ctx.move_to(x, mt + ph - 1)
        ctx.line_to(x, mt + ph + 4)
        ctx.stroke()
        txt = _fmt_tick(t)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(x - ext.width / 2, mt + ph + 22)
        ctx.show_text(txt)

    # y ticks
    for yv in y_ticks:
        y = y_map(yv)
        txt = _fmt_tick(yv)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.move_to(ml - 4, y)
        ctx.line_to(ml, y)
        ctx.stroke()
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(ml - 8 - ext.width, y + ext.height / 2)
        ctx.show_text(txt)

    # axis labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(18)
    x_label = "TIME MIN"
    y_label = "FREQ HZ"

    ext = ctx.text_extents(x_label)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 70)
    ctx.show_text(x_label)

    ctx.save()
    ctx.translate(38, mt + ph / 2)
    ctx.rotate(-PI / 2.0)
    ext = ctx.text_extents(y_label)
    ctx.move_to(-ext.width / 2, 0)
    ctx.show_text(y_label)
    ctx.restore()

    # parameter lines (bottom)
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(14)
    line1 = (
        f"start={args.beat_start:.3f}Hz  target={args.beat_target:.3f}Hz  "
        f"D={args.drop_min:.1f}min"
    )
    line2 = (
        f"l={args.sig_l:.4f}  h={args.sig_h:.4f}  "
        f"a={args.sig_a:.4f}  b={args.sig_b:.4f}"
    )
    ext = ctx.text_extents(line1)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 40)
    ctx.show_text(line1)
    ext = ctx.text_extents(line2)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 18)
    ctx.show_text(line2)

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    surface.write_to_png(args.out)


def render_drop(args):
    width, height = 1200, 700
    ml, mr, mt, mb = 150, 40, 40, 120
    pw, ph = width - ml - mr, height - mt - mb

    d_min = args.drop_min
    if d_min <= 0:
        raise ValueError("drop-min must be > 0")

    slide = bool(args.slide)
    n_step = int(args.n_step)
    step_len_sec = int(args.step_len_sec)

    # determine y range
    y_min = float("inf")
    y_max = float("-inf")
    for i in range(2001):
        t = d_min * i / 2000.0
        y = _drop_eval(t, d_min, args.beat_start, args.beat_target, slide, n_step, step_len_sec)
        y_min = min(y_min, y)
        y_max = max(y_max, y)
    y_min = min(y_min, args.beat_start, args.beat_target)
    y_max = max(y_max, args.beat_start, args.beat_target)

    if abs(y_max - y_min) < 1e-6:
        y_min -= 1.0
        y_max += 1.0

    y_pad = max((y_max - y_min) * 0.08, 0.1)
    y_min -= y_pad
    y_max += y_pad
    y_span = y_max - y_min

    y_tick_step = max(1.0, round(y_span / 8.0))
    y_tick_first = math.ceil((y_min - 1e-9) / y_tick_step) * y_tick_step
    y_ticks = []
    yv = y_tick_first
    while yv <= y_max + y_tick_step * 0.25:
        y_ticks.append(yv)
        yv += y_tick_step
    x_ticks = _build_integer_ticks(d_min)

    surface, ctx = _setup_canvas(width, height)

    def x_map(t):
        return ml + (pw - 1) * (t / d_min)

    def y_map(v):
        return mt + (y_max - v) * (ph - 1) / y_span

    _draw_grid_box(ctx, ml, mt, pw, ph, 10, y_ticks, y_map)

    # curve
    ctx.set_source_rgb(0.13, 0.37, 0.88)
    ctx.set_line_width(2.0)
    first = True
    for i in range(2001):
        t = d_min * i / 2000.0
        y = _drop_eval(t, d_min, args.beat_start, args.beat_target, slide, n_step, step_len_sec)
        px = x_map(t)
        py = y_map(y)
        if first:
            ctx.move_to(px, py)
            first = False
        else:
            ctx.line_to(px, py)
    ctx.stroke()

    # endpoint markers
    for t in (0.0, d_min):
        y = _drop_eval(t, d_min, args.beat_start, args.beat_target, slide, n_step, step_len_sec)
        px = x_map(t)
        py = y_map(y)
        ctx.set_source_rgb(0.86, 0.16, 0.16)
        ctx.arc(px, py, 2.8, 0.0, 2.0 * PI)
        ctx.fill()

    # tick marks + labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(16)
    ctx.set_source_rgb(0.15, 0.15, 0.15)

    for t in x_ticks:
        x = x_map(t)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.set_line_width(1.0)
        ctx.move_to(x, mt + ph - 1)
        ctx.line_to(x, mt + ph + 4)
        ctx.stroke()
        txt = _fmt_tick(t)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(x - ext.width / 2, mt + ph + 22)
        ctx.show_text(txt)

    for yv in y_ticks:
        y = y_map(yv)
        txt = _fmt_tick(yv)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.move_to(ml - 4, y)
        ctx.line_to(ml, y)
        ctx.stroke()
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(ml - 8 - ext.width, y + ext.height / 2)
        ctx.show_text(txt)

    # axis labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(18)
    x_label = "TIME MIN"
    y_label = "FREQ HZ"

    ext = ctx.text_extents(x_label)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 70)
    ctx.show_text(x_label)

    ctx.save()
    ctx.translate(38, mt + ph / 2)
    ctx.rotate(-PI / 2.0)
    ext = ctx.text_extents(y_label)
    ctx.move_to(-ext.width / 2, 0)
    ctx.show_text(y_label)
    ctx.restore()

    # parameter lines (bottom)
    mode_map = {0: "binaural beat", 1: "pulse", 2: "monaural beat"}
    mode_label = mode_map.get(int(args.mode_kind), "binaural beat")
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(14)
    line1 = (
        f"start={args.beat_start:.3f}Hz  target={args.beat_target:.3f}Hz  "
        f"D={args.drop_min:.1f}min"
    )
    if slide:
        line2 = f"{mode_label} mode: continuous exponential (s)"
    else:
        line2 = (
            f"{mode_label} mode: stepped exponential (k/default), "
            f"step={step_len_sec}s n={n_step}"
        )

    ext = ctx.text_extents(line1)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 40)
    ctx.show_text(line1)
    ext = ctx.text_extents(line2)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 18)
    ctx.show_text(line2)

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    surface.write_to_png(args.out)


def render_curve(args):
    width, height = 1200, 700
    ml, mr, mt, mb = 150, 40, 40, 120
    pw, ph = width - ml - mr, height - mt - mb

    d_min = args.drop_min
    if d_min <= 0:
        raise ValueError("drop-min must be > 0")

    samples = []
    with open(args.sample_file, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            samples.append(float(line))
    if len(samples) < 2:
        raise ValueError("sample-file must contain at least two numeric values")

    slide = bool(args.slide)
    n_step = int(args.n_step)
    step_len_sec = int(args.step_len_sec)

    y_min = min(samples + [args.beat_start, args.beat_target])
    y_max = max(samples + [args.beat_start, args.beat_target])
    if abs(y_max - y_min) < 1e-6:
        y_min -= 1.0
        y_max += 1.0

    y_pad = max((y_max - y_min) * 0.08, 0.1)
    y_min -= y_pad
    y_max += y_pad
    y_span = y_max - y_min

    y_tick_step = max(1.0, round(y_span / 8.0))
    y_tick_first = math.ceil((y_min - 1e-9) / y_tick_step) * y_tick_step
    y_ticks = []
    yv = y_tick_first
    while yv <= y_max + y_tick_step * 0.25:
        y_ticks.append(yv)
        yv += y_tick_step
    x_ticks = _build_integer_ticks(d_min)

    surface, ctx = _setup_canvas(width, height)

    def x_map(t):
        return ml + (pw - 1) * (t / d_min)

    def y_map(v):
        return mt + (y_max - v) * (ph - 1) / y_span

    _draw_grid_box(ctx, ml, mt, pw, ph, 10, y_ticks, y_map)

    # curve
    ctx.set_source_rgb(0.13, 0.37, 0.88)
    ctx.set_line_width(2.0)
    first = True
    n = len(samples)
    for i, y in enumerate(samples):
        t = d_min * i / (n - 1)
        px = x_map(t)
        py = y_map(y)
        if first:
            ctx.move_to(px, py)
            first = False
        else:
            ctx.line_to(px, py)
    ctx.stroke()

    # endpoint markers
    for t, y in ((0.0, samples[0]), (d_min, samples[-1])):
        px = x_map(t)
        py = y_map(y)
        ctx.set_source_rgb(0.86, 0.16, 0.16)
        ctx.arc(px, py, 2.8, 0.0, 2.0 * PI)
        ctx.fill()

    # tick marks + labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(16)
    ctx.set_source_rgb(0.15, 0.15, 0.15)

    for t in x_ticks:
        x = x_map(t)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.set_line_width(1.0)
        ctx.move_to(x, mt + ph - 1)
        ctx.line_to(x, mt + ph + 4)
        ctx.stroke()
        txt = _fmt_tick(t)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(x - ext.width / 2, mt + ph + 22)
        ctx.show_text(txt)

    for yv in y_ticks:
        y = y_map(yv)
        txt = _fmt_tick(yv)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.move_to(ml - 4, y)
        ctx.line_to(ml, y)
        ctx.stroke()
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(ml - 8 - ext.width, y + ext.height / 2)
        ctx.show_text(txt)

    # axis labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(18)
    x_label = "TIME MIN"
    y_label = "FREQ HZ"

    ext = ctx.text_extents(x_label)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 70)
    ctx.show_text(x_label)

    ctx.save()
    ctx.translate(38, mt + ph / 2)
    ctx.rotate(-PI / 2.0)
    ext = ctx.text_extents(y_label)
    ctx.move_to(-ext.width / 2, 0)
    ctx.show_text(y_label)
    ctx.restore()

    # parameter lines (bottom)
    mode_map = {0: "binaural beat", 1: "pulse", 2: "monaural beat"}
    mode_label = mode_map.get(int(args.mode_kind), "binaural beat")
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(14)
    line1 = (
        f"start={args.beat_start:.3f}Hz  target={args.beat_target:.3f}Hz  "
        f"D={args.drop_min:.1f}min"
    )
    if slide:
        line2 = f"{mode_label} mode: custom function curve (.sbgf), continuous (s)"
    else:
        line2 = (
            f"{mode_label} mode: sampled custom curve (k/default), "
            f"step={step_len_sec}s n={n_step}"
        )

    ext = ctx.text_extents(line1)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 40)
    ctx.show_text(line1)
    ext = ctx.text_extents(line2)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 18)
    ctx.show_text(line2)

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    surface.write_to_png(args.out)


def _load_iso_cycle_samples(path: str):
    ts = []
    env = []
    wave = []
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            cols = line.split()
            if len(cols) != 3:
                raise ValueError("iso-cycle sample-file must contain three numeric columns")
            ts.append(float(cols[0]))
            env.append(float(cols[1]))
            wave.append(float(cols[2]))
    if len(ts) < 2:
        raise ValueError("iso-cycle sample-file must contain at least two rows")
    return ts, env, wave


def _wave_sample(waveform: int, phase01: float) -> float:
    phase01 = phase01 - math.floor(phase01)
    phase = phase01 * 2.0 * PI
    if waveform == 1:
        return 1.0 if math.sin(phase) >= 0.0 else -1.0
    if waveform == 2:
        if phase < PI:
            return (2.0 * phase / PI) - 1.0
        return 3.0 - (2.0 * phase / PI)
    if waveform == 3:
        return (phase / PI) - 1.0
    return math.sin(phase)


def _iso_edge_shape(x: float, mode: int) -> float:
    if x <= 0.0:
        return 0.0
    if x >= 1.0:
        return 1.0
    if mode == 0:
        return 1.0 if x > 0.0 else 0.0
    if mode == 1:
        return x
    if mode == 3:
        return x * x * x * (x * (x * 6.0 - 15.0) + 10.0)
    return x * x * (3.0 - 2.0 * x)


def _iso_mod_custom(phase, start, duty, attack, release, edge_mode):
    phase = phase - math.floor(phase)
    if phase < 0.0:
        phase += 1.0
    if duty >= 1.0:
        return 1.0
    end = start + duty
    u = -1.0
    if end <= 1.0:
        if start <= phase < end:
            u = (phase - start) / duty
    else:
        if phase >= start:
            u = (phase - start) / duty
        elif phase < (end - 1.0):
            u = (phase + (1.0 - start)) / duty
    if u <= 0.0 or u >= 1.0:
        return 0.0
    if attack > 0.0 and u < attack:
        return _iso_edge_shape(u / attack, edge_mode)
    if u <= (1.0 - release):
        return 1.0
    if release > 0.0:
        return _iso_edge_shape((1.0 - u) / release, edge_mode)
    return 0.0


def render_mixam_cycle(args):
    width, height = 1500, 900
    ml, mr, mt, mb = 130, 40, 40, 160
    gap = 70
    pw = width - ml - mr
    panel_h = (height - mt - mb - gap) / 2.0
    top_y0 = mt
    bot_y0 = mt + panel_h + gap

    h_m = str(getattr(args, "h_m", "pulse") or "pulse").strip().lower()
    h_s = float(args.h_s)
    h_d = float(args.h_d)
    h_a = float(args.h_a)
    h_r = float(args.h_r)
    h_e = int(args.h_e)
    h_f = float(args.h_f)
    if h_m in ("0",):
        h_m = "pulse"
    elif h_m in ("1",):
        h_m = "cos"
    if h_m not in ("pulse", "cos"):
        raise ValueError("h-m must be pulse|cos (or 0|1)")

    surface, ctx = _setup_canvas(width, height)

    def x_map(u):
        return ml + (pw - 1) * u

    def y_map(v, y0):
        return y0 + (1.0 - v) * (panel_h - 1)

    y_ticks = [1.0 - 0.1 * i for i in range(11)]
    x_ticks = _build_linear_ticks(1.0, 10)
    _draw_grid_box(ctx, ml, top_y0, pw, panel_h, 10, y_ticks, lambda v: y_map(v, top_y0))
    _draw_grid_box(ctx, ml, bot_y0, pw, panel_h, 10, y_ticks, lambda v: y_map(v, bot_y0))

    # envelope line
    ctx.set_source_rgb(0.86, 0.16, 0.16)
    ctx.set_line_width(2.0)
    first = True
    samples = 8000
    for i in range(samples + 1):
        u = i / float(samples)
        if h_m == "cos":
            phase = (u + h_s) - math.floor(u + h_s)
            env = 0.5 * (1.0 + math.cos(2.0 * PI * phase))
        else:
            env = _iso_mod_custom(u, h_s, h_d, h_a, h_r, h_e)
        px = x_map(u)
        py = y_map(env, top_y0)
        if first:
            ctx.move_to(px, py)
            first = False
        else:
            ctx.line_to(px, py)
    ctx.stroke()

    # gain line
    ctx.set_source_rgb(0.13, 0.37, 0.88)
    ctx.set_line_width(2.0)
    first = True
    for i in range(samples + 1):
        u = i / float(samples)
        if h_m == "cos":
            phase = (u + h_s) - math.floor(u + h_s)
            env = 0.5 * (1.0 + math.cos(2.0 * PI * phase))
        else:
            env = _iso_mod_custom(u, h_s, h_d, h_a, h_r, h_e)
        gain = h_f + (1.0 - h_f) * env
        px = x_map(u)
        py = y_map(gain, bot_y0)
        if first:
            ctx.move_to(px, py)
            first = False
        else:
            ctx.line_to(px, py)
    ctx.stroke()

    # ticks + labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(16)
    ctx.set_source_rgb(0.15, 0.15, 0.15)

    for u in x_ticks:
        x = x_map(u)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.move_to(x, bot_y0 + panel_h - 1)
        ctx.line_to(x, bot_y0 + panel_h + 4)
        ctx.stroke()
        txt = _fmt_tick(u)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(x - ext.width / 2, bot_y0 + panel_h + 22)
        ctx.show_text(txt)

    for yv in y_ticks:
        for y0 in (top_y0, bot_y0):
            y = y_map(yv, y0)
            txt = _fmt_tick(yv)
            ext = ctx.text_extents(txt)
            ctx.set_source_rgb(0.35, 0.35, 0.35)
            ctx.move_to(ml - 4, y)
            ctx.line_to(ml, y)
            ctx.stroke()
            ctx.set_source_rgb(0.15, 0.15, 0.15)
            ctx.move_to(ml - 8 - ext.width, y + ext.height / 2)
            ctx.show_text(txt)

    # titles
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(17)
    if h_m == "cos":
        title = "MIXAM SINGLE-CYCLE CONTINUOUS-AM PLOT"
    else:
        title = "MIXAM SINGLE-CYCLE ENVELOPE PLOT"
    ext = ctx.text_extents(title)
    ctx.set_source_rgb(0.12, 0.12, 0.12)
    ctx.move_to(ml + (pw - ext.width) / 2, 28)
    ctx.show_text(title)

    x_label = "CYCLE"
    ext = ctx.text_extents(x_label)
    ctx.move_to(ml + (pw - ext.width) / 2, bot_y0 + panel_h + 56)
    ctx.show_text(x_label)

    for label, y0 in (("ENVELOPE", top_y0 + panel_h / 2), ("GAIN", bot_y0 + panel_h / 2)):
        ctx.save()
        ctx.translate(28, y0)
        ctx.rotate(-PI / 2.0)
        ext = ctx.text_extents(label)
        ctx.move_to(-ext.width / 2, 0)
        ctx.show_text(label)
        ctx.restore()

    # parameter lines
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(14)
    if h_m == "cos":
        line1 = f"H:m=cos s={h_s:.4f} f={h_f:.3f}"
    else:
        line1 = f"H:m=pulse s={h_s:.4f} d={h_d:.4f} a={h_a:.2f} r={h_r:.2f} e={h_e} f={h_f:.3f}"
    line2 = "mixam cycle gain = f + (1-f)*envelope"

    ext = ctx.text_extents(line1)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 40)
    ctx.show_text(line1)
    ext = ctx.text_extents(line2)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 18)
    ctx.show_text(line2)

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    surface.write_to_png(args.out)


def _iso_mod_legacy(phase, waveform):
    wave = _wave_sample(waveform, phase)
    threshold = 0.3
    if wave <= threshold:
        return 0.0
    mod = (wave - threshold) / (1.0 - threshold)
    return mod * mod * (3.0 - 2.0 * mod)


def render_iso_cycle(args):
    width, height = 1600, 980
    ml, mr, mt, mb = 130, 40, 40, 160
    gap = 70
    pw = width - ml - mr
    ph = (height - mt - mb - gap) / 2.0
    top_y0 = mt
    bot_y0 = mt + ph + gap

    if args.pulse_hz <= 0:
        raise ValueError("pulse-hz must be > 0")

    period_sec = 1.0 / args.pulse_hz
    waveform = int(args.waveform)
    waveform = waveform if 0 <= waveform <= 3 else 0
    amp = max(0.0, args.amp_pct / 100.0)
    sample_ts = sample_env = sample_wave = None
    if args.sample_file:
        sample_ts, sample_env, sample_wave = _load_iso_cycle_samples(args.sample_file)
        if sample_ts[-1] > 0:
            period_sec = sample_ts[-1]

    surface, ctx = _setup_canvas(width, height)

    def x_map(t):
        return ml + (pw - 1) * (t / period_sec)

    def y_env(v):
        return top_y0 + (1.0 - v) * (ph - 1)

    def y_wave(v):
        return bot_y0 + (1.0 - ((v + 1.0) * 0.5)) * (ph - 1)

    _draw_grid_box(ctx, ml, top_y0, pw, ph, 10, [1.0 - 0.1 * i for i in range(11)], y_env)
    _draw_grid_box(ctx, ml, bot_y0, pw, ph, 10, [1.0 - 0.2 * i for i in range(11)], y_wave)

    # envelope line
    ctx.set_source_rgb(0.86, 0.16, 0.16)
    ctx.set_line_width(2.0)
    first = True
    if sample_ts is not None:
        seq = zip(sample_ts, sample_env)
    else:
        samples = 8000
        seq = []
        for i in range(samples + 1):
            t = period_sec * i / samples
            pulse_phase = args.pulse_hz * t
            if args.opt_i:
                env = _iso_mod_custom(
                    pulse_phase, args.i_s, args.i_d, args.i_a, args.i_r, int(args.i_e)
                )
            else:
                env = _iso_mod_legacy(pulse_phase, waveform)
            seq.append((t, env))
    for t, env in seq:
        px = x_map(t)
        py = y_env(env)
        if first:
            ctx.move_to(px, py)
            first = False
        else:
            ctx.line_to(px, py)
    ctx.stroke()

    # waveform line
    ctx.set_source_rgb(0.13, 0.37, 0.88)
    ctx.set_line_width(1.4)
    first = True
    if sample_ts is not None:
        seq = zip(sample_ts, sample_wave)
    else:
        seq = []
        for i in range(samples + 1):
            t = period_sec * i / samples
            pulse_phase = args.pulse_hz * t
            carr_phase = args.carrier_hz * t
            if args.opt_i:
                env = _iso_mod_custom(
                    pulse_phase, args.i_s, args.i_d, args.i_a, args.i_r, int(args.i_e)
                )
            else:
                env = _iso_mod_legacy(pulse_phase, waveform)
            wav = _wave_sample(waveform, carr_phase) * env * amp
            seq.append((t, wav))
    for t, wav in seq:
        px = x_map(t)
        py = y_wave(wav)
        if first:
            ctx.move_to(px, py)
            first = False
        else:
            ctx.line_to(px, py)
    ctx.stroke()

    # ticks / labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(16)
    ctx.set_source_rgb(0.15, 0.15, 0.15)

    # x ticks
    for i in range(11):
        t = period_sec * i / 10.0
        x = x_map(t)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.move_to(x, bot_y0 + ph - 1)
        ctx.line_to(x, bot_y0 + ph + 4)
        ctx.stroke()
        txt = _fmt_tick(t)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(x - ext.width / 2, bot_y0 + ph + 22)
        ctx.show_text(txt)

    # y ticks (env)
    for i in range(11):
        v = 1.0 - 0.1 * i
        y = y_env(v)
        txt = _fmt_tick(v)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.move_to(ml - 4, y)
        ctx.line_to(ml, y)
        ctx.stroke()
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(ml - 8 - ext.width, y + ext.height / 2)
        ctx.show_text(txt)

    # y ticks (wave)
    for i in range(11):
        v = 1.0 - 0.2 * i
        y = y_wave(v)
        txt = _fmt_tick(v)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.move_to(ml - 4, y)
        ctx.line_to(ml, y)
        ctx.stroke()
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(ml - 8 - ext.width, y + ext.height / 2)
        ctx.show_text(txt)

    # titles
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(17)
    title = "ISOCHRONIC SINGLE-CYCLE PLOT"
    ext = ctx.text_extents(title)
    ctx.set_source_rgb(0.12, 0.12, 0.12)
    ctx.move_to(ml + (pw - ext.width) / 2, 28)
    ctx.show_text(title)

    x_label = "TIME SEC"
    ext = ctx.text_extents(x_label)
    # Keep the x-axis label clear of tick labels.
    ctx.move_to(ml + (pw - ext.width) / 2, bot_y0 + ph + 56)
    ctx.show_text(x_label)

    for label, y0 in (("ENVELOPE", top_y0 + ph / 2), ("WAVEFORM", bot_y0 + ph / 2)):
        ctx.save()
        ctx.translate(28, y0)
        ctx.rotate(-PI / 2.0)
        ext = ctx.text_extents(label)
        ctx.move_to(-ext.width / 2, 0)
        ctx.show_text(label)
        ctx.restore()

    # parameter lines
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(13)
    wave_name = ["sine", "square", "triangle", "sawtooth"][waveform]
    line1 = f"c={args.carrier_hz:.1f}Hz  p={args.pulse_hz:.2f}Hz  a={args.amp_pct:.1f}%  w={wave_name}"
    if args.line2:
        line2 = args.line2
    elif args.opt_i:
        line2 = (
            f"I:s={args.i_s:.4f} d={args.i_d:.4f} "
            f"a={args.i_a:.2f} r={args.i_r:.2f} e={int(args.i_e)}"
        )
    else:
        line2 = "I=default envelope"

    ext = ctx.text_extents(line1)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 54)
    ctx.show_text(line1)
    ext = ctx.text_extents(line2)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 28)
    ctx.show_text(line2)

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    surface.write_to_png(args.out)


def render_wave_lr(args):
    width, height = 1900, 1120
    ml, mr, mt, mb = 110, 40, 44, 160
    hgap, vgap = 70, 84
    pw = (width - ml - mr - hgap) / 2.0
    ph = (height - mt - mb - vgap) / 2.0

    carrier_hz = float(args.carrier_hz)
    beat_hz = float(args.beat_hz)
    beat_abs = abs(beat_hz)
    amp = max(0.0, args.amp_pct / 100.0)
    waveform = int(args.waveform)
    waveform = waveform if 0 <= waveform <= 3 else 0
    cycles = max(0.25, float(args.cycles))

    if beat_abs <= 0.0:
        raise ValueError("beat-hz must be non-zero")

    env_table = _build_wave_env_table(_parse_wave_samples(args.samples))
    table_size = len(env_table)
    duration_sec = cycles / beat_abs

    x0_l = ml
    x0_r = ml + pw + hgap
    y0_top = mt
    y0_bot = mt + ph + vgap

    freq_l = carrier_hz + beat_hz / 2.0
    freq_r = carrier_hz - beat_hz / 2.0

    surface, ctx = _setup_canvas(width, height)

    def x_map(x0, t):
        return x0 + (pw - 1) * (t / duration_sec)

    def y_env_map(y0, v):
        return y0 + (1.0 - v) * (ph - 1)

    def y_wave_map(y0, v):
        return y0 + (1.0 - ((v + 1.0) * 0.5)) * (ph - 1)

    env_ticks = [1.0 - 0.1 * i for i in range(11)]
    wave_ticks = [1.0, 0.5, 0.0, -0.5, -1.0]
    x_ticks = _build_linear_ticks(duration_sec, 10)

    _draw_grid_box(ctx, x0_l, y0_top, pw, ph, 10, env_ticks, lambda v: y_env_map(y0_top, v))
    _draw_grid_box(ctx, x0_r, y0_top, pw, ph, 10, env_ticks, lambda v: y_env_map(y0_top, v))
    _draw_grid_box(ctx, x0_l, y0_bot, pw, ph, 10, wave_ticks, lambda v: y_wave_map(y0_bot, v))
    _draw_grid_box(ctx, x0_r, y0_bot, pw, ph, 10, wave_ticks, lambda v: y_wave_map(y0_bot, v))

    def env_at(t):
        phase = (beat_abs * t) % 1.0
        idx = int(phase * table_size) & (table_size - 1)
        return env_table[idx]

    def plot_panel(x0, y0, y_map_fn, fn, color, line_w):
        ctx.set_source_rgb(*color)
        ctx.set_line_width(line_w)
        first = True
        n = 12000
        for i in range(n + 1):
            t = duration_sec * i / n
            y = fn(t)
            px = x_map(x0, t)
            py = y_map_fn(y0, y)
            if first:
                ctx.move_to(px, py)
                first = False
            else:
                ctx.line_to(px, py)
        ctx.stroke()

    # Envelopes (same shape in both channels)
    plot_panel(x0_l, y0_top, y_env_map, lambda t: env_at(t), (0.86, 0.16, 0.16), 2.0)
    plot_panel(x0_r, y0_top, y_env_map, lambda t: env_at(t), (0.86, 0.16, 0.16), 2.0)

    # Waveforms
    plot_panel(
        x0_l,
        y0_bot,
        y_wave_map,
        lambda t: _wave_sample(waveform, freq_l * t) * env_at(t) * amp,
        (0.13, 0.37, 0.88),
        1.5,
    )
    plot_panel(
        x0_r,
        y0_bot,
        y_wave_map,
        lambda t: _wave_sample(waveform, freq_r * t) * env_at(t) * amp,
        (0.13, 0.37, 0.88),
        1.5,
    )

    # Titles
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.set_source_rgb(0.12, 0.12, 0.12)
    ctx.set_font_size(22)
    title = args.title
    ext = ctx.text_extents(title)
    ctx.move_to(ml + (width - ml - mr - ext.width) / 2.0, 30)
    ctx.show_text(title)

    ctx.set_font_size(17)
    for txt, x0, y0 in (
        ("LEFT CHANNEL ENVELOPE", x0_l, y0_top),
        ("RIGHT CHANNEL ENVELOPE", x0_r, y0_top),
        ("LEFT CHANNEL WAVEFORM", x0_l, y0_bot),
        ("RIGHT CHANNEL WAVEFORM", x0_r, y0_bot),
    ):
        ext = ctx.text_extents(txt)
        ctx.move_to(x0 + (pw - ext.width) / 2.0, y0 - 12)
        ctx.show_text(txt)

    # Tick labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(14)

    def draw_y_ticks(x0, y0, ticks, map_fn):
        for yv in ticks:
            y = map_fn(y0, yv)
            txt = _fmt_tick(yv)
            ext = ctx.text_extents(txt)
            ctx.set_source_rgb(0.35, 0.35, 0.35)
            ctx.move_to(x0 - 4, y)
            ctx.line_to(x0, y)
            ctx.stroke()
            ctx.set_source_rgb(0.15, 0.15, 0.15)
            ctx.move_to(x0 - 8 - ext.width, y + ext.height / 2)
            ctx.show_text(txt)

    draw_y_ticks(x0_l, y0_top, env_ticks, y_env_map)
    draw_y_ticks(x0_r, y0_top, env_ticks, y_env_map)
    draw_y_ticks(x0_l, y0_bot, wave_ticks, y_wave_map)
    draw_y_ticks(x0_r, y0_bot, wave_ticks, y_wave_map)

    # X ticks on bottom row only (both columns)
    for x0 in (x0_l, x0_r):
        for t in x_ticks:
            x = x_map(x0, t)
            ctx.set_source_rgb(0.35, 0.35, 0.35)
            ctx.move_to(x, y0_bot + ph - 1)
            ctx.line_to(x, y0_bot + ph + 4)
            ctx.stroke()
            txt = _fmt_tick(t)
            ext = ctx.text_extents(txt)
            ctx.set_source_rgb(0.15, 0.15, 0.15)
            ctx.move_to(x - ext.width / 2, y0_bot + ph + 22)
            ctx.show_text(txt)

    # Axis labels
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(18)
    x_label = "TIME SEC"
    for x0 in (x0_l, x0_r):
        ext = ctx.text_extents(x_label)
        ctx.move_to(x0 + (pw - ext.width) / 2.0, y0_bot + ph + 54)
        ctx.show_text(x_label)

    for txt, x0, y0 in (
        ("ENVELOPE", x0_l, y0_top),
        ("ENVELOPE", x0_r, y0_top),
        ("WAVEFORM", x0_l, y0_bot),
        ("WAVEFORM", x0_r, y0_bot),
    ):
        ctx.save()
        ctx.translate(x0 - 74, y0 + ph / 2.0)
        ctx.rotate(-PI / 2.0)
        ext = ctx.text_extents(txt)
        ctx.move_to(-ext.width / 2.0, 0)
        ctx.show_text(txt)
        ctx.restore()

    # Bottom parameter lines
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(14)
    wave_name = ["sine", "square", "triangle", "sawtooth"][waveform]
    line1 = (
        f"carrier={carrier_hz:.3f}Hz  beat={beat_hz:.3f}Hz  amp={args.amp_pct:.1f}%  "
        f"waveform={wave_name}  duration={duration_sec:.4f}s ({cycles:.2f} beat cycles)"
    )
    sample_list = _parse_wave_samples(args.samples)
    sample_text = ", ".join(_fmt_tick(v) for v in sample_list[:16])
    if len(sample_list) > 16:
        sample_text += ", ..."
    line2 = f"waveNN samples ({len(sample_list)}): {sample_text}"

    ext = ctx.text_extents(line1)
    ctx.move_to(ml + (width - ml - mr - ext.width) / 2.0, height - 52)
    ctx.show_text(line1)
    ext = ctx.text_extents(line2)
    ctx.move_to(ml + (width - ml - mr - ext.width) / 2.0, height - 26)
    ctx.show_text(line2)

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    surface.write_to_png(args.out)


def main():
    parser = argparse.ArgumentParser(description="SBaGenX Cairo plot backend")
    sub = parser.add_subparsers(dest="mode", required=True)

    sp = sub.add_parser("sigmoid", help="Render sigmoid curve plot")
    sp.add_argument("--out", required=True)
    sp.add_argument("--drop-min", type=float, required=True)
    sp.add_argument("--beat-start", type=float, required=True)
    sp.add_argument("--beat-target", type=float, required=True)
    sp.add_argument("--sig-l", type=float, required=True)
    sp.add_argument("--sig-h", type=float, required=True)
    sp.add_argument("--sig-a", type=float, required=True)
    sp.add_argument("--sig-b", type=float, required=True)

    dp = sub.add_parser("drop", help="Render drop curve plot")
    dp.add_argument("--out", required=True)
    dp.add_argument("--drop-min", type=float, required=True)
    dp.add_argument("--beat-start", type=float, required=True)
    dp.add_argument("--beat-target", type=float, required=True)
    dp.add_argument("--slide", type=int, required=True)
    dp.add_argument("--n-step", type=int, required=True)
    dp.add_argument("--step-len-sec", type=int, required=True)
    dp.add_argument("--mode-kind", type=int, required=True)

    cp = sub.add_parser("curve", help="Render custom .sbgf beat/pulse curve plot")
    cp.add_argument("--out", required=True)
    cp.add_argument("--drop-min", type=float, required=True)
    cp.add_argument("--beat-start", type=float, required=True)
    cp.add_argument("--beat-target", type=float, required=True)
    cp.add_argument("--mode-kind", type=int, required=True)
    cp.add_argument("--slide", type=int, required=True)
    cp.add_argument("--n-step", type=int, required=True)
    cp.add_argument("--step-len-sec", type=int, required=True)
    cp.add_argument("--sample-file", required=True)

    ip = sub.add_parser("iso-cycle", help="Render isochronic single-cycle plot")
    ip.add_argument("--out", required=True)
    ip.add_argument("--carrier-hz", type=float, required=True)
    ip.add_argument("--pulse-hz", type=float, required=True)
    ip.add_argument("--amp-pct", type=float, required=True)
    ip.add_argument("--waveform", type=int, required=True)
    ip.add_argument("--opt-i", type=int, required=True)
    ip.add_argument("--i-s", type=float, required=True)
    ip.add_argument("--i-d", type=float, required=True)
    ip.add_argument("--i-a", type=float, required=True)
    ip.add_argument("--i-r", type=float, required=True)
    ip.add_argument("--i-e", type=int, required=True)
    ip.add_argument("--sample-file")
    ip.add_argument("--line2")

    mp = sub.add_parser("mixam-cycle", help="Render mixam single-cycle envelope/gain plot")
    mp.add_argument("--out", required=True)
    mp.add_argument("--h-m", type=str, default="pulse")
    mp.add_argument("--h-s", type=float, required=True)
    mp.add_argument("--h-d", type=float, required=True)
    mp.add_argument("--h-a", type=float, required=True)
    mp.add_argument("--h-r", type=float, required=True)
    mp.add_argument("--h-e", type=int, required=True)
    mp.add_argument("--h-f", type=float, required=True)

    wp = sub.add_parser("wave-lr", help="Render custom waveNN left/right plot")
    wp.add_argument("--out", required=True)
    wp.add_argument("--carrier-hz", type=float, required=True)
    wp.add_argument("--beat-hz", type=float, required=True)
    wp.add_argument("--amp-pct", type=float, default=100.0)
    wp.add_argument("--waveform", type=int, default=0)
    wp.add_argument("--samples", required=True, help="WaveNN samples, comma or space separated")
    wp.add_argument("--cycles", type=float, default=1.0, help="Beat cycles shown on x-axis")
    wp.add_argument("--title", default="CUSTOM WAVENN LEFT/RIGHT PLOT")

    args = parser.parse_args()
    if args.mode == "sigmoid":
        render_sigmoid(args)
    elif args.mode == "drop":
        render_drop(args)
    elif args.mode == "curve":
        render_curve(args)
    elif args.mode == "iso-cycle":
        render_iso_cycle(args)
    elif args.mode == "mixam-cycle":
        render_mixam_cycle(args)
    elif args.mode == "wave-lr":
        render_wave_lr(args)
    else:
        raise ValueError("Unknown mode")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        sys.stderr.write(f"Plot backend failed: {exc}\n")
        sys.exit(1)
