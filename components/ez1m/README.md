# `ez1m` — APsystems EZ1-M Microinverter (native ESPHome component)

Native ESPHome integration for the **APsystems EZ1-M** microinverter over UART.
It replaces a YAML-only `uart:` + `lambda:` integration with a proper hub
component (`EZ1MComponent`, a `PollingComponent` + `UARTDevice`) plus four
sub-platforms (`sensor`, `text_sensor`, `number`, `switch`), all parented to
the hub via `Parented<EZ1MComponent>` — the same pattern used in
[`pcm3k6w`](../pcm3k6w).

The hub polls the inverter every `update_interval`, parses the checksummed
response frame, and dispatches the decoded values to whichever entities were
declared in YAML. No entity is created unless you explicitly list it — unlike
the original lambda, every sensor/number/switch/text_sensor is opt-in.

## Installation

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [ez1m]
    refresh: 10s
```

## Wiring

The EZ1-M's UART runs at **57600 baud, 8N1**. Wire it to a UART bus on your
ESP32:

```yaml
uart:
  id: uart_inverter
  tx_pin: GPIO2
  rx_pin: GPIO1
  baud_rate: 57600
```

## Hub configuration (`ez1m:`)

```yaml
ez1m:
  id: ez1m_hub
  uart_id: uart_inverter
  model: ez1m
  update_interval: 5s
  dc_voltage_divisor: 50.0
  dc_current_divisor: 88.0
  grid_frequency_divisor: 27.32
```

| Option                     | Type     | Default | Required | Description |
|-----------------------------|----------|---------|----------|-------------|
| `id`                        | ID       | auto    | No       | ID of the hub, referenced by every sub-platform as `ez1m_id`. |
| `uart_id`                   | ID       | —       | Yes (inherited from `uart.UART_DEVICE_SCHEMA`) | The `uart:` bus the EZ1-M is wired to. |
| `model`                     | string   | `ez1m`  | No       | One of `ez1m`, `ez1h`, `ez1d`. Sets the hardware's maximum output power: 800 W / 960 W / 1800 W respectively. Drives both the ceiling enforced by `set_power_limit()`/`turn_on()` in the hub and the default upper bound (`max_value`) of the `power_limit` number below. |
| `update_interval`           | time     | `5s`    | No       | How often the hub sends the status poll request to the inverter (standard `PollingComponent` option). |
| `dc_voltage_divisor`        | float    | `50.0`  | No       | Raw-value divisor used to compute CH1/CH2 DC voltage. Adjust against a known-good meter if your unit reads slightly off. |
| `dc_current_divisor`        | float    | `88.0`  | No       | Raw-value divisor used to compute CH1/CH2 DC current. Same calibration note as above. |
| `grid_frequency_divisor`    | float    | `27.32` | No       | Raw-value divisor used to compute grid frequency. |

Only one `ez1m:` block is needed even if you declare entities across several
`sensor:`/`number:`/`switch:`/`text_sensor:` blocks — just reference the same
`ez1m_id`.

## `sensor:` platform

```yaml
sensor:
  - platform: ez1m
    ez1m_id: ez1m_hub
    ch1_dc_voltage:
      name: "CH1 DC Voltage"
    ...
