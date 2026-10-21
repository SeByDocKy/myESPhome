# apszb_inverter – one APsystems ZigBee micro-inverter (YC600 / QS1 / DS3)

`apszb_inverter` declares **one micro-inverter** of the [`apszb`](../apszb/README.md) hub and carries all its entities.
Declare one entry per inverter (up to 16); each entry is attached to the hub with `apszb_id`. The entities are
platforms of `apszb_inverter` and point to their inverter with `apszb_inverter_id: <id>`.

The hub (radio, pairing, polling, CC2530 flashing and wiring) is documented in the [`apszb` README](../apszb/README.md).
This page only covers the inverter side.

## Quick start

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome@main
    components: [apszb, apszb_inverter]

uart:
  id: uart_zb
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

apszb:
  id: aps
  radio: znp
  uart_id: uart_zb
  reset_pin: GPIO4
  ecu_id: "D8A3011B9780"

apszb_inverter:
  - id: inv1
    apszb_id: aps
    serial: "408000158215"
    type: yc600

sensor:
  - platform: apszb_inverter
    apszb_inverter_id: inv1
    ac_voltage:
      name: ${name}_ac_voltage
    power_total:
      name: ${name}_power_total
    energy_total:
      name: ${name}_energy_total
```

Several inverters of different types share the same hub: add one entry per inverter to the `apszb_inverter:` list and
use its `id` in the `apszb_inverter_id:` of its entities.

```yaml
apszb_inverter:
  - id: inv1
    apszb_id: aps
    serial: "408000158215"
    type: yc600
  - id: inv2
    apszb_id: aps
    serial: "801000123456"
    type: qs1
```

## Options

| Option | Default | Description |
|---|---|---|
| `id` | – | id of the inverter, used by the entities (`apszb_inverter_id: inv1`) |
| `apszb_id` | auto | id of the `apszb` hub (optional when there is a single hub) |
| `serial` | required | the 12 digits printed on the inverter |
| `type` | required | `yc600`, `qs1` or `ds3` |
| `short_address` | – | force the ZigBee short address (normally learnt by pairing and stored in flash), e.g. `0x103A` |
| `calibration` | `0` | W added to the power limit sent to the inverter, as in the original firmware (−100…100) |
| `rated_power` | by type | W, reference of the `power_limit_percent` output: 600 for `yc600`, 1200 for `qs1`, 730 for `ds3` |

## Entities (all optional, platform `apszb_inverter`)

Every platform lives in its own sub-directory of the component and needs `apszb_inverter_id: <id>`.

| Platform | Keys |
|---|---|
| `sensor` | `ac_voltage`, `frequency`, `temperature`, `power_total`, `energy_total`, `dc_voltage_1…4`, `dc_current_1…4`, `power_1…4`, `energy_1…4`, `link_quality`, `rssi`, `uptime` |
| `text_sensor` | `status` (unpaired / online / offline), `serial`, `short_address` |
| `binary_sensor` | `online` (goes off after several polls without answer) |
| `number` | `max_power` (throttling in W, read back from the inverter after each change) |
| `output` | `power_limit_percent` |
| `button` | `pair`, `refresh`, `query_max_power`, `reboot_inverter` |

* A `yc600` has two DC channels and a `qs1` has four: the numbered entities go from 1 to 4.
* `rssi` is only available with `radio: native`; the CC2530 only reports a link quality indicator.
* The inverters do not report an instantaneous power: like the original firmware, `power_x` is derived from the
  increase of the energy counter between two polls. `energy_total` is accumulated by the ESP and persisted in flash.
* An inverter only answers when its panels power it: at night it is `offline` and its powers fall to 0.

### Limiting the power

Two ways to throttle an inverter:

* `number: max_power` – a limit in W, set by hand.
* `output: power_limit_percent` – a float output from 0.0 to 1.0 (0…100 % of `rated_power`, never below 30 W), made for
  PID or zero-injection regulators. Bursts of writes are coalesced: only the latest limit is sent to the inverter.

```yaml
output:
  - platform: apszb_inverter
    apszb_inverter_id: inv1
    power_limit_percent:
      id: inv1_limit
```

## Ready-made packages

Two packages in [`code/`](../../code) avoid copying the whole configuration. The inverter package can be included once
per inverter, with its own name, id and serial:

```yaml
packages:
  apszb:
    url: https://github.com/SeByDocKy/myESPhome
    ref: main
    files:
      - path: code/device_apszb.yaml
        vars:
          apszb_id: aps
          apszb_radio: znp
          apszb_uart_id: uart_zb
          apszb_reset_pin: GPIO4
          apszb_ecu_id: D8A3011B9780
  apszb_inverter:
    url: https://github.com/SeByDocKy/myESPhome
    ref: main
    files:
      - path: code/device_apszb_inverter.yaml
        vars:
          apszb_inverter_name: yc600
          apszb_inverter_id: inv1
          apszb_inverter_apszb_id: aps
          apszb_inverter_serial: "408000158215"
          apszb_inverter_type: yc600
          apszb_inverter_calibration: 0
```

The inverter package adds all the entities, the daily energy (`total_daily_energy` on `power_total`) and the energy of
yesterday (template sensor). It needs an SNTP `time:` with the id `my_time` and the substitutions `my_hours`,
`my_minute` and `my_second`.

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
