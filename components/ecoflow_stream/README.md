# ecoflow_stream

Native ESPHome component for the **EcoFlow STREAM 5000** and **STREAM AC 5000** batteries over **local Modbus TCP** (real MBAP header): no cloud, no Home Assistant add-on.

> Status: **nothing has been tested on real hardware yet.** The component has only run against a Modbus TCP simulator (ESPHome host platform). Start read-only.

The register map is generated from the official EcoFlow integration [ha-ecoflow-official](https://github.com/ecoflow-oasis-official/ha-ecoflow-official) (MIT, see `LICENSE.ecoflow`).

## Supported models

| `model`         | Device         | Serial prefix |
|-----------------|----------------|---------------|
| `stream5000`    | STREAM 5000    | `ES21`        |
| `stream_ac5000` | STREAM AC 5000 | `ES22`        |

Both share the same register map. Older STREAM units are only reachable as part of a system that includes one of these two.

## Device prerequisites

- A firmware that offers local Modbus TCP (rolled out from 17 November 2026).
- EcoFlow app: device integration → third-party control → Modbus TCP. *View only* is enough to read; *View and control* is needed to write.
- A fixed IP address (DHCP reservation). Port `502`, Modbus address `1`.

## Wiring the Modbus TCP link

```
tcp_uart → modbus_tcp_uart → modbus → modbus_controller (fast + slow) → ecoflow_stream (hub) → platforms
```

`modbus_tcp_uart` is still an ESPHome pull request. At the time of writing, PRs #20061 and #20198 compile with ESPHome **2026.12.0-dev** and **not with 2026.11.1**.

```yaml
external_components:
  - source: github://pr#20061
    components: [modbus_tcp_uart]
    refresh: 1h
  - source: github://pr#20198
    components: [ble_nus, tcp_uart, uart, usb_cdc_acm, usb_uart]
    refresh: 1h
  - source: github://SeByDocKy/myESPhome
    components: [ecoflow_stream]

tcp_uart:
  - id: ecoflow_link
    host: 192.168.1.50
    port: 502

modbus_tcp_uart:
  - id: ecoflow_uart
    tcp_uart_id: ecoflow_link

modbus:
  - id: modbus_ecoflow
    uart_id: ecoflow_uart
    turnaround_time: 100ms

modbus_controller:
  - id: fast_modbus_controller
    address: 1
    modbus_id: modbus_ecoflow
    update_interval: 5s
  - id: slow_modbus_controller
    address: 1
    modbus_id: modbus_ecoflow
    update_interval: 60s
```

## Configuration

```yaml
ecoflow_stream:
  id: ecoflow_hub
  model: stream5000
  modbus_controller_id: fast_modbus_controller
  slow_modbus_controller_id: slow_modbus_controller   # optional

sensor:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    battery:
      soc:
        name: ${name}_battery_soc
      power:
        name: ${name}_battery_power
    ac:
      grid_power:
        name: ${name}_ac_grid_power
    dc_channels:
      pv0:
        power:
          name: ${name}_dc_channels_pv0_power

binary_sensor:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    system_master:
      name: ${name}_system_master
    modbus_connection:
      name: ${name}_modbus_connection
```

Every entity is listed in the [Entity reference](#entity-reference). Ready to use packages: `code/device_ecoflow_stream5000.yaml` and `code/device_ecoflow_stream_ac5000.yaml` (controls commented out).

### Hub options

| Option | Default | Meaning |
|---|---|---|
| `model` | required | `stream5000` or `stream_ac5000` |
| `modbus_controller_id` | required | controller of the fast entities |
| `slow_modbus_controller_id` | - | controller of the slow entities |
| `heartbeat_interval` | `20s` | keep-alive period (1 s to 50 s, `0ms` = never) |
| `ignore_master_flag` | `false` | publish and write system values even on a unit that is not the master |
| `charge_sign` | - | `negative` or `positive`, required by the `output` platform |
| `control_point` | `battery` | point driven by the outputs: `battery`, `inverter`, `grid_port` |
| `auto_control_point` | `false` | write the control point before a setpoint when it differs |
| `max_charge_power` / `max_discharge_power` | - | extra limits (W) of the outputs |
| `setpoint_refresh` | `0ms` | re-send the last setpoint after this delay |

## Entity reference

Every key of every platform, for both models. Declare only what you need. In the comments: wire address of the register, unit, range, and `system` for values of the system master only.

Declaring any `number`, `select`, `switch` or `output` starts the heartbeat, see [Heartbeat](#heartbeat).

<!-- entities:begin -->
### sensor (43)

```yaml
sensor:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    ac:
      inverter_power:  # 40023, W, system
        name: ${name}_ac_inverter_power
      home_power:  # 40519, W, system
        name: ${name}_ac_home_power
      grid_power:  # 40521, W, system
        name: ${name}_ac_grid_power
      max_output_power:  # 40546, W, system
        name: ${name}_ac_max_output_power
      max_input_power:  # 40548, W, system
        name: ${name}_ac_max_input_power
      frequency:  # 40594, Hz
        name: ${name}_ac_frequency
      grid_import_energy:  # 42161, kWh, system
        name: ${name}_ac_grid_import_energy
      grid_export_energy:  # 42177, kWh, system
        name: ${name}_ac_grid_export_energy
      home_energy:  # 44609, kWh, system
        name: ${name}_ac_home_energy
      phases:
        phase0:
          power:  # 44865, W, system
            name: ${name}_ac_phases_phase0_power
        phase1:
          power:  # 44867, W, system
            name: ${name}_ac_phases_phase1_power
        phase2:
          power:  # 44869, W, system
            name: ${name}_ac_phases_phase2_power
      bypass_power:  # 49218, W
        name: ${name}_ac_bypass_power
    utc_time:  # 40514
      name: ${name}_utc_time
    time_zone:  # 40518
      name: ${name}_time_zone
    dc_channels:
      power:  # 40523, W, system
        name: ${name}_dc_channels_power
      energy:  # 42257, kWh, system
        name: ${name}_dc_channels_energy
      pv0:
        power:  # 48453, W
          name: ${name}_dc_channels_pv0_power
      pv1:
        power:  # 48455, W
          name: ${name}_dc_channels_pv1_power
      pv2:
        power:  # 48457, W
          name: ${name}_dc_channels_pv2_power
      pv3:
        power:  # 48459, W
          name: ${name}_dc_channels_pv3_power
    battery:
      power:  # 40525, W, system
        name: ${name}_battery_power
      soc:  # 40527, %, system
        name: ${name}_battery_soc
      full_capacity:  # 40552, Wh, system
        name: ${name}_battery_full_capacity
      pack_count:  # 42081
        name: ${name}_battery_pack_count
      packs:
        pack0:
          soc:  # 42082, %
            name: ${name}_battery_pack0_soc
          soh:  # 48961, %
            name: ${name}_battery_pack0_soh
        pack1:
          soc:  # 42083, %
            name: ${name}_battery_pack1_soc
          soh:  # 48962, %
            name: ${name}_battery_pack1_soh
        pack2:
          soc:  # 42084, %
            name: ${name}_battery_pack2_soc
          soh:  # 48963, %
            name: ${name}_battery_pack2_soh
      charge_energy:  # 42225, kWh, system
        name: ${name}_battery_charge_energy
      discharge_energy:  # 42241, kWh, system
        name: ${name}_battery_discharge_energy
      charge_capability:  # 45121, W, system
        name: ${name}_battery_charge_capability
      discharge_capability:  # 45122, W, system
        name: ${name}_battery_discharge_capability
      min_cell_temperature:  # 48707, °C
        name: ${name}_battery_min_cell_temperature
      max_cell_temperature:  # 48709, °C
        name: ${name}_battery_max_cell_temperature
      max_cell_voltage:  # 48711, mV
        name: ${name}_battery_max_cell_voltage
      min_cell_voltage:  # 48712, mV
        name: ${name}_battery_min_cell_voltage
    system_fault_count:  # 42049, system
      name: ${name}_system_fault_count
    system_device_count:  # 44097, system
      name: ${name}_system_device_count
    system_online_device_count:  # 44099, system
      name: ${name}_system_online_device_count
    device_fault_count:  # 48209
      name: ${name}_device_fault_count
```

### binary_sensor (3)

```yaml
binary_sensor:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    system_master:  # 44098
      name: ${name}_system_master
    on_grid:  # 49217
      name: ${name}_on_grid
    modbus_connection:  # the device answers
      name: ${name}_modbus_connection
```

### text_sensor (10)

```yaml
text_sensor:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    system_phase_type:  # 40002, values: Three-phase, Single-phase
      name: ${name}_system_phase_type
    serial_number:  # 40004
      name: ${name}_serial_number
    firmware_version:  # 40012
      name: ${name}_firmware_version
    manufacturer:  # 40015
      name: ${name}_manufacturer
    system_group_id:  # 44101
      name: ${name}_system_group_id
    battery:
      packs:
        pack0:
          serial:  # 48977
            name: ${name}_battery_pack0_serial
        pack1:
          serial:  # 48985
            name: ${name}_battery_pack1_serial
        pack2:
          serial:  # 48993
            name: ${name}_battery_pack2_serial
    system_fault_codes:  # 42050, system
      name: ${name}_system_fault_codes
    device_fault_codes:  # 48210
      name: ${name}_device_fault_codes
```

### switch (2)

```yaml
switch:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    bypass_ac1:  # 49220
      name: ${name}_bypass_ac1
    heartbeat:  # keep-alive of the control mode, on by default
      name: ${name}_heartbeat
```

### number (18)

```yaml
number:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    battery:
      backup_reserve:  # 40536, %, 0 to 100, step 1, system
        name: ${name}_battery_backup_reserve
      soc_lower_limit:  # 48705, %, 0 to 100, step 1, system
        name: ${name}_battery_soc_lower_limit
      soc_upper_limit:  # 48706, %, 0 to 100, step 1, system
        name: ${name}_battery_soc_upper_limit
    breaker_capacity:  # 40540, A, 0 to 120, step 1, system
      name: ${name}_breaker_capacity
    led_brightness:  # 40541, %, 0 to 100, step 1
      name: ${name}_led_brightness
    host_election_priority:  # 44100, 0 to 65535, step 1
      name: ${name}_host_election_priority
    limits:
      battery_charge_power:  # 45633, W, 0 to 100000, step 1, system
        name: ${name}_limits_battery_charge_power
      battery_discharge_power:  # 45635, W, 0 to 100000, step 1, system
        name: ${name}_limits_battery_discharge_power
      meter_input_power:  # 45637, W, 0 to 100000, step 1, system
        name: ${name}_limits_meter_input_power
      meter_output_power:  # 45639, W, 0 to 100000, step 1, system
        name: ${name}_limits_meter_output_power
      inverter_output_power:  # 45641, W, 0 to 100000, step 1, system
        name: ${name}_limits_inverter_output_power
      inverter_input_power:  # 45643, W, 0 to 100000, step 1, system
        name: ${name}_limits_inverter_input_power
      grid_port_output_power:  # 45645, W, 0 to 100000, step 1, system
        name: ${name}_limits_grid_port_output_power
      grid_port_input_power:  # 45647, W, 0 to 100000, step 1, system
        name: ${name}_limits_grid_port_input_power
    targets:
      battery_power:  # 45650, W, -32768 to 32767, step 1, system
        name: ${name}_targets_battery_power
      meter_power:  # 45651, W, -32768 to 32767, step 1, system
        name: ${name}_targets_meter_power
      inverter_power:  # 45652, W, -32768 to 32767, step 1, system
        name: ${name}_targets_inverter_power
      grid_port_power:  # 45653, W, -32768 to 32767, step 1, system
        name: ${name}_targets_grid_port_power
```

### select (1)

```yaml
select:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    control_point:  # 45649, options: None, Battery point, Meter point, Inverter point, Grid port point, system
      name: ${name}_control_point
```

### output (2)

Needs `charge_sign` in the hub, see [Power control](#power-control).

```yaml
output:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    charge_power:  # 0.0 to 1.0 of the maximum charge power
      id: ecoflow_charge_power
    discharge_power:  # 0.0 to 1.0 of the maximum discharge power
      id: ecoflow_discharge_power
```
<!-- entities:end -->

## System master

In a multi-unit system the *system* values (see the table below) are only meaningful on the master. The hub reads the master flag and neither publishes nor writes them until the unit reports itself as master. After a boot, system values therefore appear from the second poll of their controller. The role can move to another unit.

## Heartbeat

As soon as a `number`, `select`, `switch` or `output` is declared, the hub writes the heartbeat register every `heartbeat_interval`. When the battery receives none for 60 s it returns to its own energy management. The `heartbeat` switch stops and restarts it:

```yaml
switch:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    heartbeat:
      name: ${name}_heartbeat
```

## Power control

`select.control_point` chooses which of the `targets.*_power` numbers the battery follows.

> **The sign of the target powers is not documented** by the official sources, and neither is the sign of `battery.power` and `ac.grid_power`. Check it first: select *Battery point*, write a small value (for example `100`) to `targets.battery_power`, and see whether the battery charges or discharges. Then set `charge_sign` accordingly.

The `output` platform is meant for a PID loop (zero injection). `0.0 … 1.0` maps to `0 … maximum power` reported by the device (further limited by `max_charge_power` / `max_discharge_power`); the setpoint sent is discharge minus charge.

```yaml
ecoflow_stream:
  id: ecoflow_hub
  model: stream5000
  modbus_controller_id: fast_modbus_controller
  charge_sign: negative        # CHECK on your device
  auto_control_point: true
  max_charge_power: 800
  max_discharge_power: 800

output:
  - platform: ecoflow_stream
    ecoflow_stream_id: ecoflow_hub
    charge_power:
      id: ecoflow_charge
    discharge_power:
      id: ecoflow_discharge
```

`breaker_capacity` and `host_election_priority` are installation settings: do not change them without knowing what they do.

## Limitations

- Not in the register map: battery voltage and current, cycle count, grid voltage, PV voltage and current.
- Adjacent registers are read together. A register that the firmware refuses makes its whole read range fail: remove the matching entity and report it.
- The `bypass_ac1` switch defaults to `restore_mode: DISABLED`: the AC output is not written at boot. The `heartbeat` switch is on by default.

## Registers

Holding registers (function code 03, written with 06 or 16), 32 bit values high word first. The address is the one sent on the wire (EcoFlow offset + 40001). The heartbeat (40608) is handled by the hub.

<!-- registers:begin -->
| Entity (YAML key) | Platform | Address | Type | Unit | Scope | Poll |
|---|---|---|---|---|---|---|
| `system_phase_type` | text_sensor | 40002 | U_WORD |  | unit | slow |
| `serial_number` | text_sensor | 40004-40011 | RAW |  | unit | slow |
| `firmware_version` | text_sensor | 40012-40013 | U_DWORD |  | unit | slow |
| `manufacturer` | text_sensor | 40015-40022 | RAW |  | unit | slow |
| `ac.inverter_power` | sensor | 40023 | S_WORD | W | system | fast |
| `utc_time` | sensor | 40514-40515 | U_DWORD |  | unit | slow |
| `time_zone` | sensor | 40518 | S_WORD |  | unit | slow |
| `ac.home_power` | sensor | 40519-40520 | FP32 | W | system | fast |
| `ac.grid_power` | sensor | 40521-40522 | FP32 | W | system | fast |
| `dc_channels.power` | sensor | 40523-40524 | FP32 | W | system | fast |
| `battery.power` | sensor | 40525-40526 | FP32 | W | system | fast |
| `battery.soc` | sensor | 40527 | U_WORD | % | system | fast |
| `battery.backup_reserve` | number | 40536 | U_WORD | % | system | slow |
| `breaker_capacity` | number | 40540 | U_WORD | A | system | slow |
| `led_brightness` | number | 40541 | U_WORD | % | unit | slow |
| `ac.max_output_power` | sensor | 40546-40547 | U_DWORD | W | system | slow |
| `ac.max_input_power` | sensor | 40548-40549 | U_DWORD | W | system | slow |
| `battery.full_capacity` | sensor | 40552-40553 | U_DWORD | Wh | system | slow |
| `ac.frequency` | sensor | 40594-40595 | FP32 | Hz | unit | fast |
| `system_fault_count` | sensor | 42049 | U_WORD |  | system | fast |
| `system_fault_codes` | text_sensor | 42050-42059 | RAW |  | system | fast |
| `battery.pack_count` | sensor | 42081 | U_WORD |  | unit | slow |
| `battery.packs.pack0.soc` | sensor | 42082 | U_WORD | % | unit | fast |
| `battery.packs.pack1.soc` | sensor | 42083 | U_WORD | % | unit | fast |
| `battery.packs.pack2.soc` | sensor | 42084 | U_WORD | % | unit | fast |
| `ac.grid_import_energy` | sensor | 42161-42162 | FP32 | kWh | system | fast |
| `ac.grid_export_energy` | sensor | 42177-42178 | FP32 | kWh | system | fast |
| `battery.charge_energy` | sensor | 42225-42226 | FP32 | kWh | system | fast |
| `battery.discharge_energy` | sensor | 42241-42242 | FP32 | kWh | system | fast |
| `dc_channels.energy` | sensor | 42257-42258 | FP32 | kWh | system | fast |
| `system_device_count` | sensor | 44097 | U_WORD |  | system | slow |
| `system_master` | binary_sensor | 44098 | U_WORD |  | unit | fast |
| `system_online_device_count` | sensor | 44099 | U_WORD |  | system | slow |
| `host_election_priority` | number | 44100 | U_WORD |  | unit | slow |
| `system_group_id` | text_sensor | 44101-44102 | U_DWORD |  | unit | slow |
| `ac.home_energy` | sensor | 44609-44610 | FP32 | kWh | system | fast |
| `ac.phases.phase0.power` | sensor | 44865-44866 | FP32 | W | system | fast |
| `ac.phases.phase1.power` | sensor | 44867-44868 | FP32 | W | system | fast |
| `ac.phases.phase2.power` | sensor | 44869-44870 | FP32 | W | system | fast |
| `battery.charge_capability` | sensor | 45121 | U_WORD | W | system | fast |
| `battery.discharge_capability` | sensor | 45122 | U_WORD | W | system | fast |
| `limits.battery_charge_power` | number | 45633-45634 | U_DWORD | W | system | slow |
| `limits.battery_discharge_power` | number | 45635-45636 | U_DWORD | W | system | slow |
| `limits.meter_input_power` | number | 45637-45638 | U_DWORD | W | system | slow |
| `limits.meter_output_power` | number | 45639-45640 | U_DWORD | W | system | slow |
| `limits.inverter_output_power` | number | 45641-45642 | U_DWORD | W | system | slow |
| `limits.inverter_input_power` | number | 45643-45644 | U_DWORD | W | system | slow |
| `limits.grid_port_output_power` | number | 45645-45646 | U_DWORD | W | system | slow |
| `limits.grid_port_input_power` | number | 45647-45648 | U_DWORD | W | system | slow |
| `control_point` | select | 45649 | U_WORD |  | system | slow |
| `targets.battery_power` | number | 45650 | S_WORD | W | system | slow |
| `targets.meter_power` | number | 45651 | S_WORD | W | system | slow |
| `targets.inverter_power` | number | 45652 | S_WORD | W | system | slow |
| `targets.grid_port_power` | number | 45653 | S_WORD | W | system | slow |
| `device_fault_count` | sensor | 48209 | U_WORD |  | unit | fast |
| `device_fault_codes` | text_sensor | 48210-48219 | RAW |  | unit | fast |
| `dc_channels.pv0.power` | sensor | 48453-48454 | FP32 | W | unit | fast |
| `dc_channels.pv1.power` | sensor | 48455-48456 | FP32 | W | unit | fast |
| `dc_channels.pv2.power` | sensor | 48457-48458 | FP32 | W | unit | fast |
| `dc_channels.pv3.power` | sensor | 48459-48460 | FP32 | W | unit | fast |
| `battery.soc_lower_limit` | number | 48705 | U_WORD | % | system | slow |
| `battery.soc_upper_limit` | number | 48706 | U_WORD | % | system | slow |
| `battery.min_cell_temperature` | sensor | 48707-48708 | FP32 | °C | unit | fast |
| `battery.max_cell_temperature` | sensor | 48709-48710 | FP32 | °C | unit | fast |
| `battery.max_cell_voltage` | sensor | 48711 | U_WORD | mV | unit | fast |
| `battery.min_cell_voltage` | sensor | 48712 | U_WORD | mV | unit | fast |
| `battery.packs.pack0.soh` | sensor | 48961 | U_WORD | % | unit | slow |
| `battery.packs.pack1.soh` | sensor | 48962 | U_WORD | % | unit | slow |
| `battery.packs.pack2.soh` | sensor | 48963 | U_WORD | % | unit | slow |
| `battery.packs.pack0.serial` | text_sensor | 48977-48984 | RAW |  | unit | slow |
| `battery.packs.pack1.serial` | text_sensor | 48985-48992 | RAW |  | unit | slow |
| `battery.packs.pack2.serial` | text_sensor | 48993-49000 | RAW |  | unit | slow |
| `on_grid` | binary_sensor | 49217 | U_WORD |  | unit | fast |
| `ac.bypass_power` | sensor | 49218-49219 | FP32 | W | unit | fast |
| `bypass_ac1` | switch | 49220 | U_WORD |  | unit | slow |
<!-- registers:end -->

## Regenerating the register map

```
python3 -I tools/gen_registers.py <path to ha-ecoflow-official> "<version>"
python3 -I tools/gen_yaml.py
python3 -I tools/gen_readme.py
```

## License

MIT License

Copyright (c) e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
