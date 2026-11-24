# hms_fwupdate

Firmware update of **Hoymiles HMS** micro-inverters (CMT2300A radio) from ESPHome.

The component downloads an Intel HEX firmware file over HTTP, verifies it, and sends it row by row to the
inverter through the existing [`hms`](../hms) hub. An HM (nRF24L01) variant is planned separately.

> **WARNING: EXPERIMENTAL, UNTESTED ON REAL HARDWARE.**
> A failed or interrupted update can brick the inverter (no A/B firmware, only a CRC protection).
> `dry_run` is **on by default**: the file is downloaded and verified, the first frames are logged,
> and nothing is transmitted. Use it first. Flash only an inverter you can afford to lose, at your own risk.

## Status

| Item | State |
|---|---|
| Frame layout, Intel HEX decoding, CRC16, packetization | Host-tested against an independent Python re-implementation (`tests/hms_fwupdate/run.sh`) |
| ESPHome config validation (`esphome config`) | OK |
| Full firmware compile | Not yet confirmed |
| Real inverter | **Never tested** |
| Bootloader entry (the first non-data rows) | Not identified: confirm by sniffing the reference implementation |
| Acknowledgement format on the CMT path | Accepts any `0x8E` frame whose payload starts with the row header; to be confirmed |

## How it works

1. **Scan pass.** The whole file is streamed once (never stored). The component computes its SHA-256
   (logged, and compared if you pinned one), checks every Intel HEX checksum and length, checks that the
   identity row matches the inverter model (serial-prefix rules table), counts rows and packets and detects truncation.
2. **Dry run** stops here after logging the frames of the first 3 rows and of the end-of-file row.
3. **Transmit pass.** The file is downloaded a second time, the hub enters a maintenance mode (polling stops,
   the frequency stays locked), and every row is split into 16-byte chunks sent as
   `0x0E | inverter id | DTU id | packet number | payload | [CRC16] | XOR`.
   Only the last packet of a row (flag `0x80`) is acknowledged; it is re-sent on a missing ack, then the whole row.
   Timings: 30 ms between packets, 350 ms ack timeout, up to `max_row_attempts` attempts per row.

Safeguards that the reference implementation does not have: two-pass download, SHA-256 pin, strict Intel HEX
checks, link-quality gate (measured from the telemetry poll history, since the CMT has no hardware ACK),
an `armed` switch that must be turned on right before `start`, and `dry_run` on by default.

## Configuration

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [ cmt2300a, hms, hms_fwupdate ]

http_request:
  buffer_size_rx: 2048

hms_fwupdate:
  id: fw
  hms_id: hms1
  dry_run: true
  firmwares:
    - name: HMS_2T 1.0.8
      url: http://homeassistant.local:8123/local/hoymiles/HMS_2T_1_0_8.hex
      sha256: "<64 hex digits>"
```

| Option | Default | Description |
|---|---|---|
| `hms_id` | required | The `hms` hub of the inverter to update |
| `firmwares` | required | List of `name`, `url`, optional `sha256` |
| `min_link_quality` | `90%` | Refuse to start below this share of successful polls |
| `link_quality_window` | `20` | Number of polls considered (5 to 32) |
| `require_armed` | `true` | `start` needs the armed switch; it turns itself off after use |
| `armed_timeout` | `60s` | The armed state expires after this delay |
| `dry_run` | `true` | Verify and log only, transmit nothing |
| `max_row_attempts` | `10` | Whole-row re-sends before giving up |
| `strict_hex` | `true` | Verify Intel HEX checksums and lengths |
| `on_progress` | | Automation, `x` is the progress in % |
| `on_result` | | Automation, `x` is the outcome text |

Actions: `hms_fwupdate.start`, `hms_fwupdate.abort`, `hms_fwupdate.arm`.

Platforms (each takes `hms_fwupdate_id`): `select` (`firmware`), `switch` (`armed`), `button` (`start`, `abort`),
`sensor` (`progress`, `link_quality`), `text_sensor` (`state`, `result`), `binary_sensor` (`running`).
See `code/test_hms_fwupdate.yaml` for a complete example.

## Changes in the `hms` hub

A small maintenance API was added to `hms.h`/`hms.cpp`: `fw_enter`, `fw_leave`, `fw_send_frame`, an RX callback,
and a poll-history based `get_link_quality`. Normal operation is unchanged.

## Firmware files and legal notes

- The firmware files are **not** included and not redistributable by this repository. Get them yourself and
  check their provenance; use the SHA-256 pin.
- The protocol logic in `fwupdate_core.h` is derived from the OpenDTU fork
  `xX-nichtlachen-Xx/OpenDTU` (branch `FirmwareUpdate_HM_IV`), licensed **GPL-2.0-or-later**. Those parts keep
  that license; the rest of the component is MIT. Check the compatibility before redistributing.
- Hoymiles firmware with encrypted radio may cut OpenDTU-style access, whatever this component does.

## License

MIT License

Copyright (c) e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of
the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
