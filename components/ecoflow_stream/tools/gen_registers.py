#!/usr/bin/env python3
"""Generate ../registers.py from the register table of the official EcoFlow Home Assistant integration.

Usage:
    python3 -I gen_registers.py <path to ha-ecoflow-official> [source-version]

(https://github.com/ecoflow-oasis-official/ha-ecoflow-official, MIT). registers_schema.py is only PARSED (ast), never
imported or executed.

Modbus mapping: holding registers, function code 03 (read), 06 / 16 (write), address on the wire = offset + 40001,
32 bit values HIGH WORD FIRST, strings ASCII with the first character in the high byte. Regions "S*" hold the values
of the whole system: they are only meaningful on the system master.
"""
import ast
import pprint
import sys
from pathlib import Path

ADDRESS_OFFSET = 40001
HEARTBEAT_OFFSET = 0x025F  # written by the hub, not an entity

MODELS = {"stream5000": "STREAM 5000", "stream_ac5000": "STREAM AC 5000"}

VTYPES = {"UINT16": ("U_WORD", 1), "INT16": ("S_WORD", 1), "UINT32": ("U_DWORD", 2), "FLOAT": ("FP32", 2),
          "STRING": ("RAW", 8)}


def S(platform, key, **kw):
    return (platform, key, kw)


# offset -> (platform, "yaml.path", extras). Layout of the author's other battery components: dc_channels: / ac: /
# battery:, indexed maps starting at 0, device level entities flat.
MAP = {
    0x0001: S("text_sensor", "system_phase_type", kind="states", states={1: "Three-phase", 2: "Single-phase"}),
    0x0003: S("text_sensor", "serial_number", kind="string", icon="mdi:identifier"),
    0x000B: S("text_sensor", "firmware_version", kind="version"),
    0x000E: S("text_sensor", "manufacturer", kind="string"),
    0x0016: S("sensor", "ac.inverter_power", icon="mdi:home-lightning-bolt"),
    0x0201: S("sensor", "utc_time", icon="mdi:clock-outline", category="diagnostic"),
    0x0205: S("sensor", "time_zone", icon="mdi:map-clock-outline", category="diagnostic"),
    0x0206: S("sensor", "ac.home_power", icon="mdi:home-lightning-bolt"),
    0x0208: S("sensor", "ac.grid_power", icon="mdi:transmission-tower"),
    0x020A: S("sensor", "dc_channels.power", icon="mdi:solar-panel"),
    0x020C: S("sensor", "battery.power", icon="mdi:battery-charging"),
    0x020E: S("sensor", "battery.soc", device_class="battery", icon="mdi:battery"),
    0x0217: S("number", "battery.backup_reserve", min=0, max=100, icon="mdi:battery-lock"),
    0x021B: S("number", "breaker_capacity", min=0, max=120, icon="mdi:electric-switch", category="config"),
    0x021C: S("number", "led_brightness", min=0, max=100, icon="mdi:brightness-6", category="config"),
    0x0221: S("sensor", "ac.max_output_power", icon="mdi:flash", category="diagnostic"),
    0x0223: S("sensor", "ac.max_input_power", icon="mdi:flash", category="diagnostic"),
    0x0227: S("sensor", "battery.full_capacity", icon="mdi:battery-high", category="diagnostic"),
    0x0251: S("sensor", "ac.frequency", icon="mdi:sine-wave"),
    0x0800: S("sensor", "system_fault_count", icon="mdi:alert-circle-outline", category="diagnostic"),
    0x0820: S("sensor", "battery.pack_count", icon="mdi:counter", category="diagnostic"),
    0x0870: S("sensor", "ac.grid_import_energy", icon="mdi:transmission-tower-export"),
    0x0880: S("sensor", "ac.grid_export_energy", icon="mdi:transmission-tower-import"),
    0x08B0: S("sensor", "battery.charge_energy", icon="mdi:battery-plus"),
    0x08C0: S("sensor", "battery.discharge_energy", icon="mdi:battery-minus"),
    0x08D0: S("sensor", "dc_channels.energy", icon="mdi:solar-power-variant"),
    0x1000: S("sensor", "system_device_count", icon="mdi:counter", category="diagnostic"),
    0x1001: S("binary_sensor", "system_master", icon="mdi:crown-outline", category="diagnostic"),
    0x1002: S("sensor", "system_online_device_count", icon="mdi:counter", category="diagnostic"),
    0x1003: S("number", "host_election_priority", min=0, max=65535, icon="mdi:sort-numeric-descending",
              category="config"),
    0x1004: S("text_sensor", "system_group_id", kind="decimal", icon="mdi:identifier"),
    0x1200: S("sensor", "ac.home_energy", icon="mdi:home-lightning-bolt"),
    0x1400: S("sensor", "battery.charge_capability", icon="mdi:battery-arrow-up", category="diagnostic"),
    0x1401: S("sensor", "battery.discharge_capability", icon="mdi:battery-arrow-down", category="diagnostic"),
    0x1610: S("select", "control_point", icon="mdi:tune-variant",
              options={"None": 0, "Battery point": 1, "Meter point": 2, "Inverter point": 3, "Grid port point": 4}),
    0x2010: S("sensor", "device_fault_count", icon="mdi:alert-circle-outline", category="diagnostic"),
    0x2200: S("number", "battery.soc_lower_limit", min=0, max=100, icon="mdi:battery-arrow-down"),
    0x2201: S("number", "battery.soc_upper_limit", min=0, max=100, icon="mdi:battery-arrow-up"),
    0x2202: S("sensor", "battery.min_cell_temperature"),
    0x2204: S("sensor", "battery.max_cell_temperature"),
    0x2206: S("sensor", "battery.max_cell_voltage"),
    0x2207: S("sensor", "battery.min_cell_voltage"),
    0x2400: S("binary_sensor", "on_grid", icon="mdi:transmission-tower"),
    0x2401: S("sensor", "ac.bypass_power", icon="mdi:power-socket-eu"),
    0x2403: S("switch", "bypass_ac1", icon="mdi:power-socket-eu"),
}
for i in range(3):
    MAP[0x0821 + i] = S("sensor", f"battery.packs.pack{i}.soc", device_class="battery", icon="mdi:battery")
    MAP[0x2300 + i] = S("sensor", f"battery.packs.pack{i}.soh", icon="mdi:battery-heart-variant")
    MAP[0x2310 + 8 * i] = S("text_sensor", f"battery.packs.pack{i}.serial", kind="string", icon="mdi:identifier")
    MAP[0x1300 + 2 * i] = S("sensor", f"ac.phases.phase{i}.power", icon="mdi:flash")
