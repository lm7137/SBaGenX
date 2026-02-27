using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Avalonia.Threading;

namespace gui;

public partial class MainWindow : Window
{
    private Process? _activeProcess;

    private enum RunAction
    {
        Play,
        Export,
        Plot
    }

    public MainWindow()
    {
        InitializeComponent();
        UpdateQualityHint(resetValue: true);
        RefreshCommandPreview();
    }

    private async void OnPlayClick(object? sender, RoutedEventArgs e)
    {
        await RunActionAsync(RunAction.Play);
    }

    private async void OnExportClick(object? sender, RoutedEventArgs e)
    {
        await RunActionAsync(RunAction.Export);
    }

    private async void OnPlotClick(object? sender, RoutedEventArgs e)
    {
        await RunActionAsync(RunAction.Plot);
    }

    private void OnStopClick(object? sender, RoutedEventArgs e)
    {
        if (_activeProcess == null || _activeProcess.HasExited)
        {
            AppendLog("No active process to stop.");
            SetStatus("Idle");
            return;
        }

        try
        {
            _activeProcess.Kill(entireProcessTree: true);
            AppendLog("Stop requested.");
            SetStatus("Stopping...");
        }
        catch (Exception ex)
        {
            AppendLog($"Failed to stop process: {ex.Message}");
        }
    }

    private void OnInputsChanged(object? sender, RoutedEventArgs e)
    {
        RefreshCommandPreview();
    }

    private void OnOutputFormatChanged(object? sender, RoutedEventArgs e)
    {
        UpdateQualityHint(resetValue: false);
        RefreshCommandPreview();
    }

    private async void OnBrowseExecutableClick(object? sender, RoutedEventArgs e)
    {
        var path = await PickFileAsync("Select SBaGenX executable", null);
        if (!string.IsNullOrWhiteSpace(path))
        {
            ExecutableTextBox.Text = path;
        }
    }

    private void OnDetectExecutableClick(object? sender, RoutedEventArgs e)
    {
        var detected = DetectExecutablePath();
        if (string.IsNullOrWhiteSpace(detected))
        {
            AppendLog("Could not auto-detect sbagenx executable.");
            SetStatus("Executable not found");
            return;
        }

        ExecutableTextBox.Text = detected;
        AppendLog($"Auto-detected executable: {detected}");
        SetStatus("Executable detected");
    }

    private async void OnBrowseCurveFileClick(object? sender, RoutedEventArgs e)
    {
        var path = await PickFileAsync(
            "Select Curve File",
            new[] { "*.sbgf", "*.*" });
        if (!string.IsNullOrWhiteSpace(path))
        {
            CurveFileTextBox.Text = path;
        }
    }

    private async void OnBrowseMixFileClick(object? sender, RoutedEventArgs e)
    {
        var path = await PickFileAsync(
            "Select Mix Audio",
            new[] { "*.ogg", "*.mp3", "*.flac", "*.wav", "*.*" });
        if (!string.IsNullOrWhiteSpace(path))
        {
            MixFileTextBox.Text = path;
        }
    }

    private async void OnBrowseOutputFileClick(object? sender, RoutedEventArgs e)
    {
        var fmt = GetComboSelection(OutputFormatComboBox, "wav");
        var defaultExt = fmt;
        var savePath = await PickSaveFileAsync(
            "Select Output File",
            $"SBaGenX Output (*.{defaultExt})",
            new[] { $"*.{defaultExt}", "*.*" });

        if (!string.IsNullOrWhiteSpace(savePath))
        {
            OutputFileTextBox.Text = savePath;
        }
    }

