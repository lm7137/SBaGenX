export type DocumentKind = 'sbg' | 'sbgf'

export interface DocumentRecord {
  id: string
  name: string
  path: string | null
  kind: DocumentKind
  dirty: boolean
  content: string
  lines: string[]
  diagnostics: ValidationDiagnostic[]
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

export interface FileDocument {
  path: string
  name: string
  kind: DocumentKind
  content: string
}

export interface ValidationResult {
  valid: boolean
  diagnostics: ValidationDiagnostic[]
  bridge: string
  engineVersion: string
}
