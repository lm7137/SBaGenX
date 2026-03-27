#ifndef SBAGENXLIB_H
#define SBAGENXLIB_H

/*
 * sbagenxlib public C API.
 *
 * The library renders stereo float audio from tone specs or loaded keyframed
 * programs. Device I/O remains a host responsibility. The library also offers
 * optional file/container writers for raw/WAV/OGG/FLAC/MP3 export.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SBX_API_VERSION 38  /* public API contract revision */
#define SBX_MAX_AUX_TONES 16 /* max auxiliary overlay tones */
#define SBX_MAX_AMP_ADJUST_POINTS 16 /* max -c frequency/gain breakpoints */
#define SBX_PLOT_MAX_TICKS 64
#define SBX_PLOT_TEXT_MAX 256
#define SBX_DIAG_CODE_MAX 32
#define SBX_DIAG_MESSAGE_MAX 256

/* Status codes returned by sbagenxlib APIs. */
enum {
  SBX_OK = 0,
  SBX_EINVAL = 1,
  SBX_ENOMEM = 2,
  SBX_ENOTREADY = 3
};

typedef enum {
  SBX_TONE_NONE = 0,
  SBX_TONE_BINAURAL = 1,
  SBX_TONE_MONAURAL = 2,
  SBX_TONE_ISOCHRONIC = 3,
  SBX_TONE_WHITE_NOISE = 4,
  SBX_TONE_PINK_NOISE = 5,
  SBX_TONE_BROWN_NOISE = 6,
  SBX_TONE_SPIN_PINK = 7,
  SBX_TONE_SPIN_BROWN = 8,
  SBX_TONE_SPIN_WHITE = 9,
  SBX_TONE_BELL = 10
} SbxToneMode;

typedef enum {
  SBX_WAVE_SINE = 0,
  SBX_WAVE_SQUARE = 1,
  SBX_WAVE_TRIANGLE = 2,
  SBX_WAVE_SAWTOOTH = 3,
  SBX_WAVE_CUSTOM_BASE = 1000 /* deprecated legacy placeholder; use SBX_ENV_WAVE_* for custom envelopes */
} SbxWaveform;

typedef enum {
  SBX_ENV_WAVE_NONE = 0,
  SBX_ENV_WAVE_LEGACY_BASE = 2000, /* legacy waveNN envelope id: + [0..99] */
  SBX_ENV_WAVE_CUSTOM_BASE = 2100  /* literal customNN envelope id: + [0..99] */
} SbxEnvelopeWaveform;

typedef enum {
  SBX_INTERP_LINEAR = 0,
  SBX_INTERP_STEP = 1
} SbxInterpMode;

typedef enum {
  SBX_SOURCE_NONE = 0,
  SBX_SOURCE_STATIC = 1,
  SBX_SOURCE_KEYFRAMES = 2,
  SBX_SOURCE_CURVE = 3
} SbxSourceMode;

typedef enum {
  SBX_MIXFX_NONE = 0,
  SBX_MIXFX_SPIN = 1,
  SBX_MIXFX_PULSE = 2,
  SBX_MIXFX_BEAT = 3,
  SBX_MIXFX_AM = 4
} SbxMixFxType;

typedef enum {
  SBX_MIXAM_MODE_PULSE = 0,
  SBX_MIXAM_MODE_COS = 1
} SbxMixamMode;

typedef struct {
  int type;      /* SBX_MIXFX_* */
  int waveform;  /* SBX_WAVE_* */
  double carr;   /* mixspin width in microseconds */
  double res;    /* modulation/spin frequency in Hz */
  double amp;    /* 0..1 effect amount */
  /* mixam envelope controls (all cycle-relative 0..1 unless noted). */
  int mixam_mode;        /* m: 0 pulse (d/a/r/e active), 1 raised-cosine (d/a/r/e ignored) */
  double mixam_start;    /* s: cycle phase offset */
  double mixam_duty;     /* d: on-window duty */
  double mixam_attack;   /* a: attack share of on-window */
  double mixam_release;  /* r: release share of on-window */
  int mixam_edge_mode;   /* e: 0 hard, 1 linear, 2 smoothstep, 3 smootherstep */
  double mixam_floor;    /* f: minimum gain floor */
  int mixam_bind_program_beat; /* 1 => AM rate follows current program beat */
} SbxMixFxSpec;

typedef struct {
  double time_sec;
  double amp_pct;
  int interp; /* SBX_INTERP_* */
} SbxMixAmpKeyframe;

typedef struct {
  int active;            /* 1 => enabled, 0 => disabled */
  double delta;          /* dip depth parameter */
  double epsilon;        /* Gaussian width parameter */
  double period_sec;     /* half-period spacing in seconds */
  double end_level;      /* terminal linear level at end of main phase, 0..1 */
  double main_len_sec;   /* main phase duration in seconds (drop+hold) */
  double wake_len_sec;   /* wake phase duration in seconds */
  int wake_enabled;      /* 1 => include wake phase ramp */
} SbxMixModSpec;

typedef struct {
  double freq_hz;
  double adj;
} SbxAmpAdjustPoint;

typedef struct {
  size_t point_count;
  SbxAmpAdjustPoint points[SBX_MAX_AMP_ADJUST_POINTS];
} SbxAmpAdjustSpec;

typedef struct {
  double time_sec;
  int interp; /* SBX_INTERP_* */
} SbxTimedMixFxKeyframeInfo;

typedef enum {
  SBX_EXTRA_INVALID = 0,
  SBX_EXTRA_MIXAMP = 1,
  SBX_EXTRA_TONE = 2,
  SBX_EXTRA_MIXFX = 3
} SbxExtraTokenType;

typedef struct {
  double sample_rate; /* Hz, e.g. 44100 */
  int channels;       /* currently 2 (stereo) */
} SbxEngineConfig;

typedef struct {
  SbxToneMode mode;
  double carrier_hz;
  double beat_hz;
  double amplitude;  /* 0.0 .. 1.0 */
  int waveform;      /* SBX_WAVE_* carrier waveform */
  int envelope_waveform; /* SBX_ENV_WAVE_NONE or SBX_ENV_WAVE_* + [0..99] */
  double duty_cycle; /* for isochronic mode: 0.0 .. 1.0 (default 0.403014) */
  double iso_start;   /* cycle-relative start phase for isochronic mode (default 0.048493) */
  double iso_attack;  /* attack share of isochronic on-window (default 0.5) */
  double iso_release; /* release share of isochronic on-window (default 0.5) */
  int iso_edge_mode;  /* 0 hard, 1 linear, 2 smoothstep, 3 smootherstep */
} SbxToneSpec;

typedef struct {
  double start;   /* cycle-relative start phase (0..1) */
  double duty;    /* cycle-relative on-window width (0..1) */
  double attack;  /* attack share of on-window (0..1) */
  double release; /* release share of on-window (0..1) */
  int edge_mode;  /* 0 hard, 1 linear, 2 smoothstep, 3 smootherstep */
} SbxIsoEnvelopeSpec;

typedef struct {
  double time_sec; /* keyframe timestamp, >= 0, increasing */
  SbxToneSpec tone;
  int interp; /* interpolation mode to next keyframe: SBX_INTERP_* */
} SbxProgramKeyframe;

