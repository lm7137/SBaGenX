<script lang="ts">
  import type { BeatPreviewResult } from '../types'

  export let preview: BeatPreviewResult | null = null
  export let error: string | null = null
  export let kind: 'sbg' | 'sbgf' | null = null
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

  function formatDuration(seconds: number): string {
    if (seconds >= 60) {
      const mins = Math.floor(seconds / 60)
      const secs = Math.round(seconds % 60)
      return `${mins}m ${secs}s`
    }
    return `${seconds.toFixed(seconds >= 10 ? 0 : 1)}s`
  }

  function chartPath(points: BeatPreviewResult['points']) {
    if (!preview || points.length === 0) return ''

    const usableWidth = width - padLeft - padRight
    const usableHeight = height - padTop - padBottom
    const hzSpan = Math.max(preview.maxHz - preview.minHz, 0.001)
    const duration = Math.max(preview.durationSec, 0.001)

    return points
      .map((point, index) => {
        const x = padLeft + (point.tSec / duration) * usableWidth
        const y =
          padTop +
          usableHeight -
          ((point.beatHz - preview.minHz) / hzSpan) * usableHeight
        return `${index === 0 ? 'M' : 'L'}${x.toFixed(2)},${y.toFixed(2)}`
      })
      .join(' ')
  }

  $: pathData = chartPath(preview?.points ?? [])
</script>

<div class="inspector-block preview-block">
  <div class="preview-header">
    <div>
      <p class="panel-title">Beat Preview</p>
      <p class="panel-note">
        Sampled directly from `sbagenxlib` over the current sequence duration.
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

  {#if kind !== 'sbg'}
    <div class="preview-empty">
      `.sbgf` documents remain editor/validation only for now.
    </div>
  {:else if validating}
    <div class="preview-empty">Sampling beat preview...</div>
  {:else if error}
    <div class="preview-empty error">{error}</div>
  {:else if preview}
    <div class="preview-stats">
      <div class="preview-stat">
        <span class="preview-stat-label">Min</span>
        <strong>{formatHz(preview.minHz)} Hz</strong>
      </div>
      <div class="preview-stat">
        <span class="preview-stat-label">Max</span>
        <strong>{formatHz(preview.maxHz)} Hz</strong>
      </div>
      <div class="preview-stat">
        <span class="preview-stat-label">Samples</span>
        <strong>{preview.sampleCount}</strong>
      </div>
      <div class="preview-stat">
        <span class="preview-stat-label">Axis</span>
        <strong>{preview.timeLabel}</strong>
      </div>
    </div>

    <div class="preview-chart">
      <svg viewBox={`0 0 ${width} ${height}`} role="img" aria-label="Beat preview chart">
        <defs>
          <linearGradient id="preview-line" x1="0%" y1="0%" x2="100%" y2="0%">
            <stop offset="0%" stop-color="#3a7cff" />
            <stop offset="100%" stop-color="#ff2ea6" />
          </linearGradient>
        </defs>

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

        {#if pathData}
          <path d={pathData} fill="none" stroke="url(#preview-line)" stroke-width="3" />
        {/if}

        <text x={padLeft} y="9" class="preview-label">
          {formatHz(preview.maxHz)} Hz
        </text>
        <text x={padLeft} y={height - 2} class="preview-label">
          {formatHz(preview.minHz)} Hz
        </text>
        <text x={width - padRight} y={height - 2} class="preview-label end">
          {preview.timeLabel}
        </text>
      </svg>
    </div>
  {:else}
    <div class="preview-empty">
      Beat preview appears here once the active `.sbg` validates cleanly.
    </div>
  {/if}
</div>
