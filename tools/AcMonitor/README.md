# AcMonitor

A .NET 10 console app that records data from the PanasonicAC units by listening to their MQTT traffic. It does not send anything to the units.

- `PanasonicAC_<unit>/out/` carries the state JSON. Each message becomes one row in `<out>/<unit>/state_<yyyy-MM-dd>.csv`.
- `PanasonicAC_<unit>/log/` carries the firmware's debug log. With `--log`, it is saved to `<out>/<unit>/log_<yyyy-MM-dd>.jsonl`. The log includes the raw hex of every poll response, which is useful for protocol research (see `docs/cn-cnt-protocol.md`). The 3-second `Ping` heartbeat is skipped unless you add `--ping`.
- With `--raw`, the AC serial frames are saved to `<out>/<unit>/raw_<yyyy-MM-dd>.txt` as text, one frame per line. Both directions are included. Frames received from the AC come from the hex dumps in the log. Frames sent to the AC come from the `Command full:` log lines. Frames whose length byte does not match their size are skipped.

  Each line holds the local timestamp, the direction (`RX` = from the AC, `TX` = to the AC) and the frame bytes in hex:
  ```
  2026-11-18T22:24:14.829+01:00 TX 70 0A 00 00 00 00 00 00 00 00 00 00 86
  2026-11-18T22:24:15.812+01:00 RX 70 20 30 2A 80 A0 F0 00 00 40 00 00 00 2A ...
  ```

  The firmware publishes only the received frames that pass its validity check. It does not publish rejected frames.

The app reconnects automatically. Stop it with Ctrl+C.

## Usage

```
dotnet run --project tools/AcMonitor -- --log --raw
dotnet run --project tools/AcMonitor -- --units B --out D:\ac-data --log -v
dotnet run --project tools/AcMonitor -- --help
```

The broker settings default to the values in `src/main.h`. To override them, set the environment variables `AC_MQTT_BROKER`, `AC_MQTT_PORT`, `AC_MQTT_USER` and `AC_MQTT_PASSWORD`, or pass the matching command-line options.

To build a standalone executable: `dotnet publish tools/AcMonitor -c Release -o <dir>`.
