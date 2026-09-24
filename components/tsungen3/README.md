# tsungen3

Native ESPHome component for **TSUN / TSOL GEN3 PLUS** micro-inverters (e.g.
MX1000, MX3000, MX450, MS1600/1800/2000, MS2000-D, MS800) and compatible
GEN3 PLUS storage systems, read over their local TCP interface.

## Protocol

GEN3 PLUS devices normally talk to the TSUN/Solarman cloud
(`iot.talent-monitoring.com:10000`) using **Solarman V5** framing: a small
TCP envelope (start byte, length, control code, sequence, 4-byte logger
serial, checksum, end byte) wrapping a standard **Modbus RTU** frame
(address + function code + registers + CRC16).

On firmware that exposes an SSL-encrypted cloud connection (port 10443), the
device also keeps listening on **plain TCP port 8899** on its own IP — this
is the interface the [s-allius/tsun-gen3-proxy](https://github.com/s-allius/tsun-gen3-proxy)
project calls "client mode", and what this component talks to directly. No
DNS redirection, MQTT broker or cloud loop is involved; the ESP just opens a
short-lived TCP connection to the inverter's IP every `update_interval` and
issues one Modbus "Read Holding Registers" (function `0x03`) request for the
live-data block `0x3000`..`0x3029`.

References:
- Frame structure: <https://pysolarmanv5.readthedocs.io/en/stable/solarmanv5_protocol.html>
- Register map: <https://github.com/s-allius/tsun-gen3-proxy/wiki/MODBUS-registers>

## Scope (v1)

- Read-only telemetry via a single Modbus request per poll cycle (registers
  `0x3000`-`0x3029`): grid voltage/current/frequency, temperature, rated &
  current power, PV1-4 voltage/current/power, AC daily/total energy.
- Inverter Status / Event Alarms / Event Faults are exposed as raw `0x____`
  hex strings (`text_sensor`) — the bitmap meanings aren't documented
  upstream, so no decoding is attempted yet.
- No MODBUS writes (rated power, output coefficient) and no AT+ commands.
  Same incremental approach as this author's other components (`hm`, `hms`,
  `hmsw`, `pcm3k6w`): read-only first, controls added once the read path is
  verified against real hardware.
- One TCP connection is opened, used, and closed per poll cycle (no
  persistent connection).
- `MULTI_CONF` is supported: several `tsungen3:` blocks (one per inverter,
  each with its own IP) can coexist on the same ESP, same as `hmsw`.

### Unverified assumptions — please report back if your inverter disagrees

- **Modbus slave/unit address**: defaults to `1` (`modbus_address` option).
- **Solarman "Logger Serial" field**: defaults to `0`. Some firmware may
  require the real "Monitoring SN" printed on the inverter's sticker
  (`logger_serial` option) instead of `0` to answer at all.
- **32-bit register decoding** (`AC Total Energy` at `0x301d`/`0x301e`):
  implemented as low-word-at-lower-address, matching the tsun-gen3-proxy
  wiki's "uInt32LE" notation, but not checked against a packet capture.

## Example configuration

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [tsungen3]

tsungen3:
  - id: mx1000
    host: 192.168.1.50        # fixed IP of the inverter
    port: 8899                 # client-mode plain-TCP port
    modbus_address: 1
    # logger_serial: 2093984xxx  # uncomment if the inverter never responds with 0
    poll_interval: 30s

  # Second GEN3 PLUS inverter on the same ESP (MULTI_CONF):
  # - id: mx1000_b
  #   host: 192.168.1.51
  #   poll_interval: 30s

sensor:
  - platform: tsungen3
    tsungen3_id: mx1000
    grid_voltage:
      name: "MX1000 Grid Voltage"
    grid_current:
      name: "MX1000 Grid Current"
    grid_frequency:
      name: "MX1000 Grid Frequency"
    temperature:
      name: "MX1000 Temperature"
    rated_power:
      name: "MX1000 Rated Power"
    current_power:
      name: "MX1000 Current Power"
    ac_daily_energy:
      name: "MX1000 AC Daily Energy"
    ac_total_energy:
      name: "MX1000 AC Total Energy"
    pv1_voltage:
      name: "MX1000 PV1 Voltage"
    pv1_current:
      name: "MX1000 PV1 Current"
    pv1_power:
      name: "MX1000 PV1 Power"
    pv2_voltage:
      name: "MX1000 PV2 Voltage"
    pv2_current:
      name: "MX1000 PV2 Current"
    pv2_power:
      name: "MX1000 PV2 Power"

text_sensor:
  - platform: tsungen3
    tsungen3_id: mx1000
    inverter_status:
      name: "MX1000 Inverter Status"
    event_alarms:
      name: "MX1000 Event Alarms"
    event_faults:
      name: "MX1000 Event Faults"
```

## Directory layout

```
tsungen3/
├── __init__.py          # hub config schema + codegen
├── tsungen3.h/.cpp       # hub: TCP client, Solarman V5 framing, Modbus CRC/parsing
├── sensor/
│   └── __init__.py
├── text_sensor/
│   └── __init__.py
└── README.md
```
