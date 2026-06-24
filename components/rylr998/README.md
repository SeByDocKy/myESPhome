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
- [Automation](#automation)
  - [Action — rylr998.send_packet](#action--rylr998send_packet)
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
- [Known limitations](#known-limitations)

---

## Overview

The RYLR998 / RYLR498 are compact LoRa modules built around the Semtech SX1276 chip, controlled via a simple UART AT command interface. This component translates that AT protocol into native ESPHome entities:

| Feature | Details |
|---|---|
| Communication | UART, 115 200 baud, 8N1 |
| Frequency bands | 868 MHz (RYLR498) / 915 MHz (RYLR998) — configurable |
| Max payload | 240 bytes (raw ASCII per AT protocol) |
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
  - source: "github://SeByDocKy/myESPhome/"
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

rylr998:
  id: lora                        # required — used by automations and sub-components
  uart_id: lora_uart              # required — must point to the uart block above
  address: 1                      # optional, default: 0     (0–65535)
  frequency: 868500000            # optional, default: 915000000 Hz
  spreading_factor: 9             # optional, default: 9     (5–11)
  signal_bandwidth: 125kHz        # optional, default: 125kHz (125kHz / 250kHz / 500kHz)
  coding_rate: 1                  # optional, default: 1     (1–4)
  preamble_length: 12             # optional, default: 12    (4–24, only 4–24 when network_id=18)
  network_id: 18                  # optional, default: 18    (3–15 or 18)
  tx_power: 22                    # optional, default: 22    (0–22 dBm)
  air_time: false                 # optional, default: false — enables TX rate limiting
```

#### Parameter details

| Key | Type | Default | Range | Description |
|---|---|---|---|---|
| `address` | int | `0` | 0 – 65535 | Node address. `0` also acts as broadcast destination. |
| `frequency` | Hz | `915000000` | — | RF centre frequency in Hz. Must match all nodes. |
| `spreading_factor` | int | `9` | 5 – 11 | Higher SF → better sensitivity, longer air time. See [RF guide](#lora-rf-parameter-guide). |
| `signal_bandwidth` | Hz | `125kHz` | 125 / 250 / 500 kHz | Smaller bandwidth → better sensitivity, longer air time. |
| `coding_rate` | int | `1` | 1 – 4 | `1` = 4/5 (fastest). `4` = 4/8 (most robust). |
| `preamble_length` | int | `12` | 4 – 24 | Larger preamble → fewer missed packets. Values 4–24 only when `network_id: 18`. Other network IDs must use `12`. |
| `network_id` | int | `18` | 3–15, 18 | Group isolation — only nodes with the same ID can exchange packets. |
| `tx_power` | int | `22` | 0 – 22 | RF output power in dBm. CE certification requires ≤ 14. |
| `air_time` | bool | `false` | — | When `true`, the component computes LoRa air time on boot and enforces a TX rate limit to prevent channel flooding. |

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

Exposes TX power as an adjustable slider entity in Home Assistant (0 – 22 dBm, step 1).
Writes `AT+CRFOP=<value>` to the module when the slider is moved.

```yaml
number:
  - platform: rylr998
    rylr998_id: lora
    tx_power:
      name: "LoRa TX power"
```

> **Warning:** Changing TX power at runtime is a blocking AT command (up to 1 s). Avoid triggering it at high frequency from an automation if other UART-based components (Modbus, RS-232…) are active on the same device.

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
| `data` | list of uint8 or lambda | yes | Payload bytes. Maximum 120 bytes (recommended) — see [Known limitations](#known-limitations). |

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

> **Known limitation:** Only one `on_packet` trigger is active per component instance. If two automations are declared, only the last one fires. Use a single `on_packet` block with multiple actions, or use `add_on_packet_callback()` in C++ custom code.

---

## YAML examples

### Example 1 — Minimal transmitter

Sends a fixed heartbeat packet every 30 seconds to all nodes on the network.

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
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
  - source: "github://SeByDocKy/myESPhome/"
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
  - source: "github://SeByDocKy/myESPhome/"
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
  air_time: true                 # enables automatic TX rate limiting
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
  - source: "github://SeByDocKy/myESPhome/"
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
  - source: "github://SeByDocKy/myESPhome/"
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
  - source: "github://SeByDocKy/myESPhome/"
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
    spreading_factor: 11         # long range, slow air time
    signal_bandwidth: 125kHz
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

| Use case | `spreading_factor` | `signal_bandwidth` | `coding_rate` | `preamble_length` | Air time (16 B payload) |
|---|---|---|---|---|---|
| Short range, high throughput | `7` | `500kHz` | `1` | `12` | ~5 ms |
| General purpose (**default**) | `9` | `125kHz` | `1` | `12` | ~370 ms |
| Long range, robust | `11` | `125kHz` | `4` | `12` | ~3.5 s |
| Max range (outdoor, static) | `11` | `125kHz` | `4` | `24` | ~4.5 s |

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

With `air_time: true`, the component calculates LoRa air time at boot and enforces a minimum
interval between transmissions. You should still size your `interval:` accordingly:

| `spreading_factor` | `signal_bandwidth` | Payload | Air time | Min interval @ 1 % |
|---|---|---|---|---|
| 7 | 500 kHz | 16 B | ~5 ms | 0.5 s |
| 9 | 125 kHz | 16 B | ~370 ms | 37 s |
| 9 | 125 kHz | 64 B | ~660 ms | 66 s |
| 11 | 125 kHz | 16 B | ~3 500 ms | 350 s |

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
| `13` | Data exceeds 240 bytes | Payload too large — reduce to ≤ 120 bytes when hex-encoding |
| `14` | Flash write failed | Module hardware issue |
| `15` | Unknown failure | Try `AT+NOIDEA` diagnostic command |
| `17` | Last TX not completed | Previous send still in progress — rate limiter will retry |
| `18` | Preamble not allowed | `preamble_length` out of range for the configured `network_id` |
| `19` | RX header error | Corrupted LoRa header — usually RF environment issue |
| `20` | Smart mode time invalid | `AT+MODE=2` timing parameter out of range |

`+ERR=17` ("last TX not completed") is the most common transient error at high send rates.
The component handles it gracefully — it logs a `DEBUG` message and the rate limiter will
schedule a retry. No action is required from user code.

---

## Known limitations

**Single `on_packet` trigger per instance**
Only one `on_packet` automation is active. If two `on_packet` blocks are declared in YAML,
only the last one fires. Use a single block with multiple sequential actions as a workaround.

**Payload size and hex encoding**
The component internally hex-encodes binary payloads before sending them over AT+SEND.
One binary byte becomes two ASCII hex characters. The AT protocol documents a 240-character
limit for the `<Data>` field. Depending on firmware version, this may mean:
- **Firmware A (strict ASCII limit):** maximum binary payload = **120 bytes**. Payloads of 121–240 bytes will return `+ERR=13`.
- **Firmware B (binary limit):** maximum binary payload = **240 bytes**.

To determine which behaviour your module exhibits, send a test packet of exactly 121 bytes
and check the logs for `+ERR=13`. When in doubt, keep payloads ≤ 120 bytes.

**`apply_tx_power()` / TX power slider is blocking**
Changing TX power at runtime sends an AT command synchronously (up to 1 s wait). All other
ESPHome components on the device are stalled during this time. Avoid changing TX power from
a high-frequency automation when Modbus, RS-232 or other UART components are active.

**No encryption**
The module supports an optional AES-style password (`AT+CPIN`) but this component does not
currently expose it as a configuration key. All packets are sent in plaintext.

---

## Credits

- Component author: [@SeByDocKy](https://github.com/SeByDocKy)
- RYLR998 datasheet: [REYAX Technology](https://reyax.com/products/rylr998)
- ESPHome external components documentation: [esphome.io](https://esphome.io/components/external_components.html)
