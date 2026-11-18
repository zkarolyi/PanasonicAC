# Panasonic CN-CNT serial protocol — research notes

These notes collect public reverse-engineering sources on the Panasonic CN-CNT connector, the wired port used by the CZ-TACG1 WiFi adapter. They also compare the sources with this firmware.

> **Reliability:** This was compiled on 2026-11-18 from web research. The tool used returned page summaries, not the verbatim source files. Check every bit-level detail against the raw sources listed below before relying on it. Items marked *(uncertain)* come from one source or one observation.

**Indexing:** index 0 is the header, 1 is the length and 2 onwards is the payload, the same as `src/airconditioner.h`. Control-payload index *p* corresponds to poll-response index *p+2*.

## Sources

| ID | Source | Notes |
|---|---|---|
| S1 | DomiStyle / esphome-panasonic-ac — https://github.com/DomiStyle/esphome-panasonic-ac | The most widely used implementation. Relevant files are in `components/panasonic_ac/`: `esppac_cnt.cpp`, `esppac_cnt.h`, `esppac_commands_cnt.h` and `esppac.h`. The `protocol/` folder contains `protocol_description_controller.ods`, `protocol_description_ir.ods`, `cztacg1/` and `logic_analyzer/`, none of which has been reviewed yet. |
| S2 | ssjoholm / panasonic-cn-cnt — https://github.com/ssjoholm/panasonic-cn-cnt/blob/main/Panasonic-CN-CNT-Protocol-v1.md | A detailed byte map. It was tested on a CS-HZ35XKE (nanoe-X, no Econavi), mostly in heat mode. |
| S3 | akashihi / P11 — https://github.com/akashihi/P11 | An ESPHome board for CN-CNT. It confirms the physical layer and adds no new byte meanings. |
| S4 | DomiStyle issue #124 — https://github.com/DomiStyle/esphome-panasonic-ac/issues/124 | Econavi, nanoe-X and mild-dry switches do nothing on some units. Only the search snippet was read. |
| S5 | HA forum: PACi 2-wire wired-controller bus — https://community.home-assistant.io/t/panasonic-paci-commercial-unit-wired-controller-protocol-2-wire-bus-anyone-working-on-this/996275 | This is a **different** protocol (about 495 baud). It is listed only so it is not confused with CN-CNT. |
| — | leon-v / Panasonic-CN-CNT-ESPHome — https://github.com/leon-v/Panasonic-CN-CNT-ESPHome | Not reviewed yet; suggested follow-up. |
| — | `protocol_description_query.xlsx` (this repo) | The original local protocol notes. |

## 1. Physical layer and framing

- UART at 9600 baud, 8 data bits, even parity, 1 stop bit, 5 V TTL [S1, S2, S3].
- Frame: `[header][length = payload byte count][payload][checksum]`. The checksum is `(256 - sum(previous bytes)) & 0xFF`, so the whole frame sums to 0 mod 256 [S1, S2].
- Headers:
  - `0x70` is the poll request and the poll response.
  - `0xF0` is a control command.
- Poll request: `70 0A 00 00 00 00 00 00 00 00 00 00 86`, 13 bytes [S2].
- Poll response: 35 bytes with `byte[1] = 0x20`, a 32-byte payload [S2].
- Control command: 13 bytes with `byte[1] = 0x0A` [S2].
- S1 accepts a frame only if:
  - its size is at least 12,
  - the header is `0x70` or `0xF0`,
  - `byte[1] == size - 3`,
  - and the sum mod 256 is 0.
- *(uncertain)* S2 once saw `byte[1] = 0x22`, which shifts the offsets. Check `byte[1] == 0x20` before trusting any positions.

## 2. Poll response byte map (35 bytes)

