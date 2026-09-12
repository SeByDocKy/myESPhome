# `hm` — native ESPHome component for Hoymiles HM micro-inverters

Native ESPHome/ESP-IDF port of OpenDTU's Hoymiles protocol for the **HM**
family of micro-inverters (HM-300 through HM-1500, the classic single/dual/
quad-channel models that predate the HMS line), which communicate over an
**nRF24L01(+)** radio rather than a CMT2300A. Built on top of
[`nrf24l01`](../nrf24l01/README.md), which it drives in a dedicated mode —
**not to be confused with [`hms`](../hms/README.md)**, which is for the
newer HMS family over CMT2300A.

## How HM differs from HMS (why this isn't just a copy of `hms`)

Checked directly against OpenDTU's source, not assumed by analogy:

- **Radio layer**: HM does continuous channel hopping across 5 fixed
  channels (`3, 23, 40, 61, 75`), switching the RX channel every 4ms and
  picking the next channel from the same list for every transmission —
  ported from `HoymilesRadio_NRF.cpp`. This is fundamentally different from
  HMS/CMT2300A's fixed work-channel + occasional boot-frequency handshake.
- **Serial number validation**: not a simple prefix match like HMS — each
  model (`HM_1CH`/`HM_2CH`/`HM_4CH`) uses a specific bit-shift formula plus
  special-case exceptions, ported literally from `isValidSerial()` in each
  file.
- **Byte tables**: `HM_1CH.cpp`/`HM_2CH.cpp`/`HM_4CH.cpp`, offsets/divisors
  ported exactly — the 4-channel model additionally uses a `CALC_CH_UDC`
  computed field (channel 1's voltage mirrors channel 0's, channel 3
  mirrors channel 2's) that HMS never needed.
- **Power-limit control values**: `HmActivePowerControl`'s
  persistent/non-persistent values (`0x0100`/`0x0101`) differ from
  `HmsActivePowerControl`'s (`0x0002`/`0x0003`) — verified directly in
  `ActivePowerControlCommand.cpp`.

What's **shared** with `hms` (same Hoymiles protocol family): CRC8/CRC16,
`CommandAbstract` frame structure, fragment reassembly logic, and the
DTU-serial generation from the ESP32's MAC.

## Installation

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [nrf24l01, hm]
    refresh: 10s
```

## Basic setup

```yaml
spi:
  id: spi_bus
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

nrf24l01:
  id: radio
  spi_id: spi_bus
  cs_pin: GPIO5
  ce_pin: GPIO4
  # channel / pa_level / air_data_rate / crc_length / address_width /
  # dynamic_payloads / tx_address / rx_address are all overridden internally
  # by hm: to match the fixed Hoymiles NRF protocol requirements (250kbps,
  # CRC-16, 5-byte addresses, dynamic payloads, continuous channel hopping)
  # -- only pa_level and the pin assignments really matter here.

hm:
  - id: hm_1
    nrf24l01_id: radio
    sn: "112183001234"        # INVERTER serial number (determines model / DC channel count)
    # dtu_serial: "1999812345"  # optional -- auto-derived from the ESP32 MAC if omitted
    poll_interval: 5s
    # realtime_timeout: 500ms       # optional -- same default as OpenDTU's RealTimeRunDataCommand
    # power_control_timeout: 2000ms # optional -- same default as OpenDTU's ActivePowerControlCommand

sensor:
  - platform: hm
    hm_id: hm_1
    dc_channels:
      - pv0:
          power: {name: "PV0 Power"}
          current: {name: "PV0 Current"}
          voltage: {name: "PV0 Voltage"}
          energy_today: {name: "PV0 Energy Today"}
          energy_total: {name: "PV0 Energy Total"}
    ac:
      voltage: {name: "AC Voltage"}
      current: {name: "AC Current"}
      power: {name: "AC Power"}
      frequency: {name: "AC Frequency"}
      power_factor: {name: "AC Power Factor"}
      reactive_power: {name: "AC Reactive Power"}
    inverter:
      temperature: {name: "Inverter Temperature"}
      power: {name: "Inverter DC Power"}
      energy_today: {name: "Inverter Energy Today"}
      energy_total: {name: "Inverter Energy Total"}
      efficiency: {name: "Inverter Efficiency"}

number:
  - platform: hm
    hm_id: hm_1
    power_percent: {name: "Power Limit (%)"}
    power_absolute: {name: "Power Limit (W)"}

binary_sensor:
  - platform: hm
    hm_id: hm_1
    reachable: {name: "HM Reachable"}
    producing: {name: "HM Producing"}

button:
  - platform: hm
    hm_id: hm_1
    reset_to_output_min: {name: "Reset to 2%"}
    reset_to_output_max: {name: "Reset to 100%"}
    reset_hm: {name: "HM Reset Radio"}

output:
  - platform: hm
    hm_id: hm_1
    power_limit_percent:
      id: hm1_power_limit_percent_output
```

- `reachable` — ported from `InverterAbstract::isReachable()`
  (`rx_failure_count <= REACHABLE_THRESHOLD`), published every time the
  failure counter changes.
- `producing` — ported from `InverterAbstract::isProducing()`
  (`AC power > 0`), published on every fresh telemetry frame.
- `reset_to_output_min`/`reset_to_output_max` write the **persistent**
  power-limit variant (`RelativePersistent = 0x0101` for HM — note this
  differs from HMS's `0x0003`, see the value table above), stored in the
  inverter's own EEPROM. Same rationale as `hms`: meant to be pressed rarely,
  `reset_to_output_min` forces 2% (not 0%, which would stop production
  entirely) and `reset_to_output_max` forces 100%. Each press logs a
  `WARN`-level line so accidental presses are visible.
- `reset_hm` — resets the nRF24L01 chip in hardware (power-down/flush/
  power-up cycle) and immediately re-runs the full Hoymiles NRF
  configuration, without rebooting the ESP32. Same rationale, caveats, and
  suggested `interval:`-based retry automation as
  [`hms`'s `reset_hms`](../hms/README.md#reset_hms--chip-level-reset-no-esp32-reboot)
  — this is a mitigation for an intermittent "won't connect after ESP32
  boot" symptom some users report, not a confirmed root-cause fix.

`hm:` supports multiple instances (`MULTI_CONF`) exactly like `hms:` — several
inverters can share one `nrf24l01:` radio; declare multiple `hm:` entries with
different `sn`, and matching `sensor:`/`number:` blocks per `hm_id`.

`number.power_percent`/`number.power_absolute` use the **non-persistent**
power-limit values, same rationale as `hms`.

## What's in this v1

Shipped: hub (`hm:`), `sensor` platform, `number` platform (non-persistent
power control), `binary_sensor` platform (`reachable`/`producing`), `button`
platform (persistent power-limit reset), `output` platform (float output,
non-persistent).

This gives `hm` full parity with [`hms`](../hms/README.md)'s platform set.
Note that `packet_transport` is **not** something `hm` needs or is missing —
that medium lives one layer down, on the physical radio component
([`nrf24l01`](../nrf24l01/README.md#packet_transport), which already has
one, the same way [`cmt2300a`](../cmt2300a/README.md#example-2--with-packet_transport)
does) for generic sensor-state exchange independent of any Hoymiles protocol.
`hms` doesn't have a `packet_transport` platform either, for the same reason.

## Untested on real hardware

Like every component in this repository at first delivery, this hasn't been
validated against a real HM inverter yet. Expect a debugging round (extra
logging, possibly a wiring or timing fix) similar to what `hms` went through
before its first successful exchange.
