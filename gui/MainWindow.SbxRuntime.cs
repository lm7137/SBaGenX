using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using gui.Interop;

namespace gui;

public partial class MainWindow
{
    private static readonly double[] DropBeatTargets =
    [
        4.4, 3.7, 3.1, 2.5, 2.0, 1.5, 1.2, 0.9, 0.7, 0.5, 0.4, 0.3
    ];

    private sealed class DropSigSpec
    {
        public double Level;
        public int DepthIndex;
        public bool HasStepMode;
        public bool Slide;
        public bool IsLong;
        public bool Wakeup;
        public bool IsIsochronic;
        public bool IsMonaural;
        public double Amplitude = 1.0;
        public double SigL = 0.125;
        public double SigH = 0.0;
    }

    private sealed class BuiltInKeyframePlan
    {
        public SbxNative.SbxProgramKeyframe[] Keyframes = [];
    }

    private async Task<bool> TryRunWithSbxLibAsync(RunAction action)
    {
        if (action == RunAction.Plot)
        {
            AppendLog("sbagenxlib direct mode: Plot currently falls back to CLI renderer.");
            return false;
        }

        if (!TryCheckSbxLibraryAvailable(out var unavailableReason))
        {
            AppendLog($"sbagenxlib direct mode unavailable: {unavailableReason}");
            return false;
        }

        var explicitMix = (MixFileTextBox.Text ?? string.Empty).Trim();
        if (!string.IsNullOrWhiteSpace(explicitMix))
        {
            AppendLog("sbagenxlib direct mode: mix input (-m) is not yet integrated; using CLI fallback.");
            return false;
        }

        if (!string.IsNullOrWhiteSpace((ExtraArgsTextBox.Text ?? string.Empty).Trim()))
        {
            AppendLog("sbagenxlib direct mode: Extra Args are CLI-specific; using CLI fallback.");
            return false;
        }

        if (action == RunAction.Export)
        {
            var format = GetComboSelection(OutputFormatComboBox, "wav");
            if (!string.Equals(format, "wav", StringComparison.OrdinalIgnoreCase))
            {
                AppendLog("sbagenxlib direct mode currently exports WAV only; using CLI fallback for encoded output.");
                return false;
            }
        }

        if (!TryCreateContextFromGui(out var ctx, out var durationSec, out var buildError))
        {
            AppendLog($"sbagenxlib direct mode skipped: {buildError}");
            return false;
        }

        try
        {
            SetStatus($"Running {action.ToString().ToLowerInvariant()} (sbagenxlib)...");
            SetRunningState(true);

            if (action == RunAction.Export)
            {
                var outputPath = (OutputFileTextBox.Text ?? string.Empty).Trim();
                if (string.IsNullOrWhiteSpace(outputPath))
                {
                    AppendLog("Export requires an output path.");
                    SetStatus("Invalid configuration");
                    return true;
                }

                outputPath = EnsureExtension(outputPath, "wav");
                OutputFileTextBox.Text = outputPath;

                var renderOk = await Task.Run(() => RenderContextToWav(ctx, durationSec, outputPath));
                if (!renderOk.Ok)
                {
                    AppendLog($"sbagenxlib export failed: {renderOk.Error}");
                    SetStatus("Export failed");
                    return true;
                }

                AppendLog($"sbagenxlib export complete: {outputPath}");
                SetStatus("export completed");
                return true;
            }

            if (action == RunAction.Play)
            {
                var tempPath = Path.Combine(
                    Path.GetTempPath(),
                    $"sbagenx_gui_play_{DateTime.UtcNow:yyyyMMdd_HHmmss_fff}.wav");

                var renderOk = await Task.Run(() => RenderContextToWav(ctx, durationSec, tempPath));
                if (!renderOk.Ok)
                {
                    AppendLog($"sbagenxlib play render failed: {renderOk.Error}");
                    SetStatus("Play failed");
                    return true;
                }

                try
                {
                    var psi = new ProcessStartInfo
                    {
                        FileName = tempPath,
                        UseShellExecute = true
                    };
                    Process.Start(psi);
                    AppendLog($"sbagenxlib play started via default system player: {tempPath}");
                    SetStatus("play started");
                }
                catch (Exception ex)
                {
                    AppendLog($"Could not launch default audio player ({ex.Message}). WAV saved to: {tempPath}");
                    SetStatus("play render complete");
                }

                return true;
            }

            return false;
        }
        finally
        {
            SbxNative.sbx_context_destroy(ctx);
            SetRunningState(false);
        }
    }

