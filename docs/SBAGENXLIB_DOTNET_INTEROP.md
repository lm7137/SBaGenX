SBaGenX Library .NET Interop Notes
==================================

This guide targets C# hosts (for example Avalonia front-ends) calling
`sbagenxlib` through P/Invoke.

Key Rules
---------

- Calling convention: `Cdecl`.
- String encoding: UTF-8 (`char *` in C API).
- Struct layout: `Sequential`, default packing (do not force unusual packing).
- Pointer ownership: C allocates engine/context; C# calls destroy methods.

Suggested P/Invoke Definitions
------------------------------

```csharp
using System;
using System.Runtime.InteropServices;

internal static class SbxNative
{
    private const string DllName = "sbagenxlib"; // platform-specific loader name

    public const int SBX_OK = 0;

    [StructLayout(LayoutKind.Sequential)]
    public struct SbxEngineConfig
    {
        public double sample_rate;
        public int channels;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SbxToneSpec
    {
        public int mode;
        public double carrier_hz;
        public double beat_hz;
        public double amplitude;
        public int waveform;
        public double duty_cycle;
    }

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr sbx_version();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int sbx_api_version();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void sbx_default_engine_config(out SbxEngineConfig cfg);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr sbx_context_create(ref SbxEngineConfig cfg);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void sbx_context_destroy(IntPtr ctx);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int sbx_context_load_tone_spec(IntPtr ctx, byte[] utf8ToneSpec);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int sbx_context_render_f32(IntPtr ctx, float[] outInterleavedStereo, UIntPtr frames);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int sbx_context_sample_tones(
        IntPtr ctx,
        double t0Sec,
        double t1Sec,
        UIntPtr sampleCount,
        [Out] double[] outTSec,
        [Out] SbxToneSpec[] outTones);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr sbx_context_last_error(IntPtr ctx);
}
```

UTF-8 Helper
------------

```csharp
static byte[] Utf8Z(string s) => System.Text.Encoding.UTF8.GetBytes(s + "\0");
```

Example Flow
------------

1. Create config with `sbx_default_engine_config`.
2. Create context with `sbx_context_create`.
3. Load tone/sequence (`sbx_context_load_tone_spec`, `sbx_context_load_*`).
4. Render float frames with `sbx_context_render_f32`.
5. For GUI plotting, sample tone curves with `sbx_context_sample_tones`.
6. Destroy context with `sbx_context_destroy`.

Error String Marshalling
------------------------

`sbx_context_last_error` and `sbx_engine_last_error` return object-owned C
strings. In C#, convert with:

```csharp
static string PtrToUtf8(IntPtr p) =>
    p == IntPtr.Zero ? string.Empty : Marshal.PtrToStringUTF8(p) ?? string.Empty;
```

Threading Guidance
------------------

- Treat each context/engine as single-thread-owned.
- Do not call render and mutating setters concurrently on the same handle.
- Use one context per playback/render worker.

Distribution Notes
------------------

- Ship native binary per platform (`.dll`, `.so`, `.dylib`) with your app.
- Keep `sbx_api_version()` checks at startup to fail fast on mismatched binaries.

See Also
--------

- `docs/SBAGENXLIB_API.md`
- `docs/SBAGENXLIB_QUICKSTART.md`
