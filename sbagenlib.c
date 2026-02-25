#include "sbagenlib.h"

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

static void
set_last_error(SbxEngine *eng, const char *msg) {
  if (!eng) return;
  if (!msg) {
    eng->last_error[0] = 0;
    return;
  }
  snprintf(eng->last_error, sizeof(eng->last_error), "%s", msg);
}

static double
clamp(double v, double lo, double hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static double
wrap_phase(double p) {
  while (p >= SBX_TAU) p -= SBX_TAU;
  while (p < 0.0) p += SBX_TAU;
  return p;
}

static double
smoothstep(double x) {
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  return x * x * (3.0 - 2.0 * x);
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
    tone.amplitude = clamp(tone.amplitude, 0.0, 1.0);
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
    tone.duty_cycle = clamp(tone.duty_cycle, 0.01, 1.0);
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
      eng->phase_l = wrap_phase(eng->phase_l + SBX_TAU * f_l / sr);
      eng->phase_r = wrap_phase(eng->phase_r + SBX_TAU * f_r / sr);
    } else if (eng->tone.mode == SBX_TONE_MONAURAL) {
      double f1 = eng->tone.carrier_hz - eng->tone.beat_hz * 0.5;
      double f2 = eng->tone.carrier_hz + eng->tone.beat_hz * 0.5;
      double mono = 0.5 * amp * (sin(eng->phase_l) + sin(eng->phase_r));
      left = mono;
      right = mono;
      eng->phase_l = wrap_phase(eng->phase_l + SBX_TAU * f1 / sr);
      eng->phase_r = wrap_phase(eng->phase_r + SBX_TAU * f2 / sr);
    } else {
      double env = 0.0;
      double duty = eng->tone.duty_cycle;
      double edge = duty * 0.15;
      double pos;
      double carrier;

      carrier = sin(eng->phase_l);
      eng->phase_l = wrap_phase(eng->phase_l + SBX_TAU * eng->tone.carrier_hz / sr);

      eng->pulse_phase += eng->tone.beat_hz / sr;
      while (eng->pulse_phase >= 1.0) eng->pulse_phase -= 1.0;
      pos = eng->pulse_phase;

      if (pos < duty) {
        if (edge > 1e-9 && pos < edge) {
          env = smoothstep(pos / edge);
        } else if (edge > 1e-9 && pos > (duty - edge)) {
          env = smoothstep((duty - pos) / edge);
        } else {
          env = 1.0;
        }
      }

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
