# Deye single-phase hybrid inverter - `deye_mono`

Native ESPHome component for the **Deye single-phase hybrid inverters** (SUN-xK-SG0x-LP1 family and their rebrands),
over Modbus RTU / RS485. It is a native C++ transposition of a set of `modbus_controller` YAML files (a "nominal" and an
"advanced" one): same registers, same conversions, **167 entities** in six platforms (sensor, binary_sensor, switch,
number, select, text_sensor).

The component does not carry its own Modbus engine: it sits on top of the ESPHome **`modbus`** bus and
**`modbus_controller`** that you declare in the YAML. They poll the inverter, send the writes, and handle the retries
and the offline detection. Because of that:

- there is **one polling cadence, the `update_interval` of the `modbus_controller`**; `deye_mono` has none;
- you may point several hubs at several controllers if you ever want different speeds (`MULTI_CONF`).

Written for ESPHome 2026.9.0. The total-energy part (`total_daily_energy` sensors, the "yesterday" values and the
globals they use) is **not** part of the component: it stays in ESPHome YAML (see `test_deye_mono.yaml`).

## Configuration

```yaml
uart:
  id: uart_inverter
  tx_pin: GPIO17
  rx_pin: GPIO18
  baud_rate: 9600

modbus:
  id: modbus_inverter
  uart_id: uart_inverter

modbus_controller:
  - id: deye_controller
    modbus_id: modbus_inverter
    address: 1
    update_interval: 10s        # the one and only polling cadence

deye_mono:
  - id: deye
    modbus_controller_id: deye_controller
    inverter_factor: 1.0
```

| Option | Default | Description |
|---|---|---|
| `modbus_controller_id` | required | The controller that polls the inverter |
| `inverter_factor` | `1.0` | Multiplier of every power value (the former `${inverter_factor}`), folded into the conversion at code generation |
| `use_write_multiple` | `true` | Write with function 0x10 (one register) instead of 0x06 |
| `write_hold` | `5s` | After a write, the value read for that register is ignored for this long (see *Writes*) |

Each platform block points at the hub with `deye_mono_id:` (optional when there is a single hub) and then lists the
entities it wants, each with its usual ESPHome options (`name`, `id`, `filters`, `disabled_by_default`...):

```yaml
sensor:
  - platform: deye_mono
    battery_voltage:
      name: ${name}_battery_voltage
    grid_power:
      name: ${name}_grid_power
      id: grid_power

switch:
  - platform: deye_mono
    low_power_mode:
      name: ${name}_low_power_mode
```

Only the entities you list are created, and **only their registers are read**: the controller builds its read ranges
from them. `test_deye_mono.yaml` lists every entity.

## Read frames

With every entity declared, the inverter is read with **9 frames, 193 registers** per cycle (measured against a
simulated inverter): 13-14, 28-48, 55-62, 70-97, 108-114, 150-194, 201-230, 243-293 and 326. The registers that sit
alone between two ranges are joined to the previous range on purpose (`bridge` in `registers.py`, the former
`reuse_previous_range: true`). The bridge is applied per register address, for all the entities of that address.

## Writes

- **Shared registers.** Several switches and selects live in the same register (28, 247, 248, 274-279, 280, 326). A
  write changes **only the bits of its own entity** (read-modify-write on a copy of the register as last read or
  written); the stock modbus switch writes the whole register, zeroing the neighbouring bits. A write is refused (and
  logged) while the register has not been read once.
- **Selects with a mask.** `select_charge_timezone1..6` use bits 0-3 of registers 274-279 (the bits 4 and above keep
  their value), `tou_jours_semaine` bits 0-7 of register 248, `grid_peak_shaving` bit 8 of register 280.
- **Hold-off.** After a write the value read for that register is ignored for `write_hold`, so that a poll already under
  way when the write was sent does not undo the new value in Home Assistant. The read-only entities (sensors, binary
  sensors) are never held: they are the feedback of what the inverter reports.
- Writing the same value twice in a row, while the first write is still held, sends nothing the second time.

## Calculated sensors

9 sensors are computed in C++ from registers the component reads itself (the sensors they use do not have to be
declared). They are computed **once per poll**, after the whole poll has been parsed, and published when the value
changed (add `force_update: true` to publish every cycle). They replace the former template sensors polled every
`inverter_template_update`.

## Entities