    private bool TryCheckSbxLibraryAvailable(out string reason)
    {
        reason = string.Empty;
        try
        {
            var api = SbxNative.sbx_api_version();
            if (api <= 0)
            {
                reason = "Invalid sbx_api_version().";
                return false;
            }
            return true;
        }
        catch (Exception ex)
        {
            reason = ex.Message;
            return false;
        }
    }

    private bool TryCreateContextFromGui(out IntPtr ctx, out double durationSec, out string error)
    {
        ctx = IntPtr.Zero;
        durationSec = 0.0;
        error = string.Empty;

        SbxNative.sbx_default_engine_config(out var cfg);
        ctx = SbxNative.sbx_context_create(ref cfg);
        if (ctx == IntPtr.Zero)
        {
            error = "sbx_context_create failed.";
            return false;
        }

        var mode = GetRunMode();
        if (mode == RunMode.ScriptFile)
        {
            var scriptPath = (ScriptFileTextBox.Text ?? string.Empty).Trim();
            if (string.IsNullOrWhiteSpace(scriptPath))
            {
                error = "Script mode requires a .sbg path.";
                SbxNative.sbx_context_destroy(ctx);
                ctx = IntPtr.Zero;
                return false;
            }

            var rc = SbxNative.sbx_context_load_sbg_timing_file(ctx, SbxNative.Utf8Z(scriptPath), 0);
            if (rc != SbxNative.SBX_OK)
            {
                rc = SbxNative.sbx_context_load_sequence_file(ctx, SbxNative.Utf8Z(scriptPath), 0);
            }

            if (rc != SbxNative.SBX_OK)
            {
                error = LastContextError(ctx, rc);
                SbxNative.sbx_context_destroy(ctx);
                ctx = IntPtr.Zero;
                return false;
            }

            durationSec = SbxNative.sbx_context_duration_sec(ctx);
            if (durationSec <= 0.0)
            {
                durationSec = 120.0;
                AppendLog("Script duration is open-ended; using 120s for direct render/play.");
            }
            return true;
        }

        var program = GetComboSelection(ProgramComboBox, "drop");
        if (string.Equals(program, "curve", StringComparison.OrdinalIgnoreCase))
        {
            error = "Program 'curve' is not yet available in direct sbagenxlib mode.";
            SbxNative.sbx_context_destroy(ctx);
            ctx = IntPtr.Zero;
            return false;
        }

        if (!TryBuildBuiltInKeyframes(program, out var plan, out error))
        {
            SbxNative.sbx_context_destroy(ctx);
            ctx = IntPtr.Zero;
            return false;
        }

        var rcLoad = SbxNative.sbx_context_load_keyframes(ctx, plan.Keyframes, (UIntPtr)plan.Keyframes.Length, 0);
        if (rcLoad != SbxNative.SBX_OK)
        {
            error = LastContextError(ctx, rcLoad);
            SbxNative.sbx_context_destroy(ctx);
            ctx = IntPtr.Zero;
            return false;
        }

        if (!TryConfigureAuxTones(ctx, out error))
        {
            SbxNative.sbx_context_destroy(ctx);
            ctx = IntPtr.Zero;
            return false;
        }

        durationSec = SbxNative.sbx_context_duration_sec(ctx);
        if (durationSec <= 0.0)
        {
            error = "Direct program context has zero duration.";
            SbxNative.sbx_context_destroy(ctx);
            ctx = IntPtr.Zero;
            return false;
        }

        return true;
    }

