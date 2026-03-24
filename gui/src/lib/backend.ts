import { invoke } from '@tauri-apps/api/core'
import type {
  BackendStatus,
  BeatPreviewResult,
  CurveInfoResult,
  ExportResult,
  FileDocument,
  PreviewResult,
  RecentFileEntry,
  SessionDocumentState,
  ValidationResult,
} from './types'

const fallbackStatus: BackendStatus = {
  bridge: 'tauri-rust',
  engine: 'sbagenxlib',
  mode: 'scaffold',
  target: 'frontend-only preview',
}

export async function backendStatus(): Promise<BackendStatus> {
  try {
    return await invoke<BackendStatus>('backend_status')
  } catch {
    return fallbackStatus
  }
}

export async function readTextFile(path: string): Promise<FileDocument> {
  return invoke<FileDocument>('read_text_file', { path })
}

export async function loadRecentFiles(): Promise<RecentFileEntry[]> {
  return invoke<RecentFileEntry[]>('load_recent_files')
}

export async function loadDevelopmentExamples(): Promise<FileDocument[]> {
  return invoke<FileDocument[]>('load_development_examples')
}

export async function loadSessionDocuments(): Promise<SessionDocumentState> {
  return invoke<SessionDocumentState>('load_session_documents')
}

export async function saveSessionState(
  paths: string[],
  activePath?: string | null,
): Promise<void> {
  await invoke('save_session_state', {
    paths,
    activePath,
  })
}

export async function writeTextFile(path: string, content: string): Promise<void> {
  await invoke('write_text_file', { path, content })
}

export async function validateDocument(
  kind: 'sbg' | 'sbgf',
  text: string,
  sourceName?: string | null,
): Promise<ValidationResult> {
  return invoke<ValidationResult>('validate_document', {
    args: {
      kind,
      text,
      sourceName,
    },
  })
}

export async function renderPreview(
  text: string,
  sourceName?: string | null,
): Promise<PreviewResult> {
  return invoke<PreviewResult>('render_preview', {
    args: {
      kind: 'sbg',
      text,
      sourceName,
    },
  })
}

export async function exportDocument(
  text: string,
  outputPath: string,
  sourceName?: string | null,
): Promise<ExportResult> {
  return invoke<ExportResult>('export_document', {
    args: {
      kind: 'sbg',
      text,
      outputPath,
      sourceName,
    },
  })
}

export async function sampleBeatPreview(
  text: string,
  sourceName?: string | null,
): Promise<BeatPreviewResult> {
  return invoke<BeatPreviewResult>('sample_beat_preview', {
    args: {
      kind: 'sbg',
      text,
      sourceName,
    },
  })
}

export async function inspectCurveInfo(
  text: string,
  sourceName?: string | null,
): Promise<CurveInfoResult> {
  return invoke<CurveInfoResult>('inspect_curve_info', {
    args: {
      kind: 'sbgf',
      text,
      sourceName,
    },
  })
}
