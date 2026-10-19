# sunspec_client

Native ESPHome **SunSpec** client for the core `modbus` bus (RS485 or Modbus TCP through
`tcp_uart` + `modbus_tcp_uart`).

## How it works

1. After `startup_delay`, the component looks for the `SunS` marker at registers 40000, 0 and 50000 and
   walks the model headers (id, length) until the end marker `0xFFFF`.
2. The ids found are published in the `models` text sensor (e.g. `1, 103, 160`).
3. The models of `models:` that the device really exposes are read once. A point is implemented when its value
   is not the reserved "not implemented" value of its type (and its scale factor is implemented too).
4. The entities of all implemented points are created **at once**, then Home Assistant is asked to read the entity
   list again **one single time** (the API client is asked to reconnect, like the `api_resync` component does).
5. The entities are refreshed every `update_interval`, scale factors applied (they are read again every time).

On failure (no answer, no marker) the text sensor shows `scan failed` and a new scan starts after
`scan_retry_interval`. A request without answer is retried 3 times first.

### Entity names

The names come from the SunSpec model definitions (tables generated at build time, see below), not from the
device. The name of a sensor is `name_prefix` + the SunSpec label of the point (for example `pv_inv Watts`). An empty
or missing `name_prefix` adds nothing. Points of a repeating group get `<group> <n>` in front of the label (for
example `pv_inv module 2 DC Voltage`). If two models give the same name, ` (model <id>)` is appended.

### Entity types

| SunSpec point type | Entity | State |
|---|---|---|
| `int16`, `uint16`, `int32`, `uint32`, `int64`, `uint64`, `float32` | `sensor` | value with scale factor applied, unit and device class from the model |
| `acc32`, `acc64` (energy counters) | `sensor` (`total_increasing`) | counter value; an `acc` reading 0 at discovery is "not accumulated" and gets no entity |
| `enum16` | `text_sensor` (diagnostic) | name of the enum value (the number when the value is unknown) |
| `bitfield16`, `bitfield32` | `text_sensor` (diagnostic) | names of the bits set, separated by commas (`none` when no bit is set) |
| `string` | `text_sensor` (diagnostic) | the text; an empty string means not implemented |
| each named bit of a bitfield, with `bitfield_binary_sensors: true` | `binary_sensor` (diagnostic) | ON when the bit is set |

A point gets an entity only when the device implements it: not the reserved "not implemented" value of its type
(for example `0x8000` for `int16`, `0xFFFF` for `uint16`, `enum16` and `bitfield16`, NaN for `float32`). Text
sensors are published when the text changes. The enum and bit names are the SunSpec symbol names (for example
`MPPT`, `GROUND_FAULT`).

### Controls (writes)

By default the component only reads. With `controls: [<model ids>]` the **writable** points (`access: RW` in the
SunSpec definition) of those models become controls instead of read-only sensors:

| SunSpec point | Entity | Write |
|---|---|---|
| `int16`, `uint16` | `number` | the value divided by the scale factor, written with function code 0x06 |
| `enum16` | `select` (options = enum symbols) | the value of the selected symbol |

Range of a `number`: the 16-bit register scaled by the scale factor, `0` to `100` for percentages. These are only
generic limits: the device refuses values it does not accept (the device value is read back shortly after every
write, so the entity shows what the device really holds). Points of a repeating group, `bitfield` and 32-bit points
are not writable in this version.

**Warning:** writes act on power electronics (for example `Conn` can disconnect an inverter from the grid and
`WMaxLimPct` limits its output). Enable `controls` only for the models you need, and check the effect on your device.
Models with writable points: 121 (basic settings), 123 (immediate controls), 124 (basic storage controls) and
802 (battery). Each model of `controls` must also be in `models`.

From a lambda, `id(<hub>)->set_point("WMaxLimPct", 50.0f)` writes a number control as if it was set from Home
Assistant.

### Bitfield binary sensors

