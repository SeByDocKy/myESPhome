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
    host: 192.168.1.60            # required, IP address of the battery
    sn: "WOB1NHMAMXXXXX3"         # required, serial number (mandatory in every POST)
    model: SF2400_AC_PLUS         # sets the power limits, see below
    poll_interval: 10s            # default 10s
```

| Option | Default | Description |
|---|---|---|
| `host` | required | IP address (or DNS name) of the battery |
| `port` | `80` | HTTP port |
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

**`sensor`**

| Key | Property | Notes |
|---|---|---|
| `electric_level` | `electricLevel` | Average SoC, % |
| `solar_input_power`, `solar_power_1` … `solar_power_6` | `solarInputPower`, `solarPower1..6` | W |
| `pack_input_power` | `packInputPower` | Battery discharge power, W |
| `output_pack_power` | `outputPackPower` | Battery charge power, W |
| `output_home_power` | `outputHomePower` | AC output to home, W |
| `grid_input_power` | `gridInputPower` | AC input, W |
| `grid_off_power` | `gridOffPower` | Off-grid output, W |
| `battery_voltage` | `BatVolt` | 0.01 V units |
| `enclosure_temperature` | `hyperTmp` | Assumed 0.1 K units, see caveats |
| `rssi` | `rssi` | dBm |
| `remain_out_time`, `remain_input_time` | `remainOutTime`, `remainInputTime` | min |
| `charge_max_limit` | `chargeMaxLimit` | W |
| `pack_num`, `soc_limit`, `fault_level`, `dc_status`, `ac_status` | same | Raw diagnostic codes |
| `packN_soc_level`, `packN_power`, `packN_temperature`, `packN_total_voltage`, `packN_current`, `packN_max_cell_voltage`, `packN_min_cell_voltage` | `packData[N-1]` | `N` = 1 … 6, matched by position in `packData` |

**`binary_sensor`**: `online` (polling succeeds; off after 3 consecutive failures), `heat_state`, `bypass`
(`pass`), `reverse_state`, `grid_connected` (`gridState`), `pv_active` (`pvStatus`), `soc_calibrating`
(`socStatus`), `error` (`is_error`), `fan`, `lamp`, `data_ready`.

**`text_sensor`**: `state` (`packState`: Standby / Charging / Discharging), `packN_state`, `packN_sn`.

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

**`select`**: `ac_mode` (`acMode`: Charge / Discharge), `grid_off_mode` (`gridOffMode`: Standard / Economic /
Closure). Values are read back on every poll. Note that `ac_mode` alone only flips the mode; use the numbers or
outputs to actually start charging or discharging.

**`output`**: `charge_power` and `discharge_power` are `FloatOutput`s (0.0 … 1.0 maps to 0 … max power of the
model). The effective request is `discharge − charge`, so two independent PID loops (or one signed loop split
over the two outputs) can drive the battery.

### Full example

```yaml
zensdk:
  - id: zensdk_1
    host: 192.168.1.60
    sn: "WOB1NHMAMXXXXX3"
    model: SF2400_AC_PLUS

sensor:
  - platform: zensdk
    electric_level:
      name: "Zendure SoC"
    solar_input_power:
      name: "Zendure PV power"
    pack_input_power:
      name: "Zendure battery discharge"
    output_pack_power:
      name: "Zendure battery charge"
    output_home_power:
      name: "Zendure AC output"
    grid_input_power:
      name: "Zendure AC input"
    pack1_soc_level:
      name: "Zendure pack 1 SoC"
    pack1_temperature:
      name: "Zendure pack 1 temperature"

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
    pack1_sn:
      name: "Zendure pack 1 SN"

number:
  - platform: zensdk
    power_setpoint:
      name: "Zendure power setpoint"
    soc_set:
      name: "Zendure max SoC"
    min_soc:
      name: "Zendure min SoC"

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
  otherwise volts) because the zenSDK table says "V" but other fields are in 0.01 V. `hyperTmp` is assumed to use
  the same 0.1 K encoding as the pack temperatures (`(raw − 2731) / 10`; a raw `0` is skipped). `packN_current` is
  a signed 16-bit value in 0.1 A.
- **`packState`.** The device-level `packState` is decoded like the per-pack `state` (0 standby, 1 charging,
  2 discharging), which the zenSDK table only states for the per-pack field.
- **No kick-start.** Zendure-HA adds a small power boost when the requested power is just at the device's start
  threshold. That is not implemented; the request is sent as-is.
- **`remainInputTime`** is read by Zendure-HA but absent from the zenSDK table; it is simply skipped if the
  device does not report it.
- **Local API length.** The device accepts at most 512 bytes per request; all commands here are far below that.
- **ArduinoJson / `json` component.** The report is parsed with ESPHome's `json::parse_json()`. If a future
  ESPHome release changes that signature, only `ZenSdkComponent::parse_report_()` needs adapting.
- ESP32 only (FreeRTOS task and lwIP sockets).
