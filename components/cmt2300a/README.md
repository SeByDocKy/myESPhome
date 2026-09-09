# `cmt2300a` — native ESPHome / ESP-IDF component for the CMT2300A

Native ESPHome component for the **CMT2300A** sub-GHz radio chip (RFM300W
module and similar), written directly against the ESP-IDF GPIO API
(bit-banged), without going through ESPHome's `spi:` bus. This is not a
cosmetic choice: the CMT2300A does not use real SPI — the SDIO line is a
**single, bidirectional** data line (no separate MOSI/MISO), with specific
guard timings around the CS/FCS transitions that ESPHome's `spi:` abstraction
cannot reproduce correctly.

This component provides a **generic** radio layer (send/receive raw packets,
up to 32 bytes). It also serves as the low-level foundation for other
higher-level components (for example [`hms`](../hms/README.md), which
implements the Hoymiles protocol on top of it, in a dedicated mode).

## Installation

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [cmt2300a]
    refresh: 10s
```

`refresh: 10s` makes ESPHome re-check the repository on every compile
(instead of caching indefinitely) — handy during development; you can
increase it or remove it once your config is stable.

## Wiring

| CMT2300A pin | Role |
|---|---|
| `SCLK` | Clock, bit-banged |
| `SDIO` | **Single** bidirectional data line (no separate MISO) |
| `CSB` | Chip select (register access) |
| `FCSB` | FIFO chip select (Tx/Rx data access) |
| `GPIO2` | Optional — mappable to a `TX_DONE` interrupt |
| `GPIO3` | Optional — mappable to a `PKT_DONE`/`PKT_OK` interrupt |

`GPIO2`/`GPIO3` are **not required** for operation: the component polls the
relevant status registers directly on every `loop()` tick instead of relying
on hardware interrupts (the bit-bang functions are not safe to call from an
interrupt context). You can wire and declare them if you plan to reuse them
yourself in your own lambdas; otherwise you can leave them unconnected.

## Basic configuration

```yaml
cmt2300a:
  id: cmt_radio
  clk_pin: GPIO9
  sdio_pin: GPIO10
  cs_pin: GPIO11
  fcs_pin: GPIO12
  # gpio2_pin: GPIO13       # optional
  # gpio3_pin: GPIO14       # optional
  node_id: 0x00000001       # optional, hardware address filter (default 0)
  accept_any_node_id: false # optional, default false
  fifo_threshold: 32        # optional, 1-64, default 32
  on_packet_received:
    - logger.log:
        format: "Packet received (%u bytes)"
        args: ["x.size()"]
  on_tx_done:
    - logger.log: "Transmission done"
```

### Options

| Key | Required | Default | Description |
|---|---|---|---|
| `clk_pin` | yes | — | Clock pin (output) |
| `sdio_pin` | yes | — | Bidirectional data pin |
| `cs_pin` | yes | — | Register chip-select pin (output) |
| `fcs_pin` | yes | — | FIFO chip-select pin (output) |
| `gpio2_pin` | no | — | GPIO2 interrupt pin (not actively used, see above) |
| `gpio3_pin` | no | — | GPIO3 interrupt pin (same) |
| `node_id` | no | `0` | Node address for the CMT2300A's hardware address filtering (NODE_ID register) |
| `accept_any_node_id` | no | `false` | If `true`, disables address filtering — every received packet fires `on_packet_received` |
| `fifo_threshold` | no | `32` | FIFO threshold (1-64) |
| `on_packet_received` | no | — | Automation; the `x` variable is the received packet as `std::vector<uint8_t>` |
| `on_tx_done` | no | — | Automation fired when a transmission completes |

### `cmt2300a.send` action

```yaml
button:
  - platform: template
    name: "Send test"
    on_press:
      - cmt2300a.send:
          id: cmt_radio
          data: [0xDE, 0xAD, 0xBE, 0xEF]
```

`data` accepts either a literal list of bytes or a template (a lambda
returning a `std::vector<uint8_t>`), up to 32 bytes.

## `duty_cycle` sensor

Only useful when `cmt2300a:` is shared by several `hms:` instances (or any
other future `external_mode` consumer): reports the measured percentage of
time the radio was actually locked/busy, over the window since the last
publication (`update_interval`, default `60s`). This is a real measurement,
not an estimate — useful for tuning `poll_interval` on each `hms:` instance
to keep the channel from becoming saturated (which would delay `number`/
`output` power-limit commands behind ongoing telemetry exchanges).

```yaml
sensor:
  - platform: cmt2300a
    cmt2300a_id: cmt_radio
    duty_cycle:
      name: "CMT2300A Duty Cycle"
      update_interval: 60s
```

With a generic (non-`hms`) `cmt2300a:` usage, this always reads `0%` — the
lock is only used by `external_mode` consumers.

## Example 1 — generic usage (without `packet_transport`)

Two devices exchanging raw messages, filtered by `node_id`.

```yaml
# --- Device A ---
cmt2300a:
  id: cmt_radio
  clk_pin: GPIO9
  sdio_pin: GPIO10
  cs_pin: GPIO11
  fcs_pin: GPIO12
  node_id: 0x00000001
  on_packet_received:
    - logger.log:
        format: "Received from B: %u bytes, 1st byte=0x%02X"
        args: ["x.size()", "x[0]"]

