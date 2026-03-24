use libc::free;
use libloading::Library;
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_void};
use std::path::{Path, PathBuf};

const SBX_MAX_AMP_ADJUST_POINTS: usize = 16;
const SBX_OK: c_int = 0;

#[repr(C)]
struct SbxAmpAdjustPoint {
  freq_hz: f64,
  adj: f64,
}

#[repr(C)]
struct SbxAmpAdjustSpec {
  point_count: usize,
  points: [SbxAmpAdjustPoint; SBX_MAX_AMP_ADJUST_POINTS],
}

#[repr(C)]
struct SbxMixModSpec {
  active: c_int,
  delta: f64,
  epsilon: f64,
  period_sec: f64,
  end_level: f64,
  main_len_sec: f64,
  wake_len_sec: f64,
  wake_enabled: c_int,
}

#[repr(C)]
struct SbxIsoEnvelopeSpec {
  start: f64,
  duty: f64,
  attack: f64,
  release: f64,
  edge_mode: c_int,
}

#[repr(C)]
struct SbxMixFxSpec {
  type_: c_int,
  waveform: c_int,
  carr: f64,
  res: f64,
  amp: f64,
  mixam_mode: c_int,
  mixam_start: f64,
  mixam_duty: f64,
  mixam_attack: f64,
  mixam_release: f64,
  mixam_edge_mode: c_int,
  mixam_floor: f64,
  mixam_bind_program_beat: c_int,
}

#[repr(C)]
struct SbxEngineConfig {
  sample_rate: f64,
  channels: c_int,
}

#[repr(C)]
struct SbxCurveEvalConfig {
  carrier_start_hz: f64,
  carrier_end_hz: f64,
  carrier_span_sec: f64,
  beat_start_hz: f64,
  beat_target_hz: f64,
  beat_span_sec: f64,
  hold_min: f64,
  total_min: f64,
  wake_min: f64,
  beat_amp0_pct: f64,
  mix_amp0_pct: f64,
}

#[repr(C)]
struct SbxSafeSeqfilePreamble {
  opt_s: c_int,
  opt_e: c_int,
  have_d: c_int,
  have_q: c_int,
  have_t: c_int,
  t_ms: c_int,
  have_l: c_int,
  l_ms: c_int,
  have_q_mult: c_int,
  q_mult: c_int,
  have_r: c_int,
  rate: c_int,
  have_r_rate: c_int,
  prate: c_int,
  have_b: c_int,
  pcm_bits: c_int,
  have_n: c_int,
  have_v: c_int,
  volume_pct: c_int,
  have_w: c_int,
  waveform: c_int,
  have_c: c_int,
  amp_adjust: SbxAmpAdjustSpec,
  have_a: c_int,
  mix_mod: SbxMixModSpec,
  have_i: c_int,
  iso_env: SbxIsoEnvelopeSpec,
  have_h: c_int,
  mixam_env: SbxMixFxSpec,
  have_w_output: c_int,
  have_f: c_int,
  fade_ms: c_int,
  #[cfg(target_os = "macos")]
  have_b_buffer: c_int,
  #[cfg(target_os = "macos")]
  buffer_samples: c_int,
  have_k: c_int,
  mp3_bitrate: c_int,
  have_j: c_int,
  mp3_quality: c_int,
  have_x: c_int,
  mp3_vbr_quality: f64,
  have_u: c_int,
  ogg_quality: f64,
  have_z: c_int,
  flac_compression: f64,
  #[cfg(target_os = "linux")]
  device_path: *mut c_char,
  mix_path: *mut c_char,
  out_path: *mut c_char,
}

#[repr(C)]
struct SbxContext {
  _private: [u8; 0],
}

#[repr(C)]
struct SbxCurveProgram {
  _private: [u8; 0],
}

