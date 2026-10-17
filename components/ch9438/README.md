# CH9438 — SPI to 8 × UART (full duplex) + 8 × GPIO for ESPHome

Native ESPHome component for the **WCH CH9438** (QFN32). The chip is driven over SPI only (there is no I²C
interface) and gives the ESP32 up to **8 independent full-duplex UARTs** and **8 GPIO**.

Every configured UART is a regular ESPHome `uart` component, so anything that accepts a `uart_id`
(`modbus`, `uart` consumers, other custom components, ...) can use it. The design follows the `weikai` component
(hub + one `UARTComponent` per channel + a software ring buffer per channel).

> **Status:** written from the CH9438 datasheet V1.1 and from the register/protocol behaviour of WCH's own Linux
> driver. It passes `esphome config` and the pin-conflict validation. **It has not been tested on real hardware
> yet** — see [Hardware validation checklist](#hardware-validation-checklist).

## Chip summary

| | |
|---|---|
| UARTs | 8, 16C550 compatible, 8 data bits, 1 or 2 stop bits, none/odd/even parity |
| Baud rate | 2930 bd … 12 Mbd (`192 MHz / divisor`, 12.4 fixed point divisor) |
| FIFOs | 2048 bytes RX, 1536 bytes TX per UART |
| SPI | up to 25 MHz, **mode 3 (CPOL=1, CPHA=1)**, MSB first |
| GPIO | 8, push-pull / open-drain outputs, floating / pull-up / pull-down inputs |
| RS485 | `TNOW` direction pin per UART (automatic DE/RE, polarity selectable) |
| Flow control | hardware RTS/CTS on UART1 … UART4 |
| Interrupt | `INT#`, active low, shared by all UARTs (optional here) |
| Supply | VCC 1.8 … 3.6 V, VIO 1.65 V … VCC |
| Clock | internal (192 MHz PLL) or external 16 MHz crystal |

## Wiring

| ESP32 | CH9438 |
|---|---|
| SPI CLK / MOSI / MISO | SCK / SDI / SDO |
| any GPIO (output) | SCS# |
| any GPIO (input, optional) | INT# (shared with the pin GPIO1 / RTS1 / TNOW1) |
| 3.3 V / GND | VCC (+ VIO) / GND |

`RST#` is not driven by the component: tie it high (or to a GPIO you control yourself).

**RS485:** the CH9438 only outputs logic-level UART signals. A transceiver (SP3485, MAX485, ...) is still needed on
each RS485 channel. The `TNOWx` pin is connected to its `DE` and `/RE` pins and the chip switches it automatically.

## Minimal example

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome@main
    components: [ch9438]
    refresh: 0s

spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

ch9438:
  id: ch9438_hub
  cs_pin: GPIO17
  irq_pin:                    # optional
    number: GPIO4
    mode: INPUT_PULLUP
  data_rate: 10MHz
  uart:
    - id: ch_uart_0
      channel: 0
      baud_rate: 115200
    - id: ch_uart_2
      channel: 2
      baud_rate: 9600
      rs485:                  # TNOW2 drives the DE/RE pins of an external transceiver
        invert: false

modbus:
  - id: modbus_bus
    uart_id: ch_uart_2
```

A complete example (flow control, GPIO, version text sensor) is in
[`code/test_ch9438.yaml`](../../code/test_ch9438.yaml).

## Configuration

### Hub (`ch9438:`)

| Option | Default | Description |
|---|---|---|
| `id` | — | Hub ID (`MULTI_CONF`: several chips are supported, each with its own `cs_pin`) |
| `cs_pin` | required | SPI chip select |
| `data_rate` | `4MHz` | SPI clock, 25 MHz maximum |
| `spi_mode` | `MODE3` | Do not change unless you know what you are doing (the datasheet does not state the mode, the vendor driver uses mode 3) |
| `irq_pin` | none | `INT#` input. With it, the chip is only read when the line is low. Without it, the status of all UARTs is read at every loop |
| `crystal` | `false` | `true` when a 16 MHz crystal is fitted on XI/XO (this uses the chip pin shared with GPIO7) |
| `uart` | required | List of UART channels, see below |

### UART channel (`uart:` list)

| Option | Default | Description |
|---|---|---|
| `id` | required | ID used by the consumers (`uart_id:`) |
| `channel` | required | 0 … 7 |
| `baud_rate` | required | 2930 … 12 000 000 |
| `data_bits` | `8` | Only 8 is supported by the chip |
| `stop_bits` | `1` | 1 or 2 |
| `parity` | `NONE` | `NONE`, `EVEN`, `ODD` |
| `rx_buffer_size` | `256` | Software ring buffer in bytes (64 … 4096). Bytes that do not fit stay in the chip FIFO |
| `flow_control` | `none` | `none`, `cts`, `rts_cts` (UART1 … UART4 only) |
| `rs485` | off | `true`, or `{enabled: true, invert: false}`. Activates the `TNOW` pin of the channel |

### GPIO pins

```yaml
switch:
  - platform: gpio
    name: ${name}_relay_gpio4
    pin:
      ch9438: ch9438_hub
      number: 4                # 0 … 7
      mode:
        output: true           # input / pullup / pulldown / output / open_drain
```

### `text_sensor` platform

```yaml
text_sensor:
  - platform: ch9438
    ch9438_id: ch9438_hub
    version:
      name: ${name}_chip_version
```

## Shared pins

Eight chip pins carry several functions, only **one** of which can be used at a time. The component checks it when
the configuration is validated (an error names the two conflicting functions).

| Chip pin (named after its GPIO) | Possible functions |
|---|---|
| GPIO0 | CTS1 / TNOW0 / GPIO0 |
| GPIO1 | RTS1 / TNOW1 / `INT#` / GPIO1 |
| GPIO2 | RTS2 / TNOW2 / GPIO2 |
| GPIO3 | CTS2 / TNOW3 / GPIO3 |
| GPIO4 | RTS3 / TNOW4 / GPIO4 |
| GPIO5 | CTS3 / TNOW5 / GPIO5 |
| GPIO6 | RTS4 / TNOW6 / GPIO6 |
| GPIO7 | CTS4 / TNOW7 / crystal XO / GPIO7 |

Consequences:

- Using `irq_pin` removes RTS1, TNOW1 and GPIO1. Without `irq_pin` they are available.
- `rs485` on UART *n* uses the pin of `TNOWn`. For example `rs485` on UART3 and `flow_control: cts` on UART2 are
  incompatible (both want the pin GPIO3).
- Only UART1 … UART4 have RTS/CTS.

## How it works

- **SPI protocol.** One address byte (`bit 7` = write; UART registers are at `channel * 8 + register`), a delay of
  3 µs, then the data. The FIFOs are read or written in a single burst after the address. A minimum idle time
  of 3 µs is respected between two chip selects.
- **Reception.** The chip offers a *bulk status* read (register `0x4C`) that returns IIR, LSR, MSR and the RX/TX FIFO
  counters of all 8 UARTs in a single 56-byte SPI transaction. The hub reads it in `loop()` (or only when `INT#` is
  low) and moves the bytes of the UARTs that have data directly from the chip FIFO into the ring buffer of the
  channel. **This register is not in the datasheet** (it is used by WCH's Linux driver).
- **Transmission.** `write_array()` reads the TX FIFO counter and writes as many bytes as the FIFO can take, so a
  write never overruns. `flush()` waits for the transmitter-empty flag.
- **Start-up.** Scratch register test, chip identification (`VER[3] = 0x7C`), pin function setup (every
  multiplexed function that is not used is switched off explicitly), clock selection, then per UART FIFO reset, line
  parameters, baud rate and interrupts.
- **TX counter.** The datasheet describes the TX FIFO counter as the *remaining room*. At start-up the component
  checks it on an idle UART (a free-space counter reads close to 1536, a used-space counter reads 0) and adapts.

## Limitations

- 8 data bits only (no 5/6/7 bits).
- Minimum baud rate 2930 bd: 1200 and 2400 bd are not possible.
- The hardware reset pin `RST#` and the low-power modes (`IDLE`, `SLEEP`, `SHUT`) are not used.
- `INT#` is optional here, but the vendor Linux driver always uses it. Polling at every loop costs one 56-byte SPI
  transaction per loop iteration; `irq_pin` removes it when the chip is idle.

## Hardware validation checklist

These points come from the documents, not from measurements, and should be confirmed on a bench:

1. SPI mode 3 and the 3 µs delays (the SPI test at boot fails with an explicit message otherwise).
2. The bulk status register `0x4C` (layout: IIR, LSR, MSR, RX count L/H, TX count L/H per UART).
3. Behaviour of `INT#` with the receive thresholds (256 / 512 / 1024 / 1280 bytes) and the timeout interrupt.
4. The TX FIFO counter semantic (the boot log tells which one has been detected).
5. TNOW polarity and timing on a real RS485 bus.

Use `logger: level: VERY_VERBOSE` to trace every SPI access.

## Directory layout

```
ch9438/
├── __init__.py          # hub config schema, pin conflict validation, codegen, GPIO pin schema
├── ch9438.h/.cpp        # hub, UART channels, GPIO pins
├── ch9438_defs.h        # registers and constants
├── text_sensor/
│   └── __init__.py      # chip version
└── README.md
```

## Credits

- WCH CH9438 datasheet V1.1.
- ESPHome `weikai` component (architecture reference).

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
