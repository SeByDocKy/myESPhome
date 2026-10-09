# `jackerysv3` — Jackery SolarVault 3 native ESPHome component

Local, cloud-free monitoring and control of a **Jackery SolarVault 3** AC-coupled battery from an ESP32, with
**no external MQTT broker and no Home Assistant MQTT integration**.

The SolarVault 3 is an MQTT *client*: the Jackery app lets you point it at your own MQTT broker. The
[`mqtt_broker`](../mqtt_broker/README.md) component runs a small MQTT broker **on the ESP32 itself**; this
component (pointed to it with `mqtt_broker_id:`) decodes the Jackery JSON protocol that flows through it and
exposes the result as native ESPHome entities (sensors, switches, numbers ...) through the ESPHome API:

```
 Jackery SolarVault 3  <--- MQTT (JSON) --->  mqtt_broker (ESP32)  <--- jackerysv3 --->  ESPHome API / Home Assistant
        (client)
```

> **Developed blind.** The author does not own the hardware. The protocol was reverse-read from the official
> Home Assistant integration, and the decoder was checked against it with sample payloads (see
> [Caveats](#caveats-please-verify-on-your-hardware)). Please run the first connection with
> `log_traffic: true` and report what you see.

Protocol reference: <https://github.com/Jackery-Official/jackery> (`custom_components/jackery/*.py`, `README.md`,
`HA Entity List.md`).

## Supported devices

- **Jackery SolarVault 3** (host, "DIY3" in the integration) with the Jackery app **> 2.0.0**
  (the *Device Details → Settings → MQTT* page is required).
- Optional sub-devices reported through the host: **Smart CT / smart meter** (Shelly, Eastron, Jackery meters)
  and up to **10 smart plugs**.

Only one battery is needed per `jackerysv3:` block; several batteries can be handled by one ESP32 (see
[Multiple batteries](#multiple-batteries)).

## How it works

- The `mqtt_broker` component listens on a TCP port (default `1883`). In the Jackery app you configure the
  address of the ESP32 as the MQTT server. The battery connects as an MQTT client.
- Topics (default prefix `hb`, `<sn>` = the serial number of the battery):

  | Topic | Direction | Content |
  |---|---|---|
  | `hb/device/<sn>/status` | battery → ESP | status reports (types 23, 25, 106 ...) |
  | `hb/device/<sn>/event` | battery → ESP | incremental reports (types 101, 102, 107) and sub-device lists |
  | `hb/device/<sn>/action` | ESP → battery | poll requests and commands |

- Every `poll_interval` (default 5 s) the component sends the same requests as the official integration:
  type 25 (device status), 105 (system data), 2 (all settings) and 100 for `devType` 2 (CT) and 6 (plugs).
  The battery answers on `status` / `event`.
- The reports are merged into one state exactly like the official integration does (same aliases, same merge
  rules). Values the battery does not report are derived with the **same formulas as the Jackery app / HA
  integration** (net grid power, home load, AC socket power, net battery power).
- Commands (`number`, `select`, `switch`, `button`) are JSON messages on the `action` topic:
  `{"type":1,"eventId":3,"body":{"cmd":5,"rc":1,"<field>":<value>}}` for the battery and `type: 103` for plugs.
  The new state is shown optimistically and corrected by the next report.
- When nothing is received for `offline_timeout` (default 60 s) all values become `unknown` (`NaN`), the
  `status` text sensor shows `Offline` and `online` turns off. Sub-devices (CT, plugs) go stale on their own.
- The broker is a deliberately small MQTT 3.1 / 3.1.1 / 5.0 subset, documented in the
  [`mqtt_broker` README](../mqtt_broker/README.md) (QoS 0/1/2 accepted and forwarded at QoS 0, `+` / `#`
  wildcards, keep-alive, will messages, optional user / password, no TLS, no retained messages).

## Before you start: set up the Jackery app

1. In the Jackery app: **Device Details → Settings → MQTT**.
2. Enter the **IP address of the ESP32**, the **port** (`1883` by default) and a user name / password, then
   enable it. Without `username` / `password` on the `mqtt_broker` the broker accepts **any** credentials, so
   whatever the app requires is fine. If you want authentication, put the same values on the `mqtt_broker`.
3. Note the **token** shown on that page (`token:`) and the **serial number** of the battery (`sn:`).
4. Give the ESP32 a fixed IP (DHCP reservation).

The battery **silently ignores** requests carrying a wrong token. If nothing is received for 120 s after boot
the component logs a hint (wrong token / serial number / topic prefix, or the app does not point to the ESP).

## Configuration

```yaml
mqtt_broker:                   # see the mqtt_broker README (port, credentials, max_clients, log_traffic ...)
  id: broker
  port: 1883

jackerysv3:
  id: jackery
  mqtt_broker_id: broker       # the broker the battery connects to (optional when there is a single one)
  sn: "SV3XXXXXXXXXXXX"        # required, serial number of the battery
  token: "xxxxxxxxxxxxxxxx"    # token shown in the Jackery app (MQTT page)
  topic_prefix: hb             # default "hb"
  poll_interval: 5s            # default 5s (1s .. 10min)
  offline_timeout: 60s         # default 60s
  # plug_sns: ["PLUG0SN", "PLUG1SN"]   # optional: pin plug slots plug0, plug1 ... to serial numbers
```

| Option | Default | Description |
|---|---|---|
| `mqtt_broker_id` | *auto* | The [`mqtt_broker`](../mqtt_broker/README.md) the battery connects to. Port, credentials, `max_clients`, `max_packet_size` and `log_traffic` are options of the broker. |
| `sn` | *required* | Serial number of the battery (used in the topics). |
| `token` | `""` | Token sent in every request. |
| `topic_prefix` | `hb` | Topics are `<prefix>/device/<sn>/status\|event\|action`. |
| `plug_sns` | none | Up to 10 serial numbers pinning the plug slots. Without it plugs fill `plug0`, `plug1` ... in discovery order. |

The component is `MULTI_CONF` and ESP32 only (ESP-IDF). A **time source** (`time:` with `sntp`) is
recommended because the requests carry a timestamp.

### Platforms

Every platform takes `jackerysv3_id:` (optional when there is a single hub). All entities are optional; the
indices of PV channels and plugs start at **0**.

#### `sensor`

```yaml
sensor:
  - platform: jackerysv3
    jackerysv3_id: jackery
    status_code: {name: "Status Code"}        # diagnostic, raw `stat`
    work_mode_code: {name: "Work Mode Code"}  # diagnostic, raw `workMode`
    dc_channels:
      power:  {name: "Solar Power"}           # total PV power
      energy: {name: "Solar Energy"}          # total PV energy
      pv0:
        power:  {name: "PV0 Power"}
        energy: {name: "PV0 Energy"}
      # pv1 .. pv3 likewise (4 channels)
    ac:
      grid_power:           {name: "Grid Power"}           # net, + = import, - = export (calculated)
      grid_input_power:     {name: "Grid Input Power"}
      grid_output_power:    {name: "Grid Output Power"}
      grid_input_energy:    {name: "Grid Input Energy"}
      grid_output_energy:   {name: "Grid Output Energy"}
      home_power:           {name: "Home Power"}           # calculated
      other_load_power:     {name: "Other Load Power"}
      max_feed_in_power:    {name: "Max Feed-in Power"}
      socket_power:         {name: "Socket Power"}         # AC (EPS) socket, calculated
      socket_input_power:   {name: "Socket Input Power"}
      socket_input_energy:  {name: "Socket Input Energy"}
      socket_output_energy: {name: "Socket Output Energy"}
    battery:
      soc:              {name: "Battery SOC"}
      average_soc:      {name: "Battery Average SOC"}
      temperature:      {name: "Battery Temperature"}
      pack_count:       {name: "Battery Pack Count"}
      net_power:        {name: "Battery Net Power"}        # + = charging, - = discharging
      charge_power:     {name: "Battery Charge Power"}
      discharge_power:  {name: "Battery Discharge Power"}
      charge_energy:    {name: "Battery Charge Energy"}
      discharge_energy: {name: "Battery Discharge Energy"}
    energy_flows:                                          # lifetime counters of the battery, kWh
      pv_to_battery:     {name: "PV to Battery Energy"}
      pv_to_socket:      {name: "PV to Socket Energy"}
      pv_to_grid:        {name: "PV to Grid Energy"}
      grid_to_battery:   {name: "Grid to Battery Energy"}
      grid_to_socket:    {name: "Grid to Socket Energy"}
      battery_to_socket: {name: "Battery to Socket Energy"}
      battery_to_grid:   {name: "Battery to Grid Energy"}
      socket_to_battery: {name: "Socket to Battery Energy"}
      socket_to_grid:    {name: "Socket to Grid Energy"}
    ct:                                                    # smart meter, when one is paired
      forward_power:         {name: "CT Forward Power"}
      reverse_power:         {name: "CT Reverse Power"}
      forward_energy:        {name: "CT Forward Energy"}
      reverse_energy:        {name: "CT Reverse Energy"}
      phase_a_forward_power: {name: "CT Phase A Forward Power"}   # b, c likewise
      phase_a_reverse_power: {name: "CT Phase A Reverse Power"}   # b, c likewise
    plugs:                                                 # smart plugs, 0-indexed slots (plug0 .. plug9)
      plug0:
        power:  {name: "Plug0 Power"}
        energy: {name: "Plug0 Energy"}
```

Notes:
- Energy values are the battery's counters (`raw × 0.01` kWh, `total_increasing`); the temperature is
  `raw × 0.1` °C.
- `battery.packs:` is **not offered**: the protocol only carries `batNum`, `batSoc` and `soc` (no per-pack
  data). Use `battery.pack_count`, `battery.soc` and `battery.average_soc`.
- PV channels not present on your unit (`pv1` .. `pv3`) simply stay `unknown`.

#### `binary_sensor`

| Key | Source |
|---|---|
| `online` | The battery reported within `offline_timeout`. |
| `client_connected` | At least one MQTT client is connected to the embedded broker. |
| `on_grid` | `ongridStat` |
| `ct_online` | `ctStat` |
| `grid_meter_link` | `gridSate` |
| `socket_ok` | `swEpsState` |

#### `text_sensor`

`status` (`Normal`, `Waiting`, `Alarm`, `Fault`, `Standby`, `Low power`, `Offline`), `work_mode`
(`Self-consumption`, `Battery priority`, `User-defined`, `TOU`, `Feed-in priority`, `Dynamic pricing` ...),
`firmware_version`, `model`, `ct_type`.

#### `switch`

```yaml
switch:
  - platform: jackerysv3
    jackerysv3_id: jackery
    ac_socket:            {name: "AC Socket"}              # swEps
    auto_standby_allowed: {name: "Auto Standby Allowed"}   # isAutoStandby
    plugs:
      plug0: {name: "Plug0"}                               # smart plug 0 (type 103)
```

A plug can only be switched when it is connected locally (`commMode` 1). Plugs connected through the Jackery
cloud (`commMode` 2) refuse MQTT commands (a warning is logged, as the official integration does).

#### `number`

| Key | Range | Field |
|---|---|---|
| `soc_charge_limit` | 50 – 100 % | `socChgLimit` |
| `soc_discharge_limit` | 5 – 49 % | `socDischgLimit` |
| `max_output_power` | 0 – 2500 W (step 10) | `maxOutPw` |

The limits reported by the battery (`minSocChg`, `maxSocChg`, `minSocDischg`, `maxSocDischg`) narrow the
slider at run time.

#### `select`

`auto_standby_mode`: `Invalid` / `Standby` / `On` (`autoStandby` 0 / 1 / 2).

#### `button`

`reboot`: sends `{"cmd":5,"rc":1,"reboot":1}`.

#### `output`

Float output (0.0 .. 1.0) for control loops (PID, zero injection ...): `0.0` maps to 0 W and `1.0` to 2500 W
(step 10 W); the limits reported by the battery narrow the range. The command is only sent when the integer
value changes (and repeated every 30 s otherwise).

```yaml
output:
  - platform: jackerysv3
    jackerysv3_id: jackery
    max_output_power:            # 0.0 -> 0 W, 1.0 -> 2500 W: maximum power of the grid-tied port
      id: jackery_max_output_power
```

#### What can be controlled (and what cannot)

The protocol, as used by the official integration, only exposes these writable settings: maximum output power
(`maxOutPw`), SOC charge / discharge limits (`socChgLimit`, `socDischgLimit`), AC socket (`swEps`), auto standby
(`isAutoStandby`, `autoStandby`), reboot and the smart plugs. There is **no charge / discharge power setpoint**
(unlike e.g. a Marstek or Zendure): the battery charges and discharges by itself according to its work mode.
What you can do from ESPHome is limit the grid output power (`max_output_power`, which caps the discharge /
feed-in, as a `number` or an `output`) and set the SOC limits (`number` platform; `soc_charge_limit` stops
charging at the chosen level). Changing the work mode
is not available through this protocol either (`work_mode` is read-only).

## Full example

See [`test_jackerysv3.yaml`](test_jackerysv3.yaml) for a complete configuration with every entity.

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [jackerysv3, mqtt_broker]

time:
  - platform: sntp

mqtt_broker:
  log_traffic: true            # remove once everything works

jackerysv3:
  sn: "SV3XXXXXXXXXXXX"
  token: "xxxxxxxxxxxxxxxx"

sensor:
  - platform: jackerysv3
    dc_channels:
      power: {name: "Solar Power"}
    ac:
      grid_power: {name: "Grid Power"}
      home_power: {name: "Home Power"}
    battery:
      soc: {name: "Battery SOC"}
      net_power: {name: "Battery Net Power"}

binary_sensor:
  - platform: jackerysv3
    online: {name: "Jackery Online"}
```

### Multiple batteries

```yaml
mqtt_broker:
  id: broker

jackerysv3:
  - id: jackery_1
    mqtt_broker_id: broker
    sn: "SV3XXXXXXXX1"
    token: "..."
  - id: jackery_2
    mqtt_broker_id: broker
    sn: "SV3XXXXXXXX2"
    token: "..."
```

Hubs pointing to the same `mqtt_broker` share it (all batteries connect to the same ESP address and port).
Add `jackerysv3_id:` to every platform entry. Hubs may also use several `mqtt_broker` blocks on different ports.

## Caveats (please verify on your hardware)

- **Blind development.** Nothing was run against a real SolarVault 3. The official integration does not tell
  which MQTT QoS, keep-alive, protocol version, client id or TLS setting the battery uses. The broker is
  therefore permissive (MQTT 3.1 / 3.1.1 / 5, QoS 0-2, any client id, any keep-alive). **TLS is not supported**:
  if the app only offers a TLS connection, this component cannot serve it.
- **First connection.** Set `log_traffic: true` on the `mqtt_broker` and look at the log: you should see a `CONNECT`, a
  `SUBSCRIBE` to `hb/device/<sn>/action`, then `PUBLISH` messages on `status` / `event`. If you see something
  else (another prefix, another serial format), adjust `topic_prefix` / `sn` and please report it.
- **Wrong token = silence.** The battery does not answer when it rejects the token; the only symptom is the
  "no report" hint in the log. `type 123 / errorCode 401` messages, when sent, are logged as a token warning.
- **Broker limits.** Only QoS 0 is forwarded, nothing is retained, no TLS (see the `mqtt_broker` README).
  Sockets: the listening socket plus `max_clients + 1` are reserved by the broker.
- **One broker at a time.** The battery connects to one broker only: the ESP replaces the one used by the
  official Home Assistant integration (do not run both).
- **No per-pack data** in the protocol (see `sensor` notes).
- **Calculated values** (`grid_power`, `home_power`, `socket_power`, `battery.net_power` when `batInPw` /
  `batOutPw` are missing) reproduce the formulas of the official integration; they were checked against its
  Python code on randomised inputs (12 000 cases, identical results). They rely on the same assumptions as
  the app (CT preferred over the system estimate, etc.).
- **Timestamp.** The requests carry `ts`; add a `time:` component so it is a real epoch. The battery may or
  may not check it. Without a valid clock the component waits up to 60 s after boot before sending its first
  requests, then sends them anyway and logs a warning.
- **Topic spelling.** Topics are matched case-insensitively; requests are published on the exact spelling the
  battery itself uses (learned from its first message or subscription), so a serial number typed in another
  case in the YAML does not silence the battery.

## Troubleshooting

Set `logger: level: DEBUG` and `log_traffic: true` on the `mqtt_broker`, then look for:

- `The battery subscribed to ...`: the battery is connected and listens; the component polls it right away.
- `No MQTT client is subscribed to ...`: the battery is not connected, or uses another topic prefix / serial number.
- `The battery receives our requests (...) but has not reported anything for N s`: wrong `token`, or the
  battery rejects the request timestamp (add a `time:` component).
- `Summary: ...` (every 60 s, DEBUG): messages received, request bursts delivered, clients, online state.

When reporting a problem please include this log: it shows what the battery sends and how it behaves.
