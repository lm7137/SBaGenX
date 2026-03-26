fn main() {
  let runtime_dir = std::path::Path::new("runtime-bundle");
  if !runtime_dir.exists() {
    std::fs::create_dir_all(runtime_dir).expect("failed to create runtime-bundle directory");
  }
  tauri_build::build()
}
