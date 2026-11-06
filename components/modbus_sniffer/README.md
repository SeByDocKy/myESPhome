# Modbus sniffer - `modbus_sniffer`

Native ESPHome component that **passively listens to a Modbus RTU (RS485) bus** and exposes the values exchanged between
a master and a slave as ESPHome entities, without ever transmitting anything on the bus.

It is meant for the case where the master (a PLC, an inverter, a logger...) already polls the slave and you only want to
*read over its shoulder*: the ESP32 sees the requests and the responses, decodes the registers that you declared, and
publishes them to Home Assistant.

Supported platforms: `sensor` and `binary_sensor`.

## How it works

1. The hub reads the raw bytes of the UART and splits them into frames (end of frame = 50 ms of silence on the bus).
2. Every frame is checked with the Modbus CRC-16. Frames with a bad CRC are dropped (a warning is logged). If a request
   and its response arrive glued together in one burst (very fast slave), they are split again by looking for the
   shortest prefix with a valid CRC.
3. A **read request** (function `0x03` or `0x04`, 8 bytes) is remembered: slave address, function, start address and
   register count.
4. The next **read response** from the same slave and function is paired with that request, which gives the address of
   every register it carries.
5. Each declared entity whose address falls inside that block is decoded and published.

Consequences worth knowing:

- An entity is only updated when the master actually reads its register. If the master never polls it, it never changes.
- A response can only be decoded if the matching request was seen just before. Starting the ESP32 in the middle of an
  exchange simply loses that first response.
- Only the read functions `0x03` (holding registers) and `0x04` (input registers) feed the entities. Writes are only
  logged (`DEBUG`), exceptions are logged as warnings.

## Hardware

The ESP32 only needs to *receive*. With a MAX485 / MAX3485 transceiver, keep it in permanent receive mode:

```
MAX485          ESP32
  VCC  ───────  3.3V
  GND  ───────  GND
  RO   ───────  RX pin (uart rx_pin)
  DI   ───────  not connected (or TX pin, unused)
  RE   ───────  GND
  DE   ───────  GND
  A    ───────  bus A
  B    ───────  bus B
```

Set the baud rate, parity and stop bits of the `uart:` block to the ones of the bus.

> Do not enable the `uart:` `debug:` option with `dummy_receiver: true` on the same UART: the dummy receiver consumes
> the bytes and the hub then never sees anything in the receive buffer.

## Configuration

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [modbus_sniffer]

uart:
  - id: uart_0
    tx_pin: GPIO01
    rx_pin: GPIO03
    baud_rate: 9600

modbus_sniffer:
  - id: my_sniffer
    uart_id: uart_0
    address: 1
```

### Hub options

| Option    | Type         | Default | Description                                                              |
|-----------|--------------|---------|--------------------------------------------------------------------------|
| `id`      | ID           |         | Hub ID, referenced by the entities with `modbus_sniffer_id`.             |
| `uart_id` | ID           |         | The `uart:` to listen to.                                                |
| `address` | int (1-247)  | -       | Only handle frames of this slave address. Without it, all slaves are handled. |

Several hubs (several buses) can be declared (`MULTI_CONF`).

### Sensor

```yaml
sensor:
  - platform: modbus_sniffer
    modbus_sniffer_id: my_sniffer
    name: ${name}_power
    address: 0x0008
    value_type: S_WORD
    register_type: read
    unit_of_measurement: W
    device_class: power
    state_class: measurement
```

| Option              | Type    | Default   | Description                                                      |
|---------------------|---------|-----------|------------------------------------------------------------------|
| `modbus_sniffer_id` | ID      |           | The hub (optional if there is a single one).                     |
| `address`           | hex int |           | Register address (0x0000 - 0xFFFF). Required.                    |
| `value_type`        | enum    |           | How to decode the register(s), see below. Required.              |
| `register_type`     | enum    | `holding` | `holding` for function `0x03`, `read` for function `0x04`.       |

plus all the usual ESPHome sensor options (`unit_of_measurement`, `device_class`, `filters`, ...).

| `value_type` | Registers | Decoding                                        |
|--------------|-----------|-------------------------------------------------|
| `U_WORD`     | 1         | unsigned 16 bit                                 |
| `S_WORD`     | 1         | signed 16 bit                                   |
| `U_DWORD`    | 2         | unsigned 32 bit, high word first                |
| `S_DWORD`    | 2         | signed 32 bit, high word first                  |
| `U_DWORD_R`  | 2         | unsigned 32 bit, low word first                 |
| `S_DWORD_R`  | 2         | signed 32 bit, low word first                   |
| `FP32`       | 2         | IEEE 754 float, high word first                 |
| `FP32_R`     | 2         | IEEE 754 float, low word first                  |

Use a `multiply:` filter for scaling (e.g. `multiply: 0.1` for a register in tenths).

### Binary sensor

```yaml
binary_sensor:
  - platform: modbus_sniffer
    modbus_sniffer_id: my_sniffer
    name: ${name}_relay_1_status
    address: 0x0600
    bitmask: 0x0001
    register_type: holding
```

| Option              | Type    | Default   | Description                                                          |
|---------------------|---------|-----------|----------------------------------------------------------------------|
| `modbus_sniffer_id` | ID      |           | The hub (optional if there is a single one).                         |
| `address`           | hex int |           | Register address. Required.                                          |
| `bitmask`           | hex int |           | Mask applied to the 16 bit register. Required. ON if `(value & bitmask) != 0`. |
| `register_type`     | enum    | `holding` | `holding` (`0x03`) or `read` (`0x04`).                               |

Several binary sensors can share the same register with different bitmasks.

## Troubleshooting

- **Nothing is received**: check A/B polarity, the baud rate / parity / stop bits, and that `RE` and `DE` are tied to
  GND. Make sure the UART has no `debug:` block with `dummy_receiver: true`.
- **`Invalid CRC` warnings**: almost always a wrong parity or stop-bit setting.
- **An entity never updates**: the address or `register_type` does not match what the master reads, or the master never
  reads that register. Set `logger: level: DEBUG` to see the requests and responses the hub pairs.
- **Wrong value on 32 bit types**: try the `_R` variant (low word first).

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