    private bool TryConfigureAuxTones(IntPtr ctx, out string error)
    {
        error = string.Empty;
        var tokens = SplitArgs(ToneSpecsTextBox.Text);
        if (tokens.Count == 0)
        {
            return true;
        }

        var aux = new List<SbxNative.SbxToneSpec>();
        foreach (var token in tokens)
        {
            if (token.StartsWith("mix/", StringComparison.OrdinalIgnoreCase) ||
                token.StartsWith("mixspin", StringComparison.OrdinalIgnoreCase) ||
                token.StartsWith("mixpulse", StringComparison.OrdinalIgnoreCase) ||
                token.StartsWith("mixbeat", StringComparison.OrdinalIgnoreCase))
            {
                error = $"Token '{token}' requires mix-stream integration; not yet supported in direct mode.";
                return false;
            }

            var rc = SbxNative.sbx_parse_tone_spec_ex(SbxNative.Utf8Z(token), SbxNative.SBX_WAVE_SINE, out var tone);
            if (rc != SbxNative.SBX_OK)
            {
                error = $"Could not parse tone token '{token}' in direct mode: {SbxNative.PtrToUtf8(SbxNative.sbx_status_string(rc))}";
                return false;
            }

            if (tone.mode != SbxNative.SBX_TONE_NONE)
            {
                aux.Add(tone);
            }
        }

        if (aux.Count == 0)
        {
            return true;
        }

        var setRc = SbxNative.sbx_context_set_aux_tones(ctx, aux.ToArray(), (UIntPtr)aux.Count);
        if (setRc != SbxNative.SBX_OK)
        {
            error = LastContextError(ctx, setRc);
            return false;
        }

        return true;
    }

    private bool TryBuildBuiltInKeyframes(string program, out BuiltInKeyframePlan plan, out string error)
    {
        plan = new BuiltInKeyframePlan();
        error = string.Empty;

        if (string.Equals(program, "drop", StringComparison.OrdinalIgnoreCase))
        {
            return TryBuildDropPlan(out plan, out error);
        }

        if (string.Equals(program, "sigmoid", StringComparison.OrdinalIgnoreCase))
        {
            return TryBuildSigmoidPlan(out plan, out error);
        }

        if (string.Equals(program, "slide", StringComparison.OrdinalIgnoreCase))
        {
            return TryBuildSlidePlan(out plan, out error);
        }

        error = $"Unsupported built-in program for direct mode: {program}";
        return false;
    }

    private bool TryBuildDropPlan(out BuiltInKeyframePlan plan, out string error)
    {
        plan = new BuiltInKeyframePlan();
        error = string.Empty;

        if (!TryParseTimingTriple((TimingTextBox.Text ?? string.Empty).Trim(), out var len0, out var len1, out var len2, out error))
        {
            return false;
        }

        if (!TryParseDropSigSpec((ProgramSpecTextBox.Text ?? string.Empty).Trim(), false, out var spec, out error))
        {
            return false;
        }

        var carr = 200.0 - 2.0 * spec.Level;
        if (carr < 0.0)
        {
            error = "Carrier underflow: signed-level produces carrier < 0.";
            return false;
        }

        var target = DropBeatTargets[spec.DepthIndex];
        var steplen = spec.HasStepMode ? 60 : 180;
        var nStep = 1 + (len0 - 1) / steplen;
        if (nStep < 2)
        {
            nStep = 2;
        }
        len0 = nStep * steplen;
        if (!spec.Slide)
        {
            len1 = ((len1 + steplen - 1) / steplen) * steplen;
        }

        var len = spec.IsLong ? len0 + len1 : len0;
        var c0 = carr + 5.0;
        var c2 = spec.IsLong ? carr - (5.0 * len1 / len0) : carr;

        var beats = new double[nStep];
        for (var a = 0; a < nStep; a++)
        {
            beats[a] = 10.0 * Math.Exp(Math.Log(target / 10.0) * a / (nStep - 1));
        }

        var kfs = new List<SbxNative.SbxProgramKeyframe>(nStep + 4);
        if (spec.Slide)
        {
            for (var a = 0; a < nStep; a++)
            {
                var tim = a * len0 / (double)(nStep - 1);
                var carrT = c0 + (c2 - c0) * tim / len;
                kfs.Add(CreateKeyframe(tim, spec, carrT, beats[a], spec.Amplitude, SbxNative.SBX_INTERP_LINEAR));
            }

            if (spec.IsLong)
            {
                kfs.Add(CreateKeyframe(len, spec, c2, beats[nStep - 1], spec.Amplitude, SbxNative.SBX_INTERP_LINEAR));
            }
        }
        else
        {
            var lim = len / steplen;
            for (var a = 0; a < lim; a++)
            {
                var tim0 = a * steplen;
                var tim1 = (a + 1) * steplen;
                var carrT = c0 + (c2 - c0) * tim1 / len;
                var beatT = beats[Math.Min(a, nStep - 1)];
                kfs.Add(CreateKeyframe(tim0, spec, carrT, beatT, spec.Amplitude, SbxNative.SBX_INTERP_STEP));
            }

            kfs.Add(CreateKeyframe(len, spec, c2, beats[nStep - 1], spec.Amplitude, SbxNative.SBX_INTERP_STEP));
        }

        var endSec = len;
        if (spec.Wakeup)
        {
            kfs.Add(CreateKeyframe(endSec + len2, spec, c0, beats[0], spec.Amplitude, SbxNative.SBX_INTERP_LINEAR));
            endSec += len2;
        }

        kfs.Add(CreateKeyframe(endSec + 10, spec, spec.Wakeup ? c0 : c2, spec.Wakeup ? beats[0] : beats[nStep - 1], 0.0, SbxNative.SBX_INTERP_LINEAR));
        plan.Keyframes = kfs.ToArray();
        return true;
    }

