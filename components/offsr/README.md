# offsr

Native ESPHome component implementing a **battery-current regulator** built around a
runtime-tunable PID. It reads the battery current (and voltage) from ESPHome sensors,
computes a regulation output and drives any `output` of type `float` (`0.0 - 1.0`).

The regulation target (a battery current, in amperes) depends on the battery voltage:
a *charging*, an *absorbing* or a *floating* setpoint is selected automatically. Every
parameter (setpoints, voltage thresholds, gains, output limits, mode flags) can be
exposed as a Home Assistant entity and is **persisted in flash**, so the regulator can be
tuned live without reflashing.

Typical use case: an off-grid solar installation where a surplus-power load (for example a
water heater driven by a dimmer/SSR) is modulated so that the battery current follows the
target of the current charge stage, with a thermostat-cut detection based on the power
actually consumed by the load.

```
 sensor (battery_current_id) ──┐
 sensor (battery_voltage_id) ──┼──► [ offsr ] ──► output (output_id, float 0..1)
 sensor (power_id, optional) ──┘      ▲  │
        number / switch / select ─────┘  ├──► sensor (error, output, target)
                                         └──► binary_sensor (thermostat_cut)
```

## Features

- Event-driven: the PID is recomputed every time the **battery current** sensor publishes
  a new state (no `update_interval`); the time step `dt` is measured with `millis()`.
- Three setpoints (`charging`, `absorbing`, `floating`) selected automatically from the
  battery voltage.
- Two modes selected by `pid_mode`: a **true PID** (positional form, switch on) or an
  **integral controller** (incremental form, switch off, where `Kp` acts as an integral gain).
- Reverse-acting mode (inverts the sign of the error).
- Configurable output limits (`output_min` / `output_max`) and restart level (`output_restart`).
- Under-voltage protection: the output is cut when the battery voltage falls below
  `starting_battery_voltage`.
- Thermostat-cut detection from an optional power sensor.
- `output_never_zero` option: keep the output at `output_min` instead of `0` when cut.
- `coeff_multiplier` select: coarse range selector (`10`, `1`, `0.1`, `0.01`) applied to the
  internal scaling of the gains.
- Manual override: freezes the regulator so the output can be driven by something else.
- Setpoints, thresholds, gains, limits, switches and the multiplier are restored from flash
  after a reboot.
- `MULTI_CONF`: several independent regulators can coexist in the same node.
- All entities are optional; only declare the ones you need.

## Directory layout

```
components/offsr/
├── __init__.py          # hub: battery_current_id / battery_voltage_id / output_id / power_id
├── offsr.h / .cpp       # regulation logic
├── sensor/              # error, output, target sensors
├── binary_sensor/       # thermostat_cut
├── switch/              # activation, output_never_zero, manual_override, pid_mode, reverse
├── number/              # setpoints, voltage thresholds, kp, ki, kd, output limits
├── select/              # coeff_multiplier
├── testoffsr.yaml       # minimal self-contained test configuration
└── README.md
```

## Installation

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [offsr]
    refresh: 0s