| Idx | Meaning | Source |
|---|---|---|
| 0 | Header, `0x70` | S1, S2 |
| 1 | Payload length, `0x20` | S2 |
| 2 | **Mode in the high nibble, power in the low nibble.** Mode: 0 Auto, 2 Dry, 3 Cool, 4 Heat, 6 Fan only. Power: low nibble 0 means off (S2 sees 4 when on). Examples: `0x44` Heat on, `0x34` Cool on, `0x30` Cool off. | S1, S2 |
| 3 | Target temperature, raw × 0.5 °C, range 16–30 °C. S1 applies an offset and step. | S1, S2 |
| 4 | Mild dry: `0x7F` on, `0x80` off | S1, S2 |
| 5 | Fan: `0xA0` Auto, `0x30`–`0x70` levels 1–5 | S1, S2 |
| 6 | **Swing.** High nibble is vertical: F auto, E swing, 1 up, 2 up-center, 3 center, 4 down-center, 5 down, 0 not supported. Low nibble is horizontal: D auto, 9 left, A left-center, 6 center, B right-center, C right, 0 not supported. | S1, S2 |
| 7 | **Preset and extras.** Bits 0–2 are the preset: `0x00` normal, `0x02` powerful, `0x04` quiet. Bit 6 (`0x40`) is nanoe-X. Bit 4 (`0x10`) is Econavi. | S1, S2 |
| 8 | Reserved, static | S2 |
| 9 | Unknown flag, observed `0x40` | S2 |
| 10 | Eco: `0x40` on, `0x00` off | S1, S2 |
| 11 | Reserved | S2 |
| 12 | *(uncertain)* Operating state. `0x00` off, `0x04` powering down, `0x08` fan only. Cool: `0x30` idle, `0x34` transition, `0x38` starting, `0x3C` running. Heat: `0x40` idle, `0x44` shutdown, `0x48` starting, `0x4C` running. Proposed bits: 6 heat, 5+4 cool, 3 fan, 2 compressor. | S2 only |
| 13 | Internal set temperature: the target plus about +2 to +6 raw | S2 |
| 14 | Defrost: `0x02` while defrosting, about 7–10 minutes per cycle | S1, S2 |
| 15 | Reserved | S2 |
| 16–17 | *(uncertain)* Model-specific. XKE: `00 00`, ZKE: `30 93`. | S2 |
| 18 | Indoor room temperature, °C. `0x80` means not supported; S1 then falls back to byte 21. | S1, S2 |
| 19 | Outdoor temperature, signed int8. `0x80` means not supported; S1 then falls back to byte 22. | S1, S2 |
| 20 | *(uncertain)* Humidity %, only on units with a sensor | S2 |
| 21 | Indoor coil/pipe temperature (S1 uses it as the room-temperature fallback) | S1, S2 |
| 22 | Outdoor temperature, alternate reading | S1, S2 |
| 23, 26 | `0xFF` markers | S2 |
| 24, 25, 27 | `0x80`, not supported | S2 |
| 28–29 | *(sources disagree)* Power, low byte then high byte. S2: W ≈ (b28 + 256·b29) × 1.10, outdoor unit only. S1: b28 + 256·b29 − b30. | S1, S2 |
| 30 | Compressor current × 5, so A = b30 / 5 | S2 (S1 subtracts it from the power value) |
| 31–33 | *(uncertain)* Multiplexed identifiers. b31 = `0x80` gives unit/capacity. b31 = `0xC0` gives nanoe-X status (b32 = `0x04` when on). b31 = `0xC1` gives the series. | S2 |
| 34 | Checksum | S1, S2 |

No source documents any error codes for CN-CNT.

## 3. Control command (`F0 0A` + 10-byte payload + checksum)

| Payload idx (frame idx) | Meaning |
|---|---|
| 0 (2) | Mode and power: Cool `0x34`, Heat `0x44`, Dry `0x24`, Auto `0x04`, Fan only `0x64`. For off, S1 clears the low nibble, but S2 verified an all-zero mode byte. |
| 1 (3) | Target temperature × 2 |
| 2 (4) | Mild dry: `0x7F` on, `0x80` off |
| 3 (5) | Fan, same values as the response |
| 4 (6) | Swing, same nibbles as the response. S1 presets: both `0xFD`, off `0x36`, vertical only `0xF6`, horizontal only `0x3D`. |
| 5 (7) | Preset and extras, same bits as the response. S1 writes the whole byte for Econavi, which clears the other bits. |
| 6, 7, 9 | `0x00` [S2] |
| 8 (10) | Eco: `0x40` / `0x00` |

Frames verified in S2:

```
F0 0A 44 29 80 30 5C 00 00 00 00 00 8D   Heat on, 20.5 °C
F0 0A 44 28 80 30 5C 00 00 00 00 00 8E   Heat on, 20.0 °C
F0 0A 00 00 00 00 00 00 00 00 00 00 06   Off
```

S2 builds a command like this:

1. Poll the unit.
2. Copy response bytes 2–7 and 10 into payload bytes 0–5 and 8.
3. Change the wanted fields and zero the rest.
4. Compute the checksum.

S1 resets the preset to Normal whenever the fan speed changes. Powerful and quiet appear together with fan `0xA0` [S2].

S1 and S2 disagree on payload bytes 6, 7 and 9. S1 copies the whole response, while S2 zeroes them. S2's statement is the more explicit of the two.

## 4. Protocol behaviour