```

All keys are optional — declare only the ones you want. Each accepts the
standard ESPHome `sensor:` options (`name`, `id`, `filters`, `web_server`,
`entity_category`, etc.) on top of the pre-set `unit_of_measurement`,
`device_class`, `state_class` and `accuracy_decimals` shown below.

Icons follow the same conventions as the [`hms`](../hms) component so that
similarly-typed entities look consistent across both: `mdi:power` for power,
`mdi:current-dc` for DC current, `mdi:sine-wave` for voltage,
`mdi:metronome` for frequency, `mdi:thermometer` for temperature, and
`mdi:counter` for energy.

| Key                     | Unit | Device class | State class        | Accuracy | Icon | Notes |
|--------------------------|------|---------------|---------------------|----------|------|-------|
| `ch1_dc_voltage`         | V    | voltage       | measurement         | 1        | `mdi:sine-wave` | CH1 PV string DC voltage. |
| `ch2_dc_voltage`         | V    | voltage       | measurement         | 1        | `mdi:sine-wave` | CH2 PV string DC voltage. |
| `ch1_dc_current`         | A    | current       | measurement         | 2        | `mdi:current-dc` | CH1 PV string DC current. |
| `ch2_dc_current`         | A    | current       | measurement         | 2        | `mdi:current-dc` | CH2 PV string DC current. |
| `ch1_dc_power`           | W    | power         | measurement         | 0        | `mdi:power` | Computed as CH1 voltage × current. |
| `ch2_dc_power`           | W    | power         | measurement         | 0        | `mdi:power` | Computed as CH2 voltage × current. |
| `total_dc_power`         | W    | power         | measurement         | 0        | `mdi:power` | CH1 + CH2 DC power. |
| `ac_power`               | W    | power         | measurement         | 0        | `mdi:power` | AC power delivered to the grid. |
| `grid_frequency`         | Hz   | frequency     | measurement         | 2        | `mdi:metronome` | Rejected (not published) outside the 45–55 Hz sanity range, or if the raw field reads `0xFFFF`. |
| `temperature`            | °C   | temperature   | measurement         | 0        | `mdi:thermometer` | Inverter internal temperature. Rejected if raw value is 0 or ≥ 200. |
| `daily_energy`           | kWh  | energy        | total_increasing    | 3        | `mdi:counter` | CH1 + CH2 session energy; resets each morning when the DSP restarts its internal counter. |
| `ch1_session_energy`     | kWh  | energy        | total_increasing    | 3        | `mdi:counter` | CH1 energy since the day's DSP boot. |
| `ch2_session_energy`     | kWh  | energy        | total_increasing    | 3        | `mdi:counter` | CH2 energy since the day's DSP boot. |
| `lifetime_energy`        | kWh  | energy        | total_increasing    | 3        | `mdi:counter` | RAM-accumulated lifetime total (see [Lifetime energy persistence](#lifetime-energy-persistence) below). |
| `inverter_uptime`        | s    | —             | —                   | 0        | — | Inverter uptime since DSP boot. `entity_category: diagnostic`. |
| `power_limit_readback`   | W    | power         | measurement         | 0        | `mdi:power` | The inverter's own confirmation of the power limit it actually applied, decoded from the same status-frame field that drives the `power_limit` number's readback (see below). Unlike the number, this is a plain sensor, so it gets normal history/graphing. `entity_category: diagnostic`. |

## `text_sensor:` platform

```yaml
text_sensor:
  - platform: ez1m
    ez1m_id: ez1m_hub
    inverter_state:
      name: "Inverter State"
    dsp_version:
      name: "DSP Version"
```

| Key               | Notes |
|--------------------|-------|
| `inverter_state`   | One of `Producing`, `Standby`, `Ramping up`, or `Unknown 0xNN` for any unrecognized status byte. Icon: `mdi:state-machine`. |
| `dsp_version`      | DSP firmware version string (`major.minor`). `entity_category: diagnostic`. |

## `number:` platform

```yaml
number:
  - platform: ez1m
    ez1m_id: ez1m_hub
    power_limit:
      name: "Power Limit"
    total_energy:
      name: "Total Energy"
```

| Key             | Range (default)     | Step (default) | Optimistic | Icon | Mode | Notes |
|------------------|----------------------|------------------|------------|------|------|-------|
| `power_limit`    | `min_value` 30 – `max_value` *model-dependent* (800/960/1800 W) | 1   | No  | `mdi:power`, unit W | **slider** (forced, not configurable) | Sends an output power-limit command to the inverter. The displayed value only updates once the inverter echoes the new limit back in a subsequent status frame — it is **not** set optimistically. |
| `total_energy`   | `min_value` 0 – `max_value` 999999 kWh | 0.001 | Yes | `mdi:counter` | `auto` (default) | Manual override/reset of the lifetime energy accumulator. `entity_category: config`. |

`power_limit`'s default `max_value` is derived from the hub's `model` option
(800 W for `ez1m`, 960 W for `ez1h`, 1800 W for `ez1d`) — no need to set it
yourself unless you want a narrower range than your model's ceiling.

See also the `power_limit_readback` sensor (in `sensor:` platform above), which
exposes the same hardware-confirmed value as a plain sensor for history/graphing.

Both `min_value`, `max_value` and `step` can be overridden per-entity in YAML
if you want a narrower range, e.g.:

```yaml
    power_limit:
      name: "Power Limit"
      max_value: 600
