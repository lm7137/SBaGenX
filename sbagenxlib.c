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
#define SBX_MIXBEAT_HILBERT_TAPS 31

typedef struct {
  SbxMixFxSpec spec;
  double phase;
  double mixbeat_hist[SBX_MIXBEAT_HILBERT_TAPS];
  int mixbeat_hist_pos;
} SbxMixFxState;

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
  double t_sec;
  SbxToneSpec *aux_tones;
  SbxEngine **aux_eng;
  size_t aux_count;
  float *aux_buf;
  size_t aux_buf_cap;
  SbxMixFxState *mix_fx;
  size_t mix_fx_count;
  SbxMixAmpKeyframe *mix_kf;
  size_t mix_kf_count;
  size_t mix_kf_seg;
  double mix_default_amp_pct;
};

static void engine_wave_sample(int waveform, double phase, double *out_sample);

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

static const char *
skip_optional_waveform_prefix(const char *spec, int *out_waveform) {
  const char *p = spec;
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
    if (tone->waveform < SBX_WAVE_SINE || tone->waveform > SBX_WAVE_SAWTOOTH) {
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
    engine_wave_sample(eng->tone.waveform, eng->phase_l, &left);
    engine_wave_sample(eng->tone.waveform, eng->phase_r, &right);
    left *= amp;
    right *= amp;
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
  if (!ctx) return;
  if (ctx->kfs) free(ctx->kfs);
  ctx->kfs = 0;
  ctx->kf_count = 0;
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
  for (i = 0; i < ctx->aux_count; i++) {
    if (ctx->aux_eng[i]) sbx_engine_reset(ctx->aux_eng[i]);
  }
  for (i = 0; i < ctx->mix_fx_count; i++) {
    sbx_mix_fx_reset_state(&ctx->mix_fx[i]);
  }
  ctx->mix_kf_seg = 0;
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
  out->waveform = k0->tone.waveform;
  out->duty_cycle = sbx_lerp(k0->tone.duty_cycle, k1->tone.duty_cycle, u);
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
  } else {
    return SBX_EINVAL;
  }

  if (fx.waveform < SBX_WAVE_SINE || fx.waveform > SBX_WAVE_SAWTOOTH)
    return SBX_EINVAL;
  if (!isfinite(fx.carr) || !isfinite(fx.res) || !isfinite(fx.amp) || fx.amp < 0.0)
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
  if (fx->type < SBX_MIXFX_SPIN || fx->type > SBX_MIXFX_BEAT) return SBX_EINVAL;
  if (fx->waveform < SBX_WAVE_SINE || fx->waveform > SBX_WAVE_SAWTOOTH) return SBX_EINVAL;
  if (!isfinite(fx->carr) || !isfinite(fx->res) || !isfinite(fx->amp) || fx->amp < 0.0)
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
    default:
      break;
  }

  return SBX_EINVAL;
}

