#!/usr/bin/env python3
"""Generate sunspec_models.h from the official SunSpec model definitions.

Source: https://github.com/sunspec/models (json/model_<id>.json, Apache License 2.0).

usage: gen_models.py <path to the sunspec/models json directory> <output header>

For every model the generated header holds the numeric points (the ones that become sensors) with their
offset from the model header, type, size, scale factor offset and unit, plus the repeating group if any.
Each table is guarded by SUNSPEC_MODEL_<id> so only the models listed in `models:` take flash space.
"""
import json
import sys
from pathlib import Path

MODELS = [1, 101, 102, 103, 111, 112, 113, 120, 121, 122, 123, 124, 160, 201, 202, 203, 204, 701, 702, 703, 704, 713, 714, 715, 802]

# SunSpec type -> (C++ enumerator, becomes a sensor)
TYPES = {
    "int16": "INT16", "uint16": "UINT16", "acc16": "UINT16", "int32": "INT32", "uint32": "UINT32",
    "acc32": "ACC32", "acc64": "ACC64", "int64": "INT64", "uint64": "UINT64", "float32": "FLOAT32",
    "enum16": "ENUM16", "bitfield16": "BITFIELD16", "bitfield32": "BITFIELD32", "string": "STRING",
}
SKIP_NAMES = {"ID", "L"}
# Types that become sensors
NUMERIC = {"INT16", "UINT16", "INT32", "UINT32", "INT64", "UINT64", "ACC32", "ACC64", "FLOAT32"}
# Types that become text sensors (enum: symbol name, bitfield: names of the set bits, string: the text)
TEXT = {"ENUM16", "BITFIELD16", "BITFIELD32", "STRING"}
# Points that are numeric in the model but of no use as sensors, per model id
SKIP_POINTS = {1: {"DA"}}
# SunSpec unit -> (unit shown in HA, kind)
UNITS = {
    "A": ("A", "CURRENT"), "V": ("V", "VOLTAGE"), "W": ("W", "POWER"), "Wh": ("Wh", "ENERGY"),
    "VA": ("VA", "APPARENT_POWER"), "VAr": ("var", "REACTIVE_POWER"), "Hz": ("Hz", "FREQUENCY"),
    "C": ("°C", "TEMPERATURE"), "%": ("%", "NONE"), "Pct": ("%", "NONE"), "Secs": ("s", "NONE"),
    "Ah": ("Ah", "NONE"), "Deg": ("°", "NONE"), "kWh": ("kWh", "ENERGY"), "VAh": ("VAh", "NONE"),
    "VArh": ("varh", "NONE"), "var": ("var", "REACTIVE_POWER"), "varh": ("varh", "NONE"),
    "AH": ("Ah", "NONE"), "ohms": ("Ω", "NONE"), "cos()": ("", "NONE"),
    "%Max/Sec": ("%/s", "NONE"), "S": ("S", "NONE"), "Var": ("var", "REACTIVE_POWER"), "Varh": ("varh", "NONE"),
    "WH": ("Wh", "ENERGY"),
    "%WHRtg": ("%", "NONE"), "% WChaMax/sec": ("%/s", "NONE"), "% WChaMax": ("%", "NONE"),
    " % WChaMax": ("%", "NONE"), "% WMax/sec": ("%/s", "NONE"), "% WMax": ("%", "NONE"),
    "% WGra": ("%", "NONE"), "% VArMax": ("%", "NONE"), "% VArAval": ("%", "NONE"), "% WDisChaMax": ("%", "NONE"), "% AhrRtg": ("%", "NONE"),
}


def ident(name):
    """C++ identifier part of a point name (GROUP.POINT of the flattened sync groups -> GROUP_POINT)."""
    return name.replace(".", "_")


