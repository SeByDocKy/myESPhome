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

## Scope

- Read-only telemetry via a single Modbus request per poll cycle (registers
  `0x3000`-`0x3029`): grid voltage/current/frequency, temperature, rated &
  current power, PV1-4 voltage/current/power, AC daily/total energy.
- Inverter Status / Event Alarms / Event Faults are exposed as raw `0x____`
  hex strings (`text_sensor`) — the bitmap meanings aren't documented
  upstream, so no decoding is attempted yet.
- **Write path**: `power_percent` (`number` and `output` platforms) writes
  the Output Coefficient register (`0x202C`, function `0x06` Write Single
  Register) to cap the inverter's output as a percent of Rated Power. A
  `reset_tsungen3` `button` sends `AT+Z` ("Re-start module") over the same
  TCP connection, using a different Solarman V5 frame type/sensor-type than
  Modbus polling (see "AT+ command framing" below).
- One TCP connection is opened, used, and closed per transaction (poll, or
  each control action) — no persistent connection.
- `MULTI_CONF` is supported: several `tsungen3:` blocks (one per inverter,
  each with its own IP) can coexist on the same ESP, same as `hmsw`.

### Non-blocking I/O (background FreeRTOS task)

All TCP I/O (DNS resolution, connect, send, recv — up to a 3 s timeout each)
runs on a dedicated FreeRTOS task, never on ESPHome's single-threaded main
loop:

- `update()`, `set_power_percent()` and `send_reset_command()` only enqueue a
  small job and return almost immediately.
- The background task performs the actual Solarman V5 / Modbus RTU
  transaction and posts the result back through a second queue.
- `loop()` drains that result queue on the main thread and is the only place
  that publishes sensor states or logs the outcome, since ESPHome's
  Sensor/TextSensor/API objects must only be touched from the main thread.
- If the background task is still busy with a previous job when a new one is
  enqueued (job queue full), the new one is dropped with a `WARN` log rather
  than blocking or queuing indefinitely — this only happens if a transaction
  is taking unusually long (e.g. the inverter is slow/unreachable).

Practical effect: `power_percent`/`reset_tsungen3` are genuinely
fire-and-forget now — the call returns almost instantly, and the actual
success/failure of the write or reset is only visible in the logs (and, for
the `number` platform, the optimistically-published state) a moment later,
not as a blocking round-trip. This was added after real-world testing showed
the main loop time (measured via the `debug` component's `loop_time` sensor)
spiking to ~400 ms per poll cycle with the previous synchronous
implementation, which briefly starved WiFi/API/other components' `loop()`.

### AT+ command framing (button)

Unlike the Modbus read/write path, AT+ commands are **not** Modbus RTU: they
are plain ASCII text (command + `\r`) wrapped directly in the Solarman V5
payload, with `Frame Type = 0x01` (`AT_CMD`) and `Sensor Type = 0x0002`
instead of the `0x02`/`0x0000` used for Modbus polling. This was taken from
the actual proxy source (`gen3plus/solarman_v5.py`'s `send_at_cmd()`/`AT_CMD`
constant), not just the generic pysolarmanv5 spec, since AT+ framing isn't
part of that spec. `AT+Z` ("Re-start module") is documented on the proxy's
wiki; no other AT+ commands are wired up here.

### Confirmed against real hardware (MX1000, Sep 2026)

- **Solarman "Logger Serial" field must be the real "Monitoring SN"** printed
  on the inverter's sticker — `0` (the option's default) does **not** get a
  response in client_mode. Set `logger_serial` explicitly.
- **Modbus slave/unit address `1`** works as documented.
- The read path (framing, both CRCs, register offsets, scaling) is correct:
  `rated_power` read back as exactly `1000.0 W` on a real MX1000 (1000W
  nameplate), with a single PV string wired to the PV2 input reporting
  plausible open-circuit-ish voltage while AC was still disconnected.
- `grid_frequency` reads `50.00 Hz` even with the AC side physically
  disconnected -- apparently a firmware default/quiescent value while
  ungridded, not a decoding bug.
- `inverter_status`/`event_alarms` were non-zero (`0x0002`/`0x0100`) with AC
  disconnected, consistent with a "no grid" condition, but still undecoded
  raw hex (see above).

### Unverified assumptions — please report back if your inverter disagrees

- **32-bit register decoding** (`AC Total Energy` at `0x301d`/`0x301e`):
  implemented as low-word-at-lower-address, matching the tsun-gen3-proxy
  wiki's "uInt32LE" notation, but not checked against a packet capture. Not
  yet confirmed against real hardware (no AC/production data at time of
  writing).
- **Output Coefficient register/scaling** (`0x202C`, ratio 100/1024): taken
  from the wiki's MODBUS register table and the proxy's v0.9.0 release note
  ("inverter-output-coefficient"), not from a packet capture of an actual
  write. **Test at low percentages first** — worst case if the ratio is
  wrong is an incorrect output cap, not a bricked inverter, but it hasn't
  been verified on real hardware.
- **AT+Z's actual effect**: the wiki only says "Re-start module" — whether
  this is a soft restart of the WiFi/monitoring MCU or something more
  disruptive (interrupting power production) is not documented upstream.

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
    logger_serial: 2093984xxx  # required -- the real "Monitoring SN" on the sticker;
                                 # the default (0) gets no response in client_mode
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
    ac_energy_today:
      name: "MX1000 AC Daily Energy"
    ac_energy_total:
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

number:
  - platform: tsungen3
    tsungen3_id: mx1000
    power_percent:
      name: "MX1000 Power Percent"

output:
  - platform: tsungen3
    tsungen3_id: mx1000
    power_percent:
      id: mx1000_power_percent_output

button:
  - platform: tsungen3
    tsungen3_id: mx1000
    reset_tsungen3:
      name: "MX1000 Reset"
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
├── number/
│   └── __init__.py      # power_percent (writes Output Coefficient, 0x202C)
├── output/
│   └── __init__.py      # power_percent (same write, as a FloatOutput)
├── button/
│   └── __init__.py      # reset_tsungen3 (sends AT+Z)
└── README.md
```
