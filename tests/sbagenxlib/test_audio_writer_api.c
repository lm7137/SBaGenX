#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "sbagenxlib.h"

static void fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

static uint32_t rd_u32le(const unsigned char *p) {
  return (uint32_t)p[0] |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

int main(void) {
  const char *wav_path = "/tmp/test_audio_writer_api.wav";
  const char *raw_path = "/tmp/test_audio_writer_api.raw";
  unsigned char wav_hdr[44];
  unsigned char pcm[600];
  SbxAudioWriterConfig cfg;
  SbxAudioWriter *wr;
  FILE *fp;
  long raw_size;
  size_t i;

  unlink(wav_path);
  unlink(raw_path);

  for (i = 0; i < sizeof(pcm); i++)
    pcm[i] = (unsigned char)(i & 0xFFu);

  sbx_default_audio_writer_config(&cfg);
  cfg.sample_rate = 44100.0;
  cfg.channels = 2;
  cfg.format = SBX_AUDIO_FILE_WAV;
  cfg.pcm_bits = 24;
  wr = sbx_audio_writer_create_path(wav_path, &cfg);
  if (!wr) fail("create wav writer failed");
  if (sbx_audio_writer_frame_bytes(wr) != 6)
    fail("wav writer frame bytes mismatch");
  if (sbx_audio_writer_write_bytes(wr, pcm, sizeof(pcm)) != SBX_OK)
    fail("wav writer byte write failed");
  if (sbx_audio_writer_close(wr) != SBX_OK)
    fail("wav writer close failed");
  sbx_audio_writer_destroy(wr);

  fp = fopen(wav_path, "rb");
  if (!fp) fail("open wav output failed");
  if (fread(wav_hdr, 1, sizeof(wav_hdr), fp) != sizeof(wav_hdr))
    fail("read wav header failed");
  fclose(fp);

  if (memcmp(wav_hdr + 0, "RIFF", 4) != 0) fail("wav riff tag mismatch");
  if (memcmp(wav_hdr + 8, "WAVE", 4) != 0) fail("wav wave tag mismatch");
  if (memcmp(wav_hdr + 12, "fmt ", 4) != 0) fail("wav fmt tag mismatch");
  if (rd_u32le(wav_hdr + 24) != 44100u) fail("wav sample rate mismatch");
  if (rd_u32le(wav_hdr + 28) != 264600u) fail("wav byte rate mismatch");
  if ((wav_hdr[32] | (wav_hdr[33] << 8)) != 6) fail("wav block align mismatch");
  if ((wav_hdr[34] | (wav_hdr[35] << 8)) != 24) fail("wav bit depth mismatch");
  if (rd_u32le(wav_hdr + 40) != (uint32_t)sizeof(pcm)) fail("wav data size mismatch");

  sbx_default_audio_writer_config(&cfg);
  cfg.sample_rate = 44100.0;
  cfg.channels = 2;
  cfg.format = SBX_AUDIO_FILE_RAW;
  cfg.pcm_bits = 24;
  wr = sbx_audio_writer_create_path(raw_path, &cfg);
  if (!wr) fail("create raw writer failed");
  if (sbx_audio_writer_write_bytes(wr, pcm, sizeof(pcm)) != SBX_OK)
    fail("raw writer byte write failed");
  if (sbx_audio_writer_close(wr) != SBX_OK)
    fail("raw writer close failed");
  sbx_audio_writer_destroy(wr);

  fp = fopen(raw_path, "rb");
  if (!fp) fail("open raw output failed");
  if (fseek(fp, 0, SEEK_END) != 0) fail("seek raw output failed");
  raw_size = ftell(fp);
  fclose(fp);
  if (raw_size != (long)sizeof(pcm)) fail("raw output size mismatch");

  unlink(wav_path);
  unlink(raw_path);
  printf("PASS: sbagenxlib audio writer handles raw/WAV output\n");
  return 0;
}
