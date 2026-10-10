# minipid

Native ESPHome component implementing a small, runtime-tunable **PID controller**.
It reads any ESPHome `sensor`, computes a regulation output, and drives any
`output` of type `float` (`0.0 - 1.0`). Everything that matters (setpoint, gains,
output limits, mode flags) can be exposed as Home Assistant entities and is
**persisted in flash**, so the controller can be tuned live without reflashing.

Typical use case: power regulation, for example keeping a measured grid power
at a target by modulating the level of an inverter, a dimmer or a PWM/DAC output.

```
 sensor (input_id) ──► [ minipid ] ──► output (output_id, float 0..1)
                          ▲  │
   number / switch  ──────┘  └──► sensor (error, output, target)
```

## Features

- Event-driven: the PID is recomputed every time the input sensor publishes a new state
  (no `update_interval`); the time step `dt` is measured with `millis()`.
- Two modes selected by `pid_mode`: a **true PID** (positional form, switch on) or an
  **integral controller** (incremental form, switch off, where `Kp` acts as an integral gain).
- Reverse-acting mode (inverts the sign of the error).
- Configurable output limits (`output_min` / `output_max`, in %).
- Anti-windup: conditional integration plus a bound on the integral term.
- Manual override: freezes the controller so the output can be driven by something else.
- Activation switch: when off, the output is forced to `0` and the internal state is reset.
- Setpoint, gains, limits and switches are restored from flash after a reboot.
- `MULTI_CONF`: several independent controllers can coexist in the same node.
- All entities are optional; only declare the ones you need.

## Directory layout

```
components/minipid/
├── __init__.py          # hub: input_id / output_id
├── minipid.h / .cpp     # PID computation
├── sensor/              # error, output, target sensors
├── switch/              # activation, manual_override, pid_mode, reverse
├── number/              # setpoint, kp, ki, kd, output_min, output_max
├── test_minipid.yaml    # minimal self-contained test configuration
└── README.md
```

## Installation

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [minipid]
    refresh: 0s
```

The component declares a dependency on `time`, so a `time:` platform must be present
in the configuration (for instance `platform: homeassistant`).

## Control algorithm

Gain coefficients are internally scaled so that gains entered in the UI stay in a
comfortable `0 - 10` range:

| Term | Scale factor |
|------|--------------|
| P    | `0.001`      |
| I    | `0.0001`     |
| D    | `0.001`      |

At each update:

```
error      = input - setpoint            (sign inverted when `reverse` is on)
derivative = (error - previous_error) / dt
integral  += error * dt                  (with anti-windup, see below)

P = 0.001  * Kp * error
I = 0.0001 * Ki * integral
D = 0.001  * Kd * derivative

incremental mode (pid_mode OFF):  output = previous_output + P + I + D
positional  mode (pid_mode ON ):  output = P + I + D

output = clamp(output, output_min, output_max)
```

With the default (non-reversed) sign, the output **increases when the input is above
the setpoint**. Turn `reverse` on if your process acts the other way round.

### What `pid_mode` really selects

- **`pid_mode` ON: a "true" PID**, in the algorithmic sense. The output is computed
  from scratch at each step as `P + I + D`, so the proportional term acts
  as a genuine proportional action on the current error.
- **`pid_mode` OFF: an integral controller.** Because the previous output is added at
  each step (`output = previous_output + P + ...`), the output accumulates the error
  over time. The `Kp` term is then summed at every update, which is exactly what an
  integral action does: **in this mode, `Kp` should be interpreted as an integral
  gain** (the effective integral gain also depends on how often the input sensor
  updates). `Ki` and `Kd` then add further terms on top of that accumulation.

The internal output is a fraction in `[0.0, 1.0]` and is sent to the `output_id` with
`set_level()`. The `output_min`, `output_max` numbers and the `output` sensor are
expressed in **percent**. The output is only written when its value changes.

### Anti-windup

- The integral is not updated when the candidate output would exceed the limits
  while the error keeps pushing in the same direction. It can still unwind when
  the error changes sign.
- The integral is additionally bounded to
  `(output_max - output_min) / (0.0001 * |Ki|)`.

### Switch behaviour

| Switch            | Effect |
|-------------------|--------|
| `activation`      | **Off**: PID is not computed, the device output is forced to `0`, and integral/derivative/previous output are reset. **On**: regulation runs. |
| `manual_override` | **On**: the PID update is skipped entirely. The device output is left untouched (so you can drive it from elsewhere) and the sensors are not refreshed. |
| `pid_mode`        | **On**: true PID (positional output, `P + I + D`). **Off**: integral control (incremental output, the previous output is accumulated; `Kp` then behaves as an integral gain). See [What `pid_mode` really selects](#what-pid_mode-really-selects). |
| `reverse`         | Inverts the sign of the error. |

All four switches default to **off**, so after a first flash you need to turn
`activation` on to start regulating.

## Configuration

### Hub

```yaml
minipid:
  - id: my_minipid
    input_id: my_input_sensor
    output_id: my_output
