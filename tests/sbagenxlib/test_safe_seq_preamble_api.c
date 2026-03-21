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
        "-o /tmp/example.flac\n"
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
  if (!cfg.have_Z || cfg.flac_compression != 12.0)
    fail("flac compression mismatch");
  if (!cfg.have_R || cfg.prate != 400)
    fail("parameter refresh mismatch");
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
