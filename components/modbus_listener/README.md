# Modbus listener - `modbus_listener`

Native ESPHome component that **passively listens to a Modbus RTU (RS485) bus** and exposes every frame it sees as a
hex string in a `text_sensor`: one sensor for the **requests** (`tx`, master to slave) and one for the **responses**
(`rx`, slave to master). It never transmits anything on the bus.

Unlike [`modbus_sniffer`](../modbus_sniffer), which decodes declared registers into numeric entities, the listener
gives you the **raw frames**. The output format is configurable, so the string can be pasted directly into a script, a
Home Assistant template or a test vector. The last frames are also available from C++ (lambdas), which makes it easy to
decode them yourself.

Typical uses:

- understand what a master (inverter, PLC, logger...) and a slave say to each other before writing a real component,
- build a test vector (`use_hexa` + `use_comma` + `use_bracket` gives a C array literal),
- watch a bus from Home Assistant without any decoding.

## Entities

| Platform      | Key  | Direction        | Content                                                    | Published when                         |
|---------------|------|------------------|------------------------------------------------------------|----------------------------------------|
| `text_sensor` | `tx` | master to slave  | the last **request** frame as a hex string                 | a request with a valid CRC is seen     |
| `text_sensor` | `rx` | slave to master  | the last **response** frame as a hex string                | a response with a valid CRC is seen    |

That is all: the component creates **at most two entities per `text_sensor` block**, and each of them is optional (at
least one of `tx` / `rx` is required). Both accept every usual text sensor option (`name`, `id`, `icon`, `filters`,
`internal`, `disabled_by_default`, `entity_category`, `on_value`, ...).

## How it works

1. The hub reads the raw bytes of the UART. A frame is considered complete as soon as its length and its Modbus CRC-16
   are consistent, so a request and its response are separated even when the slave answers very quickly. A silence of
   10 ms on the bus is used as a fallback.
2. Frames with a bad CRC are dropped (a warning with the raw bytes is logged).
3. If `address` is set, frames of other slaves are ignored.
4. Frames are classified. For functions `0x03` / `0x04`, an 8 byte frame is a **request** and a frame of
   `3 + ByteCount + 2` bytes is a **response**. Any other frame (writes, exceptions...) is treated as a request.
5. The frame is post-processed (`add_crc`, then `payload_length`), formatted (`use_*` options) and published on the `tx`
   or `rx` text sensor. The same processed frame is kept for the C++ getters.

### Anatomy of the frames

Read holding / input registers (`0x03` / `0x04`):

```
Request  (8 bytes):  01 03 00 00 00 02 C4 0B
                     |  |  |---| |---| |---|
                     |  |  start  count CRC
                     |  function
                     slave address

Response (9 bytes):  01 03 04 12 34 56 78 4B 6A
                     |  |  |  |---------| |---|
                     |  |  |  data        CRC
                     |  |  byte count (4 = 2 registers)
                     |  function
                     slave address
```

The response does not contain the start address: it is the one of the request published just before on `tx`.

## Hardware

The ESP only needs to *receive*. Use a **3.3 V RS485 to TTL module with automatic direction control** (`3V3`, `GND`,
`RX`, `TX` on the TTL side, no `DE`/`RE` pin to drive):

```
RS485 module        ESP32
  3V3  ───────────  3V3
  GND  ───────────  GND
  TX   ───────────  RX pin (uart rx_pin)
  RX   ───────────  TX pin (uart tx_pin, never used by the listener)
  A    ───────────  bus A
  B    ───────────  bus B
```

The **TX of the module** (data coming from the bus) goes to the **RX of the ESP**. If nothing is received, check the
labelling of your module (some name the pins from the ESP side) and swap the two signal wires. Set the baud rate,
parity and stop bits of the `uart:` block to the ones of the bus.

> Do not enable the `uart:` `debug:` option with `dummy_receiver: true` on the same UART: the dummy receiver consumes
> the bytes and the hub never sees anything.

> ESP8266 / ESP-01: there is a single full UART, shared with the logger. Use `logger: baud_rate: 0` (or move the
> logger) when the bus is on `GPIO3`.

## Minimal configuration

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [modbus_listener]

