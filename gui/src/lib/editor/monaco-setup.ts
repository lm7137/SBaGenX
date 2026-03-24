import type * as Monaco from 'monaco-editor'

let registered = false

function registerSbgLanguage(monaco: typeof Monaco) {
  monaco.languages.register({ id: 'sbg' })
  monaco.languages.setMonarchTokensProvider('sbg', {
    tokenizer: {
      root: [
        [/^\s*(#|;|\/\/).*$/, 'comment'],
        [/^\s*-[A-Za-z](?:[^\n]*)$/, 'keyword'],
        [/^\s*[A-Za-z_][\w-]*\s*:/, 'type.identifier'],
        [/\bNOW\b/, 'keyword'],
        [/\+\d{2}:\d{2}(?::\d{2})?/, 'number'],
        [/\b\d{1,4}(?:\.\d+)?(?:[@+]\d+(?:\.\d+)?(?:\/\d+(?:\.\d+)?)?)?/, 'number'],
        [/\b(?:alloff|mixspin|mixpulse|mixbeat|mix)\b/, 'keyword'],
        [/\b(?:wave|custom)\d{2}\b/, 'type'],
        [/->/, 'operator'],
        [/"/, { token: 'string.quote', bracket: '@open', next: '@string' }],
      ],
      string: [
        [/[^\\"]+/, 'string'],
        [/\\./, 'string.escape'],
        [/"/, { token: 'string.quote', bracket: '@close', next: '@pop' }],
      ],
    },
  })
}

function registerSbgfLanguage(monaco: typeof Monaco) {
  monaco.languages.register({ id: 'sbgf' })
  monaco.languages.setMonarchTokensProvider('sbgf', {
    tokenizer: {
      root: [
        [/^\s*(#|;|\/\/).*$/, 'comment'],
        [
          /\b(?:title|start|target|duration|curve|carrier|beat|amp|mixamp|solve|if|then|else)\b/,
          'keyword',
        ],
        [/[A-Za-z_][\w-]*(?=\s*=)/, 'type.identifier'],
        [/\b\d+(?:\.\d+)?\b/, 'number'],
        [/[=:+\-*/^()]/, 'operator'],
        [/"/, { token: 'string.quote', bracket: '@open', next: '@string' }],
      ],
      string: [
        [/[^\\"]+/, 'string'],
        [/\\./, 'string.escape'],
        [/"/, { token: 'string.quote', bracket: '@close', next: '@pop' }],
      ],
    },
  })
}

function registerTheme(monaco: typeof Monaco) {
  monaco.editor.defineTheme('sbagenx-light', {
    base: 'vs',
    inherit: true,
    rules: [
      { token: 'comment', foreground: '7a6f64' },
      { token: 'keyword', foreground: '0f5f9c', fontStyle: 'bold' },
      { token: 'type', foreground: '8a4f12' },
      { token: 'type.identifier', foreground: '0d3d63', fontStyle: 'bold' },
      { token: 'number', foreground: '146c4b' },
      { token: 'operator', foreground: '8d3f1b' },
      { token: 'string', foreground: '8f2b5f' },
    ],
    colors: {
      'editor.background': '#fffaf3',
      'editorLineNumber.foreground': '#918677',
      'editor.lineHighlightBackground': '#f5ecdf',
      'editor.selectionBackground': '#d7e8f7',
      'editor.inactiveSelectionBackground': '#e9decf',
    },
  })
}

export function ensureMonacoSetup(monaco: typeof Monaco) {
  if (registered) return
  registerSbgLanguage(monaco)
  registerSbgfLanguage(monaco)
  registerTheme(monaco)
  registered = true
}
