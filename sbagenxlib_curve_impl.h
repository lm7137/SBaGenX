void
sbx_default_curve_eval_config(SbxCurveEvalConfig *cfg) {
  if (!cfg) return;
  cfg->carrier_start_hz = 205.0;
  cfg->carrier_end_hz = 200.0;
  cfg->carrier_span_sec = 1800.0;
  cfg->beat_start_hz = 10.0;
  cfg->beat_target_hz = 2.5;
  cfg->beat_span_sec = 1800.0;
  cfg->hold_min = 30.0;
  cfg->total_min = 60.0;
  cfg->wake_min = 3.0;
  cfg->beat_amp0_pct = 100.0;
  cfg->mix_amp0_pct = 100.0;
}

void
sbx_default_curve_source_config(SbxCurveSourceConfig *cfg) {
  if (!cfg) return;
  cfg->mode = SBX_TONE_BINAURAL;
  cfg->waveform = SBX_WAVE_SINE;
  cfg->duty_cycle = 0.403014;
  cfg->iso_start = 0.048493;
  cfg->iso_attack = 0.5;
  cfg->iso_release = 0.5;
  cfg->iso_edge_mode = 2;
  cfg->amplitude = 1.0;
  cfg->duration_sec = 0.0;
  cfg->loop = 0;
}

static int
curve_name_char(int c, int first) {
  return isalpha(c) || c == '_' || (!first && (isdigit(c) || c == '.'));
}

static int
curve_name_eq(const char *a, const char *b) {
  return strcmp(a, b) == 0;
}

static int
curve_name_ieq(const char *a, const char *b) {
  return strcasecmp(a, b) == 0;
}

static char *
curve_trim(char *s) {
  char *e;
  while (*s && isspace((unsigned char)*s)) s++;
  e = s + strlen(s);
  while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
  return s;
}

static int
curve_has_sbgf_ext(const char *path) {
  const char *dot = strrchr(path, '.');
  return dot && curve_name_ieq(dot, ".sbgf");
}

static void
curve_set_error(SbxCurveProgram *curve, const char *fmt, ...) {
  va_list ap;
  if (!curve) return;
  va_start(ap, fmt);
  vsnprintf(curve->last_error, sizeof(curve->last_error), fmt, ap);
  curve->last_error[sizeof(curve->last_error) - 1] = 0;
  va_end(ap);
}

static int
curve_fail(SbxCurveProgram *curve, const char *fmt, ...) {
  va_list ap;
  if (!curve) return SBX_EINVAL;
  va_start(ap, fmt);
  vsnprintf(curve->last_error, sizeof(curve->last_error), fmt, ap);
  curve->last_error[sizeof(curve->last_error) - 1] = 0;
  va_end(ap);
  return SBX_EINVAL;
}

static int
curve_name_reserved(const char *name) {
  static const char *reserved[] = {
    "t", "m", "D", "H", "T", "U",
    "b0", "b1", "c0", "c1", "a0", "m0",
    "ifelse", "step", "clamp", "lerp", "ramp",
    "smoothstep", "smootherstep", "between", "pulse",
    "lt", "le", "gt", "ge", "eq", "ne",
    "seg", "min2", "max2", "param", "solve",
    "beat", "carrier", "amp", "mixamp",
    "mixspin_width", "mixspin_hz", "mixspin_amp",
    "mixpulse_hz", "mixpulse_amp",
    "mixbeat_hz", "mixbeat_amp",
    "mixam_hz"
  };
  size_t i;
  for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
    if (curve_name_eq(name, reserved[i])) return 1;
  return 0;
}

static int
curve_param_index(const SbxCurveProgram *curve, const char *name) {
  int i;
  if (!curve || !name) return -1;
  for (i = 0; i < curve->param_count; i++) {
    if (curve_name_eq(name, curve->param_names[i])) return i;
  }
  return -1;
}

static void
curve_expr_target_clear_compiled(SbxCurveExprTarget *target) {
  int i;
  if (!target) return;
  if (target->expr) te_free(target->expr);
  target->expr = 0;
  for (i = 0; i < SBX_CURVE_MAX_PIECES; i++) {
    if (target->piece_cond[i]) te_free(target->piece_cond[i]);
    if (target->piece_expr[i]) te_free(target->piece_expr[i]);
    target->piece_cond[i] = 0;
    target->piece_expr[i] = 0;
  }
}

static void
curve_expr_target_clear_loaded(SbxCurveExprTarget *target) {
  if (!target) return;
  curve_expr_target_clear_compiled(target);
  memset(target->expr_src, 0, sizeof(target->expr_src));
  target->has_expr = 0;
  target->piece_count = 0;
  memset(target->piece_cond_src, 0, sizeof(target->piece_cond_src));
  memset(target->piece_expr_src, 0, sizeof(target->piece_expr_src));
}

static int
curve_expr_target_active(const SbxCurveExprTarget *target) {
  return target && (target->has_expr || target->piece_count > 0);
}

static void
sbx_curve_clear_compiled(SbxCurveProgram *curve) {
  int i;
  if (!curve) return;
  if (curve->beat_expr) te_free(curve->beat_expr);
  if (curve->carrier_expr) te_free(curve->carrier_expr);
  if (curve->amp_expr) te_free(curve->amp_expr);
  if (curve->mixamp_expr) te_free(curve->mixamp_expr);
  curve->beat_expr = 0;
  curve->carrier_expr = 0;
  curve->amp_expr = 0;
  curve->mixamp_expr = 0;
  for (i = 0; i < SBX_CURVE_MAX_PIECES; i++) {
    if (curve->beat_piece_cond[i]) te_free(curve->beat_piece_cond[i]);
    if (curve->beat_piece_expr[i]) te_free(curve->beat_piece_expr[i]);
    if (curve->carrier_piece_cond[i]) te_free(curve->carrier_piece_cond[i]);
    if (curve->carrier_piece_expr[i]) te_free(curve->carrier_piece_expr[i]);
    if (curve->amp_piece_cond[i]) te_free(curve->amp_piece_cond[i]);
    if (curve->amp_piece_expr[i]) te_free(curve->amp_piece_expr[i]);
    if (curve->mixamp_piece_cond[i]) te_free(curve->mixamp_piece_cond[i]);
    if (curve->mixamp_piece_expr[i]) te_free(curve->mixamp_piece_expr[i]);
    curve->beat_piece_cond[i] = 0;
    curve->beat_piece_expr[i] = 0;
    curve->carrier_piece_cond[i] = 0;
    curve->carrier_piece_expr[i] = 0;
    curve->amp_piece_cond[i] = 0;
    curve->amp_piece_expr[i] = 0;
    curve->mixamp_piece_cond[i] = 0;
    curve->mixamp_piece_expr[i] = 0;
  }
  for (i = 0; i < SBX_CURVE_MIXFX_PARAM_COUNT; i++)
    curve_expr_target_clear_compiled(&curve->mixfx_targets[i]);
  curve->prepared = 0;
}