uart:
  - id: uart_0
    tx_pin: GPIO01
    rx_pin: GPIO03
    baud_rate: 9600

modbus_listener:
  - id: my_listener
    uart_id: uart_0

text_sensor:
  - platform: modbus_listener
    modbus_listener_id: my_listener
    tx:
      name: ${name}_tx
    rx:
      name: ${name}_rx
```

## Full example

A complete, ready-to-flash file using every option and both entities:

```yaml
substitutions:
  name: modbus_listener

esphome:
  name: ${name}

esp32:
  board: esp32dev
  framework:
    type: esp-idf

logger:
  level: DEBUG          # DEBUG shows every frame with its detected type

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:

ota:
  - platform: esphome

external_components:
  - source: github://SeByDocKy/myESPhome
    components: [modbus_listener]
    refresh: 0s

# The bus: only RX is used, TX is never driven by the listener
uart:
  - id: uart_0
    tx_pin: GPIO17
    rx_pin: GPIO16
    baud_rate: 9600
    data_bits: 8
    stop_bits: 1
    parity: NONE

modbus_listener:
  - id: my_listener
    uart_id: uart_0
    address: 1              # optional: only slave 1
    use_hexa: true          # 0x01 0x03 ...
    use_comma: true         # 0x01, 0x03, ...
    use_bracket: true       # [0x01, 0x03, ...]
    use_quote: false        # '[0x01, 0x03, ...]'
    add_crc: true           # keep the 2 CRC bytes
    # payload_length: 8     # optional: pad with 00 / truncate

text_sensor:
  - platform: modbus_listener
    modbus_listener_id: my_listener
    tx:
      name: ${name}_tx
      id: ${name}_tx
      icon: "mdi:arrow-right-bold"
    rx:
      name: ${name}_rx
      id: ${name}_rx
      icon: "mdi:arrow-left-bold"
```

Resulting entities in Home Assistant:

| Entity                          | Example state                                               |
|---------------------------------|-------------------------------------------------------------|
| `text_sensor.modbus_listener_tx` | `[0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B]`         |
| `text_sensor.modbus_listener_rx` | `[0x01, 0x03, 0x04, 0x12, 0x34, 0x56, 0x78, 0x4B, 0x6A]`   |

### Only requests, or only responses

Each of `tx` / `rx` can be declared alone:

```yaml
text_sensor:
  - platform: modbus_listener
    modbus_listener_id: my_listener
    rx:
      name: ${name}_responses
      icon: "mdi:arrow-left-bold"
```

### Several buses

The hub is `MULTI_CONF`: declare one hub per UART and point each `text_sensor` block to its hub.

```yaml
uart:
  - id: uart_inverter
    rx_pin: GPIO16
    tx_pin: GPIO17
    baud_rate: 9600
  - id: uart_meter
    rx_pin: GPIO18
    tx_pin: GPIO19
    baud_rate: 19200

modbus_listener:
  - id: listener_inverter
    uart_id: uart_inverter
  - id: listener_meter
    uart_id: uart_meter
    address: 3

text_sensor:
  - platform: modbus_listener
    modbus_listener_id: listener_inverter
    rx:
      name: ${name}_inverter_rx
  - platform: modbus_listener
    modbus_listener_id: listener_meter
    rx:
      name: ${name}_meter_rx
```

## Options

### Hub options (`modbus_listener:`)

| Option           | Type        | Default | Description                                                                 |
|------------------|-------------|---------|-----------------------------------------------------------------------------|
| `id`             | ID          |         | Hub ID, referenced by the text sensors with `modbus_listener_id`.           |
| `uart_id`        | ID          |         | The `uart:` to listen to.                                                   |
| `address`        | int (1-247) | -       | Only handle frames of this slave address. Without it, all slaves are handled. |
| `use_comma`      | boolean     | `false` | Separate bytes with `", "` instead of a space.                              |
| `use_hexa`       | boolean     | `false` | Prefix each byte with `0x`.                                                 |
| `use_bracket`    | boolean     | `false` | Surround the result with `[` `]`.                                           |
| `use_quote`      | boolean     | `false` | Surround the result with single quotes `'`.                                 |
| `add_crc`        | boolean     | `true`  | When `false`, the 2 CRC bytes are removed from the end of the frame.        |
| `payload_length` | int (1-255) | -       | Force the length of the output: shorter frames are padded with `00`, longer ones truncated. Applied after `add_crc`. |

