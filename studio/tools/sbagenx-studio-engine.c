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

static void print_result_prefix(const char *status, const char *file_type) {
  fputs("{\"status\":", stdout);
  json_print_string(status);
  fputs(",\"backend\":\"sbagenxlib\",\"fileType\":", stdout);
  json_print_string(file_type);
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
    print_result_prefix("unsupported", file_type);
    fputs(",\"message\":", stdout);
    json_print_string("Direct sbagenxlib validation for .sbgf is not implemented on this GUI branch yet.");
    fputs(",\"version\":", stdout);
    json_print_string(sbx_version());
    printf(",\"apiVersion\":%d}\n", sbx_api_version());
    return 0;
  }

  ctx = sbx_context_create(NULL);
  if (!ctx) {
    print_result_prefix("error", file_type);
    fputs(",\"message\":", stdout);
    json_print_string("Failed to create sbagenxlib context");
    fputs("}\n", stdout);
    return 0;
  }

  status = sbx_context_load_sbg_timing_file(ctx, path, 0);
  if (status != SBX_OK) {
    print_result_prefix("error", file_type);
    fputs(",\"message\":", stdout);
    json_print_string(sbx_context_last_error(ctx));
    fputs(",\"version\":", stdout);
    json_print_string(sbx_version());
    printf(",\"apiVersion\":%d}\n", sbx_api_version());
    sbx_context_destroy(ctx);
    return 0;
  }

  print_result_prefix("ok", file_type);
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
