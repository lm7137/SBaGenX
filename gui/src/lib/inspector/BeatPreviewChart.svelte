<script lang="ts">
  import type { BeatPreviewResult } from '../types'

  export let preview: BeatPreviewResult | null = null
  export let error: string | null = null
  export let kind: 'sbg' | 'sbgf' | 'curve-program' | 'built-in-program' | null = null
  export let validating = false

  const width = 560
  const height = 180
  const padLeft = 34
  const padRight = 10
  const padTop = 10
  const padBottom = 28

  function chooseTickStep(target: number, candidates: number[], preferred: number[] = []): number {
    let best = candidates[0]
    let bestScore =
      Math.abs(best - target) * (preferred.includes(best) ? 0.85 : 1)
    for (const candidate of candidates.slice(1)) {
      const score =
        Math.abs(candidate - target) * (preferred.includes(candidate) ? 0.85 : 1)
      if (score < bestScore) {
        best = candidate
        bestScore = score
      }
    }
    return best
  }

  function formatHz(value: number): string {
    return value >= 10 ? value.toFixed(1) : value.toFixed(2)
  }

  function formatTickHz(value: number): string {
    return Number(value.toFixed(2)).toString()
  }

  function formatHzNullable(value: number | null): string {
    return value === null ? '—' : formatHz(value)
  }

  function formatDuration(seconds: number): string {
    if (seconds >= 60) {
      const mins = Math.floor(seconds / 60)
      const secs = Math.round(seconds % 60)
      return `${mins}m ${secs}s`
    }
    return `${seconds.toFixed(seconds >= 10 ? 0 : 1)}s`
  }

  function formatTimeTick(seconds: number): string {
    if (!preview) return ''
    if (preview.timeLabel === 'TIME MIN') {
      const minutes = seconds / 60
      return Number.isInteger(minutes) ? `${minutes}` : minutes.toFixed(1)
    }
    return seconds >= 10 ? `${Math.round(seconds)}` : seconds.toFixed(1)
  }

  function buildYTicks(minValue: number, maxValue: number) {
    const span = Math.max(maxValue - minValue, 0.001)
    const step = Math.max(0.5, Math.ceil((span / 5) * 2) / 2)
    let axisMin = Math.floor(minValue / step) * step
    let axisMax = Math.ceil(maxValue / step) * step
    if (axisMin === axisMax) {
      axisMax = axisMin + step
    }

    const ticks: number[] = []
    for (let value = axisMin; value <= axisMax + step * 0.5; value += step) {
      ticks.push(Number(value.toFixed(6)))
    }

    return {
      axisMin,
      axisMax,
      ticks,
    }
  }

  function buildXTicks(durationSec: number) {
    const ticks: number[] = []
    if (durationSec <= 0) {
      return ticks
    }

    const useMinutes = preview?.timeLabel === 'TIME MIN'
    const targetStep = useMinutes ? durationSec / 60 / 5 : durationSec / 5
    const step = useMinutes
      ? chooseTickStep(targetStep, [1, 2, 5, 10, 15, 20, 30, 60], [5, 10]) * 60
      : chooseTickStep(targetStep, [0.5, 1, 2, 5, 10, 15, 30])

    for (let value = 0; value <= durationSec + step * 0.25; value += step) {
      ticks.push(Number(value.toFixed(6)))
    }

    if (ticks[ticks.length - 1] !== durationSec) {
      ticks.push(durationSec)
    }

    return ticks
  }

  function xFor(tSec: number) {
    if (!preview) return padLeft
    const usableWidth = width - padLeft - padRight
    const duration = Math.max(preview.durationSec, 0.001)
    return padLeft + (tSec / duration) * usableWidth
  }

  function yFor(value: number, minValue: number, maxValue: number) {
    const usableHeight = height - padTop - padBottom
    const span = Math.max(maxValue - minValue, 0.001)
    return padTop + usableHeight - ((value - minValue) / span) * usableHeight
  }

  function chartPath(points: BeatPreviewResult['series'][number]['points']) {
    if (!preview || points.length === 0) return ''

    const minHz = yAxis.axisMin
    const maxHz = yAxis.axisMax

    return points
      .map((point, index) => {
        if (point.beatHz === null) {
          return ''
        }
        const x = xFor(point.tSec)
        const y = yFor(point.beatHz, minHz, maxHz)
        const prev = points[index - 1]
        const cmd = index === 0 || !prev || prev.beatHz === null ? 'M' : 'L'
        return `${cmd}${x.toFixed(2)},${y.toFixed(2)}`
      })
      .filter(Boolean)
      .join(' ')
  }

  const lineColors = ['#3a7cff', '#ff2ea6', '#00a47a', '#7b61ff']

  $: yAxis = buildYTicks(preview?.minHz ?? 0, preview?.maxHz ?? 1)
  $: xTicks = buildXTicks(preview?.durationSec ?? 0)

  $: seriesPaths =
    preview?.series.map((series, index) => ({
      key: `${series.voiceIndex}-${index}`,
      label: series.label,
      color: lineColors[index % lineColors.length],
      path: chartPath(series.points),
      activeSampleCount: series.activeSampleCount,
    })) ?? []
