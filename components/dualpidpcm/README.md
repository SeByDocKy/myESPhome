# dualpidpcm

Native ESPHome external component implementing a bidirectional PID
regulator for a 3600 W / 48 V PCM (Power Conversion Module) battery
converter driven over CAN bus.

The component drives a single physical converter that can either
**charge** or **discharge** a 51.2 V LFP battery, switching direction
electronically (`discharge_charge_switch_`) rather than through two
separate devices. It regulates a measured power (e.g. house
consumption/production balance) toward a setpoint using a PID loop,
with an asymmetric watt-based deadband/hysteresis, feed-forward
injection, startup-freeze protection, undervoltage lockout, and an
optional delayed power-off on entering standby.

It is the "V2 hardware" counterpart to the `dualpid` component (which
targets a Huawei R48 charger + Hoymiles HMS microinverter pair).

## Architecture

- `dualpidpcm.h` / `dualpidpcm.cpp` — the hub (`DUALPIDPCMComponent`):
  owns the PID state machine and all regulation logic, and exposes
  getters/setters consumed by the platform components below.
- `switch/` — one `.h`/`.cpp` pair per switch entity.
- `number/` — one `.h`/`.cpp` pair per number entity (tunable
  parameters, persisted to flash via ESPHome preferences).
- `sensor/` — a single `DUALPIDPCMSensor` platform exposing several
  optional sensor sub-entities.
- `binary_sensor/` — a single `DUALPIDPCMBinarySensor` platform
  exposing several optional binary-sensor sub-entities.

## Hub configuration (`dualpidpcm:`)

| Key | Type | Required | Description |
|---|---|---|---|
| `input_id` | `sensor` | yes | Power sensor to regulate (e.g. grid import/export power). Positive/negative convention combined with `reverse`. |
| `battery_voltage_id` | `sensor` | yes | Battery voltage sensor, used for the current→power thresholds and the undervoltage lockout hysteresis. |
| `charging_output_id` | `output.float` | yes | Float output driving the converter's charging duty (0.0–1.0), sent to the CAN driver. |
| `discharging_output_id` | `output.float` | yes | Float output driving the converter's discharging duty (0.0–1.0). |
| `discharge_charge_switch_id` | `switch` | yes | Electronic direction switch: ON = charge side selected, OFF = discharge side selected. |
| `onoff_switch_id` | `switch` | yes | General on/off switch for the converter itself. |
| `current_min_charging` | `float` (0–70 A) | no | Minimum charging current used to derive the charge-side stop threshold (`Pmin_charging`). |
| `current_min_discharging` | `float` (0–70 A) | no | Minimum discharging current used to derive the discharge-side stop threshold (`Pmin_discharging`). |

## Switch entities (`switch: - platform: dualpidpcm`)

| Key | Default restore | Description |
|---|---|---|
| `activation` | — | Master enable. When off, the converter is forced to a real stop (outputs zeroed, `onoff_switch_` off) and the state machine is held in IDLE. |
| `manual_override` | — | When on, `pid_update()` is skipped entirely — for manual/external control of the outputs. |
| `pid_mode` | — | When on, the PID output is computed from scratch each cycle (`tmp = 0`) instead of accumulating on the previous output. |
| `reverse` | — | Inverts the sign of the regulation error (`error_ = -error_`). |
| `feedforward` | OFF | Enables the feed-forward output jump on large, sudden error steps (see `calculate_ff_jump()` / `ff_table`), to speed up the PID's reaction to step changes in load. |
| `allow_charging` | ON | User gate on entering/staying in CHARGE mode. Forcing it off while charging triggers an immediate real stop (never a direct bascule to discharge). |
| `allow_discharging` | ON | User gate on entering/staying in DISCHARGE mode. Combined with the undervoltage lockout via `discharge_gate()` — charging is never blocked, since it's the only way to recover from lockout. |

## Number entities (`number: - platform: dualpidpcm`)

