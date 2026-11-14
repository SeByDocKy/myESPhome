#!/usr/bin/env python3
"""Generate ../registers.py from the BLUETTI Modbus register library used by hassio-bluetti-modbus.

Usage:
    python gen_registers.py <path to bluetti_modbus_lib> [source-version]

where <path to bluetti_modbus_lib> is the folder that holds devices/balco260.py, devices/balco500.py and enums/
(https://github.com/bluetti-community/bluetti-modbus, vendored by
https://github.com/bluetti-community/hassio-bluetti-modbus).

The device files are only PARSED (ast), never imported or executed, so the generator needs nothing but the standard
library. The output is a plain Python module (no dependency at ESPHome build time).

Mapping rules (library field -> ESPHome platform):
  field(UINT16/INT16/UINT32/INT32, scale, offset)  -> sensor              (kind "value")
  nibble(addr, high)                               -> sensor              (kind "nibble")
  reference_offset_current(addr, reference)        -> sensor              (kind "ref_current")
  field(STRING)                                    -> text_sensor         (kind "string",  ASCII, low byte first)
  field(UINT64)                                    -> text_sensor         (kind "serial",  decimal)
  dotted_version(addr)                             -> text_sensor         (kind "version", major.minor.patch)
  field(ENUM, Enum)                                -> text_sensor         (kind "states")
  field(ENUM, IntFlag)                             -> text_sensor         (kind "flags",   comma separated E codes)
  UINT16 / UINT32 bit masks without a flag enum    -> text_sensor         (kind "hex")
  field(UINT16, switch name) not writable          -> binary_sensor       (kind "bool")
  field(UINT16, switch name), writable             -> switch              (kind "bool", written 1 / 0, FC 06)
  field(UINT16, ...), writable=Range(min, max)     -> number              (kind "value", min / max / step 1)
  field(ENUM, Enum), writable                      -> select              (kind "select", options = enum members)

The BLUETTI registers are read with function code 03 (holding registers) and written with 06. Words of a 32 / 64 bit
value come LOW WORD FIRST (the library says word_order="little"): ESPHome's U_DWORD_R / S_DWORD_R / U_QWORD_R.
"""
import ast
import pprint
import re
import sys
from pathlib import Path

MODELS = {
    "balco260": ("Balco260", "balco260.py", "Balco 260"),
    "balco500": ("Balco500", "balco500.py", "Balco 500"),
}
MODEL_NOTES = {
    "balco260": "validated on real hardware by the users of hassio-bluetti-modbus (Balco 260)",
    "balco500": "captured on a real Balco 500 by a user of hassio-bluetti-modbus; not validated by me",
}

NUMERIC = {
    "INT16": ("S_WORD", 1),
    "UINT16": ("U_WORD", 1),
    "INT32": ("S_DWORD_R", 2),
    "UINT32": ("U_DWORD_R", 2),
}

# Polling class: identity, versions, counters and settings change slowly -> `low_priority` controller
SLOW_PATH_PARTS = {
    "type", "serial", "serial_number", "version1", "version2", "version3", "version4", "version_count",
    "arm_version", "dsp_version", "iot_model", "iot_serial", "iot_version", "inverter_type", "pack_count",
    "dc_count", "ac_count", "inverter_count", "grid_phase_count", "output_phase_count", "inverter_phase_count",
    "cell_count", "ntc_count", "soc_low_limit", "soc_high_limit", "working_mode", "ac_output", "system_power",
    "grid_charging", "grid_feed_in",
}

# ---------------------------------------------------------------------------
# Naming: library field name -> (platform, path in the YAML)
# The layout follows the author's other battery components: dc_channels: / ac: / battery:, indexed maps starting
# at 0 (pv0, phase0, inverter0, pack0), global diagnostics flat.
# ---------------------------------------------------------------------------
FIELD = {"p": "power", "v": "voltage", "c": "current"}