typedef struct SbxEngine SbxEngine;
typedef struct SbxContext SbxContext;
typedef struct SbxCurveProgram SbxCurveProgram;
typedef struct SbxAudioWriter SbxAudioWriter;
typedef struct SbxMixInput SbxMixInput;

typedef enum {
  SBX_DIAG_ERROR = 1,
  SBX_DIAG_WARNING = 2
} SbxDiagnosticSeverity;

typedef struct {
  int severity; /* SBX_DIAG_* */
  char code[SBX_DIAG_CODE_MAX];
  uint32_t line;
  uint32_t column;
  uint32_t end_line;
  uint32_t end_column;
  char message[SBX_DIAG_MESSAGE_MAX];
} SbxDiagnostic;

typedef struct {
  double carrier_start_hz;
  double carrier_end_hz;
  double carrier_span_sec;
  double beat_start_hz;
  double beat_target_hz;
  double beat_span_sec;
  double hold_min;
  double total_min;
  double wake_min;
  double beat_amp0_pct;
  double mix_amp0_pct;
} SbxCurveEvalConfig;

typedef struct {
  double beat_hz;
  double carrier_hz;
  double beat_amp_pct;
  double mix_amp_pct;
} SbxCurveEvalPoint;

typedef struct {
  size_t parameter_count;
  int has_solve;
  int has_carrier_expr;
  int has_amp_expr;
  int has_mixamp_expr;
  size_t beat_piece_count;
  size_t carrier_piece_count;
  size_t amp_piece_count;
  size_t mixamp_piece_count;
} SbxCurveInfo;

typedef struct {
  SbxToneMode mode;
  int waveform;
  double duty_cycle;
  double iso_start;
  double iso_attack;
  double iso_release;
  int iso_edge_mode;
  double amplitude;
  double duration_sec;
  int loop;
} SbxCurveSourceConfig;

typedef struct {
  SbxToneSpec start_tone;
  double carrier_end_hz;
  double beat_target_hz;
  int drop_sec;
  int hold_sec;
  int wake_sec;
  int slide;
  int step_len_sec;
  double fade_sec;
} SbxBuiltinDropConfig;

typedef struct {
  SbxToneSpec start_tone;
  double carrier_end_hz;
  double beat_target_hz;
  int drop_sec;
  int hold_sec;
  int wake_sec;
  int slide;
  int step_len_sec;
  double fade_sec;
  double sig_l;
  double sig_h;
} SbxBuiltinSigmoidConfig;

typedef struct {
  SbxToneSpec start_tone;
  double carrier_end_hz;
  int slide_sec;
  double fade_sec;
} SbxBuiltinSlideConfig;

typedef struct {
  const char *name;
  double value;
} SbxCurveParamOverride;

typedef struct {
  const char *path;
  const SbxCurveParamOverride *overrides;
  size_t override_count;
  SbxCurveEvalConfig eval_config;
} SbxCurveFileProgramConfig;

typedef struct {
  SbxToneSpec start_tone;
  int sample_span_sec;
  int main_span_sec;
  int wake_sec;
  int step_len_sec;
  int slide;
  int mute_program_tone;
  double fade_sec;
} SbxCurveTimelineConfig;

typedef struct {
  SbxProgramKeyframe *program_frames;
  size_t program_frame_count;
  SbxMixAmpKeyframe *mix_frames;
  size_t mix_frame_count;
} SbxCurveTimeline;

typedef struct {
  unsigned int rng_state; /* caller-owned RNG state for TPDF dithering */
} SbxPcm16DitherState;

typedef enum {
  SBX_PCM_DITHER_NONE = 0,
  SBX_PCM_DITHER_TPDF = 1
} SbxPcmDitherMode;

typedef enum {
  SBX_AUDIO_FILE_RAW = 0,
  SBX_AUDIO_FILE_WAV = 1,
  SBX_AUDIO_FILE_OGG = 2,
  SBX_AUDIO_FILE_FLAC = 3,
  SBX_AUDIO_FILE_MP3 = 4
} SbxAudioFileFormat;

typedef enum {
  SBX_MIX_INPUT_RAW = 0,
  SBX_MIX_INPUT_WAV = 1,
  SBX_MIX_INPUT_OGG = 2,
  SBX_MIX_INPUT_FLAC = 3,
  SBX_MIX_INPUT_MP3 = 4
} SbxMixInputFormat;

typedef enum {
  SBX_AUDIO_WRITER_INPUT_BYTES = 0,
  SBX_AUDIO_WRITER_INPUT_S16 = 1,
  SBX_AUDIO_WRITER_INPUT_F32 = 2,
  SBX_AUDIO_WRITER_INPUT_I32 = 3
} SbxAudioWriterInputMode;

typedef struct {
  unsigned int rng_state; /* caller-owned RNG state for quantization helpers */
  int dither_mode;        /* SBX_PCM_DITHER_* */
} SbxPcmConvertState;

typedef struct {
  double sample_rate;          /* Hz, e.g. 44100 */
  int channels;                /* currently 2 (stereo) */
  int format;                  /* SBX_AUDIO_FILE_* */
  int pcm_bits;                /* raw/WAV/FLAC PCM depth: 8, 16, 24 */
  double ogg_quality;          /* Vorbis q scale 0..10 */
  int ogg_quality_set;         /* 1 => apply ogg_quality */
  double flac_compression;     /* FLAC level 0..12 */
  int flac_compression_set;    /* 1 => apply flac_compression */
  int mp3_bitrate;             /* MP3 CBR bitrate in kbps */
  int mp3_bitrate_set;         /* 1 => apply mp3_bitrate */
  int mp3_quality;             /* LAME quality 0 best .. 9 fastest */
  int mp3_quality_set;         /* 1 => apply mp3_quality */
  double mp3_vbr_quality;      /* LAME VBR quality 0 best .. 9 fastest */
  int mp3_vbr_quality_set;     /* 1 => prefer VBR using mp3_vbr_quality */
  int prefer_float_input;      /* 1 => use float MP3 path if runtime supports it */
} SbxAudioWriterConfig;

typedef void (*SbxMixWarnCallback)(void *user, const char *msg);

typedef struct {
  int mix_section;             /* filename #<digits> suffix, or -1 if absent */
  int output_rate_hz;          /* current output rate before mix source opens */
  int output_rate_is_default;  /* 1 => mix source may override output_rate_hz */
  int take_stream_ownership;   /* 1 => destroy closes the supplied FILE * */
  SbxMixWarnCallback warn_cb;  /* optional warning sink for ReplayGain/looper notes */
  void *warn_user;             /* user data for warn_cb */
} SbxMixInputConfig;

