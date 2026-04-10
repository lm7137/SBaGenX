#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "sbagenxlib.h"

#define SAMPLE_RATE 44100.0
#define FRAME_COUNT 32768

static void write_all_or_die(FILE *fp, const void *buf, size_t size, size_t count) {
  if (fwrite(buf, size, count, fp) != count) {
    perror("fwrite");
    exit(1);
  }
}

int main(int argc, char **argv) {
  SbxEngineConfig cfg;
  SbxToneSpec tone;
  SbxEngine *eng;
  float *buf_f32;
  int16_t *buf_s16;
  SbxPcmConvertState cvt;
  FILE *fp;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <out_f32.raw> <out_s16.raw>\n", argv[0]);
    return 2;
  }

  sbx_default_engine_config(&cfg);
  sbx_default_tone_spec(&tone);
  tone.mode = SBX_TONE_BINAURAL;
  tone.carrier_hz = 1000.0;
  tone.beat_hz = 0.0;
  tone.amplitude = 0.01;
  tone.waveform = SBX_WAVE_SINE;
  tone.envelope_waveform = SBX_ENV_WAVE_NONE;

  eng = sbx_engine_create(&cfg);
  if (!eng) {
    fprintf(stderr, "sbx_engine_create failed\n");
    return 1;
  }
  if (sbx_engine_set_tone(eng, &tone) != SBX_OK) {
    fprintf(stderr, "sbx_engine_set_tone failed: %s\n", sbx_engine_last_error(eng));
    return 1;
  }

  buf_f32 = (float *)calloc((size_t)FRAME_COUNT * 2u, sizeof(float));
  buf_s16 = (int16_t *)calloc((size_t)FRAME_COUNT * 2u, sizeof(int16_t));
  if (!buf_f32 || !buf_s16) {
    fprintf(stderr, "alloc failed\n");
    return 1;
  }

  if (sbx_engine_render_f32(eng, buf_f32, FRAME_COUNT) != SBX_OK) {
    fprintf(stderr, "render failed: %s\n", sbx_engine_last_error(eng));
    return 1;
  }
  fp = fopen(argv[1], "wb");
  if (!fp) {
    perror("fopen out_f32");
    return 1;
  }
  write_all_or_die(fp, buf_f32, sizeof(float), (size_t)FRAME_COUNT * 2u);
  fclose(fp);

  sbx_default_pcm_convert_state(&cvt);
  if (sbx_convert_f32_to_s16_ex(buf_f32, buf_s16, (size_t)FRAME_COUNT * 2u, &cvt) != SBX_OK) {
    fprintf(stderr, "convert failed\n");
    return 1;
  }
  fp = fopen(argv[2], "wb");
  if (!fp) {
    perror("fopen out_s16");
    return 1;
  }
  write_all_or_die(fp, buf_s16, sizeof(int16_t), (size_t)FRAME_COUNT * 2u);
  fclose(fp);

  sbx_engine_destroy(eng);
  free(buf_f32);
  free(buf_s16);
  return 0;
}
