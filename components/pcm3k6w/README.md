# pcm3k6w

Native ESPHome component for the **PCM3K6W bidirectional AC/DC converter** (grid-tied
charger / discharger / off-grid inverter) controlled over a **CAN bus** (29-bit extended IDs,
250 kbps). It replaces a pure-YAML integration built from `canbus.send` actions and `on_frame`
lambdas by a proper C++ component with one entity platform per sub-directory.

```
 canbus (mcp2515, ...) <──► [ pcm3k6w hub ] ──► sensor / binary_sensor / text_sensor
                                   ▲  │
        switch / number / select ──┘  └──► button / output (float, 0..1)
```

## Features

- Hub + entities pattern (`Parented<T>`): one `pcm3k6w:` block, then any number of
  `platform: pcm3k6w` entities, each one optional.
- Non-blocking transmit queue: every write or query is queued and sent from `loop()` with a
  minimum gap between frames (`frame_gap`), instead of blocking `delay:` steps.
- Periodic polling of the configuration registers (`poll_interval`); the live measurements
  (feedback frames, running status, bus voltage) are pushed by the converter and decoded as
  they arrive.
- Boot sequence: program version and serial number are queried once, then both current
  limits are forced to 1.0 A and the grid mode is set to *charge*.
- All commands go through the **volatile** registers. The `set_default_eeprom` button is the
  only way to write the **EEPROM** registers.
- `number` and `select` entities are restored from flash and pushed to the converter at boot.
- Values written to a `number` are quantised to its configured `step`, whatever the source
  (the number itself or an `output`).
- Non-deprecated CAN receive API (`add_frame_callback()`, the payload is not copied).

## Directory layout

```
components/pcm3k6w/
├── __init__.py          # hub: canbus_id / address / poll_interval / frame_gap
├── pcm3k6w.h / .cpp     # CAN protocol, TX queue, decoding of the RX frames
├── sensor/              # voltages, currents, powers, temperatures, readbacks
├── binary_sensor/       # alarms and fault flags
├── switch/              # power, discharge_charge, manual_fan_control
├── number/              # charge/discharge limits, AC/DC protections and alarms, fan speed
├── select/              # grid mode, phase mode, frequency, AC voltage
├── button/              # stop, start, reset, set_default_eeprom
├── output/              # charging_current_output, discharging_current_output
├── text_sensor/         # program version, serial number, running mode, frequency
└── README.md
```

## Installation

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [pcm3k6w]
    refresh: 0s
```

The component depends on `canbus`; any CAN controller platform works (`mcp2515`,
`mcp2518fd`, ...).

## Hub configuration

```yaml
canbus:
  - platform: mcp2515
    id: my_mcp2515
    spi_id: pcm_spi
    cs_pin: GPIO10
    use_extended_id: true
    can_id: 0x0A
    bit_rate: 250kbps
    mode: NORMAL
    data_rate: '4MHz'

pcm3k6w:
  id: my_pcm3k6w
  canbus_id: my_mcp2515
  address: '01'
