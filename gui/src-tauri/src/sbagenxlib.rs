use libc::{free, FILE};
use libloading::Library;
use std::ffi::{CStr, CString};
use std::fs;
#[cfg(not(target_os = "windows"))]
use std::os::unix::ffi::OsStrExt;
#[cfg(target_os = "windows")]
use std::os::windows::ffi::OsStrExt;
use std::os::raw::{c_char, c_int, c_void};
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

const SBX_MAX_AMP_ADJUST_POINTS: usize = 16;
const SBX_DIAG_CODE_MAX: usize = 32;
const SBX_DIAG_MESSAGE_MAX: usize = 256;
const SBX_OK: c_int = 0;
const SBX_DIAG_ERROR: c_int = 1;
const SBX_AUDIO_FILE_WAV: c_int = 1;
const SBX_AUDIO_FILE_OGG: c_int = 2;
const SBX_AUDIO_FILE_FLAC: c_int = 3;
const SBX_AUDIO_FILE_MP3: c_int = 4;
const SBX_AUDIO_WRITER_INPUT_BYTES: c_int = 0;
const SBX_AUDIO_WRITER_INPUT_S16: c_int = 1;
const SBX_AUDIO_WRITER_INPUT_F32: c_int = 2;
const SBX_AUDIO_WRITER_INPUT_I32: c_int = 3;
const PREVIEW_LIMIT_SEC: f64 = 120.0;
const RENDER_CHUNK_FRAMES: usize = 2048;

