# marstek

Native ESPHome component for **Marstek Venus** home batteries, read and controlled over **Modbus TCP**.

The battery is polled directly over your LAN by the ESP32 (WiFi): no RS485 wiring, no Home Assistant custom
integration in between. The register maps are the ones of the Home Assistant integration
[ViperRNMC/marstek_venus_modbus](https://github.com/ViperRNMC/marstek_venus_modbus).

The set of available entities depends on the battery **model** chosen in the hub: every platform only accepts the
keys that model has, and `esphome config` tells you when you ask for one it does not.

Requires **ESPHome 2026.9.0** or later.

## Supported models

| `model:` | Battery | Modbus TCP connection | Register map status (per the HA integration) |
|---|---|---|---|
| `a` | Venus A | Ethernet (RJ45) port of the battery | untested |
| `d` | Venus D | Ethernet (RJ45) port of the battery | untested |
| `e_v3` | Venus E v3 | Ethernet (RJ45) port of the battery | partially validated on hardware |
| `e_v12` | Venus E v1/v2 | no Ethernet port: needs an RS485 to Modbus TCP gateway (Elfin EW11, PUSR DR134, Waveshare...) | no reservation stated upstream |

Entities are **opt-in**: nothing is created unless you list it.

## Installation

The component reuses the `modbustcp` transport and the range/queue engine of `modbustcp_controller` (both in this
repository), so all three must be listed:

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome/
    components: [modbustcp, modbustcp_controller, marstek]
    refresh: 10s
```

`modbustcp_controller:` itself is **not** configured: `marstek` builds on its code, it does not instantiate it.

## Configuration

```yaml
modbustcp:
  id: venus_tcp
  ip_address: 192.168.1.60  # IP address given to the battery by your router
  ip_port: 502
  send_wait_time: 1000ms    # how long to wait for a reply before retrying

marstek:
  - id: venus
    modbustcp_id: venus_tcp
    model: e_v3             # a | d | e_v12 | e_v3
    address: 1              # Modbus unit id
    update_interval: 10s    # fast entities
    low_priority_interval: 60s
    auto_rs485_control: false

sensor:
  - platform: marstek
    marstek_id: venus
    battery:
      soc:
        name: "Battery SOC"
    battery_power:
      name: "Battery power"
    stored_energy:          # calculated: reads its own sources, nothing else to declare
      name: "Stored energy"

select:
  - platform: marstek
    marstek_id: venus
    user_work_mode:
      name: "Work mode"
```

### Hub options

| Option | Default | Description |
|---|---|---|
| `model` | required | Register map: `a`, `d`, `e_v12`, `e_v3`. |
| `modbustcp_id` | required | The `modbustcp` hub (one per battery IP address). |
| `address` | `1` | Modbus unit id. |
| `update_interval` | `10s` | Poll period of the fast entities (power, voltage, current, SOC, states...). |
| `low_priority_interval` | `60s` | Poll period of the slow entities (energy totals, versions, schedules...). Must be at least `update_interval`. Same 10 s / 60 s split as the HA integration. |
| `command_throttle` | `50ms` | Minimum delay between two requests. |
| `max_cmd_retries` | `3` | Attempts before the battery is declared offline. |
| `offline_skip_updates` | `0` | Poll cycles to skip while the battery is offline. |
| `auto_rs485_control` | `false` | Enable the RS485 control mode automatically before writing a command register (see below). |

## Entities

Number of entities per model:

| Model | sensor | text_sensor | binary_sensor | number | switch | select | button | Total |
|---|---|---|---|---|---|---|---|---|
| A | 134 | 5 | 3 | 29 | 8 | 2 | 1 | 182 |
| D | 120 | 7 | 5 | 29 | 8 | 2 | 1 | 172 |
| E v1/v2 | 36 | 8 | 6 | 31 | 8 | 3 | 1 | 93 |
| E v3 | 53 | 7 | 3 | 29 | 8 | 2 | 1 | 103 |

Every platform block takes `marstek_id:` and any of the keys below. The columns give the register of the key on each
model (`–` = not available on that model, `calc` = calculated by the component).

### sensor

The `sensor` platform groups the per-channel/per-pack entities under `dc_channels:` (indexed `pv0`..`pv3`), `ac:`, and `battery:` (with `battery.packs.pack0`..`pack5`, each holding `soc:` and an ordered `cells:` list). Every other platform stays flat.

| Key | A | D | E v1/v2 | E v3 |
|---|---|---|---|---|
| `dc_channels.pv0.voltage` | 30020 | 30020 | – | – |
| `dc_channels.pv0.current` | 30024 | 30024 | – | – |
| `dc_channels.pv0.power` | 30037 | 30037 | – | – |
| `dc_channels.pv1.voltage` | 30021 | 30021 | – | – |
| `dc_channels.pv1.current` | 30025 | 30025 | – | – |
| `dc_channels.pv1.power` | 30038 | 30038 | – | – |
| `dc_channels.pv2.voltage` | 30022 | 30022 | – | – |
| `dc_channels.pv2.current` | 30026 | 30026 | – | – |
| `dc_channels.pv2.power` | 30039 | 30039 | – | – |
| `dc_channels.pv3.voltage` | 30023 | 30023 | – | – |
| `dc_channels.pv3.current` | 30027 | 30027 | – | – |
| `dc_channels.pv3.power` | 30040 | 30040 | – | – |
| `ac.voltage` | 32200 | 32200 | 32200 | 32200 |
| `ac.current` | 37004 | 37004 | 32201 | 37004 |
| `ac.frequency` | 32204 | 32204 | 32204 | 32204 |
| `ac.power` | 30006 | 30006 | 32202 | 30006 |
| `ac.offgrid_voltage` | 32300 | 32300 | 32300 | 32300 |
| `ac.offgrid_current` | 32301 | 32301 | 32301 | 32301 |
| `ac.offgrid_power` | 32302 | 32302 | 32302 | 32302 |
| `battery.voltage` | 30100 | 30100 | 32100 | 30100 |
| `battery.current` | 30101 | 30101 | 32101 | 30101 |
| `battery.soc` | 32104 | 32104 | 32104 | 34002 |
| `battery.total_energy` | 32105 | 32105 | 32105 | 32105 |
| `battery.cycle_count` | 34003 | 34003 | – | 34003 |
| `battery.cycle_count_calc` | calc | calc | calc | calc |
| `battery.packs.pack0.soc` | 34002 | 34002 | – | 34002 |
| `battery.packs.pack0.cells[0]` | 34018 | 34018 | – | 34018 |
| `battery.packs.pack0.cells[1]` | 34019 | 34019 | – | 34019 |
| `battery.packs.pack0.cells[2]` | 34020 | 34020 | – | 34020 |
| `battery.packs.pack0.cells[3]` | 34021 | 34021 | – | 34021 |
| `battery.packs.pack0.cells[4]` | 34022 | 34022 | – | 34022 |
| `battery.packs.pack0.cells[5]` | 34023 | 34023 | – | 34023 |
| `battery.packs.pack0.cells[6]` | 34024 | 34024 | – | 34024 |
| `battery.packs.pack0.cells[7]` | 34025 | 34025 | – | 34025 |
| `battery.packs.pack0.cells[8]` | 34026 | 34026 | – | 34026 |
| `battery.packs.pack0.cells[9]` | 34027 | 34027 | – | 34027 |
| `battery.packs.pack0.cells[10]` | 34028 | 34028 | – | 34028 |
| `battery.packs.pack0.cells[11]` | 34029 | 34029 | – | 34029 |
| `battery.packs.pack0.cells[12]` | 34030 | 34030 | – | 34030 |
| `battery.packs.pack0.cells[13]` | – | 34031 | – | 34031 |
| `battery.packs.pack0.cells[14]` | – | 34032 | – | 34032 |
| `battery.packs.pack0.cells[15]` | – | 34033 | – | 34033 |
| `battery.packs.pack1.soc` | 34102 | 34102 | – | – |
| `battery.packs.pack1.cells[0]` | 34118 | 34118 | – | – |
| `battery.packs.pack1.cells[1]` | 34119 | 34119 | – | – |
| `battery.packs.pack1.cells[2]` | 34120 | 34120 | – | – |
| `battery.packs.pack1.cells[3]` | 34121 | 34121 | – | – |
| `battery.packs.pack1.cells[4]` | 34122 | 34122 | – | – |
| `battery.packs.pack1.cells[5]` | 34123 | 34123 | – | – |
| `battery.packs.pack1.cells[6]` | 34124 | 34124 | – | – |
| `battery.packs.pack1.cells[7]` | 34125 | 34125 | – | – |
| `battery.packs.pack1.cells[8]` | 34126 | 34126 | – | – |
| `battery.packs.pack1.cells[9]` | 34127 | 34127 | – | – |
| `battery.packs.pack1.cells[10]` | 34128 | 34128 | – | – |
| `battery.packs.pack1.cells[11]` | 34129 | 34129 | – | – |
| `battery.packs.pack1.cells[12]` | 34130 | 34130 | – | – |
| `battery.packs.pack1.cells[13]` | – | 34131 | – | – |
| `battery.packs.pack1.cells[14]` | – | 34132 | – | – |
| `battery.packs.pack1.cells[15]` | – | 34133 | – | – |
| `battery.packs.pack2.soc` | 34202 | 34202 | – | – |
| `battery.packs.pack2.cells[0]` | 34218 | 34218 | – | – |
| `battery.packs.pack2.cells[1]` | 34219 | 34219 | – | – |
| `battery.packs.pack2.cells[2]` | 34220 | 34220 | – | – |
| `battery.packs.pack2.cells[3]` | 34221 | 34221 | – | – |
| `battery.packs.pack2.cells[4]` | 34222 | 34222 | – | – |
| `battery.packs.pack2.cells[5]` | 34223 | 34223 | – | – |
| `battery.packs.pack2.cells[6]` | 34224 | 34224 | – | – |
| `battery.packs.pack2.cells[7]` | 34225 | 34225 | – | – |
| `battery.packs.pack2.cells[8]` | 34226 | 34226 | – | – |
| `battery.packs.pack2.cells[9]` | 34227 | 34227 | – | – |
| `battery.packs.pack2.cells[10]` | 34228 | 34228 | – | – |
| `battery.packs.pack2.cells[11]` | 34229 | 34229 | – | – |
| `battery.packs.pack2.cells[12]` | 34230 | 34230 | – | – |
| `battery.packs.pack2.cells[13]` | – | 34231 | – | – |
| `battery.packs.pack2.cells[14]` | – | 34232 | – | – |
| `battery.packs.pack2.cells[15]` | – | 34233 | – | – |
| `battery.packs.pack3.soc` | 34302 | 34302 | – | – |
| `battery.packs.pack3.cells[0]` | 34318 | 34318 | – | – |
| `battery.packs.pack3.cells[1]` | 34319 | 34319 | – | – |
| `battery.packs.pack3.cells[2]` | 34320 | 34320 | – | – |
| `battery.packs.pack3.cells[3]` | 34321 | 34321 | – | – |
| `battery.packs.pack3.cells[4]` | 34322 | 34322 | – | – |
| `battery.packs.pack3.cells[5]` | 34323 | 34323 | – | – |
| `battery.packs.pack3.cells[6]` | 34324 | 34324 | – | – |
| `battery.packs.pack3.cells[7]` | 34325 | 34325 | – | – |
| `battery.packs.pack3.cells[8]` | 34326 | 34326 | – | – |
| `battery.packs.pack3.cells[9]` | 34327 | 34327 | – | – |
| `battery.packs.pack3.cells[10]` | 34328 | 34328 | – | – |
| `battery.packs.pack3.cells[11]` | 34329 | 34329 | – | – |
| `battery.packs.pack3.cells[12]` | 34330 | 34330 | – | – |
| `battery.packs.pack3.cells[13]` | – | 34331 | – | – |
| `battery.packs.pack3.cells[14]` | – | 34332 | – | – |
| `battery.packs.pack3.cells[15]` | – | 34333 | – | – |
| `battery.packs.pack4.soc` | 34402 | 34402 | – | – |
| `battery.packs.pack4.cells[0]` | 34418 | – | – | – |
| `battery.packs.pack4.cells[1]` | 34419 | – | – | – |
| `battery.packs.pack4.cells[2]` | 34420 | – | – | – |
| `battery.packs.pack4.cells[3]` | 34421 | – | – | – |
| `battery.packs.pack4.cells[4]` | 34422 | – | – | – |
| `battery.packs.pack4.cells[5]` | 34423 | – | – | – |
| `battery.packs.pack4.cells[6]` | 34424 | – | – | – |
| `battery.packs.pack4.cells[7]` | 34425 | – | – | – |
| `battery.packs.pack4.cells[8]` | 34426 | – | – | – |
| `battery.packs.pack4.cells[9]` | 34427 | – | – | – |
| `battery.packs.pack4.cells[10]` | 34428 | – | – | – |
| `battery.packs.pack4.cells[11]` | 34429 | – | – | – |
| `battery.packs.pack4.cells[12]` | 34430 | – | – | – |
| `battery.packs.pack5.soc` | 34502 | 34502 | – | – |
| `battery.packs.pack5.cells[0]` | 34518 | – | – | – |
| `battery.packs.pack5.cells[1]` | 34519 | – | – | – |
| `battery.packs.pack5.cells[2]` | 34520 | – | – | – |
| `battery.packs.pack5.cells[3]` | 34521 | – | – | – |
| `battery.packs.pack5.cells[4]` | 34522 | – | – | – |
| `battery.packs.pack5.cells[5]` | 34523 | – | – | – |
| `battery.packs.pack5.cells[6]` | 34524 | – | – | – |
| `battery.packs.pack5.cells[7]` | 34525 | – | – | – |
| `battery.packs.pack5.cells[8]` | 34526 | – | – | – |
| `battery.packs.pack5.cells[9]` | 34527 | – | – | – |
| `battery.packs.pack5.cells[10]` | 34528 | – | – | – |
| `battery.packs.pack5.cells[11]` | 34529 | – | – | – |
| `battery.packs.pack5.cells[12]` | 34530 | – | – | – |
| `battery_power` | 30001 | 30001 | 32102 | 30001 |
| `bluetooth_status` | 30301 | 30301 | 30301 | 30301 |
| `bms_version` | 30204 | 30204 | 31102 | 30204 |
| `conversion_efficiency` | calc | calc | calc | calc |
| `ems_version` | 30200 | 30200 | 31101 | 30200 |
| `internal_mos1_temperature` | 35001 | 35001 | 35001 | 35001 |
| `internal_mos2_temperature` | 35002 | 35002 | 35002 | 35002 |
| `internal_temperature` | 35000 | 35000 | 35000 | 35000 |
| `max_cell_temperature` | 35010 | 35010 | 35010 | 35010 |
| `max_cell_voltage` | 37007 | 37007 | 37007 | 37007 |
| `min_cell_temperature` | 35011 | 35011 | 35011 | 35011 |
| `min_cell_voltage` | 37008 | 37008 | 37008 | 37008 |
| `modbus_address` | 41100 | 41100 | 41100 | 41100 |
| `round_trip_efficiency_monthly` | calc | calc | calc | calc |
| `round_trip_efficiency_total` | calc | calc | calc | calc |
| `software_version` | – | – | 31100 | – |
| `solar_power_total` | calc | calc | – | – |
| `stored_energy` | calc | calc | calc | calc |
| `total_charging_energy` | 33000 | 33000 | 33000 | 33000 |
| `total_daily_charging_energy` | 33004 | 33004 | 33004 | 33004 |
| `total_daily_discharging_energy` | 33006 | 33006 | 33006 | 33006 |
| `total_discharging_energy` | 33002 | 33002 | 33002 | 33002 |
| `total_monthly_charging_energy` | 33008 | 33008 | 33008 | 33008 |
| `total_monthly_discharging_energy` | 33010 | 33010 | 33010 | 33010 |
| `vms_version` | 30202 | 30202 | – | 30202 |
| `wifi_signal_strength` | 30303 | 30303 | 30303 | 30303 |

### text_sensor

| Key | A | D | E v1/v2 | E v3 |
|---|---|---|---|---|
| `ble_mac_address` | 30304 | 30304 | 30402 | 30304 |
| `comm_module_firmware` | 30350 | 30350 | 30800 | 30350 |
| `device_name` | 31000 | 31000 | 31000 | 31000 |
| `inverter_state` | 35100 | 35100 | 35100 | 35100 |
| `firmware_version` | calc | calc | calc | calc |
| `alarm_status` | – | 36000 | 36000 | – |
| `fault_status` | – | 36100 | 36100 | – |
| `sn_code` | – | – | 31200 | – |
| `device_ip` | – | – | – | 30400 |
| `gateway_ip` | – | – | – | 30402 |

- `fault_status` / `alarm_status` list the active bits by name, or `OK`. Bit names are only known for the E v1/v2
  (and bit 4 of the D fault register); unknown bits are shown as `Bit <n>`.
- `firmware_version` is built as in the HA integration: `V<ems>.<bms>` (E v1/v2) or `V<ems>.<vms>.<bms>`.
- `ble_mac_address` is decoded from 12 ASCII hex characters into `AA:BB:CC:DD:EE:FF`.

### binary_sensor

| Key | A | D | E v1/v2 | E v3 |
|---|---|---|---|---|
| `wifi_status` | 30300 | 30300 | 30300 | 30300 |
| `cloud_status` | 30302 | 30302 | 30302 | 30302 |
| `modbus_connection` | calc | calc | calc | calc |
| `alarm_active` | – | 36000 | 36000 | – |
| `fault_active` | – | 36100 | 36100 | – |
| `discharge_limit_mode` | – | – | 41010 | – |

- `fault_active` / `alarm_active` are ON when any bit of the corresponding status register is set.
- `modbus_connection` has no register: ON while the battery answers, OFF once it is declared offline.

### number

| Key | A | D | E v1/v2 | E v3 |
|---|---|---|---|---|
| `charge_to_soc` | 42011 | 42011 | 42011 | 42011 |
| `set_charge_power` | 42020 | 42020 | 42020 | 42020 |
| `set_discharge_power` | 42021 | 42021 | 42021 | 42021 |
| `schedule_1_days` | 43100 | 43100 | 43100 | 43100 |
| `schedule_1_start` | 43101 | 43101 | 43101 | 43101 |
| `schedule_1_end` | 43102 | 43102 | 43102 | 43102 |
| `schedule_1_mode` | 43103 | 43103 | 43103 | 43103 |
| `schedule_2_days` | 43105 | 43105 | 43105 | 43105 |
| `schedule_2_start` | 43106 | 43106 | 43106 | 43106 |
| `schedule_2_end` | 43107 | 43107 | 43107 | 43107 |
| `schedule_2_mode` | 43108 | 43108 | 43108 | 43108 |
| `schedule_3_days` | 43110 | 43110 | 43110 | 43110 |
| `schedule_3_start` | 43111 | 43111 | 43111 | 43111 |
| `schedule_3_end` | 43112 | 43112 | 43112 | 43112 |
| `schedule_3_mode` | 43113 | 43113 | 43113 | 43113 |
| `schedule_4_days` | 43115 | 43115 | 43115 | 43115 |
| `schedule_4_start` | 43116 | 43116 | 43116 | 43116 |
| `schedule_4_end` | 43117 | 43117 | 43117 | 43117 |
| `schedule_4_mode` | 43118 | 43118 | 43118 | 43118 |
| `schedule_5_days` | 43120 | 43120 | 43120 | 43120 |
| `schedule_5_start` | 43121 | 43121 | 43121 | 43121 |
| `schedule_5_end` | 43122 | 43122 | 43122 | 43122 |
| `schedule_5_mode` | 43123 | 43123 | 43123 | 43123 |
| `schedule_6_days` | 43125 | 43125 | 43125 | 43125 |
| `schedule_6_start` | 43126 | 43126 | 43126 | 43126 |
| `schedule_6_end` | 43127 | 43127 | 43127 | 43127 |
| `schedule_6_mode` | 43128 | 43128 | 43128 | 43128 |
| `max_charge_power` | 44002 | 44002 | 44002 | 44002 |
| `max_discharge_power` | 44003 | 44003 | 44003 | 44003 |
| `charging_cutoff_capacity` | – | – | 44000 | – |
| `discharging_cutoff_capacity` | – | – | 44001 | – |

- The value is `register x scale` (`charging_cutoff_capacity` and `discharging_cutoff_capacity` are in %, the
  register holds tenths of a percent).
- `schedule_<n>_start` / `_end` are `HHMM` values (830 = 08:30), not minutes.
- `schedule_<n>_mode`: `-1` self-consumption, negative = charge power in W, positive = discharge power in W.
- `schedule_<n>_days` is a **bit mask** (0-127): Monday = 1, Tuesday = 2, Wednesday = 4, Thursday = 8, Friday = 16,
  Saturday = 32, Sunday = 64. Weekdays only = 31, weekend = 96, every day = 127. (The HA integration exposes it as a
  single-day select; a number allows any combination.)
- Power limits are capped at 1500 W on the Venus A and 2500 W on the others.

### switch

| Key | A | D | E v1/v2 | E v3 |
|---|---|---|---|---|
| `backup_function` | 41200 | 41200 | 41200 | 41200 |
| `rs485_control_mode` | 42000 | 42000 | 42000 | 42000 |
| `schedule_1_enabled` | 43104 | 43104 | 43104 | 43104 |
| `schedule_2_enabled` | 43109 | 43109 | 43109 | 43109 |
| `schedule_3_enabled` | 43114 | 43114 | 43114 | 43114 |
| `schedule_4_enabled` | 43119 | 43119 | 43119 | 43119 |
| `schedule_5_enabled` | 43124 | 43124 | 43124 | 43124 |
| `schedule_6_enabled` | 43129 | 43129 | 43129 | 43129 |

- A switch is ON when its register equals its "on" value. `backup_function` is inverted on the battery (0 = on).
- `rs485_control_mode` writes `21930` (on) / `21947` (off) to register 42000.
- Nothing is written at boot: the state always comes from the battery.

### select

| Key | A | D | E v1/v2 | E v3 |
|---|---|---|---|---|
| `force_mode` | 42010 | 42010 | 42010 | 42010 |
| `user_work_mode` | 43000 | 43000 | 43000 | 43000 |
| `grid_standard` | – | – | 44100 | – |

- `user_work_mode`: `manual` (0), `anti_feed` (1), `trade_mode` (2).
- `force_mode`: `standby` (0), `charge` (1), `discharge` (2).
- `grid_standard` (E v1/v2): Auto, EN50549, Netherlands, Germany, Austria, United Kingdom, Spain, Poland, Italy, China.

### button

| Key | A | D | E v1/v2 | E v3 |
|---|---|---|---|---|
| `reset_device` | 41000 | 41000 | 41000 | 41000 |

`factory_reset` of the HA integration is deliberately **not** provided: its YAML sends the same value as
`reset_device`, which looks like a copy/paste error. `reset_device` uses the value of the HA integration, not
verified independently.

## Reading: polling and ranges

- Entities are grouped into **read ranges**: neighbouring registers are fetched in one request (at most 125
  registers), never mixing the fast and the slow polling classes. Declare fewer entities and you get fewer requests.
- Each cycle queues one request per due range. If the queue is not empty when the next cycle starts, the controller logs
  `Duplicate modbus command found`: the battery cannot be polled that fast for that many ranges. Raise
  `update_interval`, or declare fewer entities (a Venus A with all its cell voltages needs many requests).
- A reply is matched to its request by transaction id and the next request goes out as soon as the reply arrives.
- A Modbus exception reply (for instance a register the model does not implement) is logged and does not stall the queue.

## Writing and the RS485 control mode

The command registers (42010 force mode, 42011 charge to SOC, 42020 / 42021 charge and discharge power...) only work
when the battery is in **RS485 control mode**: write `21930` to register 42000 (the `rs485_control_mode` switch).
With `auto_rs485_control: true` the component does it for you, once, before the first write to a command register.

Take care: in this mode the battery follows your commands, not its own logic. Writes are queued in order behind the
polling requests of the current cycle.

## Multiple batteries

One `modbustcp` hub (one IP address) and one `marstek` hub per battery.

## Differences from the HA integration and open points

- `schedule_<n>_days` is a number (bit mask) instead of a single-day select.
- `factory_reset` is not provided (see above).
- `device_ip` / `gateway_ip` (E v3) are shown as `a.b.c.d` from the 4 bytes in register order. The HA integration
  itself has no decoder for that type, so this layout is **not confirmed**.
- `ac_offgrid_current` on the E v3: the HA notes say register 32301 returns the voltage there; treat it as unreliable.
- The Venus A and D register maps are marked "untested" upstream, the E v3 "partially validated".
- Venus A / D with a PV input: the charge / discharge energy totals may include energy that goes to the house loads
  (see the HA integration notes), so do not feed them to the Energy dashboard without checking.

## Changes to `modbustcp` and `modbustcp_controller`

`marstek` needed a sturdier transport, so `modbustcp` was rewritten and `modbustcp_controller` got a few fixes.
The C++ methods used by the controller are kept (`handle_message()` and `send_message()`, public before, are gone:
nothing in this repository called them).
Behaviour and configuration changes to be aware of when reusing these components in an existing configuration:

- **Breaking**: the hub's `host` and `port` keys were renamed to `ip_address` and `ip_port`
  (`send_wait_time` and every other key/default are unchanged):

  ```yaml
  modbustcp:
    id: venus_tcp
    ip_address: 192.168.1.60   # was: host
    ip_port: 502                # was: port
  ```

- `send_wait_time` is now the reply timeout and the next request goes out as soon as the reply arrives (see below).
- A Modbus exception reply removes the command from the queue instead of being ignored and retried.
- A reply is accepted only if its transaction id and function code match the request in flight. A non compliant
  server that does not echo the transaction id would have its replies ignored (not seen on the devices tested here,
  which were simulated).
- `custom_command` / `send_raw()`: the payload is `[unit id][function code][data...]` and the MBAP header is added
  by the component. A payload that already carries its own MBAP header would be wrapped twice.

`modbustcp`:

- Frames are parsed from a receive buffer: partial frames, several frames per packet and stray bytes are handled,
  and the parsing runs on the main loop instead of the async_tcp task.
- A reply is matched to its request (transaction id, function code); a late reply to an earlier attempt is ignored.
- Modbus exception replies are passed to the device (`on_modbus_error`) instead of being dropped.
- `waiting_for_response` is cleared by the reply, not only by the `send_wait_time` timeout. **`send_wait_time` is now
  the reply timeout**; previously every request waited the whole delay, so existing setups poll faster (use
  `command_throttle` to space requests).
- Write-multiple (FC 0x10): the MBAP length was 3 bytes short.
- `send_raw()` used `sizeof(vector)` instead of the payload size, and the reply data vector was built 9 bytes too
  long, reading past the received data.

`modbustcp_controller`:

- No more `front()` on an empty command queue (response with nothing pending).
- The write acknowledge log no longer reads past the reply.
- Read ranges are capped at 125 registers.
- New `set_split_ranges_by_skip()` (used by `marstek`): a range never mixes different `skip_updates`. Off by default.
- `MULTI_CONF_NO_DEFAULT`: components that auto-load the controller no longer get a default instance (unit 1, 60 s).

## Testing status

Done, with ESPHome 2026.9.0:

- `esphome config` on the four models with **every** entity of each model, and rejection of keys a model does not have.
- C++ generation, then a native build on the ESPHome `host` platform, run against a simulated battery (Venus E v3
  registers, then an E v1/v2 scenario): values and scaling, calculated entities, text decoding, FC 0x06 writes,
  automatic RS485 control, replies split across TCP segments, a request that gets no reply (retry), Modbus exception
  replies, a 12 s outage (offline, then back online).
- `modbustcp_controller` used alone (as in the Shelly YAMLs): `U_WORD` / `S_WORD` / `S_DWORD` reads, FC 0x06 and FC 0x10
  writes.

**Not done:** no ESP32 firmware build (the toolchain download was blocked where this was written, so the first
`esphome compile` on your machine is the real test) and no test on a real battery.

## Regenerating the register tables

`registers.py` is generated from the register YAML files of the HA integration:

```bash
python tools/gen_registers.py <path>/custom_components/marstek_modbus/registers <source-commit>
```

The register definitions are Copyright (c) 2025 Viper, MIT License
([ViperRNMC/marstek_venus_modbus](https://github.com/ViperRNMC/marstek_venus_modbus)).
