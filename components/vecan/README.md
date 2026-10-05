# vecan

Native ESPHome hub for **VE.Can**, Victron's flavour of NMEA 2000 (CAN 2.0B, 250 kbit/s, 29-bit identifiers).
It sits on top of any ESPHome `canbus` platform (`mcp2515`, `mcp2518fd`, ...) and gives device components
(`smartsolar`, later `multirs`, ...) what they need to talk to Victron products:

- ISO 11783-5 **address claim** (with contention handling) so the ESP32 is a well-behaved node on the bus
- a paced **transmit queue** (no request bursts on the bus)
- Victron **VREG** framing: register requests, ACK/NACK handling, single frames *and* NMEA 2000 fast packets
- dispatch of standard PGNs and VREG values to the device component bound to a given source address
- discovery: every Victron node that announces itself is logged with its address

## Configuration

```yaml
canbus:
  - platform: mcp2515
    id: can_bus
    cs_pin: GPIO5
    can_id: 0              # required by the canbus schema, unused by vecan
    use_extended_id: true  # VE.Can uses 29-bit identifiers
    bit_rate: 250KBPS
    clock: 8MHZ            # crystal of the MCP2515 module
    mode: LISTENONLY       # NORMAL when vecan may transmit

vecan:
  id: vecan_hub
  canbus_id: can_bus
  address: 0xA0            # source address we try to claim (default 0xA0)
  listen_only: false       # true = never transmit (no address claim, no requests)
  tx_interval: 20ms        # minimum spacing between two transmitted frames
```

| Option        | Default | Description |
|---------------|---------|-------------|
| `canbus_id`   | —       | The `canbus` component to use (required). |
| `address`     | `0xA0`  | Preferred source address. If another node already owns it with a higher priority, the next free address (128..247) is taken. |
| `listen_only` | `false` | Passive sniffing: the hub never transmits, so device components only see broadcast values. Pair it with `mode: LISTENONLY` on the MCP2515. |
| `tx_interval` | `20ms`  | Minimum delay between two frames sent by the hub. |

Tip: the `canbus` component logs every frame at `DEBUG` level. Keep `logs: canbus: INFO` unless you are debugging.

## What it does on the bus

1. 500 ms after boot it sends an ISO request for address claims, then listens for 1.5 s (this is also how Victron
   devices get discovered: `Victron device found at address 0x20`).
2. It claims its address and waits 250 ms before transmitting anything else.
3. Device components ask for registers with `request_vreg(dst, reg)` and change them with `write_vreg(dst, reg, value)`;
   frames are queued (writes before reads) and sent one at a time. The answer (or the confirmation of a write) is
   broadcast by the device, as described in Victron's public register document; a refusal is a NACK.

The NAME used for the address claim carries the "not registered" manufacturer code (2047), a unique identity derived
from the MAC address and the marine industry group.

## Writing a device component

```cpp
class MyDevice : public Component, public vecan::VeCanDevice {
  void setup() override { this->hub_->register_device(this); }
  void on_pgn(uint32_t pgn, const uint8_t *data, uint8_t len) override;      // standard PGNs
  void on_vreg(uint16_t reg, const uint8_t *data, uint16_t len) override;    // VREG values
  void on_vreg_nack(uint16_t reg, uint16_t code) override;                   // refused requests / writes
};
```

Only frames whose source address equals `set_address()` are forwarded. `vecan_proto.h` has no ESPHome dependency
(ID packing, VREG frames, fast-packet reassembly, decoders) and is unit-tested with plain `g++`, see `tests/`.

## Protocol notes and limits

- Sources: Victron's public *VE.Can registers* document (v20, 2015) and the *Data communication with Victron Energy
  products* white paper. Victron does not publish a register list per product: which registers a given device implements
  has to be checked on the bus (a device answers `0x8000` to a register it does not have).
- Register writes are supported with `write_vreg(dst, reg, value)` (single frame, up to 4 data bytes): the hub only
  frames and paces them, protection against bad values is the job of the device component (see `smartsolar`).
  Fast-packet *transmission* is not implemented.
- Fast packets are reassembled for the proprietary VREG PGN only, one packet in flight per source address, up to 96 bytes.
- A Victron GX device (Cerbo GX) is normally the system master on the bus. The ESP32 mostly reads.
  Do not write settings that the GX also manages (DVCC, charge limits...).
