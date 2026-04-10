#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "api.h"

#define SAMPLE_RATE 44100.0
#define FRAME_COUNT 32768

static void write_all_or_die(FILE *fp, const void *buf, size_t size, size_t count) {
  if (fwrite(buf, size, count, fp) != count) {
    perror("fwrite");
    exit(1);
  }
}

int main(int argc, char **argv) {
  SBG *sbg;
  char *err;
  int16_t *buf16;
  int32_t *buf32;
  FILE *fp;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <out16.raw> <out32.raw>\n", argv[0]);
    return 2;
  }

  buf16 = (int16_t *)malloc((size_t)FRAME_COUNT * 2u * sizeof(int16_t));
  buf32 = (int32_t *)malloc((size_t)FRAME_COUNT * 2u * sizeof(int32_t));
  if (!buf16 || !buf32) {
    fprintf(stderr, "alloc failed\n");
    return 1;
  }

  sbg = sbg_init(SAMPLE_RATE);
  if (!sbg) {
    fprintf(stderr, "sbg_init failed\n");
    return 1;
  }
  err = sbg_set(sbg, 0, 0, 0, "1000/1");
  if (err) {
    fprintf(stderr, "%s\n", err);
    free(err);
    return 1;
  }
  sbg_gen(sbg, (short *)buf16, FRAME_COUNT);
  fp = fopen(argv[1], "wb");
  if (!fp) {
    perror("fopen out16");
    return 1;
  }
  write_all_or_die(fp, buf16, sizeof(int16_t), (size_t)FRAME_COUNT * 2u);
  fclose(fp);
  sbg_term(sbg);

  sbg = sbg_init(SAMPLE_RATE);
  if (!sbg) {
    fprintf(stderr, "sbg_init failed\n");
    return 1;
  }
  err = sbg_set(sbg, 0, 0, 0, "1000/1");
  if (err) {
    fprintf(stderr, "%s\n", err);
    free(err);
    return 1;
  }
  sbg_gen32(sbg, (int *)buf32, FRAME_COUNT);
  fp = fopen(argv[2], "wb");
  if (!fp) {
    perror("fopen out32");
    return 1;
  }
  write_all_or_die(fp, buf32, sizeof(int32_t), (size_t)FRAME_COUNT * 2u);
  fclose(fp);
  sbg_term(sbg);

  free(buf16);
  free(buf32);
  return 0;
}
