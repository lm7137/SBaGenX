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
  beatPreview: BeatPreviewResult | null
  beatPreviewError: string | null
  curveInfo: CurveInfoResult | null
  curveInfoError: string | null
}

export interface ValidationDiagnostic {
  id: string
  documentId: string
  severity: 'error' | 'warning'
  line?: number
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

export interface PreviewResult {
  audioPath: string
  durationSec: number
  limited: boolean
  bridge: string
  engineVersion: string
}

export interface ExportResult {
  outputPath: string
  durationSec: number
  format: string
  bridge: string
  engineVersion: string
}

export interface BeatPreviewPoint {
  tSec: number
  beatHz: number
}

export interface BeatPreviewResult {
  durationSec: number
  sampleCount: number
  minHz: number
  maxHz: number
  limited: boolean
  timeLabel: string
  points: BeatPreviewPoint[]
  bridge: string
  engineVersion: string
}

export interface CurveParameter {
  name: string
  value: number
}

export interface CurveInfoResult {
  parameterCount: number
  hasSolve: boolean
  hasCarrierExpr: boolean
  hasAmpExpr: boolean
  hasMixampExpr: boolean
  beatPieceCount: number
  carrierPieceCount: number
  ampPieceCount: number
  mixampPieceCount: number
  parameters: CurveParameter[]
  bridge: string
  engineVersion: string
}