for i in range(4):
    MAP[0x2104 + 2 * i] = S("sensor", f"dc_channels.pv{i}.power", icon="mdi:solar-panel")
for i, name in enumerate(["battery_charge", "battery_discharge", "meter_input", "meter_output", "inverter_output",
                          "inverter_input", "grid_port_output", "grid_port_input"]):
    MAP[0x1600 + 2 * i] = S("number", f"limits.{name}_power", min=0, max=100000, icon="mdi:speedometer")
for i, name in enumerate(["battery", "meter", "inverter", "grid_port"]):
    MAP[0x1611 + i] = S("number", f"targets.{name}_power", min=-32768, max=32767, icon="mdi:target")

# Registers merged into one text sensor: first offset -> (key, number of registers)
GROUPS = {0x0801: ("system_fault_codes", 10), 0x2011: ("device_fault_codes", 10)}

SLOW_PLATFORMS = {"number", "select", "switch"}
SLOW_KEYS = {"utc_time", "time_zone", "serial_number", "firmware_version", "manufacturer", "system_phase_type",
             "system_group_id", "system_device_count", "system_online_device_count"}
SLOW_LEAVES = {"serial", "soh", "pack_count", "full_capacity", "max_output_power", "max_input_power"}


def presentation(unit, spec):
    out = {}
    if unit == "W":
        out = dict(unit="W", device_class="power", state_class="measurement", precision=0)
    elif unit == "KWH":
        out = dict(unit="kWh", device_class="energy", state_class="total_increasing", precision=2)
    elif unit == "WH":
        out = dict(unit="Wh", device_class="energy_storage", state_class="measurement", precision=0)
    elif unit == "Hz":
        out = dict(unit="Hz", device_class="frequency", state_class="measurement", precision=2)
    elif unit == "℃":
        out = dict(unit="°C", device_class="temperature", state_class="measurement", precision=1,
                   icon="mdi:thermometer")
    elif unit == "mV":
        out = dict(unit="mV", device_class="voltage", state_class="measurement", precision=0, icon="mdi:sine-wave")
    elif unit == "A":
        out = dict(unit="A", device_class="current", precision=0)
    elif unit == "%":
        out = dict(unit="%", state_class="measurement", precision=0)
    return out


