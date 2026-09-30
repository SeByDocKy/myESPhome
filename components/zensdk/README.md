# `zensdk` — Zendure SolarFlow native ESPHome component

Local, cloud-free control of Zendure SolarFlow batteries from an ESP32 over WiFi, using the
device's own **zenSDK local HTTP API**. No MQTT broker, no Bluetooth, no Home Assistant
integration required.

Protocol references:
- Zendure zenSDK: <https://github.com/Zendure/zenSDK> (`README.md`, `docs/en_properties.md`)
- Zendure-HA (command sequences and per-model power limits): <https://github.com/Zendure/Zendure-HA>
  (`custom_components/zendure_ha/device.py`, class `ZendureZenSdk`, and `devices/solarflow*.py`)

## Supported devices

SolarFlow 800, 800 Plus, 800 Pro, 1600 AC+, 2400 AC, 2400 AC+, 2400 Pro (the models listed by zenSDK).

**Not supported:** legacy MQTT-only devices (Hyper 2000, Hub 1200 / 2000, Ace 1500, AIO 2400). They have no
HTTP API and need a broker plus a Bluetooth provisioning step.

**Probably compatible (unofficial).** SolarFlow 3000 Mix AC+, 4000 Mix AC+ and 4000 Mix Pro are not yet in
zenSDK's official "Supported Products" list, but community Home Assistant integrations
([Gielz1986/Zendure-HA-zenSDK](https://github.com/Gielz1986/Zendure-HA-zenSDK)) talk to these models over the
same local `/properties/report` / `/properties/write` API and the same property names. **Not verified against
a real device.** There is no `model:` preset for them yet (their AC charge/discharge power limits aren't
published), so set `max_charge_power` / `max_discharge_power` explicitly. Their PV input is 2 independent
30–400 V string MPPTs rather than the 6 low-voltage `solarPower1..6` channels of the SF800–2400 range, so
`dc_channels:` should still work but likely only exposes 2 entries (`pv0`, `pv1`) — unconfirmed.

## How it works

The battery hosts a small REST server:

| Purpose | Request |
|---|---|
| Read everything | `GET /properties/report` → `{"properties": {...}, "packData": [{...}, ...]}` |
| Write properties | `POST /properties/write` with `{"sn": "<serial>", "id": <n>, "properties": {...}}` |

- All HTTP I/O runs on a dedicated FreeRTOS task, so the ESPHome main loop is never blocked. Entity updates
  are published from `loop()` on the main thread.
- One `zensdk:` block per battery (`MULTI_CONF`). Each opens its own short-lived HTTP connection.
- Charge / discharge always sends the **complete** command set
  `{smartMode, acMode, inputLimit, outputLimit}`. A bare limit write is silently ignored by the device once it
  has dropped out of smart mode (observed by the Zendure-HA maintainers on a SolarFlow 2400 Pro).
- `smartMode: 1` is used for every power command, so nothing is written to the device's flash.
- Identical consecutive power commands are not re-sent, except every 60 s (keeps the command asserted).
- The requested power is clamped to the limits of the configured model.

## Before you start: enable the local API

Recent firmware (EN 18031) has the local HTTP API **disabled by default**. According to the zenSDK README it
is enabled by adding HEMS in the Zendure app and then exiting the HEMS setup to apply it. Check with
`curl http://<ip>/properties/report`. Use a fixed IP (DHCP reservation): ESPHome cannot resolve the
`Zendure-<Model>-<MAC>.local` mDNS name.

## Configuration

```yaml
zensdk:
  - id: zensdk_1
    ip_address: 192.168.1.60      # required, IP address of the battery
    sn: "WOB1NHMAMXXXXX3"         # required, serial number (mandatory in every POST)
    model: SF2400_AC_PLUS         # sets the power limits, see below
    poll_interval: 10s            # default 10s
```

| Option | Default | Description |
|---|---|---|
| `ip_address` | required | IP address (or DNS name) of the battery |
| `ip_port` | `80` | HTTP port |
| `sn` | required | Device serial number |
| `model` | — | `SF800`, `SF800_PLUS`, `SF800_PRO`, `SF1600_AC_PLUS`, `SF2400_AC`, `SF2400_AC_PLUS`, `SF2400_PRO` |
| `max_charge_power` | from `model` | Max AC charge power in W (required if `model` is not set) |
| `max_discharge_power` | from `model` | Max AC discharge power in W (required if `model` is not set) |
| `soc_scale` | `10` | Raw `socSet` / `minSoc` units per percent, see the note below |
| `write_flash_on_stop` | `false` | Stop with `smartMode: 0` (as Zendure-HA does when not off-grid), which persists the zero limits to flash |
| `poll_interval` | `10s` | Time between `GET /properties/report` |

Power limits per model (charge / discharge, from Zendure-HA): SF800 / 800 Plus / 800 Pro 1000 / 800 W,
SF1600 AC+ 1600 / 1600 W, SF2400 AC 2400 / 2400 W, SF2400 AC+ and 2400 Pro 3200 / 2400 W.

### Platforms

Each platform lives in its own sub-directory and takes an optional `zensdk_id` (only needed with several
batteries).

> **Breaking change.** The flat `pv_power_1..6` / `pack1_*..pack6_*` keys of earlier versions were replaced by
> `dc_channels:` / `ac:` / `battery:` groups (`sensor`) and a `battery.packs:` group (`text_sensor`), all
> 0-indexed (`pv0`…`pv5`, `pack0`…`pack5`), matching this author's `hms`/`hmsw`/`deyemi` components. Update your
> YAML accordingly — see the examples below.

**`sensor`**

Structured like this author's `hms`/`hmsw`/`deyemi` components: device-level diagnostics stay flat at the
root, PV channels are grouped under `dc_channels:`, AC/grid/home entities under `ac:`, and battery entities
under `battery:` (with per-pack entries in `battery.packs:`). All lists are **0-indexed** (`pv0` … `pv5`,
`pack0` … `pack5`), consistent across every platform of this component.

| Key | Property | Notes |
|---|---|---|
| `solar_input_power` | `solarInputPower` | Total PV input, W |
| `rssi` | `rssi` | dBm |
| `enclosure_temperature` | `hyperTmp` | Published in °C; the raw scale (0.1 K, K or °C) is auto-detected, see caveats |
| `fault_level` | `faultLevel` | Raw diagnostic code |

`dc_channels:` — a list, one entry per PV / MPPT input, e.g. `- pv0: {power: {name: ...}}`. List position
(0-based) maps to `solarPower<position+1>`; only `power` is exposed (zenSDK has no per-channel voltage/current).

`ac:` (flat):

| Key | Property | Notes |
|---|---|---|
| `home_power` | `outputHomePower` | AC output to home, W |
| `grid_power` | `gridInputPower` | AC input, W |
| `offgrid_power` | `gridOffPower` | Off-grid output, W |
| `status` | `acStatus` | Raw code: 0 stopped, 1 grid-tied/off-grid running, 2 charging (`docs/zh_properties.md`) |

`battery:` (flat fields, plus a `packs:` list):

| Key | Property | Notes |
|---|---|---|
| `soc` | `electricLevel` | Average SoC, % |
| `voltage` | `BatVolt` | 0.01 V units |
| `charge_power` | `packInputPower` | Power flowing into the packs, W |
| `discharge_power` | `outputPackPower` | Power flowing out of the packs, W |
| `soc_limit` | `socLimit` | Raw diagnostic code |
| `charge_max_limit` | `chargeMaxLimit` | W |
| `pack_count` | `packNum` | Raw diagnostic code |
| `remain_out_time`, `remain_input_time` | `remainOutTime`, `remainInputTime` | min |
| `status` | `dcStatus` | Raw code: 0 stopped, 1 battery input (charging), 2 battery output (discharging) (`docs/zh_properties.md`) |

`battery.packs:` — a list, one entry per battery pack, e.g. `- pack0: {soc_level: {name: ...}, ...}`.
List position (0-based) is the index into `packData`; `pack_count` reports how many the device sees.
Per-pack fields: `soc_level`, `power`, `temperature`, `total_voltage`, `current`, `max_cell_voltage`,
`min_cell_voltage`, `delta_cell_voltage`.

**`binary_sensor`**: `online` (polling succeeds; off after 3 consecutive failures), `heat_state`, `bypass`
(`pass`), `reverse_state`, `grid_connected` (`gridState`), `pv_active` (`pvStatus`), `soc_calibrating`
(`socStatus`), `error` (`is_error`), `fan`, `lamp`, `data_ready`. (Unchanged by the `dc_channels`/`ac`/`battery`
restructuring below; it only applies to `sensor` and `text_sensor`.)

**`text_sensor`**: `state` (`packState`: Standby / Charging / Discharging) and the firmware versions stay flat
at the root (diagnostic category, published as `vX.Y.ZZ`, only re-published when they change); the per-pack
`state` / `sn` / `firmware_version` move under `battery.packs:`, same 0-indexed list as the `sensor` platform:

| Key | Property | Notes |
|---|---|---|
| `firmware_version` | `masterSoftVersion`, else `masterFirmwareVersion` | Main firmware of the device |
| `ac_firmware_version`, `dc_firmware_version`, `bms_firmware_version`, `mppt_firmware_version` | `acFirmwareVersion`, `dcFirmwareVersion`, `bmsFirmwareVersion`, `mpptFirmwareVersion` | Sub-module firmwares |
| `battery.packs[].state`, `battery.packs[].sn`, `battery.packs[].firmware_version` | `packData[i].state/sn/softVersion` | `softVersion` is the only version property documented by zenSDK |

The packed integer is decoded like Zendure-HA does: a value above 10 becomes `v<bits 15-12>.<bits 11-8>.<bits 7-0>`
(e.g. `0x2107` → `v2.1.7`), `<= 0` gives `not provided`, and 1 to 10 is printed as is. A string value is passed through
unchanged.

**`number`**

| Key | Effect |
|---|---|
| `input_limit` | AC charge power (W): sends a full smart-mode charge command |
| `output_limit` | AC output power (W): sends a full smart-mode discharge command |
| `power_setpoint` | Signed W: `> 0` discharge, `< 0` charge, `0` stop |
| `soc_set` | Target SoC (`socSet`, 70–100 %) |
| `min_soc` | Minimum SoC (`minSoc`, 0–50 %) |
| `inverse_max_power` | Max inverter output (`inverseMaxPower`, W) |

Slider ranges cover the largest model; the hub clamps to the configured limits and publishes the clamped value.

`battery.packs[].delta_cell_voltage` is computed by the component as `max_cell_voltage − min_cell_voltage` (V,
`mdi:delta`, `voltage` device class); it is skipped when either cell voltage reads 0.

**`switch`**: `lamp` (`lampSwitch`, the LED strip of the device). Zendure-HA exposes this property as a writable
"LED" switch and writes `1` / `0` with `POST /properties/write`, but the zenSDK property table lists it as
read-only, so the write is **unverified on the SolarFlow models**: if the device ignores it, the switch simply
falls back to the state read from the next poll. It is not restored at boot (nothing is written to the battery on
startup). The read-only `lamp` binary sensor shows the same property.

`kickstart` (config category, restored at boot, **off by default**) is a software-only switch, not a device
property. When on, a power request of exactly ±50 W that the device does not follow yet is bumped to the current limit
plus 4 W (at most 100 W). "Not following yet" means the limit the device reports (`outputLimit` for discharge,
`inputLimit` for charge) is already ≥ 50 W while its real flow (`outputHomePower` / `gridInputPower`) is still 0 W.
This is the logic of Zendure-HA's `ZendureZenSdk.charge()` / `discharge()` (`SmartMode.POWER_START = 50`); its purpose
is not documented there, it appears to nudge the device out of standby when a manager asks it to start at the
minimum power. It only matters for controllers that request exactly 50 W to start a battery.

**`select`**: `ac_mode` (`acMode`: Charge / Discharge), `grid_off_mode` (`gridOffMode`: Standard / Economic /
Closure). Values are read back on every poll. Note that `ac_mode` alone only flips the mode; use the numbers or
outputs to actually start charging or discharging.

**`output`**: `charge_power` and `discharge_power` are `FloatOutput`s (0.0 … 1.0 maps to 0 … max power of the
model). The effective request is `discharge − charge`, so two independent PID loops (or one signed loop split
over the two outputs) can drive the battery.

### Full example (a SolarFlow 2400 AC+ with 3 battery packs)

```yaml
zensdk:
  - id: zensdk_1
    ip_address: 192.168.1.60
    sn: "WOB1NHMAMXXXXX3"
    model: SF2400_AC_PLUS

sensor:
  - platform: zensdk
    solar_input_power:
      name: "Zendure PV power"
    ac:
      home_power:
        name: "Zendure AC output"
      grid_power:
        name: "Zendure AC input"
    battery:
      soc:
        name: "Zendure SoC"
      charge_power:
        name: "Zendure battery discharge"
      discharge_power:
        name: "Zendure battery charge"
      packs:
        - pack0:
            soc_level:
              name: "Zendure pack 0 SoC"
            temperature:
              name: "Zendure pack 0 temperature"
        - pack1:
            soc_level:
              name: "Zendure pack 1 SoC"
            temperature:
              name: "Zendure pack 1 temperature"
        - pack2:
            soc_level:
              name: "Zendure pack 2 SoC"
            temperature:
              name: "Zendure pack 2 temperature"

binary_sensor:
  - platform: zensdk
    online:
      name: "Zendure online"
    error:
      name: "Zendure error"

text_sensor:
  - platform: zensdk
    state:
      name: "Zendure state"
    firmware_version:
      name: "Zendure firmware"
    battery:
      packs:
        - pack0:
            sn:
              name: "Zendure pack 0 SN"
        - pack1:
            sn:
              name: "Zendure pack 1 SN"
        - pack2:
            sn:
              name: "Zendure pack 2 SN"

number:
  - platform: zensdk
    power_setpoint:
      name: "Zendure power setpoint"
    soc_set:
      name: "Zendure max SoC"
    min_soc:
      name: "Zendure min SoC"

switch:
  - platform: zensdk
    lamp:
      name: "Zendure LED"
    kickstart:
      name: "Zendure kickstart"

select:
  - platform: zensdk
    grid_off_mode:
      name: "Zendure off-grid mode"

output:
  - platform: zensdk
    charge_power:
      id: zensdk_charge
    discharge_power:
      id: zensdk_discharge
```

Several batteries: add one `zensdk:` block per device and set `zensdk_id:` on every platform entry.

## Caveats (please verify on your hardware)

This component was written from the public zenSDK documentation and the Zendure-HA source. It has **not** been
run against a real device yet.

- **SoC scale.** The zenSDK property table documents `socSet` / `minSoc` in plain percent, while Zendure-HA
  scales them by 10 (raw `1000` = 100 %). `soc_scale` defaults to `10` (Zendure-HA); set it to `1` if your
  firmware reports plain percent.
- **Voltage and temperature units.** `packN_total_voltage` uses a heuristic (raw > 200 is treated as 0.01 V units,
  otherwise volts) because the zenSDK table says "V" but other fields are in 0.01 V. The scale of `hyperTmp` is not
  documented by zenSDK, so `enclosure_temperature` auto-detects it (raw > 1000: 0.1 K, i.e. `(raw − 2731) / 10`;
  raw > 200: Kelvin; otherwise already °C; a raw `0` is skipped) and always publishes °C. Pack temperatures
  (`packN_temperature`) use the documented 0.1 K encoding. `packN_current` is a signed 16-bit value in 0.1 A.
- **`packState`.** The device-level `packState` is decoded like the per-pack `state` (0 standby, 1 charging,
  2 discharging), which the zenSDK table only states for the per-pack field.
- **Kick-start.** Optional (`kickstart` switch, off by default). It follows Zendure-HA's code, but the reason for it is
  inferred, not documented, and it has not been tried on a device.
- **Firmware versions.** zenSDK documents only the per-pack `softVersion`. The device-level version properties are
  the names Zendure-HA reads when present; whether the local `/properties/report` of your SolarFlow contains them is
  not confirmed, so `firmware_version` may stay without a state. Check `curl http://<ip>/properties/report`
  and tell me which keys your device reports.
- **Lamp switch.** See the `switch` platform above: the write is taken from Zendure-HA, not from the zenSDK table.
- **`remainInputTime`** is read by Zendure-HA but absent from the zenSDK table; it is simply skipped if the
  device does not report it.
- **Local API length.** The device accepts at most 512 bytes per request; all commands here are far below that.
- **SolarFlow 4000/3000 Mix.** Community evidence (see "Supported devices" above) suggests the same local API
  works, but this has not been confirmed on a real Mix device, no power-limit preset exists for it, and the
  2-MPPT PV topology may not populate `solarPower1..6` the same way as the SF800–2400 range.
- **ArduinoJson / `json` component.** The report is parsed with ESPHome's `json::parse_json()`. If a future
  ESPHome release changes that signature, only `ZenSdkComponent::parse_report_()` needs adapting.
- ESP32 only (FreeRTOS task and lwIP sockets).
