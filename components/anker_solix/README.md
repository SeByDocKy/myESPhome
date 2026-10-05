# Anker SOLIX (Modbus TCP)

Native ESPHome component for the **Anker SOLIX plug & play batteries** over **Modbus TCP**. It is a pure-ESPHome
transposition of the official Home Assistant integration
([ha-anker-solix-official](https://github.com/anker-charging/ha-anker-solix-official), v1.5.0): same registers, same
scaling, same safety rules for the writes.

It reuses the `modbustcp` (transport) and `modbustcp_controller` (read ranges, command queue, retries) components, like
`marstek` does.

## Supported models

| `model`         | Devices                                   | Status |
|-----------------|-------------------------------------------|--------|
| `max_ac`        | Solarbank Max AC, XE AC                   | Register map validated on hardware by the users of the HA integration |
| `max`           | Solarbank Max, XE                         | Same register map, not validated on hardware |
| `xe`            | XE                                        | Same register map, not validated on hardware |
| `sb4_e5000_pro` | Solarbank 4 E5000 Pro                     | Same register map, not validated on hardware |

All four models currently share one register map. The `product_name` text sensor identifies the exact device from
its serial number (characters 4-6 of a 16 digit serial number, 4-7 of a 17 digit one).
The Smart Meter Gen 2 and the Smart Plug of the official integration are not covered.

## Battery setup

1. In the Anker app: **Three-Party Control Settings -> Modbus TCP**, enable it and note the IP address.
2. Use a fixed IP (DHCP reservation). Port is 502, slave id 1.
3. **Only one Modbus client at a time** should talk to the battery: disable the Home Assistant integration when the
   ESP takes over.

## Configuration

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [modbustcp, modbustcp_controller, anker_solix]

modbustcp:
  id: modbus_tcp_1
  ip_address: 192.168.1.50
  ip_port: 502

anker_solix:
  id: anker
  model: max_ac
  modbustcp_id: modbus_tcp_1
  update_interval: 5s              # fast entities
  low_priority_interval: 60s       # slow entities (energy totals, versions, SOC limits, ...)
  auto_third_party_control: false  # select the "Third-Party Controlled" mode before writing a setpoint
  max_charge_power: 800            # optional limits (W), on top of what the battery reports
  max_discharge_power: 800
  setpoint_refresh: 0ms            # re-send the last setpoint after this long (0 = never)
```

Other hub options (inherited from the Modbus TCP controller): `command_throttle` (50 ms), `max_cmd_retries` (3),
`offline_skip_updates` (0).

See `test_anker_solix.yaml` for a configuration with every entity.

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

## Caveats

- Only the Max AC register map was validated by users of the official integration; the other models are unverified.
- The refresh / watchdog behaviour of the setpoint in Third-Party mode is not documented by Anker: if the battery
  falls back after some time, use `setpoint_refresh`.
- `registers.py` is generated by `tools/gen_registers.py` from the official integration's device files; re-run it
  to follow a new upstream version. The register definitions are (c) 2026 Anker Innovations, MIT License.
