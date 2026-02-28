#include "sbagenxlib.h"
#include "sbagenxlib_dsp.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef SBAGENXLIB_VERSION
#define SBAGENXLIB_VERSION "dev"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SBX_TAU (2.0 * M_PI)
#define SBX_CTX_SRC_NONE 0
#define SBX_CTX_SRC_STATIC 1
#define SBX_CTX_SRC_KEYFRAMES 2
#define SBX_MAX_SBG_VOICES 16
#define SBX_MAX_SBG_MIXFX 8
#define SBX_CUSTOM_WAVE_COUNT 100
#define SBX_CUSTOM_WAVE_SAMPLES 4096
#define SBX_MIXBEAT_HILBERT_TAPS 31

typedef struct {
  SbxMixFxSpec spec;
  double phase;
  double mixbeat_hist[SBX_MIXBEAT_HILBERT_TAPS];
  int mixbeat_hist_pos;
} SbxMixFxState;

typedef struct SbxVoiceSetKeyframe SbxVoiceSetKeyframe;
typedef struct SbxMixFxKeyframe SbxMixFxKeyframe;

struct SbxEngine {
  SbxEngineConfig cfg;
  SbxToneSpec tone;
  double phase_l;
  double phase_r;
  double pulse_phase;
  unsigned int rng_state;
  double pink_l[7];
  double pink_r[7];
  double brown_l;
  double brown_r;
  double bell_env;
  int bell_tick;
  int bell_tick_period;
  const double *custom_waves[SBX_CUSTOM_WAVE_COUNT];
  char last_error[256];
};

struct SbxContext {
  SbxEngine *eng;
  int loaded;
  char last_error[256];
  int default_waveform;
  int source_mode;
  SbxProgramKeyframe *kfs;
  size_t kf_count;
  int kf_loop;
  size_t kf_seg;
  double kf_duration_sec;
  SbxProgramKeyframe *mv_kfs; /* optional multivoice keyframes: [voice][kf] */
  SbxEngine **mv_eng;         /* engines for voices 1..mv_voice_count-1 */
  size_t mv_voice_count;      /* number of active voice lanes in mv_kfs */
  double t_sec;
  SbxToneSpec *aux_tones;
  SbxEngine **aux_eng;
  size_t aux_count;
  float *aux_buf;
  size_t aux_buf_cap;
  SbxMixFxState *mix_fx;
  size_t mix_fx_count;
  SbxMixFxKeyframe *sbg_mix_fx_kf;
  size_t sbg_mix_fx_kf_count;
  size_t sbg_mix_fx_slots;
  size_t sbg_mix_fx_seg;
  SbxMixFxState *sbg_mix_fx_state;
  SbxMixAmpKeyframe *mix_kf;
  size_t mix_kf_count;
  size_t mix_kf_seg;
  double mix_default_amp_pct;
  double *custom_waves[SBX_CUSTOM_WAVE_COUNT];
};

#define SBX_MV_KF(ctx, voice_idx) ((ctx)->mv_kfs + ((voice_idx) * (ctx)->kf_count))

static void engine_wave_sample(int waveform, double phase, double *out_sample);
static double sbx_lerp(double a, double b, double u);
static int snprintf_checked(char *out, size_t out_sz, const char *fmt, ...);
static int sbx_voice_set_frame_append_tone(SbxVoiceSetKeyframe *frame, const SbxToneSpec *tone);
static int sbx_voice_set_frame_append_gap(SbxVoiceSetKeyframe *frame);
static int sbx_voice_set_frame_append_mix_fx(SbxVoiceSetKeyframe *frame, const SbxMixFxSpec *fx);
static int sbx_voice_set_frame_merge(SbxVoiceSetKeyframe *dst, const SbxVoiceSetKeyframe *src);
static int ctx_set_sbg_mix_effect_keyframes_internal(SbxContext *ctx,
                                                     const SbxMixFxKeyframe *kfs,
                                                     size_t kf_count,
                                                     size_t slot_count);

static void
set_last_error(SbxEngine *eng, const char *msg) {
  if (!eng) return;
  if (!msg) {
    eng->last_error[0] = 0;
    return;
  }
  snprintf(eng->last_error, sizeof(eng->last_error), "%s", msg);
}

static void
set_ctx_error(SbxContext *ctx, const char *msg) {
  if (!ctx) return;
  if (!msg) {
    ctx->last_error[0] = 0;
    return;
  }
  snprintf(ctx->last_error, sizeof(ctx->last_error), "%s", msg);
}

static unsigned int
sbx_rand_u32(SbxEngine *eng) {
  eng->rng_state = eng->rng_state * 1664525u + 1013904223u;
  return eng->rng_state;
}

static double
sbx_rand_signed_unit(SbxEngine *eng) {
  unsigned int v = sbx_rand_u32(eng);
  return ((double)v / 2147483648.0) - 1.0;
}

static double sbx_mixbeat_hilbert_coeff[SBX_MIXBEAT_HILBERT_TAPS];
static int sbx_mixbeat_hilbert_inited = 0;

static void
sbx_mixbeat_hilbert_init_once(void) {
  int k;
  int mid = SBX_MIXBEAT_HILBERT_TAPS / 2;
  double pi = M_PI;
  if (sbx_mixbeat_hilbert_inited) return;
  memset(sbx_mixbeat_hilbert_coeff, 0, sizeof(sbx_mixbeat_hilbert_coeff));
  for (k = 0; k < SBX_MIXBEAT_HILBERT_TAPS; k++) {
    int n = k - mid;
    double h = 0.0;
    double win;
    if (n != 0 && (n & 1)) h = 2.0 / (pi * (double)n);
    win = 0.54 - 0.46 * cos((2.0 * pi * k) / (SBX_MIXBEAT_HILBERT_TAPS - 1));
    sbx_mixbeat_hilbert_coeff[k] = h * win;
  }
  sbx_mixbeat_hilbert_inited = 1;
}

static void
sbx_mix_fx_reset_state(SbxMixFxState *fx) {
  if (!fx) return;
  fx->phase = 0.0;
  memset(fx->mixbeat_hist, 0, sizeof(fx->mixbeat_hist));
  fx->mixbeat_hist_pos = 0;
}

static double
sbx_mixbeat_hilbert_step(SbxMixFxState *fx, double x) {
  int k, idx;
  double q = 0.0;
  int pos;
  if (!fx) return 0.0;
  sbx_mixbeat_hilbert_init_once();
  pos = fx->mixbeat_hist_pos;
  fx->mixbeat_hist[pos] = x;
  idx = pos;
  for (k = 0; k < SBX_MIXBEAT_HILBERT_TAPS; k++) {
    q += sbx_mixbeat_hilbert_coeff[k] * fx->mixbeat_hist[idx];
    if (--idx < 0) idx = SBX_MIXBEAT_HILBERT_TAPS - 1;
  }
  pos++;
  if (pos >= SBX_MIXBEAT_HILBERT_TAPS) pos = 0;
  fx->mixbeat_hist_pos = pos;
  return q;
}

static double
sbx_wave_sample_unit_phase(int waveform, double phase_unit) {
  double wav;
  phase_unit = sbx_dsp_wrap_unit(phase_unit);
  engine_wave_sample(waveform, phase_unit * SBX_TAU, &wav);
  return wav;
}

static const char *
skip_ws(const char *p) {
  while (*p && isspace((unsigned char)*p)) p++;
  return p;
}

static int
streq_prefix_nocase(const char *s, const char *prefix) {
  while (*prefix) {
    if (tolower((unsigned char)*s++) != tolower((unsigned char)*prefix++))
      return 0;
  }
  return 1;
}

static int
sbx_custom_wave_index(int waveform) {
  if (waveform >= SBX_WAVE_CUSTOM_BASE &&
      waveform < SBX_WAVE_CUSTOM_BASE + SBX_CUSTOM_WAVE_COUNT)
    return waveform - SBX_WAVE_CUSTOM_BASE;
  return -1;
}

static int
sbx_parse_custom_wave_prefix(const char *spec, int *out_waveform, const char **out_after) {
  int idx;
  if (!spec || !out_after) return 0;
  if (strncasecmp(spec, "wave", 4) != 0) return 0;
  if (!isdigit((unsigned char)spec[4]) || !isdigit((unsigned char)spec[5]) || spec[6] != ':')
    return 0;
  idx = (spec[4] - '0') * 10 + (spec[5] - '0');
  if (out_waveform) *out_waveform = SBX_WAVE_CUSTOM_BASE + idx;
  *out_after = spec + 7;
  return 1;
}

static void
engine_set_custom_waves(SbxEngine *eng, double *const *waves) {
  size_t i;
  if (!eng) return;
  for (i = 0; i < SBX_CUSTOM_WAVE_COUNT; i++)
    eng->custom_waves[i] = waves ? waves[i] : 0;
}

static void
ctx_sync_custom_waves(SbxContext *ctx) {
  size_t i;
  if (!ctx || !ctx->eng) return;
  engine_set_custom_waves(ctx->eng, ctx->custom_waves);
  for (i = 0; i + 1 < ctx->mv_voice_count; i++) {
    if (ctx->mv_eng && ctx->mv_eng[i])
      engine_set_custom_waves(ctx->mv_eng[i], ctx->custom_waves);
  }
  for (i = 0; i < ctx->aux_count; i++) {
    if (ctx->aux_eng && ctx->aux_eng[i])
      engine_set_custom_waves(ctx->aux_eng[i], ctx->custom_waves);
  }
}

static void
ctx_clear_custom_waves(SbxContext *ctx) {
  size_t i;
  if (!ctx) return;
  for (i = 0; i < SBX_CUSTOM_WAVE_COUNT; i++) {
    if (ctx->custom_waves[i]) free(ctx->custom_waves[i]);
    ctx->custom_waves[i] = 0;
  }
  ctx_sync_custom_waves(ctx);
}

static void
ctx_replace_custom_waves(SbxContext *ctx, double **waves) {
  size_t i;
  if (!ctx || !waves) return;
  ctx_clear_custom_waves(ctx);
  for (i = 0; i < SBX_CUSTOM_WAVE_COUNT; i++) {
    ctx->custom_waves[i] = waves[i];
    waves[i] = 0;
  }
  ctx_sync_custom_waves(ctx);
}

static int
sbx_build_custom_wave_table_from_samples(const double *samples,
                                         size_t count,
                                         double **out_table) {
  double *tbl;
  double minv, maxv;
  size_t i;

  if (!samples || count < 2 || !out_table) return SBX_EINVAL;
  minv = maxv = samples[0];
  for (i = 1; i < count; i++) {
    if (samples[i] < minv) minv = samples[i];
    if (samples[i] > maxv) maxv = samples[i];
  }
  if (!(isfinite(minv) && isfinite(maxv)) || maxv <= minv)
    return SBX_EINVAL;

  tbl = (double *)calloc(SBX_CUSTOM_WAVE_SAMPLES, sizeof(*tbl));
  if (!tbl) return SBX_ENOMEM;
  for (i = 0; i < SBX_CUSTOM_WAVE_SAMPLES; i++) {
    double pos = ((double)i * (double)count) / (double)SBX_CUSTOM_WAVE_SAMPLES;
    size_t i0 = (size_t)floor(pos) % count;
    size_t i1 = (i0 + 1) % count;
    double frac = pos - floor(pos);
    double v0 = (samples[i0] - minv) / (maxv - minv);
    double v1 = (samples[i1] - minv) / (maxv - minv);
    tbl[i] = sbx_lerp(v0, v1, frac);
  }
  *out_table = tbl;
  return SBX_OK;
}

static const char *
skip_optional_waveform_prefix(const char *spec, int *out_waveform) {
  const char *p = spec;
  if (sbx_parse_custom_wave_prefix(p, out_waveform, &p))
    return p;
  if (streq_prefix_nocase(p, "sine:")) {
    if (out_waveform) *out_waveform = SBX_WAVE_SINE;
    return p + 5;
  }
  if (streq_prefix_nocase(p, "square:")) {
    if (out_waveform) *out_waveform = SBX_WAVE_SQUARE;
    return p + 7;
  }
  if (streq_prefix_nocase(p, "triangle:")) {
    if (out_waveform) *out_waveform = SBX_WAVE_TRIANGLE;
    return p + 9;
  }
  if (streq_prefix_nocase(p, "sawtooth:")) {
    if (out_waveform) *out_waveform = SBX_WAVE_SAWTOOTH;
    return p + 9;
  }
  return p;
}

static char *
sbx_strdup_local(const char *s) {
  size_t n;
  char *out;
  if (!s) return 0;
  n = strlen(s) + 1;
  out = (char *)malloc(n);
  if (!out) return 0;
  memcpy(out, s, n);
  return out;
}

static void
rstrip_inplace(char *s) {
  size_t n;
  if (!s) return;
  n = strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) {
    s[n - 1] = 0;
    n--;
  }
}

static void
strip_inline_comment(char *s) {
  char *p = s;
  if (!s) return;
  while (*p) {
    if (*p == '#') {
      *p = 0;
      return;
    }
    if (*p == ';') {
      *p = 0;
      return;
    }
    if (*p == '/' && p[1] == '/') {
      *p = 0;
      return;
    }
    p++;
  }
}

static int
parse_time_seconds_token(const char *tok, double *out_sec) {
  char *end = 0;
  double v;
  double mul = 1.0;
  const char *p;

  if (!tok || !*tok || !out_sec) return SBX_EINVAL;
  v = strtod(tok, &end);
  if (end == tok || !isfinite(v)) return SBX_EINVAL;
  p = skip_ws(end);

  if (*p == 0) mul = 1.0;          // default: seconds
  else if ((p[0] == 's' || p[0] == 'S') && p[1] == 0) mul = 1.0;
  else if ((p[0] == 'm' || p[0] == 'M') && p[1] == 0) mul = 60.0;
  else if ((p[0] == 'h' || p[0] == 'H') && p[1] == 0) mul = 3600.0;
  else return SBX_EINVAL;

  v *= mul;
  if (!isfinite(v) || v < 0.0) return SBX_EINVAL;
  *out_sec = v;
  return SBX_OK;
}

static void
sbx_mixam_set_defaults(SbxMixFxSpec *fx) {
  if (!fx) return;
  fx->mixam_start = 0.0;
  fx->mixam_duty = 0.5;
  fx->mixam_attack = 0.5;
  fx->mixam_release = 0.5;
  fx->mixam_edge_mode = 3;
  fx->mixam_floor = 0.45;
  fx->mixam_mode = SBX_MIXAM_MODE_COS;
  fx->mixam_bind_program_beat = 0;
}

static int
sbx_validate_mixam_fields(const SbxMixFxSpec *fx) {
  if (!fx) return SBX_EINVAL;
  if (!isfinite(fx->res)) return SBX_EINVAL;
  if (!fx->mixam_bind_program_beat && fx->res <= 0.0) return SBX_EINVAL;
  if (fx->mixam_bind_program_beat != 0 && fx->mixam_bind_program_beat != 1) return SBX_EINVAL;
  if (fx->mixam_mode != SBX_MIXAM_MODE_PULSE &&
      fx->mixam_mode != SBX_MIXAM_MODE_COS) return SBX_EINVAL;
  if (!isfinite(fx->mixam_start) || fx->mixam_start < 0.0 || fx->mixam_start > 1.0) return SBX_EINVAL;
  if (!isfinite(fx->mixam_floor) || fx->mixam_floor < 0.0 || fx->mixam_floor > 1.0) return SBX_EINVAL;
  if (fx->mixam_mode == SBX_MIXAM_MODE_PULSE) {
    if (!isfinite(fx->mixam_duty) || fx->mixam_duty < 0.0 || fx->mixam_duty > 1.0) return SBX_EINVAL;
    if (!isfinite(fx->mixam_attack) || fx->mixam_attack < 0.0 || fx->mixam_attack > 1.0) return SBX_EINVAL;
    if (!isfinite(fx->mixam_release) || fx->mixam_release < 0.0 || fx->mixam_release > 1.0) return SBX_EINVAL;
    if (fx->mixam_edge_mode < 0 || fx->mixam_edge_mode > 3) return SBX_EINVAL;
    if ((fx->mixam_attack + fx->mixam_release) > (1.0 + 1e-12)) return SBX_EINVAL;
  }
  return SBX_OK;
}

static int
sbx_parse_mixam_params(const char *params, SbxMixFxSpec *fx) {
  const char *p = params;
  int saw_mode = 0;
  int saw_pulse_shape = 0;
  if (!fx) return SBX_EINVAL;
  if (!p) return SBX_OK;

  while (*p) {
    char key;
    char *end = 0;
    double val;

    while (*p && (isspace((unsigned char)*p) || *p == ':')) p++;
    if (!*p) break;

    key = (char)tolower((unsigned char)*p++);
    if (*p != '=') return SBX_EINVAL;
    p++;
    if (key == 'm') {
      size_t mlen = 0;
      char mode_tok[16];
      while (p[mlen] && p[mlen] != ':' && !isspace((unsigned char)p[mlen])) mlen++;
      if (mlen == 0 || mlen >= sizeof(mode_tok)) return SBX_EINVAL;
      memcpy(mode_tok, p, mlen);
      mode_tok[mlen] = 0;
      if (strcasecmp(mode_tok, "pulse") == 0)
        fx->mixam_mode = SBX_MIXAM_MODE_PULSE;
      else if (strcasecmp(mode_tok, "cos") == 0)
        fx->mixam_mode = SBX_MIXAM_MODE_COS;
      else
        return SBX_EINVAL;
      saw_mode = 1;
      end = (char *)(p + mlen);
    } else {
      val = strtod(p, &end);
      if (end == p || !isfinite(val)) return SBX_EINVAL;

      switch (key) {
        case 's': fx->mixam_start = val; break;
        case 'd': fx->mixam_duty = val; saw_pulse_shape = 1; break;
        case 'a': fx->mixam_attack = val; saw_pulse_shape = 1; break;
        case 'r': fx->mixam_release = val; saw_pulse_shape = 1; break;
        case 'f': fx->mixam_floor = val; break;
        case 'e': {
          int mode = (int)val;
          if (fabs(val - (double)mode) > 1e-9) return SBX_EINVAL;
          fx->mixam_edge_mode = mode;
          saw_pulse_shape = 1;
          break;
        }
        default:
          return SBX_EINVAL;
      }
    }
    p = end;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == 0) break;
    if (*p != ':') return SBX_EINVAL;
  }

  if (!saw_mode && saw_pulse_shape)
    fx->mixam_mode = SBX_MIXAM_MODE_PULSE;

  return sbx_validate_mixam_fields(fx);
}

static int
parse_hhmmss_token(const char *tok, double *out_sec) {
  size_t used = 0;
  int rc;
  if (!tok || !*tok || !out_sec) return SBX_EINVAL;
  rc = sbx_parse_sbg_clock_token(tok, &used, out_sec);
  if (rc != SBX_OK) return rc;
  if (used != strlen(tok)) return SBX_EINVAL;
  return SBX_OK;
}

static int
parse_interp_mode_token(const char *tok, int *out_interp) {
  if (!tok || !*tok || !out_interp) return SBX_EINVAL;
  if (strcasecmp(tok, "linear") == 0 || strcasecmp(tok, "ramp") == 0) {
    *out_interp = SBX_INTERP_LINEAR;
    return SBX_OK;
  }
  if (strcasecmp(tok, "step") == 0 || strcasecmp(tok, "hold") == 0) {
    *out_interp = SBX_INTERP_STEP;
    return SBX_OK;
  }
  return SBX_EINVAL;
}

struct SbxVoiceSetKeyframe {
  double time_sec;
  SbxToneSpec tones[SBX_MAX_SBG_VOICES];
  size_t tone_len;
  int interp;
  int mix_amp_present;
  double mix_amp_pct;
  SbxMixFxSpec mix_fx[SBX_MAX_SBG_MIXFX];
  size_t mix_fx_count;
};

typedef struct {
  char *name;
  SbxVoiceSetKeyframe frame;
} SbxNamedToneDef;

typedef struct {
  char *name;
  SbxVoiceSetKeyframe *entries;
  size_t count;
  size_t cap;
} SbxNamedBlockDef;

struct SbxMixFxKeyframe {
  double time_sec;
  int interp;
  SbxMixFxSpec mix_fx[SBX_MAX_SBG_MIXFX];
  size_t mix_fx_count;
};

static int
is_sbg_transition_token(const char *tok) {
  if (!tok || !tok[0] || !tok[1] || tok[2]) return 0;
  return !!strchr("<-=>", tok[0]) && !!strchr("<-=>", tok[1]);
}

