# `hms` — native ESPHome component for Hoymiles HMS micro-inverters

Native ESPHome/ESP-IDF port of OpenDTU's Hoymiles protocol (`lib/Hoymiles` +
`lib/CMT2300a`), for **HMS** micro-inverters (HMS-300 through HMS-2000,
1/2/4 DC-input models) communicating over a **CMT2300A** radio. Built on top
of the [`cmt2300a`](../cmt2300a/README.md) component, which it drives in a
dedicated mode.

## What's ported faithfully

Checked line by line against OpenDTU's real source, not from memory:

- **Frame format**: `CommandAbstract.cpp` (CRC8, 32-bit target/source
  addressing, packet structure), `MultiDataCommand.cpp`
  (`RealTimeRunDataCommand`, CRC16 over concatenated fragments),
  `DevControlCommand.cpp` (`ActivePowerControlCommand`),
  `RequestFrameCommand.cpp`, `ChannelChangeCommand.cpp`.
- **Fragment reassembly**: `InverterAbstract::addRxFragment` /
  `verifyAllFragments` (selective retransmit up to `MAX_RETRANSMIT_COUNT=5`,
  full resend up to `MAX_RESEND_COUNT=4`).
- **Per-model byte tables**: `HMS_1CH.cpp`, `HMS_2CH.cpp`, `HMS_4CH.cpp`
  (offsets, divisors, signedness — copied exactly).
- **Serial number decoding**: the same `isValidSerial()` prefix tables as
  those three files — the DC channel count is determined automatically at
  runtime from the inverter's serial number.
- **CMT2300A register banks**: `cmt2300a_params_860.h` /
  `cmt2300a_params_900.h` (Hoymiles-specific, different from the generic
  433MHz bank used by plain `cmt2300a`), merged 64-byte FIFO, fast channel
  register (`FREQ_CHNL`/`FREQ_OFS`).
- **`ChannelChangeCommand`**: sent at the inverter's "boot" frequency once it
  becomes unreachable (`REACHABLE_THRESHOLD=3` consecutive failures), exactly
  like `HMS_Abstract::sendChangeChannelRequest()` / `Hoymiles.cpp`'s main
  loop.
- **DTU serial generation**: equivalent to `Utils::generateDtuSerial()`,
  derived from the ESP32's MAC address.
- **Non-persistent vs. persistent power limits**: verified against
  `ActivePowerControlCommand.cpp`'s exact value table (see below).

## Scope / known simplifications

- **Irradiation fields (`FLD_IRR`) are not exposed** — they require a
  per-string peak-power config value that isn't implemented.
- Only **Relative/Absolute, both persistent and non-persistent**, power
  limits are supported (see the button section below); DevInfo,
  AlarmLog, SystemConfigPara, and GridProfile reads are not implemented —
  only real-time telemetry and power control.