static void
sbx_curve_clear_loaded(SbxCurveProgram *curve) {
  if (!curve) return;
  sbx_curve_clear_compiled(curve);
  memset(curve->src_file, 0, sizeof(curve->src_file));
  memset(curve->beat_expr_src, 0, sizeof(curve->beat_expr_src));
  memset(curve->carrier_expr_src, 0, sizeof(curve->carrier_expr_src));
  memset(curve->amp_expr_src, 0, sizeof(curve->amp_expr_src));
  memset(curve->mixamp_expr_src, 0, sizeof(curve->mixamp_expr_src));
  curve->has_carrier_expr = 0;
  curve->has_amp_expr = 0;
  curve->has_mixamp_expr = 0;
  curve->beat_piece_count = 0;
  curve->carrier_piece_count = 0;
  curve->amp_piece_count = 0;
  curve->mixamp_piece_count = 0;
  curve->has_solve = 0;
  curve->solve_unknown_count = 0;
  curve->solve_eq_count = 0;
  curve->param_count = 0;
  memset(curve->beat_piece_cond_src, 0, sizeof(curve->beat_piece_cond_src));
  memset(curve->beat_piece_expr_src, 0, sizeof(curve->beat_piece_expr_src));
  memset(curve->carrier_piece_cond_src, 0, sizeof(curve->carrier_piece_cond_src));
  memset(curve->carrier_piece_expr_src, 0, sizeof(curve->carrier_piece_expr_src));
  memset(curve->amp_piece_cond_src, 0, sizeof(curve->amp_piece_cond_src));
  memset(curve->amp_piece_expr_src, 0, sizeof(curve->amp_piece_expr_src));
  memset(curve->mixamp_piece_cond_src, 0, sizeof(curve->mixamp_piece_cond_src));
  memset(curve->mixamp_piece_expr_src, 0, sizeof(curve->mixamp_piece_expr_src));
  memset(curve->solve_unknown_names, 0, sizeof(curve->solve_unknown_names));
  memset(curve->solve_unknown_param_idx, 0xff, sizeof(curve->solve_unknown_param_idx));
  memset(curve->solve_eq_lhs_src, 0, sizeof(curve->solve_eq_lhs_src));
  memset(curve->solve_eq_rhs_src, 0, sizeof(curve->solve_eq_rhs_src));
  memset(curve->param_names, 0, sizeof(curve->param_names));
  memset(curve->param_values, 0, sizeof(curve->param_values));
  {
    int i;
    for (i = 0; i < SBX_CURVE_MIXFX_PARAM_COUNT; i++)
      curve_expr_target_clear_loaded(&curve->mixfx_targets[i]);
  }
  memset(&curve->cfg, 0, sizeof(curve->cfg));
  curve->loaded = 0;
}

SbxCurveProgram *
sbx_curve_create(void) {
  SbxCurveProgram *curve = (SbxCurveProgram *)calloc(1, sizeof(*curve));
  if (!curve) return NULL;
  memset(curve->solve_unknown_param_idx, 0xff, sizeof(curve->solve_unknown_param_idx));
  return curve;
}

void
sbx_curve_destroy(SbxCurveProgram *curve) {
  if (!curve) return;
  sbx_curve_clear_loaded(curve);
  free(curve);
}

void
sbx_curve_reset(SbxCurveProgram *curve) {
  if (!curve) return;
  curve->last_error[0] = 0;
  sbx_curve_clear_loaded(curve);
}

const char *
sbx_curve_last_error(const SbxCurveProgram *curve) {
  return curve ? curve->last_error : "";
}

const char *
sbx_curve_source_name(const SbxCurveProgram *curve) {
  return curve ? curve->src_file : "";
}

size_t
sbx_curve_param_count(const SbxCurveProgram *curve) {
  return curve ? (size_t)curve->param_count : 0;
}

int
sbx_curve_get_param(const SbxCurveProgram *curve,
                    size_t index,
                    const char **out_name,
                    double *out_value) {
  if (!curve) return SBX_EINVAL;
  if (index >= (size_t)curve->param_count) return SBX_EINVAL;
  if (out_name) *out_name = curve->param_names[index];
  if (out_value) *out_value = curve->param_values[index];
  return SBX_OK;
}

int
sbx_curve_get_info(const SbxCurveProgram *curve, SbxCurveInfo *out_info) {
  if (!curve || !out_info) return SBX_EINVAL;
  memset(out_info, 0, sizeof(*out_info));
  out_info->parameter_count = (size_t)curve->param_count;
  out_info->has_solve = curve->has_solve;
  out_info->has_carrier_expr = curve->has_carrier_expr;
  out_info->has_amp_expr = curve->has_amp_expr;
  out_info->has_mixamp_expr = curve->has_mixamp_expr;
  out_info->beat_piece_count = (size_t)curve->beat_piece_count;
  out_info->carrier_piece_count = (size_t)curve->carrier_piece_count;
  out_info->amp_piece_count = (size_t)curve->amp_piece_count;
  out_info->mixamp_piece_count = (size_t)curve->mixamp_piece_count;
  return SBX_OK;
}

static int
curve_set_param_value(SbxCurveProgram *curve, const char *name, double value, int allow_new) {
  int idx;
  size_t nlen;
  if (!curve || !name || !*name) return SBX_EINVAL;
  idx = curve_param_index(curve, name);
  if (idx < 0) {
    if (!allow_new)
      return curve_fail(curve, "Unknown .sbgf parameter override: %s", name);
    if (curve->param_count >= SBX_CURVE_MAX_PARAMS)
      return curve_fail(curve, "Too many .sbgf parameters (max %d)", SBX_CURVE_MAX_PARAMS);
    if (curve_name_reserved(name))
      return curve_fail(curve, "Parameter name '%s' is reserved in .sbgf", name);
    nlen = strlen(name);
    if (nlen >= SBX_CURVE_NAME_MAX)
      return curve_fail(curve, "Parameter name '%s' is too long", name);
    idx = curve->param_count++;
    memcpy(curve->param_names[idx], name, nlen + 1);
  }
  curve->param_values[idx] = value;
  sbx_curve_clear_compiled(curve);
  return SBX_OK;
}

int
sbx_curve_set_param(SbxCurveProgram *curve, const char *name, double value) {
  return curve_set_param_value(curve, name, value, 1);
}

static double curve_fn_ifelse(double cond, double a, double b) { return cond != 0.0 ? a : b; }
static double curve_fn_step(double x) { return x >= 0.0 ? 1.0 : 0.0; }
static double curve_fn_clamp(double x, double lo, double hi) {
  double t;
  if (lo > hi) { t = lo; lo = hi; hi = t; }
  return sbx_dsp_clamp(x, lo, hi);
}
static double curve_fn_lerp(double a, double b, double u) { return a + (b - a) * u; }
static double curve_fn_ramp(double x, double x0, double x1) { if (x1 == x0) return x >= x1 ? 1.0 : 0.0; return curve_fn_clamp((x - x0) / (x1 - x0), 0.0, 1.0); }
static double curve_fn_smoothstep(double edge0, double edge1, double x) {
  double u = curve_fn_ramp(x, edge0, edge1);
  return sbx_dsp_smoothstep01(u);
}
static double curve_fn_smootherstep(double edge0, double edge1, double x) {
  double u = curve_fn_ramp(x, edge0, edge1);
  return sbx_dsp_smootherstep01(u);
}
static double curve_fn_between(double x, double a, double b) {
  if (a > b) { double t = a; a = b; b = t; }
  return (x >= a && x <= b) ? 1.0 : 0.0;
}
static double curve_fn_pulse(double x, double start, double end) { return curve_fn_between(x, start, end); }
static double curve_fn_lt(double a, double b) { return a < b ? 1.0 : 0.0; }
static double curve_fn_le(double a, double b) { return a <= b ? 1.0 : 0.0; }
static double curve_fn_gt(double a, double b) { return a > b ? 1.0 : 0.0; }
static double curve_fn_ge(double a, double b) { return a >= b ? 1.0 : 0.0; }
static double curve_fn_eq(double a, double b) { return a == b ? 1.0 : 0.0; }
static double curve_fn_ne(double a, double b) { return a != b ? 1.0 : 0.0; }
static double curve_fn_seg(double x, double x0, double x1, double y0, double y1) {
  if (x <= x0) return y0;
  if (x >= x1) return y1;
  if (x1 == x0) return y1;
  return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}
static double curve_fn_min2(double a, double b) { return a < b ? a : b; }
static double curve_fn_max2(double a, double b) { return a > b ? a : b; }

static int
curve_parse_param_line(SbxCurveProgram *curve, char *line, int lno, const char *path) {
  char name[SBX_CURVE_NAME_MAX];
  char *p = curve_trim(line);
  double mult = 1.0;
  double v;
  int i = 0;
  char *endp = 0;

  if (!curve_name_char((unsigned char)*p, 1))
    return curve_fail(curve, "%s:%d: bad parameter name near '%s'", path, lno, p);
  while (*p && curve_name_char((unsigned char)*p, i == 0)) {
    if (i >= SBX_CURVE_NAME_MAX - 1)
      return curve_fail(curve, "%s:%d: parameter name is too long", path, lno);
    name[i++] = *p++;
  }
  name[i] = 0;
  p = curve_trim(p);
  if (*p != '=')
    return curve_fail(curve, "%s:%d: expected '=' after parameter name '%s'", path, lno, name);
  p = curve_trim(p + 1);
  if (!*p)
    return curve_fail(curve, "%s:%d: parameter '%s' has empty value", path, lno, name);
  errno = 0;
  v = strtod(p, &endp);
  if (endp == p || errno == ERANGE)
    return curve_fail(curve, "%s:%d: bad numeric value for parameter '%s'", path, lno, name);
  p = curve_trim(endp);
  if (*p == 'm' || *p == 'M') {
    mult = 1.0;
    p = curve_trim(p + 1);
  } else if (*p == 's' || *p == 'S') {
    mult = 1.0 / 60.0;
    p = curve_trim(p + 1);
  }
  if (*p)
    return curve_fail(curve, "%s:%d: trailing rubbish after parameter '%s'", path, lno, name);
  return curve_set_param_value(curve, name, v * mult, 1);
}