```

| Option      | Type                  | Required | Description |
|-------------|-----------------------|----------|-------------|
| `id`        | ID                    | no       | ID of this controller (needed to reference it from the platforms below). |
| `input_id`  | ID of a `sensor`      | yes      | Process variable measured by the controller. |
| `output_id` | ID of a `FloatOutput` | yes      | Output driven by the controller (level `0.0 - 1.0`). |

### `sensor:` platform

```yaml
sensor:
  - platform: minipid
    minipid_id: my_minipid
    error:
      name: ${name}_regulation_error
    output:
      name: ${name}_output
```

| Option   | Description |
|----------|-------------|
| `error`  | Current regulation error (same unit as the input sensor). |
| `output` | Current controller output, in percent. |
| `target` | Reserved. Declared by the platform but **not populated yet** (it always reports `0`). |

All are standard ESPHome sensor options (`name`, `id`, `unit_of_measurement`,
`accuracy_decimals`, `filters`, ...).

### `switch:` platform

```yaml
switch:
  - platform: minipid
    minipid_id: my_minipid
    activation:
      name: ${name}_activation
    manual_override:
      name: ${name}_manual_override
    pid_mode:
      name: ${name}_pid_mode
    reverse:
      name: ${name}_reverse
```

Each switch is optional, belongs to the `config` entity category and keeps its
state across reboots.

### `number:` platform

```yaml
number:
  - platform: minipid
    minipid_id: my_minipid
    setpoint:
      name: ${name}_setpoint
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
```

| Option       | Range         | Step | Unit | Default | Description |
|--------------|---------------|------|------|---------|-------------|
| `setpoint`   | -4000 .. 4000 | 10   | W    | 0       | Target value for the input. |
| `kp`         | 0 .. 10       | 0.1  | -    | 4.0     | Proportional gain. |
| `ki`         | 0 .. 10       | 0.1  | -    | 0.0     | Integral gain. |
| `kd`         | 0 .. 10       | 0.1  | -    | 0.0     | Derivative gain. |
| `output_min` | 0 .. 100      | 1    | %    | 2       | Lower output limit while the PID is active. |
| `output_max` | 0 .. 100      | 1    | %    | 100     | Upper output limit. |

The defaults apply until a value has been written once; after that the stored value
is restored at boot. All numbers belong to the `config` entity category.

The `setpoint` entity is declared in watts (power device class). The controller
itself is unit-agnostic: only the scaling of the gains depends on the order of
magnitude of your input signal.

## Tuning hints

1. Choose the mode first. `pid_mode` off gives an integral controller (`kp` acts as the
   integral gain), which is usually the safest start; `pid_mode` on gives a true PID.
   Start with `ki = 0`, `kd = 0` and a moderate `kp`.
2. Make sure the sign is right: if the output runs away from the setpoint, toggle `reverse`.
3. Add `ki` slowly to remove the steady-state error.
4. Use `kd` only if the input is smooth enough; a noisy input is amplified by the derivative.
   A moving-average filter on the input sensor helps a lot.
5. Narrow `output_min` / `output_max` to the range the actuator can really follow.

## Full example

```yaml
esphome:
  name: test_minipid

esp32:
  board: esp32dev
  framework:
    type: esp-idf

external_components:
  - source: github://SeByDocKy/myESPhome
    components: [minipid]
    refresh: 0s

api:
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

sensor:
  # Process variable (replace with your real measurement)
  - platform: template
    id: my_input_sensor
    name: ${name}_input_power

  - platform: minipid
    minipid_id: my_minipid
    error:
      name: ${name}_regulation_error
    output:
      name: ${name}_output

minipid:
  - id: my_minipid
    input_id: my_input_sensor
    output_id: my_output

switch:
  - platform: minipid
    minipid_id: my_minipid
    activation:
      name: ${name}_activation
    manual_override:
      name: ${name}_manual_override
    pid_mode:
      name: ${name}_pid_mode
    reverse:
      name: ${name}_reverse

number:
  - platform: minipid
    minipid_id: my_minipid
    setpoint:
      name: ${name}_setpoint
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
```

A complete test configuration is available in
[`test_minipid.yaml`](test_minipid.yaml).

## Notes and limitations

- The `target` sensor is not computed yet and always reports `0`.
- The `time` dependency is required by the component declaration.
- While `manual_override` is on, sensors are not refreshed and the output is not touched.
- The PID is only evaluated when the input sensor publishes a state. An input that
  updates very rarely gives a correspondingly slow regulation, and a long `dt` makes
  the integral and derivative terms jump accordingly.

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