DIRECT = {
    # dc_channels
    "pv_i_p_total": ("dc_channels", "power"),
    "pv_i_e_total": ("dc_channels", "energy"),
    "pv_ac_p": ("dc_channels", "ac_power"),
    "pv_ac_e": ("dc_channels", "ac_energy"),
    "pv_i_p_local": ("dc_channels", "local_power"),
    "pv_ac_p_local": ("dc_channels", "ac_local_power"),
    "pv_i_e_local": ("dc_channels", "local_energy"),
    "pv_ac_e_local": ("dc_channels", "ac_local_energy"),
    "pv_dc_count": ("dc_channels", "dc_count"),
    "pv_ac_count": ("dc_channels", "ac_count"),
    # ac
    "ac_o_p_total": ("ac", "output_power"),
    "ac_o_e_total": ("ac", "output_energy"),
    "g_i_p_total": ("ac", "grid_power"),
    "g_i_e_total": ("ac", "grid_input_energy"),
    "g_o_e_total": ("ac", "grid_output_energy"),
    "g_i_f": ("ac", "grid_frequency"),
    "d_inverter_total": ("ac", "inverter_power"),
    "g_i_p_local": ("ac", "grid_local_power"),
    "ac_o_p_local": ("ac", "output_local_power"),
    "g_i_e_local": ("ac", "grid_input_local_energy"),
    "g_o_e_local": ("ac", "grid_output_local_energy"),
    "ac_o_e_local": ("ac", "output_local_energy"),
    "d_self_consumption": ("ac", "self_consumption"),
    "d_num_inverters": ("ac", "inverter_count"),
    "d_phase_count": ("ac", "grid_phase_count"),
    "ac_phase_count": ("ac", "output_phase_count"),
    "d_inverter_phase_count": ("ac", "inverter_phase_count"),
    "d_inverter_status": ("ac", "inverter_status"),
    # battery (whole system)
    "b_v_total": ("battery", "voltage"),
    "b_c_total": ("battery", "current"),
    "b_soc_total": ("battery", "soc"),
    "b_soh_total": ("battery", "soh"),
    "b_status": ("battery", "status"),
    "b_time_to_full_total": ("battery", "time_to_full"),
    "b_time_to_empty_total": ("battery", "time_to_empty"),
    "d_num_battery_packs": ("battery", "pack_count"),
    "b_soc_low": ("battery", "soc_low_limit"),
    "b_soc_high": ("battery", "soc_high_limit"),
    # battery (the built-in pack)
    "b_type": ("battery", "packs", "pack0", "type"),
    "b_serial": ("battery", "packs", "pack0", "serial"),
    "b_ver_count": ("battery", "packs", "pack0", "version_count"),
    "b_ver_1": ("battery", "packs", "pack0", "version1"),
    "b_ver_2": ("battery", "packs", "pack0", "version2"),
    "b_ver_3": ("battery", "packs", "pack0", "version3"),
    "b_ver_4": ("battery", "packs", "pack0", "version4"),
    "b_v": ("battery", "packs", "pack0", "voltage"),
    "b_c": ("battery", "packs", "pack0", "current"),
    "b_soc": ("battery", "packs", "pack0", "soc"),
    "b_soh": ("battery", "packs", "pack0", "soh"),
    "b_cycle_count": ("battery", "packs", "pack0", "cycle_count"),
    "b_cell_count": ("battery", "packs", "pack0", "cell_count"),
    "b_ntc_count": ("battery", "packs", "pack0", "ntc_count"),
    "b_i_e": ("battery", "packs", "pack0", "energy_in"),
    "b_o_e": ("battery", "packs", "pack0", "energy_out"),
    "b_t_avg": ("battery", "packs", "pack0", "temperature"),
    "b_time_to_full": ("battery", "packs", "pack0", "time_to_full"),
    "b_time_to_empty": ("battery", "packs", "pack0", "time_to_empty"),
    "b_protect": ("battery", "packs", "pack0", "protect"),
    "b_error": ("battery", "packs", "pack0", "error"),
    "b_alarm_residential": ("battery", "packs", "pack0", "alarm_residential"),
    "b_alarm_portable": ("battery", "packs", "pack0", "alarm_portable"),
    # device level, flat
    "d_inverter_type": ("inverter_type",),
    "d_serial": ("serial_number",),
    "d_ver_arm": ("arm_version",),
    "d_ver_dsp": ("dsp_version",),
    "d_iot_model": ("iot_model",),
    "d_iot_serial": ("iot_serial",),
    "d_iot_ver": ("iot_version",),
    "d_inverter_warning": ("inverter_warning",),
    "d_inverter_fault": ("inverter_fault",),
    "d_working_mode": ("working_mode",),
    # switches (read here, written by the write platforms)
    "g_i_switch": ("grid_charging",),
    "g_o_switch": ("grid_feed_in",),
}
# 57001 is the AC output of a Balco 260 but the "System ON/OFF" of a Balco 500 (it stops the whole system)
SWITCH_57001 = {"balco260": ("ac_output",), "balco500": ("system_power",)}


