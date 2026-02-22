#!/usr/bin/env python3
"""
High-quality SBaGenX plot renderer (Python + Cairo).

Modes:
  - sigmoid:   beat/pulse curve used by -G
  - drop:      beat/pulse curve used by -P with -p drop
  - iso-cycle: one-cycle isochronic envelope + waveform used by -P
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


def _sigmoid_eval(t_min, d_min, beat_target, sig_l, sig_h, sig_a, sig_b):
    if t_min >= d_min:
        return beat_target
    return sig_a * math.tanh(sig_l * (t_min - d_min / 2.0 - sig_h)) + sig_b


def _drop_eval(t_min, d_min, beat_target, slide, n_step, step_len_sec):
    if d_min <= 0:
        return beat_target
    t_min = min(max(t_min, 0.0), d_min)

    if not slide and n_step > 1 and step_len_sec > 0:
        idx = int((t_min * 60.0) / step_len_sec)
        idx = max(0, min(idx, n_step - 1))
        return 10.0 * math.exp(math.log(beat_target / 10.0) * idx / (n_step - 1))

    return 10.0 * math.exp(math.log(beat_target / 10.0) * (t_min / d_min))


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
        y = _drop_eval(t, d_min, args.beat_target, slide, n_step, step_len_sec)
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
        y = _drop_eval(t, d_min, args.beat_target, slide, n_step, step_len_sec)
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
        y = _drop_eval(t, d_min, args.beat_target, slide, n_step, step_len_sec)
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
    samples = 8000
    for i in range(samples + 1):
        t = period_sec * i / samples
        pulse_phase = args.pulse_hz * t
        if args.opt_i:
            env = _iso_mod_custom(
                pulse_phase, args.i_s, args.i_d, args.i_a, args.i_r, int(args.i_e)
            )
        else:
            env = _iso_mod_legacy(pulse_phase, waveform)
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
    if args.opt_i:
        line2 = (
            f"I:s={args.i_s:.4f} d={args.i_d:.4f} "
            f"a={args.i_a:.2f} r={args.i_r:.2f} e={int(args.i_e)}"
        )
    else:
        line2 = "I=default threshold gate"

    ext = ctx.text_extents(line1)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 54)
    ctx.show_text(line1)
    ext = ctx.text_extents(line2)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 28)
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

    args = parser.parse_args()
    if args.mode == "sigmoid":
        render_sigmoid(args)
    elif args.mode == "drop":
        render_drop(args)
    elif args.mode == "iso-cycle":
        render_iso_cycle(args)
    else:
        raise ValueError("Unknown mode")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        sys.stderr.write(f"Plot backend failed: {exc}\n")
        sys.exit(1)
