#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "sbagenxlib.h"

#define T_POSIX
#define main sbxstudio_embedded_main
#include "sbagenx.c"
#undef main

static void json_print_string(const char *s) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  putchar('"');
  while (*p) {
    switch (*p) {
    case '\\': fputs("\\\\", stdout); break;
    case '"': fputs("\\\"", stdout); break;
    case '\b': fputs("\\b", stdout); break;
    case '\f': fputs("\\f", stdout); break;
    case '\n': fputs("\\n", stdout); break;
    case '\r': fputs("\\r", stdout); break;
    case '\t': fputs("\\t", stdout); break;
    default:
      if (*p < 0x20) printf("\\u%04x", (unsigned int)*p);
      else putchar(*p);
      break;
    }
    p++;
  }
  putchar('"');
}

static const char *source_mode_name(int mode) {
  switch (mode) {
  case SBX_SOURCE_NONE: return "none";
  case SBX_SOURCE_STATIC: return "static";
  case SBX_SOURCE_KEYFRAMES: return "keyframes";
  default: return "unknown";
  }
}

static const char *detect_file_type(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot) return "unknown";
  if (strcasecmp(dot, ".sbg") == 0) return "sbg";
  if (strcasecmp(dot, ".sbgf") == 0) return "sbgf";
  return "unknown";
}

static void print_result_prefix(const char *status, const char *backend, const char *file_type) {
  fputs("{\"status\":", stdout);
  json_print_string(status);
  fputs(",\"backend\":", stdout);
  json_print_string(backend);
  fputs(",\"fileType\":", stdout);
  json_print_string(file_type);
}

static void inspect_sbgf_file(const char *path) {
  int ok;
  int param_count;
  int has_solve;
  int has_carr;
  int has_amp;
  int has_mixamp;
  int beat_pieces;
  int carr_pieces;
  int amp_pieces;
  int mixamp_pieces;

  opt_Q = 1;
  clear_func_curve();
  load_curve_file(path);
  ok = setup_custom_func_curve(0, 1, 0,
                               205.0, 200.0, 1800.0,
                               10.0, 2.5, 1800.0,
                               30.0, 60.0, 3.0,
                               1.0, 100.0);
  if (!ok)
    error("Unable to validate curve expressions in %s", path);

  param_count = func_curve.param_count;
  has_solve = func_curve.have_solve;
  has_carr = func_curve.have_carr_expr || func_curve.carr_piece_count > 0;
  has_amp = func_curve.have_amp_expr || func_curve.amp_piece_count > 0;
  has_mixamp = func_curve.have_mixamp_expr || func_curve.mixamp_piece_count > 0;
  beat_pieces = func_curve.beat_piece_count;
  carr_pieces = func_curve.carr_piece_count;
  amp_pieces = func_curve.amp_piece_count;
  mixamp_pieces = func_curve.mixamp_piece_count;

  clear_func_curve();

  print_result_prefix("ok", "sbagenx-curve-parser", "sbgf");
  fputs(",\"version\":", stdout);
  json_print_string(VERSION);
  printf(",\"apiVersion\":%d", sbx_api_version());
  printf(",\"parameterCount\":%d", param_count);
  printf(",\"hasSolve\":%s", has_solve ? "true" : "false");
  printf(",\"hasCarrierExpr\":%s", has_carr ? "true" : "false");
  printf(",\"hasAmpExpr\":%s", has_amp ? "true" : "false");
  printf(",\"hasMixAmpExpr\":%s", has_mixamp ? "true" : "false");
  printf(",\"beatPieceCount\":%d", beat_pieces);
  printf(",\"carrierPieceCount\":%d", carr_pieces);
  printf(",\"ampPieceCount\":%d", amp_pieces);
  printf(",\"mixAmpPieceCount\":%d", mixamp_pieces);
  fputs("}\n", stdout);
}

int main(int argc, char **argv) {
  const char *path;
  const char *file_type;
  SbxContext *ctx;
  int status;

  if (argc != 2) {
    fputs("usage: sbagenx-studio-engine <file>\n", stderr);
    return 2;
  }

  path = argv[1];
  file_type = detect_file_type(path);

  if (strcmp(file_type, "sbgf") == 0) {
    inspect_sbgf_file(path);
    return 0;
  }

  ctx = sbx_context_create(NULL);
  if (!ctx) {
    print_result_prefix("error", "sbagenxlib", file_type);
    fputs(",\"message\":", stdout);
    json_print_string("Failed to create sbagenxlib context");
    fputs("}\n", stdout);
    return 0;
  }

  status = sbx_context_load_sbg_timing_file(ctx, path, 0);
  if (status != SBX_OK) {
    print_result_prefix("error", "sbagenxlib", file_type);
    fputs(",\"message\":", stdout);
    json_print_string(sbx_context_last_error(ctx));
    fputs(",\"version\":", stdout);
    json_print_string(sbx_version());
    printf(",\"apiVersion\":%d}\n", sbx_api_version());
    sbx_context_destroy(ctx);
    return 0;
  }

  print_result_prefix("ok", "sbagenxlib", file_type);
  fputs(",\"version\":", stdout);
  json_print_string(sbx_version());
  printf(",\"apiVersion\":%d", sbx_api_version());
  fputs(",\"sourceMode\":", stdout);
  json_print_string(source_mode_name(sbx_context_source_mode(ctx)));
  printf(",\"voiceCount\":%lu", (unsigned long)sbx_context_voice_count(ctx));
  printf(",\"keyframeCount\":%lu", (unsigned long)sbx_context_keyframe_count(ctx));
  printf(",\"durationSec\":%.6f", sbx_context_duration_sec(ctx));
  printf(",\"looping\":%s", sbx_context_is_looping(ctx) ? "true" : "false");
  printf(",\"hasMixAmpControl\":%s", sbx_context_has_mix_amp_control(ctx) ? "true" : "false");
  printf(",\"hasMixEffects\":%s", sbx_context_has_mix_effects(ctx) ? "true" : "false");
  fputs("}\n", stdout);

  sbx_context_destroy(ctx);
  return 0;
}
