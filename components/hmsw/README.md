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

### Non-blocking by design, and why `ip_address:` is IPv4-only

Everything in this component's request/response handling
(`HMSWComponent::loop()`/`handle_connecting_()`/`handle_sending_()`/
`handle_receiving_()`) is a small state machine driven from `loop()`:
non-blocking sockets (`setblocking(false)`), `connect()`/`write()`/`read()`
all checked for `EWOULDBLOCK`/`EAGAIN` and retried on the next tick, and a
per-state deadline checked via `millis()` -- never a `delay()` call
anywhere. Each poll/heartbeat/command opens one short-lived TCP connection
(see below), and none of that ever blocks the main ESPHome loop.

The one place that *could* still block despite all of the above:
`esphome::socket::set_sockaddr()` falls back to `lwip_getaddrinfo()` -- a
blocking, synchronous DNS lookup -- for anything that isn't already a
literal IP. Since a fresh socket/connection is opened for **every single
request**, a hostname in `ip_address:` would mean a potential multi-second
stall on every poll if DNS is slow or unreachable, not just once at boot.
`ip_address:` is validated at config time (`validate_ipv4_literal()` in
`__init__.py`) to only accept a literal dotted-quad IPv4 address for
exactly this reason -- a hostname is rejected with a clear error instead of
becoming an intermittent field symptom.

## Supported hardware

