using System.Diagnostics;
using System.IO.Compression;
using System.Net;
using System.Net.Http.Headers;
using System.Text.Json;

namespace BrokenHorizonLauncher;

internal sealed class LauncherConfig
{
    public string GameName { get; set; } = "Broken Horizon";
    public string DownloadUrl { get; set; } = "";
    public string GameExecutableName { get; set; } = "BrokenHorizon.exe";
    public string InstallFolderName { get; set; } = "BrokenHorizon";
    public bool LaunchAfterUpdate { get; set; } = true;
}

internal sealed class InstalledState
{
    public string RemoteFingerprint { get; set; } = "";
    public string ExecutablePath { get; set; } = "";
}

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new LauncherForm());
    }
}

internal sealed class LauncherForm : Form
{
    private readonly Label title = new() { Text = "BROKEN HORIZON", AutoSize = true, Font = new Font("Segoe UI", 24, FontStyle.Bold), ForeColor = Color.White };
    private readonly Label status = new() { Text = "Starting launcher...", AutoSize = false, TextAlign = ContentAlignment.MiddleCenter, ForeColor = Color.Gainsboro };
    private readonly ProgressBar progress = new() { Style = ProgressBarStyle.Marquee, MarqueeAnimationSpeed = 25 };
    private readonly Button play = new() { Text = "PLAY", Enabled = false, FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(190, 82, 35), ForeColor = Color.White, Font = new Font("Segoe UI", 13, FontStyle.Bold) };
    private readonly Button retry = new() { Text = "RETRY", Visible = false, FlatStyle = FlatStyle.Flat, ForeColor = Color.White };
    private LauncherConfig config = new();
    private string? gameExecutable;

    public LauncherForm()
    {
        Text = "Broken Horizon Launcher";
        ClientSize = new Size(620, 330);
        MinimumSize = new Size(520, 300);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.FromArgb(20, 25, 29);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;

        title.Location = new Point(145, 55);
        title.Anchor = AnchorStyles.Top;
        status.SetBounds(55, 125, 510, 48);
        progress.SetBounds(75, 183, 470, 18);
        play.SetBounds(210, 225, 200, 48);
        retry.SetBounds(255, 280, 110, 30);
        play.FlatAppearance.BorderSize = 0;
        retry.FlatAppearance.BorderColor = Color.DimGray;
        Controls.AddRange([title, status, progress, play, retry]);

        Shown += async (_, _) => await CheckAndLaunchAsync();
        play.Click += (_, _) => LaunchGame();
        retry.Click += async (_, _) => await CheckAndLaunchAsync();
    }

    private async Task CheckAndLaunchAsync()
    {
        SetBusy(true, "Checking for updates...");
        retry.Visible = false;
        try
        {
            config = LoadConfig();
            if (!Uri.TryCreate(config.DownloadUrl, UriKind.Absolute, out var downloadUri) || downloadUri.Scheme != Uri.UriSchemeHttps)
                throw new InvalidOperationException("The launcher DownloadUrl is missing or invalid.");

            string dataRoot = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), config.InstallFolderName);
            string gameRoot = Path.Combine(dataRoot, "Game");
            string statePath = Path.Combine(dataRoot, "launcher-state.json");
            Directory.CreateDirectory(dataRoot);
            InstalledState state = LoadState(statePath);
            gameExecutable = FindGameExecutable(gameRoot, state.ExecutablePath);

            using var client = CreateHttpClient();
            string fingerprint = await GetRemoteFingerprintAsync(client, downloadUri);
            bool needsUpdate = gameExecutable is null || !string.Equals(state.RemoteFingerprint, fingerprint, StringComparison.Ordinal);
            if (needsUpdate)
            {
                await InstallUpdateAsync(client, downloadUri, dataRoot, gameRoot);
                gameExecutable = FindGameExecutable(gameRoot, null)
                    ?? throw new InvalidDataException($"The downloaded ZIP does not contain a valid {config.GameExecutableName}.");
                state = new InstalledState { RemoteFingerprint = fingerprint, ExecutablePath = gameExecutable };
                File.WriteAllText(statePath, JsonSerializer.Serialize(state, JsonOptions()));
            }