button:
  - platform: template
    name: "Send to B"
    on_press:
      - cmt2300a.send:
          id: cmt_radio
          data: [0x01, 0x02, 0x03]
```

```yaml
# --- Device B (symmetric config) ---
cmt2300a:
  id: cmt_radio
  clk_pin: GPIO9
  sdio_pin: GPIO10
  cs_pin: GPIO11
  fcs_pin: GPIO12
  node_id: 0x00000002
  on_packet_received:
    - logger.log:
        format: "Received from A: %u bytes, 1st byte=0x%02X"
        args: ["x.size()", "x[0]"]

button:
  - platform: template
    name: "Send to A"
    on_press:
      - cmt2300a.send:
          id: cmt_radio
          data: [0xAA, 0xBB]
```

At this level, building/parsing the packet payload is entirely up to you in
your own lambdas — this is the lowest layer, deliberately unstructured.

## Example 2 — with `packet_transport`

The component also exposes a medium for ESPHome's official
[`packet_transport`](https://esphome.io/components/packet_transport/)
component: automatic exchange of sensor states between devices, without
having to write your own parsing.

```yaml
packet_transport:
  - platform: cmt2300a
    cmt2300a_id: cmt_radio
    update_interval: 10s
    encryption: "MySharedSecret123"
    sensors:
      - my_temperature
    binary_sensors:
      - my_door_sensor
```

On the receiving side, the generic `sensor: platform: packet_transport` and
`binary_sensor: platform: packet_transport` platforms work without knowing
anything about the underlying transport (see the full example below).

> ⚠️ **Incompatible with `hms:`** — a `cmt2300a:` instance driven by an
> `hms:` block (Hoymiles mode) cannot also serve as a generic
> `packet_transport` medium: the two protocols configure the chip in
> mutually incompatible ways. Compilation refuses this configuration with an
> explicit error if the same `cmt2300a_id` is referenced by both.

## Example 3 — full two-device demo (sensor + binary_sensor)

Two ESP32s, each with its own CMT2300A, mutually exchanging a temperature
reading and a door sensor in real time via `packet_transport`. Copy each
block into a separate YAML file.

### `device_a.yaml`

```yaml
esphome:
  name: device-a

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
ota:
  - platform: esphome
logger:

external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [cmt2300a]
    refresh: 10s

cmt2300a:
  id: cmt_radio
  clk_pin: GPIO9
  sdio_pin: GPIO10
  cs_pin: GPIO11
  fcs_pin: GPIO12

# --- What device-a SENDS to device-b, AND receives from device-b ---
sensor:
  - platform: dht
    pin: GPIO4
    temperature:
      name: "Temperature A"
      id: temperature_a
    update_interval: 30s

  - platform: packet_transport
    provider: device-b
    id: temperature_b
    name: "Temperature B (received)"

binary_sensor:
  - platform: gpio
    pin:
      number: GPIO5
      mode: INPUT_PULLUP
    name: "Door A"
    id: door_a

  - platform: packet_transport
    provider: device-b
    id: door_b
    name: "Door B (received)"

packet_transport:
  - platform: cmt2300a
    cmt2300a_id: cmt_radio
    update_interval: 10s
    encryption: "SharedSecretAB"
    sensors:
      - temperature_a
    binary_sensors:
      - door_a
```

### `device_b.yaml`

```yaml
esphome:
  name: device-b

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
ota:
  - platform: esphome
logger:

external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [cmt2300a]
    refresh: 10s

cmt2300a:
  id: cmt_radio
  clk_pin: GPIO9
  sdio_pin: GPIO10
  cs_pin: GPIO11
  fcs_pin: GPIO12

# --- What device-b SENDS to device-a, AND receives from device-a ---
sensor:
  - platform: dht
    pin: GPIO4
    temperature:
      name: "Temperature B"
      id: temperature_b
    update_interval: 30s

  - platform: packet_transport
    provider: device-a
    id: temperature_a
    name: "Temperature A (received)"

binary_sensor:
  - platform: gpio
    pin:
      number: GPIO5
      mode: INPUT_PULLUP
    name: "Door B"
    id: door_b

  - platform: packet_transport
    provider: device-a
    id: door_a
    name: "Door A (received)"

packet_transport:
  - platform: cmt2300a
    cmt2300a_id: cmt_radio
    update_interval: 10s
    encryption: "SharedSecretAB"
    sensors:
      - temperature_b
    binary_sensors:
      - door_b
```

### Key points of this demo

- **`provider:`** must match the sending device's **ESPHome name**
  (`esphome: name:`), not a radio-protocol identifier.
- **`encryption:`** must be identical on both sides — it's the only thing
  that keeps two nearby `packet_transport` pairs on the same band from
  hearing each other (the CMT2300A itself has no notion of channels in
  generic mode).
- **Each device is both a provider and a consumer**: each one declares its
  own `packet_transport:` block (sending its local sensors) AND
  `sensor:`/`binary_sensor: platform: packet_transport` platforms (receiving
  the remote sensors) — it's symmetric.
- Pins (`clk_pin`, etc.) and sensor wiring (`GPIO4`/`GPIO5`) are identical in
  both files for the example; adapt them to your actual wiring if the two
  ESP32 boards differ.
