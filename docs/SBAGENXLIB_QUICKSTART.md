SBaGenX Library Quick Start
===========================

This is the fastest path to successfully use `sbagenxlib` in a host app.

Prerequisites
-------------

- `sbagenxlib.h`
- static library built by project scripts:
  - Linux: `dist/libsbagenx-linux64.a`
  - Windows: `dist/libsbagenx-win64.a` (when available from Windows build)
- math library (`-lm`) when linking on POSIX.

Compile/Link (Linux)
--------------------

```bash
gcc -O2 -I. your_app.c dist/libsbagenx-linux64.a -lm -o your_app
```

Minimal Render Example
----------------------

```c
#include <stdio.h>
#include <stdlib.h>
#include "sbagenxlib.h"

int main(void) {
  SbxEngineConfig cfg;
  SbxContext *ctx;
  const size_t frames = 44100; /* 1 second at 44.1k */
  float *buf = (float *)calloc(frames * 2, sizeof(float));
  int rc;

  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 44100.0;
  cfg.channels = 2;

  ctx = sbx_context_create(&cfg);
  if (!ctx) return 1;

  rc = sbx_context_load_tone_spec(ctx, "200+6/40");
  if (rc != SBX_OK) {
    fprintf(stderr, "load failed: %s\n", sbx_context_last_error(ctx));
    sbx_context_destroy(ctx);
    free(buf);
    return 2;
  }

  rc = sbx_context_render_f32(ctx, buf, frames);
  if (rc != SBX_OK) {
    fprintf(stderr, "render failed: %s\n", sbx_context_last_error(ctx));
    sbx_context_destroy(ctx);
    free(buf);
    return 3;
  }

  printf("Rendered %zu stereo frames\n", frames);
  sbx_context_destroy(ctx);
  free(buf);
  return 0;
}
```

Minimal Keyframe Program Example
--------------------------------

```c
SbxProgramKeyframe kf[2];
sbx_default_tone_spec(&kf[0].tone);
sbx_default_tone_spec(&kf[1].tone);
kf[0].time_sec = 0.0;
kf[0].tone.mode = SBX_TONE_BINAURAL;
kf[0].tone.carrier_hz = 200.0;
kf[0].tone.beat_hz = 10.0;
kf[0].tone.amplitude = 0.4;
kf[0].interp = SBX_INTERP_LINEAR;
kf[1] = kf[0];
kf[1].time_sec = 1800.0; /* 30 min */
kf[1].tone.beat_hz = 2.5;
kf[1].interp = SBX_INTERP_LINEAR;

sbx_context_load_keyframes(ctx, kf, 2, 0);
```

Minimal Sequence File Load
--------------------------

```c
/* line format: <time> <tone-spec> [linear|step] */
int rc = sbx_context_load_sequence_file(ctx, "examples/sbagenxlib/minimal-keyframes.sbxseq", 0);
```

Minimal SBG Timing Subset Load
------------------------------

```c
int rc = sbx_context_load_sbg_timing_file(ctx, "examples/sbagenxlib/minimal-sbg-nested-block.sbg", 0);
```

Runtime Extras Setup (One Call)
-------------------------------

```c
SbxMixAmpKeyframe mk[2] = {
  { 0.0, 100.0, SBX_INTERP_LINEAR },
  { 3600.0, 70.0, SBX_INTERP_LINEAR }
};
SbxMixFxSpec fx[1] = {
  { SBX_MIXFX_BEAT, SBX_WAVE_SINE, 0.0, 6.0, 0.45 }
};

sbx_context_configure_runtime(ctx,
                              mk, 2, 100.0,
                              fx, 1,
                              NULL, 0);
```

Notes
-----

- The library renders audio buffers; host app owns file encoding/device output.
- Plotting is currently a CLI-level feature, not a `sbagenxlib` API surface.
- For API details and ownership/threading rules, read `docs/SBAGENXLIB_API.md`.