```

The component declares a dependency on `time`, so a `time:` platform must be present in
the configuration (for instance `platform: homeassistant`).

## Control algorithm

On every update of the battery current sensor:

1. **Target selection** (from the last battery voltage `V`):

   | Battery voltage                                         | Target current            |
   |---------------------------------------------------------|---------------------------|
   | `V < discharged_battery_voltage`                        | `charging_setpoint`       |
   | `discharged_battery_voltage <= V < charged_battery_voltage` | `absorbing_setpoint`  |
   | `V >= charged_battery_voltage`                          | `floating_setpoint`       |

2. **Error**: `error = battery_current - target` (sign inverted when `reverse` is on).
3. **PID terms** (`dt` in seconds):

   ```
   integral   += error * dt
   derivative  = (error - previous_error) / dt
   P = coeffP * m * Kp * error          coeffP = 0.001
   I = coeffI * m * Ki * integral       coeffI = 0.0001
   D = coeffD * m * Kd * derivative     coeffD = 0.001
   ```

   where `m` is the `coeff_multiplier` (default `1`).
4. **Output**: `output = clamp(base + P + I + D, output_min, output_max)`, where `base` is `0`
   in true-PID mode (`pid_mode` on) and the previous output in integral mode (`pid_mode`
   off, the default).
5. **Thermostat cut / restart**: if a `power_id` sensor is configured, the measured power is
   below 2 W and the previous output was at least `output_restart`, the output is set to
   `output_restart` and `thermostat_cut` is raised. Otherwise `thermostat_cut` is cleared.
   This lets the load restart gently after its own thermostat opened.
6. **Safety overrides**, applied last: the output is forced to `0` (or to `output_min` when
   `output_never_zero` is on) if `activation` is off, or if the battery voltage is below
   `starting_battery_voltage`.
7. The level is written to the output and the sensors / binary sensor are refreshed.

The battery voltage and the power sensor only update the stored values; they do **not**
trigger a computation by themselves.

### Switch behaviour

| Switch              | Effect |
|---------------------|--------|
| `activation`        | **Off** (default): the output is forced to `0` (or `output_min` with `output_never_zero`). Turn it on to start regulating. |
| `output_never_zero` | **On**: when the regulator is cut (activation off or low voltage), the output is held at `output_min` instead of `0`. Also applies at boot. |
| `manual_override`   | **On**: the whole computation is skipped; the output and the sensors are left untouched so something else can drive the output. |
| `pid_mode`          | **On**: true PID (positional output). **Off** (default): integral control (incremental output, the previous output is accumulated; `Kp` then behaves as an integral gain). |
| `reverse`           | **On**: inverts the sign of the error, for loads that act in the opposite direction. |

## Configuration

### Hub

```yaml
offsr:
  - id: my_offsr
    battery_current_id: battery_current
    battery_voltage_id: battery_voltage
    output_id: my_output
    power_id: load_power        # optional
```

| Option               | Type                  | Required | Description |
|----------------------|-----------------------|----------|-------------|
| `id`                 | ID                    | no       | ID of this regulator (needed to reference it from the platforms below). |
| `battery_current_id` | ID of a `sensor`      | yes      | Battery current in amperes. Each new value triggers a PID computation. |
| `battery_voltage_id` | ID of a `sensor`      | yes      | Battery voltage in volts. Used to choose the target and the under-voltage cut. |
| `output_id`          | ID of a `FloatOutput` | yes      | Output driven by the regulator (level `0.0 - 1.0`). |
| `power_id`           | ID of a `sensor`      | no       | Power consumed by the load, in watts. Enables the thermostat-cut / restart logic. |

### `sensor:` platform

```yaml
sensor:
  - platform: offsr
    offsr_id: my_offsr
    error:
      name: ${name}_regulation_error
    output:
      name: ${name}_output
    target:
      name: ${name}_target_current
```

| Option   | Unit | Description |
|----------|------|-------------|
| `error`  | A    | Current regulation error (`battery_current - target`). |
| `output` | -    | Current regulator output, as a level `0.0 - 1.0`. |
| `target` | A    | Currently selected target current (charging, absorbing or floating setpoint). |

All are standard ESPHome sensor options (`name`, `id`, `filters`, `unit_of_measurement`, ...).
They are refreshed after every computation.

### `binary_sensor:` platform

```yaml
binary_sensor:
  - platform: offsr
    offsr_id: my_offsr
    thermostat_cut:
      name: ${name}_thermostat_cut
```

| Option           | Description |
|------------------|-------------|
| `thermostat_cut` | On when the load draws less than 2 W while the output was at or above `output_restart` (the load's thermostat is considered open). Requires `power_id` on the hub; without it, it stays off. |

### `switch:` platform

```yaml
switch:
  - platform: offsr
    offsr_id: my_offsr
    activation:
      name: ${name}_activation
    output_never_zero:
      name: ${name}_output_never_zero
    manual_override:
      name: ${name}_manual_override
    pid_mode:
      name: ${name}_pid_mode
    reverse:
      name: ${name}_reverse
```

Each switch is optional, defaults to **off**, belongs to the `config` entity category and
keeps its state across reboots. See [Switch behaviour](#switch-behaviour).

### `number:` platform

```yaml
number:
  - platform: offsr
    offsr_id: my_offsr
    charging_setpoint:
      name: ${name}_charging_setpoint
    absorbing_setpoint:
      name: ${name}_absorbing_setpoint
    floating_setpoint:
      name: ${name}_floating_setpoint
    starting_battery_voltage:
      name: ${name}_starting_battery_voltage
    charged_battery_voltage:
      name: ${name}_charged_battery_voltage
    discharged_battery_voltage:
      name: ${name}_discharged_battery_voltage
    kp:
      name: ${name}_kp
    ki:
      name: ${name}_ki
    kd:
      name: ${name}_kd
    output_min:
      name: ${name}_output_min
    output_max:
      name: ${name}_output_max
    output_restart:
      name: ${name}_output_restart