Keys are the names of the entities of the former YAML (minus the `${name}_${inverter_name}_` prefix), typos included
(`battery_shuntdown_voltage`). The conversion column is applied in this order: wrap, add, scale, `inverter_factor`.

### Sensors (81)

| Key | Register | Unit | Conversion |
|---|---|---|---|
| `modbus_address` | 37 (U_WORD) |  | +1 |
| `limiter` | 55 (U_WORD) | W | x inverter_factor |
| `gen_energy_today` | 62 (U_WORD) | kWh | x0.1 |
| `battery_charging_energy_today` | 70 (U_WORD) | kWh | x0.1 |
| `battery_discharging_energy_today` | 71 (U_WORD) | kWh | x0.1 |
| `battery_charging_total` | 72 (U_DWORD_R) | kWh | x0.1 |
| `battery_discharging_total` | 74 (U_DWORD_R) | kWh | x0.1 |
| `grid_energy_imported_today` | 76 (U_WORD) | kWh | x0.1 |
| `grid_energy_exported_today` | 77 (U_WORD) | kWh | x0.1 |
| `grid_energy_imported_total` | 78 (U_WORD) | kWh | x0.1 |
| `grid_frequency` | 79 (U_WORD) | Hz | x0.01 |
| `grid_energy_exported_total` | 81 (U_WORD) | kWh | x0.1 |
| `load_energy_today` | 84 (U_WORD) | kWh | x0.1 |
| `load_energy_total` | 85 (U_DWORD_R) | kWh | x0.1 |
| `dc_transformer_temperature` | 90 (S_WORD) | °C | -1000 x0.1 |
| `dc_radiator_temperature` | 91 (S_WORD) | °C | -1000 x0.1 |
| `pv_energy_total` | 96 (U_DWORD_R) | kWh | x0.1 |
| `pv_energy_today` | 108 (U_WORD) | kWh | x0.1 |
| `pv1_voltage` | 109 (U_WORD) | V | x0.1 |
| `pv1_current` | 110 (U_WORD) | A | x0.1 |
| `pv2_voltage` | 111 (U_WORD) | V | x0.1 |
| `pv2_current` | 112 (U_WORD) | A | x0.1 |
| `pv3_voltage` | 113 (U_WORD) | V | x0.1 |
| `pv3_current` | 114 (U_WORD) | A | x0.1 |
| `grid_voltage` | 150 (S_WORD) | V | x0.1 |
| `ac_output_voltage` | 154 (U_WORD) | V | x0.1 |
| `ac_output_current` | 164 (S_WORD) | A | x0.01 |
| `aux_output_power` | 166 (S_WORD) | W | x inverter_factor |
| `grid_power_167` | 167 (S_WORD) | W | x inverter_factor |
| `grid_power` | 169 (S_WORD) | W | x inverter_factor |
| `grid_external_power` | 172 (S_WORD) | W | x inverter_factor |
| `ac_output_power` | 175 (S_WORD) | W | - |
| `load_power` | 178 (S_WORD) | W | x inverter_factor |
| `battery_temperature` | 182 (U_WORD) | °C | -1000 x0.1 |
| `battery_voltage` | 183 (U_WORD) | V | x0.01 |
| `battery_soc` | 184 (U_WORD) | % | - |
| `pv1_power` | 186 (U_WORD) | W | x inverter_factor |
| `pv2_power` | 187 (U_WORD) | W | x inverter_factor |
| `pv3_power` | 188 (U_WORD) | W | x inverter_factor |
| `battery_power` | 190 (S_WORD) | W | - |
| `battery_current` | 191 (S_WORD) | A | x0.01 x inverter_factor negated |
| `load_frequency` | 192 (U_WORD) | Hz | x0.01 |
| `ac_output_frequency` | 193 (U_WORD) | Hz | x0.01 |
| `grid_connexion` | 194 (U_WORD) |  | - |
| `battery_equalization_voltage` | 201 (U_WORD) | V | x0.01 |
| `battery_absorption_voltage` | 202 (U_WORD) | V | x0.01 |
| `battery_float_voltage` | 203 (U_WORD) | V | x0.01 |
| `battery_max_charge_current` | 210 (U_WORD) | A | - |
| `battery_max_discharge_current` | 211 (U_WORD) | A | - |
| `battery_capacity_shutdown` | 217 (U_WORD) | % | - |
| `battery_shutdown_voltage` | 220 (U_WORD) | V | x0.01 |
| `battery_restart_voltage` | 221 (U_WORD) | V | x0.01 |
| `battery_low_voltage` | 222 (U_WORD) | V | x0.01 |
| `grid_charge_battery_current` | 230 (U_WORD) | A | - |
| `grid_peak_shaving_power` | 293 (U_WORD) | W | - |
| `firmware_control_board` | 13 (U_WORD) |  | - |
| `firmware_comms_board` | 14 (U_WORD) |  | - |
| `setting_timezone1` | 250 (U_WORD) |  | wrap |
| `setting_timezone2` | 251 (U_WORD) |  | wrap |
| `setting_timezone3` | 252 (U_WORD) |  | wrap |
| `setting_timezone4` | 253 (U_WORD) |  | - |
| `setting_timezone5` | 254 (U_WORD) |  | wrap |
| `setting_timezone6` | 255 (U_WORD) |  | wrap |
| `power_timezone1` | 256 (U_WORD) | W | x inverter_factor |
| `power_timezone2` | 257 (U_WORD) | W | x inverter_factor |
| `power_timezone3` | 258 (U_WORD) | W | x inverter_factor |
| `power_timezone4` | 259 (U_WORD) | W | x inverter_factor |
| `power_timezone5` | 260 (U_WORD) | W | x inverter_factor |
| `power_timezone6` | 261 (U_WORD) | W | x inverter_factor |
| `battery_voltage_timezone1` | 262 (U_WORD) | V | x0.01 |
| `battery_voltage_timezone2` | 263 (U_WORD) | V | x0.01 |
| `battery_voltage_timezone3` | 264 (U_WORD) | V | x0.01 |
| `battery_voltage_timezone4` | 265 (U_WORD) | V | x0.01 |
| `battery_voltage_timezone5` | 266 (U_WORD) | V | x0.01 |
| `battery_voltage_timezone6` | 267 (U_WORD) | V | x0.01 |
| `setting_soc_timezone1` | 268 (U_WORD) | % | - |
| `setting_soc_timezone2` | 269 (U_WORD) | % | - |
| `setting_soc_timezone3` | 270 (U_WORD) | % | - |
| `setting_soc_timezone4` | 271 (U_WORD) | % | - |
| `setting_soc_timezone5` | 272 (U_WORD) | % | - |
| `setting_soc_timezone6` | 273 (U_WORD) | % | - |