static int
curve_parse_expr_line(SbxCurveProgram *curve, char *p, int lno, const char *path,
                      const char *kind, char *dst, size_t dstsz) {
  char *eq = strchr(p, '=');
  char *expr;
  if (!eq)
    return curve_fail(curve, "%s:%d: expected '%s = <expression>'", path, lno, kind);
  expr = curve_trim(eq + 1);
  if (!*expr)
    return curve_fail(curve, "%s:%d: empty expression for %s", path, lno, kind);
  if (strlen(expr) >= dstsz)
    return curve_fail(curve, "%s:%d: %s expression is too long", path, lno, kind);
  strcpy(dst, expr);
  return SBX_OK;
}

static int
curve_parse_piece_line(SbxCurveProgram *curve, char *s, int lno, const char *path,
                       const char *kind, int kind_len,
                       char cond_dst[SBX_CURVE_MAX_PIECES][SBX_CURVE_EXPR_MAX],
                       char expr_dst[SBX_CURVE_MAX_PIECES][SBX_CURVE_EXPR_MAX],
                       int *piece_count) {
  char *p = s + kind_len;
  char *eq;
  char *rhs;
  char *expr;
  const char *cmp_name;

  if (*p != '<' && *p != '>') return 0;
  if (*p == '<') {
    p++;
    cmp_name = (*p == '=') ? (p++, "le") : "lt";
  } else {
    p++;
    cmp_name = (*p == '=') ? (p++, "ge") : "gt";
  }
  while (*p && isspace((unsigned char)*p)) p++;
  eq = strchr(p, '=');
  if (!eq) {
    curve_fail(curve, "%s:%d: %s piecewise line must be '%s<expr = <expression>'", path, lno, kind, kind);
    return -1;
  }
  *eq = 0;
  rhs = curve_trim(p);
  expr = curve_trim(eq + 1);
  if (!*rhs) {
    curve_fail(curve, "%s:%d: missing threshold expression in %s piecewise line", path, lno, kind);
    return -1;
  }
  if (!*expr) {
    curve_fail(curve, "%s:%d: empty expression in %s piecewise line", path, lno, kind);
    return -1;
  }
  if (*piece_count >= SBX_CURVE_MAX_PIECES) {
    curve_fail(curve, "%s:%d: too many %s piecewise lines (max %d)", path, lno, kind, SBX_CURVE_MAX_PIECES);
    return -1;
  }
  if (strlen(rhs) + 16 >= SBX_CURVE_EXPR_MAX) {
    curve_fail(curve, "%s:%d: %s piecewise condition is too long", path, lno, kind);
    return -1;
  }
  if (strlen(expr) >= SBX_CURVE_EXPR_MAX) {
    curve_fail(curve, "%s:%d: %s piecewise expression is too long", path, lno, kind);
    return -1;
  }
  snprintf(cond_dst[*piece_count], SBX_CURVE_EXPR_MAX, "%s(m,%s)", cmp_name, rhs);
  strcpy(expr_dst[*piece_count], expr);
  (*piece_count)++;
  return 1;
}

static int
curve_parse_named_target_line(SbxCurveProgram *curve,
                              char *s,
                              int lno,
                              const char *path,
                              const char *kind,
                              int kind_len,
                              SbxCurveExprTarget *target) {
  int prc;
  if (!curve || !s || !kind || !target) return SBX_EINVAL;
  prc = curve_parse_piece_line(curve, s, lno, path, kind, kind_len,
                               target->piece_cond_src,
                               target->piece_expr_src,
                               &target->piece_count);
  if (prc < 0) return SBX_EINVAL;
  if (prc > 0) {
    if (target->has_expr)
      return curve_fail(curve, "%s:%d: cannot mix '%s = ...' with piecewise %s<... lines",
                        path, lno, kind, kind);
    return SBX_OK;
  }
  if (target->piece_count > 0)
    return curve_fail(curve, "%s:%d: cannot mix piecewise %s<... lines with '%s = ...'",
                      path, lno, kind, kind);
  prc = curve_parse_expr_line(curve, s, lno, path, kind,
                              target->expr_src, sizeof(target->expr_src));
  if (prc != SBX_OK) return prc;
  target->has_expr = 1;
  return SBX_OK;
}

static int
curve_parse_solve_line(SbxCurveProgram *curve, char *line, int lno, const char *path) {
  char *p = curve_trim(line);
  char *colon;
  char *eqs;
  int nunk = 0, neq = 0;

  if (curve->has_solve)
    return curve_fail(curve, "%s:%d: only one 'solve ...' line is supported per .sbgf file", path, lno);
  if (!*p)
    return curve_fail(curve, "%s:%d: solve line requires unknown names and equations", path, lno);
  colon = strchr(p, ':');
  if (!colon)
    return curve_fail(curve, "%s:%d: solve line must use ':' between unknown names and equations", path, lno);
  *colon = 0;
  p = curve_trim(p);
  eqs = curve_trim(colon + 1);
  if (!*p)
    return curve_fail(curve, "%s:%d: solve line is missing unknown names before ':'", path, lno);
  if (!*eqs)
    return curve_fail(curve, "%s:%d: solve line is missing equations after ':'", path, lno);

  while (*p) {
    char name[SBX_CURVE_NAME_MAX];
    int i = 0;
    if (!curve_name_char((unsigned char)*p, 1))
      return curve_fail(curve, "%s:%d: bad solve unknown name near '%s'", path, lno, p);
    while (*p && curve_name_char((unsigned char)*p, i == 0)) {
      if (i >= SBX_CURVE_NAME_MAX - 1)
        return curve_fail(curve, "%s:%d: solve unknown name is too long", path, lno);
      name[i++] = *p++;
    }
    name[i] = 0;
    for (i = 0; i < nunk; i++)
      if (curve_name_eq(name, curve->solve_unknown_names[i]))
        return curve_fail(curve, "%s:%d: duplicate solve unknown '%s'", path, lno, name);
    if (curve_name_reserved(name))
      return curve_fail(curve, "%s:%d: solve unknown '%s' uses a reserved name", path, lno, name);
    if (nunk >= SBX_CURVE_MAX_SOLVE_UNK)
      return curve_fail(curve, "%s:%d: too many solve unknowns (max %d)", path, lno, SBX_CURVE_MAX_SOLVE_UNK);
    strcpy(curve->solve_unknown_names[nunk], name);
    if (curve_param_index(curve, name) < 0) {
      int rc = curve_set_param_value(curve, name, 0.0, 1);
      if (rc != SBX_OK) return rc;
    }
    nunk++;
    p = curve_trim(p);
    if (!*p) break;
    if (*p != ',')
      return curve_fail(curve, "%s:%d: expected ',' between solve unknown names near '%s'", path, lno, p);
    p = curve_trim(p + 1);
  }

  while (*eqs) {
    char *seg = curve_trim(eqs);
    char *semi;
    char *eq;
    char *lhs;
    char *rhs;
    if (!*seg) break;
    semi = strchr(seg, ';');
    if (semi) {
      *semi = 0;
      eqs = semi + 1;
    } else {
      eqs = seg + strlen(seg);
    }
    if (neq >= SBX_CURVE_MAX_SOLVE_EQ)
      return curve_fail(curve, "%s:%d: too many solve equations (max %d)", path, lno, SBX_CURVE_MAX_SOLVE_EQ);
    eq = strchr(seg, '=');
    if (!eq)
      return curve_fail(curve, "%s:%d: solve equation #%d must contain '='", path, lno, neq + 1);
    *eq = 0;
    lhs = curve_trim(seg);
    rhs = curve_trim(eq + 1);
    if (!*lhs || !*rhs)
      return curve_fail(curve, "%s:%d: solve equation #%d must have both lhs and rhs expressions", path, lno, neq + 1);
    if (strlen(lhs) >= SBX_CURVE_EXPR_MAX || strlen(rhs) >= SBX_CURVE_EXPR_MAX)
      return curve_fail(curve, "%s:%d: solve equation #%d is too long", path, lno, neq + 1);
    strcpy(curve->solve_eq_lhs_src[neq], lhs);
    strcpy(curve->solve_eq_rhs_src[neq], rhs);
    neq++;
  }

  if (nunk <= 0)
    return curve_fail(curve, "%s:%d: solve line must declare at least one unknown", path, lno);
  if (neq <= 0)
    return curve_fail(curve, "%s:%d: solve line must include at least one equation", path, lno);
  if (neq != nunk)
    return curve_fail(curve, "%s:%d: solve currently requires a square system (#equations == #unknowns)", path, lno);

  curve->has_solve = 1;
  curve->solve_unknown_count = nunk;
  curve->solve_eq_count = neq;
  return SBX_OK;
}