#[cfg(target_os = "windows")]
unsafe extern "C" {
  fn _wfopen(filename: *const u16, mode: *const u16) -> *mut FILE;
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SbxAmpAdjustPoint {
  freq_hz: f64,
  adj: f64,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SbxAmpAdjustSpec {
  point_count: usize,
  points: [SbxAmpAdjustPoint; SBX_MAX_AMP_ADJUST_POINTS],
}

#[repr(C)]
#[derive(Clone, Copy)]
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
#[derive(Clone, Copy)]
struct SbxIsoEnvelopeSpec {
  start: f64,
  duty: f64,
  attack: f64,
  release: f64,
  edge_mode: c_int,
}

#[repr(C)]
#[derive(Clone, Copy)]
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
#[derive(Clone, Copy)]
struct SbxEngineConfig {
  sample_rate: f64,
  channels: c_int,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SbxPcmConvertState {
  rng_state: u32,
  dither_mode: c_int,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SbxAudioWriterConfig {
  sample_rate: f64,
  channels: c_int,
  format: c_int,
  pcm_bits: c_int,
  ogg_quality: f64,
  ogg_quality_set: c_int,
  flac_compression: f64,
  flac_compression_set: c_int,
  mp3_bitrate: c_int,
  mp3_bitrate_set: c_int,
  mp3_quality: c_int,
  mp3_quality_set: c_int,
  mp3_vbr_quality: f64,
  mp3_vbr_quality_set: c_int,
  prefer_float_input: c_int,
}

#[repr(C)]
#[allow(non_snake_case)]
struct SbxSafeSeqfilePreamble {
  opt_S: c_int,
  opt_E: c_int,
  have_D: c_int,
  have_Q: c_int,
  have_T: c_int,
  T_ms: c_int,
  have_L: c_int,
  L_ms: c_int,
  have_q: c_int,
  q_mult: c_int,
  have_r: c_int,
  rate: c_int,
  have_R: c_int,
  prate: c_int,
  have_b: c_int,
  pcm_bits: c_int,
  have_N: c_int,
  have_V: c_int,
  volume_pct: c_int,
  have_w: c_int,
  waveform: c_int,
  have_c: c_int,
  amp_adjust: SbxAmpAdjustSpec,
  have_A: c_int,
  mix_mod: SbxMixModSpec,
  have_I: c_int,
  iso_env: SbxIsoEnvelopeSpec,
  have_H: c_int,
  mixam_env: SbxMixFxSpec,
  have_W: c_int,
  have_F: c_int,
  fade_ms: c_int,
  #[cfg(target_os = "macos")]
  have_B: c_int,
  #[cfg(target_os = "macos")]
  buffer_samples: c_int,
  have_K: c_int,
  mp3_bitrate: c_int,
  have_J: c_int,
  mp3_quality: c_int,
  have_X: c_int,
  mp3_vbr_quality: f64,
  have_U: c_int,
  ogg_quality: f64,
  have_Z: c_int,
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

#[repr(C)]
struct SbxAudioWriter {
  _private: [u8; 0],
}

#[repr(C)]
struct SbxMixInput {
  _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SbxMixInputConfig {
  mix_section: c_int,
  output_rate_hz: c_int,
  output_rate_is_default: c_int,
  take_stream_ownership: c_int,
  warn_cb: Option<unsafe extern "C" fn(*mut c_void, *const c_char)>,
  warn_user: *mut c_void,
}

#[repr(C)]
struct SbxDiagnosticRaw {
  severity: c_int,
  code: [c_char; SBX_DIAG_CODE_MAX],
  line: u32,
  column: u32,
  end_line: u32,
  end_column: u32,
  message: [c_char; SBX_DIAG_MESSAGE_MAX],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SbxCurveInfoRaw {
  parameter_count: usize,
  has_solve: c_int,
  has_carrier_expr: c_int,
  has_amp_expr: c_int,
  has_mixamp_expr: c_int,
  beat_piece_count: usize,
  carrier_piece_count: usize,
  amp_piece_count: usize,
  mixamp_piece_count: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
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
#[derive(Clone, Copy)]
struct SbxToneSpec {
  mode: c_int,
  carrier_hz: f64,
  beat_hz: f64,
  amplitude: f64,
  waveform: c_int,
  envelope_waveform: c_int,
  duty_cycle: f64,
  iso_start: f64,
  iso_attack: f64,
  iso_release: f64,
  iso_edge_mode: c_int,
}

type SbxDefaultEngineConfig = unsafe extern "C" fn(*mut SbxEngineConfig);
type SbxDefaultPcmConvertState = unsafe extern "C" fn(*mut SbxPcmConvertState);
type SbxDefaultAudioWriterConfig = unsafe extern "C" fn(*mut SbxAudioWriterConfig);
type SbxDefaultSafeSeqfilePreamble = unsafe extern "C" fn(*mut SbxSafeSeqfilePreamble);
type SbxFreeSafeSeqfilePreamble = unsafe extern "C" fn(*mut SbxSafeSeqfilePreamble);
type SbxDefaultMixInputConfig = unsafe extern "C" fn(*mut SbxMixInputConfig);
type SbxDefaultCurveEvalConfig = unsafe extern "C" fn(*mut SbxCurveEvalConfig);
type SbxCurveCreate = unsafe extern "C" fn() -> *mut SbxCurveProgram;
type SbxCurveDestroy = unsafe extern "C" fn(*mut SbxCurveProgram);
type SbxCurveLoadText =
  unsafe extern "C" fn(*mut SbxCurveProgram, *const c_char, *const c_char) -> c_int;
type SbxCurvePrepare = unsafe extern "C" fn(*mut SbxCurveProgram, *const SbxCurveEvalConfig) -> c_int;
type SbxCurveGetInfo = unsafe extern "C" fn(*const SbxCurveProgram, *mut SbxCurveInfoRaw) -> c_int;
type SbxCurveParamCount = unsafe extern "C" fn(*const SbxCurveProgram) -> usize;
type SbxCurveGetParam =
  unsafe extern "C" fn(*const SbxCurveProgram, usize, *mut *const c_char, *mut f64) -> c_int;
type SbxCurveLastError = unsafe extern "C" fn(*const SbxCurveProgram) -> *const c_char;
type SbxContextCreate = unsafe extern "C" fn(*const SbxEngineConfig) -> *mut SbxContext;
type SbxContextDestroy = unsafe extern "C" fn(*mut SbxContext);
type SbxContextSetDefaultWaveform = unsafe extern "C" fn(*mut SbxContext, c_int) -> c_int;
type SbxContextSetSequenceIsoOverride =
  unsafe extern "C" fn(*mut SbxContext, *const SbxIsoEnvelopeSpec) -> c_int;
type SbxContextSetSequenceMixamOverride =
  unsafe extern "C" fn(*mut SbxContext, *const SbxMixFxSpec) -> c_int;
type SbxContextSetAmpAdjust = unsafe extern "C" fn(*mut SbxContext, *const SbxAmpAdjustSpec) -> c_int;
type SbxContextSetMixMod = unsafe extern "C" fn(*mut SbxContext, *const SbxMixModSpec) -> c_int;
type SbxPrepareSafeSeqText = unsafe extern "C" fn(
  *const c_char,
  *mut *mut c_char,
  *mut SbxSafeSeqfilePreamble,
  *mut c_char,
  usize,
) -> c_int;
type SbxValidateSbgText =
  unsafe extern "C" fn(*const c_char, *const c_char, *mut *mut SbxDiagnosticRaw, *mut usize) -> c_int;
type SbxValidateSbgfText =
  unsafe extern "C" fn(*const c_char, *const c_char, *mut *mut SbxDiagnosticRaw, *mut usize) -> c_int;
type SbxFreeDiagnostics = unsafe extern "C" fn(*mut SbxDiagnosticRaw);
type SbxContextLoadSbgTimingText =
  unsafe extern "C" fn(*mut SbxContext, *const c_char, c_int) -> c_int;
type SbxContextLastError = unsafe extern "C" fn(*const SbxContext) -> *const c_char;
type SbxContextDurationSec = unsafe extern "C" fn(*const SbxContext) -> f64;
type SbxContextIsLooping = unsafe extern "C" fn(*const SbxContext) -> c_int;
type SbxContextHasMixEffects = unsafe extern "C" fn(*const SbxContext) -> c_int;
type SbxContextTimeSec = unsafe extern "C" fn(*const SbxContext) -> f64;
type SbxContextRenderF32 = unsafe extern "C" fn(*mut SbxContext, *mut f32, usize) -> c_int;
type SbxContextMixStreamSample = unsafe extern "C" fn(
  *mut SbxContext,
  f64,
  c_int,
  c_int,
  f64,
  *mut f64,
  *mut f64,
) -> c_int;
type SbxContextVoiceCount = unsafe extern "C" fn(*const SbxContext) -> usize;
type SbxContextSampleProgramBeatVoice =
  unsafe extern "C" fn(*mut SbxContext, usize, f64, f64, usize, *mut f64, *mut f64) -> c_int;
type SbxContextSampleTonesVoice = unsafe extern "C" fn(
  *mut SbxContext,
  usize,
  f64,
  f64,
  usize,
  *mut f64,
  *mut SbxToneSpec,
) -> c_int;
type SbxVersion = unsafe extern "C" fn() -> *const c_char;
type SbxApiVersion = unsafe extern "C" fn() -> c_int;
type SbxAudioWriterCreatePath =
  unsafe extern "C" fn(*const c_char, *const SbxAudioWriterConfig) -> *mut SbxAudioWriter;
type SbxAudioWriterClose = unsafe extern "C" fn(*mut SbxAudioWriter) -> c_int;
type SbxAudioWriterDestroy = unsafe extern "C" fn(*mut SbxAudioWriter);
type SbxAudioWriterLastError = unsafe extern "C" fn(*const SbxAudioWriter) -> *const c_char;
type SbxAudioWriterInputMode = unsafe extern "C" fn(*const SbxAudioWriter) -> c_int;
type SbxAudioWriterWriteBytes = unsafe extern "C" fn(*mut SbxAudioWriter, *const c_void, usize) -> c_int;
type SbxAudioWriterWriteS16 = unsafe extern "C" fn(*mut SbxAudioWriter, *const i16, usize) -> c_int;
type SbxAudioWriterWriteF32 = unsafe extern "C" fn(*mut SbxAudioWriter, *const f32, usize) -> c_int;
type SbxAudioWriterWriteI32 = unsafe extern "C" fn(*mut SbxAudioWriter, *const i32, usize) -> c_int;
type SbxConvertF32ToS16Ex =
  unsafe extern "C" fn(*const f32, *mut i16, usize, *mut SbxPcmConvertState) -> c_int;
type SbxConvertF32ToS24_32 =
  unsafe extern "C" fn(*const f32, *mut i32, usize, *mut SbxPcmConvertState) -> c_int;
type SbxMixInputCreateStdio =
  unsafe extern "C" fn(*mut FILE, *const c_char, *const SbxMixInputConfig) -> *mut SbxMixInput;
type SbxMixInputRead = unsafe extern "C" fn(*mut SbxMixInput, *mut c_int, c_int) -> c_int;
type SbxMixInputDestroy = unsafe extern "C" fn(*mut SbxMixInput);
type SbxMixInputLastError = unsafe extern "C" fn(*const SbxMixInput) -> *const c_char;
type SbxMixInputOutputRate = unsafe extern "C" fn(*const SbxMixInput) -> c_int;

pub struct ValidationDiagnostic {
  pub severity: &'static str,
  pub line: Option<u32>,
  pub column: Option<u32>,
  pub end_line: Option<u32>,
  pub end_column: Option<u32>,
  pub message: String,
}

pub struct ValidationOutcome {
  pub valid: bool,
  pub diagnostics: Vec<ValidationDiagnostic>,
  pub engine_version: String,
}

pub struct PreviewOutcome {
  pub audio_path: String,
  pub duration_sec: f64,
  pub limited: bool,
  pub engine_version: String,
}

pub struct ExportOutcome {
  pub output_path: String,
  pub duration_sec: f64,
  pub format: String,
  pub engine_version: String,
}

pub struct BeatPreviewPoint {
  pub t_sec: f64,
  pub beat_hz: Option<f64>,
}

pub struct BeatPreviewSeries {
  pub voice_index: usize,
  pub label: String,
  pub active_sample_count: usize,
  pub points: Vec<BeatPreviewPoint>,
}

pub struct BeatPreviewOutcome {
  pub duration_sec: f64,
  pub sample_count: usize,
  pub voice_count: usize,
  pub min_hz: Option<f64>,
  pub max_hz: Option<f64>,
  pub limited: bool,
  pub time_label: String,
  pub series: Vec<BeatPreviewSeries>,
  pub engine_version: String,
}

pub struct CurveParameter {
  pub name: String,
  pub value: f64,
}

pub struct CurveInfoOutcome {
  pub parameter_count: usize,
  pub has_solve: bool,
  pub has_carrier_expr: bool,
  pub has_amp_expr: bool,
  pub has_mixamp_expr: bool,
  pub beat_piece_count: usize,
  pub carrier_piece_count: usize,
  pub amp_piece_count: usize,
  pub mixamp_piece_count: usize,
  pub parameters: Vec<CurveParameter>,
  pub engine_version: String,
}

struct Api {
  _lib: Library,
  sbx_version: SbxVersion,
  sbx_api_version: SbxApiVersion,
  sbx_default_engine_config: SbxDefaultEngineConfig,
  sbx_default_pcm_convert_state: SbxDefaultPcmConvertState,
  sbx_default_audio_writer_config: SbxDefaultAudioWriterConfig,
  sbx_default_safe_seqfile_preamble: SbxDefaultSafeSeqfilePreamble,
  sbx_default_mix_input_config: SbxDefaultMixInputConfig,
  sbx_default_curve_eval_config: SbxDefaultCurveEvalConfig,
  sbx_free_safe_seqfile_preamble: SbxFreeSafeSeqfilePreamble,
  sbx_curve_create: SbxCurveCreate,
  sbx_curve_destroy: SbxCurveDestroy,
  sbx_curve_load_text: SbxCurveLoadText,
  sbx_curve_prepare: SbxCurvePrepare,
  sbx_curve_get_info: SbxCurveGetInfo,
  sbx_curve_param_count: SbxCurveParamCount,
  sbx_curve_get_param: SbxCurveGetParam,
  sbx_curve_last_error: SbxCurveLastError,
  sbx_context_create: SbxContextCreate,
  sbx_context_destroy: SbxContextDestroy,
  sbx_context_set_default_waveform: SbxContextSetDefaultWaveform,
  sbx_context_set_sequence_iso_override: SbxContextSetSequenceIsoOverride,
  sbx_context_set_sequence_mixam_override: SbxContextSetSequenceMixamOverride,
  sbx_context_set_amp_adjust: SbxContextSetAmpAdjust,
  sbx_context_set_mix_mod: SbxContextSetMixMod,
  sbx_prepare_safe_seq_text: SbxPrepareSafeSeqText,
  sbx_validate_sbg_text: SbxValidateSbgText,
  sbx_validate_sbgf_text: SbxValidateSbgfText,
  sbx_free_diagnostics: SbxFreeDiagnostics,
  sbx_context_load_sbg_timing_text: SbxContextLoadSbgTimingText,
  sbx_context_last_error: SbxContextLastError,
  sbx_context_duration_sec: SbxContextDurationSec,
  sbx_context_is_looping: SbxContextIsLooping,
  sbx_context_has_mix_effects: SbxContextHasMixEffects,
  sbx_context_time_sec: SbxContextTimeSec,
  sbx_context_render_f32: SbxContextRenderF32,
  sbx_context_mix_stream_sample: SbxContextMixStreamSample,
  sbx_context_voice_count: SbxContextVoiceCount,
  sbx_context_sample_program_beat_voice: SbxContextSampleProgramBeatVoice,
  sbx_context_sample_tones_voice: SbxContextSampleTonesVoice,
  sbx_audio_writer_create_path: SbxAudioWriterCreatePath,
  sbx_audio_writer_close: SbxAudioWriterClose,
  sbx_audio_writer_destroy: SbxAudioWriterDestroy,
  sbx_audio_writer_last_error: SbxAudioWriterLastError,
  sbx_audio_writer_input_mode: SbxAudioWriterInputMode,
  sbx_audio_writer_write_bytes: SbxAudioWriterWriteBytes,
  sbx_audio_writer_write_s16: SbxAudioWriterWriteS16,
  sbx_audio_writer_write_f32: SbxAudioWriterWriteF32,
  sbx_audio_writer_write_i32: SbxAudioWriterWriteI32,
  sbx_convert_f32_to_s16_ex: SbxConvertF32ToS16Ex,
  sbx_convert_f32_to_s24_32: SbxConvertF32ToS24_32,
  sbx_mix_input_create_stdio: SbxMixInputCreateStdio,
  sbx_mix_input_read: SbxMixInputRead,
  sbx_mix_input_destroy: SbxMixInputDestroy,
  sbx_mix_input_last_error: SbxMixInputLastError,
  sbx_mix_input_output_rate: SbxMixInputOutputRate,
}

struct MixInputResource {
  handle: *mut SbxMixInput,
}

struct LoadedSbgContext {
  api: Api,
  ctx: *mut SbxContext,
  prepared_text: *mut c_char,
  preamble: SbxSafeSeqfilePreamble,
  engine_cfg: SbxEngineConfig,
  mix_input: Option<MixInputResource>,
}

impl Drop for LoadedSbgContext {
  fn drop(&mut self) {
    unsafe {
      if let Some(mix_input) = self.mix_input.take() {
        (self.api.sbx_mix_input_destroy)(mix_input.handle);
      }
      if !self.ctx.is_null() {
        (self.api.sbx_context_destroy)(self.ctx);
      }
      (self.api.sbx_free_safe_seqfile_preamble)(&mut self.preamble);
      if !self.prepared_text.is_null() {
        free(self.prepared_text as *mut c_void);
      }
    }
  }
}

pub(crate) struct LivePlaybackContext {
  loaded: LoadedSbgContext,
  volume_mul: f32,
  remaining_frames: usize,
  finished: bool,
  duration_sec: f64,
  limited: bool,
  mix_buf: Vec<i32>,
}

unsafe impl Send for LivePlaybackContext {}

impl LivePlaybackContext {
  pub(crate) fn channel_count(&self) -> usize {
    self.loaded.engine_cfg.channels.max(1) as usize
  }

  pub(crate) fn engine_version(&self) -> String {
    self.loaded.api.version_string()
  }

  pub(crate) fn duration_sec(&self) -> f64 {
    self.duration_sec
  }

  pub(crate) fn limited(&self) -> bool {
    self.limited
  }

  pub(crate) fn is_finished(&self) -> bool {
    self.finished
  }

  pub(crate) fn render_interleaved_f32(&mut self, out: &mut [f32]) -> Result<usize, String> {
    if self.finished {
      out.fill(0.0);
      return Ok(0);
    }

    let channels = self.channel_count();
    if channels == 0 || out.is_empty() {
      return Ok(0);
    }

    let requested_frames = out.len() / channels;
    if requested_frames == 0 {
      return Ok(0);
    }

    let frames = requested_frames.min(self.remaining_frames);
    if frames == 0 {
      self.finished = true;
      out.fill(0.0);
      return Ok(0);
    }

    let sample_count = frames * channels;
    let rendered_frames = render_frames_with_mix(
      &mut self.loaded,
      &mut self.mix_buf,
      &mut out[..sample_count],
      frames,
      self.volume_mul,
    )?;
    if rendered_frames == 0 {
      self.finished = true;
      out.fill(0.0);
      return Ok(0);
    }

    if sample_count < out.len() {
      out[sample_count..].fill(0.0);
    }

    self.remaining_frames = self.remaining_frames.saturating_sub(rendered_frames);
    if self.remaining_frames == 0 {
      self.finished = true;
    }

    Ok(rendered_frames)
  }
}

fn render_frames_with_mix(
  loaded: &mut LoadedSbgContext,
  mix_buf: &mut Vec<i32>,
  out: &mut [f32],
  frames: usize,
  volume_mul: f32,
) -> Result<usize, String> {
  let channels = loaded.engine_cfg.channels.max(1) as usize;
  let sample_count = frames * channels;
  let t0 = unsafe { (loaded.api.sbx_context_time_sec)(loaded.ctx) };
  let rc = unsafe { (loaded.api.sbx_context_render_f32)(loaded.ctx, out.as_mut_ptr(), frames) };
  if rc != SBX_OK {
    let msg = unsafe { cstr_to_string((loaded.api.sbx_context_last_error)(loaded.ctx)) };
    return Err(if msg.is_empty() {
      "failed to render audio frames from sbagenxlib".to_string()
    } else {
      msg
    });
  }

  if let Some(mix_input) = loaded.mix_input.as_mut() {
    let mix_sample_count = frames * 2;
    if mix_buf.len() < mix_sample_count {
      mix_buf.resize(mix_sample_count, 0);
    }
    let read_count = unsafe {
      (loaded.api.sbx_mix_input_read)(mix_input.handle, mix_buf.as_mut_ptr(), mix_sample_count as c_int)
    };
    if read_count < 0 {
      let msg = unsafe { cstr_to_string((loaded.api.sbx_mix_input_last_error)(mix_input.handle)) };
      return Err(if msg.is_empty() {
        "failed to read sequence mix input stream".to_string()
      } else {
        msg
      });
    }
    if read_count == 0 {
      return Ok(0);
    }
    if read_count < mix_sample_count as c_int {
      mix_buf[read_count as usize..mix_sample_count].fill(0);
    }

    let sample_rate_hz = loaded.engine_cfg.sample_rate;
    for frame_idx in 0..frames {
      let mix_base = frame_idx * 2;
      let mut add_l = 0.0;
      let mut add_r = 0.0;
      let rc = unsafe {
        (loaded.api.sbx_context_mix_stream_sample)(
          loaded.ctx,
          t0 + (frame_idx as f64 / sample_rate_hz),
          mix_buf[mix_base],
          mix_buf[mix_base + 1],
          1.0,
          &mut add_l,
          &mut add_r,
        )
      };
      if rc != SBX_OK {
        let msg = unsafe { cstr_to_string((loaded.api.sbx_context_last_error)(loaded.ctx)) };
        return Err(if msg.is_empty() {
          "sbagenxlib mix stream processing failed".to_string()
        } else {
          msg
        });
      }

      if channels == 1 {
        let mono = out[frame_idx] as f64 + ((add_l + add_r) * 0.5 / 32767.0);
        out[frame_idx] = mono as f32;
        continue;
      }

      let frame_base = frame_idx * channels;
      out[frame_base] += (add_l / 32767.0) as f32;
      out[frame_base + 1] += (add_r / 32767.0) as f32;
    }
  }

  if volume_mul != 1.0 {
    for sample in out.iter_mut().take(sample_count) {
      *sample *= volume_mul;
    }
  }

  Ok(frames)
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
        sbx_default_pcm_convert_state:
          Self::load_symbol(&lib, b"sbx_default_pcm_convert_state\0")?,
        sbx_default_audio_writer_config:
          Self::load_symbol(&lib, b"sbx_default_audio_writer_config\0")?,
        sbx_default_safe_seqfile_preamble:
          Self::load_symbol(&lib, b"sbx_default_safe_seqfile_preamble\0")?,
        sbx_default_mix_input_config:
          Self::load_symbol(&lib, b"sbx_default_mix_input_config\0")?,
        sbx_default_curve_eval_config:
          Self::load_symbol(&lib, b"sbx_default_curve_eval_config\0")?,
        sbx_free_safe_seqfile_preamble:
          Self::load_symbol(&lib, b"sbx_free_safe_seqfile_preamble\0")?,
        sbx_curve_create: Self::load_symbol(&lib, b"sbx_curve_create\0")?,
        sbx_curve_destroy: Self::load_symbol(&lib, b"sbx_curve_destroy\0")?,
        sbx_curve_load_text: Self::load_symbol(&lib, b"sbx_curve_load_text\0")?,
        sbx_curve_prepare: Self::load_symbol(&lib, b"sbx_curve_prepare\0")?,
        sbx_curve_get_info: Self::load_symbol(&lib, b"sbx_curve_get_info\0")?,
        sbx_curve_param_count: Self::load_symbol(&lib, b"sbx_curve_param_count\0")?,
        sbx_curve_get_param: Self::load_symbol(&lib, b"sbx_curve_get_param\0")?,
        sbx_curve_last_error: Self::load_symbol(&lib, b"sbx_curve_last_error\0")?,
        sbx_context_create: Self::load_symbol(&lib, b"sbx_context_create\0")?,
        sbx_context_destroy: Self::load_symbol(&lib, b"sbx_context_destroy\0")?,
        sbx_context_set_default_waveform:
          Self::load_symbol(&lib, b"sbx_context_set_default_waveform\0")?,
        sbx_context_set_sequence_iso_override:
          Self::load_symbol(&lib, b"sbx_context_set_sequence_iso_override\0")?,
        sbx_context_set_sequence_mixam_override:
          Self::load_symbol(&lib, b"sbx_context_set_sequence_mixam_override\0")?,
        sbx_context_set_amp_adjust:
          Self::load_symbol(&lib, b"sbx_context_set_amp_adjust\0")?,
        sbx_context_set_mix_mod: Self::load_symbol(&lib, b"sbx_context_set_mix_mod\0")?,
        sbx_prepare_safe_seq_text: Self::load_symbol(&lib, b"sbx_prepare_safe_seq_text\0")?,
        sbx_validate_sbg_text: Self::load_symbol(&lib, b"sbx_validate_sbg_text\0")?,
        sbx_validate_sbgf_text: Self::load_symbol(&lib, b"sbx_validate_sbgf_text\0")?,
        sbx_free_diagnostics: Self::load_symbol(&lib, b"sbx_free_diagnostics\0")?,
        sbx_context_load_sbg_timing_text:
          Self::load_symbol(&lib, b"sbx_context_load_sbg_timing_text\0")?,
        sbx_context_last_error: Self::load_symbol(&lib, b"sbx_context_last_error\0")?,
        sbx_context_duration_sec: Self::load_symbol(&lib, b"sbx_context_duration_sec\0")?,
        sbx_context_is_looping: Self::load_symbol(&lib, b"sbx_context_is_looping\0")?,
        sbx_context_has_mix_effects:
          Self::load_symbol(&lib, b"sbx_context_has_mix_effects\0")?,
        sbx_context_time_sec: Self::load_symbol(&lib, b"sbx_context_time_sec\0")?,
        sbx_context_render_f32: Self::load_symbol(&lib, b"sbx_context_render_f32\0")?,
        sbx_context_mix_stream_sample:
          Self::load_symbol(&lib, b"sbx_context_mix_stream_sample\0")?,
        sbx_context_voice_count: Self::load_symbol(&lib, b"sbx_context_voice_count\0")?,
        sbx_context_sample_program_beat_voice:
          Self::load_symbol(&lib, b"sbx_context_sample_program_beat_voice\0")?,
        sbx_context_sample_tones_voice:
          Self::load_symbol(&lib, b"sbx_context_sample_tones_voice\0")?,
        sbx_audio_writer_create_path:
          Self::load_symbol(&lib, b"sbx_audio_writer_create_path\0")?,
        sbx_audio_writer_close: Self::load_symbol(&lib, b"sbx_audio_writer_close\0")?,
        sbx_audio_writer_destroy: Self::load_symbol(&lib, b"sbx_audio_writer_destroy\0")?,
        sbx_audio_writer_last_error:
          Self::load_symbol(&lib, b"sbx_audio_writer_last_error\0")?,
        sbx_audio_writer_input_mode:
          Self::load_symbol(&lib, b"sbx_audio_writer_input_mode\0")?,
        sbx_audio_writer_write_bytes:
          Self::load_symbol(&lib, b"sbx_audio_writer_write_bytes\0")?,
        sbx_audio_writer_write_s16:
          Self::load_symbol(&lib, b"sbx_audio_writer_write_s16\0")?,
        sbx_audio_writer_write_f32:
          Self::load_symbol(&lib, b"sbx_audio_writer_write_f32\0")?,
        sbx_audio_writer_write_i32:
          Self::load_symbol(&lib, b"sbx_audio_writer_write_i32\0")?,
        sbx_convert_f32_to_s16_ex:
          Self::load_symbol(&lib, b"sbx_convert_f32_to_s16_ex\0")?,
        sbx_convert_f32_to_s24_32:
          Self::load_symbol(&lib, b"sbx_convert_f32_to_s24_32\0")?,
        sbx_mix_input_create_stdio:
          Self::load_symbol(&lib, b"sbx_mix_input_create_stdio\0")?,
        sbx_mix_input_read: Self::load_symbol(&lib, b"sbx_mix_input_read\0")?,
        sbx_mix_input_destroy: Self::load_symbol(&lib, b"sbx_mix_input_destroy\0")?,
        sbx_mix_input_last_error: Self::load_symbol(&lib, b"sbx_mix_input_last_error\0")?,
        sbx_mix_input_output_rate:
          Self::load_symbol(&lib, b"sbx_mix_input_output_rate\0")?,
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
  if let Ok(path) = std::env::var("SBAGENX_GUI_SBAGENXLIB") {
    let path = PathBuf::from(path);
    if path.exists() {
      return Ok(path);
    }
  }

  let repo_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
    .join("../..")
    .canonicalize()
    .map_err(|err| format!("failed to resolve repo root: {}", err))?;
  let dist = repo_root.join("dist");

  let mut candidates = if cfg!(target_os = "linux") {
    vec![dist.join("libsbagenx.so"), dist.join("libsbagenx.so.3")]
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

  if cfg!(target_os = "linux") {
    if let Ok(entries) = fs::read_dir(&dist) {
      let mut versioned: Vec<PathBuf> = entries
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .filter(|path| {
          path
            .file_name()
            .and_then(|name| name.to_str())
            .map(|name| name.starts_with("libsbagenx.so."))
            .unwrap_or(false)
        })
        .collect();
      versioned.sort();
      versioned.reverse();
      candidates.extend(versioned);
    }
  }

  candidates
    .into_iter()
    .find(|path| path.exists())
    .ok_or_else(|| "unable to locate a usable sbagenxlib runtime library in dist/".to_string())
}

fn cstr_to_string(ptr: *const c_char) -> String {
  if ptr.is_null() {
    return String::new();
  }
  unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() }
}

fn fixed_cstr_to_string(buf: &[c_char]) -> String {
  let len = buf.iter().position(|ch| *ch == 0).unwrap_or(buf.len());
  let bytes: Vec<u8> = buf[..len].iter().map(|ch| *ch as u8).collect();
  String::from_utf8_lossy(&bytes).into_owned()
}

fn collect_validation_diagnostics(
  api: &Api,
  raw_ptr: *mut SbxDiagnosticRaw,
  count: usize,
) -> Vec<ValidationDiagnostic> {
  if raw_ptr.is_null() || count == 0 {
    return Vec::new();
  }

  let slice = unsafe { std::slice::from_raw_parts(raw_ptr, count) };
  let diags = slice
    .iter()
    .map(|diag| ValidationDiagnostic {
      severity: if diag.severity == SBX_DIAG_ERROR {
        "error"
      } else {
        "warning"
      },
      line: (diag.line != 0).then_some(diag.line),
      column: (diag.column != 0).then_some(diag.column),
      end_line: (diag.end_line != 0).then_some(diag.end_line),
      end_column: (diag.end_column != 0).then_some(diag.end_column),
      message: fixed_cstr_to_string(&diag.message),
    })
    .collect();
  unsafe { (api.sbx_free_diagnostics)(raw_ptr) };
  diags
}

fn format_from_path(path: &Path) -> Result<(c_int, &'static str), String> {
  match path
    .extension()
    .and_then(|ext| ext.to_str())
    .map(|ext| ext.to_ascii_lowercase())
    .as_deref()
  {
    Some("wav") => Ok((SBX_AUDIO_FILE_WAV, "wav")),
    Some("ogg") => Ok((SBX_AUDIO_FILE_OGG, "ogg")),
    Some("flac") => Ok((SBX_AUDIO_FILE_FLAC, "flac")),
    Some("mp3") => Ok((SBX_AUDIO_FILE_MP3, "mp3")),
    _ => Err("supported export formats are .wav, .flac, .ogg, and .mp3".to_string()),
  }
}

fn split_mix_path_spec(spec: &str) -> (&str, i32) {
  let bytes = spec.as_bytes();
  if bytes.last().is_some_and(|byte| byte.is_ascii_digit()) {
    let mut start = bytes.len();
    while start > 0 && bytes[start - 1].is_ascii_digit() {
      start -= 1;
    }
    if start > 0 && bytes[start - 1] == b'#' {
      if let Ok(section) = spec[start..].parse::<i32>() {
        return (&spec[..start - 1], section);
      }
    }
  }
  (spec, -1)
}

fn resolve_mix_input_path(source_name: &str, mix_spec: &str) -> Result<(PathBuf, i32), String> {
  let (mix_path, mix_section) = split_mix_path_spec(mix_spec.trim());
  if mix_path.is_empty() {
    return Err("sequence mix input path (-m) is empty".to_string());
  }

  let mix_path_buf = PathBuf::from(mix_path);
  if mix_path_buf.is_absolute() {
    return Ok((mix_path_buf, mix_section));
  }

  let source_path = PathBuf::from(source_name);
  let source_dir = source_path.parent().filter(|dir| !dir.as_os_str().is_empty());
  let Some(source_dir) = source_dir else {
    return Err(format!(
      "sequence mix input path '{}' is relative, but the document has not been saved to disk",
      mix_path
    ));
  };
  Ok((source_dir.join(mix_path_buf), mix_section))
}

#[cfg(target_os = "windows")]
fn open_file_read_binary(path: &Path) -> Result<*mut FILE, String> {
  let mut wide_path: Vec<u16> = path.as_os_str().encode_wide().collect();
  wide_path.push(0);
  let mut wide_mode: Vec<u16> = "rb".encode_utf16().collect();
  wide_mode.push(0);
  let file = unsafe { _wfopen(wide_path.as_ptr(), wide_mode.as_ptr()) };
  if file.is_null() {
    return Err(format!("failed to open mix input file {} for reading", path.display()));
  }
  Ok(file)
}

#[cfg(not(target_os = "windows"))]
fn open_file_read_binary(path: &Path) -> Result<*mut FILE, String> {
  let c_path = CString::new(path.as_os_str().as_bytes())
    .map_err(|_| format!("mix input path contains embedded NUL byte: {}", path.display()))?;
  let file = unsafe { libc::fopen(c_path.as_ptr(), b"rb\0".as_ptr() as *const c_char) };
  if file.is_null() {
    return Err(format!("failed to open mix input file {} for reading", path.display()));
  }
  Ok(file)
}

fn open_mix_input_resource(
  api: &Api,
  preamble: &SbxSafeSeqfilePreamble,
  source_name: &str,
  output_rate_hz: i32,
  output_rate_is_default: bool,
) -> Result<(Option<MixInputResource>, f64), String> {
  if preamble.mix_path.is_null() {
    return Ok((None, output_rate_hz as f64));
  }

  let mix_spec = cstr_to_string(preamble.mix_path);
  let (resolved_path, mix_section) = resolve_mix_input_path(source_name, &mix_spec)?;
  let file = open_file_read_binary(&resolved_path)?;
  let mut cfg = SbxMixInputConfig {
    mix_section: -1,
    output_rate_hz: 0,
    output_rate_is_default: 0,
    take_stream_ownership: 0,
    warn_cb: None,
    warn_user: std::ptr::null_mut(),
  };
  unsafe { (api.sbx_default_mix_input_config)(&mut cfg) };
  cfg.mix_section = mix_section;
  cfg.output_rate_hz = output_rate_hz;
  cfg.output_rate_is_default = if output_rate_is_default { 1 } else { 0 };
  cfg.take_stream_ownership = 1;

  let path_hint = CString::new(resolved_path.to_string_lossy().as_bytes())
    .map_err(|_| format!("mix input path contains embedded NUL byte: {}", resolved_path.display()))?;
  let handle = unsafe { (api.sbx_mix_input_create_stdio)(file, path_hint.as_ptr(), &cfg) };
  if handle.is_null() {
    unsafe {
      libc::fclose(file);
    }
    return Err("out of memory creating mix input decoder".to_string());
  }

  let error = unsafe { cstr_to_string((api.sbx_mix_input_last_error)(handle)) };
  if !error.is_empty() {
    unsafe {
      (api.sbx_mix_input_destroy)(handle);
    }
    return Err(error);
  }

  let effective_rate_hz = unsafe { (api.sbx_mix_input_output_rate)(handle) } as f64;
  Ok((Some(MixInputResource { handle }), effective_rate_hz))
}

fn preview_path() -> Result<PathBuf, String> {
  let dir = std::env::temp_dir().join("sbagenx-gui");
  fs::create_dir_all(&dir)
    .map_err(|err| format!("failed to create preview directory {}: {}", dir.display(), err))?;
  let stamp = SystemTime::now()
    .duration_since(UNIX_EPOCH)
    .map_err(|err| format!("system clock error: {}", err))?
    .as_millis();
  Ok(dir.join(format!("preview-{}.wav", stamp)))
}

fn apply_sequence_overrides(loaded: &mut LoadedSbgContext) -> Result<(), String> {
  unsafe {
    if loaded.preamble.have_w != 0
      && (loaded.api.sbx_context_set_default_waveform)(loaded.ctx, loaded.preamble.waveform) != SBX_OK
    {
      return Err("failed to set sbagenxlib default waveform".to_string());
    }
    if loaded.preamble.have_I != 0
      && (loaded.api.sbx_context_set_sequence_iso_override)(loaded.ctx, &loaded.preamble.iso_env) != SBX_OK
    {
      return Err("failed to set sbagenxlib sequence isochronic override".to_string());
    }
    if loaded.preamble.have_H != 0
      && (loaded.api.sbx_context_set_sequence_mixam_override)(loaded.ctx, &loaded.preamble.mixam_env) != SBX_OK
    {
      return Err("failed to set sbagenxlib sequence mixam override".to_string());
    }
    if loaded.preamble.have_c != 0
      && (loaded.api.sbx_context_set_amp_adjust)(loaded.ctx, &loaded.preamble.amp_adjust) != SBX_OK
    {
      return Err("failed to set sbagenxlib amplitude-adjust curve".to_string());
    }
  }
  Ok(())
}

fn load_sbg_context_with_rate(
  text: &str,
  source_name: &str,
  sample_rate_override: Option<f64>,
  enable_mix_input: bool,
) -> Result<LoadedSbgContext, String> {
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
    return Err(if msg.is_empty() {
      format!("SBaGenX validation failed for {}", source_name)
    } else {
      msg
    });
  }

  let mut engine_cfg = SbxEngineConfig {
    sample_rate: 0.0,
    channels: 0,
  };
  unsafe { (api.sbx_default_engine_config)(&mut engine_cfg) };

  let requested_rate_hz = if let Some(rate) = sample_rate_override {
    rate.round() as i32
  } else if preamble.have_r != 0 {
    preamble.rate
  } else {
    44100
  };
  let output_rate_is_default = sample_rate_override.is_none() && preamble.have_r == 0;
  let (mix_input, effective_rate_hz) = if enable_mix_input {
    open_mix_input_resource(
      &api,
      &preamble,
      source_name,
      requested_rate_hz,
      output_rate_is_default,
    )?
  } else {
    (None, requested_rate_hz as f64)
  };
  engine_cfg.sample_rate = effective_rate_hz;

  let ctx = unsafe { (api.sbx_context_create)(&engine_cfg) };
  if ctx.is_null() {
    unsafe {
      (api.sbx_free_safe_seqfile_preamble)(&mut preamble);
      free(prepared_text as *mut c_void);
    }
    return Err("failed to create sbagenxlib context".to_string());
  }

  let mut loaded = LoadedSbgContext {
    api,
    ctx,
    prepared_text,
    preamble,
    engine_cfg,
    mix_input,
  };

  apply_sequence_overrides(&mut loaded)?;

  let load_rc =
    unsafe { (loaded.api.sbx_context_load_sbg_timing_text)(loaded.ctx, loaded.prepared_text, 0) };
  if load_rc != SBX_OK {
    let msg = unsafe { cstr_to_string((loaded.api.sbx_context_last_error)(loaded.ctx)) };
    return Err(if msg.is_empty() {
      format!("SBaGenX validation failed for {}", source_name)
    } else {
      msg
    });
  }

  if enable_mix_input {
    if loaded.preamble.have_A != 0 && loaded.mix_input.is_none() {
      return Err("-A requires a mix input stream; use -m in the sequence preamble".to_string());
    }
    if loaded.mix_input.is_none()
      && unsafe { (loaded.api.sbx_context_has_mix_effects)(loaded.ctx) } != 0
    {
      return Err("sequence file mix effects require a mix input stream (-m)".to_string());
    }
  }

  if loaded.preamble.have_A != 0 {
    let mut mix_mod = loaded.preamble.mix_mod;
    let duration_sec = unsafe { (loaded.api.sbx_context_duration_sec)(loaded.ctx) };
    mix_mod.active = 1;
    if mix_mod.main_len_sec <= 0.0 {
      mix_mod.main_len_sec = if duration_sec > 0.0 { duration_sec } else { 1.0 };
    }
    if mix_mod.wake_len_sec < 0.0 {
      mix_mod.wake_len_sec = 0.0;
    }
    let rc = unsafe { (loaded.api.sbx_context_set_mix_mod)(loaded.ctx, &mix_mod) };
    if rc != SBX_OK {
      return Err("failed to set sbagenxlib sequence mix modulation".to_string());
    }
  }

  Ok(loaded)
}

fn load_sbg_context(text: &str, source_name: &str) -> Result<LoadedSbgContext, String> {
  load_sbg_context_with_rate(text, source_name, None, false)
}

fn load_sbg_context_for_audio(
  text: &str,
  source_name: &str,
  sample_rate_override: Option<f64>,
) -> Result<LoadedSbgContext, String> {
  load_sbg_context_with_rate(text, source_name, sample_rate_override, true)
}

fn writer_config_for_path(
  loaded: &LoadedSbgContext,
  out_path: &Path,
) -> Result<(SbxAudioWriterConfig, &'static str), String> {
  let (format, format_name) = format_from_path(out_path)?;
  let mut cfg = SbxAudioWriterConfig {
    sample_rate: 0.0,
    channels: 0,
    format: 0,
    pcm_bits: 0,
    ogg_quality: 0.0,
    ogg_quality_set: 0,
    flac_compression: 0.0,
    flac_compression_set: 0,
    mp3_bitrate: 0,
    mp3_bitrate_set: 0,
    mp3_quality: 0,
    mp3_quality_set: 0,
    mp3_vbr_quality: 0.0,
    mp3_vbr_quality_set: 0,
    prefer_float_input: 0,
  };
  unsafe { (loaded.api.sbx_default_audio_writer_config)(&mut cfg) };
  cfg.sample_rate = loaded.engine_cfg.sample_rate;
  cfg.channels = loaded.engine_cfg.channels;
  cfg.format = format;
  cfg.prefer_float_input = 1;

  if format == SBX_AUDIO_FILE_WAV {
    cfg.pcm_bits = if loaded.preamble.have_b != 0 && loaded.preamble.pcm_bits == 24 {
      24
    } else {
      16
    };
    if loaded.preamble.have_b != 0 && loaded.preamble.pcm_bits == 8 {
      return Err("GUI export does not yet support 8-bit WAV output".to_string());
    }
  }

  if format == SBX_AUDIO_FILE_FLAC {
    if loaded.preamble.have_b != 0 {
      cfg.pcm_bits = loaded.preamble.pcm_bits;
    }
    if loaded.preamble.have_Z != 0 {
      cfg.flac_compression = loaded.preamble.flac_compression;
      cfg.flac_compression_set = 1;
    }
  }

  if format == SBX_AUDIO_FILE_OGG && loaded.preamble.have_U != 0 {
    cfg.ogg_quality = loaded.preamble.ogg_quality;
    cfg.ogg_quality_set = 1;
  }

  if format == SBX_AUDIO_FILE_MP3 {
    if loaded.preamble.have_K != 0 {
      cfg.mp3_bitrate = loaded.preamble.mp3_bitrate;
      cfg.mp3_bitrate_set = 1;
    }
    if loaded.preamble.have_J != 0 {
      cfg.mp3_quality = loaded.preamble.mp3_quality;
      cfg.mp3_quality_set = 1;
    }
    if loaded.preamble.have_X != 0 {
      cfg.mp3_vbr_quality = loaded.preamble.mp3_vbr_quality;
      cfg.mp3_vbr_quality_set = 1;
    }
  }

  Ok((cfg, format_name))
}

fn pack_s24le_from_i32(src: &[i32], dst: &mut Vec<u8>, sample_count: usize) {
  dst.clear();
  dst.reserve(sample_count * 3);
  for sample in src.iter().take(sample_count) {
    let value = *sample as u32;
    dst.push((value & 0xff) as u8);
    dst.push(((value >> 8) & 0xff) as u8);
    dst.push(((value >> 16) & 0xff) as u8);
  }
}

fn render_context_to_path(
  mut loaded: LoadedSbgContext,
  out_path: &Path,
  preview: bool,
) -> Result<(f64, bool, &'static str, String), String> {
  let duration_sec = unsafe { (loaded.api.sbx_context_duration_sec)(loaded.ctx) };
  let is_looping = unsafe { (loaded.api.sbx_context_is_looping)(loaded.ctx) } != 0;
  let mut render_sec = duration_sec;
  let mut limited = false;

  if preview {
    if is_looping || render_sec <= 0.0 || render_sec > PREVIEW_LIMIT_SEC {
      render_sec = PREVIEW_LIMIT_SEC;
      limited = true;
    }
  } else if is_looping || render_sec <= 0.0 {
    return Err(
      "GUI export currently requires a finite non-looping sequence duration"
        .to_string(),
    );
  }

  let (writer_cfg, format_name) = writer_config_for_path(&loaded, out_path)?;
  let out_path_str = out_path.to_string_lossy().to_string();
  let c_out_path = CString::new(out_path_str.clone())
    .map_err(|_| "output path contains embedded NUL byte".to_string())?;
  let writer = unsafe { (loaded.api.sbx_audio_writer_create_path)(c_out_path.as_ptr(), &writer_cfg) };
  if writer.is_null() {
    return Err(format!("failed to create audio writer for {}", out_path.display()));
  }

  let mut pcm_state = SbxPcmConvertState {
    rng_state: 0,
    dither_mode: 0,
  };
  unsafe { (loaded.api.sbx_default_pcm_convert_state)(&mut pcm_state) };

  let channels = loaded.engine_cfg.channels as usize;
  let mut fbuf = vec![0.0_f32; RENDER_CHUNK_FRAMES * channels];
  let mut s16buf = vec![0_i16; RENDER_CHUNK_FRAMES * channels];
  let mut i32buf = vec![0_i32; RENDER_CHUNK_FRAMES * channels];
  let mut mix_buf = Vec::<i32>::new();
  let mut bytebuf = Vec::<u8>::with_capacity(RENDER_CHUNK_FRAMES * channels * 3);
  let mut remaining_frames = (render_sec * loaded.engine_cfg.sample_rate).ceil() as usize;
  let volume_mul = if loaded.preamble.have_V != 0 {
    loaded.preamble.volume_pct as f32 / 100.0
  } else {
    1.0
  };

  let input_mode = unsafe { (loaded.api.sbx_audio_writer_input_mode)(writer) };
  let mut writer_error: Option<String> = None;

  while remaining_frames > 0 {
    let frames = remaining_frames.min(RENDER_CHUNK_FRAMES);
    let sample_count = frames * channels;
    let rendered_frames = match render_frames_with_mix(
      &mut loaded,
      &mut mix_buf,
      &mut fbuf[..sample_count],
      frames,
      volume_mul,
    ) {
      Ok(rendered_frames) => rendered_frames,
      Err(err) => {
        writer_error = Some(err);
        break;
      }
    };
    if rendered_frames == 0 {
      break;
    }

    let write_rc = match input_mode {
      SBX_AUDIO_WRITER_INPUT_F32 => unsafe {
        (loaded.api.sbx_audio_writer_write_f32)(writer, fbuf.as_ptr(), rendered_frames)
      },
      SBX_AUDIO_WRITER_INPUT_S16 => {
        let rc = unsafe {
          (loaded.api.sbx_convert_f32_to_s16_ex)(
            fbuf.as_ptr(),
            s16buf.as_mut_ptr(),
            rendered_frames * channels,
            &mut pcm_state,
          )
        };
        if rc != SBX_OK {
          writer_error = Some("sbagenxlib PCM16 conversion failed".to_string());
          break;
        }
        unsafe {
          (loaded.api.sbx_audio_writer_write_s16)(writer, s16buf.as_ptr(), rendered_frames)
        }
      }
      SBX_AUDIO_WRITER_INPUT_I32 => {
        let rc = unsafe {
          (loaded.api.sbx_convert_f32_to_s24_32)(
            fbuf.as_ptr(),
            i32buf.as_mut_ptr(),
            rendered_frames * channels,
            &mut pcm_state,
          )
        };
        if rc != SBX_OK {
          writer_error = Some("sbagenxlib PCM24 conversion failed".to_string());
          break;
        }
        unsafe {
          (loaded.api.sbx_audio_writer_write_i32)(writer, i32buf.as_ptr(), rendered_frames)
        }
      }
      SBX_AUDIO_WRITER_INPUT_BYTES => {
        match writer_cfg.pcm_bits {
          16 => {
            let rc = unsafe {
              (loaded.api.sbx_convert_f32_to_s16_ex)(
                fbuf.as_ptr(),
                s16buf.as_mut_ptr(),
                rendered_frames * channels,
                &mut pcm_state,
              )
            };
            if rc != SBX_OK {
              writer_error = Some("sbagenxlib PCM16 conversion failed".to_string());
              break;
            }
            unsafe {
              (loaded.api.sbx_audio_writer_write_bytes)(
                writer,
                s16buf.as_ptr() as *const c_void,
                rendered_frames * channels * std::mem::size_of::<i16>(),
              )
            }
          }
          24 => {
            let rc = unsafe {
              (loaded.api.sbx_convert_f32_to_s24_32)(
                fbuf.as_ptr(),
                i32buf.as_mut_ptr(),
                rendered_frames * channels,
                &mut pcm_state,
              )
            };
            if rc != SBX_OK {
              writer_error = Some("sbagenxlib PCM24 conversion failed".to_string());
              break;
            }
            pack_s24le_from_i32(&i32buf, &mut bytebuf, rendered_frames * channels);
            unsafe {
              (loaded.api.sbx_audio_writer_write_bytes)(
                writer,
                bytebuf.as_ptr() as *const c_void,
                bytebuf.len(),
              )
            }
          }
          other => {
            writer_error = Some(format!("unsupported byte-output PCM depth: {}", other));
            break;
          }
        }
      }
      other => {
        writer_error = Some(format!("unsupported sbagenxlib writer input mode: {}", other));
        break;
      }
    };

    if write_rc != SBX_OK {
      writer_error = Some(unsafe { cstr_to_string((loaded.api.sbx_audio_writer_last_error)(writer)) });
      break;
    }

    remaining_frames -= rendered_frames;
  }

  let close_rc = unsafe { (loaded.api.sbx_audio_writer_close)(writer) };
  if writer_error.is_none() && close_rc != SBX_OK {
    writer_error = Some(unsafe { cstr_to_string((loaded.api.sbx_audio_writer_last_error)(writer)) });
  }
  unsafe { (loaded.api.sbx_audio_writer_destroy)(writer) };

  if let Some(err) = writer_error {
    return Err(if err.is_empty() {
      format!("failed to render audio to {}", out_path.display())
    } else {
      err
    });
  }

  Ok((render_sec, limited, format_name, loaded.api.version_string()))
}

pub fn backend_status() -> Result<(String, i32), String> {
  let api = Api::load()?;
  Ok((api.version_string(), api.api_version()))
}

pub fn validate_sbg(text: &str, source_name: &str) -> Result<ValidationOutcome, String> {
  let api = Api::load()?;
  let c_text = CString::new(text).map_err(|_| "document contains embedded NUL byte".to_string())?;
  let c_source =
    CString::new(source_name).map_err(|_| "source name contains embedded NUL byte".to_string())?;
  let mut raw_ptr: *mut SbxDiagnosticRaw = std::ptr::null_mut();
  let mut count: usize = 0;
  let rc = unsafe {
    (api.sbx_validate_sbg_text)(c_text.as_ptr(), c_source.as_ptr(), &mut raw_ptr, &mut count)
  };
  if rc != SBX_OK {
    return Err("sbagenxlib structured validation failed".to_string());
  }
  let diagnostics = collect_validation_diagnostics(&api, raw_ptr, count);
  Ok(ValidationOutcome {
    valid: diagnostics.is_empty(),
    diagnostics,
    engine_version: format!("{} (api {})", api.version_string(), api.api_version()),
  })
}

pub fn validate_sbgf(text: &str, source_name: &str) -> Result<ValidationOutcome, String> {
  let api = Api::load()?;
  let c_text = CString::new(text).map_err(|_| "document contains embedded NUL byte".to_string())?;
  let c_source =
    CString::new(source_name).map_err(|_| "source name contains embedded NUL byte".to_string())?;
  let mut raw_ptr: *mut SbxDiagnosticRaw = std::ptr::null_mut();
  let mut count: usize = 0;
  let rc = unsafe {
    (api.sbx_validate_sbgf_text)(c_text.as_ptr(), c_source.as_ptr(), &mut raw_ptr, &mut count)
  };
  if rc != SBX_OK {
    return Err("sbagenxlib structured curve validation failed".to_string());
  }
  let diagnostics = collect_validation_diagnostics(&api, raw_ptr, count);
  Ok(ValidationOutcome {
    valid: diagnostics.is_empty(),
    diagnostics,
    engine_version: format!("{} (api {})", api.version_string(), api.api_version()),
  })
}

pub fn create_live_preview(
  text: &str,
  source_name: &str,
  sample_rate_hz: u32,
) -> Result<LivePlaybackContext, String> {
  if sample_rate_hz == 0 {
    return Err("live playback requires a valid output sample rate".to_string());
  }

  let loaded = load_sbg_context_for_audio(text, source_name, Some(sample_rate_hz as f64))?;
  let duration_sec = unsafe { (loaded.api.sbx_context_duration_sec)(loaded.ctx) };
  let is_looping = unsafe { (loaded.api.sbx_context_is_looping)(loaded.ctx) } != 0;
  let mut render_sec = duration_sec;
  let mut limited = false;

  if is_looping || render_sec <= 0.0 || render_sec > PREVIEW_LIMIT_SEC {
    render_sec = PREVIEW_LIMIT_SEC;
    limited = true;
  }

  if !render_sec.is_finite() || render_sec <= 0.0 {
    return Err("live preview requires a finite preview span".to_string());
  }

  let volume_mul = if loaded.preamble.have_V != 0 {
    loaded.preamble.volume_pct as f32 / 100.0
  } else {
    1.0
  };

  let remaining_frames = (render_sec * loaded.engine_cfg.sample_rate).ceil().max(1.0) as usize;

  Ok(LivePlaybackContext {
    loaded,
    volume_mul,
    remaining_frames,
    finished: false,
    duration_sec: render_sec,
    limited,
    mix_buf: Vec::new(),
  })
}

pub fn render_preview(text: &str, source_name: &str) -> Result<PreviewOutcome, String> {
  let loaded = load_sbg_context_for_audio(text, source_name, None)?;
  let out_path = preview_path()?;
  let (duration_sec, limited, _, engine_version) = render_context_to_path(loaded, &out_path, true)?;
  Ok(PreviewOutcome {
    audio_path: out_path.to_string_lossy().to_string(),
    duration_sec,
    limited,
    engine_version,
  })
}

pub fn export_sbg(text: &str, source_name: &str, output_path: &Path) -> Result<ExportOutcome, String> {
  let loaded = load_sbg_context_for_audio(text, source_name, None)?;
  let (duration_sec, _, format, engine_version) =
    render_context_to_path(loaded, output_path, false)?;
  Ok(ExportOutcome {
    output_path: output_path.to_string_lossy().to_string(),
    duration_sec,
    format: format.to_string(),
    engine_version,
  })
}

pub fn sample_beat_preview(text: &str, source_name: &str) -> Result<BeatPreviewOutcome, String> {
  let loaded = load_sbg_context(text, source_name)?;
  let duration_sec = unsafe { (loaded.api.sbx_context_duration_sec)(loaded.ctx) };
  let is_looping = unsafe { (loaded.api.sbx_context_is_looping)(loaded.ctx) } != 0;
  let voice_count = unsafe { (loaded.api.sbx_context_voice_count)(loaded.ctx) };
  let mut sample_span_sec = duration_sec;
  let mut limited = false;

  if is_looping || sample_span_sec <= 0.0 {
    sample_span_sec = PREVIEW_LIMIT_SEC;
    limited = true;
  }

  if !sample_span_sec.is_finite() || sample_span_sec <= 0.0 {
    return Err("beat preview currently requires a finite timeline".to_string());
  }

  let sample_count = 240usize;
  let mut min_hz = f64::INFINITY;
  let mut max_hz = f64::NEG_INFINITY;
  let mut series = Vec::with_capacity(voice_count.max(1));
  for voice_index in 0..voice_count.max(1) {
    let mut t_sec = vec![0.0_f64; sample_count];
    let mut hz = vec![0.0_f64; sample_count];
    let mut tones = vec![
      SbxToneSpec {
        mode: 0,
        carrier_hz: 0.0,
        beat_hz: 0.0,
        amplitude: 0.0,
        waveform: 0,
        envelope_waveform: 0,
        duty_cycle: 0.0,
        iso_start: 0.0,
        iso_attack: 0.0,
        iso_release: 0.0,
        iso_edge_mode: 0,
      };
      sample_count
    ];
    let beat_rc = unsafe {
      (loaded.api.sbx_context_sample_program_beat_voice)(
        loaded.ctx,
        voice_index,
        0.0,
        sample_span_sec,
        sample_count,
        t_sec.as_mut_ptr(),
        hz.as_mut_ptr(),
      )
    };
    if beat_rc != SBX_OK {
      let msg = unsafe { cstr_to_string((loaded.api.sbx_context_last_error)(loaded.ctx)) };
      return Err(if msg.is_empty() {
        "failed to sample beat preview from sbagenxlib".to_string()
      } else {
        msg
      });
    }

    let tone_rc = unsafe {
      (loaded.api.sbx_context_sample_tones_voice)(
        loaded.ctx,
        voice_index,
        0.0,
        sample_span_sec,
        sample_count,
        std::ptr::null_mut(),
        tones.as_mut_ptr(),
      )
    };
    if tone_rc != SBX_OK {
      let msg = unsafe { cstr_to_string((loaded.api.sbx_context_last_error)(loaded.ctx)) };
      return Err(if msg.is_empty() {
        "failed to sample tone activity from sbagenxlib".to_string()
      } else {
        msg
      });
    }

    let mut points = Vec::with_capacity(sample_count);
    let mut active_sample_count = 0usize;
    for index in 0..sample_count {
      let tone = tones[index];
      let beat_hz = if tone.mode == 0 {
        None
      } else {
        let beat_hz = hz[index];
        if beat_hz.is_finite() {
          active_sample_count += 1;
          if beat_hz < min_hz {
            min_hz = beat_hz;
          }
          if beat_hz > max_hz {
            max_hz = beat_hz;
          }
          Some(beat_hz)
        } else {
          None
        }
      };
      points.push(BeatPreviewPoint {
        t_sec: t_sec[index],
        beat_hz,
      });
    }

    series.push(BeatPreviewSeries {
      voice_index,
      label: format!("Voice {}", voice_index + 1),
      active_sample_count,
      points,
    });
  }

  Ok(BeatPreviewOutcome {
    duration_sec: sample_span_sec,
    sample_count,
    voice_count: voice_count.max(1),
    min_hz: if min_hz.is_finite() { Some(min_hz) } else { None },
    max_hz: if max_hz.is_finite() { Some(max_hz) } else { None },
    limited,
    time_label: if sample_span_sec >= 180.0 {
      "TIME MIN".to_string()
    } else {
      "TIME SEC".to_string()
    },
    series,
    engine_version: loaded.api.version_string(),
  })
}

pub fn inspect_curve_info(text: &str, source_name: &str) -> Result<CurveInfoOutcome, String> {
  let api = Api::load()?;
  let curve = unsafe { (api.sbx_curve_create)() };
  if curve.is_null() {
    return Err("failed to create sbagenxlib curve program".to_string());
  }

  let c_text = CString::new(text).map_err(|_| "document contains embedded NUL byte".to_string())?;
  let c_source =
    CString::new(source_name).map_err(|_| "source name contains embedded NUL byte".to_string())?;

  let load_rc = unsafe { (api.sbx_curve_load_text)(curve, c_text.as_ptr(), c_source.as_ptr()) };
  if load_rc != SBX_OK {
    let msg = unsafe { cstr_to_string((api.sbx_curve_last_error)(curve)) };
    unsafe { (api.sbx_curve_destroy)(curve) };
    return Err(if msg.is_empty() {
      "failed to load curve text in sbagenxlib".to_string()
    } else {
      msg
    });
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
  let prepare_rc = unsafe { (api.sbx_curve_prepare)(curve, &eval_cfg) };
  if prepare_rc != SBX_OK {
    let msg = unsafe { cstr_to_string((api.sbx_curve_last_error)(curve)) };
    unsafe { (api.sbx_curve_destroy)(curve) };
    return Err(if msg.is_empty() {
      "failed to prepare curve metadata in sbagenxlib".to_string()
    } else {
      msg
    });
  }

  let mut info = SbxCurveInfoRaw {
    parameter_count: 0,
    has_solve: 0,
    has_carrier_expr: 0,
    has_amp_expr: 0,
    has_mixamp_expr: 0,
    beat_piece_count: 0,
    carrier_piece_count: 0,
    amp_piece_count: 0,
    mixamp_piece_count: 0,
  };

  let info_rc = unsafe { (api.sbx_curve_get_info)(curve, &mut info) };
  if info_rc != SBX_OK {
    let msg = unsafe { cstr_to_string((api.sbx_curve_last_error)(curve)) };
    unsafe { (api.sbx_curve_destroy)(curve) };
    return Err(if msg.is_empty() {
      "failed to inspect curve metadata in sbagenxlib".to_string()
    } else {
      msg
    });
  }

  let mut parameters = Vec::new();
  let param_count = unsafe { (api.sbx_curve_param_count)(curve) };
  for index in 0..param_count {
    let mut name_ptr: *const c_char = std::ptr::null();
    let mut value = 0.0_f64;
    let rc = unsafe { (api.sbx_curve_get_param)(curve, index, &mut name_ptr, &mut value) };
    if rc != SBX_OK {
      continue;
    }
    parameters.push(CurveParameter {
      name: cstr_to_string(name_ptr),
      value,
    });
  }

  unsafe { (api.sbx_curve_destroy)(curve) };

  Ok(CurveInfoOutcome {
    parameter_count: info.parameter_count,
    has_solve: info.has_solve != 0,
    has_carrier_expr: info.has_carrier_expr != 0,
    has_amp_expr: info.has_amp_expr != 0,
    has_mixamp_expr: info.has_mixamp_expr != 0,
    beat_piece_count: info.beat_piece_count,
    carrier_piece_count: info.carrier_piece_count,
    amp_piece_count: info.amp_piece_count,
    mixamp_piece_count: info.mixamp_piece_count,
    parameters,
    engine_version: api.version_string(),
  })
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

#[cfg(test)]
mod tests {
  use super::*;
  use std::fs;
  use std::path::Path;
  use std::time::{SystemTime, UNIX_EPOCH};

  fn write_test_wav(path: &Path, sample_rate_hz: u32, frames: usize) {
    let channels = 2u16;
    let bits_per_sample = 16u16;
    let block_align = channels * (bits_per_sample / 8);
    let byte_rate = sample_rate_hz * block_align as u32;
    let data_size = (frames * block_align as usize) as u32;
    let mut bytes = Vec::with_capacity(44 + data_size as usize);
    bytes.extend_from_slice(b"RIFF");
    bytes.extend_from_slice(&(36 + data_size).to_le_bytes());
    bytes.extend_from_slice(b"WAVE");
    bytes.extend_from_slice(b"fmt ");
    bytes.extend_from_slice(&16u32.to_le_bytes());
    bytes.extend_from_slice(&1u16.to_le_bytes());
    bytes.extend_from_slice(&channels.to_le_bytes());
    bytes.extend_from_slice(&sample_rate_hz.to_le_bytes());
    bytes.extend_from_slice(&byte_rate.to_le_bytes());
    bytes.extend_from_slice(&block_align.to_le_bytes());
    bytes.extend_from_slice(&bits_per_sample.to_le_bytes());
    bytes.extend_from_slice(b"data");
    bytes.extend_from_slice(&data_size.to_le_bytes());

    for frame_idx in 0..frames {
      let t = frame_idx as f32 / sample_rate_hz as f32;
      let sample = (t * 2.0 * std::f32::consts::PI * 220.0).sin() * 0.25;
      let pcm = (sample * i16::MAX as f32) as i16;
      bytes.extend_from_slice(&pcm.to_le_bytes());
      bytes.extend_from_slice(&pcm.to_le_bytes());
    }

    fs::write(path, bytes).expect("write test wav");
  }

  fn temp_gui_test_dir(name: &str) -> PathBuf {
    let stamp = SystemTime::now()
      .duration_since(UNIX_EPOCH)
      .expect("clock")
      .as_nanos();
    let dir = std::env::temp_dir().join(format!("sbagenx-gui-{}-{}", name, stamp));
    fs::create_dir_all(&dir).expect("create temp dir");
    dir
  }

  #[test]
  fn split_mix_path_spec_parses_section_suffix() {
    let (path, section) = split_mix_path_spec("river1.ogg#12");
    assert_eq!(path, "river1.ogg");
    assert_eq!(section, 12);

    let (plain_path, plain_section) = split_mix_path_spec("mix.wav");
    assert_eq!(plain_path, "mix.wav");
    assert_eq!(plain_section, -1);
  }

  #[test]
  fn resolve_mix_input_path_uses_source_directory() {
    let source = "/tmp/project/example.sbg";
    let (path, section) = resolve_mix_input_path(source, "mix.wav#3").expect("resolve mix path");
    assert_eq!(path, PathBuf::from("/tmp/project/mix.wav"));
    assert_eq!(section, 3);
  }

  #[test]
  fn export_sbg_supports_relative_mix_input() {
    let dir = temp_gui_test_dir("mix-export");
    let mix_path = dir.join("mix.wav");
    let sbg_path = dir.join("mix-demo.sbg");
    let out_path = dir.join("out.wav");

    write_test_wav(&mix_path, 44_100, 44_100);
    let seq = "-SE -m mix.wav\nbase: 200+6/20 mix/100\n00:00 base\n00:01 -\n";
    fs::write(&sbg_path, seq).expect("write sbg");

    let result = export_sbg(seq, &sbg_path.to_string_lossy(), &out_path).expect("export sbg");
    assert_eq!(result.format, "wav");
    let meta = fs::metadata(&out_path).expect("stat exported wav");
    assert!(meta.len() > 44, "expected exported wav payload");

    let _ = fs::remove_dir_all(dir);
  }

  #[test]
  fn inspect_curve_info_solves_equations() {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
      .join("../..")
      .join("examples/basics/curve-expfit-solve-demo.sbgf");
    let text = fs::read_to_string(&path).expect("read solve example");
    let outcome = inspect_curve_info(&text, &path.to_string_lossy()).expect("inspect solve curve");

    let a = outcome
      .parameters
      .iter()
      .find(|param| param.name == "A")
      .map(|param| param.value)
      .expect("solved A");
    let c = outcome
      .parameters
      .iter()
      .find(|param| param.name == "C")
      .map(|param| param.value)
      .expect("solved C");

    assert!(outcome.has_solve);
    assert!((a - 7.58425).abs() < 1e-4, "expected solved A, got {}", a);
    assert!((c - 2.41575).abs() < 1e-4, "expected solved C, got {}", c);
  }
}
