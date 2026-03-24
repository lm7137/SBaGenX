<script lang="ts">
  import { onMount } from 'svelte'
  import { backendStatus } from './lib/backend'
  import { mockDiagnostics, mockDocuments } from './lib/mock-data'
  import type { BackendStatus, DocumentRecord, ValidationDiagnostic } from './lib/types'

  let documents: DocumentRecord[] = mockDocuments
  let activeId = documents[0]?.id ?? ''
  let diagnostics: ValidationDiagnostic[] = mockDiagnostics
  let status: BackendStatus | null = null

  const transport = {
    canPlay: true,
    canStop: false,
    canExport: true,
  }

  $: activeDocument = documents.find((doc) => doc.id === activeId) ?? documents[0]
  $: activeDiagnostics = diagnostics.filter((item) => item.documentId === activeId)

  onMount(async () => {
    status = await backendStatus()
  })

  function activateDocument(id: string) {
    activeId = id
  }
</script>

<svelte:head>
  <title>SBaGenX GUI</title>
  <meta
    name="description"
    content="Library-backed desktop editor and player for .sbg and .sbgf files."
  />
</svelte:head>

<div class="shell">
  <header class="topbar">
    <div class="brand">
      <div class="brand-mark">SX</div>
      <div>
        <p class="eyebrow">SBaGenX Desktop</p>
        <h1>Library-Backed Session Studio</h1>
      </div>
    </div>

    <div class="toolbar">
      <button class="button ghost">Open</button>
      <button class="button ghost">Save</button>
      <button class="button ghost">Save As</button>
      <span class="toolbar-divider"></span>
      <button class="button accent" disabled={!transport.canPlay}>Play</button>
      <button class="button ghost" disabled={!transport.canStop}>Stop</button>
      <button class="button export" disabled={!transport.canExport}>Export</button>
    </div>
  </header>

  <section class="statusbar">
    <div class="status-group">
      <span class="status-label">Bridge</span>
      <span class="status-value">{status?.bridge ?? 'loading'}</span>
    </div>
    <div class="status-group">
      <span class="status-label">Engine</span>
      <span class="status-value">{status?.engine ?? 'loading'}</span>
    </div>
    <div class="status-group">
      <span class="status-label">Mode</span>
      <span class="status-value">{status?.mode ?? 'loading'}</span>
    </div>
    <div class="status-group">
      <span class="status-label">Target</span>
      <span class="status-value">{status?.target ?? 'loading'}</span>
    </div>
  </section>

  <main class="workspace">
    <aside class="sidebar panel">
      <div class="panel-header">
        <p class="panel-title">Documents</p>
        <button class="mini-button">+</button>
      </div>

      <div class="document-list">
        {#each documents as document}
          <button
            class:active={document.id === activeId}
            class="document-row"
            on:click={() => activateDocument(document.id)}
          >
            <div class="document-name-row">
              <span class="document-name">{document.name}</span>
              {#if document.dirty}
                <span class="dirty-indicator"></span>
              {/if}
            </div>
            <span class="document-type">{document.kind}</span>
          </button>
        {/each}
      </div>

      <div class="panel-footer">
        <p class="panel-note">
          Multi-tab `.sbg` and `.sbgf` editing is a first-class requirement.
        </p>
      </div>
    </aside>

    <section class="editor-column">
      <div class="tabs panel">
        <div class="tab-strip">
          {#each documents as document}
            <button
              class:active={document.id === activeId}
              class="tab"
              on:click={() => activateDocument(document.id)}
            >
              <span>{document.name}</span>
              {#if document.dirty}
                <span class="tab-dirty"></span>
              {/if}
            </button>
          {/each}
        </div>
      </div>

      <section class="editor panel">
        <div class="panel-header">
          <div>
            <p class="panel-title">Editor</p>
            <p class="panel-subtitle">
              Monaco integration is the next step. This scaffold already models
              per-document tabs, dirty state, and diagnostics ownership.
            </p>
          </div>
          <div class="editor-meta">
            <span>{activeDocument?.kind ?? 'unknown'}</span>
            <span>{activeDocument?.path ?? 'unsaved'}</span>
          </div>
        </div>

        <div class="editor-surface">
          <div class="editor-gutter">
            {#if activeDocument}
              {#each activeDocument.lines as _, index}
                <span>{index + 1}</span>
              {/each}
            {/if}
          </div>

          <pre class="editor-code"><code>{activeDocument?.content ?? ''}</code></pre>
        </div>
      </section>
    </section>

    <aside class="inspector panel">
      <div class="panel-header">
        <div>
          <p class="panel-title">Diagnostics</p>
          <p class="panel-subtitle">Real-time validation will be library-backed through `sbagenxlib`.</p>
        </div>
        <span class="diagnostic-count">{activeDiagnostics.length}</span>
      </div>

      <div class="diagnostic-list">
        {#each activeDiagnostics as item}
          <article class="diagnostic">
            <div class="diagnostic-head">
              <span class:warning={item.severity === 'warning'} class="severity">{item.severity}</span>
              <span class="location">L{item.line}{#if item.column}:C{item.column}{/if}</span>
            </div>
            <p class="diagnostic-message">{item.message}</p>
          </article>
        {/each}
      </div>

      <div class="inspector-block">
        <p class="panel-title">First Milestone</p>
        <ul class="milestone-list">
          <li>Open/save `.sbg` and `.sbgf`</li>
          <li>Tabbed editor surface</li>
          <li>Debounced validation markers</li>
          <li>Play / Stop / Export</li>
          <li>Direct `sbagenxlib` integration</li>
        </ul>
      </div>
    </aside>
  </main>
</div>
