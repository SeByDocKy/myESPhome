# hmsw

Native ESPHome component for the newer **Hoymiles HMS-XXXXW** WiFi
microinverters (integrated DTU, no external radio bridge required).

## Architecture -- how this differs from `hm:`/`hms:`

`hm:` (nRF24L01, 2.4GHz) and `hms:` (CMT2300A, 868MHz) both talk to
inverters that have **no WiFi of their own**: the ESP32 acts as the radio
DTU, replicating Hoymiles' proprietary RF protocol (ported from OpenDTU).

The HMS-**W** series is different: the DTU is built into the inverter
itself, which joins your home WiFi directly. There is no radio to bridge --
this component is a plain **TCP client** that talks to the inverter's own
IP address on the local network, like any other network device. No
`nrf24l01:`/`cmt2300a:` hub is needed or referenced.

```
ESP32 (hmsw:)  <--- WiFi / TCP, port 10081, Protobuf --->  HMS-XXXXW inverter
```

## Wire protocol

Reverse-engineered by the community, not published by Hoymiles. This
component was built from two sources:

- [henkwiedig/Hoymiles-DTU-Proto](https://github.com/henkwiedig/Hoymiles-DTU-Proto) --
  original `.proto` message definitions and the frame format (`protcol.md`).
- [suaveolent/hoymiles-wifi](https://github.com/suaveolent/hoymiles-wifi) --
  a maintained Python reference client; used here to verify the exact
  request/response byte layout (`DTU.generate_message()` /
  `DTU.parse_response()` in `hoymiles_wifi/dtu.py`), since the `.proto`
  message *names* alone are misleading (see below).

### Frame format (unencrypted, non-"extended" variant)

```
byte 0-1   "HM"            magic
byte 2-3   command          2 bytes, e.g. 0xA3 0x03 = realtime data request
byte 4-5   sequence         uint16 BE, incremented per request
byte 6-7   crc16            uint16 BE, CRC-16/MODBUS of the payload only
                             (poly 0x8005 reflected/0xA001, init 0xFFFF, no xorout)
byte 8-9   total_length     uint16 BE, = payload length + 10
byte 10..  payload          protobuf-encoded message
```

One short-lived TCP connection per request (connect -> send -> read one
response -> close), exactly like the Python reference client -- **not** a
persistent socket with a background read loop. Simpler and more robust
across WiFi hiccups on an ESP32 than trying to keep a multi-minute idle
TCP connection alive.

### Command IDs used by this component

| Command   | Sent as (`pending_kind_`) | Encode this message | Decode the response as |
|-----------|---------------------------|----------------------|--------------------------|
| `0xA3 0x02` | `HEARTBEAT`  | `HBResDTO` (small, mostly empty) | `HBReqDTO` (ack) |
| `0xA3 0x03` | `REAL_DATA`  | `RealDataResDTO` (small, mostly empty) | `RealDataReqDTO` (the actual telemetry: `pv_data[]`, one `PvDataMO` per DC/PV channel) |
| `0xA3 0x05` | `POWER_LIMIT` | `CommandResDTO` (`action`/`data`) | `CommandReqDTO` (`err_code`) |
| `0xA3 0x11` | `REAL_DATA_NEW` | `RealDataNewResDTO` (small, `cp` = requested page) | `RealDataNewReqDTO` (`pv_data[]`/`sgs_data[]`/`rp_data[]`, `ap` = total pages) -- see below |

**Naming gotcha, worth documenting because it looks backwards at first
read:** the message the *client* (us) sends is the `...ResDTO`-suffixed
one, and the *inverter's reply* -- the one carrying the actual data -- is
the `...ReqDTO`-suffixed one. This isn't a mistake in our code; it matches
the field names in the original disassembled DTU firmware exactly as
ported by both upstream repos, and was verified against a real, working
Python client (`hoymiles_wifi/dtu.py`), not just the raw `.proto` shapes.

### Protobuf via nanopb

`RealData.proto`, `RealDataNew.proto` and `APPHeartbeatPB.proto` (copied
from `suaveolent/hoymiles-wifi`, which is kept more current than the
original `henkwiedig` repo -- field names differ slightly between the two)
are compiled with [nanopb](https://github.com/nanopb/nanopb) into
`RealData.pb.h/.c`, `RealDataNew.pb.h/.c` and `APPHeartbeatPB.pb.h/.c`,
committed alongside the component (not regenerated at build time, to avoid
needing `protoc` in the ESPHome build). All repeated/string fields are
bounded via `.options` files (`max_size`/`max_count`) so the generated
structs are fully static (fixed-size arrays, no `pb_callback_t`, no
malloc) -- important on an ESP32 with limited heap. The nanopb runtime
(`pb.h`, `pb_common.*`, `pb_encode.*`, `pb_decode.*`) is vendored flat
alongside `hmsw.cpp` for the same reason. If you need other message types
(config read/write, DTU reboot, etc.), regenerate from the `.proto` files
in `suaveolent/hoymiles-wifi`'s `hoymiles_wifi/protobuf/` folder the same
way (a copy of every `.proto`/`.options` used by this component lives in
`proto_hw/` next to this README, for reference -- not compiled at build
time).

### Field scaling

`PvDataMO`'s numeric fields (voltage, current, power, temperature) carry
one implied decimal digit (voltage/power/temperature) or two (current),
same convention as the RF-based HM/HMS protocol -- divide by 10 or 100
accordingly (see `HMSWComponent::handle_real_data_()`).

`RealDataNew`'s `PvMO`/`SGSMO` fields use the **same x10/x100 hypothesis**
in `handle_real_data_new_()`, but this has **not been confirmed against a
real capture** -- the `.proto` comments only say "Volts"/"Watts"/etc. with
no scale documented (same as `RealData`'s, where the convention was only
confirmed empirically after real-hardware testing). If `real_data_new`
readings come back 10x or 100x off, that's the first thing to check.

## Power limit control (`number:`/`output:`)

Relative power limit only (0-100%), same scope as `hm:`/`hms:`'s own
`power_limit_percent` number/output. Ported from
`suaveolent/hoymiles-wifi`'s `async_set_power_limit()`
(`hoymiles_wifi/dtu.py`): a `CommandResDTO` is sent with `action = 8`
(`CMD_ACTION_LIMIT_POWER`) and `data = "A:<permille>,B:0,C:0\r"` (e.g.
`"A:1000,B:0,C:0\r"` for 100.0%), on command `0xA3 0x05`; the inverter's
ack is decoded as `CommandReqDTO` (`err_code`, logged). A pending
power-limit command takes priority over the next realtime-data poll --
sent as soon as the current request/response cycle is idle, not queued
behind `poll_interval` -- same convention as `hm:`/`hms:`.

`B`/`C` are always sent as `0`: they address additional MPPT/string
groups on multi-string models this component doesn't target.

Both the `number:` and `output:` keys are named `persistent_power_percent`
(not just `power_percent`/`power_limit_percent`) -- same key on both
platforms, to make it explicit that every write goes to the inverter's
EEPROM regardless of which one you use -- see the caution section below.

```yaml
number:
  - platform: hmsw
    hmsw_id: my_hmsw
    persistent_power_percent:
      name: "Power Limit"

output:
  - platform: hmsw
    hmsw_id: my_hmsw
    persistent_power_percent:
      id: hmsw_power_output
```

## `RealDataNew` (`0xA3 0x11`) -- richer/diagnostic data source

An alternative to classic `RealData` (`0xA3 0x03`), selected with the hub's
`data_source:` option -- **one or the other for the periodic poll, never
both**, to respect the ~2s minimum spacing the DTU firmware appears to
enforce between any two requests (see "Polling interval" below). Both code
paths stay in the component regardless of which one is configured.

```yaml
hmsw:
  id: my_hmsw
  host: 192.168.1.50
  data_source: real_data_new   # default: real_data
```

What it adds over classic `RealData`:

- **`energy_daily`** per DC channel (`PvMO.energy_daily`) -- classic
  `RealData`'s `PvDataMO` has no daily-energy field at all.
- **A power-limit readback** (`SGSMO.power_limit`) -- the currently-applied
  limit as reported by the inverter itself, not just the last value this
  component sent.
- **Diagnostics**: `firmware_version`, `warning_number`, `link_status`,
  `crc_checksum` (see the entity table below for how each is mapped, or
  deliberately left out).

Ported from `suaveolent/hoymiles-wifi`'s `async_get_real_data_new()`
(`hoymiles_wifi/dtu.py`): the request is a `RealDataNewResDTO` with
`time_ymd_hms`/`offset` (a fixed `28800`, matching the Python client's
`OFFSET` constant -- not derived from our actual timezone)/`time`/`cp`
(the requested page, `0` on the first request of a poll cycle). The
inverter's `RealDataNewReqDTO` reply carries `ap` (total page count) in
its first response; if `ap > 1`, this component automatically fetches
`cp = 1 .. ap-1` as additional short-lived TCP requests (same one-connection-
per-request model as everything else here) before the poll cycle is
considered complete -- see `HMSWComponent::handle_real_data_new_()` and
`loop()`. **A single, non-gateway HMS-XXXXW is expected to report `ap=1`**
(no extra round trips); the "extended"/gateway multi-sub-device case this
component doesn't implement (see below) is where pagination across several
inverters' worth of data would actually come into play.

`sgs_data[0]` (`SGSMO`, "single grid-tied system") is where this component
reads the AC/diagnostic block from -- HMS-XXXXW is single-phase, so
`tgs_data`/`rsd_data` (three-phase / other device-type message types also
defined in `RealDataNew.proto`) are decoded but not wired up to anything.
`ac:` sensors are shared between `real_data`/`real_data_new` (same physical
quantities, just a different source field per data source); only
`energy_daily`/the power-limit readback/the diagnostic fields are
`real_data_new`-only, and stay unpublished when `data_source: real_data`
(the default) is in use.

**Not yet tested against real hardware** -- like the rest of this
component's first cut, this was built from the `.proto` shapes and the
Python reference client's request/response logic, not from a real
`RealDataNew` capture. Expect to iterate on the field scaling and the
`sgs_data`/`rsd_data`/`tgs_data` assumption above once real logs are
available.

## What this component does NOT (yet) implement

- **Encryption.** `hoymiles-wifi` supports an optional AES variant when
  the inverter has encryption enabled; this component only implements the
  plaintext path (the default for a local, unauthenticated connection).
- **The "extended" frame format** (a different, longer header used by some
  commands/models, and the only known way to address a gateway managing
  several sub-devices behind one IP).
- **Absolute (Watts) power limit, WiFi/config changes, DTU reboot, etc.**
  -- only the relative (%) limit is implemented, same scope as `hm:`/
  `hms:` started with.

## Polling interval

`poll_interval` defaults to **30s**, not something shorter. Per community
reports on the underlying protocol
([suaveolent/ha-hoymiles-wifi's README](https://github.com/suaveolent/ha-hoymiles-wifi/blob/main/README.md)),
the inverter firmware itself appears to enforce a ~30s minimum spacing
between requests regardless of what the client asks for, and polling below
~32s (120s on newer firmware) has been reported to **disable the
inverter's own sync with the Hoymiles cloud**. Going lower than 30s here
is not expected to get you fresher data -- it risks losing cloud sync for
no benefit.

Separately, the Python reference client enforces at least ~2s between *any*
two requests it sends (regardless of command), which is also why
`RealDataNew`'s multi-page fetch (see above) isn't a speed win over classic
`RealData` even when it does need more than one page.

## CAUTION: power limit and EEPROM wear

> Per [suaveolent/ha-hoymiles-wifi's README](https://github.com/suaveolent/ha-hoymiles-wifi/blob/main/README.md):
> "Please refrain from using the current power limitation feature for zero
> feed-in, as it may lead to damaging the inverter due to excessive writes
> to the EEPROM."

This is a real concern for a PID/closed-loop power-regulation use case
(zero feed-in / zero injection): unlike `hm:`/`hms:`, which distinguish a
non-persistent limit (RAM only) from an explicit persistent one (EEPROM
write, via a separate `..._persistent` call), **no non-persistent (RAM-only)
variant of this command is known to exist for HMS-XXXXW** -- confirmed by
three independent community sources (`suaveolent/ha-hoymiles-wifi`'s README,
GitHub issues #54 and #16 on that repo, and `MicHi07i/ioBroker.hoymiles-wifi`'s
README). This is why both the `number:` and `output:` config keys are named
`persistent_power_percent`:
`HMSWComponent::set_persistent_power_limit_percent()` **always** writes to
the inverter's EEPROM. Do **not** wire this into a fast PID loop (e.g.
`zero-injection`-style control against a Shelly's active power) -- treat it
as safe only for occasional/manual limit changes until a RAM-only command is
confirmed to exist for this series.

## Entity reference

Every platform is defined in its own sub-directory (`sensor/`,
`binary_sensor/`, `text_sensor/`, `number/`, `output/`), each with a
`hmsw_id:` pointing back at the `hmsw:` hub. Keys marked **RealDataNew
only** stay unpublished (no entity ever appears in Home Assistant) when
`data_source: real_data` (the default) is in use.

### `sensor:` -- up to 4 `dc_channels` entries (`pv0`..`pv3`), one `ac` block, plus top-level keys

| Key (under `dc_channels: - pvN:`) | Device class | Unit | Notes |
|---|---|---|---|
| `power` | power | W | 1 decimal |
| `current` | current | A | 2 decimals |
| `voltage` | voltage | V | 1 decimal |
| `energy_total` | energy | Wh | total_increasing |
| `temperature` | temperature | °C | 1 decimal |
| `energy_daily` | energy | Wh | state class `total` (resets daily) -- **RealDataNew only** |

| Key (under `ac:`) | Device class | Unit | Notes |
|---|---|---|---|
| `voltage` | voltage | V | 1 decimal |
| `current` | current | A | 2 decimals |
| `power` | power | W | 1 decimal |
| `frequency` | frequency | Hz | 2 decimals |
| `power_factor` | -- | (none) | 3 decimals |
| `reactive_power` | -- | VAR | 1 decimal |

Note: with `data_source: real_data_new`, the `ac:` block and
`dc_channels: .../temperature` are populated from `sgs_data[0]`/`pv_data[]`
instead of `RealDataReqDTO`'s `grid_*`/`pv_temp` fields -- same YAML keys,
same physical quantities, different source message on the wire.

| Key (top-level) | Device class | Unit | Notes |
|---|---|---|---|
| `rssi` | signal_strength | dB | inverter's own WiFi signal, diagnostic. **`real_data` only** -- `RealDataNew`'s `RpMO` has no equivalent field this component maps to it (yet) |
| `power_limit` | power | W | diagnostic; readback of the currently-applied power limit (`SGSMO.power_limit`) -- **RealDataNew only**, raw scale unverified |
| `warning_number` | -- | (none) | diagnostic, `mdi:alert` -- **RealDataNew only** |
| `link_status` | -- | (none) | diagnostic, raw firmware value (meaning/range not yet confirmed, deliberately not a `binary_sensor`) -- **RealDataNew only** |

`crc_checksum` is **not** exposed as an entity -- logged at `VERBOSE` only
(`ESP_LOGV`), see `handle_real_data_new_()`.

### `text_sensor:`

| Key | Notes |
|---|---|
| `firmware_version` | diagnostic, raw integer as reported by `SGSMO.firmware_version` (not yet decoded into a dotted version string) -- **RealDataNew only** |

### `binary_sensor:`

| Key | Device class | Notes |
|---|---|---|
| `reachable` | connectivity | diagnostic; whether the last request/response cycle succeeded |

### `number:` -- writable, 0-100%, step 1

| Key | Notes |
|---|---|
| `persistent_power_percent` | relative power limit; **every write hits the inverter's EEPROM** (see caution above) |

### `output:` -- standard float output, 0.0-1.0

| Key | Notes |
|---|---|
| `persistent_power_percent` | same command/key as the `number:` above, exposed as an `output:` for use with e.g. a `pid` climate/controller; **every write hits the inverter's EEPROM** |

## Configuration example

Pulled straight from [SeByDocKy/myESPhome](https://github.com/SeByDocKy/myESPhome)
on every build (`refresh: 10s` -- drop it, or set it much longer, once
you're not actively iterating on the component itself):

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [hmsw]
    refresh: 10s
```

Or, for local development against a working copy of the repo instead:

```yaml
external_components:
  - source:
      type: local
      path: components

hmsw:
  id: my_hmsw
  host: 192.168.1.50   # the inverter's own IP address
  port: 10081          # default, usually no need to change
  poll_interval: 30s   # default; see "Polling interval" above before going lower
  heartbeat_interval: 20s
  data_source: real_data   # default; "real_data_new" adds energy_daily/diagnostics, see above

sensor:
  - platform: hmsw
    hmsw_id: my_hmsw
    dc_channels:
      - pv0:
          power:
            name: "PV0 Power"
          voltage:
            name: "PV0 Voltage"
          current:
            name: "PV0 Current"
          energy_total:
            name: "PV0 Energy Total"
    ac:
      voltage:
        name: "AC Voltage"
      power:
        name: "AC Power"
      frequency:
        name: "AC Frequency"
    rssi:
      name: "WiFi Signal (inverter)"

binary_sensor:
  - platform: hmsw
    hmsw_id: my_hmsw
    reachable:
      name: "HMSW Reachable"

# Only populated when data_source: real_data_new is set above.
text_sensor:
  - platform: hmsw
    hmsw_id: my_hmsw
    firmware_version:
      name: "Firmware Version"
```

## Configuration example -- `data_source: real_data_new`

Same layout as above, with `data_source: real_data_new` set on the hub and
the extra entities (`energy_daily`, `power_limit`, `warning_number`,
`link_status`, `firmware_version`) wired up. `ac:`/`dc_channels:
.../temperature`/`rssi` stay exactly as they are above -- see the note
under "Entity reference" on which fields keep working and which don't
(`rssi` is `real_data`-only) once you switch. Remember this is **not yet
tested against real hardware** (see the `RealDataNew` section above).

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [hmsw]
    refresh: 10s

hmsw:
  id: my_hmsw
  host: 192.168.1.50
  poll_interval: 30s
  heartbeat_interval: 20s
  data_source: real_data_new

sensor:
  - platform: hmsw
    hmsw_id: my_hmsw
    dc_channels:
      - pv0:
          power:
            name: "PV0 Power"
          voltage:
            name: "PV0 Voltage"
          current:
            name: "PV0 Current"
          energy_total:
            name: "PV0 Energy Total"
          energy_daily:
            name: "PV0 Energy Today"
    ac:
      voltage:
        name: "AC Voltage"
      power:
        name: "AC Power"
      frequency:
        name: "AC Frequency"
    power_limit:
      name: "Power Limit (inverter readback)"
    warning_number:
      name: "Warning Number"
    link_status:
      name: "Link Status"

binary_sensor:
  - platform: hmsw
    hmsw_id: my_hmsw
    reachable:
      name: "HMSW Reachable"

text_sensor:
  - platform: hmsw
    hmsw_id: my_hmsw
    firmware_version:
      name: "Firmware Version"
```

## Configuration example -- two HMS-XXXXW inverters

Thanks to `MULTI_CONF`, one ESP32 can poll several HMS-XXXXW units at once
-- one `hmsw:` entry per inverter (a YAML list, each with its own `id:` and
`host:`), and every platform entry (`sensor:`/`binary_sensor:`/etc.) points
back at the right hub via its own `hmsw_id:`. No serial number is needed
for either unit -- the IP address alone routes each TCP connection (see
"Architecture" above); each hub also gets its own independent
`poll_interval`/`data_source` if you want them to differ.

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [hmsw]
    refresh: 10s

hmsw:
  - id: hmsw_roof
    host: 192.168.1.50   # first inverter
    poll_interval: 30s
  - id: hmsw_garage
    host: 192.168.1.51   # second inverter
    poll_interval: 30s

sensor:
  - platform: hmsw
    hmsw_id: hmsw_roof
    dc_channels:
      - pv0:
          power:
            name: "Roof PV0 Power"
    ac:
      power:
        name: "Roof AC Power"

  - platform: hmsw
    hmsw_id: hmsw_garage
    dc_channels:
      - pv0:
          power:
            name: "Garage PV0 Power"
    ac:
      power:
        name: "Garage AC Power"

binary_sensor:
  - platform: hmsw
    hmsw_id: hmsw_roof
    reachable:
      name: "HMSW Roof Reachable"
  - platform: hmsw
    hmsw_id: hmsw_garage
    reachable:
      name: "HMSW Garage Reachable"
```

## Status

First working cut, built from protocol documentation and a verified
reference client -- **not yet tested against real HMS-XXXXW hardware.**
Expect to iterate from real logs, the same way `hm:`/`hms:` were debugged
to their current state (see their own README/commit history).
