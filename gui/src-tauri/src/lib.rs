use serde::Serialize;

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct BackendStatus {
  bridge: &'static str,
  engine: &'static str,
  mode: &'static str,
  target: String,
}

#[tauri::command]
fn backend_status() -> BackendStatus {
  BackendStatus {
    bridge: "tauri-rust",
    engine: "sbagenxlib",
    mode: "scaffold",
    target: format!("{}-{}", std::env::consts::OS, std::env::consts::ARCH),
  }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
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
    .invoke_handler(tauri::generate_handler![backend_status])
    .run(tauri::generate_context!())
    .expect("error while running tauri application");
}
