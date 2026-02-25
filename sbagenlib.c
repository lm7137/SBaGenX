#include "sbagenlib.h"
#include "sbagenlib_dsp.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SBAGENLIB_VERSION
#define SBAGENLIB_VERSION "dev"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SBX_TAU (2.0 * M_PI)

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
  SbxToneSpec tone;

  if (!eng || !tone_in) return SBX_EINVAL;
  tone = *tone_in;

  if (tone.mode < SBX_TONE_NONE || tone.mode > SBX_TONE_ISOCHRONIC) {
    set_last_error(eng, "unsupported tone mode");
    return SBX_EINVAL;
  }

  if (tone.mode != SBX_TONE_NONE) {
    if (tone.carrier_hz <= 0.0 || !isfinite(tone.carrier_hz)) {
      set_last_error(eng, "carrier_hz must be finite and > 0");
      return SBX_EINVAL;
    }
    if (!isfinite(tone.beat_hz)) {
      set_last_error(eng, "beat_hz must be finite");
      return SBX_EINVAL;
    }
    if (!isfinite(tone.amplitude)) {
      set_last_error(eng, "amplitude must be finite");
      return SBX_EINVAL;
    }
    tone.amplitude = sbx_dsp_clamp(tone.amplitude, 0.0, 1.0);
  }

  if (tone.mode == SBX_TONE_MONAURAL || tone.mode == SBX_TONE_ISOCHRONIC) {
    tone.beat_hz = fabs(tone.beat_hz);
  }

  if (tone.mode == SBX_TONE_ISOCHRONIC) {
    if (tone.beat_hz <= 0.0) {
      set_last_error(eng, "isochronic beat_hz must be > 0");
      return SBX_EINVAL;
    }
    if (!isfinite(tone.duty_cycle)) {
      set_last_error(eng, "duty_cycle must be finite");
      return SBX_EINVAL;
    }
    tone.duty_cycle = sbx_dsp_clamp(tone.duty_cycle, 0.01, 1.0);
  }

  eng->tone = tone;
  sbx_engine_reset(eng);
  set_last_error(eng, NULL);
  return SBX_OK;
}

int
sbx_engine_render_f32(SbxEngine *eng, float *out, size_t frames) {
  size_t i;
  double sr;
  double amp;

  if (!eng || !out) return SBX_EINVAL;
  if (eng->cfg.channels != 2 || eng->cfg.sample_rate <= 0.0) {
    set_last_error(eng, "engine configuration is invalid");
    return SBX_ENOTREADY;
  }

  sr = eng->cfg.sample_rate;
  amp = eng->tone.amplitude;

  if (eng->tone.mode == SBX_TONE_NONE) {
    memset(out, 0, frames * eng->cfg.channels * sizeof(float));
    return SBX_OK;
  }

  for (i = 0; i < frames; i++) {
    double left = 0.0;
    double right = 0.0;

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
    } else {
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

    out[i * 2] = (float)left;
    out[i * 2 + 1] = (float)right;
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
  set_ctx_error(ctx, NULL);
  return ctx;
}

void
sbx_context_destroy(SbxContext *ctx) {
  if (!ctx) return;
  sbx_engine_destroy(ctx->eng);
  free(ctx);
}

void
sbx_context_reset(SbxContext *ctx) {
  if (!ctx || !ctx->eng) return;
  sbx_engine_reset(ctx->eng);
  set_ctx_error(ctx, NULL);
}

int
sbx_context_set_tone(SbxContext *ctx, const SbxToneSpec *tone) {
  int rc;
  if (!ctx || !ctx->eng || !tone) return SBX_EINVAL;

  rc = sbx_engine_set_tone(ctx->eng, tone);
  if (rc != SBX_OK) {
    set_ctx_error(ctx, sbx_engine_last_error(ctx->eng));
    return rc;
  }
  ctx->loaded = 1;
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
sbx_context_render_f32(SbxContext *ctx, float *out, size_t frames) {
  int rc;
  if (!ctx || !ctx->eng || !out) return SBX_EINVAL;
  if (!ctx->loaded) {
    set_ctx_error(ctx, "no tone/program loaded");
    return SBX_ENOTREADY;
  }

  rc = sbx_engine_render_f32(ctx->eng, out, frames);
  if (rc != SBX_OK) {
    set_ctx_error(ctx, sbx_engine_last_error(ctx->eng));
    return rc;
  }
  return SBX_OK;
}

const char *
sbx_context_last_error(const SbxContext *ctx) {
  if (!ctx) return "null context";
  if (ctx->last_error[0]) return ctx->last_error;
  return sbx_engine_last_error(ctx->eng);
}