def path_of(name, model):
    if name == "ac_o_switch":
        return SWITCH_57001[model]
    if name in DIRECT:
        return DIRECT[name]
    m = re.fullmatch(r"pv_(\d)_i_(p|v|c)", name)
    if m:
        return ("dc_channels", f"pv{int(m[1]) - 1}", FIELD[m[2]])
    m = re.fullmatch(r"pv_(\d)_i_type", name)
    if m:
        return ("dc_channels", f"pv{int(m[1]) - 1}", "type")
    m = re.fullmatch(r"g_(\d)_i_(p|v|c)", name)
    if m:
        return ("ac", "grid_phases", f"phase{int(m[1]) - 1}", FIELD[m[2]])
    m = re.fullmatch(r"ac_(\d)_o_(p|v|c)", name)
    if m:
        return ("ac", "output_phases", f"phase{int(m[1]) - 1}", FIELD[m[2]])
    m = re.fullmatch(r"d_inverter_(\d)_(p|v|c)", name)
    if m:
        return ("ac", "inverters", f"inverter{int(m[1]) - 1}", FIELD[m[2]])
    m = re.fullmatch(r"d_inverter_(\d)_status", name)
    if m:
        return ("ac", "inverters", f"inverter{int(m[1]) - 1}", "status")
    raise SystemExit(f"no YAML name for the library field {name!r}")


# ---------------------------------------------------------------------------
# Presentation of a numeric sensor, from its unit
# ---------------------------------------------------------------------------
ICON_BY_PATH = {
    "power": "mdi:flash",
    "voltage": "mdi:sine-wave",
    "current": "mdi:current-ac",
    "energy": "mdi:counter",
    "soc": "mdi:battery",
    "soh": "mdi:battery-heart-variant",
}


def presentation(unit, path):
    """unit / device_class / state_class / precision / icon of a numeric sensor."""
    leaf = path[-1]
    out = {}
    if unit:
        out["unit"] = unit
    if unit == "W":
        out.update(device_class="power", state_class="measurement", icon="mdi:flash")
        if path[0] == "dc_channels":
            out["icon"] = "mdi:solar-panel"
        elif leaf.startswith("grid"):
            out["icon"] = "mdi:transmission-tower"
        elif path[0] == "ac":
            out["icon"] = "mdi:home-lightning-bolt"
    elif unit in ("kWh", "Wh"):
        out.update(device_class="energy", state_class="total_increasing", icon="mdi:counter")
        if path[0] == "dc_channels":
            out["icon"] = "mdi:solar-power-variant"
        elif path[0] == "battery":
            out["icon"] = "mdi:battery-plus" if leaf == "energy_in" else "mdi:battery-minus"
    elif unit == "V":
        out.update(device_class="voltage", state_class="measurement", icon="mdi:sine-wave")
    elif unit == "A":
        out.update(device_class="current", state_class="measurement", icon="mdi:current-ac")
    elif unit == "Hz":
        out.update(device_class="frequency", state_class="measurement", icon="mdi:sine-wave")
    elif unit == "%":
        out.update(state_class="measurement", icon="mdi:battery")
        if leaf == "soc":
            out["device_class"] = "battery"
        if leaf == "soh":
            out["icon"] = "mdi:battery-heart-variant"
        if leaf == "self_consumption":
            out["icon"] = "mdi:percent"
        if leaf.startswith("soc_"):
            out["icon"] = "mdi:battery-lock"
    elif unit == "°C":
        out.update(device_class="temperature", state_class="measurement", icon="mdi:thermometer")
    elif unit == "min":
        out.update(state_class="measurement", icon="mdi:timer-outline")
    else:
        out["icon"] = "mdi:counter" if "count" in leaf or "cycle" in leaf else "mdi:information-outline"
    if leaf.endswith("count") or leaf == "cycle_count":
        out["category"] = "diagnostic"
        out.pop("state_class", None)
    return out


def precision_of(scale):
    text = f"{scale:.10f}".rstrip("0").split(".")[1] if "." in f"{scale:.10f}" else ""
    return len(text)


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------
def const(node):
    """Value of a literal (int / str / bool / `1 << n`), None when it is anything else."""
    if isinstance(node, ast.Constant):
        return node.value
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
        return -const(node.operand)
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.LShift):
        return const(node.left) << const(node.right)
    return None


def kwargs_of(call):
    out = {}
    for kw in call.keywords:
        if isinstance(kw.value, ast.Attribute):
            out[kw.arg] = kw.value.attr  # FieldType.UINT16 -> "UINT16"
        elif isinstance(kw.value, ast.Name):
            out[kw.arg] = kw.value.id  # enum_type=InverterStatus -> "InverterStatus"
        elif isinstance(kw.value, ast.Call):
            out[kw.arg] = "validated"  # writable=Range(min=5, max=100)
        else:
            out[kw.arg] = const(kw.value)
    return out


