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
  update_interval: 5s
  dc_voltage_divisor: 50.0
  dc_current_divisor: 88.0
  grid_frequency_divisor: 27.32
```

| Option                     | Type     | Default | Required | Description |
|-----------------------------|----------|---------|----------|-------------|
| `id`                        | ID       | auto    | No       | ID of the hub, referenced by every sub-platform as `ez1m_id`. |
| `uart_id`                   | ID       | —       | Yes (inherited from `uart.UART_DEVICE_SCHEMA`) | The `uart:` bus the EZ1-M is wired to. |
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

| Key                     | Unit | Device class | State class        | Accuracy | Notes |
|--------------------------|------|---------------|---------------------|----------|-------|
| `ch1_dc_voltage`         | V    | voltage       | measurement         | 1        | CH1 PV string DC voltage. |
| `ch2_dc_voltage`         | V    | voltage       | measurement         | 1        | CH2 PV string DC voltage. |
| `ch1_dc_current`         | A    | current       | measurement         | 2        | CH1 PV string DC current. |
| `ch2_dc_current`         | A    | current       | measurement         | 2        | CH2 PV string DC current. |
| `ch1_dc_power`           | W    | power         | measurement         | 0        | Computed as CH1 voltage × current. |
| `ch2_dc_power`           | W    | power         | measurement         | 0        | Computed as CH2 voltage × current. |
| `total_dc_power`         | W    | power         | measurement         | 0        | CH1 + CH2 DC power. |
| `ac_power`               | W    | power         | measurement         | 0        | AC power delivered to the grid. |
| `grid_frequency`         | Hz   | frequency     | measurement         | 2        | Rejected (not published) outside the 45–55 Hz sanity range, or if the raw field reads `0xFFFF`. |
| `temperature`            | °C   | temperature   | measurement         | 0        | Inverter internal temperature. Rejected if raw value is 0 or ≥ 200. |
| `daily_energy`           | kWh  | energy        | total_increasing    | 3        | CH1 + CH2 session energy; resets each morning when the DSP restarts its internal counter. |
| `ch1_session_energy`     | kWh  | energy        | total_increasing    | 3        | CH1 energy since the day's DSP boot. |
| `ch2_session_energy`     | kWh  | energy        | total_increasing    | 3        | CH2 energy since the day's DSP boot. |
| `lifetime_energy`        | kWh  | energy        | total_increasing    | 3        | RAM-accumulated lifetime total (see [Lifetime energy persistence](#lifetime-energy-persistence) below). Icon: `mdi:lightning-bolt`. |
| `inverter_uptime`        | s    | —             | —                   | 0        | Inverter uptime since DSP boot. `entity_category: diagnostic`. |

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

| Key             | Range (default)     | Step (default) | Optimistic | Notes |
|------------------|----------------------|------------------|------------|-------|
| `power_limit`    | `min_value` 30 – `max_value` 800 W | 1   | No  | Sends an output power-limit command to the inverter. The displayed value only updates once the inverter echoes the new limit back in a subsequent status frame — it is **not** set optimistically. |
| `total_energy`   | `min_value` 0 – `max_value` 999999 kWh | 0.001 | Yes | Manual override/reset of the lifetime energy accumulator. `entity_category: config`, icon `mdi:lightning-bolt`. |

Both `min_value`, `max_value` and `step` can be overridden per-entity in YAML
if you want a narrower range, e.g.:

```yaml
    power_limit:
      name: "Power Limit"
      max_value: 600
```

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

## Frame protocol notes

- Request/response frames start with `0xFB 0xFB` and end with `0xFE 0xFE`,
  with a 16-bit sum checksum covering bytes 2 through `len+2`.
- The hub reads the UART byte-by-byte in `loop()`, buffering and
  resynchronizing on the `0xFB 0xFB` marker if a frame fails checksum or
  footer validation — it does not rely on ESPHome's `uart.debug` YAML block.
- `dc_voltage_divisor`, `dc_current_divisor` and `grid_frequency_divisor` are
  the only tunables; the rest of the byte layout (see the field map in the
  component's `ez1m.cpp`) is fixed by the EZ1-M's firmware.
