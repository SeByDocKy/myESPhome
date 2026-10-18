# auto_gen

Prototype ESPHome external component that **registers entities at runtime**
(after boot) instead of declaring them in YAML.

It is a feasibility test for components that only know their entity list after
talking to a device (for example a SunSpec Modbus client that discovers the
models exposed by an inverter).

## How it works

ESPHome stores entities in fixed-size containers whose size is computed at
compile time. `auto_gen` therefore does two things:

1. **At code generation time** (`__init__.py`): it reserves `max_sensors`
   extra sensor slots (`CORE.platform_counts["sensor"]`) and pre-registers the
   unit of measurement in the generated string tables.
2. **At runtime** (`auto_gen.cpp`): after `delay`, it allocates a
   `sensor::Sensor`, registers it with `App.register_sensor(...)`, publishes a
   changing value, then fires `on_registered`. With `count` > 1 it repeats this
   every `create_interval` until `count` sensors exist, so the number of
   entities visible in Home Assistant grows over time.

Home Assistant only reads the entity list when the API connection is
established. If it is already connected when the entity appears, chain the
`api_resync.resync` action of the `api_resync` component in `on_registered`.

## Configuration

See `test_auto_gen.yaml` for a complete example.

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome@main
    components: [auto_gen, api_resync]

api_resync:
  id: resync_hub

auto_gen:
  id: autogen_hub
  delay: 10s
  count: 3
  create_interval: 15s
  max_sensors: 4
  sensor_name: ${name}_autogen_test
  unit_of_measurement: W
  on_registered:
    - logger.log: "auto_gen: sensor registered"
    - api_resync.resync: resync_hub
```

| Option | Default | Description |
| --- | --- | --- |
| `delay` | `10s` | Delay between boot and the creation of the first demo sensor |
| `count` | `1` | Number of demo sensors created at runtime (must not exceed `max_sensors`) |
| `create_interval` | `15s` | Delay between two successive creations |
| `max_sensors` | `4` | Sensor slots reserved at compile time (1-64) |
| `sensor_name` | `auto_gen_test` | Base name of the runtime sensors (`_<n>` is appended when `count` > 1) |
| `unit_of_measurement` | `W` | Unit of the runtime sensors (must be known at compile time) |
| `on_registered` | - | Automation fired after each registered entity |

`max_sensors` is only the number of reserved slots; `count` is how many
sensors are actually created.

## What to check on the device

- The log shows `Registered sensor '...' (1/4 slots used)`, then 2/4, 3/4 ...
  every `create_interval`.
- If Home Assistant connects **after** the registration, the entity appears
  without any resync.
- If Home Assistant was already connected, the entity appears only after the
  resync triggered from `on_registered`.
- The entity keeps the same name, object id and unique id across reboots.

## Limitations

- Entity names, units and other strings must be known (or registered) at
  compile time; only the *moment* of registration is dynamic.
- Registration happens once per entity; entities cannot be removed at runtime.
- The component depends on `esphome.core.entity_helpers`, an internal ESPHome
  module (packed entity fields, ESPHome >= 2026.8). It may need adjustments
  with other versions.
- Other entity consumers (web server, MQTT) may not pick up late entities.

## License

MIT License

Copyright (c) e-2-nomy

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
