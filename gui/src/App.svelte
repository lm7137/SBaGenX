<script lang="ts">
  import { onMount, tick } from 'svelte'
  import { convertFileSrc } from '@tauri-apps/api/core'
  import { open, save } from '@tauri-apps/plugin-dialog'
  import MonacoEditor from './lib/editor/MonacoEditor.svelte'
  import {
    backendStatus,
    exportDocument,
    readTextFile,
    renderPreview,
    validateDocument,
    writeTextFile,
  } from './lib/backend'
  import type {
    BackendStatus,
    DocumentKind,
    DocumentRecord,
    ValidationDiagnostic,
  } from './lib/types'

  let documents: DocumentRecord[] = [makeUntitledDocument('sbg'), makeUntitledDocument('sbgf')]
  let activeId = documents[0].id
  let status: BackendStatus | null = null
  let validationBridge = 'loading'
  let validationTimer: ReturnType<typeof setTimeout> | null = null
  let validatingId: string | null = null
  let transportBusy = false
  let transportMessage = 'ready'
  let isPlaying = false
  let audioSource: string | null = null
  let audioElement: HTMLAudioElement | null = null

  function makeUntitledDocument(kind: DocumentKind): DocumentRecord {
    const base =
      kind === 'sbg'
        ? '-SE\n\nalloff: -\n\nNOW alloff\n'
        : `# SBaGenX function curve
param l = 0.125
param h = 0

beat = b0 + (b1 - b0) * (0.5 + 0.5 * tanh(l * (m - D/2 - h)))
carrier = c0 + (c1 - c0) * ramp(m, 0, T)
`
    const stamp = `${kind}-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`
    return {
      id: stamp,
      name: kind === 'sbg' ? 'untitled.sbg' : 'untitled.sbgf',
      path: null,
      kind,
      dirty: false,
      content: base,
      lines: base.split('\n'),
      diagnostics: [],
    }
  }

  function activateDocument(id: string) {
    activeId = id
  }

  function upsertDocument(next: DocumentRecord) {
    const existingIndex = documents.findIndex((doc) => doc.id === next.id)
    if (existingIndex === -1) {
      documents = [...documents, next]
      return
    }
    documents = documents.map((doc) => (doc.id === next.id ? next : doc))
  }

  function updateDocument(id: string, patch: Partial<DocumentRecord>) {
    documents = documents.map((doc) => {
      if (doc.id !== id) return doc
      const merged = { ...doc, ...patch }
      merged.lines = merged.content.split('\n')
      return merged
    })
  }

  function formatDuration(seconds: number): string {
    if (seconds >= 60) {
      const mins = Math.floor(seconds / 60)
      const secs = Math.round(seconds % 60)
      return `${mins}m ${secs}s`
    }
    return `${seconds.toFixed(seconds >= 10 ? 0 : 1)}s`
  }

  function syncBridge(bridge: string, engineVersion: string) {
    validationBridge = `${bridge} · ${engineVersion}`
  }

  async function openDocuments() {
    const selection = await open({
      multiple: true,
      filters: [
        { name: 'SBaGenX files', extensions: ['sbg', 'sbgf'] },
        { name: 'Sequence files', extensions: ['sbg'] },
        { name: 'Curve files', extensions: ['sbgf'] },
      ],
    })
    if (!selection) return
    const paths = Array.isArray(selection) ? selection : [selection]
    for (const path of paths) {
      const loaded = await readTextFile(path)
      const existing = documents.find((doc) => doc.path === loaded.path)
      const next: DocumentRecord = {
        id: existing?.id ?? `doc-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`,
        name: loaded.name,
        path: loaded.path,
        kind: loaded.kind,
        dirty: false,
        content: loaded.content,
        lines: loaded.content.split('\n'),
        diagnostics: [],
      }
      upsertDocument(next)
      activeId = next.id
      queueValidation(next.id)
    }
  }

  async function saveActiveDocument() {
    if (!activeDocument) return
    let path = activeDocument.path
    if (!path) {
      path = await save({
        defaultPath: activeDocument.name,
        filters: [
          {
            name: activeDocument.kind === 'sbg' ? 'Sequence files' : 'Curve files',
            extensions: [activeDocument.kind],
          },
        ],
      })
      if (!path) return
    }
    await writeTextFile(path, activeDocument.content)
    updateDocument(activeDocument.id, {
      path,
      name: path.split(/[\\/]/).pop() ?? activeDocument.name,
      dirty: false,
    })
    transportMessage = `saved ${path.split(/[\\/]/).pop() ?? path}`
  }

  function createDocument(kind: DocumentKind) {
    const doc = makeUntitledDocument(kind)
    documents = [...documents, doc]
    activeId = doc.id
    queueValidation(doc.id)
  }

  function handleEditorChange(nextValue: string) {
    if (!activeDocument) return
    updateDocument(activeDocument.id, {
      content: nextValue,
      dirty: true,
    })
    queueValidation(activeDocument.id)
  }

  function queueValidation(id: string) {
    if (validationTimer) clearTimeout(validationTimer)
    validatingId = id
    validationTimer = setTimeout(() => {
      void runValidation(id)
    }, 300)
  }

  async function runValidation(id: string) {
    const doc = documents.find((item) => item.id === id)
    if (!doc) return
    try {
      const result = await validateDocument(doc.kind, doc.content, doc.path ?? doc.name)
      syncBridge(result.bridge, result.engineVersion)
      updateDocument(doc.id, { diagnostics: result.diagnostics })
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      const diagnostic: ValidationDiagnostic = {
        id: `diag-${doc.id}-bridge`,
        documentId: doc.id,
        severity: 'error',
        line: 1,
        column: 1,
        message,
      }
      updateDocument(doc.id, { diagnostics: [diagnostic] })
    } finally {
      validatingId = null
    }
  }

  async function playActiveDocument() {
    if (!activeDocument || activeDocument.kind !== 'sbg') return
    transportBusy = true
    transportMessage = 'rendering preview audio...'
    try {
      await stopPlayback(false)
      const result = await renderPreview(activeDocument.content, activeDocument.path ?? activeDocument.name)
      syncBridge(result.bridge, result.engineVersion)
      audioSource = convertFileSrc(result.audioPath)
      await tick()
      if (!audioElement) {
        throw new Error('audio element is not available')
      }
      audioElement.load()
      await audioElement.play()
      transportMessage = result.limited
        ? `preview playing · ${formatDuration(result.durationSec)} cap`
        : `preview playing · ${formatDuration(result.durationSec)}`
    } catch (error) {
      transportMessage = error instanceof Error ? error.message : String(error)
    } finally {
      transportBusy = false
    }
  }

  async function stopPlayback(updateMessage = true) {
    if (!audioElement) return
    audioElement.pause()
    audioElement.currentTime = 0
    isPlaying = false
    if (updateMessage) {
      transportMessage = 'playback stopped'
    }
  }

  async function exportActiveDocument() {
    if (!activeDocument || activeDocument.kind !== 'sbg') return
    const target = await save({
      defaultPath: activeDocument.name.replace(/\.sbg$/i, '.wav'),
      filters: [
        { name: 'WAV audio', extensions: ['wav'] },
        { name: 'FLAC audio', extensions: ['flac'] },
        { name: 'OGG/Vorbis audio', extensions: ['ogg'] },
        { name: 'MP3 audio', extensions: ['mp3'] },
      ],
    })
    if (!target) return

    transportBusy = true
    transportMessage = 'exporting audio...'
    try {
      const result = await exportDocument(activeDocument.content, target, activeDocument.path ?? activeDocument.name)
      syncBridge(result.bridge, result.engineVersion)
      transportMessage = `exported ${result.format.toUpperCase()} · ${formatDuration(result.durationSec)}`
    } catch (error) {
      transportMessage = error instanceof Error ? error.message : String(error)
    } finally {
      transportBusy = false
    }
  }

  function handleAudioPlay() {
    isPlaying = true
  }

  function handleAudioPause() {
    isPlaying = false
  }

  function handleAudioEnded() {
    isPlaying = false
    transportMessage = 'preview finished'
  }

  $: activeDocument = documents.find((doc) => doc.id === activeId) ?? documents[0]
  $: activeDiagnostics = activeDocument?.diagnostics ?? []
  $: canRunActive =
    !!activeDocument && activeDocument.kind === 'sbg' && activeDiagnostics.length === 0 && !transportBusy

  onMount(async () => {
    status = await backendStatus()
    for (const doc of documents) {
      void runValidation(doc.id)
    }
  })