### Text sensor options (`platform: modbus_listener`)

| Option               | Type               | Description                                                   |
|----------------------|--------------------|---------------------------------------------------------------|
| `modbus_listener_id` | ID                 | The hub (optional if there is a single one).                  |
| `tx`                 | text sensor schema | Publishes the requests (master to slave).                     |
| `rx`                 | text sensor schema | Publishes the responses (slave to master).                    |

### Output formatting

The options are combined in this order: `use_quote( use_bracket( byte (use_comma) byte ... ) )`.
Examples for the request `01 03 00 00 00 02 C4 0B`:

| Options                                          | Result                                                  |
|--------------------------------------------------|---------------------------------------------------------|
| none                                             | `01 03 00 00 00 02 C4 0B`                               |
| `use_comma`                                      | `01, 03, 00, 00, 00, 02, C4, 0B`                        |
| `use_hexa`                                       | `0x01 0x03 0x00 0x00 0x00 0x02 0xC4 0x0B`               |
| `use_hexa` + `use_comma` + `use_bracket`         | `[0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B]`      |
| the above + `use_quote`                          | `'[0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B]'`    |
| `add_crc: false`                                 | `01 03 00 00 00 02`                                     |
| `add_crc: false` + `payload_length: 8`           | `01 03 00 00 00 02 00 00`                               |
| `payload_length: 4`                              | `01 03 00 00`                                           |

`add_crc` and `payload_length` apply to the published strings **and** to the C++ getters.

## Using the frames in lambdas

The hub keeps the last processed frames (after `add_crc` / `payload_length`):

| Method                | Returns                                      |
|-----------------------|----------------------------------------------|
| `get_last_tx_frame()` | last request as `std::vector<uint8_t>`       |
| `get_last_rx_frame()` | last response as `std::vector<uint8_t>`      |
| `get_last_frame()`    | last frame seen, request or response         |

### Example: decode a register yourself

With a master that reads 2 holding registers, the response is
`slave func bytecount hi lo hi lo crc crc`. A template sensor can decode the first register on each response:

```yaml
sensor:
  - platform: template
    name: ${name}_register_0
    id: ${name}_register_0
    unit_of_measurement: W
    device_class: power
    state_class: measurement
    update_interval: 10s
    lambda: |-
      std::vector<uint8_t> rx = id(my_listener).get_last_rx_frame();
      if (rx.size() < 5 || rx[1] != 0x03)
        return {};                       // nothing usable yet: keep the previous state
      return (int16_t) ((rx[3] << 8) | rx[4]);
```

This relies on `add_crc: true` or `false` indifferently, because the register bytes come before the CRC.
For a precise address / register pairing, or several registers with value types and bitmasks, use
[`modbus_sniffer`](../modbus_sniffer) instead, which does it for you.

### Example: react to a new response

```yaml
text_sensor:
  - platform: modbus_listener
    modbus_listener_id: my_listener
    rx:
      name: ${name}_rx
      on_value:
        then:
          - lambda: |-
              ESP_LOGI("app", "new response: %s (%d bytes)", x.c_str(),
                       (int) id(my_listener).get_last_rx_frame().size());
```

## Troubleshooting

- **Nothing is received**: check A/B polarity, baud rate / parity / stop bits and the TX/RX wiring (module `TX` to
  ESP RX). Make sure the UART has no `debug:` block with `dummy_receiver: true`.
- **`Invalid CRC` warnings**: almost always a wrong parity or stop-bit setting, or a bus with several masters.
- **A frame ends up in `tx` instead of `rx`**: only functions `0x03` / `0x04` are classified as responses; every other
  frame (write requests and their echo, exceptions) is published as a request.
- **Nothing in `rx` while `tx` updates**: the slave does not answer, or it answers with a frame whose length does not
  match its byte count.
- **No entity in Home Assistant**: at least one of `tx` / `rx` must be declared under the `text_sensor:` platform.
- Set `logger: level: DEBUG` to see every frame with its detected type, and `VERBOSE` to see the CRC / padding steps.

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
