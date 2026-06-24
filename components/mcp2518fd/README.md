# mcp2518fd — ESPHome Native CAN Bus Component

Native ESPHome external component for the **Microchip MCP2518FD** CAN FD controller,
communicating via SPI. Supports Classic CAN (2.0B) and CAN FD frames, all operating modes,
optional interrupt pins, and a **text_sensor sniffer** platform for passive bus monitoring.

> ✅ **Only the [Soldered Electronics CAN Transceiver MCP2518FD](https://soldered.com/products/can-transceiver-mcp2518-board)
> (MCP2518FD + ATA6563, non-isolated) has been successfully tested** — both in CLASSIC and
> LISTEN_ONLY modes. Cheap AliExpress clones will NOT work reliably (see warning below).

---

## Features

- **Classic CAN 2.0B and CAN FD** — selectable via `mode` and `canfd_enabled`
- **All MCP2518FD operating modes** — NORMAL, CLASSIC, LISTEN_ONLY, LOOPBACK_INT, LOOPBACK_EXT, RESTRICTED, SLEEP
- **Optional interrupt pins** — INT, INT0 (TX), INT1 (RX), all active-low
- **RX FIFO** — 32 messages deep, per-loop limit of 8 frames to prevent watchdog triggers
- **text_sensor sniffer platform** — passive bus monitor in LISTEN_ONLY mode, displays the most recently active CAN IDs
- **ESP32-S3 compatible** — hardware SPI, native API integration with Home Assistant
- **Extended and standard CAN IDs** — 29-bit and 11-bit

---

## Hardware Requirements

| Item | Detail |
|---|---|
| SoC | ESP32-S3 (recommended), ESP32 |
| Framework | `esp-idf` or `arduino` |
| Module | [Soldered Electronics MCP2518FD + ATA6563](https://soldered.com/products/can-transceiver-mcp2518-board) — **the only tested module** |
| SPI | Up to 10 MHz |

> ⚠️ **Clone chips warning** — cheap AliExpress MCP2518FD modules often contain clone chips
> (`DEVID = 0x00` instead of `0x28`). These cause **silent RX failures** and can
> **block the bus in dominant state** when a second active node is present, which may damage
> transceivers. Always use a genuine MCP2518FD module.

---

## Wiring — ESP32-S3

| Signal | GPIO | Note |
|---|---|---|
| SCK  | GPIO 12 | SPI clock |
| SDI  | GPIO 13 | MISO (MCP2518FD → ESP) |
| SDO  | GPIO 11 | MOSI (ESP → MCP2518FD) |
| CS   | GPIO 10 | Chip select (active-low) |
| INT1 | GPIO x  | Optional RX interrupt (active-low) |
| VCC  | 5V      | Power supply (onboard 5V→3.3V regulator) |
| GND  | GND     | Common ground |

---

## File Structure

```
components/mcp2518fd/
├── __init__.py          ← ESPHome schema (empty placeholder)
├── canbus.py            ← ESPHome schema + codegen
├── text_sensor.py       ← optional: sniffer platform
├── mcp2518fd.h          ← C++ declarations
├── mcp2518fd.cpp        ← C++ implementation
└── mcp2518fd_defs.h     ← Register definitions (DS20006027B)
```

---

## Configuration Reference

### `canbus` platform options

| Key | Type | Default | Description |
|---|---|---|---|
| `clock` | enum | `20MHz` | MCP2518FD oscillator frequency: `4MHz`, `10MHz`, `20MHz`, `40MHz` |
| `bit_rate` | speed | `125KBPS` | Nominal CAN bit rate |
| `mode` | enum | `NORMAL` | Operating mode (see below) |
| `canfd_enabled` | bool | `false` | Enable CAN FD frames (payload up to 64 bytes) |
| `can_data_rate` | speed | `500KBPS` | Data-phase bit rate (only when `canfd_enabled: true`) |
| `int_pin` | pin | — | General interrupt pin (active-low, optional) |
| `int0_pin` | pin | — | TX interrupt pin (active-low, optional) |
| `int1_pin` | pin | — | RX interrupt pin (active-low, optional) |

### Operating modes

| Mode | Description |
|---|---|
| `NORMAL` | Standard CAN FD mode — accepts both FD and classic frames |
| `CLASSIC` | Classic CAN 2.0B only — rejects FD frames |
| `LISTEN_ONLY` | Passive monitoring — no TX, no ACK, no error frames |
| `LOOPBACK_INT` | Internal loopback for testing |
| `LOOPBACK_EXT` | External loopback for testing |
| `RESTRICTED` | Restricted operation mode |
| `SLEEP` | Low-power sleep mode |

### `text_sensor` sniffer options

| Key | Type | Default | Description |
|---|---|---|---|
| `mcp2518fd_id` | id | required | Reference to the parent `mcp2518fd` canbus |
| `frame_displayed` | int 1–6 | `1` | Number of most-recently-active CAN IDs to display |

> The sniffer displays the **6 most recently active CAN IDs** on the bus, one per line,
> updated every 500 ms. Limited to 6 lines to stay within Home Assistant's 255-character
> state limit. Each entry shows the CAN ID and its latest 8-byte payload.

---

## Examples

### Example 1 — Classic CAN, 125 kbps, receive frames

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [mcp2518fd]
    refresh: 0s

spi:
  id: spi_bus
  clk_pin: GPIO12
  miso_pin: GPIO13
  mosi_pin: GPIO11

canbus:
  - platform: mcp2518fd
    id: can0
    spi_id: spi_bus
    cs_pin: GPIO10
    can_id: 0x0A
    bit_rate: 125kbps
    clock: 20MHz
    mode: CLASSIC
    canfd_enabled: false
    use_extended_id: true

    on_frame:
      - can_id: 0x010A0051
        use_extended_id: true
        then:
          - lambda: |-
              uint16_t grid_voltage = (x[3] << 8) | x[2];
              ESP_LOGD("can", "Grid voltage: %.1f V", grid_voltage / 10.0f);
```

---

### Example 2 — Classic CAN, 250 kbps, send + receive

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [mcp2518fd]
    refresh: 0s

spi:
  id: spi_bus
  clk_pin: GPIO12
  miso_pin: GPIO13
  mosi_pin: GPIO11

canbus:
  - platform: mcp2518fd
    id: can0
    spi_id: spi_bus
    cs_pin: GPIO10
    can_id: 0x0A
    bit_rate: 250kbps
    clock: 20MHz
    mode: CLASSIC
    canfd_enabled: false
    use_extended_id: true

    on_frame:
      - can_id: 0x18FF0001
        use_extended_id: true
        then:
          - lambda: |-
              ESP_LOGD("can", "Frame received: %d bytes", x.size());

button:
  - platform: template
    name: "Send CAN frame"
    on_press:
      - canbus.send:
          canbus_id: can0
          can_id: 0x18FF0002
          use_extended_id: true
          data: [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
```

---

### Example 3 — LISTEN_ONLY mode with sniffer text_sensor

Ideal for **passive bus monitoring** without disturbing existing nodes.
No TX, no ACK — completely transparent on the bus.
The sniffer displays the 6 most recently active CAN IDs in Home Assistant.

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [mcp2518fd]
    refresh: 0s

spi:
  id: spi_bus
  clk_pin: GPIO12
  miso_pin: GPIO13
  mosi_pin: GPIO11

canbus:
  - platform: mcp2518fd
    id: can0
    spi_id: spi_bus
    cs_pin: GPIO10
    can_id: 0x0A
    bit_rate: 125kbps
    clock: 20MHz
    mode: LISTEN_ONLY       # passive — no TX, no ACK, no error frames
    canfd_enabled: false
    use_extended_id: true

text_sensor:
  - platform: mcp2518fd
    name: "CAN Bus Sniffer"
    mcp2518fd_id: can0
    frame_displayed: 6      # 1–6, limited by HA 255-char state constraint
    icon: mdi:numeric
```

Home Assistant Lovelace — display with line breaks:

```yaml
type: markdown
content: |
  **CAN Bus Sniffer**
  ```
  {{ states('sensor.can_bus_sniffer') }}
  ```
```

---

### Example 4 — CAN FD, 500 kbps nominal / 2 Mbps data

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [mcp2518fd]
    refresh: 0s

spi:
  id: spi_bus
  clk_pin: GPIO12
  miso_pin: GPIO13
  mosi_pin: GPIO11

canbus:
  - platform: mcp2518fd
    id: can0
    spi_id: spi_bus
    cs_pin: GPIO10
    can_id: 0x01
    bit_rate: 500kbps
    clock: 20MHz
    mode: NORMAL
    canfd_enabled: true
    can_data_rate: 2000kbps
    use_extended_id: false
```

---

### Example 5 — With RX interrupt pin (INT1)

Using INT1 avoids polling `CiFIFOSTA` on every `loop()` iteration,
reducing SPI traffic when the bus is idle.

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [mcp2518fd]
    refresh: 0s

spi:
  id: spi_bus
  clk_pin: GPIO12
  miso_pin: GPIO13
  mosi_pin: GPIO11

canbus:
  - platform: mcp2518fd
    id: can0
    spi_id: spi_bus
    cs_pin: GPIO10
    can_id: 0x0A
    bit_rate: 125kbps
    clock: 20MHz
    mode: CLASSIC
    canfd_enabled: false
    use_extended_id: true
    int1_pin: GPIO9           # RX interrupt, active-low — inverted forced internally
```

---

## dump_config output

At boot, the component logs its full configuration and register state:

```
[C][mcp2518fd]: MCP2518FD:
[C][mcp2518fd]:   Init success : YES
[C][mcp2518fd]:   DEVID  (0xE14): 0x00000028 (MCP2518FD=0x28)
[C][mcp2518fd]:   OSC    (0xE00): 0x00000460
[C][mcp2518fd]:   CiCON  (live): 0x06d01f40 OPMOD=6 REQOP=6 ISOCRCEN=0
[C][mcp2518fd]:   CH1(RX) CON=0x1f000000 STA=0x00000001
[C][mcp2518fd]:   CH2(TX) CON=0x031f0280 STA=0x00000001 TXNIF=1 TXEN=1
```

> If `DEVID = 0x00000000`, the chip is a **clone** — RX will silently fail.
> Replace immediately with the [Soldered Electronics module](https://soldered.com/products/can-transceiver-mcp2518-board).

---

## Bit Timing Formula

```
BR = Fosc / (BRP+1) / (TSEG1 + TSEG2 + 3)
```

Example for **125 kbps** with a 20 MHz oscillator:
`{BRP=0, TSEG1=125, TSEG2=32, SJW=4}` → 20 MHz / 1 / 160 = 125 kbps

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `DEVID = 0x00000000` | Clone chip | Replace with [Soldered Electronics module](https://soldered.com/products/can-transceiver-mcp2518-board) |
| No RX frames, TX works | Clone chip | Check DEVID — clone chips have silent RX failure |
| Bus locked in dominant state | Clone + second active node | Remove clone module **immediately** — transceiver at risk |
| `Init success: NO` | SPI wiring issue | Check CLK/SDI/SDO/CS pins and 3.3V supply |
| `OPMOD` stays at 4 (CONFIG) | Mode transition timeout | Check oscillator frequency (`clock: 20MHz`) |
| Sniffer shows "unknown" in HA | String > 255 chars | Keep `frame_displayed` at 6 or below |
| First compile fails | IDF component first-run | Run `esphome compile` a second time |

---

## Credits

- Microchip MCP2518FD datasheet DS20006027B
- ESPHome `mcp2515` component (architecture reference)
- Developed by [@SeByDocKy](https://github.com/SeByDocKy) — [SeByDocKy/myESPhome](https://github.com/SeByDocKy/myESPhome)