static double
curve_vec_norm_inf(int n, const double *v) {
  int i;
  double m = 0.0;
  for (i = 0; i < n; i++) {
    double a = fabs(v[i]);
    if (a > m) m = a;
  }
  return m;
}

static int
curve_linear_solve(int n,
                   double a[SBX_CURVE_MAX_SOLVE_UNK][SBX_CURVE_MAX_SOLVE_UNK],
                   double b[SBX_CURVE_MAX_SOLVE_UNK],
                   double x[SBX_CURVE_MAX_SOLVE_UNK]) {
  int i, j, k, piv;
  for (i = 0; i < n; i++) x[i] = 0.0;
  for (i = 0; i < n; i++) {
    double best = fabs(a[i][i]);
    piv = i;
    for (j = i + 1; j < n; j++) {
      double v = fabs(a[j][i]);
      if (v > best) {
        best = v;
        piv = j;
      }
    }
    if (best < 1e-12) return 0;
    if (piv != i) {
      for (k = i; k < n; k++) {
        double tmp = a[i][k];
        a[i][k] = a[piv][k];
        a[piv][k] = tmp;
      }
      {
        double tmp = b[i];
        b[i] = b[piv];
        b[piv] = tmp;
      }
    }
    {
      double div = a[i][i];
      for (k = i; k < n; k++) a[i][k] /= div;
      b[i] /= div;
    }
    for (j = 0; j < n; j++) {
      double f;
      if (j == i) continue;
      f = a[j][i];
      if (fabs(f) < 1e-18) continue;
      for (k = i; k < n; k++) a[j][k] -= f * a[i][k];
      b[j] -= f * b[i];
    }
  }
  for (i = 0; i < n; i++) x[i] = b[i];
  return 1;
}

static void
curve_set_unknown_vector(SbxCurveProgram *curve, int n, const double *x) {
  int i;
  for (i = 0; i < n; i++) {
    int idx = curve->solve_unknown_param_idx[i];
    if (idx >= 0 && idx < curve->param_count)
      curve->param_values[idx] = x[i];
  }
}

static int
curve_eval_solve_residuals(int n, te_expr **lhs, te_expr **rhs, double *resid) {
  int i;
  for (i = 0; i < n; i++) {
    double lv = te_eval(lhs[i]);
    double rv = te_eval(rhs[i]);
    if (!isfinite(lv) || !isfinite(rv)) return 0;
    resid[i] = lv - rv;
  }
  return 1;
}

static int
curve_apply_solve(SbxCurveProgram *curve, te_variable *vars, int vcnt) {
  te_expr *lhs[SBX_CURVE_MAX_SOLVE_EQ];
  te_expr *rhs[SBX_CURVE_MAX_SOLVE_EQ];
  double x[SBX_CURVE_MAX_SOLVE_UNK];
  double r0[SBX_CURVE_MAX_SOLVE_EQ];
  double r1[SBX_CURVE_MAX_SOLVE_EQ];
  double r2[SBX_CURVE_MAX_SOLVE_EQ];
  double jac[SBX_CURVE_MAX_SOLVE_UNK][SBX_CURVE_MAX_SOLVE_UNK];
  int n, i, j, iter, ok = 0;
  double best = 0.0;
  int err_lhs = 0, err_rhs = 0;

  if (!curve->has_solve) return SBX_OK;
  n = curve->solve_unknown_count;
  if (n <= 0 || n > SBX_CURVE_MAX_SOLVE_UNK)
    return curve_fail(curve, "%s: internal solve setup error (unknown count=%d)", curve->src_file, n);
  if (curve->solve_eq_count != n)
    return curve_fail(curve, "%s: solve requires equal equation/unknown counts", curve->src_file);

  memset(lhs, 0, sizeof(lhs));
  memset(rhs, 0, sizeof(rhs));

  for (i = 0; i < n; i++) {
    int idx = curve_param_index(curve, curve->solve_unknown_names[i]);
    if (idx < 0)
      goto solve_unknown_fail;
    curve->solve_unknown_param_idx[i] = idx;
    x[i] = curve->param_values[idx];
  }
  curve_set_unknown_vector(curve, n, x);

  for (i = 0; i < n; i++) {
    err_lhs = 0;
    lhs[i] = te_compile(curve->solve_eq_lhs_src[i], vars, vcnt, &err_lhs);
    if (!lhs[i]) goto solve_compile_lhs_fail;
    err_rhs = 0;
    rhs[i] = te_compile(curve->solve_eq_rhs_src[i], vars, vcnt, &err_rhs);
    if (!rhs[i]) goto solve_compile_rhs_fail;
  }

  for (iter = 0; iter < 40; iter++) {
    double step[SBX_CURVE_MAX_SOLVE_UNK];
    double snorm;
    int accepted = 0;
    double alpha;

    curve_set_unknown_vector(curve, n, x);
    if (!curve_eval_solve_residuals(n, lhs, rhs, r0))
      goto solve_eval_fail;
    best = curve_vec_norm_inf(n, r0);
    if (best < 1e-9) { ok = 1; break; }

    for (j = 0; j < n; j++) {
      double xj = x[j];
      double h = 1e-6 * (fabs(xj) + 1.0);
      int good_plus, good_minus;

      x[j] = xj + h;
      curve_set_unknown_vector(curve, n, x);
      good_plus = curve_eval_solve_residuals(n, lhs, rhs, r1);

      x[j] = xj - h;
      curve_set_unknown_vector(curve, n, x);
      good_minus = curve_eval_solve_residuals(n, lhs, rhs, r2);

      x[j] = xj;
      curve_set_unknown_vector(curve, n, x);

      if (good_plus && good_minus) {
        for (i = 0; i < n; i++) jac[i][j] = (r1[i] - r2[i]) / (2.0 * h);
      } else if (good_plus) {
        for (i = 0; i < n; i++) jac[i][j] = (r1[i] - r0[i]) / h;
      } else {
        goto solve_jac_fail;
      }
    }

    {
      double a[SBX_CURVE_MAX_SOLVE_UNK][SBX_CURVE_MAX_SOLVE_UNK];
      double b[SBX_CURVE_MAX_SOLVE_UNK];
      for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) a[i][j] = jac[i][j];
        b[i] = -r0[i];
      }
      if (!curve_linear_solve(n, a, b, step))
        goto solve_singular_fail;
    }

    snorm = curve_vec_norm_inf(n, step);
    alpha = 1.0;
    for (i = 0; i < 12; i++) {
      double trial[SBX_CURVE_MAX_SOLVE_UNK];
      double trial_norm;
      for (j = 0; j < n; j++) trial[j] = x[j] + alpha * step[j];
      curve_set_unknown_vector(curve, n, trial);
      if (!curve_eval_solve_residuals(n, lhs, rhs, r1)) {
        alpha *= 0.5;
        continue;
      }
      trial_norm = curve_vec_norm_inf(n, r1);
      if (trial_norm < best || trial_norm < 1e-9) {
        for (j = 0; j < n; j++) x[j] = trial[j];
        best = trial_norm;
        accepted = 1;
        break;
      }
      alpha *= 0.5;
    }
    if (!accepted) {
      if (snorm < 1e-12 && best < 1e-7) { ok = 1; break; }
      goto solve_descent_fail;
    }
    if (best < 1e-9 || alpha * snorm < 1e-12) { ok = 1; break; }
  }

  if (!ok) goto solve_converge_fail;

  curve_set_unknown_vector(curve, n, x);
  for (i = 0; i < n; i++) {
    if (lhs[i]) te_free(lhs[i]);
    if (rhs[i]) te_free(rhs[i]);
  }
  return SBX_OK;