- **Reception is by polling** the `INT_FLAG` register on every `loop()` tick
  rather than by GPIO interrupt — safer with a bit-banged SPI-like
  interface (the low-level functions aren't interrupt-safe) and fast enough
  given ESPHome's tick rate.
- **Tx is non-blocking**: transmission and its `TX_DONE` wait are spread
  across multiple `loop()` ticks instead of blocking ESPHome's main loop for
  tens of milliseconds per poll cycle.

## Installation

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [cmt2300a, hms]
    refresh: 10s
```

## Basic setup

```yaml
cmt2300a:
  id: cmt_radio
  clk_pin: GPIO9
  sdio_pin: GPIO10
  cs_pin: GPIO11
  fcs_pin: GPIO12
  # gpio2_pin / gpio3_pin are not needed here: hms drives the CMT2300A
  # directly in "external mode", bypassing the generic packet path.

hms:
  - id: hms_1
    cmt2300a_id: cmt_radio
    sn: "1410A01121AE"        # INVERTER serial number (determines model / DC channel count)
    # dtu_serial: "1999812345"  # optional -- auto-derived from the ESP32 MAC if omitted
    frequency_band: eu_860      # eu_860 (868MHz) or us_900 (915MHz)
    poll_interval: 5s
```

### `hms:` hub options

| Key | Required | Default | Description |
|---|---|---|---|
| `cmt2300a_id` | no (auto-detected if only one exists) | — | The `cmt2300a:` instance this inverter is reached through |
| `sn` | yes | — | **Inverter** serial number, 12 hex characters. Determines the model (1/2/4 DC channels) and is used to address the inverter |
| `dtu_serial` | no | auto (from MAC) | Your own DTU identity, 12 hex characters. Left unset, it's derived from the ESP32's MAC — every `hms:` instance sharing the same ESP32 gets the same value by default, matching how a single real DTU addresses several inverters |
| `frequency_band` | no | `eu_860` | `eu_860` (868MHz) or `us_900` (915MHz). **Must be identical** across every `hms:` instance sharing the same `cmt2300a_id` |
| `poll_interval` | no | `5s` | How often real-time data is requested from this inverter |

`hms:` supports multiple instances (`MULTI_CONF`), see
[Multiple inverters on one radio](#multiple-inverters-on-one-radio) below.

## `sensor` platform

```yaml
sensor:
  - platform: hms
    hms_id: hms_1
    dc_channels:
      - pv0:
          power: {name: "PV0 Power"}
          current: {name: "PV0 Current"}
          voltage: {name: "PV0 Voltage"}
          energy_today: {name: "PV0 Energy Today"}
          energy_total: {name: "PV0 Energy Total"}
      - pv1:
          power: {name: "PV1 Power"}
          current: {name: "PV1 Current"}
          voltage: {name: "PV1 Voltage"}
          energy_today: {name: "PV1 Energy Today"}
          energy_total: {name: "PV1 Energy Total"}
    ac:
      voltage: {name: "AC Voltage"}
      current: {name: "AC Current"}
      power: {name: "AC Power"}
      frequency: {name: "AC Frequency"}
      power_factor: {name: "AC Power Factor"}
      reactive_power: {name: "AC Reactive Power"}
    inverter:
      temperature: {name: "Inverter Temperature"}
      power: {name: "Inverter DC Power"}          # sum of all DC channels
      energy_today: {name: "Inverter Energy Today"}
      energy_total: {name: "Inverter Energy Total"}
      efficiency: {name: "Inverter Efficiency"}     # AC power / total DC power
    rssi: {name: "HMS RSSI"}
```

- `dc_channels` is a list of 1-4 single-key entries (the key, e.g. `pv0`, is
  a free-form label used only for YAML readability — the actual mapping to
  the inverter's real strings is by list order). **Declaring more channels
  than the model actually has is caught at compile time** with a clear error
  (the decoder cross-checks the declared count against `sn`'s prefix).
  Declaring fewer is fine (you can choose to monitor only one string).
- Every field is optional — omit whatever sensor you don't want created.
- Icons are pre-assigned by quantity type: `mdi:current-dc` /
  `mdi:current-ac` for currents, `mdi:power` for powers (W, var),
  `mdi:counter` for energies, `mdi:angle-acute` for power factor,
  `mdi:metronome` for frequency, `mdi:sine-wave` for voltages,
  `mdi:thermometer` for temperature, `mdi:percent` for efficiency,
  `mdi:signal` for RSSI.

## `binary_sensor` platform

```yaml
binary_sensor:
  - platform: hms
    hms_id: hms_1
    reachable: {name: "HMS Reachable"}
    producing: {name: "HMS Producing"}
```

- `reachable` — ported from `InverterAbstract::isReachable()`
  (`rx_failure_count <= REACHABLE_THRESHOLD`). Published every time the
  failure counter changes.
- `producing` — ported from `InverterAbstract::isProducing()`
  (`AC power > 0`). Published on every fresh telemetry frame.

## `number` platform — non-persistent power control

```yaml
number:
  - platform: hms
    hms_id: hms_1
    power_percent: {name: "Power Limit (%)"}    # 0-100
    power_absolute: {name: "Power Limit (W)"}   # 0-4000 (soft UI bound; the inverter enforces its own hardware cap)
```

Both send `ActivePowerControlCommand` with the **non-persistent** variant
(`RelativeNonPersistent = 0x0001` / `AbsoluteNonPersistent = 0x0000`,
verified against `ActivePowerControlCommand.cpp`'s value table) — the
inverter never writes this to its own flash/EEPROM, and reverts to its last
persisted limit after any power cycle. This is the right choice for frequent
dynamic control (PID loops, zero-injection automations, etc.).

## `output` platform — same, as a standard float output

```yaml
output:
  - platform: hms
    hms_id: hms_1
    power_limit_percent:
      id: hms1_power_limit_percent_output
```

A standard ESPHome `output::FloatOutput` (value range `0.0`-`1.0`, enforced
by the base class), mapped to 0-100% relative power and sent through the
same **non-persistent** path as `number.power_percent`. Lets you drive the
inverter's output from anything that can target a float output — a `pid:`
climate/control loop, a `template` automation, etc.

## `button` platform — persistent reset shortcuts

```yaml
button:
  - platform: hms
    hms_id: hms_1
    reset_to_output_min: {name: "Reset to 2%"}
    reset_to_output_max: {name: "Reset to 100%"}
```

Unlike `number`/`output`, these two buttons write the **persistent** variant
(`RelativePersistent = 0x0003`, stored in the inverter's own EEPROM) — the
value survives a mains power cycle. Meant to be pressed rarely:

- `reset_to_output_min` forces the relative limit to **2%**, not 0% — at 0%
  the HMS stops producing entirely rather than clamping down, which usually
  isn't what you want.
- `reset_to_output_max` forces it back to **100%**.

Each button press logs a `WARN`-level line ("PERSISTENT write") so accidental
presses are visible in your logs.

## Multiple inverters on one radio

A single CMT2300A can talk to several HMS inverters. Declare several `hms:`
entries pointing at the same `cmt2300a_id`, and matching platform blocks per
inverter:

```yaml
cmt2300a:
  id: cmt_radio
  clk_pin: GPIO9
  sdio_pin: GPIO10
  cs_pin: GPIO11
  fcs_pin: GPIO12

hms:
  - id: hms_1
    cmt2300a_id: cmt_radio
    sn: "1410A01121AE"
    frequency_band: eu_860

  - id: hms_2
    cmt2300a_id: cmt_radio
    sn: "1410A0112200"
    frequency_band: eu_860   # must match hms_1's

sensor:
  - platform: hms
    hms_id: hms_1
    ac:
      power: {name: "Inverter 1 AC Power"}
  - platform: hms
    hms_id: hms_2
    ac:
      power: {name: "Inverter 2 AC Power"}
```

What makes this safe, rather than the two instances stepping on each other's
register writes:

- **A cooperative lock inside `cmt2300a`**: each `hms:` instance must
  acquire it before starting a full exchange (Tx + response wait +
  retransmits) and releases it when done. An instance that fails to acquire
  it simply retries on the next tick, without shifting its own poll
  schedule.
- **Radio init runs once**: whichever `hms:` instance starts up first
  configures the register banks / merged FIFO; the others detect this and
  reuse it — with a compile-time check that every instance's
  `frequency_band` agrees (the chip physically can't serve two bands at
  once).
- **DTU identity is naturally shared**: since `dtu_serial` defaults to a
  value derived from the ESP32's MAC, every `hms:` instance on the same
  board auto-assigns itself the same one — matching how a real DTU
  addresses multiple inverters under one identity.

There's no hard limit on the number of inverters — add as many `hms:`
entries (plus their matching platform blocks) as you have physical units.
Keep in mind the radio is only ever talking to one inverter at a time
(no concurrent exchanges are possible on a single chip), so with many
inverters sharing one radio the effective per-inverter refresh rate will be
lower than `poll_interval` under contention.

## Troubleshooting

- **Nothing but `"Onduleur injoignable" / "inverter unreachable"` warnings,
  in a loop**: expected if the inverter isn't powered, or hasn't yet
  received a `ChannelChangeCommand` to move off its post-boot frequency.
  Give it a few polling cycles once powered — the reachability recovery is
  automatic.
- **`verify_all_fragments_ -> 254/255`**: no response at all within the
  retry budget — check antenna/wiring, and that `frequency_band` matches
  your region and the inverter's actual configured frequency.
- **`CRC16 invalide` / `CRC16 invalid` warnings**: a response was received
  but failed the whole-frame checksum — usually weak signal or interference,
  not a config issue.
- **Model shows as unrecognized at boot**: `sn`'s first 4 hex digits didn't
  match any known HMS prefix table (`1124`, `1143`/`1144`/`1410`/`114a`,
  `1164`/`1166`/`1420`) — double-check the serial printed on the inverter's
  label.
