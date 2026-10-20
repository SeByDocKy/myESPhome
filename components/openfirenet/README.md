# openfirenet — ESPHome component

Native ESPHome component that makes an **ESP32-S3** behave like the **RIKA Firenet 2.0 USB dongle**
(also sold under the ANIMO brand), so a pellet stove can be monitored and controlled from Home Assistant
without the manufacturer cloud.

The stove is the USB **host**, the ESP32-S3 is the USB **device** (CDC ACM). The wire protocol was
documented by the [open-firenet](https://github.com/openfirenet/open-firenet) project; this component is an
independent implementation written from that specification (no code copied).

> **Status: experimental.** The protocol layer is unit tested on a PC, the YAML schema is validated and the
> component compiles, but it has **not been validated against a real stove yet**. See *Known uncertainties*.

## Compatible stoves

The `model` text sensor decodes the model identifier reported by the stove. The protocol tables of the
[open-firenet](https://github.com/openfirenet/open-firenet) project list two brands, both from the same
manufacturer (Rika Innovative Ofentechnik):

* **RIKA** — INDUO, TOPO, ROCO, ROCO MULTIAIR, ROCO RAO, KAPO, MIRO, COMO, REVO, INTERNO, FILO, SUMO, DOMO,
  CORSO, INDUO II, REVIVO, PARO, LIVO, COMO II, REVO II, COSMO, SONO, DOMO BACK, PK E, SUMO MULTIAIR, CONNECT.
* **ANIMO** — AVITO, AVITO SLIM, AVITO RAO, ADEVO, PURE, ADUO, AMITO, ARONDO, ADUO 2.

Being listed in the protocol tables does **not** mean a model has been tested: the open-firenet project
itself names no tested models, and this component has not been validated on a real stove yet. A stove needs
the Firenet 2.0 USB-A socket; older models or other USB variants may not work. Reports are welcome.

## Requirements

* ESP32-S3 board with the native USB port wired to the stove's USB-A socket (device mode, no host).
* ESP-IDF framework, ESPHome ≥ 2026.9 (`tinyusb` + `usb_cdc_acm` components).
* `logger` on **UART0**: the native USB port belongs to TinyUSB and cannot carry logs.
* Flash the first time over UART, then use OTA over WiFi.
* `tinyusb` cannot be combined with `usb_host`.

## Minimal configuration

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

logger:
  hardware_uart: UART0

external_components:
  - source: github://SeByDocKy/myESPhome
    components: [openfirenet]

tinyusb:
  usb_vendor_id: 0x303A      # values the stove expects
  usb_product_id: 0x819A

usb_cdc_acm:
  rx_buffer_size: 2048       # the stove sends ~1.3 kB messages: must be > 256
  tx_buffer_size: 1024
  interfaces:
    - id: cdc_stove

openfirenet:
  id: stove
  uart_id: cdc_stove
  update_interval: 2s        # poll cycle period
  frame_gap: 100ms           # pause between two frames sent to the stove
```

## Hub options

| Option | Default | Description |
|---|---|---|
| `uart_id` | – | The `usb_cdc_acm` interface id. |
| `update_interval` | `2s` | Period of the poll cycle (controls, sensors). |
| `frame_gap` | `100ms` | Silence left between two frames sent to the stove (20 ms – 2 s). |

## Entities

All platforms take `openfirenet_id` (the hub id), every key is optional, and each entity accepts the usual
ESPHome options of its platform (`name`, `id`, `icon`, `filters`, `on_value`…).

### sensor

| Key | Unit | Description |
|---|---|---|
| `room_temperature` | °C | Room temperature measured by the stove (wire value ÷ 10). |
| `target_temperature` | °C | Room temperature set point currently applied. |
| `heating_power` | % | Heating power stage currently requested. |
| `flame_temperature` | – | Flame / combustion temperature as reported by the stove (raw value). |
| `id_fan_measured` | – | Induced-draft fan speed, measured (diagnostic). |
| `id_fan_set` | – | Induced-draft fan speed, set point (diagnostic). |
| `board_temperature` | °C | Mainboard temperature (diagnostic, scale ÷ 10 to be confirmed). |
| `rssi` | dBm | WiFi signal level as stored by the stove (diagnostic). |
| `pellet_hours` | h | Pellet operating hours counter. |
| `pellets_total` | kg | Total pellet consumption counter. |
| `service_countdown` | – | Countdown until the next service (diagnostic). |
| `ignition_count` | – | Number of ignitions (diagnostic). |
| `on_off_cycles` | – | Number of on/off cycles (diagnostic). |
| `error_code` | – | Raw error bitmask, 0 when no error (diagnostic). |

### text_sensor

| Key | Description |
|---|---|
| `state` | Operating state: Off, Standby, Ignition, Flame Start, Heating, Grate Cleaning, Burn Off, Split Log… |
| `error` | Active errors decoded from the bitmask, `None` when there is no error. |
| `model` | Stove make and model (e.g. `RIKA ROCO`) (diagnostic). |
| `firmware` | Mainboard firmware version, e.g. `V2.29.585.12` (diagnostic). |
| `mode` | Current operating mode: Manual, Auto, Comfort or Setback. |

### binary_sensor

| Key | Device class | Description |
|---|---|---|
| `connected` | connectivity | ON once the USB handshake with the stove is complete (diagnostic). |
| `burning` | running | ON while the stove is in a combustion phase (ignition to burn-off). |
| `error` | problem | ON when at least one error bit is set. |
| `hopper_lid_open` | opening | ON when the pellet hopper lid is open. |

### switch

| Key | Description |
|---|---|
| `power` | Switches the stove on / off. |

### number

| Key | Range | Description |
|---|---|---|
| `target_temperature` | 14 – 28 °C, step 0.5 | Room temperature set point. |
| `heating_power` | 30 – 100 %, step 1 | Heating power stage (used in manual mode). |

### select

| Key | Options | Description |
|---|---|---|
| `mode` | Manual, Auto, Comfort, Setback | Operating mode of the stove. |

## Full example

Every hub option and every entity (same file as `code/test_openfirenet.yaml`):

```yaml
substitutions:
  name: pelletstove
  friendly_name: Pellet Stove

esphome:
  name: ${name}
  friendly_name: ${friendly_name}

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

# Native USB is owned by TinyUSB, so the logger must use UART0.
logger:
  hardware_uart: UART0
  level: DEBUG

wifi:
  ssid: test
  password: testtesttest

api:
ota:
  - platform: esphome

external_components:
  - source: github://SeByDocKy/myESPhome
    components: [openfirenet]
    refresh: 10s

tinyusb:
  usb_vendor_id: 0x303A
  usb_product_id: 0x819A

usb_cdc_acm:
  rx_buffer_size: 2048
  tx_buffer_size: 1024
  interfaces:
    - id: cdc_stove

openfirenet:
  id: stove
  uart_id: cdc_stove
  update_interval: 2s
  frame_gap: 100ms

sensor:
  - platform: openfirenet
    openfirenet_id: stove
    room_temperature:
      name: ${name}_room_temperature
    target_temperature:
      name: ${name}_target_temperature
    heating_power:
      name: ${name}_heating_power
    flame_temperature:
      name: ${name}_flame_temperature
    id_fan_measured:
      name: ${name}_id_fan_measured
    id_fan_set:
      name: ${name}_id_fan_set
    board_temperature:
      name: ${name}_board_temperature
    rssi:
      name: ${name}_rssi
    pellet_hours:
      name: ${name}_pellet_hours
    pellets_total:
      name: ${name}_pellets_total
    service_countdown:
      name: ${name}_service_countdown
    ignition_count:
      name: ${name}_ignition_count
    on_off_cycles:
      name: ${name}_on_off_cycles
    error_code:
      name: ${name}_error_code

text_sensor:
  - platform: openfirenet
    openfirenet_id: stove
    state:
      name: ${name}_state
    error:
      name: ${name}_error
    model:
      name: ${name}_model
    firmware:
      name: ${name}_firmware
    mode:
      name: ${name}_mode

binary_sensor:
  - platform: openfirenet
    openfirenet_id: stove
    connected:
      name: ${name}_connected
    burning:
      name: ${name}_burning
    error:
      name: ${name}_error_active
    hopper_lid_open:
      name: ${name}_hopper_lid_open

switch:
  - platform: openfirenet
    openfirenet_id: stove
    power:
      name: ${name}_power

number:
  - platform: openfirenet
    openfirenet_id: stove
    target_temperature:
      name: ${name}_target_temperature_set
    heating_power:
      name: ${name}_heating_power_set

select:
  - platform: openfirenet
    openfirenet_id: stove
    mode:
      name: ${name}_mode_select
```

## How it works (short)

1. The stove probes the USB port with `0x16 '3'`; the component answers with the version frame
   (announcing a high bootloader version so the stove never offers a firmware update).
2. The stove confirms (`GET_CDCDEVICE_VERSION_FINISHED`) and pushes its status, which is answered.
3. Every `update_interval` the component sends `GET_CONTROLS`, `GET_SENSORS`, `GET_REVISION` and
   several `TRANSFER_COMPLETED`; the stove answers with `POST_CONTROLS` / `POST_SENSORS`, parsed by position.
4. Writes are read-modify-write of the 5 control fields. The stove echoes old values for a few seconds, so
   the new value is held optimistically for up to 8 s.
5. Frames are sent in ≤ 32-byte flushed chunks (the stove's USB host aborts after 4 consecutive full packets).

## Known uncertainties (to validate on hardware)

* Command terminator (`\n` assumed) and the exact `GET_CDCDEVICE_STATUS` layout — all in `openfirenet_protocol.h`.
* Number of `TRANSFER_COMPLETED` per cycle (3 chosen) and the default `frame_gap`.
* Scale of `board_temperature` (assumed ×10) and meaning of `flame_temperature` units.
* The stove never asserts DTR; `usb_cdc_acm` drops TX data when a flush times out without DTR.
  If frames get lost, raise `frame_gap` and check logs at `VERY_VERBOSE`.
* Setback mode may not exist on every model; scan-WiFi display on the stove panel is not implemented.

Set `logger: level: VERY_VERBOSE` to dump every RX/TX frame.

## Licence

MIT License — Copyright (c) e-2-nomy.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of
the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*This is an independent project, not affiliated with RIKA / Rika Innovative Ofentechnik. "RIKA" and "Firenet" are
trademarks of their owners.*
