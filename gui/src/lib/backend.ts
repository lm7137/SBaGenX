import { invoke } from '@tauri-apps/api/core'
import type { BackendStatus, FileDocument, ValidationResult } from './types'

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

export async function writeTextFile(path: string, content: string): Promise<void> {
  await invoke('write_text_file', { path, content })
}

export async function validateDocument(
  kind: 'sbg' | 'sbgf',
  text: string,
  sourceName?: string | null,
): Promise<ValidationResult> {
  return invoke<ValidationResult>('validate_document', {
    kind,
    text,
    sourceName,
  })
}
