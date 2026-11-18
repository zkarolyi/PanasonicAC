// AcMonitor — subscribes to the PanasonicAC firmware's MQTT topics and records
// AC state (from <unit>/out/) to daily CSV files, and optionally the debug log
// (<unit>/log/, incl. raw hex packets) to daily JSONL files.

using System.Globalization;
using System.Text;
using System.Text.Json;
using MQTTnet;

Console.OutputEncoding = Encoding.UTF8;

var opt = Options.Parse(args);
if (opt is null) return args.Contains("--help") || args.Contains("-h") ? 0 : 1;

Directory.CreateDirectory(opt.OutDir);
var writer = new DataWriter(opt.OutDir);

var factory = new MqttClientFactory();
using var client = factory.CreateMqttClient();

var clientOptions = new MqttClientOptionsBuilder()
    .WithTcpServer(opt.Broker, opt.Port)
    .WithCredentials(opt.User, opt.Password)
    .WithClientId($"AcMonitor_{Environment.MachineName}_{Environment.ProcessId}")
    .WithCleanSession()
    .WithKeepAlivePeriod(TimeSpan.FromSeconds(30))
    .Build();

var subscribeBuilder = factory.CreateSubscribeOptionsBuilder();
foreach (var unit in opt.Units)
{
    subscribeBuilder.WithTopicFilter($"PanasonicAC_{unit}/out/#");
    if (opt.Log) subscribeBuilder.WithTopicFilter($"PanasonicAC_{unit}/log/#");
}
var subscribeOptions = subscribeBuilder.Build();

client.ApplicationMessageReceivedAsync += e =>
{
    var topic = e.ApplicationMessage.Topic;
    var payload = e.ApplicationMessage.ConvertPayloadToString() ?? "";
    var parts = topic.Split('/');
    if (parts.Length < 2) return Task.CompletedTask;

    var unit = parts[0];
    var kind = parts[1];
    var now = DateTimeOffset.Now;

    try
    {
        if (kind == "out")
        {
            var line = writer.WriteState(unit, now, payload);
            if (!opt.Quiet) Console.WriteLine($"{now:HH:mm:ss} {unit} {line}");
        }
        else if (kind == "log")
        {
            if (payload == "Ping" && !opt.Ping) return Task.CompletedTask;
            writer.WriteLog(unit, now, payload);
            if (opt.Verbose) Console.WriteLine($"{now:HH:mm:ss} {unit} log: {payload}");
        }
    }
    catch (Exception ex)
    {
        Console.Error.WriteLine($"{now:HH:mm:ss} {unit} failed to record '{payload}': {ex.Message}");
    }
    return Task.CompletedTask;
};

client.DisconnectedAsync += e =>
{
    if (e.ClientWasConnected)
        Console.Error.WriteLine($"{DateTime.Now:HH:mm:ss} Disconnected: {e.Reason}");
    return Task.CompletedTask;
};

using var cts = new CancellationTokenSource();
Console.CancelKeyPress += (_, e) => { e.Cancel = true; cts.Cancel(); };

Console.WriteLine($"Broker {opt.Broker}:{opt.Port}, units {string.Join(",", opt.Units)}, " +
                  $"log topics {(opt.Log ? "on" : "off")}, output {Path.GetFullPath(opt.OutDir)}. Ctrl+C to stop.");

// Connect / reconnect loop.
using var timer = new PeriodicTimer(TimeSpan.FromSeconds(5));
try
{
    do
    {
        if (client.IsConnected) continue;
        try
        {
            await client.ConnectAsync(clientOptions, cts.Token);
            await client.SubscribeAsync(subscribeOptions, cts.Token);
            Console.WriteLine($"{DateTime.Now:HH:mm:ss} Connected and subscribed.");
        }
        catch (OperationCanceledException) { break; }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"{DateTime.Now:HH:mm:ss} Connect failed: {ex.Message}. Retrying in 5 s.");
        }
    } while (await timer.WaitForNextTickAsync(cts.Token));
}
catch (OperationCanceledException) { }

if (client.IsConnected)
    await client.DisconnectAsync();
Console.WriteLine("Stopped.");
return 0;

sealed class DataWriter(string outDir)
{
    private static readonly string StateHeader =
        "timestamp,acState,internalTemp,externalTemp,targetTemp,mode,modeRaw,fanSpeed,fanSpeedRaw,swing,swingRaw,preset,presetRaw";

    private readonly Lock _lock = new();