```

| Option          | Default | Description                                                                  |
|-----------------|---------|------------------------------------------------------------------------------|
| `id`            |         | Hub id, referenced by every platform through `pcm3k6w_id`.                   |
| `canbus_id`     |         | **Required.** The CAN bus the converter is connected to.                     |
| `address`       | `'01'`  | Module address, hex string (`'01'`, `'0A'`) or integer.                      |
| `poll_interval` | `30s`   | Period of the configuration registers polling.                               |
| `frame_gap`     | `100ms` | Minimum delay between two transmitted frames.                                |

Every platform takes `pcm3k6w_id` plus the optional entity keys listed below, and each key
accepts the usual options of its platform (`name`, `id`, `icon`, ...).

## Entities

| Platform        | Keys |
|-----------------|------|
| `sensor`        | `grid_voltage`, `inverter_voltage`, `bus_voltage`, `inverting_current`, `dc_voltage`, `dc_current`, `load_current`, `ac_power`, `dc_power`, `temperature_pfc`, `temperature_llc`, `running_mode_value`, `offgrid_frequency`, `frequency_setting_readback`, `ac_voltage_setting_readback`, `phase_mode_readback`, `charging_voltage_readback`, `charging_current_readback`, `discharging_current_readback`, `ac_overvoltage_protection_readback`, `ac_undervoltage_protection_readback`, `ac_overvoltage_alarm_readback`, `ac_undervoltage_alarm_readback`, `dc_overvoltage_protection_readback`, `dc_undervoltage_protection_readback`, `dc_overvoltage_alarm_readback`, `dc_undervoltage_alarm_readback`, `fan_speed_readback`, `grid_mode_readback` |
| `binary_sensor` | `battery_undervoltage_alarm`, `battery_overvoltage_alarm`, `grid_undervoltage_alarm`, `grid_overvoltage_alarm`, `fault_status`, `fault_soft_start_timeout`, `fault_bus_overvoltage`, `fault_bus_undervoltage`, `fault_inverter_overvoltage`, `fault_inverter_undervoltage`, `fault_inverter_rms_overcurrent`, `fault_inverter_fast_overcurrent`, `fault_dc_overvoltage`, `fault_dc_undervoltage`, `fault_dc_fast_overcurrent`, `fault_dc_rms_overcurrent`, `fault_load_overcurrent`, `fault_soft_start_short_circuit`, `fault_output_overload`, `fault_over_temperature`, `fault_fan_fault`, `fault_sync_signal_loss`, `fault_locked`, `fault_address_fault`, `fault_sn_repetition`, `fault_load_rms_overcurrent`, `fault_overload` |
| `switch`        | `power`, `discharge_charge`, `manual_fan_control` |
| `number`        | `charging_voltage`, `charging_current`, `discharging_current`, `ac_overvoltage_protection`, `ac_undervoltage_protection`, `ac_overvoltage_alarm`, `ac_undervoltage_alarm`, `dc_overvoltage_protection`, `dc_undervoltage_protection`, `dc_overvoltage_alarm`, `dc_undervoltage_alarm`, `fan_speed` |
| `select`        | `grid_mode`, `phase_mode_setting`, `frequency_setting`, `ac_voltage_setting` |
| `button`        | `stop`, `start`, `reset`, `set_default_eeprom` |
| `output`        | `charging_current_output`, `discharging_current_output` |
| `text_sensor`   | `program_version`, `sn_code`, `running_mode`, `inverter_frequency` |

### Switch semantics

`discharge_charge`: **ON = charge, OFF = discharge** (volatile register `0x38`). The
`grid_mode` select and the switch stay in sync with the mode read back from the converter.

### Outputs

`charging_current_output` and `discharging_current_output` are float outputs (`0.0 - 1.0`)
mapped linearly to `[min_current, max_current]` amperes (defaults 1.0 - 70.0 A for charge and
1.0 - 75.0 A for discharge) and written to the same volatile registers as the matching
`number`. They are meant to be driven by a `fan: platform: speed` or by a regulator such as
`offsr`.

```yaml
output:
  - platform: pcm3k6w
    pcm3k6w_id: my_pcm3k6w
    charging_current_output:
      id: charging_current_output
      min_current: 1.0
      max_current: 70.0
```

### `set_default_eeprom` button

Writes, in this order, three **EEPROM** registers:

1. `0x02` grid mode set to *charge*;
2. `0x31` charging voltage / current: voltage taken from `number.charging_voltage` (56.0 V if
   that number is not declared), current forced to 1.0 A;
3. `0x32` discharging voltage / current: same voltage, current forced to 1.0 A.

These registers are not polled. The voltage field of `0x32` has not been confirmed on real
hardware, so read it back before relying on it. EEPROM has a limited number of write cycles:
use this button for a one-time initial setup, not in automations.

## Full example

```yaml
substitutions:
  name: pcm3k6w

external_components:
  - source: github://SeByDocKy/myESPhome
    components: [pcm3k6w]
    refresh: 0s

