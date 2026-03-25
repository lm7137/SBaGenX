#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

static void
fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

static char *
read_text(const char *path) {
  FILE *fp;
  long len;
  size_t got;
  char *buf;

  fp = fopen(path, "rb");
  if (!fp) return 0;
  if (0 != fseek(fp, 0, SEEK_END)) {
    fclose(fp);
    return 0;
  }
  len = ftell(fp);
  if (len < 0) {
    fclose(fp);
    return 0;
  }
  if (0 != fseek(fp, 0, SEEK_SET)) {
    fclose(fp);
    return 0;
  }
  buf = (char *)malloc((size_t)len + 1U);
  if (!buf) {
    fclose(fp);
    return 0;
  }
  got = fread(buf, 1, (size_t)len, fp);
  fclose(fp);
  if (got != (size_t)len) {
    free(buf);
    return 0;
  }
  buf[len] = 0;
  return buf;
}

int
main(void) {
  SbxDiagnostic *diags = 0;
  size_t count = 0;
  int rc;
  char *valid_sbg = read_text("examples/sbagenxlib/minimal-sbg-wave-custom.sbg");
  char *valid_sbgf = read_text("examples/basics/curve-expfit-solve-demo.sbgf");
  const char *bad_sbg =
    "-SE\n"
    "-Z nope\n"
    "\n"
    "alloff: -\n"
    "\n"
    "NOW alloff\n";
  const char *bad_sbg_custom =
    "custom00: e=2 0 0.1 0.85 1 1 0.85 0.1 0\n"
    "\n"
    "base: custom00:196+6/35 xyz\n"
    "drift: custom00:180+4/30\n"
    "alloff: -\n"
    "\n"
    "NOW base\n"
    "+00:02:00 base ->\n"
    "+00:03:00 drift\n"
    "+00:05:00 drift ->\n"
    "+00:06:00 alloff\n";
  const char *bad_sbgf =
    "title \"broken\"\n"
    "beat =\n";

  if (!valid_sbg) fail("failed to read valid example .sbg");
  if (!valid_sbgf) fail("failed to read valid example .sbgf");

  rc = sbx_validate_sbg_text(valid_sbg, "minimal-sbg-wave-custom.sbg",
                             &diags, &count);
  if (rc != SBX_OK) fail("sbx_validate_sbg_text valid case failed");
  if (count != 0 || diags)
    fail("valid .sbg should not emit diagnostics");

  rc = sbx_validate_sbg_text(bad_sbg, "bad.sbg", &diags, &count);
  if (rc != SBX_OK) fail("sbx_validate_sbg_text invalid case failed");
  if (!diags || count != 1)
    fail("invalid .sbg should emit one diagnostic");
  if (diags[0].severity != SBX_DIAG_ERROR)
    fail("invalid .sbg diagnostic severity mismatch");
  if (strcmp(diags[0].code, "sbg-parse") != 0)
    fail("invalid .sbg diagnostic code mismatch");
  if (diags[0].line != 2)
    fail("invalid safe preamble line should report line 2");
  if (!strstr(diags[0].message, "FLAC compression level"))
    fail("invalid .sbg diagnostic message mismatch");
  sbx_free_diagnostics(diags);
  diags = 0;
  count = 0;

  rc = sbx_validate_sbg_text(bad_sbg_custom, "bad-custom.sbg", &diags, &count);
  if (rc != SBX_OK) fail("sbx_validate_sbg_text custom invalid case failed");
  if (!diags || count != 1)
    fail("invalid custom .sbg should emit one diagnostic");
  if (diags[0].line != 3)
    fail("invalid custom .sbg should report edited tone-set line");
  if (diags[0].column != 25 || diags[0].end_column != 28)
    fail("invalid custom .sbg diagnostic should highlight offending token span");
  if (!strstr(diags[0].message, "invalid named tone-set token 'xyz'"))
    fail("invalid custom .sbg diagnostic should preserve native timing error");
  sbx_free_diagnostics(diags);
  diags = 0;
  count = 0;

  rc = sbx_validate_sbgf_text(valid_sbgf, "curve-expfit-solve-demo.sbgf",
                              &diags, &count);
  if (rc != SBX_OK) fail("sbx_validate_sbgf_text valid case failed");
  if (count != 0 || diags)
    fail("valid .sbgf should not emit diagnostics");

  rc = sbx_validate_sbgf_text(bad_sbgf, "bad.sbgf", &diags, &count);
  if (rc != SBX_OK) fail("sbx_validate_sbgf_text invalid case failed");
  if (!diags || count != 1)
    fail("invalid .sbgf should emit one diagnostic");
  if (diags[0].severity != SBX_DIAG_ERROR)
    fail("invalid .sbgf diagnostic severity mismatch");
  if (strcmp(diags[0].code, "sbgf-parse") != 0 &&
      strcmp(diags[0].code, "sbgf-prepare") != 0)
    fail("invalid .sbgf diagnostic code mismatch");
  if (!diags[0].message[0])
    fail("invalid .sbgf diagnostic should include a message");
  sbx_free_diagnostics(diags);

  free(valid_sbg);
  free(valid_sbgf);
  puts("PASS: sbagenxlib structured validation diagnostics");
  return 0;
}