typedef struct {
  int opt_S;                   /* safe preamble -S */
  int opt_E;                   /* safe preamble -E */
  int have_D;                  /* safe preamble -D present */
  int have_Q;                  /* safe preamble -Q present */
  int have_T;                  /* safe preamble -T present */
  int T_ms;                    /* safe preamble -T value in ms */
  int have_L;                  /* safe preamble -L present */
  int L_ms;                    /* safe preamble -L value in ms */
  int have_q;                  /* safe preamble -q present */
  int q_mult;                  /* safe preamble -q multiplier */
  int have_r;                  /* safe preamble -r present */
  int rate;                    /* safe preamble -r sample rate */
  int have_R;                  /* safe preamble -R present */
  int prate;                   /* safe preamble -R parameter refresh rate */
  int have_b;                  /* safe preamble -b present */
  int pcm_bits;                /* safe preamble -b PCM depth */
  int have_N;                  /* safe preamble -N present */
  int have_V;                  /* safe preamble -V present */
  int volume_pct;              /* safe preamble -V value in percent */
  int have_w;                  /* safe preamble -w present */
  int waveform;                /* safe preamble -w SBX_WAVE_* */
  int have_c;                  /* safe preamble -c present */
  SbxAmpAdjustSpec amp_adjust; /* safe preamble -c amplitude-adjust curve */
  int have_A;                  /* safe preamble -A present */
  SbxMixModSpec mix_mod;       /* safe preamble -A spec */
  int have_I;                  /* safe preamble -I present */
  SbxIsoEnvelopeSpec iso_env;  /* safe preamble -I spec */
  int have_H;                  /* safe preamble -H present */
  SbxMixFxSpec mixam_env;      /* safe preamble -H spec */
  int have_W;                  /* safe preamble -W present */
  int have_F;                  /* safe preamble -F present */
  int fade_ms;                 /* safe preamble -F fade time in ms */
#ifdef T_MACOSX
  int have_B;                  /* safe preamble -B present */
  int buffer_samples;          /* safe preamble -B size in samples */
#endif
  int have_K;                  /* safe preamble -K present */
  int mp3_bitrate;             /* safe preamble -K bitrate */
  int have_J;                  /* safe preamble -J present */
  int mp3_quality;             /* safe preamble -J quality */
  int have_X;                  /* safe preamble -X present */
  double mp3_vbr_quality;      /* safe preamble -X VBR quality */
  int have_U;                  /* safe preamble -U present */
  double ogg_quality;          /* safe preamble -U quality */
  int have_Z;                  /* safe preamble -Z present */
  double flac_compression;     /* safe preamble -Z compression level */
#ifdef T_LINUX
  char *device_path;           /* safe preamble -d ALSA device path */
#endif
  char *mix_path;              /* safe preamble -m path (caller frees via helper) */
  char *out_path;              /* safe preamble -o path (caller frees via helper) */
} SbxSafeSeqfilePreamble;

typedef int (*SbxSeqOptionLineCallback)(const char *line, void *user);

typedef struct {
  int default_waveform;         /* default waveform for unprefixed tone/effect tokens */
  int have_iso_override;        /* apply to isochronic tones without custom env waveform */
  SbxIsoEnvelopeSpec iso_env;   /* explicit `-I`-style override */
  int have_mixam_override;      /* apply to mixam/mixpulse:beat tokens */
  int mixam_mode;               /* SbxMixamMode */
  double mixam_start;
  double mixam_duty;
  double mixam_attack;
  double mixam_release;
  int mixam_edge_mode;
  double mixam_floor;
} SbxImmediateParseConfig;

typedef struct {
  SbxToneSpec tones[SBX_MAX_AUX_TONES + 1];
  size_t tone_count;
  SbxMixFxSpec mix_fx[SBX_MAX_AUX_TONES];
  size_t mix_fx_count;
  int have_mix;
  double mix_amp_pct;
} SbxImmediateSpec;

typedef struct {
  int have_mix;
  double mix_amp_pct;
  SbxToneSpec aux_tones[SBX_MAX_AUX_TONES];
  size_t aux_count;
  SbxMixFxSpec mix_fx[SBX_MAX_AUX_TONES];
  size_t mix_fx_count;
  int unsupported;
  char bad_token[128];
} SbxRuntimeExtraSpec;

typedef struct {
  double time_sec;            /* context timeline time at snapshot */
  int source_mode;            /* SBX_SOURCE_* */
  SbxToneSpec primary_tone;   /* evaluated primary tone at time_sec */
  double program_beat_hz;     /* convenience mirror of primary_tone.beat_hz */
  double program_carrier_hz;  /* convenience mirror of primary_tone.carrier_hz */
  double mix_amp_pct;         /* evaluated mix amp profile at time_sec */
  size_t voice_count;         /* active keyframed/static voice lanes */
  size_t aux_tone_count;      /* configured auxiliary overlay tones */
  size_t mix_effect_count;    /* configured static + timed mix-effect slots */
} SbxRuntimeTelemetry;

typedef void (*SbxTelemetryCallback)(const SbxRuntimeTelemetry *telem, void *user);

typedef struct {
  SbxEngineConfig engine;               /* sample rate/channels for new context */
  const SbxMixAmpKeyframe *mix_kfs;     /* optional mix amp keyframes */
  size_t mix_kf_count;
  double default_mix_amp_pct;           /* runtime default when no keyframes */
  const SbxMixFxSpec *mix_fx;           /* optional static mix effects */
  size_t mix_fx_count;
  const SbxToneSpec *aux_tones;         /* optional aux runtime overlays */
  size_t aux_count;
  const SbxMixModSpec *mix_mod;         /* optional host mix modulation */
  const SbxAmpAdjustSpec *amp_adjust;   /* optional -c amplitude-adjust curve */
} SbxRuntimeContextConfig;

/* ----- Version and status ----- */

/* Runtime library version string (human-readable). */
const char *sbx_version(void);

/* Public API version integer (for feature gating). */
int sbx_api_version(void);

/* Convert status code (SBX_*) to short text. */
const char *sbx_status_string(int status);

/* ----- Defaults ----- */

/* Fill cfg with library defaults (44.1k stereo). */
void sbx_default_engine_config(SbxEngineConfig *cfg);

/* Fill tone with default binaural-safe values. */
void sbx_default_tone_spec(SbxToneSpec *tone);

/* Fill spec with library-default isochronic envelope values. */
void sbx_default_iso_envelope_spec(SbxIsoEnvelopeSpec *spec);

/* Fill spec with documented `-H`/mixam envelope defaults. */
void sbx_default_mixam_envelope_spec(SbxMixFxSpec *spec);

/* Fill cfg with default `.sbgf` evaluation environment values. */
void sbx_default_curve_eval_config(SbxCurveEvalConfig *cfg);

/* Fill cfg with default curve-backed context source settings. */
void sbx_default_curve_source_config(SbxCurveSourceConfig *cfg);

/* Fill cfg with library-default built-in drop/sigmoid/slide settings. */
void sbx_default_builtin_drop_config(SbxBuiltinDropConfig *cfg);
void sbx_default_builtin_sigmoid_config(SbxBuiltinSigmoidConfig *cfg);
void sbx_default_builtin_slide_config(SbxBuiltinSlideConfig *cfg);
void sbx_default_curve_file_program_config(SbxCurveFileProgramConfig *cfg);
void sbx_default_curve_timeline_config(SbxCurveTimelineConfig *cfg);
void sbx_default_runtime_context_config(SbxRuntimeContextConfig *cfg);

/* Fill dither state with library-default seed. */
void sbx_default_pcm16_dither_state(SbxPcm16DitherState *state);

/* Fill spec with library-default -A mix modulation parameters. */
void sbx_default_mix_mod_spec(SbxMixModSpec *spec);

/* Fill spec with library-default -c amplitude-adjust settings (disabled). */
void sbx_default_amp_adjust_spec(SbxAmpAdjustSpec *spec);

/* Set explicit seed for deterministic PCM16 dithering. */
void sbx_seed_pcm16_dither_state(SbxPcm16DitherState *state, unsigned int seed);