With `bitfield_binary_sensors: true` a `binary_sensor` is created for every named, non-reserved bit of each bitfield
point the device implements (in addition to the text sensor), for example `pv_inv Event1 Ground fault`. This is
convenient for automations and alerts, but it adds many entities (about 16 for the events of model 103, 13 per MPPT
module for the events of model 160, 21 to 29 for the meter events). It is off by default. Bits without a name in the
model (for example the vendor bitfields) stay in the text sensor only.

### Entity slots

ESPHome entity containers are sized at compile time. The component reserves one `sensor` slot per numeric point and
one `text_sensor` slot per enum, bitfield or string point (and one `binary_sensor` slot per named bit when
`bitfield_binary_sensors` is on) of the models in `models:` (and `max_modules` instances
of repeating groups). So `models:` is the wish list: entities are only created for the models the device exposes,
but slots are reserved for all of them. A log error tells when some entities could not be created for lack of slots.

## Configuration

```yaml
tcp_uart:
  - id: sunspec_link
    host: 192.168.1.50
    port: 502

modbus_tcp_uart:
  - id: sunspec_uart
    tcp_uart_id: sunspec_link

modbus:
  - id: modbus_sunspec
    uart_id: sunspec_uart

sunspec_client:
  - id: inverter
    modbus_id: modbus_sunspec
    address: 1                # Modbus unit id
    name_prefix: pv_inv       # prefix of the name of every created entity
    models: [1, 103, 160]
    controls: [123]           # optional: writable points of these models become number / select
    max_modules: 4            # instances of a repeating group (MPPT inputs of model 160) with entities
    update_interval: 10s
    wait_for_network: true           # default: true for Modbus TCP, false for RS485
    bitfield_binary_sensors: false   # true: one binary_sensor per named event/state bit
    startup_delay: 5s
    scan_retry_interval: 30s

text_sensor:
  - platform: sunspec_client
    sunspec_client_id: inverter
    models:
      name: Inverter models
```

`tcp_uart` / `modbus_tcp_uart` are not in a stable release yet (PR #20198 / #20061), see
`test_sunspec_client.yaml`. With RS485, use a regular `uart:` instead.

## Options

| Option | Default | Description |
|---|---|---|
| `modbus_id` | – | Core `modbus` bus |
| `address` | 1 | Modbus unit id |
| `name_prefix` | – | Prefix for the entity names |
| `models` | – | SunSpec model ids wanted (1, 101-103, 111-113, 120-124, 160, 201-204, 802) |
| `controls` | – | Models whose writable points become `number` / `select` controls (writes enabled), subset of `models` |
| `max_modules` | 4 | Instances of a repeating group that get entities (1 to 16) |
| `bitfield_binary_sensors` | false | One binary_sensor per named bit of bitfield points |
| `wait_for_network` | auto | Wait for WiFi/Ethernet before the first scan. Default: `true` for Modbus TCP (`modbus_tcp_uart`), `false` for a serial line (RS485) |
| `update_interval` | 10s | Refresh period of the sensors |
| `startup_delay` | 5s | Wait before the first scan |
| `scan_retry_interval` | 30s | Delay before a new scan after a failure |

## Model tables and attribution

`sunspec_models.h` and `sunspec_models_info.py` are generated by `tools/gen_models.py` from the official
[SunSpec model definitions](https://github.com/sunspec/models) (JSON files, Apache License 2.0). The point
names, labels, units, offsets and symbols in these tables come from those definitions, copyright the SunSpec
Alliance, used under the Apache License 2.0 (<https://www.apache.org/licenses/LICENSE-2.0>):

```
python3 tools/gen_models.py <path to sunspec/models>/json sunspec_models.h
```

## Notes

- Relies on `esphome.core.entity_helpers` (internal ESPHome module, ESPHome >= 2026.8) to register unit and
  device class strings used by the entities created at runtime.
- Tested with a simulated RTU device on the host platform (sensors and text sensors); not yet tested on a real
  device.

## License

MIT License

Copyright (c) e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
