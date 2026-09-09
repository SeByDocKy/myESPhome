# `nrf24l01` — native ESPHome component for the nRF24L01(+)

Native ESPHome component for the classic **nRF24L01 / nRF24L01+** 2.4GHz
transceiver, over real hardware SPI (via ESPHome's `spi:` bus — unlike
[`cmt2300a`](../cmt2300a/README.md), this chip doesn't need bit-banging).
Register map, SPI commands, and the `begin()`/`setPALevel()`/`setDataRate()`/
`openWritingPipe()`/`startListening()` sequence are ported from the
[official nRF24/RF24 Arduino library](https://github.com/nRF24/RF24),
cross-checked against the nRF24L01+ datasheet's register map (stable and
unchanged for well over a decade).

## Installation

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [nrf24l01]
    refresh: 10s
```

## Wiring

Standard 4-wire SPI plus two extra control pins:

| nRF24L01 pin | Role |
|---|---|
| `SCK` / `MOSI` / `MISO` | Declared once under `spi:` |
| `CSN` | Chip select — declared under `cs_pin:` inside `nrf24l01:` |
| `CE` | Chip enable — separate GPIO, controls Tx/Rx activation |
| `IRQ` | Optional, not actively used (see below) |

## Basic configuration

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
  # irq_pin: GPIO2        # optional, not actively used
  channel: 76              # 0-125 -> 2400+channel MHz
  pa_level: max             # min | low | high | max
  air_data_rate: 1mbps          # 1mbps | 2mbps | 250kbps
  crc_length: 16bit         # disabled | 8bit | 16bit
  address_width: 5          # 3-5 bytes
  auto_ack: true
  retry_delay: 5             # 0-15 -> (n+1)*250us between retries
  retry_count: 15            # 0-15 automatic retransmit attempts
  payload_size: 32           # 1-32 bytes (fixed payloads)
  dynamic_payloads: false
  tx_address: "E7E7E7E7E7"   # this device's writing pipe (5 bytes, hex)
  rx_address: "E7E7E7E7E7"   # this device's reading pipe 1 (5 bytes, hex)
  on_packet_received:
    - logger.log:
        format: "Packet received (%u bytes)"
        args: ["x.size()"]
```

### Options

| Key | Required | Default | Description |
|---|---|---|---|
| `spi_id` | no (auto-detected if only one `spi:` bus exists) | — | The `spi:` bus this radio is on |
| `cs_pin` | yes | — | Chip select pin (standard ESPHome SPI device option) |
| `ce_pin` | yes | — | Chip enable pin |
| `irq_pin` | no | — | IRQ pin, declared for future use but not currently polled/attached — the component checks `FIFO_STATUS` directly on every `loop()` tick instead |
| `channel` | no | `76` | RF channel, `0`-`125` → `2400 + channel` MHz |
| `pa_level` | no | `max` | `min` (-18dBm) / `low` (-12dBm) / `high` (-6dBm) / `max` (0dBm) |
| `air_data_rate` | no | `1mbps` | `1mbps` / `2mbps` / `250kbps` |
| `crc_length` | no | `16bit` | `disabled` / `8bit` / `16bit`. `disabled` is only honored if `auto_ack: false` — the chip requires CRC whenever hardware auto-ack is active; otherwise a warning is logged and CRC-16 is kept |
| `address_width` | no | `5` | `3`-`5` bytes |
| `auto_ack` | no | `true` | Hardware auto-acknowledgment on all pipes |
| `retry_delay` | no | `5` | `0`-`15`, each step is 250µs between automatic retransmit attempts |
| `retry_count` | no | `15` | `0`-`15` automatic retransmit attempts before giving up |
| `payload_size` | no | `32` | `1`-`32` bytes, used when `dynamic_payloads: false` |
| `dynamic_payloads` | no | `false` | Enables `DPL`/`ACK_PAY`/`DYN_ACK` features for variable-length payloads |
| `tx_address` | no | `E7E7E7E7E7` | 10 hex characters (5 bytes) — this device's writing pipe / pipe-0 receive address (used for auto-ack) |
| `rx_address` | no | `E7E7E7E7E7` | 10 hex characters (5 bytes) — this device's reading pipe 1 address |
| `on_packet_received` | no | — | Automation; `x` is the received payload as `std::vector<uint8_t>` |

### `nrf24l01.send` action

```yaml
button:
  - platform: template
    name: "Send test"
    on_press:
      - nrf24l01.send:
          id: radio
          data: [0xDE, 0xAD, 0xBE, 0xEF]
```

`data` accepts a literal byte list or a template. `send_packet()` briefly
leaves listening mode, transmits, waits (blocking) for `TX_DS`/`MAX_RT` up to
100ms by default, then resumes listening automatically — a transmission at
these data rates normally completes in well under a millisecond to a few
milliseconds even with retries, so this is not comparable to the CMT2300A's
blocking-vs-async concerns.

## Two-device example

```yaml
# --- Device A ---
nrf24l01:
  id: radio
  spi_id: spi_bus
  cs_pin: GPIO5
  ce_pin: GPIO4
  tx_address: "AABBCCDDEE"   # A writes here (B must listen on this)
  rx_address: "11223344EE"   # A listens here (B must write here)
  on_packet_received:
    - logger.log:
        format: "From B: %u bytes, first=0x%02X"
        args: ["x.size()", "x[0]"]

button:
  - platform: template
    name: "Send to B"
    on_press:
      - nrf24l01.send: {id: radio, data: [0x01, 0x02]}
```

```yaml
# --- Device B (addresses swapped) ---
nrf24l01:
  id: radio
  spi_id: spi_bus
  cs_pin: GPIO5
  ce_pin: GPIO4
  tx_address: "11223344EE"   # B writes here (A listens here)
  rx_address: "AABBCCDDEE"   # B listens here (A writes here)
  on_packet_received:
    - logger.log:
        format: "From A: %u bytes, first=0x%02X"
        args: ["x.size()", "x[0]"]

button:
  - platform: template
    name: "Send to A"
    on_press:
      - nrf24l01.send: {id: radio, data: [0xAA]}
```

## Known limitations of this v1

- No multi-pipe reception (pipes 2-5) exposed in YAML — only pipe 1 is opened
  for reading. The register-level API to open other pipes exists internally
  but isn't wired up to configuration yet.
- No ACK-payload support (`W_ACK_PAYLOAD`) even with `dynamic_payloads: true`
  — only plain auto-ack (empty acknowledgment) is used.
- No `packet_transport` medium yet (unlike `cmt2300a`) — can be added the
  same way if useful.