```

## `output:` platform

```yaml
output:
  - platform: ez1m
    ez1m_id: ez1m_hub
    power_output:
      id: ez1m_power_output
      max_power: 800
```

| Key             | Type   | Default | Notes |
|------------------|--------|---------|-------|
| `power_output`   | `output::FloatOutput` | — | A standard ESPHome float output, `0.0`–`1.0`. `1.0` maps to `max_power` watts, `0.0` turns the inverter off. |
| `max_power`      | float  | `800`   | Watts corresponding to `state == 1.0`. Defaults to the EZ1-M's hardware ceiling; lower it if you want to cap the output at less than full power. |

`write_state()` computes `watts = state * max_power`, then:
- if `watts <= 0`, sends the inverter's off command (same as the `switch`'s `turn_off`);
- otherwise, sends `watts` (capped at `max_power`, no lower clamp) as a
  power-limit command, exactly like the `power_limit` number. There is no
  30 W floor enforced here — if you drive this output from a PID/regulation
  component, set its own `output_min` there; below the EZ1-M's real
  minimum (~30 W) the inverter will simply stop producing.

This is meant for use cases like a `light`/`fan` output template, a PID
climate-style controller, or any other component in ESPHome that expects a
plain `output::FloatOutput` rather than a `number`. It shares the same
underlying command as `power_limit` and `inverter_onoff` — driving one will
be reflected back on the others via the hub's status-frame readback (for
`power_limit`) or state sync (for `inverter_onoff`).

## `switch:` platform

```yaml
switch:
  - platform: ez1m
    ez1m_id: ez1m_hub
    inverter_onoff:
      name: "Inverter On/Off"
```

| Key               | Restore mode          | Notes |
|--------------------|------------------------|-------|
| `inverter_onoff`   | `RESTORE_DEFAULT_ON` (default, overridable) | Turning it on re-sends the current `power_limit` number's value (or 800 W if none is configured / the value is unknown). Turning it off sends the inverter's shutdown command. The switch also self-corrects: every status frame updates it to reflect the inverter's real reported state (`Standby` → off, anything else → on), so it won't drift out of sync if the inverter is controlled by another source. |

## Other entities

These come from standard, already-existing ESPHome platforms and are simply
listed for completeness — no `ez1m` code is involved:

```yaml
sensor:
  - platform: wifi_signal
    name: "WiFi Signal"
    update_interval: 60s
  - platform: uptime
    name: "ESP Uptime"

text_sensor:
  - platform: wifi_info
    ip_address:
      name: "IP Address"
    ssid:
      name: "Connected SSID"

button:
  - platform: restart
    name: "Restart"
```

## Lifetime energy persistence

`lifetime_energy` is accumulated in RAM by summing the delta of
`daily_energy` on every poll (handling the daily DSP counter reset), and is
persisted to flash:

- automatically, once per hour, via ESPHome's native preferences API
  (`global_preferences`/NVS) — no `number` entity is required for this to
  survive a reboot;
- immediately, whenever the `total_energy` number is written to (manual
  correction or reset).

## Optional platforms

None of `sensor:`, `text_sensor:`, `number:`, `switch:` or `output:` are
required — declare only the ones you actually need. The hub's code that
references each platform's concrete type is wrapped in `#ifdef USE_SENSOR` /
`USE_TEXT_SENSOR` / `USE_NUMBER` / `USE_SWITCH` guards, so a config that
only uses `sensor:` (for example) compiles fine without ever pulling in the
`number`/`switch`/`text_sensor` headers — no dummy/unused block needed.
`output:` needs no such guard on the hub side: `EZ1MOutput` only calls the
hub's already-public `set_power_limit()`/`turn_off()`, it doesn't require
the hub to know about it.

## Frame protocol notes

- Request/response frames start with `0xFB 0xFB` and end with `0xFE 0xFE`,
  with a 16-bit sum checksum covering bytes 2 through `len+2`.
- The hub reads the UART byte-by-byte in `loop()`, buffering and
  resynchronizing on the `0xFB 0xFB` marker if a frame fails checksum or
  footer validation — it does not rely on ESPHome's `uart.debug` YAML block.
- `dc_voltage_divisor`, `dc_current_divisor` and `grid_frequency_divisor` are
  the only tunables; the rest of the byte layout (see the field map in the
  component's `ez1m.cpp`) is fixed by the EZ1-M's firmware.
