# victron_smart_shunt

Native ESPHome component reading a **Victron SmartShunt** (and BMV battery
monitors) through the **VE.Direct text protocol** (19200 baud, 8N1, receive only).
Some MPPT labels are decoded as well (shared VE.Direct parser).

## Wiring and UART

The SmartShunt VE.Direct port talks 3.3 V logic: connect its TX to the ESP32 RX
pin. Use a large RX buffer, one text block is about 300 bytes sent in a burst:

```yaml
uart:
  id: uart_shunt
  rx_pin: GPIO16
  baud_rate: 19200
  rx_buffer_size: 1024   # the default 256 bytes overflows if loop() is blocked for ~130 ms

victron_smart_shunt:
  - id: shunt
    uart_id: uart_shunt

sensor:
  - platform: victron_smart_shunt
    victron_smart_shunt_id: shunt
    battery_voltage:
      name: ${name}_battery_voltage
    battery_current:
      name: ${name}_battery_current
    state_of_charge:
      name: ${name}_state_of_charge
```

A configuration validation warning is printed when `rx_buffer_size` is below 512.
It only concerns the native UART of the ESP: a UART expander channel (for example
`weikai_spi`) has no `rx_buffer_size` option, no warning is printed and the buffering is
whatever the expander component provides (WK2168: 256 bytes hardware FIFO plus a 256 bytes
software ring buffer, which the ring buffer only helps when the expander's own `loop()` runs).
In that case keep the main loop short, a stall of more than ~130 ms can overflow the FIFO.

See `test_victron_smart_shunt.yaml` for the complete list of entities.

## Protocol handling

- **Checksum:** the fields of a block are kept until its checksum (modulo 256 sum of the block
  equal to 0) has been verified. A block with a bad checksum is dropped entirely and a warning
  is logged, so corrupted frames never reach the sensors.
- **HEX frames:** asynchronous HEX frames (`:A...\n`) that the device can insert in the middle
  of a text block are ignored and are not part of the checksum.
- **`---` values:** the device sends `---` when a value is not available (CE, SOC and TTG while
  the shunt is not synchronised, T without temperature probe). These are published as `NaN`
  (state `unknown` in Home Assistant) instead of `0`. A value that is not a number is ignored.
- **`alarm_reason_text`:** AR is a bit field, every active alarm is listed, for example
  `Low Voltage, Low SOC`.
- **Product IDs:** SmartShunt 500A/1000A/2000A, BMV-700/702/700H, BMV-710H Smart, BMV-712 Smart
  (rev 1 and 2) are recognised in the `pid` text sensor.

## Publishing behaviour

The device sends one block per second. The fast values (`battery_voltage`, `battery_current`,
`instantaneous_power`, `state_of_charge`, `consumed_amp_hours`, `time_to_go`, and the MPPT
`panel_voltage`, `panel_power`, `load_current`) are published at every block, even when unchanged,
so they can safely drive a PID or any logic expecting a regular update. The other numbers are
published only when they changed, or when the last publish is older than 60 s. Text sensors are
published on change only; `pid` and `fw_version` are published once.

## Implementation notes

- Byte-by-byte state machine with fixed-size label (15) and value (47) buffers, no heap
  allocation while parsing. A field longer than the buffers is dropped, not truncated. The
  fields of the current block wait in a 768 bytes buffer until the checksum is known.
- The UART is drained with `read_array()` in chunks of 64 bytes.
- Labels are resolved through a small static table, then a `switch` on the field.

## Reference

VE.Direct Protocol 3.34, Victron Energy:
<https://www.victronenergy.com/upload/documents/VE.Direct-Protocol-3.34.pdf>

## License

MIT License

Copyright (c) e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