spi:
  - id: pcm_spi
    clk_pin: GPIO12
    mosi_pin: GPIO11
    miso_pin: GPIO13

canbus:
  - platform: mcp2515
    id: my_mcp2515
    spi_id: pcm_spi
    cs_pin: GPIO10
    use_extended_id: true
    can_id: 0x0A
    bit_rate: 250kbps
    mode: NORMAL
    data_rate: '4MHz'

pcm3k6w:
  id: my_pcm3k6w
  canbus_id: my_mcp2515
  address: '01'

sensor:
  - platform: pcm3k6w
    pcm3k6w_id: my_pcm3k6w
    grid_voltage:
      name: ${name}_grid_voltage
    inverter_voltage:
      name: ${name}_inverter_voltage
    bus_voltage:
      name: ${name}_bus_voltage
    inverting_current:
      name: ${name}_inverting_current
    dc_voltage:
      name: ${name}_dc_voltage
    dc_current:
      name: ${name}_dc_current
    load_current:
      name: ${name}_load_current
    ac_power:
      name: ${name}_ac_power
    dc_power:
      name: ${name}_dc_power
    temperature_pfc:
      name: ${name}_temperature_pfc
    temperature_llc:
      name: ${name}_temperature_llc
    running_mode_value:
      name: ${name}_running_mode_value
    offgrid_frequency:
      name: ${name}_offgrid_frequency
    frequency_setting_readback:
      name: ${name}_frequency_setting_readback
    ac_voltage_setting_readback:
      name: ${name}_ac_voltage_setting_readback
    phase_mode_readback:
      name: ${name}_phase_mode_readback
    charging_voltage_readback:
      name: ${name}_charging_voltage_readback
    charging_current_readback:
      name: ${name}_charging_current_readback
    discharging_current_readback:
      name: ${name}_discharging_current_readback
    ac_overvoltage_protection_readback:
      name: ${name}_ac_overvoltage_protection_readback
    ac_undervoltage_protection_readback:
      name: ${name}_ac_undervoltage_protection_readback
    ac_overvoltage_alarm_readback:
      name: ${name}_ac_overvoltage_alarm_readback
    ac_undervoltage_alarm_readback:
      name: ${name}_ac_undervoltage_alarm_readback
    dc_overvoltage_protection_readback:
      name: ${name}_dc_overvoltage_protection_readback
    dc_undervoltage_protection_readback:
      name: ${name}_dc_undervoltage_protection_readback
    dc_overvoltage_alarm_readback:
      name: ${name}_dc_overvoltage_alarm_readback
    dc_undervoltage_alarm_readback:
      name: ${name}_dc_undervoltage_alarm_readback
    fan_speed_readback:
      name: ${name}_fan_speed_readback
    grid_mode_readback:
      name: ${name}_grid_mode_readback