| Key | Unit | Range | Step | Description |
|---|---|---|---|---|
| `setpoint` | W | -400 – 400 | 5 | Target power for the regulation loop. |
| `starting_battery_voltage` | V | 50.0 – 60.0 | 0.1 | Upper threshold of the undervoltage-lockout hysteresis: battery voltage must reach this value to clear the discharge lockout. |
| `stopping_battery_voltage` | V | 49.5 – 60.0 | 0.1 | Lower threshold: below this, `undervoltage_lockout_` engages and discharging is inhibited (charging stays allowed). |
| `kp` | — | 0 – 20 | 0.1 | PID proportional gain. |
| `ki` | — | 0 – 10 | 0.1 | PID integral gain. |
| `kd` | — | 0 – 10 | 0.1 | PID derivative gain. |
| `output_min_charging` | % | 0 – 100 | 1 | Minimum allowed charging duty (clamps the charging-side output). |
| `output_max_charging` | % | 0 – 100 | 1 | Maximum allowed charging duty. |
| `output_min_discharging` | % | 0 – 100 | 1 | Minimum allowed discharging duty. |
| `output_max_discharging` | % | 0 – 100 | 1 | Maximum allowed discharging duty. |
| `feedforward_threshold` | W | 0 – 1000 | 50 | Minimum error step (`|delta_error|`) required to trigger a feed-forward output jump (only used when the `feedforward` switch is on). |
| `self_consumption` | W | 0 – 50 | 1 | Converter's own idle power draw while discharging. Added to `Pmin_discharging` so discharge only starts once house consumption exceeds what the converter itself consumes. |
| `delta_idle_charging` | W | 0 – 100 | 1 | Anti-cycling margin (W) added on top of the charging stop threshold before charging is allowed to restart (`Pstart_charging`). |
| `delta_idle_discharging` | W | 0 – 100 | 1 | Same anti-cycling margin for the discharge side (`Pstart_discharging`). |
| `timer_standby_poweroff` | s | 0 – 120 | 1 | Grace period before `onoff_switch_` is actually turned off after entering standby/deadband from an active mode. `0` (default) = immediate cut, unchanged legacy behavior. If the converter needs to charge/discharge again before the timer elapses, power was never cut and no startup freeze (`STARTUP_INHIBIT_MS`) is re-armed. Rendered as a slider. |

All number entities persist their last value across reboots (ESPHome
preferences / NVS), falling back to the C++-side default if nothing
was stored yet.

## Sensor entities (`sensor: - platform: dualpidpcm`)

| Key | Unit | Description |
|---|---|---|
| `error` | W | Current regulation error (`input - setpoint`, sign-flipped by `reverse`). |
| `output` | % | Raw internal PID output position (0–100%, centered on `oneutral_` = 50%: below neutral = charging side, above = discharging side). |
| `output_charging` | % | Charging duty actually sent to `charging_output_id` (0 outside CHARGE mode). |
| `output_discharging` | % | Discharging duty actually sent to `discharging_output_id` (0 outside DISCHARGE mode). |
| `input` | W | Raw value of the regulated power sensor. |
| `mode` | — | Current state machine mode as a number: `0` = IDLE, `1` = CHARGE, `2` = DISCHARGE. |

## Binary sensor entities (`binary_sensor: - platform: dualpidpcm`)

| Key | Description |
|---|---|
| `deadband` | `true` while the converter is idle/in standby (stable IDLE mode and activated). Reflects the hysteretic mode, not the raw instantaneous watt-based deadband test, to avoid flicker near the stop thresholds. |
| `undervoltage_lockout` | `true` while the battery voltage is below `stopping_battery_voltage` (or hasn't yet recovered above `starting_battery_voltage`). Discharging is inhibited while this is set; charging remains allowed. |
| `timer_standby` | `true` from the moment the converter enters standby with `timer_standby_poweroff` grace pending (`onoff_switch_` still on, countdown running), back to `false` as soon as the grace ends — either because `onoff_switch_` was actually cut once the timer elapsed, or because charging/discharging resumed before that. Diagnostic aid to watch the feature live from Home Assistant instead of the logs. |

## Example

```yaml
dualpidpcm:
  - id: pcm
    input_id: house_power
    battery_voltage_id: battery_voltage
    charging_output_id: pcm_charging_output
    discharging_output_id: pcm_discharging_output
    discharge_charge_switch_id: pcm_discharge_charge_switch
    onoff_switch_id: pcm_onoff_switch
    current_min_charging: 2.0
    current_min_discharging: 2.0

switch:
  - platform: dualpidpcm
    dualpidpcm_id: pcm
    activation:
      name: "PCM Activation"
    allow_charging:
      name: "PCM Allow Charging"
    allow_discharging:
      name: "PCM Allow Discharging"

number:
  - platform: dualpidpcm
    dualpidpcm_id: pcm
    setpoint:
      name: "PCM Setpoint"
    kp:
      name: "PCM Kp"
    timer_standby_poweroff:
      name: "PCM Standby Power-off Delay"

sensor:
  - platform: dualpidpcm
    dualpidpcm_id: pcm
    error:
      name: "PCM Error"
    mode:
      name: "PCM Mode"

binary_sensor:
  - platform: dualpidpcm
    dualpidpcm_id: pcm
    deadband:
      name: "PCM Deadband"
    undervoltage_lockout:
      name: "PCM Undervoltage Lockout"
    timer_standby:
      name: "PCM Standby Timer Running"
```