type SbxDefaultEngineConfig = unsafe extern "C" fn(*mut SbxEngineConfig);
type SbxDefaultCurveEvalConfig = unsafe extern "C" fn(*mut SbxCurveEvalConfig);
type SbxDefaultSafeSeqfilePreamble = unsafe extern "C" fn(*mut SbxSafeSeqfilePreamble);
type SbxFreeSafeSeqfilePreamble = unsafe extern "C" fn(*mut SbxSafeSeqfilePreamble);
type SbxContextCreate = unsafe extern "C" fn(*const SbxEngineConfig) -> *mut SbxContext;
type SbxContextDestroy = unsafe extern "C" fn(*mut SbxContext);
type SbxContextSetDefaultWaveform = unsafe extern "C" fn(*mut SbxContext, c_int) -> c_int;
type SbxContextSetSequenceIsoOverride =
  unsafe extern "C" fn(*mut SbxContext, *const SbxIsoEnvelopeSpec) -> c_int;
type SbxContextSetSequenceMixamOverride =
  unsafe extern "C" fn(*mut SbxContext, *const SbxMixFxSpec) -> c_int;
type SbxPrepareSafeSeqText = unsafe extern "C" fn(
  *const c_char,
  *mut *mut c_char,
  *mut SbxSafeSeqfilePreamble,
  *mut c_char,
  usize,
) -> c_int;
type SbxContextLoadSbgTimingText =
  unsafe extern "C" fn(*mut SbxContext, *const c_char, c_int) -> c_int;
type SbxContextLastError = unsafe extern "C" fn(*const SbxContext) -> *const c_char;
type SbxCurveCreate = unsafe extern "C" fn() -> *mut SbxCurveProgram;
type SbxCurveDestroy = unsafe extern "C" fn(*mut SbxCurveProgram);
type SbxCurveLoadText =
  unsafe extern "C" fn(*mut SbxCurveProgram, *const c_char, *const c_char) -> c_int;
type SbxCurvePrepare = unsafe extern "C" fn(*mut SbxCurveProgram, *const SbxCurveEvalConfig) -> c_int;
type SbxCurveLastError = unsafe extern "C" fn(*const SbxCurveProgram) -> *const c_char;
type SbxVersion = unsafe extern "C" fn() -> *const c_char;
type SbxApiVersion = unsafe extern "C" fn() -> c_int;

pub struct ValidationDiagnostic {
  pub severity: &'static str,
  pub line: Option<u32>,
  pub column: Option<u32>,
  pub message: String,
}

pub struct ValidationOutcome {
  pub valid: bool,
  pub diagnostics: Vec<ValidationDiagnostic>,
  pub engine_version: String,
}

struct Api {
  _lib: Library,
  sbx_version: SbxVersion,
  sbx_api_version: SbxApiVersion,
  sbx_default_engine_config: SbxDefaultEngineConfig,
  sbx_default_curve_eval_config: SbxDefaultCurveEvalConfig,
  sbx_default_safe_seqfile_preamble: SbxDefaultSafeSeqfilePreamble,
  sbx_free_safe_seqfile_preamble: SbxFreeSafeSeqfilePreamble,
  sbx_context_create: SbxContextCreate,
  sbx_context_destroy: SbxContextDestroy,
  sbx_context_set_default_waveform: SbxContextSetDefaultWaveform,
  sbx_context_set_sequence_iso_override: SbxContextSetSequenceIsoOverride,
  sbx_context_set_sequence_mixam_override: SbxContextSetSequenceMixamOverride,
  sbx_prepare_safe_seq_text: SbxPrepareSafeSeqText,
  sbx_context_load_sbg_timing_text: SbxContextLoadSbgTimingText,
  sbx_context_last_error: SbxContextLastError,
  sbx_curve_create: SbxCurveCreate,
  sbx_curve_destroy: SbxCurveDestroy,
  sbx_curve_load_text: SbxCurveLoadText,
  sbx_curve_prepare: SbxCurvePrepare,
  sbx_curve_last_error: SbxCurveLastError,
}

impl Api {
  unsafe fn load_symbol<T: Copy>(lib: &Library, name: &[u8]) -> Result<T, String> {
    lib
      .get::<T>(name)
      .map(|sym| *sym)
      .map_err(|err| format!("failed to load symbol {}: {}", String::from_utf8_lossy(name), err))
  }