solve_unknown_fail:
  for (j = 0; j < n; j++) {
    if (lhs[j]) te_free(lhs[j]);
    if (rhs[j]) te_free(rhs[j]);
  }
  return curve_fail(curve, "%s: solve unknown '%s' is not a defined parameter", curve->src_file, curve->solve_unknown_names[i]);
solve_compile_lhs_fail:
  for (j = 0; j < n; j++) {
    if (lhs[j]) te_free(lhs[j]);
    if (rhs[j]) te_free(rhs[j]);
  }
  return curve_fail(curve, "%s: solve equation #%d lhs failed near column %d", curve->src_file, i + 1, err_lhs);
solve_compile_rhs_fail:
  for (j = 0; j < n; j++) {
    if (lhs[j]) te_free(lhs[j]);
    if (rhs[j]) te_free(rhs[j]);
  }
  return curve_fail(curve, "%s: solve equation #%d rhs failed near column %d", curve->src_file, i + 1, err_rhs);
solve_eval_fail:
  for (j = 0; j < n; j++) {
    if (lhs[j]) te_free(lhs[j]);
    if (rhs[j]) te_free(rhs[j]);
  }
  return curve_fail(curve, "%s: solve evaluation failed at iteration %d (non-finite residual)", curve->src_file, iter + 1);
solve_jac_fail:
  for (j = 0; j < n; j++) {
    if (lhs[j]) te_free(lhs[j]);
    if (rhs[j]) te_free(rhs[j]);
  }
  return curve_fail(curve, "%s: solve Jacobian failed for unknown '%s' at iteration %d", curve->src_file, curve->solve_unknown_names[j], iter + 1);
solve_singular_fail:
  for (j = 0; j < n; j++) {
    if (lhs[j]) te_free(lhs[j]);
    if (rhs[j]) te_free(rhs[j]);
  }
  return curve_fail(curve, "%s: solve Jacobian is singular/ill-conditioned at iteration %d", curve->src_file, iter + 1);
solve_descent_fail:
  for (j = 0; j < n; j++) {
    if (lhs[j]) te_free(lhs[j]);
    if (rhs[j]) te_free(rhs[j]);
  }
  return curve_fail(curve, "%s: solve failed to find a descent step at iteration %d", curve->src_file, iter + 1);
solve_converge_fail:
  for (j = 0; j < n; j++) {
    if (lhs[j]) te_free(lhs[j]);
    if (rhs[j]) te_free(rhs[j]);
  }
  return curve_fail(curve, "%s: solve did not converge after 40 iterations", curve->src_file);
}

static void
curve_set_eval_time(SbxCurveProgram *curve, double t_sec) {
  double t;
  if (!curve) return;
  t = t_sec;
  if (t < 0.0) t = 0.0;
  if (t > curve->cfg.carrier_span_sec) t = curve->cfg.carrier_span_sec;
  curve->ev_t = t;
  curve->ev_m = t / 60.0;
}

static int
curve_compile_expr_target(SbxCurveProgram *curve,
                          SbxCurveExprTarget *target,
                          const char *kind,
                          te_variable *vars,
                          int vcnt) {
  int i;
  int err = 0;
  if (!curve || !target || !kind) return SBX_EINVAL;
  if (target->piece_count > 0) {
    for (i = 0; i < target->piece_count; i++) {
      target->piece_cond[i] = te_compile(target->piece_cond_src[i], vars, vcnt, &err);
      if (!target->piece_cond[i]) {
        sbx_curve_clear_compiled(curve);
        curve_set_error(curve, "%s: error in %s piece condition #%d near column %d",
                        curve->src_file, kind, i + 1, err);
        return SBX_EINVAL;
      }
      err = 0;
      target->piece_expr[i] = te_compile(target->piece_expr_src[i], vars, vcnt, &err);
      if (!target->piece_expr[i]) {
        sbx_curve_clear_compiled(curve);
        curve_set_error(curve, "%s: error in %s piece expression #%d near column %d",
                        curve->src_file, kind, i + 1, err);
        return SBX_EINVAL;
      }
      err = 0;
    }
  } else if (target->has_expr) {
    target->expr = te_compile(target->expr_src, vars, vcnt, &err);
    if (!target->expr) {
      sbx_curve_clear_compiled(curve);
      curve_set_error(curve, "%s: error in %s expression near column %d",
                      curve->src_file, kind, err);
      return SBX_EINVAL;
    }
  }
  return SBX_OK;
}

static int
curve_eval_expr_target(SbxCurveProgram *curve,
                       const SbxCurveExprTarget *target,
                       const char *kind,
                       double default_value,
                       double *out_value) {
  int i, matched;
  double condv, value;
  if (!curve || !target || !kind || !out_value) return SBX_EINVAL;
  value = default_value;
  if (target->piece_count > 0) {
    matched = 0;
    for (i = 0; i < target->piece_count; i++) {
      if (!target->piece_cond[i] || !target->piece_expr[i])
        return curve_fail(curve, "Curve %s piece state is incomplete", kind);
      condv = te_eval(target->piece_cond[i]);
      if (!isfinite(condv))
        return curve_fail(curve, "Curve %s piece condition evaluated non-finite", kind);
      if (condv != 0.0) {
        value = te_eval(target->piece_expr[i]);
        matched = 1;
        break;
      }
    }
    if (!matched)
      value = te_eval(target->piece_expr[target->piece_count - 1]);
  } else if (target->has_expr && target->expr) {
    value = te_eval(target->expr);
  }
  if (!isfinite(value))
    return curve_fail(curve, "Curve %s evaluated non-finite", kind);
  *out_value = value;
  return SBX_OK;
}