/* Fill generic PCM conversion state with default seed + TPDF dither. */
void sbx_default_pcm_convert_state(SbxPcmConvertState *state);

/* Fill cfg with default file-writer settings (44.1k stereo 16-bit WAV). */
void sbx_default_audio_writer_config(SbxAudioWriterConfig *cfg);
void sbx_default_mix_input_config(SbxMixInputConfig *cfg);

/* Fill cfg with default safe sequence-file preamble values. */
void sbx_default_safe_seqfile_preamble(SbxSafeSeqfilePreamble *cfg);

/* Free heap-owned fields populated by safe sequence-file helpers. */
void sbx_free_safe_seqfile_preamble(SbxSafeSeqfilePreamble *cfg);

/* Set explicit seed and dither mode for generic PCM conversion helpers. */
void sbx_seed_pcm_convert_state(SbxPcmConvertState *state,
                                unsigned int seed,
                                int dither_mode);

/* ----- Engine API ----- */

/* Create low-level engine instance. Returns NULL on failure. */
SbxEngine *sbx_engine_create(const SbxEngineConfig *cfg);

/* Destroy engine created by sbx_engine_create(). */
void sbx_engine_destroy(SbxEngine *eng);

/* Reset internal phase/state on existing engine. */
void sbx_engine_reset(SbxEngine *eng);

/* Set active tone for engine rendering. */
int sbx_engine_set_tone(SbxEngine *eng, const SbxToneSpec *tone);

/* Render interleaved stereo float samples into out[frames * channels]. */
int sbx_engine_render_f32(SbxEngine *eng, float *out, size_t frames);

/* Last engine-local error text. */
const char *sbx_engine_last_error(const SbxEngine *eng);

/* ----- PCM conversion helpers ----- */

/*
 * Convert normalized float samples (-1..1 nominal) to signed 16-bit PCM.
 * If dither_state is non-NULL, TPDF dither is added before rounding.
 */
int sbx_convert_f32_to_s16(const float *in,
                           short *out,
                           size_t sample_count,
                           SbxPcm16DitherState *dither_state);

/*
 * Convert normalized float samples (-1..1 nominal) to signed PCM using an
 * explicit conversion state. dither_mode controls whether TPDF dither is
 * added before rounding.
 */
int sbx_convert_f32_to_s16_ex(const float *in,
                              int16_t *out,
                              size_t sample_count,
                              SbxPcmConvertState *state);
int sbx_convert_f32_to_s24_32(const float *in,
                              int32_t *out,
                              size_t sample_count,
                              SbxPcmConvertState *state);
int sbx_convert_f32_to_s32(const float *in,
                           int32_t *out,
                           size_t sample_count,
                           SbxPcmConvertState *state);

/* ----- Optional file/container writer API ----- */

/*
 * Create a file writer for raw/WAV/OGG/FLAC/MP3 output.
 * - `path` must name a writable filesystem path.
 * - WAV headers are finalized on close using the actual byte count written.
 * - OGG/FLAC use libsndfile dynamically by default.
 * - MP3 uses libmp3lame dynamically by default.
 */
SbxAudioWriter *sbx_audio_writer_create_path(const char *path,
                                             const SbxAudioWriterConfig *cfg);

/* Close/finalize the writer. Safe to call more than once. */
int sbx_audio_writer_close(SbxAudioWriter *writer);

/* Destroy a writer created by sbx_audio_writer_create_path(). */
void sbx_audio_writer_destroy(SbxAudioWriter *writer);

/* Last writer-local error text. */
const char *sbx_audio_writer_last_error(const SbxAudioWriter *writer);

/*
 * Return the host-side frame byte budget used by this writer.
 * - raw/WAV: output PCM frame bytes
 * - OGG/Vorbis: 8 (float stereo frame)
 * - FLAC 24-bit: 6
 * - FLAC 16-bit: 4
 * - MP3: 8 for float input, otherwise 4
 */
int sbx_audio_writer_frame_bytes(const SbxAudioWriter *writer);

/* Return the preferred input mode for writes through this writer. */
int sbx_audio_writer_input_mode(const SbxAudioWriter *writer);

/* Write raw byte data (used for raw/WAV output paths only). */
int sbx_audio_writer_write_bytes(SbxAudioWriter *writer,
                                 const void *buf,
                                 size_t byte_count);

/* Write interleaved stereo PCM16 frames (used by FLAC16 / legacy OGG paths). */
int sbx_audio_writer_write_s16(SbxAudioWriter *writer,
                               const int16_t *pcm,
                               size_t frame_count);

/* Write interleaved stereo float frames (used by OGG runtime path). */
int sbx_audio_writer_write_f32(SbxAudioWriter *writer,
                               const float *pcm,
                               size_t frame_count);

/* Write interleaved stereo PCM32 frames holding 24-bit FLAC values. */
int sbx_audio_writer_write_i32(SbxAudioWriter *writer,
                               const int32_t *pcm,
                               size_t frame_count);

/*
 * Open a mix-input decoder around an already-open stdio stream. The frontend
 * remains responsible for choosing the stream/path; sbagenxlib owns the
 * format-specific decoding behavior behind the returned handle.
 */
SbxMixInput *sbx_mix_input_create_stdio(FILE *stream,
                                        const char *path_hint,
                                        const SbxMixInputConfig *cfg);
int sbx_mix_input_read(SbxMixInput *input, int *dst, int sample_count);
void sbx_mix_input_destroy(SbxMixInput *input);
const char *sbx_mix_input_last_error(const SbxMixInput *input);
int sbx_mix_input_output_rate(const SbxMixInput *input);
int sbx_mix_input_output_rate_is_default(const SbxMixInput *input);
int sbx_mix_input_format(const SbxMixInput *input);

/*
 * Parse/strip a safe `-SE`-style sequence preamble from in-memory text.
 * On success:
 * - `*out_text` receives a heap-owned copy with option-only preamble lines
 *   blanked out so downstream line numbers remain stable.
 * - `out_cfg` receives the parsed wrapper settings.
 * On failure:
 * - returns `SBX_E*`
 * - `errbuf` receives a short explanation when provided.
 */
int sbx_prepare_safe_seq_text(const char *text,
                              char **out_text,
                              SbxSafeSeqfilePreamble *out_cfg,
                              char *errbuf,
                              size_t errbuf_sz);

/* File-based convenience wrapper around sbx_prepare_safe_seq_text(). */
int sbx_prepare_safe_seqfile_text(const char *path,
                                  char **out_text,
                                  SbxSafeSeqfilePreamble *out_cfg,
                                  char *errbuf,
                                  size_t errbuf_sz);

/*
 * Validate `.sbg` / `.sbgf` text and return structured diagnostics suitable
 * for editor integrations. Validation success returns `SBX_OK`; syntax/runtime
 * issues are reported via the diagnostics array rather than the return code.
 * The caller owns the returned array and must free it with
 * sbx_free_diagnostics().
 */
int sbx_validate_sbg_text(const char *text,
                          const char *source_name,
                          SbxDiagnostic **out_diags,
                          size_t *out_count);
int sbx_validate_sbgf_text(const char *text,
                           const char *source_name,
                           SbxDiagnostic **out_diags,
                           size_t *out_count);
void sbx_free_diagnostics(SbxDiagnostic *diags);

