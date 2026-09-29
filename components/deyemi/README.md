# deyemi

Native ESPHome component for **DEYE microinverters, GEN3 and GEN4 alike**,
read and controlled over their local TCP interface (Solarman V5 framing).
"deyemi" = "Deye MIcroinverter" -- named without a generation number on
purpose, because both generations turn out to share the same register map
(see "Why one component for two generations" below).

## Compatible models

- **2 MPPT** (`model: deye_2mppt`):
  - GEN3: SUN600G3-EU-230, SUN800G3-EU-230, SUN1000G3-EU-230
  - GEN4: SUN-M60G4-EU-Q0, SUN-M80G4-EU-Q0, SUN-M100G4-EU-Q0
- **4 MPPT** (`model: deye_4mppt`):
  - GEN3: SUN1300G3-EU-230, SUN1600G3-EU-230, SUN2000G3-EU-230
  - GEN4: SUN-M130G4-EU-Q0, SUN-M160G4-EU-Q0, SUN-M180G4-EU-Q0,
    SUN-M200G4-EU-Q0, SUN-M220G4-EU-Q0

This does **not** cover TSUN/TSOL microinverters -- those have a genuinely
different register map and are handled by this author's separate
[`tsungen3`](../tsungen3) component (GEN3 PLUS only).

## Why one component for two generations

