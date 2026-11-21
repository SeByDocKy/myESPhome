# RYLR998 — Native ESPHome LoRa Component

[![ESPHome](https://img.shields.io/badge/ESPHome-native%20component-blue)](https://esphome.io)
[![LoRa](https://img.shields.io/badge/LoRa-868%20%2F%20915%20MHz-green)](https://reyax.com/products/rylr998)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Native ESPHome external component for the **REYAX RYLR998** and **RYLR498** LoRa UART modules.
Provides full AT command management, bidirectional packet exchange, RSSI/SNR diagnostics, and runtime TX power control — all without any external Arduino library.

---

## Table of Contents

- [Overview](#overview)
- [Hardware](#hardware)
- [Installation](#installation)
- [Configuration reference](#configuration-reference)
  - [Main component](#main-component)
  - [Sensor sub-component](#sensor-sub-component)
  - [Number sub-component](#number-sub-component)
  - [Button sub-component](#button-sub-component)
  - [Packet transport sub-component](#packet-transport-sub-component)
- [Automation](#automation)
  - [Action — rylr998.send_packet](#action--rylr998send_packet)
  - [Action — rylr998.reset](#action--rylr998reset)
  - [Trigger — on_packet](#trigger--on_packet)
- [YAML examples](#yaml-examples)
  - [Example 1 — Minimal transmitter](#example-1--minimal-transmitter)
  - [Example 2 — Receiver with diagnostics](#example-2--receiver-with-diagnostics)
  - [Example 3 — Full bidirectional node](#example-3--full-bidirectional-node)
  - [Example 4 — Energy telemetry (float payload)](#example-4--energy-telemetry-float-payload)
  - [Example 5 — Multi-node broadcast](#example-5--multi-node-broadcast)
- [LoRa RF parameter guide](#lora-rf-parameter-guide)
- [Duty cycle and regulatory notes](#duty-cycle-and-regulatory-notes)
- [Error codes reference](#error-codes-reference)
- [Behaviour and reliability](#behaviour-and-reliability)
- [Known limitations](#known-limitations)

---

## Overview

The RYLR998 is a compact LoRa module built around a Semtech SX1262 transceiver (the RYLR498 shares the same AT command set), controlled via a simple UART AT command interface. This component translates that AT protocol into native ESPHome entities:

| Feature | Details |
|---|---|
| Communication | UART, 115 200 baud, 8N1 |
| Frequency bands | 868 MHz (RYLR498) / 915 MHz (RYLR998) — configurable |
| Max payload | 240 ASCII characters per AT protocol, i.e. **120 binary bytes** (the component hex-encodes binary data) |
| Addressing | 0 – 65 535 per node |
| Network isolation | NETWORKID (3–15 or 18) |
| TX power range | 0 – 22 dBm (1 dBm steps) |
| CE certification limit | ≤ 14 dBm (`tx_power: 14`) |
| ESPHome targets | ESP32, ESP32-S3, ESP32-P4 |

---

## Hardware

### Wiring

```
RYLR998          ESP32 (example)
─────────        ──────────────
VCC  ──────────► 3.3 V
GND  ──────────► GND
TXD  ──────────► GPIO 16  (UART RX)
RXD  ──────────► GPIO 17  (UART TX)
```

> **Note:** The module operates at 3.3 V logic. Do **not** connect directly to a 5 V UART without a level shifter.

### Module pinout (top view)

```
 ┌──────────────────────┐
 │  ANT               │
 │  GND  VCC  TXD  RXD │
 └──────────────────────┘
```

---

## Installation

Add the following block to your ESPHome YAML — no local file copy is needed:

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome@main"
    components: [rylr998]
    refresh: 10s
```

ESPHome will pull the component directly from GitHub at build time and cache it for `refresh` seconds.

---

## Configuration reference

### Main component

```yaml
uart:
  id: lora_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200
  rx_buffer_size: 1024            # recommended — see note below

rylr998:
  id: lora                        # required — used by automations and sub-components
  uart_id: lora_uart              # required — must point to the uart block above
  variant: rylr998                # optional, default: rylr998 (rylr998 / rylr999 — sets the max tx_power)
  address: 1                      # optional, default: 0     (0–65535)
  frequency: 868500000            # optional, default: 915000000 Hz
  spreading_factor: 9             # optional, default: 9     (5–11)
  signal_bandwidth: 125kHz        # optional, default: 125kHz (125kHz / 250kHz / 500kHz)
  coding_rate: 1                  # optional, default: 1     (1–4)
  preamble_length: 12             # optional, default: 12    (4–24, only 4–24 when network_id=18)
  network_id: 18                  # optional, default: 18    (3–15 or 18)
  tx_power: 22                    # optional, default: 22    (0–22 dBm, 0–30 on rylr999)
  air_time: true                  # optional, default: true  — TX guard time computed from the size of each packet (false = fixed 600 ms)
```

> **Why `rx_buffer_size: 1024`?** (a build-time warning is printed when it is below 512) The ESPHome default UART receive buffer is 256 bytes. At
> 115 200 baud it fills in about 22 ms, and a long `+RCV=` line (up to ~270 bytes) does not even
> fit in it. If another component blocks the main loop for more than ~22 ms, incoming bytes are
> silently dropped and the packet is lost. 1024 bytes gives about 89 ms of margin, which matters
> when Modbus, RS-232 or other components share the device.

#### Parameter details

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `variant` | string | `rylr998` | `rylr998`, `rylr999` | Selects the maximum `tx_power` (22 dBm / 30 dBm). The AT command set is identical. |
| `address` | int | `0` | 0 – 65535 | Node address. `0` also acts as broadcast destination. |
| `frequency` | Hz | `915000000` | — | RF centre frequency in Hz. Must match all nodes. |
| `spreading_factor` | int | `9` | 5 – 11 | Higher SF → better sensitivity, longer air time. See [RF guide](#lora-rf-parameter-guide). |
| `signal_bandwidth` | Hz | `125kHz` | 125 / 250 / 500 kHz | Smaller bandwidth → better sensitivity, longer air time. |
| `coding_rate` | int | `1` | 1 – 4 | `1` = 4/5 (fastest). `4` = 4/8 (most robust). |
| `preamble_length` | int | `12` | 4 – 24 | Larger preamble → fewer missed packets. Values other than `12` are accepted only when `network_id: 18` (otherwise the module answers `+ERR=18`); the configuration is rejected at validation time. |
| `network_id` | int | `18` | 3–15, 18 | Group isolation — only nodes with the same ID can exchange packets. |
| `tx_power` | int | `22` | 0 – 22 | RF output power in dBm. CE certification requires ≤ 14. |
| `air_time` | bool | `true` | — | Selects the **TX guard time**: how long the component considers the radio busy after an `AT+SEND`, during which the next queued packet waits. `true` (default): the air time of **each packet** is computed from its real size and the configured SF / bandwidth / coding rate / preamble (e.g. 305 ms for 16 bytes at SF9 / 125 kHz). `false`: fixed 600 ms, which is too short for large packets at slow settings. It is **not** a duty-cycle limiter — see [Duty cycle](#duty-cycle-and-regulatory-notes). |

---

### Sensor sub-component

Adds three optional diagnostic sensors to Home Assistant.

```yaml
sensor:
  - platform: rylr998
    rylr998_id: lora              # required — must match the main component id
    rssi:
      name: "LoRa RSSI"          # Received Signal Strength in dBm
    snr:
      name: "LoRa SNR"           # Signal-to-Noise Ratio in dB
    last_error:
      name: "LoRa last error"    # Last +ERR=N code (0 = no error). See error table below.
```

All three sensors are optional and independent. Use only the ones you need.

---

### Number sub-component

Exposes TX power as an adjustable slider entity in Home Assistant (0 – 22 dBm, step 1; 0 – 30 on `rylr999`).
Moving the slider queues an `AT+CRFOP=<value>` command that is sent as soon as the module is idle.
The call never blocks; the slider shows the value actually applied (clamped to the variant maximum).

```yaml
number:
  - platform: rylr998
    rylr998_id: lora
    tx_power:
      name: "LoRa TX power"
      restore_value: false       # optional, default: false
```

| Key | Default | Description |
|---|---|---|
| `restore_value` | `false` | `true`: the last value set from the slider is stored in flash and restored at boot, overriding `tx_power` from the YAML. `false`: the YAML value is always used at boot. |

> **Upgrading:** earlier versions always restored the saved value and stored it under a different key. The old saved value is ignored; set `restore_value: true` if you want persistence.

---

### Button sub-component

Adds a **Reset** button that restarts the radio module (`AT+RESET`). The component then waits for
the module's `+READY` and re-applies the whole configuration, exactly as after an unexpected
restart. Nothing blocks the main loop. `ADDRESS` and `NETWORKID` are stored in the module's flash
and are not affected.

```yaml
button:
  - platform: rylr998
    rylr998_id: lora
    reset:
      name: "LoRa reset"
```

Packets already waiting in the TX queue are kept and sent once the module is configured again.
The same operation is available from automations with the `rylr998.reset` action:

```yaml
interval:
  - interval: 1h
    then:
      - rylr998.reset: lora
```

---

### Packet transport sub-component

Shares ESPHome **sensors** and **binary sensors** between two devices over LoRa, using ESPHome's
generic [`packet_transport`](https://esphome.io/components/packet_transport/) framework (the same
mechanism as the `udp` and `espnow` transports). The values are packed, optionally encrypted,
and sent as ordinary LoRa packets. Both entity types are handled by the framework: the
`rylr998` platform only carries the frames.

**Sending side** — list the local entities to publish:

```yaml
packet_transport:
  platform: rylr998
  rylr998_id: lora
  update_interval: 2500ms
  encryption: "loralora"          # optional
  sensors:
    - a_temperature               # ids of local sensors to publish
  binary_sensors:
    - a_relay_state               # ids of local binary sensors to publish
  providers:
    - name: rylr998b              # name of the remote device
      encryption: "loralora"
```

**Receiving side** — create one entity per remote value:

```yaml
sensor:
  - platform: packet_transport
    provider: rylr998b            # name declared in providers:
    remote_id: b_temperature      # id of the sensor on the remote device
    name: ${name}_temperature_received

binary_sensor:
  - platform: packet_transport
    provider: rylr998b
    remote_id: b_relay_state      # id of the binary sensor on the remote device
    name: ${name}_relay_state_received
```

A received entity that is only used by local automations can be hidden from Home Assistant with
`internal: true` (and an `id:`) instead of a `name:`.

> A packet-transport frame must fit in **120 bytes** (the component reports this as its maximum
> packet size). Each shared value is sent with its **id as text**: a sensor costs about
> `6 + len(id)` bytes and a binary sensor `3 + len(id)` bytes, plus a small header (and some
> overhead when `encryption` is set). Short ids therefore keep the frame small. When the values do
> not fit, the framework splits them into several packets sent back to back; the TX queue holds
> **4 packets**, so share few enough values to stay within that, and keep `update_interval` above
> the total air time (see [Duty cycle](#duty-cycle-and-regulatory-notes)).
> `rylr998a.yaml` / `rylr998b.yaml` in this folder are a complete working pair.

---

## Automation

### Action — `rylr998.send_packet`

Sends a binary payload to a destination address.

```yaml
on_...:
  - rylr998.send_packet:
      id: lora
      destination: 2              # 0 = broadcast to all addresses
      data: [0x01, 0x02, 0x03]   # static byte list
```

The `data` field also accepts a **lambda** returning `std::vector<uint8_t>`:

```yaml
on_...:
  - rylr998.send_packet:
      id: lora
      destination: 0
      data: !lambda |-
        std::vector<uint8_t> buf;
        float v = id(battery_voltage).state;
        uint8_t *p = reinterpret_cast<uint8_t *>(&v);
        buf.insert(buf.end(), p, p + 4);
        return buf;
```

#### Parameters

| Key | Type | Required | Description |
|---|---|---|---|
| `id` | component ID | yes | The `rylr998` component to use. |
| `destination` | int (0–65535) | yes | Target address. `0` sends to all nodes on the same network. |
| `data` | list of uint8 or lambda | yes | Payload bytes, 1 – 120. A larger or empty payload is refused with a warning. See [Behaviour and reliability](#behaviour-and-reliability). |

---

### Action — `rylr998.reset`

Restarts the radio module (same as the [Reset button](#button-sub-component)). Non-blocking.

```yaml
on_...:
  then:
    - rylr998.reset: lora
```

---

### Trigger — `on_packet`

Fires every time a valid `+RCV=` frame is received. Provides three variables:

| Variable | Type | Description |
|---|---|---|
| `data` | `std::vector<uint8_t>` | Decoded payload bytes |
| `rssi` | `float` | Received signal strength (dBm) |
| `snr` | `float` | Signal-to-noise ratio (dB) |

```yaml
rylr998:
  id: lora
  # ...
  on_packet:
    then:
      - logger.log:
          format: "Packet received: %d bytes, RSSI %.0f dBm, SNR %.1f dB"
          args: ["data.size()", rssi, snr]
```

> `on_packet` can be declared several times on the same component: every automation fires for every packet. Payloads sent by this component are hex-encoded and decoded automatically; a payload from another firmware that is not valid hex is delivered as raw ASCII bytes.

---

## YAML examples

### Example 1 — Minimal transmitter

Sends a fixed heartbeat packet every 30 seconds to all nodes on the network.

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome@main"
    components: [rylr998]
    refresh: 10s

esphome:
  name: lora-tx-node

esp32:
  board: esp32dev

logger:

uart:
  id: lora_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: lora_uart
  address: 1
  frequency: 868500000
  spreading_factor: 9
  signal_bandwidth: 125kHz
  coding_rate: 1
  preamble_length: 12
  network_id: 18
  tx_power: 14                   # ≤ 14 dBm for CE compliance

interval:
  - interval: 30s
    then:
      - rylr998.send_packet:
          id: lora
          destination: 0         # broadcast
          data: [0xDE, 0xAD, 0xBE, 0xEF]
```

---

### Example 2 — Receiver with diagnostics

Logs every incoming packet and exposes RSSI, SNR and error code in Home Assistant.

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome@main"
    components: [rylr998]
    refresh: 10s

esphome:
  name: lora-rx-node

esp32:
  board: esp32dev

logger:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:

uart:
  id: lora_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: lora_uart
  address: 2
  frequency: 868500000
  network_id: 18
  tx_power: 14
  on_packet:
    then:
      - logger.log:
          format: "RX %d bytes | RSSI %.0f dBm | SNR %.1f dB"
          args: ["data.size()", rssi, snr]

sensor:
  - platform: rylr998
    rylr998_id: lora
    rssi:
      name: "LoRa RSSI"
    snr:
      name: "LoRa SNR"
    last_error:
      name: "LoRa last error"
```

---

### Example 3 — Full bidirectional node

A node that both sends and receives, with runtime TX power control from Home Assistant.

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome@main"
    components: [rylr998]
    refresh: 10s

esphome:
  name: lora-node-3

esp32:
  board: esp32dev

logger:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:

uart:
  id: lora_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: lora_uart
  address: 3
  frequency: 868500000
  spreading_factor: 9
  signal_bandwidth: 125kHz
  coding_rate: 1
  preamble_length: 12
  network_id: 18
  tx_power: 14
  air_time: true                 # guard time computed from the RF settings (not a duty-cycle limiter)
  on_packet:
    then:
      - logger.log:
          format: "ACK received from remote: RSSI %.0f dBm"
          args: [rssi]

sensor:
  - platform: rylr998
    rylr998_id: lora
    rssi:
      name: "LoRa RSSI"
    snr:
      name: "LoRa SNR"
    last_error:
      name: "LoRa last error"

number:
  - platform: rylr998
    rylr998_id: lora
    tx_power:
      name: "LoRa TX power"

interval:
  - interval: 60s
    then:
      - rylr998.send_packet:
          id: lora
          destination: 0
          data: [0x01, 0x00]
```

---

### Example 4 — Energy telemetry (float payload)

A remote solar/battery monitoring node that packs four float measurements into a binary
payload and sends them every 5 minutes to a central gateway (address 1).

The gateway decodes the floats in its own `on_packet` lambda.

#### Sender (remote node, address 10)

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome@main"
    components: [rylr998]
    refresh: 10s

esphome:
  name: solar-lora-remote

esp32:
  board: esp32dev

logger:

uart:
  id: lora_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: lora_uart
  address: 10
  frequency: 868500000
  spreading_factor: 9
  signal_bandwidth: 125kHz
  network_id: 18
  tx_power: 14
  air_time: true

# Placeholder sensors — replace with your actual sensor platform
sensor:
  - platform: template
    id: pv_power
    name: "PV power"
    unit_of_measurement: W
    lambda: return 1250.0;
    update_interval: 10s

  - platform: template
    id: battery_voltage
    name: "Battery voltage"
    unit_of_measurement: V
    lambda: return 51.8;
    update_interval: 10s

  - platform: template
    id: battery_current
    name: "Battery current"
    unit_of_measurement: A
    lambda: return 12.5;
    update_interval: 10s

  - platform: template
    id: soc
    name: "State of charge"
    unit_of_measurement: "%"
    lambda: return 87.0;
    update_interval: 10s

interval:
  - interval: 300s                 # every 5 minutes
    then:
      - rylr998.send_packet:
          id: lora
          destination: 1           # gateway address
          data: !lambda |-
            // Pack four IEEE-754 floats into 16 bytes
            // Layout: [pv_power(4)] [bat_voltage(4)] [bat_current(4)] [soc(4)]
            float values[4] = {
              id(pv_power).state,
              id(battery_voltage).state,
              id(battery_current).state,
              id(soc).state,
            };
            std::vector<uint8_t> buf(16);
            memcpy(buf.data(), values, 16);
            return buf;
```

#### Receiver (gateway, address 1)

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome@main"
    components: [rylr998]
    refresh: 10s

esphome:
  name: solar-lora-gateway

esp32:
  board: esp32dev

logger:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:

uart:
  id: lora_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: lora_uart
  address: 1
  frequency: 868500000
  network_id: 18
  tx_power: 14
  on_packet:
    then:
      - lambda: |-
          if (data.size() < 16) {
            ESP_LOGW("lora_gw", "Unexpected payload size: %d", data.size());
            return;
          }
          float values[4];
          memcpy(values, data.data(), 16);
          id(remote_pv_power).publish_state(values[0]);
          id(remote_bat_voltage).publish_state(values[1]);
          id(remote_bat_current).publish_state(values[2]);
          id(remote_soc).publish_state(values[3]);
          ESP_LOGI("lora_gw",
            "Telemetry: PV=%.0fW  Vbat=%.2fV  Ibat=%.1fA  SoC=%.0f%%  RSSI=%.0fdBm",
            values[0], values[1], values[2], values[3], rssi);

sensor:
  - platform: rylr998
    rylr998_id: lora
    rssi:
      name: "Gateway RSSI"
    snr:
      name: "Gateway SNR"

  - platform: template
    id: remote_pv_power
    name: "Remote PV power"
    unit_of_measurement: W
    accuracy_decimals: 0

  - platform: template
    id: remote_bat_voltage
    name: "Remote battery voltage"
    unit_of_measurement: V
    accuracy_decimals: 2

  - platform: template
    id: remote_bat_current
    name: "Remote battery current"
    unit_of_measurement: A
    accuracy_decimals: 1

  - platform: template
    id: remote_soc
    name: "Remote state of charge"
    unit_of_measurement: "%"
    accuracy_decimals: 0
```

---

### Example 5 — Multi-node broadcast

Two independent nodes on the same device (MULTI_CONF), each on a different UART.
Node A sends on network 5; node B sends on network 10. They cannot hear each other.

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome@main"
    components: [rylr998]
    refresh: 10s

esphome:
  name: dual-lora-node

esp32:
  board: esp32-s3-devkitc-1

logger:

uart:
  - id: uart_lora_a
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 115200

  - id: uart_lora_b
    tx_pin: GPIO19
    rx_pin: GPIO18
    baud_rate: 115200

rylr998:
  - id: lora_a
    uart_id: uart_lora_a
    address: 100
    frequency: 868100000
    network_id: 5
    tx_power: 14
    spreading_factor: 7          # short range, fast air time
    signal_bandwidth: 500kHz
    on_packet:
      then:
        - logger.log:
            format: "Node A RX: %d bytes"
            args: ["data.size()"]

  - id: lora_b
    uart_id: uart_lora_b
    address: 200
    frequency: 868500000
    network_id: 10
    tx_power: 10
    spreading_factor: 10         # long range, slow air time (SF10 requires 250 kHz or 500 kHz)
    signal_bandwidth: 250kHz
    on_packet:
      then:
        - logger.log:
            format: "Node B RX: %d bytes"
            args: ["data.size()"]

interval:
  - interval: 10s
    then:
      - rylr998.send_packet:
          id: lora_a
          destination: 0
          data: [0xAA, 0x01]

  - interval: 60s
    then:
      - rylr998.send_packet:
          id: lora_b
          destination: 0
          data: [0xBB, 0x01]
```

---

## LoRa RF parameter guide

All four nodes on the same network **must use identical RF parameters** (`spreading_factor`,
`signal_bandwidth`, `coding_rate`, `preamble_length`). Mismatched parameters cause invisible
communication failures — no error is reported by the module.

### Spreading Factor vs. Signal Bandwidth constraints

| `spreading_factor` | Allowed `signal_bandwidth` |
|---|---|
| 7 – 9 | 125 kHz, 250 kHz, 500 kHz |
| 10 | 250 kHz, 500 kHz |
| 11 | 500 kHz only |

### Preset recommendations

| Use case | `spreading_factor` | `signal_bandwidth` | `coding_rate` | `preamble_length` | Air time (16 binary bytes) |
|---|---|---|---|---|---|
| Short range, high throughput | `7` | `500kHz` | `1` | `12` | ~22 ms |
| General purpose (**default**) | `9` | `125kHz` | `1` | `12` | ~304 ms |
| Long range, robust | `10` | `250kHz` | `4` | `12` | ~394 ms |
| Max range (outdoor, static) | `11` | `500kHz` | `4` | `24` | ~411 ms |

Air times are computed with the Semtech formula for the frame actually sent on air:
16 binary bytes = 32 hex characters + 8 bytes added by the module = 40 bytes (CRC on, explicit header).

> **Tip from the datasheet:** For payloads larger than 100 bytes, use `spreading_factor: 8`
> instead of `9` to keep air time reasonable.

### coding_rate values

| Value | Ratio | Description |
|---|---|---|
| `1` | 4/5 | Fastest — recommended for most applications |
| `2` | 4/6 | Moderate redundancy |
| `3` | 4/7 | — |
| `4` | 4/8 | Most robust — use only on noisy channels |

---

## Duty cycle and regulatory notes

In Europe (868 MHz band, ETSI EN 300 220), most sub-bands impose a **1 % duty cycle** limit.
This means the module can only transmit for 1 % of the time in any one-hour window.

**The component does not enforce the duty cycle.** Its TX guard time (see `air_time`) only makes
queued packets wait until the previous one has left the air; it allows a new packet as soon as the
previous one has *finished*, i.e. roughly 100 % duty cycle. Compliance is the
responsibility of your own `interval:` / automation logic. The minimum interval at 1 % is
99 × the air time of the frame actually sent:

| `spreading_factor` | `signal_bandwidth` | Payload (binary) | Air time | Min interval @ 1 % |
|---|---|---|---|---|
| 7 | 500 kHz | 16 B | ~22 ms | 2.1 s |
| 9 | 125 kHz | 16 B | ~304 ms | 30 s |
| 9 | 125 kHz | 64 B | ~734 ms | 73 s |
| 10 | 250 kHz | 16 B | ~284 ms | 28 s |

> **CE certification:** RF output power must be ≤ 14 dBm (`tx_power: 14`) to comply with CE marking.
> The default `tx_power: 22` exceeds this limit and should only be used for testing.

---

## Error codes reference

The `last_error` sensor and the logs report error codes from the module's `+ERR=N` response.

| Code | Meaning | Typical cause |
|---|---|---|
| `0` | No error | — |
| `1` | Missing CR/LF | Internal framing issue |
| `2` | Command does not start with `AT` | UART noise or misconfiguration |
| `4` | Unknown command | Firmware version mismatch |
| `5` | Length mismatch | Payload length does not match declared byte count |
| `10` | TX timeout | Channel too busy, or module not ready |
| `12` | CRC error | Radio interference during reception |
| `13` | Data exceeds 240 bytes | Payload too large — the `<Data>` field is limited to 240 characters, so ≤ 120 binary bytes when hex-encoding |
| `14` | Flash write failed | Module hardware issue |
| `15` | Unknown failure | Try `AT+NOIDEA` diagnostic command |
| `17` | Last TX not completed | Previous send still in progress — packet is dropped by the module |
| `18` | Preamble not allowed | `preamble_length` out of range for the configured `network_id` |
| `19` | RX header error | Corrupted LoRa header — usually RF environment issue |
| `20` | Smart mode time invalid | `AT+MODE=2` timing parameter out of range |

`+ERR=12` (CRC) and `+ERR=19` (header) are reported by the module for a *received* frame that was
corrupted; they are not the answer to a command and are only logged at `DEBUG` level (they still
update the `last_error` sensor).

`+ERR=17` ("last TX not completed") means the module was still transmitting. The component keeps the
packet and retries it up to 3 times with an increasing delay before giving up. With `air_time: true`
(default) the guard time follows the real size of each packet, so this normally never occurs.
Any other `+ERR` answer to an `AT+SEND` drops that packet (logged as a warning).

---

## Behaviour and reliability

- **Never blocks the main loop.** The module is configured step by step from `loop()`
  (`AT`, `ADDRESS`, `BAND`, `PARAMETER`, `NETWORKID`, `CRFOP`); `setup()` returns immediately, so Modbus,
  RS-232 and other components are never stalled.
- **TX queue.** `rylr998.send_packet` / `transmit_packet()` store the packet in a fixed queue of
  **4 packets** (no heap allocation). Packets are sent one at a time, each waiting for `+OK` and for the
  air time of the previous one. When the queue is full, or the module is not ready, the packet is
  refused with a warning (and `transmit_packet()` returns `false`).
- **Module restarts.** If the module reboots by itself (`+READY`, brown-out, supply dip) the RF settings
  that are not stored in its flash are lost; the component detects `+READY` and re-sends the whole
  configuration. After 3 consecutive missing answers it re-initialises the module as well. While the
  module does not answer, the ESPHome status shows a warning and `AT` is retried every second, then
  every 10 s.
- **Manual reset.** The `reset` button and the `rylr998.reset` action send `AT+RESET`; the configuration is re-applied when the module reports `+READY`.
- **Health check.** After 60 s without any traffic an `AT` is sent to verify that the module is alive.
- **Reception while a command is pending.** `+RCV` frames are parsed independently of the command in
  flight, so a packet arriving between an `AT+SEND` and its `+OK` is neither lost nor mistaken for the answer.
- **Malformed lines** (wrong declared length, truncated, longer than 300 bytes) are discarded with a warning.

## Known limitations

**Payload size and hex encoding**
The component internally hex-encodes binary payloads before sending them over AT+SEND.
One binary byte becomes two ASCII hex characters, and the module transmits those characters
as-is. The AT protocol limits the `<Data>` field to 240 characters, and a LoRa frame cannot
carry more than 255 bytes (240 characters + 8 bytes added by the module = 248), so the
maximum binary payload is **120 bytes**. Larger payloads are refused by the component.

**No duty-cycle limiter**
Compliance with the 1 % duty cycle of the 868 MHz band is up to your automations.

**Packets are not acknowledged**
LoRa transmission by this module is fire-and-forget: `+OK` only means the module accepted the
frame. Add an application-level acknowledgement if you need guaranteed delivery.

**No encryption**
The module supports an optional AES-style password (`AT+CPIN`) but this component does not
currently expose it as a configuration key. All packets are sent in plaintext.

---

## Credits

- Component author: [@SeByDocKy](https://github.com/SeByDocKy)
- RYLR998 datasheet: [REYAX Technology](https://reyax.com/products/rylr998)
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
