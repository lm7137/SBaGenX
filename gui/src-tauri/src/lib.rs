mod sbagenxlib;

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;
use std::sync::{
  atomic::{AtomicBool, Ordering},
  Arc, Mutex,
};
use std::thread::{self, JoinHandle};
use std::time::Duration;
use tauri::{Emitter, Manager, State};

use crate::sbagenxlib::{kind_from_path, normalize_source_name};

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct BackendStatus {
  bridge: &'static str,
  engine: String,
  mode: &'static str,
  target: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct FileDocument {
  path: String,
  name: String,
  kind: String,
  content: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct RecentFileEntry {
  path: String,
  name: String,
  kind: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct SessionDocumentState {
  documents: Vec<FileDocument>,
  active_path: Option<String>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct ValidationDiagnostic {
  id: String,
  document_id: String,
  severity: String,
  line: Option<u32>,
  column: Option<u32>,
  end_line: Option<u32>,
  end_column: Option<u32>,
  message: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct ValidationResult {
  valid: bool,
  diagnostics: Vec<ValidationDiagnostic>,
  bridge: &'static str,
  engine_version: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct PreviewResult {
  audio_path: String,
  duration_sec: f64,
  limited: bool,
  bridge: &'static str,
  engine_version: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct ExportResult {
  output_path: String,
  duration_sec: f64,
  format: String,
  bridge: &'static str,
  engine_version: String,
}

#[derive(Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct LivePreviewResult {
  duration_sec: f64,
  limited: bool,
  sample_rate_hz: u32,
  channels: u16,
  bridge: &'static str,
  engine_version: String,
}

#[derive(Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct PlaybackEvent {
  state: &'static str,
  message: Option<String>,
  duration_sec: Option<f64>,
  limited: Option<bool>,
  sample_rate_hz: Option<u32>,
  channels: Option<u16>,
  bridge: &'static str,
  engine_version: Option<String>,
}

struct PlaybackRenderState {
  live: sbagenxlib::LivePlaybackContext,
  output_channels: usize,
  scratch: Vec<f32>,
  finished: bool,
  error: Option<String>,
}

struct LivePlaybackSession {
  stop_flag: Arc<AtomicBool>,
  thread: Option<JoinHandle<()>>,
}

impl LivePlaybackSession {
  fn stop(mut self) {
    self.stop_flag.store(true, Ordering::SeqCst);
    if let Some(handle) = self.thread.take() {
      let _ = handle.join();
    }
  }
}

#[derive(Default)]
struct PlaybackController {
  session: Mutex<Option<LivePlaybackSession>>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct BeatPreviewPoint {
  t_sec: f64,
  beat_hz: Option<f64>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct BeatPreviewSeries {
  voice_index: usize,
  label: String,
  active_sample_count: usize,
  points: Vec<BeatPreviewPoint>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct BeatPreviewResult {
  duration_sec: f64,
  sample_count: usize,
  voice_count: usize,
  min_hz: Option<f64>,
  max_hz: Option<f64>,
  limited: bool,
  time_label: String,
  series: Vec<BeatPreviewSeries>,
  bridge: &'static str,
  engine_version: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct CurveParameter {
  name: String,
  value: f64,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct CurveInfoResult {
  parameter_count: usize,
  has_solve: bool,
  has_carrier_expr: bool,
  has_amp_expr: bool,
  has_mixamp_expr: bool,
  beat_piece_count: usize,
  carrier_piece_count: usize,
  amp_piece_count: usize,
  mixamp_piece_count: usize,
  parameters: Vec<CurveParameter>,
  bridge: &'static str,
  engine_version: String,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct ValidateDocumentArgs {
  kind: String,
  text: String,
  source_name: Option<String>,
  mix_path_override: Option<String>,
  mix_looper_override: Option<String>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct RenderDocumentArgs {
  kind: String,
  text: String,
  source_name: Option<String>,
  mix_path_override: Option<String>,
  mix_looper_override: Option<String>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct ExportDocumentArgs {
  kind: String,
  text: String,
  source_name: Option<String>,
  output_path: String,
  mix_path_override: Option<String>,
  mix_looper_override: Option<String>,
}

#[derive(Deserialize, Clone)]
#[serde(rename_all = "camelCase")]
struct ProgramRequestArgs {
  program_kind: String,
  main_arg: String,
  drop_time_sec: i32,
  hold_time_sec: i32,
  wake_time_sec: i32,
  curve_text: Option<String>,
  source_name: Option<String>,
  mix_path: Option<String>,
  mix_looper_spec: Option<String>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct ProgramExportArgs {
  program_kind: String,
  main_arg: String,
  drop_time_sec: i32,
  hold_time_sec: i32,
  wake_time_sec: i32,
  curve_text: Option<String>,
  source_name: Option<String>,
  mix_path: Option<String>,
  mix_looper_spec: Option<String>,
  output_path: String,
}

#[derive(Deserialize, Serialize, Default)]
#[serde(rename_all = "camelCase")]
struct SessionStore {
  paths: Vec<String>,
  active_path: Option<String>,
}

fn recent_files_store_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
  let dir = app
    .path()
    .app_config_dir()
    .map_err(|err| format!("failed to resolve app config directory: {}", err))?;
  fs::create_dir_all(&dir)
    .map_err(|err| format!("failed to create app config directory {}: {}", dir.display(), err))?;
  Ok(dir.join("recent-files.json"))
}

fn session_store_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
  let dir = app
    .path()
    .app_config_dir()
    .map_err(|err| format!("failed to resolve app config directory: {}", err))?;
  fs::create_dir_all(&dir)
    .map_err(|err| format!("failed to create app config directory {}: {}", dir.display(), err))?;
  Ok(dir.join("session-state.json"))
}

fn load_recent_paths(app: &tauri::AppHandle) -> Result<Vec<String>, String> {
  let path = recent_files_store_path(app)?;
  if !path.exists() {
    return Ok(Vec::new());
  }
  let raw = fs::read_to_string(&path)
    .map_err(|err| format!("failed to read recent file store {}: {}", path.display(), err))?;
  serde_json::from_str::<Vec<String>>(&raw)
    .map_err(|err| format!("failed to parse recent file store {}: {}", path.display(), err))
}

fn save_recent_paths(app: &tauri::AppHandle, paths: &[String]) -> Result<(), String> {
  let path = recent_files_store_path(app)?;
  let raw = serde_json::to_string_pretty(paths)
    .map_err(|err| format!("failed to encode recent file store: {}", err))?;
  fs::write(&path, raw)
    .map_err(|err| format!("failed to write recent file store {}: {}", path.display(), err))
}

fn load_session_store(app: &tauri::AppHandle) -> Result<SessionStore, String> {
  let path = session_store_path(app)?;
  if !path.exists() {
    return Ok(SessionStore::default());
  }
  let raw = fs::read_to_string(&path)
    .map_err(|err| format!("failed to read session store {}: {}", path.display(), err))?;
  serde_json::from_str::<SessionStore>(&raw)
    .map_err(|err| format!("failed to parse session store {}: {}", path.display(), err))
}

fn save_session_store(app: &tauri::AppHandle, store: &SessionStore) -> Result<(), String> {
  let path = session_store_path(app)?;
  let raw = serde_json::to_string_pretty(store)
    .map_err(|err| format!("failed to encode session store: {}", err))?;
  fs::write(&path, raw)
    .map_err(|err| format!("failed to write session store {}: {}", path.display(), err))
}

fn remember_recent_path(app: &tauri::AppHandle, path: &str) -> Result<(), String> {
  let path_buf = PathBuf::from(path);
  if kind_from_path(&path_buf).is_none() {
    return Ok(());
  }
  let canonical = path_buf.to_string_lossy().into_owned();
  let mut recent = load_recent_paths(app).unwrap_or_default();
  recent.retain(|item| item != &canonical);
  recent.insert(0, canonical);
  if recent.len() > 12 {
    recent.truncate(12);
  }
  save_recent_paths(app, &recent)
}

fn candidate_runtime_library_names() -> &'static [&'static str] {
  if cfg!(target_os = "linux") {
    &["libsbagenx.so", "libsbagenx.so.3"]
  } else if cfg!(target_os = "windows") {
    if cfg!(target_pointer_width = "64") {
      &["sbagenxlib-win64.dll"]
    } else {
      &["sbagenxlib-win32.dll"]
    }
  } else if cfg!(target_os = "macos") {
    &["libsbagenx.dylib"]
  } else {
    &[]
  }
}

#[cfg(target_os = "linux")]
fn installed_linux_runtime_candidates() -> Vec<PathBuf> {
  let mut candidates = Vec::new();

  if cfg!(target_arch = "x86_64") {
    candidates.push(PathBuf::from("/usr/lib/x86_64-linux-gnu/libsbagenx.so"));
    candidates.push(PathBuf::from("/usr/lib/x86_64-linux-gnu/libsbagenx.so.3"));
  } else if cfg!(target_arch = "aarch64") {
    candidates.push(PathBuf::from("/usr/lib/aarch64-linux-gnu/libsbagenx.so"));
    candidates.push(PathBuf::from("/usr/lib/aarch64-linux-gnu/libsbagenx.so.3"));
  } else if cfg!(target_arch = "x86") {
    candidates.push(PathBuf::from("/usr/lib/i386-linux-gnu/libsbagenx.so"));
    candidates.push(PathBuf::from("/usr/lib/i386-linux-gnu/libsbagenx.so.3"));
  }

  candidates.extend([
    PathBuf::from("/usr/lib/libsbagenx.so"),
    PathBuf::from("/usr/lib/libsbagenx.so.3"),
    PathBuf::from("/usr/lib/sbagenx/libsbagenx.so"),
    PathBuf::from("/usr/lib/sbagenx/libsbagenx.so.3"),
    PathBuf::from("/usr/local/lib/libsbagenx.so"),
    PathBuf::from("/usr/local/lib/libsbagenx.so.3"),
  ]);

  candidates
}

fn prime_sbagenxlib_runtime_path(app: &tauri::AppHandle) {
  if std::env::var("SBAGENX_GUI_SBAGENXLIB")
    .ok()
    .map(|path| PathBuf::from(path).exists())
    .unwrap_or(false)
  {
    return;
  }

  let mut candidates = Vec::<PathBuf>::new();

  if let Ok(resource_dir) = app.path().resource_dir() {
    for name in candidate_runtime_library_names() {
      candidates.push(resource_dir.join(name));
    }
  }

  if let Ok(exe_path) = std::env::current_exe() {
    if let Some(exe_dir) = exe_path.parent() {
      for name in candidate_runtime_library_names() {
        candidates.push(exe_dir.join(name));
        candidates.push(exe_dir.join("resources").join(name));
        candidates.push(exe_dir.join("../lib").join(name));
      }
    }
  }

  #[cfg(target_os = "linux")]
  {
    candidates.extend(installed_linux_runtime_candidates());
  }

  for candidate in candidates {
    if candidate.exists() {
      let _ = std::env::set_var("SBAGENX_GUI_SBAGENXLIB", candidate);
      break;
    }
  }
}

fn emit_playback_event(app: &tauri::AppHandle, event: PlaybackEvent) {
  let _ = app.emit("playback-state", event);
}

fn stop_active_session(controller: &PlaybackController) {
  if let Ok(mut guard) = controller.session.lock() {
    if let Some(session) = guard.take() {
      drop(guard);
      session.stop();
    }
  }
}

fn render_into_output<T>(data: &mut [T], shared: &Arc<Mutex<PlaybackRenderState>>, stop: &Arc<AtomicBool>)
where
  T: cpal::Sample + cpal::FromSample<f32>,
{
  if data.is_empty() {
    return;
  }

  let output_channels = {
    let guard = match shared.lock() {
      Ok(guard) => guard,
      Err(_) => {
        for sample in data.iter_mut() {
          *sample = T::from_sample(0.0);
        }
        return;
      }
    };
    guard.output_channels.max(1)
  };

  if stop.load(Ordering::SeqCst) {
    for sample in data.iter_mut() {
      *sample = T::from_sample(0.0);
    }
    return;
  }

  let frames = data.len() / output_channels;
  if frames == 0 {
    return;
  }

  let mut guard = match shared.lock() {
    Ok(guard) => guard,
    Err(_) => {
      for sample in data.iter_mut() {
        *sample = T::from_sample(0.0);
      }
      return;
    }
  };

  if guard.finished {
    for sample in data.iter_mut() {
      *sample = T::from_sample(0.0);
    }
    return;
  }

  let mut scratch = std::mem::take(&mut guard.scratch);
  if scratch.len() < frames * 2 {
    scratch.resize(frames * 2, 0.0);
  }

  let render_result = guard.live.render_interleaved_f32(&mut scratch[..frames * 2]);
  let mut restore_now = false;
  let rendered_frames = match render_result {
    Ok(rendered_frames) => rendered_frames,
    Err(err) => {
      guard.error = Some(err);
      guard.finished = true;
      restore_now = true;
      0
    }
  };

  if restore_now {
    guard.scratch = scratch;
    for sample in data.iter_mut() {
      *sample = T::from_sample(0.0);
    }
    return;
  }

  for frame_idx in 0..frames {
    let (left, right) = if frame_idx < rendered_frames {
      let base = frame_idx * 2;
      (scratch[base], scratch[base + 1])
    } else {
      (0.0, 0.0)
    };

    let out_base = frame_idx * output_channels;
    if output_channels == 1 {
      data[out_base] = T::from_sample((left + right) * 0.5);
      continue;
    }

    data[out_base] = T::from_sample(left);
    data[out_base + 1] = T::from_sample(right);
    for ch in 2..output_channels {
      data[out_base + ch] = T::from_sample(0.0);
    }
  }

  if rendered_frames < frames || guard.live.is_finished() {
    guard.finished = true;
  }
  guard.scratch = scratch;
}

fn program_request_from_args(args: ProgramRequestArgs) -> Result<sbagenxlib::ProgramRuntimeRequest, String> {
  Ok(sbagenxlib::ProgramRuntimeRequest {
    kind: sbagenxlib::ProgramKind::from_str(&args.program_kind)?,
    main_arg: args.main_arg,
    drop_time_sec: args.drop_time_sec,
    hold_time_sec: args.hold_time_sec,
    wake_time_sec: args.wake_time_sec,
    curve_text: args.curve_text,
    source_name: args.source_name.unwrap_or_default(),
    mix_path: args.mix_path,
    mix_looper_spec: args.mix_looper_spec,
  })
}

fn spawn_playback_thread_with_factory<F>(
  app: tauri::AppHandle,
  stop_flag: Arc<AtomicBool>,
  startup_tx: std::sync::mpsc::Sender<Result<LivePreviewResult, String>>,
  build_live: F,
) -> JoinHandle<()>
where
  F: FnOnce(u32) -> Result<sbagenxlib::LivePlaybackContext, String> + Send + 'static,
{
  thread::spawn(move || {
    let host = cpal::default_host();
    let device = match host.default_output_device() {
      Some(device) => device,
      None => {
        let _ = startup_tx.send(Err("no default audio output device is available".to_string()));
        return;
      }
    };
    let default_config = match device.default_output_config() {
      Ok(config) => config,
      Err(err) => {
        let _ = startup_tx.send(Err(format!(
          "failed to query default audio output device: {}",
          err
        )));
        return;
      }
    };
    let sample_rate_hz = default_config.sample_rate().0;
    let output_channels = default_config.channels() as usize;

    let live = match build_live(sample_rate_hz) {
      Ok(live) => live,
      Err(err) => {
        let _ = startup_tx.send(Err(err));
        return;
      }
    };

    let shared = Arc::new(Mutex::new(PlaybackRenderState {
      live,
      output_channels,
      scratch: Vec::new(),
      finished: false,
      error: None,
    }));

    let stream_config: cpal::StreamConfig = default_config.config();
    let stream = match default_config.sample_format() {
      cpal::SampleFormat::F32 => {
        let shared = shared.clone();
        let stop = stop_flag.clone();
        let app_for_errors = app.clone();
        let stop_for_errors = stop_flag.clone();
        device.build_output_stream(
          &stream_config,
          move |data: &mut [f32], _| render_into_output(data, &shared, &stop),
          move |err| {
            emit_playback_event(
              &app_for_errors,
              PlaybackEvent {
                state: "error",
                message: Some(format!("audio output stream error: {}", err)),
                duration_sec: None,
                limited: None,
                sample_rate_hz: Some(sample_rate_hz),
                channels: Some(output_channels as u16),
                bridge: "tauri-rust",
                engine_version: None,
              },
            );
            stop_for_errors.store(true, Ordering::SeqCst);
          },
          None,
        )
      }
      cpal::SampleFormat::I16 => {
        let shared = shared.clone();
        let stop = stop_flag.clone();
        let app_for_errors = app.clone();
        let stop_for_errors = stop_flag.clone();
        device.build_output_stream(
          &stream_config,
          move |data: &mut [i16], _| render_into_output(data, &shared, &stop),
          move |err| {
            emit_playback_event(
              &app_for_errors,
              PlaybackEvent {
                state: "error",
                message: Some(format!("audio output stream error: {}", err)),
                duration_sec: None,
                limited: None,
                sample_rate_hz: Some(sample_rate_hz),
                channels: Some(output_channels as u16),
                bridge: "tauri-rust",
                engine_version: None,
              },
            );
            stop_for_errors.store(true, Ordering::SeqCst);
          },
          None,
        )
      }
      cpal::SampleFormat::U16 => {
        let shared = shared.clone();
        let stop = stop_flag.clone();
        let app_for_errors = app.clone();
        let stop_for_errors = stop_flag.clone();
        device.build_output_stream(
          &stream_config,
          move |data: &mut [u16], _| render_into_output(data, &shared, &stop),
          move |err| {
            emit_playback_event(
              &app_for_errors,
              PlaybackEvent {
                state: "error",
                message: Some(format!("audio output stream error: {}", err)),
                duration_sec: None,
                limited: None,
                sample_rate_hz: Some(sample_rate_hz),
                channels: Some(output_channels as u16),
                bridge: "tauri-rust",
                engine_version: None,
              },
            );
            stop_for_errors.store(true, Ordering::SeqCst);
          },
          None,
        )
      }
      other => {
        let _ = startup_tx.send(Err(format!(
          "unsupported audio output sample format for live preview: {:?}",
          other
        )));
        return;
      }
    };

    let stream = match stream {
      Ok(stream) => stream,
      Err(err) => {
        let _ = startup_tx.send(Err(format!(
          "failed to create audio output stream: {}",
          err
        )));
        return;
      }
    };

    if let Err(err) = stream.play() {
      let _ = startup_tx.send(Err(format!(
        "failed to start audio output stream: {}",
        err
      )));
      return;
    }

    let (duration_sec, limited, engine_version) = {
      let guard = match shared.lock() {
        Ok(guard) => guard,
        Err(_) => {
          let _ = startup_tx.send(Err("failed to read live playback state".to_string()));
          return;
        }
      };
      (
        guard.live.duration_sec(),
        guard.live.limited(),
        guard.live.engine_version(),
      )
    };

    let start_result = LivePreviewResult {
      duration_sec,
      limited,
      sample_rate_hz,
      channels: output_channels as u16,
      bridge: "tauri-rust",
      engine_version: engine_version.clone(),
    };
    let _ = startup_tx.send(Ok(start_result.clone()));
    emit_playback_event(
      &app,
      PlaybackEvent {
        state: "started",
        message: None,
        duration_sec: Some(duration_sec),
        limited: Some(limited),
        sample_rate_hz: Some(sample_rate_hz),
        channels: Some(output_channels as u16),
        bridge: "tauri-rust",
        engine_version: Some(engine_version),
      },
    );

    loop {
      if stop_flag.load(Ordering::SeqCst) {
        break;
      }

      let (finished, error, engine_version, duration_sec, limited) = {
        let mut guard = match shared.lock() {
          Ok(guard) => guard,
          Err(_) => break,
        };
        let error = guard.error.take();
        (
          guard.finished,
          error,
          guard.live.engine_version(),
          guard.live.duration_sec(),
          guard.live.limited(),
        )
      };

      if let Some(message) = error {
        emit_playback_event(
          &app,
          PlaybackEvent {
            state: "error",
            message: Some(message),
            duration_sec: Some(duration_sec),
            limited: Some(limited),
            sample_rate_hz: Some(sample_rate_hz),
            channels: Some(output_channels as u16),
            bridge: "tauri-rust",
            engine_version: Some(engine_version),
          },
        );
        stop_flag.store(true, Ordering::SeqCst);
        break;
      }

      if finished {
        emit_playback_event(
          &app,
          PlaybackEvent {
            state: "finished",
            message: None,
            duration_sec: Some(duration_sec),
            limited: Some(limited),
            sample_rate_hz: Some(sample_rate_hz),
            channels: Some(output_channels as u16),
            bridge: "tauri-rust",
            engine_version: Some(engine_version),
          },
        );
        stop_flag.store(true, Ordering::SeqCst);
        break;
      }

      thread::sleep(Duration::from_millis(60));
    }

    drop(stream);
  })
}

fn spawn_sequence_playback_thread(
  app: tauri::AppHandle,
  text: String,
  source_name: String,
  mix_path_override: Option<String>,
  mix_looper_override: Option<String>,
  stop_flag: Arc<AtomicBool>,
  startup_tx: std::sync::mpsc::Sender<Result<LivePreviewResult, String>>,
) -> JoinHandle<()> {
  spawn_playback_thread_with_factory(app, stop_flag, startup_tx, move |sample_rate_hz| {
    sbagenxlib::create_live_preview(
      &text,
      &source_name,
      mix_path_override.as_deref(),
      mix_looper_override.as_deref(),
      sample_rate_hz,
    )
  })
}

fn spawn_program_playback_thread(
  app: tauri::AppHandle,
  request: sbagenxlib::ProgramRuntimeRequest,
  stop_flag: Arc<AtomicBool>,
  startup_tx: std::sync::mpsc::Sender<Result<LivePreviewResult, String>>,
) -> JoinHandle<()> {
  spawn_playback_thread_with_factory(app, stop_flag, startup_tx, move |sample_rate_hz| {
    sbagenxlib::create_live_program_preview(&request, sample_rate_hz)
  })
}

#[tauri::command]
fn backend_status() -> BackendStatus {
  let engine = sbagenxlib::backend_status()
    .map(|(version, api)| format!("sbagenxlib {} (api {})", version, api))
    .unwrap_or_else(|err| format!("sbagenxlib unavailable: {}", err));
  BackendStatus {
    bridge: "tauri-rust",
    engine,
    mode: "ffi",
    target: format!("{}-{}", std::env::consts::OS, std::env::consts::ARCH),
  }
}

#[tauri::command]
fn read_text_file(app: tauri::AppHandle, path: String) -> Result<FileDocument, String> {
  let path_buf = PathBuf::from(&path);
  let kind = kind_from_path(&path_buf)
    .ok_or_else(|| "only .sbg and .sbgf files are supported in the current GUI scaffold".to_string())?;
  let content = fs::read_to_string(&path_buf)
    .map_err(|err| format!("failed to read {}: {}", path_buf.display(), err))?;
  let name = path_buf
    .file_name()
    .and_then(|name| name.to_str())
    .ok_or_else(|| "failed to determine document name".to_string())?
    .to_string();
  remember_recent_path(&app, &path)?;
  Ok(FileDocument {
    path,
    name,
    kind: kind.to_string(),
    content,
  })
}

#[tauri::command]
fn load_recent_files(app: tauri::AppHandle) -> Result<Vec<RecentFileEntry>, String> {
  let recent = load_recent_paths(&app)?;
  let original_len = recent.len();
  let mut entries = Vec::new();
  let mut normalized = Vec::new();

  for path in recent {
    let path_buf = PathBuf::from(&path);
    if !path_buf.exists() {
      continue;
    }
    let kind = match kind_from_path(&path_buf) {
      Some(kind) => kind,
      None => continue,
    };
    let name = match path_buf.file_name().and_then(|name| name.to_str()) {
      Some(name) => name.to_string(),
      None => continue,
    };
    normalized.push(path.clone());
    entries.push(RecentFileEntry {
      path,
      name,
      kind: kind.to_string(),
    });
  }

  if entries.len() != original_len {
    save_recent_paths(&app, &normalized)?;
  }

  Ok(entries)
}

#[tauri::command]
fn load_session_documents(app: tauri::AppHandle) -> Result<SessionDocumentState, String> {
  let SessionStore {
    paths,
    active_path: stored_active_path,
  } = load_session_store(&app)?;
  let original_len = paths.len();
  let mut documents = Vec::new();
  let mut normalized_paths = Vec::new();

  for path in paths {
    let path_buf = PathBuf::from(&path);
    if !path_buf.exists() {
      continue;
    }
    let kind = match kind_from_path(&path_buf) {
      Some(kind) => kind,
      None => continue,
    };
    let content = match fs::read_to_string(&path_buf) {
      Ok(content) => content,
      Err(_) => continue,
    };
    let name = match path_buf.file_name().and_then(|name| name.to_str()) {
      Some(name) => name.to_string(),
      None => continue,
    };
    normalized_paths.push(path.clone());
    documents.push(FileDocument {
      path,
      name,
      kind: kind.to_string(),
      content,
    });
  }

  let active_path = stored_active_path
    .clone()
    .filter(|path| normalized_paths.iter().any(|item| item == path));

  if documents.len() != original_len {
    save_session_store(
      &app,
      &SessionStore {
        paths: normalized_paths.clone(),
        active_path: stored_active_path
          .filter(|path| normalized_paths.iter().any(|item| item == path)),
      },
    )?;
  }

  Ok(SessionDocumentState {
    documents,
    active_path,
  })
}

#[tauri::command]
fn save_session_state(
  app: tauri::AppHandle,
  paths: Vec<String>,
  active_path: Option<String>,
) -> Result<(), String> {
  let store = SessionStore { paths, active_path };
  save_session_store(&app, &store)
}

fn collect_example_documents(paths: &[PathBuf]) -> Result<Vec<FileDocument>, String> {
  let mut docs = Vec::new();
  for path_buf in paths {
    if !path_buf.exists() {
      continue;
    }
    let kind = match kind_from_path(path_buf) {
      Some(kind) => kind,
      None => continue,
    };
    let content = fs::read_to_string(path_buf)
      .map_err(|err| format!("failed to read {}: {}", path_buf.display(), err))?;
    let name = path_buf
      .file_name()
      .and_then(|name| name.to_str())
      .ok_or_else(|| format!("failed to determine document name for {}", path_buf.display()))?
      .to_string();
    docs.push(FileDocument {
      path: path_buf.to_string_lossy().into_owned(),
      name,
      kind: kind.to_string(),
      content,
    });
  }
  Ok(docs)
}

#[tauri::command]
fn load_development_examples(app: tauri::AppHandle) -> Result<Vec<FileDocument>, String> {
  if cfg!(debug_assertions) {
    if let Ok(repo_root) = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..").canonicalize() {
      let dev_paths = [
        repo_root.join("examples/plus/deep-sleep-aid.sbg"),
        repo_root.join("examples/basics/curve-expfit-solve-demo.sbgf"),
      ];
      let docs = collect_example_documents(&dev_paths)?;
      if !docs.is_empty() {
        return Ok(docs);
      }
    }
  }

  if let Ok(document_dir) = app.path().document_dir() {
    let installed_examples = document_dir.join("SBaGenX").join("Examples");
    let installed_paths = [
      installed_examples.join("plus").join("deep-sleep-aid.sbg"),
      installed_examples.join("basics").join("curve-expfit-solve-demo.sbgf"),
    ];
    let docs = collect_example_documents(&installed_paths)?;
    if !docs.is_empty() {
      return Ok(docs);
    }
  }

  let repo_root = match PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..").canonicalize() {
    Ok(path) => path,
    Err(_) => return Ok(Vec::new()),
  };
  let fallback_paths = [
    repo_root.join("examples/plus").join("deep-sleep-aid.sbg"),
    repo_root.join("examples/basics/curve-expfit-solve-demo.sbgf"),
  ];
  collect_example_documents(&fallback_paths)
}

#[tauri::command]
fn write_text_file(app: tauri::AppHandle, path: String, content: String) -> Result<(), String> {
  fs::write(&path, content).map_err(|err| format!("failed to write {}: {}", path, err))?;
  remember_recent_path(&app, &path)
}

#[tauri::command]
fn validate_document(args: ValidateDocumentArgs) -> Result<ValidationResult, String> {
  let source_name = normalize_source_name(args.source_name, &args.kind);
  let outcome = match args.kind.as_str() {
    "sbg" => {
      sbagenxlib::validate_sbg(
        &args.text,
        &source_name,
        args.mix_path_override.as_deref(),
        args.mix_looper_override.as_deref(),
      )?
    }
    "sbgf" => sbagenxlib::validate_sbgf(&args.text, &source_name)?,
    _ => return Err(format!("unsupported document kind: {}", args.kind)),
  };
  let diagnostics = outcome
    .diagnostics
    .into_iter()
    .enumerate()
    .map(|(index, diag)| ValidationDiagnostic {
      id: format!("diag-{}-{}", args.kind, index),
      document_id: source_name.clone(),
      severity: diag.severity.to_string(),
      line: diag.line,
      column: diag.column,
      end_line: diag.end_line,
      end_column: diag.end_column,
      message: diag.message,
    })
    .collect();
  Ok(ValidationResult {
    valid: outcome.valid,
    diagnostics,
    bridge: "tauri-rust",
    engine_version: outcome.engine_version,
  })
}

#[tauri::command]
fn validate_program(args: ProgramRequestArgs) -> Result<ValidationResult, String> {
  let request = program_request_from_args(args)?;
  let outcome = sbagenxlib::validate_program(&request)?;
  let document_id = if request.kind == sbagenxlib::ProgramKind::Curve {
    normalize_source_name(Some(request.source_name.clone()), "sbgf")
  } else {
    format!("program:{}", request.kind.as_str())
  };
  let diagnostics = outcome
    .diagnostics
    .into_iter()
    .enumerate()
    .map(|(index, diag)| ValidationDiagnostic {
      id: format!("diag-program-{}", index),
      document_id: document_id.clone(),
      severity: diag.severity.to_string(),
      line: diag.line,
      column: diag.column,
      end_line: diag.end_line,
      end_column: diag.end_column,
      message: diag.message,
    })
    .collect();
  Ok(ValidationResult {
    valid: outcome.valid,
    diagnostics,
    bridge: "tauri-rust",
    engine_version: outcome.engine_version,
  })
}

#[tauri::command]
fn render_preview(args: RenderDocumentArgs) -> Result<PreviewResult, String> {
  if args.kind != "sbg" {
    return Err("preview is currently available only for .sbg documents".to_string());
  }
  let source_name = normalize_source_name(args.source_name, &args.kind);
  let outcome =
    sbagenxlib::render_preview(
      &args.text,
      &source_name,
      args.mix_path_override.as_deref(),
      args.mix_looper_override.as_deref(),
    )?;
  Ok(PreviewResult {
    audio_path: outcome.audio_path,
    duration_sec: outcome.duration_sec,
    limited: outcome.limited,
    bridge: "tauri-rust",
    engine_version: outcome.engine_version,
  })
}

#[tauri::command]
fn start_live_preview(
  app: tauri::AppHandle,
  controller: State<'_, PlaybackController>,
  args: RenderDocumentArgs,
) -> Result<LivePreviewResult, String> {
  if args.kind != "sbg" {
    return Err("live preview is currently available only for .sbg documents".to_string());
  }

  stop_active_session(controller.inner());

  let source_name = normalize_source_name(args.source_name, &args.kind);
  let stop_flag = Arc::new(AtomicBool::new(false));
  let (startup_tx, startup_rx) = std::sync::mpsc::channel();
  let playback_thread = spawn_sequence_playback_thread(
    app.clone(),
    args.text,
    source_name,
    args.mix_path_override,
    args.mix_looper_override,
    stop_flag.clone(),
    startup_tx,
  );

  match startup_rx.recv_timeout(Duration::from_secs(5)) {
    Ok(Ok(result)) => {
      let session = LivePlaybackSession {
        stop_flag,
        thread: Some(playback_thread),
      };
      let mut guard = controller
        .inner()
        .session
        .lock()
        .map_err(|_| "failed to store live playback session".to_string())?;
      *guard = Some(session);
      Ok(result)
    }
    Ok(Err(err)) => {
      let _ = playback_thread.join();
      Err(err)
    }
    Err(_) => {
      stop_flag.store(true, Ordering::SeqCst);
      let _ = playback_thread.join();
      Err("timed out while starting live preview".to_string())
    }
  }
}

#[tauri::command]
fn start_program_live_preview(
  app: tauri::AppHandle,
  controller: State<'_, PlaybackController>,
  args: ProgramRequestArgs,
) -> Result<LivePreviewResult, String> {
  stop_active_session(controller.inner());

  let request = program_request_from_args(args)?;
  let stop_flag = Arc::new(AtomicBool::new(false));
  let (startup_tx, startup_rx) = std::sync::mpsc::channel();
  let playback_thread =
    spawn_program_playback_thread(app.clone(), request, stop_flag.clone(), startup_tx);

  match startup_rx.recv_timeout(Duration::from_secs(5)) {
    Ok(Ok(result)) => {
      let session = LivePlaybackSession {
        stop_flag,
        thread: Some(playback_thread),
      };
      let mut guard = controller
        .inner()
        .session
        .lock()
        .map_err(|_| "failed to store live playback session".to_string())?;
      *guard = Some(session);
      Ok(result)
    }
    Ok(Err(err)) => {
      let _ = playback_thread.join();
      Err(err)
    }
    Err(_) => {
      stop_flag.store(true, Ordering::SeqCst);
      let _ = playback_thread.join();
      Err("timed out while starting live preview".to_string())
    }
  }
}

#[tauri::command]
fn stop_live_preview(
  app: tauri::AppHandle,
  controller: State<'_, PlaybackController>,
) -> Result<(), String> {
  stop_active_session(controller.inner());
  emit_playback_event(
    &app,
    PlaybackEvent {
      state: "stopped",
      message: None,
      duration_sec: None,
      limited: None,
      sample_rate_hz: None,
      channels: None,
      bridge: "tauri-rust",
      engine_version: None,
    },
  );
  Ok(())
}

#[tauri::command]
fn exit_application(app: tauri::AppHandle, controller: State<'_, PlaybackController>) -> Result<(), String> {
  stop_active_session(controller.inner());
  app.exit(0);
  Ok(())
}

#[tauri::command]
fn export_document(args: ExportDocumentArgs) -> Result<ExportResult, String> {
  if args.kind != "sbg" {
    return Err("export is currently available only for .sbg documents".to_string());
  }
  let source_name = normalize_source_name(args.source_name, &args.kind);
  let out_path = PathBuf::from(&args.output_path);
  let outcome = sbagenxlib::export_sbg(
    &args.text,
    &source_name,
    &out_path,
    args.mix_path_override.as_deref(),
    args.mix_looper_override.as_deref(),
  )?;
  Ok(ExportResult {
    output_path: outcome.output_path,
    duration_sec: outcome.duration_sec,
    format: outcome.format,
    bridge: "tauri-rust",
    engine_version: outcome.engine_version,
  })
}

#[tauri::command]
fn export_program(args: ProgramExportArgs) -> Result<ExportResult, String> {
  let request = program_request_from_args(ProgramRequestArgs {
    program_kind: args.program_kind,
    main_arg: args.main_arg,
    drop_time_sec: args.drop_time_sec,
    hold_time_sec: args.hold_time_sec,
    wake_time_sec: args.wake_time_sec,
    curve_text: args.curve_text,
    source_name: args.source_name,
    mix_path: args.mix_path,
    mix_looper_spec: args.mix_looper_spec,
  })?;
  let out_path = PathBuf::from(&args.output_path);
  let outcome = sbagenxlib::export_program(&request, &out_path)?;
  Ok(ExportResult {
    output_path: outcome.output_path,
    duration_sec: outcome.duration_sec,
    format: outcome.format,
    bridge: "tauri-rust",
    engine_version: outcome.engine_version,
  })
}

#[tauri::command]
fn sample_beat_preview(args: RenderDocumentArgs) -> Result<BeatPreviewResult, String> {
  if args.kind != "sbg" {
    return Err("beat preview is currently available only for .sbg documents".to_string());
  }
  let source_name = normalize_source_name(args.source_name, &args.kind);
  let outcome = sbagenxlib::sample_beat_preview(&args.text, &source_name)?;
  Ok(BeatPreviewResult {
    duration_sec: outcome.duration_sec,
    sample_count: outcome.sample_count,
    voice_count: outcome.voice_count,
    min_hz: outcome.min_hz,
    max_hz: outcome.max_hz,
    limited: outcome.limited,
    time_label: outcome.time_label,
    series: outcome
      .series
      .into_iter()
      .map(|series| BeatPreviewSeries {
        voice_index: series.voice_index,
        label: series.label,
        active_sample_count: series.active_sample_count,
        points: series
          .points
          .into_iter()
          .map(|point| BeatPreviewPoint {
            t_sec: point.t_sec,
            beat_hz: point.beat_hz,
          })
          .collect(),
      })
      .collect(),
    bridge: "tauri-rust",
    engine_version: outcome.engine_version,
  })
}

#[tauri::command]
fn sample_program_beat_preview(args: ProgramRequestArgs) -> Result<BeatPreviewResult, String> {
  let request = program_request_from_args(args)?;
  let outcome = sbagenxlib::sample_program_beat_preview(&request)?;
  Ok(BeatPreviewResult {
    duration_sec: outcome.duration_sec,
    sample_count: outcome.sample_count,
    voice_count: outcome.voice_count,
    min_hz: outcome.min_hz,
    max_hz: outcome.max_hz,
    limited: outcome.limited,
    time_label: outcome.time_label,
    series: outcome
      .series
      .into_iter()
      .map(|series| BeatPreviewSeries {
        voice_index: series.voice_index,
        label: series.label,
        active_sample_count: series.active_sample_count,
        points: series
          .points
          .into_iter()
          .map(|point| BeatPreviewPoint {
            t_sec: point.t_sec,
            beat_hz: point.beat_hz,
          })
          .collect(),
      })
      .collect(),
    bridge: "tauri-rust",
    engine_version: outcome.engine_version,
  })
}

#[tauri::command]
fn inspect_curve_info(args: RenderDocumentArgs) -> Result<CurveInfoResult, String> {
  if args.kind != "sbgf" {
    return Err("curve inspection is currently available only for .sbgf documents".to_string());
  }
  let source_name = normalize_source_name(args.source_name, &args.kind);
  let outcome = sbagenxlib::inspect_curve_info(&args.text, &source_name)?;
  Ok(CurveInfoResult {
    parameter_count: outcome.parameter_count,
    has_solve: outcome.has_solve,
    has_carrier_expr: outcome.has_carrier_expr,
    has_amp_expr: outcome.has_amp_expr,
    has_mixamp_expr: outcome.has_mixamp_expr,
    beat_piece_count: outcome.beat_piece_count,
    carrier_piece_count: outcome.carrier_piece_count,
    amp_piece_count: outcome.amp_piece_count,
    mixamp_piece_count: outcome.mixamp_piece_count,
    parameters: outcome
      .parameters
      .into_iter()
      .map(|param| CurveParameter {
        name: param.name,
        value: param.value,
      })
      .collect(),
    bridge: "tauri-rust",
    engine_version: outcome.engine_version,
  })
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
    .manage(PlaybackController::default())
    .plugin(tauri_plugin_dialog::init())
    .setup(|app| {
      prime_sbagenxlib_runtime_path(app.handle());
      if let (Some(window), Some(icon)) = (
        app.get_webview_window("main"),
        app.default_window_icon().cloned(),
      ) {
        let _ = window.set_icon(icon);
      }
      if cfg!(debug_assertions) {
        app.handle().plugin(
          tauri_plugin_log::Builder::default()
            .level(log::LevelFilter::Info)
            .build(),
        )?;
      }
      Ok(())
    })
    .invoke_handler(tauri::generate_handler![
      backend_status,
      read_text_file,
      load_recent_files,
      load_session_documents,
      load_development_examples,
      save_session_state,
      write_text_file,
      validate_document,
      validate_program,
      render_preview,
      start_live_preview,
      start_program_live_preview,
      stop_live_preview,
      exit_application,
      export_document,
      export_program,
      sample_beat_preview,
      sample_program_beat_preview,
      inspect_curve_info
    ])
    .run(tauri::generate_context!())
    .expect("error while running tauri application");
}