static int
sbg_transition_token_to_interp(const char *tok) {
  if (tok && (tok[0] == '=' || tok[1] == '='))
    return SBX_INTERP_STEP;
  return SBX_INTERP_LINEAR;
}

static int
named_tone_find(const SbxNamedToneDef *defs, size_t ndefs, const char *name) {
  size_t i;
  if (!defs || !name) return -1;
  for (i = 0; i < ndefs; i++) {
    if (defs[i].name && strcmp(defs[i].name, name) == 0)
      return (int)i;
  }
  return -1;
}

static int
named_block_find(const SbxNamedBlockDef *defs, size_t ndefs, const char *name) {
  size_t i;
  if (!defs || !name) return -1;
  for (i = 0; i < ndefs; i++) {
    if (defs[i].name && strcmp(defs[i].name, name) == 0)
      return (int)i;
  }
  return -1;
}

static int
sbx_frame_apply_token(SbxVoiceSetKeyframe *frame,
                      const char *tok,
                      int default_waveform,
                      const SbxNamedToneDef *defs,
                      size_t ndefs,
                      int *out_block_idx,
                      const SbxNamedBlockDef *blocks,
                      size_t nblocks) {
  int extra_type = SBX_EXTRA_INVALID;
  SbxToneSpec tone;
  SbxMixFxSpec fx;
  double mix_pct = 0.0;
  int didx;
  int bidx;
  int rc;

  if (!frame || !tok) return SBX_EINVAL;
  if (out_block_idx) *out_block_idx = -1;

  if (strcmp(tok, "-") == 0)
    return sbx_voice_set_frame_append_gap(frame);

  didx = named_tone_find(defs, ndefs, tok);
  if (didx >= 0)
    return sbx_voice_set_frame_merge(frame, &defs[didx].frame);

  bidx = named_block_find(blocks, nblocks, tok);
  if (bidx >= 0) {
    if (out_block_idx) {
      *out_block_idx = bidx;
      return SBX_OK;
    }
    return SBX_EINVAL;
  }

  rc = sbx_parse_extra_token(tok, default_waveform, &extra_type, &tone, &fx, &mix_pct);
  if (rc != SBX_OK) return rc;

  if (extra_type == SBX_EXTRA_TONE)
    return sbx_voice_set_frame_append_tone(frame, &tone);

  if (extra_type == SBX_EXTRA_MIXAMP) {
    if (frame->mix_amp_present) return SBX_EINVAL;
    frame->mix_amp_present = 1;
    frame->mix_amp_pct = mix_pct;
    return SBX_OK;
  }

  if (extra_type == SBX_EXTRA_MIXFX)
    return sbx_voice_set_frame_append_mix_fx(frame, &fx);

  return SBX_EINVAL;
}

static int
parse_sbg_relative_offset_token(const char *tok, double *out_sec) {
  const char *p;
  size_t used = 0;
  double seg = 0.0;
  double sum = 0.0;

  if (!tok || !*tok || !out_sec) return SBX_EINVAL;
  p = skip_ws(tok);
  if (*p != '+') return SBX_EINVAL;
  while (*p) {
    if (*p != '+') return SBX_EINVAL;
    p++;
    if (sbx_parse_sbg_clock_token(p, &used, &seg) != SBX_OK) return SBX_EINVAL;
    sum += seg;
    p += used;
  }
  if (!isfinite(sum) || sum < 0.0) return SBX_EINVAL;
  *out_sec = sum;
  return SBX_OK;
}

static int
split_ws_tokens_inplace(char *text, char ***out_tokv, size_t *out_count) {
  char **tokv = 0;
  size_t count = 0, cap = 0;
  char *p;

  if (!text || !out_tokv || !out_count) return SBX_EINVAL;

  p = (char *)skip_ws(text);
  while (*p) {
    char **tmp;
    char *q = p;
    if (count == cap) {
      size_t ncap = cap ? (cap * 2) : 8;
      tmp = (char **)realloc(tokv, ncap * sizeof(*tokv));
      if (!tmp) {
        if (tokv) free(tokv);
        return SBX_ENOMEM;
      }
      tokv = tmp;
      cap = ncap;
    }
    tokv[count++] = p;
    while (*q && !isspace((unsigned char)*q)) q++;
    if (*q) {
      *q++ = 0;
      p = (char *)skip_ws(q);
    } else {
      p = q;
    }
  }

  *out_tokv = tokv;
  *out_count = count;
  return SBX_OK;
}

static int
block_append_entry(SbxNamedBlockDef *blk,
                   const SbxVoiceSetKeyframe *entry) {
  SbxVoiceSetKeyframe *tmp;
  size_t ncap;
  if (!blk || !entry) return SBX_EINVAL;
  if (blk->count == blk->cap) {
    ncap = blk->cap ? (blk->cap * 2) : 8;
    tmp = (SbxVoiceSetKeyframe *)realloc(blk->entries, ncap * sizeof(*blk->entries));
    if (!tmp) return SBX_ENOMEM;
    blk->entries = tmp;
    blk->cap = ncap;
  }
  blk->entries[blk->count] = *entry;
  blk->count++;
  return SBX_OK;
}

static void
sbx_voice_set_init(SbxToneSpec *tones) {
  size_t i;
  if (!tones) return;
  for (i = 0; i < SBX_MAX_SBG_VOICES; i++) {
    sbx_default_tone_spec(&tones[i]);
    tones[i].mode = SBX_TONE_NONE;
    tones[i].amplitude = 0.0;
  }
}

static void
sbx_voice_set_frame_init(SbxVoiceSetKeyframe *frame) {
  if (!frame) return;
  memset(frame, 0, sizeof(*frame));
  sbx_voice_set_init(frame->tones);
  frame->interp = SBX_INTERP_LINEAR;
}

static int
sbx_voice_set_frame_append_tone(SbxVoiceSetKeyframe *frame, const SbxToneSpec *tone) {
  if (!frame || !tone) return SBX_EINVAL;
  if (frame->tone_len >= SBX_MAX_SBG_VOICES) return SBX_EINVAL;
  frame->tones[frame->tone_len] = *tone;
  frame->tone_len++;
  return SBX_OK;
}

static int
sbx_voice_set_frame_append_gap(SbxVoiceSetKeyframe *frame) {
  SbxToneSpec tone;
  if (!frame) return SBX_EINVAL;
  sbx_default_tone_spec(&tone);
  tone.mode = SBX_TONE_NONE;
  tone.amplitude = 0.0;
  return sbx_voice_set_frame_append_tone(frame, &tone);
}

static int
sbx_voice_set_frame_append_mix_fx(SbxVoiceSetKeyframe *frame, const SbxMixFxSpec *fx) {
  if (!frame || !fx) return SBX_EINVAL;
  if (frame->mix_fx_count >= SBX_MAX_SBG_MIXFX) return SBX_EINVAL;
  frame->mix_fx[frame->mix_fx_count++] = *fx;
  return SBX_OK;
}

static int
sbx_voice_set_frame_merge(SbxVoiceSetKeyframe *dst, const SbxVoiceSetKeyframe *src) {
  size_t i;
  if (!dst || !src) return SBX_EINVAL;
  if (dst->tone_len + src->tone_len > SBX_MAX_SBG_VOICES) return SBX_EINVAL;
  if (dst->mix_fx_count + src->mix_fx_count > SBX_MAX_SBG_MIXFX) return SBX_EINVAL;
  for (i = 0; i < src->tone_len; i++)
    dst->tones[dst->tone_len + i] = src->tones[i];
  dst->tone_len += src->tone_len;
  if (src->mix_amp_present) {
    if (dst->mix_amp_present) return SBX_EINVAL;
    dst->mix_amp_present = 1;
    dst->mix_amp_pct = src->mix_amp_pct;
  }
  for (i = 0; i < src->mix_fx_count; i++)
    dst->mix_fx[dst->mix_fx_count + i] = src->mix_fx[i];
  dst->mix_fx_count += src->mix_fx_count;
  return SBX_OK;
}

static int
parse_sbg_timeline_time_token(const char *tok,
                              double *inout_last_abs_sec,
                              double *out_sec) {
  const char *p;
  double tim = -1.0;
  double seg = 0.0;
  size_t used = 0;
  const double day_sec = 24.0 * 60.0 * 60.0;

  if (!tok || !inout_last_abs_sec || !out_sec) return SBX_EINVAL;
  p = skip_ws(tok);
  if (!*p) return SBX_EINVAL;

  if (strncasecmp(p, "NOW", 3) == 0) {
    tim = 0.0;
    p += 3;
  }

  while (*p) {
    if (*p == '+') {
      p++;
      if (sbx_parse_sbg_clock_token(p, &used, &seg) != SBX_OK)
        return SBX_EINVAL;
      if (tim < 0.0) {
        if (*inout_last_abs_sec < 0.0)
          return SBX_EINVAL;
        tim = *inout_last_abs_sec;
      }
      tim += seg;
      p += used;
      continue;
    }

    if (tim < 0.0) {
      if (sbx_parse_sbg_clock_token(p, &used, &seg) != SBX_OK)
        return SBX_EINVAL;
      tim = seg;
      if (*inout_last_abs_sec >= 0.0) {
        while (tim < *inout_last_abs_sec)
          tim += day_sec;
      }
      p += used;
      continue;
    }

    return SBX_EINVAL;
  }

  if (tim < 0.0) return SBX_EINVAL;
  *inout_last_abs_sec = tim;
  *out_sec = tim;
  return SBX_OK;
}

static int
read_text_file_alloc(const char *path, char **out_text) {
  FILE *fp = 0;
  char chunk[4096];
  char *text = 0;
  size_t len = 0, cap = 0;

  if (!path || !out_text) return SBX_EINVAL;
  *out_text = 0;

  fp = fopen(path, "rb");
  if (!fp) return SBX_EINVAL;

  for (;;) {
    size_t n = fread(chunk, 1, sizeof(chunk), fp);
    if (n > 0) {
      char *tmp;
      size_t need = len + n + 1;
      if (need > cap) {
        size_t ncap = cap ? cap : 4096;
        while (ncap < need) ncap *= 2;
        tmp = (char *)realloc(text, ncap);
        if (!tmp) {
          if (text) free(text);
          fclose(fp);
          return SBX_ENOMEM;
        }
        text = tmp;
        cap = ncap;
      }
      memcpy(text + len, chunk, n);
      len += n;
      text[len] = 0;
    }
    if (n < sizeof(chunk)) {
      if (ferror(fp)) {
        if (text) free(text);
        fclose(fp);
        return SBX_EINVAL;
      }
      break;
    }
  }

  fclose(fp);
  if (!text) {
    text = (char *)malloc(1);
    if (!text) return SBX_ENOMEM;
    text[0] = 0;
  }

  *out_text = text;
  return SBX_OK;
}

static int
normalize_tone(SbxToneSpec *tone, char *err, size_t err_sz) {
  int uses_carrier;
  int strict_carrier_positive;
  if (!tone) return SBX_EINVAL;
  if (err && err_sz) err[0] = 0;

  if (tone->mode < SBX_TONE_NONE || tone->mode > SBX_TONE_BELL) {
    if (err && err_sz) snprintf(err, err_sz, "%s", "unsupported tone mode");
    return SBX_EINVAL;
  }

  uses_carrier = (tone->mode == SBX_TONE_BINAURAL ||
                  tone->mode == SBX_TONE_MONAURAL ||
                  tone->mode == SBX_TONE_ISOCHRONIC ||
                  tone->mode == SBX_TONE_BELL ||
                  tone->mode == SBX_TONE_SPIN_PINK ||
                  tone->mode == SBX_TONE_SPIN_BROWN ||
                  tone->mode == SBX_TONE_SPIN_WHITE);
  strict_carrier_positive = (tone->mode == SBX_TONE_BINAURAL ||
                             tone->mode == SBX_TONE_MONAURAL ||
                             tone->mode == SBX_TONE_ISOCHRONIC ||
                             tone->mode == SBX_TONE_BELL);

  if (uses_carrier) {
    int custom_idx = sbx_custom_wave_index(tone->waveform);
    if (custom_idx >= 0) {
      if (tone->mode != SBX_TONE_BINAURAL) {
        if (err && err_sz) snprintf(err, err_sz, "%s", "custom waveNN envelopes are only valid for binaural tones");
        return SBX_EINVAL;
      }
    } else if (tone->waveform < SBX_WAVE_SINE || tone->waveform > SBX_WAVE_SAWTOOTH) {
      if (err && err_sz) snprintf(err, err_sz, "%s", "waveform must be a valid SBX_WAVE_* value");
      return SBX_EINVAL;
    }
  } else {
    tone->waveform = SBX_WAVE_SINE;
  }

  if (tone->mode != SBX_TONE_NONE) {
    if (!isfinite(tone->carrier_hz) || (strict_carrier_positive && tone->carrier_hz <= 0.0)) {
      if (uses_carrier) {
        if (err && err_sz) {
          if (strict_carrier_positive)
            snprintf(err, err_sz, "%s", "carrier_hz must be finite and > 0");
          else
            snprintf(err, err_sz, "%s", "carrier_hz must be finite");
        }
        return SBX_EINVAL;
      }
    }
    if (!isfinite(tone->beat_hz)) {
      if (uses_carrier) {
        if (err && err_sz) snprintf(err, err_sz, "%s", "beat_hz must be finite");
        return SBX_EINVAL;
      }
    }
    if (!isfinite(tone->amplitude)) {
      if (err && err_sz) snprintf(err, err_sz, "%s", "amplitude must be finite");
      return SBX_EINVAL;
    }
    tone->amplitude = sbx_dsp_clamp(tone->amplitude, 0.0, 1.0);
  }

  if (tone->mode == SBX_TONE_MONAURAL || tone->mode == SBX_TONE_ISOCHRONIC)
    tone->beat_hz = fabs(tone->beat_hz);
  if (tone->mode == SBX_TONE_BELL)
    tone->beat_hz = 0.0;

  if (tone->mode == SBX_TONE_ISOCHRONIC) {
    if (tone->beat_hz <= 0.0) {
      if (err && err_sz) snprintf(err, err_sz, "%s", "isochronic beat_hz must be > 0");
      return SBX_EINVAL;
    }
    if (!isfinite(tone->duty_cycle)) {
      if (err && err_sz) snprintf(err, err_sz, "%s", "duty_cycle must be finite");
      return SBX_EINVAL;
    }
    tone->duty_cycle = sbx_dsp_clamp(tone->duty_cycle, 0.01, 1.0);
  }

  return SBX_OK;
}

static int
engine_apply_tone(SbxEngine *eng, const SbxToneSpec *tone_in, int reset_phase) {
  SbxToneSpec tone;
  SbxToneMode prev_mode;
  char err[160];
  int rc;

  if (!eng || !tone_in) return SBX_EINVAL;
  prev_mode = eng->tone.mode;
  tone = *tone_in;
  rc = normalize_tone(&tone, err, sizeof(err));
  if (rc != SBX_OK) {
    set_last_error(eng, err);
    return rc;
  }
  if (sbx_custom_wave_index(tone.waveform) >= 0 &&
      !eng->custom_waves[sbx_custom_wave_index(tone.waveform)]) {
    set_last_error(eng, "custom waveNN envelope is not defined in this context");
    return SBX_EINVAL;
  }

  eng->tone = tone;
  if (reset_phase)
    sbx_engine_reset(eng);

  if (eng->tone.mode == SBX_TONE_BELL &&
      (reset_phase || prev_mode != SBX_TONE_BELL)) {
    eng->bell_env = eng->tone.amplitude;
    eng->bell_tick = 0;
    eng->bell_tick_period = (int)(eng->cfg.sample_rate / 20.0);
    if (eng->bell_tick_period < 1) eng->bell_tick_period = 1;
  } else if (eng->tone.mode != SBX_TONE_BELL && prev_mode == SBX_TONE_BELL) {
    eng->bell_env = 0.0;
    eng->bell_tick = 0;
  }

  set_last_error(eng, NULL);
  return SBX_OK;
}

static void
engine_wave_sample(int waveform, double phase, double *out_sample) {
  double u;
  if (!out_sample) return;
  switch (waveform) {
    case SBX_WAVE_SQUARE:
      u = sbx_dsp_wrap_cycle(phase, SBX_TAU) / SBX_TAU;
      *out_sample = (u < 0.5) ? 1.0 : -1.0;
      return;
    case SBX_WAVE_TRIANGLE:
      u = sbx_dsp_wrap_cycle(phase, SBX_TAU) / SBX_TAU;
      if (u < 0.25) *out_sample = 4.0 * u;
      else if (u < 0.75) *out_sample = 2.0 - 4.0 * u;
      else *out_sample = -4.0 + 4.0 * u;
      return;
    case SBX_WAVE_SAWTOOTH:
      u = sbx_dsp_wrap_cycle(phase, SBX_TAU) / SBX_TAU;
      *out_sample = -1.0 + 2.0 * u;
      return;
    case SBX_WAVE_SINE:
    default:
      *out_sample = sin(phase);
      return;
  }
}

static int
engine_custom_env_sample(const SbxEngine *eng, int waveform, double phase_unit, double *out_env) {
  const double *tbl;
  int custom_idx = sbx_custom_wave_index(waveform);
  size_t i0, i1;
  double pos, frac;
  if (!out_env) return 0;
  if (custom_idx < 0) return 0;
  if (!eng || !eng->custom_waves[custom_idx]) return -1;
  tbl = eng->custom_waves[custom_idx];
  phase_unit = sbx_dsp_wrap_unit(phase_unit);
  pos = phase_unit * (double)SBX_CUSTOM_WAVE_SAMPLES;
  i0 = (size_t)floor(pos) % SBX_CUSTOM_WAVE_SAMPLES;
  i1 = (i0 + 1) % SBX_CUSTOM_WAVE_SAMPLES;
  frac = pos - floor(pos);
  *out_env = sbx_lerp(tbl[i0], tbl[i1], frac);
  return 1;
}

static double
engine_next_white(SbxEngine *eng) {
  return sbx_rand_signed_unit(eng);
}

static double
engine_next_pink_from_state(SbxEngine *eng, double state[7]) {
  double w = sbx_rand_signed_unit(eng);
  double p;

  state[0] = 0.99886 * state[0] + 0.0555179 * w;
  state[1] = 0.99332 * state[1] + 0.0750759 * w;
  state[2] = 0.96900 * state[2] + 0.1538520 * w;
  state[3] = 0.86650 * state[3] + 0.3104856 * w;
  state[4] = 0.55000 * state[4] + 0.5329522 * w;
  state[5] = -0.7616 * state[5] - 0.0168980 * w;
  p = state[0] + state[1] + state[2] + state[3] + state[4] + state[5] + state[6] + 0.5362 * w;
  state[6] = 0.115926 * w;

  return 0.11 * p;
}

static double
engine_next_brown_from_state(SbxEngine *eng, double *state) {
  double w = sbx_rand_signed_unit(eng);
  *state += 0.02 * w;
  if (*state > 1.0) *state = 1.0;
  if (*state < -1.0) *state = -1.0;
  return *state;
}

static double
engine_next_noise_mono_for_mode(SbxEngine *eng, SbxToneMode mode) {
  switch (mode) {
    case SBX_TONE_SPIN_WHITE:
    case SBX_TONE_WHITE_NOISE:
      return engine_next_white(eng);
    case SBX_TONE_SPIN_BROWN:
    case SBX_TONE_BROWN_NOISE:
      return engine_next_brown_from_state(eng, &eng->brown_l);
    case SBX_TONE_SPIN_PINK:
    case SBX_TONE_PINK_NOISE:
    default:
      return engine_next_pink_from_state(eng, eng->pink_l);
  }
}

