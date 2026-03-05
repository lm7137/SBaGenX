#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "sbagenxlib.h"

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
  case SBX_SOURCE_CURVE: return "curve";
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
  SbxCurveProgram *curve;
  SbxCurveEvalConfig cfg;
  SbxCurveSourceConfig src_cfg;
  SbxCurveInfo info;
  SbxContext *ctx = NULL;
  int curve_owned_by_ctx = 0;
  int status;

  curve = sbx_curve_create();
  if (!curve) {
    print_result_prefix("error", "sbagenxlib", "sbgf");
    fputs(",\"message\":", stdout);
    json_print_string("Failed to create sbagenxlib curve program");
    fputs(",\"version\":", stdout);
    json_print_string(sbx_version());
    printf(",\"apiVersion\":%d}\n", sbx_api_version());
    return;
  }

  status = sbx_curve_load_file(curve, path);
  if (status == SBX_OK) {
    sbx_default_curve_eval_config(&cfg);
    status = sbx_curve_prepare(curve, &cfg);
  }
  if (status != SBX_OK) {
    print_result_prefix("error", "sbagenxlib", "sbgf");
    fputs(",\"message\":", stdout);
    json_print_string(sbx_curve_last_error(curve));
    fputs(",\"version\":", stdout);
    json_print_string(sbx_version());
    printf(",\"apiVersion\":%d}\n", sbx_api_version());
    sbx_curve_destroy(curve);
    return;
  }

  status = sbx_curve_get_info(curve, &info);
  if (status != SBX_OK) {
    print_result_prefix("error", "sbagenxlib", "sbgf");
    fputs(",\"message\":", stdout);
    json_print_string("Failed to inspect loaded curve metadata");
    fputs(",\"version\":", stdout);
    json_print_string(sbx_version());
    printf(",\"apiVersion\":%d}\n", sbx_api_version());
    sbx_curve_destroy(curve);
    return;
  }

  ctx = sbx_context_create(NULL);
  if (!ctx) {
    print_result_prefix("error", "sbagenxlib", "sbgf");
    fputs(",\"message\":", stdout);
    json_print_string("Failed to create sbagenxlib context");
    fputs(",\"version\":", stdout);
    json_print_string(sbx_version());
    printf(",\"apiVersion\":%d}\n", sbx_api_version());
    sbx_curve_destroy(curve);
    return;
  }

  sbx_default_curve_source_config(&src_cfg);
  src_cfg.duration_sec = (cfg.total_min > 0.0) ? (cfg.total_min * 60.0) : cfg.beat_span_sec;
  if (!(src_cfg.duration_sec > 0.0))
    src_cfg.duration_sec = 1800.0;
  status = sbx_context_load_curve_program(ctx, curve, &src_cfg);
  if (status != SBX_OK) {
    print_result_prefix("error", "sbagenxlib", "sbgf");
    fputs(",\"message\":", stdout);
    json_print_string(sbx_context_last_error(ctx));
    fputs(",\"version\":", stdout);
    json_print_string(sbx_version());
    printf(",\"apiVersion\":%d}\n", sbx_api_version());
    sbx_context_destroy(ctx);
    sbx_curve_destroy(curve);
    return;
  }
  curve_owned_by_ctx = 1;

  print_result_prefix("ok", "sbagenxlib", "sbgf");
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
  printf(",\"parameterCount\":%lu", (unsigned long)info.parameter_count);
  printf(",\"hasSolve\":%s", info.has_solve ? "true" : "false");
  printf(",\"hasCarrierExpr\":%s", info.has_carrier_expr ? "true" : "false");
  printf(",\"hasAmpExpr\":%s", info.has_amp_expr ? "true" : "false");
  printf(",\"hasMixAmpExpr\":%s", info.has_mixamp_expr ? "true" : "false");
  printf(",\"beatPieceCount\":%lu", (unsigned long)info.beat_piece_count);
  printf(",\"carrierPieceCount\":%lu", (unsigned long)info.carrier_piece_count);
  printf(",\"ampPieceCount\":%lu", (unsigned long)info.amp_piece_count);
  printf(",\"mixAmpPieceCount\":%lu", (unsigned long)info.mixamp_piece_count);
  fputs("}\n", stdout);
  if (curve_owned_by_ctx) sbx_context_destroy(ctx);
  else {
    sbx_context_destroy(ctx);
    sbx_curve_destroy(curve);
  }
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