"HMS-XXXXW" in this component's name refers to the naming pattern, not one
specific wattage. The distinguishing feature is the **`W` right after the
wattage figure** -- that's Hoymiles' own marker for "Wi-Fi integrated" (a
built-in DTU, joining your WiFi directly), as opposed to the plain
`HMS-XXXX-nT` models (no `W`), which have no WiFi/DTU of their own and
need an external `DTU-Lite`/`DTU-Pro` dongle talking sub-1GHz RF -- those
are what `hm:`/`hms:` (this repo's other components) target instead, not
`hmsw:`. Per Hoymiles' own product pages, the Wi-Fi-integrated lineup is:

| Family | PV inputs | Models |
|---|---|---|
| `-1T` | 1 | `HMS-300W-1T`, `HMS-350W-1T`, `HMS-400W-1T`, `HMS-450W-1T`, `HMS-500W-1T` |
| `-2T` | 2 | `HMS-600W-2T`, `HMS-700W-2T`, `HMS-800W-2T`, `HMS-900W-2T`, `HMS-1000W-2T` |
| `-4T` | 4 | `HMS-1600DW-4T`, `HMS-1800DW-4T`, `HMS-2000DW-4T` (note the extra `D` in this tier's naming, not just `W`) |

(Sources: Hoymiles' own product pages for the
[`-1T`](https://www.hoymiles.com/products/hms-300w-350w-400w-450w-500w-1t-wi-fi-integrated.html),
[`-2T`](https://open-energy.hoymiles.com/en/product/hms-600w-700w-800w-900w-1000w-2t-wi-fi-integrated-eu/)
and [`-4T`](https://www.hoymiles.com/product-release/hoymiles-wi-fi-integrated-microinverters-are-set-to-make-small-home-solar-more-cost-effective.html)
families.)

This component's `dc_channels:` array is sized for up to 4 entries
(`pv0`..`pv3`), so it covers the `-4T` tier's 4 PV inputs as well as the
smaller `-1T`/`-2T` units (just use fewer `dc_channels` entries).

**Verified vs. assumed:** the protocol itself (this component's whole
reason to exist) was only ever reverse-engineered/tested by upstream
projects against the **`HMS-800W-2T`** -- `suaveolent/hoymiles-wifi`'s own
README says as much: developed for that specific model,
"compatibility with other inverters from the series is untested at the
time of writing" (on their end too, not just this component's). This
component itself has **not yet been tested against any real hardware** at
all (see "Status" at the bottom) -- so treat every model above as "should
work, same family/protocol", not "confirmed", until proven on real
hardware.

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
| `0xA3 0x04` | `ALARM_LIST_FETCH` (alarm-list step 2) | `WInfoResDTO` (`offset`/`time`) | `WInfoReqDTO` (`mWInfo[]`, one `WInfoMO` per warning entry) -- see below |
| `0xA3 0x05` | `POWER_LIMIT` or `ALARM_LIST_REQUEST` (alarm-list step 1) | `CommandResDTO` (`action`/`data`) | `CommandReqDTO` (`err_code`) -- same wire command and ack shape for both, disambiguated internally via `pending_kind_` |
| `0xA3 0x11` | `REAL_DATA_NEW` | `RealDataNewResDTO` (small, `cp` = requested page) | `RealDataNewReqDTO` (`pv_data[]`/`sgs_data[]`/`rp_data[]`, `ap` = total pages) -- see below |
| `0x23 0x05` | `DTU_REBOOT` | `CommandResDTO` (`action`=1, no `data`) | `CommandReqDTO`, best-effort only -- see "DTU reboot" below |

**Naming gotcha, worth documenting because it looks backwards at first
read:** the message the *client* (us) sends is the `...ResDTO`-suffixed
one, and the *inverter's reply* -- the one carrying the actual data -- is
the `...ReqDTO`-suffixed one. This isn't a mistake in our code; it matches
the field names in the original disassembled DTU firmware exactly as
ported by both upstream repos, and was verified against a real, working
Python client (`hoymiles_wifi/dtu.py`), not just the raw `.proto` shapes.

### Other known commands (not implemented here)

The gap between `0xA3 0x05` and `0xA3 0x11` isn't undocumented -- it's just
not wired up in this component yet. `ohAnd/dtuGateway`'s `dtuConst.h`
enumerates the full `0xA3 0x01`-`0xA3 0x16` range (App -> DTU requests all
start `0xA3`; DTU -> App responses to those same actions start `0xA2`,
mirrored 1:1, not listed separately below), plus a handful of `0x83`/
`0xdb`/`0x23`-prefixed variants whose exact role (a different device class?
a different firmware generation? cloud-relay framing?) isn't documented
anywhere upstream, `dtuGateway` included -- they're listed in its source
with no further explanation, so treat the "meaning" column below as a
reasonable guess from the constant's name, not a verified fact:

| Command | Constant (`dtuConst.h`) | Guessed meaning | Reference `.proto` in `proto_hw/` |
|---|---|---|---|
| `0xA3 0x01` | `CMD_APP_INFO_DATA_RES_DTO` | App/device info exchange | `APPInfomationData.proto` |
| `0xA3 0x02` | `CMD_HB_RES_DTO` | Heartbeat | **implemented** (`APPHeartbeatPB.proto`) |
| `0xA3 0x03` | `CMD_REAL_DATA_RES_DTO` | Classic realtime data | **implemented** (`RealData.proto`) |
| `0xA3 0x04` | `CMD_W_INFO_RES_DTO` | Warning-list fetch | **implemented** (`AlarmData.proto`) |
| `0xA3 0x05` | `CMD_COMMAND_RES_DTO` | Generic action (`action` code selects the behaviour -- power limit, alarm-list step 1, DTU/MI reboot, etc.) | **implemented** (`CommandPB.proto`) |
| `0xA3 0x06` | `CMD_COMMAND_STATUS_RES_DTO` | Poll the completion status of a previously-sent `0xA3 0x05` action | not vendored |
| `0xA3 0x07` | `CMD_DEV_CONFIG_FETCH_RES_DTO` | Fetch device configuration | not vendored |
| `0xA3 0x08` | `CMD_DEV_CONFIG_PUT_RES_DTO` | Push device configuration | not vendored |
| `0xA3 0x09` | `CMD_GET_CONFIG` | Get config (DTU/network-level, distinct from 0x07/0x08's per-device config) | `GetConfig.proto` |
| `0xA3 0x10` | `CMD_SET_CONFIG` | Set config | `SetConfig.proto` |
| `0xA3 0x11` | `CMD_REAL_RES_DTO` | Paginated realtime data | **implemented** (`RealDataNew.proto`) |
| `0xA3 0x12` | `CMD_GPST_RES_DTO` | Unclear (possibly time/clock sync) | not vendored |
| `0xA3 0x13` | `CMD_AUTO_SEARCH` | Auto-discovery of MI/inverter units behind the DTU during setup | not vendored |
| `0xA3 0x14` | `CMD_NETWORK_INFO_RES` | Network info (WiFi/IP status) | `NetworkInfo.proto` |
| `0xA3 0x15` | `CMD_APP_GET_HIST_POWER_RES` | Historical power data | `AppGetHistPower.proto` |
| `0xA3 0x16` | `CMD_APP_GET_HIST_ED_RES` | Historical energy data | not vendored |
| `0x83 0x01`, `0x83 0x02`, `0x83 0x03`, `0x83 0x05`-`0x83 0x08` | `CMD_*_RES_DTO_ALT`/`_2` | Alternate framing for (probably) a different device role -- unexplained upstream | not vendored |
| `0xdb 0x07`, `0xdb 0x08` | `CMD_SET_CONFIG_RES`/`CMD_GET_CONFIG_RES` | Config get/set *responses*, oddly on a different command prefix than the `0xA3 0x09`/`0xA3 0x10` *requests* | not vendored |
| `0x23 0x01` | `CMD_CLOUD_INFO_DATA_RES_DTO` | Named after the app's cloud-relay path, but see note below -- not actually cloud traffic on our side | not vendored |
| `0x23 0x05` | `CMD_CLOUD_COMMAND_RES_DTO` | Same "`0x23` = cloud-named" family as `0x23 0x01` above, with genuinely different behaviour for at least `action=1` (DTU reboot) | **implemented** (`CommandPB.proto`, reused) |

**"Cloud" in these two constants' names is misleading -- no data actually
leaves the LAN through them.** In the Hoymiles app, this `0x23`-prefixed
command family (DTU/MI reboot, inverter on/off, "performance data mode",
etc. -- `dtuGateway` implements several besides the reboot, all sharing
this same header) is presumably the one normally sent *through* Hoymiles'
cloud, which then relays it to the DTU. But the `0x23 0x05` header is just
part of the payload the DTU's own firmware expects; it carries no
special routing. Confirmed directly in `dtuGateway`'s source
(`writeReqCommandRestartDevice()`, `src/dtuInterface.cpp`): it's sent over
the exact same local `AsyncClient` TCP connection, to the exact same
`serverIP:serverPort` (the DTU's own LAN IP, port 10081), as every other
request in that project -- there is no second connection to any Hoymiles
server anywhere in its codebase for this command. This component's own
`DTU_REBOOT` request works identically: it goes out over the same
short-lived local TCP connection as everything else in `hmsw.cpp`, to the
`ip_address:`/`ip_port:` you configure on the `hmsw:` hub, nothing more.

Note the `0xA3 0x09` -> `0xA3 0x10` jump (skipping `0x0A`-`0x0F`) is in the
firmware's own numbering, not a typo here -- `dtuGateway`'s source has it
the same way. `0x23 0x05` (DTU reboot) is the only one of these "extra"
commands actually confirmed to behave differently from its `0xA3`
counterpart on real hardware (per `dtuGateway`'s own testing); the rest are
untested guesses from constant names alone, and none of the additional
`.proto` files above have been compiled/vendored into this component -- they
sit in `proto_hw/` purely as a reference for whoever picks one of these up
next.

### Protobuf via nanopb

`RealData.proto`, `RealDataNew.proto` and `APPHeartbeatPB.proto` (copied
from `suaveolent/hoymiles-wifi`, which is kept more current than the
original `henkwiedig` repo -- field names differ slightly between the two),
plus `AlarmData.proto` (ported from `ohAnd/dtuGateway`'s
`include/proto/AlarmData.proto` -- only the messages this component
actually uses, `WInfoMO`/`WInfoReqDTO`/`WInfoResDTO`, not the unrelated
"warning wave data" messages in that same file), are compiled with
[nanopb](https://github.com/nanopb/nanopb) into `RealData.pb.h/.c`,
`RealDataNew.pb.h/.c`, `APPHeartbeatPB.pb.h/.c` and `AlarmData.pb.h/.c`,
committed alongside the component (not regenerated at build time, to avoid
needing `protoc` in the ESPHome build). All repeated/string fields are
bounded via `.options` files (`max_size`/`max_count`) so the generated
structs are fully static (fixed-size arrays, no `pb_callback_t`, no
malloc) -- important on an ESP32 with limited heap. The nanopb runtime
(`pb.h`, `pb_common.*`, `pb_encode.*`, `pb_decode.*`) is vendored flat
alongside `hmsw.cpp` for the same reason. If you need other message types
(config read/write, etc.), regenerate from the `.proto` files
in `suaveolent/hoymiles-wifi`'s `hoymiles_wifi/protobuf/` folder the same
way (a copy of every `.proto`/`.options` used by this component lives in
`proto_hw/` next to this README, for reference -- not compiled at build
time).

### Field scaling

`PvDataMO`'s numeric fields (voltage, current, power, temperature) carry
one implied decimal digit (voltage/power/temperature) or two (current),
same convention as the RF-based HM/HMS protocol -- divide by 10 or 100
accordingly (see `HMSWComponent::handle_real_data_()`).

`RealDataNew`'s `PvMO`/`SGSMO` fields use the **same x10/x100 convention**
in `handle_real_data_new_()`. This was originally a hypothesis carried over
from `RealData`, but it's now corroborated by an independent, real-hardware
implementation: [ohAnd/dtuGateway](https://github.com/ohAnd/dtuGateway)'s
`calcValue(value, divider = 10)` helper divides `SGSMO`/`PvMO` voltage,
current, power, frequency and temperature fields the same way this
component does (default divisor 10, current/frequency by 100) --
see `readRespRealDataNew()` in their `src/dtuInterface.cpp`. Two exceptions
worth noting from that same code:

- `energy_total`/`energy_daily` are read **unscaled off the wire** there too
  (raw Wh) -- `dtuGateway` only divides by 1000 for its own display. This
  component does the same /1000 conversion, but at publish time: `energy_total`
  (both `RealData` and `RealDataNew`) and `energy_today` (`RealDataNew` only,
  the wire field is still named `energy_daily` in the protobuf) are published
  as **kWh**, not raw Wh -- see `sensor/__init__.py`
  (`UNIT_KILOWATT_HOURS`, `accuracy_decimals=3`) and the `/ 1000.0f` in
  `handle_real_data_()`/`handle_real_data_new_()`.
- `SGSMO.power_limit` is printed there as `"%i %%"` (raw integer, **no**
  division, labelled as a percentage) in a commented-out debug line --
  which would make this component's `power_limit` sensor wrong on two
  counts (it currently divides by 10 and reports Watts). That line is
  commented out in `dtuGateway` itself, so treat it as a lead to verify on
  real hardware, not a second confirmation -- if `power_limit` reads 10x too
  low and looks like a percentage instead of Watts, this is why.

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
  ip_address: 192.168.1.50
  data_source: real_data_new   # default: real_data
```

What it adds over classic `RealData`:

- **`energy_today`** per DC channel (`PvMO.energy_daily` on the wire) --
  classic `RealData`'s `PvDataMO` has no daily-energy field at all.
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
`energy_today`/the power-limit readback/the diagnostic fields are
`real_data_new`-only, and stay unpublished when `data_source: real_data`
(the default) is in use.

**Not yet tested against real hardware** -- like the rest of this
component's first cut, this was built from the `.proto` shapes and the
Python reference client's request/response logic, not from a real
`RealDataNew` capture. Expect to iterate on the field scaling and the
`sgs_data`/`rsd_data`/`tgs_data` assumption above once real logs are
available.

## Alarm/warning list (`CMD_ACTION_ALARM_LIST`)

A read-only diagnostic feature, ported from `ohAnd/dtuGateway`'s
`writeReqCommandRequestAlarms()`/`writeReqCommandGetAlarms()`
(`src/dtuInterface.cpp`) -- **not** the same thing as `RealDataNew`'s
`SGSMO.warning_number`, which is a single raw integer this component
already exposes as its own diagnostic sensor and which is never decoded
anywhere, in this component or in `dtuGateway`.

This is a genuine **two-step** request/response sequence, unlike everything
else this component implements:

1. **Step 1** (`ALARM_LIST_REQUEST`): a `CommandResDTO` with
   `action = CMD_ACTION_ALARM_LIST (50)` is sent on `0xA3 0x05` -- the
   *same* wire command and the *same* ack shape (`CommandReqDTO`,
   `err_code`) as the power-limit command. `handle_command_response_()`
   tells the two apart via `pending_kind_` (still valid at that point,
   since it's only reset after the frame is handled -- see
   `on_frame_received_()`/`abort_request_()`), not by anything in the
   wire bytes themselves.
2. **Step 2** (`ALARM_LIST_FETCH`), triggered automatically as soon as step
   1's `err_code == 0` ack arrives: a `WInfoResDTO` (`offset=28800`,
   `time`) is sent on a *different* command, `0xA3 0x04`, and the reply is
   decoded as `WInfoReqDTO` -- a `repeated WInfoMO mWInfo` array, up to 30
   entries.

Each `WInfoMO` entry carries a packed `WCode` (`wcode1 = WCode & 0xFF`,
`wcode2 = (WCode >> 8) & 0xFFFFFF` -- only `wcode1` is looked up here, same
as `dtuGateway`) and two timestamps: the warning is considered **active**
iff `WTime1 != 0 && WTime2 == 0` (started, not yet cleared). This component
looks `wcode1` up in a small ported table (`hmsw_warnings.h`,
`hmsw_warning_label()`) and publishes:

- `active_warnings` (`text_sensor:`) -- a semicolon-joined
  `"<label> (code N)"` list of every currently-active warning, or `"None"`.
- `active_warning_count` (`sensor:`) -- how many are currently active.

**This warning-code table is community-sourced, not an official Hoymiles
document** -- ported verbatim from `dtuGateway`'s `warningCodeMap`
(`readRespCommandGetAlarms()`), which itself marks a few nearby/legacy
codes `[not approved]`/`[Unknown]` in commented-out entries this component
did *not* carry over. A code not in the table still shows up as
`"Unknown warning code (code N)"` rather than being dropped, so nothing is
silently lost -- but treat the *label text* for any given code as a
reasonable guess, not a certainty.

This poll is **off by default** (`alarm_poll_interval: 0s`) and entirely
separate from `poll_interval`/`heartbeat_interval` -- warning data changes
rarely, so there's no reason to fetch it on the same ~30s cadence as
realtime telemetry:

```yaml
hmsw:
  id: my_hmsw
  ip_address: 192.168.1.50
  alarm_poll_interval: 10min   # default: 0s (disabled)

text_sensor:
  - platform: hmsw
    hmsw_id: my_hmsw
    active_warnings:
      name: "Active Warnings"

sensor:
  - platform: hmsw
    hmsw_id: my_hmsw
    active_warning_count:
      name: "Active Warning Count"
```

## DTU reboot (`button:`)

Reboots the **DTU/inverter's own network stack** -- not the ESP32 this
component runs on. Ported from `ohAnd/dtuGateway`'s
`writeReqCommandRestartDevice()`/`requestRestartDevice()`, which that
project exposes both as a manual "Reboot DTU" button in its own web UI
*and* fires automatically as part of its own hang/error recovery (see
`handleError()`). This component wires up both: the manual `button:` below,
and (opt-in, see "Hung-DTU watchdog" further down) the same automatic
trigger `dtuGateway` uses.

**Notable wire-level difference from every other request in this
component**: the reboot request is sent on `0x23 0x05`
(`CMD_DTU_REBOOT`), not `0xA3 0x05` (`CMD_COMMAND`, used for the power
limit and the alarm-list step 1 above) -- a distinct command byte pair
`dtuGateway`'s source calls out explicitly. The payload is a `CommandResDTO`
with `action = CMD_ACTION_DTU_REBOOT (1)`, no `data` string needed.

Response handling is deliberately **best-effort**: `dtuGateway`'s own
decode of this specific reply is known to be buggy (it parses the response
with the wrong message type), so this component logs whatever comes back
without gating anything on it. Expect the DTU/inverter link to drop for a
few seconds after sending this.

```yaml
button:
  - platform: hmsw
    hmsw_id: my_hmsw
    reset_hmsw:
      name: "Reset HMSW (DTU)"
```

## Hung-DTU watchdog (`stale_data_reboot_threshold`)

Reported by several users of `suaveolent/ha-hoymiles-wifi`
([issue #16](https://github.com/suaveolent/ha-hoymiles-wifi/issues/16)):
the DTU can go into a state where it keeps answering realtime-data
requests -- so a plain "is the TCP connection alive" check (this
component's `reachable:` binary_sensor) sees nothing wrong -- but the
values it returns stop updating, frozen at whatever it last measured.
`ohAnd/dtuGateway`'s own troubleshooting notes (`readme_old.md`,
"experiences with the hoymiles HMS-800W-2T") describe both the symptom and
their fix:

> *"sometimes hanging or full shutdown/break of DTU will be prevented by
> sending an active reboot request to dtu (hanging detection at this time
> over grid voltage, should be changing at least within 10 consecutive
> incoming data)"*

This component ports that same detection: on every realtime-data poll
(either data source), the raw AC/grid voltage is compared to the previous
poll's value, and a running count of consecutive **unchanged** readings is
kept:

- **`current_stale_data`** (`sensor:`) -- that running count, published on
  every poll, **regardless of the setting below**. This is deliberate: it
  lets you watch how this count actually behaves on your own installation
  (normal jitter should reset it to 0 constantly) before committing to an
  automatic action.
- **`stale_data_reboot_threshold`** (hub option, default `0` = disabled) --
  once the count reaches this many consecutive identical readings,
  `reboot_dtu()` is triggered automatically (same request as the manual
  `button:` above, same best-effort response handling). `dtuGateway`'s own
  figure was **10** consecutive readings; with the default `poll_interval`
  of 30s that's ~5 minutes of frozen data before a reboot is sent -- adjust
  for your own `poll_interval` if you use a different one.

Why AC/grid voltage by default, not e.g. power: it's the field
`dtuGateway` uses, and grid voltage has natural mains fluctuation second to
second, so on a **grid-tied** installation it's very unlikely to sit
*exactly* unchanged for many consecutive polls unless something really is
stuck (unlike e.g. power at night, which can legitimately read a constant
`0` for hours). The comparison is done on the **raw wire integer**, not
the scaled float, to sidestep any floating-point-equality concerns
entirely.

**Caveat -- AC-coupled installations behind a hybrid inverter/battery
system:** in that setup, the HMS-XXXXW doesn't see raw grid voltage --
it sees whatever AC voltage the hybrid inverter's own output stage is
regulating, which can be held essentially rock-solid, especially
off-grid/backup-powered. Voltage-based detection is prone to **false
positives** there (perfectly healthy DTU, but voltage genuinely doesn't
move enough to reset the counter). For that case, set:

```yaml
hmsw:
  id: my_hmsw
  stale_data_metric: ac_frequency   # default: ac_voltage
```

`ac_frequency` was picked over AC power for this: power drops to exactly
`0` and sits there for hours every single night with nothing wrong,
which would make a power-based watchdog trigger constant false positives
on a schedule -- frequency has no such blind spot, since it's present and
measurable around the clock regardless of production. And on a grid-tied
installation (the hybrid inverter still connected to and synchronized
with the utility grid, not running an isolated/off-grid AC bus), the
hybrid is normally *tracking* the real grid frequency rather than
synthesizing its own fixed one -- so `ac_frequency` should still carry the
grid's natural jitter even when the hybrid pins voltage rock-solid. This
doesn't help on a true off-grid/backup bus where the hybrid is
grid-forming (there, it may well regulate frequency just as tightly as
voltage) -- there's no field this component reads that's known to reliably
distinguish "hung" from "healthy" in that specific case. Neither option's
threshold has been validated against real hardware yet; this is a
reasoned design based on the trade-off, not a tuned default.

```yaml
hmsw:
  id: my_hmsw
  ip_address: 192.168.1.50
  stale_data_reboot_threshold: 10   # default: 0 (disabled) -- see above

sensor:
  - platform: hmsw
    hmsw_id: my_hmsw
    current_stale_data:
      name: "Current Stale Data Count"
```

**Not yet validated against real hardware** -- like the rest of this
first cut, this is ported from documented behaviour/detection logic, not
from a real hang captured and reproduced against this specific component.

## What this component does NOT (yet) implement

- **Encryption.** `hoymiles-wifi` supports an optional AES variant when
  the inverter has encryption enabled; this component only implements the
  plaintext path (the default for a local, unauthenticated connection).
- **The "extended" frame format** (a different, longer header used by some
  commands/models, and the only known way to address a gateway managing
  several sub-devices behind one IP).
- **Absolute (Watts) power limit, WiFi/config changes, etc.** -- only the
  relative (%) power limit, the alarm/warning list and the DTU reboot are
  implemented so far, same starting scope as `hm:`/`hms:`.

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

One data point that nuances this, without resolving it: [ohAnd/dtuGateway](https://github.com/ohAnd/dtuGateway)
-- an independent ESP32 DTU-gateway implementation for the HMS-800W-2T,
using the exact same `CMD_ACTION_LIMIT_POWER` (`action = 8`) command this
component does -- reports, in its `readme_old.md` "experiences with the
Hoymiles HMS-800W-2T" section, real-hardware testing where sending many
power-limit updates within a few seconds of each other (with no rate
limiting) "seem[ed] to creat[e] no problems", running for "days without any
stops". Their actual instability was from combining frequent power writes
*with* frequent realtime-data reads close together in time (traced to a
~31s-or-more read interval, similar to this component's own `poll_interval`
default), not to a confirmed EEPROM failure from the writes themselves. This
doesn't confirm the write is safe long-term (no one has published wear-out
data), but it's a second independent real-hardware account that didn't
observe damage even under a fairly aggressive write cadence -- worth
weighing against the more cautious `ha-hoymiles-wifi` warning above.

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
| `energy_total` | energy | kWh | total_increasing, 3 decimals |
| `temperature` | temperature | °C | 1 decimal |
| `energy_today` | energy | kWh | state class `total` (resets daily), 3 decimals -- **RealDataNew only** |

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
| `active_warning_count` | -- | (none) | diagnostic; count of currently-active alarm-list entries -- only populated if `alarm_poll_interval` is set (see "Alarm/warning list" above) |
| `current_stale_data` | -- | (none) | diagnostic; running count of consecutive polls with an unchanged AC voltage or AC frequency reading (selected via `stale_data_metric`) -- **always populated**, independent of `stale_data_reboot_threshold` (see "Hung-DTU watchdog" above) |

`crc_checksum` is **not** exposed as an entity -- logged at `VERBOSE` only
(`ESP_LOGV`), see `handle_real_data_new_()`.

### `text_sensor:`

| Key | Notes |
|---|---|
| `firmware_version` | diagnostic, raw integer as reported by `SGSMO.firmware_version` (not yet decoded into a dotted version string) -- **RealDataNew only** |
| `active_warnings` | diagnostic; semicolon-joined `"<label> (code N)"` list of currently-active warnings, or `"None"` -- only populated if `alarm_poll_interval` is set (see "Alarm/warning list" above) |

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

### `button:`

| Key | Notes |
|---|---|
| `reset_hmsw` | reboots the DTU/inverter's own network stack, **not** the ESP32 -- see "DTU reboot" above; response handling is best-effort |

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
  ip_address: 192.168.1.50   # the inverter's own IP address
  ip_port: 10081             # default, usually no need to change
  poll_interval: 30s   # default; see "Polling interval" above before going lower
  heartbeat_interval: 20s
  data_source: real_data   # default; "real_data_new" adds energy_today/diagnostics, see above

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
            name: "PV0 Energy Total"   # published in kWh
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

# Reboots the DTU/inverter itself, not the ESP32 -- see "DTU reboot" above.
button:
  - platform: hmsw
    hmsw_id: my_hmsw
    reset_hmsw:
      name: "Reset HMSW (DTU)"
```

## Configuration example -- `data_source: real_data_new`

Same layout as above, with `data_source: real_data_new` set on the hub and
the extra entities (`energy_today`, `power_limit`, `warning_number`,
`link_status`, `firmware_version`) wired up, plus the alarm-list poll, the
DTU-reboot button, and the hung-DTU watchdog (all three independent of
`data_source`, so they'd work just as well in the first example above).
`ac:`/`dc_channels:
.../temperature`/`rssi` stay exactly as they are above -- see the note
under "Entity reference" on which fields keep working and which don't
(`rssi` is `real_data`-only) once you switch. Remember `RealDataNew` itself
is **not yet tested against real hardware** (see the `RealDataNew` section
above).

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [hmsw]
    refresh: 10s

hmsw:
  id: my_hmsw
  ip_address: 192.168.1.50
  poll_interval: 30s
  heartbeat_interval: 20s
  data_source: real_data_new
  alarm_poll_interval: 10min   # default: 0s (disabled) -- see "Alarm/warning list" above
  stale_data_reboot_threshold: 10   # default: 0 (disabled) -- see "Hung-DTU watchdog" above

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
            name: "PV0 Energy Total"   # published in kWh
          energy_today:
            name: "PV0 Energy Today"   # published in kWh
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
    active_warning_count:
      name: "Active Warning Count"
    current_stale_data:
      name: "Current Stale Data Count"

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
    active_warnings:
      name: "Active Warnings"

button:
  - platform: hmsw
    hmsw_id: my_hmsw
    reset_hmsw:
      name: "Reset HMSW (DTU)"
```

## Configuration example -- two HMS-XXXXW inverters

Thanks to `MULTI_CONF`, one ESP32 can poll several HMS-XXXXW units at once
-- one `hmsw:` entry per inverter (a YAML list, each with its own `id:` and
`ip_address:`), and every platform entry (`sensor:`/`binary_sensor:`/etc.) points
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
    ip_address: 192.168.1.50   # first inverter
    poll_interval: 30s
  - id: hmsw_garage
    ip_address: 192.168.1.51   # second inverter
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
