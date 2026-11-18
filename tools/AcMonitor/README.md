# AcMonitor

A .NET 10 console app that records data from the PanasonicAC units by listening to their MQTT traffic. It does not send anything to the units.

- `PanasonicAC_<unit>/out/` carries the state JSON. Each message becomes one row in `<out>/<unit>/state_<yyyy-MM-dd>.csv`.
- `PanasonicAC_<unit>/log/` carries the firmware's debug log. With `--log`, it is saved to `<out>/<unit>/log_<yyyy-MM-dd>.jsonl`. The log includes the raw hex of every poll response, which is useful for protocol research (see `docs/cn-cnt-protocol.md`). The 3-second `Ping` heartbeat is skipped unless you add `--ping`.

The app reconnects automatically. Stop it with Ctrl+C.

## Usage

```
dotnet run --project tools/AcMonitor -- --log
dotnet run --project tools/AcMonitor -- --units B --out D:\ac-data --log -v
dotnet run --project tools/AcMonitor -- --help
```

The broker settings default to the values in `src/main.h`. To override them, set the environment variables `AC_MQTT_BROKER`, `AC_MQTT_PORT`, `AC_MQTT_USER` and `AC_MQTT_PASSWORD`, or pass the matching command-line options.

To build a standalone executable: `dotnet publish tools/AcMonitor -c Release -o <dir>`.