### Calculated sensors (9)

| Key | Unit | Computed as |
|---|---|---|
| `battery_charging_current` | A | battery current when positive, else 0 |
| `battery_discharging_current` | A | battery current when negative (sign inverted), else 0 |
| `battery_charging_power` | W | battery voltage x charging current x `inverter_factor` |
| `battery_discharging_power` | W | -(battery voltage x discharging current x `inverter_factor`) |
| `pv_power_total` | W | PV1 + PV2 + PV3 power |
| `essential_power` | W | AC output power + grid power (reg. 167) - aux output power |
| `essential_power_1` | W | AC output power + grid power (reg. 169) - aux output power |
| `nonessential_power` | W | grid external power - grid power (reg. 167) |
| `nonessential_power_1` | W | grid external power - grid power (reg. 169) |

### Binary sensors (18)

On when `register & mask` is not zero.

| Key | Register | Mask |
|---|---|---|
| `mppt_multipoint_scanning` | 28 | 0x0020 |
| `switch_on_off` | 43 | 0xFFFF |
| `island_protection_mode` | 46 | 0xFFFF |
| `mppt_number` | 47 | 0xFFFF |
| `GFDI_mode` | 48 | 0xFFFF |
| `grid_connected_status` | 194 | 0xFFFF |
| `setting_grid_charge_timezone1` | 274 | 0x0001 |
| `setting_grid_charge_timezone2` | 275 | 0x0001 |
| `setting_grid_charge_timezone3` | 276 | 0x0001 |
| `setting_grid_charge_timezone4` | 277 | 0x0001 |
| `setting_grid_charge_timezone5` | 278 | 0x0001 |
| `setting_grid_charge_timezone6` | 279 | 0x0001 |
| `setting_gen_charge_timezone1` | 274 | 0x0002 |
| `setting_gen_charge_timezone2` | 275 | 0x0002 |
| `setting_gen_charge_timezone3` | 276 | 0x0002 |
| `setting_gen_charge_timezone4` | 277 | 0x0002 |
| `setting_gen_charge_timezone5` | 278 | 0x0002 |
| `setting_gen_charge_timezone6` | 279 | 0x0002 |

