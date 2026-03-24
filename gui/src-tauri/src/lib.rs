mod sbagenxlib;

use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;

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
fn read_text_file(path: String) -> Result<FileDocument, String> {
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
  Ok(FileDocument {
    path,
    name,
    kind: kind.to_string(),
    content,
  })
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
fn write_text_file(path: String, content: String) -> Result<(), String> {
  fs::write(&path, content).map_err(|err| format!("failed to write {}: {}", path, err))
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
      load_development_examples,
      write_text_file,
      validate_document,
      render_preview,
      export_document
    ])
    .run(tauri::generate_context!())
    .expect("error while running tauri application");
}
