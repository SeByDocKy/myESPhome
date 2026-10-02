#!/usr/bin/env python3
"""Generate ../registers.py from the register YAML files of the Home Assistant
integration https://github.com/ViperRNMC/marstek_venus_modbus

Usage:
    python gen_registers.py <path to custom_components/marstek_modbus/registers> [source-commit]

The output is a plain Python module (no PyYAML needed at ESPHome build time).
Mapping rules (HA definition -> ESPHome platform):
  SENSOR_DEFINITIONS  numeric data_type          -> sensor
                      char / mac / ipv4          -> text_sensor
                      "states" mapping           -> text_sensor (kind "states")
                      "bit_descriptions"         -> text_sensor (kind "bits") + binary_sensor "<x>_active"
  BINARY_SENSOR_DEFINITIONS                       -> binary_sensor
  SELECT_DEFINITIONS  schedule_N_days            -> number (bit mask, 0..127)
                      others                     -> select
  SWITCH_DEFINITIONS                              -> switch
  NUMBER_DEFINITIONS                              -> number
  BUTTON_DEFINITIONS  (factory_reset skipped)     -> button
  VERSION_SENSOR_DEFINITIONS                      -> text_sensor (kind "firmware")
  EFFICIENCY / STORED_ENERGY / CYCLE / SOLAR      -> sensor (kind "calc")
"""
import pprint
import sys
from pathlib import Path

import yaml

MODELS = {
    "a": ("a.yaml", "Venus A", "untested (per the HA integration)"),
    "d": ("d.yaml", "Venus D", "untested (per the HA integration)"),
    "e_v12": ("e_v12.yaml", "Venus E v1/v2", "RS485 gateway (no Ethernet port)"),
    "e_v3": ("e_v3.yaml", "Venus E v3", "partially validated (per the HA integration)"),
}

NUMERIC = {"uint16": "U_WORD", "int16": "S_WORD", "uint32": "U_DWORD", "int32": "S_DWORD"}
SKIPPED_BUTTONS = {"factory_reset"}  # same command value as reset_device in the HA YAML: unverified


def _base(key, d, extra=None):
    out = {}
    if "register" in d:
        out["register"] = d["register"]
        out["count"] = d.get("count") or (2 if d.get("data_type") in ("uint32", "int32") else 1)
    if d.get("data_type") in NUMERIC:
        out["vtype"] = NUMERIC[d["data_type"]]
    for src, dst in (
        ("unit", "unit"),
        ("device_class", "device_class"),
        ("state_class", "state_class"),
        ("icon", "icon"),
        ("category", "category"),
        ("precision", "precision"),
    ):
        if d.get(src) not in (None, ""):
            out[dst] = d[src]
    if d.get("scale", 1) != 1:
        out["scale"] = d["scale"]
    out["scan"] = d.get("scan_interval", "high")
    if extra:
        out.update(extra)
    return out


def dep_spec(sensors, dep_key):
    d = sensors[dep_key]
    return {
        "key": dep_key,
        "register": d["register"],
        "count": d.get("count") or (2 if d["data_type"] in ("uint32", "int32") else 1),
        "vtype": NUMERIC[d["data_type"]],
        "scale": d.get("scale", 1),
        "scan": d.get("scan_interval", "high"),
    }


