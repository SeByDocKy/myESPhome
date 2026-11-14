# bluetti

Native ESPHome component for **BLUETTI plug & play batteries** (Balco 260, Balco 500) over **local Modbus TCP** (real MBAP header) — no cloud, no Home Assistant add-on.

> Status: **read-only** first version (sensor, text_sensor, binary_sensor). Write platforms (switch / number / select) will follow.
> The register maps come from the community library [bluetti-modbus](https://github.com/Patrick762/bluetti-modbus) (MIT, see `LICENSE.bluetti-modbus`).

## Supported models

| `model`   | Device     | Notes |
|-----------|------------|-------|
| `balco260`| Balco 260  | Register map validated by the community |
| `balco500`| Balco 500  | Register map captured by the community; register 57001 is "System ON/OFF" |

Expansion packs are not supported yet.

## Device prerequisites

Enable Modbus TCP on the device web page (admin login: the Bluetooth password, or blank) → *Settings → Modbus TCP*, port `502`.

## Architecture

```
tcp_uart → modbus_tcp_uart → modbus_controller (fast) ─┐
                             modbus_controller (slow) ─┴→ bluetti (hub) → sensor / text_sensor / binary_sensor
```

The hub knows, per model, which register blocks the device answers (reads must stay inside a block and below a maximum span), and splits the read ranges accordingly. Slow-changing entities (versions, serial numbers, energy counters, settings) are polled by the optional slow controller.

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