  fn load() -> Result<Self, String> {
    let path = resolve_library_path()?;
    let lib = unsafe { Library::new(&path) }
      .map_err(|err| format!("failed to open sbagenxlib at {}: {}", path.display(), err))?;
    unsafe {
      Ok(Self {
        sbx_version: Self::load_symbol(&lib, b"sbx_version\0")?,
        sbx_api_version: Self::load_symbol(&lib, b"sbx_api_version\0")?,
        sbx_default_engine_config: Self::load_symbol(&lib, b"sbx_default_engine_config\0")?,
        sbx_default_curve_eval_config: Self::load_symbol(&lib, b"sbx_default_curve_eval_config\0")?,
        sbx_default_safe_seqfile_preamble:
          Self::load_symbol(&lib, b"sbx_default_safe_seqfile_preamble\0")?,
        sbx_free_safe_seqfile_preamble:
          Self::load_symbol(&lib, b"sbx_free_safe_seqfile_preamble\0")?,
        sbx_context_create: Self::load_symbol(&lib, b"sbx_context_create\0")?,
        sbx_context_destroy: Self::load_symbol(&lib, b"sbx_context_destroy\0")?,
        sbx_context_set_default_waveform:
          Self::load_symbol(&lib, b"sbx_context_set_default_waveform\0")?,
        sbx_context_set_sequence_iso_override:
          Self::load_symbol(&lib, b"sbx_context_set_sequence_iso_override\0")?,
        sbx_context_set_sequence_mixam_override:
          Self::load_symbol(&lib, b"sbx_context_set_sequence_mixam_override\0")?,
        sbx_prepare_safe_seq_text: Self::load_symbol(&lib, b"sbx_prepare_safe_seq_text\0")?,
        sbx_context_load_sbg_timing_text:
          Self::load_symbol(&lib, b"sbx_context_load_sbg_timing_text\0")?,
        sbx_context_last_error: Self::load_symbol(&lib, b"sbx_context_last_error\0")?,
        sbx_curve_create: Self::load_symbol(&lib, b"sbx_curve_create\0")?,
        sbx_curve_destroy: Self::load_symbol(&lib, b"sbx_curve_destroy\0")?,
        sbx_curve_load_text: Self::load_symbol(&lib, b"sbx_curve_load_text\0")?,
        sbx_curve_prepare: Self::load_symbol(&lib, b"sbx_curve_prepare\0")?,
        sbx_curve_last_error: Self::load_symbol(&lib, b"sbx_curve_last_error\0")?,
        _lib: lib,
      })
    }
  }

  fn version_string(&self) -> String {
    unsafe { cstr_to_string((self.sbx_version)()) }
  }

  fn api_version(&self) -> i32 {
    unsafe { (self.sbx_api_version)() as i32 }
  }
}

fn resolve_library_path() -> Result<PathBuf, String> {
  let repo_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
    .join("../..")
    .canonicalize()
    .map_err(|err| format!("failed to resolve repo root: {}", err))?;

  let dist = repo_root.join("dist")
  ;
  let candidates = if cfg!(target_os = "linux") {
    vec![
      dist.join("libsbagenx.so"),
      dist.join("libsbagenx.so.3"),
      dist.join("libsbagenx.so.3.8.1"),
    ]
  } else if cfg!(target_os = "windows") {
    if cfg!(target_pointer_width = "64") {
      vec![dist.join("sbagenxlib-win64.dll")]
    } else {
      vec![dist.join("sbagenxlib-win32.dll")]
    }
  } else if cfg!(target_os = "macos") {
    vec![dist.join("libsbagenx.dylib")]
  } else {
    Vec::new()
  };

  candidates
    .into_iter()
    .find(|p| p.exists())
    .ok_or_else(|| "unable to locate a usable sbagenxlib runtime library in dist/".to_string())
}

