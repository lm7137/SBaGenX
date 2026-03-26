<script lang="ts">
  import { onDestroy, onMount } from 'svelte'
  import type { ValidationDiagnostic } from '../types'
  import { ensureMonacoSetup } from './monaco-setup'

  export let docId: string
  export let language: 'sbg' | 'sbgf'
  export let value = ''
  export let diagnostics: ValidationDiagnostic[] = []
  export let readOnly = false
  export let onChange: (value: string) => void = () => {}

  let host: HTMLDivElement
  let editor: import('monaco-editor').editor.IStandaloneCodeEditor | null = null
  let monacoRef: typeof import('monaco-editor') | null = null
  let applyingExternalValue = false
  let decorationsCollection: import('monaco-editor').editor.IEditorDecorationsCollection | null = null

  function asPositiveInt(value: unknown): number | null {
    const n = typeof value === 'number' ? value : Number(value)
    if (!Number.isFinite(n)) return null
    const i = Math.floor(n)
    return i > 0 ? i : null
  }

  export function revealPosition(line: number, column = 1) {
    if (!editor) return
    const model = editor.getModel()
    if (!model) return
    const safeLine = Math.max(1, Math.min(Math.floor(line), model.getLineCount()))
    const safeColumn = Math.max(1, Math.min(Math.floor(column), model.getLineMaxColumn(safeLine)))
    editor.focus()
    editor.setPosition({ lineNumber: safeLine, column: safeColumn })
    editor.setSelection({
      startLineNumber: safeLine,
      startColumn: safeColumn,
      endLineNumber: safeLine,
      endColumn: safeColumn,
    })
    editor.revealPositionInCenter({ lineNumber: safeLine, column: safeColumn })
  }

  export function focusEditor() {
    editor?.focus()
  }

  function applyMarkers() {
    if (!editor || !monacoRef) return
    const model = editor.getModel()
    if (!model) return
    const markerEntries = diagnostics
      .map((diag) => {
        const startLine = asPositiveInt(diag.line)
        if (!startLine) return null
        const startColumn = asPositiveInt(diag.column) ?? 1
        const endLine = asPositiveInt(diag.endLine) ?? startLine
        const endColumn = asPositiveInt(diag.endColumn) ?? startColumn + 1
        return {
          severity:
            diag.severity === 'error'
              ? monacoRef!.MarkerSeverity.Error
              : monacoRef!.MarkerSeverity.Warning,
          message: diag.message,
          startLineNumber: startLine,
          startColumn,
          endLineNumber: endLine,
          endColumn: endLine === startLine && endColumn <= startColumn
            ? startColumn + 1
            : endColumn,
        }
      })
      .filter((diag): diag is {
        severity:
          | import('monaco-editor').MarkerSeverity.Error
          | import('monaco-editor').MarkerSeverity.Warning
        message: string
        startLineNumber: number
        startColumn: number
        endLineNumber: number
        endColumn: number
      } => !!diag)
    monacoRef.editor.setModelMarkers(
      model,
      'sbagenx',
      markerEntries,
    )

    decorationsCollection?.set(
      markerEntries.map((diag) => ({
        range: new monacoRef!.Range(
          diag.startLineNumber,
          diag.startColumn,
          diag.endLineNumber,
          diag.endColumn,
        ),
        options: {
          className:
            diag.severity === monacoRef!.MarkerSeverity.Error
              ? 'sbagenx-error-decoration'
              : 'sbagenx-warning-decoration',
          inlineClassName:
            diag.severity === monacoRef!.MarkerSeverity.Error
              ? 'sbagenx-error-range'
              : 'sbagenx-warning-range',
          inlineClassNameAffectsLetterSpacing: true,
          zIndex: 30,
          hoverMessage: [{ value: diag.message }],
        },
      })),
    )
  }

  onMount(async () => {
    const monaco = await import('monaco-editor')
    ensureMonacoSetup(monaco)
    monacoRef = monaco
    ;(window as Window & { MonacoEnvironment?: unknown }).MonacoEnvironment = {
      getWorker() {
        return new Worker(
          new URL('monaco-editor/esm/vs/editor/editor.worker.js', import.meta.url),
          { type: 'module' },
        )
      },
    }

    editor = monaco.editor.create(host, {
      value,
      language,
      theme: 'sbagenx-light',
      renderValidationDecorations: 'on',
      automaticLayout: true,
      minimap: { enabled: false },
      fontFamily: '"Iosevka Term", "Cascadia Code", Consolas, monospace',
      fontLigatures: false,
      fontSize: 14,
      lineHeight: 24,
      padding: { top: 16, bottom: 16 },
      smoothScrolling: true,
      scrollBeyondLastLine: false,
      renderWhitespace: 'selection',
      tabSize: 2,
      readOnly,
    })

    decorationsCollection = editor.createDecorationsCollection()

    editor.onDidChangeModelContent(() => {
      if (!editor || applyingExternalValue) return
      onChange(editor.getValue())
    })

    applyMarkers()
  })

  $: if (editor && editor.getModel()?.getLanguageId() !== language) {
    monacoRef?.editor.setModelLanguage(editor.getModel()!, language)
  }

  $: if (editor) {
    const current = editor.getValue()
    if (value !== current) {
      applyingExternalValue = true
      editor.setValue(value)
      applyingExternalValue = false
    }
  }

  $: if (editor && monacoRef && decorationsCollection) {
    diagnostics
    applyMarkers()
  }

  onDestroy(() => {
    decorationsCollection?.clear()
    editor?.dispose()
  })
</script>

<div class="monaco-host" bind:this={host} data-doc-id={docId}></div>