### Switches (6)

| Key | Register | Mask |
|---|---|---|
| `low_power_mode` | 28 | 0x0004 |
| `mppt_multipoint_scanning` | 28 | 0x0020 |
| `low_noise_mode` | 34 | 0x0001 |
| `toggle_solar_sell` | 247 | 0x0001 |
| `toggle_force_generator` | 326 | 0x2000 |
| `toggle_system_timer` | 248 | 0x0001 |

### Numbers (34)

value = raw x scale; written as `round(value / scale)`.

| Key | Register | Range | Step | Unit | Scale |
|---|---|---|---|---|---|
| `battery_equalization_voltage` | 201 | 50 .. 61 | 0.1 | V | 0.01 |
| `battery_absorption_voltage` | 202 | 50 .. 61 | 0.1 | V | 0.01 |
| `battery_float_voltage` | 203 | 50 .. 61 | 0.1 | V | 0.01 |
| `battery_shuntdown_voltage` | 220 | 40 .. 52 | 0.1 | V | 0.01 |
| `battery_low_voltage` | 222 | 40 .. 52 | 0.1 | V | 0.01 |
| `battery_max_charge_current` | 210 | 5 .. 185 | 5 | A | 1 |
| `battery_max_discharge_current` | 211 | 0 .. 185 | 5 | A | 1 |
| `grid_charge_battery_current` | 230 | 0 .. 185 | 5 | A | 1 |
| `max_sell_power` | 245 | 0 .. 8000 | 500 | W | 1 |
| `grid_peak_shaving_power` | 293 | 0 .. 8000 | 500 | W | 1 |
| `set_timezone1` | 250 | 0 .. 2359 | 1 |  | 1 |
| `set_timezone2` | 251 | 0 .. 2359 | 1 |  | 1 |
| `set_timezone3` | 252 | 0 .. 2359 | 1 |  | 1 |
| `set_timezone4` | 253 | 0 .. 2359 | 1 |  | 1 |
| `set_timezone5` | 254 | 0 .. 2359 | 1 |  | 1 |
| `set_timezone6` | 255 | 0 .. 2359 | 1 |  | 1 |
| `set_power_timezone1` | 256 | 0 .. 8000 | 100 | W | 1 |
| `set_power_timezone2` | 257 | 0 .. 8000 | 100 | W | 1 |
| `set_power_timezone3` | 258 | 0 .. 8000 | 100 | W | 1 |
| `set_power_timezone4` | 259 | 0 .. 8000 | 100 | W | 1 |
| `set_power_timezone5` | 260 | 0 .. 8000 | 100 | W | 1 |
| `set_power_timezone6` | 261 | 0 .. 8000 | 100 | W | 1 |
| `set_battery_voltage_timezone1` | 262 | 45 .. 63 | 0.1 | V | 0.01 |
| `set_battery_voltage_timezone2` | 263 | 45 .. 63 | 0.1 | V | 0.01 |
| `set_battery_voltage_timezone3` | 264 | 45 .. 63 | 0.1 | V | 0.01 |
| `set_battery_voltage_timezone4` | 265 | 45 .. 63 | 0.1 | V | 0.01 |
| `set_battery_voltage_timezone5` | 266 | 45 .. 63 | 0.1 | V | 0.01 |
| `set_battery_voltage_timezone6` | 267 | 45 .. 63 | 0.1 | V | 0.01 |
| `set_soc_timezone1` | 268 | 0 .. 100 | 5 | % | 1 |
| `set_soc_timezone2` | 269 | 0 .. 100 | 5 | % | 1 |
| `set_soc_timezone3` | 270 | 0 .. 100 | 5 | % | 1 |
| `set_soc_timezone4` | 271 | 0 .. 100 | 5 | % | 1 |
| `set_soc_timezone5` | 272 | 0 .. 100 | 5 | % | 1 |
| `set_soc_timezone6` | 273 | 0 .. 100 | 5 | % | 1 |

### Selects (12)

