import { invoke } from '@tauri-apps/api/core'
import type { BackendStatus } from './types'

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
