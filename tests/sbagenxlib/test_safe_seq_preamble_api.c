#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sbagenxlib.h"

static void fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

int main(void) {
  const char *path = "/tmp/test_safe_seq_preamble_api.sbg";
  FILE *fp;
  char *text = 0;
  char err[256];
  SbxSafeSeqfilePreamble cfg;
  int rc;

  unlink(path);
  fp = fopen(path, "wb");
  if (!fp) fail("open temp sequence failed");
  fputs("-SE\n"
        "-D\n"
        "-o /tmp/example.flac\n"
        "-b 24\n"
        "-L 00:08:00\n"
        "-N\n"
        "-V 75\n"
        "-w triangle\n"
        "-c 80=1,40=2\n"
#ifdef ALSA_AUDIO
        "-d hw:1\n"
#endif
        "-A d=0.2:e=0.4:k=5:E=0.6\n"
        "-I s=0:d=1:e=3\n"
        "-H d=0.2:a=0.1:r=0.3:e=2:f=0.25\n"
        "-K 224\n"
        "-J 2\n"
        "-X 2.5\n"
        "-U 6\n"
        "-Z 12\n"
        "-R 400\n"
        "-m /tmp/mix.wav\n"
        "\n"
        "custom00: e=2 0 0.1 0.9 1 1 0.9 0.1 0\n"
        "base: custom00:400@2/20\n"
        "NOW base\n", fp);
  fclose(fp);

  sbx_default_safe_seqfile_preamble(&cfg);
  err[0] = 0;
  rc = sbx_prepare_safe_seqfile_text(path, &text, &cfg, err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "prepare_safe_seqfile_text failed");
  if (!text) fail("prepared text missing");
  if (!cfg.out_path || strcmp(cfg.out_path, "/tmp/example.flac") != 0)
    fail("out_path mismatch");
  if (!cfg.mix_path || strcmp(cfg.mix_path, "/tmp/mix.wav") != 0)
    fail("mix_path mismatch");
  if (!cfg.have_D)
    fail("debug dump flag mismatch");
  if (!cfg.have_Z || cfg.flac_compression != 12.0)
    fail("flac compression mismatch");
  if (!cfg.have_R || cfg.prate != 400)
    fail("parameter refresh mismatch");
  if (!cfg.have_b || cfg.pcm_bits != 24)
    fail("PCM depth mismatch");
  if (!cfg.have_L || cfg.L_ms != 480000)
    fail("manual length mismatch");
  if (!cfg.have_N)
    fail("normalization disable flag mismatch");
  if (!cfg.have_V || cfg.volume_pct != 75)
    fail("global volume mismatch");
  if (!cfg.have_w || cfg.waveform != SBX_WAVE_TRIANGLE)
    fail("waveform mismatch");
  if (!cfg.have_c || cfg.amp_adjust.point_count != 2 ||
      fabs(cfg.amp_adjust.points[0].freq_hz - 40.0) > 1e-9 ||
      fabs(cfg.amp_adjust.points[0].adj - 2.0) > 1e-9 ||
      fabs(cfg.amp_adjust.points[1].freq_hz - 80.0) > 1e-9 ||
      fabs(cfg.amp_adjust.points[1].adj - 1.0) > 1e-9)
    fail("amp-adjust mismatch");
#ifdef ALSA_AUDIO
  if (!cfg.device_path || strcmp(cfg.device_path, "hw:1") != 0)
    fail("ALSA device mismatch");
#endif
  if (!cfg.have_A ||
      fabs(cfg.mix_mod.delta - 0.2) > 1e-9 ||
      fabs(cfg.mix_mod.epsilon - 0.4) > 1e-9 ||
      fabs(cfg.mix_mod.period_sec - 300.0) > 1e-9 ||
      fabs(cfg.mix_mod.end_level - 0.6) > 1e-9)
    fail("mix modulation mismatch");
  if (!cfg.have_I ||
      fabs(cfg.iso_env.start - 0.0) > 1e-9 ||
      fabs(cfg.iso_env.duty - 1.0) > 1e-9 ||
      cfg.iso_env.edge_mode != 3)
    fail("iso override mismatch");
  if (!cfg.have_H ||
      cfg.mixam_env.type != SBX_MIXFX_AM ||
      fabs(cfg.mixam_env.mixam_duty - 0.2) > 1e-9 ||
      fabs(cfg.mixam_env.mixam_attack - 0.1) > 1e-9 ||
      fabs(cfg.mixam_env.mixam_release - 0.3) > 1e-9 ||
      cfg.mixam_env.mixam_edge_mode != 2 ||
      fabs(cfg.mixam_env.mixam_floor - 0.25) > 1e-9)
    fail("mixam override mismatch");
  if (!cfg.have_K || cfg.mp3_bitrate != 224)
    fail("MP3 bitrate mismatch");
  if (!cfg.have_J || cfg.mp3_quality != 2)
    fail("MP3 quality mismatch");
  if (!cfg.have_X || fabs(cfg.mp3_vbr_quality - 2.5) > 1e-9)
    fail("MP3 VBR mismatch");
  if (!cfg.have_U || fabs(cfg.ogg_quality - 6.0) > 1e-9)
    fail("OGG quality mismatch");
  if (strstr(text, "-o /tmp/example.flac") != 0)
    fail("safe preamble line was not blanked out");
  if (strstr(text, "NOW base") == 0)
    fail("sequence body missing NOW line");

  free(text);
  sbx_free_safe_seqfile_preamble(&cfg);
  unlink(path);
  puts("PASS: sbagenxlib safe sequence preamble API parses and strips wrapper lines");
  return 0;
}