```

| Option                       | Range       | Step | Unit | Default | Description |
|------------------------------|-------------|------|------|---------|-------------|
| `charging_setpoint`          | -2 .. 100   | 0.5  | A    | 20      | Target battery current when `V < discharged_battery_voltage`. |
| `absorbing_setpoint`         | -2 .. 20    | 0.2  | A    | 5       | Target battery current between the two voltage thresholds. |
| `floating_setpoint`          | -2 .. 2     | 0.2  | A    | 0       | Target battery current when `V >= charged_battery_voltage`. |
| `starting_battery_voltage`   | 45 .. 60    | 0.1  | V    | 53      | Under-voltage cut: below this voltage the output is cut. |
| `charged_battery_voltage`    | 50 .. 60    | 0.1  | V    | 55.8    | Voltage above which the floating setpoint applies. |
| `discharged_battery_voltage` | 45 .. 60    | 0.1  | V    | 55.6    | Voltage below which the charging setpoint applies. |
| `kp`                         | 0 .. 10     | 0.1  | -    | 4.0     | Proportional gain (integral gain in integral mode). |
| `ki`                         | 0 .. 10     | 0.1  | -    | 0.0     | Integral gain. |
| `kd`                         | 0 .. 10     | 0.1  | -    | 0.0     | Derivative gain. |
| `output_min`                 | 0 .. 1      | 0.01 | -    | 0.18    | Lower output limit while the regulator is active. |
| `output_max`                 | 0 .. 1      | 0.01 | -    | 0.85    | Upper output limit. |
| `output_restart`             | 0 .. 1      | 0.01 | -    | 0.4     | Output level used to restart the load after a thermostat cut. |

The defaults apply until a value has been written once; after that the stored value is
restored at boot. All numbers belong to the `config` entity category.

> Keep `discharged_battery_voltage <= charged_battery_voltage`, otherwise the absorbing
> stage is never selected.

### `select:` platform

```yaml
select:
  - platform: offsr
    offsr_id: my_offsr
    coeff_multiplier:
      name: ${name}_coeff_multiplier
```

| Option             | Options                  | Default | Description |
|--------------------|--------------------------|---------|-------------|
| `coeff_multiplier` | `10`, `1`, `0.1`, `0.01` | `1`     | Multiplier applied to the three internal scaling coefficients (`coeffP`, `coeffI`, `coeffD`), i.e. to the effective gains: `Kp`, `Ki` and `Kd` act as `coeff * multiplier * K`. It gives a coarse range selector on top of the fine `kp` / `ki` / `kd` numbers. |

The selected option is stored in flash and restored at boot. The entity belongs to the
`config` entity category.

## Tuning hints

1. Start in integral mode (`pid_mode` off), with `ki = 0`, `kd = 0` and a moderate `kp`.
   Use `coeff_multiplier` to move the whole gain range up or down by a decade.
2. Make sure the sign is right: if the battery current runs away from the target, toggle
   `reverse`.
3. Set the three setpoints and the two voltage thresholds to match your battery chemistry
   (the defaults suit a 48 V LiFePO4 pack), then set `starting_battery_voltage` just above
   the level at which you want the load to stop.
4. Narrow `output_min` / `output_max` to the range the load can really follow, and set
   `output_restart` to a level at which the load reliably restarts.
5. Add `ki` slowly to remove the steady-state error; use `kd` only if the current
   measurement is smooth (a moving-average filter on the sensor helps a lot).
6. Turn `activation` on last.

## Full example

```yaml
substitutions:
  name: testoffsr

esphome:
  name: ${name}

esp32:
  board: esp32dev
  framework:
    type: esp-idf

external_components:
  - source: github://SeByDocKy/myESPhome
    components: [offsr]
    refresh: 0s

logger:
api:
ota:
  - platform: esphome
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

time:
  - platform: homeassistant
    id: homeassistant_time