</script>

<svelte:head>
  <title>SBaGenX GUI</title>
  <meta
    name="description"
    content="Library-backed desktop editor and player for .sbg and .sbgf files."
  />
</svelte:head>

<audio
  bind:this={audioElement}
  src={audioSource ?? undefined}
  on:play={handleAudioPlay}
  on:pause={handleAudioPause}
  on:ended={handleAudioEnded}
></audio>

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
      <button class="button ghost" on:click={openDocuments}>Open</button>
      <button class="button ghost" on:click={saveActiveDocument}>Save</button>
      <button class="button ghost" on:click={() => createDocument('sbg')}>New `.sbg`</button>
      <button class="button ghost" on:click={() => createDocument('sbgf')}>New `.sbgf`</button>
      <span class="toolbar-divider"></span>
      <button class="button accent" disabled={!canRunActive} on:click={playActiveDocument}>Play</button>
      <button class="button ghost" disabled={!isPlaying} on:click={() => stopPlayback()}>Stop</button>
      <button class="button export" disabled={!canRunActive} on:click={exportActiveDocument}>Export</button>
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
    <div class="status-group">
      <span class="status-label">Validation</span>
      <span class="status-value">{validationBridge}</span>
    </div>
    <div class="status-group transport-group">
      <span class="status-label">Transport</span>
      <span class="status-value">{transportMessage}</span>
    </div>
    {#if validatingId}
      <div class="status-group">
        <span class="status-label">State</span>
        <span class="status-value">validating</span>
      </div>
    {/if}
  </section>

  <main class="workspace">
    <aside class="sidebar panel">
      <div class="panel-header">
        <p class="panel-title">Documents</p>
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
            {#if document.diagnostics.length}
              <span class="document-diagnostics">{document.diagnostics.length} issue(s)</span>
            {/if}
          </button>
        {/each}
      </div>

      <div class="panel-footer">
        <p class="panel-note">
          Open `.sbg` and `.sbgf` side by side, switch between tabs, and validate
          against `sbagenxlib` directly.
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
              Monaco is wired in. Validation, preview playback, and export are running
              through the Rust bridge against `sbagenxlib`.
            </p>
          </div>
          <div class="editor-meta">
            <span>{activeDocument?.kind ?? 'unknown'}</span>
            <span>{activeDocument?.path ?? 'unsaved'}</span>
          </div>
        </div>

        <div class="editor-surface">
          {#if activeDocument}
            <MonacoEditor
              docId={activeDocument.id}
              language={activeDocument.kind}
              value={activeDocument.content}
              diagnostics={activeDiagnostics}
              onChange={handleEditorChange}
            />
          {/if}
        </div>
      </section>
    </section>

    <aside class="inspector panel">
      <div class="panel-header">
        <div>
          <p class="panel-title">Diagnostics</p>
          <p class="panel-subtitle">
            Real-time validation is now using a first real `sbagenxlib` bridge.
          </p>
        </div>
        <span class="diagnostic-count">{activeDiagnostics.length}</span>
      </div>

      <div class="diagnostic-list">
        {#if activeDiagnostics.length === 0}
          <article class="diagnostic ok">
            <div class="diagnostic-head">
              <span class="severity ok">ok</span>
              <span class="location">validated</span>
            </div>
            <p class="diagnostic-message">No validation issues for the active document.</p>
          </article>
        {/if}
        {#each activeDiagnostics as item}
          <article class="diagnostic">
            <div class="diagnostic-head">
              <span class:warning={item.severity === 'warning'} class="severity">{item.severity}</span>
              <span class="location">
                {#if item.line}
                  L{item.line}{#if item.column}:C{item.column}{/if}
                {:else}
                  general
                {/if}
              </span>
            </div>
            <p class="diagnostic-message">{item.message}</p>
          </article>
        {/each}
      </div>

      <div class="inspector-block">
        <p class="panel-title">Current Scope</p>
        <ul class="milestone-list">
          <li>Tabbed `.sbg` / `.sbgf` editing</li>
          <li>Monaco editor integration</li>
          <li>Dialog-backed open/save</li>
          <li>Debounced native validation</li>
          <li>`.sbg` preview playback and export</li>
          <li>Standalone `.sbgf` tabs remain editor/validation only</li>
        </ul>
      </div>
    </aside>
  </main>
</div>
