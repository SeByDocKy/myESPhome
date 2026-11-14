# bluetti

Native ESPHome component for **BLUETTI plug & play batteries** (Balco 260, Balco 500) over **local Modbus TCP** (real MBAP header) — no cloud, no Home Assistant add-on.

> Status: sensor, text_sensor, binary_sensor, plus **switch, number and select** to control the battery. Nothing has been tested on real hardware yet.
> The register maps come from the community library [bluetti-modbus](https://github.com/Patrick762/bluetti-modbus) (MIT, see `LICENSE.bluetti-modbus`).

## Supported models

| `model`   | Device     | Notes |
|-----------|------------|-------|
| `balco260`| Balco 260  | Register map validated by the community |
| `balco500`| Balco 500  | Register map reported as confirmed on real hardware by the community; register 57001 is "System ON/OFF" |

Expansion packs are not supported yet.

## Device prerequisites

Enable Modbus TCP on the device web page (admin login: the Bluetooth password, or blank) → *Settings → Modbus TCP*, port `502`.

## Good practice

- The device accepts **only one Modbus client at a time**: stop any other client (Home Assistant integration, another ESP) before connecting the ESP.
- Never scan the register range: the firmware only answers reads inside its register blocks (the hub already stays inside them).
- The official Home Assistant integration polls every 15 s; the 5 s / 60 s intervals used in the examples are the author's choice, not a value from BLUETTI.

## Architecture

```
tcp_uart → modbus_tcp_uart → modbus_controller (fast) ─┐
                             modbus_controller (slow) ─┴→ bluetti (hub) → sensor / text_sensor / binary_sensor
```

The hub knows, per model, which register blocks the device answers (reads must stay inside a block and below a maximum span), and splits the read ranges accordingly. Slow-changing entities (versions, serial numbers, energy counters, settings) are polled by the optional slow controller.

## Modbus TCP wiring

Real Modbus TCP (MBAP header) is provided by `modbus_tcp_uart` on top of `tcp_uart`. Both are still ESPHome pull requests, so they are pulled in with `external_components`:

```yaml
external_components:
  - source: github://pr#20061
    components: [modbus_tcp_uart]
    refresh: 1h
  - source: github://pr#20198
    components: [ble_nus, tcp_uart, uart, usb_cdc_acm, usb_uart]
    refresh: 1h
  - source: github://SeByDocKy/myESPhome
    components: [bluetti]

tcp_uart:
  - id: bluetti_link
    host: 192.168.1.50      # IP address of the Bluetti battery
    port: 502

modbus_tcp_uart:
  - id: bluetti_uart
    tcp_uart_id: bluetti_link

modbus:
  - id: modbus_bluetti
    uart_id: bluetti_uart
    turnaround_time: 100ms

modbus_controller:
  - id: fast_modbus_controller
    address: 1
    modbus_id: modbus_bluetti
    update_interval: 5s
  - id: slow_modbus_controller
    address: 1
    modbus_id: modbus_bluetti
    update_interval: 60s
```

Then add the `bluetti:` hub and the platforms as shown below. The same hub also works on a plain RS485 `uart` + `modbus` bus if a model exposes one.

## Configuration

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [bluetti]

bluetti:
  id: bluetti_hub
  model: balco260
  modbus_controller_id: fast_modbus_controller
  slow_modbus_controller_id: slow_modbus_controller   # optional

sensor:
  - platform: bluetti
    bluetti_id: bluetti_hub
    ac:
      output_power:
        name: ${name}_ac_output_power
```

Entities are grouped as `ac:`, `dc_channels:` and `battery:` (indexed from 0, e.g. `pv0`, `pack0`); device-level entities are flat. A key the selected model does not have is rejected at validation time.
The complete list of entities per model is in `code/device_bluetti_balco260.yaml` and `code/device_bluetti_balco500.yaml` (generated from `registers.py`).

`binary_sensor` additionally accepts `modbus_connection` (connectivity of the Modbus link).

## Controls (switch, number, select)

The writable registers of the library are exposed as controls. They are read back from the device, so their state follows the battery; a command writes the register with function code 06, and the values read back are ignored for 10 s so that the entity does not jump back while the device applies it.

| Platform | Key | Register | Models |
|----------|-----|----------|--------|
| `switch` | `ac_output` | 57001 | Balco 260 |
| `switch` | `system_power` | 57001 | Balco 500 — **switches the whole system on/off** |
| `switch` | `grid_charging` | 57009 | Balco 260 (read-only `binary_sensor` on the Balco 500) |
| `switch` | `grid_feed_in` | 57010 | both |
| `number` | `battery: soc_low_limit`, `soc_high_limit` | 57016 / 57017 | both, 5–100 % |
| `select` | `working_mode` | 57020 | both: Custom, Self Consumption, Backup |

Switches default to `restore_mode: DISABLED`, so nothing is written at boot. There is no `output` platform: the Balco register maps have no power setpoint to drive.

The device confirms a write with an internal register address in the echo (for example 57001 → 2011). The ESPHome Modbus client does not compare that address with the request, but this still has to be confirmed on a real device.

```yaml
switch:
  - platform: bluetti
    bluetti_id: bluetti_hub
    grid_feed_in:
      name: ${name}_grid_feed_in

number:
  - platform: bluetti
    bluetti_id: bluetti_hub
    battery:
      soc_low_limit:
        name: ${name}_battery_soc_low_limit

select:
  - platform: bluetti
    bluetti_id: bluetti_hub
    working_mode:
      name: ${name}_working_mode
```

## Regenerating the register map

```
python3 -I tools/gen_registers.py <path to bluetti_modbus_lib> "<version>"
python3 -I tools/gen_yaml.py
```

## License

MIT License

Copyright (c) e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