Deye's GEN3 and GEN4 microinverters are marketed and documented as distinct
product lines, so a first instinct (and this author's original plan) was to
split them into `deyegen3`/`deyegen4` components, mirroring how `tsungen3`
is scoped to TSUN's GEN3 PLUS specifically. That plan changed once the
research for GEN4 turned up direct evidence it wasn't warranted here:

In [StephanJoubert/home_assistant_solarman discussion #549](https://github.com/StephanJoubert/home_assistant_solarman/discussions/549),
user `flosch-dev` got a SUN-M160G4-EU-Q0 (GEN4) fully working by taking the
**GEN3** `deye_2mppt.yaml` template and adapting it to 4 trackers -- posting
the complete working register list, which matches the GEN3 lookup file's
addresses field-for-field (PV1-4 at `0x006D`-`0x0074`, Running Status at
`0x003B`, Total AC Power at `0x0056`/`0x0057`, Active Power Regulations at
`0x0028`, etc.). User `hellafritz` separately confirmed the same GEN3
template working on a SUN-M80G4-EU-Q0. Two independent real-hardware reports
saying "the GEN3 register map works unmodified on my GEN4 unit" was enough
to treat GEN3 and GEN4 as one protocol rather than two -- hence a single
`deyemi` component instead of a pair. The only thing that actually varies
between individual models is the MPPT count (2 or 4), which this component
already had to handle per-generation anyway (`model:` / auto-detection).

## ⚠️ Requires the data logger's own serial number, not the inverter's

Confirmed independently on **both** generations, not just by analogy: on
this author's TSUN hardware (different brand, same V5 transport), the
default `sn` of `0` got no response at all in client_mode. For
Deye GEN4 specifically, multiple users hit and solved the exact same issue:
*"User Serial Number from the Logger, not from the Inverter!"* (confirmed
working, SUN-M80G4-EU-Q0), and *"I used the inverter serial number instead
of the device serial number from the web interface..."* (SUN-M160G4-EU-Q0,
fixed by switching to the correct one). Set `sn` to the **data
logger's own serial number** -- found on the logger/WiFi module itself,
readable from the inverter's local web/AP interface -- not the serial number
printed on the inverter's own nameplate.

## ⚠️ Blind implementation -- not tested by this author's own hardware

This author has no Deye unit of either generation. Every register address,
scale factor and the 32-bit word order come from the sources above, not
from a packet capture of this author's own hardware -- see "Where this
register map comes from" implicit in the sections above, and "Known
unknowns" below for exactly what's most likely to still need correcting.
Please report back (working values, or wrong ones) once you have this
running against real hardware, GEN3 or GEN4.

## Protocol

Same transport as `tsungen3`: **Solarman V5** framing (start byte, length,
control code, sequence, 4-byte logger serial, checksum, end byte) wrapping a
standard **Modbus RTU** frame, over plain TCP port 8899 ("client_mode" --
the ESP connects directly to the inverter's own IP, no cloud/DNS
redirection involved). Solarman is a third-party WiFi data-logger module
used in white-label form across brands (Deye, TSUN, Sofar, Solis, ZCS
Azzurro, ...) and, as it turns out for Deye, across generations too -- the
*transport* is identical everywhere; the *register map* usually differs by
brand but not, here, by Deye generation.

Each poll cycle issues a single Modbus "Read Holding Registers" (function
`0x03`) request covering `0x0001`-`0x007D` (125 registers) in one shot.

References:
- Register map, scaling, word order (GEN3 source): [StephanJoubert/home_assistant_solarman](https://github.com/StephanJoubert/home_assistant_solarman)
  (`custom_components/solarman/inverter_definitions/deye_2mppt.yaml` and
  `deye_4mppt.yaml`)
- GEN4 confirmation of the same register list: [home_assistant_solarman discussion #549](https://github.com/StephanJoubert/home_assistant_solarman/discussions/549)
  (comments by `flosch-dev` and `hellafritz`, Aug 2024)
- Model datasheets: [SUN-M60/80/100G4-EU-Q0](https://www.deyeinverter.com/deyeinverter/2025/04/29/datasheet_sun-m60-80-100g4-eu-q0_250427_en.pdf),
  [SUN-M130/160/180/200/220G4-EU-Q0](https://www.deyeinverter.com/deyeinverter/2024/05/15/datasheet_sun-m130-160-180-200-220g4-eu-q0_240513_en.pdf)
- Frame structure: <https://pysolarmanv5.readthedocs.io/en/stable/solarmanv5_protocol.html>

## Scope

- Read-only telemetry: grid voltage/current/frequency, radiator temperature,
  rated power, current AC output power, PV1-4 voltage/current (power is
  *computed* as voltage × current -- no PV per-string power register exists
  in the register map, unlike `tsungen3`), daily/total AC energy.
- `inverter_status` (`text_sensor`) publishes a decoded string --
  `Stand-by`/`Self-check`/`Normal`/`Warning`/`Fault` -- rather than raw hex.
  No separate alarm/fault bitmask register is known to exist for this
  protocol, so there's no `event_alarms`/`event_faults` equivalent here
  (unlike `tsungen3`).
- **Write path**: `power_percent` (`number` and `output` platforms) writes
  the "Active Power Regulations" register (`0x0028`, function `0x06`) as a
  direct percent value. Note: at least one GEN4 model (SUN-M200G4-EU-Q0) is
  documented to floor this register at 1% rather than reaching a true 0%
  output -- see "Known unknowns" for a possible alternative register this
  component does not yet use.
- No reset/AT+ equivalent is known for this protocol -- no `button`
  platform.
- All blocking network I/O runs on a dedicated FreeRTOS background task --
  `update()`/`set_power_percent()` only enqueue a job and return almost
  instantly; `loop()` drains the result queue on the main thread, which is
  the only place `publish_state()`/`ESP_LOGx` are called for poll/write
  outcomes. See `tsungen3`'s README for the full rationale and the
  real-hardware `loop_time` measurements that motivated this pattern.
- `MULTI_CONF` is supported: several `deyemi:` blocks (one per inverter,
  each with its own IP, GEN3 or GEN4 or a mix) can coexist on the same ESP.

### 2-MPPT vs 4-MPPT: auto-detection

A `model:` option (`auto` / `deye_2mppt` / `deye_4mppt`, default `auto`)
controls whether a `dc_channels` entry for `pv2` or `pv3` (the 4-MPPT
variant's 3rd/4th strings) ever publishes. Note there is deliberately no
separate "GEN3" vs "GEN4" choice here -- the register map doesn't change
between them, so the MPPT count is the only thing that actually matters.

With `model: auto` (the default), the hub reads the "Inverter ID" string
register (`0x0003`-`0x0007`, 5 registers / 10 ASCII bytes) on the first
successful poll and looks for `"130"`, `"160"`, `"180"`, `"200"` or `"220"`
in it to decide it's the 4-MPPT variant -- these substrings cover both
generations' 4-MPPT model numbers at once (GEN3's 1300/1600/2000 and GEN4's
130/160/180/200/220), since e.g. "160" is a substring of both "1600" and
"M160G4". None of them collide with either generation's 2-MPPT model
numbers (GEN3: 600/800/1000; GEN4: 60/80/100). Anything that doesn't match
is treated as 2-MPPT, the conservative default -- worst case you're just
missing the `pv2`/`pv3` `dc_channels` entries rather than publishing garbage
values read from registers a 2-MPPT unit doesn't implement. **The exact
contents and byte order of the Inverter ID string were not confirmed by any
of the source reports**, so this detection logic is still a guess -- if it
picks the wrong variant on your unit, set `model:` explicitly to override it
(the startup log line "Inverter ID read as ..." will tell you what it saw
and decided). Note this is a runtime check only: since `model: auto` isn't
resolved until the first successful poll, YAML validation does **not**
cross-check the number of `dc_channels` entries against it -- declaring
`pv2`/`pv3` on what turns out to be 2-MPPT hardware isn't rejected, those
channels just never publish.

## Known unknowns -- please report back once you have real hardware

- **32-bit register word order** (`Total Production` at `0x003F`/`0x0040`,
  `Total AC Output Power` at `0x0056`/`0x0057`): implemented as
  **high-word-first**, this author's best guess from common Modbus/Solarman
  convention -- not confirmed by any source used here, GEN3 or GEN4. If
  `ac.power` or `ac.energy_total` come back wildly wrong (e.g. jumping
  by a factor of 65536), this word order is the first thing to flip.
- **"Inverter ID" string decoding**: assumed ASCII, 2 characters per
  register, high byte first. Never seen an actual decoded value from either
  generation.
- **Alternative power regulation register for true 0% output**: a
  [separate feature request](https://github.com/orgs/home-assistant/discussions/3354)
  for the SUN-M200G4-EU-Q0 (GEN4) documents that register `0x0028` (the one
  this component writes) floors at 1% output, and proposes register `0x0035`
  (decimal 53) as supporting the full 0-100% range instead. This component
  does **not** implement `0x0035` -- it wasn't confirmed working by anyone at
  the time of writing, just requested -- but it's a plausible follow-up if
  you need a true zero-injection shutdown and can test it safely. Whether
  this register also exists/works on GEN3 units is unknown.
- **`sn` mechanism**: the *requirement* (real logger serial, not
  the inverter's) is confirmed for GEN4 by real users (see the warning
  above); *why* the default of `0` gets rejected, and whether GEN3 behaves
  identically, was never explained by anyone, just worked around.
- **`modbus_address`**: defaulted to `1`, matching every other
  Solarman-based device seen so far (TSUN included) -- not explicitly
  confirmed for either Deye generation in the sources this component is
  based on.
- **Temperature offset**: implemented as `(raw - 1000) * 0.01 °C`, a common
  Deye convention per the source lookup file -- not verified against a real
  reading on either generation.
- **Coverage across the full model lineup**: GEN4 confirmations exist only
  for SUN-M160G4 (4-MPPT) and SUN-M80G4 (2-MPPT) specifically; the rest of
  the GEN4 lineup (M60G4/M100G4, M130G4/M180G4/M200G4/M220G4) and the entire
  GEN3 lineup are expected to share the same firmware family but weren't
  each individually confirmed.

## Example configuration

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [deyemi]
    refresh: 10s

  # Local checkout instead (e.g. while developing the component itself):
  # - source:
  #     type: local
  #     path: components
  #   components: [deyemi]

deyemi:
  - id: deye1
    ip_address: 192.168.1.60   # fixed IP of the inverter
    ip_port: 8899               # client-mode plain-TCP port
    modbus_address: 1
    sn: 2408xxxxx   # the DATA LOGGER's own serial number -- not the inverter's, see warning above
    model: auto                 # or "deye_2mppt" / "deye_4mppt" to override -- works for GEN3 and GEN4 alike
    poll_interval: 30s

  # Second Deye microinverter on the same ESP, GEN3 or GEN4 (MULTI_CONF):
  # - id: deye2
  #   ip_address: 192.168.1.61
  #   poll_interval: 30s

sensor:
  - platform: deyemi
    deyemi_id: deye1
    # One entry per MPPT string, 0-indexed (pv0, pv1, ...) -- matches this
    # author's hms/hmsw components. 2-MPPT hardware only needs pv0/pv1; the
    # 4-MPPT entries below are ignored (never published) on 2-MPPT units.
    dc_channels:
      - pv0:
          voltage:
            name: "Deye1 PV0 Voltage"
          current:
            name: "Deye1 PV0 Current"
          power:
            name: "Deye1 PV0 Power"
      - pv1:
          voltage:
            name: "Deye1 PV1 Voltage"
          current:
            name: "Deye1 PV1 Current"
          power:
            name: "Deye1 PV1 Power"
      - pv2:
          voltage:
            name: "Deye1 PV2 Voltage"
          current:
            name: "Deye1 PV2 Current"
          power:
            name: "Deye1 PV2 Power"
      - pv3:
          voltage:
            name: "Deye1 PV3 Voltage"
          current:
            name: "Deye1 PV3 Current"
          power:
            name: "Deye1 PV3 Power"
    ac:
      voltage:
        name: "Deye1 Grid Voltage"
      current:
        name: "Deye1 Grid Current"
      frequency:
        name: "Deye1 Grid Frequency"
      power:
        name: "Deye1 Current Power"
      energy_today:
        name: "Deye1 AC Daily Energy"
      energy_total:
        name: "Deye1 AC Total Energy"
    inverter:
      temperature:
        name: "Deye1 Temperature"
      rated_power:
        name: "Deye1 Rated Power"

text_sensor:
  - platform: deyemi
    deyemi_id: deye1
    inverter_status:
      name: "Deye1 Inverter Status"

number:
  - platform: deyemi
    deyemi_id: deye1
    power_percent:
      name: "Deye1 Power Percent"

output:
  - platform: deyemi
    deyemi_id: deye1
    power_percent:
      id: deye1_power_percent_output
```

## Directory layout

```
deyemi/
├── __init__.py          # hub config schema + codegen
├── deyemi.h/.cpp         # hub: TCP client, Solarman V5 framing, Modbus CRC/parsing
├── sensor/
│   └── __init__.py
├── text_sensor/
│   └── __init__.py
├── number/
│   └── __init__.py      # power_percent (writes Active Power Regulations, 0x0028)
├── output/
│   └── __init__.py      # power_percent (same write, as a FloatOutput)
└── README.md
```
