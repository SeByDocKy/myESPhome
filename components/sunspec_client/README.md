# sunspec_client

Native ESPHome **SunSpec** client for the core `modbus` bus (RS485 or Modbus TCP through
`tcp_uart` + `modbus_tcp_uart`).

> **Status: milestone 1.** The component scans the device (finds the `SunS` marker, walks the chain of
> models) and publishes the list of model ids in a `text_sensor`. Creation of the measurement entities
> (with `name_prefix`, one single Home Assistant resync) comes in the next milestones.

## How it works

1. After `startup_delay`, the component looks for the `SunS` marker at registers 40000, 0 and 50000.
2. It walks the model headers (id, length) until the end marker `0xFFFF` (one 2-register request per model).
3. The ids found are published in the `models` text sensor (e.g. `1, 103, 160`).
4. On failure (no answer, no marker) the text sensor shows `scan failed` and a new scan starts after
   `scan_retry_interval`. A request without answer is retried 3 times first.

`models:` is the compile-time wish list (it will reserve entity slots); the entities finally created will be
the intersection with the models the device exposes.

## Configuration

```yaml
tcp_uart:
  - id: sunspec_link
    host: 192.168.1.50
    port: 502

modbus_tcp_uart:
  - id: sunspec_uart
    tcp_uart_id: sunspec_link

modbus:
  - id: modbus_sunspec
    uart_id: sunspec_uart

sunspec_client:
  - id: inverter
    modbus_id: modbus_sunspec
    address: 1                # Modbus unit id
    name_prefix: pv_inv       # prefix of the name of every created entity
    models: [1, 103, 160]
    startup_delay: 5s
    scan_retry_interval: 30s

text_sensor:
  - platform: sunspec_client
    sunspec_client_id: inverter
    models:
      name: Inverter models
```

`tcp_uart` / `modbus_tcp_uart` are not in a stable release yet (PR #20198 / #20061), see
`test_sunspec_client.yaml`. With RS485, use a regular `uart:` instead.

## Options

| Option | Default | Description |
|---|---|---|
| `modbus_id` | – | Core `modbus` bus |
| `address` | 1 | Modbus unit id |
| `name_prefix` | – | Prefix for the entity names |
| `models` | – | SunSpec model ids wanted |
| `startup_delay` | 5s | Wait before the first scan |
| `scan_retry_interval` | 30s | Delay before a new scan after a failure |

## License

MIT License

Copyright (c) e-2-nomy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
