# Anker SOLIX - `anker_solix`

Native ESPHome component for the **Anker SOLIX plug & play batteries** (Modbus): Solarbank Max AC, Max, XE AC, XE and
Solarbank 4 E5000 Pro. It is a pure-ESPHome transposition of the official Home Assistant integration
[ha-anker-solix-official](https://github.com/anker-charging/ha-anker-solix-official) (v1.5.0): same registers, same
scaling, same safety rules for the writes.

The component does not carry its own Modbus engine: it sits on top of the ESPHome **`modbus`** bus and
**`modbus_controller`** that you declare in the YAML (they poll the battery, send the writes, and handle the retries and
the offline detection). It does not care what is under the `modbus` bus, so the same component works:

- over **RS485 (Modbus RTU)**: `uart:` + `modbus:` (see `test_anker_solix.yaml`);
- over **Modbus TCP**, with the serial-over-TCP components of ESPHome PRs #20061 (`modbus_tcp_uart`) and #20198 (`tcp_uart`, and the `uart` changes it needs): only
  the transport blocks change (see `test_anker_solix_tcp.yaml`).

The Anker app lets you enable **either Modbus TCP or Modbus RTU** (Three-Party Control Settings), not both. Written for
ESPHome 2026.9.0.

## Status - read this first

- The register map comes from the official integration, which speaks Modbus TCP. Anker publishes no map for the RTU
  mode: this component **assumes the RTU mode serves the same registers**. Not validated on hardware.
- Serial parameters (baud rate, parity, stop bits, slave address) are the ones the Anker app shows: set them in
  `uart:` and `modbus_controller:`.
- **TCP:** the battery speaks standard Modbus TCP (MBAP framing). Check in the PR that `modbus_tcp_uart` really
  converts the frames of the `modbus` bus to MBAP; a plain RTU-over-TCP tunnel (frames with a CRC, as the serial
  gateways of other makers expect) will not be understood by the battery. The PR syntax used in `test_anker_solix_tcp.yaml`
  is the one announced for it and may change before it is merged.
- Start read-only (`auto_third_party_control: false`, no use of the setpoint) and compare the readings with the Anker
  app before writing anything.

## Supported models

| `model`         | Devices                                   | Status |
|-----------------|-------------------------------------------|--------|
| `max_ac`        | Solarbank Max AC, XE AC                   | Register map validated on hardware by the users of the HA integration (TCP) |
| `max`           | Solarbank Max, XE                         | Same map, not validated |
| `xe`            | XE                                        | Same map, not validated |
| `sb4_e5000_pro` | Solarbank 4 E5000 Pro                     | Same map, not validated |

All four models currently share one register map. The `product_name` text sensor identifies the exact device from
its serial number (characters 4-6 of a 16 digit serial number, 4-7 of a 17 digit one). The Solarbank 3, the Smart Meter
Gen 2 and the Smart Plug are not covered (not in the official integration, or not in this map).

## Configuration

RS485 (Modbus RTU):

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [anker_solix]

uart:
  id: uart_rs485
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600        # as shown by the Anker app
  stop_bits: 1
  parity: NONE

modbus:
  id: modbus_rs485
  uart_id: uart_rs485

modbus_controller:
  - id: anker_mbc
    address: 1                # slave address
    modbus_id: modbus_rs485
    update_interval: 5s
  # Optional: a second controller, same address, slower: it polls the slow-changing entities
  - id: anker_mbc_slow
    address: 1
    modbus_id: modbus_rs485
    update_interval: 60s

anker_solix:
  id: anker_hub
  model: max_ac
  modbus_controller_id: anker_mbc
  slow_modbus_controller_id: anker_mbc_slow   # optional
  auto_third_party_control: false
  max_charge_power: 800                       # optional limits (W), on top of what the battery reports
  max_discharge_power: 800
  setpoint_refresh: 0ms
  read_capability_mask: true
```

Modbus TCP (ESPHome PRs #20061 and #20198): replace the `uart:` and `modbus:` blocks by

```yaml
external_components:
  - source: github://pr#20061
    components: [modbus_tcp_uart]
    refresh: 1h
  - source: github://pr#20198
    components: [ble_nus, tcp_uart, uart, usb_cdc_acm, usb_uart]
    refresh: 1h
  - source: github://SeByDocKy/myESPhome
    components: [anker_solix]

tcp_uart:
  - id: anker_link
    host: 192.168.1.50
    port: 502

modbus_tcp_uart:
  - id: anker_uart
    tcp_uart_id: anker_link

modbus:
  - id: modbus_anker
    uart_id: anker_uart
    turnaround_time: 100ms
```

and keep the `modbus_controller:` and `anker_solix:` blocks as they are.

Hub options:

| Option | Default | |
|--------|---------|--|
| `model` | required | Register map, see above |
| `modbus_controller_id` | required | The `modbus_controller` that polls the battery |
| `slow_modbus_controller_id` | - | Second `modbus_controller` (same address, slower `update_interval`) for the slow entities (energy totals, versions, SOC limits...). Without it everything is polled by the main one. |
| `auto_third_party_control` | `false` | Select the "Third-Party Controlled" mode before a power setpoint is written, and again if the Anker app changed it |
| `max_charge_power` / `max_discharge_power` | - | Limits (W) of the power setpoint, on top of what the battery reports |
| `setpoint_refresh` | `0ms` | Re-send the last power setpoint after this long without a write (0 = never) |
| `read_capability_mask` | `true` | Read register 0x8007 (SOC limit capabilities). A firmware without it answers "Illegal data address" at each poll: set `false` then, the SOC limit functions are then assumed to be supported |

Retries, offline detection and the poll rate belong to the `modbus_controller` blocks; the spacing of the requests
belongs to the `modbus` block. Each entity block uses `anker_solix_id: anker_hub` (the hub id cannot be `anker_solix`: it would
clash with the component name). See `test_anker_solix.yaml` for a configuration with every entity.

## Complete example (every entity)

The entity blocks below can be used with either transport (RS485 or serial-over-TCP): they only need the
`anker_solix:` hub of the previous section. Every key is optional, keep the ones you need. The component has no
`switch` or `button` platform: the battery has no on/off command in its register map (the operating mode is a `select`,
the backup SOC function can only be read, it is enabled in the Anker app).

```yaml
substitutions:
  name: anker-solix

sensor:
  - platform: anker_solix
    anker_solix_id: anker_hub
    ems_mode_mask:
      name: ${name}_ems_mode_mask
    parallel_capability_mask:
      name: ${name}_parallel_capability_mask
    dc_channels:
      pv_power:
        name: ${name}_pv_power
      pcs_pv_power:
        name: ${name}_pcs_pv_power
      third_party_pv_power:
        name: ${name}_third_party_pv_power
      pv_total_generation:
        name: ${name}_pv_total_generation
    ac:
      output_power:
        name: ${name}_ac_output_power
      load_power:
        name: ${name}_load_power
      grid_import_power:
        name: ${name}_grid_import_power
      grid_export_power:
        name: ${name}_grid_export_power
    battery:
      soc:
        name: ${name}_battery_soc
      charging_power:
        name: ${name}_battery_charging_power
      discharging_power:
        name: ${name}_battery_discharging_power
      rated_energy:
        name: ${name}_battery_rated_energy
      total_charging_energy:
        name: ${name}_battery_total_charging_energy
      total_discharging_energy:
        name: ${name}_battery_total_discharging_energy
      max_charging_power:
        name: ${name}_battery_max_charging_power
      max_discharging_power:
        name: ${name}_battery_max_discharging_power

text_sensor:
  - platform: anker_solix
    anker_solix_id: anker_hub
    battery_status:
      name: ${name}_battery_status
    device_sn:
      name: ${name}_device_sn
    product_name:
      name: ${name}_product_name
    device_sw_version:
      name: ${name}_device_sw_version
    device_model:
      name: ${name}_device_model

binary_sensor:
  - platform: anker_solix
    anker_solix_id: anker_hub
    backup_soc_enable:
      name: ${name}_backup_soc_enable
    modbus_connection:
      name: ${name}_modbus_connection

number:
  - platform: anker_solix
    anker_solix_id: anker_hub
    battery_power_setpoint:
      id: anker_power_setpoint
      name: ${name}_battery_power_setpoint
    charging_limit_soc:
      name: ${name}_charging_limit_soc
    discharge_limit_soc:
      name: ${name}_discharge_limit_soc
    backup_reserve_soc:
      name: ${name}_backup_reserve_soc

select:
  - platform: anker_solix
    anker_solix_id: anker_hub
    operating_mode:
      name: ${name}_operating_mode

output:
  - platform: anker_solix
    anker_solix_id: anker_hub
    charge_power:
      id: anker_charge_power
    discharge_power:
      id: anker_discharge_power
```

Driving the battery from your own logic (the setpoint is only applied in the "Third-Party Controlled" mode, see below):

```yaml
# Fixed power, in W: charge < 0, discharge > 0
button:
  - platform: template
    name: ${name}_discharge_300w
    on_press:
      - number.set:
          id: anker_power_setpoint
          value: 300

# Or with the outputs (0.0 .. 1.0 of the maximum power), e.g. from a PID loop
interval:
  - interval: 5s
    then:
      - output.set_level:
          id: anker_discharge_power
          level: 0.25     # 25 % of the maximum discharge power
```

## Entities


Sensors are grouped under `dc_channels:` (PV), `ac:` and `battery:`; the two diagnostic masks stay flat.

### Sensors

| Key | Register | Type | Notes |
|-----|----------|------|-------|
| `dc_channels: pv_power` | 10002 + 10004 | calculated | PCS PV + third-party PV (the sources need not be declared) |
| `dc_channels: pcs_pv_power` | 10002 | S32, W | |
| `dc_channels: third_party_pv_power` | 10004 | S32, W | diagnostic |
| `battery: charging_power` | 10008 | S32, W | register < 0 |
| `battery: discharging_power` | 10008 | S32, W | register > 0 |
| `ac: load_power` | 10010 | S32, W | |
| `ac: grid_import_power` / `grid_export_power` | 10012 | S32, W | > 0 import, < 0 export |
| `battery: soc` | 10014 | U16, % | |
| `dc_channels: pv_total_generation` | 10018 | U32, kWh | x0.1 |
| `battery: max_charging_power` / `max_discharging_power` | 10036 / 10038 | S32, W | diagnostic |
| `ac: output_power` | 10208 | S32, W | |
| `battery: rated_energy` | 10250 | U32, kWh | x0.1 |
| `battery: total_charging_energy` / `total_discharging_energy` | 10262 / 10264 | U32, kWh | x0.1 |
| `ems_mode_mask` / `parallel_capability_mask` | 32774 / 32775 | U16 | diagnostic |

### Text sensors
`battery_status` (10001: Idle / Charging / Discharging / Sleep), `device_sn` (10100), `product_name` (derived from
the serial number), `device_sw_version` (10112), `device_model` (32768, part number).

### Binary sensors
`backup_soc_enable` (60003), `modbus_connection` (true while the battery answers).

### Numbers
| Key | Register | Range |
|-----|----------|-------|
| `battery_power_setpoint` | 10071 (S32, write-only) | -10000..10000 W, charge < 0, discharge > 0 |
| `charging_limit_soc` | 60000 | 80..100 % |
| `discharge_limit_soc` | 60001 | 0..20 % |
| `backup_reserve_soc` | 60002 | 0..100 % (only if the backup SOC function is enabled) |

### Select
`operating_mode` (10064): Self-Consumption, Time Of Use, Third-Party Controlled, Custom, Socket Overlay, Smart,
Dynamic Tariff. Modes the device does not support (bits of register 32774) are refused.

### Output
`charge_power` / `discharge_power`: float outputs 0..1 mapped to 0..max charge / discharge power. The request sent is
`discharge - charge`, so a PID loop can drive them directly.

## Power setpoint behaviour

- The battery only applies the setpoint in the **Third-Party Controlled** mode. Select it yourself, or set
  `auto_third_party_control: true` (the hub selects it before a write and again if the Anker app changed it).
- The setpoint is clamped to the limits the battery reports (10036 / 10038) and to `max_charge_power` /
  `max_discharge_power`. If the limits are not known yet, a warning is logged.
- After a write, values read back are ignored for 15 s so entities do not jump back while the battery applies it.

## SOC limit rules

Same as the official integration: a write is refused (with a log message) when the device capability mask
(32775) says it is unsupported, and, while the backup SOC function is enabled, when it breaks the ordering
`discharge limit <= backup reserve < charging limit`. If the firmware has no capability mask (Modbus exception 2),
the functions are assumed supported.

## Notes

- RS485 is a shared bus at a low baud rate: keep the `update_interval` reasonable (the battery is read in a handful
  of requests), and only one master should talk to the battery (disable the Home Assistant integration).
- Writes go before reads in the `modbus` queue; the values read back are ignored for 15 s after a write.

## Caveats

- RTU register map and serial settings unverified (see Status); TCP depends on the PR transport (see Status).
- The refresh / watchdog behaviour of the setpoint in Third-Party mode is not documented by Anker: if the battery
  falls back after some time, use `setpoint_refresh`.
- `registers.py` is generated by `tools/gen_registers.py` from the official integration's device files; the register
  definitions are (c) 2026 Anker Innovations, MIT License.

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

The register definitions in `registers.py` come from the
[official Anker SOLIX Home Assistant integration](https://github.com/anker-charging/ha-anker-solix-official)
(MIT License, Copyright (c) 2026 Anker Innovations).
