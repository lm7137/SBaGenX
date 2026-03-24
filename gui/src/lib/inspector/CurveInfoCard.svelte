<script lang="ts">
  import type { CurveInfoResult } from '../types'

  export let info: CurveInfoResult | null = null
  export let error: string | null = null
  export let validating = false

  function formatValue(value: number): string {
    return Math.abs(value) >= 10 ? value.toFixed(3) : value.toFixed(4)
  }

  $: flags = info
    ? [
        info.hasSolve && 'solve',
        info.hasCarrierExpr && 'carrier',
        info.hasAmpExpr && 'amp',
        info.hasMixampExpr && 'mixamp',
      ].filter(Boolean)
    : []
</script>

<div class="inspector-block preview-block">
  <div class="preview-header">
    <div>
      <p class="panel-title">Curve Info</p>
      <p class="panel-note">
        Parsed directly from `sbagenxlib` for the active `.sbgf` program.
      </p>
    </div>
  </div>

  {#if validating}
    <div class="preview-empty">Inspecting curve metadata...</div>
  {:else if error}
    <div class="preview-empty error">{error}</div>
  {:else if info}
    <div class="preview-stats">
      <div class="preview-stat">
        <span class="preview-stat-label">Parameters</span>
        <strong>{info.parameterCount}</strong>
      </div>
      <div class="preview-stat">
        <span class="preview-stat-label">Beat pieces</span>
        <strong>{info.beatPieceCount}</strong>
      </div>
      <div class="preview-stat">
        <span class="preview-stat-label">Carrier pieces</span>
        <strong>{info.carrierPieceCount}</strong>
      </div>
      <div class="preview-stat">
        <span class="preview-stat-label">Flags</span>
        <strong>{flags.length ? flags.join(', ') : 'none'}</strong>
      </div>
    </div>

    {#if info.parameters.length}
      <div class="curve-param-list">
        {#each info.parameters as param}
          <div class="curve-param-row">
            <span class="curve-param-name">{param.name}</span>
            <span class="curve-param-value">{formatValue(param.value)}</span>
          </div>
        {/each}
      </div>
    {:else}
      <div class="preview-empty">No explicit parameters declared in this curve.</div>
    {/if}
  {:else}
    <div class="preview-empty">
      Curve metadata appears here once the active `.sbgf` validates cleanly.
    </div>
  {/if}
</div>
