use std::fs;
use std::path::Path;

#[cfg(windows)]
use std::env;
#[cfg(windows)]
use std::path::PathBuf;

fn ensure_runtime_dir() {
  let runtime_dir = Path::new("runtime-bundle");
  if !runtime_dir.exists() {
    fs::create_dir_all(runtime_dir).expect("failed to create runtime-bundle directory");
  }
}

#[cfg(windows)]
fn maybe_copy_webview2_loader() {
  let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
  let target_env = env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();
  if target_os != "windows" || target_env != "gnu" {
    return;
  }

  let target_arch = match env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default().as_str() {
    "x86_64" => "x64",
    "x86" => "x86",
    "aarch64" => "arm64",
    _ => return,
  };

  let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR missing"));
  let profile_dir = out_dir
    .ancestors()
    .nth(3)
    .expect("failed to resolve Cargo profile directory from OUT_DIR");

  let build_dir = profile_dir.join("build");
  let target_loader = profile_dir.join("WebView2Loader.dll");

  if target_loader.exists() {
    return;
  }

  if let Ok(entries) = fs::read_dir(&build_dir) {
    for entry in entries.flatten() {
      let path = entry.path();
      if !path.to_string_lossy().contains("webview2-com-sys") {
        continue;
      }

      let candidate = path.join("out").join(target_arch).join("WebView2Loader.dll");
      if candidate.exists() {
        fs::copy(&candidate, &target_loader).expect("failed to copy WebView2Loader.dll");
        println!(
          "cargo:warning=staged WebView2Loader.dll for windows-gnu at {}",
          target_loader.display()
        );
        return;
      }
    }
  }

  panic!(
    "failed to locate WebView2Loader.dll for windows-gnu; expected it under {}/webview2-com-sys*/out/{}/",
    build_dir.display(),
    target_arch
  );
}

#[cfg(not(windows))]
fn maybe_copy_webview2_loader() {}

fn main() {
  ensure_runtime_dir();
  maybe_copy_webview2_loader();
  tauri_build::build()
}