int
sbx_curve_load_text(SbxCurveProgram *curve, const char *text, const char *source_name) {
  char *buf, *line, *saveptr;
  int lno = 0;
  int have_beat = 0;

  if (!curve || !text) return SBX_EINVAL;
  sbx_curve_reset(curve);
  if (source_name && *source_name)
    snprintf(curve->src_file, sizeof(curve->src_file), "%s", source_name);
  else
    snprintf(curve->src_file, sizeof(curve->src_file), "<memory>.sbgf");

  if (!curve_has_sbgf_ext(curve->src_file)) {
    int is_virtual_source = (curve->src_file[0] == '<' && strchr(curve->src_file, '>') != 0);
    if (!is_virtual_source)
      return curve_fail(curve, "Curve source must use .sbgf extension: %s", curve->src_file);
  }

  buf = strdup(text);
  if (!buf)
    return curve_fail(curve, "Out of memory loading %s", curve->src_file);

  saveptr = 0;
  for (line = strtok_r(buf, "\n", &saveptr); line; line = strtok_r(NULL, "\n", &saveptr)) {
    char *s;
    int prc;
    lno++;
    if (strchr(line, '\r')) *strchr(line, '\r') = 0;
    s = curve_trim(line);
    if (!*s || *s == '#' || *s == ';') continue;
    if (s[0] == '/' && s[1] == '/') continue;

    if (!strncmp(s, "param", 5) && isspace((unsigned char)s[5])) {
      int rc = curve_parse_param_line(curve, s + 5, lno, curve->src_file);
      if (rc != SBX_OK) { free(buf); return rc; }
      continue;
    }
    if (!strncmp(s, "solve", 5) && isspace((unsigned char)s[5])) {
      int rc = curve_parse_solve_line(curve, s + 5, lno, curve->src_file);
      if (rc != SBX_OK) { free(buf); return rc; }
      continue;
    }
    if (!strncmp(s, "beat", 4) &&
        (isspace((unsigned char)s[4]) || s[4] == '=' || s[4] == '<' || s[4] == '>')) {
      prc = curve_parse_piece_line(curve, s, lno, curve->src_file, "beat", 4,
                                   curve->beat_piece_cond_src,
                                   curve->beat_piece_expr_src,
                                   &curve->beat_piece_count);
      if (prc < 0) { free(buf); return SBX_EINVAL; }
      if (prc > 0) {
        if (curve->beat_expr_src[0]) { free(buf); return curve_fail(curve, "%s:%d: cannot mix 'beat = ...' with piecewise beat<... lines", curve->src_file, lno); }
        have_beat = 1;
        continue;
      }
      if (curve->beat_piece_count > 0) { free(buf); return curve_fail(curve, "%s:%d: cannot mix piecewise beat<... lines with 'beat = ...'", curve->src_file, lno); }
      {
        int rc = curve_parse_expr_line(curve, s, lno, curve->src_file, "beat",
                                       curve->beat_expr_src, sizeof(curve->beat_expr_src));
        if (rc != SBX_OK) { free(buf); return rc; }
      }
      have_beat = 1;
      continue;
    }
    if (!strncmp(s, "carrier", 7) &&
        (isspace((unsigned char)s[7]) || s[7] == '=' || s[7] == '<' || s[7] == '>')) {
      prc = curve_parse_piece_line(curve, s, lno, curve->src_file, "carrier", 7,
                                   curve->carrier_piece_cond_src,
                                   curve->carrier_piece_expr_src,
                                   &curve->carrier_piece_count);
      if (prc < 0) { free(buf); return SBX_EINVAL; }
      if (prc > 0) {
        if (curve->has_carrier_expr) { free(buf); return curve_fail(curve, "%s:%d: cannot mix 'carrier = ...' with piecewise carrier<... lines", curve->src_file, lno); }
        continue;
      }
      if (curve->carrier_piece_count > 0) { free(buf); return curve_fail(curve, "%s:%d: cannot mix piecewise carrier<... lines with 'carrier = ...'", curve->src_file, lno); }
      {
        int rc = curve_parse_expr_line(curve, s, lno, curve->src_file, "carrier",
                                       curve->carrier_expr_src, sizeof(curve->carrier_expr_src));
        if (rc != SBX_OK) { free(buf); return rc; }
      }
      curve->has_carrier_expr = 1;
      continue;
    }
    if (!strncmp(s, "amp", 3) &&
        (isspace((unsigned char)s[3]) || s[3] == '=' || s[3] == '<' || s[3] == '>')) {
      prc = curve_parse_piece_line(curve, s, lno, curve->src_file, "amp", 3,
                                   curve->amp_piece_cond_src,
                                   curve->amp_piece_expr_src,
                                   &curve->amp_piece_count);
      if (prc < 0) { free(buf); return SBX_EINVAL; }
      if (prc > 0) {
        if (curve->has_amp_expr) { free(buf); return curve_fail(curve, "%s:%d: cannot mix 'amp = ...' with piecewise amp<... lines", curve->src_file, lno); }
        continue;
      }
      if (curve->amp_piece_count > 0) { free(buf); return curve_fail(curve, "%s:%d: cannot mix piecewise amp<... lines with 'amp = ...'", curve->src_file, lno); }
      {
        int rc = curve_parse_expr_line(curve, s, lno, curve->src_file, "amp",
                                       curve->amp_expr_src, sizeof(curve->amp_expr_src));
        if (rc != SBX_OK) { free(buf); return rc; }
      }
      curve->has_amp_expr = 1;
      continue;
    }
    if (!strncmp(s, "mixamp", 6) &&
        (isspace((unsigned char)s[6]) || s[6] == '=' || s[6] == '<' || s[6] == '>')) {
      prc = curve_parse_piece_line(curve, s, lno, curve->src_file, "mixamp", 6,
                                   curve->mixamp_piece_cond_src,
                                   curve->mixamp_piece_expr_src,
                                   &curve->mixamp_piece_count);
      if (prc < 0) { free(buf); return SBX_EINVAL; }
      if (prc > 0) {
        if (curve->has_mixamp_expr) { free(buf); return curve_fail(curve, "%s:%d: cannot mix 'mixamp = ...' with piecewise mixamp<... lines", curve->src_file, lno); }
        continue;
      }
      if (curve->mixamp_piece_count > 0) { free(buf); return curve_fail(curve, "%s:%d: cannot mix piecewise mixamp<... lines with 'mixamp = ...'", curve->src_file, lno); }
      {
        int rc = curve_parse_expr_line(curve, s, lno, curve->src_file, "mixamp",
                                       curve->mixamp_expr_src, sizeof(curve->mixamp_expr_src));
        if (rc != SBX_OK) { free(buf); return rc; }
      }
      curve->has_mixamp_expr = 1;
      continue;
    }
    {
      int mi;
      for (mi = 0; mi < SBX_CURVE_MIXFX_PARAM_COUNT; mi++) {
        const char *kind = sbx_curve_mixfx_target_names[mi];
        int kind_len = (int)strlen(kind);
        if (!strncmp(s, kind, (size_t)kind_len) &&
            (isspace((unsigned char)s[kind_len]) || s[kind_len] == '=' ||
             s[kind_len] == '<' || s[kind_len] == '>')) {
          int rc = curve_parse_named_target_line(curve, s, lno, curve->src_file,
                                                 kind, kind_len,
                                                 &curve->mixfx_targets[mi]);
          if (rc != SBX_OK) { free(buf); return rc; }
          goto parsed_line;
        }
      }
    }

    free(buf);
    return curve_fail(curve, "%s:%d: unknown line in .sbgf (expected 'param', 'solve', 'beat', 'carrier', 'amp', 'mixamp', or mix-effect targets like 'mixspin_width' / 'mixpulse_hz' / 'mixbeat_amp' / 'mixam_hz'): %s",
                      curve->src_file, lno, s);
parsed_line:
    continue;
  }

  free(buf);
  if (!have_beat)
    return curve_fail(curve, "%s: missing required beat definition in .sbgf (use 'beat = ...' or piecewise beat<... lines)", curve->src_file);

  curve->loaded = 1;
  curve->last_error[0] = 0;
  return SBX_OK;
}

int
sbx_curve_load_file(SbxCurveProgram *curve, const char *path) {
  FILE *fp;
  long sz;
  char *buf;
  int rc;

  if (!curve || !path) return SBX_EINVAL;
  if (!curve_has_sbgf_ext(path))
    return curve_fail(curve, "Curve file must use .sbgf extension: %s", path);

  fp = fopen(path, "rb");
  if (!fp)
    return curve_fail(curve, "Cannot open curve file: %s (%s)", path, strerror(errno));
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return curve_fail(curve, "Cannot seek curve file: %s", path);
  }
  sz = ftell(fp);
  if (sz < 0) {
    fclose(fp);
    return curve_fail(curve, "Cannot size curve file: %s", path);
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return curve_fail(curve, "Cannot rewind curve file: %s", path);
  }
  buf = (char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(fp);
    return curve_fail(curve, "Out of memory reading curve file: %s", path);
  }
  if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
    free(buf);
    fclose(fp);
    return curve_fail(curve, "Cannot read curve file: %s", path);
  }
  fclose(fp);
  buf[sz] = 0;
  rc = sbx_curve_load_text(curve, buf, path);
  free(buf);
  return rc;
}

