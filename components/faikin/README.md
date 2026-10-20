# faikin

Native ESPHome component for Daikin air conditioners, talking to the indoor unit over its **S21** serial port. No cloud, no MQTT broker: the unit appears in Home Assistant through the ESPHome API.

It is a port of the protocol work done in [ESP32-Faikin / ESP32-Faikout](https://github.com/revk/ESP32-Faikin) (reverse engineered S21 protocol, see its [S21 protocol wiki](https://zff.dev/RevK/ESP32-Faikout/wiki/S21-Protocol)). Everything that firmware does besides the protocol (web page, MQTT, Wi-Fi setup, OTA) is already provided by ESPHome.

## Status

Phase 1 covers **S21 only**.

| Protocol | Status |
|---|---|
| S21 (most split units) | supported |
| X50A (ducted units) | not yet |
| CN_WIRED | not yet (a separate ESPHome component for CN_WIRED exists) |

This is new code that has been unit-tested on a host (protocol layer) but not yet run against a real unit. Expect to adjust things on first use, and use the boot probe described below.

## Hardware

- The S21 port is 2400 baud, 8 data bits, even parity, 2 stop bits.
- Community reports say the lines need 5 V logic levels. Use an interface designed for it (for example a Faikin board) or a proper level shifter, not bare 3.3 V GPIOs.
- Some interface boards invert the Tx and/or Rx line. The original firmware detects this automatically; here you set `inverted: true` on the corresponding uart pin.
- The S403 connector is not isolated. It should only be used through a Daikin S21 adapter.
- Working on an air conditioner can be dangerous: isolate the power first, and do it at your own risk.

## Configuration

```yaml
external_components:
  - source:
      type: local
      path: components

uart:
  id: s21_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 2400
  data_bits: 8
  parity: EVEN
  stop_bits: 2

faikin:
  id: ac
  uart_id: s21_uart
```

The uart settings above are checked at validation time.

| Option | Default | Description |
|---|---|---|
| `update_interval` | `1s` | Minimum time between two polling cycles (minimum 200 ms). |
| `response_timeout` | `500ms` | How long to wait for the ACK and the response of one command. |
| `command_gap` | `25ms` | Pause between two commands. |
| `probe` | `true` | Run the boot probe once after the first connection. |

See [`test_faikin.yaml`](../../code/test_faikin.yaml) for a complete example. Every platform is configured with `platform: faikin`; each entity key is optional.

| Platform | Keys |
|---|---|
| `climate` | (single entity) power, mode, target temperature 18-30 °C in 0.5 °C steps, fan, swing, room temperature, action |
| `sensor` | `home_temperature`, `outside_temperature`, `liquid_temperature`, `fan_speed`, `compressor` |
| `binary_sensor` | `connected` |
| `text_sensor` | `protocol_version`, `model_code` |
| `switch` | `econo`, `powerful`, `comfort`, `quiet`, `streamer`, `sensor` |
| `number` | `demand` |

Notes:

- Modes: off, heat/cool (the unit's auto), cool, heat, dry, fan only.
- Fan: `auto`, `quiet`, plus custom modes `1` to `5`.
- `quiet` is the outdoor unit quiet mode, not the quiet fan mode of the climate entity.
- Switches and the demand number only appear to work on units that answer the corresponding status command (`F6` for the switches except `econo`, `F7` for `econo` and demand). Requests for a command the unit refused are ignored with a warning.
- The climate action is inferred: a non-zero compressor value means the compressor is running, and in auto mode the heating or cooling direction is guessed from the target and room temperatures.
- The `compressor` sensor is the raw value of the `Rd` command; its unit is not confirmed.
- `liquid_temperature` comes from the `RI` command, whose exact meaning is not fully confirmed.

## Demand control (power on demand)

Many recent units can limit their conditioning power to a percentage, sent with the `D7` command: byte 0 is `0x30 + (100 - percent)`, so 100 % (no limit) is `'0'`. The same frame carries the econo flag, which is why this component writes both together: writing econo alone would reset the limit to 100 %.

What is and is not known:

- It is a ceiling on the power drawn, not a setpoint: below the limit the unit modulates freely.
- The relation between the percentage and the power in watts is not linear, and the limit has no effect during defrost.
- Community measurements on a Perfera FTXM20A report a manual range of 40 % to 95 % plus 100 %. A **step of 5 % is not confirmed** by the protocol documentation.
- The value is not synchronised with the Daikin cloud, and may be forgotten after a power cut of the outdoor unit.

The `number` therefore takes configurable `min_value`, `max_value` and `step` (defaults 40, 100, 5). Check what the unit reads back after each write, and tune the step to what it accepts.

## Boot probe and debugging

After the first successful connection, the component queries a few identification and capability commands once and logs the raw answers at INFO level: `F8` and `FY00` (protocol version), `FC` (model code), `FK` (optional features), `FU05` (model name on recent units), and the raw `F7` and `F6` frames. Unsupported commands are logged as refused. The `protocol_version` and `model_code` text sensors are filled from this.

Two actions are available for exploration:

```yaml
- faikin.probe:
    id: ac
- faikin.send_raw:
    id: ac
    command: "F7"      # exactly two characters
    payload: ""        # optional, ASCII only
```

`faikin.send_raw` logs the answer at INFO level. Be careful with `D` commands: they change the settings of the unit, and a wrong payload can have unexpected effects.

## How it works

The unit never talks on its own. Each request is `STX c1 c2 [payload] checksum ETX`; the unit answers `ACK` (or `NAK` when it does not know the command) and, for queries, then sends `STX c1+1 c2 [payload] checksum ETX`, which is acknowledged in turn. Writes (`D1` power, mode, temperature and fan; `D5` swing; `D6` special modes; `D7` econo and demand) are acknowledged only.

- A non-blocking state machine runs in `loop()`: one transaction at a time, with timeouts. Nothing ever waits on the UART.
- Writes are sent before polls. After an accepted write, the new value is shown immediately and re-read from the unit, so a change the unit ignores is reverted.
- Each polling command has a refusal counter: a command the unit refuses is dropped from the cycle. `F9` (coarse temperatures) is only used when `RH` / `Ra` are refused.
- Three consecutive failures mark the unit offline; it is then asked for its basic status every 2 seconds until it answers.
- A checksum that would equal `02`, `03`, `06` or `15` (hex) is sent as the value plus 2, as documented by the community. The original firmware only handled `03`.
- A response that is our own request means the Tx line is looped back on Rx: check the wiring and the pin inversion.

Not carried over from the original firmware: the automatic protocol detection (configure the uart instead), the thermostat-like automation with an external reference temperature (use ESPHome's own `thermostat` or automations), and the legacy `F3` / `D3` handling for units older than protocol version 0.

## Development

The protocol layer (`s21_protocol.h`) has no ESPHome dependency and is tested on the host:

```sh
cd tests
g++ -std=c++17 -Wall -Wextra -I.. test_s21_protocol.cpp -o test_s21 && ./test_s21
```

## License

MIT License

Copyright (c) 2026 e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
