<script lang="ts">
  import { onMount, tick } from 'svelte'
  import { convertFileSrc } from '@tauri-apps/api/core'
  import { confirm, open, save } from '@tauri-apps/plugin-dialog'
  import MonacoEditor from './lib/editor/MonacoEditor.svelte'
  import logoUrl from './lib/assets/sbagenx-logo.svg'
  import {
    exportDocument,
    loadDevelopmentExamples,
    readTextFile,
    renderPreview,
    validateDocument,
    writeTextFile,
  } from './lib/backend'
  import type { DocumentKind, DocumentRecord, ValidationDiagnostic } from './lib/types'

  let documents: DocumentRecord[] = [makeUntitledDocument('sbg'), makeUntitledDocument('sbgf')]
  let activeId = documents[0].id
  let validationTimer: ReturnType<typeof setTimeout> | null = null
  let validatingId: string | null = null
  let transportBusy = false
  let transportMessage = 'ready'
  let selectedDiagnosticId: string | null = null
  let isPlaying = false
  let audioSource: string | null = null
  let audioElement: HTMLAudioElement | null = null
  let editorView: {
    revealPosition: (line: number, column?: number) => void
    focusEditor: () => void
  } | null = null

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
    selectedDiagnosticId = null
    tick().then(() => editorView?.focusEditor())
  }

  function makeDocumentRecord(source: {
    path: string | null
    name: string
    kind: DocumentKind
    content: string
  }): DocumentRecord {
    return {
      id: `doc-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`,
      name: source.name,
      path: source.path,
      kind: source.kind,
      dirty: false,
      content: source.content,
      lines: source.content.split('\n'),
      diagnostics: [],
    }
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
      const next: DocumentRecord = existing
        ? {
            ...existing,
            name: loaded.name,
            path: loaded.path,
            kind: loaded.kind,
            dirty: false,
            content: loaded.content,
            lines: loaded.content.split('\n'),
            diagnostics: [],
          }
        : makeDocumentRecord(loaded)
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
    selectedDiagnosticId = null
    queueValidation(doc.id)
    tick().then(() => editorView?.focusEditor())
  }

  async function closeDocument(id: string) {
    const doc = documents.find((item) => item.id === id)
    if (!doc) return

    if (doc.dirty) {
      const approved = await confirm(
        `Close ${doc.name} without saving your changes?`,
        {
          title: 'Unsaved changes',
          kind: 'warning',
          okLabel: 'Close anyway',
          cancelLabel: 'Keep editing',
        },
      )
      if (!approved) return
    }

    if (activeId === id) {
      await stopPlayback(false)
    }

    const currentIndex = documents.findIndex((item) => item.id === id)
    const remaining = documents.filter((item) => item.id !== id)
    if (remaining.length === 0) {
      const fallback = makeUntitledDocument('sbg')
      documents = [fallback]
      activeId = fallback.id
      transportMessage = `closed ${doc.name}`
      selectedDiagnosticId = null
      queueValidation(fallback.id)
      tick().then(() => editorView?.focusEditor())
      return
    }

    documents = remaining
    if (activeId === id) {
      const nextIndex = Math.max(0, Math.min(currentIndex, remaining.length - 1))
      activeId = remaining[nextIndex].id
      selectedDiagnosticId = null
      tick().then(() => editorView?.focusEditor())
    }
    transportMessage = `closed ${doc.name}`
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
      updateDocument(doc.id, { diagnostics: result.diagnostics })
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error)
      const diagnostic: ValidationDiagnostic = {
        id: `diag-${doc.id}-bridge`,
        documentId: doc.id,
        severity: 'error',
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
      transportMessage = `exported ${result.format.toUpperCase()} · ${formatDuration(result.durationSec)}`
    } catch (error) {
      transportMessage = error instanceof Error ? error.message : String(error)
    } finally {
      transportBusy = false
    }
  }

  async function jumpToDiagnostic(item: ValidationDiagnostic) {
    selectedDiagnosticId = item.id
    if (!item.line) return
    await tick()
    editorView?.revealPosition(item.line, item.column ?? 1)
  }

  function handleGlobalKeydown(event: KeyboardEvent) {
    const modifier = event.metaKey || event.ctrlKey
    if (!modifier || event.altKey) return
    const key = event.key.toLowerCase()

    if (key === 's') {
      event.preventDefault()
      void saveActiveDocument()
      return
    }

    if (key === 'o') {
      event.preventDefault()
      void openDocuments()
      return
    }

    if (key === 'n') {
      event.preventDefault()
      createDocument(event.shiftKey ? 'sbgf' : 'sbg')
      return
    }

    if (key === 'w' && activeDocument) {
      event.preventDefault()
      void closeDocument(activeDocument.id)
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

  onMount(() => {
    const onKeydown = (event: KeyboardEvent) => handleGlobalKeydown(event)
    window.addEventListener('keydown', onKeydown)

    void (async () => {
      const untouchedStarterTabs = documents.every((doc) => !doc.path && !doc.dirty)
      if (untouchedStarterTabs) {
        try {
          const examples = await loadDevelopmentExamples()
          if (examples.length > 0) {
            documents = examples.map((example) => makeDocumentRecord(example))
            activeId = documents[0].id
            transportMessage = 'loaded development examples'
          }
        } catch (error) {
          transportMessage = error instanceof Error ? error.message : String(error)
        }
      }
      for (const doc of documents) {
        void runValidation(doc.id)
      }
    })()

    return () => {
      window.removeEventListener('keydown', onKeydown)
      if (validationTimer) clearTimeout(validationTimer)
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

<div class="bg-overlay" aria-hidden="true"></div>
<div class="noise-overlay" aria-hidden="true"></div>

<div class="shell">
  <header class="topbar">
    <div class="brand">
      <img class="brand-logo" src={logoUrl} alt="SBaGenX" />
      <div class="brand-copy">
        <h1>SBAGENX DESKTOP</h1>
        <p class="brand-subtitle">Library-backed editor, validation, playback, and export.</p>
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

  <main class="workspace">
    <aside class="sidebar panel">
      <div class="panel-header">
        <p class="panel-title">Documents</p>
      </div>

      <div class="document-list">
        {#each documents as document}
          <div class:active={document.id === activeId} class="document-row">
            <button class="document-main" on:click={() => activateDocument(document.id)}>
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
            <button
              class="document-close"
              title={`Close ${document.name}`}
              aria-label={`Close ${document.name}`}
              on:click|stopPropagation={() => void closeDocument(document.id)}
            >
              ×
            </button>
          </div>
        {/each}
      </div>

      <div class="panel-footer">
        <p class="panel-note">
          Keep `.sbg` and `.sbgf` side by side here and switch from the
          document list.
        </p>
      </div>
    </aside>

    <section class="editor-column">
      <section class="editor panel">
        <div class="panel-header">
          <div>
            <p class="panel-title">Editor</p>
          </div>
          <div class="editor-meta">
            <span>{activeDocument?.kind ?? 'unknown'}</span>
            <span>{activeDocument?.path ?? 'unsaved'}</span>
            <span class:busy={transportBusy || validatingId === activeDocument?.id} class="editor-status">
              {#if validatingId === activeDocument?.id}
                validating...
              {:else}
                {transportMessage}
              {/if}
            </span>
          </div>
        </div>

        <div class="editor-surface">
          {#if activeDocument}
            <MonacoEditor
              bind:this={editorView}
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
          <button
            class:selected={item.id === selectedDiagnosticId}
            class:jumpable={!!item.line}
            class="diagnostic diagnostic-button"
            disabled={!item.line}
            on:click={() => void jumpToDiagnostic(item)}
          >
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
          </button>
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
