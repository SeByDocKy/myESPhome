# RYLR999 BLE — Native ESPHome LoRa Component (BLE link)

[![ESPHome](https://img.shields.io/badge/ESPHome-native%20component-blue)](https://esphome.io)
[![LoRa](https://img.shields.io/badge/LoRa-868%20%2F%20915%20MHz-green)](https://reyax.com/products/rylr999)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](#license)

Native ESPHome external component for the **REYAX RYLR999** LoRa module, driven over **Bluetooth Low
Energy** instead of a UART. The ESP32 connects to the module as a BLE client (through ESPHome's
`ble_client`), configures the radio with AT commands written to a GATT characteristic, and exchanges
LoRa packets through the same channel.

It is the BLE sibling of the [`rylr998`](../rylr998) component and shares its engine design: non-blocking
command sequencer, TX queue, per-packet air time, automatic recovery, diagnostics sensors, TX power
number, reset button and `packet_transport` platform.

---

## Table of Contents

- [Overview](#overview)
- [Installation](#installation)
- [Configuration reference](#configuration-reference)
  - [Main component](#main-component)
  - [Binary sensor sub-component](#binary-sensor-sub-component)
  - [Sensor sub-component](#sensor-sub-component)
  - [Number sub-component](#number-sub-component)
  - [Button sub-component](#button-sub-component)
  - [Packet transport sub-component](#packet-transport-sub-component)
- [YAML example](#yaml-example)
- [LoRa RF parameter guide](#lora-rf-parameter-guide)
- [Behaviour and reliability](#behaviour-and-reliability)
- [Troubleshooting](#troubleshooting)
- [Known limitations](#known-limitations)
- [Credits](#credits)
- [License](#license)

---

## Overview

| | |
|---|---|
| Module | REYAX **RYLR999** (BLE + LoRa, 0–30 dBm) |
| Link to the ESP32 | BLE GATT (`ble_client`), ESP32 family with Bluetooth only (ESP-IDF) |
| Radio configuration | `AT+ADDRESS`, `AT+BAND`, `AT+PARAMETER`, `AT+NETWORKID`, `AT+CRFOP` |
| Max binary payload | **120 bytes** (hex-encoded: 240 ASCII characters) |
| Transmission | queued (4 packets), one at a time, spaced by the computed air time |
| Ready for use when | the 6 configuration steps have been acknowledged (`lora_ready`) |

The module behaves as a **transparent serial bridge**: whatever is written to the write
characteristic is what a UART-connected RYLR998 would receive, and every line it prints
(`+OK`, `+ERR=n`, `+RCV=...`, `+READY`) comes back as GATT notifications. A command can therefore
be written in several chunks. The component uses the negotiated MTU (`MTU - 3` bytes per write,
20 bytes until the MTU is known) and reassembles the notified lines itself.

---

## Installation

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome@main"
    components: [rylr999_ble]
    refresh: 10s
```

ESPHome pulls the component from GitHub at build time. The two examples in this folder
(`rylr999_ble_example_a.yaml`, `rylr999_ble_example_b.yaml`) are a complete working pair.

The module must be **powered and advertising**, and you need its BLE MAC address and the UUIDs of
its GATT service/characteristics (a BLE scanner app such as nRF Connect shows them).

---

## Configuration reference

### Main component

```yaml
esp32_ble_tracker:

ble_client:
  - mac_address: "90:39:5E:3F:6D:81"
    id: my_ble

rylr999_ble:
  id: my_rylr999
  ble_client_id: my_ble
  service_uuid: "F000C0C0-0451-4000-B000-000000000000"
  characteristic_uuid: "F000C0C1-0451-4000-B000-000000000000"
  notify_characteristic_uuid: "F000C0C2-0451-4000-B000-000000000000"
  address: 1
  frequency: 868MHz
  spreading_factor: 9
  signal_bandwidth: 125kHz
  coding_rate: 1
  preamble_length: 12
  network_id: 10
  tx_power: 20
```

| Option | Default | Description |
|---|---|---|
| `ble_client_id` | *required* | The `ble_client` connected to the module. |
| `service_uuid` | *required* | GATT service that holds the characteristics (16, 32 or 128 bit). |
| `characteristic_uuid` | *required* | Characteristic AT commands are **written** to. |
| `notify_characteristic_uuid` | = `characteristic_uuid` | Characteristic the answers arrive on, when the module exposes a separate notify-only one (it is the case of the module this component was tested with). |
| `address` | `0` | LoRa address of this module (0–65535). |
| `frequency` | `915MHz` | Carrier frequency. Use `868MHz` in Europe. |
| `spreading_factor` | `9` | 5–11. See [constraints](#spreading-factor-vs-bandwidth). |
| `signal_bandwidth` | `125kHz` | `125kHz`, `250kHz` or `500kHz`. |
| `coding_rate` | `1` | 1–4 (4/5 … 4/8). |
| `preamble_length` | `12` | 4–24, can only differ from 12 with `network_id: 18`. |
| `network_id` | `18` | 3–15 or 18. Modules must share it to talk to each other. |
| `tx_power` | `22` | 0–30 dBm (applied with `AT+CRFOP`). |
| `air_time` | `true` | After each `AT+SEND` the next one waits for the **computed air time** of that packet (plus a small margin). `false` restores a fixed 600 ms guard (legacy). |
| `min_tx_interval` | `0ms` | Optional extra floor on the spacing between two `AT+SEND`. Packets wait in the queue; they are never dropped because of it. Usually not needed. |

#### Parameter details

- **Same radio settings on both sides.** `frequency`, `spreading_factor`, `signal_bandwidth`,
  `coding_rate`, `preamble_length` and `network_id` must be identical on every node; only `address`
  differs.
- **`air_time: true`** is what keeps the module from answering `+ERR=17` ("last TX not completed"):
  a 120-byte packet at SF9/125 kHz is on air for about 1.25 s, a fixed 600 ms guard is too short for it.
- The BLE UUIDs are sent as-is; the notify characteristic must support `NOTIFY` (or `INDICATE`),
  otherwise an explicit error is logged at connection.

### Binary sensor sub-component

```yaml
binary_sensor:
  - platform: rylr999_ble
    lora_ready:
      name: ${name}_lora_ready
```

`lora_ready` is `ON` once the BLE link is up **and** the radio configuration has been acknowledged by
the module. It goes `OFF` when the BLE link drops, when the module restarts, or while it is being
re-configured. Packets are refused while it is `OFF`.

### Sensor sub-component

```yaml
sensor:
  - platform: rylr999_ble
    rssi:
      name: ${name}_rssi
    snr:
      name: ${name}_snr
    last_error:
      name: ${name}_last_error
```

| Key | Description |
|---|---|
| `rssi` | RSSI (dBm) of the last received packet. |
| `snr` | SNR (dB) of the last received packet. |
| `last_error` | Number `n` of the last `+ERR=n` reported by the module (`0` = none yet). See the [error codes](#error-codes). |

### Number sub-component

```yaml
number:
  - platform: rylr999_ble
    tx_power:
      name: ${name}_tx_power
      restore_value: false     # default
```

Changes the TX power at runtime (0–30 dBm). The change is non-blocking: the value is stored and
`AT+CRFOP` is sent as soon as the module is idle. The displayed value is the one the module
confirmed. With `restore_value: true` the last value set from the UI survives a reboot; otherwise
`tx_power` from the YAML is always used at boot.

### Button sub-component

```yaml
button:
  - platform: rylr999_ble
    reset:
      name: ${name}_reset
```

Sends `AT+RESET`. The module reboots and the whole configuration is re-applied when it reports
`+READY` (about one second). Useful if the radio side ever gets stuck without the BLE link
dropping.

### Packet transport sub-component

Shares ESPHome **sensors** and **binary sensors** between two devices over LoRa through ESPHome's
generic [`packet_transport`](https://esphome.io/components/packet_transport/) framework (the same
mechanism as `udp` and `espnow`). The `rylr999_ble` platform only carries the frames.

**Sending side**

```yaml
packet_transport:
  platform: rylr999_ble
  rylr999_ble_id: my_rylr999
  update_interval: 10s
  encryption: !secret encryption_key       # optional
  sensors:
    - itemperature                         # ids of local sensors to publish
  binary_sensors:
    - relay_state                          # ids of local binary sensors to publish
  providers:
    - name: rylr999_ble_2                  # name of the remote device
      encryption: !secret encryption_key
```

**Receiving side** — one entity per remote value:

```yaml
sensor:
  - platform: packet_transport
    provider: rylr999_ble_2
    remote_id: lywsd_temperature           # id of the sensor on the remote device
    name: ${name}_temperature_received
```

> A frame must fit in **120 bytes**, which is what the component reports as its maximum packet size.
> Each value is sent with its **id as text**: a sensor costs about `6 + len(id)` bytes and a binary
> sensor `3 + len(id)` bytes, plus a small header (and some overhead with `encryption`). Short ids keep
> the frame small. When the values do not fit the framework splits them into several frames sent back to
> back; the TX queue holds **4 packets**, so share few enough values to stay within that.
>
> `packet_transport` sends the whole set every `update_interval` **and** immediately each time a
> shared value changes. Choose `update_interval` and the values you share so that the air time of
> the frames is much smaller than the interval — see the table below.

---

## YAML example

### Two nodes exchanging a temperature and a relay state

Each node:

- shares its **internal temperature** (a sensor) and the **state of its relay** with the other one;
- shows the other node's temperature and relay state as local entities;
- **follows the other node's relay**: the received binary sensor drives a local template switch
  (`${name}_mirror`), which switches a local output;
- exposes `lora_ready`, RSSI, SNR, last error, TX power and a reset button.

`packet_transport` only carries **sensors and binary sensors**, so the data flows like this:

```
node A                                                    node B
relay (gpio switch) --on_turn_on/off--> relay_state ──LoRa──> remote_relay_state --on_state--> mirror (template switch) --> mirror_out (gpio)
                                      (template binary sensor)  (packet_transport binary sensor)
```

and the same in the opposite direction. There are two distinct switches per node, so there is no
feedback loop: `relay` (what this node sends) and `mirror` (what the other node sends).

The two nodes use the **same file**; only the `substitutions` block differs.
Both files are provided in this folder: `rylr999_ble_example_a.yaml` and `rylr999_ble_example_b.yaml`.

**Node A** (`rylr999_ble_example_a.yaml`):

```yaml
# Two RYLR999 (BLE) nodes exchanging their internal temperature and the state of a relay.
# Flash this file on node A and the twin file (rylr999_ble_example_b.yaml) on node B: only the
# substitutions differ.
#
# packet_transport only shares sensors and binary sensors, so the relay (a switch) is mirrored
# into a binary sensor (`relay_state`) which is the entity that is sent over LoRa.
# In the other direction, the binary sensor received from the other node (`remote_relay_state`)
# drives a template switch (`mirror`) that switches a local output: the other node's relay is
# followed here.
substitutions:
  name: rylr999_ble_a
  compagnon: rylr999_ble_b
  ble_mac: "90:39:5E:3F:6D:81"      # BLE MAC address of the RYLR999 wired to this node
  lora_address: "1"
  relay_pin: GPIO4       # local relay, its state is sent to the other node
  mirror_pin: GPIO5      # output that follows the relay of the other node

esphome:
  name: ${name}
  min_version: 2026.7.0

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

logger:
  level: debug

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
ota:
  platform: esphome

external_components:
  - source: "github://SeByDocKy/myESPhome@main"
    components: [rylr999_ble]
    refresh: 10s

esp32_ble_tracker:
  scan_parameters:
    active: true

ble_client:
  - mac_address: ${ble_mac}
    id: my_ble

rylr999_ble:
  id: my_rylr999
  ble_client_id: my_ble
  service_uuid: "F000C0C0-0451-4000-B000-000000000000"
  characteristic_uuid: "F000C0C1-0451-4000-B000-000000000000"
  notify_characteristic_uuid: "F000C0C2-0451-4000-B000-000000000000"
  address: ${lora_address}
  frequency: 868MHz
  # SF7 / 125 kHz keeps the air time short (about 130 ms for a 30-byte frame), which lowers the
  # collision probability when both nodes transmit.
  spreading_factor: 7
  signal_bandwidth: 125kHz
  coding_rate: 1
  network_id: 10
  tx_power: 20

packet_transport:
  platform: rylr999_ble
  rylr999_ble_id: my_rylr999
  update_interval: 10s       # whole set re-sent every 10 s, and immediately when a value changes
  encryption: !secret encryption_key
  sensors:
    - itemp                  # short ids keep the frame small
  binary_sensors:
    - relay_state
  providers:
    - name: ${compagnon}
      encryption: !secret encryption_key

switch:
  - platform: gpio
    name: ${name}_relay
    id: relay
    pin: ${relay_pin}
    restore_mode: RESTORE_DEFAULT_OFF
    # Mirror the switch into the binary sensor that packet_transport sends.
    on_turn_on:
      - binary_sensor.template.publish:
          id: relay_state
          state: true
    on_turn_off:
      - binary_sensor.template.publish:
          id: relay_state
          state: false

  # Physical output that follows the other node's relay (kept internal: only the template switch uses it).
  - platform: gpio
    id: mirror_out
    pin: ${mirror_pin}
    internal: true

  # Template switch driven by the binary sensor received through packet_transport.
  # turn_on / turn_off are called by the binary sensor's on_state below (or manually from the UI).
  - platform: template
    name: ${name}_mirror
    id: mirror
    optimistic: true
    restore_mode: ALWAYS_OFF
    turn_on_action:
      - switch.turn_on: mirror_out
    turn_off_action:
      - switch.turn_off: mirror_out

binary_sensor:
  # Local mirror of the relay, shared with the other node.
  - platform: template
    id: relay_state
    internal: true

  # Relay state of the other node.
  - platform: packet_transport
    provider: ${compagnon}
    remote_id: relay_state
    name: ${name}_remote_relay_state
    internal: false
    # Each time a frame updates the remote relay state, apply it to the local template switch.
    on_state:
      - if:
          condition:
            lambda: 'return x;'
          then:
            - switch.turn_on: mirror
          else:
            - switch.turn_off: mirror

  - platform: rylr999_ble
    lora_ready:
      name: ${name}_lora_ready

sensor:
  - platform: internal_temperature
    id: itemp
    internal: true
    update_interval: 5s

  # Internal temperature of the other node.
  - platform: packet_transport
    provider: ${compagnon}
    remote_id: itemp
    name: ${name}_remote_temperature
    internal: false
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement
    accuracy_decimals: 1

  - platform: rylr999_ble
    rssi:
      name: ${name}_rssi
    snr:
      name: ${name}_snr
    last_error:
      name: ${name}_last_error

number:
  - platform: rylr999_ble
    tx_power:
      name: ${name}_tx_power

button:
  - platform: rylr999_ble
    reset:
      name: ${name}_reset
```

**Node B** (`rylr999_ble_example_b.yaml`) — everything is identical except the substitutions:

```yaml
substitutions:
  name: rylr999_ble_b
  compagnon: rylr999_ble_a
  ble_mac: "90:39:5E:3F:62:46"      # BLE MAC address of the RYLR999 wired to this node
  lora_address: "2"
  relay_pin: GPIO4       # local relay, its state is sent to the other node
  mirror_pin: GPIO5      # output that follows the relay of the other node
```

Notes:

- The ids (`itemp`, `relay_state`) travel as text in every frame: keep them short. This pair costs about
  30 bytes per frame, i.e. roughly 130 ms on air at SF7 / 125 kHz.
- A change of the relay is sent **immediately**, the whole set is re-sent every `update_interval`. A frame
  that is lost is therefore repaired by the next periodic one (at most `update_interval` later).
- The mirror follows the last state received. After a reboot it starts `OFF` (`restore_mode: ALWAYS_OFF`)
  until the first frame arrives, and it keeps its last state if the radio link is lost: add a timeout
  (for example a `binary_sensor` on `lora_ready` or a `script` with a delay) if the output must fall back
  to a safe state.
- `on_state` fires on every update of the remote binary sensor, so the template switch is set each time;
  this is harmless, `switch.turn_on` on a switch that is already on does nothing.
- On the receiving side `internal: false` is mandatory when an entity has a `name:`; use `internal: true`
  with an `id:` instead for a value that is only used by local automations.
- `rylr999_ble_1.yaml` / `rylr999_ble_2.yaml` are a second, simpler pair (internal temperature one way, a
  BLE thermometer the other way).

---

## LoRa RF parameter guide

### Spreading factor vs. bandwidth

The guide lists these ranges for `AT+PARAMETER`:

| Bandwidth | Spreading factors |
|---|---|
| 125 kHz | 7 – 9 |
| 250 kHz | 7 – 10 |
| 500 kHz | 7 – 11 |

A configuration outside the range of its bandwidth gives a build-time warning (the module may then
refuse `AT+PARAMETER`; the refusal is logged as `Configuration step 3 rejected` and the other
settings are still applied).

### Air time (coding rate 4/5, preamble 12, explicit header, CRC on)

Computed by the component (Semtech formula, +8 bytes added by the module). `air_time: true`
uses this value as the spacing between two transmissions.

| Settings | 10 B | 30 B | 60 B | 120 B |
|---|---|---|---|---|
| SF7 / 125 kHz | 71 ms | 128 ms | 220 ms | 394 ms |
| SF9 / 125 kHz | 243 ms | 428 ms | 694 ms | 1247 ms |
| SF7 / 500 kHz | 18 ms | 32 ms | 55 ms | 99 ms |

(Sizes are binary bytes of the packet; they travel hex-encoded.)

### Recommendations

- The module is **half-duplex and has no listen-before-talk**: while it transmits it cannot receive,
  and two modules transmitting at the same time lose both frames (the receiver then reports
  `+ERR=12` / `+ERR=19`). Two nodes that both send a frame every second collide often.
- Prefer **SF7 / 125 kHz / CR 1** unless you need the range: it is about 3–4 times shorter on air than
  SF9 and drastically lowers the collision probability.
- Share few values, keep ids short, and use an `update_interval` of several seconds.
- Mind the regional duty-cycle rules (1 % on most of the 868 MHz sub-bands in Europe). The component
  does not enforce them.

---

## Behaviour and reliability

- **Never blocks the main loop.** The radio is configured step by step from `loop()`
  (`AT`, `ADDRESS`, `BAND`, `PARAMETER`, `NETWORKID`, `CRFOP`); each step waits for its own `+OK`
  and is repeated if the module stays silent, never assumed done. A setting the module explicitly
  refuses (`+ERR`) is logged and does not block the others.
- **BLE-aware writes.** A command is written in chunks of `MTU - 3` bytes (20 bytes until the MTU
  is known); at most a few chunks are written per loop pass. If the stack refuses a write for more than
  2 s the command is abandoned and the normal recovery applies.
- **TX queue.** `packet_transport` frames are stored in a fixed queue of
  **4 packets**, without heap allocation. Packets are sent one at a time and the next one starts only
  when the previous one has been acknowledged **and** its air time has elapsed. When the queue is full
  or the radio is not ready, the packet is refused with a warning.
- **`+ERR=17`** ("last TX not completed") makes the same packet wait and retry up to 3 times; any other
  error drops that packet and is reported in the log and in `last_error`.
- **Reception while a command is pending.** `+RCV` lines are parsed independently from the command in
  flight, so a packet arriving between an `AT+SEND` and its `+OK` is neither lost nor mistaken for
  the answer. `+ERR=12` and `+ERR=19` are reception errors and are not treated as the answer of the
  command in flight.
- **Bounded memory.** The receive line buffer is fixed (300 bytes); a longer line is discarded as a whole.
- **No permanent failure.** If the module does not answer, `AT` is retried every second and then every
  10 s, the ESPHome status shows a warning, and the configuration resumes from the start as soon as it
  answers. After 3 consecutive missing answers the module is re-initialised.
- **Module restarts.** On an unexpected `+READY` (brown-out, supply dip) the whole configuration is
  re-sent.
- **BLE link loss.** When the connection drops the queue is cleared and `lora_ready` goes `OFF`; the
  configuration is redone as soon as the link and the notifications are re-established.
- **Health check.** After 60 s without traffic an `AT` is sent to check that the module is alive.

### Error codes

| `+ERR=` | Meaning | Handling (if it answers an `AT+SEND` the packet is dropped; otherwise it is logged) |
|---|---|---|
| 1 | missing CR/LF | |
| 2 | command does not start with `AT` | |
| 4 | unknown command | |
| 5 | data length mismatch | |
| 10 | TX timeout | |
| 12 | CRC error (reception) | informative, not an answer |
| 13 | TX data exceeds 240 bytes | |
| 14 | flash write failed | |
| 15 | unknown failure | |
| 17 | last TX not completed | packet retried (3×) |
| 18 | preamble value not allowed | |
| 19 | RX header error | informative, not an answer |
| 20 | smart receiving mode time not allowed | |

---

## Troubleshooting

| Symptom | What to check |
|---|---|
| Never becomes ready, "Write characteristic not found" | Service / characteristic UUIDs; scan the module with a BLE app. |
| "does not support NOTIFY" | `notify_characteristic_uuid` must point to the characteristic with the notify property. |
| `lora_ready` flaps | BLE link drops: distance, other BLE traffic, WiFi coexistence; see the BLE connection logs. |
| `+ERR=17` in `last_error` | Frames are produced faster than they can be transmitted: lower the SF, share fewer values or raise `update_interval`. |
| `+ERR=12` / `+ERR=19` often | Collisions or weak signal: see the RSSI/SNR sensors and the [recommendations](#recommendations). |
| "TX queue full" | More than 4 frames were produced before the radio could send them. |

For maximum detail set `logger: level: VERBOSE` (a component cannot log more verbosely than the global
logger level); every AT command and every received line is then printed.

---

## Known limitations

- **BLE only.** The ESP32 must stay connected to the module; there is one BLE client per module.
- **Payload size.** 120 binary bytes at most (240 hex characters). Larger payloads are refused.
- **No acknowledgement.** `+OK` only means the module accepted the frame. Add an application-level
  acknowledgement if you need guaranteed delivery.
- **No duty-cycle limiter**: compliance with the regional duty cycle is up to you.
- **Flash wear.** `AT+ADDRESS`, `AT+PARAMETER`, `AT+BAND`, `AT+NETWORKID` and `AT+CRFOP` are stored in
  the module's flash by the module itself, and the configuration is sent again at every connection.
  This is harmless for a normal use but avoid reconnecting in a loop.
- **No LoRa-level encryption** (`AT+CPIN`) is exposed; use the `encryption` option of `packet_transport`.

---

## Credits

- Component author: [@SeByDocKy](https://github.com/SeByDocKy)
- RYLR999 documentation: [REYAX Technology](https://reyax.com/products/rylr999) (the AT command guide is included in this folder)
- ESPHome external components documentation: [esphome.io](https://esphome.io/components/external_components.html)

---

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
