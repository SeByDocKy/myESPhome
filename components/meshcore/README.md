# `meshcore` — ESPHome component for the MeshCore mesh protocol

A native ESPHome external component that speaks the
[MeshCore](https://github.com/meshcore-dev/MeshCore) mesh protocol over
`sx126x`/`sx127x` LoRa radios — built by studying the official firmware
source directly (`src/Packet.h`, `src/Utils.cpp`,
`src/helpers/BaseChatMesh.cpp`, `src/Mesh.cpp`), not just the docs, so the
wire format is interoperable with real MeshCore nodes and apps on the same
channel.

It ships two things:

- **`meshcore:`** — the core component: joins one or more encrypted MeshCore
  group channels, sends/receives text messages, and does basic flood
  relaying.
- **`packet_transport: platform: meshcore`** — a backend for ESPHome's
  standard [`packet_transport`](https://esphome.io/components/packet_transport)
  component, letting you exchange sensors and binary sensors between
  ESPHome devices over a MeshCore channel with almost no custom code.

## Why

MeshCore's own apps/companion firmware are for human chat and repeaters.
This component lets plain ESPHome nodes (sensors, relays, anything) join
the same encrypted channel as a lightweight, battery/duty-cycle-friendly
transport — useful for a remote sensor ↔ base station setup that also
happens to share airtime with a MeshCore mesh, without needing WiFi in
range of every node.

## Scope (v1)

Implemented:
- Encrypted **Group Text Message** (`PAYLOAD_TYPE_GRP_TXT`, `0x05`) —
  human-readable channel chat, visible in MeshCore apps.
- Encrypted **Group Data** (`PAYLOAD_TYPE_GRP_DATA`, `0x06`) — opaque
  binary payloads, used by the `packet_transport` platform; invisible in
  MeshCore chat apps since it isn't text.
- Simple **flood relay** with packet deduplication.

Not implemented (see [`NOTES_PROTOCOLE.md`](./NOTES_PROTOCOLE.md) for the
full breakdown and reasons):
- Direct/private messages (ECDH X25519), Ed25519 node identity, adverts,
  transport-routed packets, multi-byte path hashes, airtime-aware
  retransmit delay.

None of that is needed for a sensor/actuator mesh talking to itself (or to
a MeshCore chat channel) over a shared PSK — which is what this component
targets.

## Requirements

- An ESP32 (tested on ESP32-S3, e.g. Xiao ESP32-S3) with an `sx126x` or
  `sx127x` LoRa radio already configured in your YAML.
- ESP-IDF or Arduino framework — no external crypto library needed (see
  [Cryptography](#cryptography) below).

## Installation

```yaml
external_components:
  - source: "github://SeByDocKy/myESPhome/"
    components: [meshcore]
    refresh: 0s
```

or, for local development:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [meshcore]
```

## Core component: `meshcore:`

```yaml
meshcore:
  - id: id_mesh
    lora: lora_radio          # an existing sx126x/sx127x component
    node_name: "my-node"      # optional, default "esphome-meshcore"
    hop_limit: 3              # optional, default 3
    repeat: true              # optional, default true — relay flood packets
    node_hash: 0x4e           # optional — see note below
    channels:
      - name: "my-channel"
        psk: "izOH6cXN6mrJ5e26oRXNcg=="   # 16 or 32 raw bytes, base64
    on_packet:
      then:
        - lambda: |-
            ESP_LOGI("mesh_rx", "%s on '%s': %s", from_name.c_str(), channel.c_str(), text.c_str());
```

| Key | Type | Default | Description |
|---|---|---|---|
| `id` | ID | — | Component ID, referenced elsewhere as `meshcore_id`. |
| `lora` | ID | — | An existing `sx126x`/`sx127x` component. |
| `node_name` | string | `"esphome-meshcore"` | Sender name prefixed on outgoing `GRP_TXT` messages (`"node_name: text"`). Independent of the ESPHome device name used by `packet_transport` (see below). |
| `hop_limit` | int, 0–63 | `3` | Max relay hops before a flood packet stops being repeated. |
| `repeat` | bool | `true` | Whether this node relays (repeats) flood packets it hears. |
| `node_hash` | hex byte | derived from `node_name` | Byte used to mark this node's presence in a flood packet's path. There's no Ed25519 identity in this v1 (see limitations), so this is a stand-in, not a real node public key. |
| `channels` | list | — | One or more `{name, psk}` channels to join. `psk` is base64 of 16 (AES-128, typical) or 32 raw bytes — see [PSK format](#psk-format-hex-vs-base64). |
| `on_packet` | automation | — | Fires on every decrypted `GRP_TXT` message, with `channel`, `from_name`, `text`, `rssi`, `snr` available in the trigger scope. |

### Action: `meshcore.send_group_text`

```yaml
on_...:
  - meshcore.send_group_text:
      id: id_mesh
      channel: "my-channel"
      text: "hello mesh"
```

Sends `"node_name: text"` as an encrypted `GRP_TXT` broadcast on the given
channel — this is what shows up as a chat message in MeshCore apps.

## `packet_transport: platform: meshcore`

Lets you broadcast local sensors/binary_sensors to other ESPHome nodes,
and receive theirs, reusing ESPHome's standard `packet_transport`
component instead of hand-rolled text parsing. Data travels as
`GRP_DATA` (opaque, not shown in MeshCore chat apps).

```yaml
packet_transport:
  - platform: meshcore
    meshcore_id: id_mesh
    channel: "my-channel"       # required: which meshcore channel to use
    update_interval: 1min
    sensors:                    # what THIS device broadcasts
      - bme280_temperature
      - bme280_humidity
    binary_sensors:
      - relay0_state
    providers:                  # other devices whose data to receive
      - name: other-esphome-device   # must match its esphome.name, see below

sensor:
  - platform: packet_transport
    provider: other-esphome-device
    remote_id: bme280_temperature   # the *sender's* local sensor id
    name: "Remote temperature"
    internal: false                 # required whenever name: is set
```

`meshcore_id` and `channel` are specific to this backend; everything else
(`sensors`, `binary_sensors`, `providers`, `update_interval`,
`rolling_code_enable`, `ping_pong_enable`, ...) is standard
`packet_transport` config — see the
[official docs](https://esphome.io/components/packet_transport) for the
full list.

**Important: `provider:`/`providers: name:` must match the *ESPHome
device name*** (`esphome: name:` — what `App.get_name()` returns), **not**
this component's `node_name:`. `packet_transport` self-identifies senders
by ESPHome device name, entirely independently of `meshcore:`'s own
`node_name:` (which only prefixes `GRP_TXT` chat text). Mixing these two
up is the single most common source of "nothing arrives" when wiring this
up.

### No remote actuation, by design

`packet_transport` only has `sensor` and `binary_sensor` platforms — it's
one-way state *reporting*, not a command/RPC mechanism. There's no
`switch: platform: packet_transport`. To actuate something remotely (e.g.
"master, turn on slave's relay"), model it as data flowing the *other*
direction instead: the controller declares a local `binary_sensor:
platform: template` holding the desired state and lists it under its own
`packet_transport: binary_sensors:`; the target device receives it via
`binary_sensor: platform: packet_transport` and reacts with `on_state:`.
Both directions can run simultaneously on the same channel — a device can
be a `packet_transport` provider and a consumer at once.

### Packet size / splitting

`packet_transport` automatically splits data across multiple `GRP_DATA`
packets if it would exceed this backend's max size (`MeshCore::max_group_data_size()`,
roughly 140 bytes of usable payload after the channel hash, MAC, and AES
padding overhead are accounted for). You don't need to do anything for
this — just be aware that "10 sensors" might mean 2-3 packets per update
cycle, not one, which adds airtime and a small chance of one chunk being
missed if it lands while the radio is mid-transmit elsewhere on a busy
channel.

## Cryptography

AES-128-ECB + HMAC-SHA256 (truncated to 2 bytes), matching MeshCore's own
`Utils::encryptThenMAC`/`Utils::MACThenDecrypt` exactly, including how the
channel secret and channel hash are derived from the PSK
(`BaseChatMesh::addChannel`). The implementation (`aes_sha256.h`) is
**self-contained — no mbedtls dependency**. An earlier version used
`mbedtls_aes_*`/`mbedtls_md_hmac_*`, but on a minimal ESP-IDF project
(no `api:`, no HTTPS OTA) nothing else forces the ESP-IDF Kconfig to
compile mbedtls's AES module in, causing an `undefined reference to
mbedtls_aes_*` link error. Being self-contained (matching what the
official MeshCore firmware itself does — it doesn't use mbedtls either)
avoids that whole class of project-dependent link failures.

Validated against FIPS-197 (AES-128), NIST and RFC 4231 (SHA-256 /
HMAC-SHA256) test vectors, and cross-checked against real captured
MeshCore packets.

The AES-128-ECB + MAC-on-ciphertext construction is a known, documented
limitation of the MeshCore protocol itself (see
[meshcore-dev/MeshCore#259](https://github.com/meshcore-dev/MeshCore/issues/259)) —
this component reproduces it as-is for on-air compatibility, it isn't a
weakening introduced here.

## PSK format: hex vs base64

MeshCore apps typically ask for the channel secret key in **hex**;
this component's `psk:` expects **base64** (same bytes, different text
encoding). A tiny converter:

```python
import base64, sys
raw = bytes.fromhex(sys.argv[1])
print("hex   :", raw.hex())
print("base64:", base64.b64encode(raw).decode())
```

## Radio parameters (EU868 example)

Matches the official MeshCore firmware's own defaults (verified against
`platformio.ini` and `variants/heltec_v3/platformio.ini` in the MeshCore
source), for the EU 869.4–869.65 MHz high-power sub-band:

```yaml
sx126x:
  id: lora_radio
  frequency: 869618000
  bandwidth: 62_5kHz
  spreading_factor: 8
  coding_rate: CR_4_8   # not CR_4_5 — check your app's selected radio preset
  pa_power: 22
  sync_value: [0x14, 0x24]
  preamble_size: 16
  tcxo_voltage: 1_8V
  tcxo_delay: 5ms
  rf_switch: true
  crc_enable: true
  modulation: LORA
  hw_version: sx1262
```

Radio parameters aren't universal — different MeshCore communities use
different presets (e.g. some regions favor `SF7` over `SF8` at the same
frequency to reduce congestion). Check what your target network actually
uses (visible in the app's channel/radio settings) rather than assuming
one profile fits everywhere.

## Debugging tips

- `logger: level: debug` shows this component's own traffic
  (`Envoi GRP_TXT/GRP_DATA...`, `Paquet RX brut...`) and MAC/decode
  failures.
- `logger: level: verbose` additionally shows `packet_transport`'s *own*
  receive-side diagnostics (`Found hostname ...`, `Got sensor ...`,
  `Unknown hostname ...`, `Bad payload length ...`) — these are logged at
  `V`/`VV` in the upstream component, so a packet can be arriving and
  decrypting fine while still being silently dropped one layer up
  (wrong `provider:`/ESPHome device name is the usual cause) without
  anything showing at `debug` level.
- If nothing seems to transmit/receive at all, temporarily point a second
  device at MeshCore's built-in default `"Public"` channel
  (`izOH6cXN6mrJ5e26oRXNcg==`) to rule out radio/antenna issues
  independently of your own channel's PSK.
- A `HW Version: SX1261 ...` log line on hardware known to have an SX1262
  chip is a known, usually harmless RadioLib/ESP-IDF chip-detection quirk
  — not necessarily a real hardware mismatch.

## License / credits

Modeled on the structure of the
[esphome-meshtastic](https://github.com/Andrik45719/esphome-meshtastic)
component. Protocol details verified directly against the official
[MeshCore](https://github.com/meshcore-dev/MeshCore) firmware source.
