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

  function applyMarkers() {
    if (!editor || !monacoRef) return
    const model = editor.getModel()
    if (!model) return
    monacoRef.editor.setModelMarkers(
      model,
      'sbagenx',
      diagnostics.map((diag) => ({
        severity:
          diag.severity === 'error'
            ? monacoRef!.MarkerSeverity.Error
            : monacoRef!.MarkerSeverity.Warning,
        message: diag.message,
        startLineNumber: diag.line || 1,
        startColumn: diag.column || 1,
        endLineNumber: diag.line || 1,
        endColumn: (diag.column || 1) + 1,
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

  $: applyMarkers()

  onDestroy(() => {
    editor?.dispose()
  })
</script>

<div class="monaco-host" bind:this={host} data-doc-id={docId}></div>

<style>
  .monaco-host {
    width: 100%;
    height: 100%;
    min-height: 24rem;
  }
</style>