int
sbx_curve_prepare(SbxCurveProgram *curve, const SbxCurveEvalConfig *cfg_in) {
  te_variable vars[SBX_CURVE_MAX_PARAMS + 40];
  int vcnt = 0;
  int i;
  int err = 0;
  SbxCurveEvalConfig cfg;
  int rc;

  if (!curve) return SBX_EINVAL;
  if (!curve->loaded)
    return curve_fail(curve, "No .sbgf curve loaded");

  if (cfg_in) cfg = *cfg_in;
  else sbx_default_curve_eval_config(&cfg);

  if (!(cfg.carrier_span_sec > 0.0) || !(cfg.beat_span_sec > 0.0))
    return curve_fail(curve, "Curve spans must be > 0");

  sbx_curve_clear_compiled(curve);
  curve->cfg = cfg;
  curve->ev_D = cfg.beat_span_sec / 60.0;
  curve->ev_H = cfg.hold_min;
  curve->ev_T = cfg.total_min;
  curve->ev_U = cfg.wake_min;
  curve->ev_b0 = cfg.beat_start_hz;
  curve->ev_b1 = cfg.beat_target_hz;
  curve->ev_c0 = cfg.carrier_start_hz;
  curve->ev_c1 = cfg.carrier_end_hz;
  curve->ev_a0 = cfg.beat_amp0_pct;
  curve->ev_m0 = cfg.mix_amp0_pct;

#define ADD_VAR(name_, ptr_) do { \
  vars[vcnt].name = (name_); \
  vars[vcnt].address = (ptr_); \
  vars[vcnt].type = TE_VARIABLE; \
  vars[vcnt].context = 0; \
  vcnt++; \
} while (0)
#define ADD_FN1(name_, fn_) do { \
  vars[vcnt].name = (name_); \
  vars[vcnt].address = (const void*)(fn_); \
  vars[vcnt].type = TE_FUNCTION1 | TE_FLAG_PURE; \
  vars[vcnt].context = 0; \
  vcnt++; \
} while (0)
#define ADD_FN2(name_, fn_) do { \
  vars[vcnt].name = (name_); \
  vars[vcnt].address = (const void*)(fn_); \
  vars[vcnt].type = TE_FUNCTION2 | TE_FLAG_PURE; \
  vars[vcnt].context = 0; \
  vcnt++; \
} while (0)
#define ADD_FN3(name_, fn_) do { \
  vars[vcnt].name = (name_); \
  vars[vcnt].address = (const void*)(fn_); \
  vars[vcnt].type = TE_FUNCTION3 | TE_FLAG_PURE; \
  vars[vcnt].context = 0; \
  vcnt++; \
} while (0)
#define ADD_FN5(name_, fn_) do { \
  vars[vcnt].name = (name_); \
  vars[vcnt].address = (const void*)(fn_); \
  vars[vcnt].type = TE_FUNCTION5 | TE_FLAG_PURE; \
  vars[vcnt].context = 0; \
  vcnt++; \
} while (0)

  ADD_VAR("t", &curve->ev_t);
  ADD_VAR("m", &curve->ev_m);
  ADD_VAR("D", &curve->ev_D);
  ADD_VAR("H", &curve->ev_H);
  ADD_VAR("T", &curve->ev_T);
  ADD_VAR("U", &curve->ev_U);
  ADD_VAR("b0", &curve->ev_b0);
  ADD_VAR("b1", &curve->ev_b1);
  ADD_VAR("c0", &curve->ev_c0);
  ADD_VAR("c1", &curve->ev_c1);
  ADD_VAR("a0", &curve->ev_a0);
  ADD_VAR("m0", &curve->ev_m0);

  ADD_FN3("ifelse", curve_fn_ifelse);
  ADD_FN1("step", curve_fn_step);
  ADD_FN3("clamp", curve_fn_clamp);
  ADD_FN3("lerp", curve_fn_lerp);
  ADD_FN3("ramp", curve_fn_ramp);
  ADD_FN3("smoothstep", curve_fn_smoothstep);
  ADD_FN3("smootherstep", curve_fn_smootherstep);
  ADD_FN3("between", curve_fn_between);
  ADD_FN3("pulse", curve_fn_pulse);
  ADD_FN2("lt", curve_fn_lt);
  ADD_FN2("le", curve_fn_le);
  ADD_FN2("gt", curve_fn_gt);
  ADD_FN2("ge", curve_fn_ge);
  ADD_FN2("eq", curve_fn_eq);
  ADD_FN2("ne", curve_fn_ne);
  ADD_FN5("seg", curve_fn_seg);
  ADD_FN2("min2", curve_fn_min2);
  ADD_FN2("max2", curve_fn_max2);

  for (i = 0; i < curve->param_count; i++)
    ADD_VAR(curve->param_names[i], &curve->param_values[i]);

  rc = curve_apply_solve(curve, vars, vcnt);
  if (rc != SBX_OK) goto prepare_fail;

  if (curve->beat_piece_count > 0) {
    for (i = 0; i < curve->beat_piece_count; i++) {
      curve->beat_piece_cond[i] = te_compile(curve->beat_piece_cond_src[i], vars, vcnt, &err);
      if (!curve->beat_piece_cond[i]) goto beat_piece_cond_fail;
      err = 0;
      curve->beat_piece_expr[i] = te_compile(curve->beat_piece_expr_src[i], vars, vcnt, &err);
      if (!curve->beat_piece_expr[i]) goto beat_piece_expr_fail;
      err = 0;
    }
  } else {
    curve->beat_expr = te_compile(curve->beat_expr_src, vars, vcnt, &err);
    if (!curve->beat_expr) goto beat_expr_fail;
  }

  if (curve->carrier_piece_count > 0) {
    for (i = 0; i < curve->carrier_piece_count; i++) {
      curve->carrier_piece_cond[i] = te_compile(curve->carrier_piece_cond_src[i], vars, vcnt, &err);
      if (!curve->carrier_piece_cond[i]) goto carrier_piece_cond_fail;
      err = 0;
      curve->carrier_piece_expr[i] = te_compile(curve->carrier_piece_expr_src[i], vars, vcnt, &err);
      if (!curve->carrier_piece_expr[i]) goto carrier_piece_expr_fail;
      err = 0;
    }
  } else if (curve->has_carrier_expr) {
    curve->carrier_expr = te_compile(curve->carrier_expr_src, vars, vcnt, &err);
    if (!curve->carrier_expr) goto carrier_expr_fail;
  }

  if (curve->amp_piece_count > 0) {
    for (i = 0; i < curve->amp_piece_count; i++) {
      curve->amp_piece_cond[i] = te_compile(curve->amp_piece_cond_src[i], vars, vcnt, &err);
      if (!curve->amp_piece_cond[i]) goto amp_piece_cond_fail;
      err = 0;
      curve->amp_piece_expr[i] = te_compile(curve->amp_piece_expr_src[i], vars, vcnt, &err);
      if (!curve->amp_piece_expr[i]) goto amp_piece_expr_fail;
      err = 0;
    }
  } else if (curve->has_amp_expr) {
    curve->amp_expr = te_compile(curve->amp_expr_src, vars, vcnt, &err);
    if (!curve->amp_expr) goto amp_expr_fail;
  }

  if (curve->mixamp_piece_count > 0) {
    for (i = 0; i < curve->mixamp_piece_count; i++) {
      curve->mixamp_piece_cond[i] = te_compile(curve->mixamp_piece_cond_src[i], vars, vcnt, &err);
      if (!curve->mixamp_piece_cond[i]) goto mixamp_piece_cond_fail;
      err = 0;
      curve->mixamp_piece_expr[i] = te_compile(curve->mixamp_piece_expr_src[i], vars, vcnt, &err);
      if (!curve->mixamp_piece_expr[i]) goto mixamp_piece_expr_fail;
      err = 0;
    }
  } else if (curve->has_mixamp_expr) {
    curve->mixamp_expr = te_compile(curve->mixamp_expr_src, vars, vcnt, &err);
    if (!curve->mixamp_expr) goto mixamp_expr_fail;
  }

  for (i = 0; i < SBX_CURVE_MIXFX_PARAM_COUNT; i++) {
    rc = curve_compile_expr_target(curve, &curve->mixfx_targets[i],
                                   sbx_curve_mixfx_target_names[i],
                                   vars, vcnt);
    if (rc != SBX_OK) goto prepare_fail;
  }

#undef ADD_VAR
#undef ADD_FN1
#undef ADD_FN2
#undef ADD_FN3
#undef ADD_FN5

  curve->prepared = 1;
  curve->last_error[0] = 0;
  return SBX_OK;

mixamp_expr_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in mixamp expression near column %d", curve->src_file, err);
  goto prepare_fail;
mixamp_piece_expr_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in mixamp piece expression #%d near column %d", curve->src_file, i + 1, err);
  goto prepare_fail;