def build(model_name, source):
    specs = {p: {} for p in ("sensor", "text_sensor", "binary_sensor", "switch", "number", "select")}
    regs = {r["offset"]: r for r in source if model_name in r["products"]}
    grouped = {o + i for o, (_k, n) in GROUPS.items() for i in range(n)}
    unmapped = sorted(set(regs) - set(MAP) - grouped - {HEARTBEAT_OFFSET})
    if unmapped:
        raise SystemExit(f"{model_name}: registers without a YAML name: {[hex(o) for o in unmapped]}")

    def add(platform, key, offset, count, vtype, region, extra):
        path = key.split(".")
        spec = dict(register=offset + ADDRESS_OFFSET, count=count, vtype=vtype)
        spec.update(extra)
        spec["system"] = region.startswith("S")
        slow = platform in SLOW_PLATFORMS or key in SLOW_KEYS or path[-1] in SLOW_LEAVES
        spec["scan"] = "low" if slow else "high"
        spec["path"] = path
        specs[platform][key] = spec

    for offset, reg in sorted(regs.items()):
        if offset == HEARTBEAT_OFFSET or offset in grouped:
            continue
        platform, key, extra = MAP[offset]
        writable = "W" in reg["permission"]
        if writable != (platform in SLOW_PLATFORMS):
            raise SystemExit(f"{key}: permission {reg['permission']} does not match the platform {platform}")
        vtype, count = VTYPES[reg["dtype"]]
        merged = presentation(reg["unit"], extra)
        merged.update(extra)
        if platform in ("sensor", "number"):
            merged.setdefault("icon", "mdi:information-outline")
        if platform == "sensor":
            merged.setdefault("precision", 0)
            if merged.get("category") == "diagnostic" and merged.get("unit") is None:
                merged.pop("state_class", None)
        if platform == "number":
            merged.pop("state_class", None)
            merged.pop("precision", None)
            merged.setdefault("category", None)
            merged["step"] = 1
        if platform == "text_sensor":
            merged.setdefault("icon", "mdi:information-outline")
            merged.setdefault("category", "diagnostic")
        add(platform, key, offset, count, vtype, reg["region"], merged)
    for offset, (key, count) in GROUPS.items():
        if all(offset + i in regs for i in range(count)):
            add("text_sensor", key, offset, count, "RAW", regs[offset]["region"],
                dict(kind="codes", icon="mdi:alert-outline", category="diagnostic"))
    return specs


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    schema = Path(sys.argv[1]) / "custom_components/ha_ecoflow_official/registers_schema.py"
    version = sys.argv[2] if len(sys.argv) > 2 else "unknown"
    tree = ast.parse(schema.read_text())
    source = next(ast.literal_eval(n.value) for n in tree.body
                  if isinstance(n, ast.Assign) and n.targets[0].id == "REGISTERS")
    registers = {model: {"specs": build(name, source)} for model, name in MODELS.items()}

    out = Path(__file__).resolve().parent.parent / "registers.py"
    header = f'''"""Register tables for the EcoFlow STREAM 5000 / STREAM AC 5000 (GENERATED - do not edit by hand).

Generated by tools/gen_registers.py from
https://github.com/ecoflow-oasis-official/ha-ecoflow-official version {version}
(Copyright (c) EcoFlow, MIT License, see LICENSE.ecoflow).

  register : Modbus address on the wire (holding registers)    count : number of registers
  vtype    : SensorValueType of ESPHome's modbus (32 bit values high word first)
  system   : value of the whole system, only meaningful on the system master
  scan     : polling class, "high" (update_interval) or "low" (the slow controller)
"""

'''
    body = "MODEL_NAMES = " + pprint.pformat(MODELS, sort_dicts=False) + "\n\n"
    body += "REGISTERS = " + pprint.pformat(registers, sort_dicts=False, width=118) + "\n"
    out.write_text(header + body)
    print(f"wrote {out}:", {m: {p: len(s) for p, s in r["specs"].items()} for m, r in registers.items()})


if __name__ == "__main__":
    main()
