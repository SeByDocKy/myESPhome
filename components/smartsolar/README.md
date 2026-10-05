# smartsolar

Native ESPHome component for **Victron MPPT solar chargers with a VE.Can port** (BlueSolar / SmartSolar MPPT 150/xx),
built on the [`vecan`](../vecan/README.md) hub. Reads the live values and the main settings, and can change the
charger settings (`number` / `switch`, see [Writing settings](#writing-settings)).

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

number:
  - platform: smartsolar
    smartsolar_id: mppt1
    absorption_voltage:
      name: "Absorption Voltage"
      min_value: 52.0      # mandatory: the limits of YOUR battery
      max_value: 58.4

switch:
  - platform: smartsolar
    smartsolar_id: mppt1
    charger: { name: "Charger" }
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
| sensor | `input_voltage`, `input_power` | VREG `0xEDBB`, `0xEDBC` (PV side as seen by the charger) | V, W |
| sensor | `output_voltage`, `output_current`, `output_power` | VREG `0xEDD5`, `0xEDD7`, `0xEDD6` (battery side) | V, A, W |
| sensor | `charger_max_current` | VREG `0xEDDF` (rating of the charger) | A |
| sensor | `battery_temperature_reg` | VREG `0xEDEC` (temperature sensor, K converted to °C) | °C |
| text_sensor | `tracker_mode` | VREG `0xEDB3` | Off, Limited, MPP tracking |
| text_sensor | `additional_state` | VREG `0xEDD4` | active items, e.g. "Temperature dimming" or "None" |
| text_sensor | `state` | VREG `0x0201` | Off, Bulk, Absorption, Float, ... |
| text_sensor | `error` | VREG `0xEDDA` | charger error code as text |
| text_sensor | `firmware_version`, `model`, `serial_number` | VREG `0x0102`, `0x010B`, `0x010A` | read once |
| binary_sensor | `relay`, `alarm`, `low_voltage`, `high_voltage`, `solar_activity` | PGN 127501 (status 1..5) | on / off |
| number | `absorption_voltage`, `float_voltage`, `equalization_voltage`, `max_charge_current` | see "Writing settings" | V, V, V, A |
| switch | `charger` | see "Writing settings" | on / off |

`input_*` and `output_*` come from registers that Victron defines for the HEX protocol; the docs say the charger
normally broadcasts the live data as PGNs, so these registers may be absent on some models (they are then NACKed once and
no longer polled). They are an alternative to the PGN based `pv_*` / `battery_*` entities, handy when the instance
numbers of the PGNs are not what you expect.

Only the registers behind the entities you configure are requested. A register the charger refuses with a NACK is
logged once and not polled again; a value reported as "not available" is published as `NaN`.

## Writing settings

| Platform | Key | VREG | Range enforced by the component |
|----------|-----|------|---------------------------------|
| number | `absorption_voltage` | `0xEDF7` (un16, 0.01 V) | `min_value`..`max_value` from YAML, and 5..70 V |
| number | `float_voltage` | `0xEDF6` (un16, 0.01 V) | same; must not exceed the absorption voltage |
| number | `equalization_voltage` | `0xEDF4` (un16, 0.01 V) | same |
| number | `max_charge_current` | `0xEDF0` (un16, 0.1 A) | `min_value`..`max_value` from YAML, and 0..100 A |
| switch | `charger` | `0x0200` device mode (un8) | off = 4; on = the last non-off mode the charger reported (1 by default) |

These registers live in the **non-volatile memory** of the charger and are meant for occasional configuration, not
for continuous regulation. A wrong battery voltage can damage a battery. The component therefore applies these rules:

- `min_value` and `max_value` are **mandatory** on every `number`: write down the limits of your battery.
- Writing needs a transmitting hub: it is refused when `vecan` is in `listen_only`, and while the address claim runs.
- A value equal to the one the charger already reports is never written.
- Writes are **debounced** (1.5 s for numbers: dragging a slider results in one write) and a register is not written
  more than once every **10 s**; later requests are merged into one write.
- `float_voltage` > `absorption_voltage` is refused locally (both when known from the charger or from a pending write).
- The entity is **only updated once the charger confirms** (it broadcasts the register with its new value).
  If nothing arrives within 2 s the register is read back; if the charger still has another value, an error is logged
  and the entity goes back to the real value. A NACK (for example `0x8300` value out of range) does the same.
- Do not drive these entities from a fast automation loop.
- Per Victron's register document (v23): the charge-algorithm settings (absorption, float, equalisation voltage...) can
  only be changed when the **battery type is "user defined" (255)**, otherwise the charger refuses the write (NACK).
  Some chargers also ignore them when a physical switch or a BMS controls the charging.

Frame layout (from Victron's register document): `66 99 regL regH v0 v1 v2 v3`, sent to the charger's address.
Example, absorption 58.00 V: `66 99 F7 ED A8 16 00 00`.

Not implemented: battery type, automatic equalisation, load output control and other settings, whose value tables
differ between models. They need to be checked on a real charger first (a register the charger does not have is
answered with a NACK).

## Verification status

- Unit tests (`tests/test_vecan_proto.cpp`) replay the frames given as examples in Victron's register document.
- `tests/host_e2e.yaml` runs the whole stack natively against a scripted fake MPPT: address claim, discovery, broadcast
  decoding, register requests, fast-packet model name, NACK handling, frames of other devices being ignored.
- The same run replays a write scenario (`tests/host_e2e.yaml`, `interval:` script): confirmed write, local refusals
  (float above absorption, outside the hard limits), charger NACK, write not applied by the charger, on/off switch
  with the 10 s spacing, and three quick slider changes merged into one write.
- **Not yet tested on a real charger.** The write format and the register ids come from Victron's public register
  document (v23, 2026-09); the mode values are `4` = off and, for "on", whatever non-off mode the charger reports
  (the register document lists 1 = charger only, 3 = on, 5 = eco for VE.Bus products, MPPTs usually use 1); confirm
  on your charger: use `listen_only: false`, read `0x0200` first (the `charger` switch reflects it) and then try. The PGN/register meanings come from Victron's public documents, which describe
  the BlueSolar MPPT 150/70 and 150/85 (PV on battery instance 1 of PGN 127508, binary status bits 1..5).
  Newer SmartSolar VE.Can models may map things differently: if `pv_*` stay empty, sniff the bus in `listen_only` mode
  and adjust `battery_instance` / `pv_instance`.