/*
 * Recognize/execute an option-only historical `.sbg` wrapper.
 * Success requires:
 * - every non-comment payload line begins with '-'
 * - at least one option line contains `-p` or `-i`
 * On success the callback is invoked once per trimmed option line.
 */
int sbx_run_option_only_seq_wrapper_text(const char *text,
                                         SbxSeqOptionLineCallback cb,
                                         void *user,
                                         char *errbuf,
                                         size_t errbuf_sz);

/* File-based convenience wrapper around sbx_run_option_only_seq_wrapper_text(). */
int sbx_run_option_only_seq_wrapper_file(const char *path,
                                         SbxSeqOptionLineCallback cb,
                                         void *user,
                                         char *errbuf,
                                         size_t errbuf_sz);

/* Fill cfg with defaults for immediate-token parsing (`-i`). */
void sbx_default_immediate_parse_config(SbxImmediateParseConfig *cfg);

/* Parse a token list into tones/mix amp/mix effects for immediate runtime use. */
int sbx_parse_immediate_tokens(const char *const *tokens,
                               size_t token_count,
                               const SbxImmediateParseConfig *cfg,
                               SbxImmediateSpec *out_spec,
                               char *errbuf,
                               size_t errbuf_sz);

/* Fill extra-spec with defaults for built-in/runtime extra-token parsing. */
void sbx_default_runtime_extra_spec(SbxRuntimeExtraSpec *spec);

/*
 * Parse a whitespace-delimited extra-token list used by built-in/runtime paths.
 * Unsupported tokens are recorded in out_spec->unsupported/bad_token without
 * failing the call so frontends can decide how to surface them.
 */
int sbx_parse_runtime_extra_text(const char *extra,
                                 const SbxImmediateParseConfig *cfg,
                                 SbxRuntimeExtraSpec *out_spec,
                                 char *errbuf,
                                 size_t errbuf_sz);

int sbx_validate_runtime_mix_fx_requirements(int mix_input_active,
                                             int have_mix_amp,
                                             size_t mix_fx_count,
                                             const char *prog_name,
                                             const char *mix_scope_desc,
                                             char *errbuf,
                                             size_t errbuf_sz);

/* Convenience helper for front-ends that need to detect AM/mixam extras. */
int sbx_runtime_extra_has_mixam(const SbxRuntimeExtraSpec *spec);

/*
 * ----- Parser/formatter helpers -----
 */

/* Parse one tone token using sine default waveform. */
int sbx_parse_tone_spec(const char *spec, SbxToneSpec *out_tone);

/* Parse one tone token with explicit default waveform for unprefixed tones. */
int sbx_parse_tone_spec_ex(const char *spec, int default_waveform, SbxToneSpec *out_tone);

/* Parse SBG clock token HH:MM or HH:MM:SS. Returns consumed chars. */
int sbx_parse_sbg_clock_token(const char *tok, size_t *out_consumed, double *out_sec);

/*
 * Parse an `-I` isochronic envelope option spec such as:
 *   s=0:d=0.35:a=0.12:r=0.65:e=2
 * The parser updates the supplied spec in place, so callers can either:
 *   - initialize from sbx_default_iso_envelope_spec(), or
 *   - initialize from an existing value and override selected fields
 */
int sbx_is_iso_envelope_option_spec(const char *spec);
int sbx_parse_iso_envelope_option_spec(const char *spec,
                                       SbxIsoEnvelopeSpec *out_spec,
                                       char *errbuf,
                                       size_t errbuf_sz);

/*
 * Detect whether a string looks like an `-H` mixam envelope option spec.
 * This mirrors the CLI behavior for the optional argument form.
 */
int sbx_is_mixam_envelope_option_spec(const char *spec);

/*
 * Parse an `-H` mixam envelope option spec such as:
 *   m=pulse:s=0:d=0.5:a=0.2:r=0.4:e=2:f=0.25
 * The parser updates the supplied spec in place, so callers can either:
 *   - initialize from sbx_default_mixam_envelope_spec(), or
 *   - initialize from an existing value and override selected fields
 */
int sbx_parse_mixam_envelope_option_spec(const char *spec,
                                         SbxMixFxSpec *out_spec,
                                         char *errbuf,
                                         size_t errbuf_sz);

/*
 * Detect whether a string looks like an `-A` mix-modulation option spec.
 * This mirrors the CLI behavior for the optional argument form.
 */
int sbx_is_mix_mod_option_spec(const char *spec);

/*
 * Parse an `-A` mix-modulation option spec such as:
 *   d=0.3:e=0.3:k=10:E=0.7
 * The parser updates the supplied spec in place, so callers can either:
 *   - initialize from sbx_default_mix_mod_spec(), or
 *   - initialize from an existing value and override selected fields
 *
 * `k` is interpreted in minutes in the option surface and converted to
 * period_sec in the returned SbxMixModSpec.
 */
int sbx_parse_mix_mod_option_spec(const char *spec,
                                  SbxMixModSpec *out_spec,
                                  char *errbuf,
                                  size_t errbuf_sz);

/*
 * Parse a `-c` amplitude-adjust option spec such as:
 *   80=1,40=2,30=4,20=6
 * The parser appends points to the supplied spec, then sorts them by
 * increasing frequency. This matches repeated `-c` option accumulation.
 */
int sbx_parse_amp_adjust_option_spec(const char *spec,
                                     SbxAmpAdjustSpec *out_spec,
                                     char *errbuf,
                                     size_t errbuf_sz);

/* Format mix effect as canonical token (mixspin/mixpulse/mixbeat/mixam). */
int sbx_format_mix_fx_spec(const SbxMixFxSpec *fx, char *out, size_t out_sz);

/*
 * Parse one extra token as mix amp, tone, or mix effect.
 * out_type is set to SBX_EXTRA_* on success.
 */
int sbx_parse_extra_token(const char *tok,
                          int default_waveform,
                          int *out_type,
                          SbxToneSpec *out_tone,
                          SbxMixFxSpec *out_fx,
                          double *out_mix_amp_pct);

/* Format tone as canonical token string. */
int sbx_format_tone_spec(const SbxToneSpec *tone, char *out, size_t out_sz);

/* ----- Curve program API (.sbgf) ----- */

/* Create/destroy reusable `.sbgf` curve program object. */
SbxCurveProgram *sbx_curve_create(void);
void sbx_curve_destroy(SbxCurveProgram *curve);

/* Reset loaded/compiled state on an existing curve object. */
void sbx_curve_reset(SbxCurveProgram *curve);

/* Load `.sbgf` text/file into curve object. */
int sbx_curve_load_text(SbxCurveProgram *curve, const char *text, const char *source_name);
int sbx_curve_load_file(SbxCurveProgram *curve, const char *path);

/* Override/add one parameter value before preparation. */
int sbx_curve_set_param(SbxCurveProgram *curve, const char *name, double value);

/* Compile/prepare loaded curve expressions for evaluation. */
int sbx_curve_prepare(SbxCurveProgram *curve, const SbxCurveEvalConfig *cfg);

/* Evaluate prepared curve at timeline position t_sec. */
int sbx_curve_eval(SbxCurveProgram *curve, double t_sec, SbxCurveEvalPoint *out_point);

/*
 * Sample effective beat/pulse frequency from a prepared curve program over
 * a caller-specified time range. The curve must already be prepared.
 * out_t_sec is optional (may be NULL).
 */
