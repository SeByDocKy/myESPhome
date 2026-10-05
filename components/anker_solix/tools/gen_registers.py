#!/usr/bin/env python3
"""Generate ../registers.py from the device configuration files of the official Anker SOLIX Home Assistant
integration https://github.com/anker-charging/ha-anker-solix-official

Usage:
    python gen_registers.py <path to custom_components/anker_solix_official> [source-version]

The output is a plain Python module (no PyYAML needed at ESPHome build time).

The official integration keeps one YAML file per device family in config/ (the file name is a hash of the product
number the device reports in register 32768, so the files are matched here on `product_info.default_name`).
Mapping rules (official definition -> ESPHome platform):
  read_quantities  STRING                            -> text_sensor (kind "char")
                   value_mapping (battery_status)    -> text_sensor (kind "states")
                   value_mapping (backup_soc_enable) -> binary_sensor (kind "bool")
                   numeric                           -> sensor (internal quantities become diagnostics)
                   power_split_mode                  -> sensor with "split" (negative_only: -x -> x, else 0)
                   additional_sources (pv_power)     -> sensor (kind "calc", mode "sum")
  write_quantities.operating_mode                    -> select (options filtered at write time by ems_mode_mask)
  control_items    battery_power_setpoint            -> number (kind "power_setpoint", signed: charge < 0)
                   *_soc                             -> number (kind "soc_limit")
  battery_power_direction (select) has no ESPHome counterpart: the setpoint number is signed instead.

Register types: the official integration reads the registers listed in `batch_read_ranges.input` with function
code 04 and those in `batch_read_ranges.holding` with function code 03. `rtype` is "read" (FC04) or "holding" (FC03),
the names of ModbusRegisterType in modbustcp_controller. 32-bit values are big-endian (high register first).
"""
import json
import math
import pprint
import sys
from pathlib import Path

import yaml

# model -> default_name of the official configuration file that defines it
MODELS = {
    "max_ac": "Anker SOLIX Solarbank Max AC",
    "max": "Anker SOLIX Solarbank Max",
    "xe": "Anker SOLIX XE",
    "sb4_e5000_pro": "Anker SOLIX Solarbank 4 E5000 Pro",
}
MODEL_NOTES = {
    "max_ac": "validated on hardware by the HA integration users (Solarbank Max AC)",
    "max": "same register map as the Max AC, not validated by me",
    "xe": "same register map as the Max AC, not validated by me",
    "sb4_e5000_pro": "same register map as the Max AC, not validated by me",
}

NUMERIC = {"UINT16": "U_WORD", "INT16": "S_WORD", "UINT32": "U_DWORD", "INT32": "S_DWORD"}

# Polling class of each key: slow changing or rarely needed values are read every `low_priority_interval`
SCAN_LOW = {
    "device_model", "device_sn", "device_sw_version", "product_name",
    "pv_total_generation", "cumulative_charge_energy", "cumulative_discharge_energy", "rated_energy",
    "max_charge_power", "max_discharge_power", "ems_mode_mask", "parallel_capability_mask",
    "backup_soc_enable", "charging_limit_soc", "discharge_limit_soc", "backup_reserve_soc",
}

# Registers that are always read on their own (never merged into a range with a neighbour): the official
# integration reads 0x8007 apart because a range that spans it gets a silent zero appended by the firmware that
# does not implement it, which would look like "no function supported"
OWN_RANGE = {"parallel_capability_mask"}

# Icons: the ones of the official integration, except where this author's other components use another one
ICONS = {
    "pv_power": "mdi:solar-panel",
    "pcs_pv_power": "mdi:solar-panel",
    "battery_charging_power": "mdi:battery-arrow-up",
    "grid_import_power": "mdi:transmission-tower-import",
    "grid_export_power": "mdi:transmission-tower-export",
    "ac_grid_output_power": "mdi:flash",
}

# Labels of the operating modes / battery status, as shown by the official integration (translations/en.json)
OPERATING_MODE_LABELS = {
    "self_consumption": "Self-Consumption Mode",
    "tou_mode": "Time Of Use Mode",
    "custom_mode": "Custom Mode",
    "socket_overlay_mode": "Socket Overlay Mode",
    "third_party_control": "Third-Party Controlled",
    "smart_mode": "Smart Mode",
    "dynamic_pricing": "Dynamic Tariff Mode",
}
BATTERY_STATUS_LABELS = {"standby": "Idle", "charging": "Charging", "discharging": "Discharging", "sleep": "Sleep"}


def parse_ranges(entries):
    out = []
    for item in entries or []:
        start, end = str(item).split("-")
        out.append((int(str(start), 0), int(str(end), 0)))
    return out


def rtype_of(cfg, address, count, input_ranges, holding_ranges):
    """FC04 ('read') or FC03 ('holding'): explicit register_type first, else the batch range that holds it."""
    explicit = cfg.get("register_type")
    if explicit:
        return {"input": "read", "holding": "holding"}[explicit]
    for start, end in input_ranges:
        if start <= address and address + count - 1 <= end:
            return "read"
    for start, end in holding_ranges:
        if start <= address and address + count - 1 <= end:
            return "holding"
    raise SystemExit(f"register {address} is in no batch read range and has no register_type")