def convert(doc):
    plat = {k: {} for k in ("sensor", "text_sensor", "binary_sensor", "number", "switch", "select", "button")}
    sensors = doc.get("SENSOR_DEFINITIONS", {}) or {}
    for key, d in sensors.items():
        dt = d.get("data_type")
        if "bit_descriptions" in d or (key in ("fault_status", "alarm_status")):
            bits = {int(b): str(n) for b, n in (d.get("bit_descriptions") or {}).items()}
            plat["text_sensor"][key] = _base(key, d, {"kind": "bits", "bits": bits})
            plat["binary_sensor"][key.replace("_status", "_active")] = _base(
                key, d, {"kind": "bits_any", "device_class": "problem"}
            )
        elif "states" in d:
            plat["text_sensor"][key] = _base(
                key, d, {"kind": "states", "states": {int(k): str(v) for k, v in d["states"].items()}}
            )
        elif dt in ("char", "mac", "ipv4"):
            plat["text_sensor"][key] = _base(key, d, {"kind": dt})
        elif dt in NUMERIC:
            extra = {"kind": "value"}
            if key == "ems_version":
                extra["transform"] = "ems_version"
                extra["precision"] = 1  # 1476 -> 147.6
            plat["sensor"][key] = _base(key, d, extra)
        else:
            raise SystemExit(f"unsupported data_type {dt!r} for {key}")

    for key, d in (doc.get("BINARY_SENSOR_DEFINITIONS") or {}).items():
        plat["binary_sensor"][key] = _base(key, d, {"kind": "bool"})
    plat["binary_sensor"]["modbus_connection"] = {
        "kind": "connection", "device_class": "connectivity", "category": "diagnostic",
        "icon": "mdi:lan-connect",
    }

    for key, d in (doc.get("SELECT_DEFINITIONS") or {}).items():
        if key.endswith("_days"):
            plat["number"][key] = _base(
                key, d,
                {"vtype": "U_WORD", "min": 0, "max": 127, "step": 1, "icon": "mdi:calendar-week",
                 "days_mask": True},
            )
        else:
            plat["select"][key] = _base(
                key, d, {"vtype": "U_WORD", "options": {str(k): int(v) for k, v in d["options"].items()}}
            )

    for key, d in (doc.get("SWITCH_DEFINITIONS") or {}).items():
        plat["switch"][key] = _base(
            key, d, {"vtype": "U_WORD", "on": d["command_on"], "off": d["command_off"]}
        )

    for key, d in (doc.get("NUMBER_DEFINITIONS") or {}).items():
        unit = d.get("unit")
        if unit == "min":  # HHMM encoded, not minutes: drop the misleading unit
            unit = None
        spec = _base(key, d, {
            "vtype": NUMERIC[d["data_type"]],
            "min": d["min"], "max": d["max"], "step": d["step"],
        })
        if unit:
            spec["unit"] = unit
        else:
            spec.pop("unit", None)
        plat["number"][key] = spec

    for key, d in (doc.get("BUTTON_DEFINITIONS") or {}).items():
        if key in SKIPPED_BUTTONS:
            continue
        plat["button"][key] = _base(key, d, {"vtype": "U_WORD", "command": d["command"]})

    for key, d in (doc.get("VERSION_SENSOR_DEFINITIONS") or {}).items():
        plat["text_sensor"][key] = {
            "kind": "firmware", "mode": d["mode"], "icon": d.get("icon"),
            "category": d.get("category"), "scan": "low",
            "deps": {a: dep_spec(sensors, k) for a, k in d["dependency_keys"].items()},
        }

    calc = {}
    for section in (
        "EFFICIENCY_SENSOR_DEFINITIONS", "STORED_ENERGY_SENSOR_DEFINITIONS",
        "CYCLE_SENSOR_DEFINITIONS", "SOLAR_POWER_SENSOR_DEFINITIONS",
    ):
        for key, d in (doc.get(section) or {}).items():
            mode = d.get("mode") or {
                "STORED_ENERGY_SENSOR_DEFINITIONS": "stored_energy",
                "CYCLE_SENSOR_DEFINITIONS": "cycles",
                "SOLAR_POWER_SENSOR_DEFINITIONS": "sum",
            }[section]
            calc[key] = {
                "kind": "calc", "mode": mode, "scan": "low",
                "unit": d.get("unit"), "icon": d.get("icon"), "category": d.get("category"),
                "device_class": d.get("device_class"), "state_class": d.get("state_class"),
                "precision": 1 if mode in ("round_trip", "conversion") else 2,
                "deps": {a: dep_spec(sensors, k) for a, k in d["dependency_keys"].items()},
            }
    for key, spec in calc.items():
        plat["sensor"][key] = {k: v for k, v in spec.items() if v is not None}
    for k in list(plat["text_sensor"]):
        plat["text_sensor"][k] = {kk: vv for kk, vv in plat["text_sensor"][k].items() if vv is not None}

    for p in plat:
        plat[p] = dict(sorted(plat[p].items(), key=lambda kv: (kv[1].get("register", 10**9), kv[0])))
    return plat


def main():
    src = Path(sys.argv[1])
    commit = sys.argv[2] if len(sys.argv) > 2 else "unknown"
    out = {}
    for model, (fname, _name, _note) in MODELS.items():
        out[model] = convert(yaml.safe_load((src / fname).read_text(encoding="utf-8")))
    lines = [
        '"""Register tables for the Marstek Venus models (GENERATED - do not edit by hand).',
        "",
        "Generated by tools/gen_registers.py from the register definitions of",
        "https://github.com/ViperRNMC/marstek_venus_modbus (registers/*.yaml),",
        f"source commit {commit}.",
        "The register definitions are Copyright (c) 2025 Viper, MIT License (see that repository).",
        "",
        "Per model and per ESPHome platform: key -> specification.",
        "  register/count : first holding register and number of 16-bit registers",
        "  vtype          : SensorValueType name of modbustcp_controller (32-bit values are big-endian)",
        "  scan           : polling class, 'high' (update_interval) or 'low' (low_priority_interval)",
        '"""',
        "",
        f"MODEL_NAMES = {pprint.pformat({m: v[1] for m, v in MODELS.items()}, width=100)}",
        "",
        f"MODEL_NOTES = {pprint.pformat({m: v[2] for m, v in MODELS.items()}, width=100)}",
        "",
        "REGISTERS = {",
    ]
    for model, plat in out.items():
        lines.append(f'    "{model}": {{')
        for p, entries in plat.items():
            lines.append(f'        "{p}": {{')
            for key, spec in entries.items():
                lines.append(f'            "{key}": {spec!r},')
            lines.append("        },")
        lines.append("    },")
    lines.append("}")
    Path(__file__).resolve().parent.parent.joinpath("registers.py").write_text("\n".join(lines) + "\n", encoding="utf-8")
    for model, plat in out.items():
        print(model, {p: len(e) for p, e in plat.items()})


if __name__ == "__main__":
    main()
