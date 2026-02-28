SBaGenX Library Quick Start
===========================

This is the fastest path to successfully use `sbagenxlib` in a host app.

Prerequisites
-------------

- `sbagenxlib.h`
- library built by project scripts:
  - Linux shared:
    - `dist/libsbagenx.so`
    - `dist/pkgconfig/sbagenxlib.pc`
  - Linux static:
    - `dist/libsbagenx-linux64.a`
  - Windows:
    - `dist/sbagenxlib-win64.dll`
    - `dist/libsbagenx-win64.dll.a`
  - macOS:
    - `dist/libsbagenx.dylib`
- math library (`-lm`) when linking on POSIX.

Compile/Link (Linux)
--------------------

```bash
gcc -O2 -I. your_app.c dist/libsbagenx-linux64.a -lm -o your_app
```

Compile/Link (Linux shared, pkg-config)
---------------------------------------

```bash
gcc -O2 your_app.c $(PKG_CONFIG_PATH=dist/pkgconfig pkg-config --cflags --libs sbagenxlib-uninstalled) -o your_app
```

Use `sbagenxlib-uninstalled` when linking directly against the `dist/` tree.
Use `sbagenxlib` after the library has been installed system-wide by a package
or into a prefix on your machine.

Compile/Link (Linux shared, direct dist paths)
----------------------------------------------

```bash
gcc -O2 -Idist/include your_app.c -Ldist -lsbagenx -lm -o your_app
LD_LIBRARY_PATH=dist ./your_app
```

Installable Package (Debian/Ubuntu)
-----------------------------------

After running `linux-build-sbagenx.sh` and `linux-create-deb.sh`, the generated
package installs:

- `sbagenx` into `/usr/bin`
- `sbagenxlib.h` and `sbagenlib.h` into `/usr/include`
- `libsbagenx.so*` and `libsbagenx.a` into `/usr/lib/<multiarch>`
- `sbagenxlib.pc` into `/usr/lib/<multiarch>/pkgconfig`

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

Minimal Plot-Sampling Example
-----------------------------

```c
SbxToneSpec curve[256];
double tsec[256];
int rc = sbx_context_sample_tones(ctx, 0.0, 1800.0, 256, tsec, curve);
```

Use this to drive GUI plots directly from library-evaluated tone values.

If a frontend needs transport/scrubbing, set the context clock explicitly:

```c
sbx_context_set_time_sec(ctx, 900.0); /* jump to 15:00 */
```

This resets internal oscillator/effect phase/state and restarts playback from
that timeline position.

Additional plot-data helpers:

```c
double beat_hz[256];
double mix_pct[256];
double mix_env[256], mix_gain[256];
double iso_env[256], iso_wave[256];
SbxToneSpec iso_tone;
SbxMixFxSpec fx;
SbxMixFxSpec fx_slots[8];
SbxIsoEnvelopeSpec iso;
size_t fx_count = 0;

sbx_context_sample_program_beat(ctx, 0.0, 1800.0, 256, tsec, beat_hz);
sbx_context_sample_mix_amp(ctx, 0.0, 1800.0, 256, tsec, mix_pct);

sbx_parse_mix_fx_spec("mixam:1:m=cos:s=0:f=0.45", SBX_WAVE_SINE, &fx);
sbx_sample_mixam_cycle(&fx, 1.0, 256, tsec, mix_env, mix_gain);

sbx_context_sample_mix_effects(ctx, 900.0, NULL, 0, &fx_count);
sbx_context_sample_mix_effects(ctx, 900.0, fx_slots, 8, &fx_count);

sbx_parse_tone_spec("200@1/100", &iso_tone);
sbx_default_iso_envelope_spec(&iso);
sbx_sample_isochronic_cycle(&iso_tone, &iso, 256, tsec, iso_env, iso_wave);
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
- Image rendering is frontend-owned; use `sbx_context_sample_tones()` for
  plotting data extraction.
- For API details and ownership/threading rules, read `docs/SBAGENXLIB_API.md`.
