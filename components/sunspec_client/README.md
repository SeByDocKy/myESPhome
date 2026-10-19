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
Recent devices (hybrid inverters, batteries, gateways) often expose the SunSpec 7xx "DER" series instead of
101-124: 701 (AC measurements), 702 (capacity), 703 (enter service), 704 (AC controls), 713 (storage capacity),
714 (DC measurements, one group per port) and 715 (control). These are read, with the same entities as the other
models. Limits for now:

- The 32-bit writable points (for example `WSet`, `VarSet` and the revert times of model 704) stay read only: only
  16-bit points are written. The 16-bit ones of 704 and 715 can be listed in `controls`, but the DER models are
  written in pairs (value + enable, with a revert time), so check the SunSpec specification before using them.
- The curve models 705 to 712 (volt-var, trip curves, ...) are not supported.
- 64-bit counters (for example `TotWhInj`) are published as a 32-bit float: beyond a few million Wh the
  resolution is coarser than 1 Wh.

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

## Starting points

`models` is a compile-time wish list: the entities created are the models of the list that the device really
exposes. The usual models by kind of device are below. They are typical starting points of the SunSpec
standard, not profiles checked brand by brand: devices differ, so start with a wide list, read the `models`
text sensor after the first scan, then narrow the list.

| Device | `models` to start with | Notes |
|---|---|---|
| Single-phase inverter | `[1, 101, 120, 122, 160]` | 102 for a split-phase inverter |
| Three-phase inverter | `[1, 103, 120, 122, 160]` | 160 gives the MPPT inputs |
| Inverter with floating point values | 111, 112 or 113 instead of 101, 102 or 103 | exposed by some brands |
| Energy meter | `[1, 201]` | 201 single phase, 202 split phase, 203 three-phase wye, 204 three-phase delta |
| Battery | `[1, 802]` | 124 for the charge / discharge controls |
| Recent DER device (SunSpec 7xx series) | `[1, 701, 713, 714]` | read only for now, see below |

To get controls, the model must be in **both** lists: add it to `models` (so that it is read) and its id to
`controls`. A model of `controls` that is not in `models` is refused when the configuration is validated, and so
is a model without writable points (1, 101 to 103 and 201 to 204 have none):

```yaml
sunspec_client:
  models: [1, 103, 123, 160]
  controls: [123]   # 123 is also in models
```

Models with writable points: 123 (immediate controls: power limit, connection, power factor), 121 (settings),
124 (storage controls). Controls act on the device: enable them one model at a time, see the warning above.

## Devices and the models they expose

The SunSpec Alliance defines the models (the tables of this component come from its public definitions). Which
models a given product exposes is up to the manufacturer. The table below only lists what could be checked in
published documentation; everything else must be read from the device with the `models` text sensor.

| Kind of device | Models of the standard | Supported by this component |
|---|---|---|
| Common block (every device) | 1 | yes |
| Inverter, integer values with scale factors | 101 single phase, 102 split phase, 103 three phase | yes |
| Inverter, floating point values | 111, 112, 113 | yes |
| Inverter settings, status, controls | 120 nameplate, 121 settings, 122 status, 123 immediate controls | yes |
| Storage controls | 124 | yes |
| MPPT inputs | 160 | yes |
| Energy meter, integer | 201 single phase, 202 split phase, 203 wye, 204 delta | yes |
| Energy meter, floating point | 211, 212, 213, 214 | no |
| Battery | 802 | yes |
| Network (Ethernet, IP) | 11, 12 | no |
| Advanced inverter functions | 126 to 132 | no |
| Recent "DER" series | 701 AC measurement, 702 capacity, 703 enter service, 704 AC controls, 713 storage capacity, 714 DC measurement, 715 control | yes, mostly read only |
| DER curves and trips | 705 to 712 | no |

Models the component does not support are ignored during the scan, they only appear in the `models` text sensor.

Models reported by manufacturers (documentation or SunSpec certification registry, checked on 2026-10-19):

| Manufacturer / product | Models | Source |
|---|---|---|
| SMA (Sunny Boy, Tripower, Data Manager, EDMM) | 1, 11, 12, 101-103, 120-123, 126-132, 160, 201-204, 211-214 | [SMA, SunSpec Modbus profile](https://manuals.sma.de/EDMM-20/en-US/14259660939.html) |
| Fronius (Symo, Primo, GEN24) | 1, 111-113 (float, default) or 101-103 (integer), 120, 121, 122, 123, 160; 124 on storage systems only; meters through Modbus unit ids 200-204; 701-713 on Primo GEN24 208-240 V 3.8-10 kW with the RTU interface set to Slave | [Fronius manual](https://manuals.fronius.com/html/4204102649/en-US.html) |
| Sigenergy (Sigen inverter) | 1, 704, 715 | [SunSpec certification registry](https://sunspec.org/product-certification-registry/) |
| Afore (Afore inverter) | 1, 213, 701, 702, 703, 704, 713, 714, 715 | [SunSpec certification registry](https://sunspec.org/product-certification-registry/) |
| Atmoce (M-Gateway) | 1, 703, 704 | [SunSpec certification registry](https://sunspec.org/product-certification-registry/) |

With a recent device only the common block (1) and some of the 7xx models are read by this component.

Other members of the alliance publish SunSpec certificates (the registry gives, for each product, a certificate
and a PICS file with the models): for example Sungrow, Solis (Ginlong), Yaskawa Solectria, Alpha ESS, Fox ESS,
Carlo Gavazzi and APsystems. Their model ids are not listed here because they could not be checked: look the
product up in the [certification registry](https://sunspec.org/product-certification-registry/), or scan it.

## Options

| Option | Default | Description |
|---|---|---|
| `modbus_id` | – | Core `modbus` bus |
| `address` | 1 | Modbus unit id |
| `name_prefix` | – | Prefix for the entity names |
| `models` | – | SunSpec model ids wanted (1, 101-103, 111-113, 120-124, 160, 201-204, 701-704, 713-715, 802) |
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