fn cstr_to_string(ptr: *const c_char) -> String {
  if ptr.is_null() {
    return String::new();
  }
  unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() }
}

fn make_diagnostic(message: String) -> ValidationDiagnostic {
  let (line, column) = extract_location(&message);
  ValidationDiagnostic {
    severity: "error",
    line,
    column,
    message,
  }
}

fn extract_location(message: &str) -> (Option<u32>, Option<u32>) {
  if let Some(pos) = message.find("line ") {
    let rest = &message[pos + 5..];
    let digits: String = rest.chars().take_while(|c| c.is_ascii_digit()).collect();
    if let Ok(line) = digits.parse::<u32>() {
      return (Some(line), None);
    }
  }

  let bytes = message.as_bytes();
  for i in 0..bytes.len() {
    if bytes[i] != b':' {
      continue;
    }
    let mut j = i + 1;
    let start_line = j;
    while j < bytes.len() && bytes[j].is_ascii_digit() {
      j += 1;
    }
    if j == start_line || j >= bytes.len() || bytes[j] != b':' {
      continue;
    }
    if let Ok(line) = message[start_line..j].parse::<u32>() {
      let mut k = j + 1;
      let start_col = k;
      while k < bytes.len() && bytes[k].is_ascii_digit() {
        k += 1;
      }
      if k > start_col && k < bytes.len() && bytes[k] == b':' {
        if let Ok(column) = message[start_col..k].parse::<u32>() {
          return (Some(line), Some(column));
        }
      }
      return (Some(line), None);
    }
  }

  (None, None)
}

pub fn backend_status() -> Result<(String, i32), String> {
  let api = Api::load()?;
  Ok((api.version_string(), api.api_version()))
}

pub fn validate_sbg(text: &str, source_name: &str) -> Result<ValidationOutcome, String> {
  let api = Api::load()?;
  let c_text = CString::new(text).map_err(|_| "document contains embedded NUL byte".to_string())?;
  let mut errbuf = vec![0 as c_char; 512];
  let mut prepared_text: *mut c_char = std::ptr::null_mut();
  let mut preamble = unsafe { std::mem::zeroed::<SbxSafeSeqfilePreamble>() };
  unsafe { (api.sbx_default_safe_seqfile_preamble)(&mut preamble) };
  let rc = unsafe {
    (api.sbx_prepare_safe_seq_text)(
      c_text.as_ptr(),
      &mut prepared_text,
      &mut preamble,
      errbuf.as_mut_ptr(),
      errbuf.len(),
    )
  };
  if rc != SBX_OK {
    unsafe { (api.sbx_free_safe_seqfile_preamble)(&mut preamble) };
    let msg = cstr_to_string(errbuf.as_ptr());
    return Ok(ValidationOutcome {
      valid: false,
      diagnostics: vec![make_diagnostic(if msg.is_empty() {
        "SBaGenX safe preamble validation failed".to_string()
      } else {
        msg
      })],
      engine_version: format!("{} (api {})", api.version_string(), api.api_version()),
    });
  }

  let mut eng_cfg = SbxEngineConfig {
    sample_rate: 0.0,
    channels: 0,
  };
  unsafe { (api.sbx_default_engine_config)(&mut eng_cfg) };
  let ctx = unsafe { (api.sbx_context_create)(&eng_cfg) };
  if ctx.is_null() {
    unsafe {
      (api.sbx_free_safe_seqfile_preamble)(&mut preamble);
      free(prepared_text as *mut c_void);
    }
    return Err("failed to create sbagenxlib context".to_string());
  }

  if preamble.have_w != 0 {
    unsafe {
      (api.sbx_context_set_default_waveform)(ctx, preamble.waveform);
    }
  }
  if preamble.have_i != 0 {
    unsafe {
      (api.sbx_context_set_sequence_iso_override)(ctx, &preamble.iso_env);
    }
  }
  if preamble.have_h != 0 {
    unsafe {
      (api.sbx_context_set_sequence_mixam_override)(ctx, &preamble.mixam_env);
    }
  }

  let load_rc = unsafe { (api.sbx_context_load_sbg_timing_text)(ctx, prepared_text, 0) };
  let outcome = if load_rc == SBX_OK {
    ValidationOutcome {
      valid: true,
      diagnostics: Vec::new(),
      engine_version: format!("{} (api {})", api.version_string(), api.api_version()),
    }
  } else {
    let msg = unsafe { cstr_to_string((api.sbx_context_last_error)(ctx)) };
    ValidationOutcome {
      valid: false,
      diagnostics: vec![make_diagnostic(if msg.is_empty() {
        format!("SBaGenX validation failed for {}", source_name)
      } else {
        msg
      })],
      engine_version: format!("{} (api {})", api.version_string(), api.api_version()),
    }
  };

  unsafe {
    (api.sbx_context_destroy)(ctx);
    (api.sbx_free_safe_seqfile_preamble)(&mut preamble);
    free(prepared_text as *mut c_void);
  }
  Ok(outcome)
}