def parse_device(path, class_name):
    tree = ast.parse(path.read_text())
    cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == class_name)
    info = {"ranges": [], "max_span": 50, "fields": []}  # BluettiDevice.max_span default
    for stmt in cls.body:
        if not isinstance(stmt, ast.Assign) or len(stmt.targets) != 1:
            continue
        target = stmt.targets[0].id
        value = stmt.value
        if target == "register_ranges":
            info["ranges"] = sorted(tuple(ast.literal_eval(e)) for e in value.elts)
        elif target == "max_span":
            info["max_span"] = const(value)
        elif isinstance(value, ast.Call):
            func = value.func.id
            args = [const(a) for a in value.args]
            info["fields"].append((target, func, args, kwargs_of(value)))
    return info


def parse_enums(enum_dir):
    """class name -> {'flag': bool, 'members': {name: int}, 'descriptions': {name: text}}"""
    out = {}
    for file in Path(enum_dir).glob("*.py"):
        tree = ast.parse(file.read_text())
        descriptions = {}
        for node in tree.body:
            if isinstance(node, ast.AnnAssign) and isinstance(node.value, ast.Dict):
                descriptions.update(ast.literal_eval(node.value))
        for node in tree.body:
            if not isinstance(node, ast.ClassDef):
                continue
            bases = {b.id for b in node.bases if isinstance(b, ast.Name)}
            members = {}
            for stmt in node.body:
                if isinstance(stmt, ast.Assign) and isinstance(stmt.targets[0], ast.Name):
                    value = const(stmt.value)
                    if isinstance(value, int):
                        members[stmt.targets[0].id] = value
            out[node.name] = {"flag": "IntFlag" in bases, "members": members, "descriptions": descriptions}
    return out