def cstr(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def flatten(group):
    """Points of the fixed block: the points of the group, then those of its `sync` sub groups (fixed layout,
    e.g. PFWInj of model 704), renamed GROUP.POINT so that the keys stay unique."""
    points = list(group["points"])
    for sub in group.get("groups", []):
        if sub.get("type") == "sync":
            for p in flatten(sub):
                q = dict(p)
                q["name"] = f"{sub['name']}.{p['name']}"
                q["label"] = f"{p.get('label', p['name'])} ({sub['name']})"
                points.append(q)
    return points


def is_writable(p):
    """Only 16-bit points are written (FC06): a 32-bit point is left read-only."""
    return p.get("access") == "RW" and TYPES.get(p["type"]) in ("INT16", "UINT16", "ENUM16")


def collect(points, sf_offsets, skip=()):
    """Walk a point list, return (entries, size). Offsets are relative to the start of the list."""
    out, off = [], 0
    for p in points:
        size = p["size"]
        t = p["type"]
        if p["name"] not in SKIP_NAMES and p["name"] not in skip and t in TYPES:
            unit, kind = UNITS.get(p.get("units", ""), ("", "NONE"))
            if p.get("units") and p["units"] not in UNITS:
                raise SystemExit(f"unit {p['units']!r} of {p['name']} is not mapped")
            sf = p.get("sf")
            if sf is not None and sf not in sf_offsets:
                raise SystemExit(f"scale factor {sf} of {p['name']} not found in the fixed block")
            out.append((p, off, size, unit, kind, sf_offsets.get(sf, -1) if sf else -1))
        off += size
    return out, off


def count_entries(info, entries, key):
    for p, _o, _size, unit, kind, _sfo in entries:
        rw = is_writable(p)
        if rw and key == "points":
            if TYPES[p["type"]] in ("INT16", "UINT16"):
                info["rw_numbers"] += 1
            elif TYPES[p["type"]] == "ENUM16":
                info["rw_selects"] += 1
        if TYPES[p["type"]] in NUMERIC:
            info[key] += 1
            if unit:
                info["units"].add(unit)
            info["kinds"].add(kind)
        elif TYPES[p["type"]] in TEXT:
            info["text_" + key] += 1
            if TYPES[p["type"]].startswith("BITFIELD"):
                info["bits_" + key] += sum(1 for sym in p.get("symbols", []) if not sym["name"].startswith("RESERVED"))


def main():
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    lines = [
        "// Generated by tools/gen_models.py from https://github.com/sunspec/models (Apache License 2.0).",
        "// Do not edit by hand.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace esphome::sunspec_client {",
        "",
        "enum class PointType : uint8_t {",
        "  INT16, UINT16, INT32, UINT32, INT64, UINT64, ACC32, ACC64, FLOAT32, ENUM16, BITFIELD16, BITFIELD32, STRING",
        "};",
        "",
        "enum class PointKind : uint8_t {",
        "  NONE, CURRENT, VOLTAGE, POWER, ENERGY, APPARENT_POWER, REACTIVE_POWER, FREQUENCY, TEMPERATURE",
        "};",
        "",
        "/// Name of an enum value, or of a bit (value = bit number) of a bitfield.",
        "struct SymbolDef {",
        "  uint16_t value;",
        "  const char *name;",
        "};",
        "",
        "/// One point of a model. offset: registers from the model header (ID register = 0); inside a repeating",
        "/// group, from the start of the group instance. sf_offset: offset of the scale factor register from the",
        "/// model header, -1 when the point has none.",
        "struct PointDef {",
        "  const char *key;    // SunSpec point name, e.g. PhVphA",
        "  const char *label;  // SunSpec label, e.g. Phase Voltage AN",
        "  uint16_t offset;",
        "  PointType type;",
        "  uint8_t size;       // registers",
        "  int16_t sf_offset;",
        "  const char *unit;",
        "  PointKind kind;",
        "  const SymbolDef *symbols;  // enum values or bitfield bit names, nullptr when none",
        "  uint8_t num_symbols;",
        "  bool writable;      // access RW in the SunSpec definition",
        "};",
        "",
        "/// Repeating group (e.g. the MPPT modules of model 160).",
        "struct GroupDef {",
        "  uint16_t count_offset;  // offset of the register holding the number of instances",
        "  uint16_t first_offset;  // offset of the first instance from the model header",
        "  uint16_t stride;        // registers per instance",
        "  const char *name;",
        "  const PointDef *points;",
        "  uint8_t num_points;",
        "};",
        "",
        "struct ModelDef {",
        "  uint16_t id;",
        "  const char *name;",
        "  const PointDef *points;  // fixed block",
        "  uint8_t num_points;",
        "  const GroupDef *group;   // nullptr when the model has no repeating group",
        "};",
        "",
    ]
    defs = []
    info = {}
    for mid in MODELS:
        g = json.loads((src / f"model_{mid}.json").read_text())["group"]
        points = flatten(g)
        sf_offsets, off = {}, 0
        for p in points:
            if p["type"] == "sunssf":
                sf_offsets[p["name"]] = off
            off += p["size"]
        fixed, fixed_size = collect(points, sf_offsets, SKIP_POINTS.get(mid, ()))
        info[mid] = {"points": 0, "group_points": 0, "text_points": 0, "text_group_points": 0, "bits_points": 0, "bits_group_points": 0, "rw_numbers": 0, "rw_selects": 0, "units": set(), "kinds": set()}
        count_entries(info[mid], fixed, "points")
        subs = [x for x in g.get("groups", []) if x.get("type") != "sync"]
        count_off = None
        if subs:
            # The number of instances is held by the point named by the `count` of the group (or of type count)
            cname = subs[0].get("count") if isinstance(subs[0].get("count"), str) else None
            offs, o = {}, 0
            for p in g["points"]:
                offs[p["name"]] = o
                if p["type"] == "count" and cname is None:
                    cname = p["name"]
                o += p["size"]
            count_off = offs.get(cname)
        lines.append(f"#ifdef SUNSPEC_MODEL_{mid}")
        lines.append(f"// Model {mid}: {g.get('label', g['name'])}")

        def emit(var, entries):
            # Symbol tables first (enum values, bit names)
            for p, *_ in entries:
                if TYPES[p["type"]] in TEXT and p.get("symbols"):
                    lines.append(f"static constexpr SymbolDef {var}_SYM_{ident(p['name'])}[] = {{")
                    for sym in p["symbols"]:
                        lines.append(f"    {{{sym['value']}, {cstr(sym['name'])}}},")
                    lines.append("};")
            lines.append(f"static constexpr PointDef {var}[] = {{")
            for p, o, size, unit, kind, sfo in entries:
                if TYPES[p["type"]] in TEXT and p.get("symbols"):
                    sym = f"{var}_SYM_{ident(p['name'])}, {len(p['symbols'])}"
                else:
                    sym = "nullptr, 0"
                lines.append(f"    {{{cstr(p['name'])}, {cstr(p.get('label', p['name']))}, {o}, PointType::{TYPES[p['type']]}, "
                             f"{size}, {sfo}, {cstr(unit)}, PointKind::{kind}, {sym}, "
                             f"{'true' if is_writable(p) else 'false'}}},")
            lines.append("};")

        emit(f"MODEL_{mid}_POINTS", fixed)
        group_ref = "nullptr"
        if subs:
            if len(subs) != 1 or count_off is None:
                raise SystemExit(f"model {mid}: unsupported group layout")
            sub = subs[0]
            entries, stride = collect(sub["points"], sf_offsets)
            count_entries(info[mid], entries, "group_points")
            emit(f"MODEL_{mid}_GROUP_POINTS", entries)
            lines.append(f"static constexpr GroupDef MODEL_{mid}_GROUP = {{{count_off}, {fixed_size}, {stride}, "
                         f"{cstr(sub['name'])}, MODEL_{mid}_GROUP_POINTS, {len(entries)}}};")
            group_ref = f"&MODEL_{mid}_GROUP"
        lines.append(f"static constexpr ModelDef MODEL_{mid} = {{{mid}, {cstr(g['name'])}, MODEL_{mid}_POINTS, "
                     f"{len(fixed)}, {group_ref}}};")
        lines.append("#endif")
        lines.append("")
        defs.append(mid)

    lines.append("/// Definition of a model, nullptr when the model is unknown or not compiled in.")
    lines.append("inline const ModelDef *find_model_def(uint16_t id) {")
    lines.append("  switch (id) {")
    for mid in defs:
        lines += [f"#ifdef SUNSPEC_MODEL_{mid}", f"    case {mid}:", f"      return &MODEL_{mid};", "#endif"]
    lines += ["    default:", "      return nullptr;", "  }", "}", "", "}  // namespace esphome::sunspec_client", ""]
    dst.write_text("\n".join(lines))
    kinds = ["NONE", "CURRENT", "VOLTAGE", "POWER", "ENERGY", "APPARENT_POWER", "REACTIVE_POWER", "FREQUENCY",
             "TEMPERATURE"]
    py = ["# Generated by tools/gen_models.py, do not edit by hand.", "",
          "# Number of sensors and text sensors per model: fixed block points and points of one instance of the repeating group.",
          "MODEL_INFO = {"]
    for mid, i in info.items():
        py.append(f"    {mid}: {{\"points\": {i['points']}, \"group_points\": {i['group_points']}, "
                  f"\"text_points\": {i['text_points']}, \"text_group_points\": {i['text_group_points']}, "
                  f"\"bits_points\": {i['bits_points']}, \"bits_group_points\": {i['bits_group_points']}, "
                  f"\"rw_numbers\": {i['rw_numbers']}, \"rw_selects\": {i['rw_selects']}, "
                  f"\"units\": {sorted(i['units'])!r}, \"kinds\": {sorted(i['kinds'], key=kinds.index)!r}}},")
    py += ["}", "", "# PointKind enumerators in declaration order", f"POINT_KINDS = {kinds!r}", ""]
    dst.with_name("sunspec_models_info.py").write_text("\n".join(py))
    print(f"wrote {dst}: {len(defs)} models")


if __name__ == "__main__":
    main()
