#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "sbagenxlib.h"

int main(void) {
  SbxEngineConfig cfg;
  SbxToneSpec tone;
  SbxEngine *eng;
  const size_t frames = 44100;
  float *buf;
  double sum_l = 0.0, sum_r = 0.0;
  size_t i;

  sbx_default_engine_config(&cfg);
  eng = sbx_engine_create(&cfg);
  if (!eng) {
    fprintf(stderr, "failed to create sbagenxlib engine\n");
    return 1;
  }

  sbx_default_tone_spec(&tone);
  tone.mode = SBX_TONE_BINAURAL;
  tone.carrier_hz = 200.0;
  tone.beat_hz = 10.0;
  tone.amplitude = 0.2;

  if (sbx_engine_set_tone(eng, &tone) != SBX_OK) {
    fprintf(stderr, "failed to set tone: %s\n", sbx_engine_last_error(eng));
    sbx_engine_destroy(eng);
    return 1;
  }

  buf = (float *)malloc(frames * 2 * sizeof(float));
  if (!buf) {
    sbx_engine_destroy(eng);
    return 1;
  }

  if (sbx_engine_render_f32(eng, buf, frames) != SBX_OK) {
    fprintf(stderr, "render failed: %s\n", sbx_engine_last_error(eng));
    free(buf);
    sbx_engine_destroy(eng);
    return 1;
  }

  for (i = 0; i < frames; i++) {
    sum_l += buf[i * 2] * buf[i * 2];
    sum_r += buf[i * 2 + 1] * buf[i * 2 + 1];
  }

  printf("sbagenxlib %s (api %d)\n", sbx_version(), sbx_api_version());
  printf("rendered %lu frames, rmsL=%.6f rmsR=%.6f\n",
         (unsigned long)frames,
         sqrt(sum_l / (double)frames),
         sqrt(sum_r / (double)frames));

  free(buf);
  sbx_engine_destroy(eng);
  return 0;
}