int sbx_curve_sample_program_beat(SbxCurveProgram *curve,
                                  double t0_sec,
                                  double t1_sec,
                                  size_t sample_count,
                                  double *out_t_sec,
                                  double *out_hz);

/* Compute the internal coefficients used by the built-in sigmoid program. */
int sbx_compute_sigmoid_coefficients(int drop_sec,
                                     double beat_start_hz,
                                     double beat_target_hz,
                                     double sig_l,
                                     double sig_h,
                                     double *out_a,
                                     double *out_b);

/* Build exact runtime curve programs for -p drop / -p sigmoid. */
int sbx_build_drop_curve_program(const SbxBuiltinDropConfig *cfg,
                                 SbxCurveProgram **out_curve);
int sbx_build_sigmoid_curve_program(const SbxBuiltinSigmoidConfig *cfg,
                                    SbxCurveProgram **out_curve);

/* Load, override, and prepare a `.sbgf` file program in one step. */
int sbx_prepare_curve_file_program(const SbxCurveFileProgramConfig *cfg,
                                   SbxCurveProgram **out_curve);

/* Build keyframed programs for the built-in drop / sigmoid / slide generators. */
int sbx_build_drop_keyframes(const SbxBuiltinDropConfig *cfg,
                             SbxProgramKeyframe **out_frames,
                             size_t *out_frame_count);
int sbx_build_sigmoid_keyframes(const SbxBuiltinSigmoidConfig *cfg,
                                SbxProgramKeyframe **out_frames,
                                size_t *out_frame_count);
int sbx_build_slide_keyframes(const SbxBuiltinSlideConfig *cfg,
                              SbxProgramKeyframe **out_frames,
                              size_t *out_frame_count);

/* Build program and optional mix-amplitude timelines from a prepared curve. */
int sbx_build_curve_timeline(SbxCurveProgram *curve,
                             const SbxCurveTimelineConfig *cfg,
                             SbxCurveTimeline *out_timeline);
void sbx_free_curve_timeline(SbxCurveTimeline *timeline);

/* Curve introspection. */
int sbx_curve_get_info(const SbxCurveProgram *curve, SbxCurveInfo *out_info);
size_t sbx_curve_param_count(const SbxCurveProgram *curve);
int sbx_curve_get_param(const SbxCurveProgram *curve,
                        size_t index,
                        const char **out_name,
                        double *out_value);
const char *sbx_curve_source_name(const SbxCurveProgram *curve);
const char *sbx_curve_last_error(const SbxCurveProgram *curve);

/* ----- Context lifecycle/load/render ----- */

/* Create context (higher-level runtime/load object). */
SbxContext *sbx_context_create(const SbxEngineConfig *cfg);

/* Destroy context created by sbx_context_create(). */
void sbx_context_destroy(SbxContext *ctx);

/* Reset context time and active source state. */
void sbx_context_reset(SbxContext *ctx);

/* Set static tone source on context. */
int sbx_context_set_tone(SbxContext *ctx, const SbxToneSpec *tone);

/* Set default waveform used by unprefixed parsed tones. */
int sbx_context_set_default_waveform(SbxContext *ctx, int waveform);

/* Apply or clear sequence-load `-I`/`-H` style overrides used by native loaders. */
int sbx_context_set_sequence_iso_override(SbxContext *ctx, const SbxIsoEnvelopeSpec *spec);
int sbx_context_set_sequence_mixam_override(SbxContext *ctx, const SbxMixFxSpec *spec);

/* Parse and load single tone token onto context. */
int sbx_context_load_tone_spec(SbxContext *ctx, const char *tone_spec);

/*
 * Load a prepared curve program as an exact runtime source.
 * On success the context takes ownership of `curve`.
 */
int sbx_context_load_curve_program(SbxContext *ctx,
                                   SbxCurveProgram *curve,
                                   const SbxCurveSourceConfig *cfg);

/* Load keyframed program (strictly increasing time_sec). */
int sbx_context_load_keyframes(SbxContext *ctx,
                               const SbxProgramKeyframe *frames,
                               size_t frame_count,
                               int loop);

/* Load minimal keyframe sequence text. */
int sbx_context_load_sequence_text(SbxContext *ctx, const char *text, int loop);

/* Load minimal keyframe sequence file. */
int sbx_context_load_sequence_file(SbxContext *ctx, const char *path, int loop);

/* Load SBG timing subset text (HH:MM[:SS], NOW, blocks, names). */
int sbx_context_load_sbg_timing_text(SbxContext *ctx, const char *text, int loop);

/* Load SBG timing subset file. */
int sbx_context_load_sbg_timing_file(SbxContext *ctx, const char *path, int loop);

/* ----- Runtime overlays: aux tones ----- */

/* Replace auxiliary overlay tone list (max SBX_MAX_AUX_TONES). */
int sbx_context_set_aux_tones(SbxContext *ctx, const SbxToneSpec *tones, size_t tone_count);

/* Get number of currently configured auxiliary tones. */
size_t sbx_context_aux_tone_count(const SbxContext *ctx);

/* Read configured auxiliary tone by index. */
int sbx_context_get_aux_tone(const SbxContext *ctx, size_t index, SbxToneSpec *out);

/* ----- Runtime overlays: mix effects ----- */

/* Parse one mix effect token (mixspin/mixpulse/mixbeat/mixam). */
int sbx_parse_mix_fx_spec(const char *spec, int default_waveform, SbxMixFxSpec *out_fx);

/* Replace context mix-effect chain. */
int sbx_context_set_mix_effects(SbxContext *ctx, const SbxMixFxSpec *fxv, size_t fx_count);

/* Get number of configured mix effects. */
size_t sbx_context_mix_effect_count(const SbxContext *ctx);

/* Read configured mix effect by index. */
int sbx_context_get_mix_effect(const SbxContext *ctx, size_t index, SbxMixFxSpec *out_fx);

/* Apply configured mix effects to one stereo sample pair. */
int sbx_context_apply_mix_effects(SbxContext *ctx,
                                  double mix_l,
                                  double mix_r,
                                  double base_amp,
                                  double *out_add_l,
                                  double *out_add_r);

/*
 * Full mix-stream sample path used by runtime adapters.
 * Accepts int16 mix samples and returns additive stereo contribution.
 * mix_mod_mul is an optional extra host multiplier applied on top of any
 * configured SbxMixModSpec runtime modulation.
 */
int sbx_context_mix_stream_sample(SbxContext *ctx,
                                  double t_sec,
                                  int mix_l_sample,
                                  int mix_r_sample,
                                  double mix_mod_mul,
                                  double *out_add_l,
                                  double *out_add_r);

/* ----- Runtime overlays: mix amp keyframes ----- */

/* Replace mix amplitude keyframe profile. */
int sbx_context_set_mix_amp_keyframes(SbxContext *ctx,
                                      const SbxMixAmpKeyframe *kfs,
                                      size_t kf_count,
                                      double default_amp_pct);

/* Replace mix-modulation runtime profile used by the -A host option. */
int sbx_context_set_mix_mod(SbxContext *ctx, const SbxMixModSpec *spec);

/* Replace runtime -c amplitude-adjust profile. */
int sbx_context_set_amp_adjust(SbxContext *ctx, const SbxAmpAdjustSpec *spec);

/* Read currently configured mix-modulation profile. */
int sbx_context_get_mix_mod(const SbxContext *ctx, SbxMixModSpec *out);

