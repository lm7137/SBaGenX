#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

static void fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

typedef struct {
  int count;
  char lines[4][128];
} SeenLines;

static int collect_line(const char *line, void *user) {
  SeenLines *seen = (SeenLines *)user;
  if (!seen || !line) return 1;
  if (seen->count >= 4) return 1;
  snprintf(seen->lines[seen->count], sizeof(seen->lines[seen->count]),
           "%s", line);
  seen->count++;
  return 0;
}

int main(void) {
  const char *wrapper_text =
      "# comment\n"
      "-p drop t1,0,0 -01ds+/100\n"
      "  -m /tmp/example.wav  \n";
  const char *not_wrapper_text =
      "NOW base\n"
      "+00:05:00 alloff\n";
  SeenLines seen;
  char err[256];
  int rc;

  memset(&seen, 0, sizeof(seen));
  err[0] = 0;
  rc = sbx_run_option_only_seq_wrapper_text(wrapper_text, collect_line, &seen,
                                            err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "wrapper text unexpectedly failed");
  if (seen.count != 2) fail("expected two option lines");
  if (strcmp(seen.lines[0], "-p drop t1,0,0 -01ds+/100") != 0)
    fail("first option line mismatch");
  if (strcmp(seen.lines[1], "-m /tmp/example.wav") != 0)
    fail("second option line mismatch");

  err[0] = 0;
  rc = sbx_run_option_only_seq_wrapper_text(not_wrapper_text, collect_line,
                                            &seen, err, sizeof(err));
  if (rc != SBX_EINVAL) fail("non-wrapper text should be rejected");
  if (strcmp(err, "not an option-only sequence wrapper") != 0)
    fail("unexpected non-wrapper error text");

  puts("PASS: sbagenxlib option-only wrapper API recognizes and iterates wrapper lines");
  return 0;
}