    private bool TryBuildSigmoidPlan(out BuiltInKeyframePlan plan, out string error)
    {
        plan = new BuiltInKeyframePlan();
        error = string.Empty;

        if (!TryParseTimingTriple((TimingTextBox.Text ?? string.Empty).Trim(), out var len0, out var len1, out var len2, out error))
        {
            return false;
        }

        if (!TryParseDropSigSpec((ProgramSpecTextBox.Text ?? string.Empty).Trim(), true, out var spec, out error))
        {
            return false;
        }

        var carr = 200.0 - 2.0 * spec.Level;
        if (carr < 0.0)
        {
            error = "Carrier underflow: signed-level produces carrier < 0.";
            return false;
        }

        var target = DropBeatTargets[spec.DepthIndex];
        var beatStart = 10.0;

        var steplen = spec.HasStepMode ? 60 : 180;
        if (len0 < 60)
        {
            error = "Sigmoid drop-time must be at least 1 minute.";
            return false;
        }

        var nStep = 1 + (len0 - 1) / steplen;
        if (nStep < 2)
        {
            nStep = 2;
        }
        len0 = nStep * steplen;
        if (!spec.Slide)
        {
            len1 = ((len1 + steplen - 1) / steplen) * steplen;
        }

        var len = spec.IsLong ? len0 + len1 : len0;
        var c0 = carr + 5.0;
        var c2 = spec.IsLong ? carr - (5.0 * len1 / len0) : carr;
        var dMin = len0 / 60.0;

        var u0 = Math.Tanh(spec.SigL * (0.0 - dMin / 2.0 - spec.SigH));
        var u1 = Math.Tanh(spec.SigL * (dMin - dMin / 2.0 - spec.SigH));
        var den = u1 - u0;
        if (Math.Abs(den) < 1e-9)
        {
            error = "Sigmoid parameters produce an invalid curve (try different l/h values).";
            return false;
        }

        var sigA = (target - beatStart) / den;
        var sigB = beatStart - sigA * u0;

        var beats = new double[nStep];
        for (var a = 0; a < nStep; a++)
        {
            var tim = a * len0 / (double)(nStep - 1);
            var tMin = tim / 60.0;
            beats[a] = tMin >= dMin
                ? target
                : sigA * Math.Tanh(spec.SigL * (tMin - dMin / 2.0 - spec.SigH)) + sigB;
        }

        var kfs = new List<SbxNative.SbxProgramKeyframe>(nStep + 4);
        if (spec.Slide)
        {
            for (var a = 0; a < nStep; a++)
            {
                var tim = a * len0 / (double)(nStep - 1);
                var carrT = c0 + (c2 - c0) * tim / len;
                kfs.Add(CreateKeyframe(tim, spec, carrT, beats[a], spec.Amplitude, SbxNative.SBX_INTERP_LINEAR));
            }

            if (spec.IsLong)
            {
                kfs.Add(CreateKeyframe(len, spec, c2, beats[nStep - 1], spec.Amplitude, SbxNative.SBX_INTERP_LINEAR));
            }
        }
        else
        {
            var lim = len / steplen;
            for (var a = 0; a < lim; a++)
            {
                var tim0 = a * steplen;
                var tim1 = (a + 1) * steplen;
                var carrT = c0 + (c2 - c0) * tim1 / len;
                var beatT = beats[Math.Min(a, nStep - 1)];
                kfs.Add(CreateKeyframe(tim0, spec, carrT, beatT, spec.Amplitude, SbxNative.SBX_INTERP_STEP));
            }

            kfs.Add(CreateKeyframe(len, spec, c2, beats[nStep - 1], spec.Amplitude, SbxNative.SBX_INTERP_STEP));
        }

        var endSec = len;
        if (spec.Wakeup)
        {
            kfs.Add(CreateKeyframe(endSec + len2, spec, c0, beats[0], spec.Amplitude, SbxNative.SBX_INTERP_LINEAR));
            endSec += len2;
        }

        kfs.Add(CreateKeyframe(endSec + 10, spec, spec.Wakeup ? c0 : c2, spec.Wakeup ? beats[0] : beats[nStep - 1], 0.0, SbxNative.SBX_INTERP_LINEAR));
        plan.Keyframes = kfs.ToArray();
        return true;
    }

