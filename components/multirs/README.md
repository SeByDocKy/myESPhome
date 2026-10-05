# multirs - Victron Multi RS Solar over VE.Can (experimental)

Native ESPHome component for a **Victron Multi RS Solar** on a VE.Can bus. It sits on the [`vecan`](../vecan/README.md)
hub (any ESPHome `canbus`, e.g. an MCP2515) and works with or without a Cerbo GX on the bus.

> **Status: experimental, not tested on real hardware.** It was written from the public "VE.Can registers" list and
> from community reports, and checked only against a scripted fake device on a host build. Which registers a Multi RS
> really answers, and their scales, are **assumptions**. The component therefore ships discovery helpers, and writes
> are off by default.

## Configuration

```yaml
multirs:
  id: rs1
  vecan_id: vecan_hub
  address: 0x40              # logged by the vecan hub: "Victron device found at address 0x.."
  poll_interval: 10s         # register polling (battery values are broadcast, not polled)
  battery_instance: 0        # NMEA 2000 battery instance in PGN 127508
  write_enabled: false       # default: nothing is ever written
  log_unknown_registers: false
  scan_pages: []             # e.g. [0x2200, 0xD000]
```

## Entities

| Platform | Keys |
|---|---|
| `sensor` | `battery_voltage`, `battery_current`, `battery_power`, `battery_temperature` (PGN 127508); `ac_in_voltage`, `ac_in_current`, `ac_in_power`, `ac_in_apparent_power`, `ac_in_frequency`; `ac_out_voltage`, `ac_out_current`, `ac_out_power`, `ac_out_apparent_power`, `ac_out_frequency`; `pv_voltage`, `pv_power`; `energy_today`, `energy_yesterday`, `energy_total`; `internal_temperature` |
| `text_sensor` | `state`, `error`, `firmware_version`, `model`, `serial_number` |
| `switch` | `ups_function`, `generator_load_moderation`, `weak_ac_input` (register 0xD067) |
| `select` | `mode` (register 0x0200: Charger only, Inverter only, On, Off, Eco) |

Registers used (scale assumed in brackets): AC in 0x2230 V (0.01), 0x2231 A (0.1), 0x2234 W, 0x2235 VA, 0x2238 Hz (0.01);
AC out 0x2200, 0x2201, 0x2204, 0x2205, 0x2208 with the same scales; PV 0xEDBB (0.01 V), 0xEDBC (0.01 W); yield 0xEDD3,
0xEDD1, 0xEDDD (0.01 kWh); temperature 0xEDDB (0.01 degC); state 0x0201; error 0xEDDA; firmware 0x0102; model 0x010B;
serial 0x010A. A register the device refuses (NACK) is no longer polled and its entity stays "unknown".

`state` and `error` use the generic Victron state / charger-error tables; error numbers specific to the inverter
are shown as "Unknown".

## Verifying your unit

1. Keep `listen_only: true` first: the battery sensors and the device address appear without transmitting.
2. Switch to normal mode and compare each sensor with VictronConnect or the Cerbo.
3. If a value is wrong, fix it in YAML, no recompile of the component needed:
   ```yaml
   ac_out_power:
     name: ${name}_ac_out_power
     filters:
       - multiply: 0.1
   ```
4. To discover what your unit answers, set `scan_pages: [0x2200, 0xD000]` (or other pages) and
   `log_unknown_registers: true`. Each page is requested three times with the VREG "whole page" mask (0xFF00) and every
   register received is logged at INFO level as `SCAN register 0x.... (n bytes): hex`. Whether a Multi RS answers
   page requests is not verified either.

## Writing (off by default)

Settings live in the device's non-volatile memory, so this is meant for occasional changes, not control loops.
With `write_enabled: true`:

- the three **switches** change one 2-bit field of register **0xD067** ("AC input 1 control behaviour": 1 = yes,
  2 = no). The other fields are kept as the device reports them (read-modify-write), so the switch is refused until
  the register has been read once. Community reports say the Victron `vreg` tool refuses this register and a raw CAN
  frame is accepted; this component sends a raw VREG write, which is **unverified** on a real unit.
- the **select** writes register 0x0200. The values 1 = Charger only, 2 = Inverter only, 3 = On, 4 = Off, 5 = Eco are
  an assumption. Choosing "Off" really switches the device off.
- a value equal to the one the device already has is not written; two writes of one register are at least 10 s apart;
  the device must confirm by broadcasting the new value, otherwise the register is read back and the entity returns
  to the real value (an error is logged). A refusal (NACK) is logged with its meaning.
- not written with `listen_only: true`.

## Not covered

Number entities (charge limits, AC input current limit): the scales are unknown and the AC current limit needs the
"remote control used" handshake of the device. Binary sensors: the switches already show the state.
