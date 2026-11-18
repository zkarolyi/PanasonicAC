# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ESP32 (Arduino framework, PlatformIO) firmware that bridges a Panasonic air conditioner's wired UART port to MQTT (consumed by openHAB). Several identical units are deployed; each build targets one unit via the `VERSION` macro in `src/main.h` (`A` = living room, `B` = mid floor, `C` = top floor). `VERSION` drives the MQTT client ID, topic prefix (`PanasonicAC_<VERSION>`), WiFi hostname and OTA hostname.

## Commands

PlatformIO CLI (`pio`), single env `esp32doit-devkit-v1`:

- Build: `pio run`
- Upload: `pio run -t upload` — uploads **over the air** (`upload_protocol = espota`, `upload_port = PanasonicAC_B`). When changing `VERSION`, also change `upload_port` in `platformio.ini` to match, or you will flash the wrong unit. For a first flash over USB, temporarily remove/override the `espota` settings.
- Serial monitor: `pio device monitor` (115200 baud)

There are no tests (`test/` only holds the PlatformIO template README) and no linter.

`tools/AcMonitor` is a .NET 10 console app that only listens to the units' MQTT traffic. It writes daily per-unit files: state CSV, debug log JSONL (`--log`), and raw serial frames as hex text lines (`--raw`, rebuilt from the firmware's hex log messages). Run it with `dotnet run --project tools/AcMonitor -- --log --raw`; see its README for details. Use `--raw` captures to check protocol changes against real units.

## Architecture

All logic is in `src/main.cpp`; `src/main.h` holds configuration constants, globals and forward declarations (it is included only once, so defining globals there is intentional). `src/airconditioner.h` is a header-only `AirConditioner` state class.

**AC serial protocol** (Serial2, 9600 8E1, RX=16/TX=17, 5V TTL — see `protocol_description_query.xlsx` and `docs/cn-cnt-protocol.md` for the full byte map, known gaps and sources):
- Frame = `header, length, payload..., checksum`, where checksum makes the byte sum of the whole frame 0 mod 256. Headers: `0xF0` control, `0x70` poll.
- `handle_poll()` sends a 10-byte zero poll frame when nothing has been sent for `POLL_INTERVAL` (10 s).
- Responses are read with `Serial2.readString()` using the UART read timeout (`READ_TIMEOUT`) as the frame delimiter, validated by `verify_packet()`, then parsed by `AirConditioner::loadFromPacket()` (fixed byte offsets: mode=2, target temp×2=3, fan=5, swing=6, preset=7, inside temp=18, outside temp=19 signed; `0x80` = invalid reading).
- Commands are built by `AirConditioner::getData()`: it copies the last received 10-byte payload (`data_`) and overwrites mode/temp/fan/swing/preset. Therefore **no command is sent until at least one valid response has been received** (`ACReady()`).
- Enum values in `AirConditioner` are the raw protocol bytes; string maps there define the accepted MQTT payload values (e.g. mode `Auto|Cool|Heat|Dry|Off`, fan `Auto|1..5`, swing `Auto|Up|UpCenter|Center|DownCenter|Down`, preset `Normal|Powerful|Quiet`).
- Known decoding gaps, not yet fixed: mode and power share byte 2 (high nibble = mode, low nibble = power), so `Off = 0x30` is really "Cool, power off". The low nibble of the swing byte is horizontal swing. The preset byte also carries the nanoe-X and Econavi bits. See `docs/cn-cnt-protocol.md` §5 before changing `AirConditioner`.

**MQTT** (PubSubClient, broker/credentials hardcoded in `main.h`):
- Subscribes `PanasonicAC_<V>/set/#`; handlers for `set/temperature/`, `set/mode/`, `set/fanspeed/`, `set/swing/`, `set/preset/` (note the trailing slash — topics are compared by exact string). Each sets AC state and immediately sends a control frame.
- Publishes AC state JSON (`toJsonStringStructured()`) to `.../out/` and verbose debug/hex logs to `.../log/`.
- Reconnect is non-blocking with a 5 s back-off (`mqttReconnect()`).

**WiFi provisioning**: credentials are stored in SPIFFS `/wifi.txt`. If missing or connection fails, the device starts AP `ESP32-AP` with a form at `/` posting to `/connect`; AP mode reboots after 10 min. `/dump` shows current AC state.

**Reliability (watchdogs)** — all in `timer()`, called every `loop()`:
- Hardware task WDT (`wdtTimeOut` = 60 s) is fed only from the 3 s timer tick (plus during MQTT reconnect and OTA progress). Avoid adding long blocking code to `loop()`.
- Functional MQTT watchdog: if WiFi is connected but no successful MQTT publish/receive for 5 min (`mqttLastActivity`), the ESP restarts. The 3 s "Ping" log publish in `flashLed()` doubles as the heartbeat that keeps this alive.

Code comments are partly in Hungarian.
