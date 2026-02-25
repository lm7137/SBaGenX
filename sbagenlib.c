#include "sbagenlib.h"
#include "sbagenlib_dsp.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef SBAGENLIB_VERSION
#define SBAGENLIB_VERSION "dev"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SBX_TAU (2.0 * M_PI)
#define SBX_CTX_SRC_NONE 0
#define SBX_CTX_SRC_STATIC 1
#define SBX_CTX_SRC_KEYFRAMES 2

struct SbxEngine {
  SbxEngineConfig cfg;
  SbxToneSpec tone;
  double phase_l;
  double phase_r;
  double pulse_phase;
  char last_error[256];
};

struct SbxContext {
  SbxEngine *eng;
  int loaded;
  char last_error[256];
  int source_mode;
  SbxProgramKeyframe *kfs;
  size_t kf_count;
  int kf_loop;
  size_t kf_seg;
  double kf_duration_sec;
  double t_sec;
};

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

static const char *
skip_optional_waveform_prefix(const char *spec) {
  const char *p = spec;
  if (streq_prefix_nocase(p, "sine:")) return p + 5;
  if (streq_prefix_nocase(p, "square:")) return p + 7;
  if (streq_prefix_nocase(p, "triangle:")) return p + 9;
  if (streq_prefix_nocase(p, "sawtooth:")) return p + 9;
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

static int
parse_hhmmss_token(const char *tok, double *out_sec) {
  int hh = 0, mm = 0, ss = 0;
  char tail[2];
  if (!tok || !*tok || !out_sec) return SBX_EINVAL;

  if (sscanf(tok, "%d:%d:%d%1s", &hh, &mm, &ss, tail) == 3) {
    if (hh < 0 || mm < 0 || mm >= 60 || ss < 0 || ss >= 60) return SBX_EINVAL;
    *out_sec = (double)hh * 3600.0 + (double)mm * 60.0 + (double)ss;
    return SBX_OK;
  }
  if (sscanf(tok, "%d:%d%1s", &hh, &mm, tail) == 2) {
    if (hh < 0 || mm < 0 || mm >= 60) return SBX_EINVAL;
    *out_sec = (double)hh * 3600.0 + (double)mm * 60.0;
    return SBX_OK;
  }
  return SBX_EINVAL;
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
  if (!tone) return SBX_EINVAL;
  if (err && err_sz) err[0] = 0;

  if (tone->mode < SBX_TONE_NONE || tone->mode > SBX_TONE_ISOCHRONIC) {
    if (err && err_sz) snprintf(err, err_sz, "%s", "unsupported tone mode");
    return SBX_EINVAL;
  }

  if (tone->mode != SBX_TONE_NONE) {
    if (tone->carrier_hz <= 0.0 || !isfinite(tone->carrier_hz)) {
      if (err && err_sz) snprintf(err, err_sz, "%s", "carrier_hz must be finite and > 0");
      return SBX_EINVAL;
    }
    if (!isfinite(tone->beat_hz)) {
      if (err && err_sz) snprintf(err, err_sz, "%s", "beat_hz must be finite");
      return SBX_EINVAL;
    }
    if (!isfinite(tone->amplitude)) {
      if (err && err_sz) snprintf(err, err_sz, "%s", "amplitude must be finite");
      return SBX_EINVAL;
    }
    tone->amplitude = sbx_dsp_clamp(tone->amplitude, 0.0, 1.0);
  }

  if (tone->mode == SBX_TONE_MONAURAL || tone->mode == SBX_TONE_ISOCHRONIC)
    tone->beat_hz = fabs(tone->beat_hz);

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
  char err[160];
  int rc;

  if (!eng || !tone_in) return SBX_EINVAL;
  tone = *tone_in;
  rc = normalize_tone(&tone, err, sizeof(err));
  if (rc != SBX_OK) {
    set_last_error(eng, err);
    return rc;
  }

  eng->tone = tone;
  if (reset_phase)
    sbx_engine_reset(eng);
  set_last_error(eng, NULL);
  return SBX_OK;
}

static void
engine_render_sample(SbxEngine *eng, float *out_l, float *out_r) {
  double sr = eng->cfg.sample_rate;
  double amp = eng->tone.amplitude;
  double left = 0.0, right = 0.0;

  if (eng->tone.mode == SBX_TONE_BINAURAL) {
    double f_l = eng->tone.carrier_hz + eng->tone.beat_hz * 0.5;
    double f_r = eng->tone.carrier_hz - eng->tone.beat_hz * 0.5;
    left = amp * sin(eng->phase_l);
    right = amp * sin(eng->phase_r);
    eng->phase_l = sbx_dsp_wrap_cycle(eng->phase_l + SBX_TAU * f_l / sr, SBX_TAU);
    eng->phase_r = sbx_dsp_wrap_cycle(eng->phase_r + SBX_TAU * f_r / sr, SBX_TAU);
  } else if (eng->tone.mode == SBX_TONE_MONAURAL) {
    double f1 = eng->tone.carrier_hz - eng->tone.beat_hz * 0.5;
    double f2 = eng->tone.carrier_hz + eng->tone.beat_hz * 0.5;
    double mono = 0.5 * amp * (sin(eng->phase_l) + sin(eng->phase_r));
    left = mono;
    right = mono;
    eng->phase_l = sbx_dsp_wrap_cycle(eng->phase_l + SBX_TAU * f1 / sr, SBX_TAU);
    eng->phase_r = sbx_dsp_wrap_cycle(eng->phase_r + SBX_TAU * f2 / sr, SBX_TAU);
  } else if (eng->tone.mode == SBX_TONE_ISOCHRONIC) {
    double env = 0.0;
    double duty = eng->tone.duty_cycle;
    double pos;
    double carrier;

    carrier = sin(eng->phase_l);
    eng->phase_l = sbx_dsp_wrap_cycle(eng->phase_l + SBX_TAU * eng->tone.carrier_hz / sr, SBX_TAU);

    eng->pulse_phase += eng->tone.beat_hz / sr;
    while (eng->pulse_phase >= 1.0) eng->pulse_phase -= 1.0;
    pos = eng->pulse_phase;

    env = sbx_dsp_iso_mod_factor_custom(pos, 0.0, duty, 0.15, 0.15, 2);

    left = right = amp * env * carrier;
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
  if (!ctx) return;
  if (ctx->kfs) free(ctx->kfs);
  ctx->kfs = 0;
  ctx->kf_count = 0;
  ctx->kf_loop = 0;
  ctx->kf_seg = 0;
  ctx->kf_duration_sec = 0.0;
}

static void
ctx_reset_runtime(SbxContext *ctx) {
  if (!ctx || !ctx->eng) return;
  sbx_engine_reset(ctx->eng);
  ctx->t_sec = 0.0;
  ctx->kf_seg = 0;
}

static void
ctx_eval_keyframed_tone(SbxContext *ctx, double t_sec, SbxToneSpec *out) {
  size_t i0, i1;
  double t0, t1, u;
  const SbxProgramKeyframe *k0, *k1;
  size_t n = ctx->kf_count;

  if (n == 0) {
    sbx_default_tone_spec(out);
    out->mode = SBX_TONE_NONE;
    return;
  }
  if (n == 1 || t_sec <= ctx->kfs[0].time_sec) {
    *out = ctx->kfs[0].tone;
    return;
  }
  if (t_sec >= ctx->kfs[n - 1].time_sec) {
    *out = ctx->kfs[n - 1].tone;
    return;
  }

  if (ctx->kf_seg >= n - 1) ctx->kf_seg = n - 2;
  while (ctx->kf_seg + 1 < n && t_sec > ctx->kfs[ctx->kf_seg + 1].time_sec)
    ctx->kf_seg++;
  while (ctx->kf_seg > 0 && t_sec < ctx->kfs[ctx->kf_seg].time_sec)
    ctx->kf_seg--;

  i0 = ctx->kf_seg;
  i1 = i0 + 1;
  if (i1 >= n) i1 = n - 1;
  k0 = &ctx->kfs[i0];
  k1 = &ctx->kfs[i1];

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
  if (k0->interp == SBX_INTERP_STEP)
    u = 0.0;

  out->mode = k0->tone.mode;
  out->carrier_hz = sbx_lerp(k0->tone.carrier_hz, k1->tone.carrier_hz, u);
  out->beat_hz = sbx_lerp(k0->tone.beat_hz, k1->tone.beat_hz, u);
  out->amplitude = sbx_lerp(k0->tone.amplitude, k1->tone.amplitude, u);
  out->duty_cycle = sbx_lerp(k0->tone.duty_cycle, k1->tone.duty_cycle, u);
}

const char *
sbx_version(void) {
  return SBAGENLIB_VERSION;
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
sbx_parse_tone_spec(const char *spec, SbxToneSpec *out_tone) {
  const char *p;
  int n = 0;
  double carrier = 0.0, beat = 0.0, amp_pct = 0.0;
  char c = 0;

  if (!spec || !out_tone) return SBX_EINVAL;

  p = skip_ws(spec);
  p = skip_optional_waveform_prefix(p);
  sbx_default_tone_spec(out_tone);

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
  ctx->source_mode = SBX_CTX_SRC_NONE;
  ctx->kfs = 0;
  ctx->kf_count = 0;
  ctx->kf_loop = 0;
  ctx->kf_seg = 0;
  ctx->kf_duration_sec = 0.0;
  ctx->t_sec = 0.0;
  set_ctx_error(ctx, NULL);
  return ctx;
}

void
sbx_context_destroy(SbxContext *ctx) {
  if (!ctx) return;
  ctx_clear_keyframes(ctx);
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
sbx_context_load_tone_spec(SbxContext *ctx, const char *tone_spec) {
  SbxToneSpec tone;
  int rc;

  if (!ctx || !ctx->eng || !tone_spec) return SBX_EINVAL;

  rc = sbx_parse_tone_spec(tone_spec, &tone);
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
  SbxProgramKeyframe *copy = 0;
  size_t i;
  SbxToneMode mode0 = SBX_TONE_NONE;
  char err[160];
  int rc;

  if (!ctx || !ctx->eng || !frames || frame_count == 0) return SBX_EINVAL;
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
    if (i == 0) mode0 = copy[i].tone.mode;
    else if (copy[i].tone.mode != mode0) {
      free(copy);
      set_ctx_error(ctx, "all keyframes must use the same tone mode");
      return SBX_EINVAL;
    }
  }

  if (loop && copy[frame_count - 1].time_sec <= 0.0) {
    free(copy);
    set_ctx_error(ctx, "looped keyframes require final time_sec > 0");
    return SBX_EINVAL;
  }

  ctx_clear_keyframes(ctx);
  ctx->kfs = copy;
  ctx->kf_count = frame_count;
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

  set_ctx_error(ctx, NULL);
  return SBX_OK;
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

    rc = sbx_parse_tone_spec(tone_tok, &tone);
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
      char emsg[208];
      snprintf(emsg, sizeof(emsg),
               "line %lu: expected '<HH:MM[:SS]> <tone-spec>'",
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

    rc = parse_hhmmss_token(time_tok, &tsec);
    if (rc != SBX_OK) {
      char emsg[224];
      snprintf(emsg, sizeof(emsg),
               "line %lu: invalid SBG timing token '%s'",
               (unsigned long)line_no, time_tok);
      set_ctx_error(ctx, emsg);
      rc = SBX_EINVAL;
      goto done;
    }

    rc = sbx_parse_tone_spec(tone_tok, &tone);
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
    set_ctx_error(ctx, "sbg timing text contains no keyframes");
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

    if (ctx->kf_loop && ctx->kf_duration_sec > 0.0) {
      while (ctx->t_sec >= ctx->kf_duration_sec) {
        ctx->t_sec -= ctx->kf_duration_sec;
        ctx->kf_seg = 0;
      }
    }

    ctx_eval_keyframed_tone(ctx, ctx->t_sec, &tone);
    ctx->eng->tone = tone;
    engine_render_sample(ctx->eng, &l, &r);
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
