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
import shutil
import subprocess
import sys

try:
    import cairo
except Exception as exc:
    sys.stderr.write(f"Python/Cairo backend unavailable: {exc}\n")
    sys.exit(2)


PI = math.pi
_GRAPH_VIDEO_ENCODER_ARGS = None


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


def _add_graph_video_args(parser):
    parser.add_argument(
        "--video-out",
        help="Optional MP4 graph video output with a real-time tracking cursor",
    )
    parser.add_argument(
        "--video-fps",
        type=int,
        default=30,
        help="Frame rate for optional graph video output (default 30)",
    )
    parser.add_argument(
        "--audio-file",
        help="Optional audio file to mux into the MP4 graph video",
    )


def _draw_graph_cursor(ctx, x, y, plot_top, plot_bottom, pulse):
    halo_radius = 12.0 + 8.0 * pulse
    dot_radius = 5.5 + 2.5 * pulse
    halo_alpha = 0.12 + 0.16 * pulse

    ctx.set_source_rgba(0.05, 0.28, 0.76, 0.28)
    ctx.set_line_width(2.0)
    ctx.move_to(x, plot_top)
    ctx.line_to(x, plot_bottom)
    ctx.stroke()

    ctx.set_source_rgba(0.88, 0.28, 0.14, halo_alpha)
    ctx.arc(x, y, halo_radius, 0.0, 2.0 * PI)
    ctx.fill()

    ctx.set_source_rgb(0.88, 0.28, 0.14)
    ctx.arc(x, y, dot_radius, 0.0, 2.0 * PI)
    ctx.fill_preserve()
    ctx.set_source_rgb(1.0, 1.0, 1.0)
    ctx.set_line_width(2.0)
    ctx.stroke()