</script>

<div class="inspector-block preview-block">
  <div class="preview-header">
    <div>
      <p class="panel-title">Beat Preview</p>
      <p class="panel-note">
        {#if kind === 'curve-program'}
          Sampled directly from `sbagenxlib` for the active built-in `curve` program.
        {:else if kind === 'built-in-program'}
          Sampled directly from `sbagenxlib` over the active built-in program preview window.
        {:else}
          Sampled directly from `sbagenxlib` over the current sequence duration.
        {/if}
      </p>
    </div>
    {#if preview}
      <span class="preview-duration">
        {formatDuration(preview.durationSec)}
        {#if preview.limited}
          cap
        {/if}
      </span>
    {/if}
  </div>

  {#if kind === 'sbgf'}
    <div class="preview-empty">
      `.sbgf` documents remain editor/validation only for now.
    </div>
  {:else if validating}
    <div class="preview-empty">Sampling beat preview...</div>
  {:else if error}
    <div class="preview-empty error">{error}</div>
  {:else if preview && preview.series.length === 0}
    <div class="preview-empty">
      No beat-bearing voices are active in the current target, so there is nothing meaningful to plot here.
    </div>
  {:else if preview}
    <div class="preview-stats">
      <div class="preview-stat">
        <span class="preview-stat-label">Min</span>
        <strong>{formatHzNullable(preview.minHz)}{preview.minHz === null ? '' : ' Hz'}</strong>
      </div>
      <div class="preview-stat">
        <span class="preview-stat-label">Max</span>
        <strong>{formatHzNullable(preview.maxHz)}{preview.maxHz === null ? '' : ' Hz'}</strong>
      </div>
      <div class="preview-stat">
        <span class="preview-stat-label">Voices</span>
        <strong>{preview.voiceCount}</strong>
      </div>
      <div class="preview-stat">
        <span class="preview-stat-label">Axis</span>
        <strong>{preview.timeLabel}</strong>
      </div>
    </div>

    <div class="preview-chart">
      <svg viewBox={`0 0 ${width} ${height}`} role="img" aria-label="Beat preview chart">
        <line x1={padLeft} y1={padTop} x2={padLeft} y2={height - padBottom} class="preview-axis" />
        <line
          x1={padLeft}
          y1={height - padBottom}
          x2={width - padRight}
          y2={height - padBottom}
          class="preview-axis"
        />

        {#each yAxis.ticks as tick}
          <line
            x1={padLeft}
            y1={yFor(tick, yAxis.axisMin, yAxis.axisMax)}
            x2={width - padRight}
            y2={yFor(tick, yAxis.axisMin, yAxis.axisMax)}
            class="preview-grid"
          />
          <line
            x1={padLeft - 4}
            y1={yFor(tick, yAxis.axisMin, yAxis.axisMax)}
            x2={padLeft}
            y2={yFor(tick, yAxis.axisMin, yAxis.axisMax)}
            class="preview-axis"
          />
        {/each}

        {#each xTicks as tick}
          <line
            x1={xFor(tick)}
            y1={padTop}
            x2={xFor(tick)}
            y2={height - padBottom}
            class="preview-grid preview-grid-vertical"
          />
          <line
            x1={xFor(tick)}
            y1={height - padBottom}
            x2={xFor(tick)}
            y2={height - padBottom + 4}
            class="preview-axis"
          />
        {/each}

        {#each seriesPaths as series}
          {#if series.path}
            <path
              d={series.path}
              fill="none"
              stroke={series.color}
              stroke-width="3"
              stroke-linejoin="round"
              stroke-linecap="round"
            />
          {/if}
        {/each}

        {#each yAxis.ticks as tick}
          <text
            x={padLeft - 6}
            y={yFor(tick, yAxis.axisMin, yAxis.axisMax) + 3}
            class="preview-label end"
          >
            {formatTickHz(tick)}
          </text>
        {/each}

        {#each xTicks as tick}
          <text
            x={xFor(tick)}
            y={height - padBottom + 14}
            class={`preview-label ${tick === 0 ? '' : 'end'}`}
          >
            {formatTimeTick(tick)}
          </text>
        {/each}

        <text x={width - padRight} y={height - 2} class="preview-label end">
          {preview.timeLabel}
        </text>
      </svg>
    </div>

    {#if preview.series.length > 1}
      <div class="preview-legend">
        {#each seriesPaths as series}
          <span class="preview-legend-item">
            <span class="preview-legend-swatch" style={`background:${series.color}`}></span>
            {series.label}
          </span>
        {/each}
      </div>
    {/if}
  {:else}
    <div class="preview-empty">
      {#if kind === 'curve-program'}
        Beat preview appears here once the active built-in `curve` program validates cleanly.
      {:else}
        Beat preview appears here once the active `.sbg` validates cleanly.
      {/if}
    </div>
  {/if}
</div>
