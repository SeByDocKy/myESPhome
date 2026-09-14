# sensor_combined

A native ESPHome `sensor` platform that computes a combined value
(sum, mean, or product) from any number of other sensors, and
republishes it automatically whenever one of the source sensors
updates.

## File layout

`sensor_combined` is a **platform-only** component (new `platform:`
under the existing `sensor:` domain, no new top-level YAML key), so
the config schema / codegen lives in a file named after the domain:

```
components/sensor_combined/
├── __init__.py          # CODEOWNERS only
├── sensor.py              # CONFIG_SCHEMA + to_code (the "sensor" platform)
├── sensor_combined.h
└── sensor_combined.cpp
```

## Installation

```yaml
external_components:
  - source:
      type: local
      path: components
```

Or straight from GitHub:

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [sensor_combined]
    refresh: 10s
```

## Usage

```yaml
sensor:
  - platform: sensor_combined
    id: my_sensor_combined
    name: "Average temperature"
    unit_of_measurement: "°C"
    operation: mean   # sum / mean / prod
    sensors:
      - temp1
      - temp2
      # - temp3
```

- `operation` (**Required**): `sum`, `mean`, or `prod`.
- `sensors` (**Required**, list of IDs): the source sensors to
  combine. At least one is required; there is no upper limit.
- `propagate_nan` (*Optional*, boolean, default `false`):
  - `false`: a source sensor currently reporting `NaN` is skipped;
    the operation runs on the remaining valid values (`mean` divides
    by the count of valid values, not the total number of sensors).
  - `true`: any source sensor reporting `NaN` makes the whole
    combined result `NaN`.
- All standard `sensor` schema options are supported (`name`,
  `unit_of_measurement`, `accuracy_decimals`, `device_class`,
  `state_class`, `filters`, etc.), since this platform is built on
  `sensor.sensor_schema()`.

## Behavior notes

- The combined sensor recomputes and republishes every time **any**
  of its source sensors publishes a new state (via
  `add_on_state_callback()`), not on a fixed interval.
- It stays silent (does not publish) until every source sensor has
  published at least one state (`has_state()` check), to avoid
  reporting a partial sum/mean/product at boot.
- `NaN` handling is controlled by `propagate_nan` (see above).
- If **every** source sensor is currently `NaN`, the combined sensor
  publishes `NaN` regardless of `propagate_nan` — there is nothing
  valid left to combine.