            SetBusy(false, needsUpdate ? "Update installed. Ready to play." : "Game is up to date. Ready to play.");
            if (config.LaunchAfterUpdate)
                LaunchGame();
        }
        catch (Exception ex)
        {
            SetBusy(false, "Update failed: " + ex.Message);
            retry.Visible = true;
            play.Enabled = gameExecutable is not null;
        }
    }

    private async Task InstallUpdateAsync(HttpClient client, Uri uri, string dataRoot, string gameRoot)
    {
        string workRoot = Path.Combine(dataRoot, "UpdateWork");
        string zipPath = Path.Combine(workRoot, "update.zip");
        string extractRoot = Path.Combine(workRoot, "Extracted");
        string stagedRoot = Path.Combine(workRoot, "StagedGame");
        RecreateDirectory(workRoot);
        Directory.CreateDirectory(extractRoot);

        status.Text = "Downloading update...";
        progress.Style = ProgressBarStyle.Continuous;
        progress.Value = 0;
        await DownloadUpdateAsync(client, uri, zipPath);

        status.Text = "Installing update...";
        progress.Style = ProgressBarStyle.Marquee;
        ZipFile.ExtractToDirectory(zipPath, extractRoot, true);
        string sourceExecutable = FindGameExecutable(extractRoot, null)
            ?? throw new InvalidDataException($"The update contains no valid {config.GameExecutableName}.");
        string payloadRoot = FindPayloadRoot(sourceExecutable, extractRoot);
        CopyDirectory(payloadRoot, stagedRoot);

        string? stagedExecutable = FindGameExecutable(stagedRoot, null);
        if (stagedExecutable is null)
            throw new InvalidDataException("The staged game failed executable validation.");

        string backupRoot = Path.Combine(dataRoot, "PreviousGame");
        if (Directory.Exists(backupRoot)) Directory.Delete(backupRoot, true);
        if (Directory.Exists(gameRoot)) Directory.Move(gameRoot, backupRoot);
        try
        {
            Directory.Move(stagedRoot, gameRoot);
        }
        catch
        {
            if (!Directory.Exists(gameRoot) && Directory.Exists(backupRoot)) Directory.Move(backupRoot, gameRoot);
            throw;
        }
        if (Directory.Exists(backupRoot)) Directory.Delete(backupRoot, true);
        if (Directory.Exists(workRoot)) Directory.Delete(workRoot, true);
    }

    private async Task DownloadUpdateAsync(HttpClient client, Uri uri, string zipPath)
    {
        const int chunkCount = 8;
        const long parallelThreshold = 16L * 1024 * 1024;
        long? total = null;
        Uri resolvedUri = uri;

        try
        {
            using var head = new HttpRequestMessage(HttpMethod.Head, uri);
            using var response = await client.SendAsync(head, HttpCompletionOption.ResponseHeadersRead);
            if (response.IsSuccessStatusCode)
            {
                total = response.Content.Headers.ContentLength;
                resolvedUri = response.RequestMessage?.RequestUri ?? uri;
            }
        }
        catch { /* The sequential path remains available. */ }

        // GitHub's release CDN supports ranges but does not consistently expose the
        // Accept-Ranges header after redirects. Try parallel mode whenever the size
        // is known, and fall back safely if the server answers without HTTP 206.
        if (total >= parallelThreshold)
        {
            try
            {
                status.Text = $"Downloading update with {chunkCount} connections...";
                long received = 0;
                var speedTimer = Stopwatch.StartNew();
                var displayProgress = new Progress<long>(delta =>
                {
                    long current = Interlocked.Add(ref received, delta);
                    progress.Value = Math.Clamp((int)(current * 100L / total.Value), 0, 100);
                    double mbPerSecond = current / 1048576d / Math.Max(0.1, speedTimer.Elapsed.TotalSeconds);
                    status.Text = $"Downloading update... {current / 1048576:N0} / {total.Value / 1048576:N0} MB  ({mbPerSecond:N1} MB/s)";
                });
                long chunkSize = (total.Value + chunkCount - 1) / chunkCount;
                string[] parts = Enumerable.Range(0, chunkCount).Select(i => zipPath + $".part{i}").ToArray();
                Task[] downloads = parts.Select((part, index) =>
                {
                    long start = index * chunkSize;
                    long end = Math.Min(total.Value - 1, start + chunkSize - 1);
                    return DownloadRangeAsync(client, resolvedUri, part, start, end, displayProgress);
                }).ToArray();
                await Task.WhenAll(downloads);
                await using (var output = new FileStream(zipPath, FileMode.Create, FileAccess.Write, FileShare.None, 4 * 1024 * 1024, true))
                {
                    foreach (string part in parts)
                    {
                        await using var input = new FileStream(part, FileMode.Open, FileAccess.Read, FileShare.Read, 4 * 1024 * 1024, true);
                        await input.CopyToAsync(output, 4 * 1024 * 1024);
                        File.Delete(part);
                    }
                }
                return;
            }
            catch
            {
                foreach (string part in Directory.EnumerateFiles(Path.GetDirectoryName(zipPath)!, Path.GetFileName(zipPath) + ".part*"))
                    File.Delete(part);
                if (File.Exists(zipPath)) File.Delete(zipPath);
                status.Text = "Parallel download unavailable; using standard download...";
            }
        }

        await DownloadSequentialAsync(client, uri, zipPath);
    }

    private static async Task DownloadRangeAsync(HttpClient client, Uri uri, string path, long start, long end, IProgress<long> progressReport)
    {
        using var request = new HttpRequestMessage(HttpMethod.Get, uri);
        request.Headers.Range = new RangeHeaderValue(start, end);
        using var response = await client.SendAsync(request, HttpCompletionOption.ResponseHeadersRead);
        if (response.StatusCode != HttpStatusCode.PartialContent)
            throw new HttpRequestException("The download server did not accept parallel byte ranges.");
        await using var input = await response.Content.ReadAsStreamAsync();
        await using var output = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None, 1024 * 1024, true);
        byte[] buffer = new byte[1024 * 1024];
        int count;
        while ((count = await input.ReadAsync(buffer)) > 0)
        {
            await output.WriteAsync(buffer.AsMemory(0, count));
            progressReport.Report(count);
        }
    }

    private async Task DownloadSequentialAsync(HttpClient client, Uri uri, string path)
    {
        using var response = await client.GetAsync(uri, HttpCompletionOption.ResponseHeadersRead);
        response.EnsureSuccessStatusCode();
        long? total = response.Content.Headers.ContentLength;
        await using var input = await response.Content.ReadAsStreamAsync();
        await using var output = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None, 4 * 1024 * 1024, true);
        byte[] buffer = new byte[4 * 1024 * 1024];
        long received = 0;
        var speedTimer = Stopwatch.StartNew();
        int count;
        while ((count = await input.ReadAsync(buffer)) > 0)
        {
            await output.WriteAsync(buffer.AsMemory(0, count));
            received += count;
            if (total > 0) progress.Value = Math.Clamp((int)(received * 100L / total.Value), 0, 100);
            double mbPerSecond = received / 1048576d / Math.Max(0.1, speedTimer.Elapsed.TotalSeconds);
            status.Text = total > 0
                ? $"Downloading update... {received / 1048576:N0} / {total.Value / 1048576:N0} MB  ({mbPerSecond:N1} MB/s)"
                : $"Downloading update... {received / 1048576:N0} MB  ({mbPerSecond:N1} MB/s)";
            Application.DoEvents();
        }
    }

    private static string FindPayloadRoot(string executable, string extractRoot)
    {
        var directory = new DirectoryInfo(Path.GetDirectoryName(executable)!);
        var boundary = new DirectoryInfo(extractRoot);
        DirectoryInfo best = directory;
        for (DirectoryInfo? current = directory; current is not null && current.FullName.StartsWith(boundary.FullName, StringComparison.OrdinalIgnoreCase); current = current.Parent)
        {
            if (Directory.Exists(Path.Combine(current.FullName, "BrokenHorizon", "Content", "Paks")) ||
                Directory.Exists(Path.Combine(current.FullName, "Engine")))
                best = current;
        }
        return best.FullName;
    }

    private string? FindGameExecutable(string root, string? preferred)
    {
        if (!string.IsNullOrWhiteSpace(preferred) && File.Exists(preferred) && IsWindowsExecutable(preferred)) return preferred;
        if (!Directory.Exists(root)) return null;
        return Directory.EnumerateFiles(root, config.GameExecutableName, SearchOption.AllDirectories)
            .Where(IsWindowsExecutable)
            .OrderBy(path => path.Count(c => c == Path.DirectorySeparatorChar))
            .FirstOrDefault();
    }

    private static bool IsWindowsExecutable(string path)
    {
        try
        {
            using var stream = File.OpenRead(path);
            return stream.Length > 64 && stream.ReadByte() == 'M' && stream.ReadByte() == 'Z';
        }
        catch { return false; }
    }

    private static async Task<string> GetRemoteFingerprintAsync(HttpClient client, Uri uri)
    {
        using var request = new HttpRequestMessage(HttpMethod.Head, uri);
        using var response = await client.SendAsync(request, HttpCompletionOption.ResponseHeadersRead);
        if (!response.IsSuccessStatusCode)
        {
            using var fallback = await client.GetAsync(uri, HttpCompletionOption.ResponseHeadersRead);
            fallback.EnsureSuccessStatusCode();
            return Fingerprint(fallback);
        }
        return Fingerprint(response);
    }

    private static string Fingerprint(HttpResponseMessage response)
    {
        string etag = response.Headers.ETag?.Tag ?? "";
        string modified = response.Content.Headers.LastModified?.UtcDateTime.Ticks.ToString() ?? "";
        string length = response.Content.Headers.ContentLength?.ToString() ?? "";
        // GitHub's final redirected blob URL contains a short-lived signature, so it
        // must not be part of update identity. ETag/modified/length stay stable until
        // the release asset is replaced.
        return $"{etag}|{modified}|{length}";
    }

    private static HttpClient CreateHttpClient()
    {
        var client = new HttpClient(new HttpClientHandler { AutomaticDecompression = DecompressionMethods.All }) { Timeout = TimeSpan.FromMinutes(45) };
        client.DefaultRequestHeaders.UserAgent.Add(new ProductInfoHeaderValue("BrokenHorizonLauncher", "1.0"));
        return client;
    }

    private LauncherConfig LoadConfig()
    {
        string path = Path.Combine(AppContext.BaseDirectory, "launcher-config.json");
        if (!File.Exists(path)) throw new FileNotFoundException("launcher-config.json is missing beside the launcher.");
        return JsonSerializer.Deserialize<LauncherConfig>(File.ReadAllText(path), JsonOptions()) ?? new LauncherConfig();
    }

    private static InstalledState LoadState(string path)
    {
        try { return File.Exists(path) ? JsonSerializer.Deserialize<InstalledState>(File.ReadAllText(path), JsonOptions()) ?? new() : new(); }
        catch { return new(); }
    }

    private static JsonSerializerOptions JsonOptions() => new() { PropertyNameCaseInsensitive = true, WriteIndented = true };

    private static void RecreateDirectory(string path)
    {
        if (Directory.Exists(path)) Directory.Delete(path, true);
        Directory.CreateDirectory(path);
    }

    private static void CopyDirectory(string source, string destination)
    {
        Directory.CreateDirectory(destination);
        foreach (string file in Directory.EnumerateFiles(source)) File.Copy(file, Path.Combine(destination, Path.GetFileName(file)), true);
        foreach (string directory in Directory.EnumerateDirectories(source)) CopyDirectory(directory, Path.Combine(destination, Path.GetFileName(directory)));
    }

    private void SetBusy(bool busy, string text)
    {
        status.Text = text;
        progress.Visible = busy;
        if (busy) { progress.Style = ProgressBarStyle.Marquee; progress.MarqueeAnimationSpeed = 25; }
        play.Enabled = !busy && gameExecutable is not null;
    }

    private void LaunchGame()
    {
        if (gameExecutable is null || !IsWindowsExecutable(gameExecutable))
        {
            status.Text = "The installed game executable is missing or invalid. Click Retry.";
            retry.Visible = true;
            return;
        }
        try
        {
            Process.Start(new ProcessStartInfo(gameExecutable) { WorkingDirectory = Path.GetDirectoryName(gameExecutable)!, UseShellExecute = true });
            Close();
        }
        catch (Exception ex)
        {
            status.Text = "Could not launch the game: " + ex.Message;
            retry.Visible = true;
        }
    }
}
