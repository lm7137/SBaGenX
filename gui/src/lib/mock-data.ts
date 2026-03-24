import type { DocumentRecord, ValidationDiagnostic } from './types'

const sbgContent = `-SE
-o focus-demo.flac
-Z 12

base: 200+4/20
alloff: -

NOW base
+00:05:00 base ->
+00:08:00 alloff
`

const sbgfContent = `# SBaGenX function curve
title = "One-minute sigmoid demo"
start = 10.0
target = 0.3
duration = 60.0
curve = sigmoid
l = 3.0
`

export const mockDocuments: DocumentRecord[] = [
  {
    id: 'session-main',
    name: 'focus-demo.sbg',
    path: 'examples/gui/focus-demo.sbg',
    kind: 'sbg',
    dirty: true,
    content: sbgContent,
    lines: sbgContent.split('\n'),
  },
  {
    id: 'curve-main',
    name: 'focus-demo.sbgf',
    path: 'examples/gui/focus-demo.sbgf',
    kind: 'sbgf',
    dirty: false,
    content: sbgfContent,
    lines: sbgfContent.split('\n'),
  },
]

export const mockDiagnostics: ValidationDiagnostic[] = [
  {
    id: 'diag-1',
    documentId: 'session-main',
    severity: 'warning',
    line: 2,
    column: 1,
    message: 'Export path handling is not wired yet in the scaffold bridge.',
  },
  {
    id: 'diag-2',
    documentId: 'curve-main',
    severity: 'warning',
    line: 6,
    column: 1,
    message: 'Structured .sbgf validation will be added through sbagenxlib.',
  },
]