def precision_of(cfg):
    gain = cfg.get("gain", 1)
    if isinstance(gain, (int, float)) and gain > 0 and 10 ** round(math.log10(gain)) == gain:
        return round(math.log10(gain))
    return 0


def sensor_spec(key, cfg, rtype):
    unit = cfg.get("unit")
    spec = {
        "register": cfg["address"],
        "count": cfg["count"],
        "vtype": NUMERIC[cfg["data_type"]],
        "rtype": rtype,
        "icon": ICONS.get(key, cfg.get("icon")),
        "scan": "low" if key in SCAN_LOW else "high",
        "kind": "value",
    }
    if unit and unit != "/":
        spec["unit"] = unit
    if unit == "W":
        spec["device_class"], spec["state_class"] = "power", "measurement"
    elif unit == "kWh":
        spec["device_class"] = "energy"
        if key != "rated_energy":  # the rated energy is a capacity, not a counter
            spec["state_class"] = "total_increasing"
    elif unit == "%":
        spec["device_class"], spec["state_class"] = "battery", "measurement"
    spec["precision"] = precision_of(cfg)
    gain = cfg.get("gain", 1)
    if gain not in (None, 1):
        spec["scale"] = 1.0 / gain
    if cfg.get("internal"):
        spec["category"] = "diagnostic"
    if key in OWN_RANGE:
        spec["own_range"] = True
    if cfg.get("power_split_mode"):
        spec["split"] = cfg["power_split_mode"]
    return spec


def convert(doc, labels_ok):
    input_ranges = parse_ranges((doc.get("batch_read_ranges") or {}).get("input"))
    holding_ranges = parse_ranges((doc.get("batch_read_ranges") or {}).get("holding"))
    plat = {k: {} for k in ("sensor", "text_sensor", "binary_sensor", "number", "select")}

    for key, cfg in (doc.get("read_quantities") or {}).items():
        address = int(cfg["address"])
        cfg["address"] = address
        rtype = rtype_of(cfg, address, cfg["count"], input_ranges, holding_ranges)
        mapping = cfg.get("value_mapping")
        if cfg["data_type"] == "STRING":
            plat["text_sensor"][key] = {
                "register": address, "count": cfg["count"], "vtype": "RAW", "rtype": rtype,
                "icon": cfg.get("icon"), "scan": "low" if key in SCAN_LOW else "high", "kind": "char",
            }
        elif mapping and key == "battery_status":
            plat["text_sensor"][key] = {
                "register": address, "count": 1, "vtype": "U_WORD", "rtype": rtype, "icon": cfg.get("icon"),
                "scan": "high", "kind": "states",
                "states": {int(v): BATTERY_STATUS_LABELS[name] for v, name in mapping.items()},
            }
        elif mapping and key == "backup_soc_enable":
            plat["binary_sensor"][key] = {
                "register": address, "count": 1, "vtype": "U_WORD", "rtype": rtype, "icon": cfg.get("icon"),
                "scan": "low", "kind": "bool",
            }
        elif cfg["data_type"] in NUMERIC:
            if cfg.get("additional_sources"):
                # pv_power = register + the registers of its additional sources
                sources = {"pcs": {k: v for k, v in sensor_spec(key, cfg, rtype).items()
                                   if k in ("register", "count", "vtype", "rtype", "scan")}}
                for src in cfg["additional_sources"]:
                    s = doc["read_quantities"][src]
                    sources[src] = {
                        "register": int(s["address"]), "count": s["count"], "vtype": NUMERIC[s["data_type"]],
                        "rtype": rtype_of(s, int(s["address"]), s["count"], input_ranges, holding_ranges),
                        "scan": "high",
                    }
                spec = sensor_spec(key, cfg, rtype)
                spec.update({"kind": "calc", "mode": "sum", "deps": sources})
                for k in ("register", "count", "vtype", "rtype"):
                    spec.pop(k)
                plat["sensor"][key] = spec
                # the PCS (own PV inputs) part is also offered on its own
                plat["sensor"]["pcs_pv_power"] = sensor_spec("pcs_pv_power", cfg, rtype)
            else:
                plat["sensor"][key] = sensor_spec(key, cfg, rtype)
        else:
            raise SystemExit(f"unsupported data_type {cfg['data_type']!r} for {key}")

    # Operating mode (select)
    modes = doc["write_quantities"]["enumeration_selection"]["operating_mode"]
    addr = int(modes["address"])
    plat["select"]["operating_mode"] = {
        "register": addr, "count": 1, "vtype": "U_WORD",
        "rtype": rtype_of(modes, addr, 1, input_ranges, holding_ranges),
        "icon": modes.get("icon"), "scan": "high", "kind": "mode",
        "options": {OPERATING_MODE_LABELS[name]: int(v) for v, name in modes["options"].items()},
        # value of register 10064 -> bit of the EMS mode mask (register 32774) that says the mode is supported
        "option_bits": {int(v): int(bit) for v, bit in modes["option_capability_bits"].items()},
        "hold": modes.get("write_protection_duration", 15),
    }

    # Numbers
    for key, cfg in (doc.get("control_items") or {}).items():
        address = int(cfg["address"])
        rtype = rtype_of(cfg, address, cfg["count"], input_ranges, holding_ranges)
        if key == "battery_power_setpoint":
            plat["number"][key] = {
                "register": address, "count": cfg["count"], "vtype": NUMERIC[cfg["data_type"]], "rtype": rtype,
                "unit": "W", "icon": cfg.get("icon"), "kind": "power_setpoint",
                # official range is 0..max_value plus a charge/discharge direction: here one signed number
                "min": -cfg["max_value"], "max": cfg["max_value"], "step": cfg["step"],
            }
        else:
            plat["number"][key] = {
                "register": address, "count": cfg["count"], "vtype": NUMERIC[cfg["data_type"]], "rtype": rtype,
                "unit": cfg["unit"], "icon": cfg.get("icon"), "kind": "soc_limit", "scan": "low",
                "min": cfg["min_value"], "max": cfg["max_value"], "step": cfg["step"],
                "capability_bit": cfg["capability_bit"],
                # writes are refused while the backup SOC function is disabled (write_condition)
                "needs_backup_enable": "write_condition" in cfg,
            }

    plat["binary_sensor"]["modbus_connection"] = {
        "kind": "connection", "device_class": "connectivity", "category": "diagnostic", "icon": "mdi:lan-connect",
    }
    # Product name derived from the serial number (the official integration shows it as the device model)
    sn = doc["read_quantities"]["device_sn"]
    plat["text_sensor"]["product_name"] = {
        "register": int(sn["address"]), "count": sn["count"], "vtype": "RAW",
        "rtype": rtype_of(sn, int(sn["address"]), sn["count"], input_ranges, holding_ranges),
        "icon": "mdi:information", "scan": "low", "kind": "product",
    }

    for p in plat:
        plat[p] = dict(sorted(plat[p].items(), key=lambda kv: (kv[1].get("register", 10**9), kv[0])))
        for k in plat[p]:
            plat[p][k] = {kk: vv for kk, vv in plat[p][k].items() if vv is not None}
    return plat