static void
engine_render_sample(SbxEngine *eng, float *out_l, float *out_r) {
  double sr = eng->cfg.sample_rate;
  double amp = eng->tone.amplitude;
  double left = 0.0, right = 0.0;

  if (eng->tone.mode == SBX_TONE_BINAURAL) {
    double f_l = eng->tone.carrier_hz + eng->tone.beat_hz * 0.5;
    double f_r = eng->tone.carrier_hz - eng->tone.beat_hz * 0.5;
    double env = 1.0;
    int custom_rc = engine_custom_env_sample(eng, eng->tone.waveform, eng->pulse_phase, &env);
    if (custom_rc == 1) {
      engine_wave_sample(SBX_WAVE_SINE, eng->phase_l, &left);
      engine_wave_sample(SBX_WAVE_SINE, eng->phase_r, &right);
      left *= amp * env;
      right *= amp * env;
      eng->pulse_phase += fabs(eng->tone.beat_hz) / sr;
      while (eng->pulse_phase >= 1.0) eng->pulse_phase -= 1.0;
    } else {
      engine_wave_sample(eng->tone.waveform, eng->phase_l, &left);
      engine_wave_sample(eng->tone.waveform, eng->phase_r, &right);
      left *= amp;
      right *= amp;
    }
    eng->phase_l = sbx_dsp_wrap_cycle(eng->phase_l + SBX_TAU * f_l / sr, SBX_TAU);
    eng->phase_r = sbx_dsp_wrap_cycle(eng->phase_r + SBX_TAU * f_r / sr, SBX_TAU);
  } else if (eng->tone.mode == SBX_TONE_MONAURAL) {
    double f1 = eng->tone.carrier_hz - eng->tone.beat_hz * 0.5;
    double f2 = eng->tone.carrier_hz + eng->tone.beat_hz * 0.5;
    double s1 = 0.0, s2 = 0.0;
    double mono;
    engine_wave_sample(eng->tone.waveform, eng->phase_l, &s1);
    engine_wave_sample(eng->tone.waveform, eng->phase_r, &s2);
    mono = 0.5 * amp * (s1 + s2);
    left = mono;
    right = mono;
    eng->phase_l = sbx_dsp_wrap_cycle(eng->phase_l + SBX_TAU * f1 / sr, SBX_TAU);
    eng->phase_r = sbx_dsp_wrap_cycle(eng->phase_r + SBX_TAU * f2 / sr, SBX_TAU);
  } else if (eng->tone.mode == SBX_TONE_ISOCHRONIC) {
    double env = 0.0;
    double duty = eng->tone.duty_cycle;
    double pos;
    double carrier;

    engine_wave_sample(eng->tone.waveform, eng->phase_l, &carrier);
    eng->phase_l = sbx_dsp_wrap_cycle(eng->phase_l + SBX_TAU * eng->tone.carrier_hz / sr, SBX_TAU);

    eng->pulse_phase += eng->tone.beat_hz / sr;
    while (eng->pulse_phase >= 1.0) eng->pulse_phase -= 1.0;
    pos = eng->pulse_phase;

    env = sbx_dsp_iso_mod_factor_custom(pos, 0.0, duty, 0.15, 0.15, 2);

    left = right = amp * env * carrier;
  } else if (eng->tone.mode == SBX_TONE_BELL) {
    double bell_wave = 0.0;
    if (eng->bell_env > 0.0) {
      engine_wave_sample(eng->tone.waveform, eng->phase_l, &bell_wave);
      left = right = bell_wave * eng->bell_env;
      eng->phase_l = sbx_dsp_wrap_cycle(eng->phase_l + SBX_TAU * eng->tone.carrier_hz / sr, SBX_TAU);

      eng->bell_tick++;
      if (eng->bell_tick >= eng->bell_tick_period) {
        eng->bell_tick = 0;
        eng->bell_env = eng->bell_env * (11.0 / 12.0) - (1.0 / 4096.0);
        if (eng->bell_env < 0.0) eng->bell_env = 0.0;
      }
    }
  } else if (eng->tone.mode == SBX_TONE_WHITE_NOISE) {
    left = amp * engine_next_white(eng);
    right = amp * engine_next_white(eng);
  } else if (eng->tone.mode == SBX_TONE_PINK_NOISE) {
    left = amp * engine_next_pink_from_state(eng, eng->pink_l);
    right = amp * engine_next_pink_from_state(eng, eng->pink_r);
  } else if (eng->tone.mode == SBX_TONE_BROWN_NOISE) {
    left = amp * engine_next_brown_from_state(eng, &eng->brown_l);
    right = amp * engine_next_brown_from_state(eng, &eng->brown_r);
  } else if (eng->tone.mode == SBX_TONE_SPIN_PINK ||
             eng->tone.mode == SBX_TONE_SPIN_BROWN ||
             eng->tone.mode == SBX_TONE_SPIN_WHITE) {
    double base_noise;
    double spin_mod;
    double spin_pos;
    double spin;
    double g_l, g_r;

    base_noise = engine_next_noise_mono_for_mode(eng, eng->tone.mode);
    engine_wave_sample(eng->tone.waveform, eng->phase_l, &spin_mod);
    eng->phase_l = sbx_dsp_wrap_cycle(eng->phase_l + SBX_TAU * eng->tone.beat_hz / sr, SBX_TAU);

    // Width is interpreted in microseconds (legacy spin semantics).
    spin = eng->tone.carrier_hz * 1.0e-6 * sr * spin_mod;
    spin = sbx_dsp_clamp(spin * 1.5, -1.0, 1.0);
    spin_pos = fabs(spin);

    if (spin >= 0.0) {
      g_l = 1.0 - spin_pos;
      g_r = 1.0 + spin_pos;
    } else {
      g_l = 1.0 + spin_pos;
      g_r = 1.0 - spin_pos;
    }

    left = amp * base_noise * g_l;
    right = amp * base_noise * g_r;
  }

  *out_l = (float)left;
  *out_r = (float)right;
}

static double
sbx_lerp(double a, double b, double u) {
  return a + (b - a) * u;
}

static void
ctx_clear_keyframes(SbxContext *ctx) {
  size_t i;
  if (!ctx) return;
  if (ctx->kfs) free(ctx->kfs);
  if (ctx->mv_kfs) free(ctx->mv_kfs);
  if (ctx->mv_eng) {
    for (i = 0; i + 1 < ctx->mv_voice_count; i++) {
      if (ctx->mv_eng[i]) sbx_engine_destroy(ctx->mv_eng[i]);
    }
    free(ctx->mv_eng);
  }
  ctx->kfs = 0;
  ctx->mv_kfs = 0;
  ctx->mv_eng = 0;
  ctx->kf_count = 0;
  ctx->mv_voice_count = 0;
  ctx->kf_loop = 0;
  ctx->kf_seg = 0;
  ctx->kf_duration_sec = 0.0;
}

static void
ctx_clear_aux_tones(SbxContext *ctx) {
  size_t i;
  if (!ctx) return;
  if (ctx->aux_eng) {
    for (i = 0; i < ctx->aux_count; i++) {
      if (ctx->aux_eng[i]) sbx_engine_destroy(ctx->aux_eng[i]);
    }
    free(ctx->aux_eng);
  }
  if (ctx->aux_tones) free(ctx->aux_tones);
  if (ctx->aux_buf) free(ctx->aux_buf);
  ctx->aux_eng = 0;
  ctx->aux_tones = 0;
  ctx->aux_buf = 0;
  ctx->aux_count = 0;
  ctx->aux_buf_cap = 0;
}

static void
ctx_clear_mix_effects(SbxContext *ctx) {
  if (!ctx) return;
  if (ctx->mix_fx) free(ctx->mix_fx);
  ctx->mix_fx = 0;
  ctx->mix_fx_count = 0;
}

static void
ctx_clear_sbg_mix_effect_keyframes(SbxContext *ctx) {
  if (!ctx) return;
  if (ctx->sbg_mix_fx_kf) free(ctx->sbg_mix_fx_kf);
  if (ctx->sbg_mix_fx_state) free(ctx->sbg_mix_fx_state);
  ctx->sbg_mix_fx_kf = 0;
  ctx->sbg_mix_fx_kf_count = 0;
  ctx->sbg_mix_fx_slots = 0;
  ctx->sbg_mix_fx_seg = 0;
  ctx->sbg_mix_fx_state = 0;
}

static void
ctx_clear_mix_keyframes(SbxContext *ctx) {
  if (!ctx) return;
  if (ctx->mix_kf) free(ctx->mix_kf);
  ctx->mix_kf = 0;
  ctx->mix_kf_count = 0;
  ctx->mix_kf_seg = 0;
}

static void
ctx_reset_runtime(SbxContext *ctx) {
  size_t i;
  if (!ctx || !ctx->eng) return;
  sbx_engine_reset(ctx->eng);
  for (i = 0; i + 1 < ctx->mv_voice_count; i++) {
    if (ctx->mv_eng[i]) sbx_engine_reset(ctx->mv_eng[i]);
  }
  for (i = 0; i < ctx->aux_count; i++) {
    if (ctx->aux_eng[i]) sbx_engine_reset(ctx->aux_eng[i]);
  }
  for (i = 0; i < ctx->mix_fx_count; i++) {
    sbx_mix_fx_reset_state(&ctx->mix_fx[i]);
  }
  if (ctx->sbg_mix_fx_state) {
    for (i = 0; i < ctx->sbg_mix_fx_slots; i++)
      sbx_mix_fx_reset_state(&ctx->sbg_mix_fx_state[i]);
  }
  ctx->sbg_mix_fx_seg = 0;
  ctx->mix_kf_seg = 0;
  ctx->t_sec = 0.0;
  ctx->kf_seg = 0;
}

static int
ctx_activate_keyframes_internal(SbxContext *ctx,
                                const SbxProgramKeyframe *frames,
                                size_t frame_count,
                                const SbxProgramKeyframe *mv_frames,
                                size_t mv_voice_count,
                                int loop) {
  SbxProgramKeyframe *copy = 0;
  SbxProgramKeyframe *mv_copy = 0;
  SbxEngine **mv_eng = 0;
  size_t i, vi;
  char err[160];
  int rc;

  if (!ctx || !ctx->eng || !frames || frame_count == 0) return SBX_EINVAL;
  if (mv_voice_count == 0 || mv_voice_count > SBX_MAX_SBG_VOICES)
    return SBX_EINVAL;
  if (mv_voice_count > 1 && !mv_frames) return SBX_EINVAL;

  copy = (SbxProgramKeyframe *)calloc(frame_count, sizeof(*copy));
  if (!copy) {
    set_ctx_error(ctx, "out of memory");
    return SBX_ENOMEM;
  }

  for (i = 0; i < frame_count; i++) {
    copy[i] = frames[i];
    if (!isfinite(copy[i].time_sec) || copy[i].time_sec < 0.0) {
      free(copy);
      set_ctx_error(ctx, "keyframe time_sec must be finite and >= 0");
      return SBX_EINVAL;
    }
    if (i > 0 && !(copy[i].time_sec > copy[i - 1].time_sec)) {
      free(copy);
      set_ctx_error(ctx, "keyframe time_sec values must be strictly increasing");
      return SBX_EINVAL;
    }
    if (copy[i].interp != SBX_INTERP_LINEAR &&
        copy[i].interp != SBX_INTERP_STEP) {
      free(copy);
      set_ctx_error(ctx, "keyframe interp must be SBX_INTERP_LINEAR or SBX_INTERP_STEP");
      return SBX_EINVAL;
    }
    rc = normalize_tone(&copy[i].tone, err, sizeof(err));
    if (rc != SBX_OK) {
      free(copy);
      set_ctx_error(ctx, err);
      return rc;
    }
  }

  if (mv_voice_count > 1) {
    mv_copy = (SbxProgramKeyframe *)calloc(mv_voice_count * frame_count, sizeof(*mv_copy));
    if (!mv_copy) {
      free(copy);
      set_ctx_error(ctx, "out of memory");
      return SBX_ENOMEM;
    }
    for (vi = 0; vi < mv_voice_count; vi++) {
      for (i = 0; i < frame_count; i++) {
        mv_copy[vi * frame_count + i] = mv_frames[vi * frame_count + i];
        if (fabs(mv_copy[vi * frame_count + i].time_sec - copy[i].time_sec) > 1e-9) {
          free(mv_copy);
          free(copy);
          set_ctx_error(ctx, "multivoice keyframe lanes must share identical time positions");
          return SBX_EINVAL;
        }
        if (mv_copy[vi * frame_count + i].interp != copy[i].interp) {
          free(mv_copy);
          free(copy);
          set_ctx_error(ctx, "multivoice keyframe lanes must share identical interpolation");
          return SBX_EINVAL;
        }
        rc = normalize_tone(&mv_copy[vi * frame_count + i].tone, err, sizeof(err));
        if (rc != SBX_OK) {
          free(mv_copy);
          free(copy);
          set_ctx_error(ctx, err);
          return rc;
        }
      }
    }

    mv_eng = (SbxEngine **)calloc(mv_voice_count - 1, sizeof(*mv_eng));
    if (!mv_eng) {
      free(mv_copy);
      free(copy);
      set_ctx_error(ctx, "out of memory");
      return SBX_ENOMEM;
    }
    for (vi = 1; vi < mv_voice_count; vi++) {
      mv_eng[vi - 1] = sbx_engine_create(&ctx->eng->cfg);
      if (!mv_eng[vi - 1]) {
        size_t dj;
        for (dj = 1; dj < vi; dj++) {
          if (mv_eng[dj - 1]) sbx_engine_destroy(mv_eng[dj - 1]);
        }
        free(mv_eng);
        free(mv_copy);
        free(copy);
        set_ctx_error(ctx, "out of memory");
        return SBX_ENOMEM;
      }
      engine_set_custom_waves(mv_eng[vi - 1], ctx->custom_waves);
    }
  }

  if (loop && copy[frame_count - 1].time_sec <= 0.0) {
    if (mv_eng) {
      for (vi = 1; vi < mv_voice_count; vi++) {
        if (mv_eng[vi - 1]) sbx_engine_destroy(mv_eng[vi - 1]);
      }
      free(mv_eng);
    }
    if (mv_copy) free(mv_copy);
    free(copy);
    set_ctx_error(ctx, "looped keyframes require final time_sec > 0");
    return SBX_EINVAL;
  }

  ctx_clear_keyframes(ctx);
  ctx->kfs = copy;
  ctx->mv_kfs = mv_copy;
  ctx->mv_eng = mv_eng;
  ctx->kf_count = frame_count;
  ctx->mv_voice_count = mv_voice_count;
  ctx->kf_loop = loop ? 1 : 0;
  ctx->kf_seg = 0;
  ctx->kf_duration_sec = copy[frame_count - 1].time_sec;
  ctx->source_mode = SBX_CTX_SRC_KEYFRAMES;
  ctx->loaded = 1;
  ctx->t_sec = 0.0;

  rc = engine_apply_tone(ctx->eng, &ctx->kfs[0].tone, 1);
  if (rc != SBX_OK) {
    set_ctx_error(ctx, sbx_engine_last_error(ctx->eng));
    return rc;
  }
  for (vi = 1; vi < mv_voice_count; vi++) {
    rc = engine_apply_tone(ctx->mv_eng[vi - 1], &SBX_MV_KF(ctx, vi)[0].tone, 1);
    if (rc != SBX_OK) {
      set_ctx_error(ctx, sbx_engine_last_error(ctx->mv_eng[vi - 1]));
      return rc;
    }
  }

  set_ctx_error(ctx, NULL);
  return SBX_OK;
}

static void
ctx_eval_keyframed_tone_at(const SbxProgramKeyframe *kfs,
                           size_t n,
                           double t_sec,
                           size_t *inout_seg,
                           SbxToneSpec *out) {
  size_t i0, i1;
  double t0, t1, u;
  const SbxProgramKeyframe *k0, *k1;
  size_t seg = inout_seg ? *inout_seg : 0;

  if (n == 0) {
    sbx_default_tone_spec(out);
    out->mode = SBX_TONE_NONE;
    return;
  }
  if (n == 1 || t_sec <= kfs[0].time_sec) {
    *out = kfs[0].tone;
    return;
  }
  if (t_sec >= kfs[n - 1].time_sec) {
    *out = kfs[n - 1].tone;
    return;
  }

  if (seg >= n - 1) seg = n - 2;
  while (seg + 1 < n && t_sec > kfs[seg + 1].time_sec)
    seg++;
  while (seg > 0 && t_sec < kfs[seg].time_sec)
    seg--;

  i0 = seg;
  i1 = i0 + 1;
  if (i1 >= n) i1 = n - 1;
  k0 = &kfs[i0];
  k1 = &kfs[i1];

  t0 = k0->time_sec;
  t1 = k1->time_sec;
  if (t1 <= t0) {
    *out = k0->tone;
    return;
  }
  if (t_sec >= t1) {
    *out = k1->tone;
    return;
  }
  u = (t_sec - t0) / (t1 - t0);
  if (u < 0.0) u = 0.0;
  if (u > 1.0) u = 1.0;
  if (k0->interp == SBX_INTERP_STEP ||
      k0->tone.mode != k1->tone.mode ||
      k0->tone.waveform != k1->tone.waveform)
    u = 0.0;

  out->mode = k0->tone.mode;
  out->carrier_hz = sbx_lerp(k0->tone.carrier_hz, k1->tone.carrier_hz, u);
  out->beat_hz = sbx_lerp(k0->tone.beat_hz, k1->tone.beat_hz, u);
  out->amplitude = sbx_lerp(k0->tone.amplitude, k1->tone.amplitude, u);
  out->waveform = k0->tone.waveform;
  out->duty_cycle = sbx_lerp(k0->tone.duty_cycle, k1->tone.duty_cycle, u);
  if (inout_seg) *inout_seg = seg;
}

static void
ctx_eval_keyframed_tone(SbxContext *ctx, double t_sec, SbxToneSpec *out) {
  ctx_eval_keyframed_tone_at(ctx->kfs, ctx->kf_count, t_sec, &ctx->kf_seg, out);
}

static double
ctx_eval_program_beat_hz(SbxContext *ctx, double t_sec) {
  SbxToneSpec tone;
  if (!ctx || !ctx->eng) return 0.0;

  switch (ctx->source_mode) {
    case SBX_CTX_SRC_KEYFRAMES: {
      size_t seg = ctx->kf_seg;
      double eval_t = t_sec;
      if (ctx->kf_loop && ctx->kf_duration_sec > 0.0) {
        eval_t = fmod(eval_t, ctx->kf_duration_sec);
        if (eval_t < 0.0) eval_t += ctx->kf_duration_sec;
      }
      ctx_eval_keyframed_tone_at(ctx->kfs, ctx->kf_count, eval_t, &seg, &tone);
      break;
    }
    case SBX_CTX_SRC_STATIC:
      tone = ctx->eng->tone;
      break;
    default:
      return 0.0;
  }

  if (!isfinite(tone.beat_hz)) return 0.0;
  if (tone.beat_hz <= 0.0) return fabs(tone.beat_hz);
  return tone.beat_hz;
}

const char *
sbx_version(void) {
  return SBAGENXLIB_VERSION;
}

int
sbx_api_version(void) {
  return SBX_API_VERSION;
}

const char *
sbx_status_string(int status) {
  switch (status) {
    case SBX_OK: return "ok";
    case SBX_EINVAL: return "invalid argument";
    case SBX_ENOMEM: return "out of memory";
    case SBX_ENOTREADY: return "engine not ready";
    default: return "unknown status";
  }
}

int
sbx_parse_sbg_clock_token(const char *tok, size_t *out_consumed, double *out_sec) {
  int nn = 0;
  int hh = 0;
  int mm = 0;
  int ss = 0;
  if (!tok || !*tok || !out_sec) return SBX_EINVAL;
  if (3 <= sscanf(tok, "%2d:%2d:%2d%n", &hh, &mm, &ss, &nn)) {
    if (hh < 0 || hh >= 24 || mm < 0 || mm >= 60 || ss < 0 || ss >= 60)
      return SBX_EINVAL;
    *out_sec = (double)((hh * 60 + mm) * 60 + ss);
    if (out_consumed) *out_consumed = (size_t)nn;
    return SBX_OK;
  }
  ss = 0;
  if (2 <= sscanf(tok, "%2d:%2d%n", &hh, &mm, &nn)) {
    if (hh < 0 || hh >= 24 || mm < 0 || mm >= 60)
      return SBX_EINVAL;
    *out_sec = (double)((hh * 60 + mm) * 60);
    if (out_consumed) *out_consumed = (size_t)nn;
    return SBX_OK;
  }
  return SBX_EINVAL;
}