mixamp_piece_cond_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in mixamp piece condition #%d near column %d", curve->src_file, i + 1, err);
  goto prepare_fail;
amp_expr_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in amp expression near column %d", curve->src_file, err);
  goto prepare_fail;
amp_piece_expr_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in amp piece expression #%d near column %d", curve->src_file, i + 1, err);
  goto prepare_fail;
amp_piece_cond_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in amp piece condition #%d near column %d", curve->src_file, i + 1, err);
  goto prepare_fail;
carrier_expr_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in carrier expression near column %d", curve->src_file, err);
  goto prepare_fail;
carrier_piece_expr_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in carrier piece expression #%d near column %d", curve->src_file, i + 1, err);
  goto prepare_fail;
carrier_piece_cond_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in carrier piece condition #%d near column %d", curve->src_file, i + 1, err);
  goto prepare_fail;
beat_expr_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in beat expression near column %d", curve->src_file, err);
  goto prepare_fail;
beat_piece_expr_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in beat piece expression #%d near column %d", curve->src_file, i + 1, err);
  goto prepare_fail;
beat_piece_cond_fail:
  sbx_curve_clear_compiled(curve);
  curve_set_error(curve, "%s: error in beat piece condition #%d near column %d", curve->src_file, i + 1, err);
prepare_fail:
#undef ADD_VAR
#undef ADD_FN1
#undef ADD_FN2
#undef ADD_FN3
#undef ADD_FN5
  return SBX_EINVAL;
}

int
sbx_curve_eval(SbxCurveProgram *curve, double t_sec, SbxCurveEvalPoint *out_point) {
  int i, matched;
  double condv;
  double beat, carrier, amp_pct, mixamp_pct;
  double t;

  if (!curve || !out_point) return SBX_EINVAL;
  if (!curve->prepared)
    return curve_fail(curve, "Curve is not prepared for evaluation");

  t = t_sec;
  curve_set_eval_time(curve, t);
  t = curve->ev_t;

  if (curve->beat_piece_count > 0) {
    matched = 0;
    for (i = 0; i < curve->beat_piece_count; i++) {
      if (!curve->beat_piece_cond[i] || !curve->beat_piece_expr[i])
        return curve_fail(curve, "Curve beat piece state is incomplete");
      condv = te_eval(curve->beat_piece_cond[i]);
      if (!isfinite(condv))
        return curve_fail(curve, "Curve beat piece condition evaluated non-finite");
      if (condv != 0.0) {
        beat = te_eval(curve->beat_piece_expr[i]);
        matched = 1;
        break;
      }
    }
    if (!matched)
      beat = te_eval(curve->beat_piece_expr[curve->beat_piece_count - 1]);
  } else {
    beat = te_eval(curve->beat_expr);
  }
  if (!isfinite(beat))
    return curve_fail(curve, "Curve beat evaluated non-finite");

  if (curve->carrier_piece_count > 0) {
    matched = 0;
    for (i = 0; i < curve->carrier_piece_count; i++) {
      if (!curve->carrier_piece_cond[i] || !curve->carrier_piece_expr[i])
        return curve_fail(curve, "Curve carrier piece state is incomplete");
      condv = te_eval(curve->carrier_piece_cond[i]);
      if (!isfinite(condv))
        return curve_fail(curve, "Curve carrier piece condition evaluated non-finite");
      if (condv != 0.0) {
        carrier = te_eval(curve->carrier_piece_expr[i]);
        matched = 1;
        break;
      }
    }
    if (!matched)
      carrier = te_eval(curve->carrier_piece_expr[curve->carrier_piece_count - 1]);
    if (!isfinite(carrier))
      carrier = curve->cfg.carrier_start_hz +
                (curve->cfg.carrier_end_hz - curve->cfg.carrier_start_hz) *
                (curve->cfg.carrier_span_sec > 0.0 ? (t / curve->cfg.carrier_span_sec) : 0.0);
  } else if (curve->has_carrier_expr && curve->carrier_expr) {
    carrier = te_eval(curve->carrier_expr);
    if (!isfinite(carrier))
      carrier = curve->cfg.carrier_start_hz +
                (curve->cfg.carrier_end_hz - curve->cfg.carrier_start_hz) *
                (curve->cfg.carrier_span_sec > 0.0 ? (t / curve->cfg.carrier_span_sec) : 0.0);
  } else {
    carrier = curve->cfg.carrier_start_hz +
              (curve->cfg.carrier_end_hz - curve->cfg.carrier_start_hz) *
              (curve->cfg.carrier_span_sec > 0.0 ? (t / curve->cfg.carrier_span_sec) : 0.0);
  }

  amp_pct = curve->cfg.beat_amp0_pct;
  if (curve->amp_piece_count > 0) {
    matched = 0;
    for (i = 0; i < curve->amp_piece_count; i++) {
      if (!curve->amp_piece_cond[i] || !curve->amp_piece_expr[i])
        return curve_fail(curve, "Curve amp piece state is incomplete");
      condv = te_eval(curve->amp_piece_cond[i]);
      if (!isfinite(condv))
        return curve_fail(curve, "Curve amp piece condition evaluated non-finite");
      if (condv != 0.0) {
        amp_pct = te_eval(curve->amp_piece_expr[i]);
        matched = 1;
        break;
      }
    }
    if (!matched)
      amp_pct = te_eval(curve->amp_piece_expr[curve->amp_piece_count - 1]);
  } else if (curve->has_amp_expr && curve->amp_expr) {
    amp_pct = te_eval(curve->amp_expr);
  }
  if (!isfinite(amp_pct)) amp_pct = curve->cfg.beat_amp0_pct;
  if (amp_pct < 0.0) amp_pct = 0.0;

  mixamp_pct = curve->cfg.mix_amp0_pct;
  if (curve->mixamp_piece_count > 0) {
    matched = 0;
    for (i = 0; i < curve->mixamp_piece_count; i++) {
      if (!curve->mixamp_piece_cond[i] || !curve->mixamp_piece_expr[i])
        return curve_fail(curve, "Curve mixamp piece state is incomplete");
      condv = te_eval(curve->mixamp_piece_cond[i]);
      if (!isfinite(condv))
        return curve_fail(curve, "Curve mixamp piece condition evaluated non-finite");
      if (condv != 0.0) {
        mixamp_pct = te_eval(curve->mixamp_piece_expr[i]);
        matched = 1;
        break;
      }
    }
    if (!matched)
      mixamp_pct = te_eval(curve->mixamp_piece_expr[curve->mixamp_piece_count - 1]);
  } else if (curve->has_mixamp_expr && curve->mixamp_expr) {
    mixamp_pct = te_eval(curve->mixamp_expr);
  }
  if (!isfinite(mixamp_pct)) mixamp_pct = curve->cfg.mix_amp0_pct;
  if (mixamp_pct < 0.0) mixamp_pct = 0.0;

  out_point->beat_hz = beat;
  out_point->carrier_hz = carrier;
  out_point->beat_amp_pct = amp_pct;
  out_point->mix_amp_pct = mixamp_pct;
  curve->last_error[0] = 0;
  return SBX_OK;
}

int
sbx_curve_sample_program_beat(SbxCurveProgram *curve,
                              double t0_sec,
                              double t1_sec,
                              size_t sample_count,
                              double *out_t_sec,
                              double *out_hz) {
  size_t i;

  if (!curve || !out_hz || sample_count == 0) return SBX_EINVAL;
  if (!curve->prepared)
    return curve_fail(curve, "Curve program is not prepared");
  if (!isfinite(t0_sec) || !isfinite(t1_sec))
    return curve_fail(curve, "Curve sampling times must be finite");

  for (i = 0; i < sample_count; i++) {
    double u = (sample_count <= 1) ? 0.0 : (double)i / (double)(sample_count - 1);
    double ts = sbx_lerp(t0_sec, t1_sec, u);
    SbxCurveEvalPoint pt;
    int rc = sbx_curve_eval(curve, ts, &pt);
    if (rc != SBX_OK)
      return rc;
    if (out_t_sec) out_t_sec[i] = ts;
    out_hz[i] = pt.beat_hz;
  }

  curve->last_error[0] = 0;
  return SBX_OK;
}