    /// <summary>Appends one state row; returns a short human-readable summary.</summary>
    public string WriteState(string unit, DateTimeOffset ts, string json)
    {
        using var doc = JsonDocument.Parse(json);
        var r = doc.RootElement;
        var t = r.GetProperty("temperature");

        string Num(JsonElement e, string name) =>
            e.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.Number
                ? v.GetDouble().ToString(CultureInfo.InvariantCulture) : "";
        (string text, string raw) Enum(string name) =>
            r.TryGetProperty(name, out var o)
                ? (o.TryGetProperty("text", out var tx) ? tx.GetString() ?? "" : "",
                   o.TryGetProperty("value", out var v) ? v.GetRawText() : "")
                : ("", "");

        var (mode, modeRaw) = Enum("mode");
        var (fan, fanRaw) = Enum("fanSpeed");
        var (swing, swingRaw) = Enum("swingMode");
        var (preset, presetRaw) = Enum("preset");

        var row = string.Join(",",
            ts.ToString("o", CultureInfo.InvariantCulture),
            Num(r, "acState"),
            Num(t, "internal"), Num(t, "external"), Num(t, "target"),
            Csv(mode), modeRaw, Csv(fan), fanRaw, Csv(swing), swingRaw, Csv(preset), presetRaw);

        Append(unit, "state", "csv", ts, row, StateHeader);

        return $"in {Num(t, "internal")}°C out {Num(t, "external")}°C target {Num(t, "target")}°C " +
               $"{mode} fan {fan} swing {swing} {preset}";
    }

    public void WriteLog(string unit, DateTimeOffset ts, string message)
    {
        var line = JsonSerializer.Serialize(new { timestamp = ts, message });
        Append(unit, "log", "jsonl", ts, line, header: null);
    }

    private void Append(string unit, string kind, string ext, DateTimeOffset ts, string line, string? header)
    {
        var dir = Path.Combine(outDir, unit);
        var path = Path.Combine(dir, $"{kind}_{ts:yyyy-MM-dd}.{ext}");
        lock (_lock)
        {
            Directory.CreateDirectory(dir);
            var isNew = !File.Exists(path);
            using var w = new StreamWriter(path, append: true, new UTF8Encoding(false));
            if (isNew && header is not null) w.WriteLine(header);
            w.WriteLine(line);
        }
    }

    private static string Csv(string s) =>
        s.IndexOfAny([',', '"', '\n']) >= 0 ? $"\"{s.Replace("\"", "\"\"")}\"" : s;
}

sealed record Options(
    string Broker, int Port, string User, string Password, string[] Units,
    string OutDir, bool Log, bool Ping, bool Verbose, bool Quiet)
{
    public static Options? Parse(string[] args)
    {
        // Defaults match src/main.h; env vars override defaults, CLI args override both.
        string broker = Env("AC_MQTT_BROKER") ?? "192.168.1.120";
        int port = int.TryParse(Env("AC_MQTT_PORT"), out var p) ? p : 1883;
        string user = Env("AC_MQTT_USER") ?? "openhabian";
        string password = Env("AC_MQTT_PASSWORD") ?? "openhabian";
        string units = "A,B,C";
        string outDir = "ac-data";
        bool log = false, ping = false, verbose = false, quiet = false;

        for (int i = 0; i < args.Length; i++)
        {
            string Next() => i + 1 < args.Length ? args[++i] : throw new ArgumentException($"{args[i]} needs a value");
            try
            {
                switch (args[i])
                {
                    case "--broker" or "-b": broker = Next(); break;
                    case "--port": port = int.Parse(Next()); break;
                    case "--user" or "-u": user = Next(); break;
                    case "--password" or "-p": password = Next(); break;
                    case "--units": units = Next(); break;
                    case "--out" or "-o": outDir = Next(); break;
                    case "--log": log = true; break;
                    case "--ping": ping = true; break;
                    case "--verbose" or "-v": verbose = true; break;
                    case "--quiet" or "-q": quiet = true; break;
                    case "--help" or "-h": PrintHelp(); return null;
                    default: throw new ArgumentException($"Unknown option {args[i]}");
                }
            }
            catch (Exception ex) when (ex is ArgumentException or FormatException)
            {
                Console.Error.WriteLine(ex.Message);
                PrintHelp();
                return null;
            }
        }

        var unitList = units.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        return new Options(broker, port, user, password, unitList, outDir, log, ping, verbose, quiet);
    }

    private static string? Env(string name) => Environment.GetEnvironmentVariable(name) is { Length: > 0 } v ? v : null;

    private static void PrintHelp() => Console.WriteLine("""
        AcMonitor — record PanasonicAC MQTT data to files

        Options:
          -b, --broker <host>     MQTT broker            (env AC_MQTT_BROKER,   default 192.168.1.120)
              --port <n>          MQTT port              (env AC_MQTT_PORT,     default 1883)
          -u, --user <name>       MQTT user              (env AC_MQTT_USER,     default from firmware)
          -p, --password <pw>     MQTT password          (env AC_MQTT_PASSWORD, default from firmware)
              --units <list>      Units to record, comma separated (default A,B,C)
          -o, --out <dir>         Output directory (default ./ac-data)
              --log               Also record <unit>/log/ messages (incl. raw hex packets) to JSONL
              --ping              With --log, also record the 3 s "Ping" heartbeat messages
          -v, --verbose           Print log messages to the console
          -q, --quiet             Don't print state updates to the console

        Output: <out>/<unit>/state_<date>.csv and <out>/<unit>/log_<date>.jsonl, one file per day.
        """);
}
