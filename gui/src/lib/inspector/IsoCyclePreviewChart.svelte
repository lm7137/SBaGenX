<script lang='ts'>
  import type { IsoCyclePreviewResult } from '../types'

  export let preview: IsoCyclePreviewResult | null = null
  export let error: string | null = null
  export let validating = false

  const width = 560
  const panelHeight = 120
  const padLeft = 14
  const padRight = 10
  const padTop = 10
  const padBottom = 18

  function formatHz(value: number): string {
    return value >= 10 ? value.toFixed(1) : value.toFixed(2)
  }

  function formatDuration(seconds: number): string {
    return `${seconds.toFixed(seconds >= 0.1 ? 3 : 4)}s`
  }

  function seriesPath(values: number[], minValue: number, maxValue: number) {
    if (!preview || values.length === 0) return ''
    const usableWidth = width - padLeft - padRight
    const usableHeight = panelHeight - padTop - padBottom
    const duration = Math.max(preview.durationSec, 0.001)
    const span = Math.max(maxValue - minValue, 0.001)

    return values
      .map((value, index) => {
        const point = preview?.points[index]
        if (!point) return ''
        const x = padLeft + (point.tSec / duration) * usableWidth
        const y = padTop + usableHeight - ((value - minValue) / span) * usableHeight
        const cmd = index === 0 ? 'M' : 'L'
        return `${cmd}${x.toFixed(2)},${y.toFixed(2)}`
      })
      .filter(Boolean)
      .join(' ')
  }

  $: envelopeValues = preview?.points.map((point) => point.envelope) ?? []
  $: waveValues = preview?.points.map((point) => point.wave) ?? []
  $: waveMin = waveValues.length > 0 ? Math.min(...waveValues) : -1
  $: waveMax = waveValues.length > 0 ? Math.max(...waveValues) : 1
  $: normalizedWaveMin = waveMin === waveMax ? waveMin - 1 : waveMin
  $: normalizedWaveMax = waveMin === waveMax ? waveMax + 1 : waveMax
  $: envelopePath = seriesPath(envelopeValues, 0, 1)
  $: wavePath = seriesPath(waveValues, normalizedWaveMin, normalizedWaveMax)
</script>

<div class='inspector-block preview-block'>
  <div class='preview-header'>
    <div>
      <p class='panel-title'>Isochronic Cycle</p>
      <p class='panel-note'>
        Sampled directly from `sbagenxlib` over one isochronic cycle using the active built-in tone and current `-I` parameters.
      </p>
    </div>
    {#if preview}
      <span class='preview-duration'>{formatDuration(preview.durationSec)}</span>
    {/if}
  </div>

  {#if validating}
    <div class='preview-empty'>Sampling isochronic cycle...</div>
  {:else if error}
    <div class='preview-empty error'>{error}</div>
  {:else if preview}
    <div class='preview-stats'>
      <div class='preview-stat'>
        <span class='preview-stat-label'>Carrier</span>
        <strong>{formatHz(preview.carrierHz)} Hz</strong>
      </div>
      <div class='preview-stat'>
        <span class='preview-stat-label'>Beat</span>
        <strong>{formatHz(preview.beatHz)} Hz</strong>
      </div>
      <div class='preview-stat'>
        <span class='preview-stat-label'>Samples</span>
        <strong>{preview.sampleCount}</strong>
      </div>
      <div class='preview-stat'>
        <span class='preview-stat-label'>Axis</span>
        <strong>TIME SEC</strong>
      </div>
    </div>

    <div class='cycle-chart'>
      <div class='cycle-chart-title'>Envelope</div>
      <svg viewBox={`0 0 ${width} ${panelHeight}`} role='img' aria-label='Isochronic envelope cycle chart'>
        <line x1={padLeft} y1={padTop} x2={padLeft} y2={panelHeight - padBottom} class='preview-axis' />
        <line x1={padLeft} y1={panelHeight - padBottom} x2={width - padRight} y2={panelHeight - padBottom} class='preview-axis' />
        <line x1={padLeft} y1={padTop} x2={width - padRight} y2={padTop} class='preview-grid' />
        <line x1={padLeft} y1={(panelHeight - padBottom + padTop) / 2} x2={width - padRight} y2={(panelHeight - padBottom + padTop) / 2} class='preview-grid' />
        {#if envelopePath}
          <path d={envelopePath} fill='none' stroke='#ff5c66' stroke-width='3' stroke-linejoin='round' stroke-linecap='round' />
        {/if}
        <text x={padLeft} y='9' class='preview-label'>1</text>
        <text x={padLeft} y={panelHeight - 2} class='preview-label'>0</text>
        <text x={width - padRight} y={panelHeight - 2} class='preview-label end'>TIME SEC</text>
      </svg>
    </div>

    <div class='cycle-chart'>
      <div class='cycle-chart-title'>Waveform</div>
      <svg viewBox={`0 0 ${width} ${panelHeight}`} role='img' aria-label='Isochronic waveform cycle chart'>
        <line x1={padLeft} y1={padTop} x2={padLeft} y2={panelHeight - padBottom} class='preview-axis' />
        <line x1={padLeft} y1={panelHeight - padBottom} x2={width - padRight} y2={panelHeight - padBottom} class='preview-axis' />
        <line x1={padLeft} y1={padTop} x2={width - padRight} y2={padTop} class='preview-grid' />
        <line x1={padLeft} y1={(panelHeight - padBottom + padTop) / 2} x2={width - padRight} y2={(panelHeight - padBottom + padTop) / 2} class='preview-grid' />
        {#if wavePath}
          <path d={wavePath} fill='none' stroke='#3a7cff' stroke-width='3' stroke-linejoin='round' stroke-linecap='round' />
        {/if}
        <text x={padLeft} y='9' class='preview-label'>{normalizedWaveMax.toFixed(2)}</text>
        <text x={padLeft} y={panelHeight - 2} class='preview-label'>{normalizedWaveMin.toFixed(2)}</text>
        <text x={width - padRight} y={panelHeight - 2} class='preview-label end'>TIME SEC</text>
      </svg>
    </div>
  {:else}
    <div class='preview-empty'>Isochronic cycle preview appears here once the active built-in isochronic program validates cleanly.</div>
  {/if}
</div>

<style>
  .cycle-chart {
    display: grid;
    gap: 0.35rem;
    margin-top: 0.75rem;
  }

  .cycle-chart-title {
    font-size: 0.75rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-muted);
  }
</style>
