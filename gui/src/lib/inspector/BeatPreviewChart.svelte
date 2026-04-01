<script lang="ts">
  import type { BeatPreviewResult } from '../types'

  export let preview: BeatPreviewResult | null = null
  export let error: string | null = null
  export let kind: 'sbg' | 'sbgf' | 'curve-program' | 'built-in-program' | null = null
  export let validating = false

  const width = 560
  const height = 180
  const padLeft = 14
  const padRight = 10
  const padTop = 10
  const padBottom = 18

  function formatHz(value: number): string {
    return value >= 10 ? value.toFixed(1) : value.toFixed(2)
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

  function chartPath(points: BeatPreviewResult['series'][number]['points']) {
    if (!preview || points.length === 0) return ''

    const usableWidth = width - padLeft - padRight
    const usableHeight = height - padTop - padBottom
    const minHz = preview.minHz ?? 0
    const maxHz = preview.maxHz ?? 1
    const hzSpan = Math.max(maxHz - minHz, 0.001)
    const duration = Math.max(preview.durationSec, 0.001)

    return points
      .map((point, index) => {
        if (point.beatHz === null) {
          return ''
        }
        const x = padLeft + (point.tSec / duration) * usableWidth
        const y =
          padTop +
          usableHeight -
          ((point.beatHz - minHz) / hzSpan) * usableHeight
        const prev = points[index - 1]
        const cmd = index === 0 || !prev || prev.beatHz === null ? 'M' : 'L'
        return `${cmd}${x.toFixed(2)},${y.toFixed(2)}`
      })
      .filter(Boolean)
      .join(' ')
  }

  const lineColors = ['#3a7cff', '#ff2ea6', '#00a47a', '#7b61ff']

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
        <line
          x1={padLeft}
          y1={padTop}
          x2={width - padRight}
          y2={padTop}
          class="preview-grid"
        />
        <line
          x1={padLeft}
          y1={(height - padBottom + padTop) / 2}
          x2={width - padRight}
          y2={(height - padBottom + padTop) / 2}
          class="preview-grid"
        />

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

        <text x={padLeft} y="9" class="preview-label">
          {formatHzNullable(preview.maxHz)}{preview.maxHz === null ? '' : ' Hz'}
        </text>
        <text x={padLeft} y={height - 2} class="preview-label">
          {formatHzNullable(preview.minHz)}{preview.minHz === null ? '' : ' Hz'}
        </text>
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
