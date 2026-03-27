<script lang="ts">
  import { onMount, tick } from 'svelte'
  import { listen } from '@tauri-apps/api/event'
  import { confirm, message, open, save } from '@tauri-apps/plugin-dialog'
  import { getCurrentWindow } from '@tauri-apps/api/window'
  import MonacoEditor from './lib/editor/MonacoEditor.svelte'
  import BeatPreviewChart from './lib/inspector/BeatPreviewChart.svelte'
  import CurveInfoCard from './lib/inspector/CurveInfoCard.svelte'
  import logoUrl from './lib/assets/sbagenx-logo.svg'
  import {
    exitApplication,
    exportDocument,
    inspectCurveInfo,
    loadDevelopmentExamples,
    loadRecentFiles,
    loadSessionDocuments,
    readTextFile,
    saveSessionState,
    sampleBeatPreview,
    startLivePreview,
    stopLivePreview,
    validateDocument,
    writeTextFile,
  } from './lib/backend'
  import type {
    DocumentKind,
    DocumentRecord,
    PlaybackEvent,
    RecentFileEntry,
    ValidationDiagnostic,
  } from './lib/types'

  let documents: DocumentRecord[] = [makeUntitledDocument('sbg'), makeUntitledDocument('sbgf')]
  let activeId = documents[0].id
  let validationTimer: ReturnType<typeof setTimeout> | null = null
  let validatingId: string | null = null
  let transportBusy = false
  let transportMessage = 'ready'
  let selectedDiagnosticId: string | null = null
  let isPlaying = false
  let recentFiles: RecentFileEntry[] = []
  let previewLoadingId: string | null = null
  let curveInfoLoadingId: string | null = null
  let editorView: {
    revealPosition: (line: number, column?: number) => void
    focusEditor: () => void
  } | null = null
  let unlistenPlaybackEvents: (() => void) | null = null


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
      beatPreview: null,
      beatPreviewError: null,
      curveInfo: null,
      curveInfoError: null,
    }
  }

  function activateDocument(id: string) {
    activeId = id
    selectedDiagnosticId = null
    void persistSessionState()
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
      beatPreview: null,
      beatPreviewError: null,
      curveInfo: null,
      curveInfoError: null,
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

  function currentDocumentMatches(id: string, content: string) {
    const current = documents.find((doc) => doc.id === id)
    return !!current && current.content === content
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
            beatPreview: null,
            beatPreviewError: null,
            curveInfo: null,
            curveInfoError: null,
          }
        : makeDocumentRecord(loaded)
      upsertDocument(next)
      activeId = next.id
      queueValidation(next.id)
    }
    await refreshRecentFiles()
    await persistSessionState()
  }

  async function saveActiveDocument() {
    if (!activeDocument) return
    let path = activeDocument.path
    if (!path) {
      path = await promptSavePath(activeDocument)
      if (!path) return
    }
    await writeDocumentToPath(activeDocument.id, path, activeDocument.content)
  }

  async function saveActiveDocumentAs() {
    if (!activeDocument) return
    const path = await promptSavePath(activeDocument)
    if (!path) return
    await writeDocumentToPath(activeDocument.id, path, activeDocument.content)
  }

  async function promptSavePath(doc: DocumentRecord) {
    return save({
      defaultPath: doc.path ?? doc.name,
      filters: [
        {
          name: doc.kind === 'sbg' ? 'Sequence files' : 'Curve files',
          extensions: [doc.kind],
        },
      ],
    })
  }

  async function writeDocumentToPath(id: string, path: string, content: string) {
    await writeTextFile(path, content)
    updateDocument(id, {
      path,
      name: path.split(/[\\/]/).pop() ?? path,
      dirty: false,
    })
    await refreshRecentFiles()
    transportMessage = `saved ${path.split(/[\\/]/).pop() ?? path}`
    await persistSessionState()
  }

  async function saveDocumentById(id: string) {
    const doc = documents.find((item) => item.id === id)
    if (!doc) return false
    const path = doc.path ?? (await promptSavePath(doc))
    if (!path) return false
    await writeDocumentToPath(id, path, doc.content)
    return true
  }

  async function refreshRecentFiles() {
    try {
      recentFiles = await loadRecentFiles()
    } catch {
      recentFiles = []
    }
  }

  async function persistSessionState() {
    const paths = documents
      .map((doc) => doc.path)
      .filter((path): path is string => !!path)
    const activePath = documents.find((doc) => doc.id === activeId)?.path ?? null
    await saveSessionState(paths, activePath)
  }

  async function openRecentFile(path: string) {
    try {
      const loaded = await readTextFile(path)
      const existing = documents.find((doc) => doc.path === loaded.path)
      const next = existing
        ? {
            ...existing,
            name: loaded.name,
            path: loaded.path,
            kind: loaded.kind,
            dirty: false,
            content: loaded.content,
            lines: loaded.content.split('\n'),
            diagnostics: [],
            beatPreview: null,
            beatPreviewError: null,
            curveInfo: null,
            curveInfoError: null,
          }
        : makeDocumentRecord(loaded)
      upsertDocument(next)
      activeId = next.id
      queueValidation(next.id)
      await refreshRecentFiles()
      await persistSessionState()
    } catch (error) {
      transportMessage = error instanceof Error ? error.message : String(error)
      await refreshRecentFiles()
    }
  }

  function createDocument(kind: DocumentKind) {
    const doc = makeUntitledDocument(kind)
    documents = [...documents, doc]
    activeId = doc.id
    selectedDiagnosticId = null
    queueValidation(doc.id)
    void persistSessionState()
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
      await persistSessionState()
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
    await persistSessionState()
  }

  function handleEditorChange(nextValue: string) {
    if (!activeDocument) return
    updateDocument(activeDocument.id, {
      content: nextValue,
      dirty: true,
      beatPreview: null,
      beatPreviewError: null,
      curveInfo: null,
      curveInfoError: null,
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
    const contentSnapshot = doc.content
    try {
      const result = await validateDocument(doc.kind, doc.content, doc.path ?? doc.name)
      if (!currentDocumentMatches(doc.id, contentSnapshot)) return
      updateDocument(doc.id, {
        diagnostics: result.diagnostics,
        beatPreview: result.valid && doc.kind === 'sbg' ? doc.beatPreview : null,
        beatPreviewError: null,
        curveInfo: result.valid && doc.kind === 'sbgf' ? doc.curveInfo : null,
        curveInfoError: null,
      })
      if (result.valid && doc.kind === 'sbg') {
        void loadBeatPreview(doc.id, contentSnapshot, doc.path ?? doc.name)
      } else if (result.valid && doc.kind === 'sbgf') {
        void loadCurveInfo(doc.id, contentSnapshot, doc.path ?? doc.name)
      }
    } catch (error) {
      if (!currentDocumentMatches(doc.id, contentSnapshot)) return
      const message = error instanceof Error ? error.message : String(error)
      const diagnostic: ValidationDiagnostic = {
        id: `diag-${doc.id}-bridge`,
        documentId: doc.id,
        severity: 'error',
        message,
      }
      updateDocument(doc.id, {
        diagnostics: [diagnostic],
        beatPreview: null,
        beatPreviewError: null,
        curveInfo: null,
        curveInfoError: null,
      })
    } finally {
      validatingId = null
    }
  }

  async function loadBeatPreview(id: string, text: string, sourceName: string) {
    previewLoadingId = id
    try {
      const preview = await sampleBeatPreview(text, sourceName)
      if (!currentDocumentMatches(id, text)) return
      updateDocument(id, {
        beatPreview: preview,
        beatPreviewError: null,
      })
    } catch (error) {
      if (!currentDocumentMatches(id, text)) return
      updateDocument(id, {
        beatPreview: null,
        beatPreviewError: error instanceof Error ? error.message : String(error),
      })
    } finally {
      if (previewLoadingId === id) {
        previewLoadingId = null
      }
    }
  }

  async function loadCurveInfo(id: string, text: string, sourceName: string) {
    curveInfoLoadingId = id
    try {
      const info = await inspectCurveInfo(text, sourceName)
      if (!currentDocumentMatches(id, text)) return
      updateDocument(id, {
        curveInfo: info,
        curveInfoError: null,
      })
    } catch (error) {
      if (!currentDocumentMatches(id, text)) return
      updateDocument(id, {
        curveInfo: null,
        curveInfoError: error instanceof Error ? error.message : String(error),
      })
    } finally {
      if (curveInfoLoadingId === id) {
        curveInfoLoadingId = null
      }
    }
  }

  async function playActiveDocument() {
    if (!activeDocument || activeDocument.kind !== 'sbg') return
    transportBusy = true
    transportMessage = 'starting live preview...'
    try {
      await stopPlayback(false)
      const result = await startLivePreview(
        activeDocument.content,
        activeDocument.path ?? activeDocument.name,
      )
      isPlaying = true
      transportMessage = result.limited
        ? `live preview · ${formatDuration(result.durationSec)} cap`
        : `live preview · ${formatDuration(result.durationSec)}`
    } catch (error) {
      transportMessage = error instanceof Error ? error.message : String(error)
    } finally {
      transportBusy = false
    }
  }

  async function stopPlayback(updateMessage = true) {
    try {
      await stopLivePreview()
    } catch {
      // Keep local UI state coherent even if the backend has already stopped.
    }
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

  async function finalizeWindowClose() {
    await exitApplication()
  }


  async function maybeCloseWindow() {
    const dirtyDocs = documents.filter((doc) => doc.dirty)

    const result = await message(
      dirtyDocs.length === 1
        ? `${dirtyDocs[0].name} has unsaved changes.`
        : `${dirtyDocs.length} documents have unsaved changes.`,
      {
        title: 'Unsaved changes',
        kind: 'warning',
        buttons: {
          yes: 'Save and quit',
          no: 'Discard changes and quit',
          cancel: 'Keep editing',
        },
      },
    )

    if (result === 'Keep editing' || result === 'Cancel') {
      return
    }

    if (result === 'Save and quit' || result === 'Yes') {
      for (const doc of dirtyDocs) {
        activeId = doc.id
        await tick()
        const saved = await saveDocumentById(doc.id)
        if (!saved) {
          transportMessage = `close cancelled for ${doc.name}`
          return
        }
      }
    }

    await finalizeWindowClose()
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
      if (event.shiftKey) {
        void saveActiveDocumentAs()
      } else {
        void saveActiveDocument()
      }
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

  $: activeDocument = documents.find((doc) => doc.id === activeId) ?? documents[0]
  $: activeDiagnostics = activeDocument?.diagnostics ?? []
  $: activeBeatPreview = activeDocument?.beatPreview ?? null
  $: activeBeatPreviewError = activeDocument?.beatPreviewError ?? null
  $: activeCurveInfo = activeDocument?.curveInfo ?? null
  $: activeCurveInfoError = activeDocument?.curveInfoError ?? null
  $: canRunActive =
    !!activeDocument && activeDocument.kind === 'sbg' && activeDiagnostics.length === 0 && !transportBusy

  onMount(() => {
    const onKeydown = (event: KeyboardEvent) => handleGlobalKeydown(event)
    window.addEventListener('keydown', onKeydown)

    void (async () => {
      unlistenPlaybackEvents = await listen<PlaybackEvent>('playback-state', (event) => {
        const payload = event.payload
        switch (payload.state) {
          case 'started':
            isPlaying = true
            break
          case 'finished':
            isPlaying = false
            transportBusy = false
            transportMessage = 'preview finished'
            break
          case 'stopped':
            isPlaying = false
            transportBusy = false
            break
          case 'error':
            isPlaying = false
            transportBusy = false
            transportMessage = payload.message ?? 'live preview failed'
            break
        }
      })

      await getCurrentWindow().onCloseRequested(async (event) => {
        event.preventDefault()
        const dirtyDocs = documents.filter((doc) => doc.dirty)
        if (dirtyDocs.length === 0) {
          await finalizeWindowClose()
          return
        }
        await maybeCloseWindow()
      })

      const untouchedStarterTabs = documents.every((doc) => !doc.path && !doc.dirty)
      if (untouchedStarterTabs) {
        try {
          const session = await loadSessionDocuments()
          if (session.documents.length > 0) {
            documents = session.documents.map((doc) => makeDocumentRecord(doc))
            activeId =
              documents.find((doc) => doc.path === session.activePath)?.id ??
              documents[0].id
            transportMessage = 'restored previous session'
          } else {
            const examples = await loadDevelopmentExamples()
            if (examples.length > 0) {
              documents = examples.map((example) => makeDocumentRecord(example))
              activeId = documents[0].id
              transportMessage = 'loaded development examples'
            }
          }
        } catch (error) {
          transportMessage = error instanceof Error ? error.message : String(error)
        }
      }
      await refreshRecentFiles()
      for (const doc of documents) {
        void runValidation(doc.id)
      }
    })()

    return () => {
      window.removeEventListener('keydown', onKeydown)
      unlistenPlaybackEvents?.()
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
      <button class="button ghost" on:click={saveActiveDocumentAs}>Save As</button>
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
        <div class="recent-block">
          <p class="panel-title">Recent Files</p>
          {#if recentFiles.length === 0}
            <p class="panel-note">
              Recent `.sbg` and `.sbgf` files will appear here after you open
              or save them.
            </p>
          {:else}
            <div class="recent-list">
              {#each recentFiles as entry}
                <button class="recent-button" on:click={() => void openRecentFile(entry.path)}>
                  <span class="recent-name">{entry.name}</span>
                  <span class="recent-type">{entry.kind}</span>
                </button>
              {/each}
            </div>
          {/if}
        </div>
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
      {#if activeDocument?.kind === 'sbgf'}
        <CurveInfoCard
          info={activeCurveInfo}
          error={activeCurveInfoError}
          validating={curveInfoLoadingId === activeDocument?.id}
        />
      {:else}
        <BeatPreviewChart
          preview={activeBeatPreview}
          error={activeBeatPreviewError}
          kind={activeDocument?.kind ?? null}
          validating={previewLoadingId === activeDocument?.id}
        />
      {/if}

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
                  {#if item.endLine || item.endColumn}
                    →L{item.endLine ?? item.line}:C{item.endColumn ?? item.column}
                  {/if}
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
          <li>`.sbg` beat preview sampling</li>
          <li>`.sbgf` curve metadata inspector</li>
        </ul>
      </div>
    </aside>
  </main>
</div>