| Key | Register | Mask | Options |
|---|---|---|---|
| `switch_on_off` | 43 | 0xFFFF | OFF = 0, ON = 1 |
| `battery_operate` | 213 | 0xFFFF | According to the voltage = 0, According to the capacity = 1, No battery = 2 |
| `energy_pattern` | 243 | 0xFFFF | Battery first = 0, Load first = 1 |
| `work_mode` | 244 | 0xFFFF | Selling First = 0, Zero Export + Limit to Load Only = 1, Limited to Home = 2 |
| `grid_peak_shaving` | 280 | 0x0100 | Disabled = 0, Enabled = 256 |
| `tou_jours_semaine` | 248 | 0x00FF | ❌ Désactivé = 0, ✅ Tous les jours = 255, Lun – Ven = 63, Sam – Dim = 193 |
| `select_charge_timezone1` | 274 | 0x000F | 16 options (Grid off, Gen off, GM off, BU off, CH off = 0 ... Grid on, Gen on, GM off, BU off, CH on = 15) |
| `select_charge_timezone2` | 275 | 0x000F | 16 options (Grid off, Gen off, GM off, BU off, CH off = 0 ... Grid on, Gen on, GM off, BU off, CH on = 15) |
| `select_charge_timezone3` | 276 | 0x000F | 16 options (Grid off, Gen off, GM off, BU off, CH off = 0 ... Grid on, Gen on, GM off, BU off, CH on = 15) |
| `select_charge_timezone4` | 277 | 0x000F | 16 options (Grid off, Gen off, GM off, BU off, CH off = 0 ... Grid on, Gen on, GM off, BU off, CH on = 15) |
| `select_charge_timezone5` | 278 | 0x000F | 16 options (Grid off, Gen off, GM off, BU off, CH off = 0 ... Grid on, Gen on, GM off, BU off, CH on = 15) |
| `select_charge_timezone6` | 279 | 0x000F | 16 options (Grid off, Gen off, GM off, BU off, CH off = 0 ... Grid on, Gen on, GM off, BU off, CH on = 15) |

### Text sensors (7)

| Key | Register | Shows |
|---|---|---|
| `overall_state` | 59 | standby / selftest / normal / alarm / fault |
| `time_slot_1` | 250 | start time `HH:MM` of the time-of-use slot |
| `time_slot_2` | 251 | start time `HH:MM` of the time-of-use slot |
| `time_slot_3` | 252 | start time `HH:MM` of the time-of-use slot |
| `time_slot_4` | 253 | start time `HH:MM` of the time-of-use slot |
| `time_slot_5` | 254 | start time `HH:MM` of the time-of-use slot |
| `time_slot_6` | 255 | start time `HH:MM` of the time-of-use slot |

## Differences with the YAML files

- **Shared bits** are no longer overwritten by a write (see *Writes*): the switches used to write their mask alone (or
  zero) to the whole register, the selects the whole option value.
- **Advanced voltage numbers.** `set_battery_voltage_timezone1..6` had `multiply: 0.01`. ESPHome 2026.9.0 *divides* the
  register by `multiply` for a number (`multiply: 100` of the nominal file is the right one), so the YAML reads 100x too
  much. Here the scale is simply 0.01 V per unit.
- **Battery power.** `battery_charging_power` and `battery_discharging_power` apply `inverter_factor` twice (once through
  the current, once through the power filter). This is kept as it was; with `inverter_factor: 1.0` it makes no
  difference. Edit `registers.py` (`BI` input) if you want it applied once.
- **Time-of-use registers 248-279** are bridged to the read range of 243-247 (in the former YAML the
  bridge flag of register 250 did not take effect: an unflagged entity of the register opened a range of its own).
- **`grid_peak_shaving_raw`** (an internal sensor of register 280 used for the select write) is gone: the hub keeps the
  register itself.
- `time_slot_1..6` read their register directly (they used to be template sensors on `setting_timezoneN`).
- Entities are only published when their value changed.

## Customising

`registers.py` is the whole entity catalogue (one dict per platform, documented in its header): add an entity there and
it appears in the platform schema, with no C++ to write for a plain register. The file was generated once from the two
YAML files and is meant to be edited by hand.

## Caveats

- Not validated on a real inverter yet. The C++ was compiled with ESPHome 2026.9.0 for the host platform and run against
  a simulated Modbus inverter (read frames, conversions, calculated sensors, masked writes).
- The firmware may refuse some registers or values; start by reading only, then write one entity at a time.
- Registers are read as holding registers (function 0x03).

## License

MIT License

Copyright (c) 2026 e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