pub fn validate_sbgf(text: &str, source_name: &str) -> Result<ValidationOutcome, String> {
  let api = Api::load()?;
  let c_text = CString::new(text).map_err(|_| "document contains embedded NUL byte".to_string())?;
  let c_source = CString::new(source_name).map_err(|_| "source name contains embedded NUL byte".to_string())?;
  let curve = unsafe { (api.sbx_curve_create)() };
  if curve.is_null() {
    return Err("failed to create sbagenxlib curve program".to_string());
  }

  let mut eval_cfg = SbxCurveEvalConfig {
    carrier_start_hz: 0.0,
    carrier_end_hz: 0.0,
    carrier_span_sec: 0.0,
    beat_start_hz: 0.0,
    beat_target_hz: 0.0,
    beat_span_sec: 0.0,
    hold_min: 0.0,
    total_min: 0.0,
    wake_min: 0.0,
    beat_amp0_pct: 0.0,
    mix_amp0_pct: 0.0,
  };
  unsafe { (api.sbx_default_curve_eval_config)(&mut eval_cfg) };

  let load_rc = unsafe { (api.sbx_curve_load_text)(curve, c_text.as_ptr(), c_source.as_ptr()) };
  let outcome = if load_rc != SBX_OK {
    let msg = unsafe { cstr_to_string((api.sbx_curve_last_error)(curve)) };
    ValidationOutcome {
      valid: false,
      diagnostics: vec![make_diagnostic(if msg.is_empty() {
        format!("SBaGenX curve validation failed for {}", source_name)
      } else {
        msg
      })],
      engine_version: format!("{} (api {})", api.version_string(), api.api_version()),
    }
  } else {
    let prep_rc = unsafe { (api.sbx_curve_prepare)(curve, &eval_cfg) };
    if prep_rc == SBX_OK {
      ValidationOutcome {
        valid: true,
        diagnostics: Vec::new(),
        engine_version: format!("{} (api {})", api.version_string(), api.api_version()),
      }
    } else {
      let msg = unsafe { cstr_to_string((api.sbx_curve_last_error)(curve)) };
      ValidationOutcome {
        valid: false,
        diagnostics: vec![make_diagnostic(if msg.is_empty() {
          format!("SBaGenX curve preparation failed for {}", source_name)
        } else {
          msg
        })],
        engine_version: format!("{} (api {})", api.version_string(), api.api_version()),
      }
    }
  };

  unsafe {
    (api.sbx_curve_destroy)(curve);
  }
  Ok(outcome)
}

pub fn normalize_source_name(source_name: Option<String>, fallback_kind: &str) -> String {
  source_name.unwrap_or_else(|| format!("<untitled.{}>", fallback_kind))
}

pub fn kind_from_path(path: &Path) -> Option<&'static str> {
  match path
    .extension()
    .and_then(|ext| ext.to_str())
    .map(|ext| ext.to_ascii_lowercase())
    .as_deref()
  {
    Some("sbg") => Some("sbg"),
    Some("sbgf") => Some("sbgf"),
    _ => None,
  }
}