binary_sensor:
  - platform: pcm3k6w
    pcm3k6w_id: my_pcm3k6w
    battery_undervoltage_alarm:
      name: ${name}_battery_undervoltage_alarm
    battery_overvoltage_alarm:
      name: ${name}_battery_overvoltage_alarm
    grid_undervoltage_alarm:
      name: ${name}_grid_undervoltage_alarm
    grid_overvoltage_alarm:
      name: ${name}_grid_overvoltage_alarm
    fault_status:
      name: ${name}_fault_status
    fault_soft_start_timeout:
      name: ${name}_fault_soft_start_timeout
    fault_bus_overvoltage:
      name: ${name}_fault_bus_overvoltage
    fault_bus_undervoltage:
      name: ${name}_fault_bus_undervoltage
    fault_inverter_overvoltage:
      name: ${name}_fault_inverter_overvoltage
    fault_inverter_undervoltage:
      name: ${name}_fault_inverter_undervoltage
    fault_inverter_rms_overcurrent:
      name: ${name}_fault_inverter_rms_overcurrent
    fault_inverter_fast_overcurrent:
      name: ${name}_fault_inverter_fast_overcurrent
    fault_dc_overvoltage:
      name: ${name}_fault_dc_overvoltage
    fault_dc_undervoltage:
      name: ${name}_fault_dc_undervoltage
    fault_dc_fast_overcurrent:
      name: ${name}_fault_dc_fast_overcurrent
    fault_dc_rms_overcurrent:
      name: ${name}_fault_dc_rms_overcurrent
    fault_load_overcurrent:
      name: ${name}_fault_load_overcurrent
    fault_soft_start_short_circuit:
      name: ${name}_fault_soft_start_short_circuit
    fault_output_overload:
      name: ${name}_fault_output_overload
    fault_over_temperature:
      name: ${name}_fault_over_temperature
    fault_fan_fault:
      name: ${name}_fault_fan_fault
    fault_sync_signal_loss:
      name: ${name}_fault_sync_signal_loss
    fault_locked:
      name: ${name}_fault_locked
    fault_address_fault:
      name: ${name}_fault_address_fault
    fault_sn_repetition:
      name: ${name}_fault_sn_repetition
    fault_load_rms_overcurrent:
      name: ${name}_fault_load_rms_overcurrent
    fault_overload:
      name: ${name}_fault_overload

switch:
  - platform: pcm3k6w
    pcm3k6w_id: my_pcm3k6w
    power:
      name: ${name}_power
    discharge_charge:
      name: ${name}_discharge_charge
    manual_fan_control:
      name: ${name}_manual_fan_control

number:
  - platform: pcm3k6w
    pcm3k6w_id: my_pcm3k6w
    charging_voltage:
      name: ${name}_charging_voltage
    charging_current:
      name: ${name}_charging_current
    discharging_current:
      name: ${name}_discharging_current
    ac_overvoltage_protection:
      name: ${name}_ac_overvoltage_protection
    ac_undervoltage_protection:
      name: ${name}_ac_undervoltage_protection
    ac_overvoltage_alarm:
      name: ${name}_ac_overvoltage_alarm
    ac_undervoltage_alarm:
      name: ${name}_ac_undervoltage_alarm
    dc_overvoltage_protection:
      name: ${name}_dc_overvoltage_protection
    dc_undervoltage_protection:
      name: ${name}_dc_undervoltage_protection
    dc_overvoltage_alarm:
      name: ${name}_dc_overvoltage_alarm
    dc_undervoltage_alarm:
      name: ${name}_dc_undervoltage_alarm
    fan_speed:
      name: ${name}_fan_speed

select:
  - platform: pcm3k6w
    pcm3k6w_id: my_pcm3k6w
    grid_mode:
      name: ${name}_grid_mode
    phase_mode_setting:
      name: ${name}_phase_mode_setting
    frequency_setting:
      name: ${name}_frequency_setting
    ac_voltage_setting:
      name: ${name}_ac_voltage_setting

button:
  - platform: pcm3k6w
    pcm3k6w_id: my_pcm3k6w
    stop:
      name: ${name}_stop
    start:
      name: ${name}_start
    reset:
      name: ${name}_reset
    set_default_eeprom:
      name: ${name}_set_default_eeprom

text_sensor:
  - platform: pcm3k6w
    pcm3k6w_id: my_pcm3k6w
    program_version:
      name: ${name}_program_version
    sn_code:
      name: ${name}_sn_code
    running_mode:
      name: ${name}_running_mode
    inverter_frequency:
      name: ${name}_inverter_frequency

output:
  - platform: pcm3k6w
    pcm3k6w_id: my_pcm3k6w
    charging_current_output:
      id: charging_current_output
    discharging_current_output:
      id: discharging_current_output
```

## Notes and limitations

- Phase mode and frequency changes only take effect after the converter is restarted.
- Several `platform: pcm3k6w` blocks of the same platform are not needed: declare all the
  entities of a platform in a single block.
- Compatibility: ESPHome 2026.11 or later is recommended (`add_frame_callback()`).

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
