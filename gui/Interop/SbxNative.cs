using System;
using System.Runtime.InteropServices;
using System.Text;

namespace gui.Interop;

internal static class SbxNative
{
    private const string DllName = "sbagenxlib";

    internal const int SBX_OK = 0;

    internal const int SBX_TONE_NONE = 0;
    internal const int SBX_TONE_BINAURAL = 1;
    internal const int SBX_TONE_MONAURAL = 2;
    internal const int SBX_TONE_ISOCHRONIC = 3;

    internal const int SBX_WAVE_SINE = 0;
    internal const int SBX_INTERP_LINEAR = 0;
    internal const int SBX_INTERP_STEP = 1;

    [StructLayout(LayoutKind.Sequential)]
    internal struct SbxEngineConfig
    {
        public double sample_rate;
        public int channels;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct SbxToneSpec
    {
        public int mode;
        public double carrier_hz;
        public double beat_hz;
        public double amplitude;
        public int waveform;
        public double duty_cycle;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct SbxProgramKeyframe
    {
        public double time_sec;
        public SbxToneSpec tone;
        public int interp;
    }

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sbx_api_version();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void sbx_default_engine_config(out SbxEngineConfig cfg);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void sbx_default_tone_spec(out SbxToneSpec tone);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr sbx_context_create(ref SbxEngineConfig cfg);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void sbx_context_destroy(IntPtr ctx);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sbx_context_load_keyframes(
        IntPtr ctx,
        [In] SbxProgramKeyframe[] frames,
        UIntPtr frameCount,
        int loop);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sbx_context_set_aux_tones(
        IntPtr ctx,
        [In] SbxToneSpec[] tones,
        UIntPtr toneCount);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sbx_context_load_sbg_timing_file(IntPtr ctx, byte[] utf8Path, int loop);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sbx_context_load_sequence_file(IntPtr ctx, byte[] utf8Path, int loop);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sbx_context_render_f32(IntPtr ctx, [Out] float[] outInterleavedStereo, UIntPtr frames);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sbx_context_sample_tones(
        IntPtr ctx,
        double t0Sec,
        double t1Sec,
        UIntPtr sampleCount,
        [Out] double[] outTSec,
        [Out] SbxToneSpec[] outTones);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int sbx_parse_tone_spec_ex(byte[] utf8Spec, int defaultWaveform, out SbxToneSpec outTone);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern double sbx_context_duration_sec(IntPtr ctx);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr sbx_context_last_error(IntPtr ctx);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr sbx_status_string(int status);

    internal static byte[] Utf8Z(string s)
    {
        return Encoding.UTF8.GetBytes((s ?? string.Empty) + "\0");
    }

    internal static string PtrToUtf8(IntPtr ptr)
    {
        return ptr == IntPtr.Zero ? string.Empty : Marshal.PtrToStringUTF8(ptr) ?? string.Empty;
    }
}
