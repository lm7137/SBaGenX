#include <stdio.h>
#include <stdlib.h>
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

static char *read_stdin_all(void) {
  size_t cap = 8192;
  size_t len = 0;
  char *buf = (char *)malloc(cap);
  if (!buf) return NULL;
  while (!feof(stdin)) {
    size_t avail = cap - len;
    size_t n;
    if (avail < 4096) {
      size_t new_cap = cap * 2;
      char *tmp = (char *)realloc(buf, new_cap);
      if (!tmp) {
        free(buf);
        return NULL;
      }
      buf = tmp;
      cap = new_cap;
      avail = cap - len;
    }
    n = fread(buf + len, 1, avail, stdin);
    len += n;
    if (ferror(stdin)) {
      free(buf);
      return NULL;
    }
    if (n == 0) break;
  }
  if (len + 1 > cap) {
    char *tmp = (char *)realloc(buf, len + 1);
    if (!tmp) {
      free(buf);
      return NULL;
    }
    buf = tmp;
  }
  buf[len] = '\0';
  return buf;
}

static char *read_file_all(const char *path) {
  FILE *fp;
  long size;
  size_t nread;
  char *buf;

  fp = fopen(path, "rb");
  if (!fp) return NULL;
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }
  size = ftell(fp);
  if (size < 0) {
    fclose(fp);
    return NULL;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return NULL;
  }
  buf = (char *)malloc((size_t)size + 1u);
  if (!buf) {
    fclose(fp);
    return NULL;
  }
  nread = fread(buf, 1, (size_t)size, fp);
  fclose(fp);
  if (nread != (size_t)size) {
    free(buf);
    return NULL;
  }
  buf[nread] = '\0';
  return buf;
}

static int looks_like_sbg_directive_line(const char *line, size_t len) {
  size_t i = 0;
  while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
  if (i >= len) return 0;
  if (line[i] != '-') return 0;
  i++;
  if (i >= len) return 0;
  return ((line[i] >= 'A' && line[i] <= 'Z') ||
          (line[i] >= 'a' && line[i] <= 'z'));
}

static char *strip_sbg_directive_lines(const char *text) {
  size_t in_len = strlen(text);
  char *out = (char *)malloc(in_len + 1u);
  size_t out_len = 0;
  const char *p = text;
  if (!out) return NULL;
  while (*p) {
    const char *line_start = p;
    const char *line_end = p;
    while (*line_end && *line_end != '\n') line_end++;
    if (!looks_like_sbg_directive_line(line_start, (size_t)(line_end - line_start))) {
      size_t k;
      for (k = 0; k < (size_t)(line_end - line_start); k++)
        out[out_len++] = line_start[k];
      if (*line_end == '\n') out[out_len++] = '\n';
    }
    p = (*line_end == '\n') ? (line_end + 1) : line_end;
  }
  out[out_len] = '\0';
  return out;
}

static int try_load_sbg_text_with_directive_fallback(
    SbxContext *ctx,
    const char *text,
    int *used_strip
) {
  int status = sbx_context_load_sbg_timing_text(ctx, text, 0);
  const char *err;
  char *filtered;
  if (used_strip) *used_strip = 0;
  if (status == SBX_OK) return status;
  err = sbx_context_last_error(ctx);
  if (!err || strstr(err, "invalid SBG timing token '-") == NULL)
    return status;
  filtered = strip_sbg_directive_lines(text);
  if (!filtered) return status;
  status = sbx_context_load_sbg_timing_text(ctx, filtered, 0);
  free(filtered);
  if (status == SBX_OK && used_strip) *used_strip = 1;
  return status;
}

static void print_result_prefix(const char *status, const char *backend, const char *file_type) {
  fputs("{\"status\":", stdout);
  json_print_string(status);
  fputs(",\"backend\":", stdout);
  json_print_string(backend);
  fputs(",\"fileType\":", stdout);
  json_print_string(file_type);
}

static void inspect_sbgf_common(
    const char *source_name,
    int is_text_source,
    const char *text,
    const char *path
) {
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

  if (is_text_source)
    status = sbx_curve_load_text(curve, text, source_name ? source_name : "stdin.sbgf");
  else
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

static void inspect_sbgf_file(const char *path) {
  inspect_sbgf_common(path, 0, NULL, path);
}

static void inspect_sbgf_text(const char *text, const char *source_name) {
  inspect_sbgf_common(source_name, 1, text, NULL);
}

int main(int argc, char **argv) {
  int use_stdin = 0;
  const char *stdin_type = NULL;
  const char *stdin_source = NULL;
  const char *path;
  const char *file_type;
  char *text = NULL;
  SbxContext *ctx;
  int status;
  int i;
  int used_strip = 0;

  if (argc < 2) {
    fputs("usage: sbagenx-studio-engine <file>\n", stderr);
    fputs("       sbagenx-studio-engine --stdin --type <sbg|sbgf> [--source <name>]\n", stderr);
    return 2;
  }

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--stdin") == 0) {
      use_stdin = 1;
      continue;
    }
    if (strcmp(argv[i], "--type") == 0 && i + 1 < argc) {
      stdin_type = argv[++i];
      continue;
    }
    if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) {
      stdin_source = argv[++i];
      continue;
    }
  }

  if (use_stdin) {
    if (!stdin_type ||
        (strcasecmp(stdin_type, "sbg") != 0 && strcasecmp(stdin_type, "sbgf") != 0)) {
      fputs("sbagenx-studio-engine: --stdin requires --type sbg|sbgf\n", stderr);
      return 2;
    }
    text = read_stdin_all();
    if (!text) {
      fputs("sbagenx-studio-engine: failed to read stdin\n", stderr);
      return 2;
    }
    file_type = (strcasecmp(stdin_type, "sbgf") == 0) ? "sbgf" : "sbg";
    if (strcmp(file_type, "sbgf") == 0) {
      inspect_sbgf_text(text, stdin_source);
      free(text);
      return 0;
    }
    ctx = sbx_context_create(NULL);
    if (!ctx) {
      print_result_prefix("error", "sbagenxlib", file_type);
      fputs(",\"message\":", stdout);
      json_print_string("Failed to create sbagenxlib context");
      fputs("}\n", stdout);
      free(text);
      return 0;
    }
    status = try_load_sbg_text_with_directive_fallback(ctx, text, &used_strip);
    free(text);
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
    if (used_strip) {
      fputs(",\"message\":", stdout);
      json_print_string("Validated after stripping top-level CLI directives (for example -T/-Q/-S lines).");
    }
    fputs("}\n", stdout);
    sbx_context_destroy(ctx);
    return 0;
  }

  if (argc != 2) {
    fputs("usage: sbagenx-studio-engine <file>\n", stderr);
    fputs("       sbagenx-studio-engine --stdin --type <sbg|sbgf> [--source <name>]\n", stderr);
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

  text = read_file_all(path);
  if (text)
    status = try_load_sbg_text_with_directive_fallback(ctx, text, &used_strip);
  else
    status = sbx_context_load_sbg_timing_file(ctx, path, 0);
  free(text);
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
  if (used_strip) {
    fputs(",\"message\":", stdout);
    json_print_string("Validated after stripping top-level CLI directives (for example -T/-Q/-S lines).");
  }
  fputs("}\n", stdout);

  sbx_context_destroy(ctx);
  return 0;
}