def _graph_video_encoder_args(ffmpeg_path: str):
    global _GRAPH_VIDEO_ENCODER_ARGS
    if _GRAPH_VIDEO_ENCODER_ARGS is not None:
        return list(_GRAPH_VIDEO_ENCODER_ARGS)

    encoders = ""
    try:
        probe = subprocess.run(
            [ffmpeg_path, "-hide_banner", "-encoders"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        encoders = probe.stdout or ""
    except Exception:
        encoders = ""

    if "libx264" not in encoders:
        raise RuntimeError(
            "graph video export requires FFmpeg with libx264 support; "
            "please install FFmpeg with libx264 enabled and ensure "
            "'ffmpeg' is available on PATH"
        )

    _GRAPH_VIDEO_ENCODER_ARGS = [
        "-c:v",
        "libx264",
        "-preset",
        "slow",
        "-tune",
        "animation",
        "-crf",
        "12",
        "-pix_fmt",
        "yuv444p",
    ]

    return list(_GRAPH_VIDEO_ENCODER_ARGS)


def _maybe_write_graph_video(
    base_surface,
    video_out: str,
    video_fps: int,
    duration_sec: float,
    width: int,
    height: int,
    plot_left: float,
    plot_top: float,
    plot_width: float,
    plot_height: float,
    y_pixels,
    freqs,
    audio_file: str = None,
):
    if not video_out:
        return
    if os.path.splitext(video_out)[1].lower() != ".mp4":
        raise ValueError("graph video output currently requires an .mp4 filename")
    if video_fps <= 0:
        raise ValueError("graph video fps must be > 0")
    if duration_sec <= 0.0:
        raise ValueError("graph video duration must be > 0")
    if len(y_pixels) < 2 or len(freqs) < 2 or len(y_pixels) != len(freqs):
        raise ValueError("graph video needs matching sampled y/frequency arrays")

    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg not found in PATH; graph video export requires ffmpeg")

    frame_count = max(2, int(round(duration_sec * video_fps)) + 1)
    video_args = _graph_video_encoder_args(ffmpeg)
    cmd = [
        ffmpeg,
        "-hide_banner",
        "-loglevel",
        "error",
        "-nostats",
        "-y",
        "-f",
        "rawvideo",
        "-pix_fmt",
        "bgr0",
        "-s",
        f"{width}x{height}",
        "-r",
        str(video_fps),
        "-i",
        "-",
    ]
    if audio_file:
        cmd.extend([
            "-i",
            audio_file,
            "-map",
            "0:v:0",
            "-map",
            "1:a:0",
            "-c:a",
            "alac",
            "-shortest",
        ])
    else:
        cmd.append("-an")
    cmd.extend(video_args)
    cmd.extend(["-movflags", "+faststart", video_out])

    os.makedirs(os.path.dirname(video_out) or ".", exist_ok=True)
    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    plot_bottom = plot_top + plot_height - 1
    base_surface.flush()
    frame_surface = cairo.ImageSurface(cairo.FORMAT_RGB24, width, height)
    frame_ctx = cairo.Context(frame_surface)
    frame_ctx.set_antialias(cairo.ANTIALIAS_BEST)

    beat_phase = 0.0
    prev_t = 0.0

    def interp(seq, pos):
        if pos <= 0.0:
            return seq[0]
        if pos >= len(seq) - 1:
            return seq[-1]
        i0 = int(math.floor(pos))
        i1 = i0 + 1
        frac = pos - i0
        return seq[i0] + (seq[i1] - seq[i0]) * frac

    try:
        assert proc.stdin is not None
        for frame_idx in range(frame_count):
            t_sec = duration_sec * frame_idx / float(frame_count - 1)
            progress = t_sec / duration_sec
            pos = progress * (len(y_pixels) - 1)
            x = plot_left + progress * (plot_width - 1)
            y = interp(y_pixels, pos)
            freq = max(0.0, interp(freqs, pos))
            dt_sec = max(0.0, t_sec - prev_t)
            prev_t = t_sec
            beat_phase += freq * dt_sec * 2.0 * PI
            pulse = 0.5 + 0.5 * math.sin(beat_phase)

            frame_ctx.set_source_surface(base_surface, 0, 0)
            frame_ctx.paint()
            _draw_graph_cursor(frame_ctx, x, y, plot_top, plot_bottom, pulse)
            frame_ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
            frame_ctx.set_font_size(14)
            frame_ctx.set_source_rgba(0.10, 0.13, 0.18, 0.88)
            label = f"t={t_sec:0.1f}s  beat={freq:0.3f}Hz"
            ext = frame_ctx.text_extents(label)
            pad = 10.0
            box_w = ext.width + 2.0 * pad
            box_h = ext.height + 2.0 * pad
            box_x = width - box_w - 18.0
            box_y = 16.0
            frame_ctx.set_source_rgba(1.0, 1.0, 1.0, 0.85)
            frame_ctx.rectangle(box_x, box_y, box_w, box_h)
            frame_ctx.fill()
            frame_ctx.set_source_rgba(0.10, 0.13, 0.18, 0.90)
            frame_ctx.move_to(box_x + pad, box_y + pad + ext.height)
            frame_ctx.show_text(label)
            frame_surface.flush()
            try:
                proc.stdin.write(frame_surface.get_data())
            except BrokenPipeError:
                stderr = (
                    proc.stderr.read().decode("utf-8", "replace")
                    if proc.stderr
                    else ""
                )
                rc = proc.wait()
                raise RuntimeError(
                    "ffmpeg failed while encoding graph video"
                    + (f": {stderr.strip()}" if stderr.strip() else "")
                    + f" (rc={rc})"
                ) from None
        proc.stdin.close()
        proc.stdin = None
        stderr = proc.stderr.read().decode("utf-8", "replace") if proc.stderr else ""
        rc = proc.wait()
        if rc != 0 or not os.path.exists(video_out):
            raise RuntimeError(
                "ffmpeg failed to encode graph video"
                + (f": {stderr.strip()}" if stderr.strip() else "")
            )
    finally:
        if proc.stdin is not None:
            proc.stdin.close()
        if proc.poll() is None:
            proc.kill()
            proc.wait()


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


def _parse_tick_list(text: str):
    text = (text or "").strip()
    if not text:
        return []
    return [float(item) for item in text.split(",") if item.strip()]


def _load_desc_file(path: str):
    out = {}
    with open(path, "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if not line or "=" not in line:
                continue
            key, value = line.split("=", 1)
            out[key.strip()] = value
    return out


def _load_scalar_samples(path: str):
    values = []
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            values.append(float(line))
    if len(values) < 2:
        raise ValueError("sample-file must contain at least two numeric values")
    return values


def _load_triplet_samples(path: str, label: str):
    xs = []
    a_vals = []
    b_vals = []
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            cols = line.split()
            if len(cols) != 3:
                raise ValueError(f"{label} sample-file must contain three numeric columns")
            xs.append(float(cols[0]))
            a_vals.append(float(cols[1]))
            b_vals.append(float(cols[2]))
    if len(xs) < 2:
        raise ValueError(f"{label} sample-file must contain at least two rows")
    return xs, a_vals, b_vals


def _render_program_plot(args):
    width, height = 1200, 700
    ml, mr, mt, mb = 150, 40, 40, 120
    pw, ph = width - ml - mr, height - mt - mb

    samples = _load_scalar_samples(args.sample_file)
    desc = _load_desc_file(args.desc_file)
    x_min = float(desc["x_min"])
    x_max = float(desc["x_max"])
    y_min = float(desc["y_min"])
    y_max = float(desc["y_max"])
    x_ticks = _parse_tick_list(desc.get("x_ticks", ""))
    y_ticks = _parse_tick_list(desc.get("y_ticks", ""))
    x_label = desc.get("x_label", "TIME MIN")
    y_label = desc.get("y_label", "FREQ HZ")
    title = desc.get("title", "")
    line1 = desc.get("line1", "")
    line2 = desc.get("line2", "")
    x_span = x_max - x_min
    y_span = y_max - y_min
    if x_span <= 0.0 or y_span <= 0.0:
        raise ValueError("program plot description has invalid axis ranges")

    surface, ctx = _setup_canvas(width, height)

    def x_map(t):
        return ml + (pw - 1) * ((t - x_min) / x_span)

    def y_map(v):
        return mt + (y_max - v) * (ph - 1) / y_span

    _draw_grid_box(ctx, ml, mt, pw, ph, 10, y_ticks, y_map)

    curve_vals = []
    y_pixels = []
    ctx.set_source_rgb(0.13, 0.37, 0.88)
    ctx.set_line_width(2.0)
    first = True
    n = len(samples)
    for i, y in enumerate(samples):
        t = x_min + x_span * i / (n - 1)
        curve_vals.append(y)
        px = x_map(t)
        py = y_map(y)
        y_pixels.append(py)
        if first:
            ctx.move_to(px, py)
            first = False
        else:
            ctx.line_to(px, py)
    ctx.stroke()

    for t, y in ((x_min, samples[0]), (x_max, samples[-1])):
        px = x_map(t)
        py = y_map(y)
        ctx.set_source_rgb(0.86, 0.16, 0.16)
        ctx.arc(px, py, 2.8, 0.0, 2.0 * PI)
        ctx.fill()

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

    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
    ctx.set_font_size(18)
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

    if title:
        ctx.set_font_size(17)
        ext = ctx.text_extents(title)
        ctx.move_to(ml + (pw - ext.width) / 2, 28)
        ctx.show_text(title)

    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(14)
    ext = ctx.text_extents(line1)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 40)
    ctx.show_text(line1)
    ext = ctx.text_extents(line2)
    ctx.move_to(ml + (pw - ext.width) / 2, height - 18)
    ctx.show_text(line2)

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    surface.write_to_png(args.out)
    _maybe_write_graph_video(
        surface,
        args.video_out,
        args.video_fps,
        x_span * 60.0,
        width,
        height,
        ml,
        mt,
        pw,
        ph,
        y_pixels,
        curve_vals,
        args.audio_file,
    )


def render_sigmoid(args):
    _render_program_plot(args)


def render_drop(args):
    _render_program_plot(args)


def render_curve(args):
    _render_program_plot(args)


def _load_iso_cycle_samples(path: str):
    return _load_triplet_samples(path, "iso-cycle")


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
    desc = _load_desc_file(args.desc_file)
    ts, env_vals, gain_vals = _load_triplet_samples(args.sample_file, "mixam-cycle")
    x_min = float(desc["x_min"])
    x_max = float(desc["x_max"])
    top_y_min = float(desc["top_y_min"])
    top_y_max = float(desc["top_y_max"])
    bottom_y_min = float(desc["bottom_y_min"])
    bottom_y_max = float(desc["bottom_y_max"])
    x_ticks = _parse_tick_list(desc.get("x_ticks", ""))
    top_y_ticks = _parse_tick_list(desc.get("top_y_ticks", ""))
    bottom_y_ticks = _parse_tick_list(desc.get("bottom_y_ticks", ""))
    x_label = desc.get("x_label", "CYCLE")
    top_y_label = desc.get("top_y_label", "ENVELOPE")
    bottom_y_label = desc.get("bottom_y_label", "GAIN")
    title = desc.get("title", "")
    line1 = desc.get("line1", "")
    line2 = desc.get("line2", "")
    x_span = x_max - x_min
    top_y_span = top_y_max - top_y_min
    bottom_y_span = bottom_y_max - bottom_y_min
    if x_span <= 0.0 or top_y_span <= 0.0 or bottom_y_span <= 0.0:
        raise ValueError("mixam-cycle description has invalid axis ranges")

    surface, ctx = _setup_canvas(width, height)

    def x_map(t):
        return ml + (pw - 1) * ((t - x_min) / x_span)

    def y_top(v):
        return top_y0 + (top_y_max - v) * (panel_h - 1) / top_y_span

    def y_bottom(v):
        return bot_y0 + (bottom_y_max - v) * (panel_h - 1) / bottom_y_span

    x_divs = max(1, len(x_ticks) - 1)
    _draw_grid_box(ctx, ml, top_y0, pw, panel_h, x_divs, top_y_ticks, y_top)
    _draw_grid_box(ctx, ml, bot_y0, pw, panel_h, x_divs, bottom_y_ticks, y_bottom)

    # envelope line
    ctx.set_source_rgb(0.86, 0.16, 0.16)
    ctx.set_line_width(2.0)
    first = True
    for t, env in zip(ts, env_vals):
        px = x_map(t)
        py = y_top(env)
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
    for t, gain in zip(ts, gain_vals):
        px = x_map(t)
        py = y_bottom(gain)
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

    for t in x_ticks:
        x = x_map(t)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.move_to(x, bot_y0 + panel_h - 1)
        ctx.line_to(x, bot_y0 + panel_h + 4)
        ctx.stroke()
        txt = _fmt_tick(t)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(x - ext.width / 2, bot_y0 + panel_h + 22)
        ctx.show_text(txt)

    for yv in top_y_ticks:
        y = y_top(yv)
        txt = _fmt_tick(yv)
        ext = ctx.text_extents(txt)
        ctx.set_source_rgb(0.35, 0.35, 0.35)
        ctx.move_to(ml - 4, y)
        ctx.line_to(ml, y)
        ctx.stroke()
        ctx.set_source_rgb(0.15, 0.15, 0.15)
        ctx.move_to(ml - 8 - ext.width, y + ext.height / 2)
        ctx.show_text(txt)

    for yv in bottom_y_ticks:
        y = y_bottom(yv)
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
    if title:
        ext = ctx.text_extents(title)
        ctx.set_source_rgb(0.12, 0.12, 0.12)
        ctx.move_to(ml + (pw - ext.width) / 2, 28)
        ctx.show_text(title)

    ext = ctx.text_extents(x_label)
    ctx.move_to(ml + (pw - ext.width) / 2, bot_y0 + panel_h + 56)
    ctx.show_text(x_label)

    for label, y0 in ((top_y_label, top_y0 + panel_h / 2), (bottom_y_label, bot_y0 + panel_h / 2)):
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
    sp.add_argument("--sample-file", required=True)
    sp.add_argument("--desc-file", required=True)
    _add_graph_video_args(sp)

    dp = sub.add_parser("drop", help="Render drop curve plot")
    dp.add_argument("--out", required=True)
    dp.add_argument("--sample-file", required=True)
    dp.add_argument("--desc-file", required=True)
    _add_graph_video_args(dp)

    cp = sub.add_parser("curve", help="Render custom .sbgf beat/pulse curve plot")
    cp.add_argument("--out", required=True)
    cp.add_argument("--sample-file", required=True)
    cp.add_argument("--desc-file", required=True)
    _add_graph_video_args(cp)

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
    mp.add_argument("--sample-file", required=True)
    mp.add_argument("--desc-file", required=True)

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
