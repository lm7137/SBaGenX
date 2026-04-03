export type DocumentKind = 'sbg' | 'sbgf'
export type RuntimeMode = 'sequence' | 'program'
export type ProgramKind = 'drop' | 'sigmoid' | 'slide' | 'curve'

export interface DocumentRecord {
  id: string
  name: string
  path: string | null
  kind: DocumentKind
  mixPathOverride: string | null
  mixLooperOverride: string
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
  endLine?: number
  endColumn?: number
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

export interface RecentFileEntry {
  path: string
  name: string
  kind: DocumentKind
}

export interface SessionDocumentState {
  documents: FileDocument[]
  activePath: string | null
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

export interface LivePreviewResult {
  durationSec: number
  limited: boolean
  sampleRateHz: number
  channels: number
  bridge: string
  engineVersion: string
}

export interface PlaybackEvent {
  state: 'started' | 'stopped' | 'finished' | 'error'
  message?: string | null
  durationSec?: number | null
  limited?: boolean | null
  sampleRateHz?: number | null
  channels?: number | null
  bridge: string
  engineVersion?: string | null
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
  beatHz: number | null
}

export interface BeatPreviewSeries {
  voiceIndex: number
  label: string
  activeSampleCount: number
  points: BeatPreviewPoint[]
}

export interface BeatPreviewResult {
  durationSec: number
  sampleCount: number
  voiceCount: number
  minHz: number | null
  maxHz: number | null
  limited: boolean
  timeLabel: string
  series: BeatPreviewSeries[]
  bridge: string
  engineVersion: string
}

export interface IsoCyclePreviewPoint {
  tSec: number
  envelope: number
  wave: number
}

export interface IsoCyclePreviewResult {
  durationSec: number
  sampleCount: number
  carrierHz: number
  beatHz: number
  points: IsoCyclePreviewPoint[]
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

export interface ProgramRuntimeRequest {
  programKind: ProgramKind
  mainArg: string
  dropTimeSec: number
  holdTimeSec: number
  wakeTimeSec: number
  isoParamsSpec?: string | null
  curveText?: string | null
  sourceName?: string | null
  mixPath?: string | null
  mixLooperSpec?: string | null
}