def main():
    src = Path(sys.argv[1])
    version = sys.argv[2] if len(sys.argv) > 2 else "unknown"
    docs = {}
    for path in sorted((src / "config").glob("*.yaml")):
        doc = yaml.safe_load(path.read_text(encoding="utf-8"))
        docs[doc["product_info"]["default_name"]] = doc

    tables, codes = {}, {}
    for model, default_name in MODELS.items():
        doc = docs[default_name]
        tables[model] = convert(doc, True)
        info = doc["product_info"]
        codes[model] = {"default": info["default_name"], "codes": dict(info["product_code_mapping"])}

    # The four families share one register map: emit it once
    first = next(iter(tables.values()))
    shared = all(t == first for t in tables.values())
    if not shared:
        raise SystemExit("the register maps differ between the models: emit one table per model")

    lines = [
        '"""Register tables for the Anker SOLIX Solarbank family (GENERATED - do not edit by hand).',
        "",
        "Generated by tools/gen_registers.py from the device configuration files of",
        "https://github.com/anker-charging/ha-anker-solix-official (custom_components/anker_solix_official/config),",
        f"integration version {version}.",
        "The register definitions are Copyright (c) 2026 Anker Innovations, MIT License (see that repository).",
        "",
        "Per ESPHome platform: key -> specification (every model of the family shares the same register map).",
        "  register/count : first register and number of 16-bit registers",
        "  rtype          : 'read' = input registers (function code 04), 'holding' = holding registers (03)",
        "  vtype          : SensorValueType name of modbustcp_controller (32-bit values are big-endian)",
        "  scan           : polling class, 'high' (update_interval) or 'low' (low_priority_interval)",
        '"""',
        "",
        f"MODEL_NAMES = {pprint.pformat({m: n for m, n in MODELS.items()}, width=110)}",
        "",
        f"MODEL_NOTES = {pprint.pformat(MODEL_NOTES, width=110)}",
        "",
        "# Product codes (characters 4..6 of a 16 digit serial number, 4..7 of a 17 digit one) of each model",
        f"PRODUCT_CODES = {pprint.pformat(codes, width=110)}",
        "",
        "# One register map for every model",
        "_SOLARBANK = {",
    ]
    for p, entries in first.items():
        lines.append(f'    "{p}": {{')
        for key, spec in entries.items():
            lines.append(f'        "{key}": {spec!r},')
        lines.append("    },")
    lines.append("}")
    lines.append("")
    lines.append("REGISTERS = {model: _SOLARBANK for model in MODEL_NAMES}")
    Path(__file__).resolve().parent.parent.joinpath("registers.py").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print({p: len(e) for p, e in first.items()})


if __name__ == "__main__":
    main()