static int
parse_tone_spec_with_default_waveform(const char *spec,
                                      int default_waveform,
                                      SbxToneSpec *out_tone) {
  const char *p;
  const char *p0;
  int waveform = SBX_WAVE_SINE;
  int n = 0;
  double carrier = 0.0, beat = 0.0, amp_pct = 0.0;
  char c = 0;

  if (!spec || !out_tone) return SBX_EINVAL;

  p = skip_ws(spec);
  p0 = p;
  sbx_default_tone_spec(out_tone);
  p = skip_optional_waveform_prefix(p, &waveform);
  if (p != p0) {
    out_tone->waveform = waveform;
  } else if (default_waveform >= SBX_WAVE_SINE &&
             default_waveform <= SBX_WAVE_SAWTOOTH) {
    out_tone->waveform = default_waveform;
  }

  if (*p == '-' && *skip_ws(p + 1) == 0) {
    out_tone->mode = SBX_TONE_NONE;
    out_tone->amplitude = 0.0;
    return SBX_OK;
  }

  // Noise tones: pink/<amp>, white/<amp>, brown/<amp>
  if (sscanf(p, "pink/%lf %n", &amp_pct, &n) == 1 &&
      *skip_ws(p + n) == 0) {
    if (!isfinite(amp_pct) || amp_pct < 0.0) return SBX_EINVAL;
    out_tone->mode = SBX_TONE_PINK_NOISE;
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }
  if (sscanf(p, "white/%lf %n", &amp_pct, &n) == 1 &&
      *skip_ws(p + n) == 0) {
    if (!isfinite(amp_pct) || amp_pct < 0.0) return SBX_EINVAL;
    out_tone->mode = SBX_TONE_WHITE_NOISE;
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }
  if (sscanf(p, "brown/%lf %n", &amp_pct, &n) == 1 &&
      *skip_ws(p + n) == 0) {
    if (!isfinite(amp_pct) || amp_pct < 0.0) return SBX_EINVAL;
    out_tone->mode = SBX_TONE_BROWN_NOISE;
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }

  // Bell tone: bell<carrier>/<amp>
  if (sscanf(p, "bell%lf/%lf %n", &carrier, &amp_pct, &n) == 2 &&
      *skip_ws(p + n) == 0) {
    if (carrier <= 0.0 || !isfinite(carrier) || !isfinite(amp_pct) || amp_pct < 0.0)
      return SBX_EINVAL;
    out_tone->mode = SBX_TONE_BELL;
    out_tone->carrier_hz = carrier;
    out_tone->beat_hz = 0.0;
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }

  // Spin-noise tones:
  //  spin:<width-us><spin-hz>/<amp>   (pink noise base)
  //  bspin:<width-us><spin-hz>/<amp>  (brown noise base)
  //  wspin:<width-us><spin-hz>/<amp>  (white noise base)
  if (sscanf(p, "spin:%lf%lf/%lf %n", &carrier, &beat, &amp_pct, &n) == 3 &&
      *skip_ws(p + n) == 0) {
    if (!isfinite(carrier) || !isfinite(beat) || !isfinite(amp_pct) || amp_pct < 0.0)
      return SBX_EINVAL;
    out_tone->mode = SBX_TONE_SPIN_PINK;
    out_tone->carrier_hz = carrier;
    out_tone->beat_hz = beat;
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }
  if (sscanf(p, "bspin:%lf%lf/%lf %n", &carrier, &beat, &amp_pct, &n) == 3 &&
      *skip_ws(p + n) == 0) {
    if (!isfinite(carrier) || !isfinite(beat) || !isfinite(amp_pct) || amp_pct < 0.0)
      return SBX_EINVAL;
    out_tone->mode = SBX_TONE_SPIN_BROWN;
    out_tone->carrier_hz = carrier;
    out_tone->beat_hz = beat;
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }
  if (sscanf(p, "wspin:%lf%lf/%lf %n", &carrier, &beat, &amp_pct, &n) == 3 &&
      *skip_ws(p + n) == 0) {
    if (!isfinite(carrier) || !isfinite(beat) || !isfinite(amp_pct) || amp_pct < 0.0)
      return SBX_EINVAL;
    out_tone->mode = SBX_TONE_SPIN_WHITE;
    out_tone->carrier_hz = carrier;
    out_tone->beat_hz = beat;
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }

  // Isochronic: <carrier>@<pulse>/<amp>
  if (sscanf(p, "%lf@%lf/%lf %n", &carrier, &beat, &amp_pct, &n) == 3 &&
      *skip_ws(p + n) == 0) {
    if (carrier <= 0.0 || !isfinite(carrier) || !isfinite(beat) || !isfinite(amp_pct) || amp_pct < 0.0)
      return SBX_EINVAL;
    out_tone->mode = SBX_TONE_ISOCHRONIC;
    out_tone->carrier_hz = carrier;
    out_tone->beat_hz = fabs(beat);
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }

  // Monaural: <carrier>M<beat>/<amp> (M or m)
  if (sscanf(p, "%lf%c%lf/%lf %n", &carrier, &c, &beat, &amp_pct, &n) == 4 &&
      *skip_ws(p + n) == 0 &&
      (c == 'M' || c == 'm')) {
    if (carrier <= 0.0 || !isfinite(carrier) || !isfinite(beat) || !isfinite(amp_pct) || amp_pct < 0.0)
      return SBX_EINVAL;
    out_tone->mode = SBX_TONE_MONAURAL;
    out_tone->carrier_hz = carrier;
    out_tone->beat_hz = fabs(beat);
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }

  // Binaural: <carrier><+|-><beat>/<amp>
  if (sscanf(p, "%lf%c%lf/%lf %n", &carrier, &c, &beat, &amp_pct, &n) == 4 &&
      *skip_ws(p + n) == 0 &&
      (c == '+' || c == '-')) {
    if (carrier <= 0.0 || !isfinite(carrier) || !isfinite(beat) || !isfinite(amp_pct) || amp_pct < 0.0)
      return SBX_EINVAL;
    out_tone->mode = SBX_TONE_BINAURAL;
    out_tone->carrier_hz = carrier;
    out_tone->beat_hz = (c == '-') ? -fabs(beat) : fabs(beat);
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }

  // Simple tone: <carrier>/<amp> (binaural with beat=0)
  if (sscanf(p, "%lf/%lf %n", &carrier, &amp_pct, &n) == 2 &&
      *skip_ws(p + n) == 0) {
    if (carrier <= 0.0 || !isfinite(carrier) || !isfinite(amp_pct) || amp_pct < 0.0)
      return SBX_EINVAL;
    out_tone->mode = SBX_TONE_BINAURAL;
    out_tone->carrier_hz = carrier;
    out_tone->beat_hz = 0.0;
    out_tone->amplitude = amp_pct / 100.0;
    return SBX_OK;
  }

  return SBX_EINVAL;
}

int
sbx_parse_tone_spec(const char *spec, SbxToneSpec *out_tone) {
  return parse_tone_spec_with_default_waveform(spec, SBX_WAVE_SINE, out_tone);
}

int
sbx_parse_tone_spec_ex(const char *spec, int default_waveform, SbxToneSpec *out_tone) {
  if (default_waveform < SBX_WAVE_SINE || default_waveform > SBX_WAVE_SAWTOOTH)
    return SBX_EINVAL;
  return parse_tone_spec_with_default_waveform(spec, default_waveform, out_tone);
}

int
sbx_parse_mix_fx_spec(const char *spec, int default_waveform, SbxMixFxSpec *out_fx) {
  const char *p, *p0;
  int n = 0;
  int waveform;
  SbxMixFxSpec fx;
  double carr = 0.0, res = 0.0, amp_pct = 0.0;
  int rc = SBX_OK;

  if (!spec || !out_fx) return SBX_EINVAL;
  if (default_waveform < SBX_WAVE_SINE || default_waveform > SBX_WAVE_SAWTOOTH)
    return SBX_EINVAL;

  memset(&fx, 0, sizeof(fx));
  fx.type = SBX_MIXFX_NONE;
  waveform = default_waveform;
  p = skip_ws(spec);
  p0 = p;
  p = skip_optional_waveform_prefix(p, &waveform);
  if (p == p0) fx.waveform = default_waveform;
  else fx.waveform = waveform;

  if (sscanf(p, "mixspin:%lf%lf/%lf %n", &carr, &res, &amp_pct, &n) == 3 &&
      *skip_ws(p + n) == 0) {
    fx.type = SBX_MIXFX_SPIN;
    fx.carr = carr;
    fx.res = res;
    fx.amp = amp_pct / 100.0;
  } else if (strncasecmp(p, "mixpulse:beat", 13) == 0 &&
             (p[13] == 0 || p[13] == ':' || isspace((unsigned char)p[13]))) {
    /* Compatibility alias: mixpulse:beat[:params] -> mixam:beat[:params]. */
    const char *q = p + 13;
    q = skip_ws(q);
    sbx_mixam_set_defaults(&fx);
    fx.type = SBX_MIXFX_AM;
    fx.amp = 1.0;
    fx.mixam_bind_program_beat = 1;
    fx.res = 0.0;
    if (*q == ':') q++;
    q = skip_ws(q);
    if (*q) {
      rc = sbx_parse_mixam_params(q, &fx);
      if (rc != SBX_OK) return rc;
    } else {
      rc = sbx_validate_mixam_fields(&fx);
      if (rc != SBX_OK) return rc;
    }
  } else if (sscanf(p, "mixpulse:%lf/%lf %n", &res, &amp_pct, &n) == 2 &&
             *skip_ws(p + n) == 0) {
    fx.type = SBX_MIXFX_PULSE;
    fx.res = res;
    fx.amp = amp_pct / 100.0;
  } else if (sscanf(p, "mixbeat:%lf/%lf %n", &res, &amp_pct, &n) == 2 &&
             *skip_ws(p + n) == 0) {
    fx.type = SBX_MIXFX_BEAT;
    fx.res = res;
    fx.amp = amp_pct / 100.0;
  } else if (strncasecmp(p, "mixam:", 6) == 0) {
    const char *q = p + 6;
    char *end = 0;
    q = skip_ws(q);
    sbx_mixam_set_defaults(&fx);
    fx.type = SBX_MIXFX_AM;
    fx.amp = 1.0;
    if (*q == 0) return SBX_EINVAL;
    if (strncasecmp(q, "beat", 4) == 0 &&
        (q[4] == 0 || q[4] == ':' || isspace((unsigned char)q[4]))) {
      fx.mixam_bind_program_beat = 1;
      fx.res = 0.0;
      q += 4;
      q = skip_ws(q);
    } else if (!(tolower((unsigned char)q[0]) >= 'a' && tolower((unsigned char)q[0]) <= 'z' && q[1] == '=')) {
      fx.res = strtod(q, &end);
      if (end == q || !isfinite(fx.res)) return SBX_EINVAL;
      q = skip_ws(end);
    } else {
      fx.res = 10.0; /* default pulse/beat rate when omitted */
    }
    if (*q == ':') q++;
    q = skip_ws(q);
    if (*q) {
      rc = sbx_parse_mixam_params(q, &fx);
      if (rc != SBX_OK) return rc;
    } else {
      rc = sbx_validate_mixam_fields(&fx);
      if (rc != SBX_OK) return rc;
    }
  } else {
    return SBX_EINVAL;
  }

  if (fx.waveform < SBX_WAVE_SINE || fx.waveform > SBX_WAVE_SAWTOOTH)
    return SBX_EINVAL;
  if (!isfinite(fx.carr) || !isfinite(fx.res) || !isfinite(fx.amp) || fx.amp < 0.0)
    return SBX_EINVAL;
  if (fx.type == SBX_MIXFX_AM && sbx_validate_mixam_fields(&fx) != SBX_OK)
    return SBX_EINVAL;

  *out_fx = fx;
  return SBX_OK;
}

int
sbx_parse_extra_token(const char *tok,
                      int default_waveform,
                      int *out_type,
                      SbxToneSpec *out_tone,
                      SbxMixFxSpec *out_fx,
                      double *out_mix_amp_pct) {
  const char *p;
  int n = 0;
  double v = 0.0;
  int rc;
  if (!tok || !out_type) return SBX_EINVAL;
  *out_type = SBX_EXTRA_INVALID;

  p = skip_ws(tok);
  if (sscanf(p, "mix/%lf %n", &v, &n) == 1 && *skip_ws(p + n) == 0) {
    if (!isfinite(v)) return SBX_EINVAL;
    if (out_mix_amp_pct) *out_mix_amp_pct = v;
    *out_type = SBX_EXTRA_MIXAMP;
    return SBX_OK;
  }

  if (out_fx) {
    rc = sbx_parse_mix_fx_spec(p, default_waveform, out_fx);
    if (rc == SBX_OK) {
      *out_type = SBX_EXTRA_MIXFX;
      return SBX_OK;
    }
  }

  if (out_tone) {
    rc = sbx_parse_tone_spec_ex(p, default_waveform, out_tone);
    if (rc == SBX_OK) {
      *out_type = SBX_EXTRA_TONE;
      return SBX_OK;
    }
  }

  return SBX_EINVAL;
}

static const char *
wave_name_for_tone(int waveform) {
  switch (waveform) {
    case SBX_WAVE_SQUARE: return "square";
    case SBX_WAVE_TRIANGLE: return "triangle";
    case SBX_WAVE_SAWTOOTH: return "sawtooth";
    case SBX_WAVE_SINE:
    default: return "sine";
  }
}

static int
wave_prefix_for_tone(int waveform, char *out, size_t out_sz) {
  int custom_idx = sbx_custom_wave_index(waveform);
  if (!out || out_sz == 0) return 0;
  if (custom_idx >= 0)
    return snprintf_checked(out, out_sz, "wave%02d", custom_idx);
  return snprintf_checked(out, out_sz, "%s", wave_name_for_tone(waveform));
}

static int
snprintf_checked(char *out, size_t out_sz, const char *fmt, ...) {
  int n;
  va_list ap;
  if (!out || out_sz == 0 || !fmt) return 0;
  va_start(ap, fmt);
  n = vsnprintf(out, out_sz, fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= out_sz) return 0;
  return 1;
}

int
sbx_format_mix_fx_spec(const SbxMixFxSpec *fx, char *out, size_t out_sz) {
  const char *wname;
  if (!fx || !out || out_sz == 0) return SBX_EINVAL;
  if (fx->type < SBX_MIXFX_SPIN || fx->type > SBX_MIXFX_AM) return SBX_EINVAL;
  if (fx->waveform < SBX_WAVE_SINE || fx->waveform > SBX_WAVE_SAWTOOTH) return SBX_EINVAL;
  if (!isfinite(fx->carr) || !isfinite(fx->res) || !isfinite(fx->amp) || fx->amp < 0.0)
    return SBX_EINVAL;
  if (fx->type == SBX_MIXFX_AM && sbx_validate_mixam_fields(fx) != SBX_OK)
    return SBX_EINVAL;

  wname = wave_name_for_tone(fx->waveform);
  switch (fx->type) {
    case SBX_MIXFX_SPIN:
      if (!snprintf_checked(out, out_sz, "%s:mixspin:%g%+g/%g",
                            wname, fx->carr, fx->res, fx->amp * 100.0))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_MIXFX_PULSE:
      if (!snprintf_checked(out, out_sz, "%s:mixpulse:%g/%g",
                            wname, fx->res, fx->amp * 100.0))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_MIXFX_BEAT:
      if (!snprintf_checked(out, out_sz, "%s:mixbeat:%g/%g",
                            wname, fx->res, fx->amp * 100.0))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_MIXFX_AM:
      if (fx->mixam_bind_program_beat) {
        if (fx->mixam_mode == SBX_MIXAM_MODE_COS) {
          if (!snprintf_checked(out, out_sz, "mixam:beat:m=cos:s=%g:f=%g",
                                fx->mixam_start, fx->mixam_floor))
            return SBX_EINVAL;
        } else {
          if (!snprintf_checked(out, out_sz, "mixam:beat:s=%g:d=%g:a=%g:r=%g:e=%d:f=%g",
                                fx->mixam_start, fx->mixam_duty,
                                fx->mixam_attack, fx->mixam_release,
                                fx->mixam_edge_mode, fx->mixam_floor))
            return SBX_EINVAL;
        }
      } else {
        if (fx->mixam_mode == SBX_MIXAM_MODE_COS) {
          if (!snprintf_checked(out, out_sz, "mixam:%g:m=cos:s=%g:f=%g",
                                fx->res, fx->mixam_start, fx->mixam_floor))
            return SBX_EINVAL;
        } else {
          if (!snprintf_checked(out, out_sz, "mixam:%g:s=%g:d=%g:a=%g:r=%g:e=%d:f=%g",
                                fx->res,
                                fx->mixam_start, fx->mixam_duty,
                                fx->mixam_attack, fx->mixam_release,
                                fx->mixam_edge_mode, fx->mixam_floor))
            return SBX_EINVAL;
        }
      }
      return SBX_OK;
    default:
      break;
  }

  return SBX_EINVAL;
}

