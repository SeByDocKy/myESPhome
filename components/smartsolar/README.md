# smartsolar

Native ESPHome component for **Victron MPPT solar chargers with a VE.Can port** (BlueSolar / SmartSolar MPPT 150/xx),
built on the [`vecan`](../vecan/README.md) hub. Read-only in this first version.

Live values are broadcast by the charger as standard NMEA 2000 PGNs and are simply listened to. Everything else is
read from Victron registers (VREGs) requested every `poll_interval`.

## Configuration

```yaml
vecan:
  id: vecan_hub
  canbus_id: can_bus

smartsolar:
  id: mppt1
  vecan_id: vecan_hub
  address: 0x20          # source address of the charger on the bus
  poll_interval: 10s     # register polling period (default 10s)
  battery_instance: 0    # NMEA 2000 battery instance carrying the battery side in PGN 127508
  pv_instance: 1         # ... and the PV side

sensor:
  - platform: smartsolar
    smartsolar_id: mppt1
    battery_voltage: { name: "Battery Voltage" }
    pv_power:        { name: "PV Power" }
    # see the table below for all keys

text_sensor:
  - platform: smartsolar
    smartsolar_id: mppt1
    state: { name: "Charger State" }

binary_sensor:
  - platform: smartsolar
    smartsolar_id: mppt1
    solar_activity: { name: "Daylight" }
```

`example_smartsolar.yaml` (repository root) is a complete ESP32 + MCP2515 example. Several chargers can share one bus:
add one `smartsolar:` block per charger, each with its own `address`.

### Finding the address

Flash the example in `listen_only` mode: the hub logs `Victron device found at address 0x..` for every Victron node.
If nothing is received for 30 s the component warns; check the bit rate (250 kbit/s), the MCP2515 crystal (`clock`),
the wiring and the bus termination (120 Ω at both ends).

## Entities

| Platform | Key | Source | Unit / values |
|----------|-----|--------|---------------|
| sensor | `battery_voltage`, `battery_current`, `battery_temperature` | PGN 127508, `battery_instance` | V, A, °C |
| sensor | `battery_power` | computed (V × I) | W |
| sensor | `pv_voltage`, `pv_current` | PGN 127508, `pv_instance` | V, A |
| sensor | `pv_power` | computed (V × I) | W |
| sensor | `energy_today` / `energy_yesterday` / `energy_total` | VREG `0xEDD3` / `0xEDD1` / `0xEDDD` | kWh |
| sensor | `max_power_today` / `max_power_yesterday` | VREG `0xEDD2` / `0xEDD0` | W |
| sensor | `internal_temperature` | VREG `0xEDDB` | °C |
| sensor | `absorption_voltage`, `float_voltage`, `max_charge_current` | VREG `0xEDF7`, `0xEDF6`, `0xEDF0` | V, V, A |
| text_sensor | `state` | VREG `0x0201` | Off, Bulk, Absorption, Float, ... |
| text_sensor | `error` | VREG `0xEDDA` | charger error code as text |
| text_sensor | `firmware_version`, `model`, `serial_number` | VREG `0x0102`, `0x010B`, `0x010A` | read once |
| binary_sensor | `relay`, `alarm`, `low_voltage`, `high_voltage`, `solar_activity` | PGN 127501 (status 1..5) | on / off |

Only the registers behind the entities you configure are requested. A register the charger refuses with a NACK is
logged once and not polled again; a value reported as "not available" is published as `NaN`.

## Verification status

- Unit tests (`tests/test_vecan_proto.cpp`) replay the frames given as examples in Victron's register document.
- `tests/host_e2e.yaml` runs the whole stack natively against a scripted fake MPPT: address claim, discovery, broadcast
  decoding, register requests, fast-packet model name, NACK handling, frames of other devices being ignored.
- **Not yet tested on a real charger.** The PGN/register meanings come from Victron's public documents, which describe
  the BlueSolar MPPT 150/70 and 150/85 (PV on battery instance 1 of PGN 127508, binary status bits 1..5).
  Newer SmartSolar VE.Can models may map things differently: if `pv_*` stay empty, sniff the bus in `listen_only` mode
  and adjust `battery_instance` / `pv_instance`.