    private bool TryBuildSlidePlan(out BuiltInKeyframePlan plan, out string error)
    {
        plan = new BuiltInKeyframePlan();
        error = string.Empty;

        if (!TryParseTimingSingle((TimingTextBox.Text ?? string.Empty).Trim(), out var len, out error))
        {
            return false;
        }

        var spec = (ProgramSpecTextBox.Text ?? string.Empty).Trim();
        var m = Regex.Match(spec, @"^\s*([+-]?[0-9]*\.?[0-9]+)\s*([+\-@M])\s*([+-]?[0-9]*\.?[0-9]+)\s*/\s*([0-9]*\.?[0-9]+)\s*$");
        if (!m.Success)
        {
            error = "Bad slide spec. Expected <carrier><+|-|@|M><beat>/<amp>, e.g. 200+10/1.";
            return false;
        }

        var c0 = ParseInvariantDouble(m.Groups[1].Value);
        var signal = m.Groups[2].Value[0];
        var beatRaw = ParseInvariantDouble(m.Groups[3].Value);
        var amp = ParseInvariantDouble(m.Groups[4].Value);

        var isMono = signal == 'M';
        var isIso = signal == '@';
        var beatAbs = Math.Abs(beatRaw);
        var beatVal = signal == '-' ? -beatAbs : beatAbs;
        var c1 = beatRaw / 2.0;

        var modeSpec = new DropSigSpec
        {
            IsIsochronic = isIso,
            IsMonaural = isMono
        };

        plan.Keyframes =
        [
            CreateKeyframe(0.0, modeSpec, c0, beatVal, amp, SbxNative.SBX_INTERP_LINEAR),
            CreateKeyframe(len, modeSpec, c1, beatVal, amp, SbxNative.SBX_INTERP_LINEAR),
            CreateKeyframe(len + 10.0, modeSpec, c1, beatVal, 0.0, SbxNative.SBX_INTERP_LINEAR)
        ];
        return true;
    }

    private static SbxNative.SbxProgramKeyframe CreateKeyframe(
        double timeSec,
        DropSigSpec spec,
        double carrierHz,
        double beatHz,
        double amp,
        int interp)
    {
        SbxNative.sbx_default_tone_spec(out var tone);
        tone.mode = spec.IsIsochronic
            ? SbxNative.SBX_TONE_ISOCHRONIC
            : (spec.IsMonaural ? SbxNative.SBX_TONE_MONAURAL : SbxNative.SBX_TONE_BINAURAL);
        tone.carrier_hz = carrierHz;
        tone.beat_hz = beatHz;
        tone.amplitude = amp;
        tone.waveform = SbxNative.SBX_WAVE_SINE;
        tone.duty_cycle = 0.4;

        return new SbxNative.SbxProgramKeyframe
        {
            time_sec = timeSec,
            tone = tone,
            interp = interp
        };
    }

