# apszb – APsystems ZigBee micro-inverters (YC600 / QS1 / DS3) for ESPHome

100 % native ESPHome component for the **older generation of APsystems micro-inverters** (YC600, QS1, DS3) that talk
over 2.4 GHz ZigBee (IEEE 802.15.4) to the ECU.

It is a port of [patience4711/ESP32-read-APS-inverters](https://github.com/patience4711/ESP32-read-APS-inverters) with
**two selectable radio back-ends**:

| `radio:` | Hardware | Chips | Frameworks |
|---|---|---|---|
| `native` (default) | nothing else: the IEEE 802.15.4 radio of the chip is used directly | ESP32-C6, ESP32-H2 | esp-idf |
| `znp` | a TI **CC2530** running the Z-Stack *ZNP* firmware on an UART (what the original project uses) | any ESP32 | esp-idf or Arduino |

> **Status: experimental.** Protocol encoders/decoders are unit tested against the data found in the original project,
> and the whole hub (pairing, polling, fragmentation, throttling, offline detection) is simulated on a PC with fake
> inverters, for both back-ends (the `znp` one against a fake CC2530). The on-air behaviour has **not** been validated
> against real inverters yet — see [Verification status](#verification-status).
>
> `radio: znp` reproduces the commands of the original, working project and is the safer choice if the native radio does
> not get any answer from your inverters.

## Why not the ESP Zigbee stack (esp-zigbee-sdk / ZBOSS)? (`radio: native`)

The ZigBee *radio* of the ESP32-C6/H2 is exactly what is needed, the ZigBee *stack* is not:

* the APsystems network is **unsecured** (the CC2530 reports `SecurityUse = 0`), uses a **proprietary profile**
  (`0x0F05`, endpoint `0x14`) and inverters are **not joined** through the standard association/commissioning procedure
  (they are "paired" with proprietary broadcast frames);
* the Espressif stack is built for standard secured ZigBee PRO networks (joining, trust center, ZCL). It exposes raw APS
  requests, but I found no way to disable NWK security or to address nodes that never joined;
* the existing ESPHome Zigbee components only implement end-device/router roles, no coordinator.

So this component uses the ESP-IDF **IEEE 802.15.4 driver** (`esp_ieee802154`) as a bare PHY/MAC and implements the few
ZigBee NWK/APS frames that the Z-Stack firmware of the CC2530 produced (`aps_protocol.cpp`).

## Requirements

* `radio: native`: ESP32-C6 or ESP32-H2, **esp-idf** framework
* `radio: znp`: any ESP32 (esp-idf or Arduino) and a CC2530 module flashed with the Z-Stack ZNP firmware (the one the
  original project documents), connected to a free UART (3.3 V, 115200 baud)
* ESPHome 2026.9 or newer (tested for configuration/code generation with 2026.9.1)
* A Wi-Fi channel that does not overlap 802.15.4 channel 16 (2430 MHz → Wi-Fi channels 3…7). Use 1 or 11.

## Quick start

```yaml
esp32:
  board: esp32-c6-devkitc-1
  variant: esp32c6
  framework:
    type: esp-idf

external_components:
  - source: github://SeByDocKy/myESPhome
    components: [apszb]

apszb:
  id: aps
  ecu_id: "D8A3011B9780"      # 12 hex digits; PAN id = bytes 1,0 -> 0xA3D8
  channel: 16
  poll_interval: 60s
  inverters:
    - id: inv1
      serial: "408000158215"  # printed on the inverter
      type: yc600             # yc600 | qs1 | ds3
```

See [`code/test_apszb.yaml`](../../code/test_apszb.yaml) (native radio) and [`code/test_apszb_znp.yaml`](../../code/test_apszb_znp.yaml) (CC2530) for all entities.

### Quick start with a CC2530 (`radio: znp`)

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf          # arduino works too

uart:
  id: uart_zb
  tx_pin: GPIO17           # -> CC2530 RX (P0_2)
  rx_pin: GPIO16           # <- CC2530 TX (P0_3)
  baud_rate: 115200        # required by the ZNP firmware

apszb:
  id: aps
  radio: znp
  uart_id: uart_zb
  reset_pin: GPIO4         # optional: RESET_N of the CC2530 (driven low for 50 ms at start-up)
  ecu_id: "D8A3011B9780"
  inverters:
    - id: inv1
      serial: "408000158215"
      type: yc600
```

Wiring: `3V3`, `GND`, ESP TX → CC2530 RX, ESP RX ← CC2530 TX, and optionally one GPIO → `RESET_N`. The component runs
the same start-up sequence as the original firmware (coordinator, PAN id from the ECU id, channel 16, endpoint `0x14`,
profile `0x0F05`); the Z-Stack firmware handles MAC/NWK/APS, routing, fragment reassembly and acknowledgements itself.
Differences with the native back-end: `rssi` is not available (the CC2530 only reports an LQI), and `tx_power`,
`promiscuous`, `nwk_protocol_version`, `discover_route`, `mac_ack`, `link_status_interval` and `route_request_interval`
are ignored.

### Pairing

Power the inverters from the panels (daylight) and leave `auto_pair: true`: unpaired inverters are paired automatically
at boot (and every 2 minutes while unpaired). You can also press the `pair` button. The short address of the inverter is
stored in flash; you can also force it with `short_address: 0x103A`.

## Hub options

| Option | Default | Description |
|---|---|---|
| `radio` | `native` | `native` (ESP32-C6/H2 radio) or `znp` (CC2530 on an UART) |
| `uart_id` | – | UART connected to the CC2530, required (and only allowed) with `radio: znp` |
| `reset_pin` | – | optional GPIO wired to the CC2530 `RESET_N` (`radio: znp` only) |
| `ecu_id` | required | 12 hex digits identifying the ECU. Anything works for a DIY ECU; use the real ECU id to replace a physical ECU. |
| `channel` | `16` | 802.15.4 channel (11…26). The original firmware uses 16. |
| `tx_power` | `10` | dBm (−24…20), `native` only |
| `poll_interval` | `60s` | time between two polls of each inverter |
| `response_timeout` | `2.5s` | wait for an answer (2 retries) |
| `offline_after` | `3` | failed polls before an inverter is reported offline (power sensors go to 0) |
| `auto_pair` | `true` | pair unknown inverters automatically |
| `link_status_interval` | `15s` | ZigBee *link status* broadcast (router behaviour), `0s` disables, `native` only |
| `route_request_interval` | `60s` | ZigBee *many-to-one route request* (concentrator behaviour), `0s` disables, `native` only |
| `nwk_protocol_version` | `2` | NWK protocol version in the frames (2 = ZigBee PRO, 1 = ZigBee 2006), `native` only |
| `discover_route` | `false` | set the NWK "discover route" flag, `native` only |
| `mac_ack` | `true` | request an IEEE 802.15.4 acknowledgement for unicast frames, `native` only |
| `log_frames` | `false` | log every TX/RX frame in hex (`native`: MAC frames, `znp`: ZNP commands) |
| `promiscuous` | `false` | receive frames of every PAN (sniffer mode), `native` only |

Per inverter: `serial`, `type`, `short_address` (optional), `calibration` (W added to the power limit, as in the original).

## Entities (all optional, platform `apszb`, `inverter: <id>`)

* **sensor**: `ac_voltage`, `frequency`, `temperature`, `power_total`, `energy_total`, `dc_voltage_1…4`, `dc_current_1…4`,
  `power_1…4`, `energy_1…4`, `link_quality`, `rssi`, `uptime`
* **text_sensor**: `status` (unpaired / online / offline), `serial`, `short_address`
* **binary_sensor**: `online`
* **number**: `max_power` (throttling in W, read back from the inverter after each change)
* **button**: `pair`, `refresh`, `query_max_power`, `reboot_inverter`

The inverters do not report an instantaneous power: like the original firmware, `power_x` is derived from the increase
of the energy counter between two polls. `energy_total` is accumulated by the ESP (persisted in flash).

## How the CC2530 commands map to this component (`radio: native`)

| Original (ZNP over UART) | Here |
|---|---|
| `ZB_WRITE_CONFIGURATION` EXTADDR/PANID/CHANLIST/LOGICAL_TYPE | `esp_ieee802154_set_extended_address / _panid / _channel / _short_address(0x0000)` |
| `AF_REGISTER` endpoint 0x14, profile 0x0F05 | constants in `aps_protocol.h` |
| `AF_DATA_REQUEST` to a short address (poll, query, throttle, reboot) | MAC data frame + NWK data + APS data (unicast, cluster 0x0006) |
| `AF_DATA_REQUEST` to 0xFFFF ("normal operation") | NWK/APS broadcast (cluster 0x0006) |
| `AF_DATA_REQUEST_EXT` broadcast ×4 (pairing) | NWK/APS broadcast, clusters 0x020D, 0x020C, 0x010F, 0x0101, MAC destination PAN 0xFFFF |
| `AF_INCOMING_MSG` (answers, cluster 0x0106) | parsed from the received frame, APS fragments reassembled and acknowledged |
| `ZDO_SRC_RTG_IND`, route handling of Z-Stack | optional link status / many-to-one route request broadcasts |

## Verification status

### `radio: znp`

The ZNP commands are the ones of the original project (`ZIGBEE_COORDINATOR.ino`, `ZIGBEE_POLLING.ino`, `ZIGBEE_PAIR.ino`):

| Purpose | ZNP command |
|---|---|
| start-up | `ZB_WRITE_CONFIGURATION` (startup option, EXTADDR, logical type, PAN id, channel list), `SYS_RESET_REQ`, `AF_REGISTER`, `ZB_START_REQUEST`, then wait for `ZDO_STATE_CHANGE_IND` = coordinator |
| poll / query / throttle / reboot | `AF_DATA_REQUEST` (dst = inverter short address, endpoints `0x14`, cluster `0x0006`) |
| "normal operation" | `AF_DATA_REQUEST` to `0xFFFF` |
| pairing (4 broadcasts) | `AF_DATA_REQUEST_EXT`, broadcast address mode, destination PAN `0xFFFF` |
| answers | `AF_INCOMING_MSG` (cluster `0x0106`, source = inverter short address, LQI) |

If the CC2530 does not answer the first command, the component logs an error and retries every 15 s (check the TX/RX
wiring, 115200 baud and that the ZNP firmware is flashed). Transmissions wait until the coordinator is up.

## Verification status

Verified (see `code/apszb_tests/`):

* application payloads (poll, query, throttle for YC600/DS3, pairing) are byte-identical to the original firmware,
* decoders reproduce the values of the sample frames in the original sources (DS3: 231.1 V, 50.00 Hz, 18.0 °C,
  36.8 V / 37.0 V; YC600: 221.1 V, 50.01 Hz, 34.7 V / 34.3 V; programmed power 300 W / 600 W),
* frame builder ↔ parser round trip, APS fragmentation reassembly and APS acknowledgements,
* hub state machine on a PC with the real ESPHome host sources and a fake YC600 + fake DS3: auto-pairing, polling,
  fragmented answers, power computation, throttling read-back, offline detection after the night,
* ZNP back-end: framing checked against frames of the original log, start-up sequence, TX encoding of poll / pairing /
  normal operation, parser resynchronisation, silent-modem detection and retry (`code/apszb_tests/test_znp.cpp`), and the complete
  hub against a **fake CC2530 on a pseudo terminal** with the ESPHome host UART (`code/apszb_tests/sim_znp/`): coordinator start-up,
  auto-pairing, polling, power computation, max power read-back, offline detection,
* the YAML schema validates with `esphome config` (both radios, invalid combinations are rejected) and C++ generation succeeds; the ESP-IDF radio wrapper was only
  syntax-checked against stubs of the documented driver API (**no ESP32 build was run**).

**Not verified with a real CC2530 / on the air.** For `radio: znp` the commands are copied from the original project but
the combination with this component has only been run against a simulated modem. For `radio: native` the NWK/APS headers are what a Z-Stack coordinator is expected to produce, derived from the
ZNP commands, but nobody has compared them to a capture of a real ECU yet. Things to check first if the inverters stay
silent (compare with a CC2531 + sniffer capture of the original ECU or ESP32-ECU):

1. `log_frames: true` and `logger: level: DEBUG`: do pairing broadcasts leave and do any frames come back?
2. `promiscuous: true`: look at what the inverters send (PAN id, addresses, NWK version, secured flag).
3. Try `nwk_protocol_version: 1`, `mac_ack: false`, `discover_route: true`, or disable `link_status_interval` /
   `route_request_interval`.
4. The pairing broadcast uses MAC destination PAN `0xFFFF` (the ZNP command carried `DstPanId = 0xFFFF`).
5. `esp_ieee802154_transmit()` is given `length = MAC frame + 2` (FCS added by the hardware). If transmitted frames look
   two bytes too short/long in a sniffer, this is the place to fix (`TxFrame::finish`).

## Development

```sh
# protocol unit tests (PC)
cd code/apszb_tests && g++ -std=c++17 -I../../components/apszb test_protocol.cpp ../../components/apszb/aps_protocol.cpp -o t && ./t
# ZNP (CC2530) back-end unit tests
g++ -std=c++17 -I../../components/apszb test_znp.cpp ../../components/apszb/znp_radio.cpp ../../components/apszb/aps_protocol.cpp -o t_znp && ./t_znp
```

`code/apszb_tests/sim/` (native radio, stub radio) and `code/apszb_tests/sim_znp/` (CC2530 on a pseudo terminal `/tmp/apszb_sim_tty`) contain the
hub simulations (`sim_main.cpp` + `t.yaml`): generate the sources with the ESPHome *host* platform
(`esphome compile --only-generate`, with a copy of the component whose `DEPENDENCIES` is emptied and ESP32-only calls
removed), build them together with `sim_main.cpp` (which `#include`s the generated `main.cpp`) and run. Delete
`~/.esphome/prefs/<name>.prefs` between runs, the paired short addresses are persisted there.

## License

MIT License

Copyright (c) 2026 e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
