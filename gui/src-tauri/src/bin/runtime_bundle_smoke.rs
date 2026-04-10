fn main() {
  match sbagenx_gui_lib::smoke_runtime_bundle_abi_validation() {
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