    private bool TryParseDropSigSpec(string specText, bool isSigmoid, out DropSigSpec spec, out string error)
    {
        spec = new DropSigSpec();
        error = string.Empty;

        if (string.IsNullOrWhiteSpace(specText))
        {
            error = "Program spec is required.";
            return false;
        }

        var s = specText.Trim();
        var i = 0;
        while (i < s.Length && ("+-0123456789.".IndexOf(s[i]) >= 0))
        {
            i++;
        }

        if (i == 0 || i >= s.Length)
        {
            error = "Invalid signed-level in program spec.";
            return false;
        }

        if (!double.TryParse(s[..i], NumberStyles.Float, CultureInfo.InvariantCulture, out var level))
        {
            error = "Could not parse signed-level value.";
            return false;
        }
        spec.Level = level;

        var depth = char.ToLowerInvariant(s[i]);
        if (depth < 'a' || depth > 'l')
        {
            error = "Depth letter must be between a and l.";
            return false;
        }
        spec.DepthIndex = depth - 'a';
        i++;

        while (i < s.Length)
        {
            var c = s[i];
            if (c == 's' || c == 'k')
            {
                if (spec.HasStepMode)
                {
                    error = "Only one of s/k may be provided.";
                    return false;
                }
                spec.HasStepMode = true;
                spec.Slide = c == 's';
                i++;
                continue;
            }
            if (c == '+')
            {
                spec.IsLong = true;
                i++;
                continue;
            }
            if (c == '^')
            {
                spec.Wakeup = true;
                i++;
                continue;
            }
            if (c == '@')
            {
                spec.IsIsochronic = true;
                i++;
                continue;
            }
            if (c == 'M')
            {
                spec.IsMonaural = true;
                i++;
                continue;
            }
            if (c == '/')
            {
                i++;
                var start = i;
                while (i < s.Length && ("+-0123456789.eE".IndexOf(s[i]) >= 0))
                {
                    i++;
                }
                if (start == i || !double.TryParse(s[start..i], NumberStyles.Float, CultureInfo.InvariantCulture, out var amp))
                {
                    error = "Invalid amplitude after '/'.";
                    return false;
                }
                spec.Amplitude = amp;
                continue;
            }
            if (c == ':')
            {
                if (!isSigmoid)
                {
                    error = "Unexpected ':' in drop spec.";
                    return false;
                }
                i++;
                if (i + 2 > s.Length || s[i + 1] != '=')
                {
                    error = "Bad sigmoid parameter. Expected :l=<value> or :h=<value>.";
                    return false;
                }
                var key = s[i];
                i += 2;
                var start = i;
                while (i < s.Length && ("+-0123456789.eE".IndexOf(s[i]) >= 0))
                {
                    i++;
                }
                if (start == i || !double.TryParse(s[start..i], NumberStyles.Float, CultureInfo.InvariantCulture, out var val))
                {
                    error = "Bad numeric value in sigmoid parameter.";
                    return false;
                }
                if (key == 'l')
                {
                    spec.SigL = val;
                }
                else if (key == 'h')
                {
                    spec.SigH = val;
                }
                else
                {
                    error = "Unknown sigmoid parameter key (expected l or h).";
                    return false;
                }
                continue;
            }

            if (char.IsWhiteSpace(c))
            {
                break;
            }

            error = $"Trailing character '{c}' in program spec.";
            return false;
        }

        if (spec.IsIsochronic && spec.IsMonaural)
        {
            error = "M monaural mode cannot be combined with @ isochronic mode.";
            return false;
        }

        return true;
    }