- **Polling:** S1 polls every 5000 ms, and S2 agrees.
- **Command spacing:** S1 leaves at least 250 ms between commands. Changes made within that window are merged into one pending command.
- **Frame end:** S1 treats 20 ms with no bytes as the end of a frame and does not parse the length byte.
- **Handshake:** there is none. The client is ready after the first valid `0x70` response.
- **Acknowledgement:** none is documented. S1 waits for any valid frame after sending, ignores valid `0xF0` frames and parses only `0x70` frames.
- **Unsolicited frames:** none are described. The protocol appears to be strictly poll and response.
- **IR remote:** nothing is documented. Polls show changes made with the remote. Building each command from the latest poll response avoids overwriting them. This is an inference, not a sourced statement.
- **Feature support:** it varies by model. A `0x0` nibble or a `0x80` value means not supported [S2, S4].

## 5. Differences from this firmware (`src/airconditioner.h`, `src/main.cpp`)

1. **Mode byte:** the firmware treats it as one enum value. It should be split into mode (high nibble) and power (low nibble, non-zero means on). The current `Off = 0x30` is really "Cool, power off", so any other mode while off decodes to an invalid enum value.
2. **Swing byte:** the firmware treats it as vertical swing only. It sends values such as `0xF0`, which sets horizontal to 0, meaning "not supported". It should keep the low nibble from the last response.
3. **Preset byte:** the whole byte is cast to `Preset`, so nanoe-X or Econavi produce invalid values, and the next command clears those flags. The preset should be read with `& 0x07` and the upper bits kept.
4. **`0x80` temperatures:** the firmware keeps the old value. S1 falls back to bytes 21 and 22, but byte 21 may be the coil temperature (S2).
5. **Validation:** the firmware should also check `byte[1] == 0x20` and a total length of 35.
6. **Payload bytes 6, 7 and 9:** the firmware copies them from the response, as S1 does. S2 zeroes them.
7. **Timing:** the firmware polls every 10 s, where S1 polls every 5 s. It sends a command for every MQTT message, with no 250 ms spacing or merging.
8. **Features not yet supported:** fan only, mild dry, eco, nanoe-X, Econavi, horizontal swing, defrost status, humidity, power and current, and operating state.

## 6. Observations from our own units

These frames were captured on 2026-11-18 with `tools/AcMonitor --raw`. All of them passed the checksum.

```
B  Heat on, 21 °C, quiet, running:
70 20 44 2A 80 A0 30 04 00 40 00 00 4C 2C 00 00 00 00 17 0A FF 18 0A FF 80 80 FF 80 8B 01 0A 80 85 65 36
A  Cool, power off (mode byte 0x30):
70 20 30 28 80 A0 F0 04 00 40 00 00 00 28 00 00 00 00 15 80 FF 15 80 FF 80 80 FF 80 01 00 00 C0 00 00 34
C  Cool, power off, no outdoor reading:
70 20 30 2A 80 A0 F0 00 00 40 00 00 00 2A 00 00 00 00 12 80 FF 12 80 FF 80 80 FF 80 01 00 00 80 85 65 90
```

- All responses are 35 bytes with `byte[1] = 0x20`. We have not seen the `0x22` variant.
- **Byte 12:** unit B reads `0x4C` while heating, which matches S2's "heat, compressor running". The two powered-off units read `0x00`.
- **Byte 13** is the target plus 2 raw units on unit B, which is running. That fits S2's offset of about +2 to +6. On the powered-off units it equals the target.
- **Bytes 18–19 and 21–22:** unit B reads 23 °C / 10 °C at 18–19 and 24 °C / 10 °C at 21–22. Units A and C report `0x80` (no reading) for outdoor temperature at 19 and 22 while powered off. The firmware keeps the last good value, so the outdoor temperature it publishes for those units is stale.
- **Bytes 28–30** change from poll to poll while B is running: `8B 01 0A`, `14 01 08`, `2E 01 09`. With S2's formula that is about 300–435 W and 1.6–2.0 A. The powered-off units read `01 00 00`.
- **Bytes 31–33** cycle through three blocks on every unit: `80 85 65`, `C0 00 00` and `C1 36 14`. This confirms S2's description of them as multiplexed identifiers.
- **Byte 9** is `0x40` on every unit, matching S2's observation.
- **Missed responses:** every unit sometimes fails to answer a poll. The firmware frames responses using only a 20 ms read timeout, and it publishes nothing for frames it rejects. It is not yet clear whether the AC stays silent or the firmware drops the frame.

## Open questions

- Byte 12 beyond the observed heat and cool states, bytes 16–17, and the meaning of the three blocks in bytes 31–33.
- Why some polls get no visible response (see §6).
- The power formula, since S1 and S2 differ.
- Error codes.
- Whether the AC replies to a control frame.
- Review the S1 `protocol/*.ods` files and the leon-v project.