/* Report whether a mix-modulation runtime profile is active. */
int sbx_context_has_mix_mod(const SbxContext *ctx);

/* One-call runtime extras setup (mix amp + mix effects + aux tones). */
int sbx_context_configure_runtime(SbxContext *ctx,
                                  const SbxMixAmpKeyframe *mix_kfs,
                                  size_t mix_kf_count,
                                  double default_mix_amp_pct,
                                  const SbxMixFxSpec *mix_fx,
                                  size_t mix_fx_count,
                                  const SbxToneSpec *aux_tones,
                                  size_t aux_count);

int sbx_runtime_context_create_from_immediate(const SbxImmediateSpec *imm,
                                              const SbxRuntimeContextConfig *cfg,
                                              SbxContext **out_ctx);
int sbx_runtime_context_create_from_keyframes(const SbxProgramKeyframe *kfs,
                                              size_t n,
                                              int loop_flag,
                                              const SbxRuntimeContextConfig *cfg,
                                              double *out_total_sec,
                                              SbxContext **out_ctx);
int sbx_runtime_context_create_from_curve_program(SbxCurveProgram *curve,
                                                  const SbxCurveSourceConfig *curve_cfg,
                                                  const SbxRuntimeContextConfig *cfg,
                                                  double *out_total_sec,
                                                  SbxContext **out_ctx);

/* Evaluate mix amplitude percentage at context time t_sec. */
double sbx_context_mix_amp_at(SbxContext *ctx, double t_sec);

/* Evaluate mix-modulation multiplier at context time t_sec. */
double sbx_context_mix_mod_mul_at(SbxContext *ctx, double t_sec);

/* Evaluate runtime-effective mix amplitude percentage at t_sec. */
double sbx_context_mix_amp_effective_at(SbxContext *ctx, double t_sec);

/* Evaluate one mix-modulation spec directly, without a context. */
double sbx_mix_mod_mul_at(const SbxMixModSpec *spec, double t_sec);

/*
 * Sample evaluated mix amplitude percentage over [t0_sec, t1_sec].
 * - sample_count must be >= 1.
 * - out_amp_pct must have sample_count elements.
 * - out_t_sec is optional (may be NULL).
 */
int sbx_context_sample_mix_amp(SbxContext *ctx,
                               double t0_sec,
                               double t1_sec,
                               size_t sample_count,
                               double *out_t_sec,
                               double *out_amp_pct);

/* Number of explicit mix amplitude keyframes currently loaded. */
size_t sbx_context_mix_amp_keyframe_count(const SbxContext *ctx);

/* Read explicit mix amplitude keyframe by index. */
int sbx_context_get_mix_amp_keyframe(const SbxContext *ctx,
                                     size_t index,
                                     SbxMixAmpKeyframe *out);

/* Report whether the loaded/runtime context has explicit mix/<amp> control. */
int sbx_context_has_mix_amp_control(const SbxContext *ctx);

/* Report whether the loaded/runtime context has active mix-effect content. */
int sbx_context_has_mix_effects(const SbxContext *ctx);

/* Number of timed mix-effect keyframes currently loaded. */
size_t sbx_context_timed_mix_effect_keyframe_count(const SbxContext *ctx);

/* Number of timed mix-effect slots in each keyframe. */
size_t sbx_context_timed_mix_effect_slot_count(const SbxContext *ctx);

/* Read timed mix-effect keyframe metadata by index. */
int sbx_context_get_timed_mix_effect_keyframe_info(const SbxContext *ctx,
                                                   size_t index,
                                                   SbxTimedMixFxKeyframeInfo *out);

/*
 * Read one timed mix-effect slot from a loaded keyframe.
 * out_present is set to 1 when the slot was explicitly present in that
 * keyframe, or 0 when the slot is implicitly empty/none.
 */
int sbx_context_get_timed_mix_effect_slot(const SbxContext *ctx,
                                          size_t keyframe_index,
                                          size_t slot_index,
                                          SbxMixFxSpec *out_fx,
                                          int *out_present);

/*
 * Evaluate the effective mix-effect chain at one context time.
 * - Returns static runtime mix effects first, followed by evaluated timed
 *   native `.sbg` mix-effect slots.
 * - If out_fxv is NULL, out_count may be used to query the required size.
 * - Entries with type SBX_MIXFX_NONE represent empty timed slots.
 */
int sbx_context_sample_mix_effects(SbxContext *ctx,
                                   double t_sec,
                                   SbxMixFxSpec *out_fxv,
                                   size_t out_slots,
                                   size_t *out_count);

/*
 * Evaluate the effective tone set at one context time.
 * - Returns keyframed/static voice lanes first, followed by auxiliary tones.
 * - voice lane count is given by sbx_context_voice_count().
 * - If out_tones is NULL, out_count may be used to query the required size.
 * - Does not advance context render time.
 */
int sbx_context_eval_active_tones(SbxContext *ctx,
                                  double t_sec,
                                  SbxToneSpec *out_tones,
                                  size_t out_slots,
                                  size_t *out_count);

/*
 * Runtime telemetry:
 * - Register callback to receive one snapshot per sbx_context_render_f32 call.
 * - Pass NULL callback to disable.
 */
int sbx_context_set_telemetry_callback(SbxContext *ctx,
                                       SbxTelemetryCallback cb,
                                       void *user);

/* Retrieve a runtime telemetry snapshot at the current context time. */
int sbx_context_get_runtime_telemetry(SbxContext *ctx, SbxRuntimeTelemetry *out);

/* ----- Introspection/render ----- */

/* Number of currently loaded keyframes. */
size_t sbx_context_keyframe_count(const SbxContext *ctx);

/* Number of active voice lanes in the loaded source (1 for static tones). */
size_t sbx_context_voice_count(const SbxContext *ctx);

/* Source kind currently loaded into the context (SBX_SOURCE_*). */
int sbx_context_source_mode(const SbxContext *ctx);

/* Whether the current keyframed source is configured to loop. */
int sbx_context_is_looping(const SbxContext *ctx);

/* Read keyframe by index. */
int sbx_context_get_keyframe(const SbxContext *ctx, size_t index, SbxProgramKeyframe *out);

/* Read a specific voice lane from a loaded keyframe. */
int sbx_context_get_keyframe_voice(const SbxContext *ctx,
                                   size_t index,
                                   size_t voice_index,
                                   SbxProgramKeyframe *out);

/* Program duration in seconds (0 for static tone sources). */
double sbx_context_duration_sec(const SbxContext *ctx);

/*
 * Set current render clock time in seconds.
 * This resets internal oscillator/effect phase/state and restarts playback
 * from the requested timeline time.
 */
int sbx_context_set_time_sec(SbxContext *ctx, double t_sec);

/*
 * Sample evaluated tone values over [t0_sec, t1_sec].
 * - sample_count must be >= 1.
 * - out_tones must have sample_count elements.
 * - out_t_sec is optional (may be NULL).
 * - Times are interpreted in context time domain; looped keyframe programs
 *   are wrapped to keyframe duration when sampled.
 */
int sbx_context_sample_tones(SbxContext *ctx,
                             double t0_sec,
                             double t1_sec,
                             size_t sample_count,
                             double *out_t_sec,
                             SbxToneSpec *out_tones);

/*
 * Sample one specific voice lane over [t0_sec, t1_sec].
 * - voice_index 0 is the primary lane.
 * - secondary lanes are available for multivoice native `.sbg` content.
 */
