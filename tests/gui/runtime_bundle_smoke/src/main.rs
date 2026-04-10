use std::path::PathBuf;

#[path = "../../../../gui/src-tauri/src/sbagenxlib.rs"]
mod sbagenxlib;

fn main() {
  let repo_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
    .join("../../..")
    .canonicalize()
    .expect("resolve repo root");
  let library_path = if cfg!(target_os = "linux") {
    repo_root.join("gui/src-tauri/runtime-bundle/libsbagenx.so.3")
  } else if cfg!(target_os = "windows") {
    if cfg!(target_pointer_width = "64") {
      repo_root.join("gui/src-tauri/runtime-bundle/sbagenxlib-win64.dll")
    } else {
      repo_root.join("gui/src-tauri/runtime-bundle/sbagenxlib-win32.dll")
    }
  } else if cfg!(target_os = "macos") {
    repo_root.join("gui/src-tauri/runtime-bundle/libsbagenx.dylib")
  } else {
    panic!("unsupported platform");
  };

  std::env::set_var(
    "SBAGENX_GUI_RUNTIME_BUNDLE_LIBRARY",
    library_path.as_os_str(),
  );

  match sbagenxlib::smoke_runtime_bundle_abi_validation() {
    Ok(engine_version) => {
      println!(
        "PASS: runtime bundle ABI validation succeeded (sbagenxlib {})",
        engine_version
      );
    }
    Err(err) => {
      eprintln!("ERROR: {}", err);
      std::process::exit(1);
    }
  }
}