# ---------------------------------------------------------------------------
# Building the specs
# ---------------------------------------------------------------------------
def build_model(model, info, enums):
    specs = {"sensor": {}, "text_sensor": {}, "binary_sensor": {}, "switch": {}, "number": {}, "select": {}}
    problems = []
    blocks = info["ranges"]

    def add(platform, path, spec):
        key = ".".join(path)
        spec["path"] = list(path)
        spec["scan"] = "low" if (path[-1] in SLOW_PATH_PARTS or spec.get("writable")) else "high"
        spec["register"] = spec["register"]
        if not any(lo <= spec["register"] and spec["register"] + spec["count"] - 1 <= hi for lo, hi in blocks):
            problems.append(f"{model}: {key} (register {spec['register']}, {spec['count']}) is outside the readable blocks")
        specs[platform][key] = spec

    for name, func, args, kw in info["fields"]:
        path = path_of(name, model)
        writable = bool(kw.get("writable"))

        if func == "dotted_version":
            add("text_sensor", path, dict(register=args[0], count=2, vtype="RAW", kind="version",
                                          icon="mdi:information-outline", category="diagnostic"))
            continue
        if func == "nibble":
            add("sensor", path, dict(register=args[0], count=1, vtype="U_WORD", kind="nibble",
                                     shift=0 if kw["high"] is False else 4, precision=0, icon="mdi:counter",
                                     category="diagnostic"))
            continue
        if func == "reference_offset_current":
            spec = dict(register=args[0], count=1, vtype="U_WORD", kind="ref_current", reference=kw["reference"],
                        scale=0.1, precision=1)
            spec.update(presentation("A", path))
            add("sensor", path, spec)
            continue
        if func != "field":
            raise SystemExit(f"{model}.{name}: unsupported helper {func}")

        t = kw["t"]
        address = kw["address"]
        if t in NUMERIC:
            vtype, width = NUMERIC[t]
            count = kw.get("count", width)
            scale = kw.get("scale", 1.0)
            offset = kw.get("offset", 0)
            if count != width:
                # A register group read as one value that is not a number (bit masks): shown in hex
                add("text_sensor", path, dict(register=address, count=count, vtype="RAW", kind="hex",
                                              icon="mdi:alert-circle-outline", category="diagnostic"))
                continue
            if path[-1] in ("ac_output", "system_power", "grid_charging", "grid_feed_in"):
                spec = dict(register=address, count=1, vtype="U_WORD", kind="bool", icon="mdi:toggle-switch-outline")
                add("switch" if writable else "binary_sensor", path, spec)
                continue
            if writable:
                bounds = kw.get("writable")
                if bounds != "validated" or count != 1 or width != 1:
                    raise SystemExit(f"{model}.{name}: only 1-register numbers with a Range can be written")
                spec = dict(register=address, count=1, vtype=vtype, kind="value", min=5, max=100, step=1,
                            precision=0)
                spec.update(presentation(kw.get("unit"), path))
                spec.pop("state_class", None)
                spec["icon"] = "mdi:battery-arrow-up" if path[-1].endswith("high_limit") else "mdi:battery-arrow-down"
                add("number", path, spec)
                continue
            spec = dict(register=address, count=count, vtype=vtype, kind="value", precision=precision_of(scale))
            if scale != 1.0:
                spec["scale"] = scale
            if offset:
                spec["offset"] = offset
                spec["precision"] = max(spec["precision"], 0)
            spec.update(presentation(kw.get("unit"), path))
            add("sensor", path, spec)
        elif t == "UINT64":
            add("text_sensor", path, dict(register=address, count=kw.get("count", 4), vtype="U_QWORD_R",
                                          kind="serial", icon="mdi:identifier", category="diagnostic"))
        elif t == "STRING":
            add("text_sensor", path, dict(register=address, count=kw["length"], vtype="RAW", kind="string",
                                          icon="mdi:information-outline", category="diagnostic"))
        elif t == "ENUM":
            enum = enums[kw["enum_type"]]
            if enum["flag"]:
                labels = {}
                for member, value in enum["members"].items():
                    if value:
                        labels[value.bit_length() - 1] = f"{member}: {enum['descriptions'].get(member, '')}".rstrip(": ")
                add("text_sensor", path, dict(register=address, count=kw.get("count", 1), vtype="RAW",
                                              kind="flags", flags=dict(sorted(labels.items())),
                                              icon="mdi:alert-outline"))
            else:
                states = {v: re.sub(r"(?<!^)(?=[A-Z])", " ", k) for k, v in enum["members"].items()}
                if writable:
                    add("select", path, dict(register=address, count=1, vtype="U_WORD", kind="select",
                                             options={label: v for v, label in sorted(states.items())},
                                             icon="mdi:tune-variant"))
                    continue
                add("text_sensor", path, dict(register=address, count=1, vtype="U_WORD", kind="states",
                                              states=states, icon="mdi:information-outline"))
        else:
            raise SystemExit(f"{model}.{name}: unsupported type {t}")
    return {"blocks": blocks, "max_span": info["max_span"], "specs": specs}, problems


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    lib = Path(sys.argv[1])
    version = sys.argv[2] if len(sys.argv) > 2 else "unknown"
    enums = parse_enums(lib / "enums")
    registers = {}
    all_problems = []
    for model, (cls, file, _title) in MODELS.items():
        info = parse_device(lib / "devices" / file, cls)
        registers[model], problems = build_model(model, info, enums)
        all_problems += problems
    if all_problems:
        print("\n".join(all_problems), file=sys.stderr)
        raise SystemExit("some registers are outside the readable blocks")

    out = Path(__file__).resolve().parent.parent / "registers.py"
    header = f'''"""Register tables for the BLUETTI Balco series (GENERATED - do not edit by hand).

Generated by tools/gen_registers.py from the register library of
https://github.com/bluetti-community/bluetti-modbus (vendored by
https://github.com/bluetti-community/hassio-bluetti-modbus), version {version}.
The register definitions are Copyright (c) 2026 Patrick762, MIT License (see those repositories).

Per model: the readable blocks, the largest read, and for each ESPHome platform key -> specification.
  key         : the path in the YAML joined with "." (dc_channels.pv0.power, battery.packs.pack0.soc, ...)
  register    : first register (holding registers, function code 03; the BLUETTI devices have no input registers)
  count       : number of 16-bit registers
  vtype       : SensorValueType of ESPHome's modbus; 32 / 64 bit values come LOW WORD FIRST (the _R types)
  scan        : polling class, "high" (update_interval) or "low" (the slow controller)
"""

'''
    names = {m: title for m, (_c, _f, title) in MODELS.items()}
    body = "MODEL_NAMES = " + pprint.pformat(names, sort_dicts=False) + "\n\n"
    body += "MODEL_NOTES = " + pprint.pformat(MODEL_NOTES, sort_dicts=False) + "\n\n"
    body += "REGISTERS = " + pprint.pformat(registers, sort_dicts=False, width=118) + "\n"
    out.write_text(header + body)
    counts = {m: {p: len(s) for p, s in r["specs"].items()} for m, r in registers.items()}
    print(f"wrote {out}: {counts}")


if __name__ == "__main__":
    main()