int sbx_context_sample_tones_voice(SbxContext *ctx,
                                   size_t voice_index,
                                   double t0_sec,
                                   double t1_sec,
                                   size_t sample_count,
                                   double *out_t_sec,
                                   SbxToneSpec *out_tones);

/*
 * Sample effective program beat/pulse frequency over [t0_sec, t1_sec].
 * - sample_count must be >= 1.
 * - out_hz must have sample_count elements.
 * - out_t_sec is optional (may be NULL).
 * - Looping keyframed programs are wrapped to program duration when sampled.
 */
int sbx_context_sample_program_beat(SbxContext *ctx,
                                    double t0_sec,
                                    double t1_sec,
                                    size_t sample_count,
                                    double *out_t_sec,
                                    double *out_hz);

/*
 * Sample effective beat/pulse frequency for one specific voice lane.
 * - voice_index 0 is the primary lane.
 * - secondary lanes are available for multivoice native `.sbg` content.
 */
int sbx_context_sample_program_beat_voice(SbxContext *ctx,
                                          size_t voice_index,
                                          double t0_sec,
                                          double t1_sec,
                                          size_t sample_count,
                                          double *out_t_sec,
                                          double *out_hz);

/*
 * Sample one mixam cycle for plotting/inspection.
 * - fx must be an SBX_MIXFX_AM spec with valid mixam fields.
 * - rate_hz controls the cycle duration on the time axis.
 * - at least one of out_envelope/out_gain must be non-NULL.
 * - out_t_sec is optional (may be NULL).
 */
int sbx_sample_mixam_cycle(const SbxMixFxSpec *fx,
                           double rate_hz,
                           size_t sample_count,
                           double *out_t_sec,
                           double *out_envelope,
                           double *out_gain);

/*
 * Sample the built-in exponential drop beat/pulse curve used by `-p drop`.
 * Times are in seconds. out_t_sec is optional (may be NULL).
 */
int sbx_sample_drop_curve(double drop_sec,
                          double beat_start_hz,
                          double beat_target_hz,
                          int slide,
                          int n_step,
                          int step_len_sec,
                          size_t sample_count,
                          double *out_t_sec,
                          double *out_hz);

/*
 * Sample the built-in sigmoid beat/pulse curve used by `-p sigmoid`.
 * Times are in seconds. out_t_sec is optional (may be NULL).
 */
int sbx_sample_sigmoid_curve(double drop_sec,
                             double beat_start_hz,
                             double beat_target_hz,
                             double sig_l,
                             double sig_h,
                             double sig_a,
                             double sig_b,
                             size_t sample_count,
                             double *out_t_sec,
                             double *out_hz);

typedef enum {
  SBX_PROGRAM_PLOT_DROP = 0,
  SBX_PROGRAM_PLOT_SIGMOID = 1,
  SBX_PROGRAM_PLOT_CURVE = 2
} SbxProgramPlotKind;

typedef struct {
  double x_min;
  double x_max;
  double y_min;
  double y_max;
  int x_tick_count;
  int y_tick_count;
  double x_ticks[SBX_PLOT_MAX_TICKS];
  double y_ticks[SBX_PLOT_MAX_TICKS];
  char title[SBX_PLOT_TEXT_MAX];
  char x_label[32];
  char y_label[32];
  char line1[SBX_PLOT_TEXT_MAX];
  char line2[SBX_PLOT_TEXT_MAX];
} SbxProgramPlotDesc;

typedef struct {
  double x_min;
  double x_max;
  double top_y_min;
  double top_y_max;
  double bottom_y_min;
  double bottom_y_max;
  int x_tick_count;
  int top_y_tick_count;
  int bottom_y_tick_count;
  double x_ticks[SBX_PLOT_MAX_TICKS];
  double top_y_ticks[SBX_PLOT_MAX_TICKS];
  double bottom_y_ticks[SBX_PLOT_MAX_TICKS];
  char title[SBX_PLOT_TEXT_MAX];
  char x_label[32];
  char top_y_label[32];
  char bottom_y_label[32];
  char line1[SBX_PLOT_TEXT_MAX];
  char line2[SBX_PLOT_TEXT_MAX];
} SbxDualPanelPlotDesc;

void sbx_default_program_plot_desc(SbxProgramPlotDesc *desc);
void sbx_default_dual_panel_plot_desc(SbxDualPanelPlotDesc *desc);

/*
 * Build plot metadata for built-in drop/sigmoid/custom-curve program graphs.
 * - samples should contain the already-sampled beat/pulse series over [0, drop_sec].
 * - mode_kind: 0 binaural beat, 1 pulse, 2 monaural beat.
 * - sigmoid coefficients are used only for SBX_PROGRAM_PLOT_SIGMOID.
 */
int sbx_build_program_plot_desc(SbxProgramPlotKind plot_kind,
                                double drop_sec,
                                double beat_start_hz,
                                double beat_target_hz,
                                int mode_kind,
                                int slide,
                                int n_step,
                                int step_len_sec,
                                double sig_l,
                                double sig_h,
                                double sig_a,
                                double sig_b,
                                const double *samples,
                                size_t sample_count,
                                SbxProgramPlotDesc *out_desc);

/*
 * Build plot metadata for the dual-panel mixam single-cycle plot.
 * - fx must be an SBX_MIXFX_AM spec with valid mixam fields.
 * - rate_hz controls the cycle duration shown on the time axis.
 */
int sbx_build_mixam_cycle_plot_desc(const SbxMixFxSpec *fx,
                                    double rate_hz,
                                    SbxDualPanelPlotDesc *out_desc);

/*
 * Sample one isochronic cycle for plotting/inspection.
 * - tone must be an isochronic tone with beat_hz > 0.
 * - if env is NULL, the tone's runtime envelope is used:
 *   start=tone->iso_start, duty=tone->duty_cycle, attack=tone->iso_attack,
 *   release=tone->iso_release, edge=tone->iso_edge_mode.
 * - at least one of out_envelope/out_wave must be non-NULL.
 * - out_t_sec is optional (may be NULL).
 */
int sbx_sample_isochronic_cycle(const SbxToneSpec *tone,
                                const SbxIsoEnvelopeSpec *env,
                                size_t sample_count,
                                double *out_t_sec,
                                double *out_envelope,
                                double *out_wave);

/*
 * Context-aware variant used when the tone references a context-owned custom
 * envelope (`waveNN` or `customNN`).
 */
int sbx_context_sample_isochronic_cycle(const SbxContext *ctx,
                                        const SbxToneSpec *tone,
                                        const SbxIsoEnvelopeSpec *env,
                                        size_t sample_count,
                                        double *out_t_sec,
                                        double *out_envelope,
                                        double *out_wave);

/* Query stored edge/smoothing mode for a context-owned waveNN/customNN envelope. */
int sbx_context_get_envelope_edge_mode(const SbxContext *ctx,
                                       int envelope_waveform,
                                       int *out_edge_mode);

/* Render interleaved stereo float frames from context source. */
int sbx_context_render_f32(SbxContext *ctx, float *out, size_t frames);

/* Current render clock time in seconds. */
double sbx_context_time_sec(const SbxContext *ctx);

/* Last context-local error text. */
const char *sbx_context_last_error(const SbxContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SBAGENXLIB_H */
