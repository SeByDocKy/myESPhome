# indevolt

Native ESPHome component for **INDEVOLT** plug & play batteries, using the battery's own **local HTTP API**
(no cloud, no `http_request` component, no Home Assistant add-on). It runs on an ESP32 and is modelled on the
official Home Assistant integration <https://github.com/INDEVOLT/homeassistant-indevolt>.

| Model (`model:`) | Generation | Notes |
| --- | --- | --- |
| `SF2000`, `PF2000` | Gen2 | SolidFlex 2000 / PowerFlex 2000 (power module + up to 5 SFA/PFA battery packs) |
| `BK1600`, `BK1600_ULTRA` | Gen1 | BK1600 / BK1600 Ultra (reduced point set) |

> **Status: written without access to a real device.** The protocol, the points and the command sequences were taken
> from the source of the official Home Assistant integration, and the component was exercised end to end against a
> simulated device that mimics that API (polling, parsing, writes, chunked responses, device unreachable and recovery).
> It has **not** been run on real hardware yet -- see [Caveats](#caveats-please-verify-on-your-hardware) and
> [Reporting results](#reporting-results).

## Requirements

- An ESP32 (ESP-IDF framework; the component uses a FreeRTOS task and lwIP sockets).
- The battery on the same LAN as the ESP, with a fixed IP address (DHCP reservation recommended).
- The device's **API / "OpenData" HTTP mode enabled in the INDEVOLT app**. The official integration only supports
  this mode.
- Minimum firmware (from the official integration's README): BK1600 / BK1600 Ultra `V1.3.0A_R006.072_M4848_00000039`,
  SolidFlex2000 / PowerFlex2000 `CMS: V1406.07.002E`.

## How it talks to the battery

The device exposes a small JSON-RPC-like API on `http://<ip>:8080/rpc/...`, without authentication. Everything is
addressed by numeric **points** (register-like numbers):

| Purpose | Request | Response |
| --- | --- | --- |
| Identification | `GET /rpc/Sys.GetConfig` | `{"device": {"type", "sn", "f_ver"}, ...}` |
| Read points | `POST /rpc/Indevolt.GetData?config={"t":[7101,6001,...]}` | `{"7101": 1, "6001": 1000, ...}` |
| Write points | `POST /rpc/Indevolt.SetData?config={"f":16,"t":<point>,"v":[<values>]}` | `{"result": true}` |

(The `config` JSON is percent-encoded on the wire, exactly like the aiohttp client of the Home Assistant integration
sends it.) The POST requests have an empty body.

How the component is built:

- All HTTP I/O runs in **its own FreeRTOS task**; ESPHome's main loop never waits for the network. Only the main
  loop publishes states.
- Only the points of the **entities you configure** are polled, in requests of `batch_size` points (8 by default, as
  in the official integration). A polling cycle runs one request at a time; queued writes are served **between** two
  requests, so a control command never waits for a whole cycle.
- A failed request aborts the cycle (an unreachable battery costs one timeout per cycle, not one per request).
  The `online` binary sensor goes off after 3 failed cycles.
- **Three polling loops.** The configured points are split in three lists, each polled on its own interval:
  - *fast* (`fast_poll_interval`): what drives automations -- battery power, state, SOC, work mode, PV / AC / meter /
    bypass power (points 6000, 6001, 6002, 7101, 1501, 1664-1667, 2101, 2108, 11016, 21028, 667);
  - *normal* (`normal_poll_interval`): everything else that moves -- voltages, currents, temperatures, energy
    counters, battery main board and per-pack data;
  - *slow* (`slow_poll_interval`): near-static data -- rated capacity, serial numbers, firmwares and the read-back of
    settings (role, bypass, grid charging, light, limits, backup SOC).

  With a full configuration this is about 2 requests per fast cycle, 5 per normal cycle and 3 per slow cycle. The
  faster list is always served first, and queued writes go between two requests. Any entity can be moved to another
  loop with `poll: fast | normal | slow` (e.g. a pack voltage in the fast loop); if several entities share a point,
  the fastest request wins. The duration of every request is logged at VERBOSE level, which tells how low the
  intervals can go on a given battery.
- The first fast poll runs 5 s after boot, the normal one 0.5 s later and the slow one 1 s later, then each loop on its own interval. The identification (`Sys.GetConfig`) is read once
  (retried until it works) and logged at INFO level; the model reported by the device is checked against `model:`.

## Installation

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [indevolt]
    refresh: 10s
```

## Configuration

```yaml
indevolt:
  - id: sf2000
    ip_address: 192.168.1.70
    model: SF2000              # SF2000 / PF2000 / BK1600 / BK1600_ULTRA
    fast_poll_interval: 10s     # power, SOC, state, work mode
    normal_poll_interval: 30s   # voltages, temperatures, counters, packs
    slow_poll_interval: 120s    # identification, firmware, settings read-back
```

Several batteries on one ESP: add one `indevolt:` block per device and set `indevolt_id:` on every platform entry
(`MULTI_CONF`).

| Option | Default | Description |
| --- | --- | --- |
| `ip_address` | required | IP of the battery |
| `ip_port` | `8080` | Port of the local API |
| `model` | required | `SF2000`, `PF2000`, `BK1600`, `BK1600_ULTRA` -- selects the point set (Gen1 / Gen2) |
| `fast_poll_interval` | `30s` | Time between two *fast* polling cycles (power, SOC, state, work mode; min 1 s; 30 s is the official integration's default) |
| `normal_poll_interval` | `60s` | Time between two *normal* polling cycles (voltages, currents, temperatures, counters, packs; min 1 s) |
| `slow_poll_interval` | `300s` | Time between two *slow* polling cycles (identification, firmware, settings read-back; min 1 s) |
| `timeout` | `8s` | Time budget of one HTTP transaction (0.5 s .. 60 s) |
| `batch_size` | `8` | Points requested per `GetData` call (1 .. 16) |
| `max_charge_power` | Gen2: 2400, Gen1: 1200 | Largest charge request, in W (up to 10800 W, e.g. for a cluster) |
| `max_discharge_power` | Gen2: 2400, Gen1: 800 | Largest discharge request, in W |
| `min_power` | Gen2: 50, Gen1: 0 | Requests with a smaller absolute power are sent as **standby** |
| `charge_target_soc` | `100` | Third value of the real-time command while charging (%) |
| `discharge_target_soc` | `5` | Third value of the real-time command while discharging / in standby (%) |
| `auto_realtime_mode` | `true` | Switch the device to *Real-Time Control* (work mode 4) before a power command if it is in another mode |

Entities that only exist on one generation are rejected at compile time with a clear message when `model:` does not
match (for example `rated_capacity` on a `BK1600`).

### Sensors

`dc_channels`, `ac` and `battery` follow the structure of the author's other components (`hms`, `hmsw`, `deyemi`,
`zensdk`); lists are **0-indexed** (`pv0`, `pv1`, ..., `pack0`, `pack1`, ...).

| Key | Point(s) | Gen |
| --- | --- | --- |
| `rated_capacity` | 142 | 2 |
| `dc_output_power` | 1501 | 1, 2 |
| `daily_production` | 1502 | 1, 2 |
| `total_production` (Wh -> kWh, x0.001) | 1505 | 1 |
| `meter_power` | 21028 (Gen1) / 11016 (Gen2) | 1, 2 |
| `emergency_power_supply` (%) | 6105 | 1, 2 |
| `bypass_power` / `bypass_input_energy` (Wh) | 667 / 11034 | 2 |
| `dc_channels: pvN: power` | 1664 + N (N = 0..3; Gen1: N = 0..1) | 1, 2 |
| `dc_channels: pvN: voltage` / `current` | 1600 + N / 1632 + N | 2 |
| `ac: input_power` / `output_power` | 2101 / 2108 | 1, 2 |
| `ac: input_energy` | 2107 | 1, 2 |
| `ac: output_energy` / `offgrid_output_energy` | 2104 / 2105 | 2 |
| `ac: voltage` / `frequency` | 2600 / 2612 | 2 |
| `battery: soc` / `power` | 6002 / 6000 | 1, 2 |
| `battery: daily_charge_energy` / `daily_discharge_energy` | 6004 / 6005 | 1, 2 |
| `battery: total_charge_energy` / `total_discharge_energy` | 6006 / 6007 | 1, 2 |
| `battery: mb_soc` / `mb_voltage` / `mb_current` / `mb_temperature` | 9000 / 9004 / 9013 / 9012 | 2 |
| `battery: mb_cell1_voltage` / `mb_cell2_voltage` | 9009 / 9011 | 2 |
| `battery: packs: packN: soc` | 9016, 9035, 9054, 9149, 9202 | 2 |
| `battery: packs: packN: voltage` | 9020, 9039, 9058, 9153, 9206 | 2 |
| `battery: packs: packN: current` | 19173 .. 19177 | 2 |
| `battery: packs: packN: temperature` | 9030, 9049, 9068, 9163, 9216 | 2 |
| `battery: packs: packN: cell1_voltage` / `cell2_voltage` | 9021 / 9023, 9040 / 9042, 9059 / 9061, 9154 / 9156, 9219 / 9222 | 2 |

Values are published **as the device sends them**, like the official integration does (units: W, V, A, Hz, kWh, %, degC).

### Text sensors

| Key | Point | Gen |
| --- | --- | --- |
| `device_type`, `serial_number`, `device_firmware` | `Sys.GetConfig` (`type`, `sn`, `f_ver`) | 1, 2 |
| `working_mode` | 7101: Outdoor Portable / Self-consumed Prioritized / Real-Time Control / Charge-Discharge Schedule | 1, 2 |
| `state` | 6001: Static / Charging / Discharging | 1, 2 |
| `role` | 606: Master / Slave / None | 2 |
| `ems_firmware`, `bms_mb_firmware`, `pcs_firmware`, `dcdc_firmware` | 1118, 1109, 1119, 1120 | 2 |
| `battery: mb_sn` | 9008 | 2 |
| `battery: packs: packN: sn` | 9032, 9051, 9070, 9165, 9218 | 2 |
| `battery: packs: packN: dcdc_firmware` / `bms_firmware` | 1136/1137, 1138/1139, 1140/1141, 1142/1143, 1098/1099 | 2 |

Firmware numbers are formatted like the official integration (`10203` -> `1.02.03`, `102` -> `1.02`).

### Binary sensors

`online` (polling succeeds), `meter_connected` (point 7120 == 1000, Gen1/2), `bypass_active` (point 680, Gen2).

### Controls

| Platform | Key | Points (read / write) | Gen |
| --- | --- | --- | --- |
| `number` | `power_setpoint` | -- / 47015 (full real-time command, see below) | 1, 2 |
| `number` | `charging_power` (W, >= 0) | -- / 47015 (full real-time command, charging) | 1, 2 |
| `number` | `discharging_power` (W, >= 0) | -- / 47015 (full real-time command, discharging) | 1, 2 |
| `number` | `backup_soc` (5..100 %) | 6105 / 1142 | 2 |
| `number` | `inverter_input_limit` (50..2400 W) | 11009 / 1138 | 2 |
| `number` | `max_output_power` (50..2400 W) | 11011 / 1147 | 2 |
| `number` | `feed_in_power_limit` (50..2400 W) | 11010 / 1146 | 2 |
| `number` | `power_setting` (raw real-time power) | -- / 47016 | 1, 2 |
| `number` | `soc_setting` (raw real-time target SOC) | -- / 47017 | 1, 2 |
| `select` | `work_mode` (Self-Consumed Prioritized / Real-Time Control / Charge-Discharge Schedule) | 7101 / 47005 | 2 |
| `select` | `realtime_state` (Standby / Charging / Discharging) | 6001 - 1000 / 47015 (first value only) | 1, 2 |
| `select` | `load_setting` (Smart Plug / Meter / Key Load / Custom), write-only | -- / 1 | 2 |
| `switch` | `light` | 7171 / 7265 | 2 |
| `switch` | `grid_charging` (on when 2618 == 1001) | 2618 / 1143 | 2 |
| `switch` | `bypass` | 680 / 7266 | 2 |
| `output` | `charge_power`, `discharge_power` | 47015 | 1, 2 |

Switches and numbers are optimistic: they publish what was sent, and the next poll corrects them if the device
disagreed. Nothing is written to the battery at boot.

## Controlling the power

The real-time control of the official integration is a write of `[state, power, target SOC]` to **point 47015**
(state: 0 standby, 1 charging, 2 discharging) after switching the work mode (**point 47005**) to 4. The component does
exactly that:

- `number: power_setpoint` -- one signed value in W: **> 0 discharge, < 0 charge, 0 standby**.
- `number: charging_power` / `number: discharging_power` -- one number per direction, in W (>= 0), like `input_limit` /
  `output_limit` of the `zensdk` component. Setting one starts that direction and replaces the other one; setting a
  number to 0 only stops **its own** direction (it does not cancel a request made in the other direction). The three
  numbers and the two outputs below always show the same request: after any of them changes it, `charging_power` and
  `discharging_power` display the magnitude of their own direction and 0 for the other, and `power_setpoint` the signed
  value.
- `output: charge_power` / `discharge_power` -- two `FloatOutput`s (0.0 .. 1.0 = 0 .. `max_charge_power` /
  `max_discharge_power`). The effective request is `discharge - charge`, so two independent PID loops can drive one
  battery (same as the `zensdk` component).

In both cases the request is clamped to the configured maximum powers, sent as standby when smaller than `min_power`,
and always sent as the complete triplet. An identical command is not re-sent, except once a minute to re-assert it.
With `auto_realtime_mode: true` (default) the component also writes work mode 4 first when the last known mode (point
7101, polled automatically) is another one -- this is what the official integration's service does. Set it to `false`
if you want to manage the work mode yourself.

Every distinct power request is one HTTP `POST` to the battery: do not make a PID loop change its output many times per
second.

```yaml
sensor:
  - platform: indevolt
    indevolt_id: sf2000
    battery:
      soc:
        name: ${name}_battery_soc

output:
  - platform: indevolt
    indevolt_id: sf2000
    charge_power:
      id: indevolt_charge
    discharge_power:
      id: indevolt_discharge
```

See `test_indevolt.yaml` for a complete example.

## Caveats (please verify on your hardware)

- **Not tested on a real device.** All behaviour is derived from the official integration's source code.
- **Response layout.** The component expects `GetData` to return a flat object keyed by the point number, as the
  official integration reads it (`data.update(result)`; `data.get("7101")`). If none of the requested points is found,
  a warning with the beginning of the response is logged (3 times at most).
- **Value types.** Some points come back as numbers, some as strings (for example `606` is compared with `"1000"` in
  the official integration, and serial numbers are strings). Numeric strings are accepted for numeric entities.
- **Sign conventions.** The sign of `battery: power`, `ac: ...` and `meter_power` is published as sent; the official
  integration does not document it.
- **Target SOC.** The third value of the real-time command is described only as "Target SOC" (5..100 %) in the official
  integration. `charge_target_soc: 100` / `discharge_target_soc: 5` are an interpretation (stop charging at 100 %, stop
  discharging at 5 %); tune them if the device behaves otherwise.
- **Power limits.** 2400 W per SolidFlex/PowerFlex 2000 unit, 1200 W charge / 800 W discharge on the BK1600, are taken
  from the official integration's UI limits. The official integration accepts up to 10800 W on Gen2 (it notes that this
  is only an input limit, not a guaranteed output).
- **Request size.** The official integration requests 8 points per call; whether the device accepts more is unknown --
  raise `batch_size` only if you want to try.
- **Read and write points are different address spaces.** For example point 1142 is a firmware version when read and
  the backup SOC when written. This is how the official integration uses them.
- **Concurrency.** The device's HTTP server is probably small. Avoid polling it from several clients at once (for
  example this component and the Home Assistant integration together) with a short interval.
- ESP32 only (FreeRTOS task and lwIP sockets).

## Reporting results

If you own an INDEVOLT battery and test this component, the most useful information is, with `logger: level: DEBUG`:

1. the `Device type / serial number / firmware` line and the `Sys.GetConfig:` debug line;
2. any `None of the requested points ...` or `Poll failed ...` warning;
3. the output of these commands from a computer on the same network (replace `<ip>`):

```sh
curl http://<ip>:8080/rpc/Sys.GetConfig
curl -X POST 'http://<ip>:8080/rpc/Indevolt.GetData?config=%7B%22t%22:%5B7101,6001,6002,6000%5D%7D'
```

## License

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