int
sbx_format_tone_spec(const SbxToneSpec *tone, char *out, size_t out_sz) {
  double amp_pct;
  const char *wname;
  SbxToneSpec norm;
  if (!tone || !out || out_sz == 0) return SBX_EINVAL;

  norm = *tone;
  if (normalize_tone(&norm, 0, 0) != SBX_OK) return SBX_EINVAL;
  amp_pct = norm.amplitude * 100.0;
  wname = wave_name_for_tone(norm.waveform);

  switch (norm.mode) {
    case SBX_TONE_BINAURAL: {
      char sign = norm.beat_hz < 0.0 ? '-' : '+';
      double beat = fabs(norm.beat_hz);
      if (!snprintf_checked(out, out_sz, "%s:%g%c%g/%g",
                            wname, norm.carrier_hz, sign, beat, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    }
    case SBX_TONE_MONAURAL: {
      double beat = fabs(norm.beat_hz);
      double f1 = norm.carrier_hz - beat * 0.5;
      double f2 = norm.carrier_hz + beat * 0.5;
      if (!snprintf_checked(out, out_sz, "%s:%g/%g %s:%g/%g",
                            wname, f1, amp_pct, wname, f2, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    }
    case SBX_TONE_ISOCHRONIC: {
      double pulse = fabs(norm.beat_hz);
      if (!snprintf_checked(out, out_sz, "%s:%g@%g/%g",
                            wname, norm.carrier_hz, pulse, amp_pct))
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
      if (!snprintf_checked(out, out_sz, "%s:bell%g/%g", wname, norm.carrier_hz, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_SPIN_PINK:
      if (!snprintf_checked(out, out_sz, "%s:spin:%g%+g/%g", wname, norm.carrier_hz, norm.beat_hz, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_SPIN_BROWN:
      if (!snprintf_checked(out, out_sz, "%s:bspin:%g%+g/%g", wname, norm.carrier_hz, norm.beat_hz, amp_pct))
        return SBX_EINVAL;
      return SBX_OK;
    case SBX_TONE_SPIN_WHITE:
      if (!snprintf_checked(out, out_sz, "%s:wspin:%g%+g/%g", wname, norm.carrier_hz, norm.beat_hz, amp_pct))
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
  ctx->t_sec = 0.0;
  ctx->aux_tones = 0;
  ctx->aux_eng = 0;
  ctx->aux_count = 0;
  ctx->aux_buf = 0;
  ctx->aux_buf_cap = 0;
  ctx->mix_fx = 0;
  ctx->mix_fx_count = 0;
  ctx->mix_kf = 0;
  ctx->mix_kf_count = 0;
  ctx->mix_kf_seg = 0;
  ctx->mix_default_amp_pct = 100.0;
  set_ctx_error(ctx, NULL);
  return ctx;
}

void
sbx_context_destroy(SbxContext *ctx) {
  if (!ctx) return;
  ctx_clear_mix_keyframes(ctx);
  ctx_clear_mix_effects(ctx);
  ctx_clear_aux_tones(ctx);
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
    if (states[i].spec.type < SBX_MIXFX_NONE || states[i].spec.type > SBX_MIXFX_BEAT) {
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
    SbxMixFxState *fx = &ctx->mix_fx[i];
    switch (fx->spec.type) {
      case SBX_MIXFX_SPIN: {
        double wav, val, intensity, amplified, pos, fx_l, fx_r;
        fx->phase = sbx_dsp_wrap_unit(fx->phase + fx->spec.res / sr);
        wav = sbx_wave_sample_unit_phase(fx->spec.waveform, fx->phase);
        val = fx->spec.carr * 1.0e-6 * sr * wav;
        intensity = 0.5 + fx->spec.amp * 3.5;
        amplified = val * intensity;
        amplified = sbx_dsp_clamp(amplified, -128.0, 127.0);
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

  if (!ctx || !out_add_l || !out_add_r) return SBX_EINVAL;
  if (!isfinite(mix_mod_mul)) mix_mod_mul = 1.0;

  mix_l = (double)(mix_l_sample >> 4);
  mix_r = (double)(mix_r_sample >> 4);
  mix_pct = sbx_context_mix_amp_at(ctx, t_sec);
  mix_mul = (mix_pct / 100.0) * mix_mod_mul;

  *out_add_l = mix_l * mix_mul;
  *out_add_r = mix_r * mix_mul;

  if (sbx_context_mix_effect_count(ctx) > 0) {
    double fx_add_l = 0.0;
    double fx_add_r = 0.0;
    int rc = sbx_context_apply_mix_effects(ctx, mix_l, mix_r, mix_mul * 0.7,
                                           &fx_add_l, &fx_add_r);
    if (rc != SBX_OK) return rc;
    *out_add_l += fx_add_l;
    *out_add_r += fx_add_r;
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
sbx_context_keyframe_count(const SbxContext *ctx) {
  if (!ctx || !ctx->kfs) return 0;
  return ctx->kf_count;
}

int
sbx_context_get_keyframe(const SbxContext *ctx, size_t index, SbxProgramKeyframe *out) {
  if (!ctx || !out || !ctx->kfs) return SBX_EINVAL;
  if (index >= ctx->kf_count) return SBX_EINVAL;
  *out = ctx->kfs[index];
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

    if (ctx->kf_loop && ctx->kf_duration_sec > 0.0) {
      while (ctx->t_sec >= ctx->kf_duration_sec) {
        ctx->t_sec -= ctx->kf_duration_sec;
        ctx->kf_seg = 0;
      }
    }

    ctx_eval_keyframed_tone(ctx, ctx->t_sec, &tone);
    ctx->eng->tone = tone;
    engine_render_sample(ctx->eng, &l, &r);
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