    private async Task RunActionAsync(RunAction action)
    {
        if (_activeProcess != null && !_activeProcess.HasExited)
        {
            AppendLog("Another process is already running. Stop it before starting a new one.");
            return;
        }

        if (!TryBuildInvocation(action, out var executable, out var arguments, out var error))
        {
            SetStatus("Invalid configuration");
            AppendLog(error);
            RefreshCommandPreview();
            return;
        }

        CommandPreviewTextBox.Text = BuildPreviewCommand(executable, arguments);
        AppendLog($"> {CommandPreviewTextBox.Text}");
        SetStatus($"Running {action.ToString().ToLowerInvariant()}...");
        SetRunningState(true);

        var startInfo = new ProcessStartInfo
        {
            FileName = executable,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };
        foreach (var arg in arguments)
        {
            startInfo.ArgumentList.Add(arg);
        }

        var process = new Process { StartInfo = startInfo };
        _activeProcess = process;

        try
        {
            process.Start();
        }
        catch (Exception ex)
        {
            AppendLog($"Failed to start process: {ex.Message}");
            SetStatus("Start failed");
            SetRunningState(false);
            _activeProcess = null;
            return;
        }

        var stdoutTask = PumpLogAsync(process.StandardOutput, isError: false);
        var stderrTask = PumpLogAsync(process.StandardError, isError: true);

        try
        {
            await process.WaitForExitAsync();
            await Task.WhenAll(stdoutTask, stderrTask);

            var suffix = process.ExitCode == 0 ? "completed" : $"failed (exit {process.ExitCode})";
            SetStatus($"{action.ToString().ToLowerInvariant()} {suffix}");
            AppendLog($"Process finished with exit code {process.ExitCode}.");
        }
        catch (Exception ex)
        {
            AppendLog($"Process execution error: {ex.Message}");
            SetStatus("Execution error");
        }
        finally
        {
            process.Dispose();
            _activeProcess = null;
            SetRunningState(false);
        }
    }

    private async Task PumpLogAsync(StreamReader reader, bool isError)
    {
        while (true)
        {
            var line = await reader.ReadLineAsync();
            if (line == null)
            {
                break;
            }

            AppendLog(isError ? $"ERR: {line}" : line);
        }
    }

    private bool TryBuildInvocation(
        RunAction action,
        out string executable,
        out List<string> args,
        out string error)
    {
        args = new List<string>();
        error = string.Empty;
        executable = ResolveExecutable();

        if (string.IsNullOrWhiteSpace(executable))
        {
            error = "Could not find sbagenx executable. Set the path or click Auto-detect.";
            return false;
        }

        var extraArgs = SplitArgs(ExtraArgsTextBox.Text);
        args.AddRange(extraArgs);

        var mixFile = (MixFileTextBox.Text ?? string.Empty).Trim();
        if (!string.IsNullOrWhiteSpace(mixFile))
        {
            args.Add("-m");
            args.Add(mixFile);
        }

        if (action == RunAction.Export)
        {
            var format = GetComboSelection(OutputFormatComboBox, "wav");
            var outputPath = (OutputFileTextBox.Text ?? string.Empty).Trim();
            if (string.IsNullOrWhiteSpace(outputPath))
            {
                error = "Export requires an output file path.";
                return false;
            }

            outputPath = EnsureExtension(outputPath, format);
            OutputFileTextBox.Text = outputPath;

            args.Add("-o");
            args.Add(outputPath);

            if (!TryAppendEncoderArgs(args, format, out error))
            {
                return false;
            }
        }

        if (action == RunAction.Plot)
        {
            args.Add("-P");
        }

        var program = GetComboSelection(ProgramComboBox, "drop");
        var timing = (TimingTextBox.Text ?? string.Empty).Trim();
        var programSpec = (ProgramSpecTextBox.Text ?? string.Empty).Trim();
        var toneSpecs = SplitArgs(ToneSpecsTextBox.Text);

        if (string.IsNullOrWhiteSpace(programSpec))
        {
            error = "Program spec is required.";
            return false;
        }

        if (toneSpecs.Count == 0)
        {
            error = "At least one tone spec is required (e.g. mix/99).";
            return false;
        }

        args.Add("-p");
        args.Add(program);

        if (program == "curve")
        {
            var curveFile = (CurveFileTextBox.Text ?? string.Empty).Trim();
            if (string.IsNullOrWhiteSpace(curveFile))
            {
                error = "Program 'curve' requires a .sbgf file.";
                return false;
            }

            args.Add(curveFile);
        }

        if (!string.IsNullOrWhiteSpace(timing))
        {
            args.Add(timing);
        }

        args.Add(programSpec);
        args.AddRange(toneSpecs);
        return true;
    }

