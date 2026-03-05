#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

typedef struct {
  int calls;
  SbxRuntimeTelemetry last;
} TelemetrySink;

static void
fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg ? msg : "(null)");
  exit(1);
}

static int
near(double a, double b, double eps) {
  return fabs(a - b) <= eps;
}

static void
on_telemetry(const SbxRuntimeTelemetry *telem, void *user) {
  TelemetrySink *sink = (TelemetrySink *)user;
  if (!sink || !telem) return;
  sink->calls++;
  sink->last = *telem;
}

int
main(void) {
  SbxEngineConfig cfg;
  SbxContext *ctx = 0;
  SbxToneSpec t0, t1;
  SbxProgramKeyframe kf[2];
  SbxRuntimeTelemetry telem;
  TelemetrySink sink;
  float buf[2 * 100];
  int rc;
  int calls_after_disable;

  memset(&sink, 0, sizeof(sink));
  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 200.0;
  cfg.channels = 2;

  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("sbx_context_create failed");

  if (sbx_parse_tone_spec("200+10/20", &t0) != SBX_OK)
    fail("failed parsing first tone");
  if (sbx_parse_tone_spec("180+2/20", &t1) != SBX_OK)
    fail("failed parsing second tone");

  kf[0].time_sec = 0.0;
  kf[0].tone = t0;
  kf[0].interp = SBX_INTERP_LINEAR;
  kf[1].time_sec = 2.0;
  kf[1].tone = t1;
  kf[1].interp = SBX_INTERP_LINEAR;

  rc = sbx_context_load_keyframes(ctx, kf, 2, 0);
  if (rc != SBX_OK) fail("sbx_context_load_keyframes failed");

  rc = sbx_context_set_telemetry_callback(ctx, on_telemetry, &sink);
  if (rc != SBX_OK) fail("sbx_context_set_telemetry_callback failed");
  if (sink.calls < 1) fail("telemetry callback was not invoked on registration");
  if (sink.last.source_mode != SBX_SOURCE_KEYFRAMES)
    fail("telemetry source mode mismatch");
  if (sink.last.voice_count != 1)
    fail("telemetry voice count mismatch");
  if (sink.last.aux_tone_count != 0)
    fail("telemetry aux tone count mismatch");
  if (!near(sink.last.time_sec, 0.0, 1e-9))
    fail("telemetry initial time mismatch");

  rc = sbx_context_render_f32(ctx, buf, 100);
  if (rc != SBX_OK) fail("sbx_context_render_f32 failed");
  if (sink.calls < 2) fail("telemetry callback was not invoked on render");
  if (sink.last.source_mode != SBX_SOURCE_KEYFRAMES)
    fail("telemetry source mode mismatch after render");
  if (!(sink.last.program_beat_hz > 2.0 && sink.last.program_beat_hz <= 10.0))
    fail("telemetry program beat out of expected range");
  if (!(sink.last.program_carrier_hz >= 180.0 && sink.last.program_carrier_hz <= 200.0))
    fail("telemetry carrier out of expected range");
  if (!near(sink.last.mix_amp_pct, 100.0, 1e-9))
    fail("telemetry default mix amp should be 100%%");

  rc = sbx_context_get_runtime_telemetry(ctx, &telem);
  if (rc != SBX_OK) fail("sbx_context_get_runtime_telemetry failed");
  if (telem.source_mode != SBX_SOURCE_KEYFRAMES)
    fail("runtime telemetry source mode mismatch");
  if (telem.time_sec <= 0.0)
    fail("runtime telemetry time did not advance");
  if (!(telem.program_beat_hz > 2.0 && telem.program_beat_hz <= 10.0))
    fail("runtime telemetry beat out of expected range");

  rc = sbx_context_set_telemetry_callback(ctx, 0, 0);
  if (rc != SBX_OK) fail("failed disabling telemetry callback");
  calls_after_disable = sink.calls;
  rc = sbx_context_render_f32(ctx, buf, 40);
  if (rc != SBX_OK) fail("render failed after disabling telemetry callback");
  if (sink.calls != calls_after_disable)
    fail("telemetry callback fired after disable");

  sbx_context_destroy(ctx);
  return 0;
}
