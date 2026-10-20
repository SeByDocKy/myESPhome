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

## Hardware and wiring

Only the **S21** port is supported in this phase (see the status table). The other Daikin connectors are listed below so you can recognise them, but they use different protocols.

> Working inside an air conditioner can be dangerous (mains voltage, risk of damaging the unit). Isolate the power first, never plug or unplug a lead with the unit powered, and do it at your own risk.

### Which connector does my unit have?

| Port | Protocol | Typical connector on the unit | Supported here |
|---|---|---|---|
| **S21** | S21 serial | 5-way JST EH (2.5 mm pitch, older units, "type A") or JST PAP / PA family (2.0 mm pitch, locking, newer units, "type B") | yes |
| X403 / S403 | S21 serial, **not isolated** | JST XAP ("type D") or JST PBVP-10 / modified PAP ("type E"), modern splits | wiring only through a Daikin S21 adapter, see below |
| X50A (+ X35A for power) | X50A | Molex Micro-Lock or JST ZHR for X50A, XHP for X35A; ducted units and cassettes | not yet |
| CN_WIRED | CN_WIRED | JST PH series | not yet |

Pin numbers for X403, X50A and X35A are only published as pictures by the original project, so they are not repeated here.

**Perfera FTXM (for example FTXM35).** The community reports that these units have an S21 header on the indoor unit main board, 5 pins, 2.0 mm pitch, locking (JST PA family, so the same family as the "type B" lead), often not populated with the mating plug. The stock Wi-Fi adapter (BRP069C4x) uses a different connector, S801, which is not the one to use. Reports of pin 1 differ from one unit to another, so measure before connecting (see below). The official Daikin EKRS21 lead has been reported to fit, but it is short and expensive.

### S21 pinout

The original project wires the S21 port 1:1 (pin 1 to pin 1, and so on). Pin 1 is marked with a triangle or an arrow on the board.

| Pin | Function | Notes |
|---|---|---|
| 1 | 5 V reference | Not always connected, and it is **not** GND. Used by the unit as pull-up for its Tx line. |
| 2 | Tx from the air conditioner | 5 V logic, connect to the ESP32 **Rx** pin |
| 3 | Rx to the air conditioner | 5 V logic (most units also accept 3.3 V), connect to the ESP32 **Tx** pin |
| 4 | Power, nominally 12 V | Measured values between 12 V and 14.5 V are reported |
| 5 | GND | |

Reports for some FTXM boards read pin 1 as GND and pin 5 as 5 V, others the opposite, and wire colours are not reliable (one end of a lead may need its wire order reversed). **Always check with a multimeter** which pin is GND and which pin carries the 12-14 V before connecting anything.

### Do I need a level shifter?

The S21 data lines idle at 5 V. An ESP32 input is 3.3 V and is not rated for 5 V, so the unit Tx line (pin 2) should not go straight to an ESP32 pin.

- **Unit Tx (pin 2) to ESP32 Rx: yes, protect the input.** Either a resistor divider (for example 1 kOhm in series with the pin and 2 kOhm to GND, which gives about 3.3 V from 5 V) or a bidirectional level shifter.
- **ESP32 Tx to unit Rx (pin 3): usually not needed.** Most units are reported to accept 3.3 V on this line. If a unit does not answer, put a level shifter on this line too.
- **Module:** any 4-channel bidirectional I2C-style converter built on BSS138 MOSFETs (for example the Adafruit 757 or the SparkFun BOB-12009, or the many compatible clones) is fast enough for a 2400 baud link. Feed its HV side from the 5 V rail and its LV side from the ESP32 3.3 V.
- The original Faikin board uses one FET per direction (the signal is inverted by the board), and is the reference design: if you do not want to build the interface, a Faikin board is the simplest option. Its inversion is why some boards need `inverted: true` on the uart pins here.

I could not find a Daikin document describing the exact electrical characteristics of the S21 lines, so treat the values above as community practice. If nothing is received, try the pin inversion, then a 5 V pull-up on the unit Tx line (pin 1 is meant for it).

### Wiring diagram

Logical wiring only: the physical position of the pins on the plug is not drawn, check the pin numbers (and the triangle marking pin 1) on your own connector, with a multimeter.

```text
 Daikin S21 port                                         ESP32 (3.3 V logic)
 ---------------                                         -------------------

 pin 4  (12-14.5 V) ------+------ Vin  +-----------+  5 V ------ 5V / VIN
                          |            | Mini560   |
 pin 5  (GND) ------------+------ GND --+ (buck 5V) +-- GND ---- GND
                          |            +-----------+
                          |
                          +-------------------------------- common GND

 pin 2  (unit Tx, 5 V) ---[ 1 kOhm ]---+--------------------- GPIO16  (uart rx_pin)
                                        |
                                     [ 2 kOhm ]
                                        |
                                       GND

 pin 3  (unit Rx) <------------------------------------------ GPIO17  (uart tx_pin)
                          (3.3 V is accepted by most units)

 pin 1  (5 V reference, not always connected): leave open at first
```

Variant with a BSS138 bidirectional level shifter instead of the divider:

```text
 S21 pin 1 or Mini560 5 V ---- HV          LV ---- ESP32 3V3
 S21 pin 5 (GND) ------------- GND         GND --- ESP32 GND
 S21 pin 2 (unit Tx) --------- HV1         LV1 --- GPIO16 (rx_pin)
 S21 pin 3 (unit Rx) --------- HV2         LV2 --- GPIO17 (tx_pin)
```

If nothing is received, first swap the roles of pins 2 and 3 if you wired them the other way round, then try `inverted: true` on the uart pins.

### Power supply

The ESP32 can be powered from the unit through pins 4 and 5: use a small step-down converter (12-14.5 V to 5 V, then the board regulator, or directly 3.3 V), for example a Mini560 5 V module or an equivalent buck converter rated for at least 20 V input. Check the output with a multimeter before connecting the ESP32; one user reports a Mini560 that did not deliver 5 V from these pins, so do not skip that test. Some units do not provide the 12 V on pin 4: on those the 5 V of pin 1 may be usable to power the ESP32 (very small current, an ESP32 with Wi-Fi may be too much for it), otherwise power it separately and share only the GND.

### Isolation

- **S21:** the original project only warns about the non-isolated X403 variant, which suggests the S21 port is isolated from the mains side, but I found no Daikin statement confirming it. No isolator is required by the community builds; if you want one anyway, use a digital isolator or optocouplers on the two data lines and an isolated DC-DC converter for the supply, and remember the 5 V levels.
- **X403 / S403:** this port is **not isolated** and can be at mains potential. The original project recommends going through a Daikin S21 adapter board and keeping everything inside the unit case. Do not connect an ESP32 directly to it.

### Connector parts (S21)

| Part | Reference |
|---|---|
| Female housing, type A (EH) | JST EHR-5, crimp SEH-001T-P0.6 |
| Female housing, type B (PAP) | JST PAP-05V-S, crimp SPHD-001T-P0.5 |
| Ready-made leads | EH leads (ASEHSEH22K305 12", ASEHSEH22K152 6"), PAP leads (APAPA22K305 12", APAPA22K152 6"), Daikin EKRS21 (PAP to EH adapter lead) |

JST crimping tools are expensive; pre-crimped leads are easier. Jumper wires and header pins work for a bench test but do not hold in a unit.

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