    private bool TryAppendEncoderArgs(List<string> args, string format, out string error)
    {
        error = string.Empty;
        var qualityText = (QualityTextBox.Text ?? string.Empty).Trim();

        if (format == "wav")
        {
            return true;
        }

        if (!int.TryParse(qualityText, out var q))
        {
            error = "Quality value must be an integer.";
            return false;
        }

        switch (format)
        {
            case "ogg":
                if (q < 0 || q > 10)
                {
                    error = "OGG quality must be between 0 and 10.";
                    return false;
                }
                args.Add("-U");
                args.Add(q.ToString());
                return true;

            case "flac":
                if (q < 0 || q > 12)
                {
                    error = "FLAC level must be between 0 and 12.";
                    return false;
                }
                args.Add("-Z");
                args.Add(q.ToString());
                return true;

            case "mp3":
            {
                var mode = GetComboSelection(Mp3ModeComboBox, "CBR");
                if (mode == "VBR")
                {
                    if (q < 0 || q > 9)
                    {
                        error = "MP3 VBR quality must be between 0 and 9.";
                        return false;
                    }
                    args.Add("-X");
                    args.Add(q.ToString());
                    return true;
                }

                if (q < 8 || q > 320)
                {
                    error = "MP3 bitrate must be between 8 and 320 kbps.";
                    return false;
                }
                args.Add("-K");
                args.Add(q.ToString());
                return true;
            }

            default:
                error = $"Unsupported output format '{format}'.";
                return false;
        }
    }

    private string ResolveExecutable()
    {
        var fromField = (ExecutableTextBox.Text ?? string.Empty).Trim();
        if (!string.IsNullOrWhiteSpace(fromField))
        {
            if (Path.IsPathRooted(fromField) || fromField.Contains(Path.DirectorySeparatorChar) || fromField.Contains(Path.AltDirectorySeparatorChar))
            {
                return fromField;
            }

            return FindInPath(fromField) ?? fromField;
        }

        var detected = DetectExecutablePath();
        if (!string.IsNullOrWhiteSpace(detected))
        {
            ExecutableTextBox.Text = detected;
        }
        return detected ?? string.Empty;
    }

    private string? DetectExecutablePath()
    {
        var env = Environment.GetEnvironmentVariable("SBAGENX_BIN");
        if (!string.IsNullOrWhiteSpace(env))
        {
            return env;
        }

        var cwd = Environment.CurrentDirectory;
        var candidates = new[]
        {
            Path.Combine(cwd, "dist", "sbagenx-linux64"),
            Path.Combine(cwd, "dist", "sbagenx"),
            Path.Combine(cwd, "dist", "sbagenx-win64.exe"),
            Path.Combine(cwd, "dist", "sbagenx-win32.exe"),
        };

        foreach (var candidate in candidates)
        {
            if (File.Exists(candidate))
            {
                return candidate;
            }
        }

        var inPath = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? FindInPath("sbagenx.exe") ?? FindInPath("sbagenx")
            : FindInPath("sbagenx");
        return inPath;
    }

    private static string? FindInPath(string executableName)
    {
        var path = Environment.GetEnvironmentVariable("PATH");
        if (string.IsNullOrWhiteSpace(path))
        {
            return null;
        }

        var parts = path.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries);
        foreach (var part in parts)
        {
            try
            {
                var candidate = Path.Combine(part.Trim(), executableName);
                if (File.Exists(candidate))
                {
                    return candidate;
                }
            }
            catch
            {
                // Ignore invalid path entries.
            }
        }

