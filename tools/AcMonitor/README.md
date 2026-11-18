# AcMonitor

A .NET 10 console app that records data from the PanasonicAC units by listening to their MQTT traffic. It does not send anything to the units.

- `PanasonicAC_<unit>/out/` carries the state JSON. Each message becomes one row in `<out>/<unit>/state_<yyyy-MM-dd>.csv`.
- `PanasonicAC_<unit>/log/` carries the firmware's debug log. With `--log`, it is saved to `<out>/<unit>/log_<yyyy-MM-dd>.jsonl`. The log includes the raw hex of every poll response, which is useful for protocol research (see `docs/cn-cnt-protocol.md`). The 3-second `Ping` heartbeat is skipped unless you add `--ping`.

- With `--raw`, the AC serial frames are saved as binary to `<out>/<unit>/raw_<yyyy-MM-dd>.bin`. Both directions are included. Frames received from the AC come from the hex dumps in the log. Frames sent to the AC come from the `Command full:` log lines. Frames whose length byte does not match their size are skipped.

  Each record has this layout (integers little-endian):

  | Bytes | Field |
  |---|---|
  | 8 | `int64` timestamp, Unix ms (UTC) |
  | 1 | direction: `0` = from AC, `1` = to AC |
  | 2 | `uint16` frame length *n* |
  | *n* | frame: header, length, payload, checksum |

  Python reader:
  ```python
  import struct
  d = open("raw_2026-11-18.bin", "rb").read(); i = 0
  while i < len(d):
      ts, direction, n = struct.unpack_from("<qBH", d, i)
      frame = d[i + 11 : i + 11 + n]; i += 11 + n
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
