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
| Baud rate | 2930 bd … 12 Mbd with the internal clock (`UART clock / divisor`, 12.4 fixed point divisor, see [Crystal and baud rate range](#crystal-and-baud-rate-range)) |
| FIFOs | 2048 bytes RX, 1536 bytes TX per UART |
| SPI | up to 25 MHz, **mode 0 (CPOL=0, CPHA=0)** as required by the vendor evaluation board manual, MSB first |
| GPIO | 8, push-pull / open-drain outputs, floating / pull-up / pull-down inputs |
| RS485 | `TNOW` direction pin per UART (automatic DE/RE, polarity selectable) |
| Flow control | hardware RTS/CTS on UART1 … UART4 |
| Interrupt | `INT#`, active low, shared by all UARTs (optional here) |
| Supply | VCC 1.8 … 3.6 V, VIO 1.65 V … VCC |
| Clock | internal (192 MHz PLL) or external crystal (16 MHz documented), multiplied by 12 |

## Wiring

| ESP32 | CH9438 |
|---|---|
| SPI CLK / MOSI / MISO | SCK / SDI / SDO |
| any GPIO (output) | SCS# |
| any GPIO (input, optional) | INT# (shared with the pin GPIO1 / RTS1 / TNOW1) |
| 3.3 V / GND | VCC (+ VIO) / GND |

`RST#`: connect it to a GPIO and set `reset_pin` (recommended: the vendor code resets the chip at start-up), or tie it high.

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
| `spi_mode` | `MODE0` | The evaluation board manual and the vendor MCU example use SPI mode 0 (CPOL=0, CPHA=0). The Linux driver of the CH943x family forces mode 3, which may also be accepted by the chip |
| `reset_pin` | none | Optional output connected to `RST#`: the chip is reset at start-up (low 100 ms, high 100 ms), like in the vendor code. Without it the component only waits until 500 ms after boot before the first access |
| `irq_pin` | none | `INT#` input. With it, the chip is only read when the line is low. Without it, the status of all UARTs is read at every loop |
| `crystal` | none | Frequency of the crystal fitted on XI/XO (`16MHz`, `11.0592MHz`, or a number of Hz); none = internal clock. `true` means 16 MHz (older syntax). Uses the chip pin shared with GPIO7. See [Crystal and baud rate range](#crystal-and-baud-rate-range) |
| `test_mode` | `0` | `0` off, `1` loopback of a short text, `2` loopback of a 1024 byte pattern + throughput. See [Test mode](#test-mode) |
| `uart` | required | List of UART channels, see below |

### UART channel (`uart:` list)

| Option | Default | Description |
|---|---|---|
| `id` | required | ID used by the consumers (`uart_id:`) |
| `channel` | required | 0 … 7 |
| `baud_rate` | required | Depends on the clock: 2930 … 12 000 000 with the internal clock (checked at validation) |
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

## Action `ch9438.change_baud_rate`

Changes the baud rate of one UART at run time. The bytes already queued in the transmit FIFO are sent first at the
old speed, the bytes already received are kept. A value outside 2930 … 12 000 000 is refused (error in the log, the
speed is not changed). The value can be a lambda.

```yaml
button:
  - platform: template
    name: ${name}_uart0_baud_9600
    on_press:
      - ch9438.change_baud_rate:
          id: ch_uart_0
          baud_rate: 9600

  - platform: template
    name: ${name}_uart0_baud_variable
    on_press:
      - ch9438.change_baud_rate:
          id: ch_uart_0
          baud_rate: !lambda 'return 921600;'
```

A frame being received at the exact moment of the change is lost or corrupted, as with any UART.

## Test mode

`test_mode` in the hub section enables a self-test and statistics (it is compiled in only when it is not 0, so it costs
nothing otherwise). It is meant to compare the chip with other UART expanders (`weikai`) on the same board.

| `test_mode` | Behaviour |
|---|---|
| `0` | Normal operation (default) |
| `1` | Every 2 s one UART after the other sends a short text and checks what it receives back |
| `2` | Every 1 s one UART after the other sends a 1024 byte pattern, checks it and logs the achieved throughput against the line limit |

The **TX and RX pins of every configured UART must be wired together** (loopback); an unwired UART reports a
timeout. The test never blocks `loop()`. In both modes a statistics line is logged every 10 s:

```
[stats 10000 ms] loop: 5120 calls (512/s), longest gap between two calls 2310 us | status reads: 5120, mean 74 us,
max 190 us | SPI: 5330 transactions (533/s), busy 4.1 % of the time | 11520 bytes moved to the ring buffers (1152 B/s)
```

- *longest gap between two calls*: how late `loop()` can be. It must stay well below the time needed to fill the chip
  FIFO (2048 bytes) at the baud rate used.
- *SPI busy*: share of the CPU time spent in SPI transactions, including the delays imposed by the chip.

Run the same test on the `weikai` component (its `test_mode` and `TEST_COMPONENT` flag) to get comparable figures.

## Crystal and baud rate range

The WCH evaluation board has a 16 MHz crystal (two 22 pF load capacitors) between XI and XO, and an unpopulated 0 Ω
resistor linking the XO net to the CTS4 / TNOW7 / GPIO7 net: it is fitted only when no crystal is used and CTS4,
TNOW7 or GPIO7 is needed.

The UART reference clock is 192 MHz with the internal oscillator. With an external crystal the chip multiplies it by
12 (a 16 MHz crystal gives the same 192 MHz, only more accurate). The baud rate is `UART clock / divisor` with a 16 bit
divisor, so:

- lowest baud rate = UART clock / 65535
- highest baud rate = UART clock / 16

| `crystal` | UART clock | Lowest baud rate | Highest baud rate | 2400 Bd |
|---|---|---|---|---|
| none (internal) | 192 MHz | 2930 | 12 000 000 | no |
| `16MHz` | 192 MHz | 2930 | 12 000 000 | no |
| `14.7456MHz` | 176.9 MHz | 2701 | 11 059 200 | no |
| `11.0592MHz` | 132.7 MHz | 2026 | 8 294 400 | yes (divisor 55296, exact) |
| `8MHz` | 96 MHz | 1465 | 6 000 000 | yes (divisor 40000, exact) |

1200 Bd cannot be reached with any crystal of 8 MHz or more.

> **Only 16 MHz is documented by the vendor.** Any other frequency is untested: the PLL may not lock, and the
> internal delays of the chip (including the SPI timings) scale with the clock. The component logs a warning, checks
> every `baud_rate` against the range computed from the crystal and shows the range in the log at start-up.

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
- Minimum baud rate 2930 bd with the documented clock: 1200 and 2400 bd are not possible. 2400 bd needs a lower (undocumented) crystal frequency, see [Crystal and baud rate range](#crystal-and-baud-rate-range).
- The hardware reset pin `RST#` and the low-power modes (`IDLE`, `SLEEP`, `SHUT`) are not used.
- `INT#` is optional here, but the vendor Linux driver always uses it. Polling at every loop costs one 56-byte SPI
  transaction per loop iteration; `irq_pin` removes it when the chip is idle.

## Hardware validation checklist

These points come from the documents, not from measurements, and should be confirmed on a bench:

1. SPI mode 0 and the 3 µs delays (the SPI test at boot fails with an explicit message otherwise).
2. The bulk status register `0x4C` (layout: IIR, LSR, MSR, RX count L/H, TX count L/H per UART).
3. Behaviour of `INT#` with the receive thresholds (256 / 512 / 1024 / 1280 bytes) and the timeout interrupt.
4. The TX FIFO counter semantic: the datasheet and the vendor example both use it as the free room; the boot log tells which one has been detected.
5. TNOW polarity and timing on a real RS485 bus.

Use `logger: level: VERY_VERBOSE` to trace every SPI access.

## Directory layout

```
ch9438/
├── __init__.py          # hub config schema, pin conflict validation, codegen, GPIO pin schema
├── ch9438.h/.cpp        # hub, UART channels, GPIO pins
├── ch9438_defs.h        # registers and constants
├── automation.h         # ch9438.change_baud_rate action
├── text_sensor/
│   └── __init__.py      # chip version
└── README.md
```

## Credits

- WCH CH9438 datasheet V1.1 and the CH9438 evaluation package (evaluation board manual and schematic, MCU example code): used for the SPI mode, the timings, the start-up sequence, the register map and the GPIO / pin function programming. Only facts were used, no code was copied.
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