        return null;
    }

    private void RefreshCommandPreview()
    {
        if (!TryBuildInvocation(RunAction.Play, out var exe, out var args, out var error))
        {
            CommandPreviewTextBox.Text = $"# {error}";
            return;
        }

        CommandPreviewTextBox.Text = BuildPreviewCommand(exe, args);
    }

    private static string BuildPreviewCommand(string executable, IReadOnlyCollection<string> args)
    {
        var quotedArgs = args.Select(QuoteArg);
        return $"{QuoteArg(executable)} {string.Join(" ", quotedArgs)}";
    }

    private static string QuoteArg(string arg)
    {
        if (string.IsNullOrEmpty(arg))
        {
            return "\"\"";
        }

        var needsQuotes = arg.Any(char.IsWhiteSpace) || arg.Contains('"');
        if (!needsQuotes)
        {
            return arg;
        }

        return $"\"{arg.Replace("\"", "\\\"")}\"";
    }

    private static List<string> SplitArgs(string? input)
    {
        var result = new List<string>();
        if (string.IsNullOrWhiteSpace(input))
        {
            return result;
        }

        var current = new StringBuilder();
        var inQuotes = false;
        foreach (var c in input)
        {
            if (c == '"')
            {
                inQuotes = !inQuotes;
                continue;
            }

            if (char.IsWhiteSpace(c) && !inQuotes)
            {
                if (current.Length > 0)
                {
                    result.Add(current.ToString());
                    current.Clear();
                }
                continue;
            }

            current.Append(c);
        }

        if (current.Length > 0)
        {
            result.Add(current.ToString());
        }

        return result;
    }

    private static string EnsureExtension(string path, string format)
    {
        var ext = "." + format.Trim().TrimStart('.').ToLowerInvariant();
        if (path.EndsWith(ext, StringComparison.OrdinalIgnoreCase))
        {
            return path;
        }

        return Path.ChangeExtension(path, ext);
    }

    private static string GetComboSelection(ComboBox combo, string fallback)
    {
        if (combo.SelectedItem is ComboBoxItem item && item.Content is string selected)
        {
            return selected.Trim();
        }

        return fallback;
    }

    private void UpdateQualityHint(bool resetValue)
    {
        var format = GetComboSelection(OutputFormatComboBox, "wav");
        var mp3Mode = GetComboSelection(Mp3ModeComboBox, "CBR");

        switch (format)
        {
            case "ogg":
                QualityHintTextBlock.Text = "OGG: -U quality (0..10). Typical value: 6.";
                if (resetValue)
                {
                    QualityTextBox.Text = "6";
                }
                break;

            case "flac":
                QualityHintTextBlock.Text = "FLAC: -Z compression level (0..12).";
                if (resetValue)
                {
                    QualityTextBox.Text = "5";
                }
                break;

            case "mp3":
                if (mp3Mode == "VBR")
                {
                    QualityHintTextBlock.Text = "MP3 VBR: -X quality (0..9, lower is better). Typical value: 2.";
                    if (resetValue)
                    {
                        QualityTextBox.Text = "2";
                    }
                }
                else
                {
                    QualityHintTextBlock.Text = "MP3 CBR: -K bitrate kbps (8..320). Typical value: 192.";
                    if (resetValue)
                    {
                        QualityTextBox.Text = "192";
                    }
                }
                break;

            default:
                QualityHintTextBlock.Text = "WAV: no quality flag.";
                if (resetValue)
                {
                    QualityTextBox.Text = "6";
                }
                break;
        }
    }

    private void SetRunningState(bool running)
    {
        PlayButton.IsEnabled = !running;
        ExportButton.IsEnabled = !running;
        PlotButton.IsEnabled = !running;
        StopButton.IsEnabled = running;
    }

    private void SetStatus(string status)
    {
        StatusTextBlock.Text = status;
    }

    private void AppendLog(string line)
    {
        var stamped = $"[{DateTime.Now:HH:mm:ss}] {line}";
        Dispatcher.UIThread.Post(() =>
        {
            LogTextBox.Text = string.IsNullOrEmpty(LogTextBox.Text)
                ? stamped
                : $"{LogTextBox.Text}{Environment.NewLine}{stamped}";
            LogTextBox.CaretIndex = LogTextBox.Text?.Length ?? 0;
        });
    }

    private async Task<string?> PickFileAsync(string title, string[]? patterns)
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel?.StorageProvider == null)
        {
            return null;
        }

        var options = new FilePickerOpenOptions
        {
            Title = title,
            AllowMultiple = false
        };

        if (patterns != null && patterns.Length > 0)
        {
            options.FileTypeFilter =
            [
                new FilePickerFileType("Supported files")
                {
                    Patterns = patterns
                }
            ];
        }

        var files = await topLevel.StorageProvider.OpenFilePickerAsync(options);
        if (files.Count == 0)
        {
            return null;
        }

        return ToLocalPath(files[0]);
    }

    private async Task<string?> PickSaveFileAsync(string title, string typeLabel, string[] patterns)
    {
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel?.StorageProvider == null)
        {
            return null;
        }

        var file = await topLevel.StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = title,
            FileTypeChoices =
            [
                new FilePickerFileType(typeLabel)
                {
                    Patterns = patterns
                }
            ]
        });

        if (file == null)
        {
            return null;
        }

        return ToLocalPath(file);
    }

    private static string? ToLocalPath(IStorageItem item)
    {
        if (item.Path.IsFile)
        {
            return item.Path.LocalPath;
        }

        return null;
    }
}
