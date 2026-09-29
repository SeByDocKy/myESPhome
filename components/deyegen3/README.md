# deyegen3

Native ESPHome component for **DEYE GEN3 microinverters**, read and
controlled over their local TCP interface (Solarman V5 framing).

## Compatible models (GEN3 only)

Per [StephanJoubert/home_assistant_solarman](https://github.com/StephanJoubert/home_assistant_solarman)'s
own device lookup files (this component's register map is taken directly
from these):

- **2 MPPT** (`model: deye_2mppt`): SUN600G3-EU-230, SUN800G3-EU-230,
  SUN1000G3-EU-230
- **4 MPPT** (`model: deye_4mppt`): SUN1300G3-EU-230, SUN1600G3-EU-230,
  SUN2000G3-EU-230

This does **not** cover GEN4 hardware (e.g. SUN-M160G4-EU-Q0) -- confirmed
to use a different, incompatible register map (see
[home_assistant_solarman discussion #549](https://github.com/StephanJoubert/home_assistant_solarman/discussions/549)).
GEN4 would need its own component if this author ever gets hold of one.

## ⚠️ Likely need the logger's own serial number, not the inverter's

**Not confirmed on real Deye GEN3 hardware** (see "Blind implementation"
below), but worth flagging loudly rather than burying: on this author's own
TSUN GEN3 PLUS component (`tsungen3`), the Solarman V5 "Logger Serial" field
had to be the device's **"Monitoring SN"** -- a separate number from the
inverter's own printed serial, found on a small sticker -- not the inverter's
serial number itself. Since `logger_serial` here is the exact same V5
protocol field, the same distinction almost certainly applies: try the
logger/monitoring module's own serial number (look for a sticker separate
from the main nameplate), not the inverter's. This lines up with at least
one report from a related Deye **GEN4** discussion of a user explicitly
needing "the serial number from the logger/WiFi interface, not from the
inverter" to get a connection working at all -- consistent with, though not
direct confirmation for, GEN3.

## ⚠️ Blind implementation -- not tested on real hardware

Unlike this author's [`tsungen3`](../tsungen3) component (validated against a
real TSUN MX1000), **this component was written without access to any Deye
GEN3 unit**. Every register address, scale factor, 32-bit word order and the
"Inverter ID" model-detection logic come from a third-party project
([StephanJoubert/home_assistant_solarman](https://github.com/StephanJoubert/home_assistant_solarman),
`deye_2mppt.yaml`/`deye_4mppt.yaml`), not from a packet capture of this
author's own hardware. Treat every reading with proportionate suspicion until
confirmed, and please report back (working values, or wrong ones) once you
have this running against a real inverter -- see "Known unknowns" below for
exactly what's most likely to need correcting.

## Protocol

Same transport as `tsungen3`: **Solarman V5** framing (start byte, length,
control code, sequence, 4-byte logger serial, checksum, end byte) wrapping a
standard **Modbus RTU** frame, over plain TCP port 8899 ("client_mode" --
the ESP connects directly to the inverter's own IP, no cloud/DNS
redirection involved). This is not a Deye-specific protocol: Solarman is a
third-party WiFi data-logger module that many inverter brands (Deye, TSUN,
Sofar, Solis, ZCS Azzurro, ...) use in white-label form, which is why the
*transport* is identical across brands while the *register map* is not.

Each poll cycle issues a single Modbus "Read Holding Registers" (function
`0x03`) request covering `0x0001`-`0x007D` (125 registers) in one shot --
this single block happens to include the live telemetry, the rated power,
the running-status enum, and the "Inverter ID" string all at once, per the
source project's own request definition.

References:
- Register map, scaling, word order: [StephanJoubert/home_assistant_solarman](https://github.com/StephanJoubert/home_assistant_solarman)
  (`custom_components/solarman/inverter_definitions/deye_2mppt.yaml` and
  `deye_4mppt.yaml`)
- Frame structure: <https://pysolarmanv5.readthedocs.io/en/stable/solarmanv5_protocol.html>

## Scope

- Read-only telemetry: grid voltage/current/frequency, radiator temperature,
  rated power, current AC output power, PV1-4 voltage/current (power is
  *computed* as voltage × current -- no PV per-string power register exists
  in the source register map, unlike `tsungen3`), daily/total AC energy.
- `inverter_status` (`text_sensor`) publishes a decoded string --
  `Stand-by`/`Self-check`/`Normal`/`Warning`/`Fault` -- rather than raw hex.
  Unlike `tsungen3`, no separate alarm/fault bitmask register is known to
  exist for this protocol, so there's no `event_alarms`/`event_faults`
  equivalent here.
- **Write path**: `power_percent` (`number` and `output` platforms) writes
  the "Active Power Regulations" register (`0x0028`, function `0x06`) to cap
  the inverter's output as a direct percent value (unlike `tsungen3`'s
  100/1024 ratio, this register is 1 unit = 1%).
- No reset/AT+ equivalent is known for this protocol -- no `button`
  platform. (For context: `tsungen3`'s AT+Z button turned out to get no
  response at all over the same client_mode transport on real TSUN
  hardware, so its absence here isn't necessarily a loss.)
- All blocking network I/O runs on a dedicated FreeRTOS background task from
  the start (adopted directly from `tsungen3`'s post-hoc fix, rather than
  retrofitted later) -- `update()`/`set_power_percent()` only enqueue a job
  and return almost instantly; `loop()` drains the result queue on the main
  thread, which is the only place `publish_state()`/`ESP_LOGx` are called
  for poll/write outcomes. See `tsungen3`'s README for the full rationale
  and the real-hardware `loop_time` measurements that motivated it.
- `MULTI_CONF` is supported: several `deyegen3:` blocks (one per inverter,
  each with its own IP) can coexist on the same ESP.

### 2-MPPT vs 4-MPPT: auto-detection

A `model:` option (`auto` / `deye_2mppt` / `deye_4mppt`, default `auto`)
controls whether `pv3_voltage`/`pv3_current`/`pv3_power` and their PV4
equivalents ever publish.

With `model: auto` (the default), the hub reads the "Inverter ID" string
register (`0x0003`-`0x0007`, 5 registers / 10 ASCII bytes) on the first
successful poll and looks for `"1300"`, `"1600"` or `"2000"` in it to decide
it's the 4-MPPT variant; anything else (including a string that fails to
decode as printable ASCII) is treated as 2-MPPT, which is the conservative
default -- worst case you're just missing PV3/PV4 readings, rather than
publishing garbage values read from registers a 2-MPPT unit doesn't
implement. **This detection logic is a guess, not a confirmed spec** -- the
byte order and exact content of the ID string were not verified against
real hardware. If it picks the wrong variant on your unit, set `model:`
explicitly to override it (`dump_config()`/the startup log line "Inverter ID
read as ..." will tell you what it saw and decided).

## Known unknowns -- please report back once you have real hardware

- **32-bit register word order** (`Total Production` at `0x003F`/`0x0040`,
  `Total AC Output Power` at `0x0056`/`0x0057`): implemented as
  **high-word-first**, the opposite convention from `tsungen3`'s TSUN
  registers (which are low-word-first). This is this author's best guess
  from common Modbus/Solarman convention, not confirmed. If `current_power`
  or `ac_energy_total` come back wildly wrong (e.g. jumping by a factor of
  65536), this word order is the first thing to flip.
- **"Inverter ID" string decoding**: assumed ASCII, 2 characters per
  register, high byte first. Never seen an actual decoded value.
- **Temperature offset**: implemented as `(raw - 1000) * 0.01 °C`, per the
  source project's `offset: 1000, scale: 0.01` fields -- a common Deye
  convention, but not verified against this specific register/model.
- **`logger_serial`**: assumed to behave like `tsungen3`'s finding (default
  `0` gets no response in client_mode; the real logger serial is required)
  since it's the same V5 transport field regardless of brand -- not
  confirmed for Deye.
- **`modbus_address`**: defaulted to `1`, matching every other
  Solarman-based device seen so far (including this author's own TSUN
  finding) -- not confirmed for Deye.
- **`power_percent` write**: register address, function code and 1%-per-unit
  scale all come from the source project's read-only lookup file, which
  doesn't actually document a *write* path -- the assumption that `0x0028`
  is writable via function `0x06` the same way `tsungen3`'s Output
  Coefficient register is has NOT been tested at all. Test at low
  percentages first.
- **Whether `0x0001`-`0x007D` in a single FC03 request is actually accepted
  in client_mode**: the source project reads this range against a real
  Solarman collector, but not necessarily in the exact client_mode
  connection style this component uses -- if the poll fails outright, a
  smaller/split read may be needed.

## Example configuration

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [deyegen3]
    refresh: 10s

  # Local checkout instead (e.g. while developing the component itself):
  # - source:
  #     type: local
  #     path: components
  #   components: [deyegen3]

deyegen3:
  - id: sun1000
    ip_address: 192.168.1.60   # fixed IP of the inverter
    ip_port: 8899               # client-mode plain-TCP port
    modbus_address: 1
    logger_serial: 2093984xxx  # set to the real logger serial if polling fails
    model: auto                 # or "deye_2mppt" / "deye_4mppt" to override
    poll_interval: 30s

  # Second GEN3 microinverter on the same ESP (MULTI_CONF):
  # - id: sun1000_b
  #   ip_address: 192.168.1.61
  #   poll_interval: 30s

sensor:
  - platform: deyegen3
    deyegen3_id: sun1000
    grid_voltage:
      name: "SUN1000 Grid Voltage"
    grid_current:
      name: "SUN1000 Grid Current"
    grid_frequency:
      name: "SUN1000 Grid Frequency"
    temperature:
      name: "SUN1000 Temperature"
    rated_power:
      name: "SUN1000 Rated Power"
    current_power:
      name: "SUN1000 Current Power"
    ac_energy_today:
      name: "SUN1000 AC Daily Energy"
    ac_energy_total:
      name: "SUN1000 AC Total Energy"
    pv1_voltage:
      name: "SUN1000 PV1 Voltage"
    pv1_current:
      name: "SUN1000 PV1 Current"
    pv1_power:
      name: "SUN1000 PV1 Power"
    pv2_voltage:
      name: "SUN1000 PV2 Voltage"
    pv2_current:
      name: "SUN1000 PV2 Current"
    pv2_power:
      name: "SUN1000 PV2 Power"

text_sensor:
  - platform: deyegen3
    deyegen3_id: sun1000
    inverter_status:
      name: "SUN1000 Inverter Status"

number:
  - platform: deyegen3
    deyegen3_id: sun1000
    power_percent:
      name: "SUN1000 Power Percent"

output:
  - platform: deyegen3
    deyegen3_id: sun1000
    power_percent:
      id: sun1000_power_percent_output
```

## Directory layout

```
deyegen3/
├── __init__.py          # hub config schema + codegen
├── deyegen3.h/.cpp       # hub: TCP client, Solarman V5 framing, Modbus CRC/parsing
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
