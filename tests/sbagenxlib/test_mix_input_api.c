#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

static void
fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

int
main(int argc, char **argv) {
  FILE *fp;
  SbxMixInputConfig cfg;
  SbxMixInput *mix;
  int pcm[8];
  int rv;

  if (argc != 3)
    fail("usage: test_mix_input_api <wav> <raw>");

  sbx_default_mix_input_config(&cfg);
  cfg.output_rate_hz = 44100;
  cfg.output_rate_is_default = 1;

  fp = fopen(argv[1], "rb");
  if (!fp) fail("open wav failed");
  mix = sbx_mix_input_create_stdio(fp, argv[1], &cfg);
  if (!mix) fail("create wav mix input failed");
  if (sbx_mix_input_last_error(mix)[0] != 0)
    fail("wav mix input reported unexpected error");
  if (sbx_mix_input_format(mix) != SBX_MIX_INPUT_WAV)
    fail("wav mix input format mismatch");
  if (sbx_mix_input_output_rate(mix) != 48000)
    fail("wav mix input did not inherit sample rate");
  if (sbx_mix_input_output_rate_is_default(mix) != 0)
    fail("wav mix input should consume default-rate flag");
  rv = sbx_mix_input_read(mix, pcm, 8);
  if (rv != 8)
    fail("wav mix input did not return expected sample count");
  sbx_mix_input_destroy(mix);
  fclose(fp);

  sbx_default_mix_input_config(&cfg);
  cfg.output_rate_hz = 44100;
  cfg.output_rate_is_default = 1;

  fp = fopen(argv[2], "rb");
  if (!fp) fail("open raw failed");
  mix = sbx_mix_input_create_stdio(fp, argv[2], &cfg);
  if (!mix) fail("create raw mix input failed");
  if (sbx_mix_input_last_error(mix)[0] != 0)
    fail("raw mix input reported unexpected error");
  if (sbx_mix_input_format(mix) != SBX_MIX_INPUT_RAW)
    fail("raw mix input format mismatch");
  if (sbx_mix_input_output_rate(mix) != 44100)
    fail("raw mix input unexpectedly changed sample rate");
  if (sbx_mix_input_output_rate_is_default(mix) != 1)
    fail("raw mix input should preserve default-rate flag");
  memset(pcm, 0, sizeof(pcm));
  rv = sbx_mix_input_read(mix, pcm, 8);
  if (rv != 8)
    fail("raw mix input did not return expected sample count");
  if (pcm[0] != (1 << 4) || pcm[1] != (-2 << 4) ||
      pcm[2] != (3 << 4) || pcm[3] != (-4 << 4))
    fail("raw mix input sample conversion mismatch");
  sbx_mix_input_destroy(mix);
  fclose(fp);

  return 0;
}
