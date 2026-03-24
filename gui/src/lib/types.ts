export type DocumentKind = 'sbg' | 'sbgf'

export interface DocumentRecord {
  id: string
  name: string
  path: string
  kind: DocumentKind
  dirty: boolean
  content: string
  lines: string[]
}

export interface ValidationDiagnostic {
  id: string
  documentId: string
  severity: 'error' | 'warning'
  line: number
  column?: number
  message: string
}

export interface BackendStatus {
  bridge: string
  engine: string
  mode: string
  target: string
}