    private bool TryParseTimingTriple(string timing, out int len0Sec, out int len1Sec, out int len2Sec, out string error)
    {
        len0Sec = 1800;
        len1Sec = 1800;
        len2Sec = 180;
        error = string.Empty;

        if (string.IsNullOrWhiteSpace(timing))
        {
            return true;
        }

        if (!timing.StartsWith("t", StringComparison.OrdinalIgnoreCase))
        {
            error = "Timing override must start with t, e.g. t30,30,0.";
            return false;
        }

        var tail = timing[1..];
        var parts = tail.Split(',', StringSplitOptions.TrimEntries);
        if (parts.Length != 3)
        {
            error = "Timing override for drop/sigmoid requires 3 comma-separated values: t<drop>,<hold>,<wake>.";
            return false;
        }

        if (!int.TryParse(parts[0], NumberStyles.Integer, CultureInfo.InvariantCulture, out var d) ||
            !int.TryParse(parts[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out var h) ||
            !int.TryParse(parts[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out var w))
        {
            error = "Timing override values must be whole minutes.";
            return false;
        }

        if (d <= 0 || h < 0 || w < 0)
        {
            error = "Timing override values must be non-negative, with drop > 0.";
            return false;
        }

        len0Sec = d * 60;
        len1Sec = h * 60;
        len2Sec = w * 60;
        return true;
    }

    private bool TryParseTimingSingle(string timing, out int lenSec, out string error)
    {
        lenSec = 1800;
        error = string.Empty;

        if (string.IsNullOrWhiteSpace(timing))
        {
            return true;
        }

        if (!timing.StartsWith("t", StringComparison.OrdinalIgnoreCase))
        {
            error = "Slide timing override must be t<minutes>, e.g. t60.";
            return false;
        }

        if (!double.TryParse(timing[1..], NumberStyles.Float, CultureInfo.InvariantCulture, out var mins))
        {
            error = "Invalid slide timing value.";
            return false;
        }

        if (mins <= 0)
        {
            error = "Slide timing must be > 0 minutes.";
            return false;
        }

        lenSec = (int)(mins * 60.0);
        return true;
    }

    private (bool Ok, string Error) RenderContextToWav(IntPtr ctx, double durationSec, string outPath)
    {
        try
        {
            SbxNative.sbx_default_engine_config(out var cfg);
            var sampleRate = cfg.sample_rate > 0 ? (int)Math.Round(cfg.sample_rate) : 44100;
            if (durationSec <= 0.0)
            {
                durationSec = 120.0;
            }

            var channels = 2;
            var totalFrames = (long)Math.Ceiling(durationSec * sampleRate);
            var blockFrames = 4096;
            var floatBuf = new float[blockFrames * channels];

            using var fs = new FileStream(outPath, FileMode.Create, FileAccess.Write, FileShare.Read);
            using var bw = new BinaryWriter(fs);

            WriteWavHeaderPlaceholder(bw, sampleRate, channels, 16);

            long framesWritten = 0;
            while (framesWritten < totalFrames)
            {
                var frames = (int)Math.Min(blockFrames, totalFrames - framesWritten);
                var rc = SbxNative.sbx_context_render_f32(ctx, floatBuf, (UIntPtr)frames);
                if (rc != SbxNative.SBX_OK)
                {
                    var err = LastContextError(ctx, rc);
                    return (false, err);
                }

                for (var i = 0; i < frames * channels; i++)
                {
                    var s = Math.Clamp(floatBuf[i], -1.0f, 1.0f);
                    var pcm = (short)Math.Round(s * short.MaxValue);
                    bw.Write(pcm);
                }

                framesWritten += frames;
            }

            FinalizeWavHeader(bw, sampleRate, channels, 16);
            return (true, string.Empty);
        }
        catch (Exception ex)
        {
            return (false, ex.Message);
        }
    }

    private static void WriteWavHeaderPlaceholder(BinaryWriter bw, int sampleRate, int channels, int bitsPerSample)
    {
        bw.Write("RIFF"u8.ToArray());
        bw.Write(0); // chunk size placeholder
        bw.Write("WAVE"u8.ToArray());
        bw.Write("fmt "u8.ToArray());
        bw.Write(16); // PCM subchunk size
        bw.Write((short)1); // PCM format
        bw.Write((short)channels);
        bw.Write(sampleRate);
        var byteRate = sampleRate * channels * bitsPerSample / 8;
        bw.Write(byteRate);
        var blockAlign = (short)(channels * bitsPerSample / 8);
        bw.Write(blockAlign);
        bw.Write((short)bitsPerSample);
        bw.Write("data"u8.ToArray());
        bw.Write(0); // data size placeholder
    }

    private static void FinalizeWavHeader(BinaryWriter bw, int sampleRate, int channels, int bitsPerSample)
    {
        var fileLen = bw.BaseStream.Length;
        var dataLen = (int)(fileLen - 44);
        var riffSize = (int)(fileLen - 8);

        bw.Seek(4, SeekOrigin.Begin);
        bw.Write(riffSize);
        bw.Seek(40, SeekOrigin.Begin);
        bw.Write(dataLen);
        bw.Seek(0, SeekOrigin.End);
    }

    private static string LastContextError(IntPtr ctx, int rc)
    {
        var err = SbxNative.PtrToUtf8(SbxNative.sbx_context_last_error(ctx));
        if (string.IsNullOrWhiteSpace(err))
        {
            err = SbxNative.PtrToUtf8(SbxNative.sbx_status_string(rc));
        }
        return string.IsNullOrWhiteSpace(err) ? $"sbagenxlib error code {rc}" : err;
    }

    private static double ParseInvariantDouble(string text)
    {
        return double.Parse(text, NumberStyles.Float, CultureInfo.InvariantCulture);
    }
}
