import { invoke } from '@tauri-apps/api/core'
import type {
  BackendStatus,
  BeatPreviewResult,
  CurveInfoResult,
  ExportResult,
  FileDocument,
  LivePreviewResult,
  ProgramRuntimeRequest,
  PreviewResult,
  PlaybackEvent,
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

export async function inspectMixEmbeddedLooper(path: string): Promise<string | null> {
  return invoke<string | null>('inspect_mix_embedded_looper', { path })
}

export async function validateDocument(
  kind: 'sbg' | 'sbgf',
  text: string,
  sourceName?: string | null,
  mixPathOverride?: string | null,
  mixLooperOverride?: string | null,
): Promise<ValidationResult> {
  return invoke<ValidationResult>('validate_document', {
    args: {
      kind,
      text,
      sourceName,
      mixPathOverride,
      mixLooperOverride,
    },
  })
}

export async function renderPreview(
  text: string,
  sourceName?: string | null,
  mixPathOverride?: string | null,
  mixLooperOverride?: string | null,
): Promise<PreviewResult> {
  return invoke<PreviewResult>('render_preview', {
    args: {
      kind: 'sbg',
      text,
      sourceName,
      mixPathOverride,
      mixLooperOverride,
    },
  })
}

export async function startLivePreview(
  text: string,
  sourceName?: string | null,
  mixPathOverride?: string | null,
  mixLooperOverride?: string | null,
): Promise<LivePreviewResult> {
  return invoke<LivePreviewResult>('start_live_preview', {
    args: {
      kind: 'sbg',
      text,
      sourceName,
      mixPathOverride,
      mixLooperOverride,
    },
  })
}

export async function stopLivePreview(): Promise<void> {
  await invoke('stop_live_preview')
}

export async function exportDocument(
  text: string,
  outputPath: string,
  sourceName?: string | null,
  mixPathOverride?: string | null,
  mixLooperOverride?: string | null,
): Promise<ExportResult> {
  return invoke<ExportResult>('export_document', {
    args: {
      kind: 'sbg',
      text,
      outputPath,
      sourceName,
      mixPathOverride,
      mixLooperOverride,
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

export async function inspectProgramCurveInfo(
  request: ProgramRuntimeRequest,
): Promise<CurveInfoResult> {
  return invoke<CurveInfoResult>('inspect_program_curve_info', {
    args: request,
  })
}

export async function validateProgram(
  request: ProgramRuntimeRequest,
): Promise<ValidationResult> {
  return invoke<ValidationResult>('validate_program', {
    args: request,
  })
}

export async function startProgramLivePreview(
  request: ProgramRuntimeRequest,
): Promise<LivePreviewResult> {
  return invoke<LivePreviewResult>('start_program_live_preview', {
    args: request,
  })
}

export async function exportProgram(
  request: ProgramRuntimeRequest,
  outputPath: string,
): Promise<ExportResult> {
  return invoke<ExportResult>('export_program', {
    args: {
      ...request,
      outputPath,
    },
  })
}

export async function sampleProgramBeatPreview(
  request: ProgramRuntimeRequest,
): Promise<BeatPreviewResult> {
  return invoke<BeatPreviewResult>('sample_program_beat_preview', {
    args: request,
  })
}


export async function exitApplication(): Promise<void> {
  await invoke('exit_application')
}

export type { PlaybackEvent }
