# bluetti

Native ESPHome component for **BLUETTI plug & play batteries** (Balco 260, Balco 500) over **local Modbus TCP** (real MBAP header) — no cloud, no Home Assistant add-on.

> Status: sensor, text_sensor, binary_sensor, plus **switch, number and select** to control the battery. Nothing has been tested on real hardware yet.
> The register maps come from the community library [bluetti-modbus](https://github.com/Patrick762/bluetti-modbus) (MIT, see `LICENSE.bluetti-modbus`).

## Supported models

| `model`   | Device     | Notes |
|-----------|------------|-------|
| `balco260`| Balco 260  | Register map validated by the community |
| `balco500`| Balco 500  | Register map reported as confirmed on real hardware by the community; register 57001 is "System ON/OFF" |

Expansion packs are not supported yet.

## Device prerequisites

Enable Modbus TCP on the device web page (admin login: the Bluetooth password, or blank) → *Settings → Modbus TCP*, port `502`.

## Good practice

- The device accepts **only one Modbus client at a time**: stop any other client (Home Assistant integration, another ESP) before connecting the ESP.
- Never scan the register range: the firmware only answers reads inside its register blocks (the hub already stays inside them).
- The official Home Assistant integration polls every 15 s; the 5 s / 60 s intervals used in the examples are the author's choice, not a value from BLUETTI.

## Architecture

```
tcp_uart → modbus_tcp_uart → modbus_controller (fast) ─┐
                             modbus_controller (slow) ─┴→ bluetti (hub) → sensor / text_sensor / binary_sensor
```

The hub knows, per model, which register blocks the device answers (reads must stay inside a block and below a maximum span), and splits the read ranges accordingly. Slow-changing entities (versions, serial numbers, energy counters, settings) are polled by the optional slow controller.

## Modbus TCP wiring

Real Modbus TCP (MBAP header) is provided by `modbus_tcp_uart` on top of `tcp_uart`. Both are still ESPHome pull requests, so they are pulled in with `external_components`:

```yaml
external_components:
  - source: github://pr#20061
    components: [modbus_tcp_uart]
    refresh: 1h
  - source: github://pr#20198
    components: [ble_nus, tcp_uart, uart, usb_cdc_acm, usb_uart]
    refresh: 1h
  - source: github://SeByDocKy/myESPhome
    components: [bluetti]

tcp_uart:
  - id: bluetti_link
    host: 192.168.1.50      # IP address of the Bluetti battery
    port: 502

modbus_tcp_uart:
  - id: bluetti_uart
    tcp_uart_id: bluetti_link

modbus:
  - id: modbus_bluetti
    uart_id: bluetti_uart
    turnaround_time: 100ms

modbus_controller:
  - id: fast_modbus_controller
    address: 1
    modbus_id: modbus_bluetti
    update_interval: 5s
  - id: slow_modbus_controller
    address: 1
    modbus_id: modbus_bluetti
    update_interval: 60s
```

Then add the `bluetti:` hub and the platforms as shown below. The same hub also works on a plain RS485 `uart` + `modbus` bus if a model exposes one.

## Configuration

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [bluetti]

bluetti:
  id: bluetti_hub
  model: balco260
  modbus_controller_id: fast_modbus_controller
  slow_modbus_controller_id: slow_modbus_controller   # optional

sensor:
  - platform: bluetti
    bluetti_id: bluetti_hub
    ac:
      output_power:
        name: ${name}_ac_output_power
```

Entities are grouped as `ac:`, `dc_channels:` and `battery:` (indexed from 0, e.g. `pv0`, `pack0`); device-level entities are flat. A key the selected model does not have is rejected at validation time.
The complete list of entities per model is in `code/device_bluetti_balco260.yaml` and `code/device_bluetti_balco500.yaml` (generated from `registers.py`).

`binary_sensor` additionally accepts `modbus_connection` (connectivity of the Modbus link).

## Controls (switch, number, select)

The writable registers of the library are exposed as controls. They are read back from the device, so their state follows the battery; a command writes the register with function code 06, and the values read back are ignored for 10 s so that the entity does not jump back while the device applies it.

| Platform | Key | Register | Models |
|----------|-----|----------|--------|
| `switch` | `ac_output` | 57001 | Balco 260 |
| `switch` | `system_power` | 57001 | Balco 500 — **switches the whole system on/off** |
| `switch` | `grid_charging` | 57009 | Balco 260 (read-only `binary_sensor` on the Balco 500) |
| `switch` | `grid_feed_in` | 57010 | both |
| `number` | `battery: soc_low_limit`, `soc_high_limit` | 57016 / 57017 | both, 5–100 % |
| `select` | `working_mode` | 57020 | both: Custom, Self Consumption, Backup |

Switches default to `restore_mode: DISABLED`, so nothing is written at boot. There is no `output` platform: the Balco register maps have no power setpoint to drive.

The device confirms a write with an internal register address in the echo (for example 57001 → 2011). The ESPHome Modbus client does not compare that address with the request, but this still has to be confirmed on a real device.

```yaml
switch:
  - platform: bluetti
    bluetti_id: bluetti_hub
    grid_feed_in:
      name: ${name}_grid_feed_in

number:
  - platform: bluetti
    bluetti_id: bluetti_hub
    battery:
      soc_low_limit:
        name: ${name}_battery_soc_low_limit

select:
  - platform: bluetti
    bluetti_id: bluetti_hub
    working_mode:
      name: ${name}_working_mode
```

## Registers

All registers are holding registers (read with function code 03, written with 06). 32 / 64 bit values come low word first. "Poll" tells which modbus_controller reads the entity ("slow" = `slow_modbus_controller_id`, when there is one). The YAML key is the path of the entity (`battery.soc` is `battery:` / `soc:`).

<!-- registers:begin -->
### Balco 260 (`balco260`)

Validated on real hardware by the users of hassio-bluetti-modbus (balco 260).

Readable blocks: 50001-50019, 50020-50031, 50200-50214, 50219-50220, 50234-50253, 50254-50273, 50274-50283, 51001-51008, 51200-51219, 51220-51223, 51234-51247, 53001-53012, 57001, 57009-57010, 57016-57017, 57020. Longest read: 20 registers.

| Entity (YAML key) | Platform | Register | Type | Unit | Poll | Notes |
|---|---|---|---|---|---|---|
| `ac.inverter_count` | sensor | 50001 | U_WORD |  | slow |  |
| `ac.output_power` | sensor | 50002-50003 | U_DWORD_R | W | fast |  |
| `dc_channels.power` | sensor | 50004-50005 | U_DWORD_R | W | fast |  |
| `ac.grid_power` | sensor | 50006-50007 | S_DWORD_R | W | fast |  |
| `ac.inverter_power` | sensor | 50008-50009 | S_DWORD_R | W | fast |  |
| `dc_channels.ac_power` | sensor | 50010-50011 | U_DWORD_R | W | fast |  |
| `ac.output_energy` | sensor | 50012-50013 | U_DWORD_R | kWh | fast | x 0.1 |
| `dc_channels.energy` | sensor | 50014-50015 | U_DWORD_R | kWh | fast | x 0.1 |
| `ac.grid_input_energy` | sensor | 50016-50017 | U_DWORD_R | kWh | fast | x 0.1 |
| `ac.grid_output_energy` | sensor | 50018-50019 | U_DWORD_R | kWh | fast | x 0.1 |
| `dc_channels.ac_energy` | sensor | 50020-50021 | U_DWORD_R | kWh | fast | x 0.1 |
| `ac.inverter_status` | text_sensor | 50022 | U_WORD |  | fast | enumeration |
| `inverter_warning` | text_sensor | 50023-50026 | 4 x U_WORD |  | fast | E-codes, bit b of register n = bit 16n+b |
| `inverter_fault` | text_sensor | 50027-50031 | 5 x U_WORD |  | fast | E-codes, bit b of register n = bit 16n+b |
| `inverter_type` | text_sensor | 50200-50205 | 6 x U_WORD |  | slow | ASCII, first character in the low byte |
| `serial_number` | text_sensor | 50206-50209 | U_QWORD_R |  | slow | decimal, low word first |
| `arm_version` | text_sensor | 50210-50211 | 2 x U_WORD |  | slow | major.minor.patch |
| `dsp_version` | text_sensor | 50212-50213 | 2 x U_WORD |  | slow | major.minor.patch |
| `ac.grid_frequency` | sensor | 50214 | U_WORD | Hz | fast | x 0.1 |
| `dc_channels.local_power` | sensor | 50219-50220 | U_DWORD_R | W | fast |  |
| `ac.grid_phase_count` | sensor | 50234 | U_WORD |  | slow |  |
| `ac.grid_phases.phase0.power` | sensor | 50235 | U_WORD | W | fast |  |
| `ac.grid_phases.phase0.voltage` | sensor | 50236 | U_WORD | V | fast | x 0.1 |
| `ac.grid_phases.phase0.current` | sensor | 50237 | S_WORD | A | fast | x 0.1 |
| `ac.grid_phases.phase1.power` | sensor | 50238 | U_WORD | W | fast |  |
| `ac.grid_phases.phase1.voltage` | sensor | 50239 | U_WORD | V | fast | x 0.1 |
| `ac.grid_phases.phase1.current` | sensor | 50240 | S_WORD | A | fast | x 0.1 |
| `ac.grid_phases.phase2.power` | sensor | 50241 | U_WORD | W | fast |  |
| `ac.grid_phases.phase2.voltage` | sensor | 50242 | U_WORD | V | fast | x 0.1 |
| `ac.grid_phases.phase2.current` | sensor | 50243 | S_WORD | A | fast | x 0.1 |
| `ac.output_phase_count` | sensor | 50244 | U_WORD |  | slow |  |
| `ac.output_phases.phase0.power` | sensor | 50245 | U_WORD | W | fast |  |
| `ac.output_phases.phase0.voltage` | sensor | 50246 | U_WORD | V | fast | x 0.1 |
| `ac.output_phases.phase0.current` | sensor | 50247 | U_WORD | A | fast | x 0.1 |
| `ac.output_phases.phase1.power` | sensor | 50248 | U_WORD | W | fast |  |
| `ac.output_phases.phase1.voltage` | sensor | 50249 | U_WORD | V | fast | x 0.1 |
| `ac.output_phases.phase1.current` | sensor | 50250 | U_WORD | A | fast | x 0.1 |
| `ac.output_phases.phase2.power` | sensor | 50251 | U_WORD | W | fast |  |
| `ac.output_phases.phase2.voltage` | sensor | 50252 | U_WORD | V | fast | x 0.1 |
| `ac.output_phases.phase2.current` | sensor | 50253 | U_WORD | A | fast | x 0.1 |
| `ac.inverter_phase_count` | sensor | 50254 | U_WORD |  | slow |  |
| `ac.inverters.inverter0.status` | text_sensor | 50255 | U_WORD |  | fast | enumeration |
| `ac.inverters.inverter0.power` | sensor | 50256 | S_WORD | W | fast |  |
| `ac.inverters.inverter0.voltage` | sensor | 50257 | U_WORD | V | fast | x 0.1 |
| `ac.inverters.inverter0.current` | sensor | 50258 | U_WORD | A | fast | x 0.1 |
| `ac.inverters.inverter1.status` | text_sensor | 50259 | U_WORD |  | fast | enumeration |
| `ac.inverters.inverter1.power` | sensor | 50260 | S_WORD | W | fast |  |
| `ac.inverters.inverter1.voltage` | sensor | 50261 | U_WORD | V | fast | x 0.1 |
| `ac.inverters.inverter1.current` | sensor | 50262 | U_WORD | A | fast | x 0.1 |
| `ac.inverters.inverter2.status` | text_sensor | 50263 | U_WORD |  | fast | enumeration |
| `ac.inverters.inverter2.power` | sensor | 50264 | S_WORD | W | fast |  |
| `ac.inverters.inverter2.voltage` | sensor | 50265 | U_WORD | V | fast | x 0.1 |
| `ac.inverters.inverter2.current` | sensor | 50266 | U_WORD | A | fast | x 0.1 |
| `dc_channels.ac_count` | sensor | 50267 | U_WORD |  | slow | high nibble |
| `dc_channels.dc_count` | sensor | 50267 | U_WORD |  | slow | low nibble |
| `dc_channels.pv0.type` | text_sensor | 50268 | U_WORD |  | slow | enumeration |
| `dc_channels.pv0.power` | sensor | 50269 | U_WORD | W | fast |  |
| `dc_channels.pv0.voltage` | sensor | 50270 | U_WORD | V | fast | x 0.1 |
| `dc_channels.pv0.current` | sensor | 50271 | U_WORD | A | fast | x 0.1 |
| `dc_channels.pv1.type` | text_sensor | 50272 | U_WORD |  | slow | enumeration |
| `dc_channels.pv1.power` | sensor | 50273 | U_WORD | W | fast |  |
| `dc_channels.pv1.voltage` | sensor | 50274 | U_WORD | V | fast | x 0.1 |
| `dc_channels.pv1.current` | sensor | 50275 | U_WORD | A | fast | x 0.1 |
| `dc_channels.pv2.type` | text_sensor | 50276 | U_WORD |  | slow | enumeration |
| `dc_channels.pv2.power` | sensor | 50277 | U_WORD | W | fast |  |
| `dc_channels.pv2.voltage` | sensor | 50278 | U_WORD | V | fast | x 0.1 |
| `dc_channels.pv2.current` | sensor | 50279 | U_WORD | A | fast | x 0.1 |
| `dc_channels.pv3.type` | text_sensor | 50280 | U_WORD |  | slow | enumeration |
| `dc_channels.pv3.power` | sensor | 50281 | U_WORD | W | fast |  |
| `dc_channels.pv3.voltage` | sensor | 50282 | U_WORD | V | fast | x 0.1 |
| `dc_channels.pv3.current` | sensor | 50283 | U_WORD | A | fast | x 0.1 |
| `battery.pack_count` | sensor | 51001 | U_WORD |  | slow |  |
| `battery.voltage` | sensor | 51002 | U_WORD | V | fast | x 0.1 |
| `battery.current` | sensor | 51003 | U_WORD | A | fast | x 0.1 |
| `battery.soc` | sensor | 51004 | U_WORD | % | fast |  |
| `battery.soh` | sensor | 51005 | U_WORD | % | fast |  |
| `battery.status` | text_sensor | 51006 | U_WORD |  | fast | enumeration |
| `battery.time_to_full` | sensor | 51007 | U_WORD | min | fast |  |
| `battery.time_to_empty` | sensor | 51008 | U_WORD | min | fast |  |
| `battery.packs.pack0.type` | text_sensor | 51200-51205 | 6 x U_WORD |  | slow | ASCII, first character in the low byte |
| `battery.packs.pack0.serial` | text_sensor | 51206-51209 | U_QWORD_R |  | slow | decimal, low word first |
| `battery.packs.pack0.version_count` | sensor | 51210 | U_WORD |  | slow |  |
| `battery.packs.pack0.version1` | text_sensor | 51211-51212 | 2 x U_WORD |  | slow | major.minor.patch |
| `battery.packs.pack0.version2` | text_sensor | 51213-51214 | 2 x U_WORD |  | slow | major.minor.patch |
| `battery.packs.pack0.version3` | text_sensor | 51215-51216 | 2 x U_WORD |  | slow | major.minor.patch |
| `battery.packs.pack0.version4` | text_sensor | 51217-51218 | 2 x U_WORD |  | slow | major.minor.patch |
| `battery.packs.pack0.voltage` | sensor | 51219 | U_WORD | V | fast | x 0.1 |
| `battery.packs.pack0.current` | sensor | 51220 | U_WORD | A | fast | reference 30000 |
| `battery.packs.pack0.soc` | sensor | 51221 | U_WORD | % | fast |  |
| `battery.packs.pack0.soh` | sensor | 51222 | U_WORD | % | fast |  |
| `battery.packs.pack0.cycle_count` | sensor | 51223 | U_WORD |  | fast |  |
| `battery.packs.pack0.cell_count` | sensor | 51234 | U_WORD |  | slow |  |
| `battery.packs.pack0.ntc_count` | sensor | 51235 | U_WORD |  | slow |  |
| `battery.packs.pack0.energy_in` | sensor | 51236-51237 | U_DWORD_R | Wh | fast |  |
| `battery.packs.pack0.energy_out` | sensor | 51238-51239 | U_DWORD_R | Wh | fast |  |
| `battery.packs.pack0.protect` | sensor | 51240-51241 | U_DWORD_R |  | fast |  |
| `battery.packs.pack0.error` | text_sensor | 51242-51244 | 3 x U_WORD |  | fast | bit mask shown in hex |
| `battery.packs.pack0.alarm_residential` | sensor | 51245 | U_WORD |  | fast |  |
| `battery.packs.pack0.alarm_portable` | sensor | 51246-51247 | U_DWORD_R |  | fast |  |
| `iot_model` | text_sensor | 53001-53006 | 6 x U_WORD |  | slow | ASCII, first character in the low byte |
| `iot_serial` | text_sensor | 53007-53010 | U_QWORD_R |  | slow | decimal, low word first |
| `iot_version` | text_sensor | 53011-53012 | 2 x U_WORD |  | slow | major.minor.patch |
| `ac_output` | switch | 57001 | U_WORD |  | slow | 0 / 1 |
| `grid_charging` | switch | 57009 | U_WORD |  | slow | 0 / 1 |
| `grid_feed_in` | switch | 57010 | U_WORD |  | slow | 0 / 1 |
| `battery.soc_low_limit` | number | 57016 | U_WORD | % | slow | 5-100 |
| `battery.soc_high_limit` | number | 57017 | U_WORD | % | slow | 5-100 |
| `working_mode` | select | 57020 | U_WORD |  | slow | enumeration; 1 = Custom, 2 = Self Consumption, 4 = Backup |

### Balco 500 (`balco500`)

Reported as confirmed on real hardware by the users of hassio-bluetti-modbus (3.6.0 and later).

Readable blocks: 50001-50031, 50200-50249, 50250-50271, 51001-51008, 51200-51224, 51234-51249, 53001-53012, 57001, 57009-57010, 57016-57017, 57020. Longest read: 50 registers.

| Entity (YAML key) | Platform | Register | Type | Unit | Poll | Notes |
|---|---|---|---|---|---|---|
| `ac.inverter_count` | sensor | 50001 | U_WORD |  | slow |  |
| `ac.output_power` | sensor | 50002-50003 | U_DWORD_R | W | fast |  |
| `dc_channels.power` | sensor | 50004-50005 | U_DWORD_R | W | fast |  |
| `ac.grid_power` | sensor | 50006-50007 | S_DWORD_R | W | fast |  |
| `ac.inverter_power` | sensor | 50008-50009 | S_DWORD_R | W | fast |  |
| `dc_channels.ac_power` | sensor | 50010-50011 | U_DWORD_R | W | fast |  |
| `ac.output_energy` | sensor | 50012-50013 | U_DWORD_R | kWh | fast | x 0.1 |
| `dc_channels.energy` | sensor | 50014-50015 | U_DWORD_R | kWh | fast | x 0.1 |
| `ac.grid_input_energy` | sensor | 50016-50017 | U_DWORD_R | kWh | fast | x 0.1 |
| `ac.grid_output_energy` | sensor | 50018-50019 | U_DWORD_R | kWh | fast | x 0.1 |
| `dc_channels.ac_energy` | sensor | 50020-50021 | U_DWORD_R | kWh | fast | x 0.1 |
| `ac.inverter_status` | text_sensor | 50022 | U_WORD |  | fast | enumeration |
| `inverter_warning` | text_sensor | 50023-50026 | 4 x U_WORD |  | fast | E-codes, bit b of register n = bit 16n+b |
| `inverter_fault` | text_sensor | 50027-50031 | 5 x U_WORD |  | fast | E-codes, bit b of register n = bit 16n+b |
| `inverter_type` | text_sensor | 50200-50205 | 6 x U_WORD |  | slow | ASCII, first character in the low byte |
| `serial_number` | text_sensor | 50206-50209 | U_QWORD_R |  | slow | decimal, low word first |
| `dsp_version` | text_sensor | 50210-50211 | 2 x U_WORD |  | slow | major.minor.patch |
| `ac.grid_frequency` | sensor | 50214 | U_WORD | Hz | fast | x 0.1 |
| `ac.grid_local_power` | sensor | 50215-50216 | S_DWORD_R | W | fast |  |
| `ac.output_local_power` | sensor | 50217-50218 | U_DWORD_R | W | fast |  |
| `dc_channels.local_power` | sensor | 50219-50220 | U_DWORD_R | W | fast |  |
| `dc_channels.ac_local_power` | sensor | 50221-50222 | U_DWORD_R | W | fast |  |
| `ac.grid_input_local_energy` | sensor | 50223-50224 | U_DWORD_R | kWh | fast | x 0.1 |
| `ac.grid_output_local_energy` | sensor | 50225-50226 | U_DWORD_R | kWh | fast | x 0.1 |
| `ac.output_local_energy` | sensor | 50227-50228 | U_DWORD_R | kWh | fast | x 0.1 |
| `dc_channels.local_energy` | sensor | 50229-50230 | U_DWORD_R | kWh | fast | x 0.1 |
| `dc_channels.ac_local_energy` | sensor | 50231-50232 | U_DWORD_R | kWh | fast | x 0.1 |
| `ac.self_consumption` | sensor | 50233 | U_WORD | % | fast |  |
| `ac.grid_phase_count` | sensor | 50234 | U_WORD |  | slow |  |
| `ac.grid_phases.phase0.power` | sensor | 50235 | U_WORD | W | fast |  |
| `ac.grid_phases.phase0.voltage` | sensor | 50236 | U_WORD | V | fast | x 0.1 |
| `ac.grid_phases.phase0.current` | sensor | 50237 | S_WORD | A | fast | x 0.1 |
| `ac.grid_phases.phase1.power` | sensor | 50238 | U_WORD | W | fast |  |
| `ac.grid_phases.phase1.voltage` | sensor | 50239 | U_WORD | V | fast | x 0.1 |
| `ac.grid_phases.phase1.current` | sensor | 50240 | S_WORD | A | fast | x 0.1 |
| `ac.grid_phases.phase2.power` | sensor | 50241 | U_WORD | W | fast |  |
| `ac.grid_phases.phase2.voltage` | sensor | 50242 | U_WORD | V | fast | x 0.1 |
| `ac.grid_phases.phase2.current` | sensor | 50243 | S_WORD | A | fast | x 0.1 |
| `ac.output_phase_count` | sensor | 50244 | U_WORD |  | slow |  |
| `ac.output_phases.phase0.power` | sensor | 50245 | U_WORD | W | fast |  |
| `ac.output_phases.phase0.voltage` | sensor | 50246 | U_WORD | V | fast | x 0.1 |
| `ac.output_phases.phase0.current` | sensor | 50247 | U_WORD | A | fast | x 0.1 |
| `ac.output_phases.phase1.power` | sensor | 50248 | U_WORD | W | fast |  |
| `ac.output_phases.phase1.voltage` | sensor | 50249 | U_WORD | V | fast | x 0.1 |
| `ac.output_phases.phase1.current` | sensor | 50250 | U_WORD | A | fast | x 0.1 |
| `ac.output_phases.phase2.power` | sensor | 50251 | U_WORD | W | fast |  |
| `ac.output_phases.phase2.voltage` | sensor | 50252 | U_WORD | V | fast | x 0.1 |
| `ac.output_phases.phase2.current` | sensor | 50253 | U_WORD | A | fast | x 0.1 |
| `ac.inverter_phase_count` | sensor | 50254 | U_WORD |  | slow |  |
| `ac.inverters.inverter0.status` | text_sensor | 50255 | U_WORD |  | fast | enumeration |
| `ac.inverters.inverter0.power` | sensor | 50256 | S_WORD | W | fast |  |
| `ac.inverters.inverter0.voltage` | sensor | 50257 | U_WORD | V | fast | x 0.1 |
| `ac.inverters.inverter0.current` | sensor | 50258 | U_WORD | A | fast | x 0.1 |
| `ac.inverters.inverter1.status` | text_sensor | 50259 | U_WORD |  | fast | enumeration |
| `ac.inverters.inverter1.power` | sensor | 50260 | S_WORD | W | fast |  |
| `ac.inverters.inverter1.voltage` | sensor | 50261 | U_WORD | V | fast | x 0.1 |
| `ac.inverters.inverter1.current` | sensor | 50262 | U_WORD | A | fast | x 0.1 |
| `ac.inverters.inverter2.status` | text_sensor | 50263 | U_WORD |  | fast | enumeration |
| `ac.inverters.inverter2.power` | sensor | 50264 | S_WORD | W | fast |  |
| `ac.inverters.inverter2.voltage` | sensor | 50265 | U_WORD | V | fast | x 0.1 |
| `ac.inverters.inverter2.current` | sensor | 50266 | U_WORD | A | fast | x 0.1 |
| `dc_channels.ac_count` | sensor | 50267 | U_WORD |  | slow | high nibble |
| `dc_channels.dc_count` | sensor | 50267 | U_WORD |  | slow | low nibble |
| `dc_channels.pv0.type` | text_sensor | 50268 | U_WORD |  | slow | enumeration |
| `dc_channels.pv0.power` | sensor | 50269 | U_WORD | W | fast |  |
| `dc_channels.pv0.voltage` | sensor | 50270 | U_WORD | V | fast | x 0.1 |
| `dc_channels.pv0.current` | sensor | 50271 | U_WORD | A | fast | x 0.1 |
| `battery.pack_count` | sensor | 51001 | U_WORD |  | slow |  |
| `battery.voltage` | sensor | 51002 | U_WORD | V | fast | x 0.1 |
| `battery.current` | sensor | 51003 | U_WORD | A | fast | x 0.1 |
| `battery.soc` | sensor | 51004 | U_WORD | % | fast |  |
| `battery.soh` | sensor | 51005 | U_WORD | % | fast |  |
| `battery.status` | text_sensor | 51006 | U_WORD |  | fast | enumeration |
| `battery.time_to_full` | sensor | 51007 | U_WORD | min | fast |  |
| `battery.time_to_empty` | sensor | 51008 | U_WORD | min | fast |  |
| `battery.packs.pack0.type` | text_sensor | 51200-51205 | 6 x U_WORD |  | slow | ASCII, first character in the low byte |
| `battery.packs.pack0.serial` | text_sensor | 51206-51209 | U_QWORD_R |  | slow | decimal, low word first |
| `battery.packs.pack0.version_count` | sensor | 51210 | U_WORD |  | slow |  |
| `battery.packs.pack0.version1` | text_sensor | 51211-51212 | 2 x U_WORD |  | slow | major.minor.patch |
| `battery.packs.pack0.version2` | text_sensor | 51213-51214 | 2 x U_WORD |  | slow | major.minor.patch |
| `battery.packs.pack0.version3` | text_sensor | 51215-51216 | 2 x U_WORD |  | slow | major.minor.patch |
| `battery.packs.pack0.version4` | text_sensor | 51217-51218 | 2 x U_WORD |  | slow | major.minor.patch |
| `battery.packs.pack0.voltage` | sensor | 51219 | U_WORD | V | fast | x 0.1 |
| `battery.packs.pack0.current` | sensor | 51220 | U_WORD | A | fast | x 0.1 |
| `battery.packs.pack0.soc` | sensor | 51221 | U_WORD | % | fast |  |
| `battery.packs.pack0.soh` | sensor | 51222 | U_WORD | % | fast |  |
| `battery.packs.pack0.cycle_count` | sensor | 51223 | U_WORD |  | fast |  |
| `battery.packs.pack0.temperature` | sensor | 51224 | S_WORD | °C | fast | -40 |
| `battery.packs.pack0.cell_count` | sensor | 51234 | U_WORD |  | slow |  |
| `battery.packs.pack0.ntc_count` | sensor | 51235 | U_WORD |  | slow |  |
| `battery.packs.pack0.energy_in` | sensor | 51236-51237 | U_DWORD_R | Wh | fast |  |
| `battery.packs.pack0.energy_out` | sensor | 51238-51239 | U_DWORD_R | Wh | fast |  |
| `battery.packs.pack0.protect` | sensor | 51240-51241 | U_DWORD_R |  | fast |  |
| `battery.packs.pack0.error` | text_sensor | 51242-51244 | 3 x U_WORD |  | fast | bit mask shown in hex |
| `battery.packs.pack0.alarm_residential` | sensor | 51245 | U_WORD |  | fast |  |
| `battery.packs.pack0.alarm_portable` | sensor | 51246-51247 | U_DWORD_R |  | fast |  |
| `battery.packs.pack0.time_to_full` | sensor | 51248 | U_WORD | min | fast |  |
| `battery.packs.pack0.time_to_empty` | sensor | 51249 | U_WORD | min | fast |  |
| `iot_model` | text_sensor | 53001-53006 | 6 x U_WORD |  | slow | ASCII, first character in the low byte |
| `iot_serial` | text_sensor | 53007-53010 | U_QWORD_R |  | slow | decimal, low word first |
| `iot_version` | text_sensor | 53011-53012 | 2 x U_WORD |  | slow | major.minor.patch |
| `system_power` | switch | 57001 | U_WORD |  | slow | 0 / 1 |
| `grid_charging` | binary_sensor | 57009 | U_WORD |  | slow | 0 / 1 |
| `grid_feed_in` | switch | 57010 | U_WORD |  | slow | 0 / 1 |
| `battery.soc_low_limit` | number | 57016 | U_WORD | % | slow | 5-100 |
| `battery.soc_high_limit` | number | 57017 | U_WORD | % | slow | 5-100 |
| `working_mode` | select | 57020 | U_WORD |  | slow | enumeration; 1 = Custom, 2 = Self Consumption, 4 = Backup |

<!-- registers:end -->

## Regenerating the register map

```
python3 -I tools/gen_registers.py <path to bluetti_modbus_lib> "<version>"
python3 -I tools/gen_yaml.py
python3 -I tools/gen_readme.py   # refreshes the register tables above
```

## License

MIT License

Copyright (c) e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
