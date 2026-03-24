mod sbagenxlib;

use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;
use tauri::Manager;

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
struct ValidationDiagnostic {
  id: String,
  document_id: String,
  severity: String,
  line: Option<u32>,
  column: Option<u32>,
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

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct BeatPreviewPoint {
  t_sec: f64,
  beat_hz: f64,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct BeatPreviewResult {
  duration_sec: f64,
  sample_count: usize,
  min_hz: f64,
  max_hz: f64,
  limited: bool,
  time_label: String,
  points: Vec<BeatPreviewPoint>,
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
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct RenderDocumentArgs {
  kind: String,
  text: String,
  source_name: Option<String>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct ExportDocumentArgs {
  kind: String,
  text: String,
  source_name: Option<String>,
  output_path: String,
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
fn load_development_examples() -> Result<Vec<FileDocument>, String> {
  let repo_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
    .join("../..")
    .canonicalize()
    .map_err(|err| format!("failed to resolve repo root: {}", err))?;

  let paths = [
    repo_root.join("examples/sbagenxlib/minimal-sbg-wave-custom.sbg"),
    repo_root.join("examples/basics/curve-expfit-solve-demo.sbgf"),
  ];

  let mut docs = Vec::new();
  for path_buf in paths {
    if !path_buf.exists() {
      continue;
    }
    let kind = match kind_from_path(&path_buf) {
      Some(kind) => kind,
      None => continue,
    };
    let content = fs::read_to_string(&path_buf)
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
fn write_text_file(app: tauri::AppHandle, path: String, content: String) -> Result<(), String> {
  fs::write(&path, content).map_err(|err| format!("failed to write {}: {}", path, err))?;
  remember_recent_path(&app, &path)
}

#[tauri::command]
fn validate_document(args: ValidateDocumentArgs) -> Result<ValidationResult, String> {
  let source_name = normalize_source_name(args.source_name, &args.kind);
  let outcome = match args.kind.as_str() {
    "sbg" => sbagenxlib::validate_sbg(&args.text, &source_name)?,
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
  let outcome = sbagenxlib::render_preview(&args.text, &source_name)?;
  Ok(PreviewResult {
    audio_path: outcome.audio_path,
    duration_sec: outcome.duration_sec,
    limited: outcome.limited,
    bridge: "tauri-rust",
    engine_version: outcome.engine_version,
  })
}

#[tauri::command]
fn export_document(args: ExportDocumentArgs) -> Result<ExportResult, String> {
  if args.kind != "sbg" {
    return Err("export is currently available only for .sbg documents".to_string());
  }
  let source_name = normalize_source_name(args.source_name, &args.kind);
  let out_path = PathBuf::from(&args.output_path);
  let outcome = sbagenxlib::export_sbg(&args.text, &source_name, &out_path)?;
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
    min_hz: outcome.min_hz,
    max_hz: outcome.max_hz,
    limited: outcome.limited,
    time_label: outcome.time_label,
    points: outcome
      .points
      .into_iter()
      .map(|point| BeatPreviewPoint {
        t_sec: point.t_sec,
        beat_hz: point.beat_hz,
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
    .plugin(tauri_plugin_dialog::init())
    .setup(|app| {
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
      load_development_examples,
      write_text_file,
      validate_document,
      render_preview,
      export_document,
      sample_beat_preview,
      inspect_curve_info
    ])
    .run(tauri::generate_context!())
    .expect("error while running tauri application");
}