int
sbx_format_tone_spec(const SbxToneSpec *tone, char *out, size_t out_sz) {
  double amp_pct;
  SbxToneSpec norm;
  char wprefix[24];
  if (!tone || !out || out_sz == 0) return SBX_EINVAL;

  norm = *tone;
  if (normalize_tone(&norm, 0, 0) != SBX_OK) return SBX_EINVAL;
  amp_pct = norm.amplitude * 100.0;
  if (!wave_prefix_for_tone(norm.waveform, wprefix, sizeof(wprefix)))
    return SBX_EINVAL;

  switch (norm.mode) {
    case SBX_TONE_NONE:
      if (!snprintf_checked(out, out_sz, "-"))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_BINAURAL: {
      char sign = norm.beat_hz < 0.0 ? '-' : '+';
      double beat = fabs(norm.beat_hz);
      if (!snprintf_checked(out, out_sz, "%s:%g%c%g/%g",
                            wprefix, norm.carrier_hz, sign, beat, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    }
    case SBX_TONE_MONAURAL: {
      double beat = fabs(norm.beat_hz);
      double f1 = norm.carrier_hz - beat * 0.5;
      double f2 = norm.carrier_hz + beat * 0.5;
      if (!snprintf_checked(out, out_sz, "%s:%g/%g %s:%g/%g",
                            wprefix, f1, amp_pct, wprefix, f2, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    }
    case SBX_TONE_ISOCHRONIC: {
      double pulse = fabs(norm.beat_hz);
      if (!snprintf_checked(out, out_sz, "%s:%g@%g/%g",
                            wprefix, norm.carrier_hz, pulse, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    }
    case SBX_TONE_WHITE_NOISE:
      if (!snprintf_checked(out, out_sz, "white/%g", amp_pct)) return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_PINK_NOISE:
      if (!snprintf_checked(out, out_sz, "pink/%g", amp_pct)) return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_BROWN_NOISE:
      if (!snprintf_checked(out, out_sz, "brown/%g", amp_pct)) return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_BELL:
      if (!snprintf_checked(out, out_sz, "%s:bell%g/%g", wprefix, norm.carrier_hz, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_SPIN_PINK:
      if (!snprintf_checked(out, out_sz, "%s:spin:%g%+g/%g", wprefix, norm.carrier_hz, norm.beat_hz, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_SPIN_BROWN:
      if (!snprintf_checked(out, out_sz, "%s:bspin:%g%+g/%g", wprefix, norm.carrier_hz, norm.beat_hz, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_SPIN_WHITE:
      if (!snprintf_checked(out, out_sz, "%s:wspin:%g%+g/%g", wprefix, norm.carrier_hz, norm.beat_hz, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    default:
      return SBX_EINVAL;
  }
}

void
sbx_default_engine_config(SbxEngineConfig *cfg) {
  if (!cfg) return;
  cfg->sample_rate = 44100.0;
  cfg->channels = 2;
}

void
sbx_default_tone_spec(SbxToneSpec *tone) {
  if (!tone) return;
  tone->mode = SBX_TONE_NONE;
  tone->carrier_hz = 200.0;
  tone->beat_hz = 10.0;
  tone->amplitude = 0.5;
  tone->waveform = SBX_WAVE_SINE;
  tone->duty_cycle = 0.4;
}

SbxEngine *
sbx_engine_create(const SbxEngineConfig *cfg_in) {
  SbxEngine *eng;
  SbxEngineConfig cfg;

  sbx_default_engine_config(&cfg);
  if (cfg_in) cfg = *cfg_in;

  if (cfg.sample_rate <= 0.0 || cfg.channels != 2)
    return NULL;

  eng = (SbxEngine *)calloc(1, sizeof(*eng));
  if (!eng) return NULL;

  eng->cfg = cfg;
  sbx_default_tone_spec(&eng->tone);
  eng->rng_state = 0x12345678u;
  set_last_error(eng, NULL);
  return eng;
}

void
sbx_engine_destroy(SbxEngine *eng) {
  if (!eng) return;
  free(eng);
}

void
sbx_engine_reset(SbxEngine *eng) {
  if (!eng) return;
  eng->phase_l = 0.0;
  eng->phase_r = 0.0;
  eng->pulse_phase = 0.0;
  memset(eng->pink_l, 0, sizeof(eng->pink_l));
  memset(eng->pink_r, 0, sizeof(eng->pink_r));
  eng->brown_l = 0.0;
  eng->brown_r = 0.0;
  eng->bell_env = 0.0;
  eng->bell_tick = 0;
  eng->bell_tick_period = (int)(eng->cfg.sample_rate / 20.0);
  if (eng->bell_tick_period < 1) eng->bell_tick_period = 1;
}

int
sbx_engine_set_tone(SbxEngine *eng, const SbxToneSpec *tone_in) {
  return engine_apply_tone(eng, tone_in, 1);
}

int
sbx_engine_render_f32(SbxEngine *eng, float *out, size_t frames) {
  size_t i;

  if (!eng || !out) return SBX_EINVAL;
  if (eng->cfg.channels != 2 || eng->cfg.sample_rate <= 0.0) {
    set_last_error(eng, "engine configuration is invalid");
    return SBX_ENOTREADY;
  }

  if (eng->tone.mode == SBX_TONE_NONE) {
    memset(out, 0, frames * eng->cfg.channels * sizeof(float));
    return SBX_OK;
  }

  for (i = 0; i < frames; i++) {
    float left = 0.0f, right = 0.0f;
    engine_render_sample(eng, &left, &right);
    out[i * 2] = left;
    out[i * 2 + 1] = right;
  }

  return SBX_OK;
}

const char *
sbx_engine_last_error(const SbxEngine *eng) {
  if (!eng) return "null engine";
  if (!eng->last_error[0]) return "";
  return eng->last_error;
}

SbxContext *
sbx_context_create(const SbxEngineConfig *cfg) {
  SbxContext *ctx = (SbxContext *)calloc(1, sizeof(*ctx));
  if (!ctx) return NULL;

  ctx->eng = sbx_engine_create(cfg);
  if (!ctx->eng) {
    free(ctx);
    return NULL;
  }
  ctx->loaded = 0;
  ctx->default_waveform = SBX_WAVE_SINE;
  ctx->source_mode = SBX_CTX_SRC_NONE;
  ctx->kfs = 0;
  ctx->kf_count = 0;
  ctx->kf_loop = 0;
  ctx->kf_seg = 0;
  ctx->kf_duration_sec = 0.0;
  ctx->mv_kfs = 0;
  ctx->mv_eng = 0;
  ctx->mv_voice_count = 0;
  ctx->t_sec = 0.0;
  ctx->aux_tones = 0;
  ctx->aux_eng = 0;
  ctx->aux_count = 0;
  ctx->aux_buf = 0;
  ctx->aux_buf_cap = 0;
  ctx->mix_fx = 0;
  ctx->mix_fx_count = 0;
  ctx->sbg_mix_fx_kf = 0;
  ctx->sbg_mix_fx_kf_count = 0;
  ctx->sbg_mix_fx_slots = 0;
  ctx->sbg_mix_fx_seg = 0;
  ctx->sbg_mix_fx_state = 0;
  ctx->mix_kf = 0;
  ctx->mix_kf_count = 0;
  ctx->mix_kf_seg = 0;
  ctx->mix_default_amp_pct = 100.0;
  ctx_sync_custom_waves(ctx);
  set_ctx_error(ctx, NULL);
  return ctx;
}

void
sbx_context_destroy(SbxContext *ctx) {
  if (!ctx) return;
  ctx_clear_mix_keyframes(ctx);
  ctx_clear_sbg_mix_effect_keyframes(ctx);
  ctx_clear_mix_effects(ctx);
  ctx_clear_aux_tones(ctx);
  ctx_clear_keyframes(ctx);
  ctx_clear_custom_waves(ctx);
  sbx_engine_destroy(ctx->eng);
  free(ctx);
}

void
sbx_context_reset(SbxContext *ctx) {
  if (!ctx || !ctx->eng) return;
  ctx_reset_runtime(ctx);
  set_ctx_error(ctx, NULL);
}

int
sbx_context_set_tone(SbxContext *ctx, const SbxToneSpec *tone) {
  int rc;
  if (!ctx || !ctx->eng || !tone) return SBX_EINVAL;

  rc = engine_apply_tone(ctx->eng, tone, 1);
  if (rc != SBX_OK) {
    set_ctx_error(ctx, sbx_engine_last_error(ctx->eng));
    return rc;
  }
  ctx_clear_keyframes(ctx);
  ctx->source_mode = SBX_CTX_SRC_STATIC;
  ctx->loaded = 1;
  ctx->t_sec = 0.0;
  set_ctx_error(ctx, NULL);
  return SBX_OK;
}

int
sbx_context_set_default_waveform(SbxContext *ctx, int waveform) {
  if (!ctx || !ctx->eng) return SBX_EINVAL;
  if (waveform < SBX_WAVE_SINE || waveform > SBX_WAVE_SAWTOOTH) {
    set_ctx_error(ctx, "default waveform must be SBX_WAVE_*");
    return SBX_EINVAL;
  }
  ctx->default_waveform = waveform;
  set_ctx_error(ctx, NULL);
  return SBX_OK;
}

int
sbx_context_load_tone_spec(SbxContext *ctx, const char *tone_spec) {
  SbxToneSpec tone;
  int rc;

  if (!ctx || !ctx->eng || !tone_spec) return SBX_EINVAL;

  rc = parse_tone_spec_with_default_waveform(tone_spec, ctx->default_waveform, &tone);
  if (rc != SBX_OK) {
    set_ctx_error(ctx, "invalid tone spec");
    return rc;
  }
  return sbx_context_set_tone(ctx, &tone);
}

int
sbx_context_load_keyframes(SbxContext *ctx,
                           const SbxProgramKeyframe *frames,
                           size_t frame_count,
                           int loop) {
  return ctx_activate_keyframes_internal(ctx, frames, frame_count, frames, 1, loop);
}

int
sbx_context_load_sequence_text(SbxContext *ctx, const char *text, int loop) {
  char *buf = 0, *line = 0, *next = 0;
  size_t line_no = 0;
  SbxProgramKeyframe *frames = 0;
  size_t count = 0, cap = 0;
  int rc = SBX_OK;

  if (!ctx || !ctx->eng || !text) return SBX_EINVAL;

  buf = sbx_strdup_local(text);
  if (!buf) {
    set_ctx_error(ctx, "out of memory");
    return SBX_ENOMEM;
  }

  line = buf;
  while (line && *line) {
    char *p, *q;
    char *time_tok;
    char *tone_tok;
    char *interp_tok = 0;
    int interp = SBX_INTERP_LINEAR;
    double tsec;
    SbxToneSpec tone;

    line_no++;
    next = strchr(line, '\n');
    if (next) {
      *next = 0;
      next++;
    }

    if (line[0] && line[strlen(line) - 1] == '\r')
      line[strlen(line) - 1] = 0;

    strip_inline_comment(line);
    rstrip_inplace(line);
    p = (char *)skip_ws(line);
    if (*p == 0) {
      line = next;
      continue;
    }

    q = p;
    while (*q && !isspace((unsigned char)*q)) q++;
    if (*q == 0) {
      char emsg[192];
      snprintf(emsg, sizeof(emsg),
               "line %lu: expected '<time> <tone-spec>'",
               (unsigned long)line_no);
      set_ctx_error(ctx, emsg);
      rc = SBX_EINVAL;
      goto done;
    }
    *q++ = 0;
    q = (char *)skip_ws(q);
    if (*q == 0) {
      char emsg[192];
      snprintf(emsg, sizeof(emsg),
               "line %lu: missing tone-spec",
               (unsigned long)line_no);
      set_ctx_error(ctx, emsg);
      rc = SBX_EINVAL;
      goto done;
    }

    time_tok = p;
    tone_tok = q;

    {
      char *r = q;
      while (*r && !isspace((unsigned char)*r)) r++;
      if (*r) {
        *r++ = 0;
        r = (char *)skip_ws(r);
        if (*r) {
          interp_tok = r;
          while (*r && !isspace((unsigned char)*r)) r++;
          if (*r) {
            *r++ = 0;
            r = (char *)skip_ws(r);
            if (*r) {
              char emsg[224];
              snprintf(emsg, sizeof(emsg),
                       "line %lu: unexpected trailing token '%s'",
                       (unsigned long)line_no, r);
              set_ctx_error(ctx, emsg);
              rc = SBX_EINVAL;
              goto done;
            }
          }
        }
      }
    }

    rc = parse_time_seconds_token(time_tok, &tsec);
    if (rc != SBX_OK) {
      char emsg[224];
      snprintf(emsg, sizeof(emsg),
               "line %lu: invalid time token '%s' (use s/m/h suffix or seconds default)",
               (unsigned long)line_no, time_tok);
      set_ctx_error(ctx, emsg);
      rc = SBX_EINVAL;
      goto done;
    }

    rc = parse_tone_spec_with_default_waveform(tone_tok, ctx->default_waveform, &tone);
    if (rc != SBX_OK) {
      char emsg[224];
      snprintf(emsg, sizeof(emsg),
               "line %lu: invalid tone-spec '%s'",
               (unsigned long)line_no, tone_tok);
      set_ctx_error(ctx, emsg);
      rc = SBX_EINVAL;
      goto done;
    }

    if (interp_tok) {
      rc = parse_interp_mode_token(interp_tok, &interp);
      if (rc != SBX_OK) {
        char emsg[224];
        snprintf(emsg, sizeof(emsg),
                 "line %lu: invalid interpolation token '%s' (use linear|ramp|step|hold)",
                 (unsigned long)line_no, interp_tok);
        set_ctx_error(ctx, emsg);
        rc = SBX_EINVAL;
        goto done;
      }
    }

    if (count == cap) {
      size_t ncap = cap ? (cap * 2) : 8;
      SbxProgramKeyframe *tmp;
      tmp = (SbxProgramKeyframe *)realloc(frames, ncap * sizeof(*frames));
      if (!tmp) {
        set_ctx_error(ctx, "out of memory");
        rc = SBX_ENOMEM;
        goto done;
      }
      frames = tmp;
      cap = ncap;
    }

    frames[count].time_sec = tsec;
    frames[count].tone = tone;
    frames[count].interp = interp;
    count++;
    line = next;
  }

  if (count == 0) {
    set_ctx_error(ctx, "sequence text contains no keyframes");
    rc = SBX_EINVAL;
    goto done;
  }

  rc = sbx_context_load_keyframes(ctx, frames, count, loop);

done:
  if (frames) free(frames);
  if (buf) free(buf);
  return rc;
}

int
sbx_context_load_sequence_file(SbxContext *ctx, const char *path, int loop) {
  char *text = 0;
  int rc = SBX_OK;

  if (!ctx || !ctx->eng || !path) return SBX_EINVAL;

  rc = read_text_file_alloc(path, &text);
  if (rc == SBX_EINVAL) {
    char emsg[256];
    snprintf(emsg, sizeof(emsg), "cannot open sequence file: %s", path);
    set_ctx_error(ctx, emsg);
    return rc;
  } else if (rc == SBX_ENOMEM) {
    set_ctx_error(ctx, "out of memory");
    return rc;
  } else if (rc != SBX_OK) {
    set_ctx_error(ctx, "failed reading sequence file");
    return rc;
  }

  rc = sbx_context_load_sequence_text(ctx, text, loop);

  if (text) free(text);
  return rc;
}

int
sbx_context_load_sbg_timing_text(SbxContext *ctx, const char *text, int loop) {
  char *buf = 0, *line = 0, *next = 0;
  size_t line_no = 0;
  SbxVoiceSetKeyframe *frames = 0;
  SbxProgramKeyframe *primary = 0;
  SbxProgramKeyframe *mv_frames = 0;
  SbxMixAmpKeyframe *mix_kfs = 0;
  SbxMixFxKeyframe *mix_fx_kfs = 0;
  size_t count = 0, cap = 0;
  SbxNamedToneDef *defs = 0;
  size_t ndefs = 0, defs_cap = 0;
  SbxNamedBlockDef *blocks = 0;
  size_t nblocks = 0, blocks_cap = 0;
  double *custom_waves[SBX_CUSTOM_WAVE_COUNT] = {0};
  int active_block_idx = -1;
  double active_block_last_off = -1.0;
  double last_abs_sec = -1.0;
  size_t max_voice_count = 1;
  size_t max_mix_fx_slots = 0;
  int rc = SBX_OK;

  if (!ctx || !ctx->eng || !text) return SBX_EINVAL;

  buf = sbx_strdup_local(text);
  if (!buf) {
    set_ctx_error(ctx, "out of memory");
    return SBX_ENOMEM;
  }

  line = buf;
  while (line && *line) {
    char *p, *q, *rest;
    char *time_tok;
    int interp = SBX_INTERP_LINEAR;
    int expanded_block = 0;
    int is_definition = 0;
    double tsec;
    SbxVoiceSetKeyframe frame;

    sbx_voice_set_frame_init(&frame);

    line_no++;
    next = strchr(line, '\n');
    if (next) {
      *next = 0;
      next++;
    }

    if (line[0] && line[strlen(line) - 1] == '\r')
      line[strlen(line) - 1] = 0;

    strip_inline_comment(line);
    rstrip_inplace(line);
    p = (char *)skip_ws(line);
    if (*p == 0) {
      line = next;
      continue;
    }

    q = p;
    while (*q && !isspace((unsigned char)*q)) q++;
    if (*q) {
      *q++ = 0;
      rest = (char *)skip_ws(q);
    } else {
      rest = q;
    }

    if (active_block_idx >= 0) {
      char *time_tok = p;
      int interp = SBX_INTERP_LINEAR;
      double off_sec = 0.0;
      SbxVoiceSetKeyframe blk_frame;
      SbxNamedBlockDef *blk = &blocks[active_block_idx];

      sbx_voice_set_frame_init(&blk_frame);

      if (strcmp(time_tok, "}") == 0) {
        if (rest && *rest) {
          char emsg[224];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: unexpected trailing token(s) after block close",
                   (unsigned long)line_no);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }
        active_block_idx = -1;
        active_block_last_off = -1.0;
        line = next;
        continue;
      }

      if (*rest == 0) {
        char emsg[224];
        snprintf(emsg, sizeof(emsg),
                 "line %lu: missing tone-spec or named tone-set in block '%s'",
                 (unsigned long)line_no, blk->name ? blk->name : "?");
        set_ctx_error(ctx, emsg);
        rc = SBX_EINVAL;
        goto done;
      }

      rc = parse_sbg_relative_offset_token(time_tok, &off_sec);
      if (rc != SBX_OK) {
        char emsg[256];
        snprintf(emsg, sizeof(emsg),
                 "line %lu: invalid block-relative timing token '%s' (use +HH:MM[:SS][+...])",
                 (unsigned long)line_no, time_tok);
        set_ctx_error(ctx, emsg);
        rc = SBX_EINVAL;
        goto done;
      }
      if (active_block_last_off >= 0.0 && off_sec < active_block_last_off) {
        char emsg[256];
        snprintf(emsg, sizeof(emsg),
                 "line %lu: block '%s' timings must be non-decreasing",
                 (unsigned long)line_no, blk->name ? blk->name : "?");
        set_ctx_error(ctx, emsg);
        rc = SBX_EINVAL;
        goto done;
      }

      {
        char **tokv = 0;
        size_t nt = 0;
        size_t idx = 0;
        int had_leading_transition = 0;
        int nested_block_idx = -1;
        rc = split_ws_tokens_inplace(rest, &tokv, &nt);
        if (rc == SBX_ENOMEM) {
          set_ctx_error(ctx, "out of memory");
          goto done;
        }
        if (rc != SBX_OK) {
          set_ctx_error(ctx, "failed tokenizing block entry");
          rc = SBX_EINVAL;
          goto done;
        }

        if (nt <= 0) {
          char emsg[224];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: missing tone-spec or named tone-set token in block '%s'",
                   (unsigned long)line_no, blk->name ? blk->name : "?");
          if (tokv) free(tokv);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }

        if (is_sbg_transition_token(tokv[idx])) {
          interp = sbg_transition_token_to_interp(tokv[idx]);
          had_leading_transition = 1;
          idx++;
        }
        if (idx >= nt) {
          char emsg[224];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: missing tone-spec or named tone-set after transition token in block '%s'",
                   (unsigned long)line_no, blk->name ? blk->name : "?");
          if (tokv) free(tokv);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }

        while (idx < nt) {
          int bidx = -1;
          const char *tok = tokv[idx];
          int interp_tmp;
          if (parse_interp_mode_token(tok, &interp_tmp) == SBX_OK) {
            interp = interp_tmp;
            idx++;
            continue;
          }
          if (is_sbg_transition_token(tok)) {
            interp = sbg_transition_token_to_interp(tok);
            idx++;
            continue;
          }
          rc = sbx_frame_apply_token(&blk_frame, tok, ctx->default_waveform,
                                     defs, ndefs, &bidx, blocks, nblocks);
          if (rc != SBX_OK) {
            char emsg[256];
            snprintf(emsg, sizeof(emsg),
                     "line %lu: invalid block tone-spec, unknown named tone-set, or unknown nested block '%s'",
                     (unsigned long)line_no, tok);
            if (tokv) free(tokv);
            set_ctx_error(ctx, emsg);
            rc = SBX_EINVAL;
            goto done;
          }
          if (bidx >= 0) {
            if (bidx == active_block_idx) {
              char emsg[256];
              snprintf(emsg, sizeof(emsg),
                       "line %lu: block '%s' cannot reference itself",
                       (unsigned long)line_no, blk->name ? blk->name : "?");
              if (tokv) free(tokv);
              set_ctx_error(ctx, emsg);
              rc = SBX_EINVAL;
              goto done;
            }
            if (had_leading_transition || idx + 1 < nt ||
                blk_frame.tone_len > 0 || blk_frame.mix_amp_present || blk_frame.mix_fx_count > 0) {
              char emsg[256];
              snprintf(emsg, sizeof(emsg),
                       "line %lu: nested block '%s' must appear alone in block '%s'",
                       (unsigned long)line_no, tok, blk->name ? blk->name : "?");
              if (tokv) free(tokv);
              set_ctx_error(ctx, emsg);
              rc = SBX_EINVAL;
              goto done;
            }
            nested_block_idx = bidx;
            idx++;
            break;
          }
          idx++;
        }

        if (nested_block_idx >= 0) {
          size_t bi;
          if (blocks[nested_block_idx].count == 0) {
            char emsg[256];
            snprintf(emsg, sizeof(emsg),
                     "line %lu: nested block '%s' has no entries",
                     (unsigned long)line_no, tokv[idx - 1]);
            if (tokv) free(tokv);
            set_ctx_error(ctx, emsg);
            rc = SBX_EINVAL;
            goto done;
          }
          for (bi = 0; bi < blocks[nested_block_idx].count; bi++) {
            SbxVoiceSetKeyframe nested_entry = blocks[nested_block_idx].entries[bi];
            double nested_off = off_sec + nested_entry.time_sec;
            if (active_block_last_off >= 0.0 && nested_off < active_block_last_off) {
              char emsg[256];
              snprintf(emsg, sizeof(emsg),
                       "line %lu: nested expansion in block '%s' is not time-ordered",
                       (unsigned long)line_no, blk->name ? blk->name : "?");
              if (tokv) free(tokv);
              set_ctx_error(ctx, emsg);
              rc = SBX_EINVAL;
              goto done;
            }
            nested_entry.time_sec = nested_off;
            rc = block_append_entry(blk, &nested_entry);
            if (rc != SBX_OK) {
              set_ctx_error(ctx, "out of memory");
              rc = SBX_ENOMEM;
              goto done;
            }
            if (nested_entry.tone_len > max_voice_count) max_voice_count = nested_entry.tone_len;
            if (nested_entry.mix_fx_count > max_mix_fx_slots) max_mix_fx_slots = nested_entry.mix_fx_count;
            active_block_last_off = nested_off;
          }
        } else {
          if (blk_frame.tone_len == 0 && !blk_frame.mix_amp_present && blk_frame.mix_fx_count == 0) {
            char emsg[256];
            snprintf(emsg, sizeof(emsg),
                     "line %lu: missing tone-spec or named tone-set token in block '%s'",
                     (unsigned long)line_no, blk->name ? blk->name : "?");
            if (tokv) free(tokv);
            set_ctx_error(ctx, emsg);
            rc = SBX_EINVAL;
            goto done;
          }
          if (blk_frame.mix_fx_count > 0 && !blk_frame.mix_amp_present) {
            char emsg[256];
            snprintf(emsg, sizeof(emsg),
                     "line %lu: mix effects in block '%s' require mix/<amp> on the same line or in the referenced named tone-set",
                     (unsigned long)line_no, blk->name ? blk->name : "?");
            if (tokv) free(tokv);
            set_ctx_error(ctx, emsg);
            rc = SBX_EINVAL;
            goto done;
          }
          blk_frame.time_sec = off_sec;
          blk_frame.interp = interp;
          rc = block_append_entry(blk, &blk_frame);
          if (rc != SBX_OK) {
            set_ctx_error(ctx, "out of memory");
            rc = SBX_ENOMEM;
            goto done;
          }
          if (blk_frame.tone_len > max_voice_count) max_voice_count = blk_frame.tone_len;
          if (blk_frame.mix_fx_count > max_mix_fx_slots) max_mix_fx_slots = blk_frame.mix_fx_count;
          active_block_last_off = off_sec;
        }
        if (tokv) free(tokv);
      }

      line = next;
      continue;
    }

    /* Accept named tone-set definitions: name: <tone-spec> */
    {
      size_t l0 = strlen(p);
      if (l0 > 1 && p[l0 - 1] == ':') {
        double ttmp;
        p[l0 - 1] = 0;
        if (parse_hhmmss_token(p, &ttmp) != SBX_OK)
          is_definition = 1;
        else
          p[l0 - 1] = ':';
      }
    }

    if (is_definition) {
      char *r = rest;
      char *name = p;
      int didx;
      int bidx;

      if (*name == 0) {
        char emsg[208];
        snprintf(emsg, sizeof(emsg),
                 "line %lu: empty named tone-set definition",
                 (unsigned long)line_no);
        set_ctx_error(ctx, emsg);
        rc = SBX_EINVAL;
        goto done;
      }
      if (!r || *r == 0) {
        char emsg[208];
        snprintf(emsg, sizeof(emsg),
                 "line %lu: named tone-set '%s' is missing tone-spec",
                 (unsigned long)line_no, name);
        set_ctx_error(ctx, emsg);
        rc = SBX_EINVAL;
        goto done;
      }

      if (strncmp(name, "wave", 4) == 0 &&
          isdigit((unsigned char)name[4]) &&
          isdigit((unsigned char)name[5]) &&
          name[6] == 0) {
        int wave_idx = (name[4] - '0') * 10 + (name[5] - '0');
        double *raw = 0;
        size_t raw_count = 0, raw_cap = 0;
        char *wr = r;
        if (custom_waves[wave_idx]) {
          char emsg[224];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: waveform %02d already defined",
                   (unsigned long)line_no, wave_idx);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }
        while (*wr) {
          char *end = 0;
          double v = strtod(wr, &end);
          if (end == wr || !isfinite(v)) {
            char emsg[224];
            snprintf(emsg, sizeof(emsg),
                     "line %lu: waveform %02d expects floating-point samples",
                     (unsigned long)line_no, wave_idx);
            if (raw) free(raw);
            set_ctx_error(ctx, emsg);
            rc = SBX_EINVAL;
            goto done;
          }
          if (raw_count == raw_cap) {
            size_t ncap = raw_cap ? raw_cap * 2 : 16;
            double *tmp = (double *)realloc(raw, ncap * sizeof(*raw));
            if (!tmp) {
              if (raw) free(raw);
              set_ctx_error(ctx, "out of memory");
              rc = SBX_ENOMEM;
              goto done;
            }
            raw = tmp;
            raw_cap = ncap;
          }
          raw[raw_count++] = v;
          wr = (char *)skip_ws(end);
        }
        rc = sbx_build_custom_wave_table_from_samples(raw, raw_count, &custom_waves[wave_idx]);
        if (raw) free(raw);
        if (rc != SBX_OK) {
          char emsg[224];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: invalid waveform %02d definition (need at least two non-identical samples)",
                   (unsigned long)line_no, wave_idx);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }
        line = next;
        continue;
      }

      bidx = named_block_find(blocks, nblocks, name);
      if (strcmp(r, "{") == 0) {
        if (bidx < 0 && named_tone_find(defs, ndefs, name) >= 0) {
          char emsg[224];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: '%s' is already defined as a named tone-set",
                   (unsigned long)line_no, name);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }

        if (bidx >= 0) {
          if (blocks[bidx].entries) free(blocks[bidx].entries);
          blocks[bidx].entries = 0;
          blocks[bidx].count = 0;
          blocks[bidx].cap = 0;
        } else {
          if (nblocks == blocks_cap) {
            size_t ncap = blocks_cap ? (blocks_cap * 2) : 8;
            SbxNamedBlockDef *tmp =
                (SbxNamedBlockDef *)realloc(blocks, ncap * sizeof(*blocks));
            if (!tmp) {
              set_ctx_error(ctx, "out of memory");
              rc = SBX_ENOMEM;
              goto done;
            }
            blocks = tmp;
            blocks_cap = ncap;
          }
          bidx = (int)nblocks;
          blocks[nblocks].name = sbx_strdup_local(name);
          blocks[nblocks].entries = 0;
          blocks[nblocks].count = 0;
          blocks[nblocks].cap = 0;
          if (!blocks[nblocks].name) {
            set_ctx_error(ctx, "out of memory");
            rc = SBX_ENOMEM;
            goto done;
          }
          nblocks++;
        }
        active_block_idx = bidx;
        active_block_last_off = -1.0;
        line = next;
        continue;
      }
      if (bidx >= 0) {
        char emsg[224];
        snprintf(emsg, sizeof(emsg),
                 "line %lu: '%s' is already defined as a block name",
                 (unsigned long)line_no, name);
        set_ctx_error(ctx, emsg);
        rc = SBX_EINVAL;
        goto done;
      }

      {
        while (*r) {
          int bdef = -1;
          char *v_tok = r;
          while (*r && !isspace((unsigned char)*r)) r++;
          if (*r) {
            *r++ = 0;
            r = (char *)skip_ws(r);
          }
          rc = sbx_frame_apply_token(&frame, v_tok, ctx->default_waveform,
                                     defs, ndefs, &bdef, blocks, nblocks);
          if (rc != SBX_OK || bdef >= 0) {
            char emsg[224];
            snprintf(emsg, sizeof(emsg),
                     "line %lu: invalid named tone-set token '%s'",
                     (unsigned long)line_no, v_tok);
            set_ctx_error(ctx, emsg);
            rc = SBX_EINVAL;
            goto done;
          }
        }
        if (frame.tone_len == 0 && !frame.mix_amp_present && frame.mix_fx_count == 0) {
          char emsg[208];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: named tone-set '%s' is missing content",
                   (unsigned long)line_no, name);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }
        if (frame.mix_fx_count > 0 && !frame.mix_amp_present) {
          char emsg[256];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: mix effects in named tone-set '%s' require mix/<amp> on the same line or in a referenced named tone-set",
                   (unsigned long)line_no, name);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }
        if (frame.tone_len > max_voice_count) max_voice_count = frame.tone_len;
        if (frame.mix_fx_count > max_mix_fx_slots) max_mix_fx_slots = frame.mix_fx_count;
      }

      didx = named_tone_find(defs, ndefs, name);
      if (didx >= 0) {
        defs[didx].frame = frame;
      } else {
        if (ndefs == defs_cap) {
          size_t ncap = defs_cap ? (defs_cap * 2) : 8;
          SbxNamedToneDef *tmp =
              (SbxNamedToneDef *)realloc(defs, ncap * sizeof(*defs));
          if (!tmp) {
            set_ctx_error(ctx, "out of memory");
            rc = SBX_ENOMEM;
            goto done;
          }
          defs = tmp;
          defs_cap = ncap;
        }
        defs[ndefs].name = sbx_strdup_local(name);
        if (!defs[ndefs].name) {
          set_ctx_error(ctx, "out of memory");
          rc = SBX_ENOMEM;
          goto done;
        }
        defs[ndefs].frame = frame;
        ndefs++;
      }
      line = next;
      continue;
    }

    time_tok = p;
    if (*rest == 0) {
      char emsg[192];
      snprintf(emsg, sizeof(emsg),
               "line %lu: missing tone-spec or named tone-set",
               (unsigned long)line_no);
      set_ctx_error(ctx, emsg);
      rc = SBX_EINVAL;
      goto done;
    }

    rc = parse_sbg_timeline_time_token(time_tok, &last_abs_sec, &tsec);
    if (rc != SBX_OK) {
      char emsg[224];
      snprintf(emsg, sizeof(emsg),
               "line %lu: invalid SBG timing token '%s' (use HH:MM[:SS], NOW, and optional +relative parts)",
               (unsigned long)line_no, time_tok);
      set_ctx_error(ctx, emsg);
      rc = SBX_EINVAL;
      goto done;
    }

    {
      char **tokv = 0;
      size_t nt = 0;
      size_t idx = 0;
      int had_leading_transition = 0;
      int block_idx = -1;
      rc = split_ws_tokens_inplace(rest, &tokv, &nt);
      if (rc == SBX_ENOMEM) {
        set_ctx_error(ctx, "out of memory");
        goto done;
      }
      if (rc != SBX_OK) {
        set_ctx_error(ctx, "failed tokenizing SBG timing entry");
        rc = SBX_EINVAL;
        goto done;
      }

      if (nt <= 0) {
        char emsg[224];
        snprintf(emsg, sizeof(emsg),
                 "line %lu: missing tone-spec or named tone-set token",
                 (unsigned long)line_no);
        if (tokv) free(tokv);
        set_ctx_error(ctx, emsg);
        rc = SBX_EINVAL;
        goto done;
      }

      if (is_sbg_transition_token(tokv[idx])) {
        interp = sbg_transition_token_to_interp(tokv[idx]);
        had_leading_transition = 1;
        idx++;
      }
      if (idx >= nt) {
        char emsg[224];
        snprintf(emsg, sizeof(emsg),
                 "line %lu: missing tone-spec or named tone-set after transition token",
                 (unsigned long)line_no);
        if (tokv) free(tokv);
        set_ctx_error(ctx, emsg);
        rc = SBX_EINVAL;
        goto done;
      }

      while (idx < nt) {
        int interp_tmp;
        int bidx = -1;
        const char *tok = tokv[idx];
        if (parse_interp_mode_token(tok, &interp_tmp) == SBX_OK) {
          interp = interp_tmp;
          idx++;
          continue;
        }
        if (is_sbg_transition_token(tok)) {
          interp = sbg_transition_token_to_interp(tok);
          idx++;
          continue;
        }
        rc = sbx_frame_apply_token(&frame, tok, ctx->default_waveform,
                                   defs, ndefs, &bidx, blocks, nblocks);
        if (rc != SBX_OK) {
          char emsg[224];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: invalid tone-spec or unknown named tone-set '%s'",
                   (unsigned long)line_no, tok);
          if (tokv) free(tokv);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }
        if (bidx >= 0) {
          if (had_leading_transition || idx + 1 < nt ||
              frame.tone_len > 0 || frame.mix_amp_present || frame.mix_fx_count > 0) {
            char emsg[256];
            snprintf(emsg, sizeof(emsg),
                     "line %lu: block invocation '%s' must appear alone",
                     (unsigned long)line_no, tok);
            if (tokv) free(tokv);
            set_ctx_error(ctx, emsg);
            rc = SBX_EINVAL;
            goto done;
          }
          block_idx = bidx;
          idx++;
          break;
        }
        idx++;
      }

      if (block_idx >= 0) {
        size_t bi;
        if (blocks[block_idx].count == 0) {
          char emsg[256];
          snprintf(emsg, sizeof(emsg),
                   "line %lu: block '%s' has no entries",
                   (unsigned long)line_no, tokv[idx - 1]);
          if (tokv) free(tokv);
          set_ctx_error(ctx, emsg);
          rc = SBX_EINVAL;
          goto done;
        }
        for (bi = 0; bi < blocks[block_idx].count; bi++) {
          SbxVoiceSetKeyframe expanded = blocks[block_idx].entries[bi];
          if (count == cap) {
            size_t ncap = cap ? (cap * 2) : 8;
            SbxVoiceSetKeyframe *tmp;
            tmp = (SbxVoiceSetKeyframe *)realloc(frames, ncap * sizeof(*frames));
            if (!tmp) {
              set_ctx_error(ctx, "out of memory");
              rc = SBX_ENOMEM;
              goto done;
            }
            frames = tmp;
            cap = ncap;
          }
          expanded.time_sec = tsec + expanded.time_sec;
          frames[count++] = expanded;
          if (expanded.tone_len > max_voice_count) max_voice_count = expanded.tone_len;
          if (expanded.mix_fx_count > max_mix_fx_slots) max_mix_fx_slots = expanded.mix_fx_count;
        }
        expanded_block = 1;
      }
      if (tokv) free(tokv);
    }

    if (expanded_block) {
      line = next;
      continue;
    }

    if (count == cap) {
      size_t ncap = cap ? (cap * 2) : 8;
      SbxVoiceSetKeyframe *tmp;
      tmp = (SbxVoiceSetKeyframe *)realloc(frames, ncap * sizeof(*frames));
      if (!tmp) {
        set_ctx_error(ctx, "out of memory");
        rc = SBX_ENOMEM;
        goto done;
      }
      frames = tmp;
      cap = ncap;
    }

    if (frame.tone_len == 0 && !frame.mix_amp_present && frame.mix_fx_count == 0) {
      char emsg[192];
      snprintf(emsg, sizeof(emsg),
               "line %lu: missing tone-spec or named tone-set",
               (unsigned long)line_no);
      set_ctx_error(ctx, emsg);
      rc = SBX_EINVAL;
      goto done;
    }
    if (frame.mix_fx_count > 0 && !frame.mix_amp_present) {
      char emsg[256];
      snprintf(emsg, sizeof(emsg),
               "line %lu: mix effects require mix/<amp> on the same line or in the referenced named tone-set",
               (unsigned long)line_no);
      set_ctx_error(ctx, emsg);
      rc = SBX_EINVAL;
      goto done;
    }
    frame.time_sec = tsec;
    frame.interp = interp;
    frames[count] = frame;
    if (frame.tone_len > max_voice_count) max_voice_count = frame.tone_len;
    if (frame.mix_fx_count > max_mix_fx_slots) max_mix_fx_slots = frame.mix_fx_count;
    count++;
    line = next;
  }

  if (active_block_idx >= 0) {
    char emsg[256];
    snprintf(emsg, sizeof(emsg),
             "unterminated block definition '%s' (missing closing '}')",
             blocks[active_block_idx].name ? blocks[active_block_idx].name : "?");
    set_ctx_error(ctx, emsg);
    rc = SBX_EINVAL;
    goto done;
  }

  if (count == 0) {
    set_ctx_error(ctx, "sbg timing text contains no keyframes");
    rc = SBX_EINVAL;
    goto done;
  }

  {
    size_t ki;
    int have_mix = 0;
    for (ki = 0; ki < count; ki++) {
      if (frames[ki].mix_amp_present) {
        have_mix = 1;
        break;
      }
    }
    if (have_mix) {
      mix_kfs = (SbxMixAmpKeyframe *)calloc(count, sizeof(*mix_kfs));
      if (!mix_kfs) {
        set_ctx_error(ctx, "out of memory");
        rc = SBX_ENOMEM;
        goto done;
      }
      for (ki = 0; ki < count; ki++) {
        mix_kfs[ki].time_sec = frames[ki].time_sec;
        mix_kfs[ki].interp = frames[ki].interp;
        mix_kfs[ki].amp_pct = frames[ki].mix_amp_present ? frames[ki].mix_amp_pct : 0.0;
      }
    }
    if (max_mix_fx_slots > 0) {
      mix_fx_kfs = (SbxMixFxKeyframe *)calloc(count, sizeof(*mix_fx_kfs));
      if (!mix_fx_kfs) {
        set_ctx_error(ctx, "out of memory");
        rc = SBX_ENOMEM;
        goto done;
      }
      for (ki = 0; ki < count; ki++) {
        mix_fx_kfs[ki].time_sec = frames[ki].time_sec;
        mix_fx_kfs[ki].interp = frames[ki].interp;
        mix_fx_kfs[ki].mix_fx_count = frames[ki].mix_fx_count;
        memcpy(mix_fx_kfs[ki].mix_fx, frames[ki].mix_fx, sizeof(mix_fx_kfs[ki].mix_fx));
      }
    }
  }

  primary = (SbxProgramKeyframe *)calloc(count, sizeof(*primary));
  if (!primary) {
    set_ctx_error(ctx, "out of memory");
    rc = SBX_ENOMEM;
    goto done;
  }
  if (max_voice_count > 1) {
    size_t vi;
    mv_frames = (SbxProgramKeyframe *)calloc(max_voice_count * count, sizeof(*mv_frames));
    if (!mv_frames) {
      set_ctx_error(ctx, "out of memory");
      rc = SBX_ENOMEM;
      goto done;
    }
    for (vi = 0; vi < max_voice_count; vi++) {
      size_t ki;
      for (ki = 0; ki < count; ki++) {
        mv_frames[vi * count + ki].time_sec = frames[ki].time_sec;
        mv_frames[vi * count + ki].tone = frames[ki].tones[vi];
        mv_frames[vi * count + ki].interp = frames[ki].interp;
      }
    }
  }
  {
    size_t ki;
    for (ki = 0; ki < count; ki++) {
      primary[ki].time_sec = frames[ki].time_sec;
      primary[ki].tone = frames[ki].tones[0];
      primary[ki].interp = frames[ki].interp;
    }
  }

  ctx_replace_custom_waves(ctx, custom_waves);
  rc = ctx_activate_keyframes_internal(ctx, primary, count,
                                       mv_frames ? mv_frames : primary,
                                       max_voice_count ? max_voice_count : 1,
                                       loop);
  if (rc != SBX_OK) goto done;
  rc = sbx_context_set_mix_amp_keyframes(ctx, mix_kfs, mix_kfs ? count : 0,
                                         mix_kfs ? 0.0 : 100.0);
  if (rc != SBX_OK) goto done;
  rc = ctx_set_sbg_mix_effect_keyframes_internal(ctx, mix_fx_kfs,
                                                 mix_fx_kfs ? count : 0,
                                                 max_mix_fx_slots);

done:
  if (defs) {
    size_t i;
    for (i = 0; i < ndefs; i++) {
      if (defs[i].name) free(defs[i].name);
    }
    free(defs);
  }
  if (blocks) {
    size_t i;
    for (i = 0; i < nblocks; i++) {
      if (blocks[i].name) free(blocks[i].name);
      if (blocks[i].entries) free(blocks[i].entries);
    }
    free(blocks);
  }
  if (frames) free(frames);
  if (primary) free(primary);
  if (mv_frames) free(mv_frames);
  if (mix_kfs) free(mix_kfs);
  if (mix_fx_kfs) free(mix_fx_kfs);
  {
    size_t i;
    for (i = 0; i < SBX_CUSTOM_WAVE_COUNT; i++) {
      if (custom_waves[i]) free(custom_waves[i]);
    }
  }
  if (buf) free(buf);
  return rc;
}

int
sbx_context_load_sbg_timing_file(SbxContext *ctx, const char *path, int loop) {
  char *text = 0;
  int rc;
  if (!ctx || !ctx->eng || !path) return SBX_EINVAL;

  rc = read_text_file_alloc(path, &text);
  if (rc == SBX_EINVAL) {
    char emsg[256];
    snprintf(emsg, sizeof(emsg), "cannot open sbg timing file: %s", path);
    set_ctx_error(ctx, emsg);
    return rc;
  } else if (rc == SBX_ENOMEM) {
    set_ctx_error(ctx, "out of memory");
    return rc;
  } else if (rc != SBX_OK) {
    set_ctx_error(ctx, "failed reading sbg timing file");
    return rc;
  }

  rc = sbx_context_load_sbg_timing_text(ctx, text, loop);
  free(text);
  return rc;
}

int
sbx_context_set_aux_tones(SbxContext *ctx, const SbxToneSpec *tones, size_t tone_count) {
  SbxToneSpec *copy = 0;
  SbxEngine **engv = 0;
  size_t i;
  char err[160];
  int rc;

  if (!ctx || !ctx->eng) return SBX_EINVAL;
  if (tone_count > SBX_MAX_AUX_TONES) {
    set_ctx_error(ctx, "too many aux tones");
    return SBX_EINVAL;
  }
  if (tone_count == 0) {
    ctx_clear_aux_tones(ctx);
    set_ctx_error(ctx, NULL);
    return SBX_OK;
  }
  if (!tones) {
    set_ctx_error(ctx, "aux tone list is null");
    return SBX_EINVAL;
  }

  copy = (SbxToneSpec *)calloc(tone_count, sizeof(*copy));
  if (!copy) {
    set_ctx_error(ctx, "out of memory");
    return SBX_ENOMEM;
  }
  engv = (SbxEngine **)calloc(tone_count, sizeof(*engv));
  if (!engv) {
    free(copy);
    set_ctx_error(ctx, "out of memory");
    return SBX_ENOMEM;
  }

  for (i = 0; i < tone_count; i++) {
    copy[i] = tones[i];
    rc = normalize_tone(&copy[i], err, sizeof(err));
    if (rc != SBX_OK) {
      size_t j;
      for (j = 0; j < i; j++) {
        if (engv[j]) sbx_engine_destroy(engv[j]);
      }
      free(engv);
      free(copy);
      set_ctx_error(ctx, err);
      return rc;
    }
    engv[i] = sbx_engine_create(&ctx->eng->cfg);
    if (!engv[i]) {
      size_t j;
      for (j = 0; j < i; j++) {
        if (engv[j]) sbx_engine_destroy(engv[j]);
      }
      free(engv);
      free(copy);
      set_ctx_error(ctx, "out of memory");
      return SBX_ENOMEM;
    }
    engine_set_custom_waves(engv[i], ctx->custom_waves);
    rc = engine_apply_tone(engv[i], &copy[i], 1);
    if (rc != SBX_OK) {
      size_t j;
      char emsg_local[256];
      const char *emsg = sbx_engine_last_error(engv[i]);
      snprintf(emsg_local, sizeof(emsg_local), "%s", emsg ? emsg : "invalid aux tone");
      for (j = 0; j <= i; j++) {
        if (engv[j]) sbx_engine_destroy(engv[j]);
      }
      free(engv);
      free(copy);
      set_ctx_error(ctx, emsg_local);
      return rc;
    }
  }

  ctx_clear_aux_tones(ctx);
  ctx->aux_tones = copy;
  ctx->aux_eng = engv;
  ctx->aux_count = tone_count;
  set_ctx_error(ctx, NULL);
  return SBX_OK;
}

size_t
sbx_context_aux_tone_count(const SbxContext *ctx) {
  if (!ctx || !ctx->aux_tones) return 0;
  return ctx->aux_count;
}

int
sbx_context_get_aux_tone(const SbxContext *ctx, size_t index, SbxToneSpec *out) {
  if (!ctx || !out || !ctx->aux_tones) return SBX_EINVAL;
  if (index >= ctx->aux_count) return SBX_EINVAL;
  *out = ctx->aux_tones[index];
  return SBX_OK;
}

int
sbx_context_set_mix_effects(SbxContext *ctx, const SbxMixFxSpec *fxv, size_t fx_count) {
  SbxMixFxState *states = 0;
  size_t i;
  if (!ctx || !ctx->eng) return SBX_EINVAL;
  if (fx_count > SBX_MAX_AUX_TONES) {
    set_ctx_error(ctx, "too many mix effects");
    return SBX_EINVAL;
  }
  if (fx_count == 0) {
    ctx_clear_mix_effects(ctx);
    set_ctx_error(ctx, NULL);
    return SBX_OK;
  }
  if (!fxv) {
    set_ctx_error(ctx, "mix effect list is null");
    return SBX_EINVAL;
  }

  states = (SbxMixFxState *)calloc(fx_count, sizeof(*states));
  if (!states) {
    set_ctx_error(ctx, "out of memory");
    return SBX_ENOMEM;
  }
  for (i = 0; i < fx_count; i++) {
    states[i].spec = fxv[i];
    if (states[i].spec.type < SBX_MIXFX_NONE || states[i].spec.type > SBX_MIXFX_AM) {
      free(states);
      set_ctx_error(ctx, "invalid mix effect type");
      return SBX_EINVAL;
    }
    if (states[i].spec.waveform < SBX_WAVE_SINE || states[i].spec.waveform > SBX_WAVE_SAWTOOTH) {
      free(states);
      set_ctx_error(ctx, "invalid mix effect waveform");
      return SBX_EINVAL;
    }
    if (!isfinite(states[i].spec.carr) || !isfinite(states[i].spec.res) ||
        !isfinite(states[i].spec.amp) || states[i].spec.amp < 0.0) {
      free(states);
      set_ctx_error(ctx, "invalid mix effect parameter");
      return SBX_EINVAL;
    }
    if (states[i].spec.type == SBX_MIXFX_AM &&
        sbx_validate_mixam_fields(&states[i].spec) != SBX_OK) {
      free(states);
      set_ctx_error(ctx, "invalid mixam envelope parameter");
      return SBX_EINVAL;
    }
    sbx_mix_fx_reset_state(&states[i]);
  }

  ctx_clear_mix_effects(ctx);
  ctx->mix_fx = states;
  ctx->mix_fx_count = fx_count;
  set_ctx_error(ctx, NULL);
  return SBX_OK;
}

size_t
sbx_context_mix_effect_count(const SbxContext *ctx) {
  if (!ctx || !ctx->mix_fx) return 0;
  return ctx->mix_fx_count;
}

int
sbx_context_get_mix_effect(const SbxContext *ctx, size_t index, SbxMixFxSpec *out_fx) {
  if (!ctx || !out_fx || !ctx->mix_fx) return SBX_EINVAL;
  if (index >= ctx->mix_fx_count) return SBX_EINVAL;
  *out_fx = ctx->mix_fx[index].spec;
  return SBX_OK;
}

static int
sbx_validate_mix_fx_spec_fields(const SbxMixFxSpec *spec) {
  if (!spec) return SBX_EINVAL;
  if (spec->type == SBX_MIXFX_NONE) return SBX_OK;
  if (spec->type < SBX_MIXFX_NONE || spec->type > SBX_MIXFX_AM) return SBX_EINVAL;
  if (spec->waveform < SBX_WAVE_SINE || spec->waveform > SBX_WAVE_SAWTOOTH) return SBX_EINVAL;
  if (!isfinite(spec->carr) || !isfinite(spec->res) ||
      !isfinite(spec->amp) || spec->amp < 0.0) return SBX_EINVAL;
  if (spec->type == SBX_MIXFX_AM &&
      sbx_validate_mixam_fields(spec) != SBX_OK) return SBX_EINVAL;
  return SBX_OK;
}

static void
sbx_mixfx_state_assign_spec(SbxMixFxState *state, const SbxMixFxSpec *spec) {
  int reset = 0;
  if (!state || !spec) return;
  if (state->spec.type != spec->type ||
      state->spec.waveform != spec->waveform ||
      (state->spec.type == SBX_MIXFX_AM &&
       (state->spec.mixam_mode != spec->mixam_mode ||
        state->spec.mixam_edge_mode != spec->mixam_edge_mode))) {
    reset = 1;
  }
  if (reset) sbx_mix_fx_reset_state(state);
  state->spec = *spec;
}

static void
sbx_default_mix_fx_spec(SbxMixFxSpec *fx) {
  if (!fx) return;
  memset(fx, 0, sizeof(*fx));
  fx->type = SBX_MIXFX_NONE;
  fx->waveform = SBX_WAVE_SINE;
}

static void
sbx_interp_mix_fx_spec(const SbxMixFxSpec *a,
                       const SbxMixFxSpec *b,
                       int interp,
                       double u,
                       SbxMixFxSpec *out) {
  if (!out) return;
  sbx_default_mix_fx_spec(out);
  if (!a && !b) return;
  if (!b || interp == SBX_INTERP_STEP || u <= 0.0) {
    if (a) *out = *a;
    return;
  }
  if (!a) {
    *out = *b;
    out->amp *= sbx_dsp_clamp(u, 0.0, 1.0);
    return;
  }
  if (a->type == SBX_MIXFX_NONE && b->type == SBX_MIXFX_NONE) return;
  if (a->type == SBX_MIXFX_NONE) {
    *out = *b;
    out->amp *= sbx_dsp_clamp(u, 0.0, 1.0);
    return;
  }
  if (b->type == SBX_MIXFX_NONE) {
    *out = *a;
    out->amp *= sbx_dsp_clamp(1.0 - u, 0.0, 1.0);
    return;
  }
  if (a->type == b->type && a->waveform == b->waveform) {
    *out = *a;
    out->carr = sbx_lerp(a->carr, b->carr, u);
    out->res = sbx_lerp(a->res, b->res, u);
    out->amp = sbx_lerp(a->amp, b->amp, u);
    if (a->type == SBX_MIXFX_AM) {
      out->mixam_bind_program_beat =
          (a->mixam_bind_program_beat == b->mixam_bind_program_beat) ?
          a->mixam_bind_program_beat : (u < 0.5 ? a->mixam_bind_program_beat : b->mixam_bind_program_beat);
      out->mixam_mode =
          (a->mixam_mode == b->mixam_mode) ? a->mixam_mode : (u < 0.5 ? a->mixam_mode : b->mixam_mode);
      out->mixam_edge_mode =
          (a->mixam_edge_mode == b->mixam_edge_mode) ? a->mixam_edge_mode :
          (u < 0.5 ? a->mixam_edge_mode : b->mixam_edge_mode);
      out->mixam_start = sbx_lerp(a->mixam_start, b->mixam_start, u);
      out->mixam_duty = sbx_lerp(a->mixam_duty, b->mixam_duty, u);
      out->mixam_attack = sbx_lerp(a->mixam_attack, b->mixam_attack, u);
      out->mixam_release = sbx_lerp(a->mixam_release, b->mixam_release, u);
      out->mixam_floor = sbx_lerp(a->mixam_floor, b->mixam_floor, u);
    }
    return;
  }
  if (u < 0.5) {
    *out = *a;
    out->amp *= sbx_dsp_clamp(1.0 - (u * 2.0), 0.0, 1.0);
  } else {
    *out = *b;
    out->amp *= sbx_dsp_clamp((u - 0.5) * 2.0, 0.0, 1.0);
  }
}

static int
ctx_set_sbg_mix_effect_keyframes_internal(SbxContext *ctx,
                                          const SbxMixFxKeyframe *kfs,
                                          size_t kf_count,
                                          size_t slot_count) {
  SbxMixFxKeyframe *copy = 0;
  SbxMixFxState *states = 0;
  size_t i, j;
  if (!ctx || !ctx->eng) return SBX_EINVAL;
  if (kf_count == 0 || slot_count == 0) {
    ctx_clear_sbg_mix_effect_keyframes(ctx);
    set_ctx_error(ctx, NULL);
    return SBX_OK;
  }
  if (!kfs) {
    set_ctx_error(ctx, "sbg mix effect keyframe list is null");
    return SBX_EINVAL;
  }
  if (slot_count > SBX_MAX_SBG_MIXFX) {
    set_ctx_error(ctx, "too many sbg mix-effect slots");
    return SBX_EINVAL;
  }
  copy = (SbxMixFxKeyframe *)calloc(kf_count, sizeof(*copy));
  states = (SbxMixFxState *)calloc(slot_count, sizeof(*states));
  if (!copy || !states) {
    if (copy) free(copy);
    if (states) free(states);
    set_ctx_error(ctx, "out of memory");
    return SBX_ENOMEM;
  }
  for (i = 0; i < kf_count; i++) {
    copy[i] = kfs[i];
    if (!isfinite(copy[i].time_sec) || copy[i].time_sec < 0.0) {
      free(copy);
      free(states);
      set_ctx_error(ctx, "sbg mix-effect keyframe time_sec must be finite and >= 0");
      return SBX_EINVAL;
    }
    if (i > 0 && !(copy[i].time_sec > copy[i - 1].time_sec)) {
      free(copy);
      free(states);
      set_ctx_error(ctx, "sbg mix-effect keyframe time_sec values must be strictly increasing");
      return SBX_EINVAL;
    }
    if (copy[i].interp != SBX_INTERP_LINEAR && copy[i].interp != SBX_INTERP_STEP) {
      free(copy);
      free(states);
      set_ctx_error(ctx, "sbg mix-effect keyframe interp must be SBX_INTERP_LINEAR or SBX_INTERP_STEP");
      return SBX_EINVAL;
    }
    if (copy[i].mix_fx_count > slot_count) {
      free(copy);
      free(states);
      set_ctx_error(ctx, "sbg mix-effect keyframe exceeds slot count");
      return SBX_EINVAL;
    }
    for (j = 0; j < copy[i].mix_fx_count; j++) {
      if (sbx_validate_mix_fx_spec_fields(&copy[i].mix_fx[j]) != SBX_OK) {
        free(copy);
        free(states);
        set_ctx_error(ctx, "invalid sbg mix-effect keyframe spec");
        return SBX_EINVAL;
      }
    }
    for (; j < SBX_MAX_SBG_MIXFX; j++)
      sbx_default_mix_fx_spec(&copy[i].mix_fx[j]);
  }
  for (i = 0; i < slot_count; i++) {
    sbx_default_mix_fx_spec(&states[i].spec);
    sbx_mix_fx_reset_state(&states[i]);
  }
  ctx_clear_sbg_mix_effect_keyframes(ctx);
  ctx->sbg_mix_fx_kf = copy;
  ctx->sbg_mix_fx_kf_count = kf_count;
  ctx->sbg_mix_fx_slots = slot_count;
  ctx->sbg_mix_fx_seg = 0;
  ctx->sbg_mix_fx_state = states;
  set_ctx_error(ctx, NULL);
  return SBX_OK;
}

static int
ctx_eval_sbg_mix_effects(SbxContext *ctx, double t_sec, SbxMixFxSpec *out_fxv, size_t out_slots) {
  size_t n, i0, i1, slot;
  SbxMixFxKeyframe *k0;
  SbxMixFxKeyframe *k1;
  double t0, t1, u;
  if (!ctx || !out_fxv) return SBX_EINVAL;
  if (!ctx->sbg_mix_fx_kf || ctx->sbg_mix_fx_kf_count == 0 || out_slots < ctx->sbg_mix_fx_slots)
    return SBX_OK;
  n = ctx->sbg_mix_fx_kf_count;
  if (n == 1 || t_sec <= ctx->sbg_mix_fx_kf[0].time_sec) {
    for (slot = 0; slot < ctx->sbg_mix_fx_slots; slot++) {
      if (slot < ctx->sbg_mix_fx_kf[0].mix_fx_count) out_fxv[slot] = ctx->sbg_mix_fx_kf[0].mix_fx[slot];
      else sbx_default_mix_fx_spec(&out_fxv[slot]);
    }
    return SBX_OK;
  }
  if (t_sec >= ctx->sbg_mix_fx_kf[n - 1].time_sec) {
    for (slot = 0; slot < ctx->sbg_mix_fx_slots; slot++) {
      if (slot < ctx->sbg_mix_fx_kf[n - 1].mix_fx_count) out_fxv[slot] = ctx->sbg_mix_fx_kf[n - 1].mix_fx[slot];
      else sbx_default_mix_fx_spec(&out_fxv[slot]);
    }
    return SBX_OK;
  }
  if (ctx->sbg_mix_fx_seg >= n - 1) ctx->sbg_mix_fx_seg = n - 2;
  while (ctx->sbg_mix_fx_seg + 1 < n &&
         t_sec > ctx->sbg_mix_fx_kf[ctx->sbg_mix_fx_seg + 1].time_sec)
    ctx->sbg_mix_fx_seg++;
  while (ctx->sbg_mix_fx_seg > 0 &&
         t_sec < ctx->sbg_mix_fx_kf[ctx->sbg_mix_fx_seg].time_sec)
    ctx->sbg_mix_fx_seg--;
  i0 = ctx->sbg_mix_fx_seg;
  i1 = i0 + 1;
  if (i1 >= n) i1 = n - 1;
  k0 = &ctx->sbg_mix_fx_kf[i0];
  k1 = &ctx->sbg_mix_fx_kf[i1];
  t0 = k0->time_sec;
  t1 = k1->time_sec;
  u = (t1 > t0) ? (t_sec - t0) / (t1 - t0) : 0.0;
  u = sbx_dsp_clamp(u, 0.0, 1.0);
  for (slot = 0; slot < ctx->sbg_mix_fx_slots; slot++) {
    const SbxMixFxSpec *a = (slot < k0->mix_fx_count) ? &k0->mix_fx[slot] : 0;
    const SbxMixFxSpec *b = (slot < k1->mix_fx_count) ? &k1->mix_fx[slot] : 0;
    sbx_interp_mix_fx_spec(a, b, k0->interp, u, &out_fxv[slot]);
  }
  return SBX_OK;
}

static double
sbx_mixam_gain_step(SbxMixFxState *fx, double sr, double res_hz) {
  double p;
  double g;
  if (!fx || sr <= 0.0 || !isfinite(res_hz) || res_hz <= 0.0) return 1.0;
  fx->phase = sbx_dsp_wrap_unit(fx->phase + res_hz / sr);
  if (fx->spec.mixam_mode == SBX_MIXAM_MODE_COS) {
    double ph = sbx_dsp_wrap_unit(fx->phase + fx->spec.mixam_start);
    p = 0.5 * (1.0 + cos(SBX_TAU * ph));
  } else {
    p = sbx_dsp_iso_mod_factor_custom(fx->phase,
                                      fx->spec.mixam_start,
                                      fx->spec.mixam_duty,
                                      fx->spec.mixam_attack,
                                      fx->spec.mixam_release,
                                      fx->spec.mixam_edge_mode);
  }
  g = fx->spec.mixam_floor + (1.0 - fx->spec.mixam_floor) * p;
  return sbx_dsp_clamp(g, fx->spec.mixam_floor, 1.0);
}

static void
sbx_apply_one_mix_effect(SbxMixFxState *fx,
                         const SbxMixFxSpec *spec,
                         double sr,
                         double mix_l,
                         double mix_r,
                         double base_amp,
                         double *out_add_l,
                         double *out_add_r) {
  if (!fx || !spec || !out_add_l || !out_add_r) return;
  sbx_mixfx_state_assign_spec(fx, spec);
  switch (fx->spec.type) {
    case SBX_MIXFX_SPIN: {
      double wav, val, intensity, amplified, pos, fx_l, fx_r;
      fx->phase = sbx_dsp_wrap_unit(fx->phase + fx->spec.res / sr);
      wav = sbx_wave_sample_unit_phase(fx->spec.waveform, fx->phase);
      val = fx->spec.carr * 1.0e-6 * sr * wav;
      intensity = 0.5 + fx->spec.amp * 3.5;
      amplified = sbx_dsp_clamp(val * intensity, -128.0, 127.0);
      pos = fabs(amplified);
      if (amplified >= 0.0) {
        fx_l = (mix_l * (128.0 - pos)) / 128.0;
        fx_r = mix_r + (mix_l * pos) / 128.0;
      } else {
        fx_l = mix_l + (mix_r * pos) / 128.0;
        fx_r = (mix_r * (128.0 - pos)) / 128.0;
      }
      *out_add_l += base_amp * fx_l;
      *out_add_r += base_amp * fx_r;
      break;
    }
    case SBX_MIXFX_PULSE: {
      double wav, mod_factor = 0.0, effect_intensity, gain;
      fx->phase = sbx_dsp_wrap_unit(fx->phase + fx->spec.res / sr);
      wav = sbx_wave_sample_unit_phase(fx->spec.waveform, fx->phase);
      if (wav > 0.3) {
        mod_factor = (wav - 0.3) / 0.7;
        mod_factor = sbx_dsp_smoothstep01(mod_factor);
      }
      effect_intensity = sbx_dsp_clamp(fx->spec.amp, 0.0, 1.0);
      gain = (1.0 - effect_intensity) + (effect_intensity * mod_factor);
      *out_add_l += base_amp * mix_l * gain;
      *out_add_r += base_amp * mix_r * gain;
      break;
    }
    case SBX_MIXFX_BEAT: {
      double s, c, mono, q, up, down, effect_intensity, fx_l, fx_r;
      fx->phase = sbx_dsp_wrap_unit(fx->phase + (fx->spec.res * 0.5) / sr);
      s = sbx_wave_sample_unit_phase(fx->spec.waveform, fx->phase);
      c = sbx_wave_sample_unit_phase(fx->spec.waveform, fx->phase + 0.25);
      mono = 0.5 * (mix_l + mix_r);
      q = sbx_mixbeat_hilbert_step(fx, mono);
      up = mono * c - q * s;
      down = mono * c + q * s;
      effect_intensity = sbx_dsp_clamp(fx->spec.amp, 0.0, 1.0);
      fx_l = mix_l * (1.0 - effect_intensity) + down * effect_intensity;
      fx_r = mix_r * (1.0 - effect_intensity) + up * effect_intensity;
      *out_add_l += base_amp * fx_l;
      *out_add_r += base_amp * fx_r;
      break;
    }
    default:
      break;
  }
}

int
sbx_context_apply_mix_effects(SbxContext *ctx,
                              double mix_l,
                              double mix_r,
                              double base_amp,
                              double *out_add_l,
                              double *out_add_r) {
  size_t i;
  double sr;
  if (!ctx || !ctx->eng || !out_add_l || !out_add_r) return SBX_EINVAL;
  *out_add_l = 0.0;
  *out_add_r = 0.0;
  if (ctx->mix_fx_count == 0) return SBX_OK;

  sr = ctx->eng->cfg.sample_rate;
  if (!(isfinite(sr) && sr > 0.0)) {
    set_ctx_error(ctx, "engine configuration is invalid");
    return SBX_ENOTREADY;
  }

  for (i = 0; i < ctx->mix_fx_count; i++) {
    if (ctx->mix_fx[i].spec.type == SBX_MIXFX_AM)
      continue;
    sbx_apply_one_mix_effect(&ctx->mix_fx[i], &ctx->mix_fx[i].spec, sr,
                             mix_l, mix_r, base_amp, out_add_l, out_add_r);
  }
  return SBX_OK;
}

int
sbx_context_mix_stream_sample(SbxContext *ctx,
                              double t_sec,
                              int mix_l_sample,
                              int mix_r_sample,
                              double mix_mod_mul,
                              double *out_add_l,
                              double *out_add_r) {
  double mix_l;
  double mix_r;
  double mix_pct;
  double mix_mul;
  double am_gain = 1.0;
  double program_beat_hz = 0.0;
  size_t i;
  int have_am = 0;
  int need_program_beat = 0;
  double sr = 0.0;
  SbxMixFxSpec dyn_fx[SBX_MAX_SBG_MIXFX];

  if (!ctx || !out_add_l || !out_add_r) return SBX_EINVAL;
  if (!isfinite(mix_mod_mul)) mix_mod_mul = 1.0;

  mix_l = (double)(mix_l_sample >> 4);
  mix_r = (double)(mix_r_sample >> 4);
  mix_pct = sbx_context_mix_amp_at(ctx, t_sec);
  mix_mul = (mix_pct / 100.0) * mix_mod_mul;

  *out_add_l = mix_l * mix_mul;
  *out_add_r = mix_r * mix_mul;

  if (sbx_context_mix_effect_count(ctx) > 0 || ctx->sbg_mix_fx_slots > 0) {
    double fx_add_l = 0.0;
    double fx_add_r = 0.0;
    if (sbx_context_mix_effect_count(ctx) > 0) {
      int rc = sbx_context_apply_mix_effects(ctx, mix_l, mix_r, mix_mul * 0.7,
                                             &fx_add_l, &fx_add_r);
      if (rc != SBX_OK) return rc;
      *out_add_l += fx_add_l;
      *out_add_r += fx_add_r;
      fx_add_l = 0.0;
      fx_add_r = 0.0;
    }

    sr = ctx->eng ? ctx->eng->cfg.sample_rate : 0.0;
    if (!(isfinite(sr) && sr > 0.0)) {
      set_ctx_error(ctx, "engine configuration is invalid");
      return SBX_ENOTREADY;
    }
    for (i = 0; i < ctx->mix_fx_count; i++) {
      if (ctx->mix_fx[i].spec.type == SBX_MIXFX_AM &&
          ctx->mix_fx[i].spec.mixam_bind_program_beat) {
        need_program_beat = 1;
        break;
      }
    }
    if (need_program_beat)
      program_beat_hz = ctx_eval_program_beat_hz(ctx, t_sec);
    for (i = 0; i < ctx->mix_fx_count; i++) {
      if (ctx->mix_fx[i].spec.type == SBX_MIXFX_AM) {
        double am_res_hz = ctx->mix_fx[i].spec.mixam_bind_program_beat ?
          program_beat_hz : ctx->mix_fx[i].spec.res;
        double g = sbx_mixam_gain_step(&ctx->mix_fx[i], sr, am_res_hz);
        am_gain *= g;
        if (isfinite(am_res_hz) && am_res_hz > 0.0)
          have_am = 1;
      }
    }
    if (ctx->sbg_mix_fx_slots > 0) {
      int rc = ctx_eval_sbg_mix_effects(ctx, t_sec, dyn_fx, SBX_MAX_SBG_MIXFX);
      if (rc != SBX_OK) return rc;
      for (i = 0; i < ctx->sbg_mix_fx_slots; i++) {
        if (dyn_fx[i].type == SBX_MIXFX_NONE || dyn_fx[i].amp <= 0.0)
          continue;
        if (dyn_fx[i].type == SBX_MIXFX_AM) {
          if (dyn_fx[i].mixam_bind_program_beat && !need_program_beat) {
            program_beat_hz = ctx_eval_program_beat_hz(ctx, t_sec);
            need_program_beat = 1;
          }
          double am_res_hz = dyn_fx[i].mixam_bind_program_beat ? program_beat_hz : dyn_fx[i].res;
          sbx_mixfx_state_assign_spec(&ctx->sbg_mix_fx_state[i], &dyn_fx[i]);
          am_gain *= sbx_mixam_gain_step(&ctx->sbg_mix_fx_state[i], sr, am_res_hz);
          if (isfinite(am_res_hz) && am_res_hz > 0.0)
            have_am = 1;
        } else {
          sbx_apply_one_mix_effect(&ctx->sbg_mix_fx_state[i], &dyn_fx[i], sr,
                                   mix_l, mix_r, mix_mul * 0.7, &fx_add_l, &fx_add_r);
        }
      }
      *out_add_l += fx_add_l;
      *out_add_r += fx_add_r;
    }
    if (have_am) {
      *out_add_l *= am_gain;
      *out_add_r *= am_gain;
    }
  }

  return SBX_OK;
}

int
sbx_context_set_mix_amp_keyframes(SbxContext *ctx,
                                  const SbxMixAmpKeyframe *kfs,
                                  size_t kf_count,
                                  double default_amp_pct) {
  SbxMixAmpKeyframe *copy = 0;
  size_t i;
  if (!ctx || !ctx->eng) return SBX_EINVAL;
  if (!isfinite(default_amp_pct)) {
    set_ctx_error(ctx, "invalid default mix amp");
    return SBX_EINVAL;
  }

  if (kf_count > 0 && !kfs) {
    set_ctx_error(ctx, "mix amp keyframe list is null");
    return SBX_EINVAL;
  }

  if (kf_count > 0) {
    copy = (SbxMixAmpKeyframe *)calloc(kf_count, sizeof(*copy));
    if (!copy) {
      set_ctx_error(ctx, "out of memory");
      return SBX_ENOMEM;
    }
    for (i = 0; i < kf_count; i++) {
      copy[i] = kfs[i];
      if (!isfinite(copy[i].time_sec) || copy[i].time_sec < 0.0) {
        free(copy);
        set_ctx_error(ctx, "mix amp keyframe time_sec must be finite and >= 0");
        return SBX_EINVAL;
      }
      if (i > 0 && !(copy[i].time_sec > copy[i - 1].time_sec)) {
        free(copy);
        set_ctx_error(ctx, "mix amp keyframe time_sec values must be strictly increasing");
        return SBX_EINVAL;
      }
      if (copy[i].interp != SBX_INTERP_LINEAR &&
          copy[i].interp != SBX_INTERP_STEP) {
        free(copy);
        set_ctx_error(ctx, "mix amp keyframe interp must be SBX_INTERP_LINEAR or SBX_INTERP_STEP");
        return SBX_EINVAL;
      }
      if (!isfinite(copy[i].amp_pct)) {
        free(copy);
        set_ctx_error(ctx, "mix amp keyframe amp_pct must be finite");
        return SBX_EINVAL;
      }
    }
  }

  ctx_clear_mix_keyframes(ctx);
  ctx->mix_default_amp_pct = default_amp_pct;
  ctx->mix_kf = copy;
  ctx->mix_kf_count = kf_count;
  ctx->mix_kf_seg = 0;
  set_ctx_error(ctx, NULL);
  return SBX_OK;
}

int
sbx_context_configure_runtime(SbxContext *ctx,
                              const SbxMixAmpKeyframe *mix_kfs,
                              size_t mix_kf_count,
                              double default_mix_amp_pct,
                              const SbxMixFxSpec *mix_fx,
                              size_t mix_fx_count,
                              const SbxToneSpec *aux_tones,
                              size_t aux_count) {
  int rc;
  if (!ctx || !ctx->eng) return SBX_EINVAL;
  rc = sbx_context_set_mix_amp_keyframes(ctx, mix_kfs, mix_kf_count,
                                         default_mix_amp_pct);
  if (rc != SBX_OK) return rc;
  rc = sbx_context_set_mix_effects(ctx, mix_fx, mix_fx_count);
  if (rc != SBX_OK) return rc;
  rc = sbx_context_set_aux_tones(ctx, aux_tones, aux_count);
  if (rc != SBX_OK) return rc;
  set_ctx_error(ctx, NULL);
  return SBX_OK;
}

double
sbx_context_mix_amp_at(SbxContext *ctx, double t_sec) {
  size_t n, i0, i1;
  double t0, t1, u;
  SbxMixAmpKeyframe *k0, *k1;

  if (!ctx) return 100.0;
  n = ctx->mix_kf_count;
  if (!ctx->mix_kf || n == 0) return ctx->mix_default_amp_pct;
  if (n == 1 || t_sec <= ctx->mix_kf[0].time_sec) return ctx->mix_kf[0].amp_pct;
  if (t_sec >= ctx->mix_kf[n - 1].time_sec) return ctx->mix_kf[n - 1].amp_pct;

  if (ctx->mix_kf_seg >= n - 1) ctx->mix_kf_seg = n - 2;
  while (ctx->mix_kf_seg + 1 < n &&
         t_sec > ctx->mix_kf[ctx->mix_kf_seg + 1].time_sec)
    ctx->mix_kf_seg++;
  while (ctx->mix_kf_seg > 0 &&
         t_sec < ctx->mix_kf[ctx->mix_kf_seg].time_sec)
    ctx->mix_kf_seg--;

  i0 = ctx->mix_kf_seg;
  i1 = i0 + 1;
  if (i1 >= n) i1 = n - 1;
  k0 = &ctx->mix_kf[i0];
  k1 = &ctx->mix_kf[i1];
  t0 = k0->time_sec;
  t1 = k1->time_sec;
  if (t1 <= t0) return k0->amp_pct;
  if (t_sec >= t1) return k1->amp_pct;
  u = (t_sec - t0) / (t1 - t0);
  if (u < 0.0) u = 0.0;
  if (u > 1.0) u = 1.0;
  if (k0->interp == SBX_INTERP_STEP) u = 0.0;
  return k0->amp_pct + (k1->amp_pct - k0->amp_pct) * u;
}

size_t
sbx_context_mix_amp_keyframe_count(const SbxContext *ctx) {
  if (!ctx || !ctx->mix_kf) return 0;
  return ctx->mix_kf_count;
}

int
sbx_context_get_mix_amp_keyframe(const SbxContext *ctx,
                                 size_t index,
                                 SbxMixAmpKeyframe *out) {
  if (!ctx || !out || !ctx->mix_kf) return SBX_EINVAL;
  if (index >= ctx->mix_kf_count) return SBX_EINVAL;
  *out = ctx->mix_kf[index];
  return SBX_OK;
}

int
sbx_context_has_mix_amp_control(const SbxContext *ctx) {
  if (!ctx) return 0;
  return ctx->mix_kf_count > 0 ? 1 : 0;
}

int
sbx_context_has_mix_effects(const SbxContext *ctx) {
  if (!ctx) return 0;
  return (ctx->mix_fx_count > 0 || ctx->sbg_mix_fx_slots > 0) ? 1 : 0;
}

size_t
sbx_context_timed_mix_effect_keyframe_count(const SbxContext *ctx) {
  if (!ctx || !ctx->sbg_mix_fx_kf) return 0;
  return ctx->sbg_mix_fx_kf_count;
}

size_t
sbx_context_timed_mix_effect_slot_count(const SbxContext *ctx) {
  if (!ctx || !ctx->sbg_mix_fx_kf) return 0;
  return ctx->sbg_mix_fx_slots;
}

int
sbx_context_get_timed_mix_effect_keyframe_info(const SbxContext *ctx,
                                               size_t index,
                                               SbxTimedMixFxKeyframeInfo *out) {
  if (!ctx || !out || !ctx->sbg_mix_fx_kf) return SBX_EINVAL;
  if (index >= ctx->sbg_mix_fx_kf_count) return SBX_EINVAL;
  out->time_sec = ctx->sbg_mix_fx_kf[index].time_sec;
  out->interp = ctx->sbg_mix_fx_kf[index].interp;
  return SBX_OK;
}

int
sbx_context_get_timed_mix_effect_slot(const SbxContext *ctx,
                                      size_t keyframe_index,
                                      size_t slot_index,
                                      SbxMixFxSpec *out_fx,
                                      int *out_present) {
  const SbxMixFxKeyframe *kf;
  if (!ctx || !out_fx || !out_present || !ctx->sbg_mix_fx_kf) return SBX_EINVAL;
  if (keyframe_index >= ctx->sbg_mix_fx_kf_count) return SBX_EINVAL;
  if (slot_index >= ctx->sbg_mix_fx_slots) return SBX_EINVAL;
  kf = &ctx->sbg_mix_fx_kf[keyframe_index];
  if (slot_index < kf->mix_fx_count) {
    *out_fx = kf->mix_fx[slot_index];
    *out_present = 1;
  } else {
    sbx_default_mix_fx_spec(out_fx);
    *out_present = 0;
  }
  return SBX_OK;
}

size_t
sbx_context_keyframe_count(const SbxContext *ctx) {
  if (!ctx || !ctx->kfs) return 0;
  return ctx->kf_count;
}

size_t
sbx_context_voice_count(const SbxContext *ctx) {
  if (!ctx || !ctx->kfs || ctx->kf_count == 0) return 0;
  if (ctx->mv_voice_count == 0) return 1;
  return ctx->mv_voice_count;
}

int
sbx_context_get_keyframe(const SbxContext *ctx, size_t index, SbxProgramKeyframe *out) {
  if (!ctx || !out || !ctx->kfs) return SBX_EINVAL;
  if (index >= ctx->kf_count) return SBX_EINVAL;
  *out = ctx->kfs[index];
  return SBX_OK;
}

int
sbx_context_get_keyframe_voice(const SbxContext *ctx,
                               size_t index,
                               size_t voice_index,
                               SbxProgramKeyframe *out) {
  if (!ctx || !out || !ctx->kfs) return SBX_EINVAL;
  if (index >= ctx->kf_count) return SBX_EINVAL;
  if (voice_index >= sbx_context_voice_count(ctx)) return SBX_EINVAL;
  if (voice_index == 0 || !ctx->mv_kfs) {
    *out = ctx->kfs[index];
    return SBX_OK;
  }
  *out = SBX_MV_KF(ctx, voice_index)[index];
  return SBX_OK;
}

double
sbx_context_duration_sec(const SbxContext *ctx) {
  if (!ctx || ctx->source_mode != SBX_CTX_SRC_KEYFRAMES ||
      !ctx->kfs || ctx->kf_count == 0)
    return 0.0;
  return ctx->kf_duration_sec;
}

int
sbx_context_sample_tones(SbxContext *ctx,
                         double t0_sec,
                         double t1_sec,
                         size_t sample_count,
                         double *out_t_sec,
                         SbxToneSpec *out_tones) {
  size_t i;
  size_t seg_saved;
  if (!ctx || !ctx->eng || !out_tones || sample_count == 0)
    return SBX_EINVAL;
  if (!ctx->loaded) {
    set_ctx_error(ctx, "no tone/program loaded");
    return SBX_ENOTREADY;
  }
  if (!isfinite(t0_sec) || !isfinite(t1_sec)) {
    set_ctx_error(ctx, "sampling times must be finite");
    return SBX_EINVAL;
  }

  if (ctx->source_mode != SBX_CTX_SRC_KEYFRAMES) {
    for (i = 0; i < sample_count; i++) {
      double u = (sample_count <= 1) ? 0.0 : (double)i / (double)(sample_count - 1);
      double ts = sbx_lerp(t0_sec, t1_sec, u);
      out_tones[i] = ctx->eng->tone;
      if (out_t_sec) out_t_sec[i] = ts;
    }
    set_ctx_error(ctx, NULL);
    return SBX_OK;
  }

  if (!ctx->kfs || ctx->kf_count == 0) {
    set_ctx_error(ctx, "no keyframes loaded");
    return SBX_ENOTREADY;
  }

  seg_saved = ctx->kf_seg;
  for (i = 0; i < sample_count; i++) {
    double u = (sample_count <= 1) ? 0.0 : (double)i / (double)(sample_count - 1);
    double ts = sbx_lerp(t0_sec, t1_sec, u);
    double eval_t = ts;
    if (ctx->kf_loop && ctx->kf_duration_sec > 0.0) {
      eval_t = fmod(eval_t, ctx->kf_duration_sec);
      if (eval_t < 0.0) eval_t += ctx->kf_duration_sec;
    }
    ctx_eval_keyframed_tone_at(ctx->kfs, ctx->kf_count, eval_t, &ctx->kf_seg,
                               &out_tones[i]);
    if (out_t_sec) out_t_sec[i] = ts;
  }
  ctx->kf_seg = seg_saved;
  set_ctx_error(ctx, NULL);
  return SBX_OK;
}

int
sbx_context_render_f32(SbxContext *ctx, float *out, size_t frames) {
  int rc;
  size_t i;
  double sr;
  if (!ctx || !ctx->eng || !out) return SBX_EINVAL;
  if (!ctx->loaded) {
    set_ctx_error(ctx, "no tone/program loaded");
    return SBX_ENOTREADY;
  }

  if (ctx->source_mode != SBX_CTX_SRC_KEYFRAMES) {
    rc = sbx_engine_render_f32(ctx->eng, out, frames);
    if (rc != SBX_OK) {
      set_ctx_error(ctx, sbx_engine_last_error(ctx->eng));
      return rc;
    }
    if (ctx->aux_count > 0) {
      size_t ai;
      size_t nfloat = frames * (size_t)ctx->eng->cfg.channels;
      if (nfloat > ctx->aux_buf_cap) {
        float *tmp = (float *)realloc(ctx->aux_buf, nfloat * sizeof(float));
        if (!tmp) {
          set_ctx_error(ctx, "out of memory");
          return SBX_ENOMEM;
        }
        ctx->aux_buf = tmp;
        ctx->aux_buf_cap = nfloat;
      }
      for (ai = 0; ai < ctx->aux_count; ai++) {
        size_t k;
        rc = sbx_engine_render_f32(ctx->aux_eng[ai], ctx->aux_buf, frames);
        if (rc != SBX_OK) {
          set_ctx_error(ctx, sbx_engine_last_error(ctx->aux_eng[ai]));
          return rc;
        }
        for (k = 0; k < nfloat; k++)
          out[k] += ctx->aux_buf[k];
      }
    }
    sr = ctx->eng->cfg.sample_rate;
    if (isfinite(sr) && sr > 0.0)
      ctx->t_sec += (double)frames / sr;
    return SBX_OK;
  }

  if (!ctx->kfs || ctx->kf_count == 0) {
    set_ctx_error(ctx, "no keyframes loaded");
    return SBX_ENOTREADY;
  }

  sr = ctx->eng->cfg.sample_rate;
  if (!(isfinite(sr) && sr > 0.0)) {
    set_ctx_error(ctx, "engine configuration is invalid");
    return SBX_ENOTREADY;
  }

  for (i = 0; i < frames; i++) {
    float l = 0.0f, r = 0.0f;
    SbxToneSpec tone;
    size_t vi;

    if (ctx->kf_loop && ctx->kf_duration_sec > 0.0) {
      while (ctx->t_sec >= ctx->kf_duration_sec) {
        ctx->t_sec -= ctx->kf_duration_sec;
        ctx->kf_seg = 0;
      }
    }

    ctx_eval_keyframed_tone(ctx, ctx->t_sec, &tone);
    ctx->eng->tone = tone;
    engine_render_sample(ctx->eng, &l, &r);
    for (vi = 1; vi < ctx->mv_voice_count; vi++) {
      float vl = 0.0f, vr = 0.0f;
      ctx_eval_keyframed_tone_at(SBX_MV_KF(ctx, vi), ctx->kf_count, ctx->t_sec,
                                 &ctx->kf_seg, &tone);
      ctx->mv_eng[vi - 1]->tone = tone;
      engine_render_sample(ctx->mv_eng[vi - 1], &vl, &vr);
      l += vl;
      r += vr;
    }
    if (ctx->aux_count > 0) {
      size_t ai;
      for (ai = 0; ai < ctx->aux_count; ai++) {
        float al = 0.0f, ar = 0.0f;
        engine_render_sample(ctx->aux_eng[ai], &al, &ar);
        l += al;
        r += ar;
      }
    }
    out[i * 2] = l;
    out[i * 2 + 1] = r;
    ctx->t_sec += 1.0 / sr;
  }

  return SBX_OK;
}

double
sbx_context_time_sec(const SbxContext *ctx) {
  if (!ctx) return 0.0;
  return ctx->t_sec;
}

const char *
sbx_context_last_error(const SbxContext *ctx) {
  if (!ctx) return "null context";
  if (ctx->last_error[0]) return ctx->last_error;
  return sbx_engine_last_error(ctx->eng);
}
