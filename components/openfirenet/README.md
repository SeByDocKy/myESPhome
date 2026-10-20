# openfirenet — ESPHome component

Native ESPHome component that makes an **ESP32-S3** behave like the **RIKA Firenet 2.0 USB dongle**
(also sold under the ANIMO brand), so a pellet stove can be monitored and controlled from Home Assistant
without the manufacturer cloud.

The stove is the USB **host**, the ESP32-S3 is the USB **device** (CDC ACM). The wire protocol was
documented by the [open-firenet](https://github.com/openfirenet/open-firenet) project; this component is an
independent implementation written from that specification (no code copied).

> **Status: experimental.** The protocol layer is unit tested on a PC, the YAML schema is validated and the
> component compiles, but it has **not been validated against a real stove yet**. See *Known uncertainties*.

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

A complete example with every entity is in `code/test_openfirenet.yaml` in this repository.

## Hub options

| Option | Default | Description |
|---|---|---|
| `uart_id` | – | The `usb_cdc_acm` interface id. |
| `update_interval` | `2s` | Period of the poll cycle (controls, sensors). |
| `frame_gap` | `100ms` | Silence left between two frames sent to the stove (20 ms – 2 s). |

## Entities

All platforms take `openfirenet_id` (the hub id) and every key is optional.

**sensor** — `room_temperature`, `target_temperature`, `heating_power`, `flame_temperature`,
`id_fan_measured`, `id_fan_set`, `board_temperature`, `rssi`, `pellet_hours`, `pellets_total`,
`service_countdown`, `ignition_count`, `on_off_cycles`, `error_code`.

**text_sensor** — `state` (Off, Standby, Ignition, Heating…), `error`, `model`, `firmware`, `mode`.

**binary_sensor** — `connected` (handshake done), `burning`, `error`, `hopper_lid_open`.

**switch** — `power`.

**number** — `target_temperature` (14–28 °C, step 0.5), `heating_power` (30–100 %).

**select** — `mode` (Manual, Auto, Comfort, Setback).

```yaml
switch:
  - platform: openfirenet
    openfirenet_id: stove
    power:
      name: ${name}_power
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
