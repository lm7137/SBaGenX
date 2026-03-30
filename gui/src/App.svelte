<script lang='ts'>
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
    exportProgram,
    inspectCurveInfo,
    loadDevelopmentExamples,
    loadRecentFiles,
    loadSessionDocuments,
    readTextFile,
    sampleBeatPreview,
    sampleProgramBeatPreview,
    saveSessionState,
    startLivePreview,
    startProgramLivePreview,
    stopLivePreview,
    validateDocument,
    validateProgram,
    writeTextFile,
  } from './lib/backend'
  import type {
    BeatPreviewResult,
    CurveInfoResult,
    DocumentKind,
    DocumentRecord,
    PlaybackEvent,
    ProgramKind,
    ProgramRuntimeRequest,
    RecentFileEntry,
    RuntimeMode,
    ValidationDiagnostic,
  } from './lib/types'

  const DEFAULT_PROGRAM_MAIN_ARGS: Record<ProgramKind, string> = {
    drop: '00ds+^',
    sigmoid: '00ds+^:l=0.125:h=0',
    slide: '200+10/20',
    curve: '00ls:l=0.15',
  }

  const bootstrappedExampleSuffixes = new Set([
    '/examples/plus/creativity-boost.sbg',
    '/examples/basics/curve-expfit-solve-demo.sbgf',
  ])

  type LoadedSource = {
    path: string | null
    name: string
    kind: DocumentKind
    content: string
  }

  type ValidationTask =
    | { mode: 'sequence'; id: string }
    | { mode: 'program' }

  let runtimeMode: RuntimeMode = 'program'
  let documents: DocumentRecord[] = [makeUntitledDocument('sbg')]
  let activeId = documents[0].id
  let programKind: ProgramKind = 'drop'
  let programMainArgs: Record<ProgramKind, string> = { ...DEFAULT_PROGRAM_MAIN_ARGS }
  let programDropMinutes = 30
  let programHoldMinutes = 30
  let programWakeMinutes = 3
  let programMixPath: string | null = null
  let programDiagnostics: ValidationDiagnostic[] = []
  let programBeatPreview: BeatPreviewResult | null = null
  let programBeatPreviewError: string | null = null
  let programCurveDocument: DocumentRecord = makeUntitledDocument('sbgf')
  let validationTimer: ReturnType<typeof setTimeout> | null = null
  let pendingValidationTask: ValidationTask | null = null
  let validatingTarget: string | null = null
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

  function normalizeExamplePath(path: string | null): string {
    return (path ?? '').replace(/\\/g, '/').toLowerCase()
  }

  function isBootstrappedExampleSession(docs: Array<{ path: string | null }>): boolean {
    return (
      docs.length > 0 &&
      docs.every((doc) => {
        const normalized = normalizeExamplePath(doc.path)
        return Array.from(bootstrappedExampleSuffixes).some((suffix) => normalized.endsWith(suffix))
      })
    )
  }

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
      mixPathOverride: null,
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

  function makeDocumentRecord(source: LoadedSource): DocumentRecord {
    return {
      id: `doc-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`,
      name: source.name,
      path: source.path,
      kind: source.kind,
      mixPathOverride: null,
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

  function splitLoadedSources(sources: LoadedSource[]) {
    const sequenceDocs = sources
      .filter((source) => source.kind === 'sbg')
      .map((source) => makeDocumentRecord(source))
    const curveSource = sources.find((source) => source.kind === 'sbgf')
    return {
      sequenceDocs,
      curveDoc: curveSource ? makeDocumentRecord(curveSource) : null,
    }
  }

  function ensureSequenceDocuments(nextDocs: DocumentRecord[]): DocumentRecord[] {
    return nextDocs.length > 0 ? nextDocs : [makeUntitledDocument('sbg')]
  }

  function updateDocument(id: string, patch: Partial<DocumentRecord>) {
    documents = documents.map((doc) => {
      if (doc.id !== id) return doc
      const merged = { ...doc, ...patch }
      merged.lines = merged.content.split('\n')
      return merged
    })
  }

  function updateProgramCurveDocument(patch: Partial<DocumentRecord>) {
    const merged = { ...programCurveDocument, ...patch }
    merged.lines = merged.content.split('\n')
    programCurveDocument = merged
  }

  function upsertSequenceDocument(next: DocumentRecord) {
    const existingIndex = documents.findIndex((doc) => doc.id === next.id)
    if (existingIndex === -1) {
      documents = [...documents, next]
      return
    }
    documents = documents.map((doc) => (doc.id === next.id ? next : doc))
  }

  function currentDocumentMatches(id: string, content: string) {
    const current = documents.find((doc) => doc.id === id)
    return !!current && current.content === content
  }

  function currentProgramCurveMatches(content: string) {
    return programCurveDocument.content === content
  }

  function parseMinutesField(value: number, fallbackMinutes: number): number {
    const parsed = Number(value)
    if (!Number.isFinite(parsed) || parsed < 0) return Math.round(fallbackMinutes * 60)
    return Math.round(parsed * 60)
  }

  function buildProgramRequest(): ProgramRuntimeRequest {
    return {
      programKind,
      mainArg: programMainArgs[programKind],
      dropTimeSec: parseMinutesField(programDropMinutes, 30),
      holdTimeSec: parseMinutesField(programHoldMinutes, 30),
      wakeTimeSec: parseMinutesField(programWakeMinutes, 3),
      curveText: programKind === 'curve' ? programCurveDocument.content : null,
      sourceName:
        programKind === 'curve'
          ? programCurveDocument.path ?? programCurveDocument.name
          : `program:${programKind}`,
      mixPath: programMixPath,
    }
  }

  function programSnapshot(request: ProgramRuntimeRequest = buildProgramRequest()) {
    return JSON.stringify(request)
  }

  function currentProgramMatches(snapshot: string) {
    return programSnapshot() === snapshot
  }

  function activateDocument(id: string) {
    activeId = id
    runtimeMode = 'sequence'
    selectedDiagnosticId = null
    void persistSessionState()
    tick().then(() => editorView?.focusEditor())
  }

  function selectRuntimeMode(nextMode: RuntimeMode) {
    runtimeMode = nextMode
    selectedDiagnosticId = null
    if (nextMode === 'sequence') {
      queueSequenceValidation(activeId)
    } else {
      queueProgramValidation()
    }
    tick().then(() => editorView?.focusEditor())
  }

  function selectProgramKind(nextKind: ProgramKind) {
    runtimeMode = 'program'
    programKind = nextKind
    selectedDiagnosticId = null
    queueProgramValidation()
    tick().then(() => editorView?.focusEditor())
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
      if (loaded.kind === 'sbg') {
        const existing = documents.find((doc) => doc.path === loaded.path)
        const next: DocumentRecord = existing
          ? {
              ...existing,
              name: loaded.name,
              path: loaded.path,
              kind: loaded.kind,
              mixPathOverride: existing.mixPathOverride ?? null,
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
        upsertSequenceDocument(next)
        activeId = next.id
        runtimeMode = 'sequence'
        queueSequenceValidation(next.id)
      } else {
        programCurveDocument = {
          ...makeDocumentRecord(loaded),
          id: programCurveDocument.id,
        }
        runtimeMode = 'program'
        programKind = 'curve'
        queueProgramValidation()
      }
    }
    await refreshRecentFiles()
    await persistSessionState()
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

  async function writeProgramCurveToPath(path: string, content: string) {
    await writeTextFile(path, content)
    updateProgramCurveDocument({
      path,
      name: path.split(/[\\/]/).pop() ?? path,
      dirty: false,
    })
    await refreshRecentFiles()
    transportMessage = `saved ${path.split(/[\\/]/).pop() ?? path}`
  }

  async function saveActiveSequenceDocument() {
    if (!activeDocument) return
    let path = activeDocument.path
    if (!path) {
      path = await promptSavePath(activeDocument)
      if (!path) return
    }
    await writeDocumentToPath(activeDocument.id, path, activeDocument.content)
  }

  async function saveActiveSequenceDocumentAs() {
    if (!activeDocument) return
    const path = await promptSavePath(activeDocument)
    if (!path) return
    await writeDocumentToPath(activeDocument.id, path, activeDocument.content)
  }

  async function saveProgramCurveDocument() {
    if (programKind !== 'curve') return
    let path = programCurveDocument.path
    if (!path) {
      path = await promptSavePath(programCurveDocument)
      if (!path) return
    }
    await writeProgramCurveToPath(path, programCurveDocument.content)
  }

  async function saveProgramCurveDocumentAs() {
    if (programKind !== 'curve') return
    const path = await promptSavePath(programCurveDocument)
    if (!path) return
    await writeProgramCurveToPath(path, programCurveDocument.content)
  }

  async function saveCurrentTarget() {
    if (runtimeMode === 'sequence') {
      await saveActiveSequenceDocument()
      return
    }
    if (programKind === 'curve') {
      await saveProgramCurveDocument()
      return
    }
    transportMessage = 'built-in program settings are not file-backed yet'
  }

  async function saveCurrentTargetAs() {
    if (runtimeMode === 'sequence') {
      await saveActiveSequenceDocumentAs()
      return
    }
    if (programKind === 'curve') {
      await saveProgramCurveDocumentAs()
      return
    }
    transportMessage = 'built-in program settings are not file-backed yet'
  }

  async function saveSequenceDocumentById(id: string) {
    const doc = documents.find((item) => item.id === id)
    if (!doc) return false
    const path = doc.path ?? (await promptSavePath(doc))
    if (!path) return false
    await writeDocumentToPath(id, path, doc.content)
    return true
  }

  async function saveProgramCurveIfNeeded() {
    if (!programCurveDocument.dirty) return true
    const path = programCurveDocument.path ?? (await promptSavePath(programCurveDocument))
    if (!path) return false
    await writeProgramCurveToPath(path, programCurveDocument.content)
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
    const paths = documents.map((doc) => doc.path).filter((path): path is string => !!path)
    const activePath =
      runtimeMode === 'sequence'
        ? documents.find((doc) => doc.id === activeId)?.path ?? null
        : null
    await saveSessionState(paths, activePath)
  }

  async function openRecentFile(path: string) {
    try {
      const loaded = await readTextFile(path)
      if (loaded.kind === 'sbg') {
        const existing = documents.find((doc) => doc.path === loaded.path)
        const next = existing
          ? {
              ...existing,
              name: loaded.name,
              path: loaded.path,
              kind: loaded.kind,
              mixPathOverride: existing.mixPathOverride ?? null,
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
        upsertSequenceDocument(next)
        activeId = next.id
        runtimeMode = 'sequence'
        queueSequenceValidation(next.id)
      } else {
        programCurveDocument = {
          ...makeDocumentRecord(loaded),
          id: programCurveDocument.id,
        }
        runtimeMode = 'program'
        programKind = 'curve'
        queueProgramValidation()
      }
      await refreshRecentFiles()
      await persistSessionState()
    } catch (error) {
      transportMessage = error instanceof Error ? error.message : String(error)
      await refreshRecentFiles()
    }
  }

  function createSequenceDocument() {
    const doc = makeUntitledDocument('sbg')
    documents = [...documents, doc]
    activeId = doc.id
    runtimeMode = 'sequence'
    selectedDiagnosticId = null
    queueSequenceValidation(doc.id)
    void persistSessionState()
    tick().then(() => editorView?.focusEditor())
  }

  function resetProgramCurveDocument() {
    programCurveDocument = makeUntitledDocument('sbgf')
    runtimeMode = 'program'
    programKind = 'curve'
    selectedDiagnosticId = null
    queueProgramValidation()
    tick().then(() => editorView?.focusEditor())
  }

  async function closeDocument(id: string) {
    const doc = documents.find((item) => item.id === id)
    if (!doc) return

    if (doc.dirty) {
      const approved = await confirm(`Close ${doc.name} without saving your changes?`, {
        title: 'Unsaved changes',
        kind: 'warning',
        okLabel: 'Close anyway',
        cancelLabel: 'Keep editing',
      })
      if (!approved) return
    }

    if (runtimeMode === 'sequence' && activeId === id) {
      await stopPlayback(false)
    }

    const currentIndex = documents.findIndex((item) => item.id === id)
    const remaining = documents.filter((item) => item.id !== id)
    if (remaining.length === 0) {
      const fallback = makeUntitledDocument('sbg')
      documents = [fallback]
      activeId = fallback.id
      runtimeMode = 'sequence'
      transportMessage = `closed ${doc.name}`
      selectedDiagnosticId = null
      queueSequenceValidation(fallback.id)
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
    if (runtimeMode === 'sequence') {
      if (!activeDocument) return
      updateDocument(activeDocument.id, {
        content: nextValue,
        dirty: true,
        beatPreview: null,
        beatPreviewError: null,
        curveInfo: null,
        curveInfoError: null,
      })
      queueSequenceValidation(activeDocument.id)
      return
    }

    if (programKind !== 'curve') return
    updateProgramCurveDocument({
      content: nextValue,
      dirty: true,
      curveInfo: null,
      curveInfoError: null,
    })
    programBeatPreview = null
    programBeatPreviewError = null
    queueProgramValidation()
  }

  function queueSequenceValidation(id: string) {
    if (validationTimer) clearTimeout(validationTimer)
    validatingTarget = id
    pendingValidationTask = { mode: 'sequence', id }
    validationTimer = setTimeout(() => {
      const task = pendingValidationTask
      pendingValidationTask = null
      if (!task) return
      if (task.mode === 'sequence') {
        void runSequenceValidation(task.id)
      } else {
        void runProgramValidation()
      }
    }, 300)
  }

  function queueProgramValidation() {
    if (validationTimer) clearTimeout(validationTimer)
    validatingTarget = 'program'
    pendingValidationTask = { mode: 'program' }
    validationTimer = setTimeout(() => {
      const task = pendingValidationTask
      pendingValidationTask = null
      if (!task) return
      if (task.mode === 'sequence') {
        void runSequenceValidation(task.id)
      } else {
        void runProgramValidation()
      }
    }, 300)
  }

  async function runSequenceValidation(id: string) {
    const doc = documents.find((item) => item.id === id)
    if (!doc) return
    const contentSnapshot = doc.content
    try {
      const result = await validateDocument(
        doc.kind,
        doc.content,
        doc.path ?? doc.name,
        doc.kind === 'sbg' ? doc.mixPathOverride : null,
      )
      if (!currentDocumentMatches(doc.id, contentSnapshot)) return
      updateDocument(doc.id, {
        diagnostics: result.diagnostics,
        beatPreview: result.valid && doc.kind === 'sbg' ? doc.beatPreview : null,
        beatPreviewError: null,
        curveInfo: null,
        curveInfoError: null,
      })
      if (result.valid && doc.kind === 'sbg') {
        void loadSequenceBeatPreview(doc.id, contentSnapshot, doc.path ?? doc.name)
      }
    } catch (error) {
      if (!currentDocumentMatches(doc.id, contentSnapshot)) return
      const msg = error instanceof Error ? error.message : String(error)
      updateDocument(doc.id, {
        diagnostics: [
          {
            id: `diag-${doc.id}-bridge`,
            documentId: doc.id,
            severity: 'error',
            message: msg,
          },
        ],
        beatPreview: null,
        beatPreviewError: null,
        curveInfo: null,
        curveInfoError: null,
      })
    } finally {
      if (validatingTarget === id) {
        validatingTarget = null
      }
    }
  }

  async function runProgramValidation() {
    const request = buildProgramRequest()
    const snapshot = programSnapshot(request)
    try {
      const result = await validateProgram(request)
      if (!currentProgramMatches(snapshot)) return

      if (programKind === 'curve') {
        updateProgramCurveDocument({
          diagnostics: result.diagnostics.filter((diag) => !!diag.line),
          curveInfo: result.valid ? programCurveDocument.curveInfo : null,
          curveInfoError: null,
        })
        programDiagnostics = result.diagnostics.filter((diag) => !diag.line)
      } else {
        updateProgramCurveDocument({ diagnostics: [] })
        programDiagnostics = result.diagnostics
      }

      programBeatPreview = result.valid ? programBeatPreview : null
      programBeatPreviewError = null
      if (!result.valid) {
        updateProgramCurveDocument({ curveInfo: null, curveInfoError: null })
        return
      }

      void loadProgramBeatPreview(snapshot, request)
      if (programKind === 'curve') {
        void loadProgramCurveInfo(snapshot, request.curveText ?? '', request.sourceName ?? 'curve.sbgf')
      } else {
        updateProgramCurveDocument({ curveInfo: null, curveInfoError: null })
      }
    } catch (error) {
      if (!currentProgramMatches(snapshot)) return
      const msg = error instanceof Error ? error.message : String(error)
      programDiagnostics = [
        {
          id: 'diag-program-bridge',
          documentId: programKind === 'curve' ? programCurveDocument.id : `program:${programKind}`,
          severity: 'error',
          message: msg,
        },
      ]
      updateProgramCurveDocument({
        diagnostics: [],
        curveInfo: null,
        curveInfoError: null,
      })
      programBeatPreview = null
      programBeatPreviewError = null
    } finally {
      if (validatingTarget === 'program') {
        validatingTarget = null
      }
    }
  }

  async function loadSequenceBeatPreview(id: string, text: string, sourceName: string) {
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

  async function loadProgramBeatPreview(snapshot: string, request: ProgramRuntimeRequest) {
    previewLoadingId = 'program'
    try {
      const preview = await sampleProgramBeatPreview(request)
      if (!currentProgramMatches(snapshot)) return
      programBeatPreview = preview
      programBeatPreviewError = null
    } catch (error) {
      if (!currentProgramMatches(snapshot)) return
      programBeatPreview = null
      programBeatPreviewError = error instanceof Error ? error.message : String(error)
    } finally {
      if (previewLoadingId === 'program') {
        previewLoadingId = null
      }
    }
  }

  async function loadProgramCurveInfo(snapshot: string, text: string, sourceName: string) {
    curveInfoLoadingId = 'program-curve'
    try {
      const info = await inspectCurveInfo(text, sourceName)
      if (!currentProgramMatches(snapshot) || !currentProgramCurveMatches(text)) return
      updateProgramCurveDocument({
        curveInfo: info,
        curveInfoError: null,
      })
    } catch (error) {
      if (!currentProgramMatches(snapshot) || !currentProgramCurveMatches(text)) return
      updateProgramCurveDocument({
        curveInfo: null,
        curveInfoError: error instanceof Error ? error.message : String(error),
      })
    } finally {
      if (curveInfoLoadingId === 'program-curve') {
        curveInfoLoadingId = null
      }
    }
  }

  async function playCurrentTarget() {
    transportBusy = true
    transportMessage = 'starting live preview...'
    try {
      await stopPlayback(false)
      const result =
        runtimeMode === 'sequence'
          ? await startLivePreview(
              activeDocument.content,
              activeDocument.path ?? activeDocument.name,
              activeDocument.mixPathOverride,
            )
          : await startProgramLivePreview(buildProgramRequest())
      isPlaying = true
      transportMessage = result.durationSec > 0 ? `live preview · ${formatDuration(result.durationSec)}` : 'live preview running'
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

  async function exportCurrentTarget() {
    const defaultName =
      runtimeMode === 'sequence'
        ? activeDocument.name.replace(/\.sbg$/i, '.wav')
        : programKind === 'curve'
          ? programCurveDocument.name.replace(/\.sbgf$/i, '.wav')
          : `${programKind}.wav`

    const target = await save({
      defaultPath: defaultName,
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
      const result =
        runtimeMode === 'sequence'
          ? await exportDocument(
              activeDocument.content,
              target,
              activeDocument.path ?? activeDocument.name,
              activeDocument.mixPathOverride,
            )
          : await exportProgram(buildProgramRequest(), target)
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

  async function loadMixForSequenceDocument() {
    if (!activeDocument) return
    const selection = await open({
      multiple: false,
      filters: [{ name: 'Audio files', extensions: ['wav', 'flac', 'ogg', 'mp3'] }],
    })
    if (!selection || Array.isArray(selection)) return

    updateDocument(activeDocument.id, {
      mixPathOverride: selection,
      beatPreview: null,
      beatPreviewError: null,
    })
    transportMessage = `loaded mix ${selection.split(/[\\/]/).pop() ?? selection}`
    queueSequenceValidation(activeDocument.id)
  }

  function clearMixForSequenceDocument() {
    if (!activeDocument?.mixPathOverride) return
    updateDocument(activeDocument.id, {
      mixPathOverride: null,
      beatPreview: null,
      beatPreviewError: null,
    })
    transportMessage = 'cleared loaded mix'
    queueSequenceValidation(activeDocument.id)
  }

  async function loadMixForProgram() {
    const selection = await open({
      multiple: false,
      filters: [{ name: 'Audio files', extensions: ['wav', 'flac', 'ogg', 'mp3'] }],
    })
    if (!selection || Array.isArray(selection)) return
    programMixPath = selection
    programBeatPreview = null
    programBeatPreviewError = null
    transportMessage = `loaded mix ${selection.split(/[\\/]/).pop() ?? selection}`
    queueProgramValidation()
  }

  function clearMixForProgram() {
    if (!programMixPath) return
    programMixPath = null
    programBeatPreview = null
    programBeatPreviewError = null
    transportMessage = 'cleared loaded mix'
    queueProgramValidation()
  }

  async function maybeCloseWindow() {
    const dirtyDocs = [...documents.filter((doc) => doc.dirty)]
    if (programCurveDocument.dirty) {
      dirtyDocs.push(programCurveDocument)
    }

    if (dirtyDocs.length === 0) {
      await finalizeWindowClose()
      return
    }

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
      for (const doc of documents.filter((item) => item.dirty)) {
        activeId = doc.id
        await tick()
        const saved = await saveSequenceDocumentById(doc.id)
        if (!saved) {
          transportMessage = `close cancelled for ${doc.name}`
          return
        }
      }
      if (programCurveDocument.dirty) {
        const saved = await saveProgramCurveIfNeeded()
        if (!saved) {
          transportMessage = `close cancelled for ${programCurveDocument.name}`
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
        void saveCurrentTargetAs()
      } else {
        void saveCurrentTarget()
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
      if (runtimeMode === 'program' && programKind === 'curve' && event.shiftKey) {
        resetProgramCurveDocument()
      } else {
        createSequenceDocument()
      }
      return
    }

    if (key === 'w' && runtimeMode === 'sequence' && activeDocument) {
      event.preventDefault()
      void closeDocument(activeDocument.id)
    }
  }

  $: activeDocument = documents.find((doc) => doc.id === activeId) ?? documents[0]
  $: programCurveActive = runtimeMode === 'program' && programKind === 'curve'
  $: currentProgramMainArg = programMainArgs[programKind]
  $: activeDiagnostics =
    runtimeMode === 'sequence'
      ? activeDocument?.diagnostics ?? []
      : [
          ...(programCurveActive ? programCurveDocument.diagnostics : []),
          ...programDiagnostics,
        ]
  $: activeBeatPreview = runtimeMode === 'sequence' ? activeDocument?.beatPreview ?? null : programBeatPreview
  $: activeBeatPreviewError =
    runtimeMode === 'sequence' ? activeDocument?.beatPreviewError ?? null : programBeatPreviewError
  $: activeCurveInfo = programCurveActive ? programCurveDocument.curveInfo ?? null : null
  $: activeCurveInfoError = programCurveActive ? programCurveDocument.curveInfoError ?? null : null
  $: canRunCurrent =
    runtimeMode === 'sequence'
      ? !!activeDocument && activeDiagnostics.length === 0 && !transportBusy
      : activeDiagnostics.length === 0 && !transportBusy
  $: saveDisabled = runtimeMode === 'program' && programKind !== 'curve'
  $: activeStatusPath =
    runtimeMode === 'sequence'
      ? activeDocument?.path ?? 'unsaved'
      : programCurveActive
        ? programCurveDocument.path ?? 'unsaved'
        : `program:${programKind}`
  $: activeMixLabel = runtimeMode === 'sequence' ? activeDocument?.mixPathOverride : programMixPath

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
        await maybeCloseWindow()
      })

      const untouchedStarterTabs = documents.every((doc) => !doc.path && !doc.dirty)
      if (untouchedStarterTabs) {
        try {
          const session = await loadSessionDocuments()
          if (session.documents.length > 0) {
            if (import.meta.env.DEV && isBootstrappedExampleSession(session.documents)) {
              const examples = await loadDevelopmentExamples()
              if (examples.length > 0) {
                const split = splitLoadedSources(examples)
                documents = ensureSequenceDocuments(split.sequenceDocs)
                activeId = documents[0].id
                if (split.curveDoc) {
                  programCurveDocument = { ...split.curveDoc, id: programCurveDocument.id }
                }
                transportMessage = 'loaded example files'
              }
            } else {
              const split = splitLoadedSources(session.documents)
              documents = ensureSequenceDocuments(split.sequenceDocs)
              activeId = documents.find((doc) => doc.path === session.activePath)?.id ?? documents[0].id
              if (split.curveDoc) {
                programCurveDocument = { ...split.curveDoc, id: programCurveDocument.id }
              }
              transportMessage = 'restored previous session'
            }
          } else {
            const examples = await loadDevelopmentExamples()
            if (examples.length > 0) {
              const split = splitLoadedSources(examples)
              documents = ensureSequenceDocuments(split.sequenceDocs)
              activeId = documents[0].id
              if (split.curveDoc) {
                programCurveDocument = { ...split.curveDoc, id: programCurveDocument.id }
              }
              transportMessage = 'loaded example files'
            }
          }
        } catch (error) {
          transportMessage = error instanceof Error ? error.message : String(error)
        }
      }

      await refreshRecentFiles()
      for (const doc of documents) {
        void runSequenceValidation(doc.id)
      }
      void runProgramValidation()
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
    name='description'
    content='Library-backed desktop editor and player for .sbg files and built-in programs.'
  />
</svelte:head>

<div class='bg-overlay' aria-hidden='true'></div>
<div class='noise-overlay' aria-hidden='true'></div>

<div class='shell'>
  <header class='topbar'>
    <div class='brand'>
      <img class='brand-logo' src={logoUrl} alt='SBaGenX' />
      <div class='brand-copy'>
        <h1>SBAGENX DESKTOP</h1>
        <p class='brand-subtitle'>Library-backed editor, built-in programs, playback, and export.</p>
      </div>
    </div>

    <div class='toolbar-stack'>
      <div class='mode-selector' role='radiogroup' aria-label='Runtime mode'>
        <label class:active={runtimeMode === 'program'} class='mode-option'>
          <input
            type='radio'
            name='runtime-mode'
            value='program'
            checked={runtimeMode === 'program'}
            on:change={() => selectRuntimeMode('program')}
          />
          <span>Built-in Program</span>
        </label>
        <label class:active={runtimeMode === 'sequence'} class='mode-option'>
          <input
            type='radio'
            name='runtime-mode'
            value='sequence'
            checked={runtimeMode === 'sequence'}
            on:change={() => selectRuntimeMode('sequence')}
          />
          <span>Sequence File</span>
        </label>
      </div>

      <div class='toolbar'>
        <button class='button ghost' on:click={openDocuments}>Open</button>
        <button class='button ghost' disabled={saveDisabled} on:click={saveCurrentTarget}>Save</button>
        <button class='button ghost' disabled={saveDisabled} on:click={saveCurrentTargetAs}>Save As</button>
        {#if runtimeMode === 'sequence'}
          <button class='button ghost' on:click={createSequenceDocument}>New `.sbg`</button>
          <button class='button ghost' on:click={loadMixForSequenceDocument}>Load Mix</button>
          <button
            class='button ghost'
            disabled={!activeDocument?.mixPathOverride}
            on:click={clearMixForSequenceDocument}
          >
            Clear Mix
          </button>
        {:else}
          <button class='button ghost' on:click={loadMixForProgram}>Load Mix</button>
          <button class='button ghost' disabled={!programMixPath} on:click={clearMixForProgram}>
            Clear Mix
          </button>
          {#if programKind === 'curve'}
            <button class='button ghost' on:click={resetProgramCurveDocument}>New `.sbgf`</button>
          {/if}
        {/if}
        <span class='toolbar-divider'></span>
        <button class='button accent' disabled={!canRunCurrent} on:click={playCurrentTarget}>Play</button>
        <button class='button ghost' disabled={!isPlaying} on:click={() => stopPlayback()}>Stop</button>
        <button class='button export' disabled={!canRunCurrent} on:click={exportCurrentTarget}>Export</button>
      </div>
    </div>
  </header>

  <main class:program-mode={runtimeMode === 'program'} class='workspace'>
    {#if runtimeMode === 'sequence'}
      <aside class='sidebar panel'>
        <div class='panel-header'>
          <p class='panel-title'>Documents</p>
        </div>

        <div class='document-list'>
          {#each documents as document}
            <div class:active={document.id === activeId} class='document-row'>
              <button class='document-main' on:click={() => activateDocument(document.id)}>
                <div class='document-name-row'>
                  <span class='document-name'>{document.name}</span>
                  {#if document.dirty}
                    <span class='dirty-indicator'></span>
                  {/if}
                </div>
                <span class='document-type'>{document.kind}</span>
                {#if document.diagnostics.length}
                  <span class='document-diagnostics'>{document.diagnostics.length} issue(s)</span>
                {/if}
              </button>
              <button
                class='document-close'
                title={`Close ${document.name}`}
                aria-label={`Close ${document.name}`}
                on:click|stopPropagation={() => void closeDocument(document.id)}
              >
                ×
              </button>
            </div>
          {/each}
        </div>

        <div class='panel-footer'>
          <div class='recent-block'>
            <p class='panel-title'>Recent Files</p>
            {#if recentFiles.length === 0}
              <p class='panel-note'>
                Recent `.sbg` and `.sbgf` files will appear here after you open or save them.
              </p>
            {:else}
              <div class='recent-list'>
                {#each recentFiles as entry}
                  <button class='recent-button' on:click={() => void openRecentFile(entry.path)}>
                    <span class='recent-name'>{entry.name}</span>
                    <span class='recent-type'>{entry.kind}</span>
                  </button>
                {/each}
              </div>
            {/if}
          </div>
        </div>
      </aside>
    {/if}

    <section class='editor-column'>
      {#if runtimeMode === 'sequence'}
        <section class='editor panel'>
          <div class='panel-header'>
            <div>
              <p class='panel-title'>Sequence Editor</p>
            </div>
            <div class='editor-meta'>
              <span>{activeDocument?.kind ?? 'unknown'}</span>
              <span>{activeStatusPath}</span>
              {#if activeMixLabel}
                <span class='mix-badge'>mix: {activeMixLabel.split(/[\\/]/).pop() ?? activeMixLabel}</span>
              {/if}
              <span class:busy={transportBusy || validatingTarget === activeDocument?.id} class='editor-status'>
                {#if validatingTarget === activeDocument?.id}
                  validating...
                {:else}
                  {transportMessage}
                {/if}
              </span>
            </div>
          </div>

          <div class='editor-surface'>
            {#if activeDocument}
              <MonacoEditor
                bind:this={editorView}
                docId={activeDocument.id}
                language='sbg'
                value={activeDocument.content}
                diagnostics={activeDiagnostics}
                onChange={handleEditorChange}
              />
            {/if}
          </div>
        </section>
      {:else}
        <section class='panel program-panel'>
          <div class='panel-header'>
            <div>
              <p class='panel-title'>Built-in Program</p>
              <p class='panel-subtitle'>Select a program, timing fields, main argument, and optional mix.</p>
            </div>
            <div class='editor-meta'>
              <span>{`program:${programKind}`}</span>
              {#if programCurveActive}
                <span>{activeStatusPath}</span>
              {/if}
              {#if activeMixLabel}
                <span class='mix-badge'>mix: {activeMixLabel.split(/[\\/]/).pop() ?? activeMixLabel}</span>
              {/if}
              <span class:busy={transportBusy || validatingTarget === 'program'} class='editor-status'>
                {#if validatingTarget === 'program'}
                  validating...
                {:else}
                  {transportMessage}
                {/if}
              </span>
            </div>
          </div>

          <div class='program-panel-body'>
            <div class='program-kind-row'>
              {#each ['drop', 'slide', 'sigmoid', 'curve'] as option}
                <button
                  class:active={programKind === option}
                  class='program-kind-button'
                  on:click={() => selectProgramKind(option as ProgramKind)}
                >
                  {option}
                </button>
              {/each}
            </div>

            <div class='program-grid'>
              <label class='field'>
                <span>Drop-Time (min)</span>
                <input type='number' min='0' step='0.5' bind:value={programDropMinutes} on:input={queueProgramValidation} />
              </label>
              <label class='field'>
                <span>Hold-Time (min)</span>
                <input
                  type='number'
                  min='0'
                  step='0.5'
                  bind:value={programHoldMinutes}
                  disabled={programKind === 'slide'}
                  on:input={queueProgramValidation}
                />
              </label>
              <label class='field'>
                <span>Wake-Time (min)</span>
                <input
                  type='number'
                  min='0'
                  step='0.5'
                  bind:value={programWakeMinutes}
                  disabled={programKind === 'slide'}
                  on:input={queueProgramValidation}
                />
              </label>
            </div>

            <label class='field field-full'>
              <span>Main Argument</span>
              <input
                type='text'
                value={currentProgramMainArg}
                on:input={(event) => {
                  const target = event.currentTarget as HTMLInputElement
                  programMainArgs = {
                    ...programMainArgs,
                    [programKind]: target.value,
                  }
                  queueProgramValidation()
                }}
              />
            </label>

            <div class='program-mix-row'>
              <div>
                <p class='panel-title'>Mix Input</p>
                <p class='panel-note'>
                  {#if programMixPath}
                    {programMixPath}
                  {:else}
                    No mix file selected for this program yet.
                  {/if}
                </p>
              </div>
              <div class='program-inline-actions'>
                <button class='button ghost' on:click={loadMixForProgram}>Load Mix</button>
                <button class='button ghost' disabled={!programMixPath} on:click={clearMixForProgram}>Clear Mix</button>
              </div>
            </div>
          </div>
        </section>

        {#if programCurveActive}
          <section class='editor panel'>
            <div class='panel-header'>
              <div>
                <p class='panel-title'>Curve Editor</p>
              </div>
              <div class='editor-meta'>
                <span>sbgf</span>
                <span>{programCurveDocument.path ?? 'unsaved'}</span>
                <span class:busy={curveInfoLoadingId === 'program-curve'} class='editor-status'>
                  {#if curveInfoLoadingId === 'program-curve'}
                    loading curve info...
                  {:else}
                    {programCurveDocument.name}
                  {/if}
                </span>
              </div>
            </div>

            <div class='editor-surface'>
              <MonacoEditor
                bind:this={editorView}
                docId={programCurveDocument.id}
                language='sbgf'
                value={programCurveDocument.content}
                diagnostics={programCurveDocument.diagnostics}
                onChange={handleEditorChange}
              />
            </div>
          </section>
        {/if}
      {/if}
    </section>

    <aside class='inspector panel'>
      <BeatPreviewChart
        preview={activeBeatPreview as BeatPreviewResult | null}
        error={activeBeatPreviewError}
        kind={runtimeMode === 'sequence' ? activeDocument?.kind ?? null : programCurveActive ? 'curve-program' : 'sbg'}
        validating={previewLoadingId === (runtimeMode === 'sequence' ? activeDocument?.id ?? null : 'program')}
      />

      {#if programCurveActive}
        <CurveInfoCard
          info={activeCurveInfo as CurveInfoResult | null}
          error={activeCurveInfoError}
          validating={curveInfoLoadingId === 'program-curve'}
        />
      {/if}

      <div class='panel-header'>
        <div>
          <p class='panel-title'>Diagnostics</p>
          <p class='panel-subtitle'>
            Real-time validation is now using the native `sbagenxlib` bridge.
          </p>
        </div>
        <span class='diagnostic-count'>{activeDiagnostics.length}</span>
      </div>

      <div class='diagnostic-list'>
        {#if activeDiagnostics.length === 0}
          <article class='diagnostic ok'>
            <div class='diagnostic-head'>
              <span class='severity ok'>ok</span>
              <span class='location'>validated</span>
            </div>
            <p class='diagnostic-message'>
              {#if runtimeMode === 'sequence'}
                No validation issues for the active sequence file.
              {:else}
                No validation issues for the active built-in program.
              {/if}
            </p>
          </article>
        {/if}
        {#each activeDiagnostics as item}
          <button
            class:selected={item.id === selectedDiagnosticId}
            class:jumpable={!!item.line}
            class='diagnostic diagnostic-button'
            disabled={!item.line}
            on:click={() => void jumpToDiagnostic(item)}
          >
            <div class='diagnostic-head'>
              <span class:warning={item.severity === 'warning'} class='severity'>{item.severity}</span>
              <span class='location'>
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
            <p class='diagnostic-message'>{item.message}</p>
          </button>
        {/each}
      </div>

      <div class='inspector-block'>
        <p class='panel-title'>Current Scope</p>
        <ul class='milestone-list'>
          <li>Tabbed `.sbg` editing for sequence mode</li>
          <li>Built-in `drop`, `slide`, `sigmoid`, and `curve` programs</li>
          <li>Dedicated `.sbgf` editor for the `curve` program</li>
          <li>Monaco editor integration</li>
          <li>Debounced native validation</li>
          <li>Live playback and export</li>
          <li>Beat preview sampling and curve metadata inspection</li>
        </ul>
      </div>
    </aside>
  </main>
</div>