output:
  - platform: template
    id: my_output
    type: float
    write_action:
      - lambda: 'return;'

offsr:
  - id: my_offsr
    battery_current_id: battery_current
    battery_voltage_id: battery_voltage
    output_id: my_output
    power_id: load_power

sensor:
  # Measurements (replace with your real sensors, see the next example)
  - platform: template
    name: ${name}_battery_voltage
    id: battery_voltage
  - platform: template
    name: ${name}_battery_current
    id: battery_current
  - platform: template
    name: ${name}_load_power
    id: load_power

  - platform: offsr
    offsr_id: my_offsr
    error:
      name: ${name}_regulation_error
    output:
      name: ${name}_output
    target:
      name: ${name}_target_current

binary_sensor:
  - platform: offsr
    offsr_id: my_offsr
    thermostat_cut:
      name: ${name}_thermostat_cut

switch:
  - platform: offsr
    offsr_id: my_offsr
    activation:
      name: ${name}_activation
    output_never_zero:
      name: ${name}_output_never_zero
    manual_override:
      name: ${name}_manual_override
    pid_mode:
      name: ${name}_pid_mode
    reverse:
      name: ${name}_reverse

select:
  - platform: offsr
    offsr_id: my_offsr
    coeff_multiplier:
      name: ${name}_coeff_multiplier

number:
  - platform: offsr
    offsr_id: my_offsr
    charging_setpoint:
      name: ${name}_charging_setpoint
    absorbing_setpoint:
      name: ${name}_absorbing_setpoint
    floating_setpoint:
      name: ${name}_floating_setpoint
    starting_battery_voltage:
      name: ${name}_starting_battery_voltage
    charged_battery_voltage:
      name: ${name}_charged_battery_voltage
    discharged_battery_voltage:
      name: ${name}_discharged_battery_voltage
    kp:
      name: ${name}_kp
    ki:
      name: ${name}_ki
    kd:
      name: ${name}_kd
    output_min:
      name: ${name}_output_min
    output_max:
      name: ${name}_output_max
    output_restart:
      name: ${name}_output_restart
```

### Example with real measurements

Battery current and voltage from a Victron SmartShunt (VE.Direct on a UART) and the load
power from a PZEM-004T over Modbus, driving a dimmer through an I2C GPIO expander:

```yaml
uart:
  - id: uart_2
    tx_pin: GPIO18
    rx_pin: GPIO19
    baud_rate: 9600
    stop_bits: 2
  - id: uart_0
    tx_pin: GPIO01
    rx_pin: GPIO03
    baud_rate: 19200

modbus:
  id: modbus2
  uart_id: uart_2

victron_smart_shunt:
  uart_id: uart_0

i2c:
  - id: bus_i2c
    sda: GPIO22
    scl: GPIO21

sx1509:
  - id: sx1509_hub0
    address: 0x3E
    i2c_id: bus_i2c

output:
  - platform: sx1509
    sx1509_id: sx1509_hub0
    id: output_sx1509
    pin: 0
    inverted: true

sensor:
  - platform: victron_smart_shunt
    battery_voltage:
      name: smartshunt_battery_voltage
      id: smartshunt_battery_voltage
    battery_current:
      name: smartshunt_battery_current
      id: smartshunt_battery_current

  - platform: pzemac
    address: 1
    modbus_id: modbus2
    power:
      name: puissance
      id: pzem_power

offsr:
  - id: my_offsr
    battery_current_id: smartshunt_battery_current
    battery_voltage_id: smartshunt_battery_voltage
    output_id: output_sx1509
    power_id: pzem_power
```

A complete test configuration is available in [`testoffsr.yaml`](testoffsr.yaml); it uses
template sensors and numbers to simulate the battery and the load.

## Notes and limitations

- The regulation only runs when the **battery current** sensor publishes a state. The voltage
  and power sensors only update the stored values.
- The `output` sensor reports the level as `0.0 - 1.0`, not as a percentage.
- All switches default to off, so `activation` has to be turned on once before the output
  moves.
- While `manual_override` is on, the output and the sensors are not touched.
- The thermostat-cut logic only works with a `power_id` sensor; the 2 W detection threshold
  is fixed in the code.
- The `time` dependency is required by the component declaration.

## License

This component is released under the MIT License.

```
MIT License

Copyright (c) 2026 e-2-nomy

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
```
